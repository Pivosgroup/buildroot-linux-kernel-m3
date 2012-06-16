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
    
#include <linux/kernel.h>
    
#ifndef __HDMI_DEBUG_H
#define __HDMI_DEBUG_H
    
//#define DEBUG
    
#define DEBUG_ALL				0xff
#define DEBUG_DEBUG			2
#define DEBUG_WARING				1
    
#define HDMI_PRINT(fmt,args...)	printk(fmt,## args);
    
#ifdef DEBUG
extern int hdmi_debug_level;

#define _HDMI_DBG(fmt,args...)	HDMI_PRINT(fmt,## args);
#define _HDMI_LDBG(l,fmt,args...)	\
    do {\
        if (l <= hdmi_debug_level) \
        { _HDMI_DBG(fmt,## args);}\
    } while (0) 
#else	/*  */
    
#define _HDMI_DBG(args...)	
#define _HDMI_LDBG(l,args...)
    
#endif	/*  */
    
#define HDMI_ERR(fmt,s...)			HDMI_PRINT(KERN_ERR "[HDMI-ERR]" fmt, ## s)
    
#define HDMI_INFO(fmt,s...)			HDMI_PRINT(KERN_INFO "[HDMI-INFO]" fmt, ## s)
    
#define HDMI_WARING(fmt,s...)		_HDMI_LDBG(DEBUG_WARING ,KERN_ERR "[HDMI-WAR]" fmt,## s)
    
#define HDMI_DEBUG(fmt,s...)		_HDMI_LDBG(DEBUG_DEBUG,KERN_ERR  "[HDMI-DBG]" fmt,## s)
    
#define hdmi_dbg_print(fmt,args...) _HDMI_DBG(fmt,##args)
static void inline dump(unsigned char *p, int len)
	
{
int i, j;
char s[20];
for (i = 0; i < len; i += 16)
	 {
	printk("%08x:", (unsigned int)p);
	for (j = 0; j < 16 && j < len - 0 * 16; j++)
		 {
		s[j] = (p[j] > 15 && p[j] < 128) ? p[j] : '.';
		printk(" %02x", p[j]);
		}
	s[j] = 0;
	printk(" |%s|\n", s);
	p = p + 16;
	}
}


#endif				//__HDMI_DEBUG_H
    
