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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <errno.h>

#include <sys/types.h>
#include <fcntl.h>

#include <selinux/selinux.h>
#include <selinux/label.h>

#include "cutils/android_reboot.h"
#include "cutils/properties.h"

#include "libcrecovery/common.h"
#include "voldclient/voldclient.h"
#include "common.h"
#include "install.h"
#include "make_ext4fs.h"
#include "recovery_ui.h"
#include "roots.h"
#include "ui.h"
#include "extendedcommands.h"
#include "advanced_functions.h"
#include "recovery_settings.h"
#include "nandroid.h"
#include "adb_install.h"

// md5 display
#include <pthread.h>
#include "digest/md5.h"

#ifdef PHILZ_TOUCH_RECOVERY
#include "libtouch_gui/gui_settings.h"
#endif

extern struct selabel_handle *sehandle;

// ignore_android_secure = 1: this will force skipping android secure from backup/restore jobs
static int ignore_android_secure = 0;

int get_filtered_menu_selection(const char** headers, char** items, int menu_only, int initial_selection, int items_count) {
    int index;
    int offset = 0;
    int* translate_table = (int*)malloc(sizeof(int) * items_count);
    char* items_new[items_count];

    for (index = 0; index < items_count; index++) {
        items_new[index] = items[index];
    }

    for (index = 0; index < items_count; index++) {
        if (items_new[index] == NULL)
            continue;
        char *item = items_new[index];
        items_new[index] = NULL;
        items_new[offset] = item;
        translate_table[offset] = index;
        offset++;
    }
    items_new[offset] = NULL;

    initial_selection = translate_table[initial_selection];
    int ret = get_menu_selection(headers, items_new, menu_only, initial_selection);
    if (ret < 0 || ret >= offset) {
        free(translate_table);
        return ret;
    }

    ret = translate_table[ret];
    free(translate_table);
    return ret;
}

// pass in NULL for fileExtensionOrDirectory and you will get a directory chooser
// pass in "" to gather all files without filtering extension or filename
// returned directory (when NULL is passed as file extension) has a trailing / causing a wired // return path
// choose_file_menu returns NULL when no file is found or if we choose no file in selection
// no_files_found = 1 when no valid file was found, no_files_found = 0 when we found a valid file
// WARNING : CALLER MUST ALWAYS FREE THE RETURNED POINTER
int no_files_found = 0;
char* choose_file_menu(const char* basedir, const char* fileExtensionOrDirectory, const char* headers[]) {
    const char* fixed_headers[20];
    int numFiles = 0;
    int numDirs = 0;
    int i;
    char* return_value = NULL;
    char directory[PATH_MAX];
    int dir_len = strlen(basedir);

    strcpy(directory, basedir);

    // Append a trailing slash if necessary
    if (directory[dir_len - 1] != '/') {
        strcat(directory, "/");
        dir_len++;
    }

    i = 0;
    while (headers[i]) {
        fixed_headers[i] = headers[i];
        i++;
    }
    fixed_headers[i] = directory;
    // let's spare some header space
    // fixed_headers[i + 1] = "";
    // fixed_headers[i + 2] = NULL;
    fixed_headers[i + 1] = NULL;

    char** files = gather_files(directory, fileExtensionOrDirectory, &numFiles);
    char** dirs = NULL;
    if (fileExtensionOrDirectory != NULL)
        dirs = gather_files(directory, NULL, &numDirs);
    int total = numDirs + numFiles;
    if (total == 0) {
        // we found no valid file to select
        no_files_found = 1;
        ui_print("No files found.\n");
    } else {
        // we found a valid file to select
        no_files_found = 0;
        char** list = (char**)malloc((total + 1) * sizeof(char*));
        list[total] = NULL;


        for (i = 0; i < numDirs; i++) {
            list[i] = strdup(dirs[i] + dir_len);
        }

        for (i = 0; i < numFiles; i++) {
            list[numDirs + i] = strdup(files[i] + dir_len);
        }

        for (;;) {
            int chosen_item = get_menu_selection(fixed_headers, list, 0, 0);
            if (chosen_item == GO_BACK || chosen_item == REFRESH)
                break;
            if (chosen_item < numDirs) {
                char* subret = choose_file_menu(dirs[chosen_item], fileExtensionOrDirectory, headers);
                if (subret != NULL) {
                    // we selected either a folder (or a file from a re-entrant call)
                    return_value = strdup(subret);
                    free(subret);
                    break;
                }
                // the previous re-entrant call did a GO_BACK, REFRESH or no file found in a directory: subret == NULL
                // we drop to up folder
                continue;
            }
            // we selected a file
            return_value = strdup(files[chosen_item - numDirs]);
            break;
        }
        free_string_array(list);
    }

    free_string_array(files);
    free_string_array(dirs);
    return return_value;
}

int confirm_selection(const char* title, const char* confirm) {
    // check if recovery needs no confirm, many confirm or a few confirm menus
    char path[PATH_MAX];
    int many_confirm;
    sprintf(path, "%s/%s", get_primary_storage_path(), RECOVERY_NO_CONFIRM_FILE);
    if (file_found(path))
        return 1;

    sprintf(path, "%s/%s", get_primary_storage_path(), RECOVERY_MANY_CONFIRM_FILE);
    many_confirm = file_found(path);

    char* confirm_str = strdup(confirm);
    const char* confirm_headers[] = { title, "  THIS CAN NOT BE UNDONE.", "", NULL };
    int ret = 0;

    int old_val = ui_is_showing_back_button();
    ui_set_showing_back_button(0);

    if (many_confirm) {
        char* items[] = {
            "No",
            "No",
            "No",
            "No",
            "No",
            "No",
            "No",
            confirm_str, //" Yes -- wipe partition",   // [7]
            "No",
            "No",
            "No",
            NULL
        };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        ret = (chosen_item == 7);
    } else {
        char* items[] = {
            "No",
            confirm_str, //" Yes -- wipe partition",   // [1]
            "No",
            NULL
        };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        ret = (chosen_item == 1);
    }

    free(confirm_str);
    ui_set_showing_back_button(old_val);
    return ret;
}

int confirm_with_headers(const char** confirm_headers, const char* confirm) {
    // check if recovery needs no confirm, many confirm or a few confirm menus
    char path[PATH_MAX];
    int many_confirm;
    sprintf(path, "%s/%s", get_primary_storage_path(), RECOVERY_NO_CONFIRM_FILE);
    if (file_found(path))
        return 1;

    sprintf(path, "%s/%s", get_primary_storage_path(), RECOVERY_MANY_CONFIRM_FILE);
    many_confirm = file_found(path);

    char* confirm_str = strdup(confirm);
    int ret = 0;

    int old_val = ui_is_showing_back_button();
    ui_set_showing_back_button(0);

    if (many_confirm) {
        char* items[] = {
            "No",
            "No",
            "No",
            "No",
            "No",
            "No",
            "No",
            confirm_str, //" Yes -- wipe partition",   // [7]
            "No",
            "No",
            "No",
            NULL
        };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        ret = (chosen_item == 7);
    } else {
        char* items[] = {
            "No",
            confirm_str, //" Yes -- wipe partition",   // [1]
            "No",
            NULL
        };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        ret = (chosen_item == 1);
    }

    free(confirm_str);
    ui_set_showing_back_button(old_val);
    return ret;
}

void handle_failure() {
    if (0 != ensure_path_mounted(get_primary_storage_path()))
        return;
    mkdir("/sdcard/clockworkmod", S_IRWXU | S_IRWXG | S_IRWXO);
    __system("cp /tmp/recovery.log /sdcard/clockworkmod/philz_recovery.log");
    ui_print("/tmp/recovery.log copied to /sdcard/clockworkmod/philz_recovery.log\n");
    ui_print("Send file to Phil3759 @xda\n");
}

//start print tail from custom log file
void ui_print_custom_logtail(const char* filename, int nb_lines) {
    char * backup_log;
    char tmp[PATH_MAX];
    FILE * f;
    int line=0;
    sprintf(tmp, "tail -n %d %s > /tmp/custom_tail.log", nb_lines, filename);
    __system(tmp);
    f = fopen("/tmp/custom_tail.log", "rb");
    if (f != NULL) {
        while (line < nb_lines) {
            backup_log = fgets(tmp, PATH_MAX, f);
            if (backup_log == NULL) break;
            ui_print("%s", tmp);
            line++;
        }
        fclose(f);
    } else
        LOGE("Cannot open /tmp/custom_tail.log\n");
}

/**********************************/
/*       Start md5sum display     */
/*    Original source by PhilZ    */
/*    MD5 code from twrpDigest    */
/*              by                */
/*    bigbiff/Dees_Troy TeamWin   */
/**********************************/

// calculate md5sum of filepath
// return 0 on success, 1 if cancelled by user, -1 on failure to open source file
// fills md5sum char array with the resulting md5sum
// functions calling this should first set the progress bar:
//    ui_reset_progress();
//    ui_show_progress(1, 0);
// and after it is done, reset the progress bar
//    ui_reset_progress();
static int cancel_md5digest = 0;
static int computeMD5(const char* filepath, char *md5sum) {
    struct MD5Context md5c;
    unsigned char md5sum_array[MD5LENGTH];
    unsigned char buf[1024];
    char hex[3];
    unsigned long size_total;
    unsigned long size_progress;
    unsigned len;
    int i;

    if (!file_found(filepath)) {
        LOGE("computeMD5: '%s' not found\n", filepath);
        return -1;
    }

    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        LOGE("computeMD5: can't open %s\n", filepath);
        return -1;
    }

    size_total = Get_File_Size(filepath);
    size_progress = 0;
    cancel_md5digest = 0;
    MD5Init(&md5c);
    while (!cancel_md5digest && (len = fread(buf, 1, sizeof(buf), file)) > 0) {
        MD5Update(&md5c, buf, len);
        size_progress += len;
        if (size_total != 0)
            ui_set_progress((float)size_progress / (float)size_total);
    }

    if (!cancel_md5digest) {
        MD5Final(md5sum_array ,&md5c);
        strcpy(md5sum, "");
        for (i = 0; i < 16; ++i) {
            snprintf(hex, 3 ,"%02x", md5sum_array[i]);
            strcat(md5sum, hex);
        }
    }

    fclose(file);
    return cancel_md5digest;
}

// write calculated md5 to file or to log/screen if md5file is NULL
// returns negative value on failure or total number of bytes written on success (or 0 if only display md5)
// if cancelled by user in thread mode, it returns 1
int write_md5digest(const char* filepath, const char* md5file, int append) {
    int ret;
    char md5sum[PATH_MAX];

    ret = computeMD5(filepath, md5sum);
    if (ret != 0)
        return ret;

    if (md5file == NULL) {
        ui_print("%s\n", md5sum);
    } else {
        char* b = t_BaseName(filepath);
        strcat(md5sum, "  ");
        strcat(md5sum, b);
        strcat(md5sum, "\n");
        free(b);
        if (append)
            ret = append_string_to_file(md5file, md5sum);
        else
            ret = write_string_to_file(md5file, md5sum);
    }

    return ret;
}

// verify md5sum of filepath if it matches content from md5file
// if md5file == NULL, we try filepath.md5
int verify_md5digest(const char* filepath, const char* md5file) {
    char md5file2[PATH_MAX];
    int ret = -1;

    if (!file_found(filepath)) {
        LOGE("verify_md5digest: '%s' not found\n", filepath);
        return ret;
    }

    if (md5file != NULL) {
        sprintf(md5file2, "%s", md5file);
    } else {
        sprintf(md5file2, "%s.md5", filepath);
    }

    // read md5 sum from md5file2
    // read_file_to_buffer() will call file_found() function
    unsigned long len = 0;
    char* md5read = read_file_to_buffer(md5file2, &len);
    if (md5read == NULL)
        return ret;
    md5read[len] = '\0';

    // calculate md5sum of filepath and check if it matches what we read from md5file2
    // computeMD5() != 0 if cancelled by user in multi-threading mode (1) or if file not found (-1)
    // file.md5 is formatted like (new line at end):
    // 264c7c1e6f682cb99a07c283117f7f07  test_code.c\n
    char md5sum[PATH_MAX];
    if (0 == (ret = computeMD5(filepath, md5sum))) {
        char* b = t_BaseName(filepath);
        strcat(md5sum, "  ");
        strcat(md5sum, b);
        strcat(md5sum, "\n");
        free(b);
        if (strcmp(md5read, md5sum) != 0) {
            LOGE("MD5 calc: %s\n", md5sum);
            LOGE("Expected: %s\n", md5read);
            ret = -1;
        }
    }

    free(md5read);
    return ret;
}

// thread function called when installing zip files from menu
pthread_t tmd5_display;
pthread_t tmd5_verify;
static void *md5_display_thread(void *arg) {
    char filepath[PATH_MAX];
    ui_reset_progress();
    ui_show_progress(1, 0);
    sprintf(filepath, "%s", (char*)arg);
    write_md5digest(filepath, NULL, 0);
    ui_reset_progress();
    return NULL;
}

static void *md5_verify_thread(void *arg) {
    int ret;
    char filepath[PATH_MAX];

    sprintf(filepath, "%s", (char*)arg);
    ui_reset_progress();
    ui_show_progress(1, 0);
    ret = verify_md5digest(filepath, NULL);
    ui_reset_progress();

    // ret == 1 if cancelled by user: do not log
    if (ret < 0) {
#ifdef PHILZ_TOUCH_RECOVERY
        ui_print_preset_colors(1, "red");
#endif
        ui_print("MD5 check: error\n");
    } else if (ret == 0) {
#ifdef PHILZ_TOUCH_RECOVERY
        ui_print_preset_colors(1, "green");
#endif
        ui_print("MD5 check: success\n");
    }

    return NULL;
}

void start_md5_display_thread(char* filepath) {
    // ensure_path_mounted() is not thread safe, we must disable it when starting a thread for md5 checks
    // to install the zip file, we must re-enable the ensure_path_mounted() function: it is done when stopping the thread
    // at this point, filepath should be mounted by caller
    // we ensure primary storage is also mounted as it is needed by confirm_install() function
    ensure_path_mounted(get_primary_storage_path());
    set_ensure_mount_always_true(1);

    // show the message in color (cyan by default)
    // we will reset color before exiting the thread
#ifdef PHILZ_TOUCH_RECOVERY
    ui_print_preset_colors(1, NULL);
#endif
    ui_print("Calculating md5sum...\n");

    pthread_create(&tmd5_display, NULL, &md5_display_thread, filepath);
}

void stop_md5_display_thread() {
    cancel_md5digest = 1;
    if (pthread_kill(tmd5_display, 0) != ESRCH)
        ui_print("Cancelling md5sum...\n");

    pthread_join(tmd5_display, NULL);
    set_ensure_mount_always_true(0);
#ifdef PHILZ_TOUCH_RECOVERY
    ui_print_preset_colors(0, NULL);
#endif
}

void start_md5_verify_thread(char* filepath) {
    // ensure_path_mounted() is not thread safe, we must disable it when starting a thread for md5 checks
    // to install the zip file, we must re-enable the ensure_path_mounted() function: it is done when stopping the thread
    // at this point, filepath should be mounted by caller
    // we ensure primary storage is also mounted as it is needed by confirm_install() function
    ensure_path_mounted(get_primary_storage_path());
    set_ensure_mount_always_true(1);

    // show the message in color (cyan by default)
    // we will reset color before exiting the thread
#ifdef PHILZ_TOUCH_RECOVERY
    ui_print_preset_colors(1, NULL);
#endif
    ui_print("Verifying md5sum...\n");

    pthread_create(&tmd5_verify, NULL, &md5_verify_thread, filepath);
}

void stop_md5_verify_thread() {
    cancel_md5digest = 1;
    if (pthread_kill(tmd5_verify, 0) != ESRCH)
        ui_print("Cancelling md5 check...\n");

    pthread_join(tmd5_verify, NULL);
    set_ensure_mount_always_true(0);
#ifdef PHILZ_TOUCH_RECOVERY
    ui_print_preset_colors(0, NULL);
#endif
}
// ------- End md5sum display

/***********************************************/
/* start wipe data and system options and menu */
/***********************************************/
// partition sdcard menu allows use of sd-ext
// it still must be defined in recovery.fstab to be properly used in recovery
static void show_partition_sdcard_menu() {
    const char* headers[] = { "Partition sdcard menu", "  enables sd-ext support", "", NULL };
    char** list = (char**)malloc(((MAX_NUM_MANAGED_VOLUMES) + 1) * sizeof(char*)); // + 1 for last NULL (GO_BACK menu) entry

    int i = 0;
    int list_index = 0;
    char buf[256];
    const char list_prefix[] = "Partition ";

    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    if (can_partition(primary_path)) {
        sprintf(buf, "%s%s", list_prefix, primary_path);
        list[list_index] = strdup(buf);
        ++list_index;
    }

    if (extra_paths != NULL) {
        for (i = 0; i < num_extra_volumes; ++i) {
            if (can_partition(extra_paths[i])) {
                sprintf(buf, "%s%s", list_prefix, extra_paths[i]);
                list[list_index] = strdup(buf);
                ++list_index;
            }
        }
    }
    list[list_index] = NULL;

    if (list_index == 0) {
        LOGE("no volumes found to partition.\n");
        free_string_array(extra_paths);
        return;
    }

    if (volume_for_path("/sd-ext") == NULL)
        ui_print("W: sd-ext missing in fstab!\n");

    int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
    if (chosen_item < 0) // GO_BACK || REFRESH
        goto out;

    char* path = list[chosen_item] + strlen(list_prefix);
    if (!can_partition(path)) {
        LOGE("Can't partition device: %s\n", path);
        goto out;
    }

    char* ext_sizes[] = {
        "128M",
        "256M",
        "512M",
        "1024M",
        "2048M",
        "4096M",
        NULL
    };

    char* swap_sizes[] = {
        "0M",
        "32M",
        "64M",
        "128M",
        "256M",
        NULL
    };

    char* partition_types[] = {
        "ext3",
        "ext4",
        NULL
    };

    const char* ext_headers[] = { "Ext Size", "", NULL };
    const char* swap_headers[] = { "Swap Size", "", NULL };
    const char* fstype_headers[] = { "Partition Type", "", NULL };

    int ext_size = get_menu_selection(ext_headers, ext_sizes, 0, 0);
    if (ext_size < 0)
        goto out;

    int swap_size = get_menu_selection(swap_headers, swap_sizes, 0, 0);
    if (swap_size < 0)
        goto out;

    int partition_type = get_menu_selection(fstype_headers, partition_types, 0, 0);
    if (partition_type < 0) // GO_BACK / REFRESH
        goto out;

    char cmd[PATH_MAX];
    char sddevice[256];
    Volume *vol = volume_for_path(path);

    // can_partition() ensured either blk_device or blk_device2 has /dev/block/mmcblk format
    if (strstr(vol->blk_device, "/dev/block/mmcblk") != NULL)
        strcpy(sddevice, vol->blk_device);
    else
        strcpy(sddevice, vol->blk_device2);

    // we only want the mmcblk, not the partition
    sddevice[strlen("/dev/block/mmcblkX")] = '\0';
    setenv("SDPATH", sddevice, 1);
    sprintf(cmd, "sdparted -es %s -ss %s -efs %s -s", ext_sizes[ext_size], swap_sizes[swap_size], partition_types[partition_type]);
    ui_print("Partitioning SD Card... please wait...\n");
    if (0 == __system(cmd))
        ui_print("Done!\n");
    else
        LOGE("An error occured while partitioning your SD Card. Please see /tmp/recovery.log for more details.\n");

out:
    free_string_array(list);
    free_string_array(extra_paths);
}

