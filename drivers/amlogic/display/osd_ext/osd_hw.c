/*
 * Amlogic Apollo
 * frame buffer driver
 *
 * Copyright (C) 2009 Amlogic, Inc.
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

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <mach/am_regs.h>
#include <linux/irqreturn.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/amports/canvas.h>
#include <linux/amlog.h>
#include <linux/amports/vframe_receiver.h>
#include <linux/osd/osd.h>

#define  FIQ_VSYNC
#include "osd_hw_def.h"
#include "osd_log.h"


static DECLARE_WAIT_QUEUE_HEAD(osd_ext_vsync_wq);
static bool vsync_hit = false;
static bool osd_ext_vf_need_update = false;

#if defined(CONFIG_ARCH_MESON3)
static u32 osd_ext_current_field = 0;
#endif
static struct vframe_provider_s osd_ext_vf_prov;
extern hw_para_t osd_hw;

/********************************************************************/
/***********        osd psedu frame provider        *****************/
/********************************************************************/
static vframe_t *osd_ext_vf_peek(void* op_arg)
{
	return ((osd_ext_vf_need_update && (vf.width > 0) && (vf.height > 0)) ? &vf : NULL);
}

static vframe_t *osd_ext_vf_get(void* op_arg)
{
	if (osd_ext_vf_need_update) {
		vf_ext_light_unreg_provider(&osd_ext_vf_prov);
		osd_ext_vf_need_update = false;
		return &vf;
	}
	return NULL;
}

#define PROVIDER_NAME   "osd_ext"
static const struct vframe_operations_s osd_ext_vf_provider = {
	.peek = osd_ext_vf_peek,
	.get  = osd_ext_vf_get,
	.put  = NULL,
};

static unsigned char osd_ext_vf_prov_init = 0;

static inline void  osd_ext_update_3d_mode(int enable_osd1, int enable_osd2)
{
	if (enable_osd1) {
		osd1_update_disp_3d_mode();
	}
	if (enable_osd2) {
		osd2_update_disp_3d_mode();
	}
}

static inline void wait_vsync_wakeup(void)
{
	vsync_hit = true;
	wake_up_interruptible(&osd_ext_vsync_wq);
}

static inline void  walk_through_update_list(void)
{
	u32 i, j;
	for (i = 0; i < HW_OSD_COUNT; i++) {
		for (j = 0; j < HW_REG_INDEX_MAX; j++) {
			if (osd_ext_hw.updated[i]
			    && (osd_ext_hw.updated[i] & (1 << j))
			    && (osd_ext_hw.reg[i][j].update_func != NULL)) {
				osd_ext_hw.reg[i][j].update_func();
			}
		}
	}
}

/**********************************************************************/
/**********          osd vsync irq handler              ***************/
/**********************************************************************/
#ifdef FIQ_VSYNC
static irqreturn_t vsync_isr(int irq, void *dev_id)
{
	wait_vsync_wakeup();

	return IRQ_HANDLED;
}
#endif

#ifdef FIQ_VSYNC
static void osd_ext_fiq_isr(void)
#else
static irqreturn_t vsync_isr(int irq, void *dev_id)
#endif
{
	unsigned int fb0_cfg_w0, fb1_cfg_w0;
	unsigned int current_field;

#ifdef CONFIG_AM_VIDEO2
	int viu1_sel = (READ_MPEG_REG(VPU_VIU_VENC_MUX_CTRL) >> 0) & 0x3; // 0=No connection, 1=ENCI, 2=ENCP, 3=ENCT.
	if ((viu1_sel == 1) && (READ_MPEG_REG(ENCI_VIDEO_EN) & 1)) {
		osd_ext_hw.scan_mode = SCAN_MODE_INTERLACE;
	}
#else
	if (READ_MPEG_REG(ENCI_VIDEO_EN) & 1) {
		osd_ext_hw.scan_mode = SCAN_MODE_INTERLACE;
	}
#endif
	else if (READ_MPEG_REG(ENCP_VIDEO_MODE) & (1 << 12)) {
		osd_ext_hw.scan_mode = SCAN_MODE_INTERLACE;
	} else {
		osd_ext_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	}

	if (osd_ext_hw.free_scale_enable[OSD1]) {
		osd_ext_hw.scan_mode = SCAN_MODE_PROGRESSIVE;
	}

	if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
		fb0_cfg_w0 = READ_MPEG_REG(VIU2_OSD1_BLK0_CFG_W0);
		fb1_cfg_w0 = READ_MPEG_REG(VIU2_OSD1_BLK0_CFG_W0 + REG_OFFSET);
#if defined(CONFIG_ARCH_MESON3)
		if (READ_MPEG_REG(ENCP_VIDEO_MODE) & (1 << 12)) {
			/* 1080I */

			if (READ_MPEG_REG(VENC_INTFLAG) & 0x200) {
				osd_ext_current_field = 0;
			} else {
				osd_ext_current_field = osd_ext_current_field ^ 1;
			}
		} else {
			if (READ_MPEG_REG(VENC_INTFLAG) & 4) {
				osd_ext_current_field = 0;
			} else {
				osd_ext_current_field = osd_ext_current_field ^ 1;
			}
		}
		current_field = osd_ext_current_field;
#else
		if (READ_MPEG_REG(ENCP_VIDEO_MODE) & (1 << 12)) {
			/* 1080I */

			if (READ_MPEG_REG(VENC_ENCP_LINE) >= 562) {
				/* bottom field */
				current_field = 0;
			} else {
				current_field = 1;
			}
		} else {
			current_field = READ_MPEG_REG(VENC_STATA) & 1;
		}
#endif
		fb0_cfg_w0 &= ~1;
		fb1_cfg_w0 &= ~1;
		fb0_cfg_w0 |= current_field;
		fb1_cfg_w0 |= current_field;

		WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W0, fb0_cfg_w0);
		WRITE_MPEG_REG(VIU2_OSD1_BLK1_CFG_W0, fb0_cfg_w0);
		WRITE_MPEG_REG(VIU2_OSD1_BLK2_CFG_W0, fb0_cfg_w0);
		WRITE_MPEG_REG(VIU2_OSD1_BLK3_CFG_W0, fb0_cfg_w0);
		WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W0 + REG_OFFSET, fb1_cfg_w0);
		WRITE_MPEG_REG(VIU2_OSD1_BLK1_CFG_W0 + REG_OFFSET, fb1_cfg_w0);
		WRITE_MPEG_REG(VIU2_OSD1_BLK2_CFG_W0 + REG_OFFSET, fb1_cfg_w0);
		WRITE_MPEG_REG(VIU2_OSD1_BLK3_CFG_W0 + REG_OFFSET, fb1_cfg_w0);
	}

	//go through update list
	walk_through_update_list();
	osd_ext_update_3d_mode(osd_ext_hw.mode_3d[OSD1].enable, osd_ext_hw.mode_3d[OSD2].enable);

	if (!vsync_hit) {
#ifdef FIQ_VSYNC
		fiq_bridge_pulse_trigger(&osd_ext_hw.fiq_handle_item);
#else
		wait_vsync_wakeup();
#endif
	}

#ifndef FIQ_VSYNC
	return  IRQ_HANDLED ;
#endif
}

void osd_ext_wait_vsync_hw(void)
{
	vsync_hit = false;

	wait_event_interruptible_timeout(osd_ext_vsync_wq, vsync_hit, HZ);
}

void osd_ext_set_gbl_alpha_hw(u32 index, u32 gbl_alpha)
{
	if (osd_ext_hw.gbl_alpha[index] != gbl_alpha) {

		osd_ext_hw.gbl_alpha[index] = gbl_alpha;
		add_to_update_list(index, OSD_GBL_ALPHA);

		osd_ext_wait_vsync_hw();
	}
}

u32 osd_ext_get_gbl_alpha_hw(u32  index)
{
	return osd_ext_hw.gbl_alpha[index];
}

void osd_ext_set_colorkey_hw(u32 index, u32 color_index, u32 colorkey)
{
	u8 r = 0, g = 0, b = 0, a = (colorkey & 0xff000000) >> 24;
	u32	data32;

	colorkey &= 0x00ffffff;
	switch (color_index) {
	case COLOR_INDEX_16_655:
		r = (colorkey >> 10 & 0x3f) << 2;
		g = (colorkey >> 5 & 0x1f) << 3;
		b = (colorkey & 0x1f) << 3;
		break;
	case COLOR_INDEX_16_844:
		r = colorkey >> 8 & 0xff;
		g = (colorkey >> 4 & 0xf) << 4;
		b = (colorkey & 0xf) << 4;
		break;
	case COLOR_INDEX_16_565:
		r = (colorkey >> 11 & 0x1f) << 3;
		g = (colorkey >> 5 & 0x3f) << 2;
		b = (colorkey & 0x1f) << 3;
		break;
	case COLOR_INDEX_24_888_B:
		b = colorkey >> 16 & 0xff;
		g = colorkey >> 8 & 0xff;
		r = colorkey & 0xff;
		break;
	case COLOR_INDEX_24_RGB:
	case COLOR_INDEX_YUV_422:
		r = colorkey >> 16 & 0xff;
		g = colorkey >> 8 & 0xff;
		b = colorkey & 0xff;
		break;
	}
	data32 = r << 24 | g << 16 | b << 8 | a;
	if (osd_ext_hw.color_key[index] != data32) {
		osd_ext_hw.color_key[index] = data32;
		amlog_mask_level(LOG_MASK_HARDWARE, LOG_LEVEL_LOW, "bpp:%d--r:0x%x g:0x%x b:0x%x ,a:0x%x\r\n", color_index, r, g, b, a);
		add_to_update_list(index, OSD_COLOR_KEY);

		osd_ext_wait_vsync_hw();
	}

	return;
}

