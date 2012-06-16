/*
 * linux/drivers/input/irremote/remote_kbd.c
 *
 * Keypad Driver
 *
 * Copyright (C) 2009 Amlogic Corporation
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
 * author :   jianfeng_wang
 */
 /*
 * !!caution: if you use remote ,you should disable card1 used for  ata_enable pin.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
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
#include <mach/pinmux.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "amkbd_remote.h"



#undef NEW_BOARD_LEARNING_MODE

#define IR_CONTROL_HOLD_LAST_KEY    (1<<6)
#define IR_CONTROL_DECODER_MODE     (3<<7)
#define IR_CONTROL_SKIP_HEADER      (1<<7)
#define IR_CONTROL_RESET            (1<<0)

#define KEY_RELEASE_DELAY    200

type_printk input_dbg;

static DEFINE_MUTEX(kp_enable_mutex);
static DEFINE_MUTEX(kp_file_mutex);
static void kp_tasklet(unsigned long);
static int kp_enable;
static int NEC_REMOTE_IRQ_NO=INT_REMOTE;

DECLARE_TASKLET_DISABLED(tasklet, kp_tasklet, 0);

static struct kp   *gp_kp=NULL;
char *remote_log_buf;
typedef  struct {
	char		     *platform_name;
	unsigned int  pin_mux;
	unsigned int  bit;
}pin_config_t;
static  pin_config_t  pin_config[]={
		{
			.platform_name="8626",
			.pin_mux=1,
			.bit=(1<<31),
		},
		{
			.platform_name="6236",
			.pin_mux=5,
			.bit=(1<<31),
		},
		{
			.platform_name="8726",
			.pin_mux=5,
			.bit=(1<<31),
		},
		{
			.platform_name="8726M3",
			.pin_mux=0,
			.bit=(1<<0),
		},
} ;

static __u16 key_map[256];
static __u16 mouse_map[6]; /*Left Right Up Down + middlewheel up &down*/

int remote_printk(const char *fmt, ...)
{
    va_list args;
    int r;

    if (gp_kp->debug_enable==0)  return 0;
    va_start(args, fmt);
    r = vprintk(fmt, args);
    va_end(args);
    return r;
}

static int kp_mouse_event(struct input_dev *dev, unsigned int scancode, unsigned int type)
{
    __u16 mouse_code;
    __s32 mouse_value;
    static unsigned int repeat_count = 0;
    __s32 move_accelerate[] = {0, 1, 1, 2, 2, 3, 4, 5, 6, 7, 8, 9};
    unsigned int i;
		
    for(i = 0; i < ARRAY_SIZE(mouse_map); i++)
        if(mouse_map[i] == scancode)
            break;


    if(i>= ARRAY_SIZE(mouse_map)) return -1;
    switch(type){
        case 1 ://press
            repeat_count = 0;
            break;
        case 2 ://repeat
            if(repeat_count >= ARRAY_SIZE(move_accelerate) - 1)
                repeat_count = ARRAY_SIZE(move_accelerate) - 1;
            else
                repeat_count ++;
        }
    switch(i){
        case 0 :
            mouse_code = REL_X;
            mouse_value = -(1 + move_accelerate[repeat_count]);
            break;
        case 1 :
            mouse_code = REL_X;
            mouse_value = 1 + move_accelerate[repeat_count];
            break;
        case 2 :
            mouse_code = REL_Y;
            mouse_value = -(1 + move_accelerate[repeat_count]);
            break;
        case 3 :
            mouse_code = REL_Y;
            mouse_value = 1 + move_accelerate[repeat_count];
            break;
	case 4://up
	     mouse_code= REL_WHEEL;
	     mouse_value=0x1;	 
	     break;
	case 5:
	     mouse_code= REL_WHEEL;
	     mouse_value=0xffffffff;	
	     break;
		
        }
    if(type){
        input_event(dev, EV_REL, mouse_code, mouse_value);
        input_sync(dev);
	 switch(mouse_code)
	 {
	 	case REL_X:
		case REL_Y:
		 input_dbg("mouse be %s moved %d.\n", mouse_code==REL_X?"horizontal":"vertical", mouse_value);	
		break;
		case REL_WHEEL:
		input_dbg("mouse wheel move %s .\n",mouse_value==0x1?"up":"down");
		break;
	 }
       
    }
    return 0;
}

