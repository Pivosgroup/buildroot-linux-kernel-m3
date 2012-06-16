
#include "hdmi_global.h"
#include "hdmi_cat_defstx.h"
#include "hdmi_vmode.h"
#include "hdmi_i2c.h"
#include "hdmi_cat_video.h"
#include "hdmi_cat_info_set.h"
    
#include "hdmi_debug.h"
static int CAT_SetAVIInfoFrame(AVI_InfoFrame * pAVIInfoFrame) 
{
	int i;
	unsigned char ucData;
	if (!pAVIInfoFrame)
		 {
		return -1;
		}
	CAT_Switch_HDMITX_Bank(TX_SLV0, 1);	//select bank 1
	WriteBlockHDMITX_CAT(REG_AVIINFO_DB1, 5,
			     (unsigned char *)&pAVIInfoFrame->pktbyte.
			     AVI_DB[0]);
	WriteBlockHDMITX_CAT(REG_AVIINFO_DB6, 8,
			      (unsigned char *)&pAVIInfoFrame->pktbyte.
			      AVI_DB[5]);
	for (i = 0, ucData = 0; i < 13; i++)
		 {
		ucData -= pAVIInfoFrame->pktbyte.AVI_DB[i];
		}
	ucData -=
	    0x80 + AVI_INFOFRAME_VER + AVI_INFOFRAME_TYPE + AVI_INFOFRAME_LEN;
	WriteByteHDMITX_CAT(REG_AVIINFO_SUM, ucData);
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	WriteByteHDMITX_CAT(REG_AVI_INFOFRM_CTRL, B_ENABLE_PKT | B_REPEAT_PKT);	// ENABLE_AVI_INFOFRM_PKT();
	return 0;
}
static void CAT_EnableAVIInfoFrame(int bEnable,
				     unsigned char *pAVIInfoFrame) 
{
	
//    if( !bEnable )
//    {
	    WriteByteHDMITX_CAT(REG_AVI_INFOFRM_CTRL, 0);	//disable PktAVI packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
//    }
	if (bEnable)
		 {
		CAT_SetAVIInfoFrame((AVI_InfoFrame *) pAVIInfoFrame);
		}
}
void CAT_ConfigAVIInfoFrame(Hdmi_info_para_t * info) 
{
	AVI_InfoFrame AviInfo;
	unsigned char aspec, Colorimetry, VIC;
	unsigned short top_line_bar_end, buttom_line_bar_start,
	    left_pixel_bar_end, right_pixel_bar_start;
	AviInfo.pktbyte.AVI_HB[0] = AVI_INFOFRAME_TYPE | 0x80;
	AviInfo.pktbyte.AVI_HB[1] = AVI_INFOFRAME_VER;
	AviInfo.pktbyte.AVI_HB[2] = AVI_INFOFRAME_LEN;
	switch (info->tv_mode)
		 {
	case HDMI_480I_60HZ:
		if (info->video_480i.support_16_9 == 1)
			 {
			aspec = HDMI_16x9;
			VIC = 7;
			}
		
		else
			 {
			aspec = HDMI_4x3;
			VIC = 6;
			}
		top_line_bar_end = 21;
		buttom_line_bar_start = top_line_bar_end + 240 + 1;
		left_pixel_bar_end = 124 + 114;
		right_pixel_bar_start = left_pixel_bar_end + 1440;
		Colorimetry = HDMI_ITU601;	//ITU601      
		break;
	case HDMI_576I_50HZ:
		if (info->video_576i.support_16_9 == 1)
			 {
			aspec = HDMI_16x9;
			VIC = 22;
			}
		
		else
			 {
			aspec = HDMI_4x3;
			VIC = 21;
			}
		top_line_bar_end = 22;
		buttom_line_bar_start = top_line_bar_end + 288 + 1;
		left_pixel_bar_end = 126 + 138;
		right_pixel_bar_start = left_pixel_bar_end + 1440;
		Colorimetry = HDMI_ITU601;	//ITU601              
		break;
	case HDMI_480P_60HZ:
		if (info->video_480p.support_16_9 == 1)
			 {
			aspec = HDMI_16x9;
			VIC = 3;
			}
		
		else
			 {
			aspec = HDMI_4x3;
			VIC = 2;
			}
		top_line_bar_end = 42;
		buttom_line_bar_start = top_line_bar_end + 480 + 1;
		left_pixel_bar_end = 62 + 60;
		right_pixel_bar_start = left_pixel_bar_end + 720;
		Colorimetry = HDMI_ITU601;	//ITU601 
		break;
	case HDMI_576P_50HZ:
		if (info->video_576p.support_16_9 == 1)
			 {
			aspec = HDMI_16x9;
			VIC = 18;
			}
		
		else
			 {
			aspec = HDMI_4x3;
			VIC = 17;
			}
		top_line_bar_end = 44;
		buttom_line_bar_start = top_line_bar_end + 576 + 1;
		left_pixel_bar_end = 64 + 68;
		right_pixel_bar_start = left_pixel_bar_end + 720;
		Colorimetry = HDMI_ITU601;	//ITU601 
		break;
	case HDMI_720P_60Hz:
		top_line_bar_end = 25;
		buttom_line_bar_start = top_line_bar_end + 720 + 1;
		left_pixel_bar_end = 40 + 220;
		right_pixel_bar_start = left_pixel_bar_end + 1280;
		aspec = HDMI_16x9;
		Colorimetry = HDMI_ITU709;	//ITU709
		VIC = 4;
		break;
	case HDMI_720P_50Hz:
		top_line_bar_end = 25;
		buttom_line_bar_start = top_line_bar_end + 720 + 1;
		left_pixel_bar_end = 40 + 220;
		right_pixel_bar_start = left_pixel_bar_end + 1280;
		aspec = HDMI_16x9;
		Colorimetry = HDMI_ITU709;	//ITU709
		VIC = 19;
		break;
	case HDMI_1080I_60Hz:
		top_line_bar_end = 20;
		buttom_line_bar_start = top_line_bar_end + 540 + 1;
		left_pixel_bar_end = 44 + 148;
		right_pixel_bar_start = left_pixel_bar_end + 1920;
		aspec = HDMI_16x9;
		Colorimetry = HDMI_ITU709;	//ITU709
		VIC = 5;
		break;
	case HDMI_1080I_50Hz:
		top_line_bar_end = 20;
		buttom_line_bar_start = top_line_bar_end + 540 + 1;
		left_pixel_bar_end = 44 + 148;
		right_pixel_bar_start = left_pixel_bar_end + 1920;
		aspec = HDMI_16x9;
		Colorimetry = HDMI_ITU709;	//ITU709
		VIC = 20;
		break;
	case HDMI_1080P_60Hz:
		top_line_bar_end = 41;
		buttom_line_bar_start = top_line_bar_end + 1080 + 1;
		left_pixel_bar_end = 44 + 148;
		right_pixel_bar_start = left_pixel_bar_end + 1920;
		aspec = HDMI_16x9;
		Colorimetry = HDMI_ITU709;	//ITU709
		VIC = 16;
		break;
	case HDMI_1080P_50Hz:
		top_line_bar_end = 41;
		buttom_line_bar_start = top_line_bar_end + 1080 + 1;
		left_pixel_bar_end = 44 + 148;
		right_pixel_bar_start = left_pixel_bar_end + 1920;
		aspec = HDMI_16x9;
		Colorimetry = HDMI_ITU709;	//ITU709
		VIC = 31;
		break;
	default:
		break;
		}
	switch (info->videopath_outindex)
		 {
	case 1:		//F_MODE_YUV444:
		AviInfo.pktbyte.AVI_DB[0] = (2 << 5);
		break;
	case 2:		//F_MODE_YUV422:
		AviInfo.pktbyte.AVI_DB[0] = (1 << 5);
		break;
	case 0:		//F_MODE_RGB444:
	default:
		AviInfo.pktbyte.AVI_DB[0] = (0 << 5);
		break;
		}
	AviInfo.pktbyte.AVI_DB[0] |= (1 << 4);	//bit4: Active Format Information valid(RO...R3 is active)
	
//If the bar information and the active format information do not agree, then the bar information shall take precedence.    
	    AviInfo.pktbyte.AVI_DB[0] |= (0 << 2);	//bit[3:2]: Vert. and Horiz. Bar Info invalid,  added by chen
	
//A sink should adjust its scan based on S. Such a device would overscan(television) if it received S=1, where some active pixelsand lines at the edges are not displayed.
//and underscan if it received S=2. where all active pixels&lines are displayed, withor without a border.
//If it receives S=O,then it should overscan for a CE format and underscan for an IT format.
// A sink should indicate its overscan/underscan behavior using a Video Capabilities Data Block    
	    AviInfo.pktbyte.AVI_DB[0] &= 0xfc;	//|=0x01;      //Overscanned,  
	AviInfo.pktbyte.AVI_DB[1] = 8;	//Same as picture aspect ratio
	AviInfo.pktbyte.AVI_DB[1] |= (aspec != HDMI_16x9) ? (1 << 4) : (2 << 4);	// 4:3 or 16:9
	AviInfo.pktbyte.AVI_DB[1] |= (Colorimetry != HDMI_ITU709) ? (1 << 6) : (2 << 6);	// 4:3 or 16:9
	AviInfo.pktbyte.AVI_DB[2] = 0;	//note CEA-861D standard
	AviInfo.pktbyte.AVI_DB[3] = VIC;
	AviInfo.pktbyte.AVI_DB[4] = info->repeat_pixel_flag;
	AviInfo.pktbyte.AVI_DB[5] = (unsigned char)(top_line_bar_end & 0xff);
	AviInfo.pktbyte.AVI_DB[6] =
	    (unsigned char)((top_line_bar_end & 0xff00) >> 8);
	AviInfo.pktbyte.AVI_DB[7] =
	    (unsigned char)(buttom_line_bar_start & 0xff);
	AviInfo.pktbyte.AVI_DB[8] =
	    (unsigned char)((buttom_line_bar_start & 0xff00) >> 8);
	AviInfo.pktbyte.AVI_DB[9] = (unsigned char)(left_pixel_bar_end & 0xff);
	AviInfo.pktbyte.AVI_DB[10] =
	    (unsigned char)((left_pixel_bar_end & 0xff00) >> 8);
	AviInfo.pktbyte.AVI_DB[11] =
	    (unsigned char)(right_pixel_bar_start & 0xff);
	AviInfo.pktbyte.AVI_DB[12] =
	    (unsigned char)((right_pixel_bar_start & 0xff00) >> 8);
	CAT_EnableAVIInfoFrame(1, (unsigned char *)&AviInfo);
} 

