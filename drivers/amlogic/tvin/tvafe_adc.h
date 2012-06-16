/*******************************************************************
*  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
*  File name: TVAFE_ADC.h
*  Description: IO function, structure, enum, used in TVIN AFE sub-module processing
*******************************************************************/
#ifndef _TVAFE_ADC_H
#define _TVAFE_ADC_H

// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************
//auto clock detection state machine
typedef enum tvafe_vga_auto_clk_state_e {
    VGA_CLK_IDLE,
    VGA_CLK_INIT,
    VGA_CLK_ROUGH_ADJ,
    VGA_CLK_FINE_ADJ,
    VGA_CLK_EXCEPTION,
    VGA_CLK_END,
} tvafe_vga_auto_clk_state_t;

//auto phase state machine
typedef enum tvafe_vga_auto_phase_state_e {
    VGA_PHASE_IDLE,
    VGA_PHASE_INIT,                     //auto phase init
    VGA_PHASE_SEARCH_WIN,
    VGA_PHASE_ADJ,                      //write all the phase value, get the best value by the sum value
    VGA_PHASE_EXCEPTION,
    VGA_PHASE_END,
} tvafe_vga_auto_phase_state_t;

// ***************************************************************************
// *** structure definitions *********************************************
// ***************************************************************************
typedef struct tvafe_vga_auto_win_s {
    unsigned int hstart;
    unsigned int hend;
    unsigned int vstart;
    unsigned int vend;
} tvafe_vga_auto_win_t;

#define VGA_PHASE_WIN_INDEX_0   0
#define VGA_PHASE_WIN_INDEX_1   1
#define VGA_PHASE_WIN_INDEX_2   2
#define VGA_PHASE_WIN_INDEX_3   3
#define VGA_PHASE_WIN_INDEX_4   4
#define VGA_PHASE_WIN_INDEX_5   5
#define VGA_PHASE_WIN_INDEX_6   6
#define VGA_PHASE_WIN_INDEX_7   7
#define VGA_PHASE_WIN_INDEX_8   8
#define VGA_PHASE_WIN_INDEX_MAX VGA_PHASE_WIN_INDEX_8

#define VGA_ADC_PHASE_0    0
#define VGA_ADC_PHASE_1    1
#define VGA_ADC_PHASE_2    2
#define VGA_ADC_PHASE_3    3
#define VGA_ADC_PHASE_4    4
#define VGA_ADC_PHASE_5    5
#define VGA_ADC_PHASE_6    6
#define VGA_ADC_PHASE_7    7
#define VGA_ADC_PHASE_8    8
#define VGA_ADC_PHASE_9    9
#define VGA_ADC_PHASE_10  10
#define VGA_ADC_PHASE_11  11
#define VGA_ADC_PHASE_12  12
#define VGA_ADC_PHASE_13  13
#define VGA_ADC_PHASE_14  14
#define VGA_ADC_PHASE_15  15
#define VGA_ADC_PHASE_16  16
#define VGA_ADC_PHASE_17  17
#define VGA_ADC_PHASE_18  18
#define VGA_ADC_PHASE_19  19
#define VGA_ADC_PHASE_20  20
#define VGA_ADC_PHASE_21  21
#define VGA_ADC_PHASE_22  22
#define VGA_ADC_PHASE_23  23
#define VGA_ADC_PHASE_24  24
#define VGA_ADC_PHASE_25  25
#define VGA_ADC_PHASE_26  26
#define VGA_ADC_PHASE_27  27
#define VGA_ADC_PHASE_28  28
#define VGA_ADC_PHASE_29  29
#define VGA_ADC_PHASE_30  30
#define VGA_ADC_PHASE_31  31
#define VGA_ADC_PHASE_MID VGA_ADC_PHASE_15
#define VGA_ADC_PHASE_MAX VGA_ADC_PHASE_31

typedef struct tvafe_vga_auto_state_s {
    enum tvafe_vga_auto_clk_state_e   clk_state;
    enum tvafe_vga_auto_phase_state_e phase_state;
    unsigned char                     ap_win_index;
    unsigned char                     ap_winmax_index;
    unsigned char                     ap_pha_index;
    unsigned char                     ap_phamax_index;
    unsigned char                     vs_cnt;
    unsigned char                     adj_cnt;
      signed char                     adj_dir;
    unsigned int                      ap_max_diff;
    struct tvafe_vga_auto_win_s       win;
} tvafe_vga_auto_state_t;

// *****************************************************************************
// ******** GLOBAL FUNCTION CLAIM ********
// *****************************************************************************
void tvafe_adc_set_param(struct tvafe_vga_parm_s *adc_parm, struct tvafe_info_s *info);
void tvafe_adc_get_param(struct tvafe_vga_parm_s *adc_parm, struct tvafe_info_s *info);
void tvafe_vga_vs_cnt(void);
void tvafe_vga_auto_adjust_handler(struct tvafe_info_s *info);
int tvafe_vga_auto_adjust_enable(struct tvafe_info_s *info);
void tvafe_vga_auto_adjust_disable(struct tvafe_info_s *info);
bool tvafe_adc_no_sig(void);
bool tvafe_adc_fmt_chg(struct tvafe_info_s *info);//(enum tvafe_src_type_e src_type, struct tvin_parm_s *parm);
bool tvafe_adc_get_pll_status(void);
enum tvin_sig_fmt_e tvafe_adc_search_mode(enum tvafe_src_type_e src_type);
void tvafe_set_vga_default(enum tvin_sig_fmt_e fmt);
void tvafe_set_comp_default(enum tvin_sig_fmt_e fmt);
void tvafe_adc_configure(enum tvin_sig_fmt_e fmt);
void tvafe_adc_state_init(void);
void tvafe_adc_digital_reset(void);

#endif // _TVAFE_ADC_H

