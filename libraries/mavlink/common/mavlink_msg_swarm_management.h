#pragma once
// MESSAGE SWARM_MANAGEMENT PACKING

#include <stdint.h>

#define MAVLINK_MSG_ID_SWARM_MANAGEMENT 601


typedef struct __mavlink_swarm_management_t {
 uint8_t type; /*<  Type of the message*/
 uint8_t swarm_id; /*<  ID of the swarm network*/
 uint8_t no_of_nodes; /*<  Number of nodes in the network*/
 uint8_t leader_id; /*<  ID of the swarm leader*/
} mavlink_swarm_management_t;

#define MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN 4
#define MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN 4
#define MAVLINK_MSG_ID_601_LEN 4
#define MAVLINK_MSG_ID_601_MIN_LEN 4

#define MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC 224
#define MAVLINK_MSG_ID_601_CRC 224



#if MAVLINK_COMMAND_24BIT
#define MAVLINK_MESSAGE_INFO_SWARM_MANAGEMENT { \
    601, \
    "SWARM_MANAGEMENT", \
    4, \
    {  { "type", NULL, MAVLINK_TYPE_UINT8_T, 0, 0, offsetof(mavlink_swarm_management_t, type) }, \
         { "swarm_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 1, offsetof(mavlink_swarm_management_t, swarm_id) }, \
         { "no_of_nodes", NULL, MAVLINK_TYPE_UINT8_T, 0, 2, offsetof(mavlink_swarm_management_t, no_of_nodes) }, \
         { "leader_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 3, offsetof(mavlink_swarm_management_t, leader_id) }, \
         } \
}
#else
#define MAVLINK_MESSAGE_INFO_SWARM_MANAGEMENT { \
    "SWARM_MANAGEMENT", \
    4, \
    {  { "type", NULL, MAVLINK_TYPE_UINT8_T, 0, 0, offsetof(mavlink_swarm_management_t, type) }, \
         { "swarm_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 1, offsetof(mavlink_swarm_management_t, swarm_id) }, \
         { "no_of_nodes", NULL, MAVLINK_TYPE_UINT8_T, 0, 2, offsetof(mavlink_swarm_management_t, no_of_nodes) }, \
         { "leader_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 3, offsetof(mavlink_swarm_management_t, leader_id) }, \
         } \
}
#endif

