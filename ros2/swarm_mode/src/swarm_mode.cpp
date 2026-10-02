/****************************************************************************
 * "Swarm" PX4 external flight mode (ROS 2). See swarm_mode.hpp.
 *
 * The message handling and control law follow FlightTaskSwarm::update() line by line
 * so both modes behave the same; differences are marked "external mode:".
 ****************************************************************************/
#include "swarm_mode/swarm_mode.hpp"

#include <algorithm>
#include <cmath>

#include <px4_msgs/msg/vehicle_status.hpp>
#include <px4_ros2/utils/message_version.hpp>

using namespace std::chrono_literals;

namespace swarm_mode
{

static const std::string kModeName = "Swarm";

SwarmMode::SwarmMode(rclcpp::Node & node, const SwarmModeConfig & config)
: ModeBase(node, Settings{kModeName}, config.topic_namespace_prefix),
  _node(node),
  _config(config)
{
  _trajectory_setpoint = std::make_shared<px4_ros2::TrajectorySetpointType>(*this);
  _fw_setpoint = std::make_shared<px4_ros2::FwLateralLongitudinalSetpointType>(*this);
  _vtol = std::make_shared<px4_ros2::VTOL>(*this);
  _local_position = std::make_shared<px4_ros2::OdometryLocalPosition>(*this);
  _attitude = std::make_shared<px4_ros2::OdometryAttitude>(*this);

  _vehicle_command_pub = node.create_publisher<px4_msgs::msg::VehicleCommand>(
    config.topic_namespace_prefix + "fmu/in/vehicle_command" +
    px4_ros2::getMessageNameVersion<px4_msgs::msg::VehicleCommand>(), 1);

  _link = std::make_unique<MavlinkLink>(config.mavlink_port, config.bridge_host, config.bridge_port);

  // Same executor thread as updateSetpoint(), so no locking is needed
  _poll_timer = node.create_wall_timer(5ms, [this] {pollMavlink();});
  _heartbeat_timer = node.create_wall_timer(1s, [this] {sendHeartbeat();});

  RCLCPP_INFO(node.get_logger(), "Swarm mode for sys_id %d, MAVLink on UDP %d, bridge %s:%d",
    config.sys_id, config.mavlink_port, config.bridge_host.c_str(), config.bridge_port);
}

void SwarmMode::onActivate()
{
  // FlightTaskSwarm starts from scratch every time the mode is entered
  reset();
  _active = true;
  _hold_position = _local_position->positionNed();
  _ref_z = _hold_position.z();
  _ref_yaw = _attitude->yaw();
  _hold_course = currentCourse();
  _leader_velocity_valid = false;
  _leader_course_valid = false;
  _leader_turn_rate = 0.f;
  RCLCPP_INFO(_node.get_logger(), "Swarm mode activated (%s)",
    _vtol->getCurrentState() == px4_ros2::VTOL::State::FixedWing ? "fixed-wing" : "multicopter");
}

void SwarmMode::onDeactivate()
{
  _active = false;
  RCLCPP_INFO(_node.get_logger(), "Swarm mode deactivated");
}

// ---- MAVLink ------------------------------------------------------------------------------

void SwarmMode::pollMavlink()
{
  _link->poll([this](const mavlink_message_t & msg) {handleMessage(msg);});
}

void SwarmMode::handleMessage(const mavlink_message_t & msg)
{
  // A GCS restoring its swarms asks every vehicle for SWARM_STATUS, also while the mode is off
  if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
    mavlink_command_long_t cmd;
    mavlink_msg_command_long_decode(&msg, &cmd);
    if (cmd.command == kMavCmdRequestMessage && std::lround(cmd.param1) == MAVLINK_MSG_ID_SWARM_STATUS &&
      cmd.target_system == _config.sys_id && cmd.target_component == MAV_COMP_ID_ONBOARD_COMPUTER)
    {
      sendSwarmStatus();
    }
    return;
  }

  // Like the uORB subscriptions of the internal task: only listen while the mode runs
  if (!_active) {
    return;
  }

