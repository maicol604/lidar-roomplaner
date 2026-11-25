#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ld06.h"

// ESP32 <-> Android UDP protocol (little endian). See docs/protocolo_udp.md.
//
// Common header (12 bytes):
//   u32 magic 'LDS1' | u8 version | u8 type | u8 count | u8 flags | u32 seq
// DATA (ESP32 -> phone):   header + count * { u64 esp_time_us, u8[47] ld06_packet }
//   esp_time_us = esp_timer value when the last byte of the LD06 packet was received.
// SYNC_REQUEST (phone -> ESP32):  header + i64 phone_send_ns
// SYNC_RESPONSE (ESP32 -> phone): header + i64 phone_send_ns (echo) + u64 esp_rx_us + u64 esp_tx_us
// HELLO (ESP32 -> broadcast, 1 Hz): header + u32 uptime_s + u8 fw_major + u8 fw_minor
namespace ls::proto {

constexpr uint32_t kMagic = 0x3153444C;  // bytes 'L','D','S','1'
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 12;
constexpr size_t kDataEntrySize = 8 + ld06::kPacketSize;
constexpr size_t kMaxPacketsPerDatagram = 24;
constexpr uint16_t kDefaultPhonePort = 4210;
constexpr uint16_t kDefaultEspPort = 4211;

enum class MsgType : uint8_t { Data = 1, SyncRequest = 2, SyncResponse = 3, Hello = 4 };

struct Header {
  MsgType type = MsgType::Data;
  uint8_t count = 0;
  uint8_t flags = 0;
  uint32_t seq = 0;
};

struct SyncResponse {
  uint32_t seq = 0;
  int64_t phone_send_ns = 0;
  uint64_t esp_rx_us = 0;
  uint64_t esp_tx_us = 0;
};

bool parseHeader(const uint8_t* data, size_t len, Header& out);

// Returns pointer to entry |i| inside a DATA datagram (caller validated length).
inline const uint8_t* dataEntry(const uint8_t* datagram, size_t i) {
  return datagram + kHeaderSize + i * kDataEntrySize;
}
uint64_t entryEspTime(const uint8_t* entry);
inline const uint8_t* entryRaw(const uint8_t* entry) { return entry + 8; }

void appendHeader(std::vector<uint8_t>& out, MsgType type, uint8_t count, uint32_t seq);
void appendDataEntry(std::vector<uint8_t>& out, uint64_t esp_time_us, const uint8_t* raw47);
std::vector<uint8_t> buildSyncRequest(uint32_t seq, int64_t phone_send_ns);
std::vector<uint8_t> buildSyncResponse(const SyncResponse& r);
bool parseSyncRequest(const uint8_t* data, size_t len, uint32_t& seq, int64_t& phone_send_ns);
bool parseSyncResponse(const uint8_t* data, size_t len, SyncResponse& out);

void putU32(std::vector<uint8_t>& out, uint32_t v);
void putU64(std::vector<uint8_t>& out, uint64_t v);
uint32_t getU32(const uint8_t* p);
uint64_t getU64(const uint8_t* p);

}  // namespace ls::proto
