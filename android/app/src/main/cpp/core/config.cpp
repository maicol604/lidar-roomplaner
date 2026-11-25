#include "config.h"

#include <cstdlib>
#include <sstream>

namespace ls {

Pose ScanConfig::lidarExtrinsic() const {
  // Default mount: LD06 x (0 deg) -> device +Y (up), LD06 y -> device +X, LD06 z (spin axis) -> device -Z.
  const float r[3][3] = {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}};
  Quat base = fromMatrix(r);
  Quat adj = fromEuler(ext_roll_deg * kDeg2Rad, ext_pitch_deg * kDeg2Rad, ext_yaw_deg * kDeg2Rad);
  return {{ext_tx_cm / 100.0f, ext_ty_cm / 100.0f, ext_tz_cm / 100.0f}, normalize(base * adj)};
}

ScanConfig ScanConfig::parse(const std::string& kv) {
  ScanConfig c;
  std::istringstream in(kv);
  std::string line;
  while (std::getline(in, line)) {
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq), v = line.substr(eq + 1);
    float f = static_cast<float>(std::atof(v.c_str()));
    bool b = v == "true" || v == "1";
    if (k == "source") c.source = v == "udp" ? Source::Udp : Source::Mock;
    else if (k == "transport") c.transport = v == "loopback" ? Transport::Loopback : Transport::InProcess;
    else if (k == "mode") c.mode = v == "station" ? Mode::Station : Mode::Continuous;
    else if (k == "udpPort") c.udp_port = std::atoi(v.c_str());
    else if (k == "espHost") c.esp_host = v;
    else if (k == "espPort") c.esp_port = std::atoi(v.c_str());
    else if (k == "sessionDir") c.session_dir = v;
    else if (k == "sessionName") c.session_name = v;
    else if (k == "deviceModel") c.device_model = v;
    else if (k == "appVersion") c.app_version = v;
    else if (k == "voxelCm") c.voxel_m = f / 100.0f;
    else if (k == "pointBudget") c.point_budget = std::atoll(v.c_str());
    else if (k == "minRange") c.min_range_m = f;
    else if (k == "maxRange") c.max_range_m = f;
    else if (k == "minIntensity") c.min_intensity = std::atoi(v.c_str());
    else if (k == "extTx") c.ext_tx_cm = f;
    else if (k == "extTy") c.ext_ty_cm = f;
    else if (k == "extTz") c.ext_tz_cm = f;
    else if (k == "extRoll") c.ext_roll_deg = f;
    else if (k == "extPitch") c.ext_pitch_deg = f;
    else if (k == "extYaw") c.ext_yaw_deg = f;
    else if (k == "lidarLatencyMs") c.lidar_latency_ms = f;
    else if (k == "tiltAlertDeg") c.tilt_alert_deg = f;
    else if (k == "gapAlertCm") c.gap_alert_cm = f;
    else if (k == "gapRangeM") c.gap_range_m = f;
    else if (k == "mockNoiseMm") c.mock.noise_mm = f;
    else if (k == "mockGlass") c.mock.glass_artifacts = b;
    else if (k == "mockIntensity") c.mock.intensity_by_material = b;
    else if (k == "mockDrift") c.mock.drift = b;
    else if (k == "mockDriftPct") c.mock.drift_pct = f;
    else if (k == "mockDriftYaw") c.mock.drift_yaw_deg_per_m = f;
    else if (k == "mockLoopClosure") c.mock.loop_closure = b;
    else if (k == "mockPacketLoss") c.mock.packet_loss = b;
    else if (k == "mockPacketLossPct") c.mock.packet_loss_pct = f;
    else if (k == "mockTrackingLoss") c.mock.tracking_loss = b;
    else if (k == "mockTrackingLossPerMin") c.mock.tracking_loss_per_min = f;
    else if (k == "mockMotion") {
      c.mock_device_motion = v == "device";
      c.mock_auto_tour = v == "auto";
    }
  }
  if (c.voxel_m < 0.005f) c.voxel_m = 0.005f;
  if (c.point_budget < 100000) c.point_budget = 100000;
  return c;
}

}  // namespace ls
