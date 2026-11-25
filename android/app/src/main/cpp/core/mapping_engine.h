#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "config.h"
#include "coverage_grid.h"
#include "map_store.h"
#include "voxel_set.h"

namespace ls {

// Incremental mapper: places world points into anchored sub-maps (deduplicated per voxel),
// maintains the coverage grid, the floor estimate and the point budget. Single-threaded
// (mapping thread); publishes into MapStore for the renderer.
class MappingEngine {
 public:
  explicit MappingEngine(MapStore* store) : store_(store) {}

  void reset(const ScanConfig& cfg, IAnchorProvider* anchors, int64_t t0_ns);
  // Called once per LiDAR packet with the device pose at the packet time.
  void updateDevice(const Pose& device, int64_t t_ns);
  bool addPoint(const Vec3& world, uint8_t intensity, int64_t t_ns);
  // Publishes points added since the last flush to the MapStore.
  void flush();
  // Re-reads anchor poses when the odometry backend corrected them.
  bool pollAnchors();
  void updateFloor(float device_y);
  // Splits the map so the next points start a fresh sub-map (e.g. after tracking loss).
  void breakSubmap() { force_new_submap_ = true; }
  // Drops the anchor backend when the session ends (the map stays visible).
  void detachAnchors() { anchors_ = nullptr; }
  // Loads an already-built cloud (saved session) as a single static submap. Call after reset().
  void loadStatic(const std::vector<Vec3>& points, const std::vector<uint8_t>& intensity,
                  const std::vector<float>& time, const std::vector<Vec3>& trajectory);

  const CoverageGrid& coverage() const { return coverage_; }
  float voxelSize() const { return voxel_; }
  int64_t totalPoints() const { return total_; }
  int submapCount() const { return static_cast<int>(submaps_.size()); }
  int corrections() const { return corrections_; }
  float distance() const { return distance_; }
  float floorY() const { return floor_y_; }
  // Device trajectory in the corrected world frame (anchor-relative poses re-projected).
  // |corrections| (one per submap, optional) are applied on top, as Submap::correction.
  std::vector<std::pair<double, Pose>> trajectory(const std::vector<Pose>* corrections = nullptr) const;

 private:
  struct SubmapState {
    Submap* sm;  // owned by MapStore
    VoxelSet voxels;
    Pose inv_anchor;  // inverse of the latest anchor pose (world -> local)
    Pose device_at_creation;
    int64_t created_ns = 0;
    std::vector<std::pair<double, Pose>> traj_local;  // (t_s, anchor-relative device pose)
  };
  void newSubmap(const Pose& device, int64_t t_ns);
  void enforceBudget();
  void rebuildCoverage();

  MapStore* store_;
  IAnchorProvider* anchors_ = nullptr;
  int64_t t0_ns_ = 0;
  float voxel_ = 0.02f;
  float inv_voxel_ = 50.0f;
  int64_t budget_ = 2000000;
  int64_t total_ = 0;
  std::vector<SubmapState> submaps_;
  int cur_ = -1;
  bool force_new_submap_ = true;
  std::vector<MapPoint> pending_;
  bool have_last_device_ = false;
  Pose last_device_;
  int64_t last_traj_pose_ns_ = 0;
  Vec3 last_traj_;
  float distance_ = 0;
  uint64_t anchors_version_ = 0;
  int corrections_ = 0;
  CoverageGrid coverage_;
  std::vector<uint32_t> height_hist_;  // 2 cm bins in [-5, 5) m
  float floor_y_ = -1.4f;
};

}  // namespace ls
