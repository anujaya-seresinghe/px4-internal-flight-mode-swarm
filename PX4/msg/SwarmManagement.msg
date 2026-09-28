uint64 	timestamp		# time since system start (microseconds)
uint8 type			# TYPE_* below
uint8 TYPE_CREATE = 1		# start swarm swarm_id: nodes then send their offsets with SwarmNode
uint8 TYPE_UPDATE_FORMATION = 2	# same swarm_id, new offsets: keep the current formation until all no_of_nodes SwarmNode offsets arrived, then switch
uint8 swarm_id			# id of the swarm
uint8 no_of_nodes		# number of nodes in the network
uint8 leader_id			# id of the swarm leader
