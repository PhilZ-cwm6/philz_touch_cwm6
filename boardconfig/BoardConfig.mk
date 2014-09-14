############################################
# Device Specific Config                   #
# These can go under device BoardConfig.mk #
# By PhilZ for PhilZ Touch recovery        #
############################################
#
# Info on some tags
#   - KERNEL_EXFAT_MODULE_NAME: This will force minivold to use much faster kernel modules instead of slow fuse
#                               you need either an exfat enabled prebuilt kernel or to compile exfat modules along kernel
#                               you also need to patch minivold: https://github.com/PhilZ-cwm6/android_system_vold
#   - KERNEL_NTFS_MODULE_NAME:  Same as above, but for ntfs. Currently, it is only limited write support. Better drop to fuse
#   - BOARD_USE_MTK_LAYOUT :=   true
#                               will enable backup/restore of boot, recovery and uboot partitions on MTK devices
#   - BOARD_MTK_BOOT_LABEL :=   "/bootimg"
#                               This flag is optional, it is used only if BOARD_USE_MTK_LAYOUT is defined
#                               If not defined while previous flag is enabled, default boot label is assumed "/bootimg"
#                               Else, it is assigned the defined value
#                               In fstab.device used by recovery (recovery.fstab), boot mount point must be the same as what we
#                               define in label
#                               It is also mandatory that it is defined to real device label (cat /proc/dumchar_info)
#                               Partition size is grabbed during backup/restore from /proc/dumchar_info
#                               uboot label is defined as "/uboot". It is automatically backed-up/restored with boot and recovery
#                               recovery label is assumed to be "/recovery"
#   - BOARD_HAS_LOW_RESOLUTION: (optional) for all devices 1024x768 resolution.
#                               it forces default touch sensitivity to a lower value. It can be altered in GUI Settings
#   - BOOTLOADER_CMD_ARG:       This will override ro.bootloader.mode. Mostly used for Samsung devices to access download mode
#   - TARGET_COMMON_NAME:       The device name that will be displayed on recovery start and in About dialogue
#   - BRIGHTNESS_SYS_FILE := "path"
#                               Needed to be able to alter screen brightness (touch only)
#                               If not set, recovery will try to auto-detect it on start
#                               If detection succeeds, 'brightness_user_path' key is set to the brightness path in ini file
#                               On next recovery starts, auto-detect is disabled when 'brightness_user_path' key exists
#                               If BRIGHTNESS_SYS_FILE flag is defined during compile, you cannot change brightness path without recompiling recovery
#   - TW_USE_MODEL_HARDWARE_ID_FOR_DEVICE_ID := true
#                               will force using ro.product.model as device id if available
#                               you still need to enable a LOCAL_CFLAGS if defined
#   - BOARD_HAS_SLOW_STORAGE := true
#                               when defined, size progress info during backup/restore will be disabled on default settings
#   - BOARD_HAS_NO_FB2PNG := true
#                               define this to disable fb2png shell support and spare some space (do not use screen capture in adb shell)
#   - BOARD_USE_NTFS_3G := false
#                               will not include ntfs-3g binary to format and mount NTFS partitions. This can spare about 300kb space
#                               devices using NTFS kernel modules will still be able to mount NTFS but not format to NTFS
#
#   - BOARD_RECOVERY_USE_LIBTAR := true
#                               link tar command to recovery libtar (minitar) rather than to busybox tar
#
#   - TARGET_USERIMAGES_USE_F2FS := true
#                               enable f2fs support in recovery, include ext4 <-> f2fs data migration
#                               this is an official CWM flag
#
#   - BOARD_HAS_NO_MULTIUSER_SUPPORT := true
#                               Since Android 4.2, internal storage is located in /data/media/0 (multi user compatibility)
#                               When upgrading to android 4.2, /data/media content is "migrated" to /data/media/0
#                               By default, in recovery, we always use /data/media/0 unless /data/media/.cwm_force_data_media file is found
#                               For devices with pre-4.2 android support, we can define BOARD_HAS_NO_MULTIUSER_SUPPORT flag
#                               It will default to /data/media, unless /data/media/0 exists
#                               In any case, user can toggle the storage path by create/delete the file /data/media/.cwm_force_data_media
#                               This is achieved through the Advanced menu
#
#   - TARGET_USE_CUSTOM_LUN_FILE_PATH := "/sys/class/android_usb/android%d/f_mass_storage/lun/file"
#                               It will add custom lun path support to vold (mount usb storage). For vold support, it must be in main device tree
#                               If recovery has no vold support, it will enable mount usb storage for non vold managed storage
#
#   - BOARD_UMS_LUNFILE := "/sys/class/android_usb/android%d/f_mass_storage/lun/file"
#                               Same as TARGET_USE_CUSTOM_LUN_FILE_PATH except it is not used by vold
#                               You can also define both for non vold managed storage
#
#   - BOARD_CUSTOM_GRAPHICS: this flag is set in device tree board config file. It will cause disabling of gr_save_screenshot() function
#                            on screen capture, recovery will fall to fb2png if it is available
#                            to enable use of built in gr_save_screenshot() instead of fb2png, you must:
#                               * remove "LOCAL_CFLAGS += -DHAS_CUSTOM_GRAPHICS" line from bootable/recovery/minui/Android.mk
#                               * fix any compiler error caused by the custom graphics.c (https://github.com/PhilZ-cwm6/philz_touch_cwm6/commit/4fb941bcb4824b4dd6a812960ed4870ee929da4e#diff-f71f8d16e94c30dfbe7ef8306e4e4428L68)
#


