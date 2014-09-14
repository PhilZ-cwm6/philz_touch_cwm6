/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "cutils/properties.h"
#include "cutils/android_reboot.h"

#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "voldclient/voldclient.h"
#include "libcrecovery/common.h"
#include "minadbd/adb.h"
#include "bootloader.h"
#include "common.h"
#include "install.h"
#include "roots.h"
#include "recovery_ui.h"
#include "adb_install.h"
#include "recovery_cmds.h"
#include "edifyscripting.h"
#include "extendedcommands.h"
#include "recovery_settings.h"
#include "advanced_functions.h"

struct selabel_handle *sehandle;

#ifdef PHILZ_TOUCH_RECOVERY
#include "libtouch_gui/gui_settings.h"
#endif

static const struct option OPTIONS[] = {
  { "send_intent", required_argument, NULL, 's' },
  { "update_package", required_argument, NULL, 'u' },
  { "wipe_data", no_argument, NULL, 'w' },
  { "wipe_cache", no_argument, NULL, 'c' },
  { "wipe_media", no_argument, NULL, 'm' },
  { "show_text", no_argument, NULL, 't' },
  { "just_exit", no_argument, NULL, 'x' },
  { "sideload", no_argument, NULL, 'a' },
  { "shutdown_after", no_argument, NULL, 'p' },
  { "stages", required_argument, NULL, 'g' },
  { NULL, 0, NULL, 0 },
};

#define LAST_LOG_FILE "/cache/recovery/last_log"

static const char *CACHE_LOG_DIR = "/cache/recovery";
static const char *COMMAND_FILE = "/cache/recovery/command";
static const char *INTENT_FILE = "/cache/recovery/intent";
static const char *LOG_FILE = "/cache/recovery/log";
static const char *LAST_INSTALL_FILE = "/cache/recovery/last_install";
static const char *CACHE_ROOT = "/cache";
static const char *TEMPORARY_LOG_FILE = "/tmp/recovery.log";
static const char *TEMPORARY_INSTALL_FILE = "/tmp/last_install";
static const char *SIDELOAD_TEMP_DIR = "/tmp/sideload";

char* stage = NULL;

extern UIParameters ui_parameters;    // from ui.c

/*
 * The recovery tool communicates with the main system through /cache files.
 *   /cache/recovery/command - INPUT - command line for tool, one arg per line
 *   /cache/recovery/log - OUTPUT - combined log file from recovery run(s)
 *   /cache/recovery/intent - OUTPUT - intent that was passed in
 *
 * The arguments which may be supplied in the recovery.command file:
 *   --send_intent=anystring - write the text out to recovery.intent
 *   --update_package=path - verify install an OTA package file
 *   --wipe_data - erase user data (and cache), then reboot
 *   --wipe_cache - wipe cache (but not user data), then reboot
 *   --set_encrypted_filesystem=on|off - enables / diasables encrypted fs
 *   --just_exit - do nothing; exit and reboot
 *   --sideload - enter sideload mode
 *
 * After completing, we remove /cache/recovery/command and reboot.
 * Arguments may also be supplied in the bootloader control block (BCB).
 * These important scenarios must be safely restartable at any point:
 *
 * FACTORY RESET
 * 1. user selects "factory reset"
 * 2. main system writes "--wipe_data" to /cache/recovery/command
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--wipe_data"
 *    -- after this, rebooting will restart the erase --
 * 5. erase_volume() reformats /data
 * 6. erase_volume() reformats /cache
 * 7. finish_recovery() erases BCB
 *    -- after this, rebooting will restart the main system --
 * 8. main() calls reboot() to boot main system
 *
 * OTA INSTALL
 * 1. main system downloads OTA package to /cache/some-filename.zip
 * 2. main system writes "--update_package=/cache/some-filename.zip"
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--update_package=..."
 *    -- after this, rebooting will attempt to reinstall the update --
 * 5. install_package() attempts to install the update
 *    NOTE: the package install must itself be restartable from any point
 * 6. finish_recovery() erases BCB
 *    -- after this, rebooting will (try to) restart the main system --
 * 7. ** if install failed **
 *    7a. prompt_and_wait() shows an error icon and waits for the user
 *    7b; the user reboots (pulling the battery, etc) into the main system
 * 8. main() calls maybe_install_firmware_update()
 *    ** if the update contained radio/hboot firmware **:
 *    8a. m_i_f_u() writes BCB with "boot-recovery" and "--wipe_cache"
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8b. m_i_f_u() writes firmware image into raw cache partition
 *    8c. m_i_f_u() writes BCB with "update-radio/hboot" and "--wipe_cache"
 *        -- after this, rebooting will attempt to reinstall firmware --
 *    8d. bootloader tries to flash firmware
 *    8e. bootloader writes BCB with "boot-recovery" (keeping "--wipe_cache")
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8f. erase_volume() reformats /cache
 *    8g. finish_recovery() erases BCB
 *        -- after this, rebooting will (try to) restart the main system --
 * 9. main() calls reboot() to boot main system
 */

static const int MAX_ARG_LENGTH = 4096;
static const int MAX_ARGS = 100;

