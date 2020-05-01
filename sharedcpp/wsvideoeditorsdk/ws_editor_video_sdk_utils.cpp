#include "ws_video_editor_sdk.pb.h"
#include "preview_timeline.h"
#include "ws_editor_video_sdk_utils.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
};

using namespace std;
namespace whensunset {
    namespace wsvideoeditor {
        static void sanitizein2(uint8_t *line) {
            while (*line) {
                if (*line < 0x08 || (*line > 0x0D && *line < 0x20))
                    *line = '?';
                line++;
            }
        }

        static void av_log_callback2(void *ptr, int level, const char *fmt, va_list vl) {
            int print_prefix = 1;
            char line[1024];

            if (level > av_log_get_level())
                return;

            av_log_format_line(ptr, level, fmt, vl, line, sizeof(line), &print_prefix);
            sanitizein2((uint8_t *) line);
        }

        void InitSDK() {
            av_register_all();
            avfilter_register_all();
            av_log_set_callback(av_log_callback2);
            av_log_set_level(AV_LOG_DEBUG);
        }

        int LoadProject(model::EditorProject *project) {
            int ret = 0;
            model::EditorProjectPrivateData *private_data = project->mutable_private_data();
            private_data->set_input_media_assets_number(project->media_asset_size());

            for (int i = 0; i < private_data->input_media_assets_number(); ++i) {
                model::MediaAsset *asset = project->mutable_media_asset(i);

                if ((ret = OpenMediaFile(asset->asset_path().c_str(),
                                         asset->mutable_media_asset_file_holder())) < 0) {
                    return ret;
                }
            }
            CalculateDurationAndDimension(project);
            return 0;
        }

        // 根据初始化的 video asset 数据来计算视频的宽、高、fps、duration 之类的数据，然后存起来
        void CalculateDurationAndDimension(model::EditorProject *project) {
            int width = 0;
            int height = 0;
            double fps = 0.0;
            GetProjectDimensionAndFps(project, &width, &height, &fps);
            model::EditorProjectPrivateData *private_data = project->mutable_private_data();
            private_data->set_project_duration(CalcProjectDuration(*project));
            private_data->set_project_fps(fps);
            private_data->set_project_width(width);
            private_data->set_project_height(height);
        }

        void GetProjectDimensionAndFps(model::EditorProject *project, int *width, int *height,
                                       double *fps) {
            GetProjectDimensionUnAlignAndUnLimit(project, width, height, fps);

            // limit max project size 720p or 1080p(single image)!!!
            LimitWidthAndHeight(*width, *height, ProjectMaxOutputShortEdge(*project),
                                ProjectMaxOutputLongEdge(*project), width, height);
        }

        void LimitWidthAndHeight(int original_width, int original_height, int max_short_edge,
                                 int max_long_edge, int *width_out,
                                 int *height_out) {
            int short_edge = min(original_width, original_height);
            int long_edge = max(original_width, original_height);

            if (short_edge > max_short_edge || long_edge > max_long_edge) {
                double ratio = min(max_short_edge / (double) short_edge,
                                   max_long_edge / (double) long_edge);
                *width_out = static_cast<int>(original_width * ratio);
                // height use ceil() to forbid black margin in up and down borders of player
                *height_out = static_cast<int>(ceil(original_height * ratio));
            } else {
                *width_out = original_width;
                *height_out = original_height;
            }

            *width_out += (*width_out) % 2;
            *height_out += (*height_out) % 2;
        }

