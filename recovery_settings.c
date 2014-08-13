#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/limits.h>

#include "cutils/properties.h"

#include "common.h"
#include "install.h"
#include "roots.h"
#include "recovery_ui.h"
#include "extendedcommands.h"
#include "advanced_functions.h"
#include "recovery_settings.h"
#include "nandroid.h"

// Start initialize recovery key/value settings
// touch recovery key/value settings are initialized in gui_settings.c
struct CWMSettingsIntValues auto_restore_settings = { "auto_restore_settings", 1 };
struct CWMSettingsIntValues check_root_and_recovery = { "check_root_and_recovery", 1 };
struct CWMSettingsIntValues apply_loki_patch = { "apply_loki_patch", 1 };
struct CWMSettingsIntValues twrp_backup_mode = { "twrp_backup_mode", 0 };
struct CWMSettingsIntValues compression_value = { "compression_value", TAR_GZ_DEFAULT };
struct CWMSettingsIntValues nandroid_add_preload = { "nandroid_add_preload", 0 };
struct CWMSettingsIntValues enable_md5sum = { "enable_md5sum", 1 };
struct CWMSettingsIntValues show_nandroid_size_progress = { "show_nandroid_size_progress", 0 };
struct CWMSettingsIntValues use_nandroid_simple_logging = { "use_nandroid_simple_logging", 1 };
struct CWMSettingsIntValues nand_prompt_on_low_space = { "nand_prompt_on_low_space", 1 };
struct CWMSettingsIntValues signature_check_enabled = { "signature_check_enabled", 0 };
struct CWMSettingsIntValues install_zip_verify_md5 = { "install_zip_verify_md5", 0 };

struct CWMSettingsIntValues boardEnableKeyRepeat = { "boardEnableKeyRepeat", 1 };

// these are not checked on recovery start
// they are initialized on first call
struct CWMSettingsCharValues ors_backup_path = { "ors_backup_path", "" };
struct CWMSettingsCharValues user_zip_folder = { "user_zip_folder", "" };

//----- End initialize recovery key/value settings


// pass in compiler flags to libtouch_gui
#ifdef PHILZ_TOUCH_RECOVERY

#ifdef BOARD_HAS_LOW_RESOLUTION
#define board_has_low_resolution        1
#else
#define board_has_low_resolution        0
#endif

#ifdef RECOVERY_TOUCHSCREEN_SWAP_XY
#define recovery_touchscreen_swap_xy    1
#else
#define recovery_touchscreen_swap_xy    0
#endif

#ifdef RECOVERY_TOUCHSCREEN_FLIP_X
#define recovery_touchscreen_flip_x     1
#else
#define recovery_touchscreen_flip_x     0
#endif

#ifdef RECOVERY_TOUCHSCREEN_FLIP_Y
#define recovery_touchscreen_flip_y     1
#else
#define recovery_touchscreen_flip_y     0
#endif

#ifdef BOARD_USE_B_SLOT_PROTOCOL
#define board_use_b_slot_protocol       1
#else
#define board_use_b_slot_protocol       0
#endif

#ifndef BOARD_POST_UNBLANK_COMMAND
#define BOARD_POST_UNBLANK_COMMAND      ""
#endif

// initialize the libtouch flags
struct CompilerFlagsUI libtouch_flags = {
    CHAR_HEIGHT,
    CHAR_WIDTH,
    board_has_low_resolution,
    recovery_touchscreen_swap_xy,
    recovery_touchscreen_flip_x,
    recovery_touchscreen_flip_y,
    board_use_b_slot_protocol,
    BRIGHTNESS_SYS_FILE,
    BATTERY_LEVEL_PATH,
    BOARD_POST_UNBLANK_COMMAND
};
#endif // PHILZ_TOUCH_RECOVERY

/***********************************/
/*                                 */
/* Start PhilZ Touch Settings Menu */
/*                                 */
/***********************************/

