#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <mach/gpio.h>
#include <linux/irq.h>
//#include <mach/board.h>
//#include <mach/iomux.h>
#include <linux/slab.h>
//#include <linux/input/mt.h>

#include "calibration_ts.h" 
#include "EVR_setting_table.h"

//#define CALIBRATION
//#define FW_GESTURE	//for two points or only gesture
#define CONFIG_MACH_M723HR
#define TOUCH_POWER_PIN 
#define UPDATE_INT

#ifndef UPDATE_INT
	//#include "bu21020_ver33_f012_0_00_Ges2.h"
	#include "bu21023_FF_1022_chat.h"
#else
#ifdef FW_GESTURE
	//#include "bu21023_ver33_y007_4_11_CAF.h"
	#include "bu21020_33_y109_3.h"
#else
	#include "bu21023_ver33_y108_4_11_CAF_merge.h"
	//#include "bu21020_33_y109_3.h"
#endif
#endif

#if defined(CONFIG_MACH_A8)
	//#define SW_AVG 		1 //20110729, tracy
	#define TWO_COUNT 12	//15
	//#define FILTER_1	1 //20110715, tracy
	#define FILTER_2	1 //20110715, tracy	
#else
#ifdef FW_GESTURE
	#define TWO_COUNT 12	//15
	//#define SW_AVG 		1 //20110729, tracy
	//#define STEP2SMALL	1//20110729, tracy
	//#define STEP2BIG	1//20110729, tracy
	//#define FILTER_1	1 //20110715, tracy
	//#define FILTER_2	1 //20110715, tracy	
#else
	#define TWO_COUNT 15	//12
	//#define SW_AVG 		1 //20110729, tracy
	//#define STEP2SMALL	1//20110729, tracy
	//#define STEP2BIG	1//20110729, tracy
	//#define FILTER_1	1 //20110715, tracy
	#define FILTER_2	1 //20110715, tracy	
#endif
#endif

#define DAC_LEGTH 10	//20110816, tracy
#define CALIB_LEGTH 10 //20110817, tracy

//#define TWO_COUNT 6	//15
#define ONE_COUNT 10	//10
#define KEY_COUNT 10
#define DELTA_X 100
#define DELTA_Y 160
#define STEP_DIFF 0x10


#ifdef SW_AVG
/* Size of Moving Average buffer array */
#define BUFF_ARRAY  16

#define DISTANCE_X 32
#define DISTANCE_Y 32
#define AVG_BOUND_X 16
#define AVG_BOUND_Y 16
#define S_DISTANCE_X 6
#define S_DISTANCE_Y 6
#define S_AVG_BOUND_X 4
#define S_AVG_BOUND_Y 4

/* skip times of moving average when touch starting */
#define SKIP_AVG_1 5
#define SKIP_AVG_2 5
#endif


struct touch_point
{
    unsigned int x;
    unsigned int y;

#ifdef SW_AVG
    unsigned int buff_x[BUFF_ARRAY];
    unsigned int buff_y[BUFF_ARRAY];
    unsigned char buff_cnt;
    /* Previous coordinate of touch point after moving average calculating */
    unsigned int old_avg_x;
    unsigned int old_avg_y;
#endif

};

struct touch_point tp1, tp2;

static u8 calib_finish = 0;
static u8 dac_finish = 0;
static u8	EVR_X_NO;
static u8	EVR_Y_NO;
static u8	STEP_X;
static u8	STEP_Y;
static u8 finger_count_0 = 0;	//from 0 to 1
static u8 finger_count_1 = 0;	//from 2 to 1
static u8 finger_count_2 = 0;	//from 0/1 to 2
static u8 key_count = 0;
static u8 error_flg_1 = 0;
static u8 error_flg_2 = 0;
static u8 finger =0;
static u16 x1_old    =0;
static u16 y1_old    =0;
static u16 x2_old    =0;
static u16 y2_old    =0;

/* Print the coordinate of touch point in the debug console */

volatile struct adc_point gADPoint;


#define BU21020_DEBUG
#ifdef BU21020_DEBUG
#define DBG(fmt, args...)	printk("*** " fmt, ## args)
#else
#define DBG(fmt, args...)	do{}while(0)
#endif

//#define EV_MENU					KEY_F1

#define BU21020_SPEED 100*1000
#define MAX_POINT  2

#if defined (CONFIG_TOUCHSCREEN_800X600)
#define SCREEN_MAX_X 800
#define SCREEN_MAX_Y 600
#elif defined (CONFIG_TOUCHSCREEN_800X480)
#define SCREEN_MAX_X 800
#define SCREEN_MAX_Y 480
#endif

#define PRESS_MAX 200
#define MULTI_TOUCH 1

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend bu21020_early_suspend;
#endif

#define AML_TP_PORTING
#ifdef AML_TP_PORTING
#define SCREEN_MAX_X 980
#define SCREEN_MIN_X 50
#define SCREEN_MAX_Y 920
#define SCREEN_MIN_Y 130

#define AML_INT_GPIO ((GPIOA_bank_bit0_27(16)<<16) | GPIOA_bit_bit0_27(16))
#define AML_TOUCH_EN_GPIO ((GPIOC_bank_bit0_15(3)<<16) | GPIOC_bit_bit0_15(3))
#endif

static int  bu21020_probe(struct i2c_client *client, const struct i2c_device_id *id);
static void bu21020_tpscan_timer(unsigned long data);

struct ts_event {
	u16	x1;
	u16	y1;
	u16	x2;
	u16	y2;
	u16	x3;
	u16	y3;
	u16	x4;
	u16	y4;
	u16	x5;
	u16	y5;
	u16	pressure;
	u16	w;
    u8  touch_point;
};

struct bu21020_data
{
	struct i2c_client *client;
	struct input_dev	*input_dev;
	spinlock_t 	lock;
	int			irq;
	int     int_gpio;
	int		reset_gpio;
	int		touch_en_gpio;
	int		last_points;
	struct ts_event		event;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;
//	struct hrtimer timer;
	struct timer_list timer;
};

struct i2c_client *g_client;


#ifdef SW_AVG