////////////////////////////////////////////////////////////////////////////////
// Function: SetAudioInfoFrame()
// Parameter: pAudioInfoFrame - the pointer to HDMI Audio Infoframe ucData
// Return: N/A
// Remark: Fill the Audio InfoFrame ucData, and count checksum, then fill into
//         Audio InfoFrame registers.
// Side-Effect: N/A
////////////////////////////////////////////////////////////////////////////////
static int CAT_SetAudioInfoFrame(Audio_InfoFrame * pAudioInfoFrame) 
{
	int i;
	unsigned char ucData;
	if (!pAudioInfoFrame)
		 {
		return -1;
		}
	CAT_Switch_HDMITX_Bank(TX_SLV0, 1);
	WriteByteHDMITX_CAT(REG_PKT_AUDINFO_CC,
			     pAudioInfoFrame->pktbyte.AUD_DB[0]);
	WriteByteHDMITX_CAT(REG_PKT_AUDINFO_CA,
			     pAudioInfoFrame->pktbyte.AUD_DB[1]);
	WriteByteHDMITX_CAT(REG_PKT_AUDINFO_DM_LSV,
			     pAudioInfoFrame->pktbyte.AUD_DB[4]);
	for (i = 0, ucData = 0; i < 5; i++)
		 {
		ucData -= pAudioInfoFrame->pktbyte.AUD_DB[i];
		}
	ucData -=
	    0x80 + AUDIO_INFOFRAME_VER + AUDIO_INFOFRAME_TYPE +
	    AUDIO_INFOFRAME_LEN;
	WriteByteHDMITX_CAT(REG_PKT_AUDINFO_SUM, ucData);
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);
	WriteByteHDMITX_CAT(REG_AUD_INFOFRM_CTRL, B_ENABLE_PKT | B_REPEAT_PKT);	//ENABLE_AUD_INFOFRM_PKT();
	return 0;
}
static void CAT_EnableAudioInfoFrame(int bEnable,
					unsigned char *pAudioInfoFrame) 
{
	
//    if( !bEnable )
//    {
	    WriteByteHDMITX_CAT(REG_AUD_INFOFRM_CTRL, 0);	//disable PktAUO packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
//    }
	if (bEnable)
		 {
		CAT_SetAudioInfoFrame((Audio_InfoFrame *) pAudioInfoFrame);
		}
}


