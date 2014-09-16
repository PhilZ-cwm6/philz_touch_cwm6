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
#include <dirent.h>

#include "mtdutils/mtdutils.h"
#include "mtdutils/mounts.h"
#include "roots.h"
#include "common.h"
#include "make_ext4fs.h"

#include <fs_mgr.h>
#include <libgen.h>

#include <fcntl.h> // open()

#include "flashutils/flashutils.h"  // format_unknown_device() MTD / BML / MMC support
#include "bmlutils/bmlutils.h"  // format_rfs_device()
#include "extendedcommands.h"
#include "advanced_functions.h"

#include "voldclient/voldclient.h"
#include "libcrecovery/common.h" // __popen / __pclose

static struct fstab *fstab = NULL;

extern struct selabel_handle *sehandle;

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
* fstab_extra flags will not be used, so we set them to "defaults"
* after that, we call add_extra_fstab_entries():
    - it will compare each fstab->recs[i].mount_point from main recovery.fstab to the fstab_extra->recs[i].mount_point from extra.fstab
    - if same mount point is found in both files, blk_device2, fs_type2 and fs_options2 from fstab->recs[i] are set to
      blk_device, fs_type and fs_options from fstab_extra->recs[i]
    - so, mount_point in extra.fstab must be the same as in main fstab.device we're using to create recovery.fstab
    - in cm11, since mount_point is auto, we should use same voldmanaged label in both extra.fstab and fstab.device like "voldmanaged=sdcard0:36"
      this way, we can compare fstab->recs[i].label to fstab_extra->recs[i].label to assign blk_device2, fs_type2 and fs_options2
*/
static struct fstab *fstab_extra = NULL;
static void add_extra_fstab_entries(int index) {
    if (!fstab_extra)
        return;
    int i;
    Volume* vol = &fstab->recs[index];
    for(i = 0; i < fstab_extra->num_entries; ++i) {
        Volume* extraVol = &fstab_extra->recs[i];
        if (strcmp(extraVol->mount_point, vol->mount_point) == 0) {
            vol->blk_device2 = strdup(extraVol->blk_device);
            vol->fs_type2 = strdup(extraVol->fs_type);
            if (extraVol->fs_options != NULL)
                vol->fs_options2 = strdup(extraVol->fs_options);
        }
    }
}

static void load_volume_table_extra() {
    int i;
    fs_mgr_free_fstab(fstab_extra);
    fstab_extra = fs_mgr_read_fstab("/etc/extra.fstab");
    if (!fstab_extra) {
        printf("No /etc/extra.fstab\n");
        return;
    }

    printf("\nextra filesystem table: (device2, fstype2, options2):\n");
    printf(  "======================\n");
    for(i = 0; i < fstab_extra->num_entries; ++i) {
        Volume* v = &fstab_extra->recs[i];
        printf("  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
                v->blk_device, v->length);
    }
    printf("\n");
}

Volume* volume_for_path_extra(const char* path) {
    return fs_mgr_get_entry_for_mount_point(fstab_extra, path);
}
//----- end extra.fstab support

static void write_fstab_entry(Volume *v, FILE *file)
{
    if (v == NULL || file == NULL || strcmp(v->fs_type, "ramdisk") == 0)
        return;

    // detect MTD/EMMC devices by name. BML devices won't be auto detected and we need the proper blk device in recovery.fstab!
    // mtdutils.c/cmd_bml_get_partition_device() always returns -1
    char device[200];
    if (strncmp(v->blk_device, "/", 1) != 0) {
        if (get_partition_device(v->blk_device, device) != 0) {
            printf("    invalid device: skipping /etc/fstab entry\n");
            return;
        }
    } else {
        strcpy(device, v->blk_device);
    }

    if (strcmp(v->fs_type, "emmc") != 0 && 
            !fs_mgr_is_voldmanaged(v) &&
            strncmp(device, "/", 1) == 0 &&
            strncmp(v->mount_point, "/", 1) == 0) {

        fprintf(file, "%s ", device);
        fprintf(file, "%s ", v->mount_point);
        // special case rfs cause auto will mount it as vfat on samsung.
        // use real fstype if it is an f2fs/ext4 conversion
        char* fstype = v->fs_type;
        if (v->fs_type2 != NULL) {
            if (strcmp(v->fs_type, "rfs") != 0 && 
                !(strcmp(v->fs_type, "f2fs") == 0 && strcmp(v->fs_type2, "ext4") == 0) &&
                !(strcmp(v->fs_type, "ext4") == 0 && strcmp(v->fs_type2, "f2fs") == 0))
            fstype = "auto";
        }
        fprintf(file, "%s defaults\n", fstype);
    }
}

