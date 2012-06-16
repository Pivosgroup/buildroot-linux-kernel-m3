/*
 * TVAFE adc device driver.
 *
 * Copyright (c) 2010 Frank zhao <frank.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/******************************Includes************************************/
#include <linux/errno.h>
#include <linux/kernel.h>

#include <mach/am_regs.h>

#include "tvafe.h"
#include "tvafe_regs.h"
#include "tvafe_general.h"
#include "tvafe_adc.h"
/***************************Local defines**********************************/
#define AUTO_CLK_VS_CNT       15//10 // get stable BD readings after n+1 frames
#define AUTO_PHASE_VS_CNT      1 // get stable AP readings after n+1 frames
#define ADC_WINDOW_H_OFFSET   32 // auto phase window h offset
#define ADC_WINDOW_V_OFFSET    2 // auto phase window v offset
#define MAX_AUTO_CLOCK_ORDER   4 // 1/16 headroom
#define VGA_AUTO_TRY_COUNTER 300 // vga max adjust counter, 3 seconds
// divide window into 7*7 sub-windows & make phase detection on 9 sub-windows
// -------
// -*-*-*-
// -------
// -*-*-*-
// -------
// -*-*-*-
// -------
#define VGA_AUTO_PHASE_H_WIN   7
#define VGA_AUTO_PHASE_V_WIN   7

#define TVIN_FMT_CHG_VGA_H_CNT_WOBBLE   5
#define TVIN_FMT_CHG_VGA_V_CNT_WOBBLE   1
#define TVIN_FMT_CHG_VGA_HS_CNT_WOBBLE  5
#define TVIN_FMT_CHG_VGA_VS_CNT_WOBBLE  1
#define TVIN_FMT_CHG_COMP_H_CNT_WOBBLE  7//5
#define TVIN_FMT_CHG_COMP_V_CNT_WOBBLE  3//    1
#define TVIN_FMT_CHG_COMP_HS_CNT_WOBBLE 0xffffffff // not to trust
#define TVIN_FMT_CHG_COMP_VS_CNT_WOBBLE 0xffffffff // not to trust

#define TVIN_FMT_CHK_VGA_VS_CNT_WOBBLE 1

#define TVAFE_H_MAX 0xfff
#define TVAFE_H_MIN 0x000

#define TVAFE_V_MAX 0xfff
#define TVAFE_V_MIN 0x000

#define TVAFE_VGA_VS_CNT_MAX 200

#define TVAFE_VGA_BD_EN_DELAY 4  //4//4 field delay

#define TVAFE_ADC_CONFIGURE_INIT     1
#define TVAFE_ADC_CONFIGURE_NORMAL   1|(1<<POWERDOWNZ_BIT)|(1<<RSTDIGZ_BIT) // 7
#define TVAFE_ADC_CONFIGURE_RESET_ON 1|(1<<POWERDOWNZ_BIT)                  // 5

#define TVAFE_VGA_CLK_TUNE_RANGE_ORDER  4 // 1/16 h_total
#define TVAFE_VGA_HPOS_TUNE_RANGE_ORDER 6 // 1/64 h_active
#define TVAFE_VGA_VPOS_TUNE_RANGE_ORDER 6 // 1/64 v_active

/***************************Local Structures**********************************/
static struct tvin_format_s adc_timing_info =
{
    //H_Active V_Active H_cnt Hcnt_offset Vcnt_offset Hs_cnt Hscnt_offset
        0,       0,    0,          0,           0,     0,           0,
    //H_Total V_Total Hs_Front Hs_Width Hs_bp Vs_Front Vs_Width Vs_bp Hs_Polarity
        0,     0,       0,       0,    0,       0,       0,    0, TVIN_SYNC_POL_NULL,
    //Vs_Polarity             Scan_Mode      Pixel_Clk(Khz/10) VBIs vbie
    TVIN_SYNC_POL_NULL,  TVIN_SCAN_MODE_NULL,               0,   0,   0
};

static struct tvafe_vga_auto_state_s vga_auto = {
    VGA_CLK_IDLE,
    VGA_PHASE_IDLE,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    {0, 0, 0, 0}
};

// *****************************************************************************
// Function:get ADC DVSS signal status
//
//   Params: none
//
//   Return: none
//
// *****************************************************************************
bool tvafe_adc_get_pll_status(void)
{
    return (bool)READ_APB_REG_BITS(ADC_REG_35, PLLLOCKED_BIT, PLLLOCKED_WID);
}

/*
const static  int unsigned short charge_pump_tbl[] = {0};

// *****************************************************************************
// Function:set adc clock
//
//   Params: format index
//
//   Return: success/error
//
// *****************************************************************************
static void tvafe_adc_set_clock(enum tvin_sig_fmt_e fmt)
{
    unsigned char div_ratio,vco_range_sel,i;
    unsigned short vco_gain;
    unsigned long k_vco,hs_freq,tmp;

    unsigned char  charge_pump_table[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};

	//select vco range and gain by pixel clock
    if (tvin_fmt_tbl[fmt].pixel_clk < 2500)       //25Mhz
        vco_range_sel |= 0x00;
    else if (tvin_fmt_tbl[fmt].pixel_clk <  4000)  //40MHz
        vco_range_sel |= 0x01;
    else if (tvin_fmt_tbl[fmt].pixel_clk <  10000)  //100MHz
        vco_range_sel |= 0x02;
    else
        vco_range_sel |= 0x03;
    //set vco sel reg
    WRITE_APB_REG_BITS(ADC_REG_68, vco_range_sel, VCORANGESEL_BIT, VCORANGESEL_WID);

    //set charge pump current value
    WRITE_APB_REG_BITS(ADC_REG_69, charge_pump_tbl[fmt], CHARGEPUMPCURR_BIT, CHARGEPUMPCURR_WID);


    // PLL divider programming
    div_ratio = (unsigned char)((tvin_fmt_tbl[fmt].h_total - 1) & 0x0FF0) >> 4;
    WRITE_APB_REG_BITS(ADC_REG_01, div_ratio, PLLDIVRATIO_MSB_BIT, PLLDIVRATIO_MSB_WID);

    div_ratio = (unsigned char)((tvin_fmt_tbl[fmt].h_total - 1) & 0x000F) << 4;
    WRITE_APB_REG_BITS(ADC_REG_02, div_ratio, PLLDIVRATIO_LSB_BIT, PLLDIVRATIO_LSB_WID);

    return;
}

// *****************************************************************************
// Function:set adc analog buffer bandwidth
//
//   Params: format index, adc channel
//
//   Return: none
//
// *****************************************************************************
static void tvafe_adc_set_bw_lpf(enum tvin_sig_fmt_e fmt)
{
    unsigned char i;
    unsigned int freq[] = {
           5,    7,    9,   13,   16,   20,   25,   28,
          33,   37,   40,   47,   54,   67,   74,   81,
          90,  150,  230,  320,  450,  600,  750
    };
    unsigned char lpf_ctl_tbl[] = {
         0x0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char bw_tbl[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
    };

    for (i = 0; i <= 22; i++) {
        if (((tvin_fmt_tbl[fmt].pixel_clk/100)/2) <= (unsigned long)freq[i])  //Mhz
            break;
    }

    if (i > 15) {    //if pixel clk > 2*81Mhz should close lpf
        WRITE_APB_REG_BITS(ADC_REG_19, 0, ENLPFA_BIT, ENLPFA_WID);
        WRITE_APB_REG_BITS(ADC_REG_1A, 0, ENLPFB_BIT, ENLPFB_WID);
        WRITE_APB_REG_BITS(ADC_REG_1B, 0, ENLPFC_BIT, ENLPFC_WID);
    } else {
        WRITE_APB_REG_BITS(ADC_REG_19, 1, ENLPFA_BIT, ENLPFA_WID);
        WRITE_APB_REG_BITS(ADC_REG_1A, 1, ENLPFB_BIT, ENLPFB_WID);
        WRITE_APB_REG_BITS(ADC_REG_1B, 1, ENLPFC_BIT, ENLPFC_WID);
        WRITE_APB_REG_BITS(ADC_REG_19, lpf_ctl_tbl[i], LPFBWCTRA_BIT, LPFBWCTRA_WID);
        WRITE_APB_REG_BITS(ADC_REG_1A, lpf_ctl_tbl[i], LPFBWCTRB_BIT, LPFBWCTRB_WID);
        WRITE_APB_REG_BITS(ADC_REG_1B, lpf_ctl_tbl[i], LPFBWCTRC_BIT, LPFBWCTRC_WID);
    }

    WRITE_APB_REG_BITS(ADC_REG_19, bw_tbl[i], ANABWCTRLA_BIT, ANABWCTRLA_WID);
    WRITE_APB_REG_BITS(ADC_REG_1A, bw_tbl[i], ANABWCTRLB_BIT, ANABWCTRLB_WID);
    WRITE_APB_REG_BITS(ADC_REG_1B, bw_tbl[i], ANABWCTRLB_BIT, ANABWCTRLC_WID);

    return;
}

// *****************************************************************************
// Function:set adc clamp parameter
//
//   Params: format index
//
//   Return: success/error
//
// *****************************************************************************
void tvafe_adc_set_clamp_para(enum tvin_sig_fmt_e fmt)
{
    WRITE_APB_REG_BITS(ADC_REG_03, (tvin_fmt_tbl[fmt].hs_width + 10),
        CLAMPPLACEM_BIT, CLAMPPLACEM_WID);
    WRITE_APB_REG_BITS(ADC_REG_04, (tvin_fmt_tbl[fmt].hs_bp - 10), CLAMPDURATION_BIT, CLAMPDURATION_WID);

    return;
}
*/
// *****************************************************************************
// Function:get ADC signal info(hcnt,vcnt,hpol,vpol)
//
//   Params: none
//
//   Return: none
//
// *****************************************************************************
bool tvafe_adc_no_sig(void)
{
    return (READ_APB_REG_BITS(TVFE_DVSS_INDICATOR1, NOSIG_BIT, NOSIG_WID) ? true : false);
}

