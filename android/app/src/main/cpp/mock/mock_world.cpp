#include "mock_world.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cmath>

#include "../core/protocol.h"
#include "../core/time_util.h"

namespace ls::mock {
namespace {
constexpr float kEyeHeight = 1.4f;
constexpr float kObstacleRadius = 0.05f;
constexpr int64_t kMotionKeepNs = 2000000000LL;
constexpr int64_t kMotionStaleNs = 400000000LL;
constexpr double kSampleRateHz = 4500.0;
constexpr double kRotationHz = 10.0;
constexpr int kPacketsPerDatagram = 8;
constexpr int64_t kFramePeriodNs = 33333333;
constexpr int64_t kArcoreDelayNs = 35000000;
constexpr int64_t kUartTxNs = 2040000;      // 47 bytes @ 230400 baud
constexpr int64_t kLidarInternalNs = 460000;
constexpr int64_t kEspBatchMaxNs = 30000000;

struct MaterialInfo {
  float intensity;
  float dropout;
};
MaterialInfo materialInfo(Material m) {
  switch (m) {
    case Material::Paint: return {190, 0.002f};
    case Material::Wood: return {150, 0.004f};
    case Material::Fabric: return {95, 0.01f};
    case Material::Metal: return {225, 0.03f};
    case Material::Glass: return {35, 0.3f};
    case Material::Mirror: return {200, 0.05f};
    case Material::Tile: return {175, 0.004f};
    case Material::Dark: return {40, 0.08f};
    case Material::Floor: return {140, 0.003f};
    case Material::Ceiling: return {205, 0.002f};
  }
  return {150, 0.0f};
}

// Heading with the same convention as Apartment::startYaw (0 = facing -Z, forward = (-sin, -cos)).
float headingOf(const Quat& q) {
  Vec3 f = rotate(q, {0, 0, -1});
  // Phone held flat (camera looking at the floor or ceiling): use the top edge instead.
  if (f.x * f.x + f.z * f.z < 0.04f) f = rotate(q, {0, 1, 0}) * (f.y < 0 ? 1.0f : -1.0f);
  return std::atan2(-f.x, -f.z);
}
}  // namespace

MockWorld::MockWorld(const Options& opt, DatagramSink datagrams, PoseSink poses)
    : opt_(opt), datagram_sink_(std::move(datagrams)), pose_sink_(std::move(poses)), rng_(std::random_device{}()) {
  yaw_bias_sign_ = uni_(rng_) < 0.5f ? -1.0f : 1.0f;
  scale_sign_ = uni_(rng_) < 0.5f ? -1.0f : 1.0f;
  last_tracked_ = startPose();
  station_truth_pos_ = last_tracked_.t;
  station_est_pos_ = station_truth_pos_;
  if (!opt_.device_motion || opt_.auto_tour) {
    opt_.device_motion = false;
    motion_ready_.store(true);
    real_tracking_ = true;
  }
}

MockWorld::~MockWorld() { stop(); }

void MockWorld::start() {
  if (running_.exchange(true)) return;
  t0_ns_ = simNow();
  esp_epoch_ns_ = t0_ns_ - 7300000000LL;
  next_sample_ns_ = t0_ns_;
  next_frame_ns_ = t0_ns_;
  if (opt_.loopback_udp) {
    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ >= 0) {
      sockaddr_in a{};
      a.sin_family = AF_INET;
      a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      a.sin_port = 0;
      ::bind(sock_, reinterpret_cast<sockaddr*>(&a), sizeof(a));
      ::fcntl(sock_, F_SETFL, ::fcntl(sock_, F_GETFL, 0) | O_NONBLOCK);
    }
  }
  thread_ = std::thread([this] { run(); });
}

void MockWorld::stop() {
  if (!running_.exchange(false)) return;
  if (thread_.joinable()) thread_.join();
  if (sock_ >= 0) {
    ::close(sock_);
    sock_ = -1;
  }
}

