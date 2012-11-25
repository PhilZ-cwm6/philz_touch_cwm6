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
#include "recovery.h" //for ors functions
#include "device_config.h" //for ums lun files if needed, or more to define...

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
    fprintf(file, "%s", string);
    fclose(file);
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
#define ITEM_APPLY_SDCARD     1
#define ITEM_APPLY_SIDELOAD   2
#define ITEM_SIG_CHECK        3
#define ITEM_CHOOSE_ZIP_INT   4

void show_install_update_menu()
{
    static char* headers[] = {  "Apply update from .zip", //we truncate part of header to not overwrite battery % later
                                "",
                                NULL
    };
    
    char* install_menu_items[] = {  "Choose zip from sdcard",
                                    "Apply /sdcard/update.zip",
                                    "Install zip from sideload",
                                    "Toggle Signature Verification",
                                    NULL,
                                    NULL };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc/";
        install_menu_items[4] = "Choose zip from Internal sdcard";
    }
    else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd/";
        install_menu_items[4] = "Choose zip from External sdcard";
    }
    
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, install_menu_items, 0, 0);
        switch (chosen_item)
        {
            case ITEM_SIG_CHECK:
                toggle_signature_check();
                break;
            case ITEM_APPLY_SDCARD:
            {
                if (confirm_selection("Confirm install?", "Yes - Install /sdcard/update.zip"))
                    install_zip(SDCARD_UPDATE_FILE);
                break;
            }
            case ITEM_CHOOSE_ZIP:
                show_choose_zip_menu("/sdcard/");
                break;
            case ITEM_APPLY_SIDELOAD:
                if (confirm_selection("Confirm ?", "Yes - Apply Sideload")) {
                    apply_from_adb();
                }
                break;
            case ITEM_CHOOSE_ZIP_INT:
                if (other_sd != NULL)
                    show_choose_zip_menu(other_sd);
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
        ui_print("Couldn't open directory.\n");
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
                stat(fullFileName, &info);
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
        LOGE("Failed to close directory.");
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
static int no_files_found = 1; //choose_file_menu returns string NULL when no file is found or if we choose no file in selection
                               //no_files_found = 1 when no valid file was found, no_files_found = 0 when we found a valid file
                               //added for custom ors menu support + later kernel restore
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
    if (ensure_path_mounted(mount_point) != 0) {
        LOGE ("Can't mount %s\n", mount_point);
        return;
    }

    static char* headers[] = {  "Choose a zip to apply",
                                 //let's spare some header space
                                //"",
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
                                "Leaving this menu unmount",
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

    char* confirm_headers[]  = {  title, "  THIS CAN NOT BE UNDONE.", NULL }; //let's spare some header space in Yes/No menu
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
        reset_ext4fs_info();
        int result = make_ext4fs(device, length, v->mount_point, sehandle);
        if (result != 0) {
            LOGE("format_volume: make_extf4fs failed on %s\n", device);
            return -1;
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
            sprintf(&mount_menu[mountable_volumes].mount, "mount %s", v->mount_point);
            sprintf(&mount_menu[mountable_volumes].unmount, "unmount %s", v->mount_point);
            mount_menu[mountable_volumes].v = &device_volumes[i];
            ++mountable_volumes;
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

        if (!is_data_media()) {
          options[mountable_volumes + formatable_volumes] = "mount USB storage";
          options[mountable_volumes + formatable_volumes + 1] = NULL;
        }
        else {
          options[mountable_volumes + formatable_volumes] = "format /data and /data/media (/sdcard)";
          options[mountable_volumes + formatable_volumes + 1] = NULL;
        }

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
                                "give you more options.", //fix for original phrasing
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
                            "Restore system",
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
            if (confirm_selection(confirm_restore, "Yes - Restore system"))
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

static void choose_backup_format() {
    static char* headers[] = {  "Backup Format",
                                "",
                                NULL
    };

    char* list[] = { "dup (default)",
                     "tar",
                     NULL //we fix original menu echoing older menu (null termination)
    };

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item) {
        case 0:
            write_string_to_file(NANDROID_BACKUP_FORMAT_FILE, "dup");
            ui_print("Backup format set to dedupe.\n");
            break;
        case 1:
            write_string_to_file(NANDROID_BACKUP_FORMAT_FILE, "tar");
            ui_print("Backup format set to tar.\n");
            break;
    }
}

void show_nandroid_menu()
{
    static char* headers[] = {  "Backup and Restore",
                                NULL
    };

    char* list[] = { "Backup",
                            "Restore",
                            "Delete",
                            "Advanced Restore",
                            "Free Unused Backup Data",
                            "Choose Backup Format",
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
        list[6] = "Backup to Internal sdcard";
        list[7] = "Restore from Internal sdcard";
        list[8] = "Advanced Restore from Internal sdcard";
        list[9] = "Delete from Internal sdcard";
    }
    else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd";
        list[6] = "Backup to External sdcard";
        list[7] = "Restore from External sdcard";
        list[8] = "Advanced Restore from External sdcard";
        list[9] = "Delete from External sdcard";
    }
#ifdef RECOVERY_EXTEND_NANDROID_MENU
    extend_nandroid_menu(list, 10, sizeof(list) / sizeof(char*));
#endif

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                {
                    char backup_path[PATH_MAX];
                    time_t t = time(NULL);
                    struct tm *tmp = localtime(&t);
                    if (tmp == NULL)
                    {
                        struct timeval tp;
                        gettimeofday(&tp, NULL);
                        sprintf(backup_path, "/sdcard/clockworkmod/backup/%d", tp.tv_sec);
                    }
                    else
                    {
                        strftime(backup_path, sizeof(backup_path), "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
                    }
                    nandroid_backup(backup_path);
                }
                break;
            case 1:
                show_nandroid_restore_menu("/sdcard");
                break;
            case 2:
                show_nandroid_delete_menu("/sdcard");
                break;
            case 3:
                show_nandroid_advanced_restore_menu("/sdcard");
                break;
            case 4:
                run_dedupe_gc(other_sd);
                break;
            case 5:
                choose_backup_format();
                break;
            case 6:
                {
                    char backup_path[PATH_MAX];
                    time_t t = time(NULL);
                    struct tm *timeptr = localtime(&t);
                    if (timeptr == NULL)
                    {
                        struct timeval tp;
                        gettimeofday(&tp, NULL);
                        if (other_sd != NULL) {
                            sprintf(backup_path, "%s/clockworkmod/backup/%d", other_sd, tp.tv_sec);
                        }
                        else {
                            break;
                        }
                    }
                    else
                    {
                        if (other_sd != NULL) {
                            char tmp[PATH_MAX];
                            strftime(tmp, sizeof(tmp), "clockworkmod/backup/%F.%H.%M.%S", timeptr);
                            // this sprintf results in:
                            // /emmc/clockworkmod/backup/%F.%H.%M.%S (time values are populated too)
                            sprintf(backup_path, "%s/%s", other_sd, tmp);
                        }
                        else {
                            break;
                        }
                    }
                    nandroid_backup(backup_path);
                }
                break;
            case 7:
                if (other_sd != NULL) {
                    show_nandroid_restore_menu(other_sd);
                }
                break;
            case 8:
                if (other_sd != NULL) {
                    show_nandroid_advanced_restore_menu(other_sd);
                }
                break;
            case 9:
                if (other_sd != NULL) {
                    show_nandroid_delete_menu(other_sd);
                }
                break;
            default:
#ifdef RECOVERY_EXTEND_NANDROID_MENU
                handle_nandroid_menu(10, chosen_item);
#endif
                break;
        }
    }
}

