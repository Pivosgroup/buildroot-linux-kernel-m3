/*
 *           Copyright 2012 Availink, Inc.
 *
 *  This software contains Availink proprietary information and
 *  its use and disclosure are restricted solely to the terms in
 *  the corresponding written license agreement. It shall not be 
 *  disclosed to anyone other than valid licensees without
 *  written permission of Availink, Inc.
 *
 */


///$Date: 2012-2-9 17:36 $
///
///
/// @file
/// @brief Implements the functions declared in IBSP.h. 
/// 
#include "IBSP.h"
#include <linux/semaphore.h>
#include <linux/delay.h>


/// The following table illustrates a set of PLL configuration values to operate AVL6211 in two modes:
// Standard performance mode.
// High performance mode

/// Please refer to the AVL6211 channel receiver datasheet for detailed information on highest symbol rate 
/// supported by the demod in both these modes.

///For more information on other supported clock frequencies and PLL settings for higher symbol rates, please 
///contact Availink.

/// Users can remove unused elements from the following array to reduce the SDK footprint size.
const struct AVL_DVBSx_PllConf pll_conf[] =
{
	// The following set of PLL configuration at different reference clock frequencies refer to demod operation
	// in standard performance mode.
	 {503,  1, 7, 4, 2,  4000, 11200, 16800, 25200} ///< Reference clock 4 MHz,   Demod clock 112 MHz, FEC clock 168 MHz, MPEG clock 252 MHz
	,{447,  1, 7, 4, 2,  4500, 11200, 16800, 25200} ///< Reference clock 4.5 MHz, Demod clock 112 MHz, FEC clock 168 MHz, MPEG clock 252 MHz
	,{503,  4, 7, 4, 2, 10000, 11200, 16800, 25200} ///< Reference clock 10 MHz,  Demod clock 112 MHz, FEC clock 168 MHz, MPEG clock 252 MHz
	,{503,  7, 7, 4, 2, 16000, 11200, 16800, 25200} ///< Reference clock 16 MHz,  Demod clock 112 MHz, FEC clock 168 MHz, MPEG clock 252 MHz
	,{111,  2, 7, 4, 2, 27000, 11200, 16800, 25200} ///< Reference clock 27 MHz,  Demod clock 112 MHz, FEC clock 168 MHz, MPEG clock 252 MHz
	
	// The following set of PLL configuration at different reference clock frequencies refer to demod operation
	// in high performance mode. 
	,{566,  1, 7, 4, 2,  4000, 12600, 18900, 28350} /// < Reference clock 4 MHz,   Demod clock 126 MHz, FEC clock 189 MHz, MPEG clock 283.5 MHz
	,{503,  1, 7, 4, 2,  4500, 12600, 18900, 28350} ///< Reference clock 4.5 MHz, Demod clock 126 MHz, FEC clock 189 MHz, MPEG clock 283.5 MHz
	,{566,  4, 7, 4, 2, 10000, 12600, 18900, 28350} ///< Reference clock 10 MHz,  Demod clock 126 MHz, FEC clock 189 MHz, MPEG clock 283.5 MHz
	,{566,  7, 7, 4, 2, 16000, 12600, 18900, 28350} ///< Reference clock 16 MHz,  Demod clock 126 MHz, FEC clock 189 MHz, MPEG clock 283.5 MHz
	,{377,  8, 7, 4, 2, 27000, 12600, 18900, 28350} ///< Reference clock 27 MHz,  Demod clock 126 MHz, FEC clock 189 MHz, MPEG clock 283.5 MHz

};

const AVL_uint16 pll_array_size = sizeof(pll_conf)/sizeof(struct AVL_DVBSx_PllConf);

AVL_DVBSx_ErrorCode AVL_DVBSx_IBSP_Initialize(void)
{
	return(AVL_DVBSx_EC_OK);
}

AVL_DVBSx_ErrorCode AVL_DVBSx_IBSP_Dispose(void)
{
	return(AVL_DVBSx_EC_OK);
}

AVL_DVBSx_ErrorCode AVL_DVBSx_IBSP_Delay( AVL_uint32 uiMS )
{
	msleep(uiMS);
	return(AVL_DVBSx_EC_OK);
}

AVL_DVBSx_ErrorCode AVL_DVBSx_IBSP_I2CRead(  const struct AVL_DVBSx_Chip * pAVLChip,  AVL_puchar pucBuff, AVL_puint16 puiSize )
{
	AVL_uint16 I2CSlaveAddr;
	AVL_puchar data;
	AVL_puint16 length;
	I2CSlaveAddr=pAVLChip->m_SlaveAddr;
	data = pucBuff;
	length = *puiSize;
	if(I2CRead(I2CSlaveAddr,data, length)==0)
		return AVL_DVBSx_EC_I2CFail;
	//printk("data is %x,%x\n",data[0],data[1]);
	return(AVL_DVBSx_EC_OK);
}
AVL_DVBSx_ErrorCode AVL_DVBSx_IBSP_I2CWrite(  const struct AVL_DVBSx_Chip * pAVLChip,  AVL_puchar pucBuff,  AVL_puint16  puiSize )
{
	AVL_uint16 I2CSlaveAddr;
	AVL_puchar data;
	AVL_puint16 length;
	I2CSlaveAddr=pAVLChip->m_SlaveAddr;
	data = pucBuff;
	length = *puiSize;
	if(I2CWrite(I2CSlaveAddr,data, length)==0)
		return AVL_DVBSx_EC_I2CFail;
	return(AVL_DVBSx_EC_OK);
}