void MockWorld::pushMotion(const MotionSample& s) {
  if (!opt_.device_motion) return;
  std::lock_guard<std::mutex> l(motion_mu_);
  if (!motion_.empty() && s.t_ns <= motion_.back().t_ns) return;
  if (!aligned_) {
    if (!s.tracking) return;
    Quat r = axisAngle({0, 1, 0}, apt_.startYaw() - headingOf(s.pose.q));
    Vec3 start = startPose().t;
    T_apt_tracker_ = Pose{start - rotate(r, s.pose.t), r};
    aligned_ = true;
    motion_ready_.store(true);
  }
  MotionSample m;
  m.t_ns = s.t_ns;
  m.tracking = s.tracking;
  // While the tracker is lost its pose is meaningless: the rig stays where it was last seen.
  if (s.tracking) last_tracked_ = T_apt_tracker_ * s.pose;
  m.pose = last_tracked_;
  motion_.push_back(m);
  while (motion_.size() > 2 && motion_.front().t_ns < s.t_ns - kMotionKeepNs) motion_.pop_front();
}

int64_t MockWorld::simNow() const { return nowNs() - (opt_.device_motion ? kMotionLagNs : 0); }

Pose MockWorld::startPose() const {
  Vec3 s = apt_.startPosition();
  Pose p;
  p.t = {s.x, kEyeHeight, s.z};
  p.q = axisAngle({0, 1, 0}, apt_.startYaw());
  return p;
}

Pose MockWorld::truthAt(int64_t t_ns, bool& tracking) {
  if (opt_.auto_tour) {
    tracking = true;
    return tour_.at((t_ns - t0_ns_) * 1e-9);
  }
  if (!opt_.device_motion) {
    tracking = true;
    return startPose();
  }
  std::lock_guard<std::mutex> l(motion_mu_);
  if (motion_.empty()) {
    tracking = false;
    return last_tracked_;
  }
  const MotionSample& back = motion_.back();
  if (t_ns >= back.t_ns) {
    tracking = back.tracking && t_ns - back.t_ns < kMotionStaleNs;
    return back.pose;
  }
  if (t_ns <= motion_.front().t_ns) {
    tracking = motion_.front().tracking;
    return motion_.front().pose;
  }
  size_t i = motion_.size() - 1;
  while (i > 0 && motion_[i - 1].t_ns > t_ns) --i;
  const MotionSample& a = motion_[i - 1];
  const MotionSample& b = motion_[i];
  tracking = a.tracking && b.tracking && b.t_ns - a.t_ns < kMotionStaleNs;
  float alpha = static_cast<float>(double(t_ns - a.t_ns) / double(b.t_ns - a.t_ns));
  return interpolate(a.pose, b.pose, alpha);
}

void MockWorld::updatePlacement(const Vec3& p) {
  bool inside = false;
  for (const Room& r : apt_.rooms())
    if (p.x >= r.x0 && p.x <= r.x1 && p.z >= r.z0 && p.z <= r.z1) inside = true;
  Placement pl = !inside ? Placement::Outside
                 : apt_.collides(p.x, p.z, kObstacleRadius) ? Placement::InsideObstacle
                                                            : Placement::Free;
  placement_.store(static_cast<int>(pl));
}

void MockWorld::sendToEsp(const uint8_t* data, size_t len) {
  std::lock_guard<std::mutex> l(inbox_mu_);
  inbox_.emplace_back(data, data + len);
}

void MockWorld::stationToggle() { station_toggle_req_.store(true); }

uint64_t MockWorld::espClockUs(int64_t phone_ns) const {
  double us = static_cast<double>(phone_ns - esp_epoch_ns_) * (1.0 + esp_ppm_ * 1e-6) / 1000.0;
  return static_cast<uint64_t>(us);
}

int64_t MockWorld::wifiLatencyNs() {
  double ms = 1.5 - 2.5 * std::log(std::max(1e-6f, uni_(rng_)));
  if (uni_(rng_) < 0.02f) ms += 20.0 + 50.0 * uni_(rng_);
  return static_cast<int64_t>(ms * 1e6);
}

