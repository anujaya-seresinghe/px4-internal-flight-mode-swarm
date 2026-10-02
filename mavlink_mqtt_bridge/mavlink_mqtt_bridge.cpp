#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mosquitto.h>
#include <nlohmann/json.hpp>
#include "mavlink/common/mavlink.h"

using json = nlohmann::json;

// The bundled headers omit the MAV_CMD enum
#ifndef MAV_CMD_DO_SET_MODE
#define MAV_CMD_DO_SET_MODE 176
#endif
#define MAV_CMD_NAV_RETURN_TO_LAUNCH 20
#define MAV_CMD_NAV_LAND             21
#define MAV_CMD_NAV_TAKEOFF          22
#define MAV_CMD_DO_REPOSITION        192
#define MAV_CMD_COMPONENT_ARM_DISARM 400
#define MAV_CMD_DO_VTOL_TRANSITION   3000
#define MAV_CMD_NAV_VTOL_TAKEOFF     84
#define MAV_CMD_REQUEST_MESSAGE      512
#define VTOL_STATE_MC 3.0f             // MAV_VTOL_STATE_MC
#define VTOL_STATE_FW 4.0f             // MAV_VTOL_STATE_FW

// Sender System & Component Constants
#define INJECTOR_SYS_ID              255
#define INJECTOR_COMP_ID             190 // MAV_COMP_ID_MISSIONPLANNER

// PX4 Mode Constants (PX4/src/modules/commander/px4_custom_mode.h)
#define PX4_BASE_MODE_CUSTOM_ENABLED 1.0f
#define PX4_CUSTOM_MAIN_MODE_POSCTL  3.0f
#define PX4_CUSTOM_MAIN_MODE_AUTO    4.0f
#define PX4_CUSTOM_SUB_MODE_LOITER   3.0f
#define PX4_CUSTOM_SUB_MODE_SWARM   11.0f

#define ARM_TAKEOFF_DELAY_US 300000
#define EARTH_RADIUS_M 6378137.0

static const float NaN = std::numeric_limits<float>::quiet_NaN();

struct VehicleTarget {
    sockaddr_in address;
    uint16_t port;
};

// Latest position of a vehicle, needed to turn local NED targets into global ones
struct VehicleState {
    bool has_local = false;
    bool has_global = false;
    float x = 0, y = 0, z = 0;
    double lat = 0, lon = 0; // degrees
    float alt = 0;           // metres AMSL
    float yaw = 0;           // rad, from ATTITUDE
};

// Global context structure passed to Mosquitto callbacks
struct BridgeContext {
    int udp_sock;
    struct mosquitto *mosq;
    std::mutex mtx; // guards registry and state (UDP thread writes, MQTT thread reads)
    std::map<uint8_t, VehicleTarget> uav_registry;   // PX4 autopilots (component 1)
    std::map<uint8_t, VehicleState> uav_state;
    // ROS 2 Swarm mode nodes (component MAV_COMP_ID_ONBOARD_COMPUTER), keyed by vehicle sys_id
    std::map<uint8_t, VehicleTarget> companion_registry;
    std::map<uint8_t, uint32_t> companion_custom_mode; // custom_mode that selects their Swarm mode
};

// Helper function to publish MQTT messages
void publish_mqtt(struct mosquitto *mosq, const std::string &topic, const json &payload) {
    std::string s = payload.dump();
    int rc = mosquitto_publish(mosq, NULL, topic.c_str(), s.length(), s.c_str(), 0, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT Publish failed (" << rc << "): " << mosquitto_strerror(rc) << std::endl;
    }
}

// Report a GCS-side failure to the web app the same way the vehicle would
void report(BridgeContext *ctx, uint8_t sys_id, uint16_t command, uint8_t result, const std::string &text) {
    std::cerr << " -> SysID " << (int)sys_id << ": " << text << std::endl;
    publish_mqtt(ctx->mosq, "uav/statustext", {{"sys_id", sys_id}, {"severity", 4}, {"text", "GCS: " + text}});
    publish_mqtt(ctx->mosq, "uav/command_ack", {{"sys_id", sys_id}, {"command", command}, {"result", result}});
}

