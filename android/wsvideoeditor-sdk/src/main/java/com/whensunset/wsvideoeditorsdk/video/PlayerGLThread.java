package com.whensunset.wsvideoeditorsdk.video;

import android.graphics.SurfaceTexture;
import android.opengl.GLES20;
import android.opengl.GLUtils;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;

import com.whensunset.wsvideoeditorsdk.WsMediaPlayer;
import com.whensunset.wsvideoeditorsdk.util.WSMediaLog;

import java.util.concurrent.locks.LockSupport;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL;

public class PlayerGLThread extends Thread {
  private static final String TAG = "PlayerGLThread";
  static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
  static final int EGL_OPENGL_ES2_BIT = 4;

  private final Object mLock;
  private final SurfaceTexture mSurfaceTexture;
  private EGL10 mEGL10;
  private EGLDisplay mEGLDisplay;
  private EGLConfig mEGLConfig;
  private EGLContext mEGLContext;
  private EGLSurface mEGLSurface;
  private GL mGL;

  private volatile int mWidth = -1;
  private volatile int mHeight = -1;
  private volatile boolean mRenderPaused = true;
  private volatile boolean mSizeChanged = true;
  private volatile boolean mPlayerChanged = true;
  private volatile boolean mFinished = false;
  private WsMediaPlayer mPlayer;

  public PlayerGLThread(SurfaceTexture mSurfaceTexture, WsMediaPlayer player) {
    this.mSurfaceTexture = mSurfaceTexture;
    mPlayer = player;
    mLock = new Object();
  }

  @Override
  public void run() {
    try {
      runInternal();
    } finally {
      try {
        if (mPlayer != null && mPlayer.isAttached()) {
          mPlayer.onDetachedView();
        }
      } catch (Error e) {
        WSMediaLog.e("Error detaching player! " + e, "");
      }
    }
  }

  @WorkerThread
  private void runInternal() {
    while (!mFinished) {
      long renderStartMillis = SystemClock.elapsedRealtime();
      try {
        synchronized (mLock) {
          if (mFinished) {
            WSMediaLog.i(TAG, "runInternal finished!");
            break;
          }

          if (mPlayerChanged && mPlayer != null) {
            finishGL();
            try {
              initGLSharedWithPlayer();
            } catch (Exception e) {
              WSMediaLog.e(TAG, "Error initializing GL, exiting thread", e);
              return;
            }
            if (mGL == null || mEGLConfig == null) {
              WSMediaLog.e(TAG, "Error initializing GL, mGL || mEGLConfig is null, exiting thread");
            }
            createSurface();
            mPlayer.onAttachedView(mWidth, mHeight);
            mPlayerChanged = false;
            mSizeChanged = false;
            WSMediaLog.i(TAG,
                "runInternal playerChanged:" + mPlayerChanged + ",mWidth:" + mWidth + ",mHeight:" + mHeight);
          } else if (mSizeChanged) {
            createSurface();
            mPlayer.onAttachedView(mWidth, mHeight);
            mSizeChanged = false;
            WSMediaLog.i(TAG, "runInternal mSizeChanged mWidth:" + mWidth + ",mHeight:" + mHeight);
          }

          if (mRenderPaused) {
            continue;
          }
        }

        if (!checkCurrent()) {
          WSMediaLog.e(TAG, "checkCurrent failed!");
          break;
        }

        if (mPlayer != null) {
          if (!mPlayer.isAttached()) {
            mPlayer.onAttachedView(mWidth, mHeight);
            WSMediaLog.i(TAG,
                "runInternal player attached mWidth:" + mWidth + ",mHeight:" + mHeight);
          }
          if (!mRenderPaused) {
            mPlayer.drawFrame();
          }
          if (mPlayer.getReadyState() < 2) {
            // Do not call eglSwapBuffers if WsMediaPlayer hasn't draw anything
            continue;
          }
        } else {
          GLES20.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
          GLES20.glClear(GLES20.GL_DEPTH_BUFFER_BIT | GLES20.GL_COLOR_BUFFER_BIT);
          mEGL10.eglSwapBuffers(mEGLDisplay, mEGLSurface);
          continue;
        }

        if (!mEGL10.eglSwapBuffers(mEGLDisplay, mEGLSurface)) {
          WSMediaLog.e(TAG, "Cannot swap buffers");
          break;
        }
      } finally {
        long renderEndMillis = SystemClock.elapsedRealtime();
        final long FRAME_INTERNAL = 33;
        final long SLEEP_BREATHING_ROOM = 10;
        if (renderEndMillis - renderStartMillis < FRAME_INTERNAL) {
          if (FRAME_INTERNAL - (renderEndMillis - renderStartMillis) > SLEEP_BREATHING_ROOM) {
            try {
              Thread.sleep(FRAME_INTERNAL - (renderEndMillis - renderStartMillis) - SLEEP_BREATHING_ROOM);
            } catch (InterruptedException e) {
              break;
            }
          }
          // Thread.sleep is likely to sleep longer than it should
          // A 5ms breathing room is left, after Thread.sleep returns, use LockSupport.parkNanos
          // to sleep for the remaining duration

          // it was meant to be "while (true);", use a for to prevent dead loop
          for (int i = 0; i < 5; ++i) {
            renderEndMillis = SystemClock.elapsedRealtime();
            if (renderEndMillis - renderStartMillis < FRAME_INTERNAL) {
              LockSupport.parkNanos((FRAME_INTERNAL - (renderEndMillis - renderStartMillis)) * 1000000 / 2);
            } else {
              break;
            }
          }
        }
      }
    }
    finishGL();
  }


