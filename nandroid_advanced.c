// below code is included by nandroid.c
// make it easier to merge official cm changes

/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*    TWRP backup and restore support    */
/*                and                    */
/*    Custom backup and restore support  */
/*    Are parts of PhilZ Touch Recovery  */
/*****************************************/

//these general variables are needed to not break backup and restore by external script
int backup_boot = 1, backup_recovery = 1, backup_wimax = 1, backup_system = 1;
int backup_data = 1, backup_cache = 1, backup_sdext = 1;
int backup_preload = 0, backup_efs = 0, backup_misc = 0, backup_modem = 0, backup_radio = 0;
int backup_data_media = 0;
int is_custom_backup = 0;
int reboot_after_nandroid = 0;
int android_secure_ext = 0;


// resetting progress bar and background icon once backup/restore done or cancelled by user
void finish_nandroid_job() {
    ui_print("Finalizing, please wait...\n");
    sync();
#ifdef PHILZ_TOUCH_RECOVERY
    vibrate_device(1500);
    if (show_background_icon.value)
        ui_set_background(BACKGROUND_ICON_CLOCKWORK);
    else
#endif
        ui_set_background(BACKGROUND_ICON_NONE);

    ui_reset_progress();
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
    if (fclose(fp) != 0) {
        LOGE("Failed to close '%s'\n", file_archive);
        return -1;
    }

    char magic_num[2] = {0x1F, 0x8B};
    int i;
    for(i = 0; i < 2; i++) {
        if (buff[i] != magic_num[i])
            return 0;
    }
    return 1;
}

// calculate needed space fro backup and check if we have enough free space to compute operations
// call after a successful run of Get_Size_Via_statfs() to populate Total_Size, Used_Size and Free_Size
// code adapted mostly from TARP source (dees_troy at yahoo) for PhilZ Touch
// for these Can_Be_Mounted = true in twrp
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
            strcmp(vol->fs_type, "f2fs") == 0 ||
            strcmp(vol->fs_type, "auto") == 0)
        return 0; // true
    else
        return 1; //false
}

// for these Can_Be_Mounted = false in twrp
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


