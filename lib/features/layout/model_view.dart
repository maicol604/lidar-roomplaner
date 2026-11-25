import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter/semantics.dart';

import '../../core/format.dart';
import '../../core/layout.dart';
import '../../l10n/app_localizations.dart';

/// Colours of the 3D model "studio" (always dark so the white model stands out in both themes).
class ModelPalette {
  static const backgroundTop = Color(0xFF2B3640);
  static const backgroundBottom = Color(0xFF12171C);
  static const floor = Color(0xFFD5DCE2);
  static const floorShadow = Color(0xCC05080A);
  static const wall = Color(0xFFF4F6F8);
  static const wallTop = Color(0xFFFFFFFF);
  static const wallShade = Color(0xFF8D99A4);
  static const wallEdge = Color(0xFF4F5B66);
  static const furniture = Color(0xFFA7BDCD);
  static const furnitureShade = Color(0xFF52687A);
  static const furnitureEdge = Color(0xFF1F2B35);
  static const door = Color(0x5926A69A);
  static const doorEdge = Color(0xFF80CBC4);
  static const glass = Color(0x669FD4F5);
  static const glassEdge = Color(0xFFD3EEFF);
  static const device = Color(0xFF4DD0E1);
}

/// RoomPlan-style 3D model of a [RoomLayout]: white walls with door and window openings,
/// furniture as boxes and the floor of every room. New walls and furniture rise into place while
/// the scan progresses. Drag to rotate, pinch to zoom, double tap to recenter; the same actions
/// are available as buttons ([showControls]) and as screen-reader actions.
class LayoutModelView extends StatefulWidget {
  const LayoutModelView({
    super.key,
    required this.layout,
    this.device,
    this.deviceForward,
    this.showControls = true,
    this.showFurniture = true,
    this.compact = false,
    this.emptyMessage,
  });

  final RoomLayout? layout;

  /// Current device position / forward direction (world x, z), already corrected.
  final Offset? device, deviceForward;
  final bool showControls;
  final bool showFurniture;

  /// Small live thumbnail: no controls, lighter outlines.
  final bool compact;
  final String? emptyMessage;

  @override
  State<LayoutModelView> createState() => _LayoutModelViewState();
}

class _LayoutModelViewState extends State<LayoutModelView> with SingleTickerProviderStateMixin {
  static const _defaultYaw = -0.62;
  static const _defaultPitch = 0.86;
  static const _minPitch = 0.18, _maxPitch = 1.53;
  static const _rise = Duration(milliseconds: 750);

  final _clock = Stopwatch()..start();
  late final Ticker _ticker = createTicker((_) => _tick());
  double _yaw = _defaultYaw, _pitch = _defaultPitch, _zoom = 1;
  double _yawTarget = _defaultYaw, _pitchTarget = _defaultPitch, _zoomTarget = 1;
  Rect? _frame;
  List<Duration> _wallBirth = const [], _objectBirth = const [];
  double _lastScale = 1, _lastRotation = 0;

  bool get _reduceMotion => MediaQuery.maybeDisableAnimationsOf(context) ?? false;

  @override
  void initState() {
    super.initState();
    _assignBirths(null, widget.layout, initial: true);
  }

  @override
  void didUpdateWidget(covariant LayoutModelView old) {
    super.didUpdateWidget(old);
    if (!identical(old.layout, widget.layout)) {
      _assignBirths(old.layout, widget.layout);
      _kick();
    }
  }

  @override
  void dispose() {
    _ticker.dispose();
    super.dispose();
  }

  /// Walls / objects that already existed keep their birth time; new ones rise now.
  void _assignBirths(RoomLayout? before, RoomLayout? now, {bool initial = false}) {
    final t = _clock.elapsed;
    final settled = t - _rise * 2;
    if (now == null) {
      _wallBirth = const [];
      _objectBirth = const [];
      return;
    }
    final oldWalls = before?.walls ?? const <LayoutWall>[];
    final oldBirth = _wallBirth;
    _wallBirth = [
      for (final w in now.walls) initial ? settled : _matchWall(w, oldWalls, oldBirth) ?? t,
    ];
    final oldObjects = before?.objects ?? const <LayoutObject>[];
    final oldObjBirth = _objectBirth;
    Duration objectBirth(LayoutObject o) {
      for (var i = 0; i < oldObjects.length && i < oldObjBirth.length; i++) {
        if ((oldObjects[i].center - o.center).distance < 0.4) return oldObjBirth[i];
      }
      return t;
    }

    _objectBirth = [for (final o in now.objects) initial ? settled : objectBirth(o)];
  }

