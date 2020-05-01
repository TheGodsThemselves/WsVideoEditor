#include "audio_decode_service.h"
#include "platform_logger.h"
#include "av_utils.h"
#include "preview_timeline.h"
#include <string>
#include <algorithm>

namespace whensunset {
    namespace wsvideoeditor {

        int
        AudioMixerSimpleProcess(uint8_t *data_out, uint8_t *data1, uint8_t *data2, int sample_count,
                                int dst_channels) {
            int sample_bytes_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * dst_channels;
            short *dst = (short *) data_out;
            short *src1 = (short *) data1;
            short *src2 = (short *) data2;
            for (int i = 0; i < sample_count * sample_bytes_size / 2; i++) {
                dst[i] = std::max(SHRT_MIN,
                                  std::min(SHRT_MAX, static_cast<int>(src1[i] * 1 + src2[i] * 1)));
            }
            return sample_count;
        }

        AudioDecodeService::AudioDecodeService(int buffer_size) :
                decoded_audio_buffer_(AUDIO_BUFFER_SIZE * buffer_size) {
            internal_clock_.reset(new(std::nothrow) RefClock);
            if (!internal_clock_) {
                abort();
            }
        }

        AudioDecodeService::~AudioDecodeService() {
            {
                std::lock_guard<std::mutex> lk(mutex_);
                is_released_ = true;
            }
            Stop();
        }

        void
        AudioDecodeService::SetProject(model::EditorProject project, double pos_sec) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (is_stopped_ || IsAudioAssetsChanged(project_, project)) {
                    asset_audio_updated_ = true;
                }
                if (asset_audio_updated_ || pos_sec > -PTS_EPS) {
                    decoded_audio_buffer_.Clear();
                } else if (IsAudioVolumeChanged(project_, project)) {
                    asset_volume_updated_ = true;
                }
                project_ = project;

