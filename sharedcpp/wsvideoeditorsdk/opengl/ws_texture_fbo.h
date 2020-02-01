//
// Created by 何时夕 on 2020-01-01.
//

#ifndef ANDROID_WS_TEXTURE_FBO_H
#define ANDROID_WS_TEXTURE_FBO_H

#include "platform_gl_headers.h"
#include "gl_utils.h"
#include <mutex>
#include <map>
#include <thread>

namespace whensunset {
    namespace wsvideoeditor {
        class TextureProviderInterface {
        public:
            virtual ~TextureProviderInterface() {}

            virtual GLuint gl_texture() const = 0;

            virtual int width() const = 0;

            virtual int height() const = 0;
        };

        class WsTexture;

        class WsTexturePool;

        class WsTextureFbo;

        typedef std::unique_ptr<WsTexture, void (*)(WsTexture *)> UniqueWsTexturePtr;

        typedef std::unique_ptr<WsTextureFbo> UniqueWsTextureFboPtr;

        UniqueWsTexturePtr UniqueWsTextureCreateNull();

        void WsTextureRelease(WsTexture *ws_texture);

        UniqueWsTextureFboPtr UniqueWsTextureFboPtrCreate(WsTexture *ws_texture);

        void ClearTextureColor(WsTexture *ws_texture);

        class WsTexture : TextureProviderInterface {
        public:
            virtual ~WsTexture();

            GLuint gl_texture() const { return gl_texture_; };

            int width() const { return width_; };

            int height() const { return height_; };

            WsTexturePool *ws_texture_pool() const {
                return ws_texture_pool_;
            }

        private:
            friend class WsTexturePool;

            WsTexture(WsTexturePool *ws_texture_pool, int width, int height);

            WsTexturePool *ws_texture_pool_;

            GLuint gl_texture_;

            int width_;

            int height_;

            bool is_deleted_ = false;
        };

        class WsTexturePool {
        public:
            UniqueWsTexturePtr GetWsTexturePtr(int width, int height);

            void RecycleWsTexture(WsTexture *ws_texture);

            virtual ~WsTexturePool();

        private:
            std::mutex mutex_;

            std::map<std::pair<int, int>, WsTexture *> texture_map_;
        };

        class WsTextureFbo {
        public:
            WsTextureFbo(WsTexture *ws_texture);

            GLuint gl_frame_buffer() const {
                return gl_frame_buffer_;
            }

            bool is_created() const {
                return is_created_;
            }

            virtual ~WsTextureFbo();

        private:
            GLuint gl_frame_buffer_ = 0;

            bool is_created_ = false;
        };
    }
};

#endif //ANDROID_WS_TEXTURE_FBO_H
