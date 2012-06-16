/*
 * BQ27x00 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
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
#include <linux/bq27x00_battery.h>  
#include <asm/uaccess.h>
#include <linux/fs.h>            
#include <linux/device.h>    

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend bq27x00_early_suspend;
#endif
#define DRIVER_VERSION			"1.1.0"
#define SUPPORT_DFI_WRITE

#define BQ27x00_REG_TEMP		0x06
#define BQ27x00_REG_VOLT		0x08
#define BQ27x00_REG_AI			0x14
#define BQ27x00_REG_FLAGS		0x0A
#define BQ27x00_REG_TTE			0x16
#define BQ27x00_REG_TTF			0x18
#define BQ27x00_REG_TTECP		0x26

#define BQ27000_REG_RSOC		0x0B /* Relative State-of-Charge */
#define BQ27000_FLAG_CHGS		BIT(7)

#define BQ27500_REG_SOC			0x2c
#define BQ27500_FLAG_DSC		BIT(0)
#define BQ27500_FLAG_FC			BIT(9)

static int polling_count = 0;
static int ac_status = 0;
static int usb_status = 0;
static int new_usb_status = 0;
static int battery_capacity = 100;
static int new_battery_capacity = 100;
static int charge_status = -1;
static int new_charge_status = -1;
static char fw_ver[10] = {0};

/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);
static struct timer_list polling_timer;
static struct work_struct battery_work;
	
struct bq27x00_device_info;
struct bq27x00_access_methods {
	int (*read)(u8 reg, int *rt_value, int b_single,
		struct bq27x00_device_info *di);
};

enum bq27x00_chip { BQ27000, BQ27500 };
static struct bq27x00_battery_pdata *pdata;

struct bq27x00_device_info {
	struct device 		*dev;
	int			id;
    int config_major;
	struct bq27x00_access_methods	*bus;
	struct power_supply	bat;
	struct power_supply	ac;	
	struct power_supply	usb;	
	enum bq27x00_chip	chip;
	struct class *config_class;
	
	struct i2c_client	*client;
};
static struct bq27x00_device_info *device_info;

static enum power_supply_property ac_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};
static char *ac_supply_list[] = {
	"battery",
};
static enum power_supply_property bq27x00_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MANUFACTURER,
//	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
//	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
//	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

/*
 * Common code for BQ27x00 devices
 */
#ifdef SUPPORT_DFI_WRITE
static int _bq27x00_read_i2c(struct bq27x00_device_info *di,u16 slave_addr, u8 reg, u8 len, u8 *data);
static int _bq27x00_write_i2c(struct bq27x00_device_info *di,u16 slave_addr, u8 reg, u8 len, u8 *data);
#endif
static int bq27x00_read(u8 reg, int *rt_value, int b_single,
			struct bq27x00_device_info *di)
{
	return di->bus->read(reg, rt_value, b_single, di);
}

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or < 0 if something fails.
 */
static int bq27x00_battery_temperature(struct bq27x00_device_info *di)
{
	int ret;
	int temp = 0;

	ret = bq27x00_read(BQ27x00_REG_TEMP, &temp, 0, di);
	if (ret) {
		dev_err(di->dev, "error reading temperature\n");
		return ret;
	}

	if (di->chip == BQ27500)
		return (temp - 2731)/10;  //k
	else
		return ((temp >> 2) - 273);
}

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
static int bq27x00_battery_voltage(struct bq27x00_device_info *di)
{
	int ret;
	int volt = 0;

	ret = bq27x00_read(BQ27x00_REG_VOLT, &volt, 0, di);
	if (ret) {
		dev_err(di->dev, "error reading voltage\n");
		return ret;
	}

	return volt * 1000;
}

