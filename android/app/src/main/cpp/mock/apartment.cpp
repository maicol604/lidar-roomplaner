#include "apartment.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ls::mock {
namespace {
constexpr float kWallHalf = 0.05f;
constexpr float kDoorTop = 2.05f;
constexpr float kSill = 0.9f;
constexpr float kWindowTop = 2.1f;
}  // namespace

void Apartment::wallAlongX(float z, float x0, float x1, std::vector<Gap> gaps) {
  std::sort(gaps.begin(), gaps.end(), [](const Gap& a, const Gap& b) { return a.a < b.a; });
  const float c = Apartment::kCeiling;
  float cur = x0;
  auto seg = [&](float a, float b, float y0, float y1, Material m, float half) {
    if (b - a > 1e-3f) add({a, y0, z - half}, {b, y1, z + half}, m);
  };
  for (const Gap& g : gaps) {
    seg(cur, g.a, 0, c, Material::Paint, kWallHalf);
    if (g.window) {
      seg(g.a, g.b, 0, kSill, Material::Paint, kWallHalf);
      seg(g.a, g.b, kWindowTop, c, Material::Paint, kWallHalf);
      seg(g.a, g.b, kSill, kWindowTop, Material::Glass, 0.01f);
    } else {
      seg(g.a, g.b, kDoorTop, c, Material::Paint, kWallHalf);
    }
    cur = g.b;
  }
  seg(cur, x1, 0, c, Material::Paint, kWallHalf);
}

void Apartment::wallAlongZ(float x, float z0, float z1, std::vector<Gap> gaps) {
  std::sort(gaps.begin(), gaps.end(), [](const Gap& a, const Gap& b) { return a.a < b.a; });
  const float c = Apartment::kCeiling;
  float cur = z0;
  auto seg = [&](float a, float b, float y0, float y1, Material m, float half) {
    if (b - a > 1e-3f) add({x - half, y0, a}, {x + half, y1, b}, m);
  };
  for (const Gap& g : gaps) {
    seg(cur, g.a, 0, c, Material::Paint, kWallHalf);
    if (g.window) {
      seg(g.a, g.b, 0, kSill, Material::Paint, kWallHalf);
      seg(g.a, g.b, kWindowTop, c, Material::Paint, kWallHalf);
      seg(g.a, g.b, kSill, kWindowTop, Material::Glass, 0.01f);
    } else {
      seg(g.a, g.b, kDoorTop, c, Material::Paint, kWallHalf);
    }
    cur = g.b;
  }
  seg(cur, z1, 0, c, Material::Paint, kWallHalf);
}

void Apartment::table(float x0, float z0, float x1, float z1, float h, Material m) {
  add({x0, h - 0.04f, z0}, {x1, h, z1}, m);
  const float l = 0.05f;
  add({x0, 0, z0}, {x0 + l, h - 0.04f, z0 + l}, m);
  add({x1 - l, 0, z0}, {x1, h - 0.04f, z0 + l}, m);
  add({x0, 0, z1 - l}, {x0 + l, h - 0.04f, z1}, m);
  add({x1 - l, 0, z1 - l}, {x1, h - 0.04f, z1}, m);
}