/**
 * @brief Pack a swarm_management message
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 *
 * @param type  Type of the message
 * @param swarm_id  ID of the swarm network
 * @param no_of_nodes  Number of nodes in the network
 * @param leader_id  ID of the swarm leader
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_management_pack(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg,
                               uint8_t type, uint8_t swarm_id, uint8_t no_of_nodes, uint8_t leader_id)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN];
    _mav_put_uint8_t(buf, 0, type);
    _mav_put_uint8_t(buf, 1, swarm_id);
    _mav_put_uint8_t(buf, 2, no_of_nodes);
    _mav_put_uint8_t(buf, 3, leader_id);

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN);
#else
    mavlink_swarm_management_t packet;
    packet.type = type;
    packet.swarm_id = swarm_id;
    packet.no_of_nodes = no_of_nodes;
    packet.leader_id = leader_id;

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_MANAGEMENT;
    return mavlink_finalize_message(msg, system_id, component_id, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC);
}

/**
 * @brief Pack a swarm_management message
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param status MAVLink status structure
 * @param msg The MAVLink message to compress the data into
 *
 * @param type  Type of the message
 * @param swarm_id  ID of the swarm network
 * @param no_of_nodes  Number of nodes in the network
 * @param leader_id  ID of the swarm leader
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_management_pack_status(uint8_t system_id, uint8_t component_id, mavlink_status_t *_status, mavlink_message_t* msg,
                               uint8_t type, uint8_t swarm_id, uint8_t no_of_nodes, uint8_t leader_id)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN];
    _mav_put_uint8_t(buf, 0, type);
    _mav_put_uint8_t(buf, 1, swarm_id);
    _mav_put_uint8_t(buf, 2, no_of_nodes);
    _mav_put_uint8_t(buf, 3, leader_id);

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN);
#else
    mavlink_swarm_management_t packet;
    packet.type = type;
    packet.swarm_id = swarm_id;
    packet.no_of_nodes = no_of_nodes;
    packet.leader_id = leader_id;

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_MANAGEMENT;
#if MAVLINK_CRC_EXTRA
    return mavlink_finalize_message_buffer(msg, system_id, component_id, _status, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC);
#else
    return mavlink_finalize_message_buffer(msg, system_id, component_id, _status, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN);
#endif
}

/**
 * @brief Pack a swarm_management message on a channel
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param type  Type of the message
 * @param swarm_id  ID of the swarm network
 * @param no_of_nodes  Number of nodes in the network
 * @param leader_id  ID of the swarm leader
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_swarm_management_pack_chan(uint8_t system_id, uint8_t component_id, uint8_t chan,
                               mavlink_message_t* msg,
                                   uint8_t type,uint8_t swarm_id,uint8_t no_of_nodes,uint8_t leader_id)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN];
    _mav_put_uint8_t(buf, 0, type);
    _mav_put_uint8_t(buf, 1, swarm_id);
    _mav_put_uint8_t(buf, 2, no_of_nodes);
    _mav_put_uint8_t(buf, 3, leader_id);

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN);
#else
    mavlink_swarm_management_t packet;
    packet.type = type;
    packet.swarm_id = swarm_id;
    packet.no_of_nodes = no_of_nodes;
    packet.leader_id = leader_id;

        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SWARM_MANAGEMENT;
    return mavlink_finalize_message_chan(msg, system_id, component_id, chan, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC);
}

/**
 * @brief Encode a swarm_management struct
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 * @param swarm_management C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_management_encode(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg, const mavlink_swarm_management_t* swarm_management)
{
    return mavlink_msg_swarm_management_pack(system_id, component_id, msg, swarm_management->type, swarm_management->swarm_id, swarm_management->no_of_nodes, swarm_management->leader_id);
}

/**
 * @brief Encode a swarm_management struct on a channel
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param swarm_management C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_management_encode_chan(uint8_t system_id, uint8_t component_id, uint8_t chan, mavlink_message_t* msg, const mavlink_swarm_management_t* swarm_management)
{
    return mavlink_msg_swarm_management_pack_chan(system_id, component_id, chan, msg, swarm_management->type, swarm_management->swarm_id, swarm_management->no_of_nodes, swarm_management->leader_id);
}

/**
 * @brief Encode a swarm_management struct with provided status structure
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param status MAVLink status structure
 * @param msg The MAVLink message to compress the data into
 * @param swarm_management C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_swarm_management_encode_status(uint8_t system_id, uint8_t component_id, mavlink_status_t* _status, mavlink_message_t* msg, const mavlink_swarm_management_t* swarm_management)
{
    return mavlink_msg_swarm_management_pack_status(system_id, component_id, _status, msg,  swarm_management->type, swarm_management->swarm_id, swarm_management->no_of_nodes, swarm_management->leader_id);
}

/**
 * @brief Send a swarm_management message
 * @param chan MAVLink channel to send the message
 *
 * @param type  Type of the message
 * @param swarm_id  ID of the swarm network
 * @param no_of_nodes  Number of nodes in the network
 * @param leader_id  ID of the swarm leader
 */
#ifdef MAVLINK_USE_CONVENIENCE_FUNCTIONS

static inline void mavlink_msg_swarm_management_send(mavlink_channel_t chan, uint8_t type, uint8_t swarm_id, uint8_t no_of_nodes, uint8_t leader_id)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN];
    _mav_put_uint8_t(buf, 0, type);
    _mav_put_uint8_t(buf, 1, swarm_id);
    _mav_put_uint8_t(buf, 2, no_of_nodes);
    _mav_put_uint8_t(buf, 3, leader_id);

    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_MANAGEMENT, buf, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC);
