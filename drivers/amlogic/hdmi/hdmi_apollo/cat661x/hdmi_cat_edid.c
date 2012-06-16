
#include "hdmi_cat_defstx.h"
#include "hdmi_i2c.h"
#include "hdmi_cat_edid.h"
#include "hdmi_cat_mddc.h"
#include "hdmi_cat_hdcp.h"
#include "hdmi_global.h"
    
#include "hdmi_debug.h"
static int CAT_GetEDIDData(unsigned char EDIDBlockID, unsigned char offset,
			     unsigned short Count, unsigned char *pEDIDData) 
{
	unsigned char bSegment, bCurrOffset, ucdata, TimeOut;
	unsigned char mddc_info[6];
	unsigned short RemainedCount, ReqCount;
	bSegment = EDIDBlockID / 2;
	if ((ReadByteHDMITX_CAT(REG_INT_STAT1)) & B_INT_DDC_BUS_HANG)
		 {
		hdmi_dbg_print("DDC BUS HANG, Called AboutDDC()\n");
		
//      printf("DDC BUS HANG, Called AboutDDC()\n") ;
		    CAT_AbortDDC();
		}
	RemainedCount = Count;
	bCurrOffset = EDIDBlockID * 128 + offset;
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	while (RemainedCount > 0)
		 {
		ReqCount =
		    (RemainedCount >
		     DDC_FIFO_MAXREQ) ? DDC_FIFO_MAXREQ : RemainedCount;
		CAT_ClearDDCFIFO();
		
#if 1
		    mddc_info[0] = B_MASTERDDC | B_MASTERHOST;	//switch PC
		mddc_info[1] = DDC_EDID_ADDRESS;	// for EDID ucdata get
		mddc_info[2] = bCurrOffset;
		mddc_info[3] = (unsigned char)ReqCount;
		mddc_info[4] = bSegment;
		mddc_info[5] = CMD_EDID_READ;
		WriteBlockHDMITX_CAT(REG_DDC_MASTER_CTRL, 6, mddc_info);
		
#else	/*  */
		    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL,
					 B_MASTERDDC | B_MASTERHOST);
		WriteByteHDMITX_CAT(REG_DDC_HEADER, DDC_EDID_ADDRESS);
		WriteByteHDMITX_CAT(REG_DDC_REQOFF, bCurrOffset);
		WriteByteHDMITX_CAT(REG_DDC_REQCOUNT, ReqCount);
		WriteByteHDMITX_CAT(REG_DDC_EDIDSEG, bSegment);
		WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_EDID_READ);
		
#endif	/*  */
		    bCurrOffset += ReqCount;
		RemainedCount -= ReqCount;
		for (TimeOut = 250; TimeOut > 0; TimeOut--)
			 {
			AVTimeDly(1);
			ucdata = ReadByteHDMITX_CAT(REG_DDC_STATUS);
			if (ucdata & B_DDC_DONE)
				 {
				break;
				}
			if (ucdata & B_DDC_ERROR)
				 {
				hdmi_dbg_print("Read EDID fail.\n");
				
//                printf("Read EDID fail.\n") ;
				    return -1;
				}
			}
		if (TimeOut == 0)
			 {
			hdmi_dbg_print("Read EDID TimeOut. \n");
			
//            printf("Read EDID TimeOut. \n") ;
			    return -1;
			}
		
		    //  /*    
		    do
			 {
			*(pEDIDData++) = ReadByteHDMITX_CAT(REG_DDC_READFIFO);
			ReqCount--;
		} while (ReqCount > 0);
		
		    //    */
		    //  ReadBlockHDMITX_CAT(REG_DDC_READFIFO, ReqCount, pEDIDData);
		    //    pEDIDData += ReqCount;
		}
	return 0;
}