Apartment::Apartment() {
  const float c = kCeiling;
  rooms_ = {{"Salón", 0, 0, 6, 4.5f},      {"Cocina", 6, 0, 10, 4.5f},     {"Pasillo", 0, 4.5f, 10, 5.7f},
            {"Dormitorio 1", 0, 5.7f, 4, 8}, {"Baño", 4, 5.7f, 6.2f, 8}, {"Dormitorio 2", 6.2f, 5.7f, 10, 8}};

  // Floor and ceiling slabs end at the exterior wall faces so rays leaving through windows
  // find nothing outside (no returns), as with a real open window to the street.
  add({-0.05f, -0.1f, -0.05f}, {10.05f, 0.0f, 8.05f}, Material::Floor);
  add({-0.05f, c, -0.05f}, {10.05f, c + 0.1f, 8.05f}, Material::Ceiling);

  // Exterior walls.
  wallAlongX(0.0f, -0.05f, 10.05f, {{1.5f, 3.5f, true}, {7.0f, 9.0f, true}});
  wallAlongX(8.0f, -0.05f, 10.05f, {{1.0f, 3.0f, true}, {4.8f, 5.6f, true}, {7.5f, 9.0f, true}});
  wallAlongZ(0.0f, 0.0f, 8.0f, {{1.5f, 3.0f, true}});
  wallAlongZ(10.0f, 0.0f, 8.0f, {{6.3f, 7.5f, true}});

  // Interior walls.
  wallAlongZ(6.0f, 0.05f, 4.45f, {{1.6f, 2.8f, false}});
  wallAlongX(4.5f, 0.05f, 9.95f, {{4.6f, 5.5f, false}, {8.5f, 9.35f, false}});
  wallAlongX(5.7f, 0.05f, 9.95f, {{3.0f, 3.85f, false}, {4.9f, 5.6f, false}, {6.5f, 7.35f, false}});
  wallAlongZ(4.0f, 5.75f, 7.95f, {});
  wallAlongZ(6.2f, 5.75f, 7.95f, {});

  // Salón.
  add({0.6f, 0, 3.45f}, {2.8f, 0.45f, 4.4f}, Material::Fabric);   // sofá asiento
  add({0.6f, 0, 4.15f}, {2.8f, 0.85f, 4.4f}, Material::Fabric);   // respaldo
  add({0.45f, 0, 3.45f}, {0.6f, 0.6f, 4.4f}, Material::Fabric);   // brazo
  add({2.8f, 0, 3.45f}, {2.95f, 0.6f, 4.4f}, Material::Fabric);   // brazo
  table(1.2f, 2.4f, 2.2f, 2.9f, 0.42f, Material::Wood);           // mesa de centro
  add({0.8f, 0, 0.1f}, {2.8f, 0.5f, 0.5f}, Material::Wood);       // mueble TV
  add({1.2f, 0.8f, 0.22f}, {2.4f, 1.5f, 0.28f}, Material::Dark);  // TV
  table(3.8f, 1.0f, 5.4f, 1.9f, 0.76f, Material::Wood);           // mesa comedor
  for (float cx : {4.2f, 5.0f}) {
    add({cx - 0.2f, 0.43f, 0.55f}, {cx + 0.2f, 0.47f, 0.95f}, Material::Wood);
    add({cx - 0.2f, 0.0f, 0.55f}, {cx + 0.2f, 0.95f, 0.6f}, Material::Wood);
    add({cx - 0.2f, 0.43f, 1.95f}, {cx + 0.2f, 0.47f, 2.35f}, Material::Wood);
    add({cx - 0.2f, 0.0f, 2.3f}, {cx + 0.2f, 0.95f, 2.35f}, Material::Wood);
  }
  add({5.55f, 0, 2.9f}, {5.95f, 1.9f, 4.3f}, Material::Wood);   // estantería
  add({0.2f, 0, 0.2f}, {0.35f, 1.6f, 0.35f}, Material::Metal);  // lámpara de pie

  // Cocina.
  add({6.1f, 0, 0.05f}, {9.95f, 0.88f, 0.65f}, Material::Wood);
  add({6.1f, 0.88f, 0.05f}, {9.95f, 0.92f, 0.65f}, Material::Tile);
  add({6.1f, 1.5f, 0.05f}, {6.95f, 2.2f, 0.4f}, Material::Wood);
  add({9.05f, 1.5f, 0.05f}, {9.95f, 2.2f, 0.4f}, Material::Wood);
  add({9.2f, 0, 0.7f}, {9.9f, 1.85f, 1.4f}, Material::Metal);  // nevera
  add({7.2f, 0, 2.2f}, {8.6f, 0.9f, 3.0f}, Material::Wood);    // isla

  // Pasillo.
  add({1.0f, 0, 5.35f}, {1.8f, 0.8f, 5.65f}, Material::Wood);
  add({1.0f, 1.0f, 5.62f}, {1.8f, 1.8f, 5.65f}, Material::Mirror);

  // Dormitorio 1.
  add({0.1f, 0, 6.2f}, {2.1f, 0.55f, 7.8f}, Material::Fabric);
  add({0.05f, 0, 6.2f}, {0.15f, 1.0f, 7.8f}, Material::Wood);
  add({3.3f, 0, 6.0f}, {3.9f, 2.2f, 7.6f}, Material::Wood);
  table(2.3f, 7.35f, 3.2f, 7.9f, 0.76f, Material::Wood);

  // Baño.
  add({4.1f, 0, 7.2f}, {6.1f, 0.55f, 7.9f}, Material::Tile);
  add({4.1f, 0, 6.0f}, {4.5f, 0.42f, 6.6f}, Material::Tile);
  add({5.6f, 0, 6.1f}, {6.1f, 0.8f, 6.7f}, Material::Wood);
  add({5.6f, 0.8f, 6.1f}, {6.1f, 0.9f, 6.7f}, Material::Tile);
  add({6.12f, 1.1f, 6.1f}, {6.15f, 1.9f, 6.7f}, Material::Mirror);

  // Dormitorio 2.
  add({8.0f, 0, 6.0f}, {9.6f, 0.55f, 7.9f}, Material::Fabric);
  add({9.6f, 0, 6.0f}, {9.95f, 0.5f, 6.4f}, Material::Wood);
  add({6.3f, 0, 6.6f}, {6.9f, 2.2f, 7.9f}, Material::Wood);
}