// *****************************************************************************
// Function:get ADC signal info(hcnt,vcnt,hpol,vpol)
//
//   Params: none
//
//   Return: none
//
// *****************************************************************************
bool tvafe_adc_fmt_chg(struct tvafe_info_s *info) //(enum tvafe_src_type_e src_type, struct tvin_parm_s *parm)
{
    unsigned short tmp0 = 0, tmp1 = 0;
    unsigned int   h_cnt_offset = 0, v_cnt_offset = 0;
    unsigned int   hs_cnt_offset = 0, vs_cnt_offset = 0;
    unsigned int   h_cnt_wobble = 0, v_cnt_wobble = 0;
    unsigned int   hs_cnt_wobble = 0, vs_cnt_wobble = 0;
    bool           h_pol_chg = false, v_pol_chg = false;
    bool           flag = (bool)READ_APB_REG_BITS(TVFE_DVSS_INDICATOR1,
                                                  NOSIG_BIT, NOSIG_WID);
    if (flag)
    {
        pr_info("tvafe fmt check no signal \n");
        return flag;
    }

    if (info->src_type == TVAFE_SRC_TYPE_VGA)
    {
        h_cnt_wobble = TVIN_FMT_CHG_VGA_H_CNT_WOBBLE;
        v_cnt_wobble = TVIN_FMT_CHG_VGA_V_CNT_WOBBLE;
        hs_cnt_wobble = TVIN_FMT_CHG_VGA_HS_CNT_WOBBLE;
        vs_cnt_wobble = TVIN_FMT_CHG_VGA_VS_CNT_WOBBLE;
        //h_pol
        flag = (bool)READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR3,
                                        SPOL_H_POL_BIT, SPOL_H_POL_WID);
        if (flag)
        {
            if (adc_timing_info.hs_pol == TVIN_SYNC_POL_POSITIVE)
                h_pol_chg = true;
            adc_timing_info.hs_pol = TVIN_SYNC_POL_NEGATIVE;
        }
        else
        {
            if (adc_timing_info.hs_pol == TVIN_SYNC_POL_NEGATIVE)
                h_pol_chg = true;
            adc_timing_info.hs_pol = TVIN_SYNC_POL_POSITIVE;
        }
        //v_pol
        flag = (bool)READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR3,
                                        SPOL_V_POL_BIT, SPOL_V_POL_WID);
        if (flag)
        {
            if (adc_timing_info.vs_pol == TVIN_SYNC_POL_POSITIVE)
                v_pol_chg = true;
            adc_timing_info.vs_pol = TVIN_SYNC_POL_NEGATIVE;
        }
        else
        {
            if (adc_timing_info.vs_pol == TVIN_SYNC_POL_NEGATIVE)
                v_pol_chg = true;
            adc_timing_info.vs_pol = TVIN_SYNC_POL_POSITIVE;
        }
    }
    else if (info->src_type == TVAFE_SRC_TYPE_COMP)
    {
        h_cnt_wobble = TVIN_FMT_CHG_COMP_H_CNT_WOBBLE;
        v_cnt_wobble = TVIN_FMT_CHG_COMP_V_CNT_WOBBLE;
        hs_cnt_wobble = TVIN_FMT_CHG_COMP_HS_CNT_WOBBLE;
        vs_cnt_wobble = TVIN_FMT_CHG_COMP_VS_CNT_WOBBLE;
    }
    else
    {
        pr_info("tvafe fmt check no signal \n");
        return flag;
    }

    // hs_cnt
    tmp0 = (unsigned short)READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR1_HCNT,
                                            SPOL_HCNT_NEG_BIT, SPOL_HCNT_NEG_WID);
    tmp1 = (unsigned short)READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR1_HCNT,
                                            SPOL_HCNT_POS_BIT, SPOL_HCNT_POS_WID);
    tmp0 = (tmp0 < tmp1) ? tmp0 : tmp1;
    hs_cnt_offset = ABS((signed int)adc_timing_info.hs_cnt - (signed int)tmp0);
    adc_timing_info.hs_cnt = tmp0;
    // vs_cnt
    tmp0 = (unsigned short)READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR2_VCNT,
                                            SPOL_VCNT_NEG_BIT, SPOL_VCNT_NEG_WID);
    tmp1 = (unsigned short)READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR2_VCNT,
                                            SPOL_VCNT_POS_BIT, SPOL_VCNT_POS_WID);
    tmp0 = (tmp0 < tmp1) ? tmp0 : tmp1;
    vs_cnt_offset = ABS((signed int)adc_timing_info.vs_width - (signed int)tmp0);
    adc_timing_info.vs_width = tmp0;
    // h_cnt
    tmp0 = READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR4,
                            SAM_HCNT_BIT, SAM_HCNT_WID);
    if ((info->param.status == TVIN_SIG_STATUS_STABLE) &&
        (info->src_type == TVAFE_SRC_TYPE_COMP))
        adc_timing_info.h_cnt = tvin_fmt_tbl[info->param.fmt].h_cnt;

    h_cnt_offset = ABS((signed int)adc_timing_info.h_cnt - (signed int)tmp0);
    //h_cnt_offset = ABS((signed int)tvin_fmt_tbl[fmt].h_cnt - (signed int)tmp0);
    adc_timing_info.h_cnt = tmp0;
    // v_cnt
    tmp0 = READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR4,
                            SAM_VCNT_BIT, SAM_VCNT_WID);
    if ((info->param.status == TVIN_SIG_STATUS_STABLE) &&
        (info->src_type == TVAFE_SRC_TYPE_COMP))
        adc_timing_info.v_total = tvin_fmt_tbl[info->param.fmt].v_total;

    v_cnt_offset = ABS((signed int)adc_timing_info.v_total - (signed int)tmp0);
    //v_cnt_offset = ABS((signed int)tvin_fmt_tbl[fmt].v_total- (signed int)tmp0);
    adc_timing_info.v_total = tmp0;

    if ((h_cnt_offset > h_cnt_wobble)   ||
        (v_cnt_offset > v_cnt_wobble)   ||
        (hs_cnt_offset > hs_cnt_wobble) ||
        (vs_cnt_offset > vs_cnt_wobble) ||
        h_pol_chg                       ||
        v_pol_chg
       )
        flag = true;
    else
        flag = false;


    return flag;
}

// *****************************************************************************
// Function:set ADC sync mux setting
//
//   Params: none
//
//   Return: sucess/error
//
// *****************************************************************************
//static int tvafe_adc_sync_select(enum adc_sync_type_e sync_type)
//{
//    int ret = 0;
//
//    switch (sync_type) {
//        case ADC_SYNC_AUTO:
//            tvafe_reg_set_bits(ADC_REG_39, ADC_REG_SYNCMUXCTRLBYPASS, ADC_REG_SYNCMUXCTRLBYPASS_MASK, 1);
//            tvafe_reg_set_bits(ADC_REG_39, ADC_REG_SYNCMUXCTRL, ADC_REG_SYNCMUXCTRL_MASK, 1);
//            tvafe_reg_set_bits(ADC_REG_2E, ADC_REG_HSYNCACTVOVRD, ADC_REG_HSYNCACTVOVRD_MASK, 1);
//            tvafe_reg_set_bits(ADC_REG_2E, ADC_REG_VSYNCACTVSEL, ADC_REG_VSYNCACTVSEL_MASK, 1);
//            break;
//        case ADC_SYNC_SEPARATE:
//            //...
//            break;
//        case ADC_SYNC_SOG:
//            //...
//            break;
//    }
//
//    return ret;
//}
void tvafe_adc_digital_reset(void)
{
    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 1);
    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 5);
    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 7);
}
// *****************************************************************************
// Function: search input format by the info table
//
//   Params: none
//
//   Return: format index
//
// *****************************************************************************
enum tvin_sig_fmt_e tvafe_adc_search_mode(enum tvafe_src_type_e src_type)
{
	enum tvin_sig_fmt_e index     = TVIN_SIG_FMT_NULL;
	enum tvin_sig_fmt_e index_min = TVIN_SIG_FMT_NULL;
	enum tvin_sig_fmt_e index_max = TVIN_SIG_FMT_NULL;
    //unsigned int hcnt = 0;

    switch (src_type)
    {
        case TVAFE_SRC_TYPE_VGA:
            index_min = TVIN_SIG_FMT_NULL;
            index_max = TVIN_SIG_FMT_VGA_MAX;
            break;
        case TVAFE_SRC_TYPE_COMP:
            index_min = TVIN_SIG_FMT_VGA_MAX;
            index_max = TVIN_SIG_FMT_COMP_MAX;
            break;
        default:
            break;
    }
    if (index_max)
    {
        for (index=index_min+1; index<index_max; index++)
        {
            if ((src_type == TVAFE_SRC_TYPE_COMP) ||
                (
                 // VGA h_pol
                 (adc_timing_info.hs_pol == tvin_fmt_tbl[index].hs_pol) &&
                 // VGA v_pol
                 (adc_timing_info.vs_pol == tvin_fmt_tbl[index].vs_pol) &&
                 // VGA hs_cnt
                 (ABS((signed int)adc_timing_info.hs_cnt - (signed int)tvin_fmt_tbl[index].hs_cnt) <= tvin_fmt_tbl[index].hs_cnt_offset) &&
                 // VGA vs_cnt
                 (ABS((signed int)adc_timing_info.vs_width - (signed int)tvin_fmt_tbl[index].vs_width) <= TVIN_FMT_CHK_VGA_VS_CNT_WOBBLE)
                )
               )
            {
                if (// h_cnt
                    (ABS((signed int)adc_timing_info.h_cnt- (signed int)tvin_fmt_tbl[index].h_cnt) <= tvin_fmt_tbl[index].h_cnt_offset) &&
                    // v_cnt
                    (ABS((signed int)adc_timing_info.v_total- (signed int)tvin_fmt_tbl[index].v_total) <= tvin_fmt_tbl[index].v_cnt_offset)
                   )
                break;
            }
	    }
        if (index >= index_max)
            index = TVIN_SIG_FMT_NULL;
    }

	return index;
}

// *****************************************************************************
// Function:set auto phase window
//
//   Params: format index
//
//   Return: success/error
//
// *****************************************************************************
static void tvafe_adc_set_bd_window(enum tvin_sig_fmt_e fmt)
{
    unsigned int tmp = 0;

    tmp = tvin_fmt_tbl[fmt].hs_width + tvin_fmt_tbl[fmt].hs_bp - ADC_WINDOW_H_OFFSET;
    WRITE_APB_REG_BITS(TVFE_BD_MUXCTRL1, tmp, BD_HSTART_BIT, BD_HSTART_WID);
    tmp += tvin_fmt_tbl[fmt].h_active + ADC_WINDOW_H_OFFSET + ADC_WINDOW_H_OFFSET;
    WRITE_APB_REG_BITS(TVFE_BD_MUXCTRL1, tmp, BD_HEND_BIT, BD_HEND_WID);
    tmp = tvin_fmt_tbl[fmt].vs_width + tvin_fmt_tbl[fmt].vs_bp - ADC_WINDOW_V_OFFSET;
    WRITE_APB_REG_BITS(TVFE_BD_MUXCTRL2, tmp, BD_VSTART_BIT, BD_VSTART_WID);
    tmp += tvin_fmt_tbl[fmt].v_active + ADC_WINDOW_V_OFFSET + ADC_WINDOW_V_OFFSET;
    WRITE_APB_REG_BITS(TVFE_BD_MUXCTRL2, tmp, BD_VEND_BIT, BD_VEND_WID);
}