bool send_to(BridgeContext *ctx, uint8_t sys_id, const mavlink_message_t &msg) {
    VehicleTarget target;
    {
        std::lock_guard<std::mutex> lock(ctx->mtx);
        auto it = ctx->uav_registry.find(sys_id);
        if (it == ctx->uav_registry.end()) {
            std::cerr << " -> Warning: SysID " << (int)sys_id << " not found in active UAV registry!" << std::endl;
            return false;
        }
        target = it->second;
    }
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    sendto(ctx->udp_sock, buf, len, 0, (struct sockaddr*)&target.address, sizeof(target.address));
    return true;
}

// Send to the listed vehicles, or to every known vehicle when the list is empty
void send_to_many(BridgeContext *ctx, const std::vector<uint8_t> &ids, const mavlink_message_t &msg) {
    std::vector<uint8_t> targets = ids;
    if (targets.empty()) {
        std::lock_guard<std::mutex> lock(ctx->mtx);
        for (const auto &pair : ctx->uav_registry) targets.push_back(pair.first);
    }
    for (uint8_t id : targets) send_to(ctx, id, msg);
}

void send_udp(BridgeContext *ctx, const VehicleTarget &target, const mavlink_message_t &msg) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    sendto(ctx->udp_sock, buf, len, 0, (struct sockaddr*)&target.address, sizeof(target.address));
}

// Swarm configuration goes to the PX4 autopilots (internal Swarm mode) and to the ROS 2
// companion nodes (external Swarm mode); each only acts on it while its mode is active
void send_swarm_msg(BridgeContext *ctx, const std::vector<uint8_t> &ids, const mavlink_message_t &msg) {
    send_to_many(ctx, ids, msg);
    std::vector<VehicleTarget> companions;
    {
        std::lock_guard<std::mutex> lock(ctx->mtx);
        for (const auto &pair : ctx->companion_registry) {
            if (ids.empty() || std::find(ids.begin(), ids.end(), pair.first) != ids.end()) {
                companions.push_back(pair.second);
            }
        }
    }
    for (const auto &target : companions) send_udp(ctx, target, msg);
}

void send_command_long(BridgeContext *ctx, uint8_t sys_id, uint16_t command,
                       float p1 = 0, float p2 = 0, float p3 = 0, float p4 = 0,
                       float p5 = 0, float p6 = 0, float p7 = 0) {
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(INJECTOR_SYS_ID, INJECTOR_COMP_ID, &msg,
                                  sys_id, MAV_COMP_ID_AUTOPILOT1, command, 0,
                                  p1, p2, p3, p4, p5, p6, p7);
    send_to(ctx, sys_id, msg);
}

// Ask a vehicle's PX4 Swarm task and its ROS 2 Swarm mode node (if any) for SWARM_STATUS
void request_swarm_status(BridgeContext *ctx, uint8_t sys_id) {
    send_command_long(ctx, sys_id, MAV_CMD_REQUEST_MESSAGE, (float)MAVLINK_MSG_ID_SWARM_STATUS);
    VehicleTarget companion;
    {
        std::lock_guard<std::mutex> lock(ctx->mtx);
        auto it = ctx->companion_registry.find(sys_id);
        if (it == ctx->companion_registry.end()) return;
        companion = it->second;
    }
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(INJECTOR_SYS_ID, INJECTOR_COMP_ID, &msg,
                                  sys_id, MAV_COMP_ID_ONBOARD_COMPUTER, MAV_CMD_REQUEST_MESSAGE, 0,
                                  (float)MAVLINK_MSG_ID_SWARM_STATUS, 0, 0, 0, 0, 0, 0);
    send_udp(ctx, companion, msg);
}

void send_set_mode(BridgeContext *ctx, uint8_t sys_id, float main_mode, float sub_mode) {
    send_command_long(ctx, sys_id, MAV_CMD_DO_SET_MODE, PX4_BASE_MODE_CUSTOM_ENABLED, main_mode, sub_mode);
}

