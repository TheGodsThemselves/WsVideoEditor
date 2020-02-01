#include "platform_logger.h"
#include "native_ws_media_player.h"
#include "ws_editor_video_sdk_utils.h"

namespace whensunset {
    namespace wsvideoeditor {

        NativeWSMediaPlayer::NativeWSMediaPlayer() :
                frame_renderer_("WSMediaPlayer"),
                video_decode_service_(VideoDecodeServiceCreate(5)) {
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
                RecalculateDecodeAndRenderState();
            } else {
                video_decode_service_->UpdateProject(project);

                frame_renderer_.SetEditorProject(project);
            }
            frame_renderer_.SetEditorProject(project);

            preview_time_line_.reset(new(std::nothrow) PreviewTimeline(project));
        }

        // todo test code
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
                return;
            }
            if (!paused_) {
                test_current_time += 0.04;
            }
            LOGI("NativeWSMediaPlayer::DrawFrame  current_time:%f, paused_:%s", current_time,
                 BoTSt(paused_).c_str());
            frame_renderer_.Render(current_time, std::move(decoded_frames_unit));

            glFinish();
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
            is_render_paused_ = true;
        }

        void NativeWSMediaPlayer::ResumeRender() {
            if (is_render_paused_) {
                is_render_paused_ = false;
            }
        }

        void NativeWSMediaPlayer::PauseDecode() {
            video_decode_service_->Stop();
        }

        void NativeWSMediaPlayer::ResumeDecode() {
            if (video_decode_service_->stopped()) {
                video_decode_service_->SetProject(project_, current_time_);
                video_decode_service_->Start();
            }
        }

        void NativeWSMediaPlayer::Seek(double current_time) {
            std::lock_guard<std::mutex> lk(mutex_);
        }

        void NativeWSMediaPlayer::SeekInternal(double render_pos) {
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
            paused_ = true;
        }

        bool NativeWSMediaPlayer::paused() {
            std::lock_guard<std::mutex> lk(mutex_);
            return paused_;
        }

        NativeWSMediaPlayer::~NativeWSMediaPlayer() {
            std::lock_guard<std::mutex> lk(mutex_);
            video_decode_service_->Stop();
        }
    }
}
