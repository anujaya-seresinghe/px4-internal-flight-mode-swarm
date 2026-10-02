import mqtt from 'mqtt';
import { useStore, ingest } from '../store';
import { decodeCustomMode, COMMAND_RESULT, severityLevel } from '../lib/px4';
import { createSimulator } from './simulator';

const MAV_CMD_NAMES = {
  20: 'RTL',
  21: 'LAND',
  22: 'TAKEOFF',
  84: 'VTOL TAKEOFF',
  176: 'SET_MODE',
  192: 'REPOSITION',
  400: 'ARM/DISARM',
  3000: 'VTOL TRANSITION',
};

const TELEMETRY_TOPICS = [
  'uav/attitude',
  'uav/local_position_ned',
  'uav/heartbeat',
  'uav/global_position',
  'uav/sys_status',
  'uav/extended_sys_state',
  'uav/command_ack',
  'uav/statustext',
  'uav/companion',
  'uav/param',
  'uav/swarm_status',
];

// ---- Swarm restore -----------------------------------------------------------------------
// The GCS keeps no swarm state of its own across reloads. Every vehicle is asked for SWARM_STATUS
// (MAVLink 603) once when it is discovered; followers report their complete formation, and any
// swarm the GCS does not know yet is rebuilt from that.
const SWARM_STATE = { NONE: 0, IDLE: 1, COLLECTING: 2, ACTIVE: 3, UPDATING: 4 };
const swarmStatusRequested = new Set(); // sys_ids asked since connecting

export const requestSwarmStatus = (ids) => {
  if (!ids.length) return;
  transport?.publish('uav/command', { uav_ids: ids, command: 'swarm_status' });
};

const restoreSwarm = (r) => {
  if (r.state < SWARM_STATE.COLLECTING || !r.swarm_id || !r.nodes?.length) return;
  const { swarms, upsertSwarm, log } = useStore.getState();
  const existing = swarms[r.swarm_id];
  // Swarms deployed in this session are already known (and may be mid-deployment)
  if (existing && !existing.restored) return;

  const members = r.nodes.map((n) => n.id).sort((a, b) => a - b);
  const offsets = Object.fromEntries(r.nodes.map((n) => [n.id, { x: n.x, y: n.y }]));
  const swarm = {
    id: r.swarm_id,
    leaderId: r.leader_id,
    members,
    offsets,
    external: r.source === 'ros2',
    status: r.state === SWARM_STATE.COLLECTING ? 'deploying' : 'active',
    restored: true,
  };
  if (!existing) {
    upsertSwarm({ ...swarm, createdAt: Date.now() });
    log('success', `Swarm ${r.swarm_id} restored from UAV ${r.sys_id} — leader UAV ${r.leader_id}, ${members.length} nodes, ${swarm.external ? 'ROS 2 external' : 'PX4 internal'} mode`);
  } else {
    upsertSwarm(swarm);
  }
};

// PX4 parameters the GCS shows; read once from every vehicle when it is discovered
export const WATCHED_PARAMS = ['SWARM_WEIGHT'];
const PARAM_RETRY_MS = 3000;
const PARAM_MAX_TRIES = 5;
const paramRequests = new Map(); // `${id}/${name}` -> { tries, last }

const requestMissingParams = (id) => {
  const known = useStore.getState().params[id] || {};
  const now = Date.now();
  WATCHED_PARAMS.forEach((name) => {
    if (known[name] !== undefined) return;
    const key = `${id}/${name}`;
    const r = paramRequests.get(key) || { tries: 0, last: 0 };
    if (r.tries >= PARAM_MAX_TRIES || now - r.last < PARAM_RETRY_MS) return;
    paramRequests.set(key, { tries: r.tries + 1, last: now });
    transport?.publish('uav/command', { uav_ids: [id], command: 'get_param', params: { name } });
  });
};

let transport = null; // { publish(topic, payload), close() }

