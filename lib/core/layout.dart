import 'dart:convert';
import 'dart:math' as math;
import 'dart:ui';

/// Structured model produced by the native layout engine (see layout_engine.h).
///
/// Plan coordinates are (x, z) of the Y-up world frame packed in an [Offset] (dx = x, dy = z);
/// in plan view x points right and z down. The "plan frame" (u, v) is rotated by [RoomLayout.theta]
/// so the dominant walls are axis-aligned: u = x cos θ + z sin θ, v = -x sin θ + z cos θ.
enum OpeningKind { door, window }

enum FurnitureKind { low, table, tall }

/// Planar rigid transform p -> R(a) p + t (rotation about +Y acting on (x, z)).
class PlanTransform {
  const PlanTransform(this.angle, this.tx, this.tz);

  static const identity = PlanTransform(0, 0, 0);

  final double angle, tx, tz;

  Offset rotate(Offset v) {
    final c = math.cos(angle), s = math.sin(angle);
    return Offset(c * v.dx + s * v.dy, -s * v.dx + c * v.dy);
  }

  Offset apply(Offset p) => rotate(p) + Offset(tx, tz);
}

class LayoutOpening {
  const LayoutOpening({required this.kind, required this.t0, required this.t1, required this.bottom, required this.top});

  factory LayoutOpening.fromJson(Map<String, dynamic> j) => LayoutOpening(
        kind: j['k'] == 'window' ? OpeningKind.window : OpeningKind.door,
        t0: _d(j['t0']),
        t1: _d(j['t1']),
        bottom: _d(j['b']),
        top: _d(j['t']),
      );

  final OpeningKind kind;

  /// Along the wall from its start point (m).
  final double t0, t1;

  /// Above the floor (m).
  final double bottom, top;

  double get width => t1 - t0;
}

/// Solid stretch of a wall between [bottom] and [top] (m above the floor), [t0]..[t1] along it.
class WallSpan {
  const WallSpan(this.t0, this.t1, this.bottom, this.top, {this.opening});
  final double t0, t1, bottom, top;

  /// The opening this span frames (above a door, below / above a window); null for full walls.
  final OpeningKind? opening;
}

class LayoutWall {
  const LayoutWall({
    required this.a,
    required this.b,
    required this.thickness,
    required this.height,
    this.joinedA = false,
    this.joinedB = false,
    this.extA = 0,
    this.extB = 0,
    this.faces = 1,
    this.openings = const [],
  });

  factory LayoutWall.fromJson(Map<String, dynamic> j) => LayoutWall(
        a: Offset(_d(j['ax']), _d(j['az'])),
        b: Offset(_d(j['bx']), _d(j['bz'])),
        thickness: _d(j['th'], 0.12),
        height: _d(j['h'], 2.5),
        joinedA: j['ja'] as bool? ?? false,
        joinedB: j['jb'] as bool? ?? false,
        extA: _d(j['ea']),
        extB: _d(j['eb']),
        faces: (j['faces'] as num?)?.toInt() ?? 1,
        openings: [for (final o in (j['op'] as List? ?? const [])) LayoutOpening.fromJson(o as Map<String, dynamic>)],
      );

  /// Centerline end points (x, z).
  final Offset a, b;
  final double thickness, height;
  final bool joinedA, joinedB;

  /// Extra length drawn past each end so corners close.
  final double extA, extB;

  /// 2 when both faces were observed (measured thickness).
  final int faces;
  final List<LayoutOpening> openings;

  double get length => (b - a).distance;
  Offset get direction {
    final l = length;
    return l > 1e-6 ? (b - a) / l : const Offset(1, 0);
  }

  Offset get normal => Offset(-direction.dy, direction.dx);
  Offset pointAt(double t) => a + direction * t;

