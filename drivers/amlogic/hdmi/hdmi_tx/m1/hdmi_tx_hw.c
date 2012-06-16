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

#ifndef AVOS
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
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
//#include <linux/amports/canvas.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <mach/am_regs.h>
#include <mach/power_gate.h>
#else
#include "ioapi.h"
#include <chipsupport/chipsupport.h>
#include <os/extend/interrupt.h>
#include <Drivers/include/peripheral_reg.h>
#include <Drivers/include/isa_reg.h>
#include <Drivers/include/mpeg_reg.h>
#include <Drivers/include/venc_reg.h>
#include <interrupt.h>
#include "displaydev.h"
#include "policy.h"
#define printk(...)

#define INT_HDMI_TX 25
#endif

#include "../hdmi_info_global.h"
#include "../hdmi_tx_module.h"
#include "../hdmi_tx_cec.h"
#include "hdmi_tx_reg.h"
#include "tvenc_conf.h"
//#define XTAL_24MHZ
#ifdef Wr
#undef Wr
#endif
#ifdef Rd
#undef Rd
#endif
#define Wr(reg,val) WRITE_MPEG_REG(reg,val)
#define Rd(reg)   READ_MPEG_REG(reg)
#define Wr_reg_bits(reg, val, start, len) \
  Wr(reg, (Rd(reg) & ~(((1L<<(len))-1)<<(start)))|((unsigned int)(val) << (start)))

static void hdmi_audio_init(unsigned char spdif_flag);
static void hdmitx_dump_tvenc_reg(int cur_VIC, int printk_flag);


//#define HPD_DELAY_CHECK
#ifdef CONFIG_AML_HDMI_TX_CEC
#define CEC_SUPPORT
#endif

//#define MORE_LOW_P
#define LOG_EDID

#ifdef CONFIG_AML_HDMI_TX_HDCP
static unsigned force_wrong=0;
static int hdcpkey_status = -1;
#endif
extern int task_tx_key_setting(unsigned force_wrong);
static void cec_test_fun_reserved(unsigned cmd_no);


#define HDMI_M1A 'a'
#define HDMI_M1B 'b'
#define HDMI_M1C 'c'
static unsigned char hdmi_chip_type = 0;
unsigned char hdmi_pll_mode = 0; /* 1, use external clk as hdmi pll source */
static unsigned char aud_para = 0x49;

#define HSYNC_POLARITY      1                       // HSYNC polarity: active high 
#define VSYNC_POLARITY      1                       // VSYNC polarity: active high
#define TX_INPUT_COLOR_DEPTH    0                   // Pixel bit width: 0=24-bit; 1=30-bit; 2=36-bit; 3=48-bit.
#define TX_INPUT_COLOR_FORMAT   1                   // Pixel format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422.
#define TX_INPUT_COLOR_RANGE    0                   // Pixel range: 0=16-235/240; 1=16-240; 2=1-254; 3=0-255.


#define TX_OUTPUT_COLOR_RANGE   0                   // Pixel range: 0=16-235/240; 1=16-240; 2=1-254; 3=0-255.

#if 1
//spdif
#define TX_I2S_SPDIF        0                       // 0=SPDIF; 1=I2S.
#define TX_I2S_8_CHANNEL    0                       // 0=I2S 2-channel; 1=I2S 4 x 2-channel.
#else
//i2s 8 channel
#define TX_I2S_SPDIF        1                       // 0=SPDIF; 1=I2S.
#define TX_I2S_8_CHANNEL    1                       // 0=I2S 2-channel; 1=I2S 4 x 2-channel.
#endif

#ifdef HPD_DELAY_CHECK
static struct timer_list hpd_timer;
#endif

//static struct tasklet_struct EDID_tasklet;
static unsigned delay_flag = 0;
#ifdef AML_A3
static unsigned serial_reg_val=0x24;
static unsigned char i2s_to_spdif_flag=1; //i2s clock in avos is div by 4 from amclk by audio driver, so use spdif
#else
static unsigned serial_reg_val=0x1; //0x22;
static unsigned char i2s_to_spdif_flag=0;
#endif
static unsigned color_depth_f=0;
static unsigned color_space_f=0;
static unsigned char new_reset_sequence_flag=1;
static unsigned char power_mode=1;
static unsigned char power_off_vdac_flag=0;
    /* 0, do not use fixed tvenc val for all mode; 1, use fixed tvenc val mode for 480i; 2, use fixed tvenc val mode for all modes */
static unsigned char use_tvenc_conf_flag=0;
static unsigned char hpd_debug_mode=0;
#define HPD_DEBUG_IGNORE_UNPLUG   1

static unsigned long modulo(unsigned long a, unsigned long b)
{
    if (a >= b) {
        return(a-b);
    } else {
        return(a);
    }
}
        
static signed int to_signed(unsigned int a)
{
    if (a <= 7) {
        return(a);
    } else {
        return(a-16);
    }
}

static void delay_us (int us)
{
#ifndef AVOS
    //udelay(us);
    if(delay_flag&0x1)
        mdelay((us+999)/1000);
    else
    udelay(us);
#else
//#define ISA_TIMERE IREG_TIMER_E_COUNT
    Wr(IREG_TIMER_E_COUNT,0);
    while(Rd(IREG_TIMER_E_COUNT)<us){}
#endif    
} /* delay_us */


#ifndef AVOS
static irqreturn_t intr_handler(int irq, void *dev_instance)
#else
static void intr_handler(void *arg)
#endif
{
    unsigned int data32;
#ifndef AVOS
    hdmitx_dev_t* hdmitx_device = (hdmitx_dev_t*)dev_instance;
#else
    hdmitx_dev_t* hdmitx_device = (hdmitx_dev_t*)arg;
#endif    
    
    data32 = hdmi_rd_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT); 
    hdmi_print(1,"HDMI irq %x\n",data32);
    if (data32 & (1 << 0)) {  //HPD rising 
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 0); //clear HPD rising interrupt in hdmi module
        // If HPD asserts, then start DDC transaction
#ifdef HPD_DELAY_CHECK
        del_timer(&hpd_timer);    
        hpd_timer.expires = jiffies + HZ/2;
        add_timer(&hpd_timer);
#else
        if (hdmi_rd_reg(TX_HDCP_ST_EDID_STATUS) & (1<<1)) {
            // Start DDC transaction
#if 1 //def AML_A3
            hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) | (1<<6)); // Assert sys_trigger_config
#else
//workaround, only for M1
            hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) | (1<<6)); // Assert sys_trigger_config
            hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) & ~(1<<1)); // Release sys_trigger_config_semi_manu
#endif            
            hdmitx_device->cur_edid_block=0;
            hdmitx_device->cur_phy_block_ptr=0;
            hdmitx_device->hpd_event = 1;
        // Error if HPD deasserts
        } else {
            hdmi_print(1,"HDMI Error: HDMI HPD deasserts!\n");
        }
#endif        
    } else if (data32 & (1 << 1)) { //HPD falling
        if(hpd_debug_mode&HPD_DEBUG_IGNORE_UNPLUG){
            hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 1); //clear HPD falling interrupt in hdmi module     
        }
        else{
            hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 1); //clear HPD falling interrupt in hdmi module 
#ifdef HPD_DELAY_CHECK
            del_timer(&hpd_timer);    
            hpd_timer.expires = jiffies + HZ/2;
            add_timer(&hpd_timer);
#else
            hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) & ~(1<<6)); // Release sys_trigger_config
            hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) | (1<<1)); // Assert sys_trigger_config_semi_manu
            hdmitx_device->hpd_event = 2;
#endif        
        }
    } else if (data32 & (1 << 2)) { //TX EDID interrupt
        if((hdmitx_device->cur_edid_block+2)<=EDID_MAX_BLOCK){
            int ii, jj;
            for(jj=0;jj<2;jj++){
#ifdef LOG_EDID
                int edid_log_pos=0;
                edid_log_pos+=snprintf((char*)hdmitx_device->tmp_buf+edid_log_pos, HDMI_TMP_BUF_SIZE-edid_log_pos, "EDID Interrupt cur block %d:",hdmitx_device->cur_edid_block);
#endif
                for(ii=0;ii<128;ii++){
                    hdmitx_device->EDID_buf[hdmitx_device->cur_edid_block*128+ii]
                        =hdmi_rd_reg(0x600+hdmitx_device->cur_phy_block_ptr*128+ii);
#ifdef LOG_EDID
                    if((ii&0xf)==0)
                        edid_log_pos+=snprintf((char*)hdmitx_device->tmp_buf+edid_log_pos, HDMI_TMP_BUF_SIZE-edid_log_pos, "\n");
                    edid_log_pos+=snprintf((char*)hdmitx_device->tmp_buf+edid_log_pos, HDMI_TMP_BUF_SIZE-edid_log_pos, "%02x ",hdmitx_device->EDID_buf[hdmitx_device->cur_edid_block*128+ii]);
#endif                    
                }
#ifdef LOG_EDID
                hdmitx_device->tmp_buf[edid_log_pos]=0;
                hdmi_print_buf((char*)hdmitx_device->tmp_buf, strlen((char*)hdmitx_device->tmp_buf));
                hdmi_print(0,"\n");
#endif
                hdmitx_device->cur_edid_block++;
                hdmitx_device->cur_phy_block_ptr++;
                hdmitx_device->cur_phy_block_ptr=hdmitx_device->cur_phy_block_ptr&0x3;
            }
        }        
#ifndef AML_A3
        /*walkaround: manually clear EDID interrupt, only for M1*/
        //hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) | (1<<1)); 
        //hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) & ~(1<<1)); 
        /**/
#endif        
        //tasklet_schedule(&EDID_tasklet);
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 2); //clear EDID rising interrupt in hdmi module 
    } else {
        hdmi_print(1,"HDMI Error: Unkown HDMI Interrupt source Process_Irq\n");
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  data32); //clear unkown interrupt in hdmi module 
    }
#ifdef AML_A3
    hdmi_rd_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR); // A read to allow the interrupt cleared in hdmi_module before next action
#endif    
#ifndef AVOS
    Wr(A9_0_IRQ_IN1_INTR_STAT_CLR, 1 << 25);  //clear hdmi_tx interrupt
    return IRQ_HANDLED;
#else 
    Wr(SYS_CPU_0_IRQ_IN1_INTR_STAT_CLR,(1 << INT_HDMI_TX));
    return ;
#endif
}

