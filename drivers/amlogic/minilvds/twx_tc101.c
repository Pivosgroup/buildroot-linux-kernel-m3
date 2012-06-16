//FOR MINILVDS Driver TWX_TC101

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
#include <linux/twx_tc101.h>
#include <linux/platform_device.h>
#ifdef CONFIG_SN7325
#include <linux/sn7325.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend early_suspend;
static int early_suspend_flag = 0;
#endif

static void twx_early_suspend(struct early_suspend *h);
static void twx_late_resume(struct early_suspend *h);
	
static struct i2c_client *twx_tc101_client;
#if 0
static int twx_tc101_i2c_read(unsigned char *buff, unsigned len)
{
    int res = 0;
    struct i2c_msg msgs[] = {
        {
            .addr = twx_tc101_client->addr,
            .flags = 0,
            .len = 1,
            .buf = buff,
        },
        {
            .addr = twx_tc101_client->addr,
            .flags = I2C_M_RD,
            .len = len,
            .buf = buff,
        }
    };

    res = i2c_transfer(twx_tc101_client->adapter, msgs, 2);
    if (res < 0) {
        pr_err("%s: i2c transfer failed\n", __FUNCTION__);
    }

    return res;
}
#endif

static int twx_tc101_i2c_write(unsigned char *buff, unsigned len)
{
    int res = 0;
    struct i2c_msg msg[] = {
        {
        .addr = twx_tc101_client->addr,
        .flags = 0,
        .len = len,
        .buf = buff,
        }
    };

    res = i2c_transfer(twx_tc101_client->adapter, msg, 1);
    if (res < 0) {
        pr_err("%s: i2c transfer failed\n", __FUNCTION__);
    }

    return res;
}

static int twx_tc101_send(unsigned short addr, u8 data)
{
	u8 buf[3]= {addr >> 8, addr&0xff, data};
	return twx_tc101_i2c_write(buf, 3);
}

#if 0
static u8 twx_tc101_recv(unsigned short addr)
{

   int res = 0;
 	u8 buf[2]= {addr >> 8, addr&0xff };
	u8 data = 0x69;
    struct i2c_msg msgs[] = {
        {
            .addr = twx_tc101_client->addr,
            .flags = 0,
            .len = 2,
            .buf = buf,
        },
        {
            .addr = twx_tc101_client->addr,
            .flags = I2C_M_RD,
            .len = 1,
            .buf = &data,
        }
    };
    res = i2c_transfer(twx_tc101_client->adapter, msgs, 2);
	pr_err("i2c return=%d\n", res);
   if (res < 0) {
        pr_err("%s: i2c transfer failed\n", __FUNCTION__);
   }
   else
		res = data;

    return res;
}
#endif

static int twx_tc101_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int res = 0;
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("%s: functionality check failed\n", __FUNCTION__);
        res = -ENODEV;
        goto out;
    }
    twx_tc101_client = client;
	twx_tc101_send(0xf830, 0xb2);msleep(10);//
	twx_tc101_send(0xf831, 0xf0);msleep(10);//
	twx_tc101_send(0xf833, 0xc2);msleep(10);//
	twx_tc101_send(0xf840, 0x80);msleep(10);//
	twx_tc101_send(0xf881, 0xec);msleep(10);//

	//twx_tc101_send(0xf841, 0x02);msleep(10);
	//twx_tc101_send(0xf882, 0x18);msleep(10);

	// twx_tc101_send(0xf820, 0x41);msleep(10);
	// twx_tc101_send(0xf826, 0x1b);msleep(10);
	// twx_tc101_send(0xf827, 0x12);msleep(10);
	//while(1)
	{
	//twx_tc101_send(0xf830, 0xb2);msleep(10);//
		// twx_tc101_send(0xf827, 0x12);
		// msleep(10);
		// printk("0xf820=%x\n",twx_tc101_recv(0xf820));
		// msleep(10);
		// printk("0xf833=%x\n",twx_tc101_recv(0xf833));
		//msleep(10);
		// printk("0xf830=%x\n",twx_tc101_recv(0xf830));
		// msleep(20);
	}
		
out:
    return res;
}

static int twx_tc101_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id twx_tc101_id[] = {
    { "twx_tc101", 0 },
    { }
};

static struct i2c_driver twx_tc101_i2c_driver = {
    .probe = twx_tc101_i2c_probe,
    .remove = twx_tc101_i2c_remove,
    .id_table = twx_tc101_id,
    .driver = {
    .name = "twx_tc101",
    },
};

