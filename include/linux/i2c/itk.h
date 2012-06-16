/**
 * linux/include/i2c/itk.h
 *
 * Copyright (C) 2007-2008 Avionic Design Development GmbH
 * Copyright (C) 2008-2009 Avionic Design GmbH
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Written by x
 */

#ifndef _LINUX_I2C_ITK_H_
#define _LINUX_I2C_ITK_H_

/**
 * struct itk_platform_data - platform-specific ITK data
 * @init_irq:          initialize interrupt
 * @get_irq_level:     obtain current interrupt level
 */
struct itk_platform_data {
       int (*init_irq)(void);
       int (*get_irq_level)(void);
	 void (*touch_on)(int flag);
       int tp_max_width;
       int tp_max_height;
       int lcd_max_width;
       int lcd_max_height;
       unsigned swap_xy:1;
       unsigned xpol:1;
       unsigned ypol:1;
};

#endif /* !_LINUX_I2C_ITK_H_*/
