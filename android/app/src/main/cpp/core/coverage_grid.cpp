#include "coverage_grid.h"

#include <algorithm>
#include <cmath>

namespace ls {
namespace {
constexpr int kMargin = 20;     // cells added around new data when growing
constexpr int kMaxCells = 600;  // 60 m per side
}  // namespace

void CoverageGrid::clear() {
  std::lock_guard<std::mutex> l(mu_);
  x0_ = z0_ = w_ = h_ = 0;
  bins_.clear();
  visited_.clear();
}

bool CoverageGrid::ensure(int cx, int cz) {
  if (w_ > 0 && cx >= x0_ && cx < x0_ + w_ && cz >= z0_ && cz < z0_ + h_) return true;
  int nx0, nz0, nx1, nz1;
  if (w_ == 0) {
    nx0 = cx - kMargin;
    nz0 = cz - kMargin;
    nx1 = cx + kMargin;
    nz1 = cz + kMargin;
  } else {
    nx0 = std::min(x0_, cx - kMargin);
    nz0 = std::min(z0_, cz - kMargin);
    nx1 = std::max(x0_ + w_, cx + kMargin);
    nz1 = std::max(z0_ + h_, cz + kMargin);
  }
  int nw = nx1 - nx0, nh = nz1 - nz0;
  if (nw > kMaxCells || nh > kMaxCells) return false;
  std::vector<uint8_t> nb(size_t(nw) * nh * kBins, 0), nv(size_t(nw) * nh, 0);
  for (int z = 0; z < h_; ++z)
    for (int x = 0; x < w_; ++x) {
      size_t src = size_t(z) * w_ + x;
      size_t dst = size_t(z + z0_ - nz0) * nw + (x + x0_ - nx0);
      std::copy_n(&bins_[src * kBins], kBins, &nb[dst * kBins]);
      nv[dst] = visited_[src];
    }
  bins_.swap(nb);
  visited_.swap(nv);
  x0_ = nx0;
  z0_ = nz0;
  w_ = nw;
  h_ = nh;
  return true;
}

void CoverageGrid::addPoint(float x, float y, float z) {
  int b = static_cast<int>(std::floor((y - kYMin) / kBinH));
  if (b < 0 || b >= kBins) return;
  int cx = static_cast<int>(std::floor(x / kRes)), cz = static_cast<int>(std::floor(z / kRes));
  std::lock_guard<std::mutex> l(mu_);
  if (!ensure(cx, cz)) return;
  uint8_t& c = bins_[size_t(idx(cx, cz)) * kBins + b];
  if (c < 255) ++c;
}

void CoverageGrid::addVisit(float x, float z) {
  int cx = static_cast<int>(std::floor(x / kRes)), cz = static_cast<int>(std::floor(z / kRes));
  std::lock_guard<std::mutex> l(mu_);
  if (!ensure(cx, cz)) return;
  visited_[idx(cx, cz)] = 1;
}

CoverageGrid::Image CoverageGrid::render(float floor_y) const {
  std::lock_guard<std::mutex> l(mu_);
  Image img;
  if (w_ == 0) return img;
  int minx = w_, minz = h_, maxx = -1, maxz = -1;
  for (int z = 0; z < h_; ++z)
    for (int x = 0; x < w_; ++x) {
      size_t i = size_t(z) * w_ + x;
      bool any = visited_[i] != 0;
      for (int b = 0; b < kBins && !any; ++b) any = bins_[i * kBins + b] != 0;
      if (any) {
        minx = std::min(minx, x);
        maxx = std::max(maxx, x);
        minz = std::min(minz, z);
        maxz = std::max(maxz, z);
      }
    }
  if (maxx < 0) return img;
  const int pad = 10;
  minx = std::max(0, minx - pad);
  minz = std::max(0, minz - pad);
  maxx = std::min(w_ - 1, maxx + pad);
  maxz = std::min(h_ - 1, maxz + pad);
  img.width = maxx - minx + 1;
  img.height = maxz - minz + 1;
  img.origin_x = (x0_ + minx) * kRes;
  img.origin_z = (z0_ + minz) * kRes;
  img.rgba.assign(size_t(img.width) * img.height * 4, 0);

  auto binOf = [](float y) { return static_cast<int>(std::floor((y - kYMin) / kBinH)); };
  int fb0 = std::max(0, binOf(floor_y - 0.12f)), fb1 = std::min(kBins - 1, binOf(floor_y + 0.12f));
  int ob0 = std::max(0, binOf(floor_y + 0.3f)), ob1 = std::min(kBins - 1, binOf(floor_y + 1.9f));
  for (int z = minz; z <= maxz; ++z)
    for (int x = minx; x <= maxx; ++x) {
      size_t i = size_t(z) * w_ + x;
      const uint8_t* bins = &bins_[i * kBins];
      int floor_c = 0, obst_c = 0, any_c = 0;
      for (int b = 0; b < kBins; ++b) any_c += bins[b];
      for (int b = fb0; b <= fb1; ++b) floor_c += bins[b];
      for (int b = ob0; b <= ob1; ++b) obst_c += bins[b];
      uint8_t* px = &img.rgba[(size_t(z - minz) * img.width + (x - minx)) * 4];
      if (obst_c >= 3) {
        px[0] = 236; px[1] = 239; px[2] = 244; px[3] = 255;
        ++img.obstacle_cells;
      } else if (floor_c > 0) {
        px[0] = 38; px[1] = 166; px[2] = 154; px[3] = 210;
        ++img.floor_cells;
      } else if (any_c > 0) {
        px[0] = 70; px[1] = 110; px[2] = 160; px[3] = 150;
      }
      if (visited_[i]) {
        px[0] = 255; px[1] = 152; px[2] = 0; px[3] = 255;
      }
    }
  return img;
}

}  // namespace ls