void kp_send_key(struct input_dev *dev, unsigned int scancode, unsigned int type)
{
    if(kp_mouse_event(dev, scancode, type)){
        if(scancode > ARRAY_SIZE(key_map)){
            input_dbg("scancode is 0x%04x, out of key mapping.\n", scancode);
            return;
            }
        if((key_map[scancode] >= KEY_MAX)||(key_map[scancode]==KEY_RESERVED)){
            input_dbg("scancode is 0x%04x, invalid key is 0x%04x.\n", scancode, key_map[scancode]);
            return;
            }
        input_event(dev, EV_KEY, key_map[scancode], type);
        input_sync(dev);
        switch(type){
            case 0 :
                input_dbg("release ircode = 0x%02x, scancode = 0x%04x\n", scancode, key_map[scancode]);
                break;
            case 1 :
                input_dbg("press ircode = 0x%02x, scancode = 0x%04x\n", scancode, key_map[scancode]);
                break;
            case 2 :
                input_dbg("repeat ircode = 0x%02x, scancode = 0x%04x\n", scancode, key_map[scancode]);
                break;
            }
        }
}
static void  disable_remote_irq(void)
{
	 if(!(gp_kp->work_mode&REMOTE_WORK_MODE_FIQ))
	 {
	 	disable_irq(NEC_REMOTE_IRQ_NO);
	 }
    	
}
static void  enable_remote_irq(void)
{
	if(!(gp_kp->work_mode&&REMOTE_WORK_MODE_FIQ))
	 {
	 	enable_irq(NEC_REMOTE_IRQ_NO);
	 }
	
}
static void kp_repeat_sr(unsigned long data)
{
	struct kp *kp_data=(struct kp *)data;
	u32 	status;
	u32  timer_period;
	
	status = READ_AOBUS_REG(AO_IR_DEC_STATUS);
	switch(status&REMOTE_HW_DECODER_STATUS_MASK) 
	{
		case REMOTE_HW_DECODER_STATUS_OK:
		kp_send_key(kp_data->input, (kp_data->cur_keycode>>16)&0xff, 0);	
		break ;
		default:
		SET_AOBUS_REG_MASK(AO_IR_DEC_REG1,1);//reset ir deocoder
		CLEAR_AOBUS_REG_MASK(AO_IR_DEC_REG1,1);	
		if(kp_data->repeat_tick !=0)//new key coming in.
		{
			timer_period= jiffies + 10 ;  //timer peroid waiting for a stable state.
		}
		else //repeat key check
		{
			if(kp_data->repeat_enable)
			{
				kp_send_key(kp_data->input, (kp_data->cur_keycode>>16)&0xff, 2);
			}
			timer_period=jiffies+msecs_to_jiffies(kp_data->repeat_peroid);
		}
		mod_timer(&kp_data->repeat_timer,timer_period);
		kp_data->repeat_tick =0;
		break;
	}
	
}
static void kp_timer_sr(unsigned long data)
{
    struct kp *kp_data=(struct kp *)data;
    kp_send_key(kp_data->input, (kp_data->cur_keycode>>16)&0xff ,0);
    if(!(kp_data->work_mode&REMOTE_WORK_MODE_HW))
        kp_data->step   = REMOTE_STATUS_WAIT ;
}

