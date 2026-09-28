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
];

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
      if (!known) log('info', 'ROS 2 Swarm mode node online', `uav${id}`);
      break;
    }
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
