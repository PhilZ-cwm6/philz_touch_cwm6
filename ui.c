/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

#include "voldclient/voldclient.h"
#include "minui/minui.h"
#include "common.h"
#include "recovery.h"
#include "advanced_functions.h"
#include "recovery_settings.h"
#include "ui.h"

extern int __system(const char *command);

#if defined(BOARD_HAS_NO_SELECT_BUTTON) || defined(PHILZ_TOUCH_RECOVERY)
static int gShowBackButton = 1;
#else
static int gShowBackButton = 0;
#endif

#define UI_WAIT_KEY_TIMEOUT_SEC    3600
#define UI_KEY_REPEAT_INTERVAL      80
#define UI_KEY_WAIT_REPEAT          400
#define UI_UPDATE_PROGRESS_INTERVAL 300

#define DEFAULT_INSTALL_OVERLAY_OFFSET_Y    190
UIParameters ui_parameters = {
    6,       // indeterminate progress bar frames
    20,      // fps
    7,       // installation icon frames (0 == static image)
    13, DEFAULT_INSTALL_OVERLAY_OFFSET_Y, // installation icon overlay offset
};

pthread_mutex_t gUpdateMutex = PTHREAD_MUTEX_INITIALIZER;
gr_surface gBackgroundIcon[NUM_BACKGROUND_ICONS];
static gr_surface *gInstallationOverlay;
static gr_surface *gProgressBarIndeterminate;
static gr_surface gStageMarkerEmpty;
static gr_surface gStageMarkerFill;
gr_surface gProgressBarEmpty;
gr_surface gProgressBarFill;
gr_surface gBackground;
#ifdef PHILZ_TOUCH_RECOVERY
gr_surface gVirtualKeys; // surface for our virtual key buttons
#endif
static int ui_has_initialized = 0;
static int ui_log_stdout = 1;

int boardRepeatableKeys[64], boardNumRepeatableKeys = 0;

struct bitmaps_array BITMAPS[] = {
    { &gBackgroundIcon[BACKGROUND_ICON_INSTALLING],             "icon_installing" },
    { &gBackgroundIcon[BACKGROUND_ICON_ERROR],                  "icon_error" },
    { &gBackgroundIcon[BACKGROUND_ICON_CLOCKWORK],              "icon_clockwork" },
    { &gBackgroundIcon[BACKGROUND_ICON_FIRMWARE_INSTALLING],    "icon_firmware_install" },
    { &gBackgroundIcon[BACKGROUND_ICON_FIRMWARE_ERROR],         "icon_firmware_error" },
    { &gProgressBarEmpty,                                       "progress_empty" },
    { &gProgressBarFill,                                        "progress_fill" },
#ifdef PHILZ_TOUCH_RECOVERY
    { &gVirtualKeys,                                            "virtual_keys" },
#endif
    { &gBackground,                                             "stitch" },
    { &gStageMarkerEmpty,                                       "stage_empty" },
    { &gStageMarkerFill,                                        "stage_fill" },
    { NULL,                                                     NULL },
};

// stage num / max to display for multi stage packages
static int stage = -1;
static int max_stage = -1;

static int gCurrentIcon = 0;
static int gInstallingFrame = 0;

int gProgressBarType = PROGRESSBAR_TYPE_NONE;

// Progress bar scope of current operation
static float gProgressScopeStart = 0, gProgressScopeSize = 0, gProgress = 0;
static double gProgressScopeTime, gProgressScopeDuration;

// Set to 1 when both graphics pages are the same (except for the progress bar)
static int gPagesIdentical = 0;

// Log text overlay, displayed when a magic key is pressed
char text[MAX_ROWS][MAX_COLS];
int text_cols = 0, text_rows = 0;
int text_col = 0, text_row = 0, text_top = 0;
bool show_text = 0;
bool show_text_ever = 0;   // has show_text ever been 1?

char menu[MENU_MAX_ROWS][MENU_MAX_COLS];
int show_menu = 0;
int menu_top = 0, menu_items = 0, menu_sel = 0;
int menu_show_start = 0;             // this is line which menu display is starting at
int max_menu_rows;

#ifdef NOT_ENOUGH_RAINBOWS
static unsigned cur_rainbow_color = 0;
static int gRainbowMode = 0;
#endif

// Key event input queue
pthread_mutex_t key_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t key_queue_cond = PTHREAD_COND_INITIALIZER;
int key_queue[256], key_queue_len = 0;
unsigned long key_last_repeat[KEY_MAX + 1], key_press_time[KEY_MAX + 1];
volatile char key_pressed[KEY_MAX + 1];

// Return the current time as a double (including fractions of a second).
static double now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Draw the given frame over the installation overlay animation.  The
// background is not cleared or draw with the base icon first; we
// assume that the frame already contains some other frame of the
// animation.  Does nothing if no overlay animation is defined.
// Should only be called with gUpdateMutex locked.
static void draw_install_overlay_locked(int frame) {
    if (gInstallationOverlay == NULL) return;
    gr_surface surface = gInstallationOverlay[frame];
    int iconWidth = gr_get_width(surface);
    int iconHeight = gr_get_height(surface);
    gr_blit(surface, 0, 0, iconWidth, iconHeight,
            ui_parameters.install_overlay_offset_x,
            ui_parameters.install_overlay_offset_y);
}

