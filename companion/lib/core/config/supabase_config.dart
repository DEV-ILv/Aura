/// Supabase project configuration for remote (cloud) access.
///
/// Credentials are injected at build/run time via `--dart-define`; real
/// values are never committed. See `.env.example` in the project root.
///
/// Only the **publishable (anon)** key is used by the app. The anon key is
/// designed to be embedded in clients; Row Level Security (RLS) on every
/// table is what protects the data.
///
/// The **service role key** MUST NEVER be embedded, committed, or exposed in
/// the Flutter app — it can bypass RLS entirely.
abstract final class SupabaseConfig {
  /// Project URL as shown in the Supabase dashboard.
  static const String projectUrl = String.fromEnvironment('SUPABASE_URL');

  /// Publishable (anon) key — safe for client distribution.
  static const String anonKey = String.fromEnvironment('SUPABASE_ANON_KEY');
}