// open a given path, mounting partitions as necessary
FILE*
fopen_path(const char *path, const char *mode) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return NULL;
    }

    // When writing, try to create the containing directory, if necessary.
    // Use generous permissions, the system (init.rc) will reset them.
    if (strchr("wa", mode[0])) dirCreateHierarchy(path, 0777, NULL, 1, sehandle);

    FILE *fp = fopen(path, mode);
    return fp;
}

// close a file, log an error if the error indicator is set
static void
check_and_fclose(FILE *fp, const char *name) {
    fflush(fp);
    if (ferror(fp)) LOGE("Error in %s\n(%s)\n", name, strerror(errno));
    fclose(fp);
}

// command line args come from, in decreasing precedence:
//   - the actual command line
//   - the bootloader control block (one per line, after "recovery")
//   - the contents of COMMAND_FILE (one per line)
static void
get_args(int *argc, char ***argv) {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    get_bootloader_message(&boot);  // this may fail, leaving a zeroed structure
    stage = strndup(boot.stage, sizeof(boot.stage));

    if (boot.command[0] != 0 && boot.command[0] != 255) {
        LOGI("Boot command: %.*s\n", (int)sizeof(boot.command), boot.command);
    }

    if (boot.status[0] != 0 && boot.status[0] != 255) {
        LOGI("Boot status: %.*s\n", (int)sizeof(boot.status), boot.status);
    }

    // --- if arguments weren't supplied, look in the bootloader control block
    if (*argc <= 1 && !file_found("/tmp/.ignorebootmessage")) {
        boot.recovery[sizeof(boot.recovery) - 1] = '\0';  // Ensure termination
        const char *arg = strtok(boot.recovery, "\n");
        if (arg != NULL && !strcmp(arg, "recovery")) {
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = strdup(arg);
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if ((arg = strtok(NULL, "\n")) == NULL) break;
                (*argv)[*argc] = strdup(arg);
            }
            LOGI("Got arguments from boot message\n");
        } else if (boot.recovery[0] != 0 && boot.recovery[0] != 255) {
            LOGE("Bad boot message\n\"%.20s\"\n", boot.recovery);
        }
    }

    // --- if that doesn't work, try the command file
    if (*argc <= 1) {
        FILE *fp = fopen_path(COMMAND_FILE, "r");
        if (fp != NULL) {
            char *token;
            char *argv0 = (*argv)[0];
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = argv0;  // use the same program name

            char buf[MAX_ARG_LENGTH];
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if (!fgets(buf, sizeof(buf), fp)) break;
                token = strtok(buf, "\r\n");
                if (token != NULL) {
                    (*argv)[*argc] = strdup(token);  // Strip newline.
                } else {
                    --*argc;
                }
            }

            check_and_fclose(fp, COMMAND_FILE);
            LOGI("Got arguments from %s\n", COMMAND_FILE);
        }
    }

    // --> write the arguments we have back into the bootloader control block
    // always boot into recovery after this (until finish_recovery() is called)
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    int i;
    for (i = 1; i < *argc; ++i) {
        strlcat(boot.recovery, (*argv)[i], sizeof(boot.recovery));
        strlcat(boot.recovery, "\n", sizeof(boot.recovery));
    }
    set_bootloader_message(&boot);
}

static void
set_sdcard_update_bootloader_message() {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    set_bootloader_message(&boot);
}

// How much of the temp log we have copied to the copy in cache.
static long tmplog_offset = 0;

static void
copy_log_file(const char* source, const char* destination, int append) {
    FILE *log = fopen_path(destination, append ? "a" : "w");
    if (log == NULL) {
        LOGE("Can't open %s\n", destination);
    } else {
        FILE *tmplog = fopen(source, "r");
        if (tmplog != NULL) {
            if (append) {
                fseek(tmplog, tmplog_offset, SEEK_SET);  // Since last write
            }
            char buf[4096];
            while (fgets(buf, sizeof(buf), tmplog)) fputs(buf, log);
            if (append) {
                tmplog_offset = ftell(tmplog);
            }
            check_and_fclose(tmplog, source);
        }
        check_and_fclose(log, destination);
    }
}

// Rename last_log -> last_log.1 -> last_log.2 -> ... -> last_log.$max
// Overwrites any existing last_log.$max.
static void
rotate_last_logs(int max) {
    char oldfn[256];
    char newfn[256];

    int i;
    for (i = max-1; i >= 0; --i) {
        snprintf(oldfn, sizeof(oldfn), (i==0) ? LAST_LOG_FILE : (LAST_LOG_FILE ".%d"), i);
        snprintf(newfn, sizeof(newfn), LAST_LOG_FILE ".%d", i+1);
        // ignore errors
        rename(oldfn, newfn);
    }
}

static void
copy_logs() {
    // Copy logs to cache so the system can find out what happened.
    copy_log_file(TEMPORARY_LOG_FILE, LOG_FILE, true);
    copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
    copy_log_file(TEMPORARY_INSTALL_FILE, LAST_INSTALL_FILE, false);
    chmod(LOG_FILE, 0600);
    chown(LOG_FILE, 1000, 1000);   // system user
    chmod(LAST_LOG_FILE, 0640);
    chmod(LAST_INSTALL_FILE, 0644);
    sync();
}