        void GetProjectDimensionUnAlignAndUnLimit(model::EditorProject *project, int *width,
                                                  int *height, double *fps) {
            double max_ratio = 0.0;
            double fps_temp = -1.0;
            int max_width = 0;
            int width_temp, height_temp;
            bool find_first_video_stream = false;
            for (int i = 0; i < project->media_asset_size(); ++i) {
                model::MediaAsset *asset = project->mutable_media_asset(i);
                if (CachedMediaFileHolder(asset)->streams_size() == 0) {
                    continue;
                }
                for (model::MediaStreamHolder stream : CachedMediaFileHolder(asset)->streams()) {
                    if (stream.codec_type() == "video") {
                        width_temp = stream.width();
                        height_temp = stream.height();

                        if (asset->alpha_info() == model::YUV_ALPHA_TYPE_LEFT_RIGHT) {
                            width_temp /= 2;
                            width_temp += (width_temp % 2);
                        } else if (asset->alpha_info() == model::YUV_ALPHA_TYPE_TOP_BOTTOM) {
                            height_temp /= 2;
                            height_temp += (height_temp % 2);
                        }

                        if (stream.sample_aspect_ratio().den() != 0 &&
                            stream.sample_aspect_ratio().num() != 0) {
                            if (stream.sample_aspect_ratio().den() >
                                stream.sample_aspect_ratio().num()) {
                                width_temp = (int) (width_temp *
                                                    stream.sample_aspect_ratio().num() /
                                                    stream.sample_aspect_ratio().den());
                                width_temp += width_temp % 2;
                            } else {
                                height_temp = (int) (height_temp *
                                                     stream.sample_aspect_ratio().den() /
                                                     stream.sample_aspect_ratio().num());
                                height_temp += height_temp % 2;
                            }
                        }
                        int rotation = GetMediaAssetRotation(*asset);
                        if (rotation == 90 || rotation == 270) {
                            swap(width_temp, height_temp);
                        }
                        //compare all videos, take the maximum fps
                        double asset_fps = RationalToDouble(stream.guessed_frame_rate());
                        fps_temp = max(fps_temp, asset_fps);
                        max_width = max(max_width, width_temp);
                        if ((width_temp > 0 && height_temp * 1.0 / width_temp > max_ratio) &&
                            !find_first_video_stream) {
                            *width = width_temp;
                            *height = height_temp;
                            max_ratio = height_temp * 1.0 / width_temp;
                        }
                        find_first_video_stream = true;
                        break;
                    }
                }
            }
            if (fps_temp < TIME_EPS) {
                *fps = 30.0;
            } else {
                *fps = min(fps_temp, 30.0);
            }
        }

        int GetMediaAssetRotation(const model::MediaAsset &asset) {
            if (asset.has_media_asset_file_holder()) {
                model::MediaFileHolder holder_file = asset.media_asset_file_holder();
                for (int i = 0; i < holder_file.streams_size(); i++) {
                    model::MediaStreamHolder holder_stream = holder_file.streams(i);
                    if (holder_stream.codec_type() == "video") {
                        int nature_rotation = holder_stream.rotation();
                        int user_rotation = 0;
                        return ((nature_rotation + user_rotation) % 360 + 360) %
                               360;  // normalize to [0, 360)
                    }
                }
            }
            return 0;
        }

        double RationalToDouble(const model::Rational &rational) {
            return rational.den() > 0 ? rational.num() / (double) rational.den() : 0.0;
        }

        double CalcProjectDuration(const model::EditorProject &project) {
            double total_duration = 0.0;
            for (const model::MediaAsset &asset : project.media_asset()) {
                if (asset.media_asset_file_holder().streams_size() == 0) {
                    continue;
                }
                total_duration += asset.media_asset_file_holder().duration();
            }

            return total_duration;
        }

        int OpenMediaFile(const char *path, model::MediaFileHolder *media_file_holder) {
            if (!media_file_holder) {
                assert(0);
                return AVERROR(EINVAL);
            }
            if (!path) {
                return AVERROR_INVALIDDATA;
            }
            media_file_holder->set_path(path);
            media_file_holder->set_format_name("unknown");
            media_file_holder->set_probe_score(0);
            media_file_holder->set_num_streams(0);
            media_file_holder->set_duration(0);
            media_file_holder->set_media_strema_index(-1);
            media_file_holder->set_audio_strema_index(-1);

            int ret = 0;
            AVFormatContext *fmtCtx = NULL;
            AVDictionary *opts = nullptr;
            AVInputFormat *fileInputFormat = NULL;

            if ((ret = avformat_open_input(&fmtCtx, path, fileInputFormat, &opts)) < 0) {
                av_dict_free(&opts);
                return ret;
            }
            av_dict_free(&opts);

            fmtCtx->max_analyze_duration = AV_TIME_BASE;
            if ((ret = avformat_find_stream_info(fmtCtx, NULL)) < 0) {
                avformat_close_input(&fmtCtx);
                return ret;
            }

            media_file_holder->set_path(path);
            media_file_holder->set_format_name(fmtCtx->iformat->name);
            media_file_holder->set_probe_score(fmtCtx->probe_score);
            media_file_holder->set_num_streams(fmtCtx->nb_streams);
            media_file_holder->set_duration(0.0);

            AVCodec *videoCodec = nullptr;
            int best_video_stream = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                                        &videoCodec, 0);
            media_file_holder->set_media_strema_index(
                    best_video_stream >= 0 ? best_video_stream : -1);

