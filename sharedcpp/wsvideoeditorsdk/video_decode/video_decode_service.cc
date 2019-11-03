#include "ws_editor_video_sdk_utils.h"
#include "video_decode_service.h"
#include "constants.h"
#include <cmath>

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/buffer.h"
}

namespace whensunset {
    namespace wsvideoeditor {

        const double kMaxBiasOfLastFrame = 0.1;

        void VideoDecodeService::SetProject(const model::EditorProject &project,
                                            double render_pos) {
            std::lock_guard<std::mutex> pop_frame_lk(pop_frame_mutex_);
            std::lock_guard<std::mutex> lk(member_param_mutex_);

            if (released_) {
                return;
            }
            project_ = project;
            project_changed_ = true;
            ended_ = false;
            decoded_unit_queue_.Clear();
            changed_render_pos_ = render_pos;
            decode_thread_waiting_cv_.notify_all();
            LOGI("VideoDecodeService::SetProject render_pos:%f", render_pos);
        }

        void VideoDecodeService::UpdateProject(const model::EditorProject &project) {
            {
                std::lock_guard<std::mutex> lk(member_param_mutex_);
                if (released_) {
                    return;
                }
                project_ = project;
                project_changed_ = true;
                LOGI("VideoDecodeService::UpdateProject");
            }
            decode_thread_waiting_cv_.notify_all();
        }

        void VideoDecodeService::ResetDecodePosition(double render_pos) {
            std::lock_guard<std::mutex> pop_frame_lk(pop_frame_mutex_);
            std::lock_guard<std::mutex> lk(member_param_mutex_);
            if (released_) {
                LOGI("VideoDecodeService::ResetDecodePosition released render_pos:%f", render_pos);
                return;
            }

            ended_ = false;
            decoded_unit_queue_.Close();
            changed_render_pos_ = render_pos;
            decode_thread_waiting_cv_.notify_all();
            LOGI("VideoDecodeService::ResetDecodePosition render_pos:%f", render_pos);
        }

        int
        VideoDecodeService::OpenMediaAsset(VideoDecodeContext &ctx, model::MediaAsset *asset) {
            int ret = 0;
            model::MediaFileHolder *media_file_holder = CachedMediaFileHolder(asset);
            std::string file_path = media_file_holder->path();
            model::MediaStreamHolder video_stream;
            if (media_file_holder->media_strema_index() >= 0 &&
                media_file_holder->streams_size() > media_file_holder->media_strema_index()) {
                video_stream = media_file_holder->streams(media_file_holder->media_strema_index());
            }
            ret = ctx.OpenFile(file_path);
            ctx.origin_path_ = asset->asset_path();
            LOGI("VideoDecodeService::OpenMediaAsset file_path:%s", file_path.c_str());
            return ret;
        }

