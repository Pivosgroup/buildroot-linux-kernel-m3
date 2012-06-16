/*
 * Common power driver for Amlogic Devices with one or two external
 * power supplies (AC/USB) connected to main and backup batteries,
 * and optional builtin charger.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BQ27X00_BATTERY_H__
#define __BQ27X00_BATTERY_H__

#define AML_POWER_CHARGE_AC  (1 << 0)
#define AML_POWER_CHARGE_USB (1 << 1)

struct bq27x00_battery_pdata {
	int (*is_ac_online)(void);
	int (*is_usb_online)(void);
	void (*set_charge)(int flags);
	void (*set_bat_off)(void);
	int (*get_charge_status)(void);
    void (*set_lcd_on)(void);
    void (*set_lcd_off)(void);
	unsigned int wait_for_status; /* msecs, default is 500 */
	unsigned int polling_interval; /* msecs, default is 12000 */
    unsigned int chip;
};

#endif /* __BQ27X00_BATTERY_H__ */
