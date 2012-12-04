/*
 * AMLOGIC Canvas management driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 */

#ifndef CANVAS_H
#define CANVAS_H

#include <linux/types.h>
#include <linux/kobject.h>

typedef struct {
    struct kobject kobj;
    ulong addr;
    u32 width;
    u32 height;
    u32 wrap;
    u32 blkmode;
} canvas_t;

#define OSD1_CANVAS_INDEX 0x40
#define OSD2_CANVAS_INDEX 0x43
#define OSD3_CANVAS_INDEX 0x41
#define OSD4_CANVAS_INDEX 0x42
#define ALLOC_CANVAS_INDEX  0x46

#define GE2D_MAX_CANVAS_INDEX   0x5f

#define DISPLAY_CANVAS_BASE_INDEX   0x60
#define DISPLAY_CANVAS_MAX_INDEX    0x65 

#define DISPLAY2_CANVAS_BASE_INDEX   0x66
#define DISPLAY2_CANVAS_MAX_INDEX    0x6b 

    /* DEINTERLACE_CANVAS_MAX_INDEX is 0x77 because the canvas start from 0x78 are used by h264 */
#define DI_USE_FIXED_CANVAS_IDX
#ifdef DI_USE_FIXED_CANVAS_IDX
#define DI_PRE_MEM_NR_CANVAS_IDX        0x70
#define DI_PRE_CHAN2_NR_CANVAS_IDX      0x71
#define DI_PRE_WR_NR_CANVAS_IDX         0x72
#define DI_PRE_WR_MTN_CANVAS_IDX        0x73
#define DI_POST_BUF0_CANVAS_IDX         0x74
#define DI_POST_BUF1_CANVAS_IDX         0x75
#define DI_POST_MTNCRD_CANVAS_IDX       0x76
#define DI_POST_MTNPRD_CANVAS_IDX       0x77
#else
#define DEINTERLACE_CANVAS_BASE_INDEX	0x70
#define DEINTERLACE_CANVAS_MAX_INDEX	0x7f
#endif


#define FREESCALE_CANVAS_INDEX 0x50   //for osd&video scale use
#define MAX_FREESCALE_CANVAS_INDEX 0x5f

extern void canvas_config(u32 index, ulong addr, u32 width,
                          u32 height, u32 wrap, u32 blkmode);

extern void canvas_read(u32 index, canvas_t *p);

extern void canvas_copy(unsigned src, unsigned dst);

extern void canvas_update_addr(u32 index, u32 addr);

extern unsigned int canvas_get_addr(u32 index);

#endif /* CANVAS_H */