static void tvafe_adc_set_ap_window(enum tvin_sig_fmt_e fmt, unsigned char idx)
{
    unsigned int hh = tvin_fmt_tbl[fmt].h_active / VGA_AUTO_PHASE_H_WIN;
    unsigned int vv = tvin_fmt_tbl[fmt].v_active / VGA_AUTO_PHASE_V_WIN;
    unsigned int hs = tvin_fmt_tbl[fmt].h_active + (((idx%3) << 1) + 1)*hh;
    unsigned int he = hs + hh - 1;
    unsigned int vs = tvin_fmt_tbl[fmt].v_active + (((idx/3) << 1) + 1)*vv;
    unsigned int ve = vs + vv - 1;

    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, hs, AP_HSTART_BIT, AP_HSTART_WID);
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, he, AP_HEND_BIT,   AP_HEND_WID  );
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL2, vs, AP_VSTART_BIT, AP_VSTART_WID);
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL2, ve, AP_VEND_BIT,   AP_VEND_WID  );
}
// *****************************************************************************
// Function:set afe clamp function
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
/*
static void tvafe_adc_set_clamp(enum tvin_sig_fmt_e fmt)
{
    //set clamp starting edge and duration
    tvafe_adc_set_clamp_para(fmt);

    //set clamp starting edge and duration
    //if (clamp_type <= CLAMP_BOTTOM_REGULATED)
    //    tvafe_adc_set_clamp_reference(ch, ref_val);

    //enable clamp type
    //tvafe_adc_clamp_select(ch, clamp_type);

}
*/
// *****************************************************************************
// Function:set & get clock
//
//   Params: clock value
//
//   Return: success/error
//
// *****************************************************************************
static void tvafe_vga_set_clock(unsigned int clock)
{
    unsigned int tmp;

    tmp = (clock >> 4) & 0x000000FF;
 	WRITE_APB_REG_BITS(ADC_REG_01, tmp, PLLDIVRATIO_MSB_BIT, PLLDIVRATIO_MSB_WID);
    tmp = clock & 0x0000000F;
	WRITE_APB_REG_BITS(ADC_REG_02, tmp, PLLDIVRATIO_LSB_BIT, PLLDIVRATIO_LSB_WID);

    //reset adc digital pll
    tvafe_adc_digital_reset();

    return;
}

static unsigned int tvafe_vga_get_clock(void)
{
    unsigned int data;

    data = READ_APB_REG_BITS(ADC_REG_01, PLLDIVRATIO_MSB_BIT, PLLDIVRATIO_MSB_WID) << 4;
    data |= READ_APB_REG_BITS(ADC_REG_02, PLLDIVRATIO_LSB_BIT, PLLDIVRATIO_LSB_WID);

    return data;
}

// *****************************************************************************
// Function:set & get phase
//
//   Params: phase value
//
//   Return: none
//
// *****************************************************************************
static void tvafe_vga_set_phase(unsigned int phase)
{
	WRITE_APB_REG_BITS(ADC_REG_56, phase, CLKPHASEADJ_BIT, CLKPHASEADJ_WID);

    //reset adc digital pll
    tvafe_adc_digital_reset();
    return;
}

static unsigned int tvafe_vga_get_phase(void)
{
    return READ_APB_REG_BITS(ADC_REG_56, CLKPHASEADJ_BIT, CLKPHASEADJ_WID);
}

// *****************************************************************************
// Function:set & get h  position
//
//   Params: position value
//
//   Return: none
//
// *****************************************************************************
static void tvafe_vga_set_h_pos(unsigned int hs, unsigned int he)
{
	WRITE_APB_REG_BITS(TVFE_DEG_H, hs, DEG_HSTART_BIT, DEG_HSTART_WID);
	WRITE_APB_REG_BITS(TVFE_DEG_H, he,   DEG_HEND_BIT,   DEG_HEND_WID  );

    return;
}

static unsigned int tvafe_vga_get_h_pos(void)
{
    return READ_APB_REG_BITS(TVFE_DEG_H, DEG_HSTART_BIT, DEG_HSTART_WID);
}

// *****************************************************************************
// Function:set & get v position
//
//   Params: v position value
//
//   Return: none
//
// *****************************************************************************
static void tvafe_vga_set_v_pos(unsigned int vs, unsigned int ve, enum tvin_scan_mode_e scan_mode)
{
    unsigned int offset = (scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) ? 0 : 1;
    WRITE_APB_REG_BITS(TVFE_DEG_VODD,  vs,          DEG_VSTART_ODD_BIT,  DEG_VSTART_ODD_WID );
    WRITE_APB_REG_BITS(TVFE_DEG_VODD,  ve,          DEG_VEND_ODD_BIT,    DEG_VEND_ODD_WID   );
    WRITE_APB_REG_BITS(TVFE_DEG_VEVEN, vs + offset, DEG_VSTART_EVEN_BIT, DEG_VSTART_EVEN_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VEVEN, ve + offset, DEG_VEND_EVEN_BIT,   DEG_VEND_EVEN_WID  );
}

static unsigned int tvafe_vga_get_v_pos(void)
{
    return READ_APB_REG_BITS(TVFE_DEG_VODD, DEG_VSTART_ODD_BIT, DEG_VSTART_ODD_WID);
}

static void tvafe_vga_border_detect_enable(void)
{
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, 1, BD_DET_EN_BIT, BD_DET_EN_WID);
}

static void tvafe_vga_border_detect_disable(void)
{
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, 0, BD_DET_EN_BIT, BD_DET_EN_WID);
}

static void tvafe_vga_auto_phase_enable(void)
{
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, 1, AUTOPHASE_EN_BIT, AUTOPHASE_EN_WID);
}

static void tvafe_vga_auto_phase_disable(void)
{
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, 0, AUTOPHASE_EN_BIT, AUTOPHASE_EN_WID);
}
// *****************************************************************************
// Function: border detect init
//
//   Params: format index
//
//   Return: success/error
//
// *****************************************************************************
static void tvafe_vga_border_detect_init(enum tvin_sig_fmt_e fmt)
{
    //diable border detect
    tvafe_vga_border_detect_disable();
    // pix_thr = 4 (pix-val > pix_thr => valid pixel)
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL3, 0x100/*0x10*/,
        BD_R_TH_BIT, BD_R_TH_WID);
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL5, 0x100/*0x10*/,
        BD_G_TH_BIT, BD_G_TH_WID);
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL5, 0x100/*0x10*/,
        BD_B_TH_BIT, BD_B_TH_WID);
    // pix_val > pix_thr => valid pixel
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, 1,
        BD_DET_METHOD_BIT, BD_DET_METHOD_WID);
    // line_thr = 1/16 of h_active (valid pixels > line_thr => valid line)
    WRITE_APB_REG_BITS(TVFE_BD_MUXCTRL3, (tvin_fmt_tbl[fmt].h_active)>>5/*(tvin_fmt_tbl[fmt].h_active)>>4*/,
        BD_VLD_LN_TH_BIT, BD_VLD_LN_TH_WID);
    // line_thr enable
    WRITE_APB_REG_BITS(TVFE_BD_MUXCTRL3, 1,
        BD_VALID_LN_EN_BIT, BD_VALID_LN_EN_WID);
    // continuous border detection mode
    WRITE_APB_REG_BITS(TVFE_BD_MUXCTRL4, 0,
        BD_LIMITED_FLD_RECORD_BIT, BD_LIMITED_FLD_RECORD_WID);
    WRITE_APB_REG_BITS(TVFE_BD_MUXCTRL4, 0,
        BD_FLD_CD_NUM_BIT, BD_FLD_CD_NUM_WID);
    // set a large window
    tvafe_adc_set_bd_window(fmt);
    //enable border detect
    tvafe_vga_border_detect_enable();
}

// *****************************************************************************
// Function: auto phase init
//
//   Params: format index
//
//   Return: success/error
//
// *****************************************************************************
static void tvafe_vga_auto_phase_init(enum tvin_sig_fmt_e fmt, unsigned char idx)
{
    //disable auto phase
    tvafe_vga_auto_phase_disable();
    // use diff value
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, 1,
        AP_DIFF_SEL_BIT, AP_DIFF_SEL_WID);
    // use window
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL1, 0,
        AP_SPECIFIC_POINT_OUT_BIT, AP_SPECIFIC_POINT_OUT_WID);
    // coring_thr = 4 (diff > coring_thr => valid diff)
    WRITE_APB_REG_BITS(TVFE_AP_MUXCTRL3, 0x10,
        AP_CORING_TH_BIT, AP_CORING_TH_WID);
    // set auto phase window
    tvafe_adc_set_ap_window(fmt, idx);
    //enable auto phase
    tvafe_vga_auto_phase_enable();
}

static unsigned int tvafe_vga_get_ap_diff(void)
{
    unsigned int sum_r = READ_APB_REG(TVFE_AP_INDICATOR1);
    unsigned int sum_g = READ_APB_REG(TVFE_AP_INDICATOR2);
    unsigned int sum_b = READ_APB_REG(TVFE_AP_INDICATOR3);
    unsigned int sum   = 0;

    if (sum < sum_r)
        sum = sum_r;
    if (sum < sum_g)
        sum = sum_g;
    if (sum < sum_b)
        sum = sum_b;

    return sum;
}

// *****************************************************************************
// Function:get the result of H border detection
//
//   Params: format index
//
//   Return: success/error
//
// *****************************************************************************
static void tvafe_vga_get_h_border(void)
{
    unsigned int r_left_hcnt = 0, r_right_hcnt = 0;
    unsigned int g_left_hcnt = 0, g_right_hcnt = 0;
    unsigned int b_left_hcnt = 0, b_right_hcnt = 0;

    r_right_hcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR14, BD_R_RIGHT_HCNT_BIT, BD_R_RIGHT_HCNT_WID);
    r_left_hcnt  = READ_APB_REG_BITS(TVFE_AP_INDICATOR14, BD_R_LEFT_HCNT_BIT,  BD_R_LEFT_HCNT_WID );
    g_right_hcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR16, BD_G_RIGHT_HCNT_BIT, BD_G_RIGHT_HCNT_WID);
    g_left_hcnt  = READ_APB_REG_BITS(TVFE_AP_INDICATOR16, BD_G_LEFT_HCNT_BIT,  BD_G_LEFT_HCNT_WID );
    b_right_hcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR18, BD_B_RIGHT_HCNT_BIT, BD_B_RIGHT_HCNT_WID);
    b_left_hcnt  = READ_APB_REG_BITS(TVFE_AP_INDICATOR18, BD_B_LEFT_HCNT_BIT,  BD_B_LEFT_HCNT_WID );

    vga_auto.win.hstart     = TVAFE_H_MAX;
    if (vga_auto.win.hstart > r_left_hcnt)
        vga_auto.win.hstart = r_left_hcnt;
    if (vga_auto.win.hstart > g_left_hcnt)
        vga_auto.win.hstart = g_left_hcnt;
    if (vga_auto.win.hstart > b_left_hcnt)
        vga_auto.win.hstart = b_left_hcnt;
    vga_auto.win.hend       = TVAFE_H_MIN;
    if (vga_auto.win.hend   < r_right_hcnt)
        vga_auto.win.hend   = r_right_hcnt;
    if (vga_auto.win.hend   < g_right_hcnt)
        vga_auto.win.hend   = g_right_hcnt;
    if (vga_auto.win.hend   < b_right_hcnt)
        vga_auto.win.hend   = b_right_hcnt;
}

