import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../../core/format.dart';
import '../../core/layout.dart';
import '../../core/plan_geometry.dart';
import '../../l10n/app_localizations.dart';

/// Localized names of the rooms of a layout ("Estancia 1", "Pasillo"...), in layout order.
List<String> roomNames(AppLocalizations l, RoomLayout layout) {
  var rooms = 0, corridors = 0;
  return [
    for (final r in layout.rooms)
      if (r.isCorridor)
        (++corridors == 1 ? l.roomCorridor : '${l.roomCorridor} $corridors')
      else
        l.roomNumbered(++rooms),
  ];
}

/// Plan colours with WCAG AA contrast against the paper (text >= 4.5:1, lines >= 3:1).
class PlanPalette {
  const PlanPalette({required this.paper, required this.ink, required this.muted, required this.roomFill});

  factory PlanPalette.of(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    final primary = Theme.of(context).colorScheme.primary;
    return dark
        ? PlanPalette(
            paper: const Color(0xFF121518),
            ink: const Color(0xFFE8EBEE),
            muted: const Color(0xFFAAB4BD),
            roomFill: primary.withValues(alpha: 0.14))
        : PlanPalette(
            paper: Colors.white,
            ink: const Color(0xFF1B1F23),
            muted: const Color(0xFF4A5661),
            roomFill: primary.withValues(alpha: 0.08));
  }

  final Color paper, ink, muted, roomFill;
}

/// Clean, measurable floor plan: walls, door swings, windows, rooms with their area and
/// approximate dimensions; no furniture. Drag to pan, pinch to zoom, double tap to fit, or use
/// the zoom buttons.
class FloorPlanView extends StatefulWidget {
  const FloorPlanView({super.key, required this.layout, required this.roomNames, this.showDimensions = true});

  final RoomLayout layout;
  final List<String> roomNames;
  final bool showDimensions;

  @override
  State<FloorPlanView> createState() => _FloorPlanViewState();
}

class _FloorPlanViewState extends State<FloorPlanView> {
  double _scale = 0;
  Offset _offset = Offset.zero;
  Size _size = Size.zero;
  bool _userMoved = false;
  double _startScale = 1;
  Offset _startFocal = Offset.zero, _startOffset = Offset.zero;

  @override
  void didUpdateWidget(covariant FloorPlanView old) {
    super.didUpdateWidget(old);
    if (!identical(old.layout, widget.layout) && !_userMoved && !_size.isEmpty) _fit();
  }

  double get _fitScale {
    final b = (widget.layout.planBounds() ?? Rect.zero).inflate(0.8);
    if (b.isEmpty || _size.isEmpty) return 40;
    return math.min(_size.width / b.width, _size.height / b.height);
  }

  void _fit() {
    final b = (widget.layout.planBounds() ?? Rect.zero).inflate(0.8);
    _scale = _fitScale;
    _offset = _size.center(Offset.zero) - b.center * _scale;
    _userMoved = false;
  }

  void _zoomAround(Offset focal, double factor) {
    final s = (_scale * factor).clamp(_fitScale * 0.5, _fitScale * 12);
    final planPoint = (focal - _offset) / _scale;
    setState(() {
      _scale = s;
      _offset = focal - planPoint * s;
      _userMoved = true;
    });
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final palette = PlanPalette.of(context);
    final locale = Localizations.localeOf(context).toLanguageTag();
    return LayoutBuilder(builder: (context, c) {
      final size = Size(c.maxWidth, c.maxHeight);
      if (size != _size) {
        _size = size;
        if (!_userMoved || _scale == 0) _fit();
      }
      return Stack(
        children: [
          Positioned.fill(
            child: GestureDetector(
              behavior: HitTestBehavior.opaque,
              onDoubleTap: () => setState(_fit),
              onScaleStart: (d) {
                _startScale = _scale;
                _startFocal = d.localFocalPoint;
                _startOffset = _offset;
              },
              onScaleUpdate: (d) {
                final s = (_startScale * d.scale).clamp(_fitScale * 0.5, _fitScale * 12);
                final planPoint = (_startFocal - _startOffset) / _startScale;
                setState(() {
                  _scale = s;
                  _offset = d.localFocalPoint - planPoint * s;
                  _userMoved = true;
                });
              },
              child: ExcludeSemantics(
                child: CustomPaint(
                  painter: _PlanPainter(
                    layout: widget.layout,
                    names: widget.roomNames,
                    scale: _scale,
                    offset: _offset,
                    palette: palette,
                    textScaler: MediaQuery.textScalerOf(context),
                    baseStyle: DefaultTextStyle.of(context).style,
                    locale: locale,
                    showDimensions: widget.showDimensions,
                  ),
                  child: const SizedBox.expand(),
                ),
              ),
            ),
          ),
          Positioned(left: 12, bottom: 12, child: _ScaleBar(scale: _scale, palette: palette)),
          Positioned(
            right: 8,
            bottom: 8,
            child: Material(
              color: palette.paper.withValues(alpha: 0.94),
              elevation: 2,
              shape: StadiumBorder(side: BorderSide(color: palette.muted.withValues(alpha: 0.4))),
              child: Padding(
                padding: const EdgeInsets.all(2),
                child: Row(mainAxisSize: MainAxisSize.min, children: [
                  IconButton(
                    tooltip: l.zoomOut,
                    color: palette.ink,
                    icon: const Icon(Icons.remove),
                    onPressed: () => _zoomAround(_size.center(Offset.zero), 0.8),
                  ),
                  IconButton(
                    tooltip: l.fitPlan,
                    color: palette.ink,
                    icon: const Icon(Icons.fit_screen_outlined),
                    onPressed: () => setState(_fit),
                  ),
                  IconButton(
                    tooltip: l.zoomIn,
                    color: palette.ink,
                    icon: const Icon(Icons.add),
                    onPressed: () => _zoomAround(_size.center(Offset.zero), 1.25),
                  ),
                ]),
              ),
            ),
          ),
        ],
      );
    });
  }
}

