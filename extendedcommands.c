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

// statfs
#include <sys/vfs.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "make_ext4fs.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"
#include "bmlutils/bmlutils.h"
#include "cutils/android_reboot.h"
#include "adb_install.h" //since we moved sideload function from main menu to install zip submenu

int signature_check_enabled = 1;
int script_assert_enabled = 1;
static const char *SDCARD_UPDATE_FILE = "/sdcard/update.zip";

int
get_filtered_menu_selection(char** headers, char** items, int menu_only, int initial_selection, int items_count) {
    int index;
    int offset = 0;
    int* translate_table = (int*)malloc(sizeof(int) * items_count);
    for (index = 0; index < items_count; index++) {
        if (items[index] == NULL)
            continue;
        char *item = items[index];
        items[index] = NULL;
        items[offset] = item;
        translate_table[offset] = index;
        offset++;
    }
    items[offset] = NULL;

    initial_selection = translate_table[initial_selection];
    int ret = get_menu_selection(headers, items, menu_only, initial_selection);
    if (ret < 0 || ret >= offset) {
        free(translate_table);
        return ret;
    }

    ret = translate_table[ret];
    free(translate_table);
    return ret;
}

void write_string_to_file(const char* filename, const char* string) {
    ensure_path_mounted(filename);
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p $(dirname %s)", filename);
    __system(tmp);
    FILE *file = fopen(filename, "w");
    if (file != NULL) {
	    fprintf(file, "%s", string);
	    fclose(file);
    } else
        LOGE("Cannot write to %s\n", filename);
}

void write_recovery_version() {
    struct stat st;
    if (is_data_media()) {
        if (0 == lstat("/data/media/0", &st))
            write_string_to_file("/data/media/0/clockworkmod/.recovery_version",EXPAND(RECOVERY_VERSION) "\n" EXPAND(TARGET_DEVICE));
        write_string_to_file("/data/media/clockworkmod/.recovery_version",EXPAND(RECOVERY_VERSION) "\n" EXPAND(TARGET_DEVICE));
    } else {
        if (volume_for_path("/emmc") != NULL)
            write_string_to_file("/emmc/clockworkmod/.recovery_version",EXPAND(RECOVERY_VERSION) "\n" EXPAND(TARGET_DEVICE));
        else write_string_to_file("/sdcard/clockworkmod/.recovery_version",EXPAND(RECOVERY_VERSION) "\n" EXPAND(TARGET_DEVICE));
    }
}

void
toggle_signature_check()
{
    signature_check_enabled = !signature_check_enabled;
    ui_print("Signature Check: %s\n", signature_check_enabled ? "Enabled" : "Disabled");
}

int install_zip(const char* packagefilepath)
{
    ui_print("\n-- Installing: %s\n", packagefilepath);
    if (device_flash_type() == MTD) {
        set_sdcard_update_bootloader_message();
    }
    int status = install_package(packagefilepath);
    ui_reset_progress();
    if (status != INSTALL_SUCCESS) {
        ui_set_background(BACKGROUND_ICON_ERROR);
        ui_print("Installation aborted.\n");
        return 1;
    }
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_print("\nInstall from sdcard complete.\n");
    return 0;
}

#define ITEM_CHOOSE_ZIP       0
#define ITEM_CHOOSE_ZIP_INT   1
#define ITEM_MULTI_FLASH      2
#define ITEM_APPLY_UPDATE     3 // warning: redefined in recovery_ui.h
#define ITEM_APPLY_SIDELOAD   4
#define ITEM_SIG_CHECK        5
#define ITEM_FREE_BROWSE      6

void show_install_update_menu()
{
    static char* headers[] = {  "Install update from zip file",
                                "",
                                NULL
    };

    char* install_menu_items[] = {  "Choose zip from sdcard",
                                    NULL,
                                    "Multi-zip Installer",
                                    "Apply /sdcard/update.zip",
                                    "Install zip from sideload",
                                    "Toggle Signature Verification",
                                    "Setup Free Browse Mode",
                                    NULL };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc/";
        install_menu_items[1] = "Choose zip from Internal sdcard";
    }
    else {
        other_sd = "/external_sd/";
        install_menu_items[1] = "Choose zip from External sdcard";
    }

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, install_menu_items, 0, 0);
        switch (chosen_item)
        {
            case ITEM_SIG_CHECK:
                toggle_signature_check();
                break;
            case ITEM_APPLY_UPDATE:
            {
                if (confirm_selection("Confirm install?", "Yes - Install /sdcard/update.zip"))
                    install_zip(SDCARD_UPDATE_FILE);
                break;
            }
            case ITEM_CHOOSE_ZIP:
                show_choose_zip_menu("/sdcard/");
                //write_recovery_version();
                break;
            case ITEM_APPLY_SIDELOAD:
                apply_from_adb();
                break;
            case ITEM_CHOOSE_ZIP_INT:
                if (volume_for_path(other_sd) != NULL)
                    show_choose_zip_menu(other_sd);
                break;
            case ITEM_MULTI_FLASH:
                show_multi_flash_menu();
                break;
            case ITEM_FREE_BROWSE:
#ifdef PHILZ_TOUCH_RECOVERY
                set_custom_zip_path();
#endif
                break;
            default:
                return;
        }

    }
}

void free_string_array(char** array)
{
    if (array == NULL)
        return;
    char* cursor = array[0];
    int i = 0;
    while (cursor != NULL)
    {
        free(cursor);
        cursor = array[++i];
    }
    free(array);
}

char** gather_files(const char* directory, const char* fileExtensionOrDirectory, int* numFiles)
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int total = 0;
    int i;
    char** files = NULL;
    int pass;
    *numFiles = 0;
    int dirLen = strlen(directory);

    dir = opendir(directory);
    if (dir == NULL) {
        ui_print("Couldn't open directory %s\n", directory);
        return NULL;
    }

    int extension_length = 0;
    if (fileExtensionOrDirectory != NULL)
        extension_length = strlen(fileExtensionOrDirectory);

    int isCounting = 1;
    i = 0;
    for (pass = 0; pass < 2; pass++) {
        while ((de=readdir(dir)) != NULL) {
            // skip hidden files
            if (de->d_name[0] == '.')
                continue;

            // NULL means that we are gathering directories, so skip this
            if (fileExtensionOrDirectory != NULL)
            {
                // make sure that we can have the desired extension (prevent seg fault)
                if (strlen(de->d_name) < extension_length)
                    continue;
                // compare the extension
                if (strcmp(de->d_name + strlen(de->d_name) - extension_length, fileExtensionOrDirectory) != 0)
                    continue;
            }
            else
            {
                struct stat info;
                char fullFileName[PATH_MAX];
                strcpy(fullFileName, directory);
                strcat(fullFileName, de->d_name);
                lstat(fullFileName, &info);
                // make sure it is a directory
                if (!(S_ISDIR(info.st_mode)))
                    continue;
            }

            if (pass == 0)
            {
                total++;
                continue;
            }

            files[i] = (char*) malloc(dirLen + strlen(de->d_name) + 2);
            strcpy(files[i], directory);
            strcat(files[i], de->d_name);
            if (fileExtensionOrDirectory == NULL)
                strcat(files[i], "/");
            i++;
        }
        if (pass == 1)
            break;
        if (total == 0)
            break;
        rewinddir(dir);
        *numFiles = total;
        files = (char**) malloc((total+1)*sizeof(char*));
        files[total]=NULL;
    }

    if(closedir(dir) < 0) {
        LOGE("Failed to close directory.\n");
    }

    if (total==0) {
        return NULL;
    }

    // sort the result
    if (files != NULL) {
        for (i = 0; i < total; i++) {
            int curMax = -1;
            int j;
            for (j = 0; j < total - i; j++) {
                if (curMax == -1 || strcmp(files[curMax], files[j]) < 0)
                    curMax = j;
            }
            char* temp = files[curMax];
            files[curMax] = files[total - i - 1];
            files[total - i - 1] = temp;
        }
    }

    return files;
}

// pass in NULL for fileExtensionOrDirectory and you will get a directory chooser
static int no_files_found = 0;
char* choose_file_menu(const char* directory, const char* fileExtensionOrDirectory, const char* headers[])
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int numFiles = 0;
    int numDirs = 0;
    int i;
    char* return_value = NULL;
    int dir_len = strlen(directory);

    i = 0;
    while (headers[i]) {
        i++;
    }
    const char** fixed_headers = (const char*)malloc((i + 3) * sizeof(char*));
    i = 0;
    while (headers[i]) {
        fixed_headers[i] = headers[i];
        i++;
    }
    fixed_headers[i] = directory;
    //let's spare some header space
    //fixed_headers[i + 1] = "";
    //fixed_headers[i + 2 ] = NULL;
    fixed_headers[i + 1 ] = NULL;

    char** files = gather_files(directory, fileExtensionOrDirectory, &numFiles);
    char** dirs = NULL;
    if (fileExtensionOrDirectory != NULL)
        dirs = gather_files(directory, NULL, &numDirs);
    int total = numDirs + numFiles;
    if (total == 0)
    {
        no_files_found = 1; //we found no valid file to select
        ui_print("No files found.\n");
    }
    else
    {
        no_files_found = 0; //we found a valid file to select
        char** list = (char**) malloc((total + 1) * sizeof(char*));
        list[total] = NULL;


        for (i = 0 ; i < numDirs; i++)
        {
            list[i] = strdup(dirs[i] + dir_len);
        }

        for (i = 0 ; i < numFiles; i++)
        {
            list[numDirs + i] = strdup(files[i] + dir_len);
        }

        for (;;)
        {
            int chosen_item = get_menu_selection(fixed_headers, list, 0, 0);
            if (chosen_item == GO_BACK)
                break;
            static char ret[PATH_MAX];
            if (chosen_item < numDirs)
            {
                char* subret = choose_file_menu(dirs[chosen_item], fileExtensionOrDirectory, headers);
                if (subret != NULL)
                {
                    strcpy(ret, subret);
                    return_value = ret;
                    break;
                }
                continue;
            }
            strcpy(ret, files[chosen_item - numDirs]);
            return_value = ret;
            break;
        }
        free_string_array(list);
    }

    free_string_array(files);
    free_string_array(dirs);
    free(fixed_headers);
    return return_value;
}

void show_choose_zip_menu(const char *mount_point)
{
#ifdef PHILZ_TOUCH_RECOVERY
    // browse for zip files up/backward including root system and have a default user set start folder
    if (show_custom_zip_menu() == 0)
        return;
#endif

    if (ensure_path_mounted(mount_point) != 0) {
        LOGE ("Can't mount %s\n", mount_point);
        return;
    }

    static char* headers[] = {  "Choose a zip to apply",
                                NULL
    };

    char* file = choose_file_menu(mount_point, ".zip", headers);
    if (file == NULL)
        return;
    static char* confirm_install  = "Confirm install?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Install %s", basename(file));
    if (confirm_selection(confirm_install, confirm))
        install_zip(file);
}

void show_nandroid_restore_menu(const char* path)
{
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return;
    }

    static char* headers[] = {  "Choose an image to restore",
                                NULL
    };

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/backup/", path);
    char* file = choose_file_menu(tmp, NULL, headers);
    if (file == NULL)
        return;

    if (confirm_selection("Confirm restore?", "Yes - Restore"))
        nandroid_restore(file, 1, 1, 1, 1, 1, 0);
}

void show_nandroid_delete_menu(const char* path)
{
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return;
    }

    static char* headers[] = {  "Choose an image to delete",
                                NULL
    };

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/backup/", path);
    char* file = choose_file_menu(tmp, NULL, headers);
    if (file == NULL)
        return;

    if (confirm_selection("Confirm delete?", "Yes - Delete")) {
        // nandroid_restore(file, 1, 1, 1, 1, 1, 0);
        sprintf(tmp, "rm -rf %s", file);
        __system(tmp);
    }
}

#define MAX_NUM_USB_VOLUMES 3
#define LUN_FILE_EXPANDS    2

struct lun_node {
    const char *lun_file;
    struct lun_node *next;
};

static struct lun_node *lun_head = NULL;
static struct lun_node *lun_tail = NULL;

int control_usb_storage_set_lun(Volume* vol, bool enable, const char *lun_file) {
    const char *vol_device = enable ? vol->device : "";
    int fd;
    struct lun_node *node;

    // Verify that we have not already used this LUN file
    for(node = lun_head; node; node = node->next) {
        if (!strcmp(node->lun_file, lun_file)) {
            // Skip any LUN files that are already in use
            return -1;
        }
    }

    // Open a handle to the LUN file
    LOGI("Trying %s on LUN file %s\n", vol->device, lun_file);
    if ((fd = open(lun_file, O_WRONLY)) < 0) {
        LOGW("Unable to open ums lunfile %s (%s)\n", lun_file, strerror(errno));
        return -1;
    }

    // Write the volume path to the LUN file
    if ((write(fd, vol_device, strlen(vol_device) + 1) < 0) &&
       (!enable || !vol->device2 || (write(fd, vol->device2, strlen(vol->device2)) < 0))) {
        LOGW("Unable to write to ums lunfile %s (%s)\n", lun_file, strerror(errno));
        close(fd);
        return -1;
    } else {
        // Volume path to LUN association succeeded
        close(fd);

        // Save off a record of this lun_file being in use now
        node = (struct lun_node *)malloc(sizeof(struct lun_node));
        node->lun_file = strdup(lun_file);
        node->next = NULL;
        if (lun_head == NULL)
           lun_head = lun_tail = node;
        else {
           lun_tail->next = node;
           lun_tail = node;
        }

        LOGI("Successfully %sshared %s on LUN file %s\n", enable ? "" : "un", vol->device, lun_file);
        return 0;
    }
}

