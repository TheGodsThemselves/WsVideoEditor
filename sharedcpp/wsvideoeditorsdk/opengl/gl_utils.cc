#include "gl_utils.h"
#include <string>
#include <cstring>
#include "platform_logger.h"

namespace whensunset {
    namespace wsvideoeditor {

        GLenum CheckGlError(const char *message) {
            GLenum ret = GL_NO_ERROR;
            for (GLenum error = glGetError(); error; error = glGetError()) {
                LOGE("after %s() glError (0x%x)\n", message, error);
                ret = error;
            }
            return ret;
        }

        bool IsEs3Supported() {
            const GLubyte *version = glGetString(GL_VERSION);
            if (!version) {
                return false;
            }
            return strstr((const char *) version, "OpenGL ES 3") != nullptr;
        }

        GLenum GetGlError() {
            GLenum error = GL_NO_ERROR;
            for (GLenum error = glGetError(); error; error = glGetError());
            return error;
        }
    }
}
