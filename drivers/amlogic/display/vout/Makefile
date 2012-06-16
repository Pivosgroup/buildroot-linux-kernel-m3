obj-y 	+=display.o
display-objs=vout_notify.o  vout_serve.o



obj-$(CONFIG_AM_TV_OUTPUT)  += am_tvout.o enc_clk_config.o
am_tvout-objs :=   tvoutc.o  tvconf.o 

obj-$(CONFIG_AM_LCD_MLVDS_OUTPUT)  += mlvds.o

obj-$(CONFIG_AML_MLVDS_A_TM080XDH)  += lcd_mlvds/A_TM080XDH.o

obj-$(CONFIG_AML_MLVDS_G_TM080XDH)  += lcd_mlvds/G_TM080XDH.o

obj-$(CONFIG_AML_MLVDS_G_TM080XDH2D)  += lcd_mlvds/G_TM080XDH_2D.o

obj-$(CONFIG_AM_LCD_LVDS_OUTPUT)  += lvds.o

obj-$(CONFIG_AML_LVDS_R_AT070TNA2)  += lcd_lvds/R_AT070TNA2.o

obj-$(CONFIG_AML_LVDS_HV070WSA_100)  += lcd_lvds/HV070WSA_100.o

obj-$(CONFIG_AML_LVDS_G_TM081JF01)  += lcd_lvds/G_TM081JFH01.o

obj-$(CONFIG_AML_LVDS_G_W13)  += lcd_lvds/w13.o

obj-$(CONFIG_AM_LCD_TTL_OUTPUT)  += tcon.o

obj-$(CONFIG_AML_TCON_W7NEW)  += lcd_ttl/w7new.o

obj-$(CONFIG_AML_TCON_W8)  += lcd_ttl/w8.o

obj-$(CONFIG_AML_TCON_Y7)  += lcd_ttl/y7.o

obj-$(CONFIG_AML_TCON_S7)  += lcd_ttl/s7.o

#EXTRA_CFLAGS += -DCONFIG_PM