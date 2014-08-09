/*
    PhilZ Touch - touch_gui library
    Copyright (C) <2014>  <phytowardt@gmail.com>

    This file is part of PhilZ Touch Recovery

    PhilZ Touch is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PhilZ Touch is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PhilZ Touch.  If not, see <http://www.gnu.org/licenses/>.

*/

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

#include "libcrecovery/common.h"

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "install.h"
#include "make_ext4fs.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"
#include "ui.h"
#include "extendedcommands.h"
#include "advanced_functions.h"
#include "recovery_settings.h"
#include "nandroid.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include "mtdutils/mtdutils.h"
#include "bmlutils/bmlutils.h"
#include "cutils/android_reboot.h"

#include "libtouch_gui/gui_settings.h"
#include "libtouch_gui/touch_gui.h"

/*********************************/
/*  PhilZ Touch GUI Preferences  */
/*********************************/

// Start initialize touch recovery key/value settings
struct CWMSettingsLongIntValues set_brightness = { "set_brightness", BRIGHTNESS_DEFAULT_VALUE };
struct CWMSettingsLongIntValues menu_height_increase = { "menu_height_increase", MENU_HEIGHT_INCREASE_INIT }; // initialize with constant
struct CWMSettingsLongIntValues min_log_rows = { "min_log_rows", 3 };
struct CWMSettingsIntValues show_background_icon = { "show_background_icon", 0 };
struct CWMSettingsCharValues background_image = { "background_image", "default" };
struct CWMSettingsLongIntValues header_text_color = { "header_text_color", DEFAULT_HEADER_TEXT_COLOR };
struct CWMSettingsLongIntValues menu_text_color = { "menu_text_color", DEFAULT_MENU_TEXT_COLOR };
struct CWMSettingsLongIntValues normal_text_color = { "normal_text_color", DEFAULT_NORMAL_TEXT_COLOR };
struct CWMSettingsLongIntValues menu_background_color = { "menu_background_color", DEFAULT_MENU_BACKGROUND_COLOR };
struct CWMSettingsLongIntValues menu_background_transparency = { "menu_background_transparency", 0 }; // unused value: mbackg_code[ALPHA_CHANNEL]
struct CWMSettingsLongIntValues menu_highlight_color = { "menu_highlight_color", DEFAULT_MENU_HIGHLIGHT_COLOR };
struct CWMSettingsLongIntValues menu_highlight_transparency = { "menu_highlight_transparency", 0 }; // unused value: mhlight_code[ALPHA_CHANNEL]
struct CWMSettingsLongIntValues menu_separator_color = { "menu_separator_color", DEFAULT_MENU_SEPARATOR_COLOR };
struct CWMSettingsIntValues menu_separator_transparency = { "menu_separator_transparency", 0 }; // unused value: mseparator_code[ALPHA_CHANNEL]
struct CWMSettingsIntValues show_menu_separation = { "show_menu_separation", 1 };
struct CWMSettingsIntValues show_virtual_keys = { "show_virtual_keys", 0 };
struct CWMSettingsIntValues show_clock = { "show_clock", 1 };
struct CWMSettingsIntValues show_battery = { "show_battery", 1 };
struct CWMSettingsLongIntValues batt_clock_color = { "batt_clock_color", DEFAULT_BATT_CLOCK_COLOR };
struct CWMSettingsCharValues brightness_user_path = { "brightness_user_path", "" };
struct CWMSettingsLongIntValues dim_timeout = { "dim_timeout", 60 };
struct CWMSettingsLongIntValues blank_timeout = { "blank_timeout", 180 };

struct CWMSettingsIntValues touch_to_validate = { "touch_to_validate", FULL_TOUCH_VALIDATION };
struct CWMSettingsLongIntValues scroll_sensitivity = { "scroll_sensitivity", SCROLL_SENSITIVITY_INIT };
struct CWMSettingsLongIntValues touch_accuracy = { "touch_accuracy", TOUCH_ACCURACY_INIT };
struct CWMSettingsLongIntValues slide_left_action = { "slide_left_action", 0 };
struct CWMSettingsLongIntValues slide_right_action = { "slide_right_action", 0 };
struct CWMSettingsLongIntValues double_tap_action = { "double_tap_action", 0 };
struct CWMSettingsLongIntValues press_lift_action = { "press_lift_action", 0 };
struct CWMSettingsLongIntValues press_move_action = { "press_move_action", 0 };
struct CWMSettingsIntValues enable_vibrator = { "enable_vibrator", 1 };
struct CWMSettingsIntValues wait_after_install = { "wait_after_install", 1 };

// time settings
struct CWMSettingsLongIntValues t_zone = { "t_zone", 0 };
struct CWMSettingsLongIntValues t_zone_offset = { "t_zone_offset", 0 };
struct CWMSettingsIntValues use_dst_time = { "use_dst_time", 0 };
struct CWMSettingsIntValues use_qcom_time_data_files = { "use_qcom_time_data_files", 0 };
struct CWMSettingsIntValues use_qcom_time_daemon = { "use_qcom_time_daemon", 0 };
struct CWMSettingsLongIntValues use_qcom_time_offset = { "use_qcom_time_offset", 0 };

//----- End initialize touch recovery key/value settings


// color code arrays are {blue,green,red,alpha}
// initialize default color codes except alpha channel. Alpha transparancy defaults are initialized on recovery start call to parse_gui_colors()
int mtext_code[4] = {DEFAULT_MENU_TEXT_CODE};
int mbackg_code[4] = {DEFAULT_MENU_BACKGROUND_CODE};
int mhlight_code[4] = {DEFAULT_MENU_HIGHLIGHT_CODE};
int normal_text_code[4] = {DEFAULT_NORMAL_TEXT_CODE};
int mseparator_code[4] = {DEFAULT_MENU_SEPARATOR_CODE};
int header_text_code[4] = {DEFAULT_HEADER_TEXT_CODE};
int batt_clock_code[4] = {DEFAULT_BATT_CLOCK_CODE};

// dim and blank screen
static long int max_brightness_value = 255;
bool is_blanked = 0;
bool is_dimmed = 0;

// toggle friendly log view during install_zip()
// on start, bypass user settings: do not wait after boot install scripts
int force_wait = -1;


/**************************************************************/
/* Handling of gui menus and touch key actions while in menus */
/**************************************************************/
//in recovery.c / get_menu_selection()
//  * the gui menu is drawn on screen by ui_start_menu()
//  * ui_wait_key() in ui.c will pause screen waiting for a key
//  * ui_handle_key() in ui.c will call device_handle_key() from philz_keys_s2.c
//  * device_handle_key() will read the key code and return an action value we assigned to the key
//  * back to recovery.c / get_menu_selection(), the returned "action" is converted to either chosen_item or another action to do
//  * to leave get_menu_selection() we need to break the while loop, that is chosen item must be an action (positive integer) or GO_BACK
//  * REFRESH return code can also exit get_menu_selection. REFRESH is when we insert a new usb drive since cm-10.2 (vold managed devices)
//  * we check for REFRESH return code when we use a menu that lists vold managed devices, that way we refresh them when a device is inserted
//  * if we press a non assigned key, the switch reaches the end and we remain in the while loop: menu is not refreshed
//  * if we press a key recognized by switch (action), an action occur and we have opportunity to set up an exit from while loop
//  * before leaving get_menu_selection(), the menu on screen is removed by ui_end_menu() and any key press is removed by ui_clear_key_queue()
//  * chosen_item is returned to caller of get_menu_selection()
//  * the caller applies action, and again, we'll have a menu on screen when get_menu_selection() is called
//  * ui_wait_key() will trigger and stop watching as soon as any key is pressed.
//  * if the pressed key doesn't match any action in device_handle_key(), NO_ACTION is returned
//  * on NO_ACTION we remain in the while loop and do not exit get_menu_selection()

// Following above logic, we can decompose our gesture actions behavior while we are in a menu
// actions to launch are added and incremented in below #defines: SCREEN_CAPTURE_ACTION, AROMA_BROWSER_ACTION...
// MAX_DEFINED_ACTIONS is the total number of defined actions, first one being 1
// philz_touch_gestures.c:
//  * one key (KEY_LEFTBRACE) will handle all gesture defined movements
//  * the int key_gesture is assigned the touch gesture code (SLIDE_LEFT_GESTURE, SLIDE_RIGHT_GESTURE or DOUBLE_TAP_GESTURE)
// in the active menu, get_menu_selection() / ui_wait_key() stop watching for a key as it detected KEY_LEFTBRACE
// ui_handle_key() calls device_handle_key() in philz_keys_s2.c, and KEY_LEFTBRACE will return GESTURE_ACTIONS code (defined in recovery_ui.h)
// back to recovery.c / get_menu_selection(), action = GESTURE_ACTIONS will launch handle_gesture_actions() and set chosen_item = GESTURE_ACTIONS
// handle_gesture_actions() is launched while menu is still showing on screen, since ui_end_menu() was not yet called
// handle_gesture_actions() will read key_gesture value assigned above and run the action associated to the gesture
// now, if we need to run an action in full screen without the menu showing (aroma start, show log...) we must call ui_end_menu()
// ui_end_menu() will remove menus from screen
// once our action is done, we are back to recovery.c / get_menu_selection() and there is a break after handle_gesture_actions() was launched
// we can exit the while loop since we setup chosen_item = GESTURE_ACTIONS and added the exclude condition in the while arguments
// ui_end_menu() is called a second time if already called in actions like aroma or show log.
// it won't do anything because show_menu was set to 0 on previous call to ui_end_menu()
// ui_clear_key_queue() is also called after that and get_menu_selection() returns chosen_item
// our original menu will then be drawn again on screen like if we just pressed a non valid key
//exception in some menus calling directly get_menu_selection() and not get_filtered_menu_selection()
// these will exit and Go back to previous menu as the switch reaches the end

// the long integers (slide_left_action.value, slide_right_action.value...) are assigned the action number to launch
// default action values are defined in check_gesture_actions()
// new entries are added in << Start touch gesture actions >> section
// while key_gesture = 0, allow real KEY_LEFTBRACE default action (think at it when changing DISABLE_ACTION value)
#define DISABLE_ACTION            0 // check comment above if this value is changed from 0
#define SCREEN_CAPTURE_ACTION     1
#define AROMA_BROWSER_ACTION      2
#define ADJUST_BRIGHTNESS_ACTION  3
#define SHOW_LOG_ACTION           4
#define BLANK_SCREEN_ACTION       5
#define MAX_DEFINED_ACTIONS       5
int key_gesture = 0;

void selective_load_theme_settings() {
    const char* header_choose[] = {
        "Select a theme to load",
        "",
        NULL
    };

    const char* headers[] = {
        "Select settings to load from theme",
        "",
        NULL
    };

    char* list[] = {
        "Load all recovery settings",
        "Load only GUI settings",
        NULL
    };

    char themes_dir[PATH_MAX];
    char* theme_file;

    sprintf(themes_dir, "%s/%s", get_primary_storage_path(), PHILZ_THEMES_PATH);
    if (0 != ensure_path_mounted(themes_dir))
        return;

    theme_file = choose_file_menu(themes_dir, ".ini", header_choose);
    if (theme_file == NULL)
        return;

    for(;;) {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item < 0) // GO_BACK or REFRESH
            break;

        switch (chosen_item) {
            case 0: {
                if (confirm_selection("Overwrite all settings ?", "Yes - Apply New Theme") && copy_a_file(theme_file, PHILZ_SETTINGS_FILE) == 0) {
                    char settings_copy[PATH_MAX];
                    sprintf(settings_copy, "%s/%s", get_primary_storage_path(), PHILZ_SETTINGS_FILE2);
                    copy_a_file(theme_file, settings_copy);
                    refresh_recovery_settings(0);
                    ui_print("loaded default settings from %s\n", BaseName(theme_file));
                }
                break;
            }
            case 1: {
                // start selectively loading gui settings from theme
                // do not overwrite all current settings file
                const char *gui_settings[] = THEME_GUI_SETTINGS;
                char value[PROPERTY_VALUE_MAX];
                int i;
                for(i = 0; gui_settings[i] != NULL; ++i) {
                    read_config_file(theme_file, gui_settings[i], value, "");
                    if (strcmp(value, "") == 0)
                        continue;

                    write_config_file(PHILZ_SETTINGS_FILE, gui_settings[i], value);
                }
                refresh_recovery_settings(0);
                ui_print("loaded default settings from %s\n", BaseName(theme_file));
                break;
            }
        }
    }

    free(theme_file);
}

static void check_clock_enabled() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, show_clock.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        show_clock.value = 0;
    else
        show_clock.value = 1;
}

static void check_battery_enabled() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, show_battery.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        show_battery.value = 0;
    else
        show_battery.value = 1;
}

static void check_enable_vibrator() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, enable_vibrator.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        enable_vibrator.value = 0;
    else
        enable_vibrator.value = 1;
}

static void check_boardEnableKeyRepeat() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, boardEnableKeyRepeat.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        boardEnableKeyRepeat.value = 0;
    else
        boardEnableKeyRepeat.value = 1;
}

static void check_wait_after_install() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, wait_after_install.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        wait_after_install.value = 0;
    else
        wait_after_install.value = 1;
}

static void check_menu_height() {
    char value[PROPERTY_VALUE_MAX];
    char value_def[5];
    sprintf (value_def, "%d", MENU_HEIGHT_INCREASE_0);
    read_config_file(PHILZ_SETTINGS_FILE, menu_height_increase.key, value, value_def);
    menu_height_increase.value = strtol(value, NULL, 10);
    if (menu_height_increase.value < MENU_HEIGHT_INCREASE_MIN || menu_height_increase.value > MENU_HEIGHT_INCREASE_MAX)
        menu_height_increase.value = MENU_HEIGHT_INCREASE_0;
}

static void check_scroll_sensitivity() {
    char value[PROPERTY_VALUE_MAX];
    char value_def[5];
    sprintf (value_def, "%ld", SCROLL_SENSITIVITY_0);
    read_config_file(PHILZ_SETTINGS_FILE, scroll_sensitivity.key, value, value_def);
    scroll_sensitivity.value = strtol(value, NULL, 10);
    if (scroll_sensitivity.value < SCROLL_SENSITIVITY_MIN || scroll_sensitivity.value > SCROLL_SENSITIVITY_MAX)
        scroll_sensitivity.value = SCROLL_SENSITIVITY_0;
}

static void check_touch_accuracy() {
    char value[PROPERTY_VALUE_MAX];
    char value_def[5];
    sprintf (value_def, "%d", TOUCH_ACCURACY_0);
    read_config_file(PHILZ_SETTINGS_FILE, touch_accuracy.key, value, value_def);
    touch_accuracy.value = strtol(value, NULL, 10);
    if (touch_accuracy.value < 1 || touch_accuracy.value > 11)
        touch_accuracy.value = TOUCH_ACCURACY_0;
}

