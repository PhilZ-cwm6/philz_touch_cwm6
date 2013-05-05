LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifdef PHILZ_TOUCH_RECOVERY
    #stitch.png
    include $(CLEAR_VARS)
    LOCAL_MODULE := stitch.png
    LOCAL_MODULE_TAGS := optional
    LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
    LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/res/images
    LOCAL_SRC_FILES := $(TARGET_SCREEN_WIDTH)x$(TARGET_SCREEN_HEIGHT)_bg.png
    include $(BUILD_PREBUILT)

    #other files go here

else
    #stitch.png
    include $(CLEAR_VARS)
    LOCAL_MODULE := stitch.png
    LOCAL_MODULE_TAGS := optional
    LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
    LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/res/images
    LOCAL_SRC_FILES := koush.png
    include $(BUILD_PREBUILT)
endif