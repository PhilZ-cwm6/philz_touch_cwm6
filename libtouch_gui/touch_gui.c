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

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <cutils/android_reboot.h>
#include <cutils/properties.h>

#include "common.h"
#include "libcrecovery/common.h" // __system()
#include "minui/minui.h"
#include "recovery_ui.h"
#include "voldclient/voldclient.h"
#include "advanced_functions.h"
#include "recovery_settings.h"
#include "ui.h"

#include "libtouch_gui/gui_settings.h"
#include "libtouch_gui/touch_gui.h"


/******** start philz functions declarartion ********/

// refresh screen and redraw everything
void ui_update_screen() {
    update_screen_locked();
}

// define gui layout default colors and toggle integers
// MENU_TEXT_COLOR:         text in menus
// MENU_BACKGROUND_COLOR:   menus background color
// MENU_HIGHLIGHT_COLOR:    color of highlighted menu
// MENU_SEPARATOR_COLOR:    color of the line separator between menus
// NORMAL_TEXT_COLOR:       log text (ui_print)
// HEADER_TEXT_COLOR:       top text for menu titles, include battery and clock

#define MENU_TEXT_COLOR mtext_code[0], mtext_code[1], mtext_code[2], mtext_code[3]
#define MENU_BACKGROUND_COLOR mbackg_code[0], mbackg_code[1], mbackg_code[2], mbackg_code[3]
#define MENU_HIGHLIGHT_COLOR mhlight_code[0], mhlight_code[1], mhlight_code[2], mhlight_code[3]
#define NORMAL_TEXT_COLOR normal_text_code[0], normal_text_code[1], normal_text_code[2], normal_text_code[3]
#define MENU_SEPARATOR_COLOR mseparator_code[0], mseparator_code[1], mseparator_code[2], mseparator_code[3]
#define HEADER_TEXT_COLOR header_text_code[0], header_text_code[1], header_text_code[2], header_text_code[3]
#define BATT_CLOCK_COLOR batt_clock_code[0], batt_clock_code[1], batt_clock_code[2], batt_clock_code[3]

//#define MENU_TEXT_COLOR 255, 0, 0, 255 //red
//#define MENU_TEXT_COLOR 0, 128, 0, 255 //green
//#define MENU_TEXT_COLOR 255, 160, 49, 255 //yellow-orange
//#define MENU_TEXT_COLOR 0, 0, 0, 255 //black
//#define MENU_TEXT_COLOR 0, 191, 255, 255 //blue-normal
//#define MENU_TEXT_COLOR 0, 247, 255, 255 //light-blue
//#define MENU_HIGHLIGHT_COLOR 0, 191, 255, 153 //blue-normal+alpha transparency 153
//#define MENU_BACKGROUND_COLOR 0, 0, 0, 102 //black-light (alpha transparency 102)
//#define NORMAL_TEXT_COLOR 200, 200, 200, 255 //silver
//#define NORMAL_TEXT_COLOR 0, 247, 255, 255 //light-blue
//#define NORMAL_TEXT_COLOR 255, 0, 0, 255 //red
//#define NORMAL_TEXT_COLOR 0, 128, 0, 255 //green
//#define HEADER_TEXT_COLOR NORMAL_TEXT_COLOR


/*
ui_print_color() usage example:
It is included in recovery_ui.h to include in most code
To do: make it possible to only color the last bottom x lines
// start code
    int color[] = {CYAN_BLUE_CODE};
    ui_print_color(2, color); // enable
    ui_print("%s\n", tmp);
    ui_print_color(0, 0); // disable
// end code
*/

static int uiprint_color_code[4] = {DEFAULT_NORMAL_TEXT_CODE}; // the color for the ui_print
// number of rows to color start from bottom to top
// when 0, it is diabled
static int colored_bottom_rows = 0;

// function called to enable/disable color printing
void ui_print_color(int colored_rows_num, int *color) {
    colored_bottom_rows = colored_rows_num;
    if (colored_rows_num) {
        int i;
        for (i = 0; i < 4; i++)
            uiprint_color_code[i] = color[i];
    }
}

// called from open source parts to ui_print into a preset color
// that way, we do not have to define color codes
// reassign_color_code_array() is needed since we cannot assign an array to a {...} values set
static void reassign_color_code_array(int red, int green, int blue, int alpha, int *color_code) {
    color_code[0] = red;
    color_code[1] = green;
    color_code[2] = blue;
    color_code[3] = alpha;
}

void ui_print_preset_colors(int colored_rows_num, const char* color) {
    int color_code[4];
    if (color == NULL)
        reassign_color_code_array(DEFAULT_UI_PRINT_COLOR, color_code);
    else if (strcmp(color, "red") == 0)
        reassign_color_code_array(NORMAL_RED_CODE, color_code);
    else if (strcmp(color, "cyan") == 0)
        reassign_color_code_array(CYAN_BLUE_CODE, color_code);
    else if (strcmp(color, "green") == 0)
        reassign_color_code_array(LIME_GREEN_CODE, color_code);
    else
        reassign_color_code_array(DEFAULT_UI_PRINT_COLOR, color_code);

    ui_print_color(colored_rows_num, color_code);
}

// set to 1 when touch screen event detected to enable highlight touched menu (not validation)
// while it is 1, we ignore real faked key for highlight on touch (KEY_PAGEUP press)
// also, while 1, we do not always allow refresh of battery capacity to not loose time in opening the file
static int touch_sel = 0;

/*
- first_touched_menu: menu we touch on first touch event
    * first menu is 0. -1 if outside a valid menu
    * used to highlight on first touch by ui_menu_touch_select()
- last_touched_menu: last touched menu on finger lifted
    * first menu is 0. -1 if outside a valid menu
    * used to ensure we double tap on same menu to validate it in double tap mode

Both are used in Full Touch Mode to fix this bug: double tap in full mode selects first menu: happens in gui settings on very fast double taps
      probably menu was still not drawn: touch doesn't register first touch (highlight on touch)
      then, finger lifted is registered when new menu was drawn, so first item was selected
      ----> option 1: ensure show_menu != 0 to register touch events: will break cancel nandroid jobs
      ----> option 2: if first touch when show_menu == 0, set ignore_key_action to 1 (will block menu validations on finger lifted)
      ----> option 3: in full touch mode, ensure first touch and finger lifted were on same menu (first_touched_menu == last_touched_menu)
      ----> ideally, use options 2 and 3
*/
static int first_touched_menu = -1;
static int last_touched_menu = -1;

/*
- now_scrolling == 0: touch to highlight (on first touch) allowed.
- now_scrolling == +1 or -1 to decide of scroll direction up/down in scroll_touch_menu()
- now_scrolling is reset to 0:
    * if finger lifted without moving after first touch, we reset it to 0 AFTER we register finger lifted action (touch to validate code)
    * on touching virtual keys
    * on first touch, if last touch when we lifted finger is older than 0.6 sec
- When scrolling, first touch doesn't highlight (now_scrolling != 0) unless we stop scrolling for 0.6 sec
- Below the 0.6 sec, if we try to select a menu after a scroll (touch and lift without moving), menu is highlighted on finger lifted and isn't validated
- This way, a menu cannot be validated if it was not highlighted on first touch, else it is highlighted on finger lifted and not validated
*/
static int now_scrolling = 0;

// touch scroll speed in pixels /sec
static long scroll_speed = 0;

// ignores a touch event on menu.
// enabled when in friendly log view to avoid activate menu items on finger lifted after exiting log view
int ignore_key_action = 0;

// on first touch, if finger doesn't move too much, allow long press and move gesture action
static int allow_long_press_move = 0;

// avoids calling surface function to set virtual buttons height
// also when 0, virtual key buttons are hidden/disabled
static int virtual_keys_h = 0;


/*
Below times are in milliseconds
- t_first_touch: time of first touch
- t_last_touch: last time we lifted finger where ever it was
  difference between two above timers is used for:
    * long press/lift action
    * to allow highlight on first touch after 1 sec of finger lifted
- t_old_last_touch: the time when we lifted finger before t_last_touch
    * currently only used for double tap gesture action outside a menu
- t_last_menu_touch:  last time we lifted finger while it was on a valid menu
    * currently only used in Double tap validation mode
- t_last_scroll_y: last time we have a touch_y input, 
    * used to calculate scroll speed
*/
static long long  t_last_touch = 0;
static long long t_last_menu_touch = 0;
static long long t_old_last_touch = 0;
static long long t_first_touch = 0;
static long long t_last_scroll_y = 0;

// some of our threads (md5 verify/display), show progress bar while we're still in a menu (get_menu_selection() prompt)
// this is also true in sideload mode
// avoid calling gesture actions during those situations since some are not thread safe (fb2png, brightness toggle)
// they are calling unsafe functions: ensure_path_mounted(), basename, dirname...
int ui_showing_progress_bar() {
    if (gProgressBarType != PROGRESSBAR_TYPE_NONE)
        return 1;

    return 0;
}

//kanged this vibrate stuff from teamwin (thanks guys!)
#define VIBRATOR_TIMEOUT_FILE    "/sys/class/timed_output/vibrator/enable"
#define VIBRATOR_TIME_MS    25
int vibrate_device(int timeout_ms) {
    char str[20];
    int fd;
    int ret;

    fd = open(VIBRATOR_TIMEOUT_FILE, O_WRONLY);
    if (fd < 0)
        return -1;

    ret = snprintf(str, sizeof(str), "%d", timeout_ms);
    ret = write(fd, str, ret);

    close(fd);
    return ret;
}

#define TOUCH_RESET_POS -10000
static int in_touch = 0; //1 == in a touch, 0 == finger lifted
static int touch_x = TOUCH_RESET_POS; // actual touch position
static int touch_y = TOUCH_RESET_POS; // actual touch position
static int first_x = TOUCH_RESET_POS; // x coordinate of first touch after finger was lifted
static int first_y = TOUCH_RESET_POS; // y coordinate of first touch after finger was lifted
static int last_scroll_y = TOUCH_RESET_POS; // last y that triggered an up/down scroll

// we reset gestures whenever finger is lifted
static void reset_gestures() {
    first_x = TOUCH_RESET_POS;
    first_y = TOUCH_RESET_POS;
    touch_x = TOUCH_RESET_POS;
    touch_y = TOUCH_RESET_POS;
    last_scroll_y = TOUCH_RESET_POS;
}

// if vbutton_pressed != -1, a virtual button is being pressed
// once we pressed a virtual button, it will not repeat until we lift finger
// or if BoardEnableKeyRepeat is true, it will repeat
static int vbutton_pressed = -1;

// vk_pressed != -1 when a device virtual key is pressed
// ensure we do not repeat the virtual key until finger is lifted
// this is cause some devices register many times the same touch event
static int vk_pressed = -1;


