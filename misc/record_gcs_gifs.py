"""Record short demo clips of the web GCS (built-in simulator) and convert them to GIFs.

Needs the web GCS running on :3000, Chrome, ffmpeg and Playwright:
    pip install playwright && playwright install ffmpeg
    python3 misc/record_gcs_gifs.py docs/img                 # all clips
    python3 misc/record_gcs_gifs.py docs/img gcs_fly_here    # one clip
"""
import asyncio
import re
import subprocess
import sys
import time
from pathlib import Path

from playwright.async_api import async_playwright

URL = "http://127.0.0.1:3000/?sim"
OUT = Path(sys.argv[1])
WORK = Path(__file__).parent / "clips"
W, H = 1600, 900

# Visible cursor + click ripple (headless video has no real cursor)
CURSOR_JS = """
window.addEventListener('DOMContentLoaded', () => {
  const c = document.createElement('div');
  c.style.cssText = 'position:fixed;z-index:99999;width:18px;height:18px;margin:-9px 0 0 -9px;border-radius:50%;'
    + 'background:rgba(255,255,255,.85);box-shadow:0 0 0 2px rgba(10,15,26,.8);pointer-events:none;left:-50px;top:-50px;';
  document.body.appendChild(c);
  document.addEventListener('mousemove', e => { c.style.left = e.clientX + 'px'; c.style.top = e.clientY + 'px'; }, true);
  document.addEventListener('mousedown', e => {
    const r = document.createElement('div');
    const col = e.button === 2 ? '251,191,36' : '56,189,248';
    r.style.cssText = `position:fixed;z-index:99998;left:${e.clientX}px;top:${e.clientY}px;width:10px;height:10px;margin:-5px 0 0 -5px;`
      + `border-radius:50%;border:3px solid rgba(${col},.95);pointer-events:none;transition:all .45s ease-out;`;
    document.body.appendChild(r);
    requestAnimationFrame(() => { r.style.width = r.style.height = '46px'; r.style.margin = '-23px 0 0 -23px'; r.style.opacity = '0'; });
    setTimeout(() => r.remove(), 600);
  }, true);
});
"""

BTN_JS = """(t) => { const b = [...document.querySelectorAll('button')].find(b => b.textContent.trim() === t); b.click(); }"""
TAB_JS = """(t) => [...document.querySelectorAll('[role=tab]')].find(b => b.textContent.startsWith(t)).click()"""


class Scene:
    def __init__(self, page, t0):
        self.page, self.t0, self.start = page, t0, 0.0
        self.pos = (W / 2, H / 2)

    def mark(self):
        self.start = time.monotonic() - self.t0

    async def wait(self, s):
        await self.page.wait_for_timeout(int(s * 1000))

    async def move(self, x, y, steps=25):
        await self.page.mouse.move(x, y, steps=steps)
        self.pos = (x, y)

    async def click_at(self, x, y, button="left"):
        await self.move(x, y)
        await self.wait(0.15)
        await self.page.mouse.click(x, y, button=button)
        await self.wait(0.35)

    async def click(self, locator, button="left"):
        await locator.scroll_into_view_if_needed()
        box = await locator.bounding_box()
        await self.click_at(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2, button)

    def button(self, name, exact=True):
        return self.page.get_by_role("button", name=name, exact=exact).first

    def tab(self, name):
        return self.page.get_by_role("tab", name=re.compile(f"^{name}"))

    async def canvas_point(self, sel, fx, fy):
        box = await self.page.locator(sel).bounding_box()
        return box["x"] + box["width"] * fx, box["y"] + box["height"] * fy

    # Fast setup through the DOM (trimmed from the clip)
    async def quick(self, fn, arg):
        await self.page.evaluate(fn, arg)
        await self.wait(0.3)

    async def prep_swarm(self, formation="Wedge (V)"):
        await self.quick(BTN_JS, "Select all")
        await self.quick(BTN_JS, "Takeoff")
        await self.wait(4.5)
        await self.quick(TAB_JS, "Swarm")
        await self.quick(BTN_JS, formation)
        await self.page.evaluate("() => [...document.querySelectorAll('button')].find(b => b.textContent.startsWith('Deploy swarm')).click()")
        await self.wait(9)
        await self.quick(TAB_JS, "Fleet")
        await self.quick(BTN_JS, "Fit all")


