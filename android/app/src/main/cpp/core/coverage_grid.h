#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace ls {

// Plan-view (XZ) coverage grid used for the minimap: stores a coarse height histogram per
// cell so floor / obstacles can be classified once the floor height is known.
class CoverageGrid {
 public:
  static constexpr float kRes = 0.10f;
  static constexpr int kBins = 24;
  static constexpr float kBinH = 0.25f;
  static constexpr float kYMin = -3.0f;

  void clear();
  void addPoint(float x, float y, float z);
  void addVisit(float x, float z);

  struct Image {
    int width = 0, height = 0;
    float origin_x = 0, origin_z = 0, res = kRes;
    std::vector<uint8_t> rgba;
    int floor_cells = 0, obstacle_cells = 0;
  };
  Image render(float floor_y) const;
  std::mutex& mutex() const { return mu_; }

 private:
  bool ensure(int cx, int cz);
  int idx(int cx, int cz) const { return (cz - z0_) * w_ + (cx - x0_); }

  mutable std::mutex mu_;
  int x0_ = 0, z0_ = 0, w_ = 0, h_ = 0;
  std::vector<uint8_t> bins_;  // w*h*kBins saturating counts
  std::vector<uint8_t> visited_;
};

}  // namespace ls
