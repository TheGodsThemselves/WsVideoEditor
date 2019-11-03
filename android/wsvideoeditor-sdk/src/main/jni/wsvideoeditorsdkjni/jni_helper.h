#ifndef ANDROID_WS_VIDEO_EDITOR_JNI_HELPER_H
#define ANDROID_WS_VIDEO_EDITOR_JNI_HELPER_H

#include <jni.h>
#include <google/protobuf/message_lite.h>

namespace whensunset {
    namespace wsvideoeditor {

        namespace editorsdk_jni {
            JNIEnv *GetEnv();

            jbyteArray GetSerializedBytes(JNIEnv *, const ::google::protobuf::MessageLite &);

        }

        class AttachCurrentThreadIfNeeded {
        public:
            AttachCurrentThreadIfNeeded();

            ~AttachCurrentThreadIfNeeded();

            JNIEnv *jni() {
                return jni_;
            };
        private:
            bool attached_;
            JNIEnv *jni_;
        };

        template<class T> // T is jclass, jobject, jintArray, etc
        class GlobalRef {
        public:
            GlobalRef() : obj_(nullptr) {}

            ~GlobalRef() {
                AttachCurrentThreadIfNeeded attached;
                if (obj_) {
                    AttachCurrentThreadIfNeeded attached;
                    attached.jni()->DeleteGlobalRef(obj_);
                }
            }

            T operator()() const {
                return obj_;
            }

            void reset(JNIEnv *env, T obj) {
                if (obj_) {
                    env->DeleteGlobalRef(obj_);
                }
                obj_ = static_cast<T>(env->NewGlobalRef(obj));
            }

        private:
            T obj_;
        };

        template<class T> // T is jclass, jobject, jintArray, etc
        class LocalRef {
        public:
            LocalRef(JNIEnv *env, T obj) : obj_(obj), env_(env) {};

            ~LocalRef() {
                if (obj_ != nullptr) {
                    env_->DeleteLocalRef(obj_);
                }
            }

            T operator()() const {
                return obj_;
            }

        private:
            T obj_;
            JNIEnv *env_;
        };
    }
}
#endif