static void hdmi_tvenc1080i_set(Hdmi_tx_video_para_t* param)
{
    unsigned long VFIFO2VD_TO_HDMI_LATENCY = 3;
    unsigned long TOTAL_PIXELS, PIXEL_REPEAT_HDMI, PIXEL_REPEAT_VENC, ACTIVE_PIXELS;
    unsigned FRONT_PORCH, HSYNC_PIXELS, ACTIVE_LINES, INTERLACE_MODE, TOTAL_LINES, SOF_LINES, VSYNC_LINES;
    unsigned LINES_F0, LINES_F1,BACK_PORCH, EOF_LINES, TOTAL_FRAMES;

    unsigned long total_pixels_venc ;
    unsigned long active_pixels_venc;
    unsigned long front_porch_venc  ;
    unsigned long hsync_pixels_venc ;

    unsigned long de_h_begin, de_h_end;
    unsigned long de_v_begin_even, de_v_end_even, de_v_begin_odd, de_v_end_odd;
    unsigned long hs_begin, hs_end;
    unsigned long vs_adjust;
    unsigned long vs_bline_evn, vs_eline_evn, vs_bline_odd, vs_eline_odd;
    unsigned long vso_begin_evn, vso_begin_odd;
    
    if(param->VIC==HDMI_1080i60){
         INTERLACE_MODE     = 1;                   
         PIXEL_REPEAT_VENC  = 1;                   
         PIXEL_REPEAT_HDMI  = 0;                   
         ACTIVE_PIXELS  =     (1920*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES   =     (1080/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0           = 562;                 
         LINES_F1           = 563;                 
         FRONT_PORCH        = 88;                  
         HSYNC_PIXELS       = 44;                  
         BACK_PORCH         = 148;                  
         EOF_LINES          = 2;                   
         VSYNC_LINES        = 5;                   
         SOF_LINES          = 15;                  
         TOTAL_FRAMES       = 4;                   
    }
    else if(param->VIC==HDMI_1080i50){
         INTERLACE_MODE     = 1;                   
         PIXEL_REPEAT_VENC  = 1;                   
         PIXEL_REPEAT_HDMI  = 0;                   
         ACTIVE_PIXELS  =     (1920*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES   =     (1080/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0           = 562;                 
         LINES_F1           = 563;                 
         FRONT_PORCH        = 528;                  
         HSYNC_PIXELS       = 44;                  
         BACK_PORCH         = 148;                  
         EOF_LINES          = 2;                   
         VSYNC_LINES        = 5;                   
         SOF_LINES          = 15;                  
         TOTAL_FRAMES       = 4;                   
    }
    TOTAL_PIXELS =(FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS); // Number of total pixels per line.
    TOTAL_LINES  =(LINES_F0+(LINES_F1*INTERLACE_MODE));                // Number of total lines per frame.

    total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 2200 / 1 * 2 = 4400
    active_pixels_venc= (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 1920 / 1 * 2 = 3840
    front_porch_venc  = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 88   / 1 * 2 = 176
    hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 44   / 1 * 2 = 88

    hdmi_print(0, "[ENCP_VIDEO_MODE:%x]=%x\n",ENCP_VIDEO_MODE, Rd(ENCP_VIDEO_MODE)); 
    Wr(ENCP_VIDEO_MODE,Rd(ENCP_VIDEO_MODE)|(1<<14)); // cfg_de_v = 1

    // Program DE timing
    hdmi_print(0, "[ENCP_VIDEO_HAVON_BEGIN:%x]=%x\n",ENCP_VIDEO_HAVON_BEGIN, Rd(ENCP_VIDEO_HAVON_BEGIN)); 
    de_h_begin = modulo(Rd(ENCP_VIDEO_HAVON_BEGIN) + VFIFO2VD_TO_HDMI_LATENCY,  total_pixels_venc); // (383 + 3) % 4400 = 386
    de_h_end   = modulo(de_h_begin + active_pixels_venc,                        total_pixels_venc); // (386 + 3840) % 4400 = 4226
    Wr(ENCP_DE_H_BEGIN, de_h_begin);    // 386
    Wr(ENCP_DE_H_END,   de_h_end);      // 4226
    // Program DE timing for even field
    hdmi_print(0, "[ENCP_VIDEO_VAVON_BLINE:%x]=%x\n",ENCP_VIDEO_VAVON_BLINE, Rd(ENCP_VIDEO_VAVON_BLINE)); 
    de_v_begin_even = Rd(ENCP_VIDEO_VAVON_BLINE);       // 20
    de_v_end_even   = de_v_begin_even + ACTIVE_LINES;   // 20 + 540 = 560
    Wr(ENCP_DE_V_BEGIN_EVEN,de_v_begin_even);   // 20
    Wr(ENCP_DE_V_END_EVEN,  de_v_end_even);     // 560
    // Program DE timing for odd field if needed
    if (INTERLACE_MODE) {
        // Calculate de_v_begin_odd according to enc480p_timing.v:
        //wire[10:0]	cfg_ofld_vavon_bline	= {{7{ofld_vavon_ofst1 [3]}},ofld_vavon_ofst1 [3:0]} + cfg_video_vavon_bline	+ ofld_line;
        hdmi_print(0, "[ENCP_VIDEO_OFLD_VOAV_OFST:%x]=%x\n",ENCP_VIDEO_OFLD_VOAV_OFST, Rd(ENCP_VIDEO_OFLD_VOAV_OFST)); 
        de_v_begin_odd  = to_signed((Rd(ENCP_VIDEO_OFLD_VOAV_OFST) & 0xf0)>>4) + de_v_begin_even + (TOTAL_LINES-1)/2; // 1 + 20 + (1125-1)/2 = 583
        de_v_end_odd    = de_v_begin_odd + ACTIVE_LINES;    // 583 + 540 = 1123
        Wr(ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);// 583
        Wr(ENCP_DE_V_END_ODD,   de_v_end_odd);  // 1123
    }

    // Program Hsync timing
    if (de_h_end + front_porch_venc >= total_pixels_venc) {
        hs_begin    = de_h_end + front_porch_venc - total_pixels_venc; // 4226 + 176 - 4400 = 2
        vs_adjust   = 1;
    } else {
        hs_begin    = de_h_end + front_porch_venc;
        vs_adjust   = 0;
    }
    hs_end  = modulo(hs_begin + hsync_pixels_venc,   total_pixels_venc); // (2 + 88) % 4400 = 90
    Wr(ENCP_DVI_HSO_BEGIN,  hs_begin);  // 2
    Wr(ENCP_DVI_HSO_END,    hs_end);    // 90
    
    // Program Vsync timing for even field
    if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust)) {
        vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES - (1-vs_adjust); // 20 - 15 - 5 - 0 = 0
    } else {
        vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES - VSYNC_LINES - (1-vs_adjust);
    }
    vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES); // (0 + 5) % 1125 = 5
    Wr(ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);   // 0
    Wr(ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);   // 5
    vso_begin_evn = hs_begin; // 2
    Wr(ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);  // 2
    Wr(ENCP_DVI_VSO_END_EVN,   vso_begin_evn);  // 2
    // Program Vsync timing for odd field if needed
    if (INTERLACE_MODE) {
        vs_bline_odd = de_v_begin_odd-1 - SOF_LINES - VSYNC_LINES;  // 583-1 - 15 - 5   = 562
        vs_eline_odd = de_v_begin_odd-1 - SOF_LINES;                // 583-1 - 15       = 567
        vso_begin_odd   = modulo(hs_begin + (total_pixels_venc>>1), total_pixels_venc); // (2 + 4400/2) % 4400 = 2202
        Wr(ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);   // 562
        Wr(ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);   // 567
        Wr(ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);  // 2202
        Wr(ENCP_DVI_VSO_END_ODD,   vso_begin_odd);  // 2202
    }

    Wr(VENC_DVI_SETTING, (1 << 0)               | //0=select enci hs/vs; 1=select encp hs/vs
                         (0 << 1)               | //select vso/hso as hsync vsync
                         (HSYNC_POLARITY << 2)  | //invert hs
                         (VSYNC_POLARITY << 3)  | //invert vs
#ifdef DOUBLE_CLK_720P_1080I
                         (5 << 4)               | //select vclk1 as HDMI pixel clk
#else                         
                         (2 << 4)               | //select vclk1 as HDMI pixel clk
#endif                         
                         (1 << 7)               | //0=sel external dvi; 1= sel internal hdmi
                         (0 << 8)               | //no invert clk
                         (0 << 13)              | //cfg_dvi_mode_gamma_en
                         (1 << 15)                //select encp_vs_dvi, encp_hs_dvi and encp_de as timing signal
    );
    
    Wr(VENC_DVI_SETTING_MORE, (TX_INPUT_COLOR_FORMAT==0)? 1 : 0); // [0] 0=Map data pins from Venc to Hdmi Tx as CrYCb mode;
    
}    

static void hdmi_tvenc480i_set(Hdmi_tx_video_para_t* param)
{
    unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
    unsigned long TOTAL_PIXELS, PIXEL_REPEAT_HDMI, PIXEL_REPEAT_VENC, ACTIVE_PIXELS;
    unsigned FRONT_PORCH, HSYNC_PIXELS, ACTIVE_LINES, INTERLACE_MODE, TOTAL_LINES, SOF_LINES, VSYNC_LINES;
    unsigned LINES_F0, LINES_F1,BACK_PORCH, EOF_LINES, TOTAL_FRAMES;

    unsigned long total_pixels_venc ;
    unsigned long active_pixels_venc;
    unsigned long front_porch_venc  ;
    unsigned long hsync_pixels_venc ;

    unsigned long de_h_begin, de_h_end;
    unsigned long de_v_begin_even, de_v_end_even, de_v_begin_odd, de_v_end_odd;
    unsigned long hs_begin, hs_end;
    unsigned long vs_adjust;
    unsigned long vs_bline_evn, vs_eline_evn, vs_bline_odd, vs_eline_odd;
    unsigned long vso_begin_evn, vso_begin_odd;

    if((param->VIC==HDMI_480i60)||(param->VIC==HDMI_480i60_16x9)){
         INTERLACE_MODE     = 1;                   
         PIXEL_REPEAT_VENC  = 1;                   
         PIXEL_REPEAT_HDMI  = 1;                   
         ACTIVE_PIXELS  =     (720*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES   =     (480/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0           = 262;                 
         LINES_F1           = 263;                 
         FRONT_PORCH        = 38;                  
         HSYNC_PIXELS       = 124;                  
         BACK_PORCH         = 114;                  
         EOF_LINES          = 4;                   
         VSYNC_LINES        = 3;                   
         SOF_LINES          = 15;                  
         TOTAL_FRAMES       = 4;                   
    }
    else if((param->VIC==HDMI_576i50)||(param->VIC==HDMI_576i50_16x9)){
         INTERLACE_MODE     = 1;                   
         PIXEL_REPEAT_VENC  = 1;                   
         PIXEL_REPEAT_HDMI  = 1;                   
         ACTIVE_PIXELS  =     (720*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES   =     (576/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0           = 312;                 
         LINES_F1           = 313;                 
         FRONT_PORCH        = 24;                  
         HSYNC_PIXELS       = 126;                  
         BACK_PORCH         = 138;                  
         EOF_LINES          = 2;                   
         VSYNC_LINES        = 3;                   
         SOF_LINES          = 19;                  
         TOTAL_FRAMES       = 4;                   
    }
    TOTAL_PIXELS =(FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS); // Number of total pixels per line.
    TOTAL_LINES  =(LINES_F0+(LINES_F1*INTERLACE_MODE));                // Number of total lines per frame.

    total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 1716 / 2 * 2 = 1716
    active_pixels_venc= (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 1440 / 2 * 2 = 1440
    front_porch_venc  = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 38   / 2 * 2 = 38
    hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 124  / 2 * 2 = 124

    hdmi_print(0, "[ENCP_VIDEO_MODE:%x]=%x\n",ENCP_VIDEO_MODE, Rd(ENCP_VIDEO_MODE)); 
    Wr(ENCP_VIDEO_MODE,Rd(ENCP_VIDEO_MODE)|(1<<14)); // cfg_de_v = 1

    // Program DE timing
    hdmi_print(0, "[VFIFO2VD_PIXEL_START:%x]=%x\n",VFIFO2VD_PIXEL_START, Rd(VFIFO2VD_PIXEL_START)); 
    de_h_begin = modulo(Rd(VFIFO2VD_PIXEL_START) + VFIFO2VD_TO_HDMI_LATENCY,    total_pixels_venc); // (233 + 2) % 1716 = 235
    de_h_end   = modulo(de_h_begin + active_pixels_venc,                        total_pixels_venc); // (235 + 1440) % 1716 = 1675
    Wr(ENCI_DE_H_BEGIN, de_h_begin);    // 235
    Wr(ENCI_DE_H_END,   de_h_end);      // 1675

    hdmi_print(0, "[VFIFO2VD_LINE_TOP_START:%x]=%x\n",VFIFO2VD_LINE_TOP_START, Rd(VFIFO2VD_LINE_TOP_START)); 
    hdmi_print(0, "[VFIFO2VD_LINE_BOT_START:%x]=%x\n",VFIFO2VD_LINE_BOT_START, Rd(VFIFO2VD_LINE_BOT_START)); 
    de_v_begin_even = Rd(VFIFO2VD_LINE_TOP_START);      // 17
    de_v_end_even   = de_v_begin_even + ACTIVE_LINES;   // 17 + 240 = 257
    de_v_begin_odd  = Rd(VFIFO2VD_LINE_BOT_START);      // 18
    de_v_end_odd    = de_v_begin_odd + ACTIVE_LINES;    // 18 + 480/2 = 258
    Wr(ENCI_DE_V_BEGIN_EVEN,de_v_begin_even);   // 17
    Wr(ENCI_DE_V_END_EVEN,  de_v_end_even);     // 257
    Wr(ENCI_DE_V_BEGIN_ODD, de_v_begin_odd);    // 18
    Wr(ENCI_DE_V_END_ODD,   de_v_end_odd);      // 258

    // Program Hsync timing
    if (de_h_end + front_porch_venc >= total_pixels_venc) {
        hs_begin    = de_h_end + front_porch_venc - total_pixels_venc;
        vs_adjust   = 1;
    } else {
        hs_begin    = de_h_end + front_porch_venc; // 1675 + 38 = 1713
        vs_adjust   = 0;
    }
    hs_end  = modulo(hs_begin + hsync_pixels_venc,   total_pixels_venc); // (1713 + 124) % 1716 = 121
    Wr(ENCI_DVI_HSO_BEGIN,  hs_begin);  // 1713
    Wr(ENCI_DVI_HSO_END,    hs_end);    // 121
    
    // Program Vsync timing for even field
    if (de_v_end_odd-1 + EOF_LINES + vs_adjust >= LINES_F1) {
        vs_bline_evn = de_v_end_odd-1 + EOF_LINES + vs_adjust - LINES_F1;
        vs_eline_evn = vs_bline_evn + VSYNC_LINES;
        Wr(ENCI_DVI_VSO_BLINE_EVN, vs_bline_evn);
        //vso_bline_evn_reg_wr_cnt ++;
        Wr(ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
        //vso_eline_evn_reg_wr_cnt ++;
        Wr(ENCI_DVI_VSO_BEGIN_EVN, hs_begin);
        Wr(ENCI_DVI_VSO_END_EVN,   hs_begin);
    } else {
        vs_bline_odd = de_v_end_odd-1 + EOF_LINES + vs_adjust; // 258-1 + 4 + 0 = 261
        Wr(ENCI_DVI_VSO_BLINE_ODD, vs_bline_odd); // 261
        //vso_bline_odd_reg_wr_cnt ++;
        Wr(ENCI_DVI_VSO_BEGIN_ODD, hs_begin);  // 1713
        if (vs_bline_odd + VSYNC_LINES >= LINES_F1) {
            vs_eline_evn = vs_bline_odd + VSYNC_LINES - LINES_F1; // 261 + 3 - 263 = 1
            Wr(ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);   // 1
            //vso_eline_evn_reg_wr_cnt ++;
            Wr(ENCI_DVI_VSO_END_EVN,   hs_begin);       // 1713
        } else {
            vs_eline_odd = vs_bline_odd + VSYNC_LINES;
            Wr(ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
            //vso_eline_odd_reg_wr_cnt ++;
            Wr(ENCI_DVI_VSO_END_ODD,   hs_begin);
        }
    }
    // Program Vsync timing for odd field
    if (de_v_end_even-1 + EOF_LINES + 1 >= LINES_F0) {
        vs_bline_odd = de_v_end_even-1 + EOF_LINES + 1 - LINES_F0;
        vs_eline_odd = vs_bline_odd + VSYNC_LINES;
        Wr(ENCI_DVI_VSO_BLINE_ODD, vs_bline_odd);
        //vso_bline_odd_reg_wr_cnt ++;
        Wr(ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
        //vso_eline_odd_reg_wr_cnt ++;
        vso_begin_odd   = modulo(hs_begin + (total_pixels_venc>>1), total_pixels_venc);
        Wr(ENCI_DVI_VSO_BEGIN_ODD, vso_begin_odd);
        Wr(ENCI_DVI_VSO_END_ODD,   vso_begin_odd);
    } else {
        vs_bline_evn = de_v_end_even-1 + EOF_LINES + 1; // 257-1 + 4 + 1 = 261
        Wr(ENCI_DVI_VSO_BLINE_EVN, vs_bline_evn); // 261
        //vso_bline_evn_reg_wr_cnt ++;
        vso_begin_evn   = modulo(hs_begin + (total_pixels_venc>>1), total_pixels_venc);   // (1713 + 1716/2) % 1716 = 855
        Wr(ENCI_DVI_VSO_BEGIN_EVN, vso_begin_evn);  // 855
        if (vs_bline_evn + VSYNC_LINES >= LINES_F0) {
            vs_eline_odd = vs_bline_evn + VSYNC_LINES - LINES_F0; // 261 + 3 - 262 = 2
            Wr(ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);   // 2
            //vso_eline_odd_reg_wr_cnt ++;
            Wr(ENCI_DVI_VSO_END_ODD,   vso_begin_evn);  // 855
        } else {
            vs_eline_evn = vs_bline_evn + VSYNC_LINES;
            Wr(ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
            //vso_eline_evn_reg_wr_cnt ++;
            Wr(ENCI_DVI_VSO_END_EVN,   vso_begin_evn);
        }
    }

    // Check if there are duplicate or missing timing settings
    //if ((vso_bline_evn_reg_wr_cnt != 1) || (vso_bline_odd_reg_wr_cnt != 1) ||
    //    (vso_eline_evn_reg_wr_cnt != 1) || (vso_eline_odd_reg_wr_cnt != 1)) {
        //stimulus_print("[TEST.C] Error: Multiple or missing timing settings on reg ENCI_DVI_VSO_B(E)LINE_EVN(ODD)!\n");
        //stimulus_finish_fail(1);
    //}

    Wr(VENC_DVI_SETTING, (0 << 0)               | //0=select enci hs/vs; 1=select encp hs/vs
                         (0 << 1)               | //select vso/hso as hsync vsync
                         (HSYNC_POLARITY << 2)  | //invert hs
                         (VSYNC_POLARITY << 3)  | //invert vs
                         (1 << 4)               | //select clk54 as clk
                         (1 << 7)               | //0=sel external dvi; 1= sel internal hdmi
                         (0 << 8)               | //no invert clk
                         (0 << 13)              | //cfg_dvi_mode_gamma_en
                         (1 << 15)                //select enci_vs_dvi, enci_hs_dvi and intl_de as timing signal
    );
    
    Wr(VENC_DVI_SETTING_MORE, (TX_INPUT_COLOR_FORMAT==0)? 1 : 0); // [0] 0=Map data pins from Venc to Hdmi Tx as CrYCb mode;
                                                                  //     1=Map data pins from Venc to Hdmi Tx as RGB mode.
    
}    

#ifndef AVOS
static 
#endif
void hdmi_tvenc_set(Hdmi_tx_video_para_t *param)
{
    unsigned long VFIFO2VD_TO_HDMI_LATENCY = 3;
    unsigned long TOTAL_PIXELS, PIXEL_REPEAT_HDMI, PIXEL_REPEAT_VENC, ACTIVE_PIXELS;
    unsigned FRONT_PORCH, HSYNC_PIXELS, ACTIVE_LINES, INTERLACE_MODE, TOTAL_LINES, SOF_LINES, VSYNC_LINES;
    unsigned LINES_F0, LINES_F1,BACK_PORCH, EOF_LINES, TOTAL_FRAMES;

    unsigned long total_pixels_venc ;
    unsigned long active_pixels_venc;
    unsigned long front_porch_venc  ;
    unsigned long hsync_pixels_venc ;

    unsigned long de_h_begin, de_h_end;
    unsigned long de_v_begin_even, de_v_end_even, de_v_begin_odd, de_v_end_odd;
    unsigned long hs_begin, hs_end;
    unsigned long vs_adjust;
    unsigned long vs_bline_evn, vs_eline_evn, vs_bline_odd, vs_eline_odd;
    unsigned long vso_begin_evn, vso_begin_odd;

    if((param->VIC==HDMI_480p60)||(param->VIC==HDMI_480p60_16x9)){
         INTERLACE_MODE     = 0;                   
         PIXEL_REPEAT_VENC  = 1;                   
         PIXEL_REPEAT_HDMI  = 0;                   
         ACTIVE_PIXELS      = (720*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES       = (480/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0           = 525;                 
         LINES_F1           = 525;                 
         FRONT_PORCH        = 16;                  
         HSYNC_PIXELS       = 62;                  
         BACK_PORCH         = 60;                  
         EOF_LINES          = 9;                   
         VSYNC_LINES        = 6;                   
         SOF_LINES          = 30;                  
         TOTAL_FRAMES       = 4;                   
    }
    else if((param->VIC==HDMI_576p50)||(param->VIC==HDMI_576p50_16x9)){
         INTERLACE_MODE     = 0;                   
         PIXEL_REPEAT_VENC  = 1;                   
         PIXEL_REPEAT_HDMI  = 0;                   
         ACTIVE_PIXELS      = (720*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES       = (576/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0           = 625;                 
         LINES_F1           = 625;                 
         FRONT_PORCH        = 12;                  
         HSYNC_PIXELS       = 64;                  
         BACK_PORCH         = 68;                  
         EOF_LINES          = 5;                   
         VSYNC_LINES        = 5;                   
         SOF_LINES          = 39;                  
         TOTAL_FRAMES       = 4;                   
    }
    else if(param->VIC==HDMI_720p60){
         INTERLACE_MODE     = 0;                   
         PIXEL_REPEAT_VENC  = 1;                   
         PIXEL_REPEAT_HDMI  = 0;                   
         ACTIVE_PIXELS      = (1280*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES       = (720/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0           = 750;                 
         LINES_F1           = 750;                 
         FRONT_PORCH        = 110;                  
         HSYNC_PIXELS       = 40;                  
         BACK_PORCH         = 220;                  
         EOF_LINES          = 5;                   
         VSYNC_LINES        = 5;                   
         SOF_LINES          = 20;                  
         TOTAL_FRAMES       = 4;                   
    }
    else if(param->VIC==HDMI_720p50){
         INTERLACE_MODE     = 0;                   
         PIXEL_REPEAT_VENC  = 1;                   
         PIXEL_REPEAT_HDMI  = 0;                   
         ACTIVE_PIXELS      = (1280*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES       = (720/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0           = 750;                 
         LINES_F1           = 750;                 
         FRONT_PORCH        = 440;                  
         HSYNC_PIXELS       = 40;                  
         BACK_PORCH         = 220;                  
         EOF_LINES          = 5;                   
         VSYNC_LINES        = 5;                   
         SOF_LINES          = 20;                  
         TOTAL_FRAMES       = 4;                   
    }
    else if(param->VIC==HDMI_1080p50){
         INTERLACE_MODE      =0;              
         PIXEL_REPEAT_VENC   =0;              
         PIXEL_REPEAT_HDMI   =0;              
         ACTIVE_PIXELS       =(1920*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES        =(1080/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0            =1125;           
         LINES_F1            =1125;           
         FRONT_PORCH         =528;             
         HSYNC_PIXELS        =44;             
         BACK_PORCH          =148;            
         EOF_LINES           =4;              
         VSYNC_LINES         =5;              
         SOF_LINES           =36;             
         TOTAL_FRAMES        =4;              
    }
    else{ //HDMI_1080p60, HDMI_1080p30
         INTERLACE_MODE      =0;              
         PIXEL_REPEAT_VENC   =0;              
         PIXEL_REPEAT_HDMI   =0;              
         ACTIVE_PIXELS       =(1920*(1+PIXEL_REPEAT_HDMI)); // Number of active pixels per line.
         ACTIVE_LINES        =(1080/(1+INTERLACE_MODE));    // Number of active lines per field.
         LINES_F0            =1125;           
         LINES_F1            =1125;           
         FRONT_PORCH         =88;             
         HSYNC_PIXELS        =44;             
         BACK_PORCH          =148;            
         EOF_LINES           =4;              
         VSYNC_LINES         =5;              
         SOF_LINES           =36;             
         TOTAL_FRAMES        =4;              
    }

    TOTAL_PIXELS       = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS); // Number of total pixels per line.
    TOTAL_LINES        = (LINES_F0+(LINES_F1*INTERLACE_MODE));                // Number of total lines per frame.

    total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 858 / 1 * 2 = 1716
    active_pixels_venc= (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 720 / 1 * 2 = 1440
    front_porch_venc  = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 16   / 1 * 2 = 32
    hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC); // 62   / 1 * 2 = 124

    hdmi_print(0, "[ENCP_VIDEO_MODE:%x]=%x\n",ENCP_VIDEO_MODE, Rd(ENCP_VIDEO_MODE)); 
    Wr(ENCP_VIDEO_MODE,Rd(ENCP_VIDEO_MODE)|(1<<14)); // cfg_de_v = 1
    // Program DE timing
    hdmi_print(0, "[ENCP_VIDEO_HAVON_BEGIN:%x]=%x\n",ENCP_VIDEO_HAVON_BEGIN, Rd(ENCP_VIDEO_HAVON_BEGIN)); 
    de_h_begin = modulo(Rd(ENCP_VIDEO_HAVON_BEGIN) + VFIFO2VD_TO_HDMI_LATENCY,  total_pixels_venc); // (217 + 3) % 1716 = 220
    de_h_end   = modulo(de_h_begin + active_pixels_venc,                        total_pixels_venc); // (220 + 1440) % 1716 = 1660
    Wr(ENCP_DE_H_BEGIN, de_h_begin);    // 220
    Wr(ENCP_DE_H_END,   de_h_end);      // 1660
    // Program DE timing for even field
    hdmi_print(0, "[ENCP_VIDEO_VAVON_BLINE:%x]=%x\n",ENCP_VIDEO_VAVON_BLINE, Rd(ENCP_VIDEO_VAVON_BLINE)); 
    de_v_begin_even = Rd(ENCP_VIDEO_VAVON_BLINE);       // 42
    de_v_end_even   = de_v_begin_even + ACTIVE_LINES;   // 42 + 480 = 522
    Wr(ENCP_DE_V_BEGIN_EVEN,de_v_begin_even);   // 42
    Wr(ENCP_DE_V_END_EVEN,  de_v_end_even);     // 522
    // Program DE timing for odd field if needed
    if (INTERLACE_MODE) {
        // Calculate de_v_begin_odd according to enc480p_timing.v:
        //wire[10:0]    cfg_ofld_vavon_bline    = {{7{ofld_vavon_ofst1 [3]}},ofld_vavon_ofst1 [3:0]} + cfg_video_vavon_bline    + ofld_line;
        hdmi_print(0, "[ENCP_VIDEO_OFLD_VOAV_OFST:%x]=%x\n",ENCP_VIDEO_OFLD_VOAV_OFST, Rd(ENCP_VIDEO_OFLD_VOAV_OFST)); 
        de_v_begin_odd  = to_signed((Rd(ENCP_VIDEO_OFLD_VOAV_OFST) & 0xf0)>>4) + de_v_begin_even + (TOTAL_LINES-1)/2;
        de_v_end_odd    = de_v_begin_odd + ACTIVE_LINES;
        Wr(ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);
        Wr(ENCP_DE_V_END_ODD,   de_v_end_odd);
    }

    // Program Hsync timing
    if (de_h_end + front_porch_venc >= total_pixels_venc) {
        hs_begin    = de_h_end + front_porch_venc - total_pixels_venc;
        vs_adjust   = 1;
    } else {
        hs_begin    = de_h_end + front_porch_venc; // 1660 + 32 = 1692
        vs_adjust   = 0;
    }
    hs_end  = modulo(hs_begin + hsync_pixels_venc,   total_pixels_venc); // (1692 + 124) % 1716 = 100
    Wr(ENCP_DVI_HSO_BEGIN,  hs_begin);  // 1692
    Wr(ENCP_DVI_HSO_END,    hs_end);    // 100
    
    // Program Vsync timing for even field
    if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust)) {
        vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES - (1-vs_adjust); // 42 - 30 - 6 - 1 = 5
    } else {
        vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES - VSYNC_LINES - (1-vs_adjust);
    }
    vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES); // (5 + 6) % 525 = 11
    Wr(ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);   // 5
    Wr(ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);   // 11
    vso_begin_evn = hs_begin; // 1692
    Wr(ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);  // 1692
    Wr(ENCP_DVI_VSO_END_EVN,   vso_begin_evn);  // 1692
    // Program Vsync timing for odd field if needed
    if (INTERLACE_MODE) {
        vs_bline_odd = de_v_begin_odd-1 - SOF_LINES - VSYNC_LINES;
        vs_eline_odd = de_v_begin_odd-1 - SOF_LINES;
        vso_begin_odd   = modulo(hs_begin + (total_pixels_venc>>1), total_pixels_venc);
        Wr(ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);
        Wr(ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);
        Wr(ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);
        Wr(ENCP_DVI_VSO_END_ODD,   vso_begin_odd);
    }
    Wr(VENC_DVI_SETTING_MORE, (TX_INPUT_COLOR_FORMAT==0)? 1 : 0); // [0] 0=Map data pins from Venc to Hdmi Tx as CrYCb mode;
    
    switch(param->VIC)
    {
        case HDMI_480p60:
        case HDMI_480p60_16x9:
        case HDMI_576p50:
        case HDMI_576p50_16x9:
//Note: Hsync & Vsync polarity should be negative.
//Refer to HDMI CTS 1.4A Page 169
            Wr(VENC_DVI_SETTING, (1 << 0)               | //0=select enci hs/vs; 1=select encp hs/vs
                                 (0 << 1)               | //select vso/hso as hsync vsync
                                 (0 << 2)  | //invert hs
                                 (0 << 3)  | //invert vs
                                 (2 << 4)               | //select encp_dvi_clk = clk54/2
                                 (1 << 7)               | //0=sel external dvi; 1= sel internal hdmi
                                 (0 << 8)               | //no invert clk
                                 (0 << 13)              | //cfg_dvi_mode_gamma_en
                                 (1 << 15)                //select encp_vs_dvi, encp_hs_dvi and encp_de as timing signal
              );
            break;
        case HDMI_720p60:
        case HDMI_720p50:
            Wr(VENC_DVI_SETTING, (1 << 0)               | //0=select enci hs/vs; 1=select encp hs/vs
                                 (0 << 1)               | //select vso/hso as hsync vsync
                                 (HSYNC_POLARITY << 2)  | //invert hs
                                 (VSYNC_POLARITY << 3)  | //invert vs
#ifdef DOUBLE_CLK_720P_1080I
                                (1 << 4)               | //select encp_dvi_clk = clk54/2
#else
                                (2 << 4)               | //select encp_dvi_clk = clk54/2
#endif                             
                                 (1 << 7)               | //0=sel external dvi; 1= sel internal hdmi
                                 (0 << 8)               | //no invert clk
                                 (0 << 13)              | //cfg_dvi_mode_gamma_en
                                 (1 << 15)                //select encp_vs_dvi, encp_hs_dvi and encp_de as timing signal
              );
            break;
        default:
            Wr(VENC_DVI_SETTING, (1 << 0)               | //0=select enci hs/vs; 1=select encp hs/vs
                                 (0 << 1)               | //select vso/hso as hsync vsync
                                 (HSYNC_POLARITY << 2)  | //invert hs
                                 (VSYNC_POLARITY << 3)  | //invert vs
                                 (1 << 4)               | //select clk54 as clk
                                 (1 << 7)               | //0=sel external dvi; 1= sel internal hdmi
                                 (0 << 8)               | //no invert clk
                                 (0 << 13)              | //cfg_dvi_mode_gamma_en
                                 (1 << 15)                //select encp_vs_dvi, encp_hs_dvi and encp_de as timing signal
              );
    }
}    


/*
hdmi on/off
*/
static int is_hpd_muxed(void)
{
    int ret;
#ifdef AML_A3
    ret = Rd(PERIPHS_PIN_MUX_0)&(1<<0);
#else    
    ret = Rd(PERIPHS_PIN_MUX_0)&(1<<1);
#endif
    return ret; 
}    

static void mux_hpd(void)
{
#ifdef AML_A3
    Wr(PERIPHS_PIN_MUX_0, Rd(PERIPHS_PIN_MUX_0)|(1 << 0));  // pm_gpioA_0_hdmi_hpd
#else
    Wr(PERIPHS_PIN_MUX_0, Rd(PERIPHS_PIN_MUX_0)|(1 << 1)); // pm_hdmi_hpd_5v_en
#endif        
}

static void unmux_hpd(void)
{
#ifdef AML_A3
    Wr(PERIPHS_PIN_MUX_0, Rd(PERIPHS_PIN_MUX_0)&(~(1<<0))); //use hpd as gpio
#else
    Wr(PERIPHS_PIN_MUX_0, Rd(PERIPHS_PIN_MUX_0)&(~(1 << 1))); //use hpd as gpio
#endif        
    Wr(PREG_EGPIO_EN_N, Rd(PREG_EGPIO_EN_N)|(1<<0)); //GPIOA_0 as input
}    

static int read_hpd_gpio(void)
{
    int level;
    level = Rd(PREG_EGPIO_I)&0x1; //read GPIOA_0
    return level;
}

static void digital_clk_off(unsigned char flag)
{
    if(flag&1){
#ifdef AML_A3
    /* off hdmi audio clock */
        hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_CTRL1,  hdmi_rd_reg(OTHER_BASE_ADDR + HDMI_OTHER_CTRL1)&(~(1<<13))); //
        hdmi_wr_reg(TX_AUDIO_FORMAT, hdmi_rd_reg(TX_AUDIO_FORMAT)|(1<<7));
        hdmi_wr_reg(TX_AUDIO_I2S, hdmi_rd_reg(TX_AUDIO_I2S)|(1<<1));
#endif    
    }

    if(flag&2){
        /* off hdmi pixel clock */
        Wr(HHI_GCLK_OTHER, Rd(HHI_GCLK_OTHER)&(~(1<<17))); //disable pixel clock, set cbus reg HHI_GCLK_OTHER bit [17] = 0
        Wr(VENC_DVI_SETTING, (Rd(VENC_DVI_SETTING)&(~(7<<4)))|(5<<4)); //set cbus reg VENC_DVI_SETTING bit[6:4] = 0x5
    }
    if(flag&4){
        /* off hdmi sys clock */
        Wr( HHI_HDMI_CLK_CNTL, Rd(HHI_HDMI_CLK_CNTL)&(~(1<<8))); // off hdmi sys clock gate
    }
}

static void digital_clk_on(unsigned char flag)
{
    if(flag&4){
        /* on hdmi sys clock */
#ifdef AML_A3
        // -----------------------------------------
        // HDMI (90Mhz)
        // -----------------------------------------
        //         .clk_div            ( hi_hdmi_clk_cntl[6:0] ),
        //         .clk_en             ( hi_hdmi_clk_cntl[8]   ),
        //         .clk_sel            ( hi_hdmi_clk_cntl[11:9]),
        Wr( HHI_HDMI_CLK_CNTL,  ((2 << 9)  |   // select "misc" PLL
                                 (1 << 8)  |   // Enable gated clock
                                 (5 << 0)) );  // Divide the "other" PLL output by 6
    
#else    
        // -----------------------------------------
        // HDMI (90Mhz)
        // -----------------------------------------
        //         .clk_div            ( hi_hdmi_clk_cntl[6:0] ),
        //         .clk_en             ( hi_hdmi_clk_cntl[8]   ),
        //         .clk_sel            ( hi_hdmi_clk_cntl[11:9]),
        Wr( HHI_HDMI_CLK_CNTL,  ((0 << 9)  |   // select XTAL 24MHz PLL
                                 (1 << 8)  |   // Enable gated clock
                                 (0 << 0)) );  // Divide the "other" PLL output by 6
#endif
    }
    if(flag&2){
        /* on hdmi pixel clock */
        Wr(HHI_GCLK_OTHER, Rd(HHI_GCLK_OTHER)|(1<<17)); //enable pixel clock, set cbus reg HHI_GCLK_OTHER bit [17] = 1
    }
    if(flag&1){
#ifdef AML_A3
        /* on hdmi audio clock */
        hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_CTRL1,  hdmi_rd_reg(OTHER_BASE_ADDR + HDMI_OTHER_CTRL1)|(1<<13)); 
#endif  
    }  
}

static void phy_pll_off(void)
{
    /* power down hdmi phy */
    hdmi_wr_reg(TX_CORE_CALIB_MODE, 0x8);
    hdmi_wr_reg(TX_SYS1_TERMINATION, hdmi_rd_reg(TX_SYS1_TERMINATION)&(~0xf));
    hdmi_wr_reg(TX_SYS1_AFE_SPARE0, hdmi_rd_reg(TX_SYS1_AFE_SPARE0)&(~0xf));
    hdmi_wr_reg(TX_SYS1_AFE_TEST, hdmi_rd_reg(TX_SYS1_AFE_TEST)&(~0x1f));
        /**/
#ifndef AML_A3
        /* no HDMI PLL in A3 */
    Wr(HHI_HDMI_PLL_CNTL, Rd(HHI_HDMI_PLL_CNTL)|(1<<30)); //disable HDMI PLL
    Wr(HHI_HDMI_PLL_CNTL2, Rd(HHI_HDMI_PLL_CNTL2)&(~0x38));
#endif   
    
}

/**/
void hdmi_hw_set_powermode( int power_mode, int vic)
{
    switch(power_mode){
        case 1:
            hdmi_wr_reg(0x016, 0x02);
            hdmi_wr_reg(0x014, 0x02);  

            hdmi_wr_reg(TX_CORE_CALIB_MODE, 0xc);
            hdmi_wr_reg(TX_CORE_CALIB_VALUE, 0x0);
            switch(vic)
            {
                case HDMI_480p60:
                case HDMI_480p60_16x9:
                case HDMI_720p60:
                    hdmi_wr_reg(TX_SYS1_AFE_TEST, 0x1f);    //0x17
                    hdmi_wr_reg(TX_CORE_CALIB_VALUE,0x3);   //0xf7
                    hdmi_wr_reg(TX_SYS1_AFE_RESET, 0x1);    //0x16
                    hdmi_wr_reg(TX_SYS1_BANDGAP, 0x0);      //0x14
                    hdmi_wr_reg(TX_SYS1_BIAS, 0x0);         //0x15
                    break;
                case HDMI_1080p60:
                    hdmi_wr_reg(TX_SYS1_AFE_TEST, 0x1f);     //0x17
                    hdmi_wr_reg(TX_CORE_CALIB_VALUE, 0x9);   //0xf7
                    hdmi_wr_reg(TX_SYS1_AFE_RESET, 0x2);     //0x16   //Def.
                    //hdmi_wr_reg(TX_SYS1_AFE_RESET, 0x3);     //0x16   //for HDMICT#1, 1080P, Vswing is low.
                    hdmi_wr_reg(TX_SYS1_BANDGAP, 0x2);       //0x14
                    hdmi_wr_reg(TX_SYS1_BIAS, 0x3);          //0x15
                default:
                    break;
            }
#ifdef MORE_LOW_P
            hdmi_wr_reg(0x010, 0x0);
            hdmi_wr_reg(0x01a, 0x3);
#endif        
            break;
        case 2:
            hdmi_wr_reg(0x017, 0x1f);
            hdmi_wr_reg(0x016, 0x01);
            hdmi_wr_reg(0x015, 0x03);
            hdmi_wr_reg(0x0ec, 0x0f);
            if(vic == HDMI_1080p60){
                hdmi_wr_reg(0x014, 0x02); //for 1080p only
            }
            else{
                hdmi_wr_reg(0x014, 0x0); 
            }
            break;
        default:
            hdmi_wr_reg(0x016, 0x03); //hdmi_wr_reg(0x016, 0x04);   // Bit[3:0] is HDMI-PHY's output swing control register
            hdmi_wr_reg(TX_CORE_CALIB_MODE, 0x8);
            hdmi_wr_reg(TX_CORE_CALIB_VALUE, 0xf);
#ifdef MORE_LOW_P
            hdmi_wr_reg(0x010, 0x3);
            hdmi_wr_reg(0x01a, 0xfb);
#endif            
            break;
    }
}

void hdmi_hw_init(hdmitx_dev_t* hdmitx_device)
{
    unsigned int tmp_add_data;
    
    digital_clk_on(7);
    hdmi_wr_reg(0x011, hdmi_rd_reg(0x011)&(~0x0f));
    cec_set_pending(TV_CEC_PENDING_ON);
#ifndef AML_A3
    if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
        Wr(HHI_HDMI_PLL_CNTL2, 0x50e8);
    }
    else{
        Wr(HHI_HDMI_PLL_CNTL2, 0x40e8);
    }

    Wr(HHI_HDMI_PLL_CNTL, 0x03040502); 

    Wr(HHI_HDMI_PLL_CNTL1, 0x00040003);
#endif
    Wr(HHI_HDMI_AFC_CNTL, Rd(HHI_HDMI_AFC_CNTL) | 0x3);
    // Configure HDMI TX serializer:
    //hdmi_wr_reg(0x011, 0x0f);   //Channels Power Up Setting ,"1" for Power-up ,"0" for Power-down,Bit[3:0]=CK,Data2,data1,data1,data0 Channels ;
  //hdmi_wr_reg(0x015, 0x03);   //slew rate
    hdmi_wr_reg(0x017, 0x1d);   //1d for power-up Band-gap and main-bias ,00 is power down 
    if(serial_reg_val<0x20){
        hdmi_wr_reg(0x018, 0x24);
    }
    else{
        hdmi_wr_reg(0x018, serial_reg_val);   //Serializer Internal clock setting ,please fix to vaue 24 ,other setting is only for debug  
    }
    hdmi_wr_reg(0x01a, 0xfb);   //bit[2:0]=011 ,CK channel output TMDS CLOCK ,bit[2:0]=101 ,ck channel output PHYCLCK 

    hdmi_hw_set_powermode(power_mode, 0);

    hdmi_wr_reg(0x0F7, 0x0F);   // Termination resistor calib value
  //hdmi_wr_reg(0x014, 0x07);   // This register is for pre-emphasis control ,we need test different TMDS Clcok speed then write down the suggested     value for each one ;
  //hdmi_wr_reg(0x014, 0x01);   // This is a sample for Pre-emphasis setting ,recommended for 225MHz's TMDS Setting & ** meters HDMI Cable  

    // --------------------------------------------------------
    // Program core_pin_mux to enable HDMI pins
    // --------------------------------------------------------
    //wire            pm_hdmi_cec_en              = pin_mux_reg0[2];
    //wire            pm_hdmi_hpd_5v_en           = pin_mux_reg0[1];
    //wire            pm_hdmi_i2c_5v_en           = pin_mux_reg0[0];
#ifdef AML_A3
    // --------------------------------------------------------
    // Program core_pin_mux to enable HDMI pins
    // --------------------------------------------------------
    //wire            pm_gpioA_3_hdmi_cec         = pin_mux_reg0[3];
    //wire            pm_gpioA_2_hdmi_scl         = pin_mux_reg0[2];
    //wire            pm_gpioA_1_hdmi_sda         = pin_mux_reg0[1];
    //wire            pm_gpioA_0_hdmi_hpd         = pin_mux_reg0[0];
#ifdef CEC_SUPPORT
    Wr(PERIPHS_PIN_MUX_0, Rd(PERIPHS_PIN_MUX_0)|((1 << 3) | // pm_gpioA_3_hdmi_cec
                               (1 << 2) | // pm_gpioA_2_hdmi_scl
                               (1 << 1) | // pm_gpioA_1_hdmi_sda
                               (0 << 0 ))); // pm_gpioA_0_hdmi_hpd , enable this signal after all init done to ensure fist HPD rising ok
#else
    Wr(PERIPHS_PIN_MUX_0, Rd(PERIPHS_PIN_MUX_0)|((0 << 3) | // pm_gpioA_3_hdmi_cec
                               (1 << 2) | // pm_gpioA_2_hdmi_scl
                               (1 << 1) | // pm_gpioA_1_hdmi_sda
                               (0 << 0 ))); // pm_gpioA_0_hdmi_hpd , enable this signal after all init done to ensure fist HPD rising ok
#endif

#else
#ifdef CEC_SUPPORT
    Wr(PERIPHS_PIN_MUX_0, Rd(PERIPHS_PIN_MUX_0)|((1 << 2) | // pm_hdmi_cec_en
                               (0 << 1) | // pm_hdmi_hpd_5v_en , enable this signal after all init done to ensure fist HPD rising ok
                               (1 << 0))); // pm_hdmi_i2c_5v_en
#else
    Wr(PERIPHS_PIN_MUX_0, Rd(PERIPHS_PIN_MUX_0)|((0 << 2) | // pm_hdmi_cec_en
                               (0 << 1) | // pm_hdmi_hpd_5v_en , enable this signal after all init done to ensure fist HPD rising ok
                               (1 << 0))); // pm_hdmi_i2c_5v_en
#endif                               
#endif

    // Enable these interrupts: [2] tx_edit_int_rise [1] tx_hpd_int_fall [0] tx_hpd_int_rise
    hdmi_wr_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_MASKN, 0x7);
    // HPD glitch filter
    hdmi_wr_reg(TX_HDCP_HPD_FILTER_L, 0x00);
    hdmi_wr_reg(TX_HDCP_HPD_FILTER_H, 0xa0);

#ifdef AML_A3
    hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x90); //bit5,6 is converted
    delay_us(10);
    hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x60);
    delay_us(10);
    hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 0xff);
    delay_us(10);
#else
    //new reset sequence, 2010Sep09, rain
    hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0xf0);
    delay_us(10);
    hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x00);
    delay_us(10);
    hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 0xff);
    delay_us(10);
#endif    
    /**/

    // Enable software controlled DDC transaction
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7]   = 1'b0 ;  // forced_sys_trigger
    //tmp_add_data[6]   = 1'b0 ;  // sys_trigger_config
    //tmp_add_data[5]   = 1'b0 ;  // mem_acc_seq_mode
    //tmp_add_data[4]   = 1'b0 ;  // mem_acc_seq_start
    //tmp_add_data[3]   = 1'b1 ;  // forced_mem_copy_done
    //tmp_add_data[2]   = 1'b1 ;  // mem_copy_done_config
    //tmp_add_data[1]   = 1'b1 ;  // sys_trigger_config_semi_manu
    //tmp_add_data[0]   = 1'b0 ;  // Rsrv
    hdmi_wr_reg(TX_HDCP_EDID_CONFIG, 0x0c); //// for hdcp, can not use 0x0e
    
    hdmi_wr_reg(TX_HDCP_CONFIG0,      1<<3);  //set TX rom_encrypt_off=1
    hdmi_wr_reg(TX_HDCP_MEM_CONFIG,   0<<3);  //set TX read_decrypt=0
    hdmi_wr_reg(TX_HDCP_ENCRYPT_BYTE, 0);     //set TX encrypt_byte=0x00

#ifdef CONFIG_AML_HDMI_TX_HDCP
    hdcpkey_status = task_tx_key_setting(force_wrong);
#endif
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7] = 1'b0;       // Force packet timing
    //tmp_add_data[6] = 1'b0;       // PACKET ALLOC MODE
    //tmp_add_data[5:0] = 6'd47 ;   // PACKET_START_LATENCY
    //tmp_add_data = 47;
    tmp_add_data = 58;
    hdmi_wr_reg(TX_PACKET_CONTROL_1, tmp_add_data); //this register should be set to ensure the first hdcp succeed

    //tmp_add_data[7] = 1'b0;      // cp_desired
    //tmp_add_data[6] = 1'b0;      // ess_config
    //tmp_add_data[5] = 1'b0;      // set_avmute
    //tmp_add_data[4] = 1'b1;      // clear_avmute
    //tmp_add_data[3] = 1'b0;      // hdcp_1_1
    //tmp_add_data[2] = 1'b0;      // Vsync/Hsync forced_polarity_select
    //tmp_add_data[1] = 1'b0;      // forced_vsync_polarity
    //tmp_add_data[0] = 1'b0;      // forced_hsync_polarity
    //tmp_add_data = 0x10;
    tmp_add_data = 0x0; //rain
    hdmi_wr_reg(TX_HDCP_MODE, tmp_add_data);
    //config_hdmi(1);

    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7:0]   = 0xa ; // time_divider[7:0] for DDC I2C bus clock
    //tmp_add_data = 0xa; //800k
    //tmp_add_data = 0x3f; //190k
    tmp_add_data = 0x78; //100k
    hdmi_wr_reg(TX_HDCP_CONFIG3, tmp_add_data);

    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7]   = 8'b1 ;  //cp_desired 
    //tmp_add_data[6]   = 8'b1 ;  //ess_config 
    //tmp_add_data[5]   = 8'b0 ;  //set_avmute 
    //tmp_add_data[4]   = 8'b0 ;  //clear_avmute 
    //tmp_add_data[3]   = 8'b1 ;  //hdcp_1_1 
    //tmp_add_data[2]   = 8'b0 ;  //forced_polarity 
    //tmp_add_data[1]   = 8'b0 ;  //forced_vsync_polarity 
    //tmp_add_data[0]   = 8'b0 ;  //forced_hsync_polarity
    tmp_add_data = 0x40;
    hdmi_wr_reg(TX_HDCP_MODE, tmp_add_data);

    hdmi_hw_set_powermode(power_mode, 0);

    // --------------------------------------------------------
    // Release TX out of reset
    // --------------------------------------------------------
    //new reset sequence, 2010Sep09, rain
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 1<<6); // Release resets all other TX digital clock domain, except tmds_clk
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 0x00); // Final release reset on tmds_clk domain
        delay_us(10);        
#ifdef AML_A3
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x68);        
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x60);        
        delay_us(10);