class _ScaleBar extends StatelessWidget {
  const _ScaleBar({required this.scale, required this.palette});
  final double scale;
  final PlanPalette palette;

  @override
  Widget build(BuildContext context) {
    if (scale <= 0) return const SizedBox.shrink();
    final locale = Localizations.localeOf(context).toLanguageTag();
    const options = [0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0];
    final meters = options.firstWhere((m) => m * scale >= 56, orElse: () => options.last);
    final px = meters * scale;
    final label = meters < 1
        ? formatLength(meters, locale)
        : formatLength(meters, locale, decimals: 0);
    return Semantics(
      label: AppLocalizations.of(context).scaleBar(label),
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          color: palette.paper.withValues(alpha: 0.94),
          borderRadius: BorderRadius.circular(10),
          border: Border.all(color: palette.muted.withValues(alpha: 0.4)),
        ),
        child: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.start, children: [
          Text(label, style: Theme.of(context).textTheme.labelMedium?.copyWith(color: palette.ink)),
          const SizedBox(height: 2),
          CustomPaint(size: Size(px, 8), painter: _ScaleBarPainter(palette.ink)),
        ]),
      ),
    );
  }
}

class _ScaleBarPainter extends CustomPainter {
  _ScaleBarPainter(this.color);
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final p = Paint()
      ..color = color
      ..strokeWidth = 2;
    canvas.drawLine(Offset(0, size.height / 2), Offset(size.width, size.height / 2), p);
    canvas.drawLine(Offset.zero, Offset(0, size.height), p);
    canvas.drawLine(Offset(size.width, 0), Offset(size.width, size.height), p);
  }

  @override
  bool shouldRepaint(covariant _ScaleBarPainter old) => old.color != color;
}

class _PlanPainter extends CustomPainter {
  _PlanPainter({
    required this.layout,
    required this.names,
    required this.scale,
    required this.offset,
    required this.palette,
    required this.textScaler,
    required this.baseStyle,
    required this.locale,
    required this.showDimensions,
  }) : geo = PlanGeometry(layout);

  final RoomLayout layout;
  final List<String> names;
  final double scale;
  final Offset offset;
  final PlanPalette palette;
  final TextScaler textScaler;
  final TextStyle baseStyle;
  final String locale;
  final bool showDimensions;
  final PlanGeometry geo;

  Offset _s(Offset plan) => offset + plan * scale;

  TextPainter _text(String s, double size, {FontWeight weight = FontWeight.w400, Color? color}) => TextPainter(
        text: TextSpan(
          text: s,
          style: baseStyle.copyWith(fontSize: size, fontWeight: weight, color: color ?? palette.ink, height: 1.15),
        ),
        textDirection: TextDirection.ltr,
        textScaler: textScaler,
      )..layout();

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(Offset.zero & size, Paint()..color = palette.paper);
    if (scale <= 0) return;

    // Rooms.
    final roomPath = Path();
    for (final r in layout.rooms) {
      for (final rect in r.rects) {
        roomPath.addRect(Rect.fromPoints(_s(rect.topLeft), _s(rect.bottomRight)));
      }
    }
    canvas.drawPath(roomPath, Paint()..color = palette.roomFill);

