//
// Created by 何时夕 on 2020-01-07.
//

#ifndef ANDROID_TEXTURE_LOADER_H
#define ANDROID_TEXTURE_LOADER_H

#include <memory>
#include "platform_gl_headers.h"

namespace whensunset {
    namespace wsvideoeditor {
        class TextureLoader {
        public:
            TextureLoader();

            void LoadDataToGlTexture(GLuint texture_id, int width, int height, uint8_t *raw_data,
                                     int stride,
                                     bool is_arbg_data);

        private:
            void SetSize(int width, int height) {
                if (width != width_ || height != height_) {
                    width_ = width;
                    height_ = height;
                    buffer_.reset();
                }
            }

            std::unique_ptr<uint8_t[]> buffer_;

            int width_ = 0;

            int height_ = 0;

            bool es3_supported_ = false;

        };

    }
}


#endif //ANDROID_TEXTURE_LOADER_H
