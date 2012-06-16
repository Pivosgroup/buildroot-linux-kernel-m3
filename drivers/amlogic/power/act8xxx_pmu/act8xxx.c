/*
 * PMU driver for ACT8xxx
 *
 * Copyright (c) 2010-2011 Amlogic Ltd.
 *	Elvis Yu <elvis.yu@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <mach/am_regs.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>
#include <linux/saradc.h>
#include <linux/act8xxx.h>

#define DRIVER_VERSION			"0.1.0"

static int dbg_enable = 0;
#define act8xxx_dbg(level, fmt, args...)  { if(level) \
					printk("[goodix]: " fmt, ## args); }
static int usb_status = 0;
static int new_usb_status = 0;

/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(pmu_id);
static DEFINE_MUTEX(pmu_mutex);

struct act8xxx_device_info *act8xxx_dev;	//Elvis Fool

#ifdef CONFIG_PMU_ACT8942
struct act8942_operations* act8942_opts = NULL;
#endif

static dev_t act8xxx_devno;

typedef struct pmu_dev_s {
    /* ... */
    struct cdev	cdev;             /* The cdev structure */
} pmu_dev_t;

static pmu_dev_t *act8xxx_pmu_dev = NULL;

struct act8xxx_device_info {
	struct device 		*dev;
	int			id;
	struct i2c_client	*client;

#ifdef CONFIG_PMU_ACT8942
	struct power_supply	bat;
	struct power_supply	ac;	
	struct power_supply	usb;
	struct timer_list polling_timer;
	struct work_struct work_update;
	int ac_status;
	int capacity;
	int bat_status;	
#endif
};

static struct i2c_client *this_client = NULL;

static int act8xxx_read_i2c(struct i2c_client *client, u8 reg, u8 *val);

#ifdef CONFIG_USB_ANDROID
#ifdef CONFIG_PMU_ACT8942
int pc_connect(int status)
{
	new_usb_status = status;
	if(new_usb_status == usb_status)
		return 1;
	usb_status = new_usb_status;
	power_supply_changed(&act8xxx_dev->usb);
	return 0;
}
EXPORT_SYMBOL(pc_connect);
#endif
#endif

/*
 *	ACINSTAT
 *	ACIN Status. Indicates the state of the ACIN input, typically
 *	in order to identify the type of input supply connected. Value
 *	is 1 when ACIN is above the 1.2V precision threshold, value
 *	is 0 when ACIN is below this threshold.
 */
static inline int is_ac_online(void)
{
	u8 val;	
	act8xxx_read_i2c(this_client, (ACT8942_APCH_ADDR+0xa), &val);
	
	logd("%s: get from pmu is %d.\n", __FUNCTION__, val);	
	return	(val & 0x2) ? 1 : 0;
}

static inline int is_usb_online(void)
{
	return usb_status;
}

/*
 *	Charging Status Indication
 *
 *	CSTATE[1]	CSTATE[0]	STATE MACHINE STATUS
 *
 *		1			1		PRECONDITION State
 *		1			0		FAST-CHARGE / TOP-OFF State
 *		0			1		END-OF-CHARGE State
 *		0			0		SUSPEND/DISABLED / FAULT State
 *
 */
static inline int get_charge_status(void)
{
	u8 val;
	
	act8xxx_read_i2c(this_client, (ACT8942_APCH_ADDR+0xa), &val);

	logd("%s: get from pmu is %d.\n", __FUNCTION__, val);
	
	return ((val>>4) & 0x3);
}

//export this interface for other driver
int act8xxx_is_ac_online(void)
{	
	if(this_client == NULL)
	{
		pr_err("act8xxx: have not init now, wait.... \n");
		return 1;
	}
	return is_ac_online();
}
EXPORT_SYMBOL(act8xxx_is_ac_online);

#ifdef CONFIG_PMU_ACT8942
static inline int get_bat_status(void)
{
    int ret,status;
    
    if(act8942_opts->is_ac_online()){
        status = act8942_opts->get_charge_status();
        if(act8942_opts->get_charge_status == get_charge_status){
            if(status == 0x1){
                ret = POWER_SUPPLY_STATUS_FULL;
            }
            else if(status == 0x0){
                ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
            }
            else{
                ret = POWER_SUPPLY_STATUS_CHARGING;
            }
        }
        else{//if(act8942_opts->get_charge_status == get_charge_status)
            if(status == 0x1){
                ret = POWER_SUPPLY_STATUS_FULL;
            }
            else if(status == 0x0){
                ret = POWER_SUPPLY_STATUS_CHARGING;
            }
        }
    }//if(act8942_opts->is_ac_online())
    else{
        ret = POWER_SUPPLY_STATUS_DISCHARGING;
    }
    
    return ret;
}