void osd_ext_srckey_enable_hw(u32 index, u8 enable)
{
	if (enable != osd_ext_hw.color_key_enable[index]) {
		osd_ext_hw.color_key_enable[index] = enable;
		add_to_update_list(index, OSD_COLOR_KEY_ENABLE);

		osd_ext_wait_vsync_hw();
	}
}

void osd_ext_update_disp_axis_hw(
    u32 display_h_start,
    u32 display_h_end,
    u32 display_v_start,
    u32 display_v_end,
    u32 xoffset,
    u32 yoffset,
    u32 mode_change,
    u32 index)
{
	dispdata_t disp_data;
	pandata_t pan_data;

	if (NULL == osd_ext_hw.color_info[index]) {
		return ;
	}
	if (mode_change) { //modify pandata .
		add_to_update_list(index, OSD_COLOR_MODE);
	}
	disp_data.x_start = display_h_start;
	disp_data.y_start = display_v_start;
	disp_data.x_end = display_h_end;
	disp_data.y_end = display_v_end;

	pan_data.x_start = xoffset;
	pan_data.x_end = xoffset + (display_h_end - display_h_start);
	pan_data.y_start = yoffset;
	pan_data.y_end = yoffset + (display_v_end - display_v_start);

	//if output mode change then reset pan ofFfset.
	memcpy(&osd_ext_hw.pandata[index], &pan_data, sizeof(pandata_t));
	memcpy(&osd_ext_hw.dispdata[index], &disp_data, sizeof(dispdata_t));
	add_to_update_list(index, DISP_GEOMETRY);
	if (mode_change) {
		return ;
	}
	osd_ext_wait_vsync_hw();

}

void osd_ext_setup(struct osd_ctl_s *osd_ext_ctl,
                   u32 xoffset,
                   u32 yoffset,
                   u32 xres,
                   u32 yres,
                   u32 xres_virtual,
                   u32 yres_virtual,
                   u32 disp_start_x,
                   u32 disp_start_y,
                   u32 disp_end_x,
                   u32 disp_end_y,
                   u32 fbmem,
                   const color_bit_define_t *color,
                   int index
                  )
{
	u32 w = (color->bpp * xres_virtual + 7) >> 3;
	dispdata_t disp_data;
	pandata_t pan_data;

	pan_data.x_start = xoffset;
	pan_data.x_end = xoffset + (disp_end_x - disp_start_x);
	pan_data.y_start = yoffset;
	pan_data.y_end = yoffset + (disp_end_y - disp_start_y);

	disp_data.x_start = disp_start_x;
	disp_data.y_start = disp_start_y;
	disp_data.x_end = disp_end_x;
	disp_data.y_end = disp_end_y;

	if (osd_ext_hw.fb_gem[index].addr != fbmem || osd_ext_hw.fb_gem[index].width != w ||  osd_ext_hw.fb_gem[index].height != yres_virtual) {
		printk("****** osd_ext_setup() addr=%x, width=%d, height=%d *****\n", fbmem, w, yres_virtual);
		printk("****** osd_ext_setup() index=%d, canvas_idx=%d\n", index, osd_ext_hw.fb_gem[index].canvas_idx);
		osd_ext_hw.fb_gem[index].addr = fbmem;
		osd_ext_hw.fb_gem[index].width = w;
		osd_ext_hw.fb_gem[index].height = yres_virtual;
		canvas_config(osd_ext_hw.fb_gem[index].canvas_idx, osd_ext_hw.fb_gem[index].addr,
		              osd_ext_hw.fb_gem[index].width, osd_ext_hw.fb_gem[index].height,
		              CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	}

	if (color != osd_ext_hw.color_info[index]) {
		osd_ext_hw.color_info[index] = color;
		add_to_update_list(index, OSD_COLOR_MODE);
	}
	//control fb blank only by /sys/class/graphics/fbx/blank
	/*if(osd_ext_hw.enable[index] == DISABLE)
	{
		osd_ext_hw.enable[index]=ENABLE;
		add_to_update_list(index,OSD_ENABLE);

	}*/
	if (memcmp(&pan_data, &osd_ext_hw.pandata[index], sizeof(pandata_t)) != 0 ||
	    memcmp(&disp_data, &osd_ext_hw.dispdata[index], sizeof(dispdata_t)) != 0) {
		if (!osd_ext_hw.free_scale_enable[OSD1]) { //in free scale mode ,adjust geometry para is abandoned.
			memcpy(&osd_ext_hw.pandata[index], &pan_data, sizeof(pandata_t));
			memcpy(&osd_ext_hw.dispdata[index], &disp_data, sizeof(dispdata_t));
			add_to_update_list(index, DISP_GEOMETRY);
		}
	}

	osd_ext_wait_vsync_hw();
}

void osd_ext_setpal_hw(unsigned regno,
                       unsigned red,
                       unsigned green,
                       unsigned blue,
                       unsigned transp,
                       int index
                      )
{

	if (regno < 256) {
		u32 pal;
		pal = ((red   & 0xff) << 24) |
		      ((green & 0xff) << 16) |
		      ((blue  & 0xff) <<  8) |
		      (transp & 0xff);

		WRITE_MPEG_REG(VIU2_OSD1_COLOR_ADDR + REG_OFFSET * index, regno);
		WRITE_MPEG_REG(VIU2_OSD1_COLOR + REG_OFFSET * index, pal);
	}
}

u32 osd_ext_get_osd_ext_order_hw(u32 index)
{
	return  osd_ext_hw.osd_ext_order & 0x3;
}

void osd_ext_change_osd_ext_order_hw(u32 index, u32 order)
{
	if ((order != OSD_ORDER_01) && (order != OSD_ORDER_10)) {
		return ;
	}
	osd_ext_hw.osd_ext_order = order;
	add_to_update_list(index, OSD_CHANGE_ORDER);
	osd_ext_wait_vsync_hw();
}

void osd_ext_free_scale_enable_hw(u32 index, u32 enable)
{
	static dispdata_t save_disp_data = {0, 0, 0, 0};
	static pandata_t save_pan_data = {0, 0, 0, 0};
#ifdef CONFIG_AM_VIDEO
#ifdef CONFIG_POST_PROCESS_MANAGER
	int mode_changed = 0;
	if ((index == OSD1) && (osd_ext_hw.free_scale_enable[index] != enable)) {
		mode_changed = 1;
	}
#endif
#endif

	amlog_level(LOG_LEVEL_HIGH, "osd%d free scale %s\r\n", index, enable ? "ENABLE" : "DISABLE");
	osd_ext_hw.free_scale_enable[index] = enable;
	if (index == OSD1) {
		if (enable) {
			osd_ext_vf_need_update = true;
			if ((osd_ext_hw.free_scale_data[OSD1].x_end > 0) && (osd_ext_hw.free_scale_data[OSD1].x_end > 0)) {
				vf.width = osd_ext_hw.free_scale_data[index].x_end - osd_ext_hw.free_scale_data[index].x_start + 1;
				vf.height = osd_ext_hw.free_scale_data[index].y_end - osd_ext_hw.free_scale_data[index].y_start + 1;
			} else {
				vf.width = osd_ext_hw.free_scale_width[OSD1];
				vf.height = osd_ext_hw.free_scale_height[OSD1];
			}
			vf.type = (VIDTYPE_NO_VIDEO_ENABLE | VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD);
			vf.ratio_control = DISP_RATIO_FORCECONFIG | DISP_RATIO_NO_KEEPRATIO;
#ifdef CONFIG_AM_VIDEO
			if (osd_ext_vf_prov_init == 0) {
				vf_provider_init(&osd_ext_vf_prov, PROVIDER_NAME, &osd_ext_vf_provider, NULL);
				osd_ext_vf_prov_init = 1;
			}
			vf_reg_provider(&osd_ext_vf_prov);
#endif
			memcpy(&save_disp_data, &osd_ext_hw.dispdata[OSD1], sizeof(dispdata_t));
			memcpy(&save_pan_data, &osd_ext_hw.pandata[OSD1], sizeof(pandata_t));
			osd_ext_hw.pandata[OSD1].x_end = osd_ext_hw.pandata[OSD1].x_start + vf.width - 1 - osd_ext_hw.dispdata[OSD1].x_start;
			osd_ext_hw.pandata[OSD1].y_end = osd_ext_hw.pandata[OSD1].y_start + vf.height - 1;
			osd_ext_hw.dispdata[OSD1].x_end = osd_ext_hw.dispdata[OSD1].x_start + vf.width - 1;
			osd_ext_hw.dispdata[OSD1].y_end = osd_ext_hw.dispdata[OSD1].y_start + vf.height - 1;
			add_to_update_list(OSD1, DISP_GEOMETRY);
			add_to_update_list(OSD1, OSD_COLOR_MODE);
			osd_ext_wait_vsync_hw();
		} else {
			osd_ext_vf_need_update = false;
			if (save_disp_data.x_end <= save_disp_data.x_start ||
			    save_disp_data.y_end <= save_disp_data.y_start) {
				return ;
			}
			memcpy(&osd_ext_hw.dispdata[OSD1], &save_disp_data, sizeof(dispdata_t));
			memcpy(&osd_ext_hw.pandata[OSD1], &save_pan_data, sizeof(pandata_t));
			add_to_update_list(OSD1, DISP_GEOMETRY);
			add_to_update_list(OSD1, OSD_COLOR_MODE);
			osd_ext_wait_vsync_hw();
#ifdef CONFIG_AM_VIDEO
			vf_unreg_provider(&osd_ext_vf_prov);
#endif

		}
	} else {
		add_to_update_list(OSD2, DISP_GEOMETRY);
		add_to_update_list(OSD2, OSD_COLOR_MODE);
		osd_ext_wait_vsync_hw();
	}
	osd_ext_enable_hw(osd_ext_hw.enable[index], index);
#ifdef CONFIG_AM_VIDEO
#ifdef CONFIG_POST_PROCESS_MANAGER
	if (mode_changed) {
		//vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_RESET,NULL);
		extern void vf_ppmgr_reset_ext(void);
		//vf_ppmgr_reset_ext();
	}
#endif
#endif
}

void osd_ext_get_free_scale_enable_hw(u32 index, u32 *free_scale_enable)
{
	*free_scale_enable = osd_ext_hw.free_scale_enable[index];
}

void osd_ext_free_scale_width_hw(u32 index, u32 width)
{
	osd_ext_hw.free_scale_width[index] = width;
	if (osd_ext_hw.free_scale_enable[index]) {
		osd_ext_vf_need_update = true;
		vf.width = osd_ext_hw.free_scale_width[index];
		osd_ext_hw.pandata[OSD1].x_end = osd_ext_hw.pandata[OSD1].x_start + vf.width - 1 - osd_ext_hw.dispdata[OSD1].x_start;
		osd_ext_hw.dispdata[OSD1].x_end = osd_ext_hw.dispdata[OSD1].x_start + vf.width - 1;
		add_to_update_list(index, DISP_GEOMETRY);
		add_to_update_list(index, OSD_COLOR_MODE);
		osd_ext_wait_vsync_hw();
	}
}

void osd_ext_get_free_scale_width_hw(u32 index, u32 *free_scale_width)
{
	*free_scale_width = osd_ext_hw.free_scale_width[index];
}

void osd_ext_free_scale_height_hw(u32 index, u32 height)
{
	osd_ext_hw.free_scale_height[index] = height;
	if (osd_ext_hw.free_scale_enable[index]) {
		osd_ext_vf_need_update = true;
		vf.height = osd_ext_hw.free_scale_height[index];
		osd_ext_hw.pandata[OSD1].y_end = osd_ext_hw.pandata[OSD1].y_start + vf.height - 1;
		osd_ext_hw.dispdata[OSD1].y_end = osd_ext_hw.dispdata[OSD1].y_start + vf.height - 1;
		add_to_update_list(index, DISP_GEOMETRY);
		add_to_update_list(index, OSD_COLOR_MODE);
		osd_ext_wait_vsync_hw();
	}
}

void osd_ext_get_free_scale_height_hw(u32 index, u32 *free_scale_height)
{
	*free_scale_height = osd_ext_hw.free_scale_height[index];
}

void osd_ext_get_free_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1, s32 *y1)
{
	*x0 = osd_ext_hw.free_scale_data[index].x_start;
	*y0 = osd_ext_hw.free_scale_data[index].y_start;
	*x1 = osd_ext_hw.free_scale_data[index].x_end;
	*y1 = osd_ext_hw.free_scale_data[index].y_end;
}

