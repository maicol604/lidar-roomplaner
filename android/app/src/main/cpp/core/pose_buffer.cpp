#include "pose_buffer.h"

namespace ls {

void PoseBuffer::clear() {
  std::lock_guard<std::mutex> l(mu_);
  head_ = 0;
  size_ = 0;
}

void PoseBuffer::push(const PoseSample& s) {
  std::lock_guard<std::mutex> l(mu_);
  if (size_ > 0 && s.t_ns <= get(size_ - 1).t_ns) return;  // keep monotonic
  buf_[head_] = s;
  head_ = (head_ + 1) % buf_.size();
  if (size_ < buf_.size()) ++size_;
}

PoseBuffer::Query PoseBuffer::at(int64_t t_ns, Pose& out, int64_t max_gap_ns) const {
  std::lock_guard<std::mutex> l(mu_);
  if (size_ == 0) return Query::Empty;
  if (t_ns > get(size_ - 1).t_ns) return Query::TooNew;
  if (t_ns < get(0).t_ns) return Query::TooOld;
  size_t lo = 0, hi = size_ - 1;
  while (hi - lo > 1) {
    size_t mid = (lo + hi) / 2;
    if (get(mid).t_ns <= t_ns) lo = mid; else hi = mid;
  }
  const PoseSample& a = get(lo);
  const PoseSample& b = get(hi);
  if (lo == hi || b.t_ns == a.t_ns) {
    out = a.pose;
    return a.state == TrackingState::Tracking ? Query::Ok : Query::Gap;
  }
  if (b.t_ns - a.t_ns > max_gap_ns || a.state != TrackingState::Tracking || b.state != TrackingState::Tracking)
    return Query::Gap;
  float alpha = static_cast<float>(double(t_ns - a.t_ns) / double(b.t_ns - a.t_ns));
  out = interpolate(a.pose, b.pose, alpha);
  return Query::Ok;
}

bool PoseBuffer::latest(PoseSample& out) const {
  std::lock_guard<std::mutex> l(mu_);
  if (size_ == 0) return false;
  out = get(size_ - 1);
  return true;
}

int64_t PoseBuffer::latestTime() const {
  std::lock_guard<std::mutex> l(mu_);
  return size_ == 0 ? 0 : get(size_ - 1).t_ns;
}

}  // namespace ls
