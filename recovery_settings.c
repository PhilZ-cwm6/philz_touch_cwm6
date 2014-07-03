#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>

// statfs
#include <sys/vfs.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "install.h"
#include "make_ext4fs.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "extendedcommands.h"
#include "advanced_functions.h"
#include "recovery_settings.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"
#include "bmlutils/bmlutils.h"
#include "cutils/android_reboot.h"


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

#ifndef BOARD_HAS_NO_FB2PNG
#define board_use_fb2png                1
#else
#define board_use_fb2png                0
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
    board_use_fb2png,
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

 Function is called when rebooting from recovery
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
static void check_signature_check() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, signature_check_enabled.key, value, "0");
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
        signature_check_enabled.value = 1;
    else
        signature_check_enabled.value = 0;
}

// verify md5sum of zip file before they are installed
static void check_install_zip_verify_md5() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, install_zip_verify_md5.key, value, "0");
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
        install_zip_verify_md5.value = 1;
    else
        install_zip_verify_md5.value = 0;
}

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
    if (on_start) {
        preserve_data_media(0);
        ensure_path_unmounted(PHILZ_SETTINGS_FILE);
        preserve_data_media(1);
    }
}
