#ifndef NANDROID_H
#define NANDROID_H

int nandroid_main(int argc, char** argv);
int nandroid_backup(const char* backup_path);
int nandroid_restore(const char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache, int restore_sdext, int restore_wimax);
void nandroid_dedupe_gc(const char* blob_dir);
void nandroid_force_backup_format(const char* fmt);
unsigned nandroid_get_default_backup_format();

#define NANDROID_BACKUP_FORMAT_FILE "/sdcard/clockworkmod/.default_backup_format"
#define NANDROID_BACKUP_FORMAT_TAR 0
#define NANDROID_BACKUP_FORMAT_DUP 1

//Custom nandroid backup by PhilZ
#define EFS_BACKUP_PATH "clockworkmod/custom_backup/.efs_backup"
#define CUSTOM_BACKUP_PATH "clockworkmod/custom_backup"
#define RAW_IMG_FILE 1
#define RAW_BIN_FILE 2
#define RESTORE_EFS_TAR 1
#define RESTORE_EFS_IMG 2
int custom_backup_raw_handler(const char* backup_path, const char* root);
int custom_restore_raw_handler(const char* backup_path, const char* root);
extern int is_custom_backup;
extern int backup_boot;
extern int backup_recovery;
extern int backup_system;
extern int backup_preload;
extern int backup_data;
extern int backup_cache;
extern int backup_sdext;
extern int backup_wimax;
extern int backup_efs;
extern int backup_modem;

//add for use outside nandroid.c (settings file...)
void ensure_directory(const char* dir);

#endif