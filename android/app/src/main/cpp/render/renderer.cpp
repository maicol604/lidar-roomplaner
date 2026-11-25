#include "renderer.h"

#include <android/log.h>
#include <unistd.h>

#include <chrono>
#include <cmath>

#include "../core/time_util.h"

#define LOG_TAG "LidarRenderer"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ls {
namespace {

constexpr int kBlockPoints = 65536;
constexpr size_t kMaxUploadPerFrame = 400000;
constexpr EGLint kEglOpenGlEs3Bit = 0x40;
constexpr float kFovY = 60.0f * kDeg2Rad;

const char* kPointVs = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aTime;
layout(location = 2) in float aIntensity;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform int uMode;
uniform float uFloorY;
uniform float uHeightRange;
uniform float uNow;
uniform float uAgeWindow;
uniform float uPointSize;
uniform float uProjScale;
uniform vec3 uBg;
out vec3 vColor;
out float vWorldY;
vec3 turbo(float x) {
  const vec4 kR4 = vec4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
  const vec4 kG4 = vec4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
  const vec4 kB4 = vec4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
  const vec2 kR2 = vec2(-152.94239396, 59.28637943);
  const vec2 kG2 = vec2(4.27729857, 2.82956604);
  const vec2 kB2 = vec2(-89.90310912, 27.34824973);
  vec4 v4 = vec4(1.0, x, x * x, x * x * x);
  vec2 v2 = v4.zw * v4.z;
  return vec3(dot(v4, kR4) + dot(v2, kR2), dot(v4, kG4) + dot(v2, kG2), dot(v4, kB4) + dot(v2, kB2));
}
void main() {
  vec4 w = uModel * vec4(aPos, 1.0);
  vWorldY = w.y;
  gl_Position = uMVP * vec4(aPos, 1.0);
  float t;
  if (uMode == 0) t = (w.y - uFloorY) / uHeightRange;
  else if (uMode == 1) t = aIntensity;
  else t = 1.0 - (uNow - aTime) / uAgeWindow;
  vColor = turbo(clamp(t, 0.0, 1.0));
  // Fade floor points toward the background so walls and furniture stand out.
  if (w.y - uFloorY < 0.06) vColor = mix(vColor, uBg, 0.7);
  gl_PointSize = clamp(uPointSize * uProjScale * 0.006 / max(gl_Position.w, 0.05), 1.5, 14.0);
}
)";

const char* kPointFs = R"(#version 300 es
precision mediump float;
in vec3 vColor;
in float vWorldY;
uniform float uClipY;
out vec4 frag;
void main() {
  if (vWorldY > uClipY) discard;
  vec2 c = gl_PointCoord * 2.0 - 1.0;
  if (dot(c, c) > 1.0) discard;
  frag = vec4(vColor, 1.0);
}
)";

const char* kLineVs = R"(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
  gl_Position = uMVP * vec4(aPos, 1.0);
  gl_PointSize = 5.0;
}
)";

const char* kLineFs = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 frag;
void main() { frag = uColor; }
)";

GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    LOGE("shader compile error: %s", log);
  }
  return s;
}

GLuint link(const char* vs, const char* fs) {
  GLuint p = glCreateProgram();
  GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetProgramInfoLog(p, sizeof(log), nullptr, log);
    LOGE("program link error: %s", log);
  }
  glDeleteShader(v);
  glDeleteShader(f);
  return p;
}

Vec3 orbitDir(float yaw, float pitch) {
  return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
}

}  // namespace

Renderer::Renderer(MapStore* store) : store_(store) {
  thread_ = std::thread([this] { run(); });
}

Renderer::~Renderer() {
  running_ = false;
  win_cv_.notify_all();
  if (thread_.joinable()) thread_.join();
}

void Renderer::setWindow(ANativeWindow* window) {
  std::unique_lock<std::mutex> l(win_mu_);
  if (pending_window_) ANativeWindow_release(pending_window_);
  pending_window_ = window;
  window_change_ = true;
  uint64_t id = ++window_change_id_;
  win_cv_.notify_all();
  win_cv_.wait_for(l, std::chrono::seconds(2), [&] { return window_applied_id_ >= id || !running_.load(); });
}

