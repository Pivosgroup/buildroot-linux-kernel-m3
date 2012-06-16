#ifndef AVOS
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include "m1/hdmi_tx_reg.h"

#else
#include "ioapi.h"
#include <chipsupport/chipsupport.h>
#include <os/extend/interrupt.h>
#include <Drivers/include/peripheral_reg.h>
#include <Drivers/include/isa_reg.h>
#include <Drivers/include/mpeg_reg.h>
#include <interrupt.h>
#include "displaydev.h"
#include "policy.h"
#endif


#include "hdmi_tx_module.h"
#include "hdmi_info_global.h"

#define CEA_DATA_BLOCK_COLLECTION_ADDR_1StP 0x04
#define VIDEO_TAG 0x40
#define AUDIO_TAG 0x20
#define VENDOR_TAG 0x60
#define SPEAKER_TAG 0x80



#define HDMI_EDID_BLOCK_TYPE_RESERVED     0
#define HDMI_EDID_BLOCK_TYPE_AUDIO        1
#define HDMI_EDID_BLOCK_TYPE_VIDEO        2
#define HDMI_EDID_BLOCK_TYPE_VENDER       3
#define HDMI_EDID_BLOCK_TYPE_SPEAKER      4
#define HDMI_EDID_BLOCK_TYPE_VESA         5
#define HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG 7

//-----------------------------------------------------------
static int Edid_DecodeHeader(HDMI_TX_INFO_t *info, unsigned char *buff)
{
    int i, ret = 0;
//    UpdateCRC16WithBlock( pCRC16, 8, Data);
    if(!(buff[0] | buff[7]))
    {
        for(i = 1; i < 7; i++){
           if(buff[i]!= 0xFF)
           	{
			    info->output_state = CABLE_PLUGIN_DVI_OUT;
                ret = -1;
           	}
        }
    }
    else
    	{
		    info->output_state = CABLE_PLUGIN_DVI_OUT;
            ret = -1;
    	}
    return ret;
}

void Edid_DecodeStandardTiming(HDMI_TX_INFO_t * info, unsigned char * Data, unsigned char length)
{
     unsigned char  i, TmpVal;
     int hor_pixel, frame_rate;

     for(i = 0; i < length; i++ )
     {
        if((Data[i*2] != 0x01)&&(Data[i*2 + 1] != 0x01))   //else
        {
             hor_pixel = (int)((Data[i*2]+31)*8);
             TmpVal = Data[i*2 + 1] & 0xC0;

             frame_rate = (int)((Data[i*2 + 1])& 0x3F) + 60;

             if((hor_pixel == 720) && (frame_rate == 30))
             	{
             	  info->hdmi_sup_480i  = 1;
             	}

             else if((hor_pixel == 720) && (frame_rate == 25))
        	    {
             	  info->hdmi_sup_576i  = 1;
             	}

             else if((hor_pixel == 720) && (frame_rate == 60))
        	    {
             	  info->hdmi_sup_480p  = 1;
//             	  if(TmpVal==0x40)
//             	  	info->video_480p.support_4_3  = 1;
//             	  else if(TmpVal==0xc0)
//             	  	info->video_480p.support_16_9  = 1;
             	}

             else if((hor_pixel == 720) && (frame_rate == 50))
        	   {
             	  info->hdmi_sup_576p  = 1;
             	}

             else if((hor_pixel == 1280) && (frame_rate == 60))
             {
             	  info->hdmi_sup_720p_60hz  = 1;
             	}

             else if((hor_pixel == 1280) && (frame_rate == 50))
             {
             	  info->hdmi_sup_720p_50hz  = 1;
             	}

             else if((hor_pixel == 1920) && (frame_rate == 30))
             {
             	  info->hdmi_sup_1080i_60hz  = 1;
             	}

             else if((hor_pixel == 1920) && (frame_rate == 25))
             {
             	  info->hdmi_sup_1080i_50hz  = 1;
             	}

             else if((hor_pixel == 1920) && (frame_rate == 60))
             {
             	  info->hdmi_sup_1080p_60hz  = 1;
             	}

             else if((hor_pixel == 1920) && (frame_rate == 50))
             {
             	  info->hdmi_sup_1080p_50hz  = 1;
             	}
             else if((hor_pixel == 1920) && (frame_rate == 24))
             {
             	  info->hdmi_sup_1080p_24hz  = 1;
             	}
             else if((hor_pixel == 1920) && (frame_rate == 25))
             {
             	  info->hdmi_sup_1080p_25hz  = 1;
             	}
             else if((hor_pixel == 1920) && (frame_rate == 30))
             {
             	  info->hdmi_sup_1080p_30hz  = 1;
             	}

          }
     }
}