  switch (msg.msgid) {
    case MAVLINK_MSG_ID_SWARM_MANAGEMENT: {
        mavlink_swarm_management_t m;
        mavlink_msg_swarm_management_decode(&msg, &m);
        handleSwarmManagement(m);
        break;
      }
    case MAVLINK_MSG_ID_SWARM_NODE: {
        mavlink_swarm_node_t m;
        mavlink_msg_swarm_node_decode(&msg, &m);
        handleSwarmNode(m);
        break;
      }
    case MAVLINK_MSG_ID_LOCAL_POSITION_NED: {
        if (msg.sysid == _config.sys_id) {break;}
        mavlink_local_position_ned_t p;
        mavlink_msg_local_position_ned_decode(&msg, &p);
        handleNeighbourPosition(msg.sysid, p.x, p.y, p.z, p.vx, p.vy, p.time_boot_ms);
        break;
      }
    case MAVLINK_MSG_ID_ATTITUDE: {
        if (msg.sysid == _config.sys_id) {break;}
        mavlink_attitude_t a;
        mavlink_msg_attitude_decode(&msg, &a);
        if (msg.sysid == _leader_id) {
          _ref_yaw = a.yaw;
        }
        break;
      }
    default:
      break;
  }
}

void SwarmMode::handleSwarmManagement(const mavlink_swarm_management_t & m)
{
  RCLCPP_INFO(_node.get_logger(), "swarm management received (type %d)", m.type);

  if (m.type == kTypeUpdateFormation && _swarm_id != 0 && m.swarm_id == _swarm_id) {
    // Keep flying the current formation until the complete new one has arrived
    _pending.clear();
    _pending_expected = m.no_of_nodes;
    _pending_leader_id = m.leader_id;
    _updating_formation = true;
    RCLCPP_INFO(_node.get_logger(), "swarm %d: formation update started, expecting %d nodes",
      _swarm_id, _pending_expected);
    return;
  }

  if (m.type == kTypeUpdateFormation) {
    RCLCPP_WARN(_node.get_logger(),
      "formation update for swarm %d, but this node is in swarm %d: treating it as create",
      m.swarm_id, _swarm_id);
  }

  if (_swarm_id != m.swarm_id) {
    reset();
    RCLCPP_INFO(_node.get_logger(), "swarm resetted");
  }

  _updating_formation = false;
  _pending.clear();
  _no_of_nodes = m.no_of_nodes;
  _leader_id = m.leader_id;
  _swarm_id = m.swarm_id;

  // if the node is the leader, leave swarm flight mode and return to hold
  if (_leader_id == _config.sys_id) {
    requestHold();
  }
}

void SwarmMode::handleSwarmNode(const mavlink_swarm_node_t & m)
{
  if (_updating_formation) {
    if (m.swarm_id == _swarm_id) {
      // Re-sent nodes simply overwrite their pending offset
      _pending[m.node_id] = Node{m.x, m.y};
      if (_pending.size() == _pending_expected) {
        applyPendingFormation();
      }
    }
    return;
  }

  if (_nodes.count(m.node_id) == 0) {
    _nodes[m.node_id] = Node{m.x, m.y};
    if (_nodes.size() == _no_of_nodes) {
      RCLCPP_INFO(_node.get_logger(), "all swarm nodes are received");
      buildConsensus();
    }
  }
}

void SwarmMode::handleNeighbourPosition(uint8_t node_id, float x, float y, float z, float vx, float vy,
  uint32_t time_boot_ms)
{
  const Eigen::Vector3f pos = _local_position->positionNed();
  for (ConsensusNode & c : _consensus) {
    if (c.node_id == node_id) {
      c.x = x;
      c.y = y;
      c.z = z;
      c.r_abs = std::hypot(pos.x() - x, pos.y() - y);
      c.h_abs = std::fabs(pos.z() - z);
      c.h_sign = sign(pos.z() - z);
      c.has_position = true;
    }
  }
  if (node_id == _leader_id) {
    _ref_z = z;
    _leader_velocity = {vx, vy};
    _leader_velocity_valid = true;
    updateLeaderCourse(vx, vy, time_boot_ms);
  }
}

