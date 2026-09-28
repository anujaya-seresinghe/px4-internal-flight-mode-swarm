#!/usr/bin/env bash
# Start the Micro XRCE-DDS Agent and one Swarm mode node per PX4 instance.
set -e
source /opt/ros/humble/setup.bash
source /ws/install/setup.bash

if [ "${START_XRCE_AGENT:-1}" = "1" ]; then
  MicroXRCEAgent udp4 -p "${XRCE_AGENT_PORT:-8888}" &
  sleep 1
fi

exec ros2 launch swarm_mode swarm.launch.py \
  drones:="${SWARM_DRONES:-5}" external1_sub_mode:="${EXTERNAL1_SUB_MODE:-12}"