static unsigned char Edid_TimingDescriptors[204]=    //12x17
 {
 //pixel clk --hsync active & blank  -- vsync  active & blank-- hsync/vsync off & wid -- Image size
    0x8C,0x0A,  0xA0,0x14,0x51,     0xF0,0x16,0x00,     0x26,0x7c,0x43,0x00,    //0x13,0x8e,   //480i(4:3)
     0x8C,0x0A,  0xA0,0x14,0x51,     0xF0,0x16,0x00,     0x26,0x7c,0x43,0x00,   // 0xc4,0x8e,   //480i(16:9)
     0x8C,0x0A,  0xA0,0x20,0x51,     0x20,0x18,0x10,     0x18,0x7e,0x23,0x00,   // 0x13,0x8e,   //576i (4:3)
     0x8C,0x0A,  0xA0,0x20,0x51,     0x20,0x18,0x10,     0x18,0x7e,0x23,0x00,   // 0xc4,0x8e,   //576i (16:9)
     0x8C,0x0A,  0xD0,0x8A,0x20,     0xE0,0x2D,0x10,     0x10,0x3e,0x96,0x00,   // 0x13,0x8e,   //480p (4:3)
     0x8C,0x0A,  0xD0,0x8A,0x20,     0xE0,0x2D,0x10,     0x10,0x3e,0x96,0x00,   // 0xc4,0x8e,   //480p (16:9)
     0x8C,0x0A,  0xD0,0x90,0x20,     0x40,0x31,0x20,     0x0c,0x40,0x55,0x00,   // 0x13,0x8e,   //576p  (4:3)
     0x8C,0x0A,  0xD0,0x90,0x20,     0x40,0x31,0x20,     0x0c,0x40,0x55,0x00,   // 0xc4,0x8e,  //576p  (16:9)
    0x01,0x1D,    0x00,0x72,0x51,     0xD0,0x1E,0x20,     0x6e,0x28,0x55,0x00,  //  0xc4,0x8e,  //720p60(16:9)
    0x01,0x1D,    0x00,0xBC,0x52,     0xD0,0x1E,0x20,     0xb8,0x28,0x55,0x40,  //  0xc4,0x8e,  //720p50 (16:9)
    0x01,0x1D,    0x80,0x18,0x71,     0x1C,0x16,0x20,     0x58,0x2c,0x25,0x00,  //  0xc4,0x8e,  //1080i60 (16:9)
    0x01,0x1D,    0x80,0xD0,0x72,     0x1C,0x16,0x20,     0x10,0x2c,0x25,0x80,  //  0xc4,0x8e,   //1080i50 (16:9)
    0x02,0x3a,    0x80,0x18,0x71,     0x38,0x2d,0x40,     0x58,0x2c,0x45,0x00,  //  0xc4,0x8e,  //1080p60 (16:9)
    0x02,0x3a,    0x80,0xD0,0x72,     0x38,0x2d,0x40,     0x10,0x2c,0x45,0x80,  //  0xc4,0x8e ,  //1080p50 (16:9)
    0xfa,0x1c,    0x80,0x3e,0x73,     0x38,0x2d,0x40,     0x7e,0x2c,0x45,0x80,  //  0xc4,0x8e,  //1080p24 (16:9)
    0x01,0x1D,    0x80,0xD0,0x72,     0x38,0x2d,0x40,     0x10,0x2c,0x45,0x80,  //  0xc4,0x8e,   //1080p25 (16:9)
    0x01,0x1D,    0x80,0x18,0x71,     0x38,0x2d,0x40,     0x58,0x2c,0x45,0x00,  //  0xc4,0x8e,  //1080p30 (16:9)

 };

//-----------------------------------------------------------
void Edid_CompareTimingDescriptors(HDMI_TX_INFO_t * info, unsigned char *Data)
{
   int index1,index2;

	for(index1=0;index1<17;index1++)
    {
        for(index2=0;index2<12;index2++)
        {
            if(Data[index2]!=Edid_TimingDescriptors[index1*14+index2])
                break;
        }
        if(index2==12)
        {
            switch(index1)
            {
                case 0:
                case 1:
                    info->hdmi_sup_480i  = 1;
                    break;

                case 2:
                case 3:
             	    info->hdmi_sup_576i  = 1;
                   break;

                case 4:
                case 5:
             	    info->hdmi_sup_480p  = 1;
//                  if((Data[12]==Edid_TimingDescriptors[4*14 + 12]) && (Data[13]==Edid_TimingDescriptors[4*14 + 13]))
//                     info->video_480p.support_4_3  = 1;
//                  else if((Data[12]==Edid_TimingDescriptors[5*14 + 12]) && (Data[13]==Edid_TimingDescriptors[5*14 + 13]))
//                     info->video_480p.support_16_9  = 1;
                    break;

                case 6:
                case 7:
             	    info->hdmi_sup_576p  = 1;
                    break;

                case 8:
             	    info->hdmi_sup_720p_60hz  = 1;
             	  	break;

	            case 9:
             	    info->hdmi_sup_720p_50hz  = 1;
             	  	break;

                case 10:
             	    info->hdmi_sup_1080i_60hz  = 1;
             	  	break;

                case 11:
             	    info->hdmi_sup_1080i_50hz  = 1;
                    break;

                case 12:
             	    info->hdmi_sup_1080p_60hz  = 1;
					break;

                case 13:
             	    info->hdmi_sup_1080p_50hz  = 1;
					break;

                case 14:
             	    info->hdmi_sup_1080p_24hz  = 1;
                    break;

                case 15:
             	    info->hdmi_sup_1080p_25hz  = 1;
                    break;

                case 16:
             	    info->hdmi_sup_1080p_30hz  = 1;
                    break;
                default:
                    break;
            }
			break;
        }
    }
}


