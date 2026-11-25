import 'package:flutter/material.dart';

import '../../../core/models.dart';
import '../../../l10n/app_localizations.dart';

enum AlertLevel { warning, critical }

class ScanAlert {
  const ScanAlert(this.level, this.icon, this.message);
  final AlertLevel level;
  final IconData icon;
  final String message;
}

/// Alerts that affect scan quality; always shown even when the HUD is hidden. With
/// [userMotion] false (automatic tour) advice about how to move is omitted: nobody can act on it.
List<ScanAlert> computeAlerts(ScanStats s, AppLocalizations l, {bool userMotion = true}) {
  if (!s.running || s.paused) return const [];
  final simulator = s.source == ScanSource.mock;
  return [
    if (s.alertNoData) ScanAlert(AlertLevel.critical, Icons.sensors_off, l.alertNoData),
    if (s.alertTracking && simulator && !s.motionReady)
      ScanAlert(AlertLevel.warning, Icons.screen_search_desktop_outlined, l.alertMotionInit)
    else if (s.alertTracking)
      ScanAlert(AlertLevel.critical, Icons.location_disabled, _trackingMessage(s.motionFailure, l)),
    if (s.alertStationDisplaced) ScanAlert(AlertLevel.critical, Icons.do_not_step, l.alertStationDisplaced),
    if (simulator && s.placement == MockPlacement.outside)
      ScanAlert(AlertLevel.warning, Icons.wrong_location_outlined, l.alertOutside),
    if (simulator && s.placement == MockPlacement.insideObstacle)
      ScanAlert(AlertLevel.warning, Icons.fence, l.alertInsideWall),
    if (s.alertSpeed && userMotion) ScanAlert(AlertLevel.warning, Icons.speed, l.alertSpeed(s.gapCm.toStringAsFixed(1))),
    if (s.alertTilt && userMotion)
      ScanAlert(AlertLevel.warning, Icons.screen_rotation_alt, l.alertTilt(s.tiltDeg.toStringAsFixed(0))),
    if (s.alertThermal) ScanAlert(AlertLevel.warning, Icons.thermostat, l.alertThermal),
  ];
}

String _trackingMessage(String? failure, AppLocalizations l) {
  final reason = switch (failure) {
    'insufficient_light' => l.motionFailLight,
    'excessive_motion' => l.motionFailMotion,
    'insufficient_features' => l.motionFailFeatures,
    'camera_unavailable' => l.motionFailCamera,
    _ => null,
  };
  return reason == null ? l.alertTracking : '${l.alertTracking} · $reason';
}

class AlertsBanner extends StatelessWidget {
  const AlertsBanner({super.key, required this.alerts});
  final List<ScanAlert> alerts;

  // Opaque, so white text keeps >= 4.5:1 contrast over any point cloud (WCAG 1.4.3).
  static const _critical = Color(0xFFB71C1C);
  static const _warning = Color(0xFFA34700);

  @override
  Widget build(BuildContext context) {
    return AnimatedSize(
      duration: MediaQuery.disableAnimationsOf(context) ? Duration.zero : const Duration(milliseconds: 200),
      alignment: Alignment.topCenter,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          for (final a in alerts)
            Padding(
              padding: const EdgeInsets.only(bottom: 6),
              // Status messages are announced by screen readers without moving focus (WCAG 4.1.3).
              child: Semantics(
                liveRegion: true,
                container: true,
                child: Material(
                  color: a.level == AlertLevel.critical ? _critical : _warning,
                  borderRadius: BorderRadius.circular(12),
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                    child: Row(
                      children: [
                        Icon(a.icon, color: Colors.white, size: 20),
                        const SizedBox(width: 10),
                        Expanded(
                          child: Text(a.message,
                              style: Theme.of(context)
                                  .textTheme
                                  .bodyMedium
                                  ?.copyWith(color: Colors.white, fontWeight: FontWeight.w600)),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }
}
