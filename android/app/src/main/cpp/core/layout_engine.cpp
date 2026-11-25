#include "layout_engine.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <functional>
#include <random>

namespace ls {
namespace {

constexpr float kRes = 0.05f;        // plan grid (m)
constexpr float kBinH = 0.10f;       // height bins (m)
constexpr float kBinY0 = -3.2f;      // bottom of bin 0
constexpr int kBins = 64;
constexpr int kMaxGrid = 800;        // 40 m per side
constexpr uint8_t kHiIntensity = 50;  // below: glass-like / weak returns
constexpr float kDefaultThickness = 0.12f;
constexpr float kDeg = kDeg2Rad;

int binOf(float y) { return static_cast<int>(std::floor((y - kBinY0) / kBinH)); }
float binLo(int b) { return kBinY0 + b * kBinH; }
float binCenter(int b) { return kBinY0 + (b + 0.5f) * kBinH; }
uint64_t band(float y0, float y1) {
  uint64_t m = 0;
  for (int b = 0; b < kBins; ++b) {
    float c = binCenter(b);
    if (c >= y0 && c <= y1) m |= 1ULL << b;
  }
  return m;
}
int pop(uint64_t m) { return __builtin_popcountll(m); }
int lowestBit(uint64_t m) { return m ? __builtin_ctzll(m) : -1; }
int highestBit(uint64_t m) { return m ? 63 - __builtin_clzll(m) : -1; }
uint64_t cellKey(int ix, int iz) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(ix)) << 32) | static_cast<uint32_t>(iz);
}
int keyX(uint64_t k) { return static_cast<int32_t>(k >> 32); }
int keyZ(uint64_t k) { return static_cast<int32_t>(k & 0xffffffffu); }
int cellOf(float v) { return static_cast<int>(std::floor(v / kRes)); }
// Angle between two undirected lines, in [0, pi/2].
float lineDiff(float a, float b) {
  float d = std::fmod(std::fabs(a - b), kPi);
  return std::min(d, kPi - d);
}
float normLine(float a) {
  a = std::fmod(a, kPi);
  return a < 0 ? a + kPi : a;
}
// Line direction / normal for a line angle (atan2(z, x) convention).
V2 dirOf(float a) { return {std::cos(a), std::sin(a)}; }
V2 normalOf(float a) { return {-std::sin(a), std::cos(a)}; }

// Principal direction of a 2D scatter: returns (line angle in [0, pi), linearity in [0, 1]).
std::pair<float, float> pca(double sxx, double sxz, double szz) {
  double tr = sxx + szz, det = sxx * szz - sxz * sxz;
  double disc = std::sqrt(std::max(0.0, tr * tr / 4 - det));
  double l1 = tr / 2 + disc, l2 = tr / 2 - disc;
  float a = normLine(static_cast<float>(0.5 * std::atan2(2 * sxz, sxx - szz)));
  return {a, l1 > 1e-12 ? static_cast<float>(1.0 - std::max(0.0, l2) / l1) : 0.0f};
}

// --- 3x3 (row-major, double) helpers for the pose graph -----------------------------------
void m3set(double* m, double a, double b, double c, double d, double e, double f, double g, double h, double i) {
  m[0] = a; m[1] = b; m[2] = c; m[3] = d; m[4] = e; m[5] = f; m[6] = g; m[7] = h; m[8] = i;
}
void m3mul(const double* a, const double* b, double* o) {
  double r[9];
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) r[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
  std::copy(r, r + 9, o);
}
void m3tmul(const double* a, const double* b, double* o) {  // a^T b
  double r[9];
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) r[i * 3 + j] = a[i] * b[j] + a[3 + i] * b[3 + j] + a[6 + i] * b[6 + j];
  std::copy(r, r + 9, o);
}
void m3vec(const double* m, const double* v, double* o) {
  double r[3];
  for (int i = 0; i < 3; ++i) r[i] = m[i * 3] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
  std::copy(r, r + 3, o);
}
void m3tvec(const double* m, const double* v, double* o) {  // m^T v
  double r[3];
  for (int i = 0; i < 3; ++i) r[i] = m[i] * v[0] + m[3 + i] * v[1] + m[6 + i] * v[2];
  std::copy(r, r + 3, o);
}
bool m3inv(const double* m, double* o) {
  double c0 = m[4] * m[8] - m[5] * m[7], c1 = m[5] * m[6] - m[3] * m[8], c2 = m[3] * m[7] - m[4] * m[6];
  double det = m[0] * c0 + m[1] * c1 + m[2] * c2;
  if (std::fabs(det) < 1e-18) return false;
  double id = 1.0 / det;
  o[0] = c0 * id;
  o[1] = (m[2] * m[7] - m[1] * m[8]) * id;
  o[2] = (m[1] * m[5] - m[2] * m[4]) * id;
  o[3] = c1 * id;
  o[4] = (m[0] * m[8] - m[2] * m[6]) * id;
  o[5] = (m[2] * m[3] - m[0] * m[5]) * id;
  o[6] = c2 * id;
  o[7] = (m[1] * m[6] - m[0] * m[7]) * id;
  o[8] = (m[0] * m[4] - m[1] * m[3]) * id;
  return true;
}
// Jacobian of "displacement at m, rotation" with respect to a correction (tx, tz, theta) about c.
void relJacobian(const V2& m, const V2& c, double sign, double* A) {
  V2 r = m - c;
  m3set(A, sign, 0, sign * r.z, 0, sign, -sign * r.x, 0, 0, sign);
}

// Jacobi eigen-decomposition of a symmetric 3x3 matrix: values |w|, vectors in the columns of |v|.
void eig3(const double* a, double* w, double* v) {
  double m[9];
  std::copy(a, a + 9, m);
  m3set(v, 1, 0, 0, 0, 1, 0, 0, 0, 1);
  for (int sweep = 0; sweep < 30; ++sweep) {
    double off = m[1] * m[1] + m[2] * m[2] + m[5] * m[5];
    if (off < 1e-30) break;
    for (int p = 0; p < 2; ++p)
      for (int q = p + 1; q < 3; ++q) {
        double apq = m[p * 3 + q];
        if (std::fabs(apq) < 1e-300) continue;
        double theta = (m[q * 3 + q] - m[p * 3 + p]) / (2 * apq);
        double t = (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1));
        double c = 1 / std::sqrt(t * t + 1), s = t * c;
        for (int k = 0; k < 3; ++k) {
          double mkp = m[k * 3 + p], mkq = m[k * 3 + q];
          m[k * 3 + p] = c * mkp - s * mkq;
          m[k * 3 + q] = s * mkp + c * mkq;
        }
        for (int k = 0; k < 3; ++k) {
          double mpk = m[p * 3 + k], mqk = m[q * 3 + k];
          m[p * 3 + k] = c * mpk - s * mqk;
          m[q * 3 + k] = s * mpk + c * mqk;
        }
        for (int k = 0; k < 3; ++k) {
          double vkp = v[k * 3 + p], vkq = v[k * 3 + q];
          v[k * 3 + p] = c * vkp - s * vkq;
          v[k * 3 + q] = s * vkp + c * vkq;
        }
      }
  }
  w[0] = m[0];
  w[1] = m[4];
  w[2] = m[8];
}

// Solves H d = -g only along well-constrained directions (rotation scaled by the lever arm
// |rho| so both are in metres): a corridor slides freely along its axis, so no update there.
void solveConstrained(const double* H, const double* g, double rho, double* d) {
  const double S[3] = {1, 1, 1 / rho};
  double Hs[9], gs[3], w[3], v[9];
  for (int r = 0; r < 3; ++r) {
    gs[r] = g[r] * S[r];
    for (int c = 0; c < 3; ++c) Hs[r * 3 + c] = H[r * 3 + c] * S[r] * S[c];
  }
  eig3(Hs, w, v);
  double wmax = std::max(w[0], std::max(w[1], w[2]));
  double ds[3] = {0, 0, 0};
  for (int k = 0; k < 3; ++k) {
    if (w[k] < 0.05 * wmax || w[k] <= 0) continue;
    double proj = v[k] * gs[0] + v[3 + k] * gs[1] + v[6 + k] * gs[2];
    for (int r = 0; r < 3; ++r) ds[r] -= proj / w[k] * v[r * 3 + k];
  }
  for (int r = 0; r < 3; ++r) d[r] = ds[r] * S[r];
}

Rigid2 correctionOf(const V2& pivot, const double* x) {
  float th = static_cast<float>(x[2]);
  return {th, pivot + V2(static_cast<float>(x[0]), static_cast<float>(x[1])) - rot(pivot, th)};
}

// Where the scanner was when it saw |p|: the LD06 scans the plane perpendicular to the walking
// direction, so prefer path points whose transverse plane contains |p| (nearest among them).
// Plain nearest-point would pick the wrong side of a wall when the path crosses it (doors).
V2 viewpoint(const std::vector<V2>& traj, const V2& p, const V2& fallback) {
  float best = 1e18f, best_any = 1e18f;
  V2 out = fallback, any = fallback;
  const size_t n = traj.size();
  for (size_t k = 0; k < n; ++k) {
    V2 d = p - traj[k];
    float d2 = dot(d, d);
    if (d2 < best_any) {
      best_any = d2;
      any = traj[k];
    }
    V2 f = traj[std::min(n - 1, k + 1)] - traj[k > 0 ? k - 1 : 0];
    float fl = len(f);
    if (fl > 0.02f && std::fabs(dot(d, f)) / fl > 0.35f) continue;
    if (d2 < best) {
      best = d2;
      out = traj[k];
    }
  }
  return best < 1e17f ? out : any;
}

// Writes a float with fixed decimals into a JSON buffer.
void num(std::string& o, float v, int decimals = 3) {
  char b[32];
  if (!std::isfinite(v)) v = 0;
  std::snprintf(b, sizeof(b), "%.*f", decimals, v);
  o += b;
}

}  // namespace

bool LayoutEngine::verbose = false;

Rigid2 LayoutEngine::Sub::total() const {
  float th = static_cast<float>(x[2]);
  Rigid2 r;
  r.a = D.a + th;
  r.t = rot(D.t - pivot, th) + pivot + V2(static_cast<float>(x[0]), static_cast<float>(x[1]));
  return r;
}

void LayoutEngine::reset() {
  session_ = ~0ULL;
  subs_.clear();
  nodes_.clear();
  edges_.clear();
  layout_ = Layout{};
  loop_edges_ = rejected_ = 0;
}

std::vector<Pose> LayoutEngine::corrections() const {
  std::vector<Pose> out;
  for (const Sub& s : subs_) {
    Rigid2 c = correctionOf(s.pivot, s.x);
    Pose p;
    p.q = axisAngle({0, 1, 0}, c.a);
    p.t = {c.t.x, 0, c.t.z};
    out.push_back(p);
  }
  return out;
}

const Layout& LayoutEngine::update(const Options& o) {
  snapshot();
  if (o.optimize) optimize(o);
  if (o.apply) publish();
  for (int k = 0; k < std::max(1, o.passes); ++k) extract();
  return layout_;
}

// --- ingestion ----------------------------------------------------------------------------