static enum power_supply_property bat_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
//	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
//	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
//	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

static enum power_supply_property ac_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property usb_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

inline static int measure_capacity_advanced(void);

#define to_act8942_device_info(x) container_of((x), \
				struct act8xxx_device_info, bat);

static int bat_power_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	u8 status;
	//struct act8xxx_device_info *act8942_dev = to_act8942_device_info(psy);

	switch (psp)
	{
		case POWER_SUPPLY_PROP_STATUS:
            val->intval = act8xxx_dev->bat_status;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = act8942_opts->measure_voltage();
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = val->intval <= 0 ? 0 : 1;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			val->intval = act8942_opts->measure_current();
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
		    if((act8xxx_dev->bat_status == POWER_SUPPLY_STATUS_CHARGING)
		        &&(act8xxx_dev->capacity == 0)){
                val->intval = 1;
		    }
		    else{
			    val->intval = act8xxx_dev->capacity;
			}
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = 0;		//temporary
			break;
//	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
//		ret = bq27x00_battery_time(di, BQ27x00_REG_TTE, val);
//		break;
//	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
//		ret = bq27x00_battery_time(di, BQ27x00_REG_TTECP, val);
//		break;
//	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
//		ret = bq27x00_battery_time(di, BQ27x00_REG_TTF, val);
//		break;
	    case POWER_SUPPLY_PROP_TECHNOLOGY:
	        val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
	        break;
		case POWER_SUPPLY_PROP_HEALTH:	
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		default:
			return -EINVAL;
	}

	return ret;
}

static int ac_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int retval = 0;

	switch (psp)
	{
		case POWER_SUPPLY_PROP_ONLINE:
		val->intval = act8942_opts->is_ac_online();
		break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:	
		val->intval = act8942_opts->measure_voltage();
		break;
		default:
		return -EINVAL;
	}

	return retval;
}

static int usb_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int retval = 0;

	switch (psp)
	{
		case POWER_SUPPLY_PROP_ONLINE:
		val->intval = act8942_opts->is_usb_online();	//temporary
		break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:	
		val->intval = act8942_opts->measure_voltage();
		break;
		default:
		return -EINVAL;
	}

	return retval;
}

static char *power_supply_list[] = {
	"Battery",
};

static void act8942_powersupply_init(struct act8xxx_device_info *act8942_dev)
{
	act8942_dev->bat.name = "bat";
	act8942_dev->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	act8942_dev->bat.properties = bat_power_props;
	act8942_dev->bat.num_properties = ARRAY_SIZE(bat_power_props);
	act8942_dev->bat.get_property = bat_power_get_property;
	act8942_dev->bat.external_power_changed = NULL;

    act8942_dev->ac.name = "ac";
	act8942_dev->ac.type = POWER_SUPPLY_TYPE_MAINS;
	act8942_dev->ac.supplied_to = power_supply_list,
	act8942_dev->ac.num_supplicants = ARRAY_SIZE(power_supply_list),
	act8942_dev->ac.properties = ac_power_props;
	act8942_dev->ac.num_properties = ARRAY_SIZE(ac_power_props);
	act8942_dev->ac.get_property = ac_power_get_property;

    act8942_dev->usb.name = "usb";
	act8942_dev->usb.type = POWER_SUPPLY_TYPE_USB;
	act8942_dev->usb.supplied_to = power_supply_list,
	act8942_dev->usb.num_supplicants = ARRAY_SIZE(power_supply_list),
	act8942_dev->usb.properties = usb_power_props;
	act8942_dev->usb.num_properties = ARRAY_SIZE(usb_power_props);
	act8942_dev->usb.get_property = usb_power_get_property;	
}

static void update_work_func(struct work_struct *work)
{

	int capacity,bat_status;
	
	capacity = measure_capacity_advanced();
	bat_status = get_bat_status();
	
	if(act8xxx_dev->ac_status != is_ac_online()){
		power_supply_changed(&act8xxx_dev->ac);		    
	}
	
	if(act8xxx_dev->capacity != capacity){
	    act8xxx_dev->capacity = capacity;
	    power_supply_changed(&act8xxx_dev->bat);
	}

	if(act8xxx_dev->bat_status != bat_status){
	    act8xxx_dev->bat_status = bat_status;
	    power_supply_changed(&act8xxx_dev->bat);
	}
		
}

static void polling_func(unsigned long arg)
{
	struct act8xxx_device_info *act8942_dev = (struct act8xxx_device_info *)arg;
	schedule_work(&(act8942_dev->work_update));
	mod_timer(&(act8942_dev->polling_timer), jiffies + msecs_to_jiffies(act8942_opts->update_period));  
}
#endif

/*
 * i2c specific code
 */

