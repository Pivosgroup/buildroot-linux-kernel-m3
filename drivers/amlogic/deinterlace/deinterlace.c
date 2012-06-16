/*
 * Amlogic M2
 * frame buffer driver-----------Deinterlace
 * Copyright (C) 2010 Amlogic, Inc.
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
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
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
#include <linux/proc_fs.h>
#include <linux/list.h>
//#include <linux/aml_common.h>
#include <asm/uaccess.h>
#include <mach/am_regs.h>

#include <linux/osd/osd_dev.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <linux/amports/canvas.h>
#include <linux/fiq_bridge.h>
#include "deinterlace.h"
#include "deinterlace_module.h"

#if defined(CONFIG_ARCH_MESON2)
#define FORCE_BOB_SUPPORT
#endif
//#define RUN_DI_PROCESS_IN_IRQ
//#define RUN_DI_PROCESS_IN_TIMER

#define FIQ_VSYNC
#define CHECK_VDIN_BUF_ERROR

#define DEVICE_NAME "deinterlace"

#ifdef DEBUG
#define pr_dbg(fmt, args...) printk(KERN_DEBUG "DI: " fmt, ## args)
#else
#define pr_dbg(fmt, args...)
#endif
#define pr_error(fmt, args...) printk(KERN_ERR "DI: " fmt, ## args)

#define Wr(adr, val) WRITE_MPEG_REG(adr, val)
#define Rd(adr) READ_MPEG_REG(adr)
extern void timestamp_pcrscr_enable(u32 enable);

static DEFINE_SPINLOCK(plist_lock);

static di_dev_t di_device;

static dev_t di_id;
static struct class *di_class;

#define INIT_FLAG_NOT_LOAD 0x80
static unsigned char boot_init_flag=0;
static int receiver_is_amvideo = 1;
static int buf_mgr_mode = 0;
static int buf_mgr_mode_mask = 0xf;
static int bypass_state = 0;
static int bypass_prog = 0;
static int bypass_hd = 0;
static int bypass_all = 0;
    /* prog_proc_config,
        bit[0]: only valid when buf_mgr_mode==0, when buf_mgr_mode==1, two field buffers are used for "prog vdin"
         0 "prog vdin" use two field buffers, 1 "prog vdin" use single frame buffer
        bit[2:1]: when two field buffers are used, 0 use vpp for blending ,
                                                   1 use post_di module for blending
                                                   2 debug mode, bob with top field
                                                   3 debug mode, bot with bot field
    */
static int prog_proc_config = (1<<1)|1; /*
                                            for source include both progressive and interlace pictures,
                                            always use post_di module for blending
                                         */
static int pulldown_detect = 0x10;
static int skip_wrong_field = 1;
static int frame_count = 0;
static int provider_vframe_level = 0;
static int frame_packing_mode = 1;
static int disp_frame_count = 0;
static int start_frame_drop_count = 0;
static int start_frame_hold_count = 0;
static int pre_de_irq_check = 0;
static int force_bob_flag = 0;

static uint init_flag=0;
static unsigned char active_flag=0;

#define VFM_NAME "deinterlace"

static long same_field_top_count = 0;
static long same_field_bot_count = 0;
static long same_w_r_canvas_count = 0;
static long pulldown_count = 0;
static long pulldown_buffer_mode = 1; /* bit 0:
                                        0, keep 3 buffers in pre_ready_list for checking;
                                        1, keep 4 buffers in pre_ready_list for checking;
                                     */
static long pulldown_win_mode = 0x11111;
static int same_field_source_flag_th = 60;
int nr_hfilt_en = 0;

#define real_buf_mgr_mode ((buf_mgr_mode|(get_blackout_policy()==0))&buf_mgr_mode_mask)

#define DisableVideoLayer() \
    do { CLEAR_MPEG_REG_MASK(VPP_MISC, \
         VPP_VD1_PREBLEND|VPP_VD2_PREBLEND|VPP_VD2_POSTBLEND|VPP_VD1_POSTBLEND ); \
    } while (0)

static int di_receiver_event_fun(int type, void* data, void* arg);
static void di_uninit_buf(void);
static unsigned char is_bypass(void);
static void log_buffer_state(unsigned char* tag);
u32 get_blackout_policy(void);

static const struct vframe_receiver_op_s di_vf_receiver =
{
    .event_cb = di_receiver_event_fun
};

static struct vframe_receiver_s di_vf_recv;

static vframe_t *di_vf_peek(void* arg);
static vframe_t *di_vf_get(void* arg);
static void di_vf_put(vframe_t *vf, void* arg);
static int di_event_cb(int type, void *data, void *private_data);
static void di_process(void);

static const struct vframe_operations_s deinterlace_vf_provider =
{
    .peek = di_vf_peek,
    .get  = di_vf_get,
    .put  = di_vf_put,
    .event_cb = di_event_cb,
};

static struct vframe_provider_s di_vf_prov;

int di_sema_init_flag = 0;
static struct semaphore  di_sema;

#ifdef FIQ_VSYNC
static bridge_item_t fiq_handle_item;
static irqreturn_t di_vf_put_isr(int irq, void *dev_id)
{
#ifdef RUN_DI_PROCESS_IN_IRQ
    int i;
    for(i=0; i<2; i++){
        if(active_flag){
            di_process();
        }
    }
#else
    up(&di_sema);
#endif
    return IRQ_HANDLED;
}
#endif

void trigger_pre_di_process(char idx)
{
    if(di_sema_init_flag == 0)
        return ;

    if(idx!='p')
    {
        log_buffer_state((idx=='i')?"irq":((idx=='p')?"put":((idx=='r')?"rdy":"oth")));
    }
#ifndef RUN_DI_PROCESS_IN_TIMER
#ifdef RUN_DI_PROCESS_IN_IRQ
    fiq_bridge_pulse_trigger(&fiq_handle_item);
#else
#ifdef FIQ_VSYNC
    if(idx == 'p'){
		    fiq_bridge_pulse_trigger(&fiq_handle_item);
    }
    else
#endif
    {
        up(&di_sema);
    }
#endif
#endif
}

#define DI_PRE_INTERVAL     (HZ/100)

static struct timer_list di_pre_timer;
static struct work_struct di_pre_work;

static void di_pre_timer_cb(unsigned long arg)
{
    struct timer_list *timer = (struct timer_list *)arg;

    schedule_work(&di_pre_work);

    timer->expires = jiffies + DI_PRE_INTERVAL;
    add_timer(timer);
}

/*****************************
*    di attr management :
*    enable
*    mode
*    reg
******************************/
/*config attr*/
static ssize_t show_config(struct device * dev, struct device_attribute *attr, char * buf)
{
    int pos=0;
    return pos;
}

static ssize_t store_config(struct device * dev, struct device_attribute *attr, const char * buf, size_t count);

static void dump_state(void);
static void force_source_change(void);

#define DI_RUN_FLAG_RUN             0
#define DI_RUN_FLAG_PAUSE           1
#define DI_RUN_FLAG_STEP            2
#define DI_RUN_FLAG_STEP_DONE       3

static int run_flag = DI_RUN_FLAG_RUN;

static ssize_t store_dbg(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    if(strncmp(buf, "state", 4)==0){
        dump_state();
    }
    else if(strncmp(buf,"bypass_prog", 11)==0){
        force_source_change();
        if(buf[11]=='1'){
            bypass_prog = 1;
        }
        else{
            bypass_prog = 0;
        }
    }
    else if(strncmp(buf, "prog_proc_config", 16)==0){
        if(buf[16]=='1'){
            prog_proc_config = 1;
        }
        else{
            prog_proc_config = 0;
        }
    }
    else if(strncmp(buf, "time_div", 8)==0){
    }
    else if(strncmp(buf, "show_osd", 8)==0){
        Wr(VIU_OSD1_CTRL_STAT, Rd(VIU_OSD1_CTRL_STAT)|(0xff<<12));
    }
    else if(strncmp(buf, "run", 3)==0){
        //timestamp_pcrscr_enable(1);
        run_flag = DI_RUN_FLAG_RUN;
    }
    else if(strncmp(buf, "pause", 5)==0){
        //timestamp_pcrscr_enable(0);
        run_flag = DI_RUN_FLAG_PAUSE;
    }
    else if(strncmp(buf, "step", 4)==0){
        run_flag = DI_RUN_FLAG_STEP;
    }
    else if(strncmp(buf, "dumptsync", 9) == 0){

    }
    return count;
}

static unsigned char* di_log_buf=NULL;
static unsigned int di_log_wr_pos=0;
static unsigned int di_log_rd_pos=0;
static unsigned int di_log_buf_size=0;
static unsigned int di_printk_flag=0;
unsigned int di_log_flag=0;
unsigned int buf_state_log_threshold = 16; 
unsigned int buf_state_log_start = 0; /*  set to 1 by condition of "post_ready count < buf_state_log_threshold",
																					reset to 0 by set buf_state_log_threshold as 0 */

static DEFINE_SPINLOCK(di_print_lock);

#define PRINT_TEMP_BUF_SIZE 128

int di_print_buf(char* buf, int len)
{
    unsigned long flags;
    int pos;
    int di_log_rd_pos_;
    if(di_log_buf_size==0)
        return 0;

    spin_lock_irqsave(&di_print_lock, flags);
    di_log_rd_pos_=di_log_rd_pos;
    if(di_log_wr_pos>=di_log_rd_pos)
        di_log_rd_pos_+=di_log_buf_size;

    for(pos=0;pos<len && di_log_wr_pos<(di_log_rd_pos_-1);pos++,di_log_wr_pos++){
        if(di_log_wr_pos>=di_log_buf_size)
            di_log_buf[di_log_wr_pos-di_log_buf_size]=buf[pos];
        else
            di_log_buf[di_log_wr_pos]=buf[pos];
    }
    if(di_log_wr_pos>=di_log_buf_size)
        di_log_wr_pos-=di_log_buf_size;
    spin_unlock_irqrestore(&di_print_lock, flags);
    return pos;

}

//static int log_seq = 0;
#if 0
#define di_print printk
#else
int di_print(const char *fmt, ...)
{
    va_list args;
    int avail = PRINT_TEMP_BUF_SIZE;
    char buf[PRINT_TEMP_BUF_SIZE];
    int pos,len=0;

    if(di_printk_flag){
        va_start(args, fmt);
        vprintk(fmt, args);
        va_end(args);
        return 0;
    }

    if(di_log_buf_size==0)
        return 0;

    //len += snprintf(buf+len, avail-len, "%d:",log_seq++);
    if(di_log_flag&DI_LOG_TIMESTAMP){
        len += snprintf(buf+len, avail-len, "%u:", (unsigned int)jiffies);
    }
    else if(di_log_flag&DI_LOG_PRECISE_TIMESTAMP){
        if(READ_MPEG_REG(ISA_TIMERB)==0){
            WRITE_MPEG_REG(ISA_TIMER_MUX, (READ_MPEG_REG(ISA_TIMER_MUX)&(~(3<<TIMER_B_INPUT_BIT)))
                |(TIMER_UNIT_100us<<TIMER_B_INPUT_BIT)|(1<<13)|(1<<17));
            WRITE_MPEG_REG(ISA_TIMERB, 0xffff);
            printk("Deinterlace: Init 100us TimerB\n");
        }
        len += snprintf(buf+len, avail-len, "%u:", (unsigned int)(0x10000-READ_MPEG_REG(ISA_TIMERB)));
    }
    va_start(args, fmt);
    len += vsnprintf(buf+len, avail-len, fmt, args);
    va_end(args);

    if ((avail-len) <= 0) {
        buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';
    }
    pos = di_print_buf(buf, len);
    //printk("di_print:%d %d\n", di_log_wr_pos, di_log_rd_pos);
	  return pos;
}
#endif

static ssize_t read_log(char * buf)
{
    unsigned long flags;
    ssize_t read_size=0;
    if(di_log_buf_size==0)
        return 0;
    //printk("show_log:%d %d\n", di_log_wr_pos, di_log_rd_pos);
    spin_lock_irqsave(&di_print_lock, flags);
    if(di_log_rd_pos<di_log_wr_pos){
        read_size = di_log_wr_pos-di_log_rd_pos;
    }
    else if(di_log_rd_pos>di_log_wr_pos){
        read_size = di_log_buf_size-di_log_rd_pos;
    }
    if(read_size>PAGE_SIZE)
        read_size=PAGE_SIZE;
    if(read_size>0)
        memcpy(buf, di_log_buf+di_log_rd_pos, read_size);

    di_log_rd_pos += read_size;
    if(di_log_rd_pos>=di_log_buf_size)
        di_log_rd_pos = 0;
    spin_unlock_irqrestore(&di_print_lock, flags);
    return read_size;
}

static ssize_t show_log(struct device * dev, struct device_attribute *attr, char * buf)
{
    return read_log(buf);
}

#ifdef DI_DEBUG
char log_tmp_buf[PAGE_SIZE];
static void dump_log(void)
{
    while(read_log(log_tmp_buf)>0){
        printk("%s", log_tmp_buf);    
    }    
}
#endif

static ssize_t store_log(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    int tmp;
    unsigned long flags;
    if(strncmp(buf, "bufsize", 7)==0){
        tmp = simple_strtoul(buf+7,NULL,10);
        spin_lock_irqsave(&di_print_lock, flags);
        if(di_log_buf){
            kfree(di_log_buf);
            di_log_buf=NULL;
            di_log_buf_size=0;
            di_log_rd_pos=0;
            di_log_wr_pos=0;
        }
        if(tmp>=1024){
            di_log_buf_size=0;
            di_log_rd_pos=0;
            di_log_wr_pos=0;
            di_log_buf=kmalloc(tmp, GFP_KERNEL);
            if(di_log_buf){
                di_log_buf_size=tmp;
            }
        }
        spin_unlock_irqrestore(&di_print_lock, flags);
        printk("di_store:set bufsize tmp %d %d\n",tmp, di_log_buf_size);
    }
    else if(strncmp(buf, "printk", 6)==0){
        di_printk_flag = simple_strtoul(buf+6, NULL, 10);
    }
    else{
        di_print(0, "%s", buf);
    }
    return 16;
}

static int set_noise_reduction_level(void)
{
    int nr_zone_0 = 4, nr_zone_1 = 8, nr_zone_2 = 12;
    //int nr_hfilt_en = 0;
    int nr_hfilt_mb_en = 0;
    /* int post_mb_en = 0;
    int blend_mtn_filt_en = 1;
    int blend_data_filt_en = 1;
    */
    unsigned int nr_strength = 0, nr_gain2 = 0, nr_gain1 = 0, nr_gain0 = 0;

    nr_strength = noise_reduction_level;
    if (nr_strength > 64)
        nr_strength = 64;
    nr_gain2 = 64 - nr_strength;
    nr_gain1 = nr_gain2 - ((nr_gain2 * nr_strength + 32) >> 6);
    nr_gain0 = nr_gain1 - ((nr_gain1 * nr_strength + 32) >> 6);
    nr_ctrl1 = (64 << 24) | (nr_gain2 << 16) | (nr_gain1 << 8) | (nr_gain0 << 0);

    nr_ctrl0 =     (1 << 31 ) |          									// nr yuv enable.
                       	(1 << 30 ) |          												// nr range. 3 point
                       	(0 << 29 ) |          												// max of 3 point.
                       	(nr_hfilt_en << 28 ) |          									// nr hfilter enable.
                       	(nr_hfilt_mb_en << 27 ) |          									// nr hfilter motion_blur enable.
                       	(nr_zone_2 <<16 ) |   												// zone 2
                       	(nr_zone_1 << 8 ) |    												// zone 1
                       	(nr_zone_0 << 0 ) ;   												// zone 0

//    blend_ctrl =     ( post_mb_en << 28 ) |      											// post motion blur enable.
//                              ( 0 << 27 ) |               													// mtn3p(l, c, r) max.
//                              ( 0 << 26 ) |               													// mtn3p(l, c, r) min.
//                              ( 0 << 25 ) |               													// mtn3p(l, c, r) ave.
//                              ( 1 << 24 ) |               													// mtntopbot max
//                              ( blend_mtn_filt_en  << 23 ) | 												// blend mtn filter enable.
//                              ( blend_data_filt_en << 22 ) | 												// blend data filter enable.
//                                kdeint0;                              												// kdeint.
    return 0;

}

typedef struct{
    char* name;
    uint* param;
    int (*proc_fun)(void);
}di_param_t;

unsigned long reg_mtn_info[7];

