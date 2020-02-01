//
// Created by 何时夕 on 2020-01-01.
//
#include "platform_logger.h"
#include "ws_texture_fbo.h"

namespace whensunset {
    namespace wsvideoeditor {
        UniqueWsTexturePtr UniqueWsTextureCreateNull() {
            return UniqueWsTexturePtr{nullptr, WsTextureRelease};
        }

        void WsTextureRelease(WsTexture *ws_texture) {
            if (ws_texture) {
                if (ws_texture->ws_texture_pool()) {
                    ws_texture->ws_texture_pool()->RecycleWsTexture(ws_texture);
                } else {
                    delete ws_texture;
                }
            }
        }

        UniqueWsTextureFboPtr UniqueWsTextureFboPtrCreate(WsTexture *ws_texture) {
            if (!ws_texture) {
                return UniqueWsTextureFboPtr(nullptr);
            }

            WsTextureFbo *ws_texture_fbo = new(std::nothrow) WsTextureFbo(ws_texture);
            if (!ws_texture_fbo) {
                return UniqueWsTextureFboPtr(nullptr);
            }

            if (!ws_texture_fbo->is_created()) {
                return UniqueWsTextureFboPtr(nullptr);
            }
            return UniqueWsTextureFboPtr(ws_texture_fbo);
        }

        void ClearTextureColor(WsTexture *ws_texture) {
            if (!ws_texture) {
                return;
            }
            UniqueWsTextureFboPtr uniqueWsTextureFboPtr = UniqueWsTextureFboPtrCreate(ws_texture);
            if (!uniqueWsTextureFboPtr) {
                return;
            }
            glBindFramebuffer(GL_FRAMEBUFFER, uniqueWsTextureFboPtr->gl_frame_buffer());
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            LOGI("ClearTextureColor");
        }

        WsTexture::WsTexture(WsTexturePool *ws_texture_pool, int width, int height) {
            glGenTextures(1, &gl_texture_);
            glBindTexture(GL_TEXTURE_2D, gl_texture_);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            width_ = width;
            height_ = height;
            ws_texture_pool_ = ws_texture_pool;
            ClearTextureColor(this);
            LOGI("WsTexture::WsTexture width:%d, height:%d, gl_texture:%d", width, height,
                 gl_texture_);
        }

        WsTexture::~WsTexture() {
            if (!is_deleted_) {
                if (gl_texture_) {
                    glDeleteTextures(1, &gl_texture_);
                }
                is_deleted_ = true;
            }
        }

        UniqueWsTexturePtr WsTexturePool::GetWsTexturePtr(int width, int height) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto iter = texture_map_.find(std::make_pair(width, height));
            if (iter == texture_map_.end()) {
                auto ws_texture = new(std::nothrow) WsTexture(this, width, height);
                LOGI("WsTexturePool::GetWsTexturePtr create new WsTexture width:%d, height:%d",
                     width,
                     height);
                if (!ws_texture) {
                    return UniqueWsTextureCreateNull();
                } else {
                    return UniqueWsTexturePtr(ws_texture, WsTextureRelease);
                }
            } else {
                auto ws_texture = iter->second;
                ClearTextureColor(ws_texture);
                texture_map_.erase(iter);
                LOGI("WsTexturePool::GetWsTexturePtr find cached WsTexture width:%d, height:%d",
                     width, height);
                return UniqueWsTexturePtr(ws_texture, WsTextureRelease);
            }
        }

        void WsTexturePool::RecycleWsTexture(WsTexture *ws_texture) {
            std::lock_guard<std::mutex> lk(mutex_);
            if (ws_texture->is_deleted_) {
                delete ws_texture;
            } else {
                glFlush();
                texture_map_.insert(
                        std::make_pair(std::make_pair(ws_texture->width_, ws_texture->height_),
                                       ws_texture));
            }
        }

        WsTexturePool::~WsTexturePool() {
            std::lock_guard<std::mutex> lk(mutex_);
            for (auto iter = texture_map_.begin(); iter != texture_map_.end(); iter++) {
                delete iter->second;
            }
            texture_map_.clear();
        }

        WsTextureFbo::WsTextureFbo(WsTexture *ws_texture) {
            if (!ws_texture) {
                is_created_ = false;
                return;
            }

            glGenFramebuffers(1, &gl_frame_buffer_);
            glBindFramebuffer(GL_FRAMEBUFFER, gl_frame_buffer_);
            glBindTexture(GL_TEXTURE_2D, ws_texture->gl_texture());
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   ws_texture->gl_texture(), 0);
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                switch (status) {
                    case GL_FRAMEBUFFER_UNSUPPORTED:
                        break;
                    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                        break;
                    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                        break;
                    default:
                        break;
                }
                is_created_ = false;
                LOGE("WsTextureFbo::WsTextureFbo status:%d", status);
                return;
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            is_created_ = true;
            LOGI("WsTextureFbo::WsTextureFbo gl_frame_buffer_:%d, gl_texture:%d, status:%d",
                 gl_frame_buffer_, ws_texture->gl_texture(), status);
        }

        WsTextureFbo::~WsTextureFbo() {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteFramebuffers(1, &gl_frame_buffer_);
            is_created_ = false;
        }
    }
}