int control_usb_storage_for_lun(Volume* vol, bool enable) {
    static const char* lun_files[] = {
#ifdef BOARD_UMS_LUNFILE
        BOARD_UMS_LUNFILE,
#endif
#ifdef TARGET_USE_CUSTOM_LUN_FILE_PATH
        TARGET_USE_CUSTOM_LUN_FILE_PATH,
#endif
        "/sys/devices/platform/usb_mass_storage/lun%d/file",
        "/sys/class/android_usb/android0/f_mass_storage/lun/file",
        "/sys/class/android_usb/android0/f_mass_storage/lun_ex/file",
        NULL
    };

    // If recovery.fstab specifies a LUN file, use it
    if (vol->lun) {
        return control_usb_storage_set_lun(vol, enable, vol->lun);
    }

    // Try to find a LUN for this volume
    //   - iterate through the lun file paths
    //   - expand any %d by LUN_FILE_EXPANDS
    int lun_num = 0;
    int i;
    for(i = 0; lun_files[i]; i++) {
        const char *lun_file = lun_files[i];
        for(lun_num = 0; lun_num < LUN_FILE_EXPANDS; lun_num++) {
            char formatted_lun_file[255];
    
            // Replace %d with the LUN number
            bzero(formatted_lun_file, 255);
            snprintf(formatted_lun_file, 254, lun_file, lun_num);
    
            // Attempt to use the LUN file
            if (control_usb_storage_set_lun(vol, enable, formatted_lun_file) == 0) {
                return 0;
            }
        }
    }

    // All LUNs were exhausted and none worked
    LOGW("Could not %sable %s on LUN %d\n", enable ? "en" : "dis", vol->device, lun_num);

    return -1;  // -1 failure, 0 success
}

int control_usb_storage(Volume **volumes, bool enable) {
    int res = -1;
    int i;
    for(i = 0; i < MAX_NUM_USB_VOLUMES; i++) {
        Volume *volume = volumes[i];
        if (volume) {
            int vol_res = control_usb_storage_for_lun(volume, enable);
            if (vol_res == 0) res = 0; // if any one path succeeds, we return success
        }
    }

    // Release memory used by the LUN file linked list
    struct lun_node *node = lun_head;
    while(node) {
       struct lun_node *next = node->next;
       free((void *)node->lun_file);
       free(node);
       node = next;
    }
    lun_head = lun_tail = NULL;

    return res;  // -1 failure, 0 success
}

void show_mount_usb_storage_menu()
{
    // Build a list of Volume objects; some or all may not be valid
    Volume* volumes[MAX_NUM_USB_VOLUMES] = {
        volume_for_path("/sdcard"),
        volume_for_path("/emmc"),
        volume_for_path("/external_sd")
    };

    // Enable USB storage
    if (control_usb_storage(volumes, 1))
        return;

    static char* headers[] = {  "USB Mass Storage device",
                                "Leaving this menu unmounts",
                                "your SD card from your PC.",
                                "",
                                NULL
    };

    static char* list[] = { "Unmount", NULL };

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == GO_BACK || chosen_item == 0)
            break;
    }

    // Disable USB storage
    control_usb_storage(volumes, 0);
}

int confirm_selection(const char* title, const char* confirm)
{
    struct stat info;
    if (0 == stat("/sdcard/clockworkmod/.no_confirm", &info))
        return 1;

    char* confirm_headers[]  = {  title, "  THIS CAN NOT BE UNDONE.", NULL };
    int one_confirm = 0 == stat("/sdcard/clockworkmod/.one_confirm", &info);
#ifdef BOARD_TOUCH_RECOVERY
    one_confirm = 1;
#endif
#ifdef PHILZ_TOUCH_RECOVERY
        char* items[] = { "No",
                        "No",
                        confirm, //" Yes -- wipe partition",   // [2]
                        "No",
                        NULL };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        return chosen_item == 2;
#else
    if (one_confirm) {
        char* items[] = { "No",
                        confirm, //" Yes -- wipe partition",   // [1]
                        NULL };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        return chosen_item == 1;
    }
    else {
        char* items[] = { "No",
                        "No",
                        "No",
                        "No",
                        "No",
                        "No",
                        "No",
                        confirm, //" Yes -- wipe partition",   // [7]
                        "No",
                        "No",
                        "No",
                        NULL };
        int chosen_item = get_menu_selection(confirm_headers, items, 0, 0);
        return chosen_item == 7;
    }
#endif
}

#define MKE2FS_BIN      "/sbin/mke2fs"
#define TUNE2FS_BIN     "/sbin/tune2fs"
#define E2FSCK_BIN      "/sbin/e2fsck"

extern struct selabel_handle *sehandle;
int format_device(const char *device, const char *path, const char *fs_type) {
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        // silent failure for sd-ext
        if (strcmp(path, "/sd-ext") == 0)
            return -1;
        LOGE("unknown volume \"%s\"\n", path);
        return -1;
    }
    if (is_data_media_volume_path(path)) {
        return format_unknown_device(NULL, path, NULL);
    }
    if (strstr(path, "/data") == path && is_data_media()) {
        return format_unknown_device(NULL, path, NULL);
    }
    if (strcmp(fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", path);
        return -1;
    }

    if (strcmp(fs_type, "rfs") == 0) {
        if (ensure_path_unmounted(path) != 0) {
            LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
            return -1;
        }
        if (0 != format_rfs_device(device, path)) {
            LOGE("format_volume: format_rfs_device failed on %s\n", device);
            return -1;
        }
        return 0;
    }
 
    if (strcmp(v->mount_point, path) != 0) {
        return format_unknown_device(v->device, path, NULL);
    }

    if (ensure_path_unmounted(path) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }

    if (strcmp(fs_type, "yaffs2") == 0 || strcmp(fs_type, "mtd") == 0) {
        mtd_scan_partitions();
        const MtdPartition* partition = mtd_find_partition_by_name(device);
        if (partition == NULL) {
            LOGE("format_volume: no MTD partition \"%s\"\n", device);
            return -1;
        }

        MtdWriteContext *write = mtd_write_partition(partition);
        if (write == NULL) {
            LOGW("format_volume: can't open MTD \"%s\"\n", device);
            return -1;
        } else if (mtd_erase_blocks(write, -1) == (off_t) -1) {
            LOGW("format_volume: can't erase MTD \"%s\"\n", device);
            mtd_write_close(write);
            return -1;
        } else if (mtd_write_close(write)) {
            LOGW("format_volume: can't close MTD \"%s\"\n",device);
            return -1;
        }
        return 0;
    }

    if (strcmp(fs_type, "ext4") == 0) {
        int length = 0;
        if (strcmp(v->fs_type, "ext4") == 0) {
            // Our desired filesystem matches the one in fstab, respect v->length
            length = v->length;
        }
#ifdef USE_MKE2FS_FIX
        char ext4_cmd[PATH_MAX];
        sprintf(ext4_cmd, "/sbin/mke2fs -T ext4 -b 4096 -m 0 -F %s", device);
        if (0 != __system(ext4_cmd))
#endif
        {
            reset_ext4fs_info();
            int result = make_ext4fs(device, length, v->mount_point, sehandle);
            if (result != 0) {
                LOGE("format_volume: make_ext4fs failed on %s\n", device);
                return -1;
            }
        }
        return 0;
    }

    return format_unknown_device(device, path, fs_type);
}

int format_unknown_device(const char *device, const char* path, const char *fs_type)
{
    LOGI("Formatting unknown device.\n");

    if (fs_type != NULL && get_flash_type(fs_type) != UNSUPPORTED)
        return erase_raw_partition(fs_type, device);

    // if this is SDEXT:, don't worry about it if it does not exist.
    if (0 == strcmp(path, "/sd-ext"))
    {
        struct stat st;
        Volume *vol = volume_for_path("/sd-ext");
        if (vol == NULL || 0 != stat(vol->device, &st))
        {
            ui_print("No app2sd partition found. Skipping format of /sd-ext.\n");
            return 0;
        }
    }

    if (NULL != fs_type) {
        if (strcmp("ext3", fs_type) == 0) {
            LOGI("Formatting ext3 device.\n");
            if (0 != ensure_path_unmounted(path)) {
                LOGE("Error while unmounting %s.\n", path);
                return -12;
            }
            return format_ext3_device(device);
        }

        if (strcmp("ext2", fs_type) == 0) {
            LOGI("Formatting ext2 device.\n");
            if (0 != ensure_path_unmounted(path)) {
                LOGE("Error while unmounting %s.\n", path);
                return -12;
            }
            return format_ext2_device(device);
        }
    }

    if (0 != ensure_path_mounted(path))
    {
        ui_print("Error mounting %s!\n", path);
        ui_print("Skipping format...\n");
        return 0;
    }

    static char tmp[PATH_MAX];
    if (strcmp(path, "/data") == 0) {
        sprintf(tmp, "cd /data ; for f in $(ls -a | grep -v ^media$); do rm -rf $f; done");
        __system(tmp);
        // if the /data/media sdcard has already been migrated for android 4.2,
        // prevent the migration from happening again by writing the .layout_version
        struct stat st;
        if (0 == lstat("/data/media/0", &st)) {
            char* layout_version = "2";
            FILE* f = fopen("/data/.layout_version", "wb");
            if (NULL != f) {
                fwrite(layout_version, 1, 2, f);
                fclose(f);
            }
            else {
                LOGI("error opening /data/.layout_version for write.\n");
            }
        }
        else {
            LOGI("/data/media/0 not found. migration may occur.\n");
        }
    }
    else {
        sprintf(tmp, "rm -rf %s/*", path);
        __system(tmp);
        sprintf(tmp, "rm -rf %s/.*", path);
        __system(tmp);
    }

    ensure_path_unmounted(path);
    return 0;
}

//#define MOUNTABLE_COUNT 5
//#define DEVICE_COUNT 4
//#define MMC_COUNT 2

typedef struct {
    char mount[255];
    char unmount[255];
    Volume* v;
} MountMenuEntry;

typedef struct {
    char txt[255];
    Volume* v;
} FormatMenuEntry;

int is_safe_to_format(char* name)
{
    char str[255];
    char* partition;
    property_get("ro.cwm.forbid_format", str, "/misc,/radio,/bootloader,/recovery,/efs,/wimax");

    partition = strtok(str, ", ");
    while (partition != NULL) {
        if (strcmp(name, partition) == 0) {
            return 0;
        }
        partition = strtok(NULL, ", ");
    }

    return 1;
}

void show_partition_menu()
{
    static char* headers[] = {  "Mounts and Storage Menu",
                                NULL
    };

    static MountMenuEntry* mount_menu = NULL;
    static FormatMenuEntry* format_menu = NULL;

    typedef char* string;

    int i, mountable_volumes, formatable_volumes;
    int num_volumes;
    Volume* device_volumes;

    num_volumes = get_num_volumes();
    device_volumes = get_device_volumes();

    string options[255];

    if(!device_volumes)
        return;

    mountable_volumes = 0;
    formatable_volumes = 0;

    mount_menu = malloc(num_volumes * sizeof(MountMenuEntry));
    format_menu = malloc(num_volumes * sizeof(FormatMenuEntry));

    for (i = 0; i < num_volumes; ++i) {
        Volume* v = &device_volumes[i];
        if(strcmp("ramdisk", v->fs_type) != 0 && strcmp("mtd", v->fs_type) != 0 && strcmp("emmc", v->fs_type) != 0 && strcmp("bml", v->fs_type) != 0) {
            if (strcmp("datamedia", v->fs_type) != 0) {
                sprintf(&mount_menu[mountable_volumes].mount, "mount %s", v->mount_point);
                sprintf(&mount_menu[mountable_volumes].unmount, "unmount %s", v->mount_point);
                mount_menu[mountable_volumes].v = &device_volumes[i];
                ++mountable_volumes;
            }
            if (is_safe_to_format(v->mount_point)) {
                sprintf(&format_menu[formatable_volumes].txt, "format %s", v->mount_point);
                format_menu[formatable_volumes].v = &device_volumes[i];
                ++formatable_volumes;
            }
        }
        else if (strcmp("ramdisk", v->fs_type) != 0 && strcmp("mtd", v->fs_type) == 0 && is_safe_to_format(v->mount_point))
        {
            sprintf(&format_menu[formatable_volumes].txt, "format %s", v->mount_point);
            format_menu[formatable_volumes].v = &device_volumes[i];
            ++formatable_volumes;
        }
    }

    static char* confirm_format  = "Confirm format?";
    static char* confirm = "Yes - Format";
    char confirm_string[255];

    for (;;)
    {
        for (i = 0; i < mountable_volumes; i++)
        {
            MountMenuEntry* e = &mount_menu[i];
            Volume* v = e->v;
            if(is_path_mounted(v->mount_point))
                options[i] = e->unmount;
            else
                options[i] = e->mount;
        }

        for (i = 0; i < formatable_volumes; i++)
        {
            FormatMenuEntry* e = &format_menu[i];

            options[mountable_volumes+i] = e->txt;
        }
/*
        if (!is_data_media()) {
          options[mountable_volumes + formatable_volumes] = "mount USB storage";
          options[mountable_volumes + formatable_volumes + 1] = NULL;
        }
        else {
          options[mountable_volumes + formatable_volumes] = "format /data and /data/media (/sdcard)";
          options[mountable_volumes + formatable_volumes + 1] = NULL;
        }
*/
        //Mount usb storage support for /data/media devices, by PhilZ (part 1/2)
        if (!is_data_media()) {
            options[mountable_volumes + formatable_volumes] = "mount USB storage";
            options[mountable_volumes + formatable_volumes + 1] = NULL;
        } else {
            options[mountable_volumes + formatable_volumes] = "format /data and /data/media (/sdcard)";
            options[mountable_volumes + formatable_volumes + 1] = "mount USB storage";
            options[mountable_volumes + formatable_volumes + 2] = NULL;
        }
        //end PhilZ support for mount usb storage on /data/media (part 1/2)
        
        int chosen_item = get_menu_selection(headers, &options, 0, 0);
        if (chosen_item == GO_BACK)
            break;
        if (chosen_item == (mountable_volumes+formatable_volumes)) {
            if (!is_data_media()) {
                show_mount_usb_storage_menu();
            }
            else {
                if (!confirm_selection("format /data and /data/media (/sdcard)", confirm))
                    continue;
                handle_data_media_format(1);
                ui_print("Formatting /data...\n");
                if (0 != format_volume("/data"))
                    ui_print("Error formatting /data!\n");
                else
                    ui_print("Done.\n");
                handle_data_media_format(0);  
            }
        }
        //Mount usb storage support for /data/media devices, by PhilZ (part 2/2)
        else if (is_data_media() && chosen_item == (mountable_volumes+formatable_volumes+1)) {
            show_mount_usb_storage_menu();
        }
        //end PhilZ support for mount usb storage on /data/media
        else if (chosen_item < mountable_volumes) {
            MountMenuEntry* e = &mount_menu[chosen_item];
            Volume* v = e->v;

            if (is_path_mounted(v->mount_point))
            {
                if (0 != ensure_path_unmounted(v->mount_point))
                    ui_print("Error unmounting %s!\n", v->mount_point);
            }
            else
            {
                if (0 != ensure_path_mounted(v->mount_point))
                    ui_print("Error mounting %s!\n",  v->mount_point);
            }
        }
        else if (chosen_item < (mountable_volumes + formatable_volumes))
        {
            chosen_item = chosen_item - mountable_volumes;
            FormatMenuEntry* e = &format_menu[chosen_item];
            Volume* v = e->v;

            sprintf(confirm_string, "%s - %s", v->mount_point, confirm_format);

            if (!confirm_selection(confirm_string, confirm))
                continue;
            ui_print("Formatting %s...\n", v->mount_point);
            if (0 != format_volume(v->mount_point))
                ui_print("Error formatting %s!\n", v->mount_point);
            else
                ui_print("Done.\n");
        }
    }

    free(mount_menu);
    free(format_menu);
}