void wipe_battery_stats()
{
    ensure_path_mounted("/data");
    remove("/data/system/batterystats.bin");
    ensure_path_unmounted("/data");
    ui_print("Battery Stats wiped.\n");
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

#ifdef SHOW_LOG_MENU
//we create a new show log menu easier to read for small res phones
void show_log_menu()
{
    static char* headers[] = {  "Show Log",
                                NULL
    };

    static char* list[] = { "Show Last 36 Lines",
                             NULL
    };

    for (;;) {
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK) {
            break;
        }
        switch (chosen_item)
        {
            case 0:
                ui_printlogtail(36);
                break;
        }
    }
}
#endif

void show_advanced_menu()
{
    static char* headers[] = {  "Advanced Menu",
                                NULL
    };

    static char* list[] = { "Reboot Recovery",
                            "Reboot Download",
                            "Wipe Dalvik Cache",
                            "Wipe Battery Stats",
                            "Report Error",
                            "Key Test",
                            "Show log",
                            "Fix Permissions",
                            "Partition sdcard",
                            "Partition External sdcard",
                            "Partition Internal sdcard",
                            NULL
    };

    if (!can_partition("/sdcard")) {
        list[8] = NULL;
    }
    if (!can_partition("/external_sd")) {
        list[9] = NULL;
    }
    if (!can_partition("/emmc")) {
        list[10] = NULL;
    }

    for (;;)
    {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
                break;
            case 1:
                android_reboot(ANDROID_RB_RESTART2, 0, "download");
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
                if (confirm_selection( "Confirm wipe?", "Yes - Wipe Battery Stats"))
                    wipe_battery_stats();
                break;
            case 4:
                handle_failure(1);
                break;
            case 5:
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
            case 6:
#ifdef SHOW_LOG_MENU
                show_log_menu(); //we use dedicated showlog menu
#else
                ui_printlogtail(12);
#endif
                break;
            case 7:
                ensure_path_mounted("/system");
                ensure_path_mounted("/data");
                //I hate you fixing permissions when I'm dizzy: let's confirm my touch
                if (confirm_selection("Confirm ?", "Yes - Fix Permissions")) {
                    ui_print("Fixing permissions...\n");
                    __system("fix_permissions");
                    ui_print("Done!\n");
                }
                break;
            case 8:
                partition_sdcard("/sdcard");
                break;
            case 9:
                partition_sdcard("/external_sd");
                break;
            case 10:
                partition_sdcard("/emmc");
                break;
        }
    }
}

