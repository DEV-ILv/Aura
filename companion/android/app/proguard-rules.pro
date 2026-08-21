# AURA Companion — R8 / ProGuard rules.
#
# Keeps the Flutter engine, plugin registrants and reflection-based plugin
# entry points alive under code shrinking.

# Flutter engine + embedding.
-keep class io.flutter.app.** { *; }
-keep class io.flutter.plugin.** { *; }
-keep class io.flutter.util.** { *; }
-keep class io.flutter.view.** { *; }
-keep class io.flutter.embedding.** { *; }
-keep class io.flutter.embedding.engine.** { *; }
-keep class io.flutter.plugin.common.** { *; }
-keep class io.flutter.plugins.** { *; }
-keep class io.flutter.plugin.editing.** { *; }
-keep class io.flutter.embedding.android.** { *; }

-dontwarn io.flutter.**

# flutter_local_notifications: receivers are referenced from the manifest.
-keep class com.dexterous.flutterlocalnotifications.** { *; }

# flutter_secure_storage (Android Keystore / platform channel).
-keep class com.it_nomads.fluttersecurestorage.** { *; }

# Keep annotations (R8 may strip @Keep, @RequiresApi, etc. used by plugins).
-keepattributes *Annotation*

# Supabase / GoTrue / PostgREST client JVM side is Dart-only; keep serializers.
-keep class io.supabase.** { *; }
-keep class com.auth0.jwt.** { *; }

# Avoid harmless missing-class warnings from optional plugin integrations.
-dontwarn org.slf4j.**
-dontwarn okhttp3.internal.**
-dontwarn javax.annotation.**