static void check_touch_to_validate() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, touch_to_validate.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "-1") == 0)
        touch_to_validate.value = NO_TOUCH_SUPPORT; // -1
    else if (strcmp(value, "highlight") == 0 || strcmp(value, "0") == 0)
        touch_to_validate.value = TOUCH_HIGHLIGHT_ONLY; // 0
    else if (strcmp(value, "double_tap") == 0 || strcmp(value, "2") == 0)
        touch_to_validate.value = DOUBLE_TAP_VALIDATION; // 2
    else
        touch_to_validate.value = FULL_TOUCH_VALIDATION; // 1
}

static void check_virtual_keys() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, show_virtual_keys.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        show_virtual_keys.value = 0;
    else
        show_virtual_keys.value = 1;
}

static void check_min_log_rows() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, min_log_rows.key, value, "3");
    min_log_rows.value = strtol(value, NULL, 10);
    if (min_log_rows.value < 3 || min_log_rows.value > 6)
        min_log_rows.value = 3;
}

static void check_menu_separation() {
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, show_menu_separation.key, value, "true");
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        show_menu_separation.value = 0;
    else
        show_menu_separation.value = 1;
}

static void check_gesture_actions() {
    char value[PROPERTY_VALUE_MAX];
    char value_def[5];

    // slide left action
    sprintf (value_def, "%d", BLANK_SCREEN_ACTION);
    read_config_file(PHILZ_SETTINGS_FILE, slide_left_action.key, value, value_def);
    slide_left_action.value = strtol(value, NULL, 10);
    if (slide_left_action.value < 0 || slide_left_action.value > MAX_DEFINED_ACTIONS)
        slide_left_action.value = DISABLE_ACTION;

    // slide right action
    sprintf (value_def, "%d", ADJUST_BRIGHTNESS_ACTION);
    read_config_file(PHILZ_SETTINGS_FILE, slide_right_action.key, value, value_def);
    slide_right_action.value = strtol(value, NULL, 10);
    if (slide_right_action.value < 0 || slide_right_action.value > MAX_DEFINED_ACTIONS)
        slide_right_action.value = DISABLE_ACTION;

    // double tap action
    sprintf (value_def, "%d", AROMA_BROWSER_ACTION);
    read_config_file(PHILZ_SETTINGS_FILE, double_tap_action.key, value, value_def);
    double_tap_action.value = strtol(value, NULL, 10);
    if (double_tap_action.value < 0 || double_tap_action.value > MAX_DEFINED_ACTIONS)
        double_tap_action.value = DISABLE_ACTION;

    // press 1 sec then lift finger action
    sprintf (value_def, "%d", SHOW_LOG_ACTION);
    read_config_file(PHILZ_SETTINGS_FILE, press_lift_action.key, value, value_def);
    press_lift_action.value = strtol(value, NULL, 10);
    if (press_lift_action.value < 0 || press_lift_action.value > MAX_DEFINED_ACTIONS)
        press_lift_action.value = DISABLE_ACTION;

    // press 2 sec and small move finger action
    sprintf (value_def, "%d", SCREEN_CAPTURE_ACTION);
    read_config_file(PHILZ_SETTINGS_FILE, press_move_action.key, value, value_def);
    press_move_action.value = strtol(value, NULL, 10);
    if (press_move_action.value < 0 || press_move_action.value > MAX_DEFINED_ACTIONS)
        press_move_action.value = DISABLE_ACTION;
}

// Passing a negative dim_value, will read config file and apply user set brightness (on recovery start)
// Writing set_brightness.value to config file is done directly in the toggle brightness menu
// Passing a non negative dim_value, will apply dim_value brightness level
void apply_brightness_value(long int dim_value) {
    if (strlen(libtouch_flags.brightness_sys_file) == 0) {
        // no file was defined during compile time, get the value from settings file if it exists
        read_config_file(PHILZ_SETTINGS_FILE, brightness_user_path.key, brightness_user_path.value, "no_file");
        strcpy(libtouch_flags.brightness_sys_file, brightness_user_path.value);
    }

    if (strcmp(libtouch_flags.brightness_sys_file, "no_file") == 0) {
        // no file was defined during compile and we have none in settings file
        // try to search for it in pre-defined paths. If we find one, we save it to settings for next boot
        char* brightness_path = find_file_in_path("/sys/class/backlight", "brightness", 0, 0);
        if (brightness_path == NULL)
            brightness_path = find_file_in_path("/sys/class/leds/lcd-backlight", "brightness", 0, 0);
        if (brightness_path != NULL) {
            strcpy(libtouch_flags.brightness_sys_file, brightness_path);
            snprintf(brightness_user_path.value, sizeof(brightness_user_path.value), "%s", brightness_path);
            write_config_file(PHILZ_SETTINGS_FILE, brightness_user_path.key, brightness_user_path.value);
            free(brightness_path);
        } else {
            strcpy(libtouch_flags.brightness_sys_file, "none");
        }
    }

    if (strcmp(libtouch_flags.brightness_sys_file, "none") == 0) {
        // no brightness path was defined during compile time
        // none is specified in user settings
        // we found no brightness file on start
        LOGI("No brightness file found: function disabled\n");
        return;
    }

    if (dim_value < 0) {
        // recovery start
        // first get the device maximum brightness value if it exists
        char path[PATH_MAX];
        char value[PROPERTY_VALUE_MAX];
        char value_def[5];

        sprintf(path, "%s/max_brightness", DirName(libtouch_flags.brightness_sys_file));
        FILE *f = fopen(path, "r");
        if (f != NULL) {
            int max = 0;
            fscanf(f, "%d", &max);
            fclose(f);
            if (max) max_brightness_value = max;
        }

        // read config file and load brightness value
        sprintf (value_def, "%d", BRIGHTNESS_DEFAULT_VALUE);
        read_config_file(PHILZ_SETTINGS_FILE, set_brightness.key, value, value_def);
        set_brightness.value = strtol(value, NULL, 10);
        if (set_brightness.value < 10)
            set_brightness.value = 10;
        else if (set_brightness.value > max_brightness_value)
            set_brightness.value = max_brightness_value;

        dim_value = set_brightness.value;
    }

    // apply user set brightness
    FILE *file = fopen(libtouch_flags.brightness_sys_file, "w");
    if (file == NULL) {
        LOGE("Unable to create brightness sys file!\n");
        return;
    }

    fprintf(file, "%ld\n", dim_value);
    fclose(file);    
}

static void toggle_brightness() {
    char value[10];
    if (set_brightness.value >= max_brightness_value) {
        set_brightness.value = 10;
    } else {
        set_brightness.value += (max_brightness_value / 7);
        if (set_brightness.value > max_brightness_value)
            set_brightness.value = max_brightness_value;
    }

    sprintf(value, "%ld", set_brightness.value);
    write_config_file(PHILZ_SETTINGS_FILE, set_brightness.key, value);
    apply_brightness_value(set_brightness.value);
}

static void check_dim_blank_timer() {
    char value[PROPERTY_VALUE_MAX];

    // get dim_timeout.key value in seconds
    read_config_file(PHILZ_SETTINGS_FILE, dim_timeout.key, value, "60");
    dim_timeout.value = strtol(value, NULL, 10);
    if (dim_timeout.value < 0)
        dim_timeout.value = 0;
    else if (dim_timeout.value > 300)
        dim_timeout.value = 300;

    // get blank_timeout.key value in seconds
    read_config_file(PHILZ_SETTINGS_FILE, blank_timeout.key, value, "180");
    blank_timeout.value = strtol(value, NULL, 10);
    if (blank_timeout.value < 0)
        blank_timeout.value = 0;
    else if (blank_timeout.value > 1800)
        blank_timeout.value = 1800;
}

/* Start check and apply time zone */
//
// "(UTC -11) Samoa, Midway Island">BST11;BDT
// "(UTC -10) Hawaii"                          HST10;HDT
// "(UTC -9) Alaska"                           AST9;ADT
// "(UTC -8) Pacific Time"                     PST8;PDT
// "(UTC -7) Mountain Time"                    MST7;MDT
// "(UTC -6) Central Time"                     CST6;CDT
// "(UTC -5) Eastern Time"                     EST5;EDT
// "(UTC -4) Atlantic Time"                    AST4;ADT
// "(UTC -3) Brazil, Buenos Aires"             GRNLNDST3;GRNLNDDT
// "(UTC -2) Mid-Atlantic"                     FALKST2;FALKDT
// "(UTC -1) Azores, Cape Verde"               AZOREST1;AZOREDT
// "(UTC  0) London, Dublin, Lisbon"           GMT0;BST
// "(UTC +1) Berlin, Brussels, Paris"          NFT-1;DFT
// "(UTC +2) Athens, Istanbul, South Africa"   WET-2;WET
// "(UTC +3) Moscow, Baghdad"                  SAUST-3;SAUDT
// "(UTC +4) Abu Dhabi, Tbilisi, Muscat"       WST-4;WDT
// "(UTC +5) Yekaterinburg, Islamabad"         PAKST-5;PAKDT
// "(UTC +6) Almaty, Dhaka, Colombo"           TASHST-6;TASHDT
// "(UTC +7) Bangkok, Hanoi, Jakarta"          THAIST-7;THAIDT
// "(UTC +8) Beijing, Singapore, Hong Kong"    TAIST-8;TAIDT
// "(UTC +9) Tokyo, Seoul, Yakutsk"            JST-9;JSTDT
// "(UTC +10) Eastern Australia, Guam"         EET-10;EETDT
// "(UTC +11) Vladivostok, Solomon Islands"    MET-11;METDT
// "(UTC +12) Auckland, Wellington, Fiji"      NZST-12;NZDT

// write current system time to log
static void log_current_system_time() {
    char time_string[50];
    struct tm *timeptr;
    time_t seconds;
    seconds = time(NULL);
    timeptr = localtime(&seconds);

    if (timeptr != NULL) {
        strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", timeptr);
        LOGI("Current time: %s (UTC %s%ld:%02ld) %s\n", time_string, t_zone.value <= 0 ? "" : "+",
                t_zone.value, t_zone_offset.value, use_dst_time.value ? "DST" : "");
    } else {
        LOGI("Current time: localtime() error\n");
    }
}

// set_system_time() is called from apply_time_zone() or when setting use_dst_time.value and t_zone_offset.value in menus
// on start, it is called from apply_time_zone()
// it applies the time zone environment variables
static void set_system_time() {
    char* utc_t_zone_list[] = {
        "BST11;BDT",
        "HST10;HDT",
        "AST9;ADT",
        "PST8;PDT",
        "MST7;MDT",
        "CST6;CDT",
        "EST5;EDT",
        "AST4;ADT",
        "GRNLNDST3;GRNLNDDT",
        "FALKST2;FALKDT",
        "AZOREST1;AZOREDT",
        "GMT0;BST",
        "NFT-1;DFT",
        "WET-2;WET",
        "SAUST-3;SAUDT",
        "WST-4;WDT",
        "PAKST-5;PAKDT",
        "TASHST-6;TASHDT",
        "THAIST-7;THAIDT",
        "TAIST-8;TAIDT",
        "JST-9;JSTDT",
        "EET-10;EETDT",
        "MET-11;METDT",
        "NZST-12;NZDT",
        NULL
    };

    // parse to get time zone
    char time_string[50];
    char t_zone_string[50];
    char dst_string[50];
    char *ptr;
    strcpy(time_string, utc_t_zone_list[t_zone.value + 11]);
    ptr = strtok(time_string, ";");
    strcpy(t_zone_string, ptr);
    ptr = strtok(NULL, ";");
    strcpy(dst_string, ptr);
    if (t_zone_offset.value != 0) {
        char offset[10];
        sprintf(offset, ":%ld", t_zone_offset.value);
        strcat(t_zone_string, offset);
    }
    if (use_dst_time.value)
        strcat(t_zone_string, dst_string);

    // apply time through TZ environment variable
    setenv("TZ", t_zone_string, 1);
    tzset();

    // log current system time
    log_current_system_time();
}

// Apply Time Zone
// called on recovery start or when changing time zone in menu
// on recovery start, it will read and set t_zone.value, t_zone_offset.value and use_dst_time.value values
// when called from time zone menu, it will only write and apply t_zone.value
// system time is set when calling set_system_time()
static void apply_time_zone(int write_cfg, int tz) {
    if (!write_cfg) {
        // called on recovery start, read values and apply time
        // read user config for t_zone.value in UTC hours offset
        char value[PROPERTY_VALUE_MAX];
        read_config_file(PHILZ_SETTINGS_FILE, t_zone.key, value, "0");
        t_zone.value = strtol(value, NULL, 10);
         // if no interger is found, t_zone.value = 0, no error check needed on strtol
        if (t_zone.value > 12 || t_zone.value < -11)
            t_zone.value = 0;

        // read user config for timezone offset in minutes
        read_config_file(PHILZ_SETTINGS_FILE, t_zone_offset.key, value, "0");
        t_zone_offset.value = strtol(value, NULL, 10);
        if (t_zone_offset.value >= 60 || t_zone_offset.value < 0)
            t_zone_offset.value = 0;

        // read dst time settings
        read_config_file(PHILZ_SETTINGS_FILE, use_dst_time.key, value, "false");
        if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
            use_dst_time.value = 1;
        else
            use_dst_time.value = 0;
    } else {
        // called from show_time_settings_menu(): apply user set time zone
        char value[4];
        t_zone.value = tz;
        sprintf(value, "%ld", t_zone.value);
        write_config_file(PHILZ_SETTINGS_FILE, t_zone.key, value);
    }

    // effectively apply time zones to system
    set_system_time();
}