/*
- If Is_Image(), in twrp we call void TWPartition::Setup_Image(bool Display_Error) which sets Backup_Method = DD or FLASH_UTILS
  It also calls TWPartition::Find_Partition_Size(void) which we use in CWM to read /proc/partitions to get whole partition "Size"
  For these partitions, Backup_Size will be equal to total partition size as we use a raw backup mode
    * Used = Size;
    * Backup_Size = Size;
- If Is_File_System() - see above function -, We call void TWPartition::Setup_File_System(bool Display_Error) which
  will set partition as mountable (Can_Be_Mounted = true) and sets Backup_Method = FILES
- In TWRP, "Used" space for each partition is refreshed by void TWPartitionManager::Refresh_Sizes(void) which acalls
  TWPartitionManager::Update_System_Details(void) will call bool TWPartition::Update_Size(bool Display_Error) for each partition with Can_Be_Mounted == true
- Update_Size() will get size details with Get_Size_Via_statfs() or if it fails with Get_Size_Via_df()
- So, only not mountable partitions are using Find_Partition_Size()
*/
#define BASE_PARTITIONS_NUM   13
unsigned long long Backup_Size = 0;
unsigned long long Before_Used_Size = 0;
int check_backup_size(const char* backup_path) {
    // these are the size stats for backup_path we previously refreshed by calling Get_Size_Via_statfs()
    int total_mb = (int)(Total_Size / 1048576LLU);
    int used_mb = (int)(Used_Size / 1048576LLU);
    int free_mb = (int)(Free_Size / 1048576LLU);
    int free_percent = free_mb * 100 / total_mb;
    Before_Used_Size = Used_Size; // save Used_Size to refresh data written stats later
    Backup_Size = 0;

    // supported nandroid partitions
    char* Base_Partitions_List[BASE_PARTITIONS_NUM] = {
            "/recovery",
            BOOT_PARTITION_MOUNT_POINT,
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
            "/sd-ext"
    };

    int items_num = BASE_PARTITIONS_NUM + MAX_EXTRA_NANDROID_PARTITIONS + 1;
    char* Partitions_List[items_num];
    int i;
    for (i = 0; i < BASE_PARTITIONS_NUM; ++i) {
        Partitions_List[i] = Base_Partitions_List[i];
    }

    int extra_partitions_num = get_extra_partitions_state();
    for (i = 0; i < extra_partitions_num; ++i) {
        Partitions_List[BASE_PARTITIONS_NUM + i] = extra_partition[i].mount_point;
    }

    Partitions_List[BASE_PARTITIONS_NUM + extra_partitions_num] = NULL;

    int preload_status = 0;
    if ((is_custom_backup && backup_preload) || (!is_custom_backup && nandroid_add_preload.value))
        preload_status = 1;

    int Base_Partitions_Backup_Status[] = {
            backup_recovery,
            backup_boot,
            backup_wimax,
            backup_modem,
            backup_radio,
            backup_efs,
            backup_misc,
            backup_system,
            preload_status,
            backup_data,
            backup_data,
            backup_cache,
            backup_sdext,
    };

    LOGI("Checking needed space for backup '%s'\n", backup_path);
    // calculate needed space for backup
    // assume recovery and wimax always use a raw backup mode (Is_Image() = 0)
    char skipped_parts[1024] = "";
    int ret = 0;
    Volume* vol;

    for (i = 0; Partitions_List[i] != NULL; ++i) {
        if (i >= BASE_PARTITIONS_NUM) {
            if (!extra_partition[i - BASE_PARTITIONS_NUM].backup_state)
                continue;
        } else if (!Base_Partitions_Backup_Status[i]) {
            continue;
        }

        // size of /data will be calculated later for /data/media devices to subtract sdcard size from it
        if (strcmp(Partitions_List[i], "/data") == 0 && is_data_media())
            continue;

        // redundant but keep for compatibility:
        // has_datadata() does a volume_for_path() != NULL check
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
            // Get_Size_Via_statfs() will ensure vol->mount_point != NULL
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

    // handle special partitions and folders:
    // handle /data and /data/media partitions size for /data/media devices
    unsigned long long data_backup_size = 0;
    unsigned long long data_used_bytes = 0;
    unsigned long long data_media_size = 0;
    if (is_data_media() && (backup_data || backup_data_media)) {
        if (0 == ensure_path_mounted("/data") && 0 == Get_Size_Via_statfs("/data")) {
            data_media_size = Get_Folder_Size("/data/media");
            data_used_bytes = Get_Folder_Size("/data");
            data_backup_size = data_used_bytes - data_media_size;
            LOGI("/data: tot size=%lluMb, free=%lluMb, backup size=%lluMb, used=%lluMb, media=%lluMb\n",
                    Total_Size/1048576LLU, Free_Size/1048576LLU, data_backup_size/1048576LLU,
                    data_used_bytes/1048576LLU, data_media_size/1048576LLU);
        } else {
            if (backup_data) {
                strcat(skipped_parts, " - /data");
                ret++;
            }
            if (backup_data_media) {
                strcat(skipped_parts, " - /data/media");
                ret++;
            }
        }
    }

    if (backup_data)
        Backup_Size += data_backup_size;

    // check if we are also backing up /data/media
    // if backup_path is same as /data/media, ignore this as it will not be processed by nandroid_backup_datamedia()
    if (backup_data_media && !is_data_media_volume_path(backup_path)) {
        Backup_Size += data_media_size;
        LOGI("included /data/media size\n"); // debug
    }

    // .android_secure size calculation
    // set_android_secure_path() will mount tmp so no need to remount before calling Get_Folder_Size(tmp)
    char tmp[PATH_MAX];
    set_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        unsigned long long andsec_size;
        andsec_size = Get_Folder_Size(tmp);
        Backup_Size += andsec_size;
        LOGI("%s backup size=%lluMb\n", tmp, andsec_size / 1048576LLU); // debug
    }

    // check if we have the needed space
    int backup_size_mb = (int)(Backup_Size / 1048576LLU);
    ui_print("\n>> Free space: %dMb (%d%%)\n", free_mb, free_percent);
    ui_print(">> Needed space: %dMb\n", backup_size_mb);
    if (ret)
        ui_print(">> Unknown partitions size (%d):%s\n", ret, skipped_parts);

    // dedupe wrapper needs less space than actual backup size (incremental backups)
    // only check free space in Mb if we use tar or tar.gz as a default format
    // also, add extra 50 Mb for security measures
    if (free_percent < 3 || (default_backup_handler != dedupe_compress_wrapper && free_mb < backup_size_mb + 50)) {
        LOGW("Low space for backup!\n");
        if (!ui_is_initialized()) {
            // do not prompt when it is an "adb shell nandroid backup" command
            LOGW("\n>>> Backup could fail with I/O error!! <<<\n\n");
        } else if (nand_prompt_on_low_space.value && !confirm_selection("Low free space! Continue anyway?", "Yes - Continue Nandroid Job"))
            return -1;
    }

    return 0;
}

// check size of archive files to get total backed up data size
// find all backup image files of a given partition and increment Backup_Size
// Backup_Size is set to 0 at start of nandroid_restore() process so that we do not print size progress on
void check_restore_size(const char* backup_file_image, const char* backup_path) {
    // refresh target partition size
    if (Get_Size_Via_statfs(backup_path) != 0) {
        Backup_Size = 0;
        return;
    }
    Before_Used_Size = Used_Size;

    char tmp[PATH_MAX];
    char filename[PATH_MAX];
    char** files;
    int numFiles = 0;

    sprintf(tmp, "%s/", DirName(backup_file_image));
    files = gather_files(tmp, "", &numFiles);

    // if it's a twrp multi volume backup, ensure we remove trailing 000: strlen("000") = 3
    if (strlen(backup_file_image) > strlen("win000") && strcmp(backup_file_image + strlen(backup_file_image) - strlen("win000"), "win000") == 0)
        snprintf(tmp, strlen(backup_file_image) - 3, "%s", backup_file_image);
    else
        strcpy(tmp, backup_file_image);
    sprintf(filename, "%s", BaseName(tmp));
    
    int i;
    unsigned long fsize;
    for(i = 0; i < numFiles; i++) {
        if (strstr(files[i], filename) != NULL) {
            fsize = Get_File_Size(files[i]);
            // check if it is a compressed archive and increase size by 45%
            // this needs a better implementation to do later
            if (is_gzip_file(files[i]) > 0)
                fsize += (fsize * 45) / 100;
            Backup_Size += fsize;
        }
    }

    free_string_array(files);
}

