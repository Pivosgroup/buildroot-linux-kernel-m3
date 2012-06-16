#ifndef __HDMI_GLOBAL_H__
#define __HDMI_GLOBAL_H__

#define TRUE		0
#define FALSE 	-1
#include <linux/delay.h>
#include <asm/types.h>
typedef enum { HDMI_480I_60HZ =
	    0, HDMI_576I_50HZ, HDMI_480P_60HZ, HDMI_576P_50HZ,
	    HDMI_720P_60Hz, HDMI_720P_50Hz, HDMI_1080I_60Hz,
	    HDMI_1080I_50Hz, HDMI_1080P_60Hz, HDMI_1080P_50Hz,
	    HDMI_TV_MODE_MAX 
} hdmi_tv_mode_t;
typedef enum { HDMI_4_3 =
	    0, HDMI_4_3or_16_9, HDMI_16_9, HDMI_TV_RATIOMAX 
} hdmi_tv_ratio_t;
typedef enum { UNKNOWN_DISPLAY = 0, DVI_DISPLAY, HDMI_DISPLAY, 
} display_type_t;
typedef enum { HDCP_NO_AUTH = 0, HDCP_NO_DEVICE_WITH_SLAVE_ADDR =
	    1, HDCP_BKSV_ERROR = 2, HDCP_R0S_ARE_MISSMATCH =
	    3, HDCP_RIS_ARE_MISSMATCH = 4, HDCP_REAUTHENTATION_REQ =
	    5, HDCP_REQ_AUTHENTICATION = 6, HDCP_NO_ACK_FROM_DEV =
	    7, HDCP_NO_RSEN = 8, HDCP_AUTHENTICATED =
	    9, HDCP_REPEATER_AUTH_REQ = 0xa, HDCP_REQ_SHA_CALC =
	    0xb, HDCP_REQ_SHA_HW_CALC = 0xc, HDCP_FAILED_ViERROR =
	    0xd, HDCP_MAX = 0xe 
} hdcp_auth_state_t;
typedef enum { CABLE_UNPLUG =
	    0, CABLE_PLUGIN_CHECK_EDID, CABLE_PLUGIN_CHECK_EDID_OK,
	    CABLE_PLUGIN_CHECK_EDID_CORRUPTED, CABLE_PLUGIN_HDMI_OUT,
	    CABLE_PLUGIN_DVI_OUT, CABLE_MAX 
} hdmi_output_state_t;
typedef struct {
	u32 support_format:1;
	u32 support_16_9:1;
	u32 support_4_3:1;
} hdmi_sup_tv_format_t;
typedef struct {
	unsigned char flag;
	 unsigned char format;
	 unsigned char sampling_fre;
	 unsigned char sampling_len;
	 unsigned char I2S_ctl;
} hdmi_audio_type_t;
typedef struct {
	u32 hpd_state:1;
	u32 support_480i:1;
	u32 support_576i:1;
	u32 support_480p:1;
	u32 support_576p:1;
	u32 support_720p_60hz:1;
	u32 support_720p_50hz:1;
	u32 support_1080i_60hz:1;
	u32 support_1080i_50hz:1;
	u32 support_1080p_60hz:1;
	u32 support_1080p_50hz:1;
} hdmi_sup_status_t;
typedef struct {
	u32 support_flag:1;
	u32 max_channel_num:3;
	u32 _192k:1;
	u32 _176k:1;
	u32 _96k:1;
	u32 _88k:1;
	u32 _48k:1;
	u32 _44k:1;
	u32 _32k:1;
	u32 _24bit:1;
	u32 _20bit:1;
	u32 _16bit:1;
	unsigned char _other_bit;
} hdmi_sup_audio_format_t;
typedef struct {
	u32 rlc_rrc:1;
	u32 flc_frc:1;
	u32 rc:1;
	u32 rl_rr:1;
	u32 fc:1;
	u32 lfe:1;
	u32 fl_fr:1;
} hdmi_sup_speaker_format_t;
typedef struct {
	struct hdmi_priv *priv;
	int irq_flag;
	 hdmi_tv_mode_t tv_mode;
	 hdmi_tv_ratio_t tv_ratio;
	  unsigned char audio_flag;	//Bit 0:      1 - enable hdmi audio;  0 - display hdmi audio
	unsigned char audiopath_format;	//bit[3:0]: 0 - SPDIF;  1 - I2S;    2 - DSD;    3 - HBR  .. 
	//Bit 7:      0 - Layout 0 (stereo);  1 - Layout 1 (up to 8 channels)
	unsigned char audiopath_sf;	//Sampling Freq Fs:0 - Refer to Stream Header; 1 - 32KHz; 2 - 44.1KHz; 3 - 48KHz; 4 - 88.2KHz...
	unsigned char audiopath_sl;	//bit[3:0]: 2, 4, 8, 0xA, 0xC +1 for Max 24. Even values are Max 20. Odd: Max 24.
	//bit[7:4]: 16bit/18bit/20bit/24bit Sample Size
	unsigned char audiopath_i2s_in_ctrl;	//I2S control bits
	 unsigned char sync_emb_flag;	//bit0: CCIR656 ; bit1: sync sync embedded; bit2: DDR 
	unsigned char videopath_inindex;	//Video Input Source(bit[3:0]): 0 - RGB24; 1 - RGB DVO 12; 2 - YCbCr24; 3 - YC24; 4 - YCbCr4:2:2
	unsigned char videopath_outindex;	// Video Output Color Space Conversion : 0 - RGB out; 1 - YCbr (4:4:4) out; 2 - YCbr (4:2:2) out; 
	unsigned char videopath_clocke_edge;	//Clock Edge : 0 - Falling edge; 1 - Rising Edge
	unsigned char videopath_deep_color;	// Deep Color Mode : 0 - 24 bit; 1 - 30 bits; 2 - 36 bits
	 char cea_on_fisrt_page;	// when offset pointer (Slave Addr 0x60) is not used CEA861B extension always on second page    
	 hdcp_auth_state_t auth_state;
	
	    //    hdcp_auth_state_t  old_auth_state;   
	 hdmi_output_state_t output_state;
	 u32 auto_ri_flag:1;	// If == 1, turn on Auto Ri Checking
	u32 hw_sha_calculator_flag:1;	// If  == 1, use the HW SHA calculator, otherwise, use SW SHA calculator
	
	    //0 :  Pixel data is not replicated
	    //1 :  Pixels are replicated once (each sent twice)
	    //2 :  RSVD
	    //3:   Pixels are replicated 4 times (each sent four times) 
	 u32 repeat_pixel_flag:3;
	 
	    //0b000 = MCLK is 128*Fs
	    //0b001 = MCLK is 256*Fs  (recommanded)
	    //0b010 = MCLK is 384*Fs
	    //0b011 = MCLK is 512*Fs
	    //0b100 = MCLK is 768*Fs
	    //0b101 = MCLK is 1024*Fs
	    //0b110 = MCLK is 1152*Fs
	    //0b111 = MCLK is 192*Fs    
	 u32 audio_mclk:3;
	  u32 init_hdmi_flag:1;
	 u32 init_dvi_flag:1;
	 u32 video_mode_change_flag:1;
	 u32 video_ratio_change_flag:1;
	 u32 txinit_flag:1;	//1 : indicate the system is in the initialization state; 0: indicate the hdmi initialization is done
	u32 need_check_edid:1;
	 u32 hpd_state:1;	//0: no hdmi device connected,  0x1: hdmi device connected    
	u32 hpd_change_flag:1;	//0: no change,  1: changed     
	u32 SPDIF_stream_error:1;
	 u32 Ri_128_flag:1;
	 u32 audio_fifo_overflow:1;
	 u32 audio_fifo_underflow:1;
	 u32 audio_cts_status_err_flag:1;
	 u32 back_down_flag:1;
	 u32 SPDIF_parity_error:1;
	  u32 Ri_no_match:1;
	 u32 Ri_no_change:1;
	  u32 check_hdmi_interrupt:1;
	  u32 support_underscan_flag:1;
	 u32 support_ycbcr444_flag:1;
	 u32 support_ycbcr422_flag:1;
	 u32 support_basic_audio_flag:1;
	 u32 support_ai_flag:1;
	  hdmi_sup_tv_format_t video_480i;
	 hdmi_sup_tv_format_t video_576i;
	 hdmi_sup_tv_format_t video_480p;
	 hdmi_sup_tv_format_t video_576p;
	 hdmi_sup_tv_format_t video_720p_60hz;
	 hdmi_sup_tv_format_t video_720p_50hz;
	 hdmi_sup_tv_format_t video_1080i_60hz;
	 hdmi_sup_tv_format_t video_1080i_50hz;
	 hdmi_sup_tv_format_t video_1080p_60hz;
	 hdmi_sup_tv_format_t video_1080p_50hz;
	   hdmi_sup_audio_format_t audio_linear_pcm;
	 hdmi_sup_audio_format_t audio_ac_3;
	 hdmi_sup_audio_format_t audio_mpeg1;
	 hdmi_sup_audio_format_t audio_mp3;
	 hdmi_sup_audio_format_t audio_mpeg2;
	 hdmi_sup_audio_format_t audio_aac;
	 hdmi_sup_audio_format_t audio_dts;
	 hdmi_sup_audio_format_t audio_atrac;
	  hdmi_sup_speaker_format_t speaker_allocation;
 } Hdmi_info_para_t;

#define DISPCTL_MODE_480I	0
#define DISPCTL_MODE_576I	1
#define DISPCTL_MODE_480P	2
#define DISPCTL_MODE_576P	3
#define DISPCTL_MODE_720P	4
#define DISPCTL_MODE_1080I	5
#define DISPCTL_MODE_1080P	6
#define DISPCTL_MODE_VGA	7
#define DISPCTL_MODE_SVGA	8
#define DISPCTL_MODE_XGA	9
#define DISPCTL_MODE_SXGA	10
#define DISPCTL_MODE_LCD    11
#define DISPCTL_MODE_KEEP   12
typedef enum { POLICY_HD_FREQ_60 =
	    0, POLICY_HD_FREQ_50, POLICY_HD_FREQ_AUTO, 
} policy_hd_freq_t;
void hdmi_init_para(struct hdmi_priv *priv);
static void inline AVTimeDly(int m) 
{
	msleep(m);
} void reset_hdmi_device(int level);


#endif	/*  */
