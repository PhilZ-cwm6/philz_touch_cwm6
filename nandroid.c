/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*    TWRP backup and restore support    */
/*                and                    */
/*    Custom backup and restore support  */
/*    Are part of PhilZ Touch Recovery   */
/*****************************************/

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
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include <sys/vfs.h>
#include "cutils/android_reboot.h"

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"

#include "flashutils/flashutils.h"
#include <libgen.h>


static long nandroid_start_msec;
static long now_msec(void) {
    struct timeval now;
    long mseconds;
    gettimeofday(&now, NULL);
    mseconds = now.tv_sec * 1000;
    mseconds += now.tv_usec / 1000;
    return mseconds;
}

#ifdef PHILZ_TOUCH_RECOVERY
#include "/root/Desktop/PhilZ_Touch/touch_source/philz_nandroid_gui.c"
#endif

void nandroid_generate_timestamp_path(const char* backup_path)
{
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
        strftime(backup_path, PATH_MAX, "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
    }
}

void ensure_directory(const char* dir) {
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s; chmod 777 %s;", dir, dir);
    __system(tmp);
}

static int print_and_error(const char* message) {
    ui_print("%s\n", message);
    return 1;
}

static int nandroid_backup_bitfield = 0;
#define NANDROID_FIELD_DEDUPE_CLEARED_SPACE 1
static int nandroid_files_total = 0;
static int nandroid_files_count = 0;
static void nandroid_callback(const char* filename)
{
    if (filename == NULL)
        return;
    const char* justfile = basename(filename);
    char tmp[PATH_MAX];
    strcpy(tmp, justfile);
    if (tmp[strlen(tmp) - 1] == '\n')
        tmp[strlen(tmp) - 1] = NULL;
    tmp[ui_get_text_cols() - 1] = '\0';
    nandroid_files_count++;
    ui_increment_frame();

    char size_progress[256] = "";
    if (Backup_Size != 0) {
        sprintf(size_progress, "Done %llu/%lluMb - Free %lluMb",
                (Used_Size - Before_Used_Size) / 1048576LLU, Backup_Size / 1048576LLU, Free_Size / 1048576LLU);
        size_progress[ui_get_text_cols() - 1] = '\0';
    }

#ifdef PHILZ_TOUCH_RECOVERY
    int color[] = {CYAN_BLUE_CODE};
    ui_print_color(3, color);
#endif
    // do not write size progress to log file
    ui_nolog_lines(1);
    ui_nice_print("%s\n%s\n", tmp, size_progress);
    ui_nolog_lines(-1);
    if (!ui_was_niced() && nandroid_files_total != 0)
        ui_set_progress((float)nandroid_files_count / (float)nandroid_files_total);
    if (!ui_was_niced()) {
        ui_delete_line();
        ui_delete_line();
    }
#ifdef PHILZ_TOUCH_RECOVERY
    ui_print_color(0, 0);
#endif
}

static void compute_directory_stats(const char* directory)
{
    char tmp[PATH_MAX];
    sprintf(tmp, "find %s | %s wc -l > /tmp/dircount", directory, strcmp(directory, "/data") == 0 && is_data_media() ? "grep -v /data/media |" : "");
    __system(tmp);
    char count_text[100];
    FILE* f = fopen("/tmp/dircount", "r");
    fread(count_text, 1, sizeof(count_text), f);
    fclose(f);
    nandroid_files_count = 0;
    nandroid_files_total = atoi(count_text);

    if (!twrp_backup_mode) {
        ui_reset_progress();
        ui_show_progress(1, 0);
    }
}

static long last_size_update = 0;
static void update_size_progress(const char* backup_file_image) {
    // statfs every 0.5sec interval maximum
    if (last_size_update == 0 || (now_msec() - last_size_update) > 500) {
        Get_Size_Via_statfs(backup_file_image);
        last_size_update = now_msec();
    }
}


typedef void (*file_event_callback)(const char* filename);
typedef int (*nandroid_backup_handler)(const char* backup_path, const char* backup_file_image, int callback);

static int mkyaffs2image_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd %s ; mkyaffs2image . %s.img ; exit $?", backup_path, backup_file_image);

    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute mkyaffs2image.\n");
        return -1;
    }

    int nand_starts = 1;
    last_size_update = 0;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, backup_file_image, 1, &nand_starts))
            return -1;
#endif
        tmp[PATH_MAX - 1] = NULL;
        if (callback) {
            update_size_progress(backup_file_image);
            nandroid_callback(tmp);
        }
    }

    return __pclose(fp);
}

//enable or toggle it at your wish, just give credit @PhilZ
int compression_value = TAR_FORMAT;
static int tar_compress_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    if (compression_value == TAR_FORMAT)
        sprintf(tmp, "cd $(dirname %s) ; touch %s.tar ; (tar cv %s $(basename %s) | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.) 2> /proc/self/fd/1 ; exit $?", backup_path, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path, backup_file_image);
    else
        sprintf(tmp, "cd $(dirname %s) ; touch %s.tar.gz ; (tar cv %s $(basename %s) | pigz -%d | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.gz.) 2> /proc/self/fd/1 ; exit $?", backup_path, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path, compression_value, backup_file_image);
    // users expect a nandroid backup to be like a raw image, should give choice to skip data...
    //sprintf(tmp, "cd $(dirname %s) ; touch %s.tar ; (tar cv --exclude=data/data/com.google.android.music/files/* %s $(basename %s) | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.) 2> /proc/self/fd/1 ; exit $?", backup_path, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path, backup_file_image);

    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar.\n");
        return -1;
    }

    int nand_starts = 1;
    last_size_update = 0;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, backup_file_image, 1, &nand_starts))
            return -1;
#endif
        tmp[PATH_MAX - 1] = NULL;
        if (callback) {
            update_size_progress(backup_file_image);
            nandroid_callback(tmp);
        }
    }

    return __pclose(fp);
}

