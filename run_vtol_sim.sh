#!/usr/bin/env bash
# VTOL launcher: spawns standard VTOLs (gz_standard_vtol) in Gazebo (headless), the mesh relay,
# the web GCS and the ROS 2 external Swarm mode. Nothing takes off; do that from the web GCS
# (select the VTOLs -> Takeoff & transition to FW).
#
#   ./run_vtol_sim.sh                  start 3 VTOLs, 5 m apart, with the ROS 2 Swarm mode
#   ./run_vtol_sim.sh 5                5 VTOLs
#   ./run_vtol_sim.sh -s 10            10 m apart
#   ./run_vtol_sim.sh --no-ros2        without the ROS 2 Swarm mode (PX4's internal mode can't fly fixed-wing)
#   ./run_vtol_sim.sh --gui            also open the Gazebo GUI
#   ./run_vtol_sim.sh --build          rebuild the PX4 image first
#   ./run_vtol_sim.sh stop | status | logs <i>
#
# This is run_swarm_sim.sh with VTOL defaults; see that script for details.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

args=(--vtol)
ros2=1
for arg in "$@"; do
  case "$arg" in
    --no-ros2) ros2=0 ;;
    -h | --help) sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) args+=("$arg") ;;
  esac
done
# The ROS 2 external mode is what flies the swarm in fixed-wing phase
[ "$ros2" -eq 1 ] && args+=(--ros2)

# 3 VTOLs unless a count is given (positional number, -n N or SWARM_DRONES)
export SWARM_DRONES="${SWARM_DRONES:-3}"

exec "$REPO/run_swarm_sim.sh" "${args[@]}"