const handleMessage = (topic, payload) => {
  const id = payload?.sys_id;
  if (id === undefined || id === null) return;
  const { log } = useStore.getState();

  switch (topic) {
    case 'uav/attitude':
      ingest(id, { roll: payload.roll, pitch: payload.pitch, yaw: payload.yaw });
      break;
    case 'uav/local_position_ned':
      ingest(id, { x: payload.x, y: payload.y, z: payload.z, vx: payload.vx ?? 0, vy: payload.vy ?? 0, vz: payload.vz ?? 0 });
      break;
    case 'uav/heartbeat': {
      const { main, sub } = decodeCustomMode(payload.custom_mode >>> 0);
      ingest(id, {
        armed: (payload.base_mode & 128) !== 0,
        mainMode: main,
        subMode: sub,
        systemStatus: payload.system_status,
      });
      requestMissingParams(id);
      if (!swarmStatusRequested.has(id)) {
        swarmStatusRequested.add(id);
        requestSwarmStatus([id]);
      }
      break;
    }
    case 'uav/global_position':
      ingest(id, { lat: payload.lat, lon: payload.lon, alt: payload.alt });
      break;
    case 'uav/sys_status':
      ingest(id, {
        battery: payload.battery_remaining >= 0 ? payload.battery_remaining : null,
        voltage: payload.voltage > 0 ? payload.voltage : null,
      });
      break;
    case 'uav/extended_sys_state':
      ingest(id, { landedState: payload.landed_state, vtolState: payload.vtol_state ?? 0 });
      break;
    case 'uav/command_ack': {
      const name = MAV_CMD_NAMES[payload.command] || `CMD ${payload.command}`;
      const result = COMMAND_RESULT[payload.result] ?? payload.result;
      log(payload.result === 0 ? 'success' : payload.result === 5 ? 'info' : 'error', `${name}: ${result}`, `uav${id}`);
      break;
    }
    case 'uav/companion': {
      // ROS 2 Swarm mode node of this vehicle: which custom_mode selects the external mode
      const { companions } = useStore.getState();
      const known = companions[id];
      useStore.setState({
        companions: { ...companions, [id]: { customMode: payload.custom_mode >>> 0, active: !!payload.active, lastSeen: Date.now() } },
      });
      if (!known) {
        log('info', 'ROS 2 Swarm mode node online', `uav${id}`);
        // The bridge only asks nodes it knows: ask again now that this one is online
        requestSwarmStatus([id]);
      }
      break;
    }
    case 'uav/param': {
      const { params, setParam } = useStore.getState();
      const prev = params[id]?.[payload.name];
      // REAL32 on the wire: round away the float noise (0.4 -> 0.4000000059604645)
      const value = Number(Number(payload.value).toPrecision(6));
      setParam(id, payload.name, value);
      if (prev !== undefined && prev !== value) log('info', `${payload.name} = ${value}`, `uav${id}`);
      break;
    }
    case 'uav/swarm_status':
      restoreSwarm(payload);
      break;
    case 'uav/statustext':
      log(severityLevel(payload.severity), payload.text, `uav${id}`);
      break;
    default:
  }
};

export const disconnect = () => {
  transport?.close();
  transport = null;
  useStore.getState().setLink({ status: 'disconnected' });
};

export const connect = () => {
  disconnect();
  const { link, setLink, log, resetVehicles } = useStore.getState();
  resetVehicles();
  paramRequests.clear();
  swarmStatusRequested.clear();

  if (link.source === 'sim') {
    transport = createSimulator(handleMessage);
    setLink({ status: 'connected', error: null });
    log('info', 'Connected to built-in simulator');
    return;
  }

  setLink({ status: 'connecting', error: null });
  let client;
  try {
    client = mqtt.connect(link.url, { reconnectPeriod: 2000, connectTimeout: 5000 });
  } catch (err) {
    setLink({ status: 'error', error: err.message });
    log('error', `Invalid broker URL: ${err.message}`);
    return;
  }

  client.on('connect', () => {
    client.subscribe(TELEMETRY_TOPICS);
    setLink({ status: 'connected', error: null });
    log('success', `Connected to MQTT broker ${link.url}`);
  });
  client.on('reconnect', () => setLink({ status: 'connecting' }));
  client.on('close', () => {
    if (useStore.getState().link.status === 'connected') log('warn', 'MQTT connection lost');
    if (transport?.client === client) setLink({ status: 'connecting' });
  });
  client.on('error', (err) => setLink({ status: 'error', error: err.message }));
  client.on('message', (topic, message) => {
    try {
      handleMessage(topic, JSON.parse(message.toString()));
    } catch {
      /* ignore malformed payloads */
    }
  });

  transport = {
    client,
    publish: (topic, payload) => client.publish(topic, JSON.stringify(payload)),
    close: () => client.end(true),
  };
};

export const publish = (topic, payload) => {
  if (!transport || useStore.getState().link.status !== 'connected') {
    useStore.getState().log('error', `Not connected — could not send ${topic}`);
    return false;
  }
  transport.publish(topic, payload);
  return true;
};