// *****************************************************************************
// Function:get the result of V border detection
//
//   Params: format index
//
//   Return: success/error
//
// *****************************************************************************
static void tvafe_vga_get_v_border(void)
{
    unsigned int r_top_vcnt = 0, r_bot_vcnt = 0;
    unsigned int g_top_vcnt = 0, g_bot_vcnt = 0;
    unsigned int b_top_vcnt = 0, b_bot_vcnt = 0;

    r_top_vcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR13, BD_R_TOP_VCNT_BIT, BD_R_TOP_VCNT_WID);
    r_bot_vcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR13, BD_R_BOT_VCNT_BIT, BD_R_BOT_VCNT_WID);
    g_top_vcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR15, BD_G_TOP_VCNT_BIT, BD_G_TOP_VCNT_WID);
    g_bot_vcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR15, BD_G_BOT_VCNT_BIT, BD_G_BOT_VCNT_WID);
    b_top_vcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR17, BD_B_TOP_VCNT_BIT, BD_B_TOP_VCNT_WID);
    b_bot_vcnt = READ_APB_REG_BITS(TVFE_AP_INDICATOR17, BD_B_BOT_VCNT_BIT, BD_B_BOT_VCNT_WID);

    vga_auto.win.vstart     = TVAFE_V_MAX;
    if (vga_auto.win.vstart > r_top_vcnt)
        vga_auto.win.vstart = r_top_vcnt;
    if (vga_auto.win.vstart > g_top_vcnt)
        vga_auto.win.vstart = g_top_vcnt;
    if (vga_auto.win.vstart > b_top_vcnt)
        vga_auto.win.vstart = b_top_vcnt;
    vga_auto.win.vend       = TVAFE_V_MIN;
    if (vga_auto.win.vend   < r_bot_vcnt)
        vga_auto.win.vend   = r_bot_vcnt;
    if (vga_auto.win.vend   < g_bot_vcnt)
        vga_auto.win.vend   = g_bot_vcnt;
    if (vga_auto.win.vend   < b_bot_vcnt)
        vga_auto.win.vend   = b_bot_vcnt;
}

void tvafe_vga_vs_cnt(void)
{
    if (++vga_auto.vs_cnt > TVAFE_VGA_VS_CNT_MAX)
        vga_auto.vs_cnt = TVAFE_VGA_VS_CNT_MAX;
}

// *****************************************************************************
// Function:VGA auto clock handler
//
//   Params: none
//
//   Return: sucess/error
//
// *****************************************************************************

static void tvafe_vga_auto_clock_adj(unsigned int clk, signed int diff)
{
    if (diff > 0)
        clk -= (ABS(diff) + 1) >> 1;
    if (diff < 0)
        clk += (ABS(diff) + 1) >> 1;
    tvafe_vga_set_clock(clk);
    // disable border detect
    tvafe_vga_border_detect_disable();
    // enable border detect
    //tvafe_vga_border_detect_enable();
}

static void tvafe_vga_auto_clock_handler(struct tvafe_info_s *info)
{
    unsigned int clk = 0;
    signed int diff = 0;

    //signal stable
    if (info->param.status != TVIN_SIG_STATUS_STABLE)
    {
        info->cmd_status = TVAFE_CMD_STATUS_FAILED;
        return;
    }

    switch (vga_auto.clk_state)
    {
        case VGA_CLK_IDLE:
            break;
        case VGA_CLK_INIT:
            tvafe_vga_set_phase(VGA_ADC_PHASE_MID);
            tvafe_vga_border_detect_init(info->param.fmt);
            tvafe_vga_set_clock(tvin_fmt_tbl[info->param.fmt].h_total);  //set spec clock value
            vga_auto.adj_cnt = 0;
            vga_auto.adj_dir = 0;
            vga_auto.clk_state = VGA_CLK_ROUGH_ADJ;
            vga_auto.vs_cnt  = 0;
            break;
        case VGA_CLK_ROUGH_ADJ:
            diff = 0;
            if (vga_auto.vs_cnt > AUTO_CLK_VS_CNT)
            {
                // get H border
                tvafe_vga_get_h_border();
                // get current clk
                clk = tvafe_vga_get_clock();
                // calculate new clk
                clk = (((clk * (unsigned int)tvin_fmt_tbl[info->param.fmt].h_active) << 8) / (vga_auto.win.hend - vga_auto.win.hstart + 1) + 128) >> 8;
                // if clk is too far from spec, then return error
                if ((clk > (tvin_fmt_tbl[info->param.fmt].h_total + (tvin_fmt_tbl[info->param.fmt].h_total >> MAX_AUTO_CLOCK_ORDER))) ||
                    (clk < (tvin_fmt_tbl[info->param.fmt].h_total - (tvin_fmt_tbl[info->param.fmt].h_total >> MAX_AUTO_CLOCK_ORDER)))
                   )
                {
                    vga_auto.clk_state = VGA_CLK_EXCEPTION;
                }
                else
                {
                    tvafe_vga_auto_clock_adj(clk, diff);
                    //tvafe_vga_border_detect_disable();
                    vga_auto.clk_state = VGA_CLK_FINE_ADJ;
                }
                vga_auto.vs_cnt = 0;
            }
            break;
        case VGA_CLK_FINE_ADJ:
            if (++vga_auto.adj_cnt > VGA_AUTO_TRY_COUNTER)
            {
                vga_auto.clk_state = VGA_CLK_EXCEPTION;
            }
            else
            {
                //delay about 4 field for border detection
                if (vga_auto.vs_cnt == TVAFE_VGA_BD_EN_DELAY)
                {
                    // disable border detect
                    tvafe_vga_border_detect_enable();
                }
                if (vga_auto.vs_cnt > AUTO_CLK_VS_CNT)
                {
                    //vga_auto.vs_cnt = 0;
                    // get H border
                    tvafe_vga_get_h_border();
                    // get diff
                    diff = (signed int)vga_auto.win.hend - (signed int)vga_auto.win.hstart + (signed int)1 - (signed int)tvin_fmt_tbl[info->param.fmt].h_active;
                    // get clk
                    clk = tvafe_vga_get_clock();
                    if (!diff)
                    {
                        vga_auto.clk_state = VGA_CLK_END;
                    }
                    if (diff > 0)
                    {
                        if (vga_auto.adj_dir == 1)
                        {
                            if (clk > tvin_fmt_tbl[info->param.fmt].h_total)
                            {
                                tvafe_vga_auto_clock_adj(clk, diff);
                            }
                            vga_auto.clk_state = VGA_CLK_END;
                        }
                        else
                        {
                            tvafe_vga_auto_clock_adj(clk, diff);
                            vga_auto.adj_dir = -1;
                        }
                    }
                    if (diff < 0)
                    {
                        if (vga_auto.adj_dir == -1)
                        {
                            if (clk < tvin_fmt_tbl[info->param.fmt].h_total)
                            {
                                tvafe_vga_auto_clock_adj(clk, diff);
                            }
                            vga_auto.clk_state = VGA_CLK_END;
                        }
                        else
                        {
                            tvafe_vga_auto_clock_adj(clk, diff);
                            vga_auto.adj_dir = 1;
                        }
                    }
                    vga_auto.vs_cnt = 0;
                }
            }
            break;
        case VGA_CLK_EXCEPTION: //stop auto
            // disable border detect
            tvafe_vga_border_detect_disable();
            info->cmd_status = TVAFE_CMD_STATUS_FAILED;
            vga_auto.clk_state = VGA_CLK_IDLE;
            break;
        case VGA_CLK_END: //start auto phase
            // disable border detect
            tvafe_vga_border_detect_disable();
            vga_auto.phase_state = VGA_PHASE_INIT;
            vga_auto.clk_state = VGA_CLK_IDLE;
            break;
        default:
            break;
    }

    return;
}

