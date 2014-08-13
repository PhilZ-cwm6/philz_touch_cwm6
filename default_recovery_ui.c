/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include <linux/input.h>

#include "recovery_ui.h"
#include "common.h"
#include "extendedcommands.h"
#include "advanced_functions.h"
#include "recovery_settings.h"

char* MENU_HEADERS[] = { NULL };

char* MENU_ITEMS[] = {
    "Reboot System Now",
    "Install Zip",
    "Wipe and Format Options",
    "Backup and Restore",
    "Mounts and Storage",
    "Advanced Functions",
    "Recovery Settings",
    "Power Options",
    NULL
};

void device_ui_init(UIParameters* ui_parameters) {
}

//add here what we want to run on recovery start, even for temporary recovery
int device_recovery_start() {
    refresh_recovery_settings(1);
    return 0;
}

// add here any key combo check to reboot device
int device_reboot_now(volatile char* key_pressed, int key_code) {
    return 0;
}

int device_perform_action(int which) {
    return which;
}

int device_wipe_data() {
    return 0;
}
