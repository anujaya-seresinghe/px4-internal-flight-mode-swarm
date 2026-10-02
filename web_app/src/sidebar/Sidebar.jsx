import React, { useState } from 'react';
import { useStore } from '../store';
import { FleetPanel } from './FleetPanel';
import { SwarmPanel } from './SwarmPanel';
import { ParamsPanel } from './ParamsPanel';
import { LogPanel } from './LogPanel';

const TABS = [
  { id: 'fleet', label: 'Fleet' },
  { id: 'swarm', label: 'Swarm' },
  { id: 'params', label: 'Params' },
  { id: 'log', label: 'Log' },
];

export const Sidebar = () => {
  const [tab, setTab] = useState('fleet');
  const vehicleCount = useStore((s) => Object.keys(s.vehicles).length);
  const swarmCount = useStore((s) => Object.keys(s.swarms).length);
  const errorCount = useStore((s) => s.logs.filter((l) => l.level === 'error').length);
  const weightMismatch = useStore((s) => {
    const vals = Object.keys(s.vehicles).map((id) => s.params[id]?.SWARM_WEIGHT).filter((v) => v !== undefined);
    return new Set(vals).size > 1;
  });
  const badges = { fleet: vehicleCount, swarm: swarmCount, params: weightMismatch ? '!' : null, log: errorCount || null };

  return (
    <aside className="sidebar">
      <nav className="tabs" role="tablist">
        {TABS.map((t) => (
          <button
            key={t.id}
            role="tab"
            aria-selected={tab === t.id}
            className={`tab ${tab === t.id ? 'active' : ''}`}
            onClick={() => setTab(t.id)}
          >
            {t.label}
            {badges[t.id] ? <span className={`tab-badge ${t.id === 'log' || t.id === 'params' ? 'bad' : ''}`}>{badges[t.id]}</span> : null}
          </button>
        ))}
      </nav>
      <div className="sidebar-body">
        {tab === 'fleet' && <FleetPanel />}
        {tab === 'swarm' && <SwarmPanel />}
        {tab === 'params' && <ParamsPanel />}
        {tab === 'log' && <LogPanel />}
      </div>
    </aside>
  );
};
