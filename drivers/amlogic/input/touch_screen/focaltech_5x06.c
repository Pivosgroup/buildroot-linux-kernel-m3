/* 
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver. 
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION      	DATE			AUTHOR
 *    1.0		  2010-01-05			WenFS
 *
 * note: only support mulititouch	Wenfs 2010-10-01
 */



#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <mach/gpio.h>
#include <linux/ft5x06_ts.h>
static int ft5x0x_write_reg(u8 addr, u8 para);
static int ft5x0x_read_reg(u8 addr, u8 *pdata);
static struct i2c_client *this_client;
static struct ts_platform_data *focaltechPdata2;
static int ft5x0x_printk_enable_flag=0;

#define FT5X0X_EVENT_MAX	5
//#define CONFIG_TOUCH_PANEL_KEY

#ifdef CONFIG_TOUCH_PANEL_KEY
#define TOUCH_SCREEN_RELEASE_DELAY (100 * 1000000)//unit usec
#define TAP_KEY_RELEASE_DELAY (100 * 1000000)
#define TAP_KEY_TIME 10
enum {
	NO_TOUCH,
	TOUCH_KEY_PRE,
	TAP_KEY,
	TOUCH_KEY,
	TOUCH_SCREEN,
	TOUCH_SCREEN_RELEASE,
};
#endif

struct ts_event {
	u8 id;
	s16	x;
	s16	y;
	s16	z;
	s16 w;
};

struct ft5x0x_ts_data {
	struct input_dev	*input_dev;
	struct ts_event	event[FT5X0X_EVENT_MAX];
	u8 event_num;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;
	struct early_suspend	early_suspend;
#ifdef CONFIG_TOUCH_PANEL_KEY
	u8 touch_state;
	short key;
	struct hrtimer timer;
	struct ts_event first_event;
	int offset;
	int touch_count;
#endif
};

#define ft5x0x_dbg(fmt, args...)  { if(ft5x0x_printk_enable_flag) \
					printk("[ft5x0x]: " fmt, ## args); }

//#define AC_DETECT_IN_TOUCH_DRIVER
static int ac_detect_flag_current=0;
static int ac_detect_flag_old=3;
#ifdef AC_DETECT_IN_TOUCH_DRIVER
static void Ac_Detect_In_Touch_Driver(void)
{
	unsigned char ver;
	int ac_detect_flag_temp=0;
	ac_detect_flag_temp=focaltechPdata2->Ac_is_connect();
	ac_detect_flag_current=ac_detect_flag_temp;
	if(ac_detect_flag_current!=ac_detect_flag_old)
		{
		if(1==focaltechPdata2->Ac_is_connect())
			ft5x0x_write_reg(0xb2,0x1);
		else
			ft5x0x_write_reg(0xb2,0x0);
		ac_detect_flag_old=ac_detect_flag_current;
		}
	if(1==ft5x0x_printk_enable_flag)
		{
		ft5x0x_read_reg(0xb2, &ver);
		printk("reg 0xb2=%d\n",ver);
		}
}
#endif
static ssize_t ft5x0x_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct capts *ts = (struct capts *)dev_get_drvdata(dev);

    if (!strcmp(attr->attr.name, "ft5x0xPrintFlag")) {
        memcpy(buf, &ft5x0x_printk_enable_flag,sizeof(ft5x0x_printk_enable_flag));
        printk("buf[0]=%d, buf[1]=%d\n", buf[0], buf[1]);
        return sizeof(ft5x0x_printk_enable_flag);
    }
    return 0;
}

static ssize_t ft5x0x_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct capts *ts = (struct capts *)dev_get_drvdata(dev);

	if (!strcmp(attr->attr.name, "ft5x0xPrintFlag")) {
		printk("buf[0]=%d, buf[1]=%d\n", buf[0], buf[1]);
		if (buf[0] == '0') ft5x0x_printk_enable_flag = 0;
		if (buf[0] == '1') ft5x0x_printk_enable_flag = 1;
		if (buf[0] == '2') {
			ft5x0x_printk_enable_flag=2;
			if(focaltechPdata2->power){
				focaltechPdata2->power(0);
				msleep(50);
				focaltechPdata2->power(1);
				msleep(200);
			}
		}
		if (buf[0] == '3') {
			u8 data;
			ft5x0x_write_reg(0xa5, 0x03);
			printk("set reg[0xa5] = 0x03\n");
			msleep(20);
			ft5x0x_read_reg(0xa5, &data);
			printk("read back: reg[0xa5] = %d\n", data);
    }
  }
	return count;
}

