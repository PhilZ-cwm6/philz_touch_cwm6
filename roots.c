/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include "mtdutils/mtdutils.h"
#include "mounts.h"
#include "roots.h"
#include "common.h"
#include "make_ext4fs.h"

#include <fs_mgr.h>
#include <libgen.h>
#include "flashutils/flashutils.h"
#include "extendedcommands.h"

#include "voldclient/voldclient.h"

#include "cutils/properties.h"

static struct fstab *fstab = NULL;

int get_num_volumes() {
    return fstab->num_entries;
}

Volume* get_device_volumes() {
    return fstab->recs;
}

void load_volume_table() {
    int i;
    int ret;

    fstab = fs_mgr_read_fstab("/etc/recovery.fstab");
    if (!fstab) {
        LOGE("failed to read /etc/recovery.fstab\n");
        return;
    }

    ret = fs_mgr_add_entry(fstab, "/tmp", "ramdisk", "ramdisk", 0);
    if (ret < 0 ) {
        LOGE("failed to add /tmp entry to fstab\n");
        fs_mgr_free_fstab(fstab);
        fstab = NULL;
        return;
    }

    // Process vold-managed volumes with mount point "auto"
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];
        if (fs_mgr_is_voldmanaged(v) && strcmp(v->mount_point, "auto") == 0) {
            char mount[PATH_MAX];

            // Set the mount point to /storage/label which as used by vold
            snprintf(mount, PATH_MAX, "/storage/%s", v->label);
            free(v->mount_point);
            v->mount_point = strdup(mount);
        }
    }

    fprintf(stderr, "recovery filesystem table\n");
    fprintf(stderr, "=========================\n");
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];
        fprintf(stderr, "  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
               v->blk_device, v->length);
    }
    fprintf(stderr, "\n");
}

Volume* volume_for_path(const char* path) {
    return fs_mgr_get_entry_for_mount_point(fstab, path);
}

int is_primary_storage_voldmanaged() {
    Volume* v;
    v = volume_for_path("/storage/sdcard0");
    return fs_mgr_is_voldmanaged(v);
}

static char* primary_storage_path = NULL;
char* get_primary_storage_path() {
    if (primary_storage_path == NULL) {
        if (volume_for_path("/storage/sdcard0"))
            primary_storage_path = "/storage/sdcard0";
        else
            primary_storage_path = "/sdcard";
    }
    return primary_storage_path;
}

int get_num_extra_volumes() {
    int num = 0;
    int i;
    for (i = 0; i < get_num_volumes(); i++) {
        Volume* v = get_device_volumes() + i;
        if ((strcmp("/external_sd", v->mount_point) == 0) ||
                ((strcmp(get_primary_storage_path(), v->mount_point) != 0) &&
                fs_mgr_is_voldmanaged(v) && vold_is_volume_available(v->mount_point)))
            num++;
    }
    return num;
}

char** get_extra_storage_paths() {
    int i = 0, j = 0;
    static char* paths[MAX_NUM_MANAGED_VOLUMES];
    int num_extra_volumes = get_num_extra_volumes();

    if (num_extra_volumes == 0)
        return NULL;

    for (i = 0; i < get_num_volumes(); i++) {
        Volume* v = get_device_volumes() + i;
        if ((strcmp("/external_sd", v->mount_point) == 0) ||
                ((strcmp(get_primary_storage_path(), v->mount_point) != 0) &&
                fs_mgr_is_voldmanaged(v) && vold_is_volume_available(v->mount_point))) {
            paths[j] = v->mount_point;
            j++;
        }
    }
    paths[j] = NULL;

    return paths;
}

static char* android_secure_path = NULL;
char* get_android_secure_path() {
    if (android_secure_path == NULL) {
        char tmp[PATH_MAX];
        char** extra_paths = get_extra_storage_paths();
        int num_extra_volumes = get_num_extra_volumes();
        int i;
        struct stat st;
        for (i = 0; i < num_extra_volumes; i++) {
            sprintf(tmp, "%s/.android_secure", extra_paths[i]);
            const MountedVolume* mv =
                find_mounted_volume_by_mount_point(extra_paths[i]);
            if (ensure_path_mounted(extra_paths[i]) == 0) {
                if (0 == lstat(tmp, &st)) {
                    android_secure_path = malloc(strlen(tmp)+1);
                    sprintf(android_secure_path, "%s/.android_secure", extra_paths[i]);
                    break;
                }
                else if(!mv) { // volume is already mounted
                    ensure_path_unmounted(extra_paths[i]);
                }
            }
        }
        if (android_secure_path == NULL) {
            android_secure_path = malloc(sizeof("/.android_secure") + strlen(get_primary_storage_path()) + 1);
            sprintf(android_secure_path, "%s/.android_secure", primary_storage_path);
        }
    }
    return android_secure_path;
}

