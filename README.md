__<center><big>PhilZ Touch Recovery 6 (ClockworkMod 6 based / Advanced Edition)</big></center>__

.

__Home page__
http://forum.xda-developers.com/showthread.php?t=2201860

#### Building

If you haven't build recovery ever before, please look up the thread linked above.
If you regularly build ROMs/Recoveries for your device, and have a working CWM setup
on your build machine, then you can quickly set up and build Philz Touch recovery as well

Check these two patches are present in your build/ directory
   1. https://github.com/CyanogenMod/android_build/commit/c1b0bb6
   2. https://github.com/CyanogenMod/android_build/commit/6b21727

Clone philz recovery to bootable/recovery-philz folder

    git clone https://github.com/PhilZ-cwm6/philz_touch_cwm6 bootable/recovery-philz -b cm-11.0

Now build with RECOVERY_VARIANT flag set to philz.

    . build/envsetup.sh && lunch && make -j8 recoveryimage RECOVERY_VARIANT=philz

or

    export RECOVERY_VARIANT=philz
    . build/envsetup.sh && lunch && make -j8 recoveryimage
