#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../core/map_store.h"

namespace ls {

struct TrajectorySample {
  double t_s;
  Pose pose;  // world (ARCore frame, corrected)
};

// Exports are written in a Z-up frame (x, -z, y) of the ARCore Y-up world, the convention used
// by ROS / CloudCompare / PCL.
namespace exporter {
Vec3 toZUp(const Vec3& p);
Quat toZUp(const Quat& q);
bool writePly(const std::string& path, MapStore& store, int64_t& count);
bool writePcd(const std::string& path, MapStore& store, int64_t& count);
bool writeTrajectoryCsv(const std::string& path, const std::vector<TrajectorySample>& traj);
bool writeTrajectoryJson(const std::string& path, const std::vector<TrajectorySample>& traj);
}  // namespace exporter

}  // namespace ls
