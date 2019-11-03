#ifndef SHAREDCPP_WS_VIDEO_EDITOR_PREVIEW_TIMELINE_H
#define SHAREDCPP_WS_VIDEO_EDITOR_PREVIEW_TIMELINE_H

#include "platform_logger.h"
#include "constants.h"
#include <math.h>
#include <wsvideoeditorsdk/prebuilt_protobuf/ws_video_editor_sdk.pb.h>
#include <vector>
#include <string>

namespace whensunset {
    namespace wsvideoeditor {

        class MediaAssetSegment {
        public:
            MediaAssetSegment(int asset_index = 0, int64_t asset_id = 0, double start_pos = 0.0,
                              double end_pos = 0.0) {
                media_asset_index_ = asset_index;
                asset_id_ = asset_id;
                start_pos_ = start_pos;
                end_pos_ = end_pos;
            }

            int64_t asset_id() const {
                return asset_id_;
            }

            int media_asset_index() const {
                return media_asset_index_;
            }

            double start_pos() const {
                return start_pos_;
            }

            double end_pos() const {
                return end_pos_;
            }

            double preview_duration() const {
                return end_pos_ - start_pos_;
            }

            std::string ToString() {
                return ("media_asset_index_:" + std::to_string(media_asset_index_) + ",asset_id_:" +
                        std::to_string(asset_id_) + ",start_pos_:" + std::to_string(start_pos_) +
                        ",end_pos_:" + std::to_string(end_pos_));
            }

        private:
            int media_asset_index_;

            int64_t asset_id_;

            /**
             * 当前片段在整个 project 中的开始时间
             */
            double start_pos_;

            /**
             * 当前片段在整个 project 中的结束事件
             */
            double end_pos_;
        };

        std::vector<MediaAssetSegment>
        CalculateMediaAssetToSegment(const model::EditorProject &project);

        class PreviewTimeline {
        public:
            PreviewTimeline(const model::EditorProject &project) {
                if (project.media_asset_size() == 0) {
                    LOGE("PreviewTimeline error media asset size is 0");
                } else {
                    segments_ = CalculateMediaAssetToSegment(project);
                    accumulated_duration_.push_back(0);
                    for (int i = 0; i < segments_.size(); ++i) {
                        accumulated_duration_.push_back(
                                accumulated_duration_.back() + segments_[i].preview_duration());
                    }
                }
            }

            MediaAssetSegment GetSegmentFromRenderPos(double render_pos) {
                if (segments_.empty()) {
                    return MediaAssetSegment();
                }
                for (int i = 0; i < segments_.size(); i++) {
                    if ((render_pos + TIME_EPS) > segments_[i].start_pos() &&
                        render_pos < segments_[i].end_pos()) {
                        return segments_[i];
                    }
                }
                return segments_.back();
            }

            bool IsLastSegment(MediaAssetSegment current_segment) {
                if (segments_.empty()) {
                    return false;
                }
                MediaAssetSegment last_segment = segments_.back();
                return fabs(last_segment.start_pos() - current_segment.start_pos()) < TIME_EPS &&
                       fabs(last_segment.end_pos() - current_segment.end_pos()) < TIME_EPS;
            }

            MediaAssetSegment GetNextSegmentInTimeline(MediaAssetSegment current_segment) {
                if (segments_.empty()) {
                    return MediaAssetSegment();
                }
                int segment_index = GetSegmentIndexInTimeline(current_segment);
                if (segment_index < 0 || segment_index >= segments_.size() - 1) {
                    return segments_.front();
                }
                return segments_[segment_index + 1];
            }

        private:
            int GetSegmentIndexInTimeline(MediaAssetSegment current_segment) {
                for (int i = 0; i < segments_.size(); i++) {
                    if (fabs(segments_[i].start_pos() - current_segment.start_pos()) < TIME_EPS &&
                        fabs(segments_[i].end_pos() - current_segment.end_pos()) < TIME_EPS) {
                        return i;
                    }
                }
                return -1;
            }

            std::vector<MediaAssetSegment> segments_;
            std::vector<double> accumulated_duration_;
        };


    }  // namespace wsvideoeditor
}  // namespace whensunset

#endif
