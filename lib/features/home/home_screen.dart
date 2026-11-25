import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';

import '../../core/format.dart';
import '../../core/providers.dart';
import '../../core/sessions.dart';
import '../../l10n/app_localizations.dart';
import '../diagnostics/diagnostics_screen.dart';
import '../new_session/new_session_screen.dart';
import '../settings/settings_screen.dart';
import '../viewer/cloud_viewer_screen.dart';

class HomeScreen extends ConsumerWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l = AppLocalizations.of(context);
    final sessions = ref.watch(sessionsProvider);
    final device = ref.watch(deviceInfoProvider).value;
    return Scaffold(
      appBar: AppBar(
        title: Text(l.homeTitle),
        actions: [
          IconButton(
            tooltip: l.diagnostics,
            icon: const Icon(Icons.radar),
            onPressed: () => Navigator.of(context).push(MaterialPageRoute(builder: (_) => const DiagnosticsScreen())),
          ),
          IconButton(
            tooltip: l.settings,
            icon: const Icon(Icons.settings_outlined),
            onPressed: () => Navigator.of(context).push(MaterialPageRoute(builder: (_) => const SettingsScreen())),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        icon: const Icon(Icons.view_in_ar),
        label: Text(l.newScan),
        onPressed: () async {
          await Navigator.of(context).push(MaterialPageRoute(builder: (_) => const NewSessionScreen()));
          ref.read(sessionsProvider.notifier).refresh();
        },
      ),
      body: Column(
        children: [
          if (device != null)
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 4, 16, 0),
              child: Row(
                children: [
                  Icon(Icons.phone_android, size: 16, color: Theme.of(context).colorScheme.outline),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      device.lowEnd ? '${l.deviceInfoLine(device.model, '${device.totalRamMb}')} · ${l.lowEndNotice}'
                          : l.deviceInfoLine(device.model, '${device.totalRamMb}'),
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ),
          Expanded(
            child: sessions.when(
              loading: () => const Center(child: CircularProgressIndicator()),
              error: (e, _) => Center(child: Text('$e')),
              data: (list) => list.isEmpty
                  ? Center(
                      child: Padding(
                        padding: const EdgeInsets.all(32),
                        child: Column(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(Icons.threed_rotation, size: 64, color: Theme.of(context).colorScheme.outline),
                            const SizedBox(height: 16),
                            Text(l.noSessions, textAlign: TextAlign.center),
                          ],
                        ),
                      ),
                    )
                  : RefreshIndicator(
                      onRefresh: () => ref.read(sessionsProvider.notifier).refresh(),
                      child: ListView.builder(
                        padding: const EdgeInsets.fromLTRB(12, 8, 12, 96),
                        itemCount: list.length,
                        itemBuilder: (c, i) => _SessionCard(session: list[i]),
                      ),
                    ),
            ),
          ),
        ],
      ),
    );
  }
}

class _SessionCard extends ConsumerWidget {
  const _SessionCard({required this.session});
  final SessionInfo session;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final date = DateFormat.yMMMd(Localizations.localeOf(context).toLanguageTag()).add_Hm().format(session.createdAt);
    final locale = Localizations.localeOf(context).toLanguageTag();
    final area = session.areaM2 > 0 ? ' · ${formatArea(session.areaM2, locale)}' : '';
    return Card(
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: session.complete ? theme.colorScheme.primaryContainer : theme.colorScheme.errorContainer,
          foregroundColor: session.complete ? theme.colorScheme.onPrimaryContainer : theme.colorScheme.onErrorContainer,
          child: Icon(session.source == 'udp' ? Icons.sensors : Icons.science_outlined),
        ),
        title: Text(session.name, maxLines: 1, overflow: TextOverflow.ellipsis),
        subtitle: Text(
          session.complete
              ? '$date\n${formatDuration(session.durationS)} · ${formatCount(session.points)} pts$area · '
                  '${formatMeters(session.distanceM)} · ${formatBytes(session.totalBytes)}'
              : '$date\n${l.incompleteSession} · ${formatBytes(session.totalBytes)}',
        ),
        isThreeLine: true,
        trailing: const Icon(Icons.chevron_right),
        onTap: () async {
          await openSession(context, session);
          ref.read(sessionsProvider.notifier).refresh();
        },
      ),
    );
  }
}
