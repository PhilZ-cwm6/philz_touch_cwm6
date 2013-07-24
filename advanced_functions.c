#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>

// statfs
#include <sys/vfs.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "make_ext4fs.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "extendedcommands.h"
#include "advanced_functions.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include <libgen.h>
#include "mtdutils/mtdutils.h"
#include "bmlutils/bmlutils.h"
#include "cutils/android_reboot.h"


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*       Start PhilZ Menu settings       */
/*      Code written by PhilZ@xda     */
/*      Part of PhilZ Touch Recovery     */
/*****************************************/

// redefined MENU_MAX_COLS from ui.c - Keep same value as ui.c until a better implementation.
// used to format toggle menus to device screen width (only touch build)
#define MENU_MAX_COLS 64

static int browse_for_file = 1;

//start print tail from custom log file
void ui_print_custom_logtail(const char* filename, int nb_lines) {
    char * backup_log;
    char tmp[PATH_MAX];
    FILE * f;
    int line=0;
    sprintf(tmp, "tail -n %d %s > /tmp/custom_tail.log", nb_lines, filename);
    __system(tmp);
    f = fopen("/tmp/custom_tail.log", "rb");
    if (f != NULL) {
        while (line < nb_lines) {
            backup_log = fgets(tmp, PATH_MAX, f);
            if (backup_log == NULL) break;
            ui_print("%s", tmp);
            line++;
        }
        fclose(f);
    } else
        LOGE("Cannot open /tmp/custom_tail.log\n");
}

// is there a second storage (/sdcard is always present in fstab)
int has_second_storage() {
    return (volume_for_path("/external_sd") != NULL
                || volume_for_path("/emmc") != NULL);
}

// delete a file
void delete_a_file(const char* filename) {
    ensure_path_mounted(filename);
    remove(filename);
}

