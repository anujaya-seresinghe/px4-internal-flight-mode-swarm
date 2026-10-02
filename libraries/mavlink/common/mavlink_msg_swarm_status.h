#pragma once
// MESSAGE SWARM_STATUS PACKING

#include <stdint.h>

#define MAVLINK_MSG_ID_SWARM_STATUS 603


typedef struct __mavlink_swarm_status_t {
 float x[20]; /*<  x offset of each node (as sent in SWARM_NODE)*/
 float y[20]; /*<  y offset of each node (as sent in SWARM_NODE)*/
 uint8_t state; /*<  0 = not in an active Swarm mode, 1 = in Swarm mode without a swarm, 2 = collecting SWARM_NODEs, 3 = flying the formation, 4 = formation update pending*/
 uint8_t swarm_id; /*<  ID of the swarm network (0 = none)*/
 uint8_t leader_id; /*<  ID of the swarm leader*/
 uint8_t no_of_nodes; /*<  Number of nodes in the network*/
 uint8_t node_count; /*<  Number of valid entries in node_ids, x and y*/
 uint8_t node_ids[20]; /*<  IDs of the nodes of the current formation*/
} mavlink_swarm_status_t;

#define MAVLINK_MSG_ID_SWARM_STATUS_LEN 185
#define MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN 185
#define MAVLINK_MSG_ID_603_LEN 185
#define MAVLINK_MSG_ID_603_MIN_LEN 185

#define MAVLINK_MSG_ID_SWARM_STATUS_CRC 218
#define MAVLINK_MSG_ID_603_CRC 218

#define MAVLINK_MSG_SWARM_STATUS_FIELD_X_LEN 20
#define MAVLINK_MSG_SWARM_STATUS_FIELD_Y_LEN 20
#define MAVLINK_MSG_SWARM_STATUS_FIELD_NODE_IDS_LEN 20

#if MAVLINK_COMMAND_24BIT
#define MAVLINK_MESSAGE_INFO_SWARM_STATUS { \
    603, \
    "SWARM_STATUS", \
    8, \
    {  { "state", NULL, MAVLINK_TYPE_UINT8_T, 0, 160, offsetof(mavlink_swarm_status_t, state) }, \
         { "swarm_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 161, offsetof(mavlink_swarm_status_t, swarm_id) }, \
         { "leader_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 162, offsetof(mavlink_swarm_status_t, leader_id) }, \
         { "no_of_nodes", NULL, MAVLINK_TYPE_UINT8_T, 0, 163, offsetof(mavlink_swarm_status_t, no_of_nodes) }, \
         { "node_count", NULL, MAVLINK_TYPE_UINT8_T, 0, 164, offsetof(mavlink_swarm_status_t, node_count) }, \
         { "node_ids", NULL, MAVLINK_TYPE_UINT8_T, 20, 165, offsetof(mavlink_swarm_status_t, node_ids) }, \
         { "x", NULL, MAVLINK_TYPE_FLOAT, 20, 0, offsetof(mavlink_swarm_status_t, x) }, \
         { "y", NULL, MAVLINK_TYPE_FLOAT, 20, 80, offsetof(mavlink_swarm_status_t, y) }, \
         } \
}
#else
#define MAVLINK_MESSAGE_INFO_SWARM_STATUS { \
    "SWARM_STATUS", \
    8, \
    {  { "state", NULL, MAVLINK_TYPE_UINT8_T, 0, 160, offsetof(mavlink_swarm_status_t, state) }, \
         { "swarm_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 161, offsetof(mavlink_swarm_status_t, swarm_id) }, \
         { "leader_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 162, offsetof(mavlink_swarm_status_t, leader_id) }, \
         { "no_of_nodes", NULL, MAVLINK_TYPE_UINT8_T, 0, 163, offsetof(mavlink_swarm_status_t, no_of_nodes) }, \
         { "node_count", NULL, MAVLINK_TYPE_UINT8_T, 0, 164, offsetof(mavlink_swarm_status_t, node_count) }, \
         { "node_ids", NULL, MAVLINK_TYPE_UINT8_T, 20, 165, offsetof(mavlink_swarm_status_t, node_ids) }, \
         { "x", NULL, MAVLINK_TYPE_FLOAT, 20, 0, offsetof(mavlink_swarm_status_t, x) }, \
         { "y", NULL, MAVLINK_TYPE_FLOAT, 20, 80, offsetof(mavlink_swarm_status_t, y) }, \
         } \
}
#endif