di_param_t di_params[]=
{

    {"ei_ctrl0",     &ei_ctrl0, NULL  },
    {"ei_ctrl1",     &ei_ctrl1, NULL   },
    {"ei_ctrl2",     &ei_ctrl2, NULL   },

    {"nr_ctrl0",     &nr_ctrl0, NULL   },
    {"nr_ctrl1",     &nr_ctrl1, NULL   },
    {"nr_ctrl2",     &nr_ctrl2, NULL   },
    {"nr_ctrl3",     &nr_ctrl3, NULL   },

    {"mtn_ctrl_char_diff_cnt",     &mtn_ctrl_char_diff_cnt, NULL   },
    {"mtn_ctrl_low_level",	   &mtn_ctrl_low_level, NULL   },
    {"mtn_ctrl_high_level",	   &mtn_ctrl_high_level, NULL   },
    {"mtn_ctrl", &mtn_ctrl, NULL},
    {"mtn_ctrl_diff_level",	   &mtn_ctrl_diff_level, NULL   },
    {"mtn_ctrl1_reduce",    &mtn_ctrl1_reduce, NULL  },
    {"mtn_ctrl1_shift",    &mtn_ctrl1_shift, NULL  },
    {"mtn_ctrl1", &mtn_ctrl1, NULL},
    {"blend_ctrl",   &blend_ctrl, NULL },
    {"kdeint0",       &kdeint0, NULL },
    {"kdeint1",       &kdeint1, NULL },
    {"kdeint2",       &kdeint2, NULL },
    {"blend_ctrl1",  &blend_ctrl1, NULL },
    {"blend_ctrl1_char_level", &blend_ctrl1_char_level, NULL},
    {"blend_ctrl1_angle_thd", &blend_ctrl1_angle_thd, NULL},
    {"blend_ctrl1_filt_thd", &blend_ctrl1_filt_thd, NULL},
    {"blend_ctrl1_diff_thd", &blend_ctrl1_diff_thd, NULL},
    {"blend_ctrl2",  &blend_ctrl2, NULL },
    {"blend_ctrl2_black_level", &blend_ctrl2_black_level, NULL},
    {"blend_ctrl2_mtn_no_mov", &blend_ctrl2_mtn_no_mov, NULL},
    {"mtn_thre_1_low",&mtn_thre_1_low,NULL},
    {"mtn_thre_1_high",&mtn_thre_1_high,NULL},
    {"mtn_thre_2_low",&mtn_thre_2_low,NULL},
    {"mtn_thre_2_high",&mtn_thre_2_high,NULL},
    {"mtn_info0",((uint*)&reg_mtn_info[0]) ,NULL},
    {"mtn_info1",((uint*)&reg_mtn_info[1]) ,NULL},
    {"mtn_info2",((uint*)&reg_mtn_info[2]) ,NULL},
    {"mtn_info3",((uint*)&reg_mtn_info[3]) ,NULL},
    {"mtn_info4",((uint*)&reg_mtn_info[4]) ,NULL},
    {"mtn_info5",((uint*)&reg_mtn_info[5]) ,NULL},
 	{"mtn_info6",((uint*)&reg_mtn_info[6]) ,NULL},


    {"post_ctrl__di_blend_en",  &post_ctrl__di_blend_en, NULL},
    {"post_ctrl__di_post_repeat",  &post_ctrl__di_post_repeat, NULL},
    {"di_pre_ctrl__di_pre_repeat",  &di_pre_ctrl__di_pre_repeat, NULL},

    {"noise_reduction_level", &noise_reduction_level, set_noise_reduction_level},

    {"field_32lvl", &field_32lvl, NULL},
    {"field_22lvl", &field_22lvl, NULL},
    {"field_pd_frame_diff_chg_th",          &(field_pd_th.frame_diff_chg_th), NULL},
    {"field_pd_frame_diff_num_chg_th",      &(field_pd_th.frame_diff_num_chg_th), NULL},
    {"field_pd_field_diff_chg_th",          &(field_pd_th.field_diff_chg_th), NULL},
    {"field_pd_field_diff_num_chg_th",      &(field_pd_th.field_diff_num_chg_th), NULL},
    {"field_pd_frame_diff_skew_th",     &(field_pd_th.frame_diff_skew_th), NULL},
    {"field_pd_frame_diff_num_skew_th", &(field_pd_th.frame_diff_num_skew_th), NULL},

    {"win0_start_x_r", &pd_win_prop[0].win_start_x_r, NULL},
    {"win0_end_x_r", &pd_win_prop[0].win_end_x_r, NULL},
    {"win0_start_y_r", &pd_win_prop[0].win_start_y_r, NULL},
    {"win0_end_y_r", &pd_win_prop[0].win_end_y_r, NULL},
    {"win0_32lvl", &pd_win_prop[0].win_32lvl, NULL},
    {"win0_22lvl", &pd_win_prop[0].win_22lvl, NULL},
    {"win0_pd_frame_diff_chg_th",           &(win_pd_th[0].frame_diff_chg_th), NULL},
    {"win0_pd_frame_diff_num_chg_th",       &(win_pd_th[0].frame_diff_num_chg_th), NULL},
    {"win0_pd_field_diff_chg_th",           &(win_pd_th[0].field_diff_chg_th), NULL},
    {"win0_pd_field_diff_num_chg_th",       &(win_pd_th[0].field_diff_num_chg_th), NULL},
    {"win0_pd_frame_diff_skew_th",      &(win_pd_th[0].frame_diff_skew_th), NULL},
    {"win0_pd_frame_diff_num_skew_th",  &(win_pd_th[0].frame_diff_num_skew_th), NULL},
    {"win0_pd_field_diff_num_th",  &(win_pd_th[0].field_diff_num_th), NULL},

    {"win1_start_x_r", &pd_win_prop[1].win_start_x_r, NULL},
    {"win1_end_x_r",   &pd_win_prop[1].win_end_x_r, NULL},
    {"win1_start_y_r", &pd_win_prop[1].win_start_y_r, NULL},
    {"win1_end_y_r",   &pd_win_prop[1].win_end_y_r, NULL},
    {"win1_32lvl", &pd_win_prop[1].win_32lvl, NULL},
    {"win1_22lvl", &pd_win_prop[1].win_22lvl, NULL},
    {"win1_pd_frame_diff_chg_th",           &(win_pd_th[1].frame_diff_chg_th), NULL},
    {"win1_pd_frame_diff_num_chg_th",       &(win_pd_th[1].frame_diff_num_chg_th), NULL},
    {"win1_pd_field_diff_chg_th",           &(win_pd_th[1].field_diff_chg_th), NULL},
    {"win1_pd_field_diff_num_chg_th",       &(win_pd_th[1].field_diff_num_chg_th), NULL},
    {"win1_pd_frame_diff_skew_th",      &(win_pd_th[1].frame_diff_skew_th), NULL},
    {"win1_pd_frame_diff_num_skew_th",  &(win_pd_th[1].frame_diff_num_skew_th), NULL},
    {"win1_pd_field_diff_num_th",  &(win_pd_th[1].field_diff_num_th), NULL},

    {"win2_start_x_r", &pd_win_prop[2].win_start_x_r, NULL},
    {"win2_end_x_r",   &pd_win_prop[2].win_end_x_r, NULL},
    {"win2_start_y_r", &pd_win_prop[2].win_start_y_r, NULL},
    {"win2_end_y_r",   &pd_win_prop[2].win_end_y_r, NULL},
    {"win2_32lvl", &pd_win_prop[2].win_32lvl, NULL},
    {"win2_22lvl", &pd_win_prop[2].win_22lvl, NULL},
    {"win2_pd_frame_diff_chg_th",           &(win_pd_th[2].frame_diff_chg_th), NULL},
    {"win2_pd_frame_diff_num_chg_th",       &(win_pd_th[2].frame_diff_num_chg_th), NULL},
    {"win2_pd_field_diff_chg_th",           &(win_pd_th[2].field_diff_chg_th), NULL},
    {"win2_pd_field_diff_num_chg_th",       &(win_pd_th[2].field_diff_num_chg_th), NULL},
    {"win2_pd_frame_diff_skew_th",      &(win_pd_th[2].frame_diff_skew_th), NULL},
    {"win2_pd_frame_diff_num_skew_th",  &(win_pd_th[2].frame_diff_num_skew_th), NULL},
    {"win2_pd_field_diff_num_th",  &(win_pd_th[2].field_diff_num_th), NULL},

    {"win3_start_x_r", &pd_win_prop[3].win_start_x_r, NULL},
    {"win3_end_x_r",   &pd_win_prop[3].win_end_x_r, NULL},
    {"win3_start_y_r", &pd_win_prop[3].win_start_y_r, NULL},
    {"win3_end_y_r",   &pd_win_prop[3].win_end_y_r, NULL},
    {"win3_32lvl", &pd_win_prop[3].win_32lvl, NULL},
    {"win3_22lvl", &pd_win_prop[3].win_22lvl, NULL},
    {"win3_pd_frame_diff_chg_th",           &(win_pd_th[3].frame_diff_chg_th), NULL},
    {"win3_pd_frame_diff_num_chg_th",       &(win_pd_th[3].frame_diff_num_chg_th), NULL},
    {"win3_pd_field_diff_chg_th",           &(win_pd_th[3].field_diff_chg_th), NULL},
    {"win3_pd_field_diff_num_chg_th",       &(win_pd_th[3].field_diff_num_chg_th), NULL},
    {"win3_pd_frame_diff_skew_th",      &(win_pd_th[3].frame_diff_skew_th), NULL},
    {"win3_pd_frame_diff_num_skew_th",  &(win_pd_th[3].frame_diff_num_skew_th), NULL},
    {"win3_pd_field_diff_num_th",  &(win_pd_th[3].field_diff_num_th), NULL},

    {"win4_start_x_r", &pd_win_prop[4].win_start_x_r, NULL},
    {"win4_end_x_r",   &pd_win_prop[4].win_end_x_r, NULL},
    {"win4_start_y_r", &pd_win_prop[4].win_start_y_r, NULL},
    {"win4_end_y_r",   &pd_win_prop[4].win_end_y_r, NULL},
    {"win4_32lvl", &pd_win_prop[4].win_32lvl, NULL},
    {"win4_22lvl", &pd_win_prop[4].win_22lvl, NULL},
    {"win4_pd_frame_diff_chg_th",           &(win_pd_th[4].frame_diff_chg_th), NULL},
    {"win4_pd_frame_diff_num_chg_th",       &(win_pd_th[4].frame_diff_num_chg_th), NULL},
    {"win4_pd_field_diff_chg_th",           &(win_pd_th[4].field_diff_chg_th), NULL},
    {"win4_pd_field_diff_num_chg_th",       &(win_pd_th[4].field_diff_num_chg_th), NULL},
    {"win4_pd_frame_diff_skew_th",      &(win_pd_th[4].frame_diff_skew_th), NULL},
    {"win4_pd_frame_diff_num_skew_th",  &(win_pd_th[4].frame_diff_num_skew_th), NULL},
    {"win4_pd_field_diff_num_th",  &(win_pd_th[4].field_diff_num_th), NULL},

    {"pd32_match_num",  &pd32_match_num, NULL},
    {"pd32_diff_num_0_th",  &pd32_diff_num_0_th, NULL},
    {"pd32_debug_th", &pd32_debug_th, NULL},
    {"pd22_th", &pd22_th, NULL},
    {"pd22_num_th", &pd22_num_th, NULL},

    {"", NULL}
};

static ssize_t show_parameters(struct device * dev, struct device_attribute *attr, char * buf)
{
    int i = 0;
    int len = 0;
    for(i=0; di_params[i].param; i++){
        len += sprintf(buf+len, "%s=%08x\n", di_params[i].name, *(di_params[i].param));
    }
    return len;
}

static char tmpbuf[128];
static ssize_t store_parameters(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    int i = 0;
    int j;
    while((buf[i])&&(buf[i]!='=')&&(buf[i]!=' ')){
        tmpbuf[i]=buf[i];
        i++;
    }
    tmpbuf[i]=0;
    //printk("%s\n", tmpbuf);
    if(strcmp("reset", tmpbuf)==0){
        reset_di_para();
        di_print("reset param\n");
    }
    else{
        for(j=0; di_params[j].param; j++){
            if(strcmp(di_params[j].name, tmpbuf)==0){
                int value=simple_strtoul(buf+i+1, NULL, 16);
                *(di_params[j].param) = value;
                if(di_params[j].proc_fun){
                    di_params[j].proc_fun();
                }
                di_print("set %s=%x\n", di_params[j].name, value);
                break;
            }
        }
    }
    return count;
}

typedef struct{
    char* name;
    uint* status;
}di_status_t;

di_status_t di_status[]=
{
    {"active",     &init_flag  },
    {"", NULL}
};

static ssize_t show_status(struct device * dev, struct device_attribute *attr, char * buf)
{
    int i = 0;
    int len = 0;
    di_print("%s\n", __func__);
    for(i=0; di_status[i].status; i++){
        len += sprintf(buf+len, "%s=%08x\n", di_status[i].name, *(di_status[i].status));
    }
    return len;
}

static DEVICE_ATTR(config, S_IWUGO | S_IRUGO, show_config, store_config);
static DEVICE_ATTR(parameters, S_IWUGO | S_IRUGO, show_parameters, store_parameters);
static DEVICE_ATTR(debug, S_IWUGO | S_IRUGO, NULL, store_dbg);
static DEVICE_ATTR(log, S_IWUGO | S_IRUGO, show_log, store_log);
static DEVICE_ATTR(status, S_IWUGO | S_IRUGO, show_status, NULL);
/***************************
* di buffer management
***************************/
#if defined(CONFIG_ARCH_MESON2)
#define MAX_IN_BUF_NUM        16
#define MAX_LOCAL_BUF_NUM     12
#define MAX_POST_BUF_NUM      16
#else
#define MAX_IN_BUF_NUM        8
#define MAX_LOCAL_BUF_NUM     6
#define MAX_POST_BUF_NUM      6
#endif

#define VFRAME_TYPE_IN          1
#define VFRAME_TYPE_LOCAL       2
#define VFRAME_TYPE_POST        3
#ifdef DI_DEBUG
static char* vframe_type_name[] = {"", "di_buf_in", "di_buf_loc", "di_buf_post"};
#endif
typedef struct di_buf_s{
    struct list_head list;
    vframe_t* vframe;
    int index; /* index in vframe_in_dup[] or vframe_in[], only for type of VFRAME_TYPE_IN */
    int post_proc_flag; /* 0,no post di; 1, normal post di; 2, edge only; 3, dummy */
    int new_format_flag;
    int type;
    int seq;
    int pre_ref_count; /* none zero, is used by mem_mif, chan2_mif, or wr_buf*/
    int post_ref_count; /* none zero, is used by post process */
    /*below for type of VFRAME_TYPE_LOCAL */
    unsigned int nr_adr;
    int nr_canvas_idx;
    unsigned int mtn_adr;
    int mtn_canvas_idx;
    /* pull down information */
    pulldown_detect_info_t field_pd_info;
    pulldown_detect_info_t win_pd_info[MAX_WIN_NUM];

	  unsigned long mtn_info[5];
    int pulldown_mode;
    int win_pd_mode[5];

    /*below for type of VFRAME_TYPE_POST*/
    struct di_buf_s* di_buf[2];
    struct di_buf_s* di_buf_dup_p[5]; /* 0~4: n-2, n-1, n, n+1, n+2 ; n is the field to display*/
}di_buf_t;

static unsigned long di_mem_start;
static unsigned long di_mem_size;
static unsigned int default_width = 1920;
static unsigned int default_height = 1080;
static int local_buf_num;

    /*
        progressive frame process type config:
        0, process by field;
        1, process by frame (only valid for vdin source whose width/height does not change)
    */
static vframe_t* vframe_in[MAX_IN_BUF_NUM];
static vframe_t vframe_in_dup[MAX_IN_BUF_NUM];
static vframe_t vframe_local[MAX_LOCAL_BUF_NUM*2];
static vframe_t vframe_post[MAX_POST_BUF_NUM];
static di_buf_t* cur_post_ready_di_buf = NULL;

static di_buf_t di_buf_local[MAX_LOCAL_BUF_NUM*2];
static di_buf_t di_buf_in[MAX_IN_BUF_NUM];
static di_buf_t di_buf_post[MAX_POST_BUF_NUM];

/*
all buffers are in
1) list of local_free_list,in_free_list,pre_ready_list,recycle_list
2) di_pre_stru.di_inp_buf
3) di_pre_stru.di_wr_buf
4) cur_post_ready_di_buf
5) (di_buf_t*)(vframe->private_data)->di_buf[]

6) post_free_list_head
8) (di_buf_t*)(vframe->private_data)
*/
static struct list_head local_free_list_head = LIST_HEAD_INIT(local_free_list_head); // vframe is local_vframe
static struct list_head in_free_list_head = LIST_HEAD_INIT(in_free_list_head); //vframe is NULL
static struct list_head pre_ready_list_head = LIST_HEAD_INIT(pre_ready_list_head); //vframe is either local_vframe or in_vframe
static struct list_head recycle_list_head = LIST_HEAD_INIT(recycle_list_head); //vframe is either local_vframe or in_vframe

static struct list_head post_free_list_head = LIST_HEAD_INIT(post_free_list_head); //vframe is post_vframe
static struct list_head post_ready_list_head = LIST_HEAD_INIT(post_ready_list_head); //vframe is post_vframe

static struct list_head display_list_head = LIST_HEAD_INIT(display_list_head); //vframe sent out for displaying

typedef struct{
    /* pre input */
    DI_MIF_t di_inp_mif;
    DI_MIF_t di_mem_mif;
    DI_MIF_t di_chan2_mif;
    di_buf_t* di_inp_buf;
    di_buf_t* di_mem_buf_dup_p;
    di_buf_t* di_chan2_buf_dup_p;
    /* pre output */
    DI_SIM_MIF_t di_nrwr_mif;
    DI_SIM_MIF_t di_mtnwr_mif;
    di_buf_t* di_wr_buf;
    /* pre state */
    int in_seq;
    int recycle_seq;
    int pre_ready_seq;

    int pre_de_busy; /* 1 if pre_de is not done */
    int pre_de_busy_timer_count;
    int pre_de_process_done; /* flag when irq done */
    int unreg_req_flag; /* flag is set when VFRAME_EVENT_PROVIDER_UNREG*/
    int force_unreg_req_flag;
    int disable_req_flag;
        /* current source info */
    int cur_width;
    int cur_height;
    int cur_inp_type;
    int cur_prog_flag; /* 1 for progressive source */
    int process_count; /* valid only when prog_proc_type is 0, for progressive source: top field 1, bot field 0 */
    int source_change_flag;

    unsigned char prog_proc_type; /* set by prog_proc_config when source is vdin*/
    unsigned char enable_mtnwr;
    unsigned char enable_pulldown_check;

    int same_field_source_flag;

}di_pre_stru_t;
static di_pre_stru_t di_pre_stru;

static void dump_di_pre_stru(void)
{
    printk("di_pre_stru:\n");
    printk("di_mem_buf_dup_p       = 0x%p\n", di_pre_stru.di_mem_buf_dup_p);
    printk("di_chan2_buf_dup_p     = 0x%p\n", di_pre_stru.di_chan2_buf_dup_p);
    printk("in_seq                 = %d\n", di_pre_stru.in_seq);
    printk("recycle_seq            = %d\n", di_pre_stru.recycle_seq);
    printk("pre_ready_seq          = %d\n", di_pre_stru.pre_ready_seq);
    printk("pre_de_busy            = %d\n", di_pre_stru.pre_de_busy);
    printk("pre_de_busy_timer_count= %d\n", di_pre_stru.pre_de_busy_timer_count);
    printk("pre_de_process_done    = %d\n", di_pre_stru.pre_de_process_done);
    printk("unreg_req_flag         = %d\n", di_pre_stru.unreg_req_flag);
    printk("cur_width              = %d\n", di_pre_stru.cur_width);
    printk("cur_height             = %d\n", di_pre_stru.cur_height);
    printk("cur_inp_type           = 0x%x\n", di_pre_stru.cur_inp_type);
    printk("cur_prog_flag          = %d\n", di_pre_stru.cur_prog_flag);
    printk("process_count          = %d\n", di_pre_stru.process_count);
    printk("source_change_flag     = %d\n", di_pre_stru.source_change_flag);
    printk("prog_proc_type         = %d\n", di_pre_stru.prog_proc_type);
    printk("enable_mtnwr           = %d\n", di_pre_stru.enable_mtnwr);
    printk("enable_pulldown_check  = %d\n", di_pre_stru.enable_pulldown_check);
    printk("same_field_source_flag = %d\n", di_pre_stru.same_field_source_flag);
}

typedef struct{
    DI_MIF_t di_buf0_mif;
    DI_MIF_t di_buf1_mif;
    DI_SIM_MIF_t di_mtncrd_mif;
    DI_SIM_MIF_t di_mtnprd_mif;
}di_post_stru_t;
static di_post_stru_t di_post_stru;

#define is_from_vdin(vframe) (vframe->type & VIDTYPE_VIU_422)

static void recycle_vframe_type_post(di_buf_t* di_buf);
#ifdef DI_DEBUG
static void recycle_vframe_type_post_print(di_buf_t* di_buf, const char* func);
#endif

static ssize_t store_config(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    if(strncmp(buf, "disable", 7)==0){
#ifdef DI_DEBUG
        di_print("%s: disable\n", __func__);
#endif
        if(init_flag){
            di_pre_stru.disable_req_flag = 1;
            provider_vframe_level = 0;
            trigger_pre_di_process('d');
            while(di_pre_stru.disable_req_flag){
                msleep(1);    
            }
        }
    }
    return count;
}

static int list_count(struct list_head* list_head, int type)
{
    int count = 0;
    di_buf_t *p = NULL, *ptmp;
    list_for_each_entry_safe(p, ptmp, list_head, list) {
        if((type==0)||(p->type == type)){
            count++;
        }
    }
    return count;
}