/*
- Draw the virtual keys on the screen. Does not flip pages.
- Should only be called with gUpdateMutex locked.
- Virtual keys are drawn by draw_virtualkeys_locked() called by draw_screen_locked()
- draw_screen_locked() is called by update_screen_locked() and update_progress_locked()
- show_virtual_keys toggle is implemented in draw_virtualkeys_locked() instead of the caller draw_screen_locked()
  to keep it in philz_gui_defines.c code and not ui.c, but it would ideally be in ui.c
- update_screen_locked() will not do anything until ui_has_initialized == 1
- ui_has_initialized is set to 1 at start of ui_init(), called early on recovery start process (recovery.c)
- update_screen_locked() is called by many functions to update screen, but not by ui_init() on boot
- Very early functions calling it are : ui_print() and ui_set_background()
- To avoid virtual keys being drawn on screen on start for a short time when they are disabled, we have 2 choices:
    * set show_virtual_keys.value = 0; in gui_settings.c.
      That way, they are not drawn on screen on very early init before philz gui settings are read
      from settings file
    * move the initial ui_print of recovery info just below the device_recovery_start() call in recovery.c
      since that function will read the user recovery settings file
- Do not call any LOGE or ui_print here, or it will trigger another call to pthread_mutex_lock(&gUpdateMutex)
  while it is locked here as called by update_screen_locked() after a lock to gUpdateMutex
*/
static void draw_virtualkeys_locked() {
    if (show_virtual_keys.value) {
        gr_surface surface = gVirtualKeys;
        int iconWidth = gr_get_width(surface);
        int iconHeight = gr_get_height(surface);
        int iconX = (gr_fb_width() - iconWidth) / 2;
        int iconY = (gr_fb_height() - iconHeight);
        gr_blit(surface, 0, 0, iconWidth, iconHeight, iconX, iconY);

        // draw highlight key line
        gr_color(VK_KEY_HIGHLIGHT_COLOR);
        gr_fill(iconX, iconY-2,
                iconX+iconWidth, iconY);
    }
}

// start fast ui init to apply menu hight toggle in realtime
void fast_ui_init(void) {
    pthread_mutex_lock(&gUpdateMutex);

    if (show_virtual_keys.value) {
        gr_surface surface = gVirtualKeys;
        virtual_keys_h = gr_get_height(surface);
        text_rows = (gr_fb_height() - virtual_keys_h) / FONT_HEIGHT;
    } else {
        virtual_keys_h = 0;
        text_rows = gr_fb_height() / FONT_HEIGHT;
    }

#ifdef RECOVERY_TOUCH_DEBUG
    LOGI("rows=%d\n", text_rows);
    LOGI("board_h=%d\n", gr_fb_height());
    LOGI("board_w=%d\n", gr_fb_width());
    LOGI("vkey_h=%d\n", virtual_keys_h);
#endif

    // sets number of rows in menu to start scroll
    max_menu_rows = ((text_rows - min_log_rows.value) * FONT_HEIGHT) / MENU_HEIGHT_TOTAL;

    // To respect user defined min_log_rows.value depending on used menu_height, we check if available_rows are at least equal to min_log_rows.value
    // This will also ensure that on very large menu heights with a small font for logs, available_rows is never < 0
    // In draw_touch_menu(), we define available_rows:
    //    - available_rows = (gr_fb_height() - (row * MENU_HEIGHT_TOTAL) - virtual_keys_h) / FONT_HEIGHT
    //    - row can be actually equal to (max_menu_rows + 1) for menus that need to scroll
    //    - the +1 is used to draw the last separator line between menus and logs
    while (((gr_fb_height() - ((max_menu_rows + 1) * MENU_HEIGHT_TOTAL) - virtual_keys_h) / FONT_HEIGHT) < min_log_rows.value)
    {
        --max_menu_rows;
    }

    if (max_menu_rows > MENU_MAX_ROWS)
        max_menu_rows = MENU_MAX_ROWS;
    if (text_rows > MAX_ROWS) text_rows = MAX_ROWS;
    
    pthread_mutex_unlock(&gUpdateMutex);
}

// refresh bitmaps from ramdisk (if we alter them after recovery started with ui_init())
void fast_ui_init_png() {
    pthread_mutex_lock(&gUpdateMutex);

    int i;
    for(i = 0; BITMAPS[i].name != NULL; ++i) {
        int result = res_create_surface(BITMAPS[i].name, BITMAPS[i].surface);
        if (result < 0) {
            LOGE("Missing bitmap %s\n(Code %d)\n", BITMAPS[i].name, result);
        }
    }

    pthread_mutex_unlock(&gUpdateMutex);
}

// check if a touch ends on a valid menu item and returns the menu item touched
// first menu == 0. If no valid menu, returns -1
static int ui_valid_menu_touch(int pos_y) {
    int ret = -1;
    if (show_menu <= 0) {
        ret = -2;
    } else {
        int i = 0;
        int j = 0;
        if (menu_items - menu_show_start + menu_top >= max_menu_rows) {
            // we have many menu items, they need to scroll
            j = max_menu_rows - menu_top;
        }
         else {
            j = menu_items - menu_show_start;
        }
        for(i = menu_show_start; i < (menu_show_start + j); ++i) {
            if (pos_y > ((i - menu_show_start + menu_top) * MENU_HEIGHT_TOTAL) &&
                    pos_y < ((i - menu_show_start + menu_top + 1) * MENU_HEIGHT_TOTAL))
                ret = i; // yes, we touched a selectable menu
        }
    }

    return ret;
}

// touch to highlight function, not when validating
// will highlight menu on first touch
// called by get_menu_selection() on HIGHLIGHT_ON_TOUCH. That is, only when KEY_PAGEUP is faked
// HIGHLIGHT_ON_TOUCH is returned by touch_handle_key() only when KEY_PAGEUP is pressed and touch_sel == 1:
// ui_menu_touch_select() is called only when touch_sel = 1
// first_touched_menu is menu we touched on first touch event
int ui_menu_touch_select() {
    pthread_mutex_lock(&gUpdateMutex);

    if (show_menu > 0) {
#ifdef RECOVERY_TOUCH_DEBUG
        // debug code to print variables to the log area
        LOGI("first_touched_menu=%d\n", first_touched_menu);
        LOGI("menu_sel=%d\n", menu_sel);
        LOGI("menu_show_start=%d\n", menu_show_start);
        LOGI("menu_top=%d\n", menu_top);
        LOGI("max_menu_rows=%d\n", max_menu_rows);
        LOGI("char_h=%d\n", FONT_HEIGHT);
        LOGI("menu_h=%d\n", MENU_HEIGHT_TOTAL);
        LOGI("menu_items=%d\n", menu_items);
#endif
        if (first_touched_menu >= 0 && first_touched_menu != menu_sel) {
            menu_sel = first_touched_menu;
            update_screen_locked();
        }
    }

    // touch_sel = 0: now real KEY_PAGEUP is restored to its default action
    touch_sel = 0;
    pthread_mutex_unlock(&gUpdateMutex);
    return menu_sel;
}

/*
- Hack around original cwm draw_text_line()
- This one adds a third argument to support align
- Do not call any LOGE or ui_print here, or it will trigger another call to pthread_mutex_lock(&gUpdateMutex)
  while it is locked here as called by update_screen_locked() after a lock to gUpdateMutex
*/
#define LEFT_ALIGN 0
#define CENTER_ALIGN 1
#define RIGHT_ALIGN 2
void draw_text_line(int row, const char* t, int height, int align) {
    int col = 0;
    if (t[0] != '\0') {
        int length = strnlen(t, MENU_MAX_COLS) * FONT_WIDTH;
        switch(align) {
            case LEFT_ALIGN:
                col = 1;
                break;
            case CENTER_ALIGN:
                col = ((gr_fb_width() - length) / 2);
                break;
            case RIGHT_ALIGN:
                col = gr_fb_width() - length - 1;
                break;
        }

        // Actual size vs position of text to draw.
        // This way we center it bottom to top in the new heigher row
        gr_text(col, ((row + 1) * height) - ((height - FONT_HEIGHT) / 2) - 1, t, 0);
    }
}

