#
# Makefile for the linux kernel.
#

# Object file lists.

obj-$(CONFIG_AM_STREAMING) += amports.o
amports-objs := amstream.o streambuf.o vdec.o adec.o tsdemux.o psparser.o esparser.o rmparser.o amvdec.o amve.o amcm.o amstream_profile.o
amports-y    := amstream.o streambuf.o vdec.o adec.o tsdemux.o psparser.o esparser.o rmparser.o amvdec.o amve.o amcm.o amstream_profile.o

obj-$(CONFIG_AM_VDEC_MPEG12) += amvdec_mpeg12.o
amvdec_mpeg12-objs := vmpeg12.o
amvdec_mpeg12-y    := vmpeg12.o

obj-$(CONFIG_AM_VDEC_MPEG4) += amvdec_mpeg4.o
amvdec_mpeg4-objs := vmpeg4.o
amvdec_mpeg4-y    := vmpeg4.o

obj-$(CONFIG_AM_VDEC_VC1) += amvdec_vc1.o
amvdec_vc1-objs := vvc1.o
amvdec_vc1-y    := vvc1.o

obj-$(CONFIG_AM_VDEC_H264) += amvdec_h264.o
amvdec_h264-objs := vh264.o
amvdec_h264-y    := vh264.o

obj-$(CONFIG_AM_VDEC_H264MVC) += amvdec_h264mvc.o
amvdec_h264mvc-objs := vh264mvc.o
amvdec_h264mvc-y    := vh264mvc.o

obj-$(CONFIG_AM_VDEC_MJPEG) += amvdec_mjpeg.o
amvdec_mjpeg-objs := vmjpeg.o
amvdec_mjpeg-y    := vmjpeg.o

obj-$(CONFIG_AM_VDEC_REAL) += amvdec_real.o
amvdec_real-objs := vreal.o
amvdec_real-y    := vreal.o

obj-$(CONFIG_AM_JPEGDEC) += jpegdec.o

obj-$(CONFIG_AM_VIDEO) += amvideo.o
amvideo-objs := video.o vpp.o
amvideo-y    := video.o vpp.o

obj-$(CONFIG_AM_VIDEO2) += amvideo2.o
amvideo2-objs := video2.o vpp2.o
amvideo2-y    := video2.o vpp2.o

obj-$(CONFIG_AM_DEINTERLACE) += deinterlace.o

obj-$(CONFIG_AM_TIMESYNC) += amtsync.o
amtsync-objs := tsync.o timestamp.o
amtsync-y    := tsync.o timestamp.o

obj-$(CONFIG_AM_PTSSERVER) += ptsserv.o

obj-$(CONFIG_AM_SUBTITLE) += subtitle.o

ifneq ($(CONFIG_AM_DVB),)
EXTRA_CFLAGS = -DENABLE_DEMUX_DRIVER
endif