#else
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x08);        
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x00);        
        delay_us(10);
#endif
    /*cec config*/
    cec_init(hdmitx_device);
    cec_set_pending(TV_CEC_PENDING_OFF);
    
    /**/
}    

static void hdmi_hw_reset(Hdmi_tx_video_para_t *param)
{
    unsigned int tmp_add_data;
    unsigned long TX_OUTPUT_COLOR_FORMAT;

    digital_clk_on(7);

    if(param->color==COLOR_SPACE_YUV444){
        TX_OUTPUT_COLOR_FORMAT=1;
    }
    else if(param->color==COLOR_SPACE_YUV422){
        TX_OUTPUT_COLOR_FORMAT=3;
    }
    else{
        TX_OUTPUT_COLOR_FORMAT=0;
    }
#ifndef AML_A3
    // Configure HDMI PLL
    if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
        Wr(HHI_HDMI_PLL_CNTL2, 0x50e8);
    }
    else{
        Wr(HHI_HDMI_PLL_CNTL2, 0x40e8);
    }

    if(new_reset_sequence_flag){
        Wr(HHI_HDMI_PLL_CNTL1, 0x00040003); //should turn on always for new reset sequence
    }
    else{
        Wr(HHI_HDMI_PLL_CNTL1, 0x00040003); 
    }
