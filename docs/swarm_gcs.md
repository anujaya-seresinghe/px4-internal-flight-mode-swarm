# Swarm simulation and web GCS

How to run a PX4 SITL swarm with the web ground control station, how the pieces fit together, and what to check when something doesn't work.

> SITL only. See the warning in the [README](../README.md).

## Quick start

```bash
./run_swarm_sim.sh          # 5 drones (default)
./run_swarm_sim.sh 10       # 10 drones
```

Then open **http://127.0.0.1:3000**. The pill at the top left should read **MQTT · Connected** and the Fleet tab should list the drones. Nothing takes off automatically.

| Command | What it does |
|---|---|
| `./run_swarm_sim.sh [N]` | Start N drones (1–100), the mesh relay and the web GCS |
| `-n N` or `SWARM_DRONES=N` | Same as passing N |
| `-s M` | Spawn spacing in metres (default 1) |
| `--gui` | Also open the Gazebo window (heavy; headless by default) |
| `--build` | Rebuild the PX4 image first (needed after changing files under `PX4/`) |
| `status` | Show what is running |
| `logs <i>` | Follow PX4 instance *i*'s log |
| `stop` | Stop everything |

The first run after pulling changes under `PX4/` needs `--build`. That is a full PX4 compile and takes a while.

## What the script starts

1. **PX4 container** (`px4_swarm`, from `docker-compose-px4.yaml`).
2. **Gazebo and PX4 instances**, one at a time. Instance *i* is started like the manual commands:
   ```
   HEADLESS=1 [PX4_GZ_STANDALONE=1] PX4_SYS_AUTOSTART=4001 PX4_SIM_MODEL=gz_x500 \
     PX4_GZ_MODEL_POSE=<row*s>,<col*s> ./build/px4_sitl_default/bin/px4 -d -i <i>
   ```
   Instance 0 starts Gazebo; the others join it (`PX4_GZ_STANDALONE=1`). Drones spawn in rows of 10 along east: `0,0`, `0,1` … `0,9`, then `1,0` … (with the default 1 m spacing). Instance *i* has MAVLink system ID *i + 1*. Logs are in `/tmp/px4_<i>.log` inside the container.
3. **Mesh relay** `misc/packet_forwarder.py N` on the host. It relays each drone's swarm link to every other drone, simulating the mesh network. Without it, followers never hear the leader.
4. **Web GCS stack** (`docker-compose-web-app.yaml`, compose project `px4-swarm-gcs`): Mosquitto, the MAVLink ↔ MQTT bridge, and the web app.

## Architecture

```
PX4 instance i ──UDP 15200+i──▶ packet_forwarder ──UDP 15100+j──▶ PX4 instance j   (swarm mesh)
PX4 instance i ──UDP 15400+i ⇄ 15300──▶ mavlink_mqtt_bridge ⇄ Mosquitto ⇄ web GCS (browser)
                                                              1884 (MQTT)   9001 (WebSocket)
```

| Port | Used by |
|---|---|
| 15100+i / 15200+i | Swarm mesh link of instance *i* (in / out) |
| 15400+i → 15300 | Telemetry link of instance *i* to the bridge |
| 1884 | Mosquitto MQTT (bridge). Not 1883, which a system mosquitto service may already use. |
| 9001 | Mosquitto WebSocket (browser) |
| 3000 | Web GCS |

The telemetry link (`px4-rc.mavlink`) streams `ATTITUDE`, `LOCAL_POSITION_NED`, `GLOBAL_POSITION_INT`, `SYS_STATUS`, `EXTENDED_SYS_STATE` and `STATUSTEXT`; `HEARTBEAT` is always sent. The bridge sends a GCS heartbeat to every drone at 1 Hz. PX4 needs it to arm, because `NAV_DLL_ACT` is set.

### MQTT topics

| Topic | Direction | Payload |
|---|---|---|
| `uav/local_position_ned`, `uav/attitude`, `uav/heartbeat`, `uav/global_position`, `uav/sys_status`, `uav/extended_sys_state`, `uav/command_ack`, `uav/statustext` | bridge → GCS | MAVLink fields plus `sys_id` |
| `uav/command` | GCS → bridge | `{"uav_ids": [1,2], "command": "arm\|disarm\|takeoff\|land\|hold\|rtl\|position\|swarm\|goto\|kill", "params": {"north", "east", "up", "altitude"}}` |
| `uav/swarm_management` | GCS → bridge | `{"type", "swarm_id", "no_of_nodes", "leader_id", "uav_ids"?}`: `type` 1 = create, 2 = update formation |
| `uav/swarm_node` | GCS → bridge | `{"swarm_id", "node_id", "x", "y", "uav_ids"?}` |
| `uav/swarm_flight_mode` | GCS → bridge | `{"uav_ids": [...]}` (legacy, same as `command: "swarm"`) |

When `uav_ids` is given for swarm messages, the bridge sends them only to those drones, so other swarms keep running. `goto` is converted to `MAV_CMD_DO_REPOSITION` from the drone's local and global position.

## Using the GCS

### Layout

![Taking off and deploying a swarm](img/gcs_deploy_swarm.gif)

- **Top bar**: link status (click to change broker or switch to the simulator), fleet counts, 2D / Split / 3D, Fit all, Layers, Hold all / RTL all (click twice to confirm).
- **2D map**: north-up local NED. Drag to pan, scroll to zoom, Shift-drag to box-select, right-click for commands, double-click a drone to follow it.
- **3D view**: drag to orbit, right-drag to pan, scroll to zoom, double-click to follow.

