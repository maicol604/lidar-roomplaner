#include <android/native_window_jni.h>
#include <jni.h>

#include <cstring>
#include <string>

#include "scanner_core.h"

using ls::ScannerCore;

namespace {
std::string toString(JNIEnv* env, jstring s) {
  if (!s) return {};
  const char* c = env->GetStringUTFChars(s, nullptr);
  std::string out(c);
  env->ReleaseStringUTFChars(s, c);
  return out;
}
}  // namespace

#define JNI_FN(name) Java_com_example_lidarscanner_NativeBridge_##name

extern "C" {

JNIEXPORT jboolean JNICALL JNI_FN(nativeStart)(JNIEnv* env, jobject, jstring config) {
  return ScannerCore::instance().start(ls::ScanConfig::parse(toString(env, config))) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL JNI_FN(nativeStop)(JNIEnv* env, jobject) {
  return env->NewStringUTF(ScannerCore::instance().stop().c_str());
}

JNIEXPORT jstring JNICALL JNI_FN(nativeLoadCloud)(JNIEnv* env, jobject, jstring dir) {
  return env->NewStringUTF(ScannerCore::instance().loadCloud(toString(env, dir)).c_str());
}

JNIEXPORT jboolean JNICALL JNI_FN(nativeIsRunning)(JNIEnv*, jobject) {
  return ScannerCore::instance().running() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL JNI_FN(nativeSetPaused)(JNIEnv*, jobject, jboolean paused) {
  ScannerCore::instance().setPaused(paused == JNI_TRUE);
}

JNIEXPORT void JNICALL JNI_FN(nativePushMotion)(JNIEnv*, jobject, jlong t_ns, jfloat px, jfloat py, jfloat pz,
                                                jfloat qx, jfloat qy, jfloat qz, jfloat qw, jboolean tracking) {
  ls::mock::MotionSample s;
  s.t_ns = t_ns;
  s.pose.t = {px, py, pz};
  s.pose.q = ls::normalize(ls::Quat{qw, qx, qy, qz});
  s.tracking = tracking == JNI_TRUE;
  ScannerCore::instance().pushMotion(s);
}

JNIEXPORT void JNICALL JNI_FN(nativeStationToggle)(JNIEnv*, jobject) { ScannerCore::instance().stationToggle(); }

JNIEXPORT jstring JNICALL JNI_FN(nativeStats)(JNIEnv* env, jobject) {
  return env->NewStringUTF(ScannerCore::instance().statsJson().c_str());
}

// Layout: int32 width, int32 height, float32 origin_x, float32 origin_z, float32 resolution, RGBA bytes.
JNIEXPORT jbyteArray JNICALL JNI_FN(nativeMinimap)(JNIEnv* env, jobject) {
  auto img = ScannerCore::instance().minimap();
  if (img.width == 0) return nullptr;
  const size_t header = 20;
  jbyteArray arr = env->NewByteArray(static_cast<jsize>(header + img.rgba.size()));
  uint8_t h[header];
  int32_t wh[2] = {img.width, img.height};
  float f[3] = {img.origin_x, img.origin_z, img.res};
  std::memcpy(h, wh, 8);
  std::memcpy(h + 8, f, 12);
  env->SetByteArrayRegion(arr, 0, header, reinterpret_cast<const jbyte*>(h));
  env->SetByteArrayRegion(arr, header, static_cast<jsize>(img.rgba.size()),
                          reinterpret_cast<const jbyte*>(img.rgba.data()));
  return arr;
}

JNIEXPORT jfloatArray JNICALL JNI_FN(nativeRevolution)(JNIEnv* env, jobject) {
  auto v = ScannerCore::instance().revolution();
  jfloatArray arr = env->NewFloatArray(static_cast<jsize>(v.size()));
  if (!v.empty()) env->SetFloatArrayRegion(arr, 0, static_cast<jsize>(v.size()), v.data());
  return arr;
}

JNIEXPORT jlong JNICALL JNI_FN(nativeLayoutVersion)(JNIEnv*, jobject) {
  return static_cast<jlong>(ScannerCore::instance().layoutVersion());
}

JNIEXPORT jstring JNICALL JNI_FN(nativeLayout)(JNIEnv* env, jobject) {
  return env->NewStringUTF(ScannerCore::instance().layoutJson().c_str());
}

JNIEXPORT jstring JNICALL JNI_FN(nativeComputeLayout)(JNIEnv* env, jobject) {
  return env->NewStringUTF(ScannerCore::instance().computeLayout().c_str());
}

JNIEXPORT void JNICALL JNI_FN(nativeSetSurface)(JNIEnv* env, jobject, jobject surface) {
  ANativeWindow* win = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
  ScannerCore::instance().renderer().setWindow(win);
}

JNIEXPORT void JNICALL JNI_FN(nativeSetView)(JNIEnv*, jobject, jint camera, jint color, jfloat point_size,
                                             jboolean dark, jboolean trajectory, jboolean grid, jfloat age_window,
                                             jint fps, jboolean clip_ceiling) {
  ls::ViewOptions o;
  o.camera = static_cast<ls::CameraMode>(camera);
  o.color = static_cast<ls::ColorMode>(color);
  o.point_size = point_size;
  o.dark = dark == JNI_TRUE;
  o.show_trajectory = trajectory == JNI_TRUE;
  o.show_grid = grid == JNI_TRUE;
  o.age_window_s = age_window;
  o.target_fps = fps;
  o.clip_ceiling = clip_ceiling == JNI_TRUE;
  ScannerCore::instance().renderer().setOptions(o);
}

JNIEXPORT void JNICALL JNI_FN(nativeOrbit)(JNIEnv*, jobject, jfloat dx, jfloat dy) {
  ScannerCore::instance().renderer().orbit(dx, dy);
}

JNIEXPORT void JNICALL JNI_FN(nativePan)(JNIEnv*, jobject, jfloat dx, jfloat dy) {
  ScannerCore::instance().renderer().pan(dx, dy);
}

JNIEXPORT void JNICALL JNI_FN(nativeZoom)(JNIEnv*, jobject, jfloat scale) {
  ScannerCore::instance().renderer().zoom(scale);
}

JNIEXPORT void JNICALL JNI_FN(nativeResetView)(JNIEnv*, jobject) { ScannerCore::instance().renderer().resetView(); }

}  // extern "C"