bool get_state(BridgeContext *ctx, uint8_t sys_id, VehicleState &out) {
    std::lock_guard<std::mutex> lock(ctx->mtx);
    auto it = ctx->uav_state.find(sys_id);
    if (it == ctx->uav_state.end()) return false;
    out = it->second;
    return true;
}

// MAV_CMD_DO_REPOSITION towards a local NED target. The target is converted to a global
// position using the vehicle's latest LOCAL_POSITION_NED / GLOBAL_POSITION_INT pair.
void send_goto(BridgeContext *ctx, uint8_t sys_id, const json &params) {
    VehicleState s;
    if (!get_state(ctx, sys_id, s) || !s.has_global || !s.has_local) {
        report(ctx, sys_id, MAV_CMD_DO_REPOSITION, MAV_RESULT_FAILED,
               "no global position yet (is GLOBAL_POSITION_INT streamed on the telemetry link?)");
        return;
    }

    double north = params.contains("north") ? params["north"].get<double>() : s.x;
    double east = params.contains("east") ? params["east"].get<double>() : s.y;
    double d_n = north - s.x;
    double d_e = east - s.y;
    double lat = s.lat + (d_n / EARTH_RADIUS_M) * 180.0 / M_PI;
    double lon = s.lon + (d_e / (EARTH_RADIUS_M * std::cos(s.lat * M_PI / 180.0))) * 180.0 / M_PI;
    float alt = NaN; // NaN keeps the current altitude
    if (params.contains("up")) {
        alt = s.alt + (params["up"].get<float>() - (-s.z));
    }

    mavlink_message_t msg;
    mavlink_msg_command_int_pack(INJECTOR_SYS_ID, INJECTOR_COMP_ID, &msg,
                                 sys_id, MAV_COMP_ID_AUTOPILOT1, MAV_FRAME_GLOBAL,
                                 MAV_CMD_DO_REPOSITION, 0, 0,
                                 -1.0f,                                   // default ground speed
                                 (float)MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,
                                 0.0f, NaN,                               // keep yaw
                                 (int32_t)std::llround(lat * 1e7),
                                 (int32_t)std::llround(lon * 1e7),
                                 alt);
    send_to(ctx, sys_id, msg);
}

