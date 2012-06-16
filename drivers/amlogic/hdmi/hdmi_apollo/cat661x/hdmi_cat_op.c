
#include <linux/kthread.h>
    
#include "hdmi_cat_defstx.h"
#include "hdmi_i2c.h"
#include "hdmi_cat_interrupt.h"
#include "hdmi_cat_mddc.h"
#include "hdmi_cat_audio.h"
#include "hdmi_cat_video.h"
#include "hdmi_cat_edid.h"
#include "hdmi_cat_hdcp.h"
#include "hdmi_cat_info_set.h"
    
#include "hdmi_debug.h"
#include "hdmi_module.h"
    
#include "hdmi_video.h"
    
#define HDMI_CHIP_HANDLE_MAGIC	0x48444D49	//ASCII of "HDMI" 
    
//extern gpio_hw_info_t hdmi_gpio_hw_info;
extern void RESET_HDMI_PARAS_TO_UPLPUG(Hdmi_info_para_t * info);
extern void check_video_format_hdmi(Hdmi_info_para_t * info,
				     unsigned char tv_mode);
extern void reset_hdmi_device(int level);
extern unsigned If_Support_video(Hdmi_info_para_t * info);
int hdmi_cat6611_SetAVmute(Hdmi_info_para_t * info) 
{
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	WriteByteHDMITX_CAT(REG_AV_MUTE, B_SET_AVMUTE);
	WriteByteHDMITX_CAT(REG_PKT_GENERAL_CTRL, B_ENABLE_PKT | B_REPEAT_PKT);	//enable general control packet sending the AVMute to HDMI receiver
	return 0;
}
int hdmi_cat6611_ClearAVmute(Hdmi_info_para_t * info) 
{
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	WriteByteHDMITX_CAT(REG_AV_MUTE, B_CLR_AVMUTE);
	WriteByteHDMITX_CAT(REG_PKT_GENERAL_CTRL, B_ENABLE_PKT | B_REPEAT_PKT);	//enable PktGeneral packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	return 0;
}


//-------------------------------------------------------------------
static int hdmi_cat6611_SW_ResetHDMITX(void)	//it don't modify the register you can write, reset some FIFO
{
	WriteByteHDMITX_CAT(REG_SW_RST,
			     B_REF_RST | B_AREF_RST | B_VID_RST | B_AUD_RST |
			     B_HDCP_RST);
	AVTimeDly(1);
	WriteByteHDMITX_CAT(REG_SW_RST, B_AREF_RST | B_VID_RST | B_AUD_RST | B_HDCP_RST);	//The default value of chip power on state is 0x1C.
	
	    //Reset signal for HDMI_TX_DRV. '1': all flip-flops in the transmitter, including those in the BIST pattern generator, are reset. 
	    WriteByteHDMITX_CAT(REG_AFE_DRV_CTRL, B_AFE_DRV_RST);	// Avoid power loading in un play status.
	return 0;
}


//---------------------------------------------------------------------------
static void hdmi_cat6611_HW_Reset_HDMI(Hdmi_info_para_t * info) 
{
	reset_hdmi_device(0);
	AVTimeDly(32);
	hdmi_dbg_print("\n hardware reset HDMI TX\n");
	reset_hdmi_device(1);	// output high level as GPIO 
	AVTimeDly(32);
} static int hdmi_cat6611_SetAudioPara(Hdmi_info_para_t * info) 
{
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0    
	if ((info->output_state == CABLE_PLUGIN_HDMI_OUT)
	     && (info->audio_flag & 0x1))
		 {
		
		    //The valid bit for channel 2/1 is defined in reg191[2], Valid bit should be set to ¡®1¡¯ if the corresponding audio channel maps to the NLPCM audio source.
		    CAT_SetNonPCMAudio(info);	// For LPCM audio. If AC3 or other compressed audio, please set the parameter as 1
		CAT_EnableAudioOutput(info);
		CAT_ConfigAudioInfoFrm(info);
		
//       hdmi_cat6611_ClearAVmute(info);  
//       WriteByteHDMITX_CAT(REG_SW_RST, ~(B_AUD_RST|B_AREF_RST)); //enable audio(bit4==0: Software Audio clock base signal reset, bit2: Audio FIFO reset.)
		}
	
	else
		 {
		CAT_DisableAudioOutput();
		}
	return 0;
}