void wipe_data_menu() {
    const char* headers[] = { "Choose wipe option", NULL };

    char* list[] = {
        "Factory Reset",
        "Wipe Cache",
        "Wipe Dalvik/ART Cache",
        "Clean to Install a New ROM",
        NULL,
        "Custom Format Options",
        NULL,
        NULL
    };

    if (is_data_media())
        list[4] = "Wipe User Media";

    // check if we have a volume that can be partitionned (sd-ext support)
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();
    int can_partition_volumes = 0;

    if (can_partition(primary_path)) {
        can_partition_volumes = 1;
    } else if (extra_paths != NULL) {
        int i;
        for (i = 0; i < num_extra_volumes; ++i) {
            if (can_partition(extra_paths[i])) {
                can_partition_volumes = 1;
                break;
            }
        }
        free_string_array(extra_paths);
    }
    if (can_partition_volumes)
        list[6] = "Partition sdcard (sd-ext support)";

    int chosen_item = 0;
    for (;;) {
        chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item < 0) // GO_BACK / REFRESH
            break;

        switch (chosen_item) {
            case 0: {
                wipe_data(true);
                break;
            }
            case 1: {
                if (confirm_selection("Wipe cache partition ?", "Yes - Wipe cache")) {
                    ui_print("\n-- Wiping cache...\n");
                    if (erase_volume("/cache") == 0)
                        ui_print("Cache wipe complete.\n");
                }
                break;
            }
            case 2: {
                if (0 != ensure_path_mounted("/data"))
                    break;
                if (volume_for_path("/sd-ext") != NULL)
                    ensure_path_mounted("/sd-ext");
                ensure_path_mounted("/cache");
                if (confirm_selection("Wipe dalvik cache ?", "Yes - Wipe dalvik cache")) {
                    ui_print("\n-- Wiping dalvik cache...\n");
                    __system("rm -r /data/dalvik-cache");
                    __system("rm -r /cache/dalvik-cache");
                    __system("rm -r /sd-ext/dalvik-cache");
                    ui_print("Dalvik Cache wiped.\n");
                }
                break;
            }
            case 3: {
                const char* headers[] = {
                    "Wipe user and system data ?",
                    "   data | cache | datadata",
                    "   sd-ext | android_secure",
                    "   system | preload",
                    "",
                    NULL
                };

                if (confirm_with_headers(headers, "Yes - Wipe user & system data")) {
                    wipe_data(false);
                    ui_print("-- Wiping system...\n");
                    erase_volume("/system");
                    if (volume_for_path("/preload") != NULL) {
                        ui_print("-- Wiping preload...\n");
                        erase_volume("/preload");
                    }
                    ui_print("Now flash a new ROM.\n");
                }
                break;
            }
            case 4: {
                const char* headers[] = {
                    "Wipe all user media ?",
                    "   /sdcard (/data/media)",
                    "",
                    NULL
                };

                if (confirm_with_headers(headers, "Yes - Wipe all user media")) {
                    ui_print("\n-- Wiping media...\n");
                    erase_volume("/data/media");
                    ui_print("Media wipe complete.\n");
                }
                break;
            }
            case 5: {
                show_partition_format_menu();
                break;
            }
            case 6: {
                show_partition_sdcard_menu();
                break;
            }
        }
    }
}
// ------ end wipe and format options

/*****************************************/
/*      Start Multi-Flash Zip code       */
/*      Original code by PhilZ @xda      */
/*****************************************/
void show_multi_flash_menu() {
    const char* headers_dir[] = { "Choose a set of zip files", NULL };
    const char* headers[] = { "Select files to install...", NULL };

    char tmp[PATH_MAX];
    char* zip_folder = NULL;
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    // Browse sdcards until a valid multi_flash folder is found
    // first, look for MULTI_ZIP_FOLDER in /sdcard
    struct stat st;
    ensure_path_mounted(primary_path);
    sprintf(tmp, "%s/%s/", primary_path, MULTI_ZIP_FOLDER);
    stat(tmp, &st);
    if (S_ISDIR(st.st_mode)) {
        zip_folder = choose_file_menu(tmp, NULL, headers_dir);
        // zip_folder = NULL if no subfolders found or user chose Go Back
        if (no_files_found) {
            ui_print("At least one subfolder with zip files must be created under %s\n", tmp);
            ui_print("Looking in other storage...\n");
        }
    } else {
        LOGI("%s not found. Searching other storage...\n", tmp);
    }

    // case MULTI_ZIP_FOLDER not found, or no subfolders or user selected Go Back (zip_folder == NULL)
    // search for MULTI_ZIP_FOLDER in other storage paths if they exist (extra_paths != NULL)
    int i = 0;
    struct stat s;
    if (extra_paths != NULL) {
        while (zip_folder == NULL && i < num_extra_volumes) {
            ensure_path_mounted(extra_paths[i]);
            sprintf(tmp, "%s/%s/", extra_paths[i], MULTI_ZIP_FOLDER);
            stat(tmp, &s);
            if (S_ISDIR(s.st_mode)) {
                zip_folder = choose_file_menu(tmp, NULL, headers_dir);
                if (no_files_found)
                    ui_print("At least one subfolder with zip files must be created under %s\n", tmp);
            }
            i++;
        }
        free_string_array(extra_paths);
    }

    // either MULTI_ZIP_FOLDER path not found (ui_print help)
    // or it was found but no subfolder (ui_print help above in no_files_found)
    // or user chose Go Back every time: return silently
    if (zip_folder == NULL) {
        if (!(S_ISDIR(st.st_mode)) && !(S_ISDIR(s.st_mode)))
            ui_print("Create at least 1 folder with your zip files under %s\n", MULTI_ZIP_FOLDER);
        return;
    }

    //gather zip files list
    int dir_len = strlen(zip_folder);
    int numFiles = 0;
    char** files = gather_files(zip_folder, ".zip", &numFiles);
    if (numFiles == 0) {
        ui_print("No zip files found under %s\n", zip_folder);
    } else {
        // start showing multi-zip menu
        char** list = (char**)malloc((numFiles + 3) * sizeof(char*));
        memset(list, 0, sizeof(list));
        list[0] = strdup("Select/Unselect All");
        list[1] = strdup(">> Flash Selected Files <<");
        list[numFiles+2] = NULL; // Go Back Menu

        int i;
        for(i = 2; i < numFiles + 2; i++) {
            list[i] = strdup(files[i - 2] + dir_len - 4);
            strncpy(list[i], "(x) ", 4);
        }

        int select_all = 1;
        int chosen_item;
        for (;;) {
            chosen_item = get_menu_selection(headers, list, 0, 0);
            if (chosen_item == GO_BACK || chosen_item == REFRESH)
                break;
            if (chosen_item == 1)
                break;
            if (chosen_item == 0) {
                // select / unselect all
                select_all ^= 1;
                for(i = 2; i < numFiles + 2; i++) {
                    if (select_all) strncpy(list[i], "(x)", 3);
                    else strncpy(list[i], "( )", 3);
                }
            } else if (strncmp(list[chosen_item], "( )", 3) == 0) {
                strncpy(list[chosen_item], "(x)", 3);
            } else if (strncmp(list[chosen_item], "(x)", 3) == 0) {
                strncpy(list[chosen_item], "( )", 3);
            }
        }

        //flashing selected zip files
        if (chosen_item == 1) {
            char confirm[PATH_MAX];
            sprintf(confirm, "Yes - Install from %s", BaseName(zip_folder));
            if (confirm_selection("Install selected files?", confirm)) {
                for(i = 2; i < numFiles + 2; i++) {
                    if (strncmp(list[i], "(x)", 3) == 0) {
#ifdef PHILZ_TOUCH_RECOVERY
                        force_wait = -1;
#endif
                        if (install_zip(files[i - 2]) != INSTALL_SUCCESS)
                            break;
                    }
                }
            }
        }
        free_string_array(list);
    }
    free_string_array(files);
    free(zip_folder);
}
//-------- End Multi-Flash Zip code

/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*  Start open recovery script support   */
/*  Original code: Dees_Troy  at yahoo   */
/*  Original cwm port by sk8erwitskil    */
/*  Enhanced by PhilZ @xda               */
/*****************************************/

// check ors and extendedcommand boot scripts at boot (called from recovery.c)
// verifies that the boot script exists
// if found, try to edit the boot script file to fix paths in scripts generated by GooManager, ROM Manager or user
int check_boot_script_file(const char* boot_script) {
    if (!file_found(boot_script)) {
        LOGI("Ignoring '%s' boot-script: file not found\n", BaseName(boot_script));
        return -1;
    }

    LOGI("Script file found: '%s'\n", boot_script);
    char tmp[PATH_MAX];
    sprintf(tmp, "/sbin/bootscripts_mnt.sh %s %s", boot_script, get_primary_storage_path());
    if (0 != __system(tmp)) {
        // non fatal error
        LOGE("failed to fix boot script (%s)\n", strerror(errno));
        LOGE("run without fixing...\n");
    }

    return 0;
}

// run ors script on boot (called from recovery.c)
// this must be called after check_boot_script_file()
int run_ors_boot_script() {
    int ret = 0;
    char tmp[PATH_MAX];

    if (!file_found(ORS_BOOT_SCRIPT_FILE))
        return -1;

    // move formatted ors boot script to /tmp and run it from there
    sprintf(tmp, "cp -f %s /tmp/%s", ORS_BOOT_SCRIPT_FILE, BaseName(ORS_BOOT_SCRIPT_FILE));
    __system(tmp);
    remove(ORS_BOOT_SCRIPT_FILE);

    sprintf(tmp, "/tmp/%s", BaseName(ORS_BOOT_SCRIPT_FILE));
    return run_ors_script(tmp);
}

// sets the default backup volume for ors backup command
// default is primary storage
static void get_ors_backup_volume(char *volume) {
    char value_def[PATH_MAX];
    sprintf(value_def, "%s", get_primary_storage_path());
    read_config_file(PHILZ_SETTINGS_FILE, ors_backup_path.key, ors_backup_path.value, value_def);
    // on data media device, v == NULL if it is sdcard. But, it doesn't matter since value_def will be /sdcard in that case
    Volume* v = volume_for_path(ors_backup_path.value);
    if (v != NULL && ensure_path_mounted(ors_backup_path.value) == 0 && strcmp(ors_backup_path.value, v->mount_point) == 0)
        strcpy(volume, ors_backup_path.value);
    else strcpy(volume, value_def);
}

// choose ors backup volume and save user setting
static void choose_ors_volume() {
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    const char* headers[] = { "Save ors backups to:", NULL };

    char* list[MAX_NUM_MANAGED_VOLUMES + 1];
    memset(list, 0, sizeof(list));
    list[0] = strdup(primary_path);

    char buf[80];
    int i;
    if (extra_paths != NULL) {
        for(i = 0; i < num_extra_volumes; i++) {
            sprintf(buf, "%s", extra_paths[i]);
            list[i + 1] = strdup(buf);
        }
    }
    list[num_extra_volumes + 1] = NULL;

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    if (chosen_item != GO_BACK && chosen_item != REFRESH)
        write_config_file(PHILZ_SETTINGS_FILE, ors_backup_path.key, list[chosen_item]);

    free(list[0]);
    if (extra_paths != NULL) {
        free_string_array(extra_paths);
        for(i = 0; i < num_extra_volumes; i++)
            free(list[i + 1]);
    }
}

// Parse backup options in ors
// Stock CWM as of v6.x, doesn't support backup options
#define SCRIPT_COMMAND_SIZE 512

int ors_backup_command(const char* backup_path, const char* options) {
    is_custom_backup = 1;
    int old_enable_md5sum = enable_md5sum.value;
    enable_md5sum.value = 1;
    backup_boot = 0, backup_recovery = 0, backup_wimax = 0, backup_system = 0;
    backup_preload = 0, backup_data = 0, backup_cache = 0, backup_sdext = 0;
    reset_extra_partitions_state();
    int extra_partitions_num = get_extra_partitions_state();
    ignore_android_secure = 1;
    // yaffs2 partition must be backed up using default yaffs2 wrapper
    set_override_yaffs2_wrapper(0);
    nandroid_force_backup_format("tar");

    ui_print("Setting backup options:\n");
    char value1[SCRIPT_COMMAND_SIZE];
    int line_len, i;
    strcpy(value1, options);
    line_len = strlen(options);
    for (i=0; i<line_len; i++) {
        if (value1[i] == 'S' || value1[i] == 's') {
            backup_system = 1;
            ui_print("System\n");
            if (nandroid_add_preload.value) {
                backup_preload = 1;
                ui_print("Preload enabled in nandroid settings.\n");
                ui_print("It will be Processed with /system\n");
            }
        } else if (value1[i] == 'D' || value1[i] == 'd') {
            backup_data = 1;
            ui_print("Data\n");
        } else if (value1[i] == 'C' || value1[i] == 'c') {
            backup_cache = 1;
            ui_print("Cache\n");
        } else if (value1[i] == 'R' || value1[i] == 'r') {
            backup_recovery = 1;
            ui_print("Recovery\n");
        } else if (value1[i] == '1' || value1[i] == '2' || value1[i] == '3' || value1[i] == '4' || value1[i] == '5') {
            // ascii to integer
            int val = value1[i] - 48;
            if (val <= extra_partitions_num) {
                extra_partition[val - 1].backup_state = 1;
                ui_print("%s\n", extra_partition[val - 1].mount_point);
            }
        } else if (value1[i] == 'B' || value1[i] == 'b') {
            backup_boot = 1;
            ui_print("Boot\n");
        } else if (value1[i] == 'A' || value1[i] == 'a') {
            ignore_android_secure = 0;
            ui_print("Android secure\n");
        } else if (value1[i] == 'E' || value1[i] == 'e') {
            backup_sdext = 1;
            ui_print("SD-Ext\n");
        } else if (value1[i] == 'O' || value1[i] == 'o') {
            nandroid_force_backup_format("tgz");
            ui_print("Compression is on\n");
        } else if (value1[i] == 'M' || value1[i] == 'm') {
            enable_md5sum.value = 0;
            ui_print("MD5 Generation is off\n");
        }
    }

    int ret = -1;
    if (file_found(backup_path)) {
        LOGE("Specified ors backup target '%s' already exists!\n", backup_path);
    } else if (twrp_backup_mode.value) {
        ret = twrp_backup(backup_path);
    } else {
        ret = nandroid_backup(backup_path);
    }

    is_custom_backup = 0;
    nandroid_force_backup_format("");
    set_override_yaffs2_wrapper(1);
    reset_custom_job_settings(0);
    enable_md5sum.value = old_enable_md5sum;
    return ret;
}

// run ors script code
// this can be started on boot or manually for custom ors
int run_ors_script(const char* ors_script) {
    FILE *fp = fopen(ors_script, "r");
    int ret_val = 0, cindex, line_len, i, remove_nl;
    char script_line[SCRIPT_COMMAND_SIZE], command[SCRIPT_COMMAND_SIZE],
         value[SCRIPT_COMMAND_SIZE], mount[SCRIPT_COMMAND_SIZE],
         value1[SCRIPT_COMMAND_SIZE], value2[SCRIPT_COMMAND_SIZE];
    char *val_start, *tok;

    if (fp != NULL) {
        while (fgets(script_line, SCRIPT_COMMAND_SIZE, fp) != NULL && ret_val == 0) {
            cindex = 0;
            line_len = strlen(script_line);
            if (line_len < 2)
                continue; // there's a blank line or line is too short to contain a command
            LOGI("script line: '%s'\n", script_line); // debug code
            for (i=0; i<line_len; i++) {
                if ((int)script_line[i] == 32) {
                    cindex = i;
                    i = line_len;
                }
            }
            memset(command, 0, sizeof(command));
            memset(value, 0, sizeof(value));
            if ((int)script_line[line_len - 1] == 10)
                remove_nl = 2;
            else
                remove_nl = 1;
            if (cindex != 0) {
                strncpy(command, script_line, cindex);
                LOGI("command is: '%s' and ", command);
                val_start = script_line;
                val_start += cindex + 1;
                if ((int) *val_start == 32)
                    val_start++; //get rid of space
                if ((int) *val_start == 51)
                    val_start++; //get rid of = at the beginning
                strncpy(value, val_start, line_len - cindex - remove_nl);
                LOGI("value is: '%s'\n", value);
            } else {
                strncpy(command, script_line, line_len - remove_nl + 1);
                ui_print("command is: '%s' and there is no value\n", command);
            }
            if (strcmp(command, "install") == 0) {
                // Install zip
                ui_print("Installing zip file '%s'\n", value);
                ensure_path_mounted(value);
                ret_val = install_zip(value);
                if (ret_val != INSTALL_SUCCESS) {
                    LOGE("Error installing zip file '%s'\n", value);
                    ret_val = 1;
                }
            } else if (strcmp(command, "wipe") == 0) {
                // Wipe
                if (strcmp(value, "cache") == 0 || strcmp(value, "/cache") == 0) {
                    ui_print("-- Wiping Cache Partition...\n");
                    erase_volume("/cache");
                    ui_print("-- Cache Partition Wipe Complete!\n");
                } else if (strcmp(value, "dalvik") == 0 || strcmp(value, "dalvick") == 0 || strcmp(value, "dalvikcache") == 0 || strcmp(value, "dalvickcache") == 0) {
                    ui_print("-- Wiping Dalvik Cache...\n");
                    if (0 != ensure_path_mounted("/data")) {
                        ret_val = 1;
                        break;
                    }
                    ensure_path_mounted("/sd-ext");
                    ensure_path_mounted("/cache");
                    __system("rm -r /data/dalvik-cache");
                    __system("rm -r /cache/dalvik-cache");
                    __system("rm -r /sd-ext/dalvik-cache");
                    ui_print("Dalvik Cache wiped.\n");
                    ensure_path_unmounted("/data");
                    ui_print("-- Dalvik Cache Wipe Complete!\n");
                } else if (strcmp(value, "data") == 0 || strcmp(value, "/data") == 0 || strcmp(value, "factory") == 0 || strcmp(value, "factoryreset") == 0) {
                    ui_print("-- Wiping Data Partition...\n");
                    wipe_data(0);
                    ui_print("-- Data Partition Wipe Complete!\n");
                } else {
                    LOGE("Error with wipe command value: '%s'\n", value);
                    ret_val = 1;
                }
            } else if (strcmp(command, "backup") == 0) {
                char backup_path[PATH_MAX];
                char backup_volume[PATH_MAX];
                // read user set volume target
                get_ors_backup_volume(backup_volume);

                tok = strtok(value, " ");
                strcpy(value1, tok);
                tok = strtok(NULL, " ");
                if (tok != NULL) {
                    // command line has a name for backup folder
                    memset(value2, 0, sizeof(value2));
                    strcpy(value2, tok);
                    line_len = strlen(tok);
                    if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13) {
                        if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13)
                            remove_nl = 2;
                        else
                            remove_nl = 1;
                    } else
                        remove_nl = 0;

                    strncpy(value2, tok, line_len - remove_nl);
                    ui_print("Backup folder set to '%s'\n", value2);
                    if (twrp_backup_mode.value) {
                        char device_id[PROPERTY_VALUE_MAX];
                        get_device_id(device_id);
                        sprintf(backup_path, "%s/%s/%s/%s", backup_volume, TWRP_BACKUP_PATH, device_id, value2);
                    } else {
                        sprintf(backup_path, "%s/clockworkmod/backup/%s", backup_volume, value2);
                    }
                } else if (twrp_backup_mode.value) {
                    get_twrp_backup_path(backup_volume, backup_path);
                } else {
                    get_cwm_backup_path(backup_volume, backup_path);
                }
                if (0 != (ret_val = ors_backup_command(backup_path, value1)))
                    ui_print("Backup failed !!\n");
            } else if (strcmp(command, "restore") == 0) {
                // Restore
                tok = strtok(value, " ");
                strcpy(value1, tok);
                ui_print("Restoring '%s'\n", value1);

                // custom restore settings
                is_custom_backup = 1;
                int old_enable_md5sum = enable_md5sum.value;
                enable_md5sum.value = 1;
                backup_boot = 0, backup_recovery = 0, backup_system = 0;
                backup_preload = 0, backup_data = 0, backup_cache = 0, backup_sdext = 0;
                reset_extra_partitions_state();
                int extra_partitions_num = get_extra_partitions_state();
                ignore_android_secure = 1; //disable

                // check what type of restore we need and force twrp mode in that case
                int old_twrp_backup_mode = twrp_backup_mode.value;
                if (strstr(value1, TWRP_BACKUP_PATH) != NULL)
                    twrp_backup_mode.value = 1;

                tok = strtok(NULL, " ");
                if (tok != NULL) {
                    memset(value2, 0, sizeof(value2));
                    strcpy(value2, tok);
                    ui_print("Setting restore options:\n");
                    line_len = strlen(value2);
                    for (i=0; i<line_len; i++) {
                        if (value2[i] == 'S' || value2[i] == 's') {
                            backup_system = 1;
                            ui_print("System\n");
                            if (nandroid_add_preload.value) {
                                backup_preload = 1;
                                ui_print("Preload enabled in nandroid settings.\n");
                                ui_print("It will be Processed with /system\n");
                            }
                        } else if (value2[i] == 'D' || value2[i] == 'd') {
                            backup_data = 1;
                            ui_print("Data\n");
                        } else if (value2[i] == 'C' || value2[i] == 'c') {
                            backup_cache = 1;
                            ui_print("Cache\n");
                        } else if (value2[i] == 'R' || value2[i] == 'r') {
                            backup_recovery = 1;
                            ui_print("Recovery\n");
                        } else if (value2[i] == '1' || value2[i] == '2' || value2[i] == '3' || value2[i] == '4' || value2[i] == '5') {
                            // ascii to integer
                            int val = value2[i] - 48;
                            if (val <= extra_partitions_num) {
                                extra_partition[val - 1].backup_state = 1;
                                ui_print("%s\n", extra_partition[val - 1].mount_point);
                            }
                        } else if (value2[i] == 'B' || value2[i] == 'b') {
                            backup_boot = 1;
                            ui_print("Boot\n");
                        } else if (value2[i] == 'A' || value2[i] == 'a') {
                            ignore_android_secure = 0;
                            ui_print("Android secure\n");
                        } else if (value2[i] == 'E' || value2[i] == 'e') {
                            backup_sdext = 1;
                            ui_print("SD-Ext\n");
                        } else if (value2[i] == 'M' || value2[i] == 'm') {
                            enable_md5sum.value = 0;
                            ui_print("MD5 Check is off\n");
                        }
                    }
                } else {
                    LOGI("No restore options set\n");
                    LOGI("Restoring default partitions");
                    backup_boot = 1, backup_system = 1;
                    backup_data = 1, backup_cache = 1, backup_sdext = 1;
                    ignore_android_secure = 0;
                    backup_preload = nandroid_add_preload.value;
                }

                if (twrp_backup_mode.value)
                    ret_val = twrp_restore(value1);
                else
                    ret_val = nandroid_restore(value1, backup_boot, backup_system, backup_data, backup_cache, backup_sdext, 0);
                
                if (ret_val != 0)
                    ui_print("Restore failed!\n");

                is_custom_backup = 0, twrp_backup_mode.value = old_twrp_backup_mode;
                reset_custom_job_settings(0);
                enable_md5sum.value = old_enable_md5sum;
            } else if (strcmp(command, "mount") == 0) {
                // Mount
                if (value[0] != '/') {
                    strcpy(mount, "/");
                    strcat(mount, value);
                } else
                    strcpy(mount, value);
                ensure_path_mounted(mount);
                ui_print("Mounted '%s'\n", mount);
            } else if (strcmp(command, "unmount") == 0 || strcmp(command, "umount") == 0) {
                // Unmount
                if (value[0] != '/') {
                    strcpy(mount, "/");
                    strcat(mount, value);
                } else
                    strcpy(mount, value);
                ensure_path_unmounted(mount);
                ui_print("Unmounted '%s'\n", mount);
            } else if (strcmp(command, "set") == 0) {
                // Set value
                tok = strtok(value, " ");
                strcpy(value1, tok);
                tok = strtok(NULL, " ");
                strcpy(value2, tok);
                ui_print("Setting function disabled in CWMR: '%s' to '%s'\n", value1, value2);
            } else if (strcmp(command, "mkdir") == 0) {
                // Make directory (recursive)
                ui_print("Recursive mkdir disabled in CWMR: '%s'\n", value);
            } else if (strcmp(command, "reboot") == 0) {
                // Reboot
            } else if (strcmp(command, "cmd") == 0) {
                if (cindex != 0) {
                    __system(value);
                } else {
                    LOGE("No value given for cmd\n");
                }
            } else if (strcmp(command, "print") == 0) {
                ui_print("%s\n", value);
            } else if (strcmp(command, "sideload") == 0) {
                // Install zip from sideload
                ui_print("Waiting for sideload...\n");
                ret_val = enter_sideload_mode(INSTALL_SUCCESS);
                if (ret_val != INSTALL_SUCCESS)
                    LOGE("Error installing from sideload\n");
            } else {
                LOGE("Unrecognized script command: '%s'\n", command);
                ret_val = 1;
            }
        }
        fclose(fp);
        ui_print("Done processing script file\n");
    } else {
        LOGE("Error opening script file '%s'\n", ors_script);
        return 1;
    }
    return ret_val;
}

