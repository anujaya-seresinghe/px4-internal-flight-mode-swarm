## Work in progress 
> [!WARNING]
> **SITL ONLY — DO NOT USE ON PHYSICAL HARDWARE**
> 
> This custom swarm flight mode and module are strictly designed and implemented for software-in-the-loop (**PX4 SITL**) simulation and testing purposes only. It has **not** been validated, safety-tested, or tuned for real-world physical drones. Deploying this module on operational hardware may result in unpredictable flight behavior, crashes, or severe property damage.
## Overview
Tested with PX4 v1.18.0



![3d view](docs/img/gcs_3d_view.gif)
![formation](docs/img/gcs_change_formation.gif)
![flying](docs/img/gcs_fly_here.gif)


A custom flight mode was developed to simulate UAV swarms using a consensus leader-follower control architecture. Collision avoidance is handled via artificial potential fields (APF). A custom GCS allows operators to dynamically form swarm sub-groups and assign designated leaders.



### Two MAVLINk messages are defined

    <message id="601" name="SWARM_MANAGEMENT">
      <field type="uint8_t" name="type">Type of the message: 1 = create swarm, 2 = update formation of the existing swarm swarm_id (followed by no_of_nodes SWARM_NODE with the new offsets)</field>
      <field type="uint8_t" name="swarm_id">ID of the swarm network</field>
      <field type="uint8_t" name="no_of_nodes">Number of nodes in the network</field>
      <field type="uint8_t" name="leader_id">ID of the swarm leader</field>
    </message>

    <message id="602" name="SWARM_NODE">
      <field type="uint8_t" name="swarm_id">Type of the message</field>
      <field type="uint8_t" name="node_id">Number of nodes in the network</field>
      <field type="float" name="x">x from the leader</field>
      <field type="float" name="y">y from the leader</field>
    </message>


### Three uORB messages are defined
#### Swarm management

```python
uint64 	timestamp		# time since system start (microseconds)
uint8 type			# TYPE_* below
uint8 TYPE_CREATE = 1		# start swarm swarm_id
uint8 TYPE_UPDATE_FORMATION = 2	# same swarm_id, new SwarmNode offsets, applied once all arrived
uint8 swarm_id			# id of the swarm
uint8 no_of_nodes		# number of nodes in the network
uint8 leader_id			# id of the swarm leader
```

#### Swarm node

```python
uint64 	timestamp		# time since system start (microseconds)
uint8 swarm_id			# id of the swarm
uint8 node_id
float32 x			# relative x position
float32 y			# relative y position
```

#### Swarm Information

```python
uint64 	timestamp		# time since system start (microseconds)
uint8 swarm_id
uint8 node_id
float32 x
float32 y
float32 z			# x/y/z are NaN when the message only carries yaw
float32 yaw			# NaN when the message only carries position

uint8 ORB_QUEUE_LENGTH = 32
```


### A MAVLink mode is defined
MAVLINK_MODE_SWARM is defined to include ATTITUDE and LOCAL_POSITION_NED messages.


## Using the swarm flight mode

### Quick start (one script)
`run_swarm_sim.sh` starts the PX4 container and launches 5 SITL instances by default (up to 100) in Gazebo, one at a time, in rows of 10. It then starts the mesh packet forwarder and the web GCS stack.
```
./run_swarm_sim.sh                  # 5 drones spawned at 0,0 … 0,4 (1 m apart), Gazebo headless
./run_swarm_sim.sh 25               # 25 drones (or -n 25, or SWARM_DRONES=25)
./run_swarm_sim.sh -n 5 -s 3        # 5 drones, 3 m apart
./run_swarm_sim.sh --gui            # also open the Gazebo GUI (heavy)
./run_swarm_sim.sh --build          # rebuild images first (after changing files under PX4/)
./run_swarm_sim.sh status           # what is running
./run_swarm_sim.sh logs 3           # follow PX4 instance 3's log
./run_swarm_sim.sh stop             # stop everything
```
Then open http://127.0.0.1:3000, take off from the Fleet tab and build a swarm in the Swarm tab. Full guide: [docs/swarm_gcs.md](docs/swarm_gcs.md).

### Manual start

Run the PX4 Docker container
```
docker compose -f docker-compose-px4.yaml up -d
```
and open three SITL instances inside the container with:

