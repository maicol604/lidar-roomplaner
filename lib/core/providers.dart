import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'layout.dart';
import 'models.dart';
import 'scanner_bridge.dart';
import 'sessions.dart';
import 'settings.dart';

/// Overridden in main() with the loaded instance.
final sharedPrefsProvider = Provider<SharedPreferences>((ref) => throw UnimplementedError());

final bridgeProvider = Provider<ScannerBridge>((ref) => ScannerBridge());

final sessionsRepositoryProvider = Provider<SessionsRepository>((ref) => SessionsRepository());

final deviceInfoProvider = FutureProvider<DeviceInfo>((ref) => ref.watch(bridgeProvider).deviceInfo());

class SettingsNotifier extends Notifier<AppSettings> {
  static const _key = 'settings';

  @override
  AppSettings build() => AppSettings.decode(ref.watch(sharedPrefsProvider).getString(_key));

  void update(AppSettings Function(AppSettings s) change) {
    state = change(state);
    ref.read(sharedPrefsProvider).setString(_key, state.encode());
  }

  void reset() {
    final keep = state;
    state = AppSettings(themeMode: keep.themeMode, localeCode: keep.localeCode);
    ref.read(sharedPrefsProvider).setString(_key, state.encode());
  }
}

final settingsProvider = NotifierProvider<SettingsNotifier, AppSettings>(SettingsNotifier.new);

final statsProvider = StreamProvider<ScanStats>((ref) => ref.watch(bridgeProvider).stats());

final minimapProvider = StreamProvider<MinimapData>((ref) => ref.watch(bridgeProvider).minimap());

final revolutionProvider = StreamProvider<Revolution>((ref) => ref.watch(bridgeProvider).revolution());

final layoutProvider = StreamProvider<RoomLayout>((ref) => ref.watch(bridgeProvider).layout());

class SessionsNotifier extends AsyncNotifier<List<SessionInfo>> {
  @override
  Future<List<SessionInfo>> build() => ref.watch(sessionsRepositoryProvider).list();

  Future<void> refresh() async {
    state = await AsyncValue.guard(() => ref.read(sessionsRepositoryProvider).list());
  }

  Future<void> delete(SessionInfo s) async {
    await ref.read(sessionsRepositoryProvider).delete(s);
    await refresh();
  }
}

final sessionsProvider = AsyncNotifierProvider<SessionsNotifier, List<SessionInfo>>(SessionsNotifier.new);
