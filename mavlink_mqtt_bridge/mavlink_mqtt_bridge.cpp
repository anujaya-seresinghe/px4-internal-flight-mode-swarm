#include <iostream>
#include <map>
#include <vector>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mosquitto.h>
#include <nlohmann/json.hpp>
#include "mavlink/common/mavlink.h"

using json = nlohmann::json;

#ifndef MAV_CMD_DO_SET_MODE
#define MAV_CMD_DO_SET_MODE 176
#endif

// Sender System & Component Constants
#define INJECTOR_SYS_ID              255
#define INJECTOR_COMP_ID             190 // MAV_COMP_ID_MISSIONPLANNER

// PX4 Mode Constants
#define PX4_BASE_MODE_CUSTOM_ENABLED 1.0f
#define PX4_CUSTOM_MAIN_MODE_AUTO    4.0f
#define PX4_CUSTOM_SUB_MODE_SWARM   11.0f

struct VehicleTarget {
    sockaddr_in address;
    uint16_t port;
};

// Global context structure passed to Mosquitto callbacks
struct BridgeContext {
    int udp_sock;
    std::map<uint8_t, VehicleTarget>* uav_registry;
};

// Helper function to publish MQTT messages
void publish_mqtt(struct mosquitto *mosq, const std::string &topic, const std::string &payload) {
    int rc = mosquitto_publish(mosq, NULL, topic.c_str(), payload.length(), payload.c_str(), 0, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT Publish failed (" << rc << "): " << mosquitto_strerror(rc) << std::endl;
    }
}

// MQTT Connect Callback
void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    if (rc == 0) {
        std::cout << "Connected to MQTT Broker. Subscribing to topics..." << std::endl;
        mosquitto_subscribe(mosq, NULL, "uav/swarm_management", 0);
        mosquitto_subscribe(mosq, NULL, "uav/swarm_node", 0);
        mosquitto_subscribe(mosq, NULL, "uav/swarm_flight_mode", 0);
    } else {
        std::cerr << "Mosquitto connect callback reported error code: " << rc << std::endl;
    }
}

