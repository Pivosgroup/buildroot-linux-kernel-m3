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
    
#ifndef _HDMI_VIDEO_H
#define _HDMI_VIDEO_H
#include "hdmi_global.h"
int hdmi_video_init(struct hdmi_priv *priv);
int hdmi_video_exit(struct hdmi_priv *priv);
int hdmi_get_tv_mode(Hdmi_info_para_t * info);

#endif				//_HDMI_VIDEO_H 
    
