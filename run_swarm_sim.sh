#!/usr/bin/env bash
# One-shot launcher: spawns PX4 SITL drones in Gazebo (headless), the mesh relay and the web GCS.
# Nothing takes off; do that from the web GCS.
#
#   ./run_swarm_sim.sh                 start 5 drones, relay and web GCS
#   ./run_swarm_sim.sh 25              25 drones (same as -n 25, up to 100)
#   ./run_swarm_sim.sh -n 5 -s 3       5 drones, 3 m apart
#   ./run_swarm_sim.sh --gui           also open the Gazebo GUI (heavy; headless by default)
#   ./run_swarm_sim.sh --ros2          also start the ROS 2 external Swarm mode (DDS agent + one node per drone)
#   ./run_swarm_sim.sh --vtol          standard VTOLs (gz_standard_vtol) instead of quadcopters, 5 m apart
#   ./run_swarm_sim.sh --build         rebuild the PX4 image first (needed after PX4/ changes)
#   ./run_swarm_sim.sh stop            stop everything
#   ./run_swarm_sim.sh status          show what is running
#   ./run_swarm_sim.sh logs <i>        follow PX4 instance i's log

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER=px4_swarm
PX4_DIR=/app/PX4-Autopilot
PX4_COMPOSE="$REPO/docker-compose-px4.yaml"
WEB_COMPOSE="$REPO/docker-compose-web-app.yaml"
ROS2_COMPOSE="$REPO/docker-compose-ros2.yaml"
COMPANION_BASE_PORT=15600 # ROS 2 Swarm mode node of instance i listens on 15600+i
STATE_DIR="$REPO/.sim"
FORWARDER_PID="$STATE_DIR/packet_forwarder.pid"
FORWARDER_LOG="$STATE_DIR/packet_forwarder.log"
# px4-rc.mavlink gives instance i ports 15100+i / 15200+i / 15400+i; from i = 100 they would
# collide with each other and with the bridge on 15300
MAX_DRONES=100
GRID_COLS=10 # spawn in rows of 10 so large swarms stay compact

NUM=${SWARM_DRONES:-5}
SPACING= # metres between spawn points: instance i spawns at 0,i*SPACING (default 1, or 5 for --vtol)
HEADLESS=1 # the Gazebo GUI is expensive with many vehicles
BUILD=0
ROS2=0
VTOL=0
MODEL=gz_x500
AUTOSTART=4001
CMD=start

info() { printf '\033[1;36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mWARN:\033[0m %s\n' "$*" >&2; }
die() { printf '\033[1;31mERROR:\033[0m %s\n' "$*" >&2; exit 1; }