//-----------------------------------------------------------------------------
//
//Coord_Avg
//
//-----------------------------------------------------------------------------
static void Coord_Avg (struct touch_point *tp)
{

	unsigned long temp_x = 0, temp_y = 0, temp_n = 0;
	unsigned int i;
	////////////////////////////////////////
	// X1,Y1 Moving Avg
	////////////////////////////////////////
	if((tp->x != 0) && (tp->y != 0))
	{			               			  			      				
		if(tp->buff_cnt >= SKIP_AVG_1)
		{  
		
		    if(((abs(tp->buff_x[0] - tp->x) > DISTANCE_X) && (abs(tp->buff_y[0] - tp->y) > DISTANCE_Y)) ||
		       ((abs(tp->buff_x[0] - tp->x) > S_DISTANCE_X) && (abs(tp->buff_y[0] - tp->y) < S_DISTANCE_Y)) ||
			   ((abs(tp->buff_x[0] - tp->x) < S_DISTANCE_X) && (abs(tp->buff_y[0] - tp->y) > S_DISTANCE_Y)) ||			   
			   (((tp->old_avg_x != 0) && (abs(tp->old_avg_x - tp->x) > AVG_BOUND_X)) && 
			   ( (tp->old_avg_y != 0) && (abs(tp->old_avg_y - tp->y) > AVG_BOUND_Y))) ||
			   (((tp->old_avg_x != 0) && (abs(tp->old_avg_x - tp->x) > S_AVG_BOUND_X)) && 			
			   ( (tp->old_avg_y != 0) && (abs(tp->old_avg_y - tp->y) < S_AVG_BOUND_Y)))||
			   (((tp->old_avg_x != 0) && (abs(tp->old_avg_x - tp->x) < S_AVG_BOUND_X)) && 			
			   ( (tp->old_avg_y != 0) && (abs(tp->old_avg_y - tp->y) > S_AVG_BOUND_Y))))
			{
				for (i = 0; i < tp->buff_cnt; i++)
				{
					tp->buff_x[tp->buff_cnt - i] = tp->buff_x[tp->buff_cnt - i - 1];
					tp->buff_y[tp->buff_cnt - i] = tp->buff_y[tp->buff_cnt - i - 1];
				}
				tp->buff_x[0] = tp->x;
				tp->buff_y[0] = tp->y;
 
				temp_x = 0; temp_y = 0; temp_n = 0;
        
				for (i = 0; i <= tp->buff_cnt; i++)
				{
					temp_x += ((unsigned long) (tp->buff_x[i] * (tp->buff_cnt - i + 1)));
					temp_y += ((unsigned long) (tp->buff_y[i] * (tp->buff_cnt - i + 1)));
					temp_n += (unsigned long) (tp->buff_cnt - i + 1);
				}            
				tp->x = temp_x / temp_n;
				tp->y = temp_y / temp_n;
		
				tp->old_avg_x = tp->x;
				tp->old_avg_y = tp->y;  	
				if(tp->buff_cnt < (BUFF_ARRAY-1))
					tp->buff_cnt++;
			}
			else 
			{	  
				tp->x = tp->old_avg_x;
				tp->y = tp->old_avg_y;	
			} 
		}
		else
		{
			for (i = 0; i < tp->buff_cnt; i++)
			{
				tp->buff_x[tp->buff_cnt - i] = tp->buff_x[tp->buff_cnt - i - 1];
				tp->buff_y[tp->buff_cnt - i] = tp->buff_y[tp->buff_cnt - i - 1];
			}	
			tp->buff_x[0] = tp->x;
			tp->buff_y[0] = tp->y;
			if(tp->buff_cnt < (BUFF_ARRAY-1))
				tp->buff_cnt++;
			tp->old_avg_x = tp->x;
			tp->old_avg_y = tp->y;
			tp->x = 0;
			tp->y = 0;			

		}
	}//End/ of "if((x1 != 0) && (y1 != 0))"
	else 
	{
		tp->buff_cnt = 0;
		if((tp->buff_x[0] != 0) && (tp->buff_y[0] != 0))
		{
			tp->x = tp->buff_x[0];
			tp->y = tp->buff_y[0];
		}
		else
		{
			tp->x = 0;
			tp->y = 0;
		}
		tp->buff_x[0] = 0;
		tp->buff_y[0] = 0;
		tp->old_avg_x = 0;
		tp->old_avg_y = 0;
	}


}
#endif


int bu21020_rx_data(struct i2c_client *client, char *rxData, int length)
{
#ifdef AML_TP_PORTING
	int ret=-1;
	struct i2c_msg msgs[2] = {
		[0] = {
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.len = 1,
			.buf = &rxData[0]
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = &rxData[0]
		},
	};

	ret=i2c_transfer(client->adapter,msgs,2);
	return ret;
#else
	int ret = 0;
	char reg = rxData[0];
	ret = i2c_master_reg8_recv(client, reg, rxData, length, BU21020_SPEED);
	return (ret > 0)? 0 : ret;
#endif
}

static int bu21020_tx_data(struct i2c_client *client, char *txData, int length)
{
#ifdef AML_TP_PORTING
    struct i2c_msg msg[2] = {
        [0] = {
            .addr = client->addr,
            .flags = client->flags,
            .len = length,
            .buf = &txData[0]
        },
        [1] = {
            .addr = client->addr,
            .flags = client->flags | I2C_M_NOSTART,
            .len = length-1,
            .buf = &txData[1]
        },
    };
     return i2c_transfer(client->adapter, &msg[0], 1);
#else
	int ret = 0;
	char reg = txData[0];
	ret = i2c_master_reg8_send(client, reg, &txData[1], length-1, BU21020_SPEED);
	return (ret > 0)? 0 : ret;
#endif
}

char bu21020_read_reg(struct i2c_client *client, int addr)
{
	char tmp;
	int ret = 0;

	tmp = addr;
	ret = bu21020_rx_data(client, &tmp, 1);
	if (ret < 0) {
		return ret;
	}
	return tmp;
}

int bu21020_write_reg(struct i2c_client *client,int addr,int value)
{
	char buffer[3];
	int ret = 0;

	buffer[0] = addr;
	buffer[1] = value;
	ret = bu21020_tx_data(client, &buffer[0], 2);
	return ret;
}

static void bu21020_power_en(struct bu21020_data *bu21020, int on)
{
#if defined (TOUCH_POWER_PIN)
	if (on) {
		gpio_direction_output(bu21020->touch_en_gpio, 1);
		gpio_set_value(bu21020->touch_en_gpio, 1);
		mdelay(10);
	} else {
		gpio_direction_output(bu21020->touch_en_gpio, 0);
		gpio_set_value(bu21020->touch_en_gpio, 0);
		mdelay(10);
	}
#endif
}

static void bu21020_chip_reset(struct bu21020_data *bu21020)
{
    gpio_direction_output(bu21020->reset_gpio, 0);
    gpio_set_value(bu21020->reset_gpio, 0);
	mdelay(20);
    gpio_set_value(bu21020->reset_gpio, 1);
}

static int i2c_write_interface(unsigned char* pbt_buf, int dw_lenth)
{
    int ret;
    ret = i2c_master_send(g_client, pbt_buf, dw_lenth);
    if (ret <= 0) {
        DBG("i2c_write_interface error\n");
        return -1;
    }

    return 0;
}

static int ft_cmd_write(unsigned char btcmd, unsigned char btPara1, unsigned char btPara2,
		unsigned char btPara3, int num)
{
    unsigned char write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(write_cmd, num);
}


