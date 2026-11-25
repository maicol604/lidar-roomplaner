#pragma once

#include <cstdint>
#include <deque>

namespace ls {

// Estimates the mapping between the ESP32 microsecond clock and the phone boot clock
// from NTP-style ping/pong exchanges:  phone_ns ~= offset + slope * esp_us * 1000.
// Low-RTT samples are preferred because WiFi latency is asymmetric and bursty.
class ClockSync {
 public:
  void reset();
  void addSample(int64_t phone_send_ns, int64_t phone_recv_ns, uint64_t esp_rx_us, uint64_t esp_tx_us);
  // Arrival-based fallback used until enough sync samples exist.
  void observeArrival(uint64_t esp_us, int64_t arrival_ns);

  bool synced() const { return synced_; }
  bool usable() const { return synced_ || have_arrival_; }
  int64_t espToPhone(uint64_t esp_us) const;

  double lastRttMs() const { return last_rtt_ms_; }
  double bestRttMs() const { return best_rtt_ms_; }
  double driftPpm() const { return (slope_ - 1.0) * 1e6; }
  int sampleCount() const { return static_cast<int>(samples_.size()); }

 private:
  struct Sample {
    double esp_s;    // relative to ref_esp_us_
    double phone_s;  // relative to ref_phone_ns_
    double rtt_s;
  };
  void refit();

  std::deque<Sample> samples_;
  bool have_ref_ = false;
  uint64_t ref_esp_us_ = 0;
  int64_t ref_phone_ns_ = 0;

  bool synced_ = false;
  double offset_s_ = 0;  // phone_s at esp_s == 0
  double slope_ = 1.0;

  bool have_arrival_ = false;
  int64_t arrival_offset_ns_ = 0;  // min(arrival - esp)

  double last_rtt_ms_ = 0;
  double best_rtt_ms_ = 0;
};

}  // namespace ls
