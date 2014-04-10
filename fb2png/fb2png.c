/**
 * fb2png  Save screenshot into .png.
 *
 * Copyright (C) 2012  Kyan <kyan.ql.he@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <errno.h>

#include "log.h"
#include "fb2png.h"
#include "fb.h"

// multi buffering support
// -1: will be auto detect (default)
// 0 for single buffering, 1 for double, 2 for triple, 3 for 4x buffering
int user_set_buffers_num = -1;

// debugging code for new overlay devices
#define MDP_V4_0 400
static int overlay_supported = 0;
int target_has_overlay(char *version) {
    int mdp_version;

    if (strlen(version) >= 8) {
        if(!strncmp(version, "msmfb", strlen("msmfb"))) {
            char str_ver[4];
            memcpy(str_ver, version + strlen("msmfb"), 3);
            str_ver[3] = '\0';
            mdp_version = atoi(str_ver);
            if (mdp_version >= MDP_V4_0) {
                overlay_supported = 1;
                D("overlay=msmfb");
            }
        } else if (!strncmp(version, "mdssfb", strlen("mdssfb"))) {
            overlay_supported = 1;
            D("overlay=mdssfb");
        }
    }

    return overlay_supported;
}

/**
 * Get the {@code struct fb} from device's framebuffer.
 * Return
 *      0 for success.
 */
int get_device_fb(const char* path, struct fb *fb)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    unsigned char *raw;
    unsigned int bytespp;
    unsigned int raw_size;
    unsigned int raw_line_length;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    if(ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        D("ioctl failed, %s", strerror(errno));
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        D("ioctl fixed failed, %s", strerror(errno));
        close(fd);
        return -1;
    }

    bytespp = vinfo.bits_per_pixel / 8;
    raw_line_length = finfo.line_length; // (xres + padding_offset) * bytespp
    raw_size = vinfo.yres * raw_line_length;

    // output data handler struct
    fb->bpp = vinfo.bits_per_pixel;
    fb->size = vinfo.xres * vinfo.yres * bytespp;
    fb->width = vinfo.xres;
    fb->height = vinfo.yres;
    fb->red_offset = vinfo.red.offset;
    fb->red_length = vinfo.red.length;
    fb->green_offset = vinfo.green.offset;
    fb->green_length = vinfo.green.length;
    fb->blue_offset = vinfo.blue.offset;
    fb->blue_length = vinfo.blue.length;
    fb->alpha_offset = vinfo.transp.offset;
    fb->alpha_length = vinfo.transp.length;

    // container for raw bits from the active frame buffer
    raw = malloc(raw_size);
    if (!raw) {
        D("raw: memory error");
        close(fd);
        return -1;
    }

    // debug info for overlay devices
    target_has_overlay(finfo.id);

    // capture active buffer: n is 0 for first buffer, 1 for second
    // graphics.c -> set_active_framebuffer() -> vi.yoffset = n * vi.yres;
    unsigned int active_buffer_offset = 0;
    int num_buffers = user_set_buffers_num;
    if (num_buffers < 0) {
        // default: auto detect
        num_buffers = (int)(vinfo.yoffset / vinfo.yres);
        if (num_buffers > MAX_ALLOWED_FB_BUFFERS)
            num_buffers = 0;
    }

    if (finfo.smem_len >= (raw_size * (num_buffers + 1))) {
        active_buffer_offset = raw_size * num_buffers;
    }

    // display debug fb info
    fb_dump(fb);
    D("%13s : %u", "bytespp", bytespp);
    D("%13s : %u", "raw size", raw_size);
    D("%13s : %u", "yoffset", vinfo.yoffset);
    D("%13s : %u", "pad offset", (raw_line_length / bytespp) - fb->width);
    D("%13s : %u", "buffer offset", active_buffer_offset);

    // copy the active frame buffer bits into the raw container
    lseek(fd, active_buffer_offset, SEEK_SET);
    ssize_t read_size = read(fd, raw, raw_size);
    if (read_size < 0 || (unsigned)read_size != raw_size) {
        D("read buffer error: %s", strerror(errno));
        goto oops;
    }

/*  
    Image padding (needed on some RGBX_8888 formats, maybe others?)
    we have padding_offset in bytes and bytespp = bits_per_pixel / 8
    raw_line_length = (width + padding_offset) * bytespp

    This gives: padding_offset = (raw_line_length / bytespp) - width
*/
    unsigned int padding_offset = (raw_line_length / bytespp) - fb->width;
    if (padding_offset) {
        unsigned char *data;
        unsigned char *pdata;
        unsigned char *praw;
        const unsigned char *data_buffer_end;
        const unsigned char *raw_buffer_end;

        // container for final aligned image data
        data = malloc(fb->size);
        if (!data) {
            D("data: memory error");
            goto oops;
        }

        pdata = data;
        praw = raw;
        data_buffer_end = data + fb->size;
        raw_buffer_end = raw + raw_size;

        // Add a margin to prevent buffer overflow during copy
        data_buffer_end -= bytespp * fb->width;
        raw_buffer_end -= raw_line_length;
        while (praw < raw_buffer_end && pdata < data_buffer_end) {
            memcpy(pdata, praw, bytespp * fb->width);
            pdata += bytespp * fb->width;
            praw += raw_line_length;
        }
        D("Padding done.");

        fb->data = data;
        free(raw);
    } else {
        fb->data = raw;
    }

    close(fd);
    return 0;

oops:
    free(raw);
    close(fd);
    return -1;
}

int fb2png(const char *path)
{
    struct fb fb;
    int ret;

#ifdef ANDROID
    ret = get_device_fb("/dev/graphics/fb0", &fb);
#else
    ret = get_device_fb("/dev/fb0", &fb);
#endif

    if (ret) {
        D("Failed to read framebuffer.");
        return -1;
    }

    return fb_save_png(&fb, path);
}