void Renderer::setOptions(const ViewOptions& o) {
  std::lock_guard<std::mutex> l(opt_mu_);
  opt_ = o;
}

ViewOptions Renderer::options() {
  std::lock_guard<std::mutex> l(opt_mu_);
  return opt_;
}

void Renderer::orbit(float dx, float dy) {
  std::lock_guard<std::mutex> l(opt_mu_);
  const float k = 0.006f;
  switch (opt_.camera) {
    case CameraMode::Orbit:
      cam_.orbit_yaw -= dx * k;
      cam_.orbit_pitch = std::clamp(cam_.orbit_pitch + dy * k, -1.45f, 1.45f);
      break;
    case CameraMode::Follow:
      cam_.follow_yaw_off -= dx * k;
      cam_.follow_pitch = std::clamp(cam_.follow_pitch + dy * k, -0.2f, 1.45f);
      break;
    case CameraMode::FirstPerson:
      cam_.fp_yaw -= dx * k * 0.5f;
      cam_.fp_pitch = std::clamp(cam_.fp_pitch + dy * k * 0.5f, -1.2f, 1.2f);
      break;
    case CameraMode::TopDown: {
      float mpp = cam_.top_height * 2.0f * std::tan(kFovY / 2) / float(std::max(1, height_));
      cam_.top_pan += Vec3{-dx * mpp, 0, -dy * mpp};
      break;
    }
  }
}

void Renderer::pan(float dx, float dy) {
  std::lock_guard<std::mutex> l(opt_mu_);
  if (opt_.camera == CameraMode::Orbit) {
    Vec3 right{std::cos(cam_.orbit_yaw), 0, -std::sin(cam_.orbit_yaw)};
    Vec3 fwd{-std::sin(cam_.orbit_yaw), 0, -std::cos(cam_.orbit_yaw)};
    float s = cam_.orbit_dist * 0.0012f;
    cam_.orbit_target += right * (-dx * s) + fwd * (dy * s);
  } else if (opt_.camera == CameraMode::TopDown) {
    float mpp = cam_.top_height * 2.0f * std::tan(kFovY / 2) / float(std::max(1, height_));
    cam_.top_pan += Vec3{-dx * mpp, 0, -dy * mpp};
  }
}

void Renderer::zoom(float scale) {
  if (scale <= 0.01f) return;
  std::lock_guard<std::mutex> l(opt_mu_);
  switch (opt_.camera) {
    case CameraMode::Orbit: cam_.orbit_dist = std::clamp(cam_.orbit_dist / scale, 0.5f, 80.0f); break;
    case CameraMode::Follow: cam_.follow_dist = std::clamp(cam_.follow_dist / scale, 0.6f, 25.0f); break;
    case CameraMode::TopDown: cam_.top_height = std::clamp(cam_.top_height / scale, 1.5f, 80.0f); break;
    case CameraMode::FirstPerson: break;
  }
}

void Renderer::resetView() {
  std::lock_guard<std::mutex> l(opt_mu_);
  cam_.orbit_init = false;
  cam_.follow_yaw_off = 0;
  cam_.follow_pitch = 0.5f;
  cam_.follow_dist = 3.5f;
  cam_.fp_yaw = cam_.fp_pitch = 0;
  cam_.top_pan = {0, 0, 0};
}

bool Renderer::initEgl() {
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, nullptr, nullptr)) return false;
  const EGLint attrs[] = {EGL_RENDERABLE_TYPE, kEglOpenGlEs3Bit, EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                          EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                          EGL_DEPTH_SIZE, 16, EGL_NONE};
  EGLint n = 0;
  if (!eglChooseConfig(display_, attrs, &config_, 1, &n) || n < 1) return false;
  const EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, ctx_attrs);
  if (context_ == EGL_NO_CONTEXT) return false;
  const EGLint pb_attrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  pbuffer_ = eglCreatePbufferSurface(display_, config_, pb_attrs);
  return eglMakeCurrent(display_, pbuffer_, pbuffer_, context_) == EGL_TRUE;
}

