// PhilZ Touch config file

#ifndef __ADVANCED_FUNCTIONS_H
#define __ADVANCED_FUNCTIONS_H


#include "ui_defines.h"

// format toggle menus to screen width
// used to format toggle menus to device screen width (only touch build)
void ui_format_gui_menu(char *item_menu, const char* menu_text, const char* menu_option);

// print custom logtail (detailed logging report in raw-backup.sh...)
void ui_print_custom_logtail(const char* filename, int nb_lines);

int read_config_file(const char* config_file, const char *key, char *value, const char *value_def);
int write_config_file(const char* config_file, const char* key, const char* value);
void show_philz_settings_menu();
void wipe_data_menu();
extern int no_files_found;

// aroma launcher
void run_aroma_browser();

// get partition size function (adapted from Dees_Troy - TWRP)
unsigned long long Total_Size; // Overall size of the partition
unsigned long long Used_Size; // Overall used space
unsigned long long Free_Size; // Overall free space
int Get_Size_Via_statfs(const char* Path);
int Find_Partition_Size(const char* Path);

//ors script support in recovery.c
int erase_volume(const char *volume);
void wipe_data(int confirm);
extern int no_wipe_confirm;
int check_boot_script_file(const char* boot_script);
int run_ors_boot_script();
int run_ors_script(const char* ors_script);
int ors_backup_command(const char* backup_path, const char* options);

// general system functions
unsigned long long gettime_nsec();
long long timenow_usec(void);
long long timenow_msec(void);
int is_time_interval_passed(long long msec_interval);
char* readlink_device_blk(const char* Path);
unsigned long Get_File_Size(const char* Path);
unsigned long long Get_Folder_Size(const char* Path);
int file_found(const char* filename);
int directory_found(const char* dir);
void delete_a_file(const char* filename);
void ensure_directory(const char* dir); // in nandroid.c
int is_path_ramdisk(const char* path);
int copy_a_file(const char* file_in, const char* file_out);
int append_string_to_file(const char* filename, const char* string);
char* read_file_to_buffer(const char* filepath, unsigned long *len);
char* BaseName(const char* path);
char* DirName(const char* path);

// case insensitive C-string compare (adapted from titanic-fanatic)
int strcmpi(const char *str1, const char *str2);

// calculate md5sum when installing zip files from menu
void start_md5_display_thread(char* filepath);
void stop_md5_display_thread();
void start_md5_verify_thread(char* filepath);
void stop_md5_verify_thread();

// md5sum calculate / display / write / check
int write_md5digest(const char* filepath, const char* md5file, int append);
int verify_md5digest(const char* filepath, const char* md5file);

// custom zip path + free browse mode
void set_ensure_mount_always_true(int state);
int show_custom_zip_menu();
void set_custom_zip_path();

// nandroid settings and functions
void show_twrp_restore_menu(const char* backup_volume);
void custom_backup_menu(const char* backup_volume);
void custom_restore_menu(const char* backup_volume);
void get_twrp_backup_path(const char* backup_volume, char *backup_path);
void get_cwm_backup_path(const char* backup_volume, char *backup_path);
void misc_nandroid_menu();
void get_rom_name(char *rom_name);
void get_device_id(char *device_id);
void reset_custom_job_settings(int custom_job);

// custom backup and restore menu items
enum {
  LIST_ITEM_VALIDATE,
  LIST_ITEM_REBOOT,
  LIST_ITEM_BOOT,
  LIST_ITEM_RECOVERY,
  LIST_ITEM_SYSTEM,
  LIST_ITEM_PRELOAD,
  LIST_ITEM_DATA,
  LIST_ITEM_ANDSEC,
  LIST_ITEM_CACHE,
  LIST_ITEM_SDEXT,
  LIST_ITEM_MODEM,
  LIST_ITEM_RADIO,
  LIST_ITEM_EFS,
  LIST_ITEM_MISC,
  LIST_ITEM_DATAMEDIA,
  LIST_ITEM_WIMAX,
  LIST_ITEM_EXTRA_1,
  LIST_ITEM_EXTRA_2,
  LIST_ITEM_EXTRA_3,
  LIST_ITEM_EXTRA_4,
  LIST_ITEM_EXTRA_5
};

#ifndef EXTRA_PARTITIONS_PATH
#define EXTRA_PARTITIONS_PATH   "/extra_part"
#endif
#define EXTRA_PARTITIONS_NUM    5
struct extra_partitions_list {
    char menu_label[MENU_MAX_COLS];
    int backup_state;
} extra_partition[EXTRA_PARTITIONS_NUM];

// multi zip installer
void show_multi_flash_menu();

#endif // __ADVANCED_FUNCTIONS_H

