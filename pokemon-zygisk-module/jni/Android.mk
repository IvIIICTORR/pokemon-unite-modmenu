LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE    := zygisk_loader
LOCAL_SRC_FILES := loader.cpp
LOCAL_CPPFLAGS  := -std=c++17 -fvisibility=hidden -fexceptions
LOCAL_LDLIBS    := -llog -landroid
LOCAL_LDFLAGS   := -Wl,--gc-sections,--strip-all

include $(BUILD_SHARED_LIBRARY)
