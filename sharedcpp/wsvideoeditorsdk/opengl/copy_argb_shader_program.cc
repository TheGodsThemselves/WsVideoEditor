//
// Created by 何时夕 on 2020-01-07.
//

#include "copy_argb_shader_program.h"

namespace whensunset {
    namespace wsvideoeditor {

        static const char *GlvsCopyRgba =
                "    // Vertex Shader                                                                \n"
                "    attribute vec4 a_position;                                                      \n"
                "    attribute vec2 a_texCoord0;                                                     \n"
                "    varying vec2 v_texCoord0;                                                       \n"
                "    void main(void)                                                                 \n"
                "    {                                                                               \n"
                "        gl_Position = a_position;                                                   \n"
                "        v_texCoord0 = vec2(a_texCoord0.x,a_texCoord0.y);                        \n"
                "    }                                                                               \n";

        static const char *GlfsCopyRgba =
                "    // Pixel Shader                                                                 \n"
                #ifdef GL_ES
                "    precision mediump float;                                                        \n"
                #endif
                "    uniform sampler2D  ImageInput0;                                                 \n"
                "    varying vec2 v_texCoord0;                                                       \n"
                "                                                                                    \n"
                "    void main(void)                                                                 \n"
                "    {                                                                               \n"
                "        gl_FragColor = texture2D(ImageInput0, v_texCoord0.xy);                      \n"
                "    }                                                                               \n";

        static const char *GlvsCopyBgra =
                "                                                                                    \n"
                "    // Vertex Shader                                                                \n"
                "    attribute vec4 a_position;                                                      \n"
                "    attribute vec2 a_texCoord0;                                                     \n"
                "    varying vec2 v_texCoord0;                                                       \n"
                "    void main(void)                                                                 \n"
                "    {                                                                               \n"
                "        gl_Position = a_position;                                                   \n"
                "        v_texCoord0 = vec2(a_texCoord0.x,a_texCoord0.y);                        \n"
                "    }                                                                               \n";

        static const char *GlfsCopyBgra =
                "    // Pixel Shader                                                                 \n"
                #ifdef GL_ES
                "    precision mediump float;                                                        \n"
                #endif
                "    uniform sampler2D  ImageInput0;                                                 \n"
                "    varying vec2 v_texCoord0;                                                       \n"
                "                                                                                    \n"
                "    void main(void)                                                                 \n"
                "    {                                                                               \n"
                "        gl_FragColor = texture2D(ImageInput0, v_texCoord0.xy).bgra;                      \n"
                "    }                                                                               \n";

        void CopyArgbShaderProgram::CreateProgram(ShaderProgramPool *shader_program_pool) {
            bool ret;
            if (is_rgba_) {
                ret = ws_shader_program_.CreateProgram(shader_program_pool, GlvsCopyRgba,
                                                       GlfsCopyRgba);
            } else {
                ret = ws_shader_program_.CreateProgram(shader_program_pool, GlvsCopyBgra,
                                                       GlfsCopyBgra);
            }
            if (!ret) {
                return;
            }

            vertex_coordinate_glsl_param_ = glGetAttribLocation(ws_shader_program_.program_id(),
                                                                "a_position");

            texture_coordinate_glsl_param_ = glGetAttribLocation(ws_shader_program_.program_id(),
                                                                 "a_texCoord0");

            texture_argb_glsl_param_ = glGetUniformLocation(ws_shader_program_.program_id(),
                                                            "ImageInput0");
        }

        UniqueWsTexturePtr
        CopyArgbShaderProgram::Run(const AVFrame *av_frame, int width, int height,
                                   int rotation) {
            if (!texture_argb) {
                glGenTextures(1, &texture_argb);
            }

            texture_data_loader_.LoadDataToGlTexture(texture_argb, av_frame->width,
                                                     av_frame->height, av_frame->data[0],
                                                     av_frame->linesize[0], true);

            if (rotation % 180 == 90) {
                std::swap(width, height);
            }

            UniqueWsTexturePtr ws_texture_ptr = ws_shader_program_.GetWsTexturePtr(width, height);
            if (!ws_texture_ptr) {
                return UniqueWsTextureCreateNull();
            }
            UniqueWsTextureFboPtr ws_texture_fbo_ptr = UniqueWsTextureFboPtrCreate(
                    ws_texture_ptr.get());
            if (!ws_texture_fbo_ptr) {
                return UniqueWsTextureCreateNull();
            }
            glBindFramebuffer(GL_FRAMEBUFFER, ws_texture_fbo_ptr.get()->gl_frame_buffer());
            glViewport(0, 0, ws_texture_ptr.get()->width(), ws_texture_ptr.get()->height());
            glClearColor(0, 0, 0, 0);

            glUseProgram(ws_shader_program_.program_id());

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer((GLuint) vertex_coordinate_glsl_param_, 3, GL_FLOAT, GL_FALSE, 0,
                                  *VertexCoordWithRotation(rotation));
            glEnableVertexAttribArray((GLuint) vertex_coordinate_glsl_param_);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer((GLuint) texture_coordinate_glsl_param_, 2, GL_FLOAT, GL_FALSE, 0,
                                  *TextureCoordWithRotation(rotation));
            glEnableVertexAttribArray((GLuint) texture_coordinate_glsl_param_);

            glUniform1i(texture_argb_glsl_param_, 0);
            glBindTexture(GL_TEXTURE_2D, texture_argb);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                                GL_ONE_MINUS_SRC_ALPHA);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            return ws_texture_ptr;
        }

        CopyArgbShaderProgram::~CopyArgbShaderProgram() {
            if (texture_argb) {
                glDeleteTextures(1, &texture_argb);
                texture_argb = 0;
            }
        }
    }
}

