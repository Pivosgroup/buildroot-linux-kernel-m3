#
# Makefile for DWC_otg Highspeed USB controller driver
#

KBUILD_CFLAGS += -DDEBUG 

# Use one of the following flags to compile the software in host-only or
# device-only mode.
#CPPFLAGS        += -DDWC_HOST_ONLY
#CPPFLAGS        += -DDWC_DEVICE_ONLY

#CPPFLAGS	+= -Dlinux -DDWC_HS_ELECT_TST


dwc_otg-objs	+= dwc_otg_driver.o dwc_otg_attr.o 
dwc_otg-objs	+= dwc_otg_cil.o dwc_otg_cil_intr.o 
dwc_otg-objs	+= dwc_otg_pcd.o dwc_otg_pcd_intr.o 
dwc_otg-objs	+= dwc_otg_hcd.o dwc_otg_hcd_intr.o dwc_otg_hcd_queue.o

obj-$(CONFIG_USB_DWC_OTG_HCD)	+= dwc_otg.o