// Clear the screen and draw the currently selected background icon (if any).
// Should only be called with gUpdateMutex locked.
static void draw_background_locked(int icon)
{
    gPagesIdentical = 0;
    // gr_color(0, 0, 0, 255);
    // gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    int bw = gr_get_width(gBackground);
    int bh = gr_get_height(gBackground);
    int bx = 0;
    int by = 0;

    // draw the background image as a mosaic if it is smaller than display
    for (by = 0; by < gr_fb_height(); by += bh) {
        for (bx = 0; bx < gr_fb_width(); bx += bw) {
            gr_blit(gBackground, 0, 0, bw, bh, bx, by);
        }
    }

    // draw the background icon if any
    if (icon) {
        gr_surface surface = gBackgroundIcon[icon];
        int iconWidth = gr_get_width(surface);
        int iconHeight = gr_get_height(surface);
        int stageHeight = gr_get_height(gStageMarkerEmpty);

        int sh = (max_stage >= 0) ? stageHeight : 0;

        int iconX = (gr_fb_width() - iconWidth) / 2;
        int iconY = (gr_fb_height() - (iconHeight + sh)) / 2;

        gr_blit(surface, 0, 0, iconWidth, iconHeight, iconX, iconY);
        if (icon == BACKGROUND_ICON_INSTALLING) {
            draw_install_overlay_locked(gInstallingFrame);
        }

        if (stageHeight > 0) {
            int sw = gr_get_width(gStageMarkerEmpty);
            int x = (gr_fb_width() - max_stage * gr_get_width(gStageMarkerEmpty)) / 2;
            int y = iconY + iconHeight + 20;
            int i;
            for (i = 0; i < max_stage; ++i) {
                gr_blit((i < stage) ? gStageMarkerFill : gStageMarkerEmpty,
                        0, 0, sw, stageHeight, x, y);
                x += sw;
            }
        }
    }
}

// increment background progress icon frame (installation animation)
// called only with gUpdateMutex locked
static void ui_increment_frame() {
    gInstallingFrame =
        (gInstallingFrame + 1) % ui_parameters.installing_frames;
}

// Draw the progress bar (if any) on the screen. Does not flip pages.
// Should only be called with gUpdateMutex locked.
// never called if !ui_has_initialized
static long long t_last_progress_update = 0;
static void draw_progress_locked()
{
    if (gCurrentIcon == BACKGROUND_ICON_INSTALLING) {
        // update the installation animation, if active
        if (ui_parameters.installing_frames > 0)
            ui_increment_frame();
        draw_install_overlay_locked(gInstallingFrame);
    }

    if (gProgressBarType != PROGRESSBAR_TYPE_NONE) {
        int iconHeight = gr_get_height(gBackgroundIcon[BACKGROUND_ICON_INSTALLING]);
        int width = gr_get_width(gProgressBarEmpty);
        int height = gr_get_height(gProgressBarEmpty);

        int dx = (gr_fb_width() - width)/2;
        int dy = (3*gr_fb_height() + iconHeight - 2*height)/4;

        // Erase behind the progress bar (in case this was a progress-only update)
        gr_color(0, 0, 0, 255);
        gr_fill(dx, dy, width, height);

        if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL) {
            float progress = gProgressScopeStart + gProgress * gProgressScopeSize;
            int pos = (int) (progress * width);

            if (pos > 0) {
                gr_blit(gProgressBarFill, 0, 0, pos, height, dx, dy);
            }
            if (pos < width-1) {
                gr_blit(gProgressBarEmpty, pos, 0, width-pos, height, dx+pos, dy);
            }
        }

        if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE) {
            static int frame = 0;
            gr_blit(gProgressBarIndeterminate[frame], 0, 0, width, height, dx, dy);
            frame = (frame + 1) % ui_parameters.indeterminate_frames;
        }
    }

    t_last_progress_update = timenow_msec();
}

#ifndef PHILZ_TOUCH_RECOVERY
static void draw_text_line(int row, const char* t) {
  if (t[0] != '\0') {
#ifdef NOT_ENOUGH_RAINBOWS
    if (ui_get_rainbow_mode()) ui_rainbow_mode();
#endif
    gr_text(0, (row+1)*CHAR_HEIGHT-1, t, 0);
  }
}

//#define MENU_TEXT_COLOR 255, 160, 49, 255
#define MENU_TEXT_COLOR 0, 191, 255, 255
#define NORMAL_TEXT_COLOR 200, 200, 200, 255
#define HEADER_TEXT_COLOR NORMAL_TEXT_COLOR

#endif

