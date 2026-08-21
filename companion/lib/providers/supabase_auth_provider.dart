import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../core/services/supabase_service.dart';
import 'app_providers.dart';

/// Supabase auth state exposed to the UI.
class SupabaseAuthState {
  const SupabaseAuthState({
    required this.isInitialized,
    required this.isSignedIn,
    this.email,
    this.userId,
  });

  const SupabaseAuthState.idle()
    : isInitialized = false,
      isSignedIn = false,
      email = null,
      userId = null;

  final bool isInitialized;
  final bool isSignedIn;
  final String? email;
  final String? userId;

  SupabaseAuthState copyWith({
    bool? isInitialized,
    bool? isSignedIn,
    String? email,
    String? userId,
  }) {
    return SupabaseAuthState(
      isInitialized: isInitialized ?? this.isInitialized,
      isSignedIn: isSignedIn ?? this.isSignedIn,
      email: email ?? this.email,
      userId: userId ?? this.userId,
    );
  }
}

/// Mirrors the Supabase auth session for the rest of the app.
class SupabaseAuthNotifier extends StateNotifier<SupabaseAuthState> {
  SupabaseAuthNotifier(this._service) : super(const SupabaseAuthState.idle());

  final SupabaseService _service;

  StreamSubscription<dynamic>? _subscription;

  /// Subscribes to auth events so the UI stays in sync with sign-in/out.
  void start() {
    if (_subscription != null || !_service.isInitialized) {
      return;
    }
    _subscription = _service.authState?.listen((_) => refresh());
    refresh();
  }

  /// Re-reads the current session.
  void refresh() {
    state = SupabaseAuthState(
      isInitialized: _service.isInitialized,
      isSignedIn: _service.isSignedIn,
      email: _service.userEmail,
      userId: _service.userId,
    );
  }

  @override
  void dispose() {
    _subscription?.cancel();
    super.dispose();
  }
}

final supabaseAuthProvider =
    StateNotifierProvider<SupabaseAuthNotifier, SupabaseAuthState>((ref) {
      final notifier = SupabaseAuthNotifier(ref.watch(supabaseServiceProvider));
      return notifier;
    });