void handle_command(BridgeContext *ctx, const json &j) {
    std::vector<uint8_t> ids = j.at("uav_ids").get<std::vector<uint8_t>>();
    std::string command = j.at("command").get<std::string>();
    json params = j.value("params", json::object());

    std::cout << "=== uav/command '" << command << "' for " << ids.size() << " UAV(s) ===" << std::endl;

    if (command == "vtol_takeoff") {
        // MAV_CMD_NAV_VTOL_TAKEOFF: climb vertically to `altitude`, transition to fixed-wing towards a
        // loiter point `loiter_distance` ahead along the vehicle's heading, then loiter there.
        // PX4 only accepts params 3-7 for this command over MAVLink, and loiters at
        // takeoff + VTO_LOITER_ALT (same for every vehicle by default). Set VTO_LOITER_ALT per vehicle
        // first so each VTOL keeps its own altitude layer.
        const float altitude = params.value("altitude", 40.0f);
        const double distance = params.value("loiter_distance", 300.0);
        for (uint8_t id : ids) {
            mavlink_message_t msg;
            char name[17] = "VTO_LOITER_ALT";
            mavlink_msg_param_set_pack(INJECTOR_SYS_ID, INJECTOR_COMP_ID, &msg, id, MAV_COMP_ID_AUTOPILOT1,
                                       name, altitude, MAV_PARAM_TYPE_REAL32);
            send_to(ctx, id, msg);
        }
        usleep(ARM_TAKEOFF_DELAY_US);
        for (uint8_t id : ids) send_command_long(ctx, id, MAV_CMD_COMPONENT_ARM_DISARM, 1.0f);
        usleep(ARM_TAKEOFF_DELAY_US);
        for (uint8_t id : ids) {
            VehicleState s;
            if (!get_state(ctx, id, s) || !s.has_global) {
                report(ctx, id, MAV_CMD_NAV_VTOL_TAKEOFF, MAV_RESULT_FAILED, "no global position yet for VTOL takeoff");
                continue;
            }
            const double heading = params.contains("heading_deg") ? params["heading_deg"].get<double>() * M_PI / 180.0 : s.yaw;
            const double lat = s.lat + (distance * std::cos(heading) / EARTH_RADIUS_M) * 180.0 / M_PI;
            const double lon = s.lon + (distance * std::sin(heading) / (EARTH_RADIUS_M * std::cos(s.lat * M_PI / 180.0))) * 180.0 / M_PI;
            mavlink_message_t msg;
            mavlink_msg_command_int_pack(INJECTOR_SYS_ID, INJECTOR_COMP_ID, &msg,
                                         id, MAV_COMP_ID_AUTOPILOT1, MAV_FRAME_GLOBAL,
                                         MAV_CMD_NAV_VTOL_TAKEOFF, 0, 0,
                                         NaN, NaN, NaN, NaN,              // params 1-4 unused (transition heads for the loiter point)
                                         (int32_t)std::llround(lat * 1e7),
                                         (int32_t)std::llround(lon * 1e7),
                                         s.alt + altitude);               // transition altitude AMSL
            send_to(ctx, id, msg);
        }
        return;
    }

    if (command == "takeoff") {
        // Arm first, then MAV_CMD_NAV_TAKEOFF (like QGC). NaN altitude -> MIS_TAKEOFF_ALT
        for (uint8_t id : ids) send_command_long(ctx, id, MAV_CMD_COMPONENT_ARM_DISARM, 1.0f);
        usleep(ARM_TAKEOFF_DELAY_US);
        for (uint8_t id : ids) {
            float alt = NaN;
            VehicleState s;
            if (params.contains("altitude") && get_state(ctx, id, s) && s.has_global) {
                alt = s.alt + params["altitude"].get<float>();
            }
            send_command_long(ctx, id, MAV_CMD_NAV_TAKEOFF, -1, 0, 0, NaN, NaN, NaN, alt);
        }
        return;
    }

    for (uint8_t id : ids) {
        if (command == "arm") {
            send_command_long(ctx, id, MAV_CMD_COMPONENT_ARM_DISARM, 1.0f);
        } else if (command == "disarm") {
            send_command_long(ctx, id, MAV_CMD_COMPONENT_ARM_DISARM, 0.0f);
        } else if (command == "transition_fw") {
            send_command_long(ctx, id, MAV_CMD_DO_VTOL_TRANSITION, VTOL_STATE_FW);
        } else if (command == "transition_mc") {
            send_command_long(ctx, id, MAV_CMD_DO_VTOL_TRANSITION, VTOL_STATE_MC);
        } else if (command == "kill") {
            send_command_long(ctx, id, MAV_CMD_COMPONENT_ARM_DISARM, 0.0f, 21196.0f);
        } else if (command == "land") {
            send_command_long(ctx, id, MAV_CMD_NAV_LAND, 0, 0, 0, NaN, NaN, NaN, NaN);
        } else if (command == "rtl") {
            send_command_long(ctx, id, MAV_CMD_NAV_RETURN_TO_LAUNCH);
        } else if (command == "hold") {
            send_set_mode(ctx, id, PX4_CUSTOM_MAIN_MODE_AUTO, PX4_CUSTOM_SUB_MODE_LOITER);
        } else if (command == "position") {
            send_set_mode(ctx, id, PX4_CUSTOM_MAIN_MODE_POSCTL, 0.0f);
        } else if (command == "swarm" && params.value("external", false)) {
            // ROS 2 external Swarm mode: its custom_mode comes from the companion's heartbeat
            uint32_t custom_mode = 0;
            {
                std::lock_guard<std::mutex> lock(ctx->mtx);
                auto it = ctx->companion_custom_mode.find(id);
                if (it != ctx->companion_custom_mode.end()) custom_mode = it->second;
            }
            if (custom_mode == 0) {
                report(ctx, id, MAV_CMD_DO_SET_MODE, MAV_RESULT_FAILED, "no ROS 2 Swarm mode registered for this vehicle");
            } else {
                send_set_mode(ctx, id, (float)((custom_mode >> 16) & 0xff), (float)((custom_mode >> 24) & 0xff));
            }
        } else if (command == "swarm") {
            send_set_mode(ctx, id, PX4_CUSTOM_MAIN_MODE_AUTO, PX4_CUSTOM_SUB_MODE_SWARM);
        } else if (command == "goto") {
            send_goto(ctx, id, params);
        } else if (command == "swarm_status") {
            request_swarm_status(ctx, id);
        } else if (command == "set_param") {
            // PARAM_SET for a float (REAL32) parameter, e.g. {"name":"SWARM_WEIGHT","value":1.5}.
            // PX4 answers with PARAM_VALUE, which is published on uav/param.
            mavlink_message_t msg;
            char name[17] = {};
            strncpy(name, params.at("name").get<std::string>().c_str(), 16);
            mavlink_msg_param_set_pack(INJECTOR_SYS_ID, INJECTOR_COMP_ID, &msg, id, MAV_COMP_ID_AUTOPILOT1,
                                       name, params.at("value").get<float>(), MAV_PARAM_TYPE_REAL32);
            send_to(ctx, id, msg);
        } else if (command == "get_param") {
            mavlink_message_t msg;
            char name[17] = {};
            strncpy(name, params.at("name").get<std::string>().c_str(), 16);
            mavlink_msg_param_request_read_pack(INJECTOR_SYS_ID, INJECTOR_COMP_ID, &msg, id, MAV_COMP_ID_AUTOPILOT1,
                                                name, -1);
            send_to(ctx, id, msg);
        } else {
            report(ctx, id, 0, MAV_RESULT_UNSUPPORTED, "unknown command '" + command + "'");
        }
    }
}

