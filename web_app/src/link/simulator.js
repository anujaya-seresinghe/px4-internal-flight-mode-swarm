// In-browser stand-in for the MQTT bridge + PX4 SITL. It speaks the same topics as
// mavlink_mqtt_bridge and mirrors FlightTaskSwarm's consensus law, so the GCS can be
// exercised without Gazebo.

const HOME = { lat: 47.397742, lon: 8.545594, alt: 488 };
const R_EARTH = 6378137;
const N_VEHICLES = 5;
const TAKEOFF_ALT = 5;
const V_MAX = 8;
const DT = 0.05;

const MODE = {
  POSITION: [3, 0],
  TAKEOFF: [4, 2],
  HOLD: [4, 3],
  RTL: [4, 5],
  LAND: [4, 6],
  SWARM: [4, 11],
  SWARM_EXT: [4, 12], // ROS 2 external Swarm mode (EXTERNAL1 in this repo's PX4)
};

const inSwarmMode = (v) => v.mode === MODE.SWARM || v.mode === MODE.SWARM_EXT;
const clamp = (v, lim) => Math.max(-lim, Math.min(lim, v));
const wrapPi = (a) => Math.atan2(Math.sin(a), Math.cos(a));

const makeVehicle = (id, i) => ({
  id,
  pos: [0, i * 3, 0],
  vel: [0, 0, 0],
  yaw: 0,
  roll: 0,
  pitch: 0,
  armed: false,
  mode: MODE.HOLD,
  landed: true,
  battery: 100,
  target: null,
  targetYaw: null,
  landTimer: 0,
  params: { SWARM_WEIGHT: 1 }, // PX4 parameters (only what the GCS uses)
  swarm: { id: 0, nodes: 0, leader: 0, list: [], consensus: [], pending: null },
});

