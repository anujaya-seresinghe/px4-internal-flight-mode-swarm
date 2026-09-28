import React from 'react';
import { useStore } from '../store';
import { sendCommand, dissolveSwarm } from '../lib/commands';

const SwarmMenu = ({ swarm, world, run }) => {
  const select = useStore((s) => s.select);
  return (
    <>
      <div className="context-menu-title">
        Swarm {swarm.id} · leader UAV {swarm.leaderId}
      </div>
      {/* Followers track the leader, so moving the swarm means moving its leader */}
      <button onClick={run(() => sendCommand([swarm.leaderId], 'goto', { north: world.n, east: world.e }))}>
        Fly here
      </button>
      <button onClick={run(() => sendCommand([swarm.leaderId], 'hold'))}>Hold leader</button>
      <button onClick={run(() => select(swarm.members))}>Select members</button>
      <button onClick={run(() => dissolveSwarm(swarm))}>Dissolve swarm</button>
    </>
  );
};

export const VehicleContextMenu = ({ menu, onClose }) => {
  const setFollow = useStore((s) => s.setFollow);
  const { ids = [], world, x, y, swarm } = menu;
  const label = ids.length === 1 ? `UAV ${ids[0]}` : `${ids.length} UAVs`;

  const run = (fn) => () => {
    fn();
    onClose();
  };

  return (
    <div className="context-menu" style={{ left: x, top: y }} onPointerDown={(e) => e.stopPropagation()}>
      <div className="context-menu-head mono">
        N {world.n.toFixed(1)} · E {world.e.toFixed(1)}
      </div>
      {swarm ? (
        <SwarmMenu swarm={swarm} world={world} run={run} />
      ) : ids.length === 0 ? (
        <div className="context-menu-empty">Select a vehicle or swarm to command it</div>
      ) : (
        <>
          <div className="context-menu-title">{label}</div>
          <button onClick={run(() => sendCommand(ids, 'goto', { north: world.n, east: world.e }))}>Fly here</button>
          <button onClick={run(() => sendCommand(ids, 'takeoff'))}>Takeoff</button>
          <button onClick={run(() => sendCommand(ids, 'hold'))}>Hold</button>
          <button onClick={run(() => sendCommand(ids, 'land'))}>Land</button>
          <button onClick={run(() => sendCommand(ids, 'rtl'))}>Return to launch</button>
          <button onClick={run(() => sendCommand(ids, 'swarm'))}>Swarm mode</button>
          {ids.length === 1 && <button onClick={run(() => setFollow(ids[0]))}>Follow with camera</button>}
        </>
      )}
    </div>
  );
};
