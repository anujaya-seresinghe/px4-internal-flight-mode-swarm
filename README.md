## Work in progress 

## How to use the swarm internal flight mode:
Tested with PX4 v1.18.0
Clone the PX4 repository with all the submodules and then copy the content of src and msg folders of this repository to the cloned PX4.

### Follow the guide in docs to create your own custom internal flight mode 


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


## Testing using "swarm_virtual_leader_follower"
Compile send_swarm_simulation.cpp 
Open a PX4 SITL instance with the ID 1 and perform a take off and then run compiled send_swarm_simulation.