/* Start Qualcom Time Fixes */
// parse the time daemon data files (credits to TeamWin)
static void parse_t_daemon_data_files() {
    // Devices with Qualcomm Snapdragon 800 do some shenanigans with RTC.
    // They never set it, it just ticks forward from 1970-01-01 00:00,
    // and then they have files /data/system/time/ats_* with 64bit offset
    // in miliseconds which, when added to the RTC, gives the correct time.
    // So, the time is: (offset_from_ats + value_from_RTC)
    // There are multiple ats files, they are for different systems? Bases?
    // Like, ats_1 is for modem and ats_2 is for TOD (time of day?).
    // Look at file time_genoff.h in CodeAurora, qcom-opensource/time-services

    const char *paths[] = {"/data/system/time/", "/data/time/"};
    char ats_path[PATH_MAX] = "";
    DIR *d;
    FILE *f;
    uint64_t offset = 0;
    struct timeval tv;
    struct dirent *dt;

    // Don't fix the time of it already is over year 2000, it is likely already okay, either
    // because the RTC is fine or because the recovery already set it and then crashed
    gettimeofday(&tv, NULL);
    if (tv.tv_sec > 946684800) {
        // timestamp of 2000-01-01 00:00:00
        LOGE("parse_t_daemon_data_files: time already okay (after year 2000).\n");
        return;
    }

    // on start, /data will be unmounted by refresh_recovery_settings()
    if (ensure_path_mounted("/data") != 0) {
        LOGE("parse_t_daemon_data_files: failed to mount /data\n");
        return;
    }

    // Prefer ats_2, it seems to be the one we want according to logcat on hammerhead
    // - it is the one for ATS_TOD (time of day?).
    // However, I never saw a device where the offset differs between ats files.
    size_t i;
    for (i = 0; i < (sizeof(paths)/sizeof(paths[0])); ++i) {
        DIR *d = opendir(paths[i]);
        if (!d)
            continue;

        while ((dt = readdir(d))) {
            if (dt->d_type != DT_REG || strncmp(dt->d_name, "ats_", 4) != 0)
                continue;

            if (strlen(ats_path) == 0 || strcmp(dt->d_name, "ats_2") == 0)
                sprintf(ats_path, "%s%s", paths[i], dt->d_name);
        }

        closedir(d);
    }

    if (strlen(ats_path) == 0) {
        LOGE("parse_t_daemon_data_files: no ats files found, leaving time as-is!\n");
        return;
    }

    f = fopen(ats_path, "r");
    if (!f) {
        LOGE("parse_t_daemon_data_files: failed to open file %s\n", ats_path);
        return;
    }

    if (fread(&offset, sizeof(offset), 1, f) != 1) {
        LOGE("parse_t_daemon_data_files: failed load uint64 from file %s\n", ats_path);
        fclose(f);
        return;
    }
    fclose(f);

    LOGI("parse_t_daemon_data_files: Setting time offset from file %s, offset %llu\n", ats_path, offset);

    gettimeofday(&tv, NULL);
    tv.tv_sec += offset / 1000;
    tv.tv_usec += (offset % 1000) * 1000;

    while(tv.tv_usec >= 1000000) {
        ++tv.tv_sec;
        tv.tv_usec -= 1000000;
    }

    settimeofday(&tv, NULL);
    log_current_system_time();
}

// load the qcom time_daemon
static void load_qcom_time_daemon(int on_start) {
    // unmount of /system + /data must be done in __system() or in source code after a sleep() delay. Else, they are unmounted while time_daemon is not done
    // only unmount partitions on recovery start
    if (!file_found("/system/bin/time_daemon") ||
        (!file_found("/data/time") && !file_found("/data/system/time"))) {
        LOGE("can't find time daemon\n");
        ensure_path_unmounted("/system");
        ensure_path_unmounted("/data");
    } else {
        // at this point, if on_start, show_text == 0 and the log prints are not shown on screen
        if (on_start)
            draw_visible_text_line(5, "starting time daemon...", 1);
        else
            ui_print("starting time daemon...\n");

        char cmd[PATH_MAX];
        sprintf(cmd, "export LD_LIBRARY_PATH=/system/vendor/lib:/system/lib; /system/bin/time_daemon &(sleep 2; killall time_daemon;%s) &",
                on_start ? " /sbin/umount /system; /sbin/umount /data" : "");
        __system(cmd);
        // sleep 2.5 secs, else on recovery start, refresh_recovery_settings() will unmount /data too early
        //  - time_daemon may not sync time correctly
        //  - log_current_system_time() will not have real new time
        //  - we could reach the refresh_recovery_settings() before /data is unmounted by __system() call,
        //    refresh_recovery_settings() will unmount /data
        //    when finally __system() call tries to umount /data, we get a log error: "umount: can't umount /data: Invalid argument"
        sleep(2);
        usleep(500000);
        log_current_system_time();
    }
}

// apply qcom time rtc offset
// only apply on recovery start
// else, this will increment the time by given offset when applied multiple times from menu
static void apply_qcom_rtc_offset(int on_start) {
    if (!on_start)
        return;

    if (use_qcom_time_offset.value > 0) {
        LOGI("applying rtc time offset...\n");
        FILE *f = fopen("/sys/class/rtc/rtc0/since_epoch", "r");
        if (f != NULL) {
            struct timeval tv;
            long int rtc_offset;
            fscanf(f, "%ld", &rtc_offset);
            fclose(f);
            tv.tv_sec = rtc_offset + use_qcom_time_offset.value;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
            log_current_system_time();
        }
    }
}

// first check if we must directly parse time data files
// if not, try to directly load the time_daemon service
// started on recovery start or from menu
static void apply_qcom_time_daemon_fixes(int on_start) {
    if (on_start) {
        // called on recovery start, no need to parse settings file when called from menus
        char value[PROPERTY_VALUE_MAX];
        read_config_file(PHILZ_SETTINGS_FILE, use_qcom_time_daemon.key, value, "0");
        if (strcmp(value, "1") == 0)
            use_qcom_time_daemon.value = 1;
        else
            use_qcom_time_daemon.value = 0;

        read_config_file(PHILZ_SETTINGS_FILE, use_qcom_time_data_files.key, value, "0");
        if (strcmp(value, "1") == 0 || strcmp(value, "true") == 0)
            use_qcom_time_data_files.value = 1;
        else
            use_qcom_time_data_files.value = 0;

        read_config_file(PHILZ_SETTINGS_FILE, use_qcom_time_offset.key, value, "0");
        use_qcom_time_offset.value = strtol(value, NULL, 10);
        if (use_qcom_time_offset.value < 0)
            use_qcom_time_offset.value = 0;
    }

    // Only allow one qcom time_daemon fix (this should never happen unless user manually alters settings file)
    if (use_qcom_time_data_files.value) {
        parse_t_daemon_data_files();
    } else if (use_qcom_time_daemon.value) {
        load_qcom_time_daemon(on_start);
    } else if (use_qcom_time_offset.value) {
        apply_qcom_rtc_offset(on_start);
    }
}
// ------- End Qualcom Time Fixes


////////////////////////////////
// Change gui color functions //
////////////////////////////////

// called either on start to read and apply setting + force_wait = -1 on start for boot scripts
// or called by toggle background icon menu to write setting and apply value
static void apply_background_icon(int write_cfg) {
    // write setting, only called by toggle background icon menu
    if (write_cfg) {
        char value[5];
        sprintf(value, "%d", show_background_icon.value);
        write_config_file(PHILZ_SETTINGS_FILE, show_background_icon.key, value);
    }

    // read setting and apply it
    force_wait = -1;
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, show_background_icon.key, value, "false");
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
        ui_set_background(BACKGROUND_ICON_CLOCKWORK);
        show_background_icon.value = 1;
    }
    else {
        ui_set_background(BACKGROUND_ICON_NONE);
        show_background_icon.value = 0;
    }
}

//this will only apply settings to the active session
static void apply_gui_colors(const char* key, long int value) {
    //30 lines, each with 4 columns: red value, green value, blue value, alpha value
    int menu_color_table[30][4] = {
        {WHITE_COLOR_CODE},
        {BLACK_COLOR_CODE},
        {CYAN_BLUE_CODE},
        {DEEPSKY_BLUE_CODE},
        {NORMAL_BLUE_CODE},
        {DARK_BLUE_CODE},
        {MYSTY_ROSE_CODE},
        {PINK_COLOR_CODE},
        {THISTLE_COLOR_CODE},
        {TAN_COLOR_CODE},
        {ROSY_BROWN_CODE},
        {NORMAL_RED_CODE},
        {FIREBRICK_CODE},
        {DARK_RED_CODE},
        {ORANGE_RED_CODE},
        {MAGENTA_COLOR_CODE},
        {BLUEVIOLET_CODE},
        {DARK_MAGENTA_CODE},
        {LIME_GREEN_CODE},
        {NORMAL_GREEN_CODE},
        {DARK_GREEN_CODE},
        {DARK_KHAKI_CODE},
        {DARK_OLIVE_CODE},
        {CUSTOM_SILVER_CODE},
        {DARK_GRAY_CODE},
        {NORMAL_GRAY_CODE},
        {DIM_GRAY_CODE},
        {DIMMER_GRAY_CODE},
        {YELLOW_COLOR_CODE},
        {GOLD_COLOR_CODE}
    };

    //set color values (applied on write and non write calls)
    if (strcmp(key, menu_text_color.key) == 0) {
        mtext_code[RED_CHANNEL] = menu_color_table[value][RED_CHANNEL];
        mtext_code[GREEN_CHANNEL] = menu_color_table[value][GREEN_CHANNEL];
        mtext_code[BLUE_CHANNEL] = menu_color_table[value][BLUE_CHANNEL];
        mtext_code[ALPHA_CHANNEL] = menu_color_table[value][ALPHA_CHANNEL];
    } else if (strcmp(key, menu_background_color.key) == 0) {
        mbackg_code[RED_CHANNEL] = menu_color_table[value][RED_CHANNEL];
        mbackg_code[GREEN_CHANNEL] = menu_color_table[value][GREEN_CHANNEL];
        mbackg_code[BLUE_CHANNEL] = menu_color_table[value][BLUE_CHANNEL];
    } else if (strcmp(key, menu_highlight_color.key) == 0) {
        mhlight_code[RED_CHANNEL] = menu_color_table[value][RED_CHANNEL];
        mhlight_code[GREEN_CHANNEL] = menu_color_table[value][GREEN_CHANNEL];
        mhlight_code[BLUE_CHANNEL] = menu_color_table[value][BLUE_CHANNEL];
    } else if (strcmp(key, normal_text_color.key) == 0) {
        normal_text_code[RED_CHANNEL] = menu_color_table[value][RED_CHANNEL];
        normal_text_code[GREEN_CHANNEL] = menu_color_table[value][GREEN_CHANNEL];
        normal_text_code[BLUE_CHANNEL] = menu_color_table[value][BLUE_CHANNEL];
        normal_text_code[ALPHA_CHANNEL] = menu_color_table[value][ALPHA_CHANNEL];
    } else if (strcmp(key, menu_separator_color.key) == 0) {
        mseparator_code[RED_CHANNEL] = menu_color_table[value][RED_CHANNEL];
        mseparator_code[GREEN_CHANNEL] = menu_color_table[value][GREEN_CHANNEL];
        mseparator_code[BLUE_CHANNEL] = menu_color_table[value][BLUE_CHANNEL];
    } else if (strcmp(key, header_text_color.key) == 0) {
        header_text_code[RED_CHANNEL] = menu_color_table[value][RED_CHANNEL];
        header_text_code[GREEN_CHANNEL] = menu_color_table[value][GREEN_CHANNEL];
        header_text_code[BLUE_CHANNEL] = menu_color_table[value][BLUE_CHANNEL];
    } else if (strcmp(key, batt_clock_color.key) == 0) {
        batt_clock_code[RED_CHANNEL] = menu_color_table[value][RED_CHANNEL];
        batt_clock_code[GREEN_CHANNEL] = menu_color_table[value][GREEN_CHANNEL];
        batt_clock_code[BLUE_CHANNEL] = menu_color_table[value][BLUE_CHANNEL];
    } else if (strcmp(key, menu_background_transparency.key) == 0) {
        mbackg_code[ALPHA_CHANNEL] = value;
    } else if (strcmp(key, menu_highlight_transparency.key) == 0) {
        mhlight_code[ALPHA_CHANNEL] = value;
    } else if (strcmp(key, menu_separator_transparency.key) == 0) {
        mseparator_code[ALPHA_CHANNEL] = value;
    } else
        //who knows!
        LOGE("internal error: no valid colors to write!\n");
}

//read and write gui colors from/to settings file
static void parse_gui_colors(const char* key, long int value, int write_cfg) {
    char val_string[PROPERTY_VALUE_MAX];
    char default_color[5];

    //read settings
    if (!write_cfg) {
        //read menu_text_color.value value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_MENU_TEXT_COLOR);
        read_config_file(PHILZ_SETTINGS_FILE, menu_text_color.key, val_string, default_color);
        if ((menu_text_color.value = strtol(val_string, NULL, 10)) > MAX_COLORS || menu_text_color.value < 0)
            menu_text_color.value = DEFAULT_MENU_TEXT_COLOR;
        apply_gui_colors(menu_text_color.key, menu_text_color.value);

        //read menu_background_color.key value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_MENU_BACKGROUND_COLOR);
        read_config_file(PHILZ_SETTINGS_FILE, menu_background_color.key, val_string, default_color);
        if ((menu_background_color.value = strtol(val_string, NULL, 10)) > MAX_COLORS || menu_background_color.value < 0)
            menu_background_color.value = DEFAULT_MENU_BACKGROUND_COLOR;
        apply_gui_colors(menu_background_color.key, menu_background_color.value);

        //read menu_highlight_color.key value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_MENU_HIGHLIGHT_COLOR);
        read_config_file(PHILZ_SETTINGS_FILE, menu_highlight_color.key, val_string, default_color);
        if ((menu_highlight_color.value = strtol(val_string, NULL, 10)) > MAX_COLORS || menu_highlight_color.value < 0)
            menu_highlight_color.value = DEFAULT_MENU_HIGHLIGHT_COLOR;
        apply_gui_colors(menu_highlight_color.key, menu_highlight_color.value);

        //read normal_text_color.key value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_NORMAL_TEXT_COLOR);
        read_config_file(PHILZ_SETTINGS_FILE, normal_text_color.key, val_string, default_color);
        if ((normal_text_color.value = strtol(val_string, NULL, 10)) > MAX_COLORS || normal_text_color.value < 0)
            normal_text_color.value = DEFAULT_NORMAL_TEXT_COLOR;
        apply_gui_colors(normal_text_color.key, normal_text_color.value);

        //read menu_separator_color.key value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_MENU_SEPARATOR_COLOR);
        read_config_file(PHILZ_SETTINGS_FILE, menu_separator_color.key, val_string, default_color);
        if ((menu_separator_color.value = strtol(val_string, NULL, 10)) > MAX_COLORS || menu_separator_color.value < 0)
            menu_separator_color.value = DEFAULT_MENU_SEPARATOR_COLOR;
        apply_gui_colors(menu_separator_color.key, menu_separator_color.value);

        //read header_text_color value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_HEADER_TEXT_COLOR);
        read_config_file(PHILZ_SETTINGS_FILE, header_text_color.key, val_string, default_color);
        if ((header_text_color.value = strtol(val_string, NULL, 10)) > MAX_COLORS || header_text_color.value < 0)
            header_text_color.value = DEFAULT_HEADER_TEXT_COLOR;
        apply_gui_colors(header_text_color.key, header_text_color.value);

        //read batt_clock_color.key value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_BATT_CLOCK_COLOR);
        read_config_file(PHILZ_SETTINGS_FILE, batt_clock_color.key, val_string, default_color);
        if ((batt_clock_color.value = strtol(val_string, NULL, 10)) > MAX_COLORS || batt_clock_color.value < 0)
            batt_clock_color.value = DEFAULT_BATT_CLOCK_COLOR;
        apply_gui_colors(batt_clock_color.key, batt_clock_color.value);

        //read menu_background_transparency.key value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_BACKGROUND_ALPHA);
        read_config_file(PHILZ_SETTINGS_FILE, menu_background_transparency.key, val_string, default_color);
        if ((mbackg_code[ALPHA_CHANNEL] = strtol(val_string, NULL, 10)) > 255 || mbackg_code[ALPHA_CHANNEL] < 0)
            mbackg_code[ALPHA_CHANNEL] = DEFAULT_BACKGROUND_ALPHA;
        apply_gui_colors(menu_background_transparency.key, mbackg_code[ALPHA_CHANNEL]);

        //read menu_highlight_transparency.key value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_HIGHLIGHT_ALPHA);
        read_config_file(PHILZ_SETTINGS_FILE, menu_highlight_transparency.key, val_string, default_color);
        if ((mhlight_code[ALPHA_CHANNEL] = strtol(val_string, NULL, 10)) > 255 || mhlight_code[ALPHA_CHANNEL] < 0)
            mhlight_code[ALPHA_CHANNEL] = DEFAULT_HIGHLIGHT_ALPHA;
        apply_gui_colors(menu_highlight_transparency.key, mhlight_code[ALPHA_CHANNEL]);
        
        //read menu_separator_transparency.key value and apply it
        snprintf(default_color, sizeof(default_color), "%d", DEFAULT_SEPARATOR_ALPHA);
        read_config_file(PHILZ_SETTINGS_FILE, menu_separator_transparency.key, val_string, default_color);
        if ((mseparator_code[ALPHA_CHANNEL] = strtol(val_string, NULL, 10)) > 255 || mseparator_code[ALPHA_CHANNEL] < 0)
            mseparator_code[ALPHA_CHANNEL] = DEFAULT_SEPARATOR_ALPHA;
        apply_gui_colors(menu_separator_transparency.key, mseparator_code[ALPHA_CHANNEL]);

        return;
    }

    //write settings to config file
    if (write_cfg) {
        //by default, set some transparency for menu_highlight_color.value, menu_background_color.value and menu_separator_color.value
        if (strcmp(key, menu_highlight_color.key) == 0) {
            mhlight_code[ALPHA_CHANNEL] = DEFAULT_HIGHLIGHT_ALPHA;
            snprintf(val_string, sizeof(val_string), "%d", DEFAULT_HIGHLIGHT_ALPHA);
            write_config_file(PHILZ_SETTINGS_FILE, menu_highlight_transparency.key, val_string);
        }
        if (strcmp(key, menu_background_color.key) == 0) {
            mbackg_code[ALPHA_CHANNEL] = DEFAULT_BACKGROUND_ALPHA;
            snprintf(val_string, sizeof(val_string), "%d", DEFAULT_BACKGROUND_ALPHA);
            write_config_file(PHILZ_SETTINGS_FILE, menu_background_transparency.key, val_string);
        }
        if (strcmp(key, menu_separator_color.key) == 0) {
            mseparator_code[ALPHA_CHANNEL] = DEFAULT_SEPARATOR_ALPHA;
            snprintf(val_string, sizeof(val_string), "%d", DEFAULT_SEPARATOR_ALPHA);
            write_config_file(PHILZ_SETTINGS_FILE, menu_separator_transparency.key, val_string);
        }

        //we write settings to file
        snprintf(val_string, sizeof(val_string), "%ld", value);
        write_config_file(PHILZ_SETTINGS_FILE, key, val_string);
        apply_gui_colors(key, value);
    }
}