void osd_ext_set_free_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1)
{
	osd_ext_hw.free_scale_data[index].x_start = x0;
	osd_ext_hw.free_scale_data[index].y_start = y0;
	osd_ext_hw.free_scale_data[index].x_end = x1;
	osd_ext_hw.free_scale_data[index].y_end = y1;

	if (osd_ext_hw.free_scale_enable[index]) {
		osd_ext_vf_need_update = true;
		vf.width = osd_ext_hw.free_scale_data[index].x_end - osd_ext_hw.free_scale_data[index].x_start + 1;
		vf.height = osd_ext_hw.free_scale_data[index].y_end - osd_ext_hw.free_scale_data[index].y_start + 1;
		osd_ext_hw.pandata[OSD1].x_end = osd_ext_hw.pandata[OSD1].x_start + vf.width - 1 - osd_ext_hw.dispdata[OSD1].x_start;
		osd_ext_hw.pandata[OSD1].y_end = osd_ext_hw.pandata[OSD1].y_start + vf.height - 1;
		osd_ext_hw.dispdata[OSD1].x_end = osd_ext_hw.dispdata[OSD1].x_start + vf.width - 1;
		osd_ext_hw.dispdata[OSD1].y_end = osd_ext_hw.dispdata[OSD1].y_start + vf.height - 1;

		add_to_update_list(index, DISP_GEOMETRY);
		add_to_update_list(index, OSD_COLOR_MODE);
		osd_ext_wait_vsync_hw();
	}
}

void osd_ext_get_scale_axis_hw(u32 index, s32 *x0, s32 *y0, s32 *x1, s32 *y1)
{
	*x0 = osd_ext_hw.scaledata[index].x_start;
	*x1 = osd_ext_hw.scaledata[index].x_end;
	*y0 = osd_ext_hw.scaledata[index].y_start;
	*y1 = osd_ext_hw.scaledata[index].y_end;
}

void osd_ext_set_scale_axis_hw(u32 index, s32 x0, s32 y0, s32 x1, s32 y1)
{
	osd_ext_hw.scaledata[index].x_start = x0;
	osd_ext_hw.scaledata[index].x_end = x1;
	osd_ext_hw.scaledata[index].y_start = y0;
	osd_ext_hw.scaledata[index].y_end = y1;
}

void osd_ext_get_osd_ext_info_hw(u32 index, s32(*posdval)[4], u32(*posdreq)[5], s32 info_flag)
{
	if (info_flag == 0) {
		posdval[0][0] = osd_ext_hw.pandata[index].x_start;
		posdval[0][1] = osd_ext_hw.pandata[index].x_end;
		posdval[0][2] = osd_ext_hw.pandata[index].y_start;
		posdval[0][3] = osd_ext_hw.pandata[index].y_end;

		posdval[1][0] = osd_ext_hw.dispdata[index].x_start;
		posdval[1][1] = osd_ext_hw.dispdata[index].x_end;
		posdval[1][2] = osd_ext_hw.dispdata[index].y_start;
		posdval[1][3] = osd_ext_hw.dispdata[index].y_end;

		posdval[2][0] = osd_ext_hw.scaledata[index].x_start;
		posdval[2][1] = osd_ext_hw.scaledata[index].x_end;
		posdval[2][2] = osd_ext_hw.scaledata[index].y_start;
		posdval[2][3] = osd_ext_hw.scaledata[index].y_end;
	} else if (info_flag == 1) {
		posdreq[0][0] = READ_CBUS_REG(VIU2_OSD1_BLK0_CFG_W0);
		posdreq[0][1] = READ_CBUS_REG(VIU2_OSD1_BLK0_CFG_W1);
		posdreq[0][2] = READ_CBUS_REG(VIU2_OSD1_BLK0_CFG_W2);
		posdreq[0][3] = READ_CBUS_REG(VIU2_OSD1_BLK0_CFG_W3);
		posdreq[0][4] = READ_CBUS_REG(VIU2_OSD1_BLK0_CFG_W4);

		posdreq[1][0] = READ_CBUS_REG(VIU2_OSD2_BLK0_CFG_W0);
		posdreq[1][1] = READ_CBUS_REG(VIU2_OSD2_BLK0_CFG_W1);
		posdreq[1][2] = READ_CBUS_REG(VIU2_OSD2_BLK0_CFG_W2);
		posdreq[1][3] = READ_CBUS_REG(VIU2_OSD2_BLK0_CFG_W3);
		posdreq[1][4] = READ_CBUS_REG(VIU2_OSD2_BLK0_CFG_W4);
	} else {
		;//ToDo
	}
}

void osd_ext_set_canvas_conf_hw(u32 index, u32 canvas_config)
{
	amlog_level(LOG_LEVEL_HIGH, "osd%d canvas_config %s\r\n", index, canvas_config ? "ENABLE" : "DISABLE");
	osd_ext_hw.canvas_conf[index] = canvas_config;

	add_to_update_list(index, DISP_CANVAS_CONFIG);
	osd_ext_wait_vsync_hw();
}

void osd_ext_get_block_windows_hw(u32 index, u32 *windows)
{
	memcpy(windows, osd_ext_hw.block_windows[index], sizeof(osd_ext_hw.block_windows[index]));
}

void osd_ext_set_block_windows_hw(u32 index, u32 *windows)
{
	memcpy(osd_ext_hw.block_windows[index], windows, sizeof(osd_ext_hw.block_windows[index]));
	add_to_update_list(index, DISP_GEOMETRY);
	osd_ext_wait_vsync_hw();
}

void osd_ext_get_block_mode_hw(u32 index, u32 *mode)
{
	*mode = osd_ext_hw.block_mode[index];
}