int try_mount(const char* device, const char* mount_point, const char* fs_type, const char* fs_options) {
    if (device == NULL || mount_point == NULL || fs_type == NULL)
        return -1;
    int ret = 0;
    if (fs_options == NULL) {
        ret = mount(device, mount_point, fs_type,
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
    }
    else {
        char mount_cmd[PATH_MAX];
        sprintf(mount_cmd, "mount -t %s -o%s %s %s", fs_type, fs_options, device, mount_point);
        ret = __system(mount_cmd);
    }
    if (ret == 0)
        return 0;
    LOGW("failed to mount %s (%s)\n", device, strerror(errno));
    return ret;
}

int is_data_media() {
    int i;
    int has_sdcard = 0;
    for (i = 0; i < get_num_volumes(); i++) {
        Volume* vol = get_device_volumes() + i;
        if (strcmp(vol->fs_type, "datamedia") == 0)
            return 1;
        if (strcmp(vol->mount_point, "/sdcard") == 0)
            has_sdcard = 1;
        if (fs_mgr_is_voldmanaged(vol) &&
                (strcmp(vol->mount_point, "/storage/sdcard0") == 0))
            has_sdcard = 1;
    }
    return !has_sdcard;
}

void setup_data_media() {
    int i;
    char* mount_point = "/sdcard";
    for (i = 0; i < get_num_volumes(); i++) {
        Volume* vol = get_device_volumes() + i;
        if (strcmp(vol->fs_type, "datamedia") == 0) {
            mount_point = vol->mount_point;
            break;
        }
    }
    // support /data/media/0
    char path[15];
    if (use_migrated_storage())
        sprintf(path, "/data/media/0");
    else sprintf(path, "/data/media");

    rmdir(mount_point);
    mkdir(path, 0755);
    symlink(path, mount_point);
}

int is_data_media_volume_path(const char* path) {
    Volume* v = volume_for_path(path);
    if (v != NULL)
        return strcmp(v->fs_type, "datamedia") == 0;

    if (!is_data_media()) {
        return 0;
    }

    return strcmp(path, "/sdcard") == 0 || path == strstr(path, "/sdcard/");
}

int ensure_path_mounted(const char* path) {
    return ensure_path_mounted_at_mount_point(path, NULL);
}

int ensure_path_mounted_at_mount_point(const char* path, const char* mount_point) {
    if (is_data_media_volume_path(path)) {
        if (ui_should_log_stdout()) {
            if (use_migrated_storage())
			    LOGI("using /data/media/0 for %s.\n", path);
		    else LOGI("using /data/media for %s.\n", path);
        }
        int ret;
        if (0 != (ret = ensure_path_mounted("/data")))
            return ret;
        setup_data_media();
        return 0;
    }
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted.
        return 0;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    if (NULL == mount_point)
        mount_point = v->mount_point;

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(mount_point);
    if (mv) {
        // volume is already mounted
        return 0;
    }

    mkdir(mount_point, 0755);  // in case it doesn't already exist

    if (fs_mgr_is_voldmanaged(v)) {
        return vold_mount_volume(mount_point, 1) == CommandOkay ? 0 : -1;

    } else if (strcmp(v->fs_type, "yaffs2") == 0) {
        // mount an MTD partition as a YAFFS2 filesystem.
        mtd_scan_partitions();
        const MtdPartition* partition;
        partition = mtd_find_partition_by_name(v->blk_device);
        if (partition == NULL) {
            LOGE("failed to find \"%s\" partition to mount at \"%s\"\n",
                 v->blk_device, mount_point);
            return -1;
        }
        return mtd_mount_partition(partition, mount_point, v->fs_type, 0);
    } else if (strcmp(v->fs_type, "ext4") == 0 ||
               strcmp(v->fs_type, "ext3") == 0 ||
               strcmp(v->fs_type, "rfs") == 0 ||
               strcmp(v->fs_type, "vfat") == 0) {
        if ((result = try_mount(v->blk_device, mount_point, v->fs_type, v->fs_options)) == 0)
            return 0;
        if ((result = try_mount(v->blk_device, mount_point, v->fs_type2, v->fs_options2)) == 0)
            return 0;
        if ((result = try_mount(v->blk_device2, mount_point, v->fs_type2, v->fs_options2)) == 0)
            return 0;
        return result;
    } else {
        // let's try mounting with the mount binary and hope for the best.
        char mount_cmd[PATH_MAX];
        sprintf(mount_cmd, "mount %s", mount_point);
        return __system(mount_cmd);
    }

    return -1;
}

static int ignore_data_media = 0;

int ensure_path_unmounted(const char* path) {
    // if we are using /data/media, do not ever unmount volumes /data or /sdcard
    if (is_data_media_volume_path(path)) {
        return ensure_path_unmounted("/data");
    }
    if (strstr(path, "/data") == path && is_data_media() && !ignore_data_media) {
        return 0;
    }

    Volume* v = volume_for_path(path);
    if (v == NULL) {
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }

    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted; you can't unmount it.
        return -1;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        // volume is already unmounted
        return 0;
    }

    if (fs_mgr_is_voldmanaged(volume_for_path(v->mount_point)))
        return vold_unmount_volume(v->mount_point, 0, 1) == CommandOkay ? 0 : -1;

    return unmount_mounted_volume(mv);
}

