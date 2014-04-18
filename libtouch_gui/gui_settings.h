
#ifndef __GUI_SETTINGS_H
#define __GUI_SETTINGS_H

// Wait for a key in show friendly log mode
// called in recovery.c to disable prompt for a key when installing zip files on boot
// works for both openrecoveryscript and edify extendedcommands
extern int force_wait;

void selective_load_theme_settings();

void show_touch_gui_menu();

void custom_rom_menu();

void show_log_menu();

// call to set ui_print in color, colored_rows_num is the number of rows to color from bottom to top
// if colored_rows_num == 0 then, color used for all ui_print text
void ui_print_preset_colors(int colored_rows_num, const char* color);

// Highlight on first touch
int ui_menu_touch_select();

// gesture action support
void handle_gesture_actions(const char** headers, char** items, int initial_selection);

// vibrator function
int vibrate_device(int timeout_ms);

// display and log the libtouch_gui version
void print_libtouch_version(int onscreen);

#endif // __GUI_SETTINGS_H

