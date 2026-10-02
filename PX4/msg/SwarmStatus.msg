# Swarm membership of this vehicle, published by FlightTaskSwarm while the Swarm mode runs.
# Sent as MAVLink SWARM_STATUS (603) on request, so a GCS can restore its swarms.

uint64 timestamp		# time since system start (microseconds)

uint8 STATE_NONE = 0		# not in the Swarm mode
uint8 STATE_IDLE = 1		# in the Swarm mode, no swarm yet
uint8 STATE_COLLECTING = 2	# SWARM_MANAGEMENT received, waiting for all SWARM_NODEs
uint8 STATE_ACTIVE = 3		# flying the formation
uint8 STATE_UPDATING = 4	# formation update pending (still flying the previous one)

uint8 MAX_NODES = 20

uint8 state
uint8 swarm_id
uint8 leader_id
uint8 no_of_nodes
uint8 node_count		# valid entries in node_ids, x, y
uint8[20] node_ids
float32[20] x			# offsets as sent in SWARM_NODE
float32[20] y
