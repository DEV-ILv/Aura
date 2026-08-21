import 'package:aura_companion/widgets/glass_card.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('GlassCard invokes onTap when tapped', (tester) async {
    var tapped = 0;
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: Center(
            child: GlassCard(
              blur: false,
              onTap: () => tapped++,
              child: const Text('Tile'),
            ),
          ),
        ),
      ),
    );

    await tester.tap(find.text('Tile'));
    await tester.pumpAndSettle();

    expect(tapped, 1, reason: 'onTap must fire when the card is tapped');
  });

  testWidgets('GlassCard without onTap stays inert', (tester) async {
    await tester.pumpWidget(
      const MaterialApp(
        home: Scaffold(
          body: Center(child: GlassCard(blur: false, child: Text('Panel'))),
        ),
      ),
    );

    await tester.tap(find.text('Panel'));
    await tester.pumpAndSettle();
    expect(tester.takeException(), isNull);
  });
}
