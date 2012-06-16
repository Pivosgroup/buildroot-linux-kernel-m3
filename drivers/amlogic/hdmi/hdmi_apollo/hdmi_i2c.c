/*
 * Amlogic Apollo
 * frame buffer driver
 *	-------hdmi output
 * Copyright (C) 2009 Amlogic, Inc.
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
 * Author:  zhou zhi<zhi.zhou@amlogic.com>
 * Firset add at:2009-7-28
 */  
    
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/major.h>
    
#include <asm/uaccess.h>
    
#include "hdmi_module.h"
#include "hdmi_debug.h"
//#include "hdmi_reg_def.h"
static int hdmi_slave_addr;
static struct i2c_adapter *hdmi_i2c_adapter;
struct i2c_adapter *hdmi_i2c_init_adapter(void) 
{
	
//FIXME:Maybe the id is not equalt 0?
#define I2C_ID 0
	    hdmi_slave_addr = 0x98 >> 1;
	hdmi_i2c_adapter = i2c_get_adapter(I2C_ID);
	return hdmi_i2c_adapter;
}


#define i2c_write_delay() msleep(1)
void hdmi_set_i2c_device_id(int id) 
{
	hdmi_slave_addr = id >> 1;
} int hdmi_i2c_release_adapter(struct i2c_adapter *i2c) 
{
	i2c_put_adapter(i2c);
	return 0;
}
static int hdmi_i2c_read_reg(unsigned char device_addr, unsigned reg) 
{
	u8 b0[] = {
	reg};
	int ret;
	struct i2c_msg msg[] = { {.addr = device_addr,.flags = I2C_M_RD,.buf =
				   b0,.len = 1} };
	
/*---------------------------------------------------------------*/ 
	    ret = i2c_transfer(hdmi_i2c_adapter, msg, 1);
	if (ret != 1)
		 {
		HDMI_ERR(" %s: readreg error (ret == %i)\n", __FUNCTION__,
			  ret);
		return 0xff;
		}
	return b0[0];
}


#if 0
static int hdmi_i2c_read_buf(unsigned char device_addr, unsigned reg,
			     unsigned char *buf, int len) 
{
	int ret;
	struct i2c_msg msg[] = { {.addr = device_addr,.flags = I2C_M_RD,.buf =
				   buf,.len = len} };
	
/*---------------------------------------------------------------*/ 
	    buf[0] = reg;
	ret = i2c_transfer(hdmi_i2c_adapter, msg, 1);
	if (ret != 1)
		HDMI_ERR(" %s: readreg error (ret == %i)\n", __FUNCTION__,
			  ret);
	return 0;
}


#endif	/*  */
static int hdmi_i2c_write_reg(unsigned char device_addr, unsigned reg,
			      int data) 
{
	u8 buf[] = {
	reg, data};
	struct i2c_msg msg = {.addr = device_addr,.flags = 0,.buf = buf,.len =
		    2 };
	int ret;
	
/*---------------------------------------------------------------*/ 
	    ret = i2c_transfer(hdmi_i2c_adapter, &msg, 1);
	if (ret != 1)
		HDMI_ERR(" %s, writereg error " 
			  "(reg == 0x%02x, val == 0x%02x, ret == %i)\n",
			  __FUNCTION__, reg, data, ret);
	i2c_write_delay();
	return (ret != 1) ? -EREMOTEIO : 0;
}


#if 0
static int hdmi_i2c_write_reg_word(unsigned char device_addr, unsigned reg,
				   int data) 
{
	u8 buf[] = {
	reg, data & 0xff, (data >> 8) & 0xff};
	int ret;
	struct i2c_msg msg = {.addr = device_addr,.flags = 0,.buf = buf,.len =
		    3 };
	
/*---------------------------------------------------------------*/ 
	    ret = i2c_transfer(hdmi_i2c_adapter, &msg, 1);
	if (ret != 1)
		HDMI_ERR(" %s, writereg error " 
			  "(reg == 0x%02x, val == 0x%02x, ret == %i)\n",
			  __FUNCTION__, reg, data, ret);
	i2c_write_delay();
	return (ret != 1) ? -EREMOTEIO : 0;
}
static int hdmi_i2c_write_buf(unsigned char device_addr, unsigned reg,
				 unsigned char *buf, int len) 
{
	unsigned char tempbuf[256];
	int ret;
	struct i2c_msg msg[] = { {.addr = device_addr,.flags = 0,.buf =
				   tempbuf,.len = len} };
	
/*---------------------------------------------------------------*/ 
	    tempbuf[0] = reg;
	if (len > 255)
		 {
		HDMI_ERR
		    ("[I2C] Too long data to write to hdmi device,len=%d\n",
		     len);
		return -1;
		}
	memcpy(&tempbuf[1], buf, len);
	ret = i2c_transfer(hdmi_i2c_adapter, msg, 1);
	if (ret != 1)
		HDMI_ERR(" %s: write error (ret == %i)\n", __FUNCTION__, ret);
	i2c_write_delay();
	return (ret != 1) ? -EREMOTEIO : 0;
}


#endif	/*  */
unsigned char ReadByteHDMITX_CAT(unsigned addr) 
{
	return hdmi_i2c_read_reg(hdmi_slave_addr, addr);
}


//---------------------------------------------------------------------
unsigned short ReadWordHDMITX_CAT(unsigned addr) 
{
	unsigned short temp1;
	unsigned char temp2[2];
	
	    //  hdmi_i2c_read_buf(hdmi_slave_addr, addr, temp2, 2 );
	    temp2[0] = hdmi_i2c_read_reg(hdmi_slave_addr, addr);
	temp2[1] = hdmi_i2c_read_reg(hdmi_slave_addr, addr + 1);
	temp1 = (unsigned short)temp2[0] | (temp2[1] << 8);
	return temp1;
}


//---------------------------------------------------------------------
void ReadBlockHDMITX_CAT(unsigned char addr, unsigned short len,
			 unsigned char *bufer) 
{
	
	    // hdmi_i2c_read_buf(hdmi_slave_addr, addr, bufer, len );
	int i;
	for (i = 0; i < len; i++)
		bufer[i] = hdmi_i2c_read_reg(hdmi_slave_addr, addr + i);
}


//--------------------------------------------------------------------- 
void WriteByteHDMITX_CAT(unsigned addr, unsigned char data) 
{
	hdmi_i2c_write_reg(hdmi_slave_addr, addr, data);
	return;
}


//---------------------------------------------------------------------
void WriteWordHDMITX_CAT(unsigned addr, unsigned short data) 
{
	hdmi_i2c_write_reg(hdmi_slave_addr, addr, data & 0xff);
	hdmi_i2c_write_reg(hdmi_slave_addr, addr + 1, data >> 8);
} 

//---------------------------------------------------------------------
void WriteBlockHDMITX_CAT(unsigned char addr, unsigned short len,
			  unsigned char *bufer) 
{
	int i;
	for (i = 0; i < len; i++)
		hdmi_i2c_write_reg(hdmi_slave_addr, addr + i, bufer[i]);
	
	    //hdmi_i2c_write_buf(hdmi_slave_addr, addr, bufer, len );
}