void osd_ext_set_block_mode_hw(u32 index, u32 mode)
{
	osd_ext_hw.block_mode[index] = mode;
	add_to_update_list(index, DISP_GEOMETRY);
	osd_ext_wait_vsync_hw();
}

void osd_ext_get_enforce_progressive_hw(u32 index, u32 *enforce_progressive)
{
	*enforce_progressive = osd_ext_hw.enforce_progressive;
}

void osd_ext_set_enforce_progressive_hw(u32 index, u32 enforce_progressive)
{
	osd_ext_hw.enforce_progressive = enforce_progressive;
	add_to_update_list(index, OSD_ENFORCE_PROGRESSIVE);
	osd_ext_wait_vsync_hw();
}

void osd_ext_enable_3d_mode_hw(int index, int enable)
{
	spin_lock_irqsave(&osd_ext_lock, lock_flags);
	osd_ext_hw.mode_3d[index].enable = enable;
	spin_unlock_irqrestore(&osd_ext_lock, lock_flags);
	if (enable) { //when disable 3d mode ,we should return to stardard state.
		osd_ext_hw.mode_3d[index].left_right = LEFT;
		osd_ext_hw.mode_3d[index].l_start = osd_ext_hw.pandata[index].x_start;
		osd_ext_hw.mode_3d[index].l_end = (osd_ext_hw.pandata[index].x_end +  osd_ext_hw.pandata[index].x_start) >> 1;
		osd_ext_hw.mode_3d[index].r_start = osd_ext_hw.mode_3d[index].l_end + 1;
		osd_ext_hw.mode_3d[index].r_end = osd_ext_hw.pandata[index].x_end;
		osd_ext_hw.mode_3d[index].origin_scale.h_enable = osd_ext_hw.scale[index].h_enable;
		osd_ext_hw.mode_3d[index].origin_scale.v_enable = osd_ext_hw.scale[index].v_enable;
		osd_ext_set_2x_scale_hw(index, 1, 0);
	} else {

		osd_ext_set_2x_scale_hw(index, osd_ext_hw.mode_3d[index].origin_scale.h_enable,
		                        osd_ext_hw.mode_3d[index].origin_scale.v_enable);
	}
}

void osd_ext_enable_hw(int enable , int index)
{
	osd_ext_hw.enable[index] = enable;
	add_to_update_list(index, OSD_ENABLE);

	osd_ext_wait_vsync_hw();
}

void osd_ext_set_2x_scale_hw(u32 index, u16 h_scale_enable, u16 v_scale_enable)
{
	amlog_level(LOG_LEVEL_HIGH, "osd[%d] set scale, h_scale: %s, v_scale: %s\r\n",
	            index, h_scale_enable ? "ENABLE" : "DISABLE", v_scale_enable ? "ENABLE" : "DISABLE");
	amlog_level(LOG_LEVEL_HIGH, "osd[%d].scaledata: %d %d %d %d\n",
	            index,
	            osd_ext_hw.scaledata[index].x_start,
	            osd_ext_hw.scaledata[index].x_end,
	            osd_ext_hw.scaledata[index].y_start,
	            osd_ext_hw.scaledata[index].y_end);
	amlog_level(LOG_LEVEL_HIGH, "osd[%d].pandata: %d %d %d %d\n",
	            index,
	            osd_ext_hw.pandata[index].x_start,
	            osd_ext_hw.pandata[index].x_end,
	            osd_ext_hw.pandata[index].y_start,
	            osd_ext_hw.pandata[index].y_end);

	osd_ext_hw.scale[index].h_enable = h_scale_enable;
	osd_ext_hw.scale[index].v_enable = v_scale_enable;
	add_to_update_list(index, DISP_SCALE_ENABLE);
	add_to_update_list(index, DISP_GEOMETRY);

	osd_ext_wait_vsync_hw();
}

void osd_ext_pan_display_hw(unsigned int xoffset, unsigned int yoffset, int index)
{
	long diff_x, diff_y;

#if defined(CONFIG_FB_OSD2_CURSOR)
	if (index >= 1)
#else
	if (index >= 2)
#endif
		return;

	if (xoffset != osd_ext_hw.pandata[index].x_start || yoffset != osd_ext_hw.pandata[index].y_start) {
		diff_x = xoffset - osd_ext_hw.pandata[index].x_start;
		diff_y = yoffset - osd_ext_hw.pandata[index].y_start;

		osd_ext_hw.pandata[index].x_start += diff_x;
		osd_ext_hw.pandata[index].x_end   += diff_x;
		osd_ext_hw.pandata[index].y_start += diff_y;
		osd_ext_hw.pandata[index].y_end   += diff_y;
		add_to_update_list(index, DISP_GEOMETRY);

		osd_ext_wait_vsync_hw();

		amlog_mask_level(LOG_MASK_HARDWARE, LOG_LEVEL_LOW, "offset[%d-%d]x[%d-%d]y[%d-%d]\n", \
		                 xoffset, yoffset, osd_ext_hw.pandata[index].x_start , osd_ext_hw.pandata[index].x_end , \
		                 osd_ext_hw.pandata[index].y_start , osd_ext_hw.pandata[index].y_end);
	}
}

static void osd1_update_disp_scale_enable(void)
{
	if (osd_ext_hw.scale[OSD1].h_enable) {
		SET_MPEG_REG_MASK(VIU2_OSD1_BLK0_CFG_W0, 3 << 12);
	} else {
		CLEAR_MPEG_REG_MASK(VIU2_OSD1_BLK0_CFG_W0, 3 << 12);
	}
	//if(osd_ext_hw.scan_mode != SCAN_MODE_INTERLACE)
	{
		if (osd_ext_hw.scale[OSD1].v_enable) {
			SET_MPEG_REG_MASK(VIU2_OSD1_BLK0_CFG_W0, 1 << 14);
		} else {
			CLEAR_MPEG_REG_MASK(VIU2_OSD1_BLK0_CFG_W0, 1 << 14);
		}
	}
}

static void osd2_update_disp_scale_enable(void)
{
	if (osd_ext_hw.scale[OSD2].h_enable) {
#if defined(CONFIG_FB_OSD2_CURSOR)
		CLEAR_MPEG_REG_MASK(VIU2_OSD2_BLK0_CFG_W0, 3 << 12);
#else
		SET_MPEG_REG_MASK(VIU2_OSD2_BLK0_CFG_W0, 3 << 12);
#endif
	} else {
		CLEAR_MPEG_REG_MASK(VIU2_OSD2_BLK0_CFG_W0, 3 << 12);
	}
	if (osd_ext_hw.scan_mode != SCAN_MODE_INTERLACE) {
		if (osd_ext_hw.scale[OSD2].v_enable) {
#if defined(CONFIG_FB_OSD2_CURSOR)
			CLEAR_MPEG_REG_MASK(VIU2_OSD2_BLK0_CFG_W0, 1 << 14);
#else
			SET_MPEG_REG_MASK(VIU2_OSD2_BLK0_CFG_W0, 1 << 14);
#endif
		} else {
			CLEAR_MPEG_REG_MASK(VIU2_OSD2_BLK0_CFG_W0, 1 << 14);
		}
	}

}

static void osd1_update_enforce_progressive(void)
{
	if (osd_ext_hw.enforce_progressive == 0) {
		SET_MPEG_REG_MASK(VIU2_OSD1_BLK0_CFG_W0, 1 << 1);
	} else if (osd_ext_hw.enforce_progressive != 0) {
		CLEAR_MPEG_REG_MASK(VIU2_OSD1_BLK0_CFG_W0, 1 << 1);
	}

	remove_from_update_list(OSD1, OSD_ENFORCE_PROGRESSIVE);
}

static void osd2_update_enforce_progressive(void)
{
	if (osd_ext_hw.enforce_progressive == 0) {
		SET_MPEG_REG_MASK(VIU2_OSD2_BLK0_CFG_W0, 1 << 1);
	} else if (osd_ext_hw.enforce_progressive != 0) {
		CLEAR_MPEG_REG_MASK(VIU2_OSD2_BLK0_CFG_W0, 1 << 1);
	}

	remove_from_update_list(OSD2, OSD_ENFORCE_PROGRESSIVE);
}

static void osd1_update_color_mode(void)
{
	u32 data32;

	if (osd_ext_hw.color_info[OSD1] != NULL) {
		data32 = (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) ? 2 : 0;
		data32 |= READ_MPEG_REG(VIU2_OSD1_BLK0_CFG_W0) & 0x7040;
		data32 |= osd_ext_hw.fb_gem[OSD1].canvas_idx << 16 ;
		data32 |= OSD_DATA_LITTLE_ENDIAN	 << 15 ;
		data32 |= osd_ext_hw.color_info[OSD1]->hw_colormat << 2;
		if (osd_ext_hw.color_info[OSD1]->color_index < COLOR_INDEX_YUV_422) {
			data32 |= 1                      << 7;    /* rgb enable */
		}
		data32 |=  osd_ext_hw.color_info[OSD1]->hw_blkmode << 8; /* osd_ext_blk_mode */
		WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W0, data32);
		WRITE_MPEG_REG(VIU2_OSD1_BLK1_CFG_W0, data32);
		WRITE_MPEG_REG(VIU2_OSD1_BLK2_CFG_W0, data32);
		WRITE_MPEG_REG(VIU2_OSD1_BLK3_CFG_W0, data32);
	}
	remove_from_update_list(OSD1, OSD_COLOR_MODE);

}

