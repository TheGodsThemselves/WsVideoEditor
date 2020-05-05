#include "platform_logger.h"
#include "native_ws_media_player.h"
#include "ws_editor_video_sdk_utils.h"
#include "wsvideoeditorsdkjni/audio_player_by_android.h"

namespace whensunset {
    namespace wsvideoeditor {

        constexpr std::chrono::milliseconds kReadyStateMinChangeInterval{300};

        const int HAVE_ENOUGH_VIDEO_DATA_THRESHOLD = 2;

        const int HAVE_ENOUGH_AUDIO_DATA_THRESHOLD = AUDIO_BUFFER_SIZE * 4;

        NativeWSMediaPlayer::NativeWSMediaPlayer() :
                frame_renderer_(),
                video_decode_service_(VideoDecodeServiceCreate(5)),
                audio_decode_service_(10),
                time_message_center_(2){

            time_message_center_.SetProcessFunction([&](double pts) {
                model::EditorProject project;
                {
                    std::lock_guard<std::mutex> lk(mutex_);
                    if (seeking_ || project_.media_asset_size() == 0) {
                        return;
                    }
                    project = project_;
                }

                bool played_to_end = (pts >= project_.private_data().project_duration() - PTS_EPS);

                {
                    std::lock_guard<std::mutex> lk(mutex_);
                    if (IsProjectTimelineChanged(project, project_)) {
                        return;
                    }
                    if (played_to_end && !ended_) {
                        PauseInternal();
                        ended_ = true;
                    }
                }
            });

            audio_player_.reset(
                    new(std::nothrow) AudioPlayByAndroid(&time_message_center_));
            audio_player_->SetRefClock(&audio_ref_clock_);
            audio_player_->SetAudioPlayGetObj(
                    [=](int *get_length, unsigned char *buff, int size, double *render_pos) {
                        *get_length = audio_decode_service_.GetAudio(buff, size, render_pos);
                    });
        }

        void NativeWSMediaPlayer::SetProject(const model::EditorProject &project) {
            std::unique_lock<std::mutex> lk(mutex_);

            bool is_project_timeline_changed = IsProjectTimelineChanged(project, project_);

            project_ = project;

            if (is_project_timeline_changed) {
                double pos_sec = 0.0;

                if (pos_sec <= TIME_EPS) {
                    pos_sec = 0;
                }

                video_decode_service_->SetProject(project, pos_sec);

                current_time_ = pos_sec;

                audio_player_->Pause();
                audio_player_->Flush();

                audio_ref_clock_.SetPts(pos_sec);

                RecalculateDecodeAndRenderState();
            } else {
                video_decode_service_->UpdateProject(project);

                audio_decode_service_.SetProject(project);
            }
            frame_renderer_.SetEditorProject(project);

            preview_time_line_.reset(new(std::nothrow) PreviewTimeline(project));
        }

        void NativeWSMediaPlayer::DrawFrame() {
            double current_time;
            std::chrono::system_clock::time_point last_user_seek_time;
            model::EditorProject project;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                if (!attached_ || project_.media_asset_size() == 0) {
                    return;
                }
                project = project_;
                current_time_ = current_time = GetRenderPos();
            }

            DecodedFramesUnit decoded_frames_unit = DecodedFramesUnitCreateNull();
            decoded_frames_unit = video_decode_service_->GetRenderFrameAtPtsOrNull(
                    current_time);
            if (!decoded_frames_unit.frame) {
                LOGI("DrawFrame frame is empty");
            }
            LOGI("NativeWSMediaPlayer::DrawFrame current_time:%f, paused_:%s, "
                 "decoded_frames_unit:%s", current_time,
                 BoTSt(paused_).c_str(), decoded_frames_unit.ToString().c_str());

            PlayerReadyState target_ready_state = kHaveEnoughData;
            bool should_update_ready_state = false;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                int video_buffered_frame_count = video_decode_service_->GetBufferedFrameCount();
                int audio_buffered_data_size = audio_decode_service_.GetBufferedDataSize();