//-----------------------------------------------------------
void Edid_ParseCEADetailedTimingDescriptors(HDMI_TX_INFO_t * info, unsigned char blk_mun, unsigned char BaseAddr, unsigned char *buff)
{
	unsigned char index_edid;

	for( index_edid = 0; index_edid < blk_mun; index_edid++)
    {
        Edid_CompareTimingDescriptors(info, &buff[BaseAddr]);
        BaseAddr += 18;
       if((BaseAddr + 18) > 0x7d)   //there is not the TimingDescriptors
				break;
      }

}
static vsdb_phy_addr_t vsdb_local = {0};
int get_vsdb_phy_addr(vsdb_phy_addr_t * vsdb)
{
    vsdb = &vsdb_local;
    return vsdb->valid;
}

void set_vsdb_phy_addr(vsdb_phy_addr_t * vsdb, unsigned char *edid_offset)
{
	vsdb->a = (edid_offset[4] >> 4 ) & 0xf;
	vsdb->b = (edid_offset[4] >> 0 ) & 0xf;
	vsdb->c = (edid_offset[5] >> 4 ) & 0xf;
	vsdb->d = (edid_offset[5] >> 0 ) & 0xf;
	vsdb_local = *vsdb;
	vsdb->valid = 1;
}

int Edid_Parse_check_HDMI_VSDB(HDMI_TX_INFO_t * info, unsigned char *buff)
{
	unsigned char  VSpecificBoundary, BlockAddr,  len;
	int temp_addr=0;
	VSpecificBoundary = buff[2] ;
	if(VSpecificBoundary < 4)
	{
		info->output_state = CABLE_PLUGIN_DVI_OUT;
		return -1;
	}
	BlockAddr = CEA_DATA_BLOCK_COLLECTION_ADDR_1StP;
	while( BlockAddr < VSpecificBoundary) {
		len = buff[BlockAddr] & 0x1F;
             if((buff[BlockAddr] & 0xE0)== VENDOR_TAG){		//find the HDMI Vendor Specific Data Block
                	break;
		}
		temp_addr = 	BlockAddr + len + 1;
		if(temp_addr >= VSpecificBoundary)
			break;
		BlockAddr = BlockAddr + len + 1;
	}

    set_vsdb_phy_addr(&info->vsdb_phy_addr, &buff[BlockAddr]);
	if(info->vsdb_phy_addr.a == 0) {
		printk("CEC: not a valid physical address\n");
	}
    else {
        vsdb_phy_addr_t *tmp = &info->vsdb_phy_addr;
        if(tmp->valid){
            printk("CEC: Physical address: %1x.%1x.%1x.%1x\n", tmp->a, tmp->b, tmp->c, tmp->d);
        }
    }

	//For test only.
	hdmi_print(0,"HDMI DEBUG [%s]\n", __FUNCTION__);
    hdmi_print(0,"max_tmds_clk_7:%d\n",buff[BlockAddr + 7]);
    hdmi_print(0,"Field 8:%d\n",buff[BlockAddr + 8]);
    hdmi_print(0,"video_latency_9:%d\n",buff[BlockAddr + 9]);
    hdmi_print(0,"audio_latency_10:%d\n",buff[BlockAddr + 10]);

	if(temp_addr >= VSpecificBoundary)
	{
		info->output_state = CABLE_PLUGIN_DVI_OUT;
		return -1;
	}
	else
	{
		if((buff[BlockAddr + 1]!= 0x03)||(buff[BlockAddr + 2]!= 0x0C)||(buff[BlockAddr + 3]!= 0x0))
		{
			info->output_state = CABLE_PLUGIN_DVI_OUT;
			return -1;
		}
	}
	return 0;
}

//-----------------------------------------------------------
void Edid_MonitorCapable861(HDMI_TX_INFO_t * info, unsigned char edid_flag)
{
     if(edid_flag & 0x80)
        info->support_underscan_flag = 1;
     if(edid_flag & 0x40)
        info->support_basic_audio_flag =1;
     if(edid_flag & 0x20)
     	{
        	info->support_ycbcr444_flag =1;
//        	info->videopath_outindex = 1;    // Video Output Color Space Conversion : 0 - RGB out; 1 - YCbr (4:4:4) out; 2 - YCbr (4:2:2) out;
      }
     if(edid_flag & 0x10)
     	{
        	info->support_ycbcr422_flag =1 ;
//       	 if(!(edid_flag & 0x20))
//        		info->videopath_outindex = 2;    // Video Output Color Space Conversion : 0 - RGB out; 1 - YCbr (4:4:4) out; 2 - YCbr (4:2:2) out;
      }
}


