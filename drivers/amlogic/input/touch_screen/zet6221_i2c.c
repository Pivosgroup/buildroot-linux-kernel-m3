/* drivers/input/touchscreen/zet6221_i2c.c
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * ZEITEC Semiconductor Co., Ltd
 * Tel: +886-3-579-0045
 * Fax: +886-3-579-9960
 * http://www.zeitecsemi.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>



#include <asm/io.h>
#include <asm/irq.h>
#include <asm/gpio.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>  //for mini6410 2.6.36 kree(),kmalloc()


#include <linux/gpio.h>

#include <mach/pinmux.h>
#include <mach/gpio.h>


#define ZET_TS_ID_NAME "zet6221"

//#define MJ5_TS_NAME "zet6221_touchscreen"
#define MJ5_TS_NAME "zet6221"
//#define MJ5_TS_NAME "pixcir168"

#define TS1_INT_GPIO		9  /*s3c6410*/
//#define TS1_INT_GPIO        AT91_PIN_PB17 /*AT91SAM9G45 external*/
//#define TS1_INT_GPIO        AT91_PIN_PA27 /*AT91SAM9G45 internal*/
#if 0
#define GPIO_INT ((GPIOD_bank_bit2_24(24)<<16) |GPIOD_bit_bit2_24(24))
#define GPIO_RST ((GPIOD_bank_bit2_24(23)<<16) |GPIOD_bit_bit2_24(23))
#else
//#define GPIO_ITK_PENIRQ ((GPIOA_bank_bit0_27(16)<<16) | GPIOA_bit_bit0_27(16))
//#define GPIO_ITK_RST ((GPIOC_bank_bit0_15(3)<<16) | GPIOC_bit_bit0_15(3))
#define GPIO_INT 		((GPIOA_bank_bit0_27(16)<<16) | GPIOA_bit_bit0_27(16))
#define GPIO_POWER_EN		((GPIOC_bank_bit0_15(9)<<16) | GPIOC_bit_bit0_15(9))
#define GPIO_RST 		((GPIOC_bank_bit0_15(3)<<16) | GPIOC_bit_bit0_15(3))
#endif
//#define TPINFO	1
#if 0
#define X_MAX	1536
#define Y_MAX	832
#else
#if 0
#define X_MIN	0
#define Y_MIN	0
#define X_MAX	1728
#define Y_MAX	1024
#else
// for 7' dashiye
#if 0
#define X_MIN	34
#define Y_MIN	0
#define X_MAX	1718
#define Y_MAX	980
#else
#define X_MIN	0
#define Y_MIN	0
#define X_MAX	1728
#define Y_MAX	1024
//#define X_MAX	1024
//#define Y_MAX	600
#endif
#endif
#endif
//#define FINGER_NUMBER 4
#define FINGER_NUMBER 10
//#define FINGER_NUMBER 1
#define KEY_NUMBER 0
//#define P_MAX	1
#define P_MAX	255
#define D_POLLING_TIME	25000
#define U_POLLING_TIME	25000
#define S_POLLING_TIME  100

#define MAX_KEY_NUMBER      	8
#define MAX_FINGER_NUMBER	16
#define TRUE 		1
#define FALSE 		0

#define debug_mode 1
#define DPRINTK(fmt,args...)	do { if (debug_mode) printk(KERN_EMERG "[%s][%d] "fmt"\n", __FUNCTION__, __LINE__, ##args);} while(0)

//#define TRANSLATE_ENABLE 1
#define TOPRIGHT 	0
#define TOPLEFT  	1
#define BOTTOMRIGHT	2
#define BOTTOMLEFT	3
#define ORIGIN		BOTTOMRIGHT
//#define ORIGIN		TOPRIGHT

volatile u32 bePressed[FINGER_NUMBER];
volatile u32 g_bePressed = 0;

#define DEBUG_DIY	0
#define DEBUG_DIY_2	0

struct msm_ts_platform_data {
	unsigned int x_max;
	unsigned int y_max;
	unsigned int pressure_max;
};

struct zet6221_tsdrv {
	struct i2c_client *i2c_ts;
	struct work_struct work1;
	struct input_dev *input;
	struct timer_list polling_timer;
	struct early_suspend early_suspend;
	unsigned int gpio; /* GPIO used for interrupt of TS1*/
	unsigned int irq;
	unsigned int x_max;
	unsigned int y_max;
	unsigned int pressure_max;
};

