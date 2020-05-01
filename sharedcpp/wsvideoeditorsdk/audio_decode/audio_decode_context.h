#ifndef SHAREDCPP_WS_VIDEO_EDITOR_AUDIO_DECODE_CONTEXT_H
#define SHAREDCPP_WS_VIDEO_EDITOR_AUDIO_DECODE_CONTEXT_H

#include <string>
#include "av_utils.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
};

namespace whensunset {
    namespace wsvideoeditor {

        class AudioDecodeContext {
        public:
            AudioDecodeContext(std::string tag = "NoTag");

            virtual ~AudioDecodeContext();

            int OpenFile(const std::string &path, bool need_verify_seekable = false);

            bool GetAudio(int64_t pts_dst_stream_tb, uint8_t *dst_buff, int len);

            bool Seek(double pos);

            //some getter and setter functions
            void set_dst_channels(int dst_channels) {
                dst_channels_ = dst_channels;
            }

            int dst_channels() {
                return dst_channels_;
            }

            void set_dst_sample_rate(int sample_rate) {
                dst_sample_rate_ = sample_rate;
            }

            int dst_sample_rate() {
                return dst_sample_rate_;
            }

            void set_dst_sample_fmt(AVSampleFormat dst_sample_fmt) {
                dst_sample_fmt_ = dst_sample_fmt;
            }

            AVSampleFormat dst_sample_fmt() {
                return dst_sample_fmt_;
            }

            AVSampleFormat src_sample_fmt() {
                return src_sample_fmt_;
            }

            int src_sample_rate() {
                return src_sample_rate_;
            }

            int src_channels() {
                return src_channels_;
            }

            double current_buffer_sec() {
                return current_buffer_sec_;
            }

            double duration_sec() {
                return duration_sec_;
            }

            void set_clipped_range_start(double clipped_range_start) {
                clipped_range_start_ = clipped_range_start;
            }

        private:
            bool FileHasAudio();

            void Release();

            int InitSwrContext();

            int DecodeOneAudioFrame();

            int FrameDataValidation(AVFrame *audio_frame);

            std::string path_;
            double duration_sec_;
            std::unique_ptr<AVFormatContext, decltype(&ReleaseAVFormatContext)> format_ctx_;
            std::unique_ptr<AVCodecContext, decltype(&ReleaseAVCodecContext)> codec_ctx_;
            std::unique_ptr<SwrContext, decltype(&ReleaseSwrContext)> swr_ctx_;
            int audio_stream_index_;
            AVStream *audio_stream_;
            int64_t audio_stream_start_time_;
            AVCodec *codec_;

            int dst_channels_;
            int dst_sample_rate_;
            AVSampleFormat dst_sample_fmt_;

            int src_channels_;
            int convert_src_channels_;
            int src_sample_rate_;
            AVSampleFormat src_sample_fmt_;

            double current_pkt_sec_;
            double current_buffer_sec_;

            std::unique_ptr<uint8_t[]> decode_buff_;
            int buff_size_;
            int buff_index_;
            double clipped_range_start_;
            bool last_decode_ret_;

            std::string tag_;
        };
    }
}

#endif
