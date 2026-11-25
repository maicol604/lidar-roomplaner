import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';
import 'package:share_plus/share_plus.dart';

import '../../core/format.dart';
import '../../core/providers.dart';
import '../../core/sessions.dart';
import '../../l10n/app_localizations.dart';
import '../viewer/cloud_viewer_screen.dart';

class SessionDetailScreen extends ConsumerStatefulWidget {
  const SessionDetailScreen({super.key, required this.session, this.fromViewer = false});
  final SessionInfo session;

  /// Opened from the 3D viewer: hides the "view" button and pops with `true` when deleted.
  final bool fromViewer;

  @override
  ConsumerState<SessionDetailScreen> createState() => _SessionDetailScreenState();
}

class _SessionDetailScreenState extends ConsumerState<SessionDetailScreen> {
  late final Set<SessionFileKind> _selected = {
    for (final f in widget.session.files)
      if (f.kind != SessionFileKind.pcd && f.kind != SessionFileKind.trajectoryJson) f.kind,
  };
  bool _sharing = false;

  String _fileLabel(AppLocalizations l, SessionFileKind k) => switch (k) {
        SessionFileKind.mcap => l.fileMcap,
        SessionFileKind.ply => l.filePly,
        SessionFileKind.pcd => l.filePcd,
        SessionFileKind.trajectoryCsv => l.fileCsv,
        SessionFileKind.trajectoryJson => l.fileJson,
        SessionFileKind.layout => l.fileLayout,
        SessionFileKind.floorPlanSvg => l.fileFloorPlan,
      };

  IconData _fileIcon(SessionFileKind k) => switch (k) {
        SessionFileKind.mcap => Icons.inventory_2_outlined,
        SessionFileKind.ply || SessionFileKind.pcd => Icons.grain,
        SessionFileKind.layout => Icons.view_in_ar_outlined,
        SessionFileKind.floorPlanSvg => Icons.square_foot,
        _ => Icons.timeline,
      };

  Future<void> _share() async {
    final l = AppLocalizations.of(context);
    final files = widget.session.files.where((f) => _selected.contains(f.kind)).toList();
    if (files.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(l.shareNothing)));
      return;
    }
    setState(() => _sharing = true);
    try {
      await SharePlus.instance.share(ShareParams(
        files: [for (final f in files) XFile(f.file.path)],
        subject: widget.session.name,
        text: widget.session.name,
      ));
    } finally {
      if (mounted) setState(() => _sharing = false);
    }
  }

  Future<void> _delete() async {
    final l = AppLocalizations.of(context);
    final ok = await showDialog<bool>(
      context: context,
      builder: (c) => AlertDialog(
        content: Text(l.deleteConfirm),
        actions: [
          TextButton(onPressed: () => Navigator.pop(c, false), child: Text(l.cancel)),
          FilledButton(onPressed: () => Navigator.pop(c, true), child: Text(l.delete)),
        ],
      ),
    );
    if (ok != true) return;
    await ref.read(sessionsProvider.notifier).delete(widget.session);
    if (mounted) Navigator.of(context).pop(true);
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final s = widget.session;
    final theme = Theme.of(context);
    final date = DateFormat.yMMMMd(Localizations.localeOf(context).toLanguageTag()).add_Hm().format(s.createdAt);
    Widget stat(String label, String value) => Padding(
          padding: const EdgeInsets.symmetric(vertical: 4),
          child: Row(children: [
            Expanded(child: Text(label, style: theme.textTheme.bodyMedium?.copyWith(color: theme.colorScheme.onSurfaceVariant))),
            Text(value, style: theme.textTheme.bodyLarge),
          ]),
        );
    return Scaffold(
      appBar: AppBar(
        title: Text(s.name, maxLines: 1, overflow: TextOverflow.ellipsis),
        actions: [IconButton(tooltip: l.delete, icon: const Icon(Icons.delete_outline), onPressed: _delete)],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _sharing ? null : _share,
        icon: _sharing
            ? const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2))
            : const Icon(Icons.share),
        label: Text(l.share),
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 8, 16, 96),
        children: [
          Text(date, style: theme.textTheme.bodySmall),
          if (!widget.fromViewer && CloudViewerScreen.canView(s))
            Padding(
              padding: const EdgeInsets.only(top: 12),
              child: FilledButton.icon(
                style: FilledButton.styleFrom(minimumSize: const Size.fromHeight(48)),
                icon: const Icon(Icons.view_in_ar),
                label: Text(l.viewResult),
                onPressed: () => Navigator.of(context)
                    .pushReplacement(MaterialPageRoute(builder: (_) => CloudViewerScreen(session: s))),
              ),
            ),
          if (!s.complete)
            Card(
              color: theme.colorScheme.errorContainer,
              child: ListTile(leading: const Icon(Icons.warning_amber), title: Text(l.incompleteSession)),
            ),
          const SizedBox(height: 8),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(children: [
                stat(l.summarySource, s.source == 'udp' ? l.sourceEsp32 : l.sourceMock),
                stat(l.summaryMode, s.mode == 'station' ? l.modeStation : l.modeContinuous),
                stat(l.summaryDuration, formatDuration(s.durationS)),
                if (s.areaM2 > 0)
                  stat(l.summaryArea, formatArea(s.areaM2, Localizations.localeOf(context).toLanguageTag())),
                if (s.roomCount > 0) stat(l.summaryRooms, '${s.roomCount}'),
                stat(l.summaryPoints, formatCount(s.points)),
                stat(l.summaryDistance, formatMeters(s.distanceM)),
                stat(l.summaryVoxel, '${s.voxelCm.toStringAsFixed(1)} cm'),
                stat(l.summaryCorrections, '${s.corrections}'),
                stat(l.summaryLoss, '${s.lostDatagrams} / ${s.datagrams + s.lostDatagrams}'),
              ]),
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(4, 16, 4, 8),
            child: Text(l.files, style: theme.textTheme.titleSmall?.copyWith(color: theme.colorScheme.primary)),
          ),
          Card(
            child: Column(children: [
              for (final f in s.files)
                CheckboxListTile(
                  value: _selected.contains(f.kind),
                  onChanged: (v) => setState(() => v == true ? _selected.add(f.kind) : _selected.remove(f.kind)),
                  secondary: Icon(_fileIcon(f.kind)),
                  title: Text(_fileLabel(l, f.kind)),
                  subtitle: Text('${f.file.uri.pathSegments.last} · ${formatBytes(f.sizeBytes)}'),
                ),
            ]),
          ),
        ],
      ),
    );
  }
}
