import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:native_add/native_add.dart';

void main() {
  const MethodChannel channel = MethodChannel('native_add');

  TestWidgetsFlutterBinding.ensureInitialized();

  setUp(() {
    channel.setMockMethodCallHandler((MethodCall methodCall) async {
      return '42';
    });
  });

  tearDown(() {
    channel.setMockMethodCallHandler(null);
  });

  test('getPlatformVersion', () async {
    expect(await NativeAdd.platformVersion, '42');
  });
}