/**
 * @brief Pack a swarm_status message
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 *
 * @param state  0 = not in an active Swarm mode, 1 = in Swarm mode without a swarm, 2 = collecting SWARM_NODEs, 3 = flying the formation, 4 = formation update pending
 * @param swarm_id  ID of the swarm network (0 = none)
 * @param leader_id  ID of the swarm leader
 * @param no_of_nodes  Number of nodes in the network
 * @param node_count  Number of valid entries in node_ids, x and y
 * @param node_ids  IDs of the nodes of the current formation
 * @param x  x offset of each node (as sent in SWARM_NODE)
 * @param y  y offset of each node (as sent in SWARM_NODE)
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_status_pack(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg,
                               uint8_t state, uint8_t swarm_id, uint8_t leader_id, uint8_t no_of_nodes, uint8_t node_count, const uint8_t *node_ids, const float *x, const float *y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_STATUS_LEN];
    _mav_put_uint8_t(buf, 160, state);
    _mav_put_uint8_t(buf, 161, swarm_id);
    _mav_put_uint8_t(buf, 162, leader_id);
    _mav_put_uint8_t(buf, 163, no_of_nodes);
    _mav_put_uint8_t(buf, 164, node_count);
    _mav_put_float_array(buf, 0, x, 20);
    _mav_put_float_array(buf, 80, y, 20);
    _mav_put_uint8_t_array(buf, 165, node_ids, 20);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_STATUS_LEN);
#else
    mavlink_swarm_status_t packet;
    packet.state = state;
    packet.swarm_id = swarm_id;
    packet.leader_id = leader_id;
    packet.no_of_nodes = no_of_nodes;
    packet.node_count = node_count;
    mav_array_memcpy(packet.x, x, sizeof(float)*20);
    mav_array_memcpy(packet.y, y, sizeof(float)*20);
    mav_array_memcpy(packet.node_ids, node_ids, sizeof(uint8_t)*20);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_STATUS_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_STATUS;
    return mavlink_finalize_message(msg, system_id, component_id, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN, MAVLINK_MSG_ID_SWARM_STATUS_CRC);
}

/**
 * @brief Pack a swarm_status message
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param status MAVLink status structure
 * @param msg The MAVLink message to compress the data into
 *
 * @param state  0 = not in an active Swarm mode, 1 = in Swarm mode without a swarm, 2 = collecting SWARM_NODEs, 3 = flying the formation, 4 = formation update pending
 * @param swarm_id  ID of the swarm network (0 = none)
 * @param leader_id  ID of the swarm leader
 * @param no_of_nodes  Number of nodes in the network
 * @param node_count  Number of valid entries in node_ids, x and y
 * @param node_ids  IDs of the nodes of the current formation
 * @param x  x offset of each node (as sent in SWARM_NODE)
 * @param y  y offset of each node (as sent in SWARM_NODE)
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_status_pack_status(uint8_t system_id, uint8_t component_id, mavlink_status_t *_status, mavlink_message_t* msg,
                               uint8_t state, uint8_t swarm_id, uint8_t leader_id, uint8_t no_of_nodes, uint8_t node_count, const uint8_t *node_ids, const float *x, const float *y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_STATUS_LEN];
    _mav_put_uint8_t(buf, 160, state);
    _mav_put_uint8_t(buf, 161, swarm_id);
    _mav_put_uint8_t(buf, 162, leader_id);
    _mav_put_uint8_t(buf, 163, no_of_nodes);
    _mav_put_uint8_t(buf, 164, node_count);
    _mav_put_float_array(buf, 0, x, 20);
    _mav_put_float_array(buf, 80, y, 20);
    _mav_put_uint8_t_array(buf, 165, node_ids, 20);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_STATUS_LEN);
#else
    mavlink_swarm_status_t packet;
    packet.state = state;
    packet.swarm_id = swarm_id;
    packet.leader_id = leader_id;
    packet.no_of_nodes = no_of_nodes;
    packet.node_count = node_count;
    mav_array_memcpy(packet.x, x, sizeof(float)*20);
    mav_array_memcpy(packet.y, y, sizeof(float)*20);
    mav_array_memcpy(packet.node_ids, node_ids, sizeof(uint8_t)*20);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_STATUS_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_STATUS;
#if MAVLINK_CRC_EXTRA
    return mavlink_finalize_message_buffer(msg, system_id, component_id, _status, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN, MAVLINK_MSG_ID_SWARM_STATUS_CRC);
#else
    return mavlink_finalize_message_buffer(msg, system_id, component_id, _status, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN);
#endif
}

/**
 * @brief Pack a swarm_status message on a channel
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param state  0 = not in an active Swarm mode, 1 = in Swarm mode without a swarm, 2 = collecting SWARM_NODEs, 3 = flying the formation, 4 = formation update pending
 * @param swarm_id  ID of the swarm network (0 = none)
 * @param leader_id  ID of the swarm leader
 * @param no_of_nodes  Number of nodes in the network
 * @param node_count  Number of valid entries in node_ids, x and y
 * @param node_ids  IDs of the nodes of the current formation
 * @param x  x offset of each node (as sent in SWARM_NODE)
 * @param y  y offset of each node (as sent in SWARM_NODE)
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_status_pack_chan(uint8_t system_id, uint8_t component_id, uint8_t chan,
                               mavlink_message_t* msg,
                                   uint8_t state,uint8_t swarm_id,uint8_t leader_id,uint8_t no_of_nodes,uint8_t node_count,const uint8_t *node_ids,const float *x,const float *y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_STATUS_LEN];
    _mav_put_uint8_t(buf, 160, state);
    _mav_put_uint8_t(buf, 161, swarm_id);
    _mav_put_uint8_t(buf, 162, leader_id);
    _mav_put_uint8_t(buf, 163, no_of_nodes);
    _mav_put_uint8_t(buf, 164, node_count);
    _mav_put_float_array(buf, 0, x, 20);
    _mav_put_float_array(buf, 80, y, 20);
    _mav_put_uint8_t_array(buf, 165, node_ids, 20);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_STATUS_LEN);
#else
    mavlink_swarm_status_t packet;
    packet.state = state;
    packet.swarm_id = swarm_id;
    packet.leader_id = leader_id;
    packet.no_of_nodes = no_of_nodes;
    packet.node_count = node_count;
    mav_array_memcpy(packet.x, x, sizeof(float)*20);
    mav_array_memcpy(packet.y, y, sizeof(float)*20);
    mav_array_memcpy(packet.node_ids, node_ids, sizeof(uint8_t)*20);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_STATUS_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_STATUS;
    return mavlink_finalize_message_chan(msg, system_id, component_id, chan, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN, MAVLINK_MSG_ID_SWARM_STATUS_CRC);
}

/**
 * @brief Encode a swarm_status struct
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 * @param swarm_status C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_status_encode(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg, const mavlink_swarm_status_t* swarm_status)
{
    return mavlink_msg_swarm_status_pack(system_id, component_id, msg, swarm_status->state, swarm_status->swarm_id, swarm_status->leader_id, swarm_status->no_of_nodes, swarm_status->node_count, swarm_status->node_ids, swarm_status->x, swarm_status->y);
}

/**
 * @brief Encode a swarm_status struct on a channel
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param swarm_status C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_status_encode_chan(uint8_t system_id, uint8_t component_id, uint8_t chan, mavlink_message_t* msg, const mavlink_swarm_status_t* swarm_status)
{
    return mavlink_msg_swarm_status_pack_chan(system_id, component_id, chan, msg, swarm_status->state, swarm_status->swarm_id, swarm_status->leader_id, swarm_status->no_of_nodes, swarm_status->node_count, swarm_status->node_ids, swarm_status->x, swarm_status->y);
}

/**
 * @brief Encode a swarm_status struct with provided status structure
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param status MAVLink status structure
 * @param msg The MAVLink message to compress the data into
 * @param swarm_status C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_status_encode_status(uint8_t system_id, uint8_t component_id, mavlink_status_t* _status, mavlink_message_t* msg, const mavlink_swarm_status_t* swarm_status)
{
    return mavlink_msg_swarm_status_pack_status(system_id, component_id, _status, msg,  swarm_status->state, swarm_status->swarm_id, swarm_status->leader_id, swarm_status->no_of_nodes, swarm_status->node_count, swarm_status->node_ids, swarm_status->x, swarm_status->y);
}

/**
 * @brief Send a swarm_status message
 * @param chan MAVLink channel to send the message
 *
 * @param state  0 = not in an active Swarm mode, 1 = in Swarm mode without a swarm, 2 = collecting SWARM_NODEs, 3 = flying the formation, 4 = formation update pending
 * @param swarm_id  ID of the swarm network (0 = none)
 * @param leader_id  ID of the swarm leader
 * @param no_of_nodes  Number of nodes in the network
 * @param node_count  Number of valid entries in node_ids, x and y
 * @param node_ids  IDs of the nodes of the current formation
 * @param x  x offset of each node (as sent in SWARM_NODE)
 * @param y  y offset of each node (as sent in SWARM_NODE)
 */
