LOCAL_PATH := $(call my-dir)

# ---------------------------------------------------------------------------
# libsauerquest.so — Phase 1: OpenXR bootstrap only, no Sauerbraten engine
# linkage yet (that arrives in Phase 2+). See the project plan for phases.
# ---------------------------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := sauerquest
LOCAL_LDLIBS := -llog -landroid -lGLESv3 -lEGL -ldl

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/openxr_sdk/include \
    $(LOCAL_PATH)/openxr_sdk/src/common \
    $(LOCAL_PATH)/vr_glue

LOCAL_SRC_FILES := \
    vr_glue/TBXR_Common.c \
    vr_glue/OpenXrInput.c \
    vr_glue/sauerquest_vr_bootstrap.c

LOCAL_CFLAGS := -Wall -Wno-unused-variable -Wno-unused-function

include $(BUILD_SHARED_LIBRARY)