/* On recovery exit, check if there is a settings file (PHILZ_SETTINGS_FILE)
   if not, try to restore the copy we do on primary storage (PHILZ_SETTINGS_FILE2)
 * Also, if no copy is found, always create it if there is a settings file
   we shouldn't always copy settings file on exit to avoid loosing user config after a wipe if he just change brightness by error for exp...

 Function is called when rebooting
 After success install of a new rom, before reboot, it will preserve settings if they were wiped by installed ROM
*/
void verify_settings_file() {
    char settings_copy[PATH_MAX];
    sprintf(settings_copy, "%s/%s", get_primary_storage_path(), PHILZ_SETTINGS_FILE2);

    // always have a copy of settings file in primary storage
    if (file_found(PHILZ_SETTINGS_FILE) && !file_found(settings_copy))
        copy_a_file(PHILZ_SETTINGS_FILE, settings_copy);

    // restore settings from the copy if needed (after a wipe)
    if (!file_found(PHILZ_SETTINGS_FILE) && file_found(settings_copy)) {
        ui_SetShowText(true);
        if (!auto_restore_settings.value && !confirm_selection("Restore recovery settings?", "Yes - Restore from sdcard"))
            return;
        if (copy_a_file(settings_copy, PHILZ_SETTINGS_FILE) == 0)
            ui_print("Recovery settings restored.\n");
    }
}

static void check_auto_restore_settings() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, auto_restore_settings.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        auto_restore_settings.value = 0;
    else
        auto_restore_settings.value = 1;
}

// on recovery exit, check if we need to nag for root and recovery that could be messed up
static void check_root_and_recovery_settings() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, check_root_and_recovery.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        check_root_and_recovery.value = 0;
    else
        check_root_and_recovery.value = 1;
}

// refresh nandroid compression
static void refresh_nandroid_compression() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, compression_value.key, value, TAR_GZ_DEFAULT_STR);
    if (strcmp(value, "fast") == 0)
        compression_value.value = TAR_GZ_FAST;
    else if (strcmp(value, "low") == 0)
        compression_value.value = TAR_GZ_LOW;
    else if (strcmp(value, "medium") == 0)
        compression_value.value = TAR_GZ_MEDIUM;
    else if (strcmp(value, "high") == 0)
        compression_value.value = TAR_GZ_HIGH;
    else
        compression_value.value = TAR_GZ_DEFAULT;
}

// check user setting for backup mode (TWRP vs CWM)
static void check_backup_restore_mode() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, twrp_backup_mode.key, value, "false");
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
        twrp_backup_mode.value = 1;
    else
        twrp_backup_mode.value = 0;
}

// check nandroid preload setting
static void check_nandroid_preload() {
    if (volume_for_path("/preload") == NULL)
        return; // nandroid_add_preload.value = 0 by default on recovery start

    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, nandroid_add_preload.key, value, "0");
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
        nandroid_add_preload.value = 1;
    else
        nandroid_add_preload.value = 0;
}

// check nandroid md5 sum
static void check_nandroid_md5sum() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, enable_md5sum.key, value, "1");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        enable_md5sum.value = 0;
    else
        enable_md5sum.value = 1;
}

// check show nandroid size progress
static void check_show_nand_size_progress() {
    char value_def[3] = "1";
#ifdef BOARD_HAS_SLOW_STORAGE
    sprintf(value_def, "0");
#endif
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, show_nandroid_size_progress.key, value, value_def);
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        show_nandroid_size_progress.value = 0;
    else
        show_nandroid_size_progress.value = 1;
}

// check if we need simple logging during nandroid jobs
// backup command progress (file names) will not be written to log file
// logging is still written to screen
static void check_nandroid_simple_logging() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, use_nandroid_simple_logging.key, value, "1");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        use_nandroid_simple_logging.value = 0;
    else
        use_nandroid_simple_logging.value = 1;
}