/*
Toggle Background Image / Color functions
- apply_background_image(const char* image_file, long int num, int write_cfg)
    * is used to read config file and to write user setting depending on int write_cfg it is called with
    * Read mode: called at recovery start (write_cfg == 0). image_file and num are not checked in this mode
        + we read value from background_image.value=value in philz-touch.ini file
        + if value is a user selected png, its length is always > 3 (path is primary_storage/CUSTOM_ROM_PATH/files.png)
        + else, we check to ensure it is an integer ranging from 0-MAX_COLORS (see below write mode)
    * Write mode:
        + image_file is either the file name of custom png selected by user or NULL
        + when we apply a user custom png, image_file is not NULL
        + if image_file == NULL: we consider num as the solid color index to apply
        + if we want to set a custom color, we pass 0 to MAX_COLORS as long int num and NULL to image_file
        + Solid colors are 27 png files merged in /res/images/backg.res
        + Each png is offset bytes length (2262). 0.png, 1.png... MAX_COLORS.png
        + 0.png is position WHITE_COLOR (0), 1.png is at position BLACK_COLOR (1)...
- apply_background_color(long int num) extracts the num.png file from /res/images/backg.res and overwrites stitch.png
    * it is always called by apply_background_image(...)
*/

static int file_is_png(const char* image_file) {
    if (!file_found(image_file)) {
        LOGE("Couldn't find background image %s\n", image_file);
        return 0;    
    }

    FILE *fp = fopen(image_file, "rb");
    if (fp == NULL) {
        LOGE("Failed to open background image %s\n", image_file);
        return 0;
    }
    char buff[9];
    fread(buff, 1, 8, fp);
    if (fclose(fp) != 0) {
        LOGE("Failed to close '%s'\n", image_file);
        return 0;
    }

    char magic_num[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    int i;
    for(i = 0; i < 8; i++) {
        if (buff[i] != magic_num[i])
            return 0;
    }
    return 1;
}

static void apply_background_color(long int num) {
    if (num < 0 || num > MAX_COLORS) {
        LOGE("No such index in backg.res (%ld)\n", num);
        return;
    }

    int offset = 2262;
    FILE *fp = fopen("/res/images/backg.res", "rb");
    if (fp == NULL) {
        LOGE("Failed to open file backg.res\n");
        return;
    }

    char buf[4096];
    if (fseek(fp, num * offset, SEEK_SET) != 0) {
        LOGE("fseek error on backg.res\n");
        return;
    }
    fread(buf, 1, offset, fp);
    fclose(fp);
    
    FILE *fp_out = fopen("/res/images/stitch.png", "wb");
    if (fp_out == NULL) {
        LOGE("failed to open stitch.png\n");
        return;
    }
    fwrite(buf, 1, offset, fp_out);
    fclose(fp_out);

    LOGI("Custom background color loaded (%ld)\n", num);
    return;
}

static void apply_background_image(const char* image_file, long int num, int write_cfg) {
    if (write_cfg) {
        // first check if it is a solid color to set (image_file passed as NULL)
        if (image_file == NULL) {
            apply_background_color(num);
            char tmp[5];
            sprintf(tmp, "%ld", num);
            write_config_file(PHILZ_SETTINGS_FILE, background_image.key, tmp);
            // refresh background
            fast_ui_init_png();
            return;
        }

        // we are dealing with a user custom image
        // ensure not too long path since read_config_file() will cut values at PROPERTY_VALUE_MAX max length
        if (strlen(image_file) > PROPERTY_VALUE_MAX) {
            LOGE("Error: path for selected image > %d\n", PROPERTY_VALUE_MAX);
            return;
        }

        //check magic number is really a png
        if (!file_is_png(image_file)) {
            LOGE("Invalid png file!\n");
            return;
        }

        write_config_file(PHILZ_SETTINGS_FILE, background_image.key, image_file);
        if (copy_a_file(image_file, "/res/images/stitch.png") == 0)
            LOGI("custom backround %s loaded!\n", image_file);

        // refresh background
        fast_ui_init_png();
        return;
    }

    // on recovery start, read file settings and copy image to /res/images
    char value[PROPERTY_VALUE_MAX];
    read_config_file(PHILZ_SETTINGS_FILE, background_image.key, value, "default");
    if (strcmp(value, "default") == 0) {
        LOGI("%s set to default\n", background_image.key);
        // apply_background_color(BLACK_COLOR); // PhilZ Touch 5 black theme
        LOGI("Default stitch background loaded.\n"); // default is /res/images/stitch.png
        return;
    }

    // first check if it is a solid color to apply (value would be 0 to MAX_COLORS)
    if (strlen(value) < 3) {
        long int num;
        char is_num_val[5];
        num = strtol(value, NULL, 10);
        sprintf(is_num_val, "%ld", num);
        if (strcmp(is_num_val, value) != 0) {
            LOGE("Invalid value for %s (%s)\n", background_image.key, value);
            return;
        }

        if (num < 0 || num > MAX_COLORS) {
            LOGE("Invalid number range for %s (%s)\n", value, background_image.key);
            return;
        }
        apply_background_color(num);
    } else {
        //seems we are setting a custom user image
        // first ensure it is not default png file (source = destination! for copy_a_file())
        if (strcmp(value, "/res/images/stitch.png") == 0) {
            LOGE("stitch.png is the default background!\n");
            return;
        }

        //check file exists and magic number is really a png
        if (!file_is_png(value)) {
            LOGE("Invalid png image for background!\n");
            return;
        }

        if (copy_a_file(value, "/res/images/stitch.png") == 0)
            LOGI("custom backround %s loaded!\n", value);
    }

    // refresh background
    fast_ui_init_png();
}

static void choose_background_image(const char* sd_path) {
    if (ensure_path_mounted(sd_path) != 0) {
        LOGE("Couldn't mount %s\n", sd_path);
        return;
    }

    const char* headers[] = {"Choose a background image.", NULL};

    char tmp[PATH_MAX];
    //tariling / previously needed for choose_file_menu()
    sprintf(tmp, "%s/%s/", sd_path, CUSTOM_RES_IMAGE_PATH);
    char* file = choose_file_menu(tmp, ".png", headers);
    if (file == NULL) {
        //either no valid files found or we selected no files by pressing back menu
        if (no_files_found)
            ui_print("No .png images in %s\n", tmp);
        return;
    }

    apply_background_image(file, 0, 1);
    free(file);
}

static void browse_background_image() {
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    const char* headers[] = {"Choose an image or color", "", NULL};
    int list_top_items = 5;
    char list_prefix[] = "Image from ";
    char* list[MAX_NUM_MANAGED_VOLUMES + list_top_items + 1];
    char buf[80];
    memset(list, 0, sizeof(list));
    list[0] = "Solid Color Background";
    list[1] = "Reset Koush Background";
    list[2] = "Reset PhilZ Touch Background";
    list[3] = "Toggle Background Icon";
    sprintf(buf, "%s%s", list_prefix, primary_path);
    list[4] = strdup(buf);

    int i;
    if (extra_paths != NULL) {
        for(i = 0; i < num_extra_volumes; i++) {
            sprintf(buf, "%s%s", list_prefix, extra_paths[i]);
            list[i + list_top_items] = strdup(buf);
        }
    }
    list[num_extra_volumes + list_top_items] = NULL;

    for (;;) {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK || chosen_item == REFRESH)
            break;
        switch (chosen_item)
        {
            case 0:
                {
                    char value[PROPERTY_VALUE_MAX];
                    long int num;
                    read_config_file(PHILZ_SETTINGS_FILE, background_image.key, value, "default");
                    num = strtol(value, NULL, 10);
                    num += 1;
                    if (num < 0 || num > MAX_COLORS)
                        num = 0;
                    apply_background_image(NULL, num, 1);
                }
                break;
            case 1:
                apply_background_image("/res/images/koush.png", 0, 1);
                break;
            case 2:
                write_config_file(PHILZ_SETTINGS_FILE, background_image.key, "default");
                ui_print("Restart to load PhilZ Touch default background!\n");
                break;
            case 3:
                show_background_icon.value ^= 1;
                apply_background_icon(1);
                break;
            default:
                choose_background_image(list[chosen_item] + strlen(list_prefix));
                break;
        }
    }

    free(list[4]);
    if (extra_paths != NULL) {
        free_string_array(extra_paths);
        for(i = 0; i < num_extra_volumes; i++)
            free(list[i + list_top_items]);
    }
}
//*************** end change gui color functions


// enhanced show log menu using friendly gui mode and pause until a key is pressed
// this will not be affected by the toggle of wait_after_install.value as we use force_wait = 1
void show_log_menu() {
    force_wait = 1;
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_printlogtail(36);
    if (show_background_icon.value)
        ui_set_background(BACKGROUND_ICON_CLOCKWORK);
    else
        ui_set_background(BACKGROUND_ICON_NONE);
}

// Refresh GUI and touch settings: called by refresh_recovery_settings()
// we pass on_start only on recovery start
void refresh_touch_gui_settings(int on_start) {
    // early set brightness
    apply_brightness_value(-1);

    //these must be called before fast_ui_init()
    check_menu_height();
    check_virtual_keys();
    check_min_log_rows();
    fast_ui_init();

    // set the background. fast_ui_init_png() only refreshes background image
    apply_background_image("load", 0, 0);
    fast_ui_init_png();
    apply_background_icon(0);

    // gui look
    check_battery_enabled();
    check_clock_enabled();
    parse_gui_colors("load", 0, 0);
    check_menu_separation();
    check_dim_blank_timer();

    // touch and system settings
    check_scroll_sensitivity();
    check_touch_accuracy();
    check_touch_to_validate();
    check_enable_vibrator();
    check_boardEnableKeyRepeat();
    check_wait_after_install();
    check_gesture_actions();
    if (on_start) {
        // no need to load these when refreshing recovery settings (theme loading, restore settings...)
        // only load them on recovery start
        apply_time_zone(0, 0);
        apply_qcom_time_daemon_fixes(1);
    }
}
//-------- End GUI Preferences functions

//start show GUI Preferences menu
static void change_menu_color() {
    const char* headers[] = { "Change Menu Colors", NULL };

    char* list[] = {
        "Change Menu Text Color",
        "Change Menu Background Color",
        "Change Menu Background Alpha",
        "Change Menu Highlight Color",
        "Change Menu Highlight Alpha",
        "Change Menu Separator Color",
        "Change Menu Separator Alpha",
        "Change Log and Prints Color",
        "Change Header Text Color",
        "Change Battery and Clock Color",
        NULL
    };

    for (;;) {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                menu_text_color.value += 1;
                if (menu_text_color.value > MAX_COLORS)
                    menu_text_color.value = 0;
                parse_gui_colors(menu_text_color.key, menu_text_color.value, 1);
                break;
            case 1:
                menu_background_color.value += 1;
                if (menu_background_color.value > MAX_COLORS)
                    menu_background_color.value = 0;
                parse_gui_colors(menu_background_color.key, menu_background_color.value, 1);
                break;
            case 2:
                mbackg_code[ALPHA_CHANNEL] += 51;
                if (mbackg_code[ALPHA_CHANNEL] > 255)
                    mbackg_code[ALPHA_CHANNEL] = 0;
                parse_gui_colors(menu_background_transparency.key, mbackg_code[ALPHA_CHANNEL], 1);
                break;
            case 3:
                menu_highlight_color.value += 1;
                if (menu_highlight_color.value > MAX_COLORS)
                    menu_highlight_color.value = 0;
                parse_gui_colors(menu_highlight_color.key, menu_highlight_color.value, 1);
                break;
            case 4:
                mhlight_code[ALPHA_CHANNEL] += 51;
                if (mhlight_code[ALPHA_CHANNEL] > 255)
                    mhlight_code[ALPHA_CHANNEL] = 0;
                parse_gui_colors(menu_highlight_transparency.key, mhlight_code[ALPHA_CHANNEL], 1);
                break;
            case 5:
                menu_separator_color.value += 1;
                if (menu_separator_color.value > MAX_COLORS)
                    menu_separator_color.value = 0;
                parse_gui_colors(menu_separator_color.key, menu_separator_color.value, 1);
                break;
            case 6:
                mseparator_code[ALPHA_CHANNEL] += 51;
                if (mseparator_code[ALPHA_CHANNEL] > 255)
                    mseparator_code[ALPHA_CHANNEL] = 0;
                parse_gui_colors(menu_separator_transparency.key, mseparator_code[ALPHA_CHANNEL], 1);
                break;
            case 7:
                normal_text_color.value += 1;
                if (normal_text_color.value > MAX_COLORS)
                    normal_text_color.value = 0;
                parse_gui_colors(normal_text_color.key, normal_text_color.value, 1);
                break;
            case 8:
                header_text_color.value += 1;
                if (header_text_color.value > MAX_COLORS)
                    header_text_color.value = 0;
                parse_gui_colors(header_text_color.key, header_text_color.value, 1);
                break;
            case 9:
                batt_clock_color.value += 1;
                if (batt_clock_color.value > MAX_COLORS)
                    batt_clock_color.value = 0;
                parse_gui_colors(batt_clock_color.key, batt_clock_color.value, 1);
                break;
        }
    }
}


