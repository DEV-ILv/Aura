import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../core/services/logger.dart';
import '../core/services/speech_service.dart';
import '../core/services/tts_service.dart';
import 'app_providers.dart';
import 'chat_provider.dart';

/// Phase of push-to-talk interaction.
enum VoicePhase { idle, listening, processing, speaking }

/// Voice chat state.
class VoiceState {
  const VoiceState({
    this.phase = VoicePhase.idle,
    this.transcript = '',
    this.recognizedText = '',
    this.error,
  });

  final VoicePhase phase;
  final String transcript;

  /// Latest finalised recognised text ready to send.
  final String recognizedText;
  final String? error;

  bool get isListening => phase == VoicePhase.listening;

  VoiceState copyWith({
    VoicePhase? phase,
    String? transcript,
    String? recognizedText,
    String? error,
  }) {
    return VoiceState(
      phase: phase ?? this.phase,
      transcript: transcript ?? this.transcript,
      recognizedText: recognizedText ?? this.recognizedText,
      error: error,
    );
  }
}

/// Owns the push-to-talk voice loop: STT -> chat -> TTS.
///
/// Kept streaming-ready: processing latches the transcript, then hands off to
/// the chat pipeline. Replies are spoken via TTS automatically.
class VoiceNotifier extends StateNotifier<VoiceState> {
  VoiceNotifier(this._speech, this._tts, this._chatNotifier)
    : super(const VoiceState()) {
    _sub = _speech.results.listen((result) {
      if (result.finalResult) {
        state = state.copyWith(
          recognizedText: result.recognizedWords,
          transcript: result.recognizedWords,
        );
      } else if (result.recognizedWords.isNotEmpty) {
        state = state.copyWith(transcript: result.recognizedWords);
      }
    });
  }

  final SpeechService _speech;
  final TtsService _tts;
  final ChatNotifier _chatNotifier;
  StreamSubscription<dynamic>? _sub;
  String _sessionBuffer = '';

  /// Starts listening (hold to talk).
  Future<void> startListening() async {
    bool available;
    try {
      available = await _speech.initialize();
    } catch (error) {
      Logger.warning('Speech initialization failed: $error');
      state = state.copyWith(
        error: 'Microphone or speech recognition is unavailable.',
      );
      return;
    }
    if (!available) {
      state = state.copyWith(
        error: 'Microphone or speech recognition is unavailable.',
      );
      return;
    }
    _sessionBuffer = '';
    state = state.copyWith(
      phase: VoicePhase.listening,
      transcript: '',
      recognizedText: '',
    );
    try {
      await _speech.start();
    } catch (error) {
      Logger.warning('Speech start failed: $error');
      state = state.copyWith(
        phase: VoicePhase.idle,
        error: 'Could not start the microphone.',
      );
    }
  }

  /// Stops listening and, if text was captured, sends it to the assistant.
  Future<void> stopListening() async {
    var text = '';
    try {
      text = await _speech.stop();
    } catch (error) {
      Logger.warning('Speech stop failed: $error');
      // Fall through with any recognised text already captured.
    }
    if (text.isNotEmpty) {
      _sessionBuffer = text;
    }
    final finalText = _sessionBuffer.isNotEmpty
        ? _sessionBuffer
        : state.recognizedText;
    if (finalText.trim().isEmpty) {
      state = state.copyWith(phase: VoicePhase.idle);
      return;
    }
    await _send(finalText);
  }

  /// Cancels the current capture without sending.
  Future<void> cancelListening() async {
    try {
      await _speech.cancel();
    } catch (error) {
      Logger.warning('Speech cancel failed: $error');
    }
    _sessionBuffer = '';
    state = state.copyWith(
      phase: VoicePhase.idle,
      transcript: '',
      recognizedText: '',
    );
  }

  Future<void> _send(String text) async {
    state = state.copyWith(phase: VoicePhase.processing, transcript: text);
    await _chatNotifier.send(text);
    final reply = _lastAssistantReply();
    if (reply != null && reply.isNotEmpty) {
      await _speak(reply);
    } else {
      state = state.copyWith(phase: VoicePhase.idle);
    }
  }

  String? _lastAssistantReply() {
    final messages = _chatNotifier.state.messages;
    if (messages.isEmpty) {
      return null;
    }
    final last = messages.last;
    return last.isAssistant && !last.content.startsWith('⚠')
        ? last.content
        : null;
  }

  Future<void> _speak(String text) async {
    state = state.copyWith(phase: VoicePhase.speaking);
    try {
      await _tts.speak(text);
    } catch (_) {
      Logger.warning('TTS failed during voice reply');
    }
    state = state.copyWith(phase: VoicePhase.idle);
  }

  /// Stops any in-progress speech.
  Future<void> stopSpeaking() async {
    await _tts.stop();
    state = state.copyWith(phase: VoicePhase.idle);
  }

  @override
  void dispose() {
    _sub?.cancel();
    _tts.stop();
    super.dispose();
  }
}

final voiceProvider = StateNotifierProvider<VoiceNotifier, VoiceState>((ref) {
  return VoiceNotifier(
    ref.watch(speechServiceProvider),
    ref.watch(ttsServiceProvider),
    ref.watch(chatProvider.notifier),
  );
});
