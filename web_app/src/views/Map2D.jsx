import React, { useEffect, useRef, useState } from 'react';
import { useStore, isLinkLost } from '../store';
import { COLORS, vehicleRoles, formationTargets } from '../lib/theme';
import { sendCommand } from '../lib/commands';
import { VehicleContextMenu } from './VehicleContextMenu';

// px per metre: 0.01 shows ~80 km on a typical map, enough for fixed-wing swarms
const MIN_SCALE = 0.01;
const MAX_SCALE = 200;
const HIT_RADIUS = 16;

// Pick a "nice" grid step (1, 2, 5 x 10^n) so lines are ~80px apart
const niceStep = (scale) => {
  const raw = 80 / scale;
  const pow = 10 ** Math.floor(Math.log10(raw));
  const n = raw / pow;
  return (n < 1.5 ? 1 : n < 3.5 ? 2 : n < 7.5 ? 5 : 10) * pow;
};

const fmtDist = (m) => (m >= 1000 ? `${(m / 1000).toFixed(m >= 10000 ? 0 : 1)} km` : `${m.toFixed(m < 10 ? 1 : 0)} m`);

export const Map2D = () => {
  const canvasRef = useRef(null);
  const wrapRef = useRef(null);
  const cam = useRef({ n: 0, e: 0, scale: 12 });
  const drag = useRef(null);
  const box = useRef(null);
  const cursor = useRef(null);
  const [menu, setMenu] = useState(null);
  const [readout, setReadout] = useState(null);
  const pendingGoto = useStore((s) => s.pendingGoto);

  // ---- coordinate transforms (north up, east right) ----
  const size = () => {
    const c = canvasRef.current;
    return { w: c.clientWidth, h: c.clientHeight };
  };
  const toScreen = (n, e) => {
    const { w, h } = size();
    const { n: cn, e: ce, scale } = cam.current;
    return [w / 2 + (e - ce) * scale, h / 2 - (n - cn) * scale];
  };
  const toWorld = (sx, sy) => {
    const { w, h } = size();
    const { n: cn, e: ce, scale } = cam.current;
    return { n: cn - (sy - h / 2) / scale, e: ce + (sx - w / 2) / scale };
  };
  const localPoint = (ev) => {
    const r = canvasRef.current.getBoundingClientRect();
    return [ev.clientX - r.left, ev.clientY - r.top];
  };
  const hitVehicle = (sx, sy) => {
    const { vehicles } = useStore.getState();
    let best = null;
    let bestD = HIT_RADIUS;
    Object.values(vehicles).forEach((v) => {
      if (!v.hasPosition) return;
      const [px, py] = toScreen(v.x, v.y);
      const d = Math.hypot(px - sx, py - sy);
      if (d < bestD) {
        best = v.id;
        bestD = d;
      }
    });
    return best;
  };

  const fitTo = (ids) => {
    const { vehicles } = useStore.getState();
    const pts = (ids.length ? ids : Object.keys(vehicles).map(Number))
      .map((id) => vehicles[id])
      .filter((v) => v?.hasPosition);
    if (!pts.length) return;
    const ns = pts.map((v) => v.x);
    const es = pts.map((v) => v.y);
    const { w, h } = size();
    const spanN = Math.max(...ns) - Math.min(...ns);
    const spanE = Math.max(...es) - Math.min(...es);
    cam.current.n = (Math.max(...ns) + Math.min(...ns)) / 2;
    cam.current.e = (Math.max(...es) + Math.min(...es)) / 2;
    const s = Math.min((w * 0.6) / Math.max(spanE, 1), (h * 0.6) / Math.max(spanN, 1));
    cam.current.scale = Math.max(MIN_SCALE, Math.min(40, s));
  };

  // Focus requests from the toolbar / sidebar
  const focusSeq = useStore((s) => s.focus.seq);
  useEffect(() => {
    if (focusSeq) fitTo(useStore.getState().focus.ids);
  }, [focusSeq]); // eslint-disable-line react-hooks/exhaustive-deps

  // Fit once when the first vehicles appear
  const vehicleCount = useStore((s) => Object.keys(s.vehicles).length);
  const fitted = useRef(false);
  useEffect(() => {
    if (vehicleCount && !fitted.current) {
      fitted.current = true;
      setTimeout(() => fitTo([]), 300);
    }
    if (!vehicleCount) fitted.current = false;
  }, [vehicleCount]); // eslint-disable-line react-hooks/exhaustive-deps

  // ---- render loop ----
  useEffect(() => {
    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    let raf;

    const draw = () => {
      raf = requestAnimationFrame(draw);
      const dpr = window.devicePixelRatio || 1;
      const w = canvas.clientWidth;
      const h = canvas.clientHeight;
      if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
        canvas.width = Math.round(w * dpr);
        canvas.height = Math.round(h * dpr);
      }
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

      const { vehicles, selected, selectedSwarm, swarms, overlays, follow, pendingGoto: pg } = useStore.getState();
      const swarmSel = swarms[selectedSwarm]?.members || [];
      if (follow && vehicles[follow]?.hasPosition) {
        cam.current.n += (vehicles[follow].x - cam.current.n) * 0.15;
        cam.current.e += (vehicles[follow].y - cam.current.e) * 0.15;
      }
      const { scale } = cam.current;
      const now = Date.now();
      const roles = vehicleRoles(swarms);

      ctx.fillStyle = COLORS.bg;
      ctx.fillRect(0, 0, w, h);

      // Grid
      const step = niceStep(scale);
      const tl = toWorld(0, 0);
      const br = toWorld(w, h);
      ctx.lineWidth = 1;
      for (let e = Math.floor(tl.e / step) * step; e <= br.e; e += step) {
        const [x] = toScreen(0, e);
        const major = Math.round(e / step) % 5 === 0;
        ctx.strokeStyle = Math.abs(e) < step / 2 ? COLORS.axis : major ? COLORS.gridMajor : COLORS.grid;
        ctx.beginPath();
        ctx.moveTo(Math.round(x) + 0.5, 0);
        ctx.lineTo(Math.round(x) + 0.5, h);
        ctx.stroke();
      }
      for (let n = Math.floor(br.n / step) * step; n <= tl.n; n += step) {
        const [, y] = toScreen(n, 0);
        const major = Math.round(n / step) % 5 === 0;
        ctx.strokeStyle = Math.abs(n) < step / 2 ? COLORS.axis : major ? COLORS.gridMajor : COLORS.grid;
        ctx.beginPath();
        ctx.moveTo(0, Math.round(y) + 0.5);
        ctx.lineTo(w, Math.round(y) + 0.5);
        ctx.stroke();
      }

      // Home / origin
      const [hx, hy] = toScreen(0, 0);
      ctx.strokeStyle = COLORS.success;
      ctx.fillStyle = 'rgba(52, 211, 153, 0.12)';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.arc(hx, hy, 9, 0, Math.PI * 2);
      ctx.fill();
      ctx.stroke();
      ctx.font = '600 10px Inter, sans-serif';
      ctx.fillStyle = COLORS.success;
      ctx.textAlign = 'center';
      ctx.fillText('H', hx, hy + 3.5);

      const list = Object.values(vehicles).filter((v) => v.hasPosition);

      // Swarm links + formation slots
      Object.values(swarms).forEach((s) => {
        const leader = vehicles[s.leaderId];
        if (!leader?.hasPosition) return;
        const color = roles[s.leaderId]?.color;
        const [lx, ly] = toScreen(leader.x, leader.y);
        if (overlays.links) {
          ctx.strokeStyle = color;
          ctx.globalAlpha = 0.5;
          ctx.setLineDash([4, 5]);
          ctx.lineWidth = 1.2;
          s.members.forEach((id) => {
            const m = vehicles[id];
            if (id === s.leaderId || !m?.hasPosition) return;
            ctx.beginPath();
            ctx.moveTo(lx, ly);
            ctx.lineTo(...toScreen(m.x, m.y));
            ctx.stroke();
          });
          ctx.setLineDash([]);
          ctx.globalAlpha = 1;
        }
        if (overlays.targets) {
          formationTargets(s, vehicles).forEach((t) => {
            const [tx, ty] = toScreen(t.x, t.y);
            ctx.strokeStyle = color;
            ctx.lineWidth = 1.2;
            ctx.globalAlpha = 0.8;
            ctx.beginPath();
            ctx.arc(tx, ty, 7, 0, Math.PI * 2);
            ctx.moveTo(tx - 11, ty);
            ctx.lineTo(tx - 4, ty);
            ctx.moveTo(tx + 4, ty);
            ctx.lineTo(tx + 11, ty);
            ctx.moveTo(tx, ty - 11);
            ctx.lineTo(tx, ty - 4);
            ctx.moveTo(tx, ty + 4);
            ctx.lineTo(tx, ty + 11);
            ctx.stroke();
            ctx.globalAlpha = 1;
          });
        }
      });

      // Vehicles
      list.forEach((v) => {
        const [x, y] = toScreen(v.x, v.y);
        const role = roles[v.id];
        const lost = isLinkLost(v, now);
        const isSel = selected.includes(v.id) || swarmSel.includes(v.id);
        const color = lost ? COLORS.danger : role?.isLeader ? COLORS.leader : role?.color || COLORS.vehicle;

        if (overlays.vectors) {
          const sp = Math.hypot(v.vx, v.vy);
          if (sp > 0.3) {
            const [ex, ey] = toScreen(v.x + v.vx * 2, v.y + v.vy * 2);
            ctx.strokeStyle = color;
            ctx.globalAlpha = 0.7;
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.moveTo(x, y);
            ctx.lineTo(ex, ey);
            ctx.stroke();
            ctx.globalAlpha = 1;
          }
        }

        if (isSel) {
          ctx.strokeStyle = COLORS.select;
          ctx.fillStyle = 'rgba(56, 189, 248, 0.12)';
          ctx.lineWidth = 2;
          ctx.beginPath();
          ctx.arc(x, y, 19, 0, Math.PI * 2);
          ctx.fill();
          ctx.stroke();
        }

        ctx.save();
        ctx.translate(x, y);
        ctx.rotate(v.yaw);
        ctx.fillStyle = color;
        ctx.strokeStyle = COLORS.bg;
        ctx.lineWidth = 2;
        ctx.globalAlpha = v.armed || lost ? 1 : 0.65;
        ctx.beginPath();
        ctx.moveTo(0, -14);
        ctx.lineTo(10, 10);
        ctx.lineTo(0, 5);
        ctx.lineTo(-10, 10);
        ctx.closePath();
        ctx.stroke();
        ctx.fill();
        ctx.restore();
        ctx.globalAlpha = 1;

        if (role?.isLeader) {
          ctx.fillStyle = COLORS.leader;
          ctx.font = '700 9px Inter, sans-serif';
          ctx.textAlign = 'center';
          ctx.fillText('LEAD', x, y - 22);
        }

        if (overlays.labels) {
          ctx.textAlign = 'left';
          ctx.font = '600 12px Inter, sans-serif';
          ctx.fillStyle = COLORS.text;
          ctx.fillText(`UAV ${v.id}`, x + 18, y - 2);
          ctx.font = '500 10px "JetBrains Mono", monospace';
          ctx.fillStyle = lost ? COLORS.danger : COLORS.textDim;
          ctx.fillText(lost ? 'NO LINK' : `${(-v.z).toFixed(1)} m`, x + 18, y + 11);
        }
      });

      // Pending goto marker follows the cursor
      if (pg && cursor.current) {
        const [cx, cy] = cursor.current;
        ctx.strokeStyle = COLORS.accent;
        ctx.lineWidth = 1.5;
        ctx.setLineDash([3, 3]);
        pg.ids.forEach((id) => {
          const v = vehicles[id];
          if (!v?.hasPosition) return;
          ctx.beginPath();
          ctx.moveTo(...toScreen(v.x, v.y));
          ctx.lineTo(cx, cy);
          ctx.stroke();
        });
        ctx.setLineDash([]);
      }

      // Selection box
      if (box.current) {
        const { x0, y0, x1, y1 } = box.current;
        ctx.fillStyle = 'rgba(56, 189, 248, 0.1)';
        ctx.strokeStyle = COLORS.select;
        ctx.lineWidth = 1;
        ctx.setLineDash([5, 4]);
        ctx.fillRect(Math.min(x0, x1), Math.min(y0, y1), Math.abs(x1 - x0), Math.abs(y1 - y0));
        ctx.strokeRect(Math.min(x0, x1) + 0.5, Math.min(y0, y1) + 0.5, Math.abs(x1 - x0), Math.abs(y1 - y0));
        ctx.setLineDash([]);
      }

      // Scale bar (bottom left)
      const barPx = step * scale;
      ctx.strokeStyle = COLORS.text;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.moveTo(16, h - 18);
      ctx.lineTo(16, h - 13);
      ctx.lineTo(16 + barPx, h - 13);
      ctx.lineTo(16 + barPx, h - 18);
      ctx.stroke();
      ctx.font = '500 10px "JetBrains Mono", monospace';
      ctx.fillStyle = COLORS.text;
      ctx.textAlign = 'left';
      ctx.fillText(fmtDist(step), 20 + barPx, h - 12);

      // North indicator (top left)
      ctx.save();
      ctx.translate(28, 30);
      ctx.fillStyle = COLORS.danger;
      ctx.beginPath();
      ctx.moveTo(0, -12);
      ctx.lineTo(6, 4);
      ctx.lineTo(0, 1);
      ctx.lineTo(-6, 4);
      ctx.closePath();
      ctx.fill();
      ctx.font = '700 10px Inter, sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText('N', 0, 17);
      ctx.restore();
    };
    draw();
    return () => cancelAnimationFrame(raf);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // ---- interaction ----
  const onWheel = (ev) => {
    const [sx, sy] = localPoint(ev);
    const before = toWorld(sx, sy);
    const factor = Math.exp(-ev.deltaY * 0.0015);
    cam.current.scale = Math.max(MIN_SCALE, Math.min(MAX_SCALE, cam.current.scale * factor));
    const after = toWorld(sx, sy);
    cam.current.n += before.n - after.n;
    cam.current.e += before.e - after.e;
  };

  useEffect(() => {
    const c = canvasRef.current;
    const handler = (ev) => {
      ev.preventDefault();
      onWheel(ev);
    };
    c.addEventListener('wheel', handler, { passive: false });
    return () => c.removeEventListener('wheel', handler);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const onPointerDown = (ev) => {
    setMenu(null);
    const [sx, sy] = localPoint(ev);
    if (ev.button === 2) return;
    canvasRef.current.setPointerCapture(ev.pointerId);
    if (ev.shiftKey && ev.button === 0) {
      box.current = { x0: sx, y0: sy, x1: sx, y1: sy };
      return;
    }
    drag.current = { sx, sy, n: cam.current.n, e: cam.current.e, moved: false, button: ev.button };
  };

  const onPointerMove = (ev) => {
    const [sx, sy] = localPoint(ev);
    cursor.current = [sx, sy];
    const w = toWorld(sx, sy);
    setReadout({ n: w.n, e: w.e });
    if (box.current) {
      box.current.x1 = sx;
      box.current.y1 = sy;
      return;
    }
    const d = drag.current;
    if (!d) return;
    if (Math.hypot(sx - d.sx, sy - d.sy) > 3) {
      d.moved = true;
      if (useStore.getState().follow) useStore.getState().setFollow(null);
    }
    if (d.moved) {
      cam.current.n = d.n + (sy - d.sy) / cam.current.scale;
      cam.current.e = d.e - (sx - d.sx) / cam.current.scale;
    }
  };

  const onPointerUp = (ev) => {
    const [sx, sy] = localPoint(ev);
    const { select, pendingGoto: pg, setPendingGoto } = useStore.getState();

    if (box.current) {
      const { x0, y0, x1, y1 } = box.current;
      box.current = null;
      const { vehicles } = useStore.getState();
      const ids = Object.values(vehicles)
        .filter((v) => {
          if (!v.hasPosition) return false;
          const [px, py] = toScreen(v.x, v.y);
          return px >= Math.min(x0, x1) && px <= Math.max(x0, x1) && py >= Math.min(y0, y1) && py <= Math.max(y0, y1);
        })
        .map((v) => v.id);
      select(ids, ev.ctrlKey || ev.metaKey ? 'add' : 'replace');
      return;
    }

    const d = drag.current;
    drag.current = null;
    if (!d || d.moved || d.button !== 0) return;

    if (pg) {
      const w = toWorld(sx, sy);
      sendCommand(pg.ids, 'goto', { north: w.n, east: w.e });
      setPendingGoto(null);
      return;
    }

    const hit = hitVehicle(sx, sy);
    if (hit !== null) select(hit, ev.ctrlKey || ev.metaKey ? 'toggle' : 'click');
    else if (!ev.ctrlKey && !ev.metaKey) select([]);
  };

  const onDoubleClick = (ev) => {
    const [sx, sy] = localPoint(ev);
    const hit = hitVehicle(sx, sy);
    if (hit !== null) useStore.getState().setFollow(hit);
  };

  const onContextMenu = (ev) => {
    ev.preventDefault();
    const [sx, sy] = localPoint(ev);
    const hit = hitVehicle(sx, sy);
    const { selected, select, selectedSwarm, swarms } = useStore.getState();
    const w = toWorld(sx, sy);
    const { w: vw, h: vh } = size();
    // Keep the menu inside the view (approximate menu size)
    const place = (height) => ({
      x: Math.max(4, Math.min(sx, vw - 196)),
      y: Math.max(4, Math.min(sy, vh - height)),
    });

    // A selected swarm owns the menu unless the click lands on a vehicle outside it
    const swarm = swarms[selectedSwarm];
    if (swarm && (hit === null || swarm.members.includes(hit))) {
      setMenu({ ...place(170), swarm, world: w });
      return;
    }

    let ids = selected;
    if (hit !== null && !selected.includes(hit)) {
      select(hit);
      ids = [hit];
    }
    setMenu({ ...place(ids.length ? 290 : 80), ids, world: w, onVehicle: hit !== null });
  };

  useEffect(() => {
    const onKey = (ev) => {
      if (ev.key === 'Escape') {
        useStore.getState().setPendingGoto(null);
        setMenu(null);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  return (
    <div ref={wrapRef} className="view view-2d">
      <canvas
        ref={canvasRef}
        className={pendingGoto ? 'picking' : ''}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onPointerLeave={() => {
          cursor.current = null;
          setReadout(null);
        }}
        onDoubleClick={onDoubleClick}
        onContextMenu={onContextMenu}
      />
      {pendingGoto && (
        <div className="view-banner">
          Click the map to send UAV {pendingGoto.ids.join(', ')} there · <kbd>Esc</kbd> to cancel
        </div>
      )}
      {readout && (
        <div className="coord-readout mono">
          N {readout.n.toFixed(1)} · E {readout.e.toFixed(1)}
        </div>
      )}
      {menu && <VehicleContextMenu menu={menu} onClose={() => setMenu(null)} />}
    </div>
  );
};
