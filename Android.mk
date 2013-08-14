LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

commands_recovery_local_path := $(LOCAL_PATH)
# LOCAL_CPP_EXTENSION := .c

LOCAL_SRC_FILES := \
    recovery.c \
    bootloader.c \
    install.c \
    roots.c \
    ui.c \
    mounts.c \
    extendedcommands.c \
    advanced_functions.c \
    nandroid.c \
    ../../system/core/toolbox/reboot.c \
    ../../system/core/toolbox/dynarray.c \
    firmware.c \
    edifyscripting.c \
    prop.c \
    default_recovery_ui.c \
    adb_install.c \
    verifier.c

ADDITIONAL_RECOVERY_FILES := $(shell echo $$ADDITIONAL_RECOVERY_FILES)
LOCAL_SRC_FILES += $(ADDITIONAL_RECOVERY_FILES)

LOCAL_MODULE := recovery

LOCAL_FORCE_STATIC_EXECUTABLE := true

ifdef I_AM_KOUSH
RECOVERY_NAME := ClockworkMod Recovery
LOCAL_CFLAGS += -DI_AM_KOUSH
else
ifndef RECOVERY_NAME
RECOVERY_NAME := CWM-based Recovery
endif
endif

CWM_BASE_VERSION := v6.0.3.6
LOCAL_CFLAGS += -DCWM_BASE_VERSION="$(CWM_BASE_VERSION)"
RECOVERY_VERSION := $(RECOVERY_NAME) $(CWM_BASE_VERSION)

LOCAL_CFLAGS += -DRECOVERY_VERSION="$(RECOVERY_VERSION)"
RECOVERY_API_VERSION := 2
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)

#PHILZ_TOUCH_RECOVERY := true
ifdef PHILZ_TOUCH_RECOVERY
LOCAL_CFLAGS += -DPHILZ_TOUCH_RECOVERY
RECOVERY_MOD_NAME := PhilZ Touch
else
ifndef RECOVERY_MOD_NAME
RECOVERY_MOD_NAME := CWM Advanced Edition
endif
endif

RECOVERY_MOD_VERSION := $(RECOVERY_MOD_NAME) 5
PHILZ_BUILD := 5.10.8
LOCAL_CFLAGS += -DRECOVERY_MOD_VERSION="$(RECOVERY_MOD_VERSION)"
LOCAL_CFLAGS += -DPHILZ_BUILD="$(PHILZ_BUILD)"
#compile date:
#LOCAL_CFLAGS += -DBUILD_DATE="\"`date`\""

#debug and calibration logging for touch code
#RECOVERY_TOUCH_DEBUG := true
ifeq ($(RECOVERY_TOUCH_DEBUG), true)
LOCAL_CFLAGS += -DRECOVERY_TOUCH_DEBUG
endif

##############################################################
#device specific config                                      #
#Copyright (C) 2011-2012 sakuramilk <c.sakuramilk@gmail.com> #
#adapted from kbc-developers                                 #
##############################################################

#Galaxy S2 International - i9100
ifeq ($(TARGET_PRODUCT), cm_i9100)
TARGET_COMMON_NAME := i9100
LOCAL_CFLAGS += -DTARGET_DEVICE_I9100

#Galaxy S2 - i9100g
else ifeq ($(TARGET_PRODUCT), cm_i9100g)
TARGET_COMMON_NAME := i9100G
LOCAL_CFLAGS += -DTARGET_DEVICE_I9100G

#Galaxy R / Z - i9103
else ifeq ($(TARGET_PRODUCT), cm_i9103)
TARGET_COMMON_NAME := i9103
LOCAL_CFLAGS += -DTARGET_DEVICE_I9103

#Galaxy S3 International - i9300
else ifeq ($(TARGET_PRODUCT), cm_i9300)
TARGET_COMMON_NAME := i930x
LOCAL_CFLAGS += -DTARGET_DEVICE_I9300

#Galaxy S3 T-Mobile - SGH-T999 (d2tmo)
else ifeq ($(TARGET_PRODUCT), cm_d2tmo)
TARGET_COMMON_NAME := SGH-T999
LOCAL_CFLAGS += -DTARGET_DEVICE_D2TMO

#Galaxy S4 International - i9500
else ifeq ($(TARGET_PRODUCT), cm_i9500)
TARGET_COMMON_NAME := i9500
LOCAL_CFLAGS += -DTARGET_DEVICE_I9500

