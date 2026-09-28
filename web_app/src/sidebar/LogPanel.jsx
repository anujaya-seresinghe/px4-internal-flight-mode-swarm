import React, { useEffect, useRef, useState } from 'react';
import { useStore } from '../store';

const LEVELS = ['all', 'cmd', 'info', 'success', 'warn', 'error'];
const time = (t) => new Date(t).toLocaleTimeString([], { hour12: false });

export const LogPanel = () => {
  const logs = useStore((s) => s.logs);
  const clearLogs = useStore((s) => s.clearLogs);
  const [filter, setFilter] = useState('all');
  const [query, setQuery] = useState('');
  const endRef = useRef(null);
  const listRef = useRef(null);
  const stick = useRef(true);

  const shown = logs.filter(
    (l) =>
      (filter === 'all' || l.level === filter) &&
      (!query || `${l.source} ${l.text}`.toLowerCase().includes(query.toLowerCase()))
  );

  useEffect(() => {
    if (stick.current) endRef.current?.scrollIntoView({ block: 'end' });
  }, [shown.length]);

  return (
    <div className="log-panel">
      <div className="log-toolbar">
        <input className="input" placeholder="Filter messages…" value={query} onChange={(e) => setQuery(e.target.value)} />
        <button className="btn btn-sm" onClick={clearLogs}>Clear</button>
      </div>
      <div className="segmented small">
        {LEVELS.map((l) => (
          <button key={l} className={filter === l ? 'active' : ''} onClick={() => setFilter(l)}>
            {l}
          </button>
        ))}
      </div>
      <div
        ref={listRef}
        className="log-list"
        onScroll={() => {
          const el = listRef.current;
          stick.current = el.scrollHeight - el.scrollTop - el.clientHeight < 24;
        }}
      >
        {shown.length === 0 && <p className="hint">No messages.</p>}
        {shown.map((l) => (
          <div key={l.id} className={`log-line log-${l.level}`}>
            <span className="log-time mono">{time(l.t)}</span>
            <span className="log-src mono">{l.source}</span>
            <span className="log-text">{l.text}</span>
          </div>
        ))}
        <div ref={endRef} />
      </div>
    </div>
  );
};
