#ifndef SHAREDCPP_WS_VIDEO_EDITOR_EDITORSDK2UTILS_H
#define SHAREDCPP_WS_VIDEO_EDITOR_EDITORSDK2UTILS_H

#include "ws_video_editor_sdk.pb.h"

extern "C" {
#include <libavformat/avformat.h>
};


namespace whensunset {
    namespace wsvideoeditor {

        int OpenMediaFile(const char *path, model::MediaFileHolder *media_file_holder);

        double
        CalcMediaAssetStartTime(const model::EditorProject &project, int media_asset_index);


        std::string ExtName(const std::string &str);

        bool operator==(const model::TimeRange &lhs, const model::TimeRange &rhs);

        bool operator!=(const model::TimeRange &lhs, const model::TimeRange &rhs);

        double ProjectRenderPosToAssetRenderPos(const model::EditorProject &project,
                                                double project_pts,
                                                int asset_index);

        model::MediaFileHolder *CachedMediaFileHolder(model::MediaAsset *asset);

    }
}
#endif