void LayoutEngine::snapshot() {
  struct Pending {
    size_t i;
    std::vector<MapPoint> pts;
    std::vector<Vec3> traj;
    bool traj_reset = false;
  };
  std::vector<Pending> pend;
  {
    std::lock_guard<std::mutex> l(store_->mu);
    if (store_->session_id != session_) {
      session_ = store_->session_id;
      subs_.clear();
      nodes_.clear();
      edges_.clear();
      layout_ = Layout{};
      loop_edges_ = rejected_ = 0;
    }
    floor_y_ = store_->floor_y;
    has_device_ = store_->has_device;
    device_ = store_->device.pose;
    for (size_t i = 0; i < store_->submaps.size(); ++i) {
      const Submap& s = *store_->submaps[i];
      if (i >= subs_.size()) {
        subs_.emplace_back();
        subs_.back().created = s.created_pose;
        subs_.back().generation = s.generation;
        subs_.back().current = s.current_pose;
        subs_.back().pose_ver = ~0ULL;  // forces D initialization below
      }
      Sub& d = subs_[i];
      d.current = s.current_pose;
      if (s.generation != d.generation) {
        // Decimated: already-ingested cells stay valid; skip the rebuilt vector.
        d.generation = s.generation;
        d.processed = s.points.size();
      }
      Pending p;
      p.i = i;
      if (s.points.size() > d.processed) {
        p.pts.assign(s.points.begin() + static_cast<long>(d.processed), s.points.end());
        d.processed = s.points.size();
      }
      if (s.trajectory.size() > d.traj_seen) {
        p.traj.assign(s.trajectory.begin() + static_cast<long>(d.traj_seen), s.trajectory.end());
      } else if (s.trajectory.size() < d.traj_seen) {
        p.traj = s.trajectory;
        p.traj_reset = true;
      }
      d.traj_seen = s.trajectory.size();
      if (!p.pts.empty() || !p.traj.empty() || p.traj_reset) pend.push_back(std::move(p));
    }
  }

  for (Pending& p : pend) {
    Sub& d = subs_[p.i];
    for (const MapPoint& mp : p.pts) {
      Vec3 w = d.created.apply({mp.x, mp.y, mp.z});
      int b = binOf(w.y);
      if (b < 0 || b >= kBins) continue;
      int ix = cellOf(w.x), iz = cellOf(w.z);
      Cell& c = d.cells[cellKey(ix, iz)];
      uint64_t bit = 1ULL << b;
      c.all |= bit;
      if (mp.intensity >= kHiIntensity) c.hi |= bit;
      ++c.count;
      c.sx += w.x - ix * kRes;
      c.sz += w.z - iz * kRes;
      d.ix0 = std::min(d.ix0, ix);
      d.iz0 = std::min(d.iz0, iz);
      d.ix1 = std::max(d.ix1, ix);
      d.iz1 = std::max(d.iz1, iz);
    }
    if (!p.pts.empty()) ++d.cells_ver;
    if (p.traj_reset) d.traj.clear();
    for (const Vec3& v : p.traj) {
      Vec3 w = d.created.apply(v);
      d.traj.emplace_back(w.x, w.z);
    }
  }

  // Anchor updates (odometry corrections) move whole submaps: D = current * created^-1.
  for (size_t i = 0; i < subs_.size(); ++i) {
    Sub& d = subs_[i];
    Pose dp = d.current * d.created.inverse();
    Vec3 f = rotate(dp.q, {1, 0, 0});
    Rigid2 nd{std::atan2(-f.z, f.x), {dp.t.x, dp.t.z}};
    bool first = d.pose_ver == ~0ULL;
    if (first || std::fabs(nd.a - d.D.a) > 1e-5f || len(nd.t - d.D.t) > 1e-4f) {
      d.D = nd;
      V2 pivot(d.current.t.x, d.current.t.z);
      if (first && i > 0) {
        // Start from the previous submap's correction so new data appears where it belongs.
        const Sub& prev = subs_[i - 1];
        Rigid2 c = correctionOf(prev.pivot, prev.x);
        V2 t = c.apply(pivot) - pivot;
        d.x[0] = t.x;
        d.x[1] = t.z;
        d.x[2] = c.a;
      } else if (!first) {
        // The odometry backend re-anchored this submap: its old correction no longer applies.
        d.x[0] = d.x[1] = d.x[2] = 0;
      }
      d.pivot = pivot;
      d.pose_ver = first ? 0 : d.pose_ver + 1;
    }
  }
}

// Vertical structure (walls, furniture sides) with normals oriented towards the scanner.
void LayoutEngine::refreshStructure(Sub& s) {
  if (s.sp_ver == s.cells_ver && std::fabs(s.sp_floor - floor_y_) < 0.05f) return;
  s.sp_ver = s.cells_ver;
  s.sp_floor = floor_y_;
  s.sp.clear();
  s.sn.clear();
  // Walls dominate above 0.6 m; lower down, furniture seen from different sides would bias the
  // alignment.
  const uint64_t sb = band(floor_y_ + 0.6f, floor_y_ + 2.0f);
  std::unordered_map<uint64_t, V2> pts;
  for (const auto& [k, c] : s.cells) {
    if (c.count < 2 || pop(c.all & sb) < 3) continue;
    pts[k] = {keyX(k) * kRes + c.sx / c.count, keyZ(k) * kRes + c.sz / c.count};
  }
  V2 fallback(s.created.t.x, s.created.t.z);
  for (const auto& [k, p] : pts) {
    int ix = keyX(k), iz = keyZ(k);
    double mx = 0, mz = 0;
    int n = 0;
    V2 nb[25];
    for (int dz = -2; dz <= 2; ++dz)
      for (int dx = -2; dx <= 2; ++dx) {
        auto it = pts.find(cellKey(ix + dx, iz + dz));
        if (it == pts.end()) continue;
        nb[n++] = it->second;
        mx += it->second.x;
        mz += it->second.z;
      }
    if (n < 4) continue;
    mx /= n;
    mz /= n;
    double sxx = 0, sxz = 0, szz = 0;
    for (int q = 0; q < n; ++q) {
      double ex = nb[q].x - mx, ez = nb[q].z - mz;
      sxx += ex * ex;
      sxz += ex * ez;
      szz += ez * ez;
    }
    auto [ang, lin] = pca(sxx, sxz, szz);
    if (lin < 0.6f) continue;
    V2 nrm = normalOf(ang);
    V2 view = viewpoint(s.traj, p, fallback) - p;
    float vl = len(view);
    // Grazing views give unreliable sides; skip them.
    if (vl < 1e-3f || std::fabs(dot(nrm, view)) < 0.3f * vl) continue;
    if (dot(nrm, view) < 0) nrm = nrm * -1.0f;
    s.sp.push_back(p);
    s.sn.push_back(nrm);
  }
}

// --- drift correction ---------------------------------------------------------------------

// Groups consecutive submaps into pose-graph nodes (up to 1.5 m of walking, or a whole spin on
// the spot) so each node sees enough structure for a reliable alignment. Node corrections are
// kept across updates, keyed by their first submap.
void LayoutEngine::buildNodes() {
  std::map<int, std::array<double, 3>> prev;
  for (const Node& nd : nodes_) prev[nd.first] = {nd.x[0], nd.x[1], nd.x[2]};
  nodes_.clear();
  const int n = static_cast<int>(subs_.size());
  float odo = 0;
  for (int i = 0; i < n; ++i) {
    if (i > 0) odo += len(subs_[i].pivot - subs_[i - 1].pivot);
    subs_[i].odo = odo;
    if (nodes_.empty() || odo - nodes_.back().odo > 1.5f || i - nodes_.back().first >= 16) {
      Node nd;
      nd.first = nd.last = i;
      nd.pivot = subs_[i].pivot;
      nd.odo = odo;
      auto it = prev.find(i);
      if (it != prev.end()) {
        std::copy(it->second.begin(), it->second.end(), nd.x);
      } else {
        // New node: start from the first submap's current correction.
        Rigid2 c = correctionOf(subs_[i].pivot, subs_[i].x);
        V2 t = c.apply(nd.pivot) - nd.pivot;
        nd.x[0] = t.x;
        nd.x[1] = t.z;
        nd.x[2] = c.a;
      }
      nodes_.push_back(nd);
    } else {
      nodes_.back().last = i;
    }
  }
  for (Node& nd : nodes_) {
    nd.ver = 0;
    nd.points = 0;
    for (int s = nd.first; s <= nd.last; ++s) {
      nd.ver = nd.ver * 1000003ULL + subs_[s].cells_ver * 31 + subs_[s].pose_ver + 1;
      nd.points += subs_[s].sp.size();
    }
  }
}

// Copies each node's correction into its submaps (same rigid motion, expressed at each pivot).
void LayoutEngine::applyNodes() {
  for (const Node& nd : nodes_) {
    Rigid2 c = correctionOf(nd.pivot, nd.x);
    for (int s = nd.first; s <= nd.last; ++s) {
      Sub& sub = subs_[s];
      V2 t = c.apply(sub.pivot) - sub.pivot;
      sub.x[0] = t.x;
      sub.x[1] = t.z;
      sub.x[2] = c.a;
    }
  }
}

void LayoutEngine::gather(const Node& nd, std::vector<V2>& pts, std::vector<V2>& nrm) const {
  for (int s = nd.first; s <= nd.last; ++s) {
    const Sub& sub = subs_[s];
    Rigid2 T = sub.total();
    for (size_t k = 0; k < sub.sp.size(); ++k) {
      pts.push_back(T.apply(sub.sp[k]));
      nrm.push_back(rot(sub.sn[k], T.a));
    }
  }
}

