import 'dart:convert';

import 'package:flutter/material.dart';

import 'models.dart';

/// User settings, persisted as JSON in SharedPreferences.
@immutable
class AppSettings {
  const AppSettings({
    this.themeMode = ThemeMode.system,
    this.localeCode,
    this.camera = CameraMode.follow,
    this.color = ColorMode.height,
    this.pointSize = 1.0,
    this.showTrajectory = true,
    this.showGrid = true,
    this.ageWindowS = 60,
    this.targetFps = 0,
    this.hudVisible = true,
    this.hudItems = const {...HudItem.values},
    this.voxelCm = 2.0,
    this.pointBudget = 0,
    this.minRangeM = 0.05,
    this.maxRangeM = 12.0,
    this.minIntensity = 0,
    this.tiltAlertDeg = 15,
    this.gapAlertCm = 5,
    this.espHost = '192.168.4.1',
    this.espPort = 4211,
    this.phonePort = 4210,
    this.extTx = 0,
    this.extTy = 10,
    this.extTz = 0,
    this.extRoll = 0,
    this.extPitch = 0,
    this.extYaw = 0,
    this.lidarLatencyMs = 2.5,
    this.mockNoiseMm = 15,
    this.mockGlass = true,
    this.mockIntensity = true,
    this.mockDrift = true,
    this.mockDriftPct = 1.0,
    this.mockDriftYaw = 0.4,
    this.mockLoopClosure = true,
    this.mockPacketLoss = true,
    this.mockPacketLossPct = 1.0,
    this.mockTrackingLoss = true,
    this.mockTrackingLossPerMin = 0.5,
  });

  final ThemeMode themeMode;
  final String? localeCode;
  final CameraMode camera;
  final ColorMode color;
  final double pointSize;
  final bool showTrajectory;
  final bool showGrid;
  final double ageWindowS;

  /// 0 = automatic (60 FPS, 30 on low-end devices).
  final int targetFps;
  final bool hudVisible;
  final Set<HudItem> hudItems;

  final double voxelCm;

  /// 0 = automatic (depends on device RAM).
  final int pointBudget;
  final double minRangeM;
  final double maxRangeM;
  final int minIntensity;

  final double tiltAlertDeg;
  final double gapAlertCm;

  final String espHost;
  final int espPort;
  final int phonePort;

  final double extTx, extTy, extTz, extRoll, extPitch, extYaw;
  final double lidarLatencyMs;

  final double mockNoiseMm;
  final bool mockGlass;
  final bool mockIntensity;
  final bool mockDrift;
  final double mockDriftPct;
  final double mockDriftYaw;
  final bool mockLoopClosure;
  final bool mockPacketLoss;
  final double mockPacketLossPct;
  final bool mockTrackingLoss;
  final double mockTrackingLossPerMin;

  Locale? get locale => localeCode == null ? null : Locale(localeCode!);

  int effectiveFps(DeviceInfo? info) => targetFps > 0 ? targetFps : ((info?.lowEnd ?? false) ? 30 : 60);
  int effectiveBudget(DeviceInfo? info) => pointBudget > 0 ? pointBudget : (info?.recommendedBudget ?? 1000000);