  Duration? _matchWall(LayoutWall w, List<LayoutWall> old, List<Duration> births) {
    final d = w.direction, mid = (w.a + w.b) / 2;
    for (var i = 0; i < old.length && i < births.length; i++) {
      final o = old[i];
      final od = o.direction;
      if ((d.dx * od.dx + d.dy * od.dy).abs() < 0.985) continue;
      final rel = mid - o.a;
      final across = (rel.dx * o.normal.dx + rel.dy * o.normal.dy).abs();
      if (across > 0.3) continue;
      double along(Offset p) => (p - o.a).dx * od.dx + (p - o.a).dy * od.dy;
      final a0 = math.min(along(w.a), along(w.b)), a1 = math.max(along(w.a), along(w.b));
      if (math.min(a1, o.length) - math.max(a0, 0) > 0) return births[i];
    }
    return null;
  }

  void _kick() {
    if (!_ticker.isActive) _ticker.start();
  }

  void _tick() {
    if (!mounted) return;
    final target = _targetFrame();
    var busy = false;
    if (target != null) {
      final f = _frame;
      if (f == null || _reduceMotion) {
        _frame = target;
      } else {
        final next = Rect.lerp(f, target, 0.12)!;
        busy = (next.left - target.left).abs() +
                (next.right - target.right).abs() +
                (next.top - target.top).abs() +
                (next.bottom - target.bottom).abs() >
            0.01;
        _frame = busy ? next : target;
      }
    }
    double ease(double v, double t) => _reduceMotion ? t : v + (t - v) * 0.2;
    _yaw = ease(_yaw, _yawTarget);
    _pitch = ease(_pitch, _pitchTarget);
    _zoom = ease(_zoom, _zoomTarget);
    busy = busy ||
        (_yaw - _yawTarget).abs() > 1e-3 ||
        (_pitch - _pitchTarget).abs() > 1e-3 ||
        (_zoom - _zoomTarget).abs() > 1e-3;
    final now = _clock.elapsed;
    if (!_reduceMotion) {
      for (final b in [..._wallBirth, ..._objectBirth]) {
        if (now - b < _rise) busy = true;
      }
    }
    setState(() {});
    if (!busy) _ticker.stop();
  }

  Rect? _targetFrame() {
    final b = widget.layout?.planBounds();
    if (b == null) return null;
    // Keep a minimum size so the first walls do not fill the whole view.
    final w = math.max(b.width, 3.0), h = math.max(b.height, 3.0);
    return Rect.fromCenter(center: b.center, width: w, height: h);
  }

  void _rotate(double delta) {
    _yawTarget += delta;
    _kick();
  }

  void _tilt(double delta) {
    _pitchTarget = (_pitchTarget + delta).clamp(_minPitch, _maxPitch);
    _kick();
  }

  void _zoomBy(double f) {
    _zoomTarget = (_zoomTarget * f).clamp(0.5, 5.0);
    _kick();
  }

  void _reset() {
    _yawTarget = _defaultYaw;
    _pitchTarget = _defaultPitch;
    _zoomTarget = 1;
    _kick();
  }