                if ((!decoded_frames_unit.frame && video_buffered_frame_count <= 1 &&
                     !video_decode_service_->ended()) || audio_buffered_data_size == 0) {
                    if (std::chrono::system_clock::now() - last_have_enough_data_time_ >
                        kReadyStateMinChangeInterval) {
                        should_update_ready_state = true;
                        target_ready_state = kHaveMetaData;
                    }
                } else {
                    if ((video_buffered_frame_count >= HAVE_ENOUGH_VIDEO_DATA_THRESHOLD ||
                         video_decode_service_->ended()) &&
                        audio_buffered_data_size >= HAVE_ENOUGH_AUDIO_DATA_THRESHOLD) {
                        should_update_ready_state = true;
                        target_ready_state = kHaveEnoughData;
                    } else {
                        if (std::chrono::system_clock::now() - last_have_enough_data_time_ >
                            kReadyStateMinChangeInterval) {
                            should_update_ready_state = true;
                            target_ready_state = kHaveCurrentData;
                        }
                    }
                }
                LOGI("NativeWSMediaPlayer::DrawFrame target_ready_state:%d, "
                     "video_buffered_frame_count:%d, audio_buffered_data_size:%d",
                     target_ready_state, video_buffered_frame_count, audio_buffered_data_size);
            }
            if (should_update_ready_state) {
                UpdateReadyState(target_ready_state);
            }
            frame_renderer_.Render(current_time, std::move(decoded_frames_unit));

            glFinish();
        }

        void NativeWSMediaPlayer::UpdateReadyState(const PlayerReadyState &new_ready_state) {
            std::lock_guard<std::mutex> lk(mutex_);
            if (new_ready_state >= kHaveEnoughData && ready_state_ < kHaveEnoughData) {
                last_have_enough_data_time_ = std::chrono::system_clock::now();
            }
            if (new_ready_state >= kHaveCurrentData && !paused_) {
                audio_player_->Play();
            }

            ready_state_ = new_ready_state;
        }

        void NativeWSMediaPlayer::OnAttachedToController(int width, int height) {
            std::lock_guard<std::mutex> lk(mutex_);
            attached_ = true;
            frame_renderer_.SetRenderSize(width, height);
            RecalculateDecodeAndRenderState();
        }

        void NativeWSMediaPlayer::OnDetachedFromController() {
            std::lock_guard<std::mutex> lk(mutex_);
            if (!attached_) {
                return;
            }
            attached_ = false;
            frame_renderer_.ReleaseGLResource();
            RecalculateDecodeAndRenderState();
        }

        void NativeWSMediaPlayer::RecalculateDecodeAndRenderState() {
            bool should_render = attached_;
            bool should_decode = attached_;

            if (should_decode) {
                ResumeDecode();
            } else {
                PauseDecode();
            }

            if (should_render) {
                ResumeRender();
            } else {
                PauseRender();
            }
        }

        void NativeWSMediaPlayer::PauseRender() {
            if (audio_player_) {
                audio_player_->Pause();
            }
            is_render_paused_ = true;
        }

        void NativeWSMediaPlayer::ResumeRender() {
            if (is_render_paused_) {
                is_render_paused_ = false;
            }
        }

        void NativeWSMediaPlayer::PauseDecode() {
            video_decode_service_->Stop();
            audio_decode_service_.Stop();
        }

        void NativeWSMediaPlayer::ResumeDecode() {
            if (video_decode_service_->stopped()) {
                video_decode_service_->SetProject(project_, current_time_);
                video_decode_service_->Start();
            }

            if (audio_decode_service_.is_stopped()) {
                audio_decode_service_.SetProject(project_, current_time_);
                audio_ref_clock_.SetPts(current_time_);
                audio_decode_service_.Start();
            }
        }

        void NativeWSMediaPlayer::Seek(double current_time) {
            std::lock_guard<std::mutex> lk(mutex_);
        }

        void NativeWSMediaPlayer::SeekInternal(double render_pos) {
            audio_player_->Pause();

            audio_player_->Flush();

            ended_ = false;
            seeking_ = true;

            current_time_ = render_pos;
            video_decode_service_->Seek(render_pos);
            audio_decode_service_.ResetDecodePosition(render_pos);

            audio_ref_clock_.SetPts(render_pos);
        }

        void NativeWSMediaPlayer::Play() {
            std::lock_guard<std::mutex> lk(mutex_);
            if (!paused_) {
                return;
            }
            if (ended_) {
                SeekInternal(0);
                ended_ = false;
            }
            paused_ = false;
        }

        void NativeWSMediaPlayer::Pause() {
            std::lock_guard<std::mutex> lk(mutex_);
            PauseInternal();
        }

        void NativeWSMediaPlayer::PauseInternal() {
            if (paused_) {
                return;
            }
            audio_player_->Pause();
            paused_ = true;
        }

        bool NativeWSMediaPlayer::paused() {
            std::lock_guard<std::mutex> lk(mutex_);
            return paused_;
        }

        NativeWSMediaPlayer::~NativeWSMediaPlayer() {
            std::lock_guard<std::mutex> lk(mutex_);
            video_decode_service_->Stop();
            audio_decode_service_.Stop();
        }
    }
}