bool Renderer::initGl() {
  point_prog_ = link(kPointVs, kPointFs);
  line_prog_ = link(kLineVs, kLineFs);
  u_mvp_ = glGetUniformLocation(point_prog_, "uMVP");
  u_model_ = glGetUniformLocation(point_prog_, "uModel");
  u_mode_ = glGetUniformLocation(point_prog_, "uMode");
  u_floor_ = glGetUniformLocation(point_prog_, "uFloorY");
  u_hrange_ = glGetUniformLocation(point_prog_, "uHeightRange");
  u_now_ = glGetUniformLocation(point_prog_, "uNow");
  u_age_ = glGetUniformLocation(point_prog_, "uAgeWindow");
  u_psize_ = glGetUniformLocation(point_prog_, "uPointSize");
  u_proj_scale_ = glGetUniformLocation(point_prog_, "uProjScale");
  u_clip_y_ = glGetUniformLocation(point_prog_, "uClipY");
  u_bg_ = glGetUniformLocation(point_prog_, "uBg");
  ul_mvp_ = glGetUniformLocation(line_prog_, "uMVP");
  ul_color_ = glGetUniformLocation(line_prog_, "uColor");
  glGenVertexArrays(1, &dyn_vao_);
  glGenBuffers(1, &dyn_vbo_);
  glBindVertexArray(dyn_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, dyn_vbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), nullptr);
  glBindVertexArray(0);
  return true;
}

void Renderer::applyWindowChange() {
  eglMakeCurrent(display_, pbuffer_, pbuffer_, context_);
  if (surface_ != EGL_NO_SURFACE) {
    eglDestroySurface(display_, surface_);
    surface_ = EGL_NO_SURFACE;
  }
  if (window_) {
    ANativeWindow_release(window_);
    window_ = nullptr;
  }
  window_ = pending_window_;
  pending_window_ = nullptr;
  if (window_) {
    surface_ = eglCreateWindowSurface(display_, config_, window_, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
      LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
    } else {
      eglMakeCurrent(display_, surface_, surface_, context_);
    }
  }
  window_change_ = false;
  window_applied_id_ = window_change_id_;
  win_cv_.notify_all();
}

void Renderer::clearGpu() {
  for (auto& g : gpu_) {
    for (auto& b : g.blocks) {
      glDeleteBuffers(1, &b.vbo);
      glDeleteVertexArrays(1, &b.vao);
    }
    if (g.traj_vbo) glDeleteBuffers(1, &g.traj_vbo);
    if (g.traj_vao) glDeleteVertexArrays(1, &g.traj_vao);
  }
  gpu_.clear();
}

void Renderer::syncFromStore(Pose& device, bool& has_device, float& floor_y, float& now_s, bool& station) {
  staged_.clear();
  bool session_changed = false;
  {
    std::lock_guard<std::mutex> l(store_->mu);
    if (store_->session_id != session_) {
      session_ = store_->session_id;
      session_changed = true;
    }
  }
  if (session_changed) {
    clearGpu();
    cam_.smooth_init = false;
    std::lock_guard<std::mutex> l(opt_mu_);
    cam_.orbit_init = false;
  }
  std::lock_guard<std::mutex> l(store_->mu);
  size_t budget = kMaxUploadPerFrame;
  for (size_t i = 0; i < store_->submaps.size(); ++i) {
    if (i >= gpu_.size()) gpu_.emplace_back();
    const Submap& s = *store_->submaps[i];
    GpuSubmap& g = gpu_[i];
    bool reset = s.generation != g.generation;
    size_t from = reset ? 0 : g.uploaded;
    if (reset) g.generation = s.generation;
    size_t avail = s.points.size() > from ? s.points.size() - from : 0;
    size_t n = std::min(avail, budget);
    if (n > 0 || reset) {
      Staged st{i, reset, {}};
      st.points.assign(s.points.begin() + static_cast<long>(from), s.points.begin() + static_cast<long>(from + n));
      staged_.push_back(std::move(st));
      budget -= n;
    }
    g.target = s.worldPose();
    if (!g.has_pose) {
      g.shown = g.target;
      g.has_pose = true;
    }
    if (s.trajectory.size() != g.traj.size()) g.traj = s.trajectory;
  }
  if (store_->scanline_version != scanline_version_) {
    scanline_version_ = store_->scanline_version;
    scanline_ = store_->scanline;
    for (Vec3& p : scanline_) p = store_->display_correction.apply(p);
  }
  has_device = store_->has_device;
  device = store_->display_correction * store_->device.pose;
  floor_y = store_->floor_y;
  now_s = store_->t0_ns ? static_cast<float>((nowNs() - store_->t0_ns) * 1e-9) : store_->view_now_s;
  station = store_->station_mode;
  has_view_center_ = store_->has_view_center;
  view_center_ = store_->view_center;
  view_radius_ = store_->view_radius;
  if (session_changed && has_view_center_) pending_view_init_ = true;
}