#Amazon Kindle Fire HD 8.9 (jem)
ifeq ($(TARGET_DEVICE), jem)
    TARGET_COMMON_NAME := Kindle Fire HD 8.9
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    RECOVERY_TOUCHSCREEN_SWAP_XY := true
    RECOVERY_TOUCHSCREEN_FLIP_X := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/bowser/brightness"
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/bq27541/capacity"

#Amazon Kindle Fire HD 7 (tate)
else ifeq ($(TARGET_DEVICE), tate)
    TARGET_COMMON_NAME := Kindle Fire HD 7
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/bq27541/capacity"

#Asus Transformer Pad TF300T (tf300t)
else ifeq ($(TARGET_DEVICE), tf300t)
    TARGET_COMMON_NAME := Asus Transformer TF300T
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Asus Transformer Pad Infinity TF700T (tf700t)
else ifeq ($(TARGET_DEVICE), tf700t)
    TARGET_COMMON_NAME := Asus Transformer TF700T
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Asus Memo Pad Smart (me301t)
else ifeq ($(TARGET_DEVICE), me301t)
    TARGET_COMMON_NAME := Asus Memo Pad Smart
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 1280
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Galaxy R / Z (i9103)
else ifeq ($(TARGET_DEVICE), i9103)
    TARGET_COMMON_NAME := i9103
    BOOTLOADER_CMD_ARG := "download"
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Exhibit Variants (codinalte)
else ifeq ($(TARGET_DEVICE), codinalte)
    TARGET_COMMON_NAME := SGH-T599X (codinalte)
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy X Cover 2 - skomer
else ifeq ($(TARGET_DEVICE), skomer)
    TARGET_COMMON_NAME := GT-S7710
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S3 Mini - golden
else ifeq ($(TARGET_DEVICE), golden)
    TARGET_COMMON_NAME := GT-I8190
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Grand Duos (i9082)
else ifeq ($(TARGET_DEVICE), i9082)
    TARGET_COMMON_NAME := Galaxy i9082
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S - i9000 (galaxysmtd)
else ifeq ($(TARGET_DEVICE), galaxysmtd)
    TARGET_COMMON_NAME := Galaxy i9000
    BOARD_USE_NTFS_3G := false
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/s5p_bl/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S Plus - i9001 (ariesve)
else ifeq ($(TARGET_DEVICE), ariesve)
    TARGET_COMMON_NAME := Galaxy i9001
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S Advance - janice
else ifeq ($(TARGET_DEVICE), janice)
    TARGET_COMMON_NAME := GT-I9070
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Ace 2 - codina
else ifeq ($(TARGET_DEVICE), codina)
    TARGET_COMMON_NAME := GT-I8160
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy W I8150 (ancora)
else ifeq ($(TARGET_DEVICE), ancora)
    TARGET_COMMON_NAME := Galaxy W I8150
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S Blaze 4G - SGH T-769
else ifeq ($(TARGET_DEVICE), t769)
    TARGET_COMMON_NAME := SGH-T769
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Grand Quatro (i8552)
else ifeq ($(TARGET_DEVICE), delos3geur)
    TARGET_COMMON_NAME := Galaxy i8552
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true
    