static unsigned char is_progressive(vframe_t* vframe)
{
    return ((vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE);
}

static void force_source_change(void)
{
    di_pre_stru.cur_inp_type = 0;
}

static unsigned char is_source_change(vframe_t* vframe)
{
#define VFRAME_FORMAT_MASK  (VIDTYPE_VIU_422|VIDTYPE_VIU_FIELD|VIDTYPE_VIU_SINGLE_PLANE|VIDTYPE_VIU_444)
    if(
        (di_pre_stru.cur_width!=vframe->width)||
        (di_pre_stru.cur_height!=vframe->height)||
        (di_pre_stru.cur_prog_flag!=is_progressive(vframe))||
        ((di_pre_stru.cur_inp_type&VFRAME_FORMAT_MASK)!=(vframe->type&VFRAME_FORMAT_MASK))
        ){
        return 1;
    }
    return 0;
}

static unsigned char is_bypass(void)
{
    if(bypass_all)
        return 1;
    if(di_pre_stru.cur_prog_flag&&
        ((bypass_prog)||
        (di_pre_stru.cur_width>1920)||(di_pre_stru.cur_height>1080)
        ||(di_pre_stru.cur_inp_type&VIDTYPE_VIU_444))
        )
        return 1;
    if(bypass_hd&&
        ((di_pre_stru.cur_width>720)||(di_pre_stru.cur_height>576))
      )
      return 1;

    return 0;

}

#if defined(CONFIG_ARCH_MESON2)
#if ((DEINTERLACE_CANVAS_BASE_INDEX!=0x68)||(DEINTERLACE_CANVAS_MAX_INDEX!=0x7f))
#error "DEINTERLACE_CANVAS_BASE_INDEX is not 0x68 or DEINTERLACE_CANVAS_MAX_INDEX is not 0x7f, update canvas.h"
#endif
#endif
static int di_init_buf(int width, int height, unsigned char prog_flag)
{
    int i, local_buf_num_available;
    int canvas_height = height + 8;
    frame_count = 0;
    disp_frame_count = 0;
    cur_post_ready_di_buf = NULL;
    for(i=0; i<MAX_IN_BUF_NUM; i++){
			vframe_in[i]=NULL;
		}
    memset(&di_pre_stru, 0, sizeof(di_pre_stru));
    memset(&di_post_stru, 0, sizeof(di_post_stru));
    if(prog_flag){
        di_pre_stru.prog_proc_type = 1;
        local_buf_num = di_mem_size/(width*canvas_height*2);
        local_buf_num_available = local_buf_num;
        if(local_buf_num > (2*MAX_LOCAL_BUF_NUM)){
            local_buf_num = 2*MAX_LOCAL_BUF_NUM;
        }
    }
    else{
        di_pre_stru.prog_proc_type = 0;
        local_buf_num = di_mem_size/(width*canvas_height*5/4);
        local_buf_num_available = local_buf_num;
        if(local_buf_num > MAX_LOCAL_BUF_NUM){
            local_buf_num = MAX_LOCAL_BUF_NUM;
        }
    }
    printk("%s:prog_proc_type %d, buf start %x, size %x, buf(w%d,h%d) num %d (available %d) \n",
        __func__, di_pre_stru.prog_proc_type, (unsigned int)di_mem_start,
        (unsigned int)di_mem_size, width, canvas_height, local_buf_num, local_buf_num_available);
    same_w_r_canvas_count = 0;
    same_field_top_count = 0;
    same_field_bot_count = 0;

    for(i=0; i<local_buf_num; i++){
        di_buf_t* di_buf = &(di_buf_local[i]);
        if(di_buf){
            memset(di_buf, sizeof(di_buf_t), 0);
            di_buf->type = VFRAME_TYPE_LOCAL;
            di_buf->pre_ref_count = 0;
            di_buf->post_ref_count = 0;
            if(prog_flag){
                di_buf->nr_adr = di_mem_start + (width*canvas_height*2)*i;
    	          di_buf->nr_canvas_idx = DEINTERLACE_CANVAS_BASE_INDEX+i;
	              canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr, width*2, canvas_height, 0, 0);
            }
            else{
                di_buf->nr_adr = di_mem_start + (width*canvas_height*5/4)*i;
    	          di_buf->nr_canvas_idx = DEINTERLACE_CANVAS_BASE_INDEX+i*2;
	              canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr, width*2, canvas_height/2, 0, 0);
                di_buf->mtn_adr = di_mem_start + (width*canvas_height*5/4)*i + (width*canvas_height);
    	          di_buf->mtn_canvas_idx = DEINTERLACE_CANVAS_BASE_INDEX+i*2+1;
	              canvas_config(di_buf->mtn_canvas_idx, di_buf->mtn_adr, width/2, canvas_height/2, 0, 0);
            }
            di_buf->index = i;
            di_buf->vframe = &(vframe_local[i]);
            di_buf->vframe->private_data = di_buf;
            di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
            di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
            list_add_tail(&(di_buf->list), &local_free_list_head);
        }
    }

    for(i=0; i<MAX_IN_BUF_NUM; i++){
        di_buf_t* di_buf = &(di_buf_in[i]);
        if(di_buf){
            memset(di_buf, sizeof(di_buf_t), 0);
            di_buf->type = VFRAME_TYPE_IN;
            di_buf->pre_ref_count = 0;
            di_buf->post_ref_count = 0;
            di_buf->vframe = &(vframe_in_dup[i]);
            di_buf->vframe->private_data = di_buf;
            di_buf->index = i;
            list_add_tail(&(di_buf->list), &in_free_list_head);
        }
    }

    for(i=0; i<MAX_POST_BUF_NUM; i++){
        di_buf_t* di_buf = &(di_buf_post[i]);
        if(di_buf){
            memset(di_buf, sizeof(di_buf_t), 0);
            di_buf->type = VFRAME_TYPE_POST;
            di_buf->index = i;
            di_buf->vframe = &(vframe_post[i]);
            di_buf->vframe->private_data = di_buf;
            list_add_tail(&(di_buf->list), &post_free_list_head);
        }
    }
    return 0;
}

static void di_uninit_buf(void)
{
    di_buf_t *p = NULL, *ptmp;
    int i;
    list_for_each_entry_safe(p, ptmp, &local_free_list_head, list) {
        list_del(&p->list);
    }
    list_for_each_entry_safe(p, ptmp, &in_free_list_head, list) {
        list_del(&p->list);
    }
    list_for_each_entry_safe(p, ptmp, &pre_ready_list_head, list) {
        list_del(&p->list);
    }
    list_for_each_entry_safe(p, ptmp, &recycle_list_head, list) {
        list_del(&p->list);
    }
    list_for_each_entry_safe(p, ptmp, &post_free_list_head, list) {
        list_del(&p->list);
    }
    list_for_each_entry_safe(p, ptmp, &post_ready_list_head, list) {
        list_del(&p->list);
    }
    list_for_each_entry_safe(p, ptmp, &display_list_head, list) {
        list_del(&p->list);
    }
    
    for(i=0; i<MAX_IN_BUF_NUM; i++){
			vframe_in[i]=NULL;
		}
}

static void di_clean_in_buf(void)
{
    di_buf_t *di_buf = NULL, *ptmp;
    //di_printk_flag = 1;
    frame_count = 0;
    //disp_frame_count = 0; //do not set it to 0 here to make start_frame_hold_count not work when "keep last frame" is enabled
    if(buf_mgr_mode&0x100){
        while(!list_empty(&post_ready_list_head)){
            di_buf = list_first_entry(&post_ready_list_head, struct di_buf_s, list);
            recycle_vframe_type_post(di_buf);
#ifdef DI_DEBUG
            recycle_vframe_type_post_print(di_buf, __func__);
#endif
        }
    }
    
    if(di_pre_stru.di_mem_buf_dup_p){
        di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
        di_pre_stru.di_mem_buf_dup_p = NULL;
    }
    if(di_pre_stru.di_chan2_buf_dup_p){
        di_pre_stru.di_chan2_buf_dup_p->pre_ref_count = 0;
        di_pre_stru.di_chan2_buf_dup_p = NULL;
    }

    di_pre_stru.process_count = 0;
    if(di_pre_stru.di_inp_buf){
        if(vframe_in[di_pre_stru.di_inp_buf->index]){
            vframe_in[di_pre_stru.di_inp_buf->index] = NULL;
        }
        list_add_tail(&(di_pre_stru.di_inp_buf->list), &in_free_list_head);
        di_pre_stru.di_inp_buf = NULL;
    }

    if(di_pre_stru.di_wr_buf){
        di_pre_stru.di_wr_buf->pre_ref_count = 0;
        list_add_tail(&(di_pre_stru.di_wr_buf->list), &recycle_list_head);
        di_pre_stru.di_wr_buf = NULL;
    }

    list_for_each_entry_safe(di_buf, ptmp, &pre_ready_list_head, list) {
        list_del(&di_buf->list);
        list_add_tail(&(di_buf->list), &recycle_list_head);
    }

    list_for_each_entry_safe(di_buf, ptmp, &recycle_list_head, list) {
        if(di_buf->type == VFRAME_TYPE_IN){
            di_buf->pre_ref_count = 0;
            list_del(&di_buf->list);
            if(vframe_in[di_buf->index]){
                vframe_in[di_buf->index] = NULL;
            }
            list_add_tail(&(di_buf->list), &in_free_list_head);
        }
        else{
            if((di_buf->pre_ref_count == 0)&&(di_buf->post_ref_count == 0)){
                list_del(&di_buf->list);
                list_add_tail(&(di_buf->list), &local_free_list_head);
            }
        }
    }
    di_pre_stru.cur_width = 0;
    di_pre_stru.cur_height= 0;
    di_pre_stru.cur_prog_flag = 0;
    di_pre_stru.cur_inp_type = 0;

    di_pre_stru.pre_de_process_done = 0;
    di_pre_stru.pre_de_busy = 0;
}

static void log_buffer_state(unsigned char* tag)
{
    if(di_log_flag&DI_LOG_BUFFER_STATE){
        di_buf_t *p = NULL, *ptmp;
        int in_free = 0;
        int local_free = 0;
        int pre_ready = 0;
        int post_free = 0;
        int post_ready = 0;
        int post_ready_ext = 0;
        int display = 0;
        int display_ext = 0;
        int recycle = 0;
        int di_inp = 0;
        int di_wr = 0;
        ulong fiq_flag;
        raw_local_save_flags(fiq_flag);
        local_fiq_disable();

        list_for_each_entry_safe(p, ptmp, &in_free_list_head, list) {
            in_free++;
        }
        list_for_each_entry_safe(p, ptmp, &local_free_list_head, list) {
            local_free++;
        }
        list_for_each_entry_safe(p, ptmp, &pre_ready_list_head, list) {
            pre_ready++;
        }
        list_for_each_entry_safe(p, ptmp, &post_free_list_head, list) {
            post_free++;
        }
        list_for_each_entry_safe(p, ptmp, &post_ready_list_head, list) {
            post_ready++;
            if(p->di_buf[0]){
                post_ready_ext++;
            }
            if(p->di_buf[1]){
                post_ready_ext++;
            }
        }
        list_for_each_entry_safe(p, ptmp, &display_list_head, list) {
            display++;
            if(p->di_buf[0]){
                display_ext++;
            }
            if(p->di_buf[1]){
                display_ext++;
            }
        }
        list_for_each_entry_safe(p, ptmp, &recycle_list_head, list) {
            recycle++;
        }
        if(di_pre_stru.di_inp_buf)
            di_inp++;
        if(di_pre_stru.di_wr_buf)
            di_wr++;
				
				if(buf_state_log_threshold == 0){
					  buf_state_log_start = 0;
				}
				else if(post_ready < buf_state_log_threshold){
			  		buf_state_log_start = 1;	
				}
				if(buf_state_log_start){	
	        di_print("[%s]i %d, i_f %d/%d, l_f %d/%d, pre_r %d, post_f %d/%d, post_r (%d:%d), disp (%d:%d),rec %d, di_i %d, di_w %d\r\n",
	            tag,
	            provider_vframe_level,
	            in_free,MAX_IN_BUF_NUM,
	            local_free, local_buf_num,
	            pre_ready,
	            post_free, MAX_POST_BUF_NUM,
	            post_ready, post_ready_ext,
	            display, display_ext,
	            recycle,
	            di_inp, di_wr
	            );
				}
        raw_local_irq_restore(fiq_flag);

    }


}

static void dump_state(void)
{
    di_buf_t *p = NULL, *ptmp;
    printk("provider vframe level %d\n", provider_vframe_level);
    printk("in_free_list (max %d):\n", MAX_IN_BUF_NUM);
    list_for_each_entry_safe(p, ptmp, &in_free_list_head, list) {
        printk("index %d, %x, type %d\n", p->index, (unsigned int)p, p->type);
    }
    printk("local_free_list (max %d):\n", local_buf_num);
    list_for_each_entry_safe(p, ptmp, &local_free_list_head, list) {
        printk("index %d, %x, type %d\n", p->index, (unsigned int)p, p->type);
    }
    printk("pre_ready_list:\n");
    list_for_each_entry_safe(p, ptmp, &pre_ready_list_head, list) {
        printk("index %d, %x, type %d, vframetype%x\n", p->index, (unsigned int)p, p->type, p->vframe->type);
    }
    printk("post_free_list (max %d):\n", MAX_POST_BUF_NUM);
    list_for_each_entry_safe(p, ptmp, &post_free_list_head, list) {
        printk("index %d, %x, type %d, vframetype%x\n", p->index, (unsigned int)p, p->type ,p->vframe->type);
    }
    printk("post_ready_list:\n");
    list_for_each_entry_safe(p, ptmp, &post_ready_list_head, list) {
        printk("index %d, %x, type %d, vframetype%x\n", p->index, (unsigned int)p, p->type, p->vframe->type);
        if(p->di_buf[0]){
            printk("    +index %d, %x, type %d, vframetype%x\n", p->di_buf[0]->index, (unsigned int)p->di_buf[0], p->di_buf[0]->type, p->di_buf[0]->vframe->type);
        }
        if(p->di_buf[1]){
            printk("    +index %d, %x, type %d, vframetype%x\n", p->di_buf[1]->index, (unsigned int)p->di_buf[1], p->di_buf[1]->type, p->di_buf[1]->vframe->type);
        }
    }
    printk("display_list:\n");
    list_for_each_entry_safe(p, ptmp, &display_list_head, list) {
        printk("index %d, %x, type %d, vframetype%x\n", p->index, (unsigned int)p, p->type, p->vframe->type);
        if(p->di_buf[0]){
            printk("    +index %d, %x, type %d, vframetype%x\n", p->di_buf[0]->index, (unsigned int)p->di_buf[0], p->di_buf[0]->type, p->di_buf[0]->vframe->type);
        }
        if(p->di_buf[1]){
            printk("    +index %d, %x, type %d, vframetype%x\n", p->di_buf[1]->index, (unsigned int)p->di_buf[1], p->di_buf[1]->type, p->di_buf[1]->vframe->type);
        }
    }
    printk("recycle_list:\n");
    list_for_each_entry_safe(p, ptmp, &recycle_list_head, list) {
        printk("index %d, %x, type %d, vframetype%x\n", p->index, (unsigned int)p, p->type, p->vframe->type);
    }
    if(di_pre_stru.di_inp_buf)
        printk("di_inp_buf:index %d, %x, type %d\n", di_pre_stru.di_inp_buf->index, (unsigned int)di_pre_stru.di_inp_buf, di_pre_stru.di_inp_buf->type);
    if(di_pre_stru.di_wr_buf)
        printk("di_wr_buf:index %d, %x, type %d\n", di_pre_stru.di_wr_buf->index, (unsigned int)di_pre_stru.di_wr_buf, di_pre_stru.di_wr_buf->type);
    dump_di_pre_stru();
}

/*
*  di pre process
*/
static void config_di_wr_mif(DI_SIM_MIF_t* di_nrwr_mif, DI_SIM_MIF_t* di_mtnwr_mif, di_buf_t* di_buf, vframe_t* in_vframe)
{
    	di_nrwr_mif->canvas_num = di_buf->nr_canvas_idx;

    	di_nrwr_mif->start_x			= 0;
    	di_nrwr_mif->end_x 			= in_vframe->width - 1;
    	di_nrwr_mif->start_y			= 0;
 			if(di_pre_stru.prog_proc_type == 0){
    	    di_nrwr_mif->end_y 			= in_vframe->height/2 - 1;
    	}
    	else{
          di_nrwr_mif->end_y 			= in_vframe->height - 1;
      }

 			if(di_pre_stru.prog_proc_type == 0){
        	di_mtnwr_mif->start_x 		= 0;
        	di_mtnwr_mif->end_x 			= in_vframe->width - 1;
        	di_mtnwr_mif->start_y 		= 0;
        	di_mtnwr_mif->end_y 			= in_vframe->height/2 - 1;
        	di_mtnwr_mif->canvas_num = di_buf->mtn_canvas_idx;
      }
}