void Renderer::uploadPoints(GpuSubmap& g, const std::vector<MapPoint>& pts) {
  size_t i = 0;
  while (i < pts.size()) {
    if (g.blocks.empty() || g.blocks.back().count >= kBlockPoints) {
      GpuBlock b;
      glGenVertexArrays(1, &b.vao);
      glGenBuffers(1, &b.vbo);
      glBindVertexArray(b.vao);
      glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
      glBufferData(GL_ARRAY_BUFFER, kBlockPoints * sizeof(MapPoint), nullptr, GL_DYNAMIC_DRAW);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MapPoint), reinterpret_cast<void*>(0));
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(MapPoint), reinterpret_cast<void*>(12));
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(MapPoint), reinterpret_cast<void*>(16));
      glBindVertexArray(0);
      g.blocks.push_back(b);
    }
    GpuBlock& b = g.blocks.back();
    size_t n = std::min(pts.size() - i, static_cast<size_t>(kBlockPoints - b.count));
    glBindBuffer(GL_ARRAY_BUFFER, b.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(b.count * sizeof(MapPoint)),
                    static_cast<GLsizeiptr>(n * sizeof(MapPoint)), &pts[i]);
    b.count += static_cast<int>(n);
    g.uploaded += n;
    i += n;
  }
}

void Renderer::drawLines(const std::vector<Vec3>& verts, GLenum mode, const Mat4& mvp, float r, float g, float b,
                         float a) {
  if (verts.empty()) return;
  glUseProgram(line_prog_);
  glUniformMatrix4fv(ul_mvp_, 1, GL_FALSE, mvp.m);
  glUniform4f(ul_color_, r, g, b, a);
  glBindVertexArray(dyn_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, dyn_vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(Vec3)), verts.data(), GL_STREAM_DRAW);
  glDrawArrays(mode, 0, static_cast<GLsizei>(verts.size()));
  glBindVertexArray(0);
}

Mat4 Renderer::viewMatrix(const Pose& device, bool has_device, float dt) {
  Vec3 dev_pos = has_device ? device.t : (has_view_center_ ? view_center_ : Vec3{0, 0, 0});
  Vec3 f = has_device ? rotate(device.q, {0, 0, -1}) : Vec3{0, 0, -1};
  float heading = std::atan2(-f.x, -f.z);
  if (pending_view_init_) {
    // Frame a freshly loaded saved cloud so it fits the viewport in both directions.
    pending_view_init_ = false;
    float aspect = float(width_) / float(std::max(1, height_));
    float fit = view_radius_ * 1.1f / (std::tan(kFovY / 2) * std::min(1.0f, aspect));
    std::lock_guard<std::mutex> l(opt_mu_);
    cam_.orbit_init = false;
    cam_.orbit_pitch = 0.9f;
    cam_.orbit_dist = std::clamp(fit, 2.0f, 120.0f);
    cam_.top_height = std::clamp(fit, 3.0f, 120.0f);
    cam_.top_pan = {0, 0, 0};
    cam_.smooth_init = false;
  }
  if (!cam_.smooth_init) {
    cam_.smooth_center = dev_pos;
    cam_.smooth_heading = heading;
    cam_.smooth_init = true;
  } else {
    cam_.smooth_center = lerp(cam_.smooth_center, dev_pos, std::min(1.0f, dt * 5.0f));
    float d = std::remainder(heading - cam_.smooth_heading, 2 * kPi);
    cam_.smooth_heading += d * std::min(1.0f, dt * 2.5f);
  }
  std::lock_guard<std::mutex> l(opt_mu_);
  const Vec3 up{0, 1, 0};
  switch (opt_.camera) {
    case CameraMode::Orbit: {
      if (!cam_.orbit_init) {
        cam_.orbit_target = dev_pos;
        cam_.orbit_init = true;
      }
      Vec3 eye = cam_.orbit_target + orbitDir(cam_.orbit_yaw, cam_.orbit_pitch) * cam_.orbit_dist;
      return lookAt(eye, cam_.orbit_target, up);
    }
    case CameraMode::Follow: {
      float yaw = cam_.smooth_heading + cam_.follow_yaw_off;
      Vec3 eye = cam_.smooth_center + orbitDir(yaw, cam_.follow_pitch) * cam_.follow_dist;
      return lookAt(eye, cam_.smooth_center, up);
    }
    case CameraMode::FirstPerson: {
      Pose p = device;
      p.q = normalize(p.q * axisAngle({0, 1, 0}, cam_.fp_yaw) * axisAngle({1, 0, 0}, cam_.fp_pitch));
      return fromPose(p.inverse());
    }
    case CameraMode::TopDown: {
      Vec3 c = cam_.smooth_center + cam_.top_pan;
      return lookAt(c + Vec3{0, cam_.top_height, 0}, c, {0, 0, -1});
    }
  }
  return Mat4::identity();
}

