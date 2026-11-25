#include "ply_reader.h"

#include <cstdio>
#include <cstring>
#include <sstream>

namespace ls {
namespace {

int typeSize(const std::string& t) {
  if (t == "char" || t == "uchar" || t == "int8" || t == "uint8") return 1;
  if (t == "short" || t == "ushort" || t == "int16" || t == "uint16") return 2;
  if (t == "int" || t == "uint" || t == "float" || t == "int32" || t == "uint32" || t == "float32") return 4;
  if (t == "double" || t == "float64") return 8;
  return 0;
}

double readAs(const uint8_t* p, const std::string& t) {
  if (t == "float" || t == "float32") { float v; std::memcpy(&v, p, 4); return v; }
  if (t == "double" || t == "float64") { double v; std::memcpy(&v, p, 8); return v; }
  if (t == "uchar" || t == "uint8") return p[0];
  if (t == "char" || t == "int8") return static_cast<int8_t>(p[0]);
  if (t == "ushort" || t == "uint16") { uint16_t v; std::memcpy(&v, p, 2); return v; }
  if (t == "short" || t == "int16") { int16_t v; std::memcpy(&v, p, 2); return v; }
  if (t == "uint" || t == "uint32") { uint32_t v; std::memcpy(&v, p, 4); return v; }
  if (t == "int" || t == "int32") { int32_t v; std::memcpy(&v, p, 4); return v; }
  return 0;
}

struct Prop {
  std::string name, type;
  int offset;
};

}  // namespace

bool readPly(const std::string& path, LoadedCloud& out, std::string& error) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    error = "cannot open " + path;
    return false;
  }
  char line[512];
  bool binary_le = false, in_vertex = false, vertex_first = false, seen_element = false;
  long long count = 0;
  std::vector<Prop> props;
  int stride = 0;
  if (!std::fgets(line, sizeof(line), f) || std::strncmp(line, "ply", 3) != 0) {
    std::fclose(f);
    error = "not a PLY file";
    return false;
  }
  while (std::fgets(line, sizeof(line), f)) {
    std::istringstream ss(line);
    std::string kw;
    ss >> kw;
    if (kw == "format") {
      std::string fmt;
      ss >> fmt;
      binary_le = fmt == "binary_little_endian";
    } else if (kw == "element") {
      std::string name;
      ss >> name;
      in_vertex = name == "vertex";
      if (in_vertex) {
        ss >> count;
        vertex_first = !seen_element;
      }
      seen_element = true;
    } else if (kw == "property" && in_vertex) {
      std::string type, name;
      ss >> type >> name;
      int sz = typeSize(type);
      if (sz == 0) {
        std::fclose(f);
        error = "unsupported property type " + type;
        return false;
      }
      props.push_back({name, type, stride});
      stride += sz;
    } else if (kw == "end_header") {
      break;
    }
  }
  const Prop *px = nullptr, *py = nullptr, *pz = nullptr, *pi = nullptr, *pt = nullptr;
  for (const Prop& p : props) {
    if (p.name == "x") px = &p;
    else if (p.name == "y") py = &p;
    else if (p.name == "z") pz = &p;
    else if (p.name == "intensity" || p.name == "scalar_intensity") pi = &p;
    else if (p.name == "scalar_time" || p.name == "time") pt = &p;
  }
  if (!binary_le || !vertex_first || !px || !py || !pz || count <= 0) {
    std::fclose(f);
    error = "unsupported PLY layout";
    return false;
  }
  out.points.clear();
  out.intensity.clear();
  out.time.clear();
  out.points.reserve(static_cast<size_t>(count));
  out.intensity.reserve(static_cast<size_t>(count));
  out.time.reserve(static_cast<size_t>(count));
  const size_t kBatch = 65536;
  std::vector<uint8_t> buf(kBatch * stride);
  long long left = count;
  while (left > 0) {
    size_t n = static_cast<size_t>(std::min<long long>(left, kBatch));
    size_t got = std::fread(buf.data(), stride, n, f);
    for (size_t i = 0; i < got; ++i) {
      const uint8_t* r = &buf[i * stride];
      float x = static_cast<float>(readAs(r + px->offset, px->type));
      float y = static_cast<float>(readAs(r + py->offset, py->type));
      float z = static_cast<float>(readAs(r + pz->offset, pz->type));
      out.points.push_back({x, z, -y});  // Z up -> Y up
      out.intensity.push_back(pi ? static_cast<uint8_t>(readAs(r + pi->offset, pi->type)) : 128);
      out.time.push_back(pt ? static_cast<float>(readAs(r + pt->offset, pt->type)) : 0.0f);
    }
    if (got < n) break;  // truncated file: keep what was read
    left -= static_cast<long long>(n);
  }
  std::fclose(f);
  if (out.points.empty()) {
    error = "empty cloud";
    return false;
  }
  return true;
}

bool readTrajectoryCsv(const std::string& path, LoadedCloud& out) {
  FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return false;
  char line[256];
  out.trajectory.clear();
  while (std::fgets(line, sizeof(line), f)) {
    double t, x, y, z;
    if (std::sscanf(line, "%lf,%lf,%lf,%lf", &t, &x, &y, &z) == 4)
      out.trajectory.push_back({static_cast<float>(x), static_cast<float>(z), static_cast<float>(-y)});
  }
  std::fclose(f);
  return true;
}

}  // namespace ls
