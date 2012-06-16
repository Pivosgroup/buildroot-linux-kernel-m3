/*
 * Common power driver for Amlogic Devices with one or two external
 * power supplies (AC/USB) connected to main and backup batteries,
 * and optional builtin charger.
 *
 * Copyright © 2010 Larson Jiang <larson.jiang@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/aml_power.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/usb/otg.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend power_early_suspend;
#endif

//#define AML_POWER_DBG
#define BATTERY_ARROW_NUM  20



static inline unsigned int get_irq_flags(struct resource *res)
{
	unsigned int flags = IRQF_SAMPLE_RANDOM | IRQF_SHARED;

	flags |= res->flags & IRQF_TRIGGER_MASK;

	return flags;
}

static struct device *dev;
static struct aml_power_pdata *pdata;
static struct resource *ac_irq, *usb_irq;
static struct timer_list charger_timer;
static struct timer_list supply_timer;
static struct timer_list polling_timer;
static int polling;

#ifdef CONFIG_USB_OTG_UTILS
static struct otg_transceiver *transceiver;
#endif
static struct regulator *ac_draw;

enum {
	AML_PSY_OFFLINE = 0,
	AML_PSY_ONLINE = 1,
	AML_PSY_TO_CHANGE,
};
static int new_ac_status = -1;
static int new_usb_status = 0;
static int ac_status = -1;
static int usb_status = 0;
static int battery_capacity = 100;
static int new_battery_capacity = 100;
static int battery_capacity_pos = -1;
static int charge_status = -1;
static int new_charge_status = -1;

static int bat_debug = 0;

static int aml_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
    //int capacty;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#ifdef AML_POWER_DBG
		printk(KERN_INFO "get POWER_SUPPLY_TYPE_MAINS\n");
#endif
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = pdata->is_ac_online ?
				      pdata->is_ac_online() : 0;
		else
			val->intval = pdata->is_usb_online ?
				      pdata->is_usb_online() : 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = pdata->get_bat_vol()*((3300000*3/2)/1024);
#ifdef AML_POWER_DBG
		printk(KERN_INFO "curren voltage is :%dmV\n",val->intval);
#endif
		break;
	case POWER_SUPPLY_PROP_STATUS:
#ifdef AML_POWER_DBG
		printk(KERN_INFO "get POWER_SUPPLY_PROP_STATUS\n");
#endif
		if((pdata->is_ac_online()) 
			|| ((usb_status == AML_PSY_ONLINE)&&(pdata->is_support_usb_charging == 1)))
		{
			if(pdata->get_charge_status())
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
		}
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:	
		val->intval = new_battery_capacity;
#ifdef AML_POWER_DBG
		printk(KERN_INFO "current capacity is %d%%\n,",val->intval);
#endif
		break;
	case POWER_SUPPLY_PROP_HEALTH:	
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:	
		val->intval = 1;
		break;
    case POWER_SUPPLY_PROP_TECHNOLOGY:
        val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
        break;						
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property aml_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property aml_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,    
    POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static char *aml_power_supplied_to[] = {
	"main-battery",
	"backup-battery",
};

static struct power_supply aml_psy_ac = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.supplied_to = aml_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(aml_power_supplied_to),
	.properties = aml_power_props,
	.num_properties = ARRAY_SIZE(aml_power_props),
	.get_property = aml_power_get_property,
};

static struct power_supply aml_psy_usb = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.supplied_to = aml_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(aml_power_supplied_to),
	.properties = aml_power_props,
	.num_properties = ARRAY_SIZE(aml_power_props),
	.get_property = aml_power_get_property,
};

static struct power_supply aml_psy_bat = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = aml_battery_props,
	.num_properties = ARRAY_SIZE(aml_battery_props),
	.get_property = aml_power_get_property,
};

static unsigned bat_matrix[BATTERY_ARROW_NUM] = {0};
static int count = 0;


static void get_bat_capacity(void)
{
    int value,i,num,sum;
    int min,max;
         
    if(new_ac_status > 0)//dc pluged state   
        value = pdata->get_bat_vol() - 12;
    else        
        value = pdata->get_bat_vol();

    if(bat_debug) printk("get_bat_vol = %d\n",value);
  
    if(value == -1)
        return; 
    
    bat_matrix[count] = value;
    count ++;
    
    if(count > (BATTERY_ARROW_NUM-1)){
        count = 0;
    }
    else if(battery_capacity_pos >=0){//battery_capacity_pos = -1,don't return for boot up read battery level
        return ;
   }        
    sum = 0;
    num = 0;
    min = 0x3ff;
    max = 0;  
    for(i = 0;i<=(BATTERY_ARROW_NUM-1);i++){
        if(bat_matrix[i]){
            sum += bat_matrix[i];
            num ++;
            if(max < bat_matrix[i])
                max = bat_matrix[i];
            if(min > bat_matrix[i])
                min = bat_matrix[i];           
        }
    }

    if(num>4){
        sum = sum - max -min;
        num = num -2;
    }
    else{
        return;
    }
       
    value = sum/num;    
    if(new_ac_status > 0){
        for(i=0; i<(pdata->bat_table_len -1); i++){
            if(((pdata->bat_charge_value_table)[i]<=value)&&((pdata->bat_charge_value_table)[i+1]>value))break;
        } 
    }
    else{
 
        for(i=0; i<(pdata->bat_table_len -1); i++){
            if(((pdata->bat_value_table)[i]<=value)&&((pdata->bat_value_table)[i+1]>value))break;
        }          
    }

    if(bat_debug) printk("AML_POWER_DBG i = %d,battery_capacity_pos = %d,(pdata->bat_level_table)[i] = %d,battery_capacity = %d\n",i,battery_capacity_pos,(pdata->bat_level_table)[i],battery_capacity);

    if(battery_capacity_pos >=0){
        if((battery_capacity_pos - i) >1){
            i = battery_capacity_pos - 1;
        }
        else if((i - battery_capacity_pos) >1){
            i = battery_capacity_pos + 1;
        }  
        
        if(new_ac_status > 0){//ac plug state,don't report a bat level small than before
            if(battery_capacity <= (pdata->bat_level_table)[i]){
                new_battery_capacity = (pdata->bat_level_table)[i];
                battery_capacity_pos = i;
            }
        }    
    
        if(new_ac_status == 0){//ac unplug state,don't report a bat level big than before
            if(battery_capacity >= (pdata->bat_level_table)[i]){
                new_battery_capacity = (pdata->bat_level_table)[i];
                battery_capacity_pos = i;
            }
        }   

    if(bat_debug) printk("AML_POWER_DBG i = %d,battery_capacity_pos = %d\n",i,battery_capacity_pos);
                   
    }
    else{
        new_battery_capacity = (pdata->bat_level_table)[i];
        battery_capacity_pos = i;        
    }    
           

    if(bat_debug) printk("battery_capacity = %d,max = %d,min = %d,sum = %d,num = %d,value = %d\n",new_battery_capacity,max,min,sum,num,value);

}
#ifdef CONFIG_HAS_EARLYSUSPEND
static void aml_power_early_suspend(struct early_suspend *h)
{
	if (pdata->set_charge) {
		pdata->set_charge(AML_POWER_CHARGE_AC);
        if(bat_debug) printk("fast charger on early_suspend\n\n");
	}      
}

static void aml_power_late_resume(struct early_suspend *h)
{
    int i;
    
	if (pdata->set_charge) {
		pdata->set_charge(0);
    if(bat_debug) printk("set slow charge\n");
	} 
	//update real battery level
	battery_capacity_pos = -1;
    count = 0;
    for(i = 0;i <= (BATTERY_ARROW_NUM-1);i++){
        bat_matrix[i] = 0;
    } 	
}
#endif

static void update_status(void)
{
    int i;
	if (pdata->is_ac_online)
		new_ac_status = !!pdata->is_ac_online();
		
    if(new_ac_status != ac_status){//unplug&plug ac,clear matrix 
        count = 0;
        for(i = 0;i <= (BATTERY_ARROW_NUM-1);i++){
            bat_matrix[i] = 0;
        }  
    }   
	//if (pdata->is_usb_online)  //usb not use polling
		//new_usb_status = !!pdata->is_usb_online();
		
	if (pdata->get_charge_status&&pdata->is_ac_online){
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
    
    get_bat_capacity();  
      
}


static void supply_timer_func(unsigned long unused)
{
	if (ac_status == AML_PSY_TO_CHANGE) {
		if(new_ac_status) {
			if(pdata->ic_control)
				pdata->ic_control(1);
		}
		else {
			if(pdata->ic_control)
				pdata->ic_control(0);
		}

		ac_status = new_ac_status;
		power_supply_changed(&aml_psy_ac);			  
	}

	if (usb_status == AML_PSY_TO_CHANGE) {
		if(new_usb_status) {
			if(pdata->ic_control)
				pdata->ic_control(1);
		}
		else {
			if(pdata->ic_control)
				pdata->ic_control(0);
		}
		usb_status = new_usb_status;	
		power_supply_changed(&aml_psy_usb);
	}
	
	if (charge_status == AML_PSY_TO_CHANGE) {
		charge_status = new_charge_status;	
		power_supply_changed(&aml_psy_bat);
	}

	if (battery_capacity == AML_PSY_TO_CHANGE) {
		battery_capacity = new_battery_capacity;		
		power_supply_changed(&aml_psy_bat);
	}			
}

static void psy_changed(void)
{

	/*
	 * Okay, charger set. Now wait a bit before notifying supplicants,
	 * charge power should stabilize.
	 */
	 
	//mcli
	if(supply_timer.function==NULL)
	    return;
		    
	if(pdata==NULL) 
    {  
        mod_timer(&supply_timer,jiffies + msecs_to_jiffies(500));
		
		return;
    }
    //mcli end
    
	mod_timer(&supply_timer,
		  jiffies + msecs_to_jiffies(pdata->wait_for_charger));
}