// draw and make visible a text line over current screen (no whole screen redraw)
// row == 0 is top screen
void draw_visible_text_line(int row, const char* t, int align) {
    pthread_mutex_lock(&gUpdateMutex);

    int col = 0;
    if (t[0] != '\0') {
        int length = strnlen(t, MENU_MAX_COLS) * FONT_WIDTH;
        switch(align) {
            case LEFT_ALIGN:
                col = 1;
                break;
            case CENTER_ALIGN:
                col = ((gr_fb_width() - length) / 2);
                break;
            case RIGHT_ALIGN:
                col = gr_fb_width() - length - 1;
                break;
        }

        gr_color(CYAN_BLUE_CODE);
        gr_text(col, (row + 1) * FONT_HEIGHT, t, 0);
        gr_flip();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

// - Draw clock and battery stats
// - Do not call any LOGE or ui_print here, or it will trigger another call to pthread_mutex_lock(&gUpdateMutex)
//   while it is locked here as called by update_screen_locked() after a lock to gUpdateMutex
// - Errors: -1 (init value, should never be displayed), -2 (fopen error), -3 (fgets error)
static int last_batt_level = -1;
static int get_batt_stats(void) {
    char value[6];
    static int count = -1;
    if (touch_sel && count >= 0 && count < 5) {
        // touch_sel == 1: do not loose time opening battery stats file on each touch to highlight action
        // count >= 0 to allow load of battery stats on first draw of screen
        // count < 5: allow refresh battery stats each five touches to highlight
        ++count;
        return last_batt_level;
    }

    if (count < 0 || count >= 5)
        count = 0;

    FILE* fp = fopen(libtouch_flags.battery_level_path, "rt");
    if (fp == NULL) {
        last_batt_level = -2;
        return last_batt_level;
    }

    if (fgets(value, sizeof(value), fp) != NULL) {
        last_batt_level = atoi(value);
        if (last_batt_level > 100)
            last_batt_level = 100;
        else if (last_batt_level < 0)
            last_batt_level = 0;
    } else {
        last_batt_level = -3;
    }

    fclose(fp);
    return last_batt_level;
}

// Do not call any LOGE or ui_print here, or it will trigger another call to pthread_mutex_lock(&gUpdateMutex)
// while it is locked here as called by update_screen_locked() after a lock to gUpdateMutex
static void draw_battery_clock() {
    // read battery % to display
    int batt_level = 0;
    batt_level = get_batt_stats();
    if (batt_level < 21)
        gr_color(NORMAL_RED_CODE);
    else gr_color(BATT_CLOCK_COLOR);

    // read current time for clock 
    struct tm *timeptr;
    time_t now;
    now = time(NULL);
    timeptr = localtime(&now);

    // write battery % and clock
    char batt_clock[40] = "";
    if (show_battery.value)
        sprintf(batt_clock, "[%d%%]", batt_level);

    if (show_clock.value && timeptr != NULL) {
        char tmp[15];
        char timeformat[15];
        strftime(timeformat, sizeof(timeformat), "%H:%M", timeptr);
        sprintf(tmp, "%s%s", strlen(batt_clock) == 0 ? "" : " ", timeformat);
        strcat(batt_clock, tmp);
    }

    draw_text_line(0, batt_clock, MENU_HEIGHT_TOTAL, RIGHT_ALIGN);
}

/*
This will draw the menu while scrolling - scroll_touch_menu():
- menu_items (range 1 to x):
    * number of menu items + additional Go Back menu.
    * set when calling ui_start_menu() in get_menu_selection()
- menu_sel (range 0 to x):
    * selected/highlighted menu: when we press UP/DOWN keys, get_menu_selection() will increment the selected menu (selected++ / selected--)
    * range starts at 0, not 1 for top menu item to last menu item (menu_items - 1)
    * after that, ui_menu_select() is called to identify if we need to scroll and set below values
    * ui_menu_select() will also call update_screen_locked() if the selected menu changes
    * update_screen_locked() will call draw_screen_locked() which is redirected to draw_touch_menu()
    * this will redraw whole screen
- menu_show_start (range 0 to x):
    * it is the top showing menu. If we have a short menu list not needing to scroll, menu_show_start will be 0
    * as soon as we start to scroll in a menu that's too long to fit screen, menu_show_start will increase or decrease
- menu_top (range 0 to x):
    * this is the the row number from top where first menu starts. row 0 is usually for the clock/battery, recovery name (headers)
    * additional rows are used for menu headers
    * it is set in ui_start_menu() and will be equal to the number of headers lines (first header row being 0)
- max_menu_rows (range 1 to x):
    * this is set in ui_init() on recovery start and fast_ui_init()
    * it is the maximum menu rows number that screen can display INCLUDING top header rows (menu_top) but EXCLUDING bottom log rows and virtual keys height
    * it is equal to text_rows - reserved bottom lines for ui_print logs (min_log_rows.value variable) - height of virtual buttons
text_rows:
    * is the total available number of rows to write text, from top to bottom of screen
    * it solely depends on device screen height and font height: gr_fb_height() / FONT_HEIGHT

Our scroll_touch_menu() will basically increment++ or increment-- the menu_show_start using now_scrolling (can be +1 or -1 depending on scroll direction)
We then check if menu_show_start is less than 0 (stop scrolling up) or that bottom menu item is showing: stop scrolling down
If menu_show_start changed, that is we scrolled, then we update whole screen calling update_screen_locked()
We call scroll_touch_menu() only if now_scrolling != 0
*/
static void scroll_touch_menu() {
    pthread_mutex_lock(&gUpdateMutex);

    // do some kinetics
    int menu_jump;
    if (in_touch) {
        // only jump menus (faster scroll) on finger lifted
        // no need to jump more than 5 menus
        menu_jump = 0;
    } else {
        menu_jump = scroll_speed / (gr_fb_height() * 2);
#ifdef RECOVERY_TOUCH_DEBUG
        LOGI("menu_jump=%d\n", menu_jump);
#endif
        if (menu_jump > 5)
            menu_jump = 5;
    }

    int old_menu_show_start = menu_show_start;
    menu_show_start += now_scrolling + (now_scrolling > 0 ? menu_jump : -menu_jump);

    // Check when to stop scrolling down
    // WARNING: this can set menu_show_start to a negative value on short menus
    //          we fix it during next check for scroll up
    //  - now_scrolling = 0: allow highlight on touch and validation on finger lifted
    //    needed because of "if (key_queue_len == 0 && (t_first_touch - t_last_touch) > 500)" in first touch
    // - ui_clear_key_queue(): drop remaining kinetics
    //   seems uselss as queue is quickly emptied if screen isn't refreshed
    if (menu_items - menu_show_start + menu_top < max_menu_rows) {
        menu_show_start = menu_items - max_menu_rows + menu_top;
        now_scrolling = 0;
        // ui_clear_key_queue();
    }

    // Check when to stop scrolling up
    //  - must be done after check to stop scrolling down
    //  - now_scrolling = 0: allow highlight on touch and validation on finger lifted
    //    needed because of "if (key_queue_len == 0 && (t_first_touch - t_last_touch) > 500)" in first touch
    // - ui_clear_key_queue(): drop remaining kinetics
    //   seems uselss as queue is quickly emptied if screen isn't refreshed
    if (menu_show_start < 0) {
        menu_show_start = 0;
        now_scrolling = 0;
        // ui_clear_key_queue();
    }
    
    if (menu_show_start != old_menu_show_start)
        update_screen_locked();

    pthread_mutex_unlock(&gUpdateMutex);
}

/*
- Replace draw_screen_locked(): effectively draw everything on screen
- Menu items are populated in ui_start_menu()
- Log/ui_print items are populated in ui_print()
- Do not call any LOGE or ui_print here, or it will trigger another call to pthread_mutex_lock(&gUpdateMutex)
  while it is locked here as called by update_screen_locked() after a lock to gUpdateMutex
*/
void draw_touch_menu() {
    if (show_text) {
        // don't "disable" the background any more with this...
        // gr_color(0, 0, 0, 160);
        // gr_fill(0, 0, gr_fb_width(), gr_fb_height());

        int i = 0;
        int j = 0;
        int row = 0;            // current row that we are drawing on

        draw_battery_clock();

        if (show_menu) {
            if (menu_sel >= menu_show_start && menu_sel < menu_show_start + max_menu_rows - menu_top) {
                // * start highlighted menu write, but only if it is in the range of showing menus
                //   else, we are probably scrolling and selected menu is outside screen
                // * fill highlighted menu with highlight color (area to fill) = (x, y, width, hight) in pixels
                // * width and height are not absolute pixel width/height but an x(width) + y(height) coordinate to join
                // * y set to +1 to start one line (pixel) lower and h minus 1 to fill upto one line higher
                // * we do the same at below Background fill of unselected menu
                //   to let some of the grey background show between menus
                // * x zero line is the left device board, y zero line is the top device border
                gr_color(MENU_HIGHLIGHT_COLOR);
                gr_fill(0, ((menu_top + menu_sel - menu_show_start) * MENU_HEIGHT_TOTAL) + 1,
                        gr_fb_width(), (menu_top + menu_sel - menu_show_start + 1) * MENU_HEIGHT_TOTAL);

                // draw top separator line for the highlighted menu
                if (show_menu_separation.value) {
                    gr_color(MENU_SEPARATOR_COLOR);
                    gr_fill(0, (menu_top + menu_sel - menu_show_start) * MENU_HEIGHT_TOTAL,
                            gr_fb_width(), ((menu_top + menu_sel - menu_show_start) * MENU_HEIGHT_TOTAL) + 1);
                }
            }

            // start header text write
            gr_color(HEADER_TEXT_COLOR);
            for(i = 0; i < menu_top; ++i) {
                draw_text_line(i, menu[i], MENU_HEIGHT_TOTAL, LEFT_ALIGN);
                row++;
            }

            if (menu_items - menu_show_start + menu_top >= max_menu_rows)
                j = max_menu_rows - menu_top;
            else
                j = menu_items - menu_show_start;

            // start menu text write and non highlighted menu background color fill
            for(i = menu_show_start + menu_top; i < (menu_show_start + menu_top + j); ++i) {
                if (i == menu_top + menu_sel && menu_sel >= menu_show_start && menu_sel < menu_show_start + max_menu_rows - menu_top) {
                    // write text color of selected menu (white in this case) and only if selected menu is within showing menus
                    // else, we are probably scrolling and selected menu is outside screen
                    gr_color(255, 255, 255, 255); // highlighted text always white
                    draw_text_line(i - menu_show_start , menu[i], MENU_HEIGHT_TOTAL, LEFT_ALIGN);
                } else {
                    // add dark transparent background for unselected menu
                    // background must be filled before write menu text or it will tint menu text color
                    gr_color(MENU_BACKGROUND_COLOR);
                    gr_fill(0, ((i - menu_show_start) * MENU_HEIGHT_TOTAL) + 1,
                            gr_fb_width(), (i - menu_show_start + 1) * MENU_HEIGHT_TOTAL);
                    
                    // draw a line separation between menus for situations where
                    // we have same background and menu_background color
                    if (show_menu_separation.value) {
                        gr_color(MENU_SEPARATOR_COLOR);
                        gr_fill(0, ((i - menu_show_start) * MENU_HEIGHT_TOTAL),
                                gr_fb_width(), ((i - menu_show_start) * MENU_HEIGHT_TOTAL) + 1);
                    }

                    // write text of unselected menu using defined MENU_TEXT_COLOR
                    gr_color(MENU_TEXT_COLOR);
                    draw_text_line(i - menu_show_start, menu[i], MENU_HEIGHT_TOTAL, LEFT_ALIGN);
                }
                row++;
                if (row >= max_menu_rows)
                    break;
            }

            // start write bottom line separator, the line just below last menu
            // the line above virtual buttons (pre-KK virtual keys), is part of their png image
            gr_color(MENU_SEPARATOR_COLOR);
            gr_fill(0, (row * MENU_HEIGHT_TOTAL) + (MENU_HEIGHT_TOTAL / 2) - 1,
                    gr_fb_width(), (row * MENU_HEIGHT_TOTAL) + (MENU_HEIGHT_TOTAL / 2) + 1);
                    // menu bottom separator line x, y, width and height in pixels
            row++;
        }

        // start refresh bottom log text (ui_print)
        // fix ui_prints overlapping progress bar. Deduct the rows below progress bar
        // y coordinate of progress bar is defined in draw_progress_locked() by int dy
        int available_rows;
        int cur_row;
        int start_row;
        cur_row = text_row;
        start_row = ((row * MENU_HEIGHT_TOTAL) / FONT_HEIGHT);

        if (gProgressBarType != PROGRESSBAR_TYPE_NONE) {
            // do not overlap bottom progress bar in install zip mode and backup/restore jobs
            // in sideload mode, menu is shown along install zip progress
            // in that case, start_row is > 0, so let's deduct it too
            available_rows = ((3*gr_fb_height() + gr_get_height(gBackgroundIcon[BACKGROUND_ICON_INSTALLING]) - 
                                2*gr_get_height(gProgressBarEmpty))/4) / FONT_HEIGHT;
            available_rows -= start_row;
        } else {
            available_rows = (gr_fb_height() - (row * MENU_HEIGHT_TOTAL) - virtual_keys_h) / FONT_HEIGHT;
        }

        if (available_rows < MAX_ROWS)
            cur_row = (cur_row + (MAX_ROWS - available_rows)) % MAX_ROWS;
        else
            start_row = (available_rows + ((row * MENU_HEIGHT_TOTAL) / FONT_HEIGHT)) - MAX_ROWS;

        int r;
        for(r = 0; r < (available_rows < MAX_ROWS ? available_rows : MAX_ROWS); r++) {
            if ((start_row + r) <= 1) {
                // ensure ui_prints do not overlap battery/clock header
                // cut text for the line 0 where clock is displayed and line 1 below it
                int col_offset = 1;
                if (show_clock.value) col_offset += 6;
                if (show_battery.value) col_offset += 6;
                if (text_cols - col_offset < 0) col_offset = 0; // who knows !
                text[(cur_row + r) % MAX_ROWS][text_cols - col_offset] = '\0';
            }

            // support print in color for last colored_bottom_rows num rows
            if (r >= (available_rows < MAX_ROWS ?
                      available_rows - colored_bottom_rows : MAX_ROWS - colored_bottom_rows))
                gr_color(uiprint_color_code[0], uiprint_color_code[1], uiprint_color_code[2], uiprint_color_code[3]);
            else gr_color(NORMAL_TEXT_COLOR);

            draw_text_line(start_row + r, text[(cur_row + r) % MAX_ROWS], FONT_HEIGHT, LEFT_ALIGN);
        }
    }

    draw_virtualkeys_locked(); //added to draw the virtual keys
}

// do not let header text overlap on battery and clock top display
int ui_menu_header_offset() {
    int offset = 1;
    if (show_clock.value) offset += 5;
    if (show_battery.value) offset += 6;
    if (text_cols - offset < 0) offset = 1; // who knows !

    return offset;
}

// Formats toggle menus to screen width
// No need for buffer passed in function.
// But, call with item_menu[MENU_MAX_COLS] (sizeof(item_menu) >= MENU_MAX_COLS)
// Do not call before ui_init() as text_cols == 0
void ui_format_touch_menu(char *item_menu, const char* menu_text, const char* menu_option) {
    char val_txt[MENU_MAX_COLS];
    char val_opt[MENU_MAX_COLS];
    strcpy(val_txt, menu_text);
    strcpy(val_opt, menu_option);

    // text_cols: total really available columns to ui_print, based on device screen width
    // however, for tablets for example or very high res devices, it can be superior to
    // max allowed menu chars (MENU_MAX_COLS)
    int max_menu_len = text_cols;
    if (max_menu_len > MENU_MAX_COLS - 1)
        max_menu_len = MENU_MAX_COLS - 1;

    int i;
    // +1 to add one space for separation
    int len = MENU_ITEM_HEADER_LENGTH + strlen(val_txt) + 1 + strlen(val_opt);
    if (len > max_menu_len) {
        // Menu too long to fit width. First cut menu_text until it fits but keep at least 3 chars
        i = strlen(val_txt);
        while (len > max_menu_len && i > 3) {
            val_txt[i-1] = 0;
            i = strlen(val_txt);
            len = MENU_ITEM_HEADER_LENGTH + i + 1 + strlen(val_opt);
        }

        // if still too long, cut menu_option to fit width and keep at least 3 chars:
        // we should never need to cut that much and even reach here
        i = strlen(val_opt);
        while (len > max_menu_len && i > 3) {
            val_opt[i-1] = 0;
            i = strlen(val_opt);
            len = MENU_ITEM_HEADER_LENGTH + strlen(val_txt) + 1 + i;
        }
    }

    // now, separate menu_text and menu_option with spaces to rigth align menu_option
    // remove the space we added before to account for one white space separation
    // len = MENU_ITEM_HEADER_LENGTH + strlen(val_txt) + 0 + strlen(val_opt);
    len = len - 1;
    while (len < max_menu_len) {
        strcat(val_txt, " ");
        len = MENU_ITEM_HEADER_LENGTH + strlen(val_txt) + strlen(val_opt);
    }

    strcpy(item_menu, val_txt);
    strcat(item_menu, val_opt);
}

// call to quick check key queue and return pressed key
// avoids the freeze and wait time until key pressed compared to original ui_wait_key()
static int ui_check_key() {
    pthread_mutex_lock(&key_queue_mutex);
    int key = -1;
    if (key_queue_len > 0) {
        key = key_queue[0];
        memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
    }
    pthread_mutex_unlock(&key_queue_mutex);
    return key;
}

// key press detection function, used to cancel nandroid jobs
int key_press_event() {
    int key = ui_check_key();
    int action = ui_handle_key(key, 1);
    return action;
}

// handle key codes and translates them to an action returned to get_menu_selection() in recovery.c
int touch_handle_key(int key_code, int visible) {
    if (visible) {
        switch (key_code) {
            case KEY_PAGEUP:
                if (touch_sel)
                    return HIGHLIGHT_ON_TOUCH;
                break;

            case KEY_ESC: // force ui_wait_key() to return when touching screen on some devices
                break;

            case KEY_PAGEDOWN:
                if (now_scrolling != 0)
                    scroll_touch_menu();
                break;

            case KEY_LEFTBRACE:
                // allow touch gesture actions.
                // on touch gestures, we reach here only if key_gesture was 0, that is the previosu gesture action is terminated
                // calling gesture actions is not allowed when one is already ongoing and handle_gesture_actions() did not return (key_gesture != 0)
                if (key_gesture != 0)
                    return GESTURE_ACTIONS;
                break;

            case KEY_F12: // HTC One X
                return SELECT_ITEM;

            // keep GO_BACK menu here
            case KEY_END:
            case KEY_BACKSPACE:
            case KEY_SEARCH:
            case KEY_BACK:
                if (!ui_root_menu || vbutton_pressed != -1) {
                    // avoid virtual back button remains highlighted when touched in root menu
                    return GO_BACK;
                }
                break;

            default:
                return device_handle_key(key_code, visible);
        }
    }

    return NO_ACTION;
}

/*
Applies log visibility settings (pause on logs only for now)
- engage_friendly_view is enabled by calling ui_set_background()
- engage_friendly_view is disabled when calling ui_start_menu()
- if wait_after_install.value toggle is enabled, we are prompted for a key after install zip package

- wait_after_install.value: set by user
    * enabled == 1, will wait before returning to menus gui
    * on multi-zip flash, install files on boot (edify + openrecoveryscript)
      and run aromafm.zip, it will be ignored and we never wait (force_wait = -1)
    * on show_log_menu() call (display logs from advanced menu), it will be ignored and we always wait
    * at the end, enabling it only affects install single zip files from menu
- force_wait (override wait_after_install.value user choice)
    * disabled == 0 (default when in recovery), --> we rely on wait_after_install.value set by user
    * enabled == 1, --> always wait, only used in show_log_menu()
    * never wait == -1 (default on boot time), will never wait for a key and ignores wait_after_install.value user setting:
      install scripts on boot (called by refresh_recovery_settings()) + 
      multi_flash zip + run aroma + nandroid operations + toggle background icon
    * immediately on booting recovery, force_wait is set to -1 to avoid wait on boot scripts
    * once we display first menu, it is back set to default 0 by ui_start_menu()
      no need to disable it in call function that sets it to -1
- is_friendly_view check avoids multiple calls to fast_ui_int() by ui_set_background()
  this can happen when it is called many times inside function, like with install_zip()
*/
void ui_friendly_log(int engage_friendly_view)
{
    static int is_friendly_view = 0;
    if (engage_friendly_view && is_friendly_view) {
        // blocks multiple calls to ui_set_background() inside function, before returning to ui_start_menu()
        return;
    } else if (!engage_friendly_view && !is_friendly_view) {
        // every time we navigate we are in this situation
        // if by case we set them to non 0 and ui_set_background() is not called after that!
        force_wait = 0;
    } else if (engage_friendly_view && !show_menu) {
        // is_friendly_view is always 0 here
        // we called ui_set_background() ---> engage friendly view
        // we are in full screen mode, no menus, just logging
        // show_menu check: apply_from_adb() calls ui_set_background() even before installing the zip (to show stat error) + it does install zip while menu is shown
        is_friendly_view = 1;
    } else if (!engage_friendly_view) {
        // is_friendly_view is always 1 here: we are back to ui_start_menu()().
        // Eventually wait before exit friendly view and show menu gui
        if (force_wait != -1) {
            // force_wait == -1 when we want to not wait on logs and override user choice
            if (wait_after_install.value || force_wait) {
                // if force_wait == 0, we rely on user choice
                // if force_wait == 1, we want to force a wait overriding user choice
                ignore_key_action = 1;
                ui_clear_key_queue();
                ui_print("press any key to continue.\n");
                ui_wait_key();
            }
        }
        force_wait = 0;
        is_friendly_view = 0;
    }
}

void ui_blank_screen(bool blank_screen) {
    if (blank_screen) {
        ui_dim_screen(true);
        ignore_key_action = 1;
    }

    pthread_mutex_lock(&gUpdateMutex);
    gr_fb_blank(blank_screen);
    is_blanked = blank_screen;

    // wake up screen
    if (!blank_screen) {
        ui_dim_screen(false);
        update_screen_locked();

        // some phones (i9500) need special scripts to restore touch after screen wake up
        if (strlen(libtouch_flags.board_post_unblank_command) != 0)
            __system(libtouch_flags.board_post_unblank_command);
    }

    pthread_mutex_unlock(&gUpdateMutex);
}

// set brightness to 0 or default value
// used for timed out dim screen
// when manually dimming screen, we keep 10 as minimal value to avoid locking user
void ui_dim_screen(bool dim_screen) {
    if (dim_screen)
        apply_brightness_value(0);
    else
        apply_brightness_value(set_brightness.value);

    is_dimmed = dim_screen;
}

// support refresh batt / clock, dim and blank screen after a set delay and if no key is pressed
// screen_timeout increment must be same as timeout.tv_sec += increase value or we get out of time sync
void ui_refresh_display_state(int *screen_timeout) {
    if (key_queue_len == 0) {
        // no key was pressed
        // dim screen if timer reached
        *screen_timeout += REFRESH_TIME_USB_INTERVAL;
        if (!is_dimmed && dim_timeout.value != 0 && *screen_timeout >= dim_timeout.value)
            ui_dim_screen(true);

        // blank screen if timer reached
        if (!is_blanked && blank_timeout.value != 0 && *screen_timeout >= blank_timeout.value)
            ui_blank_screen(true); // this will also set is_blanked to 1

        // refresh clock and battery display if screen is not blanked
        // if text is not visible, no need to refresh clock/time (password prompt start up screen)
        if (!is_blanked && ui_IsTextVisible()) {
            pthread_mutex_lock(&gUpdateMutex);
            update_screen_locked();
            pthread_mutex_unlock(&gUpdateMutex);
        }
    } else {
        // key_queue_len > 0: a key was pressed
        // wake up screen if it was blanked or dimmed
        // unblank screen will also reset brightness if dimmed
        if (is_blanked)
            ui_blank_screen(false);
        else if (is_dimmed)
            ui_dim_screen(false);
    }
}
/******** end philz functions declarartion ********/


/*******************************/
/*  Start touch handling code  */
/*   Original port by PhilZ    */
/*******************************/
#ifndef SYN_REPORT
#define SYN_REPORT          0x00
#endif
#ifndef SYN_CONFIG
#define SYN_CONFIG          0x01
#endif
#ifndef SYN_MT_REPORT
#define SYN_MT_REPORT       0x02
#endif

#define ABS_MT_POSITION     0x2a /* Group a set of X and Y */
#define ABS_MT_AMPLITUDE    0x2b /* Group a set of Z and W */
#define ABS_MT_SLOT         0x2f
#define ABS_MT_TOUCH_MAJOR  0x30
#define ABS_MT_TOUCH_MINOR  0x31
#define ABS_MT_WIDTH_MAJOR  0x32
#define ABS_MT_WIDTH_MINOR  0x33
#define ABS_MT_ORIENTATION  0x34
#define ABS_MT_POSITION_X   0x35
#define ABS_MT_POSITION_Y   0x36
#define ABS_MT_TOOL_TYPE    0x37
#define ABS_MT_BLOB_ID      0x38
#define ABS_MT_TRACKING_ID  0x39
#define ABS_MT_PRESSURE     0x3a
#define ABS_MT_DISTANCE     0x3b

static int device_has_vk = 0; // we found device has built in virtual keys that need touch support
static int touch_is_init = 0;
static int vk_count = 0; // number of device virtual keys

struct virtualkey {
    int scancode;
    int centerx, centery;
    int width, height;
};

struct virtualkey *vks;

// board display ranges returned by calling ioctl on each axis
static int board_min_x = 0;
static int board_max_x = 0;
static int board_min_y = 0;
static int board_max_y = 0;

// Returns empty tokens
// parse the device virtual keys file
static char *vk_strtok_r(char *str, const char *delim, char **save_str) {
    if(!str) {
        if(!*save_str)
            return NULL;
        str = (*save_str) + 1;
    }
    *save_str = strpbrk(str, delim);

    if (*save_str)
        **save_str = '\0';

    return str;
}

// handle device virtual keys
// currently this will assume we have only one touch device
// would need a fix to handle more than one touch device as per the ev_init(...) code
// MAX_DEVICES and MAX_MISC_FDS must be same as in events.c
// abs_devices struct saves the fd of each EV_ABS event call (/dev/input files)
// that way, we can blacklist some devices based on their unique fd (input event files remain open along recovery session
#define MAX_DEVICES 16
#define MAX_MISC_FDS 16
struct abs_devices {
    int fd;
    char deviceName[64];
    int ignored;
};

static struct abs_devices abs_device[MAX_DEVICES + MAX_MISC_FDS];
static unsigned abs_count = 0;

// called only once to initialize device virtual keys on the retained valid abs_device
static int vk_init(struct abs_devices e) {
    char vk_str[2048];
    char *ts = NULL;
    ssize_t len;
    int vk_fd;
    char vk_path[PATH_MAX] = "/sys/board_properties/virtualkeys.";
    strcat(vk_path, e.deviceName);
#ifdef RECOVERY_TOUCH_DEBUG
    LOGI("\n>> Checking board vk: %s\n", vk_path);
#endif

    // Some devices split the keys from the touchscreen: ignore if we cannot open file
    // vk_fd = open("/sdcard/virtualkeys.synaptics-rmi-touchscreen", O_RDONLY); // test debug
    vk_fd = open(vk_path, O_RDONLY);
    if (vk_fd < 0)
        return -1;

    len = read(vk_fd, vk_str, sizeof(vk_str)-1);
    close(vk_fd);
    if (len <= 0) {
        LOGI("error reading vk path\n");
        return -1;
    }

    vk_str[len] = '\0';
#ifdef RECOVERY_TOUCH_DEBUG
    LOGI("found: %s\n", vk_str);
#endif
    /*
       Parse device virtual keys
       Parse a line like: keytype:keycode:centerx:centery:width:height:keytype2:keycode2:centerx2:...
    */
    for(ts = vk_str, vk_count = 1; *ts; ++ts) {
        if (*ts == ':')
            ++vk_count;
    }

    if (vk_count % 6) {
        LOGW("minui: %s is %d %% 6\n", vk_path, vk_count % 6);
    }

    vk_count /= 6;
    if (vk_count <= 0) {
        LOGI("non valid format for %s\n", vk_path);
        return -1;
    }

    vks = malloc(sizeof(*vks) * vk_count);
    int i;
    for(i = 0; i < vk_count; ++i) {
        char *token[6];
        int j;

        for(j = 0; j < 6; ++j) {
            token[j] = vk_strtok_r((i||j)?NULL:vk_str, ":", &ts);
        }

        if (strcmp(token[0], "0x01") != 0) {
            /* Java does string compare, so we do too. */
            LOGW("minui: %s: ignoring unknown virtual key type %s\n", vk_path, token[0]);
            continue;
        }

        vks[i].scancode = strtol(token[1], NULL, 0);
        vks[i].centerx = strtol(token[2], NULL, 0);
        vks[i].centery = strtol(token[3], NULL, 0);
        vks[i].width = strtol(token[4], NULL, 0);
        vks[i].height = strtol(token[5], NULL, 0);
#ifdef RECOVERY_TOUCH_DEBUG
        LOGI("vks[%d]:\n", i);
        LOGI("      scancode=%d\n", vks[i].scancode);
        LOGI("      centerx=%d\n", vks[i].centerx);
        LOGI("      centery=%d\n", vks[i].centery);
        LOGI("      width=%d\n", vks[i].width);
        LOGI("      height=%d\n", vks[i].height);
#endif
    }

    return 0;
}

/*
Called on first EV_ABS event to initialize touch device and calibrate it
It will be called on each next EV_ABS event until a valid touch device is accepted and initialized
Here is where we blacklist unwanted EV_ABS devices based on their unique file descriptor (fd)
Whenever we blacklist a device, we increment abs_count. At the end, abs_device[abs_count] will be the only valid touch device we will use
Before initiating a device, we check its fd to see if it was already blacklisted
Once a device passes blacklist, touch_device_init() returns 0 and sets touch_is_init to 1
With touch_is_init == 1, the touch_device_init() is no longer called
This way, we initialize the retained touch device only once and no longer call this function afterwards
The function will calibrate touch screen interpolation returning display board min/max ranges
It will also call vk_init() to calibrate device virtual keys for the retained touch device
*/
static int abs_mt_pos_horizontal = ABS_MT_POSITION_X;
static int abs_mt_pos_vertical = ABS_MT_POSITION_Y;
static int touch_device_init(int fd) {
    unsigned int i;
    for(i=0; i <= abs_count; i++) {
        if (fd == abs_device[i].fd && abs_device[i].ignored == 1)
            return -1;
    }

    abs_device[abs_count].fd = fd;

    ssize_t len;
    len = ioctl(fd,EVIOCGNAME(sizeof(abs_device[abs_count].deviceName)),abs_device[abs_count].deviceName);
    if (len <= 0) {
        LOGE("Unable to query event object.\n");
        abs_device[abs_count].ignored = 1;
        abs_count++;
        return -1;
    }

#ifdef RECOVERY_TOUCH_DEBUG
    LOGI("input deviceName=%s\n", abs_device[abs_count].deviceName);
#endif

    // Blacklist these EV_ABS "input" devices (accelerometers)
    if (strcmp(abs_device[abs_count].deviceName, "bma250") == 0 ||
            strcmp(abs_device[abs_count].deviceName, "bma150") == 0 ||
            strcmp(abs_device[abs_count].deviceName, "lsm303dlhc_acc_lt") == 0) {
        abs_device[abs_count].ignored = 1;
        abs_count++;
        return -1;
    }

    // calculate max axis values, credits to gweedo767
    struct input_absinfo absinfo_x;
    struct input_absinfo absinfo_y;

    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &absinfo_x);
    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &absinfo_y);
    if (absinfo_x.maximum <= 0 || absinfo_y.maximum <= 0) {
        // blacklist invalid device (accelerometer?)
        abs_device[abs_count].ignored = 1;
        abs_count++;
        return -1;
    }

    // we got a valid touch device
    touch_is_init = 1;

    // set min/max axis values
    board_min_x = absinfo_x.minimum;
    board_max_x = absinfo_x.maximum;
    board_min_y = absinfo_y.minimum;
    board_max_y = absinfo_y.maximum;

    if (libtouch_flags.recovery_touchscreen_swap_xy) {
        // swap x/y max/min values
        int old_val;
        old_val = board_min_x;
        board_min_x = board_min_y;
        board_min_y = old_val;
        old_val = board_max_x;
        board_max_x = board_max_y;
        board_max_y = old_val;

        // swap the axis
        abs_mt_pos_horizontal = ABS_MT_POSITION_Y;
        abs_mt_pos_vertical = ABS_MT_POSITION_X;
    }

#ifdef RECOVERY_TOUCH_DEBUG
    LOGI("min bounds: %d x %d\n", board_min_x, board_min_y);
    LOGI("max bounds: %d x %d\n", board_max_x, board_max_y);
#endif

    if (vk_init(abs_device[abs_count]) == 0)
        device_has_vk = 1;

    return 0;
}

