LOCAL_PATH := $(call my-dir)

# device background image
include $(CLEAR_VARS)
ifdef PHILZ_TOUCH_RECOVERY
    #stitch.png
    LOCAL_MODULE := stitch.png
    LOCAL_MODULE_TAGS := optional
    LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
    LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/res/images
    LOCAL_SRC_FILES := bg/$(TARGET_SCREEN_WIDTH)x$(TARGET_SCREEN_HEIGHT)_bg.png

    #other files go here

else
    #stitch.png
    LOCAL_MODULE := stitch.png
    LOCAL_MODULE_TAGS := optional
    LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
    LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/res/images
    LOCAL_SRC_FILES := bg/koush.png
endif
include $(BUILD_PREBUILT)


# device virtual keys image
ifdef PHILZ_TOUCH_RECOVERY
    include $(CLEAR_VARS)
    LOCAL_MODULE := virtual_keys.png
    LOCAL_MODULE_TAGS := optional
    LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
    LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/res/images
    LOCAL_SRC_FILES := vk/virtual_keys_$(TARGET_SCREEN_WIDTH).png
    include $(BUILD_PREBUILT)
endif
