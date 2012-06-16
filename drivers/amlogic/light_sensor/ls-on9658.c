/*
 * Light Sensor Driver - GL3526
 *
 * Copyright (C) 2010 Amlogic Corporation
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
 */
 
#include <linux/platform_device.h>
#include <linux/input-polldev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/saradc.h>
#include <linux/delay.h>

#define POLL_INTERVAL	2000 	/* poll for input every 2s*/
#define LUX_LEVEL	4					/* 0~4 report 5 levels*/

//#define ON9658_DEBUG
#ifdef ON9658_DEBUG
#define DEBUG_OUT(fmt,args...)    printk(fmt,##args)
#else
#define DEBUG_OUT(fmt,args...)
#endif

static struct platform_device *pdev;
static struct input_polled_dev *light_sensor_idev;

static const int sAdcValues[LUX_LEVEL] = {
    500,
    50, //invalid
    50, //invalid
	50
};

struct light_sensor_info {
	int lux_level;
	int suspend;
};
static struct light_sensor_info ls_info;

/* Sysfs Files */
static ssize_t light_sensor_lux_level_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ls_info.lux_level);
}

static DEVICE_ATTR(lux_level, 0444, light_sensor_lux_level_show, NULL);

static struct attribute *light_sensor_attributes[] = {
	&dev_attr_lux_level.attr,
	NULL,
};

static struct attribute_group light_sensor_attribute_group = {
	.attrs = light_sensor_attributes,
};


/* Device model stuff */
static int light_sensor_probe(struct platform_device *dev)
{
	printk(KERN_INFO "light_sensor: device successfully initialized.\n");
	return 0;
}

#ifdef CONFIG_PM
static int light_sensor_suspend(struct platform_device *dev, pm_message_t state)
{
		ls_info.suspend = 1;
    return 0;
}

static int light_sensor_resume(struct platform_device *dev)
{
		ls_info.suspend = 0;
    return 0;
}
#else
	#define light_sensor_suspend NULL
	#define light_sensor_resume  NULL
#endif

static struct platform_driver light_sensor_driver = {
	.probe = light_sensor_probe,
	.resume = light_sensor_resume,
	.suspend = light_sensor_suspend,
	.driver	= {
		.name = "light_sensor",
		.owner = THIS_MODULE,
	},
};

static void light_sensor_dev_poll(struct input_polled_dev *dev)
{
	struct input_dev *input_dev = dev->input;
	int adc_val, adc_val2, i;

	adc_val = get_adc_sample(6);
	msleep(3);
	adc_val2 = get_adc_sample(6);
	if ((adc_val < 0) || (adc_val < 0)) {
		printk(KERN_ERR "light_sensor_dev_poll get_adc_sample %d %d\n", adc_val, adc_val2);
		return;
	}
	
	adc_val = (adc_val + adc_val2)/2;
	DEBUG_OUT("light_sensor: light_sensor_dev_poll adc_val = %d\n", adc_val);
	
	for(i = 0; i < LUX_LEVEL; i++) {
		if(adc_val > sAdcValues[i])
			break;
	}
	
	if(ls_info.lux_level != i) {
		ls_info.lux_level = i;	
		input_report_abs(input_dev, ABS_X, ls_info.lux_level);
		input_sync(input_dev);
		
		DEBUG_OUT("light_sensor: light_sensor_dev_poll lux_level = %d\n", ls_info.lux_level);
	}	
}

/* Module stuff */
static int __init light_sensor_init(void)
{
	struct input_dev *idev;
	int ret;

	ret = platform_driver_register(&light_sensor_driver);
	if (ret)
		goto out;

	pdev = platform_device_register_simple("light_sensor", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		goto out_driver;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &light_sensor_attribute_group);
	if (ret)
		goto out_device;

	light_sensor_idev = input_allocate_polled_device();
	if (!light_sensor_idev) {
		ret = -ENOMEM;
		goto out_group;
	}

	light_sensor_idev->poll = light_sensor_dev_poll;
	light_sensor_idev->poll_interval = POLL_INTERVAL;

	/* initialize the input class */
	idev = light_sensor_idev->input;
	idev->name = "light_sensor";
	idev->phys = "light_sensor/input0";
	idev->id.bustype = BUS_ISA;
	idev->dev.parent = &pdev->dev;
	idev->evbit[0] = BIT_MASK(EV_ABS);
	input_set_abs_params(idev, ABS_X,
			0, LUX_LEVEL, 0, 1);

	ret = input_register_polled_device(light_sensor_idev);
	if (ret)
		goto out_idev;

	printk(KERN_INFO "light_sensor: driver successfully loaded.\n");
	return 0;

out_idev:
	input_free_polled_device(light_sensor_idev);
out_group:
	sysfs_remove_group(&pdev->dev.kobj, &light_sensor_attribute_group);
out_device:
	platform_device_unregister(pdev);
out_driver:
	platform_driver_unregister(&light_sensor_driver);
out:
	printk(KERN_WARNING "light_sensor: driver init failed (ret=%d)!\n", ret);
	return ret;
}

static void __exit light_sensor_exit(void)
{
	input_unregister_polled_device(light_sensor_idev);
	input_free_polled_device(light_sensor_idev);
	sysfs_remove_group(&pdev->dev.kobj, &light_sensor_attribute_group);
	platform_device_unregister(pdev);
	platform_driver_unregister(&light_sensor_driver);	

	printk(KERN_INFO "light_sensor: driver unloaded.\n");
}

module_init(light_sensor_init);
module_exit(light_sensor_exit);

MODULE_AUTHOR("michael.zhang@amlogic.com");
MODULE_DESCRIPTION("Light Sensor Driver - GL3526");
MODULE_LICENSE("GPL");
