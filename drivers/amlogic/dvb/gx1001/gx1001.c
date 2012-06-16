/*****************************************************************
**
**  Copyright (C) 2009 Amlogic,Inc.
**  All rights reserved 
**        Filename : gx1001.c
**
**  comment:
**        Driver for GX1001 demodulator
**  author :
**	    jianfeng_wang@amlogic
**  version :
**	    v1.0	 09/03/05
*****************************************************************/
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include "gx1001.h"


//#include "i2c_control.c"
#define MAX_REG_VALUE_LEN				(1)

#define TUNER_DCT70708					(7)
#define TUNER_ALPSTDQE5					(8)



//#define DEMOD_STANDBY_PIN				(15)
//#define TUNER_POWER_PIN				(14)

//unsigned long  GX_OSCILLATE_FREQ		= 28636;	//(oscillate frequency) ( Unit: KHz ) 
unsigned long  GX_OSCILLATE_FREQ		= 28800;	//(oscillate frequency) ( Unit: KHz ) 
unsigned long  GX_IF_FREQUENCY			= 36000;	//(tuner carrier center frequency) ( Unit: KHz ) 

#ifdef CONFIG_AMLOGIC_BOARD_APOLLO_H_8226H2_LINUX
unsigned int  GX_TS_OUTPUT_MODE    		= 0;        // 1: Parallel output,  0: Serial output//modefied by jeffchang
#else
unsigned int  GX_TS_OUTPUT_MODE    		= 1;        // 1: Parallel output,  0: Serial output//modefied by jeffchang
#endif

int  GX_PLL_M_VALUE						= 0x0b; 	// This parameter is effective only for GX1001B
int  GX_PLL_N_VALUE						= 0x00; 	// This parameter is effective only for GX1001B
int  GX_PLL_L_VALUE						= 0x05; 	// This parameter is effective only for GX1001B

int  GX_RF_AGC_MIN_VALUE				= 0x00; 	// This parameter is effective only for GX1001B
int  GX_RF_AGC_MAX_VALUE				= 0xff; 	// This parameter is effective only for GX1001B
int  GX_IF_AGC_MIN_VALUE				= 0x00; 	// This parameter is effective only for GX1001B
int  GX_IF_AGC_MAX_VALUE				= 0xff; 	// This parameter is effective only for GX1001B

int  SFenable							= DISABLE;	//DISABLE:do not open the sf function,ENABLE:open the sf function
int  FMenable							= ENABLE;	//DISABLE:do not open the fm function,ENABLE:opne the fm function

#define AM_SUCCESS                                (0)
#define DEMOD_ERROR_BASE                          (0x80000000+0x1000)
#define DEMOD_SUCCESS                           		  (0)
#define DEMOD_ERR_NO_MEMORY                       (DEMOD_ERROR_BASE+1)
#define DEMOD_ERR_DEVICE_NOT_EXIST                (DEMOD_ERROR_BASE+2)
#define DEMOD_ERR_HARDWARE_ERROR                  (DEMOD_ERROR_BASE+3)
#define DEMOD_ERR_BAD_PARAMETER                   (DEMOD_ERROR_BASE+4)
#define DEMOD_ERR_NOT_INITLIZED                   (DEMOD_ERROR_BASE+5)
#define DEMOD_ERR_DESTROY_FAILED                  (DEMOD_ERROR_BASE+6)
#define DEMOD_ERR_FEATURE_NOT_SUPPORT             (DEMOD_ERROR_BASE+7)
#define DEMOD_ERR_OTHER                           (DEMOD_ERROR_BASE+8)
#define TUNER_TYPE TUNER_DCT70708
#define debug    1


/*******************************************************************
* I2C function
********************************************************************/

//timer function

#if 1
#define pr_dbg(fmt, args...) printk(KERN_DEBUG "GX1001: " fmt, ## args)
#else
#define pr_dbg(fmt, args...)
#endif

#define pr_error(fmt, args...) printk(KERN_ERR "GX1001: " fmt, ## args)

#define AM_TRACE pr_dbg

/*
Function:	Delay N ms
Input:		
        ms_value - delay time (10ms)
*/
static void GX_Delay_N_10ms(unsigned int ms_value)
{
	//User must add the delay function
	//Sleep(ms_value);
	//current->state = TASK_INTERRUPTIBLE;   //must do like this 
	//schedule_timeout(ms_value);
	//mdelay(ms_value*10);
	msleep(ms_value*10);
}
int gx1001_set_addr(struct gx1001_state *state, unsigned char regaddr)
{
	INT32U nRetCode = AM_SUCCESS;
	INT8U buffer[MAX_REG_VALUE_LEN+1];
	struct i2c_msg msg;

	buffer[0] = regaddr;
	memset(&msg, 0, sizeof(msg));
	msg.addr = (state->config.demod_addr);
	msg.flags &=  ~I2C_M_RD;  //write  I2C_M_RD=0x01
	msg.len = 1;
	msg.buf = buffer;
	//pr_dbg("gx1001 write dev addr %d,buf[0]=%d,msg_len=%d\r\n",msg.addr,buffer[0],msg.len);
	nRetCode = i2c_transfer(state->i2c, &msg, 1);
	
	if(nRetCode != 1)
	{
		AM_TRACE("gx1001_writeregister reg 0x%x failure,ret %d!\n",regaddr,nRetCode);
		return 0;
	}
	return 1;   //success
}
int gx1001_writeregister(struct gx1001_state *state, unsigned char regaddr, unsigned char* data, unsigned char length)
{
#if 0
	INT32U nRetCode = AM_SUCCESS;
	INT8U buffer[MAX_REG_VALUE_LEN+1];
	struct i2c_msg msg;
	
	
	if(data == 0 || length == 0)
	{
		AM_TRACE("gx1001 write register parameter error !!\n");
		return 0;
	}

	buffer[0] = regaddr;
	memcpy(&buffer[1], data, length);
	length += 1;

	memset(&msg, 0, sizeof(msg));
	msg.addr = state->config.demod_addr;
	msg.flags &=  ~I2C_M_RD;  //write  I2C_M_RD=0x01
	msg.len = length;
	msg.buf = buffer;
	//pr_dbg("gx1001 write dev addr %d,buf[0]=%d\r\n",msg.addr,buffer[0]);
	nRetCode = i2c_transfer(state->i2c, &msg, 1);

	if(nRetCode != 1)
	{
		if(regaddr!=0x10)
		AM_TRACE("gx1001_writeregister reg 0x%x failure!,ret2 %d\n",regaddr,nRetCode);
		return 0;
	}
	return 1;   //success
#endif

	int ret = 0;
	unsigned char regbuf[1];			/*8 bytes reg addr, regbuf 1 byte*/
	struct i2c_msg msg[2];			/*construct 2 msgs, 1 for reg addr, 1 for reg value, send together*/

	regbuf[0] = regaddr & 0xff;

	memset(msg, 0, sizeof(msg));

	/*write reg address*/
	msg[0].addr = (state->config.demod_addr);					
	msg[0].flags = 0;
	msg[0].buf = regbuf;
	msg[0].len = 1;

	/*write value*/
	msg[1].addr = (state->config.demod_addr);
	msg[1].flags = I2C_M_NOSTART;	/*i2c_transfer will emit a stop flag, so we should send 2 msg together,
																 * and the second msg's flag=I2C_M_NOSTART, to get the right timing*/
	msg[1].buf = data;
	msg[1].len = length;

	ret = i2c_transfer(state->i2c, msg, 2);

	if(ret<0){
		AM_TRACE(" %s: writereg error, errno is %d \n", __FUNCTION__, ret);
		return 0;
	}
	else
		return 1;

}

