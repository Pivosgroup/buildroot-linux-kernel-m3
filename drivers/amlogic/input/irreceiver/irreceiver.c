/*
 * linux/drivers/input/irremote/virtual_remote.c
 *
 * Virtual Keypad Driver
 *
 * Copyright (C) 2010 Amlogic Corporation
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * author :   geng.li
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/am_regs.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>
#include <linux/irq.h>
#include <linux/input/irreceiver.h>

//#define DEBUG
#ifdef DEBUG
#define dbg(fmt, args...) printk(KERN_INFO "ir_receiver: " fmt, ##args)
#else
#define dbg(fmt, args...)
#endif

#define DEVICE_NAME "irreceiver"
#define DEIVE_COUNT 32
#define BUFFER_SIZE 4096

static dev_t irreceiver_id;
static struct class *irreceiver_class;
static struct device *irreceiver_dev;
static struct cdev irreceiver_device;
static struct irreceiver_platform_data *irreceiver_data = NULL;
static int pwmSwitch = 0;
static struct ir_window rec_win, reced_win, send_win;
static int rec_idx, send_idx;
static int send_flag = 0; 
static unsigned int last_jiffies;
static unsigned long int time_count = 0; //10us time base
static unsigned long int shot_time = 0; 
struct timer_list check_end_timer;
static unsigned int invalid_win = 0;
static bool end_key = false;

static char logbuf[BUFFER_SIZE];
static int debug_timer = 0;
static int debug_timer_pwm = 0;

static ssize_t switch_write(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
	int val = simple_strtoul(buf, NULL, 0);
    dbg("switch write [%d]\n", val);
    
    if(val == 0)//off
    {
        debug_timer = 0;
        //close_pwm(irreceiver_data->pwm_no);
    }
    else //on
    {
        debug_timer = 1;
        //open_pwm(irreceiver_data->pwm_no);
    }
	return count;
}

inline void open_pwm(int idx)
{
    if(PWM_A == idx)
    {
        SET_CBUS_REG_MASK(PWM_MISC_REG_AB, ((1 << 15)|(0 << 8)|(1<<0)));
    }
    else if(PWM_B == idx)
    {
        SET_CBUS_REG_MASK(PWM_MISC_REG_AB, ((1 << 23)|(0 << 16)|(1<<1)));
    }
    else if(PWM_C == idx)
    {
        SET_CBUS_REG_MASK(PWM_MISC_REG_CD, ((1 << 15)|(0 << 8)|(1<<0)));
    }
    else if(PWM_D == idx)
    {
        SET_CBUS_REG_MASK(PWM_MISC_REG_CD, ((1 << 23)|(0 << 16)|(1<<1)));
    }
}

inline void close_pwm(int idx)
{
    if(PWM_A == idx)
    {
        CLEAR_CBUS_REG_MASK(PWM_MISC_REG_AB, ((1 << 15)|(0 << 8)|(1<<0)));
    }
    else if(PWM_B == idx)
    {
        CLEAR_CBUS_REG_MASK(PWM_MISC_REG_AB, ((1 << 23)|(0 << 16)|(1<<1)));
    }
    else if(PWM_C == idx)
    {
        CLEAR_CBUS_REG_MASK(PWM_MISC_REG_CD, ((1 << 15)|(0 << 8)|(1<<0)));
    }
    else if(PWM_D == idx)
    {
        CLEAR_CBUS_REG_MASK(PWM_MISC_REG_CD, ((1 << 23)|(0 << 16)|(1<<1)));
    }
}

static void init_pwm(void)
{
    int pwm_level = 24000000/irreceiver_data->freq/2;
    unsigned int pwmReg = 0;
    int idx = irreceiver_data->pwm_no;
    if(PWM_A == idx)
    {
        pwmReg = PWM_PWM_A;
    }
    else if(PWM_B == idx)
    {
        pwmReg = PWM_PWM_B;
    }
    else if(PWM_C == idx)
    {
        pwmReg = PWM_PWM_C;
    }
    else if(PWM_D == idx)
    {
        pwmReg = PWM_PWM_D;
    }
    
    close_pwm(irreceiver_data->pwm_no);
    irreceiver_data->init_pwm_pinmux();
    
	WRITE_CBUS_REG_BITS(pwmReg, pwm_level, 0, 16);  //low
    WRITE_CBUS_REG_BITS(pwmReg, pwm_level, 16, 16);  //hi
}

static void set_timer_b_event(unsigned long t)
{
    shot_time = t;
    time_count = 0;
}

static void oneshot_event(void)
{
    shot_time = 0;
    if(pwmSwitch == 0)
    {
        pwmSwitch = 1;
        open_pwm(irreceiver_data->pwm_no);//on
    }
    else
    {
        pwmSwitch = 0;
        close_pwm(irreceiver_data->pwm_no);//off
    }
    
    //set next window timer
    send_idx++;
    if(send_idx < send_win.winNum)
    {
        set_timer_b_event(send_win.winArray[send_idx]);
    }
    else //pwm should be off
    {
        close_pwm(irreceiver_data->pwm_no);;
        pwmSwitch = 0;
        send_flag = 0;
    }
}

static void timer_b_interrupt(void)
{
    //char tmp[128];    

    time_count++;
    if(debug_timer == 1 && time_count % 10 == 0)
    {
        if(debug_timer_pwm == 0)
        {
            debug_timer_pwm = 1;
            open_pwm(irreceiver_data->pwm_no);;//on
        }
        else
        {
            debug_timer_pwm = 0;
            close_pwm(irreceiver_data->pwm_no);//off
        }
    }
    
    if(shot_time !=0 && (time_count*10) >= shot_time)
    {    
        //sprintf(tmp, "[%lu][%lu]", shot_time, time_count*10);
        //strcat(logbuf, tmp);
        oneshot_event();
    }
}

static void init_timer_b(void)
{
    printk(KERN_INFO "init_timer_b\n");

    CLEAR_CBUS_REG_MASK(ISA_TIMER_MUX, TIMER_B_INPUT_MASK);
    SET_CBUS_REG_MASK(ISA_TIMER_MUX, TIMER_UNIT_10us << TIMER_B_INPUT_BIT);
    WRITE_CBUS_REG(ISA_TIMERB, 0);
    
    /* Set up the fiq handler */
    request_fiq(INT_TIMER_B, &timer_b_interrupt);
}

