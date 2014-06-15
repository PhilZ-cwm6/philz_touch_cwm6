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
#include "recovery_ui.h"
#include "advanced_functions.h"

#include "voldclient/voldclient.h"
#include "libcrecovery/common.h" // __popen / __pclose

/*
 get actual fstype from device (modified code from @kumajaya)
 device argument is the v->blk_device
 blkid output exp:  /dev/block/mmcblk1p1: UUID="3461-3337" TYPE="exfat"
*/
char* get_real_fstype(const char* device) {
    char cmd[PATH_MAX];
    char line[1024];
    static char fstype[128];
    char* real_device_fstype = NULL;

    sprintf(cmd, "/sbin/blkid -c /dev/null %s", device);
    FILE *fp = __popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Unable to execute blkid.\n");
        return NULL;
    }

    if (fgets(line, sizeof(line), fp) != NULL) {
        char* ptr = strstr(line, "TYPE=");
        if (ptr != NULL && sscanf(ptr + 5, "\"%127[^\"]\"", fstype) == 1)
            real_device_fstype = fstype;
    }
    __pclose(fp);
    if (real_device_fstype == NULL)
        fprintf(stderr, "blkid: unknown filesystem on '%s'\n", device);

    return real_device_fstype;
}

static struct fstab *fstab = NULL;