//start PhilZ Menu settings and functions
static int browse_for_file = 1;
           // 0 == stop browsing default file locations
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
    }
}
  // ** start open recovery script support ** //
  // ** adapted code from sk8erwitskil ** //
#define SCRIPT_COMMAND_SIZE 512
 //check ors script at boot (called from recovery.c)
int check_for_script_file(const char* ors_boot_script)
{
    ensure_path_mounted("/sdcard");
    if (volume_for_path("/external_sd") != NULL) {
        ensure_path_mounted("/external_sd");
    } else if (volume_for_path("/emmc") != NULL) {
        ensure_path_mounted("/emmc");
    }

    int ret_val = -1;
    char exec[512];
    FILE *fp = fopen(ors_boot_script, "r");
    if (fp != NULL) {
        ret_val = 0;
        LOGI("Script file found: '%s'\n", ors_boot_script);
        fclose(fp);
        __system("ors-mount.sh");
        // Copy script file to /tmp
        strcpy(exec, "cp ");
        strcat(exec, ors_boot_script);
        strcat(exec, " ");
        strcat(exec, "/tmp/openrecoveryscript");
        __system(exec);
        // Delete the file from /cache
        strcpy(exec, "rm ");
        strcat(exec, ors_boot_script);
        // __system(exec);
    }
    return ret_val;
}
  //run ors script code
  //this can start on boot or manually for custom ors
