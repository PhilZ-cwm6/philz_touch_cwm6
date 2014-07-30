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

#ifndef RECOVERY_ROOTS_H_
#define RECOVERY_ROOTS_H_

#include "common.h"

// Load and parse volume data from /etc/recovery.fstab.
void load_volume_table();

// Return the Volume* record for this path (or NULL).
Volume* volume_for_path(const char* path);

// Handle extra.fstab entries
Volume* volume_for_path_extra(const char* path);

// Make sure that the volume 'path' is on is mounted.  Returns 0 on
// success (volume is mounted).
int ensure_path_mounted(const char* path);
int ensure_path_mounted_at_mount_point(const char* path, const char* mount_point);

// Make sure that the volume 'path' is on is mounted.  Returns 0 on
// success (volume is unmounted);
int ensure_path_unmounted(const char* path);

// rm -rf like in c/c++
int rmtree_except(const char* path, const char* except);

// Reformat the given volume (must be the mount point only, eg
// "/cache"), no paths permitted.  Attempts to unmount the volume if
// it is mounted.
int format_volume(const char* volume);

// Ensure that all and only the volumes that packages expect to find
// mounted (/tmp and /cache) are mounted.  Returns 0 on success.
int setup_install_mounts();

// storage
char* get_primary_storage_path();
char** get_extra_storage_paths();
char* get_android_secure_path();
void setup_legacy_storage_paths();
int get_num_extra_volumes();
int get_num_volumes();
int is_primary_storage_voldmanaged();

Volume* get_device_volumes();

int is_data_media();
void setup_data_media(int mount);
int is_data_media_volume_path(const char* path);
void preserve_data_media(int val);
int is_data_media_preserved();

// check if it is an extra storage volume
int is_volume_primary_storage(Volume* v);
int is_volume_extra_storage(Volume* v);

#define MAX_NUM_MANAGED_VOLUMES 10

int use_migrated_storage();

// format device to custom fstype
int format_device(const char *device, const char *path, const char *fs_type);

// support format MTD, MMC, BML, ext2, ext3 and directory rm -rf like if a path is passed
int format_unknown_device(const char *device, const char* path, const char *fs_type);

void set_ensure_mount_always_true(int state);
char* get_real_fstype(const char* path);
int has_datadata();
int is_path_mounted(const char* path);
int volume_main(int argc, char **argv);

#endif  // RECOVERY_ROOTS_H_