static void power_on_backlight(void)
{            
    //BL_PWM -> GPIOA_7: 1 (E1)
    set_gpio_val(GPIOA_bank_bit(7), GPIOA_bit_bit0_14(7), 1);
    set_gpio_mode(GPIOA_bank_bit(7), GPIOA_bit_bit0_14(7), GPIO_OUTPUT_MODE); 

}
static int twx_tc101_probe(struct platform_device *pdev){
    int res;
	
	printk("\n\nMINI LVDS Driver Init.\n\n");

    if (twx_tc101_client)
    {
        res = 0;
    }
    else
    {
        res = i2c_add_driver(&twx_tc101_i2c_driver);
        if (res < 0) {
            printk("add twx_tc101 i2c driver error\n");
        }
    }
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	early_suspend.suspend = twx_early_suspend;
	early_suspend.resume = twx_late_resume;
	early_suspend.param = pdev;
	register_early_suspend(&early_suspend);
#endif
	power_on_backlight();
    return res;
}

static int twx_tc101_remove(struct platform_device *pdev)
{
  return 0;
}

static void power_on_lcd(void)
{
    //int setIO_level(unsigned char port, unsigned char iobits, unsigned char offset);    
    //LCD3.3V  EIO -> OD0: 0 
#ifdef CONFIG_SN7325
    configIO(0, 0);
    setIO_level(0, 0, 0);
#else
set_gpio_val(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), 0);
set_gpio_mode(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), GPIO_OUTPUT_MODE);		
#endif
    msleep(20);
    //AVDD  EIO -> OD4: 1
#ifdef CONFIG_SN7325
    configIO(0, 0);
    setIO_level(0, 1, 4);
#else
set_gpio_val(GPIOA_bank_bit(3), GPIOA_bit_bit0_14(3), 1);
set_gpio_mode(GPIOA_bank_bit(3), GPIOA_bit_bit0_14(3), GPIO_OUTPUT_MODE);
#endif
    msleep(50);
}

static int twx_tc101_suspend(struct platform_device * pdev, pm_message_t mesg)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	if (early_suspend_flag)
		return 0;
#endif
	return 0;
}

static int twx_tc101_resume(struct platform_device * pdev)
{
	int res = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (early_suspend_flag)
		return 0;
#endif

	/*
	* already in late resume, but lcd resume later than tc101, 
	* so call power_on_lcd here, and lcd_resume would call it again
	*/
	power_on_lcd();

  if (twx_tc101_client)
  {
			twx_tc101_send(0xf830, 0xb2);msleep(10);//
			twx_tc101_send(0xf831, 0xf0);msleep(10);//
			twx_tc101_send(0xf833, 0xc2);msleep(10);//
			twx_tc101_send(0xf840, 0x80);msleep(10);//
			twx_tc101_send(0xf881, 0xec);msleep(10);//
      res = 0;
  }
  else
  {
      res = i2c_add_driver(&twx_tc101_i2c_driver);
      if (res < 0) {
          printk("add twx_tc101 i2c driver error\n");
      }
  }
  
	return res;
}

int twx_tc101_reinit(void)
{
	int res = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (early_suspend_flag)
		return 0;
#endif

  if (twx_tc101_client)
  {
			twx_tc101_send(0xf830, 0xb2);msleep(10);//
			twx_tc101_send(0xf831, 0xf0);msleep(10);//
			twx_tc101_send(0xf833, 0xc2);msleep(10);//
			twx_tc101_send(0xf840, 0x80);msleep(10);//
			twx_tc101_send(0xf881, 0xec);msleep(10);//
      res = 0;
  }
	return res;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void twx_early_suspend(struct early_suspend *h)
{
	early_suspend_flag = 1;
	twx_tc101_suspend((struct platform_device *)h->param, PMSG_SUSPEND);
}

static void twx_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return;
	early_suspend_flag = 0;
	twx_tc101_resume((struct platform_device *)h->param);
}
#endif


static struct platform_driver twx_tc101_driver = {
	.probe = twx_tc101_probe,
  .remove = twx_tc101_remove,
  .suspend = twx_tc101_suspend,
  .resume = twx_tc101_resume,
	.driver = {
		.name = "twx",
	},
};

static int __init twx_tc101_init(void)
{
	int ret = 0;
    printk( "twx_tc101_init. \n");

    ret = platform_driver_register(&twx_tc101_driver);
    if (ret != 0) {
        printk(KERN_ERR "failed to register twx module, error %d\n", ret);
        return -ENODEV;
    }
    return ret;
}

static void __exit twx_tc101_exit(void)
{
	pr_info("twx_tc101: exit\n");
    platform_driver_unregister(&twx_tc101_driver);
}
	
//arch_initcall(twx_tc101_init);
module_init(twx_tc101_init);
module_exit(twx_tc101_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("MINILVDS driver for TWX_TC101");
MODULE_LICENSE("GPL");
