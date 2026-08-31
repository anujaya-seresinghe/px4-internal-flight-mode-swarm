import { useState, useEffect } from 'react';
import mqtt from 'mqtt';

export const useMqttUavs = (brokerUrl = 'ws://127.0.0.1:9001') => {
  const [uavs, setUavs] = useState({});
  const [mqttClient, setMqttClient] = useState(null);

  useEffect(() => {
    const client = mqtt.connect(brokerUrl);

    client.on('connect', () => {
      client.subscribe('uav/attitude');
      client.subscribe('uav/local_position_ned');
      setMqttClient(client);
    });

    client.on('message', (topic, message) => {
      try {
        const payload = JSON.parse(message.toString());
        const { sys_id } = payload;
        if (sys_id === undefined) return;

        setUavs((prevUavs) => {
          const current = prevUavs[sys_id] || { x: 0, y: 0, z: 0, yaw: 0 };

          if (topic === 'uav/attitude') {
            return { ...prevUavs, [sys_id]: { ...current, yaw: payload.yaw } };
          } else if (topic === 'uav/local_position_ned') {
            return {
              ...prevUavs,
              [sys_id]: { ...current, x: payload.x, y: payload.y, z: payload.z },
            };
          }
          return prevUavs;
        });
      } catch (err) {
        // ignore parse error
      }
    });

    return () => client.end();
  }, [brokerUrl]);

  const publishMessage = (topic, payload) => {
    if (mqttClient) {
      mqttClient.publish(topic, JSON.stringify(payload));
    }
  };

  return { uavs, mqttClient, publishMessage };
};