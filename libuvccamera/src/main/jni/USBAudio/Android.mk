
# +++++++++++++++++++++++++++++++++++++++Android.mk++++++++++++++++++++++++++
ROOT_PATH := $(call my-dir)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/ \
		$(LOCAL_PATH)/../

LOCAL_SHARED_LIBRARIES := usb100
LOCAL_LDLIBS := -llog
LOCAL_SRC_FILES := usbaudio_dump.cpp

LOCAL_MODULE	:= usbaudio
include $(BUILD_SHARED_LIBRARY)