static irqreturn_t kp_interrupt(int irq, void *dev_id)
{
    /* disable keyboard interrupt and schedule for handling */
//  input_dbg("===trigger one  kpads interupt \r\n");
 	tasklet_schedule(&tasklet);

	return IRQ_HANDLED;
}
static  void  kp_fiq_interrupt(void)
{
	kp_sw_reprot_key((unsigned long)gp_kp);
	WRITE_AOBUS_REG(IRQ_CLR_REG(NEC_REMOTE_IRQ_NO), 1 << IRQ_BIT(NEC_REMOTE_IRQ_NO));
}
static inline int kp_hw_reprot_key(struct kp *kp_data )
{
    int  key_index;
    unsigned  int  status,scan_code;
    static  int last_scan_code,key_hold;
    static  int last_custom_code;

    // 1        get  scan code
    scan_code=READ_AOBUS_REG(AO_IR_DEC_FRAME);
    status=READ_AOBUS_REG(AO_IR_DEC_STATUS);
    key_index=0 ;
    key_hold=-1 ;
    if(scan_code)  //key first press
    {
        last_custom_code=scan_code&0xffff;
        if(kp_data->custom_code != last_custom_code )
        {
               input_dbg("Wrong custom code is 0x%08x\n", scan_code);
            return -1;
        }
       	 //add for skyworth remote.
	 if(kp_data->work_mode == REMOTE_TOSHIBA_HW)//we start  repeat timer for check repeat.
	 {	
	 	if(kp_data->repeat_timer.expires > jiffies){//release last key.
            		kp_send_key(kp_data->input, (kp_data->cur_keycode>>16)&0xff, 0);
            	}
		kp_send_key(kp_data->input, (scan_code>>16)&0xff, 1);
	 	last_scan_code=scan_code;
    		kp_data->cur_keycode=last_scan_code;
    		kp_data->repeat_timer.data=(unsigned long)kp_data;
		//here repeat  delay is time interval from the first frame end to first repeat end.	
		kp_data->repeat_tick = jiffies;
	 	mod_timer(&kp_data->repeat_timer,jiffies+msecs_to_jiffies(kp_data->repeat_delay));
		return 0;
	 }
	 else
	 {
	 	if(kp_data->timer.expires > jiffies)
            	kp_send_key(kp_data->input, (kp_data->cur_keycode>>16)&0xff, 0);
            	kp_send_key(kp_data->input, (scan_code>>16)&0xff, 1);
        	if(kp_data->repeat_enable)
          	kp_data->repeat_tick = jiffies + msecs_to_jiffies(kp_data->input->rep[REP_DELAY]);
	 }
	
    }
    else if(scan_code==0 && status&0x1) //repeate key
    {
        scan_code=last_scan_code;
        if(kp_data->custom_code != last_custom_code ){
            return -1;
            }
        if(kp_data->repeat_enable){
            if(kp_data->repeat_tick < jiffies){
                kp_send_key(kp_data->input, (scan_code>>16)&0xff, 2);
                kp_data->repeat_tick += msecs_to_jiffies(kp_data->input->rep[REP_PERIOD]);
                }
            }
        else{
            if(kp_data->timer.expires > jiffies)
                mod_timer(&kp_data->timer,jiffies+msecs_to_jiffies(kp_data->release_delay));
           	 return -1;
            }
    }
    last_scan_code=scan_code;
    kp_data->cur_keycode=last_scan_code;
    kp_data->timer.data=(unsigned long)kp_data;
    mod_timer(&kp_data->timer,jiffies+msecs_to_jiffies(kp_data->release_delay));

    return 0 ;
}

static void kp_tasklet(unsigned long data)
{
    struct kp *kp_data = (struct kp *) data;

    if(kp_data->work_mode&REMOTE_WORK_MODE_HW)
    {
        kp_hw_reprot_key(kp_data);
    }else{
        kp_sw_reprot_key(data);
    }
}

static ssize_t kp_log_buffer_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    int ret =0;
    ret=sprintf(buf, "%s\n", remote_log_buf);
    //printk(remote_log_buf);
    remote_log_buf[0]='\0';
    return ret ;
}

static ssize_t kp_enable_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", kp_enable);
}

//control var by sysfs .
static ssize_t kp_enable_store(struct device *dev, struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int state;

    if (sscanf(buf, "%u", &state) != 1)
        return -EINVAL;

    if ((state != 1) && (state != 0))
        return -EINVAL;

    mutex_lock(&kp_enable_mutex);
    if (state != kp_enable) {
        if (state)
            enable_remote_irq();
        else
            disable_remote_irq();
        kp_enable = state;
    }
    mutex_unlock(&kp_enable_mutex);

    return strnlen(buf, count);
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, kp_enable_show, kp_enable_store);
static DEVICE_ATTR(log_buffer, S_IRUGO | S_IWUSR, kp_log_buffer_show, NULL);

