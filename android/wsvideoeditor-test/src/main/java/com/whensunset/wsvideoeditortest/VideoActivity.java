package com.whensunset.wsvideoeditortest;

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.Button;

import com.whensunset.wsvideoeditorsdk.WsMediaPlayer;
import com.whensunset.wsvideoeditorsdk.WsMediaPlayerView;
import com.whensunset.wsvideoeditorsdk.WsVideoEditorUtils;
import com.whensunset.wsvideoeditorsdk.model.EditorProject;
import com.whensunset.wsvideoeditorsdk.model.MediaAsset;

import pub.devrel.easypermissions.EasyPermissions;

public class VideoActivity extends Activity {

  private WsMediaPlayerView mPreviewView;
  private WsMediaPlayer mPlayer = null;
  private Button playBtn;

  private void updatePlayStateShow() {
    if (mPlayer != null) {
      if (!mPlayer.isPlaying()) {
        playBtn.setText("Play");
      } else {
        playBtn.setText("Pause");
      }
    }
  }

  @Override
  protected void onStop() {
    super.onStop();
    if (mPlayer != null) {
      mPlayer.pause();
    }
  }

  @Override
  protected void onCreate(@Nullable Bundle savedInstanceState) {
    try {
      super.onCreate(savedInstanceState);
      if (!EasyPermissions.hasPermissions(this, Manifest.permission.CAMERA)) {
        EasyPermissions.requestPermissions(this, "权限", 1,
            Manifest.permission.READ_EXTERNAL_STORAGE);
      }

      WsVideoEditorUtils.initJni();
      setContentView(R.layout.video_activity);
      mPreviewView = findViewById(R.id.ws_media_player_view);
      EditorProject.Builder videoEditorProjectBuilder = EditorProject.newBuilder();
      MediaAsset.Builder trackAssetBuilder = MediaAsset.newBuilder();
      trackAssetBuilder.setAssetId(System.currentTimeMillis()).setAssetPath("/sdcard/test.mp4").setVolume(1.0);
      videoEditorProjectBuilder.addMediaAsset(trackAssetBuilder.build()).setBlurPaddingArea(true);

      mPlayer = new WsMediaPlayer();
      mPlayer.setProject(videoEditorProjectBuilder.build());

      try {
        mPlayer.loadProject();
      } catch (Exception e) {
        e.printStackTrace();
        throw new RuntimeException(e);
      }

      mPreviewView.setPreviewPlayer(mPlayer);
      mPlayer.play();

      playBtn = (Button) findViewById(R.id.btnPlayVideo);
      playBtn.setOnClickListener(new View.OnClickListener() {
        @Override
        public void onClick(View view) {
          if (mPlayer.isPlaying()) {
            mPlayer.pause();
          } else {
            mPlayer.play();
          }
          updatePlayStateShow();
        }
      });
      updatePlayStateShow();

    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  @Override
  protected void onDestroy() {
    if (mPlayer != null) {
      mPreviewView.onPause();
      mPreviewView.setPreviewPlayer(null);
      mPlayer.release();
    }
    mPreviewView.onPause();
    super.onDestroy();
  }

  @Override
  protected void onPause() {
    super.onPause();
    mPreviewView.onPause();
  }

  @Override
  protected void onResume() {
    super.onResume();
    mPreviewView.onResume();
  }
}
