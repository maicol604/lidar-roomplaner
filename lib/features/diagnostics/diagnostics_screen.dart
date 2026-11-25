import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/format.dart';
import '../../core/models.dart';
import '../../core/providers.dart';
import '../../core/scanner_bridge.dart';
import '../../core/session_config.dart';
import '../../l10n/app_localizations.dart';

/// Live 2D polar view of the latest LD06 revolution plus link / clock-sync health.
class DiagnosticsScreen extends ConsumerStatefulWidget {
  const DiagnosticsScreen({super.key});

  @override
  ConsumerState<DiagnosticsScreen> createState() => _DiagnosticsScreenState();
}

class _DiagnosticsScreenState extends ConsumerState<DiagnosticsScreen> {
  bool? _attached; // true: a scan session was already running
  bool _ownSession = false;
  bool _busy = false;
  ScanSource _source = ScanSource.mock;
  late final ScannerBridge _bridge;

  @override
  void initState() {
    super.initState();
    _bridge = ref.read(bridgeProvider);
    _bridge.isRunning().then((r) {
      if (mounted) setState(() => _attached = r);
    });
  }

  @override
  void dispose() {
    if (_ownSession) _bridge.stop();
    super.dispose();
  }

  Future<void> _toggle() async {
    final bridge = ref.read(bridgeProvider);
    setState(() => _busy = true);
    if (_ownSession) {
      await bridge.stop();
      _ownSession = false;
    } else {
      final device = ref.read(deviceInfoProvider).value;
      final req = ScanRequest(name: 'diagnostics', source: _source, mode: ScanMode.continuous, deviceMotion: false);
      _ownSession = await bridge.start(buildCoreConfig(req, ref.read(settingsProvider), device));
      if (!_ownSession && mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(AppLocalizations.of(context).startFailed)));
      }
    }
    if (mounted) setState(() => _busy = false);
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final rev = ref.watch(revolutionProvider).value;
    final stats = ref.watch(statsProvider).value ?? ScanStats.empty;
    final theme = Theme.of(context);
    final active = stats.running;

    Widget row(String k, String v, {Color? color}) => Padding(
          padding: const EdgeInsets.symmetric(vertical: 3),
          child: Row(children: [
            Expanded(child: Text(k, style: theme.textTheme.bodyMedium?.copyWith(color: theme.colorScheme.onSurfaceVariant))),
            Text(v, style: theme.textTheme.bodyMedium?.copyWith(color: color, fontFeatures: const [FontFeature.tabularFigures()])),
          ]),
        );

    return Scaffold(
      appBar: AppBar(title: Text(l.diagnostics)),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (_attached == true)
            Card(child: ListTile(leading: const Icon(Icons.info_outline), title: Text(l.diagRunningSession)))
          else if (_attached == false)
            Row(children: [
              Expanded(
                child: SegmentedButton<ScanSource>(
                  segments: [
                    ButtonSegment(value: ScanSource.mock, label: Text(l.sourceMock)),
                    ButtonSegment(value: ScanSource.udp, label: const Text('ESP32')),
                  ],
                  selected: {_source},
                  onSelectionChanged: _ownSession ? null : (s) => setState(() => _source = s.first),
                ),
              ),
              const SizedBox(width: 12),
              FilledButton(
                onPressed: _busy ? null : _toggle,
                child: Text(_ownSession ? l.diagStop : l.diagStart),
              ),
            ]),
          const SizedBox(height: 16),
          AspectRatio(
            aspectRatio: 1,
            child: Card(
              color: const Color(0xFF0E1116),
              child: active && rev != null && rev.length > 0
                  ? CustomPaint(painter: _PolarPainter(rev, theme.colorScheme.primary))
                  : Center(child: Text(l.diagWaiting, style: const TextStyle(color: Colors.white70))),
            ),
          ),
          const SizedBox(height: 12),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(children: [
                row(l.hudRpm, stats.rpm.toStringAsFixed(1)),
                row(l.diagPoints, '${rev?.length ?? 0}'),
                row(l.hudRate, formatCount(stats.rawPointsPerSec)),
                row(l.diagDatagrams, stats.datagramsPerSec.toStringAsFixed(1)),
                row(l.hudLoss, '${stats.lostDatagrams} (${stats.lossPct.toStringAsFixed(2)} %)',
                    color: stats.lossPct > 5 ? theme.colorScheme.error : null),
                row(l.diagCrc, '${stats.crcErrors}', color: stats.crcErrors > 0 ? theme.colorScheme.error : null),
                row(l.hudLatency, '${stats.latencyMs.toStringAsFixed(1)} ms'),
                row(l.diagRtt, '${stats.rttMs.toStringAsFixed(1)} ms'),
                row(l.diagDrift, '${stats.driftPpm.toStringAsFixed(1)} ppm'),
                row(l.diagSynced, stats.synced ? l.yes : l.no, color: stats.synced ? Colors.green : theme.colorScheme.error),
                if (stats.source == ScanSource.udp) row(l.diagPeer, stats.peer.isEmpty ? '–' : stats.peer),
              ]),
            ),
          ),
        ],
      ),
    );
  }
}

class _PolarPainter extends CustomPainter {
  _PolarPainter(this.rev, this.accent);
  final Revolution rev;
  final Color accent;

  @override
  void paint(Canvas canvas, Size size) {
    final c = size.center(Offset.zero);
    final radius = size.shortestSide / 2 - 8;
    final dists = [for (var i = 0; i < rev.length; i++) rev.distanceM(i)]..sort();
    final p95 = dists.isEmpty ? 4.0 : dists[(dists.length * 0.95).floor().clamp(0, dists.length - 1)];
    final maxR = math.max(1.0, p95.ceilToDouble());
    final scale = radius / maxR;

    final ring = Paint()
      ..color = Colors.white24
      ..style = PaintingStyle.stroke;
    final text = TextPainter(textDirection: TextDirection.ltr);
    for (var r = 1; r <= maxR; r++) {
      canvas.drawCircle(c, r * scale, ring);
      text.text = TextSpan(text: '$r m', style: const TextStyle(color: Colors.white38, fontSize: 10));
      text.layout();
      text.paint(canvas, c + Offset(3, -r * scale));
    }
    canvas.drawLine(c - Offset(0, radius), c + Offset(0, radius), ring);
    canvas.drawLine(c - Offset(radius, 0), c + Offset(radius, 0), ring);

    // LD06 angles are clockwise; 0° is drawn at the top of the plot.
    final dot = Paint();
    for (var i = 0; i < rev.length; i++) {
      final a = rev.angleDeg(i) * math.pi / 180;
      final d = rev.distanceM(i);
      if (d > maxR * 1.2) continue;
      final p = c + Offset(math.sin(a) * d * scale, -math.cos(a) * d * scale);
      final t = (rev.intensity(i) / 255).clamp(0.0, 1.0);
      dot.color = Color.lerp(const Color(0xFF3949AB), const Color(0xFFFFEB3B), t)!;
      canvas.drawCircle(p, 1.8, dot);
    }
    canvas.drawCircle(c, 4, Paint()..color = accent);
  }

  @override
  bool shouldRepaint(covariant _PolarPainter old) => old.rev != rev;
}
