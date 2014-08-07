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


#ifndef __TOUCH_GUI_H
#define __TOUCH_GUI_H

#include <stdbool.h>

// check if a progress bar is being displayed
int ui_showing_progress_bar();

// direct draw a text line on screen and make it visible: not shown in log
void draw_visible_text_line(int row, const char* t, int align);

// used to cancel nandroid operations and support dim screen during backup/restore
int key_press_event();

// call update screen locked
void ui_update_screen();

/****************************/
/*   Toggle menu settings   */
/****************************/

// gesture action support
extern int key_gesture;
#define SLIDE_LEFT_GESTURE  1
#define SLIDE_RIGHT_GESTURE 2
#define DOUBLE_TAP_GESTURE  3
#define PRESS_LIFT_GESTURE  4
#define PRESS_MOVE_GESTURE  5

// Touch to validate toggle modes
#define NO_TOUCH_SUPPORT        -1
#define TOUCH_HIGHLIGHT_ONLY    0
#define FULL_TOUCH_VALIDATION   1
#define DOUBLE_TAP_VALIDATION   2

// brightness and blank screen settings
#define BRIGHTNESS_DEFAULT_VALUE 160
extern bool is_blanked;
extern bool is_dimmed;
void apply_brightness_value(long int dim_value);
void ui_blank_screen(bool blank_screen);
void ui_dim_screen(bool dim_screen);

// live refresh menu height and other settings normally done on recovery start using ui_init()
void fast_ui_init();

// live refresh of background and icon images
void fast_ui_init_png();

// Menu Height
// FONT_HEIGHT will define row height (that is menu hight), it is the font size (CHAR_HEIGHT = BOARD_RECOVERY_CHAR_HEIGHT in ui_defines.h)
// we use a different macro name for libtouch_gui
// for roboto_23x41.h font, BOARD_RECOVERY_CHAR_HEIGHT == 41
// smallest font height is now 16: font_7x16.h
// Minimum increase of row height is 4 to not overlap menu separators
// We will then increase menu_height_increase.value by FONT_HEIGHT/4 increments (same as FONT_HEIGHT_ROUNDED/4)
// Max increase will be set to MENU_HEIGHT_INCREASE_0 * 3 (that is around FONT_HEIGHT * 3)
// We add MENU_HEIGHT_INCREASE_MIN (+4) to that max to effectively reach 4*BOARD_RECOVERY_CHAR_HEIGHT

#define FONT_WIDTH                  (libtouch_flags.char_width)
#define FONT_HEIGHT                 (libtouch_flags.char_height)

/* To change default menu height at compile time:
#define MENU_HEIGHT_INCREASE_0      ((FONT_HEIGHT_ROUNDED) * 1.25)
#define MENU_HEIGHT_INCREASE_0      ((FONT_HEIGHT_ROUNDED) * 1.5)
#define MENU_HEIGHT_INCREASE_0      ((FONT_HEIGHT_ROUNDED) * 1.75)
#define MENU_HEIGHT_INCREASE_0      ((FONT_HEIGHT_ROUNDED) * 2)
...
#define MENU_HEIGHT_INCREASE_0      ((FONT_HEIGHT_ROUNDED) * 3)
*/
#define FONT_HEIGHT_ROUNDED         ((FONT_HEIGHT) - ((FONT_HEIGHT) % 4))
#define MENU_HEIGHT_INCREASE_0      FONT_HEIGHT_ROUNDED
#define MENU_HEIGHT_INCREASE_MIN    ((FONT_HEIGHT_ROUNDED) / 4)
#define MENU_HEIGHT_INCREASE_MAX    ((FONT_HEIGHT_ROUNDED) * 3)
#define MENU_HEIGHT_INCREASE_INIT   4   // initialization value must be constant

// total menu height (font_height + height_increase)
#define MENU_HEIGHT_TOTAL           ((FONT_HEIGHT) + menu_height_increase.value)

// Scroll Sensitivity: it is incremented by SCROLL_SENSITIVITY_MIN steps up to maximum of SCROLL_SENSITIVITY_MAX
#define SCROLL_SENSITIVITY_0    ((MENU_HEIGHT_TOTAL) - ((MENU_HEIGHT_TOTAL) % 4))
#define SCROLL_SENSITIVITY_MIN  ((SCROLL_SENSITIVITY_0) / 4)
#define SCROLL_SENSITIVITY_MAX  ((SCROLL_SENSITIVITY_0) * 2)
#define SCROLL_SENSITIVITY_INIT 16