/**********************************/
/*   Start touch gesture actions  */
/**********************************/
// capture screen to incremental file names
// prefer second storage paths first, then primary storage
#define FB2PNG_BIN "/sbin/fb2png" // recovery_cmds.h
static int fb2png_capture(const char* file_path) {
    if (!file_found(FB2PNG_BIN))
        return -1;

    char cmd[PATH_MAX];
    sprintf(cmd, "%s %s", FB2PNG_BIN, file_path);
    return __system(cmd);
}

static void screen_shot() {
    char* sd_path = NULL;
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    int i = 0;
    if (extra_paths != NULL) {
        while (i < num_extra_volumes && sd_path == NULL) {
            if (ensure_path_mounted(extra_paths[i]) == 0)
                sd_path = extra_paths[i];
            i++;
        }
    }

    // check if we found a secondary storage
    if (sd_path == NULL)
        sd_path = get_primary_storage_path();

    if (ensure_path_mounted(sd_path) != 0) {
        ui_print("no mountable storage to save screen shots.\n");
        free_string_array(extra_paths);
        return;
    }

    // reads index file to increment filename
    char path[PATH_MAX];
    char line[5]; // xxxx + new line, so that when it reaches 1000 it doesn't read it as 100
    long int file_num = 1;
    sprintf(path, "%s/%s/index", sd_path, SCREEN_CAPTURE_FOLDER);
    FILE *fp = fopen(path, "r");
    if (fp != NULL) {
        if (fgets(line, sizeof(line), fp) != NULL) {
            file_num = strtol(line, NULL, 10);
            file_num++;
            if (file_num < 1 || file_num > 999)
                file_num = 1;
        }
        fclose(fp);
    }

    // capture screen
    char dirtmp[PATH_MAX];
    int ret;
    sprintf(dirtmp, "%s", DirName(path));
    ensure_directory(dirtmp, 0755);
    sprintf(path, "%s/%s/cwm_screen%03ld.png", sd_path, SCREEN_CAPTURE_FOLDER, file_num);
    ret = gr_save_screenshot(path);
    if (ret == -2) {
        // device uses a custom graphics.c
        // try to use libfb2png
        ret = fb2png_capture(path);
        ui_print("custom graphics source detected: dropping to fb2png mode:\n");
    }
    if (ret == 0) {
        ui_print("screen shot: %s\n", path);
        sprintf(path, "%s/%s/index", sd_path, SCREEN_CAPTURE_FOLDER);
        sprintf(line, "%ld", file_num);
        write_string_to_file(path, line);
    } else {
        LOGE("screen capture failed\n");
    }
    free_string_array(extra_paths);
}

// Touch gesture actions
//  - they are only triggered when in a menu prompt view (get_menu_selection())
//  - we also disable them if progress bar is being shown
//    this can happen in md5 display/verify threads where we have progress bar while waiting for menu action
//    screen capture and brightness actions call unsafe thread functions: basename, dirname, ensure_path_mounted()
void handle_gesture_actions(const char** headers, char** items, int initial_selection) {
    int action = DISABLE_ACTION;
    if (ui_showing_progress_bar())
        action = DISABLE_ACTION;
    else if (key_gesture == SLIDE_LEFT_GESTURE)
        action = slide_left_action.value;
    else if (key_gesture == SLIDE_RIGHT_GESTURE)
        action = slide_right_action.value;
    else if (key_gesture == DOUBLE_TAP_GESTURE)
        action = double_tap_action.value;
    else if (key_gesture == PRESS_LIFT_GESTURE)
        action = press_lift_action.value;
    else if (key_gesture == PRESS_MOVE_GESTURE)
        action = press_move_action.value;

    switch (action) {
        case SCREEN_CAPTURE_ACTION:
            screen_shot();
            break;
        case AROMA_BROWSER_ACTION:
            ui_end_menu();
            run_aroma_browser();
            ui_clear_key_queue();
            ui_start_menu(headers, items, initial_selection);
            break;
        case ADJUST_BRIGHTNESS_ACTION:
            toggle_brightness();
            break;
        case SHOW_LOG_ACTION:
            ui_end_menu();
            show_log_menu();
            ui_start_menu(headers, items, initial_selection);
            break;
        case BLANK_SCREEN_ACTION:
            ui_blank_screen(true);
            // to avoid considering more keys, mainly on long press and move action, usleep for 1sec before clearing key queue
            // use 999999 micro sec as 1000000 usecs is illegal in usleep
            usleep(999999);
            ui_clear_key_queue();
            ui_wait_key(); // will also call ui_blank_screen(false) on a key press and set ignore_key_action
            break;
    }

    // now pressing the hardware key KEY_LEFTBRACE (we fake for touch gesture) will not toggle the last touch gesture
    key_gesture = 0;
}

static void gestures_action_setup() {
    const char* headers[] = { "Gesture Action Setup", NULL };

    char item_slide_left[MENU_MAX_COLS];
    char item_slide_right[MENU_MAX_COLS];
    char item_double_tap[MENU_MAX_COLS];
    char item_press_lift[MENU_MAX_COLS];
    char item_press_move[MENU_MAX_COLS];

    char* list[] = {
        item_slide_left,
        item_slide_right,
        item_double_tap,
        item_press_lift,
        item_press_move,
        NULL
    };

    char* gesture_action[] = {
        "disabled",
        "screen shot", 
        "aroma browser",
        "set brightness",
        "show log",
        "toggle screen",
        NULL
    };

    for (;;) {
        int i = 0;
        while (gesture_action[i] != NULL) {
            ui_format_gui_menu(item_slide_left, "Slide Left", gesture_action[slide_left_action.value]);
            ui_format_gui_menu(item_slide_right, "Slide Right", gesture_action[slide_right_action.value]);
            ui_format_gui_menu(item_double_tap, "Double Tap", gesture_action[double_tap_action.value]);
            ui_format_gui_menu(item_press_lift, "Press/Lift", gesture_action[press_lift_action.value]);
            ui_format_gui_menu(item_press_move, "Press/Move", gesture_action[press_move_action.value]);
            i++;
        }

        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                {
                    char value[5];
                    slide_left_action.value += 1;
                    if (slide_left_action.value > MAX_DEFINED_ACTIONS)
                        slide_left_action.value = DISABLE_ACTION;

                    sprintf(value, "%ld", slide_left_action.value);
                    write_config_file(PHILZ_SETTINGS_FILE, slide_left_action.key, value);
                }
                break;
            case 1:
                {
                    char value[5];
                    slide_right_action.value += 1;
                    if (slide_right_action.value > MAX_DEFINED_ACTIONS)
                        slide_right_action.value = DISABLE_ACTION;

                    sprintf(value, "%ld", slide_right_action.value);
                    write_config_file(PHILZ_SETTINGS_FILE, slide_right_action.key, value);
                }
                break;
            case 2:
                {
                    char value[5];
                    double_tap_action.value += 1;
                    if (double_tap_action.value > MAX_DEFINED_ACTIONS)
                        double_tap_action.value = DISABLE_ACTION;

                    sprintf(value, "%ld", double_tap_action.value);
                    write_config_file(PHILZ_SETTINGS_FILE, double_tap_action.key, value);
                }
                break;
            case 3:
                {
                    char value[5];
                    press_lift_action.value += 1;
                    if (press_lift_action.value > MAX_DEFINED_ACTIONS)
                        press_lift_action.value = DISABLE_ACTION;

                    sprintf(value, "%ld", press_lift_action.value);
                    write_config_file(PHILZ_SETTINGS_FILE, press_lift_action.key, value);
                }
                break;
            case 4:
                {
                    char value[5];
                    press_move_action.value += 1;
                    if (press_move_action.value > MAX_DEFINED_ACTIONS)
                        press_move_action.value = DISABLE_ACTION;

                    sprintf(value, "%ld", press_move_action.value);
                    write_config_file(PHILZ_SETTINGS_FILE, press_move_action.key, value);
                }
                break;
        }
    }
}
//----------- end touch gesture actions

// set time zone
static void time_zone_h_menu() {
    const char* headers[] = { "Select Time Zone", NULL };

    char* list[] = {
        "(UTC -11) Samoa, Midway Island",
        "(UTC -10) Hawaii",
        "(UTC -9) Alaska",
        "(UTC -8) Pacific Time",
        "(UTC -7) Mountain Time",
        "(UTC -6) Central Time",
        "(UTC -5) Eastern Time",
        "(UTC -4) Atlantic Time",
        "(UTC -3) Brazil, Buenos Aires",
        "(UTC -2) Mid-Atlantic",
        "(UTC -1) Azores, Cape Verde",
        "(UTC  0) London, Dublin, Lisbon",
        "(UTC +1) Berlin, Brussels, Paris",
        "(UTC +2) Athens, Istanbul, South Africa",
        "(UTC +3) Baghdad, East Africa",
        "(UTC +4) Abu Dhabi, Moscow, Muscat",
        "(UTC +5) Yekaterinburg, Islamabad",
        "(UTC +6) Almaty, Dhaka, Colombo",
        "(UTC +7) Bangkok, Hanoi, Jakarta",
        "(UTC +8) Beijing, Singapore, Hong Kong",
        "(UTC +9) Tokyo, Seoul, Yakutsk",
        "(UTC +10) Eastern Australia, Guam",
        "(UTC +11) Vladivostok, Solomon Islands",
        "(UTC +12) Auckland, Wellington, Fiji",
        NULL
    };

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item) {
        case 0:
            apply_time_zone(1, -11);
            break;
        case 1:
            apply_time_zone(1, -10);
            break;
        case 2:
            apply_time_zone(1, -9);
            break;
        case 3:
            apply_time_zone(1, -8);
            break;
        case 4:
            apply_time_zone(1, -7);
            break;
        case 5:
            apply_time_zone(1, -6);
            break;
        case 6:
            apply_time_zone(1, -5);
            break;
        case 7:
            apply_time_zone(1, -4);
            break;
        case 8:
            apply_time_zone(1, -3);
            break;
        case 9:
            apply_time_zone(1, -2);
            break;
        case 10:
            apply_time_zone(1, -1);
            break;
        case 11:
            apply_time_zone(1, 0);
            break;
        case 12:
            apply_time_zone(1, 1);
            break;
        case 13:
            apply_time_zone(1, 2);
            break;
        case 14:
            apply_time_zone(1, 3);
            break;
        case 15:
            apply_time_zone(1, 4);
            break;
        case 16:
            apply_time_zone(1, 5);
            break;
        case 17:
            apply_time_zone(1, 6);
            break;
        case 18:
            apply_time_zone(1, 7);
            break;
        case 19:
            apply_time_zone(1, 8);
            break;
        case 20:
            apply_time_zone(1, 9);
            break;
        case 21:
            apply_time_zone(1, 10);
            break;
        case 22:
            apply_time_zone(1, 11);
            break;
        case 23:
            apply_time_zone(1, 12);
            break;
    }
}