static int bu21020_check_calib(struct i2c_client * client)
{
	unsigned char ret;
	int i; 
	DBG("************Calibration Check*************\n");	
 	bu21020_write_reg(client, 0x3D, 0x00);
	for(i= 0; i<CALIB_LEGTH; i++)
	{
		bu21020_write_reg(client, 0x42, 0x00);
		mdelay(1);
		bu21020_write_reg(client, 0x42, 0x01);
		mdelay(30);
		

		DBG(" addr 0x2B=0x%x \n",bu21020_read_reg(client, 0x2B));	
		DBG(" addr 0x18=0x%x \n",bu21020_read_reg(client, 0x18));
	
		ret= bu21020_read_reg(client, 0x2A);
		DBG(" addr 0x2A=0x%x \n",ret);

		if(ret& 0x02)
		{
			calib_finish = 1;
			break;
		}
	}
	bu21020_write_reg(client, 0x42, 0x00);
	bu21020_write_reg(client, 0x30, 0x46);
	DBG(" addr 0x2A=0x%x , i=%ld\n",bu21020_read_reg(client, 0x2A),i);	
	DBG(" addr 0x2B=0x%x \n",bu21020_read_reg(client, 0x2B));	
	DBG(" addr 0x30=0x%x \n",bu21020_read_reg(client, 0x30));
	bu21020_write_reg(client, 0x3D, 0xFE);
	
	return 0; 
}


static int bu21020_check_dac(struct i2c_client * client, unsigned char reg_test1, unsigned char evr_no, 
		unsigned char step_value)
{
	
	unsigned char dac_flag = 0;
	unsigned char dac_value;
	unsigned char i;
	unsigned char evr_no_new;
	
	DBG("************DAC Check*************\n");
		//mdelay(100);
	bu21020_write_reg(client, 0x65, reg_test1);		
	bu21020_write_reg(client, 0x3D, 0x00);
	for(i=0; i<(DAC_LEGTH*2 +1); i++)
	{
		mdelay(30);//wait for 5 sampling period
		dac_value = bu21020_read_reg(client, 0x6B);
		DBG("reg_test1= 0x%x, evr = 0x%x, dac_value = 0x%x, 0x6C = 0x%x\n",
				reg_test1,bu21020_read_reg(client, 0x64), dac_value,bu21020_read_reg(client, 0x6C));
		if ( (dac_value == 0xC0) || ( dac_value==0xC1))
			dac_flag ++;
		else
		{
			dac_flag = 0;
			break;
		}
		
		if(evr_no < DAC_LEGTH )
		   evr_no_new = i+1;
		else if(evr_no >(254- DAC_LEGTH)) 
		   evr_no_new = ( 255- 2*DAC_LEGTH + i);
		else if(i%2)
		   evr_no_new = evr_no + i/2+1;
		else 
		   evr_no_new = evr_no - i/2-1;
		   
		if(reg_test1 == 0x01)
			bu21020_write_reg(client, 0x63, EVR_REG[evr_no_new]);	
		else if (reg_test1 == 0x11)
			bu21020_write_reg(client, 0x64, EVR_REG[evr_no_new]);	
		else
		{
			DBG("axis value error, reg_test1 = 0x%x\n",reg_test1);
			break;
		}
			i2c_smbus_write_byte_data(client, 0x42, 0x00);
			mdelay(1);
			i2c_smbus_write_byte_data(client, 0x42, 0x01);	
		
	}
	
	if(i)
	{
		step_value = step_value * EVR_DATA[evr_no_new] / EVR_DATA[evr_no];
		DBG("evr_no = %d, new_no = %d, EVR = 0x%x,	STEP = 0x%x\n",evr_no, evr_no_new, EVR_REG[evr_no_new], step_value);

		if(reg_test1 == 0x01)
			bu21020_write_reg(client, 0x34, step_value);	
		else if (reg_test1 == 0x11)
			bu21020_write_reg(client, 0x35, step_value);	
		else		
			DBG("axis value error, reg_test1 = 0x%x\n",reg_test1);

	}
	bu21020_write_reg(client, 0x3D, 0xFE);
	return dac_flag;
}


