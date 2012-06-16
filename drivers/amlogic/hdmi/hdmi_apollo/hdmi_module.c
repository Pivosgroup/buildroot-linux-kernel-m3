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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/device.h>

#include <linux/major.h>

#include <asm/uaccess.h>

#include "hdmi_module.h"
#include "hdmi_debug.h"
#include "hdmi_i2c.h"
#include "hdmi_video.h"

#include <asm/arch/am_regs.h>
#include <asm/arch/pinmux.h>

MODULE_DESCRIPTION("AMLOGIC APOLLO HDMI driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhou Zhi <zhi.zhou@amlogic.com>");
MODULE_VERSION("1.0.0");

#ifdef DEBUG
int hdmi_debug_level = DEBUG_ALL;
#endif
static struct hdmi_priv *hdmi_priv_data = NULL;

static int hdmi_open(struct inode *node, struct file *file);
static int hdmi_ioctl(struct inode *node, struct file *file, unsigned int cmd,
		      unsigned long args);

static int hdmi_release(struct inode *node, struct file *file);
ssize_t hdmi_read(struct file *file, char __user * ubuf, size_t size,
		  loff_t * loff);
ssize_t hdmi_write(struct file *file, const char __user * ubuf, size_t size,
		   loff_t * loff);

void stop_hdmi_cat661x(struct hdmi_priv *priv);
void start_hdmi_cat661x(struct hdmi_priv *priv);

const static struct file_operations hdmi_fops = {
	.owner = THIS_MODULE,
	.open = hdmi_open,
	.read = hdmi_read,
	.write = hdmi_write,
	.release = hdmi_release,
	.ioctl = hdmi_ioctl,
};
extern struct hdmi_device hdmi_cat661x_device;
struct hdmi_device *hdmi_dev[] = {
	NULL,
//&hdmi_cat661x_device,
};

static int hdmi_open(struct inode *node, struct file *file)
{
	HDMI_DEBUG("hdmi_open\n");
	return 0;

}

static int hdmi_ioctl(struct inode *node, struct file *file, unsigned int cmd,
		      unsigned long args)
{
	HDMI_DEBUG("hdmi_ioctl id=%d\n", cmd);
	return 0;
}

static int hdmi_release(struct inode *node, struct file *file)
{
	HDMI_DEBUG("hdmi_release\n");
	return 0;
}

ssize_t hdmi_read(struct file * file, char __user * ubuf, size_t size,
		  loff_t * loff)
{
	return 0;
}

ssize_t hdmi_write(struct file * file, const char __user * ubuf, size_t size,
		   loff_t * loff)
{
	return 0;
}

void bank_io_setting(void)
{				//this should move to BSP as default ..
	//*(unsigned long *)0xc12000c0|=0x7c000000;
	//SET_PERIPHS_REG_BITS(0xc0, 0x7c000000);
	set_mio_mux(4,0x7c000000);
}
static int hdmi_device_init(struct hdmi_priv *priv)
{
	bank_io_setting();
#if 0
	struct hdmi_device *dev;
	priv->dev = hdmi_dev[0];
	dev = priv->dev;
	if (dev == NULL) {
		HDMI_ERR("have not register any hdmi device\n");
		return -EIO;
	}
	if (dev->init_fun != NULL)
		dev->init_fun(priv);
	if (dev->video_change != NULL)
		dev->init_fun(priv);
#endif
	start_hdmi_cat661x(priv);
	return 0;
}

static void step_remove(int depth)
{
	switch (depth) {
	case 100:
	case 7:
		stop_hdmi_cat661x(hdmi_priv_data);
	case 6:
		hdmi_video_exit(hdmi_priv_data);
	case 5:
		if (hdmi_priv_data->i2c)
			hdmi_i2c_release_adapter(hdmi_priv_data->i2c);
	case 4:
		device_destroy(hdmi_priv_data->class, MKDEV(HDMI_MAJOR, 0));
	case 3:
		class_destroy(hdmi_priv_data->class);
	case 2:
		unregister_chrdev(HDMI_MAJOR, DEVICE_NAME);
	case 1:
		kfree(hdmi_priv_data);
	case 0:
	default:
		return;
	}
}

static int __init hdmi_probe(struct platform_device *pdev)
{
	int res;

	HDMI_DEBUG("hdmi start probe\n");

	hdmi_priv_data = kmalloc(sizeof(struct hdmi_priv), GFP_KERNEL);

	if (hdmi_priv_data == NULL) {
		HDMI_ERR("Can't alloc hdmi private data memory\n");
		step_remove(0);
		return -ENOMEM;
	}
	memset(hdmi_priv_data, 0, sizeof(struct hdmi_priv));
	res = register_chrdev(HDMI_MAJOR, DEVICE_NAME, &hdmi_fops);

	if (res < 0) {
		HDMI_ERR("Can't register  char devie for " DEVICE_NAME "\n");
		step_remove(1);
		return res;
	} else {
		HDMI_INFO("register " DEVICE_NAME " to char divece(%d)\n",
			  HDMI_MAJOR);
	}

	hdmi_priv_data->class = class_create(THIS_MODULE, DRIVER_NAME);
	if (hdmi_priv_data->class == NULL) {
		HDMI_ERR("class_create create error\n");
		step_remove(2);
		res = -EEXIST;
		return res;
	}
	hdmi_priv_data->dev = device_create(hdmi_priv_data->class,
					    NULL, MKDEV(HDMI_MAJOR, 0),
					    "hdmi%d", 0);
	if (hdmi_priv_data->dev == NULL) {
		HDMI_ERR("device_create create error\n");
		step_remove(3);
		res = -EEXIST;
		return res;
	}
	if (hdmi_video_init(hdmi_priv_data) != 0) {
		HDMI_ERR("Can't init hdmi video?");
		step_remove(5);
		res = -EIO;
		return res;
	}

	hdmi_init_para(hdmi_priv_data);
	if (hdmi_device_init(hdmi_priv_data)) {
		step_remove(6);
		return -EIO;
	}

	return 0;
}

static int hdmi_remove(struct platform_device *pdev)
{
	step_remove(100);
	return 0;
}

static struct platform_driver apollo_hdmi_driver = {
	.probe = hdmi_probe,
	.remove = hdmi_remove,
	.driver = {
		   .name = "apollo-hdmi",
		   } 
};

static int __init apollo_hdmi_init_module(void)
{
	HDMI_INFO(DRIVER_NAME ": init\n");
	platform_driver_register(&apollo_hdmi_driver);
	return 0;
}

static void __exit apollo_hdmi_exit_module(void)
{
	platform_driver_unregister(&apollo_hdmi_driver);
	HDMI_INFO(DRIVER_NAME ": removed\n");
	return;
}

module_init(apollo_hdmi_init_module);
module_exit(apollo_hdmi_exit_module);
