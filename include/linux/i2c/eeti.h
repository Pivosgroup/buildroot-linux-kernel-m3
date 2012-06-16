/**
 * linux/include/i2c/eeti.h
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

#ifndef _LINUX_I2C_EETI_H_
#define _LINUX_I2C_EETI_H_

/**
 * struct eeti_platform_data - platform-specific EETI data
 * @init_irq:          initialize interrupt
 * @get_irq_level:     obtain current interrupt level
 */
struct eeti_platform_data {
       int (*init_irq)(void);
       int (*get_irq_level)(void);
	 void (*touch_on)(int flag);
       int tp_max_width;
       int tp_max_height;
       int lcd_max_width;
       int lcd_max_height;
};

#endif /* !_LINUX_I2C_EETI_H_*/