        void VideoDecodeService::DecodeThreadMain() {
            SetCurrentThreadName("EditorTrackVideoDecode");
            model::EditorProject project;
            {
                std::unique_lock<std::mutex> lk(member_param_mutex_);
                project = project_;
            }
            std::unique_ptr<PreviewTimeline> preview_timeline{
                    new(std::nothrow) PreviewTimeline(project)};
            std::unique_ptr<VideoDecodeContext> ctx_current(
                    new(std::nothrow)VideoDecodeContext());
            if (!ctx_current || !preview_timeline) {
                LOGE("VideoDecodeService::DecodeThreadMain OOM 1");
                return;
            }

            MediaAssetSegment current_segment = preview_timeline->GetSegmentFromRenderPos(0.0);
            int ret = 0, decoding_asset_index = 0;
            bool is_first_frame_decoded_after_seek = false;
            double seek_pos_sec = 0.0, catch_up_to_sec_after_seek = 0.0;
            LOGI("VideoDecodeService::DecodeThreadMain start decode loop current_segment:%s",
                 current_segment.ToString().c_str());
            while (true) {
                double changed_render_pos = 0;
                bool project_changed = false;
                {
                    std::unique_lock<std::mutex> lk(member_param_mutex_);
                    decode_thread_waiting_cv_.wait(lk, [this] {
                        bool has_media_asset = HasMediaAsset();
                        LOGI("VideoDecodeService::DecodeThreadMain dtw1 stopped_:%s, has_media_asset:%s",
                             BoTSt(stopped_).c_str(), BoTSt(has_media_asset).c_str());
                        return (has_media_asset || stopped_);
                    });
                    if (stopped_) {
                        LOGI("VideoDecodeService::DecodeThreadMain stopped break 1");
                        break;
                    }

                    if (project_changed_) {
                        project = project_;
                        project_changed = true;
                        project_changed_ = false;
                        preview_timeline.reset(new(std::nothrow)PreviewTimeline(project));
                        LOGI("VideoDecodeService::DecodeThreadMain project changed");
                    }

                    changed_render_pos = changed_render_pos_;
                    changed_render_pos_ = -1;
                    LOGI("VideoDecodeService::DecodeThreadMain changed_render_pos:%f, project_changed:%s, stopped_:%s",
                         changed_render_pos, BoTSt(project_changed).c_str(), BoTSt(stopped_).c_str());
                }
                if (changed_render_pos) {
                    {
                        std::lock_guard<std::mutex> lk(member_param_mutex_);
                        if (stopped_) {
                            LOGI("VideoDecodeService::DecodeThreadMain rpc stopped break 2");
                            break;
                        }
                        ended_ = false;
                    }

                    is_first_frame_decoded_after_seek = true;
                    current_segment = preview_timeline->GetSegmentFromRenderPos(changed_render_pos);
                    int seek_to_asset_index = current_segment.media_asset_index();
                    bool decoding_asset_changed = false;
                    if (seek_to_asset_index != decoding_asset_index) {
                        decoding_asset_changed = true;
                        decoding_asset_index = seek_to_asset_index;
                        LOGI("VideoDecodeService::DecodeThreadMain rpc asset changed");
                    }
                    LOGI("VideoDecodeService::DecodeThreadMain rpc current_segment:%s, seek_to_asset_index:%d, decoding_asset_index:%d",
                         current_segment.ToString().c_str(), seek_to_asset_index, decoding_asset_index);
                    if (project_changed || decoding_asset_changed) {
                        model::MediaAsset *asset = project.mutable_media_asset(
                                decoding_asset_index);
                        if (OpenMediaAsset(*ctx_current, asset) < 0) {
                            LOGI("VideoDecodeService::DecodeThreadMain rpc project or decode changed open media asset failed");
                            break;
                        }
                        LOGI("VideoDecodeService::DecodeThreadMain rpc project or decode changed");
                    }
                    double media_asset_frame_rate = av_q2d(
                            ctx_current->video_stream_->avg_frame_rate);
                    media_asset_frame_rate = fmin(media_asset_frame_rate,
                                                  project.private_data().project_fps());
                    catch_up_to_sec_after_seek =
                            changed_render_pos - 1.0 / media_asset_frame_rate - kMaxBiasOfLastFrame;

                    double asset_render_pos = ProjectRenderPosToAssetRenderPos(project,
                                                                               changed_render_pos,
                                                                               decoding_asset_index);
                    LOGI("VideoDecodeService::DecodeThreadMain rpc seek failed media_asset_frame_rate:%f, catch_up_to_sec_after_seek:%f, asset_render_pos:%d",
                         media_asset_frame_rate, catch_up_to_sec_after_seek, asset_render_pos);
                    if (SeekTo(ctx_current.get(), asset_render_pos) < 0) {
                        LOGI("VideoDecodeService::DecodeThreadMain rpc seek failed");
                        break;
                    }
                    {
                        std::lock_guard<std::mutex> pop_frame_lk(pop_frame_mutex_);
                        decoded_unit_queue_.Clear();
                        decoded_unit_queue_.Open();
                    }
                    LOGI("VideoDecodeService::DecodeThreadMain rpc changed_render_pos:%d, ",
                         changed_render_pos);
                }
                int got_frame = 0;
                UniqueAVFramePtr frame = UniqueAVFramePtrCreateNull();
                const model::MediaAsset &decoding_asset = project.media_asset(
                        decoding_asset_index);

                double end_offset = current_segment.end_pos();
                double frame_timestamp_sec_in_track = 0.0;

                av_q2d(ctx_current->video_stream_->avg_frame_rate);
                frame = ReadOneFrame(ctx_current.get(), &ret);
                if (ret >= 0 && frame) {
                    frame_timestamp_sec_in_track = frame->pts * 1.0 / AV_TIME_BASE;
                    got_frame = 1;
                }
                LOGI("VideoDecodeService::DecodeThreadMain got_frame:%d, end_offset:%f, frame_timestamp_sec_in_track:%f, ret:%d",
                     got_frame, end_offset, frame_timestamp_sec_in_track, ret);
                {
                    std::lock_guard<std::mutex> lk(member_param_mutex_);
                    if (changed_render_pos_) {
                        LOGI("VideoDecodeService::DecodeThreadMain render pos changed");
                        continue;
                    }
                }

                if (frame) {
                    double frame_sec = frame->pts * 1.0 / AV_TIME_BASE;
                    if (frame_sec <= catch_up_to_sec_after_seek - PTS_EPS) {
                        LOGE("VideoDecodeService::DecodeThreadMain fv error frame_sec need bigger than catch_up_to_sec_after_seek frame_sec:%f, catch_up_to_sec_after_seek:%f",
                             frame_sec, catch_up_to_sec_after_seek);
                    } else if (frame_sec > end_offset) {
                        if (ctx_current->codec_context_ &&
                            avcodec_is_open(ctx_current->codec_context_)) {
                            avcodec_flush_buffers(ctx_current->codec_context_);
                        }
                        LOGI("VideoDecodeService::DecodeThreadMain fv is last frame in this media asset");
                    } else {
                        if (is_first_frame_decoded_after_seek && frame_sec >= seek_pos_sec) {
                            frame->pts = static_cast<int64_t>(seek_pos_sec * AV_TIME_BASE);
                        }

                        DecodedFramesUnit unit = DecodedFramesUnitCreateNull();
                        unit.frame = std::move(frame);
                        unit.frame_timestamp_sec = frame_timestamp_sec_in_track;
                        unit.frame_file = decoding_asset.asset_path();
                        unit.frame_media_asset_index = decoding_asset_index;
                        decoded_unit_queue_.PushBack(std::move(unit));
                        is_first_frame_decoded_after_seek = false;
                        LOGI("VideoDecodeService::DecodeThreadMain fv frame_sec:%f, seek_pos_sec:%f, frame_timestamp_sec_in_track:%f, decoding_asset_index%d",
                             frame_sec, seek_pos_sec, frame_timestamp_sec_in_track,
                             decoding_asset_index);
                    }
                }

                if (ctx_current->is_drain_loop_ && !got_frame) {
                    if (preview_timeline->IsLastSegment(current_segment)) {
                        DecodeEofHandle();
                        LOGI("VideoDecodeService::DecodeThreadMain afd this is last asset");
                    } else {
                        current_segment = preview_timeline->GetNextSegmentInTimeline(
                                current_segment);
                        decoding_asset_index = current_segment.media_asset_index();

                        std::string file_path = CachedMediaFileHolder(project.mutable_media_asset(
                                decoding_asset_index))->path();
                        if (OpenMediaAsset(*ctx_current, project.mutable_media_asset(
                                decoding_asset_index))) {
                            double pos_sec = ProjectRenderPosToAssetRenderPos(project,
                                                                              current_segment.start_pos(),
                                                                              decoding_asset_index);
                            ret = SeekTo(ctx_current.get(), pos_sec);
                            LOGI("VideoDecodeService::DecodeThreadMain open media asset pos_sec:%f, ret:%d",
                                 pos_sec, ret);
                        }
                        LOGI("VideoDecodeService::DecodeThreadMain afd jump to next asset next_segment:%s, file_path:%s",
                             current_segment.ToString().c_str(), file_path.c_str());
                        if (ret < 0) {
                            break;
                        }
                    }
                    LOGI("VideoDecodeService::DecodeThreadMain afd");
                }
            }

            {
                std::unique_lock<std::mutex> lk(member_param_mutex_);
                decode_thread_waiting_cv_.wait(lk, [this] {
                    LOGI("VideoDecodeService::DecodeThreadMain dtw2 stopped_:%s", BoTSt(stopped_).c_str());
                    return stopped_;
                });
            }
            ctx_current->Release();
            LOGI("VideoDecodeService::DecodeThreadMain decode loop end");
        }

