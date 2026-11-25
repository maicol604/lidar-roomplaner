#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../core/pose_buffer.h"

namespace mcap {
class McapWriter;
}

namespace ls {

// Raw session recorder (MCAP, ros2msg schemas, CDR encoding). Everything needed to reprocess
// the session offline is stored: raw ESP32 datagrams (LD06 packets + ESP32 timestamps),
// clock-sync exchanges, odometry poses / tracking state, anchor updates and calibration.
class Recorder {
 public:
  Recorder();
  ~Recorder();

  bool open(const std::string& path, const std::map<std::string, std::string>& session_meta,
            const std::map<std::string, std::string>& calibration);
  void close();
  bool isOpen() const { return open_.load(); }
  uint64_t bytesWritten() const { return bytes_.load(); }

  void datagram(int64_t t_boot_ns, const uint8_t* data, size_t len);
  void pose(const PoseSample& est);
  void groundTruth(int64_t t_boot_ns, const Pose& truth);
  void clockSync(int64_t t_boot_ns, int64_t send_ns, int64_t recv_ns, uint64_t esp_rx_us, uint64_t esp_tx_us);
  void anchor(int64_t t_boot_ns, int anchor_id, int submap_id, const Pose& p);

 private:
  struct Msg {
    uint16_t channel;
    int64_t t_boot_ns;
    std::vector<uint8_t> data;
  };
  void enqueue(uint16_t channel, int64_t t_boot_ns, std::vector<uint8_t>&& data);
  void run();

  std::unique_ptr<mcap::McapWriter> writer_;
  std::atomic<bool> open_{false};
  std::atomic<uint64_t> bytes_{0};
  int64_t boot_to_unix_ns_ = 0;
  std::map<uint16_t, uint16_t> channel_ids_;
  std::map<uint16_t, uint32_t> seq_;

  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<Msg> queue_;
  bool stop_ = false;
  std::thread thread_;
};

}  // namespace ls
