#ifndef SHAREDCPP_WS_VIDEO_EDITOR_REF_CLOCK_H
#define SHAREDCPP_WS_VIDEO_EDITOR_REF_CLOCK_H

#include <mutex>
#include "ws_editor_video_sdk_utils.h"
#include <cmath>

namespace whensunset {
    namespace wsvideoeditor {

        class RefClock {
        public:
            RefClock() : render_pos_(0.0) {}

            virtual ~RefClock() {};

            double GetRenderPos() {
                std::lock_guard<std::mutex> lk(mutex_);
                return render_pos_;
            }

            void SetPts(double render_pos) {
                std::lock_guard<std::mutex> lk(mutex_);
                render_pos_ = render_pos;
            }

        private:
            double render_pos_ = 0.0;
            std::mutex mutex_;
        };
    }
}
#endif
