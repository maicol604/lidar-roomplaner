#pragma once

#include <cstdint>
#include <string>

#include "math.h"

namespace ls {

struct MockParams {
  float noise_mm = 15.0f;
  bool glass_artifacts = true;
  bool intensity_by_material = true;
  bool drift = true;
  float drift_pct = 1.0f;            // translation scale error (% of distance)
  float drift_yaw_deg_per_m = 0.4f;  // heading random walk
  bool loop_closure = true;          // simulate ARCore relocalization corrections
  bool packet_loss = true;
  float packet_loss_pct = 1.0f;
  bool tracking_loss = true;
  float tracking_loss_per_min = 0.5f;
};

struct ScanConfig {
  enum class Source { Mock, Udp };
  enum class Transport { InProcess, Loopback };
  enum class Mode { Continuous, Station };

  Source source = Source::Mock;
  Transport transport = Transport::InProcess;
  Mode mode = Mode::Continuous;
  int udp_port = 4210;
  std::string esp_host = "192.168.4.1";
  int esp_port = 4211;

  std::string session_dir;  // empty: do not record
  std::string session_name;
  std::string device_model;
  std::string app_version;

  float voxel_m = 0.02f;
  int64_t point_budget = 2000000;
  float min_range_m = 0.05f;
  float max_range_m = 12.0f;
  int min_intensity = 0;

  // LiDAR mount relative to the device frame (cm + degrees applied on top of the default
  // "vertical transversal" mount: LD06 above the phone, rotation axis pointing forward).
  float ext_tx_cm = 0.0f, ext_ty_cm = 10.0f, ext_tz_cm = 0.0f;
  float ext_roll_deg = 0.0f, ext_pitch_deg = 0.0f, ext_yaw_deg = 0.0f;
  float lidar_latency_ms = 2.5f;

  float tilt_alert_deg = 15.0f;
  float gap_alert_cm = 5.0f;
  float gap_range_m = 3.0f;

  MockParams mock;
  // Mock rig driven by the phone's real motion (ARCore / IMU); false keeps it still (diagnostics).
  bool mock_device_motion = true;
  // Mock rig follows a scripted walk through the whole apartment (mockMotion=auto).
  bool mock_auto_tour = false;

  Pose lidarExtrinsic() const;
  static ScanConfig parse(const std::string& kv);
};

}  // namespace ls