//unsigned audio_fifo_overflow_count = 0;
//unsigned audio_cts_error_count = 0;
//unsigned auth_fail_overflow_count = 0;
static void check_cat6611_interrupt_status(Hdmi_info_para_t * info) 
{
	int hpd_status_new;
	unsigned char sysstat = 0;
	unsigned char interrupt_flag[3] = { 0, 0, 0 }, interrupt_mask_[3] = {
	0, 0, 0};
	sysstat = ReadByteHDMITX_CAT(REG_SYS_STATUS);
	hpd_status_new =
	    ((sysstat & (B_HPDETECT | B_RXSENDETECT)) ==
	     (B_HPDETECT | B_RXSENDETECT)) ? 1 : 0;
	if (hpd_status_new != info->hpd_state)
		 {
		info->hpd_change_flag = 1;
		info->hpd_state = hpd_status_new;
		}
	ReadBlockHDMITX_CAT(REG_INT_STAT1, 3, interrupt_flag);
	if (sysstat & B_INT_ACTIVE)
		 {
		
//         hdmi_dbg_print("there is some INT .\n");
		    if (interrupt_flag[0] & (B_INT_HPD_PLUG | B_INT_RX_SENSE))
			 {
			info->hpd_change_flag = 1;
			}
		if (interrupt_flag[0] & B_INT_AUD_OVERFLOW)
			 {
			info->audio_fifo_overflow = 1;
			
//                                       audio_fifo_overflow_count++;
			}
		if (interrupt_flag[2] & B_INT_AUD_CTS)
			 {
			info->audio_cts_status_err_flag = 1;
			
//                                       audio_cts_error_count++;
			}
		if (interrupt_flag[0] & B_INT_DDCFIFO_ERR)
			 {
			
//                       hdmi_dbg_print("DDC FIFO Error.\n");
			    CAT_ClearDDCFIFO();
			}
		if (interrupt_flag[0] & B_INT_DDC_BUS_HANG)
			 {
			
//                       hdmi_dbg_print("DDC BUS HANG.\n") ;
			    CAT_AbortDDC();
			if ((info->auto_ri_flag == 1)
			     && (info->output_state == CABLE_PLUGIN_HDMI_OUT))
				CAT_HDCP_ResumeAuthentication(info);
			}
		if (interrupt_flag[1] & B_INT_AUTH_DONE)
			 {
			
//            hdmi_dbg_print("interrupt Authenticate Done.\n") ; 
			    hdmi_cat6611_ClearAVmute(info);
			info->auth_state = HDCP_AUTHENTICATED;
			interrupt_mask_[1] = B_AUTH_DONE_MASK;
			CAT_Disable_Interrupt_Masks(interrupt_mask_);
			}
		if (interrupt_flag[1] & B_INT_AUTH_FAIL)
			 {
			
//            hdmi_dbg_print("interrupt Authenticate Fail.\n") ;        
			    CAT_AbortDDC();	//del by xintan
//            WriteByteHDMITX_CAT(REG_SW_RST, (ReadByteHDMITX_CAT(REG_SW_RST))& (~B_CPDESIRE));
//            interrupt_mask_[1] = B_AUTH_DONE_MASK; 
			if ((info->auto_ri_flag == 1)
			    && (info->output_state == CABLE_PLUGIN_HDMI_OUT))
				 {
				CAT_Enable_Interrupt_Masks(interrupt_mask_);
				info->auth_state = HDCP_REAUTHENTATION_REQ;
				}
			
			else
				 {
				info->auth_state = HDCP_NO_AUTH;
				}
			}
		if (interrupt_flag[2] & B_INT_VIDSTABLE)
			 {
			if (sysstat & B_TXVIDSTABLE)
				 {
				CAT_FireAFE();
				}
			}
		interrupt_flag[0] = 0xff;
		interrupt_flag[1] = 0xff;
		WriteBlockHDMITX_CAT(REG_INT_CLR0, 2, interrupt_flag);	//Write 1 to clear the interrupt flag.           
		sysstat = ReadByteHDMITX_CAT(REG_SYS_STATUS);
		sysstat |= B_CLR_AUD_CTS | B_INTACTDONE;
		WriteByteHDMITX_CAT(REG_SYS_STATUS, sysstat);	// clear interrupt.
		sysstat &= ~(B_INTACTDONE | B_CLR_AUD_CTS);
		WriteByteHDMITX_CAT(REG_SYS_STATUS, sysstat);	// INTACTDONE reset to zero.             
		}
	
	else
		 {
		if ((info->hpd_change_flag == 1) && (info->hpd_state == 0))	////Unplug
		{
			WriteByteHDMITX_CAT(REG_SW_RST,
					     B_AREF_RST | B_VID_RST | B_AUD_RST
					     | B_HDCP_RST);
			AVTimeDly(1);
			WriteByteHDMITX_CAT(REG_AFE_DRV_CTRL,
					     B_AFE_DRV_RST | B_AFE_DRV_PWD);
		}
		interrupt_flag[0] = 0xff;
		interrupt_flag[1] = 0xff;
		WriteBlockHDMITX_CAT(REG_INT_CLR0, 2, interrupt_flag);	//Write 1 to clear the interrupt flag.                                  
		}
}
static int hdmi_cat6611_IsHDMICompatible(Hdmi_info_para_t * info) 
{
	unsigned char sysstat;
	
//       unsigned char interrupt_flag[3] = {0, 0, 0};
	unsigned char intdata[3] = { 0, 0, 0 };
	sysstat = ReadByteHDMITX_CAT(REG_SYS_STATUS);
	info->hpd_state = ((sysstat & (B_HPDETECT | B_RXSENDETECT)) == (B_HPDETECT | B_RXSENDETECT)) ? 1 : 0;	//0: no hdmi device connected,  0x1: hdmi device connected
	ReadBlockHDMITX_CAT(REG_INT_STAT1, 3, intdata);
	
	    //hdmi_dbg_print("INT_Handler: reg%02x = %02x\n", REG_INT_STAT1, sysstat) ;
	    //hdmi_dbg_print("info->hpd_state= %02x\n", info->hpd_state) ;
	    if (sysstat & B_INT_ACTIVE)
		 {
		if (intdata[0] & B_INT_DDCFIFO_ERR)
			 {
			HDMI_DEBUG("DDC FIFO Error.\n");
			CAT_ClearDDCFIFO();
			}
		if (intdata[0] & B_INT_DDC_BUS_HANG)
			 {
			HDMI_DEBUG("DDC BUS HANG.\n");
			CAT_AbortDDC();
			
//        if( info->hpd_state )
//         {
//             // when DDC hang, and aborted DDC, the HDCP authentication need to restart.
//             CAT_HDCP_ResumeAuthentication(info) ;
//          }
			}
		if (intdata[0] & (B_INT_HPD_PLUG | B_INT_RX_SENSE))
			 {
			info->hpd_change_flag = 1;
			if (info->hpd_state == 0)	//Unplug
			{
				WriteByteHDMITX_CAT(REG_SW_RST,
						     (ReadByteHDMITX_CAT
						      (REG_SW_RST)) | B_AREF_RST
						     | B_VID_RST | B_AUD_RST |
						     B_HDCP_RST);
				AVTimeDly(1);
				
				    //bit4: all flip-flops in the transmitter, including those in the BIST pattern generator, are reset.
				    //bit5: all flip-flops in the transmitter are reset while all other analog parts are powered off.
				    WriteByteHDMITX_CAT(REG_AFE_DRV_CTRL, B_AFE_DRV_RST | B_AFE_DRV_PWD);	//Reset signal for HDMI_TX_DRV.
//             printf("Unplug, %x %x\n", ReadByteHDMITX_CAT(REG_SW_RST), ReadByteHDMITX_CAT(REG_AFE_DRV_CTRL)) ;
			}
			
			else	//hotplug
			{
			}
			}
		if (intdata[2] & B_INT_VIDSTABLE)
			 {
			if (sysstat & B_TXVIDSTABLE)
				 {
				CAT_FireAFE();
				}
			}
		intdata[0] = 0xff;
		intdata[1] = 0xff;
		WriteBlockHDMITX_CAT(REG_INT_CLR0, 2, intdata);	//Write 1 to clear the interrupt flag.
		sysstat = ReadByteHDMITX_CAT(REG_SYS_STATUS);
		sysstat |= B_CLR_AUD_CTS | B_INTACTDONE;
		WriteByteHDMITX_CAT(REG_SYS_STATUS, sysstat);	// clear interrupt.
		sysstat &= ~(B_INTACTDONE | B_CLR_AUD_CTS);
		WriteByteHDMITX_CAT(REG_SYS_STATUS, sysstat);	// INTACTDONE reset to zero.                               
		}
	
	else
		 {
		if (intdata[0] & (B_INT_HPD_PLUG | B_INT_RX_SENSE))
			info->hpd_change_flag = 1;
		if ((info->hpd_change_flag == 1) && (info->hpd_state == 0))	////Unplug
		{
			WriteByteHDMITX_CAT(REG_SW_RST,
					     B_AREF_RST | B_VID_RST | B_AUD_RST
					     | B_HDCP_RST);
			AVTimeDly(1);
			WriteByteHDMITX_CAT(REG_AFE_DRV_CTRL,
					     B_AFE_DRV_RST | B_AFE_DRV_PWD);
			
//            printf("Unplug, %x %x\n", ReadByteHDMITX_CAT(REG_SW_RST), ReadByteHDMITX_CAT(REG_AFE_DRV_CTRL)) ;
		}
		}
	return 0;
}
int hdmi_cat6611_RD_Edid(Hdmi_info_para_t * info) 
{
	hdmi_cat6611_IsHDMICompatible(info);
	if (info->hpd_state == 1)
		 {
		if (CAT_ParseEDID(info) == -1)
			 {
			info->auth_state = HDCP_NO_AUTH;
			}
		return 0;
		}
	
	else
		return -1;
}
int hdmi_cat6611_SetVideoPara(Hdmi_info_para_t * info) 
{
	int err;
	if ((info->videopath_inindex & 0x7) == 4)	//YUV422:
		err = CAT_ProgramSyncEmbeddedVideoMode(info);	// if CCIR656 input
	if ((info->output_state == CABLE_PLUGIN_HDMI_OUT)
	    || (info->output_state == CABLE_PLUGIN_DVI_OUT))
		 {
		CAT_EnableVideoOutput(info);
		CAT_ConfigAVIInfoFrame(info);
		AVTimeDly(100);
		
//      hdmi_cat6611_ClearAVmute(info);
		    err = 0;
		}
	
	else
		 {
		CAT_DisableVideoOutput();
		CAT_SetHDMIMode(info);
		err = -1;
		}
	return err;
}