void Renderer::drawFrame(float dt) {
  Pose device;
  bool has_device = false, station = false;
  float floor_y = 0, now_s = 0;
  syncFromStore(device, has_device, floor_y, now_s, station);
  for (Staged& st : staged_) {
    GpuSubmap& g = gpu_[st.index];
    if (st.reset) {
      for (auto& b : g.blocks) {
        glDeleteBuffers(1, &b.vbo);
        glDeleteVertexArrays(1, &b.vao);
      }
      g.blocks.clear();
      g.uploaded = 0;
    }
    uploadPoints(g, st.points);
  }
  float a = 1.0f - std::exp(-dt / 0.25f);
  for (auto& g : gpu_) g.shown = interpolate(g.shown, g.target, a);

  ViewOptions o = options();
  eglQuerySurface(display_, surface_, EGL_WIDTH, &width_);
  eglQuerySurface(display_, surface_, EGL_HEIGHT, &height_);
  glViewport(0, 0, width_, height_);
  if (o.dark) glClearColor(0.055f, 0.067f, 0.086f, 1.0f);
  else glClearColor(0.93f, 0.945f, 0.96f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  proj_ = perspective(kFovY, float(width_) / float(std::max(1, height_)), 0.05f, 300.0f);
  view_ = viewMatrix(device, has_device, dt);
  Mat4 vp = proj_ * view_;
  float proj_scale = float(height_) / (2.0f * std::tan(kFovY / 2));

  if (o.show_grid) {
    std::vector<Vec3> grid;
    Vec3 c = has_device ? device.t : (has_view_center_ ? view_center_ : Vec3{0, 0, 0});
    float cx = std::round(c.x), cz = std::round(c.z);
    const int r = has_device ? 15 : std::clamp(static_cast<int>(view_radius_) + 3, 5, 40);
    for (int i = -r; i <= r; ++i) {
      grid.push_back({cx + i, floor_y, cz - r});
      grid.push_back({cx + i, floor_y, cz + r});
      grid.push_back({cx - r, floor_y, cz + i});
      grid.push_back({cx + r, floor_y, cz + i});
    }
    glDepthMask(GL_FALSE);
    if (o.dark) drawLines(grid, GL_LINES, vp, 1, 1, 1, 0.07f);
    else drawLines(grid, GL_LINES, vp, 0, 0, 0, 0.09f);
    glDepthMask(GL_TRUE);
  }

  glUseProgram(point_prog_);
  glUniform1i(u_mode_, static_cast<int>(o.color));
  glUniform1f(u_floor_, floor_y);
  glUniform1f(u_hrange_, 2.6f);
  glUniform1f(u_now_, now_s);
  glUniform1f(u_age_, std::max(1.0f, o.age_window_s));
  glUniform1f(u_psize_, o.point_size);
  glUniform1f(u_proj_scale_, proj_scale);
  // Plan view and "dollhouse" mode: hide the ceiling so walls and furniture read like a floor plan.
  bool clip = o.camera == CameraMode::TopDown || o.clip_ceiling;
  glUniform1f(u_clip_y_, clip ? floor_y + 1.9f : 1e9f);
  if (o.dark) glUniform3f(u_bg_, 0.055f, 0.067f, 0.086f);
  else glUniform3f(u_bg_, 0.93f, 0.945f, 0.96f);
  for (auto& g : gpu_) {
    Mat4 model = fromPose(g.shown);
    Mat4 mvp = vp * model;
    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(u_model_, 1, GL_FALSE, model.m);
    for (auto& b : g.blocks) {
      glBindVertexArray(b.vao);
      glDrawArrays(GL_POINTS, 0, b.count);
    }
  }
  glBindVertexArray(0);

  if (o.show_trajectory) {
    glLineWidth(3.0f);
    for (auto& g : gpu_) {
      if (g.traj.size() < 2) continue;
      Mat4 mvp = vp * fromPose(g.shown);
      drawLines(g.traj, GL_LINE_STRIP, mvp, 1.0f, 0.6f, 0.0f, 0.95f);
    }
    glLineWidth(1.0f);
  }

  if (!scanline_.empty()) {
    if (o.dark) drawLines(scanline_, GL_POINTS, vp, 1.0f, 1.0f, 1.0f, 0.95f);
    else drawLines(scanline_, GL_POINTS, vp, 0.85f, 0.0f, 0.45f, 0.95f);
  }

  if (has_device && o.camera != CameraMode::FirstPerson) {
    // Phone frustum + LiDAR spin axis.
    const float w = 0.08f, h = 0.15f, d = 0.22f;
    std::vector<Vec3> m = {{0, 0, 0}, {-w, -h, -d}, {0, 0, 0}, {w, -h, -d}, {0, 0, 0}, {w, h, -d},
                           {0, 0, 0}, {-w, h, -d},  {-w, -h, -d}, {w, -h, -d}, {w, -h, -d}, {w, h, -d},
                           {w, h, -d}, {-w, h, -d}, {-w, h, -d}, {-w, -h, -d}};
    std::lock_guard<std::mutex> l(store_->mu);
    Pose lidar = store_->T_device_lidar;
    Vec3 lc = lidar.t, axis = rotate(lidar.q, {0, 0, 1});
    m.push_back(lc);
    m.push_back(lc + axis * 0.3f);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);
    Mat4 mvp = vp * fromPose(device);
    if (station) drawLines(m, GL_LINES, mvp, 0.7f, 0.4f, 1.0f, 1.0f);
    else drawLines(m, GL_LINES, mvp, 0.0f, 0.8f, 1.0f, 1.0f);
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
  }
}

