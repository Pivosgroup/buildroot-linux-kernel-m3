/*
 * Amlogic M1 
 * HDMI CEC Driver-----------HDMI_TX
 * Copyright (C) 2011 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

#include <asm/uaccess.h>
#include <asm/delay.h>
#include <mach/am_regs.h>
#include <mach/power_gate.h>
#include <linux/tvin/tvin.h>

#include <mach/gpio.h>


#include "m1/hdmi_tx_reg.h"
#include "hdmi_tx_module.h"
#include "hdmi_tx_cec.h"

//#define _RX_CEC_DBG_ON_

#ifdef _RX_CEC_DBG_ON_
#define hdmirx_cec_dbg_print(fmt, args...) printk(KERN_DEBUG fmt, ## args)//hdmi_print
#else
#define hdmirx_cec_dbg_print(fmt, args...)
#endif

#define _RX_DATA_BUF_SIZE_ 6

/* global variables */
static	unsigned char    gbl_msg[MAX_MSG];
static cec_global_info_t cec_global_info;

static struct semaphore  tv_cec_sema;

static DEFINE_SPINLOCK(p_tx_list_lock);
//static DEFINE_SPINLOCK(cec_tx_lock);

static unsigned long cec_tx_list_flags;
//static unsigned long cec_tx_flags;
static unsigned int tx_msg_cnt = 0;

static struct list_head cec_tx_msg_phead = LIST_HEAD_INIT(cec_tx_msg_phead);

static tv_cec_pending_e cec_pending_flag = TV_CEC_PENDING_OFF;
static tv_cec_polling_state_e cec_polling_state = TV_CEC_POLLING_OFF;

unsigned int menu_lang_array[] = {(((unsigned int)'c')<<16)|(((unsigned int)'h')<<8)|((unsigned int)'i'),
                                  (((unsigned int)'e')<<16)|(((unsigned int)'n')<<8)|((unsigned int)'g'),
                                  (((unsigned int)'j')<<16)|(((unsigned int)'p')<<8)|((unsigned int)'n'),
                                  (((unsigned int)'k')<<16)|(((unsigned int)'o')<<8)|((unsigned int)'r'),
                                  (((unsigned int)'f')<<16)|(((unsigned int)'r')<<8)|((unsigned int)'a'),
                                  (((unsigned int)'g')<<16)|(((unsigned int)'e')<<8)|((unsigned int)'r')
                                 };

static unsigned char * default_osd_name[16] = {
    "tv",
    "recording 1",
    "recording 2",
    "tuner 1",
    "AML STB/MID1",
    "audio system",
    "tuner 2",
    "tuner 3",
    "AML STB/MID2",
    "recording 3",
    "tunre 4",
    "AML STB/MID3",
    "reserved 1",
    "reserved 2",
    "free use",
    "unregistered"
};

static struct {
    cec_rx_message_t cec_rx_message[_RX_DATA_BUF_SIZE_];
    unsigned char rx_write_pos;
    unsigned char rx_read_pos;
    unsigned char rx_buf_size;
} cec_rx_msg_buf;

static unsigned char * osd_name_uninit = "\0\0\0\0\0\0\0\0";
static irqreturn_t cec_isr_handler(int irq, void *dev_instance);

static struct timer_list tv_cec_timer;

//static unsigned char dev = 0;
static unsigned char cec_init_flag = 0;
static unsigned char cec_mutex_flag = 0;

void tv_cec_timer_func(unsigned long arg);

//static unsigned int hdmi_rd_reg(unsigned long addr);
//static void hdmi_wr_reg(unsigned long addr, unsigned long data);

void cec_test_function(unsigned char* arg, unsigned char arg_cnt)
{
//    int i;
//    char buf[512];
//
//    switch (arg[0]) {
//    case 0x0:
//        cec_usrcmd_parse_all_dev_online();
//        break;
//    case 0x2:
//        cec_usrcmd_get_audio_status(arg[1]);
//        break;
//    case 0x3:
//        cec_usrcmd_get_deck_status(arg[1]);
//        break;
//    case 0x4:
//        cec_usrcmd_get_device_power_status(arg[1]);
//        break;
//    case 0x5:
//        cec_usrcmd_get_device_vendor_id(arg[1]);
//        break;
//    case 0x6:
//        cec_usrcmd_get_osd_name(arg[1]);
//        break;
//    case 0x7:
//        cec_usrcmd_get_physical_address(arg[1]);
//        break;
//    case 0x8:
//        cec_usrcmd_get_system_audio_mode_status(arg[1]);
//        break;
//    case 0x9:
//        cec_usrcmd_get_tuner_device_status(arg[1]);
//        break;
//    case 0xa:
//        cec_usrcmd_set_deck_cnt_mode(arg[1], arg[2]);
//        break;
//    case 0xc:
//        cec_usrcmd_set_imageview_on(arg[1]);
//        break;
//    case 0xd:
//        cec_usrcmd_set_play_mode(arg[1], arg[2]);
//        break;
//    case 0xe:
//        cec_usrcmd_get_menu_state(arg[1]);
//        break;
//    case 0xf:
//        cec_usrcmd_set_menu_state(arg[1], arg[2]);
//        break;
//    case 0x10:
//        cec_usrcmd_get_global_info(buf);
//        break;
//    case 0x11:
//        cec_usrcmd_get_menu_language(arg[1]);
//        break;
//    case 0x12:
//        cec_usrcmd_set_menu_language(arg[1], arg[2]);
//        break;
//    case 0x13:
//        cec_usrcmd_get_active_source();
//        break;
//    case 0x14:
//        cec_usrcmd_set_active_source();
//        break;
//    case 0x15:
//        cec_usrcmd_set_deactive_source(arg[1]);
//        break;
//    case 0x17:
//        cec_usrcmd_set_report_physical_address(arg[1], arg[2], arg[3], arg[4]);
//        break;
//    case 0x18:
//    	{int i = 0;
//    	cec_polling_online_dev(arg[1], &i);
//    	}
//    	break;
//    default:
//        break;
//    }
}


/***************************** cec low level code *****************************/

static unsigned int cec_get_ms_tick(void)
{
    unsigned int ret = 0;
    struct timeval cec_tick;
    do_gettimeofday(&cec_tick);
    ret = cec_tick.tv_sec * 1000 + cec_tick.tv_usec / 1000;

    return ret;
}

static unsigned int cec_get_ms_tick_interval(unsigned int last_tick)
{
    unsigned int ret = 0;
    unsigned int tick = 0;
    struct timeval cec_tick;
    do_gettimeofday(&cec_tick);
    tick = cec_tick.tv_sec * 1000 + cec_tick.tv_usec / 1000;

    if (last_tick < tick) ret = tick - last_tick;
    else ret = ((unsigned int)(-1) - last_tick) + tick;
    return ret;
}

#define TX_TIME_OUT_CNT 300

int cec_ll_tx(unsigned char *msg, unsigned char len, unsigned char *stat_header)
{
    int i;
    int ret;
    int tick = 0;
    int cnt = 0;

//    spin_lock_irqsave(&cec_tx_lock, cec_tx_flags);

    for (i = 0; i < len; i++) {
        hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_0_HEADER + i, msg[i]);
    }
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_LENGTH, len-1);
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_CMD, TX_REQ_CURRENT);
    