static DEVICE_ATTR(ft5x0xPrintFlag, S_IRWXUGO, ft5x0x_read, ft5x0x_write);
static struct attribute *ft5x0x_attr[] = {
    &dev_attr_ft5x0xPrintFlag.attr,
    NULL
};

static struct attribute_group ft5x0x_attr_group = {
    .name = NULL,
    .attrs = ft5x0x_attr,
};


/***********************************************************************************************
Name	:	ft5x0x_i2c_rxdata 

Input	:	*rxdata
                     *length

Output	:	ret

function	:	

***********************************************************************************************/
static int ft5x0x_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

    //msleep(1);
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	
	return ret;
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static int ft5x0x_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

   	//msleep(1);
	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}
/***********************************************************************************************
Name	:	 ft5x0x_write_reg

Input	:	addr -- address
                     para -- parameter

Output	:	

function	:	write register of ft5x0x

***********************************************************************************************/
static int ft5x0x_write_reg(u8 addr, u8 para)
{
    u8 buf[3];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = ft5x0x_i2c_txdata(buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! %#x ret: %d", buf[0], ret);
        return -1;
    }
    
    return 0;
}


/***********************************************************************************************
Name	:	ft5x0x_read_reg 

Input	:	addr
                     pdata

Output	:	

function	:	read register of ft5x0x

***********************************************************************************************/
static int ft5x0x_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2] = {0};

	buf[0] = addr;
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf,
		},
	};

    //msleep(1);
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

	*pdata = buf[0];
	return ret;
  
}


/***********************************************************************************************
Name	:	 ft5x0x_read_fw_ver

Input	:	 void
                     

Output	:	 firmware version 	

function	:	 read TP firmware version

***********************************************************************************************/
static unsigned char ft5x0x_read_fw_ver(void)
{
	unsigned char ver;
	ft5x0x_read_reg(FT5X0X_REG_FIRMID, &ver);
	return(ver);
}


//#define CONFIG_FOCALTECH_TOUCHSCREEN_CODE_UPG


#ifdef CONFIG_FOCALTECH_TOUCHSCREEN_CODE_UPG

typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;     //8 bit
typedef unsigned short        FTS_WORD;    //16 bit
typedef unsigned int          FTS_DWRD;    //16 bit
typedef unsigned char         FTS_BOOL;    //8 bit

#define FTS_NULL                0x0
#define FTS_TRUE                0x01
#define FTS_FALSE              0x0

#define I2C_CTPM_ADDRESS       0x70


void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            udelay(1);
        }
    }
}


/*
[function]: 
    callback: read data from ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[out]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_read_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    
    ret=i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret<=0)
    {
        printk("[TSP]i2c_read_interface error\n");
        return FTS_FALSE;
    }
  
    return FTS_TRUE;
}

/*
[function]: 
    callback: write data to ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[in]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_write_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    ret=i2c_master_send(this_client, pbt_buf, dw_lenth);
    if(ret<=0)
    {
        printk("[TSP]i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret);
        return FTS_FALSE;
    }

    return FTS_TRUE;
}

/*
[function]: 
    send a command to ctpm.
[parameters]:
    btcmd[in]        :command code;
    btPara1[in]    :parameter 1;    
    btPara2[in]    :parameter 2;    
    btPara3[in]    :parameter 3;    
    num[in]        :the valid input parameter numbers, if only command code needed and no parameters followed,then the num is 1;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL cmd_write(FTS_BYTE btcmd,FTS_BYTE btPara1,FTS_BYTE btPara2,FTS_BYTE btPara3,FTS_BYTE num)
{
    FTS_BYTE write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);
}

/*
[function]: 
    write data to ctpm , the destination address is 0.
[parameters]:
    pbt_buf[in]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_write(FTS_BYTE* pbt_buf, FTS_DWRD dw_len)
{
    
    return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}

/*
[function]: 
    read out data from ctpm,the destination address is 0.
[parameters]:
    pbt_buf[out]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_read(FTS_BYTE* pbt_buf, FTS_BYTE bt_len)
{
    return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
}


/*
[function]: 
    burn the FW to ctpm.
[parameters]:(ref. SPEC)
    pbt_buf[in]    :point to Head+FW ;
    dw_lenth[in]:the length of the FW + 6(the Head length);    
    bt_ecc[in]    :the ECC of the FW
[return]:
    ERR_OK        :no error;
    ERR_MODE    :fail to switch to UPDATE mode;
    ERR_READID    :read id fail;
    ERR_ERASE    :erase chip fail;
    ERR_STATUS    :status error;
    ERR_ECC        :ecc error.
*/


