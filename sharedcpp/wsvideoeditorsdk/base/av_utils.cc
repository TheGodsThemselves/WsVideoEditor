#include "av_utils.h"
#include "platform_logger.h"

namespace whensunset {
    namespace wsvideoeditor {

        std::string BoTSt(bool is) {
            return is ? "true" : "false";
        }

        UniqueAVFramePtr UniqueAVFramePtrCreateNull() {
            return UniqueAVFramePtr{nullptr, FreeAVFrame};
        }

        void FreeAVFrame(AVFrame *frame) {
            av_frame_free(&frame);
        }

        void FreeAVPacket(AVPacket *packet) {
            av_packet_free(&packet);
        }

        DecodedFramesUnit DecodedFramesUnitCreateNull() {
            return DecodedFramesUnit();
        }

        void SetCurrentThreadName(const std::string &name) {
            pthread_setname_np(pthread_self(), name.c_str());
        }

        AVFrame *AllocVideoFrame(AVPixelFormat pix_fmt, int width, int height) {
            AVFrame *frame = av_frame_alloc();
            if (!frame) {
                LOGE("Failed to av_frame_alloc. This means serious memory outage.");
                return NULL;
            }

            frame->format = pix_fmt;
            frame->width = width;
            frame->height = height;

            if (pix_fmt == AV_PIX_FMT_VIDEOTOOLBOX) {
                return frame;
            }

            int ret = av_frame_get_buffer(frame, 4);

            if (ret < 0) {
                av_frame_free(&frame);
                LOGE("Cannot allocate video frame data.\n");
                return NULL;
            }

            return frame;
        }

    }
}
