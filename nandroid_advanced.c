// below code is included by nandroid.c
// make it easier to merge offcial cm changes

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

int backup_boot = 1, backup_recovery = 1, backup_wimax = 1, backup_system = 1;
int backup_data = 1, backup_cache = 1, backup_sdext = 1;
int backup_preload = 0, backup_efs = 0, backup_misc = 0, backup_modem = 0, backup_radio = 0;
int backup_data_media = 0;
int is_custom_backup = 0;
int reboot_after_nandroid = 0;
int android_secure_ext = 0;


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


unsigned long long Backup_Size = 0;
unsigned long long Before_Used_Size = 0;
int check_backup_size(const char* backup_path) {
    int total_mb = (int)(Total_Size / 1048576LLU);
    int used_mb = (int)(Used_Size / 1048576LLU);
    int free_mb = (int)(Free_Size / 1048576LLU);
    int free_percent = free_mb * 100 / total_mb;
    Before_Used_Size = Used_Size;
    Backup_Size = 0;

    // backable partitions
    static const char* Partitions_List[] = {
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
            "/sd-ext",
            EXTRA_PARTITIONS_PATH"1",
            EXTRA_PARTITIONS_PATH"2",
            EXTRA_PARTITIONS_PATH"3",
            EXTRA_PARTITIONS_PATH"4",
            EXTRA_PARTITIONS_PATH"5",
            NULL
    };

    int preload_status = 0;
    if ((is_custom_backup && backup_preload) || (!is_custom_backup && nandroid_add_preload.value))
        preload_status = 1;

    int backup_status[] = {
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
            extra_partition[0].backup_state,
            extra_partition[1].backup_state,
            extra_partition[2].backup_state,
            extra_partition[3].backup_state,
            extra_partition[4].backup_state
    };

    LOGI("Checking needed space for backup '%s'\n", backup_path);
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
    if (backup_data_media && !is_data_media_volume_path(backup_path)) {
        Backup_Size += data_media_size;
        LOGI("included /data/media size\n"); // debug
    }

    char tmp[PATH_MAX];
    set_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        unsigned long long andsec_size;
        andsec_size = Get_Folder_Size(tmp);
        Backup_Size += andsec_size;
        LOGI("%s backup size=%lluMb\n", tmp, andsec_size / 1048576LLU); // debug
    }

    int backup_size_mb = (int)(Backup_Size / 1048576LLU);
    ui_print("\n>> Free space: %dMb (%d%%)\n", free_mb, free_percent);
    ui_print(">> Needed space: %dMb\n", backup_size_mb);
    if (ret)
        ui_print(">> Unknown partitions size (%d):%s\n", ret, skipped_parts);

    if (free_percent < 3 || (default_backup_handler != dedupe_compress_wrapper && free_mb < backup_size_mb + 50)) {
        LOGW("Low space for backup!\n");
        if (nand_prompt_on_low_space.value && !confirm_selection("Low free space! Continue anyway?", "Yes - Continue Nandroid Job"))
            return -1;
    }

    return 0;
}

void check_restore_size(const char* backup_file_image, const char* backup_path) {
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
            if (is_gzip_file(files[i]) > 0)
                fsize += (fsize * 45) / 100;
            Backup_Size += fsize;
        }
    }

    free_string_array(files);
}

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
    if (default_backup_handler != dedupe_compress_wrapper)
        ui_print("Compression: %.2Lf%%\n", compression * 100);
}

void show_restore_stats() {
    long long total_msec = timenow_msec() - nandroid_start_msec;
    long long minutes = total_msec / 60000LL;
    long long seconds = (total_msec % 60000LL) / 1000LL;

    ui_print("\nRestore complete!\n");
    ui_print("Restore time: %02lld:%02lld mn\n", minutes, seconds);
}

