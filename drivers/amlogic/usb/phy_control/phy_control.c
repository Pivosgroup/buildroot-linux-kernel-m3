/*
 *  /drivers/amlogic/usb/phy_control.c
 *
 *  Driver for amlogic usb clock/power/tuning...
 *
 *
 *  Copyright (C) 2011 Amlogic. By Victor Wan
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/nmi.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/irq.h>

static __inline__ uint32_t read_reg32( volatile uint32_t *_reg) 
{
        return readl(_reg);
};
static __inline__ void write_reg32( volatile uint32_t *_reg, const uint32_t _value) 
{
        writel( _value, _reg );
};
static __inline__ void clear_reg32_mask(volatile uint32_t *_reg, const uint32_t _mask)
{
	write_reg32(_reg,(read_reg32(_reg) & (~_mask)));
}
static __inline__ void set_reg32_mask(volatile uint32_t *_reg,const uint32_t _mask)
{
	write_reg32(_reg,(read_reg32(_reg) | (_mask)));
}

typedef enum {
    USB_PHY_TUNE_OTGDISABLE = 0,
    USB_PHY_TUNE_TX_VREF,
    USB_PHY_TUNE_RISETIME,
    USB_PHY_TUNE_SQRX,
    USB_PHY_TUNE_VBUS_THRE,
    USB_PHY_TUNE_REFCLKDIV,
    USB_PHY_TUNE_REFCLKSEL    
}USB_PHY_Tune_t;

typedef struct amlogic_usb_struct{
	int phy_index;
	
	/* Meet with Periphs Spec */
	volatile uint32_t phy_reg; //base address
	/*
	volatile uint32_t phy_a_reg1; //phy a
	volatile uint32_t phy_b_reg2; //phy b
	volatile uint32_t phy_a_reg3; //utmi a
	volatile uint32_t phy_b_reg4; //utmi b
	volatile uint32_t phy_reg5;
	*/
	USB_PHY_Tune_t tune;
	int32_t 	tune_value;
}amlogic_usb_struct_t;

static struct amlogic_usb_struct aus;

#define REG_MODE_COM	0
#define REG_MODE_PHY	1	//offset in aus struct
#define REG_MODE_UTMI	2	//offset in aus struct

static uint32_t decide_register_addr(amlogic_usb_struct_t *paus,int reg_mode)
{
	uint32_t addr;
	
	if(paus->phy_index != 0 && paus->phy_index != 1)
		return 0;
	if(paus->phy_reg == 0)
		return 0;

	/* Calculate the address */
	addr = paus->phy_reg + (paus->phy_index+1) * reg_mode * sizeof(uint32_t);

	return addr;
}

static int set_phy_tune(amlogic_usb_struct_t *paus)
{

    volatile uint32_t * phy_reg;
    unsigned int reg_val,mask;
    USB_PHY_Tune_t tune;
    int value,ret = 1;


    phy_reg = (volatile uint32_t *)decide_register_addr(paus,REG_MODE_PHY);
    if(phy_reg == NULL)
        return -1;
    tune = paus->tune;
    value = paus->tune_value;

    switch(tune)
    {
        case USB_PHY_TUNE_TX_VREF:
            if(value >= 0 && value <= 15)
                reg_val = value;
            else if(value == -1)
                reg_val = 8;
            else 
                return -1;
            mask = USB_PHY_TUNE_MASK_TXVREF;
            reg_val = reg_val << USB_PHY_TUNE_SHIFT_TXVREF;
            ret = 0;            
            break;
            
        case USB_PHY_TUNE_REFCLKDIV:
            if(value >= 0 && value <= 2)
                reg_val = value;
            else if(value == -1)
                reg_val = 0;
            else
                return -1;
            mask = USB_PHY_TUNE_MASK_REFCLKDIV;
            reg_val = reg_val << USB_PHY_TUNE_SHIFT_REFCLKDIV;
            ret = 0;             
            break;
            
        case USB_PHY_TUNE_OTGDISABLE:
            if(value == 0 || value == -1)
                reg_val = 0;
            else if(value == 1)
                reg_val = 1;
            else
                return -1;
            mask = USB_PHY_TUNE_MASK_OTGDISABLE;
            reg_val = reg_val << USB_PHY_TUNE_SHIFT_OTGDISABLE;
            ret = 0;               
            break;
            
        case USB_PHY_TUNE_REFCLKSEL:
            if(value >= 0 && value <= 3)
                reg_val = value;
            else if(value == -1)
                reg_val = 0;
            else
                return -1;
            mask = USB_PHY_TUNE_MASK_REFCLKSEL;
            reg_val = reg_val << USB_PHY_TUNE_SHIFT_REFCLKSEL;
            ret = 0;               
            break;
            
        case USB_PHY_TUNE_RISETIME:
             if(value >= 0 && value <= 3)
                reg_val = value;
            else if(value == -1)
                reg_val = 1;
            else
                return -1;
            mask = USB_PHY_TUNE_MASK_RISETIME;
            reg_val = reg_val << USB_PHY_TUNE_SHIFT_RISETIME;
            ret = 0;               
            break;
            
        case USB_PHY_TUNE_VBUS_THRE:
             if(value >= 0 && value <= 7)
                reg_val = value;
            else if(value == -1)
                reg_val = 4;
            else
                return -1;
            mask = USB_PHY_TUNE_MASK_VBUS_THRE;
            reg_val = reg_val << USB_PHY_TUNE_SHIFT_VBUS_THRE;
            ret = 0;               
            break;
            
        case USB_PHY_TUNE_SQRX:
             if(value >= 0 && value <= 7)
                reg_val = value;
            else if(value == -1)
                reg_val = 4;
            else
                return -1;
            mask = USB_PHY_TUNE_MASK_SQRX;
            reg_val = reg_val << USB_PHY_TUNE_SHIFT_SQRX;
            ret = 0;               
            break;
            
        default:
            return -1;
    }

    clear_reg32_mask(phy_reg,mask);
    set_reg32_mask(phy_reg,reg_val);
    //printk("set reg: 0x%x, val: 0x%x\n",(int)phy_reg,reg_val);
    return ret;	
}

