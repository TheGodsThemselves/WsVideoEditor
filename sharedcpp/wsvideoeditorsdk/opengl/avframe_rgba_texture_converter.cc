#include "avframe_rgba_texture_converter.h"
#include <memory>
#include "ws_editor_video_sdk_utils.h"
#include "base/platform_logger.h"

namespace whensunset {
    namespace wsvideoeditor {

        AVFrameRgbaTextureConverter::~AVFrameRgbaTextureConverter() {
            if (sws_context_) {
                sws_freeContext(sws_context_);
                sws_context_ = nullptr;
            }
        }

        UniqueWsTexturePtr
        AVFrameRgbaTextureConverter::Convert(AVFrame *frame, int rotation, int short_edge,
                                             int long_edge) {
            assert(frame);
            rotation = (rotation % 360 + 360) % 360;

            int frame_width = FrameDisplayWidth(frame);
            int frame_height = FrameDisplayHeight(frame);
            LimitWidthAndHeight(frame_width, frame_height, short_edge, long_edge,
                                &frame_width, &frame_height);
            LOGI("AVFrameRgbaTextureConverter::Convert frame_width:%d, frame_height:%d, "
                 "short_edge:%d, long_edge:%d, format:%d",
                 frame_width, frame_height, short_edge,
                 long_edge, frame->format);

            if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P) {
                return shader_program_pool_->GetYuv420ToRgbProgram()->Run(frame, frame_width,
                                                                          frame_height,
                                                                          rotation);
            }
            if (frame->format == AV_PIX_FMT_BGRA) {
                return shader_program_pool_->GetCopyBgraProgram()->Run(frame, frame_width,
                                                                       frame_height, rotation);
            }
            if (frame->format == AV_PIX_FMT_RGBA) {
                return shader_program_pool_->GetCopyRgbaProgram()->Run(frame, frame_width,
                                                                       frame_height, rotation);
            }


            sws_context_ = sws_getCachedContext(sws_context_,
                                                frame->width, frame->height,
                                                (AVPixelFormat) frame->format,
                                                frame->width, frame->height, AV_PIX_FMT_YUV420P,
                                                SWS_FAST_BILINEAR, NULL, NULL, NULL);
            if (!tmp_yuv_frame_) {
                tmp_yuv_frame_.reset(
                        AllocVideoFrame(AV_PIX_FMT_YUV420P, frame->width, frame->height));
            }
            sws_scale(sws_context_, (const uint8_t *const *) frame->data, frame->linesize, 0,
                      frame->height,
                      tmp_yuv_frame_->data, tmp_yuv_frame_->linesize);
            return shader_program_pool_->GetYuv420ToRgbProgram()->Run(tmp_yuv_frame_.get(),
                                                                      frame_width, frame_height,
                                                                      rotation);
        }
    }
}
