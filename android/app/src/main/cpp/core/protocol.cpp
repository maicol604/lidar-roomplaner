#include "protocol.h"

namespace ls::proto {

void putU32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void putU64(std::vector<uint8_t>& out, uint64_t v) {
  for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
uint32_t getU32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint64_t getU64(const uint8_t* p) {
  return uint64_t(getU32(p)) | (uint64_t(getU32(p + 4)) << 32);
}

bool parseHeader(const uint8_t* data, size_t len, Header& out) {
  if (len < kHeaderSize) return false;
  if (getU32(data) != kMagic || data[4] != kVersion) return false;
  out.type = static_cast<MsgType>(data[5]);
  out.count = data[6];
  out.flags = data[7];
  out.seq = getU32(data + 8);
  if (out.type == MsgType::Data) {
    if (out.count > kMaxPacketsPerDatagram) return false;
    if (len < kHeaderSize + out.count * kDataEntrySize) return false;
  }
  return true;
}

uint64_t entryEspTime(const uint8_t* entry) { return getU64(entry); }

void appendHeader(std::vector<uint8_t>& out, MsgType type, uint8_t count, uint32_t seq) {
  putU32(out, kMagic);
  out.push_back(kVersion);
  out.push_back(static_cast<uint8_t>(type));
  out.push_back(count);
  out.push_back(0);
  putU32(out, seq);
}

void appendDataEntry(std::vector<uint8_t>& out, uint64_t esp_time_us, const uint8_t* raw47) {
  putU64(out, esp_time_us);
  out.insert(out.end(), raw47, raw47 + ld06::kPacketSize);
}

std::vector<uint8_t> buildSyncRequest(uint32_t seq, int64_t phone_send_ns) {
  std::vector<uint8_t> out;
  appendHeader(out, MsgType::SyncRequest, 0, seq);
  putU64(out, static_cast<uint64_t>(phone_send_ns));
  return out;
}

std::vector<uint8_t> buildSyncResponse(const SyncResponse& r) {
  std::vector<uint8_t> out;
  appendHeader(out, MsgType::SyncResponse, 0, r.seq);
  putU64(out, static_cast<uint64_t>(r.phone_send_ns));
  putU64(out, r.esp_rx_us);
  putU64(out, r.esp_tx_us);
  return out;
}

bool parseSyncRequest(const uint8_t* data, size_t len, uint32_t& seq, int64_t& phone_send_ns) {
  Header h;
  if (!parseHeader(data, len, h) || h.type != MsgType::SyncRequest || len < kHeaderSize + 8) return false;
  seq = h.seq;
  phone_send_ns = static_cast<int64_t>(getU64(data + kHeaderSize));
  return true;
}

bool parseSyncResponse(const uint8_t* data, size_t len, SyncResponse& out) {
  Header h;
  if (!parseHeader(data, len, h) || h.type != MsgType::SyncResponse || len < kHeaderSize + 24) return false;
  out.seq = h.seq;
  out.phone_send_ns = static_cast<int64_t>(getU64(data + kHeaderSize));
  out.esp_rx_us = getU64(data + kHeaderSize + 8);
  out.esp_tx_us = getU64(data + kHeaderSize + 16);
  return true;
}

}  // namespace ls::proto