static void osd2_update_color_mode(void)
{
	u32 data32;

	if (osd_ext_hw.color_info[OSD2] != NULL) {
		data32 = (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) ? 2 : 0;
		data32 |= READ_MPEG_REG(VIU2_OSD2_BLK0_CFG_W0) & 0x7040;
		data32 |= osd_ext_hw.fb_gem[OSD2].canvas_idx << 16 ;
		data32 |= OSD_DATA_LITTLE_ENDIAN	 << 15 ;
		data32 |= osd_ext_hw.color_info[OSD2]->hw_colormat << 2;
		if (osd_ext_hw.color_info[OSD2]->color_index < COLOR_INDEX_YUV_422) {
			data32 |= 1                      << 7;    /* rgb enable */
		}
		data32 |=  osd_ext_hw.color_info[OSD2]->hw_blkmode << 8; /* osd_ext_blk_mode */
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W0, data32);
	}
	remove_from_update_list(OSD2, OSD_COLOR_MODE);
}

static void osd1_update_enable(void)
{
	u32 video_enable;

	video_enable = READ_MPEG_REG(VPP2_MISC)&VPP_VD1_PREBLEND;
	if (osd_ext_hw.enable[OSD1] == ENABLE) {
		if (osd_ext_hw.free_scale_enable[OSD1]) {
			CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD1_POSTBLEND);
			SET_MPEG_REG_MASK(VPP2_MISC, VPP_OSD1_PREBLEND);
			SET_MPEG_REG_MASK(VPP2_MISC, VPP_VD1_POSTBLEND);
			SET_MPEG_REG_MASK(VPP2_MISC, VPP_PREBLEND_EN);
		} else {
			CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD1_PREBLEND);
			if (!video_enable) {
				CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_VD1_POSTBLEND);
			}
			SET_MPEG_REG_MASK(VPP2_MISC, VPP_OSD1_POSTBLEND);
		}

	} else {
		if (osd_ext_hw.free_scale_enable[OSD1]) {
			CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD1_PREBLEND);
		} else {
			CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD1_POSTBLEND);
		}
	}
	remove_from_update_list(OSD1, OSD_ENABLE);
}

static void osd2_update_enable(void)
{
	u32 video_enable;

	video_enable = READ_MPEG_REG(VPP2_MISC)&VPP_VD1_PREBLEND;
	if (osd_ext_hw.enable[OSD2] == ENABLE) {
		if (osd_ext_hw.free_scale_enable[OSD2]) {
			CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD2_POSTBLEND);
			SET_MPEG_REG_MASK(VPP2_MISC, VPP_OSD2_PREBLEND);
			SET_MPEG_REG_MASK(VPP2_MISC, VPP_VD1_POSTBLEND);
		} else {
			CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD2_PREBLEND);
			if (!video_enable) {
				CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_VD1_POSTBLEND);
			}
			SET_MPEG_REG_MASK(VPP2_MISC, VPP_OSD2_POSTBLEND);
		}

	} else {
		if (osd_ext_hw.free_scale_enable[OSD2]) {
			CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD2_PREBLEND);
		} else {
			CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD2_POSTBLEND);
		}
	}
	remove_from_update_list(OSD2, OSD_ENABLE);
}