// Redraw everything on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
void draw_screen_locked(void)
{
    if (!ui_has_initialized) return;
    draw_background_locked(gCurrentIcon);
    draw_progress_locked();

#ifdef PHILZ_TOUCH_RECOVERY
    draw_touch_menu();
#else
    if (show_text) {
        // don't "disable" the background anymore with this...
        // gr_color(0, 0, 0, 160);
        // gr_fill(0, 0, gr_fb_width(), gr_fb_height());

        int total_rows = gr_fb_height() / CHAR_HEIGHT;
        int i = 0;
        int j = 0;
        int row = 0;            // current row that we are drawing on
        if (show_menu) {
            gr_color(MENU_TEXT_COLOR);
            gr_fill(0, (menu_top + menu_sel - menu_show_start) * CHAR_HEIGHT,
                    gr_fb_width(), (menu_top + menu_sel - menu_show_start + 1)*CHAR_HEIGHT+1);

            gr_color(HEADER_TEXT_COLOR);
            for (i = 0; i < menu_top; ++i) {
                draw_text_line(i, menu[i]);
                row++;
            }

            if (menu_items - menu_show_start + menu_top >= max_menu_rows)
                j = max_menu_rows - menu_top;
            else
                j = menu_items - menu_show_start;

            gr_color(MENU_TEXT_COLOR);
            for (i = menu_show_start + menu_top; i < (menu_show_start + menu_top + j); ++i) {
                if (i == menu_top + menu_sel) {
                    gr_color(255, 255, 255, 255);
                    draw_text_line(i - menu_show_start , menu[i]);
                    gr_color(MENU_TEXT_COLOR);
                } else {
                    gr_color(MENU_TEXT_COLOR);
                    draw_text_line(i - menu_show_start, menu[i]);
                }
                row++;
                if (row >= max_menu_rows)
                    break;
            }

            gr_fill(0, row*CHAR_HEIGHT+CHAR_HEIGHT/2-1,
                    gr_fb_width(), row*CHAR_HEIGHT+CHAR_HEIGHT/2+1);
        }

        gr_color(NORMAL_TEXT_COLOR);
        int cur_row = text_row;
        int available_rows = total_rows - row - 1;
        int start_row = row + 1;
        if (available_rows < MAX_ROWS)
            cur_row = (cur_row + (MAX_ROWS - available_rows)) % MAX_ROWS;
        else
            start_row = total_rows - MAX_ROWS;

        int r;
        for (r = 0; r < (available_rows < MAX_ROWS ? available_rows : MAX_ROWS); r++) {
            draw_text_line(start_row + r, text[(cur_row + r) % MAX_ROWS]);
        }
    }
#endif
}

// Redraw everything on the screen and flip the screen (make it visible).
// Should only be called with gUpdateMutex locked.
void update_screen_locked(void)
{
    if (!ui_has_initialized) return;
    draw_screen_locked();
    gr_flip();
}

// Updates only the progress bar, if possible, otherwise redraws the screen.
// Should only be called with gUpdateMutex locked.
static void update_progress_locked(void)
{
    if (!ui_has_initialized) return;

    // minimum of UI_UPDATE_PROGRESS_INTERVAL msec delay between progress updates if we have a text overlay
    // exception: gProgressScopeDuration != 0: to keep zip installer refresh behaviour
    if (show_text && t_last_progress_update > 0 && gProgressScopeDuration == 0 && timenow_msec() - t_last_progress_update < UI_UPDATE_PROGRESS_INTERVAL)
        return;

    if (show_text || !gPagesIdentical) {
        draw_screen_locked();    // Must redraw the whole screen
        gPagesIdentical = 1;
    } else {
        draw_progress_locked();  // Draw only the progress bar and overlays
    }
    gr_flip();
}

// Keeps the progress bar updated, even when the process is otherwise busy.
static void *progress_thread(void *cookie)
{
    double interval = 1.0 / ui_parameters.update_fps;
    for (;;) {
        double start = now();
        pthread_mutex_lock(&gUpdateMutex);

        int redraw = 0;

        // update the progress bar animation, if active
        // skip this if we have a text overlay (too expensive to update)
        if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE) {
            redraw = 1;
        } else if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && gProgressScopeDuration > 0) {
            // move the progress bar forward on timed intervals, if configured
            double elapsed = now() - gProgressScopeTime;
            float progress = 1.0 * elapsed / gProgressScopeDuration;
            if (progress > 1.0) progress = 1.0;
            if (progress > gProgress) {
                gProgress = progress;
                redraw = 1;
            }
        }

        if (gCurrentIcon == BACKGROUND_ICON_INSTALLING) {
            redraw = 1;
        }

        if (redraw) update_progress_locked();

        pthread_mutex_unlock(&gUpdateMutex);
        double end = now();
        // minimum of 20ms delay between frames
        double delay = interval - (end-start);
        if (delay < 0.02) delay = 0.02;
        usleep((long)(delay * 1000000));
    }
    return NULL;
}

static int rel_sum = 0;