// MQTT Connect Callback
void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    if (rc == 0) {
        std::cout << "Connected to MQTT Broker. Subscribing to topics..." << std::endl;
        mosquitto_subscribe(mosq, NULL, "uav/swarm_management", 0);
        mosquitto_subscribe(mosq, NULL, "uav/swarm_node", 0);
        mosquitto_subscribe(mosq, NULL, "uav/swarm_flight_mode", 0);
        mosquitto_subscribe(mosq, NULL, "uav/command", 0);
    } else {
        std::cerr << "Mosquitto connect callback reported error code: " << rc << std::endl;
    }
}

// MQTT Message Callback to handle incoming MQTT topics
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    if (!msg || !msg->payload || msg->payloadlen == 0) return;

    BridgeContext *ctx = static_cast<BridgeContext*>(obj);
    if (!ctx) return;

    std::string topic(msg->topic);
    std::string payload_str(static_cast<char*>(msg->payload), msg->payloadlen);

    try {
        json j = json::parse(payload_str);

        if (topic == "uav/command") {
            handle_command(ctx, j);
        }
        else if (topic == "uav/swarm_flight_mode") {
            // Kept for older clients; equivalent to uav/command {"command": "swarm"}
            std::cout << "=== uav/swarm_flight_mode message received ===" << std::endl;
            for (uint8_t id : j.at("uav_ids").get<std::vector<uint8_t>>()) {
                send_set_mode(ctx, id, PX4_CUSTOM_MAIN_MODE_AUTO, PX4_CUSTOM_SUB_MODE_SWARM);
                std::cout << " -> Sent MAV_CMD_DO_SET_MODE (AUTO_SWARM) to SysID " << (int)id << std::endl;
            }
        }
        else if (topic == "uav/swarm_management") {
            std::cout << "=== swarm management message received ===" << std::endl;
            // Pack MAVLink message #601 (SWARM_MANAGEMENT)
            mavlink_message_t mav_msg;
            mavlink_msg_swarm_management_pack(
                INJECTOR_SYS_ID,
                INJECTOR_COMP_ID,
                &mav_msg,
                j.at("type").get<uint8_t>(),
                j.at("swarm_id").get<uint8_t>(),
                j.at("no_of_nodes").get<uint8_t>(),
                j.at("leader_id").get<uint8_t>()
            );
            // Optional uav_ids restricts delivery to the swarm members so other swarms keep running
            send_swarm_msg(ctx, j.value("uav_ids", std::vector<uint8_t>{}), mav_msg);
        }
        else if (topic == "uav/swarm_node") {
            std::cout << "=== swarm node message received ===" << std::endl;
            // Pack MAVLink message #602 (SWARM_NODE)
            mavlink_message_t mav_msg;
            mavlink_msg_swarm_node_pack(
                INJECTOR_SYS_ID,
                INJECTOR_COMP_ID,
                &mav_msg,
                j.at("swarm_id").get<uint8_t>(),
                j.at("node_id").get<uint8_t>(),
                j.at("x").get<float>(),
                j.at("y").get<float>()
            );
            send_swarm_msg(ctx, j.value("uav_ids", std::vector<uint8_t>{}), mav_msg);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling MQTT message on topic " << topic << ": " << e.what() << std::endl;
    }
}