static void config_di_mif(DI_MIF_t* di_mif, di_buf_t*di_buf)
{
	    if(di_buf == NULL)
	        return;
	    di_mif->canvas0_addr0 = di_buf->vframe->canvas0Addr & 0xff;
	    di_mif->canvas0_addr1 = (di_buf->vframe->canvas0Addr>>8) & 0xff;
	    di_mif->canvas0_addr2 = (di_buf->vframe->canvas0Addr>>16) & 0xff;

		if ( di_buf->vframe->type & VIDTYPE_VIU_422 )
		{ //from vdin or local vframe
        if((!is_progressive(di_buf->vframe)) //interlace, from vdin or local vframe
			      ||(di_pre_stru.prog_proc_type)   //progressive(by frame), from vdin or local frame
			    ){
    			di_mif->video_mode = 0;
    			di_mif->set_separate_en = 0;
    			di_mif->src_field_mode = 0;
    			di_mif->output_field_num = 0;
    			di_mif->burst_size_y = 3;
    			di_mif->burst_size_cb = 0;
    			di_mif->burst_size_cr = 0;
    			di_mif->luma_x_start0 	= 0;
    			di_mif->luma_x_end0 		= di_buf->vframe->width - 1;
    			di_mif->luma_y_start0 	= 0;
    			if(di_pre_stru.prog_proc_type){
    			    di_mif->luma_y_end0 		= di_buf->vframe->height - 1;
    			}
    			else{
    			    di_mif->luma_y_end0 		= di_buf->vframe->height/2 - 1;
    			}
    			di_mif->chroma_x_start0 	= 0;
    			di_mif->chroma_x_end0 	= 0;
    			di_mif->chroma_y_start0 	= 0;
    			di_mif->chroma_y_end0 	= 0;

    	    di_mif->canvas0_addr0 = di_buf->vframe->canvas0Addr & 0xff;
    	    di_mif->canvas0_addr1 = (di_buf->vframe->canvas0Addr>>8) & 0xff;
    	    di_mif->canvas0_addr2 = (di_buf->vframe->canvas0Addr>>16) & 0xff;
    	  }
    	  else{
    	      //progressive (by field), from vdin only
            di_mif->video_mode = 0;
            di_mif->set_separate_en = 0;
            di_mif->src_field_mode = 1;
            di_mif->burst_size_y = 3;
            di_mif->burst_size_cb = 0;
            di_mif->burst_size_cr = 0;

            if(di_pre_stru.process_count>0){    //process top
    			    di_mif->output_field_num = 0;    									// top

        			di_mif->luma_x_start0 	= 0;
        			di_mif->luma_x_end0 		= di_buf->vframe->width - 1;
        			di_mif->luma_y_start0 	= 0;
        			di_mif->luma_y_end0 		= di_buf->vframe->height - 2;
        			di_mif->chroma_x_start0 	= 0;
        			di_mif->chroma_x_end0 	= di_buf->vframe->width/2 - 1;
        			di_mif->chroma_y_start0 	= 0;
        			di_mif->chroma_y_end0 	= di_buf->vframe->height/2 - 2;
            }
            else{        //process bot
        			di_mif->output_field_num = 1;    									// bottom

        			di_mif->luma_x_start0 	= 0;
        			di_mif->luma_x_end0 		= di_buf->vframe->width - 1;
        			di_mif->luma_y_start0 	= 1;
        			di_mif->luma_y_end0 		= di_buf->vframe->height - 1;
        			di_mif->chroma_x_start0 	= 0;
        			di_mif->chroma_x_end0 	= di_buf->vframe->width/2 - 1;
        			di_mif->chroma_y_start0 	= 1;
        			di_mif->chroma_y_end0 	= di_buf->vframe->height/2 - 1;
        	}
    	 }
		}
		else{
		    //from decoder
			di_mif->video_mode = 0;
			di_mif->set_separate_en = 1;
			di_mif->src_field_mode = 1;
			di_mif->burst_size_y = 3;
			di_mif->burst_size_cb = 1;
			di_mif->burst_size_cr = 1;

      if(((di_buf->vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)
        ||(di_pre_stru.process_count>0)){
			    di_mif->output_field_num = 0;    									// top

    			di_mif->luma_x_start0 	= 0;
    			di_mif->luma_x_end0 		= di_buf->vframe->width - 1;
    			di_mif->luma_y_start0 	= 0;
    			di_mif->luma_y_end0 		= di_buf->vframe->height - 2;
    			di_mif->chroma_x_start0 	= 0;
    			di_mif->chroma_x_end0 	= di_buf->vframe->width/2 - 1;
    			di_mif->chroma_y_start0 	= 0;
    			di_mif->chroma_y_end0 	= di_buf->vframe->height/2 - 2;
			}
      else{
			    di_mif->output_field_num = 1;    									// bottom

    			di_mif->luma_x_start0 	= 0;
    			di_mif->luma_x_end0 		= di_buf->vframe->width - 1;
    			di_mif->luma_y_start0 	= 1;
    			di_mif->luma_y_end0 		= di_buf->vframe->height - 1;
    			di_mif->chroma_x_start0 	= 0;
    			di_mif->chroma_x_end0 	= di_buf->vframe->width/2 - 1;
    			di_mif->chroma_y_start0 	= 1;
    			di_mif->chroma_y_end0 	= di_buf->vframe->height/2 - 1;
			}
		}

}

static void pre_de_process(void)
{
  int chan2_field_num = 1;
#ifdef DI_DEBUG
    di_print("%s: start\n", __func__);
#endif

    di_pre_stru.pre_de_busy = 1;
    di_pre_stru.pre_de_busy_timer_count = 0;

    config_di_mif(&di_pre_stru.di_inp_mif, di_pre_stru.di_inp_buf);
    config_di_mif(&di_pre_stru.di_mem_mif, di_pre_stru.di_mem_buf_dup_p);
    config_di_mif(&di_pre_stru.di_chan2_mif, di_pre_stru.di_chan2_buf_dup_p);
    config_di_wr_mif(&di_pre_stru.di_nrwr_mif, &di_pre_stru.di_mtnwr_mif,
        di_pre_stru.di_wr_buf, di_pre_stru.di_inp_buf->vframe);


    if((di_pre_stru.di_chan2_buf_dup_p)&&
        ((di_pre_stru.di_chan2_buf_dup_p->vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)){
            chan2_field_num = 0;
    }

    Wr(DI_PRE_SIZE,    di_pre_stru.di_nrwr_mif.end_x|(di_pre_stru.di_nrwr_mif.end_y << 16) );

    // set interrupt mask for pre module.
    Wr(DI_INTR_CTRL, (0 << 16) |       //  nrwr interrupt.
                ((di_pre_stru.enable_mtnwr?0:1) << 17) |       //  mtnwr interrupt.
                (1 << 18) |       //  diwr interrupt.
                (1 << 19) |       //  hist check interrupt.
                 0xf );            // clean all pending interrupt bits.

#if 1
		enable_di_mode_check_2(
		   	(pd_win_prop[0].win_start_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR),
        (pd_win_prop[0].win_end_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[0].win_start_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR),
		   	(pd_win_prop[0].win_end_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[1].win_start_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR),
        (pd_win_prop[1].win_end_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[1].win_start_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR),
		   	(pd_win_prop[1].win_end_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[2].win_start_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR),
        (pd_win_prop[2].win_end_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[2].win_start_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR),
		   	(pd_win_prop[2].win_end_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[3].win_start_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR),
        (pd_win_prop[3].win_end_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[3].win_start_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR),
		   	(pd_win_prop[3].win_end_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[4].win_start_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR),
        (pd_win_prop[4].win_end_x_r*di_pre_stru.di_inp_buf->vframe->width/WIN_SIZE_FACTOR)-1,
		   	(pd_win_prop[4].win_start_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR),
		   	(pd_win_prop[4].win_end_y_r*(di_pre_stru.di_inp_buf->vframe->height/2)/WIN_SIZE_FACTOR)-1
		   	);
#else
        enable_di_mode_check_2(
		    0, di_pre_stru.di_inp_buf->vframe->width-1, 0, di_pre_stru.di_inp_buf->vframe->height/2-1,						// window 0 ( start_x, end_x, start_y, end_y)
		   	0, di_pre_stru.di_inp_buf->vframe->width-1, 0, di_pre_stru.di_inp_buf->vframe->height/2-1,						// window 1 ( start_x, end_x, start_y, end_y)
		   	0, di_pre_stru.di_inp_buf->vframe->width-1, 0, di_pre_stru.di_inp_buf->vframe->height/2-1,						// window 2 ( start_x, end_x, start_y, end_y)
		   	0, di_pre_stru.di_inp_buf->vframe->width-1, 0, di_pre_stru.di_inp_buf->vframe->height/2-1,						// window 3 ( start_x, end_x, start_y, end_y)
		   	0, di_pre_stru.di_inp_buf->vframe->width-1, 0, di_pre_stru.di_inp_buf->vframe->height/2-1						// window 4 ( start_x, end_x, start_y, end_y)
		   	);
#endif
    WRITE_MPEG_REG(DI_PRE_CTRL, 0x3 << 30); //reset


    enable_di_pre_aml (  &di_pre_stru.di_inp_mif,               // di_inp
               &di_pre_stru.di_mem_mif,               // di_mem
               &di_pre_stru.di_chan2_mif,               // chan2
               &di_pre_stru.di_nrwr_mif,               // nrwrite
               &di_pre_stru.di_mtnwr_mif,            // mtn write
               1,                      // nr enable
               di_pre_stru.enable_mtnwr,                      // mtn enable
               di_pre_stru.enable_pulldown_check,                                 // pd32 check_en
               di_pre_stru.enable_pulldown_check,                                  // pd22 check_en
#if defined(CONFIG_ARCH_MESON)
			         1,                      											// hist check_en
#else 
			         0,                      											// hist check_en
#endif
               chan2_field_num,                      //  field num for chan2. 1 bottom, 0 top.
               0,                      // pre viu link.
               10                     //hold line.
             );

}


static void pre_de_done_buf_config(void)
{
    ulong fiq_flag;
    if(di_pre_stru.di_wr_buf){
        read_pulldown_info(&(di_pre_stru.di_wr_buf->field_pd_info),
                            &(di_pre_stru.di_wr_buf->win_pd_info[0])
                            );
        read_mtn_info(di_pre_stru.di_wr_buf->mtn_info,reg_mtn_info);
        if(di_pre_stru.cur_prog_flag){
            if(di_pre_stru.prog_proc_type == 0){
                if((di_pre_stru.process_count>0)
                    &&(is_progressive(di_pre_stru.di_mem_buf_dup_p->vframe))){
                        // di_mem_buf_dup_p->vframe is from in_list, and it is top field
                    di_pre_stru.di_chan2_buf_dup_p = di_pre_stru.di_wr_buf;
#ifdef DI_DEBUG
                    di_print("%s: set di_chan2_buf_dup_p to di_wr_buf\n", __func__);
#endif
                }
                else{ // di_mem_buf_dup->vfrme is either local vframe, or bot field of vframe from in_list
                    di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
                    di_pre_stru.di_mem_buf_dup_p = di_pre_stru.di_chan2_buf_dup_p;
                    di_pre_stru.di_chan2_buf_dup_p = di_pre_stru.di_wr_buf;
#ifdef DI_DEBUG
                    di_print("%s: set di_mem_buf_dup_p to di_chan2_buf_dup_p; set di_chan2_buf_dup_p to di_wr_buf\n", __func__);
#endif
                }
            }
            else{
                di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
                di_pre_stru.di_mem_buf_dup_p = di_pre_stru.di_wr_buf;
#ifdef DI_DEBUG
                di_print("%s: set di_mem_buf_dup_p to di_wr_buf\n", __func__);
#endif
            }

            di_pre_stru.di_wr_buf->seq = di_pre_stru.pre_ready_seq++;
            di_pre_stru.di_wr_buf->post_ref_count = 0;
            if(di_pre_stru.source_change_flag){
                di_pre_stru.di_wr_buf->new_format_flag = 1;
                di_pre_stru.source_change_flag = 0;
            }
            else{
                di_pre_stru.di_wr_buf->new_format_flag = 0;
            }
            if(bypass_state == 1){
                di_pre_stru.di_wr_buf->new_format_flag = 1; 
                bypass_state = 0;   
            }
            
            list_add_tail(&(di_pre_stru.di_wr_buf->list), &pre_ready_list_head);
#ifdef DI_DEBUG
            di_print("%s: process_count %d, %s[%d] => pre_ready_list\n",
                __func__, di_pre_stru.process_count, vframe_type_name[di_pre_stru.di_wr_buf->type], di_pre_stru.di_wr_buf->index);
#endif
            di_pre_stru.di_wr_buf = NULL;
        }
        else{
            di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
            di_pre_stru.di_mem_buf_dup_p = NULL;
            if(di_pre_stru.di_chan2_buf_dup_p){
                di_pre_stru.di_mem_buf_dup_p = di_pre_stru.di_chan2_buf_dup_p;
#ifdef DI_DEBUG
                di_print("%s: set di_mem_buf_dup_p to di_chan2_buf_dup_p\n", __func__);
#endif
            }
            di_pre_stru.di_chan2_buf_dup_p = di_pre_stru.di_wr_buf;

            if(di_pre_stru.di_wr_buf->post_proc_flag == 2){
                //add dummy buf, will not be displayed
                if(!list_empty(&local_free_list_head)){
                di_buf_t* di_buf_tmp;
                di_buf_tmp = list_first_entry(&local_free_list_head, struct di_buf_s, list);
                list_del(&(di_buf_tmp->list));
                di_buf_tmp->pre_ref_count = 0;
                di_buf_tmp->post_ref_count = 0;
                di_buf_tmp->post_proc_flag = 3;
                di_buf_tmp->new_format_flag = 0;
                list_add_tail(&(di_buf_tmp->list), &pre_ready_list_head);
#ifdef DI_DEBUG
                di_print("%s: dummy %s[%d] => pre_ready_list\n",
                    __func__, vframe_type_name[di_buf_tmp->type], di_buf_tmp->index);
#endif
            }
            }
            di_pre_stru.di_wr_buf->seq = di_pre_stru.pre_ready_seq++;
            di_pre_stru.di_wr_buf->post_ref_count = 0;
            if(di_pre_stru.source_change_flag){
                di_pre_stru.di_wr_buf->new_format_flag = 1;
                di_pre_stru.source_change_flag = 0;
            }
            else{
                di_pre_stru.di_wr_buf->new_format_flag = 0;
            }
            if(bypass_state == 1){
                di_pre_stru.di_wr_buf->new_format_flag = 1; 
                bypass_state = 0;   
            }
            
            list_add_tail(&(di_pre_stru.di_wr_buf->list), &pre_ready_list_head);

#ifdef DI_DEBUG
            di_print("%s: %s[%d] => pre_ready_list\n",
                __func__, vframe_type_name[di_pre_stru.di_wr_buf->type], di_pre_stru.di_wr_buf->index);
#endif
            di_pre_stru.di_wr_buf = NULL;

        }
    }

    if((di_pre_stru.process_count==0)&&(di_pre_stru.di_inp_buf)){
#ifdef DI_DEBUG
        di_print("%s: %s[%d] => recycle_list\n",
            __func__, vframe_type_name[di_pre_stru.di_inp_buf->type], di_pre_stru.di_inp_buf->index);
#endif
        raw_local_save_flags(fiq_flag);
        local_fiq_disable();
        list_add_tail(&(di_pre_stru.di_inp_buf->list), &recycle_list_head);
        di_pre_stru.di_inp_buf = NULL;
        raw_local_irq_restore(fiq_flag);

    }
}

#if defined(CONFIG_ARCH_MESON2)
/* add for di Reg re-init */
static enum vframe_source_type_e  vframe_source_type = VFRAME_SOURCE_TYPE_OTHERS;
static void di_set_para_by_tvinfo(vframe_t* vframe)
{
    if (vframe->source_type == vframe_source_type)
        return;
    pr_info("%s: tvinfo change, reset di Reg \n", __FUNCTION__);
    vframe_source_type = vframe->source_type;
    /* add for smooth skin */
    if (vframe_source_type != VFRAME_SOURCE_TYPE_OTHERS)
        nr_hfilt_en = 1;
    else
        nr_hfilt_en = 0;

    //if input is pal and ntsc
    if (vframe_source_type != VFRAME_SOURCE_TYPE_TUNER)
    {
        ei_ctrl0 =  (255 << 16) |     		// ei_filter.
                      (1 << 8) |        				// ei_threshold.
                      (0 << 2) |         				// ei bypass cf2.
                      (0 << 1);        				// ei bypass far1

        ei_ctrl1 =   (90 << 24) |      		// ei diff
                      (10 << 16) |       				// ei ang45
                      (15 << 8 ) |        				// ei peak.
                       45;             				// ei cross.

        ei_ctrl2 =    (10 << 24) |       		// close2
                      (10 << 16) |       				// close1
                      (10 << 8 ) |       				// far2
                       93;             				// far1
        kdeint2 = 25;
		mtn_ctrl= 0xe228c440;
		blend_ctrl=0x1f00019;
	pr_info("%s: tvinfo change, reset di Reg \n", __FUNCTION__);
    }
    else        //input is tuner
    {
        ei_ctrl0 =  (255 << 16) |     		// ei_filter.
                      (1 << 8) |        				// ei_threshold.
                      (0 << 2) |         				// ei bypass cf2.
                      (0 << 1);        				// ei bypass far1

        ei_ctrl1 =   ( 90 << 24) |      		// ei diff
                      (192 << 16) |       				// ei ang45
                      (15 << 8 ) |        				// ei peak.
                       128;             				// ei cross.

        ei_ctrl2 =    (10 << 24) |       		// close2
                      (255 << 16) |       				// close1
                      (10 << 8 ) |       				// far2
                       255;             				// far1
	if(kdeint1==0x10){
              kdeint2 = 25;
		mtn_ctrl= 0xe228c440 ;
		blend_ctrl=0x1f00019;
	}
       else{
	       kdeint2 = 25;
        #ifdef CONFIG_MACH_MESON2_7366M_REFE04
            mtn_ctrl = 0xe228c440;
            blend_ctrl = 0x15f00019;
            mtn_ctrl1_shift = 0x00000055;
        #else
		mtn_ctrl= 0x0 ;
		blend_ctrl=0x19f00019;
        #endif
		pr_info("%s: tvinfo change, reset di Reg in tuner source \n", __FUNCTION__);
       }
    }

   	//WRITE_MPEG_REG(DI_EI_CTRL0, ei_ctrl0);
   	//WRITE_MPEG_REG(DI_EI_CTRL1, ei_ctrl1);
   	//WRITE_MPEG_REG(DI_EI_CTRL2, ei_ctrl2);

}
#endif

static unsigned char pre_de_buf_config(void)
{
    di_buf_t* di_buf = NULL;
    vframe_t* vframe;
    if((list_empty(&in_free_list_head)&&(di_pre_stru.process_count==0))||
        list_empty(&local_free_list_head)){
        return 0;
    }

    if(di_pre_stru.process_count>0){
        /* previous is progressive top */
        di_pre_stru.process_count = 0;
    }
    else{
        //check if source change
#ifdef CHECK_VDIN_BUF_ERROR
#define WR_CANVAS_BIT                   0
#define WR_CANVAS_WID                   8
        vframe = vf_peek(VFM_NAME);
        if(vframe&&is_from_vdin(vframe)){
            if(vframe->canvas0Addr == READ_CBUS_REG_BITS((VDIN_WR_CTRL + 0), WR_CANVAS_BIT, WR_CANVAS_WID)){
                same_w_r_canvas_count++;
            }
        }
#endif

        vframe = vf_get(VFM_NAME);
        if(vframe == NULL){
            return 0;
        }
#ifdef DI_DEBUG
        di_print("%s: vf_get => %x\n", __func__, vframe);
#endif
        provider_vframe_level--;
        di_buf = list_first_entry(&in_free_list_head, struct di_buf_s, list);

        memcpy(di_buf->vframe, vframe, sizeof(vframe_t));
        di_buf->vframe->private_data = di_buf;
        vframe_in[di_buf->index] = vframe;
        di_buf->seq = di_pre_stru.in_seq;
        di_pre_stru.in_seq++;
        list_del(&di_buf->list);

        if(is_source_change(vframe)){ /* source change*/
            if(di_pre_stru.di_mem_buf_dup_p){
                di_pre_stru.di_mem_buf_dup_p->pre_ref_count = 0;
                di_pre_stru.di_mem_buf_dup_p = NULL;
            }
            if(di_pre_stru.di_chan2_buf_dup_p){
                di_pre_stru.di_chan2_buf_dup_p->pre_ref_count = 0;
                di_pre_stru.di_chan2_buf_dup_p = NULL;
            }
#ifdef DI_DEBUG
            di_print("%s: source change: %d/%d/%d=>%d/%d/%d\n", __func__,
                di_pre_stru.cur_inp_type, di_pre_stru.cur_width, di_pre_stru.cur_height,
                di_buf->vframe->type, di_buf->vframe->width, di_buf->vframe->height);
#endif
            di_pre_stru.cur_width = di_buf->vframe->width;
            di_pre_stru.cur_height= di_buf->vframe->height;
            di_pre_stru.cur_prog_flag = is_progressive(di_buf->vframe);
            di_pre_stru.cur_inp_type = di_buf->vframe->type;
            di_pre_stru.source_change_flag = 1;
            di_pre_stru.same_field_source_flag = 0;
#if defined(CONFIG_ARCH_MESON2)
            di_set_para_by_tvinfo(vframe);
#endif
        }
        else{
            /* check if top/bot interleaved */
            if(di_pre_stru.cur_prog_flag == 0){
                if((di_pre_stru.cur_inp_type & VIDTYPE_TYPEMASK) ==
                    (di_buf->vframe->type & VIDTYPE_TYPEMASK)){
#ifdef CHECK_VDIN_BUF_ERROR
                    if ((di_buf->vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)
                        same_field_top_count++;
                    else
                        same_field_bot_count++;
#endif
                    if(di_pre_stru.same_field_source_flag<same_field_source_flag_th){
                        /*some source's filed is top or bot always*/
                        di_pre_stru.same_field_source_flag++;

                    if(skip_wrong_field && is_from_vdin(di_buf->vframe)){
                        list_add_tail(&di_buf->list, &recycle_list_head);
                        return 0;
                    }
                }
            }
                else{
                    di_pre_stru.same_field_source_flag=0;
                }
            }
            di_pre_stru.cur_inp_type = di_buf->vframe->type;
        }

            if(is_bypass()){
                // bypass progressive
                di_buf->seq = di_pre_stru.pre_ready_seq++;
                di_buf->post_ref_count = 0;
                if(di_pre_stru.source_change_flag){
                    di_buf->new_format_flag = 1;
                    di_pre_stru.source_change_flag = 0;
                }
                else{
                    di_buf->new_format_flag = 0;
                }
                
                if(bypass_state == 0){
                    di_buf->new_format_flag = 1; 
                    bypass_state = 1;   
                }
                
                list_add_tail(&(di_buf->list), &pre_ready_list_head);
                di_buf->post_proc_flag = 0;
#ifdef DI_DEBUG
                di_print("%s: %s[%d] => pre_ready_list\n", __func__, vframe_type_name[di_buf->type], di_buf->index);
#endif
                return 0;
            }
            else if(is_progressive(vframe)){
                //n
                di_pre_stru.di_inp_buf = di_buf;
                if(di_pre_stru.prog_proc_type == 0){
                    di_pre_stru.process_count = 1;
                }
                else{
                    di_pre_stru.process_count = 0;
                }
#ifdef DI_DEBUG
                di_print("%s: %s[%d] => di_inp_buf, process_count %d\n",
                    __func__, vframe_type_name[di_buf->type], di_buf->index, di_pre_stru.process_count);
#endif
                if(di_pre_stru.di_mem_buf_dup_p == NULL){//use n
                    di_pre_stru.di_mem_buf_dup_p = di_buf;
#ifdef DI_DEBUG
                    di_print("%s: set di_mem_buf_dup_p to be di_inp_buf\n", __func__);
#endif
                }
            }
        else{
            //n
            di_pre_stru.di_inp_buf = di_buf;
#ifdef DI_DEBUG
            di_print("%s: %s[%d] => di_inp_buf\n", __func__, vframe_type_name[di_buf->type], di_buf->index);
#endif

            if(di_pre_stru.di_mem_buf_dup_p == NULL){// use n
                di_pre_stru.di_mem_buf_dup_p = di_buf;
#ifdef DI_DEBUG
                di_print("%s: set di_mem_buf_dup_p to be di_inp_buf\n", __func__);
#endif
            }
        }
     }

     /* di_wr_buf */
     di_buf= list_first_entry(&local_free_list_head, struct di_buf_s, list);
     list_del(&(di_buf->list));
     di_pre_stru.di_wr_buf = di_buf;
     di_pre_stru.di_wr_buf->pre_ref_count = 1;

#ifdef DI_DEBUG
     di_print("%s: %s[%d] => di_wr_buf\n", __func__, vframe_type_name[di_buf->type], di_buf->index);
#endif

    memcpy(di_buf->vframe, di_pre_stru.di_inp_buf->vframe, sizeof(vframe_t));
    di_buf->vframe->private_data = di_buf;
    di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
    di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;

		if(di_pre_stru.prog_proc_type){
        di_buf->vframe->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
    }
    else{
        if(((di_pre_stru.di_inp_buf->vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)
            ||(di_pre_stru.process_count>0)){
    				di_buf->vframe->type = VIDTYPE_INTERLACE_TOP | VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
    		}
        else{
    				di_buf->vframe->type = VIDTYPE_INTERLACE_BOTTOM | VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
    		}
    }

    /* */
    if(is_progressive(di_pre_stru.di_inp_buf->vframe)){
        di_pre_stru.enable_mtnwr = 0;
        di_pre_stru.enable_pulldown_check = 0;
        di_buf->post_proc_flag = 0;
    }
    else{
        if(di_pre_stru.di_chan2_buf_dup_p == NULL){
            di_pre_stru.enable_mtnwr = 0;
            di_pre_stru.enable_pulldown_check = 0;
            di_buf->post_proc_flag = 2;
        }
        else{
            di_pre_stru.enable_mtnwr = 1;
            di_pre_stru.enable_pulldown_check = 1;
            di_buf->post_proc_flag = 1;
        }
    }
    return 1;

}

static int check_recycle_buf(void)
{
    di_buf_t *di_buf = NULL, *ptmp;
    int ret = 0;
    list_for_each_entry_safe(di_buf, ptmp, &recycle_list_head, list) {
        if((di_buf->pre_ref_count == 0)&&(di_buf->post_ref_count == 0)){
            if(di_buf->type == VFRAME_TYPE_IN){
                list_del(&di_buf->list);
                    if(vframe_in[di_buf->index]){
                        vf_put(vframe_in[di_buf->index], VFM_NAME);
#ifdef DI_DEBUG
                        di_print("%s: vf_put(%d) %x\n", __func__, di_pre_stru.recycle_seq, vframe_in[di_buf->index]);
#endif
                        vframe_in[di_buf->index] = NULL;
                    }
                list_add_tail(&(di_buf->list), &in_free_list_head);
                di_pre_stru.recycle_seq++;
                ret |= 1;
            }
            else{
                list_del(&di_buf->list);
                list_add_tail(&(di_buf->list), &local_free_list_head);
                ret |= 2;
            }
#ifdef DI_DEBUG
            di_print("%s: recycle %s[%d]\n", __func__, vframe_type_name[di_buf->type], di_buf->index);
#endif
        }
    }
    return ret;
}

static irqreturn_t de_irq(int irq, void *dev_instance)
{
   unsigned int data32;
   data32 = Rd(DI_INTR_CTRL);
   //if ( (data32 & 0xf) != 0x1 ) {
   //     printk("%s: error %x\n", __func__, data32);
   //} else {
        Wr(DI_INTR_CTRL, data32);
   //}

    //Wr(A9_0_IRQ_IN1_INTR_STAT_CLR, 1 << 14);
   //Rd(A9_0_IRQ_IN1_INTR_STAT_CLR);
    if(di_pre_stru.pre_de_busy==0){
        di_print("%s: wrong enter %x\n", __func__, Rd(DI_INTR_CTRL));
        return IRQ_HANDLED;
    }
#ifdef DI_DEBUG
        di_print("%s: start\n", __func__);
#endif

    di_pre_stru.pre_de_process_done = 1;
    di_pre_stru.pre_de_busy = 0;

    if(init_flag){
        //printk("%s:up di sema\n", __func__);
        trigger_pre_di_process('i');
    }

#ifdef DI_DEBUG
        di_print("%s: end\n", __func__);
#endif

   return IRQ_HANDLED;
}

/*
di post process
*/
static void inc_post_ref_count(di_buf_t* di_buf)
{
    int post_blend_mode;
    if(di_buf->pulldown_mode == 0)
        post_blend_mode = 0;
    else if(di_buf->pulldown_mode == 1)
        post_blend_mode =1;
    else
        post_blend_mode = 3;


	  di_buf->di_buf_dup_p[1]->post_ref_count++;
    if ( post_blend_mode != 1 )
        di_buf->di_buf_dup_p[0]->post_ref_count++;
    di_buf->di_buf_dup_p[2]->post_ref_count++;

}

static void dec_post_ref_count(di_buf_t* di_buf)
{
    int post_blend_mode;
    if(di_buf->pulldown_mode == 0)
        post_blend_mode = 0;
    else if(di_buf->pulldown_mode == 1)
        post_blend_mode =1;
    else
        post_blend_mode = 3;


	  di_buf->di_buf_dup_p[1]->post_ref_count--;
    if ( post_blend_mode != 1 )
        di_buf->di_buf_dup_p[0]->post_ref_count--;
    di_buf->di_buf_dup_p[2]->post_ref_count--;
}

static int de_post_disable_fun(void* arg)
{
    disable_post_deinterlace_2();
    return 1; //called for new_format_flag, make video set video_property_changed
}

static int do_nothing_fun(void* arg)
{
    return 0;
}

#ifdef DI_POST_SKIP_LINE
static int get_vscale_skip_count(unsigned par)
{
    di_vscale_skip_count = (par >> 24)&0xff;        
}    
#endif

static int de_post_process(void* arg, unsigned zoom_start_x_lines,
     unsigned zoom_end_x_lines, unsigned zoom_start_y_lines, unsigned zoom_end_y_lines)
{
    di_buf_t* di_buf = (di_buf_t*)arg;
    int di_width, di_height, di_start_x, di_end_x, di_start_y, di_end_y;
    int hold_line = 10;
   	int post_blend_en, post_blend_mode;

#ifdef DI_POST_SKIP_LINE
    get_vscale_skip_count(zoom_start_x_lines);
#endif    
    zoom_start_x_lines = zoom_start_x_lines&0xffff;
    zoom_end_x_lines = zoom_end_x_lines&0xffff;
    zoom_start_y_lines = zoom_start_y_lines&0xffff;
    zoom_end_y_lines = zoom_end_y_lines&0xffff;
    
    if(di_buf->pulldown_mode == 0)
        post_blend_mode = 0;
    else if(di_buf->pulldown_mode == 1)
        post_blend_mode =1;
    else
        post_blend_mode = 3;
    //printk("post_blend_mode %d\n", post_blend_mode);

    if(init_flag == 0){
        return 0;
    }
    di_start_x = zoom_start_x_lines;
    di_end_x = zoom_end_x_lines;
    di_width = di_end_x - di_start_x + 1;
    di_start_y = (zoom_start_y_lines+1) & 0xfffffffe;
    di_end_y = (zoom_end_y_lines-1) | 0x1;
    di_height = di_end_y - di_start_y + 1;
//printk("height = (%d %d %d %d %d)\n", di_buf->vframe->height, zoom_start_x_lines, zoom_end_x_lines, zoom_start_y_lines, zoom_end_y_lines);

    	if ( READ_MPEG_REG(DI_POST_SIZE) != ((di_width-1) | ((di_height-1)<<16))
    		|| (di_post_stru.di_buf0_mif.luma_x_start0 != di_start_x) || (di_post_stru.di_buf0_mif.luma_y_start0 != di_start_y/2) )
    	{
    		initial_di_post_2(di_width, di_height, hold_line);
		    di_post_stru.di_buf0_mif.luma_x_start0 	= di_start_x;
		    di_post_stru.di_buf0_mif.luma_x_end0 	= di_end_x;
		    di_post_stru.di_buf0_mif.luma_y_start0 	= di_start_y/2;
		    di_post_stru.di_buf0_mif.luma_y_end0 	= (di_end_y + 1)/2 - 1;
		    di_post_stru.di_buf1_mif.luma_x_start0 	= di_start_x;
		    di_post_stru.di_buf1_mif.luma_x_end0 	= di_end_x;
		    di_post_stru.di_buf1_mif.luma_y_start0 	= di_start_y/2;
		    di_post_stru.di_buf1_mif.luma_y_end0 	= (di_end_y + 1)/2 - 1;
	    	di_post_stru.di_mtncrd_mif.start_x 		= di_start_x;
	    	di_post_stru.di_mtncrd_mif.end_x 		= di_end_x;
	    	di_post_stru.di_mtncrd_mif.start_y 		= di_start_y/2;
	    	di_post_stru.di_mtncrd_mif.end_y 		= (di_end_y + 1)/2 - 1;
			  di_post_stru.di_mtnprd_mif.start_x 		= di_start_x;
	    	di_post_stru.di_mtnprd_mif.end_x 		= di_end_x;
			  di_post_stru.di_mtnprd_mif.start_y 		= di_start_y/2;
	    	di_post_stru.di_mtnprd_mif.end_y 		= (di_end_y + 1)/2 - 1;
    	}

	    di_post_stru.di_buf0_mif.canvas0_addr0 = di_buf->di_buf_dup_p[1]->nr_canvas_idx;
	    if ( post_blend_mode == 1 )
	        di_post_stru.di_buf1_mif.canvas0_addr0 = di_buf->di_buf_dup_p[2]->nr_canvas_idx;
	    else
	        di_post_stru.di_buf1_mif.canvas0_addr0 = di_buf->di_buf_dup_p[0]->nr_canvas_idx;
	    di_post_stru.di_mtncrd_mif.canvas_num = di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
      di_post_stru.di_mtnprd_mif.canvas_num = di_buf->di_buf_dup_p[2]->mtn_canvas_idx;


    	post_blend_en = 1;
	    enable_di_post_2 (
	    		&di_post_stru.di_buf0_mif,
	    		&di_post_stru.di_buf1_mif,
	    		NULL,
	    		&di_post_stru.di_mtncrd_mif,
	    		&di_post_stru.di_mtnprd_mif,
	    		1, 																// ei enable
	    		post_blend_en,													// blend enable
	    		post_blend_en,													// blend mtn enable
	    		post_blend_mode,												// blend mode.
	    		1,                 												// di_vpp_en.
	    		0,                 												// di_ddr_en.
	    		(di_buf->di_buf_dup_p[1]->vframe->type & VIDTYPE_TYPEMASK)==VIDTYPE_INTERLACE_TOP ? 0 : 1,		// 1 bottom generate top
	    		hold_line,
	    		reg_mtn_info
	    	);
    return 0;
}

static int de_post_process_pd(void* arg, unsigned zoom_start_x_lines,
     unsigned zoom_end_x_lines, unsigned zoom_start_y_lines, unsigned zoom_end_y_lines)
{
    di_buf_t* di_buf = (di_buf_t*)arg;
    int di_width, di_height, di_start_x, di_end_x, di_start_y, di_end_y;
    int hold_line = 10;
   	int post_blend_mode;

#ifdef DI_POST_SKIP_LINE
    get_vscale_skip_count(zoom_start_x_lines);
#endif
    zoom_start_x_lines = zoom_start_x_lines&0xffff;
    zoom_end_x_lines = zoom_end_x_lines&0xffff;
    zoom_start_y_lines = zoom_start_y_lines&0xffff;
    zoom_end_y_lines = zoom_end_y_lines&0xffff;

    if(di_buf->pulldown_mode == 0)
        post_blend_mode = 0;
    else
        post_blend_mode =1;

    if(init_flag == 0){
        return 0;
    }
    di_start_x = zoom_start_x_lines;
    di_end_x = zoom_end_x_lines;
    di_width = di_end_x - di_start_x + 1;
    di_start_y = (zoom_start_y_lines+1) & 0xfffffffe;
    di_end_y = (zoom_end_y_lines-1) | 0x1;
    di_height = di_end_y - di_start_y + 1;
//printk("height = (%d %d %d %d %d)\n", di_buf->vframe->height, zoom_start_x_lines, zoom_end_x_lines, zoom_start_y_lines, zoom_end_y_lines);

    	if ( READ_MPEG_REG(DI_POST_SIZE) != ((di_width-1) | ((di_height-1)<<16))
    		|| (di_post_stru.di_buf0_mif.luma_x_start0 != di_start_x) || (di_post_stru.di_buf0_mif.luma_y_start0 != di_start_y/2) )
    	{
    		initial_di_post_2(di_width, di_height, hold_line);
		    di_post_stru.di_buf0_mif.luma_x_start0 	= di_start_x;
		    di_post_stru.di_buf0_mif.luma_x_end0 	= di_end_x;
		    di_post_stru.di_buf0_mif.luma_y_start0 	= di_start_y/2;
		    di_post_stru.di_buf0_mif.luma_y_end0 	= (di_end_y + 1)/2 - 1;
		    di_post_stru.di_buf1_mif.luma_x_start0 	= di_start_x;
		    di_post_stru.di_buf1_mif.luma_x_end0 	= di_end_x;
		    di_post_stru.di_buf1_mif.luma_y_start0 	= di_start_y/2;
		    di_post_stru.di_buf1_mif.luma_y_end0 	= (di_end_y + 1)/2 - 1;
	    	di_post_stru.di_mtncrd_mif.start_x 		= di_start_x;
	    	di_post_stru.di_mtncrd_mif.end_x 		= di_end_x;
	    	di_post_stru.di_mtncrd_mif.start_y 		= di_start_y/2;
	    	di_post_stru.di_mtncrd_mif.end_y 		= (di_end_y + 1)/2 - 1;
			  di_post_stru.di_mtnprd_mif.start_x 		= di_start_x;
	    	di_post_stru.di_mtnprd_mif.end_x 		= di_end_x;
			  di_post_stru.di_mtnprd_mif.start_y 		= di_start_y/2;
	    	di_post_stru.di_mtnprd_mif.end_y 		= (di_end_y + 1)/2 - 1;
    	}

	    di_post_stru.di_buf0_mif.canvas0_addr0 = di_buf->di_buf_dup_p[1]->nr_canvas_idx;
	    if ( post_blend_mode == 1 )
	        di_post_stru.di_buf1_mif.canvas0_addr0 = di_buf->di_buf_dup_p[2]->nr_canvas_idx;
	    else
	        di_post_stru.di_buf1_mif.canvas0_addr0 = di_buf->di_buf_dup_p[0]->nr_canvas_idx;
	    di_post_stru.di_mtncrd_mif.canvas_num = di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
      di_post_stru.di_mtnprd_mif.canvas_num = di_buf->di_buf_dup_p[2]->mtn_canvas_idx;


	    enable_di_post_pd (
	    		&di_post_stru.di_buf0_mif,
	    		&di_post_stru.di_buf1_mif,
	    		NULL,
	    		&di_post_stru.di_mtncrd_mif,
	    		&di_post_stru.di_mtnprd_mif,
	    		1, 																// ei enable
	    		1,													// blend enable
	    		1,													// blend mtn enable
	    		post_blend_mode,												// blend mode.
	    		1,                 												// di_vpp_en.
	    		0,                 												// di_ddr_en.
	    		(di_buf->di_buf_dup_p[1]->vframe->type & VIDTYPE_TYPEMASK)==VIDTYPE_INTERLACE_TOP ? 0 : 1,		// 1 bottom generate top
	    		hold_line
	    	);
    return 0;
}

static int de_post_process_prog(void* arg, unsigned zoom_start_x_lines,
     unsigned zoom_end_x_lines, unsigned zoom_start_y_lines, unsigned zoom_end_y_lines)
{
    di_buf_t* di_buf = (di_buf_t*)arg;
    int di_width, di_height, di_start_x, di_end_x, di_start_y, di_end_y;
    int hold_line = 10;
   	int post_blend_mode;

#ifdef DI_POST_SKIP_LINE
    get_vscale_skip_count(zoom_start_x_lines);
#endif    
    zoom_start_x_lines = zoom_start_x_lines&0xffff;
    zoom_end_x_lines = zoom_end_x_lines&0xffff;
    zoom_start_y_lines = zoom_start_y_lines&0xffff;
    zoom_end_y_lines = zoom_end_y_lines&0xffff;

    post_blend_mode =1;

    if(init_flag == 0){
        return 0;
    }
    di_start_x = zoom_start_x_lines;
    di_end_x = zoom_end_x_lines;
    di_width = di_end_x - di_start_x + 1;
    di_start_y = (zoom_start_y_lines+1) & 0xfffffffe;
    di_end_y = (zoom_end_y_lines-1) | 0x1;
    di_height = di_end_y - di_start_y + 1;
//printk("height = (%d %d %d %d %d)\n", di_buf->vframe->height, zoom_start_x_lines, zoom_end_x_lines, zoom_start_y_lines, zoom_end_y_lines);

    	if ( READ_MPEG_REG(DI_POST_SIZE) != ((di_width-1) | ((di_height-1)<<16))
    		|| (di_post_stru.di_buf0_mif.luma_x_start0 != di_start_x) || (di_post_stru.di_buf0_mif.luma_y_start0 != di_start_y/2) )
    	{
    		initial_di_post_2(di_width, di_height, hold_line);
		    di_post_stru.di_buf0_mif.luma_x_start0 	= di_start_x;
		    di_post_stru.di_buf0_mif.luma_x_end0 	= di_end_x;
		    di_post_stru.di_buf0_mif.luma_y_start0 	= di_start_y/2;
		    di_post_stru.di_buf0_mif.luma_y_end0 	= (di_end_y + 1)/2 - 1;
		    di_post_stru.di_buf1_mif.luma_x_start0 	= di_start_x;
		    di_post_stru.di_buf1_mif.luma_x_end0 	= di_end_x;
		    di_post_stru.di_buf1_mif.luma_y_start0 	= di_start_y/2;
		    di_post_stru.di_buf1_mif.luma_y_end0 	= (di_end_y + 1)/2 - 1;
	    	di_post_stru.di_mtncrd_mif.start_x 		= di_start_x;
	    	di_post_stru.di_mtncrd_mif.end_x 		= di_end_x;
	    	di_post_stru.di_mtncrd_mif.start_y 		= di_start_y/2;
	    	di_post_stru.di_mtncrd_mif.end_y 		= (di_end_y + 1)/2 - 1;
			  di_post_stru.di_mtnprd_mif.start_x 		= di_start_x;
	    	di_post_stru.di_mtnprd_mif.end_x 		= di_end_x;
			  di_post_stru.di_mtnprd_mif.start_y 		= di_start_y/2;
	    	di_post_stru.di_mtnprd_mif.end_y 		= (di_end_y + 1)/2 - 1;
    	}

	    di_post_stru.di_buf0_mif.canvas0_addr0 = di_buf->di_buf_dup_p[0]->nr_canvas_idx;
      di_post_stru.di_buf1_mif.canvas0_addr0 = di_buf->di_buf_dup_p[1]->nr_canvas_idx;
	    di_post_stru.di_mtncrd_mif.canvas_num = di_buf->di_buf_dup_p[0]->mtn_canvas_idx;
      di_post_stru.di_mtnprd_mif.canvas_num = di_buf->di_buf_dup_p[1]->mtn_canvas_idx;


	    enable_di_post_pd (
	    		&di_post_stru.di_buf0_mif,
	    		&di_post_stru.di_buf1_mif,
	    		NULL,
	    		&di_post_stru.di_mtncrd_mif,
	    		&di_post_stru.di_mtnprd_mif,
	    		1, 																// ei enable
	    		1,													// blend enable
	    		1,													// blend mtn enable
	    		post_blend_mode,												// blend mode.
	    		1,                 												// di_vpp_en.
	    		0,                 												// di_ddr_en.
	    		(di_buf->di_buf_dup_p[0]->vframe->type & VIDTYPE_TYPEMASK)==VIDTYPE_INTERLACE_TOP ? 0 : 1,		// 1 bottom generate top
	    		hold_line
	    	);
    return 0;
}

#ifdef FORCE_BOB_SUPPORT
static int de_post_process_force_bob(void* arg, unsigned zoom_start_x_lines,
     unsigned zoom_end_x_lines, unsigned zoom_start_y_lines, unsigned zoom_end_y_lines)
{
    de_post_process(arg, zoom_start_x_lines, zoom_end_x_lines, zoom_start_y_lines, zoom_end_y_lines);
    WRITE_MPEG_REG(DI_BLEND_CTRL, READ_MPEG_REG(DI_BLEND_CTRL)&0xffefffff);
    return 0;
}
#endif

int pd_detect_rst ;

static void recycle_vframe_type_post(di_buf_t* di_buf)
{
    int i;
    if( di_buf->vframe->process_fun == de_post_process_pd
        || di_buf->vframe->process_fun == de_post_process
#ifdef FORCE_BOB_SUPPORT
        || di_buf->vframe->process_fun == de_post_process_force_bob
#endif
        ){
        dec_post_ref_count(di_buf);
    }
    for(i=0;i<2;i++){
        if(di_buf->di_buf[i])
            list_add_tail(&di_buf->di_buf[i]->list, &recycle_list_head);
    }
    list_del(&di_buf->list); //remove it from display_list_head
    list_add_tail(&di_buf->list, &post_free_list_head);
}

#ifdef DI_DEBUG
static void recycle_vframe_type_post_print(di_buf_t* di_buf, const char* func)
{
    int i;
    di_print("%s:", func);
    for(i=0;i<2;i++){
        if(di_buf->di_buf[i]){
            di_print("%s[%d]<%d>=>recycle_list; ", vframe_type_name[di_buf->di_buf[i]->type], di_buf->di_buf[i]->index, i);
        }
    }
    di_print("%s[%d] =>post_free_list\n", vframe_type_name[di_buf->type], di_buf->index);
}
#endif

static int process_post_vframe(void)
{
/*
   1) get buf from post_free_list, config it according to buf in pre_ready_list, send it to post_ready_list
        (it will be send to post_free_list in di_vf_put())
   2) get buf from pre_ready_list, attach it to buf from post_free_list
        (it will be send to recycle_list in di_vf_put() )
*/
    ulong fiq_flag;
    int i,pulldown_mode_hise;
    int ret = 0;
    int buffer_keep_count = 3;
    di_buf_t* di_buf = NULL;
    di_buf_t *ready_di_buf;
    di_buf_t *p = NULL, *ptmp;
    int ready_count = list_count(&pre_ready_list_head, 0);
    if(list_empty(&post_free_list_head)){
        return 0;
    }
    if(ready_count>0){
        ready_di_buf = list_first_entry(&pre_ready_list_head, struct di_buf_s, list);
        if(ready_di_buf->post_proc_flag){
            /*if((pulldown_buffer_mode&1)
                &&(real_buf_mgr_mode==0)){
                buffer_keep_count = 4;
            }
            else{
                buffer_keep_count = 3;
            }*/
            if(ready_count>=buffer_keep_count){
                raw_local_save_flags(fiq_flag);
                local_fiq_disable();
                di_buf = list_first_entry(&post_free_list_head, struct di_buf_s, list);
                list_del(&(di_buf->list));
                raw_local_irq_restore(fiq_flag);

                i = 0;
                list_for_each_entry_safe(p, ptmp, &pre_ready_list_head, list) {
                    di_buf->di_buf_dup_p[i++] = p;
                    if(i>=buffer_keep_count)
                        break;
                }
                memcpy(di_buf->vframe, di_buf->di_buf_dup_p[1]->vframe, sizeof(vframe_t));
                di_buf->vframe->private_data = di_buf;
                if(di_buf->di_buf_dup_p[1]->post_proc_flag == 3){//dummy, not for display
                    di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
                    di_buf->di_buf[1] = NULL;
                    list_del(&di_buf->di_buf[0]->list);

                    raw_local_save_flags(fiq_flag);
                    local_fiq_disable();
                    list_add_tail(&di_buf->list, &post_ready_list_head);
                    recycle_vframe_type_post(di_buf);

                    raw_local_irq_restore(fiq_flag);

#ifdef DI_DEBUG
                    di_print("%s <dummy>: ", __func__);
#endif
                }
                else{
                    if(di_buf->di_buf_dup_p[1]->post_proc_flag == 2){
                        reset_pulldown_state();
                        di_buf->pulldown_mode = 1; /* blend with di_buf->di_buf_dup_p[2] */
                    }
                    else{
                        int pulldown_type=-1; /* 0, 2:2; 1, m:n */
                        int win_pd_type[5] = {-1,-1,-1,-1,-1};
                        int ii;
                        int pulldown_mode2;
                        insert_pd_his(&di_buf->di_buf_dup_p[1]->field_pd_info);
                        if(buffer_keep_count==4){
                        //4 buffers
                                cal_pd_parameters(&di_buf->di_buf_dup_p[2]->field_pd_info, &di_buf->di_buf_dup_p[1]->field_pd_info, &di_buf->di_buf_dup_p[3]->field_pd_info, &field_pd_th); //cal parameters of di_buf_dup_p[2]
                                pattern_check_pre_2(0, &di_buf->di_buf_dup_p[3]->field_pd_info,
                                     &di_buf->di_buf_dup_p[2]->field_pd_info,
                                    &di_buf->di_buf_dup_p[1]->field_pd_info,
                                    &di_buf->di_buf_dup_p[2]->pulldown_mode,
                                    &di_buf->di_buf_dup_p[1]->pulldown_mode,
                                    &pulldown_type, &field_pd_th
	                                );

                                  for(ii=0;ii<MAX_WIN_NUM; ii++){
                                    cal_pd_parameters(&di_buf->di_buf_dup_p[2]->win_pd_info[ii], &di_buf->di_buf_dup_p[1]->win_pd_info[ii], &di_buf->di_buf_dup_p[3]->win_pd_info[ii], &field_pd_th); //cal parameters of di_buf_dup_p[2]
                                    pattern_check_pre_2(ii+1, &di_buf->di_buf_dup_p[3]->win_pd_info[ii],
                                         &di_buf->di_buf_dup_p[2]->win_pd_info[ii],
                                        &di_buf->di_buf_dup_p[1]->win_pd_info[ii],
                                        &(di_buf->di_buf_dup_p[2]->win_pd_mode[ii]),
                                        &(di_buf->di_buf_dup_p[1]->win_pd_mode[ii]),
                                        &(win_pd_type[ii]), &win_pd_th[ii]
			                                );
                                  }
                        }
                        else{
                        //3 buffers
                                cal_pd_parameters(&di_buf->di_buf_dup_p[1]->field_pd_info, &di_buf->di_buf_dup_p[0]->field_pd_info, &di_buf->di_buf_dup_p[2]->field_pd_info, &field_pd_th); //cal parameters of di_buf_dup_p[1]
                                pattern_check_pre_2(0, &di_buf->di_buf_dup_p[2]->field_pd_info,
                                     &di_buf->di_buf_dup_p[1]->field_pd_info,
                                    &di_buf->di_buf_dup_p[0]->field_pd_info,
                                    &di_buf->di_buf_dup_p[1]->pulldown_mode,
                                    NULL, &pulldown_type, &field_pd_th);

                                  for(ii=0;ii<MAX_WIN_NUM; ii++){
                                    cal_pd_parameters(&di_buf->di_buf_dup_p[1]->win_pd_info[ii], &di_buf->di_buf_dup_p[0]->win_pd_info[ii], &di_buf->di_buf_dup_p[2]->win_pd_info[ii], &field_pd_th); //cal parameters of di_buf_dup_p[1]
                                    pattern_check_pre_2(ii+1, &di_buf->di_buf_dup_p[2]->win_pd_info[ii],
                                         &di_buf->di_buf_dup_p[1]->win_pd_info[ii],
                                        &di_buf->di_buf_dup_p[0]->win_pd_info[ii],
                                        &(di_buf->di_buf_dup_p[1]->win_pd_mode[ii]),
                                        NULL,&(win_pd_type[ii]), &win_pd_th[ii]
			                                );
                                  }
                        }
                        pulldown_mode_hise = pulldown_mode2 = detect_pd32();

                        if(di_log_flag&DI_LOG_PULLDOWN)
                        {
                            di_buf_t* dp = di_buf->di_buf_dup_p[1];
                            di_print("%02d (%x%x%x) %08x %06x %08x %06x %02x %02x %02x %02x %02x %02x %02x %02x ", dp->seq%100,
                                    dp->pulldown_mode<0?0xf:dp->pulldown_mode, pulldown_type<0?0xf:pulldown_type,
                                    pulldown_mode2<0?0xf:pulldown_mode2,
                                    dp->field_pd_info.frame_diff, dp->field_pd_info.frame_diff_num,
                                  dp->field_pd_info.field_diff, dp->field_pd_info.field_diff_num,
                                  dp->field_pd_info.frame_diff_by_pre, dp->field_pd_info.frame_diff_num_by_pre,
                                  dp->field_pd_info.field_diff_by_pre, dp->field_pd_info.field_diff_num_by_pre,
                                  dp->field_pd_info.field_diff_by_next, dp->field_pd_info.field_diff_num_by_next,
                                  dp->field_pd_info.frame_diff_skew_ratio, dp->field_pd_info.frame_diff_num_skew_ratio);
                            for(ii=0; ii<MAX_WIN_NUM; ii++){
                                di_print("(%x,%x) %08x %06x %08x %06x %02x %02x %02x %02x %02x %02x %02x %02x ", dp->win_pd_mode[ii]<0?0xf:dp->win_pd_mode[ii],
                                     win_pd_type[ii]<0?0xf:win_pd_type[ii],
                                    dp->win_pd_info[ii].frame_diff, dp->win_pd_info[ii].frame_diff_num,
                                  dp->win_pd_info[ii].field_diff, dp->win_pd_info[ii].field_diff_num,
                                  dp->win_pd_info[ii].frame_diff_by_pre, dp->win_pd_info[ii].frame_diff_num_by_pre,
                                  dp->win_pd_info[ii].field_diff_by_pre, dp->win_pd_info[ii].field_diff_num_by_pre,
                                  dp->win_pd_info[ii].field_diff_by_next, dp->win_pd_info[ii].field_diff_num_by_next,
                                  dp->win_pd_info[ii].frame_diff_skew_ratio, dp->win_pd_info[ii].frame_diff_num_skew_ratio);
                            }
                            di_print("\n");
                        }

                        di_buf->pulldown_mode = -1;
                        if(pulldown_detect){
                            if(pulldown_detect&0x1){
                                di_buf->pulldown_mode = di_buf->di_buf_dup_p[1]->pulldown_mode; //used by de_post_process
                            }
                            if(pulldown_detect&0x10){
                                if((pulldown_mode2 >=0) && (pd_detect_rst > 15)){
                                    di_buf->pulldown_mode = pulldown_mode2;
                                }
                            }
							if(pd_detect_rst <= 32)
							pd_detect_rst ++;


                            if((pulldown_win_mode&0xfffff)!=0){
                                int ii;
                                unsigned mode;
                                for(ii=0; ii<5; ii++){
                                    mode = (pulldown_win_mode>>(ii*4))&0xf;
                                    if(mode==1){
                                        if(di_buf->di_buf_dup_p[1]->pulldown_mode == 0){
                                            if((di_buf->di_buf_dup_p[1]->win_pd_info[ii].field_diff_num*win_pd_th[ii].field_diff_num_th) >=
                                                pd_win_prop[ii].pixels_num){
                                                break;
                                            }
                                        }
                                        else{
                                            if((di_buf->di_buf_dup_p[2]->win_pd_info[ii].field_diff_num*win_pd_th[ii].field_diff_num_th) >=
                                                pd_win_prop[ii].pixels_num){
                                                break;
                                            }
                                        }
										if ((ii!= 0) &&
                                            (ii!= 5) &&
                                            (ii!= 4) &&
                                            (pulldown_mode2 == 1) &&
                                            (((di_buf->di_buf_dup_p[2]->win_pd_info[ii+1].field_diff_num*100)  < di_buf->di_buf_dup_p[1]->win_pd_info[ii+1].field_diff_num) &&
                                             ((di_buf->di_buf_dup_p[2]->win_pd_info[ii-1].field_diff_num*100)  < di_buf->di_buf_dup_p[1]->win_pd_info[ii-1].field_diff_num) &&
                                             ((di_buf->di_buf_dup_p[2]->win_pd_info[ii  ].field_diff_num*100) >= di_buf->di_buf_dup_p[1]->win_pd_info[ii  ].field_diff_num)
                                            )
                                           )
                                           {
                                                di_print("out %x %06x %06x \n",
                                                         ii,
                                                         di_buf->di_buf_dup_p[2]->win_pd_info[ii].field_diff_num,
                                                         di_buf->di_buf_dup_p[1]->win_pd_info[ii].field_diff_num);
												pd_detect_rst =0;
												break;
											}
                                        if ((ii!= 0) &&
                                            (ii!= 5) &&
                                            (ii!= 4) &&
                                            (pulldown_mode2 == 0) &&
                                            ((di_buf->di_buf_dup_p[1]->win_pd_info[ii+1].field_diff_num*100)<di_buf->di_buf_dup_p[1]->win_pd_info[ii].field_diff_num) &&
                                            ((di_buf->di_buf_dup_p[1]->win_pd_info[ii-1].field_diff_num*100)<di_buf->di_buf_dup_p[1]->win_pd_info[ii].field_diff_num)
                                           )
                                           {
                                               pd_detect_rst =0;
                                               break;
                                           }
                                    }
                                    else if(mode==2){
                                        if( (pulldown_type == 0) &&
                                            (di_buf->di_buf_dup_p[1]->win_pd_info[ii].field_diff_num_pattern
                                                != di_buf->di_buf_dup_p[1]->field_pd_info.field_diff_num_pattern )
                                                ){
                                            break;
                                        }
                                        if( (pulldown_type == 1) &&
                                            (di_buf->di_buf_dup_p[1]->win_pd_info[ii].frame_diff_num_pattern
                                                != di_buf->di_buf_dup_p[1]->field_pd_info.frame_diff_num_pattern )
                                                ){
                                            break;
                                        }
                                    }
                                    else if(mode==3){
                                        if((di_buf->di_buf_dup_p[1]->win_pd_mode[ii]!= di_buf->di_buf_dup_p[1]->pulldown_mode)
                                            ||(pulldown_type!=win_pd_type[ii])){
                                            break;
                                        }
                                    }
                                }
                                if(ii<5){
                                    di_buf->pulldown_mode = -1;
                                    if(mode==1){
                                   //     printk("Deinterlace pulldown %s, win%d pd field_diff_num %08x/%08x is too big\n",
                                   //         (pulldown_type==0)?"2:2":"3:2", ii, pd_win_prop[ii].pixels_num,
                                   //         (di_buf->di_buf_dup_p[1]->pulldown_mode==0)? di_buf->di_buf_dup_p[1]->win_pd_info[ii].field_diff_num:di_buf->di_buf_dup_p[2]->win_pd_info[ii].field_diff_num
                                   //             );
                                    }
                                    else if(mode==2){
                                 //       printk("Deinterlace pulldown %s, win%d pd pattern %08x is different from field pd pattern %08x\n",
                                 //           (pulldown_type==0)?"2:2":"3:2", ii,
                                 //           (pulldown_type==0)? di_buf->di_buf_dup_p[1]->win_pd_info[ii].field_diff_num_pattern:di_buf->di_buf_dup_p[1]->win_pd_info[ii].frame_diff_num_pattern,
                                 //           (pulldown_type==0)? di_buf->di_buf_dup_p[1]->field_pd_info.field_diff_num_pattern:di_buf->di_buf_dup_p[1]->field_pd_info.frame_diff_num_pattern
                                 //              );
                                    }
                                    else{
                                 //      printk("Deinterlace pulldown, win%d pd type (%d, %d) is different from field pd type (%d, %d)\n",
                                 //           ii, di_buf->di_buf_dup_p[1]->win_pd_mode[ii], win_pd_type[ii],
                                 //           di_buf->di_buf_dup_p[1]->pulldown_mode, pulldown_type);
                                    }
                                }
                            }
                            /*

                            if(di_buf->pulldown_mode>=0)
                                 printk("%s pulldown\n", (pulldown_type==0)?"2:2":"m:n");

                            if((di_buf->vframe->tvin_sig_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
                                    && (pulldown_type == 1)){
                                        //m:n not do
                                     di_buf->pulldown_mode = -1;
                                     printk("m:n ignore\n");
                            }

                            if((di_buf->vframe->tvin_sig_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)
                                    && (pulldown_type == 0)){
                                        //2:2 not do
                                     di_buf->pulldown_mode = -1;
                                     printk("2:2 ignore\n");

                            }
                            */
                            if(di_buf->pulldown_mode != -1){
                                pulldown_count++;
                            }
#if defined(CONFIG_ARCH_MESON2)
                           if(di_buf->vframe->source_type == VFRAME_SOURCE_TYPE_TUNER)                      {
                                     di_buf->pulldown_mode = -1;
                                     //printk("2:2 ignore\n");
                            }
#endif

                        }
                    }
#ifdef FORCE_BOB_SUPPORT
        /*added for hisense*/
                    if(pd_enable){
                    if(pulldown_mode_hise == 2)
                        force_bob_flag = 1;
                    else
                        force_bob_flag = 0;
                    }
                    if(force_bob_flag!=0){
                        di_buf->vframe->type = VIDTYPE_PROGRESSIVE| VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
                        if((force_bob_flag==1)||(force_bob_flag==2)){
                            di_buf->vframe->duration<<=1;
                        }
                        di_buf->vframe->private_data = di_buf;
                        if(di_buf->di_buf_dup_p[1]->new_format_flag){ //if(di_buf->di_buf_dup_p[1]->post_proc_flag == 2){
                            di_buf->vframe->early_process_fun = de_post_disable_fun;
                        }
                        else{
                            di_buf->vframe->early_process_fun = do_nothing_fun;
                        }

                        di_buf->vframe->process_fun = de_post_process_force_bob;
                        inc_post_ref_count(di_buf);

                        di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
                        di_buf->di_buf[1] = NULL;
                        list_del(&di_buf->di_buf[0]->list);

                        raw_local_save_flags(fiq_flag);
                        local_fiq_disable();
                        list_add_tail(&di_buf->list, &post_ready_list_head);
                        if((frame_count<start_frame_drop_count)||
                            (((di_buf->di_buf_dup_p[1]->vframe->type & VIDTYPE_TYPEMASK)==VIDTYPE_INTERLACE_TOP)
                                && ((force_bob_flag&1)==0))||
                            (((di_buf->di_buf_dup_p[1]->vframe->type & VIDTYPE_TYPEMASK)==VIDTYPE_INTERLACE_BOTTOM)
                                && ((force_bob_flag&2)==0))){
                            recycle_vframe_type_post(di_buf);
#ifdef DI_DEBUG
                            recycle_vframe_type_post_print(di_buf, __func__);
#endif
                        }
                        frame_count++;
                        raw_local_irq_restore(fiq_flag);
                    }
                    else{
#endif
                    di_buf->vframe->type = VIDTYPE_PROGRESSIVE| VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
                    if(di_buf->di_buf_dup_p[1]->new_format_flag){ //if(di_buf->di_buf_dup_p[1]->post_proc_flag == 2){
                        di_buf->vframe->early_process_fun = de_post_disable_fun;
                    }
                    else{
                        di_buf->vframe->early_process_fun = do_nothing_fun;
                    }
                    if(di_buf->pulldown_mode >= 0){
                        di_buf->vframe->process_fun = de_post_process_pd;
                        inc_post_ref_count(di_buf);
                    }
                    else{
                        di_buf->vframe->process_fun = de_post_process;
                        inc_post_ref_count(di_buf);
                    }
                    di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
                    di_buf->di_buf[1] = NULL;
                    list_del(&di_buf->di_buf[0]->list);

                    raw_local_save_flags(fiq_flag);
                    local_fiq_disable();
                    list_add_tail(&di_buf->list, &post_ready_list_head);
                    if(frame_count<start_frame_drop_count){
                        recycle_vframe_type_post(di_buf);
#ifdef DI_DEBUG
                        recycle_vframe_type_post_print(di_buf, __func__);
#endif
                    }
                    frame_count++;
                    raw_local_irq_restore(fiq_flag);
#ifdef FORCE_BOB_SUPPORT
                    }
#endif

#ifdef DI_DEBUG
                    di_print("%s <interlace>: ", __func__);
#endif
                }
                ret = 1;
            }
        }
        else{
            if(is_progressive(ready_di_buf->vframe)||
                ready_di_buf->type == VFRAME_TYPE_IN){
                raw_local_save_flags(fiq_flag);
                local_fiq_disable();
                di_buf = list_first_entry(&post_free_list_head, struct di_buf_s, list);
                list_del(&(di_buf->list));
                raw_local_irq_restore(fiq_flag);

                memcpy(di_buf->vframe, ready_di_buf->vframe, sizeof(vframe_t));
                di_buf->vframe->private_data = di_buf;

                if(frame_packing_mode == 1){
#if defined(CONFIG_ARCH_MESON2)
                    if(di_buf->vframe->trans_fmt == TVIN_TFMT_3D_FP){
                        if(di_buf->vframe->height == 2205){
                            di_buf->vframe->height = 1080;
                        }
                        if(di_buf->vframe->height == 1470){
                            di_buf->vframe->height = 720;
                        }
                    }
#endif
                }
                else{
                    if(di_buf->vframe->height > 2000){ //work around hw problem
                        di_buf->vframe->height = 2000;
                    }
                }
                if(ready_di_buf->new_format_flag){
                    di_buf->vframe->early_process_fun = de_post_disable_fun;
                }
                else{
                    di_buf->vframe->early_process_fun = do_nothing_fun;
                }
                di_buf->vframe->process_fun = NULL;
                di_buf->di_buf[0] = ready_di_buf;
                di_buf->di_buf[1] = NULL;
                list_del(&ready_di_buf->list);

                raw_local_save_flags(fiq_flag);
                local_fiq_disable();
                list_add_tail(&di_buf->list, &post_ready_list_head);
                if(frame_count<start_frame_drop_count){
                    recycle_vframe_type_post(di_buf);
#ifdef DI_DEBUG
                    recycle_vframe_type_post_print(di_buf, __func__);
#endif
                }
                frame_count++;
                raw_local_irq_restore(fiq_flag);

#ifdef DI_DEBUG
                di_print("%s <prog by frame>: ", __func__);
#endif
                ret = 1;
            }
            else if(ready_count>=2){
                unsigned char prog_tb_field_proc_type = (prog_proc_config>>1)&0x3;
                raw_local_save_flags(fiq_flag);
                local_fiq_disable();
                di_buf = list_first_entry(&post_free_list_head, struct di_buf_s, list);
                list_del(&(di_buf->list));
                raw_local_irq_restore(fiq_flag);

                i = 0;
                list_for_each_entry_safe(p, ptmp, &pre_ready_list_head, list) {
                    di_buf->di_buf_dup_p[i++] = p;
                    if(i>=2)
                        break;
                }
                memcpy(di_buf->vframe, di_buf->di_buf_dup_p[0]->vframe, sizeof(vframe_t));
                di_buf->vframe->private_data = di_buf;

                if((prog_tb_field_proc_type == 1)||(real_buf_mgr_mode==1)){
                    di_buf->vframe->type = VIDTYPE_PROGRESSIVE| VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
                    if(di_buf->di_buf_dup_p[0]->new_format_flag){
                        di_buf->vframe->early_process_fun = de_post_disable_fun;
                    }
                    else{
                        di_buf->vframe->early_process_fun = do_nothing_fun;
                    }
                    di_buf->vframe->process_fun = de_post_process_prog;
                }
                else if(prog_tb_field_proc_type == 0){
                    //do weave by vpp
                    di_buf->vframe->type = VIDTYPE_PROGRESSIVE| VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE; // top and bot are in separate buffer, do not set VIDTYPE_VIU_FIELD
                    if((di_buf->di_buf_dup_p[0]->new_format_flag)
                        ||(READ_MPEG_REG(DI_IF1_GEN_REG)&1)){
                        di_buf->vframe->early_process_fun = de_post_disable_fun;
                    }
                    else{
                        di_buf->vframe->early_process_fun = do_nothing_fun;
                    }
                    di_buf->vframe->process_fun = NULL;
                    di_buf->vframe->canvas0Addr = di_buf->di_buf_dup_p[0]->nr_canvas_idx; //top
                    di_buf->vframe->canvas1Addr = di_buf->di_buf_dup_p[1]->nr_canvas_idx; //bot
                }
                else{
                    di_buf->vframe->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
                    di_buf->vframe->height >>= 1;
                    if((di_buf->di_buf_dup_p[0]->new_format_flag)
                        ||(READ_MPEG_REG(DI_IF1_GEN_REG)&1)){
                        di_buf->vframe->early_process_fun = de_post_disable_fun;
                    }
                    else{
                        di_buf->vframe->early_process_fun = do_nothing_fun;
                    }
                    if(prog_tb_field_proc_type == 2){
                    di_buf->vframe->canvas0Addr = di_buf->di_buf_dup_p[0]->nr_canvas_idx; //top
                        di_buf->vframe->canvas1Addr = di_buf->di_buf_dup_p[0]->nr_canvas_idx;
                    }
                    else{
                        di_buf->vframe->canvas0Addr = di_buf->di_buf_dup_p[1]->nr_canvas_idx; //top
                        di_buf->vframe->canvas1Addr = di_buf->di_buf_dup_p[1]->nr_canvas_idx;
                    }
                }

                di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
                di_buf->di_buf[1] = di_buf->di_buf_dup_p[1];
                list_del(&di_buf->di_buf[0]->list);
                list_del(&di_buf->di_buf[1]->list);

                raw_local_save_flags(fiq_flag);
                local_fiq_disable();
                list_add_tail(&di_buf->list, &post_ready_list_head);
                if(frame_count<start_frame_drop_count){
                    recycle_vframe_type_post(di_buf);
#ifdef DI_DEBUG
                    recycle_vframe_type_post_print(di_buf, __func__);
#endif
                }
                frame_count++;
                raw_local_irq_restore(fiq_flag);
#ifdef DI_DEBUG
                di_print("%s <prog by field>: ", __func__);
#endif
                ret = 1;
            }
        }
#ifdef DI_DEBUG
        if(di_buf){
            di_print("%s[%d](", vframe_type_name[di_buf->type], di_buf->index);
            for(i=0; i< 2; i++){
                if(di_buf->di_buf[i])
                    di_print("%s[%d],",vframe_type_name[di_buf->di_buf[i]->type], di_buf->di_buf[i]->index);
            }
            di_print(")(vframe type %x dur %d)", di_buf->vframe->type, di_buf->vframe->duration);
            if(di_buf->di_buf_dup_p[1]&&(di_buf->di_buf_dup_p[1]->post_proc_flag == 3)){
                di_print("=> recycle_list\n");
            }
            else{
                di_print("=> post_ready_list\n");
            }
        }
#endif
    }
    return ret;
}

/*
di task
*/
static void di_process(void)
{
    ulong flags;
    ulong fiq_flag;
    vframe_t * vframe;
	/* add for di Reg re-init */
	//di_set_para_by_tvinfo(vframe);
        if((di_pre_stru.unreg_req_flag||di_pre_stru.force_unreg_req_flag||di_pre_stru.disable_req_flag)&&
            (di_pre_stru.pre_de_busy==0)){
            //printk("===unreg_req_flag\n");
            if(di_pre_stru.force_unreg_req_flag||di_pre_stru.disable_req_flag){
#ifdef DI_DEBUG
                di_print("%s: force_unreg\n", __func__);
#endif
                if(bypass_state == 0){
                	vf_notify_receiver(VFM_NAME,VFRAME_EVENT_PROVIDER_FORCE_BLACKOUT,NULL);
                }
                goto unreg;
            }
            else if((real_buf_mgr_mode==1)&&(is_bypass()==0)){
                if(init_flag){
#ifdef DI_DEBUG
                    di_print("%s: di_clean_in_buf\n", __func__);
#endif
                    spin_lock_irqsave(&plist_lock, flags);
                    raw_local_save_flags(fiq_flag);
                    local_fiq_disable();
    
                    di_clean_in_buf();
                    raw_local_irq_restore(fiq_flag);
                    spin_unlock_irqrestore(&plist_lock, flags);
                }
            }
            else{
unreg:                
                init_flag = 0;
                raw_local_save_flags(fiq_flag);
                local_fiq_disable();

                if(bypass_state == 0){
	                DisableVideoLayer();
  							}
  							              
                vf_unreg_provider(&di_vf_prov);
                raw_local_irq_restore(fiq_flag);
                spin_lock_irqsave(&plist_lock, flags);

                raw_local_save_flags(fiq_flag);
                local_fiq_disable();
#ifdef DI_DEBUG
                di_print("%s: di_uninit_buf\n", __func__);
#endif
                di_uninit_buf();
                raw_local_irq_restore(fiq_flag);

                spin_unlock_irqrestore(&plist_lock, flags);
                
                di_pre_stru.force_unreg_req_flag = 0;
                di_pre_stru.disable_req_flag = 0;
            }
            di_pre_stru.unreg_req_flag = 0;
        }

        if(init_flag == 0){
            vframe = vf_peek(VFM_NAME);
            if(vframe){
		/* add for di Reg re-init */
#if defined(CONFIG_ARCH_MESON2)
		di_set_para_by_tvinfo(vframe);
#endif
#ifdef DI_DEBUG
                di_print("%s: vframe come => di_init_buf\n", __func__);
#endif
                if(is_progressive(vframe)&&is_from_vdin(vframe)&&(prog_proc_config&0x1)&&(real_buf_mgr_mode==0)){
                    spin_lock_irqsave(&plist_lock, flags);

                    raw_local_save_flags(fiq_flag);
                    local_fiq_disable();
                    di_init_buf(vframe->width, vframe->height, 1);
                    raw_local_irq_restore(fiq_flag);

                    spin_unlock_irqrestore(&plist_lock, flags);
                }
                else{
                    spin_lock_irqsave(&plist_lock, flags);

                    raw_local_save_flags(fiq_flag);
                    local_fiq_disable();
                    di_init_buf(default_width, default_height, 0);
                    raw_local_irq_restore(fiq_flag);

                    spin_unlock_irqrestore(&plist_lock, flags);
                }
                vf_provider_init(&di_vf_prov, VFM_NAME, &deinterlace_vf_provider, NULL);
                vf_reg_provider(&di_vf_prov);
                reset_pulldown_state();
                init_flag = 1;
            }
        }
        else{
#if 1
//for vdin, need re-init buffers
            if((real_buf_mgr_mode==1)&&(is_bypass()==0)){
            }
            else{
                vframe = vf_peek(VFM_NAME);
                if(vframe &&
                    is_source_change(vframe)
                    &&is_from_vdin(vframe)){
                    printk("===source_change\n");

                    spin_lock_irqsave(&plist_lock, flags);
                    raw_local_save_flags(fiq_flag);
                    local_fiq_disable();

                    vf_unreg_provider(&di_vf_prov);
#ifdef DI_DEBUG
                    di_print("%s: di_uninit_buf 2\n", __func__);
#endif
                    di_uninit_buf();

                    if(is_progressive(vframe)&&is_from_vdin(vframe)&&(prog_proc_config&0x1)&&(real_buf_mgr_mode==0)){
                        di_init_buf(vframe->width, vframe->height, 1);
                    }
                    else{
                        di_init_buf(default_width, default_height, 0);
                    }

                    /* add for di Reg re-init */
                    //di_set_para_by_tvinfo(vframe);
                    vf_reg_provider(&di_vf_prov);

                    raw_local_irq_restore(fiq_flag);
                    spin_unlock_irqrestore(&plist_lock, flags);
                }
            }

#endif
        }

        if(init_flag){
            spin_lock_irqsave(&plist_lock, flags);
            if(di_pre_stru.pre_de_process_done){
                pre_de_done_buf_config();

                WRITE_MPEG_REG(DI_PRE_CTRL, 0x3 << 30); //disable, and reset
                di_pre_stru.pre_de_process_done = 0;
#if 1
//for debug
                if(di_pre_stru.process_count==0){
                    if(prog_proc_config&0x10){
                        if(prog_proc_config&0x1){
                            di_pre_stru.prog_proc_type = 1;
                        }
                        else{
                            di_pre_stru.prog_proc_type = 0;
                        }
                        prog_proc_config &= (~0x10);
                    }
                }
#endif
            }

            raw_local_save_flags(fiq_flag);
            local_fiq_disable();
            while(check_recycle_buf()&1){};
            raw_local_irq_restore(fiq_flag);


            if((di_pre_stru.pre_de_busy==0) && pre_de_buf_config()){
                pre_de_process();
            }

            while(process_post_vframe()){};

            spin_unlock_irqrestore(&plist_lock, flags);
        }
}

void di_timer_handle(struct work_struct *work)
{
    if(di_pre_stru.pre_de_busy){
        di_pre_stru.pre_de_busy_timer_count++;
        if(di_pre_stru.pre_de_busy_timer_count>=100){
            di_pre_stru.pre_de_busy_timer_count = 0;
            di_pre_stru.pre_de_busy = 0;
            pr_info("******* DI ********** wait pre_de_irq timeout\n");
#if defined(CONFIG_ARCH_MESON2)
            set_foreign_affairs(FOREIGN_AFFAIRS_01);
            if(pre_de_irq_check){
	              pr_info("DI: Force RESET\n");
	              WRITE_MPEG_REG(WATCHDOG_RESET, 0);
                WRITE_MPEG_REG(WATCHDOG_TC,(1<<WATCHDOG_ENABLE_BIT)|(50));
            }
#endif
        }
    }

#ifdef RUN_DI_PROCESS_IN_TIMER
    {
        int i;
        for(i=0; i<10; i++){
            if(active_flag){
                di_process();
            }
        }
    }
#endif
}

static int di_task_handle(void *data)
{
    while (1)
    {

        down_interruptible(&di_sema);
        if(active_flag){
            di_process();
        }
    }

    return 0;

}
/*
provider/receiver interface

*/

static int di_receiver_event_fun(int type, void* data, void* arg)
{
    int i;
    ulong flags;
    if(type == VFRAME_EVENT_PROVIDER_UNREG){
#ifdef DI_DEBUG
        di_print("%s: vf_notify_receiver unreg\n", __func__);
#endif
        di_pre_stru.unreg_req_flag = 1;
        provider_vframe_level = 0;
        trigger_pre_di_process('n');
    }
    else if(type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG){
#ifdef DI_DEBUG
        di_print("%s: vf_notify_receiver ligth unreg\n", __func__);
#endif
        provider_vframe_level = 0;
       spin_lock_irqsave(&plist_lock, flags);
        for(i=0; i<MAX_IN_BUF_NUM; i++){
#ifdef DI_DEBUG
            if(vframe_in[i]){
                printk("DI:clear vframe_in[%d]\n", i);    
            }
#endif
            vframe_in[i] = NULL;
        }
       spin_unlock_irqrestore(&plist_lock, flags);
    }
    else if(type == VFRAME_EVENT_PROVIDER_VFRAME_READY){
#ifdef DI_DEBUG
        di_print("%s: vframe ready\n", __func__);
#endif
        provider_vframe_level++;
        trigger_pre_di_process('r');
    }
    else if(type == VFRAME_EVENT_PROVIDER_QUREY_STATE){
        int in_buf_num = 0;
        for(i=0; i<MAX_IN_BUF_NUM; i++){
            if(vframe_in[i]!=NULL){
                in_buf_num++;
            }
        }
        if(bypass_state==1){
            if(in_buf_num>1){
                return RECEIVER_ACTIVE ;
            }else{
                return RECEIVER_INACTIVE;
            }
        }
        else{
            if(in_buf_num>0){
                return RECEIVER_ACTIVE ;
            }else{
                return RECEIVER_INACTIVE;
            }
        }
    }
    return 0;
}

static vframe_t *di_vf_peek(void* arg)
{
    vframe_t* vframe_ret = NULL;
    di_buf_t* di_buf = NULL;
    if(init_flag == 0)
        return NULL;
    if((run_flag == DI_RUN_FLAG_PAUSE)||
        (run_flag == DI_RUN_FLAG_STEP_DONE))
        return NULL;

    if((disp_frame_count==0)&&(is_bypass()==0)){
        int ready_count = list_count(&post_ready_list_head, 0);
        if(ready_count>start_frame_hold_count){
           di_buf = list_first_entry(&post_ready_list_head, struct di_buf_s, list);
           vframe_ret = di_buf->vframe;
        }
    }
    else{
    if(!list_empty(&post_ready_list_head)){
       di_buf = list_first_entry(&post_ready_list_head, struct di_buf_s, list);
       vframe_ret = di_buf->vframe;
    }
    }
#ifdef DI_DEBUG
    if(vframe_ret){
         di_print("%s: %s[%d]:%x\n", __func__, vframe_type_name[di_buf->type], di_buf->index, vframe_ret);
    }
#endif
    return vframe_ret;
}

static vframe_t *di_vf_get(void* arg)
{
    vframe_t* vframe_ret = NULL;
    di_buf_t* di_buf = NULL;
    ulong flags;

    if(init_flag == 0)
        return NULL;

    if((run_flag == DI_RUN_FLAG_PAUSE)||
        (run_flag == DI_RUN_FLAG_STEP_DONE))
        return NULL;

    if((disp_frame_count==0)&&(is_bypass()==0)){
        int ready_count = list_count(&post_ready_list_head, 0);
        if(ready_count>start_frame_hold_count){
            goto get_vframe;
        }
    }
    else if (!list_empty(&post_ready_list_head)){
get_vframe:
        log_buffer_state("get");
       if(receiver_is_amvideo == 0){
           spin_lock_irqsave(&plist_lock, flags);
       }
       di_buf = list_first_entry(&post_ready_list_head, struct di_buf_s, list);
       list_del(&di_buf->list);
       list_add_tail(&di_buf->list, &display_list_head); //add it into display_list
       if(receiver_is_amvideo == 0){
           spin_unlock_irqrestore(&plist_lock, flags);
       }
       vframe_ret = di_buf->vframe;
       disp_frame_count++;
       if(run_flag == DI_RUN_FLAG_STEP){
            run_flag = DI_RUN_FLAG_STEP_DONE;
       }
    }
#ifdef DI_DEBUG
     if(vframe_ret){
         di_print("%s: %s[%d]:%x\n", __func__, vframe_type_name[di_buf->type], di_buf->index, vframe_ret);
     }
#endif
    return vframe_ret;
}

static void di_vf_put(vframe_t *vf, void* arg)
{
    di_buf_t* di_buf = (di_buf_t*)vf->private_data;
    ulong flags;
    if(init_flag == 0){
#ifdef DI_DEBUG
        di_print("%s: %x\n", __func__, vf);
#endif
        return;
    }
    log_buffer_state("put");

    if(di_buf->type == VFRAME_TYPE_POST){
        if(receiver_is_amvideo == 0){
           spin_lock_irqsave(&plist_lock, flags);
        }
        recycle_vframe_type_post(di_buf);
        if(receiver_is_amvideo == 0){
            spin_unlock_irqrestore(&plist_lock, flags);
        }
#ifdef DI_DEBUG
        recycle_vframe_type_post_print(di_buf, __func__);
#endif
    }
    else{
        if(receiver_is_amvideo == 0){
            spin_lock_irqsave(&plist_lock, flags);
        }
        list_add_tail(&(di_buf->list), &recycle_list_head);
        if(receiver_is_amvideo == 0){
            spin_unlock_irqrestore(&plist_lock, flags);
        }
#ifdef DI_DEBUG
        di_print("%s: %s[%d] =>recycle_list\n", __func__, vframe_type_name[di_buf->type], di_buf->index);
#endif
    }

    trigger_pre_di_process('p');
}

static int di_event_cb(int type, void *data, void *private_data)
{
    int i;
    if(type == VFRAME_EVENT_RECEIVER_FORCE_UNREG){
#ifdef DI_DEBUG
        di_print("%s: VFRAME_EVENT_RECEIVER_FORCE_UNREG\n", __func__);
#endif
        di_pre_stru.force_unreg_req_flag = 1;
        provider_vframe_level = 0;
        trigger_pre_di_process('f');
        while(di_pre_stru.force_unreg_req_flag){
            msleep(1);    
        }
    }
    return 0;        
}

/*****************************
*    di driver file_operations
*
******************************/
static int di_open(struct inode *node, struct file *file)
{
    di_dev_t *di_in_devp;

    /* Get the per-device structure that contains this cdev */
    di_in_devp = container_of(node->i_cdev, di_dev_t, cdev);
    file->private_data = di_in_devp;

    return 0;

}


static int di_release(struct inode *node, struct file *file)
{
    //di_dev_t *di_in_devp = file->private_data;

    /* Reset file pointer */

    /* Release some other fields */
    /* ... */
    return 0;
}



static int di_ioctl(struct inode *node, struct file *file, unsigned int cmd,   unsigned long args)
{
    int   r = 0;
    switch (cmd) {
        default:
            break;
    }
    return r;
}

const static struct file_operations di_fops = {
    .owner    = THIS_MODULE,
    .open     = di_open,
    .release  = di_release,
    .ioctl    = di_ioctl,
};

static int di_probe(struct platform_device *pdev)
{
    int r;
    struct resource *mem;
    int buf_num_avail;
    pr_dbg("di_probe\n");

    r = alloc_chrdev_region(&di_id, 0, DI_COUNT, DEVICE_NAME);
    if (r < 0) {
        pr_error("Can't register major for di device\n");
        return r;
    }
    di_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(di_class))
    {
        unregister_chrdev_region(di_id, DI_COUNT);
        return -1;
    }

    memset(&di_device, 0, sizeof(di_dev_t));

    cdev_init(&(di_device.cdev), &di_fops);
    di_device.cdev.owner = THIS_MODULE;
    cdev_add(&(di_device.cdev), di_id, DI_COUNT);

    di_device.devt = MKDEV(MAJOR(di_id), 0);
    di_device.dev = device_create(di_class, &pdev->dev, di_device.devt, &di_device, "di%d", 0); //kernel>=2.6.27

    if (di_device.dev == NULL) {
        pr_error("device_create create error\n");
        class_destroy(di_class);
        r = -EEXIST;
        return r;
    }

    device_create_file(di_device.dev, &dev_attr_config);
    device_create_file(di_device.dev, &dev_attr_debug);
    device_create_file(di_device.dev, &dev_attr_log);
    device_create_file(di_device.dev, &dev_attr_parameters);
    device_create_file(di_device.dev, &dev_attr_status);

    if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0)))
    {
    	  pr_error("\ndeinterlace memory resource undefined.\n");
        return -EFAULT;
    }

	    // declare deinterlace memory
	  di_print("Deinterlace memory: start = 0x%x, end = 0x%x, size=0x%x\n", mem->start, mem->end, mem->end-mem->start);

    di_mem_start = mem->start;
    di_mem_size = mem->end - mem->start + 1;
    init_flag = 0;

    /* set start_frame_hold_count base on buffer size */
    buf_num_avail = di_mem_size/(default_width*(default_height+8)*5/4);
    if(buf_num_avail > MAX_LOCAL_BUF_NUM){
        buf_num_avail = MAX_LOCAL_BUF_NUM;
    }
    if(buf_num_avail > 6){
        start_frame_hold_count = buf_num_avail-6;
    }
    if(start_frame_hold_count > 4){
        start_frame_hold_count = 4;
    }
    /**/

    vf_receiver_init(&di_vf_recv, VFM_NAME, &di_vf_receiver, NULL);
    vf_reg_receiver(&di_vf_recv);
    active_flag = 1;
     //data32 = (*P_A9_0_IRQ_IN1_INTR_STAT_CLR);
    r = request_irq(INT_DEINTERLACE, &de_irq,
                    IRQF_SHARED, "deinterlace",
                    (void *)"deinterlace");
    Wr(A9_0_IRQ_IN1_INTR_MASK, Rd(A9_0_IRQ_IN1_INTR_MASK)|(1<<14));
    init_MUTEX(&di_sema);
    di_sema_init_flag=1;
#ifdef FIQ_VSYNC
	fiq_handle_item.handle=di_vf_put_isr;
	fiq_handle_item.key=(u32)di_vf_put_isr;
	fiq_handle_item.name="di_vf_put_isr";
	if(register_fiq_bridge_handle(&fiq_handle_item)){
    pr_dbg("%s: register_fiq_bridge_handle fail\n", __func__);
	}
#endif
    reset_di_para();
    di_hw_init();

    /* timer */
    INIT_WORK(&di_pre_work, di_timer_handle);
    init_timer(&di_pre_timer);
    di_pre_timer.data = (ulong) & di_pre_timer;
    di_pre_timer.function = di_pre_timer_cb;
    di_pre_timer.expires = jiffies + DI_PRE_INTERVAL;
    add_timer(&di_pre_timer);
    /**/
#if (!(defined RUN_DI_PROCESS_IN_IRQ))&&(!(defined RUN_DI_PROCESS_IN_TIMER))
    di_device.task = kthread_run(di_task_handle, &di_device, "kthread_di");
#endif
    return r;
}