// announces given key as pressed or released
// used to handle key-repeat for virtual buttons
static void toggle_key_pressed(int key_code, int pressed) {
    pthread_mutex_lock(&key_queue_mutex);
    key_pressed[key_code] = pressed;
    pthread_mutex_unlock(&key_queue_mutex);
}

// called to map a touch event to a recovery virtual button
// it returns the touched key code and highlights the touched virtual button
static int input_buttons() {
    pthread_mutex_lock(&gUpdateMutex);

    int final_code = -1;
    int start_draw = 0;
    int end_draw = 0;

    gr_surface surface = gVirtualKeys;
    int fbh = gr_fb_height();
    int fbw = gr_fb_width();
    int vk_width = gr_get_width(surface);
    int keyhight = gr_get_height(surface);
    int keywidth = vk_width / 4;
    int keyoffset = (fbw - vk_width) / 2;  // pixels from left display edge to start of virtual keys

    if (touch_x < (keywidth + keyoffset + 1)) {
        // down button
        final_code = KEY_DOWN; // 108
        start_draw = keyoffset;
        end_draw = keywidth + keyoffset;
    } else if (touch_x < ((keywidth * 2) + keyoffset + 1)) {
        // up button
        final_code = KEY_UP; // 103
        start_draw = keyoffset + keywidth + 1;
        end_draw = (keywidth * 2) + keyoffset;
    } else if (touch_x < ((keywidth * 3) + keyoffset + 1)) {
        // back button
        final_code = KEY_BACK; // 158
        start_draw = keyoffset + (keywidth * 2) + 1;
        end_draw = (keywidth * 3) + keyoffset;
    } else if (touch_x < ((keywidth * 4) + keyoffset + 1)) {
        // enter key
        final_code = KEY_ENTER; // 28
        start_draw = keyoffset + (keywidth * 3) + 1;
        end_draw = (keywidth * 4) + keyoffset;
    } else {
        return final_code;
    }

    // start drawing button highlight
    // clear old touch points
    gr_color(0, 0, 0, 255); // black
    gr_fill(0, fbh-keyhight, 
            vk_width+keyoffset, fbh-keyhight+3);

    gr_color(VK_KEY_HIGHLIGHT_COLOR);
    gr_fill(start_draw, fbh-keyhight,
            end_draw, fbh-keyhight+3);
    gr_flip(); // makes visible the draw buffer we did above, without redrawing whole screen
    pthread_mutex_unlock(&gUpdateMutex);

#ifdef RECOVERY_TOUCH_DEBUG
    LOGI("Virtual Button Press:\n");
    LOGI("      Key code: %d:\n", final_code);
#endif

    return final_code;
}