void MockWorld::updateDrift(const Pose& prev_truth, const Pose& truth, int64_t t_ns, float dt) {
  Vec3 dp = truth.t - prev_truth.t;
  float ds = std::sqrt(dp.x * dp.x + dp.z * dp.z);
  traveled_ += ds;
  if (opt_.station_mode) {
    gyro_yaw_drift_ += yaw_bias_sign_ * (0.3f / 60.0f) * kDeg2Rad * dt;
    return;
  }
  const MockParams& p = opt_.params;

  // Tracking loss episodes.
  if (p.tracking_loss) {
    if (!tracking_lost_ && uni_(rng_) < p.tracking_loss_per_min / 60.0f * dt) {
      tracking_lost_ = true;
      tracking_lost_until_ = t_ns + static_cast<int64_t>((1.0f + 2.0f * uni_(rng_)) * 1e9);
    } else if (tracking_lost_ && t_ns >= tracking_lost_until_) {
      tracking_lost_ = false;
      if (p.drift) {
        std::lock_guard<std::mutex> l(anchors_mu_);
        Vec3 pivot = drift_.apply(truth.t);
        Quat r = axisAngle({0, 1, 0}, gauss_(rng_) * 0.5f * kDeg2Rad);
        drift_ = Pose{pivot - rotate(r, pivot), r} * drift_;
        drift_.t += Vec3{gauss_(rng_) * 0.03f, 0, gauss_(rng_) * 0.03f};
      }
    }
  }
  if (!p.drift || ds <= 0) return;

  float dyaw = angleBetween(prev_truth.q, truth.q);
  float sigma = p.drift_yaw_deg_per_m * 0.6f * kDeg2Rad;
  float bias = yaw_bias_sign_ * p.drift_yaw_deg_per_m * 0.5f * kDeg2Rad;
  float delta = gauss_(rng_) * sigma * std::sqrt(ds) + bias * ds + 0.004f * dyaw * yaw_bias_sign_;
  Vec3 eps = dp * (scale_sign_ * p.drift_pct / 100.0f) +
             Vec3{gauss_(rng_) * 0.003f * std::sqrt(ds), gauss_(rng_) * 0.001f * std::sqrt(ds),
                  gauss_(rng_) * 0.003f * std::sqrt(ds)};
  std::lock_guard<std::mutex> l(anchors_mu_);
  Vec3 pivot = drift_.apply(truth.t);
  Quat r = axisAngle({0, 1, 0}, delta);
  drift_ = Pose{pivot - rotate(r, pivot), r} * drift_;
  drift_.t += eps;
}

void MockWorld::maybeLoopClosure(int64_t t_ns, const Vec3& pos) {
  const MockParams& p = opt_.params;
  if (opt_.station_mode || !p.drift || !p.loop_closure) return;
  if (traveled_ - last_history_traveled_ >= 0.5f) {
    history_.push_back({t_ns, pos, drift_, traveled_});
    last_history_traveled_ = traveled_;
  }
  if (t_ns - last_closure_ns_ < 10000000000LL) return;
  for (const HistoryEntry& h : history_) {
    float dx = pos.x - h.pos.x, dz = pos.z - h.pos.z;
    if (traveled_ - h.traveled > 8.0f && dx * dx + dz * dz < 1.0f) {
      std::lock_guard<std::mutex> l(anchors_mu_);
      Pose ref = h.drift;
      for (auto& [id, a] : anchors_)
        if (a.created_t > h.t) a.drift = ref;
      for (auto& e : history_)
        if (e.t > h.t) e.drift = ref;
      drift_ = ref;
      last_closure_ns_ = t_ns;
      loop_closures_.fetch_add(1);
      anchors_version_.fetch_add(1);
      return;
    }
  }
}

