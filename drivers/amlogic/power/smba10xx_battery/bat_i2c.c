/* 
 * battery i2c interface
 * Copyright (C) 2010 Amlogic, Inc.
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
 * Author:  shoufu zhao<shoufu.zhao@amlogic.com>
 */  

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>

#include "bat_i2c.h"

#define BAT_I2C_DEBUG_LOG		0

#if BAT_I2C_DEBUG_LOG == 1
	#define logd(x...)  	printk(x)
#else
	#define logd(x...)		NULL
#endif

static struct i2c_client *bat_i2c_client = NULL;

int bat_i2c_read_byte(u8 reg_port, u8 *data_buf)
{
	s32 ret = 0;
	ret = i2c_smbus_read_byte_data(bat_i2c_client, reg_port);
	*data_buf = ret & 0xFF;
	udelay(100);
	return ret;
}

int bat_i2c_write_byte(u8 reg_port, u8 *data_buf)
{
	s32 ret = 0;
	ret = i2c_smbus_write_byte_data(bat_i2c_client, reg_port, *data_buf);
	udelay(100);
	return ret;
}

int bat_i2c_read_word(u8 reg_port, u16 *data_buf)
{
	s32 ret = 0;
	ret = i2c_smbus_read_word_data(bat_i2c_client, reg_port);
	*data_buf = ret & 0xFFFF;
	udelay(100);
	return ret;
}

int bat_i2c_write_word(u8 reg_port, u16 *data_buf)
{
	s32 ret = 0;
	ret = i2c_smbus_write_word_data(bat_i2c_client, reg_port, *data_buf);
	udelay(100);
	return ret;
}

int bat_i2c_block_read(u8 reg_port, u8 *data_buf)
{
	return i2c_smbus_read_block_data(bat_i2c_client, reg_port, data_buf);
}



static int bat_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
	
	logd("battery i2c probe\n");

    bat_i2c_client = client;
	
    return ret;
}

static int bat_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id bat_i2c_id[] = {
    { "bat_i2c", 0 },
    { }
};

static struct i2c_driver bat_i2c_driver = {
    .probe = bat_i2c_probe,
    .remove = bat_i2c_remove,
    .id_table = bat_i2c_id,
    .driver = {
    .name = "bat_i2c",
    },
};

static int __init bat_i2c_init(void)
{
    int ret;

	logd("battery i2c init\n");

    if (bat_i2c_client)
    {
        ret = 0;
    }
    else
    {
        ret = i2c_add_driver(&bat_i2c_driver);
        if (ret < 0) {
            printk("add battery i2c driver error\n");
        }
    }

    return ret;
}

module_init(bat_i2c_init);