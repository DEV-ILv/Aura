import 'package:aura_companion/core/utils/formatters.dart';
import 'package:aura_companion/core/utils/validators.dart';
import 'package:aura_companion/models/battery_info.dart';
import 'package:aura_companion/models/chat_message.dart';
import 'package:aura_companion/models/system_status.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('Validators', () {
    test('accepts valid IPv4 addresses', () {
      expect(Validators.hostname('192.168.4.1'), isNull);
      expect(Validators.hostname('10.0.0.255'), isNull);
    });

    test('rejects out-of-range octets', () {
      expect(Validators.hostname('256.0.0.1'), isNotNull);
      expect(Validators.hostname('999.1.1.1'), isNotNull);
    });

    test('accepts hostnames', () {
      expect(Validators.hostname('aura.local'), isNull);
    });

    test('rejects empty values', () {
      expect(Validators.hostname(''), isNotNull);
      expect(Validators.port(''), isNotNull);
      expect(Validators.port('0'), isNotNull);
      expect(Validators.port('65536'), isNotNull);
    });

    test('accepts a valid port', () {
      expect(Validators.port('80'), isNull);
      expect(Validators.port('65535'), isNull);
    });
  });

  group('Formatters', () {
    test('formats byte sizes', () {
      expect(Formatters.bytes(512), '512 B');
      expect(Formatters.bytes(2048), contains(' KB'));
      expect(Formatters.bytes(1048576), contains(' MB'));
    });

    test('formats uptime', () {
      expect(Formatters.uptime(45), '45s');
      expect(Formatters.uptime(3600), '1h 0m 0s');
      expect(Formatters.uptime(90000), '1d 1h 0m');
    });

    test('labels signal strength', () {
      expect(Formatters.signalLabel(-40), 'Excellent');
      expect(Formatters.signalLabel(-55), 'Good');
      expect(Formatters.signalLabel(-65), 'Fair');
      expect(Formatters.signalLabel(-80), 'Weak');
    });
  });

  group('Models', () {
    test('SystemStatus parses JSON', () {
      final status = SystemStatus.fromJson({
        'running': true,
        'uptime': 120,
        'heap_free': 200000,
        'wifi_connected': true,
        'requests': 42,
      });
      expect(status.running, isTrue);
      expect(status.uptimeSeconds, 120);
      expect(status.wifiConnected, isTrue);
      expect(status.requestCount, 42);
    });

    test('ChatMessage defaults to assistant role', () {
      final message = ChatMessage.fromJson({'content': 'hi'});
      expect(message.isAssistant, isTrue);
    });

    test('BatteryInfo reports presence', () {
      const empty = BatteryInfo.unknown();
      expect(empty.isPresent, isFalse);
      const charged = BatteryInfo(level: 50, voltage: 3.7);
      expect(charged.isPresent, isTrue);
    });
  });
}
