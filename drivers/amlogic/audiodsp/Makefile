
# Makefile for osd and tv 

audiodsp-objs=	audiodsp_module.o \
				dsp_control.o 		\
				dsp_microcode.o 	\
				dsp_mailbox.o			\
				dsp_monitor.o			\
				dsp_codec.o
audiodsp-objs += pcmenc_module.o pcmenc_stream.o
audiodsp-objs += spdif_module.o

obj-$(CONFIG_AML_AUDIO_DSP)    +=audiodsp.o

#EXTRA_CFLAGS = -DENABLE_WAIT_FORMAT
