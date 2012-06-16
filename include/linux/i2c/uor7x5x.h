/**
 * linux/include/i2c/uor7x5x.h
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

#ifndef _LINUX_UOR7X5X_ITK_H_
#define _LINUX_UOR7X5X_ITK_H_

/**
 * struct itk_platform_data - platform-specific UOR7X5X data
 * @init_irq:          initialize interrupt
 * @get_irq_level:     obtain current interrupt level
 */
struct uor7x5x_platform_data {
       int (*init_irq)(void);
       int (*get_irq_level)(void);
       int abs_xmin;
       int abs_xmax;
       int abs_ymin;
       int abs_ymax;
       int (*convert)(int x, int y);
};

#endif /* !_LINUX_UOR7X5X_ITK_H_*/