static u16 polling_time = S_POLLING_TIME;

static int __devinit zet6221_ts_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit zet6221_ts_remove(struct i2c_client *dev);

static int filterCount = 0; 
static u32 filterX[MAX_FINGER_NUMBER][2], filterY[MAX_FINGER_NUMBER][2]; 

static u8  key_menu_pressed = 0x1;
static u8  key_back_pressed = 0x1;

static u16 ResolutionX=X_MAX;
static u16 ResolutionY=Y_MAX;
static u16 FingerNum=10;
static u16 KeyNum=0;
static int bufLength=0;	

static void ts_early_suspend(struct early_suspend *handler)
{
//	struct zet6221_tsdrv *ts;
//	ts = container_of(handler, struct zet6221_tsdrv, early_suspend);
//	printk(KERN_ERR "ts_early_suspend be called\n");
//	disable_irq(ts->irq);
//	disable_irq(ts->irq2);
//	disable_irq(ts->irq3);
//	mod_timer(&ts->polling_timer,jiffies + msecs_to_jiffies(1000000));
}

static void ts_late_resume(struct early_suspend *handler)
{
//	struct zet6221_tsdrv *ts;
//	ts = container_of(handler, struct zet6221_tsdrv, early_suspend);
//	printk(KERN_ERR "ts_late_resume be called\n");
//	enable_irq(ts->irq);
///	enable_irq(ts->irq2);
//	enable_irq(ts->irq3);
//	mod_timer(&ts->polling_timer,jiffies + msecs_to_jiffies(5000));
}

//Touch Screen
static const struct i2c_device_id zet6221_ts_idtable[] = {
       { ZET_TS_ID_NAME, 0 },
       { }
};

static struct i2c_driver zet6221_ts_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = ZET_TS_ID_NAME,
	},
	.probe	  = zet6221_ts_probe,
	.remove		= __devexit_p(zet6221_ts_remove),
	.id_table = zet6221_ts_idtable,
};

/***********************************************************************
    [function]: 
		        callback: Timer Function if there is no interrupt fuction;
    [parameters]:
			    arg[in]:  arguments;
    [return]:
			    NULL;
************************************************************************/

static void polling_timer_func(unsigned long arg)
{
	struct zet6221_tsdrv *ts = (struct zet6221_tsdrv *)arg;
	schedule_work(&ts->work1);
	mod_timer(&ts->polling_timer,jiffies + msecs_to_jiffies(polling_time));
}

/***********************************************************************
    [function]: 
		        callback: read data by i2c interface;
    [parameters]:
			    client[in]:  struct i2c_client — represent an I2C slave device;
			    data [out]:  data buffer to read;
			    length[in]:  data length to read;
    [return]:
			    Returns negative errno, else the number of messages executed;
************************************************************************/
s32 zet6221_i2c_read_tsdata(struct i2c_client *client, u8 *data, u8 length)
{
	struct i2c_msg msg;
	msg.addr = client->addr;
	msg.flags = I2C_M_RD;
	msg.len = length;
	msg.buf = data;
	return i2c_transfer(client->adapter,&msg, 1);
}

/***********************************************************************
    [function]: 
		        callback: write data by i2c interface;
    [parameters]:
			    client[in]:  struct i2c_client — represent an I2C slave device;
			    data [out]:  data buffer to write;
			    length[in]:  data length to write;
    [return]:
			    Returns negative errno, else the number of messages executed;
************************************************************************/
s32 zet6221_i2c_write_tsdata(struct i2c_client *client, u8 *data, u8 length)
{
	struct i2c_msg msg;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = length;
	msg.buf = data;
	return i2c_transfer(client->adapter,&msg, 1);
}