static int bu21020_chip_init(struct i2c_client * client)
{
	int ret = 0;
	u8 r_value;
	int i;	
	struct bu21020_data *bu21020 = i2c_get_clientdata(client);
	
	gADPoint.x = 0;
	gADPoint.y = 0;

	bu21020_power_en(bu21020, 0);
	mdelay(20);
    	//gpio_direction_output(bu21020->reset_gpio, 0);
    	//gpio_set_value(bu21020->reset_gpio, 1);
	mdelay(20);
	bu21020_power_en(bu21020, 1);
	mdelay(20);
	//bu21020_chip_reset(bu21020);
	mdelay(2000);

	//analog power on
	bu21020_write_reg(client, 0x40, 0x01);
	// Wait 100usec for Power-on
	udelay(200);

	//bu21020_write_reg(client, 0x3D, 0xFF);

	// Init BU21020
	//common setting	
	bu21020_write_reg(client, 0x30, 0x06);
	//bu21020_write_reg(client, 0x31, 0x1d);
	bu21020_write_reg(client, 0x31, 0x11);
	bu21020_write_reg(client, 0x32, 0x46);      /*   */
	//bu21020_write_reg(client, 0x32, 0x6E);      /*   */
	DBG("aaa  0x30  0x46  0x%x \n",bu21020_read_reg(client, 0x30));
	DBG("bbb  0x31, 0x05  0x%x \n",bu21020_read_reg(client, 0x31));
	DBG("ccc  0x32, 0x66  0x%x \n",bu21020_read_reg(client, 0x32));

	//timing setting
	bu21020_write_reg(client, 0x33, 0x00);	
	//bu21020_write_reg(client, 0x33, 0x05);	
	bu21020_write_reg(client, 0x50, 0x00);
	bu21020_write_reg(client, 0x60, 0x08);
	//bu21020_write_reg(client, 0x61, 0x0a);
	bu21020_write_reg(client, 0x61, 0x14);
	bu21020_write_reg(client, 0x57, 0x04);

	//panel setting
#if 0
    /* for romo */
	bu21020_write_reg(client, 0x63, 0xDE);
	bu21020_write_reg(client, 0x64, 0xDC);
	bu21020_write_reg(client, 0x34, 0x60); 
	bu21020_write_reg(client, 0x35, 0xAB); 
	bu21020_write_reg(client, 0x3A, 0x40);
	bu21020_write_reg(client, 0x3B, 0x60);
	bu21020_write_reg(client, 0x36, 0x04);
	bu21020_write_reg(client, 0x37, 0x04);
#endif

#if defined (CONFIG_MACH_A7HC)
    /* for A7HCR */
	bu21020_write_reg(client, 0x63, 0x56);
	bu21020_write_reg(client, 0x64, 0x9C);
	bu21020_write_reg(client, 0x34, 0x60); 
	bu21020_write_reg(client, 0x35, 0xA0); 

	bu21020_write_reg(client, 0x3A, 0x40);
	bu21020_write_reg(client, 0x36, 0x08);
	bu21020_write_reg(client, 0x37, 0x04);
	
    /* for A7HT3R */
#elif defined(CONFIG_MACH_A7HTC) || defined(CONFIG_MACH_A70HT3R) || defined(CONFIG_MACH_A7ECHR)            /* for ZhiHong TP */
	DBG("-----------For A7HT3R ZH TP!---------\n");
	EVR_X_NO = 145; //0x76
	EVR_Y_NO = 190; //0x22
	STEP_X = 0x75;
	STEP_Y = 0x9A;
	bu21020_write_reg(client, 0x63, EVR_REG[EVR_X_NO]);
	bu21020_write_reg(client, 0x64, EVR_REG[EVR_Y_NO]);
	bu21020_write_reg(client, 0x34, STEP_X); 
	bu21020_write_reg(client, 0x35, STEP_Y); 

	bu21020_write_reg(client, 0x3A, 0x40);
	bu21020_write_reg(client, 0x36, 0x08);
	bu21020_write_reg(client, 0x37, 0x04);
#elif defined(CONFIG_MACH_M722HR) || defined(CONFIG_MACH_M723HR)	
	DBG("-----------For M722!---------\n");
	EVR_X_NO = 145; //0xF6:140  b6:146  56:151 76:145
	EVR_Y_NO = 206; //0xDC:195  d4:211  9c:200 2c:206
	STEP_X = 0x4a;
	STEP_Y = 0xba;
	bu21020_write_reg(client, 0x63, EVR_REG[EVR_X_NO]);
	bu21020_write_reg(client, 0x64, EVR_REG[EVR_Y_NO]);
	bu21020_write_reg(client, 0x34, STEP_X); 
	bu21020_write_reg(client, 0x35, STEP_Y); 

#ifdef FW_GESTURE
	bu21020_write_reg(client, 0x3A, 0x4c);
	bu21020_write_reg(client, 0x38, 0x0c);
	bu21020_write_reg(client, 0x39, 0x0c);
#else
	bu21020_write_reg(client, 0x3A, 0x6C);
	bu21020_write_reg(client, 0x36, 0x0c);
	bu21020_write_reg(client, 0x37, 0x0c);
	bu21020_write_reg(client, 0x38, 0x0c);
	bu21020_write_reg(client, 0x39, 0x0c);
#endif	
    /* for ping po1 */
#elif defined(CONFIG_MACH_A7HTC_PP)                /* for PingPo TP */
	DBG("-----------For A7HT3R PingPo TP!---------\n");
	EVR_X_NO = 153; //0x66
	EVR_Y_NO = 203; //0xCC
	STEP_X = 0x45;
	STEP_Y = 0x80;
	bu21020_write_reg(client, 0x63, EVR_REG[EVR_X_NO]);
	bu21020_write_reg(client, 0x64, EVR_REG[EVR_Y_NO]);
	bu21020_write_reg(client, 0x34, STEP_X); 
	bu21020_write_reg(client, 0x35, STEP_Y); 

	bu21020_write_reg(client, 0x3A, 0x4C);
	bu21020_write_reg(client, 0x36, 0x08);
	bu21020_write_reg(client, 0x37, 0x04);
	
#elif defined(CONFIG_MACH_A8)
	DBG("-----------For A80HTR TP!---------\n");
	#if 0
	EVR_X_NO = 163; //0xA6
	EVR_Y_NO = 195; //0xE2
	STEP_X = 0x40;
	STEP_Y = 0x80;
	bu21020_write_reg(client, 0x63, EVR_REG[EVR_X_NO]);
	bu21020_write_reg(client, 0x64, EVR_REG[EVR_Y_NO]);
	bu21020_write_reg(client, 0x34, STEP_X); 
	bu21020_write_reg(client, 0x35, STEP_Y); 

	bu21020_write_reg(client, 0x3A, 0x4C);
	bu21020_write_reg(client, 0x36, 0x04);//0CH
	bu21020_write_reg(client, 0x37, 0x04);//08H
	#else
		DBG("-----------For A80HTR Pingbo TP!---------\n");
	EVR_X_NO = 147; //0xD6
	EVR_Y_NO = 193; //0x72
	STEP_X = 0x6F;
	STEP_Y = 0x98;
	bu21020_write_reg(client, 0x63, EVR_REG[EVR_X_NO]);
	bu21020_write_reg(client, 0x64, EVR_REG[EVR_Y_NO]);
	bu21020_write_reg(client, 0x34, STEP_X); 
	bu21020_write_reg(client, 0x35, STEP_Y); 

	bu21020_write_reg(client, 0x3A, 0x4C);
	bu21020_write_reg(client, 0x36, 0x0C);//0CH
	bu21020_write_reg(client, 0x37, 0x10);//08H		
	#endif
#elif defined(CONFIG_MACH_M911)
	DBG("-----------For M911 TP!---------\n");
	//bu21020_write_reg(client, 0x31, 0x05);
	EVR_X_NO = 185; //0x62
	EVR_Y_NO = 159; //0xC6:155  0x46:159
	STEP_X = 0x92;
	STEP_Y = 0x60;//0x6d
	bu21020_write_reg(client, 0x63, EVR_REG[EVR_X_NO]);
	bu21020_write_reg(client, 0x64, EVR_REG[EVR_Y_NO]);
	bu21020_write_reg(client, 0x34, STEP_X); 
	bu21020_write_reg(client, 0x35, STEP_Y); 

	bu21020_write_reg(client, 0x3A, 0x4C);
	bu21020_write_reg(client, 0x36, 0x08);//0CH
	bu21020_write_reg(client, 0x37, 0x08);//08H

#endif

	//fixed value setting
	bu21020_write_reg(client, 0x52, 0x08);
	bu21020_write_reg(client, 0x56, 0x04); //Samuel add for test
	bu21020_write_reg(client, 0x62, 0x0F);
	bu21020_write_reg(client, 0x65, 0x01);

	// Beginning address setting of program memory for host download
	bu21020_write_reg(client, 0x70, 0x00);
	bu21020_write_reg(client, 0x71, 0x00);
	DBG( "Samuel test 6 !\n");
	// Download firmware to BU21020
	DBG("BU21020 firmware download starts!\n");

	for(i = 0; i < CODESIZE; i++)
		bu21020_write_reg(client, 0x72, program[i]); 
	DBG("BU21020 firmware download is completed!\n");
	// Download path setting
	DBG(" addr 0x74=0x%x \n",bu21020_read_reg(client, 0x74));
	DBG(" addr 0x75=0x%x \n",bu21020_read_reg(client, 0x75));
	DBG(" addr 0x76=0x%x \n",bu21020_read_reg(client, 0x76));
	
	// CPU power on
	bu21020_write_reg(client, 0x40, 0x03);

	//bu21020_write_reg(client, 0x3D, 0xFF);
	mdelay(10);	

	DBG(" addr 0x2A=0x%x \n",bu21020_read_reg(client, 0x2A));	
	DBG(" addr 0x2B=0x%x \n",bu21020_read_reg(client, 0x2B));	
	
	// Clear all Interrupt
	bu21020_write_reg(client, 0x3E, 0xFF);

	// Coordinate Offset, MAF, CAF
	bu21020_write_reg(client, 0x60, 0x08);
#ifdef FW_GESTURE
	bu21020_write_reg(client, 0x3B, 0x00);
#else
	bu21020_write_reg(client, 0x3B, 0x05);
#endif

	// Init end
	//
	bu21020_write_reg(client, 0x3E, 0xFF);
	//DBG(" addr 0x2A=0x%x \n",bu21020_read_reg(client, 0x2A));	
	bu21020_write_reg(client, 0x3D, 0xFE);

	DBG(" addr 0x60=0x%x \n",bu21020_read_reg(client, 0x60));
	DBG(" addr 0x61=0x%x \n",bu21020_read_reg(client, 0x61));
	DBG(" addr 0x63=0x%x \n",bu21020_read_reg(client, 0x63));
	DBG(" addr 0x64=0x%x \n",bu21020_read_reg(client, 0x64));
	DBG(" addr 0x34=0x%x \n",bu21020_read_reg(client, 0x34));
	DBG(" addr 0x35=0x%x \n",bu21020_read_reg(client, 0x35));
	DBG(" addr 0x36=0x%x \n",bu21020_read_reg(client, 0x36));
	DBG(" addr 0x37=0x%x \n",bu21020_read_reg(client, 0x37));	
	DBG(" addr 0x3B=0x%x \n",bu21020_read_reg(client, 0x3B));
	
	DBG(" addr 0x10=0x%x \n",bu21020_read_reg(client, 0x10));
	DBG(" addr 0x11=0x%x \n",bu21020_read_reg(client, 0x11));
	DBG(" addr 0x56=0x%x \n",bu21020_read_reg(client, 0x56));
	DBG(" addr 0x3A=0x%x \n",bu21020_read_reg(client, 0x3A));
	DBG(" addr 0x18=0x%x \n",bu21020_read_reg(client, 0x18));

	bu21020_write_reg(client, 0x65, 0x11);
	DBG(" write addr 0x65 to 0x11\n ");
	DBG(" addr 0x65=0x%x \n",bu21020_read_reg(client, 0x65));
	DBG(" addr 0x6B=0x%x \n",bu21020_read_reg(client, 0x6B));
	DBG(" addr 0x6C=0x%x \n",bu21020_read_reg(client, 0x6C));	
	bu21020_write_reg(client, 0x65, 0x01);
	DBG(" write addr 0x65 to 0x1\n ");
	DBG(" addr 0x65=0x%x \n",bu21020_read_reg(client, 0x65));
	DBG(" addr 0x6B=0x%x \n",bu21020_read_reg(client, 0x6B));
	DBG(" addr 0x6C=0x%x \n",bu21020_read_reg(client, 0x6C));	
	DBG(" addr 0x2A=0x%x \n",bu21020_read_reg(client, 0x2A));	
	DBG(" addr 0x2B=0x%x \n",bu21020_read_reg(client, 0x2B));
/*	
	DBG("----Print BU21023 Reg 1 ---\n");	
	for(i=0;i<=0x2B;i++) {
		DBG(" addr 0x%X=0x%X \n", i, bu21020_read_reg(client, i));
		//DBG(" addr 0x18=0x%x \n",bu21020_read_reg(client, 0x18));
	}
	DBG("---Print BU21023 Reg 2 ---\n");	
	for(i=0;i<=0x2B;i++) {
		DBG(" addr 0x%X=0x%X \n", i, bu21020_read_reg(client, i));
		//DBG(" addr 0x18=0x%x \n",bu21020_read_reg(client, 0x18));
	}
*/
#if 0  /* by zqqu read calibration failed */
	r_value = bu21020_read_reg(client, 0x2b);
	DBG("reg 0x2b =%x\n", r_value);

	u8 touch;
	u8 buf[32] = {0};
	if (r_value & 0x02) {
		//bu21020_write_reg(client, 0x3D, 0xFF);
		ret = bu21020_rx_data(client, buf, 9);
		tp1.x = ( (buf[0]<< 2) | buf[1] );
		tp1.y = ( (buf[2]<< 2) | buf[3] );
		tp2.x = ( (buf[4]<< 2) | buf[5] );
		tp2.y = ( (buf[6]<< 2) | buf[7] );
		touch = buf[8];
		DBG("Origin:x1=%3d, y1=%3d,  x2=%3d, y2=%3d,touch =%x\n", tp1.x, tp1.y, tp2.x, tp2.y, touch);
	}
#endif

	return ret;

}


