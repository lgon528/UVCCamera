#ifndef __AUDIO_STREAM_CALLBACK_JNI__
#define __AUDIO_STREAM_CALLBACK_JNI__

#include "libuac.h"
#include "jni.h"
#include <map>
#include <mutex>

namespace libuac {

class IAudioStreamCallbackJni : public IAudioStreamCallback{

    public:
        void onStreaming(Bytes data) override;

        IAudioStreamCallbackJni(jobject obj);
        ~IAudioStreamCallbackJni();
        static bool initIDs(JNIEnv *env);

    private:
        jobject cbObj_;
        std::mutex mutex_;
    public:
        static jclass jcls_;
        static std::map<std::string, jmethodID> methodIdMap_;
};

}



#endif