usage() { sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

in_px4() { docker exec -w "$PX4_DIR" "$CONTAINER" bash -c "$1"; }

# Wait until a command succeeds, or give up after $1 seconds
wait_for() {
  local timeout=$1; shift
  local waited=0
  until "$@" >/dev/null 2>&1; do
    sleep 1
    waited=$((waited + 1))
    if [ "$waited" -ge "$timeout" ]; then return 1; fi
  done
  return 0 # an until loop otherwise returns its body's last status, i.e. failure after any retry
}

# The container name is fixed in docker-compose-px4.yaml; don't hijack one from another project
check_container_owner() {
  local project
  project=$(docker inspect "$CONTAINER" --format '{{index .Config.Labels "com.docker.compose.project"}}' 2>/dev/null) || return 0
  local ours
  ours=$(basename "$REPO" | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9_-')
  if [ -n "$project" ] && [ "$project" != "$ours" ]; then
    die "container '$CONTAINER' belongs to compose project '$project'. Stop it first (docker stop $CONTAINER) and re-run."
  fi
}

stop_px4() {
  if docker ps --format '{{.Names}}' | grep -qx "$CONTAINER"; then
    info "Stopping PX4 instances and Gazebo"
    # [b]racket patterns stop pkill matching this bash -c wrapper itself
    in_px4 "pkill -f '[b]in/px4 ' ; pkill -f '[g]z sim' ; true" || true
  fi
}

stop_forwarder() {
  if [ -f "$FORWARDER_PID" ] && kill -0 "$(cat "$FORWARDER_PID")" 2>/dev/null; then
    info "Stopping packet forwarder"
    kill "$(cat "$FORWARDER_PID")" || true
  fi
  rm -f "$FORWARDER_PID"
}

cmd_stop() {
  check_container_owner
  stop_forwarder
  stop_px4
  info "Stopping ROS 2 Swarm mode"
  docker compose -f "$ROS2_COMPOSE" down 2>/dev/null || true
  info "Stopping web GCS stack"
  docker compose -f "$WEB_COMPOSE" down
  info "Stopping PX4 container"
  docker compose -f "$PX4_COMPOSE" down
}

cmd_status() {
  check_container_owner
  echo "PX4 instances:"
  if docker ps --format '{{.Names}}' | grep -qx "$CONTAINER"; then
    in_px4 "pgrep -af '[b]in/px4 ' | sed 's/^/  /' | grep . || echo '  none'"
  else
    echo "  container not running"
  fi
  echo "Packet forwarder:"
  if [ -f "$FORWARDER_PID" ] && kill -0 "$(cat "$FORWARDER_PID")" 2>/dev/null; then
    echo "  running (pid $(cat "$FORWARDER_PID"))"
  else
    echo "  not running"
  fi
  echo "ROS 2 Swarm mode:"
  local ros
  ros=$(docker compose -f "$ROS2_COMPOSE" ps --format '  {{.Service}}: {{.State}}' 2>/dev/null || true)
  echo "${ros:-  not running}"
  echo "Web GCS stack:"
  local web
  web=$(docker compose -f "$WEB_COMPOSE" ps --format '  {{.Service}}: {{.State}}' 2>/dev/null || true)
  echo "${web:-  not running}"
}

cmd_logs() {
  local i=${1:-0}
  docker exec "$CONTAINER" tail -n 100 -f "/tmp/px4_$i.log"
}

# Gazebo spawn "x,y" for instance i: rows of GRID_COLS along y, rows stacked along x
spawn_pose() {
  echo "$(( ($1 / GRID_COLS) * SPACING )),$(( ($1 % GRID_COLS) * SPACING ))"
}

start_instance() {
  local i=$1
  local pose env
  pose=$(spawn_pose "$i")
  env="PX4_SYS_AUTOSTART=$AUTOSTART PX4_SIM_MODEL=$MODEL PX4_GZ_MODEL_POSE=$pose"
  [ "$i" -gt 0 ] && env="PX4_GZ_STANDALONE=1 $env"
  [ "$HEADLESS" -eq 1 ] && env="HEADLESS=1 $env"
  # -d: daemon mode, no interactive pxh shell (there is no stdin under docker exec -d)
  docker exec -d -w "$PX4_DIR" "$CONTAINER" bash -c \
    "$env ./build/px4_sitl_default/bin/px4 -d -i $i > /tmp/px4_$i.log 2>&1"
}

instance_ready() {
  docker exec "$CONTAINER" grep -q 'Startup script returned successfully' "/tmp/px4_$1.log"
}

gz_world_ready() {
  docker exec "$CONTAINER" bash -c "gz topic -l 2>/dev/null | grep -q '/clock'"
}

cmd_start() {
  [ "$NUM" -ge 1 ] && [ "$NUM" -le "$MAX_DRONES" ] || die "number of drones must be 1-$MAX_DRONES"
  command -v docker >/dev/null || die "docker not found"
  command -v python3 >/dev/null || die "python3 not found (needed for misc/packet_forwarder.py)"
  check_container_owner
  mkdir -p "$STATE_DIR"

  if [ "$HEADLESS" -eq 0 ]; then
    if [ -z "${DISPLAY:-}" ]; then
      warn "DISPLAY is not set, running headless"
      HEADLESS=1
    elif command -v xhost >/dev/null; then
      xhost +local:root >/dev/null 2>&1 || warn "xhost failed; the Gazebo GUI may not open"
    fi
  fi

  local build_flag=()
  [ "$BUILD" -eq 1 ] && build_flag=(--build)

  if [ "$BUILD" -eq 1 ]; then
    info "Building and starting PX4 container (full PX4 build, this takes a while)"
  else
    info "Starting PX4 container"
  fi
  docker compose -f "$PX4_COMPOSE" up -d "${build_flag[@]}"
  wait_for 30 docker exec "$CONTAINER" true || die "PX4 container did not come up"

  # Start from a clean slate so re-running the script doesn't stack instances
  stop_forwarder
  stop_px4
  sleep 2

  info "Launching PX4 instance 0 (starts Gazebo)"
  start_instance 0
  if docker exec "$CONTAINER" bash -c 'command -v gz' >/dev/null 2>&1; then
    wait_for 120 gz_world_ready || die "Gazebo world did not start; see: $0 logs 0"
  fi
  wait_for 90 instance_ready 0 || die "PX4 instance 0 did not finish booting; see: $0 logs 0"

  local i
  for ((i = 1; i < NUM; i++)); do
    info "Launching PX4 instance $i/$((NUM - 1)) at ($(spawn_pose "$i")) m"
    start_instance "$i"
    # Spawn one at a time; Gazebo drops models when many spawn at once
    wait_for 60 instance_ready "$i" || warn "instance $i is slow to boot; see: $0 logs $i"
  done

  info "Starting mesh packet forwarder for $NUM nodes"
  local relay_args=("$NUM")
  # The ROS 2 nodes must hear the same neighbours as PX4's internal mode
  [ "$ROS2" -eq 1 ] && relay_args+=(--companion-base "$COMPANION_BASE_PORT")
  nohup python3 -u "$REPO/misc/packet_forwarder.py" "${relay_args[@]}" > "$FORWARDER_LOG" 2>&1 &
  echo $! > "$FORWARDER_PID"
  sleep 1
  kill -0 "$(cat "$FORWARDER_PID")" 2>/dev/null || die "packet forwarder exited; see $FORWARDER_LOG"

  info "Starting web GCS stack (Mosquitto, MAVLink bridge, web app)"
  # Always rebuild the (small, cached) web images so bridge/web changes are picked up
  docker compose -f "$WEB_COMPOSE" up -d --build
  wait_for 120 curl -sf http://127.0.0.1:3000 || warn "web app not answering yet on :3000"

  if [ "$ROS2" -eq 1 ]; then
    if ss -lun 2>/dev/null | grep -q ':8888 '; then
      warn "UDP 8888 is already in use; another Micro XRCE-DDS Agent may be running"
    fi
    info "Starting ROS 2 external Swarm mode (DDS agent + $NUM nodes)"
    # The image is built from the PX4 image, so rebuild it together with --build
    # Recreate: PX4 was restarted, so the modes must register again
    SWARM_DRONES="$NUM" docker compose -f "$ROS2_COMPOSE" up -d --force-recreate ${build_flag[@]+"${build_flag[@]}"}
  else
    docker compose -f "$ROS2_COMPOSE" down 2>/dev/null || true
  fi

  cat <<EOF

$(printf '\033[1;32m')Swarm simulation running$(printf '\033[0m') — $NUM $([ "$VTOL" -eq 1 ] && echo "VTOLs" || echo "drones") (sys IDs 1-$NUM)
  Web GCS         http://127.0.0.1:3000
  Gazebo          $([ "$HEADLESS" -eq 1 ] && echo "headless (use --gui for the window)" || echo "GUI")
  PX4 logs        $0 logs <instance>
  Relay log       $FORWARDER_LOG
  ROS 2 Swarm     $([ "$ROS2" -eq 1 ] && echo "running (docker logs -f swarm_ros2)" || echo "off (use --ros2)")
  Status / stop   $0 status | $0 stop

The drones are on the ground; take off from the web GCS when you're ready.
EOF
}

# ---- argument parsing ----
while [ $# -gt 0 ]; do
  case "$1" in
    start | stop | status) CMD=$1 ;;
    logs) CMD=logs; LOG_INSTANCE=${2:-0}; [ $# -gt 1 ] && shift ;;
    -n | --drones) NUM=${2:?}; shift ;;
    -s | --spacing) SPACING=${2:?}; shift ;;
    --gui) HEADLESS=0 ;;
    --headless) HEADLESS=1 ;;
    --build) BUILD=1 ;;
    --ros2) ROS2=1 ;;
    --vtol) VTOL=1 ;;
    -h | --help) usage ;;
    [0-9]*) NUM=$1 ;;
    *) warn "unknown argument: $1"; usage 1 ;;
  esac
  shift
done

[[ "$NUM" =~ ^[0-9]+$ ]] || die "-n expects a number"
if [ "$VTOL" -eq 1 ]; then
  MODEL=gz_standard_vtol
  AUTOSTART=4004
  SPACING=${SPACING:-5} # ~2 m wingspan
fi
SPACING=${SPACING:-1}
[[ "$SPACING" =~ ^[0-9]+$ ]] || die "-s expects a whole number of metres"

case "$CMD" in
  start) cmd_start ;;
  stop) cmd_stop ;;
  status) cmd_status ;;
  logs) cmd_logs "${LOG_INSTANCE:-0}" ;;
esac