#ifdef MAVLINK_USE_CONVENIENCE_FUNCTIONS

static inline void mavlink_msg_swarm_status_send(mavlink_channel_t chan, uint8_t state, uint8_t swarm_id, uint8_t leader_id, uint8_t no_of_nodes, uint8_t node_count, const uint8_t *node_ids, const float *x, const float *y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_STATUS_LEN];
    _mav_put_uint8_t(buf, 160, state);
    _mav_put_uint8_t(buf, 161, swarm_id);
    _mav_put_uint8_t(buf, 162, leader_id);
    _mav_put_uint8_t(buf, 163, no_of_nodes);
    _mav_put_uint8_t(buf, 164, node_count);
    _mav_put_float_array(buf, 0, x, 20);
    _mav_put_float_array(buf, 80, y, 20);
    _mav_put_uint8_t_array(buf, 165, node_ids, 20);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_STATUS, buf, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN, MAVLINK_MSG_ID_SWARM_STATUS_CRC);
#else
    mavlink_swarm_status_t packet;
    packet.state = state;
    packet.swarm_id = swarm_id;
    packet.leader_id = leader_id;
    packet.no_of_nodes = no_of_nodes;
    packet.node_count = node_count;
    mav_array_memcpy(packet.x, x, sizeof(float)*20);
    mav_array_memcpy(packet.y, y, sizeof(float)*20);
    mav_array_memcpy(packet.node_ids, node_ids, sizeof(uint8_t)*20);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_STATUS, (const char *)&packet, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN, MAVLINK_MSG_ID_SWARM_STATUS_CRC);
