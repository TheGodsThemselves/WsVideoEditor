#ifndef ANDROID_WS_VIDEO_EDITOR_AUDIO_PLAY_BY_AUDIOTRACK_SERVICE_H
#define ANDROID_WS_VIDEO_EDITOR_AUDIO_PLAY_BY_AUDIOTRACK_SERVICE_H

#include "audio_player.h"
#include <jni.h>
#include <thread>
#include "jni_helper.h"

namespace whensunset {
    namespace wsvideoeditor {

        enum audio_channels {
            CHANNEL_OUT_FRONT_LEFT = 0x4,
            CHANNEL_OUT_FRONT_RIGHT = 0x8,
            CHANNEL_OUT_STEREO = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT)
        };

        class AudioPlayByAndroid : public AudioPlayer {
        public:
            AudioPlayByAndroid(TimeMessageCenter *player_time_message_center);

            virtual ~AudioPlayByAndroid();

            bool Play() override;

            bool Pause() override;

            void SetAudioPlayGetObj(GetAudioDataCallback cb) override;

            double GetCurrentTimeSec(const model::EditorProject &project) override;

            void Flush() override;


        private:
            GlobalRef<jobject> audioplay_by_audiotrack_service_;
            GlobalRef<jclass> j_class_audioplay_by_audiotrack_wrapper_;
            jmethodID j_method_id_j_class_audioplay_by_audiotrack_wrapper_ctor_;
            jmethodID j_method_id_initAudioTrack_;
            jmethodID j_method_id_writeAudioTrack_;
            jmethodID j_method_id_flushAudioTrack_;
            jmethodID j_method_id_pauseAudioTrack_;
            jmethodID j_method_id_playAudioTrack_;
            jmethodID j_method_id_releaseAudioTrack_;
            jmethodID j_method_id_getCurrentPositionUsAudioTrack_;
            std::thread play_thread_;
            bool request_stop_;

            void Init();

            int Start();

            void startPlayThread();

            void Release();
        };
    }
}
#endif