static ssize_t show_key_value(struct device * dev, struct device_attribute *attr, char * buf)
{
    int i = 0;
    char tmp[10];
    memset(buf, 0, PAGE_SIZE);
    sprintf(tmp, "rec num=%d\n", rec_win.winNum);
    strcat(buf, tmp);
    for(i=0; i<rec_win.winNum; i++)
    {
        sprintf(tmp, "[%d]", rec_win.winArray[i]);
        strcat(buf, tmp);
    }
    strcat(buf, "\n");
    
    sprintf(tmp, "send num=%d\n", send_win.winNum);
    strcat(buf, tmp);
    for(i=0; i<send_win.winNum; i++)
    {
        sprintf(tmp, "[%d]", send_win.winArray[i]);
        strcat(buf, tmp);
    }
    strcat(buf, "\n");    
    return strlen(buf);
}

static ssize_t show_log(struct device * dev, struct device_attribute *attr, char * buf)
{
    memset(buf, 0, PAGE_SIZE);
    strcpy(buf, logbuf);
    return strlen(buf);
}

static DEVICE_ATTR(switch, S_IWUSR | S_IRUGO, NULL, switch_write);
static DEVICE_ATTR(keyvalue, S_IWUSR | S_IRUGO, show_key_value, NULL);
static DEVICE_ATTR(log, S_IWUSR | S_IRUGO, show_log, NULL);

static int aml_ir_receiver_open(struct inode *inode, struct file *file)
{
	dbg("aml_ir_receiver_open()\n");
	return 0;
}