static int get_phy_tune(amlogic_usb_struct_t *paus)
{

    volatile uint32_t * phy_reg;
    unsigned int shift,mask;
    USB_PHY_Tune_t tune;
    int value,ret = 1;


    phy_reg = (volatile uint32_t *)decide_register_addr(paus,REG_MODE_PHY);
    if(phy_reg == NULL)
        return -1;
    tune = paus->tune;
    value = read_reg32(phy_reg);

    switch(tune)
    {
        case USB_PHY_TUNE_TX_VREF:
            mask = USB_PHY_TUNE_MASK_TXVREF;
            shift = USB_PHY_TUNE_SHIFT_TXVREF;
            ret = 0;            
            break;
            
        case USB_PHY_TUNE_REFCLKDIV:
            mask = USB_PHY_TUNE_MASK_REFCLKDIV;
            shift = USB_PHY_TUNE_SHIFT_REFCLKDIV;
            ret = 0;             
            break;
            
        case USB_PHY_TUNE_OTGDISABLE:
            mask = USB_PHY_TUNE_MASK_OTGDISABLE;
            shift = USB_PHY_TUNE_SHIFT_OTGDISABLE;
            ret = 0;               
            break;
            
        case USB_PHY_TUNE_REFCLKSEL:
            mask = USB_PHY_TUNE_MASK_REFCLKSEL;
            shift  = USB_PHY_TUNE_SHIFT_REFCLKSEL;
            ret = 0;               
            break;
            
        case USB_PHY_TUNE_RISETIME:
            mask = USB_PHY_TUNE_MASK_RISETIME;
            shift = USB_PHY_TUNE_SHIFT_RISETIME;
            ret = 0;               
            break;
            
        case USB_PHY_TUNE_VBUS_THRE:
            mask = USB_PHY_TUNE_MASK_VBUS_THRE;
            shift = USB_PHY_TUNE_SHIFT_VBUS_THRE;
            ret = 0;               
            break;
            
        case USB_PHY_TUNE_SQRX:
            mask = USB_PHY_TUNE_MASK_SQRX;
            shift = USB_PHY_TUNE_SHIFT_SQRX;
            ret = 0;               
            break;
            
        default:
            return -1;
    }

    paus->tune_value = ((value & mask) >> shift);

    return ret;	
}
static ssize_t phy_index_show(struct device *_dev,struct device_attribute *attr,
			     char *buf);
static ssize_t phy_index_store(struct device *_dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count);
static ssize_t phy_power_show(struct device *_dev,
			     struct device_attribute *attr,
			     char *buf);
static ssize_t phy_power_store(struct device *_dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count);
static ssize_t phy_txvref_show(struct device *_dev,
			     struct device_attribute *attr,
			     char *buf);