// print backup stats summary at end of a backup
void show_backup_stats(const char* backup_path) {
    long long total_msec = timenow_msec() - nandroid_start_msec;
    long long minutes = total_msec / 60000LL;
    long long seconds = (total_msec % 60000LL) / 1000LL;

    unsigned long long final_size = Get_Folder_Size(backup_path);
    long double compression;
    if (Backup_Size == 0 || final_size == 0 || nandroid_get_default_backup_format() != NANDROID_BACKUP_FORMAT_TGZ)
        compression = 0;
    else compression = 1 - ((long double)(final_size) / (long double)(Backup_Size));

    ui_print("\nBackup complete!\n");
    ui_print("Backup time: %02lld:%02lld mn\n", minutes, seconds);
    ui_print("Backup size: %.2LfMb\n", (long double) final_size / 1048576);
    // print compression % only if it is a tar / tar.gz backup
    // keep also for tar to show it is 0% compression
    if (default_backup_handler != dedupe_compress_wrapper)
        ui_print("Compression: %.2Lf%%\n", compression * 100);
}

// show restore stats (only time for now)
void show_restore_stats() {
    long long total_msec = timenow_msec() - nandroid_start_msec;
    long long minutes = total_msec / 60000LL;
    long long seconds = (total_msec % 60000LL) / 1000LL;

    ui_print("\nRestore complete!\n");
    ui_print("Restore time: %02lld:%02lld mn\n", minutes, seconds);
}

// custom backup: raw backup through shell (ext4 raw backup not supported in backup_raw_partition())
// for efs partition
// for now called only from nandroid_backup()
// ret = 0 if success, else ret = 1
int dd_raw_backup_handler(const char* backup_path, const char* root) {
    Volume *vol = volume_for_path(root);
    ui_print("\n>> Backing up %s...\nUsing raw mode...\n", root);
    if (vol == NULL || vol->fs_type == NULL) {
        LOGE("volume not found! Skipping raw backup of %s\n", root);
        return 0;
    }

    int ret = 0;
    char tmp[PATH_MAX];
    char* device_mmcblk;
    if (strstr(vol->blk_device, "/dev/block/mmcblk") != NULL || strstr(vol->blk_device, "/dev/block/mtdblock") != NULL) {
        sprintf(tmp, "raw-backup.sh -b %s %s %s", backup_path, vol->blk_device, vol->mount_point);
    }
    else if (vol->blk_device2 != NULL &&
            (strstr(vol->blk_device2, "/dev/block/mmcblk") != NULL || strstr(vol->blk_device2, "/dev/block/mtdblock") != NULL)) {
        sprintf(tmp, "raw-backup.sh -b %s %s %s", backup_path, vol->blk_device2, vol->mount_point);
    }
    else if ((device_mmcblk = readlink_device_blk(root)) != NULL) {
        sprintf(tmp, "raw-backup.sh -b %s %s %s", backup_path, device_mmcblk, vol->mount_point);
        free(device_mmcblk);
    }
    else {
        LOGE("invalid device! Skipping raw backup of %s\n", root);
        return 0;
    }

    if (0 != (ret = __system(tmp)))
        LOGE("failed raw backup of %s...\n", root);

    //log
    //finish_nandroid_job();
    char logfile[PATH_MAX];
    sprintf(logfile, "%s/log.txt", backup_path);
    ui_print_custom_logtail(logfile, 3);
    return ret;
}

// custom raw restore handler
// used to restore efs in raw mode or modem.bin files
// for now, only called directly from outside functions (not from nandroid_restore())
// user selects an image file to restore, so backup_file_image path is already mounted
int dd_raw_restore_handler(const char* backup_file_image, const char* root) {
    ui_print("\n>> Restoring %s...\n", root);
    Volume *vol = volume_for_path(root);
    if (vol == NULL || vol->fs_type == NULL) {
        ui_print("volume not found! Skipping raw restore of %s...\n", root);
        return 0;
    }

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();

    // make sure we  have a valid image file name
    int i = 0;
    char errmsg[PATH_MAX];
    char tmp[PATH_MAX];
    char filename[PATH_MAX];
    const char *raw_image_format[] = { ".img", ".bin", NULL };

    sprintf(filename, "%s", BaseName(backup_file_image));
    while (raw_image_format[i] != NULL) {
        if (strlen(filename) > strlen(raw_image_format[i]) &&
                    strcmp(filename + strlen(filename) - strlen(raw_image_format[i]), raw_image_format[i]) == 0 &&
                    strncmp(filename, vol->mount_point + 1, strlen(vol->mount_point)-1) == 0) {
            break;
        }
        i++;
    }

    if (raw_image_format[i] == NULL) {
        sprintf(errmsg, "invalid image file! Failed to restore %s to %s\n", filename, root);
        return print_and_error(errmsg, NANDROID_ERROR_GENERAL);
    }

    //make sure file exists
    if (!file_found(backup_file_image)) {
        sprintf(errmsg, "%s not found. Skipping restore of %s\n", backup_file_image, root);
        return print_and_error(errmsg, NANDROID_ERROR_GENERAL);
    }

    //restore raw image
    int ret = 0;
    char* device_mmcblk;

    ui_print("Restoring %s to %s\n", filename, vol->mount_point);
    if (strstr(vol->blk_device, "/dev/block/mmcblk") != NULL || strstr(vol->blk_device, "/dev/block/mtdblock") != NULL) {
        sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_file_image, vol->blk_device, vol->mount_point);
    } else if (vol->blk_device2 != NULL &&
            (strstr(vol->blk_device2, "/dev/block/mmcblk") != NULL || strstr(vol->blk_device2, "/dev/block/mtdblock") != NULL)) {
        sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_file_image, vol->blk_device2, vol->mount_point);
    } else if ((device_mmcblk = readlink_device_blk(root)) != NULL) {
        sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_file_image, device_mmcblk, vol->mount_point);
        free(device_mmcblk);
    } else {
        sprintf(errmsg, "raw restore: no device found (%s)\n", root);
        return print_and_error(errmsg, NANDROID_ERROR_GENERAL);
    }

    ret = __system(tmp);
    if (0 != ret) {
        sprintf(errmsg, "failed raw restore of %s to %s\n", filename, root);
        print_and_error(errmsg, ret);
    } else {
        finish_nandroid_job();
    }

    sprintf(tmp, "%s/log.txt", DirName(backup_file_image));
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


