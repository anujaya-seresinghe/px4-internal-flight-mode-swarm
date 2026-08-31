import React, { useState, useEffect } from 'react';

export const SwarmConfigModal = ({
  selectedUavIds,
  onClose,
  onSubmitSwarm,
  onSetFlightMode,
}) => {
  const [selectedLeaderId, setSelectedLeaderId] = useState(selectedUavIds[0] || null);
  const [swarmId, setSwarmId] = useState(1);

  useEffect(() => {
    if (selectedUavIds.length > 0) {
      setSelectedLeaderId(selectedUavIds[0]);
    }
  }, [selectedUavIds]);

  const handleSubmit = () => {
    onSubmitSwarm({
      swarmId: parseInt(swarmId, 10) || 1,
      leaderId: selectedLeaderId,
      uavIds: selectedUavIds,
    });
  };

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        width: '100vw',
        height: '100vh',
        backgroundColor: 'rgba(0,0,0,0.4)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 100,
      }}
    >
      <div
        style={{
          backgroundColor: '#ffffff',
          borderRadius: '12px',
          padding: '24px',
          width: '380px',
          boxShadow: '0 8px 24px rgba(0,0,0,0.2)',
          fontFamily: 'sans-serif',
        }}
      >
        <h3 style={{ margin: '0 0 8px 0', fontSize: '18px', color: '#111' }}>
          Configure Swarm
        </h3>
        <p style={{ margin: '0 0 16px 0', fontSize: '13px', color: '#666' }}>
          Select one of the {selectedUavIds.length} captured UAVs to act as the leader node:
        </p>

        {/* Leader Selector */}
        <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', marginBottom: '20px' }}>
          {selectedUavIds.map((id) => (
            <button
              key={id}
              onClick={() => setSelectedLeaderId(id)}
              style={{
                flex: '1 1 calc(33.33% - 8px)',
                padding: '10px',
                borderRadius: '6px',
                border: selectedLeaderId === id ? '2px solid #007aff' : '1px solid #ccc',
                backgroundColor: selectedLeaderId === id ? '#e6f0ff' : '#f8f9fa',
                color: selectedLeaderId === id ? '#007aff' : '#333',
                fontWeight: 'bold',
                fontSize: '14px',
                cursor: 'pointer',
              }}
            >
              UAV {id}
            </button>
          ))}
        </div>

        {/* Swarm ID Input */}
        <div style={{ marginBottom: '20px' }}>
          <label
            htmlFor="swarm-id-input"
            style={{
              display: 'block',
              fontSize: '13px',
              fontWeight: 'bold',
              color: '#333',
              marginBottom: '6px',
            }}
          >
            Swarm ID
          </label>
          <input
            id="swarm-id-input"
            type="number"
            min="1"
            value={swarmId}
            onChange={(e) => setSwarmId(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 12px',
              borderRadius: '6px',
              border: '1px solid #ccc',
              fontSize: '14px',
              outline: 'none',
            }}
          />
        </div>

        {/* Actions */}
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '8px', flexWrap: 'wrap' }}>
          <button
            onClick={() => onSetFlightMode(selectedUavIds)}
            style={{
              padding: '8px 12px',
              borderRadius: '6px',
              border: 'none',
              backgroundColor: '#17a2b8',
              color: '#ffffff',
              fontWeight: 'bold',
              fontSize: '13px',
              cursor: 'pointer',
            }}
          >
            Set Swarm Mode
          </button>
          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              onClick={onClose}
              style={{
                padding: '8px 14px',
                borderRadius: '6px',
                border: '1px solid #ccc',
                backgroundColor: '#ffffff',
                color: '#333',
                cursor: 'pointer',
              }}
            >
              Cancel
            </button>
            <button
              onClick={handleSubmit}
              style={{
                padding: '8px 14px',
                borderRadius: '6px',
                border: 'none',
                backgroundColor: '#28a745',
                color: '#ffffff',
                fontWeight: 'bold',
                cursor: 'pointer',
              }}
            >
              Create Swarm
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};