//-----------------------------------------------------------
static void MonitorCapable861(Hdmi_info_para_t * info,
			      unsigned char edid_flag) 
{
	if (edid_flag & 0x80)
		info->support_underscan_flag = 1;
	if (edid_flag & 0x40)
		info->support_basic_audio_flag = 1;
	if (edid_flag & 0x20)
		 {
		info->support_ycbcr444_flag = 1;
		
		    //info->videopath_outindex = 1;    // Video Output Color Space Conversion : 0 - RGB out; 1 - YCbr (4:4:4) out; 2 - YCbr (4:2:2) out;   
		    info->videopath_outindex = 1;	// Video Output Color Space Conversion : 0 - RGB out; 1 - YCbr (4:4:4) out; 2 - YCbr (4:2:2) out;   
		}
	if (edid_flag & 0x10)
		 {
		info->support_ycbcr422_flag = 1;
		if (!(edid_flag & 0x20))
			info->videopath_outindex = 2;	// Video Output Color Space Conversion : 0 - RGB out; 1 - YCbr (4:4:4) out; 2 - YCbr (4:2:2) out;   
		}
}


//-----------------------------------------------------------
static void ParsingVideoDATABlock(Hdmi_info_para_t * info, unsigned char *buff,
				  unsigned char BaseAddr,
				  unsigned char NBytes) 
{
	unsigned char i;
	NBytes &= 0x1F;
	for (i = 0; i < NBytes; i++)
		 {
		switch (buff[i + BaseAddr] & 0x7F)
			 {
		case 6:
			info->video_480i.support_format = 1;
			info->video_480i.support_4_3 = 1;
			hdmi_dbg_print("Support 480i_4x3\n");
			break;
		case 7:
			info->video_480i.support_format = 1;
			info->video_480i.support_16_9 = 1;
			hdmi_dbg_print("Support 480i_16x9\n");
			break;
		case 21:
			info->video_576i.support_format = 1;
			info->video_576i.support_4_3 = 1;
			hdmi_dbg_print("Support 576i_4x3\n");
			break;
		case 22:
			info->video_576i.support_format = 1;
			info->video_576i.support_16_9 = 1;
			hdmi_dbg_print("Support 576i_16x9\n");
			break;
		case 2:
			info->video_480p.support_format = 1;
			info->video_480p.support_4_3 = 1;
			hdmi_dbg_print("Support 480p_4x3\n");
			break;
		case 3:
			info->video_480p.support_format = 1;
			info->video_480p.support_16_9 = 1;
			hdmi_dbg_print("Support 480p_16x9\n");
			break;
		case 17:
			info->video_576p.support_format = 1;
			info->video_576p.support_4_3 = 1;
			hdmi_dbg_print("Support 576p_4x3\n");
			break;
		case 18:
			info->video_576p.support_format = 1;
			info->video_576p.support_16_9 = 1;
			hdmi_dbg_print("Support 576p_16x9\n");
			break;
		case 4:
			info->video_720p_60hz.support_format = 1;
			info->video_720p_60hz.support_16_9 = 1;
			hdmi_dbg_print("Support 720p_60hz_16x9\n");
			break;
		case 19:
			info->video_720p_50hz.support_format = 1;
			info->video_720p_50hz.support_16_9 = 1;
			hdmi_dbg_print("Support 720p_50hz_16x9\n");
			break;
		case 5:
			info->video_1080i_60hz.support_format = 1;
			info->video_1080i_60hz.support_16_9 = 1;
			hdmi_dbg_print("Support 1080i_60hz_16x9\n");
			break;
		case 20:
			info->video_1080i_50hz.support_format = 1;
			info->video_1080i_50hz.support_16_9 = 1;
			hdmi_dbg_print("Support 1080i_50hz_16x9\n");
			break;
		case 16:
			info->video_1080p_60hz.support_format = 1;
			info->video_1080p_60hz.support_16_9 = 1;
			hdmi_dbg_print("Support 1080p_60hz_16x9\n");
			break;
		case 31:
			info->video_1080p_50hz.support_format = 1;
			info->video_1080p_50hz.support_16_9 = 1;
			hdmi_dbg_print("Support 1080p_50hz_16x9\n");
			break;
		default:
			break;
			}
		}
}


