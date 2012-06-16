/*
 * I2C chip deivce driver template
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
 * Author:
 *
 */
#ifndef _AMLOGIC_CAMERA_COMMON_C
#define _AMLOGIC_CAMERA_COMMON_C

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
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

#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>  //copy_from_user copy_to_user



/* Amlogic headers */
#include <mach/am_regs.h>
#include <linux/camera/amlogic_camera_common.h>

#include "amlogic_camera_ov5640.h"
#include "amlogic_camera_gc0308.h"
#include "amlogic_camera_gt2005.h"

//add for gc0308
#include <mach/pinmux.h>
#ifdef CONFIG_SN7325
#include <linux/sn7325.h>
#endif


#define CAMERA_COUNT              1

static dev_t camera_devno;
static struct class *camera_clsp = NULL;
static camera_dev_t *camera_devp = NULL;
static struct i2c_client *camera_client = NULL;

#ifdef CONFIG_CAMERA_OV5640
struct aml_camera_ctl_s amlogic_camera_info_ov5640 = {
    .para = {
    	.camera_name= AMLOGIC_CAMERA_OV5640_NAME,
        .saturation = SATURATION_0_STEP,
        .brighrness = BRIGHTNESS_0_STEP,
        .contrast = CONTRAST_0_STEP,
        .hue = HUE_0_DEGREE,
//      .special_effect = SPECIAL_EFFECT_NORMAL,
        .exposure = EXPOSURE_0_STEP,
        .sharpness = SHARPNESS_AUTO_STEP,
        .mirro_flip = MF_NORMAL,
        .resolution = TVIN_SIG_FMT_CAMERA_1280X720P_30Hz,
    },
    .awb_mode = ADVANCED_AWB,
    .test_mode = TEST_OFF,
};
#endif
#ifdef CONFIG_CAMERA_GC0308
struct aml_camera_ctl_s amlogic_camera_info_gc0308 = {
    .para = {
    	.camera_name= AMLOGIC_CAMERA_GC0308_NAME,
        .saturation = SATURATION_0_STEP,
        .brighrness = BRIGHTNESS_0_STEP,
        .contrast = CONTRAST_0_STEP,
        .hue = HUE_0_DEGREE,
//      .special_effect = SPECIAL_EFFECT_NORMAL,
        .exposure = EXPOSURE_0_STEP,
        .sharpness = SHARPNESS_AUTO_STEP,
        .mirro_flip = MF_NORMAL,
        .resolution = TVIN_SIG_FMT_CAMERA_1280X720P_30Hz,
    },
    .awb_mode = ADVANCED_AWB,
    .test_mode = TEST_OFF,
};
#endif
#ifdef CONFIG_CAMERA_GT2005
struct aml_camera_ctl_s amlogic_camera_info_gt2005 = {
    .para = {
    	.camera_name= AMLOGIC_CAMERA_GT2005_NAME,
        .saturation = SATURATION_0_STEP,
        .brighrness = BRIGHTNESS_0_STEP,
        .contrast = CONTRAST_0_STEP,
        .hue = HUE_0_DEGREE,
//      .special_effect = SPECIAL_EFFECT_NORMAL,
        .exposure = EXPOSURE_0_STEP,
        .sharpness = SHARPNESS_AUTO_STEP,
        .mirro_flip = MF_NORMAL,
        .resolution = TVIN_SIG_FMT_CAMERA_1280X720P_30Hz,
    },
    .awb_mode = ADVANCED_AWB,
    .test_mode = TEST_OFF,
};
#endif