int gx1001_readregister(struct gx1001_state *state, unsigned char regaddr, unsigned char* data, unsigned char length)
{
	INT32U nRetCode = AM_SUCCESS;
	struct i2c_msg msg;
	
	if(data == 0 || length == 0)
	{
		AM_TRACE("gx1001 read register parameter error !!\n");
		return 0;
	}

	gx1001_set_addr(state, regaddr) ; //set reg address first

	//read real data 
	memset(&msg, 0, sizeof(msg));
	msg.addr = (state->config.demod_addr);
	msg.flags |=  I2C_M_RD;  //write  I2C_M_RD=0x01
	msg.len = length;
	msg.buf = data;
	
	nRetCode = i2c_transfer(state->i2c, &msg, 1);

	if(nRetCode != 1)
	{
		AM_TRACE("gx1001_readregister reg 0x%x failure!\n",regaddr);
		return 0;
	}
	return 1;

}

int Tuner_Writeregister(struct gx1001_state *state, unsigned char* buffer,unsigned char bDataLength)
{
	INT32U nRetCode = AM_SUCCESS;
	struct i2c_msg msg;
	
	if(buffer == 0 || bDataLength == 0)
	{
		AM_TRACE("dtc70700 write register parameter error !!\n");
		return 0;
	}

	memset(&msg, 0, sizeof(msg));
	msg.addr = (state->config.tuner_addr);
	msg.flags &=  ~I2C_M_RD;  //write  I2C_M_RD=0x01
	msg.len = bDataLength;
	msg.buf = buffer;
	//pr_dbg("gx1001 write dev addr %d,buf[0]=%d\r\n",msg.addr,buffer[0]);
	nRetCode = i2c_transfer(state->i2c, &msg, 1);

	//nRetCode = AM_I2C_Write(I2C_TUNER_ADDRESS, buffer, bDataLength);
	
	if(nRetCode != 1)
	{
		AM_TRACE("gx1001_writeregister reg failure!\n");
		return 0;
	}
	return 1;   //success	
}

int Tuner_ReadRegister(struct gx1001_state *state, unsigned char *buffer,unsigned char bDatalength)
{
	INT32U nRetCode = AM_SUCCESS;
	struct i2c_msg msg;
	
	if(buffer == 0 || bDatalength == 0)
	{
		AM_TRACE("dtc70700 read register parameter error !!\n");
		return 0;
	}

	//nRetCode = AM_I2C_WriteRead(tuner_i2chandle, buffer, 0, bDatalength);
	memset(&msg, 0, sizeof(msg));
	msg.addr = (state->config.tuner_addr);
	msg.flags |=  I2C_M_RD;  //write  I2C_M_RD=0x01
	msg.len = bDatalength;
	msg.buf = buffer;
	
	nRetCode = i2c_transfer(state->i2c, &msg, 1);

	//nRetCode = AM_I2C_WriteRead(I2C_TUNER_ADDRESS, buffer, 0, bDatalength);
	if(nRetCode != 1)
	{
		AM_TRACE("gx1001_writeregister reg  failure!\n");
		return 0;
	}
	return 1;   //success	
}

/*******************************************************************
*  end i2c funciton
*******************************************************************/

#if TUNER_TYPE == TUNER_DCT70708
void gx1001_reset(struct gx1001_state *state)
{
	gpio_request(state->config.reset_pin, "gx1001:RESET");
	gpio_direction_output(state->config.reset_pin, 0);
	GX_Delay_N_10ms(60);
	gpio_direction_output(state->config.reset_pin, 1);
	GX_Delay_N_10ms(20);
}
#endif


#if TUNER_TYPE == TUNER_DCT70708
GX_STATE GX_Set_RFFrequency(struct gx1001_state *state, unsigned long fvalue)
{
		signed char UCtmp = FAILURE;
		unsigned char data[6];
		unsigned long freq;
		
		freq=(fvalue+GX_IF_FREQUENCY)*10/625;            /*freq=(fvalue+GX_IF_FREQUENCY)*/
		data[0] = state->config.tuner_addr;	         /*Tunner Address*/
		data[1] =(unsigned char)((freq>>8)&0xff);	
		data[2] =(unsigned char)(freq&0xff);	
		data[3] = 0X8b;//0xb3;	/*62.5KHz*/ modified by jeffchang
		
		if (fvalue < 153000) 
			data[4] = 0x01;
		else if (fvalue < 430000) 
			data[4] = 0x06;
		else 
			data[4] = 0x0c; 
		
		data[5] = 0xc3;

		if (SUCCESS == GX_Set_Tunner_Repeater_Enable(state, 1))	/*open the chip repeater */
		{   
			GX_Delay_N_10ms(1);		    
			if (SUCCESS == Tuner_Writeregister(state, &data[1], 5))
			{
				GX_Delay_N_10ms(1);
				UCtmp = GX_Set_Tunner_Repeater_Enable(state, 0);	/*close the chip repeater*/
				UCtmp = SUCCESS;
			}
		}

		if (SUCCESS == UCtmp)
		{
			GX_Delay_N_10ms(5);
			UCtmp = GX_HotReset_CHIP(state);
		}
		
		return UCtmp;
}
#endif

#if TUNER_TYPE == TUNER_ALPSTDQE5
GX_STATE GX_Set_RFFrequency(unsigned long fvalue)
{
		signed char UCtmp = FAILURE;
		unsigned char data[6];
		unsigned long freq;
		
		freq=(fvalue+GX_IF_FREQUENCY)*10/625;              /*freq=(fvalue+GX_IF_FREQUENCY)/Fref*/
		data[0] = I2C_TUNER_ADDRESS;	                   /*Tunner Address*/
		data[1] =(unsigned char)((freq>>8)&0xff);	
		data[2] =(unsigned char)(freq&0xff);	
		data[3] = 0x8b;										/*Fref=62.5KHz*/
		
		if (fvalue < 153000)
			data[4] = 0x60;
		else if (fvalue < 430000) 
			data[4] = 0xa2;
		else 
			data[4] = 0xaa; 
		
		data[5] = 0xc6;

		if (SUCCESS == GX_Set_Tunner_Repeater_Enable(1))	/*open the chip repeater */
		{   
			GX_Delay_N_10ms(1);		    
			if (SUCCESS == Tuner_Writeregister(&data[1], 5 ))	
			{
				GX_Delay_N_10ms(1);
				UCtmp = GX_Set_Tunner_Repeater_Enable(0);	/*close the chip repeater*/
				UCtmp = SUCCESS;
			}
		}

		if (SUCCESS == UCtmp)
		{
			GX_Delay_N_10ms(5);
			UCtmp = GX_HotReset_CHIP();
		}
		return UCtmp;
}

#endif