extern struct selabel_handle *sehandle;

int format_volume(const char* volume) {
    if (is_data_media_volume_path(volume)) {
        return format_unknown_device(NULL, volume, NULL);
    }
    // check to see if /data is being formatted, and if it is /data/media
    // Note: the /sdcard check is redundant probably, just being safe.
    if (strstr(volume, "/data") == volume && is_data_media() && !ignore_data_media) {
        return format_unknown_device(NULL, volume, NULL);
    }

    Volume* v = volume_for_path(volume);
    if (v == NULL) {
        // silent failure for sd-ext
        if (strcmp(volume, "/sd-ext") != 0)
            LOGE("unknown volume '%s'\n", volume);
        return -1;
    }
    // silent failure to format non existing sd-ext when defined in recovery.fstab
    if (strcmp(volume, "/sd-ext") == 0) {
        struct stat s;
        if (0 != stat(v->blk_device, &s)) {
            LOGI("Skipping format of sd-ext\n");
            return -1;
        }
    }

    // Only use vold format for exact matches otherwise /sdcard will be
    // formatted instead of /storage/sdcard0/.android_secure
    if (fs_mgr_is_voldmanaged(v) && strcmp(volume, v->mount_point) == 0) {
        if (ensure_path_unmounted(volume) != 0) {
            LOGE("format_volume failed to unmount %s", v->mount_point);
        }
        return vold_format_volume(v->mount_point, 1) == CommandOkay ? 0 : -1;
    }

    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", volume);
        return -1;
    }
    if (strcmp(v->mount_point, volume) != 0) {
#if 0
        LOGE("can't give path \"%s\" to format_volume\n", volume);
        return -1;
#endif
        return format_unknown_device(v->blk_device, volume, NULL);
    }

    if (ensure_path_unmounted(volume) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }

    if (strcmp(v->fs_type, "yaffs2") == 0 || strcmp(v->fs_type, "mtd") == 0) {
        mtd_scan_partitions();
        const MtdPartition* partition = mtd_find_partition_by_name(v->blk_device);
        if (partition == NULL) {
            LOGE("format_volume: no MTD partition \"%s\"\n", v->blk_device);
            return -1;
        }

        MtdWriteContext *write = mtd_write_partition(partition);
        if (write == NULL) {
            LOGW("format_volume: can't open MTD \"%s\"\n", v->blk_device);
            return -1;
        } else if (mtd_erase_blocks(write, -1) == (off_t) -1) {
            LOGW("format_volume: can't erase MTD \"%s\"\n", v->blk_device);
            mtd_write_close(write);
            return -1;
        } else if (mtd_write_close(write)) {
            LOGW("format_volume: can't close MTD \"%s\"\n", v->blk_device);
            return -1;
        }
        return 0;
    }

    if (strcmp(v->fs_type, "ext4") == 0) {
#ifdef USE_MKE2FS_FORMAT
		char ext4_cmd[PATH_MAX];
		sprintf(ext4_cmd, "/sbin/mke2fs -T ext4 -b 4096 -m 0 -F %s", v->blk_device);
        int result = __system(ext4_cmd);
#else
        int result = make_ext4fs(v->blk_device, v->length, volume, sehandle);
#endif
        if (result != 0) {
            LOGE("format_volume: format ext4 fs failed on %s\n", v->blk_device);
            return -1;
        }
#ifdef USE_MKE2FS_FORMAT
#ifdef NEED_SELINUX_FIX
        if (0 == strcmp(volume, "/data") ||
            0 == strcmp(volume, "/system") ||
            0 == strcmp(volume, "/cache"))
        {
            ensure_path_mounted(volume);
            char tmp[PATH_MAX];
            sprintf(tmp, "%s/lost+found", volume);
            if (selinux_android_restorecon(tmp) < 0 || selinux_android_restorecon(volume) < 0) {
                LOGW("restorecon: error restoring %s context\n",volume);
                //return -1;
            }
            ensure_path_unmounted(volume);
        }
#endif
#endif
        return 0;
    }