//show menu: select ors from default path
static int browse_for_file = 1; // 0 == stop browsing default file locations
static void choose_default_ors_menu(const char* volume_path) {
    if (ensure_path_mounted(volume_path) != 0) {
        LOGE("Can't mount %s\n", volume_path);
        browse_for_file = 1;
        return;
    }

    char ors_dir[PATH_MAX];
    sprintf(ors_dir, "%s/%s/", volume_path, RECOVERY_ORS_PATH);
    if (access(ors_dir, F_OK) == -1) {
        //custom folder does not exist
        browse_for_file = 1;
        return;
    }

    const char* headers[] = {
        "Choose a script to run",
        "",
        NULL
    };

    char* ors_file = choose_file_menu(ors_dir, ".ors", headers);
    if (no_files_found == 1) {
        //0 valid files to select, let's continue browsing next locations
        ui_print("No *.ors files in %s/%s\n", volume_path, RECOVERY_ORS_PATH);
        browse_for_file = 1;
    } else {
        browse_for_file = 0;
        //we found ors scripts in RECOVERY_ORS_PATH folder: do not proceed other locations even if no file is chosen
    }

    if (ors_file == NULL) {
        //either no valid files found or we selected no files by pressing back menu
        return;
    }

    char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Run %s", BaseName(ors_file));
    if (confirm_selection("Confirm run script?", confirm)) {
        run_ors_script(ors_file);
    }

    free(ors_file);
}

//show menu: browse for custom Open Recovery Script
static void choose_custom_ors_menu(const char* volume_path) {
    if (ensure_path_mounted(volume_path) != 0) {
        LOGE("Can't mount %s\n", volume_path);
        return;
    }

    const char* headers[] = {"Choose .ors script to run", NULL};

    char* ors_file = choose_file_menu(volume_path, ".ors", headers);
    if (ors_file == NULL)
        return;

    char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Run %s", BaseName(ors_file));
    if (confirm_selection("Confirm run script?", confirm)) {
        run_ors_script(ors_file);
    }

    free(ors_file);
}

//show menu: select sdcard volume to search for custom ors file
static void show_custom_ors_menu() {
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    const char* headers[] = { "Search .ors script to run", "", NULL };

    char* list[MAX_NUM_MANAGED_VOLUMES + 1];
    char list_prefix[] = "Search ";
    char buf[256];
    memset(list, 0, sizeof(list));
    sprintf(buf, "%s%s", list_prefix, primary_path);
    list[0] = strdup(buf);

    int i;
    if (extra_paths != NULL) {
        for(i = 0; i < num_extra_volumes; i++) {
            sprintf(buf, "%s%s", list_prefix, extra_paths[i]);
            list[i + 1] = strdup(buf);
        }
    }
    list[num_extra_volumes + 1] = NULL;

    int chosen_item;
    for (;;) {
        chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK || chosen_item == REFRESH)
            break;

        choose_custom_ors_menu(list[chosen_item] + strlen(list_prefix));
    }

    free(list[0]);
    if (extra_paths != NULL) {
        free_string_array(extra_paths);
        for(i = 0; i < num_extra_volumes; i++)
            free(list[i + 1]);
    }
}
//----------end open recovery script support

/**********************************/
/*       Start Get ROM Name       */
/*    Original source by PhilZ    */
/**********************************/
// formats a string to be compliant with filenames standard and limits its length to max_len
static void format_filename(char *valid_path, int max_len) {
    // remove non allowed chars (invalid file names) and limit valid_path filename to max_len chars
    // we could use a whitelist: ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-
    char invalid_fn[] = " /><%#*^$:;\"\\\t,?!{}()=+'¦|&";
    int i = 0;
    for(i=0; valid_path[i] != '\0' && i < max_len; i++) {
        size_t j = 0;
        while (j < strlen(invalid_fn)) {
            if (valid_path[i] == invalid_fn[j])
                valid_path[i] = '_';
            j++;
        }
        if (valid_path[i] == 13)
            valid_path[i] = '_';
    }

    valid_path[max_len] = '\0';
    if (valid_path[strlen(valid_path)-1] == '_') {
        valid_path[strlen(valid_path)-1] = '\0';
    }
}

// get rom_name function
// always call with rom_name[PROPERTY_VALUE_MAX]
#define MAX_ROM_NAME_LENGTH 31
void get_rom_name(char *rom_name) {
    const char *rom_id_key[] = {
        "ro.modversion",
        "ro.romversion",
        "ro.build.display.id",
        NULL
    };
    const char *key;
    int i = 0;

    strcpy(rom_name, "noname");
    while ((key = rom_id_key[i]) != NULL && strcmp(rom_name, "noname") == 0) {
        if (read_config_file("/system/build.prop", key, rom_name, "noname") < 0) {
            ui_print("failed to open /system/build.prop!\n");
            ui_print("using default noname.\n");
            break;
        }
        i++;
    }

    if (strcmp(rom_name, "noname") != 0) {
        format_filename(rom_name, MAX_ROM_NAME_LENGTH);
    }
}

/**********************************/
/*   Misc Nandroid Settings Menu  */
/**********************************/
static void regenerate_md5_sum_menu() {
    if (!confirm_selection("This is not recommended!!", "Yes - Recreate New md5 Sum"))
        return;

    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    char list_prefix[] = "Select from ";
    char buf[80];
    const char* headers[] = {"Regenerate md5 sum", "Select a backup to regenerate", NULL};
    char* list[MAX_NUM_MANAGED_VOLUMES + 1];
    memset(list, 0, sizeof(list));
    sprintf(buf, "%s%s", list_prefix, primary_path);
    list[0] = strdup(buf);

    int i;
    if (extra_paths != NULL) {
        for (i = 0; i < num_extra_volumes; i++) {
            sprintf(buf, "%s%s", list_prefix, extra_paths[i]);
            list[i + 1] = strdup(buf);
        }
    }
    list[num_extra_volumes + 1] = NULL;

    char tmp[PATH_MAX];
    int chosen_item = get_menu_selection(headers, list, 0, 0);
    if (chosen_item == GO_BACK || chosen_item == REFRESH)
        goto out;

    // select backup set and regenerate md5 sum
    sprintf(tmp, "%s/%s/", list[chosen_item] + strlen(list_prefix), CWM_BACKUP_PATH);
    if (ensure_path_mounted(tmp) != 0)
        goto out;

    char* file = choose_file_menu(tmp, "", headers);
    if (file == NULL)
        goto out;

    char backup_source[PATH_MAX];
    sprintf(backup_source, "%s", DirName(file));
    sprintf(tmp, "Process %s", BaseName(backup_source));
    if (confirm_selection("Regenerate md5 sum ?", tmp)) {
        if (0 == gen_nandroid_md5sum(backup_source))
            ui_print("Done generating md5 sum.\n");
    }

    free(file);

out:
    free(list[0]);
    if (extra_paths != NULL) {
        free_string_array(extra_paths);
        for(i = 0; i < num_extra_volumes; i++)
            free(list[i + 1]);
    }
}

void misc_nandroid_menu() {
    const char* headers[] = {
        "Misc Nandroid Settings",
        "",
        NULL
    };

    char item_md5[MENU_MAX_COLS];
    char item_preload[MENU_MAX_COLS];
    char item_twrp_mode[MENU_MAX_COLS];
    char item_size_progress[MENU_MAX_COLS];
    char item_use_nandroid_simple_logging[MENU_MAX_COLS];
    char item_nand_progress[MENU_MAX_COLS];
    char item_prompt_low_space[MENU_MAX_COLS];
    char item_ors_path[MENU_MAX_COLS];
    char item_compress[MENU_MAX_COLS];

    char* list[] = {
        item_md5,
        item_preload,
        item_twrp_mode,
        item_size_progress,
        item_use_nandroid_simple_logging,
        item_nand_progress,
        item_prompt_low_space,
        item_ors_path,
        item_compress,
        "Default Backup Format...",
        "Regenerate md5 Sum",
        NULL
    };

    int hidenandprogress;
    char* primary_path = get_primary_storage_path();
    char hidenandprogress_file[PATH_MAX];
    sprintf(hidenandprogress_file, "%s/%s", primary_path, NANDROID_HIDE_PROGRESS_FILE);

    int fmt;
    for (;;) {
        if (enable_md5sum.value) ui_format_gui_menu(item_md5, "MD5 checksum", "(x)");
        else ui_format_gui_menu(item_md5, "MD5 checksum", "( )");

        if (volume_for_path("/preload") == NULL)
            ui_format_gui_menu(item_preload, "Include /preload", "N/A");
        else if (nandroid_add_preload.value) ui_format_gui_menu(item_preload, "Include /preload", "(x)");
        else ui_format_gui_menu(item_preload, "Include /preload", "( )");

        if (twrp_backup_mode.value) ui_format_gui_menu(item_twrp_mode, "Use TWRP Mode", "(x)");
        else ui_format_gui_menu(item_twrp_mode, "Use TWRP Mode", "( )");

        if (show_nandroid_size_progress.value)
            ui_format_gui_menu(item_size_progress, "Show Nandroid Size Progress", "(x)");
        else ui_format_gui_menu(item_size_progress, "Show Nandroid Size Progress", "( )");
        list[3] = item_size_progress;

        if (use_nandroid_simple_logging.value)
            ui_format_gui_menu(item_use_nandroid_simple_logging, "Use Simple Logging (faster)", "(x)");
        else ui_format_gui_menu(item_use_nandroid_simple_logging, "Use Simple Logging (faster)", "( )");
        list[4] = item_use_nandroid_simple_logging;

        hidenandprogress = file_found(hidenandprogress_file);
        if (hidenandprogress) {
            ui_format_gui_menu(item_nand_progress, "Hide Nandroid Progress", "(x)");
            list[3] = NULL;
            list[4] = NULL;
        } else ui_format_gui_menu(item_nand_progress, "Hide Nandroid Progress", "( )");

        if (nand_prompt_on_low_space.value)
            ui_format_gui_menu(item_prompt_low_space, "Prompt on Low Free Space", "(x)");
        else ui_format_gui_menu(item_prompt_low_space, "Prompt on Low Free Space", "( )");

        char ors_volume[PATH_MAX];
        get_ors_backup_volume(ors_volume);
        ui_format_gui_menu(item_ors_path,  "ORS Backup Target", ors_volume);

        fmt = nandroid_get_default_backup_format();
        if (fmt == NANDROID_BACKUP_FORMAT_TGZ) {
            if (compression_value.value == TAR_GZ_FAST)
                ui_format_gui_menu(item_compress, "Compression", "fast");
            else if (compression_value.value == TAR_GZ_LOW)
                ui_format_gui_menu(item_compress, "Compression", "low");
            else if (compression_value.value == TAR_GZ_MEDIUM)
                ui_format_gui_menu(item_compress, "Compression", "medium");
            else if (compression_value.value == TAR_GZ_HIGH)
                ui_format_gui_menu(item_compress, "Compression", "high");
            else ui_format_gui_menu(item_compress, "Compression", TAR_GZ_DEFAULT_STR); // useless but to not make exceptions
        } else
            ui_format_gui_menu(item_compress, "Compression", "No");

        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item) {
            case 0: {
                char value[3];
                enable_md5sum.value ^= 1;
                sprintf(value, "%d", enable_md5sum.value);
                write_config_file(PHILZ_SETTINGS_FILE, enable_md5sum.key, value);
                break;
            }
            case 1: {
                char value[3];
                if (volume_for_path("/preload") == NULL)
                    nandroid_add_preload.value = 0;
                else
                    nandroid_add_preload.value ^= 1;
                sprintf(value, "%d", nandroid_add_preload.value);
                write_config_file(PHILZ_SETTINGS_FILE, nandroid_add_preload.key, value);
                break;
            }
            case 2: {
                char value[3];
                twrp_backup_mode.value ^= 1;
                sprintf(value, "%d", twrp_backup_mode.value);
                write_config_file(PHILZ_SETTINGS_FILE, twrp_backup_mode.key, value);
                break;
            }
            case 3: {
                char value[3];
                show_nandroid_size_progress.value ^= 1;
                sprintf(value, "%d", show_nandroid_size_progress.value);
                write_config_file(PHILZ_SETTINGS_FILE, show_nandroid_size_progress.key, value);
                break;
            }
            case 4: {
                char value[3];
                use_nandroid_simple_logging.value ^= 1;
                sprintf(value, "%d", use_nandroid_simple_logging.value);
                write_config_file(PHILZ_SETTINGS_FILE, use_nandroid_simple_logging.key, value);
                break;
            }
            case 5: {
                hidenandprogress ^= 1;
                if (hidenandprogress)
                    write_string_to_file(hidenandprogress_file, "1");
                else delete_a_file(hidenandprogress_file);
                break;
            }
            case 6: {
                char value[3];
                nand_prompt_on_low_space.value ^= 1;
                sprintf(value, "%d", nand_prompt_on_low_space.value);
                write_config_file(PHILZ_SETTINGS_FILE, nand_prompt_on_low_space.key, value);
                break;
            }
            case 7: {
                choose_ors_volume();
                break;
            }
            case 8: {
                if (fmt != NANDROID_BACKUP_FORMAT_TGZ) {
                    ui_print("First set backup format to tar.gz\n");
                } else {
                    // switch pigz -[ fast(1), low(3), medium(5), high(7) ] compression level
                    char value[8];
                    compression_value.value += 2;
                    if (compression_value.value == TAR_GZ_FAST)
                        sprintf(value, "fast");
                    else if (compression_value.value == TAR_GZ_LOW)
                        sprintf(value, "low");
                    else if (compression_value.value == TAR_GZ_MEDIUM)
                        sprintf(value, "medium");
                    else if (compression_value.value == TAR_GZ_HIGH)
                        sprintf(value, "high");
                    else {
                        // loop back the toggle
                        compression_value.value = TAR_GZ_FAST;
                        sprintf(value, "fast");
                    }
                    write_config_file(PHILZ_SETTINGS_FILE, compression_value.key, value);
                }
                break;
            }
            case 9: {
                choose_default_backup_format();
                break;
            }
            case 10: {
                regenerate_md5_sum_menu();
                break;
            }
        }
    }
}
//-------- End Misc Nandroid Settings

/****************************************/
/*  Start Install Zip from custom path  */
/*                 and                  */
/*       Free Browse Mode Support       */
/****************************************/
void set_custom_zip_path() {
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    const char* headers[] = { "Setup Free Browse Mode", NULL };

    int list_top_items = 2;
    char list_prefix[] = "Start Folder in ";
    char* list_main[MAX_NUM_MANAGED_VOLUMES + list_top_items + 1];
    char buf[80];
    memset(list_main, 0, sizeof(list_main));
    list_main[0] = "Disable Free Browse Mode";
    sprintf(buf, "%s%s", list_prefix, primary_path);
    list_main[1] = strdup(buf);

    int i;
    if (extra_paths != NULL) {
        for(i = 0; i < num_extra_volumes; i++) {
            sprintf(buf, "%s%s", list_prefix, extra_paths[i]);
            list_main[i + list_top_items] = strdup(buf);
        }
    }
    list_main[num_extra_volumes + list_top_items] = NULL;

    Volume* v;
    char custom_path[PATH_MAX];
    int chosen_item;
    for (;;) {
        chosen_item = get_menu_selection(headers, list_main, 0, 0);
        if (chosen_item == GO_BACK || chosen_item == REFRESH) {
            goto out;
        } else if (chosen_item == 0) {
            write_config_file(PHILZ_SETTINGS_FILE, user_zip_folder.key, "");
            ui_print("Free browse mode disabled\n");
            goto out;
        } else {
            sprintf(custom_path, "%s/", list_main[chosen_item] + strlen(list_prefix));
            if (is_data_media_volume_path(custom_path))
                v = volume_for_path("/data");
            else
                v = volume_for_path(custom_path);
            if (v == NULL || ensure_path_mounted(v->mount_point) != 0)
                continue;
            break;
        }
    }

    // populate fixed headers (display current path while browsing)
    int j = 0;
    while (headers[j]) {
        j++;
    }
    const char** fixed_headers = (const char**)malloc((j + 2) * sizeof(char*));
    j = 0;
    while (headers[j]) {
        fixed_headers[j] = headers[j];
        j++;
    }
    fixed_headers[j] = custom_path;
    fixed_headers[j + 1] = NULL;

    // start browsing for custom path
    int dir_len = strlen(custom_path);
    int numDirs = 0;
    char** dirs = gather_files(custom_path, NULL, &numDirs);
    char** list = (char**)malloc((numDirs + 3) * sizeof(char*));
    memset(list, 0, sizeof(list));
    list[0] = strdup("../");
    list[1] = strdup(">> Set current folder as default <<");
    list[numDirs + 2] = NULL; // Go Back Menu

    // populate list with current folders. Reserved list[0] for ../ to browse backward
    for(i = 2; i < numDirs + 2; i++) {
        list[i] = strdup(dirs[i - 2] + dir_len);
    }

/*
 * do not modify custom_path when browsing before we ensure it can be mounted
   else we could write a non mountable path to config file or we could alter fixed_headers path
 * vold_mount_all(): vold managed volumes need to be mounted else we end up in /storage empty folder when browsing ../ while we entered from /sdcard for example
   /storage/sdcard0, /storage/usbdisk... are not created during recovery start in init.rc but when we mount the volume for first time
*/
    char custom_path2[PATH_MAX];
    sprintf(custom_path2, "%s", custom_path);
    vold_mount_all();
    for (;;) {
        chosen_item = get_menu_selection(fixed_headers, list, 0, 0);
        if (chosen_item == GO_BACK || chosen_item == REFRESH)
            break;
        if (chosen_item == 0) {
            // browse up folder: ../
            // check for up_folder == "." for exceptions!
            const char *up_folder = DirName(custom_path);
            if (strcmp(up_folder, "/") == 0 || strcmp(up_folder, ".") == 0)
                sprintf(custom_path2, "/" );
            else
                sprintf(custom_path2, "%s/", up_folder);
        } else if (chosen_item == 1) {
            // save default folder
            // no need to ensure custom_path is mountable as it is always if we reached here
            if (strlen(custom_path) > PROPERTY_VALUE_MAX)
                LOGE("Maximum allowed path length is %d\n", PROPERTY_VALUE_MAX);
            else if (0 == write_config_file(PHILZ_SETTINGS_FILE, user_zip_folder.key, custom_path))
                ui_print("Default install zip folder set to %s\n", custom_path);
            break;
        } else {
            // continue browsing folders
            sprintf(custom_path2, "%s", dirs[chosen_item - 2]);
        }

        // mount known volumes before browsing folders
        if (is_data_media_volume_path(custom_path2) && ensure_path_mounted("/data") != 0)
            continue;
        else if (volume_for_path(custom_path2) != NULL && ensure_path_mounted(custom_path2) != 0)
            continue;

        // we're now in a mounted path or ramdisk folder: browse selected folder
        sprintf(custom_path, "%s", custom_path2);
        fixed_headers[j] = custom_path;
        dir_len = strlen(custom_path);
        numDirs = 0;
        free_string_array(list);
        free_string_array(dirs);
        dirs = gather_files(custom_path, NULL, &numDirs);
        list = (char**)malloc((numDirs + 3) * sizeof(char*));
        list[0] = strdup("../");
        list[1] = strdup(">> Set current folder as default <<");
        list[numDirs+2] = NULL;
        for(i=2; i < numDirs+2; i++) {
            list[i] = strdup(dirs[i-2] + dir_len);
        }
    }
    free_string_array(list);
    free_string_array(dirs);
    free(fixed_headers);

out:
    // free(list_main[0]);
    free(list_main[1]);
    if (extra_paths != NULL) {
        free_string_array(extra_paths);
        for(i = 0; i < num_extra_volumes; i++)
            free(list_main[i + list_top_items]);
    }
}

