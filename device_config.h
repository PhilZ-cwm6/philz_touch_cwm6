// included in recovery_ui.h
/*
 * Copyright (C) 2011-2012 sakuramilk <c.sakuramilk@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// kbc-developers code
//setup specific device settings as needed
//called in extendedcommands.c only for now
//for ums lun file fix on some devices and show log menu function


// Samsung Galaxy S Blaze 4G - SGH T-769 (t769)
#ifdef TARGET_DEVICE_T769
#define BOOTLOADER_CMD_ARG "download"
#endif

// Samsung Exhilarate SGH-I577 (i577)
#ifdef TARGET_DEVICE_I577
#define BOOTLOADER_CMD_ARG "download"
#endif

// i9100 - Galaxy S2 international
#ifdef TARGET_DEVICE_I9100
#define BOARD_UMS_LUNFILE     "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// i9100G - Galaxy S2 i9100G
#ifdef TARGET_DEVICE_I9100G
#define BOARD_UMS_LUNFILE     "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// i9103 - Galaxy R / Z
#ifdef TARGET_DEVICE_I9103
#define BOARD_UMS_LUNFILE     "/sys/devices/platform/fsl-tegra-udc/gadget/lun%d/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// Galaxy Exhibit Variants - codinaxxx
#ifdef TARGET_DEVICE_CODINAMTR
#define BOARD_UMS_LUNFILE     "/sys/devices/platform/musb-ux500.0/musb-hdrc/gadget/lun%d/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// Galaxy Grand Duos - i9082
#ifdef TARGET_DEVICE_I9082
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// Galaxy S Plus - i9001 (ariesve)
#ifdef TARGET_DEVICE_I9001
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// i9300 - Galaxy S3 international
#ifdef TARGET_DEVICE_I9300
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// S3 - d2tmo - SGH-T999
#ifdef TARGET_DEVICE_D2TMO
//#define BOARD_UMS_LUNFILE    "/sys/class/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// n7000
#ifdef TARGET_DEVICE_N7000
#define BOARD_UMS_LUNFILE     "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// n7100
#ifdef TARGET_DEVICE_N7100
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// p5100
#ifdef TARGET_DEVICE_P5100
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// p3100
#ifdef TARGET_DEVICE_P3100
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// n8000
#ifdef TARGET_DEVICE_N8000
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// i9500
#ifdef TARGET_DEVICE_I9500
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// i9505
#ifdef TARGET_DEVICE_I9505
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif

// Galaxy S4 Mini LTE - i9195 (serranoltexx) // Galaxy S4 Mini 3G - i9190 (serrano3gxx)
#ifdef TARGET_DEVICE_S4_MINI
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#define BOOTLOADER_CMD_ARG "download"
#endif