//-----------------------------------------------------------
static void Edid_ParsingVideoDATABlock(HDMI_TX_INFO_t * info, unsigned char *buff, unsigned char BaseAddr, unsigned char NBytes)
{
    unsigned char i;
    NBytes &= 0x1F;
    for(i = 0; i < NBytes; i++)
      {
		switch(buff[i + BaseAddr]&0x7F)
		{
            case 6:
            case 7:
                info->hdmi_sup_480i  = 1;
                break;

            case 21:
            case 22:
                info->hdmi_sup_576i  = 1;
			  	break;

            case 2:
            case 3:
                info->hdmi_sup_480p  = 1;
				break;

            case 17:
            case 18:
                info->hdmi_sup_576p  = 1;
                break;

            case 4:
                info->hdmi_sup_720p_60hz  = 1;
                break;

            case 19:
                info->hdmi_sup_720p_50hz  = 1;
                break;

            case 5:
                info->hdmi_sup_1080i_60hz  = 1;
                break;

            case 20:
               info->hdmi_sup_1080i_50hz  = 1;
               break;

            case 16:
                info->hdmi_sup_1080p_60hz  = 1;
                break;

            case 31:
                info->hdmi_sup_1080p_50hz  = 1;
                break;

            case 32:
                info->hdmi_sup_1080p_24hz  = 1;
                break;

            case 33:
                info->hdmi_sup_1080p_25hz  = 1;
                break;
            case 34:
                info->hdmi_sup_1080p_30hz  = 1;
                break;
            default:
                break;
        }
    }
}

//-----------------------------------------------------------
static void Edid_ParsingAudioDATABlock(HDMI_TX_INFO_t * info, unsigned char *Data, unsigned char BaseAddr, unsigned char NBytes)
{
	 unsigned char AudioFormatCode;
	 int i = BaseAddr ;
   NBytes&=0x1F;
   do{
        AudioFormatCode = (Data[i]&0xF8)>>3;
        switch(AudioFormatCode)
          {
                case 1:
       	              info->tv_audio_info._60958_PCM.support_flag = 1;
       	              info->tv_audio_info._60958_PCM.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._60958_PCM._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._60958_PCM._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._60958_PCM._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._60958_PCM._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._60958_PCM._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._60958_PCM._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._60958_PCM._32k = 1;
       	              if((Data[i+2]&0x04))
       	              	info->tv_audio_info._60958_PCM._24bit = 1;
       	              if((Data[i+2]&0x02))
       	              	info->tv_audio_info._60958_PCM._20bit = 1;
       	              if((Data[i+2]&0x01))
       	              	info->tv_audio_info._60958_PCM._16bit = 1;
                     	break;

                case 2:
       	              info->tv_audio_info._AC3.support_flag = 1;
       	              info->tv_audio_info._AC3.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._AC3._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._AC3._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._AC3._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._AC3._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._AC3._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._AC3._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._AC3._32k = 1;
       	              info->tv_audio_info._AC3._max_bit = Data[i+2];
                     	break;

                case 3:
       	              info->tv_audio_info._MPEG1.support_flag = 1;
       	              info->tv_audio_info._MPEG1.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._MPEG1._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._MPEG1._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._MPEG1._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._MPEG1._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._MPEG1._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._MPEG1._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._MPEG1._32k = 1;
       	              info->tv_audio_info._MPEG1._max_bit = Data[i+2];
                     	break;

                case 4:
       	              info->tv_audio_info._MP3.support_flag = 1;
       	              info->tv_audio_info._MP3.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._MP3._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._MP3._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._MP3._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._MP3._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._MP3._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._MP3._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._MP3._32k = 1;
       	              info->tv_audio_info._MP3._max_bit = Data[i+2];
                     	break;

                case 5:
        	            info->tv_audio_info._MPEG2.support_flag = 1;
       	              info->tv_audio_info._MPEG2.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._MPEG2._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._MPEG2._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._MPEG2._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._MPEG2._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._MPEG2._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._MPEG2._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._MPEG2._32k = 1;
       	              info->tv_audio_info._MPEG2._max_bit = Data[i+2];
                     	break;

                case 6:
        	            info->tv_audio_info._AAC.support_flag = 1;
       	              info->tv_audio_info._AAC.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._AAC._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._AAC._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._AAC._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._AAC._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._AAC._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._AAC._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._AAC._32k = 1;
       	              info->tv_audio_info._AAC._max_bit = Data[i+2];
                     	break;

                case 7:
        	            info->tv_audio_info._DTS.support_flag = 1;
       	              info->tv_audio_info._DTS.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._DTS._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._DTS._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._DTS._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._DTS._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._DTS._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._DTS._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._DTS._32k = 1;
       	              info->tv_audio_info._DTS._max_bit = Data[i+2];
                     	break;

                case 8:
        	            info->tv_audio_info._ATRAC.support_flag = 1;
       	              info->tv_audio_info._ATRAC.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._ATRAC._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._ATRAC._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._ATRAC._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._ATRAC._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._ATRAC._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._ATRAC._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._ATRAC._32k = 1;
       	              info->tv_audio_info._ATRAC._max_bit = Data[i+2];
                     	break;

                case 9:
        	            info->tv_audio_info._One_Bit_Audio.support_flag = 1;
       	              info->tv_audio_info._One_Bit_Audio.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._One_Bit_Audio._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._One_Bit_Audio._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._One_Bit_Audio._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._One_Bit_Audio._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._One_Bit_Audio._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._One_Bit_Audio._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._One_Bit_Audio._32k = 1;
       	              info->tv_audio_info._One_Bit_Audio._max_bit = Data[i+2];
                     	break;

                case 10:
        	            info->tv_audio_info._Dolby.support_flag = 1;
       	              info->tv_audio_info._Dolby.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._Dolby._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._Dolby._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._Dolby._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._Dolby._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._Dolby._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._Dolby._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._Dolby._32k = 1;
       	              info->tv_audio_info._Dolby._max_bit = Data[i+2];
                     	break;

                case 11:
        	            info->tv_audio_info._DTS_HD.support_flag = 1;
       	              info->tv_audio_info._DTS_HD.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._DTS_HD._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._DTS_HD._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._DTS_HD._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._DTS_HD._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._DTS_HD._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._DTS_HD._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._DTS_HD._32k = 1;
       	              info->tv_audio_info._DTS_HD._max_bit = Data[i+2];
                     	break;


                case 12:
        	            info->tv_audio_info._MAT.support_flag = 1;
       	              info->tv_audio_info._MAT.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._MAT._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._MAT._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._MAT._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._MAT._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._MAT._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._MAT._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._MAT._32k = 1;
       	              info->tv_audio_info._MAT._max_bit = Data[i+2];
                     	break;

                case 13:
        	            info->tv_audio_info._ATRAC.support_flag = 1;
       	              info->tv_audio_info._ATRAC.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._DST._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._DST._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._DST._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._DST._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._DST._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._DST._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._DST._32k = 1;
       	              info->tv_audio_info._DST._max_bit = Data[i+2];
                     	break;

                case 14:
        	            info->tv_audio_info._WMA.support_flag = 1;
       	              info->tv_audio_info._WMA.max_channel_num = (Data[i]&0x07);
       	              if((Data[i+1]&0x40))
       	              	info->tv_audio_info._WMA._192k = 1;
       	              if((Data[i+1]&0x20))
       	              	info->tv_audio_info._WMA._176k = 1;
       	              if((Data[i+1]&0x10))
       	              	info->tv_audio_info._WMA._96k = 1;
       	              if((Data[i+1]&0x08))
       	              	info->tv_audio_info._WMA._88k = 1;
       	              if((Data[i+1]&0x04))
       	              	info->tv_audio_info._WMA._48k = 1;
       	              if((Data[i+1]&0x02))
       	              	info->tv_audio_info._WMA._44k = 1;
       	              if((Data[i+1]&0x01))
       	              	info->tv_audio_info._WMA._32k = 1;
       	              info->tv_audio_info._WMA._max_bit = Data[i+2];
                     	break;

                default:
       	              break;
                 }
                i+=3;
      }while (i < (NBytes + BaseAddr));
}