#define    FTS_PACKET_LENGTH        128
#ifdef CONFIG_MACH_MESON_8726M_REFB04
static unsigned char CTPM_FW[]=
{
#include "ft_app848.ft"
};
#else
static unsigned char CTPM_FW[]=
{
#include "ft_app.ft"
};
#endif
E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    FTS_BYTE reg_val[2] = {0};
    FTS_DWRD i = 0;

    FTS_DWRD  packet_number;
    FTS_DWRD  j;
    FTS_DWRD  temp;
    FTS_DWRD  lenght;
    FTS_BYTE  packet_buf[FTS_PACKET_LENGTH + 6];
    FTS_BYTE  auc_i2c_write_buf[10];
    FTS_BYTE bt_ecc;
    int      i_ret;

    /*********Step 1:Reset  CTPM *****/
    /*write 0xaa to register 0xfc*/
    ft5x0x_write_reg(0xfc,0xaa);
    delay_qt_ms(50);
     /*write 0x55 to register 0xfc*/
    ft5x0x_write_reg(0xfc,0x55);
    printk("[TSP] Step 1: Reset CTPM test\n");
   
    delay_qt_ms(30);   


    /*********Step 2:Enter upgrade mode *****/
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
    do
    {
        i ++;
        i_ret = ft5x0x_i2c_txdata(auc_i2c_write_buf, 2);
        delay_qt_ms(5);
    }while(i_ret <= 0 && i < 5 );

    /*********Step 3:check READ-ID***********************/ 
	delay_qt_ms(5);
    cmd_write(0x90,0x00,0x00,0x00,4);
    byte_read(reg_val,2);
    if (reg_val[0] == 0x79 && reg_val[1] == 0x3)
    {
        printk("[TSP] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
    }
    else
    {
    	for(i=0;i<10;i++)
    		{
    		 byte_read(reg_val,2);
			 if (reg_val[0] == 0x79 && reg_val[1] == 0x3)
			 	{
			 	 printk("[TSP] Step 3: succes = 0x%x",i);
				 break;
			 	}
			 else
			 	{
			 	 printk("[TSP] Step 3: error = 0x%x",i);
			 	}
    		}
		if(i==10)
			{
			printk("[TSP] Step 3: return error = 0x%x",i);
        	return ERR_READID;
			}
        //i_is_new_protocol = 1;
    }

     /*********Step 4:erase app*******************************/
    cmd_write(0x61,0x00,0x00,0x00,1);
   
    delay_qt_ms(1500);
    printk("[TSP] Step 4: erase. \n");

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    printk("[TSP] Step 5: start upgrade. \n");
    dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j=0;j<packet_number;j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i=0;i<FTS_PACKET_LENGTH;i++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        
        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        delay_qt_ms(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
              printk("[TSP] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        byte_write(&packet_buf[0],temp+6);    
        delay_qt_ms(20);
    }

    //send the last six byte
    for (i = 0; i<6; i++)
    {
        temp = 0x6ffa + i;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        temp =1;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i]; 
        bt_ecc ^= packet_buf[6];

        byte_write(&packet_buf[0],7);  
        delay_qt_ms(20);
    }

    /*********Step 6: read out checksum***********************/
    /*send the opration head*/
    cmd_write(0xcc,0x00,0x00,0x00,1);
    byte_read(reg_val,1);
    printk("[TSP] Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc)
    {
    printk("[TSP] Step 6:  ecc error and return \n");
        return ERR_ECC;
    }

    /*********Step 7: reset the new FW***********************/
	
    cmd_write(0x07,0x00,0x00,0x00,1);
	printk("[TSP] Step 7 \n");
    return ERR_OK;
}


int fts_ctpm_fw_upgrade_with_i_file(void)
{
   FTS_BYTE*     pbt_buf = FTS_NULL;
   int i_ret;
    
    //=========FW upgrade========================*/
   pbt_buf = CTPM_FW;
   /*call the upgrade function*/
   i_ret =  fts_ctpm_fw_upgrade(pbt_buf,sizeof(CTPM_FW));
   if (i_ret != 0)
   {
       //error handling ...
       //TBD
   }

   return i_ret;
}

unsigned char fts_ctpm_get_upg_ver(void)
{
    unsigned int ui_sz;
    ui_sz = sizeof(CTPM_FW);
    if (ui_sz > 2)
    {
        return CTPM_FW[ui_sz - 2];
    }
    else
    {
        //TBD, error handling?
        return 0xff; //default value
    }
}

#endif



static int is_tp_key(struct tp_key *tp_key, int key_num, int x, int y)
{
	int i;
	
	if (tp_key && key_num) {
		for (i=0; i<key_num; i++) {
			if ((x > tp_key->x1) && (x < tp_key->x2)
			&& (y > tp_key->y1) && (y < tp_key->y2)) {
				return tp_key->key;
			}
			tp_key++;
		}
	}
	return 0;
}

#ifdef CONFIG_TOUCH_PANEL_KEY
static enum hrtimer_restart ft5x0x_timer(struct hrtimer *timer)
{
	struct ft5x0x_ts_data *data = container_of(timer, struct ft5x0x_ts_data, timer);

 	if (data->touch_state == TOUCH_SCREEN) {
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_sync(data->input_dev);
		ft5x0x_dbg("touch screen up(2)!\n");
	}
 	else if (data->touch_state == TAP_KEY) {
		input_report_key(data->input_dev, data->key, 1);
		input_report_key(data->input_dev, data->key, 0);
		input_sync(data->input_dev);
		ft5x0x_dbg("touch key(%d) down(short)\n", data->key);
		ft5x0x_dbg("touch key(%d) up(short)\n", data->key);
	}
 	data->touch_state = NO_TOUCH;
	return HRTIMER_NORESTART;
};
#endif

/*
 * return event number.
*/
static int ft5x0x_get_event(struct ts_event *event)
{
	u8 buf[32] = {0};
	int ret, i;

	ret = ft5x0x_i2c_rxdata(buf, 31);
	if (ret < 0) {
		printk("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}

	ret = (buf[2] & 0x07);
	if (ret > FT5X0X_EVENT_MAX) ret = FT5X0X_EVENT_MAX;
	for (i=0; i<ret; i++) {
		event->id = i;
		event->x = (s16)(buf[3+i*6] & 0x0F)<<8 | (s16)buf[4+i*6];
		event->y = (s16)(buf[5+i*6] & 0x0F)<<8 | (s16)buf[6+i*6];
    event->z = 200;
    event->w = 1;
    if (focaltechPdata2->swap_xy)
    	swap(event->x, event->y);
    if (focaltechPdata2->xpol)
			event->x = focaltechPdata2->screen_max_x - event->x;
    if (focaltechPdata2->ypol)
			event->y = focaltechPdata2->screen_max_y - event->y;
		if (event->x == 0) event->x = 1;
		if (event->y == 0) event->y = 1;
		event++;
	}

	return ret;
}

static void ft5x0x_report_mt_event(struct input_dev *input, struct ts_event *event, int event_num)
{
	int i;
	
	for (i=0; i<event_num; i++) {
//		input_report_abs(input, ABS_MT_TOUCH_ID, event->id);
		input_report_abs(input, ABS_MT_POSITION_X, event->x);
		input_report_abs(input, ABS_MT_POSITION_Y, event->y);
		input_report_abs(input, ABS_MT_TOUCH_MAJOR, event->z);
		input_report_abs(input, ABS_MT_WIDTH_MAJOR, event->w);
		input_mt_sync(input);
		ft5x0x_dbg("point_%d: %d, %d\n",event->id, event->x,event->y);
		event++;
	}
	input_sync(input);
}

#define enter_touch_screen_state() {\
	data->first_event = data->event[0];\
	data->offset = 0;\
	data->touch_count = 0;\
	input_report_key(data->input_dev, BTN_TOUCH, 1);\
	data->touch_state = TOUCH_SCREEN;\
}

#define enter_key_pre_state() {\
	data->key = key;\
	data->touch_count = 0;\
	data->touch_state = TOUCH_KEY_PRE;\
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void ft5x0x_ts_pen_irq_work(struct work_struct *work)
{
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event[0];
	int event_num = 0;
	
	event_num = ft5x0x_get_event(event);
	if (event_num < 0) {
		enable_irq(this_client->irq);
		return;
	}

#ifdef AC_DETECT_IN_TOUCH_DRIVER
	Ac_Detect_In_Touch_Driver();
#endif

#ifdef CONFIG_TOUCH_PANEL_KEY
	int key = 0;
	if (event_num == 1) {
		key = is_tp_key(focaltechPdata2->tp_key, focaltechPdata2->tp_key_num, event->x, event->y);
		if (key)
			ft5x0x_dbg("key pos: %d, %d\n", event->x,event->y);
	}
	
	switch (data->touch_state) {
	case NO_TOUCH:
		if(key)	{
			ft5x0x_dbg("touch key(%d) down 0\n", key);
			enter_key_pre_state();
		}
		else if (event_num) {
			ft5x0x_dbg("touch screen down\n");
			ft5x0x_report_mt_event(data->input_dev, event, event_num);
			enter_touch_screen_state();
		}
		break;

	case TOUCH_KEY_PRE:
		if (key) {
			data->key = key;
			if (++data->touch_count > TAP_KEY_TIME) {
				ft5x0x_dbg("touch key(%d) down\n", key);
				input_report_key(data->input_dev, key, 1);
				input_sync(data->input_dev);
				data->touch_state = TOUCH_KEY;
			}
		}
		else if(event_num) {
			ft5x0x_report_mt_event(data->input_dev, event, event_num);
			enter_touch_screen_state();
		}		
		else {
			hrtimer_start(&data->timer, ktime_set(0, TAP_KEY_RELEASE_DELAY), HRTIMER_MODE_REL);
			data->touch_state = TAP_KEY;
		}
		break;
	
	case TAP_KEY:
		if (key) {
			hrtimer_cancel(&data->timer);
			input_report_key(data->input_dev, data->key, 1);
			input_report_key(data->input_dev, data->key, 0);
			input_sync(data->input_dev);
			ft5x0x_dbg("touch key(%d) down(tap)\n", data->key);
			ft5x0x_dbg("touch key(%d) up(tap)\n", data->key);
			enter_key_pre_state();
		}
		else if (event_num) {
			hrtimer_cancel(&data->timer);
			ft5x0x_dbg("ignore the tap key!\n");
			ft5x0x_report_mt_event(data->input_dev, event, event_num);
			enter_touch_screen_state();
		}
		break;
		
	case TOUCH_KEY:
		if (!event_num) {
			input_report_key(data->input_dev, data->key, 0);
			input_sync(data->input_dev);
			ft5x0x_dbg("touch key(%d) up\n", data->key);
			data->touch_state = NO_TOUCH;
		}
		break;
		
	case TOUCH_SCREEN:
		if (!event_num) {
			if ((data->offset < 10) && (data->touch_count > 1)) {
				input_report_key(data->input_dev, BTN_TOUCH, 0);
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				input_sync(data->input_dev);
				ft5x0x_dbg("touch screen up!\n");
	     	data->touch_state = NO_TOUCH;
    	}
    	else {
				ft5x0x_dbg("touch screen up(1)\n");
	      hrtimer_start(&data->timer, ktime_set(0, TOUCH_SCREEN_RELEASE_DELAY), HRTIMER_MODE_REL);
			}
		}
		else {
			hrtimer_cancel(&data->timer);
			data->touch_count++;
			if (!key) {
				int offset;
				ft5x0x_report_mt_event(data->input_dev, event, event_num);
				offset = abs(event->x - data->first_event.x);
				offset += abs(event->y - data->first_event.y);
				if (offset > data->offset) data->offset = offset;
			}
		}
		break;
		
	default:
		break;
	}
#else
	if (event_num)
	{	
		input_report_key(data->input_dev, BTN_TOUCH, 1);
		ft5x0x_report_mt_event(data->input_dev, event, event_num);
	}
	else {
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_sync(data->input_dev);
	}
#endif
	enable_irq(this_client->irq);
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{
//	static int irq_count = 0;
//	printk("irq count: %d\n", irq_count++);
	struct ft5x0x_ts_data *ft5x0x_ts = dev_id;
	disable_irq_nosync(this_client->irq);	
	if (!work_pending(&ft5x0x_ts->pen_event_work)) {
		queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
	}
	return IRQ_HANDLED;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void ft5x0x_ts_suspend(struct early_suspend *handler)
{
//	struct ft5x0x_ts_data *ts;
//	ts =  container_of(handler, struct ft5x0x_ts_data, early_suspend);
	printk("==ft5x0x_ts_suspend=\n");
	if(focaltechPdata2->power) {
		u8 data;
		ft5x0x_write_reg(0xa5, 0x03);
		printk("set reg[0xa5] = 0x03\n");
		msleep(20);
		ft5x0x_read_reg(0xa5, &data);
		printk("read back: reg[0xa5] = %d\n", data);
	}
	if (focaltechPdata2->key_led_ctrl)
		focaltechPdata2->key_led_ctrl(0);
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void ft5x0x_ts_resume(struct early_suspend *handler)
{
	printk("==ft5x0x_ts_resume=\n");
	if(focaltechPdata2->power) {
		focaltechPdata2->power(0);
		msleep(50);
		focaltechPdata2->power(1);
		msleep(200);
	}
	if (focaltechPdata2->key_led_ctrl)
		focaltechPdata2->key_led_ctrl(1);
}
#endif  //CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static int 
ft5x0x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x0x_ts_data *ft5x0x_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value; 
	struct ts_platform_data *pdata = client->dev.platform_data;
	
	if (!pdata) {
		printk("%s: no platform data\n", __FUNCTION__);
		err = -ENODEV;
		goto exit_check_functionality_failed;
		
	}
	focaltechPdata2 = client->dev.platform_data;
	if(pdata->power){
		pdata->power(0);
		mdelay(50);
		pdata->power(1);
		mdelay(200);
	}

	printk("==ft5x0x_ts_probe=\n");
	
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}
	//client->irq =  client->dev.platform_data->irq;
	client->irq =pdata->irq;
	printk("==kzalloc=\n");
	ft5x0x_ts = kzalloc(sizeof(*ft5x0x_ts), GFP_KERNEL);
	if (!ft5x0x_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	printk("==kzalloc success=\n");
	this_client = client;
	i2c_set_clientdata(client, ft5x0x_ts);

	INIT_WORK(&ft5x0x_ts->pen_event_work, ft5x0x_ts_pen_irq_work);
	ft5x0x_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x0x_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
 
//	__gpio_as_irq_fall_edge(pdata->intr);		//
printk("==enable Irq=\n");
    if (pdata->init_irq) {
        pdata->init_irq();
    }
printk("==enable Irq success=\n");

	disable_irq_nosync(this_client->irq);
//	disable_irq(IRQ_EINT(6));

	printk("==input_allocate_device=\n");
	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	ft5x0x_ts->input_dev = input_dev;
#ifdef CONFIG_TOUCH_PANEL_KEY
	ft5x0x_ts->touch_state = NO_TOUCH;
	ft5x0x_ts->key = 0;
	if (pdata->tp_key && pdata->tp_key_num) {
		int i;
		for (i=0; i<pdata->tp_key_num; i++) {
			set_bit(pdata->tp_key[i].key, input_dev->keybit);
			printk("tp key (%d)registered\n", pdata->tp_key[i].key);
		}
		set_bit(EV_SYN, input_dev->evbit);
		hrtimer_init(&ft5x0x_ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ft5x0x_ts->timer.function = ft5x0x_timer;
	}
#endif

	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_X, 0, pdata->screen_max_x, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_Y, 0, pdata->screen_max_y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	input_dev->name		= FT5X0X_NAME;		//dev_name(&client->dev)
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"ft5x0x_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	printk("==register_early_suspend =\n");
	ft5x0x_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ft5x0x_ts->early_suspend.suspend = ft5x0x_ts_suspend;
	ft5x0x_ts->early_suspend.resume	= ft5x0x_ts_resume;
	register_early_suspend(&ft5x0x_ts->early_suspend);
#endif

#ifdef CONFIG_FOCALTECH_TOUCHSCREEN_CODE_UPG
    msleep(50);
    //get some register information
    uc_reg_value = ft5x0x_read_fw_ver();
    printk("[FST] Firmware old version = 0x%x\n", uc_reg_value);


    fts_ctpm_fw_upgrade_with_i_file();
    mdelay(200);
    uc_reg_value = ft5x0x_read_fw_ver();
    printk("[FST] Firmware new version = 0x%x\n", uc_reg_value);
#endif
    uc_reg_value = ft5x0x_read_fw_ver();
    printk("[FST] Firmware new version = 0x%x\n", uc_reg_value);

	err = request_irq(client->irq, ft5x0x_ts_interrupt, IRQF_DISABLED, "ft5x0x_ts", ft5x0x_ts);
//	err = request_irq(IRQ_EINT(6), ft5x0x_ts_interrupt, IRQF_TRIGGER_FALLING, "ft5x0x_ts", ft5x0x_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x0x_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}

    
//wake the CTPM
//	__gpio_as_output(GPIO_FT5X0X_WAKE);		
//	__gpio_clear_pin(GPIO_FT5X0X_WAKE);		//set wake = 0,base on system
//	 msleep(100);
//	__gpio_set_pin(GPIO_FT5X0X_WAKE);			//set wake = 1,base on system
//	msleep(100);
//	ft5x0x_set_reg(0x88, 0x05); //5, 6,7,8
//	ft5x0x_set_reg(0x80, 30);
//	msleep(50);
//  enable_irq(this_client->irq);
   // enable_irq(IRQ_EINT(6));
   
    err = sysfs_create_group(&client->dev.kobj, &ft5x0x_attr_group);

	if (focaltechPdata2->key_led_ctrl)
		focaltechPdata2->key_led_ctrl(1);
	printk("==probe over =\n");
    return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	free_irq(client->irq, ft5x0x_ts);
//	free_irq(IRQ_EINT(6), ft5x0x_ts);
exit_irq_request_failed:
exit_platform_data_null:
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
exit_create_singlethread:
	printk("==singlethread error =\n");
	i2c_set_clientdata(client, NULL);
	kfree(ft5x0x_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static int __devexit ft5x0x_ts_remove(struct i2c_client *client)
{
	printk("==ft5x0x_ts_remove=\n");
	struct ft5x0x_ts_data *ft5x0x_ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ft5x0x_ts->early_suspend);
	free_irq(client->irq, ft5x0x_ts);
//	free_irq(IRQ_EINT(6), ft5x0x_ts);
	input_unregister_device(ft5x0x_ts->input_dev);
	kfree(ft5x0x_ts);
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id ft5x0x_ts_id[] = {
	{ FT5X0X_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver ft5x0x_ts_driver = {
	.probe		= ft5x0x_ts_probe,
	.remove		= __devexit_p(ft5x0x_ts_remove),
	.id_table	= ft5x0x_ts_id,
	.driver	= {
		.name	= FT5X0X_NAME,
		.owner	= THIS_MODULE,
	},
};

/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static int __init ft5x0x_ts_init(void)
{
	int ret;
	printk("==ft5x0x_ts_init==\n");
	ret = i2c_add_driver(&ft5x0x_ts_driver);
	printk("ret=%d\n",ret);
	return ret;
//	return i2c_add_driver(&ft5x0x_ts_driver);
}

/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void __exit ft5x0x_ts_exit(void)
{
	printk("==ft5x0x_ts_exit==\n");
	i2c_del_driver(&ft5x0x_ts_driver);
}

module_init(ft5x0x_ts_init);
module_exit(ft5x0x_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");