/*
 * Return the battery average current
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27x00_battery_current(struct bq27x00_device_info *di)
{
	int ret;
	int curr = 0;
	int flags = 0;

	ret = bq27x00_read(BQ27x00_REG_AI, &curr, 0, di);

	if (ret) {
		dev_err(di->dev, "error reading current\n");
		return 0;
	}

	if (di->chip == BQ27500) {
		/* bq27500 returns signed value */
		curr = (int)(s16)curr;
	} else {
		ret = bq27x00_read(BQ27x00_REG_FLAGS, &flags, 0, di);
		if (ret < 0) {
			dev_err(di->dev, "error reading flags\n");
			return 0;
		}
		if (flags & BQ27000_FLAG_CHGS) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}
	}

	return curr * 1000;
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27x00_battery_rsoc(struct bq27x00_device_info *di)
{
	int ret;
	int rsoc = 0;

	if (di->chip == BQ27500)
		ret = bq27x00_read(BQ27500_REG_SOC, &rsoc, 0, di);
	else
		ret = bq27x00_read(BQ27000_REG_RSOC, &rsoc, 1, di);
	if (ret) {
		dev_err(di->dev, "error reading relative State-of-Charge\n");
		return ret;
	}
	
	//drop wrong capacity value
    if(rsoc > 100){
        dev_err(di->dev, "error reading rsoc\n");
        rsoc = battery_capacity;
    }
    
	return rsoc;
}
static int bq27x00_battery_status(struct bq27x00_device_info *di)
{
	int flags = 0;
	int status;
	int ret;

	ret = bq27x00_read(BQ27x00_REG_FLAGS, &flags, 0, di);
	if (ret < 0) {
		dev_err(di->dev, "error reading flags\n");
		return ret;
	}

	if (di->chip == BQ27500) {
		if (flags & BQ27500_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (flags & BQ27500_FLAG_DSC)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (flags & BQ27000_FLAG_CHGS)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	return status;
}

/*
 * Read a time register.
 * Return < 0 if something fails.
 */
static int bq27x00_battery_time(struct bq27x00_device_info *di, int reg,
				union power_supply_propval *val)
{
	int tval = 0;
	int ret;

	ret = bq27x00_read(reg, &tval, 0, di);
	if (ret) {
		dev_err(di->dev, "error reading register %02x\n", reg);
		return ret;
	}

	if (tval == 65535)
		return -ENODATA;

	val->intval = tval * 60;
	return 0;
}
#ifdef SUPPORT_DFI_WRITE
static int bq27x00_battery_get_version(struct bq27x00_device_info *di)
{
    u8 data[2];
    
    mdelay(200);	    
    data[1] = 0x00;
    data[0] = 0x02;	
        
	_bq27x00_write_i2c(di,0x55,0x00,2,data);
    mdelay(10);	
    _bq27x00_read_i2c(di,0x55,0x00,2,data);
    printk("FW version = %x.%x\n", data[1],data[0]);	
    sprintf(fw_ver,"BQ%x.%x",data[1],data[0]);
    
    return 0;
}
#endif
#ifdef CONFIG_USB_ANDROID
int pc_connect(int status) 
{
    new_usb_status = status; 
    if(new_usb_status == usb_status)
        return 1;
    if(device_info){
        usb_status = new_usb_status;        
        power_supply_changed(&device_info->usb);
    }
    return 0;
} 
static int gadget_is_usb_online(void)
{
	return usb_status;
}
EXPORT_SYMBOL(pc_connect);

#endif

#define to_bq27x00_device_info(x) container_of((x), \
				struct bq27x00_device_info, bat);
static int ac_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int retval = 0;
	
    if (psy->type == POWER_SUPPLY_TYPE_MAINS)
        val->intval = pdata->is_ac_online ? pdata->is_ac_online() : 0;
    else
        val->intval = pdata->is_usb_online ? pdata->is_usb_online() : 0;

	return retval;
}
static int bq27x00_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
//		if(pdata->is_ac_online())
//		{
//			if(pdata->get_charge_status())
//				val->intval = POWER_SUPPLY_STATUS_FULL;
//			else
//				val->intval = POWER_SUPPLY_STATUS_CHARGING;
//		}
//		else
//			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
        val->intval = new_charge_status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bq27x00_battery_voltage(di);
		if (psp == POWER_SUPPLY_PROP_PRESENT)
			val->intval = val->intval <= 0 ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq27x00_battery_current(di);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = bq27x00_battery_rsoc(di);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bq27x00_battery_temperature(di);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
