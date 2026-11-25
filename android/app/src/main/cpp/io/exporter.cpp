#include "exporter.h"

#include <cstdio>
#include <functional>

namespace ls::exporter {
namespace {

// Iterates every map point in world Z-up coordinates. Holds the store lock while copying one
// submap at a time so the renderer is never blocked for long.
void forEachPoint(MapStore& store, const std::function<void(const Vec3&, float, uint8_t)>& fn) {
  size_t n;
  {
    std::lock_guard<std::mutex> l(store.mu);
    n = store.submaps.size();
  }
  std::vector<MapPoint> pts;
  for (size_t i = 0; i < n; ++i) {
    Pose p;
    {
      std::lock_guard<std::mutex> l(store.mu);
      pts = store.submaps[i]->points;
      p = store.submaps[i]->worldPose();
    }
    for (const MapPoint& mp : pts) fn(toZUp(p.apply({mp.x, mp.y, mp.z})), mp.t, mp.intensity);
  }
}

}  // namespace

Vec3 toZUp(const Vec3& p) { return {p.x, -p.z, p.y}; }
Quat toZUp(const Quat& q) {
  // Rotation C mapping Y-up to Z-up is +90 deg about X; q' = C q C^-1.
  Quat c = axisAngle({1, 0, 0}, kPi / 2);
  return normalize(c * q * conjugate(c));
}

bool writePly(const std::string& path, MapStore& store, int64_t& count) {
  int64_t total;
  {
    std::lock_guard<std::mutex> l(store.mu);
    total = store.total_points;
  }
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  // Header is rewritten at the end with the exact vertex count.
  auto header = [&](int64_t n) {
    std::fprintf(f,
                 "ply\nformat binary_little_endian 1.0\ncomment lidarscanner preview cloud (Z up, meters)\n"
                 "element vertex %012lld\nproperty float x\nproperty float y\nproperty float z\n"
                 "property uchar intensity\nproperty float scalar_time\nend_header\n",
                 static_cast<long long>(n));
  };
  header(total);
  count = 0;
  forEachPoint(store, [&](const Vec3& p, float t, uint8_t in) {
    float xyz[3] = {p.x, p.y, p.z};
    std::fwrite(xyz, sizeof(float), 3, f);
    std::fwrite(&in, 1, 1, f);
    std::fwrite(&t, sizeof(float), 1, f);
    ++count;
  });
  std::fseek(f, 0, SEEK_SET);
  header(count);
  std::fclose(f);
  return true;
}

bool writePcd(const std::string& path, MapStore& store, int64_t& count) {
  std::vector<float> data;
  forEachPoint(store, [&](const Vec3& p, float t, uint8_t in) {
    data.push_back(p.x);
    data.push_back(p.y);
    data.push_back(p.z);
    data.push_back(static_cast<float>(in));
    data.push_back(t);
  });
  count = static_cast<int64_t>(data.size() / 5);
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::fprintf(f,
               "# .PCD v0.7 - lidarscanner preview cloud (Z up, meters)\nVERSION 0.7\nFIELDS x y z intensity time\n"
               "SIZE 4 4 4 4 4\nTYPE F F F F F\nCOUNT 1 1 1 1 1\nWIDTH %lld\nHEIGHT 1\n"
               "VIEWPOINT 0 0 0 1 0 0 0\nPOINTS %lld\nDATA binary\n",
               static_cast<long long>(count), static_cast<long long>(count));
  std::fwrite(data.data(), sizeof(float), data.size(), f);
  std::fclose(f);
  return true;
}

bool writeTrajectoryCsv(const std::string& path, const std::vector<TrajectorySample>& traj) {
  FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return false;
  std::fprintf(f, "t_s,x,y,z,qx,qy,qz,qw\n");
  for (const auto& s : traj) {
    Vec3 p = toZUp(s.pose.t);
    Quat q = toZUp(s.pose.q);
    std::fprintf(f, "%.4f,%.4f,%.4f,%.4f,%.6f,%.6f,%.6f,%.6f\n", s.t_s, p.x, p.y, p.z, q.x, q.y, q.z, q.w);
  }
  std::fclose(f);
  return true;
}

bool writeTrajectoryJson(const std::string& path, const std::vector<TrajectorySample>& traj) {
  FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return false;
  std::fprintf(f, "{\"frame\":\"z_up\",\"units\":\"m\",\"poses\":[");
  for (size_t i = 0; i < traj.size(); ++i) {
    Vec3 p = toZUp(traj[i].pose.t);
    Quat q = toZUp(traj[i].pose.q);
    std::fprintf(f, "%s{\"t\":%.4f,\"p\":[%.4f,%.4f,%.4f],\"q\":[%.6f,%.6f,%.6f,%.6f]}", i ? "," : "",
                 traj[i].t_s, p.x, p.y, p.z, q.x, q.y, q.z, q.w);
  }
  std::fprintf(f, "]}\n");
  std::fclose(f);
  return true;
}

}  // namespace ls::exporter
