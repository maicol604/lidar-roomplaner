import 'dart:convert';
import 'dart:io';

import 'package:intl/intl.dart';
import 'package:path_provider/path_provider.dart';

enum SessionFileKind { mcap, ply, pcd, trajectoryCsv, trajectoryJson, layout, floorPlanSvg }

class SessionFile {
  const SessionFile(this.kind, this.file, this.sizeBytes);
  final SessionFileKind kind;
  final File file;
  final int sizeBytes;
}

class SessionInfo {
  SessionInfo({required this.dir, required this.meta, required this.files});

  final Directory dir;

  /// session.json written by the native core when the session ends (null if interrupted).
  final Map<String, dynamic>? meta;
  final List<SessionFile> files;

  String get id => dir.uri.pathSegments.where((s) => s.isNotEmpty).last;
  bool get complete => meta != null && (meta!['exported'] as bool? ?? false);
  String get name => (meta?['name'] as String?)?.trim().isNotEmpty == true ? meta!['name'] as String : id;
  DateTime get createdAt {
    final ms = (meta?['createdAtMs'] as num?)?.toInt();
    if (ms != null) return DateTime.fromMillisecondsSinceEpoch(ms);
    return dir.statSync().modified;
  }

  double get durationS => (meta?['durationS'] as num?)?.toDouble() ?? 0;
  double get distanceM => (meta?['distanceM'] as num?)?.toDouble() ?? 0;
  int get points => (meta?['points'] as num?)?.toInt() ?? 0;
  int get corrections => (meta?['corrections'] as num?)?.toInt() ?? 0;
  int get lostDatagrams => (meta?['lostDatagrams'] as num?)?.toInt() ?? 0;
  int get datagrams => (meta?['datagrams'] as num?)?.toInt() ?? 0;
  double get voxelCm => (meta?['voxelCm'] as num?)?.toDouble() ?? 0;
  String get source => meta?['source'] as String? ?? '';
  String get mode => meta?['mode'] as String? ?? '';
  int get totalBytes => files.fold(0, (a, f) => a + f.sizeBytes);

  /// Floor area of the rooms found by the layout engine (0 if unknown).
  double get areaM2 => (meta?['areaM2'] as num?)?.toDouble() ?? 0;
  int get roomCount => (meta?['rooms'] as num?)?.toInt() ?? 0;

  File get layoutFile => File('${dir.path}/${SessionsRepository.fileName(SessionFileKind.layout)}');
  File get floorPlanSvgFile => File('${dir.path}/${SessionsRepository.fileName(SessionFileKind.floorPlanSvg)}');
}

/// Sessions live in `<app documents>/sessions/<yyyyMMdd_HHmmss>/`.
class SessionsRepository {
  static const _fileNames = {
    SessionFileKind.mcap: 'raw.mcap',
    SessionFileKind.ply: 'cloud.ply',
    SessionFileKind.pcd: 'cloud.pcd',
    SessionFileKind.trajectoryCsv: 'trajectory.csv',
    SessionFileKind.trajectoryJson: 'trajectory.json',
    SessionFileKind.layout: 'layout.json',
    SessionFileKind.floorPlanSvg: 'floorplan.svg',
  };

  static String fileName(SessionFileKind k) => _fileNames[k]!;

  Future<Directory> root() async {
    final docs = await getApplicationDocumentsDirectory();
    final dir = Directory('${docs.path}/sessions');
    if (!dir.existsSync()) await dir.create(recursive: true);
    return dir;
  }

  Future<Directory> createSessionDir(DateTime now) async {
    final r = await root();
    var id = DateFormat('yyyyMMdd_HHmmss').format(now);
    var dir = Directory('${r.path}/$id');
    for (var n = 2; dir.existsSync(); n++) {
      dir = Directory('${r.path}/${id}_$n');
    }
    await dir.create(recursive: true);
    return dir;
  }

  Future<List<SessionInfo>> list() async {
    final r = await root();
    final out = <SessionInfo>[];
    for (final e in r.listSync().whereType<Directory>()) {
      out.add(await load(e));
    }
    out.sort((a, b) => b.createdAt.compareTo(a.createdAt));
    return out;
  }

  Future<SessionInfo> load(Directory dir) async {
    Map<String, dynamic>? meta;
    final f = File('${dir.path}/session.json');
    if (f.existsSync()) {
      try {
        meta = jsonDecode(await f.readAsString()) as Map<String, dynamic>;
      } catch (_) {
        meta = null;
      }
    }
    final files = <SessionFile>[];
    _fileNames.forEach((kind, name) {
      final file = File('${dir.path}/$name');
      if (file.existsSync()) files.add(SessionFile(kind, file, file.lengthSync()));
    });
    return SessionInfo(dir: dir, meta: meta, files: files);
  }

  Future<void> delete(SessionInfo s) => s.dir.delete(recursive: true);
}