void show_nandroid_advanced_restore_menu(const char* path)
{
    if (ensure_path_mounted(path) != 0) {
        LOGE ("Can't mount sdcard\n");
        return;
    }

    static char* advancedheaders[] = {  "Choose an image to restore",
                                "",
                                "Choose an image to restore",
                                "first. The next menu will",
                                "show you more options.",
                                NULL
    };

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/backup/", path);
    char* file = choose_file_menu(tmp, NULL, advancedheaders);
    if (file == NULL)
        return;

    static char* headers[] = {  "Advanced Restore",
                                NULL
    };

    static char* list[] = { "Restore boot",
                            "Restore system (+/- preload)",
                            "Restore data",
                            "Restore cache",
                            "Restore sd-ext",
                            "Restore wimax",
                            NULL
    };
    
    if (0 != get_partition_device("wimax", tmp)) {
        // disable wimax restore option
        list[5] = NULL;
    }

    static char* confirm_restore  = "Confirm restore?";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            if (confirm_selection(confirm_restore, "Yes - Restore boot"))
                nandroid_restore(file, 1, 0, 0, 0, 0, 0);
            break;
        case 1:
            if (confirm_selection(confirm_restore, "Yes - Restore system +/- preload"))
                nandroid_restore(file, 0, 1, 0, 0, 0, 0);
            break;
        case 2:
            if (confirm_selection(confirm_restore, "Yes - Restore data"))
                nandroid_restore(file, 0, 0, 1, 0, 0, 0);
            break;
        case 3:
            if (confirm_selection(confirm_restore, "Yes - Restore cache"))
                nandroid_restore(file, 0, 0, 0, 1, 0, 0);
            break;
        case 4:
            if (confirm_selection(confirm_restore, "Yes - Restore sd-ext"))
                nandroid_restore(file, 0, 0, 0, 0, 1, 0);
            break;
        case 5:
            if (confirm_selection(confirm_restore, "Yes - Restore wimax"))
                nandroid_restore(file, 0, 0, 0, 0, 0, 1);
            break;
    }
}

static void run_dedupe_gc(const char* other_sd) {
    ensure_path_mounted("/sdcard");
    nandroid_dedupe_gc("/sdcard/clockworkmod/blobs");
    if (other_sd) {
        ensure_path_mounted(other_sd);
        char tmp[PATH_MAX];
        sprintf(tmp, "%s/clockworkmod/blobs", other_sd);
        nandroid_dedupe_gc(tmp);
    }
}

static void choose_default_backup_format() {
    static char* headers[] = {  "Default Backup Format",
                                "",
                                NULL
    };

    char **list;
    char* list_tar_default[] = { "tar (default)",
        "dup",
#ifndef PHILZ_TOUCH_RECOVERY
        "Toggle Compression",
#endif
        NULL
    };
    char* list_dup_default[] = { "tar",
        "dup (default)",
#ifndef PHILZ_TOUCH_RECOVERY
        "Toggle Compression",
#endif
        NULL
    };

    if (nandroid_get_default_backup_format() == NANDROID_BACKUP_FORMAT_DUP) {
        list = list_dup_default;
    } else {
        list = list_tar_default;
    }

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item) {
        case 0:
            write_string_to_file(NANDROID_BACKUP_FORMAT_FILE, "tar");
            ui_print("Default backup format set to tar.\n");
            break;
        case 1:
            write_string_to_file(NANDROID_BACKUP_FORMAT_FILE, "dup");
            ui_print("Default backup format set to dedupe.\n");
            break;
#ifndef PHILZ_TOUCH_RECOVERY
        case 2:
            if (compression_value == TAR_FORMAT) {
                compression_value = TAR_GZ_LOW;
                ui_print("Compression enabled until reboot\n");
            } else if (compression_value == TAR_GZ_LOW) {
                compression_value = TAR_FORMAT;
                ui_print("Compression disabled\n");
            }
            break;
#endif
    }
}

void show_nandroid_menu()
{
    static char* headers[] = {  "Backup and Restore",
                                NULL
    };

    char* list[] = { "Custom Backup and Restore",
                        "Backup",
                        "Restore",
                        "Delete",
                        "Advanced Restore",
                        "Free Unused Backup Data",
                        "Choose Default Backup Format",
                        "Misc Nandroid Settings",
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL
    };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc";
        list[8] = "Backup to Internal sdcard";
        list[9] = "Restore from Internal sdcard";
        list[10] = "Advanced Restore from Internal sdcard";
        list[11] = "Delete from Internal sdcard";
    }
    else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd";
        list[8] = "Backup to External sdcard";
        list[9] = "Restore from External sdcard";
        list[10] = "Advanced Restore from External sdcard";
        list[11] = "Delete from External sdcard";
    }
#ifdef RECOVERY_EXTEND_NANDROID_MENU
    extend_nandroid_menu(list, 12, sizeof(list) / sizeof(char*));
#endif

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                is_custom_backup = 1;
                custom_backup_restore_menu();
                is_custom_backup = 0;
                break;
            case 1:
                {
                    char backup_path[PATH_MAX];
                    char rom_name[PROPERTY_VALUE_MAX] = "noname";
#ifdef PHILZ_TOUCH_RECOVERY
                    get_rom_name(rom_name);
#endif
                    time_t t = time(NULL);
                    struct tm *timeptr = localtime(&t);
                    if (timeptr == NULL)
                    {
                        struct timeval tp;
                        gettimeofday(&tp, NULL);
                        sprintf(backup_path, "/sdcard/clockworkmod/backup/%d_%s", tp.tv_sec, rom_name);
                    }
                    else
                    {
                        char tmp[PATH_MAX];
                        strftime(tmp, sizeof(tmp), "/sdcard/clockworkmod/backup/%F.%H.%M.%S", timeptr);
                        sprintf(backup_path, "%s_%s",tmp, rom_name);
                    }
                    nandroid_backup(backup_path);
                    //write_recovery_version();
                }
                break;
            case 2:
                show_nandroid_restore_menu("/sdcard");
                //write_recovery_version();
                break;
            case 3:
                show_nandroid_delete_menu("/sdcard");
                //write_recovery_version();
                break;
            case 4:
                show_nandroid_advanced_restore_menu("/sdcard");
                //write_recovery_version();
                break;
            case 5:
                run_dedupe_gc(other_sd);
                break;
            case 6:
                choose_default_backup_format();
                break;
            case 7:
#ifdef PHILZ_TOUCH_RECOVERY
                misc_nandroid_menu();
#endif
                break;
            case 8:
                {
                    char backup_path[PATH_MAX];
                    char rom_name[PROPERTY_VALUE_MAX] = "noname";
#ifdef PHILZ_TOUCH_RECOVERY
                    get_rom_name(rom_name);
#endif
                    time_t t = time(NULL);
                    struct tm *timeptr = localtime(&t);
                    if (timeptr == NULL) {
                        struct timeval tp;
                        gettimeofday(&tp, NULL);
                        sprintf(backup_path, "%s/clockworkmod/backup/%d_%s", other_sd, tp.tv_sec, rom_name);
                    } else {
                        char tmp[PATH_MAX];
                        strftime(tmp, sizeof(tmp), "clockworkmod/backup/%F.%H.%M.%S", timeptr);
                        // this sprintf results in:
                        // /emmc/clockworkmod/backup/%F.%H.%M.%S (time values are populated too)
                        sprintf(backup_path, "%s/%s_%s", other_sd, tmp, rom_name);
                    }
                    nandroid_backup(backup_path);
                }
                break;
            case 9:
                show_nandroid_restore_menu(other_sd);
                break;
            case 10:
                show_nandroid_advanced_restore_menu(other_sd);
                break;
            case 11:
                show_nandroid_delete_menu(other_sd);
                break;
            default:
#ifdef RECOVERY_EXTEND_NANDROID_MENU
                handle_nandroid_menu(12, chosen_item);
#endif
                break;
        }
    }
}

static void partition_sdcard(const char* volume) {
    if (!can_partition(volume)) {
        ui_print("Can't partition device: %s\n", volume);
        return;
    }

    static char* ext_sizes[] = { "128M",
                                 "256M",
                                 "512M",
                                 "1024M",
                                 "2048M",
                                 "4096M",
                                 NULL };

    static char* swap_sizes[] = { "0M",
                                  "32M",
                                  "64M",
                                  "128M",
                                  "256M",
                                  NULL };

    static char* ext_headers[] = { "Ext Size", "", NULL };
    static char* swap_headers[] = { "Swap Size", "", NULL };

    int ext_size = get_menu_selection(ext_headers, ext_sizes, 0, 0);
    if (ext_size == GO_BACK)
        return;

    int swap_size = get_menu_selection(swap_headers, swap_sizes, 0, 0);
    if (swap_size == GO_BACK)
        return;

    char sddevice[256];
    Volume *vol = volume_for_path(volume);
    strcpy(sddevice, vol->device);
    // we only want the mmcblk, not the partition
    sddevice[strlen("/dev/block/mmcblkX")] = NULL;
    char cmd[PATH_MAX];
    setenv("SDPATH", sddevice, 1);
    sprintf(cmd, "sdparted -es %s -ss %s -efs ext3 -s", ext_sizes[ext_size], swap_sizes[swap_size]);
    ui_print("Partitioning SD Card... please wait...\n");
    if (0 == __system(cmd))
        ui_print("Done!\n");
    else
        ui_print("An error occured while partitioning your SD Card. Please see /tmp/recovery.log for more details.\n");
}

int can_partition(const char* volume) {
    Volume *vol = volume_for_path(volume);
    if (vol == NULL) {
        LOGI("Can't format unknown volume: %s\n", volume);
        return 0;
    }

    int vol_len = strlen(vol->device);
    // do not allow partitioning of a device that isn't mmcblkX or mmcblkXp1
    if (vol->device[vol_len - 2] == 'p' && vol->device[vol_len - 1] != '1') {
        LOGI("Can't partition unsafe device: %s\n", vol->device);
        return 0;
    }
    
    if (strcmp(vol->fs_type, "vfat") != 0) {
        LOGI("Can't partition non-vfat: %s\n", vol->fs_type);
        return 0;
    }

    return 1;
}

