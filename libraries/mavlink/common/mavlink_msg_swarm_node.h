#pragma once
// MESSAGE SWARM_NODE PACKING

#include <stdint.h>

#define MAVLINK_MSG_ID_SWARM_NODE 602


typedef struct __mavlink_swarm_node_t {
 float x; /*<  x from the leader*/
 float y; /*<  y from the leader*/
 uint8_t swarm_id; /*<  Type of the message*/
 uint8_t node_id; /*<  Number of nodes in the network*/
} mavlink_swarm_node_t;

#define MAVLINK_MSG_ID_SWARM_NODE_LEN 10
#define MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN 10
#define MAVLINK_MSG_ID_602_LEN 10
#define MAVLINK_MSG_ID_602_MIN_LEN 10

#define MAVLINK_MSG_ID_SWARM_NODE_CRC 151
#define MAVLINK_MSG_ID_602_CRC 151



#if MAVLINK_COMMAND_24BIT
#define MAVLINK_MESSAGE_INFO_SWARM_NODE { \
    602, \
    "SWARM_NODE", \
    4, \
    {  { "swarm_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 8, offsetof(mavlink_swarm_node_t, swarm_id) }, \
         { "node_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 9, offsetof(mavlink_swarm_node_t, node_id) }, \
         { "x", NULL, MAVLINK_TYPE_FLOAT, 0, 0, offsetof(mavlink_swarm_node_t, x) }, \
         { "y", NULL, MAVLINK_TYPE_FLOAT, 0, 4, offsetof(mavlink_swarm_node_t, y) }, \
         } \
}
#else
#define MAVLINK_MESSAGE_INFO_SWARM_NODE { \
    "SWARM_NODE", \
    4, \
    {  { "swarm_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 8, offsetof(mavlink_swarm_node_t, swarm_id) }, \
         { "node_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 9, offsetof(mavlink_swarm_node_t, node_id) }, \
         { "x", NULL, MAVLINK_TYPE_FLOAT, 0, 0, offsetof(mavlink_swarm_node_t, x) }, \
         { "y", NULL, MAVLINK_TYPE_FLOAT, 0, 4, offsetof(mavlink_swarm_node_t, y) }, \
         } \
}
#endif

