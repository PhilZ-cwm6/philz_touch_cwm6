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
int twrp_backup_mode = 0;
int reboot_after_nandroid = 0;
int android_secure_ext = 0;
int nandroid_add_preload = 0;
int enable_md5sum = 1;
int show_nandroid_size_progress = 0;
int compression_value = TAR_GZ_FAST;

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
    if (is_data_media() && (backup_data || backup_data_media))
    {
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
        if (!confirm_selection("Low free space! Continue anyway?", "Yes - Continue Nandroid Job"))
            return -1;
    }

    return 0;
}

void check_restore_size(const char* backup_file_image, const char* backup_path)
{
    // refresh target partition size
    if (Get_Size_Via_statfs(backup_path) != 0) {
        Backup_Size = 0;
        return;
    }
    Before_Used_Size = Used_Size;

    char tmp[PATH_MAX];
    char *dir;
    char** files;
    int numFiles = 0;

    strcpy(tmp, backup_file_image);
    dir = dirname(tmp);
    strcpy(tmp, dir);
    strcat(tmp, "/");
    files = gather_files(tmp, "", &numFiles);

    if (strlen(backup_file_image) > strlen("win000") && strcmp(backup_file_image + strlen(backup_file_image) - strlen("win000"), "win000") == 0)
        snprintf(tmp, strlen(backup_file_image) - 3, "%s", backup_file_image);
    else
        strcpy(tmp, backup_file_image);

    int i;
    unsigned long fsize;
    for(i = 0; i < numFiles; i++) {
        if (strstr(files[i], basename(tmp)) != NULL) {
            fsize = Get_File_Size(files[i]);
            if (is_gzip_file(files[i]))
                fsize += (fsize * 40) / 100;
            Backup_Size += fsize;
        }
    }

    free_string_array(files);
}

void show_backup_stats(const char* backup_path) {
    long total_msec = gettime_now_msec() - nandroid_start_msec;
    int minutes = total_msec / 60000;
    int seconds = (total_msec % 60000) / 1000;

    unsigned long long final_size = Get_Folder_Size(backup_path);
    long double compression;
    if (Backup_Size == 0 || final_size == 0 || nandroid_get_default_backup_format() != NANDROID_BACKUP_FORMAT_TGZ)
        compression = 0;
    else compression = 1 - ((long double)(final_size) / (long double)(Backup_Size));

    ui_print("\nBackup complete!\n");
    ui_print("Backup time: %02i:%02i mn\n", minutes, seconds);
    ui_print("Backup size: %.2LfMb\n", (long double) final_size / 1048576);
    if (default_backup_handler != dedupe_compress_wrapper)
        ui_print("Compression: %.2Lf%%\n", compression * 100);
}

void show_restore_stats() {
    long total_msec = gettime_now_msec() - nandroid_start_msec;
    int minutes = total_msec / 60000;
    int seconds = (total_msec % 60000) / 1000;

    ui_print("\nRestore complete!\n");
    ui_print("Restore time: %02i:%02i mn\n", minutes, seconds);
}

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
    char tmp[PATH_MAX];
    sprintf(tmp, "%s", backup_path);
    char* image_file = basename(tmp);
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
    sprintf(tmp, "%s", backup_path);
    char *logfile = dirname(tmp);
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
    char tmp[PATH_MAX];
    int backup_count;
    ui_print("Breaking backup file into multiple archives...\nGenerating file lists\n");
    backup_count = Make_File_List(backup_path);
    if (backup_count < 1) {
        LOGE("Error generating file list!\n");
        return -1;
    }
    struct stat st;
    if (0 != stat("/tmp/list/filelist000", &st)) {
        sprintf(tmp, "%s", backup_path);
        ui_print("Nothing to backup. Skipping %s\n", basename(tmp));
        return 0;
    }

    unsigned long long total_bsize = 0, file_size = 0;
    int index;
    int nand_starts = 1;
    last_size_update = 0;
    for (index=0; index<backup_count; index++)
    {
        compute_twrp_backup_stats(index);
        if (nandroid_get_default_backup_format() == NANDROID_BACKUP_FORMAT_TAR)
            sprintf(tmp, "(tar -cvf '%s%03i' -T /tmp/list/filelist%03i) 2> /proc/self/fd/1 ; exit $?", backup_file_image, index, index);
        else
            sprintf(tmp, "(tar -cv -T /tmp/list/filelist%03i | pigz -c -%d >'%s%03i') 2> /proc/self/fd/1 ; exit $?", index, compression_value, backup_file_image, index);

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

    if (0 != Get_Size_Via_statfs(backup_path))
        return print_and_error("Unable to stat backup path.\n");


    if (check_backup_size(backup_path) < 0)
        return print_and_error("Not enough free space: backup cancelled.\n");

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    nandroid_start_msec = gettime_now_msec();
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

int twrp_tar_extract_wrapper(const char* popen_command, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    strcpy(tmp, popen_command);
    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar.\n");
        return -1;
    }

    int nand_starts = 1;
    last_size_update = 0;
    while (fgets(tmp, PATH_MAX, fp) != NULL) {
#ifdef PHILZ_TOUCH_RECOVERY
        if (user_cancel_nandroid(&fp, NULL, 0, &nand_starts))
            return -1;
#endif
        tmp[PATH_MAX - 1] = NULL;
        if (callback) {
            update_size_progress(backup_path);
            nandroid_callback(tmp);
        }
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

    check_restore_size(backup_file_image, backup_path);
    if (strlen(backup_file_image) > strlen("win000") && strcmp(backup_file_image + strlen(backup_file_image) - strlen("win000"), "win000") == 0) {
        // multiple volume archive detected
        char main_filename[PATH_MAX];
        memset(main_filename, 0, sizeof(main_filename));
        strncpy(main_filename, backup_file_image, strlen(backup_file_image) - strlen("000"));
        int index = 0;
        sprintf(tmp, "%s%03i", main_filename, index);
        while(file_found(tmp)) {
            ui_print("  * Restoring archive %d\n", index + 1);
            sprintf(cmd, "cd /; tar %s '%s'; exit $?", tar_args, tmp);
            if (0 != (ret = twrp_tar_extract_wrapper(cmd, backup_path, callback)))
                return ret;
            index++;
            sprintf(tmp, "%s%03i", main_filename, index);
        }
    } else {
        //single volume archive
        sprintf(cmd, "cd %s; tar %s '%s'; exit $?", backup_path, tar_args, backup_file_image);
        sprintf(tmp, "%s", backup_file_image);
        ui_print("Restoring archive %s\n", basename(tmp));
        ret = twrp_tar_extract_wrapper(cmd, backup_path, callback);
    }
    return ret;
}

int twrp_restore(const char* backup_path)
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

    finish_nandroid_job();
    show_restore_stats();
    if (reboot_after_nandroid)
        reboot_main_system(ANDROID_RB_RESTART, 0, 0);
    return 0;
}
//------------------------ end twrp backup and restore functions


