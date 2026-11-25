#include "clock_sync.h"

#include <algorithm>
#include <vector>

namespace ls {
namespace {
constexpr size_t kMaxSamples = 300;
constexpr double kMinSpanForSlopeS = 5.0;
}  // namespace

void ClockSync::reset() { *this = ClockSync(); }

void ClockSync::addSample(int64_t phone_send_ns, int64_t phone_recv_ns, uint64_t esp_rx_us, uint64_t esp_tx_us) {
  int64_t esp_proc_ns = static_cast<int64_t>(esp_tx_us - esp_rx_us) * 1000;
  int64_t rtt_ns = (phone_recv_ns - phone_send_ns) - esp_proc_ns;
  if (rtt_ns < 0 || rtt_ns > 500000000LL) return;
  int64_t phone_mid = phone_send_ns + (phone_recv_ns - phone_send_ns) / 2;
  uint64_t esp_mid = esp_rx_us + (esp_tx_us - esp_rx_us) / 2;
  if (!have_ref_) {
    have_ref_ = true;
    ref_esp_us_ = esp_mid;
    ref_phone_ns_ = phone_mid;
  }
  Sample s;
  s.esp_s = static_cast<double>(static_cast<int64_t>(esp_mid - ref_esp_us_)) * 1e-6;
  s.phone_s = static_cast<double>(phone_mid - ref_phone_ns_) * 1e-9;
  s.rtt_s = rtt_ns * 1e-9;
  samples_.push_back(s);
  while (samples_.size() > kMaxSamples) samples_.pop_front();
  last_rtt_ms_ = s.rtt_s * 1e3;
  refit();
}

void ClockSync::refit() {
  if (samples_.empty()) return;
  std::vector<Sample> sel(samples_.begin(), samples_.end());
  std::sort(sel.begin(), sel.end(), [](const Sample& a, const Sample& b) { return a.rtt_s < b.rtt_s; });
  best_rtt_ms_ = sel.front().rtt_s * 1e3;
  size_t keep = std::max<size_t>(3, sel.size() / 4);
  if (sel.size() < 3) keep = sel.size();
  sel.resize(std::min(keep, sel.size()));

  double minE = sel.front().esp_s, maxE = minE;
  for (auto& s : sel) {
    minE = std::min(minE, s.esp_s);
    maxE = std::max(maxE, s.esp_s);
  }
  if (maxE - minE >= kMinSpanForSlopeS && sel.size() >= 3) {
    double n = static_cast<double>(sel.size()), sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (auto& s : sel) {
      sx += s.esp_s;
      sy += s.phone_s;
      sxx += s.esp_s * s.esp_s;
      sxy += s.esp_s * s.phone_s;
    }
    double den = n * sxx - sx * sx;
    if (den > 1e-12) {
      double slope = (n * sxy - sx * sy) / den;
      // Crystal drift is bounded to a few hundred ppm; reject nonsense fits.
      if (slope > 0.999 && slope < 1.001) {
        slope_ = slope;
        offset_s_ = (sy - slope_ * sx) / n;
        synced_ = samples_.size() >= 5;
        return;
      }
    }
  }
  double acc = 0;
  for (auto& s : sel) acc += s.phone_s - slope_ * s.esp_s;
  offset_s_ = acc / static_cast<double>(sel.size());
  synced_ = samples_.size() >= 5;
}

void ClockSync::observeArrival(uint64_t esp_us, int64_t arrival_ns) {
  int64_t v = arrival_ns - static_cast<int64_t>(esp_us) * 1000;
  if (!have_arrival_ || v < arrival_offset_ns_) {
    arrival_offset_ns_ = v;
    have_arrival_ = true;
  }
}

int64_t ClockSync::espToPhone(uint64_t esp_us) const {
  if (synced_) {
    double esp_s = static_cast<double>(static_cast<int64_t>(esp_us - ref_esp_us_)) * 1e-6;
    double phone_s = offset_s_ + slope_ * esp_s;
    return ref_phone_ns_ + static_cast<int64_t>(phone_s * 1e9);
  }
  return static_cast<int64_t>(esp_us) * 1000 + arrival_offset_ns_;
}

}  // namespace ls
