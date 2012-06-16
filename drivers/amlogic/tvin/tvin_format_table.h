/*
 * TVIN signal format table
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


#ifndef __TVIN_FORMAT_TABLE_H
#define __TVIN_FORMAT_TABLE_H

#include <linux/tvin/tvin.h>

#include "tvin_global.h"

extern const unsigned char tvin_canvas_tab[2][BT656IN_VF_POOL_SIZE];

extern const struct tvin_format_s tvin_fmt_tbl[TVIN_SIG_FMT_MAX + 1];

extern const unsigned char adc_vga_table[TVIN_SIG_FMT_VGA_MAX-TVIN_SIG_FMT_NULL-1][ADC_REG_NUM];
extern const unsigned char adc_component_table[TVIN_SIG_FMT_COMP_MAX-TVIN_SIG_FMT_VGA_MAX-1][ADC_REG_NUM];
extern const unsigned char adc_cvbs_table[ADC_REG_NUM];
extern const unsigned char cvd_part1_table[TVIN_SIG_FMT_CVBS_SECAM-TVIN_SIG_FMT_CVBS_NTSC_M+1][CVD_PART1_REG_NUM];
extern const unsigned char cvd_part2_table[TVIN_SIG_FMT_CVBS_SECAM-TVIN_SIG_FMT_CVBS_NTSC_M+1][CVD_PART2_REG_NUM];
// 0x87, 0x93, 0x94, 0x95, 0x96, 0xe6, 0xfa
extern const unsigned int cvd_part3_table[TVIN_SIG_FMT_CVBS_SECAM-TVIN_SIG_FMT_CVBS_NTSC_M+1][CVD_PART3_REG_NUM];
extern const unsigned int cvbs_acd_table[TVIN_SIG_FMT_CVBS_SECAM-TVIN_SIG_FMT_CVBS_NTSC_M+1][ACD_REG_MAX+1];

#endif

