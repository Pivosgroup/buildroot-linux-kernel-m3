/*
 * Amlogic M1 
 * frame buffer driver-----------HDMI_TX
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
#define HDMI_DEBUG()  printk("HDMI DEBUG: %s [%d]\n", __FUNCTION__, __LINE__)

#ifndef AVOS


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
#include <linux/proc_fs.h> 
#include <asm/uaccess.h>
#include <mach/am_regs.h>
#include <mach/clock.h>

#include <linux/osd/osd_dev.h>
#include <linux/switch.h>
#else

#include "includes.h"
#include "os_internal.h"
#include "os_cpu.h"
#include "prf.h"
#include "ioapi.h"
#include <chipsupport/chipsupport.h>
#include <os/extend/interrupt.h>
#include <Drivers/include/peripheral_reg.h>
#include <Drivers/include/isa_reg.h>
#include <Drivers/include/mpeg_reg.h>
#include <interrupt.h>
#include "displaydev.h"
#include "policy.h"
typedef struct{
    char name[32];
}vinfo_t;

vinfo_t lvideo_info;
#endif


#include "hdmi_info_global.h"
#include "hdmi_tx_cec.h"
#include "hdmi_tx_module.h"

#ifndef AVOS
#define DEVICE_NAME "amhdmitx"
#define HDMI_TX_COUNT 32
#define HDMI_TX_POOL_NUM  6
#define HDMI_TX_RESOURCE_NUM 4


#ifdef DEBUG
#define pr_dbg(fmt, args...) printk(KERN_DEBUG "amhdmitx: " fmt, ## args)
#else
#define pr_dbg(fmt, args...)
#endif
#define pr_error(fmt, args...) printk(KERN_ERR "amhdmitx: " fmt, ## args)


static dev_t hdmitx_id;
static struct class *hdmitx_class;
static struct device *hdmitx_dev;
#endif

static hdmitx_dev_t hdmitx_device;
static struct switch_dev sdev = {	// android ics switch device
	.name = "hdmi",
	};	

//static HDMI_TX_INFO_t hdmi_info;
#define INIT_FLAG_VDACOFF        0x1
    /* unplug powerdown */
#define INIT_FLAG_POWERDOWN      0x2

// HDMI CEC Function Flag
#define INIT_FLAG_CEC_FUNC       0x4

#define INIT_FLAG_NOT_LOAD 0x80

#ifdef AVOS
static unsigned char init_flag=INIT_FLAG_POWERDOWN;
static unsigned char init_powermode=0x80;
#else
static unsigned char init_flag=0;
static unsigned char init_powermode=0;
#endif
#undef DISABLE_AUDIO
unsigned char hdmi_audio_off_flag = 0;        //if set to 1, then HDMI will output no audio
                                                //In KTV case, HDMI output Picture only, and Audio is driven by other sources.
static int hpdmode = 1; /* 
                            0, do not unmux hpd when off or unplug ; 
                            1, unmux hpd when unplug;
                            2, unmux hpd when unplug  or off;
                        */
static int force_vout_index = 0;                      
static int hdmi_prbs_mode = 0xffff; /* 0xffff=disable; 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 31*/
static int hdmi_480p_force_clk = 0; /* 200, 225, 250, 270 */

static int hdmi_output_on = 1;   
static int hdmi_authenticated = -1;                     
static int auth_output_auto_off = 0;
/*****************************
*    hdmitx attr management :
*    enable
*    mode
*    reg
******************************/
static void set_test_mode(void)
{
#ifdef ENABLE_TEST_MODE
//when it is used as test source (PRBS and 20,22.5,25MHz)
                if((hdmi_480p_force_clk)&&
                   ((hdmitx_device.cur_VIC==HDMI_480p60)||
                    (hdmitx_device.cur_VIC==HDMI_480p60_16x9)||
                    (hdmitx_device.cur_VIC==HDMI_480i60)||
                    (hdmitx_device.cur_VIC==HDMI_480i60_16x9)||
                    (hdmitx_device.cur_VIC==HDMI_576p50)||
                    (hdmitx_device.cur_VIC==HDMI_576p50_16x9)||
                    (hdmitx_device.cur_VIC==HDMI_576i50)||
                    (hdmitx_device.cur_VIC==HDMI_576i50_16x9))
                    ){
                    if(hdmitx_device.HWOp.Cntl){
                        hdmitx_device.HWOp.Cntl(&hdmitx_device, HDMITX_FORCE_480P_CLK, hdmi_480p_force_clk);
                    }
                }
                if(hdmi_prbs_mode != 0xffff){
                    if(hdmitx_device.HWOp.Cntl){
                        hdmitx_device.HWOp.Cntl(&hdmitx_device, HDMITX_HWCMD_TURN_ON_PRBS, hdmi_prbs_mode);
                    }
                }
#endif                
    
}

int get_cur_vout_index(void)
/*
return value: 1, vout; 2, vout2;
*/
{
    int vout_index = 1;
#ifdef CONFIG_AM_TV_OUTPUT2
    const vinfo_t *info;
    if(force_vout_index){
        vout_index = force_vout_index;
    }
    else{
//VPU_VIU_VENC_MUX_CTRL        
// [ 3: 2] cntl_viu2_sel_venc. Select which one of the encI/P/T that VIU2 connects to:
//         0=No connection, 1=ENCI, 2=ENCP, 3=ENCT.
// [ 1: 0] cntl_viu1_sel_venc. Select which one of the encI/P/T that VIU1 connects to:
//         0=No connection, 1=ENCI, 2=ENCP, 3=ENCT.
        int viu2_sel = (READ_MPEG_REG(VPU_VIU_VENC_MUX_CTRL)>>2)&0x3;
        int viu1_sel = READ_MPEG_REG(VPU_VIU_VENC_MUX_CTRL)&0x3;
        if(((viu2_sel==1)||(viu2_sel==2))&&
            (viu1_sel!=1)&&(viu1_sel!=2)){
            vout_index = 2;    
        }
    }
#endif
    return vout_index;    
}    

vinfo_t * hdmi_get_current_vinfo(void)
{
    const vinfo_t *info;
#ifdef CONFIG_AM_TV_OUTPUT2
    if(get_cur_vout_index() == 2){
        info = get_current_vinfo2();
    }
    else{
        info = get_current_vinfo();
    }
#else
    info = get_current_vinfo();
#endif
    return info;
}    