/*
 * struct tm new_date; // strcut holding time in calendar data:
    - tm_hour: 0-23; tm_min: 0-59; tm_sec: 0-59; tm_mon: 0-11; tm_mday: 1-31; tm_year: n since 1900
 * time_t new_date_secs = time(NULL); // time(NULL) returns t_time type. It's value is current seconds since epoch (1.1.1970)
 * localtime_r(&new_date_secs, &new_date); //initialize new_date with calendar time broken down from &new_date_secs
    - localtime_r() is thread safe unlike localtime()
    - localtime() returns a pointer to an internal static "struct tm" altered by any other thread call to the function
    - since we call it in touch code, we cannot use it
    - either we copy localtime() pointer as soon as it is called or we use localtime_r()
 * new_date_secs = mktime(&new_date); // mktime reads struct tm calendar data and returns a time_t value with the total seconds elapsed since epoch
    - it is a reverse of localtime()
 * localtime_r(&new_date_secs, &new_date);
    - assigns strcut tm new_date the user set date
 * struct timeval tv;
    - can be initialized by setting its 2 members: time_t tv_sec and suseconds_t tv_usec
    - tv.tv_sec = new_date_secs; // initialize timeval struct tv_sec member. It now holds total seconds elapsed between epoch and the the new date chosen by user
    - tv.tv_usec = 0; // we need to initialize all struct members
 * settimeofday(&tv, NULL)
    - settimeofday() needs a struct timeval argument
    - it will set current time and date to the total seconds since epoch specified by the timeval struct
    - in our case, the tv timeval struct holds the date chosen by user
*/
#define CHANGE_TIME_MENU_VALIDATE       0
#define CHANGE_TIME_MENU_INCREASE       1
#define CHANGE_TIME_MENU_DECREASE       2
#define CHANGE_TIME_MENU_NEXT           3
#define CHANGE_TIME_MENU_PREVIOUS       4
#define CHANGE_TIME_MENU_DATE_BIN       5
#define CHANGE_TIME_MENU_T_DAEMON_DATA  6
#define CHANGE_TIME_MENU_T_DAEMON_LOAD  7
#define CHANGE_TIME_MENU_T_OFFSET       8
static void change_date_time_menu() {
    char item_increase[MENU_MAX_COLS];
    char item_decrease[MENU_MAX_COLS];
    char item_next[MENU_MAX_COLS];
    char item_previous[MENU_MAX_COLS];
    char item_force_system_date[MENU_MAX_COLS];
    char item_parse_time_daemon_data_files[MENU_MAX_COLS];
    char item_qcom_time_daemon[MENU_MAX_COLS];
    char item_qcom_time_offset[MENU_MAX_COLS];

    char chosen_date[MENU_MAX_COLS] = "";
    const char* headers[] = { "Change date and time:", chosen_date, "", NULL };

    char* list[] = {
        ">> Validate Chosen Date",
        item_increase,
        item_decrease,
        item_next,
        item_previous,
        item_force_system_date,
        item_parse_time_daemon_data_files,
        item_qcom_time_daemon,
        item_qcom_time_offset,
        NULL    // GO_BACK (cancel)
    };

    char value[15];
    char* date_elements[] = { "year", "month", "day", "hour", "minutes", "seconds" };
    int force_system_date = 0;
    int current = 0;
    int next = 0;
    int previous = 0;

    struct tm new_date;
    time_t new_date_secs = time(NULL);
    localtime_r(&new_date_secs, &new_date);

    for (;;) {
        // update header text
        strftime(chosen_date, sizeof(chosen_date), "--> %Y-%m-%d %H:%M:%S", &new_date);

        // update "toggle use of date -s" menu
        if (force_system_date) ui_format_gui_menu(item_force_system_date, "Try Force Persist on Reboot", "(x)");
        else ui_format_gui_menu(item_force_system_date, "Try Force Persist on Reboot", "( )");

        // qcom devices: try to directly parse the /data/time contents
        if (use_qcom_time_data_files.value) ui_format_gui_menu(item_parse_time_daemon_data_files, "Parse Time Daemon Data", "(x)");
        else ui_format_gui_menu(item_parse_time_daemon_data_files, "Parse Time Daemon Data", "( )");

        // menu to toggle load of time_daemon
        if (use_qcom_time_daemon.value)
            ui_format_gui_menu(item_qcom_time_daemon, "Qualcom Time Daemon", "(x)");
        else ui_format_gui_menu(item_qcom_time_daemon, "Qualcom Time Daemon", "( )");

        // menu to toggle use of time offset
        if (use_qcom_time_offset.value != 0)
            ui_format_gui_menu(item_qcom_time_offset, "Use RTC Time Offset...", "(x)");
        else ui_format_gui_menu(item_qcom_time_offset, "Use RTC Time Offset...", "( )");

        // update increase and decrease menus
        sprintf(item_increase, "Time + (%s)", date_elements[current]);
        sprintf(item_decrease, "Time - (%s)", date_elements[current]);

        // update next and previous menu list
        next = current + 1;
        previous = current - 1;
        if (next > 5)
            next = 0;
        else if (next < 0)
            next = 5;
        if (previous > 5)
            previous = 0;
        else if (previous < 0)
            previous = 5;
        sprintf(item_next, "Next > (%s)", date_elements[next]);
        sprintf(item_previous, "Prev < (%s)", date_elements[previous]);

        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;

        if (chosen_item == CHANGE_TIME_MENU_VALIDATE) {
            // validate the date set by user then exit
            // we first try settimeofday() function
            int ret = 0;
            struct timeval tv;
            tv.tv_sec = new_date_secs;
            tv.tv_usec = 0;
            ret = settimeofday(&tv, NULL);
            if (ret != 0)
                LOGI("set time (1) failed (%s)\n", strerror(errno));
            if (force_system_date || ret != 0) {
                // try using system date binary: some qualcom boards cannot handle settimeofday()
                // other qualcom boards won't have time persist after a reboot when using settimeofday()
                // jflte: time stays on reboot when using toolbox date -s
                char cmd[512];
                if (file_found("/system/bin/toolbox")) {
                    LOGI("trying set time from toolbox...\n");
                    strftime(chosen_date, sizeof(chosen_date), "%Y%m%d.%H%M%S", &new_date);
                    sprintf(cmd, "/system/bin/toolbox date -s %s", chosen_date);
                    ret = __system(cmd);
                    if (ret != 0)
                        LOGI("set time (2) failed (%s)\n", strerror(errno));
                } else {
                    LOGI("trying set time from busybox...\n");
                    strftime(chosen_date, sizeof(chosen_date), "%Y.%m.%d-%H:%M:%S", &new_date);
                    sprintf(cmd, "/sbin/date -s %s", chosen_date);
                    ret = __system(cmd);
                    if (ret != 0)
                        LOGI("set time (3) failed (%s)\n", strerror(errno));
                }
            }

            if (use_qcom_time_offset.value != 0) {
                FILE *f = fopen("/sys/class/rtc/rtc0/since_epoch", "r");
                if (f != NULL) {
                    long int rtc_offset;
                    fscanf(f, "%ld", &rtc_offset);
                    fclose(f);
                    use_qcom_time_offset.value = new_date_secs - rtc_offset;
                    if (use_qcom_time_offset.value < 0) {
                        // some devices like jflte cannot use this method.
                        LOGE("rtc offset fix error (%ld)\n", use_qcom_time_offset.value);
                        use_qcom_time_offset.value = 0;
                    }
                    sprintf(value, "%ld", use_qcom_time_offset.value);
                    write_config_file(PHILZ_SETTINGS_FILE, use_qcom_time_offset.key, value);
                }
            }

            if (ret == 0) {
                strftime(chosen_date, sizeof(chosen_date), "--> %Y-%m-%d %H:%M:%S", &new_date);
                ui_print("New date: %s\n", chosen_date);
            } else {
                LOGE("Failed to set new time!\n");
            }
            break; // exit menu
        } else if (chosen_item == CHANGE_TIME_MENU_DATE_BIN) {
            // toggle force use of date -s command
            force_system_date ^= 1;
        } else if (chosen_item == CHANGE_TIME_MENU_T_DAEMON_DATA) {
            const char* qcom_headers[] = {
                "Apply time daemon data:",
                "Use Only for Qualcom boards",
                NULL
            };
            char* qcom_list[] = { "Yes - Apply Time Daemon Data", NULL };
            struct timeval tv;

            // only allow one qcom time fix: time daemon, rtc offset or parse of time data files
            if (!use_qcom_time_data_files.value && use_qcom_time_offset.value != 0) {
                LOGE("first, disable RTC Time Offset\n");
                continue;
            }
            if (!use_qcom_time_data_files.value && use_qcom_time_daemon.value) {
                LOGE("first, disable Qcom Time Daemon\n");
                continue;
            }

            // prompt to enable parsing time daemon data files, but not when disabling it
            if (!use_qcom_time_data_files.value && 0 != get_menu_selection(qcom_headers, qcom_list, 0, 0))
                continue;
            use_qcom_time_data_files.value ^= 1;
            sprintf(value, "%d", use_qcom_time_data_files.value);
            write_config_file(PHILZ_SETTINGS_FILE, use_qcom_time_data_files.key, value);

            // check if the time/date is not what we expect (something near epoch)
            // assume > 2000-01-01 00:00:00
            // if after that time, try to set time to epoch and warn user to reboot into ROM, sync time and back to recovery
            gettimeofday(&tv, NULL);
            if (use_qcom_time_data_files.value && tv.tv_sec > 946684800) {
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                settimeofday(&tv, NULL);
                ui_print("If time is wrong,\n");
                ui_print("reboot to system & sync time\n");
            }
            apply_qcom_time_daemon_fixes(0);

            // refresh current time in menu header
            new_date_secs = time(NULL);
            localtime_r(&new_date_secs, &new_date);
        } else if (chosen_item == CHANGE_TIME_MENU_T_DAEMON_LOAD) {
            const char* qcom_headers[] = { "Load time daemon:", "Use Only for Qualcom boards", "And if all manual modes fail", NULL };
            char* qcom_list[] = { "Yes - Load Time Daemon", NULL };

            // only allow one qcom time fix: time daemon, rtc offset or parse of time data files
            if (!use_qcom_time_daemon.value && use_qcom_time_offset.value != 0) {
                LOGE("first, disable RTC Time Offset\n");
                continue;
            }
            if (!use_qcom_time_daemon.value && use_qcom_time_data_files.value) {
                LOGE("first, disable Parse Time Daemon Data\n");
                continue;
            }

            // prompt to enable time daemon, but not when disabling it
            if (!use_qcom_time_daemon.value && 0 != get_menu_selection(qcom_headers, qcom_list, 0, 0))
                continue;
            use_qcom_time_daemon.value ^= 1;
            sprintf(value, "%d", use_qcom_time_daemon.value);
            write_config_file(PHILZ_SETTINGS_FILE, use_qcom_time_daemon.key, value);
            apply_qcom_time_daemon_fixes(0);

            // refresh current time in menu header
            new_date_secs = time(NULL);
            localtime_r(&new_date_secs, &new_date);
        } else if (chosen_item == CHANGE_TIME_MENU_T_OFFSET) {
            // some Qcom boards that need time_daemon, can use an offset from RTC clock (LGE G2 devices)
            // use_qcom_time_offset.value holds the offset in seconds. If we set to 1, we consider it is to enable and read the offset
            // if user has a ROM that doesn't properly support time_daemon, it will need this trick
            // also this could be used if recovery is missing some selinux permissions to load time_daemon
            // we only allow use of one method: time daemon, parse time daemon data files or time offset
            const char* qcom_headers[] = { "Use time offset:", "Use Only for Qualcom boards", "And if all other modes fail", NULL };
            char* qcom_list[] = { "Yes - Enable Time Offset", NULL };
            if (use_qcom_time_offset.value == 0 && use_qcom_time_daemon.value) {
                LOGE("first, disable Qcom Time Daemon\n");
                continue;
            }
            if (use_qcom_time_offset.value == 0 && use_qcom_time_data_files.value) {
                LOGE("first, disable Parse Time Daemon Data\n");
                continue;
            }

            // prompt to enable time offset, but not when disabling it
            if (use_qcom_time_offset.value == 0 && 0 != get_menu_selection(qcom_headers, qcom_list, 0, 0))
                continue;
            if (use_qcom_time_offset.value != 0) {
                use_qcom_time_offset.value = 0;
            } else {
                use_qcom_time_offset.value = 1;
                ui_print(">> Now set date and validate\n");
            }
            sprintf(value, "%ld", use_qcom_time_offset.value);
            write_config_file(PHILZ_SETTINGS_FILE, use_qcom_time_offset.key, value);
        } else if (chosen_item == CHANGE_TIME_MENU_INCREASE || chosen_item == CHANGE_TIME_MENU_DECREASE) {
            // increase / decrease date or time
            int increment = 1;
            if (chosen_item == CHANGE_TIME_MENU_DECREASE)
                increment = -1;

            if (strcmp(date_elements[current], "year") == 0) {
                new_date.tm_year += increment;
                if (new_date.tm_year < (2014 - 1900))
                    new_date.tm_year = 2035 - 1900;
                else if (new_date.tm_year > (2035 - 1900))
                    new_date.tm_year = 2014 - 1900;
            } else if (strcmp(date_elements[current], "month") == 0) {
                new_date.tm_mon += increment;
                if (new_date.tm_mon < 0)
                    new_date.tm_mon = 11;
                else if (new_date.tm_mon > 11)
                    new_date.tm_mon = 0;
            } else if (strcmp(date_elements[current], "day") == 0) {
                new_date.tm_mday += increment;
                if (new_date.tm_mday < 1)
                    new_date.tm_mday = 31;
                else if (new_date.tm_mday > 31)
                    new_date.tm_mday = 1;
            } else if (strcmp(date_elements[current], "hour") == 0) {
                new_date.tm_hour += increment;
                if (new_date.tm_hour < 0)
                    new_date.tm_hour = 23;
                else if (new_date.tm_hour > 23)
                    new_date.tm_hour = 0;
            } else if (strcmp(date_elements[current], "minutes") == 0) {
                new_date.tm_min += increment;
                if (new_date.tm_min < 0)
                    new_date.tm_min = 59;
                else if (new_date.tm_min > 59)
                    new_date.tm_min = 0;
            } else if (strcmp(date_elements[current], "seconds") == 0) {
                new_date.tm_sec += increment;
                if (new_date.tm_sec < 0)
                    new_date.tm_sec = 59;
                else if (new_date.tm_sec > 59)
                    new_date.tm_sec = 0;
            }

            // fix any 30/31 day per month issue and update new_date tm struct with new calendar date to apply
            new_date_secs = mktime(&new_date);
            localtime_r(&new_date_secs, &new_date);
        } else if (chosen_item == CHANGE_TIME_MENU_NEXT || chosen_item == CHANGE_TIME_MENU_PREVIOUS) {
            // move to next/previous date item to set
            int increment = 1;
            if (chosen_item == CHANGE_TIME_MENU_PREVIOUS)
                increment = -1;

            current += increment;
            if (current > 5)
                current = 0;
            else if (current < 0)
                current = 5;
        }
    }
}

static void show_time_settings_menu() {
    const char* headers[] = { "Time settings", NULL };

    char item_timezone_h[MENU_MAX_COLS];
    char item_timezone_m[MENU_MAX_COLS];
    char item_dst[MENU_MAX_COLS];
    char* list[] = {
        item_timezone_h,
        item_timezone_m,
        item_dst,
        "Change Date and Time",
        NULL
    };

    for (;;) {
        // show current timezone and time in menu
        char tmp[MENU_MAX_COLS];
        struct tm *timeptr;
        time_t now = time(NULL);
        timeptr = localtime(&now);

        if (timeptr != NULL) {
            strftime(item_timezone_h, sizeof(item_timezone_h), "%H:%M", timeptr);
            sprintf(tmp, "%s UTC %s%ld:%02ld", item_timezone_h, t_zone.value <= 0 ? "" : "+", t_zone.value, t_zone_offset.value);
        } else {
            sprintf(tmp, "--:--");
        }
        ui_format_gui_menu(item_timezone_h, "Time Zone:", tmp);

        sprintf(tmp, "+%ld mn", t_zone_offset.value);
        ui_format_gui_menu(item_timezone_m, "Time Zone Offset", tmp);

        if (use_dst_time.value)
            ui_format_gui_menu(item_dst, "Day Light Saving Time", "(x)");
        else ui_format_gui_menu(item_dst, "Day Light Saving Time", "( )");

        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item) {
            case 0:
                time_zone_h_menu();
                break;
            case 1: {
                char value[4];
                t_zone_offset.value += 15;
                if (t_zone_offset.value >= 60)
                    t_zone_offset.value = 0;

                sprintf(value, "%ld", t_zone_offset.value);
                write_config_file(PHILZ_SETTINGS_FILE, t_zone_offset.key, value);
                set_system_time();
                break;
            }
            case 2: {
                char value[4];
                use_dst_time.value ^= 1;
                sprintf(value, "%d", use_dst_time.value);
                write_config_file(PHILZ_SETTINGS_FILE, use_dst_time.key, value);
                set_system_time();
                break;
            }
            case 3: {
                change_date_time_menu();
                break;
            }
        }
    }
}
// end check time zone