//-----------------------------------------------------------
static void Edid_ParsingSpeakerDATABlock(HDMI_TX_INFO_t * info, unsigned char *buff, unsigned char BaseAddr)
{
   int ii;
   for(ii = 1; ii < 0x80; )
   {
     switch(buff[BaseAddr] & ii)
      {
        	case 0x40:
        		info->tv_audio_info.speaker_allocation.rlc_rrc = 1;
        		break;

        	case 0x20:
        		info->tv_audio_info.speaker_allocation.flc_frc = 1;
        		break;

        	case 0x10:
        		info->tv_audio_info.speaker_allocation.rc = 1;
        		break;

        	case 0x08:
        		info->tv_audio_info.speaker_allocation.rl_rr = 1;
        		break;

        	case 0x04:
        		info->tv_audio_info.speaker_allocation.fc = 1;
        		break;

        	case 0x02:
        		info->tv_audio_info.speaker_allocation.lfe = 1;
        		break;

        	case 0x01:
        		info->tv_audio_info.speaker_allocation.fl_fr = 1;
        		break;

          default :
          	break;
       }
       ii = ii << 1;
     }
}



//-----------------------------------------------------------
int Edid_ParsingCEADataBlockCollection(HDMI_TX_INFO_t * info, unsigned char *buff)
{
   unsigned char AddrTag, D, Addr, Data;
   int temp_addr;

   D = buff[2];   //Byte number offset d where Detailed Timing data begins
   Addr = 4;

   AddrTag = Addr;
   do{
        Data = buff[AddrTag];
        switch(Data&0xE0)
        {
            case VIDEO_TAG:
                if((Addr + (Data&0x1f)) < D)
              	    Edid_ParsingVideoDATABlock(info, buff, Addr + 1, (Data & 0x1F) );
                break;

            case AUDIO_TAG:
                if((Addr + (Data&0x1f)) < D)
              	    Edid_ParsingAudioDATABlock(info, buff, Addr + 1, (Data & 0x1F) );
                break;

            case SPEAKER_TAG:
                if((Addr + (Data&0x1f)) < D)
              	    Edid_ParsingSpeakerDATABlock(info, buff, Addr + 1 );
                break;

            case VENDOR_TAG:
                if((Addr + (Data&0x1f)) < D)
                {
           	  	    if((buff[Addr + 1] != 0x03) || (buff[Addr + 2] != 0x0c) || (buff[Addr + 3] != 0x00))
           	  	    {
           	  	 	    info->auth_state = HDCP_NO_AUTH ;
           	  	 	    info->output_state = CABLE_PLUGIN_DVI_OUT;
           	  	    }
          		    if((Data&0x1f) > 5)
          		    {
            		 //A Source shall not transmit an ISRC1 or ISRC2 Packet to a Sink that does not have Supports_AI = 1
            		 //International Standard Recording Code (ISRC)
          			    if(buff[Addr + 6] & 0x80)
          				    info->support_ai_flag = 1;
          		    }
                }
                break;

            default:
                break;
          }
          Addr += ( Data & 0x1F ) ;   // next Tag Address
          AddrTag = ++Addr;
		temp_addr =   Addr + ( Data & 0x1F ) ;
		if(temp_addr >= D)	//force to break;
			break;
     }while (Addr < D);

   return 0;
}

