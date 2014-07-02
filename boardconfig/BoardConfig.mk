############################################
# Device Specific Config                   #
# These can go under device BoardConfig.mk #
# By PhilZ for PhilZ Touch recovery        #
############################################
#
# Info on some tags
#   - KERNEL_EXFAT_MODULE_NAME: This will force minivold to use much faster kernel modules instead of slow fuse
#                               it will only work if you have modified vold sources (contact me for info)
#                               you'll also have to copy modules to ramdisk and load them in init.rc or a loader script
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
#                               define this to disable fb2png support and spare some space (do not use screen capture)
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

#Amazon Kindle Fire HD 8.9 (jem)
ifeq ($(TARGET_PRODUCT), cm_jem)
    TARGET_COMMON_NAME := Kindle Fire HD 8.9
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    RECOVERY_TOUCHSCREEN_SWAP_XY := true
    RECOVERY_TOUCHSCREEN_FLIP_X := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/bowser/brightness"
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/bq27541/capacity"

#Amazon Kindle Fire HD 7 (tate)
else ifeq ($(TARGET_PRODUCT), cm_tate)
    TARGET_COMMON_NAME := Kindle Fire HD 7
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/bq27541/capacity"

#Asus Transformer Pad TF300T (tf300t)
else ifeq ($(TARGET_PRODUCT), cm_tf300t)
    TARGET_COMMON_NAME := Asus Transformer TF300T
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Asus Transformer Pad Infinity TF700T (tf700t)
else ifeq ($(TARGET_PRODUCT), cm_tf700t)
    TARGET_COMMON_NAME := Asus Transformer TF700T
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Galaxy R / Z (i9103)
else ifeq ($(TARGET_PRODUCT), cm_i9103)
    TARGET_COMMON_NAME := i9103
    BOOTLOADER_CMD_ARG := "download"
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Exhibit Variants (codinalte)
else ifeq ($(TARGET_PRODUCT), cm_codinalte)
    TARGET_COMMON_NAME := SGH-T599X (codinalte)
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy X Cover 2 - skomer
else ifeq ($(TARGET_PRODUCT), cm_skomer)
    TARGET_COMMON_NAME := GT-S7710
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S3 Mini - golden
else ifeq ($(TARGET_PRODUCT), cm_golden)
    TARGET_COMMON_NAME := GT-I8190
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Grand Duos (i9082)
else ifeq ($(TARGET_PRODUCT), cm_i9082)
    TARGET_COMMON_NAME := Galaxy i9082
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S - i9000 (galaxysmtd)
else ifeq ($(TARGET_PRODUCT), cm_galaxysmtd)
    TARGET_COMMON_NAME := Galaxy i9000
    BOARD_USE_NTFS_3G := false
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/s5p_bl/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S Plus - i9001 (ariesve)
else ifeq ($(TARGET_PRODUCT), cm_ariesve)
    TARGET_COMMON_NAME := Galaxy i9001
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S Advance - janice
else ifeq ($(TARGET_PRODUCT), cm_janice)
    TARGET_COMMON_NAME := GT-I9070
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Ace 2 - codina
else ifeq ($(TARGET_PRODUCT), cm_codina)
    TARGET_COMMON_NAME := GT-I8160
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy W I8150 (ancora)
else ifeq ($(TARGET_PRODUCT), cm_ancora)
    TARGET_COMMON_NAME := Galaxy W I8150
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S Blaze 4G - SGH T-769
else ifeq ($(TARGET_PRODUCT), cm_t769)
    TARGET_COMMON_NAME := SGH-T769
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Grand Quatro (i8552)
else ifeq ($(TARGET_PRODUCT), cm_delos3geur)
    TARGET_COMMON_NAME := Galaxy i8552
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true
    
#Galaxy S2 International - i9100
else ifeq ($(TARGET_PRODUCT), cm_i9100)
    TARGET_COMMON_NAME := i9100
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S2 Epic 4G Touch - SPH-D710 (d710)
else ifeq ($(TARGET_PRODUCT), cm_d710)
    TARGET_COMMON_NAME := SPH-D710
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S2 - i9100g
else ifeq ($(TARGET_PRODUCT), cm_i9100g)
    TARGET_COMMON_NAME := i9100G
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note - n7000
else ifeq ($(TARGET_PRODUCT), cm_n7000)
    TARGET_COMMON_NAME := n7000
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S2 HD LTE (SGH-I757M) - celoxhd
else ifeq ($(TARGET_PRODUCT), cm_celoxhd)
    TARGET_COMMON_NAME := SGH-I757M
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Samsung Exhilarate SGH-I577 (exhilarate)
else ifeq ($(TARGET_PRODUCT), cm_exhilarate)
    TARGET_COMMON_NAME := SGH-I577 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Core Duos (i8262)
