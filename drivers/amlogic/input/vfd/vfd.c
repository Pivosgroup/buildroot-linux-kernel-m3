/*
 * linux/drivers/input/vfd/vfd.c
 *
 * VFD Driver
 *
 * Copyright (C) 2011 Amlogic Corporation
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * author :   tiejun_peng
 */
 /*
 * !!caution: 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <mach/am_regs.h>
#include <mach/pinmux.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include <linux/input/vfd.h>

#ifdef CONFIG_VFD_SM1628
#include "vfd_comm.h"
#endif

struct vfd {
		struct input_dev *input;
		struct timer_list timer;		
	  char config_name[20];
	  struct class *config_class;
	  struct device *config_dev;
	  int config_major;	
		unsigned int cur_keycode;			
		unsigned int debug_enable;
		char set_led_value[8];
		char cur_led_value[8];
		struct vfd_key *key;
		int key_num;
};

type_vfd_printk vfd_input_dbg;

static struct vfd *gp_vfd=NULL;

static DEFINE_MUTEX(led_set_mutex);

int vfd_printk(const char *fmt, ...)
{
    va_list args;
    int r;

    if (gp_vfd->debug_enable==0)  return 0;
    va_start(args, fmt);
    r = vprintk(fmt, args);
    va_end(args);
    return r;
}

/*****************************************************************
**
** func : hardware init
**       in this function will do pin configuration and and initialize for hardware
**
********************************************************************/

static int set_led_string( char *string)
{	
		sprintf(gp_vfd->set_led_value, "%.7s", string);
		gp_vfd->set_led_value[7] = '\0';		
		return 0;
}

static ssize_t vfd_key_get(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", gp_vfd->cur_keycode);
}

static ssize_t vfd_led_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", gp_vfd->cur_led_value);
}

//control var by sysfs .
static ssize_t vfd_led_store(struct device *dev, struct device_attribute *attr,
                    const char *buf, size_t count)
{
		char value[8];
		if (sscanf(buf, "%7s", value) != 1)
       return -EINVAL;
		vfd_printk("function[%s]line%d :vfd led display: %s \n",__FUNCTION__,__LINE__, value);
		mutex_lock(&led_set_mutex);
		set_led_string(value);
		mutex_unlock(&led_set_mutex);
		
    return strlen(buf);
}


static DEVICE_ATTR(key, S_IRUGO | S_IWUSR, vfd_key_get, NULL);
static DEVICE_ATTR(led, S_IRUGO | S_IWUSR, vfd_led_show, vfd_led_store);

/*******************************************************************
**stand code
********************************************************************/


static int vfd_search_key(struct vfd *vfd)
{
		struct vfd_key *key = vfd->key;
		int value,j;
	
		value = get_vfd_key_value();		
		if (value <= 0) {
			return 0;
		}
		vfd_printk("function<%s> line <%d> get VFD key value : [%d]  \n",__FUNCTION__,__LINE__,value);
	 	for (j=0; j<vfd->key_num; j++) {
			if ((value == key->value)) {
				return key->code;
			}
			key++;
		}
		
	return 0;
}

static void vfd_work(struct vfd *vfd)
{
		int code = vfd_search_key(vfd);
	
		if (vfd->cur_keycode) {
			if (!code) {
				vfd_printk("vfd report key code: [%d] released.\n", vfd->cur_keycode);
				input_report_key(vfd->input, vfd->cur_keycode, 0);
				vfd->cur_keycode = 0;	
			}
			else {
			// detect another key while pressed
			}	
		}
		else {
			if (code) {
				vfd->cur_keycode = code;
				vfd_printk("vfd report key code: [%d] pressed.\n", vfd->cur_keycode);
				input_report_key(vfd->input, vfd->cur_keycode, 1);
			}
		}
}

void vfd_timer_sr(unsigned long data)
{
		struct vfd *vfd_data=(struct vfd *)data;
		int i;
		vfd_work(vfd_data);
		if(strcmp(gp_vfd->cur_led_value,gp_vfd->set_led_value)) {								
				vfd_printk("function[%s] line %d current LED value :%s ,set LED value :%s \n",__FUNCTION__,__LINE__,gp_vfd->cur_led_value,gp_vfd->set_led_value);
				strcpy(gp_vfd->cur_led_value,gp_vfd->set_led_value);//vfd->cur_ledcode = vfd->set_ledcode;
				set_vfd_led_value(gp_vfd->cur_led_value);
			}
		mod_timer(&vfd_data->timer,jiffies+msecs_to_jiffies(200));
}

static int
vfd_config_open(struct inode *inode, struct file *file)
{
    file->private_data = gp_vfd;
    return 0;
}

static int
vfd_config_release(struct inode *inode, struct file *file)
{
    file->private_data=NULL;
    return 0;
}

static const struct file_operations vfd_fops = {
    .owner      = THIS_MODULE,
    .open       = vfd_config_open,
    .ioctl      = NULL,
    .release    = vfd_config_release,
};