//-----------------------------------------------------------
static int hdmitx_edid_block_parse(hdmitx_dev_t* hdmitx_device, unsigned char *BlockBuf)
{
    unsigned char offset,End ;
    unsigned char count ;
    unsigned char tag ;
    int i ;
    rx_cap_t* pRXCap = &(hdmitx_device->RXCap);

    if( BlockBuf[0] != 0x02 )
        return -1 ; // not a CEA BLOCK.
    End = BlockBuf[2]  ; // CEA description.
    pRXCap->native_Mode = BlockBuf[3] ;

	  pRXCap->VIC_count = 0 ;
    pRXCap->native_VIC = 0xff ;
    
    for( offset = 4 ; offset < End ; ){
        tag = BlockBuf[offset] >> 5 ;
        count = BlockBuf[offset] & 0x1f ;
        switch( tag ){
            case HDMI_EDID_BLOCK_TYPE_AUDIO: 
                pRXCap->AUD_count = count/3 ;
                offset++ ;
                for( i = 0 ; i < pRXCap->AUD_count ; i++, offset+=3 )
                {
                    pRXCap->RxAudioCap[i].audio_format_code = (BlockBuf[offset]>>3)&0xf;
                    pRXCap->RxAudioCap[i].channel_num_max = BlockBuf[offset]&0x7;
                    pRXCap->RxAudioCap[i].freq_cc = BlockBuf[offset+1]&0x7f;
                    pRXCap->RxAudioCap[i].cc3 = BlockBuf[offset+2]&0x7;
                }
                break ;
            
            case HDMI_EDID_BLOCK_TYPE_VIDEO: 
                offset ++ ;
                for( i = 0 ; i < count ; i++, offset++ )
                {
                    unsigned char VIC ;
                    VIC = BlockBuf[offset] & (~0x80) ;
                    pRXCap->VIC[pRXCap->VIC_count] = VIC ;
                    if( BlockBuf[offset] & 0x80 ){
                        pRXCap->native_VIC = VIC;
                    }
                    pRXCap->VIC_count++ ;
                }
                break ;
            
            case HDMI_EDID_BLOCK_TYPE_VENDER: 
                offset ++ ;
                pRXCap->IEEEOUI = (unsigned long)BlockBuf[offset+2] ;
                pRXCap->IEEEOUI <<= 8 ;
                pRXCap->IEEEOUI += (unsigned long)BlockBuf[offset+1] ;
                pRXCap->IEEEOUI <<= 8 ;
                pRXCap->IEEEOUI += (unsigned long)BlockBuf[offset] ;
                /**/
                hdmi_print(0, "HDMI_EDID_BLOCK_TYPE_VENDER: IEEEOUI %x:", pRXCap->IEEEOUI);
                for(i = 0; i<count ;i++){
                    hdmi_print(0, "%d: %02x\n",i+1, BlockBuf[offset+i]);
                }
                /**/
                pRXCap->ColorDeepSupport = (unsigned long)BlockBuf[offset+5];
                pRXCap->Max_TMDS_Clock = (unsigned long)BlockBuf[offset+6]; 
                offset += count ; // ignore the remaind.
                break ;
            
            case HDMI_EDID_BLOCK_TYPE_SPEAKER: 
                offset ++ ;
                pRXCap->RxSpeakerAllocation = BlockBuf[offset] ;
                offset += 3 ;
                break ;

            case HDMI_EDID_BLOCK_TYPE_VESA: 
                offset += count+1 ;
                break ;

            case HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG: 
                offset += count+1 ; //ignore
                break ;

            default:
                offset += count+1 ; // ignore
        }
    }
    hdmitx_device->vic_count=pRXCap->VIC_count;
    return 0 ;
}



