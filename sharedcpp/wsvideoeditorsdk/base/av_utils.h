#ifndef SHAREDCPP_WS_VIDEO_EDITOR_AV_UTILS_H
#define SHAREDCPP_WS_VIDEO_EDITOR_AV_UTILS_H

#include <string>

extern "C" {
#include <libavformat/avformat.h>
};

namespace whensunset {
    namespace wsvideoeditor {

        typedef std::unique_ptr<AVFrame, void (*)(AVFrame *)> UniqueAVFramePtr;
        typedef std::unique_ptr<AVPacket, void (*)(AVPacket *)> UniqueAVPacketPtr;

        UniqueAVFramePtr UniqueAVFramePtrCreateNull();

        void SetCurrentThreadName(const std::string &name);

        std::string BoTSt(bool is);

        class DecodedFramesUnit {
        public:
            UniqueAVFramePtr frame = UniqueAVFramePtrCreateNull();

            double frame_timestamp_sec = 0.0;

            std::string frame_file = "";

            int frame_media_asset_index = -1;

            DecodedFramesUnit() {}

            DecodedFramesUnit(const DecodedFramesUnit &) = delete;

            DecodedFramesUnit &operator=(const DecodedFramesUnit &) = delete;

            DecodedFramesUnit(DecodedFramesUnit &&other) { MoveInternal(std::move(other)); }

            void operator=(DecodedFramesUnit &&other) { MoveInternal(std::move(other)); }

            operator bool() {
                return !!frame;
            }

            std::string ToString() {
                return ("frame_timestamp_sec:" + std::to_string(frame_timestamp_sec) +
                        ",frame_file:" +
                        frame_file + ",frame_media_asset_index:" +
                        std::to_string(frame_media_asset_index));
            }

        private:
            void MoveInternal(DecodedFramesUnit &&other) {
                frame = std::move(other.frame);
                frame_timestamp_sec = other.frame_timestamp_sec;
                frame_file = std::move(other.frame_file);
                frame_media_asset_index = other.frame_media_asset_index;
            }
        };

        DecodedFramesUnit DecodedFramesUnitCreateNull();

        void FreeAVFrame(AVFrame *frame);

        void FreeAVPacket(AVPacket *frame);

        AVFrame *AllocVideoFrame(AVPixelFormat pix_fmt, int width, int height);

        inline int64_t NoPtsToZero(int64_t pts) { return pts == AV_NOPTS_VALUE ? 0 : pts; };

    }
}
#endif