// Point-to-line ICP of node ni onto node nj (both in the current corrected world). On success
// stores a linear constraint on both node corrections with the ICP information matrix.
bool LayoutEngine::measure(int ni, int nj, LoopEdge& e) {
  e.ok = false;
  const Node& na = nodes_[ni];
  const Node& nb = nodes_[nj];
  if (na.points < 30 || nb.points < 30) return false;
  std::vector<V2> P, NP, Q, NQ;
  gather(na, P, NP);
  gather(nb, Q, NQ);
  // Bound the cost: at most ~900 source points.
  if (P.size() > 900) {
    size_t step = (P.size() + 899) / 900;
    size_t w = 0;
    for (size_t k = 0; k < P.size(); k += step, ++w) {
      P[w] = P[k];
      NP[w] = NP[k];
    }
    P.resize(w);
    NP.resize(w);
  }
  const size_t np = P.size(), nq = Q.size();
  float qx0 = 1e9f, qz0 = 1e9f, qx1 = -1e9f, qz1 = -1e9f;
  for (const V2& q : Q) {
    qx0 = std::min(qx0, q.x);
    qz0 = std::min(qz0, q.z);
    qx1 = std::max(qx1, q.x);
    qz1 = std::max(qz1, q.z);
  }
  const float gr = 0.1f;
  const int gw = static_cast<int>((qx1 - qx0) / gr) + 1, gh = static_cast<int>((qz1 - qz0) / gr) + 1;
  if (gw <= 0 || gh <= 0 || static_cast<int64_t>(gw) * gh > 400000) return false;
  std::vector<int> head(static_cast<size_t>(gw) * gh, -1), next(nq, -1);
  for (size_t k = 0; k < nq; ++k) {
    int gx = static_cast<int>((Q[k].x - qx0) / gr), gz = static_cast<int>((Q[k].z - qz0) / gr);
    size_t g = static_cast<size_t>(gz) * gw + gx;
    next[k] = head[g];
    head[g] = static_cast<int>(k);
  }
  auto nearest = [&](const V2& p, const V2& n, float r) {
    int cx = static_cast<int>(std::floor((p.x - qx0) / gr)), cz = static_cast<int>(std::floor((p.z - qz0) / gr));
    int rr = static_cast<int>(std::ceil(r / gr));
    float best = r * r;
    int bi = -1;
    for (int dz = -rr; dz <= rr; ++dz) {
      int gz = cz + dz;
      if (gz < 0 || gz >= gh) continue;
      for (int dx = -rr; dx <= rr; ++dx) {
        int gx = cx + dx;
        if (gx < 0 || gx >= gw) continue;
        for (int k = head[static_cast<size_t>(gz) * gw + gx]; k >= 0; k = next[k]) {
          // Faces seen from opposite sides (the two faces of a thin wall) never match.
          if (dot(n, NQ[k]) < 0.8f) continue;
          V2 d = p - Q[k];
          float d2 = dot(d, d);
          if (d2 < best) {
            best = d2;
            bi = k;
          }
        }
      }
    }
    return bi;
  };

  V2 m;
  int overlap = 0;
  for (size_t k = 0; k < np; ++k)
    if (nearest(P[k], NP[k], 0.6f) >= 0) {
      m = m + P[k];
      ++overlap;
    }
  if (overlap < 30) return false;
  m = m * (1.0f / overlap);

  float ta = 0;
  V2 tt;
  auto cur = [&](size_t k) { return rot(P[k] - m, ta) + m + tt; };
  double rho = 0;
  for (size_t k = 0; k < np; ++k) rho += dot(P[k] - m, P[k] - m);
  rho = std::max(0.5, std::sqrt(rho / np));
  for (int it = 0; it < 28; ++it) {
    float r = it < 6 ? 0.6f : it < 12 ? 0.3f : it < 20 ? 0.15f : 0.08f;
    double H[9] = {0}, g[3] = {0};
    int pairs = 0;
    for (size_t k = 0; k < np; ++k) {
      V2 p = cur(k);
      int q = nearest(p, rot(NP[k], ta), r);
      if (q < 0) continue;
      double res = dot(p - Q[q], NQ[q]);
      double w = std::fabs(res) < 0.03 ? 1.0 : 0.03 / std::fabs(res);  // Huber
      double a[3] = {NQ[q].x, NQ[q].z, dot(perp(p - m), NQ[q])};
      for (int r0 = 0; r0 < 3; ++r0) {
        for (int c0 = 0; c0 < 3; ++c0) H[r0 * 3 + c0] += w * a[r0] * a[c0];
        g[r0] += w * a[r0] * res;
      }
      ++pairs;
    }
    if (pairs < 20) return false;
    double d[3];
    solveConstrained(H, g, rho, d);
    ta += static_cast<float>(d[2]);
    tt = rot(tt, static_cast<float>(d[2])) + V2(static_cast<float>(d[0]), static_cast<float>(d[1]));
    if (it >= 20 && std::fabs(d[0]) + std::fabs(d[1]) < 2e-4 && std::fabs(d[2]) < 2e-4) break;
  }

  double H[9] = {0};
  int inliers = 0;
  double sse = 0;
  for (size_t k = 0; k < np; ++k) {
    V2 p = cur(k);
    int q = nearest(p, rot(NP[k], ta), 0.08f);
    if (q < 0) continue;
    double res = dot(p - Q[q], NQ[q]);
    if (std::fabs(res) >= 0.03) continue;
    double a[3] = {NQ[q].x, NQ[q].z, dot(perp(p - m), NQ[q])};
    for (int r0 = 0; r0 < 3; ++r0)
      for (int c0 = 0; c0 < 3; ++c0) H[r0 * 3 + c0] += a[r0] * a[c0];
    sse += res * res;
    ++inliers;
  }
  double rms = inliers > 0 ? std::sqrt(sse / inliers) : 1.0;
  auto reject = [&](const char* why) {
    if (verbose)
      std::printf("    icp %d->%d (%.2f,%.2f)->(%.2f,%.2f) rejected (%s): overlap %d inliers %d rms %.3f t (%.3f %.3f) a %.2f\n",
                  ni, nj, na.pivot.x, na.pivot.z, nb.pivot.x, nb.pivot.z, why, overlap, inliers, rms, tt.x, tt.z, ta / kDeg);
    return false;
  };
  if (inliers < 40 || inliers < 0.5 * overlap) return reject("support");
  if (rms > 0.03 || std::fabs(ta) > 8.0f * kDeg || len(tt) > 0.8f) return reject("fit");

  // Neighbouring cells are strongly correlated: cap the effective number of observations.
  const double sigma = 0.02;
  double scale = std::min(1.0, 30.0 / inliers) / (sigma * sigma);
  for (int k = 0; k < 9; ++k) e.info[k] = H[k] * scale;
  relJacobian(m, na.pivot, 1.0, e.A_i);
  relJacobian(m, nb.pivot, -1.0, e.A_j);
  double ri[3], rj[3];
  m3vec(e.A_i, na.x, ri);
  m3vec(e.A_j, nb.x, rj);
  e.z[0] = ri[0] + rj[0] + tt.x;
  e.z[1] = ri[1] + rj[1] + tt.z;
  e.z[2] = ri[2] + rj[2] + ta;
  // Odometry cannot have drifted arbitrarily between the two nodes: gate the total relative
  // correction by the distance walked between them (rejects aliased, wrong convergences).
  double walked = std::fabs(na.odo - nb.odo);
  double max_t = 0.06 + 0.03 * walked, max_a = std::min(8.0, 1.0 + 0.5 * walked) * kDeg;
  if (std::hypot(e.z[0], e.z[1]) > max_t || std::fabs(e.z[2]) > max_a) return reject("gate");
  if (verbose)
    std::printf("    icp %d->%d (%.2f,%.2f)->(%.2f,%.2f): overlap %d inliers %d rms %.3f  t (%.3f %.3f) a %.2f deg\n", ni,
                nj, na.pivot.x, na.pivot.z, nb.pivot.x, nb.pivot.z, overlap, inliers, rms, tt.x, tt.z, ta / kDeg);
  e.ok = true;
  return true;
}

void LayoutEngine::optimize(const Options& o) {
  for (Sub& s : subs_) refreshStructure(s);
  buildNodes();
  const int n = static_cast<int>(nodes_.size());
  if (n < 3) {
    edges_.clear();
    loop_edges_ = rejected_ = 0;
    applyNodes();
    return;
  }
  for (int round = 0; round < std::max(1, o.rounds); ++round) {
    struct BB {
      float x0, z0, x1, z1;
      bool ok;
    };
    std::vector<BB> bb(n);
    for (int i = 0; i < n; ++i) {
      bb[i] = {1e9f, 1e9f, -1e9f, -1e9f, nodes_[i].points >= 30};
      if (!bb[i].ok) continue;
      std::vector<V2> pts, nrm;
      gather(nodes_[i], pts, nrm);
      for (const V2& q : pts) {
        bb[i].x0 = std::min(bb[i].x0, q.x);
        bb[i].z0 = std::min(bb[i].z0, q.z);
        bb[i].x1 = std::max(bb[i].x1, q.x);
        bb[i].z1 = std::max(bb[i].z1, q.z);
      }
    }
    std::vector<std::pair<int, int>> cand;
    for (int i = 2; i < n; ++i) {
      if (!bb[i].ok) continue;
      std::vector<std::pair<float, int>> best;
      for (int j = 0; j <= i - 2; ++j) {
        if (!bb[j].ok) continue;
        float w = std::min(bb[i].x1, bb[j].x1) - std::max(bb[i].x0, bb[j].x0);
        float h = std::min(bb[i].z1, bb[j].z1) - std::max(bb[i].z0, bb[j].z0);
        if (w > 0 && h > 0 && w * h >= 0.5f) best.emplace_back(w * h, j);
      }
      std::sort(best.begin(), best.end(), [](auto& a, auto& b) { return a.first > b.first; });
      for (size_t k = 0; k < best.size() && k < 3; ++k) cand.emplace_back(i, best[k].second);
    }

    std::map<std::pair<int, int>, LoopEdge> kept;
    std::vector<std::pair<int, int>> todo;
    for (const auto& c : cand) {
      const Node& a = nodes_[c.first];
      const Node& b = nodes_[c.second];
      std::pair<int, int> key(a.first, b.first);
      auto it = edges_.find(key);
      LoopEdge e = it != edges_.end() ? it->second : LoopEdge{};
      bool moved = false;
      for (int k = 0; k < 3; ++k) {
        double tol = k == 2 ? 0.5 * kDeg : 0.03;
        moved = moved || std::fabs(a.x[k] - e.xi[k]) > tol || std::fabs(b.x[k] - e.xj[k]) > tol;
      }
      if (it == edges_.end() || e.ver_i != a.ver || e.ver_j != b.ver || moved) todo.push_back(c);
      kept[key] = e;
    }
    std::sort(todo.begin(), todo.end(), [](auto& a, auto& b) { return a.first > b.first; });
    int budget = o.max_icp < 0 ? static_cast<int>(todo.size()) : o.max_icp;
    for (const auto& c : todo) {
      if (budget-- <= 0) break;
      const Node& a = nodes_[c.first];
      const Node& b = nodes_[c.second];
      LoopEdge& e = kept[{a.first, b.first}];
      measure(c.first, c.second, e);
      e.active = true;
      e.ver_i = a.ver;
      e.ver_j = b.ver;
      std::copy(a.x, a.x + 3, e.xi);
      std::copy(b.x, b.x + 3, e.xj);
    }
    edges_.swap(kept);
    std::map<int, int> node_of_first;
    for (int i = 0; i < n; ++i) node_of_first[nodes_[i].first] = i;
    std::vector<std::array<int, 4>> loops;  // key.first, key.second, node i, node j
    for (const auto& [k, e] : edges_)
      if (e.ok) loops.push_back({k.first, k.second, node_of_first[k.first], node_of_first[k.second]});
    solve(loops);
    applyNodes();
  }
}

// Linear least squares on the node corrections (block-sparse normal equations, PCG), with
// chi-square rejection of loop edges that disagree with the consensus.
bool LayoutEngine::solve(const std::vector<std::array<int, 4>>& loops) {
  const int n = static_cast<int>(nodes_.size());
  for (const auto& l : loops) edges_[{l[0], l[1]}].active = true;
  std::vector<double> x(3 * n);
  for (int i = 0; i < n; ++i) std::copy(nodes_[i].x, nodes_[i].x + 3, &x[3 * i]);
  int rejected = 0, active_count = 0;

  for (int pass = 0; pass < 4; ++pass) {
    std::vector<std::array<double, 9>> D(n);
    std::map<std::pair<int, int>, std::array<double, 9>> off;
    std::vector<double> b(3 * n, 0.0);
    for (auto& d : D) d.fill(0);
    auto addEdge = [&](int i, int j, const double* Ai, const double* Aj, const double* z, const double* L) {
      double LAi[9], LAj[9], t[9], v[3];
      m3mul(L, Ai, LAi);
      m3mul(L, Aj, LAj);
      m3tmul(Ai, LAi, t);
      for (int k = 0; k < 9; ++k) D[i][k] += t[k];
      m3tmul(Aj, LAj, t);
      for (int k = 0; k < 9; ++k) D[j][k] += t[k];
      if (i < j) {
        m3tmul(Ai, LAj, t);
        auto& B = off[{i, j}];
        for (int k = 0; k < 9; ++k) B[k] += t[k];
      } else {
        m3tmul(Aj, LAi, t);
        auto& B = off[{j, i}];
        for (int k = 0; k < 9; ++k) B[k] += t[k];
      }
      double Lz[3];
      m3vec(L, z, Lz);
      m3tvec(Ai, Lz, v);
      for (int k = 0; k < 3; ++k) b[3 * i + k] += v[k];
      m3tvec(Aj, Lz, v);
      for (int k = 0; k < 3; ++k) b[3 * j + k] += v[k];
    };
    // Gauge (first node fixed) and a weak prior that keeps every node well-posed.
    for (int k = 0; k < 3; ++k) D[0][k * 4] += 1e6;
    for (int i = 0; i < n; ++i) {
      D[i][0] += 1.0 / 25.0;
      D[i][4] += 1.0 / 25.0;
      D[i][8] += 1.0 / 0.25;
    }
    // Odometry chain: consecutive nodes keep their relative pose, less strictly the farther apart.
    for (int i = 0; i + 1 < n; ++i) {
      const Node& a = nodes_[i];
      const Node& c = nodes_[i + 1];
      float dist = len(c.pivot - a.pivot) + 0.1f;
      double st = 0.02 + 0.02 * dist, sr = (0.25 + 0.5 * dist) * kDeg;
      double L[9], Ai[9], Aj[9], z[3] = {0, 0, 0};
      m3set(L, 1 / (st * st), 0, 0, 0, 1 / (st * st), 0, 0, 0, 1 / (sr * sr));
      relJacobian(c.pivot, a.pivot, -1.0, Ai);
      relJacobian(c.pivot, c.pivot, 1.0, Aj);
      addEdge(i, i + 1, Ai, Aj, z, L);
    }
    active_count = 0;
    for (const auto& l : loops) {
      LoopEdge& e = edges_[{l[0], l[1]}];
      if (!e.active) continue;
      ++active_count;
      addEdge(l[2], l[3], e.A_i, e.A_j, e.z, e.info);
    }

    // PCG with block-Jacobi preconditioner (warm start from the current corrections).
    std::vector<std::array<double, 9>> Pinv(n);
    for (int i = 0; i < n; ++i)
      if (!m3inv(D[i].data(), Pinv[i].data())) return false;
    auto matvec = [&](const std::vector<double>& v, std::vector<double>& y) {
      y.assign(3 * n, 0.0);
      for (int i = 0; i < n; ++i) {
        double t[3];
        m3vec(D[i].data(), &v[3 * i], t);
        for (int k = 0; k < 3; ++k) y[3 * i + k] += t[k];
      }
      for (const auto& [ij, B] : off) {
        double t[3];
        m3vec(B.data(), &v[3 * ij.second], t);
        for (int k = 0; k < 3; ++k) y[3 * ij.first + k] += t[k];
        m3tvec(B.data(), &v[3 * ij.first], t);
        for (int k = 0; k < 3; ++k) y[3 * ij.second + k] += t[k];
      }
    };
    std::vector<double> r(3 * n), zv(3 * n), p(3 * n), Ap;
    matvec(x, Ap);
    double bn = 0;
    for (int k = 0; k < 3 * n; ++k) {
      r[k] = b[k] - Ap[k];
      bn += b[k] * b[k];
    }
    auto precond = [&]() {
      for (int i = 0; i < n; ++i) m3vec(Pinv[i].data(), &r[3 * i], &zv[3 * i]);
    };
    precond();
    p = zv;
    double rz = 0;
    for (int k = 0; k < 3 * n; ++k) rz += r[k] * zv[k];
    for (int it = 0; it < 600; ++it) {
      double rn = 0;
      for (double v : r) rn += v * v;
      if (rn <= 1e-20 * std::max(1.0, bn)) break;
      matvec(p, Ap);
      double pAp = 0;
      for (int k = 0; k < 3 * n; ++k) pAp += p[k] * Ap[k];
      if (pAp <= 0) break;
      double alpha = rz / pAp;
      for (int k = 0; k < 3 * n; ++k) {
        x[k] += alpha * p[k];
        r[k] -= alpha * Ap[k];
      }
      precond();
      double rz2 = 0;
      for (int k = 0; k < 3 * n; ++k) rz2 += r[k] * zv[k];
      double beta = rz2 / rz;
      rz = rz2;
      for (int k = 0; k < 3 * n; ++k) p[k] = zv[k] + beta * p[k];
    }

    bool changed = false;
    for (const auto& l : loops) {
      LoopEdge& e = edges_[{l[0], l[1]}];
      if (!e.active) continue;
      double ri[3], rj[3], res[3], Lr[3];
      m3vec(e.A_i, &x[3 * l[2]], ri);
      m3vec(e.A_j, &x[3 * l[3]], rj);
      for (int q = 0; q < 3; ++q) res[q] = ri[q] + rj[q] - e.z[q];
      m3vec(e.info, res, Lr);
      double chi2 = res[0] * Lr[0] + res[1] * Lr[1] + res[2] * Lr[2];
      if (chi2 > 25.0) {
        e.active = false;
        ++rejected;
        changed = true;
      }
    }
    if (!changed) break;
  }

  for (int i = 0; i < n; ++i)
    if (std::fabs(x[3 * i + 2]) > 20.0 * kDeg || std::hypot(x[3 * i], x[3 * i + 1]) > 2.5) return false;
  for (int i = 0; i < n; ++i) std::copy(&x[3 * i], &x[3 * i] + 3, nodes_[i].x);
  loop_edges_ = active_count;
  rejected_ = rejected;
  return true;
}

