import 'package:flutter/material.dart';

import '../../../core/format.dart';
import '../../../core/models.dart';
import '../../../l10n/app_localizations.dart';

/// Compact, translucent telemetry panel. Items can be toggled in settings.
class Hud extends StatelessWidget {
  const Hud({super.key, required this.stats, required this.items});

  final ScanStats stats;
  final Set<HudItem> items;

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    final s = stats;
    String trackingText() {
      if (s.source == ScanSource.udp) return l.trackingStatic;
      if (s.mode == ScanMode.station && s.stationMoving) return l.trackingLimited;
      return switch (s.tracking) {
        TrackingState.tracking => l.trackingTracking,
        TrackingState.limited => l.trackingLimited,
        TrackingState.lost => l.trackingLost,
        TrackingState.initializing => l.trackingInit,
      };
    }

    Color trackingColor() {
      // Theme-aware status colours with >= 4.5:1 contrast on the HUD panel; the text itself
      // also states the status, so colour is never the only cue (WCAG 1.4.1).
      final dark = Theme.of(context).brightness == Brightness.dark;
      if (s.source == ScanSource.udp) return scheme.onSurfaceVariant;
      return switch (s.tracking) {
        TrackingState.tracking => dark ? const Color(0xFF81C784) : const Color(0xFF2E7D32),
        TrackingState.lost => scheme.error,
        _ => dark ? const Color(0xFFFFD54F) : const Color(0xFF8A6100),
      };
    }

    final tiles = <Widget>[
      if (items.contains(HudItem.points)) _tile(context, Icons.grain, l.hudPoints, formatCount(s.totalPoints)),
      if (items.contains(HudItem.rate)) _tile(context, Icons.speed, l.hudRate, formatCount(s.pointsPerSec)),
      if (items.contains(HudItem.tracking))
        _tile(context, Icons.my_location, l.hudTracking, trackingText(), valueColor: trackingColor()),
      if (items.contains(HudItem.link))
        _tile(context, Icons.wifi, '${l.hudLoss} / ${l.hudLatency}',
            '${s.lossPct.toStringAsFixed(1)}% · ${s.latencyMs.toStringAsFixed(0)} ms',
            valueColor: s.lossPct > 5 ? scheme.error : null),
      if (items.contains(HudItem.rpm)) _tile(context, Icons.rotate_right, l.hudRpm, s.rpm.toStringAsFixed(0)),
      if (items.contains(HudItem.duration)) _tile(context, Icons.timer_outlined, l.hudDuration, formatDuration(s.durationS)),
      if (items.contains(HudItem.distance))
        _tile(context, Icons.straighten, l.hudDistance, formatMeters(s.distanceM)),
      if (items.contains(HudItem.battery))
        _tile(context, s.charging ? Icons.battery_charging_full : Icons.battery_std, l.hudBattery,
            '${s.batteryLevel < 0 ? '–' : '${s.batteryLevel}%'} · ${s.batteryTempC.toStringAsFixed(0)} °C',
            valueColor: s.batteryTempC >= 42 || (s.batteryLevel >= 0 && s.batteryLevel < 15) ? scheme.error : null),
    ];
    if (tiles.isEmpty) return const SizedBox.shrink();
    return DecoratedBox(
      decoration: BoxDecoration(
        color: scheme.surface.withValues(alpha: 0.9),
        borderRadius: BorderRadius.circular(14),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
        child: Wrap(spacing: 14, runSpacing: 6, children: tiles),
      ),
    );
  }

  Widget _tile(BuildContext context, IconData icon, String label, String value, {Color? valueColor}) {
    final t = Theme.of(context).textTheme;
    final scheme = Theme.of(context).colorScheme;
    return SizedBox(
      width: 104,
      child: Row(
        children: [
          Icon(icon, size: 16, color: scheme.primary),
          const SizedBox(width: 6),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(label, style: t.labelSmall?.copyWith(color: scheme.onSurfaceVariant), maxLines: 1, overflow: TextOverflow.ellipsis),
                Text(value,
                    style: t.labelLarge?.copyWith(fontFeatures: const [FontFeature.tabularFigures()], color: valueColor),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
