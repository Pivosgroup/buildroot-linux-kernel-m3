#ifndef __HDMI_INFO_SET_CAT_H__
#define __HDMI_INFO_SET_CAT_H__

#include "hdmi_global.h"
    
#define VENDORSPEC_INFOFRAME_TYPE 0x01
#define AVI_INFOFRAME_TYPE  0x02
#define SPD_INFOFRAME_TYPE 0x03
#define AUDIO_INFOFRAME_TYPE 0x04
#define MPEG_INFOFRAME_TYPE 0x05
    
#define VENDORSPEC_INFOFRAME_VER 0x01
#define AVI_INFOFRAME_VER  0x02
#define SPD_INFOFRAME_VER 0x01
#define AUDIO_INFOFRAME_VER 0x01
#define MPEG_INFOFRAME_VER 0x01
    
#define VENDORSPEC_INFOFRAME_LEN 8
#define AVI_INFOFRAME_LEN 13
#define SPD_INFOFRAME_LEN 25
#define AUDIO_INFOFRAME_LEN 10
#define MPEG_INFOFRAME_LEN 10
    
#define ACP_PKT_LEN 9
#define ISRC1_PKT_LEN 16
#define ISRC2_PKT_LEN 16
typedef enum tagHDMI_Aspec { HDMI_4x3 = 0, HDMI_16x9 = 1 
} HDMI_Aspec;
typedef enum tagHDMI_Colorimetry { HDMI_ITU601 = 0, HDMI_ITU709 = 1 
} HDMI_Colorimetry;
struct VideoTiming {
	unsigned long VideoPixelClock;
	 unsigned char VIC;
	 unsigned char pixelrep;
};
typedef union _AVI_InfoFrame  {
	struct {
		unsigned char Type;
		 unsigned char Ver;
		 unsigned char Len;
		  unsigned char Scan:2;
		 unsigned char BarInfo:2;
		 unsigned char ActiveFmtInfoPresent:1;
		 unsigned char ColorMode:2;
		 unsigned char FU1:1;
		  unsigned char ActiveFormatAspectRatio:4;
		 unsigned char PictureAspectRatio:2;
		 unsigned char Colorimetry:2;
		  unsigned char Scaling:2;
		 unsigned char FU2:6;
		  unsigned char VIC:7;
		 unsigned char FU3:1;
		  unsigned char PixelRepetition:4;
		 unsigned char FU4:4;
		  unsigned short Ln_End_Top;
		 unsigned short Ln_Start_Bottom;
		 unsigned short Pix_End_Left;
		 unsigned short Pix_Start_Right;
	} info;
	 struct {
		unsigned char AVI_HB[3];
		 unsigned char AVI_DB[13];
	} pktbyte;
} AVI_InfoFrame;
typedef union _Audio_InfoFrame {
	struct {
		unsigned char Type;
		 unsigned char Ver;
		 unsigned char Len;
		  unsigned char AudioChannelCount:3;
		 unsigned char RSVD1:1;
		 unsigned char AudioCodingType:4;
		  unsigned char SampleSize:2;
		 unsigned char SampleFreq:3;
		 unsigned char Rsvd2:3;
		  unsigned char FmtCoding;
		  unsigned char SpeakerPlacement;
		  unsigned char Rsvd3:3;
		 unsigned char LevelShiftValue:4;
		 unsigned char DM_INH:1;
	} info;
	  struct {
		unsigned char AUD_HB[3];
		 unsigned char AUD_DB[10];
	} pktbyte;
 } Audio_InfoFrame;
extern void CAT_ConfigAVIInfoFrame(Hdmi_info_para_t * info);
extern void CAT_ConfigAudioInfoFrm(Hdmi_info_para_t * info);

#endif	/*  */