#Galaxy S2 International - i9100
else ifeq ($(TARGET_DEVICE), i9100)
    TARGET_COMMON_NAME := i9100
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S2 Epic 4G Touch - SPH-D710 (d710)
else ifeq ($(TARGET_DEVICE), d710)
    TARGET_COMMON_NAME := SPH-D710
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S2 - i9100g
else ifeq ($(TARGET_DEVICE), i9100g)
    TARGET_COMMON_NAME := i9100G
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note - n7000
else ifeq ($(TARGET_DEVICE), n7000)
    TARGET_COMMON_NAME := n7000
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S2 HD LTE (SGH-I757M) - celoxhd
else ifeq ($(TARGET_DEVICE), celoxhd)
    TARGET_COMMON_NAME := SGH-I757M
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Samsung Exhilarate SGH-I577 (exhilarate)
else ifeq ($(TARGET_DEVICE), exhilarate)
    TARGET_COMMON_NAME := SGH-I577 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Core Duos (i8262)
else ifeq ($(TARGET_DEVICE), i8262)
    TARGET_COMMON_NAME := Galaxy i8262
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S2 Skyrocket i727 - skyrocket
else ifeq ($(TARGET_DEVICE), skyrocket)
    TARGET_COMMON_NAME := Skyrocket i727
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S3 International - i9300 - i9305
else ifneq ($(filter $(TARGET_DEVICE),i9300 i9305),)
    TARGET_COMMON_NAME := Galaxy S3 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Samsung S3 Unified d2lte: d2att d2cri d2mtr d2spr d2tmo d2usc d2vzw
else ifeq ($(TARGET_DEVICE), d2lte)
    TARGET_COMMON_NAME := $(TARGET_DEVICE)
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BOARD_USE_B_SLOT_PROTOCOL := true

# Galaxy S Relay 4G - SGH-T699 (apexqtmo) // Galaxy Express AT&T (expressatt)
# d2-common (d2lte) but with lower resolution
else ifneq ($(filter $(TARGET_DEVICE),apexqtmo expressatt),)
    TARGET_COMMON_NAME := $(TARGET_DEVICE)
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 2 - n7100
else ifeq ($(TARGET_DEVICE), n7100)
    TARGET_COMMON_NAME := Galaxy Note 2
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 2 LTE - t0lte (n7105), t0lteatt (i317 / i317M canada bell), t0ltetmo (T889), l900 (sprint), i605 (verizon)
else ifneq ($(filter $(TARGET_DEVICE),t0lte t0lteatt t0ltetmo l900 i605),)
    TARGET_COMMON_NAME := Note 2 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 3 LTE - N9005 Unified (hlte): hltexx, hltespr, hltetmo, hltecan, hltevzw
else ifeq ($(TARGET_DEVICE), hlte)
    TARGET_COMMON_NAME := Note 3 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 8.0 GSM (n5100), Wifi (n5110) and LTE (n5120)
else ifneq ($(filter $(TARGET_DEVICE),n5100 n5110 n5120),)
    TARGET_COMMON_NAME := Galaxy Note 8.0
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 1280
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    # swap and flip are needed unless we fix things at kernel level
    #RECOVERY_TOUCHSCREEN_SWAP_XY := true
    #RECOVERY_TOUCHSCREEN_FLIP_Y := true
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 10.1 GSM (n8000), Wifi (n8013), LTE (n8020)
else ifneq ($(filter $(TARGET_DEVICE),n8000 n8013 n8020),)
    TARGET_COMMON_NAME := Galaxy Note 10.1 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 1280
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 10.1 2014 LTE (lt03ltexx)
else ifeq ($(TARGET_DEVICE), lt03ltexx)
    TARGET_COMMON_NAME := Note 10.1 2014 LTE
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1600
    TARGET_SCREEN_WIDTH := 2560
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 10.1 2014 Wifi (lt03wifi), 3G (lt03wifiue)
else ifneq ($(filter $(TARGET_DEVICE),lt03wifi lt03wifiue),)
    TARGET_COMMON_NAME := Note 10.1 2014 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1600
    TARGET_SCREEN_WIDTH := 2560
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S4 Exynos - i9500
else ifeq ($(TARGET_DEVICE), i9500)
    TARGET_COMMON_NAME := i9500
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_POST_UNBLANK_COMMAND := "/sbin/postunblankdisplay.sh"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S4 i9505 Qualcomm variants (jflte): jfltecan jflteatt jfltecri jfltecsp jfltespr jfltetmo jflteusc jfltevzw jgedlte jfltexx jfltezm
else ifeq ($(TARGET_DEVICE), jflte)
    TARGET_COMMON_NAME := i9505 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S4 Mini: LTE - i9195 (serranoltexx) // 3G - i9190 (serrano3gxx) // Dual Sim (serranodsub)