/*
 emmc/mtd/bml fstypes are restored directly in nandroid_restore_partition()
 * all other fstypes, include yaffs2 and auto are restored using twrp_restore_wrapper(), called by nandroid_restore_partition_extended()
 * twrp_restore_wrapper() will always extract in tar with twrp_tar_extract_wrapper()
 * TWRP uses "Backup_Method = FILES" (= tar) in "TWPartition::Setup_File_System" called by the statement: " if(Is_File_System(Fstab_File_System))"
 * Is_File_System function below:
        bool TWPartition::Is_File_System(string File_System) {
            if (File_System == "ext2" ||
                File_System == "ext3" ||
                File_System == "ext4" ||
                File_System == "vfat" ||
                File_System == "ntfs" ||
                File_System == "yaffs2" ||
                File_System == "exfat" ||
                File_System == "auto")
                return true;
            else
                return false;
        }
 * All those file types will be backed-up / extracted using tar
 * CWM on the other side, uses a special external binary for yaffs2 fstype which outputs a .img file
   So this must be accounted for when dealing with both TWRP/CWM backup files
*/

#define MAX_ARCHIVE_SIZE 4294967296LLU
int Makelist_File_Count;
unsigned long long Makelist_Current_Size;

// called only for multi-volume backups to generate stats for progress bar
// file list was gathered from mounted partition: no need to mount before stat line
static void compute_twrp_backup_stats(int index) {
    char tmp[PATH_MAX];
    char line[PATH_MAX];
    struct stat info;
    int total = 0;
    sprintf(tmp, "/tmp/list/filelist%03i", index);
    FILE *fp = fopen(tmp, "rb");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            line[strlen(line) - 1] = '\0';
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

    // Skip /data/media
    if (is_data_media() && strlen(Path) >= 11 && strncmp(Path, "/data/media", 11) == 0)
        return 0;

    // Skip google cached music
    if (strstr(Path, "data/data/com.google.android.music/files") != NULL)
        return 0;

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
        if (is_data_media() && strlen(FileName) >= 11 && strncmp(FileName, "/data/media", 11) == 0)
            continue; // Skip /data/media
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

int Make_File_List(const char* backup_path) {
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

    if (!is_path_mounted(backup_path)) {
        LOGE("Unable to find mounted volume: '%s'\n", v->mount_point);
        return -1;
    }

    // Always use split format (simpler code) - Build lists of files to backup
    char tmp[PATH_MAX];
    int backup_count;
    ui_print("Breaking backup file into multiple archives...\nGenerating file lists\n");
    backup_count = Make_File_List(backup_path);
    if (backup_count < 1) {
        LOGE("Error generating file list!\n");
        return -1;
    }

    // check we are not backing up an empty volume as it would fail to restore (tar: short read)
    // check first if a filelist was generated. If not, ensure volume is 0 size. Else, it could be an error while 
    if (!file_found("/tmp/list/filelist000")) {
        ui_print("Nothing to backup. Skipping %s\n", BaseName(backup_path));
        return 0;
    }

    unsigned long long total_bsize = 0, file_size = 0;
    int index;
    int nand_starts = 1;
    last_size_update = 0;
    set_perf_mode(1);
    for (index = 0; index < backup_count; index++)
    {
        compute_twrp_backup_stats(index);
        // folder /data/media and google cached music are excluded from tar by Generate_File_Lists(...)
        if (nandroid_get_default_backup_format() == NANDROID_BACKUP_FORMAT_TAR)
            sprintf(tmp, "(tar -cpvf '%s%03i' -T /tmp/list/filelist%03i) 2> /proc/self/fd/1 ; exit $?", backup_file_image, index, index);
        else
            sprintf(tmp, "set -o pipefail ; (tar -cpv -T /tmp/list/filelist%03i | pigz -c -%d >'%s%03i') 2> /proc/self/fd/1 ; exit $?", index, compression_value.value, backup_file_image, index);

        ui_print("  * Backing up archive %i/%i\n", (index + 1), backup_count);
        FILE *fp = __popen(tmp, "r");
        if (fp == NULL) {
            LOGE("Unable to execute tar.\n");
            set_perf_mode(0);
            return -1;
        }

        while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
            if (user_cancel_nandroid(&fp, backup_file_image, 1, &nand_starts)) {
                set_perf_mode(0);
                return -1;
            }
#endif
            tmp[PATH_MAX - 1] = '\0';
            if (callback) {
                update_size_progress(backup_file_image);
                nandroid_callback(tmp);
            }
        }

#ifdef PHILZ_TOUCH_RECOVERY
        ui_print_preset_colors(0, NULL);
#endif
        if (0 != __pclose(fp)) {
            set_perf_mode(0);
            return -1;
        }

        sprintf(tmp, "%s%03i", backup_file_image, index);
        file_size = Get_File_Size(tmp);
        if (file_size == 0) {
            LOGE("Backup file size for '%s' is 0 bytes!\n", tmp);
            set_perf_mode(0);
            return -1;
        }
        total_bsize += file_size;
    }

    __system("cd /tmp && rm -rf list");
    set_perf_mode(0);
    ui_print("Total backup size:\n  %llu bytes.\n", total_bsize);
    return 0;
}

