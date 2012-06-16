/*
 * Copyright (C) 2010 MEMSIC, Inc.
 *
 * Initial Code:
 *	Robbie Cao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
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

#include <linux/mma8452.h>

#define DEBUG			0
#define MAX_FAILURE_COUNT	3

#define MMA8452_DELAY_PWRON	300	/* ms, >= 300 ms */
#define MMA8452_DELAY_PWRDN	1	/* ms */
#define MMA8452_DELAY_SETDETECTION	MMA8452_DELAY_PWRON

#define MMA8452_RETRY_COUNT	3

static struct i2c_client *this_client;

static int mma8452_i2c_rx_data(char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	for (i = 0; i < MMA8452_RETRY_COUNT; i++) {
		if (i2c_transfer(this_client->adapter, msgs, 2) > 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= MMA8452_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, MMA8452_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int mma8452_i2c_tx_data(char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= len,
			.buf	= buf,
		}
	};
	
	for (i = 0; i < MMA8452_RETRY_COUNT; i++) {
		if (i2c_transfer(this_client->adapter, msg, 1) > 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= MMA8452_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, MMA8452_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int mma8452_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int mma8452_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mma8452_charge(unsigned char Mdata, unsigned char Ldata)
{
	int data;
	data = Mdata << 8 | Ldata;
	#if 0
	data = data>>4;
	/*if((data & 0x800)==1)
		data=0x1000-data;
	else
		data+=0x800;*/
	#else
	data = data>>8;
	#endif
	return data;
}

static int mma8452_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	unsigned char data[16] = {0};
	int vec[3] = {0};

	switch (cmd) {

	case MMA8452_IOC_PWRON:
		data[0] = MMA8452_REG_CTRL;
		data[1] = MMA8452_CTRL_PWRON_2;
		if (mma8452_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		data[0] = MMA8452_XYZ_DATA_CFG;
		data[1] = MMA8452_CTRL_MODE_2G;
		if (mma8452_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		data[0] = MMA8452_REG_CTRL;
		data[1] = MMA8452_CTRL_PWRON_2;
		data[1] |= MMA8452_CTRL_ACTIVE;
		if (mma8452_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
	#if DEBUG
		printk("MMA 8452 POWER ON SUCCEED\n");
	#endif
		/* wait PWRON done */
		msleep(MMA8452_DELAY_PWRON);
		break;

	case MMA8452_IOC_PWRDN:
		data[0] = MMA8452_REG_CTRL;
		data[1] = MMA8452_CTRL_PWRON_2;
		data[1] &= ~MMA8452_CTRL_ACTIVE;	//STANDBY
		if (mma8452_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		/* wait PWRDN done */
		msleep(MMA8452_DELAY_PWRDN);
		break;

	case MMA8452_IOC_READXYZ:
		data[0] = MMA8452_REG_DATA;
		if (mma8452_i2c_rx_data(data, 6) < 0) {
			return -EFAULT;
		}
		vec[0] = mma8452_charge(data[0], data[1]);		
		vec[1] = mma8452_charge(data[2], data[3]);		
		vec[2] = mma8452_charge(data[4], data[5]);
		
	#if DEBUG
		printk("[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
	#endif
	
		if (copy_to_user(pa, vec, sizeof(vec))) {
			return -EFAULT;
		}
		break;
	case MMA8452_IOC_READSTATUS:
		data[0] = MMA8452_REG_DATA;
		if (mma8452_i2c_rx_data(data, 6) < 0) {
			return -EFAULT;
		}
		vec[0] = mma8452_charge(data[0], data[1]);		
		vec[1] = mma8452_charge(data[2], data[3]);		
		vec[2] = mma8452_charge(data[4], data[5]);
	#if DEBUG
		printk("[X - %04x] [Y - %04x] [z - %04x]\n", 
			vec[0], vec[1], vec[2]);
	#endif
		if (copy_to_user(pa, vec, sizeof(vec))) {
			return -EFAULT;
		}
		break;
#if 0			//modify by Rojam 2011-05-11 14:38
	case MMA8452_IOC_SETDETECTION:
		data[0] = MMA8452_REG_CTRL;
		if (copy_from_user(&(data[1]), pa, sizeof(unsigned char))) {
			return -EFAULT;
		}
		if (mma8452_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		/* wait SETDETECTION done */
		msleep(MMA8452_DELAY_SETDETECTION);
		break;
#endif	//modify by Rojam 2011-05-11
	default:
		break;
	}

	return 0;
}

static ssize_t mma8452_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "MMA8452");
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(mma8452, S_IRUGO, mma8452_show, NULL);

static struct file_operations mma8452_fops = {
	.owner		= THIS_MODULE,
	.open		= mma8452_open,
	.release	= mma8452_release,
	.ioctl		= mma8452_ioctl,
};

static struct miscdevice mma8452_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mma8452",
	.fops = &mma8452_fops,
};

int mma8452_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}
	this_client = client;

	res = misc_register(&mma8452_device);
	if (res) {
		pr_err("%s: mma8452_device register failed\n", __FUNCTION__);
		goto out;
	}
	res = device_create_file(&client->dev, &dev_attr_mma8452);
	if (res) {
		pr_err("%s: device_create_file failed\n", __FUNCTION__);
		goto out_deregister;
	}
	
	return 0;

out_deregister:
	misc_deregister(&mma8452_device);
out:
	return res;
}

static int mma8452_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_mma8452);
	misc_deregister(&mma8452_device);

	return 0;
}

static const struct i2c_device_id mma8452_id[] = {
	{ MMA8452_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver mma8452_driver = {
	.probe 		= mma8452_probe,
	.remove 	= mma8452_remove,
	.id_table	= mma8452_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name = MMA8452_I2C_NAME,
	},
};

static int __init mma8452_init(void)
{
	pr_info("mma8452 driver: init\n");
	return i2c_add_driver(&mma8452_driver);
}

static void __exit mma8452_exit(void)
{
	pr_info("mma8452 driver: exit\n");
	i2c_del_driver(&mma8452_driver);
}

module_init(mma8452_init);
module_exit(mma8452_exit);

MODULE_AUTHOR("Robbie Cao<hjcao@memsic.com>");
MODULE_DESCRIPTION("MEMSIC MMA8452 (DTOS) Accelerometer Sensor Driver");
MODULE_LICENSE("GPL");