/*****************************************************************
**
** func : hardware init
**       in this function will do pin configuration and and initialize for hardware
**       decoder mode .
**
********************************************************************/
static int    hardware_init(struct platform_device *pdev)
{
	struct resource *mem; 
	unsigned int  control_value,status,data_value;
	int i;
	pin_config_t  *config=NULL;
	//step 0: set mutx to remote
	if (!(mem = platform_get_resource(pdev, IORESOURCE_IO, 0))) {
		printk("not define ioresource for remote keyboard.\n");
		return -1;
	}
	for (i=0;i<ARRAY_SIZE(pin_config);i++)
	{
		if(strcmp(pin_config[i].platform_name,mem->name)==0)
		{
			config=&pin_config[i] ;
			input_dbg("got resource :%d\r\n",i);
			break;
		}
	}
	if(NULL==config){
        printk("can not get config for remote keybrd.\n");
        return -1;
    }
	if (!strcmp(config->platform_name, "8726M3"))
	{
		int val = READ_AOBUS_REG(AO_RTI_PIN_MUX_REG);
		WRITE_AOBUS_REG(AO_RTI_PIN_MUX_REG, val | config->bit);
	}
	else
	{
		set_mio_mux(config->pin_mux,config->bit); 	
	}
    //step 1 :set reg AO_IR_DEC_CONTROL
    	control_value = 3<<28|(0xFA0 << 12) |0x13;

    	WRITE_AOBUS_REG(AO_IR_DEC_REG0, control_value);
    	control_value = READ_AOBUS_REG(AO_IR_DEC_REG1);
    	WRITE_AOBUS_REG(AO_IR_DEC_REG1, control_value | IR_CONTROL_HOLD_LAST_KEY);

    	status = READ_AOBUS_REG(AO_IR_DEC_STATUS);
    	data_value = READ_AOBUS_REG(AO_IR_DEC_FRAME);

    //step 2 : request nec_remote irq  & enable it
    return request_irq(NEC_REMOTE_IRQ_NO, kp_interrupt,
        IRQF_SHARED,"keypad", (void *)kp_interrupt);
}

static int
work_mode_config(unsigned int cur_mode)
{
	unsigned int  control_value;
    	static unsigned int 	last_mode=REMOTE_WORK_MODE_INV;
	struct irq_desc *desc = irq_to_desc(NEC_REMOTE_IRQ_NO);	
   	if(last_mode == cur_mode) return -1;	
    	if(cur_mode&REMOTE_WORK_MODE_HW)
    	{
      	  	control_value=0xbe40; //ignore  custom code .
      	  	WRITE_AOBUS_REG(AO_IR_DEC_REG1,control_value|IR_CONTROL_HOLD_LAST_KEY);
    	}
	else
	{
		control_value=0x8578;
        	WRITE_AOBUS_REG(AO_IR_DEC_REG1,control_value);
    	}

    	switch(cur_mode&REMOTE_WORK_MODE_MASK)
    	{
    		case REMOTE_WORK_MODE_HW:
		case REMOTE_WORK_MODE_SW:
		if(last_mode==REMOTE_WORK_MODE_FIQ)
		{
			//disable fiq and enable common irq
			unregister_fiq_bridge_handle(&gp_kp->fiq_handle_item);
			free_fiq(NEC_REMOTE_IRQ_NO, &kp_fiq_interrupt);
			request_irq(NEC_REMOTE_IRQ_NO, kp_interrupt,
        			IRQF_SHARED,"keypad", (void *)kp_interrupt);
		}
		break;
		case REMOTE_WORK_MODE_FIQ:
		//disable common irq and enable fiq.
		free_irq(NEC_REMOTE_IRQ_NO,kp_interrupt);
		gp_kp->fiq_handle_item.handle=remote_bridge_isr;
		gp_kp->fiq_handle_item.key=(u32)gp_kp;
		gp_kp->fiq_handle_item.name="remote_bridge";
		register_fiq_bridge_handle(&gp_kp->fiq_handle_item);
		//workaround to fix fiq mechanism bug.
		desc->depth++;
		request_fiq(NEC_REMOTE_IRQ_NO, &kp_fiq_interrupt);
		
		break;	
    	} 	
    	last_mode=cur_mode;
	//add for skyworth remote 	
	if(cur_mode==REMOTE_TOSHIBA_HW)
		setup_timer(&gp_kp->repeat_timer,kp_repeat_sr,0);
	else	
		del_timer(&gp_kp->repeat_timer) ;
	return 0;
}