static int tar_dump_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s); tar cv --exclude=data/data/com.google.android.music/files/* %s $(basename %s) 2> /dev/null | cat", backup_path, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path);

    return __system(tmp);
}

void nandroid_dedupe_gc(const char* blob_dir) {
    char backup_dir[PATH_MAX];
    strcpy(backup_dir, blob_dir);
    char *d = dirname(backup_dir);
    strcpy(backup_dir, d);
    strcat(backup_dir, "/backup");
    ui_print("Freeing space...\n");
    char tmp[PATH_MAX];
    sprintf(tmp, "dedupe gc %s $(find %s -name '*.dup')", blob_dir, backup_dir);
    __system(tmp);
    ui_print("Done freeing space.\n");
}

static int dedupe_compress_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    char blob_dir[PATH_MAX];
    strcpy(blob_dir, backup_file_image);
    char *d = dirname(blob_dir);
    strcpy(blob_dir, d);
    d = dirname(blob_dir);
    strcpy(blob_dir, d);
    d = dirname(blob_dir);
    strcpy(blob_dir, d);
    strcat(blob_dir, "/blobs");
    ensure_directory(blob_dir);

    if (!(nandroid_backup_bitfield & NANDROID_FIELD_DEDUPE_CLEARED_SPACE)) {
        nandroid_backup_bitfield |= NANDROID_FIELD_DEDUPE_CLEARED_SPACE;
        nandroid_dedupe_gc(blob_dir);
    }

    sprintf(tmp, "dedupe c %s %s %s.dup %s", backup_path, blob_dir, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "./media" : "");

    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute dedupe.\n");
        return -1;
    }

    int nand_starts = 1;
    last_size_update = 0;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, backup_file_image, 1, &nand_starts))
            return -1;
#endif
        tmp[PATH_MAX - 1] = NULL;
        if (callback) {
            update_size_progress(backup_file_image);
            nandroid_callback(tmp);
        }
    }

    return __pclose(fp);
}

static nandroid_backup_handler default_backup_handler = tar_compress_wrapper;
static char forced_backup_format[5] = "";
void nandroid_force_backup_format(const char* fmt) {
    strcpy(forced_backup_format, fmt);
}
static void refresh_default_backup_handler() {
    char fmt[5];
    if (strlen(forced_backup_format) > 0) {
        strcpy(fmt, forced_backup_format);
    }
    else {
        ensure_path_mounted("/sdcard");
        FILE* f = fopen(NANDROID_BACKUP_FORMAT_FILE, "r");
        if (NULL == f) {
            default_backup_handler = tar_compress_wrapper;
            return;
        }
        fread(fmt, 1, sizeof(fmt), f);
        fclose(f);
    }
    fmt[3] = NULL;
    if (0 == strcmp(fmt, "dup"))
        default_backup_handler = dedupe_compress_wrapper;
    else
        default_backup_handler = tar_compress_wrapper;
}

unsigned nandroid_get_default_backup_format() {
    refresh_default_backup_handler();
    if (default_backup_handler == dedupe_compress_wrapper) {
        return NANDROID_BACKUP_FORMAT_DUP;
    } else {
        return NANDROID_BACKUP_FORMAT_TAR;
    }
}

static nandroid_backup_handler get_backup_handler(const char *backup_path) {
    Volume *v = volume_for_path(backup_path);
    if (v == NULL) {
        ui_print("Unable to find volume.\n");
        return NULL;
    }
    MountedVolume *mv = find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        ui_print("Unable to find mounted volume: %s\n", v->mount_point);
        return NULL;
    }

    if (strcmp(backup_path, "/data") == 0 && is_data_media()) {
        return default_backup_handler;
    }

    if (strlen(forced_backup_format) > 0)
        return default_backup_handler;

    // cwr5, we prefer dedupe for everything except yaffs2
    if (strcmp("yaffs2", mv->filesystem) == 0) {
        return mkyaffs2image_wrapper;
    }

    return default_backup_handler;
}

int nandroid_backup_partition_extended(const char* backup_path, const char* mount_point, int umount_when_finished) {
    int ret = 0;
    char name[PATH_MAX];
    strcpy(name, basename(mount_point));

    struct stat file_info;
    int callback = stat("/sdcard/clockworkmod/.hidenandroidprogress", &file_info) != 0;

    ui_print("\n>> Backing up %s...\n", mount_point);
    if (0 != (ret = ensure_path_mounted(mount_point) != 0)) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }

    compute_directory_stats(mount_point);
    char tmp[PATH_MAX];
    scan_mounted_volumes();
    Volume *v = volume_for_path(mount_point);
    MountedVolume *mv = NULL;
    if (v != NULL)
        mv = find_mounted_volume_by_mount_point(v->mount_point);

    if (twrp_backup_mode) {
        if (strstr(name, "android_secure") != NULL)
            strcpy(name, "and-sec");
        if (mv == NULL || mv->filesystem == NULL)
            sprintf(tmp, "%s/%s.auto.win", backup_path, name);
        else
            sprintf(tmp, "%s/%s.%s.win", backup_path, name, mv->filesystem);
        
        ret = twrp_backup_wrapper(mount_point, tmp, callback);
    } else {
        if (strcmp(backup_path, "-") == 0)
            sprintf(tmp, "/proc/self/fd/1");
        else if (mv == NULL || mv->filesystem == NULL)
            sprintf(tmp, "%s/%s.auto", backup_path, name);
        else
            sprintf(tmp, "%s/%s.%s", backup_path, name, mv->filesystem);
        nandroid_backup_handler backup_handler = get_backup_handler(mount_point);

        if (backup_handler == NULL) {
            ui_print("Error finding an appropriate backup handler.\n");
            return -2;
        }
        ret = backup_handler(mount_point, tmp, callback);
    }

    if (umount_when_finished) {
        ensure_path_unmounted(mount_point);
    }
    if (0 != ret) {
        ui_print("Error while making a backup image of %s!\n", mount_point);
        return ret;
    }
    ui_print("Backup of %s completed.\n", name);
    return 0;
}

int nandroid_backup_partition(const char* backup_path, const char* root) {
    Volume *vol = volume_for_path(root);
    // make sure the volume exists before attempting anything...
    if (vol == NULL || vol->fs_type == NULL) {
        ui_print("Volume not found! Skipping backup of %s...\n", root);
        return 0;
    }

    // see if we need a raw backup (mtd)
    char tmp[PATH_MAX];
    int ret;
    if (strcmp(vol->fs_type, "mtd") == 0 ||
            strcmp(vol->fs_type, "bml") == 0 ||
            strcmp(vol->fs_type, "emmc") == 0) {
        ui_print("\n>> Backing up %s...\n", root);
        const char* name = basename(root);
        if (strcmp(backup_path, "-") == 0)
            strcpy(tmp, "/proc/self/fd/1");
        else if (twrp_backup_mode)
            sprintf(tmp, "%s/%s.%s.win", backup_path, name, vol->fs_type);
        else
            sprintf(tmp, "%s/%s.img", backup_path, name);

        ui_print("Backing up %s image...\n", name);
        if (0 != (ret = backup_raw_partition(vol->fs_type, vol->device, tmp))) {
            ui_print("Error while backing up %s image!\n", name);
            return ret;
        }

        ui_print("Backup of %s image completed.\n", name);
        return 0;
    }

    return nandroid_backup_partition_extended(backup_path, root, 1);
}


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/*                                       */
/*   Custom Backup and Restore Support   */
/*       code written by PhilZ @xda      */
/*        for PhilZ Touch Recovery       */
/*****************************************/

//these general variables are needed to not break backup and restore by external script
int backup_boot = 1, backup_recovery = 1, backup_wimax = 1, backup_system = 1;
int backup_data = 1, backup_cache = 1, backup_sdext = 1;
int backup_preload = 0, backup_efs = 0, backup_misc = 0, backup_modem = 0;
int is_custom_backup = 0;
int reboot_after_nandroid = 0;
int android_secure_ext = 0;
int nandroid_add_preload = 0;
int enable_md5sum = 1;

void finish_nandroid_job() {
    ui_print("Finalizing, please wait...\n");
    sync();
#ifdef PHILZ_TOUCH_RECOVERY
    if (show_background_icon)
        ui_set_background(BACKGROUND_ICON_CLOCKWORK);
    else
#endif
        ui_set_background(BACKGROUND_ICON_NONE);

    ui_reset_progress();
}

static int Is_File_System(const char* root) {
    Volume *vol = volume_for_path(root);
    if (vol == NULL || vol->fs_type == NULL)
        return -1; // unsupported

    if (strcmp(vol->fs_type, "ext2") == 0 ||
            strcmp(vol->fs_type, "ext3") == 0 ||
            strcmp(vol->fs_type, "ext4") == 0 ||
            strcmp(vol->fs_type, "vfat") == 0 ||
            strcmp(vol->fs_type, "yaffs2") == 0 ||
            strcmp(vol->fs_type, "exfat") == 0 ||
            strcmp(vol->fs_type, "rfs") == 0 ||
            strcmp(vol->fs_type, "auto") == 0)
        return 0; // true
    else
        return 1; //false
}

static int Is_Image(const char* root) {
    Volume *vol = volume_for_path(root);
    if (vol == NULL || vol->fs_type == NULL)
        return -1; // unsupported

    if (strcmp(vol->fs_type, "emmc") == 0 || strcmp(vol->fs_type, "mtd") == 0 ||
            strcmp(vol->fs_type, "bml") == 0)
        return 0; // true
    else
        return 1; // false
}


