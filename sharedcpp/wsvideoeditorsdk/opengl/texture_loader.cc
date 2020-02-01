//
// Created by 何时夕 on 2020-01-07.
//

#include "platform_logger.h"
#include "av_utils.h"
#include "texture_loader.h"
#include "gl_utils.h"

namespace whensunset {
    namespace wsvideoeditor {

        TextureLoader::TextureLoader() {
            es3_supported_ = IsEs3Supported();
        }

        void TextureLoader::LoadDataToGlTexture(GLuint texture_id, int width, int height,
                                                uint8_t *raw_data, int stride, bool is_arbg_data) {

            SetSize(width, height);
            glBindTexture(GL_TEXTURE_2D, texture_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            uint8_t *data_to_texture = nullptr;
            if (es3_supported_) {
                glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
                data_to_texture = raw_data;
            } else {
                int copy_size = width * 4;
                if (copy_size == stride) {
                    data_to_texture = raw_data;
                } else {
                    if (!buffer_) {
                        buffer_.reset(new(std::nothrow) uint8_t[copy_size * height]);
                        if (!buffer_) {
                            return;
                        }
                    }
                    uint8_t *p = buffer_.get();
                    uint8_t *q = raw_data;
                    for (int i = 0; i < height; ++i) {
                        memcpy(p, q, copy_size);
                        p += copy_size;
                        q += stride;
                    }
                    data_to_texture = buffer_.get();
                }
            }

            glTexImage2D(GL_TEXTURE_2D, 0, is_arbg_data ? GL_RGBA : GL_LUMINANCE, width, height, 0,
                         is_arbg_data ? GL_RGBA : GL_LUMINANCE, GL_UNSIGNED_BYTE, data_to_texture);

            glBindTexture(GL_TEXTURE_2D, 0);
            if (es3_supported_) {
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            }
            LOGI("TextureLoader::LoadDataToGlTexture width:%d, height:%d, texture_id:%d, "
                 "stride:%d, is_arbg_data:%s, es3_supported_:%s", width, height, texture_id, stride,
                 BoTSt(is_arbg_data).c_str(), BoTSt(es3_supported_).c_str());
        }
    }
}