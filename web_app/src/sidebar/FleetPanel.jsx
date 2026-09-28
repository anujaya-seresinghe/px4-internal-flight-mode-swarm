import React, { useState } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { useStore, isLinkLost, liveCompanion } from '../store';
import { modeName, LANDED_STATE, VTOL_STATE, isVtol } from '../lib/px4';
import { vehicleRoles, swarmColor } from '../lib/theme';
import { sendCommand } from '../lib/commands';
import { AttitudeIndicator, Compass, BatteryBar } from './Instruments';
import { useNow } from './useNow';

const speed = (v) => Math.hypot(v.vx, v.vy);

const VehicleRow = ({ v, selected, role, now, companion }) => {
  const select = useStore((s) => s.select);
  const lost = isLinkLost(v, now);
  return (
    <button
      className={`vehicle-row ${selected ? 'selected' : ''} ${lost ? 'lost' : ''}`}
      onClick={(e) => select(v.id, e.ctrlKey || e.metaKey || e.shiftKey ? 'toggle' : 'click')}
      onDoubleClick={() => useStore.getState().setFollow(v.id)}
    >
      <span className="vehicle-swatch" style={{ background: role?.isLeader ? 'var(--leader)' : role?.color || 'var(--vehicle)' }} />
      <span className="vehicle-id">
        UAV {v.id}
        {role && <span className="vehicle-role">{role.isLeader ? `Leader · S${role.swarm.id}` : `S${role.swarm.id}`}</span>}
      </span>
      <span className="vehicle-mode">
        <span className={`chip ${v.armed ? 'chip-armed' : ''}`}>{v.armed ? 'ARMED' : 'DISARMED'}</span>
        <span className="chip chip-mode">
          {isVtol(v) && <span className="vtol-state">{VTOL_STATE[v.vtolState]} · </span>}
          {modeName(v.mainMode, v.subMode, companion)}
        </span>
      </span>
      <span className="vehicle-metrics mono">
        <span>{lost ? <span className="danger">NO LINK</span> : `${(-v.z).toFixed(1)} m`}</span>
        <span className="dim">{speed(v).toFixed(1)} m/s</span>
      </span>
    </button>
  );
};

const SwarmRow = ({ swarm, selected, vehicles }) => {
  const selectSwarm = useStore((s) => s.selectSwarm);
  const leader = vehicles[swarm.leaderId];
  return (
    <button
      className={`vehicle-row swarm-row ${selected ? 'selected' : ''}`}
      style={{ '--swarm': swarmColor(swarm.id) }}
      onClick={() => selectSwarm(swarm.id)}
    >
      <span className="vehicle-swatch" style={{ background: 'var(--swarm)' }} />
      <span className="vehicle-id">
        Swarm {swarm.id}
        <span className="vehicle-role">
          Leader UAV {swarm.leaderId} · {swarm.members.length} drones
        </span>
      </span>
      <span className="vehicle-mode">
        <span className={`chip ${swarm.status === 'active' ? 'chip-ok' : 'chip-info'}`}>{swarm.status}</span>
      </span>
      <span className="vehicle-metrics mono">
        <span>{leader ? `${(-leader.z).toFixed(1)} m` : '—'}</span>
        <span className="dim">{leader ? `${speed(leader).toFixed(1)} m/s` : ''}</span>
      </span>
    </button>
  );
};

const SwarmCommands = ({ swarm }) => {
  const setPendingGoto = useStore((s) => s.setPendingGoto);
  const requestFocus = useStore((s) => s.requestFocus);
  const lead = [swarm.leaderId];
  return (
    <div className="command-panel">
      <p className="hint">
        Right-click the map and choose <b>Fly here</b> to move leader UAV {swarm.leaderId}; the followers keep formation.
      </p>
      <div className="btn-grid">
        <button className="btn btn-primary" onClick={() => setPendingGoto({ ids: lead })}>Go to…</button>
        <button className="btn" onClick={() => sendCommand(lead, 'hold')}>Hold leader</button>
        <button className="btn" onClick={() => requestFocus(swarm.members)}>Center view</button>
      </div>
    </div>
  );
};

const Stat = ({ label, children }) => (
  <div className="stat">
    <div className="stat-label">{label}</div>
    <div className="stat-value mono">{children}</div>
  </div>
);

