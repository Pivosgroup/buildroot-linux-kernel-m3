/*
 * Amlogic Apollo
 * frame buffer driver
 *	-------hdmi output
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
 * Author:  zhou zhi<zhi.zhou@amlogic.com>
 * Firset add at:2009-7-28
 */

#include <linux/fb.h>
#include <linux/vout/vinfo.h>
#include <linux/vout/vout_notify.h>

#include "hdmi_module.h"
#include "hdmi_debug.h"
#include "asm/arch/am_regs.h"

static int mode_changed = 1;

static int hdmi_video_event(struct notifier_block *self,
			    unsigned long action, void *data)
{
//      struct fb_event *event = data;
//      struct fb_info *info = event->info;
//      struct fb_videomode *mode;
//TODO
	int ret = 0;
	HDMI_DEBUG("Video event=%lx\n", action);
	switch (action) {
	case VOUT_EVENT_MODE_CHANGE:
		mode_changed = 1;
		break;
	}
//done:
	return ret;
}

/* no use
#if 0
static int  hdmi_video_mode_probe(void)
{
    int vmode;

    if (READ_MPEG_REG(ENCI_VIDEO_EN) & 1) {
        if (READ_MPEG_REG(ENCI_VIDEO_MODE) & 1)
            vmode = DISPCTL_MODE_576I;
        else
            vmode = DISPCTL_MODE_480I;
    }
    else {
        if (READ_MPEG_REG(ENCP_VIDEO_MODE) & (1<<12)) {
            vmode = DISPCTL_MODE_1080I;
        }
        else {
            s32 l = READ_MPEG_REG(ENCP_VIDEO_VAVON_ELINE) - READ_MPEG_REG(ENCP_VIDEO_VAVON_BLINE);

            if (l > 720)
                vmode = DISPCTL_MODE_1080P;
            else if (l > 576)
                vmode = DISPCTL_MODE_720P;
            else if (l > 480)
                vmode = DISPCTL_MODE_576P;
            else
                vmode = DISPCTL_MODE_480P;
        }
    }
    
    return vmode;
}
#endif
*/

int hdmi_get_tv_mode(Hdmi_info_para_t * info)
{
	struct hdmi_priv *priv = info->priv;
	int dispmode;
	const vinfo_t *vinfo = get_current_vinfo();
	
	if (vinfo == NULL)
		return DISPCTL_MODE_720P;
	
	switch (vinfo->mode) {
	case VMODE_480I:
    case VMODE_480CVBS:
		dispmode = DISPCTL_MODE_480I;
		break;
	case VMODE_480P:
		dispmode = DISPCTL_MODE_480P;
		break;
	case VMODE_576I:
    case VMODE_576CVBS:
		dispmode = DISPCTL_MODE_576I;
		break;
	case VMODE_576P:
		dispmode = DISPCTL_MODE_576P;
		break;
	case VMODE_720P:
		dispmode = DISPCTL_MODE_720P;
		break;
	case VMODE_1080I:
		dispmode = DISPCTL_MODE_1080I;
		break;
	case VMODE_1080P:
		dispmode = DISPCTL_MODE_1080P;
		break;
	default:
		HDMI_INFO("Detected the TV mode changed to unknow mode %d\n",
			  vinfo->mode);
		dispmode = DISPCTL_MODE_720P;
	}
	if (mode_changed || vinfo->mode != priv->video_mode) {
		HDMI_INFO("Detected the TV mode changed to %s\n",
			  vinfo->name);
		mode_changed = 0;
		priv->video_mode = vinfo->mode;
	}
	return dispmode;
}

static struct notifier_block hdmi_notifier_nb = {
	.notifier_call = hdmi_video_event,
};

int hdmi_video_init(struct hdmi_priv *priv)
{
	int res;

	res = vout_register_client(&hdmi_notifier_nb);
	if (res != 0) {

		HDMI_ERR("Can't register video notifier_block %s(%d)\n",
			 __FUNCTION__, res);
		return res;
	}
	HDMI_DEBUG("Register framebuffer notifier_block ok[%s]\n",
		   __FUNCTION__);

	priv->video_mode = DISPCTL_MODE_720P;

	return 0;
}

int hdmi_video_exit(struct hdmi_priv *priv)
{
	vout_unregister_client(&hdmi_notifier_nb);
	return 0;
}
