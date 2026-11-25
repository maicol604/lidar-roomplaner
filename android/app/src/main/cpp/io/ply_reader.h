#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../core/math.h"

namespace ls {

struct LoadedCloud {
  std::vector<Vec3> points;  // internal Y-up frame
  std::vector<uint8_t> intensity;
  std::vector<float> time;
  std::vector<Vec3> trajectory;  // internal Y-up frame
};

// Reads a binary little-endian PLY written by the exporter (Z up) and converts it back to the
// internal Y-up frame. Optional vertex properties: intensity (uchar) and scalar_time (float).
bool readPly(const std::string& path, LoadedCloud& out, std::string& error);

// Reads trajectory.csv (t_s,x,y,z,qx,qy,qz,qw in Z up) positions into out.trajectory.
bool readTrajectoryCsv(const std::string& path, LoadedCloud& out);

}  // namespace ls