/*
Function:   Set AGC parameter
Output:
        SUCCESS or FAILURE
*/
GX_STATE GX_Set_AGC_Parameter(struct gx1001_state *state)
{
	 int temp=0;

	 GX_Write_one_Byte(state, GX_AGC_STD,28);
	 
	 temp=GX_Read_one_Byte(state, GX_MODE_AGC);
	 if(temp!=FAILURE)
	 {
		//temp&=0xe6 modify be live for double agc   
		
		temp&=0xe6;
		temp=temp+0x10;
		GX_Write_one_Byte(state, GX_MODE_AGC,temp);
		return SUCCESS;
	  	
	 }
	 else
	 {
	 	return FAILURE;
	 }
	 
}

/* 
Function: Write one byte to chip
Input:
        RegAdddress -- The register address
        WriteValue  -- The write value
Output:
        SUCCESS or FAILURE
*/
GX_STATE GX_Write_one_Byte(struct gx1001_state *state, int RegAddress,int WriteValue)
{
	int UCtmp=FAILURE;
	unsigned char data[2];
	
	data[0] = (unsigned char)RegAddress;
	data[1] = (unsigned char)WriteValue; 
	
	//UCtmp = GX_I2cReadWrite( WRITE, ChipAddress,data[0],&data[1], 1 );
	UCtmp = gx1001_writeregister(state, data[0], &data[1], 1);
	if (SUCCESS == UCtmp)//ok
	{
		if ((WriteValue&0xff) == (GX_Read_one_Byte(state, RegAddress)&0xff))
			return SUCCESS;
	}
	return FAILURE;
}


/* 
Function: Write one byte to chip with no read test 
Input:
        RegAdddress -- The register address
        WriteValue  -- The write value
Output:
        SUCCESS or FAILURE
*/
GX_STATE GX_Write_one_Byte_NoReadTest(struct gx1001_state *state, int RegAddress,int WriteValue)
{
	int UCtmp=FAILURE;
	unsigned char data[2];
	
	data[0] = (unsigned char)RegAddress;
	data[1] = (unsigned char)WriteValue; 
	
	//UCtmp = GX_I2cReadWrite( WRITE, ChipAddress,data[0],&data[1], 1 );
	UCtmp = gx1001_writeregister(state, data[0], &data[1], 1);
	
	return UCtmp;
}



/* 
Function: Read one byte from chip
Input:
        RegAdddress -- The register address
Output:
        success: Read value
        FAILURE:  FAILURE 
*/
int GX_Read_one_Byte(struct gx1001_state *state, int RegAddress)
{
	int UCtmp=FAILURE;
	unsigned char Temp_RegAddress=0;
	unsigned char Temp_Read_data =0;

	Temp_RegAddress = (unsigned char)RegAddress;
	
	//UCtmp = GX_I2cReadWrite(READ, ChipAddress,Temp_RegAddress,&Temp_Read_data, 1 );
	UCtmp = gx1001_readregister(state, Temp_RegAddress, &Temp_Read_data, 1);
	
	if (SUCCESS == UCtmp)//ok
		return (Temp_Read_data&0xff) ;
	else
		return FAILURE;
}

/*
Function: get  the version of chip 
Input:None
Output:
	NEWONE--- new version
	OLDONE--- old version 
*/
GX_STATE GX_Get_Version(struct gx1001_state *state)
{
	int temp1=0,temp2=0;
	temp1 = GX_Read_one_Byte(state, GX_CHIP_IDD);
	temp2 = GX_Read_one_Byte(state, GX_CHIP_VERSION);
	if((temp1==0x01)&&(temp2==0x02))
	{
		return NEWONE;
	}
	else
	{
		return OLDONE;
	}
}

