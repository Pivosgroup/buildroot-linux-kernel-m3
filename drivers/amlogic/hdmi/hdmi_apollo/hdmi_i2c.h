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
#ifndef _HDMI_I2C_HEADER
#define _HDMI_I2C_HEADER
 /*----------------------------------------------------------------------*/ 
struct i2c_adapter *hdmi_i2c_init_adapter(void);
int hdmi_i2c_release_adapter(struct i2c_adapter *i2c);
void hdmi_set_i2c_device_id(int id);
unsigned char ReadByteHDMITX_CAT(unsigned addr);
unsigned short ReadWordHDMITX_CAT(unsigned addr);
void ReadBlockHDMITX_CAT(unsigned char addr, unsigned short len,
			  unsigned char *bufer);
void WriteByteHDMITX_CAT(unsigned addr, unsigned char data);
void WriteWordHDMITX_CAT(unsigned addr, unsigned short data);
void WriteBlockHDMITX_CAT(unsigned char addr, unsigned short len,
			   unsigned char *bufer);

#endif				//_HDMI_I2C_HEADER