// Touch accuracy: TOUCH_ACCURACY_INIT is the initialization value as it must be constant
#define TOUCH_ACCURACY_INIT 7
#define TOUCH_ACCURACY_0    (libtouch_flags.board_has_low_resolution ? 3 : 7)


/*************************/
/* Start color functions */
/*************************/

// call to set ui_print in color, colored_rows_num is the number of rows to color from bottom to top
// if colored_rows_num == 0 then, default color used for all ui_print text
void ui_print_color(int colored_rows_num, int *color);

// arrays to hold menu color codes
extern int mtext_code[4]; // menu text color code
extern int mbackg_code[4]; // menu background color code
extern int mhlight_code[4]; // menu highlight color code
extern int normal_text_code[4]; // normal text code (ui_print)
extern int header_text_code[4]; // color code for header
extern int mseparator_code[4]; // line separator between menus
extern int batt_clock_code[4]; // color code for battery and clock

// menu color array channels id
#define RED_CHANNEL   0
#define GREEN_CHANNEL 1
#define BLUE_CHANNEL  2
#define ALPHA_CHANNEL 3

//color id toggles
#define WHITE_COLOR     0
#define BLACK_COLOR     1
#define CYAN_BLUE       2
#define DEEPSKY_BLUE    3
#define NORMAL_BLUE     4
#define DARK_BLUE       5
#define MYSTY_ROSE      6
#define PINK_COLOR      7
#define THISTLE_COLOR   8
#define TAN_COLOR       9
#define ROSY_BROWN      10
#define NORMAL_RED      11
#define FIREBRICK_COLOR 12
#define DARK_RED        13
#define ORANGE_RED      14
#define MAGENTA_COLOR   15
#define BLUEVIOLET      16
#define DARK_MAGENTA    17
#define LIME_GREEN      18
#define NORMAL_GREEN    19
#define DARK_GREEN      20
#define DARK_KHAKI      21
#define DARK_OLIVE      22
#define CUSTOM_SILVER   23
#define DARK_GRAY       24
#define NORMAL_GRAY     25
#define DIM_GRAY        26
#define DIMMER_GRAY     27
#define YELLOW_COLOR    28
#define GOLD_COLOR      29

// set maximum color value to toggle between 0 and MAX_COLORS
#define MAX_COLORS      29

// handle color codes arrays
#define WHITE_COLOR_CODE     255,255,255,255
#define BLACK_COLOR_CODE     0,0,0,255
#define CYAN_BLUE_CODE       0,247,255,255
#define DEEPSKY_BLUE_CODE    0,191,255,255
#define NORMAL_BLUE_CODE     0,0,255,255
#define DARK_BLUE_CODE       0,0,139,255
#define MYSTY_ROSE_CODE      255,228,225,255
#define PINK_COLOR_CODE      255,192,203,255
#define THISTLE_COLOR_CODE   216,191,216,255
#define TAN_COLOR_CODE       210,180,140,255
#define ROSY_BROWN_CODE      188,143,143,255
#define NORMAL_RED_CODE      255,0,0,255
#define FIREBRICK_CODE       178,34,34,255
#define DARK_RED_CODE        139,0,0,255
#define ORANGE_RED_CODE      255,69,0,255
#define MAGENTA_COLOR_CODE   255,0,255,255
#define BLUEVIOLET_CODE      138,43,226,255
#define DARK_MAGENTA_CODE    139,0,139,255
#define LIME_GREEN_CODE      50,205,50,255
#define NORMAL_GREEN_CODE    0,128,0,255
#define DARK_GREEN_CODE      0,100,0,255
#define DARK_KHAKI_CODE      189,183,107,255
#define DARK_OLIVE_CODE      85,107,47,255
#define CUSTOM_SILVER_CODE   200,200,200,255
#define DARK_GRAY_CODE       169,169,169,255
#define NORMAL_GRAY_CODE     128,128,128,255
#define DIM_GRAY_CODE        105,105,105,255
#define DIMMER_GRAY_CODE     50,50,50,255
#define YELLOW_COLOR_CODE    255,255,0,255
#define GOLD_COLOR_CODE      255,215,0,255

// define default color used when calling ui_print_default_color()
#define DEFAULT_UI_PRINT_COLOR      CYAN_BLUE_CODE

// highlight color of virtual keys
#define VK_KEY_HIGHLIGHT_COLOR      CYAN_BLUE_CODE