else ifeq ($(TARGET_PRODUCT), cm_i8262)
    TARGET_COMMON_NAME := Galaxy i8262
    BOOTLOADER_CMD_ARG := "download"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S2 Skyrocket i727 - skyrocket
else ifeq ($(TARGET_PRODUCT), cm_skyrocket)
    TARGET_COMMON_NAME := Skyrocket i727
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S3 International - i9300 - i9305
else ifneq ($(filter $(TARGET_PRODUCT),cm_i9300 cm_i9305),)
    TARGET_COMMON_NAME := Galaxy S3 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Samsung S3 Unified d2lte: d2att d2cri d2mtr d2spr d2tmo d2usc d2vzw
else ifeq ($(TARGET_PRODUCT), cm_d2lte)
    TARGET_COMMON_NAME := $(TARGET_PRODUCT)
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BOARD_USE_B_SLOT_PROTOCOL := true

# Galaxy S Relay 4G - SGH-T699 (apexqtmo) // Galaxy Express AT&T (expressatt)
# d2-common (d2lte) but with lower resolution
else ifneq ($(filter $(TARGET_PRODUCT), cm_apexqtmo cm_expressatt),)
    TARGET_COMMON_NAME := $(TARGET_PRODUCT)
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 2 - n7100
else ifeq ($(TARGET_PRODUCT), cm_n7100)
    TARGET_COMMON_NAME := Galaxy Note 2
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 2 LTE - t0lte (n7105), t0lteatt (i317 / i317M canada bell), t0ltetmo (T889), l900 (sprint), i605 (verizon)
else ifneq ($(filter $(TARGET_PRODUCT),cm_t0lte cm_t0lteatt cm_t0ltetmo cm_l900 cm_i605),)
    TARGET_COMMON_NAME := Note 2 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 3 LTE - N9005 Unified (hlte): hltexx, hltespr, hltetmo, hltecan, hltevzw
else ifeq ($(TARGET_PRODUCT), cm_hlte)
    TARGET_COMMON_NAME := Note 3 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 8.0 GSM (n5100), Wifi (n5110) and LTE (n5120)
else ifneq ($(filter $(TARGET_PRODUCT),cm_n5100 cm_n5110 cm_n5120),)
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
else ifneq ($(filter $(TARGET_PRODUCT),cm_n8000 cm_n8013 cm_n8020),)
    TARGET_COMMON_NAME := Galaxy Note 10.1 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 1280
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 10.1 2014 LTE (lt03ltexx)
else ifeq ($(TARGET_PRODUCT), cm_lt03ltexx)
    TARGET_COMMON_NAME := Note 10.1 2014 LTE
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1600
    TARGET_SCREEN_WIDTH := 2560
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Note 10.1 2014 Wifi (lt03wifi), 3G (lt03wifiue)
else ifneq ($(filter $(TARGET_PRODUCT),cm_lt03wifi cm_lt03wifiue),)
    TARGET_COMMON_NAME := Note 10.1 2014 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1600
    TARGET_SCREEN_WIDTH := 2560
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S4 Exynos - i9500
else ifeq ($(TARGET_PRODUCT), cm_i9500)
    TARGET_COMMON_NAME := i9500
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_POST_UNBLANK_COMMAND := "/sbin/postunblankdisplay.sh"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S4 i9505 Qualcomm variants (jflte): jfltecan jflteatt jfltecri jfltecsp jfltespr jfltetmo jflteusc jfltevzw jgedlte jfltexx jfltezm
else ifeq ($(TARGET_PRODUCT), cm_jflte)
    TARGET_COMMON_NAME := i9505 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S4 Mini: LTE - i9195 (serranoltexx) // 3G - i9190 (serrano3gxx) // Dual Sim (serranodsub)