// check if file or folder exists
int file_found(const char* filename) {
    struct stat s;
    if (strncmp(filename, "/sbin/", 6) != 0 && strncmp(filename, "/res/", 5) != 0 &&
            strncmp(filename, "/tmp/", 5) != 0) {
        // do not try to mount ramdisk, else it will error "unknown volume for path..."
        ensure_path_mounted(filename);
    }
    if (0 == stat(filename, &s))
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

// get file size (by Dees_Troy - TWRP)
unsigned long Get_File_Size(const char* Path) {
    struct stat st;
    if (stat(Path, &st) != 0)
        return 0;
    return st.st_size;
}

// get partition size info (adapted from Dees_Troy - TWRP)
unsigned long long Total_Size; // Overall size of the partition
unsigned long long Used_Size; // Overall used space
unsigned long long Free_Size; // Overall free space

int Get_Size_Via_statfs(const char* Path) {
    struct statfs st;
    Volume* volume = volume_for_path(Path);
    if (NULL == volume) {
        LOGE("Cannot get size of null volume '%s'\n", Path);
        return -1;
    }
    if (is_data_media_volume_path(volume->mount_point))
        volume = volume_for_path("/data");
    if (volume == NULL || volume->mount_point == NULL || statfs(volume->mount_point, &st) != 0) {
        LOGE("Unable to statfs for size '%s'\n", Path);
        return -1;
    }

    Total_Size = (unsigned long long)(st.f_blocks) * (unsigned long long)(st.f_bsize);
    Free_Size = (unsigned long long)(st.f_bfree) * (unsigned long long)(st.f_bsize);
    Used_Size = Total_Size - Free_Size;
    // LOGI("%s: tot=%llu, use=%llu, free=%llu\n", volume->mount_point, Total_Size, Used_Size, Free_Size); // debug
    return 0;
}

// alternate method for statfs (emmc, mtd...)
int Find_Partition_Size(const char* Path) {
    FILE* fp;
    char line[512];
    char tmpdevice[1024];

    Volume* volume = volume_for_path(Path);
    if (volume != NULL) {
        // In this case, we'll first get the partitions we care about (with labels)
        fp = fopen("/proc/partitions", "rt");
        if (fp != NULL) {
            while (fgets(line, sizeof(line), fp) != NULL)
            {
                unsigned long major, minor, blocks;
                char device[512];
                char tmpString[64];

                if (strlen(line) < 7 || line[0] == 'm')
                    continue;

                sscanf(line + 1, "%lu %lu %lu %s", &major, &minor, &blocks, device);
                sprintf(tmpdevice, "/dev/block/");
                strcat(tmpdevice, device);

                if (volume->device != NULL && strcmp(tmpdevice, volume->device) == 0) {
                    Total_Size = blocks * 1024ULL;
                    //LOGI("%s(%s)=%llu\n", Path, volume->device, Total_Size); // debug
                    fclose(fp);
                    return 0;
                }
                if (volume->device2 != NULL && strcmp(tmpdevice, volume->device2) == 0) {
                    Total_Size = blocks * 1024ULL;
                    fclose(fp);
                    return 0;
                }
            }
            fclose(fp);
        }
    }

    LOGE("Failed to find partition size '%s'\n", Path);
    return -1;
}
//----- End partition size

// get folder size (by Dees_Troy - TWRP)
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
    if (d == NULL)
    {
        LOGE("error opening '%s'\n", path2);
        LOGE("error: %s\n", strerror(errno));
        return 0;
    }

    while ((de = readdir(d)) != NULL)
    {
        if (de->d_type == DT_DIR && strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
        {
            strcpy(filename, path2);
            strcat(filename, "/");
            strcat(filename, de->d_name);
            dutemp = Get_Folder_Size(filename);
            dusize += dutemp;
            dutemp = 0;
        }
        else if (de->d_type == DT_REG)
        {
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

// start wipe data and system options and menu
void wipe_data_menu() {
    static char* headers[] = {  "Choose wipe option",
                                NULL
    };

    char* list[] = { "Wipe Data/Factory Reset",
                    "Clean to Install a New ROM",
                    NULL
    };

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            wipe_data(1);
            break;
        case 1:
            //clean for new ROM: formats /data, /datadata, /cache, /system, /preload, /sd-ext, .android_secure
            if (confirm_selection("Wipe data, system +/- preload?", "Yes, I will install a new ROM!")) {
                wipe_data(0);
                ui_print("-- Wiping system...\n");
                erase_volume("/system");
                if (volume_for_path("/preload") != NULL) {
                    ui_print("-- Wiping preload...\n");
                    erase_volume("/preload");
                }
                ui_print("Now flash a new ROM!\n");
            }
            break;
    }
}


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*      Start Multi-Flash Zip code       */
/*      Original code by PhilZ @xda      */
/*****************************************/

#define MULTI_ZIP_FOLDER "clockworkmod/multi_flash"
void show_multi_flash_menu() {
    static char* headers_dir[] = { "Choose a set of zip files",
                                   NULL
    };
    static char* headers[] = {  "Select files to install...",
                                NULL
    };

    //browse sdcards until a valid multi_flash folder is found
    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL)
        other_sd = "/emmc";
    else if (volume_for_path("/external_sd") != NULL)
        other_sd = "/external_sd";
    
    char tmp[PATH_MAX];
    char* zip_folder = NULL;

    //look for MULTI_ZIP_FOLDER in /sdcard
    struct stat st;
    ensure_path_mounted("/sdcard");
    sprintf(tmp, "/sdcard/%s/", MULTI_ZIP_FOLDER);
    stat(tmp, &st);
    if (S_ISDIR(st.st_mode)) {
        zip_folder = choose_file_menu(tmp, NULL, headers_dir);
        // zip_folder = NULL if no subfolders found or user chose Go Back
        if (no_files_found) {
            ui_print("At least one subfolder with zip files must be created under %s\n", tmp);
            ui_print("Looking in other sd...\n");
        }
    } else
        LOGI("%s not found. Searching other sd...\n", tmp);

    // case MULTI_ZIP_FOLDER not found, or no subfolders or user selected Go Back:
    // search for MULTI_ZIP_FOLDER in other_sd
    struct stat s;
    if (other_sd != NULL) {
        ensure_path_mounted(other_sd);
        sprintf(tmp, "%s/%s/", other_sd, MULTI_ZIP_FOLDER);
        stat(tmp, &s);
        if (zip_folder == NULL && S_ISDIR(s.st_mode)) {
            zip_folder = choose_file_menu(tmp, NULL, headers_dir);
            if (no_files_found)
                ui_print("At least one subfolder with zip files must be created under %s\n", tmp);
        }
    }

    // either MULTI_ZIP_FOLDER path not found (ui_print help)
    // or it was found but no subfolder (ui_print help above in no_files_found)
    // or user chose Go Back every time: return silently
    if (zip_folder == NULL) {
        if (!(S_ISDIR(st.st_mode)) && !(S_ISDIR(s.st_mode)))
            ui_print("Create at least 1 folder with your zip files under %s\n", MULTI_ZIP_FOLDER);
        return;
    }

    //gather zip files list
    int dir_len = strlen(zip_folder);
    int numFiles = 0;
    char** files = gather_files(zip_folder, ".zip", &numFiles);
    if (numFiles == 0) {
        ui_print("No zip files found under %s\n", zip_folder);
    } else {
        // start showing multi-zip menu
        char** list = (char**) malloc((numFiles + 3) * sizeof(char*));
        list[0] = strdup("Select/Unselect All");
        list[1] = strdup(">> Flash Selected Files <<");
        list[numFiles+2] = NULL; // Go Back Menu

        int i;
        for(i=2; i < numFiles+2; i++) {
            list[i] = strdup(files[i-2] + dir_len - 4);
            strncpy(list[i], "(x) ", 4);
        }

        int select_all = 1;
        int chosen_item;
        for (;;)
        {
            chosen_item = get_menu_selection(headers, list, 0, 0);
            if (chosen_item == GO_BACK)
                break;
            if (chosen_item == 1)
                break;
            if (chosen_item == 0) {
                // select / unselect all
                select_all ^= 1;
                for(i=2; i < numFiles+2; i++) {
                    if (select_all) strncpy(list[i], "(x)", 3);
                    else strncpy(list[i], "( )", 3);
                }
            } else if (strncmp(list[chosen_item], "( )", 3) == 0) {
                strncpy(list[chosen_item], "(x)", 3);
            } else if (strncmp(list[chosen_item], "(x)", 3) == 0) {
                strncpy(list[chosen_item], "( )", 3);
            }
        }

        //flashing selected zip files
        if (chosen_item == 1) {
            static char confirm[PATH_MAX];
            sprintf(confirm, "Yes - Install from %s", basename(zip_folder));
            if (confirm_selection("Install selected files?", confirm))
            {
                for(i=2; i < numFiles+2; i++) {
                    if (strncmp(list[i], "(x)", 3) == 0) {
#ifdef PHILZ_TOUCH_RECOVERY
                        force_wait = -1;
#endif
                        if (install_zip(files[i-2]) != 0)
                            break;
                    }
                }
            }
        }
        free_string_array(list);
    }
    free_string_array(files);
}
//-------- End Multi-Flash Zip code


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*  Start open recovery script support   */
/*  Original code: Dees_Troy  at yahoo   */
/*  Original cwm port by sk8erwitskil    */
/*  Enhanced by PhilZ @xda               */
/*****************************************/

// check ors script at boot (called from recovery.c)
// format the script file to fix path in install zip commands from goomanager
#define SCRIPT_COMMAND_SIZE 512

int check_for_script_file(const char* ors_boot_script) {
    ensure_path_mounted(ors_boot_script);
    struct stat s;
    if (0 != stat(ors_boot_script, &s))
        return -1;

    char tmp[PATH_MAX];
    LOGI("Script file found: '%s'\n", ors_boot_script);
    __system("/sbin/ors-mount.sh");
    // move script file to /tmp
    sprintf(tmp, "mv %s /tmp", ors_boot_script);
    __system(tmp);

    return 0;
}

// Parse backup options in ors
// Stock CWM as of v6.x, doesn't support backup options
static int ignore_android_secure = 0;

static int ors_backup_command(const char* backup_path, const char* options) {
    is_custom_backup = 1;
    int old_compression_value = compression_value;
    compression_value = TAR_FORMAT;
#ifdef PHILZ_TOUCH_RECOVERY
    int old_enable_md5sum = enable_md5sum;
    enable_md5sum = 1;
#endif
    backup_boot = 0, backup_recovery = 0, backup_wimax = 0, backup_system = 0;
    backup_preload = 0, backup_data = 0, backup_cache = 0, backup_sdext = 0;
    ignore_android_secure = 1; //disable

    ui_print("Setting backup options:\n");
    char value1[SCRIPT_COMMAND_SIZE];
    int line_len, i;
    strcpy(value1, options);
    line_len = strlen(options);
    for (i=0; i<line_len; i++) {
        if (value1[i] == 'S' || value1[i] == 's') {
            backup_system = 1;
            ui_print("System\n");
            if (nandroid_add_preload) {
                backup_preload = 1;
                ui_print("Preload enabled in nandroid settings.\n");
                ui_print("It will be Processed with /system\n");
            }
        } else if (value1[i] == 'D' || value1[i] == 'd') {
            backup_data = 1;
            ui_print("Data\n");
        } else if (value1[i] == 'C' || value1[i] == 'c') {
            backup_cache = 1;
            ui_print("Cache\n");
        } else if (value1[i] == 'R' || value1[i] == 'r') {
            backup_recovery = 1;
            ui_print("Recovery\n");
        } else if (value1[i] == '1') {
            ui_print("%s\n", "Option for special1 ignored in CWMR");
        } else if (value1[i] == '2') {
            ui_print("%s\n", "Option for special2 ignored in CWMR");
        } else if (value1[i] == '3') {
            ui_print("%s\n", "Option for special3 ignored in CWMR");
        } else if (value1[i] == 'B' || value1[i] == 'b') {
            backup_boot = 1;
            ui_print("Boot\n");
        } else if (value1[i] == 'A' || value1[i] == 'a') {
            ignore_android_secure = 0;
            ui_print("Android secure\n");
        } else if (value1[i] == 'E' || value1[i] == 'e') {
            backup_sdext = 1;
            ui_print("SD-Ext\n");
        } else if (value1[i] == 'O' || value1[i] == 'o') {
            compression_value = TAR_GZ_LOW;
            ui_print("Compression is on\n");
        } else if (value1[i] == 'M' || value1[i] == 'm') {
#ifdef PHILZ_TOUCH_RECOVERY
            enable_md5sum = 0;
            ui_print("MD5 Generation is off\n");
#else
            ui_print("Skip md5 check: not supported\n");
#endif
        }
    }

    int ret = -1;
    if (file_found(backup_path)) {
        LOGE("Specified ors backup target '%s' already exists!\n", backup_path);
    } else if (nandroid_get_default_backup_format() != NANDROID_BACKUP_FORMAT_TAR) {
        LOGE("Default backup format must be tar!\n");
    } else if (twrp_backup_mode) {
        ret = twrp_backup(backup_path);
    } else {
        ret = nandroid_backup(backup_path);
    }
    is_custom_backup = 0;
    twrp_backup_mode = 0;
    compression_value = old_compression_value;
    reset_custom_job_settings(0);
#ifdef PHILZ_TOUCH_RECOVERY
    enable_md5sum = old_enable_md5sum;
#endif
    return ret;
}

// run ors script code
// this can be started on boot or manually for custom ors
int run_ors_script(const char* ors_script) {
    FILE *fp = fopen(ors_script, "r");
    int ret_val = 0, cindex, line_len, i, remove_nl;
    char script_line[SCRIPT_COMMAND_SIZE], command[SCRIPT_COMMAND_SIZE],
         value[SCRIPT_COMMAND_SIZE], mount[SCRIPT_COMMAND_SIZE],
         value1[SCRIPT_COMMAND_SIZE], value2[SCRIPT_COMMAND_SIZE];
    char *val_start, *tok;

    if (fp != NULL) {
        while (fgets(script_line, SCRIPT_COMMAND_SIZE, fp) != NULL && ret_val == 0) {
            cindex = 0;
            line_len = strlen(script_line);
            if (line_len < 2)
                continue; // there's a blank line or line is too short to contain a command
            LOGI("script line: '%s'\n", script_line); // debug code
            for (i=0; i<line_len; i++) {
                if ((int)script_line[i] == 32) {
                    cindex = i;
                    i = line_len;
                }
            }
            memset(command, 0, sizeof(command));
            memset(value, 0, sizeof(value));
            if ((int)script_line[line_len - 1] == 10)
                remove_nl = 2;
            else
                remove_nl = 1;
            if (cindex != 0) {
                strncpy(command, script_line, cindex);
                LOGI("command is: '%s' and ", command);
                val_start = script_line;
                val_start += cindex + 1;
                strncpy(value, val_start, line_len - cindex - remove_nl);
                LOGI("value is: '%s'\n", value);
            } else {
                strncpy(command, script_line, line_len - remove_nl + 1);
                ui_print("command is: '%s' and there is no value\n", command);
            }
            if (strcmp(command, "install") == 0) {
                // Install zip
                ui_print("Installing zip file '%s'\n", value);
                ensure_path_mounted("/sdcard");
                if (volume_for_path("/external_sd") != NULL)
                    ensure_path_mounted("/external_sd");
                if (volume_for_path("/emmc") != NULL)
                    ensure_path_mounted("/emmc");
                ret_val = install_zip(value);
                if (ret_val != INSTALL_SUCCESS) {
                    LOGE("Error installing zip file '%s'\n", value);
                    ret_val = 1;
                }
            } else if (strcmp(command, "wipe") == 0) {
                // Wipe
                if (strcmp(value, "cache") == 0 || strcmp(value, "/cache") == 0) {
                    ui_print("-- Wiping Cache Partition...\n");
                    erase_volume("/cache");
                    ui_print("-- Cache Partition Wipe Complete!\n");
                } else if (strcmp(value, "dalvik") == 0 || strcmp(value, "dalvick") == 0 || strcmp(value, "dalvikcache") == 0 || strcmp(value, "dalvickcache") == 0) {
                    ui_print("-- Wiping Dalvik Cache...\n");
                    if (0 != ensure_path_mounted("/data")) {
                        ret_val = 1;
                        break;
                    }
                    ensure_path_mounted("/sd-ext");
                    ensure_path_mounted("/cache");
                    if (no_wipe_confirm) {
                        //do not confirm before wipe for scripts started at boot
                        __system("rm -r /data/dalvik-cache");
                        __system("rm -r /cache/dalvik-cache");
                        __system("rm -r /sd-ext/dalvik-cache");
                        ui_print("Dalvik Cache wiped.\n");
                    } else {
                        if (confirm_selection( "Confirm wipe?", "Yes - Wipe Dalvik Cache")) {
                            __system("rm -r /data/dalvik-cache");
                            __system("rm -r /cache/dalvik-cache");
                            __system("rm -r /sd-ext/dalvik-cache");
                            ui_print("Dalvik Cache wiped.\n");
                        }
                    }
                    ensure_path_unmounted("/data");
                    ui_print("-- Dalvik Cache Wipe Complete!\n");
                } else if (strcmp(value, "data") == 0 || strcmp(value, "/data") == 0 || strcmp(value, "factory") == 0 || strcmp(value, "factoryreset") == 0) {
                    ui_print("-- Wiping Data Partition...\n");
                    wipe_data(0);
                    ui_print("-- Data Partition Wipe Complete!\n");
                } else {
                    LOGE("Error with wipe command value: '%s'\n", value);
                    ret_val = 1;
                }
            } else if (strcmp(command, "backup") == 0) {
                char other_sd[20] = "";
#ifdef PHILZ_TOUCH_RECOVERY
                // read user set volume target
                get_ors_backup_volume(other_sd);
#else
                // if possible, always prefer external storage as backup target
                if (volume_for_path("/external_sd") != NULL && ensure_path_mounted("/external_sd") == 0)
                    strcpy(other_sd, "/external_sd");
                else if (volume_for_path("/sdcard") != NULL && ensure_path_mounted("/sdcard") == 0)
                    strcpy(other_sd, "/sdcard");
                else if (volume_for_path("/emmc") != NULL && ensure_path_mounted("/emmc") == 0)
                    strcpy(other_sd, "/emmc");
#endif
                if (strcmp(other_sd, "") == 0) {
                    ret_val = 1;
                    LOGE("No valid volume found for ors backup target!\n");
                    continue;
                }

                char backup_path[PATH_MAX];
#ifdef PHILZ_TOUCH_RECOVERY
                // Check if ors backup is set by user to twrp mode
                if (twrp_ors_backup_format())
                    twrp_backup_mode = 1;
#endif
                tok = strtok(value, " ");
                strcpy(value1, tok);
                tok = strtok(NULL, " ");
                if (tok != NULL) {
                    // command line has a name for backup folder
                    memset(value2, 0, sizeof(value2));
                    strcpy(value2, tok);
                    line_len = strlen(tok);
                    if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13) {
                        if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13)
                            remove_nl = 2;
                        else
                            remove_nl = 1;
                    } else
                        remove_nl = 0;

                    strncpy(value2, tok, line_len - remove_nl);
                    ui_print("Backup folder set to '%s'\n", value2);
                    if (twrp_backup_mode) {
                        char device_id[PROPERTY_VALUE_MAX];
                        get_device_id(device_id);
                        sprintf(backup_path, "%s/%s/%s/%s", other_sd, TWRP_BACKUP_PATH, device_id, value2);
                    } else {
                        sprintf(backup_path, "%s/clockworkmod/backup/%s", other_sd, value2);
                    }
                } else if (twrp_backup_mode) {
                    get_twrp_backup_path(other_sd, backup_path);
                } else {
                    get_custom_backup_path(other_sd, backup_path);
                }
                if (0 != (ret_val = ors_backup_command(backup_path, value1)))
                    ui_print("Backup failed !!\n");
            } else if (strcmp(command, "restore") == 0) {
                // Restore
                tok = strtok(value, " ");
                strcpy(value1, tok);
                ui_print("Restoring '%s'\n", value1);

                // custom restore settings
                is_custom_backup = 1;
#ifdef PHILZ_TOUCH_RECOVERY
                int old_enable_md5sum = enable_md5sum;
                enable_md5sum = 1;
#endif
                backup_boot = 0, backup_recovery = 0, backup_system = 0;
                backup_preload = 0, backup_data = 0, backup_cache = 0, backup_sdext = 0;
                ignore_android_secure = 1; //disable

                // check what type of restore we need
                if (strstr(value1, TWRP_BACKUP_PATH) != NULL)
                    twrp_backup_mode = 1;

                tok = strtok(NULL, " ");
                if (tok != NULL) {
                    memset(value2, 0, sizeof(value2));
                    strcpy(value2, tok);
                    ui_print("Setting restore options:\n");
                    line_len = strlen(value2);
                    for (i=0; i<line_len; i++) {
                        if (value2[i] == 'S' || value2[i] == 's') {
                            backup_system = 1;
                            ui_print("System\n");
                            if (nandroid_add_preload) {
                                backup_preload = 1;
                                ui_print("Preload enabled in nandroid settings.\n");
                                ui_print("It will be Processed with /system\n");
                            }
                        } else if (value2[i] == 'D' || value2[i] == 'd') {
                            backup_data = 1;
                            ui_print("Data\n");
                        } else if (value2[i] == 'C' || value2[i] == 'c') {
                            backup_cache = 1;
                            ui_print("Cache\n");
                        } else if (value2[i] == 'R' || value2[i] == 'r') {
                            backup_recovery = 1;
                            ui_print("Recovery\n");
                        } else if (value2[i] == '1') {
                            ui_print("%s\n", "Option for special1 ignored in CWMR");
                        } else if (value2[i] == '2') {
                            ui_print("%s\n", "Option for special2 ignored in CWMR");
                        } else if (value2[i] == '3') {
                            ui_print("%s\n", "Option for special3 ignored in CWMR");
                        } else if (value2[i] == 'B' || value2[i] == 'b') {
                            backup_boot = 1;
                            ui_print("Boot\n");
                        } else if (value2[i] == 'A' || value2[i] == 'a') {
                            ignore_android_secure = 0;
                            ui_print("Android secure\n");
                        } else if (value2[i] == 'E' || value2[i] == 'e') {
                            backup_sdext = 1;
                            ui_print("SD-Ext\n");
                        } else if (value2[i] == 'M' || value2[i] == 'm') {
#ifdef PHILZ_TOUCH_RECOVERY
                            enable_md5sum = 0;
                            ui_print("MD5 Check is off\n");
#else
                            ui_print("Skip md5 check not supported\n");
#endif
                        }
                    }
                } else {
                    LOGI("No restore options set\n");
                    LOGI("Restoring default partitions");
                    backup_boot = 1, backup_system = 1;
                    backup_data = 1, backup_cache = 1, backup_sdext = 1;
                    ignore_android_secure = 0;
                    backup_preload = nandroid_add_preload;
                }

                if (twrp_backup_mode)
                    ret_val = twrp_restore(value1);
                else
                    ret_val = nandroid_restore(value1, backup_boot, backup_system, backup_data, backup_cache, backup_sdext, 0);
                
                if (ret_val != 0)
                    ui_print("Restore failed!\n");

                is_custom_backup = 0, twrp_backup_mode = 0;
                reset_custom_job_settings(0);
#ifdef PHILZ_TOUCH_RECOVERY
                enable_md5sum = old_enable_md5sum;
#endif
            } else if (strcmp(command, "mount") == 0) {
                // Mount
                if (value[0] != '/') {
                    strcpy(mount, "/");
                    strcat(mount, value);
                } else
                    strcpy(mount, value);
                ensure_path_mounted(mount);
                ui_print("Mounted '%s'\n", mount);
            } else if (strcmp(command, "unmount") == 0 || strcmp(command, "umount") == 0) {
                // Unmount
                if (value[0] != '/') {
                    strcpy(mount, "/");
                    strcat(mount, value);
                } else
                    strcpy(mount, value);
                ensure_path_unmounted(mount);
                ui_print("Unmounted '%s'\n", mount);
            } else if (strcmp(command, "set") == 0) {
                // Set value
                tok = strtok(value, " ");
                strcpy(value1, tok);
                tok = strtok(NULL, " ");
                strcpy(value2, tok);
                ui_print("Setting function disabled in CWMR: '%s' to '%s'\n", value1, value2);
            } else if (strcmp(command, "mkdir") == 0) {
                // Make directory (recursive)
                ui_print("Recursive mkdir disabled in CWMR: '%s'\n", value);
            } else if (strcmp(command, "reboot") == 0) {
                // Reboot
            } else if (strcmp(command, "cmd") == 0) {
                if (cindex != 0) {
                    __system(value);
                } else {
                    LOGE("No value given for cmd\n");
                }
            } else if (strcmp(command, "print") == 0) {
                ui_print("%s\n", value);
            } else if (strcmp(command, "sideload") == 0) {
                // Install zip from sideload
                ui_print("Waiting for sideload...\n");
                ensure_path_mounted("/sdcard");
                if (volume_for_path("/external_sd") != NULL)
                    ensure_path_mounted("/external_sd");
                if (volume_for_path("/emmc") != NULL)
                    ensure_path_mounted("/emmc");
                if (0 != (ret_val = apply_from_adb()))
                    LOGE("Error installing from sideload\n");
            } else {
                LOGE("Unrecognized script command: '%s'\n", command);
                ret_val = 1;
            }
        }
        fclose(fp);
        ui_print("Done processing script file\n");
    } else {
        LOGE("Error opening script file '%s'\n", ors_script);
        return 1;
    }
    return ret_val;
}
//end of open recovery script file code