  public void onWindowResize(int w, int h) {
    WSMediaLog.i(TAG,
        "onWindowResize w:" + w + ",h:" + h + ",mWidth:" + mWidth + ",mHeight:" + mHeight);
    synchronized (mLock) {
      if (mWidth != w || mHeight != h) {
        mWidth = w;
        mHeight = h;
        mSizeChanged = true;
      }
    }
  }

  public void onPlayerChange(@Nullable final WsMediaPlayer player) {
    WSMediaLog.i(TAG, "onPlayerChange player:" + player + ",mPlayer:" + mPlayer);
    synchronized (mLock) {
      if (player != mPlayer) {
        mPlayer = player;
        mPlayerChanged = true;
      }
    }
  }

  public void finish() {
    mFinished = true;
  }

  public void setRenderPaused(boolean renderPaused) {
    WSMediaLog.i(TAG,
        "setRenderPaused renderPaused:" + renderPaused + ",mRenderPaused:" + mRenderPaused);
    mRenderPaused = renderPaused;
  }

  @WorkerThread
  private void createSurface() {
    if (mEGL10 == null) {
      throw new RuntimeException("mEGL10 not initialized");
    }
    if (mEGLDisplay == null) {
      throw new RuntimeException("mEGLDisplay not initialized");
    }
    if (mEGLConfig == null) {
      throw new RuntimeException("mEGLConfig not initialized");
    }

    WSMediaLog.i(TAG, "createSurface");
    destroySurface();

    try {
      mEGLSurface = mEGL10.eglCreateWindowSurface(mEGLDisplay, mEGLConfig, mSurfaceTexture, null);
    } catch (IllegalArgumentException e) {
      WSMediaLog.e(TAG, "eglCreateWindowSurface", e);
      return;
    }


    if (mEGLSurface == null || mEGLSurface == EGL10.EGL_NO_SURFACE) {
      int error = mEGL10.eglGetError();
      if (error == EGL10.EGL_BAD_NATIVE_WINDOW) {
        WSMediaLog.e(TAG, "createWindowSurface returned EGL_BAD_NATIVE_WINDOW.");
      }
      return;
    }

    if (!mEGL10.eglMakeCurrent(mEGLDisplay, mEGLSurface, mEGLSurface, mEGLContext)) {
      WSMediaLog.e(TAG, "eglMakeCurrent failed " + GLUtils.getEGLErrorString(mEGL10.eglGetError()));
    }
  }