static int act8xxx_read_i2c(struct i2c_client *client, u8 reg, u8 *val)
{
	if (!client->adapter)
		return -ENODEV;

	struct i2c_msg msgs[] = {
        {
            .addr = client->addr,
            .flags = 0,
            .len = 1,
            .buf = &reg,
        },
        {
            .addr = client->addr,
            .flags = I2C_M_RD,
            .len = 1,
            .buf = val,
        }
    };

	if(i2c_transfer(client->adapter, msgs, 2) == 2)
	{
		return 0;
	}

	return -EBUSY;
}

static int act8xxx_write_i2c(struct i2c_client *client, u8 reg, u8 *val)
{
	unsigned char buff[2];
    buff[0] = reg;
    buff[1] = *val;
    struct i2c_msg msgs[] = {
        {
        .addr = client->addr,
        .flags = 0,
        .len = 2,
        .buf = buff,
        }
    };

	if(i2c_transfer(client->adapter, msgs, 1) == 1)
	{
		return 0;
	}

	return -EBUSY;
}

int get_vsel(void)	//Elvis Fool
{
	return 0;
}
void set_vsel(int level)	//Elvis Fool
{
	return;
}

/*
 *	REGx/VSET[ ] Output Voltage Setting
 *
 *								REGx/VSET[5:3]
 *	REGx/VSET[2:0]	000 	001 	010 	011 	100 	101 	110 	111
 *			000 	0.600	0.800 	1.000 	1.200 	1.600 	2.000 	2.400 	3.200
 *			001 	0.625	0.825 	1.025 	1.250 	1.650 	2.050 	2.500 	3.300
 *			010 	0.650	0.850	1.050	1.300	1.700	2.100	2.600	3.400
 *			011		0.675	0.875	1.075	1.350	1.750	2.150	2.700	3.500
 *			100		0.700	0.900	1.100	1.400	1.800	2.200	2.800	3.600
 *			101		0.725	0.925	1.125	1.450	1.850	2.250	2.900	3.700
 *			110		0.750	0.950	1.150	1.500	1.900	2.300	3.000	3.800
 *			111		0.775	0.975	1.175	1.550	1.950	2.350	3.100	3.900
 *
 */
static const unsigned long vset_table[] = {
	600,	625,	650,	675,	700,	725,	750,	775,
	800,	825,	850,	875,	900,	925,	950,	975,
	1000,	1025,	1050,	1075,	1100,	1125,	1150,	1175,
	1200,	1250,	1300,	1350,	1400,	1450,	1500,	1550,
	1600,	1650,	1700,	1750,	1800,	1850,	1900,	1950,
	2000,	2050,	2100,	2150,	2200,	2250,	2300,	2350,
	2400,	2500,	2600,	2700,	2800,	2900,	3000,	3100,
	3200,	3300,	3400,	3500,	3600,	3700,	3800,	3900,
};	//unit is mV

static inline int get_vset_from_table(unsigned long voltage, uint8_t *val)
{
	uint8_t i;
	*val = 0;
	if((voltage<600) || (voltage > 3900))
	{
		pr_err("Wrong VSET range! VSET range in [600:3900]mV\n");
		return -EINVAL;
	}
	
	for(i=0; i<ARRAY_SIZE(vset_table); i++)
	{
		if(voltage == vset_table[i])
		{
			*val = i;
			return 0;
		}
	}
	pr_err("voltage invalid! Please notice vset_table!\n");
	return -EINVAL;
}

static int set_reg_voltage(act8xxx_regx regx, unsigned long *voltage)
{
	int ret = 0;
	static u32 reg_addr[] = {ACT8xxx_REG1_ADDR, ACT8xxx_REG2_ADDR, ACT8xxx_REG3_ADDR,
		ACT8xxx_REG4_ADDR, ACT8xxx_REG5_ADDR, ACT8xxx_REG6_ADDR, ACT8xxx_REG7_ADDR};
	act8xxx_register_data_t register_data = { 0 };
	if((regx<1) || (regx>7))
	{
		pr_err("Wrong REG number! REG number in [1:7]\n");
		return -EINVAL;
	}
	get_vset_from_table(*voltage, &register_data.d8);
	//pr_err("set_reg_voltage:	reg_addr = 0x%02x,	data = 0x%x\n", reg_addr[regx-1], register_data.d8);
	ret = act8xxx_write_i2c(this_client, reg_addr[regx-1], &register_data.d8);	//regx-1 for compatible act8xxx_regx
	return ret;
}

