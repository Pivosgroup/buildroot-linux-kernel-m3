#
# Makefile for miscellaneous I2C chip drivers.
#
# Do not add new drivers to this directory! It is DEPRECATED.
#
# Device drivers are better grouped according to the functionality they
# implement rather than to the bus they are connected to. In particular:
# * Hardware monitoring chip drivers go to drivers/hwmon
# * RTC chip drivers go to drivers/rtc
# * I/O expander drivers go to drivers/gpio
#


obj-$(CONFIG_BOSCH_BMA222)	+= bma222.o   



ifeq ($(CONFIG_BOSCH_BMA222_ENABLE_INT1),y)
EXTRA_CFLAGS += -DBMA222_ENABLE_INT1
endif

ifeq ($(CONFIG_BOSCH_BMA222_ENABLE_INT2),y)
EXTRA_CFLAGS += -DBMA222_ENABLE_INT2
endif