bool Apartment::raycast(const Vec3& o, const Vec3& d, float tmax, Hit& hit, int ignore_box) const {
  float best = tmax;
  int best_i = -1;
  int best_axis = 0;
  const float inv[3] = {std::fabs(d.x) > 1e-9f ? 1.0f / d.x : std::numeric_limits<float>::infinity(),
                        std::fabs(d.y) > 1e-9f ? 1.0f / d.y : std::numeric_limits<float>::infinity(),
                        std::fabs(d.z) > 1e-9f ? 1.0f / d.z : std::numeric_limits<float>::infinity()};
  const float oo[3] = {o.x, o.y, o.z};
  for (size_t i = 0; i < boxes_.size(); ++i) {
    if (static_cast<int>(i) == ignore_box) continue;
    const Box& b = boxes_[i];
    const float mn[3] = {b.min.x, b.min.y, b.min.z};
    const float mx[3] = {b.max.x, b.max.y, b.max.z};
    float t0 = -std::numeric_limits<float>::infinity(), t1 = std::numeric_limits<float>::infinity();
    int axis = 0;
    bool miss = false;
    for (int a = 0; a < 3; ++a) {
      if (std::isinf(inv[a])) {
        if (oo[a] < mn[a] || oo[a] > mx[a]) {
          miss = true;
          break;
        }
        continue;
      }
      float ta = (mn[a] - oo[a]) * inv[a];
      float tb = (mx[a] - oo[a]) * inv[a];
      if (ta > tb) std::swap(ta, tb);
      if (ta > t0) {
        t0 = ta;
        axis = a;
      }
      t1 = std::min(t1, tb);
      if (t0 > t1) {
        miss = true;
        break;
      }
    }
    if (miss || t0 <= 1e-4f || t0 >= best) continue;
    best = t0;
    best_i = static_cast<int>(i);
    best_axis = axis;
  }
  if (best_i < 0) return false;
  hit.t = best;
  hit.box = best_i;
  hit.mat = boxes_[best_i].mat;
  Vec3 n{0, 0, 0};
  float s = 0;
  if (best_axis == 0) s = d.x; else if (best_axis == 1) s = d.y; else s = d.z;
  float sign = s > 0 ? -1.0f : 1.0f;
  if (best_axis == 0) n.x = sign; else if (best_axis == 1) n.y = sign; else n.z = sign;
  hit.normal = n;
  return true;
}

bool Apartment::collides(float x, float z, float radius) const {
  for (const Box& b : boxes_) {
    if (b.mat == Material::Floor || b.mat == Material::Ceiling) continue;
    if (b.max.y < 0.15f || b.min.y > 1.8f) continue;
    float cx = std::clamp(x, b.min.x, b.max.x);
    float cz = std::clamp(z, b.min.z, b.max.z);
    float dx = x - cx, dz = z - cz;
    if (dx * dx + dz * dz < radius * radius) return true;
  }
  return false;
}

}  // namespace ls::mock
