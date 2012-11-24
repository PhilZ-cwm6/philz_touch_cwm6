//ors script support defines

int erase_volume(const char *volume);

void
wipe_data(int confirm);

extern int no_wipe_confirm;

extern int check_for_script_file(const char* ors_boot_script);

extern int run_ors_script(const char* ors_script);