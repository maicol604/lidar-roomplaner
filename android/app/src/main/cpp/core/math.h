#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ls {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;
constexpr float kRad2Deg = 180.0f / kPi;

struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3() = default;
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator-() const { return {-x, -y, -z}; }
  Vec3& operator+=(const Vec3& o) {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
  float l = length(v);
  return l > 1e-9f ? v * (1.0f / l) : Vec3{0, 0, 0};
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

struct Quat {
  float w = 1, x = 0, y = 0, z = 0;
  Quat() = default;
  Quat(float w_, float x_, float y_, float z_) : w(w_), x(x_), y(y_), z(z_) {}
};

inline Quat operator*(const Quat& a, const Quat& b) {
  return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
          a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}
inline Quat conjugate(const Quat& q) { return {q.w, -q.x, -q.y, -q.z}; }
inline Quat normalize(const Quat& q) {
  float n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  if (n < 1e-12f) return {};
  return {q.w / n, q.x / n, q.y / n, q.z / n};
}
inline Vec3 rotate(const Quat& q, const Vec3& v) {
  // v' = v + 2w(u x v) + 2u x (u x v)
  Vec3 u{q.x, q.y, q.z};
  Vec3 t = cross(u, v) * 2.0f;
  return v + t * q.w + cross(u, t);
}
inline Quat axisAngle(const Vec3& axis, float angle) {
  Vec3 a = normalize(axis);
  float s = std::sin(angle * 0.5f);
  return {std::cos(angle * 0.5f), a.x * s, a.y * s, a.z * s};
}
inline Quat slerp(Quat a, Quat b, float t) {
  float d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
  if (d < 0) {
    b = {-b.w, -b.x, -b.y, -b.z};
    d = -d;
  }
  if (d > 0.9995f) {
    return normalize(Quat{a.w + (b.w - a.w) * t, a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                          a.z + (b.z - a.z) * t});
  }
  float th = std::acos(std::min(1.0f, d));
  float s = std::sin(th);
  float wa = std::sin((1 - t) * th) / s;
  float wb = std::sin(t * th) / s;
  return {a.w * wa + b.w * wb, a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb};
}
inline float angleBetween(const Quat& a, const Quat& b) {
  float d = std::fabs(a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z);
  return 2.0f * std::acos(std::min(1.0f, d));
}
// Rotation matrix (row-major r[row][col]) to quaternion.
inline Quat fromMatrix(const float r[3][3]) {
  float tr = r[0][0] + r[1][1] + r[2][2];
  Quat q;
  if (tr > 0) {
    float s = std::sqrt(tr + 1.0f) * 2;
    q = {0.25f * s, (r[2][1] - r[1][2]) / s, (r[0][2] - r[2][0]) / s, (r[1][0] - r[0][1]) / s};
  } else if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) {
    float s = std::sqrt(1.0f + r[0][0] - r[1][1] - r[2][2]) * 2;
    q = {(r[2][1] - r[1][2]) / s, 0.25f * s, (r[0][1] + r[1][0]) / s, (r[0][2] + r[2][0]) / s};
  } else if (r[1][1] > r[2][2]) {
    float s = std::sqrt(1.0f + r[1][1] - r[0][0] - r[2][2]) * 2;
    q = {(r[0][2] - r[2][0]) / s, (r[0][1] + r[1][0]) / s, 0.25f * s, (r[1][2] + r[2][1]) / s};
  } else {
    float s = std::sqrt(1.0f + r[2][2] - r[0][0] - r[1][1]) * 2;
    q = {(r[1][0] - r[0][1]) / s, (r[0][2] + r[2][0]) / s, (r[1][2] + r[2][1]) / s, 0.25f * s};
  }
  return normalize(q);
}
// Intrinsic Z-Y-X (yaw, pitch, roll) Euler angles in radians.
inline Quat fromEuler(float roll, float pitch, float yaw) {
  return axisAngle({0, 0, 1}, yaw) * axisAngle({0, 1, 0}, pitch) * axisAngle({1, 0, 0}, roll);
}

struct Pose {
  Vec3 t;
  Quat q;
  Vec3 apply(const Vec3& p) const { return rotate(q, p) + t; }
  Pose inverse() const {
    Quat qi = conjugate(q);
    return {rotate(qi, -t), qi};
  }
  Pose operator*(const Pose& o) const { return {apply(o.t), normalize(q * o.q)}; }
};

inline Pose interpolate(const Pose& a, const Pose& b, float alpha) {
  return {lerp(a.t, b.t, alpha), slerp(a.q, b.q, alpha)};
}

// Column-major 4x4 matrix (OpenGL convention).
struct Mat4 {
  float m[16];
  static Mat4 identity() {
    Mat4 r{};
    std::memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
  }
  float& at(int row, int col) { return m[col * 4 + row]; }
  float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int c = 0; c < 4; ++c)
    for (int rr = 0; rr < 4; ++rr) {
      float s = 0;
      for (int k = 0; k < 4; ++k) s += a.at(rr, k) * b.at(k, c);
      r.at(rr, c) = s;
    }
  return r;
}

inline Mat4 fromPose(const Pose& p) {
  Mat4 r = Mat4::identity();
  Vec3 ex = rotate(p.q, {1, 0, 0}), ey = rotate(p.q, {0, 1, 0}), ez = rotate(p.q, {0, 0, 1});
  r.at(0, 0) = ex.x; r.at(1, 0) = ex.y; r.at(2, 0) = ex.z;
  r.at(0, 1) = ey.x; r.at(1, 1) = ey.y; r.at(2, 1) = ey.z;
  r.at(0, 2) = ez.x; r.at(1, 2) = ez.y; r.at(2, 2) = ez.z;
  r.at(0, 3) = p.t.x; r.at(1, 3) = p.t.y; r.at(2, 3) = p.t.z;
  return r;
}

inline Mat4 perspective(float fovyRad, float aspect, float zn, float zf) {
  Mat4 r{};
  std::memset(r.m, 0, sizeof(r.m));
  float f = 1.0f / std::tan(fovyRad * 0.5f);
  r.at(0, 0) = f / aspect;
  r.at(1, 1) = f;
  r.at(2, 2) = (zf + zn) / (zn - zf);
  r.at(2, 3) = 2 * zf * zn / (zn - zf);
  r.at(3, 2) = -1;
  return r;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& upHint) {
  Vec3 f = normalize(target - eye);
  Vec3 up = upHint;
  if (std::fabs(dot(f, normalize(up))) > 0.999f) up = {0, 0, -1};
  Vec3 s = normalize(cross(f, up));
  Vec3 u = cross(s, f);
  Mat4 r = Mat4::identity();
  r.at(0, 0) = s.x; r.at(0, 1) = s.y; r.at(0, 2) = s.z;
  r.at(1, 0) = u.x; r.at(1, 1) = u.y; r.at(1, 2) = u.z;
  r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z;
  r.at(0, 3) = -dot(s, eye);
  r.at(1, 3) = -dot(u, eye);
  r.at(2, 3) = dot(f, eye);
  return r;
}

}  // namespace ls