static void osd1_update_canvas_conf(void)
{
	if (osd_ext_hw.canvas_conf[OSD1]) {
		canvas_config(osd_ext_hw.fb_gem[OSD2].canvas_idx, osd_ext_hw.fb_gem[OSD1].addr,
		              osd_ext_hw.fb_gem[OSD1].width, osd_ext_hw.fb_gem[OSD1].height,
		              CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	} else {
		canvas_config(osd_ext_hw.fb_gem[OSD2].canvas_idx, osd_ext_hw.fb_gem[OSD2].addr,
		              osd_ext_hw.fb_gem[OSD2].width, osd_ext_hw.fb_gem[OSD2].height,
		              CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	}
	remove_from_update_list(OSD1, DISP_CANVAS_CONFIG);
}

static void osd2_update_canvas_conf(void)
{
	if (osd_ext_hw.canvas_conf[OSD2]) {
		canvas_config(osd_ext_hw.fb_gem[OSD1].canvas_idx, osd_ext_hw.fb_gem[OSD2].addr,
		              osd_ext_hw.fb_gem[OSD2].width, osd_ext_hw.fb_gem[OSD2].height,
		              CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	} else {
		canvas_config(osd_ext_hw.fb_gem[OSD1].canvas_idx, osd_ext_hw.fb_gem[OSD1].addr,
		              osd_ext_hw.fb_gem[OSD1].width, osd_ext_hw.fb_gem[OSD1].height,
		              CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	}
	remove_from_update_list(OSD2, DISP_CANVAS_CONFIG);
}

static void osd1_update_color_key(void)
{
	WRITE_MPEG_REG(VIU2_OSD1_TCOLOR_AG0, osd_ext_hw.color_key[OSD1]);
	remove_from_update_list(OSD1, OSD_COLOR_KEY);
}

static void osd2_update_color_key(void)
{
	WRITE_MPEG_REG(VIU2_OSD2_TCOLOR_AG0, osd_ext_hw.color_key[OSD2]);
	remove_from_update_list(OSD2, OSD_COLOR_KEY);
}

static void osd1_update_color_key_enable(void)
{
	u32 data32;

	data32 = READ_MPEG_REG(VIU2_OSD1_BLK0_CFG_W0);
	data32 &= ~(1 << 6);
	data32 |= (osd_ext_hw.color_key_enable[OSD1] << 6);
	WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W0, data32);
	WRITE_MPEG_REG(VIU2_OSD1_BLK1_CFG_W0, data32);
	WRITE_MPEG_REG(VIU2_OSD1_BLK2_CFG_W0, data32);
	WRITE_MPEG_REG(VIU2_OSD1_BLK3_CFG_W0, data32);
	remove_from_update_list(OSD1, OSD_COLOR_KEY_ENABLE);
}

static void osd2_update_color_key_enable(void)
{
	u32 data32;

	data32 = READ_MPEG_REG(VIU2_OSD2_BLK0_CFG_W0);
	data32 &= ~(1 << 6);
	data32 |= (osd_ext_hw.color_key_enable[OSD2] << 6);
	WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W0, data32);
	remove_from_update_list(OSD2, OSD_COLOR_KEY_ENABLE);
}

static void osd1_update_gbl_alpha(void)
{

	u32 data32 = READ_MPEG_REG(VIU2_OSD1_CTRL_STAT);
	data32 &= ~(0x1ff << 12);
	data32 |= osd_ext_hw.gbl_alpha[OSD1] << 12;
	WRITE_MPEG_REG(VIU2_OSD1_CTRL_STAT, data32);
	remove_from_update_list(OSD1, OSD_GBL_ALPHA);
}

static void osd2_update_gbl_alpha(void)
{

	u32 data32 = READ_MPEG_REG(VIU2_OSD2_CTRL_STAT);
	data32 &= ~(0x1ff << 12);
	data32 |= osd_ext_hw.gbl_alpha[OSD2] << 12;
	WRITE_MPEG_REG(VIU2_OSD2_CTRL_STAT, data32);
	remove_from_update_list(OSD2, OSD_GBL_ALPHA);
}

static void osd2_update_order(void)
{
	switch (osd_ext_hw.osd_ext_order) {
	case  OSD_ORDER_01:
		CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;
	case  OSD_ORDER_10:
		SET_MPEG_REG_MASK(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;
	default:
		break;
	}
	remove_from_update_list(OSD2, OSD_CHANGE_ORDER);
}

static void osd1_update_order(void)
{
	switch (osd_ext_hw.osd_ext_order) {
	case  OSD_ORDER_01:
		CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;
	case  OSD_ORDER_10:
		SET_MPEG_REG_MASK(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
		break;
	default:
		break;
	}
	remove_from_update_list(OSD1, OSD_CHANGE_ORDER);
}

static void osd_ext_block_update_disp_geometry(u32 index)
{
	u32 data32;
	u32 data_w1, data_w2, data_w3, data_w4;
	u32 coef[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
	u32 xoff, yoff;
	u32 i;

	switch (osd_ext_hw.block_mode[index] & HW_OSD_BLOCK_LAYOUT_MASK) {
	case HW_OSD_BLOCK_LAYOUT_HORIZONTAL:
		yoff = ((osd_ext_hw.pandata[index].y_end & 0x1fff) - (osd_ext_hw.pandata[index].y_start & 0x1fff) + 1) >> 2;
		data_w1 = (osd_ext_hw.pandata[index].x_start & 0x1fff) | (osd_ext_hw.pandata[index].x_end & 0x1fff) << 16 ;
		data_w3 = (osd_ext_hw.dispdata[index].x_start & 0xfff) | (osd_ext_hw.dispdata[index].x_end & 0xfff) << 16;
		for (i = 0; i < 4; i++) {
			if (i == 3) {
				data_w2 = ((osd_ext_hw.pandata[index].y_start + yoff * i) & 0x1fff)
				          | (osd_ext_hw.pandata[index].y_end & 0x1fff) << 16;
				data_w4 = ((osd_ext_hw.dispdata[index].y_start + yoff * i) & 0xfff)
				          | (osd_ext_hw.dispdata[index].y_end & 0xfff) << 16;
			} else {
				data_w2 = ((osd_ext_hw.pandata[index].y_start + yoff * i) & 0x1fff)
				          | ((osd_ext_hw.pandata[index].y_start + yoff * (i + 1) - 1) & 0x1fff) << 16;
				data_w4 = ((osd_ext_hw.dispdata[index].y_start + yoff * i) & 0xfff)
				          | ((osd_ext_hw.dispdata[index].y_start + yoff * (i + 1) - 1) & 0xfff) << 16;
			}
			if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1) | ((((((data32 >> 16) & 0xfff) + 1) >> 1) - 1) << 16);
			}

			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1 + (i << 2), data_w1);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W2 + (i << 2), data_w2);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W3 + (i << 2), data_w3);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W4 + i, data_w4);

			osd_ext_hw.block_windows[index][i << 1] = data_w1;
			osd_ext_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}
		break;
	case HW_OSD_BLOCK_LAYOUT_VERTICAL:
		xoff = ((osd_ext_hw.pandata[index].x_end & 0x1fff) - (osd_ext_hw.pandata[index].x_start & 0x1fff) + 1) >> 2;
		data_w2 = (osd_ext_hw.pandata[index].y_start & 0x1fff) | (osd_ext_hw.pandata[index].y_end & 0x1fff) << 16 ;
		data_w4 = (osd_ext_hw.dispdata[index].y_start & 0xfff) | (osd_ext_hw.dispdata[index].y_end & 0xfff) << 16;
		if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
			data32 = data_w4;
			data_w4 = ((data32 & 0xfff) >> 1) | ((((((data32 >> 16) & 0xfff) + 1) >> 1) - 1) << 16);
		}
		for (i = 0; i < 4; i++) {
			data_w1 = ((osd_ext_hw.pandata[index].x_start  + xoff * i) & 0x1fff)
			          | ((osd_ext_hw.pandata[index].x_start + xoff * (i + 1) - 1) & 0x1fff) << 16;
			data_w3 = ((osd_ext_hw.dispdata[index].x_start + xoff * i) & 0xfff)
			          | ((osd_ext_hw.dispdata[index].x_start + xoff * (i + 1) - 1) & 0xfff) << 16;

			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1 + (i << 2), data_w1);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W2 + (i << 2), data_w2);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W3 + (i << 2), data_w3);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W4 + i, data_w4);

			osd_ext_hw.block_windows[index][i << 1] = data_w1;
			osd_ext_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}
		break;
	case HW_OSD_BLOCK_LAYOUT_GRID:
		xoff = ((osd_ext_hw.pandata[index].x_end & 0x1fff) - (osd_ext_hw.pandata[index].x_start & 0x1fff) + 1) >> 1;
		yoff = ((osd_ext_hw.pandata[index].y_end & 0x1fff) - (osd_ext_hw.pandata[index].y_start & 0x1fff) + 1) >> 1;
		for (i = 0; i < 4; i++) {
			data_w1 = ((osd_ext_hw.pandata[index].x_start + xoff * coef[i][0]) & 0x1fff)
			          | ((osd_ext_hw.pandata[index].x_start + xoff * (coef[i][0] + 1) - 1) & 0x1fff) << 16;
			data_w2 = ((osd_ext_hw.pandata[index].y_start + yoff * coef[i][1]) & 0x1fff)
			          | ((osd_ext_hw.pandata[index].y_start + yoff * (coef[i][1] + 1) - 1) & 0x1fff) << 16;
			data_w3 = ((osd_ext_hw.dispdata[index].x_start + xoff * coef[i][0]) & 0xfff)
			          | ((osd_ext_hw.dispdata[index].x_start + xoff * (coef[i][0] + 1) - 1) & 0xfff) << 16;
			data_w4 = ((osd_ext_hw.dispdata[index].y_start + yoff * coef[i][1]) & 0xfff)
			          | ((osd_ext_hw.dispdata[index].y_start + yoff * (coef[i][1] + 1) - 1) & 0xfff) << 16;

			if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1) | ((((((data32 >> 16) & 0xfff) + 1) >> 1) - 1) << 16);
			}
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1 + (i << 2), data_w1);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W2 + (i << 2), data_w2);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W3 + (i << 2), data_w3);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W4 + i, data_w4);

			osd_ext_hw.block_windows[index][i << 1] = data_w1;
			osd_ext_hw.block_windows[index][(i << 1) + 1] = data_w2;
		}
		break;
	case HW_OSD_BLOCK_LAYOUT_CUSTOMER:
		for (i = 0; i < 4; i++) {
			if (((osd_ext_hw.block_windows[index][i << 1] >> 16) & 0x1fff) > osd_ext_hw.pandata[index].x_end) {
				osd_ext_hw.block_windows[index][i << 1] = (osd_ext_hw.block_windows[index][i << 1] & 0x1fff)
				                                          | ((osd_ext_hw.pandata[index].x_end & 0x1fff) << 16);
			}
			data_w1 = osd_ext_hw.block_windows[index][i << 1] & 0x1fff1fff;
			data_w2 = ((osd_ext_hw.pandata[index].y_start & 0x1fff) + (osd_ext_hw.block_windows[index][(i << 1) + 1] & 0x1fff))
			          | (((osd_ext_hw.pandata[index].y_start & 0x1fff) << 16) + (osd_ext_hw.block_windows[index][(i << 1) + 1] & 0x1fff0000));
			data_w3 = (osd_ext_hw.dispdata[index].x_start + (data_w1 & 0xfff))
			          | (((osd_ext_hw.dispdata[index].x_start & 0xfff) << 16) + (data_w1 & 0xfff0000));
			data_w4 = (osd_ext_hw.dispdata[index].y_start + (osd_ext_hw.block_windows[index][(i << 1) + 1] & 0xfff))
			          | (((osd_ext_hw.dispdata[index].y_start & 0xfff) << 16) + (osd_ext_hw.block_windows[index][(i << 1) + 1] & 0xfff0000));
			if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
				data32 = data_w4;
				data_w4 = ((data32 & 0xfff) >> 1) | ((((((data32 >> 16) & 0xfff) + 1) >> 1) - 1) << 16);
			}
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1 + (i << 2), data_w1);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W2 + (i << 2), data_w2);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W3 + (i << 2), data_w3);
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W4 + i, data_w4);
		}
		break;

	default:
		amlog_level(LOG_LEVEL_HIGH, "ERROR block_mode: 0x%x\n", osd_ext_hw.block_mode[index]);
		break;
	}
}

static void osd1_update_disp_geometry(void)
{
	u32 data32;

	/* enable osd multi block */
	if (osd_ext_hw.block_mode[OSD1]) {
		osd_ext_block_update_disp_geometry(OSD1);
		data32 = READ_MPEG_REG(VIU2_OSD1_CTRL_STAT);
		data32 &= 0xfffffff0;
		data32 |= (osd_ext_hw.block_mode[OSD1] & HW_OSD_BLOCK_ENABLE_MASK);
		WRITE_MPEG_REG(VIU2_OSD1_CTRL_STAT, data32);
	} else {
		data32 = (osd_ext_hw.dispdata[OSD1].x_start & 0xfff) | (osd_ext_hw.dispdata[OSD1].x_end & 0xfff) << 16 ;
		WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W3 , data32);
		if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
			data32 = ((osd_ext_hw.dispdata[OSD1].y_start >> 1) & 0xfff) | ((((osd_ext_hw.dispdata[OSD1].y_end + 1) >> 1) - 1) & 0xfff) << 16 ;
		} else {
			data32 = (osd_ext_hw.dispdata[OSD1].y_start & 0xfff) | (osd_ext_hw.dispdata[OSD1].y_end & 0xfff) << 16 ;
		}
		WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W4, data32);

		/* enable osd 2x scale */
		if (osd_ext_hw.scale[OSD1].h_enable || osd_ext_hw.scale[OSD1].v_enable) {
			data32 = (osd_ext_hw.scaledata[OSD1].x_start & 0x1fff) | (osd_ext_hw.scaledata[OSD1].x_end & 0x1fff) << 16;
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1, data32);
			data32 = ((osd_ext_hw.scaledata[OSD1].y_start + osd_ext_hw.pandata[OSD1].y_start) & 0x1fff)
			         | ((osd_ext_hw.scaledata[OSD1].y_end  + osd_ext_hw.pandata[OSD1].y_start) & 0x1fff) << 16 ;
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W2, data32);

			/* adjust display x-axis */
			if (osd_ext_hw.scale[OSD1].h_enable) {
				data32 = (osd_ext_hw.dispdata[OSD1].x_start & 0xfff)
				         | ((osd_ext_hw.dispdata[OSD1].x_start + (osd_ext_hw.scaledata[OSD1].x_end - osd_ext_hw.scaledata[OSD1].x_start) * 2 + 1) & 0xfff) << 16 ;
				WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W3 , data32);
			}

			/* adjust display y-axis */
			if (osd_ext_hw.scale[OSD1].v_enable) {
				if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
					data32 = ((osd_ext_hw.dispdata[OSD1].y_start >> 1) & 0xfff)
					         | (((((osd_ext_hw.dispdata[OSD1].y_start + (osd_ext_hw.scaledata[OSD1].y_end - osd_ext_hw.scaledata[OSD1].y_start) * 2) + 1) >> 1) - 1) & 0xfff) << 16 ;
				} else {
					data32 = (osd_ext_hw.dispdata[OSD1].y_start & 0xfff)
					         | (((osd_ext_hw.dispdata[OSD1].y_start  + (osd_ext_hw.scaledata[OSD1].y_end - osd_ext_hw.scaledata[OSD1].y_start) * 2)) & 0xfff) << 16 ;
				}
				WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W4, data32);
			}
		} else if (osd_ext_hw.free_scale_enable[OSD1]
		           && (osd_ext_hw.free_scale_data[OSD1].x_end > 0)
		           && (osd_ext_hw.free_scale_data[OSD1].y_end > 0)) {
			/* enable osd free scale */
			data32 = (osd_ext_hw.free_scale_data[OSD1].x_start & 0x1fff) | (osd_ext_hw.free_scale_data[OSD1].x_end & 0x1fff) << 16;
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1, data32);
			data32 = ((osd_ext_hw.free_scale_data[OSD1].y_start + osd_ext_hw.pandata[OSD1].y_start) & 0x1fff)
			         | ((osd_ext_hw.free_scale_data[OSD1].y_end  + osd_ext_hw.pandata[OSD1].y_start) & 0x1fff) << 16 ;
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W2, data32);
		} else {
			/* normal mode */
			data32 = (osd_ext_hw.pandata[OSD1].x_start & 0x1fff) | (osd_ext_hw.pandata[OSD1].x_end & 0x1fff) << 16;
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1, data32);
			data32 = (osd_ext_hw.pandata[OSD1].y_start & 0x1fff) | (osd_ext_hw.pandata[OSD1].y_end & 0x1fff) << 16 ;
			WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W2, data32);
		}
		data32 = READ_MPEG_REG(VIU2_OSD1_CTRL_STAT);
		data32 &= 0xfffffff0;
		data32 |= HW_OSD_BLOCK_ENABLE_0;
		WRITE_MPEG_REG(VIU2_OSD1_CTRL_STAT, data32);
	}

	remove_from_update_list(OSD1, DISP_GEOMETRY);
}