// clear the recovery command and prepare to boot a (hopefully working) system,
// copy our log file to cache as well (for the system to read), and
// record any intent we were asked to communicate back to the system.
// this function is idempotent: call it as many times as you like.
static void
finish_recovery(const char *send_intent) {
    // By this point, we're ready to return to the main system...
    if (send_intent != NULL) {
        FILE *fp = fopen_path(INTENT_FILE, "w");
        if (fp == NULL) {
            LOGE("Can't open %s\n", INTENT_FILE);
        } else {
            fputs(send_intent, fp);
            check_and_fclose(fp, INTENT_FILE);
        }
    }

    copy_logs();

    // Reset to normal system boot so recovery won't cycle indefinitely.
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    set_bootloader_message(&boot);

    // Remove the command file, so recovery won't repeat indefinitely.
    if (ensure_path_mounted(COMMAND_FILE) != 0 ||
        (unlink(COMMAND_FILE) && errno != ENOENT)) {
        LOGW("Can't unlink %s\n", COMMAND_FILE);
    }

    ensure_path_unmounted(CACHE_ROOT);
    sync();  // For good measure.
}

typedef struct _saved_log_file {
    char* name;
    struct stat st;
    unsigned char* data;
    struct _saved_log_file* next;
} saved_log_file;

// remove static to be able to call it from ors menu
int
erase_volume(const char *volume) {
    bool is_cache = (strcmp(volume, CACHE_ROOT) == 0);

    int icon = ui_get_background_icon();
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();

    saved_log_file* head = NULL;

    if (is_cache) {
        // If we're reformatting /cache, we load any
        // "/cache/recovery/last*" files into memory, so we can restore
        // them after the reformat.

        ensure_path_mounted(volume);

        DIR* d;
        struct dirent* de;
        d = opendir(CACHE_LOG_DIR);
        if (d) {
            char path[PATH_MAX];
            strcpy(path, CACHE_LOG_DIR);
            strcat(path, "/");
            int path_len = strlen(path);
            while ((de = readdir(d)) != NULL) {
                if (strncmp(de->d_name, "last", 4) == 0) {
                    saved_log_file* p = (saved_log_file*) malloc(sizeof(saved_log_file));
                    strcpy(path+path_len, de->d_name);
                    p->name = strdup(path);
                    if (stat(path, &(p->st)) == 0) {
                        // truncate files to 512kb
                        if (p->st.st_size > (1 << 19)) {
                            p->st.st_size = 1 << 19;
                        }
                        p->data = (unsigned char*) malloc(p->st.st_size);
                        FILE* f = fopen(path, "rb");
                        fread(p->data, 1, p->st.st_size, f);
                        fclose(f);
                        p->next = head;
                        head = p;
                    } else {
                        free(p);
                    }
                }
            }
            closedir(d);
        } else {
            if (errno != ENOENT) {
                printf("opendir failed: %s\n", strerror(errno));
            }
        }
    }

    ui_print("Formatting %s...\n", volume);

    ensure_path_unmounted(volume);
    int result = format_volume(volume);

    if (is_cache) {
        while (head) {
            FILE* f = fopen_path(head->name, "wb");
            if (f) {
                fwrite(head->data, 1, head->st.st_size, f);
                fclose(f);
                chmod(head->name, head->st.st_mode);
                chown(head->name, head->st.st_uid, head->st.st_gid);
            }
            free(head->name);
            free(head->data);
            saved_log_file* temp = head->next;
            free(head);
            head = temp;
        }

        // Any part of the log we'd copied to cache is now gone.
        // Reset the pointer so we copy from the beginning of the temp
        // log.
        tmplog_offset = 0;
        copy_logs();
    }

    ui_set_background(icon);
    ui_reset_progress();
    return result;
}

