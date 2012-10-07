#
# Makefile for EXTERNAL DEVICE.
#


obj-$(CONFIG_CAMERA_OV2655) += camera_ov2655.o
camera_ov2655-objs := ov2655.o
#camera_ov2655-y    := ov2655.o ov2655_reg.o

obj-$(CONFIG_CAMERA_OV2665) += camera_ov2665.o
camera_ov2665-objs := ov2665.o


#obj-$(CONFIG_CAMERA_OV5640) += camera_ov5640.o
#camera_ov5640-objs := amlogic_camera_common.o
#camera_ov5640-objs += amlogic_camera_ov5640.o
#camera_ov5640-objs += amlogic_camera_gc0308.o


#obj-$(CONFIG_CAMERA_GC0308) += camera_gc0308.o
#camera_gc0308-objs := gc0308.o


obj-$(CONFIG_AMLOGIC_CAMERA_ENABLE) += amlogic_camera.o
amlogic_camera-objs := amlogic_camera_common.o

obj-$(CONFIG_CAMERA_OV5640) += camera_ov5640.o
camera_ov5640-objs := amlogic_camera_ov5640.o

obj-$(CONFIG_CAMERA_GC0308) += camera_gc0308.o
camera_gc0308-objs := amlogic_camera_gc0308.o

obj-$(CONFIG_CAMERA_GT2005) += camera_gt2005.o
camera_gt2005-objs := amlogic_camera_gt2005.o