    // Walls.
    final wallPaint = Paint()..color = palette.ink;
    for (final w in layout.walls) {
      for (final s in geo.solidSpans(w)) {
        canvas.drawPath(Path()..addPolygon([for (final p in geo.quad(w, s.t0, s.t1)) _s(p)], true), wallPaint);
      }
    }

    // Openings.
    final line = Paint()
      ..color = palette.ink
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.4;
    for (final w in layout.walls) {
      for (final o in w.openings) {
        if (o.kind == OpeningKind.window) {
          final q = [for (final p in geo.quad(w, o.t0, o.t1)) _s(p)];
          final c = [for (final p in geo.quad(w, o.t0, o.t1, halfWidth: 0)) _s(p)];
          final path = Path()..addPolygon(q, true);
          canvas.drawPath(path, Paint()..color = palette.paper);
          canvas.drawPath(path, line);
          canvas.drawLine(c[0], c[1], line);
        } else {
          final d = geo.door(w, o);
          canvas.drawLine(_s(d.hinge), _s(d.leafEnd), line..strokeWidth = 2.2);
          line.strokeWidth = 1.2;
          canvas.drawArc(Rect.fromCircle(center: _s(d.hinge), radius: d.radius * scale), d.arcStart, d.arcSweep, false,
              line);
          line.strokeWidth = 1.4;
        }
      }
    }

    // Room labels are laid out first so dimension labels never cover them.
    final occupied = <Rect>[];
    final roomLabels = <(Offset, List<TextPainter>)>[];
    for (var i = 0; i < layout.rooms.length; i++) {
      final r = layout.rooms[i];
      Rect? box;
      for (final rect in r.rects) {
        box = box == null ? rect : box.expandToInclude(rect);
      }
      if (box == null) continue;
      final sb = Rect.fromPoints(_s(box.topLeft), _s(box.bottomRight));
      bool inside(Offset plan) => r.rects.any((rect) => rect.contains(plan));
      // Labels go at the most interior point or the room's centre, whichever leaves more room.
      final options = [layout.toPlan(r.label), if (inside(box.center)) box.center];
      var c = _s(options.first);
      var maxW = -1.0, maxH = 0.0;
      for (final o in options) {
        final s = _s(o);
        final w = 2 * math.min<double>(s.dx - sb.left, sb.right - s.dx) - 10;
        if (w > maxW) {
          maxW = w;
          maxH = 2 * math.min<double>(s.dy - sb.top, sb.bottom - s.dy) - 10;
          c = s;
        }
      }
      final name = i < names.length ? names[i] : '';
      // Name, area, size: each line is kept only if it fits; a long name first tries a smaller size.
      final candidates = <List<TextPainter Function()>>[
        [() => _text(name, 13, weight: FontWeight.w700), () => _text(name, 11, weight: FontWeight.w700)],
        [() => _text(formatArea(r.area, locale), 12, weight: FontWeight.w500)],
        [
          () => _text('${formatLength(r.width, locale, decimals: 1)} × ${formatLength(r.depth, locale, decimals: 1)}',
              11,
              color: palette.muted)
        ],
      ];
      final lines = <TextPainter>[];
      var height = 0.0;
      for (final options in candidates) {
        for (final make in options) {
          final tp = make();
          if (tp.width <= maxW && height + tp.height <= maxH) {
            lines.add(tp);
            height += tp.height;
            break;
          }
        }
      }
      if (lines.isEmpty) continue;
      final width = lines.map((t) => t.width).reduce(math.max);
      occupied.add(Rect.fromCenter(center: c, width: width + 6, height: height + 4));
      roomLabels.add((c, lines));
    }