void show_advanced_menu()
{
    static char* headers[] = {  "Advanced Menu",
                                NULL
    };

    char item_datamedia[35];
    char* list[] = { "Reboot Recovery",
                        "Reboot Bootloader",
                        "Wipe Dalvik Cache",
                        "Report Error",
                        "Key Test",
                        "Show log",
                        "Fix Permissions",
                        item_datamedia,
                        NULL,
                        NULL,
                        NULL
    };

    char *other_sd = NULL;
    list[8] = "Partition Internal sdcard";
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc";
        list[8] = "Partition External sdcard";
        list[9] = "Partition Internal sdcard";
    }
    else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd";
        list[9] = "Partition External sdcard";
    }

    // do not disable list[8] for now until the bug in get_filtered_menu_selection() is fixed
    if (other_sd != NULL && !can_partition(other_sd))
        list[9] = NULL;

    for (;;)
    {
        if (is_data_media()) {
            if (use_migrated_storage())
                sprintf(item_datamedia, "Storage set to /data/media/0");
            else sprintf(item_datamedia, "Storage set to /data/media");
        }
        else sprintf(item_datamedia, "Datamedia Not Supported");

        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                reboot_main_system(ANDROID_RB_RESTART2, 0, "recovery");
                break;
            case 1:
                reboot_main_system(ANDROID_RB_RESTART2, 0, BOOTLOADER_CMD_ARG);
                break;
            case 2:
                if (0 != ensure_path_mounted("/data"))
                    break;
                ensure_path_mounted("/sd-ext");
                ensure_path_mounted("/cache");
                if (confirm_selection( "Confirm wipe?", "Yes - Wipe Dalvik Cache")) {
                    __system("rm -r /data/dalvik-cache");
                    __system("rm -r /cache/dalvik-cache");
                    __system("rm -r /sd-ext/dalvik-cache");
                    ui_print("Dalvik Cache wiped.\n");
                }
                ensure_path_unmounted("/data");
                break;
            case 3:
                handle_failure(1);
                break;
            case 4:
            {
                ui_print("Outputting key codes.\n");
                ui_print("Go back to end debugging.\n");
                int key;
                int action;
                do
                {
                    key = ui_wait_key();
                    action = device_handle_key(key, 1);
                    ui_print("Key: %d\n", key);
                }
                while (action != GO_BACK);
                break;
            }
            case 5:
#ifdef PHILZ_TOUCH_RECOVERY
                show_log_menu();
#else
                ui_printlogtail(12);
#endif
                break;
            case 6:
                ensure_path_mounted("/system");
                ensure_path_mounted("/data");
                //I hate you fixing permissions when I'm dizzy: let's confirm my touch
                if (confirm_selection("Confirm ?", "Yes - Fix Permissions")) {
                    ui_print("Fixing permissions...\n");
                    __system("fix_permissions");
                    ui_print("Done!\n");
                }
                break;
            case 7:
                if (is_data_media()) {
                    if (use_migrated_storage()) {
                        delete_a_file("/data/media/0/clockworkmod/.use_migrated_storage");
                        ui_print("storage set to /data/media\n");
                    }
                    else {
                        write_string_to_file("/data/media/0/clockworkmod/.use_migrated_storage", "1");
                        ui_print("storage set to /data/media/0\n");
                    }
                    setup_data_media();
                    ui_print("Reboot to apply settings!\n");
                }
                else ui_print("datamedia not supported\n");
                break;
            case 8:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                if (can_partition("/sdcard")) {
                    partition_sdcard("/sdcard");
                } else ui_print("Can't partition non vfat volume (/sdcard)\n");
                break;
            case 9:
                if (ensure_path_mounted(other_sd) != 0) {
                    ui_print("Can't mount %s\n", other_sd);
                    break;
                }
                partition_sdcard(other_sd);
                break;
        }
    }
}


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*       Start PhilZ Menu settings       */
/*      Code written by PhilZ@xda     */
/*      Part of PhilZ Touch Recovery     */
/*****************************************/

// redefined MENU_MAX_COLS from ui.c - Keep same value as ui.c until a better implementation.
// used to format toggle menus to device screen width (only touch build)
#define MENU_MAX_COLS 64

// 0 == stop browsing default file locations
static int browse_for_file = 1;
int twrp_backup_mode = 0;

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

// is there a second storage (/sdcard is always present in fstab)
int has_second_storage() {
    return (volume_for_path("/external_sd") != NULL
                || volume_for_path("/emmc") != NULL);
}

//delete a file
void delete_a_file(const char* filename) {
    ensure_path_mounted(filename);
    remove(filename);
}

// check if file or folder exists
int file_found(const char* filename) {
    struct stat s;
    ensure_path_mounted(filename);
    // this will error on some ramdisk path (no valid volume), but stat will return valid file if it exists
    if (0 == stat(filename, &s))
        return 1;

    return 0;
}

// check directory exists
int directory_found(const char* dir) {
    struct stat s;
    ensure_path_mounted(dir);
    lstat(dir, &s);
    if (S_ISDIR(s.st_mode))
        return 1;

    return 0;
}

// get file size (by Dees_Troy - TWRP)
unsigned long Get_File_Size(const char* Path) {
    struct stat st;
    if (stat(Path, &st) != 0)
        return 0;
    return st.st_size;
}

// get partition size info (adapted from Dees_Troy - TWRP)
unsigned long long Total_Size;
unsigned long long Used_Size;
unsigned long long Free_Size;

int Get_Size_Via_statfs(const char* Path) {
	struct statfs st;
    Volume* volume = volume_for_path(Path);
    if (NULL == volume) {
        LOGE("Cannot get size of null volume '%s'\n", Path);
        return -1;
    }
    if (is_data_media_volume_path(volume->mount_point))
        volume = volume_for_path("/data");
    if (volume == NULL || volume->mount_point == NULL || statfs(volume->mount_point, &st) != 0) {
        LOGE("Unable to statfs for size '%s'\n", Path);
        return -1;
    }

    Total_Size = (unsigned long long)(st.f_blocks) * (unsigned long long)(st.f_bsize);
    Free_Size = (unsigned long long)(st.f_bfree) * (unsigned long long)(st.f_bsize);
    Used_Size = Total_Size - Free_Size;
	return 0;
}

int Find_Partition_Size(const char* Path) {
	FILE* fp;
	char line[512];
	char tmpdevice[1024];

    Volume* volume = volume_for_path(Path);
    if (volume != NULL) {
        fp = fopen("/proc/partitions", "rt");
        if (fp != NULL) {
            while (fgets(line, sizeof(line), fp) != NULL)
            {
                unsigned long major, minor, blocks;
                char device[512];
                char tmpString[64];

                if (strlen(line) < 7 || line[0] == 'm')
                    continue;

                sscanf(line + 1, "%lu %lu %lu %s", &major, &minor, &blocks, device);
                sprintf(tmpdevice, "/dev/block/");
                strcat(tmpdevice, device);

                if (volume->device != NULL && strcmp(tmpdevice, volume->device) == 0) {
                    Total_Size = blocks * 1024ULL;
                    fclose(fp);
                    return 0;
                }
                if (volume->device2 != NULL && strcmp(tmpdevice, volume->device2) == 0) {
                    Total_Size = blocks * 1024ULL;
                    fclose(fp);
                    return 0;
                }
            }
            fclose(fp);
        }
    }

    LOGE("Failed to find partition size '%s'\n", Path);
	return -1;
}
// get folder size (by Dees_Troy - TWRP)
unsigned long long Get_Folder_Size(const char* Path) {
    DIR* d;
    struct dirent* de;
    struct stat st;
    char path2[PATH_MAX]; char filename[PATH_MAX];
    unsigned long long dusize = 0;
    unsigned long long dutemp = 0;

    // Make a copy of path in case the data in the pointer gets overwritten later
    strcpy(path2, Path);

    d = opendir(path2);
    if (d == NULL)
    {
        LOGE("error opening '%s'\n", path2);
        LOGE("error: %s\n", strerror(errno));
        return 0;
    }

    while ((de = readdir(d)) != NULL)
    {
        if (de->d_type == DT_DIR && strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
        {
            strcpy(filename, path2);
            strcat(filename, "/");
            strcat(filename, de->d_name);
            dutemp = Get_Folder_Size(filename);
            dusize += dutemp;
            dutemp = 0;
        }
        else if (de->d_type == DT_REG)
        {
            strcpy(filename, path2);
            strcat(filename, "/");
            strcat(filename, de->d_name);
            stat(filename, &st);
            dusize += (unsigned long long)(st.st_size);
        }
    }
    closedir(d);

    return dusize;
}

// start wipe data and system options and menu
void wipe_data_menu() {
    static char* headers[] = {  "Choose wipe option",
                                NULL
    };

    char* list[] = { "Wipe Data/Factory Reset",
                    "Clean to Install a New ROM",
                    NULL
    };

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            wipe_data(1);
            break;
        case 1:
            //clean for new ROM: formats /data, /datadata, /cache, /system, /preload, /sd-ext, /sdcard/.android_secure
            if (confirm_selection("Wipe data, system +/- preload?", "Yes, I will install a new ROM!")) {
                wipe_data(0);
                ui_print("-- Wiping system...\n");
                erase_volume("/system");
                if (volume_for_path("/preload") != NULL) {
                    ui_print("-- Wiping preload...\n");
                    erase_volume("/preload");
                }
                ui_print("Now flash a new ROM!\n");
            }
            break;
    }
}


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*      Start Multi-Flash Zip code       */
/*      Original code by PhilZ @xda      */
/*****************************************/

#define MULTI_ZIP_FOLDER "clockworkmod/multi_flash"
void show_multi_flash_menu() {
    static char* headers_dir[] = { "Choose a set of zip files",
                                   NULL
    };
    static char* headers[] = {  "Select files to install...",
                                NULL
    };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL)
        other_sd = "/emmc";
    else if (volume_for_path("/external_sd") != NULL)
        other_sd = "/external_sd";
    
    char tmp[PATH_MAX];
    char* zip_folder = NULL;

    struct stat st;
    ensure_path_mounted("/sdcard");
    sprintf(tmp, "/sdcard/%s/", MULTI_ZIP_FOLDER);
    stat(tmp, &st);
    if (S_ISDIR(st.st_mode)) {
        zip_folder = choose_file_menu(tmp, NULL, headers_dir);
        if (no_files_found) {
            ui_print("At least one subfolder with zip files must be created under %s\n", tmp);
            ui_print("Looking in other sd...\n");
        }
    } else
        LOGI("%s not found. Searching other sd...\n", tmp);

    struct stat s;
    if (other_sd != NULL) {
        ensure_path_mounted(other_sd);
        sprintf(tmp, "%s/%s/", other_sd, MULTI_ZIP_FOLDER);
        stat(tmp, &s);
        if (zip_folder == NULL && S_ISDIR(s.st_mode)) {
            zip_folder = choose_file_menu(tmp, NULL, headers_dir);
            if (no_files_found)
                ui_print("At least one subfolder with zip files must be created under %s\n", tmp);
        }
    }

    if (zip_folder == NULL) {
        if (!(S_ISDIR(st.st_mode)) && !(S_ISDIR(s.st_mode)))
            ui_print("Create at least 1 folder with your zip files under %s\n", MULTI_ZIP_FOLDER);
        return;
    }

    int dir_len = strlen(zip_folder);
    int numFiles = 0;
    char** files = gather_files(zip_folder, ".zip", &numFiles);
    if (numFiles == 0) {
        ui_print("No zip files found under %s\n", zip_folder);
    } else {
        char** list = (char**) malloc((numFiles + 3) * sizeof(char*));
        list[0] = strdup("Select/Unselect All");
        list[1] = strdup(">> Flash Selected Files <<");
        list[numFiles+2] = NULL;

        int i;
        for(i=2; i < numFiles+2; i++) {
            list[i] = strdup(files[i-2] + dir_len - 4);
            strncpy(list[i], "(x) ", 4);
        }

        int select_all = 1;
        int chosen_item;
        for (;;)
        {
            chosen_item = get_menu_selection(headers, list, 0, 0);
            if (chosen_item == GO_BACK)
                break;
            if (chosen_item == 1)
                break;
            if (chosen_item == 0) {
                select_all ^= 1;
                for(i=2; i < numFiles+2; i++) {
                    if (select_all) strncpy(list[i], "(x)", 3);
                    else strncpy(list[i], "( )", 3);
                }
            } else if (strncmp(list[chosen_item], "( )", 3) == 0) {
                strncpy(list[chosen_item], "(x)", 3);
            } else if (strncmp(list[chosen_item], "(x)", 3) == 0) {
                strncpy(list[chosen_item], "( )", 3);
            }
        }

        if (chosen_item == 1) {
            static char confirm[PATH_MAX];
            sprintf(confirm, "Yes - Install from %s", basename(zip_folder));
            if (confirm_selection("Install selected files?", confirm))
            {
                for(i=2; i < numFiles+2; i++) {
                    if (strncmp(list[i], "(x)", 3) == 0) {
#ifdef PHILZ_TOUCH_RECOVERY
                        force_wait = -1;
#endif
                        if (install_zip(files[i-2]) != 0)
                            break;
                    }
                }
            }
        }
        free_string_array(list);
    }
    free_string_array(files);
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

// check ors script at boot (called from recovery.c)
// format the script file to fix path in install zip commands from goomanager
#define SCRIPT_COMMAND_SIZE 512

int check_for_script_file(const char* ors_boot_script) {
    ensure_path_mounted(ors_boot_script);
    struct stat s;
    if (0 != stat(ors_boot_script, &s))
        return -1;

    char tmp[PATH_MAX];
    LOGI("Script file found: '%s'\n", ors_boot_script);
    __system("/sbin/ors-mount.sh");
    // move script file to /tmp
    sprintf(tmp, "mv %s /tmp", ors_boot_script);
    __system(tmp);

    return 0;
}

// Parse backup options in ors
// Stock CWM as of v6.x, doesn't support backup options
static int ignore_android_secure = 0;