static int di_remove(struct platform_device *pdev)
{
    di_hw_uninit();
    di_device.di_event = 0xff;
    kthread_stop(di_device.task);
#ifdef FIQ_VSYNC
  	unregister_fiq_bridge_handle(&fiq_handle_item);
#endif
    vf_unreg_provider(&di_vf_prov);
    vf_unreg_receiver(&di_vf_recv);

    di_uninit_buf();
    /* Remove the cdev */
    device_remove_file(di_device.dev, &dev_attr_config);
    device_remove_file(di_device.dev, &dev_attr_debug);
    device_remove_file(di_device.dev, &dev_attr_log);
    device_remove_file(di_device.dev, &dev_attr_parameters);
    device_remove_file(di_device.dev, &dev_attr_status);

    cdev_del(&di_device.cdev);

    device_destroy(di_class, di_id);

    class_destroy(di_class);

    unregister_chrdev_region(di_id, DI_COUNT);
    return 0;
}

static struct platform_driver di_driver = {
    .probe      = di_probe,
    .remove     = di_remove,
    .driver     = {
        .name   = DEVICE_NAME,
		    .owner	= THIS_MODULE,
    }
};

static struct platform_device* deinterlace_device = NULL;


static int  __init di_init(void)
{
    if(boot_init_flag&INIT_FLAG_NOT_LOAD)
        return 0;

    pr_dbg("di_init\n");
#if 0
	  deinterlace_device = platform_device_alloc(DEVICE_NAME,0);
    if (!deinterlace_device) {
        pr_error("failed to alloc deinterlace_device\n");
        return -ENOMEM;
    }

    if(platform_device_add(deinterlace_device)){
        platform_device_put(deinterlace_device);
        pr_error("failed to add deinterlace_device\n");
        return -ENODEV;
    }
    if (platform_driver_register(&di_driver)) {
        pr_error("failed to register di module\n");

        platform_device_del(deinterlace_device);
        platform_device_put(deinterlace_device);
        return -ENODEV;
    }
#else
    if (platform_driver_register(&di_driver)) {
        di_print("failed to register di module\n");
        return -ENODEV;
    }
#endif
    return 0;
}