#ifdef SUPPORT_DFI_WRITE	 
        val->strval = fw_ver;   
#endif		
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


static void bq27x00_powersupply_init(struct bq27x00_device_info *di)
{
	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat.properties = bq27x00_battery_props;
	di->bat.num_properties = ARRAY_SIZE(bq27x00_battery_props);
	di->bat.get_property = bq27x00_battery_get_property;
	di->bat.external_power_changed = NULL;

    di->ac.name = "ac";
	di->ac.type = POWER_SUPPLY_TYPE_MAINS;
	di->ac.supplied_to = ac_supply_list,
	di->ac.num_supplicants = ARRAY_SIZE(ac_supply_list),
	di->ac.properties = ac_power_props;
	di->ac.num_properties = ARRAY_SIZE(ac_power_props);
	di->ac.get_property = ac_power_get_property;

    di->usb.name = "usb";
	di->usb.type = POWER_SUPPLY_TYPE_USB;
	di->usb.supplied_to = ac_supply_list,
	di->usb.num_supplicants = ARRAY_SIZE(ac_supply_list),
	di->usb.properties = ac_power_props;
	di->usb.num_properties = ARRAY_SIZE(ac_power_props);
	di->usb.get_property = ac_power_get_property;
	
}

/*
 * i2c specific code
 */

static int bq27x00_read_i2c(u8 reg, int *rt_value, int b_single,
			struct bq27x00_device_info *di)
{
	struct i2c_client *client = di->client;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int err;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = data;

	data[0] = reg;
	err = i2c_transfer(client->adapter, msg, 1);

	if (err >= 0) {
		if (!b_single)
			msg->len = 2;
		else
			msg->len = 1;

		msg->flags = I2C_M_RD;
		err = i2c_transfer(client->adapter, msg, 1);
		if (err >= 0) {
			if (!b_single)
				*rt_value = get_unaligned_le16(data);
			else
				*rt_value = data[0];

			return 0;
		}
	}
	return err;
}

static void battery_work_func(struct work_struct *work)
{
    bool ac_changed,bat_changed;
    static  int led_flash=1;
    
    ac_changed = false;
    bat_changed = false;
    polling_count ++;
    if(polling_count >= 6){
        polling_count = 0;
        new_battery_capacity = bq27x00_battery_rsoc(device_info);
    }
 if((pdata->set_lcd_off)&&(pdata->set_lcd_on)&&(new_battery_capacity>=0))
 	{
        if((new_battery_capacity<=15)&&(pdata->is_ac_online()==0))
        	{
                if(led_flash==1)
                 	{
                    led_flash=0;
                    pdata->set_lcd_off();
			        //printk(KERN_ERR " set_lcd_off\n");
                 	}
		        else
		   	        {
                    led_flash=1;
                    pdata->set_lcd_on();
			        //printk(KERN_ERR " set_lcd_on\n");
		   	        }
        	}
		else if(pdata->is_ac_online()==1)
	 	    {
                    led_flash=0;
                    pdata->set_lcd_on();
		            //printk(KERN_ERR " set_lcd_on\n");
	 	    }
        else
	 	    {
                    led_flash=1;
                    pdata->set_lcd_off();
		            //printk(KERN_ERR " set_lcd_off\n");
	 	    }
 	}

    if(pdata->is_ac_online){
        if(ac_status != pdata->is_ac_online()){
            ac_status = pdata->is_ac_online();
            ac_changed = true;      
        }
    }
    if (device_info->chip == BQ27500) {
       new_charge_status = bq27x00_battery_status(device_info);        
    }
    else if (pdata->get_charge_status&&pdata->is_ac_online){
		if(pdata->is_ac_online())
		{
			if(pdata->get_charge_status())
				new_charge_status = POWER_SUPPLY_STATUS_FULL;
			else
				new_charge_status = POWER_SUPPLY_STATUS_CHARGING;
		}
		else
			new_charge_status = POWER_SUPPLY_STATUS_DISCHARGING;	   
    }
       
    if(new_charge_status != charge_status||battery_capacity != new_battery_capacity){
        charge_status = new_charge_status;
        battery_capacity = new_battery_capacity;
        bat_changed = true;
    }
    
    if(bat_changed){
        power_supply_changed(&device_info->bat); 
    }    
        
    if(ac_changed){
        power_supply_changed(&device_info->ac);      
    }
}

