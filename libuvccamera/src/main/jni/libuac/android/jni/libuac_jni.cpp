#include "jni.h"

#include "libuac.h"
#include "audio_stream_callback_jni.h"
#include "jni_helper.h"
#include "utilbase.h"

using namespace libuac;


/**
 * 初始化 uac
 */
static jint nativeInit(JNIEnv *env, jobject thiz, jstring usbfs) {
    std::string fsStr(ScopedJString(env, usbfs).GetChar());
    return UACContext::getInstance().init(fsStr);
}

static jlong nativeGetDevice(JNIEnv *env, jobject thiz, jint vid, jint pid, jint fd, jint busnum, jint devaddr, jstring sn, jstring usbfs) {
    std::string snStr(ScopedJString(env, sn).GetChar());
    std::string fsStr(ScopedJString(env, usbfs).GetChar());
    return (jlong)(void*)UACContext::getInstance().findDevice(vid, pid, fd, busnum, devaddr, snStr, fsStr).get();
}

static jint nativeOpenDevice(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->open();
}

static jint nativeStartRecord(JNIEnv *env, jobject thiz, jlong devPtr, jstring path) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    std::string pathStr(ScopedJString(env, path).GetChar());
    return device->startRecord(pathStr);
}

static jint nativeStopRecord(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->stopRecord();
}

static void nativeSetAudioStreamCallback(JNIEnv *env, jobject thiz, jlong devPtr, jobject callbackObj) {
    if(!devPtr) {
        LOGE("invalid device");
        return;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    device->setAudioStreamCallback(std::shared_ptr<IAudioStreamCallbackJni>(new IAudioStreamCallbackJni(callbackObj)));
}

static jint nativeCloseDevice(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    return device->close();
}

static jint nativeGetBitResolution(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->getBitResolution();
}


static jint nativeGetChannelCount(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->getChannelCount();
}

static jint nativeGetSampleRate(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->getSampleRate();
}


static jint nativeSetSampleRate(JNIEnv *env, jobject thiz, jlong devPtr, jint sampleRate) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    // todo set sample rate

    return 0;
}

static jstring nativeGetSupportSampleRates(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return nullptr;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    std::string rates = device->getSupportSampleRates();

    ScopedJString scopedStr(env, rates.c_str(), true);

    return scopedStr.GetJStr();
}

static jboolean nativeIsMuteAvailable(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return (jboolean)false;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    return (jboolean)(device->isMuteAvailable());
}

static jboolean nativeIsMute(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return (jboolean)false;;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    return (jboolean)(device->isMute());
}

static jint nativeSetMute(JNIEnv *env, jobject thiz, jlong devPtr, jboolean isMute) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    // todo nativeSetMute

    return 0;
}

static jboolean nativeIsVolumeAvailable(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return (jboolean)false;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    return (jboolean)(device->isVolumeAvailable());
}


static jint nativeGetVolume(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->getVolume();
}


static jint nativeGetMaxVolume(JNIEnv *env, jobject thiz, jlong devPtr) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->getMaxVolume();
}

static jint nativeSetVolume(JNIEnv *env, jobject thiz, jlong devPtr, jint volume) {
    if(!devPtr) {
        LOGE("invalid device");
        return -1;
    }

    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    // todo nativeSetVolume

    return 0;
}

const static std::string gClassName = "com/serenegiant/usb/UACAudio";
const static JNINativeMethod methods[] = {
    // main device operation
    {"nativeInit", "(Ljava/lang/String;)I", (void*) nativeInit},
    {"nativeGetDevice", "(IIIIILjava/lang/String;Ljava/lang/String;)J", (void*)nativeGetDevice},
    {"nativeOpenDevice", "(J)I", (void*)nativeOpenDevice},
    {"nativeCloseDevice", "(J)I", (void*)nativeCloseDevice},
    {"nativeSetAudioStreamCallback", "(JLcom/serenegiant/usb/IAudioStreamCallback;)V", (void*)nativeSetAudioStreamCallback},

    // record
    {"nativeStartRecord", "(JLjava/lang/String;)I", (void*)nativeStartRecord},
    {"nativeStopRecord", "(J)I", (void*)nativeStopRecord},

    // bit resolution
    {"nativeGetBitResolution", "(J)I", (void*)nativeGetBitResolution},

    // channel count
    {"nativeGetChannelCount", "(J)I", (void*)nativeGetChannelCount},

    // sample rate
    {"nativeGetSampleRate", "(J)I", (void*)nativeGetSampleRate},
    {"nativeSetSampleRate", "(JI)I", (void*)nativeSetSampleRate},
    {"nativeGetSupportSampleRates", "(J)Ljava/lang/String;", (void*)nativeGetSupportSampleRates},

    // mute
    {"nativeIsMuteAvailable", "(J)Z", (void*)nativeIsMuteAvailable},
    {"nativeIsMute", "(J)Z", (void*)nativeIsMute},
    {"nativeSetMute", "(JZ)I", (void*)nativeSetMute},

    // volume
    {"nativeIsVolumeAvailable", "(J)Z", (void*)nativeIsVolumeAvailable},
    {"nativeGetVolume", "(J)I", (void*)nativeGetVolume},
    {"nativeGetMaxVolume", "(J)I", (void*)nativeGetMaxVolume},
    {"nativeSetVolume", "(JI)I", (void*)nativeSetVolume},
};





static int registerNativeMethods(JNIEnv *env) {
	int result = 0;

	jclass clazz = env->FindClass(gClassName.c_str());
	if (clazz) {
		int result = env->RegisterNatives(clazz, methods, sizeof(methods)/sizeof(JNINativeMethod));
		if (result < 0) {
			LOGE("registerNativeMethods failed(class=%s)", gClassName.c_str());
		}
	} else {
		LOGE("registerNativeMethods: class'%s' not found", gClassName.c_str());
	}
	return result;
}

static void initIDs(JNIEnv *env) {
    if(!IAudioStreamCallbackJni::initIDs(env)) {
        LOGE("IAudioStreamCallbackJni::initIDs failed");
    }
    if(!ArrayListJni::InitIDs(env)) {
        LOGE("ArrayListJni::initIDs failed");
    }
}



extern "C"
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {

    JniHelper::Init(vm);

    ScopedJEnv scopedJEnv;
    JNIEnv * env = scopedJEnv.GetEnv();

    initIDs(env);
    registerNativeMethods(env);

    return JNI_VERSION_1_2;
}


extern "C"
JNIEXPORT void
JNI_OnUnload(JavaVM *aJvm, void *aReserved) {
}