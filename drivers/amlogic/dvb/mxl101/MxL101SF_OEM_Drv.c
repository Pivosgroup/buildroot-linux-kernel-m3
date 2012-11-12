/*******************************************************************************
 *
 * FILE NAME          : MxL101SF_OEM_Drv.cpp
 * 
 * AUTHOR             : Brenndon Lee
 * DATE CREATED       : 1/24/2008
 *
 * DESCRIPTION        : This file contains I2C emulation routine
 *                      
 *******************************************************************************
 *                Copyright (c) 2006, MaxLinear, Inc.
 ******************************************************************************/

#include "MxL101SF_OEM_Drv.h"
//#include "MsTypes.h"
//#include "drvIIC.h"

/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_ReadRegister
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|                 8/22/2008
--|
--| DESCRIPTION   : This function reads register data of the provided-address
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS Ctrl_ReadRegister(UINT8 regAddr, UINT8 *dataPtr)
{
  MXL_STATUS status = MXL_TRUE;

  // User should implememnt his own I2C read register routine corresponding to
  // his hardaware.
  
  //printf("MxL Reg Read : Addr - 0x%x, data - 0x%x\n", regAddr, *dataPtr);   
  //i2cSlaveAddr<<1;
  //i2cSlaveAddr|=0x1;
  //UINT8 i2cSlaveAddr = 0xC1;
  UINT8 i2cSlaveAddr = 0x60;
  UINT8 u8Reg[2];
  u8Reg[0] = 0xFB;
  u8Reg[1] = regAddr;
  I2CWrite(i2cSlaveAddr, u8Reg, 2);
  if(I2CRead(i2cSlaveAddr, dataPtr, 1)==0)
  {
		status = MXL_FALSE;
  }
 /*  if(MDrv_IIC_Read(i2cSlaveAddr,u8Reg,2,dataPtr,1)==FALSE)
  {
	  	status = MXL_FALSE;
  }	*/
  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_WriteRegister
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|
--| DESCRIPTION   : This function writes the provided value to the specified
--|                 address.
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS Ctrl_WriteRegister(UINT8 regAddr, UINT8 regData)
{
  MXL_STATUS status = MXL_TRUE;

  // User should implememnt his own I2C write register routine corresponding to
  // his hardaware.
  
 // printf("MxL Reg Write : Addr - 0x%x, data - 0x%x\n", regAddr, regData);    
  //i2cSlaveAddr<<1;
  //i2cSlaveAddr &= 0xFE; 
//  UINT8 i2cSlaveAddr = 0xC0;
  UINT8 i2cSlaveAddr = 0x60;
  UINT8 u8Reg[2];
  u8Reg[0] = regAddr;
  u8Reg[1] = regData;
//  I2CWrite(i2cSlaveAddr, &regAddr, 1);
  if(I2CWrite(i2cSlaveAddr, u8Reg, 2)==0)
  {
		status = MXL_FALSE;
  }
/*  if(MDrv_IIC_Write(i2cSlaveAddr,&regAddr,1,&regData,1)==FALSE)
  {
	  	status = MXL_FALSE;
  }*/
 
  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_Sleep
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 8/31/2009
--|
--| DESCRIPTION   : This function will cause the calling thread to be suspended 
--|                 from execution until the number of milliseconds specified by 
--|                 the argument time has elapsed 
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS Ctrl_Sleep(UINT16 TimeinMilliseconds)
{
  MXL_STATUS status = MXL_TRUE;

  // User should implememnt his own sleep routine corresponding to
  // his Operating System platform.
	udelay(TimeinMilliseconds);
  //printf("Ctrl_Sleep : %d msec's\n", Time);    
 // SysDelay(TimeinMilliseconds);
  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_GetTime
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 10/05/2009
--|
--| DESCRIPTION   : This function will return current system's timestamp in 
--|                 milliseconds resolution. 
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

extern unsigned int jiffies_to_msecs(const unsigned long j);

MXL_STATUS Ctrl_GetTime(UINT32 *TimeinMilliseconds)
{
  //*TimeinMilliseconds = current systems timestamp in milliseconds.
  // User should implement his own routine to get current system's timestamp in milliseconds.
//  *TimeinMilliseconds = MsOS_GetSystemTime();
//	printk("Ctrl_GetTime\n");
	*TimeinMilliseconds=jiffies_to_msecs(jiffies);
//	printk("Ctrl_GetTime,TimeinMilliseconds is %d\n",*TimeinMilliseconds);
  return MXL_TRUE;
}


extern int mxl101_get_fe_config(struct mxl101_fe_config *cfg);


extern int I2CWrite(UINT8 I2CSlaveAddr, UINT8 *data, int length)
 {
//	printk("\n[I2CWrite] enter I2CSlaveAddr is %x,length is %d,data[0] is %x,data[1] is %x\n",I2CSlaveAddr,length,data[0],data[1]);
   /* I2C write, please port this function*/
   int ret = 0;
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

	struct	mxl101_fe_config cfg;
//	int i2c_id = -1;
	/*cfg->demod_addr=0;
	cfg->tuner_addr=0;
	cfg->i2c_id=0;
	cfg->reset_pin=0;*/
	mxl101_get_fe_config(&cfg);
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
	else
		return 1;
      return 1;
 }
 
extern int  I2CRead(UINT8 I2CSlaveAddr, UINT8 *data, int length)
 {
     /* I2C read, please port this function*/
	// 	printk("\n[I2CRead] enter ");
		 UINT32 nRetCode = 0;
		 struct i2c_msg msg[1];
		 
		 if(data == 0 || length == 0)
		 {
			 printk("mxl101 read register parameter error !!\n");
			 return 0;
		 }
	 
	//	 gx1001_set_addr(state, regaddr) ; //set reg address first
	 
		 //read real data 
		 memset(msg, 0, sizeof(msg));
		 msg[0].addr = I2CSlaveAddr;
		 msg[0].flags |=  I2C_M_RD;  //write  I2C_M_RD=0x01
		 msg[0].len = length;
		 msg[0].buf = data;


		struct	mxl101_fe_config cfg;

//	int i2c_id = -1;
	/*cfg->demod_addr=0;
	cfg->tuner_addr=0;
	cfg->i2c_id=0;
	cfg->reset_pin=0;*/
		mxl101_get_fe_config(&cfg);
	//	printk("\n[I2CRead] get i2c_adapter");
	/*	i2c_handle = i2c_get_adapter(i2c_id);
		if (!i2c_handle) {
			printk("cannot get i2c adaptor\n");
		}*/
	
		 
		 nRetCode = i2c_transfer((struct i2c_adapter *)cfg.i2c_adapter, msg, 1);
	 
		 if(nRetCode != 1)
		 {
			 printk("mxl101_readregister reg failure!\n");
			 return 0;
		 }
        return 1;
 }

