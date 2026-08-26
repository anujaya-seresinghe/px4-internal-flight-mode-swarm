#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// Local MAVLink headers
#include "mavlink/common/mavlink.h"

#define INJECTOR_SYS_ID  255
#define INJECTOR_COMP_ID MAV_COMP_ID_MISSIONPLANNER

// Fallback command definition if not in headers
#ifndef MAV_CMD_DO_SET_MODE
#define MAV_CMD_DO_SET_MODE 176
#endif

// PX4 Custom Mode constants
#define PX4_BASE_MODE_CUSTOM_ENABLED 1.0f
#define PX4_CUSTOM_MAIN_MODE_AUTO   4.0f
#define PX4_CUSTOM_SUB_MODE_SWARM  11.0f

// Virtual Vehicle Specs (Node 2 / SysID 2)
#define VIRTUAL_SYS_ID 2
#define CIRCLE_RADIUS  200.0f  // 200 meters
#define OMEGA          0.2f    // Angular velocity (rad/s)

struct TargetInstance {
    uint16_t port;
    sockaddr_in addr;
};

struct NodeData {
    uint8_t swarm_id;
    uint8_t node_id;
    float x;
    float y;
};

int main()
{
    // Create UDP Socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket!" << std::endl;
        return 1;
    }

    // Target SITL ports (adjust ports to match your running SITL instances)
    std::vector<uint16_t> ports = {14580, 14581, 14582};
    std::vector<TargetInstance> targets;

    for (uint16_t port : ports) {
        TargetInstance target;
        target.port = port;
        target.addr.sin_family = AF_INET;
        target.addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &target.addr.sin_addr);
        targets.push_back(target);
    }

    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];

    // Broadcast helper
    auto broadcast_msg = [&](mavlink_message_t &msg) {
        uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
        for (const auto &target : targets) {
            sendto(sock, buffer, len, 0, (struct sockaddr *)&target.addr, sizeof(target.addr));
        }
    };

    // -------------------------------------------------------------
    // Step 1: Broadcast AUTO_SWARM Mode Command (Target System ID 0)
    // -------------------------------------------------------------
    std::cout << "--- Setting AUTO_SWARM Mode (Broadcast SysID 0) ---" << std::endl;

    mavlink_message_t cmd_msg;
    mavlink_msg_command_long_pack(
        INJECTOR_SYS_ID,
        INJECTOR_COMP_ID,
        &cmd_msg,
        0,                                 // target_system = 0 (Broadcast to all)
        MAV_COMP_ID_AUTOPILOT1,            // target_component
        MAV_CMD_DO_SET_MODE,               // command
        0,                                 // confirmation
        PX4_BASE_MODE_CUSTOM_ENABLED,      // param 1
        PX4_CUSTOM_MAIN_MODE_AUTO,         // param 2
        PX4_CUSTOM_SUB_MODE_SWARM,         // param 3
        0, 0, 0, 0
    );
    broadcast_msg(cmd_msg);
    std::cout << "-> Broadcasted MAV_CMD_DO_SET_MODE to system ID 0" << std::endl;

    usleep(500000); // 500ms delay

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
        2,  // no_of_nodes = 2
        2   // leader_id = 2 (Virtual Leader)
    );
    broadcast_msg(msg_mgmt);
    std::cout << "-> Broadcasted SWARM_MANAGEMENT (Type: 1, SwarmID: 1, Nodes: 2, Leader: 2)" << std::endl;

    usleep(100000); // 100ms delay

    // -------------------------------------------------------------
    // Step 3: Send SWARM_NODE Messages
    // -------------------------------------------------------------
    std::vector<NodeData> nodes = {
        {1, 1, -10.0f, -20.0f}, // Node 1 offset
        {1, 2,   0.0f,   0.0f}  // Node 2 offset (Leader)
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
        
        usleep(50000);
    }

    // -------------------------------------------------------------
    // Step 4: Simulate Virtual Node 2 Moving in a 200m Radius Circle
    // -------------------------------------------------------------
    std::cout << "\n--- Starting Position Streaming for Virtual Leader (SysID: 2) ---" << std::endl;
    std::cout << "Streaming LOCAL_POSITION_NED (Circle Radius = 200m) at 10 Hz... Press Ctrl+C to stop." << std::endl;

    float angle = 0.0f;
    uint32_t boot_time_ms = 0;

    while (true) {
        // Calculate Circular Trajectory
        float x = CIRCLE_RADIUS * std::cos(angle);
        float y = CIRCLE_RADIUS * std::sin(angle);
        float z = -20.0f; // 20m altitude (NED altitude is negative UP)

        // Calculate Velocity Vector (derivative of position)
        float vx = -CIRCLE_RADIUS * OMEGA * std::sin(angle);
        float vy =  CIRCLE_RADIUS * OMEGA * std::cos(angle);
        float vz =  0.0f;

        // Pack LOCAL_POSITION_NED with SysID = 2 (Virtual Node 2)
        mavlink_message_t pos_msg;
        mavlink_msg_local_position_ned_pack(
            VIRTUAL_SYS_ID,             // Sender System ID = 2
            MAV_COMP_ID_AUTOPILOT1,     // Sender Component ID
            &pos_msg,
            boot_time_ms,               // Time since boot (ms)
            x, y, z,                    // Position (m)
            vx, vy, vz                  // Velocity (m/s)
        );

        broadcast_msg(pos_msg);

        // Advance simulation time (10 Hz -> 100ms step)
        angle += OMEGA * 0.1f;
        if (angle >= 2.0f * M_PI) {
            angle -= 2.0f * M_PI;
        }

        boot_time_ms += 100;
        usleep(100000); // 100ms loop delay (10 Hz)
    }

    close(sock);
    return 0;
}