static int ors_backup_command(const char* backup_path, const char* options) {
    if (file_found(backup_path)) {
        LOGE("Specified ors backup target '%s' already exists!\n", backup_path);
        return -1;
    }
    if (nandroid_get_default_backup_format() != NANDROID_BACKUP_FORMAT_TAR) {
        LOGE("Default backup format must be tar!\n");
        return -1;
    }
    is_custom_backup = 1;
    int old_compression_value = compression_value;
    compression_value = TAR_FORMAT;
#ifdef PHILZ_TOUCH_RECOVERY
    int old_enable_md5sum = enable_md5sum;
    enable_md5sum = 1;
#endif
    backup_boot = 0, backup_recovery = 0, backup_wimax = 0, backup_system = 0;
    backup_preload = 0, backup_data = 0, backup_cache = 0, backup_sdext = 0;
    ignore_android_secure = 1; //disable

    ui_print("Setting backup options:\n");
    char value1[SCRIPT_COMMAND_SIZE];
    int line_len, i;
    strcpy(value1, options);
    line_len = strlen(options);
    for (i=0; i<line_len; i++) {
        if (value1[i] == 'S' || value1[i] == 's') {
            backup_system = 1;
            ui_print("System\n");
            if (nandroid_add_preload) {
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
        } else if (value1[i] == '1') {
            ui_print("%s\n", "Option for special1 ignored in CWMR");
        } else if (value1[i] == '2') {
            ui_print("%s\n", "Option for special2 ignored in CWMR");
        } else if (value1[i] == '3') {
            ui_print("%s\n", "Option for special3 ignored in CWMR");
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
            compression_value = TAR_GZ_LOW;
            ui_print("Compression is on\n");
        } else if (value1[i] == 'M' || value1[i] == 'm') {
#ifdef PHILZ_TOUCH_RECOVERY
            enable_md5sum = 0;
            ui_print("MD5 Generation is off\n");
#else
            ui_print("Skip md5 check: not supported\n");
#endif
        }
    }

    int ret;
    ret = nandroid_backup(backup_path);

    is_custom_backup = 0;
    compression_value = old_compression_value;
    reset_custom_job_settings(0);
#ifdef PHILZ_TOUCH_RECOVERY
    enable_md5sum = old_enable_md5sum;
#endif
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
                strncpy(value, val_start, line_len - cindex - remove_nl);
                LOGI("value is: '%s'\n", value);
            } else {
                strncpy(command, script_line, line_len - remove_nl + 1);
                ui_print("command is: '%s' and there is no value\n", command);
            }
            if (strcmp(command, "install") == 0) {
                // Install zip
                ui_print("Installing zip file '%s'\n", value);
                ensure_path_mounted("/sdcard");
                if (volume_for_path("/external_sd") != NULL)
                    ensure_path_mounted("/external_sd");
                if (volume_for_path("/emmc") != NULL)
                    ensure_path_mounted("/emmc");
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
                    if (no_wipe_confirm) {
                        //do not confirm before wipe for scripts started at boot
                        __system("rm -r /data/dalvik-cache");
                        __system("rm -r /cache/dalvik-cache");
                        __system("rm -r /sd-ext/dalvik-cache");
                        ui_print("Dalvik Cache wiped.\n");
                    } else {
                        if (confirm_selection( "Confirm wipe?", "Yes - Wipe Dalvik Cache")) {
                            __system("rm -r /data/dalvik-cache");
                            __system("rm -r /cache/dalvik-cache");
                            __system("rm -r /sd-ext/dalvik-cache");
                            ui_print("Dalvik Cache wiped.\n");
                        }
                    }
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
                char other_sd[20] = "";
#ifdef PHILZ_TOUCH_RECOVERY
                // read user set volume target
                get_ors_backup_path(other_sd);
#else
                // if possible, always prefer external storage as backup target
                if (volume_for_path("/external_sd") != NULL && ensure_path_mounted("/external_sd") == 0)
                    strcpy(other_sd, "/external_sd");
                else if (volume_for_path("/sdcard") != NULL && ensure_path_mounted("/sdcard") == 0)
                    strcpy(other_sd, "/sdcard");
                else if (volume_for_path("/emmc") != NULL && ensure_path_mounted("/emmc") == 0)
                    strcpy(other_sd, "/emmc");
#endif
                if (strcmp(other_sd, "") == 0) {
                    ret_val = 1;
                    LOGE("No valid volume found for ors backup target!\n");
                    continue;
                }

                char backup_path[PATH_MAX];
                tok = strtok(value, " ");
                strcpy(value1, tok);
                tok = strtok(NULL, " ");
                if (tok != NULL) {
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
                    sprintf(backup_path, "%s/clockworkmod/backup/%s", other_sd, value2);
                } else {
                    time_t t = time(NULL);
                    struct tm *tmp = localtime(&t);
                    if (tmp == NULL)
                    {
                        struct timeval tp;
                        gettimeofday(&tp, NULL);
                        sprintf(backup_path, "%s/clockworkmod/backup/%d", other_sd, tp.tv_sec);
                    }
                    else {
                        if (strcmp(other_sd, "/sdcard") == 0) {
                            strftime(backup_path, sizeof(backup_path), "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
                        } else if (strcmp(other_sd, "/external_sd") == 0) {
                            strftime(backup_path, sizeof(backup_path), "/external_sd/clockworkmod/backup/%F.%H.%M.%S", tmp);
                        } else {
                            strftime(backup_path, sizeof(backup_path), "/emmc/clockworkmod/backup/%F.%H.%M.%S", tmp);
                        }
                    }
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
#ifdef PHILZ_TOUCH_RECOVERY
                int old_enable_md5sum = enable_md5sum;
                enable_md5sum = 1;
#endif
                backup_boot = 0, backup_recovery = 0, backup_system = 0;
                backup_preload = 0, backup_data = 0, backup_cache = 0, backup_sdext = 0;
                ignore_android_secure = 1; //disable

                // check what type of restore we need
                if (strstr(value1, TWRP_BACKUP_PATH) != NULL)
                    twrp_backup_mode = 1;

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
                            if (nandroid_add_preload) {
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
                        } else if (value2[i] == '1') {
                            ui_print("%s\n", "Option for special1 ignored in CWMR");
                        } else if (value2[i] == '2') {
                            ui_print("%s\n", "Option for special2 ignored in CWMR");
                        } else if (value2[i] == '3') {
                            ui_print("%s\n", "Option for special3 ignored in CWMR");
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
#ifdef PHILZ_TOUCH_RECOVERY
                            enable_md5sum = 0;
                            ui_print("MD5 Check is off\n");
#else
                            ui_print("Skip md5 check not supported\n");
#endif
                        }
                    }
                } else {
                    LOGI("No restore options set\n");
                    LOGI("Restoring default partitions");
                    backup_boot = 1, backup_system = 1;
                    backup_data = 1, backup_cache = 1, backup_sdext = 1;
                    ignore_android_secure = 0;
                    backup_preload = nandroid_add_preload;
                }

                if (twrp_backup_mode)
                    ret_val = twrp_restore(value1);
                else
                    ret_val = nandroid_restore(value1, backup_boot, backup_system, backup_data, backup_cache, backup_sdext, 0);
                
                if (ret_val != 0)
                    ui_print("Restore failed!\n");

                is_custom_backup = 0, twrp_backup_mode = 0;
                reset_custom_job_settings(0);
#ifdef PHILZ_TOUCH_RECOVERY
                enable_md5sum = old_enable_md5sum;
#endif
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
                ensure_path_mounted("/sdcard");
                if (volume_for_path("/external_sd") != NULL)
                    ensure_path_mounted("/external_sd");
                if (volume_for_path("/emmc") != NULL)
                    ensure_path_mounted("/emmc");
                if (0 != (ret_val = apply_from_adb()))
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
//end of open recovery script file code

//show menu: select ors from default path
static void choose_default_ors_menu(const char* ors_path)
{
    if (ensure_path_mounted(ors_path) != 0) {
        LOGE("Can't mount %s\n", ors_path);
        browse_for_file = 1;
        return;
    }

    char ors_dir[PATH_MAX];
    sprintf(ors_dir, "%s/clockworkmod/ors/", ors_path);
    if (access(ors_dir, F_OK) == -1) {
        //custom folder does not exist
        browse_for_file = 1;
        return;
    }

    static char* headers[] = {  "Choose a script to run",
                                "",
                                NULL
    };

    char* ors_file = choose_file_menu(ors_dir, ".ors", headers);
    if (no_files_found == 1) {
        //0 valid files to select, let's continue browsing next locations
        ui_print("No *.ors files in %s/clockworkmod/ors\n", ors_path);
        browse_for_file = 1;
    } else {
        browse_for_file = 0;
        //we found ors scripts in clockworkmod/ors folder: do not proceed other locations even if no file is chosen
    }
    if (ors_file == NULL) {
        //either no valid files found or we selected no files by pressing back menu
        return;
    }
    static char* confirm_install  = "Confirm run script?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Run %s", basename(ors_file));
    if (confirm_selection(confirm_install, confirm)) {
        run_ors_script(ors_file);
    }
}

//show menu: browse for custom Open Recovery Script
static void choose_custom_ors_menu(const char* ors_path)
{
    if (ensure_path_mounted(ors_path) != 0) {
        LOGE("Can't mount %s\n", ors_path);
        return;
    }

    static char* headers[] = {  "Choose .ors script to run",
                                NULL
    };

    char* ors_file = choose_file_menu(ors_path, ".ors", headers);
    if (ors_file == NULL)
        return;
    static char* confirm_install  = "Confirm run script?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Run %s", basename(ors_file));
    if (confirm_selection(confirm_install, confirm)) {
        run_ors_script(ors_file);
    }
}

//show menu: select sdcard volume to search for custom ors file
static void show_custom_ors_menu() {
    static char* headers[] = {  "Search .ors script to run",
                                "",
                                NULL
    };

    char* list[] = { "Search sdcard",
                            NULL,
                            NULL
    };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc/";
        list[1] = "Search Internal sdcard";
    } else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd/";
        list[1] = "Search External sdcard";
    }

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                choose_custom_ors_menu("/sdcard/");
                break;
            case 1:
                choose_custom_ors_menu(other_sd);
                break;
        }
    }
}
//----------end open recovery script support


#ifdef PHILZ_TOUCH_RECOVERY
#include "/root/Desktop/PhilZ_Touch/touch_source/philz_gui_settings.c"
#endif


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*   Custom Backup and Restore Support   */
/*       code written by PhilZ @xda      */
/*        for PhilZ Touch Recovery       */
/*****************************************/

static void choose_delete_folder(const char* path) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return;
    }

    static char* headers[] = {  "Choose folder to delete",
                                NULL
    };

    char* file = choose_file_menu(path, NULL, headers);
    if (file == NULL)
        return;

    char tmp[PATH_MAX];
    sprintf(tmp, "Yes - Delete %s", basename(file));
    if (confirm_selection("Confirm delete?", tmp)) {
        sprintf(tmp, "rm -rf '%s'", file);
        __system(tmp);
    }
}

static void delete_custom_backups(const char* backup_path)
{
    static char* headers[] = {"Browse backup folders...", NULL};

    char* list[] = {"Delete from Internal sdcard",
                    NULL,
                    NULL};

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Delete from External sdcard";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            {
                char tmp[PATH_MAX];
                sprintf(tmp, "%s/%s/", int_sd, backup_path);
                choose_delete_folder(tmp);
            }
            break;
        case 1:
            {
                char tmp[PATH_MAX];
                sprintf(tmp, "%s/%s/", ext_sd, backup_path);
                choose_delete_folder(tmp);
            }
            break;
    }
}

int get_android_secure_path(char *and_sec_path) {
    if (ignore_android_secure)
        return android_secure_ext = 0;

    android_secure_ext = 1;
    struct stat st;
    if (volume_for_path("/external_sd") != NULL &&
                ensure_path_mounted("/external_sd") == 0 &&
                stat("/external_sd/.android_secure", &st) == 0) {
        strcpy(and_sec_path, "/external_sd/.android_secure");
    }
    else if (!is_data_media() && ensure_path_mounted("/sdcard") == 0 && 
                stat("/sdcard/.android_secure", &st) == 0) {
        strcpy(and_sec_path, "/sdcard/.android_secure");
    }
    else if (volume_for_path("/emmc") != NULL &&
                ensure_path_mounted("/emmc") == 0 &&
                stat("/emmc/.android_secure", &st) == 0) {
        strcpy(and_sec_path, "/emmc/.android_secure");
    }
    else android_secure_ext = 0;
    
    return android_secure_ext;
}

void reset_custom_job_settings(int custom_job) {
    if (custom_job) {
        backup_boot = 1, backup_recovery = 1, backup_system = 1;
        backup_data = 1, backup_cache = 1;
        backup_wimax = 0;
        backup_sdext = 0;
        if (twrp_backup_mode)
            backup_wimax = 0;
    } else {
        backup_boot = 1, backup_recovery = 1, backup_system = 1;
        backup_data = 1, backup_cache = 1;
        backup_wimax = 1;
        backup_sdext = 1;
    }

    backup_preload = 0;
    backup_modem = 0;
    backup_radio = 0;
    backup_efs = 0;
    backup_misc = 0;
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
    ui_print("!\n");
}

static void get_custom_backup_path(const char* sd_path, char *backup_path) {
    char rom_name[PROPERTY_VALUE_MAX] = "noname";
#ifdef PHILZ_TOUCH_RECOVERY
    get_rom_name(rom_name);
#endif
    time_t t = time(NULL);
    struct tm *timeptr = localtime(&t);
    if (timeptr == NULL) {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        if (backup_efs)
            sprintf(backup_path, "%s/%s/%d", sd_path, EFS_BACKUP_PATH, tp.tv_sec);
        else
            sprintf(backup_path, "%s/%s/%d_%s", sd_path, "clockworkmod/backup", tp.tv_sec, rom_name);
    } else {
        char tmp[PATH_MAX];
        strftime(tmp, sizeof(tmp), "%F.%H.%M.%S", timeptr);
        // this sprintf results in:
        // clockworkmod/custom_backup/%F.%H.%M.%S (time values are populated too)
        if (backup_efs)
            sprintf(backup_path, "%s/%s/%s", sd_path, EFS_BACKUP_PATH, tmp);
        else
            sprintf(backup_path, "%s/%s/%s_%s", sd_path, "clockworkmod/backup", tmp, rom_name);
    }
}

static void custom_backup_handler() {
    static char* headers[] = {"Select custom backup target", "", NULL};
    char* list[] = {"Backup to Internal sdcard",
                        NULL,
                        NULL};

    ui_print_backup_list();

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Backup to External sdcard";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            {
                if (ensure_path_mounted(int_sd) == 0) {
                    char backup_path[PATH_MAX] = "";
                    get_custom_backup_path(int_sd, backup_path);
                    nandroid_backup(backup_path);
                } else {
                    ui_print("Couldn't mount %s\n", int_sd);
                }
            }
            break;
        case 1:
            {
                if (ensure_path_mounted(ext_sd) == 0) {
                    char backup_path[PATH_MAX] = "";
                    get_custom_backup_path(ext_sd, backup_path);
                    nandroid_backup(backup_path);
                } else {
                    ui_print("Couldn't mount %s\n", ext_sd);
                }
            }
            break;
    }
}