else ifneq ($(filter $(TARGET_DEVICE),serranoltexx serrano3gxx serranodsub),)
    TARGET_COMMON_NAME := Galaxy S4 Mini ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S5 SM-G900F Qualcomm variants (klte): kltecan kltespr kltetmo kltevzw kltexx
else ifeq ($(TARGET_DEVICE), klte)
    TARGET_COMMON_NAME := Galaxy S5 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S5 SM-G900H Exynos variant (k3gxx)
else ifeq ($(TARGET_DEVICE), k3gxx)
    TARGET_COMMON_NAME := Galaxy S5 SM-G900H
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Tab 2 - p3100, p3110
else ifneq ($(filter $(TARGET_DEVICE),p3100 p3110),)
    TARGET_COMMON_NAME := Galaxy Tab 2 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 600
    TARGET_SCREEN_WIDTH := 1024
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    #RECOVERY_TOUCHSCREEN_SWAP_XY := true
    #RECOVERY_TOUCHSCREEN_FLIP_Y := true
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Tab 2 - p5100 / p5110
else ifneq ($(filter $(TARGET_DEVICE),p5100 p5110),)
    TARGET_COMMON_NAME := Galaxy Tab 2 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 1280
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Premier I9260 - superior
else ifeq ($(TARGET_DEVICE), superior)
    TARGET_COMMON_NAME := Galaxy Premier I9260
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Mega I9205 (meliusltexx) and i9200 (melius3gxx)
else ifneq ($(filter $(TARGET_DEVICE),meliusltexx melius3gxx),)
    TARGET_COMMON_NAME := Galaxy Mega ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/lcd/panel/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Tab 3 7.0: SM-T210 (lt02wifi), SM-T211 (lt023g)
else ifneq ($(filter $(TARGET_DEVICE),lt02wifi lt023g),)
    TARGET_COMMON_NAME := Galaxy Tab 3 7.0 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 600
    TARGET_SCREEN_WIDTH := 1024
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Tab 3 8.0: SM-T310 (lt01wifi), SM-T311 (lt013g), SM-T315 (lt01lte)
else ifneq ($(filter $(TARGET_DEVICE),lt01wifi lt013g lt01lte),)
    TARGET_COMMON_NAME := Galaxy Tab 3 8.0 ($(TARGET_DEVICE))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Tab Pro 8.4 WiFi SM-T320 (mondrianwifi)
else ifeq ($(TARGET_DEVICE), mondrianwifi)
    TARGET_COMMON_NAME := Galaxy Tab Pro SM-T320
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1600
    TARGET_SCREEN_WIDTH := 2560
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Google Galaxy Nexus (Samsung) - maguro, toro, toroplus (tuna common device)
else ifneq ($(filter $(TARGET_DEVICE),maguro toro toroplus),)
    TARGET_COMMON_NAME := Galaxy Nexus ($(TARGET_DEVICE))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/s6e8aa0/brightness"

#Google Nexus S (Samsung) - crespo / crespo4g
else ifneq ($(filter $(TARGET_DEVICE),crespo crespo4g),)
    TARGET_COMMON_NAME := Nexus S
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/s5p_bl/brightness"

