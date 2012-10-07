##########################################
######This Amlogic Drivers#######################

##########################################
##	THis the Boot Sequence	Do not changed  unbending
##########################################

###For Display, this is for quick logo show##############
obj-y += display/

#obj-y  += hdmi/

obj-$(CONFIG_POST_PROCESS_MANAGER) += ppmgr/

#####for Basic drivers#####
 

obj-y += uart/

obj-y += nand/

obj-y += net/

obj-y += usb/

obj-$(CONFIG_SATA_DWC_AHCI) += sata/

obj-y += canvas/

obj-y += amports/

obj-y += i2c/

obj-y += light_sensor/

obj-y += extern_io/

obj-y += input/

obj-y += spi_nor/

obj-$(CONFIG_CMEM) += cmem/

obj-$(CONFIG_DEINTERLACE) += deinterlace/
obj-$(CONFIG_JPEGLOGO) += jpeglogo/

obj-$(CONFIG_CARDREADER) += cardreader/

obj-$(CONFIG_MALI) += mali/

obj-$(CONFIG_UMP) += ump/

obj-$(CONFIG_TWX_TC101) += minilvds/

obj-$(CONFIG_AML_AUDIO_DSP) += audiodsp/
obj-$(CONFIG_AML_AUDIO_DSP_LOG) += audiodsplog/

obj-$(CONFIG_AML_RTC)+= rtc/

obj-$(CONFIG_GPS_POWERCTRL) += gps/

obj-y += power/

obj-$(CONFIG_AM_GPIO) +=gpio/

obj-y += tvin/

obj-$(CONFIG_EFUSE)+=efuse/

obj-$(CONFIG_BOOT_MONITOR) += boot_monitor/
#####Sound system is later init in kernel default..######################
obj-$(CONFIG_AMAUDIO) += amaudio/

obj-y += hdmi/

obj-$(CONFIG_AM_DVB) += dvb/

obj-$(CONFIG_AM_SMARTCARD) += smartcard/

obj-y += demod/

obj-y += wifi/
obj-y += bluetooth/
#	camera devices
obj-y += camera/
obj-y += modem/
obj-y += debug/
obj-y += vfm/
