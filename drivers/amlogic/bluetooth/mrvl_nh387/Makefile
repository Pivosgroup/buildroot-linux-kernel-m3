# File: Makefile
# Copyright (C) 2007-2010, Marvell International Ltd.
#

CC=		$(CROSS_COMPILE)gcc
LD=		$(CROSS_COMPILE)ld

BACKUP=		/root/backup
YMD=		`date +%Y%m%d%H%M`

#############################################################################
# Configuration Options
#############################################################################
# PXA3XX DMA alignment
CONFIG_PXA3XX_DMA_ALIGN=y

# SDIO suspend/resume
CONFIG_SDIO_SUSPEND_RESUME=y

#############################################################################
# Select Platform Tools
#############################################################################


EXTRA_CFLAGS += -Idrivers/amlogic/bluetooth/mrvl_nh387

#############################################################################
# Compiler Flags
#############################################################################
	EXTRA_CFLAGS += -DFPNUM='"57"'

ifeq ($(CONFIG_MRVLNH387_DEBUG),"1")
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
endif

ifeq ($(CONFIG_MRVLNH387_DEBUG),"2")
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
	EXTRA_CFLAGS += -DDEBUG_LEVEL2
	DBG=	-dbg
endif

ifdef CONFIG_MRVLNH387_PXA3XX_DMA_ALIGN
	EXTRA_CFLAGS += -DPXA3XX_DMA_ALIGN
endif

ifdef CONFIG_MRVLNH387_SDIO_SUSPEND_RESUME
	EXTRA_CFLAGS += -DSDIO_SUSPEND_RESUME
endif

#############################################################################
# Make Targets
#############################################################################

obj-$(CONFIG_SDIO_MARVELL_NH387_BT) += bt8xxx.o

bt8xxx-objs += bt_main.o \
               bt_sdiommc.o \
               bt_proc.o