/****************************************/
/*   Start support for theme settings   */
/****************************************/
#define PHILZ_MAGENTA_THEME true

// PhilZ Touch 4 Black Theme
#ifdef PHILZ_BLACK_THEME
// background image on start is set in apply_background_image()
#define DEFAULT_MENU_TEXT_CODE          CYAN_BLUE_CODE
#define DEFAULT_MENU_BACKGROUND_CODE    BLACK_COLOR_CODE
#define DEFAULT_MENU_HIGHLIGHT_CODE     DEEPSKY_BLUE_CODE
#define DEFAULT_NORMAL_TEXT_CODE        CUSTOM_SILVER_CODE
#define DEFAULT_MENU_SEPARATOR_CODE     CYAN_BLUE_CODE
#define DEFAULT_HEADER_TEXT_CODE        CUSTOM_SILVER_CODE
#define DEFAULT_BATT_CLOCK_CODE         CYAN_BLUE_CODE

// define color id default toggles
#define DEFAULT_MENU_TEXT_COLOR         CYAN_BLUE
#define DEFAULT_MENU_BACKGROUND_COLOR   BLACK_COLOR
#define DEFAULT_MENU_HIGHLIGHT_COLOR    DEEPSKY_BLUE
#define DEFAULT_NORMAL_TEXT_COLOR       CUSTOM_SILVER
#define DEFAULT_MENU_SEPARATOR_COLOR    CYAN_BLUE
#define DEFAULT_HEADER_TEXT_COLOR       CUSTOM_SILVER
#define DEFAULT_BATT_CLOCK_COLOR        CYAN_BLUE

// define transparency defaults
#define DEFAULT_BACKGROUND_ALPHA    102
#define DEFAULT_HIGHLIGHT_ALPHA     153
#define DEFAULT_SEPARATOR_ALPHA     102
#endif  // PHILZ_BLACK_THEME

// PhilZ Touch 5 Magenta Theme
#ifdef PHILZ_MAGENTA_THEME
// background image on start is set in apply_background_image()
#define DEFAULT_MENU_TEXT_CODE          THISTLE_COLOR_CODE
#define DEFAULT_MENU_BACKGROUND_CODE    BLACK_COLOR_CODE
#define DEFAULT_MENU_HIGHLIGHT_CODE     PINK_COLOR_CODE
#define DEFAULT_NORMAL_TEXT_CODE        WHITE_COLOR_CODE
#define DEFAULT_MENU_SEPARATOR_CODE     MYSTY_ROSE_CODE
#define DEFAULT_HEADER_TEXT_CODE        THISTLE_COLOR_CODE
#define DEFAULT_BATT_CLOCK_CODE         CYAN_BLUE_CODE

// define color id default toggles
#define DEFAULT_MENU_TEXT_COLOR         THISTLE_COLOR
#define DEFAULT_MENU_BACKGROUND_COLOR   BLACK_COLOR
#define DEFAULT_MENU_HIGHLIGHT_COLOR    PINK_COLOR
#define DEFAULT_NORMAL_TEXT_COLOR       WHITE_COLOR
#define DEFAULT_MENU_SEPARATOR_COLOR    MYSTY_ROSE
#define DEFAULT_HEADER_TEXT_COLOR       THISTLE_COLOR
#define DEFAULT_BATT_CLOCK_COLOR        CYAN_BLUE

// define transparency defaults
#define DEFAULT_BACKGROUND_ALPHA    102
#define DEFAULT_HIGHLIGHT_ALPHA     102
#define DEFAULT_SEPARATOR_ALPHA     102
#endif  // PHILZ_MAGENTA_THEME

// selective theme gui settings we load
#define THEME_GUI_SETTINGS { \
    set_brightness.key, \
    menu_height_increase.key, \
    min_log_rows.key, \
    show_background_icon.key, \
    background_image.key, \
    header_text_color.key, \
    menu_text_color.key, \
    normal_text_color.key, \
    menu_background_color.key, \
    menu_background_transparency.key, \
    menu_highlight_color.key, \
    menu_highlight_transparency.key, \
    menu_separator_color.key, \
    menu_separator_transparency.key, \
    show_menu_separation.key, \
    show_virtual_keys.key, \
    show_clock.key, \
    show_battery.key, \
    batt_clock_color.key, \
    dim_timeout.key, \
    blank_timeout.key, \
    NULL \
}
//-------- End support for theme settings

#endif // __TOUCH_GUI_H