int twrp_backup(const char* backup_path) {
    // keep this for extra security and keep close to stock code
    // refresh_default_backup_handler() mounts /sdcard. We stat it in nandroid_backup_partition_extended() for callback
    nandroid_backup_bitfield = 0;
    refresh_default_backup_handler();

    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path.\n", NANDROID_ERROR_GENERAL);

    int ret;
    struct statfs s;

    // refresh size stats for backup_path
    // this will also ensure volume for backup path != NULL
    if (0 != Get_Size_Via_statfs(backup_path))
        return print_and_error("Unable to stat backup path.\n", NANDROID_ERROR_GENERAL);

    // estimate backup size and ensure we have enough free space available on backup_path
    if (check_backup_size(backup_path) < 0)
        return print_and_error("Not enough free space: backup cancelled.\n", NANDROID_ERROR_GENERAL);

    // moved after backup size check to fix pause before showing low space prompt
    // this is caused by friendly log view triggering on ui_set_background(BACKGROUND_ICON_INSTALLING) call
    // also, it is expected to have the background installing icon when we actually start backup
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    nandroid_start_msec = timenow_msec(); // starts backup monitoring timer for total backup time
#ifdef PHILZ_TOUCH_RECOVERY
    last_key_ev = nandroid_start_msec; // support dim screen timeout during nandroid operation
#endif

    char tmp[PATH_MAX];
    ensure_directory(backup_path, 0755);

    if (backup_boot && volume_for_path(BOOT_PARTITION_MOUNT_POINT) != NULL &&
            0 != (ret = nandroid_backup_partition(backup_path, BOOT_PARTITION_MOUNT_POINT)))
        return print_and_error(NULL, ret);

    if (backup_recovery && volume_for_path("/recovery") != NULL &&
            0 != (ret = nandroid_backup_partition(backup_path, "/recovery")))
        return print_and_error(NULL, ret);

#ifdef BOARD_USE_MTK_LAYOUT
    if ((backup_boot || backup_recovery) && volume_for_path("/uboot") != NULL &&
            0 != (ret = nandroid_backup_partition(backup_path, "/uboot")))
        return print_and_error(NULL, ret);
#endif

    Volume *vol = volume_for_path("/efs");
    if (backup_efs &&  NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/efs")))
            return print_and_error(NULL, ret);
    }

    vol = volume_for_path("/misc");
    if (backup_misc && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/misc")))
            return print_and_error(NULL, ret);
    }

    vol = volume_for_path("/modem");
    if (backup_modem && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/modem")))
            return print_and_error(NULL, ret);
    }

    vol = volume_for_path("/radio");
    if (backup_radio && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/radio")))
            return print_and_error(NULL, ret);
    }

    if (backup_system && 0 != (ret = nandroid_backup_partition(backup_path, "/system")))
        return print_and_error(NULL, ret);

    vol = volume_for_path("/preload");
    if (backup_preload && NULL != vol) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/preload")))
            return print_and_error(NULL, ret);
    }

    if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/data")))
        return print_and_error(NULL, ret);

    if (has_datadata()) {
        if (backup_data && 0 != (ret = nandroid_backup_partition(backup_path, "/datadata")))
            return print_and_error(NULL, ret);
    }

    // handle .android_secure on external and internal storage
    set_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, tmp, 0)))
            return print_and_error(NULL, ret);
    }

    if (backup_cache && 0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return print_and_error(NULL, ret);

    if (backup_sdext) {
        if (0 != ensure_path_mounted("/sd-ext")) {
            LOGI("No sd-ext found. Skipping backup of sd-ext.\n");
        } else if (0 != (ret = nandroid_backup_partition(backup_path, "/sd-ext"))) {
            return print_and_error(NULL, ret);
        }
    }

    // handle extra partitions
    int i;
    int extra_partitions_num = get_extra_partitions_state();
    for (i = 0; i < extra_partitions_num; ++i) {
        if (extra_partition[i].backup_state && 0 != (ret = nandroid_backup_partition(backup_path, extra_partition[i].mount_point)))
            return print_and_error(NULL, ret);
    }

    if (enable_md5sum.value) {
        if (0 != (ret = gen_twrp_md5sum(backup_path)))
            return print_and_error(NULL, ret);
    }

    sprintf(tmp, "chmod -R 777 %s", backup_path);
    __system(tmp);

    finish_nandroid_job();
    show_backup_stats(backup_path);
    if (reboot_after_nandroid)
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    return 0;
}

int twrp_tar_extract_wrapper(const char* popen_command, const char* backup_path, int callback) {
    char tmp[PATH_MAX];

    strcpy(tmp, popen_command);
    set_perf_mode(1);
    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar.\n");
        set_perf_mode(0);
        return -1;
    }

    int nand_starts = 1;
    last_size_update = 0;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, NULL, 0, &nand_starts)) {
            set_perf_mode(0);
            return -1;
        }
#endif
        tmp[PATH_MAX - 1] = '\0';
        if (callback) {
            update_size_progress(backup_path);
            nandroid_callback(tmp);
        }
    }

