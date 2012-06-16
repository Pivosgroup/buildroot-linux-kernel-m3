/*******************************************************************
*  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
*  File name: TVAFE_CVD.h
*  Description: IO function, structure, enum, used in TVIN AFE sub-module processing
*******************************************************************/
#ifndef _TVAFE_CVD_H
#define _TVAFE_CVD_H

// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************
//system state machine for cvd2
typedef enum tvafe_cvd2_system_state_e {
    CVD2_IDLE_STATE = 0,
    CVD2_SIG_ON_STATE,
    CVD2_SIG_OFF_STATE,
    CVD2_SIG_DETECT_STATE,
    CVD2_MODE_CONFIG_STATE,
    CVD2_MODE_VCR_STATE,
    CVD2_MODE_UNSTABLE_STATE,
} tvafe_cvd2_system_state_t;

//cvd2 mode detect state machine
typedef enum tvafe_cvd2_mode_search_state_e {
    CVD2_DET_START = 0,
    CVD2_DET_NTSC_M,
    CVD2_DET_PAL_M,
    CVD2_DET_NTSC_443,
    CVD2_DET_PAL_60,
    CVD2_DET_PAL_CN,
    CVD2_DET_PAL_BGDI,
    CVD2_DET_SECAM,
} tvafe_cvd2_mode_search_state_t;

typedef enum tvafe_cvd2_cordic_e {
	FCMORE,
	FCLESS,
	FCSAME
} tvafe_cvd2_cordic_t;

typedef enum tvafe_cvd2_sd_state_e {
	SD_NO_SIG = 0,
	SD_UNLOCK,
	SD_NONSTD,
	SD_VCR,
	SD_HV_LOCK,
	SD_PAL_I,
	SD_PAL_M,
	SD_PAL_CN,
	SD_NTSC,
	SD_NTSC_443,
	SD_PAL_60,
	SD_SECAM
} tvafe_cvd2_sd_state_t;

// 525 lines or 625 lines mode
typedef enum tvafe_cvd2_video_lines_e {
    MODE_525 = 0,
    MODE_625,
} tvafe_cvd2_video_lines_t;

//cvd2 video detect result
typedef enum tvafe_cvd2_detect_result_e {
    VIDEO_MODE_UNLOCK = 0,
    VIDEO_MODE_NONSTD,
    VIDEO_MODE_VCR,
    VIDEO_MODE_LINES_UNSTABLE,
    VIDEO_MODE_FOUND,
    VIDEO_MODE_NON_CVBS,
    VIDEO_MODE_SYSTEM_CHANGE,
} tvafe_cvd2_detect_result_t;

// ***************************************************************************
// *** structure definitions *********************************************
// ***************************************************************************
typedef struct tvafe_cvd2_agc_s {
    unsigned int        cnt;
    unsigned int        dgain;
    unsigned int        again;
} tvafe_cvd2_agc_t;

//CVD2 status list
typedef struct tvafe_cvd2_sig_status_s {
    unsigned char  no_sig                :1;
    unsigned char  h_lock                :1;
    unsigned char  v_lock                :1;
    unsigned char  h_nonstd              :1;
    unsigned char  v_nonstd              :1;
    unsigned char  no_color_burst        :1;
    unsigned char  comb3d_off            :1;
    unsigned char  hv_lock               :1;

    unsigned char  chroma_lock           :1;
    unsigned char  pal                   :1;
    unsigned char  secam                 :1;
    unsigned char  line625               :1;
    unsigned char  fc_more               :1;
    unsigned char  fc_Less               :1;
    unsigned char  noisy                 :1;
    unsigned char  vcr                   :1;

    unsigned char  vcrtrick                 :1;
    unsigned char  vcrff                    :1;
    unsigned char  vcrrew                   :1;

    unsigned char  cordic_data_min;
    unsigned char  cordic_data_max;
    unsigned char  stable_cnt;
    unsigned char  new_fmt_cnt;
    unsigned char  chroma_stable_cnt;
    unsigned char  cvd_fmt_chg_cnt;
    unsigned char  status_1;
    unsigned char  status_2;
    unsigned char  status_3;
    struct tvafe_cvd2_fmt_cordic_s      cordic;

    enum tvafe_cvd2_sd_state_e          cur_sd_state;
    enum tvafe_cvd2_sd_state_e          detected_sd_state;

    struct tvafe_cvd2_agc_s             agc;
    unsigned                            cordic_data_sum;
    unsigned int                        cvd2_mem_addr;
    unsigned int                        cvd2_mem_size;
} tvafe_cvd2_sig_status_t;

// *****************************************************************************
// ******** GLOBAL FUNCTION CLAIM ********
// *****************************************************************************
extern void tvafe_set_cvd2_default(unsigned int mem_addr, unsigned int mem_size, enum tvin_sig_fmt_e fmt);
extern void tvafe_cvd2_state_handler(struct tvafe_info_s *info);
extern enum tvafe_cvbs_video_e  tvafe_cvd2_video_locked(void);
extern bool tvafe_cvd_no_sig(void);
extern bool tvafe_cvd_fmt_chg(void);
extern enum tvin_sig_fmt_e tvafe_cvd2_get_format(void);
extern int tvafe_cvd2_video_agc_handler(struct tvafe_info_s *info);
extern void tvafe_cvd2_state_init(void);
extern void tvafe_cvd2_video_mode_confiure(enum tvin_sig_fmt_e fmt, enum tvin_port_e port);
extern void init_cvd2_reg_module(enum tvafe_cvd2_sd_state_e vvd2_status );
extern int tvafe_cvd2_get_cordic_para(struct tvafe_cvd2_fmt_cordic_s *info);
extern int tvafe_cvd2_set_cordic_para(struct tvafe_cvd2_fmt_cordic_s *info);

#endif // _TVAFE_CVD_H
