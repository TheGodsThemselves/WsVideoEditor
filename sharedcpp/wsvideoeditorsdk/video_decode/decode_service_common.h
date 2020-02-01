#ifndef SHAREDCPP_WS_VIDEO_EDITOR_DECODE_SERVICE_COMMON_H
#define SHAREDCPP_WS_VIDEO_EDITOR_DECODE_SERVICE_COMMON_H

namespace whensunset {
    namespace wsvideoeditor {

        enum PlayerReadyState {
            kHaveNothing = 0,
            kHaveMetaData,
            kHaveCurrentData,
            kHaveEnoughData
        };

        struct DecodePositionChangeRequest {
            double render_pos;

            DecodePositionChangeRequest(double render_pos = 0.0, double playback_pts = -1.0)
                    : render_pos(render_pos) {}
        };
    }
}
#endif
