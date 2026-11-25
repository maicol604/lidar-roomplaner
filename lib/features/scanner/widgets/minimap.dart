import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import '../../../core/models.dart';

/// Plan-view coverage minimap: decodes the native RGBA grid and overlays the user position.
class Minimap extends StatefulWidget {
  const Minimap({super.key, required this.data, required this.stats, this.size = 140});

  final MinimapData? data;
  final ScanStats stats;
  final double size;

  @override
  State<Minimap> createState() => _MinimapState();
}

class _MinimapState extends State<Minimap> {
  ui.Image? _image;
  MinimapData? _decoded;

  @override
  void didUpdateWidget(covariant Minimap old) {
    super.didUpdateWidget(old);
    _decode();
  }

  @override
  void initState() {
    super.initState();
    _decode();
  }

  void _decode() {
    final d = widget.data;
    if (d == null || identical(d, _decoded)) return;
    _decoded = d;
    ui.decodeImageFromPixels(d.rgba, d.width, d.height, ui.PixelFormat.rgba8888, (img) {
      if (!mounted || !identical(_decoded, d)) {
        img.dispose();
        return;
      }
      setState(() {
        _image?.dispose();
        _image = img;
      });
    });
  }

  @override
  void dispose() {
    _image?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return ClipRRect(
      borderRadius: BorderRadius.circular(14),
      child: Container(
        width: widget.size,
        height: widget.size,
        color: Colors.black.withValues(alpha: 0.6),
        child: CustomPaint(
          painter: _MinimapPainter(_image, _decoded, widget.stats, scheme.primary),
        ),
      ),
    );
  }
}

class _MinimapPainter extends CustomPainter {
  _MinimapPainter(this.image, this.data, this.stats, this.accent);

  final ui.Image? image;
  final MinimapData? data;
  final ScanStats stats;
  final Color accent;

  @override
  void paint(Canvas canvas, Size size) {
    final img = image, d = data;
    if (img == null || d == null) return;
    final scale = math.min(size.width / img.width, size.height / img.height);
    final w = img.width * scale, h = img.height * scale;
    final off = Offset((size.width - w) / 2, (size.height - h) / 2);
    canvas.drawImageRect(
      img,
      Rect.fromLTWH(0, 0, img.width.toDouble(), img.height.toDouble()),
      off & Size(w, h),
      Paint()..filterQuality = FilterQuality.none,
    );
    if (!stats.hasDevice) return;
    final p = off + Offset((stats.deviceX - d.originX) / d.resolution * scale, (stats.deviceZ - d.originZ) / d.resolution * scale);
    final heading = math.atan2(stats.fwdX, -stats.fwdZ);
    canvas.save();
    canvas.translate(p.dx, p.dy);
    canvas.rotate(heading);
    final arrow = Path()
      ..moveTo(0, -9)
      ..lineTo(6, 6)
      ..lineTo(0, 3)
      ..lineTo(-6, 6)
      ..close();
    canvas.drawPath(arrow, Paint()..color = accent);
    canvas.drawPath(arrow, Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.2);
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant _MinimapPainter old) =>
      old.image != image || old.stats.deviceX != stats.deviceX || old.stats.deviceZ != stats.deviceZ || old.stats.fwdX != stats.fwdX;
}