// *****************************************************************************
// Function:VGA auto phase handler
//
//   Params: none
//
//   Return: sucess/error
//
// *****************************************************************************
static void tvafe_vga_auto_phase_handler(struct tvafe_info_s *info)
{
    unsigned int sum = 0, hs = 0, he = 0, vs = 0, ve = 0;

    //signal stable
    if (info->param.status != TVIN_SIG_STATUS_STABLE)
    {
        info->cmd_status = TVAFE_CMD_STATUS_FAILED;
        return;
    }

    switch (vga_auto.phase_state) {
        case VGA_PHASE_IDLE:
            break;
        case VGA_PHASE_INIT:
            vga_auto.adj_cnt         = 0;
            vga_auto.ap_max_diff     = 0;
            vga_auto.ap_pha_index    = VGA_ADC_PHASE_0;
            vga_auto.ap_phamax_index = VGA_ADC_PHASE_0;
            vga_auto.ap_win_index    = VGA_PHASE_WIN_INDEX_0;
            vga_auto.ap_winmax_index = VGA_PHASE_WIN_INDEX_0;
            tvafe_vga_set_phase(vga_auto.ap_pha_index);
            tvafe_vga_auto_phase_init(info->param.fmt, vga_auto.ap_win_index);
            vga_auto.phase_state = VGA_PHASE_SEARCH_WIN;
            vga_auto.vs_cnt          = 0;
            break;
        case VGA_PHASE_SEARCH_WIN:
            if (++vga_auto.adj_cnt > VGA_AUTO_TRY_COUNTER)
            {
                vga_auto.phase_state = VGA_PHASE_EXCEPTION;
            }
            else
            {
                if (vga_auto.vs_cnt > AUTO_PHASE_VS_CNT)
                {

                    //vga_auto.vs_cnt = 0;
                    sum = tvafe_vga_get_ap_diff();
                    if (vga_auto.ap_max_diff < sum)
                    {
                        vga_auto.ap_max_diff = sum;
                        vga_auto.ap_winmax_index = vga_auto.ap_win_index;
                    }
                    if (++vga_auto.ap_win_index > VGA_PHASE_WIN_INDEX_MAX)
                    {
                        tvafe_adc_set_ap_window(info->param.fmt, vga_auto.ap_winmax_index);
                        vga_auto.ap_max_diff = 0;
                        vga_auto.phase_state = VGA_PHASE_ADJ;
                    }
                    else
                        tvafe_adc_set_ap_window(info->param.fmt, vga_auto.ap_win_index);
                    vga_auto.vs_cnt = 0;
                }
            }
            break;
        case VGA_PHASE_ADJ:
            if (++vga_auto.adj_cnt > VGA_AUTO_TRY_COUNTER)
            {
                vga_auto.phase_state = VGA_PHASE_EXCEPTION;
            }
            else
            {
                if (vga_auto.vs_cnt > AUTO_PHASE_VS_CNT)
                {
                    vga_auto.vs_cnt = 0;
                    sum = tvafe_vga_get_ap_diff();
                    if (vga_auto.ap_max_diff < sum)
                    {
                        vga_auto.ap_max_diff = sum;
                        vga_auto.ap_phamax_index = vga_auto.ap_pha_index;
                    }
                    if (++vga_auto.ap_pha_index > VGA_ADC_PHASE_MAX)
                    {
                        tvafe_vga_set_phase(vga_auto.ap_phamax_index);
                        //enable border detect
                        tvafe_vga_border_detect_enable();
                        //tvafe_vga_auto_phase_disable();
                        //tvafe_vga_border_detect_init(info->param.fmt);
                        vga_auto.phase_state = VGA_PHASE_END;
                    }
                    else
                        tvafe_vga_set_phase(vga_auto.ap_pha_index);
                    vga_auto.vs_cnt = 0;
                }
            }
            break;
        case VGA_PHASE_EXCEPTION: //stop auto
            // disable auto phase
            tvafe_vga_auto_phase_disable();
            info->cmd_status = TVAFE_CMD_STATUS_FAILED;
            vga_auto.phase_state = VGA_PHASE_IDLE;
        case VGA_PHASE_END: //auto position
            if (vga_auto.vs_cnt > AUTO_CLK_VS_CNT)
            {
                //vga_auto.vs_cnt = 0;
                tvafe_vga_get_h_border();
                tvafe_vga_get_v_border();
                if (((vga_auto.win.hend - vga_auto.win.hstart + 1) >= tvin_fmt_tbl[info->param.fmt].h_active) ||
                    (vga_auto.win.hend > (tvin_fmt_tbl[info->param.fmt].hs_width + tvin_fmt_tbl[info->param.fmt].hs_bp + tvin_fmt_tbl[info->param.fmt].h_active - 1))
                   )
                {
                    he = vga_auto.win.hend;
                    hs = he - tvin_fmt_tbl[info->param.fmt].h_active + 1;
                }
                else if (vga_auto.win.hstart < (tvin_fmt_tbl[info->param.fmt].hs_width + tvin_fmt_tbl[info->param.fmt].hs_bp))
                {
                    hs = vga_auto.win.hstart;
                    he = hs + tvin_fmt_tbl[info->param.fmt].h_active - 1;
                }
                else
                {
                    hs = tvin_fmt_tbl[info->param.fmt].hs_width + tvin_fmt_tbl[info->param.fmt].hs_bp;
                    he = hs + tvin_fmt_tbl[info->param.fmt].h_active - 1;
                }
                if (((vga_auto.win.vend - vga_auto.win.vstart + 1) >= tvin_fmt_tbl[info->param.fmt].v_active) ||
                    (vga_auto.win.vend > (tvin_fmt_tbl[info->param.fmt].vs_width + tvin_fmt_tbl[info->param.fmt].vs_bp + tvin_fmt_tbl[info->param.fmt].v_active - 1))
                   )
                {
                    ve = vga_auto.win.vend;
                    vs = ve - tvin_fmt_tbl[info->param.fmt].v_active + 1;
                }
                else if (vga_auto.win.vstart < (tvin_fmt_tbl[info->param.fmt].vs_width + tvin_fmt_tbl[info->param.fmt].vs_bp))
                {
                    vs = vga_auto.win.vstart;
                    ve = vs + tvin_fmt_tbl[info->param.fmt].v_active - 1;
                }
                else
                {
                    vs = tvin_fmt_tbl[info->param.fmt].vs_width + tvin_fmt_tbl[info->param.fmt].vs_bp;
                    ve = vs + tvin_fmt_tbl[info->param.fmt].v_active - 1;
                }
                tvafe_vga_set_h_pos(hs, he);
                tvafe_vga_set_v_pos(vs, ve, tvafe_top_get_scan_mode());
                // disable border detect
                tvafe_vga_border_detect_disable();
                // disable auto phase
                tvafe_vga_auto_phase_disable();
                info->cmd_status = TVAFE_CMD_STATUS_SUCCESSFUL;
                vga_auto.phase_state = VGA_PHASE_IDLE;
                vga_auto.vs_cnt = 0;
            }
            break;
        default:
            break;
    }

    return;
}

// *****************************************************************************
// Function:VGA auto adjust enable
//
//   Params: none
//
//   Return: sucess/error
//
// *****************************************************************************
int tvafe_vga_auto_adjust_enable(struct tvafe_info_s *info)
{
    int ret = 0;

    if ((info->src_type == TVAFE_SRC_TYPE_VGA) &&
        (info->param.status == TVIN_SIG_STATUS_STABLE) &&
        (info->cmd_status  == TVAFE_CMD_STATUS_IDLE)
       )
    {
        info->cmd_status     = TVAFE_CMD_STATUS_PROCESSING;
        vga_auto.clk_state   = VGA_CLK_INIT;
        vga_auto.phase_state = VGA_PHASE_IDLE;
        info->vga_auto_flag  = 1;
    }
    else
    {
        info->cmd_status     = TVAFE_CMD_STATUS_FAILED;
        vga_auto.clk_state   = VGA_CLK_IDLE;
        vga_auto.phase_state = VGA_PHASE_IDLE;
        info->vga_auto_flag  = 0;
        ret = -EFAULT;
    }

    return ret;
}

void tvafe_vga_auto_adjust_disable(struct tvafe_info_s *info)
{
    if (info->cmd_status     == TVAFE_CMD_STATUS_PROCESSING)
    {
        info->cmd_status     = TVAFE_CMD_STATUS_TERMINATED;
        vga_auto.clk_state   = VGA_CLK_IDLE;
        vga_auto.phase_state = VGA_PHASE_IDLE;
        info->vga_auto_flag  = 0;
    }
}

// *****************************************************************************
// Function: VGA auto adjust handler
//
//   Params: system info
//
//   Return: success/error
//
// *****************************************************************************
void tvafe_vga_auto_adjust_handler(struct tvafe_info_s *info)
{

    //auto clock handler
    tvafe_vga_auto_clock_handler(info);

    // auto phase handler after auto clock
    tvafe_vga_auto_phase_handler(info);

    return;
}

// *****************************************************************************
// Function:configure the format setting
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_adc_clear(unsigned int val, unsigned int clear)
{
    unsigned int i=0;

    for (i=0; i<ADC_REG_NUM; i++)
    {
        if (clear)
        {
            WRITE_APB_REG((ADC_BASE_ADD+i)<<2, ((i == 0x21) ? val : 0));
        }
        else
        {
            WRITE_APB_REG(ADC_REG_21, val);
        }
    }
}

void tvafe_adc_configure(enum tvin_sig_fmt_e fmt)
{
    int i;
    const unsigned char *buff_t = NULL;

    //set adc clock by standard
    //tvafe_adc_set_clock(fmt);

    //set adc clamp by standard
    //tvafe_adc_set_clamp(fmt);

    //set channel bandwidth
    //tvafe_adc_set_bw_lpf(fmt);

    //load vga reg hardcode
    //tvafe_adc_load_hardcode();

#if 1

    tvafe_adc_clear(TVAFE_ADC_CONFIGURE_INIT, 1);
    tvafe_adc_clear(TVAFE_ADC_CONFIGURE_NORMAL, 1);
    tvafe_adc_clear(TVAFE_ADC_CONFIGURE_RESET_ON, 1);
    if (fmt < TVIN_SIG_FMT_VGA_MAX) // VGA formats
    {
        buff_t = adc_vga_table[fmt-TVIN_SIG_FMT_NULL-1];
    }
    else if (fmt < TVIN_SIG_FMT_COMP_MAX) // Component formats
    {
        buff_t = adc_component_table[fmt-TVIN_SIG_FMT_VGA_MAX-1];
    }
    else // CVBS formats
    {
        //pr_info("tvafe:  tvafe_adc_configure(CVBS)\n");
        buff_t = adc_cvbs_table;
    }

    for (i=0; i<ADC_REG_NUM; i++)
    {
        WRITE_APB_REG((ADC_BASE_ADD+i)<<2, ((i == 0x21) ? TVAFE_ADC_CONFIGURE_RESET_ON : (unsigned int)(buff_t[i])));
    }
    tvafe_adc_clear(TVAFE_ADC_CONFIGURE_NORMAL, 0);

#if 0
    //debug setting
    // diable other mux on test pins 0~27 & 30
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_0 , READ_CBUS_REG(PERIPHS_PIN_MUX_0 )&0xcff0ffdf);
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_1 , READ_CBUS_REG(PERIPHS_PIN_MUX_1 )&0xfc017fff);
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_2 , READ_CBUS_REG(PERIPHS_PIN_MUX_2 )&0xe001ffff);
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_3 , READ_CBUS_REG(PERIPHS_PIN_MUX_3 )&0xfc000000);
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_4 , READ_CBUS_REG(PERIPHS_PIN_MUX_4 )&0xff8007ff);
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_6 , READ_CBUS_REG(PERIPHS_PIN_MUX_6 )&0xffffffbf);
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_7 , READ_CBUS_REG(PERIPHS_PIN_MUX_7 )&0xff00003f);
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_10, READ_CBUS_REG(PERIPHS_PIN_MUX_10)&0xffffffb3);
    // enable TVFE_TEST mux on test pins 0~27 & 30
    WRITE_CBUS_REG(PERIPHS_PIN_MUX_9 , 0x4fffffff);//
#endif

#else
    for (i=0; i<ADC_REG_NUM; i++)
    {
        if (fmt < TVIN_SIG_FMT_VGA_MAX) // VGA formats
        {
            //pr_info("tvafe:  tvafe_adc_configure(VGA)\n");
            WRITE_APB_REG((ADC_BASE_ADD+i)<<2, (unsigned int)(adc_vga_table[fmt-TVIN_SIG_FMT_NULL-1][i]));
        }
        else if (fmt < TVIN_SIG_FMT_COMP_MAX) // Component formats
        {
            //pr_info("tvafe:  tvafe_adc_configure(COMP)\n");
            WRITE_APB_REG((ADC_BASE_ADD+i)<<2, (unsigned int)(adc_component_table[fmt-TVIN_SIG_FMT_VGA_MAX-1][i]));
        }
        else // CVBS formats
        {
            //pr_info("tvafe:  tvafe_adc_configure(CVBS)\n");
            WRITE_APB_REG((ADC_BASE_ADD+i)<<2, (unsigned int)(adc_cvbs_table[i]));
        }
    }

