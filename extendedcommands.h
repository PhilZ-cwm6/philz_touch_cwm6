/*
    PhilZ Touch - touch_gui library
    Copyright (C) <2014>  <phytowardt@gmail.com>

    This file is part of PhilZ Touch Recovery

    PhilZ Touch is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PhilZ Touch is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PhilZ Touch.  If not, see <http://www.gnu.org/licenses/>.

*/


// PhilZ Touch config file

#ifndef __ADVANCED_FUNCTIONS_H
#define __ADVANCED_FUNCTIONS_H


#include <linux/limits.h>   // PATH_MAX
#include "ui_defines.h" // MENU_MAX_COLS, CHAR_HEIGHT, CHAR_WIDTH

// print custom logtail (detailed logging report in raw-backup.sh...)
void ui_print_custom_logtail(const char* filename, int nb_lines);

void show_nandroid_restore_menu(const char* path);
void show_choose_zip_menu();
void choose_default_backup_format();
int show_nandroid_menu();
int show_partition_mounts_menu();
void show_partition_format_menu();
void show_advanced_menu();
void show_format_sdcard_menu(const char* path);
int can_partition(const char* path);
int show_install_update_menu();
void show_advanced_power_menu();

void show_philz_settings_menu();
void wipe_data_menu();

// aroma launcher
void run_aroma_browser();

//ors script support in recovery.c
int check_boot_script_file(const char* boot_script);
int run_ors_boot_script();
int run_ors_script(const char* ors_script);
int ors_backup_command(const char* backup_path, const char* options);

// export recovery log to sdcard
void handle_failure();

int verify_root_and_recovery();
void write_recovery_version();

int confirm_selection(const char* title, const char* confirm);
int confirm_with_headers(const char** confirm_headers, const char* confirm);
int get_filtered_menu_selection(const char** headers, char** items, int menu_only, int initial_selection, int items_count);
extern int no_files_found;
char* choose_file_menu(const char* basedir, const char* fileExtensionOrDirectory, const char* headers[]);

// calculate md5sum when installing zip files from menu
void start_md5_display_thread(char* filepath);
void stop_md5_display_thread();
void start_md5_verify_thread(char* filepath);
void stop_md5_verify_thread();

// md5sum calculate / display / write / check
int write_md5digest(const char* filepath, const char* md5file, int append);
int verify_md5digest(const char* filepath, const char* md5file);

// custom zip path + free browse mode + multi zip flash menus
int show_custom_zip_menu();
void set_custom_zip_path();
void show_multi_flash_menu();

// nandroid menu settings
void show_twrp_restore_menu(const char* backup_volume);
void custom_backup_menu(const char* backup_volume);
void custom_restore_menu(const char* backup_volume);
void get_twrp_backup_path(const char* backup_volume, char *backup_path);
void get_cwm_backup_path(const char* backup_volume, char *backup_path);
void misc_nandroid_menu();
void get_rom_name(char *rom_name);
void get_device_id(char *device_id);
void reset_custom_job_settings(int custom_job);

#define MAX_EXTRA_NANDROID_PARTITIONS    5
void reset_extra_partitions_state();
int get_extra_partitions_state();
struct extra_partitions_list {
    char mount_point[PATH_MAX];
    int backup_state;
} extra_partition[MAX_EXTRA_NANDROID_PARTITIONS];

#endif // __ADVANCED_FUNCTIONS_H
