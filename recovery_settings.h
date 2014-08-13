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
#define EFS_BACKUP_PATH     "clockworkmod/backup/.efs_backup"
#define MODEM_BIN_PATH      "clockworkmod/backup/.modem_bin"
#define RADIO_BIN_PATH      "clockworkmod/backup/.radio_bin"
#define CWM_BACKUP_PATH     "clockworkmod/backup"
#define TWRP_BACKUP_PATH    "TWRP/BACKUPS"
#define CUSTOM_ROM_PATH     "clockworkmod/custom_rom"

// other settings
// PHILZ_SETTINGS_FILE  : main config file loaded on start up
// PHILZ_SETTINGS_FILE2 : whenever we write to config file, we do a second copy on sdcard to be loaded after a wipe for example
// PHILZ_SETTINGS_BAK   : created/loaded from settings menus (user can save his custom settings before trying some modifications)
#define PHILZ_SETTINGS_FILE     "/data/philz-touch/philz-touch_6.ini"
#define PHILZ_SETTINGS_FILE2    "clockworkmod/philz-touch_6.sav"
#define PHILZ_SETTINGS_BAK      "clockworkmod/philz-touch_6.ini.bak"
#define PHILZ_THEMES_PATH       "clockworkmod/themes"
#define AROMA_FM_PATH           "clockworkmod/aromafm/aromafm.zip"
#define MULTI_ZIP_FOLDER        "clockworkmod/multi_flash"
#define RECOVERY_ORS_PATH       "clockworkmod/ors"
#define ORS_BOOT_SCRIPT_FILE    "/cache/recovery/openrecoveryscript"


#ifdef PHILZ_TOUCH_RECOVERY
// if these are changed, they won't take effect until we update libtouch_gui
// custom background images
#define CUSTOM_RES_IMAGE_PATH   "clockworkmod/custom_res"

// capture screen folder
#define SCREEN_CAPTURE_FOLDER   "clockworkmod/screen_shots"

// recovery lock file, pass key max chars and max allowed errors
#define RECOVERY_LOCK_FILE      "/system/.recovery_key.lok"
#define RECOVERY_LOCK_MAX_CHARS 6
#define RECOVERY_LOCK_MAX_ERROR 3
#endif


// Start recovery settings ini pairs
// first, we define pairs categories based on value type
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

// now we define the key/value pairs
/*
on recovery exit, check if we need to nag for:
    - auto_restore_settings: missing settings file after a wipe while we have a backup (auto restore or prompt to restore)
    - check_root_and_recovery: root and recovery that could be messed up (user set)
- nandroid_add_preload: must be set to 0 on start. Then, if set to 1 in recovery settings file AND /preload volume exists, it will be 1, else, it is 0
- show_background_icon: used to refresh background icon without reading settings file (nandroid exit, show_log_menu())
- show_virtual_keys: keep 0 on start to avoid virtual keys showing briefly when set disabled by user
- scroll_sensitivity: number of pixels finger needs to move to trigger a scroll /up/down by 1 menu
- touch_accuracy: number of pixels to assume a finger touch/lift action without moving (validate on finger lifted)
- t_zone: in hours
- t_zone_offset: offset value in minutes for timezone (0 to 59 mn)
- use_dst_time: enable/disable daylight saving time (0/1 toggle)
- use_qcom_time_offset: holds time offset in seconds for qualcom boards
*/
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
// selective theme gui settings
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
struct CWMSettingsCharValues brightness_user_path;
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

// time settings
struct CWMSettingsLongIntValues t_zone;
struct CWMSettingsLongIntValues t_zone_offset;
struct CWMSettingsIntValues use_dst_time;
struct CWMSettingsIntValues use_qcom_time_data_files;
struct CWMSettingsIntValues use_qcom_time_daemon;
struct CWMSettingsLongIntValues use_qcom_time_offset;

// pass compiler flags to libtouch_gui
struct CompilerFlagsUI {
    int char_height;
    int char_width;
    int board_has_low_resolution;
    int recovery_touchscreen_swap_xy;
    int recovery_touchscreen_flip_x;
    int recovery_touchscreen_flip_y;
    int board_use_b_slot_protocol;
    char brightness_sys_file[PATH_MAX];
    const char battery_level_path[PATH_MAX];
    const char board_post_unblank_command[PATH_MAX];
}; 

struct CompilerFlagsUI libtouch_flags;

// -------- End recovery settings ini pairs

// load settings from config.ini file
void refresh_touch_gui_settings(int on_start);
#endif    // PHILZ_TOUCH_RECOVERY
void refresh_recovery_settings(int on_start);

// check settings file on start and prompt to restore it if absent AND a backup is found: called by recovery.c
void verify_settings_file();

void toggle_signature_check();
void toggle_install_zip_verify_md5();
#ifdef ENABLE_LOKI
void toggle_loki_support();
int loki_support_enabled();
#endif

int read_config_file(const char* config_file, const char *key, char *value, const char *value_def);
int write_config_file(const char* config_file, const char* key, const char* value);

/*
properties reference:
ro.cwm.backup_partitions
ro.cwm.enable_key_repeat
ro.cwm.repeatable_keys
ro.cwm.forbid_mount
ro.cwm.forbid_format
ro.cwm.prefer_tar
ro.sf.lcd_density // not used in PhilZ Touch
ro.loki_disabled
ro.bootloader.mode
*/

#endif // _RECOVERY_SETTINGS_H
