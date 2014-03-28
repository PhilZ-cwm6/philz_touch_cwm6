#ifndef __UI_H
#define __UI_H


#include "ui_defines.h"
#include "common.h"


extern pthread_mutex_t gUpdateMutex;

//common.h and minui.h
extern gr_surface gBackgroundIcon[NUM_BACKGROUND_ICONS];
extern gr_surface gProgressBarEmpty;
extern gr_surface gProgressBarFill;
extern gr_surface gBackground;
#ifdef PHILZ_TOUCH_RECOVERY
extern gr_surface gVirtualKeys;
#endif

extern int boardRepeatableKeys[64];
extern int boardNumRepeatableKeys;

struct bitmaps_array {
    gr_surface* surface;
    const char *name;
};

extern struct bitmaps_array BITMAPS[];

enum ProgressBarType {
    PROGRESSBAR_TYPE_NONE,
    PROGRESSBAR_TYPE_INDETERMINATE,
    PROGRESSBAR_TYPE_NORMAL,
};

extern int gProgressBarType;

extern char text[MAX_ROWS][MAX_COLS];
extern int text_cols;
extern int text_rows;
extern int text_col;
extern int text_row;
extern int text_top;
extern int show_text;
extern int show_text_ever;
extern char menu[MENU_MAX_ROWS][MENU_MAX_COLS];
extern int show_menu;
extern int menu_top;
extern int menu_items;
extern int menu_sel;
extern int menu_show_start;
extern int max_menu_rows;

extern pthread_mutex_t key_queue_mutex;
extern pthread_cond_t key_queue_cond;
extern int key_queue[256];
extern int key_queue_len;
extern unsigned long key_last_repeat[KEY_MAX + 1];
extern unsigned long key_press_time[KEY_MAX + 1];
extern volatile char key_pressed[KEY_MAX + 1];

void update_screen_locked(void);

void draw_screen_locked(void);

void update_screen_locked(void);

#ifdef PHILZ_TOUCH_RECOVERY
void draw_touch_menu();
void draw_text_line(int row, const char* t, int height, int align);
void touch_init();
void ui_friendly_log(int engage_friendly_view);
void ui_format_touch_menu(char *item_menu, const char* menu_text, const char* menu_option);
void ui_refresh_display_state(int *screen_timeout);
int ui_menu_header_offset();
int touch_handle_input(int fd, struct input_event ev);
int touch_handle_key(int key_code, int visible);
#endif

#endif // __UI_H
