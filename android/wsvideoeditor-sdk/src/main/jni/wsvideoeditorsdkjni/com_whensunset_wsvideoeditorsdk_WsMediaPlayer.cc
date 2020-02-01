#include "com_whensunset_wsvideoeditorsdk_WsMediaPlayer.h"
#include "ws_video_editor_sdk_android_jni.pb.h"
#include "native_ws_media_player.h"
#include "ws_editor_video_sdk_utils.h"

using namespace whensunset::wsvideoeditor;

JNIEXPORT jlong JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_newNativePlayer
        (JNIEnv *, jobject) {
    NativeWSMediaPlayer *native_player = new(std::nothrow) NativeWSMediaPlayer;
    return (jlong) native_player;
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_deleteNativePlayer
        (JNIEnv *, jobject, jlong address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->OnDetachedFromController();
    delete native_player;
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_setProjectNative
        (JNIEnv *env, jobject, jlong address, jbyteArray buffer) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    model::EditorProject project;
    jbyte *buffer_elements = env->GetByteArrayElements(buffer, 0);
    project.ParseFromArray(buffer_elements, env->GetArrayLength(buffer));
    env->ReleaseByteArrayElements(buffer, buffer_elements, 0);
    native_player->SetProject(project);
}

JNIEXPORT jbyteArray JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_loadProjectNative
        (JNIEnv *env, jobject, jlong address, jbyteArray buffer, jboolean force_update) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    model::EditorProject *project = new(std::nothrow) model::EditorProject();
    jbyte *buffer_elements = env->GetByteArrayElements(buffer, 0);
    int err = AVERROR(ENOMEM);
    if (project && buffer_elements) {
        project->ParseFromArray(buffer_elements, env->GetArrayLength(buffer));
        env->ReleaseByteArrayElements(buffer, buffer_elements, 0);
        ClearFileHolderIfAssetIdChanged(*project, native_player->project());
        err = LoadProject(project);
        native_player->SetProject(*project);
    }

    model::CreateProjectNativeReturnValue ret;
    ret.set_error_code(err);
    ret.set_allocated_project(project);
    return editorsdk_jni::GetSerializedBytes(env, ret);
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_drawFrameNative
        (JNIEnv *, jobject, jlong address) {
    EGLSurface current_draw_surface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface current_read_surface = eglGetCurrentSurface(EGL_READ);
    EGLContext current_context = eglGetCurrentContext();
    EGLDisplay current_display = eglGetCurrentDisplay();

    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->DrawFrame();

    eglMakeCurrent(current_display, current_draw_surface, current_read_surface, current_context);
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_onAttachedViewNative
        (JNIEnv *, jobject, jlong address, jint width, jint height) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->OnAttachedToController(width, height);
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_onDetachedViewNative
        (JNIEnv *, jobject, jlong address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->OnDetachedFromController();
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_seekNative
        (JNIEnv *, jobject, jlong address, jdouble current_time) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->Seek(current_time);
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_playNative
        (JNIEnv *, jobject, jlong address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->Play();
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_pauseNative
        (JNIEnv *, jobject, jlong address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    native_player->Pause();
}

JNIEXPORT jboolean JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_isPlayingNative
        (JNIEnv *, jobject, jlong address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    return !native_player->paused();
}

JNIEXPORT jdouble JNICALL Java_com_whensunset_wsvideoeditorsdk_WsMediaPlayer_getCurrentTimeNative
        (JNIEnv *, jobject, jlong address) {
    NativeWSMediaPlayer *native_player = reinterpret_cast<NativeWSMediaPlayer *>(address);
    return native_player->current_time();
}
