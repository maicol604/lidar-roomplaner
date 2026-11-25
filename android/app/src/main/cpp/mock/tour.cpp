#include "tour.h"

#include <cmath>

namespace ls::mock {
namespace {
constexpr float kWalkSpeed = 0.5f;           // m/s
constexpr float kTurnRate = 70.0f * kDeg2Rad;  // rad/s, turning on the spot
constexpr float kSpinRate = 25.0f * kDeg2Rad;  // rad/s, full spins inside rooms (slow: dense profiles)
constexpr float kSwayAmp = 14.0f * kDeg2Rad;
constexpr float kSwayPeriod = 3.2f;
constexpr float kEyeHeight = 1.4f;

struct Waypoint {
  float x, z;
  bool spin;
};

// Collision-free path through Apartment (doors: salón-cocina x=6 z 1.6-2.8; pasillo z=4.5 at
// x 4.6-5.5 and 8.5-9.35; dormitorio 1 / baño / dormitorio 2 at z=5.7, x 3.0-3.85, 4.9-5.6 and
// 6.5-7.35).
constexpr Waypoint kPath[] = {
    {3.0f, 2.9f, true},  {3.0f, 1.5f, false},  {3.2f, 3.1f, false},  {5.45f, 2.65f, false}, {6.6f, 2.2f, false},
    {8.0f, 1.5f, true},  {9.0f, 1.9f, false},  {9.0f, 3.8f, false},  {8.92f, 5.1f, false},  {6.9f, 5.1f, false},
    {7.1f, 6.3f, false}, {7.5f, 7.0f, true},   {7.1f, 6.2f, false},  {6.9f, 5.1f, false},  {5.25f, 5.1f, false},
    {5.25f, 6.5f, true}, {5.25f, 5.1f, false}, {3.42f, 5.1f, false}, {3.15f, 6.0f, false},  {2.65f, 6.8f, true},
    {3.15f, 6.0f, false}, {3.42f, 5.1f, false}, {0.7f, 5.05f, true}, {5.05f, 5.05f, false}, {5.05f, 3.9f, false},
    {3.4f, 3.1f, false}, {3.0f, 2.9f, true},
};

float wrapPi(float a) {
  while (a > kPi) a -= 2 * kPi;
  while (a < -kPi) a += 2 * kPi;
  return a;
}
}  // namespace

Tour::Tour() {
  double t = 0;
  float x = kPath[0].x, z = kPath[0].z, yaw = 0;
  keys_.push_back({t, x, z, yaw});
  auto spin = [&] {
    t += 2 * kPi / kSpinRate;
    yaw += 2 * kPi;
    keys_.push_back({t, x, z, yaw});
  };
  if (kPath[0].spin) spin();
  for (size_t k = 1; k < sizeof(kPath) / sizeof(kPath[0]); ++k) {
    const Waypoint& w = kPath[k];
    float dx = w.x - x, dz = w.z - z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist > 1e-3f) {
      float d = wrapPi(std::atan2(-dx, -dz) - yaw);
      t += std::fabs(d) / kTurnRate;
      yaw += d;
      keys_.push_back({t, x, z, yaw});
      t += dist / kWalkSpeed;
      x = w.x;
      z = w.z;
      keys_.push_back({t, x, z, yaw});
    }
    if (w.spin) spin();
  }
}

Pose Tour::at(double t_s) const {
  float x = keys_.back().x, z = keys_.back().z, yaw = keys_.back().yaw;
  if (t_s <= 0) {
    x = keys_.front().x;
    z = keys_.front().z;
    yaw = keys_.front().yaw;
  } else {
    for (size_t k = 1; k < keys_.size(); ++k) {
      if (t_s > keys_[k].t) continue;
      const Key& a = keys_[k - 1];
      const Key& b = keys_[k];
      float u = b.t > a.t ? static_cast<float>((t_s - a.t) / (b.t - a.t)) : 1.0f;
      x = a.x + (b.x - a.x) * u;
      z = a.z + (b.z - a.z) * u;
      yaw = a.yaw + (b.yaw - a.yaw) * u;
      break;
    }
  }
  bool moving = t_s > 0 && t_s < duration();
  float ts = static_cast<float>(t_s);
  // Natural sway and step bob so the scan plane also sweeps walls ahead and behind.
  float sway = moving ? kSwayAmp * std::sin(2 * kPi * ts / kSwayPeriod) : 0.0f;
  float bob = moving ? 0.012f * std::sin(2 * kPi * ts / 0.55f) : 0.0f;
  Pose p;
  p.t = {x, kEyeHeight + bob, z};
  p.q = axisAngle({0, 1, 0}, yaw + sway);
  return p;
}

}  // namespace ls::mock
