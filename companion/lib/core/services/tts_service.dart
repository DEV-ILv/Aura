import 'package:flutter/services.dart';
import 'package:flutter_tts/flutter_tts.dart';

import 'logger.dart';

/// Thin wrapper over the platform text-to-speech plugin.
class TtsService {
  TtsService() {
    _tts = FlutterTts();
    _configure();
  }

  late final FlutterTts _tts;
  bool _speaking = false;

  bool get isSpeaking => _speaking;

  Future<void> _configure() async {
    try {
      await _tts.setLanguage('en-US');
      await _tts.setPitch(1.0);
      await _tts.setSpeechRate(0.5);
      await _tts.awaitSpeakCompletion(true);
      _tts.setCompletionHandler(() => _speaking = false);
      _tts.setErrorHandler((message) {
        Logger.warning('TTS error: $message');
        _speaking = false;
      });
    } on PlatformException catch (error) {
      Logger.warning('TTS configure failed: ${error.message}');
    }
  }

  /// Speaks [text], interrupting any current utterance.
  Future<void> speak(String text) async {
    if (text.trim().isEmpty) {
      return;
    }
    try {
      _speaking = true;
      await _tts.stop();
      await _tts.speak(text.trim());
    } on PlatformException catch (error) {
      Logger.warning('TTS speak failed: ${error.message}');
      _speaking = false;
    }
  }

  /// Stops any current utterance.
  Future<void> stop() async {
    try {
      await _tts.stop();
    } on PlatformException {
      // Best effort.
    }
    _speaking = false;
  }

  /// Releases TTS resources.
  Future<void> dispose() async {
    await _tts.stop();
  }
}