int show_custom_zip_menu() {
    const char* headers[] = { "Choose a zip to apply", NULL };

    int ret = 0;
    read_config_file(PHILZ_SETTINGS_FILE, user_zip_folder.key, user_zip_folder.value, "");

    // try to mount known volumes, ignore unknown ones to permit using ramdisk and other paths
    if (strcmp(user_zip_folder.value, "") == 0)
        ret = 1;
    else if (is_data_media_volume_path(user_zip_folder.value) && ensure_path_mounted("/data") != 0)
        ret = -1;
    else if (volume_for_path(user_zip_folder.value) != NULL && ensure_path_mounted(user_zip_folder.value) != 0)
        ret = -1;

    if (ret != 0) {
        LOGE("Cannot mount custom path %s\n", user_zip_folder.value);
        LOGE("You must first setup a valid folder\n");
        return ret;
    }

    char custom_path[PATH_MAX];
    sprintf(custom_path, "%s", user_zip_folder.value);
    if (custom_path[strlen(custom_path) - 1] != '/')
        strcat(custom_path, "/");
    //LOGE("Retained user_zip_folder.value to custom_path=%s\n", custom_path);

    // populate fixed headers (display current path while browsing)
    int j = 0;
    while (headers[j]) {
        j++;
    }
    const char** fixed_headers = (const char**)malloc((j + 2) * sizeof(char*));
    j = 0;
    while (headers[j]) {
        fixed_headers[j] = headers[j];
        j++;
    }
    fixed_headers[j] = custom_path;
    fixed_headers[j + 1] = NULL;

    //gather zip files and display ../ to browse backward
    int dir_len = strlen(custom_path);
    int numDirs = 0;
    int numFiles = 0;
    int total;
    char** dirs = gather_files(custom_path, NULL, &numDirs);
    char** files = gather_files(custom_path, ".zip", &numFiles);
    total = numFiles + numDirs;
    char** list = (char**)malloc((total + 2) * sizeof(char*));
    memset(list, 0, sizeof(list));
    list[0] = strdup("../");
    list[total + 1] = NULL;

    // populate menu list with current folders and zip files. Reserved list[0] for ../ to browse backward
    //LOGE(">> Dirs (num=%d):\n", numDirs);
    int i;
    for(i = 1; i < numDirs + 1; i++) {
        list[i] = strdup(dirs[i - 1] + dir_len);
        //LOGE("list[%d]=%s\n", i, list[i]);
    }
    //LOGE("\n>> Files (num=%d):\n", numFiles);
    for(i = 1; i < numFiles + 1; i++) {
        list[numDirs + i] = strdup(files[i - 1] + dir_len);
        //LOGE("list[%d]=%s\n", numDirs+i, list[numDirs+i]);
    }

    // do not modify custom_path when browsing before we ensure it can be mounted
    // else we could write a non mountable path to config file or we could alter fixed_headers path
    // vold_mount_all(): vold managed volumes need to be mounted else we end up in /storage empty folder
    //  - /storage/sdcard0, /storage/usbdisk... are not created during recovery start in init.rc but when we mount the volume for first time
    int chosen_item;
    char custom_path2[PATH_MAX];
    sprintf(custom_path2, "%s", custom_path);
    vold_mount_all();
    for (;;) {
/*
        LOGE("\n\n>> Total list:\n");
        for(i=0; i < total+1; i++) {
            LOGE("list[%d]=%s\n", i, list[i]);
        }
*/
        chosen_item = get_menu_selection(fixed_headers, list, 0, 0);
        //LOGE("\n\n>> Gathering files for chosen_item=%d:\n", chosen_item);
        if (chosen_item == REFRESH)
            continue;
        if (chosen_item == GO_BACK) {
            if (strcmp(custom_path2, "/") == 0)
                break;
            else chosen_item = 0;
        }
        if (chosen_item < numDirs+1 && chosen_item >= 0) {
            // we selected a folder: browse it
            if (chosen_item == 0) {
                // we selected ../
                const char *up_folder = DirName(custom_path2);
                sprintf(custom_path2, "%s", up_folder);
                if (strcmp(custom_path2, "/") != 0)
                    strcat(custom_path2, "/");
            } else {
                // we selected a folder
                sprintf(custom_path2, "%s", dirs[chosen_item - 1]);
            }
            //LOGE("\n\n Selected chosen_item=%d is: %s\n\n", chosen_item, custom_path2);

            // mount known volumes before browsing folders
            if (is_data_media_volume_path(custom_path2) && ensure_path_mounted("/data") != 0)
                continue;
            else if (volume_for_path(custom_path2) != NULL && ensure_path_mounted(custom_path2) != 0)
                continue;

            // we're now in a mounted path or ramdisk folder: browse selected folder
            sprintf(custom_path, "%s", custom_path2);
            fixed_headers[j] = custom_path;
            dir_len = strlen(custom_path);
            numDirs = 0;
            numFiles = 0;
            free_string_array(list);
            free_string_array(files);
            free_string_array(dirs);
            dirs = gather_files(custom_path, NULL, &numDirs);
            files = gather_files(custom_path, ".zip", &numFiles);
            total = numFiles + numDirs;
            list = (char**)malloc((total + 2) * sizeof(char*));
            list[0] = strdup("../");
            list[total+1] = NULL;
                
            //LOGE(">> Dirs (num=%d):\n", numDirs);
            for(i=1; i < numDirs+1; i++) {
                list[i] = strdup(dirs[i-1] + dir_len);
                //LOGE("list[%d]=%s\n", i, list[i]);
            }
            //LOGE("\n>> Files (num=%d):\n", numFiles);
            for(i=1; i < numFiles+1; i++) {
                list[numDirs+i] = strdup(files[i-1] + dir_len);
                //LOGE("list[%d]=%s\n", numDirs+i, list[numDirs+i]);
            }
        } else if (chosen_item > numDirs && chosen_item < total+1) {
            // we selected a zip file to install
            break;
        }
    }
/*
    LOGE("\n\n>> Selected dir contains:\n");
    for(i=0; i < total+1; i++) {
        LOGE("list[%d]=%s\n", i, list[i]);
    }
*/
    //flashing selected zip file
    if (chosen_item !=  GO_BACK) {
        char tmp[PATH_MAX];
        int confirm;

        sprintf(tmp, "Yes - Install %s", list[chosen_item]);
        if (install_zip_verify_md5.value) start_md5_verify_thread(files[chosen_item - numDirs - 1]);
        else start_md5_display_thread(files[chosen_item - numDirs - 1]);

        confirm = confirm_selection("Install selected file?", tmp);

        if (install_zip_verify_md5.value) stop_md5_verify_thread();
        else stop_md5_display_thread();

        if (confirm) {
            // when installing from ramdisk paths, do not fail on ensure_path_mounted() commands
            set_ensure_mount_always_true(1);
            install_zip(files[chosen_item - numDirs - 1]);
            set_ensure_mount_always_true(0);
        }
    }
    free_string_array(list);
    free_string_array(files);
    free_string_array(dirs);
    free(fixed_headers);
    return 0;
}
//-------- End Free Browse Mode


/*****************************************/
/*   Custom Backup and Restore Support   */
/*       code written by PhilZ @xda      */
/*        for PhilZ Touch Recovery       */
/*****************************************/

// get mount points for nandroid supported extra partitions
// returns the number of extra partitions we found
// first call must initialize the partitions state, else they can run un-initialized in backup command line calls (nandroid.c)
// always reset backup_state to 0 after use
static int extra_partitions_initialized = 0;
void reset_extra_partitions_state() {
    int i;
    for (i = 0; i < MAX_EXTRA_NANDROID_PARTITIONS; ++i) {
        extra_partition[i].backup_state = 0;
        if (!extra_partitions_initialized)
            extra_partition[i].mount_point[0] = '\0';
    }
    extra_partitions_initialized = 1;
}

int get_extra_partitions_state() {
    if (!extra_partitions_initialized)
        reset_extra_partitions_state();

    int i = 0;
    char *ptr;
    char extra_partitions_prop[PROPERTY_VALUE_MAX];
    property_get("ro.cwm.backup_partitions", extra_partitions_prop, "");

    ptr = strtok(extra_partitions_prop, ", ");
    while (ptr != NULL && i < MAX_EXTRA_NANDROID_PARTITIONS) {
        strcpy(extra_partition[i].mount_point, ptr);
        ptr = strtok(NULL, ", ");
        ++i;
    }

    return i;
}

/*
- set_android_secure_path() should be called each time we want to backup/restore .android_secure
- it will always favour external storage
- get_android_secure_path() always sets it to get_primary_storage_path()
- it will format path to retained android_secure location and set android_secure_ext to 1 or 0
- android_secure_ext = 1, will allow nandroid processing of android_secure partition
- always check android_secure_ext return code before using passed char *and_sec_path as it won't be modified if ignore_android_secure == 1
- to force other storage, user must keep only one .android_secure folder in one of the sdcards
- for /data/media devices, only second storage is allowed, not /sdcard
- custom backup and restore jobs (incl twrp and ors modes) can force .android_secure to be ignored
  this is done by setting ignore_android_secure to 1
- ignore_android_secure is by default 0 and will be reset to 0 by reset_custom_job_settings()
- On restore job: if no android_secure folder is found on any sdcard, restore is skipped.
                  you need to create at least one .android_secure folder on one of the sd cards to restore to
*/
int set_android_secure_path(char *and_sec_path) {
    if (ignore_android_secure)
        return android_secure_ext = 0;

    android_secure_ext = 1;

    struct stat st;
    char buf[80];
    char* path = NULL;
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    // search second storage path for .android_secure (favour external storage)
    int i = 0;
    if (extra_paths != NULL) {
        while (i < num_extra_volumes && path == NULL) {
            sprintf(buf, "%s/.android_secure", extra_paths[i]);
            if (ensure_path_mounted(buf) == 0 && lstat(buf, &st) == 0)
                path = buf;
            i++;
        }
        free_string_array(extra_paths);
    }

    // assign primary storage (/sdcard) only if not datamedia and we did not find .android_secure in external storage
    if (path == NULL && !is_data_media()) {
        path = get_android_secure_path();
        if (ensure_path_mounted(path) != 0 || lstat(path, &st) != 0)
            path = NULL;
    }

    if (path == NULL)
        android_secure_ext = 0;
    else strcpy(and_sec_path, path);

    return android_secure_ext;
}

void reset_custom_job_settings(int custom_job) {
    // check if we are setting defaults for custom jobs
    if (custom_job) {
        backup_boot = 1, backup_recovery = 1, backup_system = 1;
        backup_data = 1, backup_cache = 1;
        backup_wimax = 0;
        backup_sdext = 0;
    } else {
        // we are exiting backup jobs, revert to default CWM so that stock Backup / Restore behaves as expected
        backup_boot = 1, backup_recovery = 1, backup_system = 1;
        backup_data = 1, backup_cache = 1;
        backup_wimax = 1;
        backup_sdext = 1;
    }

    // preload: disabled by default to ensure it is never set to 1 on devices without /preload and no need to add extra checks in code
    //          else it will be 1 even for devices without /preload and will block efs backup/restore until we touch the preload menu
    //          or if we add extrac code checks for preload volume
    // modem is disabled from nandroid backups, but can be part of custom backup jobs
    // efs backup is disabled in stock nandroid. In custom Jobs, it must be alone.
    // backup_data_media is always 0. It can be set to 1 only in custom backup and restore menu AND if is_data_media() && !twrp_backup_mode.value
    // in TWRP it is part of backup/restore job
    backup_preload = 0;
    backup_modem = 0;
    backup_radio = 0;
    backup_efs = 0;
    backup_misc = 0;
    backup_data_media = 0;
    reset_extra_partitions_state();
    ignore_android_secure = 0;
    reboot_after_nandroid = 0;
}

static void ui_print_backup_list() {
    ui_print("This will process");
    if (backup_boot)
        ui_print(" - boot");
    if (backup_recovery)
        ui_print(" - recovery");
    if (backup_system)
        ui_print(" - system");
    if (backup_preload)
        ui_print(" - preload");
    if (backup_data)
        ui_print(" - data");
    if (backup_cache)
        ui_print(" - cache");
    if (backup_sdext)
        ui_print(" - sd-ext");
    if (backup_modem)
        ui_print(" - modem");
    if (backup_radio)
        ui_print(" - radio");
    if (backup_wimax)
        ui_print(" - wimax");
    if (backup_efs)
        ui_print(" - efs");
    if (backup_misc)
        ui_print(" - misc");
    if (backup_data_media)
        ui_print(" - data/media");

    // check extra partitions
    int i;
    int extra_partitions_num = get_extra_partitions_state();
    for (i = 0; i < extra_partitions_num; ++i) {
        if (extra_partition[i].backup_state)
            ui_print(" - %s", extra_partition[i].mount_point);
    }

    ui_print("!\n");
}

void get_cwm_backup_path(const char* backup_volume, char *backup_path) {
    char rom_name[PROPERTY_VALUE_MAX] = "noname";
    get_rom_name(rom_name);

    time_t t = time(NULL);
    struct tm *timeptr = localtime(&t);
    if (timeptr == NULL) {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        if (backup_efs)
            sprintf(backup_path, "%s/%s/%ld", backup_volume, EFS_BACKUP_PATH, tp.tv_sec);
        else
            sprintf(backup_path, "%s/%s/%ld_%s", backup_volume, CWM_BACKUP_PATH, tp.tv_sec, rom_name);
    } else {
        char tmp[PATH_MAX];
        strftime(tmp, sizeof(tmp), "%F.%H.%M.%S", timeptr);
        // this sprintf results in:
        // clockworkmod/custom_backup/%F.%H.%M.%S (time values are populated too)
        if (backup_efs)
            sprintf(backup_path, "%s/%s/%s", backup_volume, EFS_BACKUP_PATH, tmp);
        else
            sprintf(backup_path, "%s/%s/%s_%s", backup_volume, CWM_BACKUP_PATH, tmp, rom_name);
    }
}

void show_twrp_restore_menu(const char* backup_volume) {
    char backup_path[PATH_MAX];
    sprintf(backup_path, "%s/%s/", backup_volume, TWRP_BACKUP_PATH);
    if (ensure_path_mounted(backup_path) != 0) {
        LOGE("Can't mount %s\n", backup_path);
        return;
    }

    const char* headers[] = { "Choose a backup to restore", NULL };

    char device_id[PROPERTY_VALUE_MAX];
    get_device_id(device_id);
    strcat(backup_path, device_id);

    char* file = choose_file_menu(backup_path, "", headers);
    if (file == NULL) {
        // either no valid files found or we selected no files by pressing back menu
        if (no_files_found)
            ui_print("Nothing to restore in %s !\n", backup_path);
        return;
    }

    char confirm[PATH_MAX];
    char backup_source[PATH_MAX];
    sprintf(backup_source, "%s", DirName(file));
    ui_print("%s will be restored to selected partitions!\n", backup_source);
    sprintf(confirm, "Yes - Restore %s", BaseName(backup_source));
    if (confirm_selection("Restore from this backup ?", confirm))
        twrp_restore(backup_source);

    free(file);
}

