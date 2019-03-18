/**
 * Created by alderzhang on 2018/9/20.
 */

#include "jni_helper.h"
#include "utilbase.h"
#include <android/log.h>
#include <assert.h>

JavaVM *JniHelper::sJvm = nullptr;

pthread_key_t JniHelper::sKey;

void OnJniThreadExit(void *tlsData) {
//    JNIEnv* env = (JNIEnv*)tlsData;
    if (nullptr != JniHelper::GetJVM()) {
        JniHelper::GetJVM()->DetachCurrentThread();
    }
}

bool JniHelper::Init(JavaVM *jvm) {
    if (sJvm) {
        return true;
    }
    sJvm = jvm;
    if (0 != pthread_key_create(&sKey, OnJniThreadExit)) {
        __android_log_print(ANDROID_LOG_ERROR, "JniHelper", "create sKey fail");
        return false;
    }
    return true;
}

void JniHelper::UnInit() {
    pthread_key_delete(sKey);
    sJvm = nullptr;
}

JavaVM *JniHelper::GetJVM() {
    return sJvm;
}

bool JniHelper::CheckAndClearException(JNIEnv *env) {
    if(env->ExceptionCheck()){
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}

void JniHelper::DumpReferenceTables(JNIEnv *env) {
    jclass vm_class = env->FindClass("dalvik/system/VMDebug");
    jmethodID dump_mid = env->GetStaticMethodID(vm_class, "dumpReferenceTables", "()V");
    env->CallStaticVoidMethod(vm_class, dump_mid);
    env->DeleteLocalRef(vm_class);
}

ScopedJEnv::ScopedJEnv(jint capacity)
        : env_(nullptr)
{
    assert(JniHelper::sJvm);
    do {
        env_ = (JNIEnv *) pthread_getspecific(JniHelper::sKey);

        if (nullptr != env_) {
            break;
        }

        jint status = JniHelper::sJvm->GetEnv((void **) &env_, JNI_VERSION_1_6);

        if (JNI_OK == status) {
            break;
        }

        JavaVMAttachArgs args;
        args.group = NULL;
        args.name = "default";
        args.version = JNI_VERSION_1_6;
        status = JniHelper::sJvm->AttachCurrentThread(&env_, &args);

        if (JNI_OK == status) {
            pthread_setspecific(JniHelper::sKey, env_);
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "JniHelper",
                                "AttachCurrentThread failed|vm:%p|env:%p|status:%d",
                                JniHelper::sJvm, env_, status);
            env_ = nullptr;
            return;
        }
    } while (false);

//    jint ret = env_->PushLocalFrame(capacity);//创建局部变量引用栈
//    if (0 != ret) {
//        __android_log_print(ANDROID_LOG_ERROR, "JniHelper",
//                            "PushLocalFrame failed|vm:%p|env:%p|ret:%d", JniHelper::sJvm, env_,
//                            ret);
//    }
}

ScopedJEnv::~ScopedJEnv() {
    if (nullptr != env_) {
//        env_->PopLocalFrame(nullptr);//释放局部变量引用栈
    }
}

JNIEnv *ScopedJEnv::GetEnv() const {
    return env_;
}

bool ScopedJEnv::CheckAndClearException() const {
    return JniHelper::CheckAndClearException(env_);
}

ScopedJString::ScopedJString(JNIEnv *env, jstring str)
        : env_(env)
        , jstr_((jstring) env->NewLocalRef(str))
        , char_(nullptr)
        , charLen_(0)
        , jstr2char_(true)
{
    assert(env_);
    if (nullptr == env_ || nullptr == jstr_) {
        return;
    }

    JniHelper::CheckAndClearException(env_);

    char_ = env_->GetStringUTFChars(jstr_, nullptr);
    charLen_ = static_cast<size_t>(env_->GetStringUTFLength(jstr_));
}

ScopedJString::ScopedJString(JNIEnv *env, const char *str, bool keepJstr)
        : ScopedJString(env, str, 0, keepJstr)
{
}

