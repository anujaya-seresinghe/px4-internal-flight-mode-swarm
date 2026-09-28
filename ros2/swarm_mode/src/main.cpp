/****************************************************************************
 * swarm_mode_node: registers the "Swarm" external flight mode for one PX4 vehicle.
 *
 * Parameters (defaults for PX4 SITL instance 0):
 *   px4_instance        PX4 instance number; sets the DDS namespace, sys_id and MAVLink port
 *   sys_id              MAV_SYS_ID of the vehicle            (default px4_instance + 1)
 *   topic_namespace     PX4 DDS namespace                    (default "" for 0, "px4_<i>" otherwise)
 *   mavlink_port        UDP port for swarm MAVLink traffic   (default 15600 + px4_instance)
 *   bridge_host/port    mavlink_mqtt_bridge                  (default 127.0.0.1:15300)
 *   external1_sub_mode  PX4_CUSTOM_SUB_MODE_EXTERNAL1 value  (default 12 for this repo's PX4)
 * Fixed-wing phase (VTOL):
 *   fw_speed_gain       airspeed change per metre ahead of/behind the slot (default 0.3)
 *   fw_lookahead        cross-track lookahead [m] for the course correction (default 30)
 *   fw_airspeed_min/max airspeed band [m/s], FW_AIRSPD_MIN/MAX of the airframe (default 10/20)
 *   fw_height_rate_max  climb/sink limit [m/s] (default 3)
 *   fw_rotate_offsets   formation offsets are forward/right of the leader's course (default true)
 *   fw_turn_rate_ff     feed forward the slot velocity of a turning leader (default true)
 ****************************************************************************/
#include <memory>
#include <string>

#include <px4_ros2/common/exception.hpp>
#include <rclcpp/rclcpp.hpp>

#include "swarm_mode/swarm_mode.hpp"

class SwarmModeNode : public rclcpp::Node
{
public:
  SwarmModeNode()
  : Node("swarm_mode")
  {
    const int instance = declare_parameter<int>("px4_instance", 0);
    const std::string default_ns = instance == 0 ? "" : "px4_" + std::to_string(instance);
    const std::string ns = declare_parameter<std::string>("topic_namespace", default_ns);

    swarm_mode::SwarmModeConfig config;
    config.topic_namespace_prefix = ns.empty() ? "" : "/" + ns + "/";
    config.sys_id = static_cast<uint8_t>(declare_parameter<int>("sys_id", instance + 1));
    config.mavlink_port = static_cast<uint16_t>(declare_parameter<int>("mavlink_port", 15600 + instance));
    config.bridge_host = declare_parameter<std::string>("bridge_host", "127.0.0.1");
    config.bridge_port = static_cast<uint16_t>(declare_parameter<int>("bridge_port", 15300));
    config.external1_sub_mode = static_cast<uint8_t>(declare_parameter<int>("external1_sub_mode", 12));
    config.fw_speed_gain = static_cast<float>(declare_parameter<double>("fw_speed_gain", 0.3));
    config.fw_lookahead = static_cast<float>(declare_parameter<double>("fw_lookahead", 30.0));
    config.fw_airspeed_min = static_cast<float>(declare_parameter<double>("fw_airspeed_min", 10.0));
    config.fw_airspeed_max = static_cast<float>(declare_parameter<double>("fw_airspeed_max", 20.0));
    config.fw_height_rate_max = static_cast<float>(declare_parameter<double>("fw_height_rate_max", 3.0));
    config.fw_rotate_offsets = declare_parameter<bool>("fw_rotate_offsets", true);
    config.fw_turn_rate_ff = declare_parameter<bool>("fw_turn_rate_ff", true);

    _mode = std::make_unique<swarm_mode::SwarmMode>(*this, config);
    if (!_mode->doRegister()) {
      throw px4_ros2::Exception("Registration of the Swarm mode failed");
    }
    RCLCPP_INFO(get_logger(), "Swarm mode registered (nav_state %d)", _mode->id());
  }

private:
  std::unique_ptr<swarm_mode::SwarmMode> _mode;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SwarmModeNode>());
  rclcpp::shutdown();
  return 0;
}