void show_touch_gui_menu() {
    const char* headers[] = { "Touch GUI Setup", NULL };

    char item_touch[MENU_MAX_COLS];
    char item_height[MENU_MAX_COLS];
    char item_scroll[MENU_MAX_COLS];
    char item_sens[MENU_MAX_COLS];
    char item_vibra[MENU_MAX_COLS];
    char item_keyrep[MENU_MAX_COLS];
    char item_separator[MENU_MAX_COLS];
    char item_pauseLog[MENU_MAX_COLS];
    char item_logrows[MENU_MAX_COLS];
    char item_brightness[MENU_MAX_COLS];
    char item_dim_time[MENU_MAX_COLS];
    char item_blank_time[MENU_MAX_COLS];
    char item_batt_clock[MENU_MAX_COLS];
    char item_time_settings[MENU_MAX_COLS];

    char* list[] = {
        item_touch,
        item_height,
        item_scroll,
        item_sens,
        item_vibra,
        item_keyrep,
        item_separator,
        item_pauseLog,
        item_logrows,
        item_brightness,
        item_dim_time,
        item_blank_time,
        item_batt_clock,
        item_time_settings,
        "Change Background",
        "Change Menu Colors",
        "Toggle Virtual Keys",
        "Gestures Action Setup",
        NULL
    };

    char tmp[MENU_MAX_COLS];
    for (;;) {
        if (touch_to_validate.value == NO_TOUCH_SUPPORT)
            ui_format_gui_menu(item_touch, "Touch GUI", "Disabled");
        else if (touch_to_validate.value == TOUCH_HIGHLIGHT_ONLY)
            ui_format_gui_menu(item_touch, "Touch GUI", "Select only");
        else if (touch_to_validate.value == DOUBLE_TAP_VALIDATION)
            ui_format_gui_menu(item_touch, "Touch GUI", "Double tap");
        else
            ui_format_gui_menu(item_touch, "Touch GUI", "Full");

        if (menu_height_increase.value == MENU_HEIGHT_INCREASE_0)
            sprintf(tmp, "%ld (default)", MENU_HEIGHT_TOTAL);
        else sprintf(tmp, "%ld (custom)", MENU_HEIGHT_TOTAL);
        ui_format_gui_menu(item_height, "Menu Height", tmp);

        if (scroll_sensitivity.value == SCROLL_SENSITIVITY_0)
            sprintf(tmp, "%ld (default)", scroll_sensitivity.value);
        else sprintf(tmp, "%ld (custom)", scroll_sensitivity.value);
        ui_format_gui_menu(item_scroll, "Scroll Sensitivity", tmp);

        if (touch_accuracy.value == TOUCH_ACCURACY_0)
            sprintf(tmp, "%ld (default)", touch_accuracy.value);
        else sprintf(tmp, "%ld (custom)", touch_accuracy.value);
        ui_format_gui_menu(item_sens, "Touch Accuracy", tmp);

        if (enable_vibrator.value)
            ui_format_gui_menu(item_vibra, "Vibrator", "Enabled");
        else ui_format_gui_menu(item_vibra, "Vibrator", "Disabled");

        if (boardEnableKeyRepeat.value)
            ui_format_gui_menu(item_keyrep, "Key Repeat", "Enabled");
        else ui_format_gui_menu(item_keyrep, "Key Repeat", "Disabled");

        if (show_menu_separation.value)
            ui_format_gui_menu(item_separator, "Menu Separator", "Enabled");
        else ui_format_gui_menu(item_separator, "Menu Separator", "Disabled");

        if (wait_after_install.value)
            ui_format_gui_menu(item_pauseLog, "Pause on Logs", "Enabled");
        else ui_format_gui_menu(item_pauseLog, "Pause on Logs", "Disabled");

        if (min_log_rows.value == 3) //defined by MIN_LOG_ROWS in ui.c
            sprintf(tmp, "%ld (default)", min_log_rows.value);
        else sprintf(tmp, "%ld (custom)", min_log_rows.value);
        ui_format_gui_menu(item_logrows, "Set Log Rows", tmp);

        if (set_brightness.value == BRIGHTNESS_DEFAULT_VALUE)
            sprintf(tmp, "%ld (Default)", set_brightness.value);
        else sprintf(tmp, "%ld", set_brightness.value);
        ui_format_gui_menu(item_brightness, "Set Brightness", tmp);

        if (dim_timeout.value == 0)
            sprintf(tmp, "0 (Disabled)");
        else sprintf(tmp, "%ld mn", dim_timeout.value / 60);
        ui_format_gui_menu(item_dim_time, "Dim Screen Delay", tmp);

        if (blank_timeout.value == 0)
            sprintf(tmp, "0 (Disabled)");
        else sprintf(tmp, "%ld mn", blank_timeout.value / 60);
        ui_format_gui_menu(item_blank_time, "Screen Off Delay", tmp);

        if (show_clock.value && show_battery.value)
            ui_format_gui_menu(item_batt_clock, "Battery/Clock", "Both");
        else if (show_clock.value)
            ui_format_gui_menu(item_batt_clock, "Battery/Clock", "Clock");
        else if (show_battery.value)
            ui_format_gui_menu(item_batt_clock, "Battery/Clock", "Battery");
        else ui_format_gui_menu(item_batt_clock, "Battery/Clock", "Disabled");

        // show current timezone and time in menu
        struct tm *timeptr;
        time_t now = time(NULL);
        timeptr = localtime(&now);

        if (timeptr != NULL) {
            strftime(item_time_settings, sizeof(item_time_settings), "%H:%M", timeptr);
            sprintf(tmp, "%s UTC %s%ld:%02ld", item_time_settings, t_zone.value <= 0 ? "" : "+", t_zone.value, t_zone_offset.value);
        } else {
            sprintf(tmp, "--:--");
        }
        ui_format_gui_menu(item_time_settings, "Time Setup:", tmp);

        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                {
                    char value[12];
                    touch_to_validate.value += 1;
                    if (touch_to_validate.value > 2) {
                        touch_to_validate.value = -1; // NO_TOUCH_SUPPORT
                        sprintf(value, "false");
                    }
                    else if (touch_to_validate.value == 0) // TOUCH_HIGHLIGHT_ONLY
                             sprintf(value, "highlight");
                    else if (touch_to_validate.value == 2) // DOUBLE_TAP_VALIDATION
                        sprintf(value, "double_tap");
                    else    // 1 == FULL_TOUCH_VALIDATION
                        sprintf(value, "1");
                    write_config_file(PHILZ_SETTINGS_FILE, touch_to_validate.key, value);

                    // on touch to validate (1) or double tap to validate (2), we set menu height and touch sensitivity to device defaults
                    if (touch_to_validate.value == FULL_TOUCH_VALIDATION || touch_to_validate.value == DOUBLE_TAP_VALIDATION) {
                        menu_height_increase.value = MENU_HEIGHT_INCREASE_0;
                        show_virtual_keys.value = 1;
                        fast_ui_init();
                        scroll_sensitivity.value = SCROLL_SENSITIVITY_0;
                        sprintf (value, "%d", MENU_HEIGHT_INCREASE_0);
                        write_config_file(PHILZ_SETTINGS_FILE, menu_height_increase.key, value);
                        write_config_file(PHILZ_SETTINGS_FILE, show_virtual_keys.key, "1");
                        sprintf (value, "%ld", SCROLL_SENSITIVITY_0);
                        write_config_file(PHILZ_SETTINGS_FILE, scroll_sensitivity.key, value);
                    } else if (touch_to_validate.value == NO_TOUCH_SUPPORT) {
                        // hide recovery virtual keys by default in this mode since they are not used
                        show_virtual_keys.value = 0;
                        write_config_file(PHILZ_SETTINGS_FILE, show_virtual_keys.key, "0");
                        fast_ui_init();
                    }
                }
                break;
            case 1:
                {
                    char value[5];
                    menu_height_increase.value += (FONT_HEIGHT) / 4;
                    if (menu_height_increase.value > MENU_HEIGHT_INCREASE_MAX)
                        menu_height_increase.value = MENU_HEIGHT_INCREASE_MIN;

                    sprintf(value, "%ld", menu_height_increase.value);
                    write_config_file(PHILZ_SETTINGS_FILE, menu_height_increase.key, value);
                    //refresh all gui variables, call before an eventual ui_print
                    fast_ui_init();
                }
                break;
            case 2:
                {
                    char value[5];
                    scroll_sensitivity.value += SCROLL_SENSITIVITY_MIN;
                    if (scroll_sensitivity.value > SCROLL_SENSITIVITY_MAX)
                        scroll_sensitivity.value = SCROLL_SENSITIVITY_MIN;

                    sprintf(value, "%ld", scroll_sensitivity.value);
                    write_config_file(PHILZ_SETTINGS_FILE, scroll_sensitivity.key, value);
                }
                break;
            case 3:
                {
                    char value[5];
                    touch_accuracy.value += 2;
                    if (touch_accuracy.value > 11)
                        touch_accuracy.value = 1;

                    sprintf(value, "%ld", touch_accuracy.value);
                    write_config_file(PHILZ_SETTINGS_FILE, touch_accuracy.key, value);
                }
                break;
            case 4:
                {
                    char value[5];
                    enable_vibrator.value ^= 1;
                    sprintf(value, "%d", enable_vibrator.value);
                    write_config_file(PHILZ_SETTINGS_FILE, enable_vibrator.key, value);
                }
                break;
            case 5:
                {
                    char value[5];
                    boardEnableKeyRepeat.value ^= 1;
                    sprintf(value, "%d", boardEnableKeyRepeat.value);
                    write_config_file(PHILZ_SETTINGS_FILE, boardEnableKeyRepeat.key, value);
                }
                break;
            case 6:
                {
                    char value[5];
                    show_menu_separation.value ^= 1;
                    sprintf(value, "%d", show_menu_separation.value);
                    write_config_file(PHILZ_SETTINGS_FILE, show_menu_separation.key, value);
                }
                break;
            case 7:
                {
                    char value[5];
                    wait_after_install.value ^= 1;
                    sprintf(value, "%d", wait_after_install.value);
                    write_config_file(PHILZ_SETTINGS_FILE, wait_after_install.key, value);
                }
                break;
            case 8:
                {
                    char value[5];
                    min_log_rows.value += 1;
                    if (min_log_rows.value > 6)
                        min_log_rows.value = 3;
                    sprintf(value, "%ld", min_log_rows.value);
                    write_config_file(PHILZ_SETTINGS_FILE, min_log_rows.key, value);
                    fast_ui_init();
                    ui_print("Reserved %ld log rows.\n", min_log_rows.value);
                }
                break;
            case 9:
                toggle_brightness();
                break;
            case 10:
                {
                    char value[10];
                    dim_timeout.value += 60;
                    if (dim_timeout.value > 300)
                        dim_timeout.value = 0;

                    sprintf(value, "%ld", dim_timeout.value);
                    write_config_file(PHILZ_SETTINGS_FILE, dim_timeout.key, value);
                }
                break;
            case 11:
                {
                    char value[10];
                    blank_timeout.value += 60;
                    if (blank_timeout.value > 1800)
                        blank_timeout.value = 0;
                    else if (blank_timeout.value > 900)
                        blank_timeout.value = 1800;
                    else if (blank_timeout.value > 600)
                        blank_timeout.value = 900;
                    else if (blank_timeout.value > 300)
                        blank_timeout.value = 600;
                    else if (blank_timeout.value > 180)
                        blank_timeout.value = 300;
                    else if (blank_timeout.value > 60)
                        blank_timeout.value = 180;

                    sprintf(value, "%ld", blank_timeout.value);
                    write_config_file(PHILZ_SETTINGS_FILE, blank_timeout.key, value);
                }
                break;
            case 12:
                {
                    char value[5];
                    if (show_battery.value && show_clock.value) show_clock.value = 0;
                    else if (show_battery.value && !show_clock.value) {
                        show_battery.value = 0;
                        show_clock.value = 1;
                    }
                    else if (!show_battery.value && show_clock.value) show_clock.value = 0;
                    else if (!show_battery.value && !show_clock.value) {
                        show_battery.value = 1;
                        show_clock.value = 1;
                    }
                    sprintf(value, "%d", show_battery.value);
                    write_config_file(PHILZ_SETTINGS_FILE, show_battery.key, value);
                    sprintf(value, "%d", show_clock.value);
                    write_config_file(PHILZ_SETTINGS_FILE, show_clock.key, value);
                }
                break;
            case 13:
                show_time_settings_menu();
                break;
            case 14:
                browse_background_image();
                break;
            case 15:
                change_menu_color();
                break;
            case 16:
                {
                    char value[5];
                    show_virtual_keys.value ^= 1;
                    sprintf(value, "%d", show_virtual_keys.value);
                    write_config_file(PHILZ_SETTINGS_FILE, show_virtual_keys.key, value);
                    fast_ui_init();
                }
                break;
            case 17:
                gestures_action_setup();
                break;
        }
    }
}
//end show GUI Preferences menu
//-------- End PhilZ Touch GUI Preferences


/****************************************/
/*   Start Clone ROM to custom_rom.zip  */
/****************************************/

//start make_update_zip function
static int rom_files_total = 0;
static int rom_files_count = 0;

static void rom_zip_callback(const char* filename) {
    if (filename == NULL)
        return;
    //const char* justfile = basename(filename); //not needed as zip stdout starts with "adding:", unlike tar stdout
    char tmp[PATH_MAX];
    //strcpy(tmp, justfile);
    strcpy(tmp, filename);
    if (tmp[strlen(tmp) - 1] == '\n')
        tmp[strlen(tmp) - 1] = '\0';
    if (strlen(tmp) == 0)
        return;

    tmp[ui_get_text_cols() - 1] = '\0';
    rom_files_count++;
    ui_set_log_stdout(0);
    ui_set_nandroid_print(1, 1);
    ui_print("%s\n", tmp);
    ui_set_log_stdout(1);
    ui_set_nandroid_print(0, 0);

    if (rom_files_total != 0)
        ui_set_progress((float)rom_files_count / (float)rom_files_total);
}

static void get_directory_stats(const char* directory) {
    char tmp[PATH_MAX];
    char count_text[100];

    sprintf(tmp, "find %s | wc -l > /tmp/dircount", directory);
    __system(tmp);
    FILE* f = fopen("/tmp/dircount", "r");
    if (f == NULL)
        return;

    if (fgets(count_text, sizeof(count_text), f) == NULL) {
        fclose(f);
        return;
    }

    size_t len = strlen(count_text);
    if (count_text[len - 1] == '\n')
        count_text[len - 1] = '\0';

    fclose(f);
    rom_files_count = 0;
    rom_files_total = atoi(count_text);
    ui_reset_progress();
    ui_show_progress(1, 0);
}

static int rom_zip_wrapper(const char* backup_path) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd %s; (zip -r ../custom_rom_$(date +%%Y%%m%%d_%%H%%M%%S).zip *) 2> /proc/self/fd/1 ; exit $?", backup_path);
    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute zip.\n");
        return -1;
    }

    ui_clear_key_queue();
    ui_print("Press Back to cancel.\n \n");
    // support dim screen durin zip operation
    struct timeval now;
    time_t last_key_ev;
    gettimeofday(&now, NULL);
    last_key_ev = now.tv_sec;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
        int key_event = key_press_event();
        gettimeofday(&now, NULL);
        if (key_event != NO_ACTION) {
            // a key press event was detected: reset dim timeout on touch
            last_key_ev = now.tv_sec;

            // wake-up screen brightness on key event
            if (is_dimmed)
                ui_dim_screen(false);

            // support cancel nandroid tar backup
            if (key_event == GO_BACK) {
            ui_print("Cancelling custom rom job...\n");
            ui_clear_key_queue();
            __pclose(fp);
            sync();
            return -1;
            }
        } else if (!is_dimmed && dim_timeout.value != 0 && (now.tv_sec - last_key_ev) >= dim_timeout.value) {
            // dim screen on timeout
            ui_dim_screen(true);
        }

        tmp[PATH_MAX - 1] = '\0';
        rom_zip_callback(tmp);
    }
    return __pclose(fp);
}