static  int  set_disp_mode(const char *mode)
{
    int ret=-1;
    HDMI_Video_Codes_t vic;
    vic = hdmitx_edid_get_VIC(&hdmitx_device, mode, 1);
    HDMI_DEBUG();
    if(vic != HDMI_Unkown){
        hdmitx_device.mux_hpd_if_pin_high_flag = 1;
        if(hdmitx_device.vic_count == 0){
               if(hdmitx_device.unplug_powerdown){
                   return 0;
                }
            }
        }

    hdmitx_device.cur_VIC = HDMI_Unkown;
    ret = hdmitx_set_display(&hdmitx_device, vic);
    if(ret>=0){
        hdmitx_device.cur_VIC = vic;
        hdmitx_device.audio_param_update_flag = 1;
        hdmi_authenticated = -1;
        hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
        set_test_mode();
    }

    if(hdmitx_device.cur_VIC == HDMI_Unkown){
        if(hpdmode == 2){
            hdmitx_edid_clear(&hdmitx_device); /* edid will be read again when hpd is muxed and it is high */
            hdmitx_device.mux_hpd_if_pin_high_flag = 0; 
        }
        if(hdmitx_device.HWOp.Cntl){
            hdmitx_device.HWOp.Cntl(&hdmitx_device, HDMITX_HWCMD_TURNOFF_HDMIHW, (hpdmode==2)?1:0);    
        }
    }
    return ret;
}

static int set_disp_mode_auto(void)
{
    int ret=-1;
#ifndef AVOS
    const vinfo_t *info = hdmi_get_current_vinfo();
#else
    vinfo_t* info=&lvideo_info;
#endif    
    HDMI_Video_Codes_t vic;     //Prevent warning
    //msleep(500);
    vic = hdmitx_edid_get_VIC(&hdmitx_device, info->name, (hdmitx_device.disp_switch_config==DISP_SWITCH_FORCE)?1:0);
    hdmitx_device.cur_VIC = HDMI_Unkown;
    ret = hdmitx_set_display(&hdmitx_device, vic); //if vic is HDMI_Unkown, hdmitx_set_display will disable HDMI
    if(ret>=0){
        hdmitx_device.cur_VIC = vic;
        hdmitx_device.audio_param_update_flag = 1;
        hdmi_authenticated = -1;
        hdmitx_device.auth_process_timer = AUTH_PROCESS_TIME;
        set_test_mode();
    }
    if(hdmitx_device.cur_VIC == HDMI_Unkown){
        if(hpdmode==2){
            hdmitx_edid_clear(&hdmitx_device); /* edid will be read again when hpd is muxed and it is high */
            hdmitx_device.mux_hpd_if_pin_high_flag = 0;
        }
        if(hdmitx_device.HWOp.Cntl){
            hdmitx_device.HWOp.Cntl(&hdmitx_device, HDMITX_HWCMD_TURNOFF_HDMIHW, (hpdmode==2)?1:0);    
        }
    }
    return ret;
}    

static unsigned char is_dispmode_valid_for_hdmi(void)
{
    HDMI_Video_Codes_t vic;
#ifndef AVOS
    const vinfo_t *info = hdmi_get_current_vinfo();
#else
    vinfo_t* info=&lvideo_info;
#endif    
    vic = hdmitx_edid_get_VIC(&hdmitx_device, info->name, (hdmitx_device.disp_switch_config==DISP_SWITCH_FORCE)?1:0);
    return (vic != HDMI_Unkown);
}

#ifndef AVOS
/*disp_mode attr*/
static ssize_t show_disp_mode(struct device * dev, struct device_attribute *attr, char * buf)
{
    int pos=0;
    pos+=snprintf(buf+pos, PAGE_SIZE, "VIC:%d\r\n", hdmitx_device.cur_VIC);
    return pos;    
}
    
static ssize_t store_disp_mode(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    set_disp_mode(buf);
    return 16;    
}

/*cec attr*/
static ssize_t show_cec(struct device * dev, struct device_attribute *attr, char * buf)
{
    ssize_t t = cec_usrcmd_get_global_info(buf);    
    return t;
}

static ssize_t store_cec(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    cec_usrcmd_set_dispatch(buf, count);
    return count;
}


/*aud_mode attr*/
static ssize_t show_aud_mode(struct device * dev, struct device_attribute *attr, char * buf)
{
    return 0;    
}
    
static ssize_t store_aud_mode(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    //set_disp_mode(buf);
    Hdmi_tx_audio_para_t* audio_param = &(hdmitx_device.cur_audio_param);
    if(strncmp(buf, "32k", 3)==0){
        audio_param->sample_rate = FS_32K; 
    }
    else if(strncmp(buf, "44.1k", 5)==0){
        audio_param->sample_rate = FS_44K1; 
    }
    else if(strncmp(buf, "48k", 3)==0){
        audio_param->sample_rate = FS_48K; 
    }
    else{
        hdmitx_device.force_audio_flag = 0;
        return count;
    }
    audio_param->type = CT_PCM;
    audio_param->channel_num = CC_2CH;
    audio_param->sample_size = SS_16BITS; 
    
    hdmitx_device.audio_param_update_flag = 1;
    hdmitx_device.force_audio_flag = 1;
    
    return count;    
}

/*edid attr*/
static ssize_t show_edid(struct device *dev, struct device_attribute *attr, char *buf)
{
    return hdmitx_edid_dump(&hdmitx_device, buf, PAGE_SIZE);
}

static ssize_t store_edid(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    
    if(buf[0]=='d'){
        int ii,jj;
        int block_idx;
        block_idx=simple_strtoul(buf+1,NULL,16);
        if(block_idx<EDID_MAX_BLOCK){
            for(ii=0;ii<8;ii++){
                for(jj=0;jj<16;jj++){
                    printk("%02x ",hdmitx_device.EDID_buf[block_idx*128+ii*16+jj]);
                }
                printk("\n");
            }
            printk("\n");
        }
    }
    return 16;    
}

/*config attr*/
static ssize_t show_config(struct device * dev, struct device_attribute *attr, char * buf)
{   
    int pos=0;
    pos += snprintf(buf+pos, PAGE_SIZE, "disp switch (force or edid): %s\r\n", (hdmitx_device.disp_switch_config==DISP_SWITCH_FORCE)?"force":"edid");
    return pos;    
}
    
