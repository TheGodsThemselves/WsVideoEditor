//
// Created by 何时夕 on 2020-01-05.
//

#include "platform_logger.h"
#include "ws_shader_program.h"
#include "shader_program_pool.h"

namespace whensunset {
    namespace wsvideoeditor {

        static GLuint LoadGLSLShader(const char *code, int length, GLenum shader_type) {
            GLuint shader = glCreateShader(shader_type);
            GLint code_size = length;
            glShaderSource(shader, 1, &code, &code_size);
            glCompileShader(shader);
            GLint result;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

            if (GL_TRUE != result) {
                GLchar pMessage[2048];
                GLsizei msgLen = 1023;
                glGetShaderInfoLog(shader, sizeof pMessage, &msgLen, pMessage);

                LOGI("LoadGLSLShader error %s, shader:\n%s\n ", pMessage, code);
                glDeleteShader(shader);
                shader = 0;
            }
            return shader;
        }

        const GLfloat DefaultVertexCoordinates[4][3] = {
                {-1.0f, -1.0f, 0.0f},
                {1.0f,  -1.0f, 0.0f},
                {-1.0f, 1.0f,  0.0f},
                {1.0f,  1.0f,  0.0f}
        };

        const GLfloat DefaultTextureCoordinates[4][2] = {
                {0.0f, 0.0f},
                {1.0f, 0.0f},
                {0.0f, 1.0f},
                {1.0f, 1.0f},
        };

        const GLfloat VertexCoordinates90[4][3] = {
                {-1.0f, -1.0f, 0.0f},
                {1.0f,  -1.0f, 0.0f},
                {-1.0f, 1.0f,  0.0f},
                {1.0f,  1.0f,  0.0f}
        };

        const GLfloat TextureCoordinates90[4][2] = {
                {1.0f, 0.0f},
                {1.0f, 1.0f},
                {0.0f, 0.0f},
                {0.0f, 1.0f},
        };

        const GLfloat VertexCoordinates180[4][3] = {
                {-1.0f, -1.0f, 0.0f},
                {1.0f,  -1.0f, 0.0f},
                {-1.0f, 1.0f,  0.0f},
                {1.0f,  1.0f,  0.0f}
        };

        const GLfloat TextureCoordinates180[4][2] = {
                {1.0f, 1.0f},
                {0.0f, 1.0f},
                {1.0f, 0.0f},
                {0.0f, 0.0f},
        };

        const GLfloat VertexCoordinates270[4][3] = {
                {-1.0f, -1.0f, 0.0f},
                {1.0f,  -1.0f, 0.0f},
                {-1.0f, 1.0f,  0.0f},
                {1.0f,  1.0f,  0.0f}
        };

        const GLfloat TextureCoordinates270[4][2] = {
                {0.0f, 1.0f},
                {0.0f, 0.0f},
                {1.0f, 1.0f},
                {1.0f, 0.0f},
        };

        const VertexCoord *VertexCoordWithRotation(int rotation) {
            if (rotation == 0) {
                return &DefaultVertexCoordinates;
            } else if (rotation == 90) {
                return &VertexCoordinates90;
            } else if (rotation == 180) {
                return &VertexCoordinates180;
            } else if (rotation == 270) {
                return &VertexCoordinates270;
            } else {
                return &DefaultVertexCoordinates;
            }
        }

        const TextureCoord *TextureCoordWithRotation(int rotation) {
            if (rotation == 0) {
                return &DefaultTextureCoordinates;
            } else if (rotation == 90) {
                return &TextureCoordinates90;
            } else if (rotation == 180) {
                return &TextureCoordinates180;
            } else if (rotation == 270) {
                return &TextureCoordinates270;
            } else {
                return &DefaultTextureCoordinates;
            }
        }

        bool
        WsShaderProgram::CreateProgram(ShaderProgramPool *shader_program_pool, const char
        *vertex_shader_code, const char *fragment_shader_code) {
            GLuint vertex_shader = LoadGLSLShader(vertex_shader_code,
                                                  (int) strlen(vertex_shader_code),
                                                  GL_VERTEX_SHADER);
            if (vertex_shader == 0) {
                return false;
            }
            GLuint fragment_shader = LoadGLSLShader(fragment_shader_code,
                                                    (int) strlen(fragment_shader_code),
                                                    GL_FRAGMENT_SHADER);
            if (fragment_shader == 0) {
                return false;
            }

            program_id_ = glCreateProgram();
            if (program_id_ == 0) {
                return false;
            }
            shader_program_pool_ = shader_program_pool;

            glAttachShader(program_id_, vertex_shader);
            glAttachShader(program_id_, fragment_shader);
            glLinkProgram(program_id_);
            glDetachShader(program_id_, vertex_shader);
            glDeleteShader(vertex_shader);
            glDetachShader(program_id_, fragment_shader);
            glDeleteShader(fragment_shader);
            LOGI("WsShaderProgram::CreateProgram program_id_:%d, vertex_shader:%d, "
                 "fragment_shader:%d", program_id_, vertex_shader, fragment_shader);
            return true;
        }

        WsShaderProgram::~WsShaderProgram() {
            if (program_id_) {
                glDeleteProgram(program_id_);
                program_id_ = 0;
                glFinish();
            }
        }

        UniqueWsTexturePtr WsShaderProgram::GetWsTexturePtr(int width, int height) {
            return shader_program_pool_->GetWsTexturePtr(width, height);
        }
    }
}