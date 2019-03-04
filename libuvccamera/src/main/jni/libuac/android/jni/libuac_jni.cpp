#include "jni.h"

#include "libuac.h"
#include "audio_stream_callback_jni.h"
#include "jni_helper.h"
#include "utilbase.h"

using namespace libuac;


/**
 * 初始化 uac
 */
static jint nativeInit(JNIEnv *env, jobject thiz) {
    return UACContext::getInstance().init();
}

static jlong nativeGetDevice(JNIEnv *env, jobject thiz, jint vid, jint pid, jint fd, jstring sn, jint busnum, jint devaddr) {
    std::string snStr(ScopedJString(env, sn).GetChar());
    return (jlong)(void*)UACContext::getInstance().findDevice(vid, pid, snStr, fd, busnum, devaddr).get();
}

static jint nativeOpenDevice(JNIEnv *env, jobject thiz, jlong devPtr) {
    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->open();
}

static jint nativeStartRecord(JNIEnv *env, jobject thiz, jlong devPtr, jstring path) {
    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    std::string pathStr(ScopedJString(env, path).GetChar());
    return device->startRecord(pathStr);
}

static jint nativeStopRecord(JNIEnv *env, jobject thiz, jlong devPtr) {
    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);
    return device->stopRecord();
}

static void nativeSetAudioStreamCallback(JNIEnv *env, jobject thiz, jlong devPtr, jobject callbackObj) {
    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    device->setAudioStreamCallback(std::shared_ptr<IAudioStreamCallbackJni>(new IAudioStreamCallbackJni(callbackObj)));
}

const static std::string gClassName = "com/serenegiant/usb/UACAudio";
const static JNINativeMethod methods[] = {
    {"nativeInit", "()I", (void*) nativeInit},
    {"nativeGetDevice", "(IIILjava/lang/String;II)J", (void*)nativeGetDevice},
    {"nativeOpenDevice", "(J)I", (void*)nativeOpenDevice},
    {"nativeStartRecord", "(JLjava/lang/String;)I", (void*)nativeStartRecord},
    {"nativeStopRecord", "(J)I", (void*)nativeStopRecord},
    {"nativeSetAudioStreamCallback", "(JLcom/serenegiant/usb/IAudioStreamCallback;)V", (void*)nativeSetAudioStreamCallback}
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
    IAudioStreamCallbackJni::initIDs(env);
}



extern "C"
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {

    JniHelper::Init(vm);

    ScopedJEnv scopedJEnv;
    registerNativeMethods(scopedJEnv.GetEnv());

    return JNI_VERSION_1_2;
}


extern "C"
JNIEXPORT void
JNI_OnUnload(JavaVM *aJvm, void *aReserved) {
}