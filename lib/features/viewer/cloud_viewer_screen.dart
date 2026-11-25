import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';
import 'package:share_plus/share_plus.dart';

import '../../core/format.dart';
import '../../core/layout.dart';
import '../../core/models.dart';
import '../../core/plan_svg.dart';
import '../../core/providers.dart';
import '../../core/scanner_bridge.dart';
import '../../core/sessions.dart';
import '../../l10n/app_localizations.dart';
import '../layout/floor_plan_view.dart';
import '../layout/model_view.dart';
import '../scanner/widgets/minimap.dart';
import '../scanner/widgets/viewport.dart';
import '../session_detail/session_detail_screen.dart';

enum ViewerTab { model, plan, cloud }

/// Result of a saved session: RoomPlan-like 3D model, clean floor plan with measurements, and
/// the raw point cloud (cloud.ply + trajectory) in the native OpenGL renderer.
class CloudViewerScreen extends ConsumerStatefulWidget {
  const CloudViewerScreen({super.key, required this.session});
  final SessionInfo session;

  static bool canView(SessionInfo s) => s.files.any((f) => f.kind == SessionFileKind.ply);

  @override
  ConsumerState<CloudViewerScreen> createState() => _CloudViewerScreenState();
}

class _CloudViewerScreenState extends ConsumerState<CloudViewerScreen> {
  late final ScannerBridge _bridge = ref.read(bridgeProvider);
  bool _loading = true;
  String? _error;
  int _points = 0;
  CameraMode _camera = CameraMode.orbit;
  bool _clipCeiling = true;
  bool _bigMinimap = false;
  ViewerTab _tab = ViewerTab.model;
  RoomLayout? _layout;
  bool _layoutLoading = true;
  bool _showFurniture = true;
  bool _exporting = false;

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    _applyView();
  }

  Future<void> _load() async {
    // A saved model opens instantly; the cloud loads meanwhile.
    final file = widget.session.layoutFile;
    if (file.existsSync()) {
      final saved = RoomLayout.tryParse(await file.readAsString().catchError((_) => ''));
      if (mounted && saved != null) {
        setState(() {
          _layout = saved;
          _layoutLoading = false;
        });
      }
    }
    if (!CloudViewerScreen.canView(widget.session)) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _layoutLoading = false;
        _error = '';
      });
      return;
    }
    try {
      final n = await _bridge.loadCloud(widget.session.dir.path);
      if (!mounted) return;
      setState(() {
        _points = n;
        _loading = false;
      });
      _applyView();
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _layoutLoading = false;
        _error = e is StateError ? e.message : '$e';
      });
      return;
    }
    if (_layout == null) {
      // Sessions recorded before the layout engine existed: compute and keep it.
      final json = await _bridge.computeLayout();
      final computed = RoomLayout.tryParse(json);
      if (computed != null && computed.valid) file.writeAsString(json).ignore();
      if (!mounted) return;
      setState(() {
        _layout = computed;
        _layoutLoading = false;
      });
    }
  }

  void _applyView() {
    final s = ref.read(settingsProvider);
    _bridge.setView(
      camera: _camera,
      color: s.color,
      pointSize: s.pointSize,
      dark: Theme.of(context).brightness == Brightness.dark,
      trajectory: s.showTrajectory,
      grid: s.showGrid,
      ageWindowS: s.ageWindowS,
      fps: s.effectiveFps(ref.read(deviceInfoProvider).value),
      clipCeiling: _clipCeiling,
    );
  }

  Future<void> _openInfo() async {
    final deleted = await Navigator.of(context).push<bool>(
      MaterialPageRoute(builder: (_) => SessionDetailScreen(session: widget.session, fromViewer: true)),
    );
    if (deleted == true && mounted) Navigator.of(context).pop();
  }

  Future<void> _exportPlan(RoomLayout layout) async {
    final l = AppLocalizations.of(context);
    final locale = Localizations.localeOf(context).toLanguageTag();
    final date = DateFormat.yMMMMd(locale).format(widget.session.createdAt);
    setState(() => _exporting = true);
    try {
      final svg = buildFloorPlanSvg(
        layout,
        PlanSvgTexts(
          title: widget.session.name,
          subtitle: l.planSubtitle(date, formatArea(layout.totalArea, locale)),
          roomNames: roomNames(l, layout),
          door: l.legendDoor,
          window: l.legendWindow,
          disclaimer: l.planDisclaimer,
        ),
        locale,
      );
      final file = widget.session.floorPlanSvgFile;
      await file.writeAsString(svg);
      await SharePlus.instance.share(ShareParams(
        files: [XFile(file.path, mimeType: 'image/svg+xml')],
        subject: '${widget.session.name} · ${l.tabPlan}',
      ));
    } catch (e) {
      if (mounted) ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(l.planExportError('$e'))));
    } finally {
      if (mounted) setState(() => _exporting = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final settings = ref.watch(settingsProvider);
    ref.listen(settingsProvider, (_, _) => _applyView());
    final scheme = Theme.of(context).colorScheme;
    final cloudReady = !_loading && _error == null;
    final minimap = cloudReady && _tab == ViewerTab.cloud ? ref.watch(minimapProvider).value : null;
    final layout = _layout;

    final Widget content = switch (_tab) {
      ViewerTab.model => _layoutState(l) ??
          LayoutModelView(
            layout: layout,
            showFurniture: _showFurniture,
            emptyMessage: l.layoutUnavailable,
          ),
      ViewerTab.plan => _layoutState(l) ??
          ((layout == null || layout.walls.isEmpty)
              ? _message(Icons.layers_clear_outlined, l.layoutUnavailable)
              : FloorPlanView(layout: layout, roomNames: roomNames(l, layout))),
      ViewerTab.cloud => cloudReady ? const PointCloudViewport() : _cloudState(l, scheme),
    };

    return Scaffold(
      backgroundColor: _tab == ViewerTab.model ? ModelPalette.backgroundBottom : null,
      body: Stack(
        children: [
          Positioned.fill(child: content),
          SafeArea(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(8, 4, 8, 8),
              child: Column(
                children: [
                  _header(context, l, cloudReady),
                  const SizedBox(height: 8),
                  _tabs(l),
                  const SizedBox(height: 8),
                  if (_tab == ViewerTab.model && layout != null && !layout.isEmpty) _modelBar(context, l, layout),
                  if (_tab == ViewerTab.cloud && cloudReady)
                    Align(
                      alignment: Alignment.topRight,
                      child: Semantics(
                        button: true,
                        label: l.minimapSemantics,
                        hint: _bigMinimap ? l.collapse : l.expand,
                        onTap: () => setState(() => _bigMinimap = !_bigMinimap),
                        child: GestureDetector(
                          onTap: () => setState(() => _bigMinimap = !_bigMinimap),
                          child: Tooltip(
                            message: l.minimapHint,
                            child: ExcludeSemantics(
                              child: Minimap(data: minimap, stats: ScanStats.empty, size: _bigMinimap ? 240 : 128),
                            ),
                          ),
                        ),
                      ),
                    ),
                  const Spacer(),
                  if (_tab == ViewerTab.cloud && cloudReady) _cloudBar(context, l, settings.color, settings.pointSize),
                  if (_tab == ViewerTab.plan && layout != null && layout.walls.isNotEmpty) _planBar(context, l, layout),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget? _layoutState(AppLocalizations l) {
    if (_layout != null) return null;
    if (_layoutLoading) {
      return Center(
        child: Semantics(
          liveRegion: true,
          child: Column(mainAxisSize: MainAxisSize.min, children: [
            const CircularProgressIndicator(),
            const SizedBox(height: 16),
            Text(l.layoutComputing),
          ]),
        ),
      );
    }
    return _message(Icons.layers_clear_outlined, _error != null && _error!.isNotEmpty ? l.cloudLoadError(_error!) : l.layoutUnavailable);
  }

  Widget _cloudState(AppLocalizations l, ColorScheme scheme) {
    if (_loading) {
      return Center(
        child: Semantics(
          liveRegion: true,
          child: Column(mainAxisSize: MainAxisSize.min, children: [
            const CircularProgressIndicator(),
            const SizedBox(height: 16),
            Text(l.loadingCloud),
          ]),
        ),
      );
    }
    return _message(Icons.cloud_off, _error!.isEmpty ? l.noCloud : l.cloudLoadError(_error!),
        action: FilledButton.tonalIcon(onPressed: _openInfo, icon: const Icon(Icons.info_outline), label: Text(l.sessionInfo)));
  }

  Widget _message(IconData icon, String text, {Widget? action}) {
    final scheme = Theme.of(context).colorScheme;
    return Center(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(32, 140, 32, 32),
        child: Column(mainAxisSize: MainAxisSize.min, children: [
          Icon(icon, size: 56, color: scheme.onSurfaceVariant),
          const SizedBox(height: 16),
          Text(text, textAlign: TextAlign.center, style: Theme.of(context).textTheme.bodyLarge),
          if (action != null) ...[const SizedBox(height: 16), action],
        ]),
      ),
    );
  }

  Widget _header(BuildContext context, AppLocalizations l, bool cloudReady) {
    final scheme = Theme.of(context).colorScheme;
    final layout = _layout;
    final locale = Localizations.localeOf(context).toLanguageTag();
    final facts = [
      if (layout != null && layout.totalArea > 0) formatArea(layout.totalArea, locale),
      if (layout != null && layout.rooms.isNotEmpty) l.roomCount(layout.rooms.length),
      if (cloudReady) l.cloudPoints(formatCount(_points)),
      formatDuration(widget.session.durationS),
    ];
    return Row(
      children: [
        IconButton.filledTonal(
          tooltip: MaterialLocalizations.of(context).backButtonTooltip,
          icon: const BackButtonIcon(),
          onPressed: () => Navigator.of(context).pop(),
        ),
        const SizedBox(width: 8),
        Expanded(
          child: DecoratedBox(
            decoration: BoxDecoration(
              color: scheme.surface.withValues(alpha: 0.94),
              borderRadius: BorderRadius.circular(20),
            ),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 6),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Semantics(
                    header: true,
                    child: Text(widget.session.name,
                        maxLines: 1, overflow: TextOverflow.ellipsis, style: Theme.of(context).textTheme.titleSmall),
                  ),
                  Text(facts.join(' · '),
                      maxLines: 2, overflow: TextOverflow.ellipsis, style: Theme.of(context).textTheme.bodySmall),
                ],
              ),
            ),
          ),
        ),
        const SizedBox(width: 8),
        IconButton.filledTonal(tooltip: l.sessionInfo, icon: const Icon(Icons.info_outline), onPressed: _openInfo),
      ],
    );
  }

  Widget _tabs(AppLocalizations l) {
    final scheme = Theme.of(context).colorScheme;
    return DecoratedBox(
      decoration: BoxDecoration(color: scheme.surface.withValues(alpha: 0.94), borderRadius: BorderRadius.circular(24)),
      child: Padding(
        padding: const EdgeInsets.all(4),
        child: SegmentedButton<ViewerTab>(
          showSelectedIcon: false,
          segments: [
            ButtonSegment(value: ViewerTab.model, icon: const Icon(Icons.view_in_ar_outlined), label: Text(l.tabModel)),
            ButtonSegment(value: ViewerTab.plan, icon: const Icon(Icons.square_foot), label: Text(l.tabPlan)),
            ButtonSegment(value: ViewerTab.cloud, icon: const Icon(Icons.grain), label: Text(l.tabCloud)),
          ],
          selected: {_tab},
          onSelectionChanged: (s) {
            setState(() => _tab = s.first);
            if (_tab == ViewerTab.cloud) _applyView();
          },
        ),
      ),
    );
  }

  Widget _panel(Widget child) {
    final scheme = Theme.of(context).colorScheme;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: scheme.surface.withValues(alpha: 0.96),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: scheme.outlineVariant.withValues(alpha: 0.6)),
      ),
      child: Padding(padding: const EdgeInsets.fromLTRB(12, 8, 8, 8), child: child),
    );
  }

  Widget _modelBar(BuildContext context, AppLocalizations l, RoomLayout layout) {
    return Align(
      alignment: Alignment.centerRight,
      child: _panel(Row(mainAxisSize: MainAxisSize.min, children: [
        MergeSemantics(
          child: Row(mainAxisSize: MainAxisSize.min, children: [
            Text(l.furniture, style: Theme.of(context).textTheme.labelLarge),
            const SizedBox(width: 8),
            Switch(
              value: _showFurniture,
              onChanged: (v) => setState(() => _showFurniture = v),
            ),
          ]),
        ),
        IconButton(tooltip: l.measuresTitle, icon: const Icon(Icons.straighten), onPressed: () => _showMeasures(layout)),
      ])),
    );
  }

  Widget _planBar(BuildContext context, AppLocalizations l, RoomLayout layout) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 56),
      child: _panel(Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const PlanLegend(),
          const SizedBox(height: 8),
          Wrap(spacing: 8, runSpacing: 8, alignment: WrapAlignment.end, children: [
            OutlinedButton.icon(
              onPressed: () => _showMeasures(layout),
              icon: const Icon(Icons.list_alt),
              label: Text(l.measuresTitle),
            ),
            FilledButton.icon(
              onPressed: _exporting ? null : () => _exportPlan(layout),
              icon: _exporting
                  ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2))
                  : const Icon(Icons.ios_share),
              label: Text(l.planExport),
            ),
          ]),
        ],
      )),
    );
  }

  Future<void> _showMeasures(RoomLayout layout) => showModalBottomSheet<void>(
        context: context,
        isScrollControlled: true,
        showDragHandle: true,
        builder: (_) => DraggableScrollableSheet(
          expand: false,
          initialChildSize: 0.6,
          maxChildSize: 0.95,
          builder: (context, controller) => MeasuresList(layout: layout, controller: controller),
        ),
      );

  Widget _cloudBar(BuildContext context, AppLocalizations l, ColorMode color, double pointSize) {
    final notifier = ref.read(settingsProvider.notifier);
    return _panel(Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Row(
          children: [
            const Icon(Icons.grain, size: 18),
            Expanded(
              child: Slider(
                value: pointSize.clamp(0.5, 3.0),
                min: 0.5,
                max: 3,
                divisions: 10,
                label: '${l.pointSize} ×${pointSize.toStringAsFixed(2)}',
                semanticFormatterCallback: (v) => '${l.pointSize} ×${v.toStringAsFixed(2)}',
                onChanged: (v) => notifier.update((x) => x.copyWith(pointSize: v)),
              ),
            ),
          ],
        ),
        Row(
          children: [
            SegmentedButton<CameraMode>(
              showSelectedIcon: false,
              segments: [
                ButtonSegment(value: CameraMode.orbit, icon: const Icon(Icons.threed_rotation), label: Text(l.cameraOrbit)),
                ButtonSegment(value: CameraMode.top, icon: const Icon(Icons.map_outlined), label: Text(l.cameraTop)),
              ],
              selected: {_camera},
              onSelectionChanged: (s) {
                setState(() => _camera = s.first);
                _applyView();
              },
            ),
            const Spacer(),
            IconButton(
              tooltip: l.hideCeiling,
              isSelected: _clipCeiling,
              icon: const Icon(Icons.roofing_outlined),
              selectedIcon: const Icon(Icons.house_siding),
              onPressed: () {
                setState(() => _clipCeiling = !_clipCeiling);
                _applyView();
              },
            ),
            PopupMenuButton<ColorMode>(
              tooltip: l.colorMode,
              initialValue: color,
              onSelected: (c) => notifier.update((x) => x.copyWith(color: c)),
              itemBuilder: (_) => [
                PopupMenuItem(value: ColorMode.height, child: Text(l.colorHeight)),
                PopupMenuItem(value: ColorMode.intensity, child: Text(l.colorIntensity)),
                PopupMenuItem(value: ColorMode.age, child: Text(l.colorAge)),
              ],
              child: const Padding(padding: EdgeInsets.all(12), child: Icon(Icons.palette_outlined)),
            ),
            IconButton(
              tooltip: l.resetView,
              icon: const Icon(Icons.center_focus_strong_outlined),
              onPressed: _bridge.resetView,
            ),
          ],
        ),
      ],
    ));
  }
}