static int make_update_zip(const char* source_path, const char* target_volume) {
    if (ensure_path_mounted(target_volume) != 0) {
        ui_print("Can't mount %s\n", target_volume);
        return -1;
    }

    int ret = 0;
    char cmd[PATH_MAX];
    char tmp_path[PATH_MAX];
    sprintf(tmp_path, "%s/%s/tmp", target_volume, CUSTOM_ROM_PATH);

    ui_print("\nPreparing ROM structure...\n");
    sprintf(cmd, "rm -rf %s", tmp_path);
    __system(cmd);
    sprintf(cmd, "mkdir -p %s/META-INF/com/google/android", tmp_path);
    __system(cmd);

    if (source_path == NULL) {
        // create a nandroid backup from existing ROM and use it for update.zip
        backup_recovery = 0, backup_wimax = 0, backup_data = 0, backup_cache = 0, backup_sdext = 0;
        nandroid_force_backup_format("tar");
        ret = nandroid_backup(tmp_path);
        nandroid_force_backup_format("");
        backup_recovery = 1, backup_wimax = 1, backup_data = 1, backup_cache = 1, backup_sdext = 1;
        if (ret != 0) {
            LOGE("Error while creating a nandroid image!\n");
            return ret;
        }
    } else if (nandroid_add_preload.value) {
        sprintf(cmd, "cd %s; mv boot.* system.* preload.* %s", source_path, tmp_path);
    } else {
        sprintf(cmd, "cd %s; mv boot.* system.* %s", source_path, tmp_path);
    }
    __system(cmd);

    sprintf(cmd, "cp /res/image-binary %s/META-INF/com/google/android/update-binary", tmp_path);
    __system(cmd);
    sprintf(cmd, "cp /res/image-edify %s", tmp_path);
    __system(cmd);

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_print("\nCreating update.zip....\n");
    get_directory_stats(tmp_path);
    ensure_path_mounted("/system"); // zip is a dynamic binary!
    ret = rom_zip_wrapper(tmp_path);
    ensure_path_unmounted("/system");

    //restore nandroid backup source folder
    if (source_path != NULL) {
        sprintf(cmd, "cd %s; mv boot.* system.* preload.* %s", tmp_path, source_path);
        __system(cmd);
    }

    //remove tmp folder
    sprintf(cmd, "rm -rf '%s'", tmp_path);
    __system(cmd);

    if (ret != 0) {
        return print_and_error("Error while making a zip image!\n", ret);
    }

    finish_nandroid_job();
    ui_print("Custom ROM saved in %s/%s\n", target_volume, CUSTOM_ROM_PATH);
    return 0;
}

//select target volume for custom ROM
static void custom_rom_target_volume(const char* source_path) {
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    const char* headers[] = {"Choose custom ROM target", "", NULL};
    char* list[MAX_NUM_MANAGED_VOLUMES + 1];
    char list_prefix[] = "Create ROM in ";
    char buf[80];
    memset(list, 0, sizeof(list));
    sprintf(buf, "%s%s", list_prefix, primary_path);
    list[0] = strdup(buf);

    int i;
    if (extra_paths != NULL) {
        for(i = 0; i < num_extra_volumes; i++) {
            sprintf(buf, "%s%s", list_prefix, extra_paths[i]);
            list[i + 1] = strdup(buf);
        }
    }
    list[num_extra_volumes + 1] = NULL;

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    if (chosen_item != GO_BACK && chosen_item != REFRESH)
        make_update_zip(source_path, list[chosen_item] + strlen(list_prefix));

    free(list[0]);
    if (extra_paths != NULL) {
        free_string_array(extra_paths);
        for(i = 0; i < num_extra_volumes; i++)
            free(list[i + 1]);
    }
}

// select nandroid backup to make ROM
static void choose_nandroid_menu() {
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    const char* headers[] = {
        "Choose a nandroid backup",
        "to export",
        "",
        NULL
    };
    char* list[MAX_NUM_MANAGED_VOLUMES + 1];
    char list_prefix[] = "Choose from ";
    char buf[80];
    memset(list, 0, sizeof(list));
    sprintf(buf, "%s%s", list_prefix, primary_path);
    list[0] = strdup(buf);

    int i;
    if (extra_paths != NULL) {
        for(i = 0; i < num_extra_volumes; i++) {
            sprintf(buf, "%s%s", list_prefix, extra_paths[i]);
            list[i + 1] = strdup(buf);
        }
    }
    list[num_extra_volumes + 1] = NULL;

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    if (chosen_item == GO_BACK || chosen_item == REFRESH)
        goto out;
        
    if (ensure_path_mounted(list[chosen_item] + strlen(list_prefix)) != 0) {
        ui_print("Can't mount %s\n", list[chosen_item] + strlen(list_prefix));
        goto out;
    }

    char tmp[PATH_MAX];
    char* file = NULL;
    sprintf(tmp, "%s/clockworkmod/backup/", list[chosen_item] + strlen(list_prefix));
    file = choose_file_menu(tmp, NULL, headers);
    if (file == NULL)
        goto out;

    //ensure backup is in tar format
    sprintf(tmp, "%s/system.ext4.tar", file);
    if (!file_found(tmp)) {
        LOGE("Backup must be in tar format!\n");
    } else {
        sprintf(tmp, "Yes - Create using %s", BaseName(file));
        if (confirm_selection("Confirm Create ROM?", tmp))
            custom_rom_target_volume(file);
    }

    free(file);

out:
    free(list[0]);
    if (extra_paths != NULL) {
        free_string_array(extra_paths);
        for(i = 0; i < num_extra_volumes; i++)
            free(list[i + 1]);
    }
}

//start Clone ROM to update.zip menu
void custom_rom_menu() {
    const char* headers[] = {
        "Create Custom ROM",
        "",
        NULL
    };

    char* list[] = {
        "Create from Current ROM",
        "Create from Previous Backup",
        "Settings...",
        NULL
    };

    is_custom_backup = 1;
    for (;;) {
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                custom_rom_target_volume(NULL);
                break;
            case 1: 
                choose_nandroid_menu();
                break;
            case 2: 
                misc_nandroid_menu();
                break;
        }
    }

    is_custom_backup = 0;
}
//-------- End Clone ROM to update.zip


/*******************************/
/*   Start Recovery Lock Code  */
/*******************************/

// drop invalid key codes: touch events (BTN_*) and faked ignored event key (KEY_ESC)
static int is_valid_key_code(int key_code) {
    if (key_code == KEY_ESC)
        return 0;
    if (key_code >= BTN_TOOL_PEN && key_code <= BTN_GEAR_UP)
        return 0;
    return 1;
}

// setup / change recovery lock passkey
static void recovery_change_passkey() {
    int pass_key[RECOVERY_LOCK_MAX_CHARS];
    char pass_display[128] = "";
    char pass_string[128] = "";
    int i = 0;
    int key_match = 1;

    // type in new passkey
    ui_SetShowText(false);
    ui_clear_key_queue();
    for (i = 0; i < RECOVERY_LOCK_MAX_CHARS; ++i) {
        char tmp[64];
        ui_update_screen(); // remove previous drawing
        draw_visible_text_line(2, "* New Passkey (6 chars) *", 1);
        draw_visible_text_line(4, pass_display, 1);
        while (1) {
            pass_key[i] = ui_wait_key();
            if (is_valid_key_code(pass_key[i]))
                break;
        }
        sprintf(tmp, "%d,", pass_key[i]);
        strcat(pass_string, tmp);
        strcat(pass_display, "* ");
        // LOGI("new_key[%d]=%ld\n", i, pass_key[i]); // debug
    }

    // verify passkey
    strcpy(pass_display, "");
    ui_clear_key_queue();
    for (i = 0; i < RECOVERY_LOCK_MAX_CHARS; ++i) {
        ui_update_screen();
        draw_visible_text_line(2, "* Retype Passkey *", 1);
        draw_visible_text_line(4, pass_display, 1);

        int input_key;
        while (1) {
            input_key = ui_wait_key();
            if (is_valid_key_code(input_key))
                break;
        }

        if (input_key != pass_key[i]) {
            key_match = 0;
            LOGE("Passkey doesn't match\n"); // will remove any previous drawing (actual ui_print will show on returning to menu)
            draw_visible_text_line(2, "* Passkey mismatch! *", 1);
            draw_visible_text_line(4, "Press a key to exit", 1);
            ui_clear_key_queue();
            ui_wait_key();
            break;
        }
        strcat(pass_display, "* ");
        // LOGI("new_key[%d]=%ld\n", i, pass_key[i]); // debug
    }

    // if passkey was entered twice correctly, save to lock file
    if (key_match) {
        write_string_to_file(RECOVERY_LOCK_FILE, pass_string);
        ui_print("Recovery Lock is enabled\n");
    }

    ui_SetShowText(true);
}

// check if recovery needs to be locked and prompt for a pass key if it is defined
void check_recovery_lock() {
    LOGI("Checking for recovery lock...\n");
    if (!file_found(RECOVERY_LOCK_FILE)) {
        ensure_path_unmounted(RECOVERY_LOCK_FILE);
        property_set("sys.usb.recovery_lock", "0");
        return;
    }

    char line[1024];
    char *ptr;
    FILE *fp = fopen(RECOVERY_LOCK_FILE, "rb");
    if (fp == NULL) {
        LOGE("failed to open lock file\n");
        ensure_path_unmounted(RECOVERY_LOCK_FILE);
        property_set("sys.usb.recovery_lock", "0");
        return;
    }

    // read password file (one line)
    ptr = fgets(line, sizeof(line), fp);
    fclose(fp);
    ensure_path_unmounted(RECOVERY_LOCK_FILE);
    if (ptr == NULL) {
        LOGE("failed to read lock file\n");
        property_set("sys.usb.recovery_lock", "0");
        return;
    }

    // hide screen menus and ui_print
    // this function can be called on recovery start (show_text == 0) or from menus (show_text == 1) to lock recovery or reset password
    bool visible = ui_IsTextVisible();
    ui_SetShowText(false);

    // parse the password: "key1,key2,key3,key4..."
    int i = 0;
    int pass_chars = 0;
    char** pass_key = (char**)malloc((RECOVERY_LOCK_MAX_CHARS) * sizeof(char*));
    memset(pass_key, 0, sizeof(pass_key));
    ptr = strtok(line, ", \n");
    while (i < RECOVERY_LOCK_MAX_CHARS && ptr != NULL) {
        pass_key[i] = strdup(ptr);
        // LOGI("Passkey[%d]=%s\n", i, pass_key[i]); // debug
        ptr = strtok(NULL, ", \n");
        ++i;
    }

    pass_chars = i;

    // lock adb (should be already locked if on recovery boot)
    property_set("sys.usb.recovery_lock", "1");
    // property_set("sys.usb.recovery_lock", "0"); // debug

    // prompt for pass key
    // key_err: number of wrong key input by user
    long int key_input[RECOVERY_LOCK_MAX_CHARS] = { 0 };
    int key_err = 0;
    char pass_display[128] = "";
    char trials_left_message[64];

    if (pass_chars != RECOVERY_LOCK_MAX_CHARS) {
        // don't allow pass if key file was tempered with
        // this will only allow passwords of RECOVERY_LOCK_MAX_CHARS characters
        LOGI("unusual passkey length (%d)\n", pass_chars);
        key_err = RECOVERY_LOCK_MAX_ERROR;
    }

    ui_clear_key_queue();

    // workaround to refresh display buffers with active background
    // this is needed to avoid first passkey prompt screen background to be black on recovery start
    draw_visible_text_line(2, "* Recovery Locked *", 1);
    ui_update_screen(); // will wipe above text line and properly refresh the buffers

    while (key_err < RECOVERY_LOCK_MAX_ERROR) {
        // prompt for the key
        for (i = 0; i < RECOVERY_LOCK_MAX_CHARS; ++i) {
            sprintf(trials_left_message, "Trials left: %d", RECOVERY_LOCK_MAX_ERROR - key_err);
            draw_visible_text_line(2, "* Recovery Locked *", 1);
            draw_visible_text_line(3, trials_left_message, 1);
            draw_visible_text_line(5, pass_display, 1);
            while (1) {
                key_input[i] = ui_wait_key();
                if (is_valid_key_code(key_input[i]))
                    break;
            }
            strcat(pass_display, "* ");
            ui_update_screen(); // remove any previous writing to screen
            // LOGI("key press=%d\n", key_input[i]); // debug
        }

        // reset displayed input key on screen
        strcpy(pass_display, "");

        // check if the input key length is valid
        if (i != pass_chars) {
            ++key_err;
            continue;
        }

        // verify the input key
        for (i = 0; i < pass_chars; ++i) {
            if (key_input[i] != strtol(pass_key[i], NULL, 10)) {
                ++key_err;
                break;
            }
        }

        // break loop if the input key is valid
        if (i == pass_chars)
            break;
    }

    for (i = 0; i < pass_chars; ++i) {
        free(pass_key[i]);
    }
    free(pass_key);

    if (key_err >= RECOVERY_LOCK_MAX_ERROR) {
        LOGI("Max pass key errors reached!\n");
        ui_set_background(BACKGROUND_ICON_ERROR); // will also remove previous drawing
        draw_visible_text_line(2, "Wrong pass!", 1);
        draw_visible_text_line(4, "Press a key to reboot", 1);
        ui_clear_key_queue();
        ui_wait_key();
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    }

    // unlock and continue
    LOGI("Recovery unlocked\n");
    ui_SetShowText(visible);
    property_set("sys.usb.recovery_lock", "0");
}

// setup a passkey to lock recovery
void show_recovery_lock_menu() {
    // prompt for password if needed before allowing any modification
    check_recovery_lock();

    const char* headers[] = {
        "Recovery Lock Setup",
        "",
        NULL
    };

    char* list[] = {
        "Change Recovery Passkey",
        "Disable Recovery Lock",
        NULL
    };

    for (;;) {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item) {
            case 0: {
                recovery_change_passkey();
                break;
            }
            case 1: {
                if (confirm_selection("Disable Recovery Lock ?", "Yes - Disable Lock")) {
                    delete_a_file(RECOVERY_LOCK_FILE);
                }
                break;
            }
        }
    }
}
//-------- End recovery lock code

// display and log the libtouch_gui version
// called on recovery start (onscreen == ) and from About menu
void print_libtouch_version(int onscreen) {
    if (onscreen)
        ui_print("Touch GUI revision: " EXPAND(LIBTOUCH_GUI_VERSION) "\n");
    else
        LOGI("Touch GUI revision: " EXPAND(LIBTOUCH_GUI_VERSION) "\n");
}