static int get_reg_voltage(act8xxx_regx regx, unsigned long *voltage)
{
	int ret = 0;
	static u32 reg_addr[] = {ACT8xxx_REG1_ADDR, ACT8xxx_REG2_ADDR, ACT8xxx_REG3_ADDR,
		ACT8xxx_REG4_ADDR, ACT8xxx_REG5_ADDR, ACT8xxx_REG6_ADDR, ACT8xxx_REG7_ADDR};
	act8xxx_register_data_t register_data = { 0 };
	if((regx<1) || (regx>7))
	{
		pr_err("Wrong REG number! REG number in [1:7]\n");
		return -EINVAL;
	}
	ret = act8xxx_read_i2c(this_client, reg_addr[regx-1], &register_data.d8); //regx-1 for compatible act8xxx_regx
	*voltage = vset_table[register_data.REGx_VSET];
	return ret;
}

static inline void	act8xxx_dump(struct i2c_client *client)
{
	u8 val = 0;
	int ret = 0;
	
	ret = act8xxx_read_i2c(client, ACT8xxx_SYS_ADDR, &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_SYS_ADDR, val);
	
	ret = act8xxx_read_i2c(client, (ACT8xxx_SYS_ADDR+1), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_SYS_ADDR+1, val);
	
	ret = act8xxx_read_i2c(client, ACT8xxx_REG1_ADDR, &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG1_ADDR, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG1_ADDR+1), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG1_ADDR+1, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG1_ADDR+2), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG1_ADDR+2, val);

	ret = act8xxx_read_i2c(client, ACT8xxx_REG2_ADDR, &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG2_ADDR, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG2_ADDR+1), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG2_ADDR+1, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG2_ADDR+2), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG2_ADDR+2, val);

	ret = act8xxx_read_i2c(client, ACT8xxx_REG3_ADDR, &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG3_ADDR, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG3_ADDR+1), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG3_ADDR+1, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG3_ADDR+2), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG3_ADDR+2, val);

	ret = act8xxx_read_i2c(client, ACT8xxx_REG4_ADDR, &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG4_ADDR, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG4_ADDR+1), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG4_ADDR+1, val);

	ret = act8xxx_read_i2c(client, ACT8xxx_REG5_ADDR, &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG5_ADDR, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG5_ADDR+1), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG5_ADDR+1, val);

	ret = act8xxx_read_i2c(client, ACT8xxx_REG6_ADDR, &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG6_ADDR, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG6_ADDR+1), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG6_ADDR+1, val);

	ret = act8xxx_read_i2c(client, ACT8xxx_REG7_ADDR, &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG7_ADDR, val);

	ret = act8xxx_read_i2c(client, (ACT8xxx_REG7_ADDR+1), &val);
	pr_info("act8xxx: [0x%x] : 0x%x\n", ACT8xxx_REG7_ADDR+1, val);

#ifdef CONFIG_PMU_ACT8942
	ret = act8xxx_read_i2c(client, ACT8942_APCH_ADDR, &val);
	pr_info("act8942: [0x%x] : 0x%x\n", ACT8942_APCH_ADDR, val);

	ret = act8xxx_read_i2c(client, (ACT8942_APCH_ADDR+1), &val);
	pr_info("act8942: [0x%x] : 0x%x\n", ACT8942_APCH_ADDR+1, val);

	ret = act8xxx_read_i2c(client, (ACT8942_APCH_ADDR+8), &val);
	pr_info("act8942: [0x%x] : 0x%x\n", ACT8942_APCH_ADDR+8, val);

	ret = act8xxx_read_i2c(client, (ACT8942_APCH_ADDR+9), &val);
	pr_info("act8942: [0x%x] : 0x%x\n", ACT8942_APCH_ADDR+9, val);

	ret = act8xxx_read_i2c(client, (ACT8942_APCH_ADDR+0xa), &val);
	pr_info("act8942: [0x%x] : 0x%x\n", ACT8942_APCH_ADDR+0xa, val);
#endif
}

/****************************************************************************/
/* max args accepted for monitor commands */
#define CONFIG_SYS_MAXARGS		16
//#define DEBUG_PARSER

static int parse_line (char *line, char *argv[])
{
	int nargs = 0;

#ifdef DEBUG_PARSER
	pr_info ("parse_line: \"%s\"\n", line);
#endif
	while (nargs < CONFIG_SYS_MAXARGS) {

		/* skip any white space */
		while ((*line == ' ') || (*line == '\t')) {
			++line;
		}

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
#ifdef DEBUG_PARSER
		pr_info ("parse_line: nargs=%d\n", nargs);
#endif
			return (nargs);
		}

		argv[nargs++] = line;	/* begin of argument string	*/

		/* find end of string */
		while (*line && (*line != ' ') && (*line != '\t')) {
			++line;
		}

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
#ifdef DEBUG_PARSER
		pr_info ("parse_line: nargs=%d\n", nargs);
#endif
			return (nargs);
		}

		*line++ = '\0';		/* terminate current arg	 */
	}

	pr_info ("** Too many args (max. %d) **\n", CONFIG_SYS_MAXARGS);

