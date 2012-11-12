obj-$(CONFIG_AM_AVL6211) += avl6211_fe.o

avl6211_src = src
avl6211_fe-objs +=  avlfrontend.o	
avl6211_fe-objs +=  $(avl6211_src)/avl.o  $(avl6211_src)/avl_dvbsx.o	$(avl6211_src)/IBase.o	 $(avl6211_src)/avl_dvbsx.o	$(avl6211_src)/IDiseqc.o  $(avl6211_src)/II2C.o	\
										$(avl6211_src)/II2CRepeater.o  $(avl6211_src)/IRx.o  $(avl6211_src)/IBSP.o	$(avl6211_src)/LockSignal_Api.o  ##$(avl6211_src)/ExtSharpBS2S7HZ6306.o	 $(avl6211_src)/DiSEqC_source.o  
										
avl6211_fe-objs +=	$(avl6211_src)/IBlindscanAPI.o   $(avl6211_src)/IBlindScan.o

avl6211_fe-objs +=  AV2011/ExtAV2011.o	


EXTRA_CFLAGS += -Idrivers/media/dvb/dvb-core
EXTRA_CFLAGS += -Idrivers/amlogic/dvb/avl6211/include
EXTRA_CFLAGS += -Idrivers/amlogic/dvb/avl6211
EXTRA_CFLAGS += -Idrivers/amlogic/dvb/avl6211/AV2011