static char*
copy_sideloaded_package(const char* original_path) {
  if (ensure_path_mounted(original_path) != 0) {
    LOGE("Can't mount %s\n", original_path);
    return NULL;
  }

  if (ensure_path_mounted(SIDELOAD_TEMP_DIR) != 0) {
    LOGE("Can't mount %s\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }

  if (mkdir(SIDELOAD_TEMP_DIR, 0700) != 0) {
    if (errno != EEXIST) {
      LOGE("Can't mkdir %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
      return NULL;
    }
  }

  // verify that SIDELOAD_TEMP_DIR is exactly what we expect: a
  // directory, owned by root, readable and writable only by root.
  struct stat st;
  if (stat(SIDELOAD_TEMP_DIR, &st) != 0) {
    LOGE("failed to stat %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
    return NULL;
  }
  if (!S_ISDIR(st.st_mode)) {
    LOGE("%s isn't a directory\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }
  if ((st.st_mode & 0777) != 0700) {
    LOGE("%s has perms %o\n", SIDELOAD_TEMP_DIR, st.st_mode);
    return NULL;
  }
  if (st.st_uid != 0) {
    LOGE("%s owned by %lu; not root\n", SIDELOAD_TEMP_DIR, st.st_uid);
    return NULL;
  }

  char copy_path[PATH_MAX];
  strcpy(copy_path, SIDELOAD_TEMP_DIR);
  strcat(copy_path, "/package.zip");

  char* buffer = (char*)malloc(BUFSIZ);
  if (buffer == NULL) {
    LOGE("Failed to allocate buffer\n");
    return NULL;
  }

  size_t read;
  FILE* fin = fopen(original_path, "rb");
  if (fin == NULL) {
    LOGE("Failed to open %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }
  FILE* fout = fopen(copy_path, "wb");
  if (fout == NULL) {
    LOGE("Failed to open %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  while ((read = fread(buffer, 1, BUFSIZ, fin)) > 0) {
    if (fwrite(buffer, 1, read, fout) != read) {
      LOGE("Short write of %s (%s)\n", copy_path, strerror(errno));
      return NULL;
    }
  }

  free(buffer);

  if (fclose(fout) != 0) {
    LOGE("Failed to close %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  if (fclose(fin) != 0) {
    LOGE("Failed to close %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }

  // "adb push" is happy to overwrite read-only files when it's
  // running as root, but we'll try anyway.
  if (chmod(copy_path, 0400) != 0) {
    LOGE("Failed to chmod %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  return strdup(copy_path);
}

static const char**
prepend_title(const char** headers) {
    const char* title[] = { EXPAND(RECOVERY_MOD_VERSION),
                      NULL };

    // count the number of lines in our title, plus the
    // caller-provided headers.
    int count = 0;
    const char** p;
    for (p = title; *p; ++p, ++count);
    for (p = headers; *p; ++p, ++count);

    const char** new_headers = malloc((count+1) * sizeof(const char*));
    const char** h = new_headers;
    for (p = title; *p; ++p, ++h) *h = *p;
    for (p = headers; *p; ++p, ++h) *h = *p;
    *h = NULL;

    return new_headers;
}

int
get_menu_selection(const char** headers, char** items, int menu_only,
                   int initial_selection) {
    // throw away keys pressed previously, so user doesn't
    // accidentally trigger menu items.
    ui_clear_key_queue();

    int item_count = ui_start_menu(headers, items, initial_selection);
    int selected = initial_selection;
    int chosen_item = -1; // NO_ACTION
#ifdef NOT_ENOUGH_RAINBOWS
    int wrap_count = 0;
#endif

    while (chosen_item < 0 && chosen_item != GO_BACK) {
        int key = ui_wait_key();
        int visible = ui_IsTextVisible();

        if (key == -1) {   // ui_wait_key() timed out, always reboot to main system
            LOGI("timed out waiting for key input; rebooting.\n");
            ui_end_menu();
            reboot_main_system(ANDROID_RB_RESTART, 0, 0);
            sleep(5);
            LOGE("Failed to reboot system on timed out key input!!\n");
            return GO_BACK;
        }
        else if (key == -2) {   // we are returning from ui_cancel_wait_key(): trigger a GO_BACK
            return GO_BACK;
        }
        else if (key == -3) {   // an USB device was plugged in (returning from ui_wait_key())
            return REFRESH;
        }

        int action = ui_handle_key(key, visible);

        int old_selected = selected;
        selected = ui_get_selected_item();

        if (action < 0) {
            switch (action) {
                case HIGHLIGHT_UP:
                    --selected;
                    selected = ui_menu_select(selected);
                    break;
                case HIGHLIGHT_DOWN:
                    ++selected;
                    selected = ui_menu_select(selected);
                    break;
#ifdef PHILZ_TOUCH_RECOVERY
                case HIGHLIGHT_ON_TOUCH:
                    selected = ui_menu_touch_select();
                    break;
#endif
                case SELECT_ITEM:
                    chosen_item = selected;
                    if (ui_is_showing_back_button()) {
                        if (chosen_item == item_count) {
                            chosen_item = GO_BACK;
                        }
                    }
                    break;
                case NO_ACTION:
                    break;
                case GO_BACK:
                    chosen_item = GO_BACK;
                    break;
#ifdef PHILZ_TOUCH_RECOVERY
                case GESTURE_ACTIONS:
                    handle_gesture_actions(headers, items, initial_selection);
                    break;
#endif
            }
        } else if (!menu_only) {
            chosen_item = action;
        }
#ifdef NOT_ENOUGH_RAINBOWS
        if (abs(selected - old_selected) > 1) {
            wrap_count++;
            if (wrap_count == 5) {
                wrap_count = 0;
                if (ui_get_rainbow_mode()) {
                    ui_set_rainbow_mode(0);
                    ui_print("Rainbow mode disabled\n");
                }
                else {
                    ui_set_rainbow_mode(1);
                    ui_print("Rainbow mode enabled!\n");
                }
            }
        }
#endif
    }

    ui_end_menu();
    ui_clear_key_queue();
    return chosen_item;
}

static int compare_string(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

// legacy unused: we use gather_files() and choose_file_menu()
static int
update_directory(const char* path, const char* unmount_when_done,
                 int* wipe_cache) {
    ensure_path_mounted(path);

    const char* MENU_HEADERS[] = { "Choose a package to install:",
                                   path,
                                   "",
                                   NULL };
    DIR* d;
    struct dirent* de;
    d = opendir(path);
    if (d == NULL) {
        LOGE("error opening %s: %s\n", path, strerror(errno));
        if (unmount_when_done != NULL) {
            ensure_path_unmounted(unmount_when_done);
        }
        return 0;
    }

    const char** headers = prepend_title(MENU_HEADERS);

    int d_size = 0;
    int d_alloc = 10;
    char** dirs = (char**)malloc(d_alloc * sizeof(char*));
    int z_size = 1;
    int z_alloc = 10;
    char** zips = (char**)malloc(z_alloc * sizeof(char*));
    zips[0] = strdup("../");

    while ((de = readdir(d)) != NULL) {
        int name_len = strlen(de->d_name);

        if (de->d_type == DT_DIR) {
            // skip "." and ".." entries
            if (name_len == 1 && de->d_name[0] == '.') continue;
            if (name_len == 2 && de->d_name[0] == '.' &&
                de->d_name[1] == '.') continue;

            if (d_size >= d_alloc) {
                d_alloc *= 2;
                dirs = (char**)realloc(dirs, d_alloc * sizeof(char*));
            }
            dirs[d_size] = (char*)malloc(name_len + 2);
            strcpy(dirs[d_size], de->d_name);
            dirs[d_size][name_len] = '/';
            dirs[d_size][name_len+1] = '\0';
            ++d_size;
        } else if (de->d_type == DT_REG &&
                   name_len >= 4 &&
                   strncasecmp(de->d_name + (name_len-4), ".zip", 4) == 0) {
            if (z_size >= z_alloc) {
                z_alloc *= 2;
                zips = (char**)realloc(zips, z_alloc * sizeof(char*));
            }
            zips[z_size++] = strdup(de->d_name);
        }
    }
    closedir(d);

    qsort(dirs, d_size, sizeof(char*), compare_string);
    qsort(zips, z_size, sizeof(char*), compare_string);

    // append dirs to the zips list
    if (d_size + z_size + 1 > z_alloc) {
        z_alloc = d_size + z_size + 1;
        zips = (char**)realloc(zips, z_alloc * sizeof(char*));
    }
    memcpy(zips + z_size, dirs, d_size * sizeof(char*));
    free(dirs);
    z_size += d_size;
    zips[z_size] = NULL;

    int result;
    int chosen_item = 0;
    do {
        chosen_item = get_menu_selection(headers, zips, 1, chosen_item);
        if (chosen_item < 0) // GO_BACK / REFRESH
            chosen_item = 0;

        char* item = zips[chosen_item];
        int item_len = strlen(item);
        if (chosen_item == 0) {          // item 0 is always "../"
            // go up but continue browsing (if the caller is update_directory)
            result = -1;
            break;
        } else if (item[item_len-1] == '/') {
            // recurse down into a subdirectory
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);
            new_path[strlen(new_path)-1] = '\0';  // truncate the trailing '/'
            result = update_directory(new_path, unmount_when_done, wipe_cache);
            if (result >= 0) break;
        } else {
            // selected a zip file:  attempt to install it, and return
            // the status to the caller.
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);

            ui_print("\n-- Install %s ...\n", path);
            set_sdcard_update_bootloader_message();
            char* copy = copy_sideloaded_package(new_path);
            if (unmount_when_done != NULL) {
                ensure_path_unmounted(unmount_when_done);
            }
            if (copy) {
                result = install_package(copy, wipe_cache, TEMPORARY_INSTALL_FILE);
                free(copy);
            } else {
                result = INSTALL_ERROR;
            }
            break;
        }
    } while (true);

    int i;
    for (i = 0; i < z_size; ++i) free(zips[i]);
    free(zips);
    free(headers);

    if (unmount_when_done != NULL) {
        ensure_path_unmounted(unmount_when_done);
    }
    return result;
}

int install_zip(const char* packagefilepath) {
    ui_print("\n-- Installing: %s\n", packagefilepath);
    set_sdcard_update_bootloader_message();

    // will ensure_path_mounted(packagefilepath)
    // will also set background icon to installing and indeterminate progress bar
    int wipe_cache = 0;
    int status = install_package(packagefilepath, &wipe_cache, TEMPORARY_INSTALL_FILE);
    ui_reset_progress();
    if (status != INSTALL_SUCCESS) {
        copy_logs();
        ui_set_background(BACKGROUND_ICON_ERROR);
        LOGE("Installation aborted.\n");
        return 1;
    } else if (wipe_cache && erase_volume("/cache")) {
        LOGE("Cache wipe (requested by package) failed.\n");
    }

#ifdef PHILZ_TOUCH_RECOVERY
    if (show_background_icon.value)
        ui_set_background(BACKGROUND_ICON_CLOCKWORK);
    else
#endif
        ui_set_background(BACKGROUND_ICON_NONE);

    ui_print("\nInstall from sdcard complete.\n");
    return 0;
}

// remove static to be able to call it from ors menu
void
wipe_data(int confirm) {
    const char* headers[] = {
        "Wipe all user data ?",
        "   data | cache | datadata",
        "   sd-ext| android_secure",
        "",
        NULL
    };

    if (confirm && !confirm_with_headers(headers, "Yes - Wipe all user data")) {
        return;
    }

    ui_print("\n-- Wiping data...\n");
    device_wipe_data();
    erase_volume("/data");
    erase_volume("/cache");
    if (has_datadata()) {
        erase_volume("/datadata");
    }

    erase_volume("/sd-ext");

    // erase .android_secure from /sdcard or /storage/sdcard0
    erase_volume(get_android_secure_path());

    // erase .android_secure from any secondary storage
    char buf[80];
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();
    int i;
    if (extra_paths != NULL) {
        for(i = 0; i < num_extra_volumes; i++) {
            sprintf(buf, "%s/.android_secure", extra_paths[i]);
            erase_volume(buf);
        }
        free_string_array(extra_paths);
    }
    ui_print("Data wipe complete.\n");
}

int enter_sideload_mode(int status) {

    ensure_path_mounted(CACHE_ROOT);
    start_sideload();

    static const char* headers[] = {  "ADB Sideload",
                                "",
                                NULL
    };

    static char* list[] = { "Cancel sideload", NULL };
    int icon = ui_get_background_icon();
    int wipe_cache = 0;

    // we need show_text to show adb sideload cancel menu (get_menu_selection())
    bool text_visible = ui_IsTextVisible();
    ui_SetShowText(true);
    get_menu_selection(headers, list, 0, 0);
    ui_SetShowText(text_visible);
    int ret = apply_from_adb(&wipe_cache, TEMPORARY_INSTALL_FILE);

    // if item < 0 (cancel), apply_from_adb() will return INSTALL_NONE with appropriate log message
    if (ret != INSTALL_NONE) {
        status = ret;
        if (status != INSTALL_SUCCESS) {
            ui_set_background(BACKGROUND_ICON_ERROR);
            ui_print("Installation aborted.\n");
        } else {
            if (wipe_cache && erase_volume("/cache")) {
                LOGE("Cache wipe (requested by package) failed.\n");
            }
            if (ui_IsTextVisible()) {
                ui_set_background(icon);
                ui_print("\nInstall from ADB complete.\n");
            }
        }
    }
    return status;
}

static int
show_apply_update_menu() {
    /* legacy - update later */
    return 0;
}

int ui_root_menu = 0;

static void
prompt_and_wait(int status) {
    const char** headers = prepend_title((const char**)MENU_HEADERS);

    switch (status) {
        case INSTALL_SUCCESS:
        case INSTALL_NONE:
#ifndef PHILZ_TOUCH_RECOVERY
            ui_set_background(BACKGROUND_ICON_CLOCKWORK);
#endif
            break;

        case INSTALL_ERROR:
        case INSTALL_CORRUPT:
            ui_set_background(BACKGROUND_ICON_ERROR);
            break;
    }

    for (;;) {
        finish_recovery(NULL);
        ui_root_menu = 1;
        ui_reset_progress();

        int chosen_item = get_menu_selection(headers, MENU_ITEMS, 0, 0);
        ui_root_menu = 0;

        // device-specific code may take some action here.  It may
        // return one of the core actions handled in the switch
        // statement below. (this can alter show_text state!!)
        chosen_item = device_perform_action(chosen_item);

        int ret = 0;

        for (;;) {
            switch (chosen_item) {
                case ITEM_REBOOT:
                    return;

                case ITEM_WIPE_DATA:
                    if (ui_IsTextVisible())
                        wipe_data_menu();
                    else
                        wipe_data(ui_IsTextVisible());
                    if (!ui_IsTextVisible()) return;
                    break;

                case ITEM_WIPE_CACHE:
                    if (ui_IsTextVisible()) {
                        wipe_data_menu();
                    } else {
                        ui_print("\n-- Wiping cache...\n");
                        erase_volume("/cache");
                        ui_print("Cache wipe complete.\n");
                    }
                    if (!ui_IsTextVisible()) return;
                    break;

                case ITEM_APPLY_ZIP:
                    ret = show_install_update_menu();
                    break;

                case ITEM_NANDROID:
                    ret = show_nandroid_menu();
                    break;

                case ITEM_PARTITION:
                    ret = show_partition_mounts_menu();
                    break;

                case ITEM_ADVANCED:
                    show_advanced_menu();
                    break;

                case ITEM_SETTINGS:
                    show_philz_settings_menu();
                    break;

                case ITEM_POWEROFF:
                    show_advanced_power_menu();
                    break;
            }
            if (ret == REFRESH) {
                // this will restart the for() loop and run switch (chosen_item) action forcing to return to previous menu and refresh it
                ret = 0;
                continue;
            }
            break;
        }
    }
}

static void
print_property(const char *key, const char *name, void *cookie) {
    printf("%s=%s\n", key, name);
}

static void
setup_adbd() {
    struct stat f;
    const char *key_src = "/data/misc/adb/adb_keys";
    const char *key_dest = "/adb_keys";

    // Mount /data and copy adb_keys to root if it exists
    ensure_path_mounted("/data");
    if (stat(key_src, &f) == 0) {
        FILE *file_src = fopen(key_src, "r");
        if (file_src == NULL) {
            LOGE("Can't open %s\n", key_src);
        } else {
            FILE *file_dest = fopen(key_dest, "w");
            if (file_dest == NULL) {
                LOGE("Can't open %s\n", key_dest);
            } else {
                char buf[4096];
                while (fgets(buf, sizeof(buf), file_src)) fputs(buf, file_dest);
                check_and_fclose(file_dest, key_dest);

                // Enable secure adbd
                property_set("ro.adb.secure", "1");
            }
            check_and_fclose(file_src, key_src);
        }
    }
    ensure_path_unmounted("/data");

    // Trigger (re)start of adb daemon
    property_set("service.adb.root", "1");
}

// call a clean reboot
void reboot_main_system(int cmd, int flags, char *arg) {
    verify_settings_file();
    write_recovery_version();

    verify_root_and_recovery();

    finish_recovery(NULL); // sync() in here
    vold_unmount_all();

    char buffer[80];
    if ((unsigned)cmd == ANDROID_RB_POWEROFF) {
        strcpy(buffer, "shutdown,");
    } else {
        strcpy(buffer, "reboot,");
    }
    if (arg != NULL) {
        strncat(buffer, arg, sizeof(buffer));
    }
    property_set(ANDROID_RB_PROPERTY, buffer);
    sleep(5);

    // Attempt to reboot using older methods in case the recovery
    // that we are updating does not support init property reboot
    // android_reboot() is defined in libcutils/android_reboot.c
    LOGI("trying legacy android_reboot() command\n");
    android_reboot(cmd, flags, arg);
}

static int v_changed = 0;
int volumes_changed() {
    int ret = v_changed;
    if (v_changed == 1)
        v_changed = 0;
    return ret;
}

static int handle_volume_hotswap(char* label, char* path) {
    v_changed = 1;
    return 0;
}

static int handle_volume_state_changed(char* label, char* path, int state) {
    ui_print("%s: %s\n", path, volume_state_to_string(state));

    return 0;
}

static struct vold_callbacks v_callbacks = {
    .state_changed = handle_volume_state_changed,
    .disk_added = handle_volume_hotswap,
    .disk_removed = handle_volume_hotswap,
};

// used by nandroid cmd commands to support voldmanaged devices
void vold_init() {
    vold_client_start(&v_callbacks, 0);
    vold_set_automount(1);
}

int
main(int argc, char **argv) {
    time_t start = time(NULL);

    // If this binary is started with the single argument "--adbd",
    // instead of being the normal recovery binary, it turns into kind
    // of a stripped-down version of adbd that only supports the
    // 'sideload' command.  Note this must be a real argument, not
    // anything in the command file or bootloader control block; the
    // only way recovery should be run with this argument is when it
    // starts a copy of itself from the apply_from_adb() function.
    if (argc == 2 && strcmp(argv[1], "--adbd") == 0) {
        adb_main();
        return 0;
    }

    // Handle alternative invocations
    char* command = argv[0];
    char* stripped = strrchr(argv[0], '/');
    if (stripped)
        command = stripped + 1;

    if (strcmp(command, "recovery") != 0) {
        struct recovery_cmd cmd = get_command(command);
        if (cmd.name)
            return cmd.main_func(argc, argv);

#ifdef BOARD_RECOVERY_HANDLES_MOUNT
        if (!strcmp(command, "mount") && argc == 2)
        {
            load_volume_table();
            return ensure_path_mounted(argv[1]);
        }
#endif
        if (!strcmp(command, "setup_adbd")) {
            load_volume_table();
            setup_adbd();
            return 0;
        }
        if (strstr(argv[0], "start")) {
            property_set("ctl.start", argv[1]);
            return 0;
        }
        if (strstr(argv[0], "stop")) {
            property_set("ctl.stop", argv[1]);
            return 0;
        }
        return busybox_driver(argc, argv);
    }

    // devices can run specific tasks on recovery start
    __system("/sbin/postrecoveryboot.sh");

    // Clear umask for packages that copy files out to /tmp and then over
    // to /system without properly setting all permissions (eg. gapps).
    umask(0);

    // If these fail, there's not really anywhere to complain...
    freopen(TEMPORARY_LOG_FILE, "a", stdout); setbuf(stdout, NULL);
    freopen(TEMPORARY_LOG_FILE, "a", stderr); setbuf(stderr, NULL);

    printf("Starting recovery on %s", ctime(&start));

    load_volume_table();
    setup_data_media(1);
    vold_client_start(&v_callbacks, 0);
    vold_set_automount(1);
    setup_legacy_storage_paths();
    ensure_path_mounted(LAST_LOG_FILE);
    rotate_last_logs(10);
    get_args(&argc, &argv);

    const char *send_intent = NULL;
    const char *update_package = NULL;
    int wipe_data = 0, wipe_cache = 0, wipe_media = 0, show_text = 0, sideload = 0;
    bool just_exit = false;
    bool shutdown_after = false;

    printf("Checking arguments.\n");
    int arg;
    while ((arg = getopt_long(argc, argv, "", OPTIONS, NULL)) != -1) {
        switch (arg) {
        case 's': send_intent = optarg; break;
        case 'u': update_package = optarg; break;
        case 'w': wipe_data = wipe_cache = 1; break;
        case 'm': wipe_media = 1; break;
        case 'c': wipe_cache = 1; break;
        case 't': show_text = 1; break;
        case 'x': just_exit = true; break;
        case 'a': sideload = 1; break;
        case 'p': shutdown_after = true; break;
        case 'g': {
            if (stage == NULL || *stage == '\0') {
                char buffer[20] = "1/";
                strncat(buffer, optarg, sizeof(buffer)-3);
                stage = strdup(buffer);
            }
            break;
        }
        case '?':
            LOGE("Invalid command argument\n");
            continue;
        }
    }

    printf("stage is [%s]\n", stage);

    device_ui_init(&ui_parameters);
    ui_init();
    ui_print(EXPAND(RECOVERY_MOD_VERSION_BUILD) "\n");
    ui_print("ClockworkMod " EXPAND(CWM_BASE_VERSION) "\n");
    LOGI("Device target: " EXPAND(TARGET_COMMON_NAME) "\n");
#ifdef PHILZ_TOUCH_RECOVERY
    print_libtouch_version(0);
#endif

    int st_cur, st_max;
    if (stage != NULL && sscanf(stage, "%d/%d", &st_cur, &st_max) == 2) {
        ui_SetStage(st_cur, st_max);
    }
    // ui_SetStage(5, 8); // debug

    if (show_text) ui_ShowText(true);

    struct selinux_opt seopts[] = {
      { SELABEL_OPT_PATH, "/file_contexts" }
    };

    sehandle = selabel_open(SELABEL_CTX_FILE, seopts, 1);

    if (!sehandle) {
        ui_print("Warning:  No file_contexts\n");
    }

    LOGI("device_recovery_start()\n");
    device_recovery_start();

    printf("Command:");
    for (arg = 0; arg < argc; arg++) {
        printf(" \"%s\"", argv[arg]);
    }
    printf("\n");

    if (update_package) {
        // For backwards compatibility on the cache partition only, if
        // we're given an old 'root' path "CACHE:foo", change it to
        // "/cache/foo".
        if (strncmp(update_package, "CACHE:", 6) == 0) {
            int len = strlen(update_package) + 10;
            char* modified_path = (char*)malloc(len);
            strlcpy(modified_path, "/cache/", len);
            strlcat(modified_path, update_package+6, len);
            printf("(replacing path \"%s\" with \"%s\")\n",
                   update_package, modified_path);
            update_package = modified_path;
        }
    }
    printf("\n");

    property_list(print_property, NULL);
    printf("\n");

    int status = INSTALL_SUCCESS;

    if (update_package != NULL) {
        status = install_package(update_package, &wipe_cache, TEMPORARY_INSTALL_FILE);
        if (status == INSTALL_SUCCESS && wipe_cache) {
            if (erase_volume("/cache")) {
                LOGE("Cache wipe (requested by package) failed.\n");
            }
        }
        if (status != INSTALL_SUCCESS) {
            ui_print("Installation aborted.\n");

            // If this is an eng or userdebug build, then automatically
            // turn the text display on if the script fails so the error
            // message is visible.
            char buffer[PROPERTY_VALUE_MAX+1];
            property_get("ro.build.fingerprint", buffer, "");
            if (strstr(buffer, ":userdebug/") || strstr(buffer, ":eng/")) {
                ui_ShowText(true);
            }
        }
    } else if (wipe_data) {
        if (device_wipe_data()) status = INSTALL_ERROR;
        if (wipe_media) preserve_data_media(0);
        if (erase_volume("/data")) status = INSTALL_ERROR;
        if (wipe_media) preserve_data_media(1);
        if (has_datadata() && erase_volume("/datadata")) status = INSTALL_ERROR;
        if (wipe_cache && erase_volume("/cache")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) ui_print("Data wipe failed.\n");
    } else if (wipe_cache) {
        if (wipe_cache && erase_volume("/cache")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) ui_print("Cache wipe failed.\n");
    } else if (wipe_media) {
        if (is_data_media() && erase_volume("/data/media")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) ui_print("Media wipe failed.\n");
    } else if (sideload) {
        status = enter_sideload_mode(status);
    } else if (!just_exit) {
        // let's check recovery start up scripts (openrecoveryscript and ROM Manager extendedcommands)
        status = INSTALL_NONE; // No command specified, it is a normal recovery boot unless we find a boot script to run

        LOGI("Checking for extendedcommand & OpenRecoveryScript...\n");

        // we need show_text to show boot scripts log
        // only run one start up script as some ROMs create both for CWM/TWRP compatibility
        bool text_visible = ui_IsTextVisible();
        ui_SetShowText(true);
        if (0 == check_boot_script_file(EXTENDEDCOMMAND_SCRIPT)) {
            LOGI("Running extendedcommand...\n");
            status = INSTALL_ERROR;
            if (0 == run_and_remove_extendedcommand())
                status = INSTALL_SUCCESS;
        } else if (0 == check_boot_script_file(ORS_BOOT_SCRIPT_FILE)) {
            LOGI("Running openrecoveryscript....\n");
            status = INSTALL_ERROR;
            if (0 == run_ors_boot_script())
                status = INSTALL_SUCCESS;
        }

        ui_SetShowText(text_visible);
    }

    if (status == INSTALL_ERROR || status == INSTALL_CORRUPT) {
        copy_logs();
        // ui_set_background(BACKGROUND_ICON_ERROR); // will be set in prompt_and_wait() after recovery lock check
        handle_failure();
    }
    if (status != INSTALL_SUCCESS || ui_IsTextVisible()) {
        ui_SetShowText(true);
#ifdef PHILZ_TOUCH_RECOVERY
        check_recovery_lock();
#endif
        prompt_and_wait(status);
    }

    // We reach here when in main menu we choose reboot main system or on success install of boot scripts and recovery commands
    finish_recovery(send_intent);
    if (shutdown_after) {
        ui_print("Shutting down...\n");
        reboot_main_system(ANDROID_RB_POWEROFF, 0, 0);
    } else {
        ui_print("Rebooting...\n");
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    }
    return EXIT_SUCCESS;
}