static void custom_restore_handler(const char* backup_volume, const char* backup_folder) {
    char backup_path[PATH_MAX];
    char tmp[PATH_MAX];
    char backup_source[PATH_MAX];
    char* file = NULL;
    char* confirm_install = "Restore from this backup?";
    const char* headers[] = { "Choose a backup to restore", NULL };

    sprintf(backup_path, "%s/%s", backup_volume, backup_folder);
    if (ensure_path_mounted(backup_path) != 0) {
        LOGE("Can't mount %s\n", backup_path);
        return;
    }

    if (backup_efs == RESTORE_EFS_IMG) {
        if (volume_for_path("/efs") == NULL) {
            LOGE("No /efs partition to flash\n");
            return;
        }
        file = choose_file_menu(backup_path, ".img", headers);
        if (file == NULL) {
            // either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        // restore efs raw image
        sprintf(backup_source, "%s", BaseName(file));
        ui_print("%s will be flashed to /efs!\n", backup_source);
        sprintf(tmp, "Yes - Restore %s", backup_source);
        if (confirm_selection(confirm_install, tmp))
            dd_raw_restore_handler(file, "/efs");
    } else if (backup_efs == RESTORE_EFS_TAR) {
        if (volume_for_path("/efs") == NULL) {
            LOGE("No /efs partition to flash\n");
            return;
        }

        file = choose_file_menu(backup_path, NULL, headers);
        if (file == NULL) {
            // either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        // ensure there is no efs.img file in same folder (as nandroid_restore_partition_extended will force it to be restored)
        sprintf(tmp, "%s/efs.img", file);
        if (file_found(tmp)) {
            ui_print("efs.img file detected in %s!\n", file);
            ui_print("Either select efs.img to restore it,\n");
            ui_print("or remove it to restore nandroid source.\n");
        } else {
            // restore efs from nandroid tar format
            ui_print("%s will be restored to /efs!\n", file);
            sprintf(tmp, "Yes - Restore %s", BaseName(file));
            if (confirm_selection(confirm_install, tmp))
                nandroid_restore(file, 0, 0, 0, 0, 0, 0);
        }
    } else if (backup_modem == RAW_BIN_FILE) {
        file = choose_file_menu(backup_path, ".bin", headers);
        if (file == NULL) {
            // either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        // restore modem.bin raw image
        sprintf(backup_source, "%s", BaseName(file));
        Volume *vol = volume_for_path("/modem");
        if (vol != NULL) {
            ui_print("%s will be flashed to /modem!\n", backup_source);
            sprintf(tmp, "Yes - Restore %s", backup_source);
            if (confirm_selection(confirm_install, tmp))
                dd_raw_restore_handler(file, "/modem");
        } else
            LOGE("no /modem partition to flash\n");
    } else if (backup_radio == RAW_BIN_FILE) {
        file = choose_file_menu(backup_path, ".bin", headers);
        if (file == NULL) {
            // either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        // restore radio.bin raw image
        sprintf(backup_source, "%s", BaseName(file));
        Volume *vol = volume_for_path("/radio");
        if (vol != NULL) {
            ui_print("%s will be flashed to /radio!\n", backup_source);
            sprintf(tmp, "Yes - Restore %s", backup_source);
            if (confirm_selection(confirm_install, tmp))
                dd_raw_restore_handler(file, "/radio");
        } else
            LOGE("no /radio partition to flash\n");
    } else {
        // process restore job
        file = choose_file_menu(backup_path, "", headers);
        if (file == NULL) {
            // either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        sprintf(backup_source, "%s", DirName(file));
        ui_print("%s will be restored to selected partitions!\n", backup_source);
        sprintf(tmp, "Yes - Restore %s", BaseName(backup_source));
        if (confirm_selection(confirm_install, tmp)) {
            nandroid_restore(backup_source, backup_boot, backup_system, backup_data, backup_cache, backup_sdext, backup_wimax);
        }
    }

    free(file);
}

/*
* custom backup and restore jobs:
    - At least one partition to restore must be selected
* restore jobs
    - modem.bin and radio.bin file types must be restored alone (available only in custom restore mode)
    - TWRP restore:
        + accepts efs to be restored along other partitions (but modem is never of bin type and preload is always 0)
    - Custom jobs:
        + efs must be restored alone (except for twrp backup jobs)
        + else, all other tasks can be processed
* backup jobs
    - if it is a twrp backup job, we can accept efs with other partitions
    - else, we only accept them separately for custom backup jobs    
*/
static void validate_backup_job(const char* backup_volume, int is_backup) {
    int sum = backup_boot + backup_recovery + backup_system + backup_preload + backup_data +
                backup_cache + backup_sdext + backup_wimax + backup_misc + backup_data_media;

    // add extra partitions to the sum
    int i;
    int extra_partitions_num = get_extra_partitions_state();
    for (i = 0; i < extra_partitions_num; ++i) {
        if (extra_partition[i].backup_state)
            ++sum;
    }

    if (0 == (sum + backup_efs + backup_modem + backup_radio)) {
        ui_print("Select at least one partition to restore!\n");
        return;
    }

    if (is_backup)
    {
        // it is a backup job to validate: ensure default backup handler is not dup before processing
        char backup_path[PATH_MAX] = "";
        ui_print_backup_list();
        int fmt = nandroid_get_default_backup_format();
        if (fmt != NANDROID_BACKUP_FORMAT_TAR && fmt != NANDROID_BACKUP_FORMAT_TGZ) {
            LOGE("Backup format must be tar(.gz)!\n");
        } else if (twrp_backup_mode.value) {
            get_twrp_backup_path(backup_volume, backup_path);
            twrp_backup(backup_path);
        } else if (backup_efs && (sum + backup_modem + backup_radio) != 0) {
            ui_print("efs must be backed up alone!\n");
        } else {
            get_cwm_backup_path(backup_volume, backup_path);
            nandroid_backup(backup_path);
        }
    }
    else {
        // it is a restore job
        if (backup_modem == RAW_BIN_FILE) {
            if (0 != (sum + backup_efs + backup_radio))
                ui_print("modem.bin format must be restored alone!\n");
            else
                custom_restore_handler(backup_volume, MODEM_BIN_PATH);
        }
        else if (backup_radio == RAW_BIN_FILE) {
            if (0 != (sum + backup_efs + backup_modem))
                ui_print("radio.bin format must be restored alone!\n");
            else
                custom_restore_handler(backup_volume, RADIO_BIN_PATH);
        }
        else if (twrp_backup_mode.value)
            show_twrp_restore_menu(backup_volume);
        else if (backup_efs && (sum + backup_modem + backup_radio) != 0)
            ui_print("efs must be restored alone!\n");
        else if (backup_efs && (sum + backup_modem + backup_radio) == 0)
            custom_restore_handler(backup_volume, EFS_BACKUP_PATH);
        else
            custom_restore_handler(backup_volume, CWM_BACKUP_PATH);
    }
}

// custom backup and restore top menu items
#define TOP_CUSTOM_JOB_MENU_ITEMS 16
enum {
  LIST_ITEM_VALIDATE,
  LIST_ITEM_REBOOT,
  LIST_ITEM_BOOT,
  LIST_ITEM_RECOVERY,
  LIST_ITEM_SYSTEM,
  LIST_ITEM_PRELOAD,
  LIST_ITEM_DATA,
  LIST_ITEM_ANDSEC,
  LIST_ITEM_CACHE,
  LIST_ITEM_SDEXT,
  LIST_ITEM_MODEM,
  LIST_ITEM_RADIO,
  LIST_ITEM_EFS,
  LIST_ITEM_MISC,
  LIST_ITEM_DATAMEDIA,
  LIST_ITEM_WIMAX,
};

void custom_restore_menu(const char* backup_volume) {
    const char* headers[] = {
            "Custom restore job from",
            backup_volume,
            NULL
    };

    int list_items_num = TOP_CUSTOM_JOB_MENU_ITEMS + MAX_EXTRA_NANDROID_PARTITIONS + 1;
    char* list[list_items_num];
    int i;
    for (i = 0; i < list_items_num; ++i) {
        list[i] = NULL;
    }

    char menu_item_tmp[MENU_MAX_COLS];
    char tmp[PATH_MAX];
    int extra_partitions_num = get_extra_partitions_state();

    is_custom_backup = 1;
    reset_custom_job_settings(1);
    for (;;)
    {
        for (i = 0; i < list_items_num; ++i) {
            if (list[i])
                free(list[i]);
                list[i] = NULL;
        }

        list[LIST_ITEM_VALIDATE] = strdup(">> Start Custom Restore Job");

        if (reboot_after_nandroid) ui_format_gui_menu(menu_item_tmp, ">> Reboot once done", "(x)");
        else ui_format_gui_menu(menu_item_tmp, ">> Reboot once done", "( )");
        list[LIST_ITEM_REBOOT] = strdup(menu_item_tmp);

        if (volume_for_path(BOOT_PARTITION_MOUNT_POINT) != NULL) {
            if (backup_boot) ui_format_gui_menu(menu_item_tmp, "Restore boot", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore boot", "( )");
            list[LIST_ITEM_BOOT] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_BOOT] = NULL;
        }

        if (volume_for_path("/recovery") != NULL) {
            if (backup_recovery) ui_format_gui_menu(menu_item_tmp, "Restore recovery", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore recovery", "( )");
            list[LIST_ITEM_RECOVERY] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_RECOVERY] = NULL;
        }

        if (backup_system) ui_format_gui_menu(menu_item_tmp, "Restore system", "(x)");
        else ui_format_gui_menu(menu_item_tmp, "Restore system", "( )");
        list[LIST_ITEM_SYSTEM] = strdup(menu_item_tmp);

        if (volume_for_path("/preload") != NULL) {
            if (backup_preload) ui_format_gui_menu(menu_item_tmp, "Restore preload", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore preload", "( )");
            list[LIST_ITEM_PRELOAD] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_PRELOAD] = NULL;
        }

        if (backup_data) ui_format_gui_menu(menu_item_tmp, "Restore data", "(x)");
        else ui_format_gui_menu(menu_item_tmp, "Restore data", "( )");
        list[LIST_ITEM_DATA] = strdup(menu_item_tmp);

        set_android_secure_path(tmp);
        if (backup_data && android_secure_ext)
            ui_format_gui_menu(menu_item_tmp, "Restore and-sec", DirName(tmp));
        else ui_format_gui_menu(menu_item_tmp, "Restore and-sec", "( )");
        list[LIST_ITEM_ANDSEC] = strdup(menu_item_tmp);

        if (backup_cache) ui_format_gui_menu(menu_item_tmp, "Restore cache", "(x)");
        else ui_format_gui_menu(menu_item_tmp, "Restore cache", "( )");
        list[LIST_ITEM_CACHE] = strdup(menu_item_tmp);

        if (backup_sdext) ui_format_gui_menu(menu_item_tmp, "Restore sd-ext", "(x)");
        else ui_format_gui_menu(menu_item_tmp, "Restore sd-ext", "( )");
        list[LIST_ITEM_SDEXT] = strdup(menu_item_tmp);

        if (volume_for_path("/modem") != NULL) {
            if (backup_modem == RAW_IMG_FILE)
                ui_format_gui_menu(menu_item_tmp, "Restore modem [.img]", "(x)");
            else if (backup_modem == RAW_BIN_FILE)
                ui_format_gui_menu(menu_item_tmp, "Restore modem [.bin]", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore modem", "( )");
            list[LIST_ITEM_MODEM] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_MODEM] = NULL;
        }

        if (volume_for_path("/radio") != NULL) {
            if (backup_radio == RAW_IMG_FILE)
                ui_format_gui_menu(menu_item_tmp, "Restore radio [.img]", "(x)");
            else if (backup_radio == RAW_BIN_FILE)
                ui_format_gui_menu(menu_item_tmp, "Restore radio [.bin]", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore radio", "( )");
            list[LIST_ITEM_RADIO] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_RADIO] = NULL;
        }

        if (volume_for_path("/efs") != NULL) {
            if (backup_efs == RESTORE_EFS_IMG)
                ui_format_gui_menu(menu_item_tmp, "Restore efs [.img]", "(x)");
            else if (backup_efs == RESTORE_EFS_TAR)
                ui_format_gui_menu(menu_item_tmp, "Restore efs [.tar]", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore efs", "( )");
            list[LIST_ITEM_EFS] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_EFS] = NULL;
        }

        if (volume_for_path("/misc") != NULL) {
            if (backup_misc) ui_format_gui_menu(menu_item_tmp, "Restore misc", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore misc", "( )");
            list[LIST_ITEM_MISC] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_MISC] = NULL;
        }

        if (is_data_media() && !twrp_backup_mode.value) {
            if (backup_data_media)
                ui_format_gui_menu(menu_item_tmp, "Restore /data/media", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore /data/media", "( )");
            list[LIST_ITEM_DATAMEDIA] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_DATAMEDIA] = NULL;
        }

        if (volume_for_path("/wimax") != NULL && !twrp_backup_mode.value) {
            if (backup_wimax)
                ui_format_gui_menu(menu_item_tmp, "Restore WiMax", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Restore WiMax", "( )");
            list[LIST_ITEM_WIMAX] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_WIMAX] = NULL;
        }

        // show extra partitions menu if available
        for (i = 0; i < extra_partitions_num; ++i)
        {
            if (volume_for_path(extra_partition[i].mount_point) != NULL) {
                sprintf(tmp, "Restore %s", extra_partition[i].mount_point);
                if (extra_partition[i].backup_state)
                    ui_format_gui_menu(menu_item_tmp, tmp, "(x)");
                else ui_format_gui_menu(menu_item_tmp, tmp, "( )");
                list[TOP_CUSTOM_JOB_MENU_ITEMS + i] = strdup(menu_item_tmp);
            } else {
                list[TOP_CUSTOM_JOB_MENU_ITEMS + i] = NULL;
            }
        }

        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item < 0)
            break;

        switch (chosen_item)
        {
            case LIST_ITEM_VALIDATE:
                validate_backup_job(backup_volume, 0);
                break;
            case LIST_ITEM_REBOOT:
                reboot_after_nandroid ^= 1;
                break;
            case LIST_ITEM_BOOT:
                backup_boot ^= 1;
                break;
            case LIST_ITEM_RECOVERY:
                backup_recovery ^= 1;
                break;
            case LIST_ITEM_SYSTEM:
                backup_system ^= 1;
                break;
            case LIST_ITEM_PRELOAD:
                backup_preload ^= 1;
                break;
            case LIST_ITEM_DATA:
                backup_data ^= 1;
                break;
            case LIST_ITEM_ANDSEC:
                // if !backup_data, it will not be processed in any-case by nandroid operations
                // if there are extra voldmanaged volumes, warn to force restore .android_secure from one of them
                ignore_android_secure ^= 1;
                if (!ignore_android_secure && get_num_extra_volumes() != 0)
                    ui_print("To force restore to 2nd storage, keep only one .android_secure folder\n");
                break;
            case LIST_ITEM_CACHE:
                backup_cache ^= 1;
                break;
            case LIST_ITEM_SDEXT:
                backup_sdext ^= 1;
                break;
            case LIST_ITEM_MODEM:
                backup_modem++;
                if (backup_modem > 2)
                    backup_modem = 0;
                if (twrp_backup_mode.value && backup_modem == RAW_BIN_FILE)
                    backup_modem = 0;
                break;
            case LIST_ITEM_RADIO:
                backup_radio++;
                if (backup_radio > 2)
                    backup_radio = 0;
                if (twrp_backup_mode.value && backup_radio == RAW_BIN_FILE)
                    backup_radio = 0;
                break;
            case LIST_ITEM_EFS:
                backup_efs++;
                if (backup_efs > 2)
                    backup_efs = 0;
                if (twrp_backup_mode.value && backup_efs == RESTORE_EFS_IMG)
                    backup_efs = 0;
                break;
            case LIST_ITEM_MISC:
                backup_misc ^= 1;
                break;
            case LIST_ITEM_DATAMEDIA:
                if (is_data_media() && !twrp_backup_mode.value)
                    backup_data_media ^= 1;
                break;
            case LIST_ITEM_WIMAX:
                if (!twrp_backup_mode.value)
                    backup_wimax ^= 1;
                break;
            default: // extra partitions toggle
                extra_partition[chosen_item - TOP_CUSTOM_JOB_MENU_ITEMS].backup_state ^= 1;
                break;
        }
    }

    for (i = 0; i < list_items_num; ++i) {
        if (list[i])
            free(list[i]);
    }
    is_custom_backup = 0;
    reset_custom_job_settings(0);
}

void custom_backup_menu(const char* backup_volume)
{
    const char* headers[] = {
            "Custom backup job to",
            backup_volume,
            NULL
    };

    int list_items_num = TOP_CUSTOM_JOB_MENU_ITEMS + MAX_EXTRA_NANDROID_PARTITIONS + 1;
    char* list[list_items_num];
    int i;
    for (i = 0; i < list_items_num; ++i) {
        list[i] = NULL;
    }

    char menu_item_tmp[MENU_MAX_COLS];
    char tmp[PATH_MAX];
    int extra_partitions_num = get_extra_partitions_state();

    is_custom_backup = 1;
    reset_custom_job_settings(1);
    for (;;)
    {
        for (i = 0; i < list_items_num; ++i) {
            if (list[i])
                free(list[i]);
                list[i] = NULL;
        }

        list[LIST_ITEM_VALIDATE] = strdup(">> Start Custom Backup Job");

        if (reboot_after_nandroid) ui_format_gui_menu(menu_item_tmp, ">> Reboot once done", "(x)");
        else ui_format_gui_menu(menu_item_tmp, ">> Reboot once done", "( )");
        list[LIST_ITEM_REBOOT] = strdup(menu_item_tmp);

        if (volume_for_path(BOOT_PARTITION_MOUNT_POINT) != NULL) {
            if (backup_boot) ui_format_gui_menu(menu_item_tmp, "Backup boot", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup boot", "( )");
            list[LIST_ITEM_BOOT] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_BOOT] = NULL;
        }

        if (volume_for_path("/recovery") != NULL) {
            if (backup_recovery) ui_format_gui_menu(menu_item_tmp, "Backup recovery", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup recovery", "( )");
            list[LIST_ITEM_RECOVERY] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_RECOVERY] = NULL;
        }

        if (backup_system) ui_format_gui_menu(menu_item_tmp, "Backup system", "(x)");
        else ui_format_gui_menu(menu_item_tmp, "Backup system", "( )");
        list[LIST_ITEM_SYSTEM] = strdup(menu_item_tmp);

        if (volume_for_path("/preload") != NULL) {
            if (backup_preload) ui_format_gui_menu(menu_item_tmp, "Backup preload", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup preload", "( )");
            list[LIST_ITEM_PRELOAD] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_PRELOAD] = NULL;
        }

        if (backup_data) ui_format_gui_menu(menu_item_tmp, "Backup data", "(x)");
        else ui_format_gui_menu(menu_item_tmp, "Backup data", "( )");
        list[LIST_ITEM_DATA] = strdup(menu_item_tmp);

        set_android_secure_path(tmp);
        if (backup_data && android_secure_ext)
            ui_format_gui_menu(menu_item_tmp, "Backup and-sec", DirName(tmp));
        else ui_format_gui_menu(menu_item_tmp, "Backup and-sec", "( )");
        list[LIST_ITEM_ANDSEC] = strdup(menu_item_tmp);

        if (backup_cache) ui_format_gui_menu(menu_item_tmp, "Backup cache", "(x)");
        else ui_format_gui_menu(menu_item_tmp, "Backup cache", "( )");
        list[LIST_ITEM_CACHE] = strdup(menu_item_tmp);

        if (backup_sdext) ui_format_gui_menu(menu_item_tmp, "Backup sd-ext", "(x)");
        else ui_format_gui_menu(menu_item_tmp, "Backup sd-ext", "( )");
        list[LIST_ITEM_SDEXT] = strdup(menu_item_tmp);

        if (volume_for_path("/modem") != NULL) {
            if (backup_modem == RAW_IMG_FILE)
                ui_format_gui_menu(menu_item_tmp, "Backup modem [.img]", "(x)");
            else if (backup_modem == RAW_BIN_FILE)
                ui_format_gui_menu(menu_item_tmp, "Backup modem [.bin]", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup modem", "( )");
            list[LIST_ITEM_MODEM] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_MODEM] = NULL;
        }

        if (volume_for_path("/radio") != NULL) {
            if (backup_radio == RAW_IMG_FILE)
                ui_format_gui_menu(menu_item_tmp, "Backup radio [.img]", "(x)");
            else if (backup_radio == RAW_BIN_FILE)
                ui_format_gui_menu(menu_item_tmp, "Backup radio [.bin]", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup radio", "( )");
            list[LIST_ITEM_RADIO] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_RADIO] = NULL;
        }

        if (volume_for_path("/efs") != NULL) {
            if (backup_efs == RESTORE_EFS_IMG)
                ui_format_gui_menu(menu_item_tmp, "Backup efs [.img]", "(x)");
            else if (backup_efs == RESTORE_EFS_TAR)
                ui_format_gui_menu(menu_item_tmp, "Backup efs [.tar]", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup efs", "( )");
            list[LIST_ITEM_EFS] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_EFS] = NULL;
        }

        if (volume_for_path("/misc") != NULL) {
            if (backup_misc) ui_format_gui_menu(menu_item_tmp, "Backup misc", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup misc", "( )");
            list[LIST_ITEM_MISC] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_MISC] = NULL;
        }

        if (is_data_media() && !twrp_backup_mode.value) {
            if (backup_data_media)
                ui_format_gui_menu(menu_item_tmp, "Backup /data/media", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup /data/media", "( )");
            list[LIST_ITEM_DATAMEDIA] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_DATAMEDIA] = NULL;
        }

        if (volume_for_path("/wimax") != NULL && !twrp_backup_mode.value) {
            if (backup_wimax)
                ui_format_gui_menu(menu_item_tmp, "Backup WiMax", "(x)");
            else ui_format_gui_menu(menu_item_tmp, "Backup WiMax", "( )");
            list[LIST_ITEM_WIMAX] = strdup(menu_item_tmp);
        } else {
            list[LIST_ITEM_WIMAX] = NULL;
        }

        // show extra partitions menu if available
        for (i = 0; i < extra_partitions_num; ++i)
        {
            if (volume_for_path(extra_partition[i].mount_point) != NULL) {
                sprintf(tmp, "Backup %s", extra_partition[i].mount_point);
                if (extra_partition[i].backup_state)
                    ui_format_gui_menu(menu_item_tmp, tmp, "(x)");
                else ui_format_gui_menu(menu_item_tmp, tmp, "( )");
                list[TOP_CUSTOM_JOB_MENU_ITEMS + i] = strdup(menu_item_tmp);
            } else {
                list[TOP_CUSTOM_JOB_MENU_ITEMS + i] = NULL;
            }
        }

        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item < 0)
            break;

        switch (chosen_item)
        {
            case LIST_ITEM_VALIDATE:
                validate_backup_job(backup_volume, 1);
                break;
            case LIST_ITEM_REBOOT:
                reboot_after_nandroid ^= 1;
                break;
            case LIST_ITEM_BOOT:
                backup_boot ^= 1;
                break;
            case LIST_ITEM_RECOVERY:
                backup_recovery ^= 1;
                break;
            case LIST_ITEM_SYSTEM:
                backup_system ^= 1;
                break;
            case LIST_ITEM_PRELOAD:
                backup_preload ^= 1;
                break;
            case LIST_ITEM_DATA:
                backup_data ^= 1;
                break;
            case LIST_ITEM_ANDSEC:
                // if !backup_data, it will not be processed in any-case by nandroid operations
                // if there are extra voldmanaged volumes, warn to force backup .android_secure from one of them
                ignore_android_secure ^= 1;
                if (!ignore_android_secure && get_num_extra_volumes() != 0)
                    ui_print("To force backup from 2nd storage, keep only one .android_secure folder\n");
                break;
            case LIST_ITEM_CACHE:
                backup_cache ^= 1;
                break;
            case LIST_ITEM_SDEXT:
                backup_sdext ^= 1;
                break;
            case LIST_ITEM_MODEM:
                backup_modem ^= 1;
                break;
            case LIST_ITEM_RADIO:
                backup_radio ^= 1;
                break;
            case LIST_ITEM_EFS:
                backup_efs ^= 1;
                break;
            case LIST_ITEM_MISC:
                backup_misc ^= 1;
                break;
            case LIST_ITEM_DATAMEDIA:
                if (is_data_media() && !twrp_backup_mode.value)
                    backup_data_media ^= 1;
                break;
            case LIST_ITEM_WIMAX:
                if (!twrp_backup_mode.value)
                    backup_wimax ^= 1;
                break;
            default: // extra partitions toggle
                extra_partition[chosen_item - TOP_CUSTOM_JOB_MENU_ITEMS].backup_state ^= 1;
                break;
        }
    }

    for (i = 0; i < list_items_num; ++i) {
        if (list[i])
            free(list[i]);
    }
    is_custom_backup = 0;
    reset_custom_job_settings(0);
}
//------- end Custom Backup and Restore functions

// TWRP backup path and device ID generation
static void sanitize_device_id(char *device_id) {
    const char* whitelist = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-._";
    char tmp[PROPERTY_VALUE_MAX];
    char str[PROPERTY_VALUE_MAX];
    char* c = str;

    strcpy(str, device_id);
    memset(tmp, 0, sizeof(tmp));
    while (*c) {
        if (strchr(whitelist, *c))
            strncat(tmp, c, 1);
        ++c;
    }
    strcpy(device_id, tmp);
    return;
}

#define CMDLINE_SERIALNO        "androidboot.serialno="
#define CMDLINE_SERIALNO_LEN    (strlen(CMDLINE_SERIALNO))
#define CPUINFO_SERIALNO        "Serial"
#define CPUINFO_SERIALNO_LEN    (strlen(CPUINFO_SERIALNO))
#define CPUINFO_HARDWARE        "Hardware"
#define CPUINFO_HARDWARE_LEN    (strlen(CPUINFO_HARDWARE))

void get_device_id(char *device_id) {
#ifdef TW_USE_MODEL_HARDWARE_ID_FOR_DEVICE_ID
    // Now we'll use product_model_hardwareid as device id
    char model_id[PROPERTY_VALUE_MAX];
    property_get("ro.product.model", model_id, "error");
    if (strcmp(model_id, "error") != 0) {
        LOGI("=> product model: '%s'\n", model_id);
        // Replace spaces with underscores
        size_t i;
        for (i = 0; i < strlen(model_id); i++) {
            if (model_id[i] == ' ')
                model_id[i] = '_';
        }
        strcpy(device_id, model_id);
        sanitize_device_id(device_id);
        LOGI("=> using product model for device id: '%s'\n", device_id);
        return;
    }
#endif

    // First try system properties
    property_get("ro.serialno", device_id, "");
    if (strlen(device_id) != 0) {
        sanitize_device_id(device_id);
        LOGI("Using ro.serialno='%s'\n", device_id);
        return;
    }
    property_get("ro.boot.serialno", device_id, "");
    if (strlen(device_id) != 0) {
        sanitize_device_id(device_id);
        LOGI("Using ro.boot.serialno='%s'\n", device_id);
        return;
    }

    // device_id not found, looking elsewhere
    FILE *fp;
    char line[2048];
    char hardware_id[32];
    char* token;

    // Assign a blank device_id to start with
    device_id[0] = 0;

    // Try the cmdline to see if the serial number was supplied
    fp = fopen("/proc/cmdline", "rt");
    if (fp != NULL) {
        // First step, read the line. For cmdline, it's one long line
        LOGI("Checking cmdline for serialno...\n");
        fgets(line, sizeof(line), fp);
        fclose(fp);

        // Now, let's tokenize the string
        token = strtok(line, " ");
        if (strlen(token) > PROPERTY_VALUE_MAX)
            token[PROPERTY_VALUE_MAX] = 0;

        // Let's walk through the line, looking for the CMDLINE_SERIALNO token
        while (token) {
            // We don't need to verify the length of token, because if it's too short, it will mismatch CMDLINE_SERIALNO at the NULL
            if (memcmp(token, CMDLINE_SERIALNO, CMDLINE_SERIALNO_LEN) == 0) {
                // We found the serial number!
                strcpy(device_id, token + CMDLINE_SERIALNO_LEN);
                sanitize_device_id(device_id);
                LOGI("Using serialno='%s'\n", device_id);
                return;
            }
            token = strtok(NULL, " ");
        }
    }

    // Now we'll try cpuinfo for a serial number (we shouldn't reach here as it gives wired output)
    fp = fopen("/proc/cpuinfo", "rt");
    if (fp != NULL) {
        LOGI("Checking cpuinfo...\n");
        // First step, read the line.
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (memcmp(line, CPUINFO_SERIALNO, CPUINFO_SERIALNO_LEN) == 0) {
                // check the beginning of the line for "Serial"
                // We found the serial number!
                token = line + CPUINFO_SERIALNO_LEN; // skip past "Serial"
                while ((*token > 0 && *token <= 32 ) || *token == ':') {
                    // skip over all spaces and the colon
                    token++;
                }

                if (*token != 0) {
                    token[30] = 0;
                    if (token[strlen(token)-1] == 10) {
                        // checking for endline chars and dropping them from the end of the string if needed
                        char tmp[PROPERTY_VALUE_MAX];
                        memset(tmp, 0, sizeof(tmp));
                        strncpy(tmp, token, strlen(token) - 1);
                        strcpy(device_id, tmp);
                    } else {
                        strcpy(device_id, token);
                    }
                    fclose(fp);
                    sanitize_device_id(device_id);
                    LOGI("=> Using cpuinfo serialno: '%s'\n", device_id);
                    return;
                }
            } else if (memcmp(line, CPUINFO_HARDWARE, CPUINFO_HARDWARE_LEN) == 0) {
                // We're also going to look for the hardware line in cpuinfo and save it for later in case we don't find the device ID
                // We found the hardware ID
                token = line + CPUINFO_HARDWARE_LEN; // skip past "Hardware"
                while ((*token > 0 && *token <= 32 ) || *token == ':') {
                    // skip over all spaces and the colon
                    token++;
                }

                if (*token != 0) {
                    token[30] = 0;
                    if (token[strlen(token)-1] == 10) { // checking for endline chars and dropping them from the end of the string if needed
                        memset(hardware_id, 0, sizeof(hardware_id));
                        strncpy(hardware_id, token, strlen(token) - 1);
                    } else {
                        strcpy(hardware_id, token);
                    }
                    LOGI("=> hardware id from cpuinfo: '%s'\n", hardware_id);
                }
            }
        }
        fclose(fp);
    }

    if (hardware_id[0] != 0) {
        LOGW("\nusing hardware id for device id: '%s'\n", hardware_id);
        strcpy(device_id, hardware_id);
        sanitize_device_id(device_id);
        return;
    }

    strcpy(device_id, "serialno");
    LOGE("=> device id not found, using '%s'\n", device_id);
    return;
}
// End of Device ID functions

void get_twrp_backup_path(const char* backup_volume, char *backup_path) {
    char rom_name[PROPERTY_VALUE_MAX] = "noname";
    get_rom_name(rom_name);

    char device_id[PROPERTY_VALUE_MAX];
    get_device_id(device_id);

    time_t t = time(NULL);
    struct tm *timeptr = localtime(&t);
    if (timeptr == NULL) {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        sprintf(backup_path, "%s/%s/%s/%ld_%s", backup_volume, TWRP_BACKUP_PATH, device_id, tp.tv_sec, rom_name);
    } else {
        char tmp[PATH_MAX];
        strftime(tmp, sizeof(tmp), "%F.%H.%M.%S", timeptr);
        // this sprintf results in:
        // clockworkmod/backup/%F.%H.%M.%S (time values are populated too)
        sprintf(backup_path, "%s/%s/%s/%s_%s", backup_volume, TWRP_BACKUP_PATH, device_id, tmp, rom_name);
    }
}
//-------------- End PhilZ Touch Special Backup and Restore menu and handlers

// launch aromafm.zip from default locations
static int default_aromafm(const char* root) {
    char aroma_file[PATH_MAX];
    sprintf(aroma_file, "%s/%s", root, AROMA_FM_PATH);

    if (file_found(aroma_file)) {
        // will ensure_path_mounted(aroma_file)
#ifdef PHILZ_TOUCH_RECOVERY
        force_wait = -1;
#endif
        install_zip(aroma_file);
        return 0;
    }
    return -1;
}

void run_aroma_browser() {
    // look for AROMA_FM_PATH in storage paths
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();
    int ret = -1;
    int i = 0;

    // vold managed volumes need to be mounted as aromafm cannot mount them (they are not set into /etc/fstab)
    vold_mount_all();
    ret = default_aromafm(get_primary_storage_path());
    if (extra_paths != NULL) {
        i = 0;
        while (ret && i < num_extra_volumes) {
            ret = default_aromafm(extra_paths[i]);
            ++i;
        }
        free_string_array(extra_paths);
    }
    if (ret != 0)
        ui_print("No %s in storage paths\n", AROMA_FM_PATH);
}
//------ end aromafm launcher functions


//import / export recovery and theme settings
static void load_theme_settings() {
#ifdef PHILZ_TOUCH_RECOVERY
    selective_load_theme_settings();
#else
    const char* headers[] = { "Select a theme to load", "", NULL };

    char themes_dir[PATH_MAX];
    char* theme_file;

    sprintf(themes_dir, "%s/%s", get_primary_storage_path(), PHILZ_THEMES_PATH);
    if (0 != ensure_path_mounted(themes_dir))
        return;

    theme_file = choose_file_menu(themes_dir, ".ini", headers);
    if (theme_file == NULL)
        return;

    if (confirm_selection("Overwrite default settings ?", "Yes - Apply New Theme") && copy_a_file(theme_file, PHILZ_SETTINGS_FILE) == 0) {
        char settings_copy[PATH_MAX];
        sprintf(settings_copy, "%s/%s", get_primary_storage_path(), PHILZ_SETTINGS_FILE2);
        copy_a_file(theme_file, settings_copy);
        refresh_recovery_settings(0);
        ui_print("loaded default settings from %s\n", BaseName(theme_file));
    }

    free(theme_file);
#endif
}

static void import_export_settings() {
    const char* headers[] = { "Save / Restore Settings", "", NULL };

    char* list[] = {
        "Backup Recovery Settings to sdcard",
        "Restore Recovery Settings from sdcard",
        "Save Current Theme to sdcard",
        "Load Existing Theme from sdcard",
        "Delete Saved Themes",
        NULL
    };

    char backup_file[PATH_MAX];
    char themes_dir[PATH_MAX];
    sprintf(backup_file, "%s/%s", get_primary_storage_path(), PHILZ_SETTINGS_BAK);
    sprintf(themes_dir, "%s/%s", get_primary_storage_path(), PHILZ_THEMES_PATH);

    for (;;) {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item) {
            case 0: {
                if (copy_a_file(PHILZ_SETTINGS_FILE, backup_file) == 0)
                    ui_print("config file successfully backed up to %s\n", backup_file);
                break;
            }
            case 1: {
                if (copy_a_file(backup_file, PHILZ_SETTINGS_FILE) == 0) {
                    char settings_copy[PATH_MAX];
                    sprintf(settings_copy, "%s/%s", get_primary_storage_path(), PHILZ_SETTINGS_FILE2);
                    copy_a_file(backup_file, settings_copy);
                    refresh_recovery_settings(0);
                    ui_print("settings loaded from %s\n", backup_file);
                }
                break;
            }
            case 2: {
                int ret = 1;
                int i = 1;
                char path[PATH_MAX];
                while (ret && i < 10) {
                    sprintf(path, "%s/theme_%03i.ini", themes_dir, i);
                    ret = file_found(path);
                    ++i;
                }

                if (ret)
                    LOGE("Can't save more than 10 themes!\n");
                else if (copy_a_file(PHILZ_SETTINGS_FILE, path) == 0)
                    ui_print("Custom settings saved to %s\n", path);
                break;
            }
            case 3: {
                load_theme_settings();
                break;
            }
            case 4: {
                ensure_path_mounted(themes_dir);
                char* theme_file = choose_file_menu(themes_dir, ".ini", headers);
                if (theme_file == NULL)
                    break;
                if (confirm_selection("Delete selected theme ?", "Yes - Delete"))
                    delete_a_file(theme_file);
                free(theme_file);
                break;
            }
        }
    }
}

void show_philz_settings_menu()
{
    const char* headers[] = { "Recovery Settings", NULL };

    char item_check_root_and_recovery[MENU_MAX_COLS];
    char item_auto_restore[MENU_MAX_COLS];

    char* list[] = {
        item_check_root_and_recovery,
        item_auto_restore,
        "Save and Restore Settings",
        "Reset All Recovery Settings",
        "Setup Recovery Lock",
        "GUI Preferences",
        "About",
        NULL
    };

    for (;;) {
        if (check_root_and_recovery.value)
            ui_format_gui_menu(item_check_root_and_recovery, "Verify Root on Exit", "(x)");
        else ui_format_gui_menu(item_check_root_and_recovery, "Verify Root on Exit", "( )");

        if (auto_restore_settings.value)
            ui_format_gui_menu(item_auto_restore, "Auto Restore Settings", "(x)");
        else ui_format_gui_menu(item_auto_restore, "Auto Restore Settings", "( )");

        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK)
            break;

        switch (chosen_item) {
            case 0: {
                char value[5];
                check_root_and_recovery.value ^= 1;
                sprintf(value, "%d", check_root_and_recovery.value);
                write_config_file(PHILZ_SETTINGS_FILE, check_root_and_recovery.key, value);
                break;
            }
            case 1: {
                char value[5];
                auto_restore_settings.value ^= 1;
                sprintf(value, "%d", auto_restore_settings.value);
                write_config_file(PHILZ_SETTINGS_FILE, auto_restore_settings.key, value);
                break;
            }
            case 2: {
                import_export_settings();
                break;
            }
            case 3: {
                if (confirm_selection("Reset all recovery settings?", "Yes - Reset to Defaults")) {
                    char settings_copy[PATH_MAX];
                    sprintf(settings_copy, "%s/%s", get_primary_storage_path(), PHILZ_SETTINGS_FILE2);
                    delete_a_file(PHILZ_SETTINGS_FILE);
                    delete_a_file(settings_copy);
                    refresh_recovery_settings(0);
                    ui_print("All settings reset to default!\n");
                }
                break;
            }
#ifdef PHILZ_TOUCH_RECOVERY
            case 4: {
                show_recovery_lock_menu();
                break;
            }
            case 5: {
                show_touch_gui_menu();
                break;
            }
#endif
            case 6: {
                ui_print(EXPAND(RECOVERY_MOD_VERSION) "\n");
                ui_print("Build version: " EXPAND(PHILZ_BUILD) " - " EXPAND(TARGET_COMMON_NAME) "\n");
                ui_print("CWM Base version: " EXPAND(CWM_BASE_VERSION) "\n");
#ifdef PHILZ_TOUCH_RECOVERY
                print_libtouch_version(1);
#endif
                //ui_print(EXPAND(BUILD_DATE)"\n");
                ui_print("Compiled %s at %s\n", __DATE__, __TIME__);
                break;
            }
        }
    }
}
//---------------- End PhilZ Menu settings and functions