static int aml_ir_receiver_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long args)
{
    int i;
    s32 r = 0;
    unsigned long flags;
    void __user *argp = (void __user *)args;
	dbg("aml_ir_receiver_ioctl()\n");
    
    switch(cmd)
    {
    case IRRECEIVER_IOC_SEND:
        if (copy_from_user(&send_win, argp, sizeof(struct ir_window)))
		    return -EFAULT;

        #ifdef DEBUG
        for(i=0; i<send_win.winNum; i++)
            dbg("idx[%d]:[%d]\n", i, send_win.winArray[i]);
        #endif

        logbuf[0] = 0;
        send_idx = 0;
        pwmSwitch = 1;
        send_flag = 1;
        open_pwm(irreceiver_data->pwm_no);;
        if(send_idx < send_win.winNum)
        {
            local_irq_save(flags);
            set_timer_b_event(send_win.winArray[send_idx]);
            local_irq_restore(flags);
        }
        break;

    case IRRECEIVER_IOC_RECV:
		dbg("recv win [%d]\n", reced_win.winNum);
        if(copy_to_user(argp, &reced_win, sizeof(struct ir_window)))
            return -EFAULT;
        break;

	case IRRECEIVER_IOC_STUDY_S:
		dbg("IRRECEIVER_IOC_STUDY_S\n");
		rec_win.winNum = 0;
		reced_win.winNum = 0;
		invalid_win = 0;
		end_key = false;
		break;

	case IRRECEIVER_IOC_STUDY_E:
		dbg("IRRECEIVER_IOC_STUDY_E\n");
		break;
        
    default:
        r = -ENOIOCTLCMD;
        break;
    }
    
	return r;
}
static int aml_ir_receiver_release(struct inode *inode, struct file *file)
{
	dbg("aml_ir_receiver_release()\n");
	file->private_data = NULL;
	return 0;
	
}
static const struct file_operations aml_ir_receiver_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_ir_receiver_open,  
	.ioctl		= aml_ir_receiver_ioctl,
	.release	= aml_ir_receiver_release, 	
};

static void check_end_timer_sr(unsigned long data)
{
	if(invalid_win > 15)
	{
		reced_win.winNum = 1;
		reced_win.winArray[0] = 1;
	}
	else
	{
		memcpy(&reced_win, &rec_win, sizeof(rec_win));
	}
}

static void ir_fiq_interrupt(void)
{
    int pulse_width;
    char tmp[64];
    unsigned int current_jiffies = jiffies;
    sprintf(tmp, "ir_fiq_interrupt[%u]\n", current_jiffies);
    //strcat(logbuf, tmp);

    if(send_flag == 1)
	{
		//strcat(logbuf,"it is a send_flag\n");
        return;//don't receive key when sending a key
	}

	if(end_key == true)
	{
	    //strcat(logbuf,"it is a end_key\n");
		return;//don't receive repeat key
	}

    if(current_jiffies - last_jiffies > 10)//means a new key
    {   
        //dbg("it is a new ir key\n");
	    //strcat(logbuf,"it is a new ir key\n");
        rec_idx = 0;
		invalid_win = 0;
        rec_win.winNum = 0;
		reced_win.winNum = 0;
		end_key = false;
        last_jiffies = current_jiffies;
        return;//ignore first falling or rising edge
    }
    
    last_jiffies = current_jiffies;
    
    pulse_width = ( (READ_AOBUS_REG(AO_IR_DEC_REG1)) & 0x1FFF0000 ) >> 16 ;
	sprintf(tmp, "[%d]",pulse_width);
	//strcat(logbuf,tmp);

    if(pulse_width < 10)//200us
    {
    	invalid_win++;
        return;		//ignore if it is a invalid pulse
    }
	else if(pulse_width > 1000) //20ms
	{
		end_key = true;
		return;		//end of key
	}

    rec_idx++;
    if(rec_idx >= MAX_PLUSE)
        return;
    
    rec_win.winNum = rec_idx;
    rec_win.winArray[rec_idx-1] = (pulse_width*20);
	//sprintf(tmp, "[%d][%u]",rec_idx-1, rec_win.winArray[rec_idx-1]);
	//strcat(logbuf,tmp);

	mod_timer(&check_end_timer,jiffies+msecs_to_jiffies(60));
}

