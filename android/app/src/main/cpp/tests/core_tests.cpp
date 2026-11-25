// Native unit tests for the scanner core. Build with -DLIDARCORE_TESTS=ON for an Android ABI,
// push to a device with adb and run (see tool/run_native_tests.ps1).
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "core/clock_sync.h"
#include "core/config.h"
#include "core/layout_engine.h"
#include "core/ld06.h"
#include "core/map_store.h"
#include "core/mapping_engine.h"
#include "core/pose_buffer.h"
#include "core/protocol.h"
#include "core/time_util.h"
#include "io/exporter.h"
#include "io/ply_reader.h"
#include "mock/apartment.h"
#include "mock/mock_world.h"
#include "mock/tour.h"

using namespace ls;

static int g_failures = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

static void testLd06RoundTrip() {
  std::printf("ld06 round trip\n");
  ld06::Packet p;
  p.speed_dps = 3600;
  p.start_angle_cdeg = 35500;
  p.end_angle_cdeg = 380;  // wraps through 0
  p.timestamp_ms = 29999;
  for (int i = 0; i < 12; ++i) p.points[i] = {static_cast<uint16_t>(1000 + i * 37), static_cast<uint8_t>(100 + i)};
  uint8_t raw[47];
  ld06::encode(p, raw);
  ld06::Packet q;
  CHECK(ld06::decode(raw, q));
  CHECK(q.start_angle_cdeg == 35500 && q.end_angle_cdeg == 380 && q.timestamp_ms == 29999);
  CHECK(q.points[11].distance_mm == 1000 + 11 * 37 && q.points[11].intensity == 111);
  CHECK(q.spanCdeg() == 880);
  CHECK(std::fabs(q.angleDeg(11) - 3.8f) < 0.01f);
  raw[10] ^= 0x01;
  CHECK(!ld06::decode(raw, q));
}

static void testStreamParserResync() {
  std::printf("ld06 stream parser resync\n");
  ld06::Packet p{};
  p.speed_dps = 3600;
  uint8_t raw[47];
  ld06::encode(p, raw);
  std::vector<uint8_t> stream = {0x00, 0x54, 0x11, 0x54};
  stream.insert(stream.end(), raw, raw + 47);
  stream.insert(stream.end(), raw, raw + 20);  // truncated packet
  stream.insert(stream.end(), raw, raw + 47);
  int ok = 0;
  ld06::StreamParser parser([&](const uint8_t*, const ld06::Packet&) { ++ok; });
  parser.feed(stream.data(), stream.size());
  CHECK(ok >= 1);
  CHECK(parser.crcErrors() >= 1);
}

static void testProtocol() {
  std::printf("udp protocol\n");
  uint8_t raw[47];
  ld06::Packet p{};
  ld06::encode(p, raw);
  std::vector<uint8_t> d;
  proto::appendHeader(d, proto::MsgType::Data, 2, 77);
  proto::appendDataEntry(d, 123456789ULL, raw);
  proto::appendDataEntry(d, 123459999ULL, raw);
  proto::Header h;
  CHECK(proto::parseHeader(d.data(), d.size(), h));
  CHECK(h.type == proto::MsgType::Data && h.count == 2 && h.seq == 77);
  CHECK(proto::entryEspTime(proto::dataEntry(d.data(), 1)) == 123459999ULL);
  CHECK(!proto::parseHeader(d.data(), d.size() - 1, h));
  proto::SyncResponse r{5, -42, 1000, 1060};
  auto b = proto::buildSyncResponse(r);
  proto::SyncResponse r2;
  CHECK(proto::parseSyncResponse(b.data(), b.size(), r2));
  CHECK(r2.seq == 5 && r2.phone_send_ns == -42 && r2.esp_tx_us == 1060);
}

