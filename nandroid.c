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

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"

#include "flashutils/flashutils.h"
#include <libgen.h>

void nandroid_generate_timestamp_path(const char* backup_path)
{
#ifdef PHILZ_TOUCH_RECOVERY
    time_t t = time(NULL) + t_zone;
#else
    time_t t = time(NULL);
#endif
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
    sprintf(tmp, "mkdir -p %s ; chmod 775 %s", dir, dir);
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
    ui_nice_print("%s\n", tmp);
    if (!ui_was_niced() && nandroid_files_total != 0)
        ui_set_progress((float)nandroid_files_count / (float)nandroid_files_total);
    if (!ui_was_niced())
        ui_delete_line();
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

    while (fgets(tmp, PATH_MAX, fp) != NULL) {
        tmp[PATH_MAX - 1] = NULL;
        if (callback)
            nandroid_callback(tmp);
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

    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar.\n");
        return -1;
    }

    while (fgets(tmp, PATH_MAX, fp) != NULL) {
        tmp[PATH_MAX - 1] = NULL;
        if (callback)
            nandroid_callback(tmp);
    }

    return __pclose(fp);
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

    while (fgets(tmp, PATH_MAX, fp) != NULL) {
        tmp[PATH_MAX - 1] = NULL;
        if (callback)
            nandroid_callback(tmp);
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
        if (mv == NULL || mv->filesystem == NULL)
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
    if (vol == NULL || vol->fs_type == NULL)
    {
        ui_print("\n>> Backing up %s...\n", root);
        ui_print("Volume not found! Skipping backup of %s...\n", root);
        return 0;
    }

    // see if we need a raw backup (mtd)
    char tmp[PATH_MAX];
    int ret;
    if (strcmp(vol->fs_type, "mtd") == 0 ||
            strcmp(vol->fs_type, "bml") == 0 ||
            strcmp(vol->fs_type, "emmc") == 0)
    {
        ui_print("\n>> Backing up %s...\n", root);
        const char* name = basename(root);

        if (twrp_backup_mode)
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


/***************************************/
/*  Custom Backup and Restore Support  */
/*      code written by PhilZ @xda     */
/*       for PhilZ Touch Recovery      */
/*  Do not remove this credits header  */
/***************************************/

//these general variables are needed to not break backup and restore by external script
int backup_boot = 1, backup_recovery = 1, backup_wimax = 1, backup_system = 1, backup_preload = 1;
int backup_data = 1, backup_cache = 1, backup_sdext = 1;
int backup_efs = 0, backup_modem = 0;
int is_custom_backup = 0;
int reboot_after_nandroid = 0;
int android_secure_ext = 0;


void finish_nandroid_job() {
    ui_print("Finalizing, please wait...\n");
    sync();
        ui_set_background(BACKGROUND_ICON_NONE);

    ui_reset_progress();
}

//custom backup: raw backup through shell (ext4 raw backup not supported in backup_raw_partition())
//ret = 0 if success, else ret = 1
int custom_backup_raw_handler(const char* backup_path, const char* root)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);

    Volume *vol = volume_for_path(root);
    // make sure the volume exists...
    ui_print("\n>> Backing up %s...\nUsing raw mode...\n", root);
    if (vol == NULL || vol->fs_type == NULL) {
        ui_print("Volume not found! Skipping raw backup of %s...\n", root);
        return 0;
    }

    int ret = 0;
    char tmp[PATH_MAX];
    sprintf(tmp, "raw-backup.sh -b %s %s %s", backup_path, vol->device, root);
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
int custom_restore_raw_handler(const char* backup_path, const char* root)
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
    ui_print("Restoring %s to %s\n", image_file, root);
    int ret = 0;
    char tmp[PATH_MAX];
    sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_path, vol->device, root);
    if (0 != (ret = __system(tmp))) {
        ui_print("Failed raw restore of %s to %s\n", image_file, root);
    }
    //log
    finish_nandroid_job();
    char *logfile = dirname(backup_path);
    sprintf(tmp, "%s/log.txt", logfile);
    ui_print_custom_logtail(tmp, 3);
    return ret;
}
//-------- end custom backup and restore functions