static int g_screen_key=0;
#ifdef TOUCHKEY_ON_SCREEN

static unsigned char initkey_code[] =
{
    KEY_BACK, KEY_MENU, KEY_HOMEPAGE, KEY_SEARCH
};

static int get_screen_key(int x, int y)
{
	typedef struct
	{
		int x;
		int y;
		int keycode;
	}rect;
		const int span = 10;
		int idx;
		const rect rt[]=
#if defined (TOUCH_KEY_MAP)
		TOUCH_KEY_MAP;
#else
			{
				{845,	5, 		KEY_SEARCH},	/* search */
				{845,	40,		KEY_HOMEPAGE},	/* home */
				{845,	75,		KEY_MENU},		/* menu */
				{845,	104,	KEY_BACK	},	/* back */
				{0,0,0}
			};
#endif
		DBG("***x=%d, y=%d\n", x, y);
		for(idx=0; rt[idx].keycode; idx++)
		{
			if(x >= rt[idx].x-span && x<= rt[idx].x+span)
				if(y >= rt[idx].y-span && y<= rt[idx].y+span)
					return rt[idx].keycode;
		}
		return 0;
}

static int key_led_ctrl(int on)
{
#ifdef TOUCH_KEY_LED
	gpio_set_value(TOUCH_KEY_LED, on);
#endif
}

struct input_dev *tp_key_input;
static int report_screen_key(int down_up)
{
	struct input_dev * keydev=(struct input_dev *)tp_key_input;

	key_led_ctrl(down_up);
	input_report_key(keydev, g_screen_key, down_up);
	input_sync(keydev);
	if(!down_up) {
		g_screen_key=0;
	}
	return 0;
}
#endif