static void polling_timer_func(unsigned long unused)
{
    schedule_work(&battery_work);
	mod_timer(&polling_timer,jiffies + msecs_to_jiffies(pdata->polling_interval));        
}
#ifdef SUPPORT_DFI_WRITE

static int _bq27x00_read_i2c(struct bq27x00_device_info *di,u16 slave_addr, u8 reg, u8 len, u8 *data)
{
    struct i2c_client *client = di->client;
    int ret;
    u8 msgbuf0[1] = { reg };
    u16 slave = slave_addr;
    u16 flags = 0;
    
    struct i2c_msg msg[2] = { 
        { slave, flags, 1, msgbuf0 },
        { slave, flags|I2C_M_RD, len, data }
    };

    ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
    return ret;
}

static int _bq27x00_write_i2c(struct bq27x00_device_info *di,u16 slave_addr, u8 reg, u8 len, u8 *data)
{
    struct i2c_client *client = di->client;
    u8 msgbuf0[1] = { reg };
    u16 slave = slave_addr;
    u16 flags = 0;
    
    struct i2c_msg msg[2] = {
        { slave, flags, 1, msgbuf0 },
        { slave, flags|I2C_M_NOSTART, len, data }
    };

    return i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
}

loff_t bq27x00_file_seek(struct file *filp, loff_t off, int whence)                      
{
        loff_t newpos;

        switch(whence) {
          case 0: /* SEEK_SET */
                newpos = off;
                break;
          default: /* can't happen */
                return -EINVAL;
        }

        if (newpos < 0) return -EINVAL;
        filp->f_pos = newpos;
        return newpos;
}

static int bq27x00_file_open(struct inode *inode, struct file *file)
{
    int ret = 0;
    u8 data[2];
	del_timer_sync(&polling_timer);
	flush_scheduled_work();
    mdelay(1000);
    
    data[0] = 0x02;
    data[1] = 0x00;
	_bq27x00_write_i2c(device_info,0x55,0x00,2,data);
    mdelay(10);	
    _bq27x00_read_i2c(device_info,0x55,0x00,2,data);
    printk("FW version = %x,%x\n", data[1],data[0]);


    data[0] = 0x00;
    data[1] = 0x0f;
	_bq27x00_write_i2c(device_info,0x55,0x00,2,data);	
		
    printk("bq27x00_file_open\n");
    return ret;
}

static int bq27x00_file_release(struct inode *inode, struct file *file)
{
    int ret = 0;
    u8 data[2];
    
    mdelay(200);	    
    data[1] = 0x00;
    data[0] = 0x02;	
        
	_bq27x00_write_i2c(device_info,0x55,0x00,2,data);
    mdelay(10);	
    _bq27x00_read_i2c(device_info,0x55,0x00,2,data);
    printk("FW version = %x,%x\n", data[0],data[1]);	
    	
    mdelay(250);	
    setup_timer(&polling_timer, polling_timer_func, 0);
    mod_timer(&polling_timer,
			  jiffies + msecs_to_jiffies(pdata->polling_interval));     
    printk("bq27x00_file_release\n");
    return ret;
}