  AppSettings copyWith({
    ThemeMode? themeMode,
    String? Function()? localeCode,
    CameraMode? camera,
    ColorMode? color,
    double? pointSize,
    bool? showTrajectory,
    bool? showGrid,
    double? ageWindowS,
    int? targetFps,
    bool? hudVisible,
    Set<HudItem>? hudItems,
    double? voxelCm,
    int? pointBudget,
    double? minRangeM,
    double? maxRangeM,
    int? minIntensity,
    double? tiltAlertDeg,
    double? gapAlertCm,
    String? espHost,
    int? espPort,
    int? phonePort,
    double? extTx,
    double? extTy,
    double? extTz,
    double? extRoll,
    double? extPitch,
    double? extYaw,
    double? lidarLatencyMs,
    double? mockNoiseMm,
    bool? mockGlass,
    bool? mockIntensity,
    bool? mockDrift,
    double? mockDriftPct,
    double? mockDriftYaw,
    bool? mockLoopClosure,
    bool? mockPacketLoss,
    double? mockPacketLossPct,
    bool? mockTrackingLoss,
    double? mockTrackingLossPerMin,
  }) {
    return AppSettings(
      themeMode: themeMode ?? this.themeMode,
      localeCode: localeCode != null ? localeCode() : this.localeCode,
      camera: camera ?? this.camera,
      color: color ?? this.color,
      pointSize: pointSize ?? this.pointSize,
      showTrajectory: showTrajectory ?? this.showTrajectory,
      showGrid: showGrid ?? this.showGrid,
      ageWindowS: ageWindowS ?? this.ageWindowS,
      targetFps: targetFps ?? this.targetFps,
      hudVisible: hudVisible ?? this.hudVisible,
      hudItems: hudItems ?? this.hudItems,
      voxelCm: voxelCm ?? this.voxelCm,
      pointBudget: pointBudget ?? this.pointBudget,
      minRangeM: minRangeM ?? this.minRangeM,
      maxRangeM: maxRangeM ?? this.maxRangeM,
      minIntensity: minIntensity ?? this.minIntensity,
      tiltAlertDeg: tiltAlertDeg ?? this.tiltAlertDeg,
      gapAlertCm: gapAlertCm ?? this.gapAlertCm,
      espHost: espHost ?? this.espHost,
      espPort: espPort ?? this.espPort,
      phonePort: phonePort ?? this.phonePort,
      extTx: extTx ?? this.extTx,
      extTy: extTy ?? this.extTy,
      extTz: extTz ?? this.extTz,
      extRoll: extRoll ?? this.extRoll,
      extPitch: extPitch ?? this.extPitch,
      extYaw: extYaw ?? this.extYaw,
      lidarLatencyMs: lidarLatencyMs ?? this.lidarLatencyMs,
      mockNoiseMm: mockNoiseMm ?? this.mockNoiseMm,
      mockGlass: mockGlass ?? this.mockGlass,
      mockIntensity: mockIntensity ?? this.mockIntensity,
      mockDrift: mockDrift ?? this.mockDrift,
      mockDriftPct: mockDriftPct ?? this.mockDriftPct,
      mockDriftYaw: mockDriftYaw ?? this.mockDriftYaw,
      mockLoopClosure: mockLoopClosure ?? this.mockLoopClosure,
      mockPacketLoss: mockPacketLoss ?? this.mockPacketLoss,
      mockPacketLossPct: mockPacketLossPct ?? this.mockPacketLossPct,
      mockTrackingLoss: mockTrackingLoss ?? this.mockTrackingLoss,
      mockTrackingLossPerMin: mockTrackingLossPerMin ?? this.mockTrackingLossPerMin,
    );
  }

  Map<String, Object?> toJson() => {
        'themeMode': themeMode.name,
        'localeCode': localeCode,
        'camera': camera.name,
        'color': color.name,
        'pointSize': pointSize,
        'showTrajectory': showTrajectory,
        'showGrid': showGrid,
        'ageWindowS': ageWindowS,
        'targetFps': targetFps,
        'hudVisible': hudVisible,
        'hudItems': hudItems.map((e) => e.name).toList(),
        'voxelCm': voxelCm,
        'pointBudget': pointBudget,
        'minRangeM': minRangeM,
        'maxRangeM': maxRangeM,
        'minIntensity': minIntensity,
        'tiltAlertDeg': tiltAlertDeg,
        'gapAlertCm': gapAlertCm,
        'espHost': espHost,
        'espPort': espPort,
        'phonePort': phonePort,
        'extTx': extTx,
        'extTy': extTy,
        'extTz': extTz,
        'extRoll': extRoll,
        'extPitch': extPitch,
        'extYaw': extYaw,
        'lidarLatencyMs': lidarLatencyMs,
        'mockNoiseMm': mockNoiseMm,
        'mockGlass': mockGlass,
        'mockIntensity': mockIntensity,
        'mockDrift': mockDrift,
        'mockDriftPct': mockDriftPct,
        'mockDriftYaw': mockDriftYaw,
        'mockLoopClosure': mockLoopClosure,
        'mockPacketLoss': mockPacketLoss,
        'mockPacketLossPct': mockPacketLossPct,
        'mockTrackingLoss': mockTrackingLoss,
        'mockTrackingLossPerMin': mockTrackingLossPerMin,
      };

