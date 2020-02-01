package com.whensunset.wsvideoeditorsdk;

import android.support.annotation.NonNull;

import com.google.protobuf.InvalidProtocolBufferException;
import com.whensunset.wsvideoeditorsdk.model.EditorProject;
import com.whensunset.wsvideoeditorsdk.model.MediaAsset;
import com.whensunset.wsvideoeditorsdk.model.jni.CreateProjectNativeReturnValue;
import com.whensunset.wsvideoeditorsdk.util.WSMediaLog;

import java.io.IOException;

import static com.whensunset.wsvideoeditorsdk.util.WSMediaLog.projectKeyParamToString;

/**
 * 播放器
 */
public class WsMediaPlayer {
  private final static String TAG = "AndroidWSMediaPlayer";
  
  private EditorProject mProject;
  private volatile long mNativePlayerAddress = 0;
  private final Object mLock = new Object();
  private Thread mAttachedThread;
  
  public WsMediaPlayer() {
    mNativePlayerAddress = newNativePlayer();
  }
  
  public void play() {
    WSMediaLog.i(TAG, "play mNativePlayerAddress:" + mNativePlayerAddress);
    if (mNativePlayerAddress == 0) {
      return;
    }
    playNative(mNativePlayerAddress);
  }
  
  public void pause() {
    WSMediaLog.i(TAG, "pause mNativePlayerAddress:" + mNativePlayerAddress);
    if (mNativePlayerAddress == 0) {
      return;
    }
    pauseNative(mNativePlayerAddress);
  }
  
  public boolean isPlaying() {
    if (mNativePlayerAddress == 0) {
      return false;
    }
    return isPlayingNative(mNativePlayerAddress);
  }
  
  public boolean isAttached() {
    synchronized (mLock) {
      return mAttachedThread != null;
    }
  }
  
  public void seek(double currentTime) {
    WSMediaLog.i(TAG,
        "pause mNativePlayerAddress:" + mNativePlayerAddress + ",currentTime:" + currentTime);
    if (mNativePlayerAddress == 0) {
      return;
    }
    seekNative(mNativePlayerAddress, currentTime);
  }
  
  /**
   * 将 Project 设置给底层，基本上不耗时
   *
   * @param project
   */
  public void setProject(@NonNull EditorProject project) {
    WSMediaLog.i(TAG, "setProject mNativePlayerAddress:" + mNativePlayerAddress + ",mProject:"
        + projectKeyParamToString(mProject) + ",project:" + projectKeyParamToString(project));
    synchronized (mLock) {
      if (mNativePlayerAddress == 0) {
        return;
      }
      mProject = project;
      setProjectNative(mNativePlayerAddress, mProject.toByteArray());
    }
  }
  
  /**
   * 无论 project 是否变化，都加载 project，耗时
   *
   * @throws IOException
   */
  public void loadProject() throws IOException {
    loadProject(true);
  }
  
  /**
   * 加载 project
   *
   * @param forceUpdate 是否强制加载 project，如果 false 那么 project 关键参数没变的话就不会去加载 project
   * @throws IOException
   */
  public void loadProject(boolean forceUpdate) throws IOException {
    WSMediaLog.i(TAG, "loadProject forceUpdate:" + forceUpdate);
    synchronized (mLock) {
      loadProjectInternal(forceUpdate);
    }
  }
  
  /**
   * 销毁播放器的时候释放资源
   */
  public void release() {
    WSMediaLog.i(TAG, "release mNativePlayerAddress:" + mNativePlayerAddress);
    synchronized (mLock) {
      if (mNativePlayerAddress > 0) {
        long addressToDelete = mNativePlayerAddress;
        mNativePlayerAddress = 0;
        deleteNativePlayer(addressToDelete);
      }
    }
  }
  
  public double getCurrentTime() {
    if (mNativePlayerAddress == 0) {
      return 0.0;
    }
    return getCurrentTimeNative(mNativePlayerAddress);
  }
  
  @NonNull
  public EditorProject getProject() {
    if (mProject == null) {
      mProject = EditorProject.getDefaultInstance();
    }
    return mProject;
  }
  
  public void onAttachedView(int width, int height) {
    WSMediaLog.i(TAG,
        "onAttachedView mNativePlayerAddress:" + mNativePlayerAddress + ",width:" + width
            + ",height:" + height);
    synchronized (mLock) {
      if (mNativePlayerAddress == 0) {
        return;
      }
      mAttachedThread = Thread.currentThread();
      onAttachedViewNative(mNativePlayerAddress, width, height);
    }
  }
  
