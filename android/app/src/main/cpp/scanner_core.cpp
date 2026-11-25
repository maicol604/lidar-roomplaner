#include "scanner_core.h"

#include <cmath>
#include <cstdio>
#include <map>

#include "core/protocol.h"
#include "core/time_util.h"
#include "io/exporter.h"
#include "io/ply_reader.h"

namespace ls {
namespace {

std::string jsonEscape(const std::string& s) {
  std::string o;
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char b[8];
          std::snprintf(b, sizeof(b), "\\u%04x", c);
          o += b;
        } else {
          o += c;
        }
    }
  }
  return o;
}

std::string baseName(const std::string& p) {
  auto i = p.find_last_of('/');
  return i == std::string::npos ? p : p.substr(i + 1);
}

const char* sourceName(const ScanConfig& c) { return c.source == ScanConfig::Source::Mock ? "mock" : "udp"; }
const char* modeName(const ScanConfig& c) { return c.mode == ScanConfig::Mode::Station ? "station" : "continuous"; }

}  // namespace

ScannerCore& ScannerCore::instance() {
  static ScannerCore core;
  return core;
}

ScannerCore::ScannerCore() { renderer_ = std::make_unique<Renderer>(&store_); }

bool ScannerCore::start(const ScanConfig& cfg) {
  std::lock_guard<std::mutex> sl(session_mu_);
  if (running_.load()) return false;
  cfg_ = cfg;
  T_dl_ = cfg.lidarExtrinsic();
  t0_ns_ = nowNs();
  t0_unix_ms_ = unixNowNs() / 1000000;

  store_.reset(++session_counter_);
  {
    std::lock_guard<std::mutex> l(store_.mu);
    store_.T_device_lidar = T_dl_;
    store_.t0_ns = t0_ns_;
    store_.station_mode = cfg.mode == ScanConfig::Mode::Station;
  }
  poses_.clear();
  clock_.reset();
  pending_.clear();
  {
    std::lock_guard<std::mutex> l(in_mu_);
    inbox_.clear();
  }
  {
    std::lock_guard<std::mutex> l(stats_mu_);
    stats_ = Stats{};
    last_rev_.clear();
    rate_t_ = t0_ns_;
    rate_points_ = rate_raw_ = rate_dgrams_ = 0;
    pps_ = raw_pps_ = dgps_ = 0;
  }
  have_prev_start_ = false;
  rev_polar_.clear();
  rev_world_.clear();
  rev_speed_acc_ = 0;
  rev_speed_n_ = 0;
  rev_has_pose_ = false;
  have_prev_rev_pose_ = false;
  was_tracking_ = true;
  last_sync_ns_ = last_anchor_poll_ns_ = last_floor_ns_ = last_anchor_record_ns_ = 0;
  last_external_pose_ns_ = 0;
  last_peer_ns_ = 0;
  paused_ = false;

  if (!cfg.session_dir.empty()) {
    std::map<std::string, std::string> meta = {
        {"name", cfg.session_name},  {"source", sourceName(cfg)},        {"mode", modeName(cfg)},
        {"device", cfg.device_model}, {"app_version", cfg.app_version},  {"lidar", "LDROBOT LD06"},
        {"protocol", "LDS1 v1"},      {"time_base", "CLOCK_BOOTTIME"},
    };
    std::map<std::string, std::string> calib = {
        {"T_device_lidar.t", std::to_string(T_dl_.t.x) + " " + std::to_string(T_dl_.t.y) + " " + std::to_string(T_dl_.t.z)},
        {"T_device_lidar.q_wxyz", std::to_string(T_dl_.q.w) + " " + std::to_string(T_dl_.q.x) + " " +
                                       std::to_string(T_dl_.q.y) + " " + std::to_string(T_dl_.q.z)},
        {"lidar_latency_ms", std::to_string(cfg.lidar_latency_ms)},
        {"device_frame", "ARCore camera, display-oriented portrait: X right, Y up, -Z forward"},
        {"lidar_frame", "x = 0 deg mark, z = spin axis, angles clockwise from top"},
    };
    recorder_.open(cfg.session_dir + "/raw.mcap", meta, calib);
  }

  auto dsink = [this](const uint8_t* d, size_t n, int64_t t) { onDatagram(d, n, t); };
  if (cfg.source == ScanConfig::Source::Mock) {
    mock::MockWorld::Options o;
    o.params = cfg.mock;
    o.station_mode = cfg.mode == ScanConfig::Mode::Station;
    o.loopback_udp = cfg.transport == ScanConfig::Transport::Loopback;
    o.phone_port = static_cast<uint16_t>(cfg.udp_port);
    o.T_device_lidar = T_dl_;
    o.device_motion = cfg.mock_device_motion;
    o.auto_tour = cfg.mock_auto_tour;
    auto world = std::make_unique<mock::MockWorld>(
        o, o.loopback_udp ? mock::MockWorld::DatagramSink{} : mock::MockWorld::DatagramSink(dsink),
        [this](const PoseSample& est, const Pose& truth) {
          onPose(est);
          recorder_.groundTruth(est.t_ns, truth);
        });
    mapping_.reset(cfg, world.get(), t0_ns_);
    if (o.loopback_udp) {
      udp_ = std::make_unique<UdpTransport>();
      if (!udp_->start(o.phone_port, dsink)) {
        recorder_.close();
        udp_.reset();
        return false;
      }
    }
    world->start();
    std::lock_guard<std::mutex> l(mock_mu_);
    mock_ = std::move(world);
  } else {
    mapping_.reset(cfg, nullptr, t0_ns_);
    udp_ = std::make_unique<UdpTransport>();
    if (!udp_->start(static_cast<uint16_t>(cfg.udp_port), dsink, cfg.esp_host, static_cast<uint16_t>(cfg.esp_port))) {
      recorder_.close();
      udp_.reset();
      return false;
    }
  }
  running_ = true;
  ingest_ = std::thread([this] { ingestLoop(); });
  {
    std::lock_guard<std::mutex> l(layout_mu_);
    // An explicit empty model so listeners drop the previous session's layout right away.
    layout_json_ = "{\"v\":1,\"valid\":false}";
    ++layout_version_;
  }
  layout_running_ = true;
  layout_thread_ = std::thread([this] { layoutLoop(); });
  return true;
}

