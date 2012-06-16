/*
 * Common power driver for Amlogic Devices with one or two external
 * power supplies (AC/USB) connected to main and backup batteries,
 * and optional builtin charger.
 *
 * Copyright  2010 Shoufu zhao <shoufu.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/smba10xx_battery.h>


#include "bat_i2c.h"
#include "bat_sbi.h"

#define SMBA10XX_BATTERY_DEBUG_LOG		0

#if SMBA10XX_BATTERY_DEBUG_LOG == 1
	#define logd(x...)  	printk(x)
#else
	#define logd(x...)		NULL
#endif


#define CHARGE_DISABLE_TEMPERATURE		60
#define CHARGE_ENABLE_TEMPERATURE		40

static int ac_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val);

static int battery_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val);

static struct timer_list battery_poll_timer;

static struct work_struct battery_work;

static struct device *dev;
static struct smba10xx_battery_power_pdata *pdata;

struct smart_battery_dev {
	int 	ac_online;
	int		chen;
	int 	battery_alarm;
	int 	status;
	int 	present;
	int 	temperature;
	int 	rsoc;
	int 	voltage_now;
	int 	voltage_avg;
	int 	current_now;
	int 	current_avg;
	int 	time_to_empty_now;
	int 	time_to_empty_avg;
	int 	time_to_full_avg;
	int 	chemistry;
	int 	health;
}bat_info;



static const enum power_supply_property ac_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const enum power_supply_property battery_power_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
//	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
//	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,	// RSOC
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG, 
	/* No POWER_SUPPLY_PROP_TIME_TO_FULL_NOW, */
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static char *ac_supply_list[] = {
	"battery",
};

static struct power_supply all_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = battery_power_properties,
		.num_properties = ARRAY_SIZE(battery_power_properties),
		.get_property = battery_power_get_property,
	},

	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = ac_supply_list,
		.num_supplicants = ARRAY_SIZE(ac_supply_list),
		.properties = ac_power_properties,
		.num_properties = ARRAY_SIZE(ac_power_properties),
		.get_property = ac_power_get_property,
	}
};

static int ac_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int retval = 0;

	logd("ac_power_get_property\r\n");

	if (psp == POWER_SUPPLY_PROP_ONLINE) {
		val->intval = bat_info.ac_online;
	} else {
		retval = -EINVAL;
	}
	return retval;
}

static int battery_power_get_property(struct power_supply *psy, 
		enum power_supply_property psp, union power_supply_propval *val)
{
	int retval = 0;

	logd("battery_power_get_property\r\n");

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			logd ("POWER_SUPPLY_PROP_STATUS\r\n");
			val->intval = bat_info.status;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			logd ("POWER_SUPPLY_PROP_PRESENT\r\n");
			val->intval = bat_info.present;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			logd ("POWER_SUPPLY_PROP_TECHNOLOGY\r\n");
			val->intval = bat_info.chemistry;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			logd ("POWER_SUPPLY_PROP_VOLTAGE_NOW\r\n");
			val->intval = bat_info.voltage_now;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_AVG:
			logd ("POWER_SUPPLY_PROP_VOLTAGE_AVG");
			val->intval = bat_info.voltage_avg;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			logd ("POWER_SUPPLY_PROP_CURRENT_NOW\r\n");
			val->intval = bat_info.current_now;
			break;
		case POWER_SUPPLY_PROP_CURRENT_AVG:
			logd ("POWER_SUPPLY_PROP_CURENT_AVG\r\n");
			val->intval = bat_info.current_avg;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			logd ("POWER_SUPPLY_PROP_TEMP\r\n");
			val->intval = bat_info.temperature;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			logd ("POWER_SUPPLY_PROP_CAPACITY\r\n");
			val->intval = bat_info.rsoc;
			break;
		case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
			logd ("POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW\r\n");
			val->intval = bat_info.time_to_empty_now;
			break;
		case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
			logd ("POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG\r\n");
			val->intval = bat_info.time_to_empty_avg;
			break;
		case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
			logd ("POWER_SUPPLY_PROP_TIME_TO_FULL_AVG\r\n");
			val->intval = bat_info.time_to_full_avg;
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			logd ("POWER_SUPPLY_PROP_HEALTH \r\n");
			val->intval = bat_info.health;
			break;
		default:
			retval = -EINVAL;
	}
	return retval;
}

int charge_enable(bool enable)
{
	if (bat_info.chen != enable) {
		bat_info.chen = enable;
		//////NvOdmGpioSetState(bat_info.gpio_handle, bat_info.chen_pin_handle, enable);
	}

	return 0;
}

/*
 * return 0 to indicate changed.
 */
int update_ac_state(void) 
{
	/*TODO: how to detect ac attach? */
	int acin = 0;

	acin = pdata->is_ac_online();
	acin = !acin;
	if (acin != bat_info.ac_online) {
		bat_info.ac_online = acin;
		return 0;
	}
	return -1;
}

