#ifndef __HDMI_EDID_CAT_H__
#define __HDMI_EDID_CAT_H__
#include "hdmi_global.h"

#define EDID_SLV 0xA0
    
// EDID Addresses
    
#define VER_ADDR 0x12
#define NUM_EXTENSIONS_ADDR 0x7E
    
#define EXTENSION_ADDR 0x80
#define CEA_DATA_BLOCK_COLLECTION_ADDR 0x84
    
#define EXTENSION_ADDR_1StP 0x00
#define CEA_DATA_BLOCK_COLLECTION_ADDR_1StP 0x04
    
#define VIDEO_TAG 0x40
#define AUDIO_TAG 0x20
#define VENDOR_TAG 0x60
#define SPEAKER_TAG 0x80
    typedef struct {
	unsigned char ValidCEA;
	 unsigned char ValidHDMI;
	 unsigned char VideoMode;
	 unsigned char VDOModeCount;
	 unsigned char idxNativeVDOMode;
	 unsigned char VDOMode[32];
	 unsigned char AUDDesCount;
	 unsigned long IEEEOUI;
} RX_CAP;
extern int CAT_ParseEDID(Hdmi_info_para_t * info);

#endif	/*  */
    