            int bestAudioStream = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1,
                                                      nullptr, 0);
            media_file_holder->set_audio_strema_index(
                    bestAudioStream != AVERROR_STREAM_NOT_FOUND ? bestAudioStream : -1);

            double duration = DBL_MAX;
            media_file_holder->clear_streams();
            for (int i = 0; i < fmtCtx->nb_streams; ++i) {
                AVStream *stream = fmtCtx->streams[i];
                model::MediaStreamHolder *holderStream = media_file_holder->add_streams();

                holderStream->set_width(stream->codec->width);
                holderStream->set_height(stream->codec->height);

                // audio and video lengths may be not identical, so, for video, set duration to video stream length
                double stream_duration = (stream->duration == AV_NOPTS_VALUE ? 0.0 : (
                        (double) stream->duration * stream->time_base.num / stream->time_base.den));

                if (media_file_holder->media_strema_index() == i) {
                    duration = stream_duration;
                } else if (media_file_holder->media_strema_index() == -1) {
                    // no video stream
                    duration = min(duration, stream_duration);
                }

                switch (stream->codec->codec_type) {
                    case AVMEDIA_TYPE_VIDEO: {
                        holderStream->set_codec_type("video");
                        AVDictionaryEntry *entry = av_dict_get(stream->metadata, "rotate", NULL, 0);
                        if (entry != NULL && entry->value != NULL && entry->value[0] != '\0') {
                            holderStream->set_rotation(atoi(entry->value));
                        }
                        break;
                    }
                    case AVMEDIA_TYPE_AUDIO:
                        holderStream->set_codec_type("audio");
                        break;
                    case AVMEDIA_TYPE_DATA:
                        holderStream->set_codec_type("data");
                        break;
                    case AVMEDIA_TYPE_SUBTITLE:
                        holderStream->set_codec_type("subtitle");
                        break;
                    case AVMEDIA_TYPE_ATTACHMENT:
                        holderStream->set_codec_type("attachment");
                        break;
                    case AVMEDIA_TYPE_NB:
                        holderStream->set_codec_type("nb");
                        break;
                    default:
                        holderStream->set_codec_type("unknown");
                }
            }
            if (fmtCtx->nb_streams <= 0) {
                duration = 0.0;
            }
            media_file_holder->set_duration(duration);

