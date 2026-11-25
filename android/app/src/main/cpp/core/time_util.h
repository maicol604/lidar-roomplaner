#pragma once

#include <cstdint>
#include <ctime>

namespace ls {

// Phone time base: CLOCK_BOOTTIME (== SystemClock.elapsedRealtimeNanos), the same base
// ARCore camera timestamps use on devices with a REALTIME camera timestamp source.
inline int64_t nowNs() {
  timespec ts{};
  clock_gettime(CLOCK_BOOTTIME, &ts);
  return int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

inline int64_t unixNowNs() {
  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  return int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

}  // namespace ls
