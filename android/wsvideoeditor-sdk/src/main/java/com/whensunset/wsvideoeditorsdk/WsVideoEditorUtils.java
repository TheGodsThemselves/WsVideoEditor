package com.whensunset.wsvideoeditorsdk;

import android.os.Looper;
import android.support.annotation.Keep;
import android.util.Log;

public class WsVideoEditorUtils {

  private static final String TAG = "WsVideoEditorUtils";

  private static final String SDK_LOG_TAG = "WsVideoEditor TAG ";

  public static void initJni() {
    System.loadLibrary("wsvideoeditorsdk");
    initJniNative();
  }

  @Keep
  private static void nativeCallDebugLogger(int priority, String tag, String message) {
    switch (priority) {
      case Log.VERBOSE:
        Log.v(tag, SDK_LOG_TAG + message, null);
        break;
      case Log.DEBUG:
        Log.d(tag, SDK_LOG_TAG + message, null);
        break;
      case Log.INFO:
        Log.i(tag, SDK_LOG_TAG + message, null);
        break;
      case Log.WARN:
        Log.w(tag, SDK_LOG_TAG + message, null);
        break;
      case Log.ERROR:
        Log.e(tag, SDK_LOG_TAG + message, null);
        break;
      default:
        Log.v(tag, SDK_LOG_TAG + message, null);
        break;
    }
  }

  public static void checkMainThread() {
    if (Looper.getMainLooper().getThread() != Thread.currentThread()) {
      throw new RuntimeException(TAG + " checkMainThread error Thread.currentThread():" + Thread.currentThread());
    }
  }
  
  public static native void initJniNative();
}