void ScannerCore::layoutLoop() {
  LayoutEngine::Options o;
  o.max_icp = 24;
  while (layout_running_.load()) {
    {
      std::unique_lock<std::mutex> l(layout_mu_);
      layout_cv_.wait_for(l, std::chrono::milliseconds(1000), [this] { return !layout_running_.load(); });
    }
    if (!layout_running_.load()) break;
    runLayout(o);
  }
}

void ScannerCore::runLayout(const LayoutEngine::Options& o) {
  std::string json;
  {
    std::lock_guard<std::mutex> l(layout_engine_mu_);
    json = LayoutEngine::toJson(layout_.update(o));
  }
  std::lock_guard<std::mutex> l(layout_mu_);
  layout_json_ = std::move(json);
  ++layout_version_;
}

uint64_t ScannerCore::layoutVersion() {
  std::lock_guard<std::mutex> l(layout_mu_);
  return layout_version_;
}

std::string ScannerCore::layoutJson() {
  std::lock_guard<std::mutex> l(layout_mu_);
  return layout_json_;
}

std::string ScannerCore::computeLayout() {
  std::lock_guard<std::mutex> sl(session_mu_);
  if (running_.load()) return layoutJson();
  LayoutEngine::Options o;
  o.max_icp = -1;
  o.rounds = 2;
  runLayout(o);
  return layoutJson();
}