/*
 * return 0 to indicate changed.
 */
int update_battery_state(void)
{
	short current_now, current_avg;
	unsigned short status, rsoc, temperature, serial_number, health;
	unsigned short voltage_now, voltage_avg;
	unsigned short int time_to_empty_now, time_to_empty_avg, time_to_full_avg;

	int check_count = 6;

	//as the i2c will failed to read battery's registers by using smbus, we read more times to ensure battery is pluged in.
	while(check_count > 0)
	{
		if(		!BatteryStatus(&status)
			&&	!RelativeStateOfCharge(&rsoc)
			&&	!Temperature(&temperature)
			&&	!Voltage(&voltage_now)
	//		&&	!AverageVoltage(&voltage_avg)
			&&	!Current(&current_now)
			&&	!AverageCurrent(&current_avg)
			&&	!RunTimeToEmpty(&time_to_empty_now)
			&&	!AverageTimeToEmpty(&time_to_empty_avg)
			&&	!AverageTimeToEmpty(&time_to_full_avg))
	//		&& 	!StateOfHealth(&health)  )
		{
			break;
		}

		check_count--;
	}

	if(check_count <= 0)
	{
		bat_info.present = 0;
		bat_info.status = POWER_SUPPLY_STATUS_UNKNOWN;
		logd("Present=%d\r\n", bat_info.present);
		return 0;
	}

	bat_info.battery_alarm = status & SBI_BS_ALL_ALARM;
	
	if (bat_info.battery_alarm) {
	
		logd("BatteryStatus=%d\r\n", status);

		if (status & SBI_BS_OVER_CHARGED_ALARM) {
			logd("OVER_CHARGED_ALARM\r\n");
		}
		if (status & SBI_BS_OVER_TEMP_ALARM) {
			logd("OVER_TEMP_ALARM\r\n");
		}
		if (status & SBI_BS_TERMINATE_CHARGE_ALARM) {
			logd("TERMINATE_CHARGE_ALARM\r\n");
		}
		if (status & SBI_BS_TERMINATE_DISCHARGE_ALARM) {
			logd("TERMIANTE_DISCHARGE_ALARM\r\n");
		} 
		if (status & SBI_BS_REMAINING_CAPACITY_ALARM) {
			logd("REMAINING_CAPACITY_ALARM\r\n");
		} 
		if (status & SBI_BS_REMAINING_TIME_ALARM) {
			logd("REMAINING_TIME_ALARM\r\n");
		}
	}

	if (status & SBI_BS_FULLY_CHARGED) {
		bat_info.status = POWER_SUPPLY_STATUS_FULL;
		logd("Status=%s", "FULL");
	} else {
		if (!bat_info.ac_online) {
			bat_info.status = POWER_SUPPLY_STATUS_DISCHARGING;
			logd("Status=%s", "DISCHARGING");
		} else {
			bat_info.status = (status & SBI_BS_DISCHARGING) ? POWER_SUPPLY_STATUS_DISCHARGING : POWER_SUPPLY_STATUS_CHARGING;
			logd("Status=%s", ((status & SBI_BS_DISCHARGING) ? "DISCHARGING" : "CHARGING"));
		}
	}

	bat_info.present = 1;

	bat_info.temperature = temperature / 10;		// 0.1K to 1K
	bat_info.rsoc = rsoc;
	bat_info.voltage_now = voltage_now * 1000;	// mV to uV
//	bat_info.voltage_avg = voltage_avg;
	bat_info.current_now = current_now * 1000;	// mA to uA
	bat_info.current_avg = current_avg * 1000;	// mA to uA
	bat_info.time_to_empty_now = time_to_empty_now * 60;	// min to sec
	bat_info.time_to_empty_avg = time_to_empty_avg * 60;	// min to sec
	bat_info.time_to_full_avg = time_to_full_avg * 60;	// min to sec
	bat_info.chemistry = POWER_SUPPLY_TECHNOLOGY_LION;
//	bat_info.health = health;
//	bat_info.ac_online = (bat_info.status != POWER_SUPPLY_STATUS_DISCHARGING);

	logd("Present=%d\r\n", bat_info.present);
	logd("Temperature=%d\r\n", bat_info.temperature);
	logd("RelativeStateOfChage=%d\r\n", bat_info.rsoc);
	logd("Voltage=%d\r\n", bat_info.voltage_now);
//	logd("AverageVoltage=%d\r\n", bat_info.voltage_avg);
	logd("Current=%d\r\n", bat_info.current_now);
	logd("AverageCurrent=%d\r\n", bat_info.current_avg);
	logd("RunTimeToEmpty=%d\r\n", bat_info.time_to_empty_now);
	logd("AvgTimeToEmpty=%d\r\n", bat_info.time_to_empty_avg);
//	logd("Health=%d\r\n", bat_info.health);

	return 0;
}