void handle_mavlink(BridgeContext &ctx, const mavlink_message_t &msg) {
    uint8_t sys_id = msg.sysid;
    struct mosquitto *mosq = ctx.mosq;

    switch (msg.msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT: {
        mavlink_heartbeat_t hb;
        mavlink_msg_heartbeat_decode(&msg, &hb);
        if (msg.compid == MAV_COMP_ID_ONBOARD_COMPUTER) {
            {
                std::lock_guard<std::mutex> lock(ctx.mtx);
                ctx.companion_custom_mode[sys_id] = hb.custom_mode;
            }
            publish_mqtt(mosq, "uav/companion", {
                {"sys_id", sys_id},
                {"custom_mode", hb.custom_mode},
                {"active", (hb.base_mode & MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) != 0},
            });
            break;
        }
        if (msg.compid != MAV_COMP_ID_AUTOPILOT1 || hb.type == MAV_TYPE_GCS) break;
        publish_mqtt(mosq, "uav/heartbeat", {
            {"sys_id", sys_id},
            {"type", hb.type},
            {"base_mode", hb.base_mode},
            {"custom_mode", hb.custom_mode},
            {"system_status", hb.system_status},
        });
        break;
    }
    case MAVLINK_MSG_ID_ATTITUDE: {
        mavlink_attitude_t att;
        mavlink_msg_attitude_decode(&msg, &att);
        {
            std::lock_guard<std::mutex> lock(ctx.mtx);
            ctx.uav_state[sys_id].yaw = att.yaw;
        }
        publish_mqtt(mosq, "uav/attitude", {
            {"sys_id", sys_id},
            {"roll", att.roll}, {"pitch", att.pitch}, {"yaw", att.yaw},
            {"rollspeed", att.rollspeed}, {"pitchspeed", att.pitchspeed}, {"yawspeed", att.yawspeed},
        });
        break;
    }
    case MAVLINK_MSG_ID_LOCAL_POSITION_NED: {
        mavlink_local_position_ned_t pos;
        mavlink_msg_local_position_ned_decode(&msg, &pos);
        {
            std::lock_guard<std::mutex> lock(ctx.mtx);
            VehicleState &s = ctx.uav_state[sys_id];
            s.has_local = true;
            s.x = pos.x; s.y = pos.y; s.z = pos.z;
        }
        publish_mqtt(mosq, "uav/local_position_ned", {
            {"sys_id", sys_id},
            {"x", pos.x}, {"y", pos.y}, {"z", pos.z},
            {"vx", pos.vx}, {"vy", pos.vy}, {"vz", pos.vz},
        });
        break;
    }
    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
        mavlink_global_position_int_t gp;
        mavlink_msg_global_position_int_decode(&msg, &gp);
        {
            std::lock_guard<std::mutex> lock(ctx.mtx);
            VehicleState &s = ctx.uav_state[sys_id];
            s.has_global = true;
            s.lat = gp.lat / 1e7; s.lon = gp.lon / 1e7; s.alt = gp.alt / 1000.0f;
        }
        publish_mqtt(mosq, "uav/global_position", {
            {"sys_id", sys_id},
            {"lat", gp.lat / 1e7}, {"lon", gp.lon / 1e7},
            {"alt", gp.alt / 1000.0}, {"relative_alt", gp.relative_alt / 1000.0},
            {"hdg", gp.hdg == UINT16_MAX ? json(nullptr) : json(gp.hdg / 100.0)},
        });
        break;
    }
    case MAVLINK_MSG_ID_SYS_STATUS: {
        mavlink_sys_status_t st;
        mavlink_msg_sys_status_decode(&msg, &st);
        publish_mqtt(mosq, "uav/sys_status", {
            {"sys_id", sys_id},
            {"voltage", st.voltage_battery == UINT16_MAX ? -1.0 : st.voltage_battery / 1000.0},
            {"current", st.current_battery < 0 ? -1.0 : st.current_battery / 100.0},
            {"battery_remaining", st.battery_remaining},
            {"load", st.load / 10.0},
        });
        break;
    }
    case MAVLINK_MSG_ID_EXTENDED_SYS_STATE: {
        mavlink_extended_sys_state_t es;
        mavlink_msg_extended_sys_state_decode(&msg, &es);
        publish_mqtt(mosq, "uav/extended_sys_state", {{"sys_id", sys_id}, {"landed_state", es.landed_state}, {"vtol_state", es.vtol_state}});
        break;
    }
    case MAVLINK_MSG_ID_COMMAND_ACK: {
        mavlink_command_ack_t ack;
        mavlink_msg_command_ack_decode(&msg, &ack);
        if (ack.target_system != 0 && ack.target_system != INJECTOR_SYS_ID) break;
        if (ack.command == MAV_CMD_REQUEST_MESSAGE) break; // the answer itself is what matters
        publish_mqtt(mosq, "uav/command_ack", {{"sys_id", sys_id}, {"command", ack.command}, {"result", ack.result}});
        break;
    }
    case MAVLINK_MSG_ID_PARAM_VALUE: {
        if (msg.compid != MAV_COMP_ID_AUTOPILOT1) break;
        mavlink_param_value_t pv;
        mavlink_msg_param_value_decode(&msg, &pv);
        std::string name(pv.param_id, strnlen(pv.param_id, sizeof(pv.param_id)));
        // PX4 sends integer parameters bytewise in the float field
        json value;
        if (pv.param_type == MAV_PARAM_TYPE_REAL32) {
            value = pv.param_value;
        } else {
            int32_t i;
            memcpy(&i, &pv.param_value, sizeof(i));
            value = i;
        }
        publish_mqtt(mosq, "uav/param", {{"sys_id", sys_id}, {"name", name}, {"value", value}});
        break;
    }
    case MAVLINK_MSG_ID_SWARM_STATUS: {
        if (msg.compid != MAV_COMP_ID_AUTOPILOT1 && msg.compid != MAV_COMP_ID_ONBOARD_COMPUTER) break;
        mavlink_swarm_status_t st;
        mavlink_msg_swarm_status_decode(&msg, &st);
        json nodes = json::array();
        const int count = std::min<int>(st.node_count, sizeof(st.node_ids) / sizeof(st.node_ids[0]));
        for (int i = 0; i < count; i++) {
            nodes.push_back({{"id", st.node_ids[i]}, {"x", st.x[i]}, {"y", st.y[i]}});
        }
        publish_mqtt(mosq, "uav/swarm_status", {
            {"sys_id", sys_id},
            {"source", msg.compid == MAV_COMP_ID_AUTOPILOT1 ? "px4" : "ros2"},
            {"state", st.state},
            {"swarm_id", st.swarm_id},
            {"leader_id", st.leader_id},
            {"no_of_nodes", st.no_of_nodes},
            {"nodes", nodes},
        });
        break;
    }
    case MAVLINK_MSG_ID_STATUSTEXT: {
        mavlink_statustext_t txt;
        mavlink_msg_statustext_decode(&msg, &txt);
        std::string text(txt.text, strnlen(txt.text, sizeof(txt.text)));
        publish_mqtt(mosq, "uav/statustext", {{"sys_id", sys_id}, {"severity", txt.severity}, {"text", text}});
        break;
    }
    default:
        break;
    }
}