ScopedJString::ScopedJString(JNIEnv *env, const char *str, size_t len, bool keepJstr)
        : env_(env)
        , jstr_(nullptr)
        , char_(str)
        , charLen_(len)
        , jstr2char_(false)
        , keepJstr_(keepJstr)
{
    assert(env_);
    if (nullptr == env_ || nullptr == str) {
        return;
    }

    JniHelper::CheckAndClearException(env_);

    if(charLen_ <= 0){
        charLen_ = strlen(char_);
    }

    jclass strClass = env_->FindClass("java/lang/String");
    jmethodID ctorID = env_->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");

    jbyteArray bytes = env_->NewByteArray((jsize) charLen_);
    env_->SetByteArrayRegion(bytes, 0, (jsize) charLen_, (jbyte *) char_);
    jstring encoding = env_->NewStringUTF("utf-8");

    jstr_ = (jstring) env_->NewObject(strClass, ctorID, bytes, encoding);

    env_->DeleteLocalRef(bytes);
    env_->DeleteLocalRef(encoding);
    env_->DeleteLocalRef(strClass);
}

ScopedJString::~ScopedJString() {
    if (nullptr == env_ || nullptr == jstr_ || nullptr == char_) {
        return;
    }

    JniHelper::CheckAndClearException(env_);

    if (jstr2char_) {
        env_->ReleaseStringUTFChars(jstr_, char_);
    }

    if(!keepJstr_){
        env_->DeleteLocalRef(jstr_);
    }
}

jstring ScopedJString::GetJStr() const {
    if (JniHelper::CheckAndClearException(env_)) {
        return nullptr;
    }

    return jstr_;
}

const char *ScopedJString::GetChar() const {
    if (JniHelper::CheckAndClearException(env_)) {
        return nullptr;
    }

    return char_;
}

size_t ScopedJString::GetCharLength() const {
    return charLen_;
}

ScopedJStringArray::ScopedJStringArray(JNIEnv *jEnv, jsize size)
        : jEnv_(jEnv), jarray_(nullptr) {
    if (jEnv_) {
        jclass jStringClass = jEnv_->FindClass("java/lang/String");
        jarray_ = jEnv_->NewObjectArray(size, jStringClass, nullptr);
    }
}

ScopedJStringArray::~ScopedJStringArray() {
    if (jarray_) {
        jEnv_->DeleteLocalRef(jarray_);
    }
}

bool ScopedJStringArray::SetString(jsize index, const std::string &str) {
    if (index >= jEnv_->GetArrayLength(jarray_)) {
        return false;
    }
    ScopedJString jstr(jEnv_, str.c_str());
    jEnv_->SetObjectArrayElement(jarray_, index, jstr.GetJStr());
    return true;
}

jobjectArray ScopedJStringArray::GetJArray() const {
    return jarray_;
}

ScopedByteArray::ScopedByteArray(JNIEnv *env, jbyteArray array)
        : env_(env), jarray_((jbyteArray) env->NewLocalRef(array)), data_(), jarray2data_(true) {
    assert(env_);
    if (nullptr == env_ || nullptr == jarray_) {
        return;
    }

    JniHelper::CheckAndClearException(env_);

    jboolean isCopy = JNI_FALSE;
    jsize size = env_->GetArrayLength(jarray_);
    jbyte *bytes = env_->GetByteArrayElements(jarray_, &isCopy);
    data_ = std::string((const char *) bytes, (size_t) size);
    env_->ReleaseByteArrayElements(jarray_, bytes, JNI_ABORT);
}

ScopedByteArray::ScopedByteArray(JNIEnv *env, const std::string &data)
        : env_(env), jarray_(nullptr), data_(data), jarray2data_(false) {
    assert(env_);
    if (nullptr == env_) {
        return;
    }

    JniHelper::CheckAndClearException(env_);

    jarray_ = env_->NewByteArray(static_cast<jsize>(data.size()));
    env_->SetByteArrayRegion(jarray_, 0, static_cast<jsize>(data.size()), (jbyte *) data.data());
}

