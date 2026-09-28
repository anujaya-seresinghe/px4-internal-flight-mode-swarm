import React, { useEffect, useMemo, useState } from 'react';
import { useStore, isLinkLost } from '../store';
import { FORMATIONS, buildFormation, tightPairs, APF_RADIUS } from '../lib/formations';
import { swarmColor } from '../lib/theme';
import { isSwarmMode } from '../lib/px4';
import { deploySwarm, dissolveSwarm, nextSwarmId, sendCommand, updateFormation } from '../lib/commands';
import { FormationEditor } from './FormationEditor';

const STEPS = ['Swarm mode', 'Management', 'Node offsets', 'Done'];
const UPDATE_STEPS = ['Update', 'Node offsets', 'Done'];

const SwarmCard = ({ swarm, active, onOpen, onReform }) => {
  const vehicles = useStore((s) => s.vehicles);
  const select = useStore((s) => s.select);
  const requestFocus = useStore((s) => s.requestFocus);
  const setPendingGoto = useStore((s) => s.setPendingGoto);
  const [confirm, setConfirm] = useState(false);
  const color = swarmColor(swarm.id);
  const inMode = swarm.members.filter((id) => id !== swarm.leaderId && isSwarmMode(vehicles[id])).length;
  const followers = swarm.members.length - 1;

  return (
    <div
      className={`swarm-card ${active ? 'active' : ''}`}
      style={{ '--swarm': color }}
      role="button"
      tabIndex={0}
      aria-pressed={active}
      onClick={() => onOpen(swarm)}
      onKeyDown={(e) => (e.key === 'Enter' || e.key === ' ') && (e.preventDefault(), onOpen(swarm))}
    >
      <div className="swarm-card-head">
        <span className="swarm-dot" />
        <span className="swarm-name">Swarm {swarm.id}</span>
        <span className={`chip ${swarm.status === 'active' ? 'chip-ok' : swarm.status === 'failed' ? 'chip-bad' : 'chip-info'}`}>
          {swarm.status}
        </span>
      </div>
      <div className="swarm-members">
        {swarm.members.map((id) => (
          <span key={id} className={`member ${id === swarm.leaderId ? 'leader' : ''} ${vehicles[id] && isLinkLost(vehicles[id]) ? 'lost' : ''}`}>
            {id === swarm.leaderId ? '★ ' : ''}UAV {id}
          </span>
        ))}
      </div>
      <div className="swarm-meta dim">
        {inMode}/{followers} followers in swarm mode
      </div>
      {/* Buttons act on their own; they must not also open/close the card */}
      <div className="btn-row" onClick={(e) => e.stopPropagation()} onKeyDown={(e) => e.stopPropagation()}>
        <button className="btn btn-sm" onClick={() => { select(swarm.members); requestFocus(swarm.members); }}>Select</button>
        <button className="btn btn-sm" onClick={() => setPendingGoto({ ids: [swarm.leaderId] })}>Move leader…</button>
        <button className="btn btn-sm" onClick={() => onReform(swarm)}>Change formation</button>
        <button
          className={`btn btn-sm btn-danger-ghost ${confirm ? 'armed' : ''}`}
          onClick={() => {
            if (!confirm) {
              setConfirm(true);
              setTimeout(() => setConfirm(false), 3000);
            } else dissolveSwarm(swarm);
          }}
        >
          {confirm ? 'Confirm' : 'Dissolve'}
        </button>
      </div>
    </div>
  );
};

