// PX4 custom_mode decoding (see PX4/src/modules/commander/px4_custom_mode.h)

const MAIN_MODES = {
  1: 'Manual',
  2: 'Altitude',
  3: 'Position',
  4: 'Auto',
  5: 'Acro',
  6: 'Offboard',
  7: 'Stabilized',
  10: 'Termination',
  11: 'Alt Cruise',
};

const AUTO_SUB_MODES = {
  1: 'Ready',
  2: 'Takeoff',
  3: 'Hold',
  4: 'Mission',
  5: 'Return',
  6: 'Land',
  8: 'Follow',
  9: 'Precision Land',
  10: 'VTOL Takeoff',
  11: 'Swarm',
};

export const decodeCustomMode = (customMode) => {
  const main = (customMode >>> 16) & 0xff;
  const sub = (customMode >>> 24) & 0xff;
  return { main, sub };
};

export const modeName = (main, sub) => {
  if (main === 4) return AUTO_SUB_MODES[sub] || `Auto ${sub}`;
  return MAIN_MODES[main] || (main ? `Mode ${main}` : '—');
};

export const isSwarmMode = (v) => v?.mainMode === 4 && v?.subMode === 11;

// MAV_STATE
export const SYSTEM_STATUS = {
  0: 'Uninit',
  1: 'Boot',
  2: 'Calibrating',
  3: 'Standby',
  4: 'Active',
  5: 'Critical',
  6: 'Emergency',
  7: 'Poweroff',
  8: 'Terminating',
};

// MAV_LANDED_STATE
export const LANDED_STATE = {
  0: 'Unknown',
  1: 'On ground',
  2: 'In air',
  3: 'Taking off',
  4: 'Landing',
};

// MAV_RESULT
export const COMMAND_RESULT = {
  0: 'Accepted',
  1: 'Temporarily rejected',
  2: 'Denied',
  3: 'Unsupported',
  4: 'Failed',
  5: 'In progress',
  6: 'Cancelled',
};

// MAV_SEVERITY -> log level
export const severityLevel = (sev) => (sev <= 3 ? 'error' : sev <= 4 ? 'warn' : 'info');

// Commands understood by the bridge on topic uav/command (and by the simulator)
export const VEHICLE_COMMANDS = {
  arm: 'Arm',
  disarm: 'Disarm',
  takeoff: 'Takeoff',
  land: 'Land',
  hold: 'Hold',
  rtl: 'Return',
  position: 'Position',
  swarm: 'Swarm mode',
  goto: 'Go to',
  kill: 'Kill',
};