void SwarmMode::sendHeartbeat()
{
  // Tells the bridge this companion exists and which custom_mode selects the Swarm mode,
  // so the GCS can switch vehicles into it (external mode IDs are assigned at runtime)
  mavlink_message_t msg;
  mavlink_msg_heartbeat_pack(_config.sys_id, MAV_COMP_ID_ONBOARD_COMPUTER, &msg,
    MAV_TYPE_ONBOARD_CONTROLLER, MAV_AUTOPILOT_INVALID,
    _active ? MAV_MODE_FLAG_CUSTOM_MODE_ENABLED : 0, customMode(), MAV_STATE_ACTIVE);
  _link->sendToPeer(msg);
}

// Swarm membership (the complete formation), like FlightTaskSwarm's SWARM_STATUS
void SwarmMode::sendSwarmStatus()
{
  mavlink_swarm_status_t status{};
  SwarmState state = SwarmState::None;
  if (_active) {
    if (_updating_formation) {
      state = SwarmState::Updating;
    } else if (_swarm_id == 0) {
      state = SwarmState::Idle;
    } else {
      state = _nodes.size() == _no_of_nodes ? SwarmState::Active : SwarmState::Collecting;
    }
  }
  status.state = static_cast<uint8_t>(state);
  if (state != SwarmState::None) {
    status.swarm_id = _swarm_id;
    status.leader_id = _leader_id;
    status.no_of_nodes = _no_of_nodes;
    constexpr size_t kMaxNodes = sizeof(status.node_ids) / sizeof(status.node_ids[0]);
    for (const auto & [id, node] : _nodes) {
      if (status.node_count >= kMaxNodes) {
        break;
      }
      status.node_ids[status.node_count] = id;
      status.x[status.node_count] = node.x;
      status.y[status.node_count] = node.y;
      status.node_count++;
    }
  }
  mavlink_message_t msg;
  mavlink_msg_swarm_status_encode(_config.sys_id, MAV_COMP_ID_ONBOARD_COMPUTER, &msg, &status);
  _link->sendToPeer(msg);
}

uint32_t SwarmMode::customMode() const
{
  const auto nav_state = id();
  constexpr auto kExternal1 = px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_EXTERNAL1;
  constexpr auto kExternal8 = px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_EXTERNAL8;
  if (nav_state < kExternal1 || nav_state > kExternal8) {
    return 0;  // not registered (yet)
  }
  // PX4 custom_mode: main mode in bits 16-23, sub mode in bits 24-31 (px4_custom_mode.h)
  constexpr uint32_t kMainModeAuto = 4;
  const uint32_t sub_mode = _config.external1_sub_mode + (nav_state - kExternal1);
  return (sub_mode << 24) | (kMainModeAuto << 16);
}

// ---- formation ----------------------------------------------------------------------------

// (Re)build the consensus neighbours from _nodes. Neighbour positions already known are kept
// so the control output does not jump when the formation changes.
void SwarmMode::buildConsensus()
{
  const auto own = _nodes.find(_config.sys_id);
  std::vector<ConsensusNode> fresh;

  if (own == _nodes.end()) {
    RCLCPP_WARN(_node.get_logger(), "swarm %d: this node (%d) is not part of the formation",
      _swarm_id, _config.sys_id);
  } else {
    for (const auto & [id, node] : _nodes) {
      if (id == _config.sys_id) {
        continue;
      }
      ConsensusNode c;
      c.node_id = id;
      c.offset_x = own->second.x - node.x;
      c.offset_y = own->second.y - node.y;
      for (const ConsensusNode & old : _consensus) {
        if (old.node_id == id) {
          c.x = old.x;
          c.y = old.y;
          c.z = old.z;
          c.r_abs = old.r_abs;
          c.h_abs = old.h_abs;
          c.h_sign = old.h_sign;
          c.has_position = old.has_position;
          break;
        }
      }
      fresh.push_back(c);
    }
  }
  _consensus = std::move(fresh);
}

