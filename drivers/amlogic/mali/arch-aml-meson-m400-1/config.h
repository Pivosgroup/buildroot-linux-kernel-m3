/*
 * Copyright (C) 2011 Amlogic, Inc. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef __ARCH_CONFIG_H__
#define __ARCH_CONFIG_H__

#define INT_MALI_GP	48
#define INT_MALI_GP_MMU 49
#define INT_MALI_PP	50
#define INT_MALI_PP_MMU 51

/* Configuration for the AMLOGIC MESON platform */

static _mali_osk_resource_t arch_configuration [] =
{
	{
		.type = MALI400GP,
		.description = "Mali-400 GP",
		.base = 0xd0060000,
		.irq = INT_MALI_GP,
		.mmu_id = 1
	},
	{
		.type = MALI400PP,
		.base = 0xd0068000,
		.irq = INT_MALI_PP,
		.description = "Mali-400 PP",
		.mmu_id = 2
	},
#if USING_MMU
	{
		.type = MMU,
		.base = 0xd0063000,
		.irq = INT_MALI_GP_MMU,
		.description = "Mali-400 MMU for GP",
		.mmu_id = 1
	},
	{
		.type = MMU,
		.base = 0xd0064000,
		.irq = INT_MALI_PP_MMU,
		.description = "Mali-400 MMU for PP",
		.mmu_id = 2
	},
#endif
	{
		.type = OS_MEMORY,
		.description = "Mali Memory",
		.alloc_order = 5, /* Medium preference for this memory */
		.size = 0x08000000,
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_READABLE | _MALI_PP_WRITEABLE |_MALI_GP_READABLE | _MALI_GP_WRITEABLE
	},
	{
		.type = MEM_VALIDATION,
		.description = "Framebuffer",
		.base = 0x84000000,
		.size = 0x06000000,
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_WRITEABLE | _MALI_PP_READABLE
	},
	{
		.type = MALI400L2,
		.base = 0xD0061000,
		.description = "Mali-400 L2 cache"
	},
};

#endif /* __ARCH_CONFIG_H__ */