#endif
    if(delay_flag&2)
        delay_us(1000*100);
    //printk("delay 100ms\n");

    Wr(HHI_HDMI_AFC_CNTL, Rd(HHI_HDMI_AFC_CNTL) | 0x3);
    // Configure HDMI TX serializer:
    //hdmi_wr_reg(0x011, 0x0f);   //Channels Power Up Setting ,"1" for Power-up ,"0" for Power-down,Bit[3:0]=CK,Data2,data1,data1,data0 Channels ;
  //hdmi_wr_reg(0x015, 0x03);   //slew rate
    hdmi_wr_reg(0x017, 0x1d);   //1d for power-up Band-gap and main-bias ,00 is power down 
    if(new_reset_sequence_flag==0){
        if(serial_reg_val==0){
            if((param->VIC==HDMI_1080p30)||(param->VIC==HDMI_720p60)||(param->VIC==HDMI_1080i60)
                ||(param->VIC==HDMI_1080p24)){
                hdmi_wr_reg(0x018, 0x22);   
            }
            else{
                hdmi_wr_reg(0x018, 0x24);   
            }
        }
        else if(serial_reg_val==1){
            if((param->VIC==HDMI_480p60)||(param->VIC==HDMI_480p60_16x9)
                ||(param->VIC==HDMI_576p50)||(param->VIC==HDMI_576p50_16x9)
                ||(param->VIC==HDMI_480i60)||(param->VIC==HDMI_480i60_16x9)
                ||(param->VIC==HDMI_576i50)||(param->VIC==HDMI_576i50_16x9)){
                hdmi_wr_reg(0x018, 0x24);   
            }
            else{
                hdmi_wr_reg(0x018, 0x22);   
            }
        }
        else{
            hdmi_wr_reg(0x018, serial_reg_val);
        }
        if((param->VIC==HDMI_1080p60)&&(param->color_depth==COLOR_30BIT)&&(hdmi_rd_reg(0x018)==0x22)){
            hdmi_wr_reg(0x018,0x12);
        }
    }
    hdmi_wr_reg(0x01a, 0xfb);   //bit[2:0]=011 ,CK channel output TMDS CLOCK ,bit[2:0]=101 ,ck channel output PHYCLCK 

    hdmi_hw_set_powermode(power_mode, param->VIC);

    hdmi_wr_reg(0x0F7, 0x0F);   // Termination resistor calib value
  //hdmi_wr_reg(0x014, 0x07);   // This register is for pre-emphasis control ,we need test different TMDS Clcok speed then write down the suggested     value for each one ;
  //hdmi_wr_reg(0x014, 0x01);   // This is a sample for Pre-emphasis setting ,recommended for 225MHz's TMDS Setting & ** meters HDMI Cable  
    
    // delay 1000uS, then check HPLL_LOCK
    delay_us(1000);
    //while ( (Rd(HHI_HDMI_PLL_CNTL2) & (1<<31)) != (1<<31) );
 
//////////////////////////////reset    
    if(new_reset_sequence_flag){
#ifdef AML_A3
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x90);
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x60);
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 0xff);
        delay_us(10);
#else
        //new reset sequence, 2010Sep09, rain
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0xf0);
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x00);
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 0xff);
        delay_us(10);
#endif        
    }
    else{
#ifndef AML_A3
        // Keep TX (except register I/F) in reset, while programming the registers:
        tmp_add_data  = 0;
        tmp_add_data |= 1 << 7; // tx_pixel_rstn
        tmp_add_data |= 1 << 6; // tx_tmds_rstn
        tmp_add_data |= 1 << 5; // tx_audio_master_rstn
        tmp_add_data |= 1 << 4; // tx_audio_sample_rstn
        tmp_add_data |= 1 << 3; // tx_i2s_reset_rstn
        tmp_add_data |= 1 << 2; // tx_dig_reset_n_ch2
        tmp_add_data |= 1 << 1; // tx_dig_reset_n_ch1
        tmp_add_data |= 1 << 0; // tx_dig_reset_n_ch0
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, tmp_add_data);
    
        tmp_add_data  = 0;
        tmp_add_data |= 1 << 7; // HDMI_CH3_RST_IN
        tmp_add_data |= 1 << 6; // HDMI_CH2_RST_IN
        tmp_add_data |= 1 << 5; // HDMI_CH1_RST_IN
        tmp_add_data |= 1 << 4; // HDMI_CH0_RST_IN
        tmp_add_data |= 1 << 3; // HDMI_SR_RST
        tmp_add_data |= 1 << 0; // tx_dig_reset_n_ch3
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, tmp_add_data);
#endif        
    }
    // Enable software controlled DDC transaction
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7]   = 1'b0 ;  // forced_sys_trigger
    //tmp_add_data[6]   = 1'b0 ;  // sys_trigger_config
    //tmp_add_data[5]   = 1'b0 ;  // mem_acc_seq_mode
    //tmp_add_data[4]   = 1'b0 ;  // mem_acc_seq_start
    //tmp_add_data[3]   = 1'b1 ;  // forced_mem_copy_done
    //tmp_add_data[2]   = 1'b1 ;  // mem_copy_done_config
    //tmp_add_data[1]   = 1'b1 ;  // sys_trigger_config_semi_manu
    //tmp_add_data[0]   = 1'b0 ;  // Rsrv

    tmp_add_data = 0x0c; // for hdcp, can not use 0x0e 
    hdmi_wr_reg(TX_HDCP_EDID_CONFIG, tmp_add_data);
    
    hdmi_wr_reg(TX_HDCP_CONFIG0,      1<<3);  //set TX rom_encrypt_off=1
    hdmi_wr_reg(TX_HDCP_MEM_CONFIG,   0<<3);  //set TX read_decrypt=0
    hdmi_wr_reg(TX_HDCP_ENCRYPT_BYTE, 0);     //set TX encrypt_byte=0x00
    
#ifdef CONFIG_AML_HDMI_TX_HDCP
    hdcpkey_status = task_tx_key_setting(force_wrong);
#endif
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7] = 1'b0;      // Force DTV timing (Auto)
    //tmp_add_data[6] = 1'b0;      // Force Video Scan, only if [7]is set
    //tmp_add_data[5] = 1'b0 ;     // Force Video field, only if [7]is set
    //tmp_add_data[4:0] = 5'b00 ;  // Rsrv
    tmp_add_data = 0;
    hdmi_wr_reg(TX_VIDEO_DTV_TIMING, tmp_add_data);
    
    tmp_add_data  = 0;
    tmp_add_data |= 0                       << 7; // [7]   forced_default_phase
    tmp_add_data |= 0                       << 2; // [6:2] Rsrv
    tmp_add_data |= param->color_depth      << 0; // [1:0] Color_depth:0=24-bit pixel; 1=30-bit pixel; 2=36-bit pixel; 3=48-bit pixel
    hdmi_wr_reg(TX_VIDEO_DTV_MODE, tmp_add_data); // 0x00
    
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7] = 1'b0;       // Force packet timing
    //tmp_add_data[6] = 1'b0;       // PACKET ALLOC MODE
    //tmp_add_data[5:0] = 6'd47 ;   // PACKET_START_LATENCY
    //tmp_add_data = 47;
    tmp_add_data = 58;
    hdmi_wr_reg(TX_PACKET_CONTROL_1, tmp_add_data);

    // For debug: disable packets of audio_request, acr_request, deep_color_request, and avmute_request
    //hdmi_wr_reg(TX_PACKET_CONTROL_2, hdmi_rd_reg(TX_PACKET_CONTROL_2) | 0x0f);
    
    //HDMI CT 7-19 GCP PB1 through PB6 not equal to 0 | 720 3 0 37 72 16367911819.90 31822 General Control Packet (GCP) 
    //PACKET_CONTROL[~deep_color_request_enable]
    //0: horizontal GC packet transport enabled
    //1: horizontal GC packet masked
    hdmi_wr_reg(TX_PACKET_CONTROL_2, hdmi_rd_reg(TX_PACKET_CONTROL_2) | (0x1<<1));
    
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7:6] = 2'b0;     // audio_source_select[1:0]
    //tmp_add_data[5] = 1'b0;       // external_packet_enable
    //tmp_add_data[4] = 1'b1 ;      // internal_packet_enable
    //tmp_add_data[3:2] = 2'b0;     // afe_fifo_source_select_lane_1[1:0]
    //tmp_add_data[1:0] = 2'b0 ;    // afe_fifo_source_select_lane_0[1:0]
    tmp_add_data = 0x10;
    hdmi_wr_reg(TX_CORE_DATA_CAPTURE_2, tmp_add_data);
    
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7]   = 1'b0;     // monitor_lane_1
    //tmp_add_data[6:4] = 3'd0;     // monitor_select_lane_1[2:0]
    //tmp_add_data[3]   = 1'b1 ;    // monitor_lane_0
    //tmp_add_data[2:0] = 3'd7;     // monitor_select_lane_0[2:0]
    tmp_add_data = 0xf;
    hdmi_wr_reg(TX_CORE_DATA_MONITOR_1, tmp_add_data);
    
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7:3] = 5'b0;     // Rsrv
    //tmp_add_data[2:0] = 3'd2;     // monitor_select[2:0]
    tmp_add_data = 0x2;
    hdmi_wr_reg(TX_CORE_DATA_MONITOR_2, tmp_add_data);
    
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7] = 1'b1;     // forced_hdmi
    //tmp_add_data[6] = 1'b1;     // hdmi_config
    //tmp_add_data[5:4] = 2'b0;   // Rsrv
    //tmp_add_data[3] = 1'b0;     // bit_swap.
    //tmp_add_data[2:0] = 3'd0;   // channel_swap[2:0]
    tmp_add_data = 0xc0;
    hdmi_wr_reg(TX_TMDS_MODE, tmp_add_data);
    
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7] = 1'b0;  // Rsrv
    //tmp_add_data[6] = 1'b0;  // TX_CONNECT_SEL: 0=use lower channel data[29:0]; 1=use upper channel data[59:30]
    //tmp_add_data[5:0] = 'h0;  // Rsrv
    tmp_add_data = 0x0;
    hdmi_wr_reg(TX_SYS4_CONNECT_SEL_1, tmp_add_data);
    
    // Normally it makes sense to synch 3 channel output with clock channel's rising edge,
    // as HDMI's serializer is LSB out first, invert tmds_clk pattern from "1111100000" to
    // "0000011111" actually enable data synch with clock rising edge.
    //if((param->VIC==HDMI_1080p30)||(param->VIC==HDMI_720p60)||(param->VIC==HDMI_1080i60)){
    //    hdmi_wr_reg(TX_SYS4_CK_INV_VIDEO, 0xf0);
    //}
    //else{
        tmp_add_data = 1 << 4; // Set tmds_clk pattern to be "0000011111" before being sent to AFE clock channel
        hdmi_wr_reg(TX_SYS4_CK_INV_VIDEO, tmp_add_data);
    //}            
    
    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7] = 1'b0;  // Rsrv
    //tmp_add_data[6] = 1'b0;  // TX_AFE_FIFO channel 2 bypass=0
    //tmp_add_data[5] = 1'b0;  // TX_AFE_FIFO channel 1 bypass=0
    //tmp_add_data[4] = 1'b0;  // TX_AFE_FIFO channel 0 bypass=0
    //tmp_add_data[3] = 1'b1;  // output enable of clk channel (channel 3)
    //tmp_add_data[2] = 1'b1;  // TX_AFE_FIFO channel 2 enable
    //tmp_add_data[1] = 1'b1;  // TX_AFE_FIFO channel 1 enable
    //tmp_add_data[0] = 1'b1;  // TX_AFE_FIFO channel 0 enable
    tmp_add_data = 0x0f;
    hdmi_wr_reg(TX_SYS5_FIFO_CONFIG, tmp_add_data);
    
    tmp_add_data  = 0;
    tmp_add_data |= TX_OUTPUT_COLOR_FORMAT  << 6; // [7:6] output_color_format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422.
    tmp_add_data |= TX_INPUT_COLOR_FORMAT   << 4; // [5:4] input_color_format:  0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422.
    tmp_add_data |= param->color_depth   << 2; // [3:2] output_color_depth:  0=24-b; 1=30-b; 2=36-b; 3=48-b.
    tmp_add_data |= TX_INPUT_COLOR_DEPTH    << 0; // [1:0] input_color_depth:   0=24-b; 1=30-b; 2=36-b; 3=48-b.
    hdmi_wr_reg(TX_VIDEO_DTV_OPTION_L, tmp_add_data); // 0x50
    
    tmp_add_data  = 0;
    tmp_add_data |= 0                       << 4; // [7:4] Rsrv
    tmp_add_data |= TX_OUTPUT_COLOR_RANGE   << 2; // [3:2] output_color_range:  0=16-235/240; 1=16-240; 2=1-254; 3=0-255.
    tmp_add_data |= TX_INPUT_COLOR_RANGE    << 0; // [1:0] input_color_range:   0=16-235/240; 1=16-240; 2=1-254; 3=0-255.
    hdmi_wr_reg(TX_VIDEO_DTV_OPTION_H, tmp_add_data); // 0x00
    if(!hdmi_audio_off_flag){
#if 1
        hdmi_audio_init(i2s_to_spdif_flag);
#else
        hdmi_wr_reg(TX_AUDIO_PACK, 0x00); // disable audio sample packets
#endif
    }
    //tmp_add_data[7] = 1'b0;      // cp_desired
    //tmp_add_data[6] = 1'b0;      // ess_config
    //tmp_add_data[5] = 1'b0;      // set_avmute
    //tmp_add_data[4] = 1'b1;      // clear_avmute
    //tmp_add_data[3] = 1'b0;      // hdcp_1_1
    //tmp_add_data[2] = 1'b0;      // Vsync/Hsync forced_polarity_select
    //tmp_add_data[1] = 1'b0;      // forced_vsync_polarity
    //tmp_add_data[0] = 1'b0;      // forced_hsync_polarity
    //tmp_add_data = 0x10;
    tmp_add_data = 0x0; //rain
    hdmi_wr_reg(TX_HDCP_MODE, tmp_add_data);
    //config_hdmi(1);

    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7:0]   = 0xa ; // time_divider[7:0] for DDC I2C bus clock
    
    //tmp_add_data = 0xa; //800k
    //tmp_add_data = 0x3f; //190k
    tmp_add_data = 0x78; //100k
    hdmi_wr_reg(TX_HDCP_CONFIG3, tmp_add_data);

    //tmp_add_data[15:8] = 0;
    //tmp_add_data[7]   = 8'b1 ;  //cp_desired 
    //tmp_add_data[6]   = 8'b1 ;  //ess_config 
    //tmp_add_data[5]   = 8'b0 ;  //set_avmute 
    //tmp_add_data[4]   = 8'b0 ;  //clear_avmute 
    //tmp_add_data[3]   = 8'b1 ;  //hdcp_1_1 
    //tmp_add_data[2]   = 8'b0 ;  //forced_polarity 
    //tmp_add_data[1]   = 8'b0 ;  //forced_vsync_polarity 
    //tmp_add_data[0]   = 8'b0 ;  //forced_hsync_polarity
    tmp_add_data = 0x40;
    hdmi_wr_reg(TX_HDCP_MODE, tmp_add_data);
    if(param->cc == CC_ITU709){
        hdmi_wr_reg(TX_VIDEO_CSC_COEFF_CB0, 0xf2);        
        hdmi_wr_reg(TX_VIDEO_CSC_COEFF_CB1, 0x2f);        
        hdmi_wr_reg(TX_VIDEO_CSC_COEFF_CR0, 0xd4);        
        hdmi_wr_reg(TX_VIDEO_CSC_COEFF_CR1, 0x77);        
    }
    else{
        hdmi_wr_reg(TX_VIDEO_CSC_COEFF_CB0, 0x18);        
        hdmi_wr_reg(TX_VIDEO_CSC_COEFF_CB1, 0x58);        
        hdmi_wr_reg(TX_VIDEO_CSC_COEFF_CR0, 0xd0);        
        hdmi_wr_reg(TX_VIDEO_CSC_COEFF_CR1, 0x66);        
    }    

    hdmi_hw_set_powermode(power_mode, param->VIC);
    
    // --------------------------------------------------------
    // Release TX out of reset
    // --------------------------------------------------------
    if(new_reset_sequence_flag){
        //new reset sequence, 2010Sep09, rain
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 1<<6); // Release resets all other TX digital clock domain, except tmds_clk
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 0x00); // Final release reset on tmds_clk domain
        delay_us(10);        
