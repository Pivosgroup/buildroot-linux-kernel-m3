/* 
 * TCA6424 i2c interface
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
 * Author:  wang han<han.wang@amlogic.com>
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

static struct i2c_client *tca6424_client;

unsigned char get_configIO(unsigned char port);
int configIO(unsigned char port, unsigned char ioflag);
unsigned char getIO_level(unsigned char port);
int setIO_level(unsigned char port, unsigned char iobits);

static int tca6424_i2c_read(unsigned char *buff, unsigned len)
{
    int res = 0;
    struct i2c_msg msgs[] = {
		{
			.addr	= tca6424_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buff,
		},
		{
			.addr	= tca6424_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buff,
		}
	};
	
	res = i2c_transfer(tca6424_client->adapter, msgs, 2);
	if (res < 0) {
			pr_err("%s: i2c transfer failed\n", __FUNCTION__);
		}
	
	return res;
}

static int tca6424_i2c_write(unsigned char *buff, unsigned len)
{
    int res = 0;
    struct i2c_msg msg[] = {
		{
			.addr	= tca6424_client->addr,
			.flags	= 0,
			.len	= len,
			.buf	= buff,
		}
	};
	
	res = i2c_transfer(tca6424_client->adapter, msg, 1);
	if (res < 0) {
			pr_err("%s: i2c transfer failed\n", __FUNCTION__);
		}
		
    return res;
}

static int tca6424_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int res = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}
	tca6424_client = client;
	
out:
    return res;
}

static int tca6424_client_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id tca6424_id[] = {
	{ "tca6424", 0 },
	{ }
};

static struct i2c_driver tca6424_driver = {
	.probe 		= tca6424_probe,
	.remove 	= tca6424_client_remove,
	.id_table	= tca6424_id,
	.driver 	= {
		.name	= "tca6424",
	},
};

int tca6424_init(void)
{
    int res;
    printk("tca6424_init \n");
    if (tca6424_client)
    {
        res = 0;
    }
    else
    { 
        res = i2c_add_driver(&tca6424_driver);
	    if (res < 0) {
	        printk("add tca6424 i2c driver error\n");	    
	    }
	}	
	
	return res;
}

unsigned char get_configIO(unsigned char port)
{
    unsigned char ioconf = 0;
    switch(port)
    {
        case 0:
        case 1:
        case 2:
            ioconf = 0xC+port;
            tca6424_i2c_read(&ioconf, 1);
            break;
    }
    
    return ioconf;
}
arch_initcall(tca6424_init);

int configIO(unsigned char port, unsigned char ioflag)
{
    int res = 0;
    unsigned char buff_data[2];
    switch(port)
    {
        case 0:
        case 1:
        case 2:
            buff_data[0] = 0xC+port;
            buff_data[1] = ioflag;
            res = tca6424_i2c_write(buff_data, 2);
            break;
    }
    
    return res;
}

unsigned char get_config_polinv(unsigned char port)
{
    unsigned char polconf = 0;
    switch( port )
    {
        case 0:
        case 1:
        case 2:
            polconf = 0x8+port;
            tca6424_i2c_read(&polconf, 1);
            break;
    }
}

int config_pol_inv(unsigned char port, unsigned char polflag)
{
    int res = 0;
    unsigned char buff_data[2];
    
    switch( port )
    {
        case 0:
        case 1:
        case 2:
            buff_data[0] = 0x8+port;
            buff_data[2] = polflag;
            res = tca6424_i2c_write(buff_data, 2);
            break;
    }
    
    return res;
}

unsigned char getIO_level(unsigned char port)
{
    unsigned char iobits = 0;
    
    switch( port )
    {
        case 0:
        case 1:
        case 2:
            iobits = port;
            tca6424_i2c_read(&iobits, 1);
            break;
    }
    
    return iobits;
}

int setIO_level(unsigned char port, unsigned char iobits)
{
    int res = 0;
    unsigned char buff_data[2];
    
    switch( port )
    {
        case 0:
        case 1:
        case 2:
            buff_data[0] = 0x4+port;
            buff_data[1] = iobits;
            res = tca6424_i2c_write(buff_data, 2);
            break;
    }
    
    return res;
}
