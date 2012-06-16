#include <linux/init.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <mach/am_regs.h>

#include <asm/fiq.h>
#include <asm/uaccess.h>
#include "aml_demod.h"
#include "demod_func.h"

#define DRIVER_NAME "aml_demod"
#define MODULE_NAME "aml_demod"
#define DEVICE_NAME "aml_demod"

const char aml_demod_dev_id[] = "aml_demod";

static struct aml_demod_i2c demod_i2c;
static struct aml_demod_sta demod_sta;

static DECLARE_WAIT_QUEUE_HEAD(lock_wq);

static ssize_t aml_demod_info(struct class *cla, 
			      struct class_attribute *attr, 
			      char *buf)
{
    return 0;
}

static struct class_attribute aml_demod_class_attrs[] = {
    __ATTR(info,       
	   S_IRUGO | S_IWUSR, 
	   aml_demod_info,    
	   NULL),
    __ATTR_NULL
};

static struct class aml_demod_class = {
    .name = "aml_demod",
    .class_attrs = aml_demod_class_attrs,
};

static irqreturn_t aml_demod_isr(int irq, void *dev_id)
{
    if (demod_sta.dvb_mode == 0) {
	//dvbc_isr(&demod_sta);
	if(dvbc_isr_islock()){
		printk("sync4\n");
		if(waitqueue_active(&lock_wq))
			wake_up_interruptible(&lock_wq);
	}
    }
    else {
	dvbt_isr(&demod_sta);
    }

    return IRQ_HANDLED;
}

static int aml_demod_open(struct inode *inode, struct file *file)
{
    printk("Amlogic Demod DVB-T/C Open\n");    
    return 0;
}

static int aml_demod_release(struct inode *inode, struct file *file)

{
    printk("Amlogic Demod DVB-T/C Release\n");    
    return 0;
}

static int amdemod_islock(void)
{
	struct aml_demod_sts demod_sts;
	if(demod_sta.dvb_mode == 0) {
		dvbc_status(&demod_sta, &demod_i2c, &demod_sts);
		return demod_sts.ch_sts&0x1;
	} else if(demod_sta.dvb_mode == 1) {
		dvbt_status(&demod_sta, &demod_i2c, &demod_sts);
		return demod_sts.ch_sts>>12&0x1;
	}
	return 0;
}

static int aml_demod_ioctl(struct inode *inode, struct file *file,
                        unsigned int cmd, ulong arg)
{
    // printk("AML_DEMOD_SET_SYS     0x%x\n", AML_DEMOD_SET_SYS    );
    // printk("AML_DEMOD_GET_SYS     0x%x\n", AML_DEMOD_GET_SYS    );
    // printk("AML_DEMOD_TEST        0x%x\n", AML_DEMOD_TEST       );
    // printk("AML_DEMOD_DVBC_SET_CH 0x%x\n", AML_DEMOD_DVBC_SET_CH);
    // printk("AML_DEMOD_DVBC_GET_CH 0x%x\n", AML_DEMOD_DVBC_GET_CH);
    // printk("AML_DEMOD_DVBC_TEST   0x%x\n", AML_DEMOD_DVBC_TEST  );
    // printk("AML_DEMOD_DVBT_SET_CH 0x%x\n", AML_DEMOD_DVBT_SET_CH);
    // printk("AML_DEMOD_DVBT_GET_CH 0x%x\n", AML_DEMOD_DVBT_GET_CH);
    // printk("AML_DEMOD_DVBT_TEST   0x%x\n", AML_DEMOD_DVBT_TEST  );
    // printk("AML_DEMOD_SET_REG     0x%x\n", AML_DEMOD_SET_REG    );
    // printk("AML_DEMOD_GET_REG     0x%x\n", AML_DEMOD_GET_REG    );

    switch (cmd) {
    case AML_DEMOD_SET_SYS : 
	// printk("Ioctl Demod Set System\n");    	
	demod_set_sys(&demod_sta, &demod_i2c, (struct aml_demod_sys *)arg);
	break;
	
    case AML_DEMOD_GET_SYS :
	// printk("Ioctl Demod Get System\n");    	
	demod_get_sys(&demod_i2c, (struct aml_demod_sys *)arg);
	break;

    case AML_DEMOD_TEST :
	// printk("Ioctl Demod Test\n");    	
	demod_msr_clk(13);
	demod_msr_clk(14);
	demod_calc_clk(&demod_sta);
	break;

    case AML_DEMOD_TURN_ON :
	demod_turn_on(&demod_sta, (struct aml_demod_sys *)arg);
	break;

    case AML_DEMOD_TURN_OFF :
	demod_turn_off(&demod_sta, (struct aml_demod_sys *)arg);
	break;
	
    case AML_DEMOD_DVBC_SET_CH :
	// printk("Ioctl DVB-C Set Channel\n");    	
	dvbc_set_ch(&demod_sta, &demod_i2c, (struct aml_demod_dvbc *)arg);
	{
		int ret;
		ret = wait_event_interruptible_timeout(lock_wq, amdemod_islock(), 2*HZ);
		if(!ret)	printk(">>> wait lock timeout.\n");
	}
	break;

    case AML_DEMOD_DVBC_GET_CH :
	// printk("Ioctl DVB-C Get Channel\n");
	dvbc_status(&demod_sta, &demod_i2c, (struct aml_demod_sts *)arg);
	break;

    case AML_DEMOD_DVBC_TEST :
	// printk("Ioctl DVB-C Test\n");    	
	dvbc_get_test_out(0xb, 1000, (u32 *)arg);
	break;

    case AML_DEMOD_DVBT_SET_CH :
	// printk("Ioctl DVB-T Set Channel\n");    	
	dvbt_set_ch(&demod_sta, &demod_i2c, (struct aml_demod_dvbt *)arg);
	break;

    case AML_DEMOD_DVBT_GET_CH :
	// printk("Ioctl DVB-T Get Channel\n");
	dvbt_status(&demod_sta, &demod_i2c, (struct aml_demod_sts *)arg);
	break;

    case AML_DEMOD_DVBT_TEST :
	// printk("Ioctl DVB-T Test\n"); 
	dvbt_get_test_out(0x1e, 1000, (u32 *)arg);
	break;

    case AML_DEMOD_SET_REG :
	// printk("Ioctl Set Register\n"); 
   	demod_set_reg((struct aml_demod_reg *)arg);
	break;

    case AML_DEMOD_GET_REG :
	// printk("Ioctl Get Register\n");    	
   	demod_get_reg((struct aml_demod_reg *)arg);
	break;
            
    default:
	return -EINVAL;
    }

    return 0;
}

