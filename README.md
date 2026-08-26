## How to use the internal flight mode:
First clone the PX4 repo with all the submodules and then copy the content of the src folder of the repo to the cloned PX4.


### Two MAVLINk messages are defined

    <message id="601" name="SWARM_MANAGEMENT">
      <field type="uint8_t" name="type">Type of the message</field>
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


### Three uOrb messages are defined
#### Swarm management
uint64 	timestamp		# time since system start (microseconds)
uint8 type			# type of the message - initiate/delete etc.
uint8 swarm_id			# id of the swarm
uint8 no_of_nodes		# number of nodes in the network
uint8 leader_id			# id of the swarm leader

#### Swarm node
uint64 	timestamp		# time since system start (microseconds)
uint8 swarm_id			# id of the swarm
uint8 node_id
float32 x			# relative x position
float32 y			# relative y position


#### Swarm Information
uint64 	timestamp		# time since system start (microseconds)
uint8 swarm_id
uint8 node_id
float32 x
float32 y
float32 z
float32 yaw


## Testing using "swarm_3_nodes"

Open 3 PX4 SITL instances with for example:
PX4_SYS_AUTOSTART=4001 PX4_SIM_MODEL=gz_x500 ./build/px4_sitl_default/bin/px4 -i 0
PX4_GZ_STANDALONE=1 PX4_SYS_AUTOSTART=4001 PX4_GZ_MODEL_POSE="0,1" PX4_SIM_MODEL=gz_x500 ./build/px4_sitl_default/bin/px4 -i 1
PX4_GZ_STANDALONE=1 PX4_GZ_MODEL_POSE="0,2" PX4_SIM_MODEL=gz_x500 ./build/px4_sitl_default/bin/px4 -i 2

and perform a takeoff. 

Start MAVLink Router with the config file configs/mavrouter/main.conf
mavlink-routerd -c configs/mavrouter/main.conf





