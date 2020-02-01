#include "platform_logger.h"
#include "av_utils.h"
#include "prebuilt_protobuf/ws_video_editor_sdk.pb.h"
#include "yuv420_to_rgba_shader_program.h"

namespace {
    static const GLfloat kBt709VideoRangeYUV2RGBMatrix[] = {
            1.164, 1.164, 1.164,
            0.0, -0.213, 2.112,
            1.793, -0.533, 0.0,
    };

    static const GLfloat kBt709FullRangeYUV2RGBMatrix[] = {
            1.164, 1.164, 1.164,
            0.0, -0.213, 2.112,
            1.793, -0.533, 0.0,
    };

    static const GLfloat kBt601VideoRangeYUV2RGBMatrix[] = {
            1.164, 1.164, 1.164,
            0.0, -0.392, 2.017,
            1.596, -0.813, 0.0,
    };

    static const GLfloat kBt601FullRangeYUV2RGBMatrix[] = {
            1.0, 1.0, 1.0,
            0.0, -0.343, 1.765,
            1.4, -0.711, 0.0,
    };

    static const char *kGlvsYuv420pToRgb =
            "    // Vertex Shader                                                                \n"
            #ifdef GL_ES
            "    precision highp float;                                                           \n"
            #endif
            "    attribute vec4 a_position;                                                      \n"
            "    attribute vec2 a_texCoord0;                                                     \n"
            "    varying vec2 v_texCoord0;                                                       \n"
            "    void main(void)                                                                 \n"
            "    {                                                                               \n"
            "        gl_Position = a_position;                                                   \n"
            "        v_texCoord0 = vec2(a_texCoord0.x,a_texCoord0.y);                        \n"
            "    }                                                                               \n";

    static const char *kGlfsYuv420pToRgb =
            "    // Pixel Shader                                                                 \n"
            #ifdef GL_ES
            "    precision highp float;                                                           \n"
            "    precision lowp sampler2D;                                                           \n"
            #endif
            "    uniform sampler2D  ImageInput0    ;                                             \n"
            "    uniform sampler2D  ImageInput1    ;                                             \n"
            "    uniform sampler2D  ImageInput2    ;                                             \n"
            "    varying vec2 v_texCoord0;                                                       \n"
            "    uniform float y_offset;                                                         \n"
            "    uniform mat3 um3_ColorConversion;                            \n"
            "                                                                                    \n"
            "    void main(void)                                                                 \n"
            "    {                                                                               \n"
            "        vec3 yuv;                                                           \n"
            "        vec3 rgb;                                                              \n"
            "        yuv.x = texture2D(ImageInput0, v_texCoord0.xy).r - (y_offset / 255.0);      \n"
            "        yuv.y = texture2D(ImageInput1, v_texCoord0.xy).r - 0.5;                     \n"
            "        yuv.z = texture2D(ImageInput2, v_texCoord0.xy).r - 0.5;                     \n"
            "        rgb = um3_ColorConversion * yuv;                      \n"
            "                                                          \n"
            "        gl_FragColor = vec4(rgb, 1.0);                                             \n"
            "    }                                                                               \n";
}  // namespace

namespace whensunset {
    namespace wsvideoeditor {

        void Yuv420ToRgbShaderProgram::CreateProgram(ShaderProgramPool *shader_program_pool_) {
            bool ret = program_.CreateProgram(shader_program_pool_, kGlvsYuv420pToRgb,
                                              kGlfsYuv420pToRgb);
            if (!ret) {
                return;
            }

            location_tex_y_ = glGetUniformLocation(program_.program_id(), "ImageInput0");
            location_tex_u_ = glGetUniformLocation(program_.program_id(), "ImageInput1");
            location_tex_v_ = glGetUniformLocation(program_.program_id(), "ImageInput2");

            location_pos_ = glGetAttribLocation(program_.program_id(), "a_position");
            location_tex_ = glGetAttribLocation(program_.program_id(), "a_texCoord0");

            um3_color_conversion_ = glGetUniformLocation(program_.program_id(),
                                                         "um3_ColorConversion");
            y_offset_ = glGetUniformLocation(program_.program_id(), "y_offset");
            CheckGlError("glGetUniformLocation");
            LOGI("Yuv420ToRgbShaderProgram::CreateProgram location_tex_y_:%d, "
                 "location_tex_u_:%d, location_tex_v_:%d, location_pos:%d, location_tex:%d, "
                 "um3_color_conversion_:%d, y_offset:%d",
                 location_tex_y_, location_tex_u_, location_tex_v_, location_pos_,
                 location_tex_, um3_color_conversion_, y_offset_);
        }

