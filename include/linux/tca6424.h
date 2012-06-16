/* 
 * TCA6424 i2c interface
 * Copyright (C) 2010 Amlogic, Inc.
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
 * Author:  wang han<han.wang@amlogic.com>
 */  

#ifndef __TCA6424_H__
#define __TCA6424_H__

unsigned char get_configIO(unsigned char port);


int configIO(unsigned char port, unsigned char ioflag);


unsigned char get_config_polinv(unsigned char port);


int config_pol_inv(unsigned char port, unsigned char polflag);


unsigned char getIO_level(unsigned char port);


int setIO_level(unsigned char port, unsigned char iobits);

#endif /* __TCA6424_H__ */