#else
    mavlink_swarm_management_t packet;
    packet.type = type;
    packet.swarm_id = swarm_id;
    packet.no_of_nodes = no_of_nodes;
    packet.leader_id = leader_id;

    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_MANAGEMENT, (const char *)&packet, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC);
#endif
}

/**
 * @brief Send a swarm_management message
 * @param chan MAVLink channel to send the message
 * @param struct The MAVLink struct to serialize
 */
static inline void mavlink_msg_swarm_management_send_struct(mavlink_channel_t chan, const mavlink_swarm_management_t* swarm_management)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    mavlink_msg_swarm_management_send(chan, swarm_management->type, swarm_management->swarm_id, swarm_management->no_of_nodes, swarm_management->leader_id);
#else
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_MANAGEMENT, (const char *)swarm_management, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC);
#endif
}

#if MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN <= MAVLINK_MAX_PAYLOAD_LEN
/*
  This variant of _send() can be used to save stack space by reusing
  memory from the receive buffer.  The caller provides a
  mavlink_message_t which is the size of a full mavlink message. This
  is usually the receive buffer for the channel, and allows a reply to an
  incoming message with minimum stack space usage.
 */
static inline void mavlink_msg_swarm_management_send_buf(mavlink_message_t *msgbuf, mavlink_channel_t chan,  uint8_t type, uint8_t swarm_id, uint8_t no_of_nodes, uint8_t leader_id)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char *buf = (char *)msgbuf;
    _mav_put_uint8_t(buf, 0, type);
    _mav_put_uint8_t(buf, 1, swarm_id);
    _mav_put_uint8_t(buf, 2, no_of_nodes);
    _mav_put_uint8_t(buf, 3, leader_id);

    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_MANAGEMENT, buf, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC);
#else
    mavlink_swarm_management_t *packet = (mavlink_swarm_management_t *)msgbuf;
    packet->type = type;
    packet->swarm_id = swarm_id;
    packet->no_of_nodes = no_of_nodes;
    packet->leader_id = leader_id;

    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SWARM_MANAGEMENT, (const char *)packet, MAVLINK_MSG_ID_SWARM_MANAGEMENT_MIN_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN, MAVLINK_MSG_ID_SWARM_MANAGEMENT_CRC);
#endif
}
#endif

#endif

// MESSAGE SWARM_MANAGEMENT UNPACKING


/**
 * @brief Get field type from swarm_management message
 *
 * @return  Type of the message
 */
static inline uint8_t mavlink_msg_swarm_management_get_type(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  0);
}

/**
 * @brief Get field swarm_id from swarm_management message
 *
 * @return  ID of the swarm network
 */
static inline uint8_t mavlink_msg_swarm_management_get_swarm_id(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  1);
}

/**
 * @brief Get field no_of_nodes from swarm_management message
 *
 * @return  Number of nodes in the network
 */
static inline uint8_t mavlink_msg_swarm_management_get_no_of_nodes(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  2);
}

/**
 * @brief Get field leader_id from swarm_management message
 *
 * @return  ID of the swarm leader
 */
static inline uint8_t mavlink_msg_swarm_management_get_leader_id(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  3);
}

/**
 * @brief Decode a swarm_management message into a struct
 *
 * @param msg The message to decode
 * @param swarm_management C-struct to decode the message contents into
 */
static inline void mavlink_msg_swarm_management_decode(const mavlink_message_t* msg, mavlink_swarm_management_t* swarm_management)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    swarm_management->type = mavlink_msg_swarm_management_get_type(msg);
    swarm_management->swarm_id = mavlink_msg_swarm_management_get_swarm_id(msg);
    swarm_management->no_of_nodes = mavlink_msg_swarm_management_get_no_of_nodes(msg);
    swarm_management->leader_id = mavlink_msg_swarm_management_get_leader_id(msg);
#else
        uint8_t len = msg->len < MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN? msg->len : MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN;
        memset(swarm_management, 0, MAVLINK_MSG_ID_SWARM_MANAGEMENT_LEN);
    memcpy(swarm_management, _MAV_PAYLOAD(msg), len);
#endif
}
