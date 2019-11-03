#include "android_logger.h"

const static int LOG_BUF_SIZE = 1024;

namespace whensunset {
    namespace wsvideoeditor {
        namespace android_logger {
            static GlobalRef<jclass> j_class_editor_sdk_logger;
            static jmethodID j_method_id;
            static bool is_init = false;

            void Init() {
                JNIEnv *env = editorsdk_jni::GetEnv();
                LocalRef<jclass> clazz{env,
                                       env->FindClass("com/whensunset/wsvideoeditorsdk/WsVideoEditorUtils")};
                j_class_editor_sdk_logger.reset(env, clazz());
                j_method_id = env->GetStaticMethodID(clazz(), "nativeCallDebugLogger",
                                                     "(ILjava/lang/String;Ljava/lang/String;)V");
                is_init = true;
            }

            int LogPrint(int priority, const char *tag, const char *fmt, ...) {
                if (tag == nullptr) {
                    return -1;
                }
                char buf[LOG_BUF_SIZE];
                va_list ap;
                va_start(ap, fmt);
                vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
                va_end(ap);
                if (is_init) {
                    AttachCurrentThreadIfNeeded attached;
                    if (!attached.jni()) {
                        return -1;
                    }
                    LocalRef<jstring> j_tag_ref{attached.jni(), attached.jni()->NewStringUTF(tag)};
                    if (j_tag_ref() == nullptr) {
                        return -1;
                    }
                    LocalRef<jstring> j_message_ref{attached.jni(),
                                                    attached.jni()->NewStringUTF(buf)};
                    if (j_message_ref() == nullptr) {
                        return -1;
                    }
                    attached.jni()->CallStaticVoidMethod(j_class_editor_sdk_logger(), j_method_id,
                                                         priority, j_tag_ref(), j_message_ref());
                }
                return 0;
            }
        }
    }
}