  void _topView() {
    _pitchTarget = _maxPitch;
    _yawTarget = (_yawTarget / (math.pi / 2)).roundToDouble() * (math.pi / 2);
    _kick();
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final layout = widget.layout;
    final empty = layout == null || layout.isEmpty;
    if (_frame == null && !empty) _frame = _targetFrame();
    final locale = Localizations.localeOf(context).toLanguageTag();
    final summary = empty
        ? (widget.emptyMessage ?? l.modelEmpty)
        : l.modelSummary(layout.walls.length, layout.doorCount, layout.windowCount, layout.objects.length,
            formatArea(layout.totalArea, locale));

    final view = GestureDetector(
      behavior: HitTestBehavior.opaque,
      onDoubleTap: _reset,
      onScaleStart: (d) {
        _lastScale = 1;
        _lastRotation = 0;
      },
      onScaleUpdate: (d) {
        if (d.pointerCount >= 2) {
          final ratio = d.scale / _lastScale;
          _lastScale = d.scale;
          _zoomTarget = (_zoomTarget * ratio).clamp(0.5, 5.0);
          _zoom = _zoomTarget;
          _yawTarget -= d.rotation - _lastRotation;
          _yaw = _yawTarget;
          _lastRotation = d.rotation;
        } else {
          _yawTarget -= d.focalPointDelta.dx * 0.011;
          _pitchTarget = (_pitchTarget + d.focalPointDelta.dy * 0.008).clamp(_minPitch, _maxPitch);
          _yaw = _yawTarget;
          _pitch = _pitchTarget;
        }
        setState(() {});
      },
      child: CustomPaint(
        painter: _ModelPainter(
          layout: empty ? null : layout,
          frame: _frame,
          yaw: _yaw,
          pitch: _pitch,
          zoom: _zoom,
          now: _reduceMotion ? _clock.elapsed + _rise * 4 : _clock.elapsed,
          rise: _rise,
          wallBirth: _wallBirth,
          objectBirth: _objectBirth,
          device: widget.device,
          deviceForward: widget.deviceForward,
          showFurniture: widget.showFurniture,
          compact: widget.compact,
        ),
        child: const SizedBox.expand(),
      ),
    );

    return Semantics(
      container: true,
      label: '${l.modelTitle}. $summary',
      hint: empty ? null : l.modelGestureHint,
      customSemanticsActions: empty
          ? null
          : {
              CustomSemanticsAction(label: l.rotateLeft): () => _rotate(-math.pi / 8),
              CustomSemanticsAction(label: l.rotateRight): () => _rotate(math.pi / 8),
              CustomSemanticsAction(label: l.zoomIn): () => _zoomBy(1.25),
              CustomSemanticsAction(label: l.zoomOut): () => _zoomBy(0.8),
              CustomSemanticsAction(label: l.resetView): _reset,
            },
      child: DecoratedBox(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: [ModelPalette.backgroundTop, ModelPalette.backgroundBottom],
          ),
        ),
        child: Stack(
          children: [
            Positioned.fill(child: ExcludeSemantics(child: view)),
            if (empty)
              Positioned.fill(
                child: IgnorePointer(
                  child: ExcludeSemantics(
                    child: Center(
                      child: SingleChildScrollView(
                        padding: EdgeInsets.all(widget.compact ? 8 : 32),
                        child: Column(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(Icons.view_in_ar_outlined, color: Colors.white70, size: widget.compact ? 24 : 48),
                            SizedBox(height: widget.compact ? 4 : 8),
                            Text(
                              summary,
                              textAlign: TextAlign.center,
                              style: (widget.compact
                                      ? Theme.of(context).textTheme.labelSmall
                                      : Theme.of(context).textTheme.bodyLarge)
                                  ?.copyWith(color: Colors.white),
                            ),
                          ],
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            if (widget.showControls && !empty) ...[
              Positioned(
                left: 8,
                right: 8,
                bottom: 12,
                child: Center(
                  child: _ControlGroup(axis: Axis.horizontal, children: [
                    _ControlButton(Icons.rotate_left, l.rotateLeft, () => _rotate(-math.pi / 8)),
                    _ControlButton(Icons.rotate_right, l.rotateRight, () => _rotate(math.pi / 8)),
                    _ControlButton(Icons.keyboard_arrow_up, l.tiltUp, () => _tilt(0.15)),
                    _ControlButton(Icons.keyboard_arrow_down, l.tiltDown, () => _tilt(-0.15)),
                    _ControlButton(Icons.crop_free, l.topView, _topView),
                  ]),
                ),
              ),
              Positioned(
                right: 12,
                top: 0,
                bottom: 0,
                child: Center(
                  child: _ControlGroup(axis: Axis.vertical, children: [
                    _ControlButton(Icons.add, l.zoomIn, () => _zoomBy(1.25)),
                    _ControlButton(Icons.remove, l.zoomOut, () => _zoomBy(0.8)),
                    _ControlButton(Icons.center_focus_strong_outlined, l.resetView, _reset),
                  ]),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

/// Button alternative to the gestures (WCAG 2.5.1 / 2.5.7): every manipulation works with
/// single taps. 48 dp targets, white icons on a dark translucent pill.
class _ControlButton extends StatelessWidget {
  const _ControlButton(this.icon, this.tooltip, this.onPressed);
  final IconData icon;
  final String tooltip;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) => IconButton(
        style: IconButton.styleFrom(
          foregroundColor: Colors.white,
          hoverColor: Colors.white.withValues(alpha: 0.16),
          focusColor: Colors.white.withValues(alpha: 0.28),
          highlightColor: Colors.white.withValues(alpha: 0.24),
        ),
        tooltip: tooltip,
        icon: Icon(icon),
        onPressed: onPressed,
      );
}

class _ControlGroup extends StatelessWidget {
  const _ControlGroup({required this.axis, required this.children});
  final Axis axis;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) => DecoratedBox(
        decoration: BoxDecoration(
          color: const Color(0xE60E1317),
          borderRadius: BorderRadius.circular(28),
          border: Border.all(color: Colors.white.withValues(alpha: 0.16)),
        ),
        child: Padding(
          padding: const EdgeInsets.all(2),
          child: Flex(direction: axis, mainAxisSize: MainAxisSize.min, children: children),
        ),
      );
}

class _Face {
  _Face(this.points, this.depth, this.fill, this.edge, this.edgeWidth);
  final List<Offset> points;
  final double depth;
  final Color fill;
  final Color edge;
  final double edgeWidth;
}

/// Small perspective camera orbiting the model (plan frame u, v; y up from the floor).
class _Camera {
  _Camera(Size size, Rect frame, double height, double yaw, double pitch, double zoom)
      : center = size.center(Offset.zero),
        tu = frame.center.dx,
        tv = frame.center.dy,
        ty = height * 0.3,
        cy = math.cos(yaw),
        sy = math.sin(yaw),
        cp = math.cos(pitch),
        sp = math.sin(pitch) {
    final r = math.max(
        1.5, math.sqrt(frame.width * frame.width / 4 + frame.height * frame.height / 4 + height * height / 4));
    dist = r * 3.2;
    scale = math.min(size.width, size.height) * 0.5 * (dist - r) / (r * dist) * zoom;
  }

  final Offset center;
  final double tu, tv, ty, cy, sy, cp, sp;
  late final double dist, scale;

  /// Screen position and depth (larger = farther) of a point.
  (Offset, double) project(double u, double y, double v) {
    final rx = u - tu, ry = y - ty, rz = v - tv;
    final x1 = cy * rx + sy * rz, z1 = -sy * rx + cy * rz;
    final yv = ry * cp - z1 * sp;
    final dv = -ry * sp - z1 * cp;
    final k = dist / math.max(0.05 * dist, dist + dv);
    return (Offset(center.dx + scale * x1 * k, center.dy - scale * yv * k), dv);
  }

  /// Whether a face with outward normal (nu, ny, nv) through (u, y, v) faces the camera.
  bool faces(double nu, double ny, double nv, double u, double y, double v) {
    final n1x = cy * nu + sy * nv, n1z = -sy * nu + cy * nv;
    final rx = u - tu, ry = y - ty, rz = v - tv;
    final qx = cy * rx + sy * rz, qz = -sy * rx + cy * rz;
    return n1x * -qx + ny * (dist * sp - ry) + n1z * (dist * cp - qz) > 0;
  }
}

class _ModelPainter extends CustomPainter {
  _ModelPainter({
    required this.layout,
    required this.frame,
    required this.yaw,
    required this.pitch,
    required this.zoom,
    required this.now,
    required this.rise,
    required this.wallBirth,
    required this.objectBirth,
    required this.device,
    required this.deviceForward,
    required this.showFurniture,
    required this.compact,
  });

  final RoomLayout? layout;
  final Rect? frame;
  final double yaw, pitch, zoom;
  final Duration now, rise;
  final List<Duration> wallBirth, objectBirth;
  final Offset? device, deviceForward;
  final bool showFurniture, compact;

  static const _light = (-0.45, 0.80, -0.40);

  double _grow(List<Duration> births, int i) {
    if (i >= births.length) return 1;
    final t = (now - births[i]).inMicroseconds / rise.inMicroseconds;
    if (t >= 1) return 1;
    if (t <= 0) return 0.02;
    return Curves.easeOutCubic.transform(t);
  }

  @override
  void paint(Canvas canvas, Size size) {
    final l = layout, f = frame;
    if (l == null || f == null || size.isEmpty) return;
    final cam = _Camera(size, f, l.height, yaw, pitch, zoom);
    final faces = <_Face>[];
    final lightLen = math.sqrt(_light.$1 * _light.$1 + _light.$2 * _light.$2 + _light.$3 * _light.$3);

    Color shade(Color base, Color dark, double nu, double ny, double nv) {
      final d = (nu * _light.$1 + ny * _light.$2 + nv * _light.$3) / lightLen;
      return Color.lerp(dark, base, 0.58 + 0.42 * d.clamp(0.0, 1.0))!;
    }

    void addFace(List<(double, double, double)> pts, Color fill, Color edge, double edgeW) {
      final proj = [for (final p in pts) cam.project(p.$1, p.$2, p.$3)];
      final depth = proj.fold(0.0, (a, p) => a + p.$2) / proj.length;
      faces.add(_Face([for (final p in proj) p.$1], depth, fill, edge, edgeW));
    }

    // Box with footprint [p0, p1, p2, p3] (plan frame) from y0 to y1, back faces culled.
    void box(List<Offset> fp, double y0, double y1, Color base, Color top, Color dark, Color edge, double edgeW) {
      if (y1 - y0 < 1e-3) return;
      final c = (fp[0] + fp[1] + fp[2] + fp[3]) / 4;
      if (cam.faces(0, 1, 0, c.dx, y1, c.dy)) addFace([for (final p in fp) (p.dx, y1, p.dy)], top, edge, edgeW);
      for (var i = 0; i < 4; i++) {
        final a = fp[i], b = fp[(i + 1) % 4];
        var n = Offset(-(b - a).dy, (b - a).dx);
        final ln = n.distance;
        if (ln < 1e-6) continue;
        n = n / ln;
        final mid = (a + b) / 2;
        if ((mid - c).dx * n.dx + (mid - c).dy * n.dy < 0) n = -n;
        if (!cam.faces(n.dx, 0, n.dy, mid.dx, (y0 + y1) / 2, mid.dy)) continue;
        addFace([(a.dx, y0, a.dy), (b.dx, y0, b.dy), (b.dx, y1, b.dy), (a.dx, y1, a.dy)],
            shade(base, dark, n.dx, 0, n.dy), edge, edgeW);
      }
    }

    final edgeW = compact ? 0.6 : 1.0;
    for (var i = 0; i < l.walls.length; i++) {
      final w = l.walls[i];
      final g = _grow(wallBirth, i);
      final d = l.toPlan(w.direction) - l.toPlan(Offset.zero);
      final n = Offset(-d.dy, d.dx);
      final a = l.toPlan(w.a), h = w.thickness / 2;
      List<Offset> fp(double t0, double t1) =>
          [a + d * t0 + n * h, a + d * t1 + n * h, a + d * t1 - n * h, a + d * t0 - n * h];
      for (final s in w.spans()) {
        box(fp(s.t0, s.t1), s.bottom * g, s.top * g, ModelPalette.wall, ModelPalette.wallTop, ModelPalette.wallShade,
            ModelPalette.wallEdge, edgeW);
      }
      for (final o in w.openings) {
        final p0 = a + d * o.t0, p1 = a + d * o.t1;
        final window = o.kind == OpeningKind.window;
        // Thin two-sided pane in the opening (never culled).
        addFace(
          [
            (p0.dx, (window ? o.bottom : 0) * g, p0.dy),
            (p1.dx, (window ? o.bottom : 0) * g, p1.dy),
            (p1.dx, o.top * g, p1.dy),
            (p0.dx, o.top * g, p0.dy),
          ],
          window ? ModelPalette.glass : ModelPalette.door,
          window ? ModelPalette.glassEdge : ModelPalette.doorEdge,
          1.2,
        );
      }
    }
    if (showFurniture) {
      for (var i = 0; i < l.objects.length; i++) {
        final o = l.objects[i];
        final r = Rect.fromCenter(center: l.toPlan(o.center), width: o.width, height: o.depth);
        final base = switch (o.kind) {
          FurnitureKind.tall => const Color(0xFF94ADBF),
          FurnitureKind.table => const Color(0xFFB3C6D3),
          FurnitureKind.low => ModelPalette.furniture,
        };
        box([r.topLeft, r.topRight, r.bottomRight, r.bottomLeft], 0, o.height * _grow(objectBirth, i), base,
            Color.lerp(base, Colors.white, 0.35)!, ModelPalette.furnitureShade, ModelPalette.furnitureEdge, edgeW);
      }
    }

    // Floor: soft shadow, then the slab (drawn first: everything else stands on it).
    final floorPath = Path();
    for (final room in l.rooms) {
      for (final r in room.rects) {
        floorPath.addPolygon(
            [for (final p in [r.topLeft, r.topRight, r.bottomRight, r.bottomLeft]) cam.project(p.dx, 0, p.dy).$1], true);
      }
    }
    if (l.rooms.isNotEmpty) {
      canvas.drawPath(
        floorPath.shift(Offset(0, compact ? 3 : 8)),
        Paint()
          ..color = ModelPalette.floorShadow
          ..maskFilter = MaskFilter.blur(BlurStyle.normal, compact ? 4 : 12),
      );
      canvas.drawPath(floorPath, Paint()..color = ModelPalette.floor);
      canvas.drawPath(
        floorPath,
        Paint()
          ..color = ModelPalette.floor
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1,
      );
    }

    faces.sort((a, b) => b.depth.compareTo(a.depth));
    final fill = Paint();
    final stroke = Paint()
      ..style = PaintingStyle.stroke
      ..strokeJoin = StrokeJoin.round;
    for (final face in faces) {
      final path = Path()..addPolygon(face.points, true);
      canvas.drawPath(path, fill..color = face.fill);
      canvas.drawPath(
        path,
        stroke
          ..color = face.edge
          ..strokeWidth = face.edgeWidth,
      );
    }

    final dev = device;
    if (dev != null) {
      final p = l.toPlan(dev);
      final c = cam.project(p.dx, 0.02, p.dy).$1;
      final fw = deviceForward;
      if (fw != null && fw.distance > 1e-3) {
        final fd = (l.toPlan(fw) - l.toPlan(Offset.zero)) / fw.distance;
        final side = Offset(-fd.dy, fd.dx);
        final tip = cam.project(p.dx + fd.dx * 0.7, 0.02, p.dy + fd.dy * 0.7).$1;
        final l1 = cam.project(p.dx + side.dx * 0.28, 0.02, p.dy + side.dy * 0.28).$1;
        final r1 = cam.project(p.dx - side.dx * 0.28, 0.02, p.dy - side.dy * 0.28).$1;
        canvas.drawPath(Path()..addPolygon([l1, tip, r1], true),
            Paint()..color = ModelPalette.device.withValues(alpha: 0.45));
      }
      final radius = compact ? 5.0 : 8.0;
      canvas.drawCircle(c, radius + 2.5, Paint()..color = Colors.white);
      canvas.drawCircle(c, radius, Paint()..color = ModelPalette.device);
    }
  }

  @override
  bool shouldRepaint(covariant _ModelPainter old) =>
      !identical(old.layout, layout) ||
      old.frame != frame ||
      old.yaw != yaw ||
      old.pitch != pitch ||
      old.zoom != zoom ||
      old.now != now ||
      old.device != device ||
      old.deviceForward != deviceForward ||
      old.showFurniture != showFurniture;
}
