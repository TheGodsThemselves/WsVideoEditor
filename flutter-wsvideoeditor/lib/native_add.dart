import 'dart:async';
import 'dart:ffi';
import 'dart:io';

import 'package:flutter/services.dart';

class NativeAdd {
  static const MethodChannel _channel = const MethodChannel('native_add');

  static Future<String> get platformVersion async {
    final String version = await _channel.invokeMethod('getPlatformVersion');
    return version;
  }

  static int add(int a, int b) {
    return 1;
  }
}

final DynamicLibrary dylib = Platform.isAndroid
    ? DynamicLibrary.open("libwsvideoeditorsdk.so")
    : DynamicLibrary.open("native_add.framework/native_add");

final int Function(int x, int y) nativeAdd = dylib
    .lookup<NativeFunction<Int32 Function(Int32, Int32)>>("native_add")
    .asFunction();

final void Function() initsdk = dylib
    .lookup<NativeFunction<Void Function()>>
  ("Java_com_whensunset_wsvideoeditorsdk_WsVideoEditorUtils_initJniNatives")
    .asFunction();
