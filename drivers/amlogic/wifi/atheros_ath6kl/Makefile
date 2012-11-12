ifneq ($(KERNELRELEASE),)

NOSTDINC_FLAGS := -I$(shell pwd)/drivers/amlogic/wifi/atheros_ath6kl/include/ \
	-include $(shell pwd)/drivers/amlogic/wifi/atheros_ath6kl/include/linux/compat-2.6.h\
	$(CFLAGS)
obj-$(CONFIG_ATH6KL_USB_WIFI)	:= compat/
obj-$(CONFIG_ATH6KL_USB_WIFI)	+= cfg80211/
obj-$(CONFIG_ATH6KL_USB_WIFI)	+= ath6kl/
#obj-m := compat/
#obj-m += cfg80211/
#obj-m += ath6kl/


ifeq ($(BT),)
obj-$(CONFIG_COMPAT_WIRELESS) += cfg80211/ ath6kl/
#obj-$(CONFIG_COMPAT_WIRELESS_MODULES) += drivers/net/wireless/




ifeq ($(CONFIG_STAGING_EXCLUDE_BUILD),)
endif

endif


else

export PWD :=	$(shell pwd)
CFLAGS += \
        -DCOMPAT_BASE_TREE="\"$(shell cat compat_base_tree)\"" \
        -DCOMPAT_BASE_TREE_VERSION="\"$(shell cat compat_base_tree_version)\"" \
        -DCOMPAT_PROJECT="\"Compat-wireless\"" \
        -DCOMPAT_VERSION="\"$(shell cat compat_version)\""

# These exported as they are used by the scripts
# to check config and compat autoconf
export COMPAT_CONFIG=config.mk
export CONFIG_CHECK=.$(COMPAT_CONFIG)_md5sum.txt
export COMPAT_AUTOCONF=include/linux/compat_autoconf.h
export CREL=$(shell cat $(PWD)/compat_version)
export CREL_PRE:=.compat_autoconf_
export CREL_CHECK:=$(CREL_PRE)$(CREL)

include $(PWD)/$(COMPAT_CONFIG)

all: modules

#modules: $(CREL_CHECK)
#	$(MAKE) -C $(KLIB_BUILD) M=$(PWD) modules
#	@touch $@

bt: $(CREL_CHECK)
	$(MAKE) -C $(KLIB_BUILD) M=$(PWD) BT=TRUE modules
	@touch $@

# With the above and this we make sure we generate a new compat autoconf per
# new relase of compat-wireless-2.6 OR when the user updates the 
# $(COMPAT_CONFIG) file
$(CREL_CHECK):
	@# Force to regenerate compat autoconf
	@rm -f $(CONFIG_CHECK)
	@touch $@
	@md5sum $(COMPAT_CONFIG) > $(CONFIG_CHECK)


clean:
	@if [ -d net -a -d $(KLIB_BUILD) ]; then \
		$(MAKE) -C $(KLIB_BUILD) M=$(PWD) clean ;\
	fi
	@rm -f $(CREL_PRE)*

.PHONY: all clean install uninstall unload btunload wlunload modules bt

endif

clean-files += Module.symvers Module.markers modules modules.order
clean-files += $(CREL_CHECK) $(CONFIG_CHECK)