#ifdef AML_A3
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x68);        
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x60);        
        delay_us(10);
#else
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x08);        
        delay_us(10);
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x00);        
        delay_us(10);
#endif
        /* select serial*/
        if(serial_reg_val==0){
            if((param->VIC==HDMI_1080p30)||(param->VIC==HDMI_720p60)||(param->VIC==HDMI_1080i60)
                ||(param->VIC==HDMI_1080p24)){
                hdmi_wr_reg(0x018, 0x22);   
            }
            else{
                hdmi_wr_reg(0x018, 0x24);   
            }
        }
        else if(serial_reg_val==1){
            if((param->VIC==HDMI_480p60)||(param->VIC==HDMI_480p60_16x9)
                ||(param->VIC==HDMI_576p50)||(param->VIC==HDMI_576p50_16x9)
                ||(param->VIC==HDMI_480i60)||(param->VIC==HDMI_480i60_16x9)
                ||(param->VIC==HDMI_576i50)||(param->VIC==HDMI_576i50_16x9)){
                hdmi_wr_reg(0x018, 0x24);   
            }
            else{
                hdmi_wr_reg(0x018, 0x22);   
            }
        }
        else{
            hdmi_wr_reg(0x018, serial_reg_val);
        }
        if((param->VIC==HDMI_1080p60)&&(param->color_depth==COLOR_30BIT)&&(hdmi_rd_reg(0x018)==0x22)){
            hdmi_wr_reg(0x018,0x12);
        }
        
    }
    else{
#ifndef AML_A3
        Wr(HHI_HDMI_PLL_CNTL1, 0x00040000); // turn off phy_clk
        delay_us(10);
    
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x01); // Release serializer resets
        delay_us(10);
    
        Wr(HHI_HDMI_PLL_CNTL1, 0x00040003); // turn on phy_clk
        delay_us(10);
    
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x00); // Release reset on TX digital clock channel
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 1<<6); // Release resets all other TX digital clock domain, except tmds_clk
        delay_us(10);
    
        hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_1, 0x00); // Final release reset on tmds_clk domain
        
        tmp_add_data = hdmi_rd_reg(0x018);
        if((tmp_add_data==0x22)||(tmp_add_data==0x12)){
            hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x08);        
            delay_us(10);
            hdmi_wr_reg(TX_SYS5_TX_SOFT_RESET_2, 0x00);        
        }
#endif        
    }
    if(param->VIC == HDMI_1080p60)
        hdmi_wr_reg(0x018, 0x24);           // fix 1080p flicks
}

static void hdmi_audio_init(unsigned char spdif_flag)
{
    unsigned tmp_add_data;
#if 1
    /* If TX_AUDIO_FORMAT is set as 0, "Channel Status" will not be sent out correctly */
    /* TX_AUDIO_CONTROL[bit 0] should be 1, otherwise no sound??? */
    unsigned char tx_i2s_spdif;
    unsigned char tx_i2s_8_channel;
    
    hdmi_print(0,"HDMI DEBUG [%s] [%d]\n", __FUNCTION__, spdif_flag);
    
    if(spdif_flag){
        tx_i2s_spdif=0;
        tx_i2s_8_channel=0;
    }
    else{
        tx_i2s_spdif=1;
        tx_i2s_8_channel=0;
    }

    tmp_add_data  = 0;
    tmp_add_data |= tx_i2s_spdif    << 7; // [7]    I2S or SPDIF
    tmp_add_data |= tx_i2s_8_channel<< 6; // [6]    8 or 2ch
    tmp_add_data |= 2               << 4; // [5:4]  Serial Format: I2S format
    tmp_add_data |= 3               << 2; // [3:2]  Bit Width: 24-bit
    tmp_add_data |= 0               << 1; // [1]    WS Polarity: 0=WS high is right
    tmp_add_data |= 1               << 0; // [0]    For I2S: 0=one-bit audio; 1=I2S;
                                          //        For SPDIF: 0= channel status from input data; 1=from register
    hdmi_wr_reg(TX_AUDIO_FORMAT, tmp_add_data); // 0x2f

    //tmp_add_data  = 0;
    //tmp_add_data |= 0x4 << 4; // [7:4]  FIFO Depth=512
    //tmp_add_data |= 0x2 << 2; // [3:2]  Critical threshold=Depth/16
    //tmp_add_data |= 0x1 << 0; // [1:0]  Normal threshold=Depth/8
    //hdmi_wr_reg(TX_AUDIO_FIFO, tmp_add_data); // 0x49
    hdmi_wr_reg(TX_AUDIO_FIFO, aud_para); // 0x49

    hdmi_wr_reg(TX_AUDIO_LIPSYNC, 0); // [7:0] Normalized lip-sync param: 0 means S(lipsync) = S(total)/2

    tmp_add_data  = 0;
    tmp_add_data |= 0   << 7; // [7]    forced_audio_fifo_clear
    tmp_add_data |= 1   << 6; // [6]    auto_audio_fifo_clear
    tmp_add_data |= 0x0 << 4; // [5:4]  audio_packet_type: 0=audio sample packet; 1=one bit audio; 2=HBR audio packet; 3=DST audio packet.
    tmp_add_data |= 0   << 3; // [3]    Rsrv
    tmp_add_data |= 0   << 2; // [2]    Audio sample packet's valid bit: 0=valid bit is 0 for I2S, is input data for SPDIF; 1=valid bit from register
    tmp_add_data |= 0   << 1; // [1]    Audio sample packet's user bit: 0=user bit is 0 for I2S, is input data for SPDIF; 1=user bit from register
    tmp_add_data |= 0   << 0; // [0]    0=Audio sample packet's sample_flat bit is 1; 1=sample_flat is 0.
    hdmi_wr_reg(TX_AUDIO_CONTROL, tmp_add_data); // 0x40

    tmp_add_data  = 0;
    tmp_add_data |= tx_i2s_8_channel<< 7; // [7]    Audio sample packet's header layout bit: 0=layout0; 1=layout1
    tmp_add_data |= 0               << 6; // [6]    Set normal_double bit in DST packet header.
    tmp_add_data |= 0               << 0; // [5:0]  Rsrv
    hdmi_wr_reg(TX_AUDIO_HEADER, tmp_add_data); // 0x00

    tmp_add_data  = tx_i2s_8_channel ? 0xff : 0x03;
    hdmi_wr_reg(TX_AUDIO_SAMPLE, tmp_add_data); // Channel valid for up to 8 channels, 1 bit per channel.

    hdmi_wr_reg(TX_AUDIO_PACK, 0x01); // Enable audio sample packets

    // Set N = 4096 (N is not measured, N must be configured so as to be a reference to clock_meter)
    hdmi_wr_reg(TX_SYS1_ACR_N_0, 0x00); // N[7:0]
    hdmi_wr_reg(TX_SYS1_ACR_N_1, 0x18 /*0x10*/); // N[15:8]

    tmp_add_data  = 0;
    tmp_add_data |= 0xa << 4;    // [7:4] Meas Tolerance
    tmp_add_data |= 0x0 << 0;    // [3:0] N[19:16]
    hdmi_wr_reg(TX_SYS1_ACR_N_2, tmp_add_data); // 0xa0

    hdmi_wr_reg(TX_AUDIO_CONTROL,   hdmi_rd_reg(TX_AUDIO_CONTROL)|0x1); 
#else
/* reference register setting */
/* this register setting works for spdif_flag==1*/
    if(spdif_flag){
        hdmi_wr_reg(TX_AUDIO_CONTROL,   0x40);  // Address  0x5D=0x40   TX_AUDIO_CONTROL
        hdmi_wr_reg(TX_AUDIO_FIFO,   0x1 );  // Address  0x5B=0x1    TX_AUDIO_FIFO
        hdmi_wr_reg(TX_AUDIO_CONTROL,   0x40);  // Address  0x5D=0x40   TX_AUDIO_CONTROL
        hdmi_wr_reg(TX_AUDIO_FIFO,   0xD );  // Address  0x5B=0xD    TX_AUDIO_FIFO
        hdmi_wr_reg(TX_AUDIO_FIFO,   0x3D);  // Address  0x5B=0x3D   TX_AUDIO_FIFO
        hdmi_wr_reg(TX_AUDIO_LIPSYNC,   0x1 );  // Address  0x5C=0x1    TX_AUDIO_LIPSYNC
        hdmi_wr_reg(TX_AUDIO_PACK,   0x1 );  // Address  0x62=0x1    TX_AUDIO_PACK
        hdmi_wr_reg(TX_AUDIO_CONTROL,   0x40);  // Address  0x5D=0x40   TX_AUDIO_CONTROL
        hdmi_wr_reg(TX_AUDIO_HEADER,   0x0 );  // Address  0x5E=0x0    TX_AUDIO_HEADER
        hdmi_wr_reg(TX_HDCP_MODE,   0x0 );  // Address  0x2F=0x0    TX_HDCP_MODE
        hdmi_wr_reg(TX_HDCP_MODE,   0x0 );  // Address  0x2F=0x0    TX_HDCP_MODE
        hdmi_wr_reg(TX_SYS0_ACR_CTS_2,    0x20);  // Address  0x4=0x20    TX_SYS0_ACR_CTS_2
        hdmi_wr_reg(TX_SYS0_ACR_CTS_2,    0x20);  // Address  0x4=0x20    TX_SYS0_ACR_CTS_2
        hdmi_wr_reg(TX_SYS0_ACR_CTS_2,    0x20);  // Address  0x4=0x20    TX_SYS0_ACR_CTS_2
        hdmi_wr_reg(TX_SYS0_ACR_CTS_2,    0x20);  // Address  0x4=0x20    TX_SYS0_ACR_CTS_2
        hdmi_wr_reg(TX_AUDIO_CONTROL,   0x40);  // Address  0x5D=0x40   TX_AUDIO_CONTROL
        hdmi_wr_reg(TX_AUDIO_SAMPLE,   0x3 );  // Address  0x5F=0x3    TX_AUDIO_SAMPLE
        hdmi_wr_reg(TX_AUDIO_CONTROL,   0x41);  // Address  0x5D=0x41   TX_AUDIO_CONTROL
        //hdmi_wr_reg(0x280,  0x70);  // Address  0x280=0x70  TX_PKT_REG_AUDIO_INFO_BASE_ADDR
        //hdmi_wr_reg(0x29E,  0xA );  // Address  0x29E=0xA
        //hdmi_wr_reg(0x29D,  0x1 );  // Address  0x29D=0x1
        //hdmi_wr_reg(0x29C,  0x84);  // Address  0x29C=0x84
        //hdmi_wr_reg(0x281,  0x1 );  // Address  0x281=0x1
        //hdmi_wr_reg(0x29F,  0x80);  // Address  0x29F=0x80
        hdmi_wr_reg(TX_AUDIO_FORMAT,   0x0 );  // Address  0x58=0x0    TX_AUDIO_FORMAT
        hdmi_wr_reg(TX_AUDIO_I2S,   0x0 );  // Address  0x5A=0x0    TX_AUDIO_I2S
        hdmi_wr_reg(TX_AUDIO_SPDIF,   0x1 );  // Address  0x59=0x1    TX_AUDIO_SPDIF
        hdmi_wr_reg(TX_SYS1_ACR_N_2,   0x0 );  // Address  0x1E=0x0    TX_SYS1_ACR_N_2
        hdmi_wr_reg(TX_SYS1_ACR_N_1,   0x2D);  // Address  0x1D=0x2D   TX_SYS1_ACR_N_1
        hdmi_wr_reg(TX_SYS1_ACR_N_0,   0x80);  // Address  0x1C=0x80   TX_SYS1_ACR_N_0
    }
    else{
        hdmi_wr_reg(TX_AUDIO_CONTROL,                   0x40); //Address  0x5D=0x40
        hdmi_wr_reg(TX_AUDIO_FIFO,                      0x1 ); //Address  0x5B=0x1
        hdmi_wr_reg(TX_AUDIO_CONTROL,                   0x40); //Address  0x5D=0x40
        hdmi_wr_reg(TX_AUDIO_FIFO,                      0xD ); //Address  0x5B=0xD
        hdmi_wr_reg(TX_AUDIO_FIFO,                      0x3D); //Address  0x5B=0x3D
        hdmi_wr_reg(TX_AUDIO_LIPSYNC,                   0x1 ); //Address  0x5C=0x1
        hdmi_wr_reg(TX_AUDIO_PACK,                      0x1 ); //Address  0x62=0x1
        hdmi_wr_reg(TX_AUDIO_CONTROL,                   0x40); //Address  0x5D=0x40
        hdmi_wr_reg(TX_AUDIO_HEADER,                    0x0 ); //Address  0x5E=0x0
        hdmi_wr_reg(TX_SYS0_ACR_CTS_2,                  0x20); //Address  0x4=0x20
        hdmi_wr_reg(TX_SYS0_ACR_CTS_2,                  0x20); //Address  0x4=0x20
        hdmi_wr_reg(TX_SYS0_ACR_CTS_2,                  0x20); //Address  0x4=0x20
        hdmi_wr_reg(TX_SYS0_ACR_CTS_2,                  0x20); //Address  0x4=0x20
        hdmi_wr_reg(TX_AUDIO_CONTROL,                   0x40); //Address  0x5D=0x40
        hdmi_wr_reg(TX_AUDIO_SAMPLE,                    0x15); //Address  0x5F=0x15
        hdmi_wr_reg(TX_AUDIO_CONTROL,                   0x41); //Address  0x5D=0x41
        hdmi_wr_reg(TX_AUDIO_FORMAT,                    0x0 ); //Address  0x58=0x0
        hdmi_wr_reg(TX_AUDIO_HEADER,                    0x0 ); //Address  0x5E=0x0
        hdmi_wr_reg(TX_AUDIO_SAMPLE,                    0x3 ); //Address  0x5F=0x3
        hdmi_wr_reg(TX_AUDIO_SPDIF,                     0x0 ); //Address  0x59=0x0
        hdmi_wr_reg(TX_AUDIO_FORMAT,                    0x80); //Address  0x58=0x80
        hdmi_wr_reg(TX_AUDIO_I2S,                       0x1 ); //Address  0x5A=0x1
        hdmi_wr_reg(TX_AUDIO_FORMAT,                    0x81); //Address  0x58=0x81
        hdmi_wr_reg(TX_AUDIO_FORMAT,                    0xA1); //Address  0x58=0xA1
        hdmi_wr_reg(TX_SYS1_ACR_N_2,                    0x0 ); //Address  0x1E=0x0
        hdmi_wr_reg(TX_SYS1_ACR_N_1,                    0x2D); //Address  0x1D=0x2D
        hdmi_wr_reg(TX_SYS1_ACR_N_0,                    0x80); //Address  0x1C=0x80
    } 
#endif      
}

#ifdef AVOS
#define AIU_CLK_CTRL MREG_AIU_clk_ctrl 
#define AIU_958_MISC MREG_AIU_958_misc
#define AIU_958_FORCE_LEFT MREG_AIU_958_force_left
#define AIU_958_CTRL    MREG_AIU_958_ctrl
#define AIU_958_BPF MREG_AIU_958_bpf
#define AIU_I2S_MISC MREG_AIU_i2s_misc
#define AIU_CLK_CTRL_MORE MREG_AIU_clk_ctrl_more
#define AIU_958_DCU_FF_CTRL MREG_AIU_958_dcu_ff_ctrl
#endif

