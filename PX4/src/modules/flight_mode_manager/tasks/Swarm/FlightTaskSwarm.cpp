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



#include "FlightTaskSwarm.hpp"



FlightTaskSwarm::FlightTaskSwarm()
{

}

FlightTaskSwarm::~FlightTaskSwarm()
{

}


bool FlightTaskSwarm::activate(const trajectory_setpoint_s &last_setpoint)
{
  bool ret = FlightTask::activate(last_setpoint);
//       if (!_continuous_trajectory_initiate_sub.registerCallback()) {
// 		PX4_ERR("target_estimator callback registration failed");
// 	}
  PX4_INFO("FlightTaskSwarm activate was called! ret: %d", ret); // report if activation was successful
	_own_id = static_cast<uint8_t>(_param_mav_sys_id.get());
  return ret;
}

bool FlightTaskSwarm::update()
{

	if (_swarm_management_sub.updated()) {
		_swarm_management_sub.update(&_swarm_management);
		// if (_node_count == 0) {

		// }
		_no_of_nodes = _swarm_management.no_of_nodes;
		_leader_id = _swarm_management.leader_id;
		// if the node is the leader, leave swarm flight mode and return to hold
		if(_leader_id == _own_id) {
			//return false;
			vehicle_command_s command{};
			command.timestamp = hrt_absolute_time();
			command.command = vehicle_command_s::VEHICLE_CMD_DO_SET_MODE;

			// PX4 Custom Main/Sub mode parameters for Auto Loiter / Hold
			command.param1 = 1.0f; // Main mode: Auto
			command.param2 = 4.0f;
			command.param3 = 3.0f; // Sub mode: Loiter

			command.target_system = _param_mav_sys_id.get();
			command.target_component = 1;
			command.from_external = false;

			_vehicle_command_pub.publish(command);

		}

	}

	if (_swarm_node_sub.updated()) {
		_swarm_node_sub.update(&_swarm_node);
		uint8_t found = 0;

		for (Node *node : _node_list) {
			if (node->node_id == _swarm_node.node_id) {
				found = 1;
			}
		}
		if (found == 0) {
			Node *node = new Node();
			node->node_id = _swarm_node.node_id;
			node->x = _swarm_node.x;
			node->y = _swarm_node.y;
			_node_list.add(node);
			_node_count++;

			if (_node_count == _no_of_nodes) {
				PX4_INFO("all swarm nodes are received");
				Node *own_node = nullptr;
				for (Node *node_tmp : _node_list) {
					if (node_tmp->node_id == _own_id) {
						own_node = node_tmp;
						break;
					}
				}
				if (own_node != nullptr) {
					for (Node *node_tmp : _node_list) {
						if (node_tmp->node_id != _own_id) {
							ConsensusNode *consensus_node = new ConsensusNode();
							consensus_node->offset_x = own_node->x - node_tmp->x;
							consensus_node->offset_y = own_node->y - node_tmp->y;
							consensus_node->weight = 1;
							consensus_node->node_id = node_tmp->node_id;
							_consensus_list.add(consensus_node);
						}
					}
				}

			}


		}


	}

	if (_swarm_information_sub.updated()) {
		//PX4_INFO("swarm information received");
		_swarm_information_sub.update(&_swarm_information);
		if (!std::isnan(_swarm_information.x)) {
		for (ConsensusNode *consensus_node : _consensus_list) {
			if (consensus_node->node_id == _swarm_information.node_id) {
				consensus_node->x = _swarm_information.x;
				consensus_node->y = _swarm_information.y;
				consensus_node->z = _swarm_information.z;

			}
		}
		}
		if (!std::isnan(_swarm_information.x) && _swarm_information.node_id == _leader_id) {
			_ref_yaw = _swarm_information.yaw;
		}
	}

	if (_node_count == _no_of_nodes) {
		float output_x = 0;
		float output_y = 0;

		for (ConsensusNode *consensus_node : _consensus_list) {
			output_x = output_x - ( consensus_node->weight * (_position(0) - consensus_node->x - consensus_node->offset_x));
			output_y = output_y - ( consensus_node->weight * (_position(1) - consensus_node->y - consensus_node->offset_y));
		}
		_velocity_setpoint(0) = output_x;
		_velocity_setpoint(1) = output_y;
		_velocity_setpoint(2) = 0.0f;
		_yaw_setpoint = _ref_yaw;

	}


	//PX4_INFO("FlightTaskSwarm update was called!"); // report update

	return true;
}