//-----------------------------------------------------------
static void ParsingAudioDATABlock(Hdmi_info_para_t * info, unsigned char *Data,
				  unsigned char BaseAddr,
				  unsigned char NBytes) 
{
	unsigned char AudioFormatCode, i = BaseAddr;
	NBytes &= 0x1F;
	
	do {
		AudioFormatCode = (Data[i] & 0xF8) >> 3;
		switch (AudioFormatCode)
			 {
		case 1:
			info->audio_linear_pcm.support_flag = 1;
			info->audio_linear_pcm.max_channel_num =
			    (Data[i] & 0x07);
			if ((Data[i + 1] & 0x40))
				info->audio_linear_pcm._192k = 1;
			if ((Data[i + 1] & 0x20))
				info->audio_linear_pcm._176k = 1;
			if ((Data[i + 1] & 0x10))
				info->audio_linear_pcm._96k = 1;
			if ((Data[i + 1] & 0x08))
				info->audio_linear_pcm._88k = 1;
			if ((Data[i + 1] & 0x04))
				info->audio_linear_pcm._48k = 1;
			if ((Data[i + 1] & 0x02))
				info->audio_linear_pcm._44k = 1;
			if ((Data[i + 1] & 0x01))
				info->audio_linear_pcm._32k = 1;
			if ((Data[i + 2] & 0x04))
				info->audio_linear_pcm._24bit = 1;
			if ((Data[i + 2] & 0x02))
				info->audio_linear_pcm._20bit = 1;
			if ((Data[i + 2] & 0x01))
				info->audio_linear_pcm._16bit = 1;
			break;
		case 2:
			info->audio_ac_3.support_flag = 1;
			info->audio_ac_3.max_channel_num = (Data[i] & 0x07);
			if ((Data[i + 1] & 0x40))
				info->audio_ac_3._192k = 1;
			if ((Data[i + 1] & 0x20))
				info->audio_ac_3._176k = 1;
			if ((Data[i + 1] & 0x10))
				info->audio_ac_3._96k = 1;
			if ((Data[i + 1] & 0x08))
				info->audio_ac_3._88k = 1;
			if ((Data[i + 1] & 0x04))
				info->audio_ac_3._48k = 1;
			if ((Data[i + 1] & 0x02))
				info->audio_ac_3._44k = 1;
			if ((Data[i + 1] & 0x01))
				info->audio_ac_3._32k = 1;
			info->audio_ac_3._other_bit = Data[i + 2];
			break;
		case 3:
			info->audio_mpeg1.support_flag = 1;
			info->audio_mpeg1.max_channel_num = (Data[i] & 0x07);
			if ((Data[i + 1] & 0x40))
				info->audio_mpeg1._192k = 1;
			if ((Data[i + 1] & 0x20))
				info->audio_mpeg1._176k = 1;
			if ((Data[i + 1] & 0x10))
				info->audio_mpeg1._96k = 1;
			if ((Data[i + 1] & 0x08))
				info->audio_mpeg1._88k = 1;
			if ((Data[i + 1] & 0x04))
				info->audio_mpeg1._48k = 1;
			if ((Data[i + 1] & 0x02))
				info->audio_mpeg1._44k = 1;
			if ((Data[i + 1] & 0x01))
				info->audio_mpeg1._32k = 1;
			info->audio_mpeg1._other_bit = Data[i + 2];
			break;
		case 4:
			info->audio_mp3.support_flag = 1;
			info->audio_mp3.max_channel_num = (Data[i] & 0x07);
			if ((Data[i + 1] & 0x40))
				info->audio_mp3._192k = 1;
			if ((Data[i + 1] & 0x20))
				info->audio_mp3._176k = 1;
			if ((Data[i + 1] & 0x10))
				info->audio_mp3._96k = 1;
			if ((Data[i + 1] & 0x08))
				info->audio_mp3._88k = 1;
			if ((Data[i + 1] & 0x04))
				info->audio_mp3._48k = 1;
			if ((Data[i + 1] & 0x02))
				info->audio_mp3._44k = 1;
			if ((Data[i + 1] & 0x01))
				info->audio_mp3._32k = 1;
			info->audio_mp3._other_bit = Data[i + 2];
			break;
		case 5:
			info->audio_mpeg2.support_flag = 1;
			info->audio_mpeg2.max_channel_num = (Data[i] & 0x07);
			if ((Data[i + 1] & 0x40))
				info->audio_mpeg2._192k = 1;
			if ((Data[i + 1] & 0x20))
				info->audio_mpeg2._176k = 1;
			if ((Data[i + 1] & 0x10))
				info->audio_mpeg2._96k = 1;
			if ((Data[i + 1] & 0x08))
				info->audio_mpeg2._88k = 1;
			if ((Data[i + 1] & 0x04))
				info->audio_mpeg2._48k = 1;
			if ((Data[i + 1] & 0x02))
				info->audio_mpeg2._44k = 1;
			if ((Data[i + 1] & 0x01))
				info->audio_mpeg2._32k = 1;
			info->audio_mpeg2._other_bit = Data[i + 2];
			break;
		case 6:
			info->audio_mpeg2.support_flag = 1;
			info->audio_mpeg2.max_channel_num = (Data[i] & 0x07);
			if ((Data[i + 1] & 0x40))
				info->audio_mpeg2._192k = 1;
			if ((Data[i + 1] & 0x20))
				info->audio_mpeg2._176k = 1;
			if ((Data[i + 1] & 0x10))
				info->audio_mpeg2._96k = 1;
			if ((Data[i + 1] & 0x08))
				info->audio_mpeg2._88k = 1;
			if ((Data[i + 1] & 0x04))
				info->audio_mpeg2._48k = 1;
			if ((Data[i + 1] & 0x02))
				info->audio_mpeg2._44k = 1;
			if ((Data[i + 1] & 0x01))
				info->audio_mpeg2._32k = 1;
			info->audio_mpeg2._other_bit = Data[i + 2];
			break;
		case 7:
			info->audio_aac.support_flag = 1;
			info->audio_aac.max_channel_num = (Data[i] & 0x07);
			if ((Data[i + 1] & 0x40))
				info->audio_aac._192k = 1;
			if ((Data[i + 1] & 0x20))
				info->audio_aac._176k = 1;
			if ((Data[i + 1] & 0x10))
				info->audio_aac._96k = 1;
			if ((Data[i + 1] & 0x08))
				info->audio_aac._88k = 1;
			if ((Data[i + 1] & 0x04))
				info->audio_aac._48k = 1;
			if ((Data[i + 1] & 0x02))
				info->audio_aac._44k = 1;
			if ((Data[i + 1] & 0x01))
				info->audio_aac._32k = 1;
			info->audio_aac._other_bit = Data[i + 2];
			break;
		case 8:
			info->audio_dts.support_flag = 1;
			info->audio_dts.max_channel_num = (Data[i] & 0x07);
			if ((Data[i + 1] & 0x40))
				info->audio_dts._192k = 1;
			if ((Data[i + 1] & 0x20))
				info->audio_dts._176k = 1;
			if ((Data[i + 1] & 0x10))
				info->audio_dts._96k = 1;
			if ((Data[i + 1] & 0x08))
				info->audio_dts._88k = 1;
			if ((Data[i + 1] & 0x04))
				info->audio_dts._48k = 1;
			if ((Data[i + 1] & 0x02))
				info->audio_dts._44k = 1;
			if ((Data[i + 1] & 0x01))
				info->audio_dts._32k = 1;
			info->audio_dts._other_bit = Data[i + 2];
			break;
		default:
			break;
			}
		i += 3;
	} while (i < NBytes);
}


