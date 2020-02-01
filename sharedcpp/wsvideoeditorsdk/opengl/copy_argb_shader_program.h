//
// Created by 何时夕 on 2020-01-07.
//

#ifndef ANDROID_COPY_ARGB_SHADER_PROGRAM_H
#define ANDROID_COPY_ARGB_SHADER_PROGRAM_H

extern "C" {
#include "libavutil/frame.h"
}

#include "ws_shader_program.h"
#include "texture_loader.h"

namespace whensunset {
    namespace wsvideoeditor {

        class CopyArgbShaderProgram final {
        public:
            CopyArgbShaderProgram(bool is_rgba_ = true) : is_rgba_(
                    is_rgba_) {}

            void CreateProgram(ShaderProgramPool *shader_program_pool);

            UniqueWsTexturePtr Run(const AVFrame *av_frame, int width, int height, int rotation);

            virtual ~CopyArgbShaderProgram();

        private:
            GLint texture_argb_glsl_param_ = -1;

            GLint texture_coordinate_glsl_param_ = -1;

            GLint vertex_coordinate_glsl_param_ = -1;

            GLuint texture_argb = 0;

            WsShaderProgram ws_shader_program_;

            TextureLoader texture_data_loader_;

            bool is_rgba_ = true;
        };
    }
}


#endif //ANDROID_COPY_ARGB_SHADER_PROGRAM_H