        void VideoDecodeService::DecodeEofHandle() {
            std::unique_lock<std::mutex> lk(member_param_mutex_);
            bool has_position_change_request = false;
            if (changed_render_pos_) {
                has_position_change_request = true;
            }
            ended_ = true;
            lk.unlock();
            if (!has_position_change_request) {
                DecodedFramesUnit unit = DecodedFramesUnitCreateNull();
                UniqueAVFramePtr eof_frame{AllocVideoFrame(AV_PIX_FMT_YUV420P, 16, 16),
                                           FreeAVFrame};
                eof_frame->pts = 1000000000000000000LL;
                unit.frame = std::move(eof_frame);
                decoded_unit_queue_.PushBack(std::move(unit));
                LOGI("VideoDecodeService::DecodeEofHandle last frame");
            }
            lk.lock();
            decode_thread_waiting_cv_.wait(lk, [this] {
                LOGI("VideoDecodeService::DecodeEofHandle stopped_:%s", BoTSt(stopped_).c_str());
                return changed_render_pos_ || stopped_;
            });
            LOGI("VideoDecodeService::DecodeEofHandle has_position_change_request:%s",
                 BoTSt(has_position_change_request).c_str());
        }

        UniqueAVFramePtr VideoDecodeService::ReadOneFrame(VideoDecodeContext *ctx, int *ret) {
            while (true) {
                UniqueAVPacketPtr packet{av_packet_alloc(), FreeAVPacket};
                packet->data = nullptr;
                packet->size = 0;
                if (!ctx->is_drain_loop_) {
                    XASSERT(ctx->format_context_);
                    *ret = av_read_frame(ctx->format_context_, packet.get());
                    if (*ret == AVERROR_EOF) {
                        ctx->is_drain_loop_ = true;
                    } else if (*ret < 0) {
                        return UniqueAVFramePtrCreateNull();
                    }
                }
                if ((*ret >= 0 && packet->stream_index == ctx->video_stream_idx_) ||
                    ctx->is_drain_loop_) {
                    int got_frame = 0;
                    UniqueAVFramePtr frame = UniqueAVFramePtrCreateNull();
                    frame.reset(av_frame_alloc());
                    *ret = avcodec_decode_video2(ctx->codec_context_, frame.get(), &got_frame,
                                                 packet.get());
                    if (*ret < 0) {

                        return UniqueAVFramePtrCreateNull();
                    }

                    if (got_frame) {
                        if (frame->pts == AV_NOPTS_VALUE) {
                            // some videos' packet do not have pts (like kk recorder produced videos)
                            frame->pts = av_frame_get_best_effort_timestamp(frame.get());
                        }
                        ctx->current_pts_ = frame->pts;
                        frame->pts = av_rescale_q(frame->pts, ctx->video_stream_->time_base,
                                                  AV_TIME_BASE_Q);
                    } else {
                        frame.reset();
                    }
                    return frame;
                }
            }
        }