/***********************************************************************
    [function]: 
		        callback: coordinate traslating;
    [parameters]:
			    px[out]:  value of X axis;
			    py[out]:  value of Y axis;
				p [in]:   pressed of released status of fingers;
    [return]:
			    NULL;
************************************************************************/
void touch_coordinate_traslating(u32 *px, u32 *py, u8 p)
{
	int i;
	u8 pressure;

#if DEBUG_DIY
	printk("%s \n", __func__);
#endif
	#if ORIGIN == TOPRIGHT
	for(i=0;i<MAX_FINGER_NUMBER;i++){
		pressure = (p >> (MAX_FINGER_NUMBER-i-1)) & 0x1;
		if(pressure)
		{
			px[i] = X_MAX - px[i];
		}
	}
	#elif ORIGIN == BOTTOMRIGHT
	for(i=0;i<MAX_FINGER_NUMBER;i++){
		pressure = (p >> (MAX_FINGER_NUMBER-i-1)) & 0x1;
		if(pressure)
		{
			px[i] = X_MAX - px[i];
			py[i] = Y_MAX - py[i];
		}
	}
	#elif ORIGIN == BOTTOMLEFT
	for(i=0;i<MAX_FINGER_NUMBER;i++){
		pressure = (p >> (MAX_FINGER_NUMBER-i-1)) & 0x1;
		if(pressure)
		{
			py[i] = Y_MAX - py[i];
		}
	}
	#endif
}

/***********************************************************************
    [function]: 
		        callback: read finger information from TP;
    [parameters]:
    			client[in]:  struct i2c_client — represent an I2C slave device;
			    x[out]:  values of X axis;
			    y[out]:  values of Y axis;
			    z[out]:  values of Z axis;
				pr[out]:  pressed of released status of fingers;
				ky[out]:  pressed of released status of keys;
    [return]:
			    Packet ID;
************************************************************************/
u8 zet6221_ts_get_xy_from_panel(struct i2c_client *client, u32 *x, u32 *y, u32 *z, u32 *pr, u32 *ky)
{
	u8  ts_data[70];
	int ret;
	int i;
	int j = 0;
	
	memset(ts_data,0,70);

	ret=zet6221_i2c_read_tsdata(client, ts_data, bufLength);

#if 0
	for(j = 0; j < bufLength; j++)
	{
		printk("j:%d v:%x,  ", j, ts_data[j]);
	}
	printk("\n");
#endif
	
	*pr = ts_data[1];
	*pr = (*pr << 8) | ts_data[2];
		
	for(i=0;i<FingerNum;i++)
	{
		x[i]=(u8)((ts_data[3+4*i])>>4)*256 + (u8)ts_data[(3+4*i)+1];
		y[i]=(u8)((ts_data[3+4*i]) & 0x0f)*256 + (u8)ts_data[(3+4*i)+2];
#if 1
		y[i] = Y_MAX - y[i];
#endif
		z[i]=(u8)((ts_data[(3+4*i)+3]) & 0x0f);
	}
		
	//if key enable
	if(KeyNum > 0)
		ky = ts_data[3+4*FingerNum];

	return ts_data[0];
}