static void write_last_install_path(const char* install_path) {
    char path[PATH_MAX];
    sprintf(path, "%s/%s", get_primary_storage_path(), RECOVERY_LAST_INSTALL_FILE);
    write_string_to_file(path, install_path);
}

const char* read_last_install_path() {
    static char path[PATH_MAX];
    sprintf(path, "%s/%s", get_primary_storage_path(), RECOVERY_LAST_INSTALL_FILE);

    ensure_path_mounted(path);
    FILE *f = fopen(path, "r");
    if (f != NULL) {
        fgets(path, PATH_MAX, f);
        fclose(f);

        return path;
    }
    return NULL;
}

// top fixed menu items, those before extra storage volumes
#define FIXED_TOP_INSTALL_ZIP_MENUS 1
// bottom fixed menu items, those after extra storage volumes
#define FIXED_BOTTOM_INSTALL_ZIP_MENUS 7
#define FIXED_INSTALL_ZIP_MENUS (FIXED_TOP_INSTALL_ZIP_MENUS + FIXED_BOTTOM_INSTALL_ZIP_MENUS)

int show_install_update_menu() {
    char buf[100];
    int i = 0, chosen_item = 0;
    // + 1 for last NULL item
    char* install_menu_items[MAX_NUM_MANAGED_VOLUMES + FIXED_INSTALL_ZIP_MENUS + 1];

    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();

    memset(install_menu_items, 0, sizeof(install_menu_items));

    const char* headers[] = { "Install update from zip file", "", NULL };

    // FIXED_TOP_INSTALL_ZIP_MENUS
    sprintf(buf, "Choose zip from %s", primary_path);
    install_menu_items[0] = strdup(buf);

    // extra storage volumes (vold managed)
    for (i = 0; i < num_extra_volumes; i++) {
        sprintf(buf, "Choose zip from %s", extra_paths[i]);
        install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + i] = strdup(buf);
    }

    // FIXED_BOTTOM_INSTALL_ZIP_MENUS
    char item_toggle_signature_check[MENU_MAX_COLS] = "";
    char item_install_zip_verify_md5[MENU_MAX_COLS] = "";
    install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes]     = "Choose zip Using Free Browse Mode";
    install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 1] = "Choose zip from Last Install Folder";
    install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 2] = "Install zip from sideload";
    install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 3] = "Install Multiple zip Files";
    install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 4] = item_toggle_signature_check;
    install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 5] = item_install_zip_verify_md5;
    install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 6] = "Setup Free Browse Mode";

    // extra NULL for GO_BACK
    install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + FIXED_BOTTOM_INSTALL_ZIP_MENUS] = NULL;

    for (;;) {
        if (signature_check_enabled.value)
            ui_format_gui_menu(item_toggle_signature_check, "Signature Verification", "(x)");
        else ui_format_gui_menu(item_toggle_signature_check, "Signature Verification", "( )");

        if (install_zip_verify_md5.value)
            ui_format_gui_menu(item_install_zip_verify_md5, "Verify zip md5sum", "(x)");
        else ui_format_gui_menu(item_install_zip_verify_md5, "Verify zip md5sum", "( )");

        chosen_item = get_menu_selection(headers, install_menu_items, 0, 0);
        if (chosen_item == 0) {
            show_choose_zip_menu(primary_path);
        } else if (chosen_item >= FIXED_TOP_INSTALL_ZIP_MENUS && chosen_item < FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes) {
            show_choose_zip_menu(extra_paths[chosen_item - FIXED_TOP_INSTALL_ZIP_MENUS]);
        } else if (chosen_item == FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes) {
            // browse for zip files up/backward including root system and have a default user set start folder
            if (show_custom_zip_menu() != 0)
                set_custom_zip_path();
        } else if (chosen_item == FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 1) {
            const char *last_path_used = read_last_install_path();
            if (last_path_used == NULL)
                show_choose_zip_menu(primary_path);
            else
                show_choose_zip_menu(last_path_used);
        } else if (chosen_item == FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 2) {
            enter_sideload_mode(INSTALL_SUCCESS);
        } else if (chosen_item == FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 3) {
            show_multi_flash_menu();
        } else if (chosen_item == FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 4) {
            toggle_signature_check();
        } else if (chosen_item == FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 5) {
            toggle_install_zip_verify_md5();
        } else if (chosen_item == FIXED_TOP_INSTALL_ZIP_MENUS + num_extra_volumes + 6) {
            set_custom_zip_path();
        } else {
            // GO_BACK or REFRESH (chosen_item < 0)
            goto out;
        }
    }
out:
    // free all the dynamic items
    free(install_menu_items[0]);
    if (extra_paths != NULL) {
        free_string_array(extra_paths);
        for (i = 0; i < num_extra_volumes; i++)
            free(install_menu_items[FIXED_TOP_INSTALL_ZIP_MENUS + i]);
    }
    return chosen_item;
}

