import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/format.dart';
import '../../core/layout.dart';
import '../../core/models.dart';
import '../../core/providers.dart';
import '../../core/scanner_bridge.dart';
import '../../core/session_config.dart';
import '../../core/settings.dart';
import '../../l10n/app_localizations.dart';
import '../layout/model_view.dart';
import '../viewer/cloud_viewer_screen.dart';
import 'widgets/alerts.dart';
import 'widgets/hud.dart';
import 'widgets/minimap.dart';
import 'widgets/viewport.dart';

class ScannerScreen extends ConsumerStatefulWidget {
  const ScannerScreen({super.key, required this.request});
  final ScanRequest request;

  @override
  ConsumerState<ScannerScreen> createState() => _ScannerScreenState();
}

class _ScannerScreenState extends ConsumerState<ScannerScreen> {
  static const _previewW = 90.0, _previewH = 120.0;

  bool _started = false;
  bool _stopping = false;
  late bool _hudVisible = ref.read(settingsProvider).hudVisible;
  bool _bigMinimap = false;
  bool _bigModel = false;
  late final ScannerBridge _bridge = ref.read(bridgeProvider);
  bool _motionRequested = false;
  MotionStart? _motion;
  int? _previewTexture;

  bool get _isMock => widget.request.source == ScanSource.mock;
  bool get _usesMotion => _isMock && widget.request.deviceMotion && !widget.request.autoTour;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _start());
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    _applyView();
  }

  @override
  void dispose() {
    if (_motionRequested) _bridge.stopMotion();
    super.dispose();
  }

  Future<void> _start() async {
    if (_usesMotion) {
      _motionRequested = true;
      final motion = await _bridge.startMotion();
      if (!mounted) return;
      setState(() => _motion = motion);
      _reportMotionFallback(motion);
    }
    final device = await ref.read(deviceInfoProvider.future).catchError((_) => const DeviceInfo(
        model: '', sdkInt: 0, totalRamMb: 0, lowEnd: true, recommendedBudget: 1000000));
    final config = buildCoreConfig(widget.request, ref.read(settingsProvider), device, appVersion: '1.0.0');
    final ok = await _bridge.start(config);
    if (!mounted) return;
    if (!ok) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(AppLocalizations.of(context).startFailed)));
      final dir = widget.request.sessionDir;
      if (dir != null) Directory(dir).delete(recursive: true).ignore();
      Navigator.of(context).pop();
      return;
    }
    setState(() => _started = true);
    _applyView();
    if (_motion?.source == MotionSource.arcore) {
      final ratio = MediaQuery.devicePixelRatioOf(context);
      final texture = await _bridge.createCameraPreview((_previewW * ratio).round(), (_previewH * ratio).round());
      if (mounted) setState(() => _previewTexture = texture);
    }
  }

  void _reportMotionFallback(MotionStart motion) {
    final l = AppLocalizations.of(context);
    final message = motion.source == MotionSource.none
        ? l.motionNone
        : switch (motion.fallbackReason) {
            'camera_denied' => l.motionFallbackCamera,
            'arcore_install' => l.motionFallbackInstall,
            'arcore_unsupported' => l.motionFallbackUnsupported,
            'arcore_error' => l.motionFallbackError,
            _ => null,
          };
    if (message != null) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message), duration: const Duration(seconds: 6)));
    }
  }

  void _applyView() {
    final s = ref.read(settingsProvider);
    final device = ref.read(deviceInfoProvider).value;
    ref.read(bridgeProvider).setView(
          camera: s.camera,
          color: s.color,
          pointSize: s.pointSize,
          dark: Theme.of(context).brightness == Brightness.dark,
          trajectory: s.showTrajectory,
          grid: s.showGrid,
          ageWindowS: s.ageWindowS,
          fps: s.effectiveFps(device),
        );
  }

  Future<void> _confirmStop() async {
    if (_stopping) return;
    if (!_started) {
      Navigator.of(context).pop();
      return;
    }
    final l = AppLocalizations.of(context);
    final ok = await showDialog<bool>(
      context: context,
      builder: (c) => AlertDialog(
        title: Text(l.stopConfirmTitle),
        content: Text(l.stopConfirmBody),
        actions: [
          TextButton(onPressed: () => Navigator.pop(c, false), child: Text(l.cancel)),
          FilledButton(onPressed: () => Navigator.pop(c, true), child: Text(l.finish)),
        ],
      ),
    );
    if (ok == true) await _stop();
  }

  Future<void> _stop() async {
    setState(() => _stopping = true);
    await ref.read(bridgeProvider).stop();
    if (!mounted) return;
    final dir = widget.request.sessionDir;
    ref.read(sessionsProvider.notifier).refresh();
    if (dir == null) {
      Navigator.of(context).pop();
      return;
    }
    final session = await ref.read(sessionsRepositoryProvider).load(Directory(dir));
    if (!mounted) return;
    await openSession(context, session, replace: true);
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final settings = ref.watch(settingsProvider);
    ref.listen(settingsProvider, (_, _) => _applyView());
    final stats = ref.watch(statsProvider).value ?? ScanStats.empty;
    final minimap = ref.watch(minimapProvider).value;
    // Ignore the previous session's model until this scan is running.
    final layout = _started && stats.running ? ref.watch(layoutProvider).value : null;
    final alerts = computeAlerts(stats, l, userMotion: !widget.request.autoTour);
    final scheme = Theme.of(context).colorScheme;

    return PopScope(
      canPop: false,
      onPopInvokedWithResult: (didPop, _) {
        if (!didPop) _confirmStop();
      },
      child: Scaffold(
        body: Stack(
          children: [
            const Positioned.fill(child: PointCloudViewport()),
            SafeArea(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(8, 4, 8, 8),
                child: Column(
                  children: [
                    _topBar(context, l, stats),
                    const SizedBox(height: 6),
                    AlertsBanner(alerts: alerts),
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Expanded(
                          child: _hudVisible ? Hud(stats: stats, items: settings.hudItems) : const SizedBox.shrink(),
                        ),
                        const SizedBox(width: 8),
                        Semantics(
                          button: true,
                          label: l.minimapSemantics,
                          hint: _bigMinimap ? l.collapse : l.expand,
                          onTap: () => setState(() => _bigMinimap = !_bigMinimap),
                          child: GestureDetector(
                            onTap: () => setState(() => _bigMinimap = !_bigMinimap),
                            child: Tooltip(
                              message: l.minimapHint,
                              child: ExcludeSemantics(
                                child: Minimap(data: minimap, stats: stats, size: _bigMinimap ? 240 : 128),
                              ),
                            ),
                          ),
                        ),
                      ],
                    ),
                    if (_bigModel) ...[
                      const SizedBox(height: 8),
                      Expanded(child: _modelCard(context, l, layout, stats, expanded: true)),
                      const SizedBox(height: 8),
                    ] else
                      const Spacer(),
                    if (widget.request.mode == ScanMode.station) _stationPanel(context, l, stats),
                    if (!_bigModel) _dock(context, l, layout, stats),
                    _controlBar(context, l, settings, stats),
                  ],
                ),
              ),
            ),
            if (_stopping)
              Positioned.fill(
                child: ColoredBox(
                  color: scheme.surface.withValues(alpha: 0.92),
                  child: Center(
                    child: Semantics(
                      liveRegion: true,
                      child: Column(mainAxisSize: MainAxisSize.min, children: [
                        const CircularProgressIndicator(),
                        const SizedBox(height: 16),
                        Text(l.exporting, textAlign: TextAlign.center),
                      ]),
                    ),
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }

  /// Bottom dock: simulator panel (camera, motion source) and the live model thumbnail.
  Widget _dock(BuildContext context, AppLocalizations l, RoomLayout? layout, ScanStats stats) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.end,
        children: [
          Expanded(child: _usesMotion ? _simulatorPanel(context, l, stats) : const SizedBox.shrink()),
          const SizedBox(width: 8),
          SizedBox(width: 164, height: 164, child: _modelCard(context, l, layout, stats, expanded: false)),
        ],
      ),
    );
  }

  /// RoomPlan-like live model: grows while scanning; drag to rotate, pinch to zoom.
  Widget _modelCard(BuildContext context, AppLocalizations l, RoomLayout? layout, ScanStats stats,
      {required bool expanded}) {
    Offset? device, forward;
    if (layout != null && stats.hasDevice) {
      device = layout.correction.apply(Offset(stats.deviceX, stats.deviceZ));
      forward = layout.correction.rotate(Offset(stats.fwdX, stats.fwdZ));
    }
    final theme = Theme.of(context);
    return DecoratedBox(
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(22),
        border: Border.all(color: Colors.white.withValues(alpha: 0.18)),
        boxShadow: const [BoxShadow(color: Color(0x66000000), blurRadius: 16, offset: Offset(0, 6))],
      ),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(22),
        child: Stack(
          children: [
            Positioned.fill(
              child: LayoutModelView(
                layout: layout,
                device: device,
                deviceForward: forward,
                compact: !expanded,
                showControls: expanded,
                emptyMessage: _started ? l.modelEmptyLive : l.modelEmpty,
              ),
            ),
            Positioned(
              left: 10,
              top: 8,
              right: 48,
              child: IgnorePointer(
                child: ExcludeSemantics(
                  child: Row(children: [
                    const _LiveDot(),
                    const SizedBox(width: 6),
                    Flexible(
                      child: Text(
                        expanded ? l.modelLiveTitle : l.modelLiveShort,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: theme.textTheme.labelMedium?.copyWith(color: Colors.white, fontWeight: FontWeight.w600),
                      ),
                    ),
                  ]),
                ),
              ),
            ),
            Positioned(
              right: 0,
              top: 0,
              child: IconButton(
                tooltip: expanded ? l.collapseModel : l.expandModel,
                color: Colors.white,
                icon: Icon(expanded ? Icons.close_fullscreen : Icons.open_in_full),
                onPressed: () => setState(() => _bigModel = !_bigModel),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _topBar(BuildContext context, AppLocalizations l, ScanStats stats) {
    final scheme = Theme.of(context).colorScheme;
    final recording = widget.request.sessionDir != null && stats.running && !stats.paused;
    return Row(
      children: [
        IconButton.filledTonal(tooltip: l.closeScan, icon: const Icon(Icons.close), onPressed: _confirmStop),
        const SizedBox(width: 8),
        Expanded(
          child: DecoratedBox(
            decoration: BoxDecoration(
              color: scheme.surface.withValues(alpha: 0.9),
              borderRadius: BorderRadius.circular(20),
            ),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
              child: Row(
                children: [
                  if (recording) ...[
                    Semantics(
                      label: l.recording,
                      child: const Icon(Icons.fiber_manual_record, color: Color(0xFFD32F2F), size: 14),
                    ),
                    const SizedBox(width: 6),
                  ],
                  if (stats.paused) ...[
                    Icon(Icons.pause_circle, color: scheme.tertiary, size: 16),
                    const SizedBox(width: 6),
                  ],
                  Expanded(
                    child: Text(
                      stats.paused ? '${l.paused} · ${widget.request.name}' : widget.request.name,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: Theme.of(context).textTheme.titleSmall,
                    ),
                  ),
                  Text(formatDuration(stats.durationS),
                      style: const TextStyle(fontFeatures: [FontFeature.tabularFigures()])),
                ],
              ),
            ),
          ),
        ),
        const SizedBox(width: 8),
        IconButton.filledTonal(
          tooltip: _hudVisible ? l.hideHud : l.showHud,
          icon: Icon(_hudVisible ? Icons.visibility_off_outlined : Icons.visibility_outlined),
          onPressed: () => setState(() => _hudVisible = !_hudVisible),
        ),
      ],
    );
  }

  Widget _stationPanel(BuildContext context, AppLocalizations l, ScanStats stats) {
    final scheme = Theme.of(context).colorScheme;
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: DecoratedBox(
        decoration: BoxDecoration(color: scheme.surface.withValues(alpha: 0.8), borderRadius: BorderRadius.circular(14)),
        child: Padding(
          padding: const EdgeInsets.fromLTRB(12, 8, 8, 8),
          child: Row(
            children: [
              Icon(stats.stationMoving ? Icons.directions_walk : Icons.threesixty, color: scheme.primary),
              const SizedBox(width: 10),
              Expanded(
                child: Tooltip(
                  message: l.stationModeHint,
                  child: Text(stats.stationMoving ? l.stationMoving : l.stationCapturing(stats.stationCount)),
                ),
              ),
              FilledButton.tonal(
                onPressed: _started ? () => ref.read(bridgeProvider).stationToggle() : null,
                child: Text(stats.stationMoving ? l.stationFix : l.stationMove),
              ),
            ],
          ),
        ),
      ),
    );
  }

  /// Camera thumbnail (ARCore) and what is tracking the phone's real motion.
  Widget _simulatorPanel(BuildContext context, AppLocalizations l, ScanStats stats) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final source = _motion?.source ?? MotionSource.none;
    final texture = _previewTexture;
    final showHint = _started && (!stats.motionReady || stats.distanceM < 2);
    return Row(
      crossAxisAlignment: CrossAxisAlignment.end,
      children: [
        if (texture != null) ...[
          Semantics(
            image: true,
            label: l.cameraPreview,
            child: ClipRRect(
              borderRadius: BorderRadius.circular(12),
              child: SizedBox(width: _previewW, height: _previewH, child: Texture(textureId: texture)),
            ),
          ),
          const SizedBox(width: 8),
        ],
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              if (source != MotionSource.none)
                Tooltip(
                  message: source == MotionSource.arcore ? l.motionArcoreHint : l.motionImuHint,
                  child: Chip(
                    visualDensity: VisualDensity.compact,
                    backgroundColor: scheme.surface.withValues(alpha: 0.92),
                    avatar: Icon(source == MotionSource.arcore ? Icons.view_in_ar : Icons.explore_outlined, size: 18),
                    label: Text(source == MotionSource.arcore ? l.motionArcore : l.motionImu),
                  ),
                ),
              if (showHint)
                DecoratedBox(
                  decoration: BoxDecoration(
                    color: scheme.surface.withValues(alpha: 0.92),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                    child: Row(children: [
                      Icon(Icons.directions_walk, size: 18, color: scheme.primary),
                      const SizedBox(width: 8),
                      Expanded(child: Text(l.simulatorWalkHint, style: theme.textTheme.bodySmall)),
                    ]),
                  ),
                ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _controlBar(BuildContext context, AppLocalizations l, AppSettings settings, ScanStats stats) {
    final scheme = Theme.of(context).colorScheme;
    final notifier = ref.read(settingsProvider.notifier);
    final camera = settings.camera;
    final color = settings.color;
    return DecoratedBox(
      decoration: BoxDecoration(color: scheme.surface.withValues(alpha: 0.94), borderRadius: BorderRadius.circular(18)),
      child: Padding(
        padding: const EdgeInsets.all(8),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            SegmentedButton<CameraMode>(
              showSelectedIcon: false,
              segments: [
                ButtonSegment(value: CameraMode.follow, icon: const Icon(Icons.person_pin_circle_outlined), tooltip: l.cameraFollow),
                ButtonSegment(value: CameraMode.orbit, icon: const Icon(Icons.threed_rotation), tooltip: l.cameraOrbit),
                ButtonSegment(value: CameraMode.firstPerson, icon: const Icon(Icons.visibility_outlined), tooltip: l.cameraFirstPerson),
                ButtonSegment(value: CameraMode.top, icon: const Icon(Icons.map_outlined), tooltip: l.cameraTop),
              ],
              selected: {camera},
              onSelectionChanged: (s) {
                notifier.update((x) => x.copyWith(camera: s.first));
                setState(() {});
              },
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                PopupMenuButton<ColorMode>(
                  tooltip: l.colorMode,
                  initialValue: color,
                  onSelected: (c) => notifier.update((x) => x.copyWith(color: c)),
                  itemBuilder: (_) => [
                    PopupMenuItem(value: ColorMode.height, child: Text(l.colorHeight)),
                    PopupMenuItem(value: ColorMode.intensity, child: Text(l.colorIntensity)),
                    PopupMenuItem(value: ColorMode.age, child: Text(l.colorAge)),
                  ],
                  child: Chip(
                    avatar: const Icon(Icons.palette_outlined, size: 18),
                    label: Text(switch (color) {
                      ColorMode.height => l.colorHeight,
                      ColorMode.intensity => l.colorIntensity,
                      ColorMode.age => l.colorAge,
                    }),
                  ),
                ),
                const SizedBox(width: 4),
                IconButton(
                  tooltip: l.resetView,
                  icon: const Icon(Icons.center_focus_strong_outlined),
                  onPressed: () => ref.read(bridgeProvider).resetView(),
                ),
                const Spacer(),
                IconButton.filledTonal(
                  tooltip: stats.paused ? l.resume : l.pause,
                  icon: Icon(stats.paused ? Icons.play_arrow : Icons.pause),
                  onPressed: _started ? () => ref.read(bridgeProvider).setPaused(!stats.paused) : null,
                ),
                const SizedBox(width: 8),
                FilledButton.icon(
                  style: FilledButton.styleFrom(backgroundColor: scheme.error, foregroundColor: scheme.onError),
                  onPressed: _started && !_stopping ? _confirmStop : null,
                  icon: const Icon(Icons.stop),
                  label: Text(l.stop),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

/// Small pulsing "live" indicator (static when the system asks to reduce motion).
class _LiveDot extends StatefulWidget {
  const _LiveDot();

  @override
  State<_LiveDot> createState() => _LiveDotState();
}

class _LiveDotState extends State<_LiveDot> with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(vsync: this, duration: const Duration(milliseconds: 1400));

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (MediaQuery.disableAnimationsOf(context)) {
      _c.stop();
      _c.value = 1;
    } else if (!_c.isAnimating) {
      _c.repeat(reverse: true);
    }
  }

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => FadeTransition(
        opacity: Tween(begin: 0.35, end: 1.0).animate(_c),
        child: Container(
          width: 8,
          height: 8,
          decoration: const BoxDecoration(color: Color(0xFF4DD0E1), shape: BoxShape.circle),
        ),
      );
}
