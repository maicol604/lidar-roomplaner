#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "../core/map_store.h"

namespace ls {

enum class CameraMode : int { Orbit = 0, Follow = 1, FirstPerson = 2, TopDown = 3 };
enum class ColorMode : int { Height = 0, Intensity = 1, Age = 2 };

struct ViewOptions {
  CameraMode camera = CameraMode::Follow;
  ColorMode color = ColorMode::Height;
  float point_size = 1.0f;
  bool dark = true;
  bool show_trajectory = true;
  bool show_grid = true;
  float age_window_s = 60.0f;
  int target_fps = 60;
  bool clip_ceiling = false;
};

// Renders the MapStore into a Flutter SurfaceProducer surface on a dedicated GL thread.
class Renderer {
 public:
  explicit Renderer(MapStore* store);
  ~Renderer();

  // Attaches (or detaches with nullptr) the output window. Blocks until applied.
  void setWindow(ANativeWindow* window);
  void setOptions(const ViewOptions& o);
  ViewOptions options();

  // Gestures (deltas in physical pixels).
  void orbit(float dx, float dy);
  void pan(float dx, float dy);
  void zoom(float scale);
  void resetView();
  float fps() const { return fps_.load(); }

 private:
  struct GpuBlock {
    GLuint vao = 0, vbo = 0;
    int count = 0;
  };
  struct GpuSubmap {
    uint32_t generation = 0;
    size_t uploaded = 0;
    std::vector<GpuBlock> blocks;
    Pose target, shown;
    bool has_pose = false;
    std::vector<Vec3> traj;
    GLuint traj_vao = 0, traj_vbo = 0;
    size_t traj_uploaded = 0;
  };
  struct Staged {
    size_t index;
    bool reset;
    std::vector<MapPoint> points;
  };
  struct Camera {
    Vec3 orbit_target{0, 0, 0};
    float orbit_yaw = 0.8f, orbit_pitch = 0.7f, orbit_dist = 8.0f;
    bool orbit_init = false;
    float follow_yaw_off = 0, follow_pitch = 0.5f, follow_dist = 3.5f;
    float fp_yaw = 0, fp_pitch = 0;
    float top_height = 14.0f;
    Vec3 top_pan{0, 0, 0};
    Vec3 smooth_center{0, 0, 0};
    float smooth_heading = 0;
    bool smooth_init = false;
  };

  void run();
  bool initEgl();
  void applyWindowChange();
  bool initGl();
  void syncFromStore(Pose& device, bool& has_device, float& floor_y, float& now_s, bool& station);
  void uploadPoints(GpuSubmap& g, const std::vector<MapPoint>& pts);
  void clearGpu();
  void drawFrame(float dt);
  Mat4 viewMatrix(const Pose& device, bool has_device, float dt);
  void drawLines(const std::vector<Vec3>& verts, GLenum mode, const Mat4& mvp, float r, float g, float b, float a);

  MapStore* store_;
  std::thread thread_;
  std::atomic<bool> running_{true};

  std::mutex win_mu_;
  std::condition_variable win_cv_;
  ANativeWindow* pending_window_ = nullptr;
  bool window_change_ = false;
  uint64_t window_change_id_ = 0, window_applied_id_ = 0;

  ANativeWindow* window_ = nullptr;
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLConfig config_ = nullptr;
  EGLContext context_ = EGL_NO_CONTEXT;
  EGLSurface surface_ = EGL_NO_SURFACE;
  EGLSurface pbuffer_ = EGL_NO_SURFACE;
  int width_ = 1, height_ = 1;

  std::mutex opt_mu_;
  ViewOptions opt_;
  Camera cam_;

  GLuint point_prog_ = 0, line_prog_ = 0;
  GLint u_mvp_ = -1, u_model_ = -1, u_mode_ = -1, u_floor_ = -1, u_hrange_ = -1, u_now_ = -1, u_age_ = -1,
        u_psize_ = -1, u_proj_scale_ = -1, u_clip_y_ = -1, u_bg_ = -1;
  GLint ul_mvp_ = -1, ul_color_ = -1;
  GLuint dyn_vao_ = 0, dyn_vbo_ = 0;

  uint64_t session_ = ~0ULL;
  std::vector<GpuSubmap> gpu_;
  std::vector<Staged> staged_;
  std::vector<Vec3> scanline_;
  uint64_t scanline_version_ = ~0ULL;

  std::atomic<float> fps_{0};
  Mat4 proj_;
  Mat4 view_;
  bool has_view_center_ = false;
  Vec3 view_center_;
  float view_radius_ = 5.0f;
  bool pending_view_init_ = false;
};

}  // namespace ls