void Renderer::run() {
  if (!initEgl()) {
    LOGE("EGL init failed");
    running_ = false;
    win_cv_.notify_all();
    return;
  }
  initGl();
  auto last = std::chrono::steady_clock::now();
  auto fps_t0 = last;
  int frames = 0;
  while (running_.load()) {
    {
      std::unique_lock<std::mutex> l(win_mu_);
      if (window_change_) applyWindowChange();
      if (surface_ == EGL_NO_SURFACE) {
        win_cv_.wait_for(l, std::chrono::milliseconds(100), [&] { return window_change_ || !running_.load(); });
        continue;
      }
    }
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last).count();
    last = now;
    drawFrame(std::clamp(dt, 0.0f, 0.1f));
    eglSwapBuffers(display_, surface_);
    ++frames;
    float el = std::chrono::duration<float>(now - fps_t0).count();
    if (el >= 1.0f) {
      fps_ = frames / el;
      frames = 0;
      fps_t0 = now;
    }
    int target = std::max(15, options().target_fps);
    auto frame_end = now + std::chrono::microseconds(1000000 / target);
    auto after = std::chrono::steady_clock::now();
    if (after < frame_end) std::this_thread::sleep_for(frame_end - after);
  }
  {
    std::lock_guard<std::mutex> l(win_mu_);
    clearGpu();
    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
    if (pbuffer_ != EGL_NO_SURFACE) eglDestroySurface(display_, pbuffer_);
    if (window_) ANativeWindow_release(window_);
    if (pending_window_) ANativeWindow_release(pending_window_);
    eglDestroyContext(display_, context_);
    eglTerminate(display_);
  }
}

}  // namespace ls
