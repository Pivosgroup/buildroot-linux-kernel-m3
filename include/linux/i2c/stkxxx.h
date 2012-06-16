/**
 * linux/include/i2c/stkxxx.h
 *
 * Copyright (C) 2007-2008 Avionic Design Development GmbH
 * Copyright (C) 2008-2009 Avionic Design GmbH
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Written by Thierry Reding <thierry.reding@xxxxxxxxxxxxxxxxx>
 */

#ifndef LINUX_I2C_STKXXX_H
#define LINUX_I2C_TSTKXXX_H

/**
 * struct stkxxx_platform_data - platform-specific STKXXX data
 * @init_irq:          initialize interrupt
 * @get_irq_level:     obtain current interrupt level
 */
struct stkxxx_platform_data {
       int (*init_irq)(void);
       int (*get_irq_level)(void);
};

#endif /* !LINUX_I2C_TSC2003_H */
