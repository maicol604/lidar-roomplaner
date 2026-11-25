import 'dart:math' as math;
import 'dart:ui';

import 'layout.dart';

/// Door symbol in the plan frame: leaf from [hinge] to [leafEnd], swing arc around [hinge] from
/// [arcStart] (radians, screen convention: x right, y down) sweeping [arcSweep].
class DoorSymbol {
  const DoorSymbol(this.hinge, this.leafEnd, this.radius, this.arcStart, this.arcSweep);
  final Offset hinge, leafEnd;
  final double radius, arcStart, arcSweep;
}

/// Dimension of a wall in the plan frame: for exterior walls a line parallel to the wall and
/// offset away from the building; for interior walls just a label beside the wall.
class DimensionLine {
  const DimensionLine(this.p0, this.p1, this.tick, this.angle, this.length, {this.interior = false});
  final Offset p0, p1;

  /// Unit vector perpendicular to the line, pointing away from the wall.
  final Offset tick;

  /// Text angle (radians), kept upright.
  final double angle;
  final double length;
  final bool interior;
  Offset get mid => (p0 + p1) / 2;
}

/// Floor-plan geometry in the plan frame (u right, v down, metres), shared by the on-screen plan
/// and the SVG export so both draw exactly the same thing.
class PlanGeometry {
  PlanGeometry(this.layout) : centroid = _centroid(layout);

  final RoomLayout layout;

  /// Centre of the scanned area (plan frame): "inside" for door swings and dimension sides.
  final Offset centroid;

  static Offset _centroid(RoomLayout l) {
    var sum = Offset.zero;
    var w = 0.0;
    for (final r in l.rooms) {
      for (final rect in r.rects) {
        final a = rect.width * rect.height;
        sum += rect.center * a;
        w += a;
      }
    }
    if (w > 0) return sum / w;
    final b = l.planBounds();
    return b?.center ?? Offset.zero;
  }

  Offset p(Offset world) => layout.toPlan(world);
  Offset dir(LayoutWall w) => layout.toPlan(w.direction) - layout.toPlan(Offset.zero);
  Offset normal(LayoutWall w) {
    final d = dir(w);
    return Offset(-d.dy, d.dx);
  }

  /// Corners of the wall stretch [t0, t1] across its full thickness.
  List<Offset> quad(LayoutWall w, double t0, double t1, {double? halfWidth}) {
    final d = dir(w), n = normal(w), a = p(w.a);
    final h = halfWidth ?? w.thickness / 2;
    return [a + d * t0 + n * h, a + d * t1 + n * h, a + d * t1 - n * h, a + d * t0 - n * h];
  }

  /// Solid full-height stretches (what a plan shows as wall).
  List<WallSpan> solidSpans(LayoutWall w) =>
      w.spans().where((s) => s.opening == null && s.bottom <= 0.01).toList(growable: false);

  /// +1 / -1: side of the wall normal facing the building centre.
  double insideSign(LayoutWall w) {
    final mid = p((w.a + w.b) / 2);
    final s = (centroid - mid).dx * normal(w).dx + (centroid - mid).dy * normal(w).dy;
    return s >= 0 ? 1 : -1;
  }

  bool _inRoom(Offset planPoint) {
    for (final r in layout.rooms) {
      for (final rect in r.rects) {
        if (rect.contains(planPoint)) return true;
      }
    }
    return false;
  }

  /// Side of the normal with no room next to the wall (+1 / -1), or 0 for an interior wall.
  /// Exterior walls get full dimension lines outside; interior ones only a compact label.
  int outsideSign(LayoutWall w) {
    if (layout.rooms.isEmpty) return -insideSign(w).toInt();
    final n = normal(w), d = dir(w), a = p(w.a);
    var plus = 0, minus = 0;
    for (final f in const [0.25, 0.5, 0.75]) {
      final c = a + d * (w.length * f);
      final off = n * (w.thickness / 2 + 0.3);
      if (_inRoom(c + off)) plus++;
      if (_inRoom(c - off)) minus++;
    }
    if (plus == 0 && minus > 0) return 1;
    if (minus == 0 && plus > 0) return -1;
    return 0;
  }

  DoorSymbol door(LayoutWall w, LayoutOpening o) {
    final d = dir(w), n = normal(w) * insideSign(w), a = p(w.a);
    final hinge = a + d * o.t0;
    final width = o.width;
    final leafEnd = hinge + n * width;
    final leafAngle = math.atan2(n.dy, n.dx);
    final closedAngle = math.atan2(d.dy, d.dx);
    var sweep = closedAngle - leafAngle;
    while (sweep > math.pi) {
      sweep -= 2 * math.pi;
    }
    while (sweep < -math.pi) {
      sweep += 2 * math.pi;
    }
    return DoorSymbol(hinge, leafEnd, width, leafAngle, sweep);
  }

  /// Dimension of the wall's centerline length. Exterior walls: 0.35 m outside their outer
  /// face; interior walls (|DimensionLine.interior|): along the wall itself, label only.
  DimensionLine dimension(LayoutWall w) {
    final side = outsideSign(w);
    final d = dir(w), a = p(w.a), b = p(w.b);
    final out = normal(w) * (side == 0 ? -insideSign(w) : side.toDouble());
    final off = side == 0 ? out * (w.thickness / 2) : out * (w.thickness / 2 + 0.35);
    var angle = math.atan2(d.dy, d.dx);
    if (angle > math.pi / 2) angle -= math.pi;
    if (angle <= -math.pi / 2) angle += math.pi;
    return DimensionLine(a + off, b + off, out, angle, w.length, interior: side == 0);
  }
}