static void enable_audio_spdif(void)
{
//        Wr( AIU_958_MISC, 0x204a ); // // Program the IEC958 Module in the AIU
//        Wr( AIU_958_FORCE_LEFT, 0x0000 );
//        Wr( AIU_958_CTRL, 0x0240 );

    /* enable audio*/        
        hdmi_wr_reg(TX_AUDIO_I2S,   0x0 );  // Address  0x5A=0x0    TX_AUDIO_I2S

        hdmi_wr_reg(TX_AUDIO_SPDIF, 1); // TX AUDIO SPDIF Enable

        Wr(AIU_CLK_CTRL,        Rd(AIU_CLK_CTRL) | 2); // enable iec958 clock which is audio_master_clk
//        Wr( AIU_958_BPF, 0x0100 ); // Set the PCM frame size to 256 bytes
//        Wr( AIU_958_DCU_FF_CTRL, 0x0001 );
//        
//        Wr(AIU_I2S_MISC, Rd(AIU_I2S_MISC)|0x18); //i2s_to_958 directly
        hdmi_wr_reg(TX_AUDIO_FORMAT, (hdmi_rd_reg(TX_AUDIO_FORMAT)) & 0x7f); //set TX_AUDIO_FORMAT[7]=0
                                                                             // 0: use SPDIF port
                                                                             // 1: use I2S port
        hdmi_wr_reg(TX_AUDIO_FORMAT, (hdmi_rd_reg(TX_AUDIO_FORMAT)) & 0xfe); //set TX_AUDIO_FORMAT[0]=0
                                                                             // 0: use channel status bit from input data
                                                                             // 1: use configuration register to program channel status

#ifdef AML_A3
    //hdmi 958 clk  = amclk/128/2;
        Wr(AIU_CLK_CTRL, (Rd(AIU_CLK_CTRL)&(~(0x3<<4))&(~(1<<12)))|(0x1<<4));
#endif
}

static void enable_audio_i2s(void)
{
    hdmi_wr_reg(TX_AUDIO_I2S,   0x1 );  // Address  0x5A=0x0    TX_AUDIO_I2S
    hdmi_wr_reg(TX_AUDIO_SPDIF, 0); // TX AUDIO SPDIF Enable
    Wr(AIU_CLK_CTRL,        Rd(AIU_CLK_CTRL) | 3); // enable iec958 clock which is audio_master_clk
#ifdef AML_A3
    //hdmi 958 clk  = amclk/128/2;
        Wr(AIU_CLK_CTRL, (Rd(AIU_CLK_CTRL)&(~(0x3<<4))&(~(1<<12)))|(0x1<<4));
#endif
    hdmi_wr_reg(TX_AUDIO_FORMAT, (hdmi_rd_reg(TX_AUDIO_FORMAT)) | 0x81); //set TX_AUDIO_FORMAT[7]=1
                                                                         // 0: use SPDIF port
                                                                         // 1: use I2S port

}    

/************************************
*    hdmitx hardware level interface
*************************************/
static unsigned char hdmitx_m1b_getediddata(hdmitx_dev_t* hdmitx_device)
{
    if(hdmi_rd_reg(TX_HDCP_ST_EDID_STATUS) & (1<<4)){
        if((hdmitx_device->cur_edid_block+2)<=EDID_MAX_BLOCK){
            int ii, jj;
            for(jj=0;jj<2;jj++){
#ifdef LOG_EDID
                int edid_log_pos=0;
                edid_log_pos+=snprintf((char*)hdmitx_device->tmp_buf+edid_log_pos, HDMI_TMP_BUF_SIZE-edid_log_pos, "EDID Interrupt cur block %d:",hdmitx_device->cur_edid_block);
#endif
                for(ii=0;ii<128;ii++){
                    hdmitx_device->EDID_buf[hdmitx_device->cur_edid_block*128+ii]
                        =hdmi_rd_reg(0x600+hdmitx_device->cur_phy_block_ptr*128+ii);
#ifdef LOG_EDID
                    if((ii&0xf)==0)
                        edid_log_pos+=snprintf((char*)hdmitx_device->tmp_buf+edid_log_pos, HDMI_TMP_BUF_SIZE-edid_log_pos, "\n");
                    edid_log_pos+=snprintf((char*)hdmitx_device->tmp_buf+edid_log_pos, HDMI_TMP_BUF_SIZE-edid_log_pos, "%02x ",hdmitx_device->EDID_buf[hdmitx_device->cur_edid_block*128+ii]);
#endif                    
                }
#ifdef LOG_EDID
                hdmitx_device->tmp_buf[edid_log_pos]=0;
                hdmi_print_buf((char*)hdmitx_device->tmp_buf, strlen((char*)hdmitx_device->tmp_buf));
                hdmi_print(0,"\n");
#endif
                hdmitx_device->cur_edid_block++;
                hdmitx_device->cur_phy_block_ptr++;
                hdmitx_device->cur_phy_block_ptr=hdmitx_device->cur_phy_block_ptr&0x3;
            }
        }        
        return 1;
    }
    else{
        return 0;
    }    
}    

static void check_chip_type(void)
{
#ifndef AML_A3
    if(Rd(HHI_MPEG_CLK_CNTL)&(1<<11)){ //audio pll is selected as video clk
			if(hdmi_chip_type != HDMI_M1A){
			    hdmi_chip_type = HDMI_M1A; 
			    hdmi_print(1,"Set HDMI:Chip A\n");
			}
    }
    else{
			if((hdmi_chip_type != HDMI_M1B)&&(hdmi_chip_type != HDMI_M1C)){
          hdmi_chip_type = HDMI_M1C;     			    
			    hdmi_print(1,"Set HDMI:Chip C\n");
			}
		}
#endif		
}

#if 0
// Only applicable if external HPD is on and stable.
// This function generates an HDMI TX internal sys_trigger pulse that will
// restart EDID and then HDCP transfer on DDC channel.
static void restart_edid_hdcp (void)
{
    // Force clear HDMI TX internal sys_trigger
    hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) & ~(1<<6)); // Release sys_trigger_config
    hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) | (1<<1)); // Assert sys_trigger_config_semi_manu
    // Wait some time for both TX and RX to reset DDC channel
    delay_us(10);
    // Recover HDMI TX internal sys_trigger
    hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) | (1<<6)); // Assert sys_trigger_config
    hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) & ~(1<<1)); // Release sys_trigger_config_semi_manu
}
#endif

static void hdmitx_set_tvenc_reg(int cur_VIC)
{
    int i,j;
    for(i=0;hdmi_tvenc_configs[i].vic!=HDMI_Unkown;i++){
        if(cur_VIC==hdmi_tvenc_configs[i].vic){
            const  reg_t* reg_set=hdmi_tvenc_configs[i].reg_set;
            for(j=0;reg_set[j].reg;j++){
                Wr(reg_set[j].reg,reg_set[j].val);
            }
            break;
        }
    }
}    
 
static void hdmitx_dump_tvenc_reg(int cur_VIC, int printk_flag) 
{
    int i,j;
    for(i=0;hdmi_tvenc_configs[i].vic!=HDMI_Unkown;i++){
        if(cur_VIC==hdmi_tvenc_configs[i].vic){
            reg_t* reg_set=hdmi_tvenc_configs[i].reg_set;
            hdmi_print(printk_flag, "------dump tevenc reg for mode %d----\n", cur_VIC);
            for(j=0;reg_set[j].reg;j++){
                hdmi_print(printk_flag, "[%08x]=%08x\n",reg_set[j].reg,Rd(reg_set[j].reg));
            }
            hdmi_print(printk_flag, "------------------\n");
            break;
        }
    }
}    

static void hdmitx_config_tvenc_reg(int vic, unsigned reg, unsigned val)
{
    int i,j;
    for(i=0;hdmi_tvenc_configs[i].vic!=HDMI_Unkown;i++){
        if(vic==hdmi_tvenc_configs[i].vic){
            reg_t* reg_set=hdmi_tvenc_configs[i].reg_set;
            for(j=0;reg_set[j].reg;j++){
                if(reg_set[j].reg==reg){
                    reg_set[j].val = val;    
                    hdmi_print(1, "set [%08x]=%08x\n",reg_set[j].reg, reg_set[j].val);
                    break;
                }
            }
            if(reg_set[j].reg == 0){
                hdmi_print(1, "no [%08x] in config\n", reg);
            }
            break;
        }
    }
}    

static void hdmitx_set_pll(Hdmi_tx_video_para_t *param)
{
#ifndef AML_A3    
    if((param->VIC==HDMI_480p60)||(param->VIC==HDMI_480p60_16x9)
        ||(param->VIC==HDMI_576p50)||(param->VIC==HDMI_576p50_16x9)){
        if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
            Wr(HHI_HDMI_PLL_CNTL, 0x03040905); // For xtal=24MHz: PREDIV=5, POSTDIV=9, N=4, 0D=3, to get phy_clk=270MHz, tmds_clk=27MHz.
        }
        else{
            if(param->color_depth==COLOR_36BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x03040503); 
            }
            else if(param->color_depth==COLOR_30BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x0310050a); 
            }
            else{
                Wr(HHI_HDMI_PLL_CNTL, 0x03040502); //normal
            }
        }
    }
    else if((param->VIC==HDMI_480i60)||(param->VIC==HDMI_480i60_16x9)
        ||(param->VIC==HDMI_576i50)||(param->VIC==HDMI_576i50_16x9)){
        if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
            Wr(HHI_HDMI_PLL_CNTL, 0x03040905); // For xtal=24MHz: PREDIV=5, POSTDIV=9, N=4, 0D=3, to get phy_clk=270MHz, tmds_clk=27MHz.
        }
        else{
            if(param->color_depth==COLOR_36BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x03040503); 
            }
            else if(param->color_depth==COLOR_30BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x0310050a); 
            }
            else{
                Wr(HHI_HDMI_PLL_CNTL, 0x03040502); 
            }
        }
    }            
    else if(param->VIC==HDMI_1080p30){
        if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
            Wr(HHI_HDMI_PLL_CNTL, 0x0110210f);
            Wr(HHI_VID_CLK_DIV,3);
            //Wr(HHI_AUD_PLL_CNTL, 0x4863);
        }
        else{
            if(param->color_depth==COLOR_36BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x00040503); 
            }
            else if(param->color_depth==COLOR_30BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x0110050a); //30 bit
            }
            else{
                Wr(HHI_HDMI_PLL_CNTL, 0x01040502); //24 bit
            }
            Wr(HHI_VID_PLL_CNTL, 0x00190863); //0x00140863
            //Wr(HHI_VID_PLL_CNTL, 0x00190ead);
        }
    }
    else if(param->VIC==HDMI_1080p24){
        if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
            Wr(HHI_HDMI_PLL_CNTL, 0x0102030f);
            Wr(HHI_VID_CLK_DIV,4);
        }
        else{
            if(param->color_depth==COLOR_36BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x01040503); 
            }
            else if(param->color_depth==COLOR_30BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x0110050a); //30 bit
            }
            else{
                Wr(HHI_HDMI_PLL_CNTL, 0x01040502); //24 bit
            }
            Wr(HHI_VID_PLL_CNTL, 0x00190a63); //0x00140863
        }
    }
    else if((param->VIC==HDMI_1080p60)||(param->VIC==HDMI_1080p50)){
        if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
            Wr(HHI_HDMI_PLL_CNTL, 0x0008210f); // For 24MHz xtal: PREDIV=15, POSTDIV=33, N=8, 0D=0, to get phy_clk=1485MHz, tmds_clk=148.5MHz.
        }
        else{
            if(param->color_depth==COLOR_36BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x0010050c); 
            }
            else if(param->color_depth==COLOR_30BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x0010050a); 
            }
            else{
                Wr(HHI_HDMI_PLL_CNTL, 0x00040502); 
            }
        }
    }
    else if((param->VIC==HDMI_720p60)||(param->VIC==HDMI_720p50)){
        if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
#ifdef DOUBLE_CLK_720P_1080I
            Wr(HHI_HDMI_PLL_CNTL, 0x0008210f); // For 24MHz xtal: PREDIV=15, POSTDIV=33, N=8, 0D=0, to get phy_clk=1485MHz, tmds_clk=148.5MHz.
#else
            Wr(HHI_HDMI_PLL_CNTL, 0x0110210f); // For 24MHz xtal: PREDIV=15, POSTDIV=33, N=8, 0D=0, to get phy_clk=1485MHz, tmds_clk=148.5MHz.
#endif            
        }
        else{
#ifdef DOUBLE_CLK_720P_1080I
            Wr(HHI_HDMI_PLL_CNTL, 0x00040502); 
#else            
            if(param->color_depth==COLOR_36BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x00080503); 
            }
            else if(param->color_depth==COLOR_30BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x01100505); 
            }
            else{
                Wr(HHI_HDMI_PLL_CNTL, 0x01080502);
            }
#endif            
        }
    }   
    else if((param->VIC==HDMI_1080i60)||(param->VIC==HDMI_1080i50)){
        //Wr_reg_bits (HHI_VID_CLK_CNTL, 1, 4, 2);  // Select vclk1=54Mhz as HDMI TX pixel_clk to achieve 4-time pixel repeatition
        Wr_reg_bits (HHI_VID_CLK_CNTL, 0, 4, 2); 
        // set am_analog_top.u_video_pll.OD and XD to make HDMI PLL CKIN the same as vclk2/2
        //Wr_reg_bits (HHI_VID_PLL_CNTL, 1, 16, 1);  // OD: 0=no div, 1=div by 2
        //Wr_reg_bits (HHI_VID_PLL_CNTL, 4, 20, 9);  // XD: div by n
        if((hdmi_chip_type == HDMI_M1A)||(hdmi_pll_mode == 1)){
#ifdef DOUBLE_CLK_720P_1080I
            Wr(HHI_HDMI_PLL_CNTL, 0x0008210f); // For 24MHz xtal: PREDIV=15, POSTDIV=33, N=8, 0D=0, to get phy_clk=1485MHz, tmds_clk=148.5MHz.
#else
            Wr(HHI_HDMI_PLL_CNTL, 0x0110210f); // For 24MHz xtal: PREDIV=15, POSTDIV=33, N=8, 0D=0, to get phy_clk=1485MHz, tmds_clk=148.5MHz.
#endif            
        }
        else{
#ifdef DOUBLE_CLK_720P_1080I
            Wr(HHI_HDMI_PLL_CNTL, 0x00040502); 
#else            
            if(param->color_depth==COLOR_36BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x00080503); 
            }
            else if(param->color_depth==COLOR_30BIT){
                Wr(HHI_HDMI_PLL_CNTL, 0x01100505); 
            }
            else{
                Wr(HHI_HDMI_PLL_CNTL, 0x01080502); 
            }
#endif            
        }
    } 

#endif
}

static int hdmitx_m1b_set_dispmode(Hdmi_tx_video_para_t *param)
{
    if(param == NULL){ //disable HDMI
        return 0;
    }
    else if((param->VIC!=HDMI_480p60)&&(param->VIC!=HDMI_480p60_16x9)
        &&(param->VIC!=HDMI_576p50)&&(param->VIC!=HDMI_576p50_16x9)
        &&(param->VIC!=HDMI_480i60)&&(param->VIC!=HDMI_480i60_16x9)
        &&(param->VIC!=HDMI_576i50)&&(param->VIC!=HDMI_576i50_16x9)
        &&(param->VIC!=HDMI_1080p30)
        &&(param->VIC!=HDMI_1080p24)
        &&(param->VIC!=HDMI_1080p60)&&(param->VIC!=HDMI_1080p50)
        &&(param->VIC!=HDMI_720p60)&&(param->VIC!=HDMI_720p50)
        &&(param->VIC!=HDMI_1080i60)&&(param->VIC!=HDMI_1080i50)){
        return -1;
    }

#ifdef CONFIG_AML_HDMI_TX_HDCP
    hdmi_wr_reg(TX_HDCP_MODE, hdmi_rd_reg(TX_HDCP_MODE)&(~0x80)); //disable authentication
#endif    
    check_chip_type(); /* check chip_type again */
    if((hdmi_chip_type == HDMI_M1B || hdmi_chip_type == HDMI_M1C)&&(color_depth_f != 0)){
        if(color_depth_f==24)
            param->color_depth = COLOR_24BIT;
        else if(color_depth_f==30)
            param->color_depth = COLOR_30BIT;
        else if(color_depth_f==36)
            param->color_depth = COLOR_36BIT;
        else if(color_depth_f==48)
            param->color_depth = COLOR_48BIT;
    }
    hdmi_print(1,"set mode VIC %d (cd%d,cs%d,pm%d,vd%d,%x) \n",param->VIC, color_depth_f, color_space_f,power_mode,power_off_vdac_flag,serial_reg_val);
    if(color_space_f != 0){
        param->color = color_space_f;
    }

    hdmitx_set_pll(param);
        hdmi_hw_reset(param);    
    if((param->VIC==HDMI_720p60)||(param->VIC==HDMI_720p50)||
        (param->VIC==HDMI_1080i60)||(param->VIC==HDMI_1080i50)){
        Wr(ENCP_VIDEO_HAVON_BEGIN,  Rd(ENCP_VIDEO_HAVON_BEGIN)-1);     
        Wr(ENCP_VIDEO_HAVON_END,  Rd(ENCP_VIDEO_HAVON_END)-1);     
    }

    if(use_tvenc_conf_flag==1){
        if((param->VIC==HDMI_480i60)||(param->VIC==HDMI_480i60_16x9)){
            hdmitx_set_tvenc_reg(param->VIC);    
        }
        else{
            goto set_tvenc;
        }
    }
    else if(use_tvenc_conf_flag==2){
            hdmitx_set_tvenc_reg(param->VIC);    
        }        
    else{
set_tvenc:        
        if((param->VIC==HDMI_480i60)||(param->VIC==HDMI_480i60_16x9)
            ||(param->VIC==HDMI_576i50)||(param->VIC==HDMI_576i50_16x9)){
            hdmi_tvenc480i_set(param);
        }            
        else if((param->VIC==HDMI_1080i60)||(param->VIC==HDMI_1080i50)){
            hdmi_tvenc1080i_set(param);
        } 
        else{
            hdmi_tvenc_set(param);
        }
    }
    
#ifdef CONFIG_AML_HDMI_TX_HDCP
    if(hdcpkey_status>=0){
    hdmi_wr_reg(TX_HDCP_MODE, hdmi_rd_reg(TX_HDCP_MODE)|0x80); //enable authentication
    }
    else{
        printk("HDMITX: HDCP Key error, disable authentication\n");    
    }
#endif    
#ifndef AVOS
    if(power_off_vdac_flag){
        //video_dac_disable();
        SET_CBUS_REG_MASK(VENC_VDAC_SETTING, 0x1f);
    }
#endif
    hdmitx_dump_tvenc_reg(param->VIC, 0);

    hdmi_wr_reg(0x011, 0x0f);   //Channels Power Up Setting ,"1" for Power-up ,"0" for Power-down,Bit[3:0]=CK,Data2,data1,data1,data0 Channels ;
    
    return 0;
}    