void LayoutEngine::publish() {
  std::vector<Pose> c = corrections();
  std::lock_guard<std::mutex> l(store_->mu);
  if (store_->session_id != session_) return;
  size_t n = std::min(c.size(), store_->submaps.size());
  for (size_t i = 0; i < n; ++i) store_->submaps[i]->correction = c[i];
  if (n > 0) store_->display_correction = c[n - 1];
}

// --- model extraction ---------------------------------------------------------------------

void LayoutEngine::extract() {
  Layout L;
  L.submaps = static_cast<int>(subs_.size());
  L.loop_edges = loop_edges_;
  L.rejected_edges = rejected_;
  for (const Sub& s : subs_) {
    L.max_correction_m = std::max(L.max_correction_m, static_cast<float>(std::hypot(s.x[0], s.x[1])));
    L.max_correction_deg = std::max(L.max_correction_deg, static_cast<float>(std::fabs(s.x[2]) / kDeg));
  }
  const float f = floor_y_;
  L.floor_y = f;
  if (!subs_.empty()) {
    const Sub& last = subs_.back();
    L.latest_correction = correctionOf(last.pivot, last.x);
  }
  if (has_device_) {
    L.has_device = true;
    L.device = L.latest_correction.apply({device_.t.x, device_.t.z});
    Vec3 fw = rotate(device_.q, {0, 0, -1});
    V2 f2(fw.x, fw.z);
    float fl = len(f2);
    L.device_fwd = fl > 1e-3f ? rot(f2 * (1.0f / fl), L.latest_correction.a) : V2(0, -1);
  }

  // ---- 1. Global height-profile grid in the corrected world ----
  const uint64_t floorB = band(f - 0.06f, f + 0.06f);
  const uint64_t structB = band(f + 0.3f, f + 3.5f);
  std::vector<Rigid2> T(subs_.size());
  float bx0 = 1e9f, bz0 = 1e9f, bx1 = -1e9f, bz1 = -1e9f;
  float tx0 = 1e9f, tz0 = 1e9f, tx1 = -1e9f, tz1 = -1e9f;
  std::vector<V2> traj;
  std::vector<int> traj_sub;
  for (size_t i = 0; i < subs_.size(); ++i) {
    const Sub& s = subs_[i];
    T[i] = s.total();
    for (const auto& [k, c] : s.cells) {
      if (c.count < 2 || !((c.all & floorB) || pop(c.all & structB) >= 2)) continue;
      V2 p = T[i].apply({keyX(k) * kRes + c.sx / c.count, keyZ(k) * kRes + c.sz / c.count});
      bx0 = std::min(bx0, p.x);
      bz0 = std::min(bz0, p.z);
      bx1 = std::max(bx1, p.x);
      bz1 = std::max(bz1, p.z);
    }
    for (const V2& t : s.traj) {
      V2 p = T[i].apply(t);
      traj.push_back(p);
      traj_sub.push_back(static_cast<int>(i));
      tx0 = std::min(tx0, p.x);
      tz0 = std::min(tz0, p.z);
      tx1 = std::max(tx1, p.x);
      tz1 = std::max(tz1, p.z);
    }
  }
  if (bx0 > bx1) {
    layout_ = L;
    return;
  }
  if (!traj.empty()) {
    // Phantom returns (mirrors) can land far away; the LD06 range bounds the real structure.
    bx0 = std::max(bx0, tx0 - 12.5f);
    bz0 = std::max(bz0, tz0 - 12.5f);
    bx1 = std::min(bx1, tx1 + 12.5f);
    bz1 = std::min(bz1, tz1 + 12.5f);
  }
  const int gx0 = cellOf(bx0) - 4, gz0 = cellOf(bz0) - 4;
  const int W = std::min(kMaxGrid, cellOf(bx1) - gx0 + 5), H = std::min(kMaxGrid, cellOf(bz1) - gz0 + 5);
  const size_t NC = static_cast<size_t>(W) * H;
  std::vector<uint64_t> all(NC, 0), hi(NC, 0);
  std::vector<uint32_t> cnt(NC, 0);
  auto gidx = [&](const V2& p) -> int {
    int gx = cellOf(p.x) - gx0, gz = cellOf(p.z) - gz0;
    return gx < 0 || gz < 0 || gx >= W || gz >= H ? -1 : gz * W + gx;
  };
  auto gcenter = [&](int idx) { return V2((gx0 + idx % W + 0.5f) * kRes, (gz0 + idx / W + 0.5f) * kRes); };

  // Phantom returns (mirrors reflect the beam; the sensor places the point behind the mirror)
  // are seen *through* a solid wall. The solid stretches of the previous pass's walls act as
  // occluders (doors and windows let the beam through); a cell seen mostly through a wall is
  // dropped. Needs several submaps so each cell has a reliable viewpoint.
  struct Seg {
    V2 a, b;
    float x0, z0, x1, z1;
  };
  std::vector<Seg> occluders;
  if (subs_.size() > 1)
    for (const LayoutWall& w : layout_.walls) {
      const float L = w.length();
      if (L < 0.3f) continue;
      const V2 d = (w.b - w.a) * (1.0f / L);
      std::vector<LayoutOpening> ops = w.openings;
      std::sort(ops.begin(), ops.end(), [](const LayoutOpening& x, const LayoutOpening& y) { return x.t0 < y.t0; });
      float cur = 0;
      auto add = [&](float t0, float t1) {
        if (t1 - t0 < 0.05f) return;
        V2 a = w.a + d * t0, b = w.a + d * t1;
        occluders.push_back({a, b, std::min(a.x, b.x), std::min(a.z, b.z), std::max(a.x, b.x), std::max(a.z, b.z)});
      };
      for (const LayoutOpening& op : ops) {
        add(cur, op.t0);
        cur = std::max(cur, op.t1);
      }
      add(cur, L);
    }
  auto crossesWall = [&](const V2& v, const V2& p) {
    V2 r = p - v;
    float lr = len(r);
    if (lr < 0.5f) return false;
    // The beam must cross a wall at least 15 cm before reaching the cell (not its own wall).
    const float tmax = 1.0f - 0.15f / lr;
    float x0 = std::min(v.x, p.x), x1 = std::max(v.x, p.x), z0 = std::min(v.z, p.z), z1 = std::max(v.z, p.z);
    for (const Seg& s : occluders) {
      if (s.x1 < x0 || s.x0 > x1 || s.z1 < z0 || s.z0 > z1) continue;
      V2 sd = s.b - s.a, q = s.a - v;
      float den = r.x * sd.z - r.z * sd.x;
      if (std::fabs(den) < 1e-9f) continue;
      float t = (q.x * sd.z - q.z * sd.x) / den, u = (q.x * r.z - q.z * r.x) / den;
      if (t > 0.05f && t < tmax && u >= 0.0f && u <= 1.0f) return true;
    }
    return false;
  };
  std::vector<uint32_t> through, direct;
  if (!occluders.empty()) {
    through.assign(NC, 0);
    direct.assign(NC, 0);
  }
  for (size_t i = 0; i < subs_.size(); ++i) {
    const Sub& s = subs_[i];
    const V2 origin(s.created.t.x, s.created.t.z);
    for (const auto& [k, c] : s.cells) {
      V2 local(keyX(k) * kRes + c.sx / c.count, keyZ(k) * kRes + c.sz / c.count);
      V2 p = T[i].apply(local);
      int g = gidx(p);
      if (g < 0) continue;
      all[g] |= c.all;
      hi[g] |= c.hi;
      cnt[g] += c.count;
      if (!occluders.empty() && !s.traj.empty() && pop(c.all & structB) >= 2)
        (crossesWall(T[i].apply(viewpoint(s.traj, local, origin)), p) ? through : direct)[g] += c.count;
    }
  }
  int phantoms = 0;
  if (!occluders.empty())
    for (size_t g = 0; g < NC; ++g)
      if (through[g] > 0 && through[g] * 2 > direct[g] * 3) {
        all[g] = hi[g] = 0;
        cnt[g] = 0;
        ++phantoms;
      }
  if (verbose && phantoms) std::printf("    phantom cells removed: %d\n", phantoms);
  L.valid = true;

  // ---- 2. Ceiling: densest horizontal layer well above the floor ----
  int hist[kBins] = {0};
  int floor_cells = 0;
  for (size_t g = 0; g < NC; ++g) {
    uint64_t m = all[g];
    if (m & floorB) ++floor_cells;
    while (m) {
      int b = __builtin_ctzll(m);
      ++hist[b];
      m &= m - 1;
    }
  }
  int best = -1;
  for (int b = std::max(1, binOf(f + 1.9f)); b < std::min(kBins - 1, binOf(f + 4.5f) + 1); ++b)
    if (best < 0 || hist[b] > hist[best]) best = b;
  float ceil_y = f + 2.5f;
  L.ceiling_estimated = true;
  if (best > 0 && hist[best] >= std::max(80, static_cast<int>(floor_cells * 0.12f))) {
    double hm = hist[best - 1], h0 = hist[best], hp = hist[best + 1];
    double den = hm - 2 * h0 + hp;
    double off = std::fabs(den) > 1e-9 ? 0.5 * (hm - hp) / den : 0.0;
    ceil_y = binCenter(best) + static_cast<float>(std::clamp(off, -0.5, 0.5)) * kBinH;
    L.ceiling_estimated = false;
  }
  L.ceiling_y = ceil_y;
  const float Hh = ceil_y - f;

  // Height bands relative to floor and ceiling. The wall band sits above door / window lintels
  // and above most furniture, so walls appear continuous over their openings.
  const float wall_lo = f + std::max(1.85f, 0.85f * Hh), wall_hi = ceil_y - 0.12f;
  uint64_t wallB = band(wall_lo, wall_hi);
  if (pop(wallB) < 2) wallB = band(wall_hi - 0.25f, wall_hi);
  const int wall_need = std::min(2, pop(wallB));
  const uint64_t lowerB = band(f + 0.15f, f + 0.85f), upperB = band(f + 1.0f, f + 1.9f);
  const uint64_t objB = band(f + 0.08f, std::min(f + 2.1f, wall_lo - 0.05f));

  // ---- 3. Wall cells (with sub-cell centroids) and local orientation ----
  std::vector<int> wall_id(NC, -1);
  std::vector<int> wall_cells;
  if (wall_need > 0)
    for (size_t g = 0; g < NC; ++g)
      if (cnt[g] >= 3 && pop(all[g] & wallB) >= wall_need) {
        wall_id[g] = static_cast<int>(wall_cells.size());
        wall_cells.push_back(static_cast<int>(g));
      }
  const int nw = static_cast<int>(wall_cells.size());
  // Sub-cell centroids, plus the direction the cell was seen from (towards the scanner path of
  // the submap that observed it): tells which side of a wall face the room is.
  std::vector<double> acc_x(nw, 0), acc_z(nw, 0), acc_n(nw, 0), acc_vx(nw, 0), acc_vz(nw, 0);
  for (size_t i = 0; i < subs_.size(); ++i) {
    const Sub& s = subs_[i];
    for (const auto& [k, c] : s.cells) {
      V2 local(keyX(k) * kRes + c.sx / c.count, keyZ(k) * kRes + c.sz / c.count);
      V2 p = T[i].apply(local);
      int g = gidx(p);
      if (g < 0 || wall_id[g] < 0) continue;
      int w = wall_id[g];
      acc_x[w] += static_cast<double>(p.x) * c.count;
      acc_z[w] += static_cast<double>(p.z) * c.count;
      acc_n[w] += c.count;
      V2 v = viewpoint(s.traj, local, local) - local;
      float vl = len(v);
      if (vl < 1e-3f) continue;
      v = rot(v * (1.0f / vl), T[i].a);
      acc_vx[w] += v.x * c.count;
      acc_vz[w] += v.z * c.count;
    }
  }
  std::vector<V2> wp(nw), wview(nw);
  for (int w = 0; w < nw; ++w) {
    wp[w] = acc_n[w] > 0 ? V2(static_cast<float>(acc_x[w] / acc_n[w]), static_cast<float>(acc_z[w] / acc_n[w]))
                         : gcenter(wall_cells[w]);
    wview[w] = acc_n[w] > 0 ? V2(static_cast<float>(acc_vx[w] / acc_n[w]), static_cast<float>(acc_vz[w] / acc_n[w]))
                            : V2();
  }
  std::vector<float> wang(nw, 0), wlin(nw, 0);
  // Local orientation over +-25 cm: wide enough for sparse profiles (fast turns).
  for (int w = 0; w < nw; ++w) {
    int g = wall_cells[w], gx = g % W, gz = g / W;
    double mx = 0, mz = 0;
    int n = 0;
    int ids[121];
    for (int dz = -5; dz <= 5; ++dz)
      for (int dx = -5; dx <= 5; ++dx) {
        int x = gx + dx, z = gz + dz;
        if (x < 0 || z < 0 || x >= W || z >= H) continue;
        int o = wall_id[z * W + x];
        if (o < 0) continue;
        ids[n++] = o;
        mx += wp[o].x;
        mz += wp[o].z;
      }
    if (n < 4) continue;
    mx /= n;
    mz /= n;
    double sxx = 0, sxz = 0, szz = 0;
    for (int q = 0; q < n; ++q) {
      double ex = wp[ids[q]].x - mx, ez = wp[ids[q]].z - mz;
      sxx += ex * ex;
      sxz += ex * ez;
      szz += ez * ez;
    }
    auto [a, lin] = pca(sxx, sxz, szz);
    wang[w] = a;
    wlin[w] = lin;
  }

  // ---- 4. Dominant (Manhattan) orientation ----
  float theta = 0;
  {
    double h90[90] = {0};
    int used = 0;
    for (int w = 0; w < nw; ++w) {
      if (wlin[w] < 0.6f) continue;
      float a = std::fmod(wang[w], kPi / 2);
      h90[std::min(89, static_cast<int>(a / kDeg))] += wlin[w] * wlin[w];
      ++used;
    }
    if (used >= 20) {
      int pk = 0;
      double pv = -1;
      for (int b = 0; b < 90; ++b) {
        double v = 0;
        for (int k = -2; k <= 2; ++k) v += h90[(b + k + 90) % 90] * (3 - std::abs(k));
        if (v > pv) {
          pv = v;
          pk = b;
        }
      }
      double sc = 0, ss = 0;
      float center = (pk + 0.5f) * kDeg;
      for (int w = 0; w < nw; ++w) {
        if (wlin[w] < 0.6f) continue;
        float a = std::fmod(wang[w], kPi / 2);
        float d = std::fabs(a - center);
        d = std::min(d, kPi / 2 - d);
        if (d > 4 * kDeg) continue;
        sc += std::cos(4 * a) * wlin[w];
        ss += std::sin(4 * a) * wlin[w];
      }
      theta = static_cast<float>(std::atan2(ss, sc) / 4);
      // Keep the plan close to the world axes: theta in (-45, 45] degrees.
      if (theta > kPi / 4) theta -= kPi / 2;
      if (theta <= -kPi / 4) theta += kPi / 2;
    }
  }
  L.theta = theta;

  // ---- 5. Wall faces: axis-aligned peaks, then oblique lines by RANSAC ----
  struct Face {
    float ang, s, t0, t1, w;
    int side;  // +1: seen from the +normal side, -1: from the other side, 0: unknown
  };
  struct Sample {
    float t, s, v;  // along, across, view direction . normal
    bool operator<(const Sample& o) const { return t < o.t; }
  };
  std::vector<Face> faces;
  std::vector<uint8_t> assigned(nw, 0);
  auto makeRuns = [&](std::vector<Sample>& ts, float ang) {
    std::sort(ts.begin(), ts.end());
    size_t a = 0;
    for (size_t k = 1; k <= ts.size(); ++k) {
      if (k < ts.size() && ts[k].t - ts[k - 1].t <= 0.30f) continue;
      size_t cells = k - a;
      float t0 = ts[a].t, t1 = ts[k - 1].t;
      float length = t1 - t0 + kRes;
      if (length >= 0.30f && cells >= std::max<size_t>(5, static_cast<size_t>(0.25f * length / kRes))) {
        double s = 0, v = 0;
        for (size_t q = a; q < k; ++q) {
          s += ts[q].s;
          v += ts[q].v;
        }
        int side = v > 0.25 * cells ? 1 : v < -0.25 * cells ? -1 : 0;
        faces.push_back({ang, static_cast<float>(s / cells), t0 - kRes / 2, t1 + kRes / 2, static_cast<float>(cells), side});
      }
      a = k;
    }
  };
  // Cells of each wall line along one Manhattan axis (peaks of the across-line histogram).
  auto axisGroups = [&](float th, int axis, std::vector<std::vector<int>>& groups) {
    groups.clear();
    const float a = normLine(th + axis * kPi / 2);
    const V2 nn = normalOf(a);
    std::vector<int> ids;
    for (int w = 0; w < nw; ++w)
      if (wlin[w] > 0.5f && lineDiff(wang[w], a) < 15 * kDeg) ids.push_back(w);
    if (ids.size() < 5) return;
    float smin = 1e9f, smax = -1e9f;
    for (int w : ids) {
      float s = dot(wp[w], nn);
      smin = std::min(smin, s);
      smax = std::max(smax, s);
    }
    const int nb = static_cast<int>((smax - smin) / 0.01f) + 7;
    std::vector<double> h(nb, 0), hs(nb, 0);
    for (int w : ids) ++h[static_cast<int>((dot(wp[w], nn) - smin) / 0.01f) + 3];
    const double ker[5] = {1, 2, 3, 2, 1};
    for (int k = 2; k + 2 < nb; ++k)
      for (int q = -2; q <= 2; ++q) hs[k] += h[k + q] * ker[q + 2] / 9.0;
    std::vector<std::pair<double, int>> peaks;
    for (int k = 0; k < nb; ++k) {
      if (hs[k] < 3.0) continue;
      bool mx = true;
      for (int q = -5; q <= 5 && mx; ++q) {
        int o = k + q;
        if (q == 0 || o < 0 || o >= nb) continue;
        if (hs[o] > hs[k] || (q < 0 && hs[o] == hs[k])) mx = false;
      }
      if (mx) peaks.emplace_back(hs[k], k);
    }
    std::sort(peaks.begin(), peaks.end(), [](auto& x, auto& y) { return x.first > y.first; });
    std::vector<float> ps;
    for (auto& [v, k] : peaks) {
      float s = smin + (k - 3 + 0.5f) * 0.01f;
      bool near = false;
      for (float o : ps) near = near || std::fabs(o - s) < 0.06f;
      if (!near) ps.push_back(s);
    }
    groups.assign(ps.size(), {});
    for (int w : ids) {
      float s = dot(wp[w], nn);
      int bi = -1;
      float bd = 0.04f;
      for (size_t q = 0; q < ps.size(); ++q)
        if (std::fabs(ps[q] - s) < bd) {
          bd = std::fabs(ps[q] - s);
          bi = static_cast<int>(q);
        }
      if (bi >= 0) groups[bi].push_back(w);
    }
  };
  // Refine the dominant orientation from whole wall lines (more accurate than local PCA).
  {
    double sc = 0, ss = 0;
    std::vector<std::vector<int>> groups;
    for (int axis = 0; axis < 2; ++axis) {
      axisGroups(theta, axis, groups);
      for (const auto& g : groups) {
        if (g.size() < 20) continue;
        double mx = 0, mz = 0;
        for (int w : g) {
          mx += wp[w].x;
          mz += wp[w].z;
        }
        mx /= g.size();
        mz /= g.size();
        double sxx = 0, sxz = 0, szz = 0;
        for (int w : g) {
          double ex = wp[w].x - mx, ez = wp[w].z - mz;
          sxx += ex * ex;
          sxz += ex * ez;
          szz += ez * ez;
        }
        auto [a, lin] = pca(sxx, sxz, szz);
        if (lin < 0.95f) continue;
        sc += std::cos(4 * a) * g.size();
        ss += std::sin(4 * a) * g.size();
      }
    }
    if (sc * sc + ss * ss > 0) {
      float th = static_cast<float>(std::atan2(ss, sc) / 4);
      if (th > kPi / 4) th -= kPi / 2;
      if (th <= -kPi / 4) th += kPi / 2;
      if (lineDiff(th, theta) < 3 * kDeg) theta = th;
    }
    L.theta = theta;
  }
  for (int axis = 0; axis < 2; ++axis) {
    const float a = normLine(theta + axis * kPi / 2);
    const V2 d = dirOf(a), nn = normalOf(a);
    std::vector<std::vector<int>> groups;
    axisGroups(theta, axis, groups);
    for (const auto& g : groups) {
      std::vector<Sample> ts;
      for (int w : g) {
        ts.push_back({dot(wp[w], d), dot(wp[w], nn), dot(wview[w], nn)});
        assigned[w] = 1;
      }
      makeRuns(ts, a);
    }
  }
  {
    std::vector<int> rest;
    for (int w = 0; w < nw; ++w)
      if (!assigned[w] && wlin[w] > 0.6f && lineDiff(wang[w], theta) > 15 * kDeg &&
          lineDiff(wang[w], theta + kPi / 2) > 15 * kDeg)
        rest.push_back(w);
    std::mt19937 rng(7);
    for (int line = 0; line < 6 && rest.size() >= 10; ++line) {
      int best_n = 0;
      float best_a = 0, best_s = 0;
      for (int it = 0; it < 150; ++it) {
        int c = rest[rng() % rest.size()];
        float a = wang[c];
        V2 nn = normalOf(a);
        float s0 = dot(wp[c], nn);
        int cntIn = 0;
        for (int r : rest)
          if (std::fabs(dot(wp[r], nn) - s0) < 0.04f && lineDiff(wang[r], a) < 15 * kDeg) ++cntIn;
        if (cntIn > best_n) {
          best_n = cntIn;
          best_a = a;
          best_s = s0;
        }
      }
      if (best_n < 10) break;
      double mx = 0, mz = 0;
      int n = 0;
      V2 nn = normalOf(best_a);
      for (int r : rest)
        if (std::fabs(dot(wp[r], nn) - best_s) < 0.04f && lineDiff(wang[r], best_a) < 15 * kDeg) {
          mx += wp[r].x;
          mz += wp[r].z;
          ++n;
        }
      mx /= n;
      mz /= n;
      double sxx = 0, sxz = 0, szz = 0;
      for (int r : rest)
        if (std::fabs(dot(wp[r], nn) - best_s) < 0.04f && lineDiff(wang[r], best_a) < 15 * kDeg) {
          double ex = wp[r].x - mx, ez = wp[r].z - mz;
          sxx += ex * ex;
          sxz += ex * ez;
          szz += ez * ez;
        }
      float a = pca(sxx, sxz, szz).first;
      for (int axis = 0; axis < 2; ++axis)
        if (lineDiff(a, theta + axis * kPi / 2) < 7 * kDeg) a = normLine(theta + axis * kPi / 2);
      V2 d = dirOf(a);
      nn = normalOf(a);
      float s0 = static_cast<float>(mx * nn.x + mz * nn.z);
      std::vector<Sample> ts;
      std::vector<int> keep;
      for (int r : rest) {
        float s = dot(wp[r], nn);
        if (std::fabs(s - s0) < 0.05f) ts.push_back({dot(wp[r], d), s, dot(wview[r], nn)});
        else keep.push_back(r);
      }
      if (keep.size() == rest.size()) break;
      makeRuns(ts, a);
      rest.swap(keep);
    }
  }

  auto floorAt = [&](const V2& p) {
    int g = gidx(p);
    return g >= 0 && (all[g] & floorB) ? 1 : 0;
  };
  // Fraction of observed floor in a strip parallel to a line: offsets [o0, o1] from |s|, t in [t0, t1].
  auto floorFrac = [&](float ang, float s, float t0, float t1, float o0, float o1) {
    V2 d = dirOf(ang), nn = normalOf(ang);
    int hit = 0, tot = 0;
    for (float t = t0 + 0.05f; t < t1; t += 0.1f)
      for (float o = o0; o <= o1 + 1e-4f; o += 0.05f) {
        ++tot;
        hit += floorAt(d * t + nn * (s + o));
      }
    return tot ? static_cast<float>(hit) / tot : 0.0f;
  };
  if (verbose)
    for (const Face& fc : faces)
      std::printf("    face ang %.1f s %.3f t [%.2f, %.2f] cells %.0f side %d\n", fc.ang / kDeg, fc.s, fc.t0, fc.t1, fc.w,
                  fc.side);

  // ---- 6. Faces -> walls: thin walls seen from both sides become one wall with a measured
  // thickness; parallel duplicates (residual drift) are averaged. ----
  const int nf = static_cast<int>(faces.size());
  std::vector<int> parent(nf);
  for (int k = 0; k < nf; ++k) parent[k] = k;
  std::function<int(int)> find = [&](int k) { return parent[k] == k ? k : parent[k] = find(parent[k]); };
  auto endpoints = [&](const Face& fc, V2& p0, V2& p1) {
    V2 d = dirOf(fc.ang), nn = normalOf(fc.ang);
    p0 = d * fc.t0 + nn * fc.s;
    p1 = d * fc.t1 + nn * fc.s;
  };
  for (int a = 0; a < nf; ++a)
    for (int b = a + 1; b < nf; ++b) {
      const Face& fa = faces[a];
      const Face& fb = faces[b];
      if (lineDiff(fa.ang, fb.ang) > 4 * kDeg) continue;
      V2 d = dirOf(fa.ang), nn = normalOf(fa.ang), g0, g1;
      endpoints(fb, g0, g1);
      float sg = 0.5f * (dot(g0, nn) + dot(g1, nn));
      if (std::fabs(sg - fa.s) > 0.32f) continue;
      float u0 = dot(g0, d), u1 = dot(g1, d);
      if (u0 > u1) std::swap(u0, u1);
      float ov = std::min(fa.t1, u1) - std::max(fa.t0, u0);
      float shorter = std::min(fa.t1 - fa.t0, u1 - u0);
      if (ov >= std::min(0.25f, 0.5f * shorter)) parent[find(b)] = find(a);
    }
  struct WallTmp {
    float ang, s, t0, t1, th, w;
    int faces;
  };
  std::vector<WallTmp> walls;
  std::map<int, std::vector<int>> clusters;
  for (int k = 0; k < nf; ++k) clusters[find(k)].push_back(k);
  for (auto& [root, ids] : clusters) {
    int ref = ids[0];
    for (int k : ids)
      if (faces[k].w > faces[ref].w) ref = k;
    const float ang = faces[ref].ang;
    const V2 d = dirOf(ang), nn = normalOf(ang);
    struct P {
      float s, t0, t1, w;
      int side;
    };
    std::vector<P> ps;
    float t0 = 1e9f, t1 = -1e9f, wsum = 0;
    for (int k : ids) {
      V2 p0, p1;
      endpoints(faces[k], p0, p1);
      float u0 = dot(p0, d), u1 = dot(p1, d);
      if (u0 > u1) std::swap(u0, u1);
      float sgn = dot(normalOf(faces[k].ang), nn) >= 0 ? 1.0f : -1.0f;
      ps.push_back({0.5f * (dot(p0, nn) + dot(p1, nn)), u0, u1, faces[k].w, static_cast<int>(faces[k].side * sgn)});
      t0 = std::min(t0, u0);
      t1 = std::max(t1, u1);
      wsum += faces[k].w;
    }
    std::sort(ps.begin(), ps.end(), [](const P& x, const P& y) { return x.s < y.s; });
    const P& lo = ps.front();
    const P& top = ps.back();
    bool solid = false;
    if (ps.size() >= 2 && top.s - lo.s >= 0.05f) {
      if (lo.side < 0 && top.side > 0) {
        solid = true;  // each face seen from its own room
      } else if ((lo.side == 0 || top.side == 0) && top.s - lo.s > 0.12f) {
        float o0 = std::max(lo.t0, top.t0), o1 = std::min(lo.t1, top.t1);
        if (o1 - o0 > 0.2f) solid = floorFrac(ang, lo.s, o0, o1, 0.05f, top.s - lo.s - 0.05f) < 0.12f;
      }
    }
    WallTmp wt{ang, 0, t0, t1, kDefaultThickness, wsum, 1};
    if (solid) {
      wt.s = 0.5f * (lo.s + top.s);
      wt.th = std::clamp(top.s - lo.s, 0.06f, 0.45f);
      wt.faces = 2;
    } else {
      double s = 0, side = 0;
      for (const P& p : ps) {
        s += p.s * p.w;
        side += p.side * p.w;
      }
      s /= wsum;
      int sd = side > 0.2 * wsum ? 1 : side < -0.2 * wsum ? -1 : 0;
      // A single observed face: the wall body lies behind it, away from the scanned room.
      wt.s = static_cast<float>(s) - sd * kDefaultThickness / 2;
      L.merged_faces += static_cast<int>(ps.size()) - 1;
    }
    walls.push_back(wt);
  }
  // Collinear pieces split by unscanned stretches become one wall.
  for (bool changed = true; changed;) {
    changed = false;
    for (size_t a = 0; a < walls.size() && !changed; ++a)
      for (size_t b = a + 1; b < walls.size() && !changed; ++b) {
        WallTmp& A = walls[a];
        WallTmp& B = walls[b];
        if (lineDiff(A.ang, B.ang) > 3 * kDeg) continue;
        V2 d = dirOf(A.ang), nn = normalOf(A.ang);
        V2 db = dirOf(B.ang), nb = normalOf(B.ang);
        V2 b0 = db * B.t0 + nb * B.s, b1 = db * B.t1 + nb * B.s;
        if (std::fabs(0.5f * (dot(b0, nn) + dot(b1, nn)) - A.s) > 0.08f) continue;
        float u0 = dot(b0, d), u1 = dot(b1, d);
        if (u0 > u1) std::swap(u0, u1);
        float gap = std::max(u0 - A.t1, A.t0 - u1);
        if (gap > 0.45f) continue;
        float w = A.w + B.w;
        A.s = (A.s * A.w + 0.5f * (dot(b0, nn) + dot(b1, nn)) * B.w) / w;
        A.t0 = std::min(A.t0, u0);
        A.t1 = std::max(A.t1, u1);
        A.th = std::max(A.th, B.th);
        A.faces = std::max(A.faces, B.faces);
        A.w = w;
        walls.erase(walls.begin() + static_cast<long>(b));
        changed = true;
      }
  }

  // ---- 7. Junctions: extend / trim ends onto neighbouring walls so corners close ----
  struct JW {
    float ang, s, t0, t1, th;
    V2 d, n;
    bool j0 = false, j1 = false;
    float e0 = 0, e1 = 0;
    int faces;
  };
  std::vector<JW> jw;
  for (const WallTmp& w : walls) {
    JW j;
    j.ang = w.ang;
    j.s = w.s;
    j.t0 = w.t0;
    j.t1 = w.t1;
    j.th = w.th;
    j.d = dirOf(w.ang);
    j.n = normalOf(w.ang);
    j.faces = w.faces;
    jw.push_back(j);
  }
  // Corners are often occluded by furniture (beds, cabinets), so ends up to 1.6 m short of a
  // perpendicular wall still close onto it, unless floor was observed along the extension (an
  // open passage). The nearest candidate wins.
  constexpr float kReach = 1.6f;
  auto openAlong = [&](const V2& p0, const V2& p1) {
    float l = len(p1 - p0);
    // Narrower than a door: never a passage (and thin walls show floor noise at their faces).
    if (l < 0.5f) return false;
    int hit = 0, tot = 0;
    for (float t = 0.1f; t < l - 0.05f; t += 0.05f) {
      ++tot;
      hit += floorAt(p0 + (p1 - p0) * (t / l));
    }
    return tot > 0 && hit > 0.35f * tot;
  };
  for (int pass = 0; pass < 2; ++pass)
    for (size_t a = 0; a < jw.size(); ++a)
      for (int end = 0; end < 2; ++end) {
        JW& A = jw[a];
        if (end == 0 ? A.j0 : A.j1) continue;
        const float te = end == 0 ? A.t0 : A.t1;
        int bi = -1;
        float bt = 0, bu = 0, bd = 1e9f;
        for (size_t b = 0; b < jw.size(); ++b) {
          if (b == a) continue;
          const JW& B = jw[b];
          if (lineDiff(A.ang, B.ang) < 25 * kDeg) continue;
          float den = dot(A.d, B.n);
          if (std::fabs(den) < 0.3f) continue;
          float t = (B.s - A.s * dot(A.n, B.n)) / den;
          // Extend up to kReach; trim an overshoot only slightly (crossing walls stay whole).
          float grow = end == 1 ? t - te : te - t;
          if (grow > kReach + B.th / 2 || grow < -(0.3f + B.th / 2)) continue;
          if (end == 1 ? t - A.t0 < 0.2f : A.t1 - t < 0.2f) continue;
          float u = dot(A.d * t + A.n * A.s, B.d);
          if (u < B.t0 - kReach || u > B.t1 + kReach) continue;
          V2 I = A.d * t + A.n * A.s;
          if (grow > 0 && openAlong(A.d * te + A.n * A.s, I)) continue;
          if (u < B.t0 && openAlong(B.d * B.t0 + B.n * B.s, I)) continue;
          if (u > B.t1 && openAlong(B.d * B.t1 + B.n * B.s, I)) continue;
          float score = std::fabs(t - te) + std::max(0.0f, std::max(B.t0 - u, u - B.t1));
          if (score < bd) {
            bd = score;
            bi = static_cast<int>(b);
            bt = t;
            bu = u;
          }
        }
        if (bi < 0) continue;
        JW& B = jw[bi];
        if (end == 0) {
          A.t0 = bt;
          A.j0 = true;
          A.e0 = B.th / 2;
        } else {
          A.t1 = bt;
          A.j1 = true;
          A.e1 = B.th / 2;
        }
        if (!B.j0 && bu > B.t0 - kReach && bu < B.t0 + 0.3f && B.t1 - bu >= 0.2f) {
          B.t0 = bu;
          B.j0 = true;
          B.e0 = A.th / 2;
        } else if (!B.j1 && bu < B.t1 + kReach && bu > B.t1 - 0.3f && bu - B.t0 >= 0.2f) {
          B.t1 = bu;
          B.j1 = true;
          B.e1 = A.th / 2;
        }
        ++L.closed_corners;
      }

  // ---- 8. Openings: doors (nothing below the lintel) and windows (sill below, glass-like
  // or empty above). Doors are confirmed by a trajectory crossing or floor on both sides. ----
  for (const JW& j : jw) {
    LayoutWall w;
    w.a = j.d * j.t0 + j.n * j.s;
    w.b = j.d * j.t1 + j.n * j.s;
    w.thickness = j.th;
    w.height = Hh;
    w.joined_a = j.j0;
    w.joined_b = j.j1;
    w.ext_a = j.e0;
    w.ext_b = j.e1;
    w.faces = j.faces;
    std::vector<float> crossings;
    for (size_t k = 1; k < traj.size(); ++k) {
      if (traj_sub[k] != traj_sub[k - 1] && traj_sub[k] != traj_sub[k - 1] + 1) continue;
      float s0 = dot(traj[k - 1], j.n) - j.s, s1 = dot(traj[k], j.n) - j.s;
      if (s0 * s1 >= 0 || len(traj[k] - traj[k - 1]) > 0.6f) continue;
      V2 c = traj[k - 1] + (traj[k] - traj[k - 1]) * (s0 / (s0 - s1));
      crossings.push_back(dot(c, j.d));
    }
    const float margin = 0.1f, half = j.th / 2 + 0.06f;
    std::vector<uint64_t> prof_all, prof_hi;
    std::vector<float> ts;
    for (float t = j.t0 + margin; t <= j.t1 - margin; t += kRes) {
      uint64_t ma = 0, mh = 0;
      for (float o = -half; o <= half + 1e-4f; o += 0.04f) {
        int g = gidx(j.d * t + j.n * (j.s + o));
        if (g < 0) continue;
        ma |= all[g];
        mh |= hi[g];
      }
      prof_all.push_back(ma);
      prof_hi.push_back(mh);
      ts.push_back(t);
    }
    const int ns = static_cast<int>(ts.size());
    for (int kind = 0; kind < 2; ++kind) {
      std::vector<uint8_t> on(ns, 0);
      for (int k = 0; k < ns; ++k) {
        bool lintel = (prof_all[k] & wallB) != 0;
        if (kind == 0)
          on[k] = lintel && pop(prof_all[k] & lowerB) == 0 && pop(prof_all[k] & upperB) <= 1;
        else
          on[k] = lintel && pop(prof_hi[k] & lowerB) >= 2 && pop(prof_hi[k] & upperB) <= 1;
      }
      for (int k = 1; k + 1 < ns; ++k)
        if (!on[k] && on[k - 1] && on[k + 1]) on[k] = 1;
      for (int k = 0; k < ns;) {
        if (!on[k]) {
          ++k;
          continue;
        }
        int e = k;
        while (e + 1 < ns && on[e + 1]) ++e;
        float ta = ts[k] - kRes / 2, tb = ts[e] + kRes / 2, width = tb - ta;
        uint64_t ma = 0, mh = 0;
        for (int q = k; q <= e; ++q) {
          ma |= prof_all[q];
          mh |= prof_hi[q];
        }
        LayoutOpening op;
        op.t0 = ta - j.t0;
        op.t1 = tb - j.t0;
        if (kind == 0 && width >= 0.55f && width <= 1.7f) {
          bool crossed = false;
          for (float c : crossings) crossed = crossed || (c >= ta - 0.25f && c <= tb + 0.25f);
          bool open = floorFrac(j.ang, j.s, ta, tb, j.th / 2 + 0.1f, j.th / 2 + 0.6f) >= 0.25f &&
                      floorFrac(j.ang, j.s, ta, tb, -(j.th / 2 + 0.6f), -(j.th / 2 + 0.1f)) >= 0.25f;
          if (crossed || (open && width <= 1.2f)) {
            op.kind = LayoutOpening::Door;
            op.bottom = 0;
            int lb = lowestBit(ma & band(f + 1.8f, ceil_y));
            op.top = std::clamp(lb >= 0 ? binLo(lb) - f : 2.05f, 1.9f, std::max(1.9f, Hh - 0.1f));
            w.openings.push_back(op);
          }
        } else if (kind == 1 && width >= 0.35f && width <= 4.0f) {
          op.kind = LayoutOpening::Window;
          int sb = highestBit(mh & band(f, f + 1.4f));
          int tb2 = lowestBit(mh & band(f + 1.6f, ceil_y));
          op.bottom = std::clamp(sb >= 0 ? binLo(sb) + kBinH - f : 0.9f, 0.2f, 1.4f);
          op.top = std::clamp(tb2 >= 0 ? binLo(tb2) - f : 2.1f, op.bottom + 0.3f, std::max(op.bottom + 0.3f, Hh - 0.05f));
          w.openings.push_back(op);
        }
        k = e + 1;
      }
    }
    std::sort(w.openings.begin(), w.openings.end(),
              [](const LayoutOpening& x, const LayoutOpening& y) { return x.t0 < y.t0; });
    L.walls.push_back(w);
  }

  // ---- 9. Rooms: observed floor bounded by the walls (doors closed), in the plan frame ----
  const V2 du = dirOf(theta), dv = normalOf(theta);
  float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
  for (V2 c : {V2(gx0 * kRes, gz0 * kRes), V2((gx0 + W) * kRes, gz0 * kRes), V2(gx0 * kRes, (gz0 + H) * kRes),
               V2((gx0 + W) * kRes, (gz0 + H) * kRes)}) {
    u0 = std::min(u0, dot(c, du));
    u1 = std::max(u1, dot(c, du));
    v0 = std::min(v0, dot(c, dv));
    v1 = std::max(v1, dot(c, dv));
  }
  const int RW = std::min(kMaxGrid + 200, static_cast<int>((u1 - u0) / kRes) + 1);
  const int RH = std::min(kMaxGrid + 200, static_cast<int>((v1 - v0) / kRes) + 1);
  const size_t RN = static_cast<size_t>(RW) * RH;
  auto ridx = [&](const V2& p) -> int {
    int x = static_cast<int>(std::floor((dot(p, du) - u0) / kRes)), z = static_cast<int>(std::floor((dot(p, dv) - v0) / kRes));
    return x < 0 || z < 0 || x >= RW || z >= RH ? -1 : z * RW + x;
  };
  auto rcenter = [&](int i) {
    float u = u0 + (i % RW + 0.5f) * kRes, v = v0 + (i / RW + 0.5f) * kRes;
    return du * u + dv * v;
  };
  std::vector<uint8_t> fl(RN, 0), barrier(RN, 0);
  for (size_t g = 0; g < NC; ++g)
    if (all[g] & floorB) {
      int r = ridx(gcenter(static_cast<int>(g)));
      if (r >= 0) fl[r] = 1;
    }
  for (const V2& p : traj)
    for (int dz = -4; dz <= 4; ++dz)
      for (int dx = -4; dx <= 4; ++dx) {
        if (dx * dx + dz * dz > 16) continue;
        int r = ridx(p + du * (dx * kRes) + dv * (dz * kRes));
        if (r >= 0) fl[r] = 1;
      }
  auto rasterWall = [&](const LayoutWall& w, float pad, float extra, std::vector<uint8_t>& out, bool plan) {
    V2 d = w.b - w.a;
    float l = len(d);
    if (l < 1e-3f) return;
    d = d * (1.0f / l);
    V2 a = w.a - d * (w.ext_a + extra), b = w.b + d * (w.ext_b + extra);
    float half = w.thickness / 2 + pad, L2 = len(b - a);
    float r = half + kRes;
    float mnx = std::min(a.x, b.x) - r, mxx = std::max(a.x, b.x) + r, mnz = std::min(a.z, b.z) - r, mxz = std::max(a.z, b.z) + r;
    for (float z = mnz; z <= mxz; z += kRes * 0.5f)
      for (float x = mnx; x <= mxx; x += kRes * 0.5f) {
        V2 p(x, z);
        float t = std::clamp(dot(p - a, d), 0.0f, L2);
        if (len(p - (a + d * t)) > half) continue;
        int i = plan ? ridx(p) : gidx(p);
        if (i >= 0) out[i] = 1;
      }
  };
  for (const LayoutWall& w : L.walls) rasterWall(w, 0.02f, 0.03f, barrier, true);
  {
    // Close pinholes of the forward-mapped floor.
    std::vector<uint8_t> dil(RN, 0);
    for (int z = 0; z < RH; ++z)
      for (int x = 0; x < RW; ++x) {
        bool any = false;
        for (int dz = -1; dz <= 1 && !any; ++dz)
          for (int dx = -1; dx <= 1 && !any; ++dx) {
            int xx = x + dx, zz = z + dz;
            any = xx >= 0 && zz >= 0 && xx < RW && zz < RH && fl[zz * RW + xx];
          }
        dil[z * RW + x] = any;
      }
    for (int z = 0; z < RH; ++z)
      for (int x = 0; x < RW; ++x) {
        bool keep = true;
        for (int dz = -1; dz <= 1 && keep; ++dz)
          for (int dx = -1; dx <= 1 && keep; ++dx) {
            int xx = x + dx, zz = z + dz;
            keep = xx >= 0 && zz >= 0 && xx < RW && zz < RH && dil[zz * RW + xx];
          }
        fl[z * RW + x] = fl[z * RW + x] || keep;
      }
  }
  std::vector<int> comp(RN, -1);
  auto flood = [&](size_t seed, int id, const std::function<bool(size_t)>& in, std::vector<int>& cells, bool& border) {
    std::deque<size_t> q{seed};
    comp[seed] = id;
    border = false;
    while (!q.empty()) {
      size_t c = q.front();
      q.pop_front();
      cells.push_back(static_cast<int>(c));
      int x = static_cast<int>(c % RW), z = static_cast<int>(c / RW);
      if (x == 0 || z == 0 || x == RW - 1 || z == RH - 1) border = true;
      const int nx[4] = {x - 1, x + 1, x, x}, nz[4] = {z, z, z - 1, z + 1};
      for (int k = 0; k < 4; ++k) {
        if (nx[k] < 0 || nz[k] < 0 || nx[k] >= RW || nz[k] >= RH) continue;
        size_t o = static_cast<size_t>(nz[k]) * RW + nx[k];
        if (comp[o] != -1 || !in(o)) continue;
        comp[o] = id;
        q.push_back(o);
      }
    }
  };
  // Unobserved floor enclosed by walls / floor (under furniture) belongs to the room.
  {
    auto hole = [&](size_t c) { return !fl[c] && !barrier[c]; };
    int id = 0;
    for (size_t c = 0; c < RN; ++c) {
      if (comp[c] != -1 || !hole(c)) continue;
      std::vector<int> cells;
      bool border;
      flood(c, id++, hole, cells, border);
      if (!border && cells.size() <= 2400)
        for (int k : cells) fl[k] = 1;
    }
    std::fill(comp.begin(), comp.end(), -1);
  }
  std::vector<int> room_of(RN, -1);
  {
    auto free = [&](size_t c) { return fl[c] && !barrier[c]; };
    int id = 0;
    for (size_t c = 0; c < RN; ++c) {
      if (comp[c] != -1 || !free(c)) continue;
      std::vector<int> cells;
      bool border;
      flood(c, id++, free, cells, border);
      if (cells.size() < 400) continue;  // < 1 m2
      LayoutRoom room;
      room.area = cells.size() * kRes * kRes;
      int x0 = RW, z0 = RH, x1 = -1, z1 = -1;
      for (int k : cells) {
        int x = k % RW, z = k / RW;
        x0 = std::min(x0, x);
        z0 = std::min(z0, z);
        x1 = std::max(x1, x);
        z1 = std::max(z1, z);
        room_of[k] = static_cast<int>(L.rooms.size());
      }
      room.width = (x1 - x0 + 1) * kRes + 0.04f;
      room.depth = (z1 - z0 + 1) * kRes + 0.04f;
      // Label at the most interior cell (chamfer distance transform).
      const int bw = x1 - x0 + 1, bh = z1 - z0 + 1;
      std::vector<int> dist(static_cast<size_t>(bw) * bh, 0);
      const int me = static_cast<int>(L.rooms.size());
      auto inside = [&](int x, int z) { return x >= x0 && z >= z0 && x <= x1 && z <= z1 && room_of[z * RW + x] == me; };
      for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) {
          if (!inside(x, z)) continue;
          int v = 1 << 20;
          auto nb = [&](int xx, int zz, int c) {
            v = std::min(v, inside(xx, zz) ? dist[(zz - z0) * bw + (xx - x0)] + c : c);
          };
          nb(x - 1, z, 3);
          nb(x, z - 1, 3);
          nb(x - 1, z - 1, 4);
          nb(x + 1, z - 1, 4);
          dist[(z - z0) * bw + (x - x0)] = v;
        }
      int bd = -1, bi = cells[0];
      for (int z = z1; z >= z0; --z)
        for (int x = x1; x >= x0; --x) {
          if (!inside(x, z)) continue;
          int& v = dist[(z - z0) * bw + (x - x0)];
          auto nb = [&](int xx, int zz, int c) {
            v = std::min(v, inside(xx, zz) ? dist[(zz - z0) * bw + (xx - x0)] + c : c);
          };
          nb(x + 1, z, 3);
          nb(x, z + 1, 3);
          nb(x + 1, z + 1, 4);
          nb(x - 1, z + 1, 4);
          if (v > bd) {
            bd = v;
            bi = z * RW + x;
          }
        }
      room.label = rcenter(bi);
      // Rectangles: row runs merged with identical runs of the following rows.
      std::vector<std::array<int, 4>> open;  // x0, x1, zstart, zend
      for (int z = z0; z <= z1 + 1; ++z) {
        std::vector<std::pair<int, int>> runs;
        if (z <= z1)
          for (int x = x0; x <= x1;) {
            if (!inside(x, z)) {
              ++x;
              continue;
            }
            int e = x;
            while (e + 1 <= x1 && inside(e + 1, z)) ++e;
            runs.emplace_back(x, e);
            x = e + 1;
          }
        std::vector<std::array<int, 4>> next;
        for (auto& o : open) {
          bool cont = false;
          for (auto& r : runs)
            if (r.first == o[0] && r.second == o[1]) cont = true;
          if (cont) {
            o[3] = z;
            next.push_back(o);
          } else {
            room.rects.push_back({u0 + o[0] * kRes, v0 + o[2] * kRes, u0 + (o[1] + 1) * kRes, v0 + (o[3] + 1) * kRes});
          }
        }
        for (auto& r : runs) {
          bool cont = false;
          for (auto& o : next)
            if (o[0] == r.first && o[1] == r.second) cont = true;
          if (!cont) next.push_back({r.first, r.second, z, z});
        }
        open.swap(next);
      }
      L.rooms.push_back(std::move(room));
    }
  }

  // ---- 10. Furniture: compact clusters off the walls, as boxes aligned to the plan axes ----
  {
    std::vector<uint8_t> wallzone(NC, 0);
    for (const LayoutWall& w : L.walls) rasterWall(w, 0.07f, 0.0f, wallzone, false);
    std::vector<uint8_t> cand(NC, 0), dil(NC, 0);
    for (size_t g = 0; g < NC; ++g) {
      if (wall_id[g] >= 0 || wallzone[g] || cnt[g] < 2) continue;
      int p = pop(all[g] & objB);
      cand[g] = p >= 2 || (p >= 1 && cnt[g] >= 4);
    }
    for (int z = 0; z < H; ++z)
      for (int x = 0; x < W; ++x) {
        if (!cand[z * W + x]) continue;
        for (int dz = -1; dz <= 1; ++dz)
          for (int dx = -1; dx <= 1; ++dx) {
            int xx = x + dx, zz = z + dz;
            if (xx >= 0 && zz >= 0 && xx < W && zz < H) dil[zz * W + xx] = 1;
          }
      }
    std::vector<int> lab(NC, -1);
    int id = 0;
    for (size_t s = 0; s < NC; ++s) {
      if (!dil[s] || lab[s] >= 0) continue;
      std::deque<size_t> q{s};
      lab[s] = id;
      std::vector<int> members;
      while (!q.empty()) {
        size_t c = q.front();
        q.pop_front();
        if (cand[c]) members.push_back(static_cast<int>(c));
        int x = static_cast<int>(c % W), z = static_cast<int>(c / W);
        for (int dz = -1; dz <= 1; ++dz)
          for (int dx = -1; dx <= 1; ++dx) {
            int xx = x + dx, zz = z + dz;
            if (xx < 0 || zz < 0 || xx >= W || zz >= H) continue;
            size_t o = static_cast<size_t>(zz) * W + xx;
            if (!dil[o] || lab[o] >= 0) continue;
            lab[o] = id;
            q.push_back(o);
          }
      }
      ++id;
      if (members.size() < 6) continue;
      float mu0 = 1e9f, mu1 = -1e9f, mv0 = 1e9f, mv1 = -1e9f;
      int top = -1, hollow = 0;
      const int hollow_bin = binOf(f + 0.45f);
      for (int c : members) {
        V2 p = gcenter(c);
        float u = dot(p, du), v = dot(p, dv);
        mu0 = std::min(mu0, u - kRes / 2);
        mu1 = std::max(mu1, u + kRes / 2);
        mv0 = std::min(mv0, v - kRes / 2);
        mv1 = std::max(mv1, v + kRes / 2);
        uint64_t m = all[c] & objB;
        top = std::max(top, highestBit(m));
        if (lowestBit(m) >= hollow_bin) ++hollow;
      }
      float wu = mu1 - mu0, wv = mv1 - mv0, area = wu * wv;
      float h = top >= 0 ? binLo(top) + kBinH - f : 0;
      float fill = members.size() * kRes * kRes / std::max(area, 1e-3f);
      if (area < 0.06f || h < 0.25f || wu > 5.0f || wv > 5.0f || fill < 0.2f) continue;
      V2 c = du * (0.5f * (mu0 + mu1)) + dv * (0.5f * (mv0 + mv1));
      if (!L.rooms.empty()) {
        int r = ridx(c);
        if (r < 0 || room_of[r] < 0) continue;  // phantom (mirror) or outside the scanned rooms
      }
      LayoutObject ob;
      ob.center = c;
      ob.width = wu;
      ob.depth = wv;
      ob.height = std::min(h, Hh);
      ob.angle = theta;
      ob.kind = h >= 1.4f                                                                ? LayoutObject::Tall
                : (hollow >= static_cast<int>(0.5f * members.size()) && h >= 0.55f && h <= 1.2f) ? LayoutObject::Table
                                                                                         : LayoutObject::Low;
      L.objects.push_back(ob);
    }
  }
  if (verbose) {
    for (const LayoutWall& w : L.walls) {
      std::printf("    wall (%.2f, %.2f)-(%.2f, %.2f) th %.2f faces %d", w.a.x, w.a.z, w.b.x, w.b.z, w.thickness, w.faces);
      for (const LayoutOpening& op : w.openings)
        std::printf(" %s[%.2f-%.2f %.2f-%.2f]", op.kind == LayoutOpening::Door ? "D" : "W", op.t0, op.t1, op.bottom, op.top);
      std::printf("\n");
    }
    for (const LayoutRoom& r : L.rooms)
      std::printf("    room %.1f m2 at (%.2f, %.2f) %.2f x %.2f, %zu rects\n", r.area, r.label.x, r.label.z, r.width,
                  r.depth, r.rects.size());
    for (const LayoutObject& ob : L.objects)
      std::printf("    object (%.2f, %.2f) %.2f x %.2f x %.2f kind %d\n", ob.center.x, ob.center.z, ob.width, ob.depth,
                  ob.height, static_cast<int>(ob.kind));
  }
  layout_ = std::move(L);
}

