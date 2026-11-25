import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/format.dart';
import '../../core/models.dart';
import '../../core/providers.dart';
import '../../core/settings.dart';
import '../../l10n/app_localizations.dart';

class SettingsScreen extends ConsumerWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l = AppLocalizations.of(context);
    final s = ref.watch(settingsProvider);
    final device = ref.watch(deviceInfoProvider).value;
    final n = ref.read(settingsProvider.notifier);
    void set(AppSettings Function(AppSettings) f) => n.update(f);

    String hudLabel(HudItem i) => switch (i) {
          HudItem.points => l.hudPoints,
          HudItem.rate => l.hudRate,
          HudItem.tracking => l.hudTracking,
          HudItem.link => '${l.hudLoss} / ${l.hudLatency}',
          HudItem.rpm => l.hudRpm,
          HudItem.duration => l.hudDuration,
          HudItem.distance => l.hudDistance,
          HudItem.battery => '${l.hudBattery} / ${l.hudTemp}',
        };

    return Scaffold(
      appBar: AppBar(title: Text(l.settings)),
      body: ListView(
        padding: const EdgeInsets.only(bottom: 32),
        children: [
          _Section(l.settingsAppearance),
          ListTile(
            title: Text(l.theme),
            trailing: SegmentedButton<ThemeMode>(
              showSelectedIcon: false,
              segments: [
                ButtonSegment(value: ThemeMode.system, icon: const Icon(Icons.brightness_auto), tooltip: l.themeSystem),
                ButtonSegment(value: ThemeMode.light, icon: const Icon(Icons.light_mode), tooltip: l.themeLight),
                ButtonSegment(value: ThemeMode.dark, icon: const Icon(Icons.dark_mode), tooltip: l.themeDark),
              ],
              selected: {s.themeMode},
              onSelectionChanged: (v) => set((x) => x.copyWith(themeMode: v.first)),
            ),
          ),
          ListTile(
            title: Text(l.language),
            trailing: DropdownButton<String?>(
              value: s.localeCode,
              onChanged: (v) => set((x) => x.copyWith(localeCode: () => v)),
              items: [
                DropdownMenuItem(value: null, child: Text(l.languageSystem)),
                const DropdownMenuItem(value: 'es', child: Text('Español')),
                const DropdownMenuItem(value: 'en', child: Text('English')),
              ],
            ),
          ),

          _Section(l.settingsViewer),
          _SliderTile(
            label: l.pointSize,
            value: s.pointSize,
            min: 0.5,
            max: 3,
            divisions: 10,
            format: (v) => '×${v.toStringAsFixed(2)}',
            onChanged: (v) => set((x) => x.copyWith(pointSize: v)),
          ),
          SwitchListTile(
            title: Text(l.showTrajectory),
            value: s.showTrajectory,
            onChanged: (v) => set((x) => x.copyWith(showTrajectory: v)),
          ),
          SwitchListTile(title: Text(l.showGrid), value: s.showGrid, onChanged: (v) => set((x) => x.copyWith(showGrid: v))),
          _SliderTile(
            label: l.ageWindow,
            value: s.ageWindowS,
            min: 10,
            max: 600,
            divisions: 59,
            format: (v) => '${v.round()} s',
            onChanged: (v) => set((x) => x.copyWith(ageWindowS: v)),
          ),
          ListTile(
            title: Text(l.targetFps),
            trailing: SegmentedButton<int>(
              showSelectedIcon: false,
              segments: const [
                ButtonSegment(value: 0, label: Text('Auto')),
                ButtonSegment(value: 30, label: Text('30')),
                ButtonSegment(value: 60, label: Text('60')),
              ],
              selected: {s.targetFps},
              onSelectionChanged: (v) => set((x) => x.copyWith(targetFps: v.first)),
            ),
          ),

          _Section(l.settingsHud),
          SwitchListTile(
            title: Text(l.hudVisibleByDefault),
            value: s.hudVisible,
            onChanged: (v) => set((x) => x.copyWith(hudVisible: v)),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            child: Wrap(spacing: 8, runSpacing: 4, children: [
              for (final i in HudItem.values)
                FilterChip(
                  label: Text(hudLabel(i)),
                  selected: s.hudItems.contains(i),
                  onSelected: (on) => set((x) => x.copyWith(hudItems: on ? {...x.hudItems, i} : ({...x.hudItems}..remove(i)))),
                ),
            ]),
          ),

          _Section(l.settingsMapping),
          _SliderTile(
            label: l.voxel,
            value: s.voxelCm,
            min: 0.5,
            max: 10,
            divisions: 19,
            format: (v) => '${v.toStringAsFixed(1)} cm',
            onChanged: (v) => set((x) => x.copyWith(voxelCm: v)),
          ),
          ListTile(
            title: Text(l.pointBudget),
            trailing: DropdownButton<int>(
              value: s.pointBudget,
              onChanged: (v) => set((x) => x.copyWith(pointBudget: v)),
              items: [
                DropdownMenuItem(value: 0, child: Text(l.budgetAuto(formatCount(device?.recommendedBudget ?? 1000000)))),
                for (final b in [500000, 1000000, 2000000, 3000000, 5000000])
                  DropdownMenuItem(value: b, child: Text(formatCount(b))),
              ],
            ),
          ),
          _SliderTile(
            label: l.minRange,
            value: s.minRangeM,
            min: 0.02,
            max: 1,
            divisions: 49,
            format: (v) => '${v.toStringAsFixed(2)} m',
            onChanged: (v) => set((x) => x.copyWith(minRangeM: v)),
          ),
          _SliderTile(
            label: l.maxRange,
            value: s.maxRangeM,
            min: 2,
            max: 12,
            divisions: 20,
            format: (v) => '${v.toStringAsFixed(1)} m',
            onChanged: (v) => set((x) => x.copyWith(maxRangeM: v)),
          ),
          _SliderTile(
            label: l.minIntensity,
            value: s.minIntensity.toDouble(),
            min: 0,
            max: 200,
            divisions: 40,
            format: (v) => '${v.round()}',
            onChanged: (v) => set((x) => x.copyWith(minIntensity: v.round())),
          ),

          _Section(l.settingsAlerts),
          _SliderTile(
            label: l.tiltThreshold,
            value: s.tiltAlertDeg,
            min: 5,
            max: 45,
            divisions: 40,
            format: (v) => '${v.round()}°',
            onChanged: (v) => set((x) => x.copyWith(tiltAlertDeg: v)),
          ),
          _SliderTile(
            label: l.gapThreshold,
            value: s.gapAlertCm,
            min: 1,
            max: 20,
            divisions: 38,
            format: (v) => '${v.toStringAsFixed(1)} cm',
            onChanged: (v) => set((x) => x.copyWith(gapAlertCm: v)),
          ),

          _Section(l.settingsConnection),
          _TextTile(label: l.espHost, value: s.espHost, onSubmitted: (v) => set((x) => x.copyWith(espHost: v.trim()))),
          _TextTile(
            label: l.espPort,
            value: '${s.espPort}',
            number: true,
            onSubmitted: (v) => set((x) => x.copyWith(espPort: int.tryParse(v) ?? x.espPort)),
          ),
          _TextTile(
            label: l.phonePort,
            value: '${s.phonePort}',
            number: true,
            onSubmitted: (v) => set((x) => x.copyWith(phonePort: int.tryParse(v) ?? x.phonePort)),
          ),

          _Section(l.settingsCalibration),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 8),
            child: Text(l.calibrationHint, style: Theme.of(context).textTheme.bodySmall),
          ),
          for (final (label, value, apply) in <(String, double, AppSettings Function(AppSettings, double))>[
            (l.extTx, s.extTx, (x, v) => x.copyWith(extTx: v)),
            (l.extTy, s.extTy, (x, v) => x.copyWith(extTy: v)),
            (l.extTz, s.extTz, (x, v) => x.copyWith(extTz: v)),
            (l.extRoll, s.extRoll, (x, v) => x.copyWith(extRoll: v)),
            (l.extPitch, s.extPitch, (x, v) => x.copyWith(extPitch: v)),
            (l.extYaw, s.extYaw, (x, v) => x.copyWith(extYaw: v)),
            (l.lidarLatency, s.lidarLatencyMs, (x, v) => x.copyWith(lidarLatencyMs: v)),
          ])
            _TextTile(
              label: label,
              value: value.toString(),
              number: true,
              onSubmitted: (v) {
                final d = double.tryParse(v.replaceAll(',', '.'));
                if (d != null) set((x) => apply(x, d));
              },
            ),

          _Section(l.settingsMock),
          _SliderTile(
            label: l.mockNoise,
            value: s.mockNoiseMm,
            min: 0,
            max: 50,
            divisions: 50,
            format: (v) => '± ${v.round()} mm',
            onChanged: (v) => set((x) => x.copyWith(mockNoiseMm: v)),
          ),
          SwitchListTile(title: Text(l.mockGlass), value: s.mockGlass, onChanged: (v) => set((x) => x.copyWith(mockGlass: v))),
          SwitchListTile(
            title: Text(l.mockIntensity),
            value: s.mockIntensity,
            onChanged: (v) => set((x) => x.copyWith(mockIntensity: v)),
          ),
          SwitchListTile(title: Text(l.mockDrift), value: s.mockDrift, onChanged: (v) => set((x) => x.copyWith(mockDrift: v))),
          if (s.mockDrift) ...[
            _SliderTile(
              label: l.mockDriftPct,
              value: s.mockDriftPct,
              min: 0,
              max: 5,
              divisions: 50,
              format: (v) => '${v.toStringAsFixed(1)} %',
              onChanged: (v) => set((x) => x.copyWith(mockDriftPct: v)),
            ),
            _SliderTile(
              label: l.mockDriftYaw,
              value: s.mockDriftYaw,
              min: 0,
              max: 3,
              divisions: 30,
              format: (v) => '${v.toStringAsFixed(1)} °/m',
              onChanged: (v) => set((x) => x.copyWith(mockDriftYaw: v)),
            ),
            SwitchListTile(
              title: Text(l.mockLoopClosure),
              value: s.mockLoopClosure,
              onChanged: (v) => set((x) => x.copyWith(mockLoopClosure: v)),
            ),
          ],
          SwitchListTile(
            title: Text(l.mockPacketLoss),
            value: s.mockPacketLoss,
            onChanged: (v) => set((x) => x.copyWith(mockPacketLoss: v)),
          ),
          if (s.mockPacketLoss)
            _SliderTile(
              label: l.mockPacketLossPct,
              value: s.mockPacketLossPct,
              min: 0,
              max: 20,
              divisions: 40,
              format: (v) => '${v.toStringAsFixed(1)} %',
              onChanged: (v) => set((x) => x.copyWith(mockPacketLossPct: v)),
            ),
          SwitchListTile(
            title: Text(l.mockTrackingLoss),
            value: s.mockTrackingLoss,
            onChanged: (v) => set((x) => x.copyWith(mockTrackingLoss: v)),
          ),
          if (s.mockTrackingLoss)
            _SliderTile(
              label: l.mockTrackingLossRate,
              value: s.mockTrackingLossPerMin,
              min: 0,
              max: 5,
              divisions: 20,
              format: (v) => v.toStringAsFixed(2),
              onChanged: (v) => set((x) => x.copyWith(mockTrackingLossPerMin: v)),
            ),
          const SizedBox(height: 16),
          Center(
            child: OutlinedButton.icon(
              icon: const Icon(Icons.restore),
              label: Text(l.resetDefaults),
              onPressed: n.reset,
            ),
          ),
        ],
      ),
    );
  }
}