static ssize_t store_config(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    if(strncmp(buf, "force", 5)==0){
        hdmitx_device.disp_switch_config=DISP_SWITCH_FORCE;
    }
    else if(strncmp(buf, "edid", 4)==0){
        hdmitx_device.disp_switch_config=DISP_SWITCH_EDID;
    }
    else if(strncmp(buf, "vdacoff", 7)==0){
        if(hdmitx_device.HWOp.Cntl){
            hdmitx_device.HWOp.Cntl(&hdmitx_device, HDMITX_HWCMD_VDAC_OFF, 0);    
        }
    }
    else if(strncmp(buf, "powermode", 9)==0){
        int tmp;
        tmp = simple_strtoul(buf+9,NULL,10);
        if(hdmitx_device.HWOp.Cntl){
            hdmitx_device.HWOp.Cntl(&hdmitx_device, HDMITX_HWCMD_POWERMODE_SWITCH, tmp); 
            printk("hdmi: set powermode %d\n", tmp);
        }
    }
    else if(strncmp(buf, "unplug_powerdown", 16) == 0){
        if(buf[16] == '0'){
            hdmitx_device.unplug_powerdown = 0;
        }
        else{
            hdmitx_device.unplug_powerdown = 1;
        }
    }
    else if(strncmp(buf, "3d", 2)==0){
        if(strncmp(buf+2, "tb", 2)==0){
            hdmi_set_3d(&hdmitx_device, 6, 0);
        }
        else if(strncmp(buf+2, "lr", 2)==0){
            int sub_sample_mode=0; 
            if(buf[2])
                sub_sample_mode = simple_strtoul(buf+2,NULL,10);
            hdmi_set_3d(&hdmitx_device, 8, sub_sample_mode); //side by side
        }
        else if(strncmp(buf+2, "off", 3)==0){
            hdmi_set_3d(&hdmitx_device, 0xf, 0);
        }
    }
    return 16;    
}
  
    
static ssize_t store_dbg(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    hdmitx_device.HWOp.DebugFun(&hdmitx_device, buf);
    return 16;    
}

/**/
static ssize_t show_disp_cap(struct device * dev, struct device_attribute *attr, char * buf)
{   
    int i,pos=0;
    char* disp_mode_t[]={"480i","480p","576i","576p","720p","1080i","1080p","720p50hz","1080i50hz","1080p50hz",NULL};
    char* native_disp_mode = hdmitx_edid_get_native_VIC(&hdmitx_device);
    HDMI_Video_Codes_t vic;
    for(i=0; disp_mode_t[i]; i++){
        vic = hdmitx_edid_get_VIC(&hdmitx_device, disp_mode_t[i], 0);
        if( vic != HDMI_Unkown){
            pos += snprintf(buf+pos, PAGE_SIZE,"%s",disp_mode_t[i]);
            if(native_disp_mode&&(strcmp(native_disp_mode, disp_mode_t[i])==0)){
                pos += snprintf(buf+pos, PAGE_SIZE,"*\n");
            }
            else{
                pos += snprintf(buf+pos, PAGE_SIZE,"\n");
            }                
        }
    }
    return pos;    
}

static ssize_t show_hpd_state(struct device * dev, struct device_attribute *attr, char * buf)
{   
    int i,pos=0;
    pos += snprintf(buf+pos, PAGE_SIZE,"%d", hdmitx_device.hpd_state);
    return pos;    
}

static unsigned char* hdmi_log_buf=NULL;
static unsigned int hdmi_log_wr_pos=0;
static unsigned int hdmi_log_rd_pos=0;
static unsigned int hdmi_log_buf_size=0;

static DEFINE_SPINLOCK(hdmi_print_lock);

#define PRINT_TEMP_BUF_SIZE 512
int hdmi_print_buf(char* buf, int len)
{
    unsigned long flags;
    int pos;
    int hdmi_log_rd_pos_;
    if(hdmi_log_buf_size==0)
        return 0;
    
    spin_lock_irqsave(&hdmi_print_lock, flags);
    hdmi_log_rd_pos_=hdmi_log_rd_pos;
    if(hdmi_log_wr_pos>=hdmi_log_rd_pos)
        hdmi_log_rd_pos_+=hdmi_log_buf_size;

    for(pos=0;pos<len && hdmi_log_wr_pos<(hdmi_log_rd_pos_-1);pos++,hdmi_log_wr_pos++){
        if(hdmi_log_wr_pos>=hdmi_log_buf_size)
            hdmi_log_buf[hdmi_log_wr_pos-hdmi_log_buf_size]=buf[pos];
        else
            hdmi_log_buf[hdmi_log_wr_pos]=buf[pos];
    }    
    if(hdmi_log_wr_pos>=hdmi_log_buf_size)
        hdmi_log_wr_pos-=hdmi_log_buf_size;
    spin_unlock_irqrestore(&hdmi_print_lock, flags);
    return pos;
    
}

int hdmi_print(int printk_flag, const char *fmt, ...)
{
    va_list args;
    int avail = PRINT_TEMP_BUF_SIZE;
    char buf[PRINT_TEMP_BUF_SIZE];
    int pos,len=0;
    if(printk_flag){
        va_start(args, fmt);
	      vprintk(fmt, args);
        va_end(args);	
    }
    if(hdmi_log_buf_size==0)
        return 0;
        
    va_start(args, fmt);
    len += vsnprintf(buf+len, avail-len, fmt, args);
    va_end(args);	

    if ((avail-len) <= 0) {
        buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';
    }
    pos = hdmi_print_buf(buf, len);
    //printk("hdmi_print:%d %d\n", hdmi_log_wr_pos, hdmi_log_rd_pos);
	  return pos;
}

static ssize_t show_log(struct device * dev, struct device_attribute *attr, char * buf)
{   
    unsigned long flags;
    int read_size=0;
    if(hdmi_log_buf_size==0)
        return 0;
    //printk("show_log:%d %d\n", hdmi_log_wr_pos, hdmi_log_rd_pos);
    spin_lock_irqsave(&hdmi_print_lock, flags);
    if(hdmi_log_rd_pos<hdmi_log_wr_pos){
        read_size = hdmi_log_wr_pos-hdmi_log_rd_pos;
    }
    else if(hdmi_log_rd_pos>hdmi_log_wr_pos){
        read_size = hdmi_log_buf_size-hdmi_log_rd_pos;
    }
    if(read_size>PAGE_SIZE)
        read_size=PAGE_SIZE;
    if(read_size>0)
        memcpy(buf, hdmi_log_buf+hdmi_log_rd_pos, read_size);
    
    hdmi_log_rd_pos += read_size;  
    if(hdmi_log_rd_pos>=hdmi_log_buf_size)
        hdmi_log_rd_pos = 0;
    spin_unlock_irqrestore(&hdmi_print_lock, flags);
    return read_size;    
}