//    printk("CEC: follow interrupt?\n");

    if (stat_header == NULL) {
        tick = cec_get_ms_tick();

        while (hdmi_rd_reg(CEC0_BASE_ADDR+CEC_TX_MSG_STATUS) == TX_BUSY) {
            msleep(50);
//            {
//                static int i = 1;
//                printk("CEC: tx i1 = %d\n", i);
//                i++;
//            }
            cnt = cec_get_ms_tick_interval(tick);
            if (cnt >= TX_TIME_OUT_CNT)
                break;
        }
    } else if (*stat_header == 1) { // ping
        tick = cec_get_ms_tick();

        while (hdmi_rd_reg(CEC0_BASE_ADDR+CEC_TX_MSG_STATUS) == TX_BUSY) {
            msleep(50);
//            {
//                static int i = 1;
//                printk("CEC: tx i2 = %d\n", i);
//                i++;
//            }
            cnt = cec_get_ms_tick_interval(tick);
            if (cnt >= (TX_TIME_OUT_CNT / 2))
                break;
        }
    }

    ret = hdmi_rd_reg(CEC0_BASE_ADDR+CEC_TX_MSG_STATUS);
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_CMD, TX_NO_OP);

    if (cnt >= TX_TIME_OUT_CNT){
        hdmirx_cec_dbg_print("CEC: tx time out: %d\n", cnt);
    }
//    spin_unlock_irqrestore(&cec_tx_lock, cec_tx_flags);

    return ret;
}

#define RX_TIME_OUT_CNT 0x10

int cec_ll_rx( unsigned char *msg, unsigned char *len)
{
    unsigned char rx_status = hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_STATUS);
    int i;
    int tick = 0;
    int cnt = 0;
    unsigned char data;

    int rx_msg_length = hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_LENGTH) + 1;

    for (i = 0; i < rx_msg_length; i++) {
        data = hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_0_HEADER +i);
        *msg = data;
        msg++;
        //hdmirx_cec_dbg_print("cec rx message %x = %x\n", i, data);
    }
    *len = rx_msg_length;
    hdmi_wr_reg(CEC0_BASE_ADDR + CEC_TX_MSG_CMD,  RX_ACK_CURRENT);

    tick = cec_get_ms_tick();
    while (hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_STATUS) == RX_BUSY) {
        cnt = cec_get_ms_tick_interval(tick);
        if (cnt++ >= RX_TIME_OUT_CNT)
            break;
    }
    hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_MSG_CMD,  RX_NO_OP);

    if (cnt >= RX_TIME_OUT_CNT)
        hdmirx_cec_dbg_print("CEC: rx time out cnt = %x\n", cnt);

    return rx_status;
}

void cec_isr_post_process(void)
{
    /* isr post process */
    while(cec_rx_msg_buf.rx_read_pos != cec_rx_msg_buf.rx_write_pos) {
        cec_handle_message(&(cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_read_pos]));
        if (cec_rx_msg_buf.rx_read_pos == cec_rx_msg_buf.rx_buf_size - 1) {
            cec_rx_msg_buf.rx_read_pos = 0;
        } else {
            cec_rx_msg_buf.rx_read_pos++;
        }
    }

    //printk("[TV CEC RX]: rx_read_pos %x, rx_write_pos %x\n", cec_rx_msg_buf.rx_read_pos, cec_rx_msg_buf.rx_write_pos);
}

void cec_usr_cmd_post_process(void)
{
    cec_tx_message_list_t *p, *ptmp;

    /* usr command post process */
    //spin_lock_irqsave(&p_tx_list_lock, cec_tx_list_flags);

    list_for_each_entry_safe(p, ptmp, &cec_tx_msg_phead, list) {
        cec_ll_tx(p->msg, p->length, NULL);
        unregister_cec_tx_msg(p);
    }

    //spin_unlock_irqrestore(&p_tx_list_lock, cec_tx_list_flags);

    //printk("[TV CEC TX]: tx_msg_cnt = %x\n", tx_msg_cnt);
}

////void cec_timer_post_process(void)
////{
////    /* timer post process*/
////    if (cec_polling_state == TV_CEC_POLLING_ON) {
////        cec_tv_polling_online_dev();
////        cec_polling_state = TV_CEC_POLLING_OFF;
////    }
////}
void cec_node_init(hdmitx_dev_t* hdmitx_device)
{
	int i, bool = 0;
	enum _cec_log_dev_addr_e player_dev[3] = {   CEC_PLAYBACK_DEVICE_1_ADDR,
	    										 CEC_PLAYBACK_DEVICE_2_ADDR,
	    										 CEC_PLAYBACK_DEVICE_3_ADDR,
	    									  };
    if(!hdmitx_device->cec_func_flag)
        return ;
    // If VSDB is not valid, wait
    while(hdmitx_device->hdmi_info.vsdb_phy_addr.valid == 0)
        msleep(100);
        
    // Clear CEC Int. state and set CEC Int. mask
    WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR, READ_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR) | (1 << 23));    // Clear the interrupt
    WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK, READ_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK) | (1 << 23));            // Enable the hdmi cec interrupt

    init_timer(&tv_cec_timer);
    tv_cec_timer.data = (ulong) & tv_cec_timer;
    tv_cec_timer.function = tv_cec_timer_func;
    tv_cec_timer.expires = jiffies + TV_CEC_INTERVAL;
    add_timer(&tv_cec_timer);
	for(i = 0; i < 3; i++){ 
//	    printk("CEC: start poll dev\n");  	
		cec_polling_online_dev(player_dev[i], &bool);
//		printk("CEC: end poll dev\n");
		if(bool == 0){  // 0 means that no any respond
		    cec_global_info.my_node_index = player_dev[i];
			cec_global_info.cec_node_info[player_dev[i]].log_addr = player_dev[i];
		    // Set Physical address
	    	cec_global_info.cec_node_info[player_dev[i]].phy_addr.phy_addr_4 = ( ((hdmitx_device->hdmi_info.vsdb_phy_addr.a)<<12)
            	    												 +((hdmitx_device->hdmi_info.vsdb_phy_addr.b)<< 8)
            	    												 +((hdmitx_device->hdmi_info.vsdb_phy_addr.c)<< 4)
            	    												 +((hdmitx_device->hdmi_info.vsdb_phy_addr.d)    )
            	    												);
            cec_global_info.cec_node_info[player_dev[i]].vendor_id = ('A'<<16)|('m'<<8)|'l';
			cec_global_info.cec_node_info[player_dev[i]].power_status = POWER_ON,
			strcpy(cec_global_info.cec_node_info[player_dev[i]].osd_name, default_osd_name[player_dev[i]]); //Max num: 14Bytes
			hdmi_wr_reg(CEC0_BASE_ADDR+CEC_LOGICAL_ADDR0, (0x1 << 4) | player_dev[i]);
		    
     		printk("CEC: Set logical address: %d\n", player_dev[i]);

		    cec_usrcmd_set_report_physical_address();
		    
		    cec_usrcmd_set_imageview_on( CEC_TV_ADDR );   // Wakeup TV
		    msleep(200);
		    cec_usrcmd_set_imageview_on( CEC_TV_ADDR );   // Wakeup TV again
		    msleep(200);
     		//printk("CEC: Set physical address: %x\n", cec_global_info.cec_node_info[player_dev[i]].phy_addr.phy_addr_4);
    		cec_usrcmd_set_active_source();
    		break;
		}
	}
	if(bool == 1)
		printk("CEC: Can't get a valid logical address\n");
}

void cec_node_uninit(hdmitx_dev_t* hdmitx_device)
{
    if(!hdmitx_device->cec_func_flag)
        return ;
    WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK, READ_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK) & ~(1 << 23));            // Disable the hdmi cec interrupt
    //free_irq(INT_HDMI_CEC, (void *)hdmitx_device);
    del_timer_sync(&tv_cec_timer);
}

static int cec_task(void *data)
{
	extern void dump_hdmi_cec_reg(void);
    hdmitx_dev_t* hdmitx_device = (hdmitx_dev_t*) data;

//    printk("CEC: Physical Address [A]: %x\n",hdmitx_device->hdmi_info.vsdb_phy_addr.a);
//    printk("CEC: Physical Address [B]: %x\n",hdmitx_device->hdmi_info.vsdb_phy_addr.b);
//    printk("CEC: Physical Address [C]: %x\n",hdmitx_device->hdmi_info.vsdb_phy_addr.c);
//    printk("CEC: Physical Address [D]: %x\n",hdmitx_device->hdmi_info.vsdb_phy_addr.d);

    cec_init_flag = 1;
    
    cec_node_init(hdmitx_device);
    
//    dump_hdmi_cec_reg();
    
    // Get logical address

    printk("CEC: CEC task process\n");

    while (1) {
            
        down_interruptible(&tv_cec_sema);
        cec_isr_post_process();
        cec_usr_cmd_post_process();
        //\\cec_timer_post_process();
    }

    return 0;
}

