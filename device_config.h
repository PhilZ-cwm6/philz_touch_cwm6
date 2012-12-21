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


// i9300
#ifdef TARGET_DEVICE_I9300
// ums
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#endif


// i9100
#ifdef TARGET_DEVICE_I9100
// ums
#define BOARD_UMS_LUNFILE     "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
// dedicated log menu to have more room to read logs
#define SHOW_LOG_MENU "true"
#endif


// n7000
#ifdef TARGET_DEVICE_N7000
// ums
#define BOARD_UMS_LUNFILE     "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun%d/file"
#endif


// n7100
#ifdef TARGET_DEVICE_N7100
// ums
#define BOARD_UMS_LUNFILE    "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun0/file"
#endif


// p5100
#ifdef TARGET_DEVICE_P5100
// ums
#endif
