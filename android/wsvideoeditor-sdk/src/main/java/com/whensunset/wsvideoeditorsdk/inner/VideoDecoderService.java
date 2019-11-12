package com.whensunset.wsvideoeditorsdk.inner;

import android.support.annotation.NonNull;

import com.whensunset.wsvideoeditorsdk.model.EditorProject;

/**
 * wsvideoeditor::VideoDecodeService 的上层封装
 */
public class VideoDecoderService {
  private volatile long mNativeAddress = 0;
  
  public VideoDecoderService() {
    mNativeAddress = newNative(5);
  }
  
  /**
   * 释放 VideoDecoderService native 对象
   */
  public void release() {
    releaseNative(mNativeAddress);
  }
  
  /**
   * 给 native 对象设置 project
   *
   * @param startTime     开始的解码点
   * @param editorProject project 序列化之后的数据
   */
  public void setProject(double startTime, @NonNull EditorProject editorProject) {
    setProjectNative(mNativeAddress, startTime, editorProject.toByteArray());
  }
  
  /**
   * 开始解码视频
   */
  public void start() {
    startNative(mNativeAddress);
  }
  
  /**
   * 获取某个时间点的视频帧
   *
   * @param renderTime 需要截取帧的时间点
   * @return
   */
  public String getRenderFrame(double renderTime) {
    return getRenderFrameNative(mNativeAddress, renderTime);
  }
  
  /**
   * 更新 native 对象的 project
   *
   * @param editorProject
   */
  public void updateProject(@NonNull EditorProject editorProject) {
    updateProjectNative(mNativeAddress, editorProject.toByteArray());
  }
  
  /**
   * 重新设置解码的时间点
   *
   * @param seekTime 新的解码时间点
   */
  public void seek(double seekTime) {
    seekNative(mNativeAddress, seekTime);
  }
  
  /**
   * 暂停解码
   */
  public void stop() {
    stopNative(mNativeAddress);
  }
  
  /**
   * 是否已经解码到了最后一帧
   *
   * @return
   */
  public boolean ended() {
    return endedNative(mNativeAddress);
  }
  
  /**
   * 是否暂停解码
   *
   * @return
   */
  public boolean stopped() {
    return stoppedNative(mNativeAddress);
  }
  
  /**
   * 获取当前解码队列的帧数量
   *
   * @return
   */
  public int getBufferedFrameCount() {
    return getBufferedFrameCountNative(mNativeAddress);
  }
  
  private native long newNative(int bufferCapacity);
  
  private native void releaseNative(long nativeAddress);
  
  private native void setProjectNative(long nativeAddress, double startTime, byte[] projectData);
  
  private native void startNative(long nativeAddress);
  
  private native String getRenderFrameNative(long nativeAddress, double renderTime);
  
  private native void updateProjectNative(long nativeAddress, byte[] projectData);
  
  private native void seekNative(long nativeAddress, double seekTime);
  
  private native void stopNative(long nativeAddress);
  
  private native boolean endedNative(long nativeAddress);
  
  private native boolean stoppedNative(long nativeAddress);
  
  private native int getBufferedFrameCountNative(long nativeAddress);
}