static void charger_timer_func(unsigned long unused)
{
	update_status();
	psy_changed();
}

static irqreturn_t power_changed_isr(int irq, void *power_supply)
{
	if (power_supply == &aml_psy_ac)
		ac_status = AML_PSY_TO_CHANGE;
	else if (power_supply == &aml_psy_usb)
		usb_status = AML_PSY_TO_CHANGE;
	else
		return IRQ_NONE;

	/*
	 * Wait a bit before reading ac/usb line status and setting charger,
	 * because ac/usb status readings may lag from irq.
	 */
	mod_timer(&charger_timer,
		  jiffies + msecs_to_jiffies(pdata->wait_for_status));

	return IRQ_HANDLED;
}

static void polling_timer_func(unsigned long unused)
{
	int changed = 0;

	dev_dbg(dev, "polling...\n");

	update_status();

	if (!ac_irq && new_ac_status != ac_status) {
		ac_status = AML_PSY_TO_CHANGE;
		changed = 1;
	}

	if (!usb_irq && new_usb_status != usb_status) {
		usb_status = AML_PSY_TO_CHANGE;
		changed = 1;
	}

	if (!ac_irq && charge_status != new_charge_status) {
		charge_status = AML_PSY_TO_CHANGE;
		changed = 1;
	}
	
	if(!ac_irq&&new_battery_capacity != battery_capacity){
		battery_capacity = AML_PSY_TO_CHANGE;
		changed = 1;
    }
    	
	if (changed)
		psy_changed();
    
	mod_timer(&polling_timer,
		  jiffies + msecs_to_jiffies(pdata->polling_interval));
}