const VehicleDetail = ({ v, now }) => (
  <div className="detail">
    <div className="detail-instruments">
      <AttitudeIndicator roll={v.roll} pitch={v.pitch} />
      <Compass yaw={v.yaw} />
    </div>
    <div className="stat-grid">
      <Stat label="North">{v.x.toFixed(2)} m</Stat>
      <Stat label="East">{v.y.toFixed(2)} m</Stat>
      <Stat label="Altitude">{(-v.z).toFixed(2)} m</Stat>
      <Stat label="Ground speed">{speed(v).toFixed(2)} m/s</Stat>
      <Stat label="Climb">{(-v.vz).toFixed(2)} m/s</Stat>
      <Stat label="State">{LANDED_STATE[v.landedState] ?? '—'}</Stat>
      <Stat label="Battery"><BatteryBar value={v.battery} /></Stat>
      <Stat label="Voltage">{v.voltage ? `${v.voltage.toFixed(1)} V` : '—'}</Stat>
      <Stat label="Last msg">{((now - v.lastSeen) / 1000).toFixed(1)} s</Stat>
      {v.lat !== null && (
        <>
          <Stat label="Latitude">{v.lat.toFixed(6)}</Stat>
          <Stat label="Longitude">{v.lon.toFixed(6)}</Stat>
          <Stat label="AMSL">{v.alt.toFixed(1)} m</Stat>
        </>
      )}
    </div>
  </div>
);

// Takeoff + transition to fixed-wing for the selected VTOLs, each on its own altitude layer:
// in Hold a fixed-wing loiters around its own point, so VTOLs at one altitude would collide.
const VtolTakeoff = ({ ids }) => {
  const vtolIds = useStore(useShallow((s) => ids.filter((id) => isVtol(s.vehicles[id]))));
  const [altitude, setAltitude] = useState(40);
  const [step, setStep] = useState(15);
  const [distance, setDistance] = useState(300);
  const layers = vtolIds.map((id, i) => `UAV ${id} → ${Number(altitude) + i * Number(step)} m`).join(', ');

  const takeoff = () => {
    vtolIds.forEach((id, i) =>
      sendCommand([id], 'vtol_takeoff', { altitude: Number(altitude) + i * Number(step), loiter_distance: Number(distance) }));
  };

  return (
    <div className="vtol-takeoff">
      <div className="field-label">VTOL takeoff</div>
      <div className="field-row">
        <label className="field">
          <span className="hint">Altitude (m)</span>
          <input className="input mono" type="number" min="20" step="5" value={altitude} onChange={(e) => setAltitude(e.target.value)} />
        </label>
        <label className="field">
          <span className="hint">Step per VTOL (m)</span>
          <input className="input mono" type="number" min="0" step="5" value={step} onChange={(e) => setStep(e.target.value)} />
        </label>
        <label className="field">
          <span className="hint">Loiter ahead (m)</span>
          <input className="input mono" type="number" min="100" step="50" value={distance} onChange={(e) => setDistance(e.target.value)} />
        </label>
      </div>
      <button className="btn btn-primary btn-block" onClick={takeoff} disabled={!vtolIds.length}>
        Takeoff &amp; transition to FW
      </button>
      <p className="hint">
        Climbs vertically, transitions along each VTOL's heading and loiters ahead. {layers}.
        {vtolIds.length > 1 && Number(step) < 10 && <span className="danger"> Use at least 10 m between layers.</span>}
      </p>
    </div>
  );
};