static ssize_t store_log(struct device * dev, struct device_attribute *attr, const char * buf, size_t count)
{
    int tmp;
    unsigned long flags;
    if(strncmp(buf, "bufsize", 7)==0){
        tmp = simple_strtoul(buf+7,NULL,10);
        spin_lock_irqsave(&hdmi_print_lock, flags);
        if(tmp==0){
            if(hdmi_log_buf){
                kfree(hdmi_log_buf);
                hdmi_log_buf=NULL; 
                hdmi_log_buf_size=0;
                hdmi_log_rd_pos=0;   
                hdmi_log_wr_pos=0;   
            }    
        }    
        if((tmp>=1024)&&(hdmi_log_buf==NULL)){
            hdmi_log_buf_size=0;
            hdmi_log_rd_pos=0;   
            hdmi_log_wr_pos=0;   
            hdmi_log_buf=kmalloc(tmp, GFP_KERNEL);
            if(hdmi_log_buf){
                hdmi_log_buf_size=tmp;
                hdmitx_device.HWOp.DebugFun(&hdmitx_device, "v");
            }
        }            
        spin_unlock_irqrestore(&hdmi_print_lock, flags);
        //printk("hdmi_store:set bufsize tmp %d %d\n",tmp, hdmi_log_buf_size);
    }
    else{
        hdmi_print(0, "%s", buf);
    }
    return 16;
}

static DEVICE_ATTR(disp_mode, S_IWUSR | S_IRUGO, show_disp_mode, store_disp_mode);
static DEVICE_ATTR(aud_mode, S_IWUSR | S_IRUGO, show_aud_mode, store_aud_mode);
static DEVICE_ATTR(edid, S_IWUSR | S_IRUGO, show_edid, store_edid);
static DEVICE_ATTR(config, S_IWUSR | S_IRUGO, show_config, store_config);
static DEVICE_ATTR(debug, S_IWUSR | S_IRUGO, NULL, store_dbg);
static DEVICE_ATTR(disp_cap, S_IWUSR | S_IRUGO, show_disp_cap, NULL);
static DEVICE_ATTR(hpd_state, S_IWUSR | S_IRUGO, show_hpd_state, NULL);
static DEVICE_ATTR(log, S_IWUSR | S_IRUGO, show_log, store_log);
static DEVICE_ATTR(cec, S_IWUSR | S_IRUGO, show_cec, store_cec);

/*****************************
*    hdmitx display client interface 
*    
******************************/
static int hdmitx_notify_callback_v(struct notifier_block *block, unsigned long cmd , void *para)
{
    if(get_cur_vout_index()!=1)
        return 0;

    if (cmd != VOUT_EVENT_MODE_CHANGE)
        return 0;
    if(hdmitx_device.vic_count == 0){
        if(is_dispmode_valid_for_hdmi()){
            hdmitx_device.mux_hpd_if_pin_high_flag = 1;
            if(hdmitx_device.unplug_powerdown){
    			      return 0;
    			  }
		    }
    }

    set_disp_mode_auto();

    return 0;
}

#ifdef CONFIG_AM_TV_OUTPUT2
static int hdmitx_notify_callback_v2(struct notifier_block *block, unsigned long cmd , void *para)
{
    if(get_cur_vout_index()!=2)
        return 0;

    if (cmd != VOUT_EVENT_MODE_CHANGE)
        return 0;

    if(hdmitx_device.vic_count == 0){
        if(is_dispmode_valid_for_hdmi()){
            hdmitx_device.mux_hpd_if_pin_high_flag = 1;
            if(hdmitx_device.unplug_powerdown){
    			      return 0;
    			  }
		    }
    }

    set_disp_mode_auto();

    return 0;
}
#endif

static struct notifier_block hdmitx_notifier_nb_v = {
    .notifier_call    = hdmitx_notify_callback_v,
};

#ifdef CONFIG_AM_TV_OUTPUT2
static struct notifier_block hdmitx_notifier_nb_v2 = {
    .notifier_call    = hdmitx_notify_callback_v2,
};
#endif

#ifndef DISABLE_AUDIO

// Refer to CEA-861-D Page 88
#define AOUT_EVENT_REFER_TO_STREAM_HEADER       0x0
#define AOUT_EVENT_IEC_60958_PCM                0x1
#define AOUT_EVENT_RAWDATA_AC_3                 0x2
#define AOUT_EVENT_RAWDATA_MPEG1                0x3
#define AOUT_EVENT_RAWDATA_MP3                  0x4
#define AOUT_EVENT_RAWDATA_MPEG2                0x5
#define AOUT_EVENT_RAWDATA_AAC                  0x6
#define AOUT_EVENT_RAWDATA_DTS                  0x7
#define AOUT_EVENT_RAWDATA_ATRAC                0x8
#define AOUT_EVENT_RAWDATA_ONE_BIT_AUDIO        0x9
#define AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS   0xA
#define AOUT_EVENT_RAWDATA_DTS_HD               0xB
#define AOUT_EVENT_RAWDATA_MAT_MLP              0xC
#define AOUT_EVENT_RAWDATA_DST                  0xD
#define AOUT_EVENT_RAWDATA_WMA_PRO              0xE
extern int aout_register_client(struct notifier_block * ) ;
extern int aout_unregister_client(struct notifier_block * ) ;

#include <linux/soundcard.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>

static int hdmitx_notify_callback_a(struct notifier_block *block, unsigned long cmd , void *para)
{
    struct snd_pcm_substream *substream =(struct snd_pcm_substream*)para;
    Hdmi_tx_audio_para_t* audio_param = &(hdmitx_device.cur_audio_param);
    switch (cmd){
    case AOUT_EVENT_IEC_60958_PCM:
        audio_param->type = CT_PCM;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
    
        switch (substream->runtime->rate) {
            case 192000:
                audio_param->sample_rate = FS_192K; 
                break;
            case 176400:
                audio_param->sample_rate = FS_176K4; 
                break;
            case 96000:
                audio_param->sample_rate = FS_96K; 
                break;
            case 88200:
                audio_param->sample_rate = FS_88K2; 
                break;
            case 48000:
                audio_param->sample_rate = FS_48K; 
                break;
            case 44100:
                audio_param->sample_rate = FS_44K1; 
                break;
            case 32000:
                audio_param->sample_rate = FS_32K; 
                break;
            default:
                break;
        }
        hdmi_print(1, "HDMI: aout notify rate %d\n", substream->runtime->rate);
        hdmi_print(1, "HDMI: aout notify format PCM\n");
        break;
    case AOUT_EVENT_RAWDATA_AC_3:
        audio_param->type = CT_AC_3;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format AC-3\n");
        break;
    case AOUT_EVENT_RAWDATA_MPEG1:
        audio_param->type = CT_MPEG1;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format MPEG1(Layer1 2)\n");
        break;
    case AOUT_EVENT_RAWDATA_MP3:
        audio_param->type = CT_MP3;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format MP3(MPEG1 Layer3)\n");
        break;
    case AOUT_EVENT_RAWDATA_MPEG2:
        audio_param->type = CT_MPEG2;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format MPEG2\n");
        break;
    case AOUT_EVENT_RAWDATA_AAC:
        audio_param->type = CT_AAC;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format AAC\n");
        break;
    case AOUT_EVENT_RAWDATA_DTS:
        audio_param->type = CT_DTS;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format DTS\n");
        break;
    case AOUT_EVENT_RAWDATA_ATRAC:
        audio_param->type = CT_ATRAC;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format ATRAC\n");
        break;
    case AOUT_EVENT_RAWDATA_ONE_BIT_AUDIO:
        audio_param->type = CT_ONE_BIT_AUDIO;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format One Bit Audio\n");
        break;
    case AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS:
        audio_param->type = CT_DOLBY_D;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format Dobly Digital +\n");
        break;
    case AOUT_EVENT_RAWDATA_DTS_HD:
        audio_param->type = CT_DTS_HD;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format DTS-HD\n");
        break;
    case AOUT_EVENT_RAWDATA_MAT_MLP:
        audio_param->type = CT_MAT;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format MAT(MLP)\n");
        break;
    case AOUT_EVENT_RAWDATA_DST:
        audio_param->type = CT_DST;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format DST\n");
        break;
    case AOUT_EVENT_RAWDATA_WMA_PRO:
        audio_param->type = CT_WMA;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
        hdmi_print(1, "HDMI: aout notify format WMA Pro\n");
        break;
    default:
        break;
    }    
    hdmitx_device.audio_param_update_flag = 1;
    return 0;
}

static struct notifier_block hdmitx_notifier_nb_a = {
    .notifier_call    = hdmitx_notify_callback_a,
};
#endif
#else
/* AVOS */

typedef void(*set_tv_enc_post_fun_t)(char* mode);
extern set_tv_enc_post_fun_t tv_tv_enc_post_fun;
Hdmi_tx_video_para_t *hdmi_get_video_param(HDMI_Video_Codes_t VideoCode);
void hdmi_tvenc_set(Hdmi_tx_video_para_t *param);

void hdmi_tv_enc_post_func(char* mode)
{

    strcpy(lvideo_info.name, mode);
        
    if(hdmitx_device.vic_count == 0){
        if(is_dispmode_valid_for_hdmi()){
            hdmitx_device.mux_hpd_if_pin_high_flag = 1;
            if(hdmitx_device.unplug_powerdown){
    			      return;
    			  }
        }
    }

    set_disp_mode_auto();
}    


int hdmi_audio_post_func(int type, int channel, int sample_size, int rate)
{
        Hdmi_tx_audio_para_t* audio_param = &(hdmitx_device.cur_audio_param);

        audio_param->type = CT_PCM;
        audio_param->channel_num = CC_2CH;
        audio_param->sample_size = SS_16BITS; 
    
        switch (rate) {
            case 192000:
                audio_param->sample_rate = FS_192K; 
                break;
            case 176400:
                audio_param->sample_rate = FS_176K4; 
                break;
            case 96000:
                audio_param->sample_rate = FS_96K; 
                break;
            case 88200:
                audio_param->sample_rate = FS_88K2; 
                break;
            case 48000:
                audio_param->sample_rate = FS_48K; 
                break;
            case 44100:
                audio_param->sample_rate = FS_44K1; 
                break;
            case 32000:
                audio_param->sample_rate = FS_32K; 
                break;
            default:
                break;
        }
        hdmitx_device.audio_param_update_flag = 1;
        hdmi_print(1, "HDMI: aout notify rate %d\n", rate);
        return 0;
}

#endif
/******************************
*  hdmitx kernel task
*******************************/
#ifndef AVOS
static int 
#else
static void
#endif
hdmi_task_handle(void *data) 
{
    extern void hdmitx_edid_ram_buffer_clear(hdmitx_dev_t*);
    hdmitx_dev_t* hdmitx_device = (hdmitx_dev_t*)data;
    hdmitx_init_parameters(&hdmitx_device->hdmi_info);

    HDMITX_M1B_Init(hdmitx_device);

    //When init hdmi, clear the hdmitx module edid ram and edid buffer.
    hdmitx_edid_ram_buffer_clear(hdmitx_device);

    hdmitx_device->HWOp.SetupIRQ(hdmitx_device);

    if(hdmitx_device->HWOp.Cntl){
        if(init_flag&INIT_FLAG_VDACOFF){
            hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_HWCMD_VDAC_OFF, 0);    
        }
        if(init_powermode&0x80){
            hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_HWCMD_POWERMODE_SWITCH, init_powermode&0x1f);    
        }
    }
    if(init_flag&INIT_FLAG_POWERDOWN){
        hdmitx_device->HWOp.SetDispMode(NULL); //power down
        hdmitx_device->unplug_powerdown=1;
        if(hdmitx_device->HWOp.Cntl){
            hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_HWCMD_TURNOFF_HDMIHW, (hpdmode!=0)?1:0);    
        }
    }
    else{
        if(hdmitx_device->HWOp.Cntl){
            hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_HWCMD_MUX_HPD, 0);    
        }
    }
    
    HDMI_DEBUG();

    while (hdmitx_device->hpd_event != 0xff)
    {
        if((hdmitx_device->vic_count == 0)&&(hdmitx_device->mux_hpd_if_pin_high_flag)){
            if(hdmitx_device->HWOp.Cntl){
                hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH, 0);
            }
        }
        
        if((!hdmi_audio_off_flag)&&(hdmitx_device->audio_param_update_flag)&&
            ((hdmitx_device->cur_VIC != HDMI_Unkown)||(hdmitx_device->force_audio_flag))){
            hdmitx_set_audio(hdmitx_device, &(hdmitx_device->cur_audio_param));
            hdmitx_device->audio_param_update_flag = 0;
            hdmi_print(1, "HDMI: set audio param\n");
        }

        if (hdmitx_device->hpd_event == 1)
        {
            if(hdmitx_device->HWOp.GetEDIDData(hdmitx_device)){
                hdmi_print(1,"HDMI: EDID Ready\n");
                hdmitx_edid_clear(hdmitx_device);
                hdmitx_edid_parse(hdmitx_device);
                cec_node_init(hdmitx_device);
                set_disp_mode_auto();

				switch_set_state(&sdev, 1);
                hdmitx_device->hpd_event = 0;
            }  
            hdmitx_device->hpd_state = 1;  
        }
        else if(hdmitx_device->hpd_event == 2)
        {
            //When unplug hdmi, clear the hdmitx module edid ram and edid buffer.
            hdmitx_edid_ram_buffer_clear(hdmitx_device);
            hdmitx_edid_clear(hdmitx_device);
            cec_node_uninit(hdmitx_device);

            if(hdmitx_device->unplug_powerdown){
                hdmitx_set_display(hdmitx_device, HDMI_Unkown);
                if(hdmitx_device->HWOp.Cntl){
                    hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_HWCMD_TURNOFF_HDMIHW, (hpdmode!=0)?1:0);    
                }
            }
            hdmitx_device->cur_VIC = HDMI_Unkown;
            hdmi_authenticated = -1;
			switch_set_state(&sdev, 0);
            hdmitx_device->hpd_event = 0;
            hdmitx_device->hpd_state = 0;
        }    
        else{
        }            
        /* authentication process */
#ifdef CONFIG_AML_HDMI_TX_HDCP
        if(hdmitx_device->cur_VIC != HDMI_Unkown){
            if(hdmitx_device->auth_process_timer>0){
                hdmitx_device->auth_process_timer--;
            }
            else{
                hdmi_authenticated = hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_GET_AUTHENTICATE_STATE, NULL);
                if(auth_output_auto_off){
                    if(hdmi_authenticated){
                        hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_OUTPUT_ENABLE, 1);
                    }
                    else{
                        hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_OUTPUT_ENABLE, 0);
                    }
                }
                else{
                    hdmitx_device->HWOp.Cntl(hdmitx_device, HDMITX_OUTPUT_ENABLE, hdmi_output_on);
                }
            }
        }
#endif        
        /**/    
        HDMI_PROCESS_DELAY;            
    }
#ifndef AVOS
    return 0;
#endif    

}

#ifdef AVOS
#define PRINT_TEMP_BUF_SIZE 256

int hdmi_print_buf(char* buf, int len)
{
    if(AVOS_Print_Flag&DBGHLP_LOG_HDMI){
        AVOS_printf("%s", buf);
    }
    return 0;   
}

int hdmi_print(int printk_flag, const char *fmt, ...)
{
    va_list args;
    int avail = PRINT_TEMP_BUF_SIZE;
    char buf[PRINT_TEMP_BUF_SIZE];
    int pos,len=0;
    /*
    if(printk_flag){
        va_start(args, fmt);
	      vprintk(fmt, args);
        va_end(args);	
    }
    if(hdmi_log_buf_size==0)
        return 0;
    */  
    va_start(args, fmt);
    len += vsnprintf(buf+len, avail-len, fmt, args);
    va_end(args);	

    if ((avail-len) <= 0) {
        buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';
    }
    if(printk_flag){
        AVOS_printf("%s", buf);
    }
    else{
        pos = hdmi_print_buf(buf, len);
    }
    //printk("hdmi_print:%d %d\n", hdmi_log_wr_pos, hdmi_log_rd_pos);
	  return pos;
}

#define HDMI_OP_STK_SIZE            (512)
#define HDMI_OP_STK_SIZE_IN_BYTE    (HDMI_OP_STK_SIZE * 4)
#define HDMI_OP_TASK_PRI            (20)   //(19)

#define DRIVER_NAME "/dev/hdmi"

avfs_device_driver hdmi_init(avfs_device_major_number major, avfs_device_minor_number minor, void *arg)
{
    avfs_status_code status;     
    status = avfs_io_register_name(
        DRIVER_NAME,
        major,
        (avfs_device_minor_number) 0
    );   
    HDMI_DEBUG();
    memset(&hdmitx_device, 0, sizeof(hdmitx_dev_t));
    strcpy(lvideo_info.name, "");
    hdmitx_device.vic_count=0;
    if((init_flag&INIT_FLAG_POWERDOWN)&&(hpdmode==2)){
        hdmitx_device.mux_hpd_if_pin_high_flag=0;
    }
    else{
        hdmitx_device.mux_hpd_if_pin_high_flag=1;
    }
    hdmitx_device.audio_param_update_flag=0;
    tv_tv_enc_post_fun = hdmi_tv_enc_post_func;

   	hdmitx_device.taskstack = (OS_STK*) AVMem_malloc(HDMI_OP_STK_SIZE_IN_BYTE);
    AVTaskCreate(hdmi_task_handle, (void *)(&hdmitx_device), &hdmitx_device.taskstack[HDMI_OP_STK_SIZE - 1], HDMI_OP_TASK_PRI, &hdmitx_device.task_id); 
#if OS_TASK_STACKCHK_EN > 0
   	AVTaskSetStackSize(hdmitx_device.task_id, HDMI_OP_STK_SIZE * sizeof(OS_STK));
#endif
    return AVFS_SUCCESSFUL;
}

avfs_device_driver hdmi_open(avfs_device_major_number major, avfs_device_minor_number minor, void *arg)
{ 
   		    		   			
    return AVFS_SUCCESSFUL;
}

avfs_device_driver hdmi_close(avfs_device_major_number major, avfs_device_minor_number minor, void *arg)
{
    return AVFS_SUCCESSFUL;
}

avfs_device_driver hdmi_read(avfs_device_major_number major, avfs_device_minor_number minor, void *arg)
{
    return AVFS_SUCCESSFUL;    
}

avfs_device_driver hdmi_write(avfs_device_major_number major, avfs_device_minor_number minor, void *arg)
{
    return AVFS_SUCCESSFUL;
}

avfs_device_driver hdmi_ioctl(avfs_device_major_number major, avfs_device_minor_number minor, void *arg)
{
    return AVFS_NOT_IMPLEMENTED;
}

#else
/* Linux */
/*****************************
*    hdmitx driver file_operations 
*    
******************************/
static int amhdmitx_open(struct inode *node, struct file *file)
{
    hdmitx_dev_t *hdmitx_in_devp;

    /* Get the per-device structure that contains this cdev */
    hdmitx_in_devp = container_of(node->i_cdev, hdmitx_dev_t, cdev);
    file->private_data = hdmitx_in_devp;

    return 0;

}


static int amhdmitx_release(struct inode *node, struct file *file)
{
    //hdmitx_dev_t *hdmitx_in_devp = file->private_data;

    /* Reset file pointer */

    /* Release some other fields */
    /* ... */
    return 0;
}



static int amhdmitx_ioctl(struct inode *node, struct file *file, unsigned int cmd,   unsigned long args)
{
    int   r = 0;
    switch (cmd) {
        default:
            break;
    }
    return r;
}

const static struct file_operations amhdmitx_fops = {
    .owner    = THIS_MODULE,
    .open     = amhdmitx_open,
    .release  = amhdmitx_release,
    .ioctl    = amhdmitx_ioctl,
};


static int amhdmitx_probe(struct platform_device *pdev)
{
    int r;
    HDMI_DEBUG();
    pr_dbg("amhdmitx_probe\n");
    r = alloc_chrdev_region(&hdmitx_id, 0, HDMI_TX_COUNT, DEVICE_NAME);
    if (r < 0) {
        pr_error("Can't register major for amhdmitx device\n");
        return r;
    }
    hdmitx_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(hdmitx_class))
    {
        unregister_chrdev_region(hdmitx_id, HDMI_TX_COUNT);
        return -1;
        //return PTR_ERR(aoe_class);
    }
    hdmitx_device.unplug_powerdown=0;
    hdmitx_device.vic_count=0;
    hdmitx_device.auth_process_timer=0;
    hdmitx_device.force_audio_flag=0;
    if(init_flag&INIT_FLAG_CEC_FUNC){
        hdmitx_device.cec_func_flag = 1;
    }
    else{
        hdmitx_device.cec_func_flag = 0;
    }
    if((init_flag&INIT_FLAG_POWERDOWN)&&(hpdmode==2)){
        hdmitx_device.mux_hpd_if_pin_high_flag=0;
    }
    else{
        hdmitx_device.mux_hpd_if_pin_high_flag=1;
    }
    hdmitx_device.audio_param_update_flag=0;
    cdev_init(&(hdmitx_device.cdev), &amhdmitx_fops);
    hdmitx_device.cdev.owner = THIS_MODULE;
    cdev_add(&(hdmitx_device.cdev), hdmitx_id, HDMI_TX_COUNT);

    //hdmitx_dev = device_create(hdmitx_class, NULL, hdmitx_id, "amhdmitx%d", 0);
    hdmitx_dev = device_create(hdmitx_class, NULL, hdmitx_id, NULL, "amhdmitx%d", 0); //kernel>=2.6.27 

    device_create_file(hdmitx_dev, &dev_attr_disp_mode);
    device_create_file(hdmitx_dev, &dev_attr_aud_mode);
    device_create_file(hdmitx_dev, &dev_attr_edid);
    device_create_file(hdmitx_dev, &dev_attr_config);
    device_create_file(hdmitx_dev, &dev_attr_debug);
    device_create_file(hdmitx_dev, &dev_attr_disp_cap);
    device_create_file(hdmitx_dev, &dev_attr_hpd_state);
    device_create_file(hdmitx_dev, &dev_attr_log);
    device_create_file(hdmitx_dev, &dev_attr_cec);
    
    if (hdmitx_dev == NULL) {
        pr_error("device_create create error\n");
        class_destroy(hdmitx_class);
        r = -EEXIST;
        return r;
    }
    vout_register_client(&hdmitx_notifier_nb_v);
#ifdef CONFIG_AM_TV_OUTPUT2
    vout2_register_client(&hdmitx_notifier_nb_v2);
#endif
#ifndef DISABLE_AUDIO
    aout_register_client(&hdmitx_notifier_nb_a);
#endif
    hdmitx_device.task = kthread_run(hdmi_task_handle, &hdmitx_device, "kthread_hdmi");
    
	switch_dev_register(&sdev);
	if (r < 0){
		printk(KERN_ERR "hdmitx: register switch dev failed\n");
		return r;
	}    

    return r;
}

static int amhdmitx_remove(struct platform_device *pdev)
{
	switch_dev_unregister(&sdev);
	
    if(hdmitx_device.HWOp.UnInit){
        hdmitx_device.HWOp.UnInit(&hdmitx_device);
    }
    hdmitx_device.hpd_event = 0xff;
    kthread_stop(hdmitx_device.task);
    
    vout_unregister_client(&hdmitx_notifier_nb_v);    
#ifdef CONFIG_AM_TV_OUTPUT2
    vout2_unregister_client(&hdmitx_notifier_nb_v2);    
#endif
#ifndef DISABLE_AUDIO
    aout_unregister_client(&hdmitx_notifier_nb_a);
#endif

    /* Remove the cdev */
    device_remove_file(hdmitx_dev, &dev_attr_disp_mode);
    device_remove_file(hdmitx_dev, &dev_attr_aud_mode);
    device_remove_file(hdmitx_dev, &dev_attr_edid);
    device_remove_file(hdmitx_dev, &dev_attr_config);
    device_remove_file(hdmitx_dev, &dev_attr_debug);
    device_remove_file(hdmitx_dev, &dev_attr_disp_cap);
    device_remove_file(hdmitx_dev, &dev_attr_hpd_state);
    device_remove_file(hdmitx_dev, &dev_attr_log);
    device_remove_file(hdmitx_dev, &dev_attr_cec);

    cdev_del(&hdmitx_device.cdev);

    device_destroy(hdmitx_class, hdmitx_id);

    class_destroy(hdmitx_class);

    unregister_chrdev_region(hdmitx_id, HDMI_TX_COUNT);
    return 0;
}

#ifdef CONFIG_PM
static int amhdmitx_suspend(struct platform_device *pdev,pm_message_t state)
{
    pr_info("amhdmitx: hdmirx_suspend\n");
    return 0;
}

static int amhdmitx_resume(struct platform_device *pdev)
{
    pr_info("amhdmitx: resume module\n");
    return 0;
}
#endif

static struct platform_driver amhdmitx_driver = {
    .probe      = amhdmitx_probe,
    .remove     = amhdmitx_remove,
#ifdef CONFIG_PM
    .suspend    = amhdmitx_suspend,
    .resume     = amhdmitx_resume,
#endif
    .driver     = {
        .name   = DEVICE_NAME,
		    .owner	= THIS_MODULE,
    }
};

static struct platform_device* amhdmi_tx_device = NULL;


static int  __init amhdmitx_init(void)
{
    HDMI_DEBUG();
    if(init_flag&INIT_FLAG_NOT_LOAD)
        return 0;
        
    pr_dbg("amhdmitx_init\n");
    if(hdmi_log_buf_size>0){
        hdmi_log_buf=kmalloc(hdmi_log_buf_size, GFP_KERNEL);
        if(hdmi_log_buf==NULL){
            hdmi_log_buf_size=0;
        }
    }
	  amhdmi_tx_device = platform_device_alloc(DEVICE_NAME,0);
    if (!amhdmi_tx_device) {
        pr_error("failed to alloc amhdmi_tx_device\n");
        return -ENOMEM;
    }
    
    if(platform_device_add(amhdmi_tx_device)){
        platform_device_put(amhdmi_tx_device);
        pr_error("failed to add amhdmi_tx_device\n");
        return -ENODEV;
    }
    if (platform_driver_register(&amhdmitx_driver)) {
        pr_error("failed to register amhdmitx module\n");
        
        platform_device_del(amhdmi_tx_device);
        platform_device_put(amhdmi_tx_device);
        return -ENODEV;
    }
    return 0;
}




static void __exit amhdmitx_exit(void)
{
    pr_dbg("amhdmitx_exit\n");
    platform_driver_unregister(&amhdmitx_driver);
    platform_device_unregister(amhdmi_tx_device); 
    amhdmi_tx_device = NULL;
    return ;
}

module_init(amhdmitx_init);
//arch_initcall(amhdmitx_init);
module_exit(amhdmitx_exit);

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

static  int __init hdmitx_boot_para_setup(char *s)
{
    char separator[]={' ',',',';',0x0};
    char *token;
    unsigned token_len, token_offset, offset=0;
    int size=strlen(s);
    HDMI_DEBUG();
    do{
        token=next_token_ex(separator, s, size, offset, &token_len, &token_offset);
        if(token){
            if((token_len==3) && (strncmp(token, "off", token_len)==0)){
                init_flag|=INIT_FLAG_NOT_LOAD;
            }
            else if((token_len==7) && (strncmp(token, "vdacoff", token_len)==0)){
                init_flag|=INIT_FLAG_VDACOFF;
            }
            else if((token_len==16) && (strncmp(token, "unplug_powerdown", token_len)==0)){
                init_flag|=INIT_FLAG_POWERDOWN;
            }
            else if(strncmp(token, "pllmode1",  8)==0){
                    /* use external xtal as source of hdmi pll */
                hdmi_pll_mode = 1;
            }
            else if((token_len==7)&& (strncmp(token, "hpdmode", token_len)==0)){
                hpdmode = simple_strtoul(token+7,NULL,10);   
            }
            else if(strncmp(token, "audpara", 7)==0){
                int tmp;
                tmp = simple_strtoul(token+7,NULL,10);
                hdmi_set_audio_para(tmp);
                printk("hdmi: set hdmi aud_para %d\n", tmp);
            }
            else if(strncmp(token, "bufsize", 7)==0){
                int tmp;
                tmp = simple_strtoul(token+7,NULL,10);
                if(tmp>=1024){
                    hdmi_log_buf_size=0;
                    hdmi_log_rd_pos=0;   
                    hdmi_log_wr_pos=0;   
                    hdmi_log_buf_size=tmp;
                    printk("hdmi: set log buffer size %d\n", tmp);
                }            
            }
            else if(strncmp(token, "powermode", 9)==0){
                int tmp;
                tmp = simple_strtoul(token+9,NULL,10);
                init_powermode=tmp|0x80;
                printk("hdmi: set init powermode %d\n", tmp);                
            }
            else if(strncmp(token, "audiooff", 8)==0){
                hdmi_audio_off_flag = 1;
                printk("hdmi: set no audio output\n");
            }
            else if(strncmp(token, "prbs", 4)==0){
                hdmi_prbs_mode = simple_strtoul(token+4,NULL,16);
                printk("hdmi, set prbs mode as %x always\n", hdmi_prbs_mode);    
            }
            else if(strncmp(token, "480p_clk", 8)==0){
                hdmi_480p_force_clk = simple_strtoul(token+8,NULL,10);
                printk("hdmi, set 480p mode clock as %dMHz always\n", hdmi_480p_force_clk);    
            }
            else if(strncmp(token, "true", 4)==0){
                init_flag |= INIT_FLAG_CEC_FUNC;
                printk("hdmi: enable cec function\n");    
            }
            
        }    
        offset=token_offset;
    }while(token);
    return 0;
}

__setup("hdmitx=",hdmitx_boot_para_setup);

MODULE_PARM_DESC(force_vout_index, "\n force_vout_index\n");
module_param(force_vout_index, uint, 0664);

MODULE_PARM_DESC(hdmi_480p_force_clk, "\n hdmi_480p_force_clk \n");
module_param(hdmi_480p_force_clk, int, 0664);

MODULE_PARM_DESC(hdmi_prbs_mode, "\n hdmi_prbs_mode \n");
module_param(hdmi_prbs_mode, int, 0664);

MODULE_PARM_DESC(hdmi_authenticated, "\n hdmi_authenticated \n");
module_param(hdmi_authenticated, int, 0664);

MODULE_PARM_DESC(hdmi_output_on, "\n hdmi_output_on \n");
module_param(hdmi_output_on, int, 0664);

MODULE_PARM_DESC(auth_output_auto_off, "\n auth_output_auto_off \n");
module_param(auth_output_auto_off, int, 0664);

#endif