// check prompt on low backup space
static void check_prompt_on_low_space() {
    char value_def[3] = "1";
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, nand_prompt_on_low_space.key, value, value_def);
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        nand_prompt_on_low_space.value = 0;
    else
        nand_prompt_on_low_space.value = 1;
}

// check if we should verify signature during install of zip packages
// only called on recovery start
void toggle_signature_check() {
    char value[3];
    signature_check_enabled.value = !signature_check_enabled.value;
    sprintf(value, "%d", signature_check_enabled.value);
    write_config_file(PHILZ_SETTINGS_FILE, signature_check_enabled.key, value);
    // ui_print("Signature Check: %s\n", signature_check_enabled.value ? "Enabled" : "Disabled");
}

static void check_signature_check() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, signature_check_enabled.key, value, "0");
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
        signature_check_enabled.value = 1;
    else
        signature_check_enabled.value = 0;
}

// verify md5sum of zip file before they are installed
void toggle_install_zip_verify_md5() {
    char value[3];
    install_zip_verify_md5.value ^= 1;
    sprintf(value, "%d", install_zip_verify_md5.value);
    write_config_file(PHILZ_SETTINGS_FILE, install_zip_verify_md5.key, value);
}

static void check_install_zip_verify_md5() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, install_zip_verify_md5.key, value, "0");
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
        install_zip_verify_md5.value = 1;
    else
        install_zip_verify_md5.value = 0;
}

#ifdef ENABLE_LOKI
void toggle_loki_support() {
    char value[3];
    apply_loki_patch.value ^= 1;
    sprintf(value, "%d", apply_loki_patch.value);
    write_config_file(PHILZ_SETTINGS_FILE, apply_loki_patch.key, value);
    // ui_print("Loki Support: %s\n", apply_loki_patch.value ? "Enabled" : "Disabled");
}

// this is called when we load recovery settings and when we istall_package()
// it is needed when after recovery is booted, user wipes /data, then he installs a ROM: we can still return the user setting 
int loki_support_enabled() {
    char prop_value[PROPERTY_VALUE_MAX];
    int ret = -1;

    property_get("ro.loki_disabled", prop_value, "0");
    if (strcmp(prop_value, "0") == 0) {
        // device variant supports loki: check if user enabled it
        // if there is no settings file (read_config_file() < 0), it could be we have wiped /data before installing zip
        // in that case, return current value (we last loaded on start or when user last set it) and not default
        if (read_config_file(PHILZ_SETTINGS_FILE, apply_loki_patch.key, prop_value, "0") >= 0) {
            if (strcmp(prop_value, "true") == 0 || strcmp(prop_value, "1") == 0)
                apply_loki_patch.value = 1;
            else
                apply_loki_patch.value = 0;
        }
        ret = apply_loki_patch.value;
    }
    return ret;
}
#endif

void refresh_recovery_settings(int on_start) {
    check_auto_restore_settings();
    check_root_and_recovery_settings();
    refresh_nandroid_compression();
    check_backup_restore_mode();
    check_nandroid_preload();
    check_nandroid_md5sum();
    check_show_nand_size_progress();
    check_nandroid_simple_logging();
    check_prompt_on_low_space();
    check_signature_check();
    check_install_zip_verify_md5();
#ifdef ENABLE_LOKI
    loki_support_enabled();
#endif
#ifdef PHILZ_TOUCH_RECOVERY
    refresh_touch_gui_settings(on_start);
#endif
    // unmount settings file on recovery start
    // keep preserve_data_media() if we ever move settings file to /sdcard
    if (on_start) {
        preserve_data_media(0);
        ensure_path_unmounted(PHILZ_SETTINGS_FILE);
        preserve_data_media(1);
    }
}

