#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include <sys/vfs.h>
#include <libgen.h>

#include <time.h>

#include "libcrecovery/common.h"
#include "common.h"
#include "roots.h"
#include "recovery_ui.h"
#include "mtdutils/mounts.h"
#include "extendedcommands.h"
#include "advanced_functions.h"
#include "recovery_settings.h"

// time using gettimeofday()
// to get time in usec, we call timenow_usec() which will link here if clock_gettime fails
static long long gettime_usec() {
    struct timeval now;
    long long useconds;
    gettimeofday(&now, NULL);
    useconds = (long long)(now.tv_sec) * 1000000LL;
    useconds += (long long)now.tv_usec;
    return useconds;
}

// use clock_gettime for elapsed time
// this is nsec precise + less prone to issues for elapsed time
// unsigned integers cannot be negative (overflow): set error return code to 0 (1.1.1970 00:00)
unsigned long long gettime_nsec() {
    struct timespec ts;
    static int err = 0;

    if (err) return 0;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        LOGI("clock_gettime(CLOCK_MONOTONIC) failed: %s\n", strerror(errno));
        ++err;
        return 0;
    }

    unsigned long long nseconds = (unsigned long long)(ts.tv_sec) * 1000000000ULL;
    nseconds += (unsigned long long)(ts.tv_nsec);
    return nseconds;
}

// returns the current time in usec: 
long long timenow_usec() {
    // try clock_gettime
    unsigned long long nseconds;
    nseconds = gettime_nsec();
    if (nseconds == 0) {
        // LOGI("dropping to gettimeofday()\n");
        return gettime_usec();
    }

    return (long long)(nseconds / 1000ULL);
}

// returns the current time in msec: 
long long timenow_msec() {
    // first try using clock_gettime
    unsigned long long nseconds;
    nseconds = gettime_nsec();
    if (nseconds == 0) {
        // LOGI("dropping to gettimeofday()\n");
        return (gettime_usec() / 1000LL);
    }

    return (long long)(nseconds / 1000000ULL);
}

// check if a specified time interval is passed between 2 calls to this function
// before calling, reset timer by passing 0
// when timer reached, it will return 1 AND will reset the timer
// next call will start again from that point without needing to pass 0 to reset
static long long interval_passed_t_timer = 0;
int is_time_interval_passed(long long msec_interval) {
    long long t = timenow_msec();
    if (msec_interval != 0 && t - interval_passed_t_timer < msec_interval)
        return 0;

    interval_passed_t_timer = t;
    return 1;
}

// basename and dirname implementation that is thread safe, no free and doesn't modify argument
// it is extracted from NDK and modified dirname_r to never modify passed argument
// t_BaseName and t_DirName are threadsafe, but need free by caller
// todo: add error check when returning NULL as it will segfault
int BaseName_r(const char* path, char*  buffer, size_t  bufflen) {
    const char *endp, *startp;
    int len, result;

    /* Empty or NULL string gets treated as "." */
    if (path == NULL || *path == '\0') {
        startp  = ".";
        len     = 1;
        goto Exit;
    }

    /* Strip trailing slashes */
    endp = path + strlen(path) - 1;
    while (endp > path && *endp == '/') {
        endp--;
    }

    /* All slashes becomes "/" */
    if (endp == path && *endp == '/') {
        startp = "/";
        len    = 1;
        goto Exit;
    }

    /* Find the start of the base */
    startp = endp;
    while (startp > path && *(startp - 1) != '/') {
        startp--;
    }

    len = endp - startp +1;

Exit:
    result = len;
    if (buffer == NULL) {
        return result;
    }
    if (len > (int)bufflen-1) {
        len    = (int)bufflen-1;
        result = -1;
        errno  = ERANGE;
    }

    if (len >= 0) {
        memcpy( buffer, startp, len );
        buffer[len] = 0;
    }
    return result;
}