int run_ors_script(const char* ors_script) {
    FILE *fp = fopen(ors_script, "r");
    int ret_val = 0, cindex, line_len, i, remove_nl;
    char script_line[SCRIPT_COMMAND_SIZE], command[SCRIPT_COMMAND_SIZE],
         value[SCRIPT_COMMAND_SIZE], mount[SCRIPT_COMMAND_SIZE],
         value1[SCRIPT_COMMAND_SIZE], value2[SCRIPT_COMMAND_SIZE];
    char *val_start, *tok;
    int ors_system = 0;
    int ors_data = 0;
    int ors_cache = 0;
    int ors_recovery = 0;
    int ors_boot = 0;
    int ors_andsec = 0;
    int ors_sdext = 0;

    if (fp != NULL) {
        while (fgets(script_line, SCRIPT_COMMAND_SIZE, fp) != NULL && ret_val == 0) {
            cindex = 0;
            line_len = strlen(script_line);
            //if (line_len > 2)
                //continue; // there's a blank line at the end of the file, we're done!
            ui_print("script line: '%s'\n", script_line);
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
                ui_print("command is: '%s' and ", command);
                val_start = script_line;
                val_start += cindex + 1;
                strncpy(value, val_start, line_len - cindex - remove_nl);
                ui_print("value is: '%s'\n", value);
            } else {
                strncpy(command, script_line, line_len - remove_nl + 1);
                ui_print("command is: '%s' and there is no value\n", command);
            }
            if (strcmp(command, "install") == 0) {
                // Install zip
                ui_print("Installing zip file '%s'\n", value);
                ensure_path_mounted("/sdcard");
                char *other_sd = NULL;
                if (volume_for_path("/external_sd") != NULL) {
                    other_sd = "/external_sd";
                } else if (volume_for_path("/emmc") != NULL) {
                    other_sd = "/emmc";
                }
                if (other_sd != NULL){
                    ensure_path_mounted(other_sd);
                }
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
                // Backup: always use external sd if possible
                char *other_sd = NULL;
                if (volume_for_path("/external_sd") != NULL) {
                    other_sd = "/external_sd";
                } else if (volume_for_path("/sdcard") != NULL) {
                    other_sd = "/sdcard";
                } else {
                    //backup to internal sd support
                    other_sd = "/emmc";
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
                            //backup to internal sd support
                            strftime(backup_path, sizeof(backup_path), "/emmc/clockworkmod/backup/%F.%H.%M.%S", tmp);
                        }
                    }
                }
                ui_print("Backup options are ignored in CWMR: '%s'\n", value1);
                if (0 != nandroid_backup(backup_path))
                ui_print("Backup failed !!\n");
            } else if (strcmp(command, "restore") == 0) {
                // Restore
                tok = strtok(value, " ");
                strcpy(value1, tok);
                ui_print("Restoring '%s'\n", value1);
                tok = strtok(NULL, " ");
                if (tok != NULL) {
                    ors_system = 0;
                    ors_data = 0;
                    ors_cache = 0;
                    ors_boot = 0;
                    ors_sdext = 0;
                    memset(value2, 0, sizeof(value2));
                    strcpy(value2, tok);
                    ui_print("Setting restore options:\n");
                    line_len = strlen(value2);
                    for (i=0; i<line_len; i++) {
                        if (value2[i] == 'S' || value2[i] == 's') {
                            ors_system = 1;
                            ui_print("System\n");
                        } else if (value2[i] == 'D' || value2[i] == 'd') {
                            ors_data = 1;
                            ui_print("Data\n");
                        } else if (value2[i] == 'C' || value2[i] == 'c') {
                            ors_cache = 1;
                            ui_print("Cache\n");
                        } else if (value2[i] == 'R' || value2[i] == 'r') {
                            ui_print("Option for recovery ignored in CWMR\n");
                        } else if (value2[i] == '1') {
                            ui_print("%s\n", "Option for special1 ignored in CWMR");
                        } else if (value2[i] == '2') {
                            ui_print("%s\n", "Option for special1 ignored in CWMR");
                        } else if (value2[i] == '3') {
                            ui_print("%s\n", "Option for special1 ignored in CWMR");
                        } else if (value2[i] == 'B' || value2[i] == 'b') {
                            ors_boot = 1;
                            ui_print("Boot\n");
                        } else if (value2[i] == 'A' || value2[i] == 'a') {
                            ui_print("Option for android secure ignored in CWMR\n");
                        } else if (value2[i] == 'E' || value2[i] == 'e') {
                            ors_sdext = 1;
                            ui_print("SD-Ext\n");
                        } else if (value2[i] == 'M' || value2[i] == 'm') {
                            ui_print("MD5 check skip option ignored in CWMR\n");
                        }
                    }
                } else
                    LOGI("No restore options set\n");
                nandroid_restore(value1, ors_boot, ors_system, ors_data, ors_cache, ors_sdext, 0);
                ui_print("Restore complete!\n");
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
void choose_default_ors_menu(const char* ors_path)
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
void choose_custom_ors_menu(const char* ors_path)
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
void show_custom_ors_menu() {
    static char* headers[] = {  "Search .ors script to run",
                                "",
                                NULL
    };

    static char* list[] = { "Search sdcard",
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
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
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
  // ** end open recovery script support ** //
  //start show flash kernel menu (flash/restore from default location)
void flash_kernel_default (const char* kernel_path) {
    static char* headers[] = {  "Flash kernel image",
                                NULL
    };

    if (ensure_path_mounted(kernel_path) != 0) {
        LOGE ("Can't mount %s\n", kernel_path);
        return;
    }

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/clockworkmod/.kernel_bak/", kernel_path);
    //without this check, we get 2 errors in log: "directory not found":
    if (access(tmp, F_OK) != -1) {
        //folder exists, but could be empty!
        char* kernel_file = choose_file_menu(tmp, ".img", headers);
        if (kernel_file == NULL) {
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found == 1) {
                //0 valid files to select
                ui_print("No *.img files in %s\n", tmp);
            }
            return;
        }
        static char* confirm_install = "Confirm flash kernel?";
        static char confirm[PATH_MAX];
        sprintf(confirm, "Yes - Flash %s", basename(kernel_file));
        if (confirm_selection(confirm_install, confirm)) {
            char tmp[PATH_MAX];
            sprintf(tmp, "kernel-restore.sh %s %s", kernel_file, kernel_path);
            __system(tmp);
            //prints log
            char logname[PATH_MAX];
            sprintf(logname, "%s/clockworkmod/.kernel_bak/log.txt", kernel_path);
            ui_print_custom_logtail(logname, 3);
        }
    } else {
        ui_print("%s not found.\n", tmp);
        return;
    }
}
  //start flash modem menu
void flash_modem_menu(const char* modem_path)
{
    if (ensure_path_mounted(modem_path) != 0) {
        LOGE ("Can't mount %s\n", modem_path);
        browse_for_file = 1;
        return;
    }
    
    char modem_dir[PATH_MAX];
    sprintf(modem_dir, "%s/clockworkmod/.modem_bak/", modem_path);
    if (access(modem_dir, F_OK) == -1) {
        //custom folder does not exist
        browse_for_file = 1;
        return;
    }

    static char* headers[] = {  "Flash modem image",
                                "",
                                NULL
    };

    char* modem_file = choose_file_menu(modem_dir, ".bin", headers);
    if (no_files_found == 1) {
        //0 valid files to select, let's continue browsing next locations
        ui_print("No *.bin files in %s\n", modem_dir);
        browse_for_file = 1;
    } else {
        browse_for_file = 0;
        //we found modem image in clockworkmod/.modem_bak folder: do not proceed other locations even if no file is chosen
    }
    if (modem_file == NULL) {
        //either no valid files found or we selected no files by pressing back menu
        return;
    }
    static char* confirm_install  = "Confirm flash modem?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Flash %s", basename(modem_file));
    if (confirm_selection(confirm_install, confirm)) {
        char tmp[PATH_MAX];
        sprintf(tmp, "modem-flash.sh %s %s", modem_file, modem_path);
        __system(tmp);
        //prints log
        char logname[PATH_MAX];
        sprintf(logname, "%s/clockworkmod/.modem_bak/log.txt", modem_path);
        ui_print_custom_logtail(logname, 3);
    }
}

#ifdef PHILZ_TOUCH_RECOVERY
#include "/root/Desktop/PhilZ_Touch/touch_source/philz_gui_settings.c"
#endif

  //start show partition backup/restore menu
void show_efs_menu() {
    static char* headers[] = {  "Special Backup & Restore",
                                "",
                                NULL
    };

    static char* list[] = { "Backup Kernel to sdcard",
                     "Flash Kernel from sdcard",
                     "Backup EFS to sdcard",
                     "Restore EFS from sdcard",
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     "Flash Modem",
                     NULL
    };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc";
        list[4] = "Backup Kernel to Internal sdcard";
        list[5] = "Flash Kernel from Internal sdcard";
        list[6] = "Backup EFS to Internal sdcard";
        list[7] = "Restore EFS from Internal sdcard";
    } else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd";
        list[4] = "Backup Kernel to External sdcard";
        list[5] = "Flash Kernel from External sdcard";
        list[6] = "Backup EFS to External sdcard";
        list[7] = "Restore EFS from External sdcard";
    }

    for (;;) {
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                __system("kernel-backup.sh /sdcard");
                ui_print_custom_logtail("/sdcard/clockworkmod/.kernel_bak/log.txt", 3);
                break;
            case 1:
                flash_kernel_default("/sdcard");
                break;
            case 2:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                ensure_path_unmounted("/efs");
                __system("efs-backup.sh /sdcard");
                ui_print_custom_logtail("/sdcard/clockworkmod/.efsbackup/log.txt", 3);
                break;
            case 3:
                if (ensure_path_mounted("/sdcard") != 0) {
                    ui_print("Can't mount /sdcard\n");
                    break;
                }
                ensure_path_unmounted("/efs");
                if (access("/sdcard/clockworkmod/.efsbackup/efs.img", F_OK ) != -1) {
                    if (confirm_selection("Confirm?", "Yes - Restore EFS")) {
                        __system("efs-restore.sh /sdcard");
                        ui_print_custom_logtail("/sdcard/clockworkmod/.efsbackup/log.txt", 3);
                    }
                } else {
                    ui_print("No efs.img backup found in sdcard.\n");
                }
                break;
            case 4:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    char tmp[PATH_MAX];
                    sprintf(tmp, "kernel-backup.sh %s", other_sd);
                    __system(tmp);
                    //prints log
                    char logname[PATH_MAX];
                    sprintf(logname, "%s/clockworkmod/.kernel_bak/log.txt", other_sd);
                    ui_print_custom_logtail(logname, 3);
                }
                break;
            case 5:
                flash_kernel_default(other_sd);
                break;
            case 6:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    ensure_path_unmounted("/efs");
                    char tmp[PATH_MAX];
                    sprintf(tmp, "efs-backup.sh %s", other_sd);
                    __system(tmp);
                    //prints log
                    char logname[PATH_MAX];
                    sprintf(logname, "%s/clockworkmod/.efsbackup/log.txt", other_sd);
                    ui_print_custom_logtail(logname, 3);
                }
                break;
            case 7:
                {
                    if (ensure_path_mounted(other_sd) != 0) {
                        ui_print("Can't mount %s\n", other_sd);
                        break;
                    }
                    ensure_path_unmounted("/efs");
                    char filename[PATH_MAX];
                    sprintf(filename, "%s/clockworkmod/.efsbackup/efs.img", other_sd);
                    if (access(filename, F_OK ) != -1) {
                        if (confirm_selection("Confirm?", "Yes - Restore EFS")) {
                            char tmp[PATH_MAX];
                            sprintf(tmp, "efs-restore.sh %s", other_sd);
                            __system(tmp);
                            //prints log
                            char logname[PATH_MAX];
                            sprintf(logname, "%s/clockworkmod/.efsbackup/log.txt", other_sd);
                            ui_print_custom_logtail(logname, 3);
                        }
                    } else {
                        ui_print("No efs.img backup found in %s\n", other_sd);
                    }
                }
                break;
            case 8:
                {
                    flash_modem_menu("/sdcard");
                    if (browse_for_file == 0) {
                        //we found modem files in /sdcard default location
                        break;
                    }

                    char *other_sd = NULL;
                    if (volume_for_path("/emmc") != NULL) {
                        other_sd = "/emmc";
                    } else if (volume_for_path("/external_sd") != NULL) {
                        other_sd = "/external_sd";
                    }
                    if (other_sd != NULL) {
                        flash_modem_menu(other_sd);
                        //we search for modem files in second sd under default location
                        if (browse_for_file == 0) {
                            break;
                        }
                    }
                    ui_print("Nothing to flash.\n Put *.bin file under clockworkmod/.modem_bak\n");
                }
                break;
        }
    }
}
  //browse and select Aroma File Manager from custom locations on sdcards
