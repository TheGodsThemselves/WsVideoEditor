#ifndef SHAREDCPP_WS_VIDEO_EDITOR_EDITORSDK2UTILS_H
#define SHAREDCPP_WS_VIDEO_EDITOR_EDITORSDK2UTILS_H

#include "ws_video_editor_sdk.pb.h"

extern "C" {
#include "libavformat/avformat.h"
};


namespace whensunset {
    namespace wsvideoeditor {
        void InitSDK();

        int LoadProject(model::EditorProject *project);

        void CalculateDurationAndDimension(model::EditorProject *project);

        double CalcProjectDuration(const model::EditorProject &project);

        void
        GetProjectDimensionUnAlignAndUnLimit(model::EditorProject *project, int *width, int *height,
                                             double *fps);

        void LimitWidthAndHeight(int original_width, int original_height, int max_short_edge,
                                 int max_long_edge, int *width_out,
                                 int *height_out);

        void GetProjectDimensionAndFps(model::EditorProject *project, int *width, int *height,
                                       double *fps);

        int GetMediaAssetRotation(const model::MediaAsset &asset);

        double RationalToDouble(const model::Rational &rational);

        int OpenMediaFile(const char *path, model::MediaFileHolder *media_file_holder);

        double
        CalcMediaAssetStartTime(const model::EditorProject &project, int media_asset_index);

        int GetMediaAssetIndexByRenderPos(const model::EditorProject &project, double render_pos);

        std::string ExtName(const std::string &str);

        bool operator==(const model::TimeRange &lhs, const model::TimeRange &rhs);

        bool operator!=(const model::TimeRange &lhs, const model::TimeRange &rhs);

        double ProjectRenderPosToAssetRenderPos(const model::EditorProject &project,
                                                double project_pts,
                                                int asset_index);

        model::MediaFileHolder *CachedMediaFileHolder(model::MediaAsset *asset);

        int ProjectMaxOutputShortEdge(const model::EditorProject &project);

        int ProjectMaxOutputLongEdge(const model::EditorProject &project);

        bool IsAudioAssetsChanged(const model::EditorProject &old_prj,
                                  const model::EditorProject &new_prj);

        bool IsAudioVolumeChanged(const model::EditorProject &old_prj,
                                  const model::EditorProject &new_prj);

        bool IsProjectTimelineChanged(const model::EditorProject &old_prj,
                                      const model::EditorProject &new_prj);

        bool IsProjectInputTrackAssetsChanged(const model::EditorProject &old_prj,
                                              const model::EditorProject &new_prj);

        void ClearFileHolderIfAssetIdChanged(model::EditorProject &project,
                                             const model::EditorProject &old_project);
    }
}
#endif
