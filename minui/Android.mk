LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := events.c resources.c graphics_utils.c
ifneq ($(BOARD_CUSTOM_GRAPHICS),)
  LOCAL_SRC_FILES += $(BOARD_CUSTOM_GRAPHICS)
  LOCAL_CFLAGS += -DHAS_CUSTOM_GRAPHICS
else
  LOCAL_SRC_FILES += graphics.c graphics_overlay.c
endif

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib

ifeq ($(TARGET_PREBUILT_KERNEL),)
ifeq ($(call is-vendor-board-platform,QCOM),true)
  LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
  LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
endif

ifeq ($(TARGET_USES_QCOM_BSP), true)
    LOCAL_CFLAGS += -DMSM_BSP
endif
endif

LOCAL_MODULE := libminui

# This used to compare against values in double-quotes (which are just
# ordinary characters in this context).  Strip double-quotes from the
# value so that either will work.

ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),RGBX_8888)
  LOCAL_CFLAGS += -DRECOVERY_RGBX
endif
ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),BGRA_8888)
  LOCAL_CFLAGS += -DRECOVERY_BGRA
endif

ifneq ($(TARGET_RECOVERY_OVERSCAN_PERCENT),)
  LOCAL_CFLAGS += -DOVERSCAN_PERCENT=$(TARGET_RECOVERY_OVERSCAN_PERCENT)
else
  LOCAL_CFLAGS += -DOVERSCAN_PERCENT=0
endif

ifneq ($(BOARD_USE_CUSTOM_RECOVERY_FONT),)
  LOCAL_CFLAGS += -DBOARD_USE_CUSTOM_RECOVERY_FONT=$(BOARD_USE_CUSTOM_RECOVERY_FONT)
endif

ifneq ($(TARGET_RECOVERY_LCD_BACKLIGHT_PATH),)
  LOCAL_CFLAGS += -DRECOVERY_LCD_BACKLIGHT_PATH=$(TARGET_RECOVERY_LCD_BACKLIGHT_PATH)
endif

# Some devices need kernel headers for graphics
ifeq ($(TARGET_PREBUILT_KERNEL),)
  LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
  LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
endif

include $(BUILD_STATIC_LIBRARY)
