
# +++++++++++++++++++++++++++++++++++++++Android.mk++++++++++++++++++++++++++
ROOT_PATH := $(call my-dir)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/ \
		$(LOCAL_PATH)/../

LOCAL_CFLAGS := -std=c++11
LOCAL_SHARED_LIBRARIES := usb100
LOCAL_LDLIBS := -llog
LOCAL_SRC_FILES := libuac_jni.cpp \
					jni_helper.cpp \
					../../libuac.cpp 

LOCAL_MODULE	:= uac
include $(BUILD_SHARED_LIBRARY)