#ifdef DEBUG_PARSER
	pr_info ("parse_line: nargs=%d\n", nargs);
#endif
	return (nargs);
}

/****************************************************************************/


ssize_t act8xxx_register_dump(struct class *class, struct class_attribute *attr, char *buf)
{
	act8xxx_dump(this_client);
	return 0;
}

ssize_t act8xxx_test_show(struct class *class, struct class_attribute *attr, char *buf)
{
	unsigned long voltage;
	if(get_reg_voltage(ACT8xxx_REG3, &voltage))
	{
		return -1;
	}
	pr_info("test:get %ldmV\n", voltage);
	return 0;
}

ssize_t act8xxx_test_store(struct class *class, struct class_attribute *attr, char *buf)
{
	unsigned long voltage = 3100;
	if(set_reg_voltage(ACT8xxx_REG3, &voltage))
	{
		return -1;
	}
	pr_info("test:set %ldmV\n", voltage);
	return 0;
}

ssize_t act8xxx_debug_store(struct class *class, struct class_attribute *attr, char *buf)
{
    dbg_enable = !dbg_enable;
    if(dbg_enable)
	    pr_info("enable act8xxx log \n");
	else
	    pr_info("disable act8xxx log \n");	    
	return 0;
}

ssize_t act8xxx_voltage_handle(struct class *class, struct class_attribute *attr, char *buf)
{
	char *argv[CONFIG_SYS_MAXARGS + 1];	/* NULL terminated	*/
	int argc, ret, regx;
	unsigned long voltage;
	argc = parse_line(buf, argv);
	if(argc < 2)
	{
		return -EINVAL;
		//ToDo help 
	}

	if(!strcmp(argv[0], "get"))
	{
		ret = sscanf(argv[1], "reg%d", &regx);
		if(get_reg_voltage(regx, &voltage))
		{
			return -1;
		}
		pr_info("act8xxx_voltage_handle: Get reg%d voltage is %lumV\n", regx,voltage);
	}
	else if(!strcmp(argv[0], "set"))
	{
		if(argc < 3)
		{
			return -EINVAL;
			//ToDo help 
		}
		ret = sscanf(argv[1], "reg%d", &regx);
		ret = sscanf(argv[2], "%ld", &voltage);
		if(set_reg_voltage(regx, &voltage))
		{
			return -1;
		}
		pr_info("act8xxx_voltage_handle: Set reg%d voltage is %lumV\n", regx,voltage);
	}
	else
	{
		return -EINVAL;
		//ToDo help 
	}
	return 0;
}

static struct class_attribute act8xxx_class_attrs[] = {
	__ATTR(register_dump, S_IRUGO | S_IWUSR, act8xxx_register_dump, NULL),
	__ATTR(voltage, S_IRUGO | S_IWUSR, NULL, act8xxx_voltage_handle),
	__ATTR(test, S_IRUGO | S_IWUSR, act8xxx_test_show, act8xxx_test_store),
	__ATTR(debug, S_IRUGO | S_IWUSR, NULL, act8xxx_debug_store),	
	__ATTR_NULL
};

static struct class act8xxx_class = {
    .name = ACT8xxx_CLASS_NAME,
    .class_attrs = act8xxx_class_attrs,
};


#ifdef CONFIG_PMU_ACT8942
static int act8942_operations_init(struct act8942_operations* pdata)
{
	act8942_opts = pdata;
	if(act8942_opts->is_ac_online == NULL)
	{
		act8942_opts->is_ac_online = is_ac_online;
	}
	if(act8942_opts->is_usb_online == NULL)
	{
		act8942_opts->is_usb_online = is_usb_online;
	}
	if(act8942_opts->set_bat_off== NULL)
	{
		pr_err("act8942_opts->measure_voltage is NULL!\n");
		return -1;
	}
	if(act8942_opts->get_charge_status == NULL)
	{
		act8942_opts->get_charge_status = get_charge_status;
	}
	if(act8942_opts->set_charge_current == NULL)
	{
		pr_err("act8942_opts->set_charge_current is NULL!\n");
		return -1;
	}
	if(act8942_opts->measure_voltage == NULL)
	{
		pr_err("act8942_opts->measure_voltage is NULL!\n");
		return -1;
	}
	if(act8942_opts->measure_current == NULL)
	{
		pr_err("act8942_opts->measure_current is NULL!\n");
		return -1;
	}
	if(act8942_opts->measure_capacity_charging == NULL)
	{
		pr_err("act8942_opts->measure_capacity is NULL!\n");
		return -1;
	}
	if(act8942_opts->measure_capacity_battery== NULL)
	{
		pr_err("act8942_opts->measure_capacity is NULL!\n");
		return -1;
	}
	if(act8942_opts->update_period <= 0)
	{
		act8942_opts->update_period = 5000;
	}
	return 0;
}
static int capacity_sample_array[20]={
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};
static int capacity_sample_pointer = 0;