else ifneq ($(filter $(TARGET_PRODUCT),cm_serranoltexx cm_serrano3gxx cm_serranodsub),)
    TARGET_COMMON_NAME := Galaxy S4 Mini ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S5 SM-G900F Qualcomm variants (klte): kltecan kltespr kltetmo kltevzw kltexx
else ifeq ($(TARGET_PRODUCT), cm_klte)
    TARGET_COMMON_NAME := Galaxy S5 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy S5 SM-G900H Exynos variant (k3gxx)
else ifeq ($(TARGET_PRODUCT), cm_k3gxx)
    TARGET_COMMON_NAME := Galaxy S5 SM-G900H
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Tab 2 - p3100, p3110
else ifneq ($(filter $(TARGET_PRODUCT),cm_p3100 cm_p3110),)
    TARGET_COMMON_NAME := Galaxy Tab 2 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 600
    TARGET_SCREEN_WIDTH := 1024
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    #RECOVERY_TOUCHSCREEN_SWAP_XY := true
    #RECOVERY_TOUCHSCREEN_FLIP_Y := true
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Tab 2 - p5100 / p5110
else ifneq ($(filter $(TARGET_PRODUCT),cm_p5100 cm_p5110),)
    TARGET_COMMON_NAME := Galaxy Tab 2 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 1280
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Premier I9260 - superior
else ifeq ($(TARGET_PRODUCT), cm_superior)
    TARGET_COMMON_NAME := Galaxy Premier I9260
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Mega I9205 (meliusltexx) and i9200 (melius3gxx)
else ifneq ($(filter $(TARGET_PRODUCT),cm_meliusltexx cm_melius3gxx),)
    TARGET_COMMON_NAME := Galaxy Mega ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/lcd/panel/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Samsung Galaxy Tab 3 7.0: SM-T210 (lt02wifi), SM-T211 (lt023g)
else ifneq ($(filter $(TARGET_PRODUCT),cm_lt02wifi cm_lt023g),)
    TARGET_COMMON_NAME := Galaxy Tab 3 7.0 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 600
    TARGET_SCREEN_WIDTH := 1024
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Samsung Galaxy Tab 3 8.0: SM-T310 (lt01wifi), SM-T311 (lt013g), SM-T315 (lt01lte)
else ifneq ($(filter $(TARGET_PRODUCT),cm_lt01wifi cm_lt013g cm_lt01lte),)
    TARGET_COMMON_NAME := Galaxy Tab 3 8.0 ($(TARGET_PRODUCT))
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Galaxy Tab Pro 8.4 WiFi SM-T320 (mondrianwifi)
else ifeq ($(TARGET_PRODUCT), cm_mondrianwifi)
    TARGET_COMMON_NAME := Galaxy Tab Pro SM-T320
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1600
    TARGET_SCREEN_WIDTH := 2560
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_USE_B_SLOT_PROTOCOL := true

#Google Galaxy Nexus (Samsung) - maguro, toro, toroplus (tuna common device)
else ifneq ($(filter $(TARGET_PRODUCT),cm_maguro cm_toro cm_toroplus),)
    TARGET_COMMON_NAME := Galaxy Nexus ($(TARGET_PRODUCT))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/s6e8aa0/brightness"

#Google Nexus S (Samsung) - crespo / crespo4g
else ifneq ($(filter $(TARGET_PRODUCT),cm_crespo cm_crespo4g),)
    TARGET_COMMON_NAME := Nexus S
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/s5p_bl/brightness"

#Google Nexus 4 (LGE) - mako
else ifeq ($(TARGET_PRODUCT), cm_mako)
    TARGET_COMMON_NAME := Nexus 4
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 768
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Google Nexus 5 (LGE) - hammerhead
else ifeq ($(TARGET_PRODUCT), cm_hammerhead)
    TARGET_COMMON_NAME := Nexus 5
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Google Nexus 7 (ASUS) - tilapia (3G) and grouper (wifi)
else ifneq ($(filter $(TARGET_PRODUCT),cm_tilapia cm_grouper),)
    TARGET_COMMON_NAME := Nexus 7 ($(TARGET_PRODUCT))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 800
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Google Nexus 7 (ASUS) 2013 (flo), LTE (deb)
else ifneq ($(filter $(TARGET_PRODUCT),cm_flo cm_deb),)
    TARGET_COMMON_NAME := Nexus 7 (2013 $(TARGET_PRODUCT))
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1200
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Google Nexus 10 (Samsung) - manta
else ifeq ($(TARGET_PRODUCT), cm_manta)
    TARGET_COMMON_NAME := Nexus 10
    TARGET_SCREEN_HEIGHT := 1600
    TARGET_SCREEN_WIDTH := 2560
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight.0/brightness"
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/android-battery/capacity"