//show menu: select ors from default path
static void choose_default_ors_menu(const char* ors_path)
{
    if (ensure_path_mounted(ors_path) != 0) {
        LOGE("Can't mount %s\n", ors_path);
        browse_for_file = 1;
        return;
    }

    char ors_dir[PATH_MAX];
    sprintf(ors_dir, "%s/clockworkmod/ors/", ors_path);
    if (access(ors_dir, F_OK) == -1) {
        //custom folder does not exist
        browse_for_file = 1;
        return;
    }

    static char* headers[] = {  "Choose a script to run",
                                "",
                                NULL
    };

    char* ors_file = choose_file_menu(ors_dir, ".ors", headers);
    if (no_files_found == 1) {
        //0 valid files to select, let's continue browsing next locations
        ui_print("No *.ors files in %s/clockworkmod/ors\n", ors_path);
        browse_for_file = 1;
    } else {
        browse_for_file = 0;
        //we found ors scripts in clockworkmod/ors folder: do not proceed other locations even if no file is chosen
    }
    if (ors_file == NULL) {
        //either no valid files found or we selected no files by pressing back menu
        return;
    }
    static char* confirm_install  = "Confirm run script?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Run %s", basename(ors_file));
    if (confirm_selection(confirm_install, confirm)) {
        run_ors_script(ors_file);
    }
}

