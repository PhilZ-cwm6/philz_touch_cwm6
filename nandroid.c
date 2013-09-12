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
#include "advanced_functions.h"
#include "nandroid.h"
#include "mounts.h"

#include "flashutils/flashutils.h"
#include <libgen.h>


#ifdef PHILZ_TOUCH_RECOVERY
#include "/root/Desktop/PhilZ_Touch/touch_source/philz_nandroid_gui_cm-10.1.c"
#endif

// time in msec when nandroid job starts: used for dim timeout and total backup time
static long nandroid_start_msec;

void nandroid_generate_timestamp_path(const char* backup_path)
{
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    if (tmp == NULL)
    {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        sprintf(backup_path, "/sdcard/clockworkmod/backup/%ld", tp.tv_sec);
    }
    else
    {
        strftime(backup_path, PATH_MAX, "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
    }
}

void ensure_directory(const char* dir) {
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s; chmod 777 %s", dir, dir);
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
        tmp[strlen(tmp) - 1] = '\0';
    tmp[ui_get_text_cols() - 1] = '\0';

    nandroid_files_count++;
    ui_increment_frame();

    char size_progress[256] = "Size progress: N/A";
    if (show_nandroid_size_progress && Backup_Size != 0) {
        sprintf(size_progress, "Done %llu/%lluMb - Free %lluMb",
                (Used_Size - Before_Used_Size) / 1048576LLU, Backup_Size / 1048576LLU, Free_Size / 1048576LLU);
    }
    size_progress[ui_get_text_cols() - 1] = '\0';

#ifdef PHILZ_TOUCH_RECOVERY
    int color[] = {CYAN_BLUE_CODE};
    ui_print_color(3, color);
#endif

    // do not write size progress to log file
    ui_nolog_lines(1);
    // strlen(tmp) check avoids ui_nolog_lines() printing size progress to log on empty lines
    if (strlen(tmp) == 0)
        sprintf(tmp, " ");
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
static void update_size_progress(const char* Path) {
    if (!show_nandroid_size_progress || Backup_Size == 0)
        return;
    // statfs every 5 sec interval maximum (some sdcards and phones cannot support previous 0.5 sec)
    if (last_size_update == 0 || (gettime_now_msec() - last_size_update) > 5000) {
        Get_Size_Via_statfs(Path);
        last_size_update = gettime_now_msec();
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
        tmp[PATH_MAX - 1] = '\0';
        if (callback) {
            update_size_progress(backup_file_image);
            nandroid_callback(tmp);
        }
    }

    return __pclose(fp);
}

static int do_tar_compress(char* command, int callback, const char* backup_file_image) {
    char buf[PATH_MAX];

    FILE *fp = __popen(command, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar command!\n");
        return -1;
    }

    int nand_starts = 1;
    last_size_update = 0;
    while (fgets(buf, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, backup_file_image, 1, &nand_starts))
            return -1;
#endif
        buf[PATH_MAX - 1] = '\0';
        if (callback) {
            update_size_progress(backup_file_image);
            nandroid_callback(buf);
        }
    }

    return __pclose(fp);
}

static int tar_compress_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; touch %s.tar ; (tar cv --exclude=data/data/com.google.android.music/files/* %s $(basename %s) | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.) 2> /proc/self/fd/1 ; exit $?", backup_path, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path, backup_file_image);

    return do_tar_compress(tmp, callback, backup_file_image);
}

static int tar_gzip_compress_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; touch %s.tar.gz ; (tar cv --exclude=data/data/com.google.android.music/files/* %s $(basename %s) | pigz -c -%d | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.gz.) 2> /proc/self/fd/1 ; exit $?", backup_path, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path, compression_value, backup_file_image);

    return do_tar_compress(tmp, callback, backup_file_image);
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
        tmp[PATH_MAX - 1] = '\0';
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
    fmt[3] = '\0';
    if (0 == strcmp(fmt, "dup"))
        default_backup_handler = dedupe_compress_wrapper;
    else if (0 == strcmp(fmt, "tgz"))
        default_backup_handler = tar_gzip_compress_wrapper;
    else
        default_backup_handler = tar_compress_wrapper;
}

unsigned nandroid_get_default_backup_format() {
    refresh_default_backup_handler();
    if (default_backup_handler == dedupe_compress_wrapper) {
        return NANDROID_BACKUP_FORMAT_DUP;
    } else if (default_backup_handler == tar_gzip_compress_wrapper) {
        return NANDROID_BACKUP_FORMAT_TGZ;
    } else {
        return NANDROID_BACKUP_FORMAT_TAR;
    }
}

static int override_yaffs2_wrapper = 1;
void set_override_yaffs2_wrapper(int set) {
    override_yaffs2_wrapper = set;
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

    if (override_yaffs2_wrapper && strlen(forced_backup_format) > 0)
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
    char tmp[PATH_MAX];
    strcpy(name, basename(mount_point));

    struct stat file_info;
    ensure_path_mounted("/sdcard");
    int callback = stat("/sdcard/clockworkmod/.hidenandroidprogress", &file_info) != 0;

    ui_print("\n>> Backing up %s...\n", mount_point);
    if (0 != (ret = ensure_path_mounted(mount_point) != 0)) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }

    compute_directory_stats(mount_point);
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

    if (check_backup_size(backup_path) < 0)
        return print_and_error("Not enough free space: backup cancelled.\n");

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    nandroid_start_msec = gettime_now_msec();
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

    if (backup_system && 0 != (ret = nandroid_backup_partition(backup_path, "/system")))
        return ret;

    if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/data")))
        return ret;

    if (has_datadata()) {
        if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/datadata")))
            return ret;
    }

    // handle .android_secure on external and internal storage
    set_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, tmp, 0)))
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (backup_sdext) {
        vol = volume_for_path("/sd-ext");
        if (vol == NULL || 0 != statfs(vol->device, &s))
            // could be we need ensure_path_mouned("/sd-ext") before this!
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

    // 2 copies of efs are made: tarball and dd/cat raw backup
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

    vol = volume_for_path("/modem");
    if (backup_modem && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/modem")))
            return ret;
    }

    vol = volume_for_path("/radio");
    if (backup_radio && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/radio")))
            return ret;
    }

    if (backup_data_media && 0 != (ret = nandroid_backup_datamedia(backup_path)))
        return ret;

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
    last_size_update = 0;
    check_restore_size(backup_file_image, backup_path);
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, NULL, 0, &nand_starts))
            return -1;
#endif
        tmp[PATH_MAX - 1] = '\0';
        if (callback) {
            update_size_progress(backup_path);
            nandroid_callback(tmp);
        }
    }

    return __pclose(fp);
}

static int do_tar_extract(char* command, const char* backup_file_image, const char* backup_path, int callback) {
    char buf[PATH_MAX];

    FILE *fp = __popen(command, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar command.\n");
        return -1;
    }

    int nand_starts = 1;
    last_size_update = 0;
    check_restore_size(backup_file_image, backup_path);
    while (fgets(buf, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, NULL, 0, &nand_starts))
            return -1;
#endif
        buf[PATH_MAX - 1] = '\0';
        if (callback) {
            update_size_progress(backup_path);
            nandroid_callback(buf);
        }
    }

    return __pclose(fp);
}

static int tar_gzip_extract_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; cat %s* | pigz -d -c | tar xv ; exit $?", backup_path, backup_file_image);

    return do_tar_extract(tmp, backup_file_image, backup_path, callback);
}

static int tar_extract_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; cat %s* | tar xv ; exit $?", backup_path, backup_file_image);

    return do_tar_extract(tmp, backup_file_image, backup_path, callback);
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

#include "nandroid_advanced.c"

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
    struct stat file_info;
    if (strcmp(backup_path, "-") == 0) {
        if (vol)
            backup_filesystem = vol->fs_type;
        restore_handler = tar_extract_wrapper;
        strcpy(tmp, "/proc/self/fd/0");
    }
    else if (twrp_backup_mode || 0 != (ret = stat(tmp, &file_info))) {
        // can't find the backup, it may be the new backup format?
        // iterate through the backup types
        printf("couldn't find old .img format\n");
        char *filesystem;
        int i = 0;
        while ((filesystem = filesystems[i]) != NULL) {
            if (twrp_backup_mode) {
                if (strstr(name, "android_secure") != NULL)
                    strcpy(name, "and-sec");

                sprintf(tmp, "%s/%s.%s.win", backup_path, name, filesystem);
                if (0 == (ret = stat(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    break;
                }
                sprintf(tmp, "%s/%s.%s.win000", backup_path, name, filesystem);
                if (0 == (ret = stat(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    break;
                }
                sprintf(tmp, "%s/%s.auto.win", backup_path, name);
                if (0 == (ret = stat(tmp, &file_info))) {
                    break;
                }
                sprintf(tmp, "%s/%s.auto.win000", backup_path, name);
                if (0 == (ret = stat(tmp, &file_info))) {
                    break;
                }
            } else {
                sprintf(tmp, "%s/%s.%s.img", backup_path, name, filesystem);
                if (0 == (ret = stat(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    restore_handler = unyaffs_wrapper;
                    break;
                }
                sprintf(tmp, "%s/%s.%s.tar", backup_path, name, filesystem);
                if (0 == (ret = stat(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    restore_handler = tar_extract_wrapper;
                    break;
                }
                sprintf(tmp, "%s/%s.%s.tar.gz", backup_path, name, filesystem);
                if (0 == (ret = stat(tmp, &file_info))) {
                    backup_filesystem = filesystem;
                    restore_handler = tar_gzip_extract_wrapper;
                    break;
                }
                sprintf(tmp, "%s/%s.%s.dup", backup_path, name, filesystem);
                if (0 == (ret = stat(tmp, &file_info))) {
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

    ensure_path_mounted("/sdcard");
    int callback = stat("/sdcard/clockworkmod/.hidenandroidprogress", &file_info) != 0;

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

        // fix partition could be formatted when no image to restore
        // exp: if md5 check disabled and empty backup folder
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
    Backup_Size = 0;
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    nandroid_files_total = 0;
    nandroid_start_msec = gettime_now_msec();
#ifdef PHILZ_TOUCH_RECOVERY
    last_key_ev = gettime_now_msec();
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

    vol = volume_for_path("/modem");
    if (backup_modem == RAW_IMG_FILE && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/modem")))
            return ret;
    }

    vol = volume_for_path("/radio");
    if (backup_radio == RAW_IMG_FILE && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/radio")))
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
    set_android_secure_path(tmp);
    if (restore_data && android_secure_ext) {
        if (0 != (ret = nandroid_restore_partition_extended(backup_path, tmp, 0)))
            return ret;
    }

    if (restore_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (restore_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return ret;

    if (backup_data_media && 0 != (ret = nandroid_restore_datamedia(backup_path)))
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
        int ret = nandroid_dump(partition);
        sleep(10);
        return ret;
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
    char backup_path[PATH_MAX];

    if (argc > 3 || argc < 2)
        return nandroid_usage();

    if (strcmp("backup", argv[1]) == 0)
    {
        if (argc != 2)
            return nandroid_usage();

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
