import 'dart:math' as math;
import 'dart:ui';

import 'format.dart';
import 'layout.dart';
import 'plan_geometry.dart';

/// Texts used in the exported floor plan (already localized).
class PlanSvgTexts {
  const PlanSvgTexts({
    required this.title,
    required this.subtitle,
    required this.roomNames,
    required this.door,
    required this.window,
    required this.disclaimer,
  });

  final String title, subtitle;
  final List<String> roomNames;
  final String door, window;

  /// E.g. "Approximate measurements".
  final String disclaimer;
}

/// Clean floor plan (walls, doors, windows, rooms, dimensions; no furniture) as a standalone SVG.
/// Units are centimetres (1 user unit = 1 cm), drawn in the plan frame.
String buildFloorPlanSvg(RoomLayout layout, PlanSvgTexts t, String locale) {
  const ink = '#1B1F23', dim = '#44515C', roomFill = '#E7F2EF', paper = '#FFFFFF';
  final g = PlanGeometry(layout);
  final bounds = (layout.planBounds() ?? Rect.zero).inflate(0.9);
  const header = 140.0, footer = 120.0;
  double x(double u) => (u - bounds.left) * 100;
  double y(double v) => (v - bounds.top) * 100 + header;
  String pt(Offset o) => '${_n(x(o.dx))},${_n(y(o.dy))}';
  final w = bounds.width * 100, h = bounds.height * 100 + header + footer;
  final b = StringBuffer()
    ..writeln('<?xml version="1.0" encoding="UTF-8"?>')
    ..writeln('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${_n(w)} ${_n(h)}" '
        'width="${_n(w / 10)}mm" height="${_n(h / 10)}mm" font-family="Roboto, Helvetica, Arial, sans-serif">')
    ..writeln('<title>${_esc(t.title)}</title>')
    ..writeln('<rect width="${_n(w)}" height="${_n(h)}" fill="$paper"/>');

  // Rooms.
  b.writeln('<g fill="$roomFill" stroke="$roomFill" stroke-width="0.6">');
  for (final r in layout.rooms) {
    for (final rect in r.rects) {
      b.writeln('<rect x="${_n(x(rect.left))}" y="${_n(y(rect.top))}" width="${_n(rect.width * 100)}" '
          'height="${_n(rect.height * 100)}"/>');
    }
  }
  b.writeln('</g>');

  // Walls (solid full-height stretches).
  b.writeln('<g fill="$ink">');
  for (final wall in layout.walls) {
    for (final s in g.solidSpans(wall)) {
      b.writeln('<polygon points="${g.quad(wall, s.t0, s.t1).map(pt).join(' ')}"/>');
    }
  }
  b.writeln('</g>');

  // Openings.
  b.writeln('<g fill="none" stroke="$ink" stroke-width="1.6">');
  for (final wall in layout.walls) {
    for (final o in wall.openings) {
      if (o.kind == OpeningKind.window) {
        final q = g.quad(wall, o.t0, o.t1);
        final c = g.quad(wall, o.t0, o.t1, halfWidth: 0);
        b
          ..writeln('<polygon points="${q.map(pt).join(' ')}" fill="$paper"/>')
          ..writeln('<line x1="${_n(x(c[0].dx))}" y1="${_n(y(c[0].dy))}" x2="${_n(x(c[1].dx))}" y2="${_n(y(c[1].dy))}"/>');
      } else {
        final d = g.door(wall, o);
        final end = g.p(wall.a) + g.dir(wall) * o.t1;
        b
          ..writeln('<line x1="${_n(x(d.hinge.dx))}" y1="${_n(y(d.hinge.dy))}" x2="${_n(x(d.leafEnd.dx))}" '
              'y2="${_n(y(d.leafEnd.dy))}" stroke-width="2.4"/>')
          ..writeln('<path d="M ${pt(d.leafEnd)} A ${_n(d.radius * 100)} ${_n(d.radius * 100)} 0 0 '
              '${d.arcSweep > 0 ? 1 : 0} ${pt(end)}" stroke-dasharray="6 5"/>');
      }
    }
  }
  b.writeln('</g>');

  // Dimensions.
  b.writeln('<g stroke="$dim" stroke-width="1.2" fill="$dim" font-size="20" text-anchor="middle">');
  for (final wall in layout.walls) {
    if (wall.length < 0.6) continue;
    final dl = g.dimension(wall);
    final tk = dl.tick * 0.12;
    if (!dl.interior) {
      b
        ..writeln('<line x1="${_n(x(dl.p0.dx))}" y1="${_n(y(dl.p0.dy))}" x2="${_n(x(dl.p1.dx))}" y2="${_n(y(dl.p1.dy))}"/>')
        ..writeln('<line x1="${_n(x((dl.p0 - tk).dx))}" y1="${_n(y((dl.p0 - tk).dy))}" '
            'x2="${_n(x((dl.p0 + tk).dx))}" y2="${_n(y((dl.p0 + tk).dy))}"/>')
        ..writeln('<line x1="${_n(x((dl.p1 - tk).dx))}" y1="${_n(y((dl.p1 - tk).dy))}" '
            'x2="${_n(x((dl.p1 + tk).dx))}" y2="${_n(y((dl.p1 + tk).dy))}"/>');
    }
    final m = dl.mid + dl.tick * (dl.interior ? 0.14 : 0.1);
    b.writeln('<text x="${_n(x(m.dx))}" y="${_n(y(m.dy))}" stroke="none" dominant-baseline="middle"'
        '${dl.interior ? ' font-size="16"' : ''} '
        'transform="rotate(${_n(dl.angle * 180 / math.pi)} ${_n(x(m.dx))} ${_n(y(m.dy))})">'
        '${_esc(formatLength(dl.length, locale))}</text>');
  }
  b.writeln('</g>');

  // Room labels.
  b.writeln('<g fill="$ink" text-anchor="middle">');
  for (var i = 0; i < layout.rooms.length; i++) {
    final r = layout.rooms[i];
    final c = g.p(r.label);
    final name = i < t.roomNames.length ? t.roomNames[i] : '';
    b
      ..writeln('<text x="${_n(x(c.dx))}" y="${_n(y(c.dy) - 14)}" font-size="26" font-weight="600">${_esc(name)}</text>')
      ..writeln('<text x="${_n(x(c.dx))}" y="${_n(y(c.dy) + 14)}" font-size="22">${_esc(formatArea(r.area, locale))}</text>')
      ..writeln('<text x="${_n(x(c.dx))}" y="${_n(y(c.dy) + 38)}" font-size="18" fill="$dim">'
          '${_esc('${formatLength(r.width, locale, decimals: 1)} × ${formatLength(r.depth, locale, decimals: 1)}')}</text>');
  }
  b.writeln('</g>');

  // Title block, scale bar and legend.
  b
    ..writeln('<text x="40" y="56" font-size="34" font-weight="700" fill="$ink">${_esc(t.title)}</text>')
    ..writeln('<text x="40" y="96" font-size="22" fill="$dim">${_esc(t.subtitle)}</text>');
  final by = h - 60;
  b
    ..writeln('<g stroke="$ink" stroke-width="2" fill="$ink" font-size="20">')
    ..writeln('<line x1="40" y1="${_n(by)}" x2="140" y2="${_n(by)}"/>')
    ..writeln('<line x1="40" y1="${_n(by - 10)}" x2="40" y2="${_n(by + 10)}"/>')
    ..writeln('<line x1="140" y1="${_n(by - 10)}" x2="140" y2="${_n(by + 10)}"/>')
    ..writeln('<text x="90" y="${_n(by - 18)}" stroke="none" text-anchor="middle">1 m</text>')
    ..writeln('<path d="M 200 ${_n(by + 12)} L 200 ${_n(by - 28)} A 40 40 0 0 1 240 ${_n(by + 12)}" fill="none" '
        'stroke-width="1.6"/>')
    ..writeln('<text x="254" y="${_n(by + 8)}" stroke="none">${_esc(t.door)}</text>')
    ..writeln('<rect x="380" y="${_n(by - 6)}" width="60" height="12" fill="$paper" stroke-width="1.6"/>')
    ..writeln('<line x1="380" y1="${_n(by)}" x2="440" y2="${_n(by)}" stroke-width="1.6"/>')
    ..writeln('<text x="454" y="${_n(by + 8)}" stroke="none">${_esc(t.window)}</text>')
    ..writeln('<text x="40" y="${_n(by + 44)}" stroke="none" fill="$dim" font-size="16">${_esc(t.disclaimer)}</text>')
    ..writeln('</g>')
    ..writeln('</svg>');
  return b.toString();
}

String _n(double v) => v.isFinite ? v.toStringAsFixed(1) : '0';

String _esc(String s) =>
    s.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