unsigned long long Backup_Size;
unsigned long long Before_Used_Size;
static int check_backup_size() {
    int total_mb = (int)(Total_Size / 1048576LLU);
    int used_mb = (int)(Used_Size / 1048576LLU);
    int free_mb = (int)(Free_Size / 1048576LLU);
    int free_percent = free_mb * 100 / total_mb;
    Before_Used_Size = Used_Size; // save Used_Size to refresh data written stats later
    Backup_Size = 0;

    static const char* Partitions_List[] = {"/recovery",
                    "/boot",
                    "/wimax",
                    "/modem",
                    "/radio",
                    "/efs",
                    "/misc",
                    "/system",
                    "/preload",
                    "/data",
                    "/datadata",
                    "/cache",
                    "/sd-ext",
                    NULL
    };

    int preload_status = 0;
    if ((is_custom_backup && backup_preload) || (!is_custom_backup && nandroid_add_preload))
        preload_status = 1;

    int backup_status[] = {backup_recovery,
            backup_boot,
            backup_wimax,
            backup_modem,
            backup_modem,
            backup_efs,
            backup_misc,
            backup_system,
            preload_status,
            backup_data,
            backup_data,
            backup_cache,
            backup_sdext,
    };

    char skipped_parts[1024] = "";
    int ret = 0;
    Volume* vol;

    int i;
    for(i=0; Partitions_List[i] != NULL; i++) {
        if (!backup_status[i])
            continue;
        if (strcmp(Partitions_List[i], "/data") == 0 && is_data_media())
            continue;
        if (strcmp(Partitions_List[i], "/datadata") == 0 && !has_datadata())
            continue;
        
        vol = volume_for_path(Partitions_List[i]);
        if (vol == NULL) continue;

        if (Is_Image(Partitions_List[i]) == 0) {
            if (Find_Partition_Size(Partitions_List[i]) == 0) {
                Backup_Size += Total_Size;
                LOGI("%s backup size (/proc)=%lluMb\n", Partitions_List[i], Total_Size / 1048576LLU); // debug
            } else {
                ret++;
                strcat(skipped_parts, " - ");
                strcat(skipped_parts, Partitions_List[i]);
            }
        } else if (Is_File_System(Partitions_List[i]) == 0) {
            if (0 == ensure_path_mounted(vol->mount_point) && 0 == Get_Size_Via_statfs(vol->mount_point)) {
                Backup_Size += Used_Size;
                LOGI("%s backup size (stat)=%lluMb\n", Partitions_List[i], Used_Size / 1048576LLU); // debug
            } else {
                ret++;
                strcat(skipped_parts, " - ");
                strcat(skipped_parts, Partitions_List[i]);
            }
        } else {
            ret++;
            strcat(skipped_parts, " - Unknown file system: ");
            strcat(skipped_parts, Partitions_List[i]);
        }
    }

    if (backup_data && is_data_media()) {
        if (0 == ensure_path_mounted("/data") && 0 == Get_Size_Via_statfs("/data")) {
            unsigned long long data_backup_size;
            unsigned long long data_media_size = Get_Folder_Size("/data/media");
            unsigned long long data_used_bytes = Get_Folder_Size("/data");
            data_backup_size = data_used_bytes - data_media_size;
            Backup_Size += data_backup_size;
            LOGI("/data: tot size=%lluMb, free=%lluMb, backup size=%lluMb, used=%lluMb, media=%lluMb\n",
                    Total_Size/1048576LLU, Free_Size/1048576LLU, data_backup_size/1048576LLU,
                    data_used_bytes/1048576LLU, data_media_size/1048576LLU);
        } else {
            ret++;
            strcat(skipped_parts, " - /data");
        }
    }

    char tmp[PATH_MAX];
    get_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        unsigned long long andsec_size;
        andsec_size = Get_Folder_Size(tmp);
        Backup_Size += andsec_size;
        LOGI("%s backup size=%lluMb\n", tmp, andsec_size / 1048576LLU); // debug
    }

    int backup_size_mb = (int)(Backup_Size / 1048576LLU);
    backup_size_mb += 50; // extra 50 Mb for security measures

    ui_print("\n>> Free space: %dMb (%d%%)\n", free_mb, free_percent);
    ui_print(">> Needed space: %dMb\n", backup_size_mb);
    if (ret)
        ui_print(">> Unknown partitions size (%d):%s\n", ret, skipped_parts);

    if (free_percent < 3 || (default_backup_handler != tar_compress_wrapper && free_mb < backup_size_mb)) {
        if (!confirm_selection("Low free space! Continue anyway?", "Yes - Continue Nandroid Job"))
            return -1;
    }

    return 0;
}

static void show_backup_stats(const char* backup_path) {
    long total_msec = now_msec() - nandroid_start_msec;
    int minutes = total_msec / 60000;
    int seconds = (total_msec % 60000) / 1000;

    unsigned long long final_size = Get_Folder_Size(backup_path);
    long double compression;
    if (Backup_Size == 0 || final_size == 0 || compression_value == TAR_FORMAT)
        compression = 0;
    else compression = 1 - ((long double)(final_size) / (long double)(Backup_Size));

    ui_print("\nBackup complete!\n");
    ui_print("Backup time: %02i:%02i mn\n", minutes, seconds);
    ui_print("Backup size: %.2LfMb\n", (long double) final_size / 1048576);
    if (default_backup_handler == tar_compress_wrapper)
        ui_print("Compression: %.2Lf%%\n", compression * 100);
}

// show restore stats (only time for now)
static void show_restore_stats() {
    long total_msec = now_msec() - nandroid_start_msec;
    int minutes = total_msec / 60000;
    int seconds = (total_msec % 60000) / 1000;

    ui_print("\nRestore complete!\n");
    ui_print("Restore time: %02i:%02i mn\n", minutes, seconds);
}

//custom backup: raw backup through shell (ext4 raw backup not supported in backup_raw_partition())
//ret = 0 if success, else ret = 1
int dd_raw_backup_handler(const char* backup_path, const char* root)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);

    Volume *vol = volume_for_path(root);
    // make sure the volume exists...
    ui_print("\n>> Backing up %s...\nUsing raw mode...\n", root);
    if (vol == NULL || vol->fs_type == NULL) {
        ui_print("Volume not found! Skipping raw backup of %s\n", root);
        return 0;
    }

    int ret = 0;
    char tmp[PATH_MAX];
    if (vol->device[0] == '/')
        sprintf(tmp, "raw-backup.sh -b %s %s %s", backup_path, vol->device, root);
    else if (vol->device2 != NULL && vol->device2[0] == '/')
        sprintf(tmp, "raw-backup.sh -b %s %s %s", backup_path, vol->device2, root);
    else {
        ui_print("Invalid device! Skipping raw backup of %s\n", root);
        return 0;
    }

    if (0 != (ret = __system(tmp))) {
        ui_print("Failed raw backup of %s...\n", root);
    }
    //log
    //finish_nandroid_job();
    char logfile[PATH_MAX];
    sprintf(logfile, "%s/log.txt", backup_path);
    ui_print_custom_logtail(logfile, 3);
    return ret;
}

//custom raw restore handler
int dd_raw_restore_handler(const char* backup_path, const char* root)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);

    ui_print("\n>> Restoring %s...\n", root);
    Volume *vol = volume_for_path(root);
    // make sure the volume exists...
    if (vol == NULL || vol->fs_type == NULL) {
        ui_print("Volume not found! Skipping raw restore of %s...\n", root);
        return 0;
    }

    // make sure we  have a valid image file name
    const char *raw_image_format[] = { ".img", ".bin", NULL };
    char* image_file = basename(backup_path);
    int i = 0;
    while (raw_image_format[i] != NULL) {
        if (strlen(image_file) > strlen(raw_image_format[i]) &&
                    strcmp(image_file + strlen(image_file) - strlen(raw_image_format[i]), raw_image_format[i]) == 0 &&
                    strncmp(image_file, root + 1, strlen(root)-1) == 0) {
            break;
        }
        i++;
    }
    if (raw_image_format[i] == NULL) {
        ui_print("Invalid image file! Failed to restore %s to %s\n", image_file, root);
        return -1;
    }
    
    //make sure file exists
    struct stat file_check;
    if (0 != stat(backup_path, &file_check)) {
        ui_print("%s not found. Skipping restore of %s\n", backup_path, root);
        return -1;
    }

    //restore raw image
    int ret = 0;
    char tmp[PATH_MAX];
    ui_print("Restoring %s to %s\n", image_file, root);

    if (vol->device[0] == '/')
        sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_path, vol->device, root);
    else if (vol->device2 != NULL && vol->device2[0] == '/')
        sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_path, vol->device2, root);
    else {
        ui_print("Invalid device! Skipping raw restore of %s\n", root);
        return 0;
    }
    
    if (0 != (ret = __system(tmp)))
        ui_print("Failed raw restore of %s to %s\n", image_file, root);
    //log
    finish_nandroid_job();
    char *logfile = dirname(backup_path);
    sprintf(tmp, "%s/log.txt", logfile);
    ui_print_custom_logtail(tmp, 3);
    return ret;
}
//-------- end custom backup and restore functions


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/*                                       */
/*    TWRP backup and restore support    */
/*    Original CWM port by PhilZ@xda     */
/*    Original TWRP code by Dees_Troy    */
/*          (dees_troy at yahoo)         */
/*****************************************/

