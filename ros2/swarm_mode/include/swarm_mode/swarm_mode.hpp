/****************************************************************************
 * "Swarm" PX4 external flight mode (ROS 2).
 *
 * Same behaviour as the internal FlightTaskSwarm (PX4/src/modules/flight_mode_manager/
 * tasks/Swarm): consensus leader-follower velocity control with APF collision
 * avoidance, configured with the SWARM_MANAGEMENT / SWARM_NODE MAVLink messages and
 * fed with the neighbours' LOCAL_POSITION_NED / ATTITUDE from the mesh. PX4 is not
 * modified; the mode registers through px4_ros2_interface_lib over uXRCE-DDS.
 *
 * Multicopters (and VTOLs hovering) get the internal mode's velocity + yaw setpoint.
 * VTOLs in fixed-wing phase get the same velocity command converted to course,
 * airspeed and height rate, with the leader's velocity fed forward (a fixed-wing
 * cannot slow down to zero at its slot).
 ****************************************************************************/
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_ros2/components/mode.hpp>
#include <px4_ros2/control/setpoint_types/experimental/trajectory.hpp>
#include <px4_ros2/control/setpoint_types/fixedwing/lateral_longitudinal.hpp>
#include <px4_ros2/control/vtol.hpp>
#include <px4_ros2/odometry/attitude.hpp>
#include <px4_ros2/odometry/local_position.hpp>
#include <rclcpp/rclcpp.hpp>

#include "swarm_mode/mavlink_link.hpp"

namespace swarm_mode
{

struct SwarmModeConfig
{
  std::string topic_namespace_prefix;  // "" for PX4 instance 0, "/px4_<i>/" otherwise
  uint8_t sys_id{1};                   // MAV_SYS_ID of the vehicle this node controls
  uint16_t mavlink_port{15600};        // UDP port for swarm/mesh MAVLink traffic
  std::string bridge_host{"127.0.0.1"};
  uint16_t bridge_port{15300};         // mavlink_mqtt_bridge
  uint8_t external1_sub_mode{12};      // PX4_CUSTOM_SUB_MODE_EXTERNAL1 of the PX4 build

  // Fixed-wing phase (VTOL)
  float fw_speed_gain{0.3f};       // [(m/s)/m] airspeed change per metre ahead of/behind the slot
  float fw_lookahead{30.f};        // [m] cross-track lookahead: course correction atan(e_cross / L)
  float fw_airspeed_min{10.f};     // [m/s] FW_AIRSPD_MIN of the airframe
  float fw_airspeed_max{20.f};     // [m/s] FW_AIRSPD_MAX of the airframe
  float fw_height_rate_max{3.f};   // [m/s] limit of the climb/sink command
  bool fw_rotate_offsets{true};    // offsets are forward/right of the leader's course, not north/east
  bool fw_turn_rate_ff{true};      // feed forward the slot velocity of a turning leader (w x r)
};

class SwarmMode : public px4_ros2::ModeBase
{
public:
  SwarmMode(rclcpp::Node & node, const SwarmModeConfig & config);

  void onActivate() override;
  void onDeactivate() override;
  void updateSetpoint(float dt_s) override;

private:
  // SwarmManagement.msg TYPE_*
  static constexpr uint8_t kTypeUpdateFormation = 2;

  // FlightTaskSwarm constants
  static constexpr float kKz = 1.0f;
  static constexpr float kKh = 1.0f;
  static constexpr float kDeltaH = 3.0f;
  static constexpr float kDeltaR = 3.0f;

  struct Node
  {
    float x{0.f};
    float y{0.f};
  };

  struct ConsensusNode
  {
    uint8_t node_id{0};
    float x{0.f};
    float y{0.f};
    float z{0.f};
    float offset_x{0.f};
    float offset_y{0.f};
    float weight{1.f};
    float r_abs{0.f};
    float h_abs{0.f};
    int8_t h_sign{0};
    bool has_position{false};  // ignored by the control law until the neighbour's position arrived
  };

  void pollMavlink();
  void sendHeartbeat();
  void handleMessage(const mavlink_message_t & msg);
  void handleSwarmManagement(const mavlink_swarm_management_t & m);
  void handleSwarmNode(const mavlink_swarm_node_t & m);
  void handleNeighbourPosition(uint8_t node_id, float x, float y, float z, float vx, float vy,
    uint32_t time_boot_ms);
  void buildConsensus();
  void applyPendingFormation();
  void reset();
  void requestHold();
  Eigen::Vector3f consensusVelocity(float offset_rotation) const;
  Eigen::Vector2f slotVelocity(float offset_rotation) const;
  void updateLeaderCourse(float vx, float vy, uint32_t time_boot_ms);
  void updateMulticopter(const Eigen::Vector3f & velocity);
  void updateFixedWing(const Eigen::Vector3f & velocity, const Eigen::Vector2f & feedforward, bool formation_ready);
  void updateTransition();
  float currentCourse() const;
  uint32_t customMode() const;
  static int8_t sign(float x) {return static_cast<int8_t>((x > 0.f) - (x < 0.f));}

  rclcpp::Node & _node;
  SwarmModeConfig _config;
  std::unique_ptr<MavlinkLink> _link;
  rclcpp::TimerBase::SharedPtr _poll_timer;
  rclcpp::TimerBase::SharedPtr _heartbeat_timer;
  rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr _vehicle_command_pub;

  std::shared_ptr<px4_ros2::TrajectorySetpointType> _trajectory_setpoint;
  std::shared_ptr<px4_ros2::FwLateralLongitudinalSetpointType> _fw_setpoint;
  std::shared_ptr<px4_ros2::VTOL> _vtol;  // VTOL phase; Undefined on multicopters
  std::shared_ptr<px4_ros2::OdometryLocalPosition> _local_position;
  std::shared_ptr<px4_ros2::OdometryAttitude> _attitude;

  bool _active{false};
  Eigen::Vector3f _hold_position{Eigen::Vector3f::Zero()};

  uint8_t _swarm_id{0};
  uint8_t _no_of_nodes{0};
  uint8_t _leader_id{0};
  std::map<uint8_t, Node> _nodes;
  std::vector<ConsensusNode> _consensus;
  float _ref_yaw{0.f};
  float _ref_z{0.f};
  Eigen::Vector2f _leader_velocity{Eigen::Vector2f::Zero()};  // NED, fed forward in fixed-wing phase
  bool _leader_velocity_valid{false};
  float _leader_course{0.f};       // [rad] leader's course over ground (fixed-wing formation frame)
  float _leader_turn_rate{0.f};    // [rad/s] low-pass filtered course rate
  bool _leader_course_valid{false};
  uint32_t _leader_course_time_ms{0};  // leader's own timestamp (arrival times jitter through the relay)
  float _hold_course{0.f};  // fixed-wing: course kept until the formation is complete

  // Formation update (SWARM_MANAGEMENT type 2): collected here, swapped in when complete
  std::map<uint8_t, Node> _pending;
  uint8_t _pending_expected{0};
  uint8_t _pending_leader_id{0};
  bool _updating_formation{false};
};

}  // namespace swarm_mode