void MockWorld::sampleRange(const Pose& lidar_world, float angle_deg, ld06::Point& out) {
  out = {0, 0};
  Vec3 o = lidar_world.t;
  Vec3 d = normalize(rotate(lidar_world.q, ld06::toLidarFrame(angle_deg, 1.0f)));
  float total = 0;
  int ignore = -1;
  Hit h;
  bool glass_return = false;
  for (int bounce = 0; bounce < 4; ++bounce) {
    if (!apt_.raycast(o, d, 12.5f - total, h, ignore)) return;
    total += h.t;
    if (opt_.params.glass_artifacts && h.mat == Material::Glass) {
      if (uni_(rng_) < 0.75f) {
        o = o + d * h.t;
        ignore = h.box;
        continue;
      }
      glass_return = true;
      break;
    }
    if (opt_.params.glass_artifacts && h.mat == Material::Mirror) {
      float r = uni_(rng_);
      if (r < 0.55f) {  // specular reflection -> phantom point behind the mirror
        o = o + d * h.t;
        d = d - h.normal * (2.0f * dot(d, h.normal));
        ignore = h.box;
        continue;
      }
      if (r < 0.8f) return;
    }
    break;
  }
  MaterialInfo mi = materialInfo(h.mat);
  float cosi = std::fabs(dot(d, h.normal));
  float dropout = mi.dropout + (cosi < 0.08f ? 0.5f : 0.0f) + (h.mat == Material::Dark ? 0.02f * total : 0.0f);
  if (uni_(rng_) < dropout) return;
  float dist_mm = total * 1000.0f + gauss_(rng_) * opt_.params.noise_mm * 0.5f;
  if (dist_mm < 20.0f || dist_mm > 12000.0f) return;
  float inten;
  if (opt_.params.intensity_by_material) {
    inten = mi.intensity * (0.55f + 0.45f * cosi) / (1.0f + 0.04f * total) + gauss_(rng_) * 6.0f;
    if (glass_return) inten = 30 + gauss_(rng_) * 5.0f;
  } else {
    inten = 160 + gauss_(rng_) * 6.0f;
  }
  out.distance_mm = static_cast<uint16_t>(dist_mm);
  out.intensity = static_cast<uint8_t>(std::clamp(inten, 1.0f, 255.0f));
}

void MockWorld::generateLidar(int64_t from_ns, int64_t to_ns, const Pose& truth_a, const Pose& truth_b) {
  const double rate = kRotationHz * 360.0 * (1.0 + rpm_jitter_);
  const double step_deg = rate / kSampleRateHz;
  const int64_t period_ns = static_cast<int64_t>(1e9 / kSampleRateHz);
  const Pose T_dl = opt_.T_device_lidar;
  while (next_sample_ns_ <= to_ns) {
    float alpha = to_ns > from_ns ? std::clamp(float(double(next_sample_ns_ - from_ns) / double(to_ns - from_ns)), 0.0f, 1.0f) : 1.0f;
    Pose dev = interpolate(truth_a, truth_b, alpha);
    Pose lidar = dev * T_dl;
    if (cur_points_ == 0) cur_start_cdeg_ = static_cast<uint16_t>(std::lround(lidar_angle_deg_ * 100.0) % 36000);
    sampleRange(lidar, static_cast<float>(lidar_angle_deg_), cur_packet_.points[cur_points_]);
    ++cur_points_;
    if (cur_points_ == ld06::kPointsPerPacket) {
      cur_packet_.speed_dps = static_cast<uint16_t>(rate);
      cur_packet_.start_angle_cdeg = cur_start_cdeg_;
      cur_packet_.end_angle_cdeg = static_cast<uint16_t>(std::lround(lidar_angle_deg_ * 100.0) % 36000);
      finishPacket(next_sample_ns_);
      cur_points_ = 0;
    }
    lidar_angle_deg_ += step_deg;
    if (lidar_angle_deg_ >= 360.0) {
      lidar_angle_deg_ -= 360.0;
      rpm_jitter_ = std::clamp(rpm_jitter_ + gauss_(rng_) * 0.002f, -0.015f, 0.015f);
    }
    next_sample_ns_ += period_ns;
  }
}