        UniqueWsTexturePtr
        Yuv420ToRgbShaderProgram::Run(const AVFrame *frame, int width, int height, int rotation) {

            glUseProgram(program_.program_id());

            if (!is_texture_generated_) {
                glGenTextures(1, &tex_y_);
                glGenTextures(1, &tex_u_);
                glGenTextures(1, &tex_v_);
                is_texture_generated_ = true;
            }

            y_loader_.LoadDataToGlTexture(tex_y_, frame->width, frame->height,
                                          frame->data[0], frame->linesize[0], false);
            uv_loader_.LoadDataToGlTexture(tex_u_, frame->width / 2, frame->height / 2,
                                           frame->data[1], frame->linesize[1], false);
            uv_loader_.LoadDataToGlTexture(tex_v_, frame->width / 2, frame->height / 2,
                                           frame->data[2], frame->linesize[2], false);

            if (rotation % 180 == 90) {
                std::swap(width, height);
            }

            UniqueWsTexturePtr ws_texture_ptr = program_.GetWsTexturePtr(width, height);
            if (!ws_texture_ptr) {
                return UniqueWsTextureCreateNull();
            }
            UniqueWsTextureFboPtr ws_texture_fbo_wrapper = UniqueWsTextureFboPtrCreate(
                    ws_texture_ptr.get());
            if (!ws_texture_fbo_wrapper) {
                return UniqueWsTextureCreateNull();
            }
            glBindFramebuffer(GL_FRAMEBUFFER, ws_texture_fbo_wrapper->gl_frame_buffer());
            glViewport(0, 0, ws_texture_ptr->width(), ws_texture_ptr->height());

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex_y_);
            glUniform1i(location_tex_y_, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, tex_u_);
            glUniform1i(location_tex_u_, 1);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, tex_v_);
            glUniform1i(location_tex_v_, 2);


            if (frame->color_range == AVCOL_RANGE_JPEG) {
                glUniform1f(y_offset_, 0.0f);
                if (frame->colorspace == AVCOL_SPC_BT709) {
                    glUniformMatrix3fv(um3_color_conversion_, 1, GL_FALSE,
                                       kBt709FullRangeYUV2RGBMatrix);
                } else {
                    glUniformMatrix3fv(um3_color_conversion_, 1, GL_FALSE,
                                       kBt601FullRangeYUV2RGBMatrix);
                }
            } else {
                glUniform1f(y_offset_, 16.0f);
                if (frame->colorspace == AVCOL_SPC_BT709) {
                    glUniformMatrix3fv(um3_color_conversion_, 1, GL_FALSE,
                                       kBt709VideoRangeYUV2RGBMatrix);
                } else {
                    glUniformMatrix3fv(um3_color_conversion_, 1, GL_FALSE,
                                       kBt601VideoRangeYUV2RGBMatrix);
                }
            }

            glActiveTexture(GL_TEXTURE0);
            glViewport(0, 0, ws_texture_ptr->width(), ws_texture_ptr->height());

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer((GLuint) location_pos_, 3, GL_FLOAT, GL_FALSE, 0,
                                  *VertexCoordWithRotation(rotation));
            glEnableVertexAttribArray((GLuint) location_pos_);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glVertexAttribPointer((GLuint) location_tex_, 2, GL_FLOAT, GL_FALSE, 0,
                                  *TextureCoordWithRotation(rotation));
            glEnableVertexAttribArray((GLuint) location_tex_);

            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                                GL_ONE_MINUS_SRC_ALPHA);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            LOGI("Yuv420ToRgbShaderProgram::Run width:%d, height:%d, rotation:%d, "
                 "is_texture_generated_:%s, tex_y_:%d, tex_u_:%d, tex_v_:%d, frame->width:%d, "
                 "frame->height:%d, location_tex_y_:%d, location_tex_u_:%d, location_tex_v_:%d, "
                 "ws_texture_ptr->width:%d, ws_texture_ptr->height:%d, gl_frame_buffer:%d, "
                 "color_range:%d, colorspace:%d, location_pos_:%d, location_tex_:%d, y_offset_:%d",
                 width, height,
                 rotation, BoTSt(is_texture_generated_).c_str(), tex_y_, tex_u_,
                 tex_v_, frame->width, frame->height, location_tex_y_, location_tex_u_,
                 location_tex_v_, ws_texture_ptr->width(), ws_texture_ptr->height(),
                 ws_texture_fbo_wrapper->gl_frame_buffer(), frame->color_range,
                 frame->colorspace, location_pos_, location_tex_, y_offset_);
            return ws_texture_ptr;
        }

        Yuv420ToRgbShaderProgram::~Yuv420ToRgbShaderProgram() {
            if (tex_y_) {
                glDeleteTextures(1, &tex_y_);
                tex_y_ = 0;
            }
            if (tex_u_) {
                glDeleteTextures(1, &tex_u_);
                tex_u_ = 0;
            }
            if (tex_v_) {
                glDeleteTextures(1, &tex_v_);
                tex_v_ = 0;
            }
        }
    }
}