//show menu: browse for custom Open Recovery Script
static void choose_custom_ors_menu(const char* ors_path)
{
    if (ensure_path_mounted(ors_path) != 0) {
        LOGE("Can't mount %s\n", ors_path);
        return;
    }

    static char* headers[] = {  "Choose .ors script to run",
                                NULL
    };

    char* ors_file = choose_file_menu(ors_path, ".ors", headers);
    if (ors_file == NULL)
        return;
    static char* confirm_install  = "Confirm run script?";
    static char confirm[PATH_MAX];
    sprintf(confirm, "Yes - Run %s", basename(ors_file));
    if (confirm_selection(confirm_install, confirm)) {
        run_ors_script(ors_file);
    }
}

//show menu: select sdcard volume to search for custom ors file
static void show_custom_ors_menu() {
    static char* headers[] = {  "Search .ors script to run",
                                "",
                                NULL
    };

    char* list[] = { "Search sdcard",
                            NULL,
                            NULL
    };

    char *other_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        other_sd = "/emmc/";
        list[1] = "Search Internal sdcard";
    } else if (volume_for_path("/external_sd") != NULL) {
        other_sd = "/external_sd/";
        list[1] = "Search External sdcard";
    }

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                choose_custom_ors_menu("/sdcard/");
                break;
            case 1:
                choose_custom_ors_menu(other_sd);
                break;
        }
    }
}
//----------end open recovery script support


#ifdef PHILZ_TOUCH_RECOVERY
#include "/root/Desktop/PhilZ_Touch/touch_source/philz_gui_settings.c"
#endif


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/*   Custom Backup and Restore Support   */
/*       code written by PhilZ @xda      */
/*        for PhilZ Touch Recovery       */
/*****************************************/

static void choose_delete_folder(const char* path) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return;
    }

    static char* headers[] = {  "Choose folder to delete",
                                NULL
    };

    char* file = choose_file_menu(path, NULL, headers);
    if (file == NULL)
        return;

    char tmp[PATH_MAX];
    sprintf(tmp, "Yes - Delete %s", basename(file));
    if (confirm_selection("Confirm delete?", tmp)) {
        sprintf(tmp, "rm -rf '%s'", file);
        __system(tmp);
    }
}

// actually only used to delete twrp backups
static void delete_custom_backups(const char* backup_path)
{
    static char* headers[] = {"Browse backup folders...", NULL};

    char* list[] = {"Delete from Internal sdcard",
                    NULL,
                    NULL};

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Delete from External sdcard";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            {
                char tmp[PATH_MAX];
                sprintf(tmp, "%s/%s/", int_sd, backup_path);
                choose_delete_folder(tmp);
            }
            break;
        case 1:
            {
                char tmp[PATH_MAX];
                sprintf(tmp, "%s/%s/", ext_sd, backup_path);
                choose_delete_folder(tmp);
            }
            break;
    }
}

/*
- get_android_secure_path() should be called each time we want to backup/restore .android_secure
- it will always favor external storage
- it will format path to retained android_secure location and set android_secure_ext to 1 or 0
- android_secure_ext = 1, will allow nandroid processing of android_secure partition
- to force other storage, user must keep only one .android_secure folder in one of the sdcards
- for /data/media devices, only second storage is allowed, not /sdcard
- custom backup and restore jobs (incl twrp and ors modes) can force .android_secure to be ignored
  this is done by setting ignore_android_secure to 1
- ignore_android_secure is by default 0 ad will be reset to 0 by reset_custom_job_settings()
*/
int get_android_secure_path(char *and_sec_path) {
    if (ignore_android_secure)
        return android_secure_ext = 0;

    android_secure_ext = 1;
    struct stat st;
    if (volume_for_path("/external_sd") != NULL &&
                ensure_path_mounted("/external_sd") == 0 &&
                stat("/external_sd/.android_secure", &st) == 0) {
        strcpy(and_sec_path, "/external_sd/.android_secure");
    }
    else if (!is_data_media() && ensure_path_mounted("/sdcard") == 0 && 
                stat("/sdcard/.android_secure", &st) == 0) {
        strcpy(and_sec_path, "/sdcard/.android_secure");
    }
    else if (volume_for_path("/emmc") != NULL &&
                ensure_path_mounted("/emmc") == 0 &&
                stat("/emmc/.android_secure", &st) == 0) {
        strcpy(and_sec_path, "/emmc/.android_secure");
    }
    else android_secure_ext = 0;
    
    return android_secure_ext;
}

void reset_custom_job_settings(int custom_job) {
    if (custom_job) {
        backup_boot = 1, backup_recovery = 1, backup_system = 1;
        backup_data = 1, backup_cache = 1;
        backup_wimax = 0;
        backup_sdext = 0;
        if (twrp_backup_mode)
            backup_wimax = 0;
    } else {
        backup_boot = 1, backup_recovery = 1, backup_system = 1;
        backup_data = 1, backup_cache = 1;
        backup_wimax = 1;
        backup_sdext = 1;
    }

    backup_preload = 0;
    backup_modem = 0;
    backup_radio = 0;
    backup_efs = 0;
    backup_misc = 0;
    ignore_android_secure = 0;
    reboot_after_nandroid = 0;
}

static void ui_print_backup_list() {
    ui_print("This will process");
    if (backup_boot)
        ui_print(" - boot");
    if (backup_recovery)
        ui_print(" - recovery");
    if (backup_system)
        ui_print(" - system");
    if (backup_preload)
        ui_print(" - preload");
    if (backup_data)
        ui_print(" - data");
    if (backup_cache)
        ui_print(" - cache");
    if (backup_sdext)
        ui_print(" - sd-ext");
    if (backup_modem)
        ui_print(" - modem");
    if (backup_radio)
        ui_print(" - radio");
    if (backup_wimax)
        ui_print(" - wimax");
    if (backup_efs)
        ui_print(" - efs");
    if (backup_misc)
        ui_print(" - misc");
    ui_print("!\n");
}

void get_custom_backup_path(const char* sd_path, char *backup_path) {
    char rom_name[PROPERTY_VALUE_MAX] = "noname";
#ifdef PHILZ_TOUCH_RECOVERY
    get_rom_name(rom_name);
#endif
    time_t t = time(NULL);
    struct tm *timeptr = localtime(&t);
    if (timeptr == NULL) {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        if (backup_efs)
            sprintf(backup_path, "%s/%s/%d", sd_path, EFS_BACKUP_PATH, tp.tv_sec);
        else
            sprintf(backup_path, "%s/%s/%d_%s", sd_path, "clockworkmod/backup", tp.tv_sec, rom_name);
    } else {
        char tmp[PATH_MAX];
        strftime(tmp, sizeof(tmp), "%F.%H.%M.%S", timeptr);
        if (backup_efs)
            sprintf(backup_path, "%s/%s/%s", sd_path, EFS_BACKUP_PATH, tmp);
        else
            sprintf(backup_path, "%s/%s/%s_%s", sd_path, "clockworkmod/backup", tmp, rom_name);
    }
}

static void custom_backup_handler() {
    static char* headers[] = {"Select custom backup target", "", NULL};
    char* list[] = {"Backup to Internal sdcard",
                        NULL,
                        NULL};

    ui_print_backup_list();

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Backup to External sdcard";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            {
                if (ensure_path_mounted(int_sd) == 0) {
                    char backup_path[PATH_MAX] = "";
                    get_custom_backup_path(int_sd, backup_path);
                    nandroid_backup(backup_path);
                } else {
                    ui_print("Couldn't mount %s\n", int_sd);
                }
            }
            break;
        case 1:
            {
                if (ensure_path_mounted(ext_sd) == 0) {
                    char backup_path[PATH_MAX] = "";
                    get_custom_backup_path(ext_sd, backup_path);
                    nandroid_backup(backup_path);
                } else {
                    ui_print("Couldn't mount %s\n", ext_sd);
                }
            }
            break;
    }
}