int camera_read_first(char *buf, int len)
{
    int  i2c_flag = -1;
	struct i2c_msg msgs[] = {
		{
			.addr	= camera_client->addr,
			.flags	= 0,
		    #if FIRST_CAMERA_I2C_ADDRESS_FORMAT
			.len	= 2,
			#else
            .len	= 1,
            #endif
			.buf	= buf,
		},
		{
			.addr	= camera_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

		i2c_flag = i2c_transfer(camera_client->adapter, msgs, 2);

	return i2c_flag;
}

int  camera_write_first(char *buf, int len)
{
	struct i2c_msg msg[] = {
	    {
			.addr	= camera_client->addr,
			.flags	= 0,    //|I2C_M_TEN,
			.len	= len,
			.buf	= buf,
		}

	};

	if (i2c_transfer(camera_client->adapter, msg, 1) < 0)
	{
	//	unsigned short off_addr = buf[0] | (buf[1] << 8);
	//	pr_info("error in writing ov5640 register(%x). \n", off_addr);
		return -1;
	}
	else
		return 0;
}

int  camera_write_byte_first(unsigned short addr, unsigned char data)
{
	unsigned char buff[4];
  #if FIRST_CAMERA_I2C_ADDRESS_FORMAT
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    buff[2] = data;
    if((camera_write_first(buff, 3)) < 0)
        return -1;
    else
        return 0;
  #else
    buff[0] = addr;
    buff[2] = data;
    if((camera_write_first(buff, 2)) < 0)
        return -1;
    else
        return 0;
  #endif
}

int  camera_write_word_first(unsigned short addr, unsigned short data)
{
    unsigned char buff[4];
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    buff[2] = (unsigned char)(data & 0xff);
    buff[3] = (unsigned char)((data >> 8) & 0xff);
    if((camera_write_first(buff, 4)) < 0)
        return -1;
    else
        return 0;
}

unsigned char  camera_read_byte_first(unsigned short addr )
{
    unsigned char buff[4];
  #if FIRST_CAMERA_I2C_ADDRESS_FORMAT
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
  #else
    buff[0] = addr;
  #endif
    if((camera_read_first(buff, 1)) < 0)
        return -1;
    else
    {
        return buff[0];
    }
}

unsigned short camera_read_word_first(unsigned short addr)
{
    unsigned short data;
    unsigned char buff[2];
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    if((camera_read_first(buff, 2)) < 0)
        return -1;
    else
    {
        data = buff[1];
        data = (data << 8) | buff[0];
        return data;
    }
}

#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE

static dev_t camera_devno_second;
static struct class *camera_clsp_second = NULL;
static camera_dev_t *camera_devp_second = NULL;
static struct i2c_client *camera_client_second = NULL;

int camera_read_second(char *buf, int len)
{
    int  i2c_flag = -1;
	struct i2c_msg msgs[] = {
		{
			.addr	= camera_client_second->addr,
			.flags	= 0,
		    #if SECOND_CAMERA_I2C_ADDRESS_FORMAT
			.len	= 2,
			#else
            .len	= 1,
            #endif
			.buf	= buf,
		},
		{
			.addr	= camera_client_second->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};
		i2c_flag = i2c_transfer(camera_client_second->adapter, msgs, 2);

	return i2c_flag;
}

int  camera_write_second(char *buf, int len)
{
	struct i2c_msg msg[] = {
	    {
			.addr	= camera_client_second->addr,
			.flags	= 0,    //|I2C_M_TEN,
			.len	= len,
			.buf	= buf,
		}

	};
		if (i2c_transfer(camera_client_second->adapter, msg, 1) < 0) {
//			unsigned short off_addr = buf[0] | (buf[1] << 8);
//			pr_info("error in writing ov5640 register(%x). \n", off_addr);
			return -1;
		}
		else
			return 0;
}

int  camera_write_byte_second(unsigned short addr, unsigned char data)
{
    unsigned char buff[4];
  #if SECOND_CAMERA_I2C_ADDRESS_FORMAT
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    buff[2] = data;
    if((camera_write_second(buff, 3)) < 0)
        return -1;
    else
        return 0;
  #else
    buff[0] = addr;
    buff[2] = data;
    if((camera_write_second(buff, 2)) < 0)
        return -1;
    else
        return 0;
  #endif

}

unsigned char  camera_read_byte_second(unsigned short addr )
{
    unsigned char buff[4];
  #if SECOND_CAMERA_I2C_ADDRESS_FORMAT
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
  #else
    buff[0] = addr;
  #endif
    if((camera_read_second(buff, 1)) < 0)
        return -1;
    else
    {
        return buff[0];
    }
}

int  camera_write_word_second(unsigned short addr, unsigned short data)
{
    unsigned char buff[4];
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    buff[2] = (unsigned char)(data & 0xff);
    buff[3] = (unsigned char)((data >> 8) & 0xff);
    if((camera_write_second(buff, 4)) < 0)
        return -1;
    else
        return 0;

}

unsigned short camera_read_word_second(unsigned short addr)
{
    unsigned short data;
    unsigned char buff[2];
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    if((camera_read_second(buff, 2)) < 0)
        return -1;
    else
    {
        data = buff[1];
        data = (data << 8) | buff[0];
        return data;
    }
}
#endif

#ifdef CONFIG_CAMERA_OV5640
int ov5640_read(char *buf, int len)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_read_first(buf, len);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_read_second(buf, len);
	}
	#endif
}
int ov5640_write(char *buf, int len)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_write_first(buf, len);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_write_second(buf, len);
	}
	#endif
}
int ov5640_write_byte(unsigned short addr, unsigned char data)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_write_byte_first(addr, data);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_write_byte_second(addr, data);
	}
	#endif
}