    // Dimensions (screen-space text stays crisp and legible at any zoom).
    if (showDimensions) {
      final dimPaint = Paint()
        ..color = palette.muted
        ..strokeWidth = 1;
      for (final w in layout.walls) {
        if (w.length < 0.5) continue;
        final dl = geo.dimension(w);
        final a = _s(dl.p0), b = _s(dl.p1);
        final tp = _text(formatLength(dl.length, locale), 11, weight: FontWeight.w500, color: palette.muted);
        if ((b - a).distance < tp.width + (dl.interior ? 24 : 12)) continue;
        final m = _s(dl.mid) + dl.tick * (tp.height / 2 + (dl.interior ? 3 : 2));
        final upright = math.cos(dl.angle).abs() > 0.5;
        final bounds = Rect.fromCenter(
            center: m, width: upright ? tp.width + 8 : tp.height + 2, height: upright ? tp.height + 2 : tp.width + 8);
        if (occupied.any((o) => o.overlaps(bounds))) continue;
        occupied.add(bounds);
        if (!dl.interior) {
          final tk = dl.tick * 6.0;
          canvas.drawLine(a, b, dimPaint);
          canvas.drawLine(a - tk, a + tk, dimPaint);
          canvas.drawLine(b - tk, b + tk, dimPaint);
        }
        canvas.save();
        canvas.translate(m.dx, m.dy);
        canvas.rotate(dl.angle);
        final box = Rect.fromCenter(center: Offset.zero, width: tp.width + 8, height: tp.height + 2);
        canvas.drawRRect(RRect.fromRectAndRadius(box, const Radius.circular(4)),
            Paint()..color = palette.paper.withValues(alpha: 0.92));
        tp.paint(canvas, Offset(-tp.width / 2, -tp.height / 2));
        canvas.restore();
      }
    }

    for (final (c, lines) in roomLabels) {
      final total = lines.fold(0.0, (a, t) => a + t.height);
      var y = c.dy - total / 2;
      for (final t in lines) {
        final at = Offset(c.dx - t.width / 2, y);
        // Halo in the paper colour keeps text legible over door swings and lines.
        final span = t.text! as TextSpan;
        TextPainter(
          text: TextSpan(
            text: span.text,
            style: span.style!.copyWith(
              color: null,
              foreground: Paint()
                ..style = PaintingStyle.stroke
                ..strokeWidth = 3
                ..strokeJoin = StrokeJoin.round
                ..color = palette.paper,
            ),
          ),
          textDirection: TextDirection.ltr,
          textScaler: textScaler,
        )
          ..layout()
          ..paint(canvas, at);
        t.paint(canvas, at);
        y += t.height;
      }
    }
  }

  @override
  bool shouldRepaint(covariant _PlanPainter old) =>
      !identical(old.layout, layout) ||
      old.scale != scale ||
      old.offset != offset ||
      old.palette.paper != palette.paper ||
      old.textScaler != textScaler ||
      old.baseStyle != baseStyle ||
      old.locale != locale ||
      old.showDimensions != showDimensions ||
      old.names.join() != names.join();
}

/// Door / window symbols with their names, so the plan does not rely on shapes alone.
class PlanLegend extends StatelessWidget {
  const PlanLegend({super.key});

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final palette = PlanPalette.of(context);
    Widget item(CustomPainter p, String label) => Row(mainAxisSize: MainAxisSize.min, children: [
          CustomPaint(size: const Size(28, 20), painter: p),
          const SizedBox(width: 6),
          Text(label, style: Theme.of(context).textTheme.labelLarge),
        ]);
    return Wrap(spacing: 20, runSpacing: 8, crossAxisAlignment: WrapCrossAlignment.center, children: [
      item(_LegendPainter(palette, OpeningKind.door), l.legendDoor),
      item(_LegendPainter(palette, OpeningKind.window), l.legendWindow),
      item(_LegendPainter(palette, null), l.legendWall),
    ]);
  }
}

class _LegendPainter extends CustomPainter {
  _LegendPainter(this.palette, this.kind);
  final PlanPalette palette;
  final OpeningKind? kind;

  @override
  void paint(Canvas canvas, Size size) {
    final ink = Paint()..color = palette.ink;
    final line = Paint()
      ..color = palette.ink
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.4;
    final y = size.height - 4;
    switch (kind) {
      case null:
        canvas.drawRect(Rect.fromLTWH(0, y - 4, size.width, 6), ink);
      case OpeningKind.window:
        canvas.drawRect(Rect.fromLTWH(0, y - 4, 4, 6), ink);
        canvas.drawRect(Rect.fromLTWH(size.width - 4, y - 4, 4, 6), ink);
        canvas.drawRect(Rect.fromLTWH(4, y - 4, size.width - 8, 6), line);
        canvas.drawLine(Offset(4, y - 1), Offset(size.width - 4, y - 1), line);
      case OpeningKind.door:
        canvas.drawRect(Rect.fromLTWH(0, y - 4, 4, 6), ink);
        canvas.drawLine(Offset(4, y), Offset(4, y - 16), line..strokeWidth = 2);
        canvas.drawArc(Rect.fromCircle(center: Offset(4, y), radius: 16), -math.pi / 2, math.pi / 2, false,
            line..strokeWidth = 1.2);
    }
  }

  @override
  bool shouldRepaint(covariant _LegendPainter old) => old.palette.ink != palette.ink || old.kind != kind;
}