static void custom_restore_handler(const char* backup_path) {
    if (ensure_path_mounted(backup_path) != 0) {
        LOGE("Can't mount %s\n", backup_path);
        return;
    }

    static char* headers[] = {  "Choose a backup to restore",
                                NULL
    };

    struct statfs s;
    char* file = NULL;
    static char* confirm_install = "Restore from this backup?";
    char tmp[PATH_MAX];
    char *backup_source;

    if (backup_efs == RESTORE_EFS_IMG) {
        if (volume_for_path("/efs") == NULL) {
            LOGE("No /efs partition to flash\n");
            return;
        }
        file = choose_file_menu(backup_path, ".img", headers);
        if (file == NULL) {
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        //restore efs raw image
        backup_source = basename(file);
        ui_print("%s will be flashed to /efs!\n", backup_source);
        sprintf(tmp, "Yes - Restore %s", backup_source);
        if (confirm_selection(confirm_install, tmp))
            dd_raw_restore_handler(file, "/efs");
    } else if (backup_efs == RESTORE_EFS_TAR) {
        if (volume_for_path("/efs") == NULL) {
            LOGE("No /efs partition to flash\n");
            return;
        }
        file = choose_file_menu(backup_path, NULL, headers);
        if (file == NULL) {
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        sprintf(tmp, "%s/efs.img", file);
        if (0 == statfs(tmp, &s)) {
            ui_print("efs.img file detected in %s!\n", file);
            ui_print("Either select efs.img to restore it,\n");
            ui_print("or remove it to restore nandroid source.\n");
            return;
        }

        //restore efs from nandroid tar format
        ui_print("%s will be restored to /efs!\n", file);
        sprintf(tmp, "Yes - Restore %s", basename(file));
        if (confirm_selection(confirm_install, tmp))
            nandroid_restore(file, 0, 0, 0, 0, 0, 0);
    } else if (backup_modem == RAW_BIN_FILE) {
        file = choose_file_menu(backup_path, ".bin", headers);
        if (file == NULL) {
            //either no valid files found or we selected no files by pressing back menu
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        //restore modem.bin raw image
        backup_source = basename(file);
        Volume *vol = volume_for_path("/modem");
        if (vol != NULL) {
            ui_print("%s will be flashed to /modem!\n", backup_source);
            char confirm[PATH_MAX];
            sprintf(confirm, "Yes - Restore %s", backup_source);
            if (confirm_selection(confirm_install, confirm))
                dd_raw_restore_handler(file, "/modem");
        } else
            LOGE("no /modem partition to flash\n");
    } else if (backup_radio == RAW_BIN_FILE) {
        file = choose_file_menu(backup_path, ".bin", headers);
        if (file == NULL) {
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }

        //restore radio.bin raw image
        backup_source = basename(file);
        Volume *vol = volume_for_path("/radio");
        if (vol != NULL) {
            ui_print("%s will be flashed to /radio!\n", backup_source);
            char confirm[PATH_MAX];
            sprintf(confirm, "Yes - Restore %s", backup_source);
            if (confirm_selection(confirm_install, confirm))
                dd_raw_restore_handler(file, "/radio");
        } else
            LOGE("no /radio partition to flash\n");
    } else {
        //process restore job
        file = choose_file_menu(backup_path, "", headers);
        if (file == NULL) {
            if (no_files_found)
                ui_print("Nothing to restore in %s !\n", backup_path);
            return;
        }
        backup_source = dirname(file);
        ui_print("%s will be restored to selected partitions!\n", backup_source);
        sprintf(tmp, "Yes - Restore %s", basename(backup_source));
        if (confirm_selection(confirm_install, tmp)) {
            nandroid_restore(backup_source, backup_boot, backup_system, backup_data, backup_cache, backup_sdext, backup_wimax);
        }
    }
}

static void browse_backup_folders(const char* backup_path)
{
    static char* headers[] = {"Browse backup folders...", "", NULL};

    char* list[] = {"Restore from Internal sdcard",
                    NULL,
                    NULL};

    ui_print_backup_list();

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Restore from External sdcard";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            {
                char tmp[PATH_MAX];
                sprintf(tmp, "%s/%s/", int_sd, backup_path);
                if (twrp_backup_mode)
                    twrp_restore_handler(tmp);
                else
                    custom_restore_handler(tmp);
            }
            break;
        case 1:
            {
                char tmp[PATH_MAX];
                sprintf(tmp, "%s/%s/", ext_sd, backup_path);
                if (twrp_backup_mode)
                    twrp_restore_handler(tmp);
                else
                    custom_restore_handler(tmp);
            }
            break;
    }
}

/*
* custom backup and restore jobs:
    - At least one partition to restore must be selected
* restore jobs
    - modem.bin and radio.bin file types must be restored alone (available only in custom restore mode)
    - TWRP restore:
        + accepts efs to be restored along other partitions (but modem is never of bin type and preload is always 0)
    - Custom jobs:
        + efs must be restored alone (except for twrp backup jobs)
        + else, all other tasks can be processed
* backup jobs
    - if it is a twrp backup job, we can accept efs with other partitions
    - else, we only accept them separately for custom backup jobs    
*/
static void validate_backup_job(const char* backup_path) {
    int sum = backup_boot + backup_recovery + backup_system + backup_preload + backup_data +
                backup_cache + backup_sdext + backup_wimax + backup_misc;
    if (0 == (sum + backup_efs + backup_modem + backup_radio)) {
        ui_print("Select at least one partition to restore!\n");
        return;
    }

    if (backup_path != NULL)
    {
        // it is a restore job
        if (backup_modem == RAW_BIN_FILE) {
            if (0 != (sum + backup_efs + backup_radio))
                ui_print("modem.bin format must be restored alone!\n");
            else
                browse_backup_folders(MODEM_BIN_PATH);
        }
        else if (backup_radio == RAW_BIN_FILE) {
            if (0 != (sum + backup_efs + backup_modem))
                ui_print("radio.bin format must be restored alone!\n");
            else
                browse_backup_folders(RADIO_BIN_PATH);
        }
        else if (twrp_backup_mode)
            browse_backup_folders(backup_path);
        else if (backup_efs && (sum + backup_modem + backup_radio) != 0)
            ui_print("efs must be restored alone!\n");
        else if (backup_efs && (sum + backup_modem + backup_radio) == 0)
            browse_backup_folders(EFS_BACKUP_PATH);
        else
            browse_backup_folders(backup_path);
    }
    else
    {
        // it is a backup job to validate
        if (nandroid_get_default_backup_format() != NANDROID_BACKUP_FORMAT_TAR)
            LOGE("Default backup format must be tar!\n");
        else if (twrp_backup_mode)
            twrp_backup_handler();
        else if (backup_efs && (sum + backup_modem + backup_radio) != 0)
            ui_print("efs must be backed up alone!\n");
        else
            custom_backup_handler();
    }
}

// we'd better do some malloc here... later
static void custom_restore_menu(const char* backup_path) {
    static char* headers[] = {  "Custom restore job",
                                NULL
    };

    char item_boot[MENU_MAX_COLS];
    char item_recovery[MENU_MAX_COLS];
    char item_system[MENU_MAX_COLS];
    char item_preload[MENU_MAX_COLS];
    char item_data[MENU_MAX_COLS];
    char item_andsec[MENU_MAX_COLS];
    char item_cache[MENU_MAX_COLS];
    char item_sdext[MENU_MAX_COLS];
    char item_modem[MENU_MAX_COLS];
    char item_radio[MENU_MAX_COLS];
    char item_efs[MENU_MAX_COLS];
    char item_misc[MENU_MAX_COLS];
    char item_reboot[MENU_MAX_COLS];
    char item_wimax[MENU_MAX_COLS];
    char* list[] = { item_boot,
                item_recovery,
                item_system,
                item_preload,
                item_data,
                item_andsec,
                item_cache,
                item_sdext,
                item_modem,
                item_radio,
                item_efs,
                item_misc,
                ">> Start Custom Restore Job <<",
                item_reboot,
                NULL,
                NULL
    };

    char tmp[PATH_MAX];
    if (0 == get_partition_device("wimax", tmp)) {
        // show wimax restore option
        list[14] = "show wimax menu";
    }

    reset_custom_job_settings(1);
    for (;;) {
        if (backup_boot) ui_format_gui_menu(item_boot, "Restore boot", "(x)");
        else ui_format_gui_menu(item_boot, "Restore boot", "( )");

        if (backup_recovery) ui_format_gui_menu(item_recovery, "Restore recovery", "(x)");
        else ui_format_gui_menu(item_recovery, "Restore recovery", "( )");

        if (backup_system) ui_format_gui_menu(item_system, "Restore system", "(x)");
        else ui_format_gui_menu(item_system, "Restore system", "( )");

        if (volume_for_path("/preload") == NULL)
            ui_format_gui_menu(item_preload, "Restore preload", "N/A");
        else if (backup_preload) ui_format_gui_menu(item_preload, "Restore preload", "(x)");
        else ui_format_gui_menu(item_preload, "Restore preload", "( )");

        if (backup_data) ui_format_gui_menu(item_data, "Restore data", "(x)");
        else ui_format_gui_menu(item_data, "Restore data", "( )");

        get_android_secure_path(tmp);
        if (backup_data && android_secure_ext)
            ui_format_gui_menu(item_andsec, "Restore and-sec", dirname(tmp));
        else ui_format_gui_menu(item_andsec, "Restore and-sec", "( )");

        if (backup_cache) ui_format_gui_menu(item_cache, "Restore cache", "(x)");
        else ui_format_gui_menu(item_cache, "Restore cache", "( )");

        if (backup_sdext) ui_format_gui_menu(item_sdext, "Restore sd-ext", "(x)");
        else ui_format_gui_menu(item_sdext, "Restore sd-ext", "( )");

        if (volume_for_path("/modem") == NULL)
            ui_format_gui_menu(item_modem, "Restore modem", "N/A");
        else if (backup_modem == RAW_IMG_FILE)
            ui_format_gui_menu(item_modem, "Restore modem [.img]", "(x)");
        else if (backup_modem == RAW_BIN_FILE)
            ui_format_gui_menu(item_modem, "Restore modem [.bin]", "(x)");
        else ui_format_gui_menu(item_modem, "Restore modem", "( )");

        if (volume_for_path("/radio") == NULL)
            ui_format_gui_menu(item_radio, "Restore radio", "N/A");
        else if (backup_radio == RAW_IMG_FILE)
            ui_format_gui_menu(item_radio, "Restore radio [.img]", "(x)");
        else if (backup_radio == RAW_BIN_FILE)
            ui_format_gui_menu(item_radio, "Restore radio [.bin]", "(x)");
        else ui_format_gui_menu(item_radio, "Restore radio", "( )");

        if (volume_for_path("/efs") == NULL)
            ui_format_gui_menu(item_efs, "Restore efs", "N/A");
        else if (backup_efs == RESTORE_EFS_IMG)
            ui_format_gui_menu(item_efs, "Restore efs [.img]", "(x)");
        else if (backup_efs == RESTORE_EFS_TAR)
            ui_format_gui_menu(item_efs, "Restore efs [.tar]", "(x)");
        else ui_format_gui_menu(item_efs, "Restore efs", "( )");

        if (volume_for_path("/misc") == NULL)
            ui_format_gui_menu(item_misc, "Restore misc", "N/A");
        else if (backup_misc) ui_format_gui_menu(item_misc, "Restore misc", "(x)");
        else ui_format_gui_menu(item_misc, "Restore misc", "( )");

        if (reboot_after_nandroid) ui_format_gui_menu(item_reboot, "Reboot once done", "(x)");
        else ui_format_gui_menu(item_reboot, "Reboot once done", "( )");

        if (NULL != list[14]) {
            if (backup_wimax)
                ui_format_gui_menu(item_wimax, "Restore WiMax", "(x)");
            else ui_format_gui_menu(item_wimax, "Restore WiMax", "( )");
            list[14] = item_wimax;
        }


        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                backup_boot ^= 1;
                break;
            case 1:
                backup_recovery ^= 1;
                break;
            case 2:
                backup_system ^= 1;
                break;
            case 3:
                if (volume_for_path("/preload") == NULL)
                    backup_preload = 0;
                else backup_preload ^= 1;
                break;
            case 4:
                backup_data ^= 1;
                break;
            case 5:
                ignore_android_secure ^= 1;
                if (!ignore_android_secure && has_second_storage())
                    ui_print("To force restore to 2nd storage, keep only one .android_secure folder\n");
                break;
            case 6:
                backup_cache ^= 1;
                break;
            case 7:
                backup_sdext ^= 1;
                break;
            case 8:
                if (volume_for_path("/modem") == NULL)
                    backup_modem = 0;
                else {
                    backup_modem++;
                    if (backup_modem > 2)
                        backup_modem = 0;
                    if (twrp_backup_mode && backup_modem == RAW_BIN_FILE)
                        backup_modem = 0;
                }
                break;
            case 9:
                if (volume_for_path("/radio") == NULL)
                    backup_radio = 0;
                else {
                    backup_radio++;
                    if (backup_radio > 2)
                        backup_radio = 0;
                    if (twrp_backup_mode && backup_radio == RAW_BIN_FILE)
                        backup_radio = 0;
                }
                break;
            case 10:
                if (volume_for_path("/efs") == NULL)
                    backup_efs = 0;
                else {
                    backup_efs++;
                    if (backup_efs > 2)
                        backup_efs = 0;
                    if (twrp_backup_mode && backup_efs == RESTORE_EFS_IMG)
                        backup_efs = 0;
                }
                break;
            case 11:
                if (volume_for_path("/misc") == NULL)
                    backup_misc = 0;
                else backup_misc ^= 1;
                break;
            case 12:
                validate_backup_job(backup_path);
                break;
            case 13:
                reboot_after_nandroid ^= 1;
                break;
            case 14:
                if (twrp_backup_mode) backup_wimax = 0;
                else backup_wimax ^= 1;
                break;
        }
    }
    reset_custom_job_settings(0);
}