static int bu21020_process_points(struct bu21020_data *data)
{
	struct ts_event *event = &data->event;
	struct i2c_client *client = data->client;	
	u8 start_reg=0x0;
	u8 buf[32] = {0};
	int ret = -1;
	int status = 0;
	int last_points;
	int id = 0;

	//u8 finger;
	u8 touch;
	int xd,yd;
	static int point1_down, point2_down;
	static int first_start = 0;

	start_reg = 0x20;
	buf[0] = start_reg;

	//DBG(" addr 0x2A=0x%x \n",bu21020_read_reg(client, 0x2A));
	//DBG(" addr 0x2B=0x%x \n",bu21020_read_reg(client, 0x2B));
	//ret = bu21020_rx_data(client, buf, 8);
	ret = bu21020_rx_data(client, buf, 9);
	if (ret < 0) {
		DBG("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		goto out;
	}

	memset(event, 0, sizeof(struct ts_event));

	tp1.x = ( (buf[0]<< 2) | buf[1] );
	tp1.y = ( (buf[2]<< 2) | buf[3] );
	tp2.x = ( (buf[4]<< 2) | buf[5] );
	tp2.y = ( (buf[6]<< 2) | buf[7] );

	touch = buf[8];
	
#if defined(CONFIG_MACH_A8)
	ret=bu21020_write_reg(client, 0x3E, 0xFF);	
#endif
	DBG("Origin:x1=%3d, y1=%3d,  x2=%3d, y2=%3d,touch =%x\n", tp1.x, tp1.y, tp2.x, tp2.y, touch);

#ifdef FILTER_1 //if coordinate and touch_info are asynchronous, ignore information this time and next time

	if( (tp1.y > 0) && (tp1.x > 0) && (tp2.y>0) && (tp2.x>0) )
	{

		if (error_flg_2 || error_flg_1)
		{
			error_flg_1 =0;
			if((touch & 0x03)==0x03)
				error_flg_2 =0;
			else
				error_flg_2 =1;
			goto out;
		}	
		else if((touch&0x03)!=0x03)
		{
			error_flg_2 =1;
			goto out;
		}
		else 
			error_flg_2 = 0;

	}
	else if ( ((tp1.y > 0) && (tp1.x > 0)) || ((tp2.y>0) && (tp2.x>0) ))
	{

		if (error_flg_1 || error_flg_2)
		{
			error_flg_2 =0;
			if((touch & 0x03)==0x01)
				error_flg_1 =0;
			else
				error_flg_1 =1;				
			goto out;
		}	
		else if((touch&0x03)!=0x01)
		{
			error_flg_1 =1;
			goto out;
		}
		else 
			error_flg_1 = 0;			
	}
	else if( (tp1.y > 0) || (tp1.x > 0) || (tp2.y>0) || (tp2.x>0) || (touch&0x03)	)
	{

		error_flg_1 =0;
		error_flg_2 =0;
		goto out;
	}
	else
	{
		error_flg_1 =0;
		error_flg_2 =0;
	}		

#endif

#ifdef FILTER_2 //add counter between touch status from 2/0 to 1, or from 1/0 to 2

	if( ( (tp1.y > 0) && (tp1.x > 0) ) && ( (tp2.y>0) && (tp2.x>0) ) )
	{

#ifdef SW_AVG		
		if(finger_count_2 >= (TWO_COUNT - SKIP_AVG_1))
	    {	Coord_Avg(&tp1);
	   		Coord_Avg(&tp2);
		}
#endif		
		finger = 2;	

		finger_count_1 = 0; 
		finger_count_2++;
		if(finger_count_2 < TWO_COUNT)
		{
			goto out;
		}
#ifdef STEP2SMALL
		if(finger_count_2 >= TWO_COUNT)
		{
			if (abs(tp1.x-x1_old) < STEP_DIFF)
				tp1.x = x1_old;

			if (abs(tp1.y-y1_old) < STEP_DIFF)
				tp1.y = y1_old;

			if (abs(tp2.x-x2_old) < STEP_DIFF)
				tp2.x = x2_old;

			if (abs(tp2.y-y2_old) < STEP_DIFF)
				tp2.y = y2_old;
		}
		x1_old = tp1.x;
		y1_old = tp1.y;
		x2_old = tp2.x;
		y2_old = tp2.y;
#endif

	}
	else if ( ( (tp1.y>0)&&(tp1.x>0) ) || ( (tp2.y>0)&& (tp2.x>0) ) )
	{
		if((tp2.x != 0) && (tp2.y != 0))
		{
			tp1.x = tp2.x;
			tp1.y = tp2.y;
			tp2.x = 0;
			tp2.y = 0;
		}	
		if(finger==0)
		{
			finger_count_0 =1;
			finger = 1;
			goto out;
		}			
		else if(finger==2)
		{
			finger_count_1 = ONE_COUNT;
			finger_count_2 = 0;
			finger = 1;	
#ifdef SW_AVG 
			tp1.x = 0;
			tp1.y = 0;
	    	Coord_Avg(&tp1);
	   		Coord_Avg(&tp2);
#endif		
			goto out;
		}
		else //if(finger ==1)
		{
			if(finger_count_1 > 1)
			{
				finger_count_1--;				
				goto out;
			}
			else if(finger_count_1 == 1)
			{
				finger_count_1--;				
				goto out;
			}
			else if(finger_count_0 == 1)
			{
				finger_count_0 =0;
			}

		}				
		finger = 1;

	}
	else
	{
#if 0
		if(finger ==1)
		{
			finger = 0;
			goto out;
		}
#endif
		finger = 0;	
		error_flg_1 = 0;
		error_flg_2 = 0;
		finger_count_0 = 0;
		finger_count_1 = 0;
		finger_count_2 = 0;
#ifdef SW_AVG
		tp1.x = 0;
		tp1.y = 0;
		tp2.x = 0;
		tp2.y = 0;
		Coord_Avg(&tp1);
	  	Coord_Avg(&tp2);
#endif		
	}
#endif	

	event->x1 =	tp1.x ;
	event->y1 =	tp1.y ;
	event->x2 =	tp2.x ;
	event->y2 =	tp2.y ;

	//DBG(" addr 0x18=0x%x \n",bu21020_read_reg(client, 0x18));
	//DBG(" addr 0x10=0x%x \n",bu21020_read_reg(client, 0x10));
	//DBG(" addr 0x11=0x%x \n",bu21020_read_reg(client, 0x11));
	//DBG("before:x1=%3d, y1=%3d,  x2=%3d, y2=%3d,touch =%x\n", event->x1, event->y1, event->x2, event->y2, touch);

	if((event->x1==0 && event->y1==0 && event->x2==0 && event->y2==0 ) || (touch == 0))
	//if((event->x1==0 || event->y1==0) && (event->x2==0 || event->y2==0) )
	{
#ifdef AML_TP_PORTING
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_mt_sync(data->input_dev);
		input_sync(data->input_dev);
		DBG("finger up!");
#else
		if( event->x1==0 && event->y1==0 ) {
			input_mt_slot(data->input_dev, 0);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
		}
		if( event->x2==0 && event->y2==0 ) {
			input_mt_slot(data->input_dev, 1);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
		}
		input_sync(data->input_dev);
#endif
		if(!calib_finish)
		{
			bu21020_check_calib(client);
			DBG("check calib result: %s\n", calib_finish? "success" : "fail");
		}
		if(!dac_finish)
		{
		  dac_finish = bu21020_check_dac(client, 0x11, EVR_Y_NO, STEP_Y);
			DBG("Y axis check DAC result : %s\n", dac_finish ? "fail" : "success");
			ret = bu21020_check_dac(client, 0x01, EVR_X_NO, STEP_X);
			DBG("X axis check DAC result : %s\n", ret ? "fail" : "success");
			dac_finish = ~(dac_finish | ret);
			DBG("check DAC result : %s\n", dac_finish? "success" : "fail");
			if(0x01 & bu21020_read_reg(client, 0x28))
			{
				calib_finish = 0;
				dac_finish = 0;
			}				
			DBG("calib_finish=%d,dac_finish=%d\n",calib_finish,dac_finish);

		}
		
		goto out;
	}

	if ((event->x1 > 0) && (event->y1 > 0)) {
		gADPoint.x=event->x1;
		gADPoint.y=event->y1;
#ifdef CALIBRATION
		TouchPanelCalibrateAPoint(event->x1, event->y1, &xd, &yd);
		event->x1 = xd / 4;
		event->y1 = yd / 4;
#endif
		event->x1 = SCREEN_MAX_X+SCREEN_MIN_X-event->x1;
		//DBG("after: x1=%3d, y1=%3d\n", event->x1, event->y1);

		event->pressure=200;
		event->w=50;
#ifdef AML_TP_PORTING
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x1);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y1);
		DBG("TOUCH_NO=%d,(%d,%d), pressure=%d, w=%d\n",1,event->x1, event->y1, event->pressure, event->w);
		input_mt_sync(data->input_dev);
#else
		input_mt_slot(data->input_dev, 0);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
		//input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, 0); 
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
		//input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, event->w);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x1);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y1);
		//DBG("TOUCH_NO=%d,(%d,%d), pressure=%d, w=%d\n",1,event->x1, event->y1, event->pressure, event->w);
		//input_mt_sync(data->input_dev);
#endif
	}

	if ((event->x2 > 0) && (event->y2 > 0)) {
		gADPoint.x=event->x2;
		gADPoint.y=event->y2;
#ifdef CALIBRATION
		TouchPanelCalibrateAPoint(event->x2, event->y2, &xd, &yd);
		event->x2 = xd / 4;
		event->y2 = yd / 4;
#endif
		event->x2 = SCREEN_MAX_X+SCREEN_MIN_X-event->x2;
		//DBG("after: x2=%3d, y2=%3d,\n", event->x2, event->y2);

		event->pressure=200;
		event->w=50;
#ifdef AML_TP_PORTING
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x2);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y2);		
		DBG("TOUCH_NO=%d,(%d,%d), pressure=%d, w=%d\n",2,event->x2, event->y2, event->pressure, event->w);
		input_mt_sync(data->input_dev);
