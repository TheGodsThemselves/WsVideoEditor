#include "shader_program_pool.h"
#include "platform_logger.h"

namespace whensunset {
    namespace wsvideoeditor {

        ShaderProgramPool::ShaderProgramPool() {
            ws_texture_pool_.reset(new(std::nothrow) WsTexturePool());
            if (!ws_texture_pool_) {
                abort();
            }
            LOGI("ShaderProgramPool::ShaderProgramPool");
        }

        WsFinalDrawProgram *ShaderProgramPool::GetWsFinalDrawProgram() {
            if (!ws_final_draw_program_) {
                ws_final_draw_program_.reset(new(std::nothrow) WsFinalDrawProgram());
                if (!ws_final_draw_program_) {
                    abort();
                }
                ws_final_draw_program_->CreateProgram(this);
            }
            return ws_final_draw_program_.get();
        }

        Yuv420ToRgbShaderProgram *ShaderProgramPool::GetYuv420ToRgbProgram() {
            if (!yuv420_to_rgb_program_) {
                yuv420_to_rgb_program_.reset(new(std::nothrow) Yuv420ToRgbShaderProgram());
                if (!yuv420_to_rgb_program_) {
                    abort();
                }
                yuv420_to_rgb_program_->CreateProgram(this);
            }
            LOGI("ShaderProgramPool::GetYuv420ToRgbProgram");
            return yuv420_to_rgb_program_.get();
        }

        CopyArgbShaderProgram *ShaderProgramPool::GetCopyBgraProgram() {
            if (!copy_bgra_program_) {
                copy_bgra_program_.reset(new(std::nothrow) CopyArgbShaderProgram(false));
                if (!copy_bgra_program_) {
                    abort();
                }
                copy_bgra_program_->CreateProgram(this);
            }
            return copy_bgra_program_.get();
        }

        CopyArgbShaderProgram *ShaderProgramPool::GetCopyRgbaProgram() {
            if (!copy_rgba_program_) {
                copy_rgba_program_.reset(new(std::nothrow) CopyArgbShaderProgram(true));
                if (!copy_rgba_program_) {
                    abort();
                }
                copy_rgba_program_->CreateProgram(this);
            }
            return copy_rgba_program_.get();
        }

        void ShaderProgramPool::Clear() {
            texture_map_.clear();
            ws_final_draw_program_.reset();
            yuv420_to_rgb_program_.reset();
            copy_rgba_program_.reset();
            copy_bgra_program_.reset();
            ws_texture_pool_.reset(new(std::nothrow) WsTexturePool());
            if (!ws_texture_pool_) {
                abort();
            }
        }

        UniqueWsTexturePtr ShaderProgramPool::GetWsTexturePtr(int width, int height) {
            return ws_texture_pool_->GetWsTexturePtr(width, height);
        }
    }
}