#ifdef USE_F2FS
    if (strcmp(v->fs_type, "f2fs") == 0) {
        const char* args[] = { "mkfs.f2fs", v->blk_device };
        int result = make_f2fs_main(2, (char**)args);
        if (result != 0) {
            LOGE("format_volume: mkfs.f2fs failed on %s\n", v->blk_device);
            return -1;
        }
        return 0;
    }
#endif

#if 0
    LOGE("format_volume: fs_type \"%s\" unsupported\n", v->fs_type);
    return -1;
#endif
    return format_unknown_device(v->blk_device, volume, v->fs_type);
}

void ignore_data_media_workaround(int ignore) {
  ignore_data_media = ignore;
}

void setup_legacy_storage_paths() {
    char* primary_path = get_primary_storage_path();

    if (!is_data_media_volume_path(primary_path)) {
        rmdir("/sdcard");
        symlink(primary_path, "/sdcard");
    }
}

/**********************************/
/*       Start file parser        */
/*    Original source by PhilZ    */
/**********************************/
// todo: parse settings file in one pass and make pairs of key:value
// get value of key from a given config file
int read_config_file(const char* config_file, const char *key, char *value, const char *value_def) {
    int ret = 0;
    char line[PROPERTY_VALUE_MAX];
    ensure_path_mounted(config_file);
    FILE *fp = fopen(config_file, "rb");
    if (fp != NULL) {
        while(fgets(line, sizeof(line), fp) != NULL) {
            if (strstr(line, key) != NULL && strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
                strcpy(value, strstr(line, "=") + 1);
                //remove trailing \n char
                if (value[strlen(value)-1] == '\n')
                    value[strlen(value)-1] = '\0';
                if (value[0] != '\0') {
                    fclose(fp);
                    LOGI("%s=%s\n", key, value);
                    return ret;
                }
            }
        }
        ret = 1;
        fclose(fp);
    } else {
        LOGI("Cannot open %s\n", config_file);
        ret = -1;
    }

    strcpy(value, value_def);
    LOGI("%s set to default (%s)\n", key, value_def);
    return ret;
}

/**********************************/
/*       Start Get ROM Name       */
/*    Original source by PhilZ    */
/**********************************/
// formats a string to be compliant with filenames standard and limits its length to max_len
static void format_filename(char *valid_path, int max_len) {
    // remove non allowed chars (invalid file names) and limit valid_path filename to max_len chars
    // we could use a whitelist: ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-
    char invalid_fn[] = " /><%#*^$:;\"\\\t,?!{}()=+'¦|";
    int i = 0;
    for(i=0; valid_path[i] != '\0' && i < max_len; i++) {
        int j = 0;
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
#define MAX_ROM_NAME_LENGTH 31
void get_rom_name(char *rom_name) {
    const char *rom_id_key[] = { "ro.modversion", "ro.romversion", "ro.product.version", "ro.build.version.incremental", NULL };
    const char *key;
    sprintf(rom_name, "noname");
    int i = 0;
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
#ifdef USE_MIGRATED_STORAGE
static int migrated_storage = 1;
#else
static int migrated_storage = -1;
#endif

int use_migrated_storage() {
    if (migrated_storage == -1) {
        migrated_storage = 0;
        if (ensure_path_mounted("/data") != 0)
            return 0;
        char android_ver[PROPERTY_VALUE_MAX];
        if (read_config_file("/system/build.prop", "ro.build.version.release", android_ver, "4.1") < 0) {
            ui_print("failed to open /system/build.prop!\n");
            return 0;
        }
        struct stat s;
	    if (strncmp(android_ver,"4.2",3) >= 0 && lstat("/data/media/0", &s) == 0)
            migrated_storage = 1;
    }

    return migrated_storage;
}