                if (pos_sec > -PTS_EPS) {
                    pos_sec = std::max(0.0, pos_sec);
                    position_change_request_.reset(
                            new(std::nothrow) DecodePositionChangeRequest(pos_sec));
                    if (!position_change_request_) {
                        return;
                    }
                    internal_clock_->SetPts(pos_sec);
                }
            }
            cv_.notify_all();
        }

        void AudioDecodeService::ResetDecodePosition(double render_pos) {
            std::lock_guard<std::mutex> lock(mutex_);
            decoded_audio_buffer_.Clear();
            position_change_request_.reset(
                    new(std::nothrow) DecodePositionChangeRequest(render_pos));
            if (!position_change_request_) {
                return;
            }
            cv_.notify_all();
        }

        void AudioDecodeService::Start() {
            std::lock_guard<std::mutex> start_stop_lk(start_stop_mutex_);
            {
                std::lock_guard<std::mutex> lk(mutex_);
                if (!is_stopped_ || is_released_) {
                    return;
                }
                if (project_.media_asset_size() == 0) {
                    return;
                }

                is_stopped_ = false;
                decoded_audio_buffer_.ReOpen();
                decoded_audio_buffer_.Clear();
                internal_clock_->SetPts(0.0);
            }
            decode_thread_ = std::thread(&AudioDecodeService::DecodeWorker, this);
        }

        void AudioDecodeService::Stop() {
            std::lock_guard<std::mutex> start_stop_lk(start_stop_mutex_);
            {
                std::lock_guard<std::mutex> lk(mutex_);
                is_stopped_ = true;
                decoded_audio_buffer_.Clear();
                decoded_audio_buffer_.Release();
            }
            cv_.notify_all();
            if (decode_thread_.joinable()) {
                decode_thread_.join();
            }
            audio_decoders_.clear();
        }

        int AudioDecodeService::GetAudio(uint8_t *buff, int size, double *render_pos) {
            std::unique_lock<std::mutex> lk(mutex_);
            memset(buff, 0, static_cast<size_t >(size));

            int got_length = decoded_audio_buffer_.Get(buff, size, render_pos, false);

            if (got_length > 0) {
                internal_clock_->SetPts(*render_pos);
            }
            int mix_len = size / 2;
            short *dst = (short *) buff;
            short *src = (short *) buff;

            for (int t = 0; t < mix_len; t++) {
                int mix_value = src[t] + dst[t];
                mix_value = std::max(SHRT_MIN, std::min(SHRT_MAX, mix_value));
                dst[t] = static_cast<short >(mix_value);
            }
            return got_length;
        }

        void AudioDecodeService::DecodeWorker() {
            SetCurrentThreadName("EditorTrackAudioDecode");
            do {
                bool asset_audio_updated = false;
                bool asset_volume_updated = false;
                model::EditorProject project;
                std::unique_ptr<DecodePositionChangeRequest> position_change_request(nullptr);
                {
                    std::unique_lock<std::mutex> lk(mutex_);
                    cv_.wait(lk,
                             [this] { return project_.media_asset_size() > 0 || is_stopped_; });
                    if (is_stopped_) {
                        break;
                    }
                    project = project_;
                    if (position_change_request_) {
                        position_change_request = std::move(position_change_request_);
                    } else if (asset_audio_updated_) {
                        asset_audio_updated = asset_audio_updated_;
                        asset_audio_updated_ = asset_volume_updated_ = false;
                        decoded_audio_buffer_.Clear();
                        // reset buffer read position
                        buffer_track_pos_ = internal_clock_->GetRenderPos();
                        position_change_request.reset(new(std::nothrow) DecodePositionChangeRequest(
                                buffer_track_pos_));

                        if (!position_change_request) {
                            return;
                        }
                    } else if (asset_volume_updated_) {
                        asset_volume_updated = true;
                        asset_volume_updated_ = false;
                    }
                }
                if (asset_audio_updated) {
                    UpdateAudioDecoders(project);
                } else if (asset_volume_updated) {
                    UpdateAudioDecodersVolume(project);
                }
                if (position_change_request) {
                    buffer_track_pos_ = position_change_request->render_pos;
                    SeekAudioDecoder(buffer_track_pos_);
                    internal_clock_->SetPts(buffer_track_pos_);
                    decoded_audio_buffer_.Clear();
                    position_change_request.reset();
                }
                BufferOneAudioSample(project);
            } while (true);
        }

        void
        AudioDecodeService::BufferOneAudioSample(const model::EditorProject &project) {
            int sample_bytes_size = av_get_bytes_per_sample(dst_sample_fmt_) * dst_channels_;
            int len = dst_sample_rate_ / 100 * sample_bytes_size;
            std::unique_ptr<uint8_t[]> buff(new(std::nothrow) uint8_t[len]);
            if (!buff) {
                return;
            }
            memset(buff.get(), 0, static_cast<size_t >(len));

            double buffer_track_pos = buffer_track_pos_;

            GetAudioBufferInternal(project, buff.get(), len);

            bool has_position_change_request = false;
            {
                std::unique_lock<std::mutex> lk(mutex_);
                if (position_change_request_) {
                    has_position_change_request = true;
                }
            }

            if (!has_position_change_request) {
                buffer_track_pos = fmax(buffer_track_pos, internal_clock_->GetRenderPos());
                decoded_audio_buffer_.Put(buff.get(), len, buffer_track_pos);
            }
        }

        void AudioDecodeService::SeekAudioDecoder(double track_pos) {
            if (audio_decoders_.size() == 0) {
                return;
            }
            for (const auto &audio_decoder_ptr : audio_decoders_) {
                AssetAudioDecoder *const audio_decoder = audio_decoder_ptr.get();
                double seek_sec = track_pos;
                double offset = audio_decoder->display_range_.start();
                // get offset from the start of display range
                seek_sec = fmax(0.0, seek_sec - offset);
                // make sure the offset is inside asset duration in repeat mode
                if (audio_decoder->is_repeat_) {
                    double duration = audio_decoder->clipped_range_.duration();
                    while (seek_sec > duration) {
                        seek_sec -= duration;
                    }
                }
                // get position in audio asset
                seek_sec = seek_sec + audio_decoder->clipped_range_.start();
                audio_decoder->audio_decode_ctx_->Seek(seek_sec);
            }
        }

        void
        AudioDecodeService::GetAudioBufferInternal(const model::EditorProject &project,
                                                        uint8_t *buff, int len) {
            int sample_bytes_size = av_get_bytes_per_sample(dst_sample_fmt_) * dst_channels_;
            int need_nb_samples = len / sample_bytes_size;

            memset(buff, 0, static_cast<size_t >(len));

            bool first_audio_track_found = false;
            bool get_audio_ret = true;
            double buffer_track_pos = buffer_track_pos_;
            double buffer_track_pos_after_got = buffer_track_pos_;
            double cur_get_sec = 0;
            double start_offset = 0.0;

            auto decode_audio_frame = [&](AssetAudioDecoder *const audio_decoder) {
                AudioDecodeContext *audio_decode_ctx = audio_decoder->audio_decode_ctx_.get();
                // real get position of audio decoder should add output_delay
                std::unique_ptr<uint8_t[]> sub_buff{new(std::nothrow) uint8_t[len]};
                if (!sub_buff) {
                    return;
                }
                memset(sub_buff.get(), 0, static_cast<size_t >(len));
                get_audio_ret = true;

                start_offset = audio_decoder->display_range_.start();

                while (1) {
                    // Get samples from audio decoder
                    // Calculate the true position in the original audio asset
                    std::unique_ptr<uint8_t[]> original_audio_buff(new(std::nothrow) uint8_t[len]);
                    if (!original_audio_buff) {
                        break;
                    }
                    memset(original_audio_buff.get(), 0, static_cast<size_t >(len));
                    cur_get_sec -= start_offset;
                    cur_get_sec = fmax(0.0, cur_get_sec);
                    int64_t cur_get_pos = static_cast<long>(cur_get_sec *
                                                            audio_decode_ctx->dst_sample_rate());
                    int64_t clipped_start_pos = static_cast<int >(round(
                            audio_decoder->clipped_range_.start() *
                            audio_decode_ctx->dst_sample_rate()));
                    int64_t clipped_end_pos = static_cast<int >(round(
                            (audio_decoder->clipped_range_.start() +
                             audio_decoder->clipped_range_.duration())
                            * audio_decode_ctx->dst_sample_rate()));
                    int64_t clipped_len = clipped_end_pos - clipped_start_pos;
                    // if repeated, cur_get_pos need % clipped_len
                    if (audio_decoder->is_repeat_) {
                        double duration = audio_decoder->clipped_range_.duration();
                        int loop_count = 0;
                        while (cur_get_sec >= duration - TIME_EPS) {
                            cur_get_sec -= duration;
                            loop_count++;
                        }
                        // seek if loop count changed
                        if (audio_decoder->current_loop_count_ != loop_count) {
                            // position in audio asset
                            cur_get_sec = cur_get_sec + audio_decoder->clipped_range_.start();
                            audio_decode_ctx->Seek(cur_get_sec);
                            audio_decoder->current_loop_count_ = loop_count;
                        }
                        cur_get_pos %= clipped_len;
                    }
                    get_audio_ret = audio_decode_ctx->GetAudio(cur_get_pos + clipped_start_pos,
                                                               original_audio_buff.get(), len);

                    if (!get_audio_ret) {
                        break;
                    }
                    // not audio processor, just break
                    memcpy(sub_buff.get(), original_audio_buff.get(), len);
                    break;
                }

                if (!get_audio_ret) {
                    return;
                }

                if (get_audio_ret && !first_audio_track_found) {
                    // only origin track assets can only affect clock
                    first_audio_track_found = true;
                    buffer_track_pos_after_got = audio_decode_ctx->current_buffer_sec() +
                                                 audio_decoder->display_range_.start()
                                                 - audio_decoder->clipped_range_.start();
                }

                AudioMixerSimpleProcess(buff, buff, sub_buff.get(), need_nb_samples, dst_channels_);
            };

            for (int i = 0; i < audio_decoders_.size(); i++) {
                // 4 cases we need to ignore the audio asset:
                // 1st: not wanted type
                // 2nd: volume == 0.0
                AssetAudioDecoder *const audio_decoder = audio_decoders_[i].get();

                // 3rd: not in display range. (Notice: display range should use render_pos_)
                if (buffer_track_pos >= audio_decoder->display_range_.start() +
                                        audio_decoder->display_range_.duration() - PTS_EPS
                    || buffer_track_pos < audio_decoder->display_range_.start() - PTS_EPS) {
                    continue;
                }
                // 4th: is in display range, but is not repeated and out of audio file range.
                cur_get_sec = buffer_track_pos;
                start_offset = audio_decoder->display_range_.start();
                // use private clock in audio decode context
                AudioDecodeContext *audio_decode_ctx = audio_decoder->audio_decode_ctx_.get();
                if (!audio_decoder->is_repeat_
                    && cur_get_sec >= start_offset + audio_decoder->clipped_range_.duration()) {
                    continue;
                }
                // 5th: clipped range has no duration, will be seen as illegal asset
                if (audio_decoder->clipped_range_.duration() < TIME_EPS) {
                    continue;
                }

                decode_audio_frame(audio_decoder);

                if (get_audio_ret && !first_audio_track_found) {
                    // only origin track assets can only affect clock
                    double buffer_track_pos_tmp;
                    first_audio_track_found = true;
                    buffer_track_pos_tmp = audio_decode_ctx->current_buffer_sec() +
                                           audio_decoder->display_range_.start()
                                           - audio_decoder->clipped_range_.start();
                    // QuickFix: for 500w
                    buffer_track_pos_ = fmax(buffer_track_pos_tmp, buffer_track_pos_);
                }
            }

            if (!first_audio_track_found) {
                // not got track audio, also need to forward buffer_track_pos_
                buffer_track_pos_after_got += ((double) need_nb_samples) / dst_sample_rate_;
            }
            buffer_track_pos_ = std::max(buffer_track_pos_, fmax(0.0, buffer_track_pos_after_got));
        }

        void AudioDecodeService::UpdateAudioDecodersVolume(
                const model::EditorProject &project) {
            for (int i = 0; i < project.media_asset_size(); ++i) {
                auto asset = project.media_asset(i);
                auto decoder = std::find_if(audio_decoders_.begin(), audio_decoders_.end(),
                                            [&asset](
                                                    const std::unique_ptr<AssetAudioDecoder> &decoder) {
                                                return decoder->asset_id_ == asset.asset_id();
                                            });
                if (decoder != audio_decoders_.end()) {
                    (*decoder)->volume_ = asset.volume();
                }
            }
        }

        bool
        AudioDecodeService::UpdateAudioDecoders(const model::EditorProject &project) {
            ClearInvalidDecoders(project);
            double start_sec = 0.0;
            PreviewTimeline timeline(project);
            for (int i = 0; i < project.media_asset_size(); i++) {
                const model::MediaAsset &asset = project.media_asset(i);
                std::string audio_path;
                double src_file_duration = 0.0;
                double clipped_duration = 0.0;
                if (asset.has_media_asset_file_holder()) {
                    src_file_duration = asset.media_asset_file_holder().duration();
                }
                clipped_duration = src_file_duration;

                if (src_file_duration < TIME_EPS && clipped_duration < TIME_EPS) {
                    continue;
                }

                audio_path = asset.asset_path();

                AssetAudioDecoder *asset_audio_decoder = nullptr;
                int insert_pos = FindInsertDecoderPosition(project, i);
                asset_audio_decoder = AddAudioDecoder(audio_path, &asset, src_file_duration,
                                                      insert_pos);

                if (!asset_audio_decoder) {
                    start_sec += clipped_duration;
                    continue;
                }

                if (src_file_duration < TIME_EPS) {
                    src_file_duration = asset_audio_decoder->audio_decode_ctx_->duration_sec();
                }

                clipped_duration = src_file_duration;

                model::TimeRange display_range;
                display_range.set_start(start_sec);
                display_range.set_duration(clipped_duration);
                asset_audio_decoder->display_range_ = display_range;
                asset_audio_decoder->volume_ = asset.volume();
                asset_audio_decoder->is_repeat_ = false;

                start_sec += clipped_duration;
            }

            return true;
        }

        void
        AudioDecodeService::ClearInvalidDecoders(const model::EditorProject &project) {
            auto iter = audio_decoders_.begin();
            while (iter != audio_decoders_.end()) {
                bool should_remove = true;
                // found it in track assets
                for (model::MediaAsset asset : project.media_asset()) {
                    if (asset.asset_id() == (*iter)->asset_id_
                        && asset.asset_path() == (*iter)->asset_path_
                        && (*iter)->audio_decode_ctx_) {
                        should_remove = false;
                    }
                }

                if (should_remove) {
                    iter = audio_decoders_.erase(iter);
                } else {
                    ++iter;
                }
            }
        }

        int
        AudioDecodeService::FindInsertDecoderPosition(const model::EditorProject &project,
                                                           int track_index) {
            if (track_index == 0) {
                return 0;
            }

            model::MediaAsset asset_prev = project.media_asset(track_index - 1);

            int insert_pos = 1;
            auto iter = audio_decoders_.begin();
            while (iter != audio_decoders_.end()) {

                if (asset_prev.asset_id() == (*iter)->asset_id_
                    && asset_prev.asset_path() == (*iter)->asset_path_
                    && (*iter)->audio_decode_ctx_) {
                    return insert_pos;
                }

                ++insert_pos;
                ++iter;
            }
            return insert_pos;
        }
    }
}
