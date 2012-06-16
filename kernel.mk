
##make -f kernel.mk TARGET=f02 INSTALL_ROOT=../xxxx/root/

TOP=$(PWD)

TARGET_OUT_TOP?= $(ANDROID_PRODUCT_OUT)

INSTALL_ROOT ?= $(TARGET_OUT_TOP)/root/

RECOVERY_ROOT?= $(shell echo $(INSTALL_ROOT)  | sed 's?ref/root/?ref/recovery/root/?g')

IMAGE_ROOT?= $(TARGET_OUT_TOP)

TARGET?=$(TARGET_PRODUCT:%ref=%)


MOD_DIR=$(TOP)/.out/modules

default:all

MMAKE?=make -j 7


.PHONE:=preconfig





preconfig:
	make meson_ref$(TARGET)_defconfig 	

.config_orig:preconfig
	cp .config .config_orig

config_uImage:.config_orig
	echo root=$(INSTALL_ROOT)
	sed 's?CONFIG_INITRAMFS_SOURCE=".*"?CONFIG_INITRAMFS_SOURCE=\"$(INSTALL_ROOT)\"?g'  $< > $<.tmp 
	cp -f $<.tmp $@



config_uImage_recovery:.config_orig
	echo root=$(RECOVERY_ROOT)
	sed 's?CONFIG_INITRAMFS_SOURCE=".*"?CONFIG_INITRAMFS_SOURCE=\"$(RECOVERY_ROOT)\"?g'  $< > $<.tmp 
	cp -f $<.tmp $@

#	CONFIG_INITRAMFS_SOURCE="../out/target/product/f02ref/root/"


pmodules:config_uImage	
	cp -f config_uImage .config
	mkdir -p $(MOD_DIR)
	$(MMAKE) modules	 MOD_INS_DIR=$(MOD_DIR)
	cp -f $(MOD_DIR)/mali.ko $(RECOVERY_ROOT)/boot/
	cp -f $(MOD_DIR)/ump.ko $(RECOVERY_ROOT)/boot/

puImage:config_uImage
	cp -f config_uImage .config
	$(MMAKE) uImage 
	cp arch/arm/boot/uImage uImage

puImage_recovery:config_uImage_recovery
	cp -f config_uImage_recovery .config
	$(MMAKE) uImage
	cp arch/arm/boot/uImage uImage_recovery

modules:config_uImage pmodules
uImage:config_uImage puImage
uImage_recovery:modules config_uImage_recovery puImage_recovery

stepbuild:preconfig config_uImage puImage pmodules puImage config_uImage_recovery puImage_recovery 
	

all:stepbuild

install:all
	cp -f uImage $(IMAGE_ROOT)/
	cp -f uImage_recovery $(IMAGE_ROOT)/
	cp -f $(MOD_DIR)/*.ko $(IMAGE_ROOT)/system/lib/