#HP Touchpad (tenderloin)
else ifeq ($(TARGET_PRODUCT), cm_tenderloin)
    TARGET_COMMON_NAME := HP Touchpad
    TARGET_SCREEN_HEIGHT := 768
    TARGET_SCREEN_WIDTH := 1024
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Desire X - protou (No 10.2 tree)
else ifeq ($(TARGET_PRODUCT), cm_protou)
    TARGET_COMMON_NAME := HTC Desire X
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Droid Incredible 4G LTE - fireball
else ifeq ($(TARGET_PRODUCT), cm_fireball)
    TARGET_COMMON_NAME := Droid Incredible 4G LTE
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Explorer - pico (no cm tree)
else ifeq ($(TARGET_PRODUCT), cm_pico)
    TARGET_COMMON_NAME := HTC Explorer
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One - m7 (m7ul, m7tmo, m7att) / m7spr / m7vzw
else ifneq ($(filter $(TARGET_PRODUCT), cm_m7 cm_m7spr cm_m7vzw),)
    TARGET_COMMON_NAME := HTC One ($(TARGET_PRODUCT))
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Droid DNA (dlx)
else ifeq ($(TARGET_PRODUCT), cm_dlx)
    TARGET_COMMON_NAME := HTC Droid DNA (dlx)
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Desire 601 (zara)
else ifeq ($(TARGET_PRODUCT), cm_zara)
    TARGET_COMMON_NAME := HTC Desire 601 (zara)
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One X (endeavoru) - HTC One X+ (enrc2b)
else ifneq ($(filter $(TARGET_PRODUCT),cm_endeavoru cm_enrc2b),)
    TARGET_COMMON_NAME := HTC One X
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/tegra-pwm-bl/brightness"

#HTC One XL - evita
else ifeq ($(TARGET_PRODUCT), cm_evita)
    TARGET_COMMON_NAME := HTC One XL
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One S - ville
else ifeq ($(TARGET_PRODUCT), cm_ville)
    TARGET_COMMON_NAME := HTC One S
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One V - primou (no cm device tree)
else ifeq ($(TARGET_PRODUCT), cm_primou)
    TARGET_COMMON_NAME := HTC One V
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Evo 4G LTE (jewel)
else ifeq ($(TARGET_PRODUCT), cm_jewel)
    TARGET_COMMON_NAME := Evo 4G LTE
    KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Rezound (vigor)
else ifeq ($(TARGET_PRODUCT), cm_vigor)
    TARGET_COMMON_NAME := HTC Rezound
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One M8 (m8)
else ifneq ($(filter $(TARGET_PRODUCT),cm_m8 cm_m8spr cm_m8vzw cm_m8att),)
    TARGET_COMMON_NAME := HTC One M8 ($(TARGET_PRODUCT))
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One Mini (m4)
else ifeq ($(TARGET_PRODUCT), cm_m4)
    TARGET_COMMON_NAME := HTC One Mini
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Wildfire S (marvel) - armv6
else ifeq ($(TARGET_PRODUCT), cm_marvel)
    TARGET_COMMON_NAME := HTC Wildfire S
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_7x16.h\"
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Huawei Acsend P1 U9200 - viva (no cm tree)
else ifeq ($(TARGET_PRODUCT), cm_viva)
    TARGET_COMMON_NAME := Huawei_Acsend_P1_U9200
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd/brightness"

