#pragma once

#include <string>
#include <vector>

#include "../core/math.h"

namespace ls::mock {

enum class Material : uint8_t { Paint, Wood, Fabric, Metal, Glass, Mirror, Tile, Dark, Floor, Ceiling };

struct Box {
  Vec3 min, max;
  Material mat;
};

struct Hit {
  float t = 0;
  Material mat = Material::Paint;
  Vec3 normal;
  int box = -1;
};

struct Room {
  std::string name;
  float x0, z0, x1, z1;
};

// Fixed synthetic apartment (meters, Y up, floor at y = 0). Plan view: X to the right, Z downwards.
// Salón + cocina abierta al norte, pasillo central, dormitorio 1, baño y dormitorio 2 al sur.
class Apartment {
 public:
  Apartment();
  bool raycast(const Vec3& o, const Vec3& d, float tmax, Hit& hit, int ignore_box = -1) const;
  bool collides(float x, float z, float radius) const;
  const std::vector<Box>& boxes() const { return boxes_; }
  const std::vector<Room>& rooms() const { return rooms_; }
  Vec3 startPosition() const { return {3.0f, 1.4f, 2.9f}; }
  float startYaw() const { return 0.0f; }  // facing -Z (north)
  static constexpr float kCeiling = 2.6f;

 private:
  void add(Vec3 mn, Vec3 mx, Material m) { boxes_.push_back({mn, mx, m}); }
  struct Gap {
    float a, b;
    bool window;  // false = door
  };
  void wallAlongX(float z, float x0, float x1, std::vector<Gap> gaps);
  void wallAlongZ(float x, float z0, float z1, std::vector<Gap> gaps);
  void table(float x0, float z0, float x1, float z1, float h, Material m);

  std::vector<Box> boxes_;
  std::vector<Room> rooms_;
};

}  // namespace ls::mock