void show_choose_zip_menu(const char *mount_point) {
    if (ensure_path_mounted(mount_point) != 0) {
        LOGE("Can't mount %s\n", mount_point);
        return;
    }

    const char* headers[] = { "Choose a zip to apply", NULL };

    char* file = choose_file_menu(mount_point, ".zip", headers);
    if (file == NULL)
        return;

    char tmp[PATH_MAX];
    int yes_confirm;

    sprintf(tmp, "Yes - Install %s", BaseName(file));
    if (install_zip_verify_md5.value) start_md5_verify_thread(file);
    else start_md5_display_thread(file);

    yes_confirm = confirm_selection("Confirm install?", tmp);

    if (install_zip_verify_md5.value) stop_md5_verify_thread();
    else stop_md5_display_thread();

    if (yes_confirm) {
        install_zip(file);
        sprintf(tmp, "%s", DirName(file));
        write_last_install_path(tmp);
    }

    free(file);
}

void show_nandroid_restore_menu(const char* path) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return;
    }

    const char* headers[] = { "Choose an image to restore", NULL };

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/backup/", path);
    char* file = choose_file_menu(tmp, NULL, headers);
    if (file == NULL)
        return;

    if (confirm_selection("Confirm restore?", "Yes - Restore"))
        nandroid_restore(file, 1, 1, 1, 1, 1, 0);

    free(file);
}

void show_nandroid_delete_menu(const char* volume_path) {
    if (ensure_path_mounted(volume_path) != 0) {
        LOGE("Can't mount %s\n", volume_path);
        return;
    }

    const char* headers[] = { "Choose a backup to delete", NULL };
    char path[PATH_MAX];
    char tmp[PATH_MAX];
    char* file;

    if (twrp_backup_mode.value) {
        char device_id[PROPERTY_VALUE_MAX];
        get_device_id(device_id);
        sprintf(path, "%s/%s/%s", volume_path, TWRP_BACKUP_PATH, device_id);
    } else {
        sprintf(path, "%s/%s", volume_path, CWM_BACKUP_PATH);    
    }

    for(;;) {
        file = choose_file_menu(path, NULL, headers);
        if (file == NULL)
            return;

        sprintf(tmp, "Yes - Delete %s", BaseName(file));
        if (confirm_selection("Confirm delete?", tmp)) {
            sprintf(tmp, "rm -rf '%s'", file);
            __system(tmp);
        }

        free(file);
    }
}

/****************************/
/* Format and mount options */
/****************************/
#define LUN_FILE_EXPANDS    2

struct lun_node {
    const char *lun_file;
    struct lun_node *next;
};

static struct lun_node *lun_head = NULL;
static struct lun_node *lun_tail = NULL;

static bool vold_volume_uses_legacy_lun(Volume* vol) {
    if (vol != NULL &&
            fs_mgr_is_voldmanaged(vol) && vold_is_volume_available(vol->mount_point) &&
            vol->blk_device2 != NULL && strcmp(vol->blk_device2, vol->blk_device) != 0)
        return true;
    return false;
}

static int control_usb_storage_set_lun(Volume* vol, const char *lun_file, bool enable) {
    struct lun_node *node;
    FILE* fp;

    // verify that we have not already used this LUN file
    for (node = lun_head; node; node = node->next) {
        if (strcmp(node->lun_file, lun_file) == 0) {
            // skip any LUN files that are already in use
            return -1;
        }
    }

    // open a handle to the LUN file. If !enable, it will discard its contents and we do not write it
    LOGI("Trying %s on LUN file %s\n", vol->mount_point, lun_file);
    if ((fp = fopen(lun_file, "w")) == NULL) {
        LOGW("Unable to open ums lunfile %s (%s)\n", lun_file, strerror(errno));
        return -1;
    }

    // write the volume path to the LUN file
    struct statfs info;
    char* device = vol->blk_device;
    if (enable) {
        if (statfs(device, &info) != 0 || fwrite(device, 1, strlen(device), fp) != strlen(device)) {
            device = vol->blk_device2;
            if (device == NULL || statfs(device, &info) != 0 || fwrite(device, 1, strlen(device), fp) != strlen(device)) {
                LOGW("Unable to write to ums lunfile %s (%s)\n", lun_file, strerror(errno));
                fclose(fp);
                return -1;
            }
        }
    }
    fclose(fp);

    // volume path to LUN association succeeded
    // save off a record of this lun_file being in use now
    node = (struct lun_node *)malloc(sizeof(struct lun_node));
    node->lun_file = strdup(lun_file);
    node->next = NULL;
    if (lun_head == NULL)
       lun_head = lun_tail = node;
    else {
       lun_tail->next = node;
       lun_tail = node;
    }

    LOGI("Successfully %s '%s' on LUN file '%s'\n", enable ? "shared" : "unshared", enable ? device : vol->mount_point, lun_file);
    ui_print("%s (%s).\n", vol->mount_point, enable ? "shared" : "unshared");
    return 0;
}

static int control_usb_storage_for_lun(Volume* vol, bool enable) {
    if (vol == NULL || ensure_path_unmounted(vol->mount_point) != 0)
        return -1;

    const char* lun_files[] = {
#ifdef BOARD_UMS_LUNFILE
        BOARD_UMS_LUNFILE,
#endif
#ifdef TARGET_USE_CUSTOM_LUN_FILE_PATH
        TARGET_USE_CUSTOM_LUN_FILE_PATH,
#endif
        "/sys/class/android_usb/android0/f_mass_storage/lun0/file",
        "/sys/class/android_usb/android%d/f_mass_storage/lun/file",
        "/sys/class/android_usb/android0/f_mass_storage/lun_ex/file",
        "/sys/devices/platform/usb_mass_storage/lun%d/file",
        "/sys/devices/platform/msm_hsusb/gadget/lun0/file",
        "/sys/devices/virtual/android_usb/android0/f_mass_storage/lun",
        NULL
    };

    // if recovery.fstab specifies a LUN file, use it (vol->lun no longer parsed by fs_mgr in kitkat)
    if (vol->lun) {
        return control_usb_storage_set_lun(vol, vol->lun, enable);
    }

    // try to find a LUN for this volume
    //   - iterate through the lun file paths
    //   - expand any %d by LUN_FILE_EXPANDS
    int lun_num = 0;
    int i;
    for (i = 0; lun_files[i]; i++) {
        const char *lun_file = lun_files[i];
        for(lun_num = 0; lun_num < LUN_FILE_EXPANDS; ++lun_num) {
            char formatted_lun_file[255];
    
            // replace %d with the LUN number
            bzero(formatted_lun_file, 255);
            snprintf(formatted_lun_file, 254, lun_file, lun_num);
    
            // attempt to use the LUN file
            if (control_usb_storage_set_lun(vol, formatted_lun_file, enable) == 0) {
                return 0;
            }
        }
    }

    // all LUNs were exhausted and none worked
    LOGW("Could not %s %s on LUN %d\n", enable ? "enable" : "disable", vol->blk_device, lun_num);

    return -1;  // -1 failure, 0 success
}

// Enable USB storage
// if we have a valid blk_device2, try to use legacy mode rather than vold to share the volume
// this improves transfer speed up to 10x depending on the file system
static int control_usb_storage(bool enable) {
    int i = 0;
    int num = 0;
    int num_extra_volumes = get_num_extra_volumes();
    char** extra_paths = get_extra_storage_paths();
    Volume* vol[num_extra_volumes + 1];

    // initialisze volumes list to NULL (extra volumes + primary volume)
    for (i = 0; i < num_extra_volumes + 1; ++i) {
        vol[i] = NULL;
    }

    // assign vol list to extra storage volumes
    i = 0;
    if (extra_paths != NULL) {
        for (i = 0; i < num_extra_volumes; ++i) {
            vol[i] = volume_for_path(extra_paths[i]);
        }
        free_string_array(extra_paths);
    }

    // assign primary storage volume if it is a physical volume
    vol[i] = volume_for_path(get_primary_storage_path());
    if (!is_volume_primary_storage(vol[i]))
        vol[i] = NULL;

    for (i = 0; i < num_extra_volumes + 1; ++i) {
        if (vol[i] == NULL)
            continue;

        if (fs_mgr_is_voldmanaged(vol[i])) {
            if (!vold_is_volume_available(vol[i]->mount_point))
                continue;

            // if we have a valid blk_device2, try to use legacy mode rather than vold to share the volume
            if (vold_volume_uses_legacy_lun(vol[i])) {
                if (control_usb_storage_for_lun(vol[i], enable) == 0)
                    ++num;
                continue;
            } else if (enable) {
                vold_share_volume(vol[i]->mount_point);
            } else {
                vold_unshare_volume(vol[i]->mount_point, 1);
            }
            ++num;
        } else if (control_usb_storage_for_lun(vol[i], enable) == 0) {
            ++num;
        }        
    }
    
    // Release memory used by the LUN file linked list
    struct lun_node *node = lun_head;
    while (node) {
       struct lun_node *next = node->next;
       free((void *)node->lun_file);
       free(node);
       node = next;
    }
    lun_head = lun_tail = NULL;

    property_set("sys.storage.ums_enabled", enable && num ? "1" : "0");

    return num != 0;
}

static void show_mount_usb_storage_menu() {
    if (!control_usb_storage(true))
        return;

    const char* headers[] = {
        "USB Mass Storage device",
        "Leaving this menu unmounts",
        "your SD card from your PC.",
        "",
        NULL
    };

    char* list[] = { "Unmount", NULL };

    for (;;) {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK || chosen_item == 0)
            break;
    }

    // Disable USB storage
    control_usb_storage(false);
}

// ext4 <-> f2fs conversion
#ifdef USE_F2FS
static void show_format_ext4_or_f2fs_menu(const char* volume) {
    if (is_data_media_volume_path(volume))
        return;

    Volume* v = volume_for_path(volume);
    if (v == NULL)
        return;

    const char* headers[] = { "Format device:", v->mount_point, "", NULL };

    char* list[] = {
        "default",
        "ext4",
        "f2fs",
        NULL
    };

    char cmd[PATH_MAX];
    char confirm[128];
    int ret;
    int chosen_item = get_menu_selection(headers, list, 0, 0);

    if (chosen_item < 0) // REFRESH or GO_BACK
        return;

    sprintf(confirm, "Format %s (%s) ?", v->mount_point, list[chosen_item]);
    if (is_data_media() && strcmp(v->mount_point, "/data") == 0) {
        if (is_data_media_preserved()) {
            // code call error
            LOGE("cannot convert partition without wiping whole data!");
            return;
        }

        const char* confirm_headers[] = {
            confirm,
            "   this will wipe /sdcard",
            "   (/data/media storage)",
            "",
            NULL
        };
        ret = confirm_with_headers(confirm_headers, "Yes - Format device");
    } else {
        ret = confirm_selection(confirm, "Yes - Format device");
    }

    if (ret != true)
        return;

    if (ensure_path_unmounted(v->mount_point) != 0)
        return;

    switch (chosen_item) {
        case 0:
            ret = format_volume(v->mount_point);
            break;
        case 1:
        case 2:
            ret = format_device(v->blk_device, v->mount_point, list[chosen_item]);
            break;
    }

    // refresh volume table (Volume*) and recreate the /etc/fstab file for proper system mount command function
    load_volume_table();
    setup_data_media(1);

    if (ret)
        LOGE("Could not format %s (%s)\n", volume, list[chosen_item]);
    else
        ui_print("Done formatting %s (%s)\n", volume, list[chosen_item]);
}
#endif

typedef struct {
    char mount[255];
    char unmount[255];
    char path[PATH_MAX];
} MountMenuEntry;

typedef struct {
    char txt[255];
    char path[PATH_MAX];
    char type[255];
} FormatMenuEntry;

typedef struct {
    char *path;
    char *type;
    int can_mount;
    int can_format;
} MFMatrix;

static void get_mnt_fmt_capabilities(MFMatrix *mfm) {
    const int NUM_MNT_PNTS = 6;
    const int NUM_FS_TYPES = 6;
    MFMatrix mp_matrix[NUM_MNT_PNTS];
    MFMatrix fs_matrix[NUM_FS_TYPES];

    // Defined capabilities:   fs_type     mnt fmt
    fs_matrix[0] = (MFMatrix){ NULL,  "bml",       0,  1 };
    fs_matrix[1] = (MFMatrix){ NULL,  "datamedia", 0,  1 };
    fs_matrix[2] = (MFMatrix){ NULL,  "emmc",      0,  1 };
    fs_matrix[3] = (MFMatrix){ NULL,  "mtd",       0,  0 };
    fs_matrix[4] = (MFMatrix){ NULL,  "ramdisk",   0,  0 };
    fs_matrix[5] = (MFMatrix){ NULL,  "swap",      0,  0 };

    // Defined capabilities:   mount_point   mnt fmt
    mp_matrix[0] = (MFMatrix){ "/misc",       NULL,   0,  0 };
    mp_matrix[1] = (MFMatrix){ "/radio",      NULL,   0,  0 };
    mp_matrix[2] = (MFMatrix){ "/bootloader", NULL,   0,  0 };
    mp_matrix[3] = (MFMatrix){ "/recovery",   NULL,   0,  0 };
    mp_matrix[4] = (MFMatrix){ "/efs",        NULL,   0,  0 };
    mp_matrix[5] = (MFMatrix){ "/wimax",      NULL,   0,  0 };

    int i;
    for (i = 0; i < NUM_FS_TYPES; i++) {
        if (strcmp(mfm->type, fs_matrix[i].type) == 0) {
            mfm->can_mount = fs_matrix[i].can_mount;
            mfm->can_format = fs_matrix[i].can_format;
        }
    }
    for (i = 0; i < NUM_MNT_PNTS; i++) {
        if (strcmp(mfm->path, mp_matrix[i].path) == 0) {
            mfm->can_mount = mp_matrix[i].can_mount;
            mfm->can_format = mp_matrix[i].can_format;
        }
    }

    // User-defined capabilities
    char *custom_mp;
    char custom_forbidden_mount[PROPERTY_VALUE_MAX];
    char custom_forbidden_format[PROPERTY_VALUE_MAX];
    property_get("ro.cwm.forbid_mount", custom_forbidden_mount, "");
    property_get("ro.cwm.forbid_format", custom_forbidden_format, "");

    custom_mp = strtok(custom_forbidden_mount, ",");
    while (custom_mp != NULL) {
        if (strcmp(mfm->path, custom_mp) == 0) {
            mfm->can_mount = 0;
        }
        custom_mp = strtok(NULL, ",");
    }

    custom_mp = strtok(custom_forbidden_format, ",");
    while (custom_mp != NULL) {
        if (strcmp(mfm->path, custom_mp) == 0) {
            mfm->can_format = 0;
        }
        custom_mp = strtok(NULL, ",");
    }
}

void show_partition_format_menu() {
    const char* headers[] = { "Format partitions menu", NULL };

    char* confirm_format = "Confirm format?";
    char* confirm = "Yes - Format";
    char confirm_string[255];
    char* list[256];

    int i = 0;
    int formatable_volumes = 0;
    int num_volumes;
    int chosen_item = 0;

    num_volumes = get_num_volumes();

    if (!num_volumes) {
        LOGE("empty fstab list!\n");
        return;
    }

    FormatMenuEntry format_menu[num_volumes];

    for (i = 0; i < num_volumes; i++) {
        Volume* v = get_device_volumes() + i;

        if (fs_mgr_is_voldmanaged(v) && !vold_is_volume_available(v->mount_point)) {
            continue;
        }

        MFMatrix mfm = { v->mount_point, v->fs_type, 1, 1 };
        get_mnt_fmt_capabilities(&mfm);

        if (mfm.can_format) {
            sprintf(format_menu[formatable_volumes].txt, "format %s", v->mount_point);
            sprintf(format_menu[formatable_volumes].path, "%s", v->mount_point);
            sprintf(format_menu[formatable_volumes].type, "%s", v->fs_type);
            ++formatable_volumes;
        }
    }

#ifdef USE_F2FS
    int enable_f2fs_ext4_conversion = 0;
#endif
    for (;;) {
        for (i = 0; i < formatable_volumes; i++) {
            list[i] = format_menu[i].txt;
        }

        if (!is_data_media()) {
            list[formatable_volumes] = NULL;
#ifdef USE_F2FS
            list[formatable_volumes] = "toggle f2fs <-> ext4 migration";
            list[formatable_volumes + 1] = NULL;
#endif
        } else {
            list[formatable_volumes] = "format /data and /data/media (/sdcard)";
            list[formatable_volumes + 1] = NULL;
#ifdef USE_F2FS
            list[formatable_volumes + 1] = "toggle f2fs <-> ext4 migration";
            list[formatable_volumes + 2] = NULL;
#endif
        }

        chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item < 0)    // GO_BACK / REFRESH
            break;

        if (is_data_media() && chosen_item == formatable_volumes) {
#ifdef USE_F2FS
            if (enable_f2fs_ext4_conversion) {
                preserve_data_media(0);
                show_format_ext4_or_f2fs_menu("/data");
                preserve_data_media(1);
            } else
#endif
            {
                if (!confirm_selection("format /data and /data/media (/sdcard)", confirm))
                    continue;
                ui_print("Formatting /data...\n");
                preserve_data_media(0);
                if (0 != format_volume("/data"))
                    LOGE("Error formatting /data!\n");
                else
                    ui_print("Done.\n");
                preserve_data_media(1);
            }
            setup_data_media(1); // recreate /data/media with proper permissions, mount /data and unmount when done
        } else if (chosen_item < formatable_volumes) {
            FormatMenuEntry* e = &format_menu[chosen_item];
            sprintf(confirm_string, "%s - %s", e->path, confirm_format);

            // support user choice fstype when formatting external storage
            // ensure fstype==auto because most devices with internal vfat storage cannot be formatted to other types
            // if e->type == auto and it is not an extra storage, it will be wiped using format_volume() below (rm -rf like)
            if (strcmp(e->type, "auto") == 0) {
                Volume* v = volume_for_path(e->path);
                if (fs_mgr_is_voldmanaged(v) || can_partition(e->path)) {
                    show_format_sdcard_menu(e->path);
                    continue;
                }
            }

#ifdef USE_F2FS
            if (enable_f2fs_ext4_conversion && !(is_data_media() && strcmp(e->path, "/data") == 0)) {
                if (strcmp(e->type, "ext4") == 0 || strcmp(e->type, "f2fs") == 0) {
                    show_format_ext4_or_f2fs_menu(e->path);
                    continue;
                } else {
                    ui_print("unsupported file system (%s)\n", e->type);
                }
            } else
#endif
            {
                if (!confirm_selection(confirm_string, confirm))
                    continue;
                ui_print("Formatting %s...\n", e->path);
                if (0 != format_volume(e->path))
                    LOGE("Error formatting %s!\n", e->path);
                else
                    ui_print("Done.\n");
            }
        }
#ifdef USE_F2FS
        else if ((is_data_media() && chosen_item == (formatable_volumes + 1)) ||
                    (!is_data_media() && chosen_item == (formatable_volumes))) {
            enable_f2fs_ext4_conversion ^= 1;
            ui_print("ext4 <-> f2fs conversion %s\n", enable_f2fs_ext4_conversion ? "enabled" : "disabled");
        }
#endif
    }
}

int show_partition_mounts_menu() {
    const char* headers[] = { "Mounts and Storage", NULL };
    char* list[256];

    int i = 0;
    int mountable_volumes = 0;
    int num_volumes;
    int chosen_item = 0;
    int is_vold_ums_capable = 0;

    num_volumes = get_num_volumes();

    if (!num_volumes) {
        LOGE("empty fstab list!\n");
        return GO_BACK;
    }

    MountMenuEntry mount_menu[num_volumes];

    for (i = 0; i < num_volumes; i++) {
        Volume* v = get_device_volumes() + i;

        if (is_data_media_volume_path(v->mount_point)) {
            // do not show mount/unmount /sdcard on /data/media devices (when recovery.fstab entry with fs_type == "datamedia")
            continue;
        } else if (fs_mgr_is_voldmanaged(v) && !vold_is_volume_available(v->mount_point)) {
                continue;
        } else if (is_volume_primary_storage(v) || is_volume_extra_storage(v)) {
            is_vold_ums_capable = 1;
        }

        MFMatrix mfm = { v->mount_point, v->fs_type, 1, 1 };
        get_mnt_fmt_capabilities(&mfm);

        if (mfm.can_mount) {
            sprintf(mount_menu[mountable_volumes].mount, "mount %s", v->mount_point);
            sprintf(mount_menu[mountable_volumes].unmount, "unmount %s", v->mount_point);
            sprintf(mount_menu[mountable_volumes].path, "%s", v->mount_point);
            ++mountable_volumes;
        }
    }

    for (;;) {
        for (i = 0; i < mountable_volumes; i++) {
            if (is_path_mounted(mount_menu[i].path))
                list[i] = mount_menu[i].unmount;
            else
                list[i] = mount_menu[i].mount;
        }

        if (is_vold_ums_capable) {
            list[mountable_volumes] = "mount USB storage";
            list[mountable_volumes + 1] = NULL;
        } else {
            list[mountable_volumes] = NULL;
        }

        chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item < 0) // GO_BACK / REFRESH
            break;

        if (chosen_item < mountable_volumes) {
            MountMenuEntry* e = &mount_menu[chosen_item];

            if (is_path_mounted(e->path)) {
                if (0 != ensure_path_unmounted(e->path))
                    LOGE("Error unmounting %s!\n", e->path);
            } else {
                if (0 != ensure_path_mounted(e->path))
                    LOGE("Error mounting %s!\n", e->path);
            }
        } else {
            // chosen_item == mountable_volumes && is_vold_ums_capable
            show_mount_usb_storage_menu();
        }
    }

    return chosen_item;
}
// ------ End Format and mount options

