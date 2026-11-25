#include "recorder.h"

#include <mcap/writer.hpp>

#include "../core/time_util.h"

namespace ls {
namespace {

enum Channel : uint16_t { kDatagram = 1, kPose, kTracking, kTruth, kSync, kAnchor };

const char* kSep = "================================================================================\n";

std::string timeDef() { return std::string(kSep) + "MSG: builtin_interfaces/Time\nint32 sec\nuint32 nanosec\n"; }
std::string poseDefs() {
  return std::string(kSep) + "MSG: geometry_msgs/Pose\nPoint position\nQuaternion orientation\n" + kSep +
         "MSG: geometry_msgs/Point\nfloat64 x\nfloat64 y\nfloat64 z\n" + kSep +
         "MSG: geometry_msgs/Quaternion\nfloat64 x 0\nfloat64 y 0\nfloat64 z 0\nfloat64 w 1\n";
}

const std::string kPoseStamped =
    "std_msgs/Header header\ngeometry_msgs/Pose pose\n" + std::string(kSep) +
    "MSG: std_msgs/Header\nbuiltin_interfaces/Time stamp\nstring frame_id\n" + timeDef() + poseDefs();
const std::string kRawDatagram =
    "# Raw UDP datagram from the ESP32 bridge (protocol LDS1, see docs/protocolo_udp.md)\n"
    "builtin_interfaces/Time stamp\nuint8[] data\n" + timeDef();
const std::string kClockSync =
    "builtin_interfaces/Time stamp\nint64 phone_send_ns\nint64 phone_recv_ns\nuint64 esp_rx_us\nuint64 esp_tx_us\n" +
    timeDef();
const std::string kTrackingState =
    "# 0 initializing, 1 tracking, 2 limited, 3 lost\nbuiltin_interfaces/Time stamp\nuint8 state\n" + timeDef();
const std::string kAnchorPose =
    "builtin_interfaces/Time stamp\nint32 anchor_id\nint32 submap_id\ngeometry_msgs/Pose pose\n" + timeDef() + poseDefs();

class Cdr {
 public:
  Cdr() { b_ = {0x00, 0x01, 0x00, 0x00}; }
  void align(size_t n) {
    while ((b_.size() - 4) % n) b_.push_back(0);
  }
  template <typename T>
  void put(T v) {
    align(sizeof(T));
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    b_.insert(b_.end(), p, p + sizeof(T));
  }
  void str(const std::string& s) {
    put<uint32_t>(static_cast<uint32_t>(s.size() + 1));
    b_.insert(b_.end(), s.begin(), s.end());
    b_.push_back(0);
  }
  void bytes(const uint8_t* d, size_t n) {
    put<uint32_t>(static_cast<uint32_t>(n));
    b_.insert(b_.end(), d, d + n);
  }
  void time(int64_t unix_ns) {
    put<int32_t>(static_cast<int32_t>(unix_ns / 1000000000LL));
    put<uint32_t>(static_cast<uint32_t>(unix_ns % 1000000000LL));
  }
  void pose(const Pose& p) {
    put<double>(p.t.x);
    put<double>(p.t.y);
    put<double>(p.t.z);
    put<double>(p.q.x);
    put<double>(p.q.y);
    put<double>(p.q.z);
    put<double>(p.q.w);
  }
  std::vector<uint8_t>&& take() { return std::move(b_); }