static void __exit di_exit(void)
{
    pr_dbg("di_exit\n");
    platform_driver_unregister(&di_driver);
    platform_device_unregister(deinterlace_device);
    deinterlace_device = NULL;
    return ;
}

MODULE_PARM_DESC(bypass_hd, "\n bypass_hd \n");
module_param(bypass_hd, int, 0664);

MODULE_PARM_DESC(bypass_prog, "\n bypass_prog \n");
module_param(bypass_prog, int, 0664);

MODULE_PARM_DESC(bypass_all, "\n bypass_all \n");
module_param(bypass_all, int, 0664);

MODULE_PARM_DESC(pulldown_detect, "\n pulldown_detect \n");
module_param(pulldown_detect, int, 0664);

MODULE_PARM_DESC(prog_proc_config, "\n prog_proc_config \n");
module_param(prog_proc_config, int, 0664);

MODULE_PARM_DESC(skip_wrong_field, "\n skip_wrong_field \n");
module_param(skip_wrong_field, int, 0664);

MODULE_PARM_DESC(frame_count, "\n frame_count \n");
module_param(frame_count, int, 0664);

MODULE_PARM_DESC(start_frame_drop_count, "\n start_frame_drop_count \n");
module_param(start_frame_drop_count, int, 0664);

MODULE_PARM_DESC(start_frame_hold_count, "\n start_frame_hold_count \n");
module_param(start_frame_hold_count, int, 0664);

