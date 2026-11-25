import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';

import '../../core/models.dart';
import '../../core/providers.dart';
import '../../core/session_config.dart';
import '../../l10n/app_localizations.dart';
import '../scanner/scanner_screen.dart';

class NewSessionScreen extends ConsumerStatefulWidget {
  const NewSessionScreen({super.key});

  @override
  ConsumerState<NewSessionScreen> createState() => _NewSessionScreenState();
}

class _NewSessionScreenState extends ConsumerState<NewSessionScreen> {
  final _name = TextEditingController();
  ScanSource _source = ScanSource.mock;
  MockTransport _transport = MockTransport.inProcess;
  ScanMode _mode = ScanMode.continuous;
  bool _record = true;
  bool _autoTour = false;
  bool _starting = false;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (_name.text.isEmpty) {
      final date = DateFormat('dd/MM HH:mm').format(DateTime.now());
      _name.text = AppLocalizations.of(context).defaultSessionName(date);
    }
  }

  @override
  void dispose() {
    _name.dispose();
    super.dispose();
  }

  Future<void> _start() async {
    setState(() => _starting = true);
    final now = DateTime.now();
    String? dir;
    if (_record) dir = (await ref.read(sessionsRepositoryProvider).createSessionDir(now)).path;
    final request = ScanRequest(
      name: _name.text.trim(),
      source: _source,
      transport: _transport,
      mode: _mode,
      sessionDir: dir,
      autoTour: _source == ScanSource.mock && _mode == ScanMode.continuous && _autoTour,
    );
    if (!mounted) return;
    await Navigator.of(context).pushReplacement(MaterialPageRoute(builder: (_) => ScannerScreen(request: request)));
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final theme = Theme.of(context);
    Widget section(String title) => Padding(
          padding: const EdgeInsets.fromLTRB(4, 20, 4, 8),
          child: Text(title, style: theme.textTheme.titleSmall?.copyWith(color: theme.colorScheme.primary)),
        );
    return Scaffold(
      appBar: AppBar(title: Text(l.newScan)),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          TextField(
            controller: _name,
            decoration: InputDecoration(labelText: l.sessionName, border: const OutlineInputBorder()),
          ),
          section(l.source),
          RadioGroup<ScanSource>(
            groupValue: _source,
            onChanged: (v) => setState(() => _source = v ?? _source),
            child: Column(children: [
              RadioListTile(value: ScanSource.mock, title: Text(l.sourceMock), subtitle: Text(l.sourceMockDesc)),
              RadioListTile(value: ScanSource.udp, title: Text(l.sourceEsp32), subtitle: Text(l.sourceEsp32Desc)),
            ]),
          ),
          section(l.mode),
          SegmentedButton<ScanMode>(
            segments: [
              ButtonSegment(value: ScanMode.continuous, label: Text(l.modeContinuous), icon: const Icon(Icons.directions_walk)),
              ButtonSegment(value: ScanMode.station, label: Text(l.modeStation), icon: const Icon(Icons.threesixty)),
            ],
            selected: {_mode},
            onSelectionChanged: (s) => setState(() => _mode = s.first),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(4, 8, 4, 0),
            child: Text(_mode == ScanMode.continuous ? l.modeContinuousDesc : l.modeStationDesc,
                style: theme.textTheme.bodySmall),
          ),
          const SizedBox(height: 12),
          if (_source == ScanSource.mock && _mode == ScanMode.continuous)
            SwitchListTile(
              value: _autoTour,
              title: Text(l.autoTour),
              subtitle: Text(l.autoTourDesc),
              secondary: const Icon(Icons.route_outlined),
              onChanged: (v) => setState(() => _autoTour = v),
            ),
          SwitchListTile(
            value: _record,
            title: Text(l.record),
            secondary: const Icon(Icons.fiber_manual_record_outlined),
            onChanged: (v) => setState(() => _record = v),
          ),
          if (_source == ScanSource.mock)
            ExpansionTile(
              title: Text(l.advanced),
              tilePadding: const EdgeInsets.symmetric(horizontal: 4),
              children: [
                RadioGroup<MockTransport>(
                  groupValue: _transport,
                  onChanged: (v) => setState(() => _transport = v ?? _transport),
                  child: Column(children: [
                    RadioListTile(value: MockTransport.inProcess, title: Text(l.transportInProcess)),
                    RadioListTile(value: MockTransport.loopback, title: Text(l.transportLoopback)),
                  ]),
                ),
              ],
            ),
          const SizedBox(height: 24),
          FilledButton.icon(
            onPressed: _starting ? null : _start,
            icon: const Icon(Icons.play_arrow),
            label: Text(l.startScan),
            style: FilledButton.styleFrom(minimumSize: const Size.fromHeight(52)),
          ),
        ],
      ),
    );
  }
}