async def scene_deploy(s):
    s.mark()
    await s.wait(1)
    await s.click(s.button("Select all"))
    await s.click(s.button("Takeoff"))
    await s.wait(4)
    await s.click(s.tab("Swarm"))
    await s.click(s.button("Wedge (V)"))
    await s.click(s.page.get_by_role("button", name=re.compile("^Deploy swarm")))
    await s.wait(6.5)
    await s.click(s.button("Fit all"))
    await s.wait(3)


async def scene_fly_here(s):
    await s.prep_swarm()
    # zoom the 2D map out a little so there is room to fly
    x, y = await s.canvas_point(".view-2d canvas", 0.5, 0.5)
    await s.page.mouse.move(x, y)
    for _ in range(3):
        await s.page.mouse.wheel(0, 300)
        await s.wait(0.1)
    await s.wait(0.5)
    s.mark()
    await s.wait(0.8)
    await s.click(s.page.locator(".swarm-row").first)
    await s.wait(0.6)
    for fx, fy in [(0.78, 0.22), (0.25, 0.7)]:
        px, py = await s.canvas_point(".view-2d canvas", fx, fy)
        await s.click_at(px, py, button="right")
        await s.wait(0.5)
        await s.click(s.page.get_by_role("button", name="Fly here"))
        await s.wait(7)


async def scene_change_formation(s):
    await s.prep_swarm()
    await s.quick(TAB_JS, "Swarm")
    s.mark()
    await s.wait(0.8)
    for formation in ["Line abreast", "Circle"]:
        await s.click(s.button("Change formation"))
        await s.wait(0.4)
        await s.click(s.button(formation))
        await s.wait(0.8)
        await s.click(s.page.get_by_role("button", name=re.compile("^Update formation")))
        await s.wait(7)
        await s.page.evaluate("() => document.querySelector('.sidebar-body').scrollTo({top: 0, behavior: 'smooth'})")
        await s.wait(0.6)


async def scene_3d(s):
    await s.prep_swarm()
    await s.quick(BTN_JS, "3D")
    await s.wait(1)
    await s.quick(BTN_JS, "Fit all")
    s.mark()
    await s.wait(0.8)
    await s.click(s.page.locator(".swarm-row").first)
    await s.click(s.button("Go to…"))
    gx, gy = await s.canvas_point(".view-3d canvas", 0.72, 0.62)
    await s.click_at(gx, gy)
    await s.wait(1.5)
    # slow orbit while the swarm flies
    cx, cy = await s.canvas_point(".view-3d canvas", 0.45, 0.45)
    await s.move(cx, cy, steps=10)
    await s.page.mouse.down()
    await s.page.mouse.move(cx + 260, cy - 40, steps=120)
    await s.page.mouse.up()
    await s.wait(4)


SCENES = {
    "gcs_deploy_swarm": (scene_deploy, "split"),
    "gcs_fly_here": (scene_fly_here, "2d"),
    "gcs_change_formation": (scene_change_formation, "2d"),
    "gcs_3d_view": (scene_3d, "3d"),
}


def to_gif(webm, start, gif, width=1000, fps=10):
    vf = (f"fps={fps},scale={width}:-1:flags=lanczos,split[a][b];"
          "[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-ss", f"{start:.2f}", "-i", str(webm), "-vf", vf, str(gif)], check=True)


async def main():
    WORK.mkdir(exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    only = sys.argv[2:] or list(SCENES)
    async with async_playwright() as p:
        browser = await p.chromium.launch(
            channel="chrome", headless=True,
            args=["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"],
        )
        for name in only:
            fn, view = SCENES[name]
            vdir = WORK / name
            ctx = await browser.new_context(viewport={"width": W, "height": H}, record_video_dir=str(vdir),
                                            record_video_size={"width": W, "height": H})
            await ctx.add_init_script(CURSOR_JS)
            # start in the view the clip needs
            await ctx.add_init_script(
                "try{localStorage.setItem('gcs.settings', JSON.stringify({view:'%s', overlays:{labels:true,vectors:true,links:true,targets:true}}))}catch(e){}" % view)
            t0 = time.monotonic()
            page = await ctx.new_page()
            await page.goto(URL)
            await page.wait_for_selector(".vehicle-row")
            await page.wait_for_timeout(1500)
            s = Scene(page, t0)
            await fn(s)
            video = page.video
            await ctx.close()
            webm = await video.path()
            gif = OUT / f"{name}.gif"
            to_gif(webm, s.start, gif)
            print(f"{gif}  ({gif.stat().st_size / 1e6:.1f} MB, from {s.start:.1f}s)", flush=True)
        await browser.close()


asyncio.run(main())