static void custom_backup_menu() {
    static char* headers[] = {  "Custom backup job",
                                NULL
    };

    char item_boot[MENU_MAX_COLS];
    char item_recovery[MENU_MAX_COLS];
    char item_system[MENU_MAX_COLS];
    char item_preload[MENU_MAX_COLS];
    char item_data[MENU_MAX_COLS];
    char item_andsec[MENU_MAX_COLS];
    char item_cache[MENU_MAX_COLS];
    char item_sdext[MENU_MAX_COLS];
    char item_modem[MENU_MAX_COLS];
    char item_radio[MENU_MAX_COLS];
    char item_efs[MENU_MAX_COLS];
    char item_misc[MENU_MAX_COLS];
    char item_reboot[MENU_MAX_COLS];
    char item_wimax[MENU_MAX_COLS];
    char* list[] = { item_boot,
                item_recovery,
                item_system,
                item_preload,
                item_data,
                item_andsec,
                item_cache,
                item_sdext,
                item_modem,
                item_radio,
                item_efs,
                item_misc,
                ">> Start Custom Backup Job <<",
                item_reboot,
                NULL,
                NULL
    };

    char tmp[PATH_MAX];
    if (volume_for_path("/wimax") != NULL) {
        // show wimax backup option
        list[14] = "show wimax menu";
    }

    reset_custom_job_settings(1);
    for (;;) {
        if (backup_boot) ui_format_gui_menu(item_boot, "Backup boot", "(x)");
        else ui_format_gui_menu(item_boot, "Backup boot", "( )");

        if (backup_recovery) ui_format_gui_menu(item_recovery, "Backup recovery", "(x)");
        else ui_format_gui_menu(item_recovery, "Backup recovery", "( )");

        if (backup_system) ui_format_gui_menu(item_system, "Backup system", "(x)");
        else ui_format_gui_menu(item_system, "Backup system", "( )");

        if (volume_for_path("/preload") == NULL)
            ui_format_gui_menu(item_preload, "Backup preload", "N/A");
        else if (backup_preload) ui_format_gui_menu(item_preload, "Backup preload", "(x)");
        else ui_format_gui_menu(item_preload, "Backup preload", "( )");

        if (backup_data) ui_format_gui_menu(item_data, "Backup data", "(x)");
        else ui_format_gui_menu(item_data, "Backup data", "( )");

        get_android_secure_path(tmp);
        if (backup_data && android_secure_ext)
            ui_format_gui_menu(item_andsec, "Backup and-sec", dirname(tmp));
        else ui_format_gui_menu(item_andsec, "Backup and-sec", "( )");

        if (backup_cache) ui_format_gui_menu(item_cache, "Backup cache", "(x)");
        else ui_format_gui_menu(item_cache, "Backup cache", "( )");

        if (backup_sdext) ui_format_gui_menu(item_sdext, "Backup sd-ext", "(x)");
        else ui_format_gui_menu(item_sdext, "Backup sd-ext", "( )");

        if (volume_for_path("/modem") == NULL)
            ui_format_gui_menu(item_modem, "Backup modem", "N/A");
        else if (backup_modem) ui_format_gui_menu(item_modem, "Backup modem [.img]", "(x)");
        else ui_format_gui_menu(item_modem, "Backup modem", "( )");

        if (volume_for_path("/radio") == NULL)
            ui_format_gui_menu(item_radio, "Backup radio", "N/A");
        else if (backup_radio) ui_format_gui_menu(item_radio, "Backup radio [.img]", "(x)");
        else ui_format_gui_menu(item_radio, "Backup radio", "( )");

        if (volume_for_path("/efs") == NULL)
            ui_format_gui_menu(item_efs, "Backup efs", "N/A");
        else if (backup_efs && twrp_backup_mode)
            ui_format_gui_menu(item_efs, "Backup efs", "(x)");
        else if (backup_efs && !twrp_backup_mode)
            ui_format_gui_menu(item_efs, "Backup efs [img&tar]", "(x)");
        else ui_format_gui_menu(item_efs, "Backup efs", "( )");

        if (volume_for_path("/misc") == NULL)
            ui_format_gui_menu(item_misc, "Backup misc", "N/A");
        else if (backup_misc) ui_format_gui_menu(item_misc, "Backup misc", "(x)");
        else ui_format_gui_menu(item_misc, "Backup misc", "( )");

        if (reboot_after_nandroid) ui_format_gui_menu(item_reboot, "Reboot once done", "(x)");
        else ui_format_gui_menu(item_reboot, "Reboot once done", "( )");

        if (NULL != list[14]) {
            if (backup_wimax)
                ui_format_gui_menu(item_wimax, "Backup WiMax", "(x)");
            else ui_format_gui_menu(item_wimax, "Backup WiMax", "( )");
            list[14] = item_wimax;
        }

        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                backup_boot ^= 1;
                break;
            case 1:
                backup_recovery ^= 1;
                break;
            case 2:
                backup_system ^= 1;
                break;
            case 3:
                if (volume_for_path("/preload") == NULL)
                    backup_preload = 0;
                else backup_preload ^= 1;
                break;
            case 4:
                backup_data ^= 1;
                break;
            case 5:
                ignore_android_secure ^= 1;
                if (!ignore_android_secure && has_second_storage())
                    ui_print("To force backup from 2nd storage, keep only one .android_secure folder\n");
                break;
            case 6:
                backup_cache ^= 1;
                break;
            case 7:
                backup_sdext ^= 1;
                break;
            case 8:
                if (volume_for_path("/modem") == NULL)
                    backup_modem = 0;
                else backup_modem ^= 1;
                break;
            case 9:
                if (volume_for_path("/radio") == NULL)
                    backup_radio = 0;
                else backup_radio ^= 1;
                break;
            case 10:
                if (volume_for_path("/efs") == NULL)
                    backup_efs = 0;
                else backup_efs ^= 1;
                break;
            case 11:
                if (volume_for_path("/misc") == NULL)
                    backup_misc = 0;
                else backup_misc ^= 1;
                break;
            case 12:
                validate_backup_job(NULL);
                break;
            case 13:
                reboot_after_nandroid ^= 1;
                break;
            case 14:
                if (twrp_backup_mode) backup_wimax = 0;
                else backup_wimax ^= 1;
                break;
        }
    }
    reset_custom_job_settings(0);
}
//------- end Custom Backup and Restore functions


