/****************************************************************************
 * Non-blocking MAVLink-over-UDP endpoint for the swarm companion node.
 *
 * Receives SWARM_MANAGEMENT / SWARM_NODE from the GCS bridge and the neighbours'
 * LOCAL_POSITION_NED / ATTITUDE from the mesh relay, and sends the companion
 * HEARTBEAT to the bridge. Uses the MAVLink headers in libraries/mavlink, which
 * contain the swarm messages (601, 602).
 ****************************************************************************/
#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <mavlink/common/mavlink.h>
#pragma GCC diagnostic pop

namespace swarm_mode
{

class MavlinkLink
{
public:
  MavlinkLink(uint16_t listen_port, const std::string & peer_host, uint16_t peer_port)
  {
    _sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sock < 0) {
      throw std::runtime_error(std::string("socket: ") + strerror(errno));
    }
    int opt = 1;
    setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(_sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
      throw std::runtime_error("bind UDP " + std::to_string(listen_port) + ": " + strerror(errno));
    }

    _peer.sin_family = AF_INET;
    _peer.sin_port = htons(peer_port);
    if (inet_pton(AF_INET, peer_host.c_str(), &_peer.sin_addr) != 1) {
      throw std::runtime_error("invalid peer address " + peer_host);
    }
  }

  ~MavlinkLink() {if (_sock >= 0) {close(_sock);}}

  MavlinkLink(const MavlinkLink &) = delete;
  MavlinkLink & operator=(const MavlinkLink &) = delete;

  /** Read everything that is waiting on the socket and hand each message to on_message. */
  void poll(const std::function<void(const mavlink_message_t &)> & on_message)
  {
    uint8_t buf[2048];
    while (true) {
      const ssize_t n = recv(_sock, buf, sizeof(buf), MSG_DONTWAIT);
      if (n <= 0) {
        return;
      }
      mavlink_message_t msg;
      for (ssize_t i = 0; i < n; ++i) {
        if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &_status)) {
          on_message(msg);
        }
      }
    }
  }

  void sendToPeer(const mavlink_message_t & msg)
  {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    sendto(_sock, buf, len, 0, reinterpret_cast<const sockaddr *>(&_peer), sizeof(_peer));
  }

private:
  int _sock{-1};
  sockaddr_in _peer{};
  mavlink_status_t _status{};
};

}  // namespace swarm_mode