#else
		input_mt_slot(data->input_dev, 1);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
		//input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, 1); 
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
		//input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, event->w);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x2);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y2);		
		//DBG("TOUCH_NO=%d,(%d,%d), pressure=%d, w=%d\n",2,event->x2, event->y2, event->pressure, event->w);
		//input_mt_sync(data->input_dev);
#endif
	}

	input_sync(data->input_dev); 

out:
#ifdef UPDATE_INT
#if defined(CONFIG_MACH_A8)
  	ret = 0;
#else
	ret=bu21020_write_reg(client, 0x3E, 0xFF);	
#endif

	if(ret < 0)
	{		
		DBG("bu21020 i2c txdata failed\n");	
	}

#else
	if (finger)
	{
		mod_timer(&data->timer,jiffies + msecs_to_jiffies(50) );
	} else {
		// Clear all Interrupt
		i2c_smbus_write_byte_data(ts->client, 0x3E, 0xFF);
		hrtimer_cancel(&ts->timer);

		if (ts->use_irq)
			enable_irq(ts->client->irq);
	}
#endif

	return 1;
}

static void  bu21020_delaywork_func(struct work_struct *work)
{
	struct bu21020_data *bu21020 = container_of(work, struct bu21020_data, pen_event_work);
	struct i2c_client *client = bu21020->client;

	bu21020_process_points(bu21020);
	enable_irq(client->irq);	
}

static irqreturn_t bu21020_interrupt(int irq, void *handle)
{
	struct bu21020_data *bu21020_ts = handle;

	//DBG("--- Enter:%s %d\n",__FUNCTION__,__LINE__);
	disable_irq_nosync(irq);
	schedule_work(&bu21020_ts->pen_event_work);
	return IRQ_HANDLED;
}

static void bu21020_tpscan_timer(unsigned long data)
{
	struct bu21020_data *bu21020_ts=(struct bu21020_data *)data;
	//DBG("*** 1111111\n");
	schedule_work(&bu21020_ts->pen_event_work);
	//DBG("*** bu21020_tpscan_timer enter\n");
}

static int bu21020_remove(struct i2c_client *client)
{
	struct bu21020_data *bu21020 = i2c_get_clientdata(client);
	
    input_unregister_device(bu21020->input_dev);
    input_free_device(bu21020->input_dev);
    free_irq(client->irq, bu21020);
    kfree(bu21020); 
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&bu21020_early_suspend);
#endif      
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu21020_suspend(struct early_suspend *h)
{
	int err;
	int w_value;
	int reg;

	DBG("==bu21020_ts_suspend=\n");
	disable_irq(g_client->irq);		
}

static void bu21020_resume(struct early_suspend *h)
{
	struct bu21020_data *bu21020 = i2c_get_clientdata(g_client);
	
	bu21020_write_reg(g_client, 0x40, 0x02);
	mdelay(1);
	bu21020_write_reg(g_client, 0x3E, 0xFF);
	bu21020_write_reg(g_client, 0x40, 0x03);
	mdelay(1);
	bu21020_write_reg(g_client, 0x3E, 0xFF);
	mdelay(30);	//bu21020_chip_reset(bu21020);

	DBG("==bu21020_ts_resume=\n");
	enable_irq(g_client->irq);		
}
#else
static int bu21020_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}
static int bu21020_resume(struct i2c_client *client)
{
	return 0;
}
#endif

static const struct i2c_device_id bu21020_id[] = {
		{"bu21020", 0},
		{ }
};

static struct i2c_driver bu21020_driver = {
	.driver = {
		.name = "bu21020",
	    },
	.id_table 	= bu21020_id,
	.probe		= bu21020_probe,
	.remove		= __devexit_p(bu21020_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND	
	.suspend = &bu21020_suspend,
	.resume = &bu21020_resume,
#endif	
};

static int bu21020_init_client(struct i2c_client *client)
{
	struct bu21020_data *bu21020;
	int ret;

	bu21020 = i2c_get_clientdata(client);

	DBG("irq is %d\n",client->irq);
	if ( !gpio_is_valid(client->irq)) {
		DBG("+++++++++++gpio_is_invalid\n");
		return -EINVAL;
	}

	gpio_free(bu21020->int_gpio);
	ret = gpio_request(bu21020->int_gpio, "bu21020_int");
	if (ret) {
		DBG( "failed to request bu21020 GPIO%d\n", bu21020->int_gpio);
		return ret;
	}

//    ret = gpio_direction_input(bu21020->int_gpio);
//    if (ret) {
//        DBG("failed to set bu21020  gpio input\n");
//return ret;
//    }
	//gpio_pull_updown(client->irq, GPIOPullUp);
	//bu21020->int_gpio=client->irq;
	//client->irq = gpio_to_irq(client->irq);
	bu21020->irq = client->irq;
       	gpio_direction_input(bu21020->int_gpio);
        gpio_enable_edge_int(gpio_to_idx(bu21020->int_gpio), 1, client->irq - INT_GPIO_0);

	ret = request_irq(client->irq, bu21020_interrupt, /*IRQF_TRIGGER_FALLING*/ IRQF_TRIGGER_LOW, client->dev.driver->name, bu21020);
	DBG("request irq is %d,ret is  0x%x\n",client->irq,ret);
	if (ret ) {
		DBG(KERN_ERR "bu21020_init_client: request irq failed,ret is %d\n",ret);
        return ret;
	}
 
	return 0;
}

static int  bu21020_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bu21020_data *bu21020_ts;
#ifdef AML_TP_PORTING
#else
	struct ts_hw_data *pdata = client->dev.platform_data;
#endif
	int err = 0;
	int i;