static ssize_t phy_txvref_store(struct device *_dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count);
static ssize_t phy_otgdisable_show(struct device *_dev,
			     struct device_attribute *attr,
			     char *buf);
static ssize_t phy_otgdisable_store(struct device *_dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count);
DEVICE_ATTR(index, S_IRUGO | S_IWUSR, phy_index_show, phy_index_store);
DEVICE_ATTR(por, S_IRUGO | S_IWUSR, phy_power_show, phy_power_store);
DEVICE_ATTR(txvref, S_IRUGO | S_IWUSR, phy_txvref_show, phy_txvref_store);
DEVICE_ATTR(otgdisable, S_IRUGO | S_IWUSR, phy_otgdisable_show, phy_otgdisable_store);
/**
 * Show the value of phy index
 * 
 */
static ssize_t phy_index_show(struct device *_dev,
			     struct device_attribute *attr,
			     char *buf)
{
	amlogic_usb_struct_t * paus = dev_get_drvdata(_dev);
	uint32_t addr;


	addr = decide_register_addr(paus,REG_MODE_COM);
	if (addr != 0) {
		return snprintf(buf,
				sizeof("A\n") + 1,
				"%c\n", (paus->phy_index?'B':'A'));
	} else {
		dev_err(_dev, "Invalid addr (0x%0x,0x%x)\n", paus->phy_index,paus->phy_reg);
		return sprintf(buf, "invalid address\n");
	}
}

/**
 * Store the value to phy index 
 * 
 * 
 */
static ssize_t phy_index_store(struct device *_dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	amlogic_usb_struct_t * paus = dev_get_drvdata(_dev);

	char c = toupper(buf[0]);

	if(c == 'A' || c == 'B' ){
		paus->phy_index = (c - 'A');
	}else if(c == '0' || c == '1'){
		paus->phy_index = (c - '0');
	}else{
		dev_err(_dev,"Invalid phy index %c\n",c);
		return 0;
	}

	return count;
	
}

/**
 * Show the value of PHY power state
 * 
 */
static ssize_t phy_power_show(struct device *_dev,
			     struct device_attribute *attr,
			     char *buf)
{
	amlogic_usb_struct_t * paus = dev_get_drvdata(_dev);
	uint32_t val,mask;
	volatile uint32_t *addr;


	addr = (volatile uint32_t *)decide_register_addr(paus,REG_MODE_COM);
	if (addr != NULL) {
		
		mask = paus->phy_index?PREI_USB_PHY_B_POR:PREI_USB_PHY_A_POR;
		val = read_reg32(addr);
		return snprintf(buf,
				sizeof("off \n") + 1,
				"%s\n", ((val & mask)?"off":"on"));
	} else {
		dev_err(_dev, "Invalid addr (0x%0x,0x%x)\n", paus->phy_index,paus->phy_reg);
		return sprintf(buf, "invalid address\n");
	}
}

/**
 * Store the value in the register of PHY power
 * 
 * 
 */
static ssize_t phy_power_store(struct device *_dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	amlogic_usb_struct_t * paus = dev_get_drvdata(_dev);
	volatile uint32_t *addr;
	uint32_t val;
	char *p;
	int len,is_on,mask;

	p = memchr(buf, '\n', count);
	len = p ? p - buf : count;

	/* First, check if we are requested to on/off */
	if (len == 3 && !strncmp(buf, "off", len)) {
		is_on = 0;
	}else if(len == 2 && !strncmp(buf, "on", len)){
		is_on = 1;
	}else{
		dev_err(_dev,"Invalid power state %s\n",buf);
		return 0;
	}

	/* Second, set power on/off */
	addr = (volatile uint32_t *)decide_register_addr(paus,REG_MODE_COM);
	if (addr != NULL) {
		mask = paus->phy_index?PREI_USB_PHY_B_POR:PREI_USB_PHY_A_POR;
		val = read_reg32(addr);
		//printk("get reg: 0x%x, val: 0x%x\n",(int)addr,val);
		if(is_on)
			val = val & ~mask;
		else
			val = val | mask;
		//printk("set reg: 0x%x, val: 0x%x\n",(int)addr,val);
		write_reg32(addr,val);
	}

	return count;
	
}

/**
 * Show the value of phy tune tx_vref
 * 
 */