/***************************** cec low level code end *****************************/


/***************************** cec middle level code *****************************/

void tv_cec_timer_func(unsigned long arg)
{
    struct timer_list *timer = (struct timer_list *)arg;
    timer->expires = jiffies + TV_CEC_INTERVAL;

    if (cec_pending_flag == TV_CEC_PENDING_OFF) {
        cec_polling_state = TV_CEC_POLLING_ON;
    }

    add_timer(timer);

    up(&tv_cec_sema);
}

void register_cec_rx_msg(unsigned char *msg, unsigned char len )
{
    memset((void*)(&(cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos])), 0, sizeof(cec_rx_message_t));
    memcpy(cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].content.buffer, msg, len);

    cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].operand_num = len >= 2 ? len - 2 : 0;
    cec_rx_msg_buf.cec_rx_message[cec_rx_msg_buf.rx_write_pos].msg_length = len;

    if (cec_rx_msg_buf.rx_write_pos == cec_rx_msg_buf.rx_buf_size - 1) {
        cec_rx_msg_buf.rx_write_pos = 0;
    } else {
        cec_rx_msg_buf.rx_write_pos++;
    }

    up(&tv_cec_sema);    
}

void register_cec_tx_msg(unsigned char *msg, unsigned char len )
{
    cec_tx_message_list_t* cec_usr_message_list = kmalloc(sizeof(cec_tx_message_list_t), GFP_ATOMIC);

    if (cec_usr_message_list != NULL) {
        memset(cec_usr_message_list, 0, sizeof(cec_tx_message_list_t));
        memcpy(cec_usr_message_list->msg, msg, len);
        cec_usr_message_list->length = len;

        spin_lock_irqsave(&p_tx_list_lock, cec_tx_list_flags);
        list_add_tail(&cec_usr_message_list->list, &cec_tx_msg_phead);
        spin_unlock_irqrestore(&p_tx_list_lock, cec_tx_list_flags);

        tx_msg_cnt++;

        up(&tv_cec_sema);
    }
}

void unregister_cec_tx_msg(cec_tx_message_list_t* cec_tx_message_list)
{
    if (cec_tx_message_list != NULL) {
        list_del(&cec_tx_message_list->list);
        kfree(cec_tx_message_list);
        cec_tx_message_list = NULL;

        if (tx_msg_cnt > 0) tx_msg_cnt--;
    }
}

void cec_hw_reset(void)
{
    unsigned char index = cec_global_info.my_node_index;
    WRITE_APB_REG(HDMI_CNTL_PORT, READ_APB_REG(HDMI_CNTL_PORT)|(1<<16));
    hdmi_wr_reg(OTHER_BASE_ADDR+HDMI_OTHER_CTRL0, 0xc); //[3]cec_creg_sw_rst [2]cec_sys_sw_rst
    hdmi_wr_reg(OTHER_BASE_ADDR+CEC_TX_CLEAR_BUF, 0x1);
    hdmi_wr_reg(OTHER_BASE_ADDR+CEC_RX_CLEAR_BUF, 0x1);
    
    //mdelay(10);
    {//Delay some time
    	int i = 10;
    	while(i--);
    }
    hdmi_wr_reg(OTHER_BASE_ADDR+CEC_TX_CLEAR_BUF, 0x0);
    hdmi_wr_reg(OTHER_BASE_ADDR+CEC_RX_CLEAR_BUF, 0x0);
    hdmi_wr_reg(OTHER_BASE_ADDR+HDMI_OTHER_CTRL0, 0x0);
    WRITE_APB_REG(HDMI_CNTL_PORT, READ_APB_REG(HDMI_CNTL_PORT)&(~(1<<16)));

    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_H, 0x00 );
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_L, 0xf0 );

    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_LOGICAL_ADDR0, (0x1 << 4) | cec_global_info.cec_node_info[index].log_addr);
}

static unsigned char check_cec_msg_valid(const cec_rx_message_t* pcec_message)
{
    unsigned char rt = 0;
    unsigned char opcode;
    unsigned char opernum;
    if (!pcec_message)
        return rt;

    opcode = pcec_message->content.msg.opcode;
    opernum = pcec_message->operand_num;

    switch (opcode) {
        case CEC_OC_VENDOR_REMOTE_BUTTON_UP:
        case CEC_OC_STANDBY:
        case CEC_OC_RECORD_OFF:
        case CEC_OC_RECORD_TV_SCREEN:
        case CEC_OC_TUNER_STEP_DECREMENT:
        case CEC_OC_TUNER_STEP_INCREMENT:
        case CEC_OC_GIVE_AUDIO_STATUS:
        case CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS:
        case CEC_OC_USER_CONTROL_RELEASED:
        case CEC_OC_GIVE_OSD_NAME:
        case CEC_OC_GIVE_PHYSICAL_ADDRESS:
        case CEC_OC_GET_CEC_VERSION:
        case CEC_OC_GET_MENU_LANGUAGE:
        case CEC_OC_GIVE_DEVICE_VENDOR_ID:
        case CEC_OC_GIVE_DEVICE_POWER_STATUS:
        case CEC_OC_TEXT_VIEW_ON:
        case CEC_OC_IMAGE_VIEW_ON:
        case CEC_OC_ABORT_MESSAGE:
        case CEC_OC_REQUEST_ACTIVE_SOURCE:
            if ( opernum == 0)  rt = 1;
            break;
        case CEC_OC_SET_SYSTEM_AUDIO_MODE:
        case CEC_OC_RECORD_STATUS:
        case CEC_OC_DECK_CONTROL:
        case CEC_OC_DECK_STATUS:
        case CEC_OC_GIVE_DECK_STATUS:
        case CEC_OC_GIVE_TUNER_DEVICE_STATUS:
        case CEC_OC_PLAY:
        case CEC_OC_MENU_REQUEST:
        case CEC_OC_MENU_STATUS:
        case CEC_OC_REPORT_AUDIO_STATUS:
        case CEC_OC_TIMER_CLEARED_STATUS:
        case CEC_OC_SYSTEM_AUDIO_MODE_STATUS:
        case CEC_OC_USER_CONTROL_PRESSED:
        case CEC_OC_CEC_VERSION:
        case CEC_OC_REPORT_POWER_STATUS:
        case CEC_OC_SET_AUDIO_RATE:
            if ( opernum == 1)  rt = 1;
            break;
        case CEC_OC_INACTIVE_SOURCE:
        case CEC_OC_SYSTEM_AUDIO_MODE_REQUEST:
        case CEC_OC_FEATURE_ABORT:
        case CEC_OC_ACTIVE_SOURCE:
        case CEC_OC_ROUTING_INFORMATION:
        case CEC_OC_SET_STREAM_PATH:
            if (opernum == 2) rt = 1;
            break;
        case CEC_OC_REPORT_PHYSICAL_ADDRESS:
        case CEC_OC_SET_MENU_LANGUAGE:
        case CEC_OC_DEVICE_VENDOR_ID:
            if (opernum == 3) rt = 1;
            break;
        case CEC_OC_ROUTING_CHANGE:
        case CEC_OC_SELECT_ANALOGUE_SERVICE:
            if (opernum == 4) rt = 1;
            break;
        case CEC_OC_VENDOR_COMMAND_WITH_ID:
            if ((opernum > 3)&&(opernum < 15))  rt = 1;
            break;
        case CEC_OC_VENDOR_REMOTE_BUTTON_DOWN:
            if (opernum < 15)  rt = 1;
            break;
        case CEC_OC_SELECT_DIGITAL_SERVICE:
            if (opernum == 7) rt = 1;
            break;
        case CEC_OC_SET_ANALOGUE_TIMER:
        case CEC_OC_CLEAR_ANALOGUE_TIMER:
            if (opernum == 11) rt = 1;
            break;
        case CEC_OC_SET_DIGITAL_TIMER:
        case CEC_OC_CLEAR_DIGITAL_TIMER:
            if (opernum == 14) rt = 1;
            break;
        case CEC_OC_TIMER_STATUS:
            if ((opernum == 1 || opernum == 3)) rt = 1;
            break;
        case CEC_OC_TUNER_DEVICE_STATUS:
            if ((opernum == 5 || opernum == 8)) rt = 1;
            break;
        case CEC_OC_RECORD_ON:
            if (opernum > 0 && opernum < 9)  rt = 1;
            break;
        case CEC_OC_CLEAR_EXTERNAL_TIMER:
        case CEC_OC_SET_EXTERNAL_TIMER:
            if ((opernum == 9 || opernum == 10)) rt = 1;
            break;
        case CEC_OC_SET_TIMER_PROGRAM_TITLE:
        case CEC_OC_SET_OSD_NAME:
            if (opernum > 0 && opernum < 15) rt = 1;
            break;
        case CEC_OC_SET_OSD_STRING:
            if (opernum > 1 && opernum < 15) rt = 1;
            break;
        case CEC_OC_VENDOR_COMMAND:
            if (opernum < 15)   rt = 1;
            break;
        default:
            rt = 1;
            break;
    }

    if ((rt == 0) & (opcode != 0)){
        hdmirx_cec_dbg_print("CEC: opcode & opernum not match: %x, %x\n", opcode, opernum);
    }
    
    //?????rt = 1; // temporal
    return rt;
}
static char *rx_status[] = {
    "RX_IDLE ",
    "RX_BUSY ",
    "RX_DONE ",
    "RX_ERROR",
};

