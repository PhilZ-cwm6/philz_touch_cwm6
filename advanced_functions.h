#ifndef __EXTENDEDCOMMANDS_H
#define __EXTENDEDCOMMANDS_H

unsigned long long gettime_nsec();
long long timenow_usec(void);
long long timenow_msec(void);
int is_time_interval_passed(long long msec_interval);

// general system functions
int ensure_directory(const char* path, mode_t mode);
void delete_a_file(const char* filename);
int file_found(const char* filename);
int directory_found(const char* dir);
int is_path_ramdisk(const char* path);
int copy_a_file(const char* file_in, const char* file_out);
int append_string_to_file(const char* filename, const char* string);
char* find_file_in_path(const char* dir, const char* filename, int depth, int follow);
char* read_file_to_buffer(const char* filepath, unsigned long *len);
char* BaseName(const char* path);
char* DirName(const char* path);
char* t_BaseName(const char* path);
char* t_DirName(const char* path);
char* readlink_device_blk(const char* Path);

// case insensitive C-string compare (adapted from titanic-fanatic)
int strcmpi(const char *str1, const char *str2);

// get partition size function (adapted from Dees_Troy - TWRP)
unsigned long long Total_Size; // Overall size of the partition
unsigned long long Used_Size; // Overall used space
unsigned long long Free_Size; // Overall free space
unsigned long Get_File_Size(const char* Path);
unsigned long long Get_Folder_Size(const char* Path);
int Find_Partition_Size(const char* Path);
int Get_Size_Via_statfs(const char* Path);

void set_gather_hidden_files(int enable);

char** gather_files(const char* basedir, const char* fileExtensionOrDirectory, int* numFiles);

int write_string_to_file(const char* filename, const char* string);

void free_string_array(char** array);

void free_array_contents(char** array);

#endif  // __EXTENDEDCOMMANDS_H
