#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace ls {

// Compact open-addressing hash set of voxel keys (≈16 bytes per occupied voxel).
class VoxelSet {
 public:
  static uint64_t key(int ix, int iy, int iz) {
    constexpr int64_t kOff = 1 << 20;
    return (uint64_t(ix + kOff) & 0x1FFFFF) | ((uint64_t(iy + kOff) & 0x1FFFFF) << 21) |
           ((uint64_t(iz + kOff) & 0x1FFFFF) << 42) | (1ULL << 63);
  }
  void clear() {
    slots_.clear();
    size_ = 0;
  }
  size_t size() const { return size_; }
  // Returns true if inserted (was absent).
  bool insert(uint64_t k) {
    if ((size_ + 1) * 2 > slots_.size()) grow();
    size_t mask = slots_.size() - 1;
    size_t i = hash(k) & mask;
    while (slots_[i] != 0) {
      if (slots_[i] == k) return false;
      i = (i + 1) & mask;
    }
    slots_[i] = k;
    ++size_;
    return true;
  }

 private:
  static size_t hash(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    return static_cast<size_t>(k);
  }
  void grow() {
    std::vector<uint64_t> old;
    old.swap(slots_);
    slots_.assign(old.empty() ? 1024 : old.size() * 2, 0);
    size_ = 0;
    for (uint64_t k : old)
      if (k) insert(k);
  }
  std::vector<uint64_t> slots_;
  size_t size_ = 0;
};

}  // namespace ls