#define MAX_ARCHIVE_SIZE 4294967296LLU
int Makelist_File_Count;
unsigned long long Makelist_Current_Size;

static void compute_twrp_backup_stats(int index)
{
    char tmp[PATH_MAX];
    char line[PATH_MAX];
    struct stat info;
    int total = 0;
    sprintf(tmp, "/tmp/list/filelist%03i", index);
    FILE *fp = fopen(tmp, "rb");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            line[strlen(line)-1] = '\0';
            stat(line, &info);
            if (S_ISDIR(info.st_mode)) {
                compute_directory_stats(line);
                total += nandroid_files_total;
            } else
                total += 1;
        }
        nandroid_files_total = total;
        fclose(fp);
    } else {
        LOGE("Cannot compute backup stats for %s\n", tmp);
        LOGE("No progress will be shown during backup\n");
        nandroid_files_total = 0;
    }

    nandroid_files_count = 0;
    ui_reset_progress();
    ui_show_progress(1, 0);
}

int Add_Item(const char* Item_Name) {
    char actual_filename[255];
    FILE *fp;

    if (Makelist_File_Count > 999) {
        LOGE("File count is too large!\n");
        return -1;
    }

    sprintf(actual_filename, "/tmp/list/filelist%03i", Makelist_File_Count);

    fp = fopen(actual_filename, "a");
    if (fp == NULL) {
        LOGE("Failed to open '%s'\n", actual_filename);
        return -1;
    }
    if (fprintf(fp, "%s\n", Item_Name) < 0) {
        LOGE("Failed to write to '%s'\n", actual_filename);
        return -1;
    }
    if (fclose(fp) != 0) {
        LOGE("Failed to close '%s'\n", actual_filename);
        return -1;
    }
    return 0;
}

int Generate_File_Lists(const char* Path) {
    DIR* d;
    struct dirent* de;
    struct stat st;
    char FileName[PATH_MAX];

    if (is_data_media() && strlen(Path) >= 11 && strncmp(Path, "/data/media", 11) == 0)
        return 0; // Skip /data/media

    d = opendir(Path);
    if (d == NULL)
    {
        LOGE("error opening '%s'\n", Path);
        return -1;
    }

    while ((de = readdir(d)) != NULL)
    {
        sprintf(FileName, "%s/", Path);
        strcat(FileName, de->d_name);
        if (de->d_type == DT_DIR && strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
        {
            unsigned long long folder_size = Get_Folder_Size(FileName);
            if (Makelist_Current_Size + folder_size > MAX_ARCHIVE_SIZE) {
                if (Generate_File_Lists(FileName) < 0)
                    return -1;
            } else {
                strcat(FileName, "/");
                if (Add_Item(FileName) < 0)
                    return -1;
                Makelist_Current_Size += folder_size;
            }
        }
        else if (de->d_type == DT_REG || de->d_type == DT_LNK)
        {
            stat(FileName, &st);

            if (Makelist_Current_Size != 0 && Makelist_Current_Size + st.st_size > MAX_ARCHIVE_SIZE) {
                Makelist_File_Count++;
                Makelist_Current_Size = 0;
            }
            if (Add_Item(FileName) < 0)
                return -1;
            Makelist_Current_Size += st.st_size;
            if (st.st_size > 2147483648LL)
                LOGE("There is a file that is larger than 2GB in the file system\n'%s'\nThis file may not restore properly\n", FileName);
        }
    }
    closedir(d);
    return 0;
}

int Make_File_List(const char* backup_path)
{
    Makelist_File_Count = 0;
    Makelist_Current_Size = 0;
    __system("cd /tmp && rm -rf list");
    __system("cd /tmp && mkdir list");
    if (Generate_File_Lists(backup_path) < 0) {
        LOGE("Error generating file list\n");
        return -1;
    }
    ui_print("Done, generated %i file(s).\n", (Makelist_File_Count + 1));
    return (Makelist_File_Count + 1);
}

int twrp_backup_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    Volume *v = volume_for_path(backup_path);
    if (v == NULL) {
        ui_print("Unable to find volume.\n");
        return -1;
    }
    MountedVolume *mv = find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        ui_print("Unable to find mounted volume: %s\n", v->mount_point);
        return -1;
    }

    // Always use split format (simpler code) - Build lists of files to backup
    int backup_count;
    ui_print("Breaking backup file into multiple archives...\nGenerating file lists\n");
    backup_count = Make_File_List(backup_path);
    if (backup_count < 1) {
        LOGE("Error generating file list!\n");
        return -1;
    }
    struct stat st;
    if (0 != stat("/tmp/list/filelist000", &st)) {
        ui_print("Nothing to backup. Skipping %s\n", basename(backup_path));
        return 0;
    }

    unsigned long long total_bsize = 0, file_size;
    char tmp[PATH_MAX];
    int index;
    int nand_starts = 1;
    last_size_update = 0;
    for (index=0; index<backup_count; index++)
    {
        compute_twrp_backup_stats(index);
        if (compression_value == TAR_FORMAT)
            sprintf(tmp, "(tar -cvf '%s%03i' -T /tmp/list/filelist%03i) 2> /proc/self/fd/1 ; exit $?", backup_file_image, index, index);
        else
            sprintf(tmp, "(tar -cv -T /tmp/list/filelist%03i | pigz -%d >'%s%03i') 2> /proc/self/fd/1 ; exit $?", index, compression_value, backup_file_image, index);

        ui_print("  * Backing up archive %i/%i\n", (index + 1), backup_count);

        FILE *fp = __popen(tmp, "r");
        if (fp == NULL) {
            ui_print("Unable to execute tar.\n");
            return -1;
        }

        while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, backup_file_image, 1, &nand_starts))
            return -1;
#endif
            tmp[PATH_MAX - 1] = NULL;
            if (callback) {
                update_size_progress(backup_file_image);
                nandroid_callback(tmp);
            }
        }
        if (0 != __pclose(fp))
            return -1;

        sprintf(tmp, "%s%03i", backup_file_image, index);
        file_size = Get_File_Size(tmp);
        if (file_size == 0) {
            LOGE("Backup file size for '%s' is 0 bytes.\n", tmp); // oh noes! file size is 0, abort! abort!
            return -1;
        }
        total_bsize += file_size;
    }

    __system("cd /tmp && rm -rf list");
    ui_print("Total backup size:\n  %llu bytes.\n", total_bsize);
    return 0;
}