#Galaxy S4 - i9505, jfltexx
else ifneq ($(filter $(TARGET_PRODUCT),cm_jfltexx cm_jflteatt cm_jfltevzw),)
TARGET_COMMON_NAME := i9505
LOCAL_CFLAGS += -DTARGET_DEVICE_I9505
LOCAL_CFLAGS += -DBOARD_HAS_SLOW_STORAGE
#USE_EXFAT_FUSE_BIN := true

#Galaxy Note - n7000
else ifeq ($(TARGET_PRODUCT), cm_n7000)
TARGET_COMMON_NAME := n7000
LOCAL_CFLAGS += -DTARGET_DEVICE_N7000

#Galaxy Note 2 - n7100
else ifeq ($(TARGET_PRODUCT), cm_n7100)
TARGET_COMMON_NAME := n710x-i317M-T889
LOCAL_CFLAGS += -DTARGET_DEVICE_N7100

#Galaxy Note 10.1 - n8000
else ifeq ($(TARGET_PRODUCT), cm_n8000)
TARGET_COMMON_NAME := n8000
LOCAL_CFLAGS += -DTARGET_DEVICE_N8000

#Galaxy Tab 2 - p3100
else ifeq ($(TARGET_PRODUCT), cm_p3100)
TARGET_COMMON_NAME := p3100
LOCAL_CFLAGS += -DTARGET_DEVICE_P3100

#Galaxy Tab 2 - p5100
else ifeq ($(TARGET_PRODUCT), cm_p5100)
TARGET_COMMON_NAME := p5100
LOCAL_CFLAGS += -DTARGET_DEVICE_P5100

#HTC Desire X - protou
else ifeq ($(TARGET_PRODUCT), cm_protou)
TARGET_COMMON_NAME := HTC_Desire_X
LOCAL_CFLAGS += -DTARGET_DEVICE_HTC_DESIRE_X

#HTC Droid Incredible 4G LTE - fireball
else ifeq ($(TARGET_PRODUCT), cm_fireball)
TARGET_COMMON_NAME := HTC_Droid_Incredible_4G_LTE
LOCAL_CFLAGS += -DTARGET_DEVICE_FIREBALL

#HTC Explorer - pico
else ifeq ($(TARGET_PRODUCT), cm_pico)
TARGET_COMMON_NAME := HTC_Explorer
BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
LOCAL_CFLAGS += -DTARGET_DEVICE_PICO

#HTC One - m7ul / m7spr / m7tmo
else ifneq ($(filter $(TARGET_PRODUCT),cm_m7ul cm_m7spr cm_m7tmo),)
TARGET_COMMON_NAME := HTC_One_$(TARGET_PRODUCT)
LOCAL_CFLAGS += -DTARGET_DEVICE_HTC_ONE

#HTC One X - endeavoru
else ifeq ($(TARGET_PRODUCT), cm_endeavoru)
TARGET_COMMON_NAME := HTC_One_X
LOCAL_CFLAGS += -DTARGET_DEVICE_ENDEAVORU

#HTC One XL - evita
else ifeq ($(TARGET_PRODUCT), cm_evita)
TARGET_COMMON_NAME := HTC_One_XL
LOCAL_CFLAGS += -DTARGET_DEVICE_EVITA

#HTC One S - ville
else ifeq ($(TARGET_PRODUCT), cm_ville)
TARGET_COMMON_NAME := HTC_One_S
LOCAL_CFLAGS += -DTARGET_DEVICE_VILLE

#LGE Nexus 4 - mako
else ifeq ($(TARGET_PRODUCT), cm_mako)
TARGET_COMMON_NAME := Nexus_4
LOCAL_CFLAGS += -DTARGET_DEVICE_MAKO

#ASUS Nexus 7 - tilapia
else ifeq ($(TARGET_PRODUCT), cm_tilapia)
TARGET_COMMON_NAME := ASUS_Nexus_7
LOCAL_CFLAGS += -DTARGET_DEVICE_TILAPIA

#Samsung Nexus 10 - manta
else ifeq ($(TARGET_PRODUCT), cm_manta)
TARGET_COMMON_NAME := Samsung_Nexus_10
LOCAL_CFLAGS += -DTARGET_DEVICE_MANTA

#Samsung Galaxy Nexus - maguro, toro, toroplus (tuna common device)
else ifeq ($(TARGET_PRODUCT), cm_maguro)
TARGET_COMMON_NAME := Samsung_Galaxy_Nexus
LOCAL_CFLAGS += -DTARGET_DEVICE_GALAXY_NEXUS

