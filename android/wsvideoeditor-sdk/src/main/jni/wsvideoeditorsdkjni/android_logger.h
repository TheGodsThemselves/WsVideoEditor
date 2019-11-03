#ifndef ANDROID_WS_VIDEO_EDITOR_ANDROID_LOGGER_H
#define ANDROID_WS_VIDEO_EDITOR_ANDROID_LOGGER_H

#include <jni.h>
#include "jni_helper.h"
#include <android/log.h>

namespace whensunset {
    namespace wsvideoeditor {
        namespace android_logger {
            void Init();

            int LogPrint(int, const char *, const char *, ...);
        }
    }
}
#endif
