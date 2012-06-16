/*
 *
 * arch/arm/mach-meson/include/mach/platform.h
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * License terms: GNU General Public License (GPL) version 2
 * Basic platform init and mapping functions.
 */

#ifndef __ASM_ARCH_PLATFORM_H
#define __ASM_ARCH_PLATFORM_H

#ifndef __ASSEMBLY__

void meson_map_io(void);
void meson_init_irq(void);
void meson_init_devices(void);
void meson_cache_init(void);
extern struct sys_timer meson_sys_timer;

#endif

#endif