inline static int measure_capacity_advanced(void)	
{
	int current_capacity, i,capacity;	
    int num,sum,min,max;
    	
	int current_status = act8942_opts->is_ac_online();


	if(current_status)
	{
		current_capacity = act8942_opts->measure_capacity_charging();
	}
	else
	{
		current_capacity = act8942_opts->measure_capacity_battery();
	}

	act8xxx_dbg(dbg_enable, "current_status=%d,current_capacity = %d\n", current_status,current_capacity);
	
	if(act8942_opts->asn < 2)
	{
		return current_capacity;
	}
	
    capacity_sample_array[capacity_sample_pointer] = current_capacity;
    capacity_sample_pointer ++;
    
    if(capacity_sample_pointer >= act8942_opts->asn){
        capacity_sample_pointer = 0;
    }
    	
    sum = 0;
    num = 0;
    min = 100;
    max = 0; 
    
    for(i = 0;i<act8942_opts->asn;i++){
        if(capacity_sample_array[i] != -1){
            sum += capacity_sample_array[i];
            num ++;
            if(max < capacity_sample_array[i])
                max = capacity_sample_array[i];
            if(min > capacity_sample_array[i])
                min = capacity_sample_array[i];    
        }       
    }
  
    // drop max and min capacity
    if(num>2){
        sum = sum - max -min;
    }
    else{
        return -1;
    }
    
    current_capacity = sum/(num-2);   
    
	if(act8942_opts->rvp&&act8xxx_dev->capacity != -1)
	{
        if (current_status) {
            //ac online   don't report percentage smaller than prev 
        	capacity = (current_capacity > act8xxx_dev->capacity) ? current_capacity : act8xxx_dev->capacity;
        } else {
            //ac  not online   don't report percentage bigger than prev 
            capacity = (current_capacity < act8xxx_dev->capacity) ? current_capacity : act8xxx_dev->capacity;
        }

	}
	else
	{
		capacity = current_capacity;
	}
	act8xxx_dbg(dbg_enable, "sum=%d,num =%d,max=%d,min=%d,real_capacity =%d,capacity =%d \n", sum,num,max,min,current_capacity,capacity);		
	return capacity;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#ifdef CONFIG_PMU_ACT8942	
static void act8xxx_suspend(struct early_suspend *h)
{
	act8942_opts->set_charge_current(1);
	pr_info("fast charger on early_suspend\n\n");    
}

static void act8xxx_resume(struct early_suspend *h)
{
    act8942_opts->set_charge_current(0);
	pr_info("slow charger on resume\n\n");
}

#elif defined(CONFIG_PMU_ACT8862)
static void act8xxx_suspend(struct early_suspend *h)
{
	pr_info("fast charger on early_suspend\n\n");    
}

static void act8xxx_resume(struct early_suspend *h)
{
	pr_info("slow charger on resume\n\n");
}

#endif

static struct early_suspend act8xxx_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.suspend = act8xxx_suspend,
	.resume = act8xxx_resume,
	.param = NULL,
};
#endif

static int act8xxx_i2c_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	//struct act8xxx_device_info *act8xxx_dev;	//Elvis Fool
	int num;
	int retval = 0;

	pr_info("act8xxx_i2c_probe\n");

#ifdef CONFIG_PMU_ACT8942		
	if(act8942_operations_init((struct act8942_operations*)client->dev.platform_data))
	{
		dev_err(&client->dev, "failed to init act8942_opts!\n");
		return -EINVAL;
	}
#endif
	
	/* Get new ID for the new PMU device */
	retval = idr_pre_get(&pmu_id, GFP_KERNEL);
	if (retval == 0)
	{
		return -ENOMEM;
	}

	mutex_lock(&pmu_mutex);
	retval = idr_get_new(&pmu_id, client, &num);
	mutex_unlock(&pmu_mutex);
	if (retval < 0)
	{
		return retval;
	}

	act8xxx_dev = kzalloc(sizeof(*act8xxx_dev), GFP_KERNEL);
	if (!act8xxx_dev) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto act8xxx_failed_2;
	}
	act8xxx_dev->id = num;
#ifdef CONFIG_PMU_ACT8942		
	act8xxx_dev->capacity = -1;
#endif	
	//act8xxx_dev->chip = id->driver_data; //elvis

	this_client = client;

	i2c_set_clientdata(client, act8xxx_dev);
	act8xxx_dev->dev = &client->dev;
	act8xxx_dev->client = client;

