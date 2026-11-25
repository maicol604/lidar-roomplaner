import 'models.dart';
import 'settings.dart';

/// Parameters chosen on the "new scan" screen.
class ScanRequest {
  const ScanRequest({
    required this.name,
    required this.source,
    required this.mode,
    this.transport = MockTransport.inProcess,
    this.sessionDir,
    this.deviceMotion = true,
    this.autoTour = false,
  });

  final String name;
  final ScanSource source;
  final MockTransport transport;
  final ScanMode mode;

  /// Directory where the session is recorded; null runs without recording (diagnostics).
  final String? sessionDir;

  /// Simulator rig follows the phone's real motion; false keeps it still (diagnostics).
  final bool deviceMotion;

  /// Simulator rig walks a scripted tour through the whole virtual apartment (demo / testing
  /// without a large free space). Takes precedence over [deviceMotion].
  final bool autoTour;
}

/// Builds the `key=value` configuration consumed by the native core (ScanConfig::parse).
String buildCoreConfig(ScanRequest r, AppSettings s, DeviceInfo? device, {String appVersion = ''}) {
  String f(double v) => v.toString();
  final kv = <String, String>{
    'source': r.source == ScanSource.udp ? 'udp' : 'mock',
    'transport': r.transport == MockTransport.loopback ? 'loopback' : 'inprocess',
    'mode': r.mode == ScanMode.station ? 'station' : 'continuous',
    'udpPort': '${s.phonePort}',
    'espHost': s.espHost,
    'espPort': '${s.espPort}',
    'sessionDir': r.sessionDir ?? '',
    'sessionName': r.name,
    'deviceModel': device?.model ?? '',
    'appVersion': appVersion,
    'voxelCm': f(s.voxelCm),
    'pointBudget': '${s.effectiveBudget(device)}',
    'minRange': f(s.minRangeM),
    'maxRange': f(s.maxRangeM),
    'minIntensity': '${s.minIntensity}',
    'extTx': f(s.extTx),
    'extTy': f(s.extTy),
    'extTz': f(s.extTz),
    'extRoll': f(s.extRoll),
    'extPitch': f(s.extPitch),
    'extYaw': f(s.extYaw),
    'lidarLatencyMs': f(s.lidarLatencyMs),
    'tiltAlertDeg': f(s.tiltAlertDeg),
    'gapAlertCm': f(s.gapAlertCm),
    'mockNoiseMm': f(s.mockNoiseMm),
    'mockGlass': '${s.mockGlass}',
    'mockIntensity': '${s.mockIntensity}',
    'mockDrift': '${s.mockDrift}',
    'mockDriftPct': f(s.mockDriftPct),
    'mockDriftYaw': f(s.mockDriftYaw),
    'mockLoopClosure': '${s.mockLoopClosure}',
    'mockPacketLoss': '${s.mockPacketLoss}',
    'mockPacketLossPct': f(s.mockPacketLossPct),
    'mockTrackingLoss': '${s.mockTrackingLoss}',
    'mockTrackingLossPerMin': f(s.mockTrackingLossPerMin),
    'mockMotion': r.autoTour ? 'auto' : (r.deviceMotion ? 'device' : 'static'),
  };
  // Values are single-line by construction; strip line breaks from free text just in case.
  return kv.entries.map((e) => '${e.key}=${e.value.replaceAll(RegExp(r'[\r\n]'), ' ')}').join('\n');
}
