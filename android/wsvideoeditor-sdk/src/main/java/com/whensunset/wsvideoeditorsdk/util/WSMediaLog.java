package com.whensunset.wsvideoeditorsdk.util;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import com.whensunset.wsvideoeditorsdk.model.EditorProject;

/**
 * 视频编辑 Sdk 的日志类
 */
public class WSMediaLog {

  private static final String NULL_STRING = "null";

  public static int i(String tag, String msg) {
    return Log.i(tag, msg);
  }

  public static int w(String tag, String msg) {
    return Log.w(tag, msg);
  }

  public static int e(String tag, String msg) {
    return Log.e(tag, msg);
  }

  public static int e(String tag, String msg, Throwable tr) {
    return Log.e(tag, msg, tr);
  }

  @NonNull
  public static String projectKeyParamToString(@Nullable EditorProject editorProject) {
    if (editorProject == null) {
      return NULL_STRING;
    }
    // todo 后续 project 字段多了，再只取关键参数
    return editorProject.toString();
  }
}