#LG Optimus One P500 (p500) - armv6
else ifeq ($(TARGET_PRODUCT), cm_p500)
    TARGET_COMMON_NAME := Optimus One P500
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus G ATT (e970) - Canada (e973) - Sprint (ls970) - Intl (e975)
else ifneq ($(filter $(TARGET_PRODUCT),cm_e970 cm_e973 cm_ls970 cm_e975),)
    TARGET_COMMON_NAME := Optimus G ($(TARGET_PRODUCT))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 768
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus G Pro (GSM) - e980
else ifeq ($(TARGET_PRODUCT), cm_e980)
    TARGET_COMMON_NAME := Optimus G Pro
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Spectrum 4G - vs920
else ifeq ($(TARGET_PRODUCT), cm_vs920)
    TARGET_COMMON_NAME := Spectrum 4G
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Nitro HD AT&T - p930
else ifeq ($(TARGET_PRODUCT), cm_p930)
    TARGET_COMMON_NAME := LG Nitro HD
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG G2 AT&T (d800) - G2 TMO (d801) - G2 Int (d802) - G2 CAN (d803) - d805 - d806 - G2 Verizon (vs980) - G2 Sprint (ls980)
else ifneq ($(filter $(TARGET_PRODUCT),cm_d800 cm_d801 cm_d802 cm_d803 cm_d805 cm_d806 cm_vs980 cm_ls980),)
    TARGET_COMMON_NAME := LG G2 ($(TARGET_PRODUCT))
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG G Pad 8.3 (v500)
else ifeq ($(TARGET_PRODUCT), cm_v500)
    TARGET_COMMON_NAME := LG G Pad 8.3
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1200
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus 4X HD P880 (p880)
else ifeq ($(TARGET_PRODUCT), cm_p880)
    TARGET_COMMON_NAME := Optimus 4X HD P880
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus L5 E610 (e610)
else ifeq ($(TARGET_PRODUCT), cm_e610)
    TARGET_COMMON_NAME := Optimus L5 E610
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus L7 P700 (p700)
else ifeq ($(TARGET_PRODUCT), cm_p700)
    TARGET_COMMON_NAME := Optimus L7 P700
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus L7 P705 (p705)
else ifeq ($(TARGET_PRODUCT), cm_p705)
    TARGET_COMMON_NAME := Optimus L7 P705
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Micromax A57 - a57 (no cm tree)
else ifeq ($(TARGET_PRODUCT), cm_a57)
    TARGET_COMMON_NAME := Micromax_A57
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
    BOARD_USE_NTFS_3G := false
    TARGET_SCREEN_HEIGHT := 480
    TARGET_SCREEN_WIDTH := 320
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Motorola Droid RAZR M - xt907
else ifeq ($(TARGET_PRODUCT), cm_xt907)
    TARGET_COMMON_NAME := Droid RAZR M
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola Droid RAZR HD GSM (xt925) and US (xt926)
else ifneq ($(filter $(TARGET_PRODUCT),cm_xt925 cm_xt926),)
    TARGET_COMMON_NAME := Droid RAZR HD ($(TARGET_PRODUCT))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola Atrix HD
else ifeq ($(TARGET_PRODUCT), cm_mb886)
    TARGET_COMMON_NAME := Atrix HD
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola - unified moto_msm8960 (mb886, xt925, xt926)
else ifeq ($(TARGET_PRODUCT), cm_moto_msm8960)
    TARGET_COMMON_NAME := Droid msm8960
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola Moto X: TMO (xt1053), US Cellular (xt1055), Sprint (xt1056), GSM (xt1058), VZW (xt1060)
else ifneq ($(filter $(TARGET_PRODUCT),cm_xt1053 cm_xt1055 cm_xt1056 cm_xt1058 cm_xt1060),)
    TARGET_COMMON_NAME := Moto X ($(TARGET_PRODUCT))
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola Moto G Unified (falcon): Verizon (xt1028), Boostmobile (xt1031), GSM (xt1032), Dual Sim (xt1033), Retail US (xt1034), Google Play Edition (falcon_gpe)
else ifeq ($(TARGET_PRODUCT), cm_falcon)
    TARGET_COMMON_NAME := Moto G (falcon)
    TARGET_USERIMAGES_USE_F2FS := true
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Oppo Find5 (find5)
else ifeq ($(TARGET_PRODUCT), cm_find5)
    TARGET_COMMON_NAME := Oppo Find5
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Oppo N1 (n1)
else ifeq ($(TARGET_PRODUCT), cm_n1)
    TARGET_COMMON_NAME := Oppo N1
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Oppo Find7,Find7a,X9007,X9006 (find7)
else ifeq ($(TARGET_PRODUCT), cm_find7)
    TARGET_COMMON_NAME := Oppo Find7
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Sony Xperia Z (yuga)
else ifeq ($(TARGET_PRODUCT), cm_yuga)
    TARGET_COMMON_NAME := Xperia Z
    KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lm3533-lcd-bl/brightness"