// Heartbeat as a GCS so PX4 sees a ground station on this link. Without it the
// "No connection to the GCS" preflight check blocks arming when NAV_DLL_ACT is set.
void gcs_heartbeat_loop(BridgeContext *ctx) {
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(INJECTOR_SYS_ID, INJECTOR_COMP_ID, &msg,
                               MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
    while (true) {
        send_to_many(ctx, {}, msg);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    std::cout << "=== MAVLINK TO MQTT TELEMETRY BRIDGE STARTED ===" << std::endl;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
        return 1;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(15300);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        std::cerr << "Failed to bind UDP port 15300: " << strerror(errno) << std::endl;
        close(sock);
        return 1;
    }

    BridgeContext bridge_ctx;
    bridge_ctx.udp_sock = sock;

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("mavlink_bridge_client", true, &bridge_ctx);
    if (!mosq) {
        std::cerr << "Failed to create Mosquitto client instance!" << std::endl;
        close(sock);
        return 1;
    }
    bridge_ctx.mosq = mosq;

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    const char *mqtt_host = std::getenv("MQTT_HOST") ? std::getenv("MQTT_HOST") : "127.0.0.1";
    int mqtt_port = std::getenv("MQTT_PORT") ? std::atoi(std::getenv("MQTT_PORT")) : 1883;
    std::cout << "Connecting to MQTT broker on " << mqtt_host << ":" << mqtt_port << "..." << std::endl;
    while (mosquitto_connect(mosq, mqtt_host, mqtt_port, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT Broker connection failed. Retrying in 1s..." << std::endl;
        sleep(1);
    }
    std::cout << "Successfully connected to MQTT Broker!" << std::endl;

    mosquitto_loop_start(mosq);
    std::thread(gcs_heartbeat_loop, &bridge_ctx).detach();

    std::cout << "Listening for MAVLink streams on UDP port 15300..." << std::endl;

    uint8_t buffer[2048];
    mavlink_status_t rcv_status;

    while (true) {
        sockaddr_in sender_addr{};
        socklen_t addr_len = sizeof(sender_addr);

        ssize_t bytes_rcvd = recvfrom(sock, buffer, sizeof(buffer), 0,
                                      (struct sockaddr*)&sender_addr, &addr_len);

        if (bytes_rcvd < 0) {
            std::cerr << "recvfrom error: " << strerror(errno) << std::endl;
            usleep(100000);
            continue;
        }

        mavlink_message_t msg;
        for (ssize_t i = 0; i < bytes_rcvd; ++i) {
            if (mavlink_parse_char(MAVLINK_COMM_0, buffer[i], &msg, &rcv_status)) {

                uint8_t sys_id = msg.sysid;
                uint16_t sender_port = ntohs(sender_addr.sin_port);

                {
                    std::lock_guard<std::mutex> lock(bridge_ctx.mtx);
                    // Keep autopilots and companions apart: they share the sys_id
                    const bool companion = msg.compid == MAV_COMP_ID_ONBOARD_COMPUTER;
                    auto &registry = companion ? bridge_ctx.companion_registry : bridge_ctx.uav_registry;
                    const char *what = companion ? "ROS 2 Swarm mode node" : "UAV";
                    auto it = registry.find(sys_id);
                    if (!companion && msg.compid != MAV_COMP_ID_AUTOPILOT1) {
                        // other components: nothing to register
                    } else if (it == registry.end()) {
                        registry[sys_id] = { sender_addr, sender_port };
                        std::cout << "-> Discovered " << what << " [SysID: " << (int)sys_id
                                  << "] Mapped to Port: " << sender_port << std::endl;
                    } else if (it->second.port != sender_port) {
                        it->second = { sender_addr, sender_port };
                        std::cout << "-> Updated " << what << " [SysID: " << (int)sys_id
                                  << "] Target Port: " << sender_port << std::endl;
                    }
                }

                handle_mavlink(bridge_ctx, msg);
            }
        }
    }

    close(sock);
    mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