/***********************************************************************
    [function]: 
		        callback: get dynamic report information;
    [parameters]:
    			client[in]:  struct i2c_client — represent an I2C slave device;

    [return]:
			    1;
************************************************************************/
u8 zet6221_ts_get_report_mode(struct i2c_client *client)
{
	u8 ts_report_cmd[1] = {178};
	u8 ts_reset_cmd[1] = {176};
	u8 ts_in_data[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	
	int ret;

	ret=zet6221_i2c_write_tsdata(client, ts_report_cmd, 1);

	if (ret > 0)
	{
		while(1)
		{
			udelay(1);

			if (gpio_get_value(GPIO_INT) == 0)
			{
				DPRINTK( "int low\n");
				ret=zet6221_i2c_read_tsdata(client, ts_in_data, 17);

				ResolutionX = ts_in_data[9] & 0xff;
				ResolutionX = (ResolutionX << 8)|(ts_in_data[8] & 0xff);
				ResolutionY = ts_in_data[11] & 0xff;
				ResolutionY = (ResolutionY << 8) | (ts_in_data[10] & 0xff);
				FingerNum = (ts_in_data[15] & 0x7f);
				KeyNum = (ts_in_data[15] & 0x80);

				if(KeyNum==0)
					bufLength  = 3+4*FingerNum;
				else
					bufLength  = 3+4*FingerNum+1;

				//DPRINTK( "bufLength=%d\n",bufLength);
				
				break;
				

			}/*else
				DPRINTK( "int high\n");*/
		}

	}
	return 1;
}

/***********************************************************************
    [function]: 
		        callback: interrupt function;
    [parameters]:
    			irq[in]:  irq value;
    			dev_id[in]: dev_id;

    [return]:
			    NULL;
************************************************************************/
static irqreturn_t zet6221_ts_interrupt(int irq, void *dev_id)
{
	//polling_time	= D_POLLING_TIME;

	/*if (finger_mode > 0)
	{*/
#if DEBUG_DIY
		printk("%s \n",__func__);
#endif
		if (gpio_get_value(GPIO_INT) == 0)
		{
			/* IRQ is triggered by FALLING code here */
			struct zet6221_tsdrv *ts_drv = dev_id;
			schedule_work(&ts_drv->work1);
			//DPRINTK("TS1_INT_GPIO falling\n");
		}else
		{
		//	DPRINTK("TS1_INT_GPIO raising\n");
		}
	//}

	return IRQ_HANDLED;
}

/***********************************************************************
    [function]: 
		        callback: touch information handler;
    [parameters]:
    			_work[in]:  struct work_struct;

    [return]:
			    NULL;
************************************************************************/
static void zet6221_ts_work(struct work_struct *_work)
{
	u32 x[MAX_FINGER_NUMBER], y[MAX_FINGER_NUMBER], z[MAX_FINGER_NUMBER], pr, ky, points;
	u32 px,py,pz;
	u8 ret;
	//u8 pressure;
	int pressure;
	int i,j;
	int count = 0;


#if DEBUG_DIY
	printk("%s \n", __func__);
#endif
	if (bufLength == 0)
	{
		return;
	}

	if (gpio_get_value(GPIO_INT) != 0)
	{
		/* do not read when IRQ is triggered by RASING*/
		//DPRINTK("INT HIGH\n");
		return;
	}

	struct zet6221_tsdrv *ts =
		container_of(_work, struct zet6221_tsdrv, work1);

	struct i2c_client *tsclient1 = ts->i2c_ts;

	ret = zet6221_ts_get_xy_from_panel(tsclient1, x, y, z, &pr, &ky);

	if(ret == 0x3C)
	{

#if DEBUG_DIY
		DPRINTK( "x1= %d, y1= %d x2= %d, y2= %d [PR] = %d [KY] = %d\n", x[0], y[0], x[1], y[1], pr, ky);
#endif
		
		points = pr;

		count = 0;
		for(j = 0; points; points >>= 1)
		{
			count += points & 1;
		}
#if DEBUG_DIY_2
		printk("count: %d  \n", count);
#endif
		points = pr;
		
		#if defined(TRANSLATE_ENABLE)
		touch_coordinate_traslating(x, y, points);
		#endif

		for(i=0;i<FingerNum;i++){
			pressure = (points >> (MAX_FINGER_NUMBER-i-1)) & 0x1;
#if DEBUG_DIY
			DPRINTK( "valid=%d pressure[%d]= %d x= %d y= %d\n",points , i, pressure,x[i],y[i]);
#endif

			if(pressure)
			{
				px = x[i];
				py = y[i];
				pz = z[i];

#if DEBUG_DIY
			DPRINTK( "valid=%d pressure[%d]= %d x= %d y= %d\n",points , i, pressure,x[i],y[i]);
#endif
				bePressed[i] = 1;
				g_bePressed += 1;
				input_report_key(ts->input, BTN_TOUCH, 1);
				input_report_abs(ts->input, ABS_MT_TRACKING_ID, i);
		    	input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 1);
		    	input_report_abs(ts->input, ABS_MT_POSITION_X, px);
		   		input_report_abs(ts->input, ABS_MT_POSITION_Y, py);
		    	input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 1);
		   		input_mt_sync(ts->input);
#if DEBUG_DIY_2
				printk("press sync px:%d  py:%d \n", px, py);
#endif

			}
			//else if(x[i] > 0 || y[i] > 0)
			//else if((bePressed[i] ==1) && (x[i] > 0 || y[i] != Y_MAX)) {
			else if((bePressed[i] ==1) ) {
				bePressed[i] = 0;
				g_bePressed--;
				input_report_key(ts->input, BTN_TOUCH, 0);
				input_report_abs(ts->input, ABS_MT_TRACKING_ID, i);
				input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 0);
		    	input_report_abs(ts->input, ABS_MT_POSITION_X, x[i]);
		   		input_report_abs(ts->input, ABS_MT_POSITION_Y, y[i]);
		    	input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 0);
				input_mt_sync(ts->input);
#if DEBUG_DIY_2
				printk("release sync i:%d  x[i]:%d  y[i]:%d \n",i, x[i],y[i]);
#endif
			}
		}