void SwarmMode::applyPendingFormation()
{
  _nodes = std::move(_pending);
  _pending.clear();
  _no_of_nodes = _pending_expected;
  _leader_id = _pending_leader_id;
  _updating_formation = false;
  buildConsensus();
  RCLCPP_INFO(_node.get_logger(), "swarm %d: new formation applied (%d nodes)", _swarm_id,
    _no_of_nodes);
}

void SwarmMode::reset()
{
  _swarm_id = 0;
  _no_of_nodes = 0;
  _nodes.clear();
  _consensus.clear();
  _pending.clear();
  _pending_expected = 0;
  _updating_formation = false;
}

void SwarmMode::requestHold()
{
  // PX4 Custom Main/Sub mode parameters for Auto Loiter / Hold, as in FlightTaskSwarm
  px4_msgs::msg::VehicleCommand cmd{};
  cmd.timestamp = _node.get_clock()->now().nanoseconds() / 1000;
  cmd.command = px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE;
  cmd.param1 = 1.0f;  // custom mode enabled
  cmd.param2 = 4.0f;  // main mode: Auto
  cmd.param3 = 3.0f;  // sub mode: Loiter
  cmd.target_system = _config.sys_id;
  cmd.target_component = 1;
  cmd.source_system = _config.sys_id;
  cmd.source_component = MAV_COMP_ID_ONBOARD_COMPUTER;
  cmd.from_external = true;
  _vehicle_command_pub->publish(cmd);
  RCLCPP_INFO(_node.get_logger(), "designated leader of swarm %d: switching to Hold", _swarm_id);
}

// ---- control law --------------------------------------------------------------------------

void SwarmMode::updateSetpoint(float /*dt_s*/)
{
  const auto vtol_state = _vtol->getCurrentState();
  if (vtol_state == px4_ros2::VTOL::State::TransitionToFixedWing ||
    vtol_state == px4_ros2::VTOL::State::TransitionToMulticopter)
  {
    updateTransition();
    return;
  }

  const bool fixed_wing = vtol_state == px4_ros2::VTOL::State::FixedWing;
  const bool formation_ready = _no_of_nodes > 1 && _nodes.size() == _no_of_nodes;

  if (!formation_ready) {
    if (fixed_wing) {
      updateFixedWing(Eigen::Vector3f::Zero(), Eigen::Vector2f::Zero(), false);
    } else {
      // external mode: hold where the mode was entered until the formation is known
      // (the internal task leaves its setpoints unset in that phase)
      _trajectory_setpoint->updatePosition(_hold_position);
    }
    return;
  }

  if (fixed_wing) {
    // Fixed-wing: formation frame = leader's course, slot velocity fed forward
    const float rotation = _config.fw_rotate_offsets ? _leader_course : 0.f;
    updateFixedWing(consensusVelocity(rotation), slotVelocity(rotation), true);
  } else {
    // multicopter, or VTOL in hover (VTOL state Undefined/Multicopter): identical to FlightTaskSwarm
    updateMulticopter(consensusVelocity(0.f));
  }
}

// Consensus + APF of FlightTaskSwarm. offset_rotation turns the formation offsets from
// north/east into the leader's frame (0 for multicopters: identical to the internal mode).
Eigen::Vector3f SwarmMode::consensusVelocity(float offset_rotation) const
{
  const Eigen::Vector3f pos = _local_position->positionNed();
  const float c_rot = std::cos(offset_rotation);
  const float s_rot = std::sin(offset_rotation);
  float output_x = 0.f;
  float output_y = 0.f;
  float apf_sum = 0.f;

  for (const ConsensusNode & c : _consensus) {
    if (!c.has_position) {
      continue;
    }
    const float offset_x = c_rot * c.offset_x - s_rot * c.offset_y;
    const float offset_y = s_rot * c.offset_x + c_rot * c.offset_y;
    output_x -= c.weight * (pos.x() - c.x - offset_x);
    output_y -= c.weight * (pos.y() - c.y - offset_y);

    if (c.h_abs <= 2 * kDeltaH && c.r_abs <= 2 * kDeltaR) {
      apf_sum += ((1 / (c.h_abs + 1)) - (1 / ((2 * kDeltaH) + 1))) *
        (c.h_sign / ((c.h_abs + 1) * (c.h_abs + 1)));
    }
  }
  apf_sum *= kKh / (_no_of_nodes - 1);

  return {output_x, output_y, ((_ref_z - pos.z()) * kKz) + apf_sum};
}