//-----------------------------------------------------------
static void ParsingSpeakerDATABlock(Hdmi_info_para_t * info,
				    unsigned char *buff,
				    unsigned char BaseAddr) 
{
	int ii;
	for (ii = 1; ii < 0x80;)
		 {
		switch (buff[BaseAddr] & ii)
			 {
		case 0x40:
			info->speaker_allocation.rlc_rrc = 1;
			break;
		case 0x20:
			info->speaker_allocation.flc_frc = 1;
			break;
		case 0x10:
			info->speaker_allocation.rc = 1;
			break;
		case 0x08:
			info->speaker_allocation.rl_rr = 1;
			break;
		case 0x04:
			info->speaker_allocation.fc = 1;
			break;
		case 0x02:
			info->speaker_allocation.lfe = 1;
			break;
		case 0x01:
			info->speaker_allocation.fl_fr = 1;
			break;
		default:
			break;
			}
		ii = ii << 1;
		}
}


//-----------------------------------------------------------
static int ParsingCEADataBlockCollection(Hdmi_info_para_t * info,
					 unsigned char *buff) 
{
	unsigned char AddrTag, D, Addr, Data;
	D = buff[2];		//Byte number offset d where Detailed Timing data begins
	Addr = 4;
	
//   D+=Addr;
	    AddrTag = Addr;
	
	do {
		Data = buff[AddrTag];
		switch (Data & 0xE0)
			 {
		case VIDEO_TAG:
			ParsingVideoDATABlock(info, buff, Addr + 1,
					       (Data & 0x1F));
			break;
		case AUDIO_TAG:
			ParsingAudioDATABlock(info, buff, Addr + 1,
					       (Data & 0x1F));
			break;
		case SPEAKER_TAG:
			ParsingSpeakerDATABlock(info, buff, Addr + 1);
			break;
		case VENDOR_TAG:
			if ((buff[Addr + 1] != 0x03)
			     || (buff[Addr + 2] != 0x0c)
			     || (buff[Addr + 3] != 0x00))
				 {
				info->auth_state = HDCP_NO_AUTH;
				info->output_state = CABLE_PLUGIN_DVI_OUT;
				}
			break;
			}
		Addr += (Data & 0x1F);	// next Tag Address
		AddrTag = ++Addr;
	} while (Addr < D);
	return 0;
}


