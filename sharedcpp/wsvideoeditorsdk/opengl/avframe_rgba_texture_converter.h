#ifndef SHAREDCPP_WS_VIDEO_EDITOR_OPENGL_AVFRAME_TEXTURE_CONVERTER
#define SHAREDCPP_WS_VIDEO_EDITOR_OPENGL_AVFRAME_TEXTURE_CONVERTER

#include "shader_program_pool.h"
#include "av_utils.h"

extern "C" {
#include "libswscale/swscale.h"
}

namespace whensunset {
    namespace wsvideoeditor {

        class AVFrameRgbaTextureConverter final {
        public:
            UniqueWsTexturePtr Convert(AVFrame *frame, int rotation, int short_edge, int long_edge);

            explicit AVFrameRgbaTextureConverter(ShaderProgramPool *pool) : shader_program_pool_(
                    pool) {}

            ~AVFrameRgbaTextureConverter();

        private:
            ShaderProgramPool *shader_program_pool_;

            SwsContext *sws_context_ = nullptr;

            UniqueAVFramePtr tmp_yuv_frame_{nullptr, FreeAVFrame};

        };

    }
}

#endif
