//
// Created by 何时夕 on 2019-12-30.
//

#ifndef ANDROID_FRAME_RENDERER_H
#define ANDROID_FRAME_RENDERER_H

#include <string>
#include <mutex>
#include "opengl/avframe_rgba_texture_converter.h"
#include "opengl/ws_final_draw_program.h"
#include "opengl/shader_program_pool.h"
#include "ws_video_editor_sdk.pb.h"
#include "av_utils.h"

namespace whensunset {
    namespace wsvideoeditor {
        class FrameRenderer {
        public:
            FrameRenderer(const std::string tag = "FrameRenderer");

            virtual ~FrameRenderer();

            void SetEditorProject(const model::EditorProject &project);

            void SetRenderSize(int render_width, int render_height);

            void Render(double render_pos, DecodedFramesUnit frame_unit);

            void ReleaseGLResource();

            int showing_media_asset_index() const {
                return showing_media_asset_index_;
            }

        private:
            WsFinalDrawProgram *GetWsFinalDrawProgram() {
                std::lock_guard<std::mutex> lk(render_mutex_);
                return shader_program_pool_.GetWsFinalDrawProgram();
            }

            void RenderInner(double render_pos, bool is_new_frame);

            AVFrameRgbaTextureConverter avframe_rgba_texture_converter_;

            ShaderProgramPool shader_program_pool_;

            DecodedFramesUnit current_frame_unit_;

            std::mutex render_mutex_;

            UniqueWsTexturePtr current_original_frame_texture_;

            bool project_changed_;

            int showing_media_asset_index_;

            int showing_media_asset_rotation_;

            model::EditorProject project_;

            int render_height_;

            int render_width_;

            std::string tag_;

        };
    }
}

#endif //ANDROID_FRAME_RENDERER_H
