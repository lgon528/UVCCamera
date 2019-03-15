
# +++++++++++++++++++++++++++++++++++++++Android.mk++++++++++++++++++++++++++
ROOT_PATH := $(call my-dir)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/ \
		$(LOCAL_PATH)/../../ \
		$(LOCAL_PATH)/../../../

LOCAL_CFLAGS := -std=c++11
LOCAL_CFLAGS += -DUSE_LOGALL
LOCAL_CPPFLAGS += -ffunction-sections -fdata-sections
LOCAL_CFLAGS += -ffunction-sections -fdata-sections
LOCAL_CFLAGS += -fno-exceptions -fno-rtti
LOCAL_CPPFLAGS += -fno-exceptions -fno-rtti -fvisibility=hidden
LOCAL_CFLAGS += -Os
LOCAL_LDFLAGS += -Wl,--gc-sections

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := usb100
LOCAL_LDLIBS := -llog

LOCAL_SRC_FILES := libuac_jni.cpp \
					jni_helper.cpp \
					audio_stream_callback_jni.cpp \
					../../libuac.cpp \
					../../utils.cpp

LOCAL_MODULE	:= uac
include $(BUILD_SHARED_LIBRARY)