#Google Nexus 4 (LGE) - mako
else ifeq ($(TARGET_DEVICE), mako)
    TARGET_COMMON_NAME := Nexus 4
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 768
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Google Nexus 5 (LGE) - hammerhead
else ifeq ($(TARGET_DEVICE), hammerhead)
    TARGET_COMMON_NAME := Nexus 5
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Google Nexus 7 (ASUS) - tilapia (3G) and grouper (wifi)
else ifneq ($(filter $(TARGET_DEVICE),tilapia grouper),)
    TARGET_COMMON_NAME := Nexus 7 ($(TARGET_DEVICE))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Google Nexus 7 (ASUS) 2013 (flo), LTE (deb)
else ifneq ($(filter $(TARGET_DEVICE),flo deb),)
    TARGET_COMMON_NAME := Nexus 7 (2013 $(TARGET_DEVICE))
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1200
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Google Nexus 10 (Samsung) - manta
else ifeq ($(TARGET_DEVICE), manta)
    TARGET_COMMON_NAME := Nexus 10
    TARGET_SCREEN_HEIGHT := 1600
    TARGET_SCREEN_WIDTH := 2560
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight.0/brightness"
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/android-battery/capacity"

#HP Touchpad (tenderloin)
else ifeq ($(TARGET_DEVICE), tenderloin)
    TARGET_COMMON_NAME := HP Touchpad
    TARGET_SCREEN_HEIGHT := 768
    TARGET_SCREEN_WIDTH := 1024
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Desire X - protou (No 10.2 tree)
else ifeq ($(TARGET_DEVICE), protou)
    TARGET_COMMON_NAME := HTC Desire X
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Droid Incredible 4G LTE - fireball
else ifeq ($(TARGET_DEVICE), fireball)
    TARGET_COMMON_NAME := Droid Incredible 4G LTE
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Explorer - pico (no cm tree)
else ifeq ($(TARGET_DEVICE), pico)
    TARGET_COMMON_NAME := HTC Explorer
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One - m7 (m7ul, m7tmo, m7att) / m7spr / m7vzw
else ifneq ($(filter $(TARGET_DEVICE),m7 m7spr m7vzw),)
    TARGET_COMMON_NAME := HTC One ($(TARGET_DEVICE))
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Droid DNA (dlx)
else ifeq ($(TARGET_DEVICE), dlx)
    TARGET_COMMON_NAME := HTC Droid DNA (dlx)
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Desire 601 (zara)
else ifeq ($(TARGET_DEVICE), zara)
    TARGET_COMMON_NAME := HTC Desire 601 (zara)
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One X (endeavoru) - HTC One X+ (enrc2b)
else ifneq ($(filter $(TARGET_DEVICE),endeavoru enrc2b),)
    TARGET_COMMON_NAME := HTC One X
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/tegra-pwm-bl/brightness"

#HTC One XL - evita
else ifeq ($(TARGET_DEVICE), evita)
    TARGET_COMMON_NAME := HTC One XL
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One S - ville
else ifeq ($(TARGET_DEVICE), ville)
    TARGET_COMMON_NAME := HTC One S
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One V - primou (no cm device tree)
else ifeq ($(TARGET_DEVICE), primou)
    TARGET_COMMON_NAME := HTC One V
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Evo 4G LTE (jewel)
else ifeq ($(TARGET_DEVICE), jewel)
    TARGET_COMMON_NAME := Evo 4G LTE
    KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Rezound (vigor)
else ifeq ($(TARGET_DEVICE), vigor)
    TARGET_COMMON_NAME := HTC Rezound
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One M8 (m8)
else ifneq ($(filter $(TARGET_DEVICE),m8 m8spr m8vzw m8att),)
    TARGET_COMMON_NAME := HTC One M8 ($(TARGET_DEVICE))
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One Mini (m4)
else ifeq ($(TARGET_DEVICE), m4)
    TARGET_COMMON_NAME := HTC One Mini
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Wildfire S (marvel) - armv6
else ifeq ($(TARGET_DEVICE), marvel)
    TARGET_COMMON_NAME := HTC Wildfire S
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_7x16.h\"
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Huawei Acsend P1 U9200 - viva (no cm tree)
else ifeq ($(TARGET_DEVICE), viva)
    TARGET_COMMON_NAME := Huawei_Acsend_P1_U9200
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd/brightness"