static void run_dedupe_gc() {
    char path[PATH_MAX];
    char* fmt = "%s/clockworkmod/blobs";
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int i = 0;

    sprintf(path, fmt, primary_path);
    ensure_path_mounted(primary_path);
    nandroid_dedupe_gc(path);

    if (extra_paths != NULL) {
        for (i = 0; i < get_num_extra_volumes(); i++) {
            ensure_path_mounted(extra_paths[i]);
            sprintf(path, fmt, extra_paths[i]);
            nandroid_dedupe_gc(path);
        }
        free_string_array(extra_paths);
    }
}

void choose_default_backup_format() {
    const char* headers[] = { "Default Backup Format", "", NULL };

    int fmt = nandroid_get_default_backup_format();

    char **list;
    char* list_tar_default[] = { "tar (default)",
                                 "dup",
                                 "tar + gzip",
                                 NULL };
    char* list_dup_default[] = { "tar",
                                 "dup (default)",
                                 "tar + gzip",
                                 NULL };
    char* list_tgz_default[] = { "tar",
                                 "dup",
                                 "tar + gzip (default)",
                                 NULL };

    if (fmt == NANDROID_BACKUP_FORMAT_DUP) {
        list = list_dup_default;
    } else if (fmt == NANDROID_BACKUP_FORMAT_TGZ) {
        list = list_tgz_default;
    } else {
        list = list_tar_default;
    }

    char path[PATH_MAX];
    sprintf(path, "%s/%s", get_primary_storage_path(), NANDROID_BACKUP_FORMAT_FILE);
    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item) {
        case 0: {
            write_string_to_file(path, "tar");
            ui_print("Default backup format set to tar.\n");
            break;
        }
        case 1: {
            write_string_to_file(path, "dup");
            ui_print("Default backup format set to dedupe.\n");
            break;
        }
        case 2: {
            write_string_to_file(path, "tgz");
            ui_print("Default backup format set to tar + gzip.\n");
            break;
        }
    }
}

static void add_nandroid_options_for_volume(char** menu, char* path, int offset) {
    char buf[100];

    sprintf(buf, "Backup to %s", path);
    menu[offset] = strdup(buf);

    sprintf(buf, "Restore from %s", path);
    menu[offset + 1] = strdup(buf);

    sprintf(buf, "Delete from %s", path);
    menu[offset + 2] = strdup(buf);

    sprintf(buf, "Custom Backup to %s", path);
    menu[offset + 3] = strdup(buf);

    sprintf(buf, "Custom Restore from %s", path);
    menu[offset + 4] = strdup(buf);
}

// number of actions added for each volume by add_nandroid_options_for_volume()
// these go on top of menu list
#define NANDROID_ACTIONS_NUM 5
// number of fixed bottom entries after volume actions
#define NANDROID_FIXED_ENTRIES 3

int show_nandroid_menu() {
    char* primary_path = get_primary_storage_path();
    char** extra_paths = get_extra_storage_paths();
    int num_extra_volumes = get_num_extra_volumes();
    int i = 0, offset = 0, chosen_item = 0;
    char* chosen_path = NULL;
    int action_entries_num = (num_extra_volumes + 1) * NANDROID_ACTIONS_NUM;
                                   // +1 for primary_path
    const char* headers[] = { "Backup and Restore", NULL };

    // (MAX_NUM_MANAGED_VOLUMES + 1) for primary_path (/sdcard)
    // + 1 for extra NULL entry
    char* list[((MAX_NUM_MANAGED_VOLUMES + 1) * NANDROID_ACTIONS_NUM) + NANDROID_FIXED_ENTRIES + 1];
    memset(list, 0, sizeof(list));

    // actions for primary_path
    add_nandroid_options_for_volume(list, primary_path, offset);
    offset += NANDROID_ACTIONS_NUM;

    // actions for voldmanaged volumes
    if (extra_paths != NULL) {
        for (i = 0; i < num_extra_volumes; i++) {
            add_nandroid_options_for_volume(list, extra_paths[i], offset);
            offset += NANDROID_ACTIONS_NUM;
        }
    }

    // fixed bottom entries
    list[offset]     = "Clone ROM to update.zip";
    list[offset + 1] = "Free Unused Backup Data";
    list[offset + 2] = "Misc Nandroid Settings";
    offset += NANDROID_FIXED_ENTRIES;

    // extra NULL for GO_BACK
    list[offset] = NULL;
    offset++;

    for (;;) {
        chosen_item = get_filtered_menu_selection(headers, list, 0, 0, offset);
        if (chosen_item < 0) // GO_BACK / REFRESH
            break;

        // fixed bottom entries
        if (chosen_item == action_entries_num) {
#ifdef PHILZ_TOUCH_RECOVERY
            custom_rom_menu();
#else
            ui_print("Unsupported in open source version!\n");
#endif
        } else if (chosen_item == (action_entries_num + 1)) {
            run_dedupe_gc();
        } else if (chosen_item == (action_entries_num + 2)) {
            misc_nandroid_menu();
        } else if (chosen_item < action_entries_num) {
            // get nandroid volume actions path
            if (chosen_item < NANDROID_ACTIONS_NUM) {
                chosen_path = primary_path;
            } else if (extra_paths != NULL) {
                chosen_path = extra_paths[(chosen_item / NANDROID_ACTIONS_NUM) - 1];
            }

            // process selected nandroid action
            int chosen_subitem = chosen_item % NANDROID_ACTIONS_NUM;
            switch (chosen_subitem) {
                case 0: {
                    char backup_path[PATH_MAX];
                    if (twrp_backup_mode.value) {
                        int fmt = nandroid_get_default_backup_format();
                        if (fmt != NANDROID_BACKUP_FORMAT_TAR && fmt != NANDROID_BACKUP_FORMAT_TGZ) {
                            LOGE("TWRP backup format must be tar(.gz)!\n");
                        } else {
                            get_twrp_backup_path(chosen_path, backup_path);
                            twrp_backup(backup_path);
                        }
                    } else {
                        get_cwm_backup_path(chosen_path, backup_path);
                        nandroid_backup(backup_path);
                    }
                    break;
                }
                case 1: {
                    if (twrp_backup_mode.value)
                        show_twrp_restore_menu(chosen_path);
                    else
                        show_nandroid_restore_menu(chosen_path);
                    break;
                }
                case 2:
                    show_nandroid_delete_menu(chosen_path);
                    break;
                case 3:
                    custom_backup_menu(chosen_path);
                    break;
                case 4:
                    custom_restore_menu(chosen_path);
                    break;
                default:
                    break;
            }
        } else {
            goto out;
        }
    }
out:
    free_string_array(extra_paths);
    for (i = 0; i < action_entries_num; i++)
        free(list[i]);
    return chosen_item;
}

int can_partition(const char* path) {
    if (is_data_media_volume_path(path))
        return 0;

    Volume *vol = volume_for_path(path);
    if (vol == NULL) {
        LOGI("Can't format unknown volume: %s\n", path);
        return 0;
    }
    if (strcmp(vol->fs_type, "auto") != 0) {
        LOGI("cannot partition non auto filesystem: %s (%s)\n", path, vol->fs_type);
        return 0;
    }

    // do not allow partitioning of a device that isn't mmcblkX or mmcblkXp1
    // needed with new vold managed volumes and virtual device path links
    size_t vol_len;
    char *device = NULL;
    if (strstr(vol->blk_device, "/dev/block/mmcblk") != NULL) {
        device = vol->blk_device;
    } else if (vol->blk_device2 != NULL && strstr(vol->blk_device2, "/dev/block/mmcblk") != NULL) {
        device = vol->blk_device2;
    } else {
        LOGI("Can't partition non mmcblk device: %s\n", vol->blk_device);
        return 0;
    }

    vol_len = strlen(device);
    if (device[vol_len - 2] == 'p' && device[vol_len - 1] != '1') {
        LOGI("Can't partition unsafe device: %s\n", device);
        return 0;
    }

    return 1;
}

// pass in mount point as argument
#ifdef USE_F2FS
extern int make_f2fs_main(int argc, char **argv);
#endif
void show_format_sdcard_menu(const char* path) {
    if (is_data_media_volume_path(path))
        return;

    Volume *v = volume_for_path(path);
    if (v == NULL || strcmp(v->fs_type, "auto") != 0)
        return;
    if (!fs_mgr_is_voldmanaged(v) && !can_partition(path))
        return;

    const char* headers[] = { "Format device:", path, "", NULL };

    char* list[] = {
        "default",
        "ext2",
        "ext3",
        "ext4",
        "vfat",
        "exfat",
        "ntfs",
#ifdef USE_F2FS
        "f2fs",
#endif
        NULL
    };

    int ret = -1;
    char cmd[PATH_MAX];
    int chosen_item = get_menu_selection(headers, list, 0, 0);
    if (chosen_item < 0) // REFRESH or GO_BACK
        return;
    if (!confirm_selection("Confirm formatting?", "Yes - Format device"))
        return;

    if (ensure_path_unmounted(v->mount_point) != 0)
        return;

    switch (chosen_item) {
        case 0: {
            ret = format_volume(v->mount_point);
            break;
        }
        case 1:
        case 2: {
            // workaround for new vold managed volumes that cannot be recognized by pre-built ext2/ext3 bins
            const char *device = v->blk_device2;
            if (device == NULL)
                device = v->blk_device;
            ret = format_unknown_device(device, v->mount_point, list[chosen_item]);
            break;
        }
        default: {
            if (fs_mgr_is_voldmanaged(v)) {
                ret = vold_custom_format_volume(v->mount_point, list[chosen_item], 1) == CommandOkay ? 0 : -1;
            } else if (strcmp(list[chosen_item], "vfat") == 0) {
                sprintf(cmd, "/sbin/newfs_msdos -F 32 -O android -c 8 %s", v->blk_device);
                ret = __system(cmd);
            } else if (strcmp(list[chosen_item], "exfat") == 0) {
                sprintf(cmd, "/sbin/mkfs.exfat %s", v->blk_device);
                ret = __system(cmd);
            } else if (strcmp(list[chosen_item], "ntfs") == 0) {
                sprintf(cmd, "/sbin/mkntfs -f %s", v->blk_device);
                ret = __system(cmd);
            } else if (strcmp(list[chosen_item], "ext4") == 0) {
                char *secontext = NULL;
                if (selabel_lookup(sehandle, &secontext, v->mount_point, S_IFDIR) < 0) {
                    LOGE("cannot lookup security context for %s\n", v->mount_point);
                    ret = make_ext4fs(v->blk_device, v->length, path, NULL);
                } else {
                    ret = make_ext4fs(v->blk_device, v->length, path, sehandle);
                    freecon(secontext);
                }
            }
#ifdef USE_F2FS
            else if (strcmp(list[chosen_item], "f2fs") == 0) {
                char* args[] = { "mkfs.f2fs", v->blk_device };
                ret = make_f2fs_main(2, args);
            }
#endif
            break;
        }
    }

    if (ret)
        LOGE("Could not format %s (%s)\n", path, list[chosen_item]);
    else
        ui_print("Done formatting %s (%s)\n", path, list[chosen_item]);
}

void show_advanced_power_menu() {
    const char* headers[] = { "Advanced power options", "", NULL };

    char* list[] = {
        "Reboot Recovery",
        "Reboot to Bootloader",
        "Power Off",
        NULL
    };

    char bootloader_mode[PROPERTY_VALUE_MAX];
#ifdef BOOTLOADER_CMD_ARG
    // force this extra way to use BoardConfig.mk flags
    sprintf(bootloader_mode, BOOTLOADER_CMD_ARG);
#else
    property_get("ro.bootloader.mode", bootloader_mode, "bootloader");
#endif
    if (strcmp(bootloader_mode, "download") == 0)
        list[1] = "Reboot to Download Mode";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item) {
        case 0:
            ui_print("Rebooting recovery...\n");
            reboot_main_system(ANDROID_RB_RESTART2, 0, "recovery");
            break;
        case 1:
            ui_print("Rebooting to %s mode...\n", bootloader_mode);
            reboot_main_system(ANDROID_RB_RESTART2, 0, bootloader_mode);
            break;
        case 2:
            ui_print("Shutting down...\n");
            reboot_main_system(ANDROID_RB_POWEROFF, 0, 0);
            break;
    }
}

void show_advanced_menu() {
    const char* headers[] = { "Advanced menu", NULL };
    char* list[9] = {
        "Open Recovery Script",
        "Aroma File Manager",
        "Re-root System (SuperSU)",
        "Report Error",
        "Key Test",
        "Show log",
        NULL,   // data/media toggle
        NULL,   // loki toggle
        NULL
    };


    for (;;) {
        if (is_data_media()) {
            if (use_migrated_storage())
                list[6] = "Sdcard target: /data/media/0";
            else list[6] = "Sdcard target: /data/media";
        }

#ifdef ENABLE_LOKI
        char item_loki_toggle_menu[MENU_MAX_COLS];
        int enabled = loki_support_enabled();
        if (enabled < 0) {
            list[7] = NULL;
        } else {
            if (enabled)
                ui_format_gui_menu(item_loki_toggle_menu, "Apply Loki Patch", "(x)");
            else
                ui_format_gui_menu(item_loki_toggle_menu, "Apply Loki Patch", "( )");
            list[7] = item_loki_toggle_menu;
        }
#endif

        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item < 0) // GO_BACK || REFRESH
            break;

        switch (chosen_item) {
            case 0: {
                //search in default ors paths
                char* primary_path = get_primary_storage_path();
                char** extra_paths = get_extra_storage_paths();
                int num_extra_volumes = get_num_extra_volumes();
                int i = 0;

                // choose_default_ors_menu() will set browse_for_file
                // browse_for_file == 0 ---> we found .ors scripts in primary storage default location
                choose_default_ors_menu(primary_path);
                if (extra_paths != NULL) {
                    while (browse_for_file && i < num_extra_volumes) {
                        // while we did not find an ors script in default location, continue searching in other volumes
                        choose_default_ors_menu(extra_paths[i]);
                        i++;
                    }
                    free_string_array(extra_paths);
                }

                if (browse_for_file) {
                    //no files found in default locations, let's search manually for a custom ors
                    ui_print("No .ors files under %s\n", RECOVERY_ORS_PATH);
                    ui_print("Manually search .ors files...\n");
                    show_custom_ors_menu();
                }
                break;
            }
            case 1: {
                run_aroma_browser();
                break;
            }
            case 2: {
                if (confirm_selection("Remove existing su ?", "Yes - Apply SuperSU")) {
                    if (0 == __system("/sbin/install-su.sh"))
                        ui_print("Done!\nNow, install SuperSU from market.\n");
                    else
                        ui_print("Failed to apply root!\n");
                }
                break;
            }
            case 3: {
                handle_failure();
                break;
            }
            case 4: {
                ui_print("Outputting key codes.\n");
                ui_print("Go back to end debugging.\n");
                int key;
                int action;
                do {
                    key = ui_wait_key();
                    action = device_handle_key(key, 1);
                    ui_print("Key: %d\n", key);
                } while (action != GO_BACK);
                break;
            }
            case 5: {
#ifdef PHILZ_TOUCH_RECOVERY
                show_log_menu();
#else
                ui_printlogtail(24);
                ui_wait_key();
                ui_clear_key_queue();
#endif
                break;
            }
            case 6: {
                if (is_data_media() && ensure_path_mounted("/data") == 0) {
                    if (use_migrated_storage()) {
                        write_string_to_file("/data/media/.cwm_force_data_media", "1");
                        ui_print("storage set to /data/media\n");
                    } else {
                        // For devices compiled with BOARD_HAS_NO_MULTIUSER_SUPPORT := true, create /data/media/0
                        // to force using it when calling check_migrated_storage() through setup_data_media()
                        ensure_directory("/data/media/0", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
                        delete_a_file("/data/media/.cwm_force_data_media");
                        ui_print("storage set to /data/media/0\n");
                    }
                    // recreate /sdcard link
                    setup_data_media(0); // /data is mounted above. No need to mount/unmount on call
                    ui_print("Reboot to apply settings!\n");
                }
                break;
            }
            case 7: {
#ifdef ENABLE_LOKI
                toggle_loki_support();
                break;
#endif
            }
        }
    }
}

// called on recovery exit
// data will be mounted by call to write_string_to_file() on /data/media devices
// we need to ensure a proper unmount
void write_recovery_version() {
    char path[PATH_MAX];
    sprintf(path, "%s/%s", get_primary_storage_path(), RECOVERY_VERSION_FILE);
    write_string_to_file(path, EXPAND(RECOVERY_VERSION) "\n" EXPAND(TARGET_DEVICE));
    // force unmount /data for /data/media devices as we call this on recovery exit
    preserve_data_media(0);
    ensure_path_unmounted(path);
    preserve_data_media(1);
}

// on recovery exit, check if root is missing/broken and if a script to overwrite recovery is executable
int verify_root_and_recovery() {
    if (!check_root_and_recovery.value)
        return 0;

    if (ensure_path_mounted("/system") != 0)
        return 0;

    int ret = 0;
    struct stat st;
    // check to see if install-recovery.sh is going to clobber recovery
    // install-recovery.sh is also used to run the su daemon on stock rom for 4.3+
    // so verify that doesn't exist...
    if (0 != lstat("/system/etc/.installed_su_daemon", &st)) {
        // check install-recovery.sh exists and is executable
        if (0 == lstat("/system/etc/install-recovery.sh", &st)) {
            if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                ui_SetShowText(true);
                if (confirm_selection("ROM may flash stock recovery on boot. Fix?", "Yes - Disable recovery flash")) {
                    __system("chmod -x /system/etc/install-recovery.sh");
                    ret = 1;
                }
            }
        }
    }

    // do not check permissions if system android version is 4.3+
    // in that case, no need to chmod 06755 as it could break root on +4.3 ROMs
    // for 4.3+, su daemon will set proper 755 permissions before app requests root
    // if no su file is found, recovery will just install su daemon on 4.3 ROMs to gain root
    // credits @Chainfire
    char value[PROPERTY_VALUE_MAX];
    int needs_suid = 1;
    read_config_file("/system/build.prop", "ro.build.version.sdk", value, "0");
    if (atoi(value) >= 18)
        needs_suid = 0;

    int exists = 0; // su exists, regular file or symlink
    int su_nums = 0; // su bin as regular file, not symlink
    if (0 == lstat("/system/bin/su", &st)) {
        exists = 1;
        if (S_ISREG(st.st_mode)) {
            su_nums += 1;
            if (needs_suid && (st.st_mode & (S_ISUID | S_ISGID)) != (S_ISUID | S_ISGID)) {
                ui_SetShowText(true);
                if (confirm_selection("Root access possibly lost. Fix?", "Yes - Fix root (/system/bin/su)")) {
                    __system("chmod 6755 /system/bin/su");
                    ret = 1;
                }
            }
        }
    }

    if (0 == lstat("/system/xbin/su", &st)) {
        exists = 1;
        if (S_ISREG(st.st_mode)) {
            su_nums += 1;
            if (needs_suid && (st.st_mode & (S_ISUID | S_ISGID)) != (S_ISUID | S_ISGID)) {
                ui_SetShowText(true);
                if (confirm_selection("Root access possibly lost. Fix?", "Yes - Fix root (/system/xbin/su)")) {
                    __system("chmod 6755 /system/xbin/su");
                    ret = 1;
                }
            }
        }
    }

    // If we have no root (exists == 0) or we have two su instances (exists == 2), prompt to properly root the device
    if (!exists || su_nums != 1) {
        ui_SetShowText(true);
        if (confirm_selection("Root access is missing/broken. Root device?", "Yes - Apply root (/system/xbin/su)")) {
            __system("/sbin/install-su.sh");
            ret = 2;
        }
    }

    ensure_path_unmounted("/system");
    return ret;
}
