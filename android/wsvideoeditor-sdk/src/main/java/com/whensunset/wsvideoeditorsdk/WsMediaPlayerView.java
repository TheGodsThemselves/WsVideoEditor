package com.whensunset.wsvideoeditorsdk;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.AttributeSet;
import android.view.TextureView;

import com.whensunset.wsvideoeditorsdk.util.WSMediaLog;
import com.whensunset.wsvideoeditorsdk.video.PlayerGLThread;

import static com.whensunset.wsvideoeditorsdk.WsVideoEditorUtils.checkMainThread;

public class WsMediaPlayerView extends TextureView implements TextureView.SurfaceTextureListener {
  private static final String TAG = "WsMediaPlayerView";

  @Nullable
  private WsMediaPlayer mPlayer;

  private PlayerGLThread mPlayerGLThread;

  public WsMediaPlayerView(Context context) {
    super(context.getApplicationContext());
    init();
  }

  public WsMediaPlayerView(Context context, AttributeSet attrs) {
    super(context.getApplicationContext(), attrs);
    init();
  }

  private void init() {
    WSMediaLog.i(TAG, "init");
    setOpaque(false);
    setSurfaceTextureListener(this);
  }

  @UiThread
  public void setPreviewPlayer(@Nullable final WsMediaPlayer player) {
    checkMainThread();
    if (mPlayerGLThread != null) {
      mPlayerGLThread.onPlayerChange(player);
      mPlayerGLThread.setRenderPaused(true);

      if (!mPlayerGLThread.isAlive() && mPlayerGLThread.getState() == Thread.State.NEW) {
        mPlayerGLThread.start();
      }
      mPlayerGLThread.setRenderPaused(false);
    }
    mPlayer = player;
    WSMediaLog.i(TAG, "setPreviewPlayer mPlayerGLThread:" + mPlayerGLThread);
  }

  @UiThread
  public void onResume() {
    checkMainThread();
    if (mPlayerGLThread != null) {
      mPlayerGLThread.setRenderPaused(false);
    }
    WSMediaLog.i(TAG, "onResume mPlayerGLThread:" + mPlayerGLThread);
  }

  @Override
  public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
    mPlayerGLThread = new PlayerGLThread(surface, mPlayer);
    mPlayerGLThread.setName(TAG);
    mPlayerGLThread.onWindowResize(width, height);
    if (mPlayer != null) {
      mPlayerGLThread.setRenderPaused(false);
      mPlayerGLThread.start();
    }
    WSMediaLog.i(TAG, "onSurfaceTextureAvailable width:" + width + ",height:" + height);
  }

  @Override
  public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
    mPlayerGLThread.onWindowResize(width, height);
    WSMediaLog.i(TAG, "onSurfaceTextureSizeChanged width:" + width + ",height:" + height);
  }

  @UiThread
  public void onPause() {
    checkMainThread();
    if (mPlayerGLThread != null) {
      mPlayerGLThread.setRenderPaused(true);
    }
    WSMediaLog.i(TAG, "onPause mPlayerGLThread:" + mPlayerGLThread);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
    mPlayerGLThread.setRenderPaused(true);
    mPlayerGLThread.finish();
    mPlayerGLThread = null;
    WSMediaLog.i(TAG, "onSurfaceTextureDestroyed");
    return true;
  }

  @Nullable
  public WsMediaPlayer getPlayer() {
    return mPlayer;
  }

  @Override
  public void onSurfaceTextureUpdated(SurfaceTexture surface) {
  }
}