std::string ScannerCore::stop() {
  std::lock_guard<std::mutex> sl(session_mu_);
  if (!running_.load()) return "{}";
  if (mock_) mock_->stop();
  if (udp_) udp_->stop();
  running_ = false;
  in_cv_.notify_all();
  if (ingest_.joinable()) ingest_.join();
  {
    std::lock_guard<std::mutex> l(layout_mu_);
    layout_running_ = false;
  }
  layout_cv_.notify_all();
  if (layout_thread_.joinable()) layout_thread_.join();
  mapping_.flush();
  mapping_.pollAnchors();
  recordAnchors(nowNs());
  recorder_.close();

  // Final model: full drift optimization (every loop edge measured), corrections applied to the
  // map before exporting so cloud, trajectory and plan agree; extra passes reject phantoms.
  LayoutEngine::Options lo;
  lo.max_icp = -1;
  lo.rounds = 3;
  lo.passes = 3;
  runLayout(lo);
  Layout layout;
  std::vector<Pose> corrections;
  {
    std::lock_guard<std::mutex> l(layout_engine_mu_);
    layout = layout_.layout();
    corrections = layout_.corrections();
  }
  int doors = 0, windows = 0;
  for (const LayoutWall& w : layout.walls)
    for (const LayoutOpening& op : w.openings) (op.kind == LayoutOpening::Door ? doors : windows)++;

  int64_t duration_ms = (nowNs() - t0_ns_) / 1000000;
  int64_t points = 0, ply_count = 0, pcd_count = 0;
  {
    std::lock_guard<std::mutex> l(store_.mu);
    points = store_.total_points;
  }
  Stats st;
  {
    std::lock_guard<std::mutex> l(stats_mu_);
    st = stats_;
  }
  bool exported = false;
  const std::string& dir = cfg_.session_dir;
  if (!dir.empty()) {
    std::vector<TrajectorySample> traj;
    for (auto& [t, p] : mapping_.trajectory(&corrections)) traj.push_back({t, p});
    exported = exporter::writePly(dir + "/cloud.ply", store_, ply_count) &&
               exporter::writePcd(dir + "/cloud.pcd", store_, pcd_count) &&
               exporter::writeTrajectoryCsv(dir + "/trajectory.csv", traj) &&
               exporter::writeTrajectoryJson(dir + "/trajectory.json", traj);
    if (FILE* f = std::fopen((dir + "/layout.json").c_str(), "w")) {
      std::string json = LayoutEngine::toJson(layout);
      std::fwrite(json.data(), 1, json.size(), f);
      std::fclose(f);
    }
  }
  char buf[2560];
  std::snprintf(buf, sizeof(buf),
                "{\"version\":1,\"id\":\"%s\",\"name\":\"%s\",\"createdAtMs\":%lld,\"durationS\":%.1f,"
                "\"source\":\"%s\",\"mode\":\"%s\",\"distanceM\":%.2f,\"points\":%lld,\"submaps\":%d,"
                "\"corrections\":%d,\"voxelCm\":%.2f,\"datagrams\":%llu,\"lostDatagrams\":%llu,"
                "\"device\":\"%s\",\"appVersion\":\"%s\",\"frame\":\"z_up\",\"exported\":%s,"
                "\"areaM2\":%.2f,\"rooms\":%d,\"walls\":%d,\"doors\":%d,\"windows\":%d,\"objects\":%d,"
                "\"loopEdges\":%d,"
                "\"files\":{\"mcap\":\"raw.mcap\",\"ply\":\"cloud.ply\",\"pcd\":\"cloud.pcd\","
                "\"trajectoryCsv\":\"trajectory.csv\",\"trajectoryJson\":\"trajectory.json\",\"layout\":\"layout.json\"}}",
                jsonEscape(baseName(dir)).c_str(), jsonEscape(cfg_.session_name).c_str(),
                static_cast<long long>(t0_unix_ms_), duration_ms / 1000.0, sourceName(cfg_), modeName(cfg_),
                mapping_.distance(), static_cast<long long>(points), mapping_.submapCount(), mapping_.corrections(),
                mapping_.voxelSize() * 100.0f, static_cast<unsigned long long>(st.datagrams),
                static_cast<unsigned long long>(st.lost), jsonEscape(cfg_.device_model).c_str(),
                jsonEscape(cfg_.app_version).c_str(), exported ? "true" : "false", layout.totalArea(),
                static_cast<int>(layout.rooms.size()), static_cast<int>(layout.walls.size()), doors, windows,
                static_cast<int>(layout.objects.size()), layout.loop_edges);
  std::string json = buf;
  if (!dir.empty()) {
    if (FILE* f = std::fopen((dir + "/session.json").c_str(), "w")) {
      std::fputs(json.c_str(), f);
      std::fclose(f);
    }
  }
  mapping_.detachAnchors();
  {
    std::lock_guard<std::mutex> l(mock_mu_);
    mock_.reset();
  }
  udp_.reset();
  return json;
}