#endif

}

void tvafe_adc_set_param(struct tvafe_vga_parm_s *adc_parm, struct tvafe_info_s *info)
{
    signed int data = 0;
    unsigned int tmp = 0;

    enum tvin_scan_mode_e scan_mode = tvafe_top_get_scan_mode();

    // clk
    data = tvin_fmt_tbl[info->param.fmt].h_total >> TVAFE_VGA_CLK_TUNE_RANGE_ORDER;
    if (ABS(adc_parm->clk_step) > data)
    {
        adc_parm->clk_step = (adc_parm->clk_step > 0) ? data : (0 - data);
    }
    tmp = (unsigned int)((signed int)tvin_fmt_tbl[info->param.fmt].h_total + adc_parm->clk_step);
    tvafe_vga_set_clock(tmp);
    // phase
    if (adc_parm->phase > VGA_ADC_PHASE_MAX)
        adc_parm->phase = VGA_ADC_PHASE_MAX;
    tmp = adc_parm->phase;
    tvafe_vga_set_phase(tmp);
    // hpos
    data = tvin_fmt_tbl[info->param.fmt].h_active >> TVAFE_VGA_HPOS_TUNE_RANGE_ORDER;
    if (ABS(adc_parm->hpos_step) > data)
    {
        adc_parm->hpos_step = (adc_parm->hpos_step > 0) ? data : (0 - data);
    }
    tmp = (unsigned int)((signed int)tvin_fmt_tbl[info->param.fmt].hs_width + (signed int)tvin_fmt_tbl[info->param.fmt].hs_bp + adc_parm->hpos_step);
    tvafe_vga_set_h_pos(tmp, tmp + tvin_fmt_tbl[info->param.fmt].h_active - 1);
    // vpos
    data = tvin_fmt_tbl[info->param.fmt].v_active >> ((scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) ? TVAFE_VGA_VPOS_TUNE_RANGE_ORDER : TVAFE_VGA_VPOS_TUNE_RANGE_ORDER - 1);
    if (ABS(adc_parm->vpos_step) > data)
    {
        adc_parm->vpos_step = (adc_parm->vpos_step > 0) ? data : (0 - data);
    }
    tmp = (unsigned int)((signed int)tvin_fmt_tbl[info->param.fmt].vs_width + (signed int)tvin_fmt_tbl[info->param.fmt].vs_bp + adc_parm->vpos_step);
    tvafe_vga_set_v_pos(tmp, tmp + tvin_fmt_tbl[info->param.fmt].v_active - 1, scan_mode);
}

void tvafe_adc_get_param(struct tvafe_vga_parm_s *adc_parm, struct tvafe_info_s *info)
{
    //signed int tmp = 0;

    adc_parm->clk_step  = (signed int)tvafe_vga_get_clock() - (signed int)tvin_fmt_tbl[info->param.fmt].h_total;
    adc_parm->phase     = tvafe_vga_get_phase();
    adc_parm->hpos_step = (signed int)tvin_fmt_tbl[info->param.fmt].hs_width + (signed int)tvin_fmt_tbl[info->param.fmt].hs_bp - (signed int)tvafe_vga_get_h_pos();
    adc_parm->vpos_step = (signed int)tvin_fmt_tbl[info->param.fmt].vs_width + (signed int)tvin_fmt_tbl[info->param.fmt].vs_bp - (signed int)tvafe_vga_get_v_pos();
}

