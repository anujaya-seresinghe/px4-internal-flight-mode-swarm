import React, { useState } from 'react';
import { useMqttUavs } from './hooks/useMqttUavs';
import { UavCanvas } from './components/UavCanvas';
import { FloatingToolbar } from './components/FloatingToolbar';
import { SwarmConfigModal } from './components/SwarmConfigModal';

const App = () => {
  const { uavs, publishMessage } = useMqttUavs('ws://127.0.0.1:9001');

  const [selectedBox, setSelectedBox] = useState(null);
  const [capturedUavIds, setCapturedUavIds] = useState([]);
  const [showModal, setShowModal] = useState(false);

  const handleSelectionComplete = (box, uavIds) => {
    setSelectedBox(box);
    setCapturedUavIds(uavIds);
  };

  const handleOpenModal = () => {
    if (capturedUavIds.length === 0) {
      alert('No UAVs found inside the selected region!');
      setSelectedBox(null);
      return;
    }
    setShowModal(true);
  };

  const handlePublishSwarmFlightMode = (targetIds = capturedUavIds) => {
    if (targetIds.length === 0) {
      alert('No UAVs selected for Swarm Flight Mode!');
      return;
    }
    publishMessage('uav/swarm_flight_mode', { uav_ids: targetIds });
  };

  const handleCreateSwarm = ({ swarmId, leaderId, uavIds }) => {
    // 1. Publish Swarm Management
    publishMessage('uav/swarm_management', {
      type: 1,
      swarm_id: swarmId,
      no_of_nodes: uavIds.length,
      leader_id: leaderId,
    });

    // Reset selection UI
    setShowModal(false);
    setSelectedBox(null);

    // 2. Sequentially publish nodes
    setTimeout(() => {
      uavIds.forEach((nodeId, index) => {
        setTimeout(() => {
          publishMessage('uav/swarm_node', {
            swarm_id: swarmId,
            node_id: nodeId,
            x: 10 * nodeId,
            y: 0,
          });
        }, index * 100);
      });
    }, 500);
  };

  return (
    <div style={{ position: 'relative', width: '100vw', height: '100vh', userSelect: 'none' }}>
      <UavCanvas
        uavs={uavs}
        disabled={showModal}
        onSelectionComplete={handleSelectionComplete}
      />

      {!showModal && (
        <FloatingToolbar
          selectedBox={selectedBox}
          onCreateSwarm={handleOpenModal}
          onSetFlightMode={() => handlePublishSwarmFlightMode()}
        />
      )}

      {showModal && (
        <SwarmConfigModal
          selectedUavIds={capturedUavIds}
          onClose={() => {
            setShowModal(false);
            setSelectedBox(null);
          }}
          onSubmitSwarm={handleCreateSwarm}
          onSetFlightMode={handlePublishSwarmFlightMode}
        />
      )}
    </div>
  );
};

export default App;