// backup /data/media support
int nandroid_backup_datamedia(const char* backup_path)
{
    ui_print("\n>> Backing up /data/media...\n");
    if (is_data_media_volume_path(backup_path)) {
        // non fatal failure
        LOGE("  - can't backup folder to its self, skipping...\n");
        return 0;
    }

    if (0 != ensure_path_mounted("/data"))
        return -1;

    ensure_path_mounted("/sdcard");
    struct stat s;
    int callback = stat("/sdcard/clockworkmod/.hidenandroidprogress", &s) != 0;

    compute_directory_stats("/data/media");
    Volume *v = volume_for_path("/data");
    if (v == NULL)
        return -1;

    char backup_file_image[PATH_MAX];
    sprintf(backup_file_image, "%s/datamedia.%s", backup_path, v->fs_type == NULL ? "auto" : v->fs_type);

    char cmd[PATH_MAX];
    int fmt;
    fmt = nandroid_get_default_backup_format();
    if (fmt == NANDROID_BACKUP_FORMAT_TAR) {
        sprintf(cmd, "cd / ; touch %s.tar ; (tar cv data/media | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.) 2> /proc/self/fd/1 ; exit $?",
                backup_file_image, backup_file_image);
    }
    else if (fmt == NANDROID_BACKUP_FORMAT_TGZ) {
        sprintf(cmd, "cd / ; touch %s.tar.gz ; (tar cv data/media | pigz -c -%d | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.gz.) 2> /proc/self/fd/1 ; exit $?",
                backup_file_image, compression_value, backup_file_image);
    }
    else {
        // non fatal failure
        LOGE("  - backup format must be tar(.gz), skipping...\n");
        return 0;
    }

    int ret;
    ret = do_tar_compress(cmd, callback, backup_file_image);

    ensure_path_unmounted("/data");

    if (0 != ret)
        return print_and_error("Failed to backup /data/media!\n");

    ui_print("Backup of /data/media completed.\n");
    return 0;
}

int nandroid_restore_datamedia(const char* backup_path)
{
    ui_print("\n>> Restoring /data/media...\n");
    if (is_data_media_volume_path(backup_path)) {
        // non fatal failure
        LOGE("  - can't restore folder to its self, skipping...\n");
        return 0;
    }

    Volume *v = volume_for_path("/data");
    if (v == NULL)
        return -1;

    ensure_path_mounted("/sdcard");
    struct stat s;
    int callback = stat("/sdcard/clockworkmod/.hidenandroidprogress", &s) != 0;

    char backup_file_image[PATH_MAX];
    char cmd[PATH_MAX];
    const char *filesystems[] = { "yaffs2", "ext2", "ext3", "ext4", "vfat", "exfat", "rfs", "auto", NULL };
    char *filesystem = NULL;
    int i = 0;
    nandroid_restore_handler restore_handler = NULL;
    while ((filesystem = filesystems[i]) != NULL) {
        sprintf(backup_file_image, "%s/datamedia.%s.tar", backup_path, filesystem);
        if (0 == stat(backup_file_image, &s)) {
            restore_handler = tar_extract_wrapper;
            sprintf(cmd, "cd / ; cat %s* | tar xv ; exit $?", backup_file_image);
            break;
        }
        sprintf(backup_file_image, "%s/datamedia.%s.tar.gz", backup_path, filesystem);
        if (0 == stat(backup_file_image, &s)) {
            restore_handler = tar_gzip_extract_wrapper;
            sprintf(cmd, "cd / ; cat %s* | pigz -d -c | tar xv ; exit $?", backup_file_image);
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

    // data can be unmounted by format_unknown_device()
    if (0 != ensure_path_mounted("/data"))
        return -1;

    if (0 != do_tar_extract(cmd, backup_file_image, "/data", callback))
        return print_and_error("Failed to restore /data/media!\n");

    ui_print("Restore of /data/media completed.\n");
    return 0;
}
