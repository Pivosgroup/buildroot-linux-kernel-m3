#ifndef __HDMI_VIDEO_CAT_H__
#define __HDMI_VIDEO_CAT_H__

#include "hdmi_global.h"
    
#define T_MODE_CCIR656 (1<<0)
#define T_MODE_SYNCEMB (1<<1)
#define T_MODE_INDDR (1<<2)
    
#define SIZEOF_CSCMTX 18
#define SIZEOF_CSCGAIN 6
#define SIZEOF_CSCOFFSET 3
    
// sync embedded table setting, defined as comment.
    struct SyncEmbeddedSetting {
	
//    unsigned char fmt ;
	unsigned char RegHVPol;	// Reg90
	unsigned char RegHfPixel;	// Reg91
	unsigned char RegHSSL;	// Reg95
	unsigned char RegHSEL;	// Reg96
	unsigned char RegHSH;	// Reg97
	unsigned char RegVSS1;	// RegA0
	unsigned char RegVSE1;	// RegA1
	unsigned char RegVSS2;	// RegA2
	unsigned char RegVSE2;	// RegA3
	unsigned char REGHVPol656;	// Reg90 when CCIR656
	unsigned char REGHfPixel656;	//Reg91 when CCIR656
	unsigned char RegHSSL656;	// Reg95 when CCIR656
	unsigned char RegHSEL656;	// Reg96 when CCIR656
	unsigned char RegHSH656;	// Reg97 when CCIR656
	unsigned char VFreq;
	 unsigned long PCLK;
};
extern int CAT_ProgramSyncEmbeddedVideoMode(Hdmi_info_para_t * info);
extern void CAT_FireAFE(void);
extern int CAT_EnableVideoOutput(Hdmi_info_para_t * info);
extern void CAT_DisableVideoOutput(void);
extern void CAT_SetHDMIMode(Hdmi_info_para_t * info);

#endif	/*  */
    