#define PRINTF_REG_MSN
#ifdef PRINTF_REG_MSN
unsigned char register_msg_flag = 0;
unsigned char cat6611_reg_data[384];;
static void CAT_DumpCat6611Reg(void) 
{
	int i, j;
	unsigned char ucData;
	for (i = 0; i < 384; i++)
		 {
		cat6611_reg_data[i] = 0;
		}
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);
	for (i = 0; i < 0x100; i += 16)
		 {
		for (j = 0; j < 16; j++)
			 {
			ucData =
			    ReadByteHDMITX_CAT((unsigned char)((i + j) & 0xFF));
			cat6611_reg_data[i + j] = ucData;
		} } CAT_Switch_HDMITX_Bank(TX_SLV0, 1);
	for (i = 0x130; i < 0x1B0; i += 16)
		 {
		for (j = 0; j < 16; j++)
			 {
			ucData =
			    ReadByteHDMITX_CAT((unsigned char)((i + j) & 0xFF));
			cat6611_reg_data[i + j - 0x30] = ucData;
		} } CAT_Switch_HDMITX_Bank(TX_SLV0, 0);
} 
#endif	/*  */
    
//---------------------------------------------------------------------------
void hdmi_cat6611_InitUNPLUG(Hdmi_info_para_t * info) 
{
	unsigned char abData[3];
	hdmi_cat6611_HW_Reset_HDMI(info);
	AVTimeDly(1);
	hdmi_cat6611_SW_ResetHDMITX();
	AVTimeDly(1);
	RESET_HDMI_PARAS_TO_UPLPUG(info);
	check_video_format_hdmi(info, hdmi_get_tv_mode(info));
	hdmi_cat6611_SetAVmute(info);
	hdmi_cat6611_SetVideoPara(info);
	hdmi_cat6611_SetAudioPara(info);
	hdmi_cat6611_ClearAVmute(info);
	CAT_EnableHDCP(info, 0);
	info->init_hdmi_flag = 1;
	
	    //1: disable this interrupt; 0: Enable this interrupt
	    abData[0] =
	    B_AUDIO_OVFLW_MASK | B_DDC_NOACK_MASK | B_DDC_NOACK_MASK |
	    B_DDC_BUS_HANG_MASK;
	abData[1] =
	    B_PKT_AVI_MASK | B_PKT_ISRC_MASK | B_PKT_ACP_MASK | B_PKT_NULL_MASK
	    | B_PKT_GEN_MASK | B_KSVLISTCHK_MASK | B_AUTH_DONE_MASK |
	    B_AUTH_FAIL_MASK;
	abData[2] =
	    B_AUDCTS_MASK | B_VSYNC_MASK | B_VIDSTABLE_MASK | B_PKT_MPG_MASK |
	    B_PKT_SPD_MASK | B_PKT_AUD_MASK;
	CAT_Set_Interrupt_Masks(abData);
	hdmi_dbg_print("(InitDVITX)is Done\n");
	
//   printf("(InitDVITX)is Done\n");           
} static int hdmi_cat6611_InitHDMITX(Hdmi_info_para_t * info) 
{
	unsigned char temp_data[4];
	unsigned char abData[3];
	info->txinit_flag = 1;	//indicate the system is in the initialization state
	hdmi_dbg_print("(InitHDMITX)is starting\n");
	check_video_format_hdmi(info, hdmi_get_tv_mode(info));
	info->video_mode_change_flag = 0;
	hdmi_cat6611_HW_Reset_HDMI(info);
	AVTimeDly(10);
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	ReadBlockHDMITX_CAT(REG_VENDOR_ID0, 4, temp_data);
	
//    if(hdmi_gpio_hw_info.interrupt_pol)
//      WriteByteHDMITX_CAT(REG_INT_CTRL, B_INTPOL_ACTL | B_INTPOL_ACTH) ; // depends on the interrupt type: higth acitve 
//    else
//      WriteByteHDMITX_CAT(REG_INT_CTRL, 0) ; // depends on the interrupt type: low acitve      
	    hdmi_cat6611_SW_ResetHDMITX();
	AVTimeDly(1);
	
//    id_device = ReadWordHDMITX_CAT(REG_VENDOR_ID0); 
//    id_device = ReadWordHDMITX_CAT(REG_DEVICE_ID0);
	    WriteByteHDMITX_CAT(REG_NULL_CTRL, 0);	//disable PktNull packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	WriteByteHDMITX_CAT(REG_ACP_CTRL, 0);	//disable PktACP packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	WriteByteHDMITX_CAT(REG_ISRC1_CTRL, 0);	//disable PktISRC1 packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	WriteByteHDMITX_CAT(REG_ISRC2_CTRL, 0);	//disable PktISRC2 packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	WriteByteHDMITX_CAT(REG_AVI_INFOFRM_CTRL, 0);	//disable PktAVI packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	WriteByteHDMITX_CAT(REG_AUD_INFOFRM_CTRL, 0);	//disable PktAUO packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	WriteByteHDMITX_CAT(REG_SPD_INFOFRM_CTRL, 0);	//disable PktSPD packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	WriteByteHDMITX_CAT(REG_MPG_INFOFRM_CTRL, 0);	//disable PktMPG packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
	AVTimeDly(200);
	hdmi_cat6611_IsHDMICompatible(info);
	hdmi_dbg_print("info->hpd_state=%d\n", info->hpd_state);
	if (info->hpd_state == 1)
		 {
		if (CAT_ParseEDID(info) == -1)
			 {
			info->output_state = CABLE_PLUGIN_DVI_OUT;
			HDMI_DEBUG("CAT_ParseEDID error\n");
			info->auth_state = HDCP_NO_AUTH;
			
//            info->output_state = CABLE_PLUGIN_DVI_OUT ;
			}
		
//        if(info->support_ycbcr444_flag == 1)
//               info->videopath_outindex = 1;    // Video Output Color Space Conversion : 0 - RGB out; 1 - YCbr (4:4:4) out; 2 - YCbr (4:2:2) out;   
		    hdmi_cat6611_SetAVmute(info);
		hdmi_cat6611_SetVideoPara(info);
		if (info->output_state == CABLE_PLUGIN_HDMI_OUT)
			CAT_EnableHDCP(info, info->auto_ri_flag);
		
		else
			 {
			CAT_EnableHDCP(info, 0);
			}
		hdmi_cat6611_SetAudioPara(info);
		hdmi_cat6611_ClearAVmute(info);
		
		    //1: disable this interrupt; 0: Enable this interrupt
		    abData[0] = B_DDC_NOACK_MASK | B_DDC_FIFO_ERR_MASK;
		if ((info->auto_ri_flag == 1)
		     && (info->output_state == CABLE_PLUGIN_HDMI_OUT))
			abData[1] =
			    B_PKT_AVI_MASK | B_PKT_ISRC_MASK | B_PKT_ACP_MASK |
			    B_PKT_NULL_MASK | B_PKT_GEN_MASK;
		
		else
			abData[1] =
			    B_PKT_AVI_MASK | B_PKT_ISRC_MASK | B_PKT_ACP_MASK |
			    B_PKT_NULL_MASK | B_PKT_GEN_MASK | B_AUTH_FAIL_MASK
			    | B_AUTH_DONE_MASK;
		
//      abData[2] = B_AUDCTS_MASK | B_VSYNC_MASK | B_PKT_MPG_MASK | B_PKT_SPD_MASK | B_PKT_AUD_MASK; 
		    abData[2] =
		    B_VSYNC_MASK | B_PKT_MPG_MASK | B_PKT_SPD_MASK |
		    B_PKT_AUD_MASK;
		CAT_Set_Interrupt_Masks(abData);
		hdmi_dbg_print("(InitHDMITX)is Done\n");
		
#ifdef PRINTF_REG_MSN    
		    CAT_DumpCat6611Reg();
		
#endif	/*  */
		    info->init_hdmi_flag = 0;
		}
	
	else			// unplug mode, ...
	{
		hdmi_cat6611_InitUNPLUG(info);
	}
	info->txinit_flag = 0;	// indicate the hdmi initialization is done 
	return 0;
}
int detecte_hdmi_phy(void) 
{
	
	    //FIXME:whic register should good for detect the hdmi phy?
	int vid, did;
	vid = ReadByteHDMITX_CAT(REG_VENDOR_ID0);
	vid |= ReadByteHDMITX_CAT(REG_VENDOR_ID1) << 8;
	did = ReadByteHDMITX_CAT(REG_DEVICE_ID0);
	did |= ReadByteHDMITX_CAT(REG_DEVICE_ID1) << 8;
	printk("vid=%x,did=%x\n", vid, did);
	
	    //Here should be the true device id---
	    return (vid == 0xffff && did == 0xffff) ? -1 : 0;
}