  factory AppSettings.fromJson(Map<String, dynamic> j) {
    const d = AppSettings();
    T e<T extends Enum>(List<T> values, Object? name, T fallback) =>
        values.firstWhere((v) => v.name == name, orElse: () => fallback);
    double n(String k, double f) => (j[k] as num?)?.toDouble() ?? f;
    int i(String k, int f) => (j[k] as num?)?.toInt() ?? f;
    bool b(String k, bool f) => j[k] as bool? ?? f;
    final hud = (j['hudItems'] as List?)?.map((x) => e(HudItem.values, x, HudItem.points)).toSet();
    return AppSettings(
      themeMode: e(ThemeMode.values, j['themeMode'], d.themeMode),
      localeCode: j['localeCode'] as String?,
      camera: e(CameraMode.values, j['camera'], d.camera),
      color: e(ColorMode.values, j['color'], d.color),
      pointSize: n('pointSize', d.pointSize),
      showTrajectory: b('showTrajectory', d.showTrajectory),
      showGrid: b('showGrid', d.showGrid),
      ageWindowS: n('ageWindowS', d.ageWindowS),
      targetFps: i('targetFps', d.targetFps),
      hudVisible: b('hudVisible', d.hudVisible),
      hudItems: hud ?? d.hudItems,
      voxelCm: n('voxelCm', d.voxelCm),
      pointBudget: i('pointBudget', d.pointBudget),
      minRangeM: n('minRangeM', d.minRangeM),
      maxRangeM: n('maxRangeM', d.maxRangeM),
      minIntensity: i('minIntensity', d.minIntensity),
      tiltAlertDeg: n('tiltAlertDeg', d.tiltAlertDeg),
      gapAlertCm: n('gapAlertCm', d.gapAlertCm),
      espHost: j['espHost'] as String? ?? d.espHost,
      espPort: i('espPort', d.espPort),
      phonePort: i('phonePort', d.phonePort),
      extTx: n('extTx', d.extTx),
      extTy: n('extTy', d.extTy),
      extTz: n('extTz', d.extTz),
      extRoll: n('extRoll', d.extRoll),
      extPitch: n('extPitch', d.extPitch),
      extYaw: n('extYaw', d.extYaw),
      lidarLatencyMs: n('lidarLatencyMs', d.lidarLatencyMs),
      mockNoiseMm: n('mockNoiseMm', d.mockNoiseMm),
      mockGlass: b('mockGlass', d.mockGlass),
      mockIntensity: b('mockIntensity', d.mockIntensity),
      mockDrift: b('mockDrift', d.mockDrift),
      mockDriftPct: n('mockDriftPct', d.mockDriftPct),
      mockDriftYaw: n('mockDriftYaw', d.mockDriftYaw),
      mockLoopClosure: b('mockLoopClosure', d.mockLoopClosure),
      mockPacketLoss: b('mockPacketLoss', d.mockPacketLoss),
      mockPacketLossPct: n('mockPacketLossPct', d.mockPacketLossPct),
      mockTrackingLoss: b('mockTrackingLoss', d.mockTrackingLoss),
      mockTrackingLossPerMin: n('mockTrackingLossPerMin', d.mockTrackingLossPerMin),
    );
  }

  String encode() => jsonEncode(toJson());

  static AppSettings decode(String? s) {
    if (s == null || s.isEmpty) return const AppSettings();
    try {
      return AppSettings.fromJson(jsonDecode(s) as Map<String, dynamic>);
    } catch (_) {
      return const AppSettings();
    }
  }
}