/***********************************/
/* TWRP backup and restore support */
/* Original CWM port by PhilZ@xda  */
/* Original TWRP code by Dees_Troy */
/*       (dees_troy at yahoo)      */
/*     Keep this credits header    */
/***********************************/

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
    sync();
            tmp[PATH_MAX - 1] = NULL;
            if (callback)
                nandroid_callback(tmp);
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

int twrp_backup(const char* backup_path)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    
    if (ensure_path_mounted(backup_path) != 0) {
        return print_and_error("Can't mount backup path.\n");
    }
    
    Volume* volume = volume_for_path(backup_path);
    if (NULL == volume)
        return print_and_error("Unable to find volume for backup path.\n");
    if (is_data_media_volume_path(volume->mount_point))
        volume = volume_for_path("/data");
    int ret;
    struct statfs s;
    if (NULL != volume) {
        if (0 != (ret = statfs(volume->mount_point, &s)))
            return print_and_error("Unable to stat backup path.\n");
        uint64_t bavail = s.f_bavail;
        uint64_t bsize = s.f_bsize;
        uint64_t sdcard_free = bavail * bsize;
        uint64_t sdcard_free_mb = sdcard_free / (uint64_t)(1024 * 1024);
        ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);
        if (sdcard_free_mb < 150)
            ui_print("There may not be enough free space to complete backup... continuing...\n");
    }
    char tmp[PATH_MAX];
    ensure_directory(backup_path);

    if (backup_boot && 0 != (ret = nandroid_backup_partition(backup_path, "/boot")))
        return ret;

    if (backup_recovery && 0 != (ret = nandroid_backup_partition(backup_path, "/recovery")))
        return ret;

    if (backup_efs) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/efs")))
            return ret;
    }
    
    if (backup_modem) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/modem")))
            return ret;
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
    if (backup_data && android_secure_ext == 0) {
        // android_secure_ext == 0: assume default /sdcard path
        if (is_data_media())
            ui_print("Skipping android_secure backup from /data/media.\n");
        else if (0 != stat("/sdcard/.android_secure", &s))
            ui_print("No /sdcard/.android_secure found. Skipping backup of applications on external storage.\n");
        else if (0 != (ret = nandroid_backup_partition_extended(backup_path, "/sdcard/.android_secure", 0)))
            return ret;
    }
    else if (backup_data && android_secure_ext == 1) {
        // android_secure_ext == 1: look in second storage
        if (0 == stat("/external_sd/.android_secure", &s))
            ret = nandroid_backup_partition_extended(backup_path, "/external_sd/.android_secure", 0);
        else if (0 == stat("/emmc/.android_secure", &s))
            ret = nandroid_backup_partition_extended(backup_path, "/emmc/.android_secure", 0);
        else
            ui_print("No .android_secure found on second storage. Skipping backup of applications on external storage.\n");

        if (ret != 0)
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    Volume *vol = volume_for_path("/sd-ext");
    if (backup_sdext) {
        if (vol == NULL || 0 != stat(vol->device, &s))
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

#ifdef PHILZ_TOUCH_RECOVERY
    if (enable_md5sum)
#endif
    {
        if (0 != (ret = gen_twrp_md5sum(backup_path)))
            return ret;
    }

    sprintf(tmp, "chmod -R 777 %s", backup_path);
    __system(tmp);

    finish_nandroid_job();
    ui_print("\nTWRP Backup complete!\n");
    if (reboot_after_nandroid)
        reboot_main_system();
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
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
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
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();

    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path\n");

    char tmp[PATH_MAX];
#ifdef PHILZ_TOUCH_RECOVERY
    if (enable_md5sum)
#endif
    {
        if (0 != check_twrp_md5sum(backup_path))
            return print_and_error("MD5 mismatch!\n");
    }

    int ret;

    if (backup_boot && NULL != volume_for_path("/boot") && 0 != (ret = nandroid_restore_partition(backup_path, "/boot")))
        return ret;

    if (backup_recovery && 0 != (ret = nandroid_restore_partition(backup_path, "/recovery")))
        return ret;

    if (backup_efs == RESTORE_EFS_TAR && 0 != (ret = nandroid_restore_partition(backup_path, "/efs")))
        return ret;
        
    if (backup_modem == RAW_IMG_FILE && 0 != (ret = nandroid_restore_partition(backup_path, "/modem")))
        return ret;

    if (backup_system && 0 != (ret = nandroid_restore_partition(backup_path, "/system")))
        return ret;

    if (backup_data && 0 != (ret = nandroid_restore_partition(backup_path, "/data")))
        return ret;
        
    if (has_datadata()) {
        if (backup_data && 0 != (ret = nandroid_restore_partition(backup_path, "/datadata")))
            return ret;
    }

    // handle .android_secure on external and internal storage
    if (backup_data && android_secure_ext == 0) {
        // android_secure_ext == 0: restore to default /sdcard path
        if (is_data_media())
            ui_print("Skipping android_secure restore to /data/media.\n");
        else if (0 != (ret = nandroid_restore_partition_extended(backup_path, "/sdcard/.android_secure", 0)))
            return ret;
    }
    else if (backup_data && android_secure_ext == 1) {
        // android_secure_ext == 1: restore to second storage
        if (volume_for_path("/external_sd") != NULL)
            ret = nandroid_restore_partition_extended(backup_path, "/external_sd/.android_secure", 0);
        else if (volume_for_path("/emmc") != NULL)
            ret = nandroid_restore_partition_extended(backup_path, "/emmc/.android_secure", 0);
        else
            ui_print("Skipping android_secure restore: no secondary storage found!\n");

        if (ret != 0)
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (backup_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return ret;

    finish_nandroid_job();
    ui_print("\nTWRP Restore complete!\n");
    if (reboot_after_nandroid)
        reboot_main_system();
    return 0;
}
//------------------------ end twrp backup and restore functions

int nandroid_backup(const char* backup_path)
{
    nandroid_backup_bitfield = 0;
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    refresh_default_backup_handler();
    
    if (ensure_path_mounted(backup_path) != 0) {
        return print_and_error("Can't mount backup path.\n");
    }
    
    Volume* volume = volume_for_path(backup_path);
    if (NULL == volume)
        return print_and_error("Unable to find volume for backup path.\n");
    if (is_data_media_volume_path(volume->mount_point))
        volume = volume_for_path("/data");
    int ret;
    struct statfs s;
    if (NULL != volume) {
        if (0 != (ret = statfs(volume->mount_point, &s)))
            return print_and_error("Unable to stat backup path.\n");
        uint64_t bavail = s.f_bavail;
        uint64_t bsize = s.f_bsize;
        uint64_t sdcard_free = bavail * bsize;
        uint64_t sdcard_free_mb = sdcard_free / (uint64_t)(1024 * 1024);
        ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);
        if (sdcard_free_mb < 150)
            ui_print("There may not be enough free space to complete backup... continuing...\n");
    }
    char tmp[PATH_MAX];
    ensure_directory(backup_path);

    if (backup_boot && 0 != (ret = nandroid_backup_partition(backup_path, "/boot")))
        return ret;

    if (backup_recovery && 0 != (ret = nandroid_backup_partition(backup_path, "/recovery")))
        return ret;

    Volume *vol = volume_for_path("/wimax");
    if (backup_wimax && vol != NULL && 0 == stat(vol->device, &s))
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
    if (backup_efs) {
        //first backup in raw format, returns 0 on success (or if skipped), else 1
        strcpy(tmp, backup_path);
        if (0 != custom_backup_raw_handler(dirname(tmp), "/efs")) {
            ui_print("EFS raw image backup failed! Trying tar backup...\n");
        }
        //second backup in tar format
        ui_print("creating 2nd copy in tar...\n");
        if (0 != (ret = nandroid_backup_partition(backup_path, "/efs")))
            return ret;
    }
    
    if (backup_modem) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/modem")))
            return ret;
    }

    if (backup_system && 0 != (ret = nandroid_backup_partition(backup_path, "/system")))
        return ret;

    if (is_custom_backup && backup_preload) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/preload"))) {
            ui_print("Failed to backup /preload!\n");
            return ret;
        }
    }
    else if (!is_custom_backup
#ifdef PHILZ_TOUCH_RECOVERY
                && nandroid_add_preload
#endif
            )
    {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/preload"))) {
            ui_print("Failed to backup preload! Try to disable it.\n");
            ui_print("Skipping /preload...\n");
            //return ret;
        }
    }

    if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/data")))
        return ret;

    if (has_datadata()) {
        if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/datadata")))
            return ret;
    }

    // handle .android_secure on external and internal storage
    if (!is_custom_backup)
        android_secure_ext = get_android_secure_path();
    if (backup_data && android_secure_ext == 0) {
        // android_secure_ext == 0: assume default /sdcard path
        if (is_data_media())
            ui_print("Skipping android_secure backup from /data/media.\n");
        else if (0 != stat("/sdcard/.android_secure", &s))
            ui_print("No /sdcard/.android_secure found. Skipping backup of applications on external storage.\n");
        else if (0 != (ret = nandroid_backup_partition_extended(backup_path, "/sdcard/.android_secure", 0)))
            return ret;
    }
    else if (backup_data && android_secure_ext == 1) {
        // android_secure_ext == 1: look in second storage
        if (0 == stat("/external_sd/.android_secure", &s))
            ret = nandroid_backup_partition_extended(backup_path, "/external_sd/.android_secure", 0);
        else if (0 == stat("/emmc/.android_secure", &s))
            ret = nandroid_backup_partition_extended(backup_path, "/emmc/.android_secure", 0);
        else
            ui_print("No .android_secure found on second storage. Skipping backup of applications on external storage.\n");

        if (ret != 0)
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (backup_sdext) {
        vol = volume_for_path("/sd-ext");
        if (vol == NULL || 0 != stat(vol->device, &s))
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

#ifdef PHILZ_TOUCH_RECOVERY
    if (enable_md5sum)
#endif
    {
        ui_print("Generating md5 sum...\n");
        sprintf(tmp, "nandroid-md5.sh %s", backup_path);
        if (0 != (ret = __system(tmp))) {
            ui_print("Error while generating md5 sum!\n");
            return ret;
        }
    }

    sprintf(tmp, "chmod -R 777 %s ; chmod -R u+r,u+w,g+r,g+w,o+r,o+w /sdcard/clockworkmod ; chmod u+x,g+x,o+x /sdcard/clockworkmod/backup ; chmod u+x,g+x,o+x /sdcard/clockworkmod/blobs", backup_path);
    __system(tmp);

    finish_nandroid_job();
    ui_print("\nBackup complete!\n");
    if (reboot_after_nandroid)
        reboot_main_system();
    return 0;
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

    while (fgets(tmp, PATH_MAX, fp) != NULL) {
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

    while (fgets(tmp, PATH_MAX, fp) != NULL) {
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

    while (fgets(path, PATH_MAX, fp) != NULL) {
        if (callback)
            nandroid_callback(path);
    }

    return __pclose(fp);
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
    struct stat file_info;
    if (twrp_backup_mode || 0 != (ret = statfs(tmp, &file_info))) {
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
    }

    ensure_directory(mount_point);

    int callback = stat("/sdcard/clockworkmod/.hidenandroidprogress", &file_info) != 0;
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

    if (twrp_backup_mode)
    {
        if (0 != (ret = twrp_restore_wrapper(tmp, mount_point, callback))) {
            ui_print("Error while restoring %s!\n", mount_point);
            return ret;
        }
    }
    else
    {
        if (restore_handler == NULL)
            restore_handler = get_restore_handler(mount_point);
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
        ui_print("\n>> Restoring %s...\n", root);
        ui_print("Volume not found! Skipping restore of %s...\n", root);
        return 0;
    }

    // see if we need a raw restore (mtd)
    char tmp[PATH_MAX];
    if (strcmp(vol->fs_type, "mtd") == 0 ||
            strcmp(vol->fs_type, "bml") == 0 ||
            strcmp(vol->fs_type, "emmc") == 0)
    {
        ui_print("\n>> Restoring %s...\nUsing raw mode...\n", root);
        int ret;
        const char* name = basename(root);

        //fix partition could be formatted when no image to restore (exp: if md5 check disabled and empty backup folder)
        struct stat file_check;
        if (twrp_backup_mode)
            sprintf(tmp, "%s%s.%s.win", backup_path, root, vol->fs_type);
        else
            sprintf(tmp, "%s%s.img", backup_path, root);

        if (0 != stat(tmp, &file_check)) {
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
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();

    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path\n");
    
    char tmp[PATH_MAX];

#ifdef PHILZ_TOUCH_RECOVERY
    if (enable_md5sum)
#endif
    {
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

    struct stat s;
    Volume *vol = volume_for_path("/wimax");
    if (restore_wimax && vol != NULL && 0 == stat(vol->device, &s))
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
    if (backup_efs == RESTORE_EFS_TAR && 0 != (ret = nandroid_restore_partition(backup_path, "/efs")))
        return ret;
        
    if (backup_modem == RAW_IMG_FILE && 0 != (ret = nandroid_restore_partition(backup_path, "/modem")))
        return ret;

    if (restore_system && 0 != (ret = nandroid_restore_partition(backup_path, "/system")))
        return ret;

    if (is_custom_backup && backup_preload) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/preload"))) {
            ui_print("Failed to restore /preload!\n");
            return ret;
        }
    }
    else if (!is_custom_backup
#ifdef PHILZ_TOUCH_RECOVERY
                && nandroid_add_preload
#endif
            )
    {
        if (restore_system && 0 != (ret = nandroid_restore_partition(backup_path, "/preload"))) {
            ui_print("Failed to restore preload! Try to disable it.\n");
            ui_print("Skipping /preload...\n");
            //return ret;
        }
    }

    if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "/data")))
        return ret;
        
    if (has_datadata()) {
        if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "/datadata")))
            return ret;
    }

    // handle .android_secure on external and internal storage
    if (!is_custom_backup)
        android_secure_ext = get_android_secure_path();
    if (backup_data && android_secure_ext == 0) {
        // android_secure_ext == 0: restore to default /sdcard path
        if (is_data_media())
            ui_print("Skipping android_secure restore to /data/media.\n");
        else if (0 != (ret = nandroid_restore_partition_extended(backup_path, "/sdcard/.android_secure", 0)))
            return ret;
    }
    else if (backup_data && android_secure_ext == 1) {
        // android_secure_ext == 1: restore to second storage
        if (volume_for_path("/external_sd") != NULL)
            ret = nandroid_restore_partition_extended(backup_path, "/external_sd/.android_secure", 0);
        else if (volume_for_path("/emmc") != NULL)
            ret = nandroid_restore_partition_extended(backup_path, "/emmc/.android_secure", 0);
        else
            ui_print("Skipping android_secure restore: no secondary storage found!\n");

        if (ret != 0)
            return ret;
    }

    if (restore_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (restore_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return ret;

    finish_nandroid_job();
    ui_print("\nRestore complete!\n");
    if (reboot_after_nandroid)
        reboot_main_system();
    return 0;
}

int nandroid_usage()
{
    printf("Usage: nandroid backup\n");
    printf("Usage: nandroid restore <directory>\n");
    return 1;
}

int nandroid_main(int argc, char** argv)
{
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
    
    return nandroid_usage();
}