// there is a trailing / in passed backup_path: needed for choose_file_menu()
static void custom_restore_handler(const char* backup_path) {
    if (ensure_path_mounted(backup_path) != 0) {
        LOGE("Can't mount %s\n", backup_path);
        return;
    }

    static char* headers[] = {  "Choose a backup to restore",
                                NULL
    };

    struct statfs s;
    char* file = NULL;
    static char* confirm_install = "Restore from this backup?";
    char tmp[PATH_MAX];
    char *backup_source;

    if (backup_efs == RESTORE_EFS_IMG) {
        if (volume_for_path("/efs") == NULL) {
            LOGE("No /efs partition to flash\n");
            return;
        }
        file = choose_file_menu(backup_path, ".img", headers);
        if (file == NULL) {
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        //restore efs raw image
        backup_source = basename(file);
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
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        //ensure there is no efs.img file in same folder (as nandroid_restore_partition_extended will force it to be restored)
        sprintf(tmp, "%s/efs.img", file);
        if (0 == statfs(tmp, &s)) {
            ui_print("efs.img file detected in %s!\n", file);
            ui_print("Either select efs.img to restore it,\n");
            ui_print("or remove it to restore nandroid source.\n");
            return;
        }

        //restore efs from nandroid tar format
        ui_print("%s will be restored to /efs!\n", file);
        sprintf(tmp, "Yes - Restore %s", basename(file));
        if (confirm_selection(confirm_install, tmp))
            nandroid_restore(file, 0, 0, 0, 0, 0, 0);
    } else if (backup_modem == RAW_BIN_FILE) {
        file = choose_file_menu(backup_path, ".bin", headers);
        if (file == NULL) {
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        //restore modem.bin raw image
        backup_source = basename(file);
        Volume *vol = volume_for_path("/modem");
        if (vol != NULL) {
            ui_print("%s will be flashed to /modem!\n", backup_source);
            char confirm[PATH_MAX];
            sprintf(confirm, "Yes - Restore %s", backup_source);
            if (confirm_selection(confirm_install, confirm))
                dd_raw_restore_handler(file, "/modem");
        } else
            LOGE("no /modem partition to flash\n");
    } else if (backup_radio == RAW_BIN_FILE) {
        file = choose_file_menu(backup_path, ".bin", headers);
        if (file == NULL) {
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        //restore radio.bin raw image
        backup_source = basename(file);
        Volume *vol = volume_for_path("/radio");
        if (vol != NULL) {
            ui_print("%s will be flashed to /radio!\n", backup_source);
            char confirm[PATH_MAX];
            sprintf(confirm, "Yes - Restore %s", backup_source);
            if (confirm_selection(confirm_install, confirm))
                dd_raw_restore_handler(file, "/radio");
        } else
            LOGE("no /radio partition to flash\n");
    } else {
        //process backup job
        file = choose_file_menu(backup_path, "", headers);
        if (file == NULL) {
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }
        backup_source = dirname(file);
        ui_print("%s will be restored to selected partitions!\n", backup_source);
        sprintf(tmp, "Yes - Restore %s", basename(backup_source));
        if (confirm_selection(confirm_install, tmp)) {
            nandroid_restore(backup_source, backup_boot, backup_system, backup_data, backup_cache, backup_sdext, backup_wimax);
        }
    }
}

static void browse_backup_folders(const char* backup_path)
{
    static char* headers[] = {"Browse backup folders...", "", NULL};

    char* list[] = {"Restore from Internal sdcard",
                    NULL,
                    NULL};

    ui_print_backup_list();

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Restore from External sdcard";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            {
                char tmp[PATH_MAX];
                sprintf(tmp, "%s/%s/", int_sd, backup_path);
                if (twrp_backup_mode)
                    twrp_restore_handler(tmp);
                else
                    custom_restore_handler(tmp);
            }
            break;
        case 1:
            {
                char tmp[PATH_MAX];
                sprintf(tmp, "%s/%s/", ext_sd, backup_path);
                if (twrp_backup_mode)
                    twrp_restore_handler(tmp);
                else
                    custom_restore_handler(tmp);
            }
            break;
    }
}

static void validate_backup_job(const char* backup_path) {
    int sum = backup_boot + backup_recovery + backup_system + backup_preload + backup_data +
                backup_cache + backup_sdext + backup_wimax + backup_misc;
    if (0 == (sum + backup_efs + backup_modem + backup_radio)) {
        ui_print("Select at least one partition to restore!\n");
        return;
    }

    if (backup_path != NULL)
    {
        // it is a restore job
        if (backup_modem == RAW_BIN_FILE) {
            if (0 != (sum + backup_efs + backup_radio))
                ui_print("modem.bin format must be restored alone!\n");
            else
                browse_backup_folders(MODEM_BIN_PATH);
        }
        else if (backup_radio == RAW_BIN_FILE) {
            if (0 != (sum + backup_efs + backup_modem))
                ui_print("radio.bin format must be restored alone!\n");
            else
                browse_backup_folders(RADIO_BIN_PATH);
        }
        else if (twrp_backup_mode)
            browse_backup_folders(backup_path);
        else if (backup_efs && (sum + backup_modem + backup_radio) != 0)
            ui_print("efs must be restored alone!\n");
        else if (backup_efs && (sum + backup_modem + backup_radio) == 0)
            browse_backup_folders(EFS_BACKUP_PATH);
        else
            browse_backup_folders(backup_path);
    }
    else
    {
        // it is a backup job to validate
        if (nandroid_get_default_backup_format() != NANDROID_BACKUP_FORMAT_TAR)
            LOGE("Default backup format must be tar!\n");
        else if (twrp_backup_mode)
            twrp_backup_handler();
        else if (backup_efs && (sum + backup_modem + backup_radio) != 0)
            ui_print("efs must be backed up alone!\n");
        else
            custom_backup_handler();
    }
}

// we'd better do some malloc here... later
static void custom_restore_menu(const char* backup_path) {
    static char* headers[] = {  "Custom restore job",
                                NULL
    };

    char item_boot[MENU_MAX_COLS];
    char item_recovery[MENU_MAX_COLS];
    char item_system[MENU_MAX_COLS];
    char item_preload[MENU_MAX_COLS];
    char item_data[MENU_MAX_COLS];
    char item_andsec[MENU_MAX_COLS];
    char item_cache[MENU_MAX_COLS];
    char item_sdext[MENU_MAX_COLS];
    char item_modem[MENU_MAX_COLS];
    char item_radio[MENU_MAX_COLS];
    char item_efs[MENU_MAX_COLS];
    char item_misc[MENU_MAX_COLS];
    char item_reboot[MENU_MAX_COLS];
    char item_wimax[MENU_MAX_COLS];
    char* list[] = { item_boot,
                item_recovery,
                item_system,
                item_preload,
                item_data,
                item_andsec,
                item_cache,
                item_sdext,
                item_modem,
                item_radio,
                item_efs,
                item_misc,
                ">> Start Custom Restore Job <<",
                item_reboot,
                NULL,
                NULL
    };

    char tmp[PATH_MAX];
    if (0 == get_partition_device("wimax", tmp)) {
        // show wimax restore option
        list[14] = "show wimax menu";
    }

    reset_custom_job_settings(1);
    for (;;) {
        if (backup_boot) ui_format_gui_menu(item_boot, "Restore boot", "(x)");
        else ui_format_gui_menu(item_boot, "Restore boot", "( )");

        if (backup_recovery) ui_format_gui_menu(item_recovery, "Restore recovery", "(x)");
        else ui_format_gui_menu(item_recovery, "Restore recovery", "( )");

        if (backup_system) ui_format_gui_menu(item_system, "Restore system", "(x)");
        else ui_format_gui_menu(item_system, "Restore system", "( )");

        if (volume_for_path("/preload") == NULL)
            ui_format_gui_menu(item_preload, "Restore preload", "N/A");
        else if (backup_preload) ui_format_gui_menu(item_preload, "Restore preload", "(x)");
        else ui_format_gui_menu(item_preload, "Restore preload", "( )");

        if (backup_data) ui_format_gui_menu(item_data, "Restore data", "(x)");
        else ui_format_gui_menu(item_data, "Restore data", "( )");

        get_android_secure_path(tmp);
        if (backup_data && android_secure_ext)
            ui_format_gui_menu(item_andsec, "Restore and-sec", dirname(tmp));
        else ui_format_gui_menu(item_andsec, "Restore and-sec", "( )");

        if (backup_cache) ui_format_gui_menu(item_cache, "Restore cache", "(x)");
        else ui_format_gui_menu(item_cache, "Restore cache", "( )");

        if (backup_sdext) ui_format_gui_menu(item_sdext, "Restore sd-ext", "(x)");
        else ui_format_gui_menu(item_sdext, "Restore sd-ext", "( )");

        if (volume_for_path("/modem") == NULL)
            ui_format_gui_menu(item_modem, "Restore modem", "N/A");
        else if (backup_modem == RAW_IMG_FILE)
            ui_format_gui_menu(item_modem, "Restore modem [.img]", "(x)");
        else if (backup_modem == RAW_BIN_FILE)
            ui_format_gui_menu(item_modem, "Restore modem [.bin]", "(x)");
        else ui_format_gui_menu(item_modem, "Restore modem", "( )");

        if (volume_for_path("/radio") == NULL)
            ui_format_gui_menu(item_radio, "Restore radio", "N/A");
        else if (backup_radio == RAW_IMG_FILE)
            ui_format_gui_menu(item_radio, "Restore radio [.img]", "(x)");
        else if (backup_radio == RAW_BIN_FILE)
            ui_format_gui_menu(item_radio, "Restore radio [.bin]", "(x)");
        else ui_format_gui_menu(item_radio, "Restore radio", "( )");

        if (volume_for_path("/efs") == NULL)
            ui_format_gui_menu(item_efs, "Restore efs", "N/A");
        else if (backup_efs == RESTORE_EFS_IMG)
            ui_format_gui_menu(item_efs, "Restore efs [.img]", "(x)");
        else if (backup_efs == RESTORE_EFS_TAR)
            ui_format_gui_menu(item_efs, "Restore efs [.tar]", "(x)");
        else ui_format_gui_menu(item_efs, "Restore efs", "( )");

        if (volume_for_path("/misc") == NULL)
            ui_format_gui_menu(item_misc, "Restore misc", "N/A");
        else if (backup_misc) ui_format_gui_menu(item_misc, "Restore misc", "(x)");
        else ui_format_gui_menu(item_misc, "Restore misc", "( )");

        if (reboot_after_nandroid) ui_format_gui_menu(item_reboot, "Reboot once done", "(x)");
        else ui_format_gui_menu(item_reboot, "Reboot once done", "( )");

        if (NULL != list[14]) {
            if (backup_wimax)
                ui_format_gui_menu(item_wimax, "Restore WiMax", "(x)");
            else ui_format_gui_menu(item_wimax, "Restore WiMax", "( )");
            list[14] = item_wimax;
        }


        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                backup_boot ^= 1;
                break;
            case 1:
                backup_recovery ^= 1;
                break;
            case 2:
                backup_system ^= 1;
                break;
            case 3:
                if (volume_for_path("/preload") == NULL)
                    backup_preload = 0;
                else backup_preload ^= 1;
                break;
            case 4:
                backup_data ^= 1;
                break;
            case 5:
                ignore_android_secure ^= 1;
                if (!ignore_android_secure && has_second_storage())
                    ui_print("To force restore to 2nd storage, keep only one .android_secure folder\n");
                break;
            case 6:
                backup_cache ^= 1;
                break;
            case 7:
                backup_sdext ^= 1;
                break;
            case 8:
                if (volume_for_path("/modem") == NULL)
                    backup_modem = 0;
                else {
                    backup_modem++;
                    if (backup_modem > 2)
                        backup_modem = 0;
                    if (twrp_backup_mode && backup_modem == RAW_BIN_FILE)
                        backup_modem = 0;
                }
                break;
            case 9:
                if (volume_for_path("/radio") == NULL)
                    backup_radio = 0;
                else {
                    backup_radio++;
                    if (backup_radio > 2)
                        backup_radio = 0;
                    if (twrp_backup_mode && backup_radio == RAW_BIN_FILE)
                        backup_radio = 0;
                }
                break;
            case 10:
                if (volume_for_path("/efs") == NULL)
                    backup_efs = 0;
                else {
                    backup_efs++;
                    if (backup_efs > 2)
                        backup_efs = 0;
                    if (twrp_backup_mode && backup_efs == RESTORE_EFS_IMG)
                        backup_efs = 0;
                }
                break;
            case 11:
                if (volume_for_path("/misc") == NULL)
                    backup_misc = 0;
                else backup_misc ^= 1;
                break;
            case 12:
                validate_backup_job(backup_path);
                break;
            case 13:
                reboot_after_nandroid ^= 1;
                break;
            case 14:
                if (twrp_backup_mode) backup_wimax = 0;
                else backup_wimax ^= 1;
                break;
        }
    }
    reset_custom_job_settings(0);
}