//-----------------------------------------------------------
static void CompareTimingDescriptors(Hdmi_info_para_t * info,
				     unsigned char *Data) 
{
	int index1, index2;
	unsigned char TimingDescriptors[168] =	//12x14
	{ 0x8C, 0x0A, 0xA0, 0x14, 0x51, 0xF0, 0x16, 0x00, 0x26, 0x7c, 0x43, 0x00, 0x13, 0x8e,	//480i(4:3)
		0x8C, 0x0A, 0xA0, 0x14, 0x51, 0xF0, 0x16, 0x00, 0x26, 0x7c, 0x43, 0x00, 0xc4, 0x8e,	//480i(16:9)
		0x8C, 0x0A, 0xA0, 0x20, 0x51, 0x20, 0x18, 0x10, 0x18, 0x7e, 0x23, 0x00, 0x13, 0x8e,	//576i (4:3)
		0x8C, 0x0A, 0xA0, 0x20, 0x51, 0x20, 0x18, 0x10, 0x18, 0x7e, 0x23, 0x00, 0xc4, 0x8e,	//576i (16:9)
		0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3e, 0x96, 0x00, 0x13, 0x8e,	//480p (4:3) 
		0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3e, 0x96, 0x00, 0xc4, 0x8e,	//480p (16:9)
		0x8C, 0x0A, 0xD0, 0x90, 0x20, 0x40, 0x31, 0x20, 0x0c, 0x40, 0x55, 0x00, 0x13, 0x8e,	//576p  (4:3) 
		0x8C, 0x0A, 0xD0, 0x90, 0x20, 0x40, 0x31, 0x20, 0x0c, 0x40, 0x55, 0x00, 0xc4, 0x8e,	//576p  (16:9)          
		0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6e, 0x28, 0x55, 0x00, 0xc4, 0x8e,	//720p60(16:9)       
		0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20, 0xb8, 0x28, 0x55, 0x40, 0xc4, 0x8e,	//720p50 (16:9)  
		0x01, 0x1D, 0x80, 0x18, 0x71, 0x1C, 0x16, 0x20, 0x58, 0x2c, 0x25, 0x00, 0xc4, 0x8e,	//1080i60 (16:9)  
		0x01, 0x1D, 0x80, 0xD0, 0x72, 0x1C, 0x16, 0x20, 0x10, 0x2c, 0x25, 0x80, 0xc4, 0x8e	//1080i50 (16:9)                   
	};
	for (index1 = 0; index1 < 12; index1++)
		 {
		for (index2 = 0; index2 < 12; index2++)
			 {
			if (Data[index2] !=
			     TimingDescriptors[index1 * 14 + index2])
				break;
			}
		if (index2 == 12)
			 {
			switch (index1)
				 {
			case 0:
			case 1:
				info->video_480i.support_format = 1;
				if ((Data[12] == TimingDescriptors[12])
				     && (Data[13] == TimingDescriptors[13]))
					 {
					info->video_480i.support_4_3 = 1;
					hdmi_dbg_print
					    ("Detailed support 480i_60hz_4x3\n");
					}
				
				else if ((Data[12] ==
					  TimingDescriptors[1 * 14 + 12])
					 && (Data[13] ==
					     TimingDescriptors[1 * 14 + 13]))
					 {
					info->video_480i.support_16_9 = 1;
					hdmi_dbg_print
					    ("Detailed support 480i_60hz_16x9\n");
					}
				break;
			case 2:
			case 3:
				info->video_576i.support_format = 1;
				if ((Data[12] ==
				      TimingDescriptors[2 * 14 + 12])
				     && (Data[13] ==
					 TimingDescriptors[2 * 14 + 13]))
					 {
					info->video_576i.support_4_3 = 1;
					hdmi_dbg_print
					    ("Detailed support 576i_50hz_4x3\n");
					}
				
				else if ((Data[12] ==
					  TimingDescriptors[3 * 14 + 12])
					 && (Data[13] ==
					     TimingDescriptors[3 * 14 + 13]))
					 {
					info->video_576i.support_16_9 = 1;
					hdmi_dbg_print
					    ("Detailed support 576i_50hz_16x9\n");
					}
				break;
			case 4:
			case 5:
				info->video_480p.support_format = 1;
				if ((Data[12] ==
				      TimingDescriptors[4 * 14 + 12])
				     && (Data[13] ==
					 TimingDescriptors[4 * 14 + 13]))
					 {
					info->video_480p.support_4_3 = 1;
					hdmi_dbg_print
					    ("Detailed support 480p_60hz_4x3\n");
					}
				
				else if ((Data[12] ==
					  TimingDescriptors[5 * 14 + 12])
					 && (Data[13] ==
					     TimingDescriptors[5 * 14 + 13]))
					 {
					info->video_480p.support_16_9 = 1;
					hdmi_dbg_print
					    ("Detailed support 480p_60hz_16x9\n");
					}
				break;
			case 6:
			case 7:
				info->video_576p.support_format = 1;
				if ((Data[12] ==
				      TimingDescriptors[6 * 14 + 12])
				     && (Data[13] ==
					 TimingDescriptors[6 * 14 + 13]))
					 {
					info->video_576p.support_4_3 = 1;
					hdmi_dbg_print
					    ("Detailed support 576p_50hz_4x3\n");
					}
				
				else if ((Data[12] ==
					  TimingDescriptors[7 * 14 + 12])
					 && (Data[13] ==
					     TimingDescriptors[7 * 14 + 13]))
					 {
					info->video_576p.support_16_9 = 1;
					hdmi_dbg_print
					    ("Detailed support 576p_50hz_16x9\n");
					}
				break;
			case 8:
				info->video_720p_60hz.support_format = 1;
				if ((Data[12] ==
				      TimingDescriptors[8 * 14 + 12])
				     && (Data[13] ==
					 TimingDescriptors[8 * 14 + 13]))
					 {
					info->video_720p_60hz.support_16_9 = 1;
					hdmi_dbg_print
					    ("Detailed support 720p_60hz_16x9\n");
					}
				break;
			case 9:
				info->video_720p_50hz.support_format = 1;
				if ((Data[12] ==
				      TimingDescriptors[9 * 14 + 12])
				     && (Data[13] ==
					 TimingDescriptors[9 * 14 + 13]))
					 {
					info->video_720p_50hz.support_16_9 = 1;
					hdmi_dbg_print
					    ("Detailed support 720p_50hz_16x9\n");
					}
				break;
			case 10:
				info->video_1080i_60hz.support_format = 1;
				if ((Data[12] ==
				      TimingDescriptors[10 * 14 + 12])
				     && (Data[13] ==
					 TimingDescriptors[10 * 14 + 13]))
					 {
					info->video_1080i_60hz.support_16_9 =
					    1;
					hdmi_dbg_print
					    ("Detailed support 1080i_60hz_16x9\n");
					}
				break;
			case 11:
				info->video_1080i_50hz.support_format = 1;
				if ((Data[12] ==
				      TimingDescriptors[11 * 14 + 12])
				     && (Data[13] ==
					 TimingDescriptors[11 * 14 + 13]))
					 {
					info->video_1080i_50hz.support_16_9 =
					    1;
					hdmi_dbg_print
					    ("Detailed support 1080i_50hz_16x9\n");
					}
				break;
			default:
				break;
				}
			break;
			}
		}
}


