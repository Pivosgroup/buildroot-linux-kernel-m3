#
# Makefile for the Amlogic NAND drivers.
#

obj-$(CONFIG_AM_NAND)	+= aml_nand.o
obj-$(CONFIG_ARCH_MESON)	+= m1_nand.o
obj-$(CONFIG_ARCH_MESON3)	+= m3_nand.o
obj-$(CONFIG_AML_NFTL)	+= aml_nftl/
ifeq ($(CONFIG_AML_NFTL_NEW), y)              
	obj-$(CONFIG_AML_NFTL_NEW)	+= aml_nftl_new/
else                                          
	obj-$(CONFIG_AML_NFTL)		+= aml_nftl/      
endif
obj-$(CONFIG_AML_NAND_KEY)	+=nand_key.o
