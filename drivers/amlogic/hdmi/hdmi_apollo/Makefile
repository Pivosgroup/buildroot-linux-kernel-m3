apollo_hdmi-objs=hdmi_module.o hdmi_i2c.o hdmi_video.o

apollo_hdmi-objs+=hdmi_global.o hdmi_vmtables.o
####add configs for other devies later..


apollo_hdmi-objs+=cat661x/hdmi_cat_audio.o	\
						cat661x/hdmi_cat_edid.o	\
						cat661x/hdmi_cat_hdcp.o	\
						cat661x/hdmi_cat_info_set.o		\
						cat661x/hdmi_cat_mddc.o	\
						cat661x/hdmi_cat_video.o	\
						cat661x/hdmi_cat_op.o	\
						cat661x/hdmi_cat_interrupt.o	



obj-$(CONFIG_AML_APOLLO_HDMI) +=apollo_hdmi.o 

EXTRA_CFLAGS += -Idrivers/video/apollo/hdmi/cat661x 
									