int twrp_backup(const char* backup_path) {
    nandroid_backup_bitfield = 0;
    refresh_default_backup_handler();
    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path.\n");
    
    int ret;
    struct statfs s;

    // refresh size stats for backup_path
    if (0 != Get_Size_Via_statfs(backup_path))
        return print_and_error("Unable to stat backup path.\n");

    if (check_backup_size() < 0)
        return print_and_error("Not enough free space: backup cancelled.\n");

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    nandroid_start_msec = now_msec();
#ifdef PHILZ_TOUCH_RECOVERY
    last_key_ev = nandroid_start_msec;
#endif

    char tmp[PATH_MAX];
    ensure_directory(backup_path);

    if (backup_boot && 0 != (ret = nandroid_backup_partition(backup_path, "/boot")))
        return ret;

    if (backup_recovery && 0 != (ret = nandroid_backup_partition(backup_path, "/recovery")))
        return ret;

    Volume *vol = volume_for_path("/efs");
    if (backup_efs &&  NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/efs")))
            return ret;
    }

    vol = volume_for_path("/misc");
    if (backup_misc && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/misc")))
            return ret;
    }

    sprintf(tmp, "/modem");
    vol = volume_for_path(tmp);
    if (vol == NULL) {
        sprintf(tmp, "/radio");
        vol = volume_for_path(tmp);
    }
    if (backup_modem && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, tmp)))
            return ret;
    }

    if (backup_system && 0 != (ret = nandroid_backup_partition(backup_path, "/system")))
        return ret;

    vol = volume_for_path("/preload");
    if (backup_preload && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/preload")))
            return ret;
    }

    if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/data")))
        return ret;

    if (has_datadata()) {
        if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/datadata")))
            return ret;
    }

    // handle .android_secure on external and internal storage
    get_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, tmp, 0)))
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    vol = volume_for_path("/sd-ext");
    if (backup_sdext) {
        if (vol == NULL || 0 != statfs(vol->device, &s))
        {
            ui_print("No sd-ext found. Skipping backup of sd-ext.\n");
        }
        else
        {
            if (0 != ensure_path_mounted("/sd-ext"))
                ui_print("Could not mount sd-ext. sd-ext backup may not be supported on this device. Skipping backup of sd-ext.\n");
            else if (0 != (ret = nandroid_backup_partition(backup_path, "/sd-ext")))
                return ret;
        }
    }

    if (enable_md5sum) {
        if (0 != (ret = gen_twrp_md5sum(backup_path)))
            return ret;
    }

    sprintf(tmp, "chmod -R 777 %s", backup_path);
    __system(tmp);

    finish_nandroid_job();
    show_backup_stats(backup_path);
    if (reboot_after_nandroid)
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    return 0;
}

static int is_gzip_file(const char* file_archive) {
    if (!file_found(file_archive)) {
        LOGE("Couldn't find archive file %s\n", file_archive);
        return -1;    
    }

    FILE *fp = fopen(file_archive, "rb");
    if (fp == NULL) {
        LOGE("Failed to open archive file %s\n", file_archive);
        return -1;
    }
    char buff[3];
    fread(buff, 1, 2, fp);
    static char magic_num[2] = {0x1F, 0x8B};
    int i;
    for(i = 0; i < 2; i++) {
        if (buff[i] != magic_num[i])
            return 0;
    }
    return 1;
}

int twrp_tar_extract_wrapper(const char* popen_command, int callback) {
    char tmp[PATH_MAX];
    strcpy(tmp, popen_command);
    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar.\n");
        return -1;
    }

    int nand_starts = 1;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, NULL, 0, &nand_starts))
            return -1;
#endif
        tmp[PATH_MAX - 1] = NULL;
        if (callback)
            nandroid_callback(tmp);
    }

    return __pclose(fp);
}

int twrp_restore_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    char cmd[PATH_MAX];
    char tar_args[6];
    int ret;
    // tar vs tar.gz format?
    if ((ret = is_gzip_file(backup_file_image)) < 0)
        return ret;
    if (ret == 0)
        sprintf(tar_args, "-xvf");
    else
        sprintf(tar_args, "-xzvf");

    if (strlen(backup_file_image) > strlen("win000") && strcmp(backup_file_image + strlen(backup_file_image) - strlen("win000"), "win000") == 0) {
        // multiple volume archive detected
        char main_filename[PATH_MAX];
        memset(main_filename, 0, sizeof(main_filename));
        strncpy(main_filename, backup_file_image, strlen(backup_file_image) - strlen("000"));
        int index = 0;
        sprintf(tmp, "%s%03i", main_filename, index);
        while(file_found(tmp)) {
            compute_archive_stats(tmp);
            ui_print("  * Restoring archive %d\n", index + 1);
            sprintf(cmd, "cd /; tar %s '%s'; exit $?", tar_args, tmp);
            if (0 != (ret = twrp_tar_extract_wrapper(cmd, callback)))
                return ret;
            index++;
            sprintf(tmp, "%s%03i", main_filename, index);
        }
    } else {
        //single volume archive
        compute_archive_stats(backup_file_image);
        sprintf(cmd, "cd %s; tar %s '%s'; exit $?", backup_path, tar_args, backup_file_image);
        ui_print("Restoring archive %s\n", basename(backup_file_image));
        ret = twrp_tar_extract_wrapper(cmd, callback);
    }
    return ret;
}

int twrp_restore(const char* backup_path)
{
    ensure_path_mounted("/sdcard"); // to be able to stat .hidenandroidprogress in nandroid_restore_partition_extended()
    Backup_Size = 0;
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    nandroid_start_msec = now_msec();
#ifdef PHILZ_TOUCH_RECOVERY
    last_key_ev = now_msec();
#endif
    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path\n");

    char tmp[PATH_MAX];
    if (enable_md5sum) {
        if (0 != check_twrp_md5sum(backup_path))
            return print_and_error("MD5 mismatch!\n");
    }

    int ret;

    if (backup_boot && NULL != volume_for_path("/boot") && 0 != (ret = nandroid_restore_partition(backup_path, "/boot")))
        return ret;

    if (backup_recovery && 0 != (ret = nandroid_restore_partition(backup_path, "/recovery")))
        return ret;

    Volume *vol = volume_for_path("/efs");
    if (backup_efs == RESTORE_EFS_TAR && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/efs")))
            return ret;
    }

    vol = volume_for_path("/misc");
    if (backup_misc && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/misc")))
            return ret;
    }

    sprintf(tmp, "/modem");
    vol = volume_for_path(tmp);
    if (vol == NULL) {
        sprintf(tmp, "/radio");
        vol = volume_for_path(tmp);
    }
    if (vol != NULL) {
        if (backup_modem == RAW_IMG_FILE && 0 != (ret = nandroid_restore_partition(backup_path, tmp)))
            return ret;
    }

    if (backup_system && 0 != (ret = nandroid_restore_partition(backup_path, "/system")))
        return ret;

    vol = volume_for_path("/preload");
    if (backup_preload && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/preload")))
            return ret;
    }

    if (backup_data && 0 != (ret = nandroid_restore_partition(backup_path, "/data")))
        return ret;
        
    if (has_datadata()) {
        if (backup_data && 0 != (ret = nandroid_restore_partition(backup_path, "/datadata")))
            return ret;
    }

    // handle .android_secure on external and internal storage
    get_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_restore_partition_extended(backup_path, tmp, 0)))
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (backup_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return ret;

    finish_nandroid_job();
    show_restore_stats();
    if (reboot_after_nandroid)
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    return 0;
}
//------------------------ end twrp backup and restore functions