  @WorkerThread
  private void destroySurface() {
    if (mEGLSurface != null && mEGLSurface != EGL10.EGL_NO_SURFACE) {
      mEGL10.eglMakeCurrent(mEGLDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE,
          EGL10.EGL_NO_CONTEXT);
      mEGL10.eglDestroySurface(mEGLDisplay, mEGLSurface);
      mEGLSurface = null;
    }
  }

  @WorkerThread
  private boolean checkCurrent() {
    if (!mEGLContext.equals(mEGL10.eglGetCurrentContext()) || !mEGLSurface.equals(mEGL10.eglGetCurrentSurface(EGL10.EGL_DRAW))) {
      final int error = mEGL10.eglGetError();
      if (error != EGL10.EGL_SUCCESS) {
        WSMediaLog.e(TAG, "EGL error = 0x" + Integer.toHexString(error));
      }
      if (!mEGL10.eglMakeCurrent(mEGLDisplay, mEGLSurface, mEGLSurface, mEGLContext)) {
        return false;
      }
      if (error != EGL10.EGL_SUCCESS) {
        WSMediaLog.e(TAG, "EGL error = 0x" + Integer.toHexString(error));
      }
    }
    return true;
  }

  @WorkerThread
  private void initGLSharedWithPlayer() {
    if (mEGL10 != null) {
      finishGL();
    }

    mEGL10 = (EGL10) EGLContext.getEGL();

    mEGLDisplay = mEGL10.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
    if (mEGLDisplay == EGL10.EGL_NO_DISPLAY) {
      throw new RuntimeException("eglGetDisplay failed " + GLUtils.getEGLErrorString(mEGL10.eglGetError()));
    }

    int[] version = new int[2];
    if (!mEGL10.eglInitialize(mEGLDisplay, version)) {
      throw new RuntimeException("eglInitialize failed " + GLUtils.getEGLErrorString(mEGL10.eglGetError()));
    }

    mEGLConfig = chooseEglConfig();
    if (mEGLConfig == null) {
      throw new RuntimeException("mEGLConfig not initialized");
    }

    int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE};
    mEGLContext = mEGL10.eglCreateContext(mEGLDisplay, mEGLConfig, mEGL10.eglGetCurrentContext(),
        attrib_list);

    createSurface();

    if (!mEGL10.eglMakeCurrent(mEGLDisplay, mEGLSurface, mEGLSurface, mEGLContext)) {
      throw new RuntimeException("eglMakeCurrent failed " + GLUtils.getEGLErrorString(mEGL10.eglGetError()));
    }

    mGL = mEGLContext.getGL();
  }

  @WorkerThread
  private void finishGL() {
    if (mEGL10 != null) {
      mEGL10.eglDestroyContext(mEGLDisplay, mEGLContext);
      mEGL10.eglTerminate(mEGLDisplay);
      mEGL10.eglDestroySurface(mEGLDisplay, mEGLSurface);
      mEGLSurface = null;
      mEGL10 = null;
    }
  }

  @WorkerThread
  private EGLConfig chooseEglConfig() {
    int[] configsCount = new int[1];
    EGLConfig[] configs = new EGLConfig[1];
    int[] configSpec = getConfig();
    if (!mEGL10.eglChooseConfig(mEGLDisplay, configSpec, configs, 1, configsCount)) {
      throw new IllegalArgumentException("eglChooseConfig failed " + GLUtils.getEGLErrorString(mEGL10.eglGetError()));
    } else if (configsCount[0] > 0) {
      return configs[0];
    }
    return null;
  }

  @WorkerThread
  private int[] getConfig() {
    return new int[]{EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL10.EGL_RED_SIZE, 8,
        EGL10.EGL_GREEN_SIZE, 8, EGL10.EGL_BLUE_SIZE, 8, EGL10.EGL_ALPHA_SIZE, 8,
        EGL10.EGL_DEPTH_SIZE, 0, EGL10.EGL_STENCIL_SIZE, 0, EGL10.EGL_NONE};
  }
}
