/*
 * linux/drivers/input/irremote/sw_remote_kbd.c
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
#include "amkbd_remote.h"

extern  char *remote_log_buf;
static int mode_flag,addbit;
static unsigned int savekeycode;
static int  dbg_printk(const char *fmt, ...)
{
	char buf[100];
	va_list args;

	va_start(args, fmt);  
	vscnprintf(buf,100,fmt,args);
	if(strlen(remote_log_buf)+(strlen(buf)+64)>REMOTE_LOG_BUF_LEN)
		remote_log_buf[0]='\0';
	strcat(remote_log_buf,buf);
	va_end(args);
	return 0;
}

static int  get_pulse_width(unsigned long data)
{
	struct kp *kp_data = (struct kp *) data;
	int  pulse_width;
	const char* state;

	pulse_width = (READ_AOBUS_REG(AO_IR_DEC_REG1) & 0x1FFF0000 ) >> 16;
	state = kp_data->step==REMOTE_STATUS_WAIT?"wait":\
		kp_data->step==REMOTE_STATUS_LEADER?"leader":\
		kp_data->step==REMOTE_STATUS_DATA?"data":\
		kp_data->step==REMOTE_STATUS_SYNC?"sync":NULL;
	dbg_printk("%02d:pulse_wdith:%d==>%s\r\n", kp_data->bit_count - kp_data->bit_num,pulse_width,state);
	//sometimes we found remote  pulse width==0.	in order to sync machine state we modify it .
	if(pulse_width==0){
		switch(kp_data->step){
			case REMOTE_STATUS_LEADER:
				pulse_width = kp_data->time_window[0] + 1;
				break;
			case REMOTE_STATUS_DATA:
				pulse_width = kp_data->time_window[2] + 1;
				break;
		}
	}
	return pulse_width;
}

static inline void kbd_software_mode_remote_wait(unsigned long data)
{
	unsigned short pulse_width;
	struct kp *kp_data = (struct kp *) data;

	pulse_width     = get_pulse_width(data) ;
	kp_data->step   = REMOTE_STATUS_LEADER;
	kp_data->cur_keycode = 0 ;
	kp_data->bit_num = kp_data->bit_count ;
	mode_flag = 0;
	//addbit = 0;
}
static inline void kbd_software_mode_remote_leader(unsigned long data)
{
	unsigned short pulse_width;
	struct kp *kp_data = (struct kp *) data;

	pulse_width     = get_pulse_width(data) ;
	if((pulse_width > kp_data->time_window[0]) && (pulse_width <kp_data->time_window[1])){
		kp_data->step   = REMOTE_STATUS_DATA;
		mode_flag = 1;
		//addbit = 0;
	}
	else {
		kp_data->step    = REMOTE_STATUS_WAIT ;
		mode_flag = 0;
	}

	kp_data->cur_keycode = 0 ;
	kp_data->bit_num = kp_data->bit_count ;
}
static inline void kbd_software_mode_remote_send_key(unsigned long data)
{
	struct kp *kp_data = (struct kp *) data;
	unsigned int mouse_xvalue,mouse_yvalue,tmpkeyscan;
	unsigned int  reort_key_code=kp_data->cur_keycode>>16&0xffff;

	kp_data->step = REMOTE_STATUS_SYNC;
	if(kp_data->repeate_flag){
		if(kp_data->custom_code != (kp_data->cur_keycode&0xffff ))
			return;
		if(((reort_key_code&0xff)^(reort_key_code>>8&0xff))!=0xff)
			return;
		if(kp_data->repeat_tick < jiffies){
			kp_send_key(kp_data->input, (kp_data->cur_keycode>>16)&0xff, 2);
			kp_data->repeat_tick += msecs_to_jiffies(kp_data->input->rep[REP_PERIOD]);
		}   
	}
	else{
		switch(kp_data->work_mode){
			case REMOTE_WORK_MODE_FIQ_RCMM :
				input_dbg("#### cur_keycode code is 0x%08x :: num = %d\n", kp_data->cur_keycode,kp_data->bit_count);
				if(kp_data->bit_count == 46){
					unsigned int  function_flag;
					function_flag = (kp_data->cur_keycode&0x1f);
					mouse_xvalue = (kp_data->cur_keycode>>6)&0x3ff;
				       mouse_yvalue = (kp_data->cur_keycode>>16)&0x3ff;
					function_flag =  ((kp_data->cur_keycode>>5)&0x1)|((function_flag&0x3)<<3)|((function_flag&0xc)>>1);
					mouse_xvalue = (((kp_data->cur_keycode>>4)&0x1)<<10)|((mouse_xvalue&0x3)<<8)|((mouse_xvalue&0xc)<<4)|(mouse_xvalue&0x30)|((mouse_xvalue&0xc0)>>4)|((mouse_xvalue&0x300)>>8);
					//input_dbg(" ((mouse_yvalue&0x3)<<8)  = 0x%08x ;((mouse_yvalue&0xc)<<4) = 0x%08x (mouse_yvalue&0x30)==0x%x ((mouse_yvalue&0xc0)>>4) = 0x%x ((mouse_yvalue&0x300)>>8) =0x%x\n",((mouse_yvalue&0x3)<<8), ((mouse_yvalue&0xc)<<4),(mouse_yvalue&0x30),((mouse_yvalue&0xc0)>>4),((mouse_yvalue&0x300)>>8));
					mouse_yvalue = ((mouse_yvalue&0x3)<<8)|((mouse_yvalue&0xc)<<4)|(mouse_yvalue&0x30)|((mouse_yvalue&0xc0)>>4)|((mouse_yvalue&0x300)>>8);
					input_dbg(" mouse_yvalue  = 0x%08x ;mouse_xvalue = 0x%08x *function_flag==0x%x\n",mouse_yvalue, mouse_xvalue,function_flag);
					kp_data->function_flag = function_flag;
					switch(function_flag&0x1f){
					case 0x0 :	
					input_report_abs(kp_data->input,ABS_X, mouse_xvalue&0x7ff);
					input_report_abs(kp_data->input,ABS_Y, mouse_yvalue&0x3ff);
					input_sync(kp_data->input);
					input_dbg(" abs ms\n");
					break;
					case 0x1 :
						input_report_key(kp_data->input, BTN_RIGHT, 1);
						input_sync(kp_data->input);
						input_dbg(" press rightkey\n");
						kp_data->release_delay = kp_data->tmp_release_delay;
					break;
					case 0x4 :
						input_report_key(kp_data->input, BTN_LEFT, 1);
						input_sync(kp_data->input);
						input_dbg("press left  key \n");
						kp_data->release_delay = kp_data->tmp_release_delay;
					break;
					}
				}
				else if(kp_data->bit_count == 32)
					kp_send_key(kp_data->input, 0x100|(kp_data->cur_keycode>>(kp_data->bit_count - 8)), 1);
				else if(kp_data->bit_count == 12){
						 tmpkeyscan =kp_data->cur_keycode>>(kp_data->bit_count - 8);
						 kp_data->cur_keycode = ((tmpkeyscan&0x3)<<6)|((tmpkeyscan&0xc)<<2)|((tmpkeyscan&0x30)>>2)|((tmpkeyscan&0xc0)>>6);
					 	 kp_send_key(kp_data->input, kp_data->cur_keycode&0xff, 1);
						 input_dbg(" tmpkeyscan = 0x%08x ********cur_keycode code is 0x%08x\n", tmpkeyscan,kp_data->cur_keycode);
				}
				else
					kp_send_key(kp_data->input, kp_data->cur_keycode>>(kp_data->bit_count - 8), 1);
				break;
			default :
				if(kp_data->custom_code != (kp_data->cur_keycode&0xffff )){
					input_dbg("Wrong custom code is 0x%08x\n", kp_data->cur_keycode);
					return ;
				}
				if(((reort_key_code&0xff)^(reort_key_code>>8&0xff))==0xff)
					kp_send_key(kp_data->input, (kp_data->cur_keycode>>16)&0xff, 1);
		}
		if(kp_data->repeat_enable)
			kp_data->repeat_tick = jiffies + msecs_to_jiffies(kp_data->input->rep[REP_DELAY]);
	}
}
static inline void kbd_software_mode_remote_data(unsigned long data)
{
	unsigned short pulse_width;
	struct kp *kp_data = (struct kp *) data;

	pulse_width     = get_pulse_width(data) ;
	kp_data->step   = REMOTE_STATUS_DATA ;
	mode_flag ++;
	switch(kp_data->work_mode){
		case REMOTE_WORK_MODE_SW :
		case REMOTE_WORK_MODE_FIQ :
			
			if((pulse_width > kp_data->time_window[2]) && (pulse_width < kp_data->time_window[3])) {
				kp_data->bit_num--;
			}
			else if((pulse_width > kp_data->time_window[4]) && (pulse_width < kp_data->time_window[5])) {
				kp_data->cur_keycode |= 1<<(kp_data->bit_count-kp_data->bit_num) ;
				kp_data->bit_num--;
			}else {
				kp_data->step   = REMOTE_STATUS_WAIT ;
			}
			if(kp_data->bit_num == 0) {
				kp_data->repeate_flag= 0;
				kp_data->send_data=1;	
				if(kp_data->work_mode==REMOTE_WORK_MODE_FIQ)
					fiq_bridge_pulse_trigger(&kp_data->fiq_handle_item);
				else
					remote_bridge_isr(0,kp_data);	
			}
			break;
		case REMOTE_WORK_MODE_FIQ_RCMM :
			if(kp_data->bit_num == kp_data->bit_count -32)
			{
				addbit = 1;
				savekeycode = kp_data->cur_keycode;
				kp_data->cur_keycode = 0;
				kp_data->bit_count = kp_data->bit_count -32;
			}
			if((pulse_width > kp_data->time_window[2]) && (pulse_width < kp_data->time_window[3])) {//00
				if((mode_flag == 2)&&(kp_data->bit_count == 12)){
					kp_data->bit_count = kp_data->bit_num = 46;
				}
#if 0
				if((kp_data->bit_num == 12)&&(kp_data->bit_count == 24)){/*sub mode is remote control*/
					kp_data->bit_count += 8;
					kp_data->bit_num += 8;
				}
