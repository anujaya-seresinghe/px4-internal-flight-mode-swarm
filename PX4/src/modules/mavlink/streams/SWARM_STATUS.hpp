/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef SWARM_STATUS_HPP
#define SWARM_STATUS_HPP

#include <uORB/topics/swarm_status.h>
#include <uORB/topics/vehicle_status.h>

// Swarm membership from FlightTaskSwarm. Only sent on request (MAV_CMD_REQUEST_MESSAGE 603),
// and always answered: outside the Swarm mode with state 0, so a GCS knows there is no swarm.
class MavlinkStreamSwarmStatus : public MavlinkStream
{
public:
	static MavlinkStream *new_instance(Mavlink *mavlink) { return new MavlinkStreamSwarmStatus(mavlink); }

	static constexpr const char *get_name_static() { return "SWARM_STATUS"; }
	static constexpr uint16_t get_id_static() { return MAVLINK_MSG_ID_SWARM_STATUS; }

	const char *get_name() const override { return get_name_static(); }
	uint16_t get_id() override { return get_id_static(); }

	unsigned get_size() override
	{
		return MAVLINK_MSG_ID_SWARM_STATUS_LEN + MAVLINK_NUM_NON_PAYLOAD_BYTES;
	}

private:
	explicit MavlinkStreamSwarmStatus(Mavlink *mavlink) : MavlinkStream(mavlink) {}

	uORB::Subscription _swarm_status_sub{ORB_ID(swarm_status)};
	uORB::Subscription _vehicle_status_sub{ORB_ID(vehicle_status)};

	static constexpr hrt_abstime MAX_AGE{2000000}; // [us] the task publishes at 2 Hz while it runs

	bool send() override
	{
		mavlink_swarm_status_t msg{};
		swarm_status_s status{};
		vehicle_status_s vehicle_status{};

		const bool in_swarm_mode = _vehicle_status_sub.copy(&vehicle_status)
					   && vehicle_status.nav_state == vehicle_status_s::NAVIGATION_STATE_SWARM;

		if (in_swarm_mode && _swarm_status_sub.copy(&status) && hrt_elapsed_time(&status.timestamp) < MAX_AGE) {
			msg.state = status.state;
			msg.swarm_id = status.swarm_id;
			msg.leader_id = status.leader_id;
			msg.no_of_nodes = status.no_of_nodes;
			msg.node_count = math::min(status.node_count, (uint8_t)swarm_status_s::MAX_NODES);

			for (int i = 0; i < msg.node_count; i++) {
				msg.node_ids[i] = status.node_ids[i];
				msg.x[i] = status.x[i];
				msg.y[i] = status.y[i];
			}
		}

		mavlink_msg_swarm_status_send_struct(_mavlink->get_channel(), &msg);
		return true;
	}
};

#endif // SWARM_STATUS_HPP
