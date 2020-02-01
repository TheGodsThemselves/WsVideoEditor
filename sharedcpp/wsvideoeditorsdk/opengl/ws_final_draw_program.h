//
// Created by 何时夕 on 2020-01-05.
//

#ifndef ANDROID_WS_FINAL_DRAW_PROGRAM_H
#define ANDROID_WS_FINAL_DRAW_PROGRAM_H

#include "ws_shader_program.h"

namespace whensunset{
    namespace wsvideoeditor{
        class WsFinalDrawProgram{
        public:
            void SetRenderSize(int render_width, int render_height);

            void SetProjectSize(int project_width, int project_height);

            void CreateProgram(ShaderProgramPool *shader_program_pool);

            void DrawGlTexture(WsTexture *ws_texture);

        private:
            void UpdateViewTextureLocation();

            WsShaderProgram ws_shader_program_;

            GLfloat vertex_pos_[12];

            int project_height_;

            int project_width_;

            int render_height_;

            int render_width_;
        };

    }
}

#endif //ANDROID_WS_FINAL_DRAW_PROGRAM_H