#if 1
			if(count == 0)
			{
#if DEBUG_DIY_2
				printk("release count:%d bePressed:%d  \n",count, g_bePressed);
#endif
				g_bePressed = 0;
				input_report_key(ts->input, BTN_TOUCH, 0);
				input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, 0);
				input_mt_sync(ts->input);
			}
#endif

			input_sync(ts->input);		

	}

}

static int __devinit zet6221_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int result;
	struct input_dev *input_dev;
	struct zet6221_tsdrv *zet6221_ts;

	DPRINTK( "[TS] zet6221_ts_probe \n");
	zet6221_ts = kzalloc(sizeof(struct zet6221_tsdrv), GFP_KERNEL);
	zet6221_ts->i2c_ts = client;
	zet6221_ts->gpio = GPIO_INT; /*s3c6410*/
	//zet6221_ts->gpio = TS1_INT_GPIO;
	
	i2c_set_clientdata(client, zet6221_ts);

	client->driver = &zet6221_ts_driver;

	INIT_WORK(&zet6221_ts->work1, zet6221_ts_work);

	input_dev = input_allocate_device();
	if (!input_dev || !zet6221_ts) {
		result = -ENOMEM;
		goto fail_alloc_mem;
	}
	
	i2c_set_clientdata(client, zet6221_ts);

	input_dev->name = MJ5_TS_NAME;
	//input_dev->phys = "zet6221_touch/input0";
	//input_dev->id.bustype = BUS_HOST;
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0xABCD;
	input_dev->dev.parent = &client->dev;
//	input_dev->id.product = 0x0002;
//	input_dev->id.version = 0x0100;

#if defined(TPINFO)
	udelay(100);
	zet6221_ts_get_report_mode(client);
#else
	ResolutionX = X_MAX;
	ResolutionY = Y_MAX;
	FingerNum = FINGER_NUMBER;
	KeyNum = KEY_NUMBER;
	if(KeyNum==0)
		bufLength  = 3+4*FingerNum;
	else
		bufLength  = 3+4*FingerNum+1;
#endif

	DPRINTK( "ResolutionX=%d ResolutionY=%d FingerNum=%d KeyNum=%d\n",ResolutionX,ResolutionY,FingerNum,KeyNum);

#if 0
        set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit); 
        set_bit(ABS_MT_POSITION_X, input_dev->absbit); 
        set_bit(ABS_MT_POSITION_Y, input_dev->absbit); 
        //set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit); 
        set_bit(ABS_MT_TRACKING_ID, input_dev->absbit); 
	set_bit(BTN_TOUCH, input_dev->keybit);
#endif

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 8, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, X_MIN, X_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, Y_MIN, Y_MAX, 0, 0);

#if 0
	set_bit(KEY_BACK, input_dev->keybit);
	set_bit(KEY_HOME, input_dev->keybit);
	set_bit(KEY_MENU, input_dev->keybit);
#endif

	//input_dev->evbit[0] = BIT(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) | BIT_MASK(BTN_2);

	result = input_register_device(input_dev);
	if (result)
		goto fail_ip_reg;

	zet6221_ts->input = input_dev;

	input_set_drvdata(zet6221_ts->input, zet6221_ts);

	//setup_timer(&zet6221_ts->polling_timer, polling_timer_func, (unsigned long)zet6221_ts);
	//mod_timer(&zet6221_ts->polling_timer,jiffies + msecs_to_jiffies(200));
	
	
	//s3c6410
	//result = gpio_request(zet6221_ts->gpio, "GPN"); 
	result = gpio_request(zet6221_ts->gpio, "GPN"); 
	if (result)
		goto gpio_request_fail;

	//zet6221_ts->irq = gpio_to_irq(zet6221_ts->gpio);
	zet6221_ts->irq = client->irq;
	DPRINTK( "[TS] zet6221_ts_probe.gpid_to_irq [zet6221_ts->irq=%d]\n",zet6221_ts->irq);

    /* set input mode */
    gpio_direction_input(GPIO_INT);
    /* set gpio interrupt #0 source=GPIOD_24, and triggered by falling edge(=1) */
    gpio_enable_edge_int(gpio_to_idx(GPIO_INT), 1, INT_GPIO_0-INT_GPIO_0);

	result = request_irq(zet6221_ts->irq, zet6221_ts_interrupt,IRQF_TRIGGER_FALLING, ZET_TS_ID_NAME, zet6221_ts);
	if (result)
		goto request_irq_fail;

	//AT91SAM9G45