static ssize_t bq27x00_file_read( struct file *file, char __user *buf,                     
    size_t count, loff_t *ppos )                                                    
{
    uint8_t buffer[20]={0};
    u16 addr = 0x0b;
    unsigned reg = *ppos;
//    int i;
        
    if(count > 128){
        printk("%s, buffer exceed 128 bytes\n", __func__);
        return -1;
    }
      
    _bq27x00_read_i2c(device_info,addr,reg,count,buffer);

    copy_to_user(buf,buffer,count);
  
    return count;
}

static ssize_t bq27x00_file_write( struct file *file, const char __user *buf,
    size_t count, loff_t *ppos )
{
    uint8_t buffer[100]={0};
    u16 addr = 0x0b;
    unsigned reg = *ppos;
    int i;

    if(count > 100){
        printk("%s, buffer exceed 100 bytes\n", __func__);
        return -1;
    }
 

    copy_from_user(buffer, buf, count);   
    
    for(i  = 0;i<count;i++){
        _bq27x00_write_i2c(device_info,addr,reg+i,1,buffer+i);
        mdelay(1);	
    }

    return -1;
}
          
static const struct file_operations bq27x00_fops = {
     .owner      = THIS_MODULE,
     .llseek     = bq27x00_file_seek,           
     .open       = bq27x00_file_open,
     .release    = bq27x00_file_release,
     .read       = bq27x00_file_read,
     .write      = bq27x00_file_write,
     .ioctl		 = NULL, 
 };
 

#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void early_suspend(struct early_suspend *h)
{
    	/* Kill the battery timer sync */
	del_timer_sync(&polling_timer);
	flush_scheduled_work();
	
	if (pdata->set_charge) {
		pdata->set_charge(1);
            printk("fast charger on early_suspend\n\n");
	}      
}

static void late_resume(struct early_suspend *h)
{

    setup_timer(&polling_timer, polling_timer_func, 0);
    mod_timer(&polling_timer,
			  jiffies + msecs_to_jiffies(pdata->polling_interval));    
	if (pdata->set_charge) {
		pdata->set_charge(0);
        printk("set slow charge\n");
	} 
}
#endif

