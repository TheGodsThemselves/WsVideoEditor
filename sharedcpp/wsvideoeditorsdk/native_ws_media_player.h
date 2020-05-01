#ifndef SHAREDCPP_WS_VIDEO_EDITOR_NATIVE_WS_MEDIA_PLAYER_H
#define SHAREDCPP_WS_VIDEO_EDITOR_NATIVE_WS_MEDIA_PLAYER_H

#include <deque>
#include "libffmpeg/config.h"
#include <atomic>
#include <wsvideoeditorsdk/audio_decode/audio_decode_service.h>

#include "video_decode_service.h"
#include "frame_renderer.h"
#include "constants.h"

#include "opengl/shader_program_pool.h"
#include "opengl/avframe_rgba_texture_converter.h"
#include "preview_timeline.h"
#include "decode_service_common.h"
#include "audio_player.h"

namespace whensunset {
    namespace wsvideoeditor {

        class NativeWSMediaPlayer {
        public:
            NativeWSMediaPlayer();

            virtual ~NativeWSMediaPlayer();

            void SetProject(const model::EditorProject &project);

            void DrawFrame();

            void OnAttachedToController(int width, int height);

            void OnDetachedFromController();

            void Seek(double current_time);

            void Play();

            void Pause();

            bool paused();

            const model::EditorProject &project() {
                std::lock_guard<std::mutex> lk(mutex_);
                return project_;
            }

            double current_time() {
                return current_time_;
            }

            bool attached() {
                std::lock_guard<std::mutex> lk(mutex_);
                return attached_;
            }

            double GetRenderPos() {
                if (seeking_) {
                    return current_time_;
                }
                return audio_player_->GetCurrentTimeSec(project_);
            }

        private:
            void UpdateReadyState(const PlayerReadyState &new_ready_state);

            void SeekInternal(double render_pos);

            void PauseInternal();

            void PauseDecode();

            void ResumeDecode();

            void PauseRender();

            void ResumeRender();

            void RecalculateDecodeAndRenderState();

            bool is_render_paused_ = true;

            bool attached_ = false;

            bool ended_ = false;

            bool paused_ = true;

            bool seeking_ = false;

            mutable std::mutex mutex_;

            double current_time_ = 0.0;

            FrameRenderer frame_renderer_;

            std::unique_ptr<VideoDecodeService> video_decode_service_;

            std::unique_ptr<PreviewTimeline> preview_time_line_;

            model::EditorProject project_;

            std::unique_ptr<AudioPlayer> audio_player_;

            AudioDecodeService audio_decode_service_;

            RefClock audio_ref_clock_;

            TimeMessageCenter time_message_center_;

            PlayerReadyState ready_state_ = kHaveNothing;

            std::chrono::system_clock::time_point last_have_enough_data_time_;
        };
    }
}
#endif
