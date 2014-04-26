LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libtar_recovery
LOCAL_MODULE_TAGS = optional
LOCAL_SRC_FILES := lib/append.c \
			lib/block.c \
			lib/decode.c \
			lib/encode.c \
			lib/extract.c \
			lib/handle.c \
			listhash/libtar_hash.c \
			listhash/libtar_list.c \
			lib/output.c \
			lib/util.c \
			lib/wrapper.c \
			compat/strlcpy.c \
			compat/basename.c \
			compat/dirname.c \
			compat/strmode.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/lib \
    $(LOCAL_PATH)/compat \
    $(LOCAL_PATH)/listhash

LOCAL_CFLAGS += -DHAVE_SELINUX

include $(BUILD_STATIC_LIBRARY)

# minitar recovery binary
include $(CLEAR_VARS)

LOCAL_MODULE := tar
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_TAGS = eng
LOCAL_SRC_FILES := minitar/minitar.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/lib \
    $(LOCAL_PATH)/compat \
    $(LOCAL_PATH)/listhash \
    external/zlib

LOCAL_STATIC_LIBRARIES := libc libtar_recovery libz libselinux
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin

LOCAL_FORCE_STATIC_EXECUTABLE := true

include $(BUILD_EXECUTABLE)