// Velocity of this vehicle's slot: leader velocity plus, for a turning leader, w x r where r is
// the slot relative to the leader (a slot on the outside of a turn has to fly faster).
Eigen::Vector2f SwarmMode::slotVelocity(float offset_rotation) const
{
  const Eigen::Vector2f leader = _leader_velocity_valid ? _leader_velocity : Eigen::Vector2f::Zero();
  const auto own = _nodes.find(_config.sys_id);
  const auto lead = _nodes.find(_leader_id);
  if (!_config.fw_turn_rate_ff || !_leader_course_valid || own == _nodes.end() || lead == _nodes.end()) {
    return leader;
  }
  const float dx = own->second.x - lead->second.x;
  const float dy = own->second.y - lead->second.y;
  const float c_rot = std::cos(offset_rotation);
  const float s_rot = std::sin(offset_rotation);
  const Eigen::Vector2f r{c_rot * dx - s_rot * dy, s_rot * dx + c_rot * dy};
  // NED, turn rate positive clockwise seen from above: w x r = w * (-r_e, r_n).
  // Capped at half the leader speed so a turn-rate error can never flip the slot's direction.
  Eigen::Vector2f turn = _leader_turn_rate * Eigen::Vector2f{-r.y(), r.x()};
  const float turn_max = 0.5f * leader.norm();
  if (turn.norm() > turn_max) {
    turn *= turn_max / turn.norm();
  }
  return leader + turn;
}

void SwarmMode::updateLeaderCourse(float vx, float vy, uint32_t time_boot_ms)
{
  constexpr float kMinSpeed = 3.f;        // [m/s] course is meaningless when slow
  constexpr float kRateFilter = 0.1f;     // low-pass on the differentiated course (10 Hz input)
  // [rad/s] a 30 deg coordinated turn at 15 m/s is g*tan(30)/15 = 0.38 rad/s. Differentiating the
  // course amplifies noise and loiter-entry turns; unclamped spikes banked a follower to 53 deg
  constexpr float kMaxTurnRate = 0.35f;

  if (std::hypot(vx, vy) < kMinSpeed) {
    _leader_course = _ref_yaw;
    _leader_turn_rate = 0.f;
    _leader_course_valid = false;
    return;
  }

  const float course = std::atan2(vy, vx);
  if (_leader_course_valid) {
    const float dt = static_cast<float>(time_boot_ms - _leader_course_time_ms) * 1e-3f;
    if (dt > 1e-3f && dt < 1.f) {
      const float d_course = std::remainder(course - _leader_course, 2.f * static_cast<float>(M_PI));
      const float rate = std::clamp(d_course / dt, -kMaxTurnRate, kMaxTurnRate);
      _leader_turn_rate += kRateFilter * (rate - _leader_turn_rate);
    }
  }
  _leader_course = course;
  _leader_course_time_ms = time_boot_ms;
  _leader_course_valid = true;
}

void SwarmMode::updateMulticopter(const Eigen::Vector3f & velocity)
{
  _trajectory_setpoint->update(velocity, {}, _ref_yaw);
}