class _Section extends StatelessWidget {
  const _Section(this.title);
  final String title;

  @override
  Widget build(BuildContext context) {
    final t = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 24, 16, 4),
      child: Text(title, style: t.textTheme.titleSmall?.copyWith(color: t.colorScheme.primary)),
    );
  }
}

class _SliderTile extends StatelessWidget {
  const _SliderTile({
    required this.label,
    required this.value,
    required this.min,
    required this.max,
    required this.divisions,
    required this.format,
    required this.onChanged,
  });

  final String label;
  final double value, min, max;
  final int divisions;
  final String Function(double) format;
  final ValueChanged<double> onChanged;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(children: [Expanded(child: Text(label)), Text(format(value))]),
          Slider(value: value.clamp(min, max), min: min, max: max, divisions: divisions, onChanged: onChanged),
        ],
      ),
    );
  }
}

class _TextTile extends StatefulWidget {
  const _TextTile({required this.label, required this.value, required this.onSubmitted, this.number = false});
  final String label;
  final String value;
  final bool number;
  final ValueChanged<String> onSubmitted;

  @override
  State<_TextTile> createState() => _TextTileState();
}

class _TextTileState extends State<_TextTile> {
  late final _c = TextEditingController(text: widget.value);
  final _focus = FocusNode();

  @override
  void initState() {
    super.initState();
    _focus.addListener(() {
      if (!_focus.hasFocus) widget.onSubmitted(_c.text);
    });
  }

  @override
  void didUpdateWidget(covariant _TextTile old) {
    super.didUpdateWidget(old);
    if (!_focus.hasFocus && old.value != widget.value) _c.text = widget.value;
  }

  @override
  void dispose() {
    _c.dispose();
    _focus.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 6),
      child: TextField(
        controller: _c,
        focusNode: _focus,
        keyboardType: widget.number ? const TextInputType.numberWithOptions(decimal: true, signed: true) : TextInputType.text,
        decoration: InputDecoration(labelText: widget.label, border: const OutlineInputBorder(), isDense: true),
        onSubmitted: widget.onSubmitted,
      ),
    );
  }
}
