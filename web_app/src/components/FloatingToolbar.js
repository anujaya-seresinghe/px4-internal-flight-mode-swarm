import React from 'react';

export const FloatingToolbar = ({ selectedBox, onCreateSwarm, onSetFlightMode }) => {
  if (!selectedBox) return null;

  return (
    <div
      style={{
        position: 'absolute',
        left: `${selectedBox.left + selectedBox.width / 2}px`,
        top: `${selectedBox.top + selectedBox.height + 12}px`,
        transform: 'translateX(-50%)',
        display: 'flex',
        gap: '8px',
        zIndex: 10,
      }}
    >
      <button
        onClick={onCreateSwarm}
        style={{
          backgroundColor: '#007aff',
          color: '#ffffff',
          border: 'none',
          borderRadius: '6px',
          padding: '10px 18px',
          fontSize: '14px',
          fontWeight: 'bold',
          cursor: 'pointer',
          boxShadow: '0 4px 12px rgba(0,0,0,0.15)',
        }}
      >
        Create Swarm
      </button>
      <button
        onClick={onSetFlightMode}
        style={{
          backgroundColor: '#17a2b8',
          color: '#ffffff',
          border: 'none',
          borderRadius: '6px',
          padding: '10px 18px',
          fontSize: '14px',
          fontWeight: 'bold',
          cursor: 'pointer',
          boxShadow: '0 4px 12px rgba(0,0,0,0.15)',
        }}
      >
        Set On Swarm Flight Mode
      </button>
    </div>
  );
};