int  ov5640_write_word(unsigned short addr, unsigned short data)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_write_word_first(addr, data);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_write_word_second(addr, data);
	}
	#endif
}
unsigned short ov5640_read_word(unsigned short addr)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_read_word_first(addr);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_read_word_second(addr);
	}
	#endif
}
unsigned char  ov5640_read_byte(unsigned short addr )
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_read_byte_first(addr );
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_OV5640_NAME))
	{
		return camera_read_byte_second(addr );
	}
	#endif
}
#endif

#ifdef CONFIG_CAMERA_GC0308
int  gc0308_write(char *buf, int len)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GC0308_NAME))
	{
		return camera_write_first(buf, len);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GC0308_NAME))
	{
		return camera_write_second(buf, len);
	}
	#endif
}
int  gc0308_write_byte(unsigned short addr, unsigned char data)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GC0308_NAME))
	{
		return camera_write_byte_first(addr, data);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GC0308_NAME))
	{
		return camera_write_byte_second(addr, data);
	}
	#endif
}
int  gc0308_read(char *buf, int len)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GC0308_NAME))
	{
		return camera_read_first(buf, len);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GC0308_NAME))
	{
		return camera_read_second(buf, len);
	}
	#endif
}

#endif

#ifdef CONFIG_CAMERA_GT2005
int gt2005_write(char *buf, int len)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GT2005_NAME))
	{
		return camera_write_first(buf, len);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GT2005_NAME))
	{
		return camera_write_second(buf, len);
	}
	#endif
}
int gt2005_write_byte(unsigned short addr, unsigned char data)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GT2005_NAME))
	{
		return camera_write_byte_first(addr, data);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GT2005_NAME))
	{
		return camera_write_byte_second(addr, data);
	}
	#endif
}
int gt2005_read(char *buf, int len)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GT2005_NAME))
	{
		return camera_read_first(buf, len);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GT2005_NAME))
	{
		return camera_read_second(buf, len);
	}
	#endif
}

unsigned char gt2005_read_byte(unsigned short addr)
{
	if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GT2005_NAME))
	{
		return camera_read_byte_first(addr);
	}
	#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
	else if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GT2005_NAME))
	{
		return camera_read_byte_second(addr);
	}
	#endif
}
#endif
//int  ov5640_write_word(unsigned short addr, unsigned short data);
//unsigned short ov5640_read_word(unsigned short addr);
//unsigned char  ov5640_read_byte(unsigned short addr );


static int camera_open(struct inode *inode, struct file *file)
{
    camera_dev_t *devp;

    /* Get the per-device structure that contains this cdev */
    devp = container_of(inode->i_cdev, camera_dev_t, cdev);
    file->private_data = devp;

    return 0;
}