ScopedByteArray::~ScopedByteArray() {
    if (nullptr == env_ || nullptr == jarray_) {
        return;
    }

    JniHelper::CheckAndClearException(env_);

    if (!jarray2data_) {
        env_->DeleteLocalRef(jarray_);
    }
}

jbyteArray ScopedByteArray::GetJArray() const {
    if (JniHelper::CheckAndClearException(env_)) {
        return nullptr;
    }

    return jarray_;
}

const std::string &ScopedByteArray::GetData() const {
//    if (JniHelper::CheckAndClearException(env_)) {
//        return nullptr;
//    }

    return data_;
}


jclass ArrayListJni::jcls_;
std::map<std::string, jmethodID> ArrayListJni::methodIdMap_;
bool ArrayListJni::InitIDs(JNIEnv *env){
    if(jcls_){
        return true;
    }

    jclass cls = env->FindClass("java/util/ArrayList");
    if(cls == nullptr){
        LOGE("JNI Error!! ArrayList class not found");
        return false;
    }
    jcls_ = (jclass)env->NewGlobalRef(cls);

    jmethodID jmethod = nullptr;

    jmethod = env->GetMethodID(jcls_, "<init>", "()V");
    if(jmethod == nullptr){
        LOGE("JNI Error!! ArrayList constructor method not found");
        return false;
    }
    methodIdMap_["constructor"] = jmethod;


    jmethod = env->GetMethodID(cls, "add", "(Ljava/lang/Object;)Z");
    if(jmethod == nullptr){
        LOGE("JNI Error!! ArrayList add method not found");
        return false;
    }
    methodIdMap_["add"] = jmethod;

    jmethod = env->GetMethodID(cls, "get", "(I)Ljava/lang/Object;");
    if(jmethod == nullptr){
        LOGE("JNI Error!! ArrayList get method not found");
        return false;
    }
    methodIdMap_["get"] = jmethod;

    jmethod = env->GetMethodID(cls, "size", "()I");
    if(jmethod == nullptr){
        LOGE("JNI Error!! ArrayList size method not found");
        return false;
    }
    methodIdMap_["size"] = jmethod;

    return true;

}

jobject ArrayListJni::NewArrayList(){
    ScopedJEnv scopedJEnv;
    auto env = scopedJEnv.GetEnv();

    if(!InitIDs(env)){
        LOGE("JNI Error!! ArrayListJni init failed");
        return nullptr;
    }

    jobject listObj = env->NewObject(jcls_, methodIdMap_["constructor"]);
    return listObj;
}

bool ArrayListJni::Add(jobject listObj, jobject obj){

    ScopedJEnv scopedJEnv;
    auto env = scopedJEnv.GetEnv();

    if(!InitIDs(env)){
        LOGE("JNI Error!! ArrayListJni init failed");
        return false;
    }

    if(!listObj || !obj) return false;

    return env->CallBooleanMethod(listObj, methodIdMap_["add"], obj);
}

jobject ArrayListJni::Get(jobject listObj, int i){

    ScopedJEnv scopedJEnv;
    auto env = scopedJEnv.GetEnv();

    if(!InitIDs(env)){
        LOGE("JNI Error!! ArrayListJni init failed");
        return nullptr;
    }

    if(!listObj) return nullptr;

    auto size = env->CallIntMethod(listObj, methodIdMap_["size"]);
    if(i >= size){
        return nullptr;
    }

    return env->CallObjectMethod(listObj, methodIdMap_["get"], i);
}

int ArrayListJni::Size(jobject listObj){

    ScopedJEnv scopedJEnv;
    auto env = scopedJEnv.GetEnv();

    if(!InitIDs(env)){
        LOGE("JNI Error!! ArrayListJni init failed");
        return 0;
    }

    if(!listObj) return 0;

    return env->CallIntMethod(listObj, methodIdMap_["size"]);
}
