package com.whensunset.wsvideoeditortest;

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.TextView;

import com.whensunset.wsvideoeditorsdk.WsVideoEditorUtils;
import com.whensunset.wsvideoeditorsdk.inner.VideoDecoderService;
import com.whensunset.wsvideoeditorsdk.model.EditorProject;
import com.whensunset.wsvideoeditorsdk.model.MediaAsset;

import java.util.concurrent.atomic.AtomicLong;

import pub.devrel.easypermissions.EasyPermissions;

public class TestActivity extends Activity {
  
  @Override
  protected void onCreate(@Nullable Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.test_activity);
    WsVideoEditorUtils.initJni();
    if (!EasyPermissions.hasPermissions(this, Manifest.permission.CAMERA)) {
      EasyPermissions.requestPermissions(this, "权限", 1,
          Manifest.permission.READ_EXTERNAL_STORAGE);
    }
    initButton();
  }
  
  TextView showState;
  TextView showFrame;
  VideoDecoderService mVideoDecoderService;
  AtomicLong timestamp = new AtomicLong(0);
  
  private void initButton() {
    showState = findViewById(R.id.show_state);
    showFrame = findViewById(R.id.show_frame);
    mVideoDecoderService = new VideoDecoderService();
  
    final EditorProject.Builder videoEditorProjectBuilder = EditorProject.newBuilder();
    MediaAsset.Builder builder = MediaAsset.newBuilder();
    builder.setAssetId(System.currentTimeMillis()).setAssetPath("/sdcard/test.mp4")
        .setVolume(1.0);
    videoEditorProjectBuilder.addMediaAsset(builder.build());
  
    final StringBuilder stringBuilder = new StringBuilder();
    findViewById(R.id.start).setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        stringBuilder.delete(0, stringBuilder.toString().length());
        mVideoDecoderService.setProject(0, videoEditorProjectBuilder.build());
        mVideoDecoderService.start();
      }
    });
    
    findViewById(R.id.seek).setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        stringBuilder.delete(0, stringBuilder.toString().length());
        timestamp.set(3000);
        mVideoDecoderService.seek(3);
      }
    });
    
    findViewById(R.id.stop).setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        stringBuilder.delete(0, stringBuilder.toString().length());
        mVideoDecoderService.stop();
      }
    });
  
    new Thread(new Runnable() {
      @Override
      public void run() {
        int size = 0;
        while (true) {
          if (mVideoDecoderService.stopped() || mVideoDecoderService.ended()) {
            continue;
          }
          long startTime = System.currentTimeMillis();
          final String frameString = mVideoDecoderService.getRenderFrame(timestamp.get() * 1f / 1000);
          final long costTime = System.currentTimeMillis() - startTime;
          final String finalFrameString = frameString + ",costTime:" + costTime + ",timestamp:" + timestamp.get() + ",size:" + size + "\n\n";
          size++;
          timestamp.set(timestamp.get() + 30);
          try {
            Thread.sleep(30);
          } catch (InterruptedException e) {
            e.printStackTrace();
          }
          showFrame.post(new Runnable() {
            @Override
            public void run() {
              stringBuilder.append(finalFrameString);
              showFrame.setText(stringBuilder.toString());
            }
          });
        }
      }
    }).start();
    
    new Thread(new Runnable() {
      @Override
      public void run() {
        while (true) {
          showState.post(new Runnable() {
            @Override
            public void run() {
              showState.setText("ended:" + mVideoDecoderService.ended() + ",stopped:" + mVideoDecoderService.stopped() + ",frameCount:" + mVideoDecoderService.getBufferedFrameCount());
            }
          });
          try {
            Thread.sleep(200);
          } catch (InterruptedException e) {
            e.printStackTrace();
          }
        }
      }
    }).start();
  }
}