static int camera_release(struct inode *inode, struct file *file)
{
//    camera_dev_t *devp = file->private_data;
    file->private_data = NULL;

    return 0;
}
static int camera_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int res = 0;
    void __user *argp = (void __user *)arg;
    struct camera_info_s para;
    switch(cmd)
    {
        case CAMERA_IOC_START:
            //printk( " start camera engineer. \n ");
            if(copy_from_user(&para, argp, sizeof(struct camera_info_s)))
            {
                printk(KERN_ERR " para is error, use the default value. \n ");
            }
            else
            {
            	#ifdef CONFIG_CAMERA_OV5640
                if (!strcmp(para.camera_name,AMLOGIC_CAMERA_OV5640_NAME))
                    memcpy(&amlogic_camera_info_ov5640.para, &para,sizeof(struct camera_info_s) );
                #endif
                #ifdef CONFIG_CAMERA_GC0308
				printk("camera ioctl para.camera_name is %p para.camera_name is %s. \n",para.camera_name,para.camera_name);
                if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GC0308_NAME))
                    memcpy(&amlogic_camera_info_gc0308.para, &para,sizeof(struct camera_info_s) );
                #endif
                #ifdef CONFIG_CAMERA_GT2005
                if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GT2005_NAME))
                    memcpy(&amlogic_camera_info_gt2005.para, &para,sizeof(struct camera_info_s) );
                #endif
            }
            printk("saturation = %d, brighrness = %d, contrast = %d, hue = %d, exposure = %d, sharpness = %d, mirro_flip = %d, resolution = %d . \n",
                para.saturation, para.brighrness, para.contrast, para.hue, para.exposure, para.sharpness, para.mirro_flip, para.resolution);

            #ifdef CONFIG_CAMERA_OV5640
            if (!strcmp(para.camera_name,AMLOGIC_CAMERA_OV5640_NAME))
                start_camera_ov5640();
            #endif
            #ifdef CONFIG_CAMERA_GC0308
            if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GC0308_NAME))
                start_camera_gc0308();
            #endif
            #ifdef CONFIG_CAMERA_GT2005
            if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GT2005_NAME))
                start_camera_gt2005();
            #endif
            
            printk(KERN_INFO " start camera  %s. \n ",para.camera_name);
            break;

        case CAMERA_IOC_STOP:

            if(copy_from_user(&para, argp, sizeof(struct camera_info_s)))
            {
                printk(KERN_ERR " para is error, use the default value. \n ");
            }

            #ifdef CONFIG_CAMERA_OV5640
            if (!strcmp(para.camera_name,AMLOGIC_CAMERA_OV5640_NAME))
                stop_camera_ov5640();
            #endif
            #ifdef CONFIG_CAMERA_GC0308
            if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GC0308_NAME))
                stop_camera_gc0308();
            #endif
            #ifdef CONFIG_CAMERA_GT2005
            if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GT2005_NAME))
                stop_camera_gt2005();
            #endif
            printk(KERN_INFO " stop camera %s. \n ",para.camera_name);

            break;

        case CAMERA_IOC_SET_PARA:
            if(copy_from_user(&para, argp, sizeof(struct camera_info_s)))
            {
                pr_err(" no para for camera setting, do nothing. \n ");
            }
            else
            {
             #ifdef CONFIG_CAMERA_OV5640
             if (!strcmp(para.camera_name,AMLOGIC_CAMERA_OV5640_NAME))
                 set_camera_para_ov5640(&para);
             #endif
             #ifdef CONFIG_CAMERA_GC0308
             if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GC0308_NAME))
                 set_camera_para_gc0308(&para);
             #endif
             #ifdef CONFIG_CAMERA_GT2005
             if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GT2005_NAME))
                 set_camera_para_gt2005(&para);
             #endif
             printk(KERN_INFO " set camera para %s. \n ",para.camera_name);
            }
            break;

        case CAMERA_IOC_GET_PARA:

             #ifdef CONFIG_CAMERA_OV5640
             if (!strcmp(para.camera_name,AMLOGIC_CAMERA_OV5640_NAME))
                 copy_to_user((void __user *)arg, &amlogic_camera_info_ov5640.para, sizeof(struct camera_info_s));
             #endif
             #ifdef CONFIG_CAMERA_GC0308
             if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GC0308_NAME))
                 copy_to_user((void __user *)arg, &amlogic_camera_info_gc0308.para, sizeof(struct camera_info_s));
             #endif
             #ifdef CONFIG_CAMERA_GT2005
             if (!strcmp(para.camera_name,AMLOGIC_CAMERA_GT2005_NAME))
                 copy_to_user((void __user *)arg, &amlogic_camera_info_gt2005.para, sizeof(struct camera_info_s));
             #endif
             printk(KERN_INFO " get camera para %s. \n ",para.camera_name);

            break;

        default:
                printk("camera ioctl cmd is invalid. \n");
                break;
    }
    return res;
}

