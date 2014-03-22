/*
 *   -- http://android-fb2png.googlecode.com/svn/trunk/fb.c --
 *
 *   Copyright 2011, Kyan He <kyan.ql.he@gmail.com>
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "log.h"
#include "fb.h"
#include "img_process.h"

// log fb info
void fb_dump(const struct fb* fb)
{
    D("%13s : %d", "bpp", fb->bpp);
    D("%13s : %d", "size", fb->size);
    D("%13s : %d", "width", fb->width);
    D("%13s : %d", "height", fb->height);
    D("%13s : %d %d %d %d", "ARGB offset",
            fb->alpha_offset, fb->red_offset,
            fb->green_offset, fb->blue_offset);
    D("%13s : %d %d %d %d", "ARGB length",
            fb->alpha_length, fb->red_length,
            fb->green_length, fb->blue_length);
}

/**
 * Returns the format of fb.
 */
static int fb_get_format(const struct fb *fb)
{
    int ao = fb->alpha_offset;
    int ro = fb->red_offset;
    int go = fb->green_offset;
    int bo = fb->blue_offset;

#define FB_FORMAT_UNKNOWN   0
#define FB_FORMAT_RGB565    1
#define FB_FORMAT_ARGB8888  2
#define FB_FORMAT_RGBA8888  3
#define FB_FORMAT_ABGR8888  4
#define FB_FORMAT_BGRA8888  5
#define FB_FORMAT_RGBX8888  FB_FORMAT_RGBA8888

    /* TODO: use offset */
    if (fb->bpp == 16)
        return FB_FORMAT_RGB565;

    /* TODO: validate */
    if (ao == 0 && ro == 8)
        return FB_FORMAT_ARGB8888;

    /*
    CWM: support devices with TARGET_RECOVERY_PIXEL_FORMAT := "RGBX_8888"
    if (PIXEL_FORMAT == GGL_PIXEL_FORMAT_RGBX_8888) {
             vi.red.offset     = 24;
             vi.red.length     = 8;
             vi.green.offset   = 16;
             vi.green.length   = 8;
             vi.blue.offset    = 8;
             vi.blue.length    = 8;
             vi.transp.offset  = 0;
             vi.transp.length  = 8;
    }
    */
    if (ao == 0 && ro == 24 && go == 16 && bo == 8)
        return FB_FORMAT_RGBX8888;

    if (ao == 0 && bo == 8)
        return FB_FORMAT_ABGR8888;

    if (ro == 0)
        return FB_FORMAT_RGBA8888;

    if (bo == 0)
        return FB_FORMAT_BGRA8888;

    /* fallback */
    return FB_FORMAT_UNKNOWN;
}

int fb_save_png(const struct fb *fb, const char *path)
{
    char *rgb_matrix;
    int ret = -1;

    /* Allocate RGB Matrix. */
    rgb_matrix = malloc(fb->width * fb->height * 3);
    if(!rgb_matrix) {
        D("rgb_matrix: memory error");
        return -1;
    }

    int fmt = fb_get_format(fb);
    D("Framebuffer Pixel Format: %d", fmt);

    switch(fmt) {
        case FB_FORMAT_RGB565:
            /* emulator use rgb565 */
            ret = rgb565_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        case FB_FORMAT_ARGB8888:
            /* most devices use argb8888 */
            ret = argb8888_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        case FB_FORMAT_ABGR8888:
            ret = abgr8888_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        case FB_FORMAT_BGRA8888:
            ret = bgra8888_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        case FB_FORMAT_RGBA8888:
            ret = rgba8888_to_rgb888(fb->data,
                    rgb_matrix, fb->width * fb->height);
            break;
        default:
            D("Unsupported framebuffer type.");
            break;
    }

    if (ret != 0)
        D("Error while processing input image.");
    else if (0 != (ret = save_png(path, rgb_matrix, fb->width, fb->height)))
        D("Failed to save in PNG format.");

    free(rgb_matrix);
    free(fb->data);
    return ret;
}