#ifdef CONFIG_PMU_ACT8942	
	act8942_powersupply_init(act8xxx_dev);

	retval = power_supply_register(&client->dev, &act8xxx_dev->bat);
	if (retval) {
		dev_err(&client->dev, "failed to register battery\n");
		goto act8xxx_failed_2;
	}

	retval = power_supply_register(&client->dev, &act8xxx_dev->ac);
	if (retval) {
		dev_err(&client->dev, "failed to register ac\n");
		goto act8xxx_failed_2;
	}
	
	retval = power_supply_register(&client->dev, &act8xxx_dev->usb);
	if (retval) {
		dev_err(&client->dev, "failed to register usb\n");
		goto act8xxx_failed_2;
	}

	INIT_WORK(&(act8xxx_dev->work_update), update_work_func);
	
	init_timer(&(act8xxx_dev->polling_timer));
	act8xxx_dev->polling_timer.expires = jiffies + msecs_to_jiffies(act8942_opts->update_period);
	act8xxx_dev->polling_timer.function = polling_func;
	act8xxx_dev->polling_timer.data = act8xxx_dev;
    add_timer(&(act8xxx_dev->polling_timer));
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&act8xxx_early_suspend);
#endif
	dev_info(&client->dev, "support ver. %s enabled\n", DRIVER_VERSION);

	return 0;

act8xxx_failed_2:
	kfree(act8xxx_dev);
act8xxx_failed_1:
	mutex_lock(&pmu_mutex);
	idr_remove(&pmu_id, num);
	mutex_unlock(&pmu_mutex);

	return retval;
}

static int act8xxx_i2c_remove(struct i2c_client *client)
{
	struct act8xxx_device_info *act8xxx_dev = i2c_get_clientdata(client);
	pr_info("act8xxx_i2c_remove\n");
#ifdef CONFIG_PMU_ACT8942	
	power_supply_unregister(&act8xxx_dev->bat);
	del_timer(&(act8xxx_dev->polling_timer));
	kfree(act8xxx_dev->bat.name);
#endif

	mutex_lock(&pmu_mutex);
	idr_remove(&pmu_id, act8xxx_dev->id);
	mutex_unlock(&pmu_mutex);

	kfree(act8xxx_dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&act8xxx_early_suspend);
#endif
	return 0;
}

static int act8xxx_open(struct inode *inode, struct file *file)
{
    pmu_dev_t *pmu_dev;
	pr_info("act8xxx_open\n");
    /* Get the per-device structure that contains this cdev */
    pmu_dev = container_of(inode->i_cdev, pmu_dev_t, cdev);
    file->private_data = pmu_dev;

    return 0;
}

static int act8xxx_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;

    return 0;
}

static int act8xxx_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int size, i;
	act8xxx_i2c_msg_t *msgs = NULL;
	//struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct i2c_client *client = this_client;

	if(_IOC_DIR(cmd) == _IOC_READ)
	{
		size = _IOC_SIZE(cmd);
		if(size)
		{

			msgs = kmalloc((sizeof(act8xxx_i2c_msg_t)*size), GFP_KERNEL);
			if(!msgs)
			{
				pr_err("act8xxx_ioctl: failed to allocate memory for act8xxx_i2c_msgs.\n");
				return -ENOMEM;
			}

			ret = copy_from_user(msgs, (void __user *)arg, (sizeof(act8xxx_i2c_msg_t)*size));
			if(ret)
            {
                pr_err("act8xxx_ioctl: copy_from_user failed!\n ");
				kfree(msgs);
				return ret;
            }

			for(i=0; i<size; i++)
			{
				act8xxx_read_i2c(client, msgs[i].reg, &msgs[i].val);
			}

			copy_to_user((void __user *)arg, msgs, (sizeof(act8xxx_i2c_msg_t)*size));
			kfree(msgs);
			return 0;
		}
		else
		{
			return -EINVAL;
		}
	}
	
	if(_IOC_DIR(cmd) == _IOC_WRITE)
	{
		size = _IOC_SIZE(cmd);
		if(size)
		{

			msgs = kmalloc((sizeof(act8xxx_i2c_msg_t)*size), GFP_KERNEL);
			if(!msgs)
			{
				pr_err("act8xxx_ioctl: failed to allocate memory for act8xxx_i2c_msgs.\n");
				return -ENOMEM;
			}

			ret = copy_from_user(msgs, (void __user *)arg, (sizeof(act8xxx_i2c_msg_t)*size));
			if(ret)
            {
                pr_err("act8xxx_ioctl: copy_from_user failed!\n ");
				kfree(msgs);
				return ret;
            }

			for(i=0; i<size; i++)
			{
				act8xxx_write_i2c(client, msgs[i].reg, &msgs[i].val);
			}
			kfree(msgs);
			return 0;
		}
		else
		{
			return -EINVAL;
		}
	}
    return ret;
}