/**
 * @brief Pack a swarm_node message
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 *
 * @param swarm_id  Type of the message
 * @param node_id  Number of nodes in the network
 * @param x  x from the leader
 * @param y  y from the leader
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_node_pack(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg,
                               uint8_t swarm_id, uint8_t node_id, float x, float y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_NODE_LEN];
    _mav_put_float(buf, 0, x);
    _mav_put_float(buf, 4, y);
    _mav_put_uint8_t(buf, 8, swarm_id);
    _mav_put_uint8_t(buf, 9, node_id);

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_NODE_LEN);
#else
    mavlink_swarm_node_t packet;
    packet.x = x;
    packet.y = y;
    packet.swarm_id = swarm_id;
    packet.node_id = node_id;

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_NODE_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_NODE;
    return mavlink_finalize_message(msg, system_id, component_id, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN, MAVLINK_MSG_ID_SWARM_NODE_CRC);
}

/**
 * @brief Pack a swarm_node message
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param status MAVLink status structure
 * @param msg The MAVLink message to compress the data into
 *
 * @param swarm_id  Type of the message
 * @param node_id  Number of nodes in the network
 * @param x  x from the leader
 * @param y  y from the leader
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_node_pack_status(uint8_t system_id, uint8_t component_id, mavlink_status_t *_status, mavlink_message_t* msg,
                               uint8_t swarm_id, uint8_t node_id, float x, float y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_NODE_LEN];
    _mav_put_float(buf, 0, x);
    _mav_put_float(buf, 4, y);
    _mav_put_uint8_t(buf, 8, swarm_id);
    _mav_put_uint8_t(buf, 9, node_id);

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_NODE_LEN);
#else
    mavlink_swarm_node_t packet;
    packet.x = x;
    packet.y = y;
    packet.swarm_id = swarm_id;
    packet.node_id = node_id;

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_NODE_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_NODE;
#if MAVLINK_CRC_EXTRA
    return mavlink_finalize_message_buffer(msg, system_id, component_id, _status, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN, MAVLINK_MSG_ID_SWARM_NODE_CRC);
#else
    return mavlink_finalize_message_buffer(msg, system_id, component_id, _status, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN);
#endif
}

/**
 * @brief Pack a swarm_node message on a channel
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param swarm_id  Type of the message
 * @param node_id  Number of nodes in the network
 * @param x  x from the leader
 * @param y  y from the leader
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_node_pack_chan(uint8_t system_id, uint8_t component_id, uint8_t chan,
                               mavlink_message_t* msg,
                                   uint8_t swarm_id,uint8_t node_id,float x,float y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_NODE_LEN];
    _mav_put_float(buf, 0, x);
    _mav_put_float(buf, 4, y);
    _mav_put_uint8_t(buf, 8, swarm_id);
    _mav_put_uint8_t(buf, 9, node_id);

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_NODE_LEN);
#else
    mavlink_swarm_node_t packet;
    packet.x = x;
    packet.y = y;
    packet.swarm_id = swarm_id;
    packet.node_id = node_id;

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_NODE_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_NODE;
    return mavlink_finalize_message_chan(msg, system_id, component_id, chan, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN, MAVLINK_MSG_ID_SWARM_NODE_CRC);
}

/**
 * @brief Encode a swarm_node struct
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 * @param swarm_node C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_node_encode(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg, const mavlink_swarm_node_t* swarm_node)
{
    return mavlink_msg_swarm_node_pack(system_id, component_id, msg, swarm_node->swarm_id, swarm_node->node_id, swarm_node->x, swarm_node->y);
}

/**
 * @brief Encode a swarm_node struct on a channel
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param swarm_node C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_node_encode_chan(uint8_t system_id, uint8_t component_id, uint8_t chan, mavlink_message_t* msg, const mavlink_swarm_node_t* swarm_node)
{
    return mavlink_msg_swarm_node_pack_chan(system_id, component_id, chan, msg, swarm_node->swarm_id, swarm_node->node_id, swarm_node->x, swarm_node->y);
}

/**
 * @brief Encode a swarm_node struct with provided status structure
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param status MAVLink status structure
 * @param msg The MAVLink message to compress the data into
 * @param swarm_node C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_node_encode_status(uint8_t system_id, uint8_t component_id, mavlink_status_t* _status, mavlink_message_t* msg, const mavlink_swarm_node_t* swarm_node)
{
    return mavlink_msg_swarm_node_pack_status(system_id, component_id, _status, msg,  swarm_node->swarm_id, swarm_node->node_id, swarm_node->x, swarm_node->y);
}

/**
 * @brief Send a swarm_node message
 * @param chan MAVLink channel to send the message
 *
 * @param swarm_id  Type of the message
 * @param node_id  Number of nodes in the network
 * @param x  x from the leader
 * @param y  y from the leader
 */
#ifdef MAVLINK_USE_CONVENIENCE_FUNCTIONS

static inline void mavlink_msg_swarm_node_send(mavlink_channel_t chan, uint8_t swarm_id, uint8_t node_id, float x, float y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_NODE_LEN];
    _mav_put_float(buf, 0, x);
    _mav_put_float(buf, 4, y);
    _mav_put_uint8_t(buf, 8, swarm_id);
    _mav_put_uint8_t(buf, 9, node_id);

    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_NODE, buf, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN, MAVLINK_MSG_ID_SWARM_NODE_CRC);
#else
    mavlink_swarm_node_t packet;
    packet.x = x;
    packet.y = y;
    packet.swarm_id = swarm_id;
    packet.node_id = node_id;

    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_NODE, (const char *)&packet, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN, MAVLINK_MSG_ID_SWARM_NODE_CRC);
#endif
}

