//
// Created by 何时夕 on 2020-03-01.
//

#ifndef ANDROID_AUDIO_PLAYER_H
#define ANDROID_AUDIO_PLAYER_H

#include "constants.h"
#include <cstdint>
#include <mutex>
#include "ref_clock.h"
#include "time_message_center.h"

namespace whensunset {
    namespace wsvideoeditor {

        typedef std::function<void(int *get_length, unsigned char *buff, int size,
                                   double *render_pos)> GetAudioDataCallback;

        class AudioPlayer {
        public:
            AudioPlayer(TimeMessageCenter *player_time_message_center)
                    : player_time_message_center_(
                    player_time_message_center) {}

            virtual ~AudioPlayer() {}

            virtual bool Play() = 0;

            virtual bool Pause() = 0;

            virtual void Flush() = 0;

            virtual bool IsAudioPlaying() = 0;

            virtual void SetAudioPlayGetObj(GetAudioDataCallback cb) = 0;

            virtual double GetCurrentTimeSec(const model::EditorProject &project) = 0;

            void SetRefClock(RefClock *ref_clock) {
                std::lock_guard<std::mutex> lk(clock_mutex_);
                ref_clock_ = ref_clock;
            }

            void SetAudioParam(int samplerate, int channel) {
                sample_rate_ = samplerate;
                channel_config_ = channel;
            }

        protected:
            std::unique_ptr<uint8_t[]> buff_;

            int buff_size_;

            RefClock *ref_clock_ = nullptr;

            bool is_playing_ = false;

            bool is_inited_ = false;

            GetAudioDataCallback get_buff_func_;

            int sample_rate_;

            int channel_config_;

            std::mutex clock_mutex_;

            std::mutex player_mutex_;

            int play_retry_times_ = 0;

            TimeMessageCenter *player_time_message_center_ = nullptr;
        };
    }
}

#endif //ANDROID_AUDIO_PLAYER_H