void choose_aromafm_menu(const char* aromafm_path)
{
    if (ensure_path_mounted(aromafm_path) != 0) {
        LOGE("Can't mount %s\n", aromafm_path);
        return;
    }

    static char* headers[] = {  "Browse for aromafm.zip",
                                NULL
    };

    char* aroma_file = choose_file_menu(aromafm_path, "aromafm.zip", headers);
    if (aroma_file == NULL)
        return;
    static char* confirm_install  = "Confirm Run Aroma?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Run %s", basename(aroma_file));
    if (confirm_selection(confirm_install, confirm)) {
        install_zip(aroma_file);
    }
}
  //Show custom aroma menu: manually browse sdcards for Aroma file manager
void custom_aroma_menu() {
    static char* headers[] = {  "Browse for aromafm.zip",
                                "",
                                NULL
    };

    static char* list[] = { "Search sdcard",
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
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                choose_aromafm_menu("/sdcard/");
                break;
            case 1:
                choose_aromafm_menu(other_sd);
                break;
        }
    }
}
  //launch aromafm.zip from default locations
static int default_aromafm (const char* aromafm_path) {
        if (ensure_path_mounted(aromafm_path) != 0) {
            //no sdcard at moint point
            return 0;
        }
        char aroma_file[PATH_MAX];
        sprintf(aroma_file, "%s/clockworkmod/.aromafm/aromafm.zip", aromafm_path);
        if (access(aroma_file, F_OK) != -1) {
            install_zip(aroma_file);
            return 1;
        }
        return 0;
}
  //start show PhilZ Settings Menu
