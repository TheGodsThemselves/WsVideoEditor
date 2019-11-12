package com.whensunset.wsvideoeditorsdk;

import android.support.annotation.Keep;
import android.util.Log;

public class WsVideoEditorUtils {

  private static final String TAG = "WsVideoEditorUtils";

  public static void initJni() {
    System.loadLibrary("wsvideoeditorsdk");
    initJniNative();
  }

  @Keep
  private static void nativeCallDebugLogger(int priority, String tag, String message) {
    switch (priority) {
      case Log.VERBOSE:
        Log.v(tag, message, null);
        break;
      case Log.DEBUG:
        Log.d(tag, message, null);
        break;
      case Log.INFO:
        Log.i(tag, message, null);
        break;
      case Log.WARN:
        Log.w(tag, message, null);
        break;
      case Log.ERROR:
        Log.e(tag, message, null);
        break;
      default:
        Log.v(tag, message, null);
        break;
    }
  }
  public static native void initJniNative();
}
