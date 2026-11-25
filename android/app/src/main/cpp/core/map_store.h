#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "math.h"
#include "pose_buffer.h"

namespace ls {

#pragma pack(push, 1)
struct MapPoint {
  float x, y, z;  // submap-local coordinates
  float t;        // seconds since session start
  uint8_t intensity;
  uint8_t pad[3];
};
#pragma pack(pop)
static_assert(sizeof(MapPoint) == 20, "MapPoint must be 20 bytes");

struct Submap {
  int id = 0;
  int anchor_id = -1;
  Pose created_pose;  // anchor pose when the submap was created (local frame)
  Pose current_pose;  // latest anchor pose reported by the odometry backend
  // Drift correction found by the layout engine (planar pose graph), applied on top of the
  // anchor pose. Identity until the engine finds loop constraints.
  Pose correction;
  std::vector<MapPoint> points;
  std::vector<Vec3> trajectory;  // local
  uint32_t generation = 0;       // bumped when points are rebuilt (decimation)

  // Local -> corrected world.
  Pose worldPose() const { return correction * current_pose; }
};

// State shared between the mapping thread (writer) and the render / UI threads (readers).
// Writers append under |mu|; readers copy what they need under |mu|.
struct MapStore {
  std::mutex mu;
  uint64_t session_id = 0;
  std::vector<std::unique_ptr<Submap>> submaps;
  int64_t total_points = 0;

  std::vector<Vec3> scanline;  // latest LiDAR revolution, world frame
  uint64_t scanline_version = 0;

  bool has_device = false;
  PoseSample device;  // latest estimated device pose
  // Layout-engine correction of the newest submap; maps |device| and |scanline| onto the
  // corrected map.
  Pose display_correction;
  Pose T_device_lidar;
  float floor_y = -1.4f;
  float session_time_s = 0;
  int64_t t0_ns = 0;
  bool station_mode = false;

  // Viewer mode (saved session loaded from disk): camera framing and fixed "now" for age colors.
  bool has_view_center = false;
  Vec3 view_center;
  float view_radius = 5.0f;
  float view_now_s = 0;

  void reset(uint64_t new_session) {
    std::lock_guard<std::mutex> l(mu);
    session_id = new_session;
    submaps.clear();
    total_points = 0;
    scanline.clear();
    ++scanline_version;
    has_device = false;
    display_correction = Pose{};
    floor_y = -1.4f;
    session_time_s = 0;
    has_view_center = false;
    view_now_s = 0;
  }
};

}  // namespace ls
