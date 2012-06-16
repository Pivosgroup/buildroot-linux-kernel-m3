/*
 * Common power driver for Amlogic Devices with one or two external
 * power supplies (AC/USB) connected to main and backup batteries,
 * and optional builtin charger.
 *
 * Copyright © 2011-4-27 alex.deng <alex.deng@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/usb/otg.h>

#include <linux/rfkill.h>

#include <linux/aml_modem.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend modem_early_suspend;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static int aml_modem_suspend(struct early_suspend *handler);
static int aml_modem_resume(struct early_suspend *handler);
#else

#define aml_modem_suspend NULL
#define aml_modem_resume NULL

#endif

static struct device *dev;
static struct aml_modem_pdata *g_pData;


#ifdef CONFIG_HAS_EARLYSUSPEND
static int aml_modem_suspend(struct early_suspend *handler)
{
    printk("aml_modem_suspend !!!!!!!!!!!!!###################\n");
    int ret = -1;

    #if 0    
    if(handler ) 
    {
        g_pData->power_off();
        g_pData->disable();
        ret = 0;
    }
    return ret;
    #endif
    return 0 ;
}

static int aml_modem_resume(struct early_suspend *handler)
{
    printk("aml_modem_resume !!!!!!!!!!!!!###################\n");
    int ret = -1;
    #if 0
    if(handler )
    {
        g_pData->power_on();
        g_pData->enable();
        // need to reset ??
        #if 0  
        g_pData->reset();
        #endif
        ret = 0 ;
    }
    return ret;
    #endif
    return 0 ;
}
#endif

static int modem_set_block(void *data, bool blocked)
{
    pr_info("modem_RADIO going: %s\n", blocked ? "off" : "on");

    if( NULL == data)
    {
        return -1 ;
    }

    struct aml_modem_pdata *pData = (struct aml_modem_pdata *)data;

    if (!blocked)
    {
        pr_info("amlogic modem: going ON\n");
        if ( pData->power_on && pData->enable && pData->reset) 
        {
            pData->power_on();
            pData->enable();
            pData->reset();
        }
    } 
    else 
    {
        pr_info("amlogic modem: going OFF\n");
        if (NULL != pData->power_off) 
        {
            pData->power_off();
        }
    }
    return 0;
}

static const struct rfkill_ops modem_rfkill_ops = {
	.set_block = modem_set_block,
};

static int __init aml_modem_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct rfkill *rfk;

    printk(" aml_modem_probe !!!!!!!!!!! #################  \n");

    dev = &pdev->dev;

    if (pdev->id != -1) {
    	dev_err(dev, "it's meaningless to register several "
    		"pda_powers; use id = -1\n");
    	ret = -EINVAL;
    	goto exit;
    }

    g_pData = pdev->dev.platform_data;

    rfk = rfkill_alloc("modem-dev", &pdev->dev, RFKILL_TYPE_WWAN,
			&modem_rfkill_ops, g_pData);
						   
    if (!rfk)
    {
        printk("aml_modem_probe, rfk alloc fail\n");
        ret = -ENOMEM;
        goto exit;
    }

    ret = rfkill_register(rfk);
    if (ret)
    {
        printk("aml_modem_probe, rfkill_register fail\n");
        goto err_rfkill;
    }

    platform_set_drvdata(pdev, rfk);

    #ifdef CONFIG_HAS_EARLYSUSPEND
        modem_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
        modem_early_suspend.suspend = aml_modem_suspend;
        modem_early_suspend.resume = aml_modem_resume;
        modem_early_suspend.param = g_pData;
        register_early_suspend(&modem_early_suspend);
    #endif

err_rfkill:
	rfkill_destroy(rfk);

exit:;	
    return ret;
}

static int aml_modem_remove(struct platform_device *pdev)
{
    int ret = 0;

    printk(" aml_modem_probe !!!!!!!!!!! #################  \n");

    dev = &pdev->dev;

    if (pdev->id != -1) {
        dev_err(dev, "it's meaningless to register several "
        	"pda_powers; use id = -1\n");
        ret = -EINVAL;
        goto exit;
    }

    g_pData = pdev->dev.platform_data;

    if (g_pData->power_off) {
        ret = g_pData->power_off();

    }

    struct rfkill *rfk = platform_get_drvdata(pdev);

    platform_set_drvdata(pdev, NULL);

    if (rfk) {
    	rfkill_unregister(rfk);
    	rfkill_destroy(rfk);
    }
    rfk = NULL;

    #ifdef CONFIG_HAS_EARLYSUSPEND
        unregister_early_suspend(&modem_early_suspend);
    #endif
    
exit:;	
	return ret;
}


MODULE_ALIAS("platform:aml-modem");

static struct platform_driver aml_modem_pdrv = {
    .driver = {
        .name = "aml-modem",
        .owner = THIS_MODULE,	
    },
    .probe = aml_modem_probe,
    .remove = aml_modem_remove,
    .suspend = aml_modem_suspend,
    .resume = aml_modem_resume,
};

static int __init aml_modem_init(void)
{
	printk("amlogic 3G supply init !!!!!!!!!!!!########### \n");
	return platform_driver_register(&aml_modem_pdrv);
}

static void __exit aml_modem_exit(void)
{
	platform_driver_unregister(&aml_modem_pdrv);
}

module_init(aml_modem_init);
module_exit(aml_modem_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Deng");