/// Text alternative to the plan (WCAG 1.1.1): every room and wall with its measurements.
class MeasuresList extends StatelessWidget {
  const MeasuresList({super.key, required this.layout, this.controller});
  final RoomLayout layout;
  final ScrollController? controller;

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final locale = Localizations.localeOf(context).toLanguageTag();
    final names = roomNames(l, layout);
    Widget section(String t) => Padding(
          padding: const EdgeInsets.fromLTRB(16, 20, 16, 4),
          child: Semantics(
            header: true,
            child: Text(t, style: theme.textTheme.titleSmall?.copyWith(color: theme.colorScheme.primary)),
          ),
        );
    String len(double m) => formatLength(m, locale);
    return ListView(
      controller: controller,
      padding: const EdgeInsets.only(bottom: 24),
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16),
          child: Semantics(header: true, child: Text(l.measuresTitle, style: theme.textTheme.titleLarge)),
        ),
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 4, 16, 0),
          child: Text(l.planDisclaimer, style: theme.textTheme.bodySmall),
        ),
        ListTile(
          leading: const Icon(Icons.square_foot),
          title: Text(l.measuresTotalArea),
          trailing: Text(formatArea(layout.totalArea, locale), style: theme.textTheme.titleMedium),
        ),
        ListTile(
          leading: const Icon(Icons.height),
          title: Text(l.measuresCeiling),
          subtitle: layout.ceilingEstimated ? Text(l.measuresCeilingEstimated) : null,
          trailing: Text(len(layout.height), style: theme.textTheme.titleMedium),
        ),
        ListTile(
          leading: const Icon(Icons.door_front_door_outlined),
          title: Text(l.measuresOpenings),
          trailing: Text(l.doorsWindows(layout.doorCount, layout.windowCount), style: theme.textTheme.bodyLarge),
        ),
        if (layout.stats.loopEdges > 0)
          ListTile(
            leading: const Icon(Icons.auto_fix_high_outlined),
            title: Text(l.driftCorrected),
            subtitle: Text(l.driftCorrectedDetail(layout.stats.loopEdges,
                (layout.stats.maxCorrectionM * 100).toStringAsFixed(0), layout.stats.maxCorrectionDeg.toStringAsFixed(1))),
          ),
        section(l.measuresRooms),
        for (var i = 0; i < layout.rooms.length; i++)
          ListTile(
            leading: CircleAvatar(child: Text('${i + 1}')),
            title: Text(names[i]),
            subtitle: Text('${len(layout.rooms[i].width)} × ${len(layout.rooms[i].depth)}'),
            trailing: Text(formatArea(layout.rooms[i].area, locale), style: theme.textTheme.titleMedium),
          ),
        section(l.measuresWalls),
        for (var i = 0; i < layout.walls.length; i++)
          ListTile(
            dense: true,
            leading: const Icon(Icons.horizontal_rule),
            title: Text(l.wallItem(i + 1, len(layout.walls[i].length))),
            subtitle: Text([
              l.wallThickness(len(layout.walls[i].thickness)),
              for (final o in layout.walls[i].openings)
                o.kind == OpeningKind.door ? l.doorWidth(len(o.width)) : l.windowWidth(len(o.width)),
            ].join(' · ')),
          ),
      ],
    );
  }
}

/// Opens a session: the result viewer when it has a cloud, otherwise its detail page.
Future<void> openSession(BuildContext context, SessionInfo s, {bool replace = false}) {
  final route = MaterialPageRoute<void>(
    builder: (_) => CloudViewerScreen.canView(s) ? CloudViewerScreen(session: s) : SessionDetailScreen(session: s),
  );
  final nav = Navigator.of(context);
  return replace ? nav.pushReplacement(route) : nav.push(route);
}

