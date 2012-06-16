/*
 * VDIN driver
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *         Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __TVIN_VDIN_CTL_H
#define __TVIN_VDIN_CTL_H


#include <linux/amports/vframe.h>

#include "vdin.h"


// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************
/*
YUV601:  SDTV BT.601            YCbCr (16~235, 16~240, 16~240)
YUV601F: SDTV BT.601 Full_Range YCbCr ( 0~255,  0~255,  0~255)
YUV709:  HDTV BT.709            YCbCr (16~235, 16~240, 16~240)
YUV709F: HDTV BT.709 Full_Range YCbCr ( 0~255,  0~255,  0~255)
RGBS:                       StudioRGB (16~235, 16~235, 16~235)
RGB:                              RGB ( 0~255,  0~255,  0~255)
*/

typedef enum vdin_format_convert_e {
    VDIN_FORMAT_CONVERT_YUV_YUV422 = 0,
    VDIN_FORMAT_CONVERT_YUV_YUV444,
    VDIN_FORMAT_CONVERT_YUV_RGB,
    VDIN_FORMAT_CONVERT_RGB_YUV422,
    VDIN_FORMAT_CONVERT_RGB_YUV444,
    VDIN_FORMAT_CONVERT_RGB_RGB,
} vdin_format_convert_t;

typedef enum vdin_matrix_csc_e {
	VDIN_MATRIX_NULL = 0,
    VDIN_MATRIX_RGB_YUV601,
    VDIN_MATRIX_YUV601_RGB,
    VDIN_MATRIX_RGB_YUV601F,
    VDIN_MATRIX_YUV601F_RGB,
    VDIN_MATRIX_RGBS_YUV601,
    VDIN_MATRIX_YUV601_RGBS,
    VDIN_MATRIX_RGBS_YUV601F,
    VDIN_MATRIX_YUV601F_RGBS,
    VDIN_MATRIX_YUV601F_YUV601,
    VDIN_MATRIX_YUV601_YUV601F,
    VDIN_MATRIX_RGB_YUV709,
    VDIN_MATRIX_YUV709_RGB,
    VDIN_MATRIX_RGB_YUV709F,
    VDIN_MATRIX_YUV709F_RGB,
    VDIN_MATRIX_RGBS_YUV709,
    VDIN_MATRIX_YUV709_RGBS,
    VDIN_MATRIX_RGBS_YUV709F,
    VDIN_MATRIX_YUV709F_RGBS,
    VDIN_MATRIX_YUV709F_YUV709,
    VDIN_MATRIX_YUV709_YUV709F,
    VDIN_MATRIX_YUV601_YUV709,
    VDIN_MATRIX_YUV709_YUV601,
    VDIN_MATRIX_YUV601_YUV709F,
    VDIN_MATRIX_YUV709F_YUV601,
    VDIN_MATRIX_YUV601F_YUV709,
    VDIN_MATRIX_YUV709_YUV601F,
    VDIN_MATRIX_YUV601F_YUV709F,
    VDIN_MATRIX_YUV709F_YUV601F,
    VDIN_MATRIX_RGBS_RGB,
    VDIN_MATRIX_RGB_RGBS,
} vdin_matrix_csc_t;

// ***************************************************************************
// *** structure definitions *********************************************
// ***************************************************************************
typedef struct vdin_matrix_lup_s {
    unsigned int pre_offset0_1;
    unsigned int pre_offset2;
    unsigned int coef00_01;
    unsigned int coef02_10;
    unsigned int coef11_12;
    unsigned int coef20_21;
    unsigned int coef22;
    unsigned int post_offset0_1;
    unsigned int post_offset2;
} vdin_matrix_lup_t;

typedef struct vdin_stat_s {
    unsigned int   sum_luma   ; // VDIN_HIST_LUMA_SUM_REG
    unsigned int   sum_pixel  ; // VDIN_HIST_PIX_CNT_REG
} vdin_stat_t;

typedef struct vdin_hist_cfg_s {
    unsigned int                pow;
    unsigned int                win_en;
    unsigned int                rd_en;
    unsigned int                hstart;
    unsigned int                hend;
    unsigned int                vstart;
    unsigned int                vend;
} vdin_hist_cfg_t;

// *****************************************************************************
// ******** GLOBAL FUNCTION CLAIM ********
// *****************************************************************************
//for vdin
extern void vdin_set_vframe_prop_info(vframe_t *vf, unsigned int offset);
extern void vdin_set_regs(struct vdin_regs_s *p, int offset);
extern void vdin_set_all_regs(struct vdin_dev_s *devp);
extern void vdin_set_default_regmap(unsigned int offset);
extern void vdin_set_meas_mux(unsigned int offset, enum tvin_port_e port_);

//for decoder

#if defined(CONFIG_ARCH_MESON2)
extern void vdin_get_meas_timing(struct vdin_dev_s *devp);
#endif
extern unsigned int vdin_get_active_h(unsigned int offset);
extern unsigned int vdin_get_active_v(unsigned int offset);
extern unsigned int vdin_get_total_v(unsigned int offset);

extern void vdin_set_canvas_id(unsigned int offset, unsigned int canvas_id);
#endif

