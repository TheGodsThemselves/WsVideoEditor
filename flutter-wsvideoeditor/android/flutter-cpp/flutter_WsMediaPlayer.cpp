//
// Created by 何时夕 on 2020-02-24.
//

#include "com_whensunset_wsvideoeditorsdk_WsMediaPlayer.h"
#include "ws_video_editor_sdk_android_jni.pb.h"
#include "native_ws_media_player.h"
#include "ws_editor_video_sdk_utils.h"

using namespace whensunset::wsvideoeditor;
extern "C" {

__attribute__((visibility("default")))
int64_t flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_newNativePlayer() {
    NativeWSMediaPlayer *native_player = new(std::nothrow) NativeWSMediaPlayer;
    return (int64_t) native_player;
}

__attribute__((visibility("default")))
void flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_deleteNativePlayer(int64_t address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->OnDetachedFromController();
    delete native_player;
}

__attribute__((visibility("default")))
void flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_setProjectNative
        (int64_t address, jbyteArray buffer) {
//    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
//    model::EditorProject project;
//    jbyte *buffer_elements = env->GetByteArrayElements(buffer, 0);
//    project.ParseFromArray(buffer_elements, env->GetArrayLength(buffer));
//    env->ReleaseByteArrayElements(buffer, buffer_elements, 0);
//    native_player->SetProject(project);
}

__attribute__((visibility("default")))
jbyteArray flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_loadProjectNative
        (JNIEnv *env, jobject, int64_t address, jbyteArray buffer, jboolean force_update) {
//    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
//    model::EditorProject *project = new(std::nothrow) model::EditorProject();
//    jbyte *buffer_elements = env->GetByteArrayElements(buffer, 0);
//    int err = AVERROR(ENOMEM);
//    if (project && buffer_elements) {
//        project->ParseFromArray(buffer_elements, env->GetArrayLength(buffer));
//        env->ReleaseByteArrayElements(buffer, buffer_elements, 0);
//        ClearFileHolderIfAssetIdChanged(*project, native_player->project());
//        err = LoadProject(project);
//        native_player->SetProject(*project);
//    }
//
//    model::CreateProjectNativeReturnValue ret;
//    ret.set_error_code(err);
//    ret.set_allocated_project(project);
//    return editorsdk_jni::GetSerializedBytes(env, ret);
    return NULL;
}

__attribute__((visibility("default")))
void flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_drawFrameNative(int64_t address) {
    EGLSurface current_draw_surface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface current_read_surface = eglGetCurrentSurface(EGL_READ);
    EGLContext current_context = eglGetCurrentContext();
    EGLDisplay current_display = eglGetCurrentDisplay();

    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->DrawFrame();

    eglMakeCurrent(current_display, current_draw_surface, current_read_surface, current_context);
}

__attribute__((visibility("default")))
void
flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_onAttachedViewNative(int64_t address,
                                                                           int width,
                                                                           int height) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->OnAttachedToController(width, height);
}

__attribute__((visibility("default")))
void flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_onDetachedViewNative(int64_t address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->OnDetachedFromController();
}

__attribute__((visibility("default")))
void flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_seekNative(int64_t address,
                                                                      double current_time) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->Seek(current_time);
}

__attribute__((visibility("default")))
void flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_playNative(int64_t address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->Play();
}

__attribute__((visibility("default")))
void flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_pauseNative(int64_t address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->Pause();
}

__attribute__((visibility("default")))
jboolean flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_isPlayingNative(int64_t address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    return !native_player->paused();
}

__attribute__((visibility("default")))
jdouble
flutter_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_getCurrentTimeNative(int64_t address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    return native_player->current_time();
}

}
