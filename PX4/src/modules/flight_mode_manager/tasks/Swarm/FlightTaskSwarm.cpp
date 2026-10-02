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
	// The task only exists while the Swarm mode runs
	publishStatus(swarm_status_s::STATE_NONE);
	reset();
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
	if (_swarm_management_sub.update(&_swarm_management)) {
		PX4_INFO("swarm management received (type %d)", _swarm_management.type);

		if (_swarm_management.type == swarm_management_s::TYPE_UPDATE_FORMATION
		    && _swarm_id != 0 && _swarm_management.swarm_id == _swarm_id) {
			// Keep flying the current formation until the complete new one has arrived
			_pending_list.clear();
			_pending_expected = _swarm_management.no_of_nodes;
			_pending_leader_id = _swarm_management.leader_id;
			_updating_formation = true;
			PX4_INFO("swarm %d: formation update started, expecting %d nodes", _swarm_id, _pending_expected);

		} else {
			if (_swarm_management.type == swarm_management_s::TYPE_UPDATE_FORMATION) {
				PX4_WARN("formation update for swarm %d, but this node is in swarm %d: treating it as create",
					 _swarm_management.swarm_id, _swarm_id);
			}

			if (_swarm_id != _swarm_management.swarm_id) {
				reset();
				PX4_INFO("swarm resetted");
			}

			_updating_formation = false;
			_pending_list.clear();
			_no_of_nodes = _swarm_management.no_of_nodes;
			_leader_id = _swarm_management.leader_id;
			_swarm_id = _swarm_management.swarm_id;

			// if the node is the leader, leave swarm flight mode and return to hold
			if (_leader_id == _own_id) {
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
	}

	if (_swarm_node_sub.update(&_swarm_node)) {
		if (_updating_formation) {
			if (_swarm_node.swarm_id == _swarm_id) {
				// Re-sent nodes simply overwrite their pending offset
				upsertNode(_pending_list, _swarm_node.node_id, _swarm_node.x, _swarm_node.y);

				if (_pending_list.size() == _pending_expected) {
					applyPendingFormation();
				}
			}

		} else {
			bool found = false;

			for (Node *node : _node_list) {
				if (node->node_id == _swarm_node.node_id) {
					found = true;
					break;
				}
			}

			if (!found) {
				upsertNode(_node_list, _swarm_node.node_id, _swarm_node.x, _swarm_node.y);
				_node_count++;

				if (_node_count == _no_of_nodes) {
					PX4_INFO("all swarm nodes are received");
					buildConsensus();
				}
			}
		}
	}

	// Drain the queue: every neighbour sends position and attitude separately
	while (_swarm_information_sub.update(&_swarm_information)) {
		if (!std::isnan(_swarm_information.x)) {
			for (ConsensusNode *consensus_node : _consensus_list) {
				if (consensus_node->node_id == _swarm_information.node_id) {
					consensus_node->x = _swarm_information.x;
					consensus_node->y = _swarm_information.y;
					consensus_node->z = _swarm_information.z;
					consensus_node->r_abs = sqrtf(((_position(0) - _swarm_information.x) * (_position(0) - _swarm_information.x))
								      + ((_position(1) - _swarm_information.y) * (_position(1) - _swarm_information.y)));
					consensus_node->h_abs = fabsf(_position(2) - _swarm_information.z);
					consensus_node->h_sign = sign(_position(2) - _swarm_information.z);
					consensus_node->has_position = true;
				}
			}
		}

		if (!std::isnan(_swarm_information.yaw) && _swarm_information.node_id == _leader_id) {
			_ref_yaw = _swarm_information.yaw;
		}

		if (!std::isnan(_swarm_information.z) && _swarm_information.node_id == _leader_id) {
			_ref_z = _swarm_information.z;
		}
	}

	if (_node_count == _no_of_nodes && _no_of_nodes > 1) {
		float output_x = 0;
		float output_y = 0;
		float apf_sum = 0;
		// Same consensus weight for every neighbour, read live from SWARM_WEIGHT so a change
		// from the GCS applies immediately (the task reloads parameters on parameter_update)
		const float weight = _param_swarm_weight.get();

		for (ConsensusNode *consensus_node : _consensus_list) {
			if (!consensus_node->has_position) {
				continue;
			}

			consensus_node->weight = weight;

			output_x = output_x - (consensus_node->weight * (_position(0) - consensus_node->x - consensus_node->offset_x));
			output_y = output_y - (consensus_node->weight * (_position(1) - consensus_node->y - consensus_node->offset_y));

			if (consensus_node->h_abs <= 2 * _DELTA_H && consensus_node->r_abs <= 2 * _DELTA_R) {
				apf_sum = apf_sum + ((1 / (consensus_node->h_abs + 1)) - (1 / ((2 * _DELTA_H) + 1))) * (consensus_node->h_sign / ((
						  consensus_node->h_abs + 1) * (consensus_node->h_abs + 1)));
			}
		}

		apf_sum = apf_sum * (_Kh / (_no_of_nodes - 1));

		_velocity_setpoint(0) = output_x;
		_velocity_setpoint(1) = output_y;
		_velocity_setpoint(2) = ((_ref_z - _position(2)) * _kz) + apf_sum;
		_yaw_setpoint = _ref_yaw;
	}

	if (hrt_elapsed_time(&_status_published) >= STATUS_INTERVAL) {
		uint8_t state = swarm_status_s::STATE_IDLE;

		if (_updating_formation) {
			state = swarm_status_s::STATE_UPDATING;

		} else if (_swarm_id != 0) {
			state = (_node_count == _no_of_nodes) ? swarm_status_s::STATE_ACTIVE : swarm_status_s::STATE_COLLECTING;
		}

		publishStatus(state);
	}

	return true;
}

// Swarm membership for MAVLink SWARM_STATUS: the complete formation this node flies, so a GCS
// can restore the swarm from any follower (the leader leaves the Swarm mode)
void FlightTaskSwarm::publishStatus(uint8_t state)
{
	swarm_status_s status{};
	status.state = state;

	if (state != swarm_status_s::STATE_NONE) {
		status.swarm_id = _swarm_id;
		status.leader_id = _leader_id;
		status.no_of_nodes = _no_of_nodes;

		for (Node *node : _node_list) {
			if (status.node_count >= swarm_status_s::MAX_NODES) {
				break;
			}

			status.node_ids[status.node_count] = node->node_id;
			status.x[status.node_count] = node->x;
			status.y[status.node_count] = node->y;
			status.node_count++;
		}
	}

	status.timestamp = hrt_absolute_time();
	_swarm_status_pub.publish(status);
	_status_published = status.timestamp;
}

void FlightTaskSwarm::upsertNode(IntrusiveSortedList<Node *> &list, uint8_t node_id, float x, float y)
{
	for (Node *node : list) {
		if (node->node_id == node_id) {
			node->x = x;
			node->y = y;
			return;
		}
	}

	Node *node = new Node();
	node->node_id = node_id;
	node->x = x;
	node->y = y;
	list.add(node);
}

// (Re)build the consensus neighbours from _node_list. Neighbour positions already known are kept
// so the control output does not jump when the formation changes.
void FlightTaskSwarm::buildConsensus()
{
	Node *own_node = nullptr;

	for (Node *node : _node_list) {
		if (node->node_id == _own_id) {
			own_node = node;
			break;
		}
	}

	ConsensusNode *fresh[UINT8_MAX] {};
	size_t count = 0;

	if (own_node != nullptr) {
		for (Node *node : _node_list) {
			if (node->node_id == _own_id || count >= UINT8_MAX) {
				continue;
			}

			ConsensusNode *consensus_node = new ConsensusNode();
			consensus_node->node_id = node->node_id;
			consensus_node->offset_x = own_node->x - node->x;
			consensus_node->offset_y = own_node->y - node->y;
			consensus_node->weight = _param_swarm_weight.get();

			for (ConsensusNode *old : _consensus_list) {
				if (old->node_id == node->node_id) {
					consensus_node->x = old->x;
					consensus_node->y = old->y;
					consensus_node->z = old->z;
					consensus_node->r_abs = old->r_abs;
					consensus_node->h_abs = old->h_abs;
					consensus_node->h_sign = old->h_sign;
					consensus_node->has_position = old->has_position;
					break;
				}
			}

			fresh[count++] = consensus_node;
		}

	} else {
		PX4_WARN("swarm %d: this node (%d) is not part of the formation", _swarm_id, (int)_own_id);
	}

	_consensus_list.clear();

	for (size_t i = 0; i < count; i++) {
		_consensus_list.add(fresh[i]);
	}
}

void FlightTaskSwarm::applyPendingFormation()
{
	// Move the pending nodes into the active list
	Node *pending[UINT8_MAX] {};
	size_t count = 0;

	for (Node *node : _pending_list) {
		if (count < UINT8_MAX) {
			pending[count++] = node;
		}
	}

	_node_list.clear();

	for (size_t i = 0; i < count; i++) {
		_pending_list.remove(pending[i]);
		_node_list.add(pending[i]);
	}

	_pending_list.clear();
	_no_of_nodes = _pending_expected;
	_node_count = static_cast<uint8_t>(count);
	_leader_id = _pending_leader_id;
	_updating_formation = false;

	buildConsensus();
	PX4_INFO("swarm %d: new formation applied (%d nodes)", _swarm_id, _no_of_nodes);
}

void FlightTaskSwarm::reset()
{
	_swarm_id = 0;
	_no_of_nodes = 0;
	_node_count = 0;

	_consensus_list.clear();
	_node_list.clear();
	_pending_list.clear();
	_pending_expected = 0;
	_updating_formation = false;
}


int8_t FlightTaskSwarm::sign(float x) {
    if (x < 0) {
        return -1;
    }
    if (x > 0) {
        return 1;
    }
    return 0;
}
