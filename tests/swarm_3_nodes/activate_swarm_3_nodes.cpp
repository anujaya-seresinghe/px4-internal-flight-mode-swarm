#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// Include your local MAVLink header library
#include "mavlink/common/mavlink.h"
// Fallback definition for MAVLink command 176 if not present in your headers
#ifndef MAV_CMD_DO_SET_MODE
#define MAV_CMD_DO_SET_MODE 176
#endif

// System and Component ID for this sender script
#define INJECTOR_SYS_ID  255
#define INJECTOR_COMP_ID MAV_COMP_ID_MISSIONPLANNER

// PX4 Custom Mode constants matching your Python reference
#define PX4_BASE_MODE_CUSTOM_ENABLED 1.0f  // MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
#define PX4_CUSTOM_MAIN_MODE_AUTO   4.0f  // Main mode AUTO
#define PX4_CUSTOM_SUB_MODE_SWARM  11.0f  // Sub mode AUTO_SWARM

struct TargetInstance {
    uint16_t port;
    uint8_t sys_id;
    sockaddr_in addr;
};

// Data structures for node payload
struct NodeData {
    uint8_t swarm_id;
    uint8_t node_id;
    float x;
    float y;
};

int main()
{
    // 1. Create UDP Socket with non-blocking timeout for ACK parsing
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket!" << std::endl;
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = 2; // 2 second timeout for receiving ACKs
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // Target SITL instances: ports 14580 (SysID 1), 14581 (SysID 2), 14582 (SysID 3)
    std::vector<TargetInstance> targets = {
        {14580, 1, {}},
        {14581, 2, {}},
        {14582, 3, {}}
    };

    for (auto &target : targets) {
        target.addr.sin_family = AF_INET;
        target.addr.sin_port = htons(target.port);
        inet_pton(AF_INET, "127.0.0.1", &target.addr.sin_addr);
    }

    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    // Helper: Send a MAVLink message to all SITL instances
    auto broadcast_msg = [&](mavlink_message_t &msg) {
        uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
        for (const auto &target : targets) {
            sendto(sock, buffer, len, 0, (struct sockaddr *)&target.addr, sizeof(target.addr));
        }
    };

    // Helper: Send a direct MAVLink message to a specific SITL instance
    auto send_to_target = [&](mavlink_message_t &msg, const TargetInstance &target) {
        uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
        sendto(sock, buffer, len, 0, (struct sockaddr *)&target.addr, sizeof(target.addr));
    };

    // -------------------------------------------------------------
    // Step 1: Set all 3 SITL instances to AUTO_SWARM mode
    // -------------------------------------------------------------
    std::cout << "--- Setting Mode to AUTO_SWARM on all SITL instances ---" << std::endl;

    for (const auto &target : targets) {
        mavlink_message_t cmd_msg;
        mavlink_msg_command_long_pack(
            INJECTOR_SYS_ID,
            INJECTOR_COMP_ID,
            &cmd_msg,
            target.sys_id,                     // Target System ID
            MAV_COMP_ID_AUTOPILOT1,            // Target Component ID
            MAV_CMD_DO_SET_MODE,               // Command
            0,                                 // Confirmation
            PX4_BASE_MODE_CUSTOM_ENABLED,      // Param 1: Base Mode
            PX4_CUSTOM_MAIN_MODE_AUTO,         // Param 2: Main Mode (AUTO)
            PX4_CUSTOM_SUB_MODE_SWARM,         // Param 3: Sub Mode (SWARM)
            0, 0, 0, 0                         // Params 4-7: Unused
        );

        send_to_target(cmd_msg, target);
        std::cout << "Sent AUTO_SWARM mode command to SysID " << (int)target.sys_id 
                  << " (Port " << target.port << ")..." << std::endl;

        // Listen for COMMAND_ACK response
        bool ack_received = false;
        uint8_t rcv_buf[MAVLINK_MAX_PACKET_LEN];
        
        while (true) {
            ssize_t bytes_rcvd = recv(sock, rcv_buf, sizeof(rcv_buf), 0);
            if (bytes_rcvd < 0) {
                break; // Timeout
            }

            mavlink_message_t rcv_msg;
            mavlink_status_t rcv_status;
            for (ssize_t i = 0; i < bytes_rcvd; ++i) {
                if (mavlink_parse_char(MAVLINK_COMM_0, rcv_buf[i], &rcv_msg, &rcv_status)) {
                    if (rcv_msg.msgid == MAVLINK_MSG_ID_COMMAND_ACK) {
                        mavlink_command_ack_t ack;
                        mavlink_msg_command_ack_decode(&rcv_msg, &ack);
                        if (ack.command == MAV_CMD_DO_SET_MODE) {
                            std::cout << "-> ACK from SysID " << (int)rcv_msg.sysid 
                                      << ": Result Code " << (int)ack.result << std::endl;
                            ack_received = true;
                            break;
                        }
                    }
                }
            }
            if (ack_received) break;
        }

        if (!ack_received) {
            std::cout << "-> Warning: No ACK received from SysID " << (int)target.sys_id << std::endl;
        }
    }

    usleep(1000000); // Wait 1 second before publishing swarm configuration

    // -------------------------------------------------------------
    // Step 2: Send SWARM_MANAGEMENT Message
    // -------------------------------------------------------------
    std::cout << "\n--- Broadcasting Swarm Configurations ---" << std::endl;

    mavlink_message_t msg_mgmt;
    mavlink_msg_swarm_management_pack(
        INJECTOR_SYS_ID,
        INJECTOR_COMP_ID,
        &msg_mgmt,
        1,  // type = 1
        1,  // swarm_id = 1
        3,  // no_of_nodes = 3
        1   // leader_id = 1
    );
    broadcast_msg(msg_mgmt);
    std::cout << "-> Broadcasted SWARM_MANAGEMENT (Type: 1, SwarmID: 1, Nodes: 3, Leader: 1)" << std::endl;

    usleep(100000); // 100ms pause

    // -------------------------------------------------------------
    // Step 3: Send SWARM_NODE Messages
    // -------------------------------------------------------------
    std::vector<NodeData> nodes = {
        {1, 1,   0.0f, 0.0f},  // Node 1
        {1, 2, -10.0f, 0.0f},  // Node 2
        {1, 3, -20.0f, 0.0f}   // Node 3
    };

    for (const auto &node : nodes) {
        mavlink_message_t msg_node;
        mavlink_msg_swarm_node_pack(
            INJECTOR_SYS_ID,
            INJECTOR_COMP_ID,
            &msg_node,
            node.swarm_id,
            node.node_id,
            node.x,
            node.y
        );
        broadcast_msg(msg_node);
        std::cout << "-> Broadcasted SWARM_NODE (SwarmID: " << (int)node.swarm_id 
                  << ", NodeID: " << (int)node.node_id 
                  << ", X: " << node.x 
                  << ", Y: " << node.y << ")" << std::endl;
        
        usleep(50000); // 50ms pause between nodes
    }

    close(sock);
    std::cout << "\nSwarm mode activation and layout deployment complete!" << std::endl;
    return 0;
}
