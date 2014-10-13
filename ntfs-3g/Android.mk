
# libfuse lite
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    libfuse-lite/fuse.c \
    libfuse-lite/fusermount.c \
    libfuse-lite/fuse_kern_chan.c \
    libfuse-lite/fuse_loop.c \
    libfuse-lite/fuse_lowlevel.c \
    libfuse-lite/fuse_opt.c \
    libfuse-lite/fuse_session.c \
    libfuse-lite/fuse_signals.c \
    libfuse-lite/helper.c \
    libfuse-lite/mount.c \
    libfuse-lite/mount_util.c \
    androidglue/statvfs.c

# $(LOCAL_PATH)/.. : do not use hard-coded path (bootable/recovery) to later support different recovery path for variants
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/include/fuse-lite \
    $(LOCAL_PATH)/../ntfs-3g/androidglue/include

LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64  -DHAVE_CONFIG_H
LOCAL_MODULE := libfuse-lite.recovery
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libcutils
include $(BUILD_STATIC_LIBRARY)


# libntfs-3g
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    libntfs-3g/acls.c \
    libntfs-3g/attrib.c \
    libntfs-3g/attrlist.c \
    libntfs-3g/bitmap.c \
    libntfs-3g/bootsect.c \
    libntfs-3g/cache.c \
    libntfs-3g/collate.c \
    libntfs-3g/compat.c \
    libntfs-3g/compress.c \
    libntfs-3g/debug.c \
    libntfs-3g/device.c \
    libntfs-3g/dir.c \
    libntfs-3g/efs.c \
    libntfs-3g/index.c \
    libntfs-3g/inode.c \
    libntfs-3g/lcnalloc.c \
    libntfs-3g/logfile.c \
    libntfs-3g/logging.c \
    libntfs-3g/mft.c \
    libntfs-3g/misc.c \
    libntfs-3g/mst.c \
    libntfs-3g/object_id.c \
    libntfs-3g/reparse.c \
    libntfs-3g/runlist.c \
    libntfs-3g/security.c \
    libntfs-3g/unistr.c \
    libntfs-3g/unix_io.c \
    libntfs-3g/volume.c \
    libntfs-3g/realpath.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/include/fuse-lite \
    $(LOCAL_PATH)/../ntfs-3g/include/ntfs-3g

LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64  -DHAVE_CONFIG_H
LOCAL_MODULE := libntfs-3g.recovery
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libcutils
include $(BUILD_STATIC_LIBRARY)


# ntfs-3g mount binary
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    src/ntfs-3g_main.c \
    src/ntfs-3g.c \
    src/ntfs-3g_common.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/include/fuse-lite \
    $(LOCAL_PATH)/../ntfs-3g/include/ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/androidglue/include \
    $(LOCAL_PATH)/../ntfs-3g/src

LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE := mount.ntfs-3g

# need unique module name, but binary name should be same as in vold
# https://github.com/CyanogenMod/android_system_vold
LOCAL_MODULE_STEM := ntfs-3g
# LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
# LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libfuse-lite.recovery libntfs-3g.recovery
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)


# ntfs-3g mount library
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    src/ntfs-3g.c \
    src/ntfs-3g_common.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/include/fuse-lite \
    $(LOCAL_PATH)/../ntfs-3g/include/ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/androidglue/include \
    $(LOCAL_PATH)/../ntfs-3g/src

LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE := libmount.ntfs-3g
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libfuse-lite.recovery libntfs-3g.recovery
include $(BUILD_STATIC_LIBRARY)


# ntfsprogs - ntfsfix binary
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    ntfsprogs/ntfsfix_main.c \
    ntfsprogs/ntfsfix.c \
    ntfsprogs/utils.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/include/fuse-lite \
    $(LOCAL_PATH)/../ntfs-3g/include/ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/androidglue/include \
    $(LOCAL_PATH)/../ntfs-3g/ntfsprogs

LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE := ntfsfix.recovery

# need unique module name, but binary name should be same as in vold
# https://github.com/CyanogenMod/android_system_vold
LOCAL_MODULE_STEM := ntfsfix
# LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
# LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
#libext2_uuid: external/e2fsprogs/lib/uuid
LOCAL_STATIC_LIBRARIES := libc libext2_uuid libfuse-lite libntfs-3g
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)


# ntfsprogs - ntfsfix library
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    ntfsprogs/ntfsfix.c \
    ntfsprogs/utils.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/include/fuse-lite \
    $(LOCAL_PATH)/../ntfs-3g/include/ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/androidglue/include \
    $(LOCAL_PATH)/../ntfs-3g/ntfsprogs

LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE := libntfsfix.recovery
LOCAL_MODULE_TAGS := eng
#libext2_uuid: external/e2fsprogs/lib/uuid
LOCAL_STATIC_LIBRARIES := libc libext2_uuid libfuse-lite libntfs-3g
include $(BUILD_STATIC_LIBRARY)


# ntfsprogs - mkntfs binary
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    ntfsprogs/mkntfs_main.c \
    ntfsprogs/attrdef.c \
    ntfsprogs/boot.c \
    ntfsprogs/sd.c \
    ntfsprogs/mkntfs.c \
    ntfsprogs/utils.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/include/fuse-lite \
    $(LOCAL_PATH)/../ntfs-3g/include/ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/androidglue/include \
    $(LOCAL_PATH)/../ntfs-3g/ntfsprogs \
    external/e2fsprogs/lib

LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE := mkntfs.recovery

# need unique moduel name, but binary name should be same as in vold
# https://github.com/CyanogenMod/android_system_vold
LOCAL_MODULE_STEM := mkntfs
# LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
# LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libfuse-lite.recovery libntfs-3g.recovery
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)


# ntfsprogs - mkntfs library
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    ntfsprogs/attrdef.c \
    ntfsprogs/boot.c \
    ntfsprogs/sd.c \
    ntfsprogs/mkntfs.c \
    ntfsprogs/utils.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/include/fuse-lite \
    $(LOCAL_PATH)/../ntfs-3g/include/ntfs-3g \
    $(LOCAL_PATH)/../ntfs-3g/androidglue/include \
    $(LOCAL_PATH)/../ntfs-3g/ntfsprogs \
    external/e2fsprogs/lib

LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE := libmkntfs.recovery
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libfuse-lite.recovery libntfs-3g.recovery
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_STATIC_LIBRARY)