//static char *tx_status[] = {
//    "TX_IDLE ",
//    "TX_BUSY ",
//    "TX_DONE ",
//    "TX_ERROR",
//};

static irqreturn_t cec_isr_handler(int irq, void *dev_instance)
{
    unsigned int data_msg_num, data_msg_stat;

    if (cec_pending_flag == TV_CEC_PENDING_ON) {
        WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR, 1 << 23);             // Clear the interrupt
        return IRQ_NONE;
    }
    data_msg_stat = hdmi_rd_reg(CEC0_BASE_ADDR+CEC_RX_MSG_STATUS);
    if (data_msg_stat) {
//        hdmirx_cec_dbg_print("CEC Irq Rx Status: %s\n", rx_status[data_msg_stat&3]);
        if ((data_msg_stat & 0x3) == RX_DONE) {
            data_msg_num = hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_NUM_MSG);
            if (data_msg_num == 1) {
                unsigned char rx_msg[MAX_MSG], rx_len;
                cec_ll_rx(rx_msg, &rx_len);
                                            //Num  Leng Head Op1   Op2   Op3
                hdmirx_cec_dbg_print("CEC: rx N:%lx L:%lx H:%lx O1:%lx O2:%lx O3:%lx\n", hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_NUM_MSG), 
                                                        hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_LENGTH),
                                                        hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_0_HEADER),
                                                        hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_1_OPCODE),
                                                        hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_2_OP1),
                                                        hdmi_rd_reg(CEC0_BASE_ADDR + CEC_RX_MSG_3_OP2)
                                                        );
                register_cec_rx_msg(rx_msg, rx_len);
            } else {
                hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_CLEAR_BUF,  0x01);
                hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_MSG_CMD,  RX_NO_OP);
                //cec_hw_reset();
                printk("CEC: recevie error[0x%x]\n", data_msg_num);
            }
        } else {
            hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_CLEAR_BUF,  0x01);
            hdmi_wr_reg(CEC0_BASE_ADDR + CEC_RX_MSG_CMD,  RX_NO_OP);
            //cec_hw_reset();
            printk("CEC: recevie error[%s]\n", rx_status[data_msg_stat&3]);
        }
    }

    data_msg_stat = hdmi_rd_reg(CEC0_BASE_ADDR+CEC_TX_MSG_STATUS);
    if (data_msg_stat) {
//        hdmirx_cec_dbg_print("CEC Irq Tx Status: %s\n", tx_status[data_msg_stat&3]);
    }
    cec_hw_reset();
    WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_STAT_CLR, 1 << 23);             // Clear the interrupt

    return IRQ_HANDLED;
}

static unsigned short cec_log_addr_to_dev_type(unsigned char log_addr)
{
    unsigned short us = CEC_UNREGISTERED_DEVICE_TYPE;
    if ((1 << log_addr) & CEC_DISPLAY_DEVICE) {
        us = CEC_DISPLAY_DEVICE_TYPE;
    } else if ((1 << log_addr) & CEC_RECORDING_DEVICE) {
        us = CEC_RECORDING_DEVICE_TYPE;
    } else if ((1 << log_addr) & CEC_PLAYBACK_DEVICE) {
        us = CEC_PLAYBACK_DEVICE_TYPE;
    } else if ((1 << log_addr) & CEC_TUNER_DEVICE) {
        us = CEC_TUNER_DEVICE_TYPE;
    } else if ((1 << log_addr) & CEC_AUDIO_SYSTEM_DEVICE) {
        us = CEC_AUDIO_SYSTEM_DEVICE_TYPE;
    }

    return us;
}
//
//static cec_hdmi_port_e cec_find_hdmi_port(unsigned char log_addr)
//{
//    cec_hdmi_port_e rt = CEC_HDMI_PORT_UKNOWN;
//
//    if ((cec_global_info.dev_mask & (1 << log_addr)) &&
//            (cec_global_info.cec_node_info[log_addr].phy_addr != 0) &&
//            (cec_global_info.cec_node_info[log_addr].hdmi_port == CEC_HDMI_PORT_UKNOWN)) {
//        if ((cec_global_info.cec_node_info[log_addr].phy_addr & 0xF000) == 0x1000) {
//            cec_global_info.cec_node_info[log_addr].hdmi_port = CEC_HDMI_PORT_1;
//        } else if ((cec_global_info.cec_node_info[log_addr].phy_addr & 0xF000) == 0x2000) {
//            cec_global_info.cec_node_info[log_addr].hdmi_port = CEC_HDMI_PORT_2;
//        } else if ((cec_global_info.cec_node_info[log_addr].phy_addr & 0xF000) == 0x3000) {
//            cec_global_info.cec_node_info[log_addr].hdmi_port = CEC_HDMI_PORT_3;
//        }
//    }
//
//    rt = cec_global_info.cec_node_info[log_addr].hdmi_port;
//
//    return rt;
//}

// -------------- command from cec devices ---------------------

void cec_polling_online_dev(int log_addr, int *bool)
{
    //int log_addr = 0;
    int r;
    unsigned short dev_mask_tmp = 0;
    unsigned char msg[1];
    unsigned char ping = 1;
    

    //for (log_addr = 1; log_addr < CEC_UNREGISTERED_ADDR; log_addr++) {
        msg[0] = (log_addr<<4) | log_addr;
        r = cec_ll_tx(msg, 1, &ping);
        if (r != TX_DONE) {
            dev_mask_tmp &= ~(1 << log_addr);
            memset(&(cec_global_info.cec_node_info[log_addr]), 0, sizeof(cec_node_info_t));
        	*bool = 0;
        }
        if (r == TX_DONE) {
            dev_mask_tmp |= 1 << log_addr;
            cec_global_info.cec_node_info[log_addr].log_addr = log_addr;
            cec_global_info.cec_node_info[log_addr].dev_type = cec_log_addr_to_dev_type(log_addr);
//            cec_find_hdmi_port(log_addr);
            *bool = 1;
        }
    //}
    printk("CEC: poll online logic device: 0x%x BOOL: %d\n", log_addr, *bool);

    if (cec_global_info.dev_mask != dev_mask_tmp) {
        cec_global_info.dev_mask = dev_mask_tmp;
    }

    //hdmirx_cec_dbg_print("cec log device exist: %x\n", dev_mask_tmp);
}