#ifdef CONFIG_USB_OTG_UTILS
static int otg_is_usb_online(void)
{
	return (transceiver->state == OTG_STATE_B_PERIPHERAL);
}
#endif
#ifdef CONFIG_USB_ANDROID
int pc_connect(int status) 
{
    new_usb_status = status; 
    if(new_usb_status == usb_status)
        return 1;
    usb_status = AML_PSY_TO_CHANGE;
    psy_changed(); 
    return 0;
} 
static int gadget_is_usb_online(void)
{
	return usb_status;
}
EXPORT_SYMBOL(pc_connect);

#endif
static ssize_t store_powerhold(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{
	if(buf[0] == 'y'){
    if(bat_debug) printk("system off\n");    
        if(pdata->set_bat_off)
        pdata->set_bat_off();
    }

	return count;
}

static ssize_t store_debug(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{
	if(buf[0] == '1'){
	   bat_debug = 1; 
    }
    else{
	   bat_debug = 0;         
    }        
	return count;
}

static ssize_t show_debug(struct class *class, 
			struct class_attribute *attr,	char *buf)
{
	return sprintf(buf, "bat-debug value is %d\n", bat_debug);
}

static struct class_attribute powerhold_class_attrs[] = {
    __ATTR(bat-off,  S_IRUGO | S_IWUSR, NULL,    store_powerhold),
    __ATTR(bat-debug,  S_IRUGO | S_IWUSR, show_debug,    store_debug),    
    __ATTR_NULL
};

static struct class powerhold_class = {
    .name = "powerhold",
    .class_attrs = powerhold_class_attrs,
};

static int aml_power_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev = &pdev->dev;

	if (pdev->id != -1) {
		dev_err(dev, "it's meaningless to register several "
			"pda_powers; use id = -1\n");
		ret = -EINVAL;
		goto wrongid;
	}

	pdata = pdev->dev.platform_data;

	if (pdata->init) {
		ret = pdata->init(dev);
		if (ret < 0)
			goto init_failed;
	}
    if(pdata->bat_value_table == NULL
        ||pdata->bat_charge_value_table == NULL
        ||pdata->bat_level_table == NULL
        ||pdata->bat_table_len <= 0
        ){
        goto init_failed;        
    }
        
	update_status();

	if (!pdata->wait_for_status)
		pdata->wait_for_status = 500;

	if (!pdata->wait_for_charger)
		pdata->wait_for_charger = 500;

	if (!pdata->polling_interval)
		pdata->polling_interval = 2000;

	if (!pdata->ac_max_uA)
		pdata->ac_max_uA = 500000;

	setup_timer(&charger_timer, charger_timer_func, 0);
	setup_timer(&supply_timer, supply_timer_func, 0);

	ac_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "ac");
	usb_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "usb");

	if (pdata->supplied_to) {
		aml_psy_ac.supplied_to = pdata->supplied_to;
		aml_psy_ac.num_supplicants = pdata->num_supplicants;
		aml_psy_usb.supplied_to = pdata->supplied_to;
		aml_psy_usb.num_supplicants = pdata->num_supplicants;
	}

	ac_draw = regulator_get(dev, "ac_draw");
	if (IS_ERR(ac_draw)) {
		dev_dbg(dev, "couldn't get ac_draw regulator\n");
		ac_draw = NULL;
		ret = PTR_ERR(ac_draw);
	}

	if (pdata->is_ac_online) {
	            
		ret = power_supply_register(&pdev->dev, &aml_psy_ac);
		if (ret) {
			dev_err(dev, "failed to register %s power supply\n",
				aml_psy_ac.name);
			goto ac_supply_failed;
		}

		if (ac_irq) {
			ret = request_irq(ac_irq->start, power_changed_isr,
					  get_irq_flags(ac_irq), ac_irq->name,
					  &aml_psy_ac);
			if (ret) {
				dev_err(dev, "request ac irq failed\n");
				goto ac_irq_failed;
			}
		} else {
			polling = 1;
		}
	}
   	ret = class_register(&powerhold_class);
	if(ret){
		printk(" class register powerhold_class fail!\n");
	}
