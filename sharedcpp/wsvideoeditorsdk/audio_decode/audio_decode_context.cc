#include "audio_decode_context.h"
#include "av_utils.h"
#include "constants.h"
#include "platform_logger.h"
#include <stdio.h>
#include <cmath>

#pragma clang diagnostic push
// Deprecated FFmpeg APIs must be used for maintaining backwards compatibility with FFmpeg 3.0
// Ignoring such warnings when compiling against newer FFmpeg versions
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace whensunset {
    namespace wsvideoeditor {

// Minimum audio buffer size, in byte size
        const int kMinAudioBufferSize = 2048;
        const int kMinProbeSize = 1024;
        const int kMaxProbeSize = 1024 * 1024;
        const double kCorrectionDuration = 0.05;
        const char kMP4Demuxer[] = "mov,mp4,m4a,3gp,3g2,mj2";

        AudioDecodeContext::AudioDecodeContext(std::string tag) : format_ctx_(nullptr,
                                                                              ReleaseAVFormatContext),
                                                                  codec_ctx_(nullptr,
                                                                             ReleaseAVCodecContext),
                                                                  swr_ctx_(nullptr,
                                                                           ReleaseSwrContext),
                                                                  tag_(tag) {
            audio_stream_index_ = -1;
            audio_stream_ = nullptr;
            codec_ = nullptr;
            duration_sec_ = 0;
            dst_channels_ = 2;
            dst_sample_rate_ = 44100 * 1;
            dst_sample_fmt_ = AV_SAMPLE_FMT_S16;
            buff_index_ = 0;
            buff_size_ = 0;
            current_pkt_sec_ = 0.0;
            current_buffer_sec_ = 0.0;
            last_decode_ret_ = false;
            clipped_range_start_ = 0.0;
        }

        AudioDecodeContext::~AudioDecodeContext() {
            Release();
        }

        void AudioDecodeContext::Release() {
            path_ = "";
            audio_stream_index_ = -1;

            buff_index_ = 0;
            buff_size_ = 0;
        }

        bool AudioDecodeContext::FileHasAudio() {
            return (audio_stream_index_ >= 0 && codec_ && codec_ctx_);
        }

        int AudioDecodeContext::OpenFile(const std::string &path, bool need_verify_seekable) {
            path_ = path;
            int ret = 0;
            AVFormatContext *format_ctx = nullptr;
            if ((ret = avformat_open_input(&format_ctx, path_.c_str(), nullptr, nullptr)) < 0) {
                LOGE("AudioDecodeContext::OpenFile Error opening audio file: %s. err %s\n",
                     path_.c_str(), av_err2str(ret));
                return ret;
            }
            format_ctx_.reset(format_ctx);
            bool is_demuxer_mp4 = false;

            if (format_ctx_->iformat != nullptr && format_ctx_->iformat->name != nullptr) {
                is_demuxer_mp4 = !strcmp(format_ctx_->iformat->name, kMP4Demuxer);
            }

            // try kMinProbeSize first
            format_ctx_->probesize = kMinProbeSize;
            if ((ret = avformat_find_stream_info(format_ctx_.get(), nullptr)) < 0) {
                LOGE("AudioDecodeContext::OpenFile Error find_stream_info: %s. err %s\n",
                     path_.c_str(), av_err2str(ret));
                return ret;
            }

            audio_stream_index_ = av_find_best_stream(format_ctx_.get(), AVMEDIA_TYPE_AUDIO, -1, -1,
                                                      nullptr, 0);
            if (audio_stream_index_ == AVERROR_STREAM_NOT_FOUND) {
                if (is_demuxer_mp4) {
                    // audio AVStream info in mp4 is inside header, do not need to do avformat_find_stream_info with probesize 1M
                    LOGE("AudioDecodeContext::OpenFile Error, no audio in mp4: %s\n",
                         path_.c_str());
                    return AVERROR_STREAM_NOT_FOUND;
                }
                LOGI("AudioDecodeContext::OpenFile Error with probesize %d, going to try probesize %d\n",
                     kMinProbeSize, kMaxProbeSize);
                // if kMinProbeSize is not enough to read audio stream info, use kMaxProbeSize
                AVFormatContext *format_ctx_new = nullptr;
                if ((ret = avformat_open_input(&format_ctx_new, path_.c_str(), nullptr, nullptr)) <
                    0) {
                    return ret;
                }
                format_ctx_new->probesize = kMaxProbeSize;
                if ((ret = avformat_find_stream_info(format_ctx_new, nullptr)) < 0) {
                    return ret;
                }
                format_ctx_.reset(format_ctx_new);

                audio_stream_index_ = av_find_best_stream(format_ctx_.get(), AVMEDIA_TYPE_AUDIO, -1,
                                                          -1, nullptr, 0);
                if (audio_stream_index_ == AVERROR_STREAM_NOT_FOUND) {
                    LOGE("AudioDecodeContext::OpenFile Error find audio stream: %s. err %s\n",
                         path_.c_str(), av_err2str(ret));
                    return AVERROR_STREAM_NOT_FOUND;
                }

            }

            audio_stream_ = format_ctx_.get()->streams[audio_stream_index_];
            codec_ = avcodec_find_decoder(
                    format_ctx_->streams[audio_stream_index_]->codec->codec_id);
            codec_ctx_.reset(avcodec_alloc_context3(NULL));
            if ((ret = avcodec_copy_context(codec_ctx_.get(),
                                            format_ctx_->streams[audio_stream_index_]->codec)) <
                0) {
                LOGE("AudioDecodeContext::OpenFile Error.  avcodec_copy_context, path: %s. err %s\n",
                     path_.c_str(), av_err2str(ret));
                return ret;
            }

            // Open Codec
            if (!codec_ || (ret = avcodec_open2(codec_ctx_.get(), codec_, nullptr) < 0)) {
                audio_stream_ = nullptr;
                audio_stream_index_ = -1;
                codec_ctx_.reset();
                LOGE("AudioDecodeContext::OpenFile, codec not found, or open failed, path: %s. err %s\n",
                     path_.c_str(), av_err2str(ret));
                return ret;
            } else {
                src_channels_ = codec_ctx_->channels;
                src_sample_rate_ = codec_ctx_->sample_rate;
                src_sample_fmt_ = codec_ctx_->sample_fmt;
            }

            if (audio_stream_->duration == AV_NOPTS_VALUE) {
                LOGE("audio decode audio_stream_ duration == AV_NOPTS_VALUE");
                // audio has no duration value, use video duration
                int video_stream_idx = av_find_best_stream(format_ctx_.get(), AVMEDIA_TYPE_VIDEO,
                                                           -1, -1, nullptr, 0);
                if (video_stream_idx >= 0) {
                    AVStream *video_stream = format_ctx_.get()->streams[video_stream_idx];
                    duration_sec_ = video_stream->duration * av_q2d(video_stream->time_base);
                }
            } else {
                duration_sec_ = audio_stream_->duration * av_q2d(audio_stream_->time_base);
            }

            if (need_verify_seekable) {
                // some file's audio do not allow seek operation, but it can play in order
                ret = av_seek_frame(format_ctx_.get(), audio_stream_index_, 0,
                                    AVSEEK_FLAG_BACKWARD);
                if (ret < 0) {
                    // seeking with AVSEEK_FLAG_BACKWARD failed, seek again without flag
                    ret = av_seek_frame(format_ctx_.get(), audio_stream_index_, 0, 0);
                    if (ret < 0) {
                        LOGE("AudioDecodeContext::OpenFile need verify seekable, failed. code: %d (%s)",
                             ret, av_err2str(ret));
                        return ret;
                    }
                }
            }
            avcodec_flush_buffers(codec_ctx_.get());

            if (audio_stream_ && duration_sec_ > 0) {
                if (audio_stream_->start_time != AV_NOPTS_VALUE) {
                    // audio start pos
                    audio_stream_start_time_ = static_cast<int64_t>(
                            audio_stream_->start_time * av_q2d(audio_stream_->time_base) *
                            (double) dst_sample_rate_ + 0.5);
                } else {
                    audio_stream_start_time_ = 0;
                }
            }
            return 0;
        }

        int AudioDecodeContext::InitSwrContext() {
            SwrContext *swr_ctx = swr_alloc_set_opts(nullptr,
                                                     av_get_default_channel_layout(dst_channels_),
                                                     dst_sample_fmt_, dst_sample_rate_,
                                                     av_get_default_channel_layout(
                                                             convert_src_channels_),
                                                     src_sample_fmt_, src_sample_rate_, 0, nullptr);
            int ret = AVERROR(ENOMEM);
            if (swr_ctx) {
                ret = swr_init(swr_ctx);
            }
            if (ret < 0) {
                LOGE("AudioDecodeContext swr_context init failed! dst ac: %d, af: %d, ar: %d, src ac: %d, af: %d, ar: %d",
                     dst_channels_, dst_sample_fmt_, dst_sample_rate_, convert_src_channels_,
                     src_sample_fmt_, src_sample_rate_);
                return ret;
            }
            swr_ctx_.reset(swr_ctx);
            return 0;
        }

// In case crashes in swr_convert()
// @return  0 means ok, other means fail
        int AudioDecodeContext::FrameDataValidation(AVFrame *audio_frame) {
            if (audio_frame == nullptr || audio_frame->extended_data == nullptr ||
                audio_frame->nb_samples == 0) {
                return 0;
            }
            if (av_sample_fmt_is_planar(src_sample_fmt_)) {
                for (int i = 0; i < src_channels_; i++) {
                    if (audio_frame->extended_data[i] == nullptr) {
                        return AVERROR_INVALIDDATA;
                    }
                }
            }
            return 0;
        }

        int AudioDecodeContext::DecodeOneAudioFrame() {
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = nullptr;
            packet.size = 0;

            double sec_per_sample = av_q2d(audio_stream_->time_base);
            while (1) {
                XASSERT(format_ctx_.get());
                int ret = av_read_frame(format_ctx_.get(), &packet);
                if (ret == AVERROR(EAGAIN)) {
                    continue;
                } else if (ret == AVERROR_EOF) {
                    // File read eof, do nothing
                    return ret;
                } else if (ret < 0) {
                    LOGW("read_frame_ret error, ret:%d, (%s)", ret, av_err2str(ret));
                    return ret;
                }
                if ((packet.flags & AV_PKT_FLAG_DISCARD) ||
                    packet.stream_index != audio_stream_index_) {
                    av_packet_unref(&packet);
                    continue;
                }

                UniqueAVFramePtr audio_frame = UniqueAVFramePtr(av_frame_alloc(), FreeAVFrame);
                int got_frame;
                ret = avcodec_decode_audio4(codec_ctx_.get(), audio_frame.get(), &got_frame,
                                            &packet);
                if (ret < 0) {
                    LOGE("AudioDecodeContext avcodec_decode_audio4 failed, ret: %d %s, path: %s",
                         ret, av_err2str(ret), path_.c_str());
                    av_packet_unref(&packet);
                    return ret;
                }

                int64_t dts = packet.dts;
                if (dts != AV_NOPTS_VALUE) {
                    // use dts with priority
                    if (dts < audio_stream_->start_time - TIME_EPS * dst_sample_rate_) {
                        LOGW("AudioDecodeContext decoded packet pts (%lld) is before stream start time (%lld)! Check your file!",
                             dts, audio_stream_->start_time);
                        av_packet_unref(&packet);
                        continue;
                    }

                    double pkt_sec = dts * sec_per_sample;
                    if (current_pkt_sec_ <= pkt_sec + TIME_EPS) {
                        current_pkt_sec_ = pkt_sec;
                    } else {
                        LOGD("decoded audio frame dts < current_pkt_sec_ (%f < %f), decode next audio frame!!!",
                             pkt_sec, current_pkt_sec_);
                        av_packet_unref(&packet);
                        continue;
                    }
                } else {
                    current_pkt_sec_ =
                            av_frame_get_best_effort_timestamp(audio_frame.get()) * sec_per_sample;
                }

                // The packet is lost when got_frame is 0, should use its duration in order to avoid pts goes wrong
                int nb_samples = got_frame ? audio_frame->nb_samples
                                           : static_cast<int>(packet.duration);
                int dst_sample_count = nb_samples;
                bool frame_valid = got_frame && (FrameDataValidation(audio_frame.get()) == 0);
                convert_src_channels_ = src_channels_;

                // mono to stereo should not use ffmpeg due to that ffmpeg will lower the volume in this case
                // see: https://sound.stackexchange.com/questions/42709/why-does-ffmpegs-conversion-from-mono-to-stereo-lower-the-volume
                // and https://trac.ffmpeg.org/wiki/AudioChannelManipulation
                uint8_t *frame_data_stereo[2];
                std::unique_ptr<uint8_t[]> frame_data_stereo_ptr(nullptr);
                if (dst_channels_ == 2 && src_channels_ == 1) {
                    convert_src_channels_ = dst_channels_;
                    if (frame_valid) {
                        int src_bytes_per_sample = av_get_bytes_per_sample(src_sample_fmt_);
                        frame_data_stereo_ptr.reset(
                                new(std::nothrow) uint8_t[src_bytes_per_sample * nb_samples * 2]);
                        if (!frame_data_stereo_ptr) {
                            LOGE("Out of native memory when trying to alloc planar frame_data_stereo %d bytes!",
                                 src_bytes_per_sample * nb_samples * 2);
                            av_packet_unref(&packet);
                            return AVERROR(ENOMEM);
                        }
                        if (av_sample_fmt_is_planar(src_sample_fmt_)) {
                            // planar
                            frame_data_stereo[0] = frame_data_stereo_ptr.get();
                            frame_data_stereo[1] =
                                    frame_data_stereo_ptr.get() + src_bytes_per_sample * nb_samples;
                            memcpy(frame_data_stereo[0], audio_frame->extended_data[0],
                                   src_bytes_per_sample * nb_samples);
                            memcpy(frame_data_stereo[1], audio_frame->extended_data[0],
                                   src_bytes_per_sample * nb_samples);
                        } else {
                            // packed
                            frame_data_stereo[0] = frame_data_stereo_ptr.get();
                            frame_data_stereo[1] = nullptr;
                            for (int i = 0; i < nb_samples; i++) {
                                memcpy(frame_data_stereo[0] + 2 * i * src_bytes_per_sample,
                                       audio_frame->extended_data[0] + i * src_bytes_per_sample,
                                       src_bytes_per_sample);
                                memcpy(frame_data_stereo[0] + (2 * i + 1) * src_bytes_per_sample,
                                       audio_frame->extended_data[0] + i * src_bytes_per_sample,
                                       src_bytes_per_sample);
                            }
                        }
                    }
                }

                bool need_resample = src_sample_fmt_ != dst_sample_fmt_ ||
                                     src_sample_rate_ != dst_sample_rate_ ||
                                     convert_src_channels_ != dst_channels_;
                if (need_resample) {
                    if (!swr_ctx_) {
                        if (InitSwrContext() < 0) {
                            LOGE("InitSwrContext() failed");
                            av_packet_unref(&packet);
                            return ret;
                        }
                    }
                    dst_sample_count = static_cast<int>(av_rescale_rnd(
                            swr_get_delay(swr_ctx_.get(), src_sample_rate_) +
                            nb_samples, dst_sample_rate_, src_sample_rate_, AV_ROUND_UP));
                }

                int decoded_nb_bytes =
                        dst_sample_count * dst_channels_ * av_get_bytes_per_sample(dst_sample_fmt_);
                decode_buff_.reset(new(std::nothrow) uint8_t[decoded_nb_bytes]);
                if (!decode_buff_) {
                    LOGE("Out of native memory when trying to alloc %d bytes!", decoded_nb_bytes);
                    av_packet_unref(&packet);
                    return AVERROR(ENOMEM);
                }
                // Because AV_SAMPLE_FMT_S16 is planar, so we just use 1D array
                // we could use av_sample_fmt_is_planar() to check whether a sample format is planar or packed
                assert(dst_sample_fmt_ == AV_SAMPLE_FMT_S16);
                uint8_t *sws_buff = decode_buff_.get();
                memset(sws_buff, 0, static_cast<size_t>(decoded_nb_bytes));
                uint8_t **frame_data = frame_data_stereo_ptr ? frame_data_stereo
                                                             : audio_frame->extended_data;
                if (!got_frame) {
                    LOGD("AudioDecodeContext avcodec_decode_audio4 got_frame is 0, return a zero-frame with %d bytes",
                         decoded_nb_bytes);
                } else if (need_resample) {
                    if (!frame_valid) {
                        LOGE("Invalid audio frame data!");
                        av_packet_unref(&packet);
                        return AVERROR_INVALIDDATA;
                    }

                    int out_samples = swr_convert(swr_ctx_.get(), &sws_buff, decoded_nb_bytes,
                                                  (const uint8_t **) frame_data, nb_samples);
                    if (out_samples <= 0) {
                        LOGW("swr_convert has no samples, count: %d", out_samples);
                    }
                    decoded_nb_bytes =
                            out_samples * dst_channels_ * av_get_bytes_per_sample(dst_sample_fmt_);
                } else {
                    memcpy(sws_buff, frame_data[0], decoded_nb_bytes);
                }
                av_packet_unref(&packet);
                return decoded_nb_bytes;
            }
        }

        bool AudioDecodeContext::Seek(double pos) {
            pos = fmax(0.0, pos);
            if (pos > duration_sec_ - TIME_EPS) {
                LOGW("AudioDecodeContext seek position(%f) exceeds duration(%f), will limit to duration",
                     pos, duration_sec_);
                pos = duration_sec_ - TIME_EPS;
            }

            // Do not need add audio_stream_->start_time!
            int64_t timestamp = static_cast<int64_t >(pos / av_q2d(audio_stream_->time_base) + 0.5);

            LOGI("AudioDecode::GetAudio Seek to timestamp: %lld, sec: %f, current: %f\n", timestamp,
                 pos, current_pkt_sec_);
            int seek_ret = av_seek_frame(format_ctx_.get(), audio_stream_index_, timestamp,
                                         AVSEEK_FLAG_BACKWARD);
            if (seek_ret < 0) {
                LOGE("audio seek failed with error %d (%s)\n", seek_ret, av_err2str(seek_ret));
                return false;
            }
            avcodec_flush_buffers(codec_ctx_.get());
            buff_index_ = buff_size_;
            current_pkt_sec_ = pos;
            current_buffer_sec_ = pos;
            last_decode_ret_ = false;
            return true;
        }

        bool AudioDecodeContext::GetAudio(int64_t pts_dst_stream_tb, uint8_t *dst_buff, int len) {
            if (!FileHasAudio()) {
                return false;
            }
            assert(pts_dst_stream_tb >= 0);
            if (len < 0 || pts_dst_stream_tb >= (duration_sec_ * dst_sample_rate_ + 0.5)) {
                return false;
            }
            if (pts_dst_stream_tb < audio_stream_start_time_) {
                LOGI("tag: %s, AudioDecodeContext::GetAudio sample pos is before audio start time, pts_dst_stream_tb: %lld - %f, audio start time: %lld - %f",
                     tag_.c_str(), pts_dst_stream_tb,
                     pts_dst_stream_tb / (double) dst_sample_rate(),
                     audio_stream_start_time_,
                     audio_stream_start_time_ / (double) dst_sample_rate());
                return false;
            }

            int audio_size = 0;
            while (len > 0) {
                if (buff_index_ >= buff_size_) {
                    // decode one frame
                    audio_size = DecodeOneAudioFrame();
                    if (audio_size <= 0) {
                        // if error occurred, just output silence
                        decode_buff_.reset(new(std::nothrow) uint8_t[kMinAudioBufferSize]);
                        if (!decode_buff_) {
                            LOGE("Out of native memory when try to alloc %d bytes!",
                                 kMinAudioBufferSize);
                            return false;
                        }
                        memset(decode_buff_.get(), 0, kMinAudioBufferSize);
                        buff_size_ = kMinAudioBufferSize;
                        last_decode_ret_ = false;
                    } else {
                        buff_size_ = audio_size;
                        last_decode_ret_ = true;
                    }
                    buff_index_ = 0;
                }
                // buffer_size_ means the buffer size, buffer_index_ means buffer read position
                // current_buffer_sec_ is the reading buffer position int the file
                int bytes_per_sample = dst_channels_ * av_get_bytes_per_sample(dst_sample_fmt_);
                if (last_decode_ret_ &&
                    current_pkt_sec_ < clipped_range_start_ + kCorrectionDuration
                    && pts_dst_stream_tb / (double) dst_sample_rate_ - current_buffer_sec_ >
                       TIME_EPS) {
                    // check whether buffer is behind pts_dst_stream_tb
                    // If buffer is behind pts_dst_stream_tb (within kCorrectionDuration range), we drop the buffers
                    int need_drop_nb_bytes =
                            (pts_dst_stream_tb - current_buffer_sec_ * dst_sample_rate_) *
                            bytes_per_sample;
                    // need_drop_nb_bytes should be size of samples, which is aligned to bytes_per_sample
                    need_drop_nb_bytes += (bytes_per_sample -
                                           need_drop_nb_bytes % bytes_per_sample);
                    if (buff_size_ - buff_index_ <= need_drop_nb_bytes) {
                        buff_index_ = buff_size_;
                        continue; // DecodeOneAudioFrame()
                    } else {
                        buff_index_ += need_drop_nb_bytes;
                    }
                }

                int bytes_available = std::min(len, buff_size_ - buff_index_);
                memcpy(dst_buff, decode_buff_.get() + buff_index_,
                       static_cast<size_t>(bytes_available));
                len -= bytes_available;
                dst_buff += bytes_available;
                buff_index_ += bytes_available;
                if (last_decode_ret_) {
                    current_buffer_sec_ = current_pkt_sec_ +
                                          (buff_index_ * 1.0 / bytes_per_sample) / dst_sample_rate_;
                }
            }
            return last_decode_ret_;
        }
    }
}

#pragma clang diagnostic pop
