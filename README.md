__<center><big>PhilZ Touch 3 (clockworkmod 6 based recovery)</big></center>__

.

__Home page__
http://forum.xda-developers.com/showthread.php?t=1877270

.

__Touch interface__

toggle between 3 touch modes: __FULL TOUCH__ , __DOUBLE TAP__ to validate and __SEMI TOUCH__ (scroll but no touch validation)

- new touch code completely revised compared to v2.x and older
- __very stable full touch mode:__ no more skips, jumps or wrongly validations while scrolling
- when you set full touch mode, it defaults to optimized settings for menu height and touch sensitivity (you still can alter them manually later)
- adjust scrolling touch sensitivity in 6 settings
- adjust menu height in 6 settings
- toggle vibrator on/off when using bottom virtual buttons
- toggle key repeat for volume up/down scrolling when maintained pressed: needs recovery restart to take effect
- all toggles are applied live except key repeat

.

__Additional features through PhilZ Settings__

- openrecovery script support
- backup and flash kernels directly from recovery
- time stamped kernel backups
- backup and restore EFS from recovery
- flash modem from recovery
- browse phone with root access using full GUI in Aroma File Manager: default location or browse for path
- support openrecovery script in 3 modes using a smart one touch menu: goomanager, default custom scripts location, browse for script
- poweroff, reboot to recovery and reboot to download mode options

.

.


__<center><big>PhilZ Touch 3 Quick Guide</big></center>__

.

__Special Backup and Restore menu__

- Backup/Flash Kernel: backup kernel in time stamped boot_$date.img file. To flash kernels, they must be in one of the sdcards under clockworkmod/.kernel_bak folder. If you have zImage files, rename them to *.img file

- Backup/Flash EFS: creates/restores efs.img file under one of the sdcards in clockworkmod/.efsbackup folder

- Flash modem: you can put *.bin modem files under clockworkmod/.modem_bak folder in one of the sdcards. Note, if you recompile my recovery from source, you must verify that recovery.fstab file for your device has the /modem entry or you need to add it manually

.

__Open Recovery Script Support (ORS) *Credits to sk8erwitskil__

On start, recovery looks automatically for <code>/cache/recovery/openrecoveryscript</code> installed by goomanager
If it finds it, it is run and phone will reboot

You can also add custom ors scripts you edit your self. When pressing the ors menu, it will look at default locations for your custom scripts:

<code>clockworkmod/ors</code> in external sd, then <code>clockworkmod/ors</code> in internal sd. Put your custom scripts there with file extension __.ors__

That way you can access your jobs (flash, wipe, backup, restore...) instantly

If no scripts are found in previous 2 steps, you get option to browse both sdcards for a custom locatiom

To learn how to write ors scripts to automate your backup/restore/wipe/flash tasks, read here, it is very easy: <code>http://wiki.rootzwiki.com/OpenRecoveryScript</code>
Give Goomanager a try

.

__Aroma File Manager Support *Credits to amarullz and sk8erwitskil__

You get here the possibility to browse your phone with root access in a friendly GUI file browser, while being in recovery
You even now get a terminal emulator to run in recovery

- Download Aroma File Manager from its Home Page
- Get the 1.80 version and name the file aromafm.zip
- Put the aromafm.zip in <code>clockworkmod/.aromafm/aromafm.zip</code> in external or internal sdcards
- You can eventually put it elsewhere, but you'll have to browse manually in that case

In recovery, tap the ORS Menu

It will first look in default locations. If it founds the aromafm.zip file, it will launch the file manager automatically
Else, it will let you browse sdcards for a custom location
Putting it in default locations makes it a tap and lauch button

.

__Touch GUI Preferences__

You can toggle through 3 touch modes:

<u>Full Touch</u>: menus are validated by touching them. I added extra checks to make it robust to validation by error while scrolling. After scrolling, your first touch will only highlight touched menu instead of validate it: that's not a bug but an on purpose security feature.

When Full Touch mode is selected, it will automatically set recommended menu height and touch sensitivity. You can alter them later if you want

<u>Double Tap</u>: menus are highlighted on first touch. To validate action, you need to double tap the menu

<u>Semi Touch</u>: the classic semi-touch interface I enhanced. Menus are selected/highlighted on first touch. You can scroll and return to previous menu by swiping left, but no validation on touch.

<u>Adjust touch sensitivity</u> when scrolling up/down. You get 6 custom settings

<u>Menu height can be adjusted in 6 settings</u>

<u>Toggle vibrator on/off</u> when touching bottom virtual buttons

<u>Toggle key repeat</u>: enables contiuous scrolling while you maintain pressure on vol up/down keys (need recovery restart to take effect)*

<u>Config files</u> are saved in <code>/system/philz-cwm6/</code> folder. That way your settings are saved and remembered after restart or even a factory reset

<u>All settings are applied live</u>, no restart needed (except toggle key repeat) and they are saved after recovery restart