int nandroid_backup(const char* backup_path)
{
    nandroid_backup_bitfield = 0; // for dedupe mode
    refresh_default_backup_handler();
    
    if (ensure_path_mounted(backup_path) != 0) {
        return print_and_error("Can't mount backup path.\n");
    }
/*
    // replaced by Get_Size_Via_statfs() check
    Volume* volume = volume_for_path(backup_path);
    if (NULL == volume)
        return print_and_error("Unable to find volume for backup path.\n");
    if (is_data_media_volume_path(volume->mount_point))
        volume = volume_for_path("/data");
*/
    int ret;
    struct statfs s;

    // refresh size stats for backup_path
    if (0 != (ret = Get_Size_Via_statfs(backup_path)))
        return print_and_error("Unable to stat backup path.\n");

    if (check_backup_size() < 0)
        return print_and_error("Not enough free space: backup cancelled.\n");

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    nandroid_start_msec = now_msec();
#ifdef PHILZ_TOUCH_RECOVERY
    last_key_ev = nandroid_start_msec; //support dim screen timeout during nandroid operation
#endif

    char tmp[PATH_MAX];
    ensure_directory(backup_path);

    if (backup_boot && 0 != (ret = nandroid_backup_partition(backup_path, "/boot")))
        return ret;

    if (backup_recovery && 0 != (ret = nandroid_backup_partition(backup_path, "/recovery")))
        return ret;

    Volume *vol = volume_for_path("/wimax");
    if (backup_wimax && vol != NULL && 0 == statfs(vol->device, &s))
    {
        char serialno[PROPERTY_VALUE_MAX];
        ui_print("\n>> Backing up WiMAX...\n");
        serialno[0] = 0;
        property_get("ro.serialno", serialno, "");
        sprintf(tmp, "%s/wimax.%s.img", backup_path, serialno);
        ret = backup_raw_partition(vol->fs_type, vol->device, tmp);
        if (0 != ret)
            return print_and_error("Error while dumping WiMAX image!\n");
    }

    //2 copies of efs are made: tarball and raw backup
    vol = volume_for_path("/efs");
    if (backup_efs && vol != NULL) {
        //first backup in raw format, returns 0 on success (or if skipped), else 1
        strcpy(tmp, backup_path);
        if (0 != dd_raw_backup_handler(dirname(tmp), "/efs"))
            ui_print("EFS raw image backup failed! Trying native backup...\n");

        //second backup in native cwm format
        ui_print("creating 2nd copy...\n");
        if (0 != (ret = nandroid_backup_partition(backup_path, "/efs")))
            return ret;
    }

    vol = volume_for_path("/misc");
    if (backup_misc && vol != NULL) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/misc")))
            return ret;
    }

    sprintf(tmp, "/modem");
    vol = volume_for_path(tmp);
    if (vol == NULL) {
        sprintf(tmp, "/radio");
        vol = volume_for_path(tmp);
    }
    if (backup_modem && vol != NULL) {
        if (0 != (ret = nandroid_backup_partition(backup_path, tmp)))
            return ret;
    }

    if (backup_system && 0 != (ret = nandroid_backup_partition(backup_path, "/system")))
        return ret;

    vol = volume_for_path("/preload");
    if (vol != NULL) {
        if (is_custom_backup && backup_preload) {
            if (0 != (ret = nandroid_backup_partition(backup_path, "/preload"))) {
                ui_print("Failed to backup /preload!\n");
                return ret;
            }
        }
        else if (!is_custom_backup && nandroid_add_preload) {
            if (0 != (ret = nandroid_backup_partition(backup_path, "/preload"))) {
                ui_print("Failed to backup preload! Try to disable it.\n");
                ui_print("Skipping /preload...\n");
                //return ret;
            }
        }
    }

    if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/data")))
        return ret;

    if (has_datadata()) {
        if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/datadata")))
            return ret;
    }

    // handle .android_secure on external and internal storage
    get_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, tmp, 0)))
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (backup_sdext) {
        vol = volume_for_path("/sd-ext");
        if (vol == NULL || 0 != statfs(vol->device, &s))
        {
            ui_print("No sd-ext found. Skipping backup of sd-ext.\n");
        }
        else
        {
            if (0 != ensure_path_mounted("/sd-ext"))
                ui_print("Could not mount sd-ext. sd-ext backup may not be supported on this device. Skipping backup of sd-ext.\n");
            else if (0 != (ret = nandroid_backup_partition(backup_path, "/sd-ext")))
                return ret;
        }
    }

    if (enable_md5sum) {
        ui_print("Generating md5 sum...\n");
        sprintf(tmp, "nandroid-md5.sh %s", backup_path);
        if (0 != (ret = __system(tmp))) {
            ui_print("Error while generating md5 sum!\n");
            return ret;
        }
    }

    sprintf(tmp, "cp /tmp/recovery.log %s/recovery.log", backup_path);
    __system(tmp);

    sprintf(tmp, "chmod -R 777 %s ; chmod -R u+r,u+w,g+r,g+w,o+r,o+w /sdcard/clockworkmod ; chmod u+x,g+x,o+x /sdcard/clockworkmod/backup ; chmod u+x,g+x,o+x /sdcard/clockworkmod/blobs", backup_path);
    __system(tmp);

    finish_nandroid_job();
    show_backup_stats(backup_path);
    if (reboot_after_nandroid)
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    return 0;
}

int nandroid_dump(const char* partition) {
    // silence our ui_print statements and other logging
    ui_set_log_stdout(0);

    nandroid_backup_bitfield = 0;
    refresh_default_backup_handler();

    // override our default to be the basic tar dumper
    default_backup_handler = tar_dump_wrapper;

    if (strcmp(partition, "boot") == 0) {
        return __system("dump_image boot /proc/self/fd/1 | cat");
    }

    if (strcmp(partition, "recovery") == 0) {
        return __system("dump_image recovery /proc/self/fd/1 | cat");
    }

    if (strcmp(partition, "data") == 0) {
        return nandroid_backup_partition("-", "/data");
    }

    if (strcmp(partition, "system") == 0) {
        return nandroid_backup_partition("-", "/system");
    }

    return 1;
}

typedef int (*nandroid_restore_handler)(const char* backup_file_image, const char* backup_path, int callback);

static int unyaffs_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd %s ; unyaffs %s ; exit $?", backup_path, backup_file_image);
    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute unyaffs.\n");
        return -1;
    }

    int nand_starts = 1;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, NULL, 0, &nand_starts))
            return -1;
#endif
        tmp[PATH_MAX - 1] = NULL;
        if (callback)
            nandroid_callback(tmp);
    }

    return __pclose(fp);
}

void compute_archive_stats(const char* archive_file)
{
    char tmp[PATH_MAX];
    if (twrp_backup_mode) {
        if (1 == is_gzip_file(archive_file))
            sprintf(tmp, "tar -tzf '%s' | wc -l > /tmp/archivecount", archive_file);
        else
            sprintf(tmp, "tar -tf '%s' | wc -l > /tmp/archivecount", archive_file);
    }
    else if (strlen(archive_file) > strlen(".tar") && strcmp(archive_file + strlen(archive_file) - strlen(".tar"), ".tar") == 0)
        sprintf(tmp, "cat %s* | tar -t | wc -l > /tmp/archivecount", archive_file);
    else if (strlen(archive_file) > strlen(".tar.gz") && strcmp(archive_file + strlen(archive_file) - strlen(".tar.gz"), ".tar.gz") == 0)
        sprintf(tmp, "cat %s* | tar -tz | wc -l > /tmp/archivecount", archive_file);

    ui_print("Computing archive stats for %s\n", basename(archive_file));
    if (0 != __system(tmp)) {
        nandroid_files_total = 0;
        LOGE("Failed computing archive stats for %s\n", archive_file);
        return;
    }
    char count_text[100];
    FILE* f = fopen("/tmp/archivecount", "r");
    fread(count_text, 1, sizeof(count_text), f);
    fclose(f);
    nandroid_files_count = 0;
    nandroid_files_total = atoi(count_text);
    ui_reset_progress();
    ui_show_progress(1, 0);
}

static int tar_extract_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    if (strlen(backup_file_image) > strlen("tar.gz") && strcmp(backup_file_image + strlen(backup_file_image) - strlen("tar.gz"), "tar.gz") == 0)
        sprintf(tmp, "cd $(dirname %s) ; cat %s* | pigz -d | tar xv ; exit $?", backup_path, backup_file_image);
    else
        sprintf(tmp, "cd $(dirname %s) ; cat %s* | tar xv ; exit $?", backup_path, backup_file_image);

    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar.\n");
        return -1;
    }

    int nand_starts = 1;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, NULL, 0, &nand_starts))
            return -1;
#endif
        tmp[PATH_MAX - 1] = NULL;
        if (callback)
            nandroid_callback(tmp);
    }

    return __pclose(fp);
}

static int dedupe_extract_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    char blob_dir[PATH_MAX];
    strcpy(blob_dir, backup_file_image);
    char *bd = dirname(blob_dir);
    strcpy(blob_dir, bd);
    bd = dirname(blob_dir);
    strcpy(blob_dir, bd);
    bd = dirname(blob_dir);
    sprintf(tmp, "dedupe x %s %s/blobs %s; exit $?", backup_file_image, bd, backup_path);

    char path[PATH_MAX];
    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute dedupe.\n");
        return -1;
    }

    int nand_starts = 1;
    while (fgets(path, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, NULL, 0, &nand_starts))
            return -1;