int dd_raw_backup_handler(const char* backup_path, const char* root) {
    ui_set_background(BACKGROUND_ICON_INSTALLING);

    Volume *vol = volume_for_path(root);
    // make sure the volume exists...
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

int dd_raw_restore_handler(const char* backup_file_image, const char* root) {
    ui_set_background(BACKGROUND_ICON_INSTALLING);

    ui_print("\n>> Restoring %s...\n", root);
    Volume *vol = volume_for_path(root);
    // make sure the volume exists...
    if (vol == NULL || vol->fs_type == NULL) {
        LOGE("volume not found! Skipping raw restore of %s...\n", root);
        return 0;
    }

    // make sure we  have a valid image file name
    int i = 0;
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
        LOGE("invalid image file! Failed to restore %s to %s\n", filename, root);
        return -1;
    }

    //make sure file exists
    if (!file_found(backup_file_image)) {
        LOGE("%s not found. Skipping restore of %s\n", backup_file_image, root);
        return -1;
    }

    //restore raw image
    int ret = 0;
    char* device_mmcblk;

    ui_print("Restoring %s to %s\n", filename, vol->mount_point);
    if (strstr(vol->blk_device, "/dev/block/mmcblk") != NULL || strstr(vol->blk_device, "/dev/block/mtdblock") != NULL) {
        sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_file_image, vol->blk_device, vol->mount_point);
    }
    else if (vol->blk_device2 != NULL &&
            (strstr(vol->blk_device2, "/dev/block/mmcblk") != NULL || strstr(vol->blk_device2, "/dev/block/mtdblock") != NULL)) {
        sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_file_image, vol->blk_device2, vol->mount_point);
    }
    else if ((device_mmcblk = readlink_device_blk(root)) != NULL) {
        sprintf(tmp, "raw-backup.sh -r '%s' %s %s", backup_file_image, device_mmcblk, vol->mount_point);
        free(device_mmcblk);
    }
    else {
        LOGE("invalid device! Skipping raw restore of %s\n", root);
        return 0;
    }

    if (0 != (ret = __system(tmp)))
        LOGE("failed raw restore of %s to %s\n", filename, root);
    //log
    finish_nandroid_job();
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



#define MAX_ARCHIVE_SIZE 4294967296LLU
int Makelist_File_Count;
unsigned long long Makelist_Current_Size;

