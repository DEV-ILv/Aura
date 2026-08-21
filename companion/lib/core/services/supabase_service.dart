import 'package:supabase_flutter/supabase_flutter.dart';

import '../config/supabase_config.dart';
import 'logger.dart';

/// Raised when a cloud operation cannot be performed (service offline or not
/// initialised).
class SupabaseUnavailableException implements Exception {
  const SupabaseUnavailableException(this.message);

  final String message;

  @override
  String toString() => message;
}

/// Thin wrapper around the initialised Supabase client.
///
/// Initialisation happens once during app startup in `main()`. The service is
/// deliberately safe to touch before initialisation — every operation either
/// works or throws a [SupabaseUnavailableException] that callers can surface
/// gracefully.
class SupabaseService {
  SupabaseService._();

  static final SupabaseService instance = SupabaseService._();

  bool _initialized = false;
  String _initError = '';

  /// Whether the Supabase client was successfully initialised.
  bool get isInitialized => _initialized;

  /// Initialisation error message (empty when initialisation succeeded).
  String get initError => _initError;

  /// Initialises the shared Supabase client. Safe to call more than once.
  Future<void> init() async {
    if (_initialized) {
      return;
    }
    try {
      await Supabase.initialize(
        url: SupabaseConfig.projectUrl,
        publishableKey: SupabaseConfig.anonKey,
      );
      _initialized = true;
    } catch (error, stackTrace) {
      _initError = '$error';
      Logger.error('Supabase initialisation failed: $error', stackTrace);
    }
  }

  /// The active client, or `null` when unavailable.
  SupabaseClient? get client => _initialized ? Supabase.instance.client : null;

  SupabaseClient _requireClient() {
    final value = client;
    if (value == null) {
      throw const SupabaseUnavailableException(
        'Cloud service is unavailable right now. Check your internet '
        'connection and try again.',
      );
    }
    return value;
  }

  /// Whether a cloud session is currently active.
  bool get isSignedIn =>
      _initialized && Supabase.instance.client.auth.currentUser != null;

  /// The signed-in user's email, or `null` when signed out.
  String? get userEmail =>
      _initialized ? Supabase.instance.client.auth.currentUser?.email : null;

  /// The signed-in user's id, or `null` when signed out.
  String? get userId =>
      _initialized ? Supabase.instance.client.auth.currentUser?.id : null;

  /// Emits auth state changes (sign-in, sign-out, token refresh, …).
  Stream<AuthState>? get authState =>
      _initialized ? Supabase.instance.client.auth.onAuthStateChange : null;

  /// Signs in with email + password.
  Future<void> signIn({required String email, required String password}) async {
    await _requireClient().auth.signInWithPassword(
      email: email,
      password: password,
    );
  }

  /// Creates a new account. When email confirmation is required by the
  /// project, [emailConfirmedRequired] is surfaced through an exception so the
  /// UI can prompt the user to check their inbox.
  Future<void> signUp({required String email, required String password}) async {
    final response = await _requireClient().auth.signUp(
      email: email,
      password: password,
    );
    if (response.session == null && response.user == null) {
      throw const SupabaseUnavailableException(
        'Account created — check your inbox to confirm your email, then sign in.',
      );
    }
  }

  /// Sends a password-reset email to [email].
  Future<void> sendPasswordReset({required String email}) async {
    await _requireClient().auth.resetPasswordForEmail(email);
  }

  /// Signs the current cloud session out.
  Future<void> signOut() async {
    if (!_initialized) {
      return;
    }
    await Supabase.instance.client.auth.signOut();
  }
}