/**
 * @brief Send a swarm_node message
 * @param chan MAVLink channel to send the message
 * @param struct The MAVLink struct to serialize
 */
static inline void mavlink_msg_swarm_node_send_struct(mavlink_channel_t chan, const mavlink_swarm_node_t* swarm_node)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    mavlink_msg_swarm_node_send(chan, swarm_node->swarm_id, swarm_node->node_id, swarm_node->x, swarm_node->y);
#else
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_NODE, (const char *)swarm_node, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN, MAVLINK_MSG_ID_SWARM_NODE_CRC);
#endif
}

#if MAVLINK_MSG_ID_SWARM_NODE_LEN <= MAVLINK_MAX_PAYLOAD_LEN
/*
  This variant of _send() can be used to save stack space by reusing
  memory from the receive buffer.  The caller provides a
  mavlink_message_t which is the size of a full mavlink message. This
  is usually the receive buffer for the channel, and allows a reply to an
  incoming message with minimum stack space usage.
 */
static inline void mavlink_msg_swarm_node_send_buf(mavlink_message_t *msgbuf, mavlink_channel_t chan,  uint8_t swarm_id, uint8_t node_id, float x, float y)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char *buf = (char *)msgbuf;
    _mav_put_float(buf, 0, x);
    _mav_put_float(buf, 4, y);
    _mav_put_uint8_t(buf, 8, swarm_id);
    _mav_put_uint8_t(buf, 9, node_id);

    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_NODE, buf, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN, MAVLINK_MSG_ID_SWARM_NODE_CRC);
#else
    mavlink_swarm_node_t *packet = (mavlink_swarm_node_t *)msgbuf;
    packet->x = x;
    packet->y = y;
    packet->swarm_id = swarm_id;
    packet->node_id = node_id;

    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_NODE, (const char *)packet, MAVLINK_MSG_ID_SWARM_NODE_MIN_LEN, MAVLINK_MSG_ID_SWARM_NODE_LEN, MAVLINK_MSG_ID_SWARM_NODE_CRC);
#endif
}
#endif

#endif

// MESSAGE SWARM_NODE UNPACKING


/**
 * @brief Get field swarm_id from swarm_node message
 *
 * @return  Type of the message
 */
static inline uint8_t mavlink_msg_swarm_node_get_swarm_id(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  8);
}

/**
 * @brief Get field node_id from swarm_node message
 *
 * @return  Number of nodes in the network
 */
static inline uint8_t mavlink_msg_swarm_node_get_node_id(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  9);
}

/**
 * @brief Get field x from swarm_node message
 *
 * @return  x from the leader
 */
static inline float mavlink_msg_swarm_node_get_x(const mavlink_message_t* msg)
{
    return _MAV_RETURN_float(msg,  0);
}

/**
 * @brief Get field y from swarm_node message
 *
 * @return  y from the leader
 */
static inline float mavlink_msg_swarm_node_get_y(const mavlink_message_t* msg)
{
    return _MAV_RETURN_float(msg,  4);
}

/**
 * @brief Decode a swarm_node message into a struct
 *
 * @param msg The message to decode
 * @param swarm_node C-struct to decode the message contents into
 */
static inline void mavlink_msg_swarm_node_decode(const mavlink_message_t* msg, mavlink_swarm_node_t* swarm_node)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    swarm_node->x = mavlink_msg_swarm_node_get_x(msg);
    swarm_node->y = mavlink_msg_swarm_node_get_y(msg);
    swarm_node->swarm_id = mavlink_msg_swarm_node_get_swarm_id(msg);
    swarm_node->node_id = mavlink_msg_swarm_node_get_node_id(msg);
#else
        uint8_t len = msg->len < MAVLINK_MSG_ID_SWARM_NODE_LEN? msg->len : MAVLINK_MSG_ID_SWARM_NODE_LEN;
        memset(swarm_node, 0, MAVLINK_MSG_ID_SWARM_NODE_LEN);
    memcpy(swarm_node, _MAV_PAYLOAD(msg), len);
#endif
}