#endif
				kp_data->bit_num-=2;
			}else if((pulse_width > kp_data->time_window[4]) && (pulse_width < kp_data->time_window[5])) {//01
				kp_data->cur_keycode |= 1<<(kp_data->bit_count-kp_data->bit_num) ;
				kp_data->bit_num-=2;
			}else if((pulse_width > kp_data->time_window[8]) && (pulse_width < kp_data->time_window[9])) {//10
				if((mode_flag == 2)&&(kp_data->bit_count == 46)){
					kp_data->bit_count = kp_data->bit_num = 12;
				}
				kp_data->cur_keycode |= 2<<(kp_data->bit_count-kp_data->bit_num) ;
#if 0				
				if((kp_data->bit_num == 20)&&(kp_data->bit_count == 32)){/*sub mode is keyboard*/
					kp_data->bit_count -= 8;
					kp_data->bit_num -= 8;
				}
#endif
				kp_data->bit_num-=2;
			}else if((pulse_width > kp_data->time_window[10]) && (pulse_width < kp_data->time_window[11])) {//11
				kp_data->cur_keycode |= 3<<(kp_data->bit_count-kp_data->bit_num) ;
				kp_data->bit_num-=2;
			}else {
				kp_data->step   = REMOTE_STATUS_WAIT ;
			}
			if(kp_data->bit_num == 0) {
				kp_data->repeate_flag = 0;
				if(addbit  == 1)
				{
					kp_data->bit_count = 46;
					kp_data->cur_keycode = ((kp_data->cur_keycode&0x3fff)<<12)|(savekeycode>>20);
					savekeycode =0;
					addbit = 0;
					kp_data->release_delay = 17;
				}
				else
					kp_data->release_delay =kp_data->tmp_release_delay;
				kp_data->send_data = 1;	
				fiq_bridge_pulse_trigger(&kp_data->fiq_handle_item);
				mode_flag = 0;
			}
			break;            
	}

}
static inline void kbd_software_mode_remote_sync(unsigned long data)
{
	unsigned short pulse_width;
	struct kp *kp_data = (struct kp *) data;

	pulse_width = get_pulse_width(data);
	if((pulse_width > kp_data->time_window[6]) && (pulse_width < kp_data->time_window[7])) {
		kp_data->repeate_flag=1;
		if(kp_data->repeat_enable)
			kp_data->send_data=1;		  	
		else{
			kp_data->step  = REMOTE_STATUS_SYNC;
			return;
		}
	}
	kp_data->step  = REMOTE_STATUS_SYNC;
	if((kp_data->work_mode==REMOTE_WORK_MODE_FIQ)||(kp_data->work_mode==REMOTE_WORK_MODE_FIQ_RCMM))
		fiq_bridge_pulse_trigger(&kp_data->fiq_handle_item);
	else
		remote_bridge_isr(0,kp_data);
}
void kp_sw_reprot_key(unsigned long data)
{
	struct kp *kp_data = (struct kp *) data;
	int	current_jiffies=jiffies;
	if(((current_jiffies - kp_data->last_jiffies) > 20) && (kp_data->step  <=  REMOTE_STATUS_SYNC)){
		kp_data->step = REMOTE_STATUS_WAIT;
	}
	kp_data->last_jiffies = current_jiffies ;  //ignore a little msecs
	switch( kp_data->step){
		case REMOTE_STATUS_WAIT:
			kbd_software_mode_remote_wait(data) ;
			break;
		case REMOTE_STATUS_LEADER:
			kbd_software_mode_remote_leader(data);
			break;
		case REMOTE_STATUS_DATA:
			kbd_software_mode_remote_data(data) ;
			break;
		case REMOTE_STATUS_SYNC:
			kbd_software_mode_remote_sync(data) ;
			break;
		default:
			break;
	}
}
irqreturn_t remote_bridge_isr(int irq, void *dev_id)
{
	struct kp *kp_data = (struct kp *) dev_id;

	if(kp_data->send_data) //report key 
	{
		kbd_software_mode_remote_send_key((unsigned long)kp_data);
		kp_data->send_data=0;
	}
	kp_data->timer.data=(unsigned long)kp_data;
	mod_timer(&kp_data->timer,jiffies+msecs_to_jiffies(kp_data->release_delay));
	return IRQ_HANDLED;
}