// MQTT Message Callback to handle incoming MQTT topics
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    if (!msg || !msg->payload || msg->payloadlen == 0) return;

    BridgeContext *ctx = static_cast<BridgeContext*>(obj);
    if (!ctx || !ctx->uav_registry) return;

    std::string topic(msg->topic);
    std::string payload_str(static_cast<char*>(msg->payload), msg->payloadlen);

    try {
        json j = json::parse(payload_str);

        if (topic == "uav/swarm_flight_mode") {
            std::cout << "=== uav/swarm_flight_mode message received ===" << std::endl;
            std::vector<uint8_t> target_ids = j.at("uav_ids").get<std::vector<uint8_t>>();

            for (uint8_t target_sys_id : target_ids) {
                auto it = ctx->uav_registry->find(target_sys_id);
                if (it == ctx->uav_registry->end()) {
                    std::cerr << " -> Warning: SysID " << (int)target_sys_id 
                              << " not found in active UAV registry!" << std::endl;
                    continue;
                }

                const VehicleTarget& target = it->second;

                // Pack MAV_CMD_DO_SET_MODE command
                mavlink_message_t cmd_msg;
                mavlink_msg_command_long_pack(
                    INJECTOR_SYS_ID,               // System ID (255)
                    INJECTOR_COMP_ID,              // Component ID (190)
                    &cmd_msg,
                    target_sys_id,                 // Target System ID
                    MAV_COMP_ID_AUTOPILOT1,        // Target Component ID (1)
                    MAV_CMD_DO_SET_MODE,           // Command (176)
                    0,                             // Confirmation
                    PX4_BASE_MODE_CUSTOM_ENABLED,  // Param 1
                    PX4_CUSTOM_MAIN_MODE_AUTO,     // Param 2
                    PX4_CUSTOM_SUB_MODE_SWARM,     // Param 3
                    0.0f, 0.0f, 0.0f, 0.0f         // Params 4-7
                );

                uint8_t buf[MAVLINK_MAX_PACKET_LEN];
                uint16_t len = mavlink_msg_to_send_buffer(buf, &cmd_msg);

                sendto(
                    ctx->udp_sock,
                    buf,
                    len,
                    0,
                    (struct sockaddr*)&target.address,
                    sizeof(target.address)
                );

                std::cout << " -> Sent MAV_CMD_DO_SET_MODE (AUTO_SWARM) to SysID " 
                          << (int)target_sys_id << " on Port " << target.port << std::endl;
            }
        }
        else if (topic == "uav/swarm_management") {
            std::cout << "=== swarm management message received ===" << std::endl;
            uint8_t type = j.at("type").get<uint8_t>();
            uint8_t swarm_id = j.at("swarm_id").get<uint8_t>();
            uint8_t no_of_nodes = j.at("no_of_nodes").get<uint8_t>();
            uint8_t leader_id = j.at("leader_id").get<uint8_t>();

            // Pack MAVLink message #601 (SWARM_MANAGEMENT)
            mavlink_message_t mav_msg;
            mavlink_msg_swarm_management_pack(
                INJECTOR_SYS_ID,
                INJECTOR_COMP_ID,
                &mav_msg,
                type,
                swarm_id,
                no_of_nodes,
                leader_id
            );

            uint8_t buf[MAVLINK_MAX_PACKET_LEN];
            uint16_t len = mavlink_msg_to_send_buffer(buf, &mav_msg);

            for (const auto& pair : *(ctx->uav_registry)) {
                const VehicleTarget& target = pair.second;
                sendto(
                    ctx->udp_sock,
                    buf,
                    len,
                    0,
                    (struct sockaddr*)&target.address,
                    sizeof(target.address)
                );
            }
        }
        else if (topic == "uav/swarm_node") {
            std::cout << "=== swarm node message received ===" << std::endl;
            uint8_t swarm_id = j.at("swarm_id").get<uint8_t>();
            uint8_t node_id = j.at("node_id").get<uint8_t>();
            float x = j.at("x").get<float>();
            float y = j.at("y").get<float>();

            // Pack MAVLink message #602 (SWARM_NODE)
            mavlink_message_t mav_msg;
            mavlink_msg_swarm_node_pack(
                INJECTOR_SYS_ID,
                INJECTOR_COMP_ID,
                &mav_msg,
                swarm_id,
                node_id,
                x,
                y
            );

            uint8_t buf[MAVLINK_MAX_PACKET_LEN];
            uint16_t len = mavlink_msg_to_send_buffer(buf, &mav_msg);

            for (const auto& pair : *(ctx->uav_registry)) {
                const VehicleTarget& target = pair.second;
                sendto(
                    ctx->udp_sock,
                    buf,
                    len,
                    0,
                    (struct sockaddr*)&target.address,
                    sizeof(target.address)
                );
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling MQTT message on topic " << topic << ": " << e.what() << std::endl;
    }
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    std::cout << "=== MAVLINK TO MQTT TELEMETRY BRIDGE STARTED ===" << std::endl;

    std::map<uint8_t, VehicleTarget> uav_registry;

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
    bridge_ctx.uav_registry = &uav_registry;

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("mavlink_bridge_client", true, &bridge_ctx);
    if (!mosq) {
        std::cerr << "Failed to create Mosquitto client instance!" << std::endl;
        close(sock);
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    std::cout << "Connecting to MQTT broker on 127.0.0.1:1883..." << std::endl;
    while (mosquitto_connect(mosq, "127.0.0.1", 1883, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT Broker connection failed. Retrying in 1s..." << std::endl;
        sleep(1);
    }
    std::cout << "Successfully connected to MQTT Broker!" << std::endl;

    mosquitto_loop_start(mosq);

    std::cout << "Listening for MAVLink streams on UDP port 15300..." << std::endl;

    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
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

                if (uav_registry.find(sys_id) == uav_registry.end()) {
                    uav_registry[sys_id] = { sender_addr, sender_port };
                    std::cout << "-> Discovered New UAV! [SysID: " << (int)sys_id 
                              << "] Mapped to Port: " << sender_port << std::endl;
                } else if (uav_registry[sys_id].port != sender_port) {
                    uav_registry[sys_id].address = sender_addr;
                    uav_registry[sys_id].port = sender_port;
                    std::cout << "-> Updated UAV [SysID: " << (int)sys_id 
                              << "] Target Port: " << sender_port << std::endl;
                }

                if (msg.msgid == MAVLINK_MSG_ID_ATTITUDE) {
                    mavlink_attitude_t att;
                    mavlink_msg_attitude_decode(&msg, &att);

                    std::ostringstream json_ss;
                    json_ss << "{"
                            << "\"sys_id\":" << (int)sys_id << ","
                            << "\"roll\":" << att.roll << ","
                            << "\"pitch\":" << att.pitch << ","
                            << "\"yaw\":" << att.yaw << ","
                            << "\"rollspeed\":" << att.rollspeed << ","
                            << "\"pitchspeed\":" << att.pitchspeed << ","
                            << "\"yawspeed\":" << att.yawspeed
                            << "}";

                    publish_mqtt(mosq, "uav/attitude", json_ss.str());
                }
                else if (msg.msgid == MAVLINK_MSG_ID_LOCAL_POSITION_NED) {
                    mavlink_local_position_ned_t pos;
                    mavlink_msg_local_position_ned_decode(&msg, &pos);

                    std::ostringstream json_ss;
                    json_ss << "{"
                            << "\"sys_id\":" << (int)sys_id << ","
                            << "\"x\":" << pos.x << ","
                            << "\"y\":" << pos.y << ","
                            << "\"z\":" << pos.z << ","
                            << "\"vx\":" << pos.vx << ","
                            << "\"vy\":" << pos.vy << ","
                            << "\"vz\":" << pos.vz
                            << "}";

                    publish_mqtt(mosq, "uav/local_position_ned", json_ss.str());
                }
            }
        }
    }

    close(sock);
    mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}