#endif
}

/**
 * @brief Send a swarm_status message
 * @param chan MAVLink channel to send the message
 * @param struct The MAVLink struct to serialize
 */
static inline void mavlink_msg_swarm_status_send_struct(mavlink_channel_t chan, const mavlink_swarm_status_t* swarm_status)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    mavlink_msg_swarm_status_send(chan, swarm_status->state, swarm_status->swarm_id, swarm_status->leader_id, swarm_status->no_of_nodes, swarm_status->node_count, swarm_status->node_ids, swarm_status->x, swarm_status->y);
#else
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_STATUS, (const char *)swarm_status, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN, MAVLINK_MSG_ID_SWARM_STATUS_CRC);
#endif
}

#if MAVLINK_MSG_ID_SWARM_STATUS_LEN <= MAVLINK_MAX_PAYLOAD_LEN
/*
  This variant of _send() can be used to save stack space by reusing
  memory from the receive buffer.  The caller provides a
  mavlink_message_t which is the size of a full mavlink message. This
  is usually the receive buffer for the channel, and allows a reply to an
  incoming message with minimum stack space usage.
 */
static inline void mavlink_msg_swarm_status_send_buf(mavlink_message_t *msgbuf, mavlink_channel_t chan,  uint8_t state, uint8_t swarm_id, uint8_t leader_id, uint8_t no_of_nodes, uint8_t node_count, const uint8_t *node_ids, const float *x, const float *y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char *buf = (char *)msgbuf;
    _mav_put_uint8_t(buf, 160, state);
    _mav_put_uint8_t(buf, 161, swarm_id);
    _mav_put_uint8_t(buf, 162, leader_id);
    _mav_put_uint8_t(buf, 163, no_of_nodes);
    _mav_put_uint8_t(buf, 164, node_count);
    _mav_put_float_array(buf, 0, x, 20);
    _mav_put_float_array(buf, 80, y, 20);
    _mav_put_uint8_t_array(buf, 165, node_ids, 20);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_STATUS, buf, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN, MAVLINK_MSG_ID_SWARM_STATUS_CRC);
