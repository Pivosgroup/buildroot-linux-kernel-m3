/*
 *
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * echo wc 0x12345678 1234 > aml_debug
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>

/* Amlogic headers */
#include <mach/am_regs.h>

/* Local headers */
#include "aml_debug.h"

#define DRIVER_NAME     "amlogic_debug"
#define DEVICE_NAME     "amlogic_debug"
#define BUS_NAME        "pseudo"


static void amdbg_usage(void)
{
    pr_err("Usage:echo rc|rp|rx|rh|rm address > /sys/bus/pseudo/drivers/amlogic_debug/amlogic_reg\n");
    pr_err("      echo wc|wp|wx|wh|wm address value > /sys/bus/pseudo/drivers/amlogic_debug/amlogic_reg\n");
    pr_err("       r:read w:write c:CBUS p:APB x:AXI h:AHB m:MPEG\n");
    return;
}



/*
* support bus CBUS/APB/AXI/AHB/MEPG register read/write.
* special support ra/wa command required by Frank.Zhao for APB.
*
* adress must be hexadecimal and prefix with ox.
*/
static ssize_t amdbg_store(struct device_driver * drv, const char * buf, size_t count)
{
    int n = 0;
    char *buf_orig, *ps, *token;
    char *parm[3];
    unsigned int addr = 0, val = 0, retval = 0;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
        parm[n++] = token;
	}

    if ((n > 0) && (strlen(parm[0]) != 2))
    {
        pr_err("invalid command\n");
        amdbg_usage();
        kfree(buf_orig);
        return count;
    }

    if ((parm[0][0] == 'r'))
    {
        if (n != 2)
        {
            pr_err("read: invalid parameter\n");
            amdbg_usage();
            kfree(buf_orig);
            return count;
        }
        addr = simple_strtol(parm[1], NULL, 16);
        pr_err("%s 0x%x\n", parm[0], addr);
        switch (parm[0][1])
        {
            case 'c':
                retval = READ_CBUS_REG(addr);
                break;
            case 'p':
                retval = READ_APB_REG(addr);
                break;
            case 'a':
                addr = addr << 2;
                retval = READ_APB_REG(addr);
                break;
            case 'x':
                retval = READ_AXI_REG(addr);
                break;
            case 'h':
                retval = READ_AHB_REG(addr);
                break;
            case 'm':
                retval = READ_MPEG_REG(addr);
                break;
            default:
                break;
        }
        pr_info("%s: 0x%x --> 0x%x\n", parm[0], addr, retval);
    }
    else if ((parm[0][0] == 'w'))
    {
        if (n != 3)
        {
            pr_err("write: invalid parameter\n");
            amdbg_usage();
            kfree(buf_orig);
            return count;
        }
        addr = simple_strtol(parm[1], NULL, 16);
        val  = simple_strtol(parm[2], NULL, 16);
        pr_err("%s 0x%x 0x%x", parm[0], addr, val);
        switch (parm[0][1])
        {
            case 'c':
                retval = WRITE_CBUS_REG(addr, val);
                break;
            case 'p':
                retval = WRITE_APB_REG(addr, val);
                break;
            case 'a':
                addr = addr << 2;
                retval = WRITE_APB_REG(addr, val);
                break;
            case 'x':
                retval = WRITE_AXI_REG(addr, val);
                break;
            case 'h':
                retval = WRITE_AHB_REG(addr, val);
                break;
            case 'm':
                retval = WRITE_MPEG_REG(addr, val);
                break;
            default:
                break;
        }
        pr_info("%s: 0x%x <-- 0x%x\n", parm[0], addr, retval);
    }
    else
    {
        pr_err("invalid command\n");
        amdbg_usage();
    }

	kfree(buf_orig);
    return count;
}

DRIVER_ATTR(amlogic_reg, S_IWUSR | S_IRUGO, NULL, amdbg_store);


static void amdbg_release(struct device *dev)
{
    return;
}

static struct device amdbg_dev = {
	.init_name	= DEVICE_NAME,
    .release    = amdbg_release,
};

static int amdbg_bus_match(struct device *dev, struct device_driver *dev_driver)
{
	return 1;
}

static struct bus_type amdbg_bus = {
	.name   = BUS_NAME,
	.match  = amdbg_bus_match,
};

static struct device_driver amdbg_drv = {
	.name   = DRIVER_NAME,
    .bus    = &amdbg_bus,
};

static int __init amdbg_init(void)
{
    int ret = 0;

    ret = device_register(&amdbg_dev);
	if (ret < 0) {
		pr_warning("aml_debug: device_register error: %d\n", ret);
        return ret;
	}

	ret = bus_register(&amdbg_bus);
	if (ret < 0) {
		pr_warning("aml_debug: bus_register error: %d\n", ret);
        device_unregister(&amdbg_dev);
		return ret;
	}

    ret = driver_register(&amdbg_drv);
	if (ret < 0) {
		pr_warning("aml_debug: driver_register error: %d\n", ret);
        bus_unregister(&amdbg_bus);
        device_unregister(&amdbg_dev);
        return ret;
	}

    ret = driver_create_file(&amdbg_drv, &driver_attr_amlogic_reg);
	if (ret < 0) {
		pr_warning("aml_debug: driver_create_file error: %d\n",ret);
        driver_remove_file(&amdbg_drv, &driver_attr_amlogic_reg);
        driver_unregister(&amdbg_drv);
        bus_unregister(&amdbg_bus);
        device_unregister(&amdbg_dev);
        return ret;
	}
    return ret;
}

static void __exit amdbg_exit(void)
{
    driver_remove_file(&amdbg_drv, &driver_attr_amlogic_reg);
    driver_unregister(&amdbg_drv);
    bus_unregister(&amdbg_bus);
    device_unregister(&amdbg_dev);
}


module_init(amdbg_init);
module_exit(amdbg_exit);

MODULE_DESCRIPTION("Amlogic debug driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobby Yang <bo.yang@amlogic.com>");