#ifdef CONFIG_USB_OTG_UTILS
	transceiver = otg_get_transceiver();
	if (transceiver && !pdata->is_usb_online) {
		pdata->is_usb_online = otg_is_usb_online;
	}
#endif
#ifdef CONFIG_USB_ANDROID
    pdata->is_usb_online = gadget_is_usb_online;
#endif
	if (pdata->is_usb_online) {
		ret = power_supply_register(&pdev->dev, &aml_psy_usb);
		if (ret) {
			dev_err(dev, "failed to register %s power supply\n",
				aml_psy_usb.name);
			goto usb_supply_failed;
		}

		if (usb_irq) {
			ret = request_irq(usb_irq->start, power_changed_isr,
					  get_irq_flags(usb_irq),
					  usb_irq->name, &aml_psy_usb);
			if (ret) {
				dev_err(dev, "request usb irq failed\n");
				goto usb_irq_failed;
			}
		} else {
			polling = 1;
		}
	}

	ret = power_supply_register(&pdev->dev, &aml_psy_bat);
	if (ret) {
		dev_err(dev, "failed to register battery\n");
		goto bat_supply_failed;
	}
	
	if (polling) {
		dev_dbg(dev, "will poll for status\n");
		setup_timer(&polling_timer, polling_timer_func, 0);
		mod_timer(&polling_timer,
			  jiffies + msecs_to_jiffies(pdata->polling_interval));
	}

	if (ac_irq || usb_irq)
		device_init_wakeup(&pdev->dev, 1);
