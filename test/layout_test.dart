import 'dart:convert';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:lidarscanner/core/layout.dart';
import 'package:lidarscanner/core/models.dart';
import 'package:lidarscanner/core/plan_svg.dart';
import 'package:lidarscanner/core/session_config.dart';
import 'package:lidarscanner/core/settings.dart';
import 'package:lidarscanner/features/layout/floor_plan_view.dart';
import 'package:lidarscanner/features/layout/model_view.dart';
import 'package:lidarscanner/l10n/app_localizations.dart';

/// A 4 x 3 m room (walls on the plan axes) with a door on the south wall and a window on the
/// north wall, a table, and the device in the middle, as the native engine writes it.
String sampleJson({double theta = 0}) => jsonEncode({
      'v': 1,
      'valid': true,
      'floorY': 0.0,
      'ceilingY': 2.6,
      'ceilingEstimated': false,
      'theta': theta,
      'walls': [
        {'ax': 0, 'az': 0, 'bx': 4, 'bz': 0, 'th': 0.12, 'h': 2.6, 'ja': true, 'jb': true, 'ea': 0.06, 'eb': 0.06,
          'faces': 1, 'op': [{'k': 'window', 't0': 1.0, 't1': 2.2, 'b': 0.9, 't': 2.1}]},
        {'ax': 4, 'az': 0, 'bx': 4, 'bz': 3, 'th': 0.12, 'h': 2.6, 'ja': true, 'jb': true, 'ea': 0.06, 'eb': 0.06,
          'faces': 1, 'op': []},
        {'ax': 4, 'az': 3, 'bx': 0, 'bz': 3, 'th': 0.10, 'h': 2.6, 'ja': true, 'jb': true, 'ea': 0.06, 'eb': 0.06,
          'faces': 2, 'op': [{'k': 'door', 't0': 0.5, 't1': 1.3, 'b': 0.0, 't': 2.05}]},
        {'ax': 0, 'az': 3, 'bx': 0, 'bz': 0, 'th': 0.12, 'h': 2.6, 'ja': true, 'jb': true, 'ea': 0.06, 'eb': 0.06,
          'faces': 1, 'op': []},
      ],
      'objects': [
        {'cx': 2.0, 'cz': 1.5, 'w': 1.2, 'd': 0.8, 'h': 0.76, 'a': theta, 'k': 'table'},
      ],
      'rooms': [
        {'area': 11.28, 'lx': 2.0, 'lz': 1.5, 'w': 3.88, 'd': 2.88, 'rects': [[0.06, 0.06, 3.94, 2.94]]},
      ],
      'device': {'x': 2.0, 'z': 2.0, 'fx': 0.0, 'fz': -1.0},
      'corr': {'a': 0.01, 'tx': 0.05, 'tz': -0.02},
      'stats': {'submaps': 12, 'loopEdges': 3, 'rejectedEdges': 1, 'mergedFaces': 2, 'closedCorners': 4,
        'maxCorrectionM': 0.21, 'maxCorrectionDeg': 2.5},
    });

Widget localized(Widget child, {Locale locale = const Locale('es')}) => MaterialApp(
      locale: locale,
      localizationsDelegates: AppLocalizations.localizationsDelegates,
      supportedLocales: AppLocalizations.supportedLocales,
      home: Scaffold(body: child),
    );

