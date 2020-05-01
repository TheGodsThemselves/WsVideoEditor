//
// Created by 何时夕 on 2020-02-24.
//

#include "flutter_WsVideoEditorUtils.h"
#include <stdint.h>
#include "com_whensunset_wsvideoeditorsdk_WsVideoEditorUtils.h"
#include "ws_editor_video_sdk_utils.h"

using namespace whensunset::wsvideoeditor;
extern "C" {

__attribute__((visibility("default")))
void flutter_com_whensunset_wsvideoeditorsdk_WsVideoEditorUtils_initJniNatives() {
    InitSDK();
}

}
