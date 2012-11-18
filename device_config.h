//setup specific device settings as needed
//called in extendedcommands.c only for now
//for ums lun file fix on some devices and show log menu function


// i9300
#ifdef TARGET_DEVICE_I9300
// ums
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
#endif


// p5100
#ifdef TARGET_DEVICE_P5100
// ums
#endif