#LG Optimus One P500 (p500) - armv6
else ifeq ($(TARGET_DEVICE), p500)
    TARGET_COMMON_NAME := Optimus One P500
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus G ATT (e970) - Canada (e973) - Sprint (ls970) - Intl (e975)
else ifneq ($(filter $(TARGET_DEVICE),e970 e973 ls970 e975),)
    TARGET_COMMON_NAME := Optimus G ($(TARGET_DEVICE))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 768
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus G Pro (GSM) - e980
else ifeq ($(TARGET_DEVICE), e980)
    TARGET_COMMON_NAME := Optimus G Pro
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Spectrum 4G - vs920
else ifeq ($(TARGET_DEVICE), vs920)
    TARGET_COMMON_NAME := Spectrum 4G
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Nitro HD AT&T - p930
else ifeq ($(TARGET_DEVICE), p930)
    TARGET_COMMON_NAME := LG Nitro HD
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG G2 AT&T (d800) - G2 TMO (d801) - G2 Int (d802) - G2 CAN (d803) - d805 - d806 - G2 Verizon (vs980) - G2 Sprint (ls980)
else ifneq ($(filter $(TARGET_DEVICE),d800 d801 d802 d803 d805 d806 vs980 ls980),)
    TARGET_COMMON_NAME := LG G2 ($(TARGET_DEVICE))
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG G Pad 8.3 (v500)
else ifeq ($(TARGET_DEVICE), v500)
    TARGET_COMMON_NAME := LG G Pad 8.3
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1200
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus 4X HD P880 (p880)
else ifeq ($(TARGET_DEVICE), p880)
    TARGET_COMMON_NAME := Optimus 4X HD P880
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus L5 E610 (e610)
else ifeq ($(TARGET_DEVICE), e610)
    TARGET_COMMON_NAME := Optimus L5 E610
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus L7 P700 (p700)
else ifeq ($(TARGET_DEVICE), p700)
    TARGET_COMMON_NAME := Optimus L7 P700
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus L7 P705 (p705)
else ifeq ($(TARGET_DEVICE), p705)
    TARGET_COMMON_NAME := Optimus L7 P705
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Micromax A57 - a57 (no cm tree)
else ifeq ($(TARGET_DEVICE), a57)
    TARGET_COMMON_NAME := Micromax_A57
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Motorola Droid RAZR M - xt907
else ifeq ($(TARGET_DEVICE), xt907)
    TARGET_COMMON_NAME := Droid RAZR M
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola Droid RAZR HD GSM (xt925) and US (xt926)
else ifneq ($(filter $(TARGET_DEVICE),xt925 xt926),)
    TARGET_COMMON_NAME := Droid RAZR HD ($(TARGET_DEVICE))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola Atrix HD
else ifeq ($(TARGET_DEVICE), mb886)
    TARGET_COMMON_NAME := Atrix HD
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola - unified moto_msm8960 (mb886, xt925, xt926, xt901, xt905, xt907)
else ifeq ($(TARGET_DEVICE), moto_msm8960)
    TARGET_COMMON_NAME := Droid msm8960
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola Moto X: unified moto_msm8960dt [TMO (xt1053), US Cellular (xt1055), Sprint (xt1056), GSM (xt1058), VZW (xt1060), VZW Droid Maxx-Dev Edition (xt1080)]
else ifneq ($(filter $(TARGET_DEVICE),moto_msm8960dt),)
    TARGET_COMMON_NAME := Moto X ($(TARGET_DEVICE))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola Moto G Unified (falcon): Verizon (xt1028), Boostmobile (xt1031), GSM (xt1032), Dual Sim (xt1033), Retail US (xt1034), Google Play Edition (falcon_gpe)
else ifeq ($(TARGET_DEVICE), falcon)
    TARGET_COMMON_NAME := Moto G (falcon)
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Oneplus bacon, A0001 (One Plus One)
else ifeq ($(TARGET_DEVICE), bacon)
    TARGET_COMMON_NAME := One Plus One
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Oppo Find5 (find5)
else ifeq ($(TARGET_DEVICE), find5)
    TARGET_COMMON_NAME := Oppo Find5
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Oppo N1 (n1)
else ifeq ($(TARGET_DEVICE), n1)
    TARGET_COMMON_NAME := Oppo N1
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Oppo Find7,Find7a,X9007,X9006 (find7)
else ifeq ($(TARGET_DEVICE), find7)
    TARGET_COMMON_NAME := Oppo Find7
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Pantech Burst (presto)
else ifeq ($(TARGET_DEVICE), presto)
    TARGET_COMMON_NAME := Pantech Burst P9070
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/batterys/capacity"