void MockWorld::finishPacket(int64_t t_end_ns) {
  cur_packet_.timestamp_ms = static_cast<uint16_t>(((t_end_ns - t0_ns_) / 1000000) % ld06::kTimestampWrapMs);
  uint8_t raw[ld06::kPacketSize];
  ld06::encode(cur_packet_, raw);
  int64_t rx_ns = t_end_ns + kLidarInternalNs + kUartTxNs;
  if (datagram_count_ == 0) datagram_first_ns_ = rx_ns;
  proto::appendDataEntry(datagram_, espClockUs(rx_ns), raw);
  ++datagram_count_;
  if (datagram_count_ >= kPacketsPerDatagram) flushDatagram(rx_ns + 300000);
}

void MockWorld::flushDatagram(int64_t send_ns) {
  if (datagram_count_ == 0) return;
  std::vector<uint8_t> out;
  out.reserve(proto::kHeaderSize + datagram_.size());
  proto::appendHeader(out, proto::MsgType::Data, static_cast<uint8_t>(datagram_count_), datagram_seq_++);
  out.insert(out.end(), datagram_.begin(), datagram_.end());
  datagram_.clear();
  datagram_count_ = 0;
  if (opt_.params.packet_loss && uni_(rng_) < opt_.params.packet_loss_pct / 100.0f) return;
  to_phone_.emplace(send_ns + wifiLatencyNs(), std::move(out));
}

Pose MockWorld::estimatedPose(const Pose& truth, int64_t) {
  if (opt_.station_mode) {
    Pose p;
    p.q = normalize(axisAngle({0, 1, 0}, gyro_yaw_drift_) * truth.q);
    p.t = {station_est_pos_.x, truth.t.y, station_est_pos_.z};
    return p;
  }
  return drift_ * truth;
}

void MockWorld::emitCameraFrames(int64_t from_ns, int64_t to_ns, const Pose& truth_a, const Pose& truth_b) {
  while (next_frame_ns_ <= to_ns) {
    float alpha = to_ns > from_ns ? std::clamp(float(double(next_frame_ns_ - from_ns) / double(to_ns - from_ns)), 0.0f, 1.0f) : 1.0f;
    Pose truth = interpolate(truth_a, truth_b, alpha);
    ScheduledPose sp;
    sp.deliver_t = next_frame_ns_ + kArcoreDelayNs;
    sp.est.t_ns = next_frame_ns_;
    sp.est.pose = estimatedPose(truth, next_frame_ns_);
    sp.truth = truth;
    if (!motion_ready_.load()) {
      sp.est.state = TrackingState::Initializing;
    } else if (!real_tracking_) {
      sp.est.state = TrackingState::Lost;
    } else if (opt_.station_mode) {
      sp.est.state = station_moving_.load() ? TrackingState::Limited : TrackingState::Tracking;
    } else {
      sp.est.state = tracking_lost_ ? TrackingState::Lost : TrackingState::Tracking;
    }
    pose_queue_.push_back(sp);
    next_frame_ns_ += kFramePeriodNs;
  }
}

void MockWorld::handleEspMessage(const uint8_t* data, size_t len, int64_t now_ns, bool via_socket) {
  uint32_t seq;
  int64_t send_ns;
  if (!proto::parseSyncRequest(data, len, seq, send_ns)) return;
  proto::SyncResponse r;
  r.seq = seq;
  r.phone_send_ns = send_ns;
  int64_t rx_t = via_socket ? now_ns : send_ns + wifiLatencyNs();
  int64_t tx_t = rx_t + 60000;
  r.esp_rx_us = espClockUs(rx_t);
  r.esp_tx_us = espClockUs(tx_t);
  auto bytes = proto::buildSyncResponse(r);
  if (via_socket) {
    // Answer real-time requests immediately: queueing them behind the simulation lag would
    // inflate the measured round trip.
    sendLoopback(bytes);
  } else {
    to_phone_.emplace(tx_t + wifiLatencyNs(), std::move(bytes));
  }
}

