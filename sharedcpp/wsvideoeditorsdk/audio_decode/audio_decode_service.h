#ifndef SHAREDCPP_WS_VIDEO_EDITOR_AUDIO_DECODE_SERVICE_H
#define SHAREDCPP_WS_VIDEO_EDITOR_AUDIO_DECODE_SERVICE_H

#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <cmath>
#include "decode_service_common.h"
#include "platform_logger.h"
#include "constants.h"
#include "prebuilt_protobuf/ws_video_editor_sdk.pb.h"
#include "ref_clock.h"
#include "audio_sample_ring_buffer.h"
#include "audio_decode_context.h"

namespace whensunset {
    namespace wsvideoeditor {

        struct AssetAudioDecoder {
            uint64_t asset_id_;
            std::string asset_path_;

            double volume_;
            bool is_repeat_;
            int current_loop_count_;
            model::TimeRange display_range_;
            model::TimeRange clipped_range_;
            std::unique_ptr<AudioDecodeContext> audio_decode_ctx_;
        };

        class AudioDecodeService {

        public:
            AudioDecodeService(int buffer_size = 6);

            virtual ~AudioDecodeService();

            void Start();

            void Stop();

            void ResetDecodePosition(double render_pos);

            void SetProject(model::EditorProject project, double pos_sec = -1.0);

            int GetAudio(uint8_t *buff, int size, double *render_pos);

            int GetBufferedDataSize() {
                std::lock_guard<std::mutex> lk(mutex_);
                return decoded_audio_buffer_.size();
            }

            bool is_stopped() {
                std::lock_guard<std::mutex> lock(mutex_);
                return is_stopped_;
            }

        private:
            std::mutex mutex_;

            std::mutex start_stop_mutex_;

            std::condition_variable cv_;

            bool is_stopped_ = true;

            bool is_released_ = false;

            bool asset_audio_updated_ = false;

            bool asset_volume_updated_ = false;

            model::EditorProject project_;

            std::unique_ptr<DecodePositionChangeRequest> position_change_request_;

            AVSampleFormat dst_sample_fmt_ = AV_SAMPLE_FMT_S16;

            int dst_channels_ = 2;

            int dst_sample_rate_ = 44100;

            std::unique_ptr<RefClock> internal_clock_;

            std::vector<std::unique_ptr<AssetAudioDecoder>> audio_decoders_;

            whensunset::base::AudioSampleRingBuffer<uint8_t> decoded_audio_buffer_;

            double buffer_track_pos_ = 0.0;

            void DecodeWorker();

            std::thread decode_thread_;

            bool UpdateAudioDecoders(const model::EditorProject &project);

            void UpdateAudioDecodersVolume(const model::EditorProject &project);

            void ClearInvalidDecoders(const model::EditorProject &project);

            int
            FindInsertDecoderPosition(const model::EditorProject &project, int track_index);

            void BufferOneAudioSample(const model::EditorProject &project);

            void
            GetAudioBufferInternal(const model::EditorProject &project,
                                   uint8_t *buff, int len);

            void SeekAudioDecoder(double track_pos);;

            template<typename T>
            AssetAudioDecoder *
            AddAudioDecoder(std::string path, T *asset, double src_file_duration, int insert_pos) {
                AssetAudioDecoder *asset_audio_decoder = nullptr;
                if (asset->asset_path() == "") {
                    LOGE("tag: %s, AddAudioDecoder track asset path is empty, ignore it! id: %llu",
                         "", asset->asset_id());
                    return asset_audio_decoder;
                }
                for (const auto &audio_decoder : audio_decoders_) {
                    if (audio_decoder->asset_id_ == asset->asset_id()) {
                        asset_audio_decoder = audio_decoder.get();
                        break;
                    }
                }

                if (!asset_audio_decoder) {
                    std::unique_ptr<AssetAudioDecoder> audio_decoder{
                            new(std::nothrow) AssetAudioDecoder()};
                    if (!audio_decoder) {
                        LOGE("OOM in AddAudioDecoder!!!");
                        return nullptr;
                    }
                    asset_audio_decoder = audio_decoder.get();
                    asset_audio_decoder->current_loop_count_ = 0;
                    asset_audio_decoder->audio_decode_ctx_.reset(
                            new(std::nothrow) AudioDecodeContext(""));
                    if (!asset_audio_decoder->audio_decode_ctx_) {
                        LOGE("OOM in AddAudioDecoder!!!");
                        return nullptr;
                    }
                    int ret = 0;
                    if ((ret = asset_audio_decoder->audio_decode_ctx_->OpenFile(path)) < 0
                        || asset_audio_decoder->audio_decode_ctx_->duration_sec() < TIME_EPS) {
                        LOGE("tag: %s, AddAudioDecoder open track asset failed or duration is 0! path: %s, error code: %d",
                             path.c_str(), "", ret);
                        return nullptr;
                    }

                    asset_audio_decoder->audio_decode_ctx_->set_dst_channels(dst_channels_);
                    asset_audio_decoder->audio_decode_ctx_->set_dst_sample_fmt(dst_sample_fmt_);
                    asset_audio_decoder->audio_decode_ctx_->set_dst_sample_rate(dst_sample_rate_);
                    if (insert_pos >= audio_decoders_.size()) {
                        audio_decoders_.push_back(std::move(audio_decoder));
                    } else {
                        audio_decoders_.insert(audio_decoders_.begin() + insert_pos,
                                               std::move(audio_decoder));
                    }
                }

                if (asset_audio_decoder) {
                    asset_audio_decoder->asset_id_ = asset->asset_id();
                    asset_audio_decoder->asset_path_ = path;
                    if (src_file_duration < TIME_EPS) {
                        // track asset (not the first) which has clipped range, do not has probed file
                        src_file_duration = asset_audio_decoder->audio_decode_ctx_->duration_sec();
                    }

                    if (src_file_duration <= TIME_EPS) {
                        LOGE("AddAudioDecoder, asset probed file duration is 0!!! Please check file!!! path: %s",
                             path.c_str());
                    }
                    asset_audio_decoder->clipped_range_.set_start(0.0);
                    asset_audio_decoder->clipped_range_.set_duration(src_file_duration);
                    asset_audio_decoder->audio_decode_ctx_->set_clipped_range_start(
                            asset_audio_decoder->clipped_range_.start());
                }
                return asset_audio_decoder;
            }

        };

    }
}
#endif
