#include "jni_video_convert.h"
#include "libyuv.h"

#define LOG_TAG "video_convert"

#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##args)

void Java_com_ksy_recordlib_service_util_video_VideoFormatConvertor_imageProcess
(JNIEnv *env, jclass, jbyteArray src, jint src_format, jbyteArray dst, jint dst_format, jint width, jint height,
 jint rotation) 
{
    jbyte *src_buf = env->GetByteArrayElements(src, NULL);
    jbyte *dst_buf = env->GetByteArrayElements(dst, NULL);

    if (src_buf != NULL && dst_buf != NULL) {

        if (src_format == com_ksy_recordlib_service_util_video_VideoFormatConvertor_VIDEO_FORMAT_RGBA && 
                dst_format == com_ksy_recordlib_service_util_video_VideoFormatConvertor_VIDEO_FORMAT_NV21) {

            //AGBR in memory(byte order): R G B A
            libyuv::ABGRToARGB((uint8*)src_buf, width*4, (uint8*)src_buf, width*4, width, height);

            uint8* dst_y = (uint8*)dst_buf;
            uint8* dst_vu = (uint8*)(dst_buf + width*height);
            libyuv::ARGBToNV21((const uint8*)src_buf, width*4, dst_y, width,
                    dst_vu, width, width, height);
        }

        env->ReleaseByteArrayElements(src, src_buf, 0);
        env->ReleaseByteArrayElements(dst, dst_buf, 0);
    }
}
