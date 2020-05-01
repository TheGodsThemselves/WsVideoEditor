package com.whensunset.wsvideoeditorsdk.audio;

/**
 * AudioTrack position tracker
 * Reference to:
 * https://github.com/google/ExoPlayer/blob/f7ed789fc3841b2536f5629c6bb10f4cb4ee5130/library/core
 * /src/main/java/com/google/android/exoplayer2/audio/AudioTrackPositionTracker.java#L220
 */

import android.media.AudioTrack;
import android.os.Build;
import android.os.SystemClock;
import android.support.annotation.IntDef;

import com.whensunset.wsvideoeditorsdk.util.WSMediaLog;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Method;

/**
 * Wraps an {@link AudioTrack}, exposing a position based on {@link
 * AudioTrack#getPlaybackHeadPosition()} and AudioTrack#getTimestamp(AudioTimestamp) (this is a
 * hidden method).
 *
 * <p>
 * Call {@link #setAudioTrack(AudioTrack, int, int)} to set the audio track to wrap. Call
 * {@link #mayHandleBuffer(long)} if there is input data to write to the track. If it returns false,
 * the audio track position is stabilizing and no data may be written. Call {@link #start()}
 * immediately before calling {@link AudioTrack#play()}. Call {@link #pause()} when pausing the
 * track. Call {@link #handleEndOfStream(long)} when no more data will be written to the track. When
 * the audio track will no longer be used, call {@link #reset()}.
 */
final class AudioTrackPositionTracker {
  private static final String TAG = "AudioTrackPositionTracker";

  private static final String METHOD_GETLATENCY = "getLatency";

  /**
   * {@link AudioTrack} playback states.
   */
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({PLAYSTATE_STOPPED, PLAYSTATE_PAUSED, PLAYSTATE_PLAYING})
  private @interface PlayState {}

  /**
   * @see AudioTrack#PLAYSTATE_STOPPED
   */
  private static final int PLAYSTATE_STOPPED = AudioTrack.PLAYSTATE_STOPPED;
  /**
   * @see AudioTrack#PLAYSTATE_PAUSED
   */
  private static final int PLAYSTATE_PAUSED = AudioTrack.PLAYSTATE_PAUSED;
  /**
   * @see AudioTrack#PLAYSTATE_PLAYING
   */
  private static final int PLAYSTATE_PLAYING = AudioTrack.PLAYSTATE_PLAYING;

  /**
   * AudioTrack timestamps are deemed spurious if they are offset from the system clock by more than
   * this amount.
   *
   * <p>
   * This is a fail safe that should not be required on correctly functioning devices.
   */
  private static final long MAX_AUDIO_TIMESTAMP_OFFSET_US =
      5 * AudioTimestampPoller.MICROS_PER_SECOND;

  /**
   * AudioTrack latencies are deemed impossibly large if they are greater than this amount.
   *
   * <p>
   * This is a fail safe that should not be required on correctly functioning devices.
   */
  private static final long MAX_LATENCY_US = 5 * AudioTimestampPoller.MICROS_PER_SECOND;

  private static final long FORCE_RESET_WORKAROUND_TIMEOUT_MS = 200;

  private static final int MAX_PLAYHEAD_OFFSET_COUNT = 10;
  private static final int MIN_PLAYHEAD_OFFSET_SAMPLE_INTERVAL_US = 30000;
  private static final int MIN_LATENCY_SAMPLE_INTERVAL_US = 500000;

  private final long[] mPlayheadOffsets;

  private AudioTrack mAudioTrack;
  private int mOutputPcmFrameSize;
  private int mBufferSize;
  private AudioTimestampPoller mAudioTimestampPoller;
  private int mOutputSampleRate;
  private long mBufferSizeUs;

  private long mSmoothedPlayheadOffsetUs;
  private long mLastPlayheadSampleTimeUs;

  private Method mGetLatencyMethod;
  private long mLatencyUs;
  private boolean mHasData;

  private long mLastLatencySampleTimeUs;
  private long mLastRawPlaybackHeadPosition;
  private long mRawPlaybackHeadWrapCount;
  private int mNextPlayheadOffsetIndex;
  private int mPlayheadOffsetCount;
  private long mStopTimestampUs;
  private long mForceResetWorkaroundTimeMs;
  private long mStopPlaybackHeadPosition;
  private long mEndPlaybackHeadPosition;