// handle device virtual keys
// will return the key code from touched virtual key
int input_vk() {
    int i = 0;
    while (i < vk_count) {
        if (touch_x > (vks[i].centerx - (vks[i].width / 2)) && 
                touch_x < (vks[i].centerx + (vks[i].width / 2))) {
#ifdef RECOVERY_TOUCH_DEBUG
            LOGI("Returned vks[%d] for touch_x=%d:\n", i, touch_x);
            LOGI("      scancode=%d\n", vks[i].scancode);
            LOGI("      centerx=%d\n", vks[i].centerx);
            LOGI("      centery=%d\n", vks[i].centery);
            LOGI("      width=%d\n", vks[i].width);
            LOGI("      height=%d\n", vks[i].height);
#endif
            return vks[i].scancode;
        }
        i++;
    }
    return -1;
}

/*
Track finger up/down: ported from vk_modify() in twrp and old cwm sources
It is handling either ev.type == EV_ABS || ev.type == EV_SYN
Return codes:
    * -1 completely drop events from unsupported non touch devices like bm250 and b150 gyroscope devices sending ABS_X and ABS_Y
    *  1 we are in a touch
         code will either trigger first touch event time or track x/y coordinates
    *  0 informs it is a finger lifted event
*/
static int current_slot = 0;
static int lastWasSynReport = 0;
static int touchReleaseOnNextSynReport = 0;
static int use_tracking_id_negative_as_touch_release = 0;