            avformat_close_input(&fmtCtx);
            return ret;
        }

        double
        CalcMediaAssetStartTime(const model::EditorProject &project, int media_asset_index) {
            double start_time = 0.0;
            for (int i = 0; i < min(media_asset_index, project.media_asset_size()); i++) {
                start_time += project.media_asset(i).media_asset_file_holder().duration();
            }
            return start_time;
        }

        bool operator==(const model::TimeRange &lhs, const model::TimeRange &rhs) {
            return lhs.start() == rhs.start() && lhs.duration() == rhs.duration();
        }

        bool operator!=(const model::TimeRange &lhs, const model::TimeRange &rhs) {
            return !(lhs == rhs);
        }

        double ProjectRenderPosToAssetRenderPos(const model::EditorProject &project,
                                                double project_pts,
                                                int asset_index) {
            if (asset_index < 0 || asset_index >= project.media_asset_size()) {
                return 0.0;
            }
            double asset_pts = project_pts - CalcMediaAssetStartTime(project, asset_index);
            return asset_pts;
        }


        model::MediaFileHolder *CachedMediaFileHolder(model::MediaAsset *asset) {
            if (!asset->has_media_asset_file_holder() ||
                asset->media_asset_file_holder().path() == ""
                || (asset->media_asset_file_holder().path() != asset->asset_path())) {
                OpenMediaFile(asset->asset_path().c_str(),
                              asset->mutable_media_asset_file_holder());
            }
            return asset->mutable_media_asset_file_holder();
        }

        string ExtName(const string &file_path) {
            size_t dot_index = file_path.rfind(".");
            string ext;
            if (dot_index != string::npos) {
                ext = file_path.substr(dot_index + 1);
            }
            return ext;
        }

        int ProjectMaxOutputShortEdge(const model::EditorProject &project) {
            return ShortEdge720p;
        }

        int ProjectMaxOutputLongEdge(const model::EditorProject &project) {
            return LongEdge720p;
        }

        int GetMediaAssetIndexByRenderPos(const model::EditorProject &project, double render_pos) {
            double sum = 0.0;
            for (int i = 0; i < project.media_asset_size(); ++i) {
                double duration = project.media_asset(i).media_asset_file_holder().duration();

                if (sum < render_pos && render_pos < sum + duration) {
                    return i;
                }
                sum += duration;
            }
            return project.media_asset_size() - 1;
        }

        bool IsAudioVolumeChanged(const model::EditorProject &old_prj,
                                  const model::EditorProject &new_prj) {
            if (old_prj.media_asset_size() != new_prj.media_asset_size()) {
                return true;
            }
            for (int i = 0; i < old_prj.media_asset_size(); ++i) {
                if (fabs(old_prj.media_asset(i).volume() - new_prj.media_asset(i).volume()) >
                    1e-3) {
                    return true;
                }
            }
            return false;
        }

        bool IsAudioAssetsChanged(const model::EditorProject &old_prj,
                                  const model::EditorProject &new_prj) {
            if (IsProjectTimelineChanged(old_prj, new_prj)) {
                return true;
            }

            for (int i = 0; i < old_prj.media_asset_size(); i++) {
                for (int j = 0; j < new_prj.media_asset_size(); j++) {
                    model::MediaAsset new_prj_track_asset = new_prj.media_asset(j);
                    model::MediaAsset old_prj_track_asset = old_prj.media_asset(i);
                    if (old_prj_track_asset.asset_id() == new_prj_track_asset.asset_id()) {
                        return true;
                    }
                }
            }
            return false;
        }

        bool IsProjectTimelineChanged(const model::EditorProject &old_prj,
                                      const model::EditorProject &new_prj) {

            if (new_prj.media_asset_size() != old_prj.media_asset_size()) {
                return true;
            }
            return IsProjectInputTrackAssetsChanged(old_prj, new_prj);
        }

        bool IsProjectInputTrackAssetsChanged(const model::EditorProject &old_prj,
                                              const model::EditorProject &new_prj) {
            if (new_prj.project_id() != old_prj.project_id()) {
                return true;
            }
            if (new_prj.private_data().input_media_assets_number() !=
                old_prj.private_data().input_media_assets_number()) {
                return true;
            }
            for (int i = 0; i < new_prj.private_data().input_media_assets_number(); ++i) {
                const model::MediaAsset &new_asset = new_prj.media_asset(i);
                const model::MediaAsset &old_asset = old_prj.media_asset(i);
                if (new_asset.asset_path() != old_asset.asset_path()) {
                    return true;
                }
            }
            return false;
        }

        void ClearFileHolderIfAssetIdChanged(model::EditorProject &project,
                                             const model::EditorProject &old_project) {
            int len = min(project.media_asset_size(), old_project.media_asset_size());
            for (int i = 0; i < len; ++i) {
                if (project.media_asset(i).asset_id() != old_project.media_asset(i).asset_id()) {
                    project.mutable_media_asset(i)->clear_media_asset_file_holder();
                }
            }
        }
    }
}