std::string ScannerCore::loadCloud(const std::string& dir) {
  std::lock_guard<std::mutex> sl(session_mu_);
  if (running_.load()) return "{\"ok\":false,\"error\":\"scan running\"}";
  LoadedCloud cloud;
  std::string error;
  if (!readPly(dir + "/cloud.ply", cloud, error)) return "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}";
  readTrajectoryCsv(dir + "/trajectory.csv", cloud);

  Vec3 mn = cloud.points.front(), mx = mn;
  float max_t = 0;
  for (const Vec3& p : cloud.points) {
    mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
    mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
  }
  for (float t : cloud.time) max_t = std::max(max_t, t);

  store_.reset(++session_counter_);
  {
    std::lock_guard<std::mutex> l(store_.mu);
    store_.t0_ns = 0;
    store_.station_mode = false;
    store_.has_view_center = true;
    store_.view_center = (mn + mx) * 0.5f;
    store_.view_radius = 0.5f * std::sqrt((mx.x - mn.x) * (mx.x - mn.x) + (mx.z - mn.z) * (mx.z - mn.z));
    store_.view_now_s = max_t;
  }
  {
    std::lock_guard<std::mutex> l(stats_mu_);
    stats_ = Stats{};
    last_rev_.clear();
  }
  ScanConfig cfg;
  cfg.point_budget = static_cast<int64_t>(cloud.points.size()) * 2 + 1;
  mapping_.reset(cfg, nullptr, 0);
  mapping_.loadStatic(cloud.points, cloud.intensity, cloud.time, cloud.trajectory);
  return "{\"ok\":true,\"points\":" + std::to_string(cloud.points.size()) + "}";
}

void ScannerCore::pushMotion(const mock::MotionSample& s) {
  std::lock_guard<std::mutex> l(mock_mu_);
  if (mock_ && running_.load()) mock_->pushMotion(s);
}

void ScannerCore::stationToggle() {
  std::lock_guard<std::mutex> l(mock_mu_);
  if (mock_ && running_.load()) mock_->stationToggle();
}

void ScannerCore::onPose(const PoseSample& est, bool external) {
  if (external) last_external_pose_ns_ = est.t_ns;
  poses_.push(est);
  if (!paused_.load()) recorder_.pose(est);
  {
    std::lock_guard<std::mutex> l(store_.mu);
    store_.device = est;
    store_.has_device = true;
  }
  std::lock_guard<std::mutex> l(stats_mu_);
  if (stats_.tracking == static_cast<int>(TrackingState::Tracking) && est.state == TrackingState::Lost)
    ++stats_.tracking_losses;
  stats_.tracking = static_cast<int>(est.state);
  stats_.last_pose_ns = est.t_ns;
}

void ScannerCore::onDatagram(const uint8_t* data, size_t len, int64_t arrival_ns) {
  {
    std::lock_guard<std::mutex> l(in_mu_);
    if (inbox_.size() > 4000) inbox_.pop_front();
    inbox_.emplace_back(std::vector<uint8_t>(data, data + len), arrival_ns);
  }
  in_cv_.notify_one();
}

