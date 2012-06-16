
#include "hdmi_global.h"
#include "hdmi_cat_defstx.h"
#include "hdmi_i2c.h"
#include "hdmi_cat_video.h"
#include "hdmi_vmode.h"
    
#include "hdmi_debug.h"
extern VModeInfoType VModeTables[NMODES];
extern int hdmi_cat6611_SetAVmute(Hdmi_info_para_t * info);
int CAT_ProgramSyncEmbeddedVideoMode(Hdmi_info_para_t * info)	// if Embedded Video, need to generate timing with pattern register
{
	int vic;
	
//    int rtn = 1;
	struct SyncEmbeddedSetting SyncEmbTable[] = { 
		    // {FMT,0x90, 0x91,
		    // 0x90, 0x91, 0x95, 0x96,0x97, 0xA0, 0xA1, 0xA2, 0xA3, 0x90_CCIR8,0x91_CCIR8, 0x95_CCIR8, 0x960_CCIR8,  0x97_CCIR8, VFREQ,  PCLK       },
		{0xE0, 0x1B, 0x11, 0x4F, 0x00, 0x04, 0x70, 0x0A, 0xD1, 0xE0, 0x37, 0x23, 0x9F, 0x00, 60, 27000000},	//480I
		{0xA0, 0x1B, 0x0A, 0x49, 0x00, 0x02, 0x50, 0x3A, 0xD1, 0x50, 0x37, 0x15, 0x93, 0x00, 50, 27000000},	//576I
	};
	
#ifdef AML_NIKE  
	    // 0x90,0x91,  0x92,    0x93,  0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0xA0,0xA1,0xA2,0xA3
	unsigned char DEGen480i[] =
	    { 0x89, 0x35, 0xee - 1, 0x8e - 1, 0x60, 0xB3 - 1, 0x7B - 1, 0x06,
0x0C, 0x02, 0x11, 0x01, 0x10, 0x18, 0x08, 0x21, 0x0C, 0x22, 0x06, 0x91 };
	
#else				//ifdef AML_APOLLO
	unsigned char DEGen480i[] =
	    { 0x89, 0x35, 0xee + 1, 0x8e + 1, 0x60, 0xB3 - 1, 0x7B - 1, 0x06,
0x0C, 0x02, 0x11, 0x01, 0x10, 0x18, 0x08, 0x21, 0x0C, 0x22, 0x06, 0x91 };
	
#endif	/*  */
	unsigned char DEGen576i[] =
	    { 0xD9, 0x35, 0x06 - 1, 0xA6 - 1, 0x61, 0xBD - 1, 0x7B - 1, 0x06,
0x70, 0x02, 0x16, 0x36, 0x10, 0x4F, 0x6F, 0x21, 0x00, 0x30, 0x39, 0xC1 };
	unsigned char *pTimingGen;
	if ((info->tv_mode != HDMI_480I_60HZ)
	      && (info->tv_mode != HDMI_576I_50HZ))
		return -1;
	hdmi_dbg_print("ProgramSyncEmbeddedVideoMode \n");
	if (info->sync_emb_flag & T_MODE_SYNCEMB)
		 {
		CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
		if (info->sync_emb_flag & T_MODE_CCIR656)
			 {
			hdmi_dbg_print("Embedded,  CCIR8\n");
			
//                  printf("Embedded,  CCIR8\n");
			    WriteByteHDMITX_CAT(REG_HVPol, SyncEmbTable[info->tv_mode].REGHVPol656);	// Reg90
			WriteByteHDMITX_CAT(REG_HfPixel, SyncEmbTable[info->tv_mode].REGHfPixel656);	// Reg91
			WriteByteHDMITX_CAT(REG_HSSL, SyncEmbTable[info->tv_mode].RegHSSL656);	// Reg95
			WriteByteHDMITX_CAT(REG_HSEL, SyncEmbTable[info->tv_mode].RegHSEL656);	// Reg96
			WriteByteHDMITX_CAT(REG_HSH, SyncEmbTable[info->tv_mode].RegHSH656);	// Reg97
			}
		
		else
			 {
			hdmi_dbg_print("Embedded, CCIR16\n");
			
//            printf("Embedded, CCIR16\n");  
			    WriteByteHDMITX_CAT(REG_HVPol, SyncEmbTable[info->tv_mode].RegHVPol);	// Reg90
			WriteByteHDMITX_CAT(REG_HfPixel, SyncEmbTable[info->tv_mode].RegHfPixel);	// Reg91
			WriteByteHDMITX_CAT(REG_HSSL, SyncEmbTable[info->tv_mode].RegHSSL);	// Reg95
			WriteByteHDMITX_CAT(REG_HSEL, SyncEmbTable[info->tv_mode].RegHSEL);	// Reg96
			WriteByteHDMITX_CAT(REG_HSH, SyncEmbTable[info->tv_mode].RegHSH);	// Reg97
			}
		WriteByteHDMITX_CAT(REG_VSS1, SyncEmbTable[info->tv_mode].RegVSS1);	// RegA0
		WriteByteHDMITX_CAT(REG_VSE1, SyncEmbTable[info->tv_mode].RegVSE1);	// RegA1 
		WriteByteHDMITX_CAT(REG_VSS2, SyncEmbTable[info->tv_mode].RegVSS2);	// RegA2
		WriteByteHDMITX_CAT(REG_VSE2, SyncEmbTable[info->tv_mode].RegVSE2);	// RegA3
		}
	
	else if ((info->videopath_inindex & 0x7) == 4)	//YUV422:
	{
		hdmi_dbg_print("Generate Sync and DE by Sync\n");
		
//        printf("Generate Sync and DE by Sync\n") ;
		    switch (info->tv_mode)
			 {
		case HDMI_480I_60HZ:
			pTimingGen = DEGen480i;
			break;
		case HDMI_576I_50HZ:
			pTimingGen = DEGen576i;
			break;
		default:
			hdmi_dbg_print
			    ("Not a supported timing for VIC %d to gen sync/DE from sync.\n",
			     info->tv_mode);
			
//            printf("Not a supported timing for VIC %d to gen sync/DE from sync.\n") ;
			    return -1;
			}
		for (vic = 0x90; vic < 0xA4; vic++)
			 {
			WriteByteHDMITX_CAT(vic, pTimingGen[vic - 0x90]);	// Reg90 ~ RegA3
			}
	}
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Function: SetInputMode
// InputMode - use [1:0] to identify the color space for reg70[7:6], 
// definition:
//            #define F_MODE_RGB444  0
//            #define F_MODE_YUV422 4
//            #define F_MODE_YUV444 2
//            #define F_MODE_CLRMOD_MASK 3
// VideoInputType - defined the CCIR656 D[0], SYNC Embedded D[1], and DDR input in D[2].
// Side-Effect: Reg70.
////////////////////////////////////////////////////////////////////////////////
static void CAT_SetInputMode(Hdmi_info_para_t * info) 
{
	unsigned char ucData;
	hdmi_dbg_print("CAT_SetInputMode=%d\n", info->videopath_inindex & 0x7);
	ucData = ReadByteHDMITX_CAT(REG_INPUT_MODE);
	ucData &=
	    ~(M_INCOLMOD | B_2X656CLK | B_SYNCEMB | B_INDDR | B_PCLKDIV2);
	switch (info->videopath_inindex & 0x7)
		 {
	case 4:		//F_MODE_YUV422:
		ucData |= B_IN_YUV422 | B_2X656CLK;
		break;
	case 2:		//F_MODE_YUV444:
		ucData |= B_IN_YUV444;
		break;
	case 0:		//F_MODE_RGB444:
	default:
		ucData |= B_IN_RGB;
		break;
		}
	
	    // if(info->sync_emb_flag & T_MODE_PCLKDIV2)
	    //   {
	    //     ucData |= B_PCLKDIV2 ; 
	    //hdmi_dbg_print("PCLK Divided by 2 mode\n") ;
	    // }
	    
//    if( info->sync_emb_flag & T_MODE_CCIR656 )
//    {
//        ucData |= B_2X656CLK ; 
//        hdmi_dbg_print("CCIR656 mode\n") ;
//        printf("CCIR656 mode\n") ;
//    }
	    if (info->sync_emb_flag & T_MODE_SYNCEMB)
		 {
		ucData |= B_SYNCEMB;
		hdmi_dbg_print("Sync Embedded mode\n");
		
//        printf("Sync Embedded mode\n") ;
		}
	if (info->sync_emb_flag & T_MODE_INDDR)
		 {
		ucData |= B_INDDR;
		hdmi_dbg_print("Input DDR mode\n");
		
//        printf("Input DDR mode\n") ;
		}
	WriteByteHDMITX_CAT(REG_INPUT_MODE, ucData);
}


////////////////////////////////////////////////////////////////////////////////
// Function: SetCSCScale
// Parameter: bInputMode - 
//             D[1:0] - Color Mode
//             D[4] - Colorimetry 0: ITU_BT601 1: ITU_BT709
//             D[5] - Quantization 0: 0_255 1: 16_235
//             D[6] - Up/Dn Filter 'Required' 
//                    0: no up/down filter
//                    1: enable up/down filter when csc need.
//             D[7] - Dither Filter 'Required'
//                    0: no dither enabled.
//                    1: enable dither and dither free go "when required".
//            bOutputMode -
//             D[1:0] - Color mode.
// Remark: reg72~reg8D will be programmed depended the input with table.
////////////////////////////////////////////////////////////////////////////////
static void CAT_SetCSCScale(Hdmi_info_para_t * info) 
{
	unsigned char ucData, csc;
	unsigned char filter = 0;	// filter is for Video CTRL DN_FREE_GO, EN_DITHER, and ENUDFILT
#ifdef AML_NIKE
	unsigned char bCSCOffset_16_235[] = { 0x00, 0x80, 0x00 };
	
#endif	/*  */
	unsigned char bCSCOffset_0_255[] = { 0x10, 0x80, 0x10 };
	
	    //input data is RGB
//    unsigned char  bCSCMtx_RGB2YUV_ITU601_16_235[] =
//    {
//        0xB2,0x04,0x64,0x02,0xE9,0x00,
//        0x93,0x3C,0x18,0x04,0x56,0x3F,
//        0x49,0x3D,0x9F,0x3E,0x18,0x04
//    } ;
	    
//    unsigned char  bCSCMtx_RGB2YUV_ITU601_0_255[] =
//    {
//        0x09,0x04,0x0E,0x02,0xC8,0x00,
//        0x0E,0x3D,0x84,0x03,0x6E,0x3F,
//        0xAC,0x3D,0xD0,0x3E,0x84,0x03
//    } ;
//
//    unsigned char  bCSCMtx_RGB2YUV_ITU709_16_235[] =
//    {
//        0xB8,0x05,0xB4,0x01,0x93,0x00,
//        0x49,0x3C,0x18,0x04,0x9F,0x3F,
//        0xD9,0x3C,0x10,0x3F,0x18,0x04
//    } ;
//
//    unsigned char  bCSCMtx_RGB2YUV_ITU709_0_255[] =
//    {
//        0xE5,0x04,0x78,0x01,0x81,0x00,
//        0xCE,0x3C,0x84,0x03,0xAE,0x3F,
//        0x49,0x3D,0x33,0x3F,0x84,0x03
//    } ;
	    //input data is YUV
	unsigned char bCSCMtx_YUV2RGB_ITU601_0_255[] = 
	    { 0x4F, 0x09, 0x81, 0x39, 0xDF, 0x3C, 0x4F, 0x09, 0xC2, 0x0C,
0x00, 0x00, 0x4F, 0x09, 0x00, 0x00, 0x1E, 0x10 
	};
	
#ifdef AML_NIKE
	unsigned char bCSCMtx_YUV2RGB_ITU601_16_235[] = 
	    { 0x00, 0x08, 0x6A, 0x3A, 0x4F, 0x3D, 0x00, 0x08, 0xF7, 0x0A,
0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0xDB, 0x0D 
	};
	unsigned char bCSCMtx_YUV2RGB_ITU709_16_235[] = 
	    { 0x00, 0x08, 0x53, 0x3C, 0x89, 0x3E, 0x00, 0x08, 0x51, 0x0C,
0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x87, 0x0E 
	};
	
#endif	/*  */
	unsigned char bCSCMtx_YUV2RGB_ITU709_0_255[] = 
	    { 0x4F, 0x09, 0xBA, 0x3B, 0x4B, 0x3E, 0x4F, 0x09, 0x56, 0x0E,
0x00, 0x00, 0x4F, 0x09, 0x00, 0x00, 0xE7, 0x10 
	};
	
	    // (1) YUV422 in, RGB/YUV444 output ( Output is 8-bit, input is 12-bit)
	    // (2) YUV444/422  in, RGB output ( CSC enable, and output is not YUV422 )
	    // (3) RGB in, YUV444 output   ( CSC enable, and output is not YUV422 ) 
	    // YUV444/RGB444 <-> YUV422 need set up/down filter.
	    switch (info->videopath_inindex)
		 {
	case 2:		//F_MODE_YUV444:
		hdmi_dbg_print("Input mode is   YUV444 =%d\n",
			       info->videopath_outindex);
		
//        printf("Input mode is YUV444 ") ;
		    switch (info->videopath_outindex)
			 {
		case 1:	//F_MODE_YUV444:
			hdmi_dbg_print("Output mode is YUV444\n");
			
//            printf("Output mode is YUV444\n") ;
			    csc = B_CSC_BYPASS;
			break;
		case 2:	//F_MODE_YUV422:
			hdmi_dbg_print("Output mode is YUV422\n");
			
//            printf("Output mode is YUV422\n") ;
//            if( info->videopath_inindex & F_VIDMODE_EN_UDFILT) // YUV444 to YUV422 need up/down filter for processing.
			    filter |= B_EN_UDFILTER;
			csc = B_CSC_BYPASS;
			break;
		case 0:	//F_MODE_RGB444:
			hdmi_dbg_print("Output mode is RGB444\n");
			
//            printf("Output mode is RGB444\n") ;
			    csc = B_CSC_YUV2RGB;
			
//            if( info->videopath_inindex & F_VIDMODE_EN_DITHER) // YUV444 to RGB444 need dither
//                filter |= B_EN_DITHER | B_DNFREE_GO ;
			    break;
		default:
			break;
			}
		break;
	case 4:		//F_MODE_YUV422:
		hdmi_dbg_print("Input mode is YUV422 =%d\n",
			       info->videopath_outindex);
		switch (info->videopath_outindex)
			 {
		case 1:	//F_MODE_YUV444:
			hdmi_dbg_print("Output mode is YUV444\n");
			csc = B_CSC_BYPASS;
			
//            if( bInputMode & F_VIDMODE_EN_UDFILT) // YUV422 to YUV444 need up filter
			    filter |= B_EN_UDFILTER;
			
//            if( info->videopath_inindex & F_VIDMODE_EN_DITHER) // YUV422 to YUV444 need dither
//                filter |= B_EN_DITHER | B_DNFREE_GO ;
			    break;
		case 2:	//F_MODE_YUV422:
			hdmi_dbg_print("Output mode is YUV422\n");
			csc = B_CSC_BYPASS;
			break;
		case 0:	//F_MODE_RGB444:
			hdmi_dbg_print("Output mode is RGB444\n");
			csc = B_CSC_YUV2RGB;
			
//            if( info->videopath_inindex & F_VIDMODE_EN_UDFILT) // YUV422 to RGB444 need up/dn filter.
//                filter |= B_EN_UDFILTER ;         
//            if( info->videopath_inindex & F_VIDMODE_EN_DITHER) // YUV422 to RGB444 need dither
//                filter |= B_EN_DITHER | B_DNFREE_GO ;
			    break;
		default:
			break;
			}
		break;
	case 0:		//F_MODE_RGB444:
		hdmi_dbg_print("Input mode is RGB444\n");
		switch (info->videopath_outindex)
			 {
		case 1:	//F_MODE_YUV444:
			hdmi_dbg_print("Output mode is YUV444\n");
			csc = B_CSC_RGB2YUV;
			
//            if( info->videopath_inindex & F_VIDMODE_EN_DITHER) // RGB444 to YUV444 need dither
//                filter |= B_EN_DITHER | B_DNFREE_GO ;
			    break;
		case 2:	//F_MODE_YUV422:
			hdmi_dbg_print("Output mode is YUV422\n");
			
//            if( info->videopath_inindex & F_VIDMODE_EN_UDFILT) // RGB444 to YUV422 need down filter.
			    filter |= B_EN_UDFILTER;
			
//            if( info->videopath_inindex & F_VIDMODE_EN_DITHER) // RGB444 to YUV422 need dither
//                filter |= B_EN_DITHER | B_DNFREE_GO ;
			    csc = B_CSC_RGB2YUV;
			break;
		case 0:	//F_MODE_RGB444:
			hdmi_dbg_print("Output mode is RGB444\n");
			csc = B_CSC_BYPASS;
			break;
		default:
			break;
			}
		break;
	default:
		break;
		}
	
//    if(( csc == B_CSC_RGB2YUV ) && (info->videopath_inindex == 0))  //set the CSC metrix registers by colorimetry and quantization 
//    {
//       switch(bInputMode&(F_VIDMODE_ITU709|F_VIDMODE_16_235))
//        {
//         case F_VIDMODE_ITU709|F_VIDMODE_16_235:
//            hdmi_dbg_print("ITU709 16-235 ") ;
//            WriteBlockHDMITX_CAT(REG_CSC_YOFF, SIZEOF_CSCOFFSET, bCSCOffset_16_235) ;
//            WriteBlockHDMITX_CAT(REG_CSC_MTX11_L, SIZEOF_CSCMTX, bCSCMtx_RGB2YUV_ITU709_16_235) ;
//            break ;
//         case F_VIDMODE_ITU709|F_VIDMODE_0_255:
//            hdmi_dbg_print("ITU709 0-255 ") ;
//            WriteBlockHDMITX_CAT(REG_CSC_YOFF, SIZEOF_CSCOFFSET, bCSCOffset_0_255) ;
//            WriteBlockHDMITX_CAT(REG_CSC_MTX11_L, SIZEOF_CSCMTX, bCSCMtx_RGB2YUV_ITU709_0_255) ;
//            break ;
//         case F_VIDMODE_ITU601|F_VIDMODE_16_235:
//            hdmi_dbg_print("ITU601 16-235 ") ;
//            WriteBlockHDMITX_CAT(REG_CSC_YOFF, SIZEOF_CSCOFFSET, bCSCOffset_16_235) ;
//            WriteBlockHDMITX_CAT(REG_CSC_MTX11_L, SIZEOF_CSCMTX, bCSCMtx_RGB2YUV_ITU601_16_235) ;
//            break ;
//         case F_VIDMODE_ITU601|F_VIDMODE_0_255:
//         default:            
//            hdmi_dbg_print("ITU601 0-255 ") ;
//            WriteBlockHDMITX_CAT(REG_CSC_YOFF, SIZEOF_CSCOFFSET, bCSCOffset_0_255) ;
//            WriteBlockHDMITX_CAT(REG_CSC_MTX11_L, SIZEOF_CSCMTX, bCSCMtx_RGB2YUV_ITU601_0_255) ;
//            break ;
//        }
//
//    }
	    if ((csc == B_CSC_YUV2RGB)
		 && ((info->videopath_inindex == 2)
		     || (info->videopath_inindex == 4)))
		 {
		
#ifdef AML_NIKE			//y is in (16-235) and cb/cr is in (16-240)
		    WriteBlockHDMITX_CAT(REG_CSC_YOFF, SIZEOF_CSCOFFSET,
					 bCSCOffset_16_235);
		
#else				//if defined AML_APOLLO  //Apollo, We can control using 0-255 or 16-235 depending on the setting of the Color Matrix.     
		    WriteBlockHDMITX_CAT(REG_CSC_YOFF, SIZEOF_CSCOFFSET,
					 bCSCOffset_0_255);
		
#endif	/*  */
		    switch (info->tv_mode)
			 {
		case HDMI_720P_60Hz:
		case HDMI_720P_50Hz:
		case HDMI_1080I_60Hz:
		case HDMI_1080I_50Hz:
		case HDMI_1080P_60Hz:
		case HDMI_1080P_50Hz:
			
#ifdef AML_NIKE			//y is in (16-235) and cb/cr is in (16-240)
			    hdmi_dbg_print("ITU709 16-235 \n");
			WriteBlockHDMITX_CAT(REG_CSC_MTX11_L, SIZEOF_CSCMTX,
					      bCSCMtx_YUV2RGB_ITU709_16_235);
			
#else				//if defined AML_APOLLO  //Apollo, We can control using 0-255 or 16-235 depending on the setting of the Color Matrix.     
			    hdmi_dbg_print("ITU709 0-255 \n");
			WriteBlockHDMITX_CAT(REG_CSC_MTX11_L, SIZEOF_CSCMTX,
					      bCSCMtx_YUV2RGB_ITU709_0_255);
			
#endif	/*  */
			    break;
		case HDMI_480I_60HZ:
		case HDMI_576I_50HZ:
		case HDMI_480P_60HZ:
		case HDMI_576P_50HZ:
		default:
			
#ifdef AML_NIKE			//y is in (16-235) and cb/cr is in (16-240)
			    hdmi_dbg_print("ITU601 16-235 \n");
			WriteBlockHDMITX_CAT(REG_CSC_MTX11_L, SIZEOF_CSCMTX,
					      bCSCMtx_YUV2RGB_ITU601_16_235);
			
#else				//if defined AML_APOLLO  //Apollo, We can control using 0-255 or 16-235 depending on the setting of the Color Matrix.     
			    hdmi_dbg_print("ITU601 0-255 \n");
			WriteBlockHDMITX_CAT(REG_CSC_MTX11_L, SIZEOF_CSCMTX,
					      bCSCMtx_YUV2RGB_ITU601_0_255);
			
#endif	/*  */
			    break;
			}
		}
	ucData =
	    (ReadByteHDMITX_CAT(REG_CSC_CTRL)) &
	    (~(M_CSC_SEL | B_DNFREE_GO | B_EN_DITHER | B_EN_UDFILTER));
	ucData |= filter | csc;
	WriteByteHDMITX_CAT(REG_CSC_CTRL, ucData);
}
void CAT_SetHDMIMode(Hdmi_info_para_t * info) 
{
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	if (info->output_state == CABLE_PLUGIN_HDMI_OUT)
		 {
		WriteByteHDMITX_CAT(REG_HDMI_MODE, B_HDMI_MODE);
		}
	
	else
		 {
		WriteByteHDMITX_CAT(REG_HDMI_MODE, B_DVI_MODE);
		WriteByteHDMITX_CAT(REG_NULL_CTRL, 0);	//disable PktNull packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
		WriteByteHDMITX_CAT(REG_ACP_CTRL, 0);	//disable PktACP packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
		WriteByteHDMITX_CAT(REG_ISRC1_CTRL, 0);	//disable PktISRC1 packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
		WriteByteHDMITX_CAT(REG_ISRC2_CTRL, 0);	//disable PktISRC2 packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
		WriteByteHDMITX_CAT(REG_AVI_INFOFRM_CTRL, 0);	//disable PktAVI packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
		WriteByteHDMITX_CAT(REG_AUD_INFOFRM_CTRL, 0);	//disable PktAUO packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
		WriteByteHDMITX_CAT(REG_SPD_INFOFRM_CTRL, 0);	//disable PktSPD packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
		WriteByteHDMITX_CAT(REG_MPG_INFOFRM_CTRL, 0);	//disable PktMPG packet sending & repeat. bit0: enable/diable sending; bit1: enable/disble repeat
		CAT_Switch_HDMITX_Bank(TX_SLV0, 1);	//select bank 1
		WriteByteHDMITX_CAT(REG_AVIINFO_DB1, 0);	//AVI is invalid
		CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
		}
}


////////////////////////////////////////////////////////////////////////////////
// Function: SetupAFE
//            FALSE - PCLK < 80Hz ( for mode less than 1080p) 
//            TRUE  - PCLK > 80Hz ( for 1080p mode or above)
// Remark: set reg62~reg65 depended on HighFreqMode
//         reg61 have to be programmed at last and after video stable input.
////////////////////////////////////////////////////////////////////////////////
static void CAT_SetupAFE(Hdmi_info_para_t * info) 
{
	unsigned char uc;
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	uc = ((ReadByteHDMITX_CAT(REG_INT_CTRL)) & B_IDENT_6612) ?
	    B_AFE_XP_BYPASS : 0;
	WriteByteHDMITX_CAT(REG_AFE_DRV_CTRL, B_AFE_DRV_RST);	//Reset signal for HDMI_TX_DRV. '1': all flip-flops in the transmitter,
	//including those in the BIST pattern generator, are reset.
	if ((info->tv_mode == HDMI_1080P_60Hz)
	    || (info->tv_mode == HDMI_1080P_50Hz))
		 {
		WriteByteHDMITX_CAT(REG_AFE_XP_CTRL1,
				     B_AFE_XP_GAINBIT | B_AFE_XP_RESETB | uc);
		WriteByteHDMITX_CAT(REG_AFE_XP_CTRL2, B_XP_CLKSEL_1_PCLKHV);	//ASCLK=2.5 * Channel C, (filtered or unfiltered) of TMDSTXPLL018CChannel C = 1 * PCLKHV
		WriteByteHDMITX_CAT(REG_AFE_IP_CTRL,
				    B_AFE_IP_GAINBIT | B_AFE_IP_SEDB |
				    B_AFE_IP_RESETB | B_AFE_IP_PDIV1);
		WriteByteHDMITX_CAT(REG_AFE_RING, 0x00);
		}
	
	else
		 {
		WriteByteHDMITX_CAT(REG_AFE_XP_CTRL1,
				     B_AFE_XP_ER0 | B_AFE_XP_RESETB | uc);
		WriteByteHDMITX_CAT(REG_AFE_XP_CTRL2, B_XP_CLKSEL_1_PCLKHV);	//ASCLK=2.5 * Channel C, (filtered or unfiltered) of TMDSTXPLL018CChannel C = 1 * PCLKHV
		WriteByteHDMITX_CAT(REG_AFE_IP_CTRL,
				    B_AFE_IP_SEDB | B_AFE_IP_ER0 |
				    B_AFE_IP_RESETB | B_AFE_IP_PDIV1);
		WriteByteHDMITX_CAT(REG_AFE_RING, 0x00);
		}
	if (info->output_state == CABLE_PLUGIN_HDMI_OUT)
		WriteByteHDMITX_CAT(REG_SW_RST,
				     (ReadByteHDMITX_CAT(REG_SW_RST)) &
				     (~
				      (B_REF_RST | B_AREF_RST | B_VID_RST |
				       B_HDMI_RST)));
	
	else if (info->output_state == CABLE_PLUGIN_DVI_OUT)
		WriteByteHDMITX_CAT(REG_SW_RST,
				     (ReadByteHDMITX_CAT(REG_SW_RST)) &
				     (~(B_REF_RST | B_VID_RST | B_HDMI_RST)));
	
//    WriteByteHDMITX_CAT(REG_INT_MASK3, (ReadByteHDMITX_CAT(REG_INT_MASK3)) | (B_VIDSTABLE_MASK)) ;
//    uc = ReadByteHDMITX_CAT(REG_SW_RST) ;
//    uc &= ~(B_REF_RST|B_VID_RST) ;
//    WriteByteHDMITX_CAT(REG_SW_RST, uc) ;
//    AVTimeDly(1) ;
}


////////////////////////////////////////////////////////////////////////////////
// Function: FireAFE
// Parameter: N/A
// Return: N/A
// Remark: write reg61 with 0x04
//         When program reg61 with 0x04, then audio and video circuit work.
// Side-Effect: N/A
////////////////////////////////////////////////////////////////////////////////
void CAT_FireAFE(void) 
{
	unsigned char uc;
	
//    uc = ReadByteHDMITX_CAT(REG_SW_RST) ;
//    uc &= ~(B_REF_RST|B_VID_RST) ;
//    WriteByteHDMITX_CAT(REG_SW_RST, uc|B_VID_RST) ;
//    AVTimeDly(10) ;
//    WriteByteHDMITX_CAT(REG_SW_RST, uc) ;
//    AVTimeDly(10) ;
	    
	do {
		uc = ReadByteHDMITX_CAT(REG_SYS_STATUS);
		AVTimeDly(1);
	} while (!(uc & B_TXVIDSTABLE));
	uc = ReadByteHDMITX_CAT(REG_SW_RST);
	WriteByteHDMITX_CAT(REG_SW_RST, uc | B_VID_RST);
	AVTimeDly(2);
	uc = ReadByteHDMITX_CAT(REG_SW_RST);
	WriteByteHDMITX_CAT(REG_SW_RST, uc & (~B_VID_RST));
	AVTimeDly(100);
	uc = ReadByteHDMITX_CAT(REG_INT_CLR1);
	WriteByteHDMITX_CAT(REG_INT_CLR1, uc | B_CLR_VIDSTABLE);
	uc = 0;
	uc = ReadByteHDMITX_CAT(REG_SYS_STATUS);
	uc = ((uc & (~B_CLR_AUD_CTS)) | B_INTACTDONE);
	WriteByteHDMITX_CAT(REG_SYS_STATUS, uc);
	uc &= ~B_INTACTDONE;
	WriteByteHDMITX_CAT(REG_SYS_STATUS, uc);
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
//    WriteByteHDMITX_CAT(REG_AFE_DRV_CTRL, 0);   
	uc = 0;			//0x01 << O_AFE_DRV_SR;
	WriteByteHDMITX_CAT(REG_AFE_DRV_CTRL, uc);	//Slew rate control for output TMDS signals.
//    AVTimeDly(100) ;
}
int CAT_EnableVideoOutput(Hdmi_info_para_t * info) 
{
	int i;
	unsigned char uc = 0;
	hdmi_dbg_print("EnableVideoOutput()\n");
	
//    printf("EnableVideoOutput()\n") ;
	    if (info->output_state == CABLE_PLUGIN_HDMI_OUT)
		WriteByteHDMITX_CAT(REG_SW_RST,
				     (ReadByteHDMITX_CAT(REG_SW_RST)) &
				     (~
				      (B_REF_RST | B_AREF_RST | B_VID_RST |
				       B_HDMI_RST)));
	
	else if (info->output_state == CABLE_PLUGIN_DVI_OUT)
		 {
		hdmi_dbg_print("DVI output\n");
		WriteByteHDMITX_CAT(REG_SW_RST,
				     (ReadByteHDMITX_CAT(REG_SW_RST)) &
				     (~(B_REF_RST | B_VID_RST | B_HDMI_RST)));
		info->videopath_outindex = 0;
		}
	CAT_SetInputMode(info);
	CAT_SetCSCScale(info);
	CAT_SetHDMIMode(info);
	CAT_SetupAFE(info);	// pass if High Freq request    
	for (i = 0; i < 100; i++)
		 {
		if ((ReadByteHDMITX_CAT(REG_SYS_STATUS)) & B_TXVIDSTABLE)
			 {
			break;
			}
		AVTimeDly(1);
		}
	CAT_FireAFE();		//start AFE.
	
//    WriteByteHDMITX_CAT(REG_INT_CLR0, 0x0) ;
//    WriteByteHDMITX_CAT(REG_INT_CLR1, B_CLR_VIDSTABLE) ;
//    WriteByteHDMITX_CAT(REG_SYS_STATUS,B_INTACTDONE) ; // clear interrupt.
//    WriteByteHDMITX_CAT(REG_INT_MASK3, (ReadByteHDMITX_CAT(REG_INT_MASK3)) & (~B_VIDSTABLE_MASK)) ;
	    uc = ReadByteHDMITX_CAT(REG_CLK_CTRL1);
	
//    if(info->tv_mode != HDMI_480I_60HZ)
	    uc |= B_VDO_LATCH_EDGE;
	
//    else
//         uc &= ~B_VDO_LATCH_EDGE; 
	    WriteByteHDMITX_CAT(REG_CLK_CTRL1, uc);
	return 0;
}
void CAT_DisableVideoOutput(void) 
{
	WriteByteHDMITX_CAT(REG_SW_RST,
			     (ReadByteHDMITX_CAT(REG_SW_RST)) | B_VID_RST |
			     B_AUD_RST | B_AREF_RST | B_HDCP_RST);
	WriteByteHDMITX_CAT(REG_AFE_DRV_CTRL,
			     B_AFE_DRV_PWD | B_AFE_DRV_ENBIST);
} 
