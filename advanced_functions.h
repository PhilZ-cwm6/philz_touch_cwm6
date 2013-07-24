/* Start PhilZ Settings */

// print custom logtail (detailed logging report in raw-backup.sh...)
void ui_print_custom_logtail(const char* filename, int nb_lines);
void show_philz_settings();
void wipe_data_menu();
extern int no_files_found;

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
extern int check_for_script_file(const char* ors_boot_script);
extern int run_ors_script(const char* ors_script);

// general system functions
unsigned long Get_File_Size(const char* Path);
unsigned long long Get_Folder_Size(const char* Path);
int file_found(const char* filename);
int directory_found(const char* dir);
void delete_a_file(const char* filename);
void ensure_directory(const char* dir);
//---------- End PhilZ Settings
