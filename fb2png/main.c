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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "fb2png.h"

#ifdef ANDROID
    #define DEFAULT_SAVE_PATH "/data/local/fbdump.png"
#else
    #define DEFAULT_SAVE_PATH "fbdump.png"
#endif


void print_version() {
    printf(
        "\n"
        "Android Screenshooter - fb2png\n"
        "Author: Kyan He <kyan.ql.he@gmail.com>\n"
        "Maintained by Phil3759 & McKael @xda\n"
        "v2.0.0 <2014> \n"
        "\n"
    );
}

int print_usage() {
    print_version();
    printf(
        "Usage: fb2png [-option=][path/to/output.png]\n"
        "   The default output path is /data/local/fbdump.png\n"
        "Options: \n"
        "   -buffer=n  0:single 1:double... buffering (default=auto detect)\n"
        "\n"
    );

    return 0;
}

int parse_options(const char* option) {
    char buffer_opt[] = "-buffer=";
    int found_option = 0;
    long int value;

    if (strncmp(option, buffer_opt, strlen(buffer_opt)) == 0) {
        value = atoi(option + strlen(buffer_opt));
        if (value >= 0 && value <= MAX_ALLOWED_FB_BUFFERS) {
            user_set_buffers_num = (int)value;
            found_option = 1;
        } else {
            printf("invalid buffer option (%ld)\n", value);
            found_option = -1;
        }
    } else if (strcmp(option, "-help") == 0 || strcmp(option, "--help") == 0 || strcmp(option, "-h") == 0) {
        // print help and exit
        found_option = -1;
    }

    return found_option;
}

int main(int argc, char **argv)
{
    char* path = NULL;
    int ret = 0;
    int i = 1;

    while (i < argc && path == NULL) {
        ret = parse_options(argv[i]);
        if (ret == 0)
            path = argv[i];
        else if (ret < 0)
            return print_usage();
        ++i;
    }

    // all options must come before path
    if (i != argc)
        return print_usage();

    if (path == NULL)
        path = DEFAULT_SAVE_PATH;

    if (strlen(path) >= PATH_MAX) {
        printf("Output path too long!\n");
        return -1;
    }

    print_version();
    printf("%s -buffer=%d%s %s\n",
            argv[0],
            user_set_buffers_num,
            user_set_buffers_num < 0 ? " (auto)" : "",
            path
    );

    if (0 == (ret = fb2png(path)))
        printf("Image saved to %s\n", path);

    return ret;
}
