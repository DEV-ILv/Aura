import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../api/api_service.dart';
import '../models/device_control.dart';
import 'app_providers.dart';
import 'connection_provider.dart';

/// State of the V2 device-control surface.
class DeviceControlState {
  const DeviceControlState({
    this.isLoading = true,
    this.isBusy = false,
    this.display = const DisplayControlState.unknown(),
    this.led = const LedControlState.unknown(),
    this.networks = const [],
    this.mic = const MicLiveLevel.unknown(),
    this.lastError = '',
  });

  final bool isLoading;
  final bool isBusy;
  final DisplayControlState display;
  final LedControlState led;
  final List<WifiNetwork> networks;
  final MicLiveLevel mic;
  final String lastError;

  DeviceControlState copyWith({
    bool? isLoading,
    bool? isBusy,
    DisplayControlState? display,
    LedControlState? led,
    List<WifiNetwork>? networks,
    MicLiveLevel? mic,
    String? lastError,
  }) {
    return DeviceControlState(
      isLoading: isLoading ?? this.isLoading,
      isBusy: isBusy ?? this.isBusy,
      display: display ?? this.display,
      led: led ?? this.led,
      networks: networks ?? this.networks,
      mic: mic ?? this.mic,
      lastError: lastError ?? this.lastError,
    );
  }
}

/// Controller for the physical device-control endpoints.
///
/// Hardware controls only function over a direct local-LAN REST session; the
/// UI is responsible for gating those behind `ConnectionMode.local`.
class DeviceControlNotifier extends StateNotifier<DeviceControlState> {
  DeviceControlNotifier(this._api, {required this._canPollNow})
    : super(const DeviceControlState());

  final ApiService _api;
  final bool Function() _canPollNow;
  Timer? _micTimer;
  bool _micInFlight = false;

  /// Loads the persisted display and LED state from the device.
  Future<void> load() async {
    state = state.copyWith(isLoading: true, lastError: '');
    try {
      final display = await _api.fetchDisplayControl();
      final led = await _api.fetchLedControl();
      state = state.copyWith(
        isLoading: false,
        display: display,
        led: led,
        lastError: '',
      );
    } catch (error) {
      state = state.copyWith(isLoading: false, lastError: '$error');
    }
  }

  /// Applies display settings then refreshes the local mirror.
  Future<void> setDisplay(Map<String, dynamic> body) async {
    await _runBusy(() async {
      await _api.setDisplayControl(body);
      final display = await _api.fetchDisplayControl();
      state = state.copyWith(display: display);
    });
  }

  /// Applies LED settings then refreshes the local mirror.
  Future<void> setLed(Map<String, dynamic> body) async {
    await _runBusy(() async {
      await _api.setLedControl(body);
      final led = await _api.fetchLedControl();
      state = state.copyWith(led: led);
    });
  }

  /// Applies speaker settings (no mirror state is fetched back).
  Future<void> setAudio(Map<String, dynamic> body) async {
    await _runBusy(() => _api.setAudioControl(body));
  }

  /// Applies microphone settings.
  Future<void> setMic(Map<String, dynamic> body) async {
    await _runBusy(() => _api.setMicControl(body));
  }

  /// Scans for nearby networks and stores the results.
  Future<void> scanWifi() async {
    await _runBusy(() async {
      final networks = await _api.scanWifi();
      state = state.copyWith(networks: networks);
    });
  }

  /// Clears the saved credentials on the device.
  Future<void> forgetWifi() async {
    await _runBusy(() => _api.forgetWifi());
  }

  /// Connects the device to a Wi-Fi network (existing `/api/wifi` endpoint).
  Future<void> connectWifi({
    required String ssid,
    required String password,
  }) async {
    await _runBusy(() => _api.connectWifi(ssid: ssid, password: password));
  }

  /// Requests a device restart.
  Future<void> restartDevice() async {
    await _runBusy(() => _api.restartDevice());
  }

  /// Requests a factory reset.
  Future<void> factoryReset() async {
    await _runBusy(() => _api.factoryReset());
  }

  /// Begins polling the live microphone level (cancelled on dispose).
  void startMicPolling() {
    _micTimer ??= Timer.periodic(const Duration(milliseconds: 800), (_) {
      _refreshMic();
    });
  }

  void stopMicPolling() {
    _micTimer?.cancel();
    _micTimer = null;
  }

  Future<void> _refreshMic() async {
    // Never poll a dead or cloud-only connection: repeated timers against an
    // unreachable/stale address would otherwise pile up timeouts every 800ms.
    if (_micInFlight || state.isBusy || !_canPollNow()) return;
    _micInFlight = true;
    try {
      final mic = await _api.fetchMicLevel();
      if (!mounted) return;
      state = state.copyWith(mic: mic);
    } catch (_) {
      // Live meter is best-effort; failures are ignored while polling.
    } finally {
      _micInFlight = false;
    }
  }

  Future<void> _runBusy(Future<void> Function() action) async {
    state = state.copyWith(isBusy: true, lastError: '');
    try {
      await action();
      state = state.copyWith(isBusy: false);
    } catch (error) {
      state = state.copyWith(isBusy: false, lastError: '$error');
      rethrow;
    }
  }

  @override
  void dispose() {
    stopMicPolling();
    super.dispose();
  }
}

final deviceControlProvider =
    StateNotifierProvider<DeviceControlNotifier, DeviceControlState>((ref) {
      return DeviceControlNotifier(
        ref.watch(apiServiceProvider),
        canPollNow: () {
          final connection = ref.read(connectionProvider);
          return connection.isConnected &&
              connection.mode == ConnectionMode.local;
        },
      );
    });
