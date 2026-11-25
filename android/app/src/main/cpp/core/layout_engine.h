#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "map_store.h"

namespace ls {

// Point of the floor plane: (x, z) of the Y-up world frame. Plan view: x right, z down.
struct V2 {
  float x = 0, z = 0;
  V2() = default;
  V2(float x_, float z_) : x(x_), z(z_) {}
  V2 operator+(const V2& o) const { return {x + o.x, z + o.z}; }
  V2 operator-(const V2& o) const { return {x - o.x, z - o.z}; }
  V2 operator*(float s) const { return {x * s, z * s}; }
};
inline float dot(const V2& a, const V2& b) { return a.x * b.x + a.z * b.z; }
inline float len(const V2& a) { return std::sqrt(dot(a, a)); }
// Rotation about +Y acting on (x, z), same convention as axisAngle({0, 1, 0}, a).
inline V2 rot(const V2& p, float a) {
  float c = std::cos(a), s = std::sin(a);
  return {c * p.x + s * p.z, -s * p.x + c * p.z};
}
// Derivative of rot(p, a) at a = 0.
inline V2 perp(const V2& p) { return {p.z, -p.x}; }

// Planar rigid transform p -> rot(p, a) + t.
struct Rigid2 {
  float a = 0;
  V2 t;
  V2 apply(const V2& p) const { return rot(p, a) + t; }
};

struct LayoutOpening {
  enum Kind : int { Door = 0, Window = 1 };
  Kind kind = Door;
  float t0 = 0, t1 = 0;       // along the wall from its start point |a| (m)
  float bottom = 0, top = 0;  // above the floor (m)
};

struct LayoutWall {
  V2 a, b;  // centerline end points
  float thickness = 0.12f;
  float height = 2.5f;
  bool joined_a = false, joined_b = false;  // end meets another wall (corner / T junction)
  float ext_a = 0, ext_b = 0;               // extra length drawn past each end so corners close
  int faces = 1;                            // 2 = both faces observed (measured thickness)
  std::vector<LayoutOpening> openings;
  float length() const { return len(b - a); }
};

struct LayoutObject {
  enum Kind : int { Low = 0, Table = 1, Tall = 2 };
  V2 center;
  float width = 0, depth = 0, height = 0;  // width along the plan u axis, depth along v
  float angle = 0;                         // plan axes angle (Layout::theta)
  Kind kind = Low;
};

struct LayoutRoom {
  float area = 0;
  V2 label;                   // most interior point (world)
  float width = 0, depth = 0;  // extent along the plan axes (m)
  std::vector<std::array<float, 4>> rects;  // u0, v0, u1, v1 in the plan frame
};

// Structured model of the scanned space (corrected world frame, metres).
struct Layout {
  bool valid = false;
  float floor_y = 0, ceiling_y = 2.5f;
  bool ceiling_estimated = true;
  // Dominant (Manhattan) axes: u = (cos, sin), v = (-sin, cos) in (x, z).
  float theta = 0;
  std::vector<LayoutWall> walls;
  std::vector<LayoutObject> objects;
  std::vector<LayoutRoom> rooms;
  bool has_device = false;
  V2 device, device_fwd;
  // Maps the odometry world (MapStore::device, ScanStats) onto this layout.
  Rigid2 latest_correction;
  // Diagnostics of the drift correction and regularization.
  int submaps = 0, loop_edges = 0, rejected_edges = 0, merged_faces = 0, closed_corners = 0;
  float max_correction_m = 0, max_correction_deg = 0;