void ScannerCore::ingestLoop() {
  while (running_.load()) {
    std::deque<std::pair<std::vector<uint8_t>, int64_t>> batch;
    {
      std::unique_lock<std::mutex> l(in_mu_);
      in_cv_.wait_for(l, std::chrono::milliseconds(5), [&] { return !inbox_.empty() || !running_.load(); });
      batch.swap(inbox_);
    }
    int64_t now = nowNs();
    if (cfg_.source == ScanConfig::Source::Udp && now - last_external_pose_ns_ > 300000000LL) {
      // No odometry backend yet (ARCore arrives in F3): keep the rig static at the origin.
      PoseSample s;
      s.t_ns = now;
      s.state = TrackingState::Tracking;
      poses_.push(s);
      std::lock_guard<std::mutex> l(store_.mu);
      store_.device = s;
      store_.has_device = true;
    }
    for (auto& [d, t] : batch) handleDatagram(d, t);
    processPending(now);

    int64_t sync_period = clock_.sampleCount() < 20 ? 50000000LL : 250000000LL;
    if (now - last_sync_ns_ > sync_period) sendSyncRequest(now);
    if (now - last_anchor_poll_ns_ > 100000000LL) {
      last_anchor_poll_ns_ = now;
      if (mapping_.pollAnchors()) recordAnchors(now);
    }
    if (now - last_anchor_record_ns_ > 1000000000LL) recordAnchors(now);
    if (now - last_floor_ns_ > 1000000000LL) {
      last_floor_ns_ = now;
      PoseSample dev;
      mapping_.updateFloor(poses_.latest(dev) ? dev.pose.t.y : 0.0f);
    }
    std::lock_guard<std::mutex> l(stats_mu_);
    stats_.map_distance = mapping_.distance();
    stats_.map_submaps = mapping_.submapCount();
    stats_.map_corrections = mapping_.corrections();
    stats_.map_voxel = mapping_.voxelSize();
    stats_.map_floor = mapping_.floorY();
    stats_.rtt_ms = clock_.bestRttMs();
    stats_.drift_ppm = clock_.driftPpm();
    stats_.synced = clock_.synced();
    if (mock_) {
      stats_.station_moving = mock_->stationMoving();
      stats_.station_displaced = mock_->stationDisplaced();
      stats_.station_count = mock_->stationCount();
      stats_.loop_closures = mock_->loopClosures();
      stats_.motion_ready = mock_->motionReady();
      stats_.placement = static_cast<int>(mock_->placement());
    }
    if (udp_ && now - last_peer_ns_ > 1000000000LL) {
      last_peer_ns_ = now;
      stats_.peer = udp_->peerAddress();
    }
  }
}

void ScannerCore::handleDatagram(const std::vector<uint8_t>& d, int64_t arrival_ns) {
  if (!paused_.load()) recorder_.datagram(arrival_ns, d.data(), d.size());
  proto::Header h;
  if (!proto::parseHeader(d.data(), d.size(), h)) return;
  if (h.type == proto::MsgType::Data) {
    uint64_t last_esp = 0;
    for (size_t i = 0; i < h.count; ++i) {
      const uint8_t* e = proto::dataEntry(d.data(), i);
      uint64_t esp = proto::entryEspTime(e);
      clock_.observeArrival(esp, arrival_ns);
      ld06::Packet pk;
      if (!ld06::decode(proto::entryRaw(e), pk)) {
        std::lock_guard<std::mutex> l(stats_mu_);
        ++stats_.crc_errors;
        continue;
      }
      last_esp = esp;
      PendingPacket pp{esp, pk};
      auto it = pending_.end();
      while (it != pending_.begin() && std::prev(it)->esp_us > esp) --it;
      pending_.insert(it, pp);
    }
    std::lock_guard<std::mutex> l(stats_mu_);
    ++stats_.datagrams;
    stats_.packets += h.count;
    stats_.last_data_ns = arrival_ns;
    if (stats_.have_seq) {
      uint32_t gap = h.seq - stats_.last_seq - 1;
      if (h.seq > stats_.last_seq && gap > 0 && gap < 1000) stats_.lost += gap;
      // A late (reordered) datagram was counted as lost when its successor arrived.
      if (h.seq < stats_.last_seq && stats_.last_seq - h.seq < 1000 && stats_.lost > 0) --stats_.lost;
    }
    if (!stats_.have_seq || h.seq > stats_.last_seq) stats_.last_seq = h.seq;
    stats_.have_seq = true;
    if (last_esp) {
      double lat = (arrival_ns - clock_.espToPhone(last_esp)) * 1e-6;
      stats_.latency_ms = stats_.latency_ms == 0 ? lat : stats_.latency_ms * 0.95 + lat * 0.05;
    }
  } else if (h.type == proto::MsgType::SyncResponse) {
    proto::SyncResponse r;
    if (proto::parseSyncResponse(d.data(), d.size(), r)) {
      clock_.addSample(r.phone_send_ns, arrival_ns, r.esp_rx_us, r.esp_tx_us);
      if (!paused_.load()) recorder_.clockSync(arrival_ns, r.phone_send_ns, arrival_ns, r.esp_rx_us, r.esp_tx_us);
    }
  }
}

