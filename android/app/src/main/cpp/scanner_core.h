#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/clock_sync.h"
#include "core/config.h"
#include "core/layout_engine.h"
#include "core/ld06.h"
#include "core/map_store.h"
#include "core/mapping_engine.h"
#include "core/pose_buffer.h"
#include "io/recorder.h"
#include "mock/mock_world.h"
#include "net/udp_transport.h"
#include "render/renderer.h"

namespace ls {

// Orchestrates one scanning session: transport (mock / UDP) -> decode -> clock sync ->
// per-point pose interpolation (de-skew) -> mapping -> MapStore (rendered by Renderer).
class ScannerCore {
 public:
  static ScannerCore& instance();

  bool start(const ScanConfig& cfg);
  // Stops the session, exports the preview cloud/trajectory and returns session.json content.
  std::string stop();
  // Loads a saved session (cloud.ply + trajectory.csv in |dir|) for viewing. Returns JSON
  // {"ok":bool,"points":n,"error":"..."}; fails while a scan is running.
  std::string loadCloud(const std::string& dir);
  void setPaused(bool paused) { paused_ = paused; }
  bool running() const { return running_.load(); }

  // Real phone motion (ARCore / IMU) that drives the simulated rig.
  void pushMotion(const mock::MotionSample& s);
  void stationToggle();
  // Pose input. |external| marks poses from a real odometry backend (ARCore, F3).
  void onPose(const PoseSample& est, bool external = false);

  std::string statsJson();
  CoverageGrid::Image minimap();
  // Latest revolution in polar form: [angle_deg, distance_m, intensity] * n.
  std::vector<float> revolution();

  // Structured model (walls, openings, furniture, rooms). While scanning it is refreshed about
  // once per second by the layout thread; |layoutVersion| changes with every refresh.
  uint64_t layoutVersion();
  std::string layoutJson();
  // Computes the layout of the loaded map now (saved sessions). Returns the JSON.
  std::string computeLayout();

  Renderer& renderer() { return *renderer_; }
  MapStore& store() { return store_; }

 private:
  ScannerCore();
  struct PendingPacket {
    uint64_t esp_us;
    ld06::Packet pkt;
  };
  struct RevPoint {
    float angle, dist, intensity;
  };

  void onDatagram(const uint8_t* data, size_t len, int64_t arrival_ns);
  void ingestLoop();
  void handleDatagram(const std::vector<uint8_t>& d, int64_t arrival_ns);
  void processPending(int64_t now_ns);
  void processPacket(const ld06::Packet& pkt, int64_t t_end_ns);
  void finishRevolution();
  void sendSyncRequest(int64_t now_ns);
  void recordAnchors(int64_t now_ns);
  void layoutLoop();
  void runLayout(const LayoutEngine::Options& o);

  MapStore store_;
  std::unique_ptr<Renderer> renderer_;
  MappingEngine mapping_{&store_};
  LayoutEngine layout_{&store_};
  std::mutex layout_engine_mu_;  // serializes LayoutEngine::update
  std::thread layout_thread_;
  std::atomic<bool> layout_running_{false};
  std::mutex layout_mu_;  // guards layout_json_ / layout_version_ and the loop's wait
  std::condition_variable layout_cv_;
  std::string layout_json_;
  uint64_t layout_version_ = 0;
  PoseBuffer poses_;
  ClockSync clock_;
  Recorder recorder_;
  std::unique_ptr<mock::MockWorld> mock_;
  std::unique_ptr<UdpTransport> udp_;

  ScanConfig cfg_;
  Pose T_dl_;
  std::atomic<bool> running_{false};
  std::atomic<bool> paused_{false};
  std::thread ingest_;
  std::mutex session_mu_;  // serializes start/stop
  uint64_t session_counter_ = 0;
  int64_t t0_ns_ = 0;
  int64_t t0_unix_ms_ = 0;

  std::mutex in_mu_;
  std::condition_variable in_cv_;
  std::deque<std::pair<std::vector<uint8_t>, int64_t>> inbox_;

  std::deque<PendingPacket> pending_;
  uint32_t sync_seq_ = 0;
  int64_t last_sync_ns_ = 0;
  int64_t last_anchor_poll_ns_ = 0;
  int64_t last_floor_ns_ = 0;
  int64_t last_anchor_record_ns_ = 0;

  // Revolution tracking.
  bool have_prev_start_ = false;
  uint16_t prev_start_cdeg_ = 0;
  std::vector<RevPoint> rev_polar_;
  std::vector<Vec3> rev_world_;
  double rev_speed_acc_ = 0;
  int rev_speed_n_ = 0;
  bool rev_has_pose_ = false;
  Pose rev_device_pose_;
  bool have_prev_rev_pose_ = false;
  Pose prev_rev_lidar_;
  bool was_tracking_ = true;

  // Stats (guarded by stats_mu_). Snapshot fields are refreshed by the ingest thread so UI
  // threads never touch transports that stop() may be tearing down.
  std::mutex stats_mu_;
  struct Stats {
    uint64_t datagrams = 0, lost = 0, crc_errors = 0, packets = 0, raw_points = 0, valid_points = 0,
             dropped_no_pose = 0, revolutions = 0;
    bool have_seq = false;
    uint32_t last_seq = 0;
    double latency_ms = 0;
    float rpm = 0;
    float gap_cm = 0, tilt_deg = 0;
    int64_t last_data_ns = 0;
    int64_t last_pose_ns = 0;
    int tracking = 0;
    int tracking_losses = 0;
    float map_distance = 0, map_voxel = 0, map_floor = 0;
    int map_submaps = 0, map_corrections = 0;
    bool station_moving = false, station_displaced = false;
    int station_count = 0, loop_closures = 0;
    bool motion_ready = true;
    int placement = 0;
    double rtt_ms = 0, drift_ppm = 0;
    bool synced = false;
    std::string peer;
  } stats_;
  int64_t last_external_pose_ns_ = 0;
  int64_t last_peer_ns_ = 0;
  std::mutex mock_mu_;  // guards mock_ lifetime against UI-thread calls
  std::vector<RevPoint> last_rev_;
  // Rate windows.
  int64_t rate_t_ = 0;
  uint64_t rate_points_ = 0, rate_raw_ = 0, rate_dgrams_ = 0;
  double pps_ = 0, raw_pps_ = 0, dgps_ = 0;
};

}  // namespace ls