  float totalArea() const {
    float a = 0;
    for (const auto& r : rooms) a += r.area;
    return a;
  }
};

// Builds a RoomPlan-like model (walls with doors and windows, furniture boxes, rooms) from the
// anchored submaps of a MapStore.
//
// Odometry drift makes revisited walls appear twice. Before extracting the model the engine
// estimates a planar correction per submap with a pose graph: an odometry chain between
// consecutive submaps (trusted locally, weaker with distance) plus loop edges from
// point-to-line ICP between non-consecutive submaps that see the same structure. Loop edges
// carry the ICP information matrix (so a corridor only constrains across it) and outliers are
// rejected by their chi-square. Remaining errors are regularized while extracting: walls are
// snapped to the dominant axes, duplicated faces merged and corners closed.
//
// Not thread-safe: one caller at a time (layout thread while scanning, then stop()).
class LayoutEngine {
 public:
  struct Options {
    bool optimize = true;  // estimate drift corrections
    int max_icp = 40;      // ICP alignments per update (< 0: unlimited)
    int rounds = 1;        // measure + solve rounds
    bool apply = true;     // write the corrections into the MapStore submaps
    // Extraction passes. Phantom returns (mirrors) are rejected using the walls of the previous
    // pass as occluders, so a final, standalone computation should use 2.
    int passes = 1;
  };

  explicit LayoutEngine(MapStore* store) : store_(store) {}
  void reset();
  const Layout& update(const Options& o);
  const Layout& layout() const { return layout_; }
  // Corrections as 3D poses, one per submap (Submap::correction).
  std::vector<Pose> corrections() const;
  static std::string toJson(const Layout& l);
  // Prints loop-edge measurements and the extracted walls / rooms (native tests).
  static bool verbose;

 private:
  struct Cell {
    uint64_t all = 0, hi = 0;  // occupied height bins (hi: intensity >= threshold)
    uint32_t count = 0;
    float sx = 0, sz = 0;  // point sums (centroid)
  };
  struct Sub {
    Pose created, current;
    uint32_t generation = 0;
    size_t processed = 0;
    std::unordered_map<uint64_t, Cell> cells;  // creation-world frame
    int ix0 = 1 << 30, iz0 = 1 << 30, ix1 = -(1 << 30), iz1 = -(1 << 30);
    std::vector<V2> traj;  // creation-world frame
    size_t traj_seen = 0;
    uint64_t cells_ver = 0, pose_ver = 0;
    Rigid2 D;  // creation world -> odometry world (anchor updates)
    V2 pivot;  // rotation centre of the correction (odometry world)
    float odo = 0;  // distance walked from the first submap (pivot to pivot)
    double x[3] = {0, 0, 0};  // correction: tx, tz, theta about |pivot|
    uint64_t sp_ver = ~0ULL;
    float sp_floor = 1e9f;
    std::vector<V2> sp, sn;  // structure points + oriented normals (creation world)
    Rigid2 total() const;    // creation world -> corrected world
  };
  struct LoopEdge {
    uint64_t ver_i = 0, ver_j = 0;
    double xi[3] = {0, 0, 0}, xj[3] = {0, 0, 0};  // node states when measured
    bool ok = false;
    bool active = true;
    double A_i[9], A_j[9], z[3], info[9];
  };
  // Pose-graph node: consecutive submaps sharing one correction.
  struct Node {
    int first = 0, last = 0;  // submap range
    V2 pivot;
    float odo = 0;
    double x[3] = {0, 0, 0};
    uint64_t ver = 0;
    size_t points = 0;
  };

  void snapshot();
  void refreshStructure(Sub& s);
  void buildNodes();
  void applyNodes();
  void gather(const Node& nd, std::vector<V2>& pts, std::vector<V2>& nrm) const;
  bool measure(int ni, int nj, LoopEdge& e);
  void optimize(const Options& o);
  bool solve(const std::vector<std::array<int, 4>>& loops);
  void publish();
  void extract();

  MapStore* store_;
  uint64_t session_ = ~0ULL;
  float floor_y_ = -1.4f;
  bool has_device_ = false;
  Pose device_;
  std::vector<Sub> subs_;
  std::vector<Node> nodes_;
  std::map<std::pair<int, int>, LoopEdge> edges_;  // keyed by the nodes' first submaps
  Layout layout_;
  int loop_edges_ = 0, rejected_ = 0;
};

}  // namespace ls