static int touch_track(int fd, struct input_event ev) {
    // - This is used to ditch useless event handlers, like an accelerometer
    // - Ideally, we should do like the minuitwrp/events.c/evs[ev_count].fd = &ev_fds[ev_count]; entry
    //   and try to get settings in touch_device_init() and vk_init() for each evs[ev_count] device
    // - In our case, we assume fd is the same as ev_init() that opens /dev/input files is called only once
    // /dev/input opened files are not closed later in recovery so they should keep their fd identical
    // during initialization by touch_device_init(), abs_count was incremented after each device we blacklist
    // at this point, abs_count corresponds to the initialized device in touch_device_init()
    if (fd != abs_device[abs_count].fd)
        return -1;

    // On most Samsung devices, type: 3  code: 39  value: -1, aka EV_ABS ABS_MT_TRACKING_ID -1 indicates a true touch release
    // on first touch event, we must enable the use ABS_MT_TRACKING_ID value for finger up
    if (libtouch_flags.board_use_b_slot_protocol && !use_tracking_id_negative_as_touch_release)
        use_tracking_id_negative_as_touch_release = 1;

    // start tracking finger up/down
    int finger_up = 0;
    if (ev.type == EV_ABS) {
        /*
        This is a touch event
        Currently we only handle ABS_MT_POSITION_X and ABS_MT_POSITION_Y coordinates
            - to do: support ABS_X and ABS_Y if needed
        We also only support one touch device: all EV_ABS events are treated by same code and we assume same settings set in
        touch_device_init() and vk_init()
            - to do: if needed, properly set different calibration settings for additional
              touch devices (use events.c call rather than loop for fd check like above)
        */

        // lock on first slot and ditch all other slots events
        if (ev.code == ABS_MT_SLOT) { //47
            current_slot = ev.value;
#ifdef RECOVERY_TOUCH_DEBUG
            LOGI("EV: => EV_ABS ABS_MT_SLOT %d\n", ev.value);
#endif
            return 1;
        }
        if (current_slot != 0)
            return 1;

        switch (ev.code)
        {
            case ABS_X: //00
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS  ABS_X %d\n", ev.value);
#endif
                break;

            case ABS_Y: //01
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS  ABS_Y %d\n", ev.value);
#endif
                break;

            case ABS_MT_POSITION: //2a
                if (ev.value == (1 << 31))
                    lastWasSynReport = 1;
                else
                    lastWasSynReport = 0;
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS  ABS_MT_POSITION %d\n", lastWasSynReport);
#endif
                break;

            case ABS_MT_TOUCH_MAJOR: //30
                if (ev.value == 0) {
                    // We're in a touch release, although some devices will still send positions as well
                    touchReleaseOnNextSynReport = 1;
                }
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS  ABS_MT_TOUCH_MAJOR %d\n", ev.value);
#endif
                break;

            case ABS_MT_PRESSURE: //3a
                if (ev.value == 0) {
                    // We're in a touch release, although some devices will still send positions as well
                    touchReleaseOnNextSynReport = 1;
                }
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS  ABS_MT_PRESSURE %d\n", ev.value);
#endif
                break;

            case ABS_MT_POSITION_X: //35
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS  ABS_MT_POSITION_X %d\n", ev.value);
#endif
                break;

            case ABS_MT_POSITION_Y: //36
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS  ABS_MT_POSITION_Y %d\n", ev.value);
#endif
                break;

            case ABS_MT_TOUCH_MINOR: //31
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS ABS_MT_TOUCH_MINOR %d\n", ev.value);
#endif
                break;

            case ABS_MT_WIDTH_MAJOR: //32
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS ABS_MT_WIDTH_MAJOR %d\n", ev.value);
#endif
                break;

            case ABS_MT_WIDTH_MINOR: //33
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS ABS_MT_WIDTH_MINOR %d\n", ev.value);
#endif
                break;

            case ABS_MT_TRACKING_ID: //39
                if (ev.value < 0) {
                    touchReleaseOnNextSynReport = 2;
                    // if, by default, we did not enable use of ABS_MT_TRACKING_ID through BOARD_USE_B_SLOT_PROTOCOL
                    // let's enable it now. However it will make very first touch unpredictable
                    if (!use_tracking_id_negative_as_touch_release)
                        use_tracking_id_negative_as_touch_release = 1;
                }
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("EV: => EV_ABS ABS_MT_TRACKING_ID %d\n", ev.value);
#endif
                break;

#ifdef RECOVERY_TOUCH_DEBUG
            // All of these items are strictly for logging purposes only.
            // Return 1 because they don't need to be handled but they can inform we started touching screen (set in_touch to 1)
            case ABS_MT_ORIENTATION: //34
                LOGI("EV: => EV_ABS ABS_MT_ORIENTATION %d\n", ev.value);
                return 1;
                break;

            case ABS_MT_TOOL_TYPE: //37
                LOGI("EV: => EV_ABS ABS_MT_TOOL_TYPE %d\n", ev.value);
                return 1;
                break;

            case ABS_MT_BLOB_ID: //38
                LOGI("EV: => EV_ABS ABS_MT_BLOB_ID %d\n", ev.value);
                return 1;
                break;

            case ABS_MT_DISTANCE: //3b
                LOGI("EV: => EV_ABS ABS_MT_DISTANCE %d\n", ev.value);
                return 1;
                break;
#endif
            default:
                // This is an unhandled message, just skip it
                return 1;
        }

        if (ev.code != ABS_MT_POSITION) {
            lastWasSynReport = 0;
            return 1;
        }
    }

    // Check if we should ignore the message (but still treat EV_ABS events to translate coordinates: return 1)
    if (ev.code != ABS_MT_POSITION && (ev.type != EV_SYN || (ev.code != SYN_REPORT && ev.code != SYN_MT_REPORT))) {
        lastWasSynReport = 0;
        return 1;
    }

#ifdef RECOVERY_TOUCH_DEBUG
    if (ev.type == EV_SYN && ev.code == SYN_REPORT)
        LOGI("EV: => EV_SYN  SYN_REPORT\n");
    if (ev.type == EV_SYN && ev.code == SYN_MT_REPORT)
        LOGI("EV: => EV_SYN  SYN_MT_REPORT\n");
#endif

    // Discard the MT versions
    if (ev.code == SYN_MT_REPORT)
        return 1;

    // check if we are finger-up state
    if (!use_tracking_id_negative_as_touch_release) {
        if (lastWasSynReport == 1 || touchReleaseOnNextSynReport == 1)
            finger_up = 1;
    } else if (touchReleaseOnNextSynReport == 2) // ABS_MT_TRACKING_ID with ev.value = -1;
        finger_up = 1;

    if (finger_up) {
        // we are finger-up state, reset the value and return 0
        touchReleaseOnNextSynReport = 0;
#ifdef RECOVERY_TOUCH_DEBUG
        LOGI("lastWasSynReport=%d, touchReleaseOnNextSynReport=%d, use_tracking_id_negative_as_touch_release=%d\n",
                lastWasSynReport, touchReleaseOnNextSynReport, use_tracking_id_negative_as_touch_release);
#endif
         return 0;
    }

    lastWasSynReport = 1;
    return 1;
}

// handle device keys here instead of in input_callback()
// this way, we can modify ev and use modified values
// if we rely on input_callback() like previously, we must pass the ev struct as a pointer
// else, modifications done in touch_handle_input() are not seen by input_callback()
static int key_handle_input (struct input_event ev) {
    if (ev.type != EV_KEY || ev.code > KEY_MAX)
        return 0;

    pthread_mutex_lock(&key_queue_mutex);

    key_pressed[ev.code] = ev.value;

    const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
    if (ev.value > 0 && key_queue_len < queue_max) {
        key_queue[key_queue_len] = ev.code;
        ++key_queue_len;

        if (boardEnableKeyRepeat.value) {
            struct timeval now;
            gettimeofday(&now, NULL);

            key_press_time[ev.code] = (now.tv_sec * 1000) + (now.tv_usec / 1000);
            key_last_repeat[ev.code] = 0;
        }

        pthread_cond_signal(&key_queue_cond);
    }
    pthread_mutex_unlock(&key_queue_mutex);

    if (ev.value > 0 && device_reboot_now(key_pressed, ev.code)) {
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    }

    return 0;
}