/*****************************************/
/*   DO NOT REMOVE THIS CREDITS HEARDER  */
/* IF YOU MODIFY ANY PART OF THIS SOURCE */
/*  YOU MUST AGREE TO SHARE THE CHANGES  */
/*                                       */
/* Part of TWRP Backup & Restore Support */
/*    Original CWM port by PhilZ @xda    */
/*    Original TWRP code by Dees_Troy    */
/*         (dees_troy at yahoo)          */
/*****************************************/

int check_twrp_md5sum(const char* backup_path) {
    char tmp[PATH_MAX];
    int numFiles = 0;
    ensure_path_mounted(backup_path);
    strcpy(tmp, backup_path);
    if (strcmp(tmp + strlen(tmp) - 1, "/") != 0)
        strcat(tmp, "/");

    ui_print("\n>> Checking MD5 sums...\n");
    char** files = gather_files(tmp, ".md5", &numFiles);
    if (numFiles == 0) {
        ui_print("No md5 checksum files found in %s\n", tmp);
        free_string_array(files);
        return -1;
    }

    int i = 0;
    for(i=0; i < numFiles; i++) {
        sprintf(tmp, "cd '%s' && md5sum -c '%s'", backup_path, basename(files[i]));
        if (0 != __system(tmp)) {
            ui_print("md5sum error in %s!\n", basename(files[i]));
            free_string_array(files);
            return -1;
        }
    }

    ui_print("MD5 sum ok.\n");
    free_string_array(files);
    return 0;
}

int gen_twrp_md5sum(const char* backup_path) {
    ui_print("\n>> Generating md5 sum...\n");
    ensure_path_mounted(backup_path);
    char tmp[PATH_MAX];
    int numFiles = 0;
    sprintf(tmp, "%s/", backup_path);
    // this will exclude subfolders!
    char** files = gather_files(tmp, "", &numFiles);
    if (numFiles == 0) {
        ui_print("No files found in backup path %s\n", tmp);
        free_string_array(files);
        return -1;
    }

    int i = 0;
    for(i=0; i < numFiles; i++) {
        sprintf(tmp, "cd '%s'; md5sum '%s' > '%s.md5'", backup_path, basename(files[i]), basename(files[i]));
        if (0 != __system(tmp)) {
            ui_print("Error while generating md5 sum for %s!\n", files[i]);
            free_string_array(files);
            return -1;
        }
    }

    ui_print("MD5 sum created.\n");
    free_string_array(files);
    return 0;
}

// Device ID functions
static void sanitize_device_id(char *device_id) {
    const char* whitelist ="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-._";
    char str[PROPERTY_VALUE_MAX];
    char* c = str;

    strcpy(str, device_id);
    char tmp[PROPERTY_VALUE_MAX];
    memset(tmp, 0, sizeof(tmp));
    while (*c) {
        if (strchr(whitelist, *c))
            strncat(tmp, c, 1);
        c++;
    }
    strcpy(device_id, tmp);
    return;
}

#define CMDLINE_SERIALNO        "androidboot.serialno="
#define CMDLINE_SERIALNO_LEN    (strlen(CMDLINE_SERIALNO))
#define CPUINFO_SERIALNO        "Serial"
#define CPUINFO_SERIALNO_LEN    (strlen(CPUINFO_SERIALNO))
#define CPUINFO_HARDWARE        "Hardware"
#define CPUINFO_HARDWARE_LEN    (strlen(CPUINFO_HARDWARE))

void get_device_id(char *device_id) {
    // First try system properties
    property_get("ro.serialno", device_id, "");
    if (strlen(device_id) != 0) {
        sanitize_device_id(device_id);
        LOGI("Using ro.serialno='%s'\n", device_id);
        return;
    }
    property_get("ro.boot.serialno", device_id, "");
    if (strlen(device_id) != 0) {
        sanitize_device_id(device_id);
        LOGI("Using ro.boot.serialno='%s'\n", device_id);
        return;
    }

    // device_id not found, looking elsewhere
    FILE *fp;
    char line[2048];
    char hardware_id[32];
    char* token;

    // Assign a blank device_id to start with
    device_id[0] = 0;

    // Try the cmdline to see if the serial number was supplied
    fp = fopen("/proc/cmdline", "rt");
    if (fp != NULL)
    {
        // First step, read the line. For cmdline, it's one long line
        LOGI("Checking cmdline for serialno...\n");
        fgets(line, sizeof(line), fp);
        fclose(fp);

        // Now, let's tokenize the string
        token = strtok(line, " ");
        if (strlen(token) > PROPERTY_VALUE_MAX)
            token[PROPERTY_VALUE_MAX] = 0;

        // Let's walk through the line, looking for the CMDLINE_SERIALNO token
        while (token)
        {
            // We don't need to verify the length of token, because if it's too short, it will mismatch CMDLINE_SERIALNO at the NULL
            if (memcmp(token, CMDLINE_SERIALNO, CMDLINE_SERIALNO_LEN) == 0)
            {
                // We found the serial number!
                strcpy(device_id, token + CMDLINE_SERIALNO_LEN);
                sanitize_device_id(device_id);
                LOGI("Using serialno='%s'\n", device_id);
                return;
            }
            token = strtok(NULL, " ");
        }
    }

    // Now we'll try cpuinfo for a serial number (we shouldn't reach here as it gives wired output)
    fp = fopen("/proc/cpuinfo", "rt");
    if (fp != NULL)
    {
        LOGI("Checking cpuinfo...\n");
        while (fgets(line, sizeof(line), fp) != NULL) { // First step, read the line.
            if (memcmp(line, CPUINFO_SERIALNO, CPUINFO_SERIALNO_LEN) == 0)  // check the beginning of the line for "Serial"
            {
                // We found the serial number!
                token = line + CPUINFO_SERIALNO_LEN; // skip past "Serial"
                while ((*token > 0 && *token <= 32 ) || *token == ':') token++; // skip over all spaces and the colon
                if (*token != 0) {
                    token[30] = 0;
                    if (token[strlen(token)-1] == 10) { // checking for endline chars and dropping them from the end of the string if needed
                        char tmp[PROPERTY_VALUE_MAX];
                        memset(tmp, 0, sizeof(tmp));
                        strncpy(tmp, token, strlen(token) - 1);
                        strcpy(device_id, tmp);
                    } else {
                        strcpy(device_id, token);
                    }
                    fclose(fp);
                    sanitize_device_id(device_id);
                    LOGI("=> Using cpuinfo serialno: '%s'\n", device_id);
                    return;
                }
            } else if (memcmp(line, CPUINFO_HARDWARE, CPUINFO_HARDWARE_LEN) == 0) {
                // We're also going to look for the hardware line in cpuinfo and save it for later in case we don't find the device ID
                // We found the hardware ID
                token = line + CPUINFO_HARDWARE_LEN; // skip past "Hardware"
                while ((*token > 0 && *token <= 32 ) || *token == ':')  token++; // skip over all spaces and the colon
                if (*token != 0) {
                    token[30] = 0;
                    if (token[strlen(token)-1] == 10) { // checking for endline chars and dropping them from the end of the string if needed
                        memset(hardware_id, 0, sizeof(hardware_id));
                        strncpy(hardware_id, token, strlen(token) - 1);
                    } else {
                        strcpy(hardware_id, token);
                    }
                    LOGI("=> hardware id from cpuinfo: '%s'\n", hardware_id);
                }
            }
        }
        fclose(fp);
    }

    if (hardware_id[0] != 0) {
        LOGW("\nusing hardware id for device id: '%s'\n", hardware_id);
        strcpy(device_id, hardware_id);
        sanitize_device_id(device_id);
        return;
    }

    strcpy(device_id, "serialno");
    LOGE("=> device id not found, using '%s'\n", device_id);
    return;
}
// End of Device ID functions

void get_twrp_backup_path(const char* sd_path, char *backup_path) {
    char rom_name[PROPERTY_VALUE_MAX] = "noname";
#ifdef PHILZ_TOUCH_RECOVERY
    get_rom_name(rom_name);
#endif
    time_t t = time(NULL);

    char device_id[PROPERTY_VALUE_MAX];
    get_device_id(device_id);

    struct tm *timeptr = localtime(&t);
    if (timeptr == NULL) {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        sprintf(backup_path, "%s/%s/%s/%d_%s", sd_path, TWRP_BACKUP_PATH, device_id, tp.tv_sec, rom_name);
    } else {
        char tmp[PATH_MAX];
        strftime(tmp, sizeof(tmp), "%F.%H.%M.%S", timeptr);
        sprintf(backup_path, "%s/%s/%s/%s_%s", sd_path, TWRP_BACKUP_PATH, device_id, tmp, rom_name);
    }
}

void twrp_backup_handler() {
    static char* headers[] = {"Select TWRP backup target", "", NULL};
    char* list[] = {"Backup to Internal sdcard",
                        NULL,
                        NULL};

    ui_print_backup_list();

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Backup to External sdcard";

    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            {
                if (ensure_path_mounted(int_sd) == 0) {
                    char backup_path[PATH_MAX];
                    get_twrp_backup_path(int_sd, backup_path);
                    twrp_backup(backup_path);
                }
            }
            break;
        case 1:
            {
                if (ensure_path_mounted(ext_sd) == 0) {
                    char backup_path[PATH_MAX];
                    get_twrp_backup_path(ext_sd, backup_path);
                    twrp_backup(backup_path);
                }
            }
            break;
    }
}