// thread unsafe
char* BaseName(const char* path) {
    static char* bname = NULL;
    int ret;

    if (bname == NULL) {
        bname = (char*)malloc(PATH_MAX);
        if (bname == NULL)
            return(NULL);
    }
    ret = BaseName_r(path, bname, PATH_MAX);
    return (ret < 0) ? NULL : bname;
}

int DirName_r(const char*  path, char*  buffer, size_t  bufflen) {
    const char *endp, *startp;
    int result, len;

    /* Empty or NULL string gets treated as "." */
    if (path == NULL || *path == '\0') {
        startp = ".";
        len  = 1;
        goto Exit;
    }

    /* Strip trailing slashes */
    endp = path + strlen(path) - 1;
    while (endp > path && *endp == '/') {
        endp--;
    }

    /* Find the start of the dir */
    while (endp > path && *endp != '/') {
        endp--;
    }

    /* Either the dir is "/" or there are no slashes */
    if (endp == path) {
        startp = (*endp == '/') ? "/" : ".";
        len  = 1;
        goto Exit;
    }

    do {
        endp--;
    } while (endp > path && *endp == '/');

    startp = path;
    len = endp - startp +1;

Exit:
    result = len;
    if (len+1 > PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (buffer == NULL)
        return result;

    if (len > (int)bufflen-1) {
        len    = (int)bufflen-1;
        result = -1;
        errno  = ERANGE;
    }

    if (len >= 0) {
        memcpy( buffer, startp, len );
        buffer[len] = 0;
    }
    return result;
}

// thread unsafe
char* DirName(const char* path) {
    static char*  bname = NULL;
    int ret;

    if (bname == NULL) {
        bname = (char*)malloc(PATH_MAX);
        if (bname == NULL)
            return(NULL);
    }

    ret = DirName_r(path, bname, PATH_MAX);
    return (ret < 0) ? NULL : bname;
}

// thread safe dirname (free by caller)
char* t_DirName(const char* path) {
    int ret;
    char* bname = (char*)malloc(PATH_MAX);
    if (bname == NULL) {
        LOGE("t_DirName: memory error\n");
        return NULL;
    }

    ret = DirName_r(path, bname, PATH_MAX);
    if (ret < 0) {
        LOGE("t_DirName: error\n");
        return NULL;
    }
    
    return bname;
}

// thread safe basename (free by caller)
char* t_BaseName(const char* path) {
    int ret;
    char* bname = (char*)malloc(PATH_MAX);
    if (bname == NULL) {
        LOGE("t_BaseName: memory error\n");
        return NULL;
    }

    ret = BaseName_r(path, bname, PATH_MAX);
    if (ret < 0) {
        LOGE("t_BaseName: error\n");
        return NULL;
    }
    
    return bname;
}

// case insensitive C-string compare (adapted from titanic-fanatic)
int strcmpi(const char *str1, const char *str2) {
    int i = 0;
    int ret = 0;

    while (ret == 0 && str1[i] && str2[i]) {
        ret = tolower(str1[i]) - tolower(str2[i]);
        ++i;
    }

    return ret;
}

// delete a file
void delete_a_file(const char* filename) {
    ensure_path_mounted(filename);
    remove(filename);
}

// search for 'file' in 'dir': only last occurrence is returned if many!
// depth <= 0: no depth limit
// follow != 0: follow links
char* find_file_in_path(const char* dir, const char* filename, int depth, int follow) {
    FILE *fp;
    char* ret = NULL;
    char buff[PATH_MAX];
    char cmd[PATH_MAX];
    char suffix[2] = "";
    char options[64] = "";
    if (dir[strlen(dir) - 1] != '/')
        strcpy(suffix, "/");
    if (depth > 0)
        sprintf(options, " -maxdepth %d", depth);
    if (follow)
        strcat(options, " -follow");

    sprintf(cmd, "find %s%s%s -name '%s'", dir, suffix, options, filename);
    fp = __popen(cmd, "r");
    if (fp == NULL){
        return ret;
    }

    while (fgets(buff, sizeof(buff), fp) != NULL) {
        size_t len = strlen(buff);
        if (buff[len - 1] == '\n')
            buff[len - 1] = '\0';
        ret = strdup(buff);
    }

    __pclose(fp);
    return ret;
}

// check if file or folder exists
int file_found(const char* filename) {
    struct stat s;
    if (strncmp(filename, "/sbin/", 6) != 0 && strncmp(filename, "/res/", 5) != 0 && strncmp(filename, "/tmp/", 5) != 0) {
        // do not try to mount ramdisk, else it will error "unknown volume for path..."
        ensure_path_mounted(filename);
    }
    if (0 == lstat(filename, &s))
        return 1;

    return 0;
}

// check directory exists
int directory_found(const char* dir) {
    struct stat s;
    ensure_path_mounted(dir);
    lstat(dir, &s);
    if (S_ISDIR(s.st_mode))
        return 1;

    return 0;
}

int ensure_directory(const char* path, mode_t mode) {
    char dir[PATH_MAX] = "";
    char* p;
    size_t len = 0;
    size_t i = 0;

    if (path[0] != '/') {
        LOGE("mkdir_p: no full path specified (%s)\n", path); // caller error!
        return -1;
    }

    // remove successive '/' symbols
    for (len = 0; len < strlen(path); ++len) {
        if (path[len] == path[len + 1] && path[len] == '/')
            continue;

        dir[i] = path[len];
        ++i;
    }

    // create path recursively
    for (p = strchr(&dir[1], '/'); p != NULL; p = strchr(p+1, '/')) {
        *p = '\0';
        if (mkdir(dir, mode) != 0 && errno != EEXIST) {
            return -1;
        }
        *p = '/';
    }
    if (mkdir(dir, mode) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

// check if path is in ramdisk since volume_for_path() will be NULL on these
int is_path_ramdisk(const char* path) {
    const char *ramdisk_dirs[] = { "/sbin/", "/res/", "/tmp/", NULL };
    int i = 0;
    while (ramdisk_dirs[i] != NULL) {
        if (strncmp(path, ramdisk_dirs[i], strlen(ramdisk_dirs[i])) == 0)
            return 1;
        i++;
    }
    return 0;
}

// copy file (ramdisk check compatible)
int copy_a_file(const char* file_in, const char* file_out) {
    if (strcmp(file_in, file_out) == 0) {
        LOGI("source and destination files are same, skipping copy.\n");
        return 0;
    }

    if (!is_path_ramdisk(file_in) && ensure_path_mounted(file_in) != 0) {
        LOGE("copy: cannot mount volume %s\n", file_in);
        return -1;
    }

    if (!is_path_ramdisk(file_out) && ensure_path_mounted(file_out) != 0) {
        LOGE("copy: cannot mount volume %s\n", file_out);
        return -1;
    }

    char tmp[PATH_MAX];
    sprintf(tmp, "%s", DirName(file_out));
    ensure_directory(tmp, 0755);
    FILE *fp = fopen(file_in, "rb");
    if (fp == NULL) {
        LOGE("copy: source file not found (%s)\n", file_in);
        return -1;
    }
    FILE *fp_out = fopen(file_out, "wb");
    if (fp_out == NULL) {
        LOGE("copy: failed to create destination file %s\n", file_out);
        fclose(fp);
        return -1;
    }

    // start copy
    size_t size;
    while ((size = fread(tmp, 1, sizeof(tmp), fp))) {
        fwrite(tmp, 1, size, fp_out);
    }
    fclose(fp);
    fclose(fp_out);
    return 0;
}

// append a string to text file, create the directories and file if it doesn't exist
int append_string_to_file(const char* filename, const char* string) {
    char tmp[PATH_MAX];
    int ret = -1;

    ensure_path_mounted(filename);
    mkdir(DirName(filename), S_IRWXU | S_IRWXG | S_IRWXO);

    FILE *file = fopen(filename, "a");
    if (file != NULL) {
        ret = fprintf(file, "%s", string);
        fclose(file);
    } else
        LOGE("Cannot append to %s\n", filename);

    return ret;
}

// get file size (by Dees_Troy - TWRP)
// needs ensure_path_mounted(Path) by caller
unsigned long Get_File_Size(const char* Path) {
    struct stat st;
    if (stat(Path, &st) != 0)
        return 0;
    return st.st_size;
}

// get partition size info (adapted from Dees_Troy - TWRP)
unsigned long long Total_Size = 0; // Overall size of the partition
unsigned long long Used_Size = 0; // Overall used space
unsigned long long Free_Size = 0; // Overall free space

// this needs the volume to be mounted by caller
// do not ensure path mounted here to avoid time loss when refreshing backup size during nandroid operations
int Get_Size_Via_statfs(const char* Path) {
    struct statfs st;
    Volume* volume;
    if (is_data_media_volume_path(Path))
        volume = volume_for_path("/data");
    else
        volume = volume_for_path(Path);
    if (NULL == volume) {
        LOGE("No volume found to get size from '%s'\n", Path);
        return -1;
    }

    if (volume->mount_point == NULL || statfs(volume->mount_point, &st) != 0) {
        LOGE("Unable to statfs for size '%s'\n", Path);
        return -1;
    }

    Total_Size = (unsigned long long)(st.f_blocks) * (unsigned long long)(st.f_bsize);
    Free_Size = (unsigned long long)(st.f_bfree) * (unsigned long long)(st.f_bsize);
    Used_Size = Total_Size - Free_Size;
    // LOGI("%s: tot=%llu, use=%llu, free=%llu\n", volume->mount_point, Total_Size, Used_Size, Free_Size); // debug
    return 0;
}

// alternate method for statfs (emmc, mtd, mtk...)
int Find_Partition_Size(const char* Path) {
    char line[512];
    char tmpdevice[1024];
    FILE* fp;
    Volume* volume;

    if (is_data_media_volume_path(Path))
        volume = volume_for_path("/data");
    else
        volume = volume_for_path(Path);

    if (volume == NULL) {
        LOGE("Failed to find partition size '%s'\n", Path);
        LOGE("  > invalid volume %s\n", Path);
        return -1;
    }

    // In this case, we'll first get the partitions we care about (with labels)
/*
    --> Start by checking if it is an MTK based device (cat /proc/dumchar_info)
    Part_Name    Size               StartAddr         Type   MapTo
    preloader    0x0000000000040000 0x0000000000000000   2   /dev/misc-sd
    dsp_bl       0x00000000005c0000 0x0000000000040000   2   /dev/misc-sd
    mbr          0x0000000000004000 0x0000000000000000   2   /dev/block/mmcblk0
    ebr1         0x0000000000004000 0x0000000000004000   2   /dev/block/mmcblk0p1
    pmt          0x0000000000400000 0x0000000000008000   2   /dev/block/mmcblk0
    nvram        0x0000000000500000 0x0000000000408000   2   /dev/block/mmcblk0
    seccfg       0x0000000000020000 0x0000000000908000   2   /dev/block/mmcblk0
    uboot        0x0000000000060000 0x0000000000928000   2   /dev/block/mmcblk0
    bootimg      0x0000000000600000 0x0000000000988000   2   /dev/block/mmcblk0
    recovery     0x0000000000600000 0x0000000000f88000   2   /dev/block/mmcblk0
    sec_ro       0x0000000000600000 0x0000000001588000   2   /dev/block/mmcblk0p2
    misc         0x0000000000060000 0x0000000001b88000   2   /dev/block/mmcblk0
    logo         0x0000000000300000 0x0000000001be8000   2   /dev/block/mmcblk0
    expdb        0x0000000000200000 0x0000000001ee8000   2   /dev/block/mmcblk0
    android      0x0000000020100000 0x00000000020e8000   2   /dev/block/mmcblk0p3
    cache        0x0000000020100000 0x00000000221e8000   2   /dev/block/mmcblk0p4
    usrdata      0x0000000020100000 0x00000000422e8000   2   /dev/block/mmcblk0p5
    fat          0x00000000854f8000 0x00000000623e8000   2   /dev/block/mmcblk0p6
    bmtpool      0x0000000001500000 0x00000000ff9f00a8   2   /dev/block/mmcblk0
    Part_Name:Partition name you should open;
    Size:size of partition
    StartAddr:Start Address of partition;
    Type:Type of partition(MTD=1,EMMC=2)
    MapTo:actual device you operate
*/
    fp = fopen("/proc/dumchar_info", "rt");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            char label[32], device[32];
            unsigned long size = 0;

            sscanf(line, "%s %lx %*x %*u %s", label, &size, device);

            // Skip header, annotation  and blank lines
            if ((strncmp(device, "/dev/", 5) != 0) || (strlen(line) < 8))
                continue;

            sprintf(tmpdevice, "/dev/");
            strcat(tmpdevice, label);
            if (volume->blk_device != NULL && strcmp(tmpdevice, volume->blk_device) == 0) {
                Total_Size = size;
                fclose(fp);
                return 0;
            }
            if (volume->blk_device2 != NULL && strcmp(tmpdevice, volume->blk_device2) == 0) {
                Total_Size = size;
                fclose(fp);
                return 0;
            }
        }

        fclose(fp);
    }

/*  It is not an MTK device entry:
    --> Try mtd / emmc devices (cat /proc/partitions):
    major minor #blocks name
    179  0 15388672 mmcblk0
    179  1    65536 mmcblk0p1
    179  2      512 mmcblk0p2
    179  3      512 mmcblk0p3
    179  4     2048 mmcblk0p4
    179  5      512 mmcblk0p5
    179  6    22528 mmcblk0p6
    179  7    22528 mmcblk0p7
*/
    int ret = -1;
    fp = fopen("/proc/partitions", "rt");
    if (fp != NULL) {
        // try to read blk_device link target for devices not using /dev/block/xxx in recovery.fstab
        char* mmcblk_from_link = readlink_device_blk(Path);
        while (ret && fgets(line, sizeof(line), fp) != NULL) {
            unsigned long major, minor, blocks;
            char device[512];

            if (strlen(line) < 7 || line[0] == 'm')
                continue;

            sscanf(line + 1, "%lu %lu %lu %s", &major, &minor, &blocks, device);
            sprintf(tmpdevice, "/dev/block/");
            strcat(tmpdevice, device);

            if (volume->blk_device != NULL && strcmp(tmpdevice, volume->blk_device) == 0) {
                Total_Size = blocks * 1024ULL;
                //LOGI("%s(%s)=%llu\n", Path, volume->blk_device, Total_Size); // debug
                ret = 0;
            } else if (volume->blk_device2 != NULL && strcmp(tmpdevice, volume->blk_device2) == 0) {
                Total_Size = blocks * 1024ULL;
                ret = 0;
            } else if (mmcblk_from_link != NULL && strcmp(tmpdevice, mmcblk_from_link) == 0) {
                Total_Size = blocks * 1024ULL;
                ret = 0;
            }
        }

        fclose(fp);
        if (mmcblk_from_link != NULL)
            free(mmcblk_from_link);
    }

    if (ret != 0)
        LOGE("Failed to find partition size '%s'\n", Path);
    return ret;
}
//----- End partition size

// get folder size (by Dees_Troy - TWRP)
// size of /data will include /data/media, so needs to be calculated by caller
// always ensure_path_mounted(Path) before calling it
// do not ensure_path_mounted(Path) here to avoid calling it on each opendir(subfolder)
unsigned long long Get_Folder_Size(const char* Path) {
    DIR* d;
    struct dirent* de;
    struct stat st;
    char path2[PATH_MAX]; char filename[PATH_MAX];
    unsigned long long dusize = 0;
    unsigned long long dutemp = 0;

    // Make a copy of path in case the data in the pointer gets overwritten later
    strcpy(path2, Path);

    d = opendir(path2);
    if (d == NULL) {
        LOGE("error opening '%s'\n", path2);
        LOGE("error: %s\n", strerror(errno));
        return 0;
    }

    while ((de = readdir(d)) != NULL) {
        if (de->d_type == DT_DIR && strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
            strcpy(filename, path2);
            strcat(filename, "/");
            strcat(filename, de->d_name);
            dutemp = Get_Folder_Size(filename);
            dusize += dutemp;
            dutemp = 0;
        } else if (de->d_type == DT_REG) {
            strcpy(filename, path2);
            strcat(filename, "/");
            strcat(filename, de->d_name);
            stat(filename, &st);
            dusize += (unsigned long long)(st.st_size);
        }
    }
    closedir(d);
    return dusize;
}

/*
Read a file to buffer: buffer must be freed by caller if return value is not NULL.
len is the total bytes we read if successful
That way we can use fwrite(str, len, fd) to copy the file
To use it as string argument, we must add terminating null by caller: buffer[len] = '\0';
Exp:
    unsigned long len = 0;
    char* buffer = read_file_to_buffer(md5file, &len);
    if (buffer != NULL) {
        buffer[len] = '\0';
        printf("buffer=%s\n", buffer);
        free(buffer);
    }
*/
char* read_file_to_buffer(const char* filepath, unsigned long *len) {
    if (!file_found(filepath)) {
        LOGE("read_file_to_buffer: '%s' not found\n", filepath);
        return NULL;
    }

    // obtain file size:
    unsigned long size = Get_File_Size(filepath);

    // allocate memory to contain the whole file:
    char* buffer = (char*)malloc(size + 1);
    if (buffer == NULL) {
        LOGE("read_file_to_buffer: memory error\n");
        return NULL;
    }

    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        LOGE("read_file_to_buffer: can't open '%s'\n", filepath);
        free(buffer);
        return NULL;
    }

    // copy the file into the buffer:
    *len = fread(buffer, 1, size, file);
    if (size != *len) {
        LOGE("read_file_to_buffer: read error\n");
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return buffer;
}

// returns negative value on failure and total bytes written on success
int write_string_to_file(const char* filename, const char* string) {
    char tmp[PATH_MAX];
    int ret = -1;

    ensure_path_mounted(filename);
    sprintf(tmp, "mkdir -p $(dirname %s)", filename);
    __system(tmp);
    FILE *file = fopen(filename, "w");
    if (file != NULL) {
        ret = fprintf(file, "%s", string);
        fclose(file);
    } else
        LOGE("Cannot write to %s\n", filename);

    return ret;
}

// try to resolve link from blk_device to real /dev/block/mmcblk or /dev/block/mtdblock
// free by caller
char* readlink_device_blk(const char* Path)
{
    char* mmcblk_from_link = NULL;
    Volume* vol;
    if (is_data_media_volume_path(Path))
        vol = volume_for_path("/data");
    else
        vol = volume_for_path(Path);
    if (vol == NULL)
        return NULL;

    char buf[1024];
    ssize_t len = readlink(vol->blk_device, buf, sizeof(buf)-1);
    if (len == -1) {
        // LOGI("failed to get device mmcblk link '%s'\n", vol->blk_device);
        return NULL;
    }

    buf[len] = '\0';
    mmcblk_from_link = strdup(buf);
    if (mmcblk_from_link == NULL)
        LOGE("readlink_device_blk: memory error\n");
    else
        LOGI("found device mmcblk link: '%s' -> '%s'\n", vol->blk_device, mmcblk_from_link);

    return mmcblk_from_link;
}

void free_array_contents(char** array) {
    if (array == NULL)
        return;

    char* cursor = array[0];
    int i = 0;
    while (cursor != NULL) {
        free(cursor);
        cursor = array[++i];
    }
}

void free_string_array(char** array) {
    if (array == NULL)
        return;

    char* cursor = array[0];
    int i = 0;
    while (cursor != NULL) {
        free(cursor);
        cursor = array[++i];
    }
    free(array);
}

// to gather directories you need to pass NULL for fileExtensionOrDirectory
// else, only files are gathered. Pass "" to gather all files
// NO  MORE NEEDED: if it is not called by choose_file_menu(), passed directory MUST end with a trailing /
static int gather_hidden_files = 0;
void set_gather_hidden_files(int enable) {
    gather_hidden_files = enable;
}

char** gather_files(const char* basedir, const char* fileExtensionOrDirectory, int* numFiles) {
    DIR *dir;
    struct dirent *de;
    int total = 0;
    int i;
    char** files = NULL;
    int pass;
    *numFiles = 0;
    int dirLen = strlen(basedir);
    char directory[PATH_MAX];

    // Append a trailing slash if necessary
    strcpy(directory, basedir);
    if (directory[dirLen - 1] != '/') {
        strcat(directory, "/");
        ++dirLen;
    }

    dir = opendir(directory);
    if (dir == NULL) {
        ui_print("Couldn't open directory %s\n", directory);
        return NULL;
    }

    unsigned int extension_length = 0;
    if (fileExtensionOrDirectory != NULL)
        extension_length = strlen(fileExtensionOrDirectory);

    i = 0;
    // first pass (pass==0) only returns "total" valid file names to initialize files[total] size
    // second pass (pass == 1), rewinddir and initializes files[i] with directory contents
    for (pass = 0; pass < 2; pass++) {
        while ((de = readdir(dir)) != NULL) {
            // skip hidden files
            if (!gather_hidden_files && de->d_name[0] == '.')
                continue;

            // NULL means that we are gathering directories, so skip this
            if (fileExtensionOrDirectory != NULL) {
                if (strcmp("", fileExtensionOrDirectory) == 0) {
                    // we exclude directories since they are gathered on second call to gather_files() by choose_file_menu()
                    // and we keep stock behavior: folders are gathered only by passing NULL
                    // else, we break things at strcat(files[i], "/") in end of while loop
                    struct stat info;
                    char fullFileName[PATH_MAX];
                    strcpy(fullFileName, directory);
                    strcat(fullFileName, de->d_name);
                    lstat(fullFileName, &info);
                    // make sure it is not a directory
                    if (S_ISDIR(info.st_mode))
                        continue;
                } else {
                    // make sure that we can have the desired extension (prevent seg fault)
                    if (strlen(de->d_name) < extension_length)
                        continue;
                    // compare the extension
                    if (strcmp(de->d_name + strlen(de->d_name) - extension_length, fileExtensionOrDirectory) != 0)
                        continue;
                }
            } else {
                struct stat info;
                char fullFileName[PATH_MAX];
                strcpy(fullFileName, directory);
                strcat(fullFileName, de->d_name);
                lstat(fullFileName, &info);
                // make sure it is a directory
                if (!(S_ISDIR(info.st_mode)))
                    continue;
            }

            if (pass == 0) {
                total++;
                continue;
            }

            // only second pass (pass==1) reaches here: initializes files[i] with directory contents
            files[i] = (char*)malloc(dirLen + strlen(de->d_name) + 2);
            strcpy(files[i], directory);
            strcat(files[i], de->d_name);
            if (fileExtensionOrDirectory == NULL)
                strcat(files[i], "/");
            i++;
        }
        if (pass == 1)
            break;
        if (total == 0)
            break;
        // only first pass (pass == 0) reaches here. We rewinddir for second pass
        // initialize "total" with number of valid files to show and initialize files[total]
        rewinddir(dir);
        *numFiles = total;
        files = (char**)malloc((total + 1) * sizeof(char*));
        files[total] = NULL;
    }

    if (closedir(dir) < 0) {
        LOGE("Failed to close directory.\n");
    }

    if (total == 0) {
        return NULL;
    }

    // sort the result
    if (files != NULL) {
        for (i = 0; i < total; i++) {
            int curMax = -1;
            int j;
            for (j = 0; j < total - i; j++) {
                if (curMax == -1 || strcmpi(files[curMax], files[j]) < 0)
                    curMax = j;
            }
            char* temp = files[curMax];
            files[curMax] = files[total - i - 1];
            files[total - i - 1] = temp;
        }
    }

    return files;
}
