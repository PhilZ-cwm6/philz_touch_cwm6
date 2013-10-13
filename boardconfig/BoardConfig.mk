############################################
# Device Specific Config                   #
# These can go under device BoardConfig.mk #
# By PhilZ for PhilZ Touch recovery        #
############################################

#Galaxy S2 International - i9100
ifeq ($(TARGET_PRODUCT), cm_i9100)
    TARGET_COMMON_NAME := i9100
    BOOTLOADER_CMD_ARG := "download"
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy S2 - i9100g
else ifeq ($(TARGET_PRODUCT), cm_i9100g)
    TARGET_COMMON_NAME := i9100G
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
    BOOTLOADER_CMD_ARG := "download"
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy R / Z - i9103
else ifeq ($(TARGET_PRODUCT), cm_i9103)
    TARGET_COMMON_NAME := i9103
    BOARD_UMS_LUNFILE := "/sys/devices/platform/fsl-tegra-udc/gadget/lun%d/file"
    BOOTLOADER_CMD_ARG := "download"
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Galaxy Exhibit Variants - codinaxxx
else ifeq ($(TARGET_PRODUCT), cm_codinamtr)
    TARGET_COMMON_NAME := SGH-T599N
    BOARD_UMS_LUNFILE := "/sys/devices/platform/musb-ux500.0/musb-hdrc/gadget/lun%d/file"
    BOOTLOADER_CMD_ARG := "download"
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy Grand Duos - i9082
else ifeq ($(TARGET_PRODUCT), cm_i9082)
    TARGET_COMMON_NAME := Galaxy_I9082
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy S Plus - i9001 (ariesve)
else ifeq ($(TARGET_PRODUCT), cm_ariesve)
    TARGET_COMMON_NAME := Galaxy_I9001
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
    BOOTLOADER_CMD_ARG := "download"
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Galaxy S3 International - i9300
else ifeq ($(TARGET_PRODUCT), cm_i9300)
    TARGET_COMMON_NAME := i930x
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy S3 T-Mobile - SGH-T999 (d2tmo)
else ifeq ($(TARGET_PRODUCT), cm_d2tmo)
    TARGET_COMMON_NAME := SGH-T999
    BOOTLOADER_CMD_ARG := "download"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Galaxy S4 International - i9500
else ifeq ($(TARGET_PRODUCT), cm_i9500)
    TARGET_COMMON_NAME := i9500
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    BOARD_USE_FB2PNG := false
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    BOARD_POST_UNBLANK_COMMAND := "/sbin/postunblankdisplay.sh"

#Galaxy S4 - i9505, jfltexx
else ifneq ($(filter $(TARGET_PRODUCT),cm_jfltexx cm_jflteatt cm_jfltevzw),)
    TARGET_COMMON_NAME := i9505
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    BOARD_USE_FB2PNG := false
    #BOARD_HAS_SLOW_STORAGE := true
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    KERNEL_NTFS_MODULE_NAME := "ntfs"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Galaxy S4 Mini LTE - i9195 (serranoltexx) // Galaxy S4 Mini 3G - i9190 (serrano3gxx)
else ifneq ($(filter $(TARGET_PRODUCT),cm_serranoltexx cm_serrano3gxx),)
    TARGET_COMMON_NAME := Galaxy_S4_Mini-$(TARGET_PRODUCT)
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    KERNEL_EXFAT_MODULE_NAME := "exfat"
    KERNEL_NTFS_MODULE_NAME := "ntfs"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Galaxy Note - n7000
else ifeq ($(TARGET_PRODUCT), cm_n7000)
    TARGET_COMMON_NAME := n7000
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
    BOOTLOADER_CMD_ARG := "download"
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy Note 2 - n7100
else ifeq ($(TARGET_PRODUCT), cm_n7100)
    TARGET_COMMON_NAME := n710x-i317M-T889
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy Note 10.1 - n8000
else ifeq ($(TARGET_PRODUCT), cm_n8000)
    TARGET_COMMON_NAME := n8000
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy Tab 2 - p3100
else ifeq ($(TARGET_PRODUCT), cm_p3100)
    TARGET_COMMON_NAME := p3100
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"
    RECOVERY_TOUCHSCREEN_SWAP_XY := true
    RECOVERY_TOUCHSCREEN_FLIP_Y := true

#Galaxy Tab 2 - p5100
else ifeq ($(TARGET_PRODUCT), cm_p5100)
    TARGET_COMMON_NAME := p5100
    BOARD_UMS_LUNFILE := "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
    BOOTLOADER_CMD_ARG := "download"
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/panel/brightness"

#Galaxy S Blaze 4G - SGH T-769 (t769)
else ifeq ($(TARGET_PRODUCT), cm_t769)
    TARGET_COMMON_NAME := SGH_T769
    BOOTLOADER_CMD_ARG := "download"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
    BOARD_HAS_LOW_RESOLUTION := true