#endif
        if (callback)
            nandroid_callback(path);
    }

    return __pclose(fp);
}

static int tar_undump_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; tar xv ", backup_path);

    return __system(tmp);
}

static nandroid_restore_handler get_restore_handler(const char *backup_path) {
    Volume *v = volume_for_path(backup_path);
    if (v == NULL) {
        ui_print("Unable to find volume.\n");
        return NULL;
    }
    scan_mounted_volumes();
    MountedVolume *mv = find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        ui_print("Unable to find mounted volume: %s\n", v->mount_point);
        return NULL;
    }

    if (strcmp(backup_path, "/data") == 0 && is_data_media()) {
        return tar_extract_wrapper;
    }

    // cwr 5, we prefer tar for everything unless it is yaffs2
    char str[255];
    char* partition;
    property_get("ro.cwm.prefer_tar", str, "false");
    if (strcmp("true", str) != 0) {
        return unyaffs_wrapper;
    }

    if (strcmp("yaffs2", mv->filesystem) == 0) {
        return unyaffs_wrapper;
    }

    return tar_extract_wrapper;
}

int nandroid_restore_partition_extended(const char* backup_path, const char* mount_point, int umount_when_finished) {
    int ret = 0;
    char* name = basename(mount_point);

    nandroid_restore_handler restore_handler = NULL;
    const char *filesystems[] = { "yaffs2", "ext2", "ext3", "ext4", "vfat", "exfat", "rfs", NULL };
    const char* backup_filesystem = NULL;
    Volume *vol = volume_for_path(mount_point);
    const char *device = NULL;
    if (vol != NULL)
        device = vol->device;

    ui_print("\n>> Restoring %s...\n", mount_point);
    char tmp[PATH_MAX];
    sprintf(tmp, "%s/%s.img", backup_path, name);
    struct statfs file_info;
    if (strcmp(backup_path, "-") == 0) {
        if (vol)
            backup_filesystem = vol->fs_type;
        restore_handler = tar_extract_wrapper;
        strcpy(tmp, "/proc/self/fd/0");
    }
    else if (twrp_backup_mode || 0 != (ret = statfs(tmp, &file_info))) {
        // can't find the backup, it may be the new backup format?
        // iterate through the backup types
        printf("couldn't find old .img format\n");
        char *filesystem;
        int i = 0;
        while ((filesystem = filesystems[i]) != NULL) {
            if (twrp_backup_mode)
            {
                if (strstr(name, "android_secure") != NULL)
                    strcpy(name, "and-sec");

                sprintf(tmp, "%s/%s.%s.win", backup_path, name, filesystem);
                if (0 == (ret = statfs(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    break;
                }
                sprintf(tmp, "%s/%s.%s.win000", backup_path, name, filesystem);
                if (0 == (ret = statfs(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    break;
                }
                sprintf(tmp, "%s/%s.auto.win", backup_path, name);
                if (0 == (ret = statfs(tmp, &file_info))) {
                    break;
                }
                sprintf(tmp, "%s/%s.auto.win000", backup_path, name);
                if (0 == (ret = statfs(tmp, &file_info))) {
                    break;
                }
            } else {
                sprintf(tmp, "%s/%s.%s.img", backup_path, name, filesystem);
                if (0 == (ret = statfs(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    restore_handler = unyaffs_wrapper;
                    break;
                }
                sprintf(tmp, "%s/%s.%s.tar", backup_path, name, filesystem);
                if (0 == (ret = statfs(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    restore_handler = tar_extract_wrapper;
                    break;
                }
                sprintf(tmp, "%s/%s.%s.tar.gz", backup_path, name, filesystem);
                if (0 == (ret = statfs(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    restore_handler = tar_extract_wrapper;
                    break;
                }
                sprintf(tmp, "%s/%s.%s.dup", backup_path, name, filesystem);
                if (0 == (ret = statfs(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    restore_handler = dedupe_extract_wrapper;
                    break;
                }
            }
            i++;
        }

        if (twrp_backup_mode) {
            if (ret != 0) {
                ui_print("Could not find TWRP backup image for %s\n", mount_point);
                ui_print("Skipping restore of %s\n", mount_point);
                return 0;
            }
            ui_print("Found backup image: %s\n", basename(tmp));
        }            
        else if (backup_filesystem == NULL || restore_handler == NULL) {
            //ui_print("%s.img not found. Skipping restore of %s.\n", name, mount_point);
            ui_print("No %s backup found(img, tar, dup). Skipping restore of %s.\n", name, mount_point);
            return 0;
        }
        else {
            printf("Found new backup image: %s\n", tmp);
        }
    }

    // If the fs_type of this volume is "auto" or mount_point is /data
    // and is_data_media, let's revert
    // to using a rm -rf, rather than trying to do a
    // ext3/ext4/whatever format.
    // This is because some phones (like DroidX) will freak out if you
    // reformat the /system or /data partitions, and not boot due to
    // a locked bootloader.
    // Other devices, like the Galaxy Nexus, XOOM, and Galaxy Tab 10.1
    // have a /sdcard symlinked to /data/media.
    // Or of volume does not exist (.android_secure), just rm -rf.
    if (vol == NULL || 0 == strcmp(vol->fs_type, "auto"))
        backup_filesystem = NULL;
    if (0 == strcmp(vol->mount_point, "/data") && is_data_media())
        backup_filesystem = NULL;

    ensure_directory(mount_point);

    int callback = statfs("/sdcard/clockworkmod/.hidenandroidprogress", &file_info) != 0;
    if (!twrp_backup_mode) compute_archive_stats(tmp);

    ui_print("Restoring %s...\n", name);
    if (backup_filesystem == NULL) {
        if (0 != (ret = format_volume(mount_point))) {
            ui_print("Error while formatting %s!\n", mount_point);
            return ret;
        }
    }
    else if (0 != (ret = format_device(device, mount_point, backup_filesystem))) {
        ui_print("Error while formatting %s!\n", mount_point);
        return ret;
    }

    if (0 != (ret = ensure_path_mounted(mount_point))) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }

    if (twrp_backup_mode) {
        if (0 != (ret = twrp_restore_wrapper(tmp, mount_point, callback))) {
            ui_print("Error while restoring %s!\n", mount_point);
            return ret;
        }
    } else {
        if (restore_handler == NULL)
            restore_handler = get_restore_handler(mount_point);

        // override restore handler for undump
        if (strcmp(backup_path, "-") == 0) {
            restore_handler = tar_undump_wrapper;
        }

        if (restore_handler == NULL) {
            ui_print("Error finding an appropriate restore handler.\n");
            return -2;
        }

        if (0 != (ret = restore_handler(tmp, mount_point, callback))) {
            ui_print("Error while restoring %s!\n", mount_point);
            return ret;
        }
    }

    if (umount_when_finished) {
        ensure_path_unmounted(mount_point);
    }

    return 0;
}

int nandroid_restore_partition(const char* backup_path, const char* root) {
    Volume *vol = volume_for_path(root);
    // make sure the volume exists...
    if (vol == NULL || vol->fs_type == NULL) {
        ui_print("Volume not found! Skipping restore of %s...\n", root);
        return 0;
    }

    // see if we need a raw restore (mtd)
    char tmp[PATH_MAX];
    if (strcmp(vol->fs_type, "mtd") == 0 ||
            strcmp(vol->fs_type, "bml") == 0 ||
            strcmp(vol->fs_type, "emmc") == 0) {
        ui_print("\n>> Restoring %s...\nUsing raw mode...\n", root);
        int ret;
        const char* name = basename(root);

        //fix partition could be formatted when no image to restore (exp: if md5 check disabled and empty backup folder)
        struct stat file_check;
        if (strcmp(backup_path, "-") == 0)
            strcpy(tmp, backup_path);
        else if (twrp_backup_mode)
            sprintf(tmp, "%s%s.%s.win", backup_path, root, vol->fs_type);
        else
            sprintf(tmp, "%s%s.img", backup_path, root);

        if (0 != strcmp(backup_path, "-") && 0 != stat(tmp, &file_check)) {
            ui_print("%s not found. Skipping restore of %s\n", basename(tmp), root);
            return 0;
        }

        ui_print("Erasing %s before restore...\n", name);
        if (0 != (ret = format_volume(root))) {
            ui_print("Error while erasing %s image!\n", name);
            return ret;
        }
        //sprintf(tmp, "%s%s.img", backup_path, root);
        ui_print("Restoring %s image...\n", name);
        if (0 != (ret = restore_raw_partition(vol->fs_type, vol->device, tmp))) {
            ui_print("Error while flashing %s image!\n", name);
            return ret;
        }
        return 0;
    }
    return nandroid_restore_partition_extended(backup_path, root, 1);
}

int nandroid_restore(const char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache, int restore_sdext, int restore_wimax)
{
    ensure_path_mounted("/sdcard"); // to be able to stat .hidenandroidprogress in nandroid_restore_partition_extended()
    Backup_Size = 0;
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    nandroid_start_msec = now_msec();
#ifdef PHILZ_TOUCH_RECOVERY
    last_key_ev = now_msec();
#endif
    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path\n");
    
    char tmp[PATH_MAX];
    if (enable_md5sum) {
        ui_print("Checking MD5 sums...\n");
        sprintf(tmp, "cd %s && md5sum -c nandroid.md5", backup_path);
        if (0 != __system(tmp))
            return print_and_error("MD5 mismatch!\n");
    }

    int ret;

    if (restore_boot && NULL != volume_for_path("/boot") && 0 != (ret = nandroid_restore_partition(backup_path, "/boot")))
        return ret;

    if (is_custom_backup) {
        if (backup_recovery && 0 != (ret = nandroid_restore_partition(backup_path, "/recovery")))
            return ret;
    }

    struct statfs s;
    Volume *vol = volume_for_path("/wimax");
    if (restore_wimax && vol != NULL && 0 == statfs(vol->device, &s))
    {
        ui_print("\n>> Restoring WiMAX...\n");
        char serialno[PROPERTY_VALUE_MAX];
        
        serialno[0] = 0;
        property_get("ro.serialno", serialno, "");
        sprintf(tmp, "%s/wimax.%s.img", backup_path, serialno);

        struct stat st;
        if (0 != stat(tmp, &st))
        {
            ui_print("WARNING: WiMAX partition exists, but nandroid\n");
            ui_print("         backup does not contain WiMAX image.\n");
            ui_print("         You should create a new backup to\n");
            ui_print("         protect your WiMAX keys.\n");
        }
        else
        {
            ui_print("Erasing WiMAX before restore...\n");
            if (0 != (ret = format_volume("/wimax")))
                return print_and_error("Error while formatting wimax!\n");
            ui_print("Restoring WiMAX image...\n");
            if (0 != (ret = restore_raw_partition(vol->fs_type, vol->device, tmp)))
                return ret;
        }
    }

    // restore of raw efs image files (efs_time-stamp.img) is done elsewhere
    // as it needs to pass in a filename (instead of a folder) as backup_path
    // this could be done here since efs is processed alone, but must be done before md5 checksum!
    // same applies for modem.bin restore
    vol = volume_for_path("/efs");
    if (backup_efs == RESTORE_EFS_TAR && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/efs")))
            return ret;
    }

    vol = volume_for_path("/misc");
    if (backup_misc && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/misc")))
            return ret;
    }

    sprintf(tmp, "/modem");
    vol = volume_for_path(tmp);
    if (vol == NULL) {
        sprintf(tmp, "/radio");
        vol = volume_for_path(tmp);
    }
    if (vol != NULL) {
        if (backup_modem == RAW_IMG_FILE && 0 != (ret = nandroid_restore_partition(backup_path, tmp)))
            return ret;
    }

    if (restore_system && 0 != (ret = nandroid_restore_partition(backup_path, "/system")))
        return ret;

    vol = volume_for_path("/preload");
    if (vol != NULL) {
        if (is_custom_backup && backup_preload) {
            if (0 != (ret = nandroid_restore_partition(backup_path, "/preload"))) {
                ui_print("Failed to restore /preload!\n");
                return ret;
            }
        }
        else if (!is_custom_backup && nandroid_add_preload) {
            if (restore_system && 0 != (ret = nandroid_restore_partition(backup_path, "/preload"))) {
                ui_print("Failed to restore preload! Try to disable it.\n");
                ui_print("Skipping /preload...\n");
                //return ret;
            }
        }
    }

    if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "/data")))
        return ret;
        
    if (has_datadata()) {
        if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "/datadata")))
            return ret;
    }

    // handle .android_secure on external and internal storage
    get_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_restore_partition_extended(backup_path, tmp, 0)))
            return ret;
    }

    if (restore_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (restore_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return ret;

    finish_nandroid_job();
    show_restore_stats();
    if (reboot_after_nandroid)
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    return 0;
}

int nandroid_undump(const char* partition) {
    nandroid_files_total = 0;

    int ret;

    if (strcmp(partition, "boot") == 0) {
        return __system("flash_image boot /proc/self/fd/0");
    }

    if (strcmp(partition, "recovery") == 0) {
        if(0 != (ret = nandroid_restore_partition("-", "/recovery")))
            return ret;
    }

    if (strcmp(partition, "system") == 0) {
        if(0 != (ret = nandroid_restore_partition("-", "/system")))
            return ret;
    }

    if (strcmp(partition, "data") == 0) {
        if(0 != (ret = nandroid_restore_partition("-", "/data")))
            return ret;
    }

    sync();
    return 0;
}

int nandroid_usage()
{
    printf("Usage: nandroid backup\n");
    printf("Usage: nandroid restore <directory>\n");
    printf("Usage: nandroid dump <partition>\n");
    printf("Usage: nandroid undump <partition>\n");
    return 1;
}

static int bu_usage() {
    printf("Usage: bu <fd> backup partition\n");
    printf("Usage: Prior to restore:\n");
    printf("Usage: echo -n <partition> > /tmp/ro.bu.restore\n");
    printf("Usage: bu <fd> restore\n");
    return 1;
}

int bu_main(int argc, char** argv) {
    load_volume_table();

    if (strcmp(argv[2], "backup") == 0) {
        if (argc != 4) {
            return bu_usage();
        }

        int fd = atoi(argv[1]);
        char* partition = argv[3];

        if (fd != STDOUT_FILENO) {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // fprintf(stderr, "%d %d %s\n", fd, STDOUT_FILENO, argv[3]);
        return nandroid_dump(partition);
    }
    else if (strcmp(argv[2], "restore") == 0) {
        if (argc != 3) {
            return bu_usage();
        }

        int fd = atoi(argv[1]);
        if (fd != STDIN_FILENO) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        char partition[100];
        FILE* f = fopen("/tmp/ro.bu.restore", "r");
        fread(partition, 1, sizeof(partition), f);
        fclose(f);

        // fprintf(stderr, "%d %d %s\n", fd, STDIN_FILENO, argv[3]);
        return nandroid_undump(partition);
    }

    return bu_usage();
}

int nandroid_main(int argc, char** argv)
{
    load_volume_table();

    if (argc > 3 || argc < 2)
        return nandroid_usage();

    if (strcmp("backup", argv[1]) == 0)
    {
        if (argc != 2)
            return nandroid_usage();
        
        char backup_path[PATH_MAX];
        nandroid_generate_timestamp_path(backup_path);
        return nandroid_backup(backup_path);
    }

    if (strcmp("restore", argv[1]) == 0)
    {
        if (argc != 3)
            return nandroid_usage();
        return nandroid_restore(argv[2], 1, 1, 1, 1, 1, 0);
    }

    if (strcmp("dump", argv[1]) == 0)
    {
        if (argc != 3)
            return nandroid_usage();
        return nandroid_dump(argv[2]);
    }

    if (strcmp("undump", argv[1]) == 0)
    {
        if (argc != 3)
            return nandroid_usage();
        return nandroid_undump(argv[2]);
    }

    return nandroid_usage();
}
