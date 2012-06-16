/*******************************************************************
*  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
*  File name: tvafe.h
*  Description: IO function, structure, enum, used in TVIN AFE sub-module processing
*******************************************************************/

#ifndef _TVAFE_H
#define _TVAFE_H


#include <linux/tvin/tvin.h>
#include "tvin_global.h"
#include "tvin_format_table.h"

/******************************Definitions************************************/

#define ABS(x) ( (x)<0 ? -(x) : (x))

// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************

typedef enum tvafe_src_type_e {
    TVAFE_SRC_TYPE_NULL = 0,
    TVAFE_SRC_TYPE_CVBS,
	TVAFE_SRC_TYPE_SVIDEO,
    TVAFE_SRC_TYPE_VGA,
    TVAFE_SRC_TYPE_COMP,
    TVAFE_SRC_TYPE_SCART,
} tvafe_src_type_t;

// ***************************************************************************
// *** structure definitions *********************************************
// ***************************************************************************
//typedef struct tvafe_adc_parm_s {
//    unsigned int                            clk;
//    unsigned int                            phase;
//    unsigned int                            hpos;
//    unsigned int                            vpos;
//} tvafe_adc_parm_t;

typedef struct tvafe_info_s {
    unsigned int                            cvd2_mem_addr;
    unsigned int                            cvd2_mem_size;

    struct tvafe_pin_mux_s                  *pinmux;

    //signal parameters
    struct tvin_parm_s                      param;
    enum tvafe_src_type_e                   src_type;

    //VGA settings
    unsigned char                           vga_auto_flag;
    enum tvafe_cmd_status_e                 cmd_status;

    //adc calibration data
    struct tvafe_adc_cal_s                  adc_cal_val;

    //WSS data
    struct tvafe_comp_wss_s                 comp_wss;

    //for canvas
    unsigned char                           rd_canvas_index;
    unsigned char                           wr_canvas_index;
    unsigned char                           buff_flag[TVAFE_VF_POOL_SIZE];
    unsigned                                pbufAddr;
    unsigned                                decbuf_size;
    unsigned                                canvas_total_count : 4;

    unsigned                                sig_status_cnt          : 16;
    unsigned                                cvbs_dec_state          : 1;    //1: enable the decode; 0: disable the decode
    unsigned                                s_video_dec_state       : 1;    //1: enable the decode; 0: disable the decode
    unsigned                                vga_dec_state           : 1;    //1: enable the decode; 0: disable the decode
    unsigned                                comp_dec_state          : 1;    //1: enable the decode; 0: disable the decode

    unsigned                                video_in_changing_flag  : 1;
    unsigned                                wrap_flag : 1;

} tvafe_info_t;


#endif  // _TVAFE_H
