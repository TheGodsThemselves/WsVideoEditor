package com.whensunset.wsvideoeditortest;

import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.Nullable;

import com.whensunset.wsvideoeditorsdk.WsVideoEditorUtils;

public class TestActivity extends Activity {
  @Override
  protected void onCreate(@Nullable Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.test_activity);
    WsVideoEditorUtils.initJni();
  }
}