        int VideoDecodeService::SeekTo(VideoDecodeContext *ctx, double render_pos) {
            ctx->is_drain_loop_ = false;
            int64_t current_dts = ctx->current_pts_ + NoPtsToZero(ctx->video_stream_->first_dts);
            // Notice, do not add 0.5 for rounding the dts, we just get the floor value. Otherwise, bfr may loss the first frame (when seeked).
            int64_t target_dts = (int64_t) (render_pos / av_q2d(ctx->video_stream_->time_base))
                                 + NoPtsToZero(ctx->video_stream_->first_dts);

            // Quick fix: 57s bfr seek to the last two dts might incur av_seek_frame return -22 (invalid argument)
            // Here quick fix, bfr seek target_dts - 3
            std::string ext;
            size_t dot_index = ctx->origin_path_.rfind(".");
            if (dot_index != std::string::npos) {
                ext = ctx->origin_path_.substr(dot_index + 1);
            }

            int target_gop = -1, current_gop = -1;
            if (ctx->has_gop_structure_) {
                int i = 0;
                for (i = 0;
                     i < ctx->keyframe_dts_.size() && ctx->keyframe_dts_[i] <= target_dts; ++i);
                target_gop = i - 1;
                for (i = 0;
                     i < ctx->keyframe_dts_.size() && ctx->keyframe_dts_[i] <= current_dts; ++i);
                current_gop = i - 1;
            }

            // If target is in the same GoP and  target_dts >= last decoded frame's PTS + 5 frames safe boundary,
            // we can let it decode sequentially to the target
            int64_t no_seek_safe_timestamp = current_dts +
                                             av_rescale_q(5, ctx->video_stream_->avg_frame_rate,
                                                          ctx->video_stream_->time_base);

            if (target_gop != current_gop || target_dts < no_seek_safe_timestamp ||
                !ctx->has_gop_structure_) {
                int ret = av_seek_frame(ctx->format_context_, ctx->video_stream_idx_, target_dts,
                                        AVSEEK_FLAG_BACKWARD);
                if (ret < 0) {
                    // seeking with AVSEEK_FLAG_BACKWARD failed, seek again without flag
                    // Happens when decoding MPEG-4 encoded files
                    ret = av_seek_frame(ctx->format_context_, ctx->video_stream_idx_, target_dts,
                                        0);
                }
                if (ret < 0) {
                    return ret;
                }
                avcodec_flush_buffers(ctx->codec_context_);
            }
            return 0;
        }