 private:
  std::vector<uint8_t> b_;
};

}  // namespace

Recorder::Recorder() = default;
Recorder::~Recorder() { close(); }

bool Recorder::open(const std::string& path, const std::map<std::string, std::string>& session_meta,
                    const std::map<std::string, std::string>& calibration) {
  close();
  writer_ = std::make_unique<mcap::McapWriter>();
  mcap::McapWriterOptions opts("ros2");
  opts.compression = mcap::Compression::None;
  opts.chunkSize = 1024 * 1024;
  auto st = writer_->open(path, opts);
  if (!st.ok()) {
    writer_.reset();
    return false;
  }
  boot_to_unix_ns_ = unixNowNs() - nowNs();

  struct Def {
    uint16_t ch;
    const char* topic;
    const char* type;
    const std::string* schema;
  };
  const Def defs[] = {
      {kDatagram, "/esp32/datagram", "lidar_scanner_msgs/msg/RawDatagram", &kRawDatagram},
      {kPose, "/odometry/pose", "geometry_msgs/msg/PoseStamped", &kPoseStamped},
      {kTracking, "/odometry/tracking_state", "lidar_scanner_msgs/msg/TrackingState", &kTrackingState},
      {kTruth, "/mock/ground_truth", "geometry_msgs/msg/PoseStamped", &kPoseStamped},
      {kSync, "/esp32/clock_sync", "lidar_scanner_msgs/msg/ClockSync", &kClockSync},
      {kAnchor, "/odometry/anchor", "lidar_scanner_msgs/msg/AnchorPose", &kAnchorPose},
  };
  for (const Def& d : defs) {
    mcap::Schema schema(d.type, "ros2msg", *d.schema);
    writer_->addSchema(schema);
    mcap::Channel channel(d.topic, "cdr", schema.id);
    writer_->addChannel(channel);
    channel_ids_[d.ch] = channel.id;
    seq_[d.ch] = 0;
  }
  mcap::KeyValueMap meta(session_meta.begin(), session_meta.end());
  meta["boot_to_unix_ns"] = std::to_string(boot_to_unix_ns_);
  meta["world_frame"] = "arcore_world (Y up, -Z forward at start)";
  (void)writer_->write(mcap::Metadata{"session", meta});
  (void)writer_->write(mcap::Metadata{"calibration", mcap::KeyValueMap(calibration.begin(), calibration.end())});

  {
    std::lock_guard<std::mutex> l(mu_);
    stop_ = false;
    queue_.clear();
  }
  bytes_ = 0;
  open_ = true;
  thread_ = std::thread([this] { run(); });
  return true;
}

void Recorder::close() {
  if (!open_.exchange(false)) return;
  {
    std::lock_guard<std::mutex> l(mu_);
    stop_ = true;
  }
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
  writer_->close();
  writer_.reset();
}

void Recorder::enqueue(uint16_t channel, int64_t t_boot_ns, std::vector<uint8_t>&& data) {
  if (!open_.load()) return;
  {
    std::lock_guard<std::mutex> l(mu_);
    queue_.push_back({channel, t_boot_ns, std::move(data)});
  }
  cv_.notify_one();
}

void Recorder::run() {
  for (;;) {
    std::deque<Msg> batch;
    {
      std::unique_lock<std::mutex> l(mu_);
      cv_.wait(l, [this] { return stop_ || !queue_.empty(); });
      batch.swap(queue_);
      if (batch.empty() && stop_) return;
    }
    for (Msg& m : batch) {
      mcap::Message msg;
      msg.channelId = channel_ids_[m.channel];
      msg.sequence = ++seq_[m.channel];
      msg.logTime = static_cast<mcap::Timestamp>(m.t_boot_ns + boot_to_unix_ns_);
      msg.publishTime = msg.logTime;
      msg.data = reinterpret_cast<const std::byte*>(m.data.data());
      msg.dataSize = m.data.size();
      if (writer_->write(msg).ok()) bytes_ += m.data.size();
    }
  }
}

void Recorder::datagram(int64_t t, const uint8_t* data, size_t len) {
  if (!open_.load()) return;
  Cdr c;
  c.time(t + boot_to_unix_ns_);
  c.bytes(data, len);
  enqueue(kDatagram, t, c.take());
}

void Recorder::pose(const PoseSample& est) {
  if (!open_.load()) return;
  Cdr c;
  c.time(est.t_ns + boot_to_unix_ns_);
  c.str("arcore_world");
  c.pose(est.pose);
  enqueue(kPose, est.t_ns, c.take());
  Cdr s;
  s.time(est.t_ns + boot_to_unix_ns_);
  s.put<uint8_t>(static_cast<uint8_t>(est.state));
  enqueue(kTracking, est.t_ns, s.take());
}

void Recorder::groundTruth(int64_t t, const Pose& truth) {
  if (!open_.load()) return;
  Cdr c;
  c.time(t + boot_to_unix_ns_);
  c.str("mock_world");
  c.pose(truth);
  enqueue(kTruth, t, c.take());
}

void Recorder::clockSync(int64_t t, int64_t send_ns, int64_t recv_ns, uint64_t esp_rx_us, uint64_t esp_tx_us) {
  if (!open_.load()) return;
  Cdr c;
  c.time(t + boot_to_unix_ns_);
  c.put<int64_t>(send_ns);
  c.put<int64_t>(recv_ns);
  c.put<uint64_t>(esp_rx_us);
  c.put<uint64_t>(esp_tx_us);
  enqueue(kSync, t, c.take());
}

void Recorder::anchor(int64_t t, int anchor_id, int submap_id, const Pose& p) {
  if (!open_.load()) return;
  Cdr c;
  c.time(t + boot_to_unix_ns_);
  c.put<int32_t>(anchor_id);
  c.put<int32_t>(submap_id);
  c.pose(p);
  enqueue(kAnchor, t, c.take());
}

}  // namespace ls