const static struct file_operations aml_demod_fops = {
    .owner    = THIS_MODULE,
    .open     = aml_demod_open,
    .release  = aml_demod_release,
    .ioctl    = aml_demod_ioctl,
};

static struct device *aml_demod_dev;

static int __init aml_demod_init(void)
{
    int r = 0;

    printk("Amlogic Demod DVB-T/C Init\n");    

    init_waitqueue_head(&lock_wq);

    /* hook demod isr */
    r = request_irq(INT_DEMOD, &aml_demod_isr,
                    IRQF_SHARED, "aml_demod",
                    (void *)aml_demod_dev_id);

    if (r) {
        printk("aml_demod irq register error.\n");
        r = -ENOENT;
        goto err0;
    }

    /* sysfs node creation */
    r = class_register(&aml_demod_class);
    if (r) {
        printk("create aml_demod class fail\r\n");
        goto err1;
    }

    /* create video device */
    r = register_chrdev(AML_DEMOD_MAJOR, "aml_demod", &aml_demod_fops);
    if (r < 0) {
        printk("Can't register major for aml_demod device\n");
        goto err2;
    }

    aml_demod_dev = device_create(&aml_demod_class, NULL,
				  MKDEV(AML_DEMOD_MAJOR, 0), NULL,
				  DEVICE_NAME);

    if (IS_ERR(aml_demod_dev)) {
        printk("Can't create aml_demod device\n");
        goto err3;
    }

    return (0);

  err3:
    unregister_chrdev(AML_DEMOD_MAJOR, DEVICE_NAME);

  err2:
    free_irq(INT_DEMOD, (void *)aml_demod_dev_id);

  err1:
    class_unregister(&aml_demod_class);
    
  err0:
    return r;
}

static void __exit aml_demod_exit(void)
{
    printk("Amlogic Demod DVB-T/C Exit\n");    

    device_destroy(&aml_demod_class, MKDEV(AML_DEMOD_MAJOR, 0));

    unregister_chrdev(AML_DEMOD_MAJOR, DEVICE_NAME);

    free_irq(INT_DEMOD, (void *)aml_demod_dev_id);

    class_unregister(&aml_demod_class);
}

module_init(aml_demod_init);
module_exit(aml_demod_exit);

MODULE_LICENSE("GPL");
//MODULE_AUTHOR(DRV_AUTHOR);
//MODULE_DESCRIPTION(DRV_DESC);
