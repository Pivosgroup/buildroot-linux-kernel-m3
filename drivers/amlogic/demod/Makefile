
obj-$(CONFIG_AM_DEMOD) += aml_fe.o

aml_fe-objs := demod_func.o dvbt_func.o dvbc_func.o i2c_func.o tuner_func.o

aml_fe-objs += mxl/MxL5007.o
aml_fe-objs += mxl/MxL5007_API.o
aml_fe-objs += mxl/MxL_User_Define.o
aml_fe-objs += nxp/tuner_fj2207.o
aml_fe-objs += nxp/tmbslNT220x.o
aml_fe-objs += nxp/tmbslNT220xInstance.o
aml_fe-objs += nxp/tmddNT220x.o
aml_fe-objs += nxp/tmddNT220xInstance.o
aml_fe-objs += nxp/tmddNT220x_Advanced.o

ifeq ($(CONFIG_AM_DEMOD_DEBUG), y)
  aml_fe-objs	+= aml_demod.o
else
  aml_fe-objs	+= amlfrontend.o
endif

EXTRA_CFLAGS += -Idrivers/media/dvb/dvb-core