#Samsung Exhilarate SGH-I577 (i577)
else ifeq ($(TARGET_PRODUCT), cm_i577)
    TARGET_COMMON_NAME := SGH_I577
    BOOTLOADER_CMD_ARG := "download"
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Desire X - protou
else ifeq ($(TARGET_PRODUCT), cm_protou)
    TARGET_COMMON_NAME := HTC_Desire_X
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Droid Incredible 4G LTE - fireball
else ifeq ($(TARGET_PRODUCT), cm_fireball)
    TARGET_COMMON_NAME := HTC_Droid_Incredible_4G_LTE
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC Explorer - pico
else ifeq ($(TARGET_PRODUCT), cm_pico)
    TARGET_COMMON_NAME := HTC_Explorer
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
    BOARD_USE_NTFS_3G := false
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One - m7ul / m7spr / m7tmo
else ifneq ($(filter $(TARGET_PRODUCT),cm_m7ul cm_m7spr cm_m7tmo),)
    TARGET_COMMON_NAME := HTC_One-$(TARGET_PRODUCT)
    #KERNEL_EXFAT_MODULE_NAME := "texfat"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One X - endeavoru
else ifeq ($(TARGET_PRODUCT), cm_endeavoru)
    TARGET_COMMON_NAME := HTC_One_X
    BOARD_USE_NTFS_3G := false
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/tegra-pwm-bl/brightness"

#HTC One XL - evita
else ifeq ($(TARGET_PRODUCT), cm_evita)
    TARGET_COMMON_NAME := HTC_One_XL
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One S - ville
else ifeq ($(TARGET_PRODUCT), cm_ville)
    TARGET_COMMON_NAME := HTC_One_S
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#HTC One V - primou
else ifeq ($(TARGET_PRODUCT), cm_primou)
    TARGET_COMMON_NAME := HTC_One_V
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LGE Nexus 4 - mako
else ifeq ($(TARGET_PRODUCT), cm_mako)
    TARGET_COMMON_NAME := Nexus_4
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#LG Optimus G ATT (e970) - Canada (e973) - Sprint (ls970) - Intl (e975)
else ifneq ($(filter $(TARGET_PRODUCT),cm_e970 cm_e973 cm_ls970 cm_e975),)
    TARGET_COMMON_NAME := LG_Optimus_G-$(TARGET_PRODUCT)
    #KERNEL_EXFAT_MODULE_NAME = "texfat"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#ASUS Nexus 7 (Wifi) - tilapia (grouper)
else ifneq ($(filter $(TARGET_PRODUCT),cm_tilapia cm_grouper),)
    TARGET_COMMON_NAME := ASUS_Nexus_7-$(TARGET_PRODUCT)
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight/brightness"

#Samsung Nexus 10 - manta
else ifeq ($(TARGET_PRODUCT), cm_manta)
    TARGET_COMMON_NAME := Samsung_Nexus_10
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/pwm-backlight.0/brightness"
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/android-battery/capacity"

#Samsung Galaxy Nexus - maguro, toro, toroplus (tuna common device)
else ifneq ($(filter $(TARGET_PRODUCT),cm_maguro cm_toro cm_toroplus),)
    TARGET_COMMON_NAME := Samsung_Galaxy_Nexus-$(TARGET_PRODUCT)
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/s6e8aa0/brightness"

#Samsung Nexus S - crespo
else ifeq ($(TARGET_PRODUCT), cm_crespo)
    TARGET_COMMON_NAME := Samsung_Nexus_S
    BOARD_HAS_LOW_RESOLUTION := true
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/s5p_bl/brightness"

#Sony Xperia Z - yuga
else ifeq ($(TARGET_PRODUCT), cm_yuga)
    TARGET_COMMON_NAME := Sony_Xperia_Z
    KERNEL_EXFAT_MODULE_NAME := "texfat"
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lm3533-lcd-bl/brightness"

#Huawei Acsend P1 U9200 - viva
else ifeq ($(TARGET_PRODUCT), cm_viva)
    TARGET_COMMON_NAME := Huawei_Acsend_P1_U9200
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd/brightness"

#Motorola Droid RAZR M (xt907)
else ifeq ($(TARGET_PRODUCT), cm_xt907)
    TARGET_COMMON_NAME := Droid_RAZR_M
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Motorola RAZR i XT890 (smi)
else ifeq ($(TARGET_PRODUCT), cm_smi)
    TARGET_COMMON_NAME := RAZR_I
    NO_AROMA_FILE_MANAGER := true
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"

#Motorola Droid RAZR HD GSM (xt925) and US (xt926)
else ifneq ($(filter $(TARGET_PRODUCT),cm_xt925 cm_xt926),)
    TARGET_COMMON_NAME := Droid_RAZR_HD-$(TARGET_PRODUCT)
    BRIGHTNESS_SYS_FILE := "/sys/class/backlight/lcd-backlight/brightness"

#Micromax A57 (a57)
else ifeq ($(TARGET_PRODUCT), cm_a57)
    TARGET_COMMON_NAME := Micromax_A57
    BOARD_USE_CUSTOM_RECOVERY_FONT := \"font_10x18.h\"
    BOARD_USE_NTFS_3G := false
    BRIGHTNESS_SYS_FILE := "/sys/class/leds/lcd-backlight/brightness"
endif

#---- end device specific config

# temporary workaround to backup/restore selinux context, thanks to @xiaolu
RECOVERY_NEED_SELINUX_FIX := true

# The below flags must always be defined as default in BoardConfig.mk, unless defined above:
# device name to display in About dialog
ifndef TARGET_COMMON_NAME
    TARGET_COMMON_NAME := $(TARGET_PRODUCT)
endif

# battery level default path (PhilZ Touch Only)
ifndef BATTERY_LEVEL_PATH
    BATTERY_LEVEL_PATH := "/sys/class/power_supply/battery/capacity"
endif

