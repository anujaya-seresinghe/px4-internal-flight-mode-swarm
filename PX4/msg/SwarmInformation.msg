uint64 	timestamp		# time since system start (microseconds)
uint8 swarm_id
uint8 node_id
float32 x
float32 y
float32 z			# x/y/z are NaN when the message only carries yaw
float32 yaw			# NaN when the message only carries position

# Every neighbour sends position and attitude; queue them so none are dropped between task updates
uint8 ORB_QUEUE_LENGTH = 32
