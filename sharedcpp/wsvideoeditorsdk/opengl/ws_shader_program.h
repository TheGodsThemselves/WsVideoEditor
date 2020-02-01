//
// Created by 何时夕 on 2020-01-05.
//

#ifndef ANDROID_WS_SHADER_H
#define ANDROID_WS_SHADER_H

#include "ws_texture_fbo.h"

namespace whensunset {
    namespace wsvideoeditor {

        extern const GLfloat DefaultVertexCoordinates[4][3];
        extern const GLfloat DefaultTextureCoordinates[4][2];
        extern const GLfloat VertexCoordinates90[4][3];
        extern const GLfloat TextureCoordinates90[4][2];
        extern const GLfloat VertexCoordinates180[4][3];
        extern const GLfloat TextureCoordinates180[4][2];
        extern const GLfloat VertexCoordinates270[4][3];
        extern const GLfloat TextureCoordinates270[4][2];

        typedef GLfloat VertexCoord[4][3];
        typedef GLfloat TextureCoord[4][2];

        const VertexCoord *VertexCoordWithRotation(int rotation);

        const TextureCoord *TextureCoordWithRotation(int rotation);

        class ShaderProgramPool;

        class WsShaderProgram {
        public:
            bool
            CreateProgram(ShaderProgramPool *shader_program_pool, const char *vertex_shader_code,
                          const char *fragment_shader_code);

            UniqueWsTexturePtr GetWsTexturePtr(int width, int height);

            GLuint program_id() const {
                return program_id_;
            }

            virtual ~WsShaderProgram();

        private:
            ShaderProgramPool *shader_program_pool_;

            GLuint program_id_;
        };

    }
}


#endif //ANDROID_WS_SHADER_H
