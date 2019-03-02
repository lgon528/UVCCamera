#include "jni.h"

#include "libuac.h"




/**
 * 初始化 uac
 */
static jint nativeInit(JNIEnv *env, jobject thiz) {
    return UACContext::getInstance().init();
}

static jlong nativeGetDevice(JNIEnv *env, jobject thiz, jint vid, jint pid, jint fd, jstring sn) {
    std::string sn_str(ScopedJString(env, fs).GetChar());
    return UACContext::getInstance().findDevice(vid, pid, sn, fd).get();
}

static jint nativeOpenDevice(JNIEnv *env, jobject thiz, jlong devicePtr) {
    UACDevice *device = reinterpret_cast<UACDevice*>(devicePtr);

}


const static std::string gClassName = ""; // todo package/class name
const static JNINativeMethod methods[] = {
    {"nativeInit", "()I", (void*) nativeInit},
    {"nativeGetDevice", "(IIILjava/lang/String;)J", (void*)nativeGetDevice},
    {"nativeOpenDevice", "(J)I", (void*)nativeOpenDevice}
};

jint registerNativeMethods(JNIEnv* env) {
	int result = 0;

	jclass clazz = env->FindClass(class_name);
	if (LIKELY(clazz)) {
		int result = env->RegisterNatives(clazz, methods, num_methods);
		if (UNLIKELY(result < 0)) {
			LOGE("registerNativeMethods failed(class=%s)", class_name);
		}
	} else {
		LOGE("registerNativeMethods: class'%s' not found", class_name);
	}
	return result;
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