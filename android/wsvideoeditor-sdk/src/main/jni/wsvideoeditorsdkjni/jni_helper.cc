#include "jni_helper.h"
#include "wsvideoeditorsdk/base/platform_logger.h"

JavaVM *javaVM;

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    javaVM = vm;
    whensunset::wsvideoeditor::AttachCurrentThreadIfNeeded attached;
    whensunset::wsvideoeditor::android_logger::Init();
    return JNI_VERSION_1_2;
}

namespace whensunset {
    namespace wsvideoeditor {

        namespace editorsdk_jni {
            jbyteArray
            GetSerializedBytes(JNIEnv *env, const ::google::protobuf::MessageLite &message) {
                int size = message.ByteSize();
                uint8_t *data = new(std::nothrow) uint8_t[size];
                if (!data) {
                    LOGE("OOM in GetSerializedBytes!!!");
                    return nullptr;
                }
                message.SerializeToArray(data, size);

                jbyteArray array = env->NewByteArray(size);
                if (array) {
                    env->SetByteArrayRegion(array, 0, size, (jbyte *) data);
                } else {
                    LOGE("OOM in GetSerializedBytes!!!");
                }
                delete[]data;

                return array;
            }

            JNIEnv *GetEnv() {
                void *env = nullptr;
                javaVM->GetEnv(&env, JNI_VERSION_1_6);
                return reinterpret_cast<JNIEnv *>(env);
            }
        }

        AttachCurrentThreadIfNeeded::AttachCurrentThreadIfNeeded() :
                attached_(false),
                jni_(nullptr) {
            jni_ = editorsdk_jni::GetEnv();
            if (!jni_) {
                jint ret = javaVM->AttachCurrentThread(&jni_, nullptr);
                attached_ = (ret == JNI_OK);
            }
        }

        AttachCurrentThreadIfNeeded::~AttachCurrentThreadIfNeeded() {
            if (attached_) {
                javaVM->DetachCurrentThread();
            }
        }
    }
}