void MockWorld::sendLoopback(const std::vector<uint8_t>& bytes) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(opt_.phone_port);
  ::sendto(sock_, bytes.data(), bytes.size(), 0, reinterpret_cast<sockaddr*>(&a), sizeof(a));
}

void MockWorld::deliver(int64_t now_ns) {
  while (!to_phone_.empty() && to_phone_.begin()->first <= now_ns) {
    auto it = to_phone_.begin();
    if (opt_.loopback_udp && sock_ >= 0) {
      sendLoopback(it->second);
    } else if (datagram_sink_) {
      datagram_sink_(it->second.data(), it->second.size(), it->first);
    }
    to_phone_.erase(it);
  }
  while (!pose_queue_.empty() && pose_queue_.front().deliver_t <= now_ns) {
    if (pose_sink_) pose_sink_(pose_queue_.front().est, pose_queue_.front().truth);
    pose_queue_.pop_front();
  }
}

void MockWorld::run() {
  int64_t prev = simNow();
  Pose prev_truth = truthAt(prev, real_tracking_);
  while (running_.load()) {
    usleep(2000);
    int64_t now = simNow();
    float dt = std::clamp(float((now - prev) * 1e-9), 0.0f, 0.05f);

    if (station_toggle_req_.exchange(false) && opt_.station_mode) {
      if (station_moving_.load()) {
        station_truth_pos_ = prev_truth.t;
        station_est_pos_ = station_truth_pos_ + Vec3{gauss_(rng_) * 0.02f, 0, gauss_(rng_) * 0.02f};
        station_count_.fetch_add(1);
        station_moving_.store(false);
        station_displaced_.store(false);
      } else {
        station_moving_.store(true);
      }
    }

    Pose truth = truthAt(now, real_tracking_);
    updatePlacement(truth.t);
    updateDrift(prev_truth, truth, now, dt);
    maybeLoopClosure(now, truth.t);
    if (opt_.station_mode && !station_moving_.load()) {
      float dx = truth.t.x - station_truth_pos_.x, dz = truth.t.z - station_truth_pos_.z;
      station_displaced_.store(dx * dx + dz * dz > 0.09f);
    }

    generateLidar(prev, now, prev_truth, truth);
    if (datagram_count_ > 0 && now - datagram_first_ns_ > kEspBatchMaxNs) flushDatagram(now);
    emitCameraFrames(prev, now, prev_truth, truth);

    std::vector<std::vector<uint8_t>> inbox;
    {
      std::lock_guard<std::mutex> l(inbox_mu_);
      inbox.swap(inbox_);
    }
    for (auto& m : inbox) handleEspMessage(m.data(), m.size(), now, false);
    if (sock_ >= 0) {
      uint8_t buf[256];
      for (;;) {
        ssize_t n = ::recv(sock_, buf, sizeof(buf), 0);
        if (n <= 0) break;
        handleEspMessage(buf, static_cast<size_t>(n), nowNs(), true);
      }
    }
    deliver(now);
    prev = now;
    prev_truth = truth;
  }
}

int MockWorld::createAnchor(const Pose& world_pose, int64_t t_ns) {
  std::lock_guard<std::mutex> l(anchors_mu_);
  int id = next_anchor_id_++;
  Anchor a;
  a.drift = opt_.station_mode ? Pose{} : drift_;
  a.true_pose = a.drift.inverse() * world_pose;
  a.created_t = t_ns;
  anchors_[id] = a;
  return id;
}

bool MockWorld::anchorPose(int id, Pose& out) {
  std::lock_guard<std::mutex> l(anchors_mu_);
  auto it = anchors_.find(id);
  if (it == anchors_.end()) return false;
  out = it->second.drift * it->second.true_pose;
  return true;
}

}  // namespace ls::mock