static int amlogic_camera_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	//unsigned char data[16] = {0};
	int res = 0;
	struct amlogic_camera_platform_data *pdata = pdata = client->dev.platform_data;
	if(pdata){
		if(pdata->first_init)
			pdata->first_init();
		else
			pr_info("first camera init failed");
		#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
		if(pdata->second_init)
			pdata->second_init();
		else
			pr_info("second camera init failed");
		#endif
		
		}
    
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		return -1;
	}
	camera_client = client;
//    res = misc_register(&ov5640_device);
    printk( "amlogic camera: camera_client->addr = %x\n", camera_client->addr);
    msleep(10);
	return res;
}

static int amlogic_camera_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id amlogic_camera_i2c_id[] = {
	{ AMLOGIC_CAMERA_I2C_NAME_FIRST, 0 },
	{ }
};

static struct i2c_driver amlogic_camera_i2c_driver = {
    .id_table	= amlogic_camera_i2c_id,
	.probe 		= amlogic_camera_i2c_probe,
	.remove 	= amlogic_camera_i2c_remove,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= AMLOGIC_CAMERA_I2C_NAME_FIRST,
	},
};

static struct file_operations camera_fops = {
    .owner   = THIS_MODULE,
    .open    = camera_open,
    .release = camera_release,
    .ioctl   = camera_ioctl,
};

static int amlogic_camera_probe(struct platform_device *pdev)
{
    int ret;
    struct device *devp;

    printk( "camera: start amlogic camera probe. \n");

    camera_devp = kmalloc(sizeof(struct camera_dev_s), GFP_KERNEL);
    if (!camera_devp)
    {
        pr_err( "camera: failed to allocate memory for camera device\n");
        return -ENOMEM;
    }
    msleep(10);

    ret = alloc_chrdev_region(&camera_devno, 0, CAMERA_COUNT, AMLOGIC_CAMERA_DEVICE_NAME_FIRST);
	if (ret < 0) {
		pr_err("camera: failed to allocate chrdev. \n");
		return 0;
	}

    camera_clsp = class_create(THIS_MODULE, AMLOGIC_CAMERA_DEVICE_NAME_FIRST);
    if (IS_ERR(camera_clsp))
    {
        unregister_chrdev_region(camera_devno, CAMERA_COUNT);
        return PTR_ERR(camera_clsp);
    }

    /* connect the file operations with cdev */
    cdev_init(&camera_devp->cdev, &camera_fops);
    camera_devp->cdev.owner = THIS_MODULE;
    /* connect the major/minor number to the cdev */
    ret = cdev_add(&camera_devp->cdev, camera_devno, 1);
    if (ret) {
        pr_err( "camera: failed to add device. \n");
        /* @todo do with error */
        return ret;
    }
    /* create /dev nodes */
    devp = device_create(camera_clsp, NULL, MKDEV(MAJOR(camera_devno), 0),
                        NULL, "camera%d", 0);
    if (IS_ERR(devp)) {
        pr_err("camera: failed to create device node\n");
        class_destroy(camera_clsp);
        /* @todo do with error */
        return PTR_ERR(devp);
    }

    /* We expect this driver to match with the i2c device registered
     * in the board file immediately. */
    ret = i2c_add_driver(&amlogic_camera_i2c_driver);
    if (ret < 0 || camera_client == NULL) {
        pr_err("camera: failed to add i2c driver. \n");
        ret = -ENOTSUPP;
    }

    printk( "camera: driver initialized ok\n");
//    reset_hw_camera();      //remove into ***_board.c
    return ret;
}

static int amlogic_camera_remove(struct platform_device *pdev)
{
//    power_down_hw_camera();     //remove into ***_board.c
#ifdef CONFIG_CAMERA_OV5640
    if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_OV5640_NAME))
        power_down_sw_camera_ov5640();