/* 
system/core/fs_mgr/include/fs_mgr.h
struct fstab {
    int num_entries;
    struct fstab_rec *recs;
    char *fstab_filename;
};

struct fstab_rec {
    char *blk_device;
    char *mount_point;
    char *fs_type;
    unsigned long flags;
    char *fs_options;
    int fs_mgr_flags;
    char *key_loc;
    char *verity_loc;
    long long length;
    char *label;
    int partnum;
    int swap_prio;
    unsigned int zram_size;

    // cwm
    char *blk_device2;
    char *fs_type2;
    char *fs_options2;

    char *lun;
};

*******

system/core/fs_mgr/fs_mgr.c

static struct flag_list mount_flags[] = {
    { "noatime",    MS_NOATIME },
    { "noexec",     MS_NOEXEC },
    { "nosuid",     MS_NOSUID },
    { "nodev",      MS_NODEV },
    { "nodiratime", MS_NODIRATIME },
    { "ro",         MS_RDONLY },
    { "rw",         0 },
    { "remount",    MS_REMOUNT },
    { "bind",       MS_BIND },
    { "rec",        MS_REC },
    { "unbindable", MS_UNBINDABLE },
    { "private",    MS_PRIVATE },
    { "slave",      MS_SLAVE },
    { "shared",     MS_SHARED },
    { "sync",       MS_SYNCHRONOUS },
    { "defaults",   0 },
    { 0,            0 },
};

static struct flag_list fs_mgr_flags[] = {
    { "wait",        MF_WAIT },
    { "check",       MF_CHECK },
    { "encryptable=",MF_CRYPT },
    { "nonremovable",MF_NONREMOVABLE },
    { "voldmanaged=",MF_VOLDMANAGED},
    { "length=",     MF_LENGTH },
    { "recoveryonly",MF_RECOVERYONLY },
    { "swapprio=",   MF_SWAPPRIO },
    { "zramsize=",   MF_ZRAMSIZE },
    { "verify",      MF_VERIFY },
    { "noemulatedsd", MF_NOEMULATEDSD },
    { "defaults",    0 },
    { 0,             0 },
};

****

Support additional extra.fstab entries and add device2
* Needed until fs_mgr_read_fstab() starts to parse a blk_device2 entries
* extra.fstab sample:
----> start extra.fstab
# add here entries already existing in main device fstab, but for which you want a blk_device2, fs_type2 or fs_options2
# used to partition sdcard and format it to ext2/ext3
# used also to stat for size of mtd/yaffs2 partitions

# blk_device2           # mount_point           fs_type2    fs_options2     flags (not used in extra.fstab code)
/dev/block/mmcblk0p28   /storage/sdcard0 		auto	    defaults		defaults
/dev/block/mmcblk1p1	/storage/sdcard1 		auto	    defaults		defaults
/dev/block/sda1			/storage/usbdisk0 		auto	    defaults		defaults
<---- end extra.fstab

* system/core/fs_mgr/fs_mgr.c/fs_mgr_read_fstab() will parse our recovery.fstab file:
    - it will populate recovery fstab struct: fstab->recs[i].blk_device, fstab->recs[i].mount_point, fstab->recs[i].fs_type, fstab->recs[i].fs_options
    - fs_options are defines by "static struct flag_list mount_flags[]"
    - after that, it will call parse_flags() to parse the flags
    - flags are defined in "static struct flag_list fs_mgr_flags[]"
    - the flag voldmanaged=label:partnum is processed by parse_flags() in cm-10.2
        + label is always set to NULL in cm-10.2
        + in cm-11, label is used to define v->mount_point = fstab->recs[i].label as mount point is set to auto in fstab.device
        + partnum is the mmcblk/mtdblk real number for voldmanaged partition
        + exp for a device with dedicated internal sdcard partition:
          voldmanaged=sdcard0:36: label is sdcard0, mount_point will be /storage/sdcard0 and partition is /dev/block/mmcblk0p36
          in cm-10.2, what ever we set label, it doesn't matter as it is discarded by parse_flags() and we use the defined mount_point in fstab.device
    - to null options, use "defaults" or a non defined entry in flag_list mount_flags[] like "default"
    - to null flags, use "defaults" or a non defined entry in flag_list fs_mgr_flags[] like "default"
* once recovery fstab struct is populated by roots.c/load_volume_table(), we process extra.fstab by load_volume_table_extra()
* it will parse the extra.fstab by calling fs_mgr_read_fstab()
* this will populate fstab_extra->recs[i].blk_device, fstab_extra->recs[i].mount_point, fstab_extra->recs[i].fs_type, fstab_extra->recs[i].fs_options
* fstab_extra flags will not be used in cm-10.2, so we them to "defaults"
* after that, we call add_extra_fstab_entries():
    - it will compare each fstab->recs[i].mount_point from main recovery.fstab to the fstab_extra->recs[i].mount_point from extra.fstab
    - if same mount point is found in both files, blk_device2, fs_type2 and fs_options2 from fstab->recs[i] are set to blk_device, fs_type and fs_options from fstab_extra->recs[i]
    - so, mount point in extra.fstab must be the same as in main fstab.device we're using to create recovery.fstab
    - in cm11, since mount_point is auto, we should use same voldmanaged flag in both extra.fstab and fstab.device like "voldmanaged=sdcard0:36"
      this way, we can compare fstab->recs[i].label to fstab_extra->recs[i].label to assign blk_device2, fs_type2 and fs_options2
*/
static struct fstab *fstab_extra = NULL;
static void add_extra_fstab_entries(int num) {
    int i;
    for(i = 0; i < fstab->num_entries; ++i) {
        if (strcmp(fstab->recs[i].mount_point, fstab_extra->recs[num].mount_point) == 0) {
            fstab->recs[i].blk_device2 = strdup(fstab_extra->recs[num].blk_device);
            fstab->recs[i].fs_type2 = strdup(fstab_extra->recs[num].fs_type);
            if (fstab_extra->recs[num].fs_options != NULL)
                fstab->recs[i].fs_options2 = strdup(fstab_extra->recs[num].fs_options);
        }
    }
}

static void load_volume_table_extra() {
    int i;
    fs_mgr_free_fstab(fstab_extra);
    fstab_extra = fs_mgr_read_fstab("/etc/extra.fstab");
    if (!fstab_extra) {
        fprintf(stderr, "No /etc/extra.fstab\n");
        return;
    }

    fprintf(stderr, "\nextra filesystem table: (device2, fstype2, options2):\n");
    fprintf(stderr,   "======================\n");
    for(i = 0; i < fstab_extra->num_entries; ++i) {
        Volume* v = &fstab_extra->recs[i];
        add_extra_fstab_entries(i);
        fprintf(stderr, "  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
                v->blk_device, v->length);
    }
    fprintf(stderr, "\n");
}

