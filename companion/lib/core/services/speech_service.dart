import 'dart:async';

import 'package:speech_to_text/speech_to_text.dart' as stt;

import 'logger.dart';

/// Result of a single push-to-talk capture.
class SpeechResult {
  const SpeechResult({
    required this.recognizedWords,
    required this.confidence,
    required this.finalResult,
  });

  final String recognizedWords;
  final double confidence;
  final bool finalResult;
}

/// Thin wrapper over the platform speech-to-text plugin.
///
/// Provides a stable interface plus lifecycle-safe cancellation so the rest
/// of the app never touches plugin APIs directly.
class SpeechService {
  SpeechService() {
    _speech = stt.SpeechToText();
  }

  late final stt.SpeechToText _speech;
  final StreamController<SpeechResult> _results = StreamController.broadcast();
  bool _available = false;
  bool _listening = false;
  String _lastFinalText = '';

  Stream<SpeechResult> get results => _results.stream;

  bool get isListening => _listening;

  bool get isAvailable => _available;

  /// Initialises the recogniser and requests microphone permission.
  Future<bool> initialize() async {
    _available = await _speech.initialize(
      onStatus: _onStatus,
      onError: (error) => Logger.warning('Speech error: ${error.errorMsg}'),
    );
    return _available;
  }

  void _onStatus(String status) {
    if (status == 'done' || status == 'notListening') {
      _listening = false;
    } else if (status == 'listening') {
      _listening = true;
    }
  }

  /// Starts listening. Non-final partial results are streamed as they come.
  Future<void> start() async {
    if (!_available) {
      await initialize();
    }
    if (_listening) {
      return;
    }
    await _speech.listen(
      onResult: (result) {
        if (result.finalResult) {
          _lastFinalText = result.recognizedWords;
        }
        _results.add(
          SpeechResult(
            recognizedWords: result.recognizedWords,
            confidence: result.confidence,
            finalResult: result.finalResult,
          ),
        );
      },
      listenOptions: stt.SpeechListenOptions(
        listenMode: stt.ListenMode.dictation,
      ),
    );
  }

  /// Stops listening and returns the last finalised recognised text.
  Future<String> stop() async {
    await _speech.stop();
    _listening = false;
    return _lastFinalText;
  }

  /// Cancels and discards the current capture.
  Future<void> cancel() async {
    await _speech.cancel();
    _listening = false;
  }

  /// Releases recogniser resources.
  Future<void> dispose() async {
    await _speech.cancel();
    await _results.close();
  }
}
