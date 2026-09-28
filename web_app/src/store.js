import { create } from 'zustand';

const FLUSH_MS = 100;
export const LINK_TIMEOUT_MS = 3000;

const loadSettings = () => {
  try {
    return JSON.parse(localStorage.getItem('gcs.settings')) || {};
  } catch {
    return {};
  }
};

const defaultBroker = () => `ws://${window.location.hostname || '127.0.0.1'}:9001`;

const saved = loadSettings();

export const useStore = create((set, get) => ({
  link: {
    // Always start on the real link; the simulator is only used via ?sim or the connection menu
    source: 'mqtt',
    url: saved.url || defaultBroker(),
    status: 'disconnected',
    error: null,
  },
  vehicles: {},
  selected: [],
  selectedSwarm: null, // a swarm selected as a unit (commands go to its leader)
  companions: {}, // sys_id -> { customMode, active, lastSeen } of the ROS 2 Swarm mode nodes
  swarms: {},
  logs: [],
  view: saved.view || 'split',
  follow: null,
  focus: { seq: 0, ids: [] },
  pendingGoto: null,
  overlays: { labels: true, vectors: true, links: true, targets: true, ...saved.overlays },

  setLink: (patch) => set((s) => ({ link: { ...s.link, ...patch } })),
  setView: (view) => set({ view }),
  setFollow: (follow) => set({ follow }),
  toggleOverlay: (key) => set((s) => ({ overlays: { ...s.overlays, [key]: !s.overlays[key] } })),
  requestFocus: (ids) => set((s) => ({ focus: { seq: s.focus.seq + 1, ids } })),
  setPendingGoto: (pendingGoto) => set({ pendingGoto }),

  // Selecting vehicles always clears a swarm selection (and vice versa)
  select: (ids, mode = 'replace') =>
    set((s) => {
      const list = Array.isArray(ids) ? ids : [ids];
      if (mode === 'toggle') {
        const next = new Set(s.selected);
        list.forEach((id) => (next.has(id) ? next.delete(id) : next.add(id)));
        return { selected: [...next].sort((a, b) => a - b), selectedSwarm: null };
      }
      if (mode === 'add') {
        return { selected: [...new Set([...s.selected, ...list])].sort((a, b) => a - b), selectedSwarm: null };
      }
      // 'click': clicking the only selected vehicle again deselects it
      if (mode === 'click' && s.selected.length === 1 && s.selected[0] === list[0]) {
        return { selected: [], selectedSwarm: null };
      }
      return { selected: [...list].sort((a, b) => a - b), selectedSwarm: null };
    }),

  // Clicking the selected swarm again deselects it
  selectSwarm: (id) => set((s) => ({ selectedSwarm: s.selectedSwarm === id ? null : id, selected: [] })),

  log: (level, text, source = 'gcs') =>
    set((s) => ({
      logs: [...s.logs.slice(-499), { id: `${Date.now()}-${Math.random()}`, t: Date.now(), level, text, source }],
    })),
  clearLogs: () => set({ logs: [] }),

  upsertSwarm: (swarm) => set((s) => ({ swarms: { ...s.swarms, [swarm.id]: { ...s.swarms[swarm.id], ...swarm } } })),
  removeSwarm: (id) =>
    set((s) => {
      const next = { ...s.swarms };
      delete next[id];
      return { swarms: next, selectedSwarm: s.selectedSwarm === id ? null : s.selectedSwarm };
    }),

  resetVehicles: () => {
    pending.clear();
    set({ vehicles: {}, companions: {}, selected: [], selectedSwarm: null, follow: null });
  },
}));

// Persist a few UI preferences
useStore.subscribe((s, prev) => {
  if (s.link === prev.link && s.view === prev.view && s.overlays === prev.overlays) return;
  try {
    localStorage.setItem(
      'gcs.settings',
      JSON.stringify({ url: s.link.url, view: s.view, overlays: s.overlays })
    );
  } catch {
    /* storage unavailable */
  }
});

// ---- Telemetry ingest -------------------------------------------------------
// Telemetry arrives at up to 50 Hz per vehicle; buffer and flush into the store at 10 Hz.

const pending = new Map();

export const ingest = (id, patch) => {
  const cur = pending.get(id);
  pending.set(id, cur ? { ...cur, ...patch } : patch);
};

const newVehicle = (id) => ({
  id,
  x: 0, y: 0, z: 0,
  vx: 0, vy: 0, vz: 0,
  roll: 0, pitch: 0, yaw: 0,
  armed: false,
  mainMode: 0,
  subMode: 0,
  systemStatus: null,
  landedState: null,
  battery: null,
  voltage: null,
  lat: null, lon: null, alt: null,
  firstSeen: Date.now(),
  lastSeen: Date.now(),
  hasPosition: false,
});

setInterval(() => {
  if (pending.size === 0) return;
  const now = Date.now();
  const { vehicles } = useStore.getState();
  const next = { ...vehicles };
  let added = [];
  pending.forEach((patch, id) => {
    const prev = next[id] || newVehicle(id);
    if (!next[id]) added.push(id);
    const v = { ...prev, ...patch, lastSeen: now };
    if (patch.x !== undefined) v.hasPosition = true;
    next[id] = v;
  });
  pending.clear();
  useStore.setState({ vehicles: next });
  added.forEach((id) => useStore.getState().log('info', `Vehicle ${id} discovered`, `uav${id}`));
}, FLUSH_MS);

export const isLinkLost = (v, now = Date.now()) => now - v.lastSeen > LINK_TIMEOUT_MS;

/** ROS 2 Swarm mode node of a vehicle, if it is alive */
export const liveCompanion = (companions, id, now = Date.now()) => {
  const c = companions[id];
  return c && c.customMode && now - c.lastSeen <= LINK_TIMEOUT_MS ? c : null;
};