void show_philz_settings()
{
    static char* headers[] = {  "PhilZ Settings",
                                NULL
    };

    static char* list[] = { "Open Recovery Script",
                            "Special Backup and Restore",
                            "Aroma File Manager",
                            "GUI Preferences",
                            "About",
                             NULL
    };

    for (;;) {
        //header function so that "Toggle menu" doesn't reset to main menu on action selected
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                {
                    __system("ors-mount.sh");
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
                show_efs_menu();
                break;
            case 2:
                //we mount sdcards so that they can be accessed when in aroma file manager gui
                if (volume_for_path("/sdcard") != NULL) {
                    ensure_path_mounted("/sdcard");
                }
                if (volume_for_path("/external_sd") != NULL) {
                    ensure_path_mounted("/external_sd");
                } else if (volume_for_path("/emmc") != NULL) {
                    ensure_path_mounted("/emmc");
                }
                //look for clockworkmod/.aromafm/aromafm.zip in /external_sd, then /sdcard and finally /emmc
                if (volume_for_path("/external_sd") != NULL) {
                    if (default_aromafm("/external_sd")) {
                        break;
                    }
                }
                if (volume_for_path("/sdcard") != NULL) {
                    if (default_aromafm("/sdcard")) {
                        break;
                    }
                }
                if (volume_for_path("/emmc") != NULL) {
                    if (default_aromafm("/emmc")) {
                        break;
                    }
                }
                ui_print("No clockworkmod/.aromafm/aromafm.zip on sdcards\n");
                ui_print("Browsing custom locations\n");
                custom_aroma_menu();
                break;
            case 3:
#ifdef PHILZ_TOUCH_RECOVERY
                show_touch_gui_menu();
#endif
                break;
            case 4:
                ui_print(EXPAND(RECOVERY_VERSION)"\n");
                ui_print("Build version: "EXPAND(PHILZ_BUILD)" - "EXPAND(TARGET_DEVICE)"\n");
                ui_print("CWM Base version: "EXPAND(CWM_BASE_VERSION)"\n");
                //ui_print(EXPAND(BUILD_DATE)"\n");
                ui_print("Compiled %s at %s\n", __DATE__, __TIME__);
                break;
        }
    }
}
  //end show PhilZ Menu
//end PhilZ Menu settings and functions

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
        setup_data_media();
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
    __system("cp /tmp/recovery.log /sdcard/clockworkmod/recovery.log");
    ui_print("/tmp/recovery.log was copied to /sdcard/clockworkmod/recovery.log. Please open ROM Manager to report the issue.\n");
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

    if (0 == lstat("/system/bin/su", &st)) {
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

    ensure_path_unmounted("/system");
    return ret;
}