//unsigned audio_fifo_overflow_count = 0;
//unsigned audio_cts_error_count = 0;
//unsigned auth_fail_overflow_count = 0;
extern unsigned char CAT6611_bPendingAudioAdjust;
static int hdmi_cat6611_task_handle(void *data) 
{
    Hdmi_info_para_t * info = data;
    struct hdmi_priv *priv = info->priv;
    unsigned char sysstat;
    unsigned char interrupt_mask[3];
    unsigned char err;

#ifdef PRINTF_REG_MSN
    unsigned char temp_test[8];
    unsigned char test_Addr;
    unsigned short test_NBytes;
#endif/*  */

    //unsigned char audio_temp[12];  
    int video_fd;

    // video_appmode_t video_appmode;
    //if(hdmi_gpio_hw_info.interrupt_flag)
    //   AVEnableIrq(info->irq_flag); 
    video_fd = 0;
    err = 0;
    while (priv->i2c == NULL)
    {
        priv->i2c = hdmi_i2c_init_adapter();
        if (kthread_should_stop())
            return 0;
        msleep(1000);   //waiting utill i2c ready;
    }
    
    while (detecte_hdmi_phy() != 0)
    {
        HDMI_ERR("Can't detected a cat661x device,connect error or no device!\n");
        hdmi_i2c_release_adapter(priv->i2c);
        priv->i2c = NULL;
        kthread_should_stop();
        priv->task = NULL;
        return 0;
    }
    
    hdmi_cat6611_InitHDMITX(&priv->hinfo);
    msleep(10);
    while (!kthread_should_stop())
    {
        sysstat = 0;

#ifdef PRINTF_REG_MSN   
        temp_test[0] = 0;
        temp_test[1] = 0;
        temp_test[2] = 0;
        temp_test[3] = 0;
        temp_test[4] = 0;
        temp_test[5] = 0;
        temp_test[6] = 0;
        temp_test[7] = 0;
#endif  /*  */

        check_cat6611_interrupt_status(info);
        if (info->hpd_state == 1)
        {
            check_video_format_hdmi(info, hdmi_get_tv_mode(info));
        }

        // hdmi_dbg_print("info->hpd_change_flag=%d,in[%d]\n",info->hpd_change_flag,__LINE__);  
        if ((info->hpd_change_flag == 1))
        {
            info->hpd_change_flag = 0;
            hdmi_dbg_print("video format changed.\n");
            sysstat = ReadByteHDMITX_CAT(REG_SYS_STATUS);
            
            if ((sysstat & (B_HPDETECT | B_RXSENDETECT)) 
                == (B_HPDETECT | B_RXSENDETECT))
            {
                if (info->init_hdmi_flag == 1)
                {
                    hdmi_cat6611_InitHDMITX(info);
                }
            }
            else
            {
                hdmi_cat6611_InitUNPLUG(info);
            }
        }
        
        if ((info->auto_ri_flag == 1)
            && (info->auth_state != HDCP_AUTHENTICATED)
            && (info->hpd_state == 1))
        {
            //auth_fail_overflow_count++;
            if ((info->auto_ri_flag == 1)
                && (info->output_state == CABLE_PLUGIN_HDMI_OUT))
                CAT_HDCP_ResumeAuthentication(info);
        }
        
        if ((info->video_mode_change_flag == 1))
        {
            WriteByteHDMITX_CAT(REG_SW_RST,
                B_REF_RST | B_AREF_RST | B_VID_RST | B_AUD_RST | B_HDCP_RST);
            AVTimeDly(1);
            WriteByteHDMITX_CAT(REG_SW_RST, B_AREF_RST | B_VID_RST | B_AUD_RST | B_HDCP_RST);	//The default value of chip power on state is 0x1C.                
            AVTimeDly(200);
            info->video_mode_change_flag = 0;

            if (info->hpd_state == 1)
            {
                hdmi_cat6611_SetAVmute(info);
                if ((info->tv_mode == HDMI_480I_60HZ)
                    || (info->tv_mode == HDMI_576I_50HZ))
                {
                    info->repeat_pixel_flag = 1;	// pixel repeat 1 time   
                }
                else
                {
                    info->repeat_pixel_flag = 0;	// no pixel repeat                                           
                }
                hdmi_cat6611_SetVideoPara(info);
                AVTimeDly(1);

                if (info->output_state == CABLE_PLUGIN_HDMI_OUT)
                {
                    CAT_EnableHDCP(info, info->auto_ri_flag);
                }
                else
                {
                    CAT_EnableHDCP(info, 0);
                }
                AVTimeDly(1);
                hdmi_cat6611_SetAudioPara(info);
                hdmi_cat6611_ClearAVmute(info);
                AVTimeDly(1);

                // 1: disable this interrupt; 0: Enable this interrupt
                interrupt_mask[0] = B_DDC_NOACK_MASK | B_DDC_FIFO_ERR_MASK;
                if ((info->auto_ri_flag == 1)
                    && (info->output_state == CABLE_PLUGIN_HDMI_OUT))
                {
                    interrupt_mask[1] =
                        B_PKT_AVI_MASK | B_PKT_ISRC_MASK |
                        B_PKT_ACP_MASK | B_PKT_NULL_MASK |
                        B_PKT_GEN_MASK;
                }
                else
                {
                    interrupt_mask[1] =
                        B_PKT_AVI_MASK | B_PKT_ISRC_MASK |
                        B_PKT_ACP_MASK | B_PKT_NULL_MASK |
                        B_PKT_GEN_MASK | B_AUTH_FAIL_MASK |
                        B_AUTH_DONE_MASK;
                }
               interrupt_mask[2] =
                    B_VSYNC_MASK | B_PKT_MPG_MASK |
                    B_PKT_SPD_MASK | B_PKT_AUD_MASK;
                CAT_Set_Interrupt_Masks(interrupt_mask);
            }
        }
        
        if ((info->audio_fifo_overflow == 1)
            || (info->audio_cts_status_err_flag == 1))
        {
            //hdmi_dbg_print("audio error.\n");
            info->audio_fifo_overflow = 0;
            info->audio_cts_status_err_flag = 0;
            CAT6611_bPendingAudioAdjust = 1;
        }
        
        if (info->hpd_state == 1)
        {
            CAT_SetAudioChannel(info);
        }

#ifdef PRINTF_REG_MSN
        if (register_msg_flag == 1)
        {
            test_Addr = 0;
            test_NBytes = 1;
            ReadBlockHDMITX_CAT(test_Addr, test_NBytes, temp_test);
            WriteBlockHDMITX_CAT(test_Addr, test_NBytes, temp_test);
        }
        if (register_msg_flag == 2)
        {
            CAT_Switch_HDMITX_Bank(TX_SLV0, 1);
            test_Addr = 0;
            test_NBytes = 1;
            ReadBlockHDMITX_CAT(test_Addr, test_NBytes, temp_test);
            WriteBlockHDMITX_CAT(test_Addr, test_NBytes, temp_test);
            CAT_Switch_HDMITX_Bank(TX_SLV0, 0);
        }
        if (register_msg_flag == 3)
            CAT_DumpCat6611Reg();
#endif/*  */

        AVTimeDly(500);
    }

    return 0;
}


//hdmi_cat6611_task_handle(Hdmi_info_para_t * info)
void start_hdmi_cat661x(struct hdmi_priv *priv) 
{
    priv->task = kthread_run(hdmi_cat6611_task_handle, &priv->hinfo, "kthread_hdmi");
}

void stop_hdmi_cat661x(struct hdmi_priv *priv) 
{
    if (priv->task)
        kthread_stop(priv->task);
}

