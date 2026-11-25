#pragma once

#include <vector>

#include "../core/math.h"

namespace ls::mock {

// Scripted walk through the synthetic apartment, used by the simulator's automatic tour and by
// tests. It visits every room through its door, turns on the spot before each leg and spins
// 360 degrees in each room so the vertical scan plane sweeps every wall, then returns to the
// start (loop closure). Heading convention as Apartment::startYaw (0 = facing -Z).
class Tour {
 public:
  Tour();
  // Device pose (eye height, phone upright) |t_s| seconds after the start; holds the last pose.
  Pose at(double t_s) const;
  double duration() const { return keys_.empty() ? 0.0 : keys_.back().t; }

 private:
  struct Key {
    double t;
    float x, z, yaw;
  };
  std::vector<Key> keys_;
};

}  // namespace ls::mock
