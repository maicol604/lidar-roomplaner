import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:lidarscanner/core/models.dart';
import 'package:lidarscanner/core/session_config.dart';
import 'package:lidarscanner/core/settings.dart';

void main() {
  Map<String, String> parse(String kv) =>
      {for (final l in kv.split('\n')) l.substring(0, l.indexOf('=')): l.substring(l.indexOf('=') + 1)};

  const lowEnd = DeviceInfo(model: 'x', sdkInt: 30, totalRamMb: 3000, lowEnd: true, recommendedBudget: 1000000);

  test('core config uses automatic budget and maps enums', () {
    const req = ScanRequest(
      name: 'Salón\nplanta baja',
      source: ScanSource.mock,
      transport: MockTransport.loopback,
      mode: ScanMode.station,
      sessionDir: '/data/s/1',
    );
    final kv = parse(buildCoreConfig(req, const AppSettings(), lowEnd));
    expect(kv['source'], 'mock');
    expect(kv['transport'], 'loopback');
    expect(kv['mode'], 'station');
    expect(kv['pointBudget'], '1000000');
    expect(kv['sessionName'], 'Salón planta baja');
    expect(kv['sessionDir'], '/data/s/1');
    expect(kv['voxelCm'], '2.0');
    expect(kv['mockMotion'], 'device');
    const diag = ScanRequest(name: 'd', source: ScanSource.mock, mode: ScanMode.continuous, deviceMotion: false);
    expect(parse(buildCoreConfig(diag, const AppSettings(), lowEnd))['mockMotion'], 'static');
  });

  test('explicit budget and fps override device defaults', () {
    const s = AppSettings(pointBudget: 5000000, targetFps: 60);
    expect(s.effectiveBudget(lowEnd), 5000000);
    expect(s.effectiveFps(lowEnd), 60);
    expect(const AppSettings().effectiveFps(lowEnd), 30);
  });

  test('settings survive an encode/decode round trip', () {
    const s = AppSettings(
      themeMode: ThemeMode.dark,
      localeCode: 'en',
      camera: CameraMode.top,
      hudItems: {HudItem.points, HudItem.battery},
      extYaw: -3.5,
      mockDrift: false,
    );
    final d = AppSettings.decode(s.encode());
    expect(d.themeMode, ThemeMode.dark);
    expect(d.localeCode, 'en');
    expect(d.camera, CameraMode.top);
    expect(d.hudItems, {HudItem.points, HudItem.battery});
    expect(d.extYaw, -3.5);
    expect(d.mockDrift, isFalse);
    expect(AppSettings.decode('{not json').voxelCm, 2.0);
  });

  test('minimap header is parsed and truncated payloads are rejected', () {
    final bytes = Uint8List(20 + 2 * 3 * 4);
    final bd = ByteData.sublistView(bytes);
    bd.setInt32(0, 2, Endian.little);
    bd.setInt32(4, 3, Endian.little);
    bd.setFloat32(8, -1.5, Endian.little);
    bd.setFloat32(12, 4.0, Endian.little);
    bd.setFloat32(16, 0.1, Endian.little);
    final m = MinimapData.fromBytes(bytes)!;
    expect(m.width, 2);
    expect(m.height, 3);
    expect(m.originX, -1.5);
    expect(m.resolution, closeTo(0.1, 1e-6));
    expect(MinimapData.fromBytes(Uint8List.sublistView(bytes, 0, bytes.length - 1)), isNull);
  });

  test('stats event merges native JSON with battery and thermal info', () {
    final s = ScanStats.fromEvent({
      'core': '{"running":true,"mode":"station","tracking":3,"alertTilt":true,"tiltDeg":21.5,"totalPoints":1234}',
      'batteryLevel': 80,
      'batteryTempC': 37.5,
      'charging': false,
      'thermalStatus': 3,
    });
    expect(s.running, isTrue);
    expect(s.mode, ScanMode.station);
    expect(s.tracking, TrackingState.lost);
    expect(s.alertTilt, isTrue);
    expect(s.totalPoints, 1234);
    expect(s.alertThermal, isTrue);
  });

  test('simulator motion state and placement are parsed', () {
    final s = ScanStats.fromEvent({
      'core': '{"running":true,"source":"mock","motionReady":false,"placement":2}',
      'motionSource': 'arcore',
      'motionFailure': 'insufficient_light',
    });
    expect(s.motionReady, isFalse);
    expect(s.placement, MockPlacement.outside);
    expect(s.motionSource, MotionSource.arcore);
    expect(s.motionFailure, 'insufficient_light');
    expect(ScanStats.empty.motionReady, isTrue);
    expect(MotionStart.fromMap({'source': 'imu', 'fallbackReason': 'camera_denied'}).source, MotionSource.imu);
    expect(MotionStart.fromMap(null).source, MotionSource.none);
  });
}
