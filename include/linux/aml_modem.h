/*
 * Common power driver for Amlogic Devices with one or two external
 * power supplies (AC/USB) connected to main and backup batteries,
 * and optional builtin charger.
 *
 * Copyright © 2011-4-27 alex.deng <alex.deng@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __AML_modem_H__
#define __AML_modem_H__

struct device;

struct aml_modem_pdata {
	int (*init)(struct device *dev);
	int (*power_on)(void);
	int (*power_off)(void);
	void (*reset)(void);
	void (*enable)(void);
	void (*disable)(void);
	void (*exit)(struct device *dev);
	
};

#endif /* __AML_modem_H__ */