void cec_report_phy_addr(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    cec_global_info.dev_mask |= 1 << log_addr;
    cec_global_info.cec_node_info[log_addr].dev_type = cec_log_addr_to_dev_type(log_addr);
    cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_DEVICE_TYPE;
    memcpy(cec_global_info.cec_node_info[log_addr].osd_name_def, default_osd_name[log_addr], 16);
    if ((cec_global_info.cec_node_info[log_addr].real_info_mask & INFO_MASK_OSD_NAME) == 0) {
        memcpy(cec_global_info.cec_node_info[log_addr].osd_name, osd_name_uninit, 16);
    }
    cec_global_info.cec_node_info[log_addr].log_addr = log_addr;
    cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_LOGIC_ADDRESS;
    cec_global_info.cec_node_info[log_addr].phy_addr.phy_addr_4 = (pcec_message->content.msg.operands[0] << 8) | pcec_message->content.msg.operands[1];
    cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_PHYSICAL_ADDRESS;
//

}

void cec_give_physical_address(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    unsigned char index = cec_global_info.my_node_index;
    //if (cec_global_info.dev_mask & (1 << log_addr)) {
        unsigned char msg[5];
        msg[0] = (4 << (cec_global_info.cec_node_info[index].log_addr))| log_addr;
        msg[1] = CEC_OC_REPORT_PHYSICAL_ADDRESS;
        msg[2] = cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.ab;
        msg[3] = cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.cd;
        msg[4] = cec_global_info.cec_node_info[index].log_addr;
        cec_ll_tx(msg, 5, NULL);
    //}
//    hdmirx_cec_dbg_print("cec_report_phy_addr: %x\n", cec_global_info.cec_node_info[index].log_addr);
}

//***************************************************************
void cec_give_device_vendor_id(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    unsigned char index = cec_global_info.my_node_index;
    //if (cec_global_info.dev_mask & (1 << log_addr)) {
        unsigned char msg[5];
        msg[0] = (4 << (cec_global_info.cec_node_info[index].log_addr))| log_addr;
        msg[1] = CEC_OC_DEVICE_VENDOR_ID;
        msg[2] = (cec_global_info.cec_node_info[index].vendor_id >> 16) & 0xff;
        msg[3] = (cec_global_info.cec_node_info[index].vendor_id >> 8) & 0xff;
        msg[4] = (cec_global_info.cec_node_info[index].vendor_id >> 0) & 0xff;
        cec_ll_tx(msg, 5, NULL);
    //}
//    hdmirx_cec_dbg_print("%s: %x\n", cec_global_info.cec_node_info[index].log_addr);
}

////////////////////////////////////////////////
void cec_give_osd_name(cec_rx_message_t* pcec_message)
{

    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    unsigned char index = cec_global_info.my_node_index;
	unsigned char osd_len = strlen(cec_global_info.cec_node_info[index].osd_name);
    //if (cec_global_info.dev_mask & (1 << log_addr)) {
        unsigned char msg[16];
        msg[0] = (4 << (cec_global_info.cec_node_info[index].log_addr))| log_addr;
        msg[1] = CEC_OC_SET_OSD_NAME;
        memcpy(&msg[2], cec_global_info.cec_node_info[index].osd_name, osd_len);
//        msg[2] = (cec_global_info.cec_node_info[index].vendor_id >> 16) & 0xff;
//        msg[3] = (cec_global_info.cec_node_info[index].vendor_id >> 8) & 0xff;
//        msg[4] = (cec_global_info.cec_node_info[index].vendor_id >> 0) & 0xff;
        cec_ll_tx(msg, 2 + osd_len, NULL);
    //}
//    hdmirx_cec_dbg_print("%s: %x\n", cec_global_info.cec_node_info[index].log_addr);
}

//// TODO
void cec_standby(cec_rx_message_t* pcec_message)
{
    printk("CEC: System will be in standby mode\n");
}

void cec_report_power_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.cec_node_info[log_addr].power_status = pcec_message->content.msg.operands[0];
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_POWER_STATUS;
        hdmirx_cec_dbg_print("cec_report_power_status: %x\n", cec_global_info.cec_node_info[log_addr].power_status);
    }
}

void cec_feature_abort(cec_rx_message_t* pcec_message)
{
    hdmirx_cec_dbg_print("cec_feature_abort: opcode %x\n", pcec_message->content.msg.opcode);
}

void cec_report_version(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.cec_node_info[log_addr].cec_version = pcec_message->content.msg.operands[0];
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_CEC_VERSION;
        hdmirx_cec_dbg_print("cec_report_version: %x\n", cec_global_info.cec_node_info[log_addr].cec_version);
    }
}

// TODO
//////////////////////////////////
void cec_active_source(cec_rx_message_t* pcec_message)
{
    if(pcec_message->content.msg.header != cec_global_info.my_node_index)
        printk("CEC: set no hdmi output\n");
////    unsigned char log_addr = pcec_message->content.msg.header >> 4;
//    unsigned char index = cec_global_info.my_node_index;
//
//    //if (cec_global_info.dev_mask & (1 << log_addr)) {
//        unsigned char msg[4];
//        msg[0] = (4 << (cec_global_info.cec_node_info[index].log_addr))| CEC_BROADCAST_ADDR;
//        msg[1] = CEC_OC_ACTIVE_SOURCE;
//        msg[2] = cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.ab;
//        msg[3] = cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.cd;
//        cec_ll_tx(msg, 4, NULL);
//    //}
}

//////////////////////////////////
void cec_set_stream_path(cec_rx_message_t* pcec_message)
{
//    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    unsigned char index = cec_global_info.my_node_index;

    //if (cec_global_info.dev_mask & (1 << log_addr)) {
        unsigned char msg[4];
        msg[0] = (4 << (cec_global_info.cec_node_info[index].log_addr))| CEC_BROADCAST_ADDR;
        msg[1] = CEC_OC_ACTIVE_SOURCE;
        msg[2] = cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.ab;
        msg[3] = cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.cd;
        cec_ll_tx(msg, 4, NULL);
    //}
}

//
void cec_request_active_source(cec_rx_message_t* pcec_message)
{
    cec_set_stream_path(pcec_message);
}

void cec_give_device_power_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    unsigned char index = cec_global_info.my_node_index;

    //if (cec_global_info.dev_mask & (1 << log_addr)) {
        unsigned char msg[3];
        msg[0] = (4 << (cec_global_info.cec_node_info[index].log_addr))| log_addr;
        msg[1] = CEC_OC_REPORT_POWER_STATUS;
        msg[2] = cec_global_info.cec_node_info[index].power_status;
        cec_ll_tx(msg, 3, NULL);
    //}
}

void cec_deactive_source(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        if (cec_global_info.active_log_dev == log_addr) {
        cec_global_info.active_log_dev = 0;
        }
        hdmirx_cec_dbg_print("cec_deactive_source: %x\n", log_addr);
    }
}

void cec_get_version(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        unsigned char msg[3];
        msg[0] = log_addr;
        msg[1] = CEC_OC_CEC_VERSION;
        msg[2] = CEC_VERSION_13A;
        cec_ll_tx(msg, 3, NULL);
    }
}

void cec_give_deck_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {

    }
}

void cec_menu_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.cec_node_info[log_addr].menu_state = pcec_message->content.msg.operands[0];
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_MENU_STATE;
        hdmirx_cec_dbg_print("cec_menu_status: %x\n", cec_global_info.cec_node_info[log_addr].menu_state);
    }
}