#ifdef PHILZ_TOUCH_RECOVERY
    ui_print_preset_colors(0, NULL);
#endif
    set_perf_mode(0);
    return __pclose(fp);
}

int twrp_restore_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char path[PATH_MAX];
    char cmd[PATH_MAX];
    char tar_args[10];
    int ret;

    // tar vs tar.gz format?
    if ((ret = is_gzip_file(backup_file_image)) < 0)
        return ret;
    if (ret == 0)
        sprintf(tar_args, "-xvpf");
    else
        sprintf(tar_args, "-xzvpf");

    check_restore_size(backup_file_image, backup_path);
    if (strlen(backup_file_image) > strlen("win000") && strcmp(backup_file_image + strlen(backup_file_image) - strlen("win000"), "win000") == 0) {
        // multiple volume archive detected
        char main_filename[PATH_MAX];
        memset(main_filename, 0, sizeof(main_filename));
        strncpy(main_filename, backup_file_image, strlen(backup_file_image) - strlen("000"));

        int index = 0;
        sprintf(path, "%s%03i", main_filename, index);
        while (file_found(path)) {
            ui_print("  * Restoring archive %d\n", index + 1);
            sprintf(cmd, "cd /; tar %s '%s'; exit $?", tar_args, path);
            if (0 != (ret = twrp_tar_extract_wrapper(cmd, backup_path, callback)))
                return ret;
            index++;
            sprintf(path, "%s%03i", main_filename, index);
        }
    } else {
        //single volume archive
        sprintf(cmd, "cd %s; tar %s '%s'; exit $?", backup_path, tar_args, backup_file_image);
        ui_print("Restoring archive %s\n", BaseName(backup_file_image));
        ret = twrp_tar_extract_wrapper(cmd, backup_path, callback);
    }
    return ret;
}