//-----------------------------------------------------------
static void ParseCEADetailedTimingDescriptors(Hdmi_info_para_t * info,
					      unsigned char blk_mun,
					      unsigned char BaseAddr,
					      unsigned char *buff) 
{
	unsigned char index_edid;
	for (index_edid = 0; index_edid < blk_mun; index_edid++)
		 {
		CompareTimingDescriptors(info, &buff[BaseAddr]);
		BaseAddr += 18;
		if ((BaseAddr + 18) > (0x7d + 0x80 * (info->cea_on_fisrt_page)))	//there is not the TimingDescriptors
			break;
		}
}


/////////////////////////////////////////////////////////////////////
// CAT_ParseEDID()
// Check EDID check sum and EDID 1.3 extended segment.
/////////////////////////////////////////////////////////////////////
int CAT_ParseEDID(Hdmi_info_para_t * info) 
{
	unsigned char CheckSum, BlockCount;
	unsigned char EDID_Buf[128];
	int i, err;
	info->cea_on_fisrt_page = 0;
	err = CAT_GetEDIDData(info->cea_on_fisrt_page, 0, 128, EDID_Buf);	//read the DecodeHeader datas
	if (err == -1)
		return -1;
	
//        dump(EDID_Buf,128);
//        printf(" the first edid block edid datas \n") ;       
	    for (i = 0, CheckSum = 0; i < 128; i++)
		 {
		
//        printf(" %02X", EDID_Buf[i]) ;
		    CheckSum += EDID_Buf[i];
		
//        if((((i + 1) % 0x10) == 0) && (i != 0) ) 
//           printf(" \n") ;    
		    CheckSum &= 0xFF;
		}
	
//         printf(" \n") ;
	    if (CheckSum != 0)
		 {
		info->output_state = CABLE_PLUGIN_DVI_OUT;
		
//                 return -1 ;
		}
	if (!(EDID_Buf[0] | EDID_Buf[7]))
		 {
		for (i = 1; i < 7; i++)
			 {
			if (EDID_Buf[i] != 0xff)
				 {
				info->output_state = CABLE_PLUGIN_DVI_OUT;
				return -1;
				}
			}
		}
	
	else
		 {
		info->output_state = CABLE_PLUGIN_DVI_OUT;
		return -1;
		}
	ParseCEADetailedTimingDescriptors(info, 4, 0x36, EDID_Buf);
	BlockCount = EDID_Buf[0x7E];
	if (BlockCount == 0)
		 {
		info->output_state = CABLE_PLUGIN_DVI_OUT;
		return -1;	// do nothing.
		}
	
//    else if ( BlockCount > 4 )
//    {
//        BlockCount = 4 ;
//    }
	    for (info->cea_on_fisrt_page = 1;
		  info->cea_on_fisrt_page <= BlockCount;
		  info->cea_on_fisrt_page++)
		 {
		err =
		    CAT_GetEDIDData(info->cea_on_fisrt_page, 0, 128, EDID_Buf);
		if (err != -1)
			 {
			
//           printf(" the %dth edid block edid datas \n", info->cea_on_fisrt_page ) ;   
			    for (i = 0, CheckSum = 0; i < 128; i++)
				 {
				
//               printf(" %02X", EDID_Buf[i]) ;
				    CheckSum += EDID_Buf[i];
				
//               if((((i + 1) % 0x10) == 0) && (i != 0) ) 
//                  printf(" \n") ;
				    CheckSum &= 0xFF;
				}
			if ((CheckSum == 0) && (EDID_Buf[0] == 0x2)
			      && (EDID_Buf[1] == 0x3))
				 {
				MonitorCapable861(info, EDID_Buf[3]);
				err =
				    ParsingCEADataBlockCollection(info,
								  EDID_Buf);
				
//                if( err != -1 )
//                      { 
				    ParseCEADetailedTimingDescriptors(info, 5,
								      EDID_Buf
								      [2],
								      EDID_Buf);
				if (info->output_state != CABLE_PLUGIN_DVI_OUT)
					info->output_state =
					    CABLE_PLUGIN_HDMI_OUT;
				
//                  }
//                  else 
				    //                     return -1;                                         
				}
			
//           else
//              {
//                      c= CABLE_PLUGIN_DVI_OUT;
//                return -1;
//              }
			}
		
		else
			 {
			return -1;
			}
		}
	return err;
}