void cec_deck_status(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        cec_global_info.cec_node_info[log_addr].specific_info.playback.deck_info = pcec_message->content.msg.operands[0];
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_DECK_INfO;
        hdmirx_cec_dbg_print("cec_deck_status: %x\n", cec_global_info.cec_node_info[log_addr].specific_info.playback.deck_info);
    }
}

void cec_device_vendor_id(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        int i, tmp = 0;
        for (i = 0; i < pcec_message->operand_num; i++) {
            tmp |= (pcec_message->content.msg.operands[i] << ((pcec_message->operand_num - i - 1)*8));
        }
//        cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id= tmp;
//        cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id_byte_num = pcec_message->operand_num;
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_VENDOR_ID;
//        hdmirx_cec_dbg_print("cec_device_vendor_id: %lx\n", cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id);
    }
}

void cec_set_osd_name(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        memcpy(cec_global_info.cec_node_info[log_addr].osd_name,  pcec_message->content.msg.operands, 14);
        cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_OSD_NAME;
        hdmirx_cec_dbg_print("cec_set_osd_name: %s\n", cec_global_info.cec_node_info[log_addr].osd_name);
    }
}

void cec_vendor_cmd_with_id(cec_rx_message_t* pcec_message)
{
//    unsigned char log_addr = pcec_message->content.msg.header >> 4;
//    if (cec_global_info.dev_mask & (1 << log_addr)) {
//        if (cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id_byte_num != 0) {
//            int i = cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id_byte_num;
//            int tmp = 0;
//            for ( ; i < pcec_message->operand_num; i++) {
//                tmp |= (pcec_message->content.msg.operands[i] << ((cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id_byte_num - i - 1)*8));
//            }
//            hdmirx_cec_dbg_print("cec_vendor_cmd_with_id: %lx, %x\n", cec_global_info.cec_node_info[log_addr].vendor_id.vendor_id, tmp);
//        }
//    }
}

void cec_set_menu_language(cec_rx_message_t* pcec_message)
{
    unsigned char log_addr = pcec_message->content.msg.header >> 4;
    if (cec_global_info.dev_mask & (1 << log_addr)) {
        if (pcec_message->operand_num == 3) {
            int i;
            unsigned int tmp = ((pcec_message->content.msg.operands[0] << 16)  |
                                (pcec_message->content.msg.operands[1] << 8) |
                                (pcec_message->content.msg.operands[2]));

            hdmirx_cec_dbg_print("%c, %c, %c\n", pcec_message->content.msg.operands[0],
                                 pcec_message->content.msg.operands[1],
                                 pcec_message->content.msg.operands[2]);

            for (i = 0; i < (sizeof(menu_lang_array)/sizeof(menu_lang_array[0])); i++) {
                if (menu_lang_array[i] == tmp)
                    break;
            }

            cec_global_info.cec_node_info[log_addr].menu_lang = i;

            cec_global_info.cec_node_info[log_addr].real_info_mask |= INFO_MASK_MENU_LANGUAGE;

            hdmirx_cec_dbg_print("cec_set_menu_language: %x\n", cec_global_info.cec_node_info[log_addr].menu_lang);
        }
    }
}

void cec_handle_message(cec_rx_message_t* pcec_message)
{
    unsigned char	brdcst, opcode;
    unsigned char	initiator, follower;
    unsigned char   operand_num;
    unsigned char   msg_length;

    /* parse message */
    if ((!pcec_message) || (check_cec_msg_valid(pcec_message) == 0)) return;

    initiator	= pcec_message->content.msg.header >> 4;
    follower	= pcec_message->content.msg.header & 0x0f;
    opcode		= pcec_message->content.msg.opcode;
    operand_num = pcec_message->operand_num;
    brdcst      = (follower == 0x0f);
    msg_length  = pcec_message->msg_length;

    /* process messages from tv polling and cec devices */

    switch (opcode) {
    case CEC_OC_ACTIVE_SOURCE:
        cec_active_source(pcec_message);
        break;
    case CEC_OC_INACTIVE_SOURCE:
        cec_deactive_source(pcec_message);
        break;
    case CEC_OC_CEC_VERSION:
        cec_report_version(pcec_message);
        break;
    case CEC_OC_DECK_STATUS:
        cec_deck_status(pcec_message);
        break;
    case CEC_OC_DEVICE_VENDOR_ID:
        cec_device_vendor_id(pcec_message);
        break;
    case CEC_OC_FEATURE_ABORT:
        cec_feature_abort(pcec_message);
        break;
    case CEC_OC_GET_CEC_VERSION:
        cec_get_version(pcec_message);
        break;
    case CEC_OC_GIVE_DECK_STATUS:
        cec_give_deck_status(pcec_message);
        break;
    case CEC_OC_MENU_STATUS:
        cec_menu_status(pcec_message);
        break;
    case CEC_OC_REPORT_PHYSICAL_ADDRESS:
        cec_report_phy_addr(pcec_message);
        break;
    case CEC_OC_REPORT_POWER_STATUS:
        cec_report_power_status(pcec_message);
        break;
    case CEC_OC_SET_OSD_NAME:
        cec_set_osd_name(pcec_message);
        break;
    case CEC_OC_VENDOR_COMMAND_WITH_ID:
        cec_vendor_cmd_with_id(pcec_message);
        break;
    case CEC_OC_SET_MENU_LANGUAGE:
        cec_set_menu_language(pcec_message);
        break;
    case CEC_OC_GIVE_PHYSICAL_ADDRESS:
        cec_give_physical_address(pcec_message);
        break;
    case CEC_OC_GIVE_DEVICE_VENDOR_ID:
        cec_give_device_vendor_id(pcec_message);
        break;
    case CEC_OC_GIVE_OSD_NAME:
        cec_give_osd_name(pcec_message);
        break;
    case CEC_OC_STANDBY:
        cec_standby(pcec_message);
        break;
    case CEC_OC_SET_STREAM_PATH:
        cec_set_stream_path(pcec_message);
        break;
    case CEC_OC_REQUEST_ACTIVE_SOURCE:
        cec_request_active_source(pcec_message);
        break;
    case CEC_OC_GIVE_DEVICE_POWER_STATUS:
        cec_give_device_power_status(pcec_message);
        break;
    case CEC_OC_VENDOR_REMOTE_BUTTON_DOWN:
    case CEC_OC_VENDOR_REMOTE_BUTTON_UP:
    case CEC_OC_CLEAR_ANALOGUE_TIMER:
    case CEC_OC_CLEAR_DIGITAL_TIMER:
    case CEC_OC_CLEAR_EXTERNAL_TIMER:
    case CEC_OC_DECK_CONTROL:
    case CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS:
    case CEC_OC_GIVE_TUNER_DEVICE_STATUS:
    case CEC_OC_IMAGE_VIEW_ON:      //not support in source
    case CEC_OC_MENU_REQUEST:
    case CEC_OC_SET_OSD_STRING:
    case CEC_OC_SET_SYSTEM_AUDIO_MODE:
    case CEC_OC_SET_TIMER_PROGRAM_TITLE:
    case CEC_OC_SYSTEM_AUDIO_MODE_REQUEST:
    case CEC_OC_SYSTEM_AUDIO_MODE_STATUS:
    case CEC_OC_TEXT_VIEW_ON:       //not support in source
    case CEC_OC_TIMER_CLEARED_STATUS:
    case CEC_OC_TIMER_STATUS:
    case CEC_OC_TUNER_DEVICE_STATUS:
    case CEC_OC_TUNER_STEP_DECREMENT:
    case CEC_OC_TUNER_STEP_INCREMENT:
    case CEC_OC_USER_CONTROL_PRESSED:
    case CEC_OC_USER_CONTROL_RELEASED:
    case CEC_OC_VENDOR_COMMAND:
    case CEC_OC_ROUTING_CHANGE:
    case CEC_OC_ROUTING_INFORMATION:
    case CEC_OC_SELECT_ANALOGUE_SERVICE:
    case CEC_OC_SELECT_DIGITAL_SERVICE:
    case CEC_OC_SET_ANALOGUE_TIMER :
    case CEC_OC_SET_AUDIO_RATE:
    case CEC_OC_SET_DIGITAL_TIMER:
    case CEC_OC_SET_EXTERNAL_TIMER:
    case CEC_OC_PLAY:
    case CEC_OC_RECORD_OFF:
    case CEC_OC_RECORD_ON:
    case CEC_OC_RECORD_STATUS:
    case CEC_OC_RECORD_TV_SCREEN:
    case CEC_OC_REPORT_AUDIO_STATUS:
    case CEC_OC_GET_MENU_LANGUAGE:
    case CEC_OC_GIVE_AUDIO_STATUS:
    case CEC_OC_ABORT_MESSAGE:
        printk("CEC: not support cmd: %x\n", opcode);
        break;
    default:
        break;
    }
}