static void compute_twrp_backup_stats(int index) {
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

    const MountedVolume *mv = find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        ui_print("Unable to find mounted volume: %s\n", v->mount_point);
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

    if (!file_found("/tmp/list/filelist000")) {
        ui_print("Nothing to backup. Skipping %s\n", BaseName(backup_path));
        return 0;
    }

    unsigned long long total_bsize = 0, file_size = 0;
    int index;
    int nand_starts = 1;
    last_size_update = 0;
    set_perf_mode(1);
    for (index=0; index<backup_count; index++)
    {
        compute_twrp_backup_stats(index);
        if (nandroid_get_default_backup_format() == NANDROID_BACKUP_FORMAT_TAR)
#ifdef BOARD_RECOVERY_USE_BBTAR
            sprintf(tmp, "(tar -cvf '%s%03i' -T /tmp/list/filelist%03i) 2> /proc/self/fd/1 ; exit $?", backup_file_image, index, index);
#else
            sprintf(tmp, "(tar -cvsf '%s%03i' -T /tmp/list/filelist%03i) 2> /proc/self/fd/1 ; exit $?", backup_file_image, index, index);
#endif
        else
#ifdef BOARD_RECOVERY_USE_BBTAR
            sprintf(tmp, "set -o pipefail ; (tar -cv -T /tmp/list/filelist%03i | pigz -c -%d >'%s%03i') 2> /proc/self/fd/1 ; exit $?", index, compression_value.value, backup_file_image, index);
#else
            sprintf(tmp, "(set -o pipefail ; tar -cvs -T /tmp/list/filelist%03i | pigz -c -%d >'%s%03i') 2> /proc/self/fd/1 ; exit $?", index, compression_value.value, backup_file_image, index);
#endif
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
    nandroid_backup_bitfield = 0;
    refresh_default_backup_handler();

    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path.\n");

    int ret;
    struct statfs s;

    if (0 != Get_Size_Via_statfs(backup_path))
        return print_and_error("Unable to stat backup path.\n");


    if (check_backup_size(backup_path) < 0)
        return print_and_error("Not enough free space: backup cancelled.\n");

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    nandroid_start_msec = timenow_msec();
#ifdef PHILZ_TOUCH_RECOVERY
    last_key_ev = nandroid_start_msec;
#endif

    char tmp[PATH_MAX];
    ensure_directory(backup_path);

    if (backup_boot && volume_for_path(BOOT_PARTITION_MOUNT_POINT) != NULL &&
            0 != (ret = nandroid_backup_partition(backup_path, BOOT_PARTITION_MOUNT_POINT)))
        return ret;

    if (backup_recovery && volume_for_path("/recovery") != NULL &&
            0 != (ret = nandroid_backup_partition(backup_path, "/recovery")))
        return ret;

#ifdef BOARD_USE_MTK_LAYOUT
    if ((backup_boot || backup_recovery) && volume_for_path("/uboot") != NULL &&
            0 != (ret = nandroid_backup_partition(backup_path, "/uboot")))
        return ret;
#endif

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
    set_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, tmp, 0)))
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    vol = volume_for_path("/sd-ext");
    if (backup_sdext) {
        if (vol == NULL || 0 != statfs(vol->blk_device, &s))
            // could be we need ensure_path_mouned("/sd-ext") before this!
        {
            LOGI("No sd-ext found. Skipping backup of sd-ext.\n");
        }
        else
        {
            if (0 != ensure_path_mounted("/sd-ext"))
                LOGI("Could not mount sd-ext. sd-ext backup may not be supported on this device. Skipping backup of sd-ext.\n");
            else if (0 != (ret = nandroid_backup_partition(backup_path, "/sd-ext")))
                return ret;
        }
    }

    // handle extra partitions
    int i;
    for(i = 0; i < EXTRA_PARTITIONS_NUM; ++i) {
        sprintf(tmp, "%s%d", EXTRA_PARTITIONS_PATH, i+1);
        if (extra_partition[i].backup_state && 0 != (ret = nandroid_backup_partition(backup_path, tmp)))
            return ret;
    }

    if (enable_md5sum.value) {
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
#ifdef BOARD_RECOVERY_USE_BBTAR
        sprintf(tar_args, "-xvf");
#else
        sprintf(tar_args, "-xvsf");
#endif
    else
#ifdef BOARD_RECOVERY_USE_BBTAR
        sprintf(tar_args, "-xzvf");
#else
        sprintf(tar_args, "-xzvsf");
#endif

    check_restore_size(backup_file_image, backup_path);
    if (strlen(backup_file_image) > strlen("win000") && strcmp(backup_file_image + strlen(backup_file_image) - strlen("win000"), "win000") == 0) {
        // multiple volume archive detected
        char main_filename[PATH_MAX];
        memset(main_filename, 0, sizeof(main_filename));
        strncpy(main_filename, backup_file_image, strlen(backup_file_image) - strlen("000"));

        int index = 0;
        sprintf(path, "%s%03i", main_filename, index);
        while(file_found(path)) {
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
    Backup_Size = 0;
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    nandroid_files_total = 0;
    nandroid_start_msec = timenow_msec();
#ifdef PHILZ_TOUCH_RECOVERY
    last_key_ev = timenow_msec();
#endif
    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path\n");

    char tmp[PATH_MAX];
    if (enable_md5sum.value) {
        if (0 != check_twrp_md5sum(backup_path))
            return print_and_error("MD5 mismatch!\n");
    }

    int ret;

    if (backup_boot && volume_for_path(BOOT_PARTITION_MOUNT_POINT) != NULL &&
            0 != (ret = nandroid_restore_partition(backup_path, BOOT_PARTITION_MOUNT_POINT)))
        return ret;

    if (backup_recovery && volume_for_path("/recovery") != NULL &&
            0 != (ret = nandroid_restore_partition(backup_path, "/recovery")))
        return ret;

#ifdef BOARD_USE_MTK_LAYOUT
    if ((backup_boot || backup_recovery) && volume_for_path("/uboot") != NULL &&
            0 != (ret = nandroid_restore_partition(backup_path, "/uboot")))
        return ret;
#endif

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
    set_android_secure_path(tmp);
    if (backup_data && android_secure_ext) {
        if (0 != (ret = nandroid_restore_partition_extended(backup_path, tmp, 0)))
            return ret;
    }

    if (backup_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (backup_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return ret;

    // handle extra partitions
    int i;
    for(i = 0; i < EXTRA_PARTITIONS_NUM; ++i) {
        sprintf(tmp, "%s%d", EXTRA_PARTITIONS_PATH, i+1);
        if (extra_partition[i].backup_state && 0 != (ret = nandroid_restore_partition(backup_path, tmp)))
            return ret;
    }

    finish_nandroid_job();
    show_restore_stats();
    if (reboot_after_nandroid)
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    return 0;
}
//------------------------ end twrp backup and restore functions


// backup /data/media support
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
#ifdef BOARD_RECOVERY_USE_BBTAR
        sprintf(tmp, "cd / ; touch %s.tar ; set -o pipefail ; (tar cv data/media | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.) 2> /proc/self/fd/1 ; exit $?",
                backup_file_image, backup_file_image);
#else
        sprintf(tmp, "cd / ; touch %s.tar ; set -o pipefail ; (tar -csv data/media | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.) 2> /proc/self/fd/1 ; exit $?",
                backup_file_image, backup_file_image);
#endif
    } else if (fmt == NANDROID_BACKUP_FORMAT_TGZ) {
#ifdef BOARD_RECOVERY_USE_BBTAR
        sprintf(tmp, "cd / ; touch %s.tar.gz ; set -o pipefail ; (tar cv data/media | pigz -c -%d | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.gz.) 2> /proc/self/fd/1 ; exit $?",
                backup_file_image, compression_value.value, backup_file_image);
#else
        sprintf(tmp, "cd / ; touch %s.tar.gz ; set -o pipefail ; (tar -csv data/media | pigz -c -%d | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.gz.) 2> /proc/self/fd/1 ; exit $?",
                backup_file_image, compression_value.value, backup_file_image);
#endif
    } else {
        // non fatal failure
        LOGE("  - backup format must be tar(.gz), skipping...\n");
        return 0;
    }

    int ret;
    ret = do_tar_compress(tmp, callback, backup_file_image);

    ensure_path_unmounted("/data");

    if (0 != ret)
        return print_and_error("Failed to backup /data/media!\n");

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
#ifdef BOARD_RECOVERY_USE_BBTAR
            sprintf(tmp, "cd / ; set -o pipefail ; cat %s* | tar xv ; exit $?", backup_file_image);
#else
            sprintf(tmp, "cd / ; set -o pipefail ; cat %s* | tar -xsv ; exit $?", backup_file_image);
#endif
            break;
        }
        sprintf(backup_file_image, "%s/datamedia.%s.tar.gz", backup_path, filesystem);
        if (0 == stat(backup_file_image, &s)) {
            restore_handler = tar_gzip_extract_wrapper;
#ifdef BOARD_RECOVERY_USE_BBTAR
            sprintf(tmp, "cd / ; set -o pipefail ; cat %s* | pigz -d -c | tar xv ; exit $?", backup_file_image);
#else
            sprintf(tmp, "cd / ; set -o pipefail ; cat %s* | pigz -d -c | tar -xsv ; exit $?", backup_file_image);
#endif
            break;
        }
        i++;
    }

    if (filesystem == NULL || restore_handler == NULL) {
        LOGE("No backup found, skipping...\n");
        return 0;
    }

    if (0 != format_unknown_device(NULL, "/data/media", NULL))
        return print_and_error("Error while erasing /data/media\n");

    if (0 != ensure_path_mounted("/data"))
        return -1;

    if (0 != do_tar_extract(tmp, backup_file_image, "/data", callback))
        return print_and_error("Failed to restore /data/media!\n");

    ui_print("Restore of /data/media completed.\n");
    return 0;
}

