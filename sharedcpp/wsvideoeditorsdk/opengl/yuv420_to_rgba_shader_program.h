#ifndef SHAREDCPP_WS_VIDEO_EDITOR_SHADER_PROGRAM_YUV420_TO_RGB_H
#define SHAREDCPP_WS_VIDEO_EDITOR_SHADER_PROGRAM_YUV420_TO_RGB_H

extern "C" {
#include "libavutil/frame.h"
};

#include <memory>
#include "ws_shader_program.h"
#include "platform_gl_headers.h"
#include "texture_loader.h"

namespace whensunset {
    namespace wsvideoeditor {

        class Yuv420ToRgbShaderProgram final {
        public:

            void CreateProgram(ShaderProgramPool *shader_program_pool_);

            UniqueWsTexturePtr Run(const AVFrame *frame, int width, int height, int rotation);

            ~Yuv420ToRgbShaderProgram();

        private:
            GLint location_tex_y_ = -1;
            GLint location_tex_u_ = -1;
            GLint location_tex_v_ = -1;
            GLint location_pos_ = -1;
            GLint location_tex_ = -1;
            GLuint tex_y_ = 0;
            GLuint tex_u_ = 0;
            GLuint tex_v_ = 0;
            GLint um3_color_conversion_;
            GLfloat y_offset_;

            bool is_texture_generated_ = false;
            TextureLoader y_loader_;
            TextureLoader uv_loader_;
            WsShaderProgram program_;
        };
    }
}
#endif