#else
    mavlink_swarm_status_t *packet = (mavlink_swarm_status_t *)msgbuf;
    packet->state = state;
    packet->swarm_id = swarm_id;
    packet->leader_id = leader_id;
    packet->no_of_nodes = no_of_nodes;
    packet->node_count = node_count;
    mav_array_memcpy(packet->x, x, sizeof(float)*20);
    mav_array_memcpy(packet->y, y, sizeof(float)*20);
    mav_array_memcpy(packet->node_ids, node_ids, sizeof(uint8_t)*20);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_STATUS, (const char *)packet, MAVLINK_MSG_ID_SWARM_STATUS_MIN_LEN, MAVLINK_MSG_ID_SWARM_STATUS_LEN, MAVLINK_MSG_ID_SWARM_STATUS_CRC);
#endif
}
#endif

#endif

// MESSAGE SWARM_STATUS UNPACKING


/**
 * @brief Get field state from swarm_status message
 *
 * @return  0 = not in an active Swarm mode, 1 = in Swarm mode without a swarm, 2 = collecting SWARM_NODEs, 3 = flying the formation, 4 = formation update pending
 */
static inline uint8_t mavlink_msg_swarm_status_get_state(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  160);
}

/**
 * @brief Get field swarm_id from swarm_status message
 *
 * @return  ID of the swarm network (0 = none)
 */
static inline uint8_t mavlink_msg_swarm_status_get_swarm_id(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  161);
}

/**
 * @brief Get field leader_id from swarm_status message
 *
 * @return  ID of the swarm leader
 */
static inline uint8_t mavlink_msg_swarm_status_get_leader_id(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  162);
}

/**
 * @brief Get field no_of_nodes from swarm_status message
 *
 * @return  Number of nodes in the network
 */
static inline uint8_t mavlink_msg_swarm_status_get_no_of_nodes(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  163);
}

/**
 * @brief Get field node_count from swarm_status message
 *
 * @return  Number of valid entries in node_ids, x and y
 */
static inline uint8_t mavlink_msg_swarm_status_get_node_count(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  164);
}

/**
 * @brief Get field node_ids from swarm_status message
 *
 * @return  IDs of the nodes of the current formation
 */
static inline uint16_t mavlink_msg_swarm_status_get_node_ids(const mavlink_message_t* msg, uint8_t *node_ids)
{
    return _MAV_RETURN_uint8_t_array(msg, node_ids, 20,  165);
}

/**
 * @brief Get field x from swarm_status message
 *
 * @return  x offset of each node (as sent in SWARM_NODE)
 */
static inline uint16_t mavlink_msg_swarm_status_get_x(const mavlink_message_t* msg, float *x)
{
    return _MAV_RETURN_float_array(msg, x, 20,  0);
}

/**
 * @brief Get field y from swarm_status message
 *
 * @return  y offset of each node (as sent in SWARM_NODE)
 */
static inline uint16_t mavlink_msg_swarm_status_get_y(const mavlink_message_t* msg, float *y)
{
    return _MAV_RETURN_float_array(msg, y, 20,  80);
}

/**
 * @brief Decode a swarm_status message into a struct
 *
 * @param msg The message to decode
 * @param swarm_status C-struct to decode the message contents into
 */
static inline void mavlink_msg_swarm_status_decode(const mavlink_message_t* msg, mavlink_swarm_status_t* swarm_status)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    mavlink_msg_swarm_status_get_x(msg, swarm_status->x);
    mavlink_msg_swarm_status_get_y(msg, swarm_status->y);
    swarm_status->state = mavlink_msg_swarm_status_get_state(msg);
    swarm_status->swarm_id = mavlink_msg_swarm_status_get_swarm_id(msg);
    swarm_status->leader_id = mavlink_msg_swarm_status_get_leader_id(msg);
    swarm_status->no_of_nodes = mavlink_msg_swarm_status_get_no_of_nodes(msg);
    swarm_status->node_count = mavlink_msg_swarm_status_get_node_count(msg);
    mavlink_msg_swarm_status_get_node_ids(msg, swarm_status->node_ids);
#else
        uint8_t len = msg->len < MAVLINK_MSG_ID_SWARM_STATUS_LEN? msg->len : MAVLINK_MSG_ID_SWARM_STATUS_LEN;
        memset(swarm_status, 0, MAVLINK_MSG_ID_SWARM_STATUS_LEN);
    memcpy(swarm_status, _MAV_PAYLOAD(msg), len);
#endif
}