Volume* volume_for_path_extra(const char* path) {
    return fs_mgr_get_entry_for_mount_point(fstab_extra, path);
}
//----- end extra.fstab support

int get_num_volumes() {
    return fstab->num_entries;
}

Volume* get_device_volumes() {
    return fstab->recs;
}

void load_volume_table() {
    int i;
    int ret;

    fs_mgr_free_fstab(fstab);
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

    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];

        // Process vold-managed volumes with mount point "auto"
        if (fs_mgr_is_voldmanaged(v) && strcmp(v->mount_point, "auto") == 0) {
            char mount[PATH_MAX];

            // Set the mount point to /storage/label which as used by vold
            snprintf(mount, PATH_MAX, "/storage/%s", v->label);
            free(v->mount_point);
            v->mount_point = strdup(mount);
        }
    }

    load_volume_table_extra();

#ifdef USE_F2FS
    // allow switching between f2fs/ext4 depending on actual real format
    // if fstab entry matches the real device fs_type, do nothing
    // also skip vold managed devices as vold relies on the defined flags. These should be set to auto fstype for free formatting
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];

        if (strcmp(v->fs_type, "ext4") == 0 || strcmp(v->fs_type, "f2fs") == 0) {
            char* real_fstype = get_real_fstype(v->blk_device);
            if (real_fstype == NULL || strcmp(real_fstype, v->fs_type) == 0 || fs_mgr_is_voldmanaged(v))
                continue;

            if (strcmp(real_fstype, "ext4") == 0 || strcmp(real_fstype, "f2fs") == 0) {
                char *fstab_fstype;
                char *fstab_options = NULL;
                if (v->fs_type2 != NULL && v->fs_options2 != NULL && strcmp(real_fstype, v->fs_type2) == 0) {
                    // try to use existing fs_options2 if possible
                    fstab_fstype = strdup(v->fs_type);
                    free(v->fs_type);
                    v->fs_type = strdup(v->fs_type2);
                    free(v->fs_type2);
                    v->fs_type2 = strdup(fstab_fstype);
                    if (v->fs_options != NULL) {
                        fstab_options = strdup(v->fs_options);
                        free(v->fs_options);
                    }
                    v->fs_options = strdup(v->fs_options2);
                    free(v->fs_options2);
                    if (fstab_options != NULL) {
                        v->fs_options2 = strdup(fstab_options);
                        free(fstab_options);
                    }
                } else {
                    // no fs_options2: drop to bare minimal default fs_options
                    fstab_fstype = strdup(v->fs_type);
                    free(v->fs_type);
                    v->fs_type = strdup(real_fstype);

                    if (v->fs_options != NULL)
                        free(v->fs_options);

                    if (strcmp(v->fs_type, "f2fs") == 0) {
                        v->fs_options = strdup("rw,noatime,nodev,nodiratime,inline_xattr");
                    } else {
                        // ext4: default options will be set in try_mount()
                        v->fs_options = NULL;
                    }
                }
                fprintf(stderr, "%s: %s -> %s\n", v->mount_point, fstab_fstype, v->fs_type);
                free(fstab_fstype);
            }
        }
    }
#endif

#ifdef BOARD_NATIVE_DUALBOOT_SINGLEDATA
    device_truedualboot_after_load_volume_table();