void ScannerCore::processPending(int64_t now_ns) {
  int64_t latest_pose = poses_.latestTime();
  int64_t latency_ns = static_cast<int64_t>(cfg_.lidar_latency_ms * 1e6);
  while (!pending_.empty()) {
    const PendingPacket& p = pending_.front();
    int64_t t_end = clock_.espToPhone(p.esp_us) - latency_ns;
    if (t_end > latest_pose && now_ns - t_end < 1500000000LL) break;  // wait for odometry
    ld06::Packet pk = p.pkt;
    pending_.pop_front();
    processPacket(pk, t_end);
  }
}

void ScannerCore::processPacket(const ld06::Packet& pkt, int64_t t_end_ns) {
  float speed = pkt.speed_dps > 0 ? static_cast<float>(pkt.speed_dps) : 3600.0f;
  double dur_ns = (pkt.spanCdeg() / 100.0) / speed * 1e9;
  if (have_prev_start_ && pkt.start_angle_cdeg + 1000 < prev_start_cdeg_) finishRevolution();
  prev_start_cdeg_ = pkt.start_angle_cdeg;
  have_prev_start_ = true;
  rev_speed_acc_ += speed;
  ++rev_speed_n_;

  Pose dev_end;
  bool end_ok = poses_.at(t_end_ns, dev_end) == PoseBuffer::Query::Ok;
  if (end_ok && !was_tracking_) mapping_.breakSubmap();
  was_tracking_ = end_ok;
  if (end_ok && !paused_.load()) mapping_.updateDevice(dev_end, t_end_ns);

  uint64_t valid = 0, dropped = 0;
  for (int i = 0; i < ld06::kPointsPerPacket; ++i) {
    const ld06::Point& pt = pkt.points[i];
    float angle = pkt.angleDeg(i);
    float d = pt.distance_mm / 1000.0f;
    if (pt.distance_mm > 0) rev_polar_.push_back({angle, d, static_cast<float>(pt.intensity)});
    if (pt.distance_mm == 0 || d < cfg_.min_range_m || d > cfg_.max_range_m || pt.intensity < cfg_.min_intensity)
      continue;
    int64_t t_i = t_end_ns - static_cast<int64_t>(dur_ns * (ld06::kPointsPerPacket - 1 - i) / (ld06::kPointsPerPacket - 1));
    Pose dev;
    if (poses_.at(t_i, dev) != PoseBuffer::Query::Ok) {
      ++dropped;
      continue;
    }
    Vec3 w = dev.apply(T_dl_.apply(ld06::toLidarFrame(angle, d)));
    if (rev_world_.size() < 2000) rev_world_.push_back(w);
    ++valid;
    if (!paused_.load()) mapping_.addPoint(w, pt.intensity, t_i);
  }
  if (end_ok) {
    rev_device_pose_ = dev_end;
    rev_has_pose_ = true;
  }
  mapping_.flush();
  std::lock_guard<std::mutex> l(stats_mu_);
  stats_.raw_points += ld06::kPointsPerPacket;
  stats_.valid_points += valid;
  stats_.dropped_no_pose += dropped;
}