![3D view: sending the swarm with Go to… and orbiting the camera](img/gcs_3d_view.gif)

- **Sidebar**: Fleet, Swarm and Log tabs.

Positions are each drone's own `LOCAL_POSITION_NED`. PX4 puts each drone's local origin where it spawned, so right after launch the drones overlap near (0, 0) even though they are apart in Gazebo.

### Fleet tab
- **Vehicles**: click to select, click again to deselect, Ctrl/Shift-click to multi-select. The selection panel has instruments, telemetry and commands: arm, disarm, takeoff, hold, land, return, position, swarm mode, go to (pick on map), change altitude, emergency stop (click twice).
- **Swarms**: click a swarm to select it, click again to deselect. With a swarm selected, right-click the map → **Fly here** moves its **leader**; the followers keep formation. Also: Go to…, Hold leader, Center view.

![Selecting a swarm and flying it with right-click → Fly here](img/gcs_fly_here.gif)

### Swarm tab
1. Pick members (defaults to the current selection), a leader and a swarm ID.
2. Choose a formation (line, column, wedge, echelon, circle, grid), spacing and heading, or drag followers in the editor for a custom one. Offsets are north/east metres from the leader. Pairs closer than 6 m (the APF radius) get a warning.
3. **Deploy**. The GCS:
   1. switches every member to swarm mode (the flight task only listens while active),
   2. sends `SWARM_MANAGEMENT` (the leader switches itself to Hold),
   3. sends one `SWARM_NODE` per member, 150 ms apart.

Members must be airborne first.

### Changing the formation of a running swarm

![Wedge → line abreast → circle, same swarm ID](img/gcs_change_formation.gif)

Click **Change formation** on the swarm card (or enter the existing swarm's ID in the New swarm form and click **Change formation of swarm N**), pick or draw the new formation, then click **Update formation of swarm N**. The swarm ID, members and leader stay the same (they are locked while editing). The GCS sends:

1. `SWARM_MANAGEMENT` with `type = 2` (update formation), same `swarm_id`, `no_of_nodes` and `leader_id`,
2. one `SWARM_NODE` per member with the new offsets.

No flight modes change. Each follower keeps flying the current formation while the new offsets arrive, and switches to all of them at once when the last one is in, so followers never fly a mix of old and new slots. If a `SWARM_NODE` is lost the follower simply stays in the old formation; click Update again (re-sent offsets overwrite the pending ones).

The GCS only knows swarms deployed from it; they are not restored after a page reload.

### Log tab
Command acknowledgements, `STATUSTEXT` from the drones and GCS events, with level filters.

### Built-in simulator
The GIFs on this page were recorded with it (`misc/record_gcs_gifs.py` re-records them).

`http://127.0.0.1:3000/?sim` runs 5 simulated drones in the browser with the same consensus law as `FlightTaskSwarm`. Use it to try the GCS without PX4. The page always starts on the real MQTT link otherwise; check the pill (**MQTT** vs **Simulator**) to see which you are looking at.

## PX4 changes in this repo

- **Formation update** (`SwarmManagement.msg`, `FlightTaskSwarm`): `SWARM_MANAGEMENT.type` now has meaning. `TYPE_CREATE = 1` keeps the old behaviour. `TYPE_UPDATE_FORMATION = 2` for the swarm a node already belongs to collects `no_of_nodes` new `SWARM_NODE` offsets in a pending list and swaps them in atomically, keeping the neighbours' last known positions so the control output does not jump. An update for a swarm the node is not in is treated as create. No MAVLink fields changed, so no header regeneration is needed.
- **Neighbours without a position yet** are ignored by the control law (previously their uninitialised position was used until the first message arrived). Node lists are freed properly on reset.

- **Follower yaw** (`mavlink_receiver.cpp`, `SwarmInformation.msg`, `FlightTaskSwarm.cpp`): position and attitude messages from neighbours each filled only half of an uninitialised `swarm_information`. So position messages carried a random yaw that overwrote the followers' reference yaw. Unused fields are now NaN, the topic has a queue of 32, and the task reads every queued message.
- **Extra telemetry streams** on the bridge link (`px4-rc.mavlink`) for go-to, battery and status text.

Both need a PX4 rebuild (`./run_swarm_sim.sh --build`).

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Page shows 5 fake drones | You are on the simulator. Open `http://127.0.0.1:3000/` (without `?sim`) or pick MQTT bridge in the connection menu. |
| `http://127.0.0.1:3000` does not load | The web stack starts after all drones boot. Check `./run_swarm_sim.sh status`. Port 3000 must be free. |
| "container 'px4_swarm' belongs to compose project …" | Another project uses that container name. Stop it first. |
| "Preflight Fail: barometer missing / Found 0 compass", "BARO/MAG #0 failed: TIMEOUT" | Gazebo transport discovery over Wi-Fi multicast drops subscribers, so lazy sensors stop publishing. `GZ_IP=127.0.0.1` in `docker-compose-px4.yaml` fixes it; recreate the container if it predates that change. |
| "Preflight Fail: No connection to the GCS" | The bridge is not running (its heartbeat is required). |
| Bridge connects but no drones appear | Check that PX4 streams to UDP 15300 and nothing else holds that port. `docker logs mavlink_bridge` lists discovered drones. |
| Go to fails with "no global position yet" | The PX4 image predates the extra telemetry streams; rebuild with `--build`. |
| Followers do not follow | The packet forwarder must be running with the right drone count, and all members must be in swarm mode before deploy. |
