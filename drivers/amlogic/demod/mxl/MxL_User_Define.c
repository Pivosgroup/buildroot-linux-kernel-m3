/*
 
 Driver APIs for MxL5007 Tuner
 
 Copyright, Maxlinear, Inc.
 All Rights Reserved
 
 File Name:      MxL_User_Define.c

 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//									   //
//	I2C Functions (implement by customer)				   //
//									   //
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include "../aml_demod.h"
#include "../demod_func.h"

/******************************************************************************
**
**  Name: MxL_I2C_Write
**
**  Description:    I2C write operations
**
**  Parameters:    	
**	DeviceAddr	- MxL5007 Device address
**	pArray		- Write data array pointer
**	count		- total number of array
**
**  Returns:        0 if success
**
**  Revision History:
**
**   SCR      Date      Author  Description
**  -------------------------------------------------------------------------
**   N/A   12-16-2007   khuang initial release.
**
******************************************************************************/
int MxL_I2C_Write(u32 I2C_adap, char* pArray, int count)
{
    int ret;
    struct i2c_msg msg;
    struct aml_demod_i2c *adap = (struct aml_demod_i2c *)I2C_adap;

    msg.addr = adap->addr;
    msg.flags = 0;
    msg.len = count;
    msg.buf = pArray;
    
    ret = am_demod_i2c_xfer(adap, &msg, 1);
    
    return (!ret);
}

/******************************************************************************
**
**  Name: MxL_I2C_Read
**
**  Description:    I2C read operations
**
**  Parameters:    	
**	DeviceAddr	- MxL5007 Device address
**	Addr		- register address for read
**	*Data		- data return
**
**  Returns:        0 if success
**
**  Revision History:
**
**   SCR      Date      Author  Description
**  -------------------------------------------------------------------------
**   N/A   12-16-2007   khuang initial release.
**
******************************************************************************/
int MxL_I2C_Read(u32 I2C_adap, int Addr, char* mData)
{
    int ret;
    unsigned char data[2];
    struct i2c_msg msg[2];
    struct aml_demod_i2c *adap = (struct aml_demod_i2c *)I2C_adap;

    data[0] = 0xfb;
    data[1] = Addr&0xff;

    msg[0].addr = adap->addr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf = data;

    msg[1].addr = adap->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = mData;
    
    ret = am_demod_i2c_xfer(adap, msg, 2);

    return (!ret);
}

/******************************************************************************
**
**  Name: MxL_Delay
**
**  Description:    Delay function in milli-second
**
**  Parameters:    	
**	mSec		- milli-second to delay
**
**  Returns:        0
**
**  Revision History:
**
**   SCR      Date      Author  Description
**  -------------------------------------------------------------------------
**   N/A   12-16-2007   khuang initial release.
**
******************************************************************************/
void MxL_Delay(int mSec)
{
    mdelay(mSec);
}