static void battery_work_func(struct work_struct *work)
{
	bool ac_changed, battery_changed;
	logd("battery_work working...\r\n");

	ac_changed = !update_ac_state();
	battery_changed = !update_battery_state();

	/* Safe battery temprature check */
	if (bat_info.chen) {
		if (bat_info.temperature > CHARGE_DISABLE_TEMPERATURE) {
			charge_enable(false);
		}
	} else {
		if (bat_info.temperature < CHARGE_ENABLE_TEMPERATURE) {
			charge_enable(true);
		}
	}

	if (ac_changed) {
		logd("using ac\n");
		power_supply_changed(&all_supplies[1]);
	}
	if (battery_changed) {
		logd("using battery\n");
		power_supply_changed(&all_supplies[0]);
	}
}

static void battery_poll_timer_func(unsigned long unused)
{
	schedule_work(&battery_work);
	mod_timer(&battery_poll_timer, jiffies + msecs_to_jiffies(pdata->battery_poll_timer_interval));
}

void acin_isr(void *arg)
{
	logd("acin_isr...\r\n");
	schedule_work(&battery_work);
}


static void smart_battery_init_values(void)
{
	bat_info.ac_online = 0;
	bat_info.chen = 0;
	bat_info.battery_alarm = 0;
	bat_info.status = 0;
	bat_info.present = 0;
	bat_info.temperature = 0;
	bat_info.rsoc = 0;
	bat_info.voltage_now = 0;
	bat_info.voltage_avg = 0;
	bat_info.current_now = 0;
	bat_info.current_avg = 0;
	bat_info.time_to_empty_now = 0;
	bat_info.time_to_empty_avg = 0;
	bat_info.time_to_full_avg = 0;
	bat_info.chemistry = 0;
	bat_info.health = 0;
}


static ssize_t store_powerhold(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{
	if(buf[0] == 'y'){
		logd("system off\n");
        pdata->set_bat_off();
    }

	return 0;
}

static struct class_attribute powerhold_class_attrs[] = {
    __ATTR(bat-off,  S_IRUGO | S_IWUSR, NULL,    store_powerhold),
    __ATTR_NULL
};

static struct class powerhold_class = {
    .name = "powerhold",
    .class_attrs = powerhold_class_attrs,
};

static int smart_battery_probe(struct platform_device *pdev)
{
	int ret = 0, i;

	logd("battery: probe() IN\r\n");

	dev = &pdev->dev;

	if (pdev->id != -1) {
		dev_err(dev, "it's meaningless to register several "
			"pda_powers; use id = -1\n");
		ret = -EINVAL;
		goto wrongid;
	}

	pdata = pdev->dev.platform_data;

	smart_battery_init_values();

	update_ac_state();
	update_battery_state();

	INIT_WORK(&battery_work, battery_work_func);

	for (i = 0; i < ARRAY_SIZE(all_supplies); i++) {
		power_supply_register(&pdev->dev, &all_supplies[i]);
	}

	if (!pdata->battery_poll_timer_interval)
		pdata->battery_poll_timer_interval = 12 * 1000;

	setup_timer(&battery_poll_timer, battery_poll_timer_func, 0);
	mod_timer(&battery_poll_timer, jiffies + msecs_to_jiffies(pdata->battery_poll_timer_interval));

	ret = class_register(&powerhold_class);
	if(ret){
		logd(" class register powerhold_class fail!\n");
	}
	
	logd("battery: probe() OUT");

wrongid:
	logd("battery_probe failed... ");
	return -1;
}

static int smart_battery_remove (struct platform_device *pdev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(all_supplies); i++) {
		power_supply_unregister(&all_supplies[i]);
	}
	return 0;
}

static int smart_battery_suspend (struct platform_device *pdev, 
	pm_message_t state) 
{
	logd("battery: suspend\r\n");
	
	/* Kill the battery timer sync */
	del_timer_sync(&battery_poll_timer);
	flush_scheduled_work();
	return 0;
}

static int smart_battery_resume (struct platform_device *pdev)
{
	logd("battery: resume\r\n");
	setup_timer(&battery_poll_timer, battery_poll_timer_func, 0);
	mod_timer(&battery_poll_timer, jiffies + msecs_to_jiffies(pdata->battery_poll_timer_interval));	
	return 0;
}

static struct platform_driver smart_battery_driver = {
	.probe = smart_battery_probe,
	.remove = smart_battery_remove,
	.suspend = smart_battery_suspend,
	.resume = smart_battery_resume,
	.driver.name = SMBA10XX_POWER_MANAGER_NAME,
	.driver.owner = THIS_MODULE,
};

static int smart_battery_init(void)
{
	logd("battery: init() IN ");
	platform_driver_register(&smart_battery_driver);
	logd("battery: init() OUT ");
	return 0;
}

static void smart_battery_exit(void)
{
	platform_driver_unregister(&smart_battery_driver);
}

module_init(smart_battery_init);
module_exit(smart_battery_exit);

MODULE_AUTHOR("");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for Smba10xx smart battery");
