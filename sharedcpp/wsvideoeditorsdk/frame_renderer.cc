//
// Created by 何时夕 on 2019-12-30.
//

#include "platform_logger.h"
#include "frame_renderer.h"
#include "opengl/platform_gl_headers.h"
#include "opengl/gl_utils.h"
#include "ws_editor_video_sdk_utils.h"

namespace whensunset {
    namespace wsvideoeditor {
        FrameRenderer::FrameRenderer() :
                avframe_rgba_texture_converter_(&shader_program_pool_),
                current_original_frame_texture_(UniqueWsTextureCreateNull()) {

        }

        FrameRenderer::~FrameRenderer() {
            current_original_frame_texture_.reset();
        }

        void FrameRenderer::SetEditorProject(const model::EditorProject &project) {
            std::lock_guard<std::mutex> lk(render_mutex_);
            project_ = project;
            project_changed_ = true;
        }

        void FrameRenderer::SetRenderSize(int render_width, int render_height) {
            std::lock_guard<std::mutex> lk(render_mutex_);
            render_width_ = render_width;
            render_height_ = render_height;
        }

        void FrameRenderer::Render(double render_pos, DecodedFramesUnit frame_unit) {
            bool is_new_frame = false;

            if (frame_unit.frame) {
                is_new_frame = true;
                current_frame_unit_ = std::move(frame_unit);
            }

            if (!current_frame_unit_) {
                std::lock_guard<std::mutex> lk(render_mutex_);
                glClearColor(1.0, 1.0, 1.0, 1.0);
                glClear(GL_COLOR_BUFFER_BIT);
                CheckGlError("glClear");
                return;
            }
            LOGI("FrameRenderer::Render is_new_frame:%s", BoTSt(is_new_frame).c_str());
            RenderInner(render_pos, is_new_frame);
        }

        void FrameRenderer::RenderInner(double render_pos, bool is_new_frame) {
            int render_width = 0;
            int render_height = 0;
            int project_width = 0;
            int project_height = 0;
            AVFrame *render_frame = current_frame_unit_.frame.get();
            model::EditorProject project;
            {
                std::lock_guard<std::mutex> lk(render_mutex_);
                render_height = render_height_;
                render_width = render_width_;
                project = project_;
                project_width = project.private_data().project_width();
                project_height = project.private_data().project_height();

                double current_frame_real_render_pos = render_pos;
                if (render_frame && render_frame->pts > 0) {
                    current_frame_real_render_pos = render_frame->pts / (double(AV_TIME_BASE));
                }
                int showing_media_asset_index = GetMediaAssetIndexByRenderPos(project,
                                                                              current_frame_real_render_pos);
                if ((showing_media_asset_index != showing_media_asset_index_ && is_new_frame) ||
                    project_changed_) {
                    project_changed_ = false;
                    showing_media_asset_rotation_ = GetMediaAssetRotation(
                            project.media_asset(showing_media_asset_index));
                    showing_media_asset_index_ = showing_media_asset_index;
                }
                LOGI("FrameRenderer::RenderInner render_height:%d, render_width:%d, "
                     "project_height:%d, project_width:%d, current_frame_real_render_pos:%f, "
                     "showing_media_asset_index:%d, project_changed_:%s, "
                     "showing_media_asset_rotation_:%d",
                     render_height,
                     render_width,
                     project_height, project_width, current_frame_real_render_pos,
                     showing_media_asset_index, BoTSt(project_changed_).c_str(),
                     showing_media_asset_rotation_);
            }

            if (is_new_frame || !current_original_frame_texture_) {
                current_original_frame_texture_ = avframe_rgba_texture_converter_.Convert(
                        render_frame,
                        (360 -
                         showing_media_asset_rotation_) %
                        360,
                        ProjectMaxOutputShortEdge(
                                project),
                        ProjectMaxOutputLongEdge(
                                project));
                if (!current_original_frame_texture_) {
                    return;
                }
            }

            GetWsFinalDrawProgram()->SetRenderSize(render_width, render_height);
            GetWsFinalDrawProgram()->SetProjectSize(project_width, project_height);
            GetWsFinalDrawProgram()->DrawGlTexture(current_original_frame_texture_.get());

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
            glDisable(GL_BLEND);
        }

        void FrameRenderer::ReleaseGLResource() {
            std::lock_guard<std::mutex> lk(render_mutex_);

            current_original_frame_texture_.reset();

            shader_program_pool_.Clear();

            glFinish();
        }
    }
}