// --------------- cec command from user application --------------------

void cec_usrcmd_parse_all_dev_online(void)
{
    int i;
    unsigned short tmp_mask;

    hdmirx_cec_dbg_print("cec online: ###############################################\n");
    hdmirx_cec_dbg_print("active_log_dev %x\n", cec_global_info.active_log_dev);
    for (i = 0; i < MAX_NUM_OF_DEV; i++) {
        tmp_mask = 1 << i;
        if (tmp_mask & cec_global_info.dev_mask) {
            hdmirx_cec_dbg_print("cec online: -------------------------------------------\n");
            hdmirx_cec_dbg_print("hdmi_port:     %x\n", cec_global_info.cec_node_info[i].hdmi_port);
            hdmirx_cec_dbg_print("dev_type:      %x\n", cec_global_info.cec_node_info[i].dev_type);
            hdmirx_cec_dbg_print("power_status:  %x\n", cec_global_info.cec_node_info[i].power_status);
            hdmirx_cec_dbg_print("cec_version:   %x\n", cec_global_info.cec_node_info[i].cec_version);
            hdmirx_cec_dbg_print("vendor_id:     %x\n", cec_global_info.cec_node_info[i].vendor_id);
            hdmirx_cec_dbg_print("phy_addr:      %x\n", cec_global_info.cec_node_info[i].phy_addr.phy_addr_4);
            hdmirx_cec_dbg_print("log_addr:      %x\n", cec_global_info.cec_node_info[i].log_addr);
            hdmirx_cec_dbg_print("osd_name:      %s\n", cec_global_info.cec_node_info[i].osd_name);
            hdmirx_cec_dbg_print("osd_name_def:  %s\n", cec_global_info.cec_node_info[i].osd_name_def);
            hdmirx_cec_dbg_print("menu_state:    %x\n", cec_global_info.cec_node_info[i].menu_state);

            if (cec_global_info.cec_node_info[i].dev_type == CEC_PLAYBACK_DEVICE_TYPE) {
                hdmirx_cec_dbg_print("deck_cnt_mode: %x\n", cec_global_info.cec_node_info[i].specific_info.playback.deck_cnt_mode);
                hdmirx_cec_dbg_print("deck_info:     %x\n", cec_global_info.cec_node_info[i].specific_info.playback.deck_info);
                hdmirx_cec_dbg_print("play_mode:     %x\n", cec_global_info.cec_node_info[i].specific_info.playback.play_mode);
            }
        }
    }
    hdmirx_cec_dbg_print("##############################################################\n");
}