#Sony Xperia M (nicki)
else ifeq ($(TARGET_DEVICE), nicki)
    TARGET_COMMON_NAME := Xperia M
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 854
    TARGET_SCREEN_WIDTH := 480
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Sony Xperia Z (yuga)
else ifeq ($(TARGET_DEVICE), yuga)
    TARGET_COMMON_NAME := Xperia Z
    KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lm3533-lcd-bl/brightness"

#Sony Xperia Z1 (honami)
else ifeq ($(TARGET_DEVICE), honami)
    TARGET_COMMON_NAME := Xperia Z1
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#Sony Xperia ZU (togari)
else ifeq ($(TARGET_DEVICE), togari)
    TARGET_COMMON_NAME := Xperia ZU
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#Sony Xperia S (nozomi)
else ifeq ($(TARGET_DEVICE), nozomi)
    TARGET_COMMON_NAME := Xperia S
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Sony Xperia SP (huashan)
else ifeq ($(TARGET_DEVICE), huashan)
    TARGET_COMMON_NAME := Xperia SP
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight1/brightness"

#Sony Xperia T (mint)
else ifeq ($(TARGET_DEVICE), mint)
    TARGET_COMMON_NAME := Xperia T
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight_1/brightness"

#Sony Xperia ZL (odin)
else ifeq ($(TARGET_DEVICE), odin)
    TARGET_COMMON_NAME := Xperia ZL
    KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lm3533-lcd-bl-1/brightness"

#Sony Xperia ZR (dogo)
else ifeq ($(TARGET_DEVICE), dogo)
    TARGET_COMMON_NAME := Xperia ZR
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lm3533-lcd-bl/brightness"

#Sony Xperia Tablet Z (pollux)
else ifeq ($(TARGET_DEVICE), pollux)
    TARGET_COMMON_NAME := Xperia Tablet Z
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    BRIGHTNESS_SYS_FILE := "/sys/devices/i2c-0/0-002c/backlight/lcd-backlight/brightness"

#Sony Xperia Z1 Compact (amami)
else ifeq ($(TARGET_DEVICE), amami)
    TARGET_COMMON_NAME := Xperia Z1 Compact
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#ZTE Warp Sequent - N861 (warp2)
else ifeq ($(TARGET_DEVICE), warp2)
    TARGET_COMMON_NAME := ZTE Warp Sequent - N861
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#ZTE Awe (nex)
else ifeq ($(TARGET_DEVICE), nex)
    TARGET_COMMON_NAME := ZTE Awe (nex)
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

endif
#---- end device specific config


# use libtar for backup/restore instead of busybox
# BOARD_RECOVERY_USE_LIBTAR := true

# The below flags must always be defined as default in BoardConfig.mk, unless defined above:
# device name to display in About dialog
ifndef TARGET_COMMON_NAME
    TARGET_COMMON_NAME := $(TARGET_DEVICE)
endif

LOCAL_CFLAGS += -DTARGET_COMMON_NAME="$(TARGET_COMMON_NAME)"

ifdef PHILZ_TOUCH_RECOVERY
    # Battery level default path (PhilZ Touch Only)
    ifndef BATTERY_LEVEL_PATH
        BATTERY_LEVEL_PATH := "/sys/class/power_supply/battery/capacity"
    endif

    ifndef BRIGHTNESS_SYS_FILE
        BRIGHTNESS_SYS_FILE := ""
    endif

    ifndef TARGET_SCREEN_HEIGHT
        $(warning ************************************************************)
        $(warning * TARGET_SCREEN_HEIGHT is NOT SET in BoardConfig.mk )
        $(warning ************************************************************)
        $(error stopping)
    endif

    ifndef TARGET_SCREEN_WIDTH
        $(warning ************************************************************)
        $(warning * TARGET_SCREEN_WIDTH is NOT SET in BoardConfig.mk )
        $(warning ************************************************************)
        $(error stopping)
    endif
endif