void main() {
  test('layout JSON is parsed with openings, rooms, furniture and diagnostics', () {
    final l = RoomLayout.tryParse(sampleJson())!;
    expect(l.valid, isTrue);
    expect(l.height, closeTo(2.6, 1e-9));
    expect(l.walls, hasLength(4));
    expect(l.walls[2].faces, 2);
    expect(l.doorCount, 1);
    expect(l.windowCount, 1);
    expect(l.objects.single.kind, FurnitureKind.table);
    expect(l.totalArea, closeTo(11.28, 1e-9));
    expect(l.rooms.single.rects.single, const Rect.fromLTRB(0.06, 0.06, 3.94, 2.94));
    expect(l.stats.loopEdges, 3);
    expect(l.stats.maxCorrectionDeg, 2.5);
    expect(RoomLayout.tryParse(''), isNull);
    expect(RoomLayout.tryParse('{"walls": [oops'), isNull);
    // The native core emits an explicit empty model when a scan starts.
    expect(RoomLayout.tryParse('{"v":1,"valid":false}')!.isEmpty, isTrue);
  });

  test('plan frame and correction transforms follow the native conventions', () {
    final l = RoomLayout.tryParse(sampleJson(theta: math.pi / 6))!;
    const p = Offset(1.3, -2.7);
    final back = l.fromPlan(l.toPlan(p));
    expect(back.dx, closeTo(p.dx, 1e-9));
    expect(back.dy, closeTo(p.dy, 1e-9));
    // u axis = (cos θ, sin θ): a point along it has v = 0.
    final u = l.toPlan(Offset(math.cos(math.pi / 6), math.sin(math.pi / 6)));
    expect(u.dx, closeTo(1, 1e-9));
    expect(u.dy, closeTo(0, 1e-9));
    // Rotation about +Y as axisAngle({0,1,0}, a): x axis -> (cos a, -sin a).
    const t = PlanTransform(math.pi / 2, 1, 2);
    final r = t.apply(const Offset(1, 0));
    expect(r.dx, closeTo(1, 1e-9));
    expect(r.dy, closeTo(1, 1e-9));
  });

  test('walls are split around doors (lintel) and windows (sill + lintel)', () {
    final l = RoomLayout.tryParse(sampleJson())!;
    final door = l.walls[2].spans();
    expect(door.map((s) => (s.t0, s.t1, s.bottom, s.top, s.opening)).toList(), [
      (-0.06, 0.5, 0.0, 2.6, null),
      (0.5, 1.3, 2.05, 2.6, OpeningKind.door),
      (1.3, 4.06, 0.0, 2.6, null),
    ]);
    final window = l.walls[0].spans();
    expect(window.where((s) => s.opening == OpeningKind.window).map((s) => (s.bottom, s.top)).toList(), [
      (0.0, 0.9),
      (2.1, 2.6),
    ]);
  });

  test('floor plan SVG has walls, openings, localized dimensions and escaped text', () {
    final l = RoomLayout.tryParse(sampleJson())!;
    final svg = buildFloorPlanSvg(
      l,
      const PlanSvgTexts(
        title: 'Piso <Centro> & co',
        subtitle: '25 nov 2026',
        roomNames: ['Estancia 1'],
        door: 'Puerta',
        window: 'Ventana',
        disclaimer: 'Medidas aproximadas',
      ),
      'es',
    );
    expect(svg, startsWith('<?xml'));
    expect(svg, contains('<title>Piso &lt;Centro&gt; &amp; co</title>'));
    expect(RegExp('<polygon points=').allMatches(svg).length, greaterThanOrEqualTo(6));
    expect(svg, contains('4,00 m'));
    expect(svg, contains('3,00 m'));
    expect(svg, contains('11,3 m²'));
    expect(svg, contains('stroke-dasharray'));
    expect(svg.trim(), endsWith('</svg>'));
  });

  test('automatic tour selects the scripted simulator motion', () {
    const device = DeviceInfo(model: 'x', sdkInt: 30, totalRamMb: 8000, lowEnd: false, recommendedBudget: 3000000);
    String motion(ScanRequest r) => buildCoreConfig(r, const AppSettings(), device)
        .split('\n')
        .firstWhere((l) => l.startsWith('mockMotion='))
        .substring('mockMotion='.length);
    expect(motion(const ScanRequest(name: 'a', source: ScanSource.mock, mode: ScanMode.continuous, autoTour: true)),
        'auto');
    expect(motion(const ScanRequest(name: 'a', source: ScanSource.mock, mode: ScanMode.continuous)), 'device');
  });

  testWidgets('3D model exposes a summary and button / screen-reader alternatives to gestures', (tester) async {
    final handle = tester.ensureSemantics();
    final layout = RoomLayout.tryParse(sampleJson())!;
    await tester.pumpWidget(localized(SizedBox(width: 400, height: 500, child: LayoutModelView(layout: layout))));
    await tester.pumpAndSettle();
    expect(
      find.bySemanticsLabel(RegExp('Modelo 3D\\. 4 paredes, 1 puerta, 1 ventana, 1 mueble · 11,3 m²')),
      findsOneWidget,
    );
    for (final tip in ['Girar a la izquierda', 'Girar a la derecha', 'Acercar', 'Alejar', 'Recentrar vista']) {
      expect(find.byTooltip(tip), findsOneWidget);
    }
    final node = tester.getSemantics(find.bySemanticsLabel(RegExp('Modelo 3D\\.')));
    expect(node.getSemanticsData().customSemanticsActionIds, hasLength(5));
    await tester.tap(find.byTooltip('Acercar'));
    await tester.pumpAndSettle();
    handle.dispose();
  });

  testWidgets('empty model shows guidance instead of a blank view', (tester) async {
    await tester.pumpWidget(localized(const SizedBox(width: 300, height: 300, child: LayoutModelView(layout: null))));
    expect(find.text('Aún no hay modelo. Recorre el espacio y aparecerá aquí.'), findsOneWidget);
    expect(find.byTooltip('Acercar'), findsNothing);
  });

  testWidgets('floor plan renders with zoom buttons and an accessible scale', (tester) async {
    final handle = tester.ensureSemantics();
    final layout = RoomLayout.tryParse(sampleJson())!;
    await tester.pumpWidget(localized(SizedBox(
      width: 400,
      height: 500,
      child: Builder(
        builder: (c) => FloorPlanView(layout: layout, roomNames: roomNames(AppLocalizations.of(c), layout)),
      ),
    )));
    await tester.pumpAndSettle();
    expect(find.byTooltip('Encajar el plano'), findsOneWidget);
    expect(find.bySemanticsLabel(RegExp('^Escala: ')), findsOneWidget);
    await tester.tap(find.byTooltip('Acercar'));
    await tester.pumpAndSettle();
    handle.dispose();
  });

  testWidgets('rooms are named in reading order and corridors are recognised', (tester) async {
    late List<String> names;
    final layout = RoomLayout.fromJson({
      'rooms': [
        {'area': 20, 'lx': 0, 'lz': 0, 'w': 5, 'd': 4, 'rects': []},
        {'area': 9, 'lx': 0, 'lz': 0, 'w': 8, 'd': 1.1, 'rects': []},
        {'area': 9, 'lx': 0, 'lz': 0, 'w': 3, 'd': 3, 'rects': []},
      ],
    });
    await tester.pumpWidget(localized(Builder(builder: (c) {
      names = roomNames(AppLocalizations.of(c), layout);
      return const SizedBox();
    })));
    expect(names, ['Estancia 1', 'Pasillo', 'Estancia 2']);
  });
}
