#include "audio_player_by_android.h"
#include "ws_editor_video_sdk_utils.h"
#include "av_utils.h"

namespace whensunset {
    namespace wsvideoeditor {


        AudioPlayByAndroid::AudioPlayByAndroid(
                TimeMessageCenter *player_time_message_center) : AudioPlayer(
                player_time_message_center) {
            buff_size_ = AUDIO_BUFFER_SIZE;
            buff_.reset(new(std::nothrow) uint8_t[buff_size_]);
            get_buff_func_ = nullptr;
            play_retry_times_ = 0;
            is_playing_ = false;
            is_inited_ = false;
            request_stop_ = false;
            sample_rate_ = 44100;
            channel_config_ = CHANNEL_OUT_STEREO;
            Init();
            Start();
        }

        void AudioPlayByAndroid::Init() {
            AttachCurrentThreadIfNeeded attached;
            JNIEnv *env = attached.jni();

            {
                LocalRef<jclass> clazz{env, env->FindClass(
                        "com/whensunset/wsvideoeditorsdk/audio/AudioPlayByAudioTrack")};
                jthrowable jexception = env->ExceptionOccurred();
                if (jexception) {
                    env->ExceptionDescribe();
                    env->DeleteLocalRef(jexception);
                    assert(false);
                }
                j_class_audioplay_by_audiotrack_wrapper_.reset(env, clazz());
                j_method_id_j_class_audioplay_by_audiotrack_wrapper_ctor_ = env->GetMethodID(
                        clazz(), "<init>", "()V");
                j_class_audioplay_by_audiotrack_wrapper_.reset(env, clazz());

                LocalRef<jobject> obj{env,
                                      env->NewObject(clazz(),
                                                     j_method_id_j_class_audioplay_by_audiotrack_wrapper_ctor_)};
                audioplay_by_audiotrack_service_.reset(env, obj());

                j_method_id_initAudioTrack_ = env->GetMethodID(clazz(), "initAudioTrack", "(II)I");
                assert(j_method_id_initAudioTrack_);
                j_method_id_writeAudioTrack_ = env->GetMethodID(clazz(), "writeAudioTrack",
                                                                "([BIJ)I");
                assert(j_method_id_writeAudioTrack_);
                j_method_id_pauseAudioTrack_ = env->GetMethodID(clazz(), "pauseAudioTrack", "()V");
                assert(j_method_id_pauseAudioTrack_);
                j_method_id_playAudioTrack_ = env->GetMethodID(clazz(), "playAudioTrack", "()V");
                assert(j_method_id_playAudioTrack_);
                j_method_id_releaseAudioTrack_ = env->GetMethodID(clazz(), "releaseAudioTrack",
                                                                  "()I");
                assert(j_method_id_releaseAudioTrack_);

                j_method_id_getCurrentPositionUsAudioTrack_ = env->GetMethodID(clazz(),
                                                                               "getCurrentPositionUs",
                                                                               "()J");
                assert(j_method_id_getCurrentPositionUsAudioTrack_);
                j_method_id_flushAudioTrack_ = env->GetMethodID(clazz(), "flushAudioTrack", "()V");
                assert(j_method_id_flushAudioTrack_);
            }
        }