//////////////////////////////////////////////////
void cec_usrcmd_get_cec_version(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, 
            CEC_OC_GET_CEC_VERSION);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_audio_status(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_AUDIO_STATUS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_deck_status(unsigned char log_addr)
{
    MSG_P1(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_DECK_STATUS, STATUS_REQ_ON);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_set_deck_cnt_mode(unsigned char log_addr, deck_cnt_mode_e deck_cnt_mode)
{
    MSG_P1(cec_global_info.my_node_index, log_addr, CEC_OC_DECK_CONTROL, deck_cnt_mode);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_get_device_power_status(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_DEVICE_POWER_STATUS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_device_vendor_id(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_DEVICE_VENDOR_ID);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_osd_name(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_OSD_NAME);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_physical_address(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_PHYSICAL_ADDRESS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_system_audio_mode_status(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_SYSTEM_AUDIO_MODE_STATUS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_standby(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_STANDBY);

    register_cec_tx_msg(gbl_msg, 2);
}

/////////////////////////
void cec_usrcmd_set_imageview_on(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, 
            CEC_OC_IMAGE_VIEW_ON);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_text_view_on(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, 
            CEC_OC_TEXT_VIEW_ON);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_get_tuner_device_status(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_GIVE_TUNER_DEVICE_STATUS);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_play_mode(unsigned char log_addr, play_mode_e play_mode)
{
    MSG_P1(cec_global_info.my_node_index, log_addr, CEC_OC_PLAY, play_mode);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_get_menu_state(unsigned char log_addr)
{
    MSG_P1(cec_global_info.my_node_index, log_addr, CEC_OC_MENU_REQUEST, MENU_REQ_QUERY);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_set_menu_state(unsigned char log_addr, menu_req_type_e menu_req_type)
{
    MSG_P1(cec_global_info.my_node_index, log_addr, CEC_OC_MENU_REQUEST, menu_req_type);

    register_cec_tx_msg(gbl_msg, 3);
}

void cec_usrcmd_get_menu_language(unsigned char log_addr)
{
    MSG_P0(cec_global_info.my_node_index, log_addr, CEC_OC_GET_MENU_LANGUAGE);

    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_menu_language(unsigned char log_addr, cec_menu_lang_e menu_lang)
{
    MSG_P3(cec_global_info.my_node_index, log_addr, CEC_OC_SET_MENU_LANGUAGE, (menu_lang_array[menu_lang]>>16)&0xFF,
           (menu_lang_array[menu_lang]>>8)&0xFF,
           (menu_lang_array[menu_lang])&0xFF);
    register_cec_tx_msg(gbl_msg, 5);
}

void cec_usrcmd_get_active_source(void)
{
    MSG_P0(cec_global_info.my_node_index, 0xF, CEC_OC_REQUEST_ACTIVE_SOURCE);
        
    register_cec_tx_msg(gbl_msg, 2);
}

void cec_usrcmd_set_active_source(void)
{
    unsigned char index = cec_global_info.my_node_index;
//	printk("CEC: %s Initor:%d Follower:%d Phy_Addr:%2x%2x\n",__func__, cec_global_info.my_node_index, log_addr, phy_addr_ab, phy_addr_cd);
	//printk("\n", log_addr, cec_global_info.cec_node_info[log_addr].phy_addr);
    MSG_P2(index, CEC_BROADCAST_ADDR, 
            CEC_OC_ACTIVE_SOURCE,
			cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.ab,
			cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.cd);

    register_cec_tx_msg(gbl_msg, 4);
}

void cec_usrcmd_set_deactive_source(unsigned char log_addr)
{
    MSG_P2(cec_global_info.my_node_index, log_addr, CEC_OC_INACTIVE_SOURCE, 
                                                  cec_global_info.cec_node_info[log_addr].phy_addr.phy_addr_2.ab,
                                                  cec_global_info.cec_node_info[log_addr].phy_addr.phy_addr_2.cd);

    register_cec_tx_msg(gbl_msg, 4);
}

void cec_usrcmd_clear_node_dev_real_info_mask(unsigned char log_addr, cec_info_mask mask)
{
    cec_global_info.cec_node_info[log_addr].real_info_mask &= ~mask;
}

//void cec_usrcmd_set_stream_path(unsigned char log_addr)
//{
//    MSG_P2(cec_global_info.my_node_index, log_addr, CEC_OC_SET_STREAM_PATH, 
//                                                  cec_global_info.cec_node_info[log_addr].phy_addr.phy_addr_2.ab,
//                                                  cec_global_info.cec_node_info[log_addr].phy_addr.phy_addr_2.cd);
//
//    register_cec_tx_msg(gbl_msg, 4);
//}

void cec_usrcmd_set_report_physical_address(void)
{
    unsigned char index = cec_global_info.my_node_index;

    MSG_P3(index, CEC_BROADCAST_ADDR, 
            CEC_OC_REPORT_PHYSICAL_ADDRESS, 
			cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.ab,
			cec_global_info.cec_node_info[index].phy_addr.phy_addr_2.cd,
			cec_global_info.cec_node_info[index].dev_type);

    register_cec_tx_msg(gbl_msg, 5);
}

/***************************** cec middle level code end *****************************/


/***************************** cec high level code *****************************/

void cec_init(hdmitx_dev_t* hdmitx_device)
{
    if(!hdmitx_device->cec_func_flag){
        printk("CEC not init\n");
        return ;
    }
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_H, 0x00 );
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_L, 0xf0 );


// ?????
//    if (cec_init_flag == 1) return;

    cec_rx_msg_buf.rx_write_pos = 0;
    cec_rx_msg_buf.rx_read_pos = 0;
    cec_rx_msg_buf.rx_buf_size = sizeof(cec_rx_msg_buf.cec_rx_message)/sizeof(cec_rx_msg_buf.cec_rx_message[0]);
    memset(cec_rx_msg_buf.cec_rx_message, 0, sizeof(cec_rx_msg_buf.cec_rx_message));

    memset(&cec_global_info, 0, sizeof(cec_global_info_t));
    //cec_global_info.my_node_index = CEC0_LOG_ADDR;

    if (cec_mutex_flag == 0) {
        init_MUTEX(&tv_cec_sema);
        cec_mutex_flag = 1;
    }
    
    kthread_run(cec_task, (void*)hdmitx_device, "kthread_cec");

    request_irq(INT_HDMI_CEC, &cec_isr_handler,
                IRQF_SHARED, "amhdmitx-cec",
                (void *)hdmitx_device);

    return;
}

void cec_uninit(hdmitx_dev_t* hdmitx_device)
{
    if(!hdmitx_device->cec_func_flag){
        return ;
    }

    if (cec_init_flag == 1) {
        WRITE_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK, READ_MPEG_REG(A9_0_IRQ_IN1_INTR_MASK) & ~(1 << 23));            // Disable the hdmi cec interrupt
        free_irq(INT_HDMI_CEC, (void *)hdmitx_device);

        del_timer_sync(&tv_cec_timer);

        cec_init_flag = 0;
    }
}

void cec_set_pending(tv_cec_pending_e on_off)
{
    cec_pending_flag = on_off;
}

size_t cec_usrcmd_get_global_info(char * buf)
{
    int i = 0;
    int dev_num = 0;

    cec_node_info_t * buf_node_addr = (cec_node_info_t *)(buf + (unsigned int)(((cec_global_info_to_usr_t*)0)->cec_node_info_online));

    for (i = 0; i < MAX_NUM_OF_DEV; i++) {
        if (cec_global_info.dev_mask & (1 << i)) {
            memcpy(&(buf_node_addr[dev_num]), &(cec_global_info.cec_node_info[i]), sizeof(cec_node_info_t));
            dev_num++;
        }
    }

    buf[0] = dev_num;
    buf[1] = cec_global_info.active_log_dev;
#if 0
    printk("\n");
    printk("%x\n",(unsigned int)(((cec_global_info_to_usr_t*)0)->cec_node_info_online));
    printk("%x\n", ((cec_global_info_to_usr_t*)buf)->dev_number);
    printk("%x\n", ((cec_global_info_to_usr_t*)buf)->active_log_dev);
    printk("%x\n", ((cec_global_info_to_usr_t*)buf)->cec_node_info_online[0].hdmi_port);
    for (i=0; i < (sizeof(cec_node_info_t) * dev_num) + 2; i++) {
        printk("%x,",buf[i]);
    }
    printk("\n");
#endif
    return (sizeof(cec_node_info_t) * dev_num) + (unsigned int)(((cec_global_info_to_usr_t*)0)->cec_node_info_online);
}

void cec_usrcmd_set_dispatch(const char * buf, size_t count)
{
    int i = 0;
    int j = 0;
    char param[16] = {0};

    if(count > 32){
        printk("CEC: too many args\n");
    }
    for(i = 0; i < count; i++){
        if ( (buf[i] >= '0') && (buf[i] <= 'f') ){
            param[j] = simple_strtoul(&buf[i], NULL, 16);
            j ++;
        }
        while ( buf[i] != ' ' )
            i ++;
    }

    hdmirx_cec_dbg_print("cec_usrcmd_set_dispatch: \n");

    switch (param[0]) {
    case GET_CEC_VERSION:   //0 LA
        cec_usrcmd_get_cec_version(param[1]);
        break;
    case GET_DEV_POWER_STATUS:
        cec_usrcmd_get_device_power_status(param[1]);
        break;
    case GET_DEV_VENDOR_ID:
        cec_usrcmd_get_device_vendor_id(param[1]);
        break;
    case GET_OSD_NAME:
        cec_usrcmd_get_osd_name(param[1]);
        break;
    case GET_PHYSICAL_ADDR:
        cec_usrcmd_get_physical_address(param[1]);
        break;
    case SET_STANDBY:       //d LA
        cec_usrcmd_set_standby(param[1]);
        break;
    case SET_IMAGEVIEW_ON:  //e LA
        cec_usrcmd_set_imageview_on(param[1]);
        break;
    case GIVE_DECK_STATUS:
        cec_usrcmd_get_deck_status(param[1]);
        break;
    case SET_DECK_CONTROL_MODE:
        cec_usrcmd_set_deck_cnt_mode(param[1], param[2]);
        break;
    case SET_PLAY_MODE:
        cec_usrcmd_set_play_mode(param[1], param[2]);
        break;
    case GET_SYSTEM_AUDIO_MODE:
        cec_usrcmd_get_system_audio_mode_status(param[1]);
        break;
    case GET_TUNER_DEV_STATUS:
        cec_usrcmd_get_tuner_device_status(param[1]);
        break;
    case GET_AUDIO_STATUS:
        cec_usrcmd_get_audio_status(param[1]);
        break;
    case GET_OSD_STRING:
        break;
    case GET_MENU_STATE:
        cec_usrcmd_get_menu_state(param[1]);
        break;
    case SET_MENU_STATE:
        cec_usrcmd_set_menu_state(param[1], param[2]);
        break;
    case SET_MENU_LANGAGE:
        cec_usrcmd_set_menu_language(param[1], param[2]);
        break;
    case GET_MENU_LANGUAGE:
        cec_usrcmd_get_menu_language(param[1]);
        break;
    case GET_ACTIVE_SOURCE:     //13???????
        cec_usrcmd_get_active_source();
        break;
    case SET_ACTIVE_SOURCE:
        cec_usrcmd_set_active_source();
        break;
    case SET_DEACTIVE_SOURCE:
        cec_usrcmd_set_deactive_source(param[1]);
        break;
//    case CLR_NODE_DEV_REAL_INFO_MASK:
//        cec_usrcmd_clear_node_dev_real_info_mask(param[1], (((cec_info_mask)param[2]) << 24) |
//                                                         (((cec_info_mask)param[3]) << 16) |
//                                                         (((cec_info_mask)param[4]) << 8)  |
//                                                         ((cec_info_mask)param[5]));
//        break;
    case REPORT_PHYSICAL_ADDRESS:    //17 
    	cec_usrcmd_set_report_physical_address();
    	break;
    case SET_TEXT_VIEW_ON:          //18 LA
    	cec_usrcmd_text_view_on(param[1]);
    default:
        break;
    }
}

/***************************** cec high level code end *****************************/

