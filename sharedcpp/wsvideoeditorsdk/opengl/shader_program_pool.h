#ifndef SHAREDCPP_WS_VIDEO_EDITOR_SHADER_PROGRAM_POOL_H
#define SHAREDCPP_WS_VIDEO_EDITOR_SHADER_PROGRAM_POOL_H

#include <memory>
#include <unordered_map>
#include <list>
#include <utility>
#include "yuv420_to_rgba_shader_program.h"
#include "ws_final_draw_program.h"
#include "copy_argb_shader_program.h"

namespace whensunset {
    namespace wsvideoeditor {

        class ShaderProgramPool {
        public:
            ShaderProgramPool();

            WsFinalDrawProgram *GetWsFinalDrawProgram();

            Yuv420ToRgbShaderProgram *GetYuv420ToRgbProgram();

            CopyArgbShaderProgram *GetCopyRgbaProgram();

            CopyArgbShaderProgram *GetCopyBgraProgram();

            void Clear();

            UniqueWsTexturePtr GetWsTexturePtr(int width, int height);

        private:
            std::unique_ptr<WsFinalDrawProgram> ws_final_draw_program_;

            std::unique_ptr<Yuv420ToRgbShaderProgram> yuv420_to_rgb_program_;

            std::unique_ptr<CopyArgbShaderProgram> copy_rgba_program_;

            std::unique_ptr<CopyArgbShaderProgram> copy_bgra_program_;

            std::map<std::string, UniqueWsTexturePtr> texture_map_;

            std::unique_ptr<WsTexturePool> ws_texture_pool_;
        };
    }
}
#endif
