#
# Makefile for the linux kernel.
#

# Common support
obj-y := core.o clock.o common.o fiq.o fiq_bridge.o
obj-m :=
obj-n :=
obj-  :=

obj-$(CONFIG_CPU_FREQ) 		+= cpu.o
obj-$(CONFIG_ARCH_MESON)	+= pinmux.o
obj-$(CONFIG_ARCH_MESON)	+= gpio.o
obj-$(CONFIG_ARCH_MESON)	+= power_gate.o
obj-$(CONFIG_ARCH_MESON)	+= sd_pad.o
obj-$(CONFIG_ARCH_MESON)	+= lm.o usbclock.o sataclock.o clk_set.o sram.o sleep.o

obj-$(CONFIG_MACH_MESON_8726M_REFA00)	+= board-8726m-refa00.o

obj-$(CONFIG_MACH_MESON_8726M_REFB01)	+= board-8726m-refb01.o
obj-$(CONFIG_MACH_MESON_8726M_REFB02)	+= board-8726m-refb02.o
obj-$(CONFIG_MACH_MESON_8726M_REFB03)	+= board-8726m-refb03.o
obj-$(CONFIG_MACH_MESON_8726M_REFB04)	+= board-8726m-refb04.o
obj-$(CONFIG_MACH_MESON_8726M_REFB05)	+= board-8726m-refb05.o
obj-$(CONFIG_MACH_MESON_8726M_REFB06)	+= board-8726m-refb06.o
obj-$(CONFIG_MACH_MESON_8726M_REFB07)	+= board-8726m-refb07.o
obj-$(CONFIG_MACH_MESON_8726M_REFB08)	+= board-8726m-refb08.o
obj-$(CONFIG_MACH_MESON_8726M_REFB09)	+= board-8726m-refb09.o
obj-$(CONFIG_MACH_MESON_8726M_REFB09)	+= amlogic-wifi.o
obj-$(CONFIG_MACH_MESON_8726M_REFB10)	+= board-8726m-refb10.o
obj-$(CONFIG_MACH_MESON_8726M_REFB11)	+= board-8726m-refb11.o
obj-$(CONFIG_MACH_MESON_8726M_REFB12)	+= board-8726m-refb12.o
obj-$(CONFIG_MACH_MESON_8726M_REFB13)	+= board-8726m-refb13.o
obj-$(CONFIG_MACH_MESON_8726M_REFB14)	+= board-8726m-refb14.o
obj-$(CONFIG_MACH_MESON_8726M_REFB16)	+= board-8726m-refb16.o
obj-$(CONFIG_MACH_MESON_8726M_REFB17)	+= board-8726m-refb17.o
obj-$(CONFIG_MACH_MESON_8726M_REFB18)	+= board-8726m-refb18.o
obj-$(CONFIG_MACH_MESON_8726M_REFB19)	+= board-8726m-refb19.o

obj-$(CONFIG_MACH_MESON_8726M_REFC01)	+= board-8726m-refc01.o
obj-$(CONFIG_MACH_MESON_8626M_REFC02)	+= board-8626m-refc02.o
obj-$(CONFIG_MACH_MESON_8726M_REFC03)	+= board-8726m-refc03.o
obj-$(CONFIG_MACH_MESON_8726M_REFC04)	+= board-8726m-refc04.o
obj-$(CONFIG_MACH_MESON_8726M_REFC05)	+= board-8726m-refc05.o
obj-$(CONFIG_MACH_MESON_8726M_REFC06)	+= board-8726m-refc06.o
obj-$(CONFIG_MACH_MESON_8726M_REFC08)	+= board-8726m-refc08.o

obj-$(CONFIG_MACH_MESON_6236M)	+= board-6236m.o
obj-$(CONFIG_MACH_MESON_8226M)	+= board-8226m.o

obj-$(CONFIG_MACH_MESON_8726M_G7)	+= board-8726m-g7.o
obj-$(CONFIG_MACH_MESON_6236M_SH)	+= board-6236m-sh.o
obj-$(CONFIG_MACH_MESON_8626M_SH)	+= board-8626m-sh.o

# Power Management
obj-$(CONFIG_SUSPEND)			+= pm.o