#Samsung Nexus S - crespo
else ifeq ($(TARGET_PRODUCT), cm_crespo)
TARGET_COMMON_NAME := Samsung_Nexus_S
LOCAL_CFLAGS += -DTARGET_DEVICE_SAMSUNG_NEXUS_S

#Undefined Device
else
TARGET_COMMON_NAME := $(TARGET_PRODUCT)
endif

LOCAL_CFLAGS += -DTARGET_COMMON_NAME="$(TARGET_COMMON_NAME)"

ifdef PHILZ_TOUCH_RECOVERY
ifeq ($(BOARD_USE_CUSTOM_RECOVERY_FONT),)
  BOARD_USE_CUSTOM_RECOVERY_FONT := \"roboto_15x24.h\"
endif
endif

#############################
#end device specific config #
#############################


ifdef BOARD_TOUCH_RECOVERY
ifeq ($(BOARD_USE_CUSTOM_RECOVERY_FONT),)
  BOARD_USE_CUSTOM_RECOVERY_FONT := \"roboto_15x24.h\"
endif
endif

ifeq ($(BOARD_USE_CUSTOM_RECOVERY_FONT),)
  BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
endif

BOARD_RECOVERY_CHAR_WIDTH := $(shell echo $(BOARD_USE_CUSTOM_RECOVERY_FONT) | cut -d _  -f 2 | cut -d . -f 1 | cut -d x -f 1)
BOARD_RECOVERY_CHAR_HEIGHT := $(shell echo $(BOARD_USE_CUSTOM_RECOVERY_FONT) | cut -d _  -f 2 | cut -d . -f 1 | cut -d x -f 2)

LOCAL_CFLAGS += -DBOARD_RECOVERY_CHAR_WIDTH=$(BOARD_RECOVERY_CHAR_WIDTH) -DBOARD_RECOVERY_CHAR_HEIGHT=$(BOARD_RECOVERY_CHAR_HEIGHT)

BOARD_RECOVERY_DEFINES := BOARD_HAS_NO_SELECT_BUTTON BOARD_UMS_LUNFILE BOARD_RECOVERY_ALWAYS_WIPES BOARD_RECOVERY_HANDLES_MOUNT BOARD_TOUCH_RECOVERY RECOVERY_EXTEND_NANDROID_MENU TARGET_USE_CUSTOM_LUN_FILE_PATH TARGET_DEVICE

