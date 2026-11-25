import 'package:intl/intl.dart';

String formatDuration(double seconds) {
  final s = seconds.floor();
  final h = s ~/ 3600, m = (s % 3600) ~/ 60, sec = s % 60;
  String two(int v) => v.toString().padLeft(2, '0');
  return h > 0 ? '$h:${two(m)}:${two(sec)}' : '${two(m)}:${two(sec)}';
}

String formatCount(num v) {
  if (v >= 1e6) return '${(v / 1e6).toStringAsFixed(2)} M';
  if (v >= 1e4) return '${(v / 1e3).toStringAsFixed(0)} k';
  if (v >= 1e3) return '${(v / 1e3).toStringAsFixed(1)} k';
  return v.toStringAsFixed(0);
}

String formatBytes(int b) {
  if (b >= 1 << 30) return '${(b / (1 << 30)).toStringAsFixed(2)} GB';
  if (b >= 1 << 20) return '${(b / (1 << 20)).toStringAsFixed(1)} MB';
  if (b >= 1 << 10) return '${(b / (1 << 10)).toStringAsFixed(0)} KB';
  return '$b B';
}

String formatMeters(double m) => m >= 100 ? '${m.toStringAsFixed(0)} m' : '${m.toStringAsFixed(1)} m';

/// Plan measurement with the locale's decimal separator, e.g. "3,45 m" in Spanish.
String formatLength(double m, String locale, {int decimals = 2}) =>
    '${NumberFormat.decimalPatternDigits(locale: locale, decimalDigits: decimals).format(m)} m';

/// Floor area with the locale's decimal separator, e.g. "12,4 m²".
String formatArea(double m2, String locale) =>
    '${NumberFormat.decimalPatternDigits(locale: locale, decimalDigits: 1).format(m2)} m²';