static int
remote_config_open(struct inode *inode, struct file *file)
{
    file->private_data = gp_kp;
    return 0;
}
static int
remote_config_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long args)
{
    struct kp   *kp=(struct kp*)filp->private_data;
    void  __user* argp =(void __user*)args;
    unsigned int   val, i;

    if(args)
    {
        copy_from_user(&val,argp,sizeof(unsigned long));
    }
    mutex_lock(&kp_file_mutex);
    //cmd input
    switch(cmd)
    {
        // 1 set part
        case REMOTE_IOC_RESET_KEY_MAPPING:
        for(i = 0; i < ARRAY_SIZE(key_map); i++)
            key_map[i] = KEY_RESERVED;
        for(i = 0; i < ARRAY_SIZE(mouse_map); i++)
            mouse_map[i] = 0xffff;
        break;
        case REMOTE_IOC_SET_KEY_MAPPING:
            if((val >> 16) >= ARRAY_SIZE(key_map)){
                mutex_unlock(&kp_file_mutex);
                return  -1;
                }
            key_map[val>>16] = val & 0xffff;
        break;
        case REMOTE_IOC_SET_MOUSE_MAPPING:
            if((val >> 16) >= ARRAY_SIZE(mouse_map)){
                mutex_unlock(&kp_file_mutex);
                return  -1;
                }
            mouse_map[val>>16] = val & 0xff;
        break;
        case REMOTE_IOC_SET_REPEAT_DELAY:
        copy_from_user(&kp->repeat_delay,argp,sizeof(long));
        break;
        case REMOTE_IOC_SET_REPEAT_PERIOD:
        copy_from_user(&kp->repeat_peroid,argp,sizeof(long));
        break;
        case  REMOTE_IOC_SET_REPEAT_ENABLE:
        copy_from_user(&kp->repeat_enable,argp,sizeof(long));
        break;
        case  REMOTE_IOC_SET_DEBUG_ENABLE:
        copy_from_user(&kp->debug_enable,argp,sizeof(long));
        break;
        case  REMOTE_IOC_SET_MODE:
        copy_from_user(&kp->work_mode,argp,sizeof(long));
        break;
        case  REMOTE_IOC_SET_BIT_COUNT:
        copy_from_user(&kp->bit_count,argp,sizeof(long));
        break;
        case  REMOTE_IOC_SET_CUSTOMCODE:
        copy_from_user(&kp->custom_code,argp,sizeof(long));
        break;
        case  REMOTE_IOC_SET_REG_BASE_GEN:
        WRITE_AOBUS_REG(AO_IR_DEC_REG0,val);
        break;
        case REMOTE_IOC_SET_REG_CONTROL:
        WRITE_AOBUS_REG(AO_IR_DEC_REG1,val);
        break;
        case REMOTE_IOC_SET_REG_LEADER_ACT:
        WRITE_AOBUS_REG(AO_IR_DEC_LDR_ACTIVE,val);
        break;
        case REMOTE_IOC_SET_REG_LEADER_IDLE:
        WRITE_AOBUS_REG(AO_IR_DEC_LDR_IDLE,val);
        break;
        case REMOTE_IOC_SET_REG_REPEAT_LEADER:
        WRITE_AOBUS_REG(AO_IR_DEC_LDR_REPEAT,val);
        break;
        case REMOTE_IOC_SET_REG_BIT0_TIME:
        WRITE_AOBUS_REG(AO_IR_DEC_BIT_0,val);
        break;
        case REMOTE_IOC_SET_RELEASE_DELAY:
        copy_from_user(&kp->release_delay,argp,sizeof(long));
        break;
        //SW
        case REMOTE_IOC_SET_TW_LEADER_ACT:
        kp->time_window[0]=val&0xffff;
        kp->time_window[1]=(val>>16)&0xffff;
        break;
        case REMOTE_IOC_SET_TW_BIT0_TIME:
        kp->time_window[2]=val&0xffff;
        kp->time_window[3]=(val>>16)&0xffff;
        break;
        case REMOTE_IOC_SET_TW_BIT1_TIME:
        kp->time_window[4]=val&0xffff;
        kp->time_window[5]=(val>>16)&0xffff;
        break;
        case REMOTE_IOC_SET_TW_REPEATE_LEADER:
        kp->time_window[6]=val&0xffff;
        kp->time_window[7]=(val>>16)&0xffff;
        break;
        // 2 get  part
        case REMOTE_IOC_GET_REG_BASE_GEN:
        val=READ_AOBUS_REG(AO_IR_DEC_REG0);
        break;
        case REMOTE_IOC_GET_REG_CONTROL:
        val=READ_AOBUS_REG(AO_IR_DEC_REG1);
        break;
        case REMOTE_IOC_GET_REG_LEADER_ACT:
        val=READ_AOBUS_REG(AO_IR_DEC_LDR_ACTIVE);
        break;
        case REMOTE_IOC_GET_REG_LEADER_IDLE:
        val=READ_AOBUS_REG(AO_IR_DEC_LDR_IDLE);
        break;
        case REMOTE_IOC_GET_REG_REPEAT_LEADER:
        val=READ_AOBUS_REG(AO_IR_DEC_LDR_REPEAT);
        break;
        case REMOTE_IOC_GET_REG_BIT0_TIME:
        val=READ_AOBUS_REG(AO_IR_DEC_BIT_0);
        break;
        case REMOTE_IOC_GET_REG_FRAME_DATA:
        val=READ_AOBUS_REG(AO_IR_DEC_FRAME);
        break;
        case REMOTE_IOC_GET_REG_FRAME_STATUS:
        val=READ_AOBUS_REG(AO_IR_DEC_STATUS);
        break;
        //sw
        case REMOTE_IOC_GET_TW_LEADER_ACT:
        val=kp->time_window[0]|(kp->time_window[1]<<16);
        break;
        case REMOTE_IOC_GET_TW_BIT0_TIME:
        val=kp->time_window[2]|(kp->time_window[3]<<16);
        break;
        case REMOTE_IOC_GET_TW_BIT1_TIME:
        val=kp->time_window[4]|(kp->time_window[5]<<16);
        break;
        case REMOTE_IOC_GET_TW_REPEATE_LEADER:
        val=kp->time_window[6]|(kp->time_window[7]<<16);
        break;
    }
    //output result
    switch(cmd)
    {
        case REMOTE_IOC_SET_REPEAT_ENABLE:
        if (kp->repeat_enable)
        {
            kp->input->rep[REP_DELAY] = kp->repeat_delay;
            kp->input->rep[REP_PERIOD] = kp->repeat_peroid;
        }else{
            kp->input->rep[REP_DELAY]=0xffffffff;
            kp->input->rep[REP_PERIOD]=0xffffffff;
        }
        break;
        case REMOTE_IOC_SET_MODE:
	 work_mode_config(kp->work_mode);
        break;
        case REMOTE_IOC_GET_REG_BASE_GEN:
        case REMOTE_IOC_GET_REG_CONTROL:
        case REMOTE_IOC_GET_REG_LEADER_ACT  :
        case REMOTE_IOC_GET_REG_LEADER_IDLE:
        case REMOTE_IOC_GET_REG_REPEAT_LEADER:
        case REMOTE_IOC_GET_REG_BIT0_TIME:
        case REMOTE_IOC_GET_REG_FRAME_DATA:
        case REMOTE_IOC_GET_REG_FRAME_STATUS:
        case REMOTE_IOC_GET_TW_LEADER_ACT   :
        case REMOTE_IOC_GET_TW_BIT0_TIME:
        case REMOTE_IOC_GET_TW_BIT1_TIME:
        case REMOTE_IOC_GET_TW_REPEATE_LEADER:
        copy_to_user(argp,&val,sizeof(long));
        break;
    }
    mutex_unlock(&kp_file_mutex);
    return  0;
}

