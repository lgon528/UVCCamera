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

static int nativeCloseDevice(JNIEnv *env, jobject thiz, jlong devPtr) {
    UACDevice *device = reinterpret_cast<UACDevice*>(devPtr);

    return device->close();
}

const static std::string gClassName = "com/serenegiant/usb/UACAudio";
const static JNINativeMethod methods[] = {
    {"nativeInit", "(Ljava/lang/String;)I", (void*) nativeInit},
    {"nativeGetDevice", "(IIIIILjava/lang/String;Ljava/lang/String;)J", (void*)nativeGetDevice},
    {"nativeOpenDevice", "(J)I", (void*)nativeOpenDevice},
    {"nativeCloseDevice", "(J)I", (void*)nativeCloseDevice},
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
    if(!IAudioStreamCallbackJni::initIDs(env)) {
        LOGE("IAudioStreamCallbackJni::initIDs failed");
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