/* TOP */ //TVIN_SIG_FMT_VGA_800X600P_60D317
const static  int vga_top_reg_default[][2] = {
    {TVFE_DVSS_MUXCTRL             , 0x07000008,} ,// TVFE_DVSS_MUXCTRL
    {TVFE_DVSS_MUXVS_REF           , 0x00000000,} ,// TVFE_DVSS_MUXVS_REF
    {TVFE_DVSS_MUXCOAST_V          , 0x0200000c,} ,// TVFE_DVSS_MUXCOAST_V
    {TVFE_DVSS_SEP_HVWIDTH         , 0x000a0073,} ,// TVFE_DVSS_SEP_HVWIDTH
    {TVFE_DVSS_SEP_HPARA           , 0x026b0343,} ,// TVFE_DVSS_SEP_HPARA
    {TVFE_DVSS_SEP_VINTEG          , 0x0fff0100,} ,// TVFE_DVSS_SEP_VINTEG
    {TVFE_DVSS_SEP_H_THR           , 0x00005002,} ,// TVFE_DVSS_SEP_H_THR
    {TVFE_DVSS_SEP_CTRL            , 0x40000008,} ,// TVFE_DVSS_SEP_CTRL
    {TVFE_DVSS_GEN_WIDTH           , 0x000a0073,} ,// TVFE_DVSS_GEN_WIDTH
    {TVFE_DVSS_GEN_PRD             , 0x020d0359,} ,// TVFE_DVSS_GEN_PRD
    {TVFE_DVSS_GEN_COAST           , 0x01cc001c,} ,// TVFE_DVSS_GEN_COAST
    {TVFE_DVSS_NOSIG_PARA          , 0x00000009,} ,// TVFE_DVSS_NOSIG_PARA
    {TVFE_DVSS_NOSIG_PLS_TH        , 0x05000010,} ,// TVFE_DVSS_NOSIG_PLS_TH
    {TVFE_DVSS_GATE_H              , 0x00270016,} ,// TVFE_DVSS_GATE_H
    {TVFE_DVSS_GATE_V              , 0x020a0009,} ,// TVFE_DVSS_GATE_V
    {TVFE_DVSS_INDICATOR1          , 0x00000000,} ,// TVFE_DVSS_INDICATOR1
    {TVFE_DVSS_INDICATOR2          , 0x00000000,} ,// TVFE_DVSS_INDICATOR2
    {TVFE_DVSS_MVDET_CTRL1         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL1
    {TVFE_DVSS_MVDET_CTRL2         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL2
    {TVFE_DVSS_MVDET_CTRL3         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL3
    {TVFE_DVSS_MVDET_CTRL4         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL4
    {TVFE_DVSS_MVDET_CTRL5         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL5
    {TVFE_DVSS_MVDET_CTRL6         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL6
    {TVFE_DVSS_MVDET_CTRL7         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL7
    {TVFE_SYNCTOP_SPOL_MUXCTRL     , 0x00000009,} ,// TVFE_SYNCTOP_SPOL_MUXCTRL
    {TVFE_SYNCTOP_INDICATOR1_HCNT  , 0x00000000,} ,// TVFE_SYNCTOP_INDICATOR1_HCNT
    {TVFE_SYNCTOP_INDICATOR2_VCNT  , 0x00000000,} ,// TVFE_SYNCTOP_INDICATOR2_VCNT
    {TVFE_SYNCTOP_INDICATOR3       , 0x00000000,} ,// TVFE_SYNCTOP_INDICATOR3
    {TVFE_SYNCTOP_SFG_MUXCTRL1     , 0x81315107,} ,// TVFE_SYNCTOP_SFG_MUXCTRL1
    {TVFE_SYNCTOP_SFG_MUXCTRL2     , 0x01330000,} ,// TVFE_SYNCTOP_SFG_MUXCTRL2
    {TVFE_SYNCTOP_INDICATOR4       , 0x00000000,} ,// TVFE_SYNCTOP_INDICATOR4
    {TVFE_SYNCTOP_SAM_MUXCTRL      , 0x00082001,} ,// TVFE_SYNCTOP_SAM_MUXCTRL
    {TVFE_MISC_WSS1_MUXCTRL1       , 0x00000000,} ,// TVFE_MISC_WSS1_MUXCTRL1
    {TVFE_MISC_WSS1_MUXCTRL2       , 0x00000000,} ,// TVFE_MISC_WSS1_MUXCTRL2
    {TVFE_MISC_WSS2_MUXCTRL1       , 0x00000000,} ,// TVFE_MISC_WSS2_MUXCTRL1
    {TVFE_MISC_WSS2_MUXCTRL2       , 0x00000000,} ,// TVFE_MISC_WSS2_MUXCTRL2
    {TVFE_MISC_WSS1_INDICATOR1     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR1
    {TVFE_MISC_WSS1_INDICATOR2     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR2
    {TVFE_MISC_WSS1_INDICATOR3     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR3
    {TVFE_MISC_WSS1_INDICATOR4     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR4
    {TVFE_MISC_WSS1_INDICATOR5     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR5
    {TVFE_MISC_WSS2_INDICATOR1     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR1
    {TVFE_MISC_WSS2_INDICATOR2     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR2
    {TVFE_MISC_WSS2_INDICATOR3     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR3
    {TVFE_MISC_WSS2_INDICATOR4     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR4
    {TVFE_MISC_WSS2_INDICATOR5     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR5
    {TVFE_AP_MUXCTRL1              , 0x19310010,} ,// TVFE_AP_MUXCTRL1
    {TVFE_AP_MUXCTRL2              , 0x00200010,} ,// TVFE_AP_MUXCTRL2
    {TVFE_AP_MUXCTRL3              , 0x10000030,} ,// TVFE_AP_MUXCTRL3
    {TVFE_AP_MUXCTRL4              , 0x00000000,} ,// TVFE_AP_MUXCTRL4
    {TVFE_AP_MUXCTRL5              , 0x10040000,} ,// TVFE_AP_MUXCTRL5
    {TVFE_AP_INDICATOR1            , 0x00000000,} ,// TVFE_AP_INDICATOR1
    {TVFE_AP_INDICATOR2            , 0x00000000,} ,// TVFE_AP_INDICATOR2
    {TVFE_AP_INDICATOR3            , 0x00000000,} ,// TVFE_AP_INDICATOR3
    {TVFE_AP_INDICATOR4            , 0x00000000,} ,// TVFE_AP_INDICATOR4
    {TVFE_AP_INDICATOR5            , 0x00000000,} ,// TVFE_AP_INDICATOR5
    {TVFE_AP_INDICATOR6            , 0x00000000,} ,// TVFE_AP_INDICATOR6
    {TVFE_AP_INDICATOR7            , 0x00000000,} ,// TVFE_AP_INDICATOR7
    {TVFE_AP_INDICATOR8            , 0x00000000,} ,// TVFE_AP_INDICATOR8
    {TVFE_AP_INDICATOR9            , 0x00000000,} ,// TVFE_AP_INDICATOR9
    {TVFE_AP_INDICATOR10           , 0x00000000,} ,// TVFE_AP_INDICATOR10
    {TVFE_AP_INDICATOR11           , 0x00000000,} ,// TVFE_AP_INDICATOR11
    {TVFE_AP_INDICATOR12           , 0x00000000,} ,// TVFE_AP_INDICATOR12
    {TVFE_AP_INDICATOR13           , 0x00000000,} ,// TVFE_AP_INDICATOR13
    {TVFE_AP_INDICATOR14           , 0x00000000,} ,// TVFE_AP_INDICATOR14
    {TVFE_AP_INDICATOR15           , 0x00000000,} ,// TVFE_AP_INDICATOR15
    {TVFE_AP_INDICATOR16           , 0x00000000,} ,// TVFE_AP_INDICATOR16
    {TVFE_AP_INDICATOR17           , 0x00000000,} ,// TVFE_AP_INDICATOR17
    {TVFE_AP_INDICATOR18           , 0x00000000,} ,// TVFE_AP_INDICATOR18
    {TVFE_AP_INDICATOR19           , 0x00000000,} ,// TVFE_AP_INDICATOR19
    {TVFE_BD_MUXCTRL1              , 0x01320000,} ,// TVFE_BD_MUXCTRL1
    {TVFE_BD_MUXCTRL2              , 0x0020d000,} ,// TVFE_BD_MUXCTRL2
    {TVFE_BD_MUXCTRL3              , 0x00000000,} ,// TVFE_BD_MUXCTRL3
    {TVFE_BD_MUXCTRL4              , 0x00000000,} ,// TVFE_BD_MUXCTRL4
    {TVFE_CLP_MUXCTRL1             , 0x00000000,} ,// TVFE_CLP_MUXCTRL1
    {TVFE_CLP_MUXCTRL2             , 0x00000000,} ,// TVFE_CLP_MUXCTRL2
    {TVFE_CLP_MUXCTRL3             , 0x00000000,} ,// TVFE_CLP_MUXCTRL3
    {TVFE_CLP_MUXCTRL4             , 0x00000000,} ,// TVFE_CLP_MUXCTRL4
    {TVFE_CLP_INDICATOR1           , 0x00000000,} ,// TVFE_CLP_INDICATOR1
    {TVFE_BPG_BACKP_H              , 0x00000000,} ,// TVFE_BPG_BACKP_H
    {TVFE_BPG_BACKP_V              , 0x00000000,} ,// TVFE_BPG_BACKP_V
    {TVFE_DEG_H                    , 0x003f80d8,} ,// TVFE_DEG_H
    {TVFE_DEG_VODD                 , 0x0027301b,} ,// TVFE_DEG_VODD
    {TVFE_DEG_VEVEN                , 0x0027301b,} ,// TVFE_DEG_VEVEN
    {TVFE_OGO_OFFSET1              , 0x00000000,} ,// TVFE_OGO_OFFSET1
    {TVFE_OGO_GAIN1                , 0x00000000,} ,// TVFE_OGO_GAIN1
    {TVFE_OGO_GAIN2                , 0x00000000,} ,// TVFE_OGO_GAIN2
    {TVFE_OGO_OFFSET2              , 0x00000000,} ,// TVFE_OGO_OFFSET2
    {TVFE_OGO_OFFSET3              , 0x00000000,} ,// TVFE_OGO_OFFSET3
    {TVFE_VAFE_CTRL                , 0x00000001,} ,// TVFE_VAFE_CTRL
    {TVFE_VAFE_STATUS              , 0x00000000,} ,// TVFE_VAFE_STATUS
    {TVFE_TOP_CTRL                 , 0x00008750,} ,// TVFE_TOP_CTRL
    {TVFE_CLAMP_INTF               , 0x00000000,} ,// TVFE_CLAMP_INTF
    {TVFE_RST_CTRL                 , 0x00000000,} ,// TVFE_RST_CTRL
    {TVFE_EXT_VIDEO_AFE_CTRL_MUX1  , 0x00000000,} ,// TVFE_EXT_VIDEO_AFE_CTRL_MUX1
    {TVFE_AAFILTER_CTRL1           , 0x00082222,} ,// TVFE_AAFILTER_CTRL1
    {TVFE_AAFILTER_CTRL2           , 0x252b39c6,} ,// TVFE_AAFILTER_CTRL2
    {TVFE_EDID_CONFIG              , 0x01800050,} ,// TVFE_EDID_CONFIG
    {TVFE_EDID_RAM_ADDR            , 0x00000100,} ,// TVFE_EDID_RAM_ADDR
    {TVFE_EDID_RAM_WDATA           , 0x00000000,} ,// TVFE_EDID_RAM_WDATA
    {TVFE_EDID_RAM_RDATA           , 0x00000000,} ,// TVFE_EDID_RAM_RDATA
    {TVFE_APB_ERR_CTRL_MUX1        , 0x00000000,} ,// TVFE_APB_ERR_CTRL_MUX1
    {TVFE_APB_ERR_CTRL_MUX2        , 0x00000000,} ,// TVFE_APB_ERR_CTRL_MUX2
    {TVFE_APB_INDICATOR1           , 0x00000000,} ,// TVFE_APB_INDICATOR1
    {TVFE_APB_INDICATOR2           , 0x00000000,} ,// TVFE_APB_INDICATOR2
    {TVFE_ADC_READBACK_CTRL        , 0xa0142003,} ,// TVFE_ADC_READBACK_CTRL
    {TVFE_ADC_READBACK_INDICATOR   , 0x00000000,} ,// TVFE_ADC_READBACK_INDICATOR
    {TVFE_INT_CLR                  , 0x00000000,} ,// TVFE_INT_CLR
    {TVFE_INT_MSKN                 , 0x00000000,} ,// TVFE_INT_MASKN
    {TVFE_INT_INDICATOR1           , 0x00000000,} ,// TVFE_INT_INDICATOR1
    {TVFE_INT_SET                  , 0x00000000,} ,// TVFE_INT_SET
    {TVFE_CHIP_VERSION             , 0x00000000,} ,// TVFE_CHIP_VERSION
    {0xFFFFFFFF                    , 0x00000000,} // TVFE_CHIP_VERSION
}; //TVIN_SIG_FMT_VGA_800X600P_60D317

void tvafe_set_vga_default(enum tvin_sig_fmt_e fmt)
{
    unsigned int i = 0;

    /** write top register **/
    while (vga_top_reg_default[i][0] != 0xFFFFFFFF) {
        WRITE_APB_REG(vga_top_reg_default[i][0], vga_top_reg_default[i][1]);
        i++;
    }

    pr_info("tvafe_set_vga_default fmt=%d\n", (int)fmt);

    /** write 7740 register **/
    tvafe_adc_configure(fmt);

}
/* TOP */

///zhuang wei
const static  int comp_top_reg_default[][2] = {
   {TVFE_DVSS_MUXCTRL              , 0x072a1480,} ,// TVFE_DVSS_MUXCTRL //zhuang
    {TVFE_DVSS_MUXVS_REF           , 0x00000000,} ,// TVFE_DVSS_MUXVS_REF
    {TVFE_DVSS_MUXCOAST_V          , 0x00000000,} ,// TVFE_DVSS_MUXCOAST_V
    {TVFE_DVSS_SEP_HVWIDTH         , 0x00000000,} ,// TVFE_DVSS_SEP_HVWIDTH
    {TVFE_DVSS_SEP_HPARA           , 0x00000000,} ,// TVFE_DVSS_SEP_HPARA
    {TVFE_DVSS_SEP_VINTEG          , 0x00000000,} ,// TVFE_DVSS_SEP_VINTEG
    {TVFE_DVSS_SEP_H_THR           , 0x00000000,} ,// TVFE_DVSS_SEP_H_THR
    {TVFE_DVSS_SEP_CTRL            , 0x00000000,} ,// TVFE_DVSS_SEP_CTRL
    {TVFE_DVSS_GEN_WIDTH           , 0x00000000,} ,// TVFE_DVSS_GEN_WIDTH
    {TVFE_DVSS_GEN_PRD             , 0x00000000,} ,// TVFE_DVSS_GEN_PRD
    {TVFE_DVSS_GEN_COAST           , 0x00000000,} ,// TVFE_DVSS_GEN_COAST
    {TVFE_DVSS_NOSIG_PARA          , 0x00000000,} ,// TVFE_DVSS_NOSIG_PARA
    {TVFE_DVSS_NOSIG_PLS_TH        , 0x00000000,} ,// TVFE_DVSS_NOSIG_PLS_TH
    {TVFE_DVSS_GATE_H              , 0x00000000,} ,// TVFE_DVSS_GATE_H
    {TVFE_DVSS_GATE_V              , 0x00000000,} ,// TVFE_DVSS_GATE_V
    {TVFE_DVSS_INDICATOR1          , 0x00000000,} ,// TVFE_DVSS_INDICATOR1
    {TVFE_DVSS_INDICATOR2          , 0x00000000,} ,// TVFE_DVSS_INDICATOR2
    {TVFE_DVSS_MVDET_CTRL1         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL1
    {TVFE_DVSS_MVDET_CTRL2         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL2
    {TVFE_DVSS_MVDET_CTRL3         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL3
    {TVFE_DVSS_MVDET_CTRL4         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL4
    {TVFE_DVSS_MVDET_CTRL5         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL5
    {TVFE_DVSS_MVDET_CTRL6         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL6
    {TVFE_DVSS_MVDET_CTRL7         , 0x00000000,} ,// TVFE_DVSS_MVDET_CTRL7
    {TVFE_SYNCTOP_SPOL_MUXCTRL     , 0x00000009,} ,// TVFE_SYNCTOP_SPOL_MUXCTRL
    {TVFE_SYNCTOP_INDICATOR1_HCNT  , 0x00000000,} ,// TVFE_SYNCTOP_INDICATOR1_HCNT
    {TVFE_SYNCTOP_INDICATOR2_VCNT  , 0x00000000,} ,// TVFE_SYNCTOP_INDICATOR2_VCNT
    {TVFE_SYNCTOP_INDICATOR3       , 0x00000000,} ,// TVFE_SYNCTOP_INDICATOR3
    {TVFE_SYNCTOP_SFG_MUXCTRL1     , 0x812880d8,} ,// TVFE_SYNCTOP_SFG_MUXCTRL1
    {TVFE_SYNCTOP_SFG_MUXCTRL2     , 0x00334400,} ,// TVFE_SYNCTOP_SFG_MUXCTRL2
    {TVFE_SYNCTOP_INDICATOR4       , 0x00000000,} ,// TVFE_SYNCTOP_INDICATOR4
    {TVFE_SYNCTOP_SAM_MUXCTRL      , 0x00082001,} ,// TVFE_SYNCTOP_SAM_MUXCTRL
    {TVFE_MISC_WSS1_MUXCTRL1       , 0x00000000,} ,// TVFE_MISC_WSS1_MUXCTRL1
    {TVFE_MISC_WSS1_MUXCTRL2       , 0x00000000,} ,// TVFE_MISC_WSS1_MUXCTRL2
    {TVFE_MISC_WSS2_MUXCTRL1       , 0x00000000,} ,// TVFE_MISC_WSS2_MUXCTRL1
    {TVFE_MISC_WSS2_MUXCTRL2       , 0x00000000,} ,// TVFE_MISC_WSS2_MUXCTRL2
    {TVFE_MISC_WSS1_INDICATOR1     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR1
    {TVFE_MISC_WSS1_INDICATOR2     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR2
    {TVFE_MISC_WSS1_INDICATOR3     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR3
    {TVFE_MISC_WSS1_INDICATOR4     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR4
    {TVFE_MISC_WSS1_INDICATOR5     , 0x00000000,} ,// TVFE_MISC_WSS1_INDICATOR5
    {TVFE_MISC_WSS2_INDICATOR1     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR1
    {TVFE_MISC_WSS2_INDICATOR2     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR2
    {TVFE_MISC_WSS2_INDICATOR3     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR3
    {TVFE_MISC_WSS2_INDICATOR4     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR4
    {TVFE_MISC_WSS2_INDICATOR5     , 0x00000000,} ,// TVFE_MISC_WSS2_INDICATOR5
    {TVFE_AP_MUXCTRL1              , 0x00000000,} ,// TVFE_AP_MUXCTRL1
    {TVFE_AP_MUXCTRL2              , 0x00000000,} ,// TVFE_AP_MUXCTRL2
    {TVFE_AP_MUXCTRL3              , 0x00000000,} ,// TVFE_AP_MUXCTRL3
    {TVFE_AP_MUXCTRL4              , 0x00000000,} ,// TVFE_AP_MUXCTRL4
    {TVFE_AP_MUXCTRL5              , 0x00000000,} ,// TVFE_AP_MUXCTRL5
    {TVFE_AP_INDICATOR1            , 0x00000000,} ,// TVFE_AP_INDICATOR1
    {TVFE_AP_INDICATOR2            , 0x00000000,} ,// TVFE_AP_INDICATOR2
    {TVFE_AP_INDICATOR3            , 0x00000000,} ,// TVFE_AP_INDICATOR3
    {TVFE_AP_INDICATOR4            , 0x00000000,} ,// TVFE_AP_INDICATOR4
    {TVFE_AP_INDICATOR5            , 0x00000000,} ,// TVFE_AP_INDICATOR5
    {TVFE_AP_INDICATOR6            , 0x00000000,} ,// TVFE_AP_INDICATOR6
    {TVFE_AP_INDICATOR7            , 0x00000000,} ,// TVFE_AP_INDICATOR7
    {TVFE_AP_INDICATOR8            , 0x00000000,} ,// TVFE_AP_INDICATOR8
    {TVFE_AP_INDICATOR9            , 0x00000000,} ,// TVFE_AP_INDICATOR9
    {TVFE_AP_INDICATOR10           , 0x00000000,} ,// TVFE_AP_INDICATOR10
    {TVFE_AP_INDICATOR11           , 0x00000000,} ,// TVFE_AP_INDICATOR11
    {TVFE_AP_INDICATOR12           , 0x00000000,} ,// TVFE_AP_INDICATOR12
    {TVFE_AP_INDICATOR13           , 0x00000000,} ,// TVFE_AP_INDICATOR13
    {TVFE_AP_INDICATOR14           , 0x00000000,} ,// TVFE_AP_INDICATOR14
    {TVFE_AP_INDICATOR15           , 0x00000000,} ,// TVFE_AP_INDICATOR15
    {TVFE_AP_INDICATOR16           , 0x00000000,} ,// TVFE_AP_INDICATOR16
    {TVFE_AP_INDICATOR17           , 0x00000000,} ,// TVFE_AP_INDICATOR17
    {TVFE_AP_INDICATOR18           , 0x00000000,} ,// TVFE_AP_INDICATOR18
    {TVFE_AP_INDICATOR19           , 0x00000000,} ,// TVFE_AP_INDICATOR19
    {TVFE_BD_MUXCTRL1              , 0x00000000,} ,// TVFE_BD_MUXCTRL1
    {TVFE_BD_MUXCTRL2              , 0x00000000,} ,// TVFE_BD_MUXCTRL2
    {TVFE_BD_MUXCTRL3              , 0x00000000,} ,// TVFE_BD_MUXCTRL3
    {TVFE_BD_MUXCTRL4              , 0x00000000,} ,// TVFE_BD_MUXCTRL4
    {TVFE_CLP_MUXCTRL1             , 0x00000000,} ,// TVFE_CLP_MUXCTRL1
    {TVFE_CLP_MUXCTRL2             , 0x00000000,} ,// TVFE_CLP_MUXCTRL2
    {TVFE_CLP_MUXCTRL3             , 0x00000000,} ,// TVFE_CLP_MUXCTRL3
    {TVFE_CLP_MUXCTRL4             , 0x00000000,} ,// TVFE_CLP_MUXCTRL4
    {TVFE_CLP_INDICATOR1           , 0x00000000,} ,// TVFE_CLP_INDICATOR1
    {TVFE_BPG_BACKP_H              , 0x00000000,} ,// TVFE_BPG_BACKP_H
    {TVFE_BPG_BACKP_V              , 0x00000000,} ,// TVFE_BPG_BACKP_V
    {TVFE_DEG_H                    , 0x00621121,} ,// TVFE_DEG_H
    {TVFE_DEG_VODD                 , 0x002e8018,} ,// TVFE_DEG_VODD //zhuang
    {TVFE_DEG_VEVEN                , 0x002e8018,} ,// TVFE_DEG_VEVEN //zhuang
    {TVFE_OGO_OFFSET1              , 0x00000000,} ,// TVFE_OGO_OFFSET1
    {TVFE_OGO_GAIN1                , 0x00000000,} ,// TVFE_OGO_GAIN1
    {TVFE_OGO_GAIN2                , 0x00000000,} ,// TVFE_OGO_GAIN2
    {TVFE_OGO_OFFSET2              , 0x00000000,} ,// TVFE_OGO_OFFSET2
    {TVFE_OGO_OFFSET3              , 0x00000000,} ,// TVFE_OGO_OFFSET3
    {TVFE_VAFE_CTRL                , 0x00000201,} ,// TVFE_VAFE_CTRL //zhuang
    {TVFE_VAFE_STATUS              , 0x00000000,} ,// TVFE_VAFE_STATUS
    {TVFE_TOP_CTRL                 , 0x00008750,} ,// TVFE_TOP_CTRL
    {TVFE_CLAMP_INTF               , 0x00000000,} ,// TVFE_CLAMP_INTF
    {TVFE_RST_CTRL                 , 0x00000000,} ,// TVFE_RST_CTRL
    {TVFE_EXT_VIDEO_AFE_CTRL_MUX1  , 0x00000000,} ,// TVFE_EXT_VIDEO_AFE_CTRL_MUX1
    {TVFE_AAFILTER_CTRL1           , 0x00082222,} ,// TVFE_AAFILTER_CTRL1
    {TVFE_AAFILTER_CTRL2           , 0x252b39c6,} ,// TVFE_AAFILTER_CTRL2
    {TVFE_EDID_CONFIG              , 0x01800050,} ,// TVFE_EDID_CONFIG
    {TVFE_EDID_RAM_ADDR            , 0x00000100,} ,// TVFE_EDID_RAM_ADDR
    {TVFE_EDID_RAM_WDATA           , 0x00000000,} ,// TVFE_EDID_RAM_WDATA
    {TVFE_EDID_RAM_RDATA           , 0x00000000,} ,// TVFE_EDID_RAM_RDATA
    {TVFE_APB_ERR_CTRL_MUX1        , 0x00000000,} ,// TVFE_APB_ERR_CTRL_MUX1
    {TVFE_APB_ERR_CTRL_MUX2        , 0x00000000,} ,// TVFE_APB_ERR_CTRL_MUX2
    {TVFE_APB_INDICATOR1           , 0x00000000,} ,// TVFE_APB_INDICATOR1
    {TVFE_APB_INDICATOR2           , 0x00000000,} ,// TVFE_APB_INDICATOR2
    {TVFE_ADC_READBACK_CTRL        , 0x00000000,} ,// TVFE_ADC_READBACK_CTRL
    {TVFE_ADC_READBACK_INDICATOR   , 0x00000000,} ,// TVFE_ADC_READBACK_INDICATOR
    {TVFE_INT_CLR                  , 0x00000000,} ,// TVFE_INT_CLR
    {TVFE_INT_MSKN                 , 0x00000000,} ,// TVFE_INT_MASKN
    {TVFE_INT_INDICATOR1           , 0x00000000,} ,// TVFE_INT_INDICATOR1
    {TVFE_INT_SET                  , 0x00000000,} ,// TVFE_INT_SET
    {TVFE_CHIP_VERSION             , 0x00000000,} ,// TVFE_CHIP_VERSION
    {0xFFFFFFFF                    , 0x00000000,}
};

/* TVAFE_SIG_FORMAT_576I_50        */
void tvafe_set_comp_default(enum tvin_sig_fmt_e fmt)
{
    unsigned int i = 0;

    /** write top register **/
    while (comp_top_reg_default[i][0] != 0xFFFFFFFF) {
        WRITE_APB_REG(comp_top_reg_default[i][0], comp_top_reg_default[i][1]);
        i++;
    }

    pr_info("tvafe_set_comp_default fmt=%d\n", (int)fmt);

    /** write 7740 register **/
    tvafe_adc_configure(fmt);

#if 0
    for (i=0 ; i < 112; i++)
    {
       //WRITE_APB_REG((TOP_BASE_ADD+i)<<2,(vafe_default[i]));
       pr_info("%x ", READ_APB_REG((ADC_BASE_ADD+i)<<2));
       if ((i%15 == 0)&&(i != 0))
       pr_info("\n");
    };

    pr_info("\n............below for top contrl.................\n");
    for (i=0 ; i <= 0x90; i++)
    {
       //WRITE_APB_REG((TOP_BASE_ADD+i)<<2,(vafe_default[i]));
       pr_info("%x ", READ_APB_REG((TOP_BASE_ADD+i)<<2));
       if ((i%15 == 0)&&(i != 0))
       pr_info("\n");
    };
 #endif
    pr_info("tvafe_set_comp_default END\n");
}