AVL_DVBSx_ErrorCode AVL_DVBSx_IBSP_InitSemaphore( AVL_psemaphore pSemaphore )
{
	init_MUTEX(pSemaphore);
	return(AVL_DVBSx_EC_OK);
}

AVL_DVBSx_ErrorCode AVL_DVBSx_IBSP_WaitSemaphore( AVL_psemaphore pSemaphore )
{
	if(down_interruptible(pSemaphore))
		return -AVL_DVBSx_EC_GeneralFail;
	return(AVL_DVBSx_EC_OK);
}

AVL_DVBSx_ErrorCode AVL_DVBSx_IBSP_ReleaseSemaphore( AVL_psemaphore pSemaphore )
{
	up(pSemaphore);
	return(AVL_DVBSx_EC_OK);
}


extern int avl6211_get_fe_config(struct avl6211_fe_config *cfg);


extern AVL_int32 I2CWrite(AVL_uchar I2CSlaveAddr, AVL_uchar *data, AVL_int32 length)
 {
//	printk("\n[I2CWrite] enter I2CSlaveAddr is %x,length is %d,data is %x, %x,%x\n",I2CSlaveAddr,length,data[0],data[1],data[2]);
//	printk("I2CSlaveAddr is %d\n",I2CSlaveAddr);
   /* I2C write, please port this function*/
   AVL_int32 ret = 0;
//	unsigned char regbuf[1];			/*8 bytes reg addr, regbuf 1 byte*/
	struct i2c_msg msg;			/*construct 2 msgs, 1 for reg addr, 1 for reg value, send together*/

//	regbuf[0] = I2CSlaveAddr & 0xff;

	memset(&msg, 0, sizeof(msg));

	/*write reg address*/
/*	msg[0].addr = (state->config.demod_addr);					
	msg[0].flags = 0;
	msg[0].buf = regbuf;
	msg[0].len = 1;*/


	/*write value*/
	msg.addr = I2CSlaveAddr;
	msg.flags = 0;  //I2C_M_NOSTART;	/*i2c_transfer will emit a stop flag, so we should send 2 msg together,
																// * and the second msg's flag=I2C_M_NOSTART, to get the right timing*/
	msg.buf = data;
	msg.len = length;
#if 0

	/*write reg address*/
	msg[0].addr = 0x80;					
	msg[0].flags = 0;
	msg[0].buf = 0x7;
	msg[0].len = 1;

	/*write value*/
	msg[1].addr = 0x80;
	msg[1].flags = I2C_M_NOSTART;	/*i2c_transfer will emit a stop flag, so we should send 2 msg together,
																 * and the second msg's flag=I2C_M_NOSTART, to get the right timing*/
	msg[1].buf = 0x8;
	msg[1].len = 1;

#endif

	struct	avl6211_fe_config cfg;
//	int i2c_id = -1;
	/*cfg->demod_addr=0;
	cfg->tuner_addr=0;
	cfg->i2c_id=0;
	cfg->reset_pin=0;*/
	avl6211_get_fe_config(&cfg);
	/*i2c_handle = i2c_get_adapter(i2c_id,i2c_handle);
	if (!i2c_handle) {
		printk("cannot get i2c adaptor\n");
		return 0;
	}*/
	ret = i2c_transfer((struct i2c_adapter *)cfg.i2c_adapter, &msg, 1);
	if(ret<0){
		printk(" %s: writereg error, errno is %d \n", __FUNCTION__, ret);
		return 0;
	}
	else{
		//printk(" %s:write success, errno is %d \n", __FUNCTION__, ret);
		return 1;
	}
      return 1;
 }
 
extern AVL_int32  I2CRead(AVL_uchar I2CSlaveAddr, AVL_uchar *data, AVL_int32 length)
 {
     /* I2C read, please port this function*/
	//	printk("I2CSlaveAddr is %d,length is %d\n",I2CSlaveAddr,length);
//		printk("I2CSlaveAddr is %d\n",I2CSlaveAddr);
		 AVL_uint32 nRetCode = 0;
		 struct i2c_msg msg[1];
		 
		 if(data == 0 || length == 0)
		 {
			 printk("avl6211 read register parameter error !!\n");
			 return 0;
		 }
	 
		 //read real data 
		 memset(msg, 0, sizeof(msg));
		 msg[0].addr = I2CSlaveAddr;
		 msg[0].flags |=  I2C_M_RD;  //write  I2C_M_RD=0x01
		 msg[0].len = length;
		 msg[0].buf = data;


		struct	avl6211_fe_config cfg;
		avl6211_get_fe_config(&cfg);
	//	printk("\n[I2CRead] get i2c_adapter");
	/*	i2c_handle = i2c_get_adapter(i2c_id);
		if (!i2c_handle) {
			printk("cannot get i2c adaptor\n");
		}*/
	
		 
		 nRetCode = i2c_transfer((struct i2c_adapter *)cfg.i2c_adapter, msg, 1);
	 
		 if(nRetCode != 1)
		 {
			 printk("avl6211_readregister reg failure!\n");
			 return 0;
		 }
        return 1;
 }