#Sony Xperia Z1 (honami)
else ifeq ($(TARGET_PRODUCT), cm_honami)
    TARGET_COMMON_NAME := Xperia Z1
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#Sony Xperia Z2 (sirius)
else ifeq ($(TARGET_PRODUCT), cm_sirius)
    TARGET_COMMON_NAME := Xperia Z2
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#Sony Xperia Tablet Z2 (castor)
else ifeq ($(TARGET_PRODUCT), cm_castor)
    TARGET_COMMON_NAME := Xperia Tablet Z2
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#Sony Xperia Tablet Z2 LTE (castor_windy)
else ifeq ($(TARGET_PRODUCT), cm_castor_windy)
    TARGET_COMMON_NAME := Xperia Tablet Z2 LTE
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#Sony Xperia ZU (togari)
else ifeq ($(TARGET_PRODUCT), cm_togari)
    TARGET_COMMON_NAME := Xperia ZU
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#Sony Xperia S (nozomi)
else ifeq ($(TARGET_PRODUCT), cm_nozomi)
    TARGET_COMMON_NAME := Xperia S
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Sony Xperia SP (huashan)
else ifeq ($(TARGET_PRODUCT), cm_huashan)
    TARGET_COMMON_NAME := Xperia SP
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight1/brightness"

#Sony Xperia T (mint)
else ifeq ($(TARGET_PRODUCT), cm_mint)
    TARGET_COMMON_NAME := Xperia T
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight_1/brightness"

#Sony Xperia ZL (odin)
else ifeq ($(TARGET_PRODUCT), cm_odin)
    TARGET_COMMON_NAME := Xperia ZL
    KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1920
    TARGET_SCREEN_WIDTH := 1080
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lm3533-lcd-bl/brightness"

#Sony Xperia ZR (dogo)
else ifeq ($(TARGET_PRODUCT), cm_dogo)
    TARGET_COMMON_NAME := Xperia ZR
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lm3533-lcd-bl/brightness"

#Sony Xperia Tablet Z (pollux)
else ifeq ($(TARGET_PRODUCT), cm_pollux)
    TARGET_COMMON_NAME := Xperia Tablet Z
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    BRIGHTNESS_SYS_FILE := "/sys/devices/i2c-0/0-002c/backlight/lcd-backlight/brightness"

#Sony Xperia Tablet Z LTE (pollux_windy)
else ifeq ($(TARGET_PRODUCT), cm_pollux_windy)
    TARGET_COMMON_NAME := Xperia Tablet Z LTE
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1200
    TARGET_SCREEN_WIDTH := 1920
    BRIGHTNESS_SYS_FILE := "/sys/devices/i2c-0/0-002c/backlight/lcd-backlight/brightness"

#Sony Xperia Z1 Compact (amami)
else ifeq ($(TARGET_PRODUCT), cm_amami)
    TARGET_COMMON_NAME := Xperia Z1 Compact
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    TARGET_SCREEN_HEIGHT := 1280
    TARGET_SCREEN_WIDTH := 720
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/wled:backlight/brightness"

#ZTE Warp Sequent - N861 (warp2)
else ifeq ($(TARGET_PRODUCT), cm_warp2)
    TARGET_COMMON_NAME := ZTE Warp Sequent - N861
    TARGET_SCREEN_HEIGHT := 960
    TARGET_SCREEN_WIDTH := 540
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#ZTE Awe (nex)
else ifeq ($(TARGET_PRODUCT), cm_nex)
    TARGET_COMMON_NAME := ZTE Awe (nex)
    TARGET_SCREEN_HEIGHT := 800
    TARGET_SCREEN_WIDTH := 480
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

endif
#---- end device specific config


# use libtar for backup/restore instead of busybox
BOARD_RECOVERY_USE_LIBTAR := true

# The below flags must always be defined as default in BoardConfig.mk, unless defined above:
# device name to display in About dialog
ifndef TARGET_COMMON_NAME
    TARGET_COMMON_NAME := $(TARGET_PRODUCT)
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
