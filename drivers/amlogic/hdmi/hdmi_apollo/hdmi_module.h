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
    
#ifndef __HDMI_MODULE_H
#define __HDMI_MODULE_H
    
#define DRIVER_NAME	"apollo-hdmi"
#define MODULE_NAME	"apollo-hdmi"
#define DEVICE_NAME	"hdmi"
    
#include "hdmi_global.h"
#include "linux/fb.h"
struct hdmi_priv  {
	struct class *class;
	struct device *dev;
	struct i2c_adapter *i2c;
	struct hdmi_device *hdmi_dev;
	struct task_struct *task;
	int video_mode;
	 Hdmi_info_para_t hinfo;
	 int irq_num;
	 unsigned long base_addr;
 };
struct hdmi_device  {
	char name[32];
	 int (*init_fun) (struct hdmi_priv *);
	int (*video_change) (struct hdmi_priv *);
	int (*audio_change) (struct hdmi_priv *);
	int (*exit) (struct hdmi_priv *);
};

#endif				//__HDMI_MODULE_H
