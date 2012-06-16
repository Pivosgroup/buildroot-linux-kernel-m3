
/*
 * Amlogic Apollo
 * frame buffer driver
 *	-------hdmi output
 * Copyright (C) 2009 Amlogic, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  zhou zhi<zhi.zhou@amlogic.com>
 * Firset add at:2009-7-28
 */ 
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
    
#include <linux/major.h>
#include <linux/delay.h>
    
#include <asm/uaccess.h>
    
#include "hdmi_module.h"
#include "hdmi_debug.h"
#include "hdmi_i2c.h"
#include "hdmi_video.h"
#include "hdmi_global.h"
    
#include "asm/arch/am_regs.h"
void RESET_HDMI_PARAS_TO_UPLPUG(Hdmi_info_para_t * info) 
{
	info->output_state = CABLE_UNPLUG;
	info->hpd_state = 0;
	info->txinit_flag = 0;
	info->cea_on_fisrt_page = 0;
	info->need_check_edid = 0;
	info->auth_state = HDCP_NO_AUTH;
	
	    //info->old_auth_state = HDCP_NO_AUTH;
	    info->auto_ri_flag = 1;	// If == 1, turn on Auto Ri Checking
	info->hw_sha_calculator_flag = 1;	// If  == 1, use the HW SHA calculator, otherwise, use SW SHA calculator
	info->sync_emb_flag = 0;	//bit0: CCIR656 ; bit1: sync sync embedded; bit2: DDR    
	//info->sync_emb_flag = 3;       //bit0: CCIR656 ; bit1: sync sync embedded; bit2: DDR  
	info->videopath_inindex = 4;	// YCbCr24 bit;
	info->videopath_outindex = 2;	// Video Output Color Space Conversion : 0 - RGB out; 1 - YCbr (4:4:4) out; 2 - YCbr (4:2:2) out;   
	info->videopath_clocke_edge = 0;	// Falling edge;
	info->videopath_deep_color = 0;	// 24/30/36 bit; 
	
	    //0 :  Pixel data is not replicated
	    //1 :  Pixels are replicated once (each sent twice)
	    //2 :  RSVD
	    //3:   Pixels are replicated 4 times (each sent four times) 
	    info->repeat_pixel_flag = 0;
	info->audio_mclk = 1;	//MCLK is 256*Fs
	info->init_hdmi_flag = 1;
	info->init_dvi_flag = 0;
	info->hpd_change_flag = 0;
	info->SPDIF_stream_error = 0;
	info->Ri_128_flag = 0;
	info->audio_fifo_overflow = 0;
	info->audio_fifo_underflow = 0;
	info->audio_cts_status_err_flag = 0;
	info->back_down_flag = 0;
	info->SPDIF_parity_error = 0;
	info->Ri_no_match = 0;
	info->Ri_no_change = 0;
	info->check_hdmi_interrupt = 0;
	info->video_480i.support_format = 0;
	info->video_480i.support_16_9 = 0;
	info->video_480i.support_4_3 = 0;
	info->video_576i.support_format = 0;
	info->video_576i.support_16_9 = 0;
	info->video_576i.support_4_3 = 0;
	info->video_480p.support_format = 0;
	info->video_480p.support_16_9 = 0;
	info->video_480p.support_4_3 = 0;
	info->video_576p.support_format = 0;
	info->video_576p.support_16_9 = 0;
	info->video_576p.support_4_3 = 0;
	info->video_720p_60hz.support_format = 0;
	info->video_720p_60hz.support_16_9 = 0;
	info->video_720p_60hz.support_4_3 = 0;
	info->video_720p_50hz.support_format = 0;
	info->video_720p_50hz.support_16_9 = 0;
	info->video_720p_50hz.support_4_3 = 0;
	info->video_1080i_60hz.support_format = 0;
	info->video_1080i_60hz.support_16_9 = 0;
	info->video_1080i_60hz.support_4_3 = 0;
	info->video_1080i_50hz.support_format = 0;
	info->video_1080i_50hz.support_16_9 = 0;
	info->video_1080i_50hz.support_4_3 = 0;
	info->video_1080p_60hz.support_format = 0;
	info->video_1080p_60hz.support_16_9 = 0;
	info->video_1080p_60hz.support_4_3 = 0;
	info->video_1080p_50hz.support_format = 0;
	info->video_1080p_50hz.support_16_9 = 0;
	info->video_1080p_50hz.support_4_3 = 0;
	info->support_underscan_flag = 0;
	info->support_ycbcr444_flag = 0;
	info->support_ycbcr422_flag = 0;
	info->support_basic_audio_flag = 0;
	
	    //A Source shall not transmit an ISRC1 or ISRC2 Packet to a Sink that does not have Supports_AI = 1 
	    //International Standard Recording Code (ISRC) 
	    info->support_ai_flag = 0;
	info->audio_linear_pcm.support_flag = 0;
	info->audio_linear_pcm.max_channel_num = 0;
	info->audio_linear_pcm._192k = 0;
	info->audio_linear_pcm._176k = 0;
	info->audio_linear_pcm._96k = 0;
	info->audio_linear_pcm._88k = 0;
	info->audio_linear_pcm._48k = 0;
	info->audio_linear_pcm._44k = 0;
	info->audio_linear_pcm._32k = 0;
	info->audio_linear_pcm._24bit = 0;
	info->audio_linear_pcm._20bit = 0;
	info->audio_linear_pcm._16bit = 0;
	info->audio_linear_pcm._other_bit = 0;
	info->audio_ac_3.support_flag = 0;
	info->audio_ac_3.max_channel_num = 0;
	info->audio_ac_3._192k = 0;
	info->audio_ac_3._176k = 0;
	info->audio_ac_3._96k = 0;
	info->audio_ac_3._88k = 0;
	info->audio_ac_3._48k = 0;
	info->audio_ac_3._44k = 0;
	info->audio_ac_3._32k = 0;
	info->audio_ac_3._24bit = 0;
	info->audio_ac_3._20bit = 0;
	info->audio_ac_3._16bit = 0;
	info->audio_ac_3._other_bit = 0;
	info->audio_mpeg1.support_flag = 0;
	info->audio_mpeg1.max_channel_num = 0;
	info->audio_mpeg1._192k = 0;
	info->audio_mpeg1._176k = 0;
	info->audio_mpeg1._96k = 0;
	info->audio_mpeg1._88k = 0;
	info->audio_mpeg1._48k = 0;
	info->audio_mpeg1._44k = 0;
	info->audio_mpeg1._32k = 0;
	info->audio_mpeg1._24bit = 0;
	info->audio_mpeg1._20bit = 0;
	info->audio_mpeg1._16bit = 0;
	info->audio_mpeg1._other_bit = 0;
	info->audio_mp3.support_flag = 0;
	info->audio_mp3.max_channel_num = 0;
	info->audio_mp3._192k = 0;
	info->audio_mp3._176k = 0;
	info->audio_mp3._96k = 0;
	info->audio_mp3._88k = 0;
	info->audio_mp3._48k = 0;
	info->audio_mp3._44k = 0;
	info->audio_mp3._32k = 0;
	info->audio_mp3._24bit = 0;
	info->audio_mp3._20bit = 0;
	info->audio_mp3._16bit = 0;
	info->audio_mp3._other_bit = 0;
	info->audio_mpeg2.support_flag = 0;
	info->audio_mpeg2.max_channel_num = 0;
	info->audio_mpeg2._192k = 0;
	info->audio_mpeg2._176k = 0;
	info->audio_mpeg2._96k = 0;
	info->audio_mpeg2._88k = 0;
	info->audio_mpeg2._48k = 0;
	info->audio_mpeg2._44k = 0;
	info->audio_mpeg2._32k = 0;
	info->audio_mpeg2._24bit = 0;
	info->audio_mpeg2._20bit = 0;
	info->audio_mpeg2._16bit = 0;
	info->audio_mpeg2._other_bit = 0;
	info->audio_aac.support_flag = 0;
	info->audio_aac.max_channel_num = 0;
	info->audio_aac._192k = 0;
	info->audio_aac._176k = 0;
	info->audio_aac._96k = 0;
	info->audio_aac._88k = 0;
	info->audio_aac._48k = 0;
	info->audio_aac._44k = 0;
	info->audio_aac._32k = 0;
	info->audio_aac._24bit = 0;
	info->audio_aac._20bit = 0;
	info->audio_aac._16bit = 0;
	info->audio_aac._other_bit = 0;
	info->audio_dts.support_flag = 0;
	info->audio_dts.max_channel_num = 0;
	info->audio_dts._192k = 0;
	info->audio_dts._176k = 0;
	info->audio_dts._96k = 0;
	info->audio_dts._88k = 0;
	info->audio_dts._48k = 0;
	info->audio_dts._44k = 0;
	info->audio_dts._32k = 0;
	info->audio_dts._24bit = 0;
	info->audio_dts._20bit = 0;
	info->audio_dts._16bit = 0;
	info->audio_dts._other_bit = 0;
	info->audio_atrac.support_flag = 0;
	info->audio_atrac.max_channel_num = 0;
	info->audio_atrac._192k = 0;
	info->audio_atrac._176k = 0;
	info->audio_atrac._96k = 0;
	info->audio_atrac._88k = 0;
	info->audio_atrac._48k = 0;
	info->audio_atrac._44k = 0;
	info->audio_atrac._32k = 0;
	info->audio_atrac._24bit = 0;
	info->audio_atrac._20bit = 0;
	info->audio_atrac._16bit = 0;
	info->audio_atrac._other_bit = 0;
	info->speaker_allocation.rlc_rrc = 0;
	info->speaker_allocation.flc_frc = 0;
	info->speaker_allocation.rc = 0;
	info->speaker_allocation.rl_rr = 0;
	info->speaker_allocation.fc = 0;
	info->speaker_allocation.lfe = 0;
	info->speaker_allocation.fl_fr = 0;
} void hdmi_init_para(struct hdmi_priv *priv) 
{
	Hdmi_info_para_t * info = &priv->hinfo;
	info->priv = priv;
	RESET_HDMI_PARAS_TO_UPLPUG(info);
	info->irq_flag = -1;
	info->video_mode_change_flag = 0;
	info->tv_mode = HDMI_720P_60Hz;
	info->tv_ratio = HDMI_4_3;
	info->audio_flag = 1;	//Bit 0:      1 - enable hdmi audio output;  0 - display hdmi audio out
	info->audiopath_format = 1;	//bit[3:0]: 0 - SPDIF; 1 - I2S; 2 - DSD; 3 - HBR; 
	//bit[7:4]: 0 - PCM format; 1 - AC3 format; 2 - MPEG1 (Layers 1 & 2); 3 - MP3 (MPEG1 Layer 3) 
	//         4 - MPEG2 (multichannel); 5 - AAC; 6 - DTS; 7 - ATRAC  
	info->audiopath_sf = 2;	//Sampling Freq Fs:0 - Refer to Stream Header; 1 - 32KHz; 2 - 44KHz; 3 - 48KHz; 4 - 88KHz...
	info->audiopath_sl = 0x01;	//bit[3:0]: Sample length : 1, 2.0, 3.0, 4.0, 4.1, 5.1, 6.1, 7.1, 8, 0xA, 0xC +1 for Max 24. Even values are Max 20. Odd: Max 24.
	//bit[7:4]: 16bit/18bit/20bit/22bit/24bit Sample Size
	info->audiopath_i2s_in_ctrl = 0;	//I2S control bits
} unsigned If_Support_video(Hdmi_info_para_t * info) 
{
	char suppport_flag = 0;
	info->video_480i.support_format = 1;
	switch (info->tv_mode)
		 {
	case HDMI_480I_60HZ:
		if (info->video_480i.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_576I_50HZ:
		if (info->video_576i.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_480P_60HZ:
		if (info->video_480p.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_576P_50HZ:
		if (info->video_576p.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_720P_60Hz:
		if (info->video_720p_60hz.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_720P_50Hz:
		if (info->video_720p_50hz.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_1080I_60Hz:
		if (info->video_1080i_60hz.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_1080I_50Hz:
		if (info->video_1080i_50hz.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_1080P_60Hz:
		if (info->video_1080p_60hz.support_format == 1)
			suppport_flag = 1;
		break;
	case HDMI_1080P_50Hz:
		if (info->video_1080p_50hz.support_format == 1)
			suppport_flag = 1;
		break;
	default:
		break;
		}
	return suppport_flag;
}
int policy_get_hd_freq(void) 
{				//FIXME
	return POLICY_HD_FREQ_50;
}
void check_video_format_hdmi(Hdmi_info_para_t * info, unsigned short tv_mode) 
{
	policy_hd_freq_t hd_freq = policy_get_hd_freq();	/* video timing control */
	if (tv_mode >= HDMI_TV_MODE_MAX)
		 {
		if ((info->video_480p.support_format) == 1)
			tv_mode = DISPCTL_MODE_480P;
		
		else if ((info->video_576p.support_format) == 1)
			tv_mode = DISPCTL_MODE_480P;
		
		else
			tv_mode = DISPCTL_MODE_720P;
		}
	if (tv_mode == DISPCTL_MODE_720P)
		 {
		if (hd_freq == POLICY_HD_FREQ_50)
			 {
			tv_mode = HDMI_720P_50Hz;
			
			    //hdmi_dbg_print("video format: 720P_50Hz\n"); 
			}
		
		else
			 {
			tv_mode = HDMI_720P_60Hz;
			
			    //hdmi_dbg_print("video format: 720P_60Hz\n");
			}
		}
	
	else if (tv_mode == DISPCTL_MODE_1080I)
		 {
		if (hd_freq == POLICY_HD_FREQ_50)
			 {
			tv_mode = HDMI_1080I_50Hz;
			
			    //hdmi_dbg_print("video format: 1080I_50Hz\n");
			}
		
		else
			 {
			tv_mode = HDMI_1080I_60Hz;
			
			    //hdmi_dbg_print("video format: 1080I_60Hz\n");
			}
		}
	
	else if (tv_mode == DISPCTL_MODE_1080P)
		 {
		if (hd_freq == POLICY_HD_FREQ_50)
			 {
			tv_mode = HDMI_1080P_50Hz;
			
			    //hdmi_dbg_print("video format: 1080P_50Hz\n");
			}
		
		else
			 {
			tv_mode = HDMI_1080P_60Hz;
			
			    //hdmi_dbg_print("video format: 1080P_60Hz\n");
			}
		}
	
	else if (tv_mode == DISPCTL_MODE_480P)
		 {
		
		    //hdmi_dbg_print("video format: 480P\n");
		}
	
	else if (tv_mode == DISPCTL_MODE_576P)
		 {
		
		    //hdmi_dbg_print("video format: 576P\n");
		}
	
	else if (tv_mode == DISPCTL_MODE_480I)
		 {
		
		    //hdmi_dbg_print("video format: 480I\n");
		}
	
	else if (tv_mode == DISPCTL_MODE_576I)
		 {
		
		    //hdmi_dbg_prinT("video format: 576\n");
		}
	if (info->tv_mode != tv_mode)
		 {
		info->tv_mode = tv_mode;
		info->video_mode_change_flag = 1;
		}
	
	else
		 {
		info->video_mode_change_flag = 0;
		}
	if ((tv_mode == DISPCTL_MODE_480I) || (tv_mode == DISPCTL_MODE_576I))
		 {
		info->repeat_pixel_flag = 1;	// pixel repeat 1 time
		//#if (defined AML_APOLLO) 
		info->videopath_inindex &= 0xf8;
		info->videopath_inindex |= 4;	//YUV422:
		//#endif   
		}
	
	else
		 {
		info->repeat_pixel_flag = 0;	// no pixel repeat
		//#if (defined AML_APOLLO)   
		info->videopath_inindex &= 0xf8;
		info->videopath_inindex |= 2;	//YUV444:
		//#endif
		}
}
void reset_hdmi_device(int level) 
{
} 