void ScannerCore::finishRevolution() {
  float rpm = rev_speed_n_ > 0 ? static_cast<float>(rev_speed_acc_ / rev_speed_n_ / 6.0) : 0.0f;
  float tilt = -1, gap = -1;
  if (rev_has_pose_) {
    Pose lidar = rev_device_pose_ * T_dl_;
    Vec3 axis = rotate(lidar.q, {0, 0, 1});
    tilt = std::asin(std::min(1.0f, std::fabs(axis.y))) * kRad2Deg;
    if (have_prev_rev_pose_) {
      float worst = 0;
      for (int k = 0; k < 8; ++k) {
        Vec3 q = ld06::toLidarFrame(45.0f * k, cfg_.gap_range_m);
        Vec3 a = prev_rev_lidar_.apply(q), b = lidar.apply(q);
        worst = std::max(worst, std::fabs(dot(b - a, axis)));
      }
      gap = worst * 100.0f;
    }
    prev_rev_lidar_ = lidar;
    have_prev_rev_pose_ = true;
  } else {
    have_prev_rev_pose_ = false;
  }
  {
    std::lock_guard<std::mutex> l(store_.mu);
    store_.scanline = rev_world_;
    ++store_.scanline_version;
  }
  {
    std::lock_guard<std::mutex> l(stats_mu_);
    ++stats_.revolutions;
    stats_.rpm = rpm;
    last_rev_.swap(rev_polar_);
    if (tilt >= 0) stats_.tilt_deg = stats_.tilt_deg * 0.6f + tilt * 0.4f;
    if (gap >= 0) stats_.gap_cm = stats_.gap_cm * 0.6f + gap * 0.4f;
  }
  rev_polar_.clear();
  rev_world_.clear();
  rev_speed_acc_ = 0;
  rev_speed_n_ = 0;
  rev_has_pose_ = false;
}

void ScannerCore::sendSyncRequest(int64_t now_ns) {
  last_sync_ns_ = now_ns;
  auto req = proto::buildSyncRequest(sync_seq_++, nowNs());
  if (udp_) {
    udp_->send(req.data(), req.size());
  } else if (mock_) {
    mock_->sendToEsp(req.data(), req.size());
  }
}

void ScannerCore::recordAnchors(int64_t now_ns) {
  last_anchor_record_ns_ = now_ns;
  if (!recorder_.isOpen()) return;
  std::lock_guard<std::mutex> l(store_.mu);
  for (auto& s : store_.submaps)
    if (s->anchor_id >= 0) recorder_.anchor(now_ns, s->anchor_id, s->id, s->current_pose);
}