// this will handle all input events
// keyboard events (EV_KEY) are sent to be processed by key_handle_input()
// mouse up/down events (EV_REL) are returned to input_callback()
// touch events are handled and mapped to a key if possible
// return 0 to handle a non touch event by input_callback()
// return 1 if it is a handled touch event so that input_callback() returns 0 without adding them again to the queue
int touch_handle_input(int fd, struct input_event ev) {
    int ret;
    int fbh = gr_fb_height();
    int fbw = gr_fb_width();

#ifdef RECOVERY_TOUCH_DEBUG
    LOGI("\n>> Start input event handling:\n");
    LOGI("   - ignore action=%d\n", ignore_key_action);
    LOGI("   - EV: type: %x  code: %x  value: %d\n", ev.type, ev.code, ev.value);

    switch (ev.type)
    {
        case EV_SYN: // 0
            LOGI("   - EV_SYN (%d)\n", EV_SYN);
            break;
        case EV_REL: // 2
            LOGI("   - EV_REL (%d)\n", EV_REL);
            break;
        case EV_ABS: // 3
            LOGI("   - EV_ABS (%d)\n", EV_ABS);
            break;
        case EV_KEY: // 1
            LOGI("   - EV_KEY (%d)\n", EV_KEY);
            break;
        default:
            LOGI("   - Undefined Event Type\n");
            break;
    }
#endif

    // each hardware key event, will trigger EV_KEY two times
    // on press: EV_KEY with ev.value 1 for hardware keys
    // when releasing hardware key, we get EV_KEY with ev.value = 0
    // EV_KEY will not be queued when ev.value is set to 0
    // On some devices, this happens also on touch events (i9505): first touch causes a EV_KEY with value 1 and lift finger causes same with ev.value = 0
    // the EV_KEY seems to happen as last event on finger lifted, after all other events include EV_ABS
    // however, order is not guaranteed maybe?
    // resetting ignore_key_action here ensures that after a hardware key, we can use touch immediately on next action
    // else, we would have to wait for a real touch event with finger lifted to allow again touch
    // In some devices like i9100, touch events do not trigger an EV_KEY: so, ui_wait_key() cannot return by touching the screen
    // in pause on logs, we must touch a hardware key: we will fix this in touch code below
    if (ev.type == EV_KEY) {
        // this is a hardware key event. If ignore_key_action == 1, we must drop it
        // either we could set ev.value to 0 or we fake a non used key. Here, we fake KEY_ESC
        // on hardware key release, set ignore_key_action to 0 to be able to use touch immediately after a hardware key press
        if (ignore_key_action) {
            ev.code = KEY_ESC;
            if (ev.value == 0)
                ignore_key_action = 0;
        }
        key_handle_input(ev);
        return 1;
    }

    // accept only possible touch events:
    //  * filter EV_REL (mouse up/down only events)
    //  * EV_KEY (keyboard) was filtered above and redirected to key_handle_input()
    //  * also filter any other eventual sensor event
    //  * return 0 so that non touch events (mouse only for now...) can be handled by input_callback()
    if (ev.type != EV_ABS && ev.type != EV_SYN)
        return 0;

    // completely disable touch support if set by user
    if (touch_to_validate.value == NO_TOUCH_SUPPORT)
        return 0;

    // touch is init only with correct fd touch device:
    // ensure we have a touch event to init touch device, 
    if (!touch_is_init) {
        if (ev.type != EV_ABS) {
            // return 0 to handle key events until we start a touch event
            // we could also return 1 as this is an EV_SYN event that input_callback() won't handle anyway
            return 0;
        }
        if (touch_device_init(fd) != 0) {
            // this is an EV_ABS type event, but from invalid devices like accelerometer...
            // discard it as we will try to init touch on next EV_ABS event, hopefully from an other valid device
            return 1;
        }
    }

    // we are either ev.type == EV_ABS || ev.type == EV_SYN
    ret = touch_track(fd, ev);
    if (ret == -1) {
        // drop non touch events from an ignored device (gyroscope sending EV_ABS...)
        return 1;
    } else if (ret == 0) {
        // finger lifted! lets run with this
        in_touch = 0;
        t_old_last_touch = t_last_touch;
        t_last_touch = timenow_msec();

        if (ignore_key_action) {
            // vbutton_pressed and vk_pressed cannot be set to other than -1 on first touch if ignore_key_action == 1
            // so we can add the if/else without causing virtual button to remain pressed
            // set ev.type to EV_KEY but a non used ev.code: KEY_ESC
            // this will fix some devices cannot return from paused logs when touching the screen
            // the cause is touch events do not trigger an EV_KEY event by the driver, so ui_wait_key() cannot return
            ignore_key_action = 0;
            ev.type = EV_KEY;
            ev.code = KEY_ESC;
            ev.value = 1;
        } else if (vbutton_pressed != -1) {
            // release virtual button key if it was pressed
            toggle_key_pressed(vbutton_pressed, 0);
            vbutton_pressed = -1;
            now_scrolling = 0;
        } else if (vk_pressed != -1) {
            // release device virtual key if it was pressed
            vk_pressed = -1;
            now_scrolling = 0;
        } else if (now_scrolling != 0) {
            // - when scrolling, if finger is lifted, do some kinetics on scroll
            // - do not always clear key queue on finger lifted to allow use of touch outside menu screens
            //   exp: cancel nandroid job using device/recovery virtual keys
            // - do not always reset now_scrolling to 0 on finger lifted, else we always highlight on first touch during scroll
            // - scroll_speed != 0: avoid scroll persistence if we do a touch/lift action shortly (< 300 msec) after a fast scroll
            // - if after a scroll, we pause finger on screen for +300 msec then we lift it, assume we do not scroll
            if (scroll_speed == 0 || (t_last_touch - t_last_scroll_y) > 300) {
                scroll_speed = 0;
                now_scrolling = 0;
            } else {
                ev.type = EV_KEY;
                ev.code = KEY_PAGEDOWN;
                ev.value = 1;
            }
        } else if (touch_y < (fbh - virtual_keys_h) && touch_y != TOUCH_RESET_POS) {
            // finger lifted above the touch panel (aka virtual buttons) region
            // find the menu we were touching on finger lifted event
            int old_last_touched_menu = last_touched_menu;
            last_touched_menu = ui_valid_menu_touch(touch_y);
#ifdef RECOVERY_TOUCH_DEBUG
            LOGI("first touched menu=%d\n", first_touched_menu);
            LOGI("last touched menu=%d\n", last_touched_menu);
            if (abs(touch_x - first_x) > touch_accuracy.value || abs(touch_y - first_y) > touch_accuracy.value)
                LOGI("accuracy skip:x=%d, %d - y=%d, %d\n", touch_x, first_x, touch_y, first_y);
#endif
            if (abs(touch_x - first_x) <= touch_accuracy.value && abs(touch_y - first_y) <= touch_accuracy.value) {
                // finger did not move between first touch and when it was lifted (no scroll)
                if (last_touched_menu == -1 && key_gesture == 0) {
                    // finger lifted outside a valid menu, support double tap gestures
                    // do not allow gestures if there is one still ongoing (key_gesture != 0): wait for handle_gesture_actions() to return
                    if (t_last_touch - t_old_last_touch < 350 && t_old_last_touch != 0) {
                        ev.type = EV_KEY;
                        ev.code = KEY_LEFTBRACE;
                        key_gesture = DOUBLE_TAP_GESTURE;
                        ev.value = 1;
                      // support long press + lift finger gesture if finger pressed for 1 sec then lifted
                    } else if (t_last_touch - t_first_touch > 1000) {
                        ev.type = EV_KEY;
                        ev.code = KEY_LEFTBRACE;
                        key_gesture = PRESS_LIFT_GESTURE;
                        ev.value = 1;
                    }
                } else if (touch_to_validate.value == FULL_TOUCH_VALIDATION) {
                    // validate touched menu on finger lifted
                    // last_touched_menu == first_touched_menu: ensure we lifted finger above same menu we first touched
                    // this check fixes this bug: double tap in full mode could validate first menu: happens on very fast double taps as one touch can occur on a non drawn menu
                    // we add an extra check: ensure the menu we are going to validate is the real highlighted one (ui_get_selected_item())
                    if (last_touched_menu >= 0 && last_touched_menu == first_touched_menu &&
                            last_touched_menu == ui_get_selected_item()) {
                        ev.type = EV_KEY; //touch panel support!!!
                        ev.code = KEY_ENTER; // we fake a Hardware key to use in touch to highlight function
                        ev.value = 1;
                    }
                } else if (touch_to_validate.value == DOUBLE_TAP_VALIDATION) {
                    // * validate menu after double tap (on 2nd time finger lifted)
                    // * t_last_menu_touch != 0 avoids first touch to validate on first recovery start
                    // * last_touched_menu == old_last_touched_menu ensure the double tap was on same valid menu
                    // * last_touched_menu == ui_get_selected_item() ensures we are going to validate the real highlighted menu
                    if (t_last_touch - t_last_menu_touch < 350 && t_last_menu_touch != 0 &&
                            last_touched_menu >= 0 && last_touched_menu == old_last_touched_menu && last_touched_menu == ui_get_selected_item()) {
                        ev.type = EV_KEY;
                        ev.code = KEY_ENTER;
                        ev.value = 1;
                    }
                    t_last_menu_touch = t_last_touch;
                } /*
                else if (touch_to_validate.value == TOUCH_HIGHLIGHT_ONLY) {

                } */
            } else if (last_touched_menu == -1 && key_gesture == 0) {
                // support slide left and right actions outside menu items and virtual buttons area
                // do not allow gestures if there is one still ongoing (key_gesture != 0): wait for handle_gesture_actions() to return
                // touch_y != TOUCH_RESET_POS condition above should not cause issues on a so long slide
                if ((touch_x - first_x < ((fbw / 3) * -1)) && ui_valid_menu_touch(first_y) == -1) {
                    /* Finger moved at least 1/3 of board width to trigger slide right action
                     * To avoid triggering slide left/right action while scrolling in a deviated line:
                         - we require finger lifted outside menus (last_touched_menu == -1)
                         - we require first touch being outside a valid menu: ui_valid_menu_touch(first_y) == -1
                         - we require move 1/3 of board width */
                    ev.type = EV_KEY;
                    ev.code = KEY_LEFTBRACE;
                    key_gesture = SLIDE_LEFT_GESTURE;
                    ev.value = 1;
                } else if ((touch_x - first_x > (fbw / 3)) && ui_valid_menu_touch(first_y) == -1) {
                    /* Finger moved at least 1/3 of board width to trigger slide right action
                     * To avoid triggering slide left/right action while scrolling in a deviated line:
                         - we require finger lifted outside menus (last_touched_menu == -1)
                         - we require first touch being outside a valid menu: ui_valid_menu_touch(first_y) == -1
                         - we require move 1/3 of board width */
                    ev.type = EV_KEY;
                    ev.code = KEY_LEFTBRACE;
                    key_gesture = SLIDE_RIGHT_GESTURE;
                    ev.value = 1;
                }
            }
        }/* else if (touch_y > (fbh - virtual_keys_h) && touch_y < fbh) {
            // finger lifted in the touch panel (aka virtual buttons) region, not needed:
            // when lifted here, vbutton_pressed != -1 so on finger lifted, above code will reset and return 1
        }*/

        /* Whenever finger is lifted:
            - allow_long_press_move = 0 is optional. Next very first touch event will set it to 1 anyway.
            - reset_gestures() to reset all coordinates
            - don't reset now_scrolling to 0 here, else we get highlight on every first touch during scrolling
        */
        allow_long_press_move = 0;
        reset_gestures();
    } else if (ev.type == EV_ABS && current_slot == 0) {
        // this is a touch event
        // first touch and all finger swiping after this should be dropped
        if (in_touch == 0) {
            // starting to track touch, this is first touch. in_touch = 0 only when finger lifted
            // t_first_touch: allow first touch to select and validate after one sec of scrolling
            in_touch = 1;
            allow_long_press_move = 1; // only allow long press move gesture on first touch
            scroll_speed = 0; // avoid scroll persistence if we do a touch/lift action shortly (< 200 msec) after a fast scroll
            t_first_touch = timenow_msec();

            if (!ignore_key_action && now_scrolling != 0) {
                // After a scroll, we can highlight on first touch and validate on finger lifted:
                //   - if after the scroll and finger lifted, we wait +0.5 sec before next touch
                //     AND
                //   - if there are no active kinetics (key_queue_len == 0)
                // It will also limit false selections while scrolling
                // now_scrolling = 0 will also stop kinetics as we need it != 0 to call touch_scroll() function
                // the extra ignore_key_action check avoids clearing the queue we need to track wake up screen events
                pthread_mutex_lock(&key_queue_mutex);
                if (key_queue_len == 0 && (t_first_touch - t_last_touch) > 650)
                    now_scrolling = 0;

                // key_queue_len = 0: always stop scrolling kinetics on first touch.
                // unlike now_scrolling = 0, it won't allow highlight on first touch and validate on finger lifted
                key_queue_len = 0;
                pthread_mutex_unlock(&key_queue_mutex);
            }

            if (enable_vibrator.value)
                vibrate_device(VIBRATOR_TIME_MS);
        }

        if (ev.code == abs_mt_pos_horizontal) {
            // interpolate  coordinates if needed
            float touch_x_rel = (float)(ev.value - board_min_x) / (float)(board_max_x - board_min_x + 1);
            //printf("touch_x_rel=%f\n", touch_x_rel);        
            touch_x = touch_x_rel * fbw;
            //printf("touch_x_1=%i\n", touch_x);

            if (libtouch_flags.recovery_touchscreen_flip_x)
                touch_x = fbw - touch_x;

#ifdef RECOVERY_TOUCH_DEBUG
            LOGI("first_x=%d\n", first_x);
            LOGI("touch_x=%d\n", touch_x);
#endif
            if (first_x == TOUCH_RESET_POS) {
                // this is first x registration on touch
                first_x = touch_x;
            }

            // do not allow long press and move gesture action as soon as finger moves too much
            if (abs(touch_x - first_x) > (3 * touch_accuracy.value))
                allow_long_press_move = 0;

            if (touch_y > (fbh - virtual_keys_h) && touch_y < fbh) {
                // finger on contact with touch panel region (bottom virtual buttons)
                // no need to check for touch_x > 0 as it is always here
                // vbutton_pressed != -1: don't repeat virtual button while it is pressed and we make small moves (when key repeat is disabled)
                if (!ignore_key_action && vbutton_pressed == -1 && (ret = input_buttons()) != -1) {
                    vbutton_pressed = ret;
                    now_scrolling = 0;
                    ev.type = EV_KEY;
                    ev.code = vbutton_pressed;
                    ev.value = 1;
                }
            } else if (!ignore_key_action && device_has_vk && touch_y > fbh && vk_pressed == -1 && (ret = input_vk()) != -1) {
                // finger on contact with device virtual keys
                // vk_pressed != -1: don't repeate virtual key until finger is lifted
                vk_pressed = ret;
                now_scrolling = 0;
                ev.type = EV_KEY;
                ev.code = vk_pressed;
                ev.value = 1;
            } else if (vbutton_pressed != -1) {
                // finger above touch panel region (bottom virtual buttons)
                // release virtual button key if it was pressed and we swiped out side area without finger lifted
                toggle_key_pressed(vbutton_pressed, 0);
                vbutton_pressed = -1;
            }
        } else if (ev.code == abs_mt_pos_vertical) {
            // interpolate  coordinates if needed
            float touch_y_rel = (float)(ev.value - board_min_y) / (float)(board_max_y - board_min_y + 1);
            //printf("touch_y_rel=%f\n", touch_y_rel);        
            touch_y = touch_y_rel * fbh;
            //printf("touch_y_1=%i\n", touch_y);

            if (libtouch_flags.recovery_touchscreen_flip_y)
                touch_y = fbh - touch_y;

#ifdef RECOVERY_TOUCH_DEBUG
            LOGI("first_y=%d\n", first_y);
            LOGI("touch_y=%d\n", touch_y);
#endif
            if (first_y == TOUCH_RESET_POS) {
                // this is first y registration on touch,
                // finger was lifted before, and when lifted we have a reset_gestures()
                first_y = touch_y;
                last_scroll_y = touch_y;
                t_last_scroll_y = timenow_msec();
                first_touched_menu = ui_valid_menu_touch(touch_y);
                if (touch_y < (fbh - virtual_keys_h) && first_touched_menu >= 0 && now_scrolling == 0) {
                    // * we are above touch panel (virtual buttons) region
                    //   highlight on first touch, before finger lift and
                    //   only if we're not scrolling (now_scrolling == 0)
                    // * touch_sel = 1 to fake KEY_PAGEUP action to HIGHLIGHT UP/DOWN,
                    //   while it is 1, use of real KEY_PAGEUP button is blocked
                    // we allow this even if ignore_key_action == 1 since it is safe
                    touch_sel = 1;
                    ev.type = EV_KEY;
                    ev.code = KEY_PAGEUP; // we fake a Hardware key to use in touch to highlight function
                    ev.value = 1;
                }
            }

            // do not allow long press and move gesture action as soon as finger moves too much
            if (abs(touch_y - first_y) > (3 * touch_accuracy.value))
                allow_long_press_move = 0;

            // start touch up/down scroll code
            // only allow scrolling if we touch a valid menu (ui_valid_menu_touch(touch_y) >= 0)
            // this will fix slide left/right trigger while scrolling if scroll_sensitivity.value is set too low
            int val = touch_y - last_scroll_y;
            if (!ignore_key_action && abs(val) > scroll_sensitivity.value && ui_valid_menu_touch(touch_y) >= 0) {
                // calculate vertical finger speed for touch scroll kinetics
                long long t_now = timenow_msec();
                scroll_speed = (long)(1000 * ((double)(abs(val)) / (double)(t_now - t_last_scroll_y)));
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("speed: %ld\n", scroll_speed);
#endif
                last_scroll_y = touch_y;
                t_last_scroll_y = t_now;

                // check scroll direction
                if (val > 0) {
                    now_scrolling = -1;
                    ev.type = EV_KEY;
                    ev.code = KEY_PAGEDOWN;
                    ev.value = 1;
                } else {
                    now_scrolling = 1;
                    ev.type = EV_KEY;
                    ev.code = KEY_PAGEDOWN;
                    ev.value = 1;
                }
            }

            if (touch_y < (fbh - virtual_keys_h)) {
                // finger is above virtual buttons area
                // release virtual button key if it was pressed and we swiped out side area without finger lifted
                if (vbutton_pressed != -1) {
                    toggle_key_pressed(vbutton_pressed, 0);
                    vbutton_pressed = -1;
                } else if (key_gesture == 0 && !ignore_key_action && allow_long_press_move &&
                            (timenow_msec() - t_first_touch) > 2000 && ui_valid_menu_touch(touch_y) == -1) {
                    // support press and move gesture action after 2 sec only if outside menu area
                    // do not allow gestures if there is one still ongoing (key_gesture != 0): wait for handle_gesture_actions() to return
                    // no need to ensure finger did not move above touch_accuracy.value:
                    //    - allow_long_press_move is set to 0 as soon as finger  moves above that threshold
                    ev.type = EV_KEY;
                    ev.code = KEY_LEFTBRACE;
                    key_gesture = PRESS_MOVE_GESTURE;
                    ev.value = 1;
                    vibrate_device(VIBRATOR_TIME_MS);
                    allow_long_press_move = 0; // do not come back here until next first screen touch
                }
            } else if (touch_x != TOUCH_RESET_POS && touch_y > (fbh - virtual_keys_h) && touch_y < fbh) {
                // finger on contact with touch panel region (bottom virtual buttons)
                if (!ignore_key_action && vbutton_pressed == -1 && (ret = input_buttons()) != -1) {
                    vbutton_pressed = ret;
                    now_scrolling = 0;
                    ev.type = EV_KEY;
                    ev.code = vbutton_pressed;
                    ev.value = 1;
                }
            } else if (!ignore_key_action && device_has_vk && vk_pressed == -1 &&
                       touch_x != TOUCH_RESET_POS && (ret = input_vk()) != -1) {
                // finger on device virtual keys
                now_scrolling = 0;
                vk_pressed = ret;
                ev.type = EV_KEY;
                ev.code = vk_pressed;
                ev.value = 1;
            }
        }
    }

    // Handle touch key
    // at this point, ev.type == EV_KEY if it was successfully converted from touch to a key
    // else, ev.type == either EV_ABS or EV_SYN
    // both are not handled by input_callback(), so return 1
    if (ev.type != EV_KEY || ev.code > KEY_MAX)
        return 1;

#ifdef RECOVERY_TOUCH_DEBUG
    LOGI("Touch EV_KEY=%d\n", ev.code);
#endif

    pthread_mutex_lock(&key_queue_mutex);
    // enable virtual buttons to repeat while pressed, do not allow other fake touch keys
    if (vbutton_pressed != -1) {
        key_pressed[vbutton_pressed] = 1;
    }

    const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
    if (ev.value > 0 && key_queue_len < queue_max) {
        if (now_scrolling != 0) {
            // we were touch scrolling: handle kinetics
            if (in_touch) {
                // finger is on screen, scrolling
                // key_queue_len = 0: don't queue up/down events. Stops scrolling as soon as finger stops moving while in touch
                key_queue_len = 0;
                key_queue[key_queue_len] = ev.code;
                ++key_queue_len;
            } else {
                // finger lifted after we were scrolling
                // do some kinetics as needed
                int scroll_persist = 4 * (scroll_speed / gr_fb_height());
#ifdef RECOVERY_TOUCH_DEBUG
                LOGI("scroll_persist=%d\n", scroll_persist);
#endif
                if (scroll_persist != 0 && scroll_persist < 4)
                    scroll_persist = 4;
                if (scroll_persist > queue_max)
                    scroll_persist = queue_max - 1;

                key_queue_len = 0;
                while (key_queue_len < scroll_persist) {
                    // add key to the queue for scroll to persist on finger lifted
                    key_queue[key_queue_len] = ev.code;
                    ++key_queue_len;
                }
            }
        } else {
            // add key to the queue
            key_queue[key_queue_len] = ev.code;
            ++key_queue_len;
        }

        if (boardEnableKeyRepeat.value) {
            // needed or our fake up/down keys while swiping won't register in ui_wait_key_with_repeat()
            struct timeval now;
            gettimeofday(&now, NULL);

            key_press_time[ev.code] = (now.tv_sec * 1000) + (now.tv_usec / 1000);
            key_last_repeat[ev.code] = 0;
        }

        pthread_cond_signal(&key_queue_cond);
    }
    pthread_mutex_unlock(&key_queue_mutex);

    return 1;
}

// started on init: called only by ui_init()
// populate some variables for touch to limit calling functions
void touch_init() {
    // enable touch repeat and do not rely on ro.cwm.repeatable_keys / ro.cwm.enable_key_repeat
    boardEnableKeyRepeat.value = 1;
    boardRepeatableKeys[boardNumRepeatableKeys++] = KEY_UP;
    boardRepeatableKeys[boardNumRepeatableKeys++] = KEY_DOWN;
    boardRepeatableKeys[boardNumRepeatableKeys++] = KEY_VOLUMEUP;
    boardRepeatableKeys[boardNumRepeatableKeys++] = KEY_VOLUMEDOWN;
}
