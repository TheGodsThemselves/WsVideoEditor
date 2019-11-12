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
    EditorProject.Builder videoEditorProjectBuilder = EditorProject.newBuilder();
    MediaAsset.Builder builder = MediaAsset.newBuilder();
    builder.setAssetId(System.currentTimeMillis()).setAssetPath("/sdcard/test.mp4")
        .setVolume(1.0);
    videoEditorProjectBuilder.addMediaAsset(builder.build());
    mVideoDecoderService.setProject(0.1, videoEditorProjectBuilder.build());
    
    findViewById(R.id.start).setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        mVideoDecoderService.start();
      }
    });
    
    findViewById(R.id.seek).setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        mVideoDecoderService.seek(3);
      }
    });
    
    findViewById(R.id.stop).setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        mVideoDecoderService.stop();
      }
    });
    
    new Thread(new Runnable() {
      @Override
      public void run() {
        while (true) {
          showFrame.post(new Runnable() {
            @Override
            public void run() {
//              showFrame.setText(mVideoDecoderService.getRenderFrame(timestamp.get()));
            }
          });
          timestamp.set(timestamp.get() + 30);
          try {
            Thread.sleep(30);
          } catch (InterruptedException e) {
            e.printStackTrace();
          }
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
