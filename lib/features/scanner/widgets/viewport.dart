import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/providers.dart';
import '../../../core/scanner_bridge.dart';
import '../../../l10n/app_localizations.dart';

/// Displays the native OpenGL point cloud renderer (Flutter Texture backed by a SurfaceProducer)
/// and forwards gestures: 1 finger = orbit, 2 fingers = pan + pinch zoom, double tap = recenter.
class PointCloudViewport extends ConsumerStatefulWidget {
  const PointCloudViewport({super.key});

  @override
  ConsumerState<PointCloudViewport> createState() => _PointCloudViewportState();
}

class _PointCloudViewportState extends ConsumerState<PointCloudViewport> {
  late final ScannerBridge _bridge = ref.read(bridgeProvider);
  int? _textureId;
  Size _requested = Size.zero;
  bool _creating = false;
  double _lastScale = 1;
  int _pointers = 0;

  Future<void> _ensureTexture(Size physical) async {
    final bridge = _bridge;
    final w = physical.width.round(), h = physical.height.round();
    if (w <= 0 || h <= 0 || physical == _requested) return;
    _requested = physical;
    if (_textureId == null) {
      if (_creating) return;
      _creating = true;
      final id = await bridge.createViewport(w, h);
      _creating = false;
      if (!mounted) {
        bridge.disposeViewport();
        return;
      }
      setState(() => _textureId = id);
      // The layout may have changed while the texture was being created.
      if (_requested.width.round() != w || _requested.height.round() != h) {
        await bridge.resizeViewport(_requested.width.round(), _requested.height.round());
      }
    } else {
      await bridge.resizeViewport(w, h);
    }
  }

  @override
  void dispose() {
    if (_textureId != null) _bridge.disposeViewport();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final dpr = MediaQuery.devicePixelRatioOf(context);
    final bridge = _bridge;
    final l = AppLocalizations.of(context);
    return Semantics(
      image: true,
      label: l.cloudViewSemantics,
      hint: l.cloudViewHint,
      child: LayoutBuilder(builder: (context, constraints) {
      final physical = Size(constraints.maxWidth * dpr, constraints.maxHeight * dpr);
      WidgetsBinding.instance.addPostFrameCallback((_) => _ensureTexture(physical));
      return Listener(
        onPointerDown: (_) => _pointers++,
        onPointerUp: (_) => _pointers = (_pointers - 1).clamp(0, 10),
        onPointerCancel: (_) => _pointers = (_pointers - 1).clamp(0, 10),
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onDoubleTap: bridge.resetView,
          onScaleStart: (_) => _lastScale = 1,
          onScaleUpdate: (d) {
            final delta = d.focalPointDelta * dpr;
            if (d.pointerCount >= 2 || _pointers >= 2) {
              if (delta != Offset.zero) bridge.pan(delta.dx, delta.dy);
              final ratio = d.scale / _lastScale;
              _lastScale = d.scale;
              if ((ratio - 1).abs() > 0.001) bridge.zoom(ratio);
            } else if (delta != Offset.zero) {
              bridge.orbit(delta.dx, delta.dy);
            }
          },
          child: _textureId == null ? const SizedBox.expand() : Texture(textureId: _textureId!),
        ),
      );
      }),
    );
  }
}
