#include "mapping_engine.h"

#include <algorithm>
#include <cmath>

namespace ls {
namespace {
constexpr float kSubmapMaxDist = 2.0f;
constexpr float kSubmapMaxAngle = 45.0f * kDeg2Rad;
constexpr int64_t kSubmapMaxNs = 20000000000LL;
constexpr float kTrajStep = 0.05f;
constexpr float kHistMin = -5.0f;
constexpr float kHistBin = 0.02f;
constexpr int kHistBins = 500;
}  // namespace

void MappingEngine::reset(const ScanConfig& cfg, IAnchorProvider* anchors, int64_t t0_ns) {
  anchors_ = anchors;
  t0_ns_ = t0_ns;
  voxel_ = cfg.voxel_m;
  inv_voxel_ = 1.0f / voxel_;
  budget_ = cfg.point_budget;
  total_ = 0;
  submaps_.clear();
  cur_ = -1;
  force_new_submap_ = true;
  pending_.clear();
  have_last_device_ = false;
  last_traj_pose_ns_ = 0;
  distance_ = 0;
  anchors_version_ = anchors ? anchors->anchorsVersion() : 0;
  corrections_ = 0;
  coverage_.clear();
  height_hist_.assign(kHistBins, 0);
  floor_y_ = -1.4f;
}

void MappingEngine::newSubmap(const Pose& device, int64_t t_ns) {
  flush();
  auto sm = std::make_unique<Submap>();
  sm->id = static_cast<int>(submaps_.size());
  sm->anchor_id = anchors_ ? anchors_->createAnchor(device, t_ns) : -1;
  sm->created_pose = device;
  sm->current_pose = device;
  SubmapState st;
  st.sm = sm.get();
  st.inv_anchor = device.inverse();
  st.device_at_creation = device;
  st.created_ns = t_ns;
  {
    std::lock_guard<std::mutex> l(store_->mu);
    store_->submaps.push_back(std::move(sm));
  }
  submaps_.push_back(std::move(st));
  cur_ = static_cast<int>(submaps_.size()) - 1;
  force_new_submap_ = false;
  last_traj_ = device.t;
  std::lock_guard<std::mutex> l(store_->mu);
  submaps_[cur_].sm->trajectory.push_back(submaps_[cur_].inv_anchor.apply(device.t));
}

void MappingEngine::updateDevice(const Pose& device, int64_t t_ns) {
  if (have_last_device_) {
    Vec3 d = device.t - last_device_.t;
    float h = std::sqrt(d.x * d.x + d.z * d.z);
    if (h < 1.0f) distance_ += h;
  }
  have_last_device_ = true;
  last_device_ = device;

  bool need_new = force_new_submap_ || cur_ < 0;
  if (!need_new) {
    const SubmapState& s = submaps_[cur_];
    need_new = length(device.t - s.device_at_creation.t) > kSubmapMaxDist ||
               angleBetween(device.q, s.device_at_creation.q) > kSubmapMaxAngle || t_ns - s.created_ns > kSubmapMaxNs;
  }
  if (need_new) newSubmap(device, t_ns);

  if (t_ns - last_traj_pose_ns_ >= 100000000LL) {
    last_traj_pose_ns_ = t_ns;
    submaps_[cur_].traj_local.emplace_back((t_ns - t0_ns_) * 1e-9, submaps_[cur_].inv_anchor * device);
  }

  if (length(device.t - last_traj_) > kTrajStep) {
    last_traj_ = device.t;
    Vec3 local = submaps_[cur_].inv_anchor.apply(device.t);
    {
      std::lock_guard<std::mutex> l(store_->mu);
      submaps_[cur_].sm->trajectory.push_back(local);
    }
    coverage_.addVisit(device.t.x, device.t.z);
  }
}

bool MappingEngine::addPoint(const Vec3& world, uint8_t intensity, int64_t t_ns) {
  if (cur_ < 0) return false;
  SubmapState& s = submaps_[cur_];
  Vec3 local = s.inv_anchor.apply(world);
  uint64_t k = VoxelSet::key(static_cast<int>(std::floor(local.x * inv_voxel_)),
                             static_cast<int>(std::floor(local.y * inv_voxel_)),
                             static_cast<int>(std::floor(local.z * inv_voxel_)));
  if (!s.voxels.insert(k)) return false;
  MapPoint mp{};
  mp.x = local.x;
  mp.y = local.y;
  mp.z = local.z;
  mp.t = static_cast<float>((t_ns - t0_ns_) * 1e-9);
  mp.intensity = intensity;
  pending_.push_back(mp);
  coverage_.addPoint(world.x, world.y, world.z);
  int hb = static_cast<int>(std::floor((world.y - kHistMin) / kHistBin));
  if (hb >= 0 && hb < kHistBins) ++height_hist_[hb];
  return true;
}

void MappingEngine::flush() {
  if (pending_.empty() || cur_ < 0) return;
  total_ += static_cast<int64_t>(pending_.size());
  {
    std::lock_guard<std::mutex> l(store_->mu);
    auto& pts = submaps_[cur_].sm->points;
    pts.insert(pts.end(), pending_.begin(), pending_.end());
    store_->total_points = total_;
  }
  pending_.clear();
  enforceBudget();
}

bool MappingEngine::pollAnchors() {
  if (!anchors_) return false;
  uint64_t v = anchors_->anchorsVersion();
  if (v == anchors_version_) return false;
  anchors_version_ = v;
  flush();
  {
    std::lock_guard<std::mutex> l(store_->mu);
    for (auto& s : submaps_) {
      Pose p;
      if (s.sm->anchor_id >= 0 && anchors_->anchorPose(s.sm->anchor_id, p)) {
        s.sm->current_pose = p;
        s.inv_anchor = p.inverse();
      }
    }
  }
  ++corrections_;
  rebuildCoverage();
  return true;
}

std::vector<std::pair<double, Pose>> MappingEngine::trajectory(const std::vector<Pose>* corrections) const {
  std::vector<std::pair<double, Pose>> out;
  for (size_t k = 0; k < submaps_.size(); ++k) {
    const SubmapState& s = submaps_[k];
    Pose anchor = s.inv_anchor.inverse();
    if (corrections && k < corrections->size()) anchor = (*corrections)[k] * anchor;
    for (const auto& [t, p] : s.traj_local) out.emplace_back(t, anchor * p);
  }
  return out;
}

void MappingEngine::loadStatic(const std::vector<Vec3>& points, const std::vector<uint8_t>& intensity,
                               const std::vector<float>& time, const std::vector<Vec3>& trajectory) {
  auto sm = std::make_unique<Submap>();
  sm->id = 0;
  sm->points.reserve(points.size());
  float top = -1e9f;
  for (size_t i = 0; i < points.size(); ++i) {
    const Vec3& p = points[i];
    MapPoint mp{};
    mp.x = p.x;
    mp.y = p.y;
    mp.z = p.z;
    mp.t = i < time.size() ? time[i] : 0.0f;
    mp.intensity = i < intensity.size() ? intensity[i] : 128;
    sm->points.push_back(mp);
    coverage_.addPoint(p.x, p.y, p.z);
    int hb = static_cast<int>(std::floor((p.y - kHistMin) / kHistBin));
    if (hb >= 0 && hb < kHistBins) ++height_hist_[hb];
    top = std::max(top, p.y);
  }
  float traj_y = 0;
  for (const Vec3& t : trajectory) {
    coverage_.addVisit(t.x, t.z);
    traj_y += t.y;
  }
  sm->trajectory = trajectory;
  SubmapState st;
  st.sm = sm.get();
  total_ = static_cast<int64_t>(points.size());
  {
    std::lock_guard<std::mutex> l(store_->mu);
    store_->submaps.push_back(std::move(sm));
    store_->total_points = total_;
  }
  submaps_.push_back(std::move(st));
  // The scan device height (trajectory) bounds the floor search; without it use the cloud top.
  updateFloor(trajectory.empty() ? top : traj_y / static_cast<float>(trajectory.size()));
}

void MappingEngine::rebuildCoverage() {
  coverage_.clear();
  height_hist_.assign(kHistBins, 0);
  for (auto& s : submaps_) {
    Pose p = s.inv_anchor.inverse();
    for (const MapPoint& mp : s.sm->points) {
      Vec3 w = p.apply({mp.x, mp.y, mp.z});
      coverage_.addPoint(w.x, w.y, w.z);
      int hb = static_cast<int>(std::floor((w.y - kHistMin) / kHistBin));
      if (hb >= 0 && hb < kHistBins) ++height_hist_[hb];
    }
    for (const Vec3& t : s.sm->trajectory) {
      Vec3 w = p.apply(t);
      coverage_.addVisit(w.x, w.z);
    }
  }
}

void MappingEngine::updateFloor(float device_y) {
  int limit = static_cast<int>(std::floor((device_y - 0.5f - kHistMin) / kHistBin));
  limit = std::min(limit, kHistBins);
  uint32_t best = 0;
  int best_i = -1;
  for (int i = 0; i < limit; ++i) {
    // Smooth over 3 bins to tolerate range noise.
    uint32_t c = height_hist_[i] + (i > 0 ? height_hist_[i - 1] : 0) + (i + 1 < kHistBins ? height_hist_[i + 1] : 0);
    if (c > best) {
      best = c;
      best_i = i;
    }
  }
  if (best_i >= 0 && best >= 300) floor_y_ = kHistMin + (best_i + 0.5f) * kHistBin;
  std::lock_guard<std::mutex> l(store_->mu);
  store_->floor_y = floor_y_;
}

void MappingEngine::enforceBudget() {
  if (total_ <= budget_) return;
  voxel_ *= 1.5f;
  inv_voxel_ = 1.0f / voxel_;
  int64_t total = 0;
  for (auto& s : submaps_) {
    VoxelSet vs;
    std::vector<MapPoint> kept;
    kept.reserve(s.sm->points.size() / 2);
    for (const MapPoint& mp : s.sm->points) {
      uint64_t k = VoxelSet::key(static_cast<int>(std::floor(mp.x * inv_voxel_)),
                                 static_cast<int>(std::floor(mp.y * inv_voxel_)),
                                 static_cast<int>(std::floor(mp.z * inv_voxel_)));
      if (vs.insert(k)) kept.push_back(mp);
    }
    total += static_cast<int64_t>(kept.size());
    {
      std::lock_guard<std::mutex> l(store_->mu);
      s.sm->points.swap(kept);
      ++s.sm->generation;
    }
    s.voxels = std::move(vs);
  }
  total_ = total;
  std::lock_guard<std::mutex> l(store_->mu);
  store_->total_points = total_;
}

}  // namespace ls