```
PX4_SYS_AUTOSTART=4001 PX4_SIM_MODEL=gz_x500 ./build/px4_sitl_default/bin/px4 -i 0
PX4_GZ_STANDALONE=1 PX4_SYS_AUTOSTART=4001 PX4_GZ_MODEL_POSE="0,1" PX4_SIM_MODEL=gz_x500 ./build/px4_sitl_default/bin/px4 -i 1
PX4_GZ_STANDALONE=1 PX4_GZ_MODEL_POSE="0,2" PX4_SIM_MODEL=gz_x500 ./build/px4_sitl_default/bin/px4 -i 2
```
and perfrom a takeoff for all three and execute misc/packet_forwader.py which simulates a mesh network between 3 UAVs.

### Using the web GCS
Start the Docker containers in docker-compose-web-app.yaml (Mosquitto, the MAVLink ↔ MQTT bridge and the web app)
```
docker compose -f docker-compose-web-app.yaml up -d
```
and open http://127.0.0.1:3000.

![Web GCS: take off and deploy a swarm](docs/img/gcs_deploy_swarm.gif)

The GCS has a 2D tactical map (local NED) and a 3D scene. You can show either one or both side by side. The sidebar on the right has three tabs:

- **Fleet**: live vehicle list with arming state, flight mode, altitude, speed, battery and link health, plus an attitude indicator and compass for the selected vehicle. Commands: arm, disarm, takeoff, land, hold, return, position, swarm mode, go to (click on the map), change altitude and emergency stop.
- **Swarm**: pick members and a leader, choose a formation (line, column, wedge, echelon, circle, grid, or drag nodes for a custom one) and deploy. **Change formation** updates a running swarm in place (same swarm ID) with `SWARM_MANAGEMENT` type 2. The GCS switches the members to swarm mode, then sends `SWARM_MANAGEMENT` and one `SWARM_NODE` per member, addressed only to those members. Other swarms keep running. Pairs closer than the APF radius get a warning.
- **Log**: command acknowledgements, `STATUSTEXT` from the vehicles and GCS events.

Map controls: drag to pan, scroll to zoom, shift-drag to box-select, right-click for a context menu, double-click a vehicle to follow it.

To try the GCS without PX4, open http://127.0.0.1:3000/?sim or pick *Simulator* in the connection menu. It simulates five quadcopters running the same consensus law as `FlightTaskSwarm`.

For go-to, battery and status text, the telemetry link needs the extra streams added in `PX4/ROMFS/px4fmu_common/init.d-posix/px4-rc.mavlink`. Rebuild the PX4 image after pulling this change.

#### MQTT topics

| Topic | Direction | Payload |
|---|---|---|
| `uav/local_position_ned`, `uav/attitude`, `uav/heartbeat`, `uav/global_position`, `uav/sys_status`, `uav/extended_sys_state`, `uav/command_ack`, `uav/statustext` | bridge → GCS | MAVLink fields plus `sys_id` |
| `uav/command` | GCS → bridge | `{"uav_ids": [1,2], "command": "arm\|disarm\|takeoff\|land\|hold\|rtl\|position\|swarm\|goto\|kill", "params": {"north", "east", "up", "altitude"}}` |
| `uav/swarm_management` | GCS → bridge | `{"type", "swarm_id", "no_of_nodes", "leader_id", "uav_ids"?}` |
| `uav/swarm_node` | GCS → bridge | `{"swarm_id", "node_id", "x", "y", "uav_ids"?}` |
| `uav/swarm_flight_mode` | GCS → bridge | `{"uav_ids": [...]}` (legacy, same as `command: "swarm"`) |

When the leader UAV moves, the rest of the swarm network follows.


## Tests 
## swarm_3_nodes
Compile activate_swarm_3_nodes.cpp in tests/swarm_3_nodes

Make sure misc/px4_mavlink_streams/px4-rc.mavlink is in ROMFS/px4fmu_common/init.d-posix/ 

Execute swarm_3_nodes. This creates a swarm network with 3 nodes with ID 1 as the leader. 

## swarm_virtual_leader_follower
Compile send_swarm_simulation.cpp 
Open a PX4 SITL instance with the ID 1 and perform a take off and then run compiled send_swarm_simulation.


## Misc
Follow the guide internal_flight_mode in docs to create your own custom internal flight mode 

## Reference
Wang, J., & Hu, X. (2010). Distributed Consensus in Multi-vehicle Cooperative Control: Theory and Applications (Ren, W. and Beard, R.W.; 2008) [Book Shelf]. *IEEE Control Systems Magazine*, 30(3), 85-86. https://doi.org/10.1109/MCS.2010.936430

Kuriki, Y., & Namerikawa, T. (2014). Consensus-based cooperative formation control with collision avoidance for a multi-UAV system. *2014 American Control Conference*, 2077–2082. https://doi.org/10.1109/ACC.2014.6858777





