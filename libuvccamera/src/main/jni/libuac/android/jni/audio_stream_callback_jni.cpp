//
// Created by lgon528 on 2019/3/2.
//

#include "utilbase.h"
#include "audio_stream_callback_jni.h"

#include "jni_helper.h"


namespace libuac {


    jclass IAudioStreamCallbackJni::jcls_;
    std::map<std::string, jmethodID> IAudioStreamCallbackJni::methodIdMap_;

    IAudioStreamCallbackJni::IAudioStreamCallbackJni(jobject obj) {
        ScopedJEnv scopedJEnv;
        cbObj_ = scopedJEnv.GetEnv()->NewGlobalRef(obj);
    }

    IAudioStreamCallbackJni::~IAudioStreamCallbackJni() {
        ScopedJEnv scopedJEnv;
        scopedJEnv.GetEnv()->DeleteGlobalRef(cbObj_);
    }

    bool IAudioStreamCallbackJni::initIDs(JNIEnv *env) {

        if(jcls_ != nullptr){
            return true;
        }

        jclass cls = env->FindClass("com/serenegiant/usb/IAudioStreamCallback");
        if(cls == nullptr){
            LOGE("JNI Error!! IAudioStreamCallback class not found");
            return false;
        }
        jcls_ = (jclass)env->NewGlobalRef(cls);

        jmethodID jmethod = nullptr;
        jmethod = env->GetMethodID(jcls_, "onStreaming", "(B)V");
        if(jmethod == nullptr){
            LOGE("JNI Error!! IAudioStreamCallback onStreaming not found");
            return false;
        }
        methodIdMap_["onStreaming"] = jmethod;

        return true;
    }


    void IAudioStreamCallbackJni::onStreaming(Bytes data) {
        ScopedJEnv scopedJEnv;
        auto env = scopedJEnv.GetEnv();

        ScopedByteArray scopedByteArray(env, data);
        env->CallVoidMethod(cbObj_, methodIdMap_["onStreaming"], scopedByteArray.GetJArray());
    }
}