module_init(di_init);
module_exit(di_exit);

MODULE_PARM_DESC(same_w_r_canvas_count, "\n canvas of write and read are same \n");
module_param(same_w_r_canvas_count, long, 0664);

MODULE_PARM_DESC(same_field_top_count, "\n same top field \n");
module_param(same_field_top_count, long, 0664);

MODULE_PARM_DESC(same_field_bot_count, "\n same bottom field \n");
module_param(same_field_bot_count, long, 0664);

MODULE_PARM_DESC(same_field_source_flag_th, "\n same_field_source_flag_th \n");
module_param(same_field_source_flag_th, int, 0664);

MODULE_PARM_DESC(pulldown_count, "\n pulldown_count \n");
module_param(pulldown_count, long, 0664);

MODULE_PARM_DESC(pulldown_buffer_mode, "\n pulldown_buffer_mode \n");
module_param(pulldown_buffer_mode, long, 0664);

MODULE_PARM_DESC(pulldown_win_mode, "\n pulldown_win_mode \n");
module_param(pulldown_win_mode, long, 0664);

MODULE_PARM_DESC(frame_packing_mode, "\n frame_packing_mode \n");
module_param(frame_packing_mode, int, 0664);

MODULE_PARM_DESC(di_log_flag, "\n di log flag \n");
module_param(di_log_flag, int, 0664);

MODULE_PARM_DESC(buf_state_log_threshold, "\n buf_state_log_threshold \n");
module_param(buf_state_log_threshold, int, 0664);

MODULE_PARM_DESC(buf_mgr_mode, "\n buf_mgr_mode\n");
module_param(buf_mgr_mode, uint, 0664);

MODULE_PARM_DESC(buf_mgr_mode_mask, "\n buf_mgr_mode_mask\n");
module_param(buf_mgr_mode_mask, uint, 0664);

MODULE_PARM_DESC(pre_de_irq_check, "\n pre_de_irq_check\n");
module_param(pre_de_irq_check, uint, 0664);

MODULE_PARM_DESC(bypass_state, "\n bypass_state\n");
module_param(bypass_state, uint, 0664);

MODULE_PARM_DESC(force_bob_flag, "\n force_bob_flag\n");
module_param(force_bob_flag, uint, 0664);

MODULE_DESCRIPTION("AMLOGIC HDMI TX driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");


static char* next_token_ex(char* seperator, char *buf, unsigned size, unsigned offset, unsigned *token_len, unsigned *token_offset)
{ /* besides characters defined in seperator, '\"' are used as seperator; and any characters in '\"' will not act as seperator */
	char *pToken = NULL;
    char last_seperator = 0;
    char trans_char_flag = 0;
    if(buf){
    	for (;offset<size;offset++){
    	    int ii=0;
    	    char ch;
            if (buf[offset] == '\\'){
                trans_char_flag = 1;
                continue;
            }
    	    while(((ch=seperator[ii++])!=buf[offset])&&(ch)){
    	    }
    	    if (ch){
                if (!pToken){
    	            continue;
                }
    	        else {
                    if (last_seperator != '"'){
    	                *token_len = (unsigned)(buf + offset - pToken);
    	                *token_offset = offset;
    	                return pToken;
    	            }
    	        }
            }
    	    else if (!pToken)
            {
                if (trans_char_flag&&(buf[offset] == '"'))
                    last_seperator = buf[offset];
    	        pToken = &buf[offset];
            }
            else if ((trans_char_flag&&(buf[offset] == '"'))&&(last_seperator == '"')){
                *token_len = (unsigned)(buf + offset - pToken - 2);
                *token_offset = offset + 1;
                return pToken + 1;
            }
            trans_char_flag = 0;
        }
        if (pToken) {
            *token_len = (unsigned)(buf + offset - pToken);
            *token_offset = offset;
        }
    }
	return pToken;
}

static  int __init di_boot_para_setup(char *s)
{
    char separator[]={' ',',',';',0x0};
    char *token;
    unsigned token_len, token_offset, offset=0;
    int size=strlen(s);
    do{
        token=next_token_ex(separator, s, size, offset, &token_len, &token_offset);
        if(token){
            if((token_len==3) && (strncmp(token, "off", token_len)==0)){
                boot_init_flag|=INIT_FLAG_NOT_LOAD;
            }
        }
        offset=token_offset;
    }while(token);
    return 0;
}

__setup("di=",di_boot_para_setup);


vframe_t* get_di_inp_vframe(void)
{
    vframe_t* vframe = NULL;
    if(di_pre_stru.di_inp_buf){
        vframe = di_pre_stru.di_inp_buf->vframe;
    }
    return vframe;
}
