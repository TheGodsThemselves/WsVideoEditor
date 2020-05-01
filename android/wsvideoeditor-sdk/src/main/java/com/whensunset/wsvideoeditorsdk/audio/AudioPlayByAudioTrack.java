package com.whensunset.wsvideoeditorsdk.audio;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.os.ConditionVariable;
import android.support.annotation.IntDef;
import android.support.annotation.Keep;

import com.whensunset.wsvideoeditorsdk.util.WSMediaLog;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class AudioPlayByAudioTrack {
  private static final String TAG = "AudioPlayByAudioTrack";

  /**
   * Represents an unset {@link android.media.AudioTrack} session identifier. Equal to
   * {@link AudioManager#AUDIO_SESSION_ID_GENERATE}.
   */
  public static final int AUDIO_SESSION_ID_UNSET = AudioManager.AUDIO_SESSION_ID_GENERATE;

  /**
   * A multiplication factor to apply to the minimum buffer size requested by the underlying
   * {@link AudioTrack}.
   */
  private static final int BUFFER_MULTIPLICATION_FACTOR = 2;
  /**
   * A minimum length for the {@link AudioTrack} buffer, in microseconds.
   */
  private static final long MIN_BUFFER_DURATION_US = 50_000;
  /**
   * A maximum length for the {@link AudioTrack} buffer, in microseconds.
   */
  private static final long MAX_BUFFER_DURATION_US = 750_000;

  /**
   * Returned by {@link #getCurrentPositionUs()} when the position is not set.
   */
  public static final long CURRENT_POSITION_NOT_SET = Long.MIN_VALUE;

  /**
   * A maximum length for {@link #mStartMediaTimeState} change to START_NEED_SYNC
   */
  public static final long START_TIME_STATE_NEED_SYNC_MAX_DIFF = 100_000;

  /**
   * Represents states of the {@link #mStartMediaTimeUs} value.
   */
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({START_NOT_SET, START_IN_SYNC, START_NEED_SYNC})
  private @interface StartMediaTimeState {}

  private static final int START_NOT_SET = 0;
  private static final int START_IN_SYNC = 1;
  private static final int START_NEED_SYNC = 2;
  private static final float VOLUME_STEP = 0.25f;

  private AudioTrack mAudioTrack = null;
  private final ConditionVariable mReleasingConditionVariable;
  private float mVolume = 1.0f;

  private int mAudioSessionId;
  private int mBufferSize;
  private int mSampleRateInHz = 44100;
  private int mChannelConfig =
      AudioFormat.CHANNEL_OUT_FRONT_LEFT | AudioFormat.CHANNEL_OUT_FRONT_RIGHT;

  private final AudioTrackPositionTracker mPositionTracker;
  private int mOutputPcmFrameSize;
  private @StartMediaTimeState
  int mStartMediaTimeState;
  private long mStartMediaTimeUs;
  private long mWrittenPcmBytes;
  private boolean mIsPlaying = false;

  private final Object mLock = new Object();
  private final Object mWriteDataLock = new Object();

  public AudioPlayByAudioTrack() {
    mPositionTracker = new AudioTrackPositionTracker();
    mReleasingConditionVariable = new ConditionVariable(true);
    mAudioSessionId = AUDIO_SESSION_ID_UNSET;
    WSMediaLog.i(TAG, "AudioPlayByAudioTrack");
  }

  @Keep
  public int initAudioTrack(int sampleRateInHz, int channelConfig) {
    WSMediaLog.i(TAG,
        "initAudioTrack sampleRateInHz:" + sampleRateInHz + "," + "channelConfig:" + channelConfig);
    mSampleRateInHz = sampleRateInHz;
    mChannelConfig = channelConfig;
    int minBufferSize = AudioTrack.getMinBufferSize(sampleRateInHz, channelConfig,
        AudioFormat.ENCODING_PCM_16BIT);
    if (minBufferSize == AudioTrack.ERROR_BAD_VALUE) {
      WSMediaLog.e(TAG, "initAudioTrack failed: minbuffersize is ERROR_BAD_VALUE");
      return -1;
    }

    // ENCODING_PCM_16BIT need 2 bytes, and we use stereo, so output pcm frame size should be 4
    mOutputPcmFrameSize = 2 * 2;
    int multipliedBufferSize = minBufferSize * BUFFER_MULTIPLICATION_FACTOR;
    int minAppBufferSize = (int) durationUsToFrames(MIN_BUFFER_DURATION_US) * mOutputPcmFrameSize;
    int maxAppBufferSize = (int) Math.max(minBufferSize,
        durationUsToFrames(MAX_BUFFER_DURATION_US) * mOutputPcmFrameSize);
    mBufferSize = Math.max(minAppBufferSize, Math.min(multipliedBufferSize, maxAppBufferSize));

    WSMediaLog.i(TAG, "initAudioTrack mOutputPcmFrameSize:" + mOutputPcmFrameSize + "," +
        "multipliedBufferSize:" + multipliedBufferSize + ",minBufferSize: " + minAppBufferSize +
        ",maxAppBufferSize:" + maxAppBufferSize + ",minBufferSize: " + minBufferSize + "," +
        "bufferSize:" + mBufferSize);
    synchronized (mLock) {
      try {
        initialize();
      } catch (InitializationException e) {
        WSMediaLog.e(TAG, "initAudioTrack initialize failed: ", e);
        return -1;
      }
    }
    return 0;
  }

  private void initialize() throws InitializationException {
    // If we're asynchronously releasing a previous audio track then we block until it has been
    // released. This guarantees that we cannot end up in a state where we have multiple audio
    // track instances. Without this guarantee it would be possible, in extreme cases, to exhaust
    // the shared memory that's available for audio track buffers. This would in turn cause the
    // initialization of the audio track to fail.
    mReleasingConditionVariable.block();
    mAudioTrack = initializeAudioTrack();
    int audioSessionId = mAudioTrack.getAudioSessionId();
    if (mAudioSessionId != audioSessionId) {
      mAudioSessionId = audioSessionId;
      WSMediaLog.i(TAG, "initialize audioSessionId:" + audioSessionId);
    }
    mPositionTracker.setAudioTrack(mAudioTrack, mOutputPcmFrameSize, mBufferSize);
  }

  private AudioTrack initializeAudioTrack() throws InitializationException {
    AudioTrack audioTrack;
    try {
      if (mAudioSessionId == AUDIO_SESSION_ID_UNSET) {
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, mSampleRateInHz, mChannelConfig,
            AudioFormat.ENCODING_PCM_16BIT, mBufferSize, AudioTrack.MODE_STREAM);
      } else {
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, mSampleRateInHz, mChannelConfig,
            AudioFormat.ENCODING_PCM_16BIT, mBufferSize, AudioTrack.MODE_STREAM, mAudioSessionId);
      }
      WSMediaLog.i(TAG, "initializeAudioTrack sampleRateInHz:" + mSampleRateInHz + "," +
          "channelConfig:" + mChannelConfig + ",bufferSize = " + mBufferSize);
    } catch (Exception e) {
      WSMediaLog.e(TAG, "initAudioTrack new instance failed: ", e);
      throw new InitializationException(-1, mSampleRateInHz, mChannelConfig, mBufferSize);
    }
    int state = audioTrack.getState();
    if (state != AudioTrack.STATE_INITIALIZED) {
      try {
        audioTrack.release();
      } catch (Exception e) {
        WSMediaLog.e(TAG, "initAudioTrack release failed: ", e);
      }
      throw new InitializationException(state, mSampleRateInHz, mChannelConfig, mBufferSize);
    }
    return audioTrack;
  }

  @Keep
  public int releaseAudioTrack() {
    synchronized (mLock) {
      return releaseAudioTrackInternal();
    }
  }

  private boolean isInitialized() {
    return mAudioTrack != null;
  }

  private int releaseAudioTrackInternal() {
    if (isInitialized()) {
      final AudioTrack toRelease = mAudioTrack;
      synchronized (mWriteDataLock) {
        mAudioTrack = null;
      }

      mReleasingConditionVariable.close();
      new Thread() {
        @Override
        public void run() {
          try {
            toRelease.flush();
            toRelease.release();
          } catch (IllegalStateException e) {
            WSMediaLog.e(TAG, "releaseAudioTrack Failed: ", e);
          } finally {
            mReleasingConditionVariable.open();
          }
        }
      }.start();
    }
    return 0;
  }

  @Keep
  public void playAudioTrack() {
    synchronized (mLock) {
      if (!mIsPlaying) {
        playInternal();
      }
    }
  }

  private void playInternal() {
    if (isInitialized()) {
      mPositionTracker.start();
      mAudioTrack.play();
    }
    mIsPlaying = true;
  }

  @Keep
  public void flushAudioTrack() {
    synchronized (mLock) {
      /*
       * Android doc:
       * stop(): Stops playing the audio data. When used on an instance created
       * in MODE_STREAM mode, audio will stop playing after the last buffer that
       * was written has been played. For an immediate stop, use pause(),
       * followed by flush() to discard audio data that hasn't been played back
       * yet.
       * flush(): Flushes the audio data currently queued for playback. Any data
       * that has not been played back will be discarded. No-op if not stopped
       * or paused, or if the track's creation mode is not MODE_STREAM.
       */
      reset();
      // TODO: flush the data, but now flush will clear all playBackHeadPosition and timeStamp,
      // there is bug now!!!
      // mAudioTrack.flush();
      // for fade in (line 297)
      mVolume = -VOLUME_STEP;
    }
  }

  @Keep
  public void pauseAudioTrack() {
    synchronized (mLock) {
      mIsPlaying = false;
      if (!isInitialized()) {
        return;
      }
    }
    try {
      mPositionTracker.pause();
      if (mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
        mAudioTrack.pause();
      }
    } catch (IllegalStateException e) {
      WSMediaLog.e(TAG, "pauseAudioTrack Failed: ", e);
    }
  }

  @Keep
  public int writeAudioTrack(byte[] audioData, int size, long audioDataTimeUs) {
    synchronized (mLock) {
      if (!isInitialized()) {
        try {
          initialize();
        } catch (InitializationException e) {
          WSMediaLog.e(TAG, "writeAudioTrack initAudioTrack failed: ", e);
          return -1;
        }
        if (mIsPlaying) {
          try {
            playInternal();
          } catch (IllegalStateException e) {
            WSMediaLog.e(TAG, "resumeAudioTrack Failed: ", e);
            return -1;
          }
        }
      }

      if (!mPositionTracker.mayHandleBuffer(getWrittenFrames())) {
        return -1;
      }

      if (mVolume < 1.0f) {
        mVolume = Math.max(0.0f, Math.min(mVolume + VOLUME_STEP, 1.0f));
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
          mAudioTrack.setStereoVolume(mVolume, mVolume);
        } else {
          mAudioTrack.setVolume(mVolume);
        }
      }
      if (mStartMediaTimeState == START_NOT_SET) {
        WSMediaLog.e(TAG, "mStartMediaTimeState == START_NOT_SET");
        mStartMediaTimeUs = Math.max(0, audioDataTimeUs);
        mStartMediaTimeState = START_IN_SYNC;
      } else {
        // Sanity check that presentationTimeUs is consistent with the expected value.
        long expectedPresentationTimeUs =
            mStartMediaTimeUs + framesToDurationUs(getWrittenFrames());
        if (mStartMediaTimeState == START_IN_SYNC && Math.abs(expectedPresentationTimeUs - audioDataTimeUs) > START_TIME_STATE_NEED_SYNC_MAX_DIFF) {
          WSMediaLog.w(TAG, "Discontinuity detected [expected " + expectedPresentationTimeUs + ","
              + " got " + audioDataTimeUs + "] written " + getWrittenFrames() + ", " +
              "mStartMediaTimeUs: " + mStartMediaTimeUs);
          mStartMediaTimeState = START_NEED_SYNC;
        }
        if (mStartMediaTimeState == START_NEED_SYNC) {
          // Adjust startMediaTimeUs to be consistent with the current buffer's start time and the
          // number of bytes submitted.
          long diff = audioDataTimeUs - expectedPresentationTimeUs;
          mStartMediaTimeUs += (audioDataTimeUs - expectedPresentationTimeUs);
          mStartMediaTimeState = START_IN_SYNC;
          WSMediaLog.w(TAG, "Discontinuity try to sync [expect " + expectedPresentationTimeUs +
              ", got " + audioDataTimeUs + " diff " + diff + ", mStartMediaTimeUs: " + mStartMediaTimeUs + "]");
        }
      }
    }
    int bytesWritten = 0;
    // AudioTrack.write would block a lot of time, so we should not hold the mLock
    // use a separate lock
    synchronized (mWriteDataLock) {
      if (mAudioTrack != null) {
        bytesWritten = mAudioTrack.write(audioData, 0, size);
      }
    }

    synchronized (mLock) {
      if (bytesWritten < 0) {
        WSMediaLog.e(TAG,
            "writeAudioTrack exception! written: " + bytesWritten + ", size: " + size);
      }
      mWrittenPcmBytes += bytesWritten;

      if (mPositionTracker.isStalled(getWrittenFrames())) {
        WSMediaLog.w(TAG, "Resetting stalled audio track");
        reset();
      }
      return bytesWritten;
    }
  }

  private void reset() {
    mWrittenPcmBytes = 0;
    mStartMediaTimeState = START_NOT_SET;
    if (isInitialized()) {
      if (mPositionTracker.isPlaying()) {
        mPositionTracker.pause();
      }
    }
    releaseAudioTrackInternal();
    mPositionTracker.reset();
  }

  private long durationUsToFrames(long durationUs) {
    return (durationUs * mSampleRateInHz) / AudioTimestampPoller.MICROS_PER_SECOND;
  }

  private long framesToDurationUs(long frameCount) {
    return (frameCount * AudioTimestampPoller.MICROS_PER_SECOND) / mSampleRateInHz;
  }

  @Keep
  public long getCurrentPositionUs() {
    synchronized (mLock) {
      if (!isInitialized()) {
        return CURRENT_POSITION_NOT_SET;
      }
    }
    if (mStartMediaTimeState == START_NOT_SET) {
      return CURRENT_POSITION_NOT_SET;
    }
    long positionUs = mPositionTracker.getCurrentPositionUs();
    positionUs = Math.min(positionUs, framesToDurationUs(getWrittenFrames()));
    return mStartMediaTimeUs + positionUs;
  }

  private long getWrittenFrames() {
    return mWrittenPcmBytes / mOutputPcmFrameSize;
  }

  private final class InitializationException extends Exception {
    /**
     * The underlying {@link AudioTrack}'s state, if applicable.
     */
    public final int audioTrackState;

    /**
     * @param audioTrackState The underlying {@link AudioTrack}'s state, if applicable.
     * @param sampleRate      The requested sample rate in Hz.
     * @param channelConfig   The requested channel configuration.
     * @param bufferSize      The requested buffer size in bytes.
     */
    public InitializationException(int audioTrackState, int sampleRate, int channelConfig,
                                   int bufferSize) {
      super("AudioTrack init failed: " + audioTrackState + ", Config(" + sampleRate + ", " + channelConfig + ", " + bufferSize + ")");
      this.audioTrackState = audioTrackState;
    }
  }
}