$(foreach board_define,$(BOARD_RECOVERY_DEFINES), \
  $(if $($(board_define)), \
    $(eval LOCAL_CFLAGS += -D$(board_define)=\"$($(board_define))\") \
  ) \
  )

LOCAL_STATIC_LIBRARIES :=

LOCAL_CFLAGS += -DUSE_EXT4
LOCAL_C_INCLUDES += system/extras/ext4_utils
LOCAL_STATIC_LIBRARIES += libext4_utils_static libz libsparse_static

# This binary is in the recovery ramdisk, which is otherwise a copy of root.
# It gets copied there in config/Makefile.  LOCAL_MODULE_TAGS suppresses
# a (redundant) copy of the binary in /system/bin for user builds.
# TODO: Build the ramdisk image in a more principled way.

LOCAL_MODULE_TAGS := eng

ifeq ($(BOARD_CUSTOM_RECOVERY_KEYMAPPING),)
  LOCAL_SRC_FILES += default_recovery_keys.c
else
  LOCAL_SRC_FILES += $(BOARD_CUSTOM_RECOVERY_KEYMAPPING)
endif

LOCAL_STATIC_LIBRARIES += libmake_ext4fs libext4_utils_static libz libsparse_static
LOCAL_STATIC_LIBRARIES += libminzip libunz libmincrypt

LOCAL_STATIC_LIBRARIES += libminizip libminadbd libedify libbusybox libmkyaffs2image libunyaffs liberase_image libdump_image libflash_image

LOCAL_STATIC_LIBRARIES += libdedupe libcrypto_static libcrecovery libflashutils libmtdutils libmmcutils libbmlutils

ifeq ($(BOARD_USES_BML_OVER_MTD),true)
LOCAL_STATIC_LIBRARIES += libbml_over_mtd
endif

LOCAL_STATIC_LIBRARIES += libminui libpixelflinger_static libpng libcutils
LOCAL_STATIC_LIBRARIES += libstdc++ libc

LOCAL_STATIC_LIBRARIES += libselinux

LOCAL_C_INCLUDES += system/extras/ext4_utils

include $(BUILD_EXECUTABLE)

RECOVERY_LINKS := bu make_ext4fs edify busybox flash_image dump_image mkyaffs2image unyaffs erase_image nandroid reboot volume setprop getprop dedupe minizip

# nc is provided by external/netcat
RECOVERY_SYMLINKS := $(addprefix $(TARGET_RECOVERY_ROOT_OUT)/sbin/,$(RECOVERY_LINKS))
$(RECOVERY_SYMLINKS): RECOVERY_BINARY := $(LOCAL_MODULE)
$(RECOVERY_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Symlink: $@ -> $(RECOVERY_BINARY)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(RECOVERY_BINARY) $@

ALL_DEFAULT_INSTALLED_MODULES += $(RECOVERY_SYMLINKS)

# Now let's do recovery symlinks
BUSYBOX_LINKS := $(shell cat external/busybox/busybox-minimal.links)
exclude := tune2fs mke2fs
RECOVERY_BUSYBOX_SYMLINKS := $(addprefix $(TARGET_RECOVERY_ROOT_OUT)/sbin/,$(filter-out $(exclude),$(notdir $(BUSYBOX_LINKS))))
$(RECOVERY_BUSYBOX_SYMLINKS): BUSYBOX_BINARY := busybox
$(RECOVERY_BUSYBOX_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Symlink: $@ -> $(BUSYBOX_BINARY)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(BUSYBOX_BINARY) $@

ALL_DEFAULT_INSTALLED_MODULES += $(RECOVERY_BUSYBOX_SYMLINKS) 

include $(CLEAR_VARS)
LOCAL_MODULE := nandroid-md5.sh
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := nandroid-md5.sh
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := killrecovery.sh
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := killrecovery.sh
include $(BUILD_PREBUILT)

#philz external scripts
include $(CLEAR_VARS)
LOCAL_MODULE := raw-backup.sh
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := raw-backup.sh
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := ors-mount.sh
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := ors-mount.sh
include $(BUILD_PREBUILT)
#end philz external scripts

include $(CLEAR_VARS)

LOCAL_SRC_FILES := verifier_test.c verifier.c

LOCAL_MODULE := verifier_test

LOCAL_FORCE_STATIC_EXECUTABLE := true

LOCAL_MODULE_TAGS := tests

LOCAL_STATIC_LIBRARIES := libmincrypt libcutils libstdc++ libc

include $(BUILD_EXECUTABLE)

include $(commands_recovery_local_path)/bmlutils/Android.mk
include $(commands_recovery_local_path)/dedupe/Android.mk
include $(commands_recovery_local_path)/flashutils/Android.mk
include $(commands_recovery_local_path)/libcrecovery/Android.mk
include $(commands_recovery_local_path)/minui/Android.mk
include $(commands_recovery_local_path)/minelf/Android.mk
include $(commands_recovery_local_path)/minzip/Android.mk
include $(commands_recovery_local_path)/minadbd/Android.mk
include $(commands_recovery_local_path)/mtdutils/Android.mk
include $(commands_recovery_local_path)/mmcutils/Android.mk
include $(commands_recovery_local_path)/tools/Android.mk
include $(commands_recovery_local_path)/edify/Android.mk
include $(commands_recovery_local_path)/updater/Android.mk
include $(commands_recovery_local_path)/applypatch/Android.mk
include $(commands_recovery_local_path)/utilities/Android.mk
include $(commands_recovery_local_path)/su/Android.mk
include $(commands_recovery_local_path)/pigz/Android.mk
include $(commands_recovery_local_path)/fb2png/Android.mk
include $(commands_recovery_local_path)/device_images/Android.mk
include $(commands_recovery_local_path)/dosfstools/Android.mk

ifneq ($(BOARD_USE_EXFAT_FUSE),false)
include $(commands_recovery_local_path)/fuse/Android.mk \
        $(commands_recovery_local_path)/exfat/Android.mk
endif

ifneq ($(BOARD_USE_NTFS_3G),false)
    include $(commands_recovery_local_path)/ntfs-3g/Android.mk
endif

ifeq ($(NO_AROMA_FILE_MANAGER),)
	include $(commands_recovery_local_path)/aromafm/Android.mk
endif
commands_recovery_local_path :=