static ssize_t store_powerhold(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{
	if(buf[0] == 'y'){
    printk("system off\n");    
        if(pdata->set_bat_off)
        pdata->set_bat_off();
    }

	return count;
}

static struct class_attribute powerhold_class_attrs[] = {
    __ATTR(bat-off,  S_IRUGO | S_IWUSR, NULL,    store_powerhold), 
    __ATTR_NULL
};

static struct class powerhold_class = {
    .name = "powerhold",
    .class_attrs = powerhold_class_attrs,
};

static int bq27x00_battery_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	char *name;
	struct bq27x00_device_info *di;
	struct bq27x00_access_methods *bus;
	int num;
	int retval = 0;

	/* Get new ID for the new battery device */
	retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;
	mutex_lock(&battery_mutex);
	retval = idr_get_new(&battery_id, client, &num);
	mutex_unlock(&battery_mutex);
	if (retval < 0)
		return retval;
    pdata = (struct bq27x00_battery_pdata*)client->dev.platform_data;
#ifdef CONFIG_USB_ANDROID
    pdata->is_usb_online = gadget_is_usb_online;
#endif
	if (pdata->set_charge) {
		pdata->set_charge(0);
        printk("set slow charge\n");
	} 
	
	name = kasprintf(GFP_KERNEL, "bq27x00");
	if (!name) {
		dev_err(&client->dev, "failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}
	di->id = num;
	di->chip = pdata->chip;

	bus = kzalloc(sizeof(*bus), GFP_KERNEL);
	if (!bus) {
		dev_err(&client->dev, "failed to allocate access method "
					"data\n");
		retval = -ENOMEM;
		goto batt_failed_3;
	}

	i2c_set_clientdata(client, di);
	di->dev = &client->dev;
	di->bat.name = name;
	bus->read = &bq27x00_read_i2c;
	di->bus = bus;
	di->client = client;

	bq27x00_powersupply_init(di);

	retval = power_supply_register(&client->dev, &di->bat);
	if (retval) {
		dev_err(&client->dev, "failed to register battery\n");
		goto batt_failed_4;
	}

	retval = power_supply_register(&client->dev, &di->ac);
	if (retval) {
		dev_err(&client->dev, "failed to register ac\n");
		goto batt_failed_4;
	}
	
	retval = power_supply_register(&client->dev, &di->usb);
	if (retval) {
		dev_err(&client->dev, "failed to register usb\n");
		goto batt_failed_4;
	}
	
   	retval = class_register(&powerhold_class);
	if(retval){
		printk(" class register powerhold_class fail!\n");
	}
		
	if (!pdata->polling_interval)
		pdata->polling_interval = 2000;


	INIT_WORK(&battery_work, battery_work_func);
    setup_timer(&polling_timer, polling_timer_func, 0);
    mod_timer(&polling_timer,
			  jiffies + msecs_to_jiffies(pdata->polling_interval));

#ifdef CONFIG_HAS_EARLYSUSPEND
    bq27x00_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    bq27x00_early_suspend.suspend = early_suspend;
    bq27x00_early_suspend.resume = late_resume;
    bq27x00_early_suspend.param = NULL;
	register_early_suspend(&bq27x00_early_suspend);
#endif
	
	dev_info(&client->dev, "support ver. %s enabled\n", DRIVER_VERSION);
	
	di->config_major = register_chrdev(0,di->bat.name,&bq27x00_fops);
    printk("bq27x00_fops majo = %d\n",di->config_major);
	if(di->config_major<=0){
		printk("register char device error\n");
	}	
	
    di->config_class=class_create(THIS_MODULE,di->bat.name);
    di->dev=device_create(di->config_class,	NULL,
    		MKDEV(di->config_major,0),NULL,di->bat.name);	
    		
    bq27x00_battery_get_version(di);		
    device_info = di;    		
	return 0;

batt_failed_4:
	kfree(bus);
batt_failed_3:
	kfree(di);
batt_failed_2:
	kfree(name);
batt_failed_1:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27x00_battery_remove(struct i2c_client *client)
{
	struct bq27x00_device_info *di = i2c_get_clientdata(client);

	power_supply_unregister(&di->bat);
	power_supply_unregister(&di->ac);
	power_supply_unregister(&di->usb);	
    unregister_chrdev(di->config_major,di->bat.name);
    if(di->config_class)
    {
        if(di->dev)
        device_destroy(di->config_class,MKDEV(di->config_major,0));
        class_destroy(di->config_class);
    }    
	kfree(di->bat.name);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&bq27x00_early_suspend);
#endif
	kfree(di);

	return 0;
}

/*
 * Module stuff
 */

static const struct i2c_device_id bq27x00_id[] = {
	{ "bq27200", BQ27000 },	/* bq27200 is same as bq27000, but with i2c */
	{ "bq27500", BQ27500 },
	{},
};

static struct i2c_driver bq27x00_battery_driver = {
	.driver = {
		.name = "bq27x00-battery",
	},
	.probe = bq27x00_battery_probe,
	.remove = bq27x00_battery_remove,
	.id_table = bq27x00_id,
};

static int __init bq27x00_battery_init(void)
{
	int ret;

	ret = i2c_add_driver(&bq27x00_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27x00 driver\n");

	return ret;
}
module_init(bq27x00_battery_init);

static void __exit bq27x00_battery_exit(void)
{
	i2c_del_driver(&bq27x00_battery_driver);
}
module_exit(bq27x00_battery_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27x00 battery monitor driver");
MODULE_LICENSE("GPL");
