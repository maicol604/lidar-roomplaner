#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "../core/config.h"
#include "../core/ld06.h"
#include "../core/pose_buffer.h"
#include "apartment.h"
#include "tour.h"

namespace ls::mock {

// Real motion of the phone (ARCore or IMU) in the tracker's own gravity-aligned frame
// (Y up; device frame as in PoseSample). |tracking| is false while the tracker cannot localize.
struct MotionSample {
  int64_t t_ns = 0;  // CLOCK_BOOTTIME
  Pose pose;
  bool tracking = false;
};

// Where the rig is with respect to the virtual apartment (the user walks in a real space and
// may cross virtual walls or leave the apartment).
enum class Placement : int { Free = 0, InsideObstacle = 1, Outside = 2 };

// Simulates the full hardware chain in-process: the phone+LD06 rig carried through the
// synthetic apartment, the LD06 byte stream, the ESP32 bridge (timestamps, batching, its own
// drifting clock, WiFi latency and loss) and an ARCore-like odometry (drift, tracking loss,
// relocalization corrections that move anchors).
//
// The rig follows the real phone: the first tracked MotionSample is mapped onto the apartment
// start pose (position and heading) and every later sample moves the rig by the same amount.
// The simulation runs kMotionLagNs behind real time so that device poses (which arrive with
// some latency) are always available when LiDAR samples are generated.
class MockWorld : public IAnchorProvider {
 public:
  using DatagramSink = std::function<void(const uint8_t* data, size_t len, int64_t arrival_ns)>;
  using PoseSink = std::function<void(const PoseSample& est, const Pose& truth)>;

  static constexpr int64_t kMotionLagNs = 120000000;

  struct Options {
    MockParams params;
    bool station_mode = false;
    bool loopback_udp = false;
    uint16_t phone_port = 4210;
    Pose T_device_lidar;
    // false: the rig stays still at the start pose and odometry is always tracking (diagnostics).
    bool device_motion = true;
    // The rig follows a scripted walk through every room (Tour) instead of the phone.
    bool auto_tour = false;
  };

  MockWorld(const Options& opt, DatagramSink datagrams, PoseSink poses);
  ~MockWorld() override;

  void start();
  void stop();
  void pushMotion(const MotionSample& s);
  // True once the device tracker delivered its first tracked pose (rig aligned with the apartment).
  bool motionReady() const { return motion_ready_.load(); }
  Placement placement() const { return static_cast<Placement>(placement_.load()); }
  // Phone -> ESP32 message (in-process transport).
  void sendToEsp(const uint8_t* data, size_t len);
  // Station mode: toggles between "capturing at a fixed station" and "moving to the next one".
  void stationToggle();
  bool stationMoving() const { return station_moving_.load(); }
  int stationCount() const { return station_count_.load(); }
  bool stationDisplaced() const { return station_displaced_.load(); }
  int loopClosures() const { return loop_closures_.load(); }

  int createAnchor(const Pose& world_pose, int64_t t_ns) override;
  bool anchorPose(int id, Pose& out) override;
  uint64_t anchorsVersion() override { return anchors_version_.load(); }

 private:
  struct Scheduled {
    int64_t t;
    std::vector<uint8_t> bytes;
  };
  struct ScheduledPose {
    int64_t deliver_t;
    PoseSample est;
    Pose truth;
  };
  struct Anchor {
    Pose true_pose;
    Pose drift;
    int64_t created_t;
  };
  struct HistoryEntry {
    int64_t t;
    Vec3 pos;
    Pose drift;
    float traveled;
  };

  void run();
  int64_t simNow() const;
  Pose startPose() const;
  // Rig pose in the apartment at |t_ns|; |tracking| reports whether the real tracker was localized.
  Pose truthAt(int64_t t_ns, bool& tracking);
  void updatePlacement(const Vec3& p);
  void sendLoopback(const std::vector<uint8_t>& bytes);
  void updateDrift(const Pose& prev_truth, const Pose& truth, int64_t t_ns, float dt);
  void maybeLoopClosure(int64_t t_ns, const Vec3& pos);
  void generateLidar(int64_t from_ns, int64_t to_ns, const Pose& truth_a, const Pose& truth_b);
  void sampleRange(const Pose& lidar_world, float angle_deg, ld06::Point& out);
  void finishPacket(int64_t t_end_ns);
  void flushDatagram(int64_t now_ns);
  void emitCameraFrames(int64_t from_ns, int64_t to_ns, const Pose& truth_a, const Pose& truth_b);
  Pose estimatedPose(const Pose& truth, int64_t t_ns);
  void deliver(int64_t now_ns);
  void handleEspMessage(const uint8_t* data, size_t len, int64_t now_ns, bool via_socket);
  uint64_t espClockUs(int64_t phone_ns) const;
  int64_t wifiLatencyNs();

  Options opt_;
  DatagramSink datagram_sink_;
  PoseSink pose_sink_;
  Apartment apt_;
  Tour tour_;
  std::mt19937 rng_;
  std::normal_distribution<float> gauss_{0.0f, 1.0f};
  std::uniform_real_distribution<float> uni_{0.0f, 1.0f};

  std::thread thread_;
  std::atomic<bool> running_{false};

  // Device motion, already transformed into the apartment frame (guarded by motion_mu_).
  std::mutex motion_mu_;
  std::deque<MotionSample> motion_;
  bool aligned_ = false;
  Pose T_apt_tracker_;
  Pose last_tracked_;
  std::atomic<bool> motion_ready_{false};
  std::atomic<int> placement_{0};
  bool real_tracking_ = false;

  int64_t t0_ns_ = 0;
  int64_t esp_epoch_ns_ = 0;
  double esp_ppm_ = 35.0;

  // LiDAR emulation.
  double lidar_angle_deg_ = 0;
  int64_t next_sample_ns_ = 0;
  ld06::Packet cur_packet_{};
  int cur_points_ = 0;
  uint16_t cur_start_cdeg_ = 0;
  float rpm_jitter_ = 0;
  std::vector<uint8_t> datagram_;
  int datagram_count_ = 0;
  uint32_t datagram_seq_ = 0;
  int64_t datagram_first_ns_ = 0;

  // Odometry emulation.
  Pose drift_;  // estimated = drift * truth
  int64_t next_frame_ns_ = 0;
  bool tracking_lost_ = false;
  int64_t tracking_lost_until_ = 0;
  float traveled_ = 0;
  float last_history_traveled_ = -1;
  std::vector<HistoryEntry> history_;
  int64_t last_closure_ns_ = 0;
  float yaw_bias_sign_ = 1;
  float scale_sign_ = 1;

  // Station mode.
  std::atomic<bool> station_moving_{false};
  std::atomic<bool> station_toggle_req_{false};
  std::atomic<int> station_count_{1};
  std::atomic<bool> station_displaced_{false};
  Vec3 station_truth_pos_;
  Vec3 station_est_pos_;
  float gyro_yaw_drift_ = 0;

  std::mutex anchors_mu_;
  std::map<int, Anchor> anchors_;
  int next_anchor_id_ = 1;
  std::atomic<uint64_t> anchors_version_{0};
  std::atomic<int> loop_closures_{0};

  // Delivery queues.
  std::multimap<int64_t, std::vector<uint8_t>> to_phone_;
  std::deque<ScheduledPose> pose_queue_;
  std::mutex inbox_mu_;
  std::vector<std::vector<uint8_t>> inbox_;

  int sock_ = -1;
};

}  // namespace ls::mock
