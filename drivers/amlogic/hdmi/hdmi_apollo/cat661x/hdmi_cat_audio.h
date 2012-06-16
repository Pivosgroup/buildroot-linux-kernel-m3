#ifndef __HDMI_AUDIO_CAT_H__
#define __HDMI_AUDIO_CAT_H__
#include "hdmi_global.h"

//Sampling Freq Fs:0 - Refer to Stream Header; 1 - 32KHz; 2 - 44.1KHz; 3 - 48KHz; 4 - 88.2KHz...
typedef enum { FS_OTHER = 0, FS_32K = 1, FS_44K1 = 2, FS_48K =
	    3, FS_88K2 = 4, FS_96K = 5, FS_176K4 = 6, FS_192K = 7, 
} Audio_Fs_t;

// Audio Enable
#define ENABLE_SPDIF    (1<<4)
#define ENABLE_I2S_SRC3  (1<<3)
#define ENABLE_I2S_SRC2  (1<<2)
#define ENABLE_I2S_SRC1  (1<<1)
#define ENABLE_I2S_SRC0  (1<<0)
    
#define AUD_SWL_NOINDICATE  0x0
#define AUD_SWL_16          0x2
#define AUD_SWL_17          0xC
#define AUD_SWL_18          0x4
#define AUD_SWL_20          0xA	// for maximum 20 bit
#define AUD_SWL_21          0xD
#define AUD_SWL_22          0x5
#define AUD_SWL_23          0x9
#define AUD_SWL_24          0xB
extern int CAT_AdjustAudioSampleClock(Hdmi_info_para_t * info);
extern void CAT_SetNonPCMAudio(Hdmi_info_para_t * info);
extern void CAT_EnableAudioOutput(Hdmi_info_para_t * info);
extern void CAT_DisableAudioOutput(void);
extern void CAT_SetAudioChannel(Hdmi_info_para_t * info);

#endif	/*  */
