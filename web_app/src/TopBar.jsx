import React, { useEffect, useRef, useState } from 'react';
import { useStore } from './store';
import { connect, disconnect } from './link';
import { sendCommand } from './lib/commands';
import { useNow } from './sidebar/useNow';

const STATUS_LABEL = {
  connected: 'Connected',
  connecting: 'Connecting…',
  disconnected: 'Disconnected',
  error: 'Error',
};

const useOutside = (ref, onClose) => {
  useEffect(() => {
    const h = (e) => ref.current && !ref.current.contains(e.target) && onClose();
    document.addEventListener('pointerdown', h);
    return () => document.removeEventListener('pointerdown', h);
  }, [ref, onClose]);
};

const ConnectionMenu = ({ onClose }) => {
  const link = useStore((s) => s.link);
  const setLink = useStore((s) => s.setLink);
  const [url, setUrl] = useState(link.url);
  const ref = useRef(null);
  useOutside(ref, onClose);

  const apply = (source) => {
    setLink({ source, url });
    connect();
    onClose();
  };

  return (
    <div className="popover" ref={ref}>
      <div className="popover-title">Data link</div>
      <div className="segmented">
        <button className={link.source === 'mqtt' ? 'active' : ''} onClick={() => setLink({ source: 'mqtt' })}>MQTT bridge</button>
        <button className={link.source === 'sim' ? 'active' : ''} onClick={() => setLink({ source: 'sim' })}>Simulator</button>
      </div>
      {link.source === 'mqtt' ? (
        <>
          <label className="field-label" htmlFor="broker-url">Broker WebSocket URL</label>
          <input id="broker-url" className="input mono" value={url} onChange={(e) => setUrl(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && apply('mqtt')} />
          <p className="hint">Mosquitto WebSocket listener (configs/mqtt/mosquitto.conf, port 9001).</p>
        </>
      ) : (
        <p className="hint">Five simulated quadcopters running the same consensus law as FlightTaskSwarm. Use it to try the GCS without PX4 SITL.</p>
      )}
      {link.error && <div className="warn bad">{link.error}</div>}
      <div className="btn-row">
        <button className="btn btn-primary" onClick={() => apply(link.source)}>
          {link.status === 'connected' ? 'Reconnect' : 'Connect'}
        </button>
        {link.status !== 'disconnected' && (
          <button className="btn" onClick={() => { disconnect(); onClose(); }}>Disconnect</button>
        )}
      </div>
    </div>
  );
};

const OverlayMenu = ({ onClose }) => {
  const overlays = useStore((s) => s.overlays);
  const toggle = useStore((s) => s.toggleOverlay);
  const ref = useRef(null);
  useOutside(ref, onClose);
  const items = [
    ['labels', 'Labels'],
    ['vectors', 'Velocity vectors (2D)'],
    ['links', 'Swarm links'],
    ['targets', 'Formation slots'],
  ];
  return (
    <div className="popover popover-right" ref={ref}>
      <div className="popover-title">Layers</div>
      {items.map(([k, label]) => (
        <label key={k} className="check">
          <input type="checkbox" checked={!!overlays[k]} onChange={() => toggle(k)} />
          {label}
        </label>
      ))}
    </div>
  );
};

export const TopBar = () => {
  const link = useStore((s) => s.link);
  const view = useStore((s) => s.view);
  const setView = useStore((s) => s.setView);
  const follow = useStore((s) => s.follow);
  const setFollow = useStore((s) => s.setFollow);
  const requestFocus = useStore((s) => s.requestFocus);
  const vehicles = useStore((s) => s.vehicles);
  const swarmCount = useStore((s) => Object.keys(s.swarms).length);
  const [menu, setMenu] = useState(null);
  const [confirmAll, setConfirmAll] = useState(null);
  const now = useNow(1000);

  const list = Object.values(vehicles);
  const ids = list.map((v) => v.id);
  const armed = list.filter((v) => v.armed).length;
  const airborne = list.filter((v) => -v.z > 0.5).length;

  const allCmd = (command) => {
    if (confirmAll !== command) {
      setConfirmAll(command);
      setTimeout(() => setConfirmAll((c) => (c === command ? null : c)), 3000);
      return;
    }
    setConfirmAll(null);
    sendCommand(ids, command);
  };

  return (
    <header className="topbar">
      <div className="brand">
        <svg width="22" height="22" viewBox="0 0 32 32" aria-hidden="true">
          <path d="M16 3 L25 26 L16 20.5 L7 26 Z" fill="var(--accent)" />
        </svg>
        <span className="brand-name">Swarm GCS</span>
      </div>

      <div className="topbar-group">
        <div className="anchor">
          <button className={`status-pill status-${link.status}`} onClick={() => setMenu(menu === 'link' ? null : 'link')}>
            <span className="status-dot" />
            {link.source === 'sim' ? 'Simulator' : 'MQTT'} · {STATUS_LABEL[link.status]}
          </button>
          {menu === 'link' && <ConnectionMenu onClose={() => setMenu(null)} />}
        </div>
        <div className="fleet-stats">
          <span><b className="mono">{list.length}</b> vehicles</span>
          <span><b className="mono">{armed}</b> armed</span>
          <span><b className="mono">{airborne}</b> airborne</span>
          <span><b className="mono">{swarmCount}</b> swarms</span>
        </div>
      </div>

      <div className="topbar-group">
        <div className="segmented" role="group" aria-label="View">
          {[['2d', '2D'], ['split', 'Split'], ['3d', '3D']].map(([k, l]) => (
            <button key={k} className={view === k ? 'active' : ''} onClick={() => setView(k)}>{l}</button>
          ))}
        </div>
        <button className="btn btn-sm" onClick={() => requestFocus([])} title="Fit all vehicles in view">Fit all</button>
        {follow !== null && (
          <button className="btn btn-sm btn-accent" onClick={() => setFollow(null)} title="Stop following">
            Following UAV {follow} ✕
          </button>
        )}
        <div className="anchor">
          <button className="btn btn-sm" onClick={() => setMenu(menu === 'layers' ? null : 'layers')}>Layers</button>
          {menu === 'layers' && <OverlayMenu onClose={() => setMenu(null)} />}
        </div>
        <span className="divider" />
        <button className={`btn btn-sm btn-warn ${confirmAll === 'hold' ? 'armed' : ''}`} disabled={!ids.length} onClick={() => allCmd('hold')}>
          {confirmAll === 'hold' ? 'Confirm hold all' : 'Hold all'}
        </button>
        <button className={`btn btn-sm btn-danger ${confirmAll === 'rtl' ? 'armed' : ''}`} disabled={!ids.length} onClick={() => allCmd('rtl')}>
          {confirmAll === 'rtl' ? 'Confirm RTL all' : 'RTL all'}
        </button>
        <span className="clock mono">{new Date(now).toLocaleTimeString([], { hour12: false })}</span>
      </div>
    </header>
  );
};