static   void  osd2_update_disp_geometry(void)
{
	u32 data32;
	data32 = (osd_ext_hw.dispdata[OSD2].x_start & 0xfff) | (osd_ext_hw.dispdata[OSD2].x_end & 0xfff) << 16 ;
	WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W3 , data32);
	if (osd_ext_hw.scan_mode == SCAN_MODE_INTERLACE) {
		data32 = ((osd_ext_hw.dispdata[OSD2].y_start >> 1) & 0xfff) | ((((osd_ext_hw.dispdata[OSD2].y_end + 1) >> 1) - 1) & 0xfff) << 16 ;
	} else {
		data32 = (osd_ext_hw.dispdata[OSD2].y_start & 0xfff) | (osd_ext_hw.dispdata[OSD2].y_end & 0xfff) << 16 ;
	}
	WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W4, data32);

	if (osd_ext_hw.scale[OSD2].h_enable || osd_ext_hw.scale[OSD2].v_enable) {
#if defined(CONFIG_FB_OSD2_CURSOR)
		data32 = (osd_ext_hw.pandata[OSD2].x_start & 0x1fff) | (osd_ext_hw.pandata[OSD2].x_end & 0x1fff) << 16;
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W1, data32);
		data32 = (osd_ext_hw.pandata[OSD2].y_start & 0x1fff) | (osd_ext_hw.pandata[OSD2].y_end & 0x1fff) << 16 ;
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W2, data32);
#else
		data32 = (osd_ext_hw.scaledata[OSD2].x_start & 0x1fff) | (osd_ext_hw.scaledata[OSD2].x_end & 0x1fff) << 16;
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W1, data32);
		data32 = ((osd_ext_hw.scaledata[OSD2].y_start + osd_ext_hw.pandata[OSD2].y_start) & 0x1fff)
		         | ((osd_ext_hw.scaledata[OSD2].y_end  + osd_ext_hw.pandata[OSD2].y_start) & 0x1fff) << 16 ;
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W2, data32);
#endif
	} else {
		data32 = (osd_ext_hw.pandata[OSD2].x_start & 0x1fff) | (osd_ext_hw.pandata[OSD2].x_end & 0x1fff) << 16;
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W1, data32);
		data32 = (osd_ext_hw.pandata[OSD2].y_start & 0x1fff) | (osd_ext_hw.pandata[OSD2].y_end & 0x1fff) << 16 ;
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W2, data32);
	}
	remove_from_update_list(OSD2, DISP_GEOMETRY);
}

static void osd1_update_disp_3d_mode(void)
{
	/*step 1 . set pan data */
	u32  data32;

	if (osd_ext_hw.mode_3d[OSD1].left_right == LEFT) {
		data32 = (osd_ext_hw.mode_3d[OSD1].l_start & 0x1fff) | (osd_ext_hw.mode_3d[OSD1].l_end & 0x1fff) << 16;
		WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1, data32);
	} else {
		data32 = (osd_ext_hw.mode_3d[OSD1].r_start & 0x1fff) | (osd_ext_hw.mode_3d[OSD1].r_end & 0x1fff) << 16;
		WRITE_MPEG_REG(VIU2_OSD1_BLK0_CFG_W1, data32);
	}
	osd_ext_hw.mode_3d[OSD1].left_right ^= 1;
}

static void osd2_update_disp_3d_mode(void)
{
	u32  data32;

	if (osd_ext_hw.mode_3d[OSD2].left_right == LEFT) {
		data32 = (osd_ext_hw.mode_3d[OSD2].l_start & 0x1fff) | (osd_ext_hw.mode_3d[OSD2].l_end & 0x1fff) << 16;
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W1, data32);
	} else {
		data32 = (osd_ext_hw.mode_3d[OSD2].r_start & 0x1fff) | (osd_ext_hw.mode_3d[OSD2].r_end & 0x1fff) << 16;
		WRITE_MPEG_REG(VIU2_OSD2_BLK0_CFG_W1, data32);
	}
	osd_ext_hw.mode_3d[OSD2].left_right ^= 1;
}

void osd_ext_init_hw(u32  logo_loaded)
{
	u32 group, idx, data32;

	for (group = 0; group < HW_OSD_COUNT; group++)
		for (idx = 0; idx < HW_REG_INDEX_MAX; idx++) {
			osd_ext_hw.reg[group][idx].update_func = hw_func_array[group][idx];
		}
	osd_ext_hw.updated[OSD1] = 0;
	osd_ext_hw.updated[OSD2] = 0;
	//here we will init default value ,these value only set once .
	if (!logo_loaded) {
		data32  = 1;
		data32 |= 4   << 5;  // hold_fifo_lines
		data32 |= 3   << 10; // burst_len_sel: 3=64
		data32 |= 32  << 12; // fifo_depth_val: 32*8=256

		WRITE_MPEG_REG(VIU2_OSD1_FIFO_CTRL_STAT, data32);
		WRITE_MPEG_REG(VIU2_OSD2_FIFO_CTRL_STAT, data32);

		SET_MPEG_REG_MASK(VPP2_MISC, VPP_POSTBLEND_EN);
		CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_PREBLEND_EN);
		CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OSD1_POSTBLEND | VPP_OSD2_POSTBLEND);
		data32  = 0x1          << 0; // osd_ext_blk_enable
		data32 |= OSD_GLOBAL_ALPHA_DEF << 12;
		data32 |= (1 << 21)	;
		WRITE_MPEG_REG(VIU2_OSD1_CTRL_STAT , data32);
		WRITE_MPEG_REG(VIU2_OSD2_CTRL_STAT , data32);
	}

#if defined(CONFIG_FB_OSD2_CURSOR)
	SET_MPEG_REG_MASK(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
	osd_ext_hw.osd_ext_order = OSD_ORDER_10;
#else
	CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_POST_FG_OSD2 | VPP_PRE_FG_OSD2);
	osd_ext_hw.osd_ext_order = OSD_ORDER_01;
#endif

	osd_ext_hw.enable[OSD2] = osd_ext_hw.enable[OSD1] = DISABLE;
	osd_ext_hw.fb_gem[OSD1].canvas_idx = OSD3_CANVAS_INDEX;
	osd_ext_hw.fb_gem[OSD2].canvas_idx = OSD4_CANVAS_INDEX;
	printk("***** osd_ext_init_hw(0) canvas_idx=%d\n", osd_ext_hw.fb_gem[OSD1].canvas_idx);
	printk("***** osd_ext_init_hw(1) canvas_idx=%d\n", osd_ext_hw.fb_gem[OSD2].canvas_idx);
	osd_ext_hw.gbl_alpha[OSD1] = OSD_GLOBAL_ALPHA_DEF;
	osd_ext_hw.gbl_alpha[OSD2] = OSD_GLOBAL_ALPHA_DEF;
	osd_ext_hw.color_info[OSD1] = NULL;
	osd_ext_hw.color_info[OSD2] = NULL;
	vf.width = vf.height = 0;
	osd_ext_hw.color_key[OSD1] = osd_ext_hw.color_key[OSD2] = 0xffffffff;
	osd_ext_hw.free_scale_enable[OSD1] = osd_ext_hw.free_scale_enable[OSD2] = 0;
	osd_ext_hw.scale[OSD1].h_enable = osd_ext_hw.scale[OSD1].v_enable = 0;
	osd_ext_hw.scale[OSD2].h_enable = osd_ext_hw.scale[OSD2].v_enable = 0;
	osd_ext_hw.mode_3d[OSD2].enable = osd_ext_hw.mode_3d[OSD1].enable = 0;
	osd_ext_hw.block_mode[OSD1] = osd_ext_hw.block_mode[OSD2] = 0;


