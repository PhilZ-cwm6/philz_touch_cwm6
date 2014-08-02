/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include "minui/minui.h"
#include "cutils/properties.h"
#include "install.h"
#include "common.h"
#include "recovery_ui.h"
#include "adb_install.h"
#include "minadbd/adb.h"

static pthread_t sideload_thread;

static void
set_usb_driver(bool enabled) {
    int fd = open("/sys/class/android_usb/android0/enable", O_WRONLY);
    if (fd < 0) {
        ui_print("failed to open driver control: %s\n", strerror(errno));
        return;
    }
    if (write(fd, enabled ? "1" : "0", 1) < 0) {
        ui_print("failed to set driver control: %s\n", strerror(errno));
    }
    if (close(fd) < 0) {
        ui_print("failed to close driver control: %s\n", strerror(errno));
    }
}

static void
stop_adbd() {
    property_set("ctl.stop", "adbd");
    set_usb_driver(false);
}


static void
maybe_restart_adbd() {
    char value[PROPERTY_VALUE_MAX+1];
    int len = property_get("ro.debuggable", value, NULL);
    if (len == 1 && value[0] == '1') {
        ui_print("Restarting adbd...\n");
        set_usb_driver(true);
        property_set("ctl.start", "adbd");
    }
}

struct sideload_waiter_data {
    pid_t child;
};

static struct sideload_waiter_data waiter;

void *adb_sideload_thread(void* v) {
    struct sideload_waiter_data* data = (struct sideload_waiter_data*)v;

    int status;
    waitpid(data->child, &status, 0);
    LOGI("sideload process finished\n");

    ui_cancel_wait_key();

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LOGI("status %d\n", WEXITSTATUS(status));
    }

    LOGI("sideload thread finished\n");
    return NULL;
}

void
start_sideload() {
    stop_adbd();
    set_usb_driver(true);

    ui_print("\n\nNow send the package you want to apply\n"
              "to the device with \"adb sideload <filename>\"...\n");

    if ((waiter.child = fork()) == 0) {
        execl("/sbin/recovery", "recovery", "--adbd", NULL);
        _exit(-1);
    }

    pthread_create(&sideload_thread, NULL, &adb_sideload_thread, &waiter);
}

int
apply_from_adb(int* wipe_cache, const char* install_file) {

    set_usb_driver(false);
    maybe_restart_adbd();

    // kill the child
    kill(waiter.child, SIGTERM);
    pthread_join(sideload_thread, NULL);
    ui_clear_key_queue();

    struct stat st;
    if (stat(ADB_SIDELOAD_FILENAME, &st) != 0) {
        if (errno == ENOENT) {
            ui_print("No package received.\n");
            return INSTALL_NONE; // Go Back / Cancel
        }
        ui_print("Error reading package:\n  %s\n", strerror(errno));
        return INSTALL_ERROR;
    }

    int status = install_package(ADB_SIDELOAD_FILENAME, wipe_cache, install_file);
    ui_reset_progress(); // install_package will set indeterminate progress

    remove(ADB_SIDELOAD_FILENAME);

    return status;
}
