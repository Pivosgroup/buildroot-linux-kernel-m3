obj-$(CONFIG_AML_HDMI_TX)		+= hdmitx.o


hdmitx-objs := hdmi_tx.o hdmi_tx_cec.o hdmi_tx_video.o hdmi_tx_audio.o hdmi_tx_edid.o hdmi_tx_audio.o \

obj-$(CONFIG_ARCH_MESON3)       += m3/hdmi_tx_reg.o m3/hdmi_tx_hw.o
obj-$(CONFIG_ARCH_MESON)        += m1/hdmi_tx_reg.o m1/hdmi_tx_hw.o

obj-$(CONFIG_AML_HDMI_TX_HDCP) += hdmi_tx_hdcp.o

EXTRA_CFLAGS += -O0


