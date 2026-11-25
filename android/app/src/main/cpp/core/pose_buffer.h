#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "math.h"

namespace ls {

enum class TrackingState : int { Initializing = 0, Tracking = 1, Limited = 2, Lost = 3 };

struct PoseSample {
  int64_t t_ns = 0;
  Pose pose;  // T_world_device (device: X right, Y up, -Z out of the back camera)
  TrackingState state = TrackingState::Tracking;
};

// Thread-safe ring buffer of device poses with time interpolation.
class PoseBuffer {
 public:
  explicit PoseBuffer(size_t capacity = 512) : buf_(capacity) {}
  void clear();
  void push(const PoseSample& s);

  enum class Query { Ok, TooNew, TooOld, Gap, Empty };
  // Interpolates the pose at |t_ns|. Gaps larger than |max_gap_ns| (tracking lost) fail.
  Query at(int64_t t_ns, Pose& out, int64_t max_gap_ns = 200000000LL) const;
  bool latest(PoseSample& out) const;
  int64_t latestTime() const;

 private:
  mutable std::mutex mu_;
  std::vector<PoseSample> buf_;
  size_t head_ = 0;  // next write index
  size_t size_ = 0;
  const PoseSample& get(size_t i) const { return buf_[(head_ + buf_.size() - size_ + i) % buf_.size()]; }
};

// Anchors keep sub-maps attached to the physical world; the odometry backend (ARCore)
// may move them when it corrects drift. Implemented by ARCore (F3) and by the mock.
class IAnchorProvider {
 public:
  virtual ~IAnchorProvider() = default;
  virtual int createAnchor(const Pose& world_pose, int64_t t_ns) = 0;
  virtual bool anchorPose(int id, Pose& out) = 0;
  // Increments whenever any anchor pose changes.
  virtual uint64_t anchorsVersion() = 0;
};

}  // namespace ls
