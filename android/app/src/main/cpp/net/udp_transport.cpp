#include "udp_transport.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../core/protocol.h"
#include "../core/time_util.h"

namespace ls {

bool UdpTransport::start(uint16_t port, Sink sink, const std::string& default_host, uint16_t default_port) {
  stop();
  has_default_peer_ = false;
  if (!default_host.empty() && default_port != 0) {
    default_peer_ = {};
    default_peer_.sin_family = AF_INET;
    default_peer_.sin_port = htons(default_port);
    has_default_peer_ = inet_pton(AF_INET, default_host.c_str(), &default_peer_.sin_addr) == 1;
  }
  sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_ < 0) return false;
  int one = 1;
  ::setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  ::setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
  int rcvbuf = 1 << 20;
  ::setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  a.sin_port = htons(port);
  if (::bind(sock_, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
    ::close(sock_);
    sock_ = -1;
    return false;
  }
  sink_ = std::move(sink);
  has_peer_ = false;
  running_ = true;
  thread_ = std::thread([this] { run(); });
  return true;
}

void UdpTransport::stop() {
  if (running_.exchange(false) && thread_.joinable()) thread_.join();
  if (sock_ >= 0) {
    ::close(sock_);
    sock_ = -1;
  }
}

bool UdpTransport::send(const uint8_t* data, size_t len) {
  if (sock_ < 0) return false;
  sockaddr_in p;
  if (has_peer_.load()) {
    std::lock_guard<std::mutex> l(peer_mu_);
    p = peer_;
  } else if (has_default_peer_) {
    // Until the ESP32 is discovered, sync requests double as a subscription to its AP address.
    p = default_peer_;
  } else {
    return false;
  }
  return ::sendto(sock_, data, len, 0, reinterpret_cast<sockaddr*>(&p), sizeof(p)) == static_cast<ssize_t>(len);
}

std::string UdpTransport::peerAddress() const {
  if (!has_peer_.load()) return "";
  std::lock_guard<std::mutex> l(peer_mu_);
  char buf[INET_ADDRSTRLEN] = {0};
  inet_ntop(AF_INET, &peer_.sin_addr, buf, sizeof(buf));
  return std::string(buf) + ":" + std::to_string(ntohs(peer_.sin_port));
}

void UdpTransport::run() {
  uint8_t buf[2048];
  while (running_.load()) {
    pollfd pfd{sock_, POLLIN, 0};
    if (::poll(&pfd, 1, 100) <= 0) continue;
    sockaddr_in src{};
    socklen_t sl = sizeof(src);
    ssize_t n = ::recvfrom(sock_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&src), &sl);
    if (n <= 0) continue;
    int64_t arrival = nowNs();
    proto::Header h;
    if (!proto::parseHeader(buf, static_cast<size_t>(n), h)) continue;
    if (h.type == proto::MsgType::Data || h.type == proto::MsgType::Hello) {
      std::lock_guard<std::mutex> l(peer_mu_);
      peer_ = src;
      has_peer_ = true;
    }
    if (sink_) sink_(buf, static_cast<size_t>(n), arrival);
  }
}

}  // namespace ls