#ifdef FIQ_VSYNC
	osd_ext_hw.fiq_handle_item.handle = vsync_isr;
	osd_ext_hw.fiq_handle_item.key = (u32)vsync_isr;
	osd_ext_hw.fiq_handle_item.name = "osd_ext_vsync";
	if (register_fiq_bridge_handle(&osd_ext_hw.fiq_handle_item))
#else
	if (request_irq(INT_VIU2_VSYNC, &vsync_isr,
	                IRQF_SHARED , "am_osd_ext_vsync", osd_ext_setup))
#endif
	{
		amlog_level(LOG_LEVEL_HIGH, "can't request irq for vsync\r\n");
	}

#ifdef FIQ_VSYNC
	request_fiq(INT_VIU2_VSYNC, &osd_ext_fiq_isr);
#endif

	return ;
}

#if defined(CONFIG_FB_OSD2_CURSOR)
void osd_ext_cursor_hw(s16 x, s16 y, s16 xstart, s16 ystart, u32 osd_ext_w, u32 osd_ext_h, int index)
{
	dispdata_t disp_tmp;

	if (index != 1) {
		return;
	}

	memcpy(&disp_tmp, &osd_ext_hw.dispdata[OSD1], sizeof(dispdata_t));
	if (osd_ext_hw.scale[OSD2].h_enable && (osd_ext_hw.scaledata[OSD2].x_start > 0)
	    && (osd_ext_hw.scaledata[OSD2].x_end > 0)) {
		x = x * osd_ext_hw.scaledata[OSD2].x_end / osd_ext_hw.scaledata[OSD2].x_start;
		if (osd_ext_hw.scaledata[OSD2].x_end > osd_ext_hw.scaledata[OSD2].x_start) {
			disp_tmp.x_start = osd_ext_hw.dispdata[OSD1].x_start * osd_ext_hw.scaledata[OSD2].x_end / osd_ext_hw.scaledata[OSD2].x_start;
			disp_tmp.x_end = osd_ext_hw.dispdata[OSD1].x_end * osd_ext_hw.scaledata[OSD2].x_end / osd_ext_hw.scaledata[OSD2].x_start;
		}
	}

	if (osd_ext_hw.scale[OSD2].v_enable && (osd_ext_hw.scaledata[OSD2].y_start > 0)
	    && (osd_ext_hw.scaledata[OSD2].y_end > 0)) {
		y = y * osd_ext_hw.scaledata[OSD2].y_end / osd_ext_hw.scaledata[OSD2].y_start;
		if (osd_ext_hw.scaledata[OSD2].y_end > osd_ext_hw.scaledata[OSD2].y_start) {
			disp_tmp.y_start = osd_ext_hw.dispdata[OSD1].y_start * osd_ext_hw.scaledata[OSD2].y_end / osd_ext_hw.scaledata[OSD2].y_start;
			disp_tmp.y_end = osd_ext_hw.dispdata[OSD1].y_end * osd_ext_hw.scaledata[OSD2].y_end / osd_ext_hw.scaledata[OSD2].y_start;
		}
	}

	x += xstart;
	y += ystart;
	/**
	 * Use pandata to show a partial cursor when it is at the edge because the
	 * registers can't have negative values and because we need to manually
	 * clip the cursor when it is past the edge.  The edge is hardcoded
	 * to the OSD0 area.
	 */
	osd_ext_hw.dispdata[OSD2].x_start = x;
	osd_ext_hw.dispdata[OSD2].y_start = y;
	if (x <  disp_tmp.x_start) {
		// if negative position, set osd to 0,y and pan.
		if ((disp_tmp.x_start - x) < osd_ext_w) {
			osd_ext_hw.pandata[OSD2].x_start = disp_tmp.x_start - x;
			osd_ext_hw.pandata[OSD2].x_end = osd_ext_w - 1;
		}
		osd_ext_hw.dispdata[OSD2].x_start = 0;
	} else {
		osd_ext_hw.pandata[OSD2].x_start = 0;
		if (x + osd_ext_w > disp_tmp.x_end) {
			// if past positive edge, set osd to inside of the edge and pan.
			if (x < osd_ext_hw.dispdata[OSD1].x_end) {
				osd_ext_hw.pandata[OSD2].x_end = disp_tmp.x_end - x;
			}
		} else {
			osd_ext_hw.pandata[OSD2].x_end = osd_ext_w - 1;
		}
	}
	if (y < disp_tmp.y_start) {
		if ((disp_tmp.y_start - y) < osd_ext_h) {
			osd_ext_hw.pandata[OSD2].y_start = disp_tmp.y_start - y;
			osd_ext_hw.pandata[OSD2].y_end = osd_ext_h - 1;
		}
		osd_ext_hw.dispdata[OSD2].y_start = 0;
	} else {
		osd_ext_hw.pandata[OSD2].y_start = 0;
		if (y + osd_ext_h > disp_tmp.y_end) {
			if (y < disp_tmp.y_end) {
				osd_ext_hw.pandata[OSD2].y_end = disp_tmp.y_end - y;
			}
		} else {
			osd_ext_hw.pandata[OSD2].y_end = osd_ext_h - 1;
		}
	}
	osd_ext_hw.dispdata[OSD2].x_end = osd_ext_hw.dispdata[OSD2].x_start + osd_ext_hw.pandata[OSD2].x_end - osd_ext_hw.pandata[OSD2].x_start;
	osd_ext_hw.dispdata[OSD2].y_end = osd_ext_hw.dispdata[OSD2].y_start + osd_ext_hw.pandata[OSD2].y_end - osd_ext_hw.pandata[OSD2].y_start;
	add_to_update_list(OSD2, DISP_GEOMETRY);
}
#endif //CONFIG_FB_OSD2_CURSOR

void osd_ext_suspend_hw(void)
{
	osd_ext_hw.reg_status_save = READ_MPEG_REG(VPP2_MISC) & OSD_RELATIVE_BITS;

	CLEAR_MPEG_REG_MASK(VPP2_MISC, OSD_RELATIVE_BITS);

	printk("osd_ext_suspended\n");

	return ;

}

void osd_ext_resume_hw(void)
{
	SET_MPEG_REG_MASK(VPP2_MISC, osd_ext_hw.reg_status_save);
	// fix bug for  suspend when movie playing, and then resume
	add_to_update_list(OSD1, OSD_ENABLE);
	printk("osd_ext_resumed\n");

	return ;
}

void osd_ext_clone_pan(u32 index)
{
	s32 offset = 0;
	if (osd_ext_hw.clone[index]) {
		if (osd_ext_hw.pandata[index].y_start > osd_hw.pandata[index].y_start) {
			offset -= osd_ext_hw.pandata[index].y_end - osd_ext_hw.pandata[index].y_start + 1;
		} else if (osd_ext_hw.pandata[index].y_start < osd_hw.pandata[index].y_start) {
			offset += osd_ext_hw.pandata[index].y_end - osd_ext_hw.pandata[index].y_start + 1;
		}
		osd_ext_hw.pandata[index].y_start += offset;
		osd_ext_hw.pandata[index].y_end += offset;
		add_to_update_list(index, DISP_GEOMETRY);
		osd_ext_wait_vsync_hw();
	}
}

void osd_ext_get_clone_hw(u32 index, u32 *clone)
{
	amlog_level(LOG_LEVEL_HIGH, "get osd_ext[%d]->clone: %d\n", index, osd_ext_hw.clone[index]);
	*clone = osd_ext_hw.clone[index];
}

void osd_ext_set_clone_hw(u32 index, u32 clone)
{
	static const color_bit_define_t *color_info[HW_OSD_COUNT] = {};
	static pandata_t pandata[HW_OSD_COUNT] = {};

	amlog_level(LOG_LEVEL_HIGH, "set osd_ext[%d]->clone: %d->%d\n", index, clone);
	osd_ext_hw.clone[index] = clone;
	if(osd_ext_hw.clone[index]) {
		color_info[index] = osd_ext_hw.color_info[index];
		osd_ext_hw.color_info[index] = osd_hw.color_info[index];
		memcpy(&pandata, &osd_ext_hw.pandata[index], sizeof(pandata_t));
		canvas_config(osd_ext_hw.fb_gem[index].canvas_idx, osd_hw.fb_gem[index].addr,
			osd_hw.fb_gem[index].width, osd_hw.fb_gem[index].height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	} else {
		canvas_config(osd_ext_hw.fb_gem[index].canvas_idx, osd_ext_hw.fb_gem[index].addr,
			osd_ext_hw.fb_gem[index].width, osd_ext_hw.fb_gem[index].height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		osd_ext_hw.color_info[index] = color_info[index];
		memcpy(&osd_ext_hw.pandata[index], &pandata, sizeof(pandata_t));
	}
	add_to_update_list(index, OSD_COLOR_MODE);
}