static void hdmitx_m1b_set_packet(int type, unsigned char* DB, unsigned char* HB)
{
    // AVI frame
    int i ;
    unsigned char ucData ;
    unsigned int pkt_reg_base=TX_PKT_REG_AVI_INFO_BASE_ADDR;
    int pkt_data_len=0;
    
    switch(type)
    {
        case HDMI_PACKET_AVI:
            pkt_reg_base=TX_PKT_REG_AVI_INFO_BASE_ADDR; 
            pkt_data_len=13;
            break;
        case HDMI_PACKET_VEND:
            pkt_reg_base=TX_PKT_REG_VEND_INFO_BASE_ADDR;
            pkt_data_len=6;
            break;
        case HDMI_AUDIO_INFO:
            pkt_reg_base=TX_PKT_REG_AUDIO_INFO_BASE_ADDR;
            pkt_data_len=9;
            break;
        default:
            break;
    }
    
    if(DB){
        for(i=0;i<pkt_data_len;i++){
            hdmi_wr_reg(pkt_reg_base+i+1, DB[i]);  
        }
    
        for(i = 0,ucData = 0; i < pkt_data_len ; i++)
        {
            ucData -= DB[i] ;
        }
        for(i=0; i<3; i++){
            ucData -= HB[i];
        }
        hdmi_wr_reg(pkt_reg_base+0x00, ucData);  
    
        hdmi_wr_reg(pkt_reg_base+0x1C, HB[0]);        
        hdmi_wr_reg(pkt_reg_base+0x1D, HB[1]);        
        hdmi_wr_reg(pkt_reg_base+0x1E, HB[2]);        
        hdmi_wr_reg(pkt_reg_base+0x1F, 0x00ff);        // Enable packet generation
    }
    else{
        hdmi_wr_reg(pkt_reg_base+0x1F, 0x0);        // disable packet generation
    }
#if 0
    printk("AVI:%02x\n",ucData);
    for(i=0;i<13;i++)
        printk("%02x ", DB[i]);
    printk("\n");
    for(i=0;i<3;i++)
        printk("%02x ", HB[i]);
#endif    
}


static void hdmitx_m1b_setaudioinfoframe(unsigned char* AUD_DB, unsigned char* CHAN_STAT_BUF)
{
    int i ;
    unsigned char AUD_HB[3]={0x84, 0x1, 0xa};
    hdmitx_m1b_set_packet(HDMI_AUDIO_INFO, AUD_DB, AUD_HB);    
    //channel status
    if(CHAN_STAT_BUF){
        for(i=0;i<24;i++){
            hdmi_wr_reg(TX_IEC60958_SUB1_OFFSET+i, CHAN_STAT_BUF[i]);        
            hdmi_wr_reg(TX_IEC60958_SUB2_OFFSET+i, CHAN_STAT_BUF[24+i]);
        }
    }
}
    
static int hdmitx_m1b_set_audmode(struct hdmi_tx_dev_s* hdmitx_device, Hdmi_tx_audio_para_t* audio_param)
{
    unsigned int audio_N_para = 6272;
    unsigned int audio_N_tolerance = 1;
//    unsigned int audio_CTS = 30000;

    hdmi_print(0,"HDMI DEBUG [%s] hdmitx_device->cur_VIC=[%d]\n", __FUNCTION__, hdmitx_device->cur_VIC);
    
//Refer to HDMI SPEC V1.4 Page 137
    switch(hdmitx_device->cur_VIC)
    {
        //TMDS Clock:27MHz
        case HDMI_480p60:
        case HDMI_480p60_16x9:
        case HDMI_576p50:
        case HDMI_576p50_16x9:
        case HDMI_480i60:
        case HDMI_480i60_16x9:
        case HDMI_576i50:
        case HDMI_576i50_16x9:
            switch(audio_param->sample_rate)
            {
                case FS_32K:
                case FS_44K1:
                    audio_N_para = 6272;
                    break;
                case FS_88K2:
                    audio_N_para = 12544;
                    break;
                case FS_176K4:
                    audio_N_para = 25088;
                    break;
                case FS_48K:
                    audio_N_para = 6144;
                    break;
                case FS_96K:
                    audio_N_para = 12288;
                    break;
                case FS_192K:
                    audio_N_para = 24576;
                    break;
                default:
                    break;
            }
            break;
        //TMDS Clock:74.176MHz
        case HDMI_720p60:
        case HDMI_1080i60:
        case HDMI_1080p30:
        case HDMI_1080p24:
            switch(audio_param->sample_rate)
            {
                case FS_32K:
                    audio_N_para = 11648;
                    break;
                case FS_44K1:
                    audio_N_para = 17836;
                    break;
                case FS_88K2:
                    audio_N_para = 35672;
                    break;
                case FS_176K4:
                    audio_N_para = 71344;
                    break;
                case FS_48K:
                    audio_N_para = 11648;
                    break;
                case FS_96K:
                    audio_N_para = 23296;
                    break;
                case FS_192K:
                    audio_N_para = 46592;
                    break;
                default:
                    break;
            }
            break;
        //TMDS Clock:74.25MHz
        case HDMI_720p50:
        case HDMI_1080i50:
            switch(audio_param->sample_rate)
            {
                case FS_32K:
                    audio_N_para = 4096;
                    break;
                case FS_44K1:
                    audio_N_para = 6272;
                    break;
                case FS_88K2:
                    audio_N_para = 12544;
                    break;
                case FS_176K4:
                    audio_N_para = 25088;
                    break;
                case FS_48K:
                    audio_N_para = 6144;
                    break;
                case FS_96K:
                    audio_N_para = 12288;
                    break;
                case FS_192K:
                    audio_N_para = 24576;
                    break;
                default:
                    break;
            }
            break;
        //TMDS Clock:148.5MHz
        case HDMI_1080p50:
            switch(audio_param->sample_rate)
            {
                case FS_32K:
                    audio_N_para = 4096;
                    break;
                case FS_44K1:
                    audio_N_para = 6272;
                    break;
                case FS_88K2:
                    audio_N_para = 12544;
                    break;
                case FS_176K4:
                    audio_N_para = 25088;
                    break;
                case FS_48K:
                    audio_N_para = 6144;
                    break;
                case FS_96K:
                    audio_N_para = 12288;
                    break;
                case FS_192K:
                    audio_N_para = 24576;
                    break;
                default:
                    break;
            }
            break;
        //TMDS Clock:148.325MHz
        case HDMI_1080p60:
            switch(audio_param->sample_rate)
            {
                case FS_32K:
                    audio_N_para = 11648;
                    break;
                case FS_44K1:
                    audio_N_para = 8918;
                    break;
                case FS_88K2:
                    audio_N_para = 17836;
                    break;
                case FS_176K4:
                    audio_N_para = 35672;
                    break;
                case FS_48K:
                    audio_N_para = 5824;
                    break;
                case FS_96K:
                    audio_N_para = 11648;
                    break;
                case FS_192K:
                    audio_N_para = 23296;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    hdmi_wr_reg(TX_SYS1_ACR_N_0, (audio_N_para&0xff)); // N[7:0]
    hdmi_wr_reg(TX_SYS1_ACR_N_1, (audio_N_para>>8)&0xff); // N[15:8]
    hdmi_wr_reg(TX_SYS1_ACR_N_2, (audio_N_tolerance<<4)|((audio_N_para>>16)&0xf)); // N[19:16]
    hdmi_wr_reg(TX_AUDIO_CONTROL,   hdmi_rd_reg(TX_AUDIO_CONTROL)|0x1); 

    hdmi_wr_reg(TX_SYS0_ACR_CTS_0, 0);      //audio_CTS & 0xff);
    hdmi_wr_reg(TX_SYS0_ACR_CTS_1, 0);      //(audio_CTS>>8) & 0xff);
    hdmi_wr_reg(TX_SYS0_ACR_CTS_2, 0);      //(1<<5)|(1<<4)|((audio_CTS>>16)&0xf));
    switch(audio_param->type)
    {
        case CT_PCM:
            i2s_to_spdif_flag = 0;
            enable_audio_i2s();
            break;
        case CT_AC_3:
        case CT_DTS:
            i2s_to_spdif_flag = 1;
            enable_audio_spdif();
            break;
        default:
            i2s_to_spdif_flag = 0;
            enable_audio_i2s();
            break;
    }

//    if(i2s_to_spdif_flag)
//        enable_audio_spdif();
//    else
//        enable_audio_i2s();
    return 0;
}    
    
static void hdmitx_m1b_setupirq(hdmitx_dev_t* hdmitx_device)
{
#ifndef AVOS
   int r;
   r = request_irq(INT_HDMI_TX, &intr_handler,
                    IRQF_SHARED, "amhdmitx-hpd",
                    (void *)hdmitx_device);
    Rd(A9_0_IRQ_IN1_INTR_STAT_CLR);
    Wr(A9_0_IRQ_IN1_INTR_MASK, Rd(A9_0_IRQ_IN1_INTR_MASK)|(1 << 25));


#else
  AVRequestIrqContext(ISR_TYPE_GENERAL2, (1 << INT_HDMI_TX), intr_handler, NULL, (void *)hdmitx_device);  		         	   

  Rd(SYS_CPU_0_IRQ_IN1_INTR_STAT_CLR);
  Wr(SYS_CPU_0_IRQ_IN1_INTR_MASK, Rd(SYS_CPU_0_IRQ_IN1_INTR_MASK)|(1 << INT_HDMI_TX));

#endif
}    


#if 1

//Expect 8*10-Bit shift pattern data:
//
//0x2e3   = 1011100011
//0x245   = 1001000101
//0x1cb   = 0111001011
//0x225   = 1000100101
//0x2da   = 1011011010
//0x3e0   = 1111100000
//0x367   = 1101100111
//0x000   = 0000000000

static void turn_on_shift_pattern (void)
{
    unsigned int tmp_add_data;
    hdmi_wr_reg(TX_SYS0_BIST_DATA_0, 0x00);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_1, 0x6c);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_2, 0xfe);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_3, 0x41);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_4, 0x5b);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_5, 0x91);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_6, 0x3a);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_7, 0x9d);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_8, 0x68);
    hdmi_wr_reg(TX_SYS0_BIST_DATA_9, 0xc7);

    //tmp_add_data[7:6] = 2'b0;     // audio_source_select[1:0]
    //tmp_add_data[5] = 1'b0;       // external_packet_enable
    //tmp_add_data[4] = 1'b1 ;      // internal_packet_enable
    //tmp_add_data[3:2] = 2'b0;     // afe_fifo_source_select_lane_1[1:0]
    //tmp_add_data[1:0] = 2'd3 ;    // afe_fifo_source_select_lane_0[1:0] : 0=data path; 1=injected on lane 0; 2=inject on lane 1; 3=BIST.
    tmp_add_data = 0x13;
    hdmi_wr_reg(TX_CORE_DATA_CAPTURE_2, tmp_add_data);

    hdmi_wr_reg(TX_SYS0_BIST_CONTROL, 0x00); // Reset BIST
    hdmi_wr_reg(TX_SYS0_BIST_CONTROL, 0xc0); // Enable shift pattern BIST
}

static void turn_off_shift_pattern (void)
{
    unsigned int tmp_add_data;
    hdmi_wr_reg(TX_SYS0_BIST_CONTROL, 0x00); // Reset BIST

    //tmp_add_data[7:6] = 2'b0;     // audio_source_select[1:0]
    //tmp_add_data[5] = 1'b0;       // external_packet_enable
    //tmp_add_data[4] = 1'b1 ;      // internal_packet_enable
    //tmp_add_data[3:2] = 2'b0;     // afe_fifo_source_select_lane_1[1:0]
    //tmp_add_data[1:0] = 2'd0 ;    // afe_fifo_source_select_lane_0[1:0] : 0=data path; 1=injected on lane 0; 2=inject on lane 1; 3=BIST.
    tmp_add_data = 0x10;
    hdmi_wr_reg(TX_CORE_DATA_CAPTURE_2, tmp_add_data);
}

static void turn_on_prbs_mode(int prbs_mode)
{
    unsigned int tmp_add_data;
   tmp_add_data     = 0;
    tmp_add_data    |= 0    << 6;   // [7:6] audio_source_select[1:0]
    tmp_add_data    |= 0    << 5;   //   [5] external_packet_enable
    tmp_add_data    |= 0    << 4;   //   [4] internal_packet_enable
    tmp_add_data    |= 0    << 2;   // [3:2] afe_fifo_source_select_lane_1[1:0]: 0=DATA_PATH; 1=TMDS_LANE_0; 2=TMDS_LANE_1; 3=BIST_PATTERN
    tmp_add_data    |= 3    << 0;   // [1:0] afe_fifo_source_select_lane_0[1:0]: 0=DATA_PATH; 1=TMDS_LANE_0; 2=TMDS_LANE_1; 3=BIST_PATTERN
    hdmi_wr_reg(TX_CORE_DATA_CAPTURE_2, tmp_add_data); // 0x03

    tmp_add_data     = 0;
    tmp_add_data    |= 0    << 7;   //   [7] monitor_lane_1
    tmp_add_data    |= 0    << 4;   // [6:4] monitor_select_lane_1[2:0]
    tmp_add_data    |= 1    << 3;   //   [3] monitor_lane_0
    tmp_add_data    |= 7    << 0;   // [2:0] monitor_select_lane_0[2:0]: 7=TMDS_ENCODE
    hdmi_wr_reg(TX_CORE_DATA_MONITOR_1, tmp_add_data); // 0x0f

    // Program PRBS_MODE
    hdmi_wr_reg(TX_SYS1_PRBS_DATA, prbs_mode); // 0=PRBS 11; 1=PRBS 15; 2=PRBS 7; 3=PRBS 31.
    // Program PRBS BIST

    tmp_add_data     = 0;
    tmp_add_data    |= 1    << 7;   //   [7] afe_bist_enable
    tmp_add_data    |= 0    << 6;   //   [6] tmds_shift_pattern_select
    tmp_add_data    |= 3    << 4;   // [5:4] tmds_prbs_pattern_select[1:0]:
                                    //       0=output all 0; 1=output 8-bit pattern;
                                    //       2=output 1-bit differential pattern; 3=output 10-bit pattern
    tmp_add_data    |= 0    << 3;   //   [3] Rsrv
    tmp_add_data    |= 0    << 0;   // [2:0] tmds_repeat_bist_pattern[2:0]
   hdmi_wr_reg(TX_SYS0_BIST_CONTROL, tmp_add_data); // 0xb0

   printk("PRBS mode %d On\n", prbs_mode);
}
    
#endif

static void hdmitx_m1b_uninit(hdmitx_dev_t* hdmitx_device)
{
#ifndef AVOS
    Rd(A9_0_IRQ_IN1_INTR_STAT_CLR);
    Wr(A9_0_IRQ_IN1_INTR_MASK, Rd(A9_0_IRQ_IN1_INTR_MASK)&(~(1 << 25)));
    free_irq(INT_HDMI_TX, (void *)hdmitx_device);
#ifdef CEC_SUPPORT
    //CEC
    Wr(A9_0_IRQ_IN1_INTR_MASK, Rd(A9_0_IRQ_IN1_INTR_MASK)&(~(1 << 23)));
    free_irq(INT_HDMI_CEC, (void *)hdmitx_device);
#endif    
#ifdef HPD_DELAY_CHECK
    del_timer(&hpd_timer);    
#endif

#endif    
    hdmi_print(1,"power off hdmi, unmux hpd\n");
    
    phy_pll_off();
    digital_clk_off(7); //off sys clk
    unmux_hpd();
}    

static int hdmitx_m1b_cntl(hdmitx_dev_t* hdmitx_device, int cmd, unsigned argv)
{
    if(cmd == HDMITX_HWCMD_POWERMODE_SWITCH){
        power_mode=argv;
        hdmi_hw_set_powermode(power_mode, hdmitx_device->cur_VIC);
    }
#ifndef AVOS
    else if(cmd == HDMITX_HWCMD_VDAC_OFF){
        power_off_vdac_flag=1;
        //video_dac_disable();
        SET_CBUS_REG_MASK(VENC_VDAC_SETTING, 0x1f);
    }
#endif 
    else if(cmd == HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH){
        /* turnon digital module if gpio is high */
        if(is_hpd_muxed() == 0){
            if(read_hpd_gpio()){
                hdmi_print(1,"mux hpd\n");
                digital_clk_on(4);
                delay_us(1000*100);
                mux_hpd();
            }
        }
    } 
    else if(cmd == HDMITX_HWCMD_MUX_HPD){
         mux_hpd();
    } 
    else if(cmd == HDMITX_HWCMD_TURNOFF_HDMIHW){
        int unmux_hpd_flag = argv;
        WRITE_MPEG_REG(VENC_DVI_SETTING, READ_MPEG_REG(VENC_DVI_SETTING)&(~(1<<13))); //bit 13 is used by HDMI only
        digital_clk_on(4); //enable sys clk so that hdmi registers can be accessed when calling phy_pll_off/digit_clk_off
        if(unmux_hpd_flag){
            hdmi_print(1,"power off hdmi, unmux hpd\n");
            phy_pll_off();
            digital_clk_off(7); //off sys clk
            unmux_hpd();
        }
        else{
            hdmi_print(1,"power off hdmi\n");
            phy_pll_off();
            digital_clk_off(3); //do not off sys clk
        }
    }
    else if(cmd == HDMITX_HWCMD_TURN_ON_PRBS){
        Rd(A9_0_IRQ_IN1_INTR_STAT_CLR);
        Wr(A9_0_IRQ_IN1_INTR_MASK, Rd(A9_0_IRQ_IN1_INTR_MASK)&(~(1 << 25)));
        turn_on_prbs_mode(argv);    
    }
    else if(cmd == HDMITX_OUTPUT_ENABLE){
             //Channels Power Up Setting ,"1" for Power-up ,"0" for Power-down,Bit[3:0]=CK,Data2,data1,data1,data0 Channels ;
        if(argv){
            hdmi_wr_reg(TX_SYS1_TERMINATION, hdmi_rd_reg(TX_SYS1_TERMINATION)|0xf);
        }
        else{
            hdmi_wr_reg(TX_SYS1_TERMINATION, hdmi_rd_reg(TX_SYS1_TERMINATION)&(~0xf));
        }
    }
    else if(cmd == HDMITX_GET_AUTHENTICATE_STATE){
        if(((hdmi_rd_reg(TX_HDCP_ST_AUTHENTICATION)&0xf)==0x4)
            &&((hdmi_rd_reg(TX_HDCP_ST_STATUS_3)&0xa0)==0xa0)){
            return 1;
        }
        else{
            return 0;
        }
    }
    else if(cmd == HDMITX_FORCE_480P_CLK){
        switch(argv){
            case 200:
                Wr(HHI_VID_PLL_CNTL, 0x500632);
                printk("hdmi: set video pll 20Mhz\n");
                break;    
            case 225:
                Wr(HHI_VID_PLL_CNTL, 0x50084b);
                printk("hdmi: set video pll 22.5Mhz\n");
                break;    
            case 250:
                Wr(HHI_VID_PLL_CNTL, 0x500c7d);
                printk("hdmi: set video pll 25Mhz\n");
                break;
            case 270:
                Wr(HHI_VID_PLL_CNTL, 0x50042d);
                printk("hdmi: set video pll 27Mhz\n");
                break;    
        }
        
    }
    return 0;
                
}
#if 0
#include <mach/gpio.h>
struct gpio_addr
{
	unsigned long mode_addr;
	unsigned long out_addr;
	unsigned long in_addr;
};
static struct gpio_addr gpio_addrs[]=
{
	[PREG_EGPIO]={PREG_EGPIO_EN_N,PREG_EGPIO_O,PREG_EGPIO_I},
	[PREG_FGPIO]={PREG_FGPIO_EN_N,PREG_FGPIO_O,PREG_FGPIO_I},
	[PREG_GGPIO]={PREG_GGPIO_EN_N,PREG_GGPIO_O,PREG_GGPIO_I},
	[PREG_HGPIO]={PREG_HGPIO_EN_N,PREG_HGPIO_O,PREG_HGPIO_I},
};


