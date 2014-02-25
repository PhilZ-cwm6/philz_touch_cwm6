
#ifndef __NANDROID_GUI_H
#define __NANDROID_GUI_H

// timer reset on each touch screen event (dim screen during nandroid jobs)
long long last_key_ev;

int user_cancel_nandroid(FILE **fp, const char* backup_file_image, int is_backup, int *nand_starts);

#endif // __NANDROID_GUI_H

