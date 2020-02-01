#include "constants.h"
#include "ws_editor_video_sdk_utils.h"
#include "preview_timeline.h"
#include "frame_renderer.h"

namespace whensunset {
    namespace wsvideoeditor {
        std::vector<MediaAssetSegment>
        CalculateMediaAssetToSegment(const model::EditorProject &project) {
            std::vector<MediaAssetSegment> segments;
            int startPos = 0;
            for (int i = 0; i < project.media_asset_size(); ++i) {
                const model::MediaAsset mediaAsset = project.media_asset(i);
                double duration = mediaAsset.media_asset_file_holder().duration();
                segments.push_back(
                        MediaAssetSegment(i, mediaAsset.asset_id(), startPos, startPos + duration));
                startPos += duration;
            }
            return segments;
        }
    }
}
