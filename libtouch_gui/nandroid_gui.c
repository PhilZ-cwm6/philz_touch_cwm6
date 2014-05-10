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

/****************************************/
/*   Start Nandroid touch functions     */
/****************************************/

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

#include <signal.h>
#include <sys/wait.h>

#include "libcrecovery/common.h"

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include <sys/vfs.h>
#include "cutils/android_reboot.h"

#include "extendedcommands.h"
#include "advanced_functions.h"
#include "recovery_settings.h"
#include "nandroid.h"
#include "mounts.h"

#include "flashutils/flashutils.h"
#include <libgen.h>

#include "libtouch_gui/touch_gui.h"
#include "libtouch_gui/nandroid_gui.h"

// timer reset on each touch screen event (dim screen during nandroid jobs)
// it will be initialized on a backup/restore job
long long last_key_ev;

// checks to see if user cancel action during nandroid job
// also dims screen during nandroid job if no key action
int user_cancel_nandroid(FILE **fp, const char* backup_file_image, int is_backup, int *nand_starts) {
    if (*nand_starts) {
        // initialize settings
        ui_clear_key_queue();
        ui_print("\nPress Back to cancel.\n");
        *nand_starts = 0;
    }

    int key_event = key_press_event();
    if (key_event != NO_ACTION) {
        // a key press event was detected: reset dim timeout on touch
        last_key_ev = timenow_msec();

        // wake-up screen brightness on key event
        if (is_dimmed)
            ui_dim_screen(0);

        // support cancel nandroid job
        if (key_event == GO_BACK) {
            // print last 1 log rows in cyan blue
            int color[] = {CYAN_BLUE_CODE};
            ui_print_color(1, color);

            ui_print("Really cancel? (press Back)\n");
            is_time_interval_passed(0);
            ui_clear_key_queue();
            while (!is_time_interval_passed(5000)) {
                key_event = key_press_event();
                if (key_event != NO_ACTION)
                    break;
                continue;
            }

            if (key_event != GO_BACK) {
                ui_delete_line(1);
                return 0;
            }

            ui_print("Cancelling, please wait...\n");
            ui_clear_key_queue();
            __pclose(*fp);
            if (is_backup) {
                ui_print("Deleting backup...\n");
                char cmd[PATH_MAX];
                sync(); // before deleting backup folder
                sprintf(cmd, "rm -rf '%s'", DirName(backup_file_image));
                __system(cmd);
            }

            finish_nandroid_job(); // will also do a sync() and some ui_prints
            if (!is_backup) {
                // heading \n to not bother with text spanning on one or two lines, depending on device res
                ui_print_color(2, color);
                ui_print("\nPartition was left corrupted after cancel command!\n");
            }

            ui_print_color(0, 0);
            return 1;
        }
    } else if (!is_dimmed && dim_timeout.value != 0 && (timenow_msec() - last_key_ev) / 1000 >= dim_timeout.value) {
        // dim screen on timeout
        ui_dim_screen(1);
    }

    return 0;
}
//-------- End Nandroid touch functions