static void ir_hardware_init(void)
{
    rec_idx = 0;
	invalid_win = 0;
    last_jiffies = 0xffffffff;

    irreceiver_data->init_ir_pinmux();
    
	setup_timer(&check_end_timer, check_end_timer_sr, 0);
	
    request_fiq(INT_REMOTE, &ir_fiq_interrupt);
}

static int __init aml_ir_receiver_probe(struct platform_device *pdev)
{
	int r;
	dbg("ir receiver probe\n");

    if(pdev != NULL)
        irreceiver_data = pdev->dev.platform_data;

    if(irreceiver_data == NULL){
        printk(KERN_ERR "can't get platform data\n");
        return -1;
    }
    
	r = alloc_chrdev_region(&irreceiver_id, 0, DEIVE_COUNT, DEVICE_NAME);
	if (r < 0) {
		printk(KERN_ERR "Can't register major for ir receiver device\n");
		return r;
	}
	irreceiver_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(irreceiver_class)) {
		unregister_chrdev_region(irreceiver_id, DEIVE_COUNT);
		printk(KERN_ERR "Can't create class for ir receiver device\n");
		return -1;
	}
	cdev_init(&irreceiver_device, &aml_ir_receiver_fops);
	irreceiver_device.owner = THIS_MODULE;
	cdev_add(&(irreceiver_device), irreceiver_id, DEIVE_COUNT);

	irreceiver_dev = device_create(irreceiver_class, NULL, irreceiver_id, NULL, "irreceiver%d", 0); //kernel>=2.6.27 

	if (irreceiver_dev == NULL) {
		dbg("irreceiver_dev create error\n");
		class_destroy(irreceiver_class);
		return -EEXIST;
	}

	device_create_file(irreceiver_dev, &dev_attr_switch);
    device_create_file(irreceiver_dev, &dev_attr_keyvalue);
    device_create_file(irreceiver_dev, &dev_attr_log);
    
    ir_hardware_init();
    init_pwm();
    init_timer_b();
    
	return 0;
}

static int aml_ir_receiver_remove(struct platform_device *pdev)
{
	dbg("remove IR Receiver\n");
	
	/* unregister everything */
    free_fiq(INT_REMOTE, &ir_fiq_interrupt);
    free_fiq(INT_TIMER_B, &timer_b_interrupt);

	del_timer(&check_end_timer);
    
    /* Remove the cdev */
	device_remove_file(irreceiver_dev, &dev_attr_switch);
    device_remove_file(irreceiver_dev, &dev_attr_keyvalue);
    device_remove_file(irreceiver_dev, &dev_attr_log);

    cdev_del(&irreceiver_device);

    device_destroy(irreceiver_class, irreceiver_id);

    class_destroy(irreceiver_class);

    unregister_chrdev_region(irreceiver_id, DEIVE_COUNT);
	return 0;
}

static struct platform_driver aml_ir_receiver_driver = {
	.probe		= aml_ir_receiver_probe,
	.remove		= aml_ir_receiver_remove,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= DEVICE_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __devinit aml_ir_receiver_init(void)
{
    if (platform_driver_register(&aml_ir_receiver_driver)) {
        dbg("failed to register aml_ir_receiver_driver module\n");
        return -ENODEV;
    }
    
	return 0;
}

static void __exit aml_ir_receiver_exit(void)
{
	dbg("IR Receiver exit \n");
    platform_driver_unregister(&aml_ir_receiver_driver);
}

module_init(aml_ir_receiver_init);
module_exit(aml_ir_receiver_exit);

MODULE_AUTHOR("geng.li");
MODULE_DESCRIPTION("IR Receiver Driver");
MODULE_LICENSE("GPL");