static void testClockSync() {
  std::printf("clock sync (offset + 50 ppm drift, asymmetric jitter)\n");
  std::mt19937 rng(1);
  std::exponential_distribution<double> lat(1.0 / 3e6);
  const double ppm = 50e-6;
  const int64_t epoch = 5'000'000'000LL;
  auto esp = [&](int64_t phone) { return static_cast<uint64_t>((phone - epoch) * (1 + ppm) / 1000.0); };
  ClockSync cs;
  int64_t t = 100'000'000'000LL;
  for (int i = 0; i < 400; ++i, t += 250'000'000LL) {
    int64_t up = 1'000'000 + static_cast<int64_t>(lat(rng)), down = 1'000'000 + static_cast<int64_t>(lat(rng));
    uint64_t rx = esp(t + up);
    cs.addSample(t, t + up + 60'000 + down, rx, rx + 60);
  }
  CHECK(cs.synced());
  int64_t probe = t + 1'000'000'000LL;
  double err_ms = std::fabs(static_cast<double>(cs.espToPhone(esp(probe)) - probe)) * 1e-6;
  std::printf("  error %.3f ms, drift %.1f ppm\n", err_ms, cs.driftPpm());
  CHECK(err_ms < 1.0);
  // ESP32 runs 50 ppm fast, so phone time advances 50 ppm slower than ESP32 time.
  CHECK(std::fabs(cs.driftPpm() + 50.0) < 15.0);
}

static void testPoseBuffer() {
  std::printf("pose buffer interpolation\n");
  PoseBuffer pb;
  PoseSample a, b, c;
  a.t_ns = 0;
  b.t_ns = 100;
  b.pose.t = {1, 0, 0};
  b.pose.q = axisAngle({0, 1, 0}, kPi / 2);
  pb.push(a);
  pb.push(b);
  Pose out;
  CHECK(pb.at(50, out) == PoseBuffer::Query::Ok);
  CHECK(std::fabs(out.t.x - 0.5f) < 1e-5f);
  CHECK(std::fabs(angleBetween(out.q, axisAngle({0, 1, 0}, kPi / 4))) < 1e-3f);
  CHECK(pb.at(150, out) == PoseBuffer::Query::TooNew);
  c.t_ns = 1'000'000'000LL;
  pb.push(c);
  CHECK(pb.at(500'000'000LL, out) == PoseBuffer::Query::Gap);
}

static void testMappingDedup() {
  std::printf("mapping voxel dedup + budget\n");
  MapStore store;
  store.reset(1);
  MappingEngine m(&store);
  ScanConfig cfg;
  cfg.voxel_m = 0.02f;
  cfg.point_budget = 100000;
  m.reset(cfg, nullptr, 0);
  m.updateDevice(Pose{}, 1);
  CHECK(m.addPoint({1.001f, 0.5f, 0.3f}, 100, 2));
  CHECK(!m.addPoint({1.005f, 0.505f, 0.305f}, 100, 3));
  CHECK(m.addPoint({1.05f, 0.5f, 0.3f}, 100, 4));
  m.flush();
  CHECK(m.totalPoints() == 2);
  for (int i = 0; i < 150000; ++i) m.addPoint({(i % 400) * 0.021f, (i / 400) * 0.021f, 1.0f}, 50, 5);
  m.flush();
  CHECK(m.totalPoints() <= 100000);
  CHECK(m.voxelSize() > 0.02f);
}

// End-to-end: mock hardware -> bytes -> clock sync -> pose interpolation -> world points.
// With odometry drift disabled every reconstructed point must lie on the apartment geometry.
static void testMockEndToEnd() {
  std::printf("mock end-to-end (3 s, rotating in place)\n");
  ScanConfig cfg;
  cfg.mock.drift = false;
  cfg.mock.glass_artifacts = false;
  cfg.mock.packet_loss = false;
  cfg.mock.tracking_loss = false;
  std::mutex mu;
  std::vector<std::pair<std::vector<uint8_t>, int64_t>> dgrams;
  PoseBuffer poses(2048);
  mock::MockWorld::Options o;
  o.params = cfg.mock;
  o.T_device_lidar = cfg.lidarExtrinsic();
  mock::MockWorld world(
      o,
      [&](const uint8_t* d, size_t n, int64_t t) {
        std::lock_guard<std::mutex> l(mu);
        dgrams.emplace_back(std::vector<uint8_t>(d, d + n), t);
      },
      [&](const PoseSample& est, const Pose&) { poses.push(est); });
  world.start();
  // The phone rotates in place (~37 deg/s) somewhere in its own tracking frame.
  std::atomic<bool> feeding{true};
  std::thread feeder([&] {
    const int64_t t0 = nowNs();
    while (feeding.load()) {
      int64_t t = nowNs();
      mock::MotionSample s;
      s.t_ns = t;
      s.pose.t = {-4.0f, 0.2f, 7.5f};
      s.pose.q = axisAngle({0, 1, 0}, 0.65f * float((t - t0) * 1e-9));
      s.tracking = true;
      world.pushMotion(s);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });
  ClockSync cs;
  uint32_t seq = 0;
  int64_t end = nowNs() + 3'000'000'000LL;
  while (nowNs() < end) {
    auto req = proto::buildSyncRequest(seq++, nowNs());
    world.sendToEsp(req.data(), req.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  world.stop();
  feeding.store(false);
  feeder.join();

  std::vector<std::pair<uint64_t, ld06::Packet>> packets;
  for (auto& [d, t] : dgrams) {
    proto::Header h;
    if (!proto::parseHeader(d.data(), d.size(), h)) continue;
    if (h.type == proto::MsgType::SyncResponse) {
      proto::SyncResponse r;
      if (proto::parseSyncResponse(d.data(), d.size(), r)) cs.addSample(r.phone_send_ns, t, r.esp_rx_us, r.esp_tx_us);
    } else if (h.type == proto::MsgType::Data) {
      for (size_t i = 0; i < h.count; ++i) {
        const uint8_t* e = proto::dataEntry(d.data(), i);
        cs.observeArrival(proto::entryEspTime(e), t);
        ld06::Packet pk;
        CHECK(ld06::decode(proto::entryRaw(e), pk));
        packets.emplace_back(proto::entryEspTime(e), pk);
      }
    }
  }
  CHECK(cs.synced());
  size_t pts = packets.size() * 12;
  std::printf("  packets %zu (%.0f pts/s), rpm %.0f\n", packets.size(), pts / 3.0,
              packets.empty() ? 0.0 : packets[0].second.speed_dps / 6.0);
  CHECK(pts / 3.0 > 4000 && pts / 3.0 < 5000);

  Pose T_dl = cfg.lidarExtrinsic();
  int inside = 0, total = 0, no_pose = 0;
  for (auto& [esp_us, pk] : packets) {
    int64_t t_end = cs.espToPhone(esp_us) - static_cast<int64_t>(cfg.lidar_latency_ms * 1e6);
    double dur = (pk.spanCdeg() / 100.0) / pk.speed_dps * 1e9;
    for (int i = 0; i < 12; ++i) {
      if (pk.points[i].distance_mm == 0) continue;
      int64_t ti = t_end - static_cast<int64_t>(dur * (11 - i) / 11);
      Pose dev;
      if (poses.at(ti, dev) != PoseBuffer::Query::Ok) {
        ++no_pose;
        continue;
      }
      Vec3 w = dev.apply(T_dl.apply(ld06::toLidarFrame(pk.angleDeg(i), pk.points[i].distance_mm / 1000.0f)));
      ++total;
      if (w.x > -0.15f && w.x < 10.15f && w.z > -0.15f && w.z < 8.15f && w.y > -0.12f && w.y < 2.72f) ++inside;
    }
  }
  double frac = total ? double(inside) / total : 0;
  std::printf("  reconstructed %d points, %.2f%% inside the apartment, %d without pose\n", total, frac * 100, no_pose);
  CHECK(total > 8000);
  CHECK(frac > 0.995);
}

// The first tracked phone pose is mapped onto the apartment start pose; later real motion
// moves the rig by the same amount (heading included), with no joystick involved.
static void testMockFollowsDeviceMotion() {
  std::printf("mock rig follows device motion\n");
  mock::MockWorld::Options o;
  o.params.drift = false;
  o.params.tracking_loss = false;
  std::mutex mu;
  PoseSample last_est;
  Pose last_truth;
  mock::MockWorld world(
      o, [](const uint8_t*, size_t, int64_t) {},
      [&](const PoseSample& est, const Pose& truth) {
        std::lock_guard<std::mutex> l(mu);
        last_est = est;
        last_truth = truth;
      });
  auto latest = [&](PoseSample& est, Pose& truth) {
    std::lock_guard<std::mutex> l(mu);
    est = last_est;
    truth = last_truth;
  };
  auto wait = [](int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); };
  world.start();
  wait(300);
  PoseSample est;
  Pose truth;
  latest(est, truth);
  CHECK(!world.motionReady());
  CHECK(est.state == TrackingState::Initializing);

  // Tracker frame: the phone starts at (5, 0.3, -2) facing +X and walks 1 m forward.
  const Quat facing_x = axisAngle({0, 1, 0}, -kPi / 2);
  auto push = [&](float fwd, bool tracking) {
    mock::MotionSample s;
    s.t_ns = nowNs();
    s.pose.t = {5.0f + fwd, 0.3f, -2.0f};
    s.pose.q = facing_x;
    s.tracking = tracking;
    world.pushMotion(s);
  };
  for (int i = 0; i <= 50; ++i) {
    push(i / 50.0f, true);
    wait(20);
  }
  wait(300);
  latest(est, truth);
  CHECK(world.motionReady());
  CHECK(est.state == TrackingState::Tracking);
  // Apartment start is (3.0, 1.4, 2.9) facing -Z: 1 m forward ends at z = 1.9.
  CHECK(std::fabs(truth.t.x - 3.0f) < 0.02f && std::fabs(truth.t.y - 1.4f) < 0.02f &&
        std::fabs(truth.t.z - 1.9f) < 0.02f);
  Vec3 f = rotate(truth.q, {0, 0, -1});
  CHECK(f.z < -0.99f);
  CHECK(world.placement() == mock::Placement::Free);

  // Real tracking lost: odometry reports Lost and the rig stays where it was last seen.
  for (int i = 0; i < 15; ++i) {
    push(5.0f, false);
    wait(20);
  }
  wait(200);
  latest(est, truth);
  CHECK(est.state == TrackingState::Lost);
  CHECK(std::fabs(truth.t.z - 1.9f) < 0.02f);

  // Walking 4 m in total leaves the 10 x 8 m apartment through its north wall.
  for (int i = 0; i < 15; ++i) {
    push(4.0f, true);
    wait(20);
  }
  wait(250);
  CHECK(world.placement() == mock::Placement::Outside);
  world.stop();
}

static void testPlyRoundTrip() {
  std::printf("ply export/import round trip\n");
  MapStore store;
  store.reset(1);
  auto sm = std::make_unique<Submap>();
  sm->current_pose.t = {1, 0, 0};  // anchor offset must be applied on export
  for (int i = 0; i < 100; ++i) {
    MapPoint mp{};
    mp.x = i * 0.1f;
    mp.y = 1.5f;
    mp.z = -2.0f;
    mp.t = i * 0.5f;
    mp.intensity = static_cast<uint8_t>(i);
    sm->points.push_back(mp);
  }
  store.submaps.push_back(std::move(sm));
  store.total_points = 100;
  const char* path = "/data/local/tmp/lidarcore_test.ply";
  int64_t n = 0;
  CHECK(exporter::writePly(path, store, n) && n == 100);
  LoadedCloud c;
  std::string err;
  CHECK(readPly(path, c, err));
  CHECK(c.points.size() == 100);
  if (c.points.size() == 100) {
    CHECK(std::fabs(c.points[10].x - 2.0f) < 1e-5f);
    CHECK(std::fabs(c.points[10].y - 1.5f) < 1e-5f);
    CHECK(std::fabs(c.points[10].z + 2.0f) < 1e-5f);
    CHECK(c.intensity[42] == 42 && std::fabs(c.time[42] - 21.0f) < 1e-5f);
  }
  std::remove(path);
}

// Synthetic scan of the apartment along the scripted tour: one LD06 revolution (0.8 deg steps,
// 7.5 mm noise, glass mostly transparent) every 0.1 s, fed through the MappingEngine without an
// anchor backend. |yaw_deg_per_m| / |scale| accumulate odometry drift in the estimated poses, so
// revisited walls disagree unless the layout engine corrects them.
static void syntheticScan(MappingEngine& m, float yaw_deg_per_m, float scale, unsigned seed) {
  mock::Apartment apt;
  mock::Tour tour;
  ScanConfig cfg;
  cfg.point_budget = 4000000;
  m.reset(cfg, nullptr, 0);
  const Pose T_dl = cfg.lidarExtrinsic();
  std::mt19937 rng(seed);
  std::normal_distribution<float> noise(0.0f, 0.0075f);
  std::uniform_real_distribution<float> uni(0.0f, 1.0f);
  Pose drift, prev = tour.at(0);
  for (double t = 0; t <= tour.duration() + 1.0; t += 0.1) {
    Pose truth = tour.at(t);
    Vec3 dp = truth.t - prev.t;
    float ds = std::sqrt(dp.x * dp.x + dp.z * dp.z);
    if (ds > 0) {
      Vec3 pivot = drift.apply(truth.t);
      Quat r = axisAngle({0, 1, 0}, yaw_deg_per_m * kDeg2Rad * ds);
      drift = Pose{pivot - rotate(r, pivot), r} * drift;
      drift.t += Vec3{dp.x, 0, dp.z} * scale;
    }
    prev = truth;
    Pose est = drift * truth;
    int64_t t_ns = static_cast<int64_t>(t * 1e9) + 1;
    m.updateDevice(est, t_ns);
    Pose lidar = truth * T_dl;
    for (float ang = 0; ang < 360.0f; ang += 0.8f) {
      Vec3 dir = normalize(rotate(lidar.q, ld06::toLidarFrame(ang, 1.0f)));
      Vec3 o = lidar.t;
      float total = 0;
      int ignore = -1;
      bool hit = false;
      uint8_t inten = 180;
      mock::Hit h;
      for (int bounce = 0; bounce < 3; ++bounce) {
        if (!apt.raycast(o, dir, 12.0f - total, h, ignore)) break;
        total += h.t;
        if (h.mat == mock::Material::Glass && uni(rng) < 0.75f) {
          o = o + dir * h.t;
          ignore = h.box;
          continue;
        }
        if (h.mat == mock::Material::Mirror && uni(rng) < 0.55f) {
          // Specular reflection: the return is placed along the original ray (phantom behind
          // the mirror), as with the real sensor.
          o = o + dir * h.t;
          dir = dir - h.normal * (2.0f * dot(dir, h.normal));
          ignore = h.box;
          continue;
        }
        hit = true;
        if (h.mat == mock::Material::Glass) inten = 30;
        break;
      }
      if (!hit) continue;
      float d = total + noise(rng);
      if (d < 0.05f) continue;
      m.addPoint(est.apply(T_dl.apply(ld06::toLidarFrame(ang, d))), inten, t_ns);
    }
    m.flush();
    if (static_cast<int>(t * 10) % 10 == 0) m.updateFloor(est.t.y);
  }
  m.updateFloor(1.4f);
}

// Distance of the detected walls (sampled along their centerline) to the apartment's real walls.
struct WallError {
  float rms = 0, max = 0;
  int duplicates = 0;
};
static WallError wallError(const Layout& l) {
  struct Gt {
    bool along_x;  // z = c, x in [a0, a1]; else x = c, z in [a0, a1]
    float c, a0, a1;
  };
  const Gt gt[] = {{true, 0, 0, 10},     {true, 8, 0, 10},     {false, 0, 0, 8},     {false, 10, 0, 8},
                   {false, 6, 0, 4.5f},  {true, 4.5f, 0, 10},  {true, 5.7f, 0, 10},  {false, 4, 5.7f, 8},
                   {false, 6.2f, 5.7f, 8}};
  WallError e;
  double sse = 0;
  int n = 0;
  for (const LayoutWall& w : l.walls) {
    if (w.length() < 1.0f) continue;
    V2 d = w.b - w.a;
    bool along_x = std::fabs(d.x) > std::fabs(d.z);
    for (int k = 0; k <= 4; ++k) {
      V2 p = w.a + d * (k / 4.0f);
      float best = 1e9f;
      for (const Gt& g : gt) {
        if (g.along_x != along_x) continue;
        float along = g.along_x ? p.x : p.z, across = g.along_x ? p.z : p.x;
        float out = std::max(0.0f, std::max(g.a0 - along, along - g.a1));
        best = std::min(best, std::hypot(across - g.c, out));
      }
      sse += best * best;
      e.max = std::max(e.max, best);
      ++n;
    }
  }
  e.rms = n ? static_cast<float>(std::sqrt(sse / n)) : 1e9f;
  for (size_t a = 0; a < l.walls.size(); ++a)
    for (size_t b = a + 1; b < l.walls.size(); ++b) {
      const LayoutWall& A = l.walls[a];
      const LayoutWall& B = l.walls[b];
      V2 da = (A.b - A.a) * (1.0f / std::max(1e-3f, A.length()));
      V2 db = (B.b - B.a) * (1.0f / std::max(1e-3f, B.length()));
      if (std::fabs(dot(da, db)) < 0.98f) continue;
      V2 na(-da.z, da.x);
      V2 mb = (B.a + B.b) * 0.5f;
      if (std::fabs(dot(mb - A.a, na)) > 0.4f) continue;
      float t0 = dot(B.a - A.a, da), t1 = dot(B.b - A.a, da);
      if (std::min(std::max(t0, t1), A.length()) - std::max(std::min(t0, t1), 0.0f) > 0.3f) ++e.duplicates;
    }
  return e;
}

static void printLayout(const char* tag, const Layout& l, const WallError& e, double ms) {
  int doors = 0, windows = 0;
  for (const LayoutWall& w : l.walls)
    for (const LayoutOpening& op : w.openings) (op.kind == LayoutOpening::Door ? doors : windows)++;
  std::printf("  %s: %.0f ms, floor %.3f ceiling %.3f, theta %.2f deg, %zu walls (rms %.3f max %.3f dup %d), "
              "%d doors, %d windows, %zu rooms %.1f m2, %zu objects, %d submaps, %d loop edges (%d rejected), "
              "max corr %.2f m %.2f deg\n",
              tag, ms, l.floor_y, l.ceiling_y, l.theta / kDeg2Rad, l.walls.size(), e.rms, e.max, e.duplicates, doors,
              windows, l.rooms.size(), l.totalArea(), l.objects.size(), l.submaps, l.loop_edges, l.rejected_edges,
              l.max_correction_m, l.max_correction_deg);
}

static double elapsedMs(int64_t since_ns) { return (nowNs() - since_ns) * 1e-6; }

static void testLayoutApartment() {
  std::printf("layout: synthetic apartment tour, no drift\n");
  MapStore store;
  store.reset(1);
  MappingEngine m(&store);
  syntheticScan(m, 0, 0, 3);
  LayoutEngine le(&store);
  LayoutEngine::Options o;
  o.max_icp = -1;
  o.rounds = 2;
  o.passes = 3;
  int64_t t0 = nowNs();
  const Layout& l = le.update(o);
  WallError e = wallError(l);
  printLayout("model", l, e, elapsedMs(t0));
  CHECK(l.valid);
  CHECK(std::fabs(l.floor_y) < 0.05f);
  CHECK(!l.ceiling_estimated && std::fabs(l.ceiling_y - mock::Apartment::kCeiling) < 0.06f);
  float th = std::fmod(l.theta, kPi / 2);
  CHECK(std::min(th, kPi / 2 - th) < 1.0f * kDeg2Rad);
  CHECK(l.walls.size() >= 9 && l.walls.size() <= 22);
  CHECK(e.rms < 0.04f);
  CHECK(e.max < 0.1f);  // no phantom walls behind the mirrors
  CHECK(e.duplicates == 0);
  bool corridor_wall = false;
  int doors = 0, windows = 0;
  for (const LayoutWall& w : l.walls) {
    V2 mid = (w.a + w.b) * 0.5f;
    if (std::fabs(w.a.z - w.b.z) < 0.05f && std::fabs(mid.z - 4.5f) < 0.05f && w.faces == 2 &&
        w.thickness > 0.06f && w.thickness < 0.16f && w.length() > 6.0f)
      corridor_wall = true;
    for (const LayoutOpening& op : w.openings) (op.kind == LayoutOpening::Door ? doors : windows)++;
  }
  CHECK(corridor_wall);
  CHECK(doors >= 5 && doors <= 8);
  CHECK(windows >= 3);
  CHECK(l.rooms.size() == 6);
  CHECK(l.totalArea() > 68.0f && l.totalArea() < 80.0f);
  CHECK(l.objects.size() >= 6);
  std::string json = LayoutEngine::toJson(l);
  CHECK(json.find("\"walls\":[{") != std::string::npos && json.find("\"k\":\"door\"") != std::string::npos);
}

// Drift makes the walls seen on the way back land elsewhere; the pose graph must pull them back.
static void testLayoutDriftCorrection() {
  std::printf("layout: drift correction (0.12 deg/m heading, 0.8%% scale)\n");
  MapStore store;
  store.reset(1);
  MappingEngine m(&store);
  syntheticScan(m, 0.12f, 0.008f, 5);
  LayoutEngine raw(&store);
  LayoutEngine::Options o;
  o.optimize = false;
  o.apply = false;
  o.passes = 3;
  int64_t t0 = nowNs();
  WallError before = wallError(raw.update(o));
  printLayout("uncorrected", raw.layout(), before, elapsedMs(t0));
  LayoutEngine fixed(&store);
  o.optimize = true;
  o.apply = true;
  o.max_icp = -1;
  o.rounds = 4;
  t0 = nowNs();
  const Layout& l = fixed.update(o);
  WallError after = wallError(l);
  printLayout("corrected", l, after, elapsedMs(t0));
  CHECK(l.loop_edges >= 1);
  CHECK(after.rms < 0.06f);
  CHECK(after.rms < 0.6f * before.rms);
  CHECK(after.duplicates <= before.duplicates);
  bool applied = false;
  for (const auto& s : store.submaps) applied = applied || std::fabs(s->correction.t.x) + std::fabs(s->correction.t.z) > 1e-3f;
  CHECK(applied);
}

int main() {
  testLd06RoundTrip();
  testStreamParserResync();
  testProtocol();
  testClockSync();
  testPoseBuffer();
  testMappingDedup();
  testPlyRoundTrip();
  testLayoutApartment();
  testLayoutDriftCorrection();
  testMockEndToEnd();
  testMockFollowsDeviceMotion();
  std::printf(g_failures ? "FAILED (%d)\n" : "ALL TESTS PASSED\n", g_failures);
  return g_failures ? 1 : 0;
}
