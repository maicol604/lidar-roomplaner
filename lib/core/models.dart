import 'dart:convert';
import 'dart:typed_data';

enum ScanSource { mock, udp }

enum MockTransport { inProcess, loopback }

enum ScanMode { continuous, station }

enum CameraMode { orbit, follow, firstPerson, top }

enum ColorMode { height, intensity, age }

enum TrackingState { initializing, tracking, limited, lost }

/// What moves the simulated rig: the phone's real motion via ARCore or via its IMU.
enum MotionSource { arcore, imu, none }

/// Position of the simulated rig with respect to the virtual apartment.
enum MockPlacement { free, insideObstacle, outside }

/// Result of starting the phone motion tracker for the simulator.
class MotionStart {
  const MotionStart(this.source, this.fallbackReason);

  factory MotionStart.fromMap(Map<Object?, Object?>? m) => MotionStart(
        switch (m?['source']) {
          'arcore' => MotionSource.arcore,
          'imu' => MotionSource.imu,
          _ => MotionSource.none,
        },
        m?['fallbackReason'] as String?,
      );

  final MotionSource source;

  /// Why ARCore is not used: camera_denied, arcore_install, arcore_unsupported or arcore_error.
  final String? fallbackReason;
}

/// HUD items the user can show or hide (quality alerts are always visible).
enum HudItem { points, rate, tracking, link, rpm, duration, distance, battery }

class DeviceInfo {
  const DeviceInfo({
    required this.model,
    required this.sdkInt,
    required this.totalRamMb,
    required this.lowEnd,
    required this.recommendedBudget,
  });

  factory DeviceInfo.fromMap(Map<Object?, Object?> m) => DeviceInfo(
        model: m['model'] as String? ?? '',
        sdkInt: (m['sdkInt'] as num?)?.toInt() ?? 0,
        totalRamMb: (m['totalRamMb'] as num?)?.toInt() ?? 0,
        lowEnd: m['lowEnd'] as bool? ?? false,
        recommendedBudget: (m['recommendedBudget'] as num?)?.toInt() ?? 1000000,
      );

  final String model;
  final int sdkInt;
  final int totalRamMb;
  final bool lowEnd;
  final int recommendedBudget;
}

/// Snapshot of the native core state merged with battery / thermal info.
class ScanStats {
  const ScanStats(this._core,
      {this.batteryLevel = -1,
      this.batteryTempC = 0,
      this.charging = false,
      this.thermalStatus = -1,
      this.motionSource = MotionSource.none,
      this.motionFailure});

  factory ScanStats.fromEvent(Object? event) {
    final m = (event as Map<Object?, Object?>?) ?? const {};
    final core = jsonDecode(m['core'] as String? ?? '{}') as Map<String, dynamic>;
    return ScanStats(
      core,
      batteryLevel: (m['batteryLevel'] as num?)?.toInt() ?? -1,
      batteryTempC: (m['batteryTempC'] as num?)?.toDouble() ?? 0,
      charging: m['charging'] as bool? ?? false,
      thermalStatus: (m['thermalStatus'] as num?)?.toInt() ?? -1,
      motionSource: MotionStart.fromMap({'source': m['motionSource']}).source,
      motionFailure: m['motionFailure'] as String?,
    );
  }

  static const empty = ScanStats({});

  final Map<String, dynamic> _core;
  final int batteryLevel;
  final double batteryTempC;
  final bool charging;
  final int thermalStatus;
  final MotionSource motionSource;

  /// ARCore tracking failure reason (insufficient_light, excessive_motion, ...), if any.
  final String? motionFailure;

  double _d(String k) => (_core[k] as num?)?.toDouble() ?? 0;
  int _i(String k) => (_core[k] as num?)?.toInt() ?? 0;
  bool _b(String k) => _core[k] as bool? ?? false;

  bool get running => _b('running');
  bool get paused => _b('paused');
  ScanSource get source => _core['source'] == 'udp' ? ScanSource.udp : ScanSource.mock;
  ScanMode get mode => _core['mode'] == 'station' ? ScanMode.station : ScanMode.continuous;
  double get durationS => _d('durationS');
  int get totalPoints => _i('totalPoints');
  double get pointsPerSec => _d('pointsPerSec');
  double get rawPointsPerSec => _d('rawPointsPerSec');
  double get datagramsPerSec => _d('datagramsPerSec');
  int get lostDatagrams => _i('lostDatagrams');
  double get lossPct => _d('lossPct');
  int get crcErrors => _i('crcErrors');
  double get latencyMs => _d('latencyMs');
  double get rttMs => _d('rttMs');
  double get driftPpm => _d('driftPpm');
  bool get synced => _b('synced');
  double get rpm => _d('rpm');
  double get distanceM => _d('distanceM');
  TrackingState get tracking => TrackingState.values[_i('tracking').clamp(0, 3)];
  int get trackingLosses => _i('trackingLosses');
  double get gapCm => _d('gapCm');
  double get tiltDeg => _d('tiltDeg');
  bool get alertSpeed => _b('alertSpeed');
  bool get alertTilt => _b('alertTilt');
  bool get alertTracking => _b('alertTracking');
  bool get alertNoData => _b('alertNoData');
  bool get alertStationDisplaced => _b('alertStationDisplaced');
  bool get alertThermal => thermalStatus >= 3; // THERMAL_STATUS_SEVERE
  bool get hasDevice => _b('hasDevice');
  double get deviceX => _d('deviceX');
  double get deviceZ => _d('deviceZ');
  double get fwdX => _d('fwdX');
  double get fwdZ => _d('fwdZ');
  int get submaps => _i('submaps');
  int get corrections => _i('corrections');
  int get loopClosures => _i('loopClosures');
  double get voxelCm => _d('voxelCm');
  bool get stationMoving => _b('stationMoving');
  int get stationCount => _i('stationCount');
  int get recordBytes => _i('recordBytes');
  double get renderFps => _d('renderFps');
  String get peer => _core['peer'] as String? ?? '';

  /// Simulator: the rig is aligned with the phone's real motion (tracker delivered a pose).
  bool get motionReady => _core['motionReady'] as bool? ?? true;
  MockPlacement get placement => MockPlacement.values[_i('placement').clamp(0, 2)];
}

/// Plan-view coverage image produced by the native coverage grid.
class MinimapData {
  const MinimapData({
    required this.width,
    required this.height,
    required this.originX,
    required this.originZ,
    required this.resolution,
    required this.rgba,
  });

  static MinimapData? fromBytes(Uint8List bytes) {
    if (bytes.length < 20) return null;
    final bd = ByteData.sublistView(bytes);
    final w = bd.getInt32(0, Endian.little);
    final h = bd.getInt32(4, Endian.little);
    if (w <= 0 || h <= 0 || bytes.length < 20 + w * h * 4) return null;
    return MinimapData(
      width: w,
      height: h,
      originX: bd.getFloat32(8, Endian.little),
      originZ: bd.getFloat32(12, Endian.little),
      resolution: bd.getFloat32(16, Endian.little),
      rgba: Uint8List.sublistView(bytes, 20, 20 + w * h * 4),
    );
  }

  final int width;
  final int height;
  final double originX;
  final double originZ;
  final double resolution;
  final Uint8List rgba;
}

/// One LD06 revolution in polar form.
class Revolution {
  const Revolution(this.data);
  final Float32List data;
  int get length => data.length ~/ 3;
  double angleDeg(int i) => data[i * 3];
  double distanceM(int i) => data[i * 3 + 1];
  double intensity(int i) => data[i * 3 + 2];
}
