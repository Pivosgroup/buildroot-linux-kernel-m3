/*
 * Common power driver for Amlogic Devices with one or two external
 * power supplies (AC/USB) connected to main and backup batteries,
 * and optional builtin charger.
 *
 * Copyright 2010 Shoufu zhao<shoufu.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SMBA10XX_BATTERY_H__
#define __SMBA10XX_BATTERY_H__

#define	SMBA10XX_POWER_MANAGER_NAME 		"smba10xx_battery"
#define	SMBA10XX_I2C_NAME 					"bat_i2c"
#define	SMBA10XX_I2C_ADDR 					(0x0B)

struct device;

struct smba10xx_battery_power_pdata {
	int (*is_ac_online)(void);
	int (*get_charge_status)(void);
	void (*set_charge)(int flags);
	void (*set_bat_off)(void);

	unsigned int battery_poll_timer_interval;		//default 12000 ms
};

#endif /* __SMBA10XX_BATTERY_H__ */
