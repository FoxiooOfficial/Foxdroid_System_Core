LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

TOOLS := \
	cat \
	chmod \
	chown \
	cmp \
	date \
	dd \
	df \
	dmesg \
	getevent \
	getprop \
	hd \
	help \
	id \
	ifconfig \
	iftop \
	insmod \
	ioctl \
	ionice \
	kill \
	ln \
	log \
	ls \
	lsmod \
	lsof \
	mkdir \
	mount \
	mv \
	nandread \
	netstat \
	newfs_msdos \
	notify \
	printenv \
	ps \
	renice \
	rm \
	rmdir \
	rmmod \
	route \
	schedtop \
	sendevent \
	setconsole \
	setprop \
	sleep \
	smd \
	start \
	stop \
	sync \
	top \
	touch \
	umount \
	uptime \
	vmstat \
	watchprops \
	wipe

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
TOOLS += r
endif

LOCAL_SRC_FILES:= \
	dynarray.c \
	toolbox.c \
	$(patsubst %,%.c,$(TOOLS))

TOOLS += reboot

ifeq ($(BOARD_USES_BOOTMENU),true)
	LOCAL_SRC_FILES += ../../../external/bootmenu/libreboot/reboot.c
else
	LOCAL_SRC_FILES += reboot.c
endif

LOCAL_SHARED_LIBRARIES := libcutils libc libusbhost

LOCAL_MODULE:= toolbox

# Including this will define $(intermediates).
#
include $(BUILD_EXECUTABLE)

$(LOCAL_PATH)/toolbox.c: $(intermediates)/tools.h

TOOLS_H := $(intermediates)/tools.h
$(TOOLS_H): PRIVATE_TOOLS := $(TOOLS)
$(TOOLS_H): PRIVATE_CUSTOM_TOOL = echo "/* file generated automatically */" > $@ ; for t in $(PRIVATE_TOOLS) ; do echo "TOOL($$t)" >> $@ ; done
$(TOOLS_H): $(LOCAL_PATH)/Android.mk
$(TOOLS_H):
	$(transform-generated-source)

# Make #!/system/bin/toolbox launchers for each tool.
#
SYMLINKS := $(addprefix $(TARGET_OUT)/bin/,$(TOOLS))
$(SYMLINKS): TOOLBOX_BINARY := $(LOCAL_MODULE)
$(SYMLINKS): $(LOCAL_INSTALLED_MODULE) $(LOCAL_PATH)/Android.mk
	@echo "Symlink: $@ -> $(TOOLBOX_BINARY)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(TOOLBOX_BINARY) $@

ALL_DEFAULT_INSTALLED_MODULES += $(SYMLINKS)

# We need this so that the installed files could be picked up based on the
# local module name
ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
    $(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) $(SYMLINKS)
