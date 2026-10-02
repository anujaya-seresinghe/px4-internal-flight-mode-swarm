import { publish } from '../link';
import { useStore } from '../store';
import { VEHICLE_COMMANDS } from './px4';

// SwarmManagement.msg TYPE_* values
const SWARM_CREATE = 1;
const SWARM_UPDATE_FORMATION = 2;

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const label = (ids) => (ids.length === 1 ? `UAV ${ids[0]}` : `UAVs ${ids.join(', ')}`);

export const sendCommand = (ids, command, params) => {
  if (!ids.length) return false;
  const ok = publish('uav/command', { uav_ids: ids, command, ...(params ? { params } : {}) });
  if (ok) useStore.getState().log('cmd', `${VEHICLE_COMMANDS[command] || command} → ${label(ids)}`);
  return ok;
};

/**
 * Deploy a swarm following the order FlightTaskSwarm needs:
 * 1. every member into swarm mode (the task only listens while active)
 * 2. SWARM_MANAGEMENT (leader switches itself to Hold)
 * 3. one SWARM_NODE per member, spaced out because the uORB topics have a queue of 1
 * Messages are addressed to the members only, so other swarms are left alone.
 */
export const deploySwarm = async ({ swarmId, leaderId, members, offsets, external = false, onStep }) => {
  const { log, upsertSwarm } = useStore.getState();
  const step = (i) => onStep?.(i);

  upsertSwarm({ id: swarmId, leaderId, members, offsets, external, status: 'deploying', createdAt: Date.now() });
  log('cmd', `Deploying swarm ${swarmId} — leader UAV ${leaderId}, ${members.length} nodes, ${external ? 'ROS 2 external' : 'PX4 internal'} mode`);

  step(0);
  // external: the bridge selects the ROS 2 Swarm mode using the custom_mode its node advertises
  if (!sendCommand(members, 'swarm', external ? { external: true } : undefined)) {
    upsertSwarm({ id: swarmId, status: 'failed' });
    return false;
  }
  await sleep(1500);

  step(1);
  publish('uav/swarm_management', {
    type: SWARM_CREATE,
    swarm_id: swarmId,
    no_of_nodes: members.length,
    leader_id: leaderId,
    uav_ids: members,
  });
  await sleep(500);

  step(2);
  for (const id of members) {
    const o = offsets[id] || { x: 0, y: 0 };
    publish('uav/swarm_node', { swarm_id: swarmId, node_id: id, x: o.x, y: o.y, uav_ids: members });
    await sleep(150);
  }

  step(3);
  upsertSwarm({ id: swarmId, status: 'active' });
  log('success', `Swarm ${swarmId} deployed`);
  return true;
};

/**
 * Change the formation of a running swarm in place (same swarm ID, members and leader).
 * SWARM_MANAGEMENT type 2 makes FlightTaskSwarm keep the current formation while the new
 * offsets arrive, then switch to all of them at once. No mode changes are sent.
 */
export const updateFormation = async ({ swarm, offsets, onStep }) => {
  const { log, upsertSwarm } = useStore.getState();
  const step = (i) => onStep?.(i);
  const { id: swarmId, leaderId, members } = swarm;

  log('cmd', `Updating formation of swarm ${swarmId}`);
  step(0);
  const ok = publish('uav/swarm_management', {
    type: SWARM_UPDATE_FORMATION,
    swarm_id: swarmId,
    no_of_nodes: members.length,
    leader_id: leaderId,
    uav_ids: members,
  });
  if (!ok) return false;
  await sleep(300);

  step(1);
  for (const id of members) {
    const o = offsets[id] || { x: 0, y: 0 };
    publish('uav/swarm_node', { swarm_id: swarmId, node_id: id, x: o.x, y: o.y, uav_ids: members });
    await sleep(150);
  }

  step(2);
  upsertSwarm({ id: swarmId, offsets, status: 'active' });
  log('success', `Swarm ${swarmId} formation updated`);
  return true;
};

export const dissolveSwarm = (swarm) => {
  const { removeSwarm, log } = useStore.getState();
  sendCommand(swarm.members, 'hold');
  removeSwarm(swarm.id);
  log('info', `Swarm ${swarm.id} dissolved — members switched to Hold`);
};

/** Set the consensus weight of FlightTaskSwarm (PX4 parameter SWARM_WEIGHT) on these vehicles. */
export const setSwarmWeight = (ids, value) => {
  if (!ids.length || !Number.isFinite(value)) return false;
  const ok = publish('uav/command', { uav_ids: ids, command: 'set_param', params: { name: 'SWARM_WEIGHT', value } });
  if (ok) useStore.getState().log('cmd', `SWARM_WEIGHT = ${value} → ${label(ids)}`);
  return ok;
};

export const nextSwarmId = () => {
  const ids = Object.keys(useStore.getState().swarms).map(Number);
  let id = 1;
  while (ids.includes(id)) id++;
  return id;
};
