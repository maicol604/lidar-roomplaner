import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/services.dart';

import 'layout.dart';
import 'models.dart';

/// Dart side of the native scanner core (see android/.../ScannerPlugin.kt).
class ScannerBridge {
  static const _core = MethodChannel('lidarscanner/core');
  static const _statsChannel = EventChannel('lidarscanner/stats');
  static const _minimapChannel = EventChannel('lidarscanner/minimap');
  static const _revolutionChannel = EventChannel('lidarscanner/revolution');
  static const _layoutChannel = EventChannel('lidarscanner/layout');

  Stream<ScanStats>? _stats;
  Stream<MinimapData>? _minimap;
  Stream<Revolution>? _revolution;
  Stream<RoomLayout>? _layout;

  Future<DeviceInfo> deviceInfo() async {
    final m = await _core.invokeMapMethod<Object?, Object?>('deviceInfo');
    return DeviceInfo.fromMap(m ?? const {});
  }

  Future<bool> start(String config) async => await _core.invokeMethod<bool>('start', {'config': config}) ?? false;

  /// Stops the session; returns the native session summary (session.json content).
  Future<String> stop() async => await _core.invokeMethod<String>('stop') ?? '{}';

  Future<bool> isRunning() async => await _core.invokeMethod<bool>('isRunning') ?? false;

  /// Loads a saved session's cloud.ply (+ trajectory) into the renderer. Returns the point
  /// count, or throws a [StateError] with the native error message.
  Future<int> loadCloud(String sessionDir) async {
    final res = jsonDecode(await _core.invokeMethod<String>('loadCloud', {'dir': sessionDir}) ?? '{}')
        as Map<String, dynamic>;
    if (res['ok'] != true) throw StateError(res['error'] as String? ?? 'load failed');
    return (res['points'] as num?)?.toInt() ?? 0;
  }

  Future<void> setPaused(bool paused) => _core.invokeMethod('setPaused', {'paused': paused});

  /// Computes the structured model of the loaded cloud (saved sessions). Returns the native JSON
  /// (parse with [RoomLayout.tryParse]).
  Future<String> computeLayout() async => await _core.invokeMethod<String>('computeLayout') ?? '';

  /// Starts tracking the phone's real motion for the simulator (ARCore, IMU as fallback).
  /// May ask for the camera permission.
  Future<MotionStart> startMotion() async =>
      MotionStart.fromMap(await _core.invokeMapMethod<Object?, Object?>('startMotion'));

  Future<void> stopMotion() => _core.invokeMethod('stopMotion');

  /// Camera thumbnail texture (ARCore only); null when unavailable.
  Future<int?> createCameraPreview(int width, int height) =>
      _core.invokeMethod<int>('createCameraPreview', {'width': width, 'height': height});

  Future<void> disposeCameraPreview() => _core.invokeMethod('disposeCameraPreview');

  Future<void> stationToggle() => _core.invokeMethod('stationToggle');

  Future<int> createViewport(int width, int height) async =>
      await _core.invokeMethod<int>('createViewport', {'width': width, 'height': height}) ?? -1;

  Future<void> resizeViewport(int width, int height) =>
      _core.invokeMethod('resizeViewport', {'width': width, 'height': height});

  Future<void> disposeViewport() => _core.invokeMethod('disposeViewport');

  Future<void> setView({
    required CameraMode camera,
    required ColorMode color,
    required double pointSize,
    required bool dark,
    required bool trajectory,
    required bool grid,
    required double ageWindowS,
    required int fps,
    bool clipCeiling = false,
  }) =>
      _core.invokeMethod('setView', {
        'camera': camera.index,
        'color': color.index,
        'pointSize': pointSize,
        'dark': dark,
        'trajectory': trajectory,
        'grid': grid,
        'ageWindow': ageWindowS,
        'fps': fps,
        'clipCeiling': clipCeiling,
      });

  Future<void> orbit(double dx, double dy) => _core.invokeMethod('orbit', {'dx': dx, 'dy': dy});
  Future<void> pan(double dx, double dy) => _core.invokeMethod('pan', {'dx': dx, 'dy': dy});
  Future<void> zoom(double scale) => _core.invokeMethod('zoom', {'scale': scale});
  Future<void> resetView() => _core.invokeMethod('resetView');

  Stream<ScanStats> stats() =>
      _stats ??= _statsChannel.receiveBroadcastStream().map(ScanStats.fromEvent).asBroadcastStream();

  Stream<MinimapData> minimap() => _minimap ??= _minimapChannel
      .receiveBroadcastStream()
      .map((e) => e is Uint8List ? MinimapData.fromBytes(e) : null)
      .where((m) => m != null)
      .cast<MinimapData>()
      .asBroadcastStream();

  Stream<Revolution> revolution() => _revolution ??= _revolutionChannel
      .receiveBroadcastStream()
      .map((e) => Revolution(e is Float32List ? e : Float32List(0)))
      .asBroadcastStream();

  /// Live structured model while scanning (about once per second, only when it changed).
  Stream<RoomLayout> layout() => _layout ??= _layoutChannel
      .receiveBroadcastStream()
      .map((e) => RoomLayout.tryParse(e as String?))
      .where((l) => l != null)
      .cast<RoomLayout>()
      .asBroadcastStream();
}
