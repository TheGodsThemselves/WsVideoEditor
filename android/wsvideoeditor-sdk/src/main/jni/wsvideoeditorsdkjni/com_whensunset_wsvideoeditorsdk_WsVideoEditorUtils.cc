#include "com_whensunset_wsvideoeditorsdk_WsVideoEditorUtils.h"

#include "ws_editor_video_sdk_utils.h"

using namespace whensunset::wsvideoeditor;

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsVideoEditorUtils_initJniNative
        (JNIEnv *, jclass) {
    InitSDK();
}