static void custom_backup_menu() {
    static char* headers[] = {  "Custom backup job",
                                NULL
    };

    char item_boot[MENU_MAX_COLS];
    char item_recovery[MENU_MAX_COLS];
    char item_system[MENU_MAX_COLS];
    char item_preload[MENU_MAX_COLS];
    char item_data[MENU_MAX_COLS];
    char item_andsec[MENU_MAX_COLS];
    char item_cache[MENU_MAX_COLS];
    char item_sdext[MENU_MAX_COLS];
    char item_modem[MENU_MAX_COLS];
    char item_radio[MENU_MAX_COLS];
    char item_efs[MENU_MAX_COLS];
    char item_misc[MENU_MAX_COLS];
    char item_reboot[MENU_MAX_COLS];
    char item_wimax[MENU_MAX_COLS];
    char* list[] = { item_boot,
                item_recovery,
                item_system,
                item_preload,
                item_data,
                item_andsec,
                item_cache,
                item_sdext,
                item_modem,
                item_radio,
                item_efs,
                item_misc,
                ">> Start Custom Backup Job <<",
                item_reboot,
                NULL,
                NULL
    };

    char tmp[PATH_MAX];
    if (volume_for_path("/wimax") != NULL) {
        // show wimax backup option
        list[14] = "show wimax menu";
    }

    reset_custom_job_settings(1);
    for (;;) {
        if (backup_boot) ui_format_gui_menu(item_boot, "Backup boot", "(x)");
        else ui_format_gui_menu(item_boot, "Backup boot", "( )");

        if (backup_recovery) ui_format_gui_menu(item_recovery, "Backup recovery", "(x)");
        else ui_format_gui_menu(item_recovery, "Backup recovery", "( )");

        if (backup_system) ui_format_gui_menu(item_system, "Backup system", "(x)");
        else ui_format_gui_menu(item_system, "Backup system", "( )");

        if (volume_for_path("/preload") == NULL)
            ui_format_gui_menu(item_preload, "Backup preload", "N/A");
        else if (backup_preload) ui_format_gui_menu(item_preload, "Backup preload", "(x)");
        else ui_format_gui_menu(item_preload, "Backup preload", "( )");

        if (backup_data) ui_format_gui_menu(item_data, "Backup data", "(x)");
        else ui_format_gui_menu(item_data, "Backup data", "( )");

        get_android_secure_path(tmp);
        if (backup_data && android_secure_ext)
            ui_format_gui_menu(item_andsec, "Backup and-sec", dirname(tmp));
        else ui_format_gui_menu(item_andsec, "Backup and-sec", "( )");

        if (backup_cache) ui_format_gui_menu(item_cache, "Backup cache", "(x)");
        else ui_format_gui_menu(item_cache, "Backup cache", "( )");

        if (backup_sdext) ui_format_gui_menu(item_sdext, "Backup sd-ext", "(x)");
        else ui_format_gui_menu(item_sdext, "Backup sd-ext", "( )");

        if (volume_for_path("/modem") == NULL)
            ui_format_gui_menu(item_modem, "Backup modem", "N/A");
        else if (backup_modem) ui_format_gui_menu(item_modem, "Backup modem [.img]", "(x)");
        else ui_format_gui_menu(item_modem, "Backup modem", "( )");

        if (volume_for_path("/radio") == NULL)
            ui_format_gui_menu(item_radio, "Backup radio", "N/A");
        else if (backup_radio) ui_format_gui_menu(item_radio, "Backup radio [.img]", "(x)");
        else ui_format_gui_menu(item_radio, "Backup radio", "( )");

        if (volume_for_path("/efs") == NULL)
            ui_format_gui_menu(item_efs, "Backup efs", "N/A");
        else if (backup_efs && twrp_backup_mode)
            ui_format_gui_menu(item_efs, "Backup efs", "(x)");
        else if (backup_efs && !twrp_backup_mode)
            ui_format_gui_menu(item_efs, "Backup efs [img&tar]", "(x)");
        else ui_format_gui_menu(item_efs, "Backup efs", "( )");

        if (volume_for_path("/misc") == NULL)
            ui_format_gui_menu(item_misc, "Backup misc", "N/A");
        else if (backup_misc) ui_format_gui_menu(item_misc, "Backup misc", "(x)");
        else ui_format_gui_menu(item_misc, "Backup misc", "( )");

        if (reboot_after_nandroid) ui_format_gui_menu(item_reboot, "Reboot once done", "(x)");
        else ui_format_gui_menu(item_reboot, "Reboot once done", "( )");

        if (NULL != list[14]) {
            if (backup_wimax)
                ui_format_gui_menu(item_wimax, "Backup WiMax", "(x)");
            else ui_format_gui_menu(item_wimax, "Backup WiMax", "( )");
            list[14] = item_wimax;
        }

        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                backup_boot ^= 1;
                break;
            case 1:
                backup_recovery ^= 1;
                break;
            case 2:
                backup_system ^= 1;
                break;
            case 3:
                if (volume_for_path("/preload") == NULL)
                    backup_preload = 0;
                else backup_preload ^= 1;
                break;
            case 4:
                backup_data ^= 1;
                break;
            case 5:
                ignore_android_secure ^= 1;
                if (!ignore_android_secure && has_second_storage())
                    ui_print("To force backup from 2nd storage, keep only one .android_secure folder\n");
                break;
            case 6:
                backup_cache ^= 1;
                break;
            case 7:
                backup_sdext ^= 1;
                break;
            case 8:
                if (volume_for_path("/modem") == NULL)
                    backup_modem = 0;
                else backup_modem ^= 1;
                break;
            case 9:
                if (volume_for_path("/radio") == NULL)
                    backup_radio = 0;
                else backup_radio ^= 1;
                break;
            case 10:
                if (volume_for_path("/efs") == NULL)
                    backup_efs = 0;
                else backup_efs ^= 1;
                break;
            case 11:
                if (volume_for_path("/misc") == NULL)
                    backup_misc = 0;
                else backup_misc ^= 1;
                break;
            case 12:
                validate_backup_job(NULL);
                break;
            case 13:
                reboot_after_nandroid ^= 1;
                break;
            case 14:
                if (twrp_backup_mode) backup_wimax = 0;
                else backup_wimax ^= 1;
                break;
        }
    }
    reset_custom_job_settings(0);
}
//------- end Custom Backup and Restore functions


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/* Part of TWRP Backup & Restore Support */
/*    Original CWM port by PhilZ @xda    */
/*    Original TWRP code by Dees_Troy    */
/*         (dees_troy at yahoo)          */
/*****************************************/

int check_twrp_md5sum(const char* backup_path) {
    char tmp[PATH_MAX];
    int numFiles = 0;
    ensure_path_mounted(backup_path);
    strcpy(tmp, backup_path);
    if (strcmp(tmp + strlen(tmp) - 1, "/") != 0)
        strcat(tmp, "/");

    ui_print("\n>> Checking MD5 sums...\n");
    char** files = gather_files(tmp, ".md5", &numFiles);
    if (numFiles == 0) {
        ui_print("No md5 checksum files found in %s\n", tmp);
        free_string_array(files);
        return -1;
    }

    int i = 0;
    for(i=0; i < numFiles; i++) {
        sprintf(tmp, "cd '%s' && md5sum -c '%s'", backup_path, basename(files[i]));
        if (0 != __system(tmp)) {
            ui_print("md5sum error in %s!\n", basename(files[i]));
            free_string_array(files);
            return -1;
        }
    }

    ui_print("MD5 sum ok.\n");
    free_string_array(files);
    return 0;
}

int gen_twrp_md5sum(const char* backup_path) {
    ui_print("\n>> Generating md5 sum...\n");
    ensure_path_mounted(backup_path);
    char tmp[PATH_MAX];
    int numFiles = 0;
    sprintf(tmp, "%s/", backup_path);
    char** files = gather_files(tmp, "", &numFiles);
    if (numFiles == 0) {
        ui_print("No files found in backup path %s\n", tmp);
        free_string_array(files);
        return -1;
    }

    int i = 0;
    for(i=0; i < numFiles; i++) {
        sprintf(tmp, "cd '%s'; md5sum '%s' > '%s.md5'", backup_path, basename(files[i]), basename(files[i]));
        if (0 != __system(tmp)) {
            ui_print("Error while generating md5 sum for %s!\n", files[i]);
            free_string_array(files);
            return -1;
        }
    }

    ui_print("MD5 sum created.\n");
    free_string_array(files);
    return 0;
}