int hdmitx_edid_parse(hdmitx_dev_t* hdmitx_device)
{
    unsigned char CheckSum ;
    unsigned char BlockCount ;
    unsigned char* EDID_buf = hdmitx_device->EDID_buf;
    int i, j, ret_val ;
    hdmi_print(0, "EDID Parser:\n");
    ret_val = Edid_DecodeHeader(&hdmitx_device->hdmi_info, &EDID_buf[0]);

//    if(ret_val == -1)
//        return -1;

    for( i = 0, CheckSum = 0 ; i < 128 ; i++ )
    {
        CheckSum += EDID_buf[i] ;
        CheckSum &= 0xFF ;
    }
    
    if( CheckSum != 0 )
    {
        hdmitx_device->hdmi_info.output_state = CABLE_PLUGIN_DVI_OUT;
        hdmi_print(0, "PLUGIN_DVI_OUT\n");
//        return -1 ;
    }
	
    Edid_DecodeStandardTiming(&hdmitx_device->hdmi_info, &EDID_buf[26], 8);
    Edid_ParseCEADetailedTimingDescriptors(&hdmitx_device->hdmi_info, 4, 0x36, &EDID_buf[0]);

    BlockCount = EDID_buf[0x7E] ;

    if( BlockCount == 0 ){
        hdmitx_device->hdmi_info.output_state = CABLE_PLUGIN_DVI_OUT;
        hdmi_print(0, "EDID BlockCount=0\n");
        return 0 ; // do nothing.
    }

    else if ( BlockCount > EDID_MAX_BLOCK )
    {
        BlockCount = EDID_MAX_BLOCK ;
    }
        	
    for( i = 1 ; i <= BlockCount ; i++ )
    {

        if((BlockCount > 1) && (i == 1))
        {
                CheckSum = 0;       //ignore the block1 data
        }
        else
        {
            if(((BlockCount == 1) && (i == 1)) || ((BlockCount > 1) && (i == 2)))
                Edid_Parse_check_HDMI_VSDB( &hdmitx_device->hdmi_info,  &EDID_buf[i * 128]);

            for( j = 0, CheckSum = 0 ; j < 128 ; j++ )
            {
                CheckSum += EDID_buf[i*128 + j] ;
                CheckSum &= 0xFF ;
            }
            if( CheckSum != 0 )
            {
                    hdmitx_device->hdmi_info.output_state = CABLE_PLUGIN_DVI_OUT;
            }
            else
            {
                Edid_MonitorCapable861(&hdmitx_device->hdmi_info, EDID_buf[i * 128 + 3]);
                ret_val = Edid_ParsingCEADataBlockCollection(&hdmitx_device->hdmi_info, &EDID_buf[i * 128]);
                Edid_ParseCEADetailedTimingDescriptors(&hdmitx_device->hdmi_info, 5, EDID_buf[i * 128 + 2], &EDID_buf[i * 128]);
                if(hdmitx_device->hdmi_info.output_state != CABLE_PLUGIN_DVI_OUT)
                    hdmitx_device->hdmi_info.output_state = CABLE_PLUGIN_HDMI_OUT;

            }

        }

        if( EDID_buf[i*128+0] == 0x2  )
        {
            if(hdmitx_edid_block_parse(hdmitx_device, &(EDID_buf[i*128]))>=0){
                if(hdmitx_device->RXCap.IEEEOUI==0x0c03){
                    break;
                }
            }
        }
    }
#if 1    
    i=hdmitx_edid_dump(hdmitx_device, (char*)(hdmitx_device->tmp_buf), HDMI_TMP_BUF_SIZE);
    hdmitx_device->tmp_buf[i]=0;
    hdmi_print_buf((char*)(hdmitx_device->tmp_buf), strlen((char*)(hdmitx_device->tmp_buf)));
    hdmi_print(0,"\n");
#endif    
    return 0;

}

typedef struct{
    const char* disp_mode;
    HDMI_Video_Codes_t VIC;
}dispmode_vic_t;

static dispmode_vic_t dispmode_VIC_tab[]=
{
    {"480i", HDMI_480i60}, 
    {"480i", HDMI_480i60_16x9},
    {"480p", HDMI_480p60},
    {"480p", HDMI_480p60_16x9},
    {"576i", HDMI_576i50},
    {"576i", HDMI_576i50_16x9},
    {"576p", HDMI_576p50},
    {"576p", HDMI_576p50_16x9},
    {"720p", HDMI_720p60},
    {"1080i", HDMI_1080i60},
    {"1080p", HDMI_1080p60},
    {"1080P30", HDMI_1080p30},
    {"1080P24", HDMI_1080p24},
    {"720p50hz", HDMI_720p50},
    {"1080i50hz", HDMI_1080i50},
    {"1080p50hz", HDMI_1080p50},
};    

HDMI_Video_Codes_t hdmitx_edid_get_VIC(hdmitx_dev_t* hdmitx_device, const char* disp_mode, char force_flag)
{
    rx_cap_t* pRXCap = &(hdmitx_device->RXCap);
	  int  i,j;
#ifndef AVOS
	  int count=ARRAY_SIZE(dispmode_VIC_tab);
#else
    int count=sizeof(dispmode_VIC_tab)/sizeof(dispmode_vic_t);
#endif	  
	  HDMI_Video_Codes_t vic=HDMI_Unkown;
    int mode_name_len=0;
    //printk("disp_mode is %s\n", disp_mode);
    for(i=0;i<count;i++)
    {
        if(strncmp(disp_mode, dispmode_VIC_tab[i].disp_mode, strlen(dispmode_VIC_tab[i].disp_mode))==0)
        {
            if((vic==HDMI_Unkown)||(strlen(dispmode_VIC_tab[i].disp_mode)>mode_name_len)){
                vic = dispmode_VIC_tab[i].VIC;
                mode_name_len = strlen(dispmode_VIC_tab[i].disp_mode);
            }
        }
    }
    if(vic!=HDMI_Unkown){
        if(force_flag==0){
            for( j = 0 ; j < pRXCap->VIC_count ; j++ ){
                if(pRXCap->VIC[j]==vic)
                    break;    
            }
            if(j>=pRXCap->VIC_count){
                vic = HDMI_Unkown;
            }
        }
    }    
    return vic;
}    