// --- JSON ----------------------------------------------------------------------------------

std::string LayoutEngine::toJson(const Layout& l) {
  std::string o;
  o.reserve(4096 + l.walls.size() * 200 + l.objects.size() * 120);
  o += "{\"v\":1,\"valid\":";
  o += l.valid ? "true" : "false";
  o += ",\"floorY\":";
  num(o, l.floor_y);
  o += ",\"ceilingY\":";
  num(o, l.ceiling_y);
  o += ",\"ceilingEstimated\":";
  o += l.ceiling_estimated ? "true" : "false";
  o += ",\"theta\":";
  num(o, l.theta, 5);
  o += ",\"walls\":[";
  for (size_t i = 0; i < l.walls.size(); ++i) {
    const LayoutWall& w = l.walls[i];
    if (i) o += ',';
    o += "{\"ax\":";
    num(o, w.a.x);
    o += ",\"az\":";
    num(o, w.a.z);
    o += ",\"bx\":";
    num(o, w.b.x);
    o += ",\"bz\":";
    num(o, w.b.z);
    o += ",\"th\":";
    num(o, w.thickness);
    o += ",\"h\":";
    num(o, w.height);
    o += ",\"ja\":";
    o += w.joined_a ? "true" : "false";
    o += ",\"jb\":";
    o += w.joined_b ? "true" : "false";
    o += ",\"ea\":";
    num(o, w.ext_a);
    o += ",\"eb\":";
    num(o, w.ext_b);
    o += ",\"faces\":";
    o += std::to_string(w.faces);
    o += ",\"op\":[";
    for (size_t k = 0; k < w.openings.size(); ++k) {
      const LayoutOpening& op = w.openings[k];
      if (k) o += ',';
      o += op.kind == LayoutOpening::Door ? "{\"k\":\"door\",\"t0\":" : "{\"k\":\"window\",\"t0\":";
      num(o, op.t0);
      o += ",\"t1\":";
      num(o, op.t1);
      o += ",\"b\":";
      num(o, op.bottom);
      o += ",\"t\":";
      num(o, op.top);
      o += '}';
    }
    o += "]}";
  }
  o += "],\"objects\":[";
  for (size_t i = 0; i < l.objects.size(); ++i) {
    const LayoutObject& ob = l.objects[i];
    if (i) o += ',';
    o += "{\"cx\":";
    num(o, ob.center.x);
    o += ",\"cz\":";
    num(o, ob.center.z);
    o += ",\"w\":";
    num(o, ob.width);
    o += ",\"d\":";
    num(o, ob.depth);
    o += ",\"h\":";
    num(o, ob.height);
    o += ",\"a\":";
    num(o, ob.angle, 5);
    o += ",\"k\":\"";
    o += ob.kind == LayoutObject::Tall ? "tall" : ob.kind == LayoutObject::Table ? "table" : "low";
    o += "\"}";
  }
  o += "],\"rooms\":[";
  for (size_t i = 0; i < l.rooms.size(); ++i) {
    const LayoutRoom& r = l.rooms[i];
    if (i) o += ',';
    o += "{\"area\":";
    num(o, r.area, 2);
    o += ",\"lx\":";
    num(o, r.label.x);
    o += ",\"lz\":";
    num(o, r.label.z);
    o += ",\"w\":";
    num(o, r.width, 2);
    o += ",\"d\":";
    num(o, r.depth, 2);
    o += ",\"rects\":[";
    for (size_t k = 0; k < r.rects.size(); ++k) {
      if (k) o += ',';
      o += '[';
      for (int q = 0; q < 4; ++q) {
        if (q) o += ',';
        num(o, r.rects[k][q]);
      }
      o += ']';
    }
    o += "]}";
  }
  o += "]";
  if (l.has_device) {
    o += ",\"device\":{\"x\":";
    num(o, l.device.x);
    o += ",\"z\":";
    num(o, l.device.z);
    o += ",\"fx\":";
    num(o, l.device_fwd.x);
    o += ",\"fz\":";
    num(o, l.device_fwd.z);
    o += '}';
  }
  o += ",\"corr\":{\"a\":";
  num(o, l.latest_correction.a, 6);
  o += ",\"tx\":";
  num(o, l.latest_correction.t.x, 4);
  o += ",\"tz\":";
  num(o, l.latest_correction.t.z, 4);
  o += "},\"stats\":{\"submaps\":";
  o += std::to_string(l.submaps);
  o += ",\"loopEdges\":";
  o += std::to_string(l.loop_edges);
  o += ",\"rejectedEdges\":";
  o += std::to_string(l.rejected_edges);
  o += ",\"mergedFaces\":";
  o += std::to_string(l.merged_faces);
  o += ",\"closedCorners\":";
  o += std::to_string(l.closed_corners);
  o += ",\"maxCorrectionM\":";
  num(o, l.max_correction_m);
  o += ",\"maxCorrectionDeg\":";
  num(o, l.max_correction_deg, 2);
  o += "}}";
  return o;
}

}  // namespace ls