////////////////////////////////////////////////////////////////////////////////
// Function: ConfigAudioInfoFrm
// Parameter: NumChannel, number from 1 to 8
// Remark: Evaluate. The speakerplacement is only for reference.
//         For production, the caller of SetAudioInfoFrame should program
//         Speaker placement by actual status.
//In cases where the audio information in the Audio InfoFrame does not agree with the actual 
//audio stream being received, the conflicting information in the Audio InfoFrame shall be ignored.
////////////////////////////////////////////////////////////////////////////////
void CAT_ConfigAudioInfoFrm(Hdmi_info_para_t * info) 
{
	unsigned char i, ct, cc, sf, ss, ca, lsv;
	Audio_InfoFrame AudioInfo;
	AudioInfo.pktbyte.AUD_HB[0] = AUDIO_INFOFRAME_TYPE;
	AudioInfo.pktbyte.AUD_HB[1] = 1;
	AudioInfo.pktbyte.AUD_HB[2] = 10;
	
//HDMI requires the CT, SS and SF fields to be set to 0 ("Refer to Stream Header") as these items are carried in the audio stream.    
	    ct = 0;		//audio code type: Refer to Stream Header
	cc = 0;			//audio channel count: Refer to Stream Header
	ss = 0;			//sample size: Refer to Stream Header
	sf = 0;			//sampling frequency: Refer to Stream Header
	ca = 0;			//Speaker Mapping: 2 channels(FR & FL) ,applyonlyto multi-channel(Le.,morethantwochannels)uncompressedaudio.
	lsv = 0;		//DTV Monitor how much the source device attenuated the audio during a down-mixing operation. 0db  
	
//    // 6611 did not accept this, cannot set anything.
	    AudioInfo.pktbyte.AUD_DB[0] = (ct << 4) | cc;
	AudioInfo.pktbyte.AUD_DB[1] = (sf << 2) | ss;
	AudioInfo.pktbyte.AUD_DB[2] = 0;
	AudioInfo.pktbyte.AUD_DB[3] = ca;
	AudioInfo.pktbyte.AUD_DB[4] = lsv;
	for (i = 5; i < 10; i++)
		 {
		AudioInfo.pktbyte.AUD_DB[i] = 0;	//these datas must be 0
		}
	
//    /* 
//    CAT6611 does not need to fill the sample size information in audio info frame.
//    ignore the part.
//    */
	    CAT_EnableAudioInfoFrame(1, (unsigned char *)&AudioInfo);
} 
