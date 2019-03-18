/**
 * Created by alderzhang on 2018/9/20.
 */

#ifndef IMSDK_JNI_HELPER_H
#define IMSDK_JNI_HELPER_H

#include <jni.h>
#include <pthread.h>

#include <string>
#include <map>

#define EXPORTED_API

class EXPORTED_API ScopedJEnv {
public:
    ScopedJEnv(jint capacity = 16);

    ~ScopedJEnv();

    JNIEnv *GetEnv() const;

    bool CheckAndClearException() const;

private:
    JNIEnv *env_;
};

class EXPORTED_API ScopedJString {
public:
    ScopedJString(JNIEnv *env, jstring str);

    ScopedJString(JNIEnv *env, const char *str, bool keepJstr = false);

    ScopedJString(JNIEnv *env, const char *str, size_t len, bool keepJstr = false);

    ~ScopedJString();

    jstring GetJStr() const;

    const char *GetChar() const;

    size_t GetCharLength() const;

private:
    JNIEnv *env_;
    jstring jstr_;
    const char *char_;
    size_t charLen_;
    bool jstr2char_;
    bool keepJstr_ = false;
};

class EXPORTED_API ScopedByteArray {
public:
    ScopedByteArray(JNIEnv *env, jbyteArray array);

    ScopedByteArray(JNIEnv *env, const std::string &data);

    ~ScopedByteArray();

    jbyteArray GetJArray() const;

    const std::string &GetData() const;

private:
    JNIEnv *env_;
    jbyteArray jarray_;
    std::string data_;
    bool jarray2data_;
};

class EXPORTED_API ScopedJStringArray {
public:
    ScopedJStringArray(JNIEnv *jEnv, jsize size);

    ~ScopedJStringArray();

    bool SetString(jsize index, const std::string &str);

    jobjectArray GetJArray() const;

private:
    JNIEnv *jEnv_;
    jobjectArray jarray_;
};

class EXPORTED_API JniHelper {
    friend class ScopedJEnv;

public:
    static bool Init(JavaVM *jvm);

    static void UnInit();

    static JavaVM *GetJVM();

    static bool CheckAndClearException(JNIEnv *env);
    static void DumpReferenceTables(JNIEnv *env);

private:
    static JavaVM *sJvm;
    static pthread_key_t sKey;
};


class EXPORTED_API ArrayListJni {
public:
    static bool InitIDs(JNIEnv *env);
    static jobject NewArrayList();
    static bool Add(jobject listObj, jobject obj);
    static jobject Get(jobject listObj, int i);
    static int Size(jobject listObj);

public:
    static jclass jcls_;
    static std::map<std::string, jmethodID> methodIdMap_;
};


#endif //IMSDK_JNI_HELPER_H