int get_num_volumes() {
    return fstab->num_entries;
}

Volume* get_device_volumes() {
    return fstab->recs;
}

static int is_datamedia = 0;
int is_data_media()
{
    return is_datamedia;
}

// is volume a physical primary storage ?
int is_volume_primary_storage(Volume* v)
{
    if (v == NULL)
        return 0;

    // Static mount point /sdcard is primary storage, except when it's
    // declared as datamedia
    if (strcmp(v->mount_point, "/sdcard") == 0) {
        if (strcmp(v->fs_type, "datamedia") == 0) {
            return 0;
        }
        return 1;
    }

    // Static mount points beginning with /mnt/media_rw/sdcard are primary
    // storage except when a non-zero digit follows (eg. sdcard[1-9])
    if (strcmp(v->mount_point, "/mnt/media_rw/sdcard0") == 0 || strcmp(v->mount_point, "/mnt/media_rw/sdcard") == 0) {
        return 1;
    }

    // Dynamic mount points which label begins with sdcard are primary storage
    // except when a non-zero digit follows (eg. sdcard[1-9])
    // load_volume_table() allows a custom moint point different from /storage/label
    if (fs_mgr_is_voldmanaged(v)) {
        if (strcmp(v->label, "sdcard0") == 0 || strcmp(v->label, "sdcard") == 0)
            return 1;
    }

    return 0;
}

// check if the volume is used as secondary storage
// we also allow voldmanaged usb volumes
int is_volume_extra_storage(Volume* v) {
    if (strcmp(v->mount_point, get_primary_storage_path()) == 0)
        return 0;

    if (strcmp(v->mount_point, "/external_sd") == 0 ||
            strncmp(v->mount_point, "/mnt/media_rw/sdcard", 20) == 0) {
        return 1;
    }

    // load_volume_table() allows a custom moint point different from /storage/label
    if (fs_mgr_is_voldmanaged(v)) {
        return 1;
    }

    return 0;
}

void load_volume_table()
{
    int i;
    int ret;

    fs_mgr_free_fstab(fstab);
    fstab = fs_mgr_read_fstab("/etc/recovery.fstab");
    if (!fstab) {
        printf("failed to read /etc/recovery.fstab\n");
        return;
    }

    ret = fs_mgr_add_entry(fstab, "/tmp", "ramdisk", "ramdisk", 0);
    if (ret < 0 ) {
        printf("failed to add /tmp entry to fstab\n");
        fs_mgr_free_fstab(fstab);
        fstab = NULL;
        return;
    }

    // load /etc/extra.fstab entries 
    load_volume_table_extra();

    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];

        // Process vold-managed volumes with mount point "auto"
        // we also allow a custom moint point different from /storage/label
        // https://source.android.com/devices/tech/storage/config-example.html
        if (fs_mgr_is_voldmanaged(v) && strcmp(v->mount_point, "auto") == 0) {
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "/storage/%s", v->label);
            free(v->mount_point);
            v->mount_point = strdup(path);
        }

        // get blk_device2, fstype_2 and fs_options2 if they exist
        add_extra_fstab_entries(i);
    }