export const SwarmPanel = () => {
  const vehicles = useStore((s) => s.vehicles);
  const selected = useStore((s) => s.selected);
  const swarms = useStore((s) => s.swarms);

  const [members, setMembers] = useState([]);
  const [leaderId, setLeaderId] = useState(null);
  const [swarmId, setSwarmId] = useState(nextSwarmId());
  const [formation, setFormation] = useState('wedge');
  const [spacing, setSpacing] = useState(8);
  const [heading, setHeading] = useState(0);
  const [custom, setCustom] = useState(null);
  const [step, setStep] = useState(null);
  const [editing, setEditing] = useState(null);

  const list = Object.values(vehicles).sort((a, b) => a.id - b.id);
  const swarmList = Object.values(swarms).sort((a, b) => a.id - b.id);

  // Seed the builder from the current selection
  useEffect(() => {
    if (step !== null || editing) return;
    if (selected.length) {
      setMembers(selected);
      if (!selected.includes(leaderId)) setLeaderId(selected[0]);
      setCustom(null);
    }
  }, [selected]); // eslint-disable-line react-hooks/exhaustive-deps

  useEffect(() => {
    if (editing && !swarms[editing]) setEditing(null); // swarm dissolved while editing
    else if (!editing && swarms[swarmId] && step === null) setSwarmId(nextSwarmId());
  }, [swarms]); // eslint-disable-line react-hooks/exhaustive-deps

  const followers = members.filter((id) => id !== leaderId);
  const offsets = useMemo(() => {
    if (leaderId === null || !members.includes(leaderId)) return {};
    if (formation === 'custom' && custom) {
      const o = { [leaderId]: { x: 0, y: 0 } };
      followers.forEach((id, i) => (o[id] = custom[id] || { x: -(i + 1) * spacing, y: 0 }));
      return o;
    }
    return buildFormation(formation, leaderId, followers, Number(spacing) || 1, Number(heading) || 0);
  }, [formation, leaderId, members, spacing, heading, custom]); // eslint-disable-line react-hooks/exhaustive-deps

  const toggleMember = (id) => {
    // Changing membership means a different swarm: leave "change formation" for "new swarm"
    if (editing) {
      setEditing(null);
      setSwarmId(nextSwarmId());
    }
    setMembers((m) => {
      const next = m.includes(id) ? m.filter((x) => x !== id) : [...m, id].sort((a, b) => a - b);
      if (!next.includes(leaderId)) setLeaderId(next[0] ?? null);
      return next;
    });
  };

  const moveNode = (id, pos) => {
    setCustom({ ...offsets, [id]: pos });
    setFormation('custom');
  };

  // Edit an existing swarm in place, keeping the shape currently picked in the form
  const editExisting = (swarm) => {
    if (formation === 'custom') setCustom(offsets);
    setMembers(swarm.members);
    setLeaderId(swarm.leaderId);
    setSwarmId(swarm.id);
    setEditing(swarm.id);
  };

  const reform = (swarm) => {
    setMembers(swarm.members);
    setLeaderId(swarm.leaderId);
    setCustom(swarm.offsets);
    setFormation('custom');
    setSwarmId(swarm.id);
    setEditing(swarm.id);
  };

  // Clicking a swarm card opens it for a formation change; clicking it again returns to "new swarm"
  const openSwarm = (swarm) => {
    if (editing === swarm.id) cancelEdit();
    else reform(swarm);
  };

  const notFlying = members.filter((id) => {
    const v = vehicles[id];
    return !v || !v.armed || -v.z < 0.5;
  });
  const lost = members.filter((id) => vehicles[id] && isLinkLost(vehicles[id]));
  const tight = tightPairs(offsets);
  const idTaken = swarms[swarmId] && swarmId !== editing;
  const canDeploy = members.length >= 2 && leaderId !== null && step === null && !idTaken && swarmId >= 1 && swarmId <= 255;

  const cancelEdit = () => {
    setEditing(null);
    setSwarmId(nextSwarmId());
  };

  const applyFormation = async () => {
    await updateFormation({ swarm: swarms[editing], offsets, onStep: setStep });
    setTimeout(() => {
      setStep(null);
      cancelEdit();
    }, 1000);
  };

  const deploy = async () => {
    const { removeSwarm, upsertSwarm } = useStore.getState();
    // A vehicle can only be in one swarm: drop it from any previous one
    Object.values(useStore.getState().swarms).forEach((s) => {
      if (s.id === swarmId) return;
      const remaining = s.members.filter((id) => !members.includes(id));
      if (remaining.length === s.members.length) return;
      if (remaining.length < 2 || !remaining.includes(s.leaderId)) removeSwarm(s.id);
      else upsertSwarm({ id: s.id, members: remaining });
    });
    await deploySwarm({ swarmId, leaderId, members, offsets, onStep: setStep });
    setTimeout(() => {
      setStep(null);
      setEditing(null);
      setSwarmId(nextSwarmId());
    }, 1200);
  };

  return (
    <div className="panel-stack">
      <section className="section">
        <div className="section-head">
          <h3>Active swarms <span className="count">{swarmList.length}</span></h3>
        </div>
        {swarmList.length === 0 ? (
          <p className="hint">No swarms deployed from this station yet.</p>
        ) : (
          swarmList.map((s) => <SwarmCard key={s.id} swarm={s} active={editing === s.id} onOpen={openSwarm} onReform={reform} />)
        )}
      </section>

      <section className="section">
        <div className="section-head">
          <h3>{editing ? `Change formation of swarm ${editing}` : 'New swarm'}</h3>
          {editing && <button className="link-btn" onClick={cancelEdit}>Cancel</button>}
        </div>

        <div className="field">
          <div className="field-label">Members</div>
          {list.length === 0 ? (
            <p className="hint">No vehicles connected.</p>
          ) : (
            <div className="member-picker">
              {list.map((v) => (
                <button
                  key={v.id}
                  className={`member-toggle ${members.includes(v.id) ? 'on' : ''}`}
                  onClick={() => toggleMember(v.id)}
                >
                  UAV {v.id}
                </button>
              ))}
            </div>
          )}
          {editing && <p className="hint">Click a UAV to change the members; that starts a new swarm instead.</p>}
        </div>

        {members.length > 0 && (
          <>
            <div className="field-row">
              <div className="field">
                <label className="field-label" htmlFor="leader-select">Leader</label>
                <select id="leader-select" className="input" disabled={!!editing} value={leaderId ?? ''} onChange={(e) => setLeaderId(Number(e.target.value))}>
                  {members.map((id) => (
                    <option key={id} value={id}>UAV {id}</option>
                  ))}
                </select>
              </div>
              <div className="field">
                <label className="field-label" htmlFor="swarm-id">Swarm ID</label>
                <input id="swarm-id" className="input mono" disabled={!!editing} type="number" min="1" max="255" value={swarmId} onChange={(e) => setSwarmId(Number(e.target.value))} />
              </div>
            </div>

            <div className="field">
              <div className="field-label">Formation</div>
              <div className="segmented wrap">
                {Object.entries(FORMATIONS).map(([k, f]) => (
                  <button
                    key={k}
                    className={formation === k ? 'active' : ''}
                    onClick={() => {
                      if (k === 'custom') setCustom(offsets);
                      setFormation(k);
                    }}
                  >
                    {f.label}
                  </button>
                ))}
              </div>
            </div>

            {formation !== 'custom' && (
              <div className="field-row">
                <div className="field">
                  <label className="field-label" htmlFor="spacing">Spacing <span className="mono">{spacing} m</span></label>
                  <input id="spacing" type="range" min="2" max="30" step="0.5" value={spacing} onChange={(e) => setSpacing(Number(e.target.value))} />
                </div>
                <div className="field">
                  <label className="field-label" htmlFor="heading">Heading <span className="mono">{heading}°</span></label>
                  <input id="heading" type="range" min="0" max="359" step="1" value={heading} onChange={(e) => setHeading(Number(e.target.value))} />
                </div>
              </div>
            )}

            {leaderId !== null && (
              <FormationEditor offsets={offsets} leaderId={leaderId} color={swarmColor(swarmId || 1)} onMove={moveNode} />
            )}

            <div className="warnings">
              {members.length < 2 && <div className="warn">A swarm needs at least two vehicles.</div>}
              {idTaken && (
                <div className="warn">
                  Swarm {swarmId} already exists (leader UAV {swarms[swarmId].leaderId}). To change its shape, keep the ID and
                  update it in place:
                  <button className="btn btn-primary btn-block" style={{ marginTop: 8 }} onClick={() => editExisting(swarms[swarmId])}>
                    Change formation of swarm {swarmId}
                  </button>
                </div>
              )}
              {notFlying.length > 0 && (
                <div className="warn">
                  UAV {notFlying.join(', ')} {notFlying.length > 1 ? 'are' : 'is'} not airborne. Swarm mode only engages in flight.
                  <button className="link-btn" onClick={() => sendCommand(notFlying, 'takeoff')}>Take off now</button>
                </div>
              )}
              {lost.length > 0 && <div className="warn bad">No telemetry from UAV {lost.join(', ')}.</div>}
              {tight.length > 0 && (
                <div className="warn">
                  {tight.length} pair{tight.length > 1 ? 's are' : ' is'} closer than {APF_RADIUS} m, so APF collision avoidance will push them apart vertically.
                </div>
              )}
            </div>

            {step !== null ? (
              <ol className="deploy-steps">
                {(editing ? UPDATE_STEPS : STEPS).map((s, i) => (
                  <li key={s} className={i < step ? 'done' : i === step ? 'current' : ''}>{s}</li>
                ))}
              </ol>
            ) : (
              editing ? (
                <button className="btn btn-primary btn-block" disabled={!canDeploy} onClick={applyFormation}>
                  Update formation of swarm {editing}
                </button>
              ) : (
                <button className="btn btn-primary btn-block" disabled={!canDeploy} onClick={deploy}>
                  Deploy swarm {swarmId}
                </button>
              )
            )}
            {editing ? (
              <p className="hint">
                Sends SWARM_MANAGEMENT (update formation) and the new SWARM_NODE offsets to the same swarm. Followers keep the
                current formation until all offsets arrived, then switch together. No mode changes; members and leader stay the same.
              </p>
            ) : (
              <p className="hint">
                Deploying switches every member to swarm mode, then sends SWARM_MANAGEMENT and one SWARM_NODE per member. The leader
                drops into Hold. Move it and the followers keep formation.
              </p>
            )}
          </>
        )}
      </section>
    </div>
  );
};
