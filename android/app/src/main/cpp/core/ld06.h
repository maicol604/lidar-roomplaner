#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "math.h"

// LDROBOT LD06 serial protocol (230400 8N1).
// Packet (47 bytes): 0x54 | 0x2C | speed u16 (deg/s) | start angle u16 (0.01 deg) |
//   12 x { distance u16 (mm), intensity u8 } | end angle u16 (0.01 deg) |
//   timestamp u16 (ms, wraps at 30000) | CRC8
namespace ls::ld06 {

constexpr uint8_t kHeader = 0x54;
constexpr uint8_t kVerLen = 0x2C;
constexpr int kPointsPerPacket = 12;
constexpr int kPacketSize = 47;
constexpr int kTimestampWrapMs = 30000;

struct Point {
  uint16_t distance_mm = 0;
  uint8_t intensity = 0;
};

struct Packet {
  uint16_t speed_dps = 0;
  uint16_t start_angle_cdeg = 0;
  Point points[kPointsPerPacket];
  uint16_t end_angle_cdeg = 0;
  uint16_t timestamp_ms = 0;

  // Angular span covered by the packet in hundredths of a degree (handles wrap).
  int spanCdeg() const { return (end_angle_cdeg + 36000 - start_angle_cdeg) % 36000; }
  float angleDeg(int i) const {
    float a = (start_angle_cdeg + spanCdeg() * i / float(kPointsPerPacket - 1)) / 100.0f;
    return a >= 360.0f ? a - 360.0f : a;
  }
};

// LD06 angles grow clockwise seen from the top of the sensor. LiDAR frame: x = 0 deg mark,
// z = spin axis (top of the sensor), y = z x x.
inline Vec3 toLidarFrame(float angle_deg, float distance_m) {
  float a = angle_deg * kDeg2Rad;
  return {distance_m * std::cos(a), -distance_m * std::sin(a), 0.0f};
}

uint8_t crc8(const uint8_t* data, size_t len);

// Decodes exactly one packet from |buf| (47 bytes). Returns false on bad header/CRC.
bool decode(const uint8_t* buf, Packet& out);

// Encodes |p| into |buf| (47 bytes) including the CRC.
void encode(const Packet& p, uint8_t* buf);

// Byte-stream re-synchronizing parser (for raw UART streams).
class StreamParser {
 public:
  using Callback = std::function<void(const uint8_t* raw, const Packet& pkt)>;
  explicit StreamParser(Callback cb) : cb_(std::move(cb)) {}
  void feed(const uint8_t* data, size_t len);
  uint64_t crcErrors() const { return crc_errors_; }

 private:
  Callback cb_;
  uint8_t buf_[kPacketSize];
  int pos_ = 0;
  uint64_t crc_errors_ = 0;
};

}  // namespace ls::ld06
