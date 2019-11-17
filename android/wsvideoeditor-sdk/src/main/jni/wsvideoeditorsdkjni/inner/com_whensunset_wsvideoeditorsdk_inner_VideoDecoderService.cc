#include "com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService.h"
#include "video_decode_service.h"
#include "ws_editor_video_sdk_utils.h"

using namespace whensunset::wsvideoeditor;
using namespace model;

JNIEXPORT jlong JNICALL Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_newNative
        (JNIEnv *, jobject, jint buffer_capacity) {
    return reinterpret_cast<jlong>(new(std::nothrow)VideoDecodeService(buffer_capacity));
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_releaseNative
        (JNIEnv *, jobject, jlong address) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    delete native_decode_service;
}

JNIEXPORT void JNICALL
Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_setProjectNative
        (JNIEnv *env, jobject, jlong address, jdouble render_pos, jbyteArray buffer) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    model::EditorProject project;
    jbyte *buffer_elements = env->GetByteArrayElements(buffer, 0);
    project.ParseFromArray(buffer_elements, env->GetArrayLength(buffer));
    env->ReleaseByteArrayElements(buffer, buffer_elements, 0);
    LoadProject(&project);
    native_decode_service->SetProject(project, render_pos);
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_startNative
        (JNIEnv *, jobject, jlong address) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    native_decode_service->Start();
}

JNIEXPORT jstring JNICALL
Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_getRenderFrameNative
        (JNIEnv *env, jobject, jlong address, jdouble render_pos) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    DecodedFramesUnit decodedFramesUnit = native_decode_service->GetRenderFrameAtPtsOrNull(
            render_pos);
    std::string toString = decodedFramesUnit.ToString();
    return env->NewStringUTF(toString.c_str());
}

JNIEXPORT void JNICALL
Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_updateProjectNative
        (JNIEnv *env, jobject, jlong address, jbyteArray buffer) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    model::EditorProject project;
    jbyte *buffer_elements = env->GetByteArrayElements(buffer, 0);
    project.ParseFromArray(buffer_elements, env->GetArrayLength(buffer));
    env->ReleaseByteArrayElements(buffer, buffer_elements, 0);
    native_decode_service->UpdateProject(project);
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_seekNative
        (JNIEnv *, jobject, jlong address, jdouble render_pos) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    native_decode_service->Seek(render_pos);
}

JNIEXPORT void JNICALL Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_stopNative
        (JNIEnv *, jobject, jlong address) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    native_decode_service->Stop();
}

JNIEXPORT jboolean JNICALL
Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_endedNative
        (JNIEnv *, jobject, jlong address) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    return static_cast<jboolean>(native_decode_service->ended());
}

JNIEXPORT jboolean JNICALL
Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_stoppedNative
        (JNIEnv *, jobject, jlong address) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    return static_cast<jboolean>(native_decode_service->stopped());
}

JNIEXPORT jint JNICALL
Java_com_whensunset_wsvideoeditorsdk_inner_VideoDecoderService_getBufferedFrameCountNative
        (JNIEnv *, jobject, jlong address) {
    VideoDecodeService *native_decode_service = reinterpret_cast<VideoDecodeService *>(address);
    return native_decode_service->GetBufferedFrameCount();
}
