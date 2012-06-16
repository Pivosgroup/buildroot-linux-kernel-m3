#ifndef _HDMI_TX_MODULE_H
#define _HDMI_TX_MODULE_H
#include "hdmi_info_global.h"
/*****************************
*    hdmitx attr management 
******************************/

/************************************
*    hdmitx device structure
*************************************/
#define VIC_MAX_NUM 60
#define AUD_MAX_NUM 60
typedef struct
{
    unsigned char audio_format_code;
    unsigned char channel_num_max;
    unsigned char freq_cc;        
    unsigned char cc3;
} rx_audio_cap_t;

typedef struct rx_cap_
{
    unsigned char native_Mode;
    /*video*/
    unsigned char VIC[VIC_MAX_NUM];
    unsigned char VIC_count;
    unsigned char native_VIC;
    /*audio*/
    rx_audio_cap_t RxAudioCap[AUD_MAX_NUM];
    unsigned char AUD_count;
    unsigned char RxSpeakerAllocation;
    /*vendor*/    
    unsigned int IEEEOUI;
    unsigned int ColorDeepSupport;
    unsigned int Max_TMDS_Clock; 
    
}rx_cap_t;


#define EDID_MAX_BLOCK  20       //4
#define HDMI_TMP_BUF_SIZE            1024
typedef struct hdmi_tx_dev_s {
#ifdef AVOS
	  INT16U             task_id;
	  OS_STK             * taskstack;
#else
    struct cdev cdev;             /* The cdev structure */
    struct proc_dir_entry *proc_file;
    struct task_struct *task;
#endif
    struct {
        void (*SetPacket)(int type, unsigned char* DB, unsigned char* HB);
        void (*SetAudioInfoFrame)(unsigned char* AUD_DB, unsigned char* CHAN_STAT_BUF);
        unsigned char (*GetEDIDData)(struct hdmi_tx_dev_s* hdmitx_device);
        int (*SetDispMode)(Hdmi_tx_video_para_t *param);
        int (*SetAudMode)(struct hdmi_tx_dev_s* hdmitx_device, Hdmi_tx_audio_para_t* audio_param);
        void (*SetupIRQ)(struct hdmi_tx_dev_s* hdmitx_device);
        void (*DebugFun)(struct hdmi_tx_dev_s* hdmitx_device, const char * buf);
        void (*UnInit)(struct hdmi_tx_dev_s* hdmitx_device);
        int (*Cntl)(struct hdmi_tx_dev_s* hdmitx_device, int cmd, unsigned arg);
    }HWOp;
    
    //wait_queue_head_t   wait_queue;            /* wait queues */
    /*EDID*/
    unsigned cur_edid_block;
    unsigned cur_phy_block_ptr;
    unsigned char EDID_buf[EDID_MAX_BLOCK*128]; 
    rx_cap_t RXCap;
    int vic_count;
    /*audio*/
    Hdmi_tx_audio_para_t cur_audio_param;
    int audio_param_update_flag;
    /*status*/
#define DISP_SWITCH_FORCE       0
#define DISP_SWITCH_EDID        1    
    unsigned char disp_switch_config; /* 0, force; 1,edid */
    unsigned char cur_VIC;
    unsigned char unplug_powerdown;
    /**/
    unsigned char hpd_event; /* 1, plugin; 2, plugout */
    unsigned char hpd_state; /* 1, connect; 0, disconnect */
    unsigned char force_audio_flag;
    unsigned char mux_hpd_if_pin_high_flag; 
	unsigned char cec_func_flag;
    int  auth_process_timer;
    HDMI_TX_INFO_t hdmi_info;
    unsigned char tmp_buf[HDMI_TMP_BUF_SIZE];
}hdmitx_dev_t;
#define HDMI_SOURCE_DESCRIPTION 0
#define HDMI_PACKET_VEND        1
#define HDMI_MPEG_SOURCE_INFO   2
#define HDMI_PACKET_AVI         3
#define HDMI_AUDIO_INFO         4
#define HDMI_AUDIO_CONTENT_PROTECTION   5

#ifdef AVOS
#define HDMI_PROCESS_DELAY  AVTimeDly(500)
#define AUTH_PROCESS_TIME   (4000/500)
#else
#define HDMI_PROCESS_DELAY  msleep(100)
#define AUTH_PROCESS_TIME   (4000/100)
#endif        


#define HDMITX_VER "2012Apr17b"

/************************************
*    hdmitx protocol level interface
*************************************/
extern void hdmitx_init_parameters(HDMI_TX_INFO_t *info);

extern int hdmitx_edid_parse(hdmitx_dev_t* hdmitx_device);

HDMI_Video_Codes_t hdmitx_edid_get_VIC(hdmitx_dev_t* hdmitx_device,const char* disp_mode, char force_flag);

extern int hdmitx_edid_dump(hdmitx_dev_t* hdmitx_device, char* buffer, int buffer_len);

extern void hdmitx_edid_clear(hdmitx_dev_t* hdmitx_device);

extern char* hdmitx_edid_get_native_VIC(hdmitx_dev_t* hdmitx_device);

extern int hdmitx_set_display(hdmitx_dev_t* hdmitx_device, HDMI_Video_Codes_t VideoCode);

extern int hdmi_set_3d(hdmitx_dev_t* hdmitx_device, int type, unsigned int param);

extern int hdmitx_set_audio(hdmitx_dev_t* hdmitx_device, Hdmi_tx_audio_para_t* audio_param);

extern int hdmi_print(int printk_flag, const char *fmt, ...);

extern  int hdmi_print_buf(char* buf, int len);

extern void hdmi_set_audio_para(int para);

extern void hdmitx_output_rgb(void);

extern int get_cur_vout_index(void);

/************************************
*    hdmitx hardware level interface
*************************************/
//#define DOUBLE_CLK_720P_1080I
extern unsigned char hdmi_pll_mode; /* 1, use external clk as hdmi pll source */

extern void HDMITX_M1A_Init(hdmitx_dev_t* hdmitx_device);

extern void HDMITX_M1B_Init(hdmitx_dev_t* hdmitx_device);

extern unsigned char hdmi_audio_off_flag;

#define HDMITX_HWCMD_POWERMODE_SWITCH    0x1
#define HDMITX_HWCMD_VDAC_OFF           0x2
#define HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH       0x3
#define HDMITX_HWCMD_TURNOFF_HDMIHW           0x4
#define HDMITX_HWCMD_MUX_HPD                0x5
#define HDMITX_HWCMD_PLL_MODE                0x6
#define HDMITX_HWCMD_TURN_ON_PRBS           0x7
#define HDMITX_FORCE_480P_CLK                0x8
#define HDMITX_OUTPUT_ENABLE                 0x9
#define HDMITX_GET_AUTHENTICATE_STATE        0xa


#endif
