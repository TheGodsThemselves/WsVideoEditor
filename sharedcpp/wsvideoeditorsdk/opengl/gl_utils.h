#ifndef SHAREDCPP_WS_VIDEO_EDITOR_GL_UTILS_H
#define SHAREDCPP_WS_VIDEO_EDITOR_GL_UTILS_H

#include "platform_gl_headers.h"
#include <memory>
#include <string>
#include <thread>

namespace whensunset {
    namespace wsvideoeditor {
        GLenum CheckGlError(const char *message);

        GLenum GetGlError();

        bool IsEs3Supported();

    }
}

#endif
