import 'package:aura_companion/websocket/websocket_service.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('repeated connect failures are handled, not thrown', () async {
    final service = WebSocketService();
    service.setEnabled(true);

    // No WS server is listening, so every attempt is refused. Any unhandled
    // async error escaping here would fail the test via the test zone.
    service.configure('ws://127.0.0.1:1');

    await Future<void>.delayed(const Duration(milliseconds: 400));

    service.dispose();
    expect(service.isEnabled, isTrue);
  });
}
