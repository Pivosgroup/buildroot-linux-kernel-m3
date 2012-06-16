/*
 * AMLOGIC backlight driver.
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
 * Author:  Wang Han <han.wang@amlogic.com>
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/aml_bl.h>
#include <mach/power_gate.h>

//#define AML_BL_DBG

struct aml_bl {
	const struct aml_bl_platform_data	*pdata;
	struct backlight_device			*bldev;
	struct platform_device			*pdev;	
};

static int aml_bl_update_status(struct backlight_device *bd)
{
	struct aml_bl *amlbl = bl_get_data(bd);
	int brightness = bd->props.brightness;	
#ifdef AML_BL_DBG
    printk(KERN_INFO "enter aml_bl_update_status\n");
#endif
//	if (bd->props.power != FB_BLANK_UNBLANK)
//		brightness = 0;
//	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
//		brightness = 0;

    if( brightness < 0 )
        brightness = 0;
    else if( brightness > 255 )
        brightness = 255;
        
    if((brightness > 0)&&(0 == IS_CLK_GATE_ON(VGHL_PWM))){
        CLK_GATE_ON(VGHL_PWM);
#ifdef AML_BL_DBG        
        printk("CLK_GATE_ON VGHL_PWM\n");        
#endif        
    } 
    
    if( amlbl->pdata->set_bl_level )
        amlbl->pdata->set_bl_level(brightness);

    if((brightness == 0)&&(IS_CLK_GATE_ON(VGHL_PWM))){
        CLK_GATE_OFF(VGHL_PWM);
#ifdef AML_BL_DBG         
        printk("CLK_GATE_OFF VGHL_PWM\n");   
#endif                 
    }
    
  
     
#ifdef AML_BL_DBG
    if( amlbl->pdata->get_bl_level )
	    printk(KERN_INFO "%d\n", amlbl->pdata->get_bl_level());
#endif
	return 0;
}

static int aml_bl_get_brightness(struct backlight_device *bd)
{
	struct aml_bl *amlbl = bl_get_data(bd);
	int brightness;
#ifdef AML_BL_DBG
    printk(KERN_INFO "enter aml_bl_get_brightness\n");
#endif
    if( amlbl->pdata->get_bl_level )
	    brightness = amlbl->pdata->get_bl_level();
#ifdef AML_BL_DBG
    printk(KERN_INFO "%d\n", brightness);
#endif    
	return brightness;
}

static const struct backlight_ops aml_bl_ops = {
	.get_brightness = aml_bl_get_brightness,
	.update_status  = aml_bl_update_status,
};

static int aml_bl_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	const struct aml_bl_platform_data *pdata;
	struct backlight_device *bldev;
	struct aml_bl *amlbl;
	int retval;

    printk(KERN_INFO "enter aml_bl_probe\n");
	amlbl = kzalloc(sizeof(struct aml_bl), GFP_KERNEL);
	if (!amlbl)
    {   
        printk(KERN_ERR "kzalloc error\n");
		return -ENOMEM;
    }

	amlbl->pdev = pdev;
    
	pdata = pdev->dev.platform_data;
	if (!pdata) {
        printk(KERN_ERR "missing platform data\n");
		retval = -ENODEV;
		goto err;
	}	

	amlbl->pdata = pdata;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 255;
	bldev = backlight_device_register("aml-bl", &pdev->dev, amlbl,
					  &aml_bl_ops, &props);
	if (IS_ERR(bldev)) {
        printk(KERN_ERR "failed to register backlight\n");
		retval = PTR_ERR(bldev);
		goto err;
	}

	amlbl->bldev = bldev;

	platform_set_drvdata(pdev, amlbl);
	
	bldev->props.power = FB_BLANK_UNBLANK;
	bldev->props.brightness = 250;

    if( pdata->bl_init )
        pdata->bl_init();
    if( pdata->power_on_bl )
        pdata->power_on_bl();
	if( pdata->set_bl_level )
        pdata->set_bl_level(250);

	return 0;

err:
	kfree(amlbl);
	return retval;
}

static int __exit aml_bl_remove(struct platform_device *pdev)
{
	struct aml_bl *amlbl = platform_get_drvdata(pdev);

    printk(KERN_INFO "enter aml_bl_remove\n");
	backlight_device_unregister(amlbl->bldev);
	platform_set_drvdata(pdev, NULL);
	kfree(amlbl);

	return 0;
}

static struct platform_driver aml_bl_driver = {
	.driver = {
		.name = "aml-bl",
        .owner = THIS_MODULE,
	},
	.probe = aml_bl_probe,
	.remove = __exit_p(aml_bl_remove),
};

static int __init aml_bl_init(void)
{
    //printk(KERN_INFO "enter aml_bl_init\n");
	return platform_driver_register(&aml_bl_driver);    
}
module_init(aml_bl_init);

static void __exit aml_bl_exit(void)
{
    //printk(KERN_INFO "enter aml_bl_exit\n");
	platform_driver_unregister(&aml_bl_driver);
}
module_exit(aml_bl_exit);

MODULE_DESCRIPTION("Amlogic backlight driver");
MODULE_LICENSE("GPL");