export const createSimulator = (emit) => {
  const vehicles = Array.from({ length: N_VEHICLES }, (_, i) => makeVehicle(i + 1, i));
  const byId = (id) => vehicles.find((v) => v.id === id);
  let tick = 0;

  const ack = (v, command, result = 0) => emit('uav/command_ack', { sys_id: v.id, command, result });
  const text = (v, severity, t) => emit('uav/statustext', { sys_id: v.id, severity, text: t });

  const setMode = (v, mode) => {
    v.mode = mode;
    if (mode === MODE.HOLD || mode === MODE.POSITION) v.target = [...v.pos];
    if (mode === MODE.SWARM || mode === MODE.SWARM_EXT) resetSwarm(v);
  };

  const resetSwarm = (v) => {
    v.swarm = { id: 0, nodes: 0, leader: 0, list: [], consensus: [], pending: null };
  };

  // SWARM_STATUS (603) as sent by FlightTaskSwarm / the ROS 2 node running `mode`
  const swarmStatus = (v, mode, source) => {
    const s = v.swarm;
    const base = { sys_id: v.id, source, state: 0, swarm_id: 0, leader_id: 0, no_of_nodes: 0, nodes: [] };
    if (v.mode !== mode) return base;
    const state = s.pending ? 4 : s.id === 0 ? 1 : s.list.length === s.nodes ? 3 : 2;
    return { ...base, state, swarm_id: s.id, leader_id: s.leader, no_of_nodes: s.nodes, nodes: s.list.map((n) => ({ ...n })) };
  };

  const handleCommand = (v, command, params = {}) => {
    switch (command) {
      case 'arm':
        if (!v.landed) return ack(v, 400, 0);
        v.armed = true;
        text(v, 6, 'Armed by external command');
        return ack(v, 400, 0);
      case 'disarm':
        if (!v.landed) {
          text(v, 4, 'Disarming denied: not landed');
          return ack(v, 400, 1);
        }
        v.armed = false;
        text(v, 6, 'Disarmed by external command');
        return ack(v, 400, 0);
      case 'kill':
        v.armed = false;
        text(v, 2, 'Kill switch engaged');
        return ack(v, 400, 0);
      case 'takeoff':
        v.armed = true;
        v.landed = false;
        v.mode = MODE.TAKEOFF;
        v.target = [v.pos[0], v.pos[1], -(params.altitude ?? TAKEOFF_ALT)];
        text(v, 6, 'Takeoff detected');
        return ack(v, 22, 0);
      case 'land':
        if (v.landed) return ack(v, 21, 1);
        setMode(v, MODE.LAND);
        return ack(v, 21, 0);
      case 'rtl':
        if (v.landed) return ack(v, 20, 1);
        setMode(v, MODE.RTL);
        return ack(v, 20, 0);
      case 'hold':
      case 'position':
        if (v.landed) {
          v.mode = command === 'hold' ? MODE.HOLD : MODE.POSITION;
          return ack(v, 176, 0);
        }
        setMode(v, command === 'hold' ? MODE.HOLD : MODE.POSITION);
        return ack(v, 176, 0);
      case 'swarm':
        if (!v.armed || v.landed) {
          text(v, 4, 'Swarm mode requires the vehicle to be flying');
          return ack(v, 176, 2);
        }
        setMode(v, params.external ? MODE.SWARM_EXT : MODE.SWARM);
        return ack(v, 176, 0);
      case 'goto': {
        if (v.landed) return ack(v, 192, 2);
        const t = [...v.pos];
        if (Number.isFinite(params.north)) t[0] = params.north;
        if (Number.isFinite(params.east)) t[1] = params.east;
        if (Number.isFinite(params.up)) t[2] = -params.up;
        v.mode = MODE.HOLD;
        v.target = t;
        return ack(v, 192, 0);
      }
      case 'swarm_status':
        // Like the bridge: PX4's Swarm task and the vehicle's ROS 2 Swarm mode node both answer
        emit('uav/swarm_status', swarmStatus(v, MODE.SWARM, 'px4'));
        emit('uav/swarm_status', swarmStatus(v, MODE.SWARM_EXT, 'ros2'));
        return undefined;
      case 'set_param':
        // PX4 rejects unknown names and clamps nothing; it echoes PARAM_VALUE on success
        if (!(params.name in v.params)) return;
        v.params[params.name] = Math.fround(params.value);
        return emit('uav/param', { sys_id: v.id, name: params.name, value: v.params[params.name] });
      case 'get_param':
        if (!(params.name in v.params)) return;
        return emit('uav/param', { sys_id: v.id, name: params.name, value: v.params[params.name] });
      default:
        return ack(v, 0, 3);
    }
  };

  // Mirrors FlightTaskSwarm::update() message handling (only while in swarm mode)
  const addressed = (m, v) => !m.uav_ids || m.uav_ids.includes(v.id);

  const buildConsensus = (v, list) => {
    const own = list.find((n) => n.id === v.id);
    v.swarm.consensus = own
      ? list.filter((n) => n.id !== v.id).map((n) => ({ id: n.id, ox: own.x - n.x, oy: own.y - n.y }))
      : [];
  };

  const handleSwarmManagement = (m) => {
    vehicles.forEach((v) => {
      if (!inSwarmMode(v) || !addressed(m, v)) return;
      // type 2: same swarm, new offsets. Keep the current formation until all of them arrived
      if (m.type === 2 && v.swarm.id !== 0 && v.swarm.id === m.swarm_id) {
        v.swarm.pending = { expected: m.no_of_nodes, leader: m.leader_id, list: [] };
        return;
      }
      if (v.swarm.id !== m.swarm_id) resetSwarm(v);
      v.swarm.pending = null;
      v.swarm.id = m.swarm_id;
      v.swarm.nodes = m.no_of_nodes;
      v.swarm.leader = m.leader_id;
      if (m.leader_id === v.id) {
        setMode(v, MODE.HOLD);
        text(v, 6, `Designated leader of swarm ${m.swarm_id}`);
      }
    });
  };

  const handleSwarmNode = (m) => {
    vehicles.forEach((v) => {
      if (!inSwarmMode(v) || !addressed(m, v)) return;
      const s = v.swarm;
      if (s.pending) {
        if (m.swarm_id !== s.id) return;
        s.pending.list = [...s.pending.list.filter((n) => n.id !== m.node_id), { id: m.node_id, x: m.x, y: m.y }];
        if (s.pending.list.length === s.pending.expected) {
          s.list = s.pending.list;
          s.nodes = s.pending.expected;
          s.leader = s.pending.leader;
          s.pending = null;
          buildConsensus(v, s.list);
          text(v, 6, `Swarm ${s.id}: new formation applied`);
        }
        return;
      }
      if (s.list.some((n) => n.id === m.node_id)) return;
      s.list.push({ id: m.node_id, x: m.x, y: m.y });
      if (s.list.length === s.nodes && s.list.some((n) => n.id === v.id)) {
        buildConsensus(v, s.list);
        text(v, 6, `Swarm ${s.id}: all ${s.nodes} nodes received`);
      }
    });
  };

  const step = () => {
    vehicles.forEach((v) => {
      let cmd = [0, 0, 0];
      const [N, E, D] = v.pos;
      const [mMain, mSub] = v.mode;

      if (v.armed && !v.landed) {
        if (mMain === 4 && (mSub === 11 || mSub === 12)) {
          const s = v.swarm;
          if (s.consensus.length && s.list.length === s.nodes) {
            const leader = byId(s.leader);
            // FlightTaskSwarm uses SWARM_WEIGHT on every neighbour; the ROS 2 mode a fixed weight of 1
            const w = mSub === 11 ? v.params.SWARM_WEIGHT : 1;
            s.consensus.forEach((c) => {
              const o = byId(c.id);
              if (!o) return;
              cmd[0] -= w * (N - o.pos[0] - c.ox);
              cmd[1] -= w * (E - o.pos[1] - c.oy);
            });
            if (leader) {
              cmd[2] = leader.pos[2] - D;
              v.targetYaw = leader.yaw;
            }
          }
        } else if (mMain === 4 && mSub === 6) {
          cmd = [0, 0, 1];
        } else if (mMain === 4 && mSub === 5) {
          const home = [0, (v.id - 1) * 3];
          const dist = Math.hypot(home[0] - N, home[1] - E);
          if (dist > 0.5) {
            const rtlAlt = -Math.max(10, -D);
            cmd = [(home[0] - N) * 0.8, (home[1] - E) * 0.8, (rtlAlt - D) * 1.5];
          } else {
            cmd = [(home[0] - N), (home[1] - E), 1];
          }
        } else if (v.target) {
          cmd = [(v.target[0] - N) * 0.8, (v.target[1] - E) * 0.8, (v.target[2] - D) * 1.2];
          if (mMain === 4 && mSub === 2 && Math.abs(v.target[2] - D) < 0.2) {
            v.mode = MODE.HOLD;
            text(v, 6, 'Takeoff complete, holding');
          }
        }
        const h = Math.hypot(cmd[0], cmd[1]);
        if (h > V_MAX) {
          cmd[0] *= V_MAX / h;
          cmd[1] *= V_MAX / h;
        }
        cmd[2] = clamp(cmd[2], 3);
        if (h > 1.5 && !(mMain === 4 && (mSub === 11 || mSub === 12))) v.targetYaw = Math.atan2(cmd[1], cmd[0]);
      } else if (!v.landed) {
        cmd = [v.vel[0] * 0.98, v.vel[1] * 0.98, 9]; // unpowered descent
      }

      const k = Math.min(1, DT * 5);
      const acc = [0, 1, 2].map((i) => (cmd[i] - v.vel[i]) * k);
      for (let i = 0; i < 3; i++) v.vel[i] += acc[i];
      for (let i = 0; i < 3; i++) v.pos[i] += v.vel[i] * DT;

      if (v.targetYaw !== null) v.yaw = wrapPi(v.yaw + clamp(wrapPi(v.targetYaw - v.yaw), 1.5 * DT));
      // Tilt towards acceleration for a convincing attitude
      const c = Math.cos(v.yaw);
      const s = Math.sin(v.yaw);
      const aFwd = (acc[0] * c + acc[1] * s) / DT;
      const aRight = (-acc[0] * s + acc[1] * c) / DT;
      v.pitch += (clamp(-aFwd * 0.08, 0.4) - v.pitch) * 0.2;
      v.roll += (clamp(aRight * 0.08, 0.4) - v.roll) * 0.2;

      if (v.pos[2] >= 0) {
        v.pos[2] = 0;
        if (!v.landed && v.vel[2] >= 0) {
          v.landed = true;
          v.vel = [0, 0, 0];
          v.roll = 0;
          v.pitch = 0;
          if (v.armed) {
            v.landTimer = 1.5;
            text(v, 6, 'Landing detected');
          }
        }
      }
      if (v.landed && v.armed && v.landTimer > 0) {
        v.landTimer -= DT;
        if (v.landTimer <= 0) {
          v.armed = false;
          v.mode = MODE.HOLD;
          text(v, 6, 'Disarmed by landing');
        }
      }
      if (v.armed) v.battery = Math.max(0, v.battery - DT * (v.landed ? 0.005 : 0.03));
    });
  };

  const telemetry = () => {
    tick++;
    vehicles.forEach((v) => {
      emit('uav/local_position_ned', {
        sys_id: v.id, x: v.pos[0], y: v.pos[1], z: v.pos[2], vx: v.vel[0], vy: v.vel[1], vz: v.vel[2],
      });
      emit('uav/attitude', { sys_id: v.id, roll: v.roll, pitch: v.pitch, yaw: v.yaw });
      if (tick % 2 === 0) {
        emit('uav/global_position', {
          sys_id: v.id,
          lat: HOME.lat + (v.pos[0] / R_EARTH) * (180 / Math.PI),
          lon: HOME.lon + (v.pos[1] / (R_EARTH * Math.cos((HOME.lat * Math.PI) / 180))) * (180 / Math.PI),
          alt: HOME.alt - v.pos[2],
        });
      }
      if (tick % 10 === 0) {
        emit('uav/heartbeat', {
          sys_id: v.id,
          base_mode: (v.armed ? 128 : 0) | 1,
          custom_mode: ((v.mode[1] << 24) | (v.mode[0] << 16)) >>> 0,
          system_status: v.armed ? 4 : 3,
        });
        emit('uav/sys_status', {
          sys_id: v.id, voltage: 14.8 + (v.battery / 100) * 2, battery_remaining: Math.round(v.battery),
        });
        // stand-in for the ROS 2 swarm_mode node's heartbeat (custom_mode of EXTERNAL1)
        emit('uav/companion', { sys_id: v.id, custom_mode: ((MODE.SWARM_EXT[1] << 24) | (MODE.SWARM_EXT[0] << 16)) >>> 0, active: v.mode === MODE.SWARM_EXT });
        emit('uav/extended_sys_state', {
          sys_id: v.id, landed_state: v.landed ? 1 : v.mode === MODE.LAND ? 4 : v.mode === MODE.TAKEOFF ? 3 : 2,
        });
      }
    });
  };

  const physics = setInterval(step, DT * 1000);
  const stream = setInterval(telemetry, 100);
  telemetry();

  const publish = (topic, payload) => {
    // Network latency so command sequencing behaves like the real link
    setTimeout(() => {
      switch (topic) {
        case 'uav/command':
          (payload.uav_ids || []).forEach((id) => {
            const v = byId(id);
            if (v) handleCommand(v, payload.command, payload.params);
          });
          break;
        case 'uav/swarm_flight_mode':
          (payload.uav_ids || []).forEach((id) => {
            const v = byId(id);
            if (v) handleCommand(v, 'swarm');
          });
          break;
        case 'uav/swarm_management':
          handleSwarmManagement(payload);
          break;
        case 'uav/swarm_node':
          handleSwarmNode(payload);
          break;
        default:
      }
    }, 20);
  };

  return {
    publish,
    close: () => {
      clearInterval(physics);
      clearInterval(stream);
    },
  };
};
