#include "ws_video_editor_sdk.pb.h"
#include "preview_timeline.h"
#include "ws_editor_video_sdk_utils.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
};

using namespace std;
namespace whensunset {
    namespace wsvideoeditor {

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
                model::MediaStreamHolder *probedStream = media_file_holder->add_streams();

                probedStream->set_width(stream->codec->width);
                probedStream->set_height(stream->codec->height);

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
                        probedStream->set_codec_type("video");
                        AVDictionaryEntry *entry = av_dict_get(stream->metadata, "rotate", NULL, 0);
                        if (entry != NULL && entry->value != NULL && entry->value[0] != '\0') {
                            probedStream->set_rotation(atoi(entry->value));
                        }
                        break;
                    }
                    case AVMEDIA_TYPE_AUDIO:
                        probedStream->set_codec_type("audio");
                        break;
                    case AVMEDIA_TYPE_DATA:
                        probedStream->set_codec_type("data");
                        break;
                    case AVMEDIA_TYPE_SUBTITLE:
                        probedStream->set_codec_type("subtitle");
                        break;
                    case AVMEDIA_TYPE_ATTACHMENT:
                        probedStream->set_codec_type("attachment");
                        break;
                    case AVMEDIA_TYPE_NB:
                        probedStream->set_codec_type("nb");
                        break;
                    default:
                        probedStream->set_codec_type("unknown");
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
                OpenMediaFile(asset->asset_path().c_str(), asset->mutable_media_asset_file_holder());
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

    }
}
