// Colours shared by the canvas (2D) and WebGL (3D) views; keep in sync with styles.css
export const COLORS = {
  bg: '#0a0f1a',
  grid: '#152033',
  gridMajor: '#1f2e47',
  axis: '#2c4166',
  text: '#c9d4e5',
  textDim: '#6b7a93',
  accent: '#38bdf8',
  vehicle: '#94a3b8',
  leader: '#fbbf24',
  danger: '#f87171',
  success: '#34d399',
  select: '#38bdf8',
};

const SWARM_PALETTE = ['#a78bfa', '#34d399', '#f472b6', '#60a5fa', '#fb923c', '#2dd4bf'];

export const swarmColor = (swarmId) => SWARM_PALETTE[(swarmId - 1) % SWARM_PALETTE.length];

/** Map of vehicle id -> { swarm, isLeader, color } */
export const vehicleRoles = (swarms) => {
  const roles = {};
  Object.values(swarms).forEach((s) => {
    s.members.forEach((id) => {
      roles[id] = { swarm: s, isLeader: s.leaderId === id, color: swarmColor(s.id) };
    });
  });
  return roles;
};

/** Formation slot (local NED) each follower is steering towards, given the leader position. */
export const formationTargets = (swarm, vehicles) => {
  const leader = vehicles[swarm.leaderId];
  const lo = swarm.offsets[swarm.leaderId];
  if (!leader || !lo) return [];
  return swarm.members
    .filter((id) => id !== swarm.leaderId && swarm.offsets[id])
    .map((id) => ({
      id,
      x: leader.x + swarm.offsets[id].x - lo.x,
      y: leader.y + swarm.offsets[id].y - lo.y,
      z: leader.z,
    }));
};
