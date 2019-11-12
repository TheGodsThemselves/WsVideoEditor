#include "video_decode_context.h"
#include "platform_logger.h"
#include "ws_editor_video_sdk_utils.h"

#pragma clang diagnostic push
// Deprecated FFmpeg APIs must be used for maintaining backwards compatibility with FFmpeg 3.0
// Ignoring such warnings when compiling against newer FFmpeg versions
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace whensunset {
    namespace wsvideoeditor {

        int VideoDecodeContext::OpenFile(const std::string &file_path) {
            is_drain_loop_ = false;
            if (is_opened_ && file_path == path_) {
                LOGI("VideoDecodeContext::OpenFile is_opened_:%s, file_path:%s, path_:%s",
                     BoTSt(is_opened_).c_str(), file_path.c_str(), path_.c_str());
                return 0;
            }

            Release();

            path_ = file_path;
            std::string ext = ExtName(file_path);
            has_gop_structure_ = (ext != "jpg" && ext != "png");
            AVDictionary *opts = nullptr;
            int ret = 0;
            const char *cpath = file_path.c_str();
            LOGI("VideoDecodeContext::OpenFile has_gop_structure_:%s",
                 BoTSt(has_gop_structure_).c_str());
            if ((ret = avformat_open_input(&format_context_, cpath, NULL, &opts)) < 0) {
                av_dict_free(&opts);
                LOGE("VideoDecodeContext::OpenFile open input error ret:%s", av_err2str(ret));
                return ret;
            };
            av_dict_free(&opts);
            if ((ret = avformat_find_stream_info(format_context_, NULL)) < 0) {
                LOGE("VideoDecodeContext::OpenFile error find stream info ret:%s", av_err2str(ret));
                return ret;
            }
            AVCodec *codec = nullptr;
            video_stream_idx_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1,
                                                    &codec, 0);
            LOGI("VideoDecodeContext::OpenFile video_stream_idx_:%d", video_stream_idx_);
            if ((ret = video_stream_idx_) < 0) {
                LOGE("VideoDecodeContext::OpenFile error find base video stream ret:%s",
                     av_err2str(ret));
                return ret;
            }
            video_stream_ = format_context_->streams[video_stream_idx_];
            if (codec == NULL) {
                LOGE("VideoDecodeContext::OpenFile codec is null");
                return -1;
            }
            codec_context_ = avcodec_alloc_context3(codec);
            if (!codec_context_) {
                LOGE("VideoDecodeContext::OpenFile OOM 1");
                return AVERROR(ENOMEM);
            }

            if ((ret = avcodec_copy_context(codec_context_, video_stream_->codec)) < 0) {
                LOGE("VideoDecodeContext::OpenFile error copying codec context, ret:%s",
                     av_err2str(ret));
                return ret;
            }
            codec_context_->refcounted_frames = 1;
            if ((ret = avcodec_open2(codec_context_, codec, NULL)) < 0) {
                LOGE("VideoDecodeContext::OpenFile error opening codec ret:%s", av_err2str(ret));
                return ret;
            }
            is_opened_ = true;
            path_ = file_path;
            ReadGopStructure();
            return 0;
        }

        int VideoDecodeContext::ReadGopStructure() {
            if (!video_stream_ || video_stream_->duration <= 0) {
                LOGI("VideoDecodeContext::ReadGopStructure Stream is null or doesn't have index entries. video_stream_:%s, duration:%lld",
                     BoTSt(video_stream_ != nullptr).c_str(), video_stream_->duration);
                return -1;
            }

            LOGI("VideoDecodeContext::ReadGopStructure video_stream_->index_entries: %s, nb_index_entries: %d, duration: %lld",
                 BoTSt(!video_stream_->index_entries).c_str(), video_stream_->nb_index_entries,
                 video_stream_->duration);
            keyframe_dts_.clear();
            gop_frame_count_.clear();
            int frame_count = video_stream_->nb_index_entries;
            if (has_gop_structure_) {
                for (int i = 0; i < video_stream_->nb_index_entries; ++i) {
                    int flags = video_stream_->index_entries[i].flags;
                    if (flags & AVINDEX_KEYFRAME) {
                        keyframe_dts_.push_back(video_stream_->index_entries[i].timestamp);
                        gop_frame_count_.push_back(1);
                    } else if (flags & AVINDEX_DISCARD_FRAME) {
                        // do nothing
                    } else {
                        if (gop_frame_count_.size()) {
                            ++gop_frame_count_.back();
                        }
                    }
                }
            } else {
                LOGE("VideoDecodeContext::ReadGopStructure not support");
            }
            LOGI("VideoDecodeContext::ReadGopStructure has_gop_structure_:%s, frame_count:%d, gop_frame_count_.size:%d",
                 BoTSt(has_gop_structure_).c_str(), frame_count, gop_frame_count_.size());
            return 0;
        }

        void VideoDecodeContext::Release() {
            video_stream_idx_ = -1;
            video_stream_ = NULL;
            if (codec_context_ != NULL) {
                // NOTE: must avcodec_close first then release hwaccel_context, otherwise something will leak.
                avcodec_close(codec_context_);
                if (codec_context_->hwaccel_context) {
                    codec_context_->hwaccel = nullptr;
                    codec_context_->hwaccel_context = nullptr;
                }
                codec_context_ = NULL;
            }
            if (format_context_ != NULL) {
                avformat_close_input(&format_context_);
                format_context_ = NULL;
            }
            keyframe_dts_.clear();
            gop_frame_count_.clear();
            has_gop_structure_ = false;
            is_opened_ = false;
            current_pts_ = -1;
            LOGI("VideoDecodeContext::Release");
        }
    }
}
#pragma clang diagnostic pop