int gen_nandroid_md5sum(const char* backup_path) {
    char md5file[PATH_MAX];
    int ret = -1;
    int numFiles = 0;

    ui_print("\n>> Generating md5 sum...\n");
    ensure_path_mounted(backup_path);
    ui_reset_progress();
    ui_show_progress(1, 0);

    // this will exclude subfolders!
    char** files = gather_files(backup_path, "", &numFiles);
    if (numFiles == 0) {
        LOGE("No files found in backup path %s\n", backup_path);
        goto out;
    }

    // create empty md5file, overwrite existing one if we're regenerating the md5 for the backup
    sprintf(md5file, "%s/nandroid.md5", backup_path);
    write_string_to_file(md5file, "");

    int i = 0;
    for(i = 0; i < numFiles; i++) {
        // exclude md5 and log files
        if (strcmp(BaseName(files[i]), "nandroid.md5") == 0 || strcmp(BaseName(files[i]), "recovery.log") == 0)
            continue;
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
    char** files = gather_files(backup_path, "", &numFiles);
    if (numFiles == 0) {
        free_string_array(files);
        return -1;
    }

    ui_reset_progress();
    ui_show_progress(1, 0);
    for(i = 0; i < numFiles; i++) {
        // exclude md5 and log files
        if (strcmp(BaseName(files[i]), "nandroid.md5") == 0 || strcmp(BaseName(files[i]), "recovery.log") == 0)
            continue;
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