  public void onDetachedView() {
    WSMediaLog.i(TAG, "onDetachedView mNativePlayerAddress:" + mNativePlayerAddress);
    synchronized (mLock) {
      if (mNativePlayerAddress == 0) {
        return;
      }
      if (BuildConfig.DEBUG &&
          mAttachedThread != null &&
          mAttachedThread != Thread.currentThread()) {
        throw new Error("Programming error! Must detach and attach in the same thread!");
      }
      onDetachedViewNative(mNativePlayerAddress);
      mAttachedThread = null;
    }
  }
  
  public void drawFrame() {
    if (BuildConfig.DEBUG) {
      WSMediaLog.i(TAG,
          "drawFrame mNativePlayerAddress:" + mNativePlayerAddress + ",mProject[:"
              + projectKeyParamToString(mProject));
    }
    if (mNativePlayerAddress == 0) {
      return;
    }
    if (BuildConfig.DEBUG &&
        mAttachedThread != null &&
        mAttachedThread != Thread.currentThread()) {
      throw new Error("Programming error! Must attach and draw in the same thread!");
    }
    
    drawFrameNative(mNativePlayerAddress);
  }
  
  private void loadProjectInternal(boolean forceUpdate) throws IOException {
    WSMediaLog.i(TAG,
        "loadProjectInternal mNativePlayerAddress:" + mNativePlayerAddress + ",mProject[:"
            + projectKeyParamToString(mProject) + ",forceUpdate:" + forceUpdate);
    if (mNativePlayerAddress == 0) {
      return;
    }
    if (mProject == null) {
      throw new IllegalArgumentException("player project is null");
    }
    byte[] bytes =
        loadProjectNative(mNativePlayerAddress, mProject.toByteArray(), forceUpdate);
    CreateProjectNativeReturnValue nativeRet;
    try {
      nativeRet = CreateProjectNativeReturnValue.parseFrom(bytes);
    } catch (InvalidProtocolBufferException e) {
      throw new RuntimeException("Error parsing project from protobuf!", e);
    }
    
    if (nativeRet.getErrorCode() != 0) {
      throw new IOException("Probe file failed with error code " + nativeRet.getErrorCode());
    }
    
    EditorProject.Builder videoEditorProjectBuilder = mProject.toBuilder();
    if (videoEditorProjectBuilder.getMediaAssetCount() == nativeRet.getProject()
        .getMediaAssetCount()) {
      for (int i = 0; i < videoEditorProjectBuilder.getMediaAssetCount(); ++i) {
        MediaAsset.Builder trackAssetBuilder =
            videoEditorProjectBuilder.getMediaAsset(i).toBuilder();
        trackAssetBuilder
            .setMediaAssetFileHolder(nativeRet.getProject().getMediaAsset(i).getMediaAssetFileHolder());
        videoEditorProjectBuilder.setMediaAsset(i, trackAssetBuilder);
      }
    }
    
    videoEditorProjectBuilder.setPrivateData(nativeRet.getProject().getPrivateData());
    mProject = videoEditorProjectBuilder.build();
    WSMediaLog.i(TAG, "loadProjectInternal mProject:" + projectKeyParamToString(mProject));
  }
  
  @Override
  protected void finalize() throws Throwable {
    super.finalize();
    if (mNativePlayerAddress > 0) {
      WSMediaLog.w(TAG, "Delete native player in finalize, release() was not called!");
      deleteNativePlayer(mNativePlayerAddress);
    }
  }
  
  public int getReadyState() {
    if (mNativePlayerAddress == 0) {
      return 2;
    }
    return getReadyStateNative(mNativePlayerAddress);
  }
  
  private int getReadyStateNative(long nativePlayerAddress) {
    // todo
    return 2;
  }
  
  private native long newNativePlayer();
  
  private native void deleteNativePlayer(long nativePlayerAddress);
  
  private native void setProjectNative(long nativePlayerAddress, byte[] data);
  
  private native byte[] loadProjectNative(long nativePlayerAddress, byte[] data,
                                          boolean forceUpdate);
  
  private native void drawFrameNative(long nativePlayerAddress);
  
  private native void onAttachedViewNative(long nativePlayerAddress, int width, int height);
  
  private native void onDetachedViewNative(long nativePlayerAddress);
  
  private native void seekNative(long nativePlayerAddress, double currentTime);
  
  private native void playNative(long mNativePlayerAddress);
  
  private native void pauseNative(long mNativePlayerAddress);
  
  private native boolean isPlayingNative(long mNativePlayerAddress);
  
  private native double getCurrentTimeNative(long mNativePlayerAddress);
}
