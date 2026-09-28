// Formation offsets are relative to the leader in local NED metres ({x: north, y: east}).
// FlightTaskSwarm uses them as fixed NED offsets (offset = own - other), they do not
// rotate with the leader's heading — use the formation heading to orient them.

// APF in FlightTaskSwarm engages when two nodes are within 2 * _DELTA_R horizontally
export const APF_RADIUS = 6;

export const FORMATIONS = {
  line: { label: 'Line abreast' },
  column: { label: 'Column' },
  wedge: { label: 'Wedge (V)' },
  echelon: { label: 'Echelon' },
  circle: { label: 'Circle' },
  grid: { label: 'Grid' },
  custom: { label: 'Custom' },
};

// Slots for n followers, heading north. Returns [{x, y}]
const presetSlots = (type, n, s) => {
  const slots = [];
  for (let i = 0; i < n; i++) {
    const k = Math.floor(i / 2) + 1;
    const side = i % 2 === 0 ? 1 : -1;
    switch (type) {
      case 'line':
        slots.push({ x: 0, y: side * k * s });
        break;
      case 'column':
        slots.push({ x: -(i + 1) * s, y: 0 });
        break;
      case 'wedge':
        slots.push({ x: -k * s, y: side * k * s });
        break;
      case 'echelon':
        slots.push({ x: -(i + 1) * s, y: (i + 1) * s });
        break;
      case 'circle': {
        const r = Math.max(s, (s * n) / (2 * Math.PI));
        const a = (2 * Math.PI * i) / n;
        slots.push({ x: r * Math.cos(a), y: r * Math.sin(a) });
        break;
      }
      case 'grid': {
        const cols = Math.ceil(Math.sqrt(n + 1));
        const idx = i + 1; // leader occupies slot 0
        const row = Math.floor(idx / cols);
        const col = idx % cols;
        slots.push({ x: -row * s, y: col * s });
        break;
      }
      default:
        slots.push({ x: -(i + 1) * s, y: 0 });
    }
  }
  return slots;
};

const rotate = ({ x, y }, headingDeg) => {
  const a = (headingDeg * Math.PI) / 180;
  const c = Math.cos(a);
  const s = Math.sin(a);
  // NED: rotate clockwise (viewed from above) by heading
  return { x: x * c - y * s, y: x * s + y * c };
};

const round = (v) => Math.round(v * 10) / 10;

/** Build {id: {x, y}} offsets for leader + followers. */
export const buildFormation = (type, leaderId, followerIds, spacing, headingDeg = 0) => {
  const offsets = { [leaderId]: { x: 0, y: 0 } };
  presetSlots(type, followerIds.length, spacing).forEach((slot, i) => {
    const r = rotate(slot, headingDeg);
    offsets[followerIds[i]] = { x: round(r.x), y: round(r.y) };
  });
  return offsets;
};

/** Pairs of nodes closer than APF_RADIUS. */
export const tightPairs = (offsets) => {
  const ids = Object.keys(offsets);
  const pairs = [];
  for (let i = 0; i < ids.length; i++) {
    for (let j = i + 1; j < ids.length; j++) {
      const a = offsets[ids[i]];
      const b = offsets[ids[j]];
      const d = Math.hypot(a.x - b.x, a.y - b.y);
      if (d < APF_RADIUS) pairs.push({ a: ids[i], b: ids[j], d });
    }
  }
  return pairs;
};