  /// Solid pieces of the wall, from -[extA] to [length] + [extB], around its openings.
  List<WallSpan> spans() {
    final l = length;
    final ops = [...openings]..sort((x, y) => x.t0.compareTo(y.t0));
    final out = <WallSpan>[];
    var cur = -extA;
    for (final o in ops) {
      final t0 = o.t0.clamp(0.0, l), t1 = o.t1.clamp(0.0, l);
      if (t1 - t0 < 0.05 || t0 < cur) continue;
      if (t0 > cur) out.add(WallSpan(cur, t0, 0, height));
      if (o.kind == OpeningKind.door) {
        if (o.top < height - 0.02) out.add(WallSpan(t0, t1, o.top, height, opening: OpeningKind.door));
      } else {
        if (o.bottom > 0.02) out.add(WallSpan(t0, t1, 0, o.bottom, opening: OpeningKind.window));
        if (o.top < height - 0.02) out.add(WallSpan(t0, t1, o.top, height, opening: OpeningKind.window));
      }
      cur = t1;
    }
    if (l + extB > cur) out.add(WallSpan(cur, l + extB, 0, height));
    return out;
  }
}

class LayoutObject {
  const LayoutObject({
    required this.center,
    required this.width,
    required this.depth,
    required this.height,
    required this.angle,
    required this.kind,
  });

  factory LayoutObject.fromJson(Map<String, dynamic> j) => LayoutObject(
        center: Offset(_d(j['cx']), _d(j['cz'])),
        width: _d(j['w']),
        depth: _d(j['d']),
        height: _d(j['h']),
        angle: _d(j['a']),
        kind: switch (j['k']) {
          'tall' => FurnitureKind.tall,
          'table' => FurnitureKind.table,
          _ => FurnitureKind.low,
        },
      );

  final Offset center;

  /// Width along the plan u axis, depth along v (m).
  final double width, depth, height;
  final double angle;
  final FurnitureKind kind;
}

class LayoutRoom {
  const LayoutRoom({required this.area, required this.label, required this.width, required this.depth, required this.rects});

  factory LayoutRoom.fromJson(Map<String, dynamic> j) => LayoutRoom(
        area: _d(j['area']),
        label: Offset(_d(j['lx']), _d(j['lz'])),
        width: _d(j['w']),
        depth: _d(j['d']),
        rects: [
          for (final r in (j['rects'] as List? ?? const []))
            if (r is List && r.length == 4) Rect.fromLTRB(_d(r[0]), _d(r[1]), _d(r[2]), _d(r[3])),
        ],
      );

  final double area;

  /// Most interior point (x, z).
  final Offset label;

  /// Extent along the plan axes (m).
  final double width, depth;

  /// Floor as rectangles in the plan frame (u, v).
  final List<Rect> rects;

  /// Long and narrow: most likely a corridor.
  bool get isCorridor => math.min(width, depth) < 1.6 && math.max(width, depth) >= 3 * math.min(width, depth);
}

class LayoutStats {
  const LayoutStats({
    this.submaps = 0,
    this.loopEdges = 0,
    this.rejectedEdges = 0,
    this.mergedFaces = 0,
    this.closedCorners = 0,
    this.maxCorrectionM = 0,
    this.maxCorrectionDeg = 0,
  });

  factory LayoutStats.fromJson(Map<String, dynamic>? j) => j == null
      ? const LayoutStats()
      : LayoutStats(
          submaps: (j['submaps'] as num?)?.toInt() ?? 0,
          loopEdges: (j['loopEdges'] as num?)?.toInt() ?? 0,
          rejectedEdges: (j['rejectedEdges'] as num?)?.toInt() ?? 0,
          mergedFaces: (j['mergedFaces'] as num?)?.toInt() ?? 0,
          closedCorners: (j['closedCorners'] as num?)?.toInt() ?? 0,
          maxCorrectionM: _d(j['maxCorrectionM']),
          maxCorrectionDeg: _d(j['maxCorrectionDeg']),
        );

  final int submaps, loopEdges, rejectedEdges, mergedFaces, closedCorners;
  final double maxCorrectionM, maxCorrectionDeg;
}

class RoomLayout {
  const RoomLayout({
    this.valid = false,
    this.floorY = 0,
    this.ceilingY = 2.5,
    this.ceilingEstimated = true,
    this.theta = 0,
    this.walls = const [],
    this.objects = const [],
    this.rooms = const [],
    this.device,
    this.deviceForward,
    this.correction = PlanTransform.identity,
    this.stats = const LayoutStats(),
  });