// Fixed-wing: the consensus velocity goes to zero at the slot, which a fixed-wing cannot fly.
// Feed the leader's velocity forward and use the consensus term as a correction on top, then
// turn the result into course + airspeed + height rate.
void SwarmMode::updateFixedWing(const Eigen::Vector3f & velocity, const Eigen::Vector2f & feedforward,
  bool formation_ready)
{
  constexpr float kMinSpeedForCourse = 1.f;  // [m/s] below this the direction is noise

  const float height_rate = std::clamp(-velocity.z(), -_config.fw_height_rate_max, _config.fw_height_rate_max);

  if (!formation_ready) {
    // keep the course the mode was entered with, hold altitude, default airspeed
    const Eigen::Vector3f pos = _local_position->positionNed();
    const float hold_rate = std::clamp(-(_hold_position.z() - pos.z()) * kKz,
        -_config.fw_height_rate_max, _config.fw_height_rate_max);
    _fw_setpoint->update(px4_ros2::FwLateralLongitudinalSetpoint()
      .withCourse(_hold_course)
      .withHeightRate(hold_rate));
    return;
  }

  // Consensus target: the average of where the neighbours say this vehicle should be.
  // consensusVelocity() returns -sum(pos - target_j), so the error to the target is -sum / n.
  int neighbours = 0;
  for (const ConsensusNode & c : _consensus) {
    neighbours += c.has_position ? 1 : 0;
  }
  const Eigen::Vector2f error = neighbours > 0 ?
    Eigen::Vector2f(-velocity.head<2>() / static_cast<float>(neighbours)) : Eigen::Vector2f::Zero();

  // Split the error along the slot's motion: along-track -> airspeed, cross-track -> course.
  // (Flying the direction of "slot velocity + gain * error" oscillates when the slot turns.)
  const float slot_speed = feedforward.norm();
  const Eigen::Vector2f along = slot_speed > kMinSpeedForCourse ?
    Eigen::Vector2f(feedforward / slot_speed) :
    Eigen::Vector2f(std::cos(_leader_course), std::sin(_leader_course));
  const Eigen::Vector2f right{-along.y(), along.x()};  // NED: right of the direction of travel
  const float e_along = error.dot(along);   // > 0: ahead of the slot
  const float e_cross = error.dot(right);   // > 0: right of the slot's path

  const float slot_course = std::atan2(along.y(), along.x());
  const float course = std::remainder(slot_course - std::atan(e_cross / _config.fw_lookahead),
      2.f * static_cast<float>(M_PI));
  const float airspeed = std::clamp(slot_speed - _config.fw_speed_gain * e_along,
      _config.fw_airspeed_min, _config.fw_airspeed_max);
  // Turn with the slot instead of waiting for a course error (FRD: right turn positive)
  constexpr float kMaxLateralAccel = 9.81f * 0.577f;  // g * tan(30 deg): feedforward banks at most 30 deg
  const float lateral_accel = _config.fw_turn_rate_ff ?
    std::clamp(slot_speed * _leader_turn_rate, -kMaxLateralAccel, kMaxLateralAccel) : 0.f;

  _fw_setpoint->update(px4_ros2::FwLateralLongitudinalSetpoint()
    .withCourse(course)
    .withEquivalentAirspeed(airspeed)
    .withLateralAcceleration(lateral_accel)
    .withHeightRate(height_rate));
}

// During a transition PX4 needs both halves, as in the px4_ros2 VTOL example: the multicopter
// part decelerates/accelerates, the fixed-wing part keeps the current course level.
void SwarmMode::updateTransition()
{
  const Eigen::Vector3f acceleration = _vtol->computeAccelerationSetpointDuringTransition();
  _trajectory_setpoint->update(Eigen::Vector3f{NAN, NAN, 0.f}, acceleration);
  _fw_setpoint->updateWithHeightRate(0.f, currentCourse());
}

float SwarmMode::currentCourse() const
{
  const Eigen::Vector3f v = _local_position->velocityNed();
  if (std::hypot(v.x(), v.y()) > 1.f) {
    return std::atan2(v.y(), v.x());
  }
  return _attitude->yaw();
}

}  // namespace swarm_mode