#endif

    fprintf(stderr, "recovery filesystem table\n");
    fprintf(stderr, "=========================\n");
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];
        fprintf(stderr, "  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
                v->blk_device, v->length);
        if (v->blk_device2 != NULL) {
            // print extra volume table
            fprintf(stderr, "  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type2,
                    v->blk_device2, v->length);
        }
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
        android_secure_path = malloc(sizeof("/.android_secure") + strlen(get_primary_storage_path()) + 1);
        sprintf(android_secure_path, "%s/.android_secure", primary_storage_path);
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
        // LOGE("ret =%d - device=%s - mount_point=%s - fstype=%s\n", ret, device, mount_point, fs_type); // debug
    }
    else {
        char mount_cmd[PATH_MAX];
        sprintf(mount_cmd, "mount -t %s -o%s %s %s", fs_type, fs_options, device, mount_point);
        ret = __system(mount_cmd);
    }
    if (ret == 0)
        return 0;
    LOGW("failed to mount %s %s %s (%s)\n", device, mount_point, fs_type, strerror(errno));
    return ret;
}

/*
- Since Android 4.2, internal storage is located in /data/media/0 (multi user compatibility)
- When upgrading to android 4.2, /data/media content is "migrated" to /data/media/0
- In recovery, we force use of /data/media instead of /data/media/0 for internal storage if /data/media/.cwm_force_data_media file is found
- For devices with pre-4.2 android support, we can define BOARD_HAS_NO_MULTIUSER_SUPPORT flag to default to /data/media, unless /data/media/0 exists
- If we call use_migrated_storage() directly, we need to ensure_path_mounted("/data") before
- On recovery start, no need to mount /data before as use_migrated_storage() is called by setup_data_media()
  setup_data_media() is either called by ensure_path_mounted() which will mount /data or
  it is called by process_volumes() where /data is mounted before and unmounted after calling setup_data_media()
*/
int use_migrated_storage() {
    struct stat s;
#ifdef BOARD_HAS_NO_MULTIUSER_SUPPORT
    return lstat("/data/media/0", &s) == 0 &&
            lstat("/data/media/.cwm_force_data_media", &s) != 0;
#else
    return lstat("/data/media/.cwm_force_data_media", &s) != 0;
#endif
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

    // recreate /data/media with proper permissions
    rmdir(mount_point);
    mkdir("/data/media", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    // support /data/media/0 (Android 4.2+)
    char* path = "/data/media";
    if (use_migrated_storage()) {
        path = "/data/media/0";
        mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }
    symlink(path, mount_point);
/*
    // debug
    if (ui_should_log_stdout())
        LOGI("using %s for %s\n", path, mount_point);
*/
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

static int ensure_path_mounted_always_true = 0;
void set_ensure_mount_always_true(int state) {
    ensure_path_mounted_always_true = state;
}

int ensure_path_mounted(const char* path) {
    if (ensure_path_mounted_always_true)
        return 0;
    return ensure_path_mounted_at_mount_point(path, NULL);
}

// not thread safe because of scan_mounted_volumes()
int ensure_path_mounted_at_mount_point(const char* path, const char* mount_point) {
#ifdef BOARD_NATIVE_DUALBOOT_SINGLEDATA
    if (device_truedualboot_mount(path, mount_point) <= 0)
        return 0;
#endif

    if (is_data_media_volume_path(path)) {
        if (ui_should_log_stdout() && ui_is_initialized()) {
            // ui_is_initialized() check to limit output logging during "adb shell nandroid" commands
            // also, will limit logging when "mount /sdcard" from shell  on /data/media devices with BOARD_RECOVERY_HANDLES_MOUNT enabled
            LOGI("setting up /data/media(/0) for %s.\n", path);
        }
        int ret;
        if (0 != (ret = ensure_path_mounted("/data")))
            return ret;
        setup_data_media();
        return 0;
    }
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        // silent failure for sd-ext
        if (strncmp(path, "/sd-ext", 7) != 0)
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
        result = vold_mount_volume(mount_point, 1) == CommandOkay ? 0 : -1;
        if (result == 0) return 0;
    }

    if (strcmp(v->fs_type, "yaffs2") == 0) {
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
#ifdef USE_F2FS
               strcmp(v->fs_type, "f2fs") == 0 ||
#endif
               strcmp(v->fs_type, "rfs") == 0 ||
               strcmp(v->fs_type, "vfat") == 0) {
        // LOGE("main pass: %s %s %s %s\n", v->blk_device, mount_point, v->fs_type, v->fs_type2); // debug
        if ((result = try_mount(v->blk_device, mount_point, v->fs_type, v->fs_options)) == 0)
            return 0;
        if ((result = try_mount(v->blk_device, mount_point, v->fs_type2, v->fs_options2)) == 0)
            return 0;
        if ((result = try_mount(v->blk_device2, mount_point, v->fs_type2, v->fs_options2)) == 0)
            return 0;
        return result;
    } else if (strcmp(v->fs_type, "auto") == 0) {
        // either we are using fstab with non vold managed external storage or vold failed to mount a storage (ext2/ext4, some vfat systems)
        // on vold managed devices, we need the blk_device2
        char mount_cmd[PATH_MAX];
        sprintf(mount_cmd, "mount %s %s", v->blk_device2 != NULL ? v->blk_device2 : v->blk_device, mount_point);
        return __system(mount_cmd);
    } else {
        // let's try mounting with the mount binary and hope for the best.
        // case called by ensure_path_mounted_at_mount_point("/emmc", "/sdcard") in edifyscripting.c (this now obsolete)
        // however, keep the code for clarity and eventual dual boot support using preload or other partitions in future
        // keep both alternatives to not break things for MTD devices when using v->blk_device on system mount command
        char mount_cmd[PATH_MAX];
        if (strcmp(v->mount_point, mount_point) != 0)
            sprintf(mount_cmd, "mount %s %s", v->blk_device, mount_point);
        else
            sprintf(mount_cmd, "mount %s", v->mount_point);
        return __system(mount_cmd);
    }

    return -1;
}

// not thread safe because of scan_mounted_volumes()
int ensure_path_unmounted(const char* path) {
#ifdef BOARD_NATIVE_DUALBOOT_SINGLEDATA
    if (device_truedualboot_unmount(path) <= 0)
        return 0;
#endif

    // if we are using /data/media, do not ever unmount volumes /data or /sdcard
    if (is_data_media_volume_path(path)) {
        return ensure_path_unmounted("/data");
    }
    if (strstr(path, "/data") == path && is_data_media() && is_data_media_preserved()) {
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
#ifdef BOARD_NATIVE_DUALBOOT_SINGLEDATA
    if (device_truedualboot_format_volume(volume) <= 0)
        return 0;
#endif

    if (is_data_media_volume_path(volume)) {
        // format_unknown_device() with NULL as fstype will end to a rm -rf command issued on path /sdcard, that is /data/media folder
        return format_unknown_device(NULL, volume, NULL);
    }
    // check to see if /data is being formatted, and if it is /data/media
    // Note: the /sdcard check is redundant probably, just being safe.
    // by default, is_data_media_preserved() == 1, so we will go to format_unknown_device()
    // format_unknown_device() with null fstype will rm -rf /data, excluding /data/media path
    // if preserve_data_media(0) is called, is_data_media_preserved() will return 0
    // in that case, we will not use format_unknown_device() but proceed to below with a true format command issued
    if (strstr(volume, "/data") == volume && is_data_media() && is_data_media_preserved()) {
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
        int result = make_ext4fs(v->blk_device, v->length, volume, sehandle);
        if (result != 0) {
            LOGE("format_volume: make_extf4fs failed on %s\n", v->blk_device);
            return -1;
        }
        return 0;
    }

#ifdef USE_F2FS
    if (strcmp(v->fs_type, "f2fs") == 0) {
        char* args[] = { "mkfs.f2fs", v->blk_device };
        if (make_f2fs_main(2, args) != 0) {
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

static int data_media_preserved_state = 1;
void preserve_data_media(int val) {
    data_media_preserved_state = val;
}

int is_data_media_preserved() {
    return data_media_preserved_state;
}

void setup_legacy_storage_paths() {
    char* primary_path = get_primary_storage_path();

    if (!is_data_media_volume_path(primary_path)) {
        rmdir("/sdcard");
        symlink(primary_path, "/sdcard");
    }
}