const CommandPanel = ({ ids }) => {
  const vtol = useStore((s) => ids.some((id) => isVtol(s.vehicles[id])));
  // VTOLs need the ROS 2 external mode: PX4's internal Swarm mode does not run in fixed-wing
  const rosSwarm = useStore((s) => vtol && ids.every((id) => liveCompanion(s.companions, id)));
  const [alt, setAlt] = useState(10);
  const [killArm, setKillArm] = useState(false);
  const setPendingGoto = useStore((s) => s.setPendingGoto);
  const requestFocus = useStore((s) => s.requestFocus);
  const cmd = (c, p) => () => sendCommand(ids, c, p);

  return (
    <div className="command-panel">
      <div className="btn-grid">
        <button className="btn" onClick={cmd('arm')}>Arm</button>
        <button className="btn" onClick={cmd('disarm')}>Disarm</button>
        <button className="btn btn-primary" onClick={cmd('takeoff')}>Takeoff</button>
        <button className="btn" onClick={cmd('hold')}>Hold</button>
        <button className="btn" onClick={cmd('land')}>Land</button>
        <button className="btn" onClick={cmd('rtl')}>Return</button>
        <button className="btn" onClick={cmd('position')}>Position</button>
        <button
          className="btn btn-swarm"
          onClick={cmd('swarm', rosSwarm ? { external: true } : undefined)}
          title={rosSwarm ? 'ROS 2 external Swarm mode (flies fixed-wing too)' : 'PX4 internal Swarm mode (multicopter only)'}
        >
          {rosSwarm ? 'Swarm (ROS 2)' : 'Swarm mode'}
        </button>
        <button className="btn" onClick={() => requestFocus(ids)}>Center view</button>
        {vtol && (
          <>
            <button className="btn" onClick={cmd('transition_fw')} title="MAV_CMD_DO_VTOL_TRANSITION to fixed-wing">To FW</button>
            <button className="btn" onClick={cmd('transition_mc')} title="MAV_CMD_DO_VTOL_TRANSITION to multicopter">To MC</button>
          </>
        )}
      </div>
      {vtol && <VtolTakeoff ids={ids} />}
      <div className="inline-form">
        <button className="btn btn-grow" onClick={() => setPendingGoto({ ids })}>Go to… (pick on map)</button>
      </div>
      <div className="inline-form">
        <label className="field-label" htmlFor="alt-input">Altitude</label>
        <input id="alt-input" className="input input-narrow mono" type="number" min="1" step="1" value={alt} onChange={(e) => setAlt(e.target.value)} />
        <span className="dim">m</span>
        <button className="btn btn-grow" onClick={cmd('goto', { up: Number(alt) })}>Change altitude</button>
      </div>
      <button
        className={`btn btn-danger btn-block ${killArm ? 'armed' : ''}`}
        onClick={() => {
          if (!killArm) {
            setKillArm(true);
            setTimeout(() => setKillArm(false), 3000);
            return;
          }
          setKillArm(false);
          sendCommand(ids, 'kill');
        }}
      >
        {killArm ? 'Click again to KILL motors' : 'Emergency stop'}
      </button>
    </div>
  );
};

export const FleetPanel = () => {
  const vehicles = useStore((s) => s.vehicles);
  const selected = useStore((s) => s.selected);
  const swarms = useStore((s) => s.swarms);
  const select = useStore((s) => s.select);
  const selectedSwarm = useStore((s) => s.selectedSwarm);
  const companions = useStore((s) => s.companions);
  const now = useNow(500);
  const swarmList = Object.values(swarms).sort((a, b) => a.id - b.id);
  const list = Object.values(vehicles).sort((a, b) => a.id - b.id);
  const roles = vehicleRoles(swarms);
  const single = selected.length === 1 ? vehicles[selected[0]] : null;

  if (!list.length) {
    return (
      <div className="empty">
        <div className="empty-title">No vehicles yet</div>
        <p>Waiting for telemetry on <span className="mono">uav/local_position_ned</span>. Check that the MAVLink bridge is running, or switch to the built-in simulator from the connection menu.</p>
      </div>
    );
  }

  return (
    <div className="panel-stack">
      <section className="section">
        <div className="section-head">
          <h3>Vehicles <span className="count">{list.length}</span></h3>
          <div className="section-actions">
            <button className="link-btn" onClick={() => select(list.map((v) => v.id))}>Select all</button>
            {selected.length > 0 && <button className="link-btn" onClick={() => select([])}>Clear</button>}
          </div>
        </div>
        <div className="vehicle-list">
          {list.map((v) => (
            <VehicleRow key={v.id} v={v} selected={selected.includes(v.id)} role={roles[v.id]} now={now} companion={liveCompanion(companions, v.id, now)} />
          ))}
        </div>
        <p className="hint">Click to select, click again to deselect · Ctrl/Shift-click to multi-select · Shift-drag on the 2D map to box-select · double-click to follow</p>
      </section>

      <section className="section">
        <div className="section-head">
          <h3>Swarms <span className="count">{swarmList.length}</span></h3>
        </div>
        {swarmList.length === 0 ? (
          <p className="hint">No swarms yet. Create one in the Swarm tab.</p>
        ) : (
          <div className="vehicle-list">
            {swarmList.map((sw) => (
              <SwarmRow key={sw.id} swarm={sw} selected={selectedSwarm === sw.id} vehicles={vehicles} />
            ))}
          </div>
        )}
        {swarms[selectedSwarm] && <SwarmCommands swarm={swarms[selectedSwarm]} />}
      </section>

      {selected.length > 0 && (
        <section className="section">
          <div className="section-head">
            <h3>{single ? `UAV ${single.id}` : `${selected.length} vehicles selected`}</h3>
            {single && <span className="chip chip-mode">{modeName(single.mainMode, single.subMode, liveCompanion(companions, single.id, now))}</span>}
          </div>
          {single && <VehicleDetail v={single} now={now} />}
          <CommandPanel ids={selected} />
        </section>
      )}
    </div>
  );
};
