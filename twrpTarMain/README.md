
**twrpTar command usages**


 
   	twrpTar <action> [options]

  actions: 
  
        -c create 
        -x extract

 options:
 
    -d    target directory
    -t    output file
	-m    skip media subfolder (has data media)
	-z    compress backup (/sbin/pigz must be present)
	-e    encrypt/decrypt backup followed by password (/sbin/openaes must be present)
	-u    encrypt using userdata encryption (must be used with -e)

 Example: 
 
       twrpTar -c -d /cache -t /sdcard/test.tar
       twrpTar -x -d /cache -t /sdcard/test.tar



------------------------------------------------------------------------------------

**Backup cache partitions (ext4) logs**



      ~ # twrpTar -c -d /cache -t /sdcard/test-cache.tar
      I:Creating backup...
      I:Creating tar file '/sdcard/test-cache.tar'
      I:addFile '/cache/recovery' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/log' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.1' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.3' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_install' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.2' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.4' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.5' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.6' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.7' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.8' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.9' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.10' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/dalvik-cache' including root: 0
      setting selinux context: u:object_r:dalvikcache_data_file:s0
      I:addFile '/cache/backup' including root: 0
      setting selinux context: u:object_r:cache_backup_file:s0
      I:Thread id 0 tarList done, 0 archives.
      I:Thread ID 0 finished successfully.
      I:createTarFork() process ended with RC=0


      tar created successfully.