static int set_gpio_valaaa(gpio_bank_t bank,int bit,unsigned long val)
{
	unsigned long addr=gpio_addrs[bank].out_addr;
	WRITE_CBUS_REG_BITS(addr,val?1:0,bit,1);

	return 0;
}

static unsigned long  get_gpio_valaaa(gpio_bank_t bank,int bit)
{
	unsigned long addr=gpio_addrs[bank].in_addr;
	return READ_CBUS_REG_BITS(addr,bit,1);
}

static int set_gpio_modeaaa(gpio_bank_t bank,int bit,gpio_mode_t mode)
{
	unsigned long addr=gpio_addrs[bank].mode_addr;
	WRITE_CBUS_REG_BITS(addr,mode,bit,1);
	return 0;
}
static gpio_mode_t get_gpio_modeaaa(gpio_bank_t bank,int bit)
{
	unsigned long addr=gpio_addrs[bank].mode_addr;
	return (READ_CBUS_REG_BITS(addr,bit,1)>0)?(GPIO_INPUT_MODE):(GPIO_OUTPUT_MODE);
}

#endif

static void hdmitx_print_info(hdmitx_dev_t* hdmitx_device, int printk_flag)
{
    hdmi_print(printk_flag, "------------------\nHdmitx driver version: %s\nSerial %x\nColor Depth %d\n", HDMITX_VER, serial_reg_val, color_depth_f);
    hdmi_print(printk_flag, "chip type %c\n", hdmi_chip_type);
    hdmi_print(printk_flag, "reset sequence %d\n", new_reset_sequence_flag);
    hdmi_print(printk_flag, "power mode %d\n", power_mode);
    hdmi_print(printk_flag, "%spowerdown when unplug\n",hdmitx_device->unplug_powerdown?"":"do not ");
    hdmi_print(printk_flag, "use_tvenc_conf_flag=%d\n",use_tvenc_conf_flag); 
    hdmi_print(printk_flag, "vdac %s\n", power_off_vdac_flag?"off":"on");
    hdmi_print(printk_flag, "hdmi audio %s\n", hdmi_audio_off_flag?"off":"on");
    if(!hdmi_audio_off_flag){
        hdmi_print(printk_flag, "audio out type %s\n", i2s_to_spdif_flag?"spdif":"i2s");
        printk("SPDIF AC3 is %s\n", (hdmitx_device->cur_audio_param.type == CT_AC_3)?"on":"off");
        printk("SPDIF DTS is %s\n", (hdmitx_device->cur_audio_param.type == CT_DTS)?"on":"off");
    }
    hdmi_print(printk_flag, "delay flag %d\n", delay_flag);
    if(hdmitx_device->cec_func_flag){
        hdmi_print(printk_flag, "CEC Ver: %s\n", CEC_VERSION);
    }
    hdmi_print(printk_flag, "CEC function: %sable\n", hdmitx_device->cec_func_flag?"En":"Dis");
    hdmi_print(printk_flag, "------------------\n");
}

extern void dump_hdmi_cec_reg(void)
{
    int i = 0, j = 0;
    printk("--Dump HDMI CEC Regs [Addr] Val --\n");
    printk("[%4x]: ", CEC0_BASE_ADDR+i);
    for(i = CEC_TX_MSG_0_HEADER; i < CEC_CLOCK_DIV_L+1; i++){
        printk("%2x ", (unsigned int)hdmi_rd_reg(CEC0_BASE_ADDR+i));
        j++;
        if(j%8 == 0)
            printk("\n[%4x]: ", CEC0_BASE_ADDR+i+1);
    }
    j = 0;
    i = CEC_RX_MSG_0_HEADER;
    printk("\n[%4x]: ", CEC0_BASE_ADDR+i);
    for(i = CEC_RX_MSG_0_HEADER; i < CEC_TX_NUM_MSG+1; i++){
        printk("%2x ", (unsigned int)hdmi_rd_reg(CEC0_BASE_ADDR+i));
        j++;
        if(j%8 == 0)
            printk("\n[%4x]: ", CEC0_BASE_ADDR+i+1);
    }
    printk("\n");
}
static void hdmitx_m1b_debug(hdmitx_dev_t* hdmitx_device, const char* buf)
{ 
#ifndef AVOS
    char tmpbuf[128];
    int i=0;
    unsigned int adr;
    unsigned int value=0;
    int size = strlen(buf);
    while((buf[i])&&(buf[i]!=',')&&(buf[i]!=' ')){
        tmpbuf[i]=buf[i];
        i++;    
    }
    tmpbuf[i]=0;
    if(strncmp(tmpbuf, "dumpreg", 7)==0){
        hdmitx_dump_tvenc_reg(hdmitx_device->cur_VIC, 1);
        return;
    }
    else if(strncmp(tmpbuf, "hdmiaudio", 9)==0){
        value=simple_strtoul(tmpbuf+9, NULL, 16);
        if(value == 1){
            hdmi_audio_off_flag = 1;
            hdmi_audio_init(i2s_to_spdif_flag);
        }
        else if(value == 0){
            hdmi_audio_off_flag = 0;
            hdmi_wr_reg(TX_AUDIO_PACK, 0x00); // disable audio sample packets
        }
    }
    else if(strncmp(tmpbuf, "cec", 3)==0){
        int j = 0;
//        int i_pre = 0;
        char buf_tmp[128];
        char tmp[128];
        if (size > 30)  return;
//        printk("buf: %s\n", buf);//////
//        printk("size: %d\n", size);//////
        memcpy(buf_tmp, buf, size);
        buf_tmp[size] = '\0';
        for(i = 0; i < size+1; i++){
        	  if((buf[i] == ' ') && (buf[i+1] != '\0')){
        	  	  tmp[j] = simple_strtoul(&buf[i+1], NULL, 16);
//        	  	  printk("tmp[%d] = %d\n", j, tmp[j]);//////
        	  	  j ++;
        	  }
        }

        cec_test_function(tmp, j);
        return;
    }
    else if(strncmp(tmpbuf, "cfgreg", 6)==0){
        adr=simple_strtoul(tmpbuf+6, NULL, 16);
        value=simple_strtoul(buf+i+1, NULL, 16);
        hdmitx_config_tvenc_reg(hdmitx_device->cur_VIC, adr, value);
        return;
    }
    else if(strncmp(tmpbuf, "tvenc_flag", 10)==0){
        use_tvenc_conf_flag = tmpbuf[10]-'0';
        printk("set use_tvenc_conf_flag = %d\n", use_tvenc_conf_flag);
    }
#ifdef CEC_SUPPORT    
    else if(tmpbuf[0]=='c'){
        if(tmpbuf[1]=='r'){
            //cec_init(hdmitx_device);
            cec_hw_reset();
            printk("CEC Reinit & Reset H/W module\n");
        }
        if((tmpbuf[1]>='0') && (tmpbuf[1]<='9'))
            cec_test_fun_reserved(tmpbuf[1]-'0');
        return;
    }
    else if(strncmp(tmpbuf, "dumpcec", 7)==0){
            dump_hdmi_cec_reg();
        return;
    }
#endif    
    else if(strncmp(tmpbuf, "ignore_unplug_on", 16)==0){
        hpd_debug_mode|=HPD_DEBUG_IGNORE_UNPLUG;
    }
    else if(strncmp(tmpbuf, "force_plug_off", 17)==0){
        hpd_debug_mode&=~HPD_DEBUG_IGNORE_UNPLUG;
    }
    else if(strncmp(tmpbuf, "reset", 5)==0){
        if(tmpbuf[5]=='0')
            new_reset_sequence_flag=0;
        else 
            new_reset_sequence_flag=1;
        return;
    }
    else if(strncmp(tmpbuf, "delay_flag", 10)==0){
        delay_flag = tmpbuf[10]-'0';    
    }
    else if(tmpbuf[0]=='v'){
        hdmitx_print_info(hdmitx_device, 1);
        return;    
    }
    else if(tmpbuf[0]=='s'){
        serial_reg_val=simple_strtoul(tmpbuf+1,NULL,16);
        return;
    }
    else if(tmpbuf[0]=='c'){
        if(tmpbuf[1]=='d'){
            color_depth_f=simple_strtoul(tmpbuf+2,NULL,10);
            if((color_depth_f!=24)&&(color_depth_f!=30)&&(color_depth_f!=36)){
                printk("Color depth %d is not supported\n", color_depth_f);
                color_depth_f=0;
            }
            return;
        }
        else if(tmpbuf[1]=='s'){
            color_space_f=simple_strtoul(tmpbuf+2,NULL,10);
            if(color_space_f>2){
                printk("Color space %d is not supported\n", color_space_f);
                color_space_f=0;
            }
        }
    }
    else if(strncmp(tmpbuf,"i2s",2)==0){
        if(strncmp(tmpbuf+3,"off",3)==0)
            i2s_to_spdif_flag=1;
        else   
            i2s_to_spdif_flag=0;
    }
    else if(strncmp(tmpbuf, "pattern_on", 10)==0){
        turn_on_shift_pattern();
        printk("Shift Pattern On\n");
        return;        
    }
    else if(strncmp(tmpbuf, "pattern_off", 11)==0){
        turn_off_shift_pattern();
        printk("Shift Pattern Off\n");
        return;        
    }
    else if(strncmp(tmpbuf, "prbs", 4)==0){
        int prbs_mode =simple_strtoul(tmpbuf+4, NULL, 10);
        turn_on_prbs_mode(prbs_mode);
        return;
    }
    else if(tmpbuf[0]=='w'){
        adr=simple_strtoul(tmpbuf+2, NULL, 16);
        value=simple_strtoul(buf+i+1, NULL, 16);
        if(buf[1]=='h'){
            hdmi_wr_reg(adr, value);
        }
        else if(buf[1]=='c'){
            WRITE_MPEG_REG(adr, value);
        }
        else if(buf[1]=='p'){
            WRITE_APB_REG(adr, value);   
        }
        printk("write %x to %s reg[%x]\n",value,buf[1]=='p'?"APB":(buf[1]=='h'?"HDMI":"CBUS"), adr);
    }
    else if(tmpbuf[0]=='r'){
        adr=simple_strtoul(tmpbuf+2, NULL, 16);
        if(buf[1]=='h'){
            value = hdmi_rd_reg(adr);
            
        }
        else if(buf[1]=='c'){
            value = READ_MPEG_REG(adr);
        }
        else if(buf[1]=='p'){
            value = READ_APB_REG(adr);
        }
        printk("%s reg[%x]=%x\n",buf[1]=='p'?"APB":(buf[1]=='h'?"HDMI":"CBUS"), adr, value);
    }
#endif    
}

#ifdef HPD_DELAY_CHECK
static void hpd_post_process(unsigned long arg)
{
    hdmitx_dev_t* hdmitx_device = (hdmitx_dev_t*)arg;
    hdmi_print(1,"hpd_post_process\n");
    if (hdmi_rd_reg(TX_HDCP_ST_EDID_STATUS) & (1<<1)) {
        // Start DDC transaction
        hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) | (1<<6)); // Assert sys_trigger_config
        hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) & ~(1<<1)); // Release sys_trigger_config_semi_manu
    
        hdmitx_device->cur_edid_block=0;
        hdmitx_device->cur_phy_block_ptr=0;
        hdmitx_device->hpd_event = 1;
        hdmi_print(1,"hpd_event 1\n");
    } 
    else{ //HPD falling
        hdmi_wr_only_reg(OTHER_BASE_ADDR + HDMI_OTHER_INTR_STAT_CLR,  1 << 1); //clear HPD falling interrupt in hdmi module 
        hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) & ~(1<<6)); // Release sys_trigger_config
        hdmi_wr_reg(TX_HDCP_EDID_CONFIG, hdmi_rd_reg(TX_HDCP_EDID_CONFIG) | (1<<1)); // Assert sys_trigger_config_semi_manu
        hdmitx_device->hpd_event = 2;
        hdmi_print(1,"hpd_event 2\n");
    }

    del_timer(&hpd_timer);    
}    
#endif

void HDMITX_M1B_Init(hdmitx_dev_t* hdmitx_device)
{
    hdmitx_device->HWOp.SetPacket = hdmitx_m1b_set_packet;
    hdmitx_device->HWOp.SetAudioInfoFrame = hdmitx_m1b_setaudioinfoframe;
    hdmitx_device->HWOp.GetEDIDData = hdmitx_m1b_getediddata;
    hdmitx_device->HWOp.SetDispMode = hdmitx_m1b_set_dispmode;
    hdmitx_device->HWOp.SetAudMode = hdmitx_m1b_set_audmode;
    hdmitx_device->HWOp.SetupIRQ = hdmitx_m1b_setupirq;
    hdmitx_device->HWOp.DebugFun = hdmitx_m1b_debug;
    hdmitx_device->HWOp.UnInit = hdmitx_m1b_uninit;
    hdmitx_device->HWOp.Cntl = hdmitx_m1b_cntl;
#ifdef HPD_DELAY_CHECK
    /*hdp timer*/
    init_timer(&hpd_timer);
    hpd_timer.function = &hpd_post_process;
    hpd_timer.data = (unsigned long)hdmitx_device;
    /**/
#endif
    
    if(hdmi_chip_type==0){
        check_chip_type();
    }    
    
    //Wr(HHI_HDMI_PLL_CNTL, Rd(HHI_HDMI_PLL_CNTL)|(1<<30)); //disable HDMI PLL
    // --------------------------------------------------------
    // Configure HDMI TX analog, and use HDMI PLL to generate TMDS clock
    // --------------------------------------------------------
    // Enable APB3 fail on error
    WRITE_APB_REG(HDMI_CNTL_PORT, READ_APB_REG(HDMI_CNTL_PORT)|(1<<15)); //APB3 err_en

    /**/    
    hdmi_hw_init(hdmitx_device);
    
#ifdef CEC_SUPPORT    
    /*cec config*/
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_H, 0x0 );
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_CLOCK_DIV_L, 0xf0 );
//    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_LOGICAL_ADDR0, (0x1 << 4) | CEC0_LOG_ADDR);
#endif    
}    

#ifndef AVOS    
static  int __init hdmi_chip_select(char *s)
{
#ifndef AML_A3
	switch(s[0])
	{
		case 'a':
		case 'A':
			hdmi_chip_type = HDMI_M1A;
			break;
		case 'b':
		case 'B':
      i2s_to_spdif_flag=1;
		  hdmi_chip_type = HDMI_M1B;
		  break;
    default:
			hdmi_chip_type = HDMI_M1C;
			break;
	}
#endif	
	return 0;
}

__setup("chip=",hdmi_chip_select);

void hdmi_set_audio_para(int para)
{
	aud_para = para;

}
#endif

#ifdef CEC_SUPPORT
static int cec_echo_flag=1;    
typedef struct {
	int msg[5];
	int length;
} cec_msg_t;
cec_msg_t msg_array[] = {
	{{0x40, 0x04},2},
	{{0x4f, 0x82, 0x20, 0x00}, 4},
	{{0x4f, 0x82, 0x20, 0x00}, 4},
};
static void cec_test_fun_reserved(unsigned cmd_no)
{
    /* use CEC0_LOG_ADDR as target address */
    int i;
    cec_echo_flag=0;
    printk("CEC test is starting!!!!!!!!\n");

    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_LOGICAL_ADDR0, (0x1 << 4) | 4);

    for (i = 0; i < msg_array[cmd_no].length; i++)
    {
        hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_0_HEADER + i, msg_array[cmd_no].msg[i]);
    }
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_LENGTH, msg_array[cmd_no].length);
    
    hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_CMD, TX_REQ_CURRENT);
    //hdmi_wr_reg(CEC0_BASE_ADDR+CEC_TX_MSG_CMD, TX_REQ_NEXT);

}
#endif

#if 0
static void hdcp_status_loop_check()
{
    static unsigned reg190=0;
    static unsigned reg192=0;
    static unsigned reg195=0;
    static unsigned reg19f=0;
    static unsigned reg194=0;
    unsigned char check_flag=0;
    if(hdmi_rd_reg(0x190)!=reg190){
        reg190=hdmi_rd_reg(0x190);
        check_flag|=0x1;
    }
    if(hdmi_rd_reg(0x192)!=reg192){
        reg192=hdmi_rd_reg(0x192);
        check_flag|=0x2;
    }
    if(hdmi_rd_reg(0x195)!=reg195){
        reg195=hdmi_rd_reg(0x195);
        check_flag|=0x4;
    }
    if(hdmi_rd_reg(0x19f)!=reg19f){
        reg19f=hdmi_rd_reg(0x19f);
        check_flag|=0x8;
    }
    if(hdmi_rd_reg(0x194)!=reg194){
        reg194=hdmi_rd_reg(0x194);
        check_flag|=0x10;
    }
    if(check_flag){
        printk("[%c190,%c192,%c195,%c19f,%c194]=%02x,%02x,%02x,%02x,%02x\n",
            (check_flag&0x1)?'*':' ',(check_flag&0x2)?'*':' ', 
            (check_flag&0x4)?'*':' ',(check_flag&0x8)?'*':' ',
                (check_flag&0x10)?'*':' ',
                reg190,reg192,reg195,reg19f,reg194);
    }
}            
#endif            
    