GX_STATE GX_Get_AllReg(struct gx1001_state *state)
{
	AM_TRACE("GX_CHIP_ID          0x00   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_CHIP_ID                   ));
	AM_TRACE("GX_MAN_PARA         0x10   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_MAN_PARA                  ));
	AM_TRACE("GX_INT_PO1_SEL      0x11   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_INT_PO1_SEL               ));
	AM_TRACE("GX_SYSOK_PO2_SEL    0x12   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_SYSOK_PO2_SEL             ));
	AM_TRACE("GX_STATE_IND        0x13   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_STATE_IND                 ));
	AM_TRACE("GX_TST_SEL          0x14   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_TST_SEL                   ));
	AM_TRACE("GX_I2C_RST          0x15   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_I2C_RST                   ));
	AM_TRACE("GX_MAN_RST          0x16   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_MAN_RST                   ));
	AM_TRACE("GX_BIST             0x18   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_BIST                      ));
	AM_TRACE("GX_MODE_AGC         0x20   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_MODE_AGC                  ));
	AM_TRACE("GX_AGC_PARA         0x21   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_AGC_PARA                  ));
	AM_TRACE("GX_AGC2_THRES       0x22   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_AGC2_THRES                ));
	AM_TRACE("GX_AGC12_RATIO      0x23   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_AGC12_RATIO               ));
	AM_TRACE("GX_AGC_STD          0x24   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_AGC_STD                   ));
	AM_TRACE("GX_SCAN_TIME        0x25   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_SCAN_TIME                 ));
	AM_TRACE("GX_DCO_CENTER_H     0x26   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_DCO_CENTER_H              ));
	AM_TRACE("GX_DCO_CENTER_L     0x27   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_DCO_CENTER_L              ));
	AM_TRACE("GX_BBC_TST_SEL      0x28   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_BBC_TST_SEL               ));
	AM_TRACE("GX_AGC_ERR_MEAN     0x2B   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_AGC_ERR_MEAN              ));
	AM_TRACE("GX_FREQ_OFFSET_H    0x2C   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_FREQ_OFFSET_H             ));
	AM_TRACE("GX_FREQ_OFFSET_L    0x2D   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_FREQ_OFFSET_L             ));
	AM_TRACE("GX_AGC1_CTRL        0x2E   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_AGC1_CTRL                 ));
	AM_TRACE("GX_AGC2_CTRL        0x2F   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_AGC2_CTRL                 ));
	AM_TRACE("GX_FSAMPLE_H        0x40   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_FSAMPLE_H                 ));
	AM_TRACE("GX_FSAMPLE_M        0x41   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_FSAMPLE_M                 ));
	AM_TRACE("GX_FSAMPLE_L        0x42   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_FSAMPLE_L                 ));
	AM_TRACE("GX_SYMB_RATE_H      0x43   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_SYMB_RATE_H               ));
	AM_TRACE("GX_SYMB_RATE_M      0x44   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_SYMB_RATE_M               ));
	AM_TRACE("GX_SYMB_RATE_L      0x45   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_SYMB_RATE_L               ));
	AM_TRACE("GX_TIM_LOOP_CTRL_L  0x46   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_TIM_LOOP_CTRL_L           ));
	AM_TRACE("GX_TIM_LOOP_CTRL_H  0x47   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_TIM_LOOP_CTRL_H           ));
	AM_TRACE("GX_TIM_LOOP_BW      0x48   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_TIM_LOOP_BW               ));
	AM_TRACE("GX_EQU_CTRL         0x50   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_EQU_CTRL                  ));
	AM_TRACE("GX_SUM_ERR_POW_L    0x51   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_SUM_ERR_POW_L             ));
	AM_TRACE("GX_SUM_ERR_POW_H    0x52   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_SUM_ERR_POW_H             ));
	AM_TRACE("GX_EQU_BYPASS       0x53   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_EQU_BYPASS                ));
	AM_TRACE("GX_EQU_TST_SEL      0x54   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_EQU_TST_SEL               ));
	AM_TRACE("GX_EQU_COEF_L       0x55   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_EQU_COEF_L                ));
	AM_TRACE("GX_EQU_COEF_M       0x56   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_EQU_COEF_M                ));
	AM_TRACE("GX_EQU_COEF_H       0x57   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_EQU_COEF_H                ));
	AM_TRACE("GX_EQU_IND          0x58   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_EQU_IND                   ));
	AM_TRACE("GX_RSD_CONFIG       0x80   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_RSD_CONFIG                ));
	AM_TRACE("GX_ERR_SUM_1        0x81   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_ERR_SUM_1                 ));
	AM_TRACE("GX_ERR_SUM_2        0x82   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_ERR_SUM_2                 ));
	AM_TRACE("GX_ERR_SUM_3        0x83   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_ERR_SUM_3                 ));
	AM_TRACE("GX_ERR_SUM_4        0x84   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_ERR_SUM_4                 ));
	AM_TRACE("GX_RSD_DEFAULT      0x85   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_RSD_DEFAULT               ));
	AM_TRACE("GX_OUT_FORMAT       0x90   Value = 0X%02x\n",GX_Read_one_Byte(state, GX_OUT_FORMAT                ));
	return 1;
}

/*
Function: Set Mode Scan Mode
Input: mode		--0:disable mode scan
				  1:enable mode scan
Output:
	SUCCESS or FAILURE
*/
GX_STATE GX_Set_Mode_Scan(struct gx1001_state *state, int mode)
{
	int temp;

	temp=GX_Read_one_Byte(state, GX_MODE_SCAN_ENA);

	if( 0==mode)
		temp&=0x7f;
	else
		temp|=0x80;

	if(GX_Write_one_Byte(state, GX_MODE_SCAN_ENA,temp))
		return SUCCESS;
	else
		return FAILURE;
}



/*
Function: Set FM Mode
Input:mode   ---0:disable FM
				1:enable FM
Output:
	SUCCESS or FAILURE

*/
GX_STATE GX_Set_FM(struct gx1001_state *state, int mode)
{
	int temp;
	temp=GX_Read_one_Byte(state, GX_FM_CANCEL_CTRL);
	if( 0==mode )
		temp&=0xef;
	else
		temp|=0x10;
	if(GX_Write_one_Byte(state, GX_FM_CANCEL_CTRL,temp))
		 return SUCCESS;
	 else
		 return FAILURE;

}


/*
Function: Set SF Mode
Input: ----0:disable SF
		   1:enable SF
Output:
	SUCCESS or FAILURE

*/
GX_STATE GX_Set_SF(struct gx1001_state *state, int mode)
{
	int temp;
	temp=GX_Read_one_Byte(state, GX_SF_CANCEL_CTRL);
	if(0==mode)
		temp&=0xef;
	else
		temp|=0x10;
	if(GX_Write_one_Byte(state, GX_SF_CANCEL_CTRL,temp))
		return SUCCESS;
	else
		return FAILURE;

}


/*
Function:Set the Minimum of RF AGC Controller
Input: --------int value:the minimum value
Output:
SUCCESS or FAILURE
*/
GX_STATE GX_Set_RF_Min(struct gx1001_state *state, int value)
{
	if(GX_Write_one_Byte(state, GX_RF_MIN,value))
		return SUCCESS;
	else
		return FAILURE;
}


/*
Function:Set the Maximum value of RF AGC Controller
Input:-------int value :the maximum value
Output:
SUCCESS or FAILURE

*/
GX_STATE GX_Set_RF_Max(struct gx1001_state *state, int value)
{
	if(GX_Write_one_Byte(state, GX_RF_MAX,value))
		return SUCCESS;
	else
		return FAILURE;
}


/*
Function:Set the Minimum of IF AGC Controller
Input:---------int value :the maximum value
Output:
SUCCESS or FAILURE

*/
GX_STATE GX_set_IF_Min(struct gx1001_state *state, int value)
{
	if(GX_Write_one_Byte(state, GX_IF_MIN,value))
		return SUCCESS;
	else
		return FAILURE;
}

/*
Funtion:Set the Maximum value of IF AGC Controller
Input:---------int value: the Maximum value
Output:
SUCCESS or FAILURE

*/
GX_STATE GX_set_IF_Max(struct gx1001_state *state, int value)
{
	if(GX_Write_one_Byte(state, GX_IF_MAX,value))
		return SUCCESS;
	else
		return FAILURE;
}

/*
Function: Set  IF ThresHold Auto Adjust 
Input:--------0:disable
			  1:enable
Output:
	SUCCESS or FAILURE

*/
GX_STATE GX_SET_AUTO_IF_THRES(struct gx1001_state *state, int mode)
{
	int temp;
	temp=GX_Read_one_Byte(state, GX_AUTO_THRESH);
	if(0==mode)
		temp&=0x7f;
	else
		temp|=0x80;
	if(GX_Write_one_Byte(state, GX_AUTO_THRESH,temp))
		return SUCCESS;
	else 
		return FAILURE;
}



/*
Function: Set Tim Scan
Input: -------- 0:disable
			    1:enable
Output:
	SUCCESS or FAILURE

*/
GX_STATE GX_Set_Tim_Scan(struct gx1001_state *state, int mode)
{
	int temp;
	temp = GX_Read_one_Byte(state, GX_TIM_SCAN_ENA);
	if(0==mode)
		temp&=0x7f;
	else
		temp|=0x80;
	if(GX_Write_one_Byte(state, GX_TIM_SCAN_ENA,temp))
		return SUCCESS;
	else
		return FAILURE;
}


/*
Function: Set Digital AGC
Input: -------- 0:disable
				1:enable
Output:
	SUCCESS or FAILURE

*/
GX_STATE GX_Set_Digital_AGC(struct gx1001_state *state, int mode)
{
	int temp;
	temp = GX_Read_one_Byte(state, GX_DIGITAL_AGC_ON);
	if(0==mode)
		temp&=0x7f;
	else
		temp|=0x80;
	if(GX_Write_one_Byte(state, GX_DIGITAL_AGC_ON,temp))
		return SUCCESS;
	else
		return FAILURE;
}
/*
Funtion:	Set chip wake up
Input:
Sleep_Enable    --    1: Sleep
					  0: Working
Output:
			SUCCESS or FAILURE

*/
GX_STATE GX_Set_Sleep(struct gx1001_state *state, int Sleep_Enable)
{

	if(NEWONE==GX_Get_Version(state))
	{	
		int temp1=0,temp2=0;
		int UCtmp1 = FAILURE,UCtmp2 = FAILURE;

		temp1=GX_Read_one_Byte(state, GX_MAN_PARA);	/*0x10 - bit2*/
		temp2=GX_Read_one_Byte(state, 0x14);/*0x14 - - bit4-bit5*/
		if ((temp1!= FAILURE)&&(temp2!= FAILURE))
		{
			temp1 &=0xfb;
			temp2 &=0xcf;
				
			temp1 |= 0x04&(Sleep_Enable<<2);
			temp2 |= 0x10&(Sleep_Enable<<4);
			temp2 |= 0x20&(Sleep_Enable<<5);

			UCtmp1 = GX_Write_one_Byte(state, GX_MAN_PARA,temp1);
			UCtmp2 = GX_Write_one_Byte(state, 0x14,temp2);
			
			if ((SUCCESS == UCtmp1)&&(SUCCESS == UCtmp2))
			{
				if (0==Sleep_Enable )
				{
					UCtmp1 =GX_HotReset_CHIP(state);
				}
			}
		}
		return UCtmp1&&UCtmp2;
	}
	return 1;
	
}

/*
Function: Set Pll Value
Input:
	Pll_M_Value;
	Pll_N_Value;
	Pll_L_Value;
Output:
	SUCCESS or FAILURE
*/
GX_STATE GX_Set_Pll_Value(struct gx1001_state *state, int Pll_M_Value,int Pll_N_Value,int Pll_L_Value)
{
	if(GX_Write_one_Byte(state, GX_PLL_M,Pll_M_Value))
	{
		if(GX_Write_one_Byte(state, GX_PLL_L,((Pll_N_Value<<4)&0xf0)|(Pll_L_Value&0x0f)))
			return SUCCESS;	
		else
			return FAILURE;
	}
	else
		return FAILURE;

}

/*
Function:
*/
/*
Function:	Search signal with setted parameters
Input:
	    Symbol_Rate_1   --  Used first symbol rate value (range: 450 -- 9000)     (Unit: kHz)
	    Symbol_Rate_2	--  Used second symbol rate value. Please set 0 if no use	(Unit: kHz)
	
	    Spec_Mode	    --	0£ºsearch only positive spectrum
	    			    	1£ºsearch only negative spectrum
	    			    	2£ºfirst positive and then negative spectrum
	    			    	3£ºfirst negative and then positive spectrum
	                    
	    Qam_Size	    --  0-2 = reserved;
	    			    	3 = 16QAM;
	    			    	4 = 32QAM; 
	    			    	5 = 64QAM; 
	    			    	6 = 128QAM;
	    			    	7 = 256QAM.
	                    
	    RF_Freq		    --  The RF frequency (KHz)
                        
	    Wait_OK_X_ms    --  The waiting time before give up one search ( Unit: ms )
	    			        (Range: 250ms -- 2000ms, Recommend: 700ms)


Output:
		SUCCESS --  Have signal
		FAILURE	--  No signal
*/
GX_STATE GX_Search_Signal(  struct gx1001_state *state,
                            unsigned long Symbol_Rate_1,
                            unsigned long Symbol_Rate_2,
                            int Spec_Mode,
                            int Qam_Size,
                            unsigned long RF_Freq,
                            int Wait_OK_X_ms)
{
	int After_EQU_OK_Delay	= 60;	//60 ms,  
	int spec_invert_enable	= 0;	//spec invert enable flag
	int spec_invert_value	= 0;	//next spec invert value
	int symbol_2_enable		= 0;	//Symbol_Rate_2 enable flag

    int wait_ok_x_ms_temp	= 0;    //for save Wait_OK_X_ms
    int wait_ok_SF_temp=0;              // wait for lock SF 
    int GX1001Bflag=0;
    //-----------------------------------------------------------------------------

	 GX_CoolReset_CHIP(state);		// must be this !! will result in one i2c error do not care
	 if (FAILURE == GX_Init_Chip(state)) 
	 {
	 	pr_error("GX_Init_Chip FAILURE\n");
	 	return FAILURE;
	 }
	
	//-----------------------------------------------------------------------------
	wait_ok_x_ms_temp = Wait_OK_X_ms/10;	//as 700 ms = 70 * 10ms_Delay

	if (FAILURE == GX_Select_DVB_QAM_Size(state, Qam_Size)) 
	{
		pr_error("GX_Select_DVB_QAM_Size FAILURE\n");
		return FAILURE;	//Set QAM size
	}
	if (FAILURE == GX_SetSymbolRate(state, Symbol_Rate_1/1000)) 
	{
		pr_error("GX_SetSymbolRate FAILURE\n");
		return FAILURE;	//Set Symbol rate value
	}
	if (FAILURE == GX_Set_RFFrequency(state, RF_Freq))	
	{
		pr_error("GX_Set_RFFrequency FAILURE\n");
		return FAILURE;		//Set tuner frequency
	}
	if (Symbol_Rate_2 >= 4500) 
		symbol_2_enable = 1;	//Symbol_Rate_2 enable
	
	if (Symbol_Rate_1<2500) 
		After_EQU_OK_Delay = 100;   //100ms   (if  <2.5M  = 100ms)
	
	//-----------------------------------------------------------------------------
	
	if(NEWONE == GX_Get_Version(state))     //the chip version is GX1001B
	{
        GX1001Bflag=1;
	 	Spec_Mode=0;
	}
	
SYMBOL_2_SEARCH:
	switch (Spec_Mode)
	{
	case 3:	// first negative and then positive
		{
			spec_invert_enable = 1;
			spec_invert_value  = 0;	//next spec invert value
		}
	case 1:	//negative
		{
			GX_SetSpecInvert(state, 1);
		}
		break;
	case 2:// first positive and then negative
		{
			spec_invert_enable = 1;
			spec_invert_value  = 1;	//next spec invert value
		}
	default://positive
		{
			GX_SetSpecInvert(state, 0);
		}
		break;
	}
	//-----------------------------------------------------------------------------

SPEC_INVERT_SEARCH:
	if (FAILURE == GX_HotReset_CHIP(state)) 
	{
		pr_error("GX_HotReset_CHIP FAILURE\n");
		return FAILURE;
	}
	wait_ok_x_ms_temp = Wait_OK_X_ms/10;	//as 700 ms = 70 * 10ms_Delay

	while ((FAILURE == GX_Read_EQU_OK(state)) && (wait_ok_x_ms_temp))
	{
		wait_ok_x_ms_temp --;
		GX_Delay_N_10ms(1);		//Delay 10 ms
	}

	if ( 0 == wait_ok_x_ms_temp)           //Read EQU time over
	{    
	    if(GX1001Bflag==1&&SFenable==ENABLE)     //the chip version is GX1001B
	      {  GX_Set_SF(state, ENABLE);
		
	         GX_Set_FM(state, ENABLE);
	         GX_HotReset_CHIP(state);
	         wait_ok_SF_temp=80;
	         while ((FAILURE == GX_Read_ALL_OK(state)) && (wait_ok_SF_temp))
	         {
	         wait_ok_SF_temp --;
		 GX_Delay_N_10ms(2);		//Delay 20 ms	
	         }
	         if(SUCCESS==GX_Read_ALL_OK(state))    //SUCCESS while open SF&FM
	         { 
	          return SUCCESS;
	         }
	    	}
	  else if (symbol_2_enable)
			{
				symbol_2_enable = 0;
                            if (Symbol_Rate_2<25000) 
                               After_EQU_OK_Delay = 100;   //100ms
                            else
                               After_EQU_OK_Delay = 60;   //60ms
                    		  GX_SetSymbolRate(state, Symbol_Rate_2) ;
                            if(GX1001Bflag==1)
			         {GX_Set_SF(state, DISABLE);
	                        GX_Set_FM(state, DISABLE);
			          }
		              goto SYMBOL_2_SEARCH;
			   }
	  else
	  	{
	  		return FAILURE;
	  	}
	}	
	
	GX_Delay_N_10ms(After_EQU_OK_Delay/10);		//Delay After_EQU_OK_Delay ms

	if (SUCCESS == GX_Read_ALL_OK(state))	//All ok
	{
		if(GX1001Bflag==1&&FMenable==ENABLE)     //the chip version is GX1001B
		GX_Set_FM(state, ENABLE);                 //open FM for GX1001B
		return SUCCESS;
	}
	else
	{
		if (spec_invert_enable)
		{
			spec_invert_enable = 0;				//disable spec invert
			if (FAILURE == GX_SetSpecInvert(state, spec_invert_value))  
			{
				return FAILURE;	//spec invert
			}
			else
			{
				goto SPEC_INVERT_SEARCH;
			}
		}
		else
		{
			return FAILURE;
		}
		
	}
}

//========================================================================================================================


/*
Function: Set TS output mode
Input:
		0 - Serial
		1 - Parallel
Output:
        SUCCESS or FAILURE
*/
GX_STATE GX_Set_OutputMode(struct gx1001_state *state, int mode)
{
	int temp=0;
	int UCtmp = FAILURE;
	temp=GX_Read_one_Byte(state, GX_OUT_FORMAT);	/*0x90 - bit6*/

	if (temp != FAILURE)
	{
		temp &= 0xbf;
		if (mode) temp+=0x60;//clock_posedge must set to 0,do not use 0x40  jeffchang

		UCtmp = GX_Write_one_Byte(state, GX_OUT_FORMAT,temp);
	}
	return UCtmp;
}



/* 
Function: Select QAM size (4 - 256), only for DVB.
Input:
        size  --  0-2 = reserved;
			        3 = 16QAM;
			        4 = 32QAM; 
			        5 = 64QAM; 
			        6 = 128QAM;
			        7 = 256QAM.
utput:
        SUCCESS or FAILURE
*/
GX_STATE GX_Select_DVB_QAM_Size(struct gx1001_state *state, int size)
{
	int temp=0;
	int UCtmp = FAILURE;

	if ((size>7)||(size<=2)) size = 5;
		size<<=5;

	temp=GX_Read_one_Byte(state, GX_MODE_AGC);

	if (temp != FAILURE)
	{
		temp &= 0x1f;
		temp += size;
		UCtmp = GX_Write_one_Byte(state, GX_MODE_AGC,temp);  /*0x20 - bit7:5   */
	}
	return UCtmp;
}



/* 
Function: Set symbol rate 
Input:
        Symbol_Rate_Value :  The range is from 450 to 9000	(Unit: kHz)
        
Output:
        SUCCESS or FAILURE
*/
GX_STATE GX_SetSymbolRate(struct gx1001_state *state, unsigned long Symbol_Rate_Value)
{
	int UCtmp = FAILURE;
	unsigned long temp_value=0;
	
	temp_value = Symbol_Rate_Value*1000;        

	UCtmp =	GX_Write_one_Byte(state, GX_SYMB_RATE_H,((int)((temp_value>>16)&0xff)));	/*0x43*/

	if (SUCCESS == UCtmp)
	{
		UCtmp = GX_Write_one_Byte(state, GX_SYMB_RATE_M,((int)((temp_value>>8)&0xff)));	/*0x44*/

		if (SUCCESS == UCtmp)
		{
			UCtmp = GX_Write_one_Byte(state, GX_SYMB_RATE_L,((int)( temp_value&0xff))); /*0x45*/
		}
	}
	return UCtmp;
}


/*
Function: Set oscillate frequancy 
Output:
        SUCCESS or FAILURE 
*/
GX_STATE GX_SetOSCFreq(struct gx1001_state *state)
{
	int UCtmp = FAILURE;
	unsigned long temp=0;
	unsigned long OSC_frequancy_Value =0; 

	if( NEWONE==GX_Get_Version(state))	
	{	
		OSC_frequancy_Value=GX_OSCILLATE_FREQ*( GX_PLL_M_VALUE+1)/((GX_PLL_N_VALUE+1)*(GX_PLL_L_VALUE+1))/2;
	}
	else
	{
		OSC_frequancy_Value = GX_OSCILLATE_FREQ;       // KHz
	}
	temp=OSC_frequancy_Value*250;  

	UCtmp =GX_Write_one_Byte(state, GX_FSAMPLE_H,((int)((temp>>16)&0xff)));       //0x40
	if (SUCCESS == UCtmp)
	{
		UCtmp = GX_Write_one_Byte(state, GX_FSAMPLE_M,((int)((temp>>8)&0xff)));   //0x41

		if (SUCCESS == UCtmp)
		{
			UCtmp = GX_Write_one_Byte(state, GX_FSAMPLE_L,((int)( temp&0xff)));   //0x42
		}
	}
	return UCtmp;
}

/*
Function:  Init the GX1001
Output:
SUCCESS or FAILURE
*/
GX_STATE GX_Init_Chip(struct gx1001_state *state)
{ 
	int UCtmp=FAILURE;

	UCtmp = GX_Set_AGC_Parameter(state); //Set AGC parameter
	if (SUCCESS == UCtmp)
	{
		UCtmp = GX_SetOSCFreq(state);                        /* set crystal frequency */  
		if (SUCCESS == UCtmp)
		{
			UCtmp = GX_Set_OutputMode(state, GX_TS_OUTPUT_MODE);    /* set the TS output mode */
		}		
	}
	
	if( ( NEWONE == GX_Get_Version(state))&&( SUCCESS == UCtmp ) )
	{
		UCtmp = GX_Set_Digital_AGC(state, ENABLE);
		if(UCtmp==FAILURE)return UCtmp;
		UCtmp = GX_Set_RF_Min(state, GX_RF_AGC_MIN_VALUE);
		if(UCtmp==FAILURE)return UCtmp;
		UCtmp = GX_Set_RF_Max(state, GX_RF_AGC_MAX_VALUE);	
		if(UCtmp==FAILURE)return UCtmp;
		UCtmp = GX_set_IF_Min(state, GX_IF_AGC_MIN_VALUE);
		if(UCtmp==FAILURE)return UCtmp;
		UCtmp = GX_set_IF_Max(state, GX_IF_AGC_MAX_VALUE);
		if(UCtmp==FAILURE)return UCtmp;
		UCtmp = GX_Set_Pll_Value(state, GX_PLL_M_VALUE,GX_PLL_N_VALUE,GX_PLL_L_VALUE);	
		if(UCtmp==FAILURE)return UCtmp;
	}
	return UCtmp;
}

/* 
Function: Hot reset the Chip 
Output:
        SUCCESS or FAILURE 
*/
GX_STATE GX_HotReset_CHIP(struct gx1001_state *state)
{
	int UCtmp = FAILURE;
	int temp;

	temp=GX_Read_one_Byte(state, GX_MAN_PARA);

	if (temp != FAILURE)
	{
		temp|=0x02;
		UCtmp = GX_Write_one_Byte_NoReadTest(state, GX_MAN_PARA,temp);
	}

	return UCtmp;
}



/* 
Function: Cool reset the Chip 
Output:
        SUCCESS or FAILURE 
*/
GX_STATE GX_CoolReset_CHIP(struct gx1001_state *state)
{
	int UCtmp = FAILURE;
	int temp;
	
	temp=GX_Read_one_Byte(state, GX_MAN_PARA);
	if (temp != FAILURE)
	{
		temp|=0x08;
		UCtmp = GX_Write_one_Byte_NoReadTest(state, GX_MAN_PARA,temp);
	}

	return UCtmp;
}



/* 
Function: Read EQU OK
Output:
        SUCCESS - EQU OK, FAILURE - EQU Fail
*/
GX_STATE GX_Read_EQU_OK(struct gx1001_state *state)
{
	int Read_temp=0;

	Read_temp=GX_Read_one_Byte(state, GX_STATE_IND);         /*0x13*/
	if (Read_temp != FAILURE)
	{
		if ((Read_temp&0xe0)==0xe0) 
		{
			return SUCCESS;
		}	
	}
	return FAILURE;
}

/* 
Function: Read ALL OK
Output:
        SUCCESS - all ok, FAILURE - not all ok 
*/
GX_STATE GX_Read_ALL_OK(struct gx1001_state *state)
{
	int Read_temp=0;
	
	Read_temp=GX_Read_one_Byte(state, GX_STATE_IND);         /*0x13*/
	if (Read_temp != FAILURE)
	{
		if ((Read_temp&0xf1)==0xf1)                 /*DVB-C : 0xF1*/
				return SUCCESS;
	}
	
	return FAILURE;
}


/* Function: Enable/Disable the Tunner repeater
Input:	
        1 - On
		0 - Off
Output:
        SUCCESS or FAILURE 
*/
GX_STATE GX_Set_Tunner_Repeater_Enable(struct gx1001_state *state, int OnOff)
{
	int UCtmp = FAILURE;
	int Read_temp;

	Read_temp=GX_Read_one_Byte(state, GX_MAN_PARA);

	if (Read_temp != FAILURE)
	{
		if(OnOff)
		{
			Read_temp|=0x40;        /*Open*/
		}
		else
		{
			Read_temp&=0xbf;        /*Close*/
		}
		
		UCtmp = GX_Write_one_Byte(state, GX_MAN_PARA,Read_temp);
	}

	return UCtmp;
}



//==============================================================================================

/*
Function:   convert a integer to percentage ranging form 0% to 100%  
Input:
        value - integer
        low   - lower limit of input,corresponding to 0%  .if value <= low ,return 0
        high  - upper limit of input,corresponding to 100%.if value >= high,return 100
Output:
        0~100 - percentage
*/
unsigned char GX_Change2percent(int value,int low,int high)
{
    unsigned char temp=0;
    if (value<=low) return 0;
    if (value>=high) return 100;
    temp = (unsigned char)((value-low)*100/(high-low));
    return temp;
}


/* 
Function:   100LogN calculating function 
Output:
        = 100LogN
*/
int GX_100Log(int iNumber_N)
{
	int iLeftMoveCount_M=0;
	int iChangeN_Y=0;
	int iBuMaY_X=0;
	int iReturn_value=0;
	long iTemp=0,iResult=0,k=0;

	iChangeN_Y=iNumber_N;
	
	for (iLeftMoveCount_M=0;iLeftMoveCount_M<16;iLeftMoveCount_M++)
	{
		if ((iChangeN_Y&0x8000)==0x8000)
			break;
		else
		{
			iChangeN_Y=iNumber_N<<iLeftMoveCount_M;
		}
	}

	iBuMaY_X=0x10000-iChangeN_Y;	//get 2's complement

	k=(long)iBuMaY_X*10000/65536;

	//iTemp= k+(1/2)*(k*k)+(1/3)*(k*k*k)+(1/4)*(k*k*k*k)
	iTemp = k + (k*k)/20000 + ((k*k/10000)*(k*33/100))/10000 + ((k*k/100000)*(k*k/100000))/400;

	//iResult=4.816480-(iTemp/2.302585);
	iResult=48165-(iTemp*10000/23025);	//4.8165 = lg2^16

	k=iResult-3010*(iLeftMoveCount_M-1);
	
	iReturn_value=(k/100);   //magnify logN by 100 times
	
	return iReturn_value;
}       
        
        
        
/*
Function : get the signal quality expressed in percentage
Output:
        The SNR value (range is [0,100])   ( 0 express SNR = 5 dB ,  100  express SNR = 35 dB )
*/
int GX_Get_SNR(struct gx1001_state *state)
{       
	int S_N_value=0,read_temp=0;
	int read_temp1=0;
	int read_temp2=0;

	if (GX_Read_ALL_OK(state)==SUCCESS)
	{
		read_temp1 =( GX_Read_one_Byte(state, GX_SUM_ERR_POW_L)&0xff);
		read_temp2 =( GX_Read_one_Byte(state, GX_SUM_ERR_POW_H)&0xff);
		if ((read_temp1>0)||(read_temp2>0))
		{
			read_temp = read_temp1 + (read_temp2<<8);       //SN= 49.3-10log(read_temp) 
			S_N_value = 493 - GX_100Log(read_temp);         //magnifid by 10 times
			return GX_Change2percent(S_N_value,50,350);	
		}
	}
	return 0;
}       
 
//========================================================================================================================




/*
Function: Set spectrum invert
Input:   
       Spec_invert         : 1 - Yes, 0 - No.
Output:SUCCESS or FAILURE 
*/
GX_STATE GX_SetSpecInvert(struct gx1001_state *state, int Spec_invert)
{
	int write_value=0;
	unsigned OSC_frequancy_Value=0;
	unsigned Carrier_center			=	GX_IF_FREQUENCY;       

	if( NEWONE==GX_Get_Version(state) )	
	{	
		OSC_frequancy_Value=GX_OSCILLATE_FREQ*( GX_PLL_M_VALUE+1)/((GX_PLL_N_VALUE+1)*(GX_PLL_L_VALUE+1))/2;
	}
	else
	{
		OSC_frequancy_Value = GX_OSCILLATE_FREQ;       // KHz
	}   

	if (Carrier_center<OSC_frequancy_Value)
	{
	      if (Spec_invert)
	              write_value=(int)(((OSC_frequancy_Value-Carrier_center)*1000)/1024);
	      else
	              write_value=(int)((Carrier_center*1000)/1024);
	}
	else
	{
	       if (Spec_invert)
	               write_value=(int)((((2*OSC_frequancy_Value-Carrier_center)*1000)/1024));
	       else
	               write_value=(int)(((Carrier_center-OSC_frequancy_Value)*1000)/1024);
	}
	
	if (SUCCESS == GX_Write_one_Byte(state, GX_DCO_CENTER_H,(((write_value>>8)&0xff))))           //0x26
	{
		if (SUCCESS ==	GX_Write_one_Byte(state, GX_DCO_CENTER_L,(( write_value&0xff))))      //0x27
			return SUCCESS;
	}
	return FAILURE;
}


//========================================================================================================================


/*
Function: 	Get Error Rate value 
Input:		
			*E_param: for get the exponent of E
Output:
          	FAILURE:    Read Error
          	other: 		Error Rate Value
          	
Example:	if  return value = 456 and  E_param = 3 ,then here means the Error Rate value is : 4.56 E-3
*/
int GX_Get_ErrorRate(struct gx1001_state *state)
{
	int flag = 0;
	int e_value = 0;
	int return_value = 0;
	int temp=0;
	unsigned char Read_Value[4];
	unsigned long Error_count=0;
	int i=0;
	unsigned long divied = 53477376;	//(2^20 * 51)


	if (GX_Read_ALL_OK(state) == FAILURE)
	{
		return 0;
	}
	
	for (i=0;i<4;i++)	//Read Error Count
	{
		flag = GX_Read_one_Byte(state, GX_ERR_SUM_1 + i);
		if (FAILURE == flag)
		{
			return 0;
		}
		else
		{
			Read_Value[i] = (unsigned char)flag;
		}
	}
	Read_Value[3] &= 0x03;
	Error_count = (unsigned long)(Read_Value[0]+ (Read_Value[1]<<8) + (Read_Value[2]<<16) + (Read_Value[3]<<24));
	
	//ERROR RATE = (ERROR COUNT) /£¨ ( 2^(2* 5 +5))*204*8£©    //bit      
	
	for (i=0;i<20;i++)
	{
		temp = Error_count/divied;

		if (temp)
		{
			return_value = Error_count/(divied/100);
			break;
		}
		else
		{
			e_value +=1;
			Error_count *=10;
		}
	}
	return return_value;
}

/*
Function:   get the signal intensity expressed in percentage
Output:
        The signal Strength value  ( Range is [0,100] )
*/
unsigned char GX_Get_Signal_Strength(struct gx1001_state *state)
{
    unsigned int iAGC1_word=300,iAGC2_word=300,Amp_Value;
    unsigned int agc1_temp=0,agc2_temp=0;
    
     //the following parameters are specific for certain tuner
    int C0=95;
    int C1=0xb2,	A1=20;
    int C2=204,		A2=0;
    int C3=0x8c,	A3=20;
    int C4=179,		A4=0;
    //-----------------------------------------------
        
	int i=0;

	while (i<40)
	{
		agc1_temp =GX_Read_one_Byte(state, GX_AGC1_CTRL);
		agc2_temp =GX_Read_one_Byte(state, GX_AGC2_CTRL);

		
		if ((agc1_temp>0)&&(agc2_temp>0))
		{

			if ((((agc1_temp - iAGC1_word)<5)||((agc1_temp - iAGC1_word)>-5))&&(((agc2_temp - iAGC2_word)<5)||((agc2_temp - iAGC2_word)>-5)))
			{
				break;
			}
			
			iAGC1_word = agc1_temp;
			iAGC2_word = agc2_temp;
		}

		GX_Delay_N_10ms(1);
		i++;

	}

	if (i>=40) 
	{
		iAGC1_word =GX_Read_one_Byte(state, GX_AGC1_CTRL);
		iAGC2_word =GX_Read_one_Byte(state, GX_AGC2_CTRL);
	}

	if (iAGC1_word > 0xe4) iAGC1_word = 0xe4;
	Amp_Value = C0 - ((iAGC1_word-C1)*(A1-A2))/(C2-C1) - ((iAGC2_word-C3)*(A3-A4))/(C4-C3);
	return GX_Change2percent(Amp_Value,0,100);

}  

uint  GetTunerStatus(struct gx1001_state *state)
{
	unsigned char bRead =0;
	
#if TUNER_TYPE == TUNER_DCT70708	
	/*write dct70708*/
	GX_Set_Tunner_Repeater_Enable(state, 1);	/*For open the chip repeater function*/
	GX_Delay_N_10ms(1);
	Tuner_ReadRegister(state, &bRead,1);
	
	GX_Delay_N_10ms(1);
	GX_Set_Tunner_Repeater_Enable(state, 0);	/*For close the chip repeater function*/	
	GX_Delay_N_10ms(1);//by maj. orig is OS_Sleep(20);
#endif

	return bRead ;


}

/***********************************************/

AM_ErrorCode_t demod_init(struct gx1001_state *state)
{
	state->i2c = i2c_get_adapter(state->config.i2c_id);
	if (!state->i2c) {
		pr_error("cannot get i2c adaptor\n");
	}
	
#if TUNER_TYPE == TUNER_DCT70708
	gx1001_reset(state);						// need reset demod on our board					
#endif
	
	return DEMOD_SUCCESS;
}

AM_ErrorCode_t demod_deinit(struct gx1001_state *state)
{
	if(state->i2c)
	{
		i2c_put_adapter(state->i2c);
	}
	
#if TUNER_TYPE == TUNER_DCT70708
	gx1001_reset(state);						// need reset demod on our board					
#endif
	return DEMOD_SUCCESS;
}

AM_ErrorCode_t demod_check_locked(struct gx1001_state *state, unsigned char* lock)
{
	if(lock == 0)
		return DEMOD_ERR_BAD_PARAMETER;
	
	if(GX_Read_ALL_OK(state) == SUCCESS)
		*lock = 1;
	else
		*lock = 0;

	return DEMOD_SUCCESS;
}
AM_ErrorCode_t demod_connect(struct gx1001_state *state, unsigned int freq_khz, unsigned char qam_size, unsigned int sr_ks)
{
	int search_return = 0;
	
	int GX_QamSize;
	
	switch (qam_size)
	{
		case QAM_16:
		GX_QamSize = 3;
		break;
		case QAM_32:
		GX_QamSize = 4;
		break ;
		case  QAM_64:
		GX_QamSize = 5;
		break;
		case  QAM_128:
		GX_QamSize = 6;
		break ;
		default:	//default 256
		GX_QamSize = 7;
		break;
	};
	search_return = GX_Search_Signal(state, sr_ks, 0, 2, GX_QamSize, freq_khz, 700);
	
	if (SUCCESS == search_return) 	// have sinal	
	{	
		pr_dbg("search signal success\r\n");
		return DEMOD_SUCCESS;
	}		
	else
	{	
		pr_dbg("Search signal failed !!\n");
		return DEMOD_ERR_OTHER;		// no signal 
	}	

}

AM_ErrorCode_t demod_disconnect(struct gx1001_state *state)
{
	return DEMOD_SUCCESS;
}

AM_ErrorCode_t demod_get_signal_strength(struct gx1001_state *state, unsigned int* strength)
{
	if(strength == 0)
		return DEMOD_ERR_BAD_PARAMETER;

	*strength = GX_Get_Signal_Strength(state);

	return DEMOD_SUCCESS;
}

AM_ErrorCode_t demod_get_signal_quality(struct gx1001_state *state, unsigned int* quality)
{
	if(quality == 0)
		return DEMOD_ERR_BAD_PARAMETER;
	
	*quality = GX_Get_SNR(state);

	return DEMOD_SUCCESS;
	
}

uint demod_get_signal_errorate(struct gx1001_state *state, unsigned int* errorrate)
{
	if(errorrate == 0)
		return DEMOD_ERR_BAD_PARAMETER;

	*errorrate = GX_Get_ErrorRate(state);

	return DEMOD_SUCCESS;
}

/***********************************************/