static ssize_t phy_txvref_show(struct device *_dev,
			     struct device_attribute *attr,
			     char *buf)
{
	amlogic_usb_struct_t * paus = dev_get_drvdata(_dev);
	int ret;

	paus->tune = USB_PHY_TUNE_TX_VREF;
	ret = get_phy_tune(paus);
	if (ret == 0) {
		return snprintf(buf,
				sizeof("15\n") + 1,
				"%d\n", paus->tune_value);
	} else {
		dev_err(_dev, "Invalid addr (0x%0x,0x%x)\n", paus->phy_index,paus->phy_reg);
		return sprintf(buf, "invalid address\n");
	}
}

/**
 * Store the value to phy index 
 * 
 * 
 */
static ssize_t phy_txvref_store(struct device *_dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	amlogic_usb_struct_t * paus = dev_get_drvdata(_dev);
	int ret;
	uint32_t value = simple_strtoul(buf,NULL,10);

	if(value <= 15 ){
		paus->tune = USB_PHY_TUNE_TX_VREF;
		paus->tune_value = value;
		ret = set_phy_tune(paus);
	}else{
		dev_err(_dev,"Invalid txvref value %d\n",value);
		return 0;
	}

	if(ret !=0)
		return 0;

	return count;
	
}

/**
 * Show the value of otgdisable
 * 
 */
static ssize_t phy_otgdisable_show(struct device *_dev,
			     struct device_attribute *attr,
			     char *buf)
{
	amlogic_usb_struct_t * paus = dev_get_drvdata(_dev);
	int ret;

	paus->tune = USB_PHY_TUNE_OTGDISABLE;
	ret = get_phy_tune(paus);
	if (ret == 0) {
		return sprintf(buf, "%s\n", paus->tune_value ? "off":"on");
	} else {
		dev_err(_dev, "Invalid addr (0x%0x,0x%x)\n", paus->phy_index,paus->phy_reg);
		return sprintf(buf, "invalid address\n");
	}
}

/**
 * Store the value in the register 
 * on: enable otg
 * off:disable otg
 */
static ssize_t phy_otgdisable_store(struct device *_dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	amlogic_usb_struct_t * paus = dev_get_drvdata(_dev);
	int ret;
	uint32_t val = 0;

	if(strncmp(buf, "on", sizeof("on")-1) == 0)
		val = 0;
	else if(strncmp(buf, "off", sizeof("off")-1) == 0)
		val = 1;
	else{
		dev_err(_dev,"Invalid otgdisable value %s\n",buf);
		return 0;
	}
	//printk("%s-%s[%d]\n",__func__,val?"off":"on",val);

	paus->tune = USB_PHY_TUNE_OTGDISABLE;
	paus->tune_value = val;
	ret = set_phy_tune(paus);

	if(ret !=0 )
		return 0;

	return count;
	
}


/******************************************************/
static void create_device_attribs(struct device *dev)
{
	device_create_file(dev, &dev_attr_index);
	device_create_file(dev, &dev_attr_por);
	device_create_file(dev, &dev_attr_txvref);
	device_create_file(dev, &dev_attr_otgdisable);
}
static void remove_device_attribs(struct device *dev)
{
	device_remove_file(dev, &dev_attr_index);
	device_remove_file(dev, &dev_attr_por);
	device_remove_file(dev, &dev_attr_txvref);
	device_remove_file(dev, &dev_attr_otgdisable);
}

/******************************************************/
static int amlogic_usb_probe(struct platform_device *pdev) 
{
	int ret = 0;

	memset(&aus,0,sizeof(struct amlogic_usb_struct));

	aus.phy_reg = pdev->resource->start;
	dev_set_drvdata(&pdev->dev,&aus);
	create_device_attribs(&pdev->dev);
	
	return ret;
}



static int amlogic_usb_remove(struct platform_device *pdev) 
{
	remove_device_attribs(&pdev->dev);
	return 0;
}

static struct platform_driver amlogic_usb_driver = { 
	.probe = amlogic_usb_probe, 
	.remove = amlogic_usb_remove, 
//	.shutdown
//	.resume
//	.suspend
	.driver = {
			.name = "usb_phy_control",						
			.owner = THIS_MODULE,						
	}, 
};

static int __init amlogic_usb_init(void) 
{
	return platform_driver_register(&amlogic_usb_driver);
}

static void __exit amlogic_usb_exit(void) 
{
	platform_driver_unregister(&amlogic_usb_driver);
} 

arch_initcall(amlogic_usb_init);
module_exit(amlogic_usb_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("Amlogic USB controller power/clock driver");
MODULE_LICENSE("GPL");