static int input_callback(int fd, short revents, void *data)
{
    struct input_event ev;
    int ret;
    int fake_key = 0;

    ret = ev_get_input(fd, revents, &ev);
    if (ret)
        return -1;

#ifdef PHILZ_TOUCH_RECOVERY
    if (touch_handle_input(fd, ev))
        return 0;
#endif

    if (ev.type == EV_SYN) {
        return 0;
    } else if (ev.type == EV_REL) {
        if (ev.code == REL_Y) {
            // accumulate the up or down motion reported by
            // the trackball.  When it exceeds a threshold
            // (positive or negative), fake an up/down
            // key event.
            rel_sum += ev.value;
            if (rel_sum > 3) {
                fake_key = 1;
                ev.type = EV_KEY;
                ev.code = KEY_DOWN;
                ev.value = 1;
                rel_sum = 0;
            } else if (rel_sum < -3) {
                fake_key = 1;
                ev.type = EV_KEY;
                ev.code = KEY_UP;
                ev.value = 1;
                rel_sum = 0;
            }
        }
    } else {
        rel_sum = 0;
    }

    if (ev.type != EV_KEY || ev.code > KEY_MAX)
        return 0;

    if (ev.value == 2) {
        boardEnableKeyRepeat.value = 0;
    }

    pthread_mutex_lock(&key_queue_mutex);
    if (!fake_key) {
        // our "fake" keys only report a key-down event (no
        // key-up), so don't record them in the key_pressed
        // table.
        key_pressed[ev.code] = ev.value;
    }
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

// Reads input events, handles special hot keys, and adds to the key queue.
static void *input_thread(void *cookie)
{
    for (;;) {
        if (!ev_wait(-1))
            ev_dispatch();
    }
    return NULL;
}

void ui_init(void)
{
    ui_has_initialized = 1;
    gr_init();
    ev_init(input_callback, NULL);
#ifdef PHILZ_TOUCH_RECOVERY
    touch_init();
#endif

    text_col = text_row = 0;
    text_rows = gr_fb_height() / CHAR_HEIGHT;
    max_menu_rows = text_rows - MIN_LOG_ROWS;

    if (max_menu_rows > MENU_MAX_ROWS)
        max_menu_rows = MENU_MAX_ROWS;
    if (text_rows > MAX_ROWS) text_rows = MAX_ROWS;
    text_top = 1;

    text_cols = gr_fb_width() / CHAR_WIDTH;
    if (text_cols > MAX_COLS - 1) text_cols = MAX_COLS - 1;

    int i;
    for (i = 0; BITMAPS[i].name != NULL; ++i) {
        int result = res_create_surface(BITMAPS[i].name, BITMAPS[i].surface);
        if (result < 0) {
            LOGE("Missing bitmap %s\n(Code %d)\n", BITMAPS[i].name, result);
        }
    }

    gProgressBarIndeterminate = malloc(ui_parameters.indeterminate_frames *
                                       sizeof(gr_surface));
    for (i = 0; i < ui_parameters.indeterminate_frames; ++i) {
        char filename[40];
        // "indeterminate01.png", "indeterminate02.png", ...
        sprintf(filename, "indeterminate%02d", i+1);
        int result = res_create_surface(filename, gProgressBarIndeterminate+i);
        if (result < 0) {
            LOGE("Missing bitmap %s\n(Code %d)\n", filename, result);
        }
    }

    if (ui_parameters.installing_frames > 0) {
        gInstallationOverlay = malloc(ui_parameters.installing_frames *
                                      sizeof(gr_surface));
        for (i = 0; i < ui_parameters.installing_frames; ++i) {
            char filename[40];
            // "icon_installing_overlay01.png",
            // "icon_installing_overlay02.png", ...
            sprintf(filename, "icon_installing_overlay%02d", i+1);
            int result = res_create_surface(filename, gInstallationOverlay+i);
            if (result < 0) {
                LOGE("Missing bitmap %s\n(Code %d)\n", filename, result);
            }
        }

        // Adjust the offset to account for the positioning of the
        // base image on the screen.
        if (gBackgroundIcon[BACKGROUND_ICON_INSTALLING] != NULL) {
            gr_surface bg = gBackgroundIcon[BACKGROUND_ICON_INSTALLING];
            ui_parameters.install_overlay_offset_x +=
                (gr_fb_width() - gr_get_width(bg)) / 2;
            ui_parameters.install_overlay_offset_y +=
                (gr_fb_height() - gr_get_height(bg)) / 2;
        }
    } else {
        gInstallationOverlay = NULL;
    }
#ifndef PHILZ_TOUCH_RECOVERY
    // we manage this in touch_init()
    char enable_key_repeat[PROPERTY_VALUE_MAX];
    property_get("ro.cwm.enable_key_repeat", enable_key_repeat, "");
    if (!strcmp(enable_key_repeat, "true") || !strcmp(enable_key_repeat, "1")) {
        boardEnableKeyRepeat.value = 1;

        char key_list[PROPERTY_VALUE_MAX];
        property_get("ro.cwm.repeatable_keys", key_list, "");
        if (strlen(key_list) == 0) {
            boardRepeatableKeys[boardNumRepeatableKeys++] = KEY_UP;
            boardRepeatableKeys[boardNumRepeatableKeys++] = KEY_DOWN;
            boardRepeatableKeys[boardNumRepeatableKeys++] = KEY_VOLUMEUP;
            boardRepeatableKeys[boardNumRepeatableKeys++] = KEY_VOLUMEDOWN;
        } else {
            char *pch = strtok(key_list, ",");
            while (pch != NULL) {
                boardRepeatableKeys[boardNumRepeatableKeys++] = atoi(pch);
                pch = strtok(NULL, ",");
            }
        }
    }
#endif

    pthread_t t;
    pthread_create(&t, NULL, progress_thread, NULL);
    pthread_create(&t, NULL, input_thread, NULL);
    //prints custom text at bottom of recovery interface on start
    //useless here if we use fast_ui_init() in default_recovery_ui.c: will be wiped
    //Better ui_print foot notes in recovery.c in that case
    //ui_prints are added in recovery.c under device_recovery_start()
    //ui_print("Clockworkmod 6.0.1.5\n");
}

int ui_is_initialized() {
    return ui_has_initialized;
}

char *ui_copy_image(int icon, int *width, int *height, int *bpp) {
    pthread_mutex_lock(&gUpdateMutex);
    draw_background_locked(icon);
    *width = gr_fb_width();
    *height = gr_fb_height();
    *bpp = sizeof(gr_pixel) * 8;
    int size = *width * *height * sizeof(gr_pixel);
    char *ret = malloc(size);
    if (ret == NULL) {
        LOGE("Can't allocate %d bytes for image\n", size);
    } else {
        memcpy(ret, gr_fb_data(), size);
    }
    pthread_mutex_unlock(&gUpdateMutex);
    return ret;
}

void ui_set_background(int icon)
{
#ifdef PHILZ_TOUCH_RECOVERY
    // call before locking gUpdateMutex as we need a ui_print in ui_friendly_log()
    ui_friendly_log(1);
#endif
    pthread_mutex_lock(&gUpdateMutex);
    gCurrentIcon = icon;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

// return current background icon
int ui_get_background_icon() {
    int icon;
    pthread_mutex_lock(&gUpdateMutex);
    icon = gCurrentIcon;
    pthread_mutex_unlock(&gUpdateMutex);
    return icon;
}

void ui_show_indeterminate_progress()
{
    if (!ui_has_initialized) return;

    pthread_mutex_lock(&gUpdateMutex);
    if (gProgressBarType != PROGRESSBAR_TYPE_INDETERMINATE) {
        gProgressBarType = PROGRESSBAR_TYPE_INDETERMINATE;
        update_progress_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_progress(float portion, int seconds)
{
    if (!ui_has_initialized) return;

    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NORMAL;
    gProgressScopeStart += gProgressScopeSize;
    gProgressScopeSize = portion;
    gProgressScopeTime = now();
    gProgressScopeDuration = seconds;
    gProgress = 0;
    update_progress_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_set_progress(float fraction)
{
    if (!ui_has_initialized) return;

    pthread_mutex_lock(&gUpdateMutex);
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && fraction > gProgress) {
        // Skip updates that aren't visibly different.
        int width = gr_get_width(gProgressBarIndeterminate[0]);
        float scale = width * gProgressScopeSize;
        if ((int) (gProgress * scale) != (int) (fraction * scale)) {
            gProgress = fraction;
            update_progress_locked();
        }
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_reset_progress()
{
    if (!ui_has_initialized) return;

    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NONE;
    gProgressScopeStart = 0;
    gProgressScopeSize = 0;
    gProgressScopeTime = 0;
    gProgressScopeDuration = 0;
    gProgress = 0;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

// do a reset and show the progress bar without updating screen
// it will be drawn on next call to update screen locked
// this is a loop friendly version to avoid flashy effects in loop calls
void ui_quick_reset_and_show_progress(float portion, int seconds)
{
    if (!ui_has_initialized) return;

    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NORMAL;
    gProgressScopeStart = 0;
    gProgressScopeSize = portion;
    gProgressScopeTime = now();
    gProgressScopeDuration = seconds;
    gProgress = 0;
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_get_text_cols() {
    return text_cols;
}

static int ui_print_no_screen_update = 0;
static int ui_print_replace_lines = 0;
void ui_set_nandroid_print(int enable, int num) {
    ui_print_no_screen_update = enable;
    ui_print_replace_lines = num;
}

void ui_print(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 256, fmt, ap);
    va_end(ap);

    // write text to log file
    if (ui_log_stdout)
        fputs(buf, stdout);

    if (!ui_has_initialized)
        return;

    // now, we write log to screen
    pthread_mutex_lock(&gUpdateMutex);
    if (ui_print_replace_lines) {
        int i;
        for(i = 0; i < ui_print_replace_lines; ++i) {
            text[text_row][0] = '\0';
            text_row = (text_row - 1 + text_rows) % text_rows;
            text_col = 0;
        }
    }

    if (text_rows > 0 && text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || text_col >= text_cols) {
                text[text_row][text_col] = '\0';
                text_col = 0;
                text_row = (text_row + 1) % text_rows;
                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
            }
            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
        }
        text[text_row][text_col] = '\0';
        if (!ui_print_no_screen_update)
            update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_printlogtail(int nb_lines) {
    char * log_data;
    char tmp[PATH_MAX];
    FILE * f;
    int line=0;
    //don't log output to recovery.log
    ui_log_stdout=0;
    sprintf(tmp, "tail -n %d /tmp/recovery.log > /tmp/tail.log", nb_lines);
    __system(tmp);
    f = fopen("/tmp/tail.log", "rb");
    if (f != NULL) {
        while (line < nb_lines) {
            log_data = fgets(tmp, PATH_MAX, f);
            if (log_data == NULL) break;
            ui_print("%s", tmp);
            line++;
        }
        fclose(f);
    }
#ifndef PHILZ_TOUCH_RECOVERY
    ui_print("Return to menu with any key.\n");
#endif
    ui_log_stdout=1;
}

int ui_start_menu(const char** headers, char** items, int initial_selection) {
#ifdef PHILZ_TOUCH_RECOVERY
    // call before locking gUpdateMutex as we need a ui_print in ui_friendly_log()
    ui_friendly_log(0);
#endif
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        for (i = 0; i < text_rows; ++i) {
            // populate top header array (menu title, clock, battery)
            if (headers[i] == NULL) break;

            int offset = 1;
#ifdef PHILZ_TOUCH_RECOVERY
            // do not let header text overlap on battery and clock top display
            if (i == 0)
                offset = ui_menu_header_offset();
#endif
            strncpy(menu[i], headers[i], text_cols - offset);
            menu[i][text_cols - offset] = '\0';
        }

        // populate menu items
        menu_top = i;
        for (; i < MENU_MAX_ROWS; ++i) {
            if (items[i-menu_top] == NULL) break;
            strcpy(menu[i], MENU_ITEM_HEADER);
            strncpy(menu[i] + MENU_ITEM_HEADER_LENGTH, items[i-menu_top], MENU_MAX_COLS - 1 - MENU_ITEM_HEADER_LENGTH);
            menu[i][MENU_MAX_COLS-1] = '\0';
        }

        if (gShowBackButton && !ui_root_menu) {
            strcpy(menu[i], " - +++++Go Back+++++");
            ++i;
        }

        menu_items = i - menu_top;
        show_menu = 1;
        menu_sel = menu_show_start = initial_selection;
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
    if (gShowBackButton && !ui_root_menu) {
        return menu_items - 1;
    }
    return menu_items;
}

// No need to pass a buffer as argument. But, call with item_menu[MENU_MAX_COLS] (sizeof(item_menu) >= MENU_MAX_COLS)
void ui_format_gui_menu(char *item_menu, const char* menu_text, const char* menu_option) {
#ifdef PHILZ_TOUCH_RECOVERY
    // truely right align options and left align menu text for any device
    ui_format_touch_menu(item_menu, menu_text, menu_option);
#else
    int len = strlen(menu_text) + strlen(menu_option) + strlen(" - ");
    if (len > MENU_MAX_COLS) {
        // no time to format it better: reduce menu length!
        strcpy(item_menu, "");
        return;
    }
    strcpy(item_menu, menu_option);
    strcat(item_menu, " - ");
    strcat(item_menu, menu_text);
#endif
}

int ui_menu_select(int sel) {
    int old_sel;
    pthread_mutex_lock(&gUpdateMutex);
    if (show_menu > 0) {
        old_sel = menu_sel;
        menu_sel = sel;

        if (menu_sel < 0) menu_sel = menu_items + menu_sel;
        if (menu_sel >= menu_items) menu_sel = menu_sel - menu_items;


        if (menu_sel < menu_show_start && menu_show_start > 0) {
            menu_show_start = menu_sel;
        }

        if (menu_sel - menu_show_start + menu_top >= max_menu_rows) {
            menu_show_start = menu_sel + menu_top - max_menu_rows + 1;
        }

        sel = menu_sel;

        if (menu_sel != old_sel) update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
    return sel;
}

void ui_end_menu() {
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    if (show_menu > 0 && text_rows > 0 && text_cols > 0) {
        show_menu = 0;
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

bool ui_IsTextVisible()
{
    pthread_mutex_lock(&gUpdateMutex);
    int visible = show_text;
    pthread_mutex_unlock(&gUpdateMutex);
    return visible;
}

// show_text was visible at least once
bool ui_WasTextEverVisible()
{
    pthread_mutex_lock(&gUpdateMutex);
    int ever_visible = show_text_ever;
    pthread_mutex_unlock(&gUpdateMutex);
    return ever_visible;
}

// immediately refresh screen and show text
void ui_ShowText(bool visible)
{
    pthread_mutex_lock(&gUpdateMutex);
    show_text = visible;
    if (show_text) show_text_ever = 1;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

// show text, but not immediately: next screen update will show text
void ui_SetShowText(bool visible) {
    show_text = visible;
    if (show_text) show_text_ever = 1;
}

// Return true if USB is connected.
static int usb_connected() {
    int fd = open("/sys/class/android_usb/android0/state", O_RDONLY);
    if (fd < 0) {
        printf("failed to open /sys/class/android_usb/android0/state: %s\n",
               strerror(errno));
        return 0;
    }

    char buf;
    /* USB is connected if android_usb state is CONNECTED or CONFIGURED */
    int connected = (read(fd, &buf, 1) == 1) && (buf == 'C');
    if (close(fd) < 0) {
        printf("failed to close /sys/class/android_usb/android0/state: %s\n",
               strerror(errno));
    }
    return connected;
}

// assigns -2 code to key queue which triggers GO_BACK in get_menu_selection()
void ui_cancel_wait_key() {
    pthread_mutex_lock(&key_queue_mutex);
    key_queue[key_queue_len] = -2;
    key_queue_len++;
    pthread_cond_signal(&key_queue_cond);
    pthread_mutex_unlock(&key_queue_mutex);
}

extern int volumes_changed();

int ui_wait_key()
{
    if (boardEnableKeyRepeat.value) return ui_wait_key_with_repeat();
    pthread_mutex_lock(&key_queue_mutex);
    int timeouts = UI_WAIT_KEY_TIMEOUT_SEC;
#ifdef PHILZ_TOUCH_RECOVERY
    int display_state = 0;
#endif
    // Time out after REFRESH_TIME_USB_INTERVAL seconds to catch volume changes, refresh clock, and loop for
    // UI_WAIT_KEY_TIMEOUT_SEC to restart a device not connected to USB
    do {
        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec;
        timeout.tv_nsec = now.tv_usec * 1000;
        timeout.tv_sec += REFRESH_TIME_USB_INTERVAL;

        int rc = 0;
        while (key_queue_len == 0 && rc != ETIMEDOUT) {
            rc = pthread_cond_timedwait(&key_queue_cond, &key_queue_mutex,
                                        &timeout);
            if (volumes_changed()) {
                pthread_mutex_unlock(&key_queue_mutex);
                return REFRESH;
            }
        }
        // reboot timer (timeouts) decrement must be same as timeout.tv_sec += increase value or we get out of time sync
        timeouts -= REFRESH_TIME_USB_INTERVAL;
#ifdef PHILZ_TOUCH_RECOVERY
        ui_refresh_display_state(&display_state);
#endif
    } while ((timeouts > 0 || usb_connected()) && key_queue_len == 0);

    int key = -1;
    if (key_queue_len > 0) {
        key = key_queue[0];
        memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
    }
    pthread_mutex_unlock(&key_queue_mutex);
    return key;
}

// util for ui_wait_key_with_repeat
int key_can_repeat(int key)
{
    int k = 0;
    for (;k < boardNumRepeatableKeys; ++k) {
        if (boardRepeatableKeys[k] == key) {
            break;
        }
    }
    if (k < boardNumRepeatableKeys) return 1;
    return 0;
}

int ui_wait_key_with_repeat()
{
    int key = -1;

    // Loop to wait for more keys.
    do {
        int timeouts = UI_WAIT_KEY_TIMEOUT_SEC;
#ifdef PHILZ_TOUCH_RECOVERY
        int display_state = 0;
#endif
        int rc = 0;
        struct timeval now;
        struct timespec timeout;
        pthread_mutex_lock(&key_queue_mutex);
        while (key_queue_len == 0 && timeouts > 0) {
            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec;
            timeout.tv_nsec = now.tv_usec * 1000;
            timeout.tv_sec += REFRESH_TIME_USB_INTERVAL;

            rc = 0;
            while (key_queue_len == 0 && rc != ETIMEDOUT) {
                rc = pthread_cond_timedwait(&key_queue_cond, &key_queue_mutex,
                                            &timeout);
                if (volumes_changed()) {
                    pthread_mutex_unlock(&key_queue_mutex);
                    return REFRESH;
                }
            }
            // reboot timer (timeouts) decrement must be same as timeout.tv_sec += increase value or we get out of time sync
            timeouts -= REFRESH_TIME_USB_INTERVAL;
#ifdef PHILZ_TOUCH_RECOVERY
            ui_refresh_display_state(&display_state);
#endif
        }
#ifdef PHILZ_TOUCH_RECOVERY
        // either a key was pressed (key_queue_len > 0) or reboot timer (timeouts) is reached
        // wake up screen if it was blanked or dimmed but only if no key was pressed (key_queue_len == 0)
        // this will avoid wake up screen after reboot timer reached AND USB cable is connected (no reboot) and screen was blanked/dimmed
        ui_refresh_display_state(&display_state);
#endif
        pthread_mutex_unlock(&key_queue_mutex);

        // either reboot timer is reached or a key was pressed
        // if reboot timer was reached (key_queue_len == 0) AND no USB cable connected, reboot by returning -1
        if (rc == ETIMEDOUT && !usb_connected()) {
            return -1;
        }

        // either a key was pressed (key_queue_len > 0) or reboot timer is reached AND USB cable is connected (key_queue_len == 0)
        // Loop to wait wait for more keys, or repeated keys to be ready.
        while (1) {
            unsigned long now_msec;

            gettimeofday(&now, NULL);
            now_msec = (now.tv_sec * 1000) + (now.tv_usec / 1000);

            pthread_mutex_lock(&key_queue_mutex);

            // Replacement for the while conditional, so we don't have to lock the entire
            // loop, because that prevents the input system from touching the variables while
            // the loop is running which causes problems.
            if (key_queue_len == 0) {
                pthread_mutex_unlock(&key_queue_mutex);
                break;
            }

            key = key_queue[0];
            memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);

            // sanity check the returned key.
            if (key < 0) {
                pthread_mutex_unlock(&key_queue_mutex);
                return key;
            }

            // Check for already released keys and drop them if they've repeated.
            if (!key_pressed[key] && key_last_repeat[key] > 0) {
                pthread_mutex_unlock(&key_queue_mutex);
                continue;
            }

            if (key_can_repeat(key)) {
                // Re-add the key if a repeat is expected, since we just popped it. The
                // if below will determine when the key is actually repeated (returned)
                // in the mean time, the key will be passed through the queue over and
                // over and re-evaluated each time.
                if (key_pressed[key]) {
                    key_queue[key_queue_len] = key;
                    key_queue_len++;
                }
                if ((now_msec > key_press_time[key] + UI_KEY_WAIT_REPEAT && now_msec > key_last_repeat[key] + UI_KEY_REPEAT_INTERVAL) ||
                        key_last_repeat[key] == 0) {
                    key_last_repeat[key] = now_msec;
                } else {
                    // Not ready
                    pthread_mutex_unlock(&key_queue_mutex);
                    continue;
                }
            }
            pthread_mutex_unlock(&key_queue_mutex);
            return key;
        }
    } while (1);

    return key;
}

int ui_key_pressed(int key)
{
    // This is a volatile static array, don't bother locking
    return key_pressed[key];
}

void ui_clear_key_queue() {
    pthread_mutex_lock(&key_queue_mutex);
    key_queue_len = 0;
    pthread_mutex_unlock(&key_queue_mutex);
}

void ui_set_log_stdout(int enabled) {
    ui_log_stdout = enabled;
}

int ui_should_log_stdout()
{
    return ui_log_stdout;
}

void ui_set_showing_back_button(int showBackButton) {
    gShowBackButton = showBackButton;
}

int ui_is_showing_back_button() {
    return gShowBackButton && !ui_root_menu;
}

int ui_get_selected_item() {
  return menu_sel;
}

int ui_handle_key(int key, int visible) {
#ifdef PHILZ_TOUCH_RECOVERY
    return touch_handle_key(key, visible);
#else
    return device_handle_key(key, visible);
#endif
}

// must be called after ui_init()
void ui_SetStage(int current, int max) {
    pthread_mutex_lock(&gUpdateMutex);
    stage = current;
    max_stage = max;

    if (gInstallationOverlay != NULL && gBackgroundIcon[BACKGROUND_ICON_INSTALLING] != NULL) {
        // Adjust the offset to account for the positioning of the
        // base image on the screen.
        gr_surface bg = gBackgroundIcon[BACKGROUND_ICON_INSTALLING];
        ui_parameters.install_overlay_offset_y = DEFAULT_INSTALL_OVERLAY_OFFSET_Y +
            (gr_fb_height() - (gr_get_height(bg) +
                               ((max_stage >= 0) ? gr_get_height(gStageMarkerEmpty) : 0))) / 2;
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

#ifdef NOT_ENOUGH_RAINBOWS
int ui_get_rainbow_mode() {
    return gRainbowMode;
}

void ui_rainbow_mode() {
    static int colors[] = { 255, 0, 0,        // red
                            255, 127, 0,      // orange
                            255, 255, 0,      // yellow
                            0, 255, 0,        // green
                            60, 80, 255,      // blue
                            143, 0, 255 };    // violet

    gr_color(colors[cur_rainbow_color], colors[cur_rainbow_color+1], colors[cur_rainbow_color+2], 255);
    cur_rainbow_color += 3;
    if (cur_rainbow_color >= (sizeof(colors) / sizeof(colors[0]))) cur_rainbow_color = 0;
}

void ui_set_rainbow_mode(int rainbowMode) {
    gRainbowMode = rainbowMode;

    pthread_mutex_lock(&gUpdateMutex);
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}
#endif

