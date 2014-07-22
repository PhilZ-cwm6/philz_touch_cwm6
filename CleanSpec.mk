# Copyright (C) 2007 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# If you don't need to do a full clean build but would like to touch
# a file or delete some intermediate files, add a clean step to the end
# of the list.  These steps will only be run once, if they haven't been
# run before.
#
# E.g.:
#     $(call add-clean-step, touch -c external/sqlite/sqlite3.h)
#     $(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libz_intermediates)
#
# Always use "touch -c" and "rm -f" or "rm -rf" to gracefully deal with
# files that are missing or have been moved.
#
# Use $(PRODUCT_OUT) to get to the "out/target/product/blah/" directory.
# Use $(OUT_DIR) to refer to the "out" directory.
#
# If you need to re-do something that's already mentioned, just copy
# the command and add it to the bottom of the list.  E.g., if a change
# that you made last week required touching a file and a change you
# made today requires touching the same file, just copy the old
# touch step and add it to the end of the list.
#
# ************************************************
# NEWER CLEAN STEPS MUST BE AT THE END OF THE LIST
# ************************************************

# For example:
#$(call add-clean-step, rm -rf $(OUT_DIR)/target/common/obj/APPS/AndroidTests_intermediates)
#$(call add-clean-step, rm -rf $(OUT_DIR)/target/common/obj/JAVA_LIBRARIES/core_intermediates)
#$(call add-clean-step, find $(OUT_DIR) -type f -name "IGTalkSession*" -print0 | xargs -0 rm -f)
#$(call add-clean-step, rm -rf $(PRODUCT_OUT)/data/*)

# ************************************************
# NEWER CLEAN STEPS MUST BE AT THE END OF THE LIST
# ************************************************
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/EXECUTABLES/recovery_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/EXECUTABLES/dump_image_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/EXECUTABLES/flash_image_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/EXECUTABLES/erase_image_intermediates)

$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libflashutils_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libmmcutils_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libmtdutils_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libbmlutils_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libcrecovery_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libvoldclient_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libminui_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libdedupe_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libedify_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libdump_image_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/liberase_image_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libflash_image_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libminizip_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libminadbd_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libmkyaffs2image_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libunyaffs_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libminelf_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libbml_over_mtd_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libmake_ext4fs_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libtouch_gui_intermediates)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/STATIC_LIBRARIES/libloki_static_intermediates)

$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/SHARED_LIBRARIES/libminui_intermediates)

$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/RECOVERY_EXECUTABLES)

$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/EXECUTABLES/updater_intermediates)
