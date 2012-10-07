HOSTDRIVER_DIR ?= $(shell pwd)/drivers/amlogic/wifi/nano_c047_12
OBJ_DIR=$(HOSTDRIVER_DIR)/obj
-include $(HOSTDRIVER_DIR)/platform/defs.inc
include $(HOSTDRIVER_DIR)/platform/$(PLATFORM)/platform.inc
include $(HOSTDRIVER_DIR)/common.inc


V?=0
ifeq ($V,0)
	Q=@
endif	

ifeq ($(KERNEL_MAJOR_VERSION),2.4)
	MAKEFILE=Makefile-2.4
else
	MAKEFILE=Makefile
endif

obj-y += kernel/ic/
obj-y += kernel/ksdio/

