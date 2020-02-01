//
// Created by 何时夕 on 2020-01-05.
//

#include "platform_logger.h"
#include "av_utils.h"
#include "ws_final_draw_program.h"

namespace whensunset {
    namespace wsvideoeditor {

        static const char *GlvsWndVertexShader =
#ifdef GL_ES
                "precision highp float;  \n"
                #endif
                "attribute vec4 vPosition;\n"
                "attribute vec2 a_texCoord0;\n"
                "varying vec2 v_texCoord0;\n"
                "void main() {\n"
                "  gl_Position = vPosition;\n"
                "  v_texCoord0 = a_texCoord0;\n"
                "}\n";

        static const char *GlfsWndFragmentShader =
#ifdef GL_ES
                "precision highp float;  \n"
                #endif
                "uniform sampler2D ImageSampler;\n"
                "varying vec2 v_texCoord0;\n"
                "void main() {\n"
                "  vec2 coord = v_texCoord0.xy;\n"
                "  gl_FragColor = texture2D(ImageSampler, vec2(coord.x,coord.y));\n"
                "  \n"
                "}\n";

        void WsFinalDrawProgram::SetRenderSize(int render_width, int render_height) {
            LOGI("WsFinalDrawProgram::SetRenderSize render_width:%d, render_height:%d, "
                 "render_width_%d, render_height_:%d", render_width, render_height,
                 render_width_, render_height_);
            if (render_height == render_height_ && render_width == render_width_) {
                return;
            }
            render_width_ = render_width;
            render_height_ = render_height;
            UpdateViewTextureLocation();
        }

        void WsFinalDrawProgram::SetProjectSize(int project_width, int project_height) {
            if (project_height == project_height_ && project_width == project_width_) {
                return;
            }
            project_width_ = project_width;
            project_height_ = project_height;
            UpdateViewTextureLocation();
        }

        void WsFinalDrawProgram::CreateProgram(ShaderProgramPool *shader_program_pool) {
            ws_shader_program_.CreateProgram(shader_program_pool, GlvsWndVertexShader,
                                             GlfsWndFragmentShader);
        }

        void WsFinalDrawProgram::DrawGlTexture(WsTexture *ws_texture) {
            GLuint program_id;
            glUseProgram(ws_shader_program_.program_id());
            program_id = ws_shader_program_.program_id();
            glDisable(GL_BLEND);
            LOGI("WsFinalDrawProgram::DrawGlTexture program_id:%d, gl_texture:%d",
                 ws_shader_program_.program_id(), ws_texture->gl_texture());

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, render_width_, render_height_);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            GLuint gvPositionHandle = glGetAttribLocation(program_id, "vPosition");
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer(gvPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, vertex_pos_);
            glEnableVertexAttribArray(gvPositionHandle);

            GLint gvCoordHandle = glGetAttribLocation(program_id, "a_texCoord0");
            GLfloat texture_coordinate[4][2];
            memcpy(texture_coordinate, DefaultTextureCoordinates, sizeof texture_coordinate);
            std::swap(texture_coordinate[0][0], texture_coordinate[0 + 2][0]);
            std::swap(texture_coordinate[0][1], texture_coordinate[0 + 2][1]);
            std::swap(texture_coordinate[1][0], texture_coordinate[1 + 2][0]);
            std::swap(texture_coordinate[1][1], texture_coordinate[1 + 2][1]);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer(gvCoordHandle, 2, GL_FLOAT, GL_FALSE, 0, texture_coordinate);
            glEnableVertexAttribArray(gvCoordHandle);


            GLuint src_texture = ws_texture->gl_texture();
            GLint src_texture_min_filter = GL_LINEAR;
            GLint gvTexHandle = glGetUniformLocation(program_id, "ImageSampler");
            bool should_generate_mipmap = false;
            if (IsEs3Supported() &&
                (render_width_ < ws_texture->width() || render_height_ < ws_texture->height())) {
                src_texture_min_filter = GL_LINEAR_MIPMAP_LINEAR;
                should_generate_mipmap = true;
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, src_texture);
            if (should_generate_mipmap) {
                GetGlError();
                glGenerateMipmap(GL_TEXTURE_2D);
                if (GetGlError() != GL_NO_ERROR) {
                    // Failed to generate mipmap! fallback to GL_LINEAR
                    src_texture_min_filter = GL_LINEAR;
                    assert(false);
                }
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, src_texture_min_filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glUniform1i(gvTexHandle, 0);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            LOGI("WsFinalDrawProgram::DrawGlTexture program_id:%d, gl_texture:%d, "
                 "render_width_:%d, render_height_:%d, gvPositionHandle:%d, gvCoordHandle:%d, "
                 "gvTexHandle:%d, should_generate_mipmap:%s",
                 ws_shader_program_.program_id(), ws_texture->gl_texture(), render_width_,
                 render_height_, gvPositionHandle, gvCoordHandle, gvTexHandle,
                 BoTSt(should_generate_mipmap).c_str());
        }

        void WsFinalDrawProgram::UpdateViewTextureLocation() {
            if (project_width_ == 0 || project_height_ == 0 || render_width_ == 0 ||
                render_height_ == 0) {
                LOGE("WsFinalDrawProgram::UpdateViewTextureLocation dimension error "
                     "project_width_:%d, project_height_:%d, render_width_:%d, "
                     "render_height_:%d", project_width_, project_height_, render_width_,
                     render_height_);
                return;
            }

            int x_show = project_width_;
            int y_show = project_height_;
            int x_frame = render_width_;
            int y_frame = render_height_;
            float x_scale = 1, y_scale = 1;
            if (x_show > 0 && y_show > 0 && x_frame > 0 && y_frame > 0) {
                float x_ratio = x_show / (float) x_frame;
                float y_ratio = y_show / (float) y_frame;
                if (x_ratio > y_ratio) {
                    y_scale = y_ratio / x_ratio;
                } else {
                    x_scale = x_ratio / y_ratio;
                }
            }
            vertex_pos_[0] = DefaultVertexCoordinates[0][0] * x_scale;
            vertex_pos_[3] = DefaultVertexCoordinates[1][0] * x_scale;
            vertex_pos_[6] = DefaultVertexCoordinates[2][0] * x_scale;
            vertex_pos_[9] = DefaultVertexCoordinates[3][0] * x_scale;
            vertex_pos_[0 + 1] = DefaultVertexCoordinates[0][1] * y_scale;
            vertex_pos_[3 + 1] = DefaultVertexCoordinates[1][1] * y_scale;
            vertex_pos_[6 + 1] = DefaultVertexCoordinates[2][1] * y_scale;
            vertex_pos_[9 + 1] = DefaultVertexCoordinates[3][1] * y_scale;
            LOGE("WsFinalDrawProgram::UpdateViewTextureLocation x_show:%d, y_show:%d, x_frame:%d,"
                 " y_frame:%d, x_scale:%f, y_scale:%f", x_show, y_show, x_frame, y_frame,
                 x_scale, y_scale);
        }
    }
}