char* hdmitx_edid_get_native_VIC(hdmitx_dev_t* hdmitx_device)
{
    rx_cap_t* pRXCap = &(hdmitx_device->RXCap);
	  int  i;
#ifndef AVOS
	  int count=ARRAY_SIZE(dispmode_VIC_tab);
#else
    int count=sizeof(dispmode_VIC_tab)/sizeof(dispmode_vic_t);
#endif	  

	  char* disp_mode_ret=NULL;
    for(i=0;i<count;i++){
        if(pRXCap->native_VIC==dispmode_VIC_tab[i].VIC){
            disp_mode_ret = (char*)(dispmode_VIC_tab[i].disp_mode);
            break;    
        }
    }    
    return disp_mode_ret;
}    

#define EDID_RAM_ADDR_SIZE      (4*128)
//Clear HDMI Hardware Module EDID RAM and EDID Buffer
void hdmitx_edid_ram_buffer_clear(hdmitx_dev_t* hdmitx_device)
{
    unsigned int i = 0;
    
    //Clear HDMI Hardware Module EDID RAM
    for(i = 0; i < EDID_RAM_ADDR_SIZE; i++)
    {
        hdmi_wr_reg(TX_RX_EDID_OFFSET + i, 0);
    }
    //Clear EDID Buffer
    for(i = 0; i < EDID_MAX_BLOCK*128; i++)
    {
        hdmitx_device->EDID_buf[i] = 0;
    }
}

//Clear the Parse result of HDMI Sink's EDID.
void hdmitx_edid_clear(hdmitx_dev_t* hdmitx_device)
{
    rx_cap_t* pRXCap = &(hdmitx_device->RXCap);
    hdmitx_device->vic_count=0;
    pRXCap->VIC_count = 0;
    pRXCap->AUD_count = 0;
    pRXCap->IEEEOUI = 0;
    pRXCap->native_Mode = 0;
    pRXCap->native_VIC = 0xff;
    pRXCap->RxSpeakerAllocation = 0;
    hdmitx_device->hdmi_info.vsdb_phy_addr.a = 0;
    hdmitx_device->hdmi_info.vsdb_phy_addr.b = 0;
    hdmitx_device->hdmi_info.vsdb_phy_addr.c = 0;
    hdmitx_device->hdmi_info.vsdb_phy_addr.d = 0;
    hdmitx_device->hdmi_info.vsdb_phy_addr.valid = 0;
    memset(&vsdb_local, 0, sizeof(vsdb_phy_addr_t));    
}

int hdmitx_edid_dump(hdmitx_dev_t* hdmitx_device, char* buffer, int buffer_len)
{
    int i,pos=0;
    rx_cap_t* pRXCap = &(hdmitx_device->RXCap);
    pos+=snprintf(buffer+pos, buffer_len-pos, "EDID block number: 0x%x\r\n",hdmitx_device->EDID_buf[0x7e]);

    pos+=snprintf(buffer+pos, buffer_len-pos, "Source Physical Address[a.b.c.d]: %x.%x.%x.%x\r\n",
    	hdmitx_device->hdmi_info.vsdb_phy_addr.a, hdmitx_device->hdmi_info.vsdb_phy_addr.b, hdmitx_device->hdmi_info.vsdb_phy_addr.c, hdmitx_device->hdmi_info.vsdb_phy_addr.d);

    pos+=snprintf(buffer+pos, buffer_len-pos, "native Mode %x, VIC (native %d):\r\n",
        pRXCap->native_Mode, pRXCap->native_VIC);

    pos+=snprintf(buffer+pos, buffer_len-pos, "ColorDeepSupport %x, MaxTMDSClock %d\r\n",
        pRXCap->ColorDeepSupport, pRXCap->Max_TMDS_Clock); 

    for( i = 0 ; i < pRXCap->VIC_count ; i++ )
    {
        pos+=snprintf(buffer+pos, buffer_len-pos,"%d ", pRXCap->VIC[i]);
    }
    pos+=snprintf(buffer+pos, buffer_len-pos,"\r\n");
    pos+=snprintf(buffer+pos, buffer_len-pos, "Audio {format, channel, freq, cce}\r\n");
    for( i =0; i< pRXCap->AUD_count; i++)
    {
        pos+=snprintf(buffer+pos, buffer_len-pos, "{%d, %d, %x, %x}\r\n", pRXCap->RxAudioCap[i].audio_format_code,
            pRXCap->RxAudioCap[i].channel_num_max, pRXCap->RxAudioCap[i].freq_cc, pRXCap->RxAudioCap[i].cc3);
    }
    pos+=snprintf(buffer+pos,buffer_len-pos,"Speaker Allocation: %x\r\n", pRXCap->RxSpeakerAllocation);
    pos+=snprintf(buffer+pos,buffer_len-pos,"Vendor: %x\r\n", pRXCap->IEEEOUI);
    return pos;        
}    