#ifdef USE_F2FS
    // allow switching between f2fs/ext4 depending on actual real format
    // if fstab entry matches the real device fs_type, do nothing
    // also skip vold managed devices as vold relies on the defined flags.
    // vold managed devices should be set to auto fstype for free formatting
    printf("checking ext4 <-> f2fs conversion...\n");
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];

        if (strcmp(v->fs_type, "ext4") == 0 || strcmp(v->fs_type, "f2fs") == 0) {
            char* real_fstype = get_real_fstype(v->mount_point);
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
                printf("  %s: %s -> %s\n", v->mount_point, fstab_fstype, v->fs_type);
                free(fstab_fstype);
            }
        }
    }
    printf("\n");
#endif

    // Create /etc/fstab so tools like Busybox mount work
    FILE *file;
    file = fopen("/etc/mtab", "a");
    fclose(file);
    file = fopen("/etc/fstab", "w");
    if (file == NULL) {
        LOGW("Unable to create /etc/fstab!\n");
    }

    is_datamedia = 1;

    printf("recovery filesystem table\n");
    printf("=========================\n");
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];
        printf("  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
                v->blk_device, v->length);
        if (v->blk_device2 != NULL) {
            // print extra volume table
            printf("    %s %s %s %lld\n", v->mount_point, v->fs_type2,
                    v->blk_device2, v->length);
        }

        if (is_volume_primary_storage(v)) {
            is_datamedia = 0;
        }

        write_fstab_entry(v, file);
    }

    if (file != NULL)
        fclose(file);

    printf("\n");
}

Volume* volume_for_path(const char* path) {
    return fs_mgr_get_entry_for_mount_point(fstab, path);
}

static char* primary_storage_path = NULL;
char* get_primary_storage_path() {
    int i = 0;
    if (primary_storage_path == NULL) {
        primary_storage_path = "/sdcard";
        for (i = 0; i < fstab->num_entries; ++i) {
            Volume* v = &fstab->recs[i];
            if (is_volume_primary_storage(v)) {
                // one time malloc
                primary_storage_path = strdup(v->mount_point);
                break;
            }
        }
    }
    return primary_storage_path;
}

int is_primary_storage_voldmanaged() {
    Volume* v = volume_for_path(get_primary_storage_path());
    return fs_mgr_is_voldmanaged(v);
}

int get_num_extra_volumes() {
    int num = 0;
    int i;
    for (i = 0; i < get_num_volumes(); ++i) {
        Volume* v = get_device_volumes() + i;
        if (is_volume_extra_storage(v)) {
            if (fs_mgr_is_voldmanaged(v) && !vold_is_volume_available(v->mount_point))
                continue;

            num++;
        }
    }
    return num;
}