        DecodedFramesUnit VideoDecodeService::GetRenderFrameAtPtsOrNull(double pts_sec,
                                                                        int get_frame_flags) {
            std::lock_guard<std::mutex> pop_frame_lk(pop_frame_mutex_);
            std::lock_guard<std::mutex> lock(member_param_mutex_);
            DecodedFramesUnit unit = DecodedFramesUnitCreateNull();
            if (stopped_) {
                return unit;
            }
            return GetRenderFrameAtPtsInternal(pts_sec, get_frame_flags);
        }

// If frames in queue has PTS 5, 6, 7, 8, ... and the client gets render frame at 0
// The frame will never be available, in this case, null is returned and *no_such_frame is set to true;
        DecodedFramesUnit
        VideoDecodeService::GetRenderFrameAtPtsInternal(double pts_sec, int get_frame_flags) {
            // Assume pop_frame_mutex_ and member_param_mutex_ are held
            int64_t pts = (int64_t) (pts_sec * AV_TIME_BASE + 0.5);
            DecodedFramesUnit ret = DecodedFramesUnitCreateNull();
            bool no_such_frame = true;

            if (std::fabs(pts_sec - 0.0) < PTS_EPS) {
                // first frame
                if (decoded_unit_queue_.Size() >= 1) {
                    bool got_frame = false;
                    auto result = decoded_unit_queue_.PopFrontIf(
                            [&](const std::vector<DecodedFramesUnit> &units) {
                                if (units.size() < 1) {
                                    return false;
                                }
                                double first_pts = units[0].frame->pts / (double) AV_TIME_BASE;
                                if (std::fabs(first_pts - pts_sec) < 0.005) {
                                    got_frame = true;
                                    return true;
                                }
                                return false;
                            });
                    if (got_frame) {
                        ret = std::move(result.second);
                        return ret;
                    }
                }
            }

            while (decoded_unit_queue_.Size() > 1 && !decoded_unit_queue_.is_closed()) {
                bool should_discard_first_frame = false;
                bool should_save_first_frame = false;
                auto result = decoded_unit_queue_.PopFrontIf(
                        [&](const std::vector<DecodedFramesUnit> &units) {
                            if (units.size() <= 1) {
                                return false;
                            }
                            int64_t first_pts = units[0].frame->pts;
                            int64_t second_pts = units[1].frame->pts;

                            // second_pts <= pts  ==>  the first frame should be dropped
                            should_discard_first_frame = second_pts <= pts;
                            // first_pts <= pts and second_pts > pts  ==>  the first frame is the candidate
                            should_save_first_frame = first_pts <= pts && second_pts > pts;
                            // first_pts > pts  ==>  no such frame.
                            // Note first_pts > pts will be evaluated multiple times and it should never be false
                            no_such_frame = no_such_frame && first_pts > pts;

                            // If get latest frame if lagged, always save the first frame when it's pts <= requested pts
                            if ((get_frame_flags & 1) && first_pts <= pts) {
                                should_discard_first_frame = false;
                                should_save_first_frame = true;
                            }

                            return should_discard_first_frame || should_save_first_frame;
                        });
                if (should_discard_first_frame) {
                    // Do nothing
                } else if (should_save_first_frame) {
                    ret = std::move(result.second);
                } else { // first->pts > pts
                    return ret;
                }
            }

            if (!ret && ended_ && !decoded_unit_queue_.Empty()) {
                auto result = decoded_unit_queue_.PopFrontIf(
                        [pts](const std::vector<DecodedFramesUnit> &units) {
                            return units.size() > 0 && units.front().frame->pts <= pts;
                        });
                if (result.first) {
                    ret = std::move(result.second);
                }
            }

            return ret;
        }