int twrp_restore(const char* backup_path) {
    Backup_Size = 0; // by default, do not calculate size

    // progress bar will be of indeterminate progress
    // setting nandroid_files_total = 0 will force this in nandroid_callback()
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    nandroid_files_total = 0;
    nandroid_start_msec = timenow_msec();
#ifdef PHILZ_TOUCH_RECOVERY
    // support dim screen timeout during nandroid operation
    last_key_ev = timenow_msec();
#endif
    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path\n", NANDROID_ERROR_GENERAL);

    char tmp[PATH_MAX];
    if (enable_md5sum.value) {
        if (0 != check_twrp_md5sum(backup_path))
            return print_and_error("MD5 mismatch!\n", NANDROID_ERROR_GENERAL);
    }

    ui_show_indeterminate_progress(); // call after verify_nandroid_md5sum() as it will reset the progress

    int ret;

    if (backup_boot && volume_for_path(BOOT_PARTITION_MOUNT_POINT) != NULL &&
            0 != (ret = nandroid_restore_partition(backup_path, BOOT_PARTITION_MOUNT_POINT)))
        return print_and_error(NULL, ret);

    if (backup_recovery && volume_for_path("/recovery") != NULL &&
            0 != (ret = nandroid_restore_partition(backup_path, "/recovery")))
        return print_and_error(NULL, ret);

#ifdef BOARD_USE_MTK_LAYOUT
    if ((backup_boot || backup_recovery) && volume_for_path("/uboot") != NULL &&
            0 != (ret = nandroid_restore_partition(backup_path, "/uboot")))
        return print_and_error(NULL, ret);
#endif

    Volume *vol = volume_for_path("/efs");
    if (backup_efs == RESTORE_EFS_TAR && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/efs")))
            return print_and_error(NULL, ret);
    }

    vol = volume_for_path("/misc");
    if (backup_misc && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/misc")))
            return print_and_error(NULL, ret);
    }

    vol = volume_for_path("/modem");
    if (backup_modem == RAW_IMG_FILE && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/modem")))
            return print_and_error(NULL, ret);
    }

    vol = volume_for_path("/radio");
    if (backup_radio == RAW_IMG_FILE && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/radio")))
            return print_and_error(NULL, ret);
    }

    if (backup_system && 0 != (ret = nandroid_restore_partition(backup_path, "/system")))
        return print_and_error(NULL, ret);

    vol = volume_for_path("/preload");
    if (backup_preload && vol != NULL) {
        if (0 != (ret = nandroid_restore_partition(backup_path, "/preload")))
            return print_and_error(NULL, ret);
    }

    if (backup_data && 0 != (ret = nandroid_restore_partition(backup_path, "/data")))
        return print_and_error(NULL, ret);
        
    if (has_datadata()) {
        if (backup_data && 0 != (ret = nandroid_restore_partition(backup_path, "/datadata")))
            return print_and_error(NULL, ret);
    }

    // handle .android_secure on external and internal storage
    set_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_restore_partition_extended(backup_path, tmp, 0)))
            return print_and_error(NULL, ret);
    }

    if (backup_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return print_and_error(NULL, ret);

    if (backup_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return print_and_error(NULL, ret);

    // handle extra partitions
    int i;
    int extra_partitions_num = get_extra_partitions_state();
    for (i = 0; i < extra_partitions_num; ++i) {
        if (extra_partition[i].backup_state && 0 != (ret = nandroid_restore_partition(backup_path, extra_partition[i].mount_point)))
            return print_and_error(NULL, ret);
    }

    finish_nandroid_job();
    show_restore_stats();
    if (reboot_after_nandroid)
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    return 0;
}
//------------------------ end twrp backup and restore functions


// backup /data/media support
// we reach here only if backup_data_media == 1
// backup_data_media can be set to 1 only in "custom backup and restore" menu AND if is_data_media() && !twrp_backup_mode.value
int nandroid_backup_datamedia(const char* backup_path) {
    char tmp[PATH_MAX];
    ui_print("\n>> Backing up /data/media...\n");
    if (is_data_media_volume_path(backup_path)) {
        // non fatal failure
        LOGE("  - can't backup folder to its self, skipping...\n");
        return 0;
    }

    if (0 != ensure_path_mounted("/data"))
        return -1;

    sprintf(tmp, "%s/%s", get_primary_storage_path(), NANDROID_HIDE_PROGRESS_FILE);
    ensure_path_mounted(tmp);
    int callback = !file_found(tmp);

    compute_directory_stats("/data/media");
    Volume *v = volume_for_path("/data");
    if (v == NULL)
        return -1;

    char backup_file_image[PATH_MAX];
    sprintf(backup_file_image, "%s/datamedia.%s", backup_path, v->fs_type == NULL ? "auto" : v->fs_type);

    int fmt;
    fmt = nandroid_get_default_backup_format();
    if (fmt == NANDROID_BACKUP_FORMAT_TAR) {
        sprintf(tmp, "cd / ; touch %s.tar ; set -o pipefail ; (tar -cpv data/media | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.) 2> /proc/self/fd/1 ; exit $?",
                backup_file_image, backup_file_image);
    } else if (fmt == NANDROID_BACKUP_FORMAT_TGZ) {
        sprintf(tmp, "cd / ; touch %s.tar.gz ; set -o pipefail ; (tar -cpv data/media | pigz -c -%d | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.gz.) 2> /proc/self/fd/1 ; exit $?",
                backup_file_image, compression_value.value, backup_file_image);
    } else {
        // non fatal failure
        LOGE("  - backup format must be tar(.gz), skipping...\n");
        return 0;
    }

    int ret;
    ret = do_tar_compress(tmp, callback, backup_file_image);

    ensure_path_unmounted("/data");

    if (0 != ret)
        return print_and_error("Failed to backup /data/media!\n", ret);

    ui_print("Backup of /data/media completed.\n");
    return 0;
}

int nandroid_restore_datamedia(const char* backup_path) {
    char tmp[PATH_MAX];
    ui_print("\n>> Restoring /data/media...\n");
    if (is_data_media_volume_path(backup_path)) {
        // non fatal failure
        LOGE("  - can't restore folder to its self, skipping...\n");
        return 0;
    }

    Volume *v = volume_for_path("/data");
    if (v == NULL)
        return -1;

    sprintf(tmp, "%s/%s", get_primary_storage_path(), NANDROID_HIDE_PROGRESS_FILE);
    ensure_path_mounted(tmp);
    int callback = !file_found(tmp);

    struct stat s;
    char backup_file_image[PATH_MAX];
    const char *filesystems[] = { "yaffs2", "ext2", "ext3", "ext4", "vfat", "exfat", "rfs", "f2fs", "auto", NULL };
    const char *filesystem;
    int i = 0;
    nandroid_restore_handler restore_handler = NULL;
    while ((filesystem = filesystems[i]) != NULL) {
        sprintf(backup_file_image, "%s/datamedia.%s.tar", backup_path, filesystem);
        if (0 == stat(backup_file_image, &s)) {
            restore_handler = tar_extract_wrapper;
            sprintf(tmp, "cd / ; set -o pipefail ; cat %s* | tar -xpv ; exit $?", backup_file_image);
            break;
        }
        sprintf(backup_file_image, "%s/datamedia.%s.tar.gz", backup_path, filesystem);
        if (0 == stat(backup_file_image, &s)) {
            restore_handler = tar_gzip_extract_wrapper;
            sprintf(tmp, "cd / ; set -o pipefail ; cat %s* | pigz -d -c | tar -xpv ; exit $?", backup_file_image);
            break;
        }
        i++;
    }

    if (filesystem == NULL || restore_handler == NULL) {
        LOGE("No backup found, skipping...\n");
        return 0;
    }

    if (0 != format_unknown_device(NULL, "/data/media", NULL))
        return print_and_error("Error while erasing /data/media\n", NANDROID_ERROR_GENERAL);

    // data can be unmounted by format_unknown_device()
    if (0 != ensure_path_mounted("/data"))
        return -1;

    if (0 != do_tar_extract(tmp, backup_file_image, "/data", callback))
        return print_and_error("Failed to restore /data/media!\n", NANDROID_ERROR_GENERAL);

    ui_print("Restore of /data/media completed.\n");
    return 0;
}

int gen_nandroid_md5sum(const char* backup_path) {
    char md5file[PATH_MAX];
    char** files;
    int ret = -1;
    int numFiles = 0;

    ui_print("\n>> Generating md5 sum...\n");
    ensure_path_mounted(backup_path);

    // this will exclude subfolders!
    set_gather_hidden_files(1);
    files = gather_files(backup_path, "", &numFiles);
    set_gather_hidden_files(0);
    if (numFiles == 0) {
        LOGE("No files found in backup path %s\n", backup_path);
        goto out;
    }

    // create empty md5file, overwrite existing one if we're regenerating the md5 for the backup
    sprintf(md5file, "%s/nandroid.md5", backup_path);
    write_string_to_file(md5file, "");

    int i = 0;
    for (i = 0; i < numFiles; i++) {
        // exclude md5 and log files
        if (strcmp(BaseName(files[i]), "nandroid.md5") == 0 || strcmp(BaseName(files[i]), "recovery.log") == 0)
            continue;

        ui_quick_reset_and_show_progress(1, 0);
        ui_print("  > %s\n", BaseName(files[i]));
        if (write_md5digest(files[i], md5file, 1) < 0)
            goto out;
    }

    ret = 0;

out:
    ui_reset_progress();
    free_string_array(files);
    if (ret != 0)
        LOGE("Error while generating md5 sum!\n");

    return ret;
}

int verify_nandroid_md5sum(const char* backup_path) {
    char* backupfile;
    char line[PATH_MAX];
    char md5file[PATH_MAX];

    ui_print("\n>> Checking MD5 sums...\n");
    ensure_path_mounted(backup_path);
    sprintf(md5file, "%s/nandroid.md5", backup_path);
    FILE *fp = fopen(md5file, "r");
    if (fp == NULL) {
        LOGE("cannot open md5file\n");
        return -1;
    }

    // unlike original cwm, an empty md5 file will fail check
    // also, if a file doesn't have and md5sum entry, it will fail
    while (fgets(line, sizeof(line), fp) != NULL) {
        backupfile = strstr(line, "  ");
        // skip empty new lines, but non other bad formatted lines
        if (backupfile == NULL && strcmp(line, "\n") != 0) {
            fclose(fp);
            return -1;
        }

        // mis-formatted line (backupfile must be at least one char as it includes the two spaces)
        if (strlen(backupfile) < 3 || strcmp(backupfile, "  \n") == 0) {
            fclose(fp);
            return -1;
        }

        // save the line before we modify it
        char *md5sum = strdup(line);
        if (md5sum == NULL) {
            LOGE("memory error\n");
            fclose(fp);
            return -1;
        }

        // remove leading two spaces and end new line
        backupfile += 2; // 2 == strlen("  ")
        if (strcmp(backupfile + strlen(backupfile) - 1, "\n") == 0)
            backupfile[strlen(backupfile) - 1] = '\0';

        // create a temporary md5file for each backup file
        sprintf(md5file, "/tmp/%s.md5", backupfile);
        write_string_to_file(md5file, md5sum);
        free(md5sum);
    }

    fclose(fp);

    // verify backup integrity for each backupfile to the md5sum we saved in temporary md5file
    int i = 0;
    int numFiles = 0;
    char** files;
    set_gather_hidden_files(1);
    files = gather_files(backup_path, "", &numFiles);
    set_gather_hidden_files(1);
    if (numFiles == 0) {
        free_string_array(files);
        return -1;
    }

    for(i = 0; i < numFiles; i++) {
        // exclude md5 and log files
        if (strcmp(BaseName(files[i]), "nandroid.md5") == 0 || strcmp(BaseName(files[i]), "recovery.log") == 0)
            continue;

        ui_quick_reset_and_show_progress(1, 0);
        sprintf(md5file, "/tmp/%s.md5", BaseName(files[i]));
        ui_print("  > %s\n", BaseName(files[i]));
        if (verify_md5digest(files[i], md5file) != 0) {
            free_string_array(files);
            ui_reset_progress();
            return -1;
        }
        delete_a_file(md5file);
    }

    ui_reset_progress();
    free_string_array(files);
    return 0;
}

int check_twrp_md5sum(const char* backup_path) {
    char md5file[PATH_MAX];
    char** files;
    int numFiles = 0;

    ui_print("\n>> Checking MD5 sums...\n");
    ensure_path_mounted(backup_path);
    files = gather_files(backup_path, "", &numFiles);
    if (numFiles == 0) {
        LOGE("No files found in %s\n", backup_path);
        free_string_array(files);
        return -1;
    }

    int i = 0;
    for(i = 0; i < numFiles; i++) {
        // exclude md5 files
        char *str = strstr(files[i], ".md5");
        if (str != NULL && strcmp(str, ".md5") == 0)
            continue;

        ui_quick_reset_and_show_progress(1, 0);
        ui_print("   - %s\n", BaseName(files[i]));
        sprintf(md5file, "%s.md5", files[i]);
        if (verify_md5digest(files[i], md5file) != 0) {
            LOGE("md5sum error!\n");
            ui_reset_progress();
            free_string_array(files);
            return -1;
        }
    }

    ui_print("MD5 sum ok.\n");
    ui_reset_progress();
    free_string_array(files);
    return 0;
}

int gen_twrp_md5sum(const char* backup_path) {
    char md5file[PATH_MAX];
    int numFiles = 0;

    ui_print("\n>> Generating md5 sum...\n");
    ensure_path_mounted(backup_path);

    // this will exclude subfolders!
    char** files = gather_files(backup_path, "", &numFiles);
    if (numFiles == 0) {
        LOGE("No files found in backup path %s\n", backup_path);
        free_string_array(files);
        return -1;
    }

    int i = 0;
    for(i = 0; i < numFiles; i++) {
        ui_quick_reset_and_show_progress(1, 0);
        ui_print("   - %s\n", BaseName(files[i]));
        sprintf(md5file, "%s.md5", files[i]);
        if (write_md5digest(files[i], md5file, 0) < 0) {
            LOGE("Error while generating md5sum!\n");
            ui_reset_progress();
            free_string_array(files);
            return -1;
        }
    }

    ui_print("MD5 sum created.\n");
    ui_reset_progress();
    free_string_array(files);
    return 0;
}