	DBG("%s enter\n",__FUNCTION__);

	bu21020_ts = kzalloc(sizeof(struct bu21020_data), GFP_KERNEL);
	if (!bu21020_ts) {
		DBG("[bu21020]:alloc data failed.\n");
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	g_client = client;
	bu21020_ts->client = client;
	bu21020_ts->last_points = 0;
	i2c_set_clientdata(client, bu21020_ts);
#ifdef AML_TP_PORTING
	bu21020_ts->touch_en_gpio = AML_TOUCH_EN_GPIO;
	bu21020_ts->int_gpio = AML_INT_GPIO;
#else
	bu21020_ts->reset_gpio = pdata->reset_gpio;
	bu21020_ts->touch_en_gpio = pdata->touch_en_gpio;
	bu21020_ts->int_gpio = pdata->int_gpio;

	gpio_free(bu21020_ts->reset_gpio);
	err = gpio_request(bu21020_ts->reset_gpio, "bu21020 rst");
	if (err) {
		DBG( "failed to request bu21020 reset GPIO%d\n",gpio_to_irq(client->irq));
		goto exit_alloc_gpio_rst_failed;
	}
#endif

#if defined (TOUCH_POWER_PIN)
	//gpio_free(bu21020_ts->touch_en_gpio);
	err = gpio_request(bu21020_ts->touch_en_gpio, "bu21020_ts power enable");
	if (err) {
		//DBG( "failed to request bu21020_ts power enable GPIO%d\n",gpio_to_irq(client->irq));
		goto exit_alloc_gpio_power_failed;
	}
#if defined (TOUCH_POWER_MUX_NAME)
	rk29_mux_api_set(TOUCH_POWER_MUX_NAME, TOUCH_POWER_MUX_MODE_GPIO);
#endif
#endif

	err = bu21020_chip_init(client);
	if (err < 0) {
		printk(KERN_ERR
		       "bu21020_probe: bu21020 chip init failed\n");
		goto exit_request_gpio_irq_failed;
	}

	bu21020_ts->timer.expires=jiffies + msecs_to_jiffies(1);
	setup_timer(&bu21020_ts->timer,bu21020_tpscan_timer,(unsigned long)bu21020_ts);

	INIT_WORK(&bu21020_ts->pen_event_work, bu21020_delaywork_func);

	err = bu21020_init_client(client);
	if (err < 0) {
		printk(KERN_ERR
		       "bu21020_probe: bu21020_init_client failed\n");
		goto exit_request_gpio_irq_failed;
	}
		
	bu21020_ts->input_dev = input_allocate_device();
	if (!bu21020_ts->input_dev) {
		err = -ENOMEM;
		printk(KERN_ERR
		       "bu21020_probe: Failed to allocate input device\n");
		goto exit_input_allocate_device_failed;
	}

	bu21020_ts->input_dev->name = "bu21020-ts";
	bu21020_ts->input_dev->dev.parent = &client->dev;

	err = input_register_device(bu21020_ts->input_dev);
	if (err < 0) {
		printk(KERN_ERR
		       "bu21020_probe: Unable to register input device: %s\n",
		       bu21020_ts->input_dev->name);
		goto exit_input_register_device_failed;
	}

#ifdef TOUCHKEY_ON_SCREEN
	#ifdef TOUCH_KEY_LED
		err = gpio_request(TOUCH_KEY_LED, "key led");
		if (err < 0) {
			printk(KERN_ERR
			       "bu21020_probe: Unable to request gpio: %d\n",
			       TOUCH_KEY_LED);
			goto exit_input_register_device_failed;
		}
		gpio_direction_output(TOUCH_KEY_LED, GPIO_LOW);
		gpio_set_value(TOUCH_KEY_LED, GPIO_LOW);
	#endif
	
	tp_key_input = bu21020_ts->input_dev;
	for (i = 0; i < ARRAY_SIZE(initkey_code); i++)
		__set_bit(initkey_code[i], bu21020_ts->input_dev->keybit);
#endif

#if MULTI_TOUCH
	__set_bit(EV_SYN, bu21020_ts->input_dev->evbit);
	__set_bit(EV_KEY, bu21020_ts->input_dev->evbit);
	__set_bit(EV_ABS, bu21020_ts->input_dev->evbit);
#ifdef AML_TP_PORTING
	__set_bit(BTN_TOUCH, bu21020_ts->input_dev->keybit);
#else
	__set_bit(INPUT_PROP_DIRECT, bu21020_ts->input_dev->propbit);
	
	input_mt_init_slots(bu21020_ts->input_dev, MAX_POINT);
#endif
	input_set_abs_params(bu21020_ts->input_dev, ABS_MT_POSITION_X, SCREEN_MIN_X, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(bu21020_ts->input_dev, ABS_MT_POSITION_Y, SCREEN_MIN_Y, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(bu21020_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(bu21020_ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 50, 0, 0);
	input_set_abs_params(bu21020_ts->input_dev, ABS_MT_TRACKING_ID, 0, 4, 0, 0);
#else
	__set_bit(EV_SYN, bu21020_ts->input_dev->evbit);
	__set_bit(EV_KEY, bu21020_ts->input_dev->evbit);
	__set_bit(EV_ABS, bu21020_ts->input_dev->evbit);
	__set_bit(BTN_TOUCH, bu21020_ts->input_dev->keybit);

	//input_mt_init_slots(bu21020_ts->input_dev, 1);
	input_set_abs_params(bu21020_ts->input_dev, ABS_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(bu21020_ts->input_dev, ABS_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(bu21020_ts->input_dev, ABS_PRESSURE, 0, 50, 0, 0);
#endif
	
#ifdef CONFIG_HAS_EARLYSUSPEND
    bu21020_early_suspend.suspend = bu21020_suspend;
    bu21020_early_suspend.resume = bu21020_resume;
    bu21020_early_suspend.level = 0x2;
    register_early_suspend(&bu21020_early_suspend);
#endif
	
	return 0;

exit_input_register_device_failed:
	input_free_device(bu21020_ts->input_dev);
exit_input_allocate_device_failed:
    free_irq(client->irq, bu21020_ts);
exit_request_gpio_irq_failed:
	kfree(bu21020_ts);	
exit_alloc_gpio_power_failed:
#if defined (TOUCH_POWER_PIN)
	gpio_free(bu21020_ts->touch_en_gpio);
#endif
exit_alloc_gpio_rst_failed:
    gpio_free(bu21020_ts->reset_gpio);
exit_alloc_data_failed:
	DBG("%s error\n",__FUNCTION__);
	return err;
}

static int __init bu21020_mod_init(void)
{

	DBG("bu21020 module init\n");
	return i2c_add_driver(&bu21020_driver);
}

static void __exit bu21020_mod_exit(void)
{
	i2c_del_driver(&bu21020_driver);
}

module_init(bu21020_mod_init);
module_exit(bu21020_mod_exit);

MODULE_DESCRIPTION("BU21020 touchpad driver");
MODULE_AUTHOR("lbt <kernel@rock-chips>");
MODULE_LICENSE("GPL");