static int
remote_config_release(struct inode *inode, struct file *file)
{
    file->private_data=NULL;
    return 0;
}

static const struct file_operations remote_fops = {
    .owner      = THIS_MODULE,
    .open       =remote_config_open,
    .ioctl      = remote_config_ioctl,
    .release        = remote_config_release,
};

static int  register_remote_dev(struct kp  *kp)
{
    int ret=0;
    strcpy(kp->config_name,"amremote");
    ret=register_chrdev(0,kp->config_name,&remote_fops);
    if(ret <=0)
    {
        printk("register char dev tv error\r\n");
        return  ret ;
    }
    kp->config_major=ret;
    printk("remote config major:%d\r\n",ret);
    kp->config_class=class_create(THIS_MODULE,kp->config_name);
    kp->config_dev=device_create(kp->config_class,NULL,MKDEV(kp->config_major,0),NULL,kp->config_name);
    return ret;
}

static int __init kp_probe(struct platform_device *pdev)
{
    struct kp *kp;
    struct input_dev *input_dev;

    int i,ret;

    kp_enable=1;
    kp = kzalloc(sizeof(struct kp), GFP_KERNEL);
    input_dev = input_allocate_device();
    if (!kp || !input_dev) {
        kfree(kp);
        input_free_device(input_dev);
        return -ENOMEM;
    }
    gp_kp=kp;
    kp->debug_enable=0;

    input_dbg=remote_printk;
    platform_set_drvdata(pdev, kp);
    kp->work_mode=REMOTE_NEC_HW;
    kp->input = input_dev;
    kp->release_delay=KEY_RELEASE_DELAY;
    kp->custom_code=0xff00;
    kp->bit_count=32;  //default 32bit for sw mode.
    kp->last_jiffies=0xffffffff;
    kp->last_pulse_width=0;


    kp->step = REMOTE_STATUS_WAIT;
    kp->time_window[0]=0x1;
    kp->time_window[1]=0x1;
    kp->time_window[2]=0x1;
    kp->time_window[3]=0x1;
    kp->time_window[4]=0x1;
    kp->time_window[5]=0x1;
    kp->time_window[6]=0x1;
    kp->time_window[7]=0x1;
    /* Disable the interrupt for the MPUIO keyboard */

    for(i = 0; i < ARRAY_SIZE(key_map); i++)
        key_map[i] = KEY_RESERVED;
    for(i = 0; i < ARRAY_SIZE(mouse_map); i++)
        mouse_map[i] = 0xffff;
    kp->repeat_delay = 250;
    kp->repeat_peroid = 33;

    /* get the irq and init timer*/
    input_dbg("set drvdata completed\r\n");
    tasklet_enable(&tasklet);
    tasklet.data = (unsigned long) kp;
    setup_timer(&kp->timer, kp_timer_sr, 0) ;
    	

    ret = device_create_file(&pdev->dev, &dev_attr_enable);
    if (ret < 0)
        goto err1;
       ret=device_create_file(&pdev->dev, &dev_attr_log_buffer);
    if(ret<0)
    {
        device_remove_file(&pdev->dev, &dev_attr_enable);
        goto err1;
    }

    input_dbg("device_create_file completed \r\n");
    input_dev->evbit[0] = BIT_MASK(EV_KEY)  | BIT_MASK(EV_REL);
    input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |BIT_MASK(BTN_RIGHT)|BIT_MASK(BTN_MIDDLE);
    input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y)| BIT_MASK(REL_WHEEL);
    input_dev->keybit[BIT_WORD(BTN_MOUSE)] |=BIT_MASK(BTN_SIDE)|BIT_MASK(BTN_EXTRA);	
    for (i = 0; i<KEY_MAX; i++){
			set_bit( i, input_dev->keybit);
    }

    //clear_bit(0,input_dev->keybit);
    input_dev->name = "aml_keypad";
    input_dev->phys = "keypad/input0";
    //input_dev->cdev.dev = &pdev->dev;
    //input_dev->private = kp;
    input_dev->dev.parent = &pdev->dev;

    input_dev->id.bustype = BUS_ISA;
    input_dev->id.vendor = 0x0001;
    input_dev->id.product = 0x0001;
    input_dev->id.version = 0x0100;
    kp->repeat_enable=0;


    input_dev->rep[REP_DELAY]=0xffffffff;
    input_dev->rep[REP_PERIOD]=0xffffffff;


    input_dev->keycodesize = sizeof(unsigned short);
    input_dev->keycodemax = 0x1ff;

    ret = input_register_device(kp->input);
    if (ret < 0) {
        printk(KERN_ERR "Unable to register keypad input device\n");
        goto err2;
    }
    input_dbg("input_register_device completed \r\n");
    if(hardware_init(pdev))  goto err3;

    register_remote_dev(gp_kp);
    remote_log_buf = (char*)__get_free_pages(GFP_KERNEL,REMOTE_LOG_BUF_ORDER);
    remote_log_buf[0]='\0';
    printk("physical address:0x%x\n",(unsigned int )virt_to_phys(remote_log_buf));
    return 0;