// Device ID functions
static void sanitize_device_id(char *device_id) {
    const char* whitelist ="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-._";
    char str[PROPERTY_VALUE_MAX];
    char* c = str;

    strcpy(str, device_id);
    char tmp[PROPERTY_VALUE_MAX];
    memset(tmp, 0, sizeof(tmp));
    while (*c) {
        if (strchr(whitelist, *c))
            strncat(tmp, c, 1);
        c++;
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

static void get_device_id(char *device_id) {
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
    if (fp != NULL)
    {
        // First step, read the line. For cmdline, it's one long line
        LOGI("Checking cmdline for serialno...\n");
        fgets(line, sizeof(line), fp);
        fclose(fp);

        // Now, let's tokenize the string
        token = strtok(line, " ");
        if (strlen(token) > PROPERTY_VALUE_MAX)
            token[PROPERTY_VALUE_MAX] = 0;

        // Let's walk through the line, looking for the CMDLINE_SERIALNO token
        while (token)
        {
            // We don't need to verify the length of token, because if it's too short, it will mismatch CMDLINE_SERIALNO at the NULL
            if (memcmp(token, CMDLINE_SERIALNO, CMDLINE_SERIALNO_LEN) == 0)
            {
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
    if (fp != NULL)
    {
        LOGI("Checking cpuinfo...\n");
        while (fgets(line, sizeof(line), fp) != NULL) { // First step, read the line.
            if (memcmp(line, CPUINFO_SERIALNO, CPUINFO_SERIALNO_LEN) == 0)  // check the beginning of the line for "Serial"
            {
                // We found the serial number!
                token = line + CPUINFO_SERIALNO_LEN; // skip past "Serial"
                while ((*token > 0 && *token <= 32 ) || *token == ':') token++; // skip over all spaces and the colon
                if (*token != 0) {
                    token[30] = 0;
                    if (token[strlen(token)-1] == 10) { // checking for endline chars and dropping them from the end of the string if needed
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
            } else if (memcmp(line, CPUINFO_HARDWARE, CPUINFO_HARDWARE_LEN) == 0) {// We're also going to look for the hardware line in cpuinfo and save it for later in case we don't find the device ID
                // We found the hardware ID
                token = line + CPUINFO_HARDWARE_LEN; // skip past "Hardware"
                while ((*token > 0 && *token <= 32 ) || *token == ':')  token++; // skip over all spaces and the colon
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

static void get_twrp_backup_path(const char* sd_path, char *backup_path) {
    char rom_name[PROPERTY_VALUE_MAX] = "noname";
#ifdef PHILZ_TOUCH_RECOVERY
    get_rom_name(rom_name);
#endif
    time_t t = time(NULL);

    char device_id[PROPERTY_VALUE_MAX];
    get_device_id(device_id);

    struct tm *timeptr = localtime(&t);
    if (timeptr == NULL) {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        sprintf(backup_path, "%s/%s/%s/%d_%s", sd_path, TWRP_BACKUP_PATH, device_id, tp.tv_sec, rom_name);
    } else {
        char tmp[PATH_MAX];
        strftime(tmp, sizeof(tmp), "%F.%H.%M.%S", timeptr);
        // this sprintf results in:
        // clockworkmod/custom_backup/%F.%H.%M.%S (time values are populated too)
        sprintf(backup_path, "%s/%s/%s/%s_%s", sd_path, TWRP_BACKUP_PATH, device_id, tmp, rom_name);
    }
}

void twrp_backup_handler() {
    static char* headers[] = {"Select TWRP backup target", "", NULL};
    char* list[] = {"Backup to Internal sdcard",
                        NULL,
                        NULL};

    ui_print_backup_list();

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Backup to External sdcard";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            {
                if (ensure_path_mounted(int_sd) == 0) {
                    char backup_path[PATH_MAX];
                    get_twrp_backup_path(int_sd, backup_path);
                    twrp_backup(backup_path);
                } else
                    ui_print("Couldn't mount %s\n", int_sd);
            }
            break;
        case 1:
            {
                if (ensure_path_mounted(ext_sd) == 0) {
                    char backup_path[PATH_MAX];
                    get_twrp_backup_path(ext_sd, backup_path);
                    twrp_backup(backup_path);
                } else
                    ui_print("Couldn't mount %s\n", ext_sd);
            }
            break;
    }
}

void twrp_restore_handler(const char* backup_path) {
    if (ensure_path_mounted(backup_path) != 0) {
        LOGE("Can't mount %s\n", backup_path);
        return;
    }

    static char* headers[] = {  "Choose a backup to restore",
                                NULL
    };

    char tmp[PATH_MAX];
    char device_id[PROPERTY_VALUE_MAX];
    get_device_id(device_id);
    sprintf(tmp, "%s%s/", backup_path, device_id);

    char* file = choose_file_menu(tmp, "", headers);
    if (file == NULL) {
        //either no valid files found or we selected no files by pressing back menu
        if (no_files_found)
            ui_print("Nothing to restore in %s !\n", tmp);
        return;
    }

    char *backup_source;
    backup_source = dirname(file);
    ui_print("%s will be restored to selected partitions!\n", backup_source);
    sprintf(tmp, "Yes - Restore %s", basename(backup_source));
    if (confirm_selection("Restore from this backup?", tmp))
        twrp_restore(backup_source);
}

static void twrp_backup_restore_menu() {
    static char* headers[] = {  "TWRP Backup and Restore",
                                "",
                                NULL
    };
    static char* list[] = { "Backup in TWRP Format",
                    "Restore from TWRP Format",
                    "Delete TWRP Backup Image",
                    NULL
    };

    twrp_backup_mode = 1;

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                custom_backup_menu();
                break;
            case 1:
                custom_restore_menu(TWRP_BACKUP_PATH);
                break;
            case 2:
                {
                    char tmp[PATH_MAX];
                    char device_id[PROPERTY_VALUE_MAX];
                    get_device_id(device_id);
                    sprintf(tmp, "%s/%s/", TWRP_BACKUP_PATH, device_id);
                    delete_custom_backups(tmp);
                }
                break;
        }
    }

    twrp_backup_mode = 0;
}

static void regenerate_md5_sum_menu() {
    if (!confirm_selection("This is not recommended!!", "Yes - Recreate New md5 Sum"))
        return;

    static char* headers[] = {"Regenerating md5 sum", "Select a backup to regenerate", NULL};

    char* list[] = {"Select from Internal sdcard",
                    NULL,
                    NULL};

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Select from External sdcard";

    char backup_path[PATH_MAX];
    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            sprintf(backup_path, "%s", int_sd);
            break;
        case 1:
            sprintf(backup_path, "%s", ext_sd);
            break;
        default:
            return;
    }

    // select backup set and regenerate md5 sum
    strcat(backup_path, "/clockworkmod/backup/");
    if (ensure_path_mounted(backup_path) != 0)
        return;

    char* file = choose_file_menu(backup_path, "", headers);
    if (file == NULL) return;

    char tmp[PATH_MAX];
    char *backup_source;
    backup_source = dirname(file);
    sprintf(tmp, "Process %s", basename(backup_source));
    if (!confirm_selection("Regenerate md5 sum?", tmp))
        return;

    ui_print("Generating md5 sum...\n");
    // to do (optional): remove recovery.log from md5 sum, but no real need to extra code for this!
    sprintf(tmp, "rm -f '%s/nandroid.md5'; nandroid-md5.sh %s", backup_source, backup_source);
    if (0 != __system(tmp))
        ui_print("Error while generating md5 sum!\n");
    else ui_print("Done generating md5 sum.\n");
}
//-------- End TWRP Backup and Restore Options


// Custom backup and restore menu
void custom_backup_restore_menu() {
    static char* headers[] = {  "Custom Backup & Restore",
                                "",
                                NULL
    };

    static char* list[] = { "Custom Backup Job",
                    "Custom Restore Job",
                    "TWRP Backup & Restore",
                    "Regenerate md5 Sum",
                    "Clone ROM to update.zip",
                    "Misc Nandroid Settings",
                    NULL
    };

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {

            case 0:
                custom_backup_menu();
                break;
            case 1:
                custom_restore_menu("clockworkmod/backup");
                break;
            case 2:
                twrp_backup_restore_menu();
                break;
            case 3:
                regenerate_md5_sum_menu();
                break;
            case 4:
#ifdef PHILZ_TOUCH_RECOVERY
                custom_rom_menu();
#endif
                break;
            case 5:
#ifdef PHILZ_TOUCH_RECOVERY
                misc_nandroid_menu();
#endif
                break;
        }
    }
}
//-------------- End PhilZ Touch Special Backup and Restore menu and handlers


// launch aromafm.zip from default locations
static int default_aromafm(const char* aromafm_path) {
        if (ensure_path_mounted(aromafm_path) != 0)
            return -1;

        char aroma_file[PATH_MAX];
        sprintf(aroma_file, "%s/clockworkmod/aromafm/aromafm.zip", aromafm_path);
        if (access(aroma_file, F_OK) != -1) {
#ifdef PHILZ_TOUCH_RECOVERY
            force_wait = -1;
#endif
            install_zip(aroma_file);
            return 0;
        }
        return -1;
}

void run_aroma_browser() {
    //we mount volumes so that they can be accessed when in aroma file manager gui
    ensure_path_mounted("/system");
    ensure_path_mounted("/data");
    if (volume_for_path("/sdcard") != NULL)
        ensure_path_mounted("/sdcard");
    if (volume_for_path("/external_sd") != NULL)
        ensure_path_mounted("/external_sd");
    if (volume_for_path("/emmc") != NULL)
        ensure_path_mounted("/emmc");

    //look for clockworkmod/.aromafm/aromafm.zip in /external_sd, then /sdcard and finally /emmc
    int ret = -1;
    if (volume_for_path("/external_sd") != NULL)
        ret = default_aromafm("/external_sd");
    if (ret != 0 && volume_for_path("/sdcard") != NULL)
        ret = default_aromafm("/sdcard");
    if (ret != 0 && volume_for_path("/emmc") != NULL)
        ret = default_aromafm("/emmc");
    if (ret != 0)
        ui_print("No clockworkmod/aromafm/aromafm.zip on sdcards\n");

    // unmount system and data
    ensure_path_unmounted("/system");
    ensure_path_unmounted("/data");
}
//------ end aromafm launcher functions

// start show PhilZ Touch Settings Menu
void show_philz_settings()
{
    static char* headers[] = {  "PhilZ Settings",
                                NULL
    };

    static char* list[] = { "Open Recovery Script",
                            "Custom Backup and Restore",
                            "Aroma File Manager",
                            "GUI Preferences",
                            "Save and Restore Settings",
                            "Reset All Recovery Settings",
                            "About",
                             NULL
    };

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                {
                    //search in default ors path
                    choose_default_ors_menu("/sdcard");
                    if (browse_for_file == 0) {
                        //we found .ors scripts in /sdcard default location
                        break;
                    }

                    char *other_sd = NULL;
                    if (volume_for_path("/emmc") != NULL) {
                        other_sd = "/emmc";
                    } else if (volume_for_path("/external_sd") != NULL) {
                        other_sd = "/external_sd";
                    }
                    if (other_sd != NULL) {
                        choose_default_ors_menu(other_sd);
                        //we search for .ors files in second sd under default location
                        if (browse_for_file == 0) {
                            //.ors files found
                            break;
                        }
                    }
                    //no files found in default locations, let's search manually for a custom ors
                    ui_print("No .ors files under clockworkmod/ors in sdcards\n");
                    ui_print("Manually search .ors file...\n");
                    show_custom_ors_menu();
                }
                break;
            case 1:
                is_custom_backup = 1;
                custom_backup_restore_menu();
                is_custom_backup = 0;
                break;
            case 2:
                run_aroma_browser();
                break;
            case 3:
#ifdef PHILZ_TOUCH_RECOVERY
                show_touch_gui_menu();
#endif
                break;
            case 4:
#ifdef PHILZ_TOUCH_RECOVERY
                import_export_settings();
#endif
                break;
            case 5:
#ifdef PHILZ_TOUCH_RECOVERY
                if (confirm_selection("Reset all recovery settings?", "Yes - Reset to Defaults")) {
                    delete_a_file(PHILZ_SETTINGS_FILE);
                    refresh_philz_settings();
                    ui_print("All settings reset to default!\n");
                }
#endif
                break;
            case 6:
                ui_print(EXPAND(RECOVERY_MOD_VERSION) "\n");
                ui_print("Build version: " EXPAND(PHILZ_BUILD) " - " EXPAND(TARGET_NAME) "\n");
                ui_print("CWM Base version: " EXPAND(CWM_BASE_VERSION) "\n");
                //ui_print(EXPAND(BUILD_DATE)"\n");
                ui_print("Compiled %s at %s\n", __DATE__, __TIME__);
                break;
        }
    }
}

/***************************************************/
/*      End PhilZ Menu settings and functions      */
/***************************************************/

void write_fstab_root(char *path, FILE *file)
{
    Volume *vol = volume_for_path(path);
    if (vol == NULL) {
        LOGW("Unable to get recovery.fstab info for %s during fstab generation!\n", path);
        return;
    }

    char device[200];
    if (vol->device[0] != '/')
        get_partition_device(vol->device, device);
    else
        strcpy(device, vol->device);

    fprintf(file, "%s ", device);
    fprintf(file, "%s ", path);
    // special case rfs cause auto will mount it as vfat on samsung.
    fprintf(file, "%s rw\n", vol->fs_type2 != NULL && strcmp(vol->fs_type, "rfs") != 0 ? "auto" : vol->fs_type);
}

void create_fstab()
{
    struct stat info;
    __system("touch /etc/mtab");
    FILE *file = fopen("/etc/fstab", "w");
    if (file == NULL) {
        LOGW("Unable to create /etc/fstab!\n");
        return;
    }
    Volume *vol = volume_for_path("/boot");
    if (NULL != vol && strcmp(vol->fs_type, "mtd") != 0 && strcmp(vol->fs_type, "emmc") != 0 && strcmp(vol->fs_type, "bml") != 0)
         write_fstab_root("/boot", file);
    write_fstab_root("/cache", file);
    write_fstab_root("/data", file);
    write_fstab_root("/datadata", file);
    write_fstab_root("/emmc", file);
    write_fstab_root("/system", file);
    write_fstab_root("/sdcard", file);
    write_fstab_root("/sd-ext", file);
    write_fstab_root("/external_sd", file);
    fclose(file);
    LOGI("Completed outputting fstab.\n");
}

int bml_check_volume(const char *path) {
    ui_print("Checking %s...\n", path);
    ensure_path_unmounted(path);
    if (0 == ensure_path_mounted(path)) {
        ensure_path_unmounted(path);
        return 0;
    }
    
    Volume *vol = volume_for_path(path);
    if (vol == NULL) {
        LOGE("Unable process volume! Skipping...\n");
        return 0;
    }
    
    ui_print("%s may be rfs. Checking...\n", path);
    char tmp[PATH_MAX];
    sprintf(tmp, "mount -t rfs %s %s", vol->device, path);
    int ret = __system(tmp);
    printf("%d\n", ret);
    return ret == 0 ? 1 : 0;
}

void process_volumes() {
    create_fstab();

    if (is_data_media()) {
		int count = 5;
		while (count > 0 && ensure_path_mounted("/data")) {
			usleep(500000);
			count--;
		}
        if (count == 0) // internal storage will be linked to /data/media
            LOGE("could not mount /data to setup /data/media path!\n");

        setup_data_media();

        if (count != 0) {
            count = 5;
            while (count > 0 && umount("/data")) {
                usleep(500000);
                count--;
            }
            if (count == 0)
                LOGE("could not unmount /data after /data/media setup");
        }
    }

    return;

    // dead code.
    if (device_flash_type() != BML)
        return;

    ui_print("Checking for ext4 partitions...\n");
    int ret = 0;
    ret = bml_check_volume("/system");
    ret |= bml_check_volume("/data");
    if (has_datadata())
        ret |= bml_check_volume("/datadata");
    ret |= bml_check_volume("/cache");
    
    if (ret == 0) {
        ui_print("Done!\n");
        return;
    }
    
    char backup_path[PATH_MAX];
    time_t t = time(NULL);
    char backup_name[PATH_MAX];
    struct timeval tp;
    gettimeofday(&tp, NULL);
    sprintf(backup_name, "before-ext4-convert-%d", tp.tv_sec);
    sprintf(backup_path, "/sdcard/clockworkmod/backup/%s", backup_name);

    ui_set_show_text(1);
    ui_print("Filesystems need to be converted to ext4.\n");
    ui_print("A backup and restore will now take place.\n");
    ui_print("If anything goes wrong, your backup will be\n");
    ui_print("named %s. Try restoring it\n", backup_name);
    ui_print("in case of error.\n");

    nandroid_backup(backup_path);
    nandroid_restore(backup_path, 1, 1, 1, 1, 1, 0);
    ui_set_show_text(0);
}

void handle_failure(int ret)
{
    if (ret == 0)
        return;
    if (0 != ensure_path_mounted("/sdcard"))
        return;
    mkdir("/sdcard/clockworkmod", S_IRWXU | S_IRWXG | S_IRWXO);
    ui_print("Log copied to /sdcard/clockworkmod/philz_recovery.log\n");
    ui_print("Send philz_recovery.log to Phil3759 @xda\n");
    __system("cp /tmp/recovery.log /sdcard/clockworkmod/philz_recovery.log");
}

int is_path_mounted(const char* path) {
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        return 0;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted.
        return 1;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return 0;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv) {
        // volume is already mounted
        return 1;
    }
    return 0;
}

int has_datadata() {
    Volume *vol = volume_for_path("/datadata");
    return vol != NULL;
}

int volume_main(int argc, char **argv) {
    load_volume_table();
    return 0;
}

int verify_root_and_recovery() {
    if (ensure_path_mounted("/system") != 0)
        return 0;

    int ret = 0;
    struct stat st;
    if (0 == lstat("/system/etc/install-recovery.sh", &st)) {
        if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            ui_show_text(1);
            ret = 1;
            if (confirm_selection("ROM may flash stock recovery on boot. Fix?", "Yes - Disable recovery flash")) {
                __system("chmod -x /system/etc/install-recovery.sh");
            }
        }
    }

    int exists = 0;
    if (0 == lstat("/system/bin/su", &st)) {
        exists = 1;
        if (S_ISREG(st.st_mode)) {
            if ((st.st_mode & (S_ISUID | S_ISGID)) != (S_ISUID | S_ISGID)) {
                ui_show_text(1);
                ret = 1;
                if (confirm_selection("Root access possibly lost. Fix?", "Yes - Fix root (/system/bin/su)")) {
                    __system("chmod 6755 /system/bin/su");
                }
            }
        }
    }

    if (0 == lstat("/system/xbin/su", &st)) {
        exists = 1;
        if (S_ISREG(st.st_mode)) {
            if ((st.st_mode & (S_ISUID | S_ISGID)) != (S_ISUID | S_ISGID)) {
                ui_show_text(1);
                ret = 1;
                if (confirm_selection("Root access possibly lost. Fix?", "Yes - Fix root (/system/xbin/su)")) {
                    __system("chmod 6755 /system/xbin/su");
                }
            }
        }
    }

    if (!exists) {
        ui_show_text(1);
        ret = 1;
        if (confirm_selection("Root access is missing. Root device?", "Yes - Root device (/system/xbin/su)")) {
            __system("cp /sbin/su.recovery /system/xbin/su");
            __system("chmod 6755 /system/xbin/su");
            __system("ln -sf /system/xbin/su /system/bin/su");
        }
    }

    ensure_path_unmounted("/system");
    return ret;
}
