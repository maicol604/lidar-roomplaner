#include "ld06.h"

#include <array>

namespace ls::ld06 {
namespace {

constexpr std::array<uint8_t, 256> makeCrcTable() {
  std::array<uint8_t, 256> t{};
  for (int i = 0; i < 256; ++i) {
    uint8_t c = static_cast<uint8_t>(i);
    for (int b = 0; b < 8; ++b) c = (c & 0x80) ? static_cast<uint8_t>((c << 1) ^ 0x4D) : static_cast<uint8_t>(c << 1);
    t[i] = c;
  }
  return t;
}
constexpr auto kCrcTable = makeCrcTable();
static_assert(kCrcTable[1] == 0x4D && kCrcTable[4] == 0x79, "LD06 CRC table mismatch");

inline uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline void wr16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>(v >> 8);
}

}  // namespace

uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; ++i) crc = kCrcTable[(crc ^ data[i]) & 0xFF];
  return crc;
}

bool decode(const uint8_t* buf, Packet& out) {
  if (buf[0] != kHeader || buf[1] != kVerLen) return false;
  if (crc8(buf, kPacketSize - 1) != buf[kPacketSize - 1]) return false;
  out.speed_dps = rd16(buf + 2);
  out.start_angle_cdeg = rd16(buf + 4);
  const uint8_t* p = buf + 6;
  for (int i = 0; i < kPointsPerPacket; ++i, p += 3) {
    out.points[i].distance_mm = rd16(p);
    out.points[i].intensity = p[2];
  }
  out.end_angle_cdeg = rd16(buf + 42);
  out.timestamp_ms = rd16(buf + 44);
  return true;
}

void encode(const Packet& pk, uint8_t* buf) {
  buf[0] = kHeader;
  buf[1] = kVerLen;
  wr16(buf + 2, pk.speed_dps);
  wr16(buf + 4, pk.start_angle_cdeg);
  uint8_t* p = buf + 6;
  for (int i = 0; i < kPointsPerPacket; ++i, p += 3) {
    wr16(p, pk.points[i].distance_mm);
    p[2] = pk.points[i].intensity;
  }
  wr16(buf + 42, pk.end_angle_cdeg);
  wr16(buf + 44, pk.timestamp_ms);
  buf[46] = crc8(buf, kPacketSize - 1);
}

void StreamParser::feed(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    uint8_t b = data[i];
    if (pos_ == 0) {
      if (b == kHeader) buf_[pos_++] = b;
      continue;
    }
    if (pos_ == 1) {
      if (b == kVerLen) {
        buf_[pos_++] = b;
      } else {
        pos_ = (b == kHeader) ? 1 : 0;
      }
      continue;
    }
    buf_[pos_++] = b;
    if (pos_ == kPacketSize) {
      Packet pk;
      if (decode(buf_, pk)) {
        cb_(buf_, pk);
      } else {
        ++crc_errors_;
      }
      pos_ = 0;
    }
  }
}

}  // namespace ls::ld06