err3:
//     free_irq(NEC_REMOTE_IRQ_NO,kp_interrupt);
    input_unregister_device(kp->input);
    input_dev = NULL;
err2:
    device_remove_file(&pdev->dev, &dev_attr_enable);
    device_remove_file(&pdev->dev, &dev_attr_log_buffer);
err1:

    kfree(kp);
    input_free_device(input_dev);

    return -EINVAL;
}

static int kp_remove(struct platform_device *pdev)
{
    struct kp *kp = platform_get_drvdata(pdev);

    /* disable keypad interrupt handling */
    input_dbg("remove kpads \r\n");
    tasklet_disable(&tasklet);
    tasklet_kill(&tasklet);

    /* unregister everything */
    input_unregister_device(kp->input);
    free_pages((unsigned long)remote_log_buf,REMOTE_LOG_BUF_ORDER);
    device_remove_file(&pdev->dev, &dev_attr_enable);
    device_remove_file(&pdev->dev, &dev_attr_log_buffer);
    if(kp->work_mode & REMOTE_WORK_MODE_FIQ )
    {
        free_fiq(NEC_REMOTE_IRQ_NO, &kp_fiq_interrupt);
        free_irq(BRIDGE_IRQ,gp_kp);
    }
    else
    {
    	 free_irq(NEC_REMOTE_IRQ_NO,kp_interrupt);
    }
    input_free_device(kp->input);


    unregister_chrdev(kp->config_major,kp->config_name);
    if(kp->config_class)
    {
        if(kp->config_dev)
        device_destroy(kp->config_class,MKDEV(kp->config_major,0));
        class_destroy(kp->config_class);
    }

    kfree(kp);
    gp_kp=NULL ;
    return 0;
}

static struct platform_driver kp_driver = {
    .probe      = kp_probe,
    .remove     = kp_remove,
    .suspend    = NULL,
    .resume     = NULL,
    .driver     = {
        .name   = "m1-kp",
    },
};

static int __devinit kp_init(void)
{
    printk(KERN_INFO "Keypad Driver\n");

    return platform_driver_register(&kp_driver);
}

static void __exit kp_exit(void)
{
    printk(KERN_INFO "Keypad exit \n");
    platform_driver_unregister(&kp_driver);
}

module_init(kp_init);
module_exit(kp_exit);

MODULE_AUTHOR("jianfeng_wang");
MODULE_DESCRIPTION("Keypad Driver");
MODULE_LICENSE("GPL");