  factory RoomLayout.fromJson(Map<String, dynamic> j) {
    final dev = j['device'] as Map<String, dynamic>?;
    final corr = j['corr'] as Map<String, dynamic>?;
    return RoomLayout(
      valid: j['valid'] as bool? ?? false,
      floorY: _d(j['floorY']),
      ceilingY: _d(j['ceilingY'], 2.5),
      ceilingEstimated: j['ceilingEstimated'] as bool? ?? true,
      theta: _d(j['theta']),
      walls: [for (final w in (j['walls'] as List? ?? const [])) LayoutWall.fromJson(w as Map<String, dynamic>)],
      objects: [for (final o in (j['objects'] as List? ?? const [])) LayoutObject.fromJson(o as Map<String, dynamic>)],
      rooms: [for (final r in (j['rooms'] as List? ?? const [])) LayoutRoom.fromJson(r as Map<String, dynamic>)],
      device: dev == null ? null : Offset(_d(dev['x']), _d(dev['z'])),
      deviceForward: dev == null ? null : Offset(_d(dev['fx']), _d(dev['fz'])),
      correction: corr == null ? PlanTransform.identity : PlanTransform(_d(corr['a']), _d(corr['tx']), _d(corr['tz'])),
      stats: LayoutStats.fromJson(j['stats'] as Map<String, dynamic>?),
    );
  }

  /// Parses the native JSON; null when it is empty or malformed.
  static RoomLayout? tryParse(String? json) {
    if (json == null || json.trim().isEmpty) return null;
    try {
      return RoomLayout.fromJson(jsonDecode(json) as Map<String, dynamic>);
    } catch (_) {
      return null;
    }
  }

  static const empty = RoomLayout();

  final bool valid;
  final double floorY, ceilingY;
  final bool ceilingEstimated;
  final double theta;
  final List<LayoutWall> walls;
  final List<LayoutObject> objects;
  final List<LayoutRoom> rooms;

  /// Device position and forward direction (x, z) when the layout was computed.
  final Offset? device;
  final Offset? deviceForward;

  /// Maps the odometry world (ScanStats device position) onto this layout.
  final PlanTransform correction;
  final LayoutStats stats;

  double get height => ceilingY - floorY;
  bool get isEmpty => walls.isEmpty && rooms.isEmpty && objects.isEmpty;
  double get totalArea => rooms.fold(0.0, (a, r) => a + r.area);
  int get doorCount => walls.fold(0, (a, w) => a + w.openings.where((o) => o.kind == OpeningKind.door).length);
  int get windowCount => walls.fold(0, (a, w) => a + w.openings.where((o) => o.kind == OpeningKind.window).length);

  /// World (x, z) -> plan frame (u, v).
  Offset toPlan(Offset p) {
    final c = math.cos(theta), s = math.sin(theta);
    return Offset(p.dx * c + p.dy * s, -p.dx * s + p.dy * c);
  }

  /// Plan frame (u, v) -> world (x, z).
  Offset fromPlan(Offset uv) {
    final c = math.cos(theta), s = math.sin(theta);
    return Offset(uv.dx * c - uv.dy * s, uv.dx * s + uv.dy * c);
  }

  /// Bounds of walls and rooms in the plan frame; null when there is nothing to show.
  Rect? planBounds() {
    Rect? r;
    void add(Rect x) => r = r == null ? x : r!.expandToInclude(x);
    for (final w in walls) {
      final pa = toPlan(w.a), pb = toPlan(w.b);
      add(Rect.fromPoints(pa, pb).inflate(w.thickness / 2 + math.max(w.extA, w.extB)));
    }
    for (final room in rooms) {
      for (final rect in room.rects) {
        add(rect);
      }
    }
    for (final o in objects) {
      add(Rect.fromCenter(center: toPlan(o.center), width: o.width, height: o.depth));
    }
    return r;
  }
}

double _d(Object? v, [double fallback = 0]) => (v as num?)?.toDouble() ?? fallback;
