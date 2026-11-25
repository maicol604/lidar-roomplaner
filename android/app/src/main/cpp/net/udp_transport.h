#pragma once

#include <netinet/in.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ls {

// Receives ESP32 datagrams on |port| and sends phone->ESP32 messages back to the last peer.
class UdpTransport {
 public:
  using Sink = std::function<void(const uint8_t* data, size_t len, int64_t arrival_ns)>;
  ~UdpTransport() { stop(); }
  bool start(uint16_t port, Sink sink, const std::string& default_host = "", uint16_t default_port = 0);
  void stop();
  bool send(const uint8_t* data, size_t len);
  bool hasPeer() const { return has_peer_.load(); }
  std::string peerAddress() const;

 private:
  void run();
  int sock_ = -1;
  Sink sink_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> has_peer_{false};
  mutable std::mutex peer_mu_;
  sockaddr_in peer_{};
  bool has_default_peer_ = false;
  sockaddr_in default_peer_{};
};

}  // namespace ls