static int  register_vfd_dev(struct vfd  *vfd)
{
    int ret=0;
    strcpy(vfd->config_name,"aml_vfd");
    ret=register_chrdev(0,vfd->config_name,&vfd_fops);
    if(ret <=0)
    {
        printk("register char dev vfd error\r\n");
        return  ret ;
    }
    vfd->config_major=ret;
    printk("vfd config major:%d\r\n",ret);
    vfd->config_class=class_create(THIS_MODULE,vfd->config_name);
    vfd->config_dev=device_create(vfd->config_class,NULL,MKDEV(vfd->config_major,0),NULL,vfd->config_name);
    return ret;
}

static int __init vfd_probe(struct platform_device *pdev)
{
    struct vfd *vfd;
    struct input_dev *input_dev;
    int ret,i;
    struct vfd_key *temp_key;
    struct vfd_platform_data *pdata = pdev->dev.platform_data;
    
    if (!pdata) {
        dev_err(&pdev->dev, "platform data is required!\n");
        return -EINVAL;
    }
    if(hardware_init(pdata)) {
    		dev_err(&pdev->dev, "vfd hardware initial failed!\n");  
    		return -EINVAL;
    }
    
    vfd = kzalloc(sizeof(struct vfd), GFP_KERNEL);
    input_dev = input_allocate_device();
    if (!vfd || !input_dev)
        goto err1;
 
    gp_vfd = vfd;
    vfd->debug_enable = 1;
    vfd_input_dbg = vfd_printk;
		platform_set_drvdata(pdev, vfd);
		
		vfd->input = input_dev;
		vfd->cur_keycode = 0;
		strcpy(vfd->set_led_value, "HELLO");
		strcpy(vfd->cur_led_value, "");
		
		setup_timer(&vfd->timer, vfd_timer_sr, (unsigned long)vfd) ;
		mod_timer(&vfd->timer, jiffies+msecs_to_jiffies(100));
		
		/* setup input device */
    set_bit(EV_KEY, input_dev->evbit);
    set_bit(EV_REP, input_dev->evbit);
    
    vfd->key = pdata->key;
    vfd->key_num = pdata->key_num;
    temp_key = pdata->key;
		for (i=0; i<vfd->key_num; i++) {
				set_bit(temp_key->code, input_dev->keybit);
				printk(KERN_INFO "%s vfd key(%d) registed.\n", temp_key->name, temp_key->code);
        temp_key++;
    }
    
		ret = device_create_file(&pdev->dev, &dev_attr_key);
		if (ret < 0)
        goto err1;
    ret = device_create_file(&pdev->dev, &dev_attr_led);  
		if (ret < 0)
        goto err1;
    vfd_input_dbg("device_create_file completed \r\n");   
      
    input_dev->name = "vfd_keypad";
    input_dev->phys = "vfd_keypad/input0";
    input_dev->dev.parent = &pdev->dev;

    input_dev->id.bustype = BUS_ISA;
    input_dev->id.vendor = 0x0001;
    input_dev->id.product = 0x0001;
    input_dev->id.version = 0x0100;

    input_dev->rep[REP_DELAY]=0xffffffff;
    input_dev->rep[REP_PERIOD]=0xffffffff;

    input_dev->keycodesize = sizeof(unsigned short);
    input_dev->keycodemax = 0x1ff;

    ret = input_register_device(vfd->input);
    if (ret < 0) {
        printk(KERN_ERR "Unable to register vfdkeypad input device\n");
        goto err2;
    }
    vfd_input_dbg("input_register_device completed \r\n");    

    register_vfd_dev(gp_vfd);
    return 0;
/*    
err3:
    input_unregister_device(vfd->input);
    input_dev = NULL;
*/    
err2:
    device_remove_file(&pdev->dev, &dev_attr_key);
    device_remove_file(&pdev->dev, &dev_attr_led);
err1:
    kfree(vfd);
    input_free_device(input_dev);

    return -EINVAL;
}

static int vfd_remove(struct platform_device *pdev)
{
	  struct vfd *vfd = platform_get_drvdata(pdev);
	   /* unregister everything */
    input_unregister_device(vfd->input);
		device_remove_file(&pdev->dev, &dev_attr_key);
    device_remove_file(&pdev->dev, &dev_attr_led);
    
		input_free_device(vfd->input);
    unregister_chrdev(vfd->config_major,vfd->config_name);
    if(vfd->config_class)
    {
        if(vfd->config_dev)
        device_destroy(vfd->config_class,MKDEV(vfd->config_major,0));
        class_destroy(vfd->config_class);
    }       
    kfree(vfd);
    gp_vfd = NULL ;
    return 0;	
	
}



static struct platform_driver vfd_driver = {
    .probe      = vfd_probe,
    .remove     = vfd_remove,
    .suspend    = NULL,
    .resume     = NULL,
    .driver     = {
        .name   = "m1-vfd",
    },
};

static int __devinit vfd_init(void)
{
    printk(KERN_INFO "VFD Driver\n");

    return platform_driver_register(&vfd_driver);
}

static void __exit vfd_exit(void)
{
    printk(KERN_INFO "VFD exit \n");
    platform_driver_unregister(&vfd_driver);
}

module_init(vfd_init);
module_exit(vfd_exit);

MODULE_AUTHOR("tiejun_peng");
MODULE_DESCRIPTION("Amlogic VFD Driver");
MODULE_LICENSE("GPL");
