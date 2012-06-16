/*
 * TVIN Tuner Bridge Driver
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


/* Standard Linux Headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/sysctl.h>



/* Amlogic Headers */
#include <linux/tvin/tvin.h>

/* Driver Headers */
#include "fq1216me.h"
#include "fq1216me_tuner.h"
#include "fq1216me_demod.h"


#define TUNER_DEVICE_NAME "tuner_fq1216me"


typedef struct fq1216me_device_s {
    struct class            *clsp;
    dev_t                   devno;
    struct cdev             cdev;
//	struct i2c_adapter      *adap;
//    struct i2c_client       *tuner;
//    struct i2c_client       *demod;

    /* reserved for futuer abstract */
    struct tvin_tuner_ops_s *tops;
}fq1216me_device_t;


static struct fq1216me_device_s *devp;

static int fq1216me_open(struct inode *inode, struct file *file)
{
    struct fq1216me_device_s *devp_o;
//    pr_info( "%s . \n", __FUNCTION__ );
    /* Get the per-device structure that contains this cdev */
    devp_o = container_of(inode->i_cdev, fq1216me_device_t, cdev);
    file->private_data = devp_o;

    return 0;
}



static int fq1216me_release(struct inode *inode, struct file *file)
{

    file->private_data = NULL;

    //    pr_info( "%s . \n", __FUNCTION__ );
    return 0;
}

static int fq1216me_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct fq1216me_device_s *devp_c;
    void __user *argp = (void __user *)arg;

	if (_IOC_TYPE(cmd) != TVIN_IOC_MAGIC) {
		return -EINVAL;
	}

    devp_c = container_of(inode->i_cdev, fq1216me_device_t, cdev);
//    pr_info("%s:  \n",__FUNCTION__);
    switch (cmd)
    {
        case TVIN_IOC_G_TUNER_STD:
            {
                tuner_std_id std_id = 0;
                fq1216me_get_std(&std_id);
    		    if (copy_to_user((void __user *)arg, &std_id, sizeof(tuner_std_id)))
    		    {
                    pr_err("%s: TVIN_IOC_G_TUNER_STD para is error. \n " ,__FUNCTION__);
                    ret = -EFAULT;
    		    }
//                pr_info("TVIN_IOC_G_TUNER_STD: %lx, \n", std_id);

            }
            break;
        case TVIN_IOC_S_TUNER_STD:
            {
                tuner_std_id std_id = 0;
                if (copy_from_user(&std_id, argp, sizeof(tuner_std_id)))
                {
                    pr_err("%s: TVIN_IOC_S_TUNER_STD para is error. \n ",__FUNCTION__);
                    ret = -EFAULT;
                    break;
                }
                fq1216me_set_tuner_std(std_id);
                fq1216me_set_demod_std(std_id);
    //            fq1216me_set_tuner();
                fq1216me_set_demod();
//                pr_info("TVIN_IOC_S_TUNER_STD \n");

            }
            break;
        case TVIN_IOC_G_TUNER_FREQ:
            {
                struct tuner_freq_s tuner_freq = {0, 0};
                fq1216me_get_freq(&tuner_freq);
    		    if (copy_to_user((void __user *)arg, &tuner_freq, sizeof(struct tuner_freq_s)))
    		    {
                    pr_err("%s: TVIN_IOC_G_TUNER_FREQ para is error. \n " ,__FUNCTION__);
                    ret = -EFAULT;
    		    }
//                pr_info(" TVIN_IOC_G_TUNER_FREQ: %d, \n", tuner_freq.freq);

            }
            break;
        case TVIN_IOC_S_TUNER_FREQ:
            {
                struct tuner_freq_s tuner_freq = {0, 0};
                if (copy_from_user(&tuner_freq, argp, sizeof(struct tuner_freq_s)))
                {
                    pr_err("%s: TVIN_IOC_S_TUNER_FREQ para is error. \n " ,__FUNCTION__);
                    ret = -EFAULT;
                    break;
                }
                fq1216me_set_freq(tuner_freq);
                fq1216me_set_tuner();
    //            fq1216me_set_demod();
//            pr_info(" TVIN_IOC_S_TUNER_FREQ\n");

            }
            break;
        case TVIN_IOC_G_TUNER_PARM:
            {
                struct tuner_parm_s tuner_parm = {0, 0, 0, 0, 0};
                fq1216me_get_tuner(&tuner_parm);
                fq1216me_get_afc(&tuner_parm);
    		    if (copy_to_user((void __user *)arg, &tuner_parm, sizeof(struct tuner_parm_s)))
    		    {
                    pr_err("%s: TVIN_IOC_G_TUNER_PARM para is error. \n " ,__FUNCTION__);
                    ret = -EFAULT;
    		    }
//                  pr_info(" TVIN_IOC_G_TUNER_PARM-- status: %d, \n", tuner_parm.if_status);
            }
            break;
        default:
            ret = -ENOIOCTLCMD;
            break;
    }

    return ret;
}