/**********************************/
/*       Start file parser        */
/*    Original source by PhilZ    */
/**********************************/
// todo: parse settings file in one pass and make pairs of key:value
// get value of key from a given config file
// always call with value[PROPERTY_VALUE_MAX] to prevent any buffer overflow caused by strcpy(value, strstr(line, "=") + 1);
int read_config_file(const char* config_file, const char *key, char *value, const char *value_def) {
    int ret = 0;
    char line[PROPERTY_VALUE_MAX];
    ensure_path_mounted(config_file);
    FILE *fp = fopen(config_file, "rb");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strstr(line, key) != NULL && strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
                // we found the key: try to get its value, remove trailing \n and ensure it is not an empty value
                strcpy(value, strstr(line, "=") + 1);
                if (value[strlen(value)-1] == '\n')
                    value[strlen(value)-1] = '\0';
                if (value[0] != '\0') {
                    fclose(fp);
                    LOGI("%s=%s\n", key, value);
                    return ret;
                }
            }
        }
        // either we didn't find the key or it has an empty value
        ret = 1;
        fclose(fp);
    } else {
        LOGI("Cannot open %s\n", config_file);
        ret = -1;
    }

    // set value to default
    strcpy(value, value_def);
    LOGI("%s set to default (%s)\n", key, value_def);
    return ret;
}

// set value of key in config file
int write_config_file(const char* config_file, const char* key, const char* value) {
    if (ensure_path_mounted(config_file) != 0) {
        LOGE("Cannot mount path for settings file: %s\n", config_file);
        return -1;
    }

    char config_file_tmp[PATH_MAX];
    char tmp[PATH_MAX];
    sprintf(config_file_tmp, "%s.tmp", config_file);
    sprintf(tmp, "%s", DirName(config_file_tmp));
    ensure_directory(tmp, 0755);
    delete_a_file(config_file_tmp);

    FILE *f_tmp = fopen(config_file_tmp, "wb");
    if (f_tmp == NULL) {
        LOGE("failed to create temporary settings file!\n");
        return -1;
    }

    FILE *fp = fopen(config_file, "rb");
    if (fp == NULL) {
        // we need to create a new settings file: write an info header
        const char* header[] = {
            "#PhilZ Touch Settings File\n",
            "#Edit only in appropriate UNIX format (Notepad+++...)\n",
            "#Entries are in the form of:\n",
            "#key=value\n",
            "#Do not add spaces in between!\n",
            "\n",
            NULL
        };

        int i;
        for(i = 0; header[i] != NULL; i++) {
            fwrite(header[i], 1, strlen(header[i]), f_tmp);
        }
    } else {
        // parse existing config file and write new temporary file.
        char line[PROPERTY_VALUE_MAX];
        while (fgets(line, sizeof(line), fp) != NULL) {
            // ignore any existing line with key we want to set
            if (strstr(line, key) != NULL && strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=')
                continue;
            // ensure trailing \n, in case some one got a bad editor...
            if (line[strlen(line) - 1] != '\n')
                strcat(line, "\n");
            fwrite(line, 1, strlen(line), f_tmp);
        }
        fclose(fp);
    }

    // write new key=value entry
    char new_entry[PROPERTY_VALUE_MAX];
    sprintf(new_entry, "%s=%s\n", key, value);
    fwrite(new_entry, 1, strlen(new_entry), f_tmp);
    fclose(f_tmp);

    if (rename(config_file_tmp, config_file) != 0) {
        LOGE("failed to rename temporary settings file!\n");
        return -1;
    }

    // if we are editing recovery settings file, create a second copy on primary storage
    if (strcmp(PHILZ_SETTINGS_FILE, config_file) == 0) {
        sprintf(tmp, "%s/%s", get_primary_storage_path(), PHILZ_SETTINGS_FILE2);
        if (copy_a_file(config_file, tmp) != 0)
            LOGE("failed duplicating settings file to primary storage!\n");
    }

    LOGI("%s was set to %s\n", key, value);
    return 0;
}
//----- end file settings parser
