import React, { useState } from 'react';
import { useStore } from '../store';
import { setSwarmWeight } from '../lib/commands';

// Range of the PX4 parameter (flight_task_swarm_params.yaml)
const MIN = 0.01;
const MAX = 10;

const fmt = (v) => Number(v).toFixed(2);

/**
 * Consensus weight of PX4's internal Swarm mode (parameter SWARM_WEIGHT).
 * The swarm graph is undirected: an edge i-j must have the same weight seen from i and from j,
 * so the weight is one value for the whole fleet. It is always set on every vehicle at once,
 * and the list shows what each vehicle reports so a mismatch is visible.
 */
export const SwarmWeight = () => {
  const vehicles = useStore((s) => s.vehicles);
  const params = useStore((s) => s.params);
  const [draft, setDraft] = useState('');

  const ids = Object.keys(vehicles).map(Number).sort((a, b) => a - b);
  const reported = ids.map((id) => params[id]?.SWARM_WEIGHT).filter((v) => v !== undefined);
  // Most common reported value = the fleet's weight; anything else is a mismatch
  const counts = new Map();
  reported.forEach((v) => counts.set(v, (counts.get(v) || 0) + 1));
  const fleetValue = [...counts.entries()].sort((a, b) => b[1] - a[1])[0]?.[0];
  const uniform = counts.size <= 1 && reported.length === ids.length;

  const value = Number(draft === '' ? fleetValue ?? 1 : draft);
  const valid = Number.isFinite(value) && value >= MIN && value <= MAX;

  const apply = () => {
    if (!valid || !ids.length) return;
    if (setSwarmWeight(ids, value)) setDraft('');
  };

  return (
    <div className="swarm-weight">
      <div className="swarm-weight-row">
        <span className="field-label" title="PX4 parameter SWARM_WEIGHT: consensus gain on every edge of the swarm graph">
          All UAVs <span className="mono">{fleetValue !== undefined ? fmt(fleetValue) : '—'}</span>
        </span>
        <input
          className="input mono swarm-weight-input"
          type="number"
          min={MIN}
          max={MAX}
          step="0.05"
          placeholder={fleetValue !== undefined ? fmt(fleetValue) : '1.00'}
          value={draft}
          onChange={(e) => setDraft(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && apply()}
          aria-label="New SWARM_WEIGHT for all UAVs"
        />
        <button className="btn btn-sm" disabled={!valid || !ids.length} onClick={apply}>
          {uniform ? 'Set all' : 'Sync all'}
        </button>
      </div>

      {ids.length === 0 ? (
        <p className="hint">No vehicles connected.</p>
      ) : (
        <div className="swarm-weight-list">
          {ids.map((id) => {
            const v = params[id]?.SWARM_WEIGHT;
            const off = v === undefined || v !== fleetValue;
            return (
              <span key={id} className={`swarm-weight-item mono ${off ? 'off' : ''}`}>
                UAV {id} <b>{v !== undefined ? fmt(v) : '—'}</b>
              </span>
            );
          })}
        </div>
      )}

      {reported.length === 0 && ids.length > 0 && (
        <div className="hint">No SWARM_WEIGHT reported yet (PX4 built without it? Rebuild with --build).</div>
      )}
      {!uniform && reported.length > 0 && (
        <div className="warn">Weights differ between UAVs, so the graph is not symmetric. Set them all to one value.</div>
      )}
      <div className="hint">
        Same weight on every edge of the undirected swarm graph, so it is always set on all UAVs together. Applies
        immediately, also in flight. Not used by the ROS 2 external mode.
      </div>
    </div>
  );
};