static struct file_operations fq1216me_fops = {
    .owner   = THIS_MODULE,
    .open    = fq1216me_open,
    .release = fq1216me_release,
    .ioctl   = fq1216me_ioctl,
};


static int fq1216me_device_init(struct fq1216me_device_s *devp)
{
    int ret;
    struct device *devp_;

    devp = kmalloc(sizeof(struct fq1216me_device_s), GFP_KERNEL);
    if (!devp)
    {
        printk(KERN_ERR "failed to allocate memory\n");
        return -ENOMEM;
    }

    ret = alloc_chrdev_region(&devp->devno, 0, 1, TUNER_DEVICE_NAME);
	if (ret < 0) {
		printk(KERN_ERR "failed to allocate major number\n");
        kfree(devp);
		return -1;
	}
    devp->clsp = class_create(THIS_MODULE, TUNER_DEVICE_NAME);
    if (IS_ERR(devp->clsp))
    {
        unregister_chrdev_region(devp->devno, 1);
        kfree(devp);
        ret = (int)PTR_ERR(devp->clsp);
        return -1;
    }
    cdev_init(&devp->cdev, &fq1216me_fops);
    devp->cdev.owner = THIS_MODULE;
    ret = cdev_add(&devp->cdev, devp->devno, 1);
    if (ret) {
        printk(KERN_ERR "failed to add device\n");
        class_destroy(devp->clsp);
        unregister_chrdev_region(devp->devno, 1);
        kfree(devp);
        return ret;
    }
    devp_ = device_create(devp->clsp, NULL, devp->devno , NULL, "tuner%d", 0);
    if (IS_ERR(devp_)) {
        pr_err("failed to create device node\n");
        class_destroy(devp->clsp);
        unregister_chrdev_region(devp->devno, 1);
        kfree(devp);

        /* @todo do with error */
        return PTR_ERR(devp);;
    }
    return 0;
}

static void fq1216me_device_release(struct fq1216me_device_s *devp)
{
    cdev_del(&devp->cdev);
    device_destroy(devp->clsp, devp->devno );
    cdev_del(&devp->cdev);
    class_destroy(devp->clsp);
    unregister_chrdev_region(devp->devno, 1);
    kfree(devp);
}


static int fq1216me_probe(struct platform_device *pdev)
{
    int ret;
    int i2c_nr = 0;
    pr_info("%s: \n", __FUNCTION__);
    ret = fq1216me_device_init(devp);
    if (ret)
    {
        pr_info(" device init failed\n");
        return ret;
    }

    printk("fq1216me_probe ok.\n");
    return 0;
}

static int fq1216me_remove(struct platform_device *pdev)
{

    fq1216me_device_release(devp);
    printk(KERN_ERR "driver removed ok.\n");

    return 0;
}

static struct platform_driver fq1216me_driver = {
    .probe      = fq1216me_probe,
    .remove     = fq1216me_remove,
    .driver     = {
        .name   = TUNER_DEVICE_NAME,
    }
};


static int __init fq1216me_init(void)
{
    int ret = 0;
    pr_info( "%s . \n", __FUNCTION__ );
    ret = platform_driver_register(&fq1216me_driver);
    if (ret != 0) {
        printk(KERN_ERR "failed to register module, error %d\n", ret);
        return -ENODEV;
    }
    return ret;
}

static void __exit fq1216me_exit(void)
{
    pr_info( "%s . \n", __FUNCTION__ );

    platform_driver_unregister(&fq1216me_driver);
}

module_init(fq1216me_init);
module_exit(fq1216me_exit);

MODULE_DESCRIPTION("Amlogic FQ1216ME Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bo Yang <bo.yang@amlogic.com>");