std::string ScannerCore::statsJson() {
  int64_t now = nowNs();
  bool has_device;
  PoseSample dev;
  int64_t points;
  {
    std::lock_guard<std::mutex> l(store_.mu);
    has_device = store_.has_device;
    dev = store_.device;
    points = store_.total_points;
  }
  std::lock_guard<std::mutex> l(stats_mu_);
  double dt = (now - rate_t_) * 1e-9;
  if (dt >= 1.0) {
    pps_ = (stats_.valid_points - rate_points_) / dt;
    raw_pps_ = (stats_.raw_points - rate_raw_) / dt;
    dgps_ = (stats_.datagrams - rate_dgrams_) / dt;
    rate_points_ = stats_.valid_points;
    rate_raw_ = stats_.raw_points;
    rate_dgrams_ = stats_.datagrams;
    rate_t_ = now;
  }
  bool running = running_.load();
  bool continuous = cfg_.mode == ScanConfig::Mode::Continuous;
  bool mock = cfg_.source == ScanConfig::Source::Mock;
  bool station_moving = running && stats_.station_moving;
  bool no_data = running && (stats_.last_data_ns == 0 ? now - t0_ns_ > 2000000000LL : now - stats_.last_data_ns > 1500000000LL);
  bool tracking_alert = running && mock &&
                        ((continuous && stats_.tracking != static_cast<int>(TrackingState::Tracking)) ||
                         stats_.tracking == static_cast<int>(TrackingState::Lost) ||
                         stats_.tracking == static_cast<int>(TrackingState::Initializing) ||
                         now - stats_.last_pose_ns > 500000000LL);
  bool station_displaced = running && !continuous && stats_.station_displaced;
  bool speed_alert = running && !station_moving && stats_.gap_cm > cfg_.gap_alert_cm;
  bool tilt_alert = running && !station_moving && stats_.tilt_deg > cfg_.tilt_alert_deg;
  Vec3 f = rotate(dev.pose.q, {0, 0, -1});
  uint64_t total_dg = stats_.datagrams + stats_.lost;
  const std::string& peer = stats_.peer;

  char buf[2600];
  std::snprintf(
      buf, sizeof(buf),
      "{\"running\":%s,\"paused\":%s,\"source\":\"%s\",\"mode\":\"%s\",\"durationS\":%.1f,\"totalPoints\":%lld,"
      "\"pointsPerSec\":%.0f,\"rawPointsPerSec\":%.0f,\"datagramsPerSec\":%.1f,\"datagrams\":%llu,"
      "\"lostDatagrams\":%llu,\"lossPct\":%.2f,\"crcErrors\":%llu,\"latencyMs\":%.1f,\"rttMs\":%.1f,"
      "\"driftPpm\":%.1f,\"synced\":%s,\"rpm\":%.1f,\"revolutions\":%llu,\"distanceM\":%.2f,\"tracking\":%d,"
      "\"trackingLosses\":%d,\"droppedNoPose\":%llu,\"gapCm\":%.1f,\"tiltDeg\":%.1f,\"alertSpeed\":%s,"
      "\"alertTilt\":%s,\"alertTracking\":%s,\"alertNoData\":%s,\"alertStationDisplaced\":%s,\"hasDevice\":%s,"
      "\"deviceX\":%.3f,\"deviceY\":%.3f,\"deviceZ\":%.3f,\"fwdX\":%.3f,\"fwdZ\":%.3f,\"floorY\":%.3f,"
      "\"submaps\":%d,\"corrections\":%d,\"loopClosures\":%d,\"voxelCm\":%.2f,\"pointBudget\":%lld,"
      "\"stationMoving\":%s,\"stationCount\":%d,\"recordBytes\":%llu,\"renderFps\":%.0f,\"peer\":\"%s\","
      "\"motionReady\":%s,\"placement\":%d}",
      running ? "true" : "false", paused_.load() ? "true" : "false", sourceName(cfg_), modeName(cfg_),
      running ? (now - t0_ns_) * 1e-9 : 0.0, static_cast<long long>(points), pps_, raw_pps_, dgps_,
      static_cast<unsigned long long>(stats_.datagrams), static_cast<unsigned long long>(stats_.lost),
      total_dg ? 100.0 * stats_.lost / total_dg : 0.0, static_cast<unsigned long long>(stats_.crc_errors),
      stats_.latency_ms, stats_.rtt_ms, stats_.drift_ppm, stats_.synced ? "true" : "false", stats_.rpm,
      static_cast<unsigned long long>(stats_.revolutions), stats_.map_distance, stats_.tracking,
      stats_.tracking_losses, static_cast<unsigned long long>(stats_.dropped_no_pose), stats_.gap_cm,
      stats_.tilt_deg, speed_alert ? "true" : "false", tilt_alert ? "true" : "false",
      tracking_alert ? "true" : "false", no_data ? "true" : "false", station_displaced ? "true" : "false",
      has_device ? "true" : "false", dev.pose.t.x, dev.pose.t.y, dev.pose.t.z, f.x, f.z, stats_.map_floor,
      stats_.map_submaps, stats_.map_corrections, stats_.loop_closures, stats_.map_voxel * 100.0f,
      static_cast<long long>(cfg_.point_budget), station_moving ? "true" : "false",
      stats_.station_count, static_cast<unsigned long long>(recorder_.bytesWritten()),
      renderer_->fps(), jsonEscape(peer).c_str(), stats_.motion_ready ? "true" : "false",
      running && mock ? stats_.placement : 0);
  return buf;
}

CoverageGrid::Image ScannerCore::minimap() {
  float floor_y;
  {
    std::lock_guard<std::mutex> l(store_.mu);
    floor_y = store_.floor_y;
  }
  return mapping_.coverage().render(floor_y);
}

std::vector<float> ScannerCore::revolution() {
  std::lock_guard<std::mutex> l(stats_mu_);
  std::vector<float> out;
  out.reserve(last_rev_.size() * 3);
  for (const RevPoint& p : last_rev_) {
    out.push_back(p.angle);
    out.push_back(p.dist);
    out.push_back(p.intensity);
  }
  return out;
}

}  // namespace ls