static struct file_operations act8xxx_fops = {
    .owner   = THIS_MODULE,
    .open    = act8xxx_open,
    .release = act8xxx_release,
    .ioctl   = act8xxx_ioctl,
};
#ifdef CONFIG_PMU_ACT8942
static void pmu_power_off(void)
{
	u8 val = 0;
	int ret = 0;
	if(act8942_opts->is_ac_online()){ //AC in after power off press
        arm_pm_restart("","charging_reboot");
	}
    
	ret = act8xxx_read_i2c(this_client, ACT8xxx_REG4_ADDR+1, &val);
	val = val|0x80;
	//printk("val = %x\n",val);
  ret = act8xxx_write_i2c(this_client, ACT8xxx_REG4_ADDR+1, &val);
  val = val&(~(0x80));
	//printk("val = %x\n",val);    
  ret = act8xxx_write_i2c(this_client, ACT8xxx_REG4_ADDR+1, &val);    
}
#endif

static int act8xxx_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev_p;
	
	pr_info("act8xxx_probe\n");
	act8xxx_pmu_dev = kmalloc(sizeof(pmu_dev_t), GFP_KERNEL);
	if (!act8xxx_pmu_dev)
	{
		pr_err("act8xxx: failed to allocate memory for pmu device\n");
		return -ENOMEM;
	}

	ret = alloc_chrdev_region(&act8xxx_devno, 0, 1, ACT8xxx_DEVICE_NAME);
	if (ret < 0) {
		pr_err("act8xxx: failed to allocate chrdev. \n");
		return 0;
	}

	/* connect the file operations with cdev */
	cdev_init(&act8xxx_pmu_dev->cdev, &act8xxx_fops);
	act8xxx_pmu_dev->cdev.owner = THIS_MODULE;

	/* connect the major/minor number to the cdev */
	ret = cdev_add(&act8xxx_pmu_dev->cdev, act8xxx_devno, 1);
	if (ret) {
		pr_err("act8xxx: failed to add device. \n");
		/* @todo do with error */
		return ret;
	}

	ret = class_register(&act8xxx_class);
	if(ret)
	{
		printk(" class register i2c_class fail!\n");
		return ret;
	}

	/* create /dev nodes */
    dev_p = device_create(&act8xxx_class, NULL, MKDEV(MAJOR(act8xxx_devno), 0),
                        NULL, "act8xxx");
    if (IS_ERR(dev_p)) {
        pr_err("act8xxx: failed to create device node\n");
        /* @todo do with error */
        return PTR_ERR(dev_p);;
    }

    printk( "act8xxx: driver initialized ok\n");
#ifdef CONFIG_PMU_ACT8942		    
    if(pm_power_off == NULL)
	    pm_power_off = pmu_power_off;
#endif	
    return ret;
}

static int act8xxx_remove(struct platform_device *pdev)
{
	pr_info("act8xxx_remove\n");
    cdev_del(&act8xxx_pmu_dev->cdev);
    unregister_chrdev_region(act8xxx_devno, 1);
    kfree(act8xxx_pmu_dev);

     return 0;
}


static const struct i2c_device_id act8xxx_i2c_id[] = {
	{ ACT8xxx_I2C_NAME, 0 },
	{},
};


static struct i2c_driver act8xxx_i2c_driver = {
	.driver = {
		.name = "ACT8xxx-PMU",
	},
	.probe = act8xxx_i2c_probe,
	.remove = act8xxx_i2c_remove,
	.id_table = act8xxx_i2c_id,
};

static struct platform_driver ACT8xxx_platform_driver = {
	.probe = act8xxx_probe,
    .remove = act8xxx_remove,
	.driver = {
	.name = ACT8xxx_DEVICE_NAME,
	},
};


static int __init act8xxx_pmu_init(void)
{
	int ret;
	pr_info("act8xxx_pmu_init\n");
	ret = platform_driver_register(&ACT8xxx_platform_driver);
	if (ret) {
        printk(KERN_ERR "failed to register ACT8xxx module, error %d\n", ret);
        return -ENODEV;
    }
	ret = i2c_add_driver(&act8xxx_i2c_driver);
	if (ret < 0)
	{
        pr_err("act8xxx: failed to add i2c driver. \n");
        ret = -ENOTSUPP;
    }

	return ret;
}
module_init(act8xxx_pmu_init);

static void __exit act8xxx_pmu_exit(void)
{
	pr_info("act8xxx_pmu_exit\n");
	i2c_del_driver(&act8xxx_i2c_driver);
    platform_driver_unregister(&ACT8xxx_platform_driver);
}
module_exit(act8xxx_pmu_exit);

MODULE_AUTHOR("Elvis Yu <elvis.yu@amlogic.com>");
MODULE_DESCRIPTION("ACT8xxx PMU driver");
MODULE_LICENSE("GPL");


