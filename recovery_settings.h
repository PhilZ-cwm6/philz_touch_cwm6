/*
    Define path for all recovery settings files here
*/

#ifndef _RECOVERY_SETTINGS_H
#define _RECOVERY_SETTINGS_H

#include "cutils/properties.h"  // PROPERTY_VALUE_MAX
#include <linux/limits.h>   // PATH_MAX

#define RECOVERY_NO_CONFIRM_FILE    "clockworkmod/.no_confirm"
#define RECOVERY_MANY_CONFIRM_FILE  "clockworkmod/.many_confirm"
#define RECOVERY_VERSION_FILE       "clockworkmod/.recovery_version"
#define RECOVERY_LAST_INSTALL_FILE  "clockworkmod/.last_install_path"
#define EXTENDEDCOMMAND_SCRIPT      "/cache/recovery/extendedcommand"

// nandroid settings
#define NANDROID_HIDE_PROGRESS_FILE  "clockworkmod/.hidenandroidprogress"
#define NANDROID_BACKUP_FORMAT_FILE  "clockworkmod/.default_backup_format"
#ifdef BOARD_RECOVERY_USE_BBTAR
#define NANDROID_IGNORE_SELINUX_FILE "clockworkmod/.ignore_nandroid_secontext"
#endif

#define EFS_BACKUP_PATH     "clockworkmod/backup/.efs_backup"
#define MODEM_BIN_PATH      "clockworkmod/backup/.modem_bin"
#define RADIO_BIN_PATH      "clockworkmod/backup/.radio_bin"
#define CWM_BACKUP_PATH     "clockworkmod/backup"
#define TWRP_BACKUP_PATH    "TWRP/BACKUPS"
#define CUSTOM_ROM_PATH     "clockworkmod/custom_rom"

// other settings
#define PHILZ_SETTINGS_FILE     "/data/philz-touch/philz-touch_6.ini"
#define PHILZ_SETTINGS_BAK      "clockworkmod/philz-touch_6.ini.bak"
#define PHILZ_THEMES_PATH       "clockworkmod/themes"
#define AROMA_FM_PATH           "clockworkmod/aromafm/aromafm.zip"
#define MULTI_ZIP_FOLDER        "clockworkmod/multi_flash"
#define RECOVERY_ORS_PATH       "clockworkmod/ors"
#define ORS_BOOT_SCRIPT_FILE    "/cache/recovery/openrecoveryscript"

#ifdef PHILZ_TOUCH_RECOVERY
#define CUSTOM_RES_IMAGE_PATH   "clockworkmod/custom_res"
#define SCREEN_CAPTURE_FOLDER   "clockworkmod/screen_shots"
#endif

struct CWMSettingsIntValues {
    const char key[56];
    int value;
};

struct CWMSettingsLongIntValues {
    const char key[56];
    long int value;
};

struct CWMSettingsCharValues {
    const char key[56];
    char value[PROPERTY_VALUE_MAX];
}; 

struct CWMSettingsIntValues auto_restore_settings;
struct CWMSettingsIntValues check_root_and_recovery;
struct CWMSettingsIntValues apply_loki_patch;
struct CWMSettingsIntValues twrp_backup_mode;
struct CWMSettingsIntValues compression_value;
struct CWMSettingsIntValues nandroid_add_preload;
struct CWMSettingsIntValues enable_md5sum;
struct CWMSettingsIntValues show_nandroid_size_progress;
struct CWMSettingsIntValues use_nandroid_simple_logging;
struct CWMSettingsIntValues nand_prompt_on_low_space;
struct CWMSettingsIntValues signature_check_enabled;
struct CWMSettingsIntValues install_zip_verify_md5;

struct CWMSettingsIntValues boardEnableKeyRepeat;

// these are not checked on recovery start
// they are initialized on first call
struct CWMSettingsCharValues ors_backup_path;
struct CWMSettingsCharValues user_zip_folder;

#ifdef PHILZ_TOUCH_RECOVERY
struct CWMSettingsLongIntValues set_brightness;
struct CWMSettingsLongIntValues menu_height_increase;
struct CWMSettingsLongIntValues min_log_rows;
struct CWMSettingsIntValues show_background_icon;
struct CWMSettingsCharValues background_image;
struct CWMSettingsLongIntValues header_text_color;
struct CWMSettingsLongIntValues menu_text_color;
struct CWMSettingsLongIntValues normal_text_color;
struct CWMSettingsLongIntValues menu_background_color;
struct CWMSettingsLongIntValues menu_background_transparency;
struct CWMSettingsLongIntValues menu_highlight_color;
struct CWMSettingsLongIntValues menu_highlight_transparency;
struct CWMSettingsLongIntValues menu_separator_color;
struct CWMSettingsIntValues menu_separator_transparency;
struct CWMSettingsIntValues show_menu_separation;
struct CWMSettingsIntValues show_virtual_keys;
struct CWMSettingsIntValues show_clock;
struct CWMSettingsIntValues show_battery;
struct CWMSettingsLongIntValues batt_clock_color;
struct CWMSettingsLongIntValues dim_timeout;
struct CWMSettingsLongIntValues blank_timeout;

struct CWMSettingsIntValues touch_to_validate;
struct CWMSettingsLongIntValues scroll_sensitivity;
struct CWMSettingsLongIntValues touch_accuracy;
struct CWMSettingsLongIntValues slide_left_action;
struct CWMSettingsLongIntValues slide_right_action;
struct CWMSettingsLongIntValues double_tap_action;
struct CWMSettingsLongIntValues press_lift_action;
struct CWMSettingsLongIntValues press_move_action;
struct CWMSettingsIntValues enable_vibrator;
struct CWMSettingsIntValues wait_after_install;

struct CWMSettingsLongIntValues t_zone;
struct CWMSettingsLongIntValues t_zone_offset;
struct CWMSettingsIntValues use_dst_time;
struct CWMSettingsIntValues use_qcom_time_daemon;
struct CWMSettingsLongIntValues use_qcom_time_offset;

#endif // PHILZ_TOUCH_RECOVERY
void verify_settings_file();
void refresh_recovery_settings(int on_start);
#ifdef PHILZ_TOUCH_RECOVERY
void refresh_touch_gui_settings(int on_start);
#endif

struct CompilerFlagsUI {
    int char_height;
    int char_width;
    int board_has_low_resolution;
    int recovery_touchscreen_swap_xy;
    int recovery_touchscreen_flip_x;
    int recovery_touchscreen_flip_y;
    int board_use_b_slot_protocol;
    int board_use_fb2png;
    const char brightness_sys_file[PATH_MAX];
    const char battery_level_path[PATH_MAX];
    const char board_post_unblank_command[PATH_MAX];
}; 

struct CompilerFlagsUI libtouch_flags;

#endif // _RECOVERY_SETTINGS_H