  public AudioTrackPositionTracker() {
    WSMediaLog.i(TAG, "AudioTrackPositionTracker Build.VERSION.SDK_INT:" + Build.VERSION.SDK_INT);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
      try {
        mGetLatencyMethod = AudioTrack.class.getMethod(METHOD_GETLATENCY, (Class<?>[]) null);
      } catch (NoSuchMethodException e) {
        WSMediaLog.w(TAG, "AudioTrackPositionTracker getLatency method not exist");
      }
    }
    mPlayheadOffsets = new long[MAX_PLAYHEAD_OFFSET_COUNT];
  }

  /**
   * Sets the {@link AudioTrack} to wrap. Subsequent method calls on this instance relate to this
   * track's position, until the next call to {@link #reset()}.
   *
   * @param audioTrack         The audio track to wrap.
   * @param outputPcmFrameSize For PCM output encodings, the frame size. The value is ignored
   *                           otherwise.
   * @param bufferSize         The audio track buffer size in bytes.
   */
  public void setAudioTrack(AudioTrack audioTrack, int outputPcmFrameSize, int bufferSize) {
    this.mAudioTrack = audioTrack;
    this.mOutputPcmFrameSize = outputPcmFrameSize;
    this.mBufferSize = bufferSize;
    mAudioTimestampPoller = new AudioTimestampPoller(audioTrack);
    mOutputSampleRate = audioTrack.getSampleRate();
    mBufferSizeUs = framesToDurationUs(bufferSize / outputPcmFrameSize);
    mLastRawPlaybackHeadPosition = 0;
    mRawPlaybackHeadWrapCount = 0;
    mHasData = false;
    mStopTimestampUs = AudioTimestampPoller.TIME_UNSET;
    mForceResetWorkaroundTimeMs = AudioTimestampPoller.TIME_UNSET;
    mLatencyUs = 0;
  }

  public long getCurrentPositionUs() {
    if (mAudioTrack.getPlayState() == PLAYSTATE_PLAYING) {
      maybeSampleSyncParams();
    }


    // If the device supports it, use the playback timestamp from AudioTrack.getTimestamp.
    // Otherwise, derive a smoothed position by sampling the track's frame position.
    long systemTimeUs = System.nanoTime() / 1000;
    if (mAudioTimestampPoller.hasTimestamp()) {
      // Calculate the speed-adjusted position using the timestamp (which may be in the future).
      long timestampPositionFrames = mAudioTimestampPoller.getTimestampPositionFrames();
      long timestampPositionUs = framesToDurationUs(timestampPositionFrames);
      if (!mAudioTimestampPoller.isTimestampAdvancing()) {
        return timestampPositionUs;
      }
      long elapsedSinceTimestampUs =
          systemTimeUs - mAudioTimestampPoller.getTimestampSystemTimeUs();
      return timestampPositionUs + elapsedSinceTimestampUs;
    } else {
      long positionUs;
      if (mPlayheadOffsetCount == 0) {
        // The AudioTrack has started, but we don't have any samples to compute a smoothed position.
        positionUs = getPlaybackHeadPositionUs();
      } else {
        // getPlaybackHeadPositionUs() only has a granularity of ~20 ms, so we base the position off
        // the system clock (and a smoothed offset between it and the playhead position) so as to
        // prevent jitter in the reported positions.
        positionUs = systemTimeUs + mSmoothedPlayheadOffsetUs;
      }
      positionUs -= mLatencyUs;
      return positionUs;
    }
  }

  /**
   * Starts position tracking. Must be called immediately before {@link AudioTrack#play()}.
   */
  public void start() {
    mAudioTimestampPoller.reset();
  }

  /**
   * Returns whether the audio track is in the playing state.
   */
  public boolean isPlaying() {
    return mAudioTrack.getPlayState() == PLAYSTATE_PLAYING;
  }

  /**
   * Converts a time in microseconds to the corresponding time in milliseconds, preserving
   * {@link AudioTimestampPoller#TIME_UNSET} and {@link AudioTimestampPoller#TIME_END_OF_SOURCE}
   * values.
   *
   * @param timeUs The time in microseconds.
   * @return The corresponding time in milliseconds.
   */
  public static long usToMs(long timeUs) {
    return (timeUs == AudioTimestampPoller.TIME_UNSET || timeUs == AudioTimestampPoller.TIME_END_OF_SOURCE) ? timeUs : (timeUs / 1000);
  }

  /**
   * Checks the state of the audio track and returns whether the caller can write data to the track.
   *
   * @param writtenFrames The number of frames that have been written.
   * @return Whether the caller can write data to the track.
   */
  public boolean mayHandleBuffer(long writtenFrames) {
    @PlayState int playState = mAudioTrack.getPlayState();
    boolean hadData = mHasData;
    mHasData = hasPendingData(writtenFrames);
    return true;
  }

  /**
   * Returns an estimate of the number of additional bytes that can be written to the audio track's
   * buffer without running out of space.
   *
   * <p>
   * May only be called if the output encoding is one of the PCM encodings.
   *
   * @param writtenBytes The number of bytes written to the audio track so far.
   * @return An estimate of the number of bytes that can be written.
   */
  public int getAvailableBufferSize(long writtenBytes) {
    int bytesPending = (int) (writtenBytes - (getPlaybackHeadPosition() * mOutputPcmFrameSize));
    // WSMediaLog.i("AudioPlayByAudioTrack", "getAvailableBufferSize bytesPending: " + bytesPending
    // + ", writtenBytes: " + writtenBytes + ", getPlaybackHeadPosition: " +
    // getPlaybackHeadPosition()
    // + ", mOutputPcmFrameSize: " + mOutputPcmFrameSize
    // + ", mBufferSize: " + mBufferSize);
    return mBufferSize - bytesPending;
  }

  /**
   * Returns whether the track is in an invalid state and must be recreated.
   */
  public boolean isStalled(long writtenFrames) {
    return mForceResetWorkaroundTimeMs != AudioTimestampPoller.TIME_UNSET && writtenFrames > 0 && SystemClock.elapsedRealtime() - mForceResetWorkaroundTimeMs >= FORCE_RESET_WORKAROUND_TIMEOUT_MS;
  }

  /**
   * Records the writing position at which the stream ended, so that the reported position can
   * continue to increment while remaining data is played out.
   *
   * @param writtenFrames The number of frames that have been written.
   */
  public void handleEndOfStream(long writtenFrames) {
    mStopPlaybackHeadPosition = getPlaybackHeadPosition();
    mStopTimestampUs = SystemClock.elapsedRealtime() * 1000;
    mEndPlaybackHeadPosition = writtenFrames;
  }

  /**
   * Returns whether the audio track has any pending data to play out at its current position.
   *
   * @param writtenFrames The number of frames written to the audio track.
   * @return Whether the audio track has any pending data to play out.
   */
  public boolean hasPendingData(long writtenFrames) {
    return writtenFrames > getPlaybackHeadPosition();
  }

  /**
   * Pauses the audio track position tracker, returning whether the audio track needs to be paused
   * to cause playback to pause. If {@code false} is returned the audio track will pause without
   * further interaction, as the end of stream has been handled.
   */
  public boolean pause() {
    resetSyncParams();
    if (mStopTimestampUs == AudioTimestampPoller.TIME_UNSET) {
      // The audio track is going to be paused, so reset the timestamp poller to ensure it doesn't
      // supply an advancing position.
      mAudioTimestampPoller.reset();
      return true;
    }
    // We've handled the end of the stream already, so there's no need to pause the track.
    return false;
  }

  /**
   * Resets the position tracker. Should be called when the audio track previous passed to {@link
   * #setAudioTrack(AudioTrack, int, int)} is no longer in use.
   */
  public void reset() {
    resetSyncParams();
    mAudioTrack = null;
    mAudioTimestampPoller = null;
  }

  private void maybeSampleSyncParams() {
    long playbackPositionUs = getPlaybackHeadPositionUs();
    if (playbackPositionUs == 0) {
      // The AudioTrack hasn't output anything yet.
      return;
    }
    long systemTimeUs = System.nanoTime() / 1000;
    if (systemTimeUs - mLastPlayheadSampleTimeUs >= MIN_PLAYHEAD_OFFSET_SAMPLE_INTERVAL_US) {
      // Take a new sample and update the smoothed offset between the system clock and the playhead.
      mPlayheadOffsets[mNextPlayheadOffsetIndex] = playbackPositionUs - systemTimeUs;
      mNextPlayheadOffsetIndex = (mNextPlayheadOffsetIndex + 1) % MAX_PLAYHEAD_OFFSET_COUNT;
      if (mPlayheadOffsetCount < MAX_PLAYHEAD_OFFSET_COUNT) {
        mPlayheadOffsetCount++;
      }
      mLastPlayheadSampleTimeUs = systemTimeUs;
      mSmoothedPlayheadOffsetUs = 0;
      for (int i = 0; i < mPlayheadOffsetCount; i++) {
        mSmoothedPlayheadOffsetUs += mPlayheadOffsets[i] / mPlayheadOffsetCount;
      }
    }

    maybePollAndCheckTimestamp(systemTimeUs, playbackPositionUs);
    maybeUpdateLatency(systemTimeUs);
  }

  private void maybePollAndCheckTimestamp(long systemTimeUs, long playbackPositionUs) {
    if (!mAudioTimestampPoller.maybePollTimestamp(systemTimeUs)) {
      return;
    }

    // Perform sanity checks on the timestamp and accept/reject it.
    long audioTimestampSystemTimeUs = mAudioTimestampPoller.getTimestampSystemTimeUs();
    long audioTimestampPositionFrames = mAudioTimestampPoller.getTimestampPositionFrames();
    if (Math.abs(audioTimestampSystemTimeUs - systemTimeUs) > MAX_AUDIO_TIMESTAMP_OFFSET_US) {
      mAudioTimestampPoller.rejectTimestamp();
    } else if (Math.abs(framesToDurationUs(audioTimestampPositionFrames) - playbackPositionUs) > MAX_AUDIO_TIMESTAMP_OFFSET_US) {
      mAudioTimestampPoller.rejectTimestamp();
    } else {
      mAudioTimestampPoller.acceptTimestamp();
    }
  }

  private void maybeUpdateLatency(long systemTimeUs) {
    if (mGetLatencyMethod != null && systemTimeUs - mLastLatencySampleTimeUs >= MIN_LATENCY_SAMPLE_INTERVAL_US) {
      try {
        // Compute the audio track latency, excluding the latency due to the buffer (leaving
        // latency due to the mixer and audio hardware driver).
        mLatencyUs =
            (Integer) mGetLatencyMethod.invoke(mAudioTrack, (Object[]) null) * 1000L - mBufferSizeUs;
        // Sanity check that the latency is non-negative.
        mLatencyUs = Math.max(mLatencyUs, 0);
        // Sanity check that the latency isn't too large.
        if (mLatencyUs > MAX_LATENCY_US) {
          mLatencyUs = 0;
        }
      } catch (Exception e) {
        // The method existed, but doesn't work. Don't try again.
        mGetLatencyMethod = null;
      }
      mLastLatencySampleTimeUs = systemTimeUs;
    }
  }

  private long framesToDurationUs(long frameCount) {
    return (frameCount * AudioTimestampPoller.MICROS_PER_SECOND) / mOutputSampleRate;
  }

  private void resetSyncParams() {
    mSmoothedPlayheadOffsetUs = 0;
    mPlayheadOffsetCount = 0;
    mNextPlayheadOffsetIndex = 0;
    mLastPlayheadSampleTimeUs = 0;
  }

  private long getPlaybackHeadPositionUs() {
    return framesToDurationUs(getPlaybackHeadPosition());
  }

  /**
   * {@link AudioTrack#getPlaybackHeadPosition()} returns a value intended to be interpreted as an
   * unsigned 32 bit integer, which also wraps around periodically. This method returns the playback
   * head position as a long that will only wrap around if the value exceeds {@link Long#MAX_VALUE}
   * (which in practice will never happen).
   *
   * @return The playback head position, in frames.
   */
  private long getPlaybackHeadPosition() {
    if (mStopTimestampUs != AudioTimestampPoller.TIME_UNSET) {
      // Simulate the playback head position up to the total number of frames submitted.
      long elapsedTimeSinceStopUs = (SystemClock.elapsedRealtime() * 1000) - mStopTimestampUs;
      long framesSinceStop =
          (elapsedTimeSinceStopUs * mOutputSampleRate) / AudioTimestampPoller.MICROS_PER_SECOND;
      return Math.min(mEndPlaybackHeadPosition, mStopPlaybackHeadPosition + framesSinceStop);
    }

    int state = mAudioTrack.getPlayState();
    if (state == PLAYSTATE_STOPPED) {
      // The audio track hasn't been started.
      return 0;
    }

    long rawPlaybackHeadPosition = 0xFFFFFFFFL & mAudioTrack.getPlaybackHeadPosition();

    if (Build.VERSION.SDK_INT <= 28) {
      if (rawPlaybackHeadPosition == 0 && mLastRawPlaybackHeadPosition > 0 && state == PLAYSTATE_PLAYING) {
        // If connecting a Bluetooth audio device fails, the AudioTrack may be left in a state
        // where its Java API is in the playing state, but the native track is stopped. When this
        // happens the playback head position gets stuck at zero. In this case, return the old
        // playback head position and force the track to be reset after
        // {@link #FORCE_RESET_WORKAROUND_TIMEOUT_MS} has elapsed.
        if (mForceResetWorkaroundTimeMs == AudioTimestampPoller.TIME_UNSET) {
          mForceResetWorkaroundTimeMs = SystemClock.elapsedRealtime();
        }
        return mLastRawPlaybackHeadPosition;
      } else {
        mForceResetWorkaroundTimeMs = AudioTimestampPoller.TIME_UNSET;
      }
    }

    if (mLastRawPlaybackHeadPosition > rawPlaybackHeadPosition) {
      // The value must have wrapped around.
      mRawPlaybackHeadWrapCount++;
    }
    mLastRawPlaybackHeadPosition = rawPlaybackHeadPosition;
    return rawPlaybackHeadPosition + (mRawPlaybackHeadWrapCount << 32);
  }
}