        void AudioPlayByAndroid::startPlayThread() {
            SetCurrentThreadName("EditorAudioPlayByAudioTrack");
            AttachCurrentThreadIfNeeded attached;
            JNIEnv *env = attached.jni();
            jbyteArray jarray = env->NewByteArray(buff_size_);
            if (!jarray) {
                return;
            }
            while (1) {
                std::unique_lock<std::mutex> lk(player_mutex_);
                if (request_stop_) {
                    break;
                }
                if (is_playing_) {
                    if (!buff_) {
                        //retry alloc buffer
                        buff_.reset(new(std::nothrow) uint8_t[buff_size_]);
                        if (!buff_) {
                            break;
                        }
                    }
                    memset(buff_.get(), 0, buff_size_);

                    double render_pos = ref_clock_->GetRenderPos();
                    int got_length = 0;
                    if (get_buff_func_) {
                        get_buff_func_(&got_length, buff_.get(), buff_size_, &render_pos);
                        if (got_length == 0) {
                            lk.unlock();
                            // Didn't get anything, buff_ will be memset to 0
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            continue;
                        }
                    }

                    env->SetByteArrayRegion(jarray, 0, buff_size_, (jbyte *) buff_.get());

                    // enqueue another buffer
                    int64_t playback_pos_us = static_cast<int64_t >(render_pos * 1000000l +
                                                                    TIME_EPS);
                    lk.unlock();
                    int bytes_written = env->CallIntMethod(audioplay_by_audiotrack_service_(),
                                                           j_method_id_writeAudioTrack_,
                                                           jarray, buff_size_, playback_pos_us);
                    lk.lock();
                    if (bytes_written < buff_size_) {
                        // we use stereo s16, so it needs 4 bytes one sample
                        assert(bytes_written % 4 == 0);
                    }
                } else {
                    lk.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            env->DeleteLocalRef(jarray);
        }

        int AudioPlayByAndroid::Start() {
            AttachCurrentThreadIfNeeded attached;
            JNIEnv *env = attached.jni();
            int ret = -1;
            while ((ret < 0) && play_retry_times_++ < 3) {
                ret = env->CallIntMethod(audioplay_by_audiotrack_service_(),
                                         j_method_id_initAudioTrack_,
                                         sample_rate_, channel_config_);
                if (ret < 0) {
                    std::this_thread::sleep_for(
                            std::chrono::milliseconds(10 * (play_retry_times_ + 1)));
                }
            }
            if (!ret) {
                is_inited_ = true;
                play_thread_ = std::thread(&AudioPlayByAndroid::startPlayThread, this);
            }
            return ret;
        }

        double
        AudioPlayByAndroid::GetCurrentTimeSec(const model::EditorProject &project) {
            std::unique_lock<std::mutex> lk(player_mutex_);
            double render_pos_sec = ref_clock_->GetRenderPos();
            double playback_pts = 0;

            if (is_playing_) {
                AttachCurrentThreadIfNeeded attached;
                JNIEnv *env = attached.jni();
                int64_t audio_track_time_us = env->CallLongMethod(
                        audioplay_by_audiotrack_service_(),
                        j_method_id_getCurrentPositionUsAudioTrack_);
                playback_pts = audio_track_time_us / 1000000.0;

                if (playback_pts > -TIME_EPS && project.media_asset_size() > 0) {
                    render_pos_sec = playback_pts;
                }
            }


            if (ref_clock_) {
                ref_clock_->SetPts(render_pos_sec);
            }
            if (player_time_message_center_) {
                player_time_message_center_->AddMessage(render_pos_sec);
            }
            return render_pos_sec;
        }

        void AudioPlayByAndroid::Release() {
            {
                std::lock_guard<std::mutex> lk(player_mutex_);
                request_stop_ = true;
            }
            if (play_thread_.joinable()) {
                play_thread_.join();
            }
            AttachCurrentThreadIfNeeded attached;
            JNIEnv *env = attached.jni();
            int ret = env->CallIntMethod(audioplay_by_audiotrack_service_(),
                                         j_method_id_releaseAudioTrack_);
            if (ret < 0) {
            }
        }

        AudioPlayByAndroid::~AudioPlayByAndroid() {
            Release();
        }

        bool AudioPlayByAndroid::Play() {
            std::lock_guard<std::mutex> lk(player_mutex_);
            if (is_inited_) {
                is_playing_ = true;
                AttachCurrentThreadIfNeeded attached;
                JNIEnv *env = attached.jni();
                env->CallVoidMethod(audioplay_by_audiotrack_service_(),
                                    j_method_id_playAudioTrack_);
                return true;
            }
            return false;
        }

        bool AudioPlayByAndroid::Pause() {
            std::lock_guard<std::mutex> lk(player_mutex_);
            if (is_inited_) {
                is_playing_ = false;
            }
            AttachCurrentThreadIfNeeded attached;
            JNIEnv *env = attached.jni();
            env->CallVoidMethod(audioplay_by_audiotrack_service_(), j_method_id_pauseAudioTrack_);
            return true;
        }

        void AudioPlayByAndroid::Flush() {
            std::lock_guard<std::mutex> lk(player_mutex_);
            AttachCurrentThreadIfNeeded attached;
            JNIEnv *env = attached.jni();
            env->CallVoidMethod(audioplay_by_audiotrack_service_(), j_method_id_flushAudioTrack_);
        }

        bool AudioPlayByAndroid::IsAudioPlaying() {
            std::lock_guard<std::mutex> lk(player_mutex_);
            return is_inited_ && is_playing_;

        }


        void AudioPlayByAndroid::SetAudioPlayGetObj(GetAudioDataCallback cb) {
            std::lock_guard<std::mutex> lk(player_mutex_);
            get_buff_func_ = cb;
        }
    }
}