#endif
#ifdef CONFIG_CAMERA_GC0308
if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GC0308_NAME))
        stop_camera_gc0308();
#endif
#ifdef CONFIG_CAMERA_GT2005
if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GT2005_NAME))
        stop_camera_gt2005();
#endif
    i2c_del_driver(&amlogic_camera_i2c_driver);
    device_destroy(camera_clsp, MKDEV(MAJOR(camera_devno), 0));
    cdev_del(&camera_devp->cdev);
    class_destroy(camera_clsp);
    unregister_chrdev_region(camera_devno, CAMERA_COUNT);
    kfree(camera_devp);

     return 0;
}

static struct platform_driver amlogic_camera_driver = {
	.probe = amlogic_camera_probe,
    .remove = amlogic_camera_remove,
	.driver = {
		.name = AMLOGIC_CAMERA_DEVICE_NAME_FIRST,
//		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE

static int amlogic_camera_i2c_probe_second(struct i2c_client *client, const struct i2c_device_id *id)
{
	//unsigned char data[16] = {0};
	int res = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		return -1;
	}
	camera_client_second = client;
//    res = misc_register(&ov5640_device);
    printk( "amlogic camera: camera_client_second->addr = %x\n", camera_client_second->addr);
    msleep(10);
	return res;
}

static int amlogic_camera_i2c_remove_second(struct i2c_client *client)
{
	return 0;
}


static const struct i2c_device_id amlogic_camera_i2c_id_second[] = {
	{ AMLOGIC_CAMERA_I2C_NAME_SECOND, 0 },
	{ }
};

static struct i2c_driver amlogic_camera_i2c_driver_second = {
    .id_table	= amlogic_camera_i2c_id_second,
	.probe 		= amlogic_camera_i2c_probe_second,
	.remove 	= amlogic_camera_i2c_remove_second,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= AMLOGIC_CAMERA_I2C_NAME_SECOND,
	},
};

static int amlogic_camera_probe_second(struct platform_device *pdev)
{
    int ret;
    struct device *devp;

    printk( "camera second :start amlogic second camera probe. \n");

    camera_devp_second = kmalloc(sizeof(struct camera_dev_s), GFP_KERNEL);
    if (!camera_devp_second)
    {
        pr_err( "camera second : failed to allocate memory for second camera device\n");
        return -ENOMEM;
    }
    msleep(10);

    ret = alloc_chrdev_region(&camera_devno_second, 0, CAMERA_COUNT, AMLOGIC_CAMERA_DEVICE_NAME_SECOND);
	if (ret < 0) {
		pr_err("camera second : failed to allocate chrdev. \n");
		return 0;
	}

    camera_clsp_second = class_create(THIS_MODULE, AMLOGIC_CAMERA_DEVICE_NAME_SECOND);
    if (IS_ERR(camera_clsp_second))
    {
        unregister_chrdev_region(camera_devno_second, CAMERA_COUNT);
        return PTR_ERR(camera_clsp_second);
    }

    /* connect the file operations with cdev */
    cdev_init(&camera_devp_second->cdev, &camera_fops);
    camera_devp->cdev.owner = THIS_MODULE;
    /* connect the major/minor number to the cdev */
    ret = cdev_add(&camera_devp_second->cdev, camera_devno_second, 1);
    if (ret) {
        pr_err( "camera second : failed to add device. \n");
        /* @todo do with error */
        return ret;
    }
    /* create /dev nodes */
    devp = device_create(camera_clsp_second, NULL, MKDEV(MAJOR(camera_devno_second), 0),
                        NULL, "camera%d", 0);
    if (IS_ERR(devp)) {
        pr_err("camera second : failed to create device node\n");
        class_destroy(camera_clsp_second);
        /* @todo do with error */
        return PTR_ERR(devp);
    }

    /* We expect this driver to match with the i2c device registered
     * in the board file immediately. */
    ret = i2c_add_driver(&amlogic_camera_i2c_driver_second);
    if (ret < 0 || camera_client_second == NULL) {
        pr_err("camera: failed to add i2c driver. \n");
        ret = -ENOTSUPP;
    }

    printk( "camera second : driver initialized ok\n");
    return ret;
}