/*	if (at91_set_gpio_input(TS1_INT_GPIO, 0)) {
		printk(KERN_DEBUG "Cannot set pin %i for GPIO input.\n", TS1_INT_GPIO);
	}
	
	if (at91_set_deglitch(TS1_INT_GPIO, 1)) {
		printk(KERN_DEBUG "Cannot set pin %i for GPIO deglitch.\n", TS1_INT_GPIO);
	}
	
	result = request_irq (TS1_INT_GPIO, (void *)zet6221_ts_interrupt, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, ZET_TS_ID_NAME, zet6221_ts);
	if (result < 0)
		goto request_irq_fail;
*/



/*	zet6221_ts->early_suspend.suspend = ts_early_suspend,
	zet6221_ts->early_suspend.resume = ts_late_resume,
	zet6221_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 2,
	register_early_suspend(&zet6221_ts->early_suspend);
	disable_irq(zet6221_ts->irq);
*/

	printk("success! \n");
	return 0;

request_irq_fail:
	gpio_free(zet6221_ts->gpio);
gpio_request_fail:
	free_irq(zet6221_ts->irq, zet6221_ts);
	input_unregister_device(input_dev);
	input_dev = NULL;
fail_ip_reg:
fail_alloc_mem:
	input_free_device(input_dev);
	kfree(zet6221_ts);
	printk("false ! \n");
	return result;
}

static int __devexit zet6221_ts_remove(struct i2c_client *dev)
{
	struct zet6221_tsdrv *zet6221_ts = i2c_get_clientdata(dev);

	free_irq(zet6221_ts->irq, zet6221_ts);
	gpio_free(zet6221_ts->gpio);
	del_timer_sync(&zet6221_ts->polling_timer);
	input_unregister_device(zet6221_ts->input);
	kfree(zet6221_ts);

	return 0;
}
static int __init zet6221_ts_init(void)
{
#if 0
	set_gpio_val(GPIOD_bank_bit2_24(23), GPIOD_bit_bit2_24(23), 0);
    set_gpio_mode(GPIOD_bank_bit2_24(23), GPIOD_bit_bit2_24(23), GPIO_OUTPUT_MODE);
	mdelay(20);
   	set_gpio_val(GPIOD_bank_bit2_24(23), GPIOD_bit_bit2_24(23), 1);
    set_gpio_mode(GPIOD_bank_bit2_24(23), GPIOD_bit_bit2_24(23), GPIO_OUTPUT_MODE);
#else
	
	gpio_direction_output(GPIO_POWER_EN, 0);
	mdelay(1);
	gpio_direction_output(GPIO_POWER_EN, 1);
	mdelay(10);

	gpio_direction_output(GPIO_RST, 1);
	mdelay(10);
	gpio_direction_output(GPIO_RST, 0);
	mdelay(10);
	gpio_direction_output(GPIO_RST, 1);
#endif
	
	i2c_add_driver(&zet6221_ts_driver);
	return 0;
}
module_init(zet6221_ts_init);

static void __exit zet6221_ts_exit(void)
{
    i2c_del_driver(&zet6221_ts_driver);
}
module_exit(zet6221_ts_exit);

void zet6221_set_ts_mode(u8 mode)
{
	DPRINTK( "[Touch Screen]ts mode = %d \n", mode);
}
EXPORT_SYMBOL_GPL(zet6221_set_ts_mode);


MODULE_DESCRIPTION("ZET6221 I2C Touch Screen driver");
MODULE_LICENSE("GPL v2");