#ifdef CONFIG_HAS_EARLYSUSPEND
    power_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    power_early_suspend.suspend = aml_power_early_suspend;
    power_early_suspend.resume = aml_power_late_resume;
    power_early_suspend.param = pdev;
	register_early_suspend(&power_early_suspend);
#endif

	if (pdata->set_charge) {
		pdata->set_charge(0);
	}
	
//	//power off when low power
//    get_bat_capacity();
//	if (pdata->is_ac_online) {
//        if((new_battery_capacity <=4)&&(!pdata->is_ac_online())){
//            if(pdata->set_bat_off)
//                pdata->set_bat_off();
//        }	
//    }    

    
	return 0;
bat_supply_failed:
	power_supply_unregister(&aml_psy_bat);
usb_irq_failed:
	if (pdata->is_usb_online)
		power_supply_unregister(&aml_psy_usb);
usb_supply_failed:
	if (pdata->is_ac_online && ac_irq)
		free_irq(ac_irq->start, &aml_psy_ac);
#ifdef CONFIG_USB_OTG_UTILS
	if (transceiver)
		otg_put_transceiver(transceiver);
#endif
ac_irq_failed:
	if (pdata->is_ac_online)
		power_supply_unregister(&aml_psy_ac);
ac_supply_failed:
	if (ac_draw) {
		regulator_put(ac_draw);
		ac_draw = NULL;
	}
	if (pdata->exit)
		pdata->exit(dev);
init_failed:
wrongid:
	return ret;
}

static int aml_power_remove(struct platform_device *pdev)
{
	if (pdata->is_usb_online && usb_irq)
		free_irq(usb_irq->start, &aml_psy_usb);
	if (pdata->is_ac_online && ac_irq)
		free_irq(ac_irq->start, &aml_psy_ac);

	if (polling)
		del_timer_sync(&polling_timer);
	del_timer_sync(&charger_timer);
	del_timer_sync(&supply_timer);
	
	power_supply_unregister(&aml_psy_bat);
	if (pdata->is_usb_online)
		power_supply_unregister(&aml_psy_usb);
	if (pdata->is_ac_online)
		power_supply_unregister(&aml_psy_ac);
	
#ifdef CONFIG_USB_OTG_UTILS
	if (transceiver)
		otg_put_transceiver(transceiver);
#endif
	if (ac_draw) {
		regulator_put(ac_draw);
		ac_draw = NULL;
	}
	if (pdata->exit)
		pdata->exit(dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&power_early_suspend);
#endif
	return 0;
}

#ifdef CONFIG_PM
static int ac_wakeup_enabled;
static int usb_wakeup_enabled;

static int aml_power_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (device_may_wakeup(&pdev->dev)) {
		if (ac_irq)
			ac_wakeup_enabled = !enable_irq_wake(ac_irq->start);
		if (usb_irq)
			usb_wakeup_enabled = !enable_irq_wake(usb_irq->start);
	}
    
	return 0;
}

static int aml_power_resume(struct platform_device *pdev)
{
	if (device_may_wakeup(&pdev->dev)) {
		if (usb_irq && usb_wakeup_enabled)
			disable_irq_wake(usb_irq->start);
		if (ac_irq && ac_wakeup_enabled)
			disable_irq_wake(ac_irq->start);
	}

	if(pdata->powerkey_led_onoff)
		pdata->powerkey_led_onoff(1);
    return 0;
}
#else
#define aml_power_suspend NULL
#define aml_power_resume NULL
#endif /* CONFIG_PM */

MODULE_ALIAS("platform:aml-power");

static struct platform_driver aml_power_pdrv = {
	.driver = {
		.name = "aml-power",
	},
	.probe = aml_power_probe,
	.remove = aml_power_remove,
	.suspend = aml_power_suspend,
	.resume = aml_power_resume,
};

static int __init aml_power_init(void)
{
	printk("amlogic power supply init\n");
	return platform_driver_register(&aml_power_pdrv);
}

static void __exit aml_power_exit(void)
{
	platform_driver_unregister(&aml_power_pdrv);
}

module_init(aml_power_init);
module_exit(aml_power_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Larson Jiang");
