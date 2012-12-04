KERNEL_VERSION	:= `uname -r`
M               := drivers/media/video/uvc
CPPFLAGS        := -I$(M)/include $(CPPFLAGS)
KBUILD_CPPFLAGS := -I$(M)/include $(KBUILD_CPPFLAGS)
LINUXINCLUDE    := -I$(M)/include $(LINUXINCLUDE)
EXTRA_CFLAGS	:= -include $(M)/compat.h


uvcvideo-objs  := uvc_driver.o uvc_queue.o uvc_v4l2.o uvc_video.o uvc_ctrl.o \
		  uvc_status.o uvc_isight.o
obj-$(CONFIG_USB_VIDEO_CLASS) += uvcvideo.o
