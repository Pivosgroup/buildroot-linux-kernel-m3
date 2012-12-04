#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/errno.h>

#include <asm/uaccess.h>
#include <asm/delay.h>
#include <mach/am_regs.h>
#include <mach/power_gate.h>
#include <linux/tvin/tvin.h>

#include <mach/gpio.h>

#include "m1/hdmi_tx_reg.h"
#include "hdmi_tx_module.h"
#include "hdmi_tx_cec.h"
//#include "hdmi_cec_key.h"

unsigned int cec_key_flag =0;

__u16 cec_key_map[128] = {
    KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, 0 , 0 , 0 ,//0x00
    0 , KEY_HOMEPAGE , KEY_MENU, 0, 0, KEY_BACK, 0, 0,
    0 , 0, 0, 0, 0, 0, 0, 0,//0x10
    0 , 0, 0, 0, 0, 0, 0, 0,
    KEY_0 , KEY_1, KEY_2, KEY_3,KEY_4, KEY_5, KEY_6, KEY_7,//0x20
    KEY_8 , KEY_9, KEY_DOT, 0, 0, 0, 0, 0,
    0 , 0, 0, 0, 0, 0, 0, 0,//0x30
    0 , 0, 0, 0, 0, 0, 0, 0,
    
    KEY_POWER , KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_MUTE, KEY_PLAYPAUSE, KEY_STOP, KEY_PLAYPAUSE, 0,//0x40
    KEY_REWIND, KEY_FASTFORWARD, 0, KEY_PREVIOUSSONG, KEY_NEXTSONG, 0, 0, 0,
    0 , 0, 0, 0, 0, 0, 0, 0,//0x50
    0 , 0, 0, 0, 0, 0, 0, 0,
    KEY_PLAYCD, KEY_PLAYPAUSE, KEY_RECORD, KEY_PAUSECD, KEY_STOPCD, KEY_MUTE, 0, KEY_TUNER,//0x60
    0 , KEY_MEDIA, 0, 0, KEY_POWER, KEY_POWER, 0, 0,
    0 , KEY_BLUE, KEY_RED, KEY_GREEN, KEY_YELLOW, 0, 0, 0,//0x70
    0 , 0, 0, 0, 0, 0, 0, 0,
};

void cec_send_event(cec_rx_message_t* pcec_message)
{
    int i;
    unsigned char brdcst, opcode;
    unsigned char initiator, follower;
    unsigned char operand_num;
    unsigned char msg_length;
    unsigned char operands[14];
    
    /* parse message */
    if ((!pcec_message) || (check_cec_msg_valid(pcec_message) == 0)) return;

    initiator   = pcec_message->content.msg.header >> 4;
    follower    = pcec_message->content.msg.header & 0x0f;
    opcode      = pcec_message->content.msg.opcode;   
    operand_num = pcec_message->operand_num;
    brdcst      = (follower == 0x0f);
    msg_length  = pcec_message->msg_length;
    
    for (i = 0; i < operand_num; i++ ) {
       operands[i] = pcec_message->content.msg.operands[i]; 
       hdmitx_cec_dbg_print("\n--------operands[%d]:%u---------\n", i, operands[i]);       
    }
    if(cec_key_flag) {
        input_event(remote_cec_dev, EV_KEY, cec_key_map[operands[0]], 1);
        input_sync(remote_cec_dev);
        hdmitx_cec_dbg_print("\n--------cec_key_map[operands[0]]:%d---------\n",cec_key_map[operands[0]]);
    }
    else{
        input_event(remote_cec_dev, EV_KEY, cec_key_map[operands[0]], 0);
        input_sync(remote_cec_dev);
        hdmitx_cec_dbg_print("\n--------cec_key_map[operands[0]]:%d---------\n",cec_key_map[operands[0]]);
    }   

    hdmitx_cec_dbg_print("\n--------cec_send_event---------\n");
}


void cec_send_event_irq(void)
{
    int i;
    unsigned char   operand_num_irq;
    unsigned char operands_irq[14];
    //unsigned char  opcode_irq;    
    //unsigned char   msg_length_irq;
    
    // msg_length_irq  = cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].msg_length;           
    //opcode_irq	= cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].content.msg.opcode;
         
    operand_num_irq = cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].operand_num;
    for (i = 0; i < operand_num_irq; i++ )
    {
        operands_irq[i] = cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].content.msg.operands[i]; 
        hdmitx_cec_dbg_print("\n--------operands_irq[%d]:%u---------\n", i, operands_irq[i]);       
    }
    
    switch(cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].content.msg.operands[0]){
    case 0x33:
        cec_system_audio_mode_request();
        //cec_set_system_audio_mode();
        break;
    case 0x35:
        break;
    default:
        break;      
    }	
    
    input_event(remote_cec_dev, EV_KEY, cec_key_map[operands_irq[0]], 1);
    input_sync(remote_cec_dev);	
    input_event(remote_cec_dev, EV_KEY, cec_key_map[operands_irq[0]], 0);
    input_sync(remote_cec_dev);
    hdmitx_cec_dbg_print("\n--------cec_key_map[operands_irq[0]]:%d---------\n",cec_key_map[operands_irq[0]]);       		
   	
    hdmitx_cec_dbg_print("\n--------cec_send_event_irq---------\n");  	 	
}

void cec_user_control_pressed_irq(void)
{
    hdmitx_cec_dbg_print("\nCEC Key pressed \n");
    //pcec_message->content.msg.flag = 1;
    //cec_key_flag = 1;
    cec_send_event_irq();
}

void cec_user_control_released_irq(void)  
{
    hdmitx_cec_dbg_print("\nCEC Key released \n");
    //pcec_message->content.msg.flag = 0;
    // cec_key_flag = 0;
    //cec_send_event_irq();
}

void cec_standby_irq(void)
{
    printk("CEC: System will be in standby mode\n");
    input_event(remote_cec_dev, EV_KEY, KEY_POWER, 1);
    input_sync(remote_cec_dev);
    input_event(remote_cec_dev, EV_KEY, KEY_POWER, 0);
    input_sync(remote_cec_dev);
    
    //cec_send_event_irq();
}

void cec_user_control_pressed(cec_rx_message_t* pcec_message)
{
    printk("\nCEC Key pressed \n");
    //pcec_message->content.msg.flag = 1;
    cec_key_flag = 1;
    cec_send_event(pcec_message);
}

void cec_user_control_released(cec_rx_message_t* pcec_message)  
{
    printk("\nCEC Key released \n");
    //pcec_message->content.msg.flag = 0;
    cec_key_flag = 1;
    cec_send_event(pcec_message);
}

void cec_standby(cec_rx_message_t* pcec_message)
{
    printk("CEC: System will be in standby mode\n");
    input_event(remote_cec_dev, EV_KEY, KEY_POWER, 1);
    input_sync(remote_cec_dev);
    input_event(remote_cec_dev, EV_KEY, KEY_POWER, 0);
    input_sync(remote_cec_dev);
    
    //cec_send_event(pcec_message);
}