        void VideoDecodeService::Start() {
            std::lock_guard<std::mutex> start_stop_lk(start_stop_mutex_);
            std::lock_guard<std::mutex> lk(member_param_mutex_);
            if (!stopped_ || released_) {
                LOGI("VideoDecodeService::Start need not start stopped_:%s, released_:%s",
                     BoTSt(stopped_).c_str(),
                     BoTSt(released_).c_str());
                return;
            }
            stopped_ = false;
            decode_thread_ = std::thread(&VideoDecodeService::DecodeThreadMain, this);
            decoded_unit_queue_.Open();
            LOGI("VideoDecodeService::Start");
        }

        void VideoDecodeService::Stop() {
            std::lock_guard<std::mutex> start_stop_lk_(start_stop_mutex_);
            {
                std::lock_guard<std::mutex> pop_frame_lk(pop_frame_mutex_);
                std::lock_guard<std::mutex> lk(member_param_mutex_);
                stopped_ = true;
                decoded_unit_queue_.Close();
                decoded_unit_queue_.Clear();
            }
            decode_thread_waiting_cv_.notify_all();
            if (decode_thread_.joinable()) {
                decode_thread_.join();
            }
            LOGI("VideoDecodeService::Stop");
        }

        std::unique_ptr<VideoDecodeService> VideoDecodeServiceCreate(int buffer_capacity) {
            VideoDecodeService *impl = new(std::nothrow)VideoDecodeService(
                    buffer_capacity);
            if (!impl) {
                abort();
            }
            return std::unique_ptr<VideoDecodeService>{impl};
        }
    }
}
