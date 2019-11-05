#ifndef SHAREDCPP_WS_VIDEO_EDITOR_VIDEO_DECODE_CONTEXT_H
#define SHAREDCPP_WS_VIDEO_EDITOR_VIDEO_DECODE_CONTEXT_H

#include <string>
#include <vector>
#include "av_utils.h"
#include "jni_helper.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
};

namespace whensunset {
    namespace wsvideoeditor {

        class VideoDecodeContext {
        public:
            AVCodecContext *codec_context_ = NULL;

            AVFormatContext *format_context_ = NULL;

            AVStream *video_stream_ = NULL;

            int video_stream_idx_ = -1;

            bool has_gop_structure_ = false;

            int64_t current_pts_ = -1;

            std::vector<int64_t> keyframe_dts_;

            std::vector<int> gop_frame_count_;

            std::string path_ = "";

            std::string origin_path_ = "";

            // 是否当前的 TrackAsset 已经到了最后一帧
            bool is_drain_loop_ = false;

            VideoDecodeContext() {}

            int OpenFile(const std::string &file_path);

            void Release();

            virtual ~VideoDecodeContext() { Release(); }

        private:
            bool is_opened_ = false;

            int ReadGopStructure();
        };
    }
}
#endif