void twrp_restore_handler(const char* backup_path) {
    if (ensure_path_mounted(backup_path) != 0) {
        LOGE("Can't mount %s\n", backup_path);
        return;
    }

    static char* headers[] = {  "Choose a backup to restore",
                                NULL
    };

    char tmp[PATH_MAX];
    char device_id[PROPERTY_VALUE_MAX];
    get_device_id(device_id);
    sprintf(tmp, "%s%s/", backup_path, device_id);

    char* file = choose_file_menu(tmp, "", headers);
    if (file == NULL) {
        if (no_files_found)
            ui_print("Nothing to restore in %s !\n", tmp);
        return;
    }

    char *backup_source;
    backup_source = dirname(file);
    ui_print("%s will be restored to selected partitions!\n", backup_source);
    sprintf(tmp, "Yes - Restore %s", basename(backup_source));
    if (confirm_selection("Restore from this backup?", tmp))
        twrp_restore(backup_source);
}

static void twrp_backup_restore_menu() {
    static char* headers[] = {  "TWRP Backup and Restore",
                                "",
                                NULL
    };
    static char* list[] = { "Backup in TWRP Format",
                    "Restore from TWRP Format",
                    "Delete TWRP Backup Image",
                    NULL
    };

    twrp_backup_mode = 1;

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                custom_backup_menu();
                break;
            case 1:
                custom_restore_menu(TWRP_BACKUP_PATH);
                break;
            case 2:
                {
                    char tmp[PATH_MAX];
                    char device_id[PROPERTY_VALUE_MAX];
                    get_device_id(device_id);
                    sprintf(tmp, "%s/%s/", TWRP_BACKUP_PATH, device_id);
                    delete_custom_backups(tmp);
                }
                break;
        }
    }

    twrp_backup_mode = 0;
}

static void regenerate_md5_sum_menu() {
    if (!confirm_selection("This is not recommended!!", "Yes - Recreate New md5 Sum"))
        return;

    static char* headers[] = {"Regenerating md5 sum", "Select a backup to regenerate", NULL};

    char* list[] = {"Select from Internal sdcard",
                    NULL,
                    NULL};

    char *int_sd = "/sdcard";
    char *ext_sd = NULL;
    if (volume_for_path("/emmc") != NULL) {
        int_sd = "/emmc";
        ext_sd = "/sdcard";
    } else if (volume_for_path("/external_sd") != NULL)
        ext_sd = "/external_sd";

    if (ext_sd != NULL)
        list[1] = "Select from External sdcard";

    char backup_path[PATH_MAX];
    int chosen_item = get_menu_selection(headers, list, 0, 0);
    switch (chosen_item)
    {
        case 0:
            sprintf(backup_path, "%s", int_sd);
            break;
        case 1:
            sprintf(backup_path, "%s", ext_sd);
            break;
        default:
            return;
    }

    // select backup set and regenerate md5 sum
    strcat(backup_path, "/clockworkmod/backup/");
    if (ensure_path_mounted(backup_path) != 0)
        return;

    char* file = choose_file_menu(backup_path, "", headers);
    if (file == NULL) return;

    char tmp[PATH_MAX];
    char *backup_source;
    backup_source = dirname(file);
    sprintf(tmp, "Process %s", basename(backup_source));
    if (!confirm_selection("Regenerate md5 sum?", tmp))
        return;

    ui_print("Generating md5 sum...\n");
    // to do (optional): remove recovery.log from md5 sum, but no real need to extra code for this!
    sprintf(tmp, "rm -f '%s/nandroid.md5'; nandroid-md5.sh %s", backup_source, backup_source);
    if (0 != __system(tmp))
        ui_print("Error while generating md5 sum!\n");
    else ui_print("Done generating md5 sum.\n");
}
//-------- End TWRP Backup and Restore Options


// Custom backup and restore menu
void custom_backup_restore_menu() {
    static char* headers[] = {  "Custom Backup & Restore",
                                "",
                                NULL
    };

    static char* list[] = { "Custom Backup Job",
                    "Custom Restore Job",
                    "TWRP Backup & Restore",
                    "Regenerate md5 Sum",
                    "Clone ROM to update.zip",
                    "Misc Nandroid Settings",
                    NULL
    };

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {

            case 0:
                custom_backup_menu();
                break;
            case 1:
                custom_restore_menu("clockworkmod/backup");
                break;
            case 2:
                twrp_backup_restore_menu();
                break;
            case 3:
                regenerate_md5_sum_menu();
                break;
            case 4:
#ifdef PHILZ_TOUCH_RECOVERY
                custom_rom_menu();
#endif
                break;
            case 5:
#ifdef PHILZ_TOUCH_RECOVERY
                misc_nandroid_menu();
#endif
                break;
        }
    }
}
//-------------- End PhilZ Touch Special Backup and Restore menu and handlers


// launch aromafm.zip from default locations
static int default_aromafm(const char* aromafm_path) {
        if (ensure_path_mounted(aromafm_path) != 0)
            return -1;

        char aroma_file[PATH_MAX];
        sprintf(aroma_file, "%s/clockworkmod/aromafm/aromafm.zip", aromafm_path);
        if (access(aroma_file, F_OK) != -1) {
#ifdef PHILZ_TOUCH_RECOVERY
            force_wait = -1;
#endif
            install_zip(aroma_file);
            return 0;
        }
        return -1;
}

void run_aroma_browser() {
    //we mount volumes so that they can be accessed when in aroma file manager gui
    ensure_path_mounted("/system");
    ensure_path_mounted("/data");
    if (volume_for_path("/sdcard") != NULL)
        ensure_path_mounted("/sdcard");
    if (volume_for_path("/external_sd") != NULL)
        ensure_path_mounted("/external_sd");
    if (volume_for_path("/emmc") != NULL)
        ensure_path_mounted("/emmc");

    int ret = -1;
    if (volume_for_path("/external_sd") != NULL)
        ret = default_aromafm("/external_sd");
    if (ret != 0 && volume_for_path("/sdcard") != NULL)
        ret = default_aromafm("/sdcard");
    if (ret != 0 && volume_for_path("/emmc") != NULL)
        ret = default_aromafm("/emmc");
    if (ret != 0)
        ui_print("No clockworkmod/aromafm/aromafm.zip on sdcards\n");

    // unmount system and data
    ensure_path_unmounted("/system");
    ensure_path_unmounted("/data");
}
//------ end aromafm launcher functions

// start show PhilZ Touch Settings Menu
void show_philz_settings()
{
    static char* headers[] = {  "PhilZ Settings",
                                NULL
    };

    static char* list[] = { "Open Recovery Script",
                            "Custom Backup and Restore",
                            "Aroma File Manager",
                            "GUI Preferences",
                            "Save and Restore Settings",
                            "Reset All Recovery Settings",
                            "About",
                             NULL
    };

    for (;;) {
        int chosen_item = get_filtered_menu_selection(headers, list, 0, 0, sizeof(list) / sizeof(char*));
        if (chosen_item == GO_BACK)
            break;
        switch (chosen_item)
        {
            case 0:
                {
                    //search in default ors path
                    choose_default_ors_menu("/sdcard");
                    if (browse_for_file == 0) {
                        //we found .ors scripts in /sdcard default location
                        break;
                    }

                    char *other_sd = NULL;
                    if (volume_for_path("/emmc") != NULL) {
                        other_sd = "/emmc";
                    } else if (volume_for_path("/external_sd") != NULL) {
                        other_sd = "/external_sd";
                    }
                    if (other_sd != NULL) {
                        choose_default_ors_menu(other_sd);
                        //we search for .ors files in second sd under default location
                        if (browse_for_file == 0) {
                            //.ors files found
                            break;
                        }
                    }
                    //no files found in default locations, let's search manually for a custom ors
                    ui_print("No .ors files under clockworkmod/ors in sdcards\n");
                    ui_print("Manually search .ors file...\n");
                    show_custom_ors_menu();
                }
                break;
            case 1:
                is_custom_backup = 1;
                custom_backup_restore_menu();
                is_custom_backup = 0;
                break;
            case 2:
                run_aroma_browser();
                break;
            case 3:
#ifdef PHILZ_TOUCH_RECOVERY
                show_touch_gui_menu();
#endif
                break;
            case 4:
#ifdef PHILZ_TOUCH_RECOVERY
                import_export_settings();
#endif
                break;
            case 5:
#ifdef PHILZ_TOUCH_RECOVERY
                if (confirm_selection("Reset all recovery settings?", "Yes - Reset to Defaults")) {
                    delete_a_file(PHILZ_SETTINGS_FILE);
                    refresh_philz_settings();
                    ui_print("All settings reset to default!\n");
                }
#endif
                break;
            case 6:
                ui_print(EXPAND(RECOVERY_MOD_VERSION) "\n");
                ui_print("Build version: " EXPAND(PHILZ_BUILD) " - " EXPAND(TARGET_COMMON_NAME) "\n");
                ui_print("CWM Base version: " EXPAND(CWM_BASE_VERSION) "\n");
                //ui_print(EXPAND(BUILD_DATE)"\n");
                ui_print("Compiled %s at %s\n", __DATE__, __TIME__);
                break;
        }
    }
}

/***************************************************/
/*      End PhilZ Menu settings and functions      */
/***************************************************/
