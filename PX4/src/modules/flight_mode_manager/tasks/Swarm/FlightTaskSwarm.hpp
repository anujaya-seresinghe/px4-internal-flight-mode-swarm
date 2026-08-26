/****************************************************************************
 *
 *   Copyright (c) 2018-2019 PX4 Development Team. All rights reserved.
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



#pragma once

#include "FlightTask.hpp"

 #include <mathlib/mathlib.h>

 #include <uORB/Publication.hpp>
#include <uORB/SubscriptionInterval.hpp>

#include <containers/IntrusiveSortedList.hpp>
#include <uORB/SubscriptionCallback.hpp>
#include <uORB/topics/swarm_management.h>
#include <uORB/topics/swarm_node.h>
#include <uORB/topics/swarm_information.h>

//#include <px4_platform_common/module_params.hpp>

#include <uORB/topics/vehicle_command.h>

class Node : public IntrusiveSortedListNode<Node *>

{
	public:
		uint8_t node_id;
		float x;
		float y;
		bool operator<=(const Node &other) const
    {
        return node_id <= other.node_id;
    }

};


class ConsensusNode : public IntrusiveSortedListNode<ConsensusNode *>

{
	public:
		uint8_t node_id;
		float x;
		float y;
		float z;
		float offset_x;
		float offset_y;
		float weight;
		 bool operator<=(const ConsensusNode &other) const
    {
        return node_id <= other.node_id;
    }

};





class FlightTaskSwarm : public FlightTask
{
public:
  	FlightTaskSwarm();
  	virtual ~FlightTaskSwarm();

  	bool update() override;
	bool activate(const trajectory_setpoint_s &last_setpoint) override;
//subcribe to trajectory initiator, trajectory
private:
	int32_t _own_id;
	float _origin_z{0.f};
	uint8_t _no_of_nodes;
	uint8_t _leader_id;
	uint8_t _node_count = 0;
	IntrusiveSortedList<Node *> _node_list;
	IntrusiveSortedList<ConsensusNode *> _consensus_list;
	swarm_management_s _swarm_management;
	swarm_node_s _swarm_node;
	swarm_information_s _swarm_information;




	uORB::Subscription _swarm_management_sub{ORB_ID(swarm_management)};
	uORB::Subscription _swarm_node_sub{ORB_ID(swarm_node)};
	uORB::Subscription _swarm_information_sub{ORB_ID(swarm_information)};


	uORB::Publication<vehicle_command_s> _vehicle_command_pub{ORB_ID(vehicle_command)};

	DEFINE_PARAMETERS_CUSTOM_PARENT(FlightTask,
        (ParamInt<px4::params::MAV_SYS_ID>) _param_mav_sys_id
    )

};