char** get_extra_storage_paths() {
    char** paths = NULL;
    int num_extra_volumes = get_num_extra_volumes();
    int i = 0, j = 0;

    if (num_extra_volumes == 0)
        return NULL;

    paths = (char**)malloc((num_extra_volumes + 1) * sizeof(char*));
    if (paths == NULL) {
        LOGE("get_extra_storage_paths: memory error\n");
        return NULL;
    }

    for (i = 0; i < get_num_volumes(); ++i) {
        Volume* v = get_device_volumes() + i;
        if (is_volume_extra_storage(v)) {
            if (fs_mgr_is_voldmanaged(v) && !vold_is_volume_available(v->mount_point))
                continue;

            paths[j] = strdup(v->mount_point);
            ++j;
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
    if (strcmp(fs_type, "auto") == 0) {
        char mount_cmd[PATH_MAX];
        sprintf(mount_cmd, "mount %s %s", device, mount_point);
        ret = __system(mount_cmd);
    } else if (fs_options == NULL) {
        ret = mount(device, mount_point, fs_type,
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        // LOGE("ret =%d - device=%s - mount_point=%s - fstype=%s\n", ret, device, mount_point, fs_type); // debug
    } else {
        char mount_cmd[PATH_MAX];
        sprintf(mount_cmd, "mount -t %s -o%s %s %s", fs_type, fs_options, device, mount_point);
        ret = __system(mount_cmd);
    }

    if (ret != 0)
        LOGW("failed to mount %s %s %s (%s)\n", device, mount_point, fs_type, strerror(errno));
    return ret;
}

/*
- Since Android 4.2, internal storage is located in /data/media/0 (multi user compatibility)
- When upgrading to android 4.2, /data/media content is "migrated" to /data/media/0
- In recovery, we force use of /data/media instead of /data/media/0 for internal storage if /data/media/.cwm_force_data_media file is found
- For devices with pre-4.2 android support, we can define BOARD_HAS_NO_MULTIUSER_SUPPORT flag to default to /data/media, unless /data/media/0 exists
- If we call check_migrated_storage() directly, we need to ensure_path_mounted("/data") before
- On recovery start, we first call load_volume_table() then setup_data_media(mount = true)
- setup_data_media(mount = true) will mount /data, call check_migrated_storage() then unmount /data
*/
#ifdef BOARD_HAS_NO_MULTIUSER_SUPPORT
static int is_migrated_storage = 0;
#else
static int is_migrated_storage = 1;
#endif

int use_migrated_storage() {
    return is_migrated_storage;
}

static int check_migrated_storage() {
    struct stat s;
#ifdef BOARD_HAS_NO_MULTIUSER_SUPPORT
    is_migrated_storage = (lstat("/data/media/0", &s) == 0 &&
                          lstat("/data/media/.cwm_force_data_media", &s) != 0);
#else
    is_migrated_storage = (lstat("/data/media/.cwm_force_data_media", &s) != 0);
#endif
    return is_migrated_storage;
}

// load_volume_table() must have been called at this stage, so we can use ensure_path_mounted()
// data partition must be mounted if we pass in argument mount = 0
// setup_data_media() is either called by:
//  - ensure_path_mounted() which will mount /data
//  - on recovery start AFTER load_volume_table() AND with mount = true argument (setup_data_media(mount = true))
//  - in show_advanced_menu() to toggle /data/media target
//  - in show_partition_menu() and show_format_ext4_or_f2fs_menu() after format /data to recreate sdcard link
void setup_data_media(int mount) {
    if (!is_data_media())
        return;

    if (mount) {
        int count = 0;
        int ret = -1;
        while (count < 5 && ret != 0) {
            usleep(500000); // wait before first trial to avoid busy device on recovery start
            ret = ensure_path_mounted("/data");
            ++count;
        }
        if (ret != 0) {
            LOGE("could not mount /data to setup /data/media path!\n");
            return;
        }
    }

    int i;
    char* mount_point = "/sdcard";
    for (i = 0; i < get_num_volumes(); ++i) {
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
    if (check_migrated_storage()) {
        path = "/data/media/0";
        mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }
    symlink(path, mount_point);

    if (mount) {
        int count = 0;
        int ret = -1;
        while (count < 5 && ret != 0) {
            ret = ensure_path_unmounted("/data");
            usleep(500000);
            ++count;
        }
        if (ret != 0)
            LOGE("could not unmount /data after /data/media setup\n");
    }
}

// handle /sdcard/ path when not defined in recovery.fstab
int is_data_media_volume_path(const char* path) {
    if (!is_data_media())
        return 0;

    Volume* v = volume_for_path(path);
    if (v != NULL)
        return strcmp(v->fs_type, "datamedia") == 0;

    if (strcmp(path, "/sdcard") == 0 || strncmp(path, "/sdcard/", 8) == 0)
        return 1;

    return 0;
}

// ensure_path_mounted_always_true will not try to mount a partition and always return true
// we need this in md5 thread as ensure_path_mounted_at_mount_point() is not thread friendly
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
    if (is_data_media_volume_path(path)) {
        int ret;
        if (0 != (ret = ensure_path_mounted("/data")))
            return ret;
        setup_data_media(0);
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

    if (mount_point == NULL)
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
    } else if (strcmp(v->fs_type, "auto") == 0 || 
               strcmp(v->fs_type, "ext4") == 0 ||
               strcmp(v->fs_type, "ext3") == 0 ||
#ifdef USE_F2FS
               strcmp(v->fs_type, "f2fs") == 0 ||
#endif
               strcmp(v->fs_type, "rfs") == 0 ||
               strcmp(v->fs_type, "vfat") == 0) {
        // LOGE("main pass: %s %s %s %s\n", v->blk_device, mount_point, v->fs_type, v->fs_type2); // debug
        if ((result = try_mount(v->blk_device, mount_point, v->fs_type, v->fs_options)) == 0)
            return 0;
        if ((result = try_mount(v->blk_device2, mount_point, v->fs_type2, v->fs_options2)) == 0)
            return 0;
        if ((result = try_mount(v->blk_device, mount_point, v->fs_type2, v->fs_options2)) == 0)
            return 0;
        return result;
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
    // if we are using /data/media, do not unmount /sdcard until !is_data_media_preserved()
    if (is_data_media_volume_path(path)) {
        if (is_data_media_preserved())
            return 0;
        else
            return ensure_path_unmounted("/data");
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

// recursively deletes path except 'except' path
// this will preserve the /path dir and leave it empty
int rmtree_except(const char* path, const char* except)
{
    char pathbuf[PATH_MAX];
    int rc = 0;
    DIR* dp = opendir(path);
    if (dp == NULL) {
        return -1;
    }
    struct dirent* de;
    while ((de = readdir(dp)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        if (except && !strcmp(de->d_name, except))
            continue;
        struct stat st;
        snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, de->d_name);
        rc = lstat(pathbuf, &st);
        if (rc != 0) {
            LOGE("Failed to stat %s\n", pathbuf);
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            rc = rmtree_except(pathbuf, NULL);
            if (rc != 0)
                break;
            rc = rmdir(pathbuf);
        }
        else {
            rc = unlink(pathbuf);
        }
        if (rc != 0) {
            LOGI("Failed to remove %s: %s\n", pathbuf, strerror(errno));
            break;
        }
    }
    closedir(dp);
    return rc;
}

#ifdef USE_F2FS
extern int make_f2fs_main(int argc, char **argv);
#endif
int format_volume(const char* volume) {
    // check if we're formatting primary_storage (/sdcard) on /data/media device
    // in that case, issue a rm -rf like command
    if (is_data_media_volume_path(volume)) {
        return format_unknown_device(NULL, volume, NULL);
    }

    Volume* v = volume_for_path(volume);
    // silent failure for non existing sd-ext (when we factory reset)
    if (v == NULL) {
        if (strcmp(volume, "/sd-ext") != 0)
            LOGE("unknown volume \"%s\"\n", volume);
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

    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", volume);
        return -1;
    }

    // if we're formatting a path, this will act like 'rm -rf volume'
    if (strcmp(v->mount_point, volume) != 0) {
        return format_unknown_device(NULL, volume, NULL);
    }

    // check to see if /data is being formatted, and if it is /data/media
    // by default, is_data_media_preserved() == 1, so we will go to format_unknown_device()
    // format_unknown_device() with null fstype will rm -rf /data, excluding /data/media path
    // if preserve_data_media(0) is called, is_data_media_preserved() will return 0
    // in that case, we will not use format_unknown_device() but proceed to below with a true format command issued
    if (strcmp(volume, "/data") == 0 && is_data_media() && is_data_media_preserved()) {
        return format_unknown_device(NULL, volume, NULL);
    }

    if (ensure_path_unmounted(volume) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }

    // Only use vold format for exact matches otherwise /sdcard will be
    // formatted instead of /storage/sdcard0/.android_secure
    // this check is redundant as we excluded above the (volume != v->mount_point) case
    if (fs_mgr_is_voldmanaged(v) && strcmp(volume, v->mount_point) == 0) {
        if (ensure_path_unmounted(volume) != 0) {
            LOGE("format_volume failed to unmount %s", v->mount_point);
        }
        if (strcmp(v->fs_type, "auto") == 0) {
            // Format with current filesystem
            return vold_format_volume(v->mount_point, 1) == CommandOkay ? 0 : -1;
        } else {
            // Format filesystem defined in fstab
            return vold_custom_format_volume(v->mount_point, v->fs_type, 1) == CommandOkay ? 0 : -1;
        }
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
            LOGE("format_volume: make_ext4fs failed on %s\n", v->blk_device);
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

    return format_unknown_device(v->blk_device, volume, v->fs_type);
}

// mount /cache and unmount all other partitions before installing zip file
int setup_install_mounts() {
    if (fstab == NULL) {
        LOGE("can't set up install mounts: no fstab loaded\n");
        return -1;
    }

    int i;
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = fstab->recs + i;

        // do not unmount vold managed devices (we need this for aroma file manager zip installer to be able to see the vold devices)
        if (fs_mgr_is_voldmanaged(v))
            continue;
        if (strcmp(v->mount_point, "/tmp") == 0 ||
                strcmp(v->mount_point, "/cache") == 0) {
            if (ensure_path_mounted(v->mount_point) != 0) return -1;

        } else {
            if (ensure_path_unmounted(v->mount_point) != 0) return -1;
        }
    }
    return 0;
}

/* by default: 
    - do not unmount /data if user requests unmounting /sdcard
    - do not format whole /data (include /data/media) when format /data is requested
      instead, do rm -rf with /data/media excluded
*/
static int data_media_preserved_state = 1;
void preserve_data_media(int val) {
    data_media_preserved_state = val;
}

int is_data_media_preserved() {
    return data_media_preserved_state;
}

void setup_legacy_storage_paths() {
    // sdcard symlink is done in setup_data_media()
    if (is_data_media())
        return;

    char* primary_path = get_primary_storage_path();
    if (strcmp(primary_path, "/sdcard") != 0) {
        rmdir("/sdcard");
        symlink(primary_path, "/sdcard");
    }
}

// format to user choice fstype
// called by nandroid_restore_partition_extended() and format_ext4_or_f2fs()
// doesn't support vold managed devices
int format_device(const char *device, const char *path, const char *fs_type) {
    if (is_data_media_volume_path(path)) {
        // we're formatting primary_storage (/sdcard) on /data/media device: issue a rm -rf like command
        return format_unknown_device(NULL, path, NULL);
    }

    Volume* v = volume_for_path(path);
    if (v == NULL) {
        LOGE("unknown volume \"%s\"\n", path);
        return -1;
    }

    if (strcmp(fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", path);
        return -1;
    }

    // if we're formatting a path, this will act like 'rm -rf volume'
    if (strcmp(v->mount_point, path) != 0) {
        return format_unknown_device(v->blk_device, path, NULL);
    }

    // check to see if /data is being formatted, and if it is /data/media
    // by default, is_data_media_preserved() == 1, so we will go to format_unknown_device()
    // format_unknown_device() with null fstype will rm -rf /data, excluding /data/media path
    // if preserve_data_media(0) is called, is_data_media_preserved() will return 0
    // in that case, we will not use format_unknown_device() but proceed to below with a true format command issued
    if (strcmp(path, "/data") == 0 && is_data_media() && is_data_media_preserved()) {
        return format_unknown_device(NULL, path, NULL);
    }

    if (ensure_path_unmounted(path) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }

    if (strcmp(fs_type, "rfs") == 0) {
        if (0 != format_rfs_device(device, path)) {
            LOGE("format_volume: format_rfs_device failed on %s\n", device);
            return -1;
        }
        return 0;
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
        } else if (mtd_erase_blocks(write, -1) == (off_t) - 1) {
            LOGW("format_volume: can't erase MTD \"%s\"\n", device);
            mtd_write_close(write);
            return -1;
        } else if (mtd_write_close(write)) {
            LOGW("format_volume: can't close MTD \"%s\"\n", device);
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

        int result = make_ext4fs(device, length, v->mount_point, sehandle);
        if (result != 0) {
            LOGE("format_volume: make_ext4fs failed on %s\n", device);
            return -1;
        }
        return 0;
    }

#ifdef USE_F2FS
    if (strcmp(fs_type, "f2fs") == 0) {
        char* args[] = { "mkfs.f2fs", v->blk_device };
        if (make_f2fs_main(2, args) != 0) {
            LOGE("format_volume: mkfs.f2fs failed on %s\n", v->blk_device);
            return -1;
        }
        return 0;
    }
#endif
    return format_unknown_device(device, path, fs_type);
}

// these are declared in mmcutils.h: do not include the header file to avoid duplicate define of BLOCK_SIZE warnings
extern int format_ext2_device(const char *device);
extern int format_ext3_device(const char *device);

// support format MTD, MMC, BML, ext2, ext3 and directory rm -rf like
// if fstype is NULL, it will continue with rm -rf "path" command ignoring 'device'
// if it is /data on data media device, we'll exclude /data/media
// on any other path or partition: rm -rf 'path'
int format_unknown_device(const char *device, const char* path, const char *fs_type) {
    LOGI("Formatting unknown device.\n");

    // format MTD, MMC, BML
    if (fs_type != NULL && get_flash_type(fs_type) != UNSUPPORTED)
        return erase_raw_partition(fs_type, device);

    // if this is SDEXT:, don't worry about it if it does not exist.
    if (strcmp(path, "/sd-ext") == 0) {
        struct stat st;
        Volume *vol = volume_for_path("/sd-ext");
        if (vol == NULL || 0 != stat(vol->blk_device, &st)) {
            LOGI("No app2sd partition found. Skipping format of /sd-ext.\n");
            return 0;
        }
    }

    if (fs_type != NULL) {
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

    if (0 != ensure_path_mounted(path)) {
        LOGE("Error mounting %s!\n", path);
        return -1;
    }

    int ret;
    char tmp[PATH_MAX];
    if (strcmp(path, "/data") == 0 && is_data_media() && is_data_media_preserved()) {
        // Preserve .layout_version to avoid "nesting bug"
        // if the /data/media sdcard has already been migrated for android 4.2,
        // prevent the migration from happening again by saving the .layout_version
        LOGI("Preserving layout version\n");
        unsigned char layout_buf[256];
        ssize_t layout_buflen = -1;
        int fd;
        fd = open("/data/.layout_version", O_RDONLY);
        if (fd != -1) {
            layout_buflen = read(fd, layout_buf, sizeof(layout_buf));
            close(fd);
        } else {
            LOGI("/data/media/0 not found. migration may occur.\n");
        }

        ret = rmtree_except("/data", "media");

        // Restore .layout_version
        if (layout_buflen > 0) {
            LOGI("Restoring layout version\n");
            fd = open("/data/.layout_version", O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (fd != -1) {
                write(fd, layout_buf, layout_buflen);
                close(fd);
            }
        }
    } else {
        ret = rmtree_except(path, NULL);
    }

    ensure_path_unmounted(path);
    return 0;
}

/*
 get actual fstype from device (modified code from @kumajaya)
 device argument is a path
 blkid output exp:  /dev/block/mmcblk1p1: UUID="3461-3337" TYPE="exfat"
 to do: use libblkid inline code
*/
char* get_real_fstype(const char* path) {
    char cmd[PATH_MAX];
    char line[1024];
    static char fstype[128];
    char* real_device_fstype = NULL;

    Volume* vol = volume_for_path(path);
    if (vol == NULL) {
        printf("  get_real_fstype: volume not found (%s).\n", path);
        return NULL;
    }

    sprintf(cmd, "/sbin/blkid -c /dev/null %s", vol->blk_device);
    FILE *fp = __popen(cmd, "r");
    if (fp == NULL) {
        printf("  get_real_fstype: blkid error\n");
        return NULL;
    }

    if (fgets(line, sizeof(line), fp) != NULL) {
        char* ptr = strstr(line, "TYPE=");
        if (ptr != NULL && sscanf(ptr + 5, "\"%127[^\"]\"", fstype) == 1)
            real_device_fstype = fstype;
    }
    __pclose(fp);
    if (real_device_fstype == NULL)
        printf("  get_real_fstype: unknown filesystem (%s)\n", path);

    return real_device_fstype;
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

    if (scan_mounted_volumes() < 0)
        return 0;

    const MountedVolume* mv = find_mounted_volume_by_mount_point(v->mount_point);
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

// recovery command helper to create /etc/fstab and link /data/media path
int volume_main(int argc, char **argv) {
    load_volume_table();
    setup_data_media(1);
    return 0;
}