static int amlogic_camera_remove_second(struct platform_device *pdev)
{
//    power_down_hw_camera();     //remove into ***_board.c
#ifdef CONFIG_CAMERA_OV5640
    if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_OV5640_NAME))
        power_down_sw_camera_ov5640();
#endif
#ifdef CONFIG_CAMERA_GC0308
    if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GC0308_NAME))
        stop_camera_gc0308();
#endif
#ifdef CONFIG_CAMERA_GT2005
    if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GT2005_NAME))
        stop_camera_gt2005();
#endif
    i2c_del_driver(&amlogic_camera_i2c_driver_second);
    device_destroy(camera_clsp_second, MKDEV(MAJOR(camera_devno_second), 0));
    cdev_del(&camera_devp_second->cdev);
    class_destroy(camera_clsp_second);
    unregister_chrdev_region(camera_devno_second, CAMERA_COUNT);
    kfree(camera_devp_second);
    return 0;
}

static struct platform_driver amlogic_camera_driver_second = {
	.probe = amlogic_camera_probe_second,
    .remove = amlogic_camera_remove_second,
	.driver = {
		.name = AMLOGIC_CAMERA_DEVICE_NAME_SECOND,
//		.owner = THIS_MODULE,
	},
};
#endif

static int __init amlogic_camera_init(void)
{
    int ret = 0;
    printk( "amlogic camera driver: init. \n");
	/*
    if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GT2005_NAME))
    {
	    eth_set_pinmux(ETH_BANK0_GPIOC3_C12,ETH_CLK_OUT_GPIOC12_REG3_1, 1);
		#ifdef CONFIG_SN7325
	    	configIO(1, 0);
	    	setIO_level(1, 1, 0);
	    #endif
    }*/
   /* if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_FIRST,AMLOGIC_CAMERA_GC0308_NAME))
    {
	    eth_set_pinmux(ETH_BANK0_GPIOC3_C12,ETH_CLK_OUT_GPIOC12_REG3_1, 1);
		#ifdef CONFIG_SN7325
	    	configIO(1, 0);
	    	setIO_level(1, 0, 0);
	    #endif
    }*/
    ret = platform_driver_register(&amlogic_camera_driver);
    if (ret != 0) {
        printk(KERN_ERR "failed to register amlogic camera module, error %d\n", ret);
        return -ENODEV;
    }
	
     #ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
      /*if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GT2005_NAME))
      {
	    eth_set_pinmux(ETH_BANK0_GPIOC3_C12,ETH_CLK_OUT_GPIOC12_REG3_1, 1);
		#ifdef CONFIG_SN7325
	    	configIO(1, 0);
	    	setIO_level(1, 1, 0);
	    #endif
      }
      if (!strcmp(AMLOGIC_CAMERA_DEVICE_NAME_SECOND,AMLOGIC_CAMERA_GC0308_NAME))
      {
	    eth_set_pinmux(ETH_BANK0_GPIOC3_C12,ETH_CLK_OUT_GPIOC12_REG3_1, 1);
		#ifdef CONFIG_SN7325
	    	configIO(1, 0);
	    	setIO_level(1, 0, 0);
	    #endif
      }*/
    ret = platform_driver_register(&amlogic_camera_driver_second);
    if (ret != 0) {
        printk(KERN_ERR "failed to register second amlogic camera module, error %d\n", ret);
        return -ENODEV;
    }
    #endif
    return ret;

//	return i2c_add_driver(&amlogic_camera_i2c_driver);
}

static void __exit amlogic_camera_exit(void)
{
	pr_info("amlogic camera driver: exit\n");
    platform_driver_unregister(&amlogic_camera_driver);

    #ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
    platform_driver_unregister(&amlogic_camera_driver_second);
    #endif

}

module_init(amlogic_camera_init);
module_exit(amlogic_camera_exit);

MODULE_AUTHOR("pmd@amlogic.com>");
MODULE_DESCRIPTION(" amlogic camera device ");
MODULE_LICENSE("GPL");

#endif
