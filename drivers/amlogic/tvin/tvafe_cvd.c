/*
 * TVAFE cvd2 device driver.
 *
 * Copyright (c) 2010 Frank zhao <frank.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/******************************Includes************************************/
#include <linux/kernel.h>
#include <linux/delay.h>
#include <mach/am_regs.h>
#include <asm/uaccess.h>
#include "tvin_global.h"
#include "tvafe.h"
#include "tvafe_general.h"
#include "tvafe_regs.h"
#include "tvafe_cvd.h"
#include "tvafe_adc.h"

/******************************Definitions************************************/
//digital gain value for AGC
#define  DAGC_GAIN_STANDARD                 0x200//0xC8
#define  DAGC_GAIN_RANGE                    0x10//0x64
#define  DAGC_GAIN_RANGE1                   0x78//0xC8
#define  DAGC_GAIN_RANGE2                   0xFF


#define	FC_LESS_NTSC443_TO_NTSCM_MAX        128
#define	FC_LESS_NTSC443_TO_NTSCM_MIN        96

#define	FC_MORE_NTSCM_TO_NTSC443_MAX        200
#define	FC_MORE_NTSCM_TO_NTSC443_MIN        130

#define	FC_LESS_PAL_I_TO_PAL_N_MAX          108
#define	FC_LESS_PAL_I_TO_PAL_N_MIN          52	//40

#define	FC_LESS_PAL_60_TO_PAL_M_MAX	        100
#define	FC_LESS_PAL_60_TO_PAL_M_MIN	        35

#define	FC_MORE_PAL_N_TO_PAL_I_MAX	        205
#define	FC_MORE_PAL_N_TO_PAL_I_MIN	        145

#define	FC_MORE_PAL_M_TO_PAL_60_MAX		    205
#define	FC_MORE_PAL_M_TO_PAL_60_MIN		    150

#define CORDIC_FILTER_COUNT                 20
#define PAL_I_TO_SECAM_CNT                  40  //205
#define NEW_FMT_CHECK_CNT                   25   //-->pal_i / ntsc are ok, no_color_burst is not stable
//#define NEW_FMT_CHECK_CNT                   40    //

#define NEW_FMT_CHANGE_CNT                   3

#define SECAM_STABLE_CNT                    211


/***************************Global Variables**********************************/
static struct tvafe_cvd2_sig_status_s    cvd2_sig_status = {
    .no_sig = 0,//                 :1;
    .h_lock = 0,//                 :1;
    .v_lock = 0,//                 :1;
    .h_nonstd = 0,//               :1;
    .v_nonstd = 0,//               :1;
    .no_color_burst = 0,//         :1;
    .comb3d_off = 0,//             :1;

    .hv_lock = 0,//                :1;
    .chroma_lock = 0,//            :1;
    .pal = 0,//                    :1;
    .secam = 0,//                  :1;
    .line625 = 0,//                :1;
    .fc_more = 0,//                :1;
    .fc_Less = 0,//                :1;
    .noisy = 0,//                  :1;
    .vcr = 0,//                    :1;
    .vcrtrick = 0,//               :1;
    .vcrff = 0,//                  :1;
    .vcrrew = 0,//                 :1;

    .cordic_data_min = 0xff,
    .cordic_data_max = 0,
    .stable_cnt = 0,
    .new_fmt_cnt = 0,
    .chroma_stable_cnt = 0,
    .cvd_fmt_chg_cnt = 0,

    .cur_sd_state = 0,
    .detected_sd_state = 0,
    .cordic = {
                    .ntsc443_to_ntscm_fc_less = {   FC_LESS_NTSC443_TO_NTSCM_MAX,
                                                    FC_LESS_NTSC443_TO_NTSCM_MIN,
                                                },
                    .ntscm_to_ntsc443_fc_more = {   FC_MORE_NTSCM_TO_NTSC443_MAX,
                                                    FC_MORE_NTSCM_TO_NTSC443_MIN,
                                                },
                    .pali_to_paln_fc_less =     {   FC_LESS_PAL_I_TO_PAL_N_MAX,
                                                    FC_LESS_PAL_I_TO_PAL_N_MIN,
                                                },
                    .paln_to_pali_fc_more =     {   FC_MORE_PAL_N_TO_PAL_I_MAX,
                                                    FC_MORE_PAL_N_TO_PAL_I_MIN,
                                                },
                    .pal60_to_palm_fc_less =    {   FC_LESS_PAL_60_TO_PAL_M_MAX,
                                                    FC_LESS_PAL_60_TO_PAL_M_MIN,
                                                },
                    .palm_to_pal60_fc_less =    {   FC_MORE_PAL_M_TO_PAL_60_MAX,
                                                    FC_MORE_PAL_M_TO_PAL_60_MIN,
                                                },
                },

    .agc = {
                .cnt = 0,
                .dgain = 0,
                .again = 0x30
            },

    .cordic_data_sum = 0,//unsigned   ;
    .cvd2_mem_addr = 0,
    .cvd2_mem_size = 0,
};

/*
static u32getCDTOstatus(void)
{
	u32 tmp_data, cdto_data = 0;

	tmp_data = READ_APB_REG(CVD2_CHROMA_DTO_INCREMENT_31_24);
	cdto_data = (tmp_data & 0xff) << 24;
	tmp_data = READ_APB_REG(CVD2_CHROMA_DTO_INCREMENT_23_16);
	cdto_data |= (tmp_data & 0xff)  << 16;
	tmp_data = READ_APB_REG(CVD2_CHROMA_DTO_INCREMENT_15_8);
	cdto_data |= (tmp_data & 0xff)  << 8;
	tmp_data = READ_APB_REG(CVD2_CHROMA_DTO_INCREMENT_7_0);
	cdto_data |= (tmp_data & 0xff) ;

    return cdto_data;
}
*/

static inline void programCDTO(unsigned data)
{
	unsigned tmp_data;

	tmp_data = (data & 0xff000000) >> 24;
	WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_31_24,	tmp_data);
	tmp_data = (data & 0xff0000) >> 16;
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_23_16,	tmp_data);
	tmp_data = (data & 0xff00) >> 8;
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_15_8,	tmp_data);
	tmp_data = data & 0xff;
    WRITE_APB_REG(CVD2_CHROMA_DTO_INCREMENT_7_0,	tmp_data);

    return;
}


static inline void programHDTO(unsigned data)
{
	unsigned tmp_data;

	tmp_data = (data & 0xff000000) >> 24;
	WRITE_APB_REG(CVD2_HSYNC_DTO_INCREMENT_31_24,	tmp_data);
	tmp_data = (data & 0xff0000) >> 16;
    WRITE_APB_REG(CVD2_HSYNC_DTO_INCREMENT_23_16,	tmp_data);
	tmp_data = (data & 0xff00) >> 8;
    WRITE_APB_REG(CVD2_HSYNC_DTO_INCREMENT_15_8,	tmp_data);
	tmp_data = data & 0xff;
    WRITE_APB_REG(CVD2_HSYNC_DTO_INCREMENT_7_0,	tmp_data);

    return;
}

/*
static void programHSDTO(unsigned data)
{
	unsigned tmp_data;

	tmp_data = (data & 0xff000000) >> 24;
	WRITE_APB_REG(CVD2_HSYNC_DTO_INCREMENT_31_24, tmp_data);
	tmp_data = (data & 0xff0000) >> 16;
    WRITE_APB_REG(CVD2_HSYNC_DTO_INCREMENT_23_16, tmp_data);
	tmp_data = (data & 0xff00) >> 8;
    WRITE_APB_REG(CVD2_HSYNC_DTO_INCREMENT_15_8,  tmp_data);
	tmp_data = data & 0xff;
    WRITE_APB_REG(CVD2_HSYNC_DTO_INCREMENT_7_0,   tmp_data);

    return;
}
*/

static void tvafe_cvd2_reset_cnt(void)
{
    cvd2_sig_status.cordic_data_sum = 0;
    cvd2_sig_status.stable_cnt = 0;
//    cvd2_sig_status.pali_to_secam_cnt = 0;
    cvd2_sig_status.cordic_data_min = 0xff;
    cvd2_sig_status.cordic_data_max = 0;
    cvd2_sig_status.new_fmt_cnt = 0;       //after set parameter for new format,
                //wait 5 field at least, then to check the status
    cvd2_sig_status.agc.dgain = 0;
    cvd2_sig_status.chroma_stable_cnt = 0;
    return;
}

static inline void cvd2_print_status(void)
{
#if 0
    pr_info("%s: h_lock:%d, v_lock:%d, chroma_lock:%d, h_nonstd:%d, v_nonstd:%d, no_color_burst:%d, pal:%d, secam:%d, line625:%d\n",
        __FUNCTION__,
        cvd2_sig_status.h_lock,
        cvd2_sig_status.v_lock,
        cvd2_sig_status.chroma_lock,
        cvd2_sig_status.h_nonstd,
        cvd2_sig_status.v_nonstd,
        cvd2_sig_status.no_color_burst,
        cvd2_sig_status.pal,
        cvd2_sig_status.secam,
        cvd2_sig_status.line625);
#else
//    pr_info("%s: status_1 = %x, status_2 = %x, status_3 = %x\n",__FUNCTION__, cvd2_sig_status.status_1, cvd2_sig_status.status_2, cvd2_sig_status.status_3);


#endif
}

static inline void ProgramPALi(void)
{
    unsigned buf;
    unsigned hagcVal;
    WRITE_APB_REG(CVD2_CONTROL0, 0x32); // colour_mode, vline_625, hpixel
    WRITE_APB_REG_BITS(CVD2_YC_SEPARATION_CONTROL, 0, 7, 1); // ntsc443_mode

    // read macrovision flag mv_vbi_detected
    buf = READ_APB_REG(CVD2_STATUS_REGISTER1);
    if(buf & 0x10)
       hagcVal = 165;
    else
       hagcVal = 220;

    WRITE_APB_REG(CVD2_LUMA_AGC_VALUE, hagcVal); // hagc--specify the luma AGC target value.
    WRITE_APB_REG(CVD2_NOISE_THRESHOLD,	0x32);  //ok--value sets the noise THRESHOLD, Default value =50
    WRITE_APB_REG(CVD2_OUTPUT_CONTROL,	0x20);  //ok swaps cb/cr,
    WRITE_APB_REG(CVD2_CHROMA_AGC,	0x8a);	//ok--set chroma  AGC target (default = 138)
    WRITE_APB_REG(CVD2_CHROMA_KILL,	0x00);

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HSTART, 132); //130);   //hactive_start--These bits control the active video line time interval. This specifies the beginning of
    //active line. This register is used to centre the horizontal position, and should not be used to crop the image to a smaller size.
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HWIDTH, 80); // hactive_width = data + 640

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART,	42); //ok-- vactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT,	193); //ok--vactive_height = 384 + value

    //	WRITE_APB_REG_BITS(0x07, 0xf, 0, 0x21); // yc_delay

    WRITE_APB_REG(CVD2_CHROMA_AGC, 138);   //0x75); // cagc: set the chroma AGC target

    //burst gate window-- Note that this window is set to be bigger than the burst. The automatic burst position tracker finds
    //the burst within this window.
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_START,	0x32);// ok chroma burst gate start--default value
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_END,	0x46);// ok  chroma burst gate  end

    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_END,0x5a);  	// cordic_gate_end :--start position for the secam burst gate for cordic frequency measurment
    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_START, 0x46);    // cordic_gate_start	,add by xintan

    /* WRITE_APB_REG_BITS(0xfd, 0xff, 0, 7168);    //These determine the threshold for the chroma kill to be active. If the calculated
    //chroma burst level is before the ckill value. Chroma kill will be active,add by xintan
    */
    WRITE_APB_REG(CVD2_CAGC_GATE_END, 80);  	// cagc_gate_end : for the burst position used by the chroma agc calculation,add by xintan
    WRITE_APB_REG(CVD2_CAGC_GATE_START, 50);    // cagc_gate_start--specified the beginning of the burst position used by the chroma agc calculation

    programCDTO(CVD2_CHROMA_DTO_PAL_I);
    programHDTO(CVD2_HSYNC_DTO_PAL_I);
    //the most-significant byte is stored in 0x18 and the least-significant byte is stored in 0x1b


    WRITE_APB_REG(CVD2_CKILL_LEVEL_15_8,	0x1c);// ok C kill ms --determine the threshold for the chroma kill to be active. If the calculated
    WRITE_APB_REG(CVD2_CKILL_LEVEL_7_0,	0x00);//chroma burst level is before the ckill value. Chroma kill will be active.


    WRITE_APB_REG_BITS(CVD2_REG_B2, 0, 6, 1);	// comb2only

    // reset MMU (toggle lbadrgen_rst)
    WRITE_APB_REG_BITS(CVD2_REG_B2, 1, 7, 1);// lbadrgen_rst
    WRITE_APB_REG_BITS(CVD2_REG_B2, 0, 7, 1);// lbadrgen_rst

}

static inline void ProgramPALM(void)
{
    unsigned buf;
    unsigned hagcVal;

    WRITE_APB_REG(CVD2_CONTROL0,	0x04);  //ok1
    WRITE_APB_REG(CVD2_CONTROL1,	0x00);  //ok1
    WRITE_APB_REG(CVD2_CONTROL2,	0x43);  //ok-- bit0 : enable set, enables the luma/composite AGC .
    //bit1 : enable set, enables the chroma AGC .
    //bit6--auto resuces the gain (set in register 4) by 25% when macro-vision encoded signals are detected.
    WRITE_APB_REG(CVD2_YC_SEPARATION_CONTROL,	0x02);  //ok

    // read macrovision flag mv_vbi_detected
    buf = READ_APB_REG(CVD2_STATUS_REGISTER1);
    if(buf & 0x10)
       hagcVal = 166;
    else
       hagcVal = 221;
    WRITE_APB_REG(CVD2_LUMA_AGC_VALUE, hagcVal); // hagc--specify the luma AGC target value.


    WRITE_APB_REG(CVD2_NOISE_THRESHOLD,	0x32);  //ok--value sets the noise THRESHOLD, Default value =50
    WRITE_APB_REG(CVD2_OUTPUT_CONTROL,	0x20);  //ok swaps cb/cr
    WRITE_APB_REG(CVD2_CHROMA_AGC,	0x8a);	//set chroma  AGC target (default = 138)
    WRITE_APB_REG(CVD2_CHROMA_KILL,	0x00);

    programCDTO(CVD2_CHROMA_DTO_PAL_M);
    programHDTO(CVD2_HSYNC_DTO_PAL_M);

    //burst gate window-- Note that this window is set to be bigger than the burst. The automatic burst position tracker finds
    //the burst within this window.
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_START,	0x32);// ok chroma burst gate start--default value
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_END,	0x46);// ok  chroma burst gate  end

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HSTART,	0x82); //ok-- hactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HWIDTH,	0x50); //ok--hactive_width = data + 640

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART,	0x22); //ok-- vactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT,	0x61); //ok--vactive_height = 384 + value

    WRITE_APB_REG(CVD2_REG_B0,	0x3a);
    WRITE_APB_REG(CVD2_3DCOMB_FILTER,	0xf3);
    WRITE_APB_REG(CVD2_REG_B2,	0x0c);// ok  adaptive C bandwide
    WRITE_APB_REG(CVD2_2DCOMB_ADAPTIVE_GAIN_CONTROL,	0x03);// ok  frame buffer
    WRITE_APB_REG(CVD2_MOTION_DETECTOR_NOISE_TH,	0x23);

    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_START,	0x46);// ok--start position for the secam burst gate for cordic frequency measurment
    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_END,	0x5a);// ok

    WRITE_APB_REG(CVD2_CAGC_GATE_START,	0x32);// ok  burst agc start--specified the beginning of the burst position used by the chroma agc calculation
    WRITE_APB_REG(CVD2_CAGC_GATE_END,	0x50);// ok  burst agc end

    WRITE_APB_REG(CVD2_CKILL_LEVEL_15_8,	0x1c);// ok C kill ms --determine the threshold for the chroma kill to be active. If the calculated
    WRITE_APB_REG(CVD2_CKILL_LEVEL_7_0,	0x00);//chroma burst level is before the ckill value. Chroma kill will be active.
}

static inline void ProgramPALCN(void)
{
    unsigned buf;
    unsigned hagcVal;

    WRITE_APB_REG(CVD2_CONTROL0,	0x36);  //ok
    WRITE_APB_REG(CVD2_CONTROL1,	0x01);  //ok --enable pedestal subtraction in order to luma -= some offset
    WRITE_APB_REG(CVD2_CONTROL2,	0x43);  //ok-- bit0 : enable set, enables the luma/composite AGC .
    //bit1 : enable set, enables the chroma AGC .
    //bit6--auto resuces the gain (set in register 4) by 25% when macro-vision encoded signals are detected.
    WRITE_APB_REG(CVD2_YC_SEPARATION_CONTROL,	0x02);  //ok

    // read macrovision flag mv_vbi_detected
    buf = READ_APB_REG(CVD2_STATUS_REGISTER1);
    if(buf & 0x10)
       hagcVal = 166;
    else
       hagcVal = 221;
    WRITE_APB_REG(CVD2_LUMA_AGC_VALUE, hagcVal);// hagc--specify the luma AGC target value.

    WRITE_APB_REG(CVD2_NOISE_THRESHOLD,	0x32);  //ok--value sets the noise THRESHOLD, Default value =50
    WRITE_APB_REG(CVD2_OUTPUT_CONTROL,	0x20);  //ok swaps cb/cr
    WRITE_APB_REG(CVD2_CHROMA_AGC,	0x8a);	//ok--set chroma  AGC target (default = 138)
    WRITE_APB_REG(CVD2_CHROMA_KILL,	0x00);

    programCDTO(CVD2_CHROMA_DTO_PAL_CN);
    programHDTO(CVD2_HSYNC_DTO_PAL_CN);

    //burst gate window-- Note that this window is set to be bigger than the burst. The automatic burst position tracker finds
    //the burst within this window.
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_START,	0x32);// ok chroma burst gate start--default value
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_END,	0x46);// ok  chroma burst gate  end


    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HSTART,	0x8a); //ok-- hactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HWIDTH,	0x50); //ok--hactive_width = data + 640

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART,	0x2a); //ok-- vactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT,	0xc1); //ok--vactive_height = 384 + value

    WRITE_APB_REG(CVD2_REG_B2,	0x0c);// ok  adaptive C bandwide
    WRITE_APB_REG(CVD2_2DCOMB_ADAPTIVE_GAIN_CONTROL,	0x03);// ok  frame buffer
    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_START,	0x46);// ok--start position for the secam burst gate for cordic frequency measurment
    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_END,	0x5a);// ok
    WRITE_APB_REG(CVD2_CAGC_GATE_START,	0x32);// ok  burst agc start--specified the beginning of the burst position used by the chroma agc calculation
    WRITE_APB_REG(CVD2_CAGC_GATE_END,	0x50);// ok  burst agc end
    WRITE_APB_REG(CVD2_CKILL_LEVEL_15_8,	0x1c);// ok C kill ms --determine the threshold for the chroma kill to be active. If the calculated
    WRITE_APB_REG(CVD2_CKILL_LEVEL_7_0,	0xdc);//chroma burst level is before the ckill value. Chroma kill will be active.

}


static inline void ProgramPAL60(void)
{
    unsigned buf;
    unsigned hagcVal;

    WRITE_APB_REG(CVD2_CONTROL0,	0x04);
    WRITE_APB_REG(CVD2_CONTROL1,	0x00);//pedestal
    WRITE_APB_REG(CVD2_CONTROL2,	0x43);  //ok-- bit0 : enable set, enables the luma/composite AGC .
    //bit1 : enable set, enables the chroma AGC .
    //bit6--auto resuces the gain (set in register 4) by 25% when macro-vision encoded signals are detected.
    WRITE_APB_REG(CVD2_YC_SEPARATION_CONTROL,	0x02);//adaptive 3d

    // read macrovision flag mv_vbi_detected
    buf = READ_APB_REG(CVD2_STATUS_REGISTER1);
    if(buf & 0x10)
       hagcVal = 165;
    else
       hagcVal = 220;
    WRITE_APB_REG(CVD2_LUMA_AGC_VALUE, hagcVal); // hagc--specify the luma AGC target value.

    WRITE_APB_REG(CVD2_NOISE_THRESHOLD,	0x0a);	//ok--value sets the noise THRESHOLD, Default value =50
    WRITE_APB_REG(CVD2_OUTPUT_CONTROL,	0x20);//YC DELAY for PAL
    WRITE_APB_REG(CVD2_CHROMA_AGC,	0x67);//-set chroma  AGC target (default = 138)
    WRITE_APB_REG(CVD2_CHROMA_KILL,	0x01);//Set  video input to Pal60

    programCDTO(CVD2_CHROMA_DTO_PAL_60);
    programHDTO(CVD2_HSYNC_DTO_PAL_60);


    //burst gate window-- Note that this window is set to be bigger than the burst. The automatic burst position tracker finds
    //the burst within this window.
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_START,	0x32);// ok chroma burst gate start--default value
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_END,	0x46);// ok  chroma burst gate  end

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HSTART,	0x84); //ok-- hactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HWIDTH,	0x50); //ok--hactive_width = data + 640

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART,	0x2a); //ok-- vactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT,	0x61); //ok--vactive_height = 384 + value

    WRITE_APB_REG(CVD2_REG_B2,	0x0c);//adaptive C bandwide
    WRITE_APB_REG(CVD2_2DCOMB_ADAPTIVE_GAIN_CONTROL,	0x0b);//frame buffer
    WRITE_APB_REG(CVD2_CAGC_GATE_START,	0x32);// ok  burst agc start--specified the beginning of the burst position used by the chroma agc calculation
    WRITE_APB_REG(CVD2_CAGC_GATE_END,	0x50);//bust agc end

    WRITE_APB_REG(CVD2_CKILL_LEVEL_15_8,	0x05);// ok C kill ms --determine the threshold for the chroma kill to be active. If the calculated
    WRITE_APB_REG(CVD2_CKILL_LEVEL_7_0,	0x00);//chroma burst level is before the ckill value. Chroma kill will be active.
}

static inline void ProgramNTSC(void)
{

    unsigned buf;
    unsigned hagcVal;
    WRITE_APB_REG(CVD2_CONTROL0,	0x00);		// colour_mode--NTSC, vline_525, video color standard

    WRITE_APB_REG_BITS(CVD2_CONTROL1, 1, 0, 1);		// ped -- why?

    WRITE_APB_REG_BITS(CVD2_YC_SEPARATION_CONTROL, 0, 7, 1);		// ntsc443_mode

    WRITE_APB_REG(CVD2_NOISE_THRESHOLD,	0x32);  //ok--value sets the noise THRESHOLD, Default value =50
    WRITE_APB_REG(CVD2_OUTPUT_CONTROL,	0x60);  //ok swaps cb/cr, auto blue screen mode.

    WRITE_APB_REG(CVD2_CHROMA_AGC,	0x8a);	//ok--set chroma  AGC target (default = 138)
    WRITE_APB_REG_BITS(CVD2_CHROMA_KILL, 0, 0, 1);		// Pal60_mode   ,add by xintan

    // read macrovision flag mv_vbi_detected
    buf = READ_APB_REG(CVD2_STATUS_REGISTER1);  //if a MacroVision signal is detected and ¡°mv_hagc_mode¡± (02.6h) is set,
    if(buf & 0x10)
       hagcVal = 166;
    else
       hagcVal = 221;

    //then this value is automatically reduced by 25%.--(221 * 0.75% = 166)
    WRITE_APB_REG(CVD2_LUMA_AGC_VALUE, hagcVal);	//specify the luma AGC target value.
    WRITE_APB_REG(CVD2_CHROMA_KILL,	0x00);

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HSTART, 130);   //132);		// hactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HWIDTH, 80); // hactive_width = data + 640,add by xintan

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART, 34);		// vactive_start  (unit is half lines)
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT, 97); // vactive_height -- 97=> 384+97=481 half lines)

    //burst gate window-- Note that this window is set to be bigger than the burst. The automatic burst position tracker finds
    //the burst within this window.
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_START,	0x32);// ok chroma burst gate start--default value
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_END,	0x46);// ok  chroma burst gate  end


    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_END, 0x5a);  	// cordic_gate_end : --start position for the secam burst gate for cordic frequency measurment
    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_START, 0x46);    // cordic_gate_start	,add by xintan

    //  WRITE_APB_REG_BITS(0xfd, 0xff, 0, 7168);    //These determine the threshold for the chroma kill to be active. If the calculated
    //chroma burst level is before the ckill value. Chroma kill will be active,add by xintan

    WRITE_APB_REG(CVD2_CAGC_GATE_END, 80);  	// cagc_gate_end : for the burst position used by the chroma agc calculation,add by xintan
    WRITE_APB_REG(CVD2_CAGC_GATE_START, 50);    // cagc_gate_start,--specified the beginning of the burst position used by the chroma agc calculation

    programCDTO(CVD2_CHROMA_DTO_NTSC_M);
    programHDTO(CVD2_HSYNC_DTO_NTSC_M);

    //the most-significant byte is stored in 0x18 and the least-significant byte is stored in 0x1b

    WRITE_APB_REG(CVD2_CKILL_LEVEL_15_8,	0x1c);// ok C kill ms --determine the threshold for the chroma kill to be active. If the calculated
    WRITE_APB_REG(CVD2_CKILL_LEVEL_7_0,	0x00);//chroma burst level is before the ckill value. Chroma kill will be active.

    WRITE_APB_REG_BITS(CVD2_REG_B2, 0, 6, 1);		// comb2only, This bit will enable CVD2 to work only with2D YC separation mode

    // reset MMU (toggle lbadrgen_rst)
    WRITE_APB_REG_BITS(CVD2_REG_B2, 1, 7, 1);		// lbadrgen_rst: This bit is use to reset the Line/Frame buffer address generation. This also reset the
    //               address pointer of the FIFO type FRAME buffer. Each time if the video standard is
    //               changed (e.g from PAL to NTSC). The FRAME buffer has to set up the read/write
    //               pointer properly. This bit can be used to force the set up to occur even though the set
    //               up should be done by the CVD2 automatically.

    WRITE_APB_REG_BITS(CVD2_REG_B2, 0, 7, 1);		// lbadrgen_rst

}

static inline void ProgramNTSC443(void)
{
    unsigned buf;
    unsigned hagcVal;

    WRITE_APB_REG(CVD2_CONTROL0,	0x00);  //ok
    WRITE_APB_REG(CVD2_CONTROL1,	0x01);  //ok--enable pedestal subtraction in order to luma -= some offset
    WRITE_APB_REG(CVD2_CONTROL2,	0x43);  //ok-- bit0 : enable set, enables the luma/composite AGC .
    //bit1 : enable set, enables the chroma AGC .
    //bit6--auto resuces the gain (set in register 4) by 25% when macro-vision encoded signals are detected.
    WRITE_APB_REG(CVD2_YC_SEPARATION_CONTROL,	0x82);  //ok
    // read macrovision flag mv_vbi_detected
    buf = READ_APB_REG(CVD2_STATUS_REGISTER1);  //if a MacroVision signal is detected and ¡°mv_hagc_mode¡± (02.6h) is set,
    if(buf & 0x10)
       hagcVal = 166;
    else
       hagcVal = 221;
    WRITE_APB_REG(CVD2_LUMA_AGC_VALUE, hagcVal); // hagc--specify the luma AGC target value.

    WRITE_APB_REG(CVD2_NOISE_THRESHOLD,	0x32);  //ok--value sets the noise THRESHOLD, Default value =50
    WRITE_APB_REG(CVD2_OUTPUT_CONTROL,	0x60);  //ok swaps cb/cr, auto blue screen mode.
    WRITE_APB_REG(CVD2_CHROMA_AGC,	0x8a);	//ok--set chroma  AGC target (default = 138)
    WRITE_APB_REG(CVD2_CHROMA_KILL,	0x00);

    programCDTO(CVD2_CHROMA_DTO_NTSC_443);
    programHDTO(CVD2_HSYNC_DTO_NTSC_443);

    //burst gate window-- Note that this window is set to be bigger than the burst. The automatic burst position tracker finds
    //the burst within this window.
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_START,	0x32);// ok chroma burst gate start--default value
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_END,	0x46);// ok  chroma burst gate  end


    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HSTART,	0x82); //ok-- hactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HWIDTH,	0x50); //ok--hactive_width = data + 640

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART,	0x22); //ok-- vactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT,	0x61); //ok--vactive_height = 384 + value

    WRITE_APB_REG(CVD2_REG_B2,	0x0c);// ok  adaptive C bandwide
    WRITE_APB_REG(CVD2_2DCOMB_ADAPTIVE_GAIN_CONTROL,	0x03);// ok  frame buffer
    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_START,	0x46);// ok--start position for the secam burst gate for cordic frequency measurment
    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_END,	0x5a);// ok
    WRITE_APB_REG(CVD2_CAGC_GATE_START,	0x32);// ok  burst agc start--specified the beginning of the burst position used by the chroma agc calculation
    WRITE_APB_REG(CVD2_CAGC_GATE_END,	0x50);// ok  burst agc end

    WRITE_APB_REG(CVD2_CKILL_LEVEL_15_8,	0x1c);// ok C kill ms --determine the threshold for the chroma kill to be active. If the calculated
    WRITE_APB_REG(CVD2_CKILL_LEVEL_7_0,	0x00);//chroma burst level is before the ckill value. Chroma kill will be active.

}

static inline void ProgramSECAM(void)
{
    unsigned buf, hagcVal;


    WRITE_APB_REG(CVD2_CONTROL0,	0x38);  //ok
    WRITE_APB_REG(CVD2_CONTROL1,	0x00);  //ok
    WRITE_APB_REG(CVD2_CONTROL2,	0x43);  //ok-- bit0 : enable set, enables the luma/composite AGC .
    //bit1 : enable set, enables the chroma AGC .
    //bit6--auto resuces the gain (set in register 4) by 25% when macro-vision encoded signals are detected.
    WRITE_APB_REG(CVD2_YC_SEPARATION_CONTROL,	0x00);  //ok--fully adaptive comb (2-D adaptive comb)

    // read macrovision flag mv_vbi_detected
    buf = READ_APB_REG(CVD2_STATUS_REGISTER1);  //if a MacroVision signal is detected and ¡°mv_hagc_mode¡± (02.6h) is set,
    if(buf & 0x10)
       hagcVal = 165;
    else
       hagcVal = 220;
    WRITE_APB_REG(CVD2_LUMA_AGC_VALUE, hagcVal); // hagc--specify the luma AGC target value.

    WRITE_APB_REG(CVD2_NOISE_THRESHOLD,	0x32);  //ok--value sets the noise THRESHOLD, Default value =50

    WRITE_APB_REG(CVD2_OUTPUT_CONTROL,	0x20);  //ok swaps cb/cr
    WRITE_APB_REG(CVD2_CHROMA_AGC,	0xc8);	//ok--set chroma  AGC target (default = 138)
    WRITE_APB_REG(CVD2_CHROMA_KILL,	0x00);

    programCDTO(CVD2_CHROMA_DTO_SECAM);
    programHDTO(CVD2_HSYNC_DTO_SECAM);

    //burst gate window-- Note that this window is set to be bigger than the burst. The automatic burst position tracker finds
    //the burst within this window.
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_START,	0x5a);// ok chroma burst gate start
    WRITE_APB_REG(CVD2_CHROMA_BURST_GATE_END,	0x6e);// ok  chroma burst gate  end

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HSTART,	0x76); //ok-- hactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_HWIDTH,	0x50); //ok--hactive_width = data + 640

    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VSTART,	0x29); //ok-- vactive_start
    WRITE_APB_REG(CVD2_ACTIVE_VIDEO_VHEIGHT,	0xbf); //ok--vactive_height = 384 + value

    WRITE_APB_REG(CVD2_REG_B2,	0x0c);// ok  adaptive C bandwide
    WRITE_APB_REG(CVD2_2DCOMB_ADAPTIVE_GAIN_CONTROL,	0x03);// ok  frame buffer

    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_START,	0x3c);// ok--start position for the secam burst gate for cordic frequency measurment
    WRITE_APB_REG(CVD2_CORDIC_FREQUENCY_GATE_END,	0x6e);// ok

    WRITE_APB_REG(CVD2_CAGC_GATE_START,	0x50);// ok  burst agc start--specified the beginning of the burst position used by the chroma agc calculation
    WRITE_APB_REG(CVD2_CAGC_GATE_END,	0x6e);// ok  burst agc end

    WRITE_APB_REG(CVD2_CKILL_LEVEL_15_8,	0x05);// ok C kill ms --determine the threshold for the chroma kill to be active. If the calculated
    WRITE_APB_REG(CVD2_CKILL_LEVEL_7_0,	0xdc);//chroma burst level is before the ckill value. Chroma kill will be active.

}


void init_cvd2_reg_module(enum tvafe_cvd2_sd_state_e cvd2_status )
{
	//unsigned temp_data;

    WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);

    //disable 3D
    WRITE_APB_REG_BITS(CVD2_REG_B2, 1, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);

	WRITE_APB_REG(CVD2_REG_CD, 0x0c);


    switch(cvd2_status)
    {
        case SD_SECAM:
            ProgramSECAM();
            break;

        case SD_NTSC:
            ProgramNTSC();
            break;

        case SD_NTSC_443:
            ProgramNTSC443();
            break;

        case SD_PAL_60:
            ProgramPAL60();
            break;

        case SD_PAL_CN:
            ProgramPALCN();
            break;

        case SD_PAL_M:
            ProgramPALM();
            break;

        case SD_PAL_I:
        default:
            ProgramPALi();
            break;
    }

	// set threshold to determine if this is PAL input, in detect PAL_M /PAL_CN / PAL_60, sometime, the signal is stable,
	//but PAL detected flag is lost in CVD register 3Ch [0]( STATUS PAL DETECTED)
	WRITE_APB_REG(CVD2_PAL_DETECTION_THRESHOLD, 0x1f);
	WRITE_APB_REG(CVD2_NOISE_THRESHOLD, 0xFF);

	WRITE_APB_REG(CVD2_SECAM_FREQ_OFFSET_RANGE, 0x50);  //adjut SECAM bit threshould value
    WRITE_APB_REG(CVD2_REG_06, 0x80);
    WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);

    tvafe_cvd2_reset_cnt();

    return;
}
/* TOP */
const static unsigned int cvbs_top_reg_default[][2] = {
    {TVFE_DVSS_MUXCTRL                      ,0x07000008/*0x00000000*/,}, // TVFE_DVSS_MUXCTRL
    {TVFE_DVSS_MUXVS_REF                    ,0x00000000,}, // TVFE_DVSS_MUXVS_REF
    {TVFE_DVSS_MUXCOAST_V                   ,0x00000000,}, // TVFE_DVSS_MUXCOAST_V
    {TVFE_DVSS_SEP_HVWIDTH                  ,0x00000000,}, // TVFE_DVSS_SEP_HVWIDTH
    {TVFE_DVSS_SEP_HPARA                    ,0x00000000,}, // TVFE_DVSS_SEP_HPARA
    {TVFE_DVSS_SEP_VINTEG                   ,0x00000000,}, // TVFE_DVSS_SEP_VINTEG
    {TVFE_DVSS_SEP_H_THR                    ,0x00000000,}, // TVFE_DVSS_SEP_H_THR
    {TVFE_DVSS_SEP_CTRL                     ,0x00000000,}, // TVFE_DVSS_SEP_CTRL
    {TVFE_DVSS_GEN_WIDTH                    ,0x00000000,}, // TVFE_DVSS_GEN_WIDTH
    {TVFE_DVSS_GEN_PRD                      ,0x00000000,}, // TVFE_DVSS_GEN_PRD
    {TVFE_DVSS_GEN_COAST                    ,0x00000000,}, // TVFE_DVSS_GEN_COAST
    {TVFE_DVSS_NOSIG_PARA                   ,0x00000000,}, // TVFE_DVSS_NOSIG_PARA
    {TVFE_DVSS_NOSIG_PLS_TH                 ,0x00000000,}, // TVFE_DVSS_NOSIG_PLS_TH
    {TVFE_DVSS_GATE_H                       ,0x00000000,}, // TVFE_DVSS_GATE_H
    {TVFE_DVSS_GATE_V                       ,0x00000000,}, // TVFE_DVSS_GATE_V
    {TVFE_DVSS_INDICATOR1                   ,0x00000000,}, // TVFE_DVSS_INDICATOR1
    {TVFE_DVSS_INDICATOR2                   ,0x00000000,}, // TVFE_DVSS_INDICATOR2
    {TVFE_DVSS_MVDET_CTRL1                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL1
    {TVFE_DVSS_MVDET_CTRL2                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL2
    {TVFE_DVSS_MVDET_CTRL3                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL3
    {TVFE_DVSS_MVDET_CTRL4                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL4
    {TVFE_DVSS_MVDET_CTRL5                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL5
    {TVFE_DVSS_MVDET_CTRL6                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL6
    {TVFE_DVSS_MVDET_CTRL7                  ,0x00000000,}, // TVFE_DVSS_MVDET_CTRL7
    {TVFE_SYNCTOP_SPOL_MUXCTRL              ,0x00000009,}, // TVFE_SYNCTOP_SPOL_MUXCTRL
    {TVFE_SYNCTOP_INDICATOR1_HCNT           ,0x00000000,}, // TVFE_SYNCTOP_INDICATOR1_HCNT
    {TVFE_SYNCTOP_INDICATOR2_VCNT           ,0x00000000,}, // TVFE_SYNCTOP_INDICATOR2_VCNT
    {TVFE_SYNCTOP_INDICATOR3                ,0x00000000,}, // TVFE_SYNCTOP_INDICATOR3
    {TVFE_SYNCTOP_SFG_MUXCTRL1              ,0x00000000,}, // TVFE_SYNCTOP_SFG_MUXCTRL1
    {TVFE_SYNCTOP_SFG_MUXCTRL2              ,0x00330000,}, // TVFE_SYNCTOP_SFG_MUXCTRL2
    {TVFE_SYNCTOP_INDICATOR4                ,0x00000000,}, // TVFE_SYNCTOP_INDICATOR4
    {TVFE_SYNCTOP_SAM_MUXCTRL               ,0x00082001,}, // TVFE_SYNCTOP_SAM_MUXCTRL
    {TVFE_MISC_WSS1_MUXCTRL1                ,0x00000000,}, // TVFE_MISC_WSS1_MUXCTRL1
    {TVFE_MISC_WSS1_MUXCTRL2                ,0x00000000,}, // TVFE_MISC_WSS1_MUXCTRL2
    {TVFE_MISC_WSS2_MUXCTRL1                ,0x00000000,}, // TVFE_MISC_WSS2_MUXCTRL1
    {TVFE_MISC_WSS2_MUXCTRL2                ,0x00000000,}, // TVFE_MISC_WSS2_MUXCTRL2
    {TVFE_MISC_WSS1_INDICATOR1              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR1
    {TVFE_MISC_WSS1_INDICATOR2              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR2
    {TVFE_MISC_WSS1_INDICATOR3              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR3
    {TVFE_MISC_WSS1_INDICATOR4              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR4
    {TVFE_MISC_WSS1_INDICATOR5              ,0x00000000,}, // TVFE_MISC_WSS1_INDICATOR5
    {TVFE_MISC_WSS2_INDICATOR1              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR1
    {TVFE_MISC_WSS2_INDICATOR2              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR2
    {TVFE_MISC_WSS2_INDICATOR3              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR3
    {TVFE_MISC_WSS2_INDICATOR4              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR4
    {TVFE_MISC_WSS2_INDICATOR5              ,0x00000000,}, // TVFE_MISC_WSS2_INDICATOR5
    {TVFE_AP_MUXCTRL1                       ,0x00000000,}, // TVFE_AP_MUXCTRL1
    {TVFE_AP_MUXCTRL2                       ,0x00000000,}, // TVFE_AP_MUXCTRL2
    {TVFE_AP_MUXCTRL3                       ,0x00000000,}, // TVFE_AP_MUXCTRL3
    {TVFE_AP_MUXCTRL4                       ,0x00000000,}, // TVFE_AP_MUXCTRL4
    {TVFE_AP_MUXCTRL5                       ,0x00000000,}, // TVFE_AP_MUXCTRL5
    {TVFE_AP_INDICATOR1                     ,0x00000000,}, // TVFE_AP_INDICATOR1
    {TVFE_AP_INDICATOR2                     ,0x00000000,}, // TVFE_AP_INDICATOR2
    {TVFE_AP_INDICATOR3                     ,0x00000000,}, // TVFE_AP_INDICATOR3
    {TVFE_AP_INDICATOR4                     ,0x00000000,}, // TVFE_AP_INDICATOR4
    {TVFE_AP_INDICATOR5                     ,0x00000000,}, // TVFE_AP_INDICATOR5
    {TVFE_AP_INDICATOR6                     ,0x00000000,}, // TVFE_AP_INDICATOR6
    {TVFE_AP_INDICATOR7                     ,0x00000000,}, // TVFE_AP_INDICATOR7
    {TVFE_AP_INDICATOR8                     ,0x00000000,}, // TVFE_AP_INDICATOR8
    {TVFE_AP_INDICATOR9                     ,0x00000000,}, // TVFE_AP_INDICATOR9
    {TVFE_AP_INDICATOR10                    ,0x00000000,}, // TVFE_AP_INDICATOR10
    {TVFE_AP_INDICATOR11                    ,0x00000000,}, // TVFE_AP_INDICATOR11
    {TVFE_AP_INDICATOR12                    ,0x00000000,}, // TVFE_AP_INDICATOR12
    {TVFE_AP_INDICATOR13                    ,0x00000000,}, // TVFE_AP_INDICATOR13
    {TVFE_AP_INDICATOR14                    ,0x00000000,}, // TVFE_AP_INDICATOR14
    {TVFE_AP_INDICATOR15                    ,0x00000000,}, // TVFE_AP_INDICATOR15
    {TVFE_AP_INDICATOR16                    ,0x00000000,}, // TVFE_AP_INDICATOR16
    {TVFE_AP_INDICATOR17                    ,0x00000000,}, // TVFE_AP_INDICATOR17
    {TVFE_AP_INDICATOR18                    ,0x00000000,}, // TVFE_AP_INDICATOR18
    {TVFE_AP_INDICATOR19                    ,0x00000000,}, // TVFE_AP_INDICATOR19
    {TVFE_BD_MUXCTRL1                       ,0x00000000,}, // TVFE_BD_MUXCTRL1
    {TVFE_BD_MUXCTRL2                       ,0x00000000,}, // TVFE_BD_MUXCTRL2
    {TVFE_BD_MUXCTRL3                       ,0x00000000,}, // TVFE_BD_MUXCTRL3
    {TVFE_BD_MUXCTRL4                       ,0x00000000,}, // TVFE_BD_MUXCTRL4
    {TVFE_CLP_MUXCTRL1                      ,0x00000000,}, // TVFE_CLP_MUXCTRL1
    {TVFE_CLP_MUXCTRL2                      ,0x00000000,}, // TVFE_CLP_MUXCTRL2
    {TVFE_CLP_MUXCTRL3                      ,0x00000000,}, // TVFE_CLP_MUXCTRL3
    {TVFE_CLP_MUXCTRL4                      ,0x00000000,}, // TVFE_CLP_MUXCTRL4
    {TVFE_CLP_INDICATOR1                    ,0x00000000,}, // TVFE_CLP_INDICATOR1
    {TVFE_BPG_BACKP_H                       ,0x00000000,}, // TVFE_BPG_BACKP_H
    {TVFE_BPG_BACKP_V                       ,0x00000000,}, // TVFE_BPG_BACKP_V
    {TVFE_DEG_H                             ,0x00000000,}, // TVFE_DEG_H
    {TVFE_DEG_VODD                          ,0x00000000,}, // TVFE_DEG_VODD
    {TVFE_DEG_VEVEN                         ,0x00000000,}, // TVFE_DEG_VEVEN
    {TVFE_OGO_OFFSET1                       ,0x00000000,}, // TVFE_OGO_OFFSET1
    {TVFE_OGO_GAIN1                         ,0x00000000,}, // TVFE_OGO_GAIN1
    {TVFE_OGO_GAIN2                         ,0x00000000,}, // TVFE_OGO_GAIN2
    {TVFE_OGO_OFFSET2                       ,0x00000000,}, // TVFE_OGO_OFFSET2
    {TVFE_OGO_OFFSET3                       ,0x00000000,}, // TVFE_OGO_OFFSET3
    {TVFE_VAFE_CTRL                         ,0x00000000,}, // TVFE_VAFE_CTRL
    {TVFE_VAFE_STATUS                       ,0x00000000,}, // TVFE_VAFE_STATUS
    {TVFE_TOP_CTRL                          ,0x000C4B60/*0x00004B60*/,}, // TVFE_TOP_CTRL
    {TVFE_CLAMP_INTF                        ,0x00008666,}, // TVFE_CLAMP_INTF
    {TVFE_RST_CTRL                          ,0x00000000,}, // TVFE_RST_CTRL
    {TVFE_EXT_VIDEO_AFE_CTRL_MUX1           ,0x00000000,}, // TVFE_EXT_VIDEO_AFE_CTRL_MUX1
    {TVFE_AAFILTER_CTRL1                    ,0x00082222,}, // TVFE_AAFILTER_CTRL1
    {TVFE_AAFILTER_CTRL2                    ,0x252b39c6,}, // TVFE_AAFILTER_CTRL2
    {TVFE_EDID_CONFIG                       ,0x00000000,}, // TVFE_EDID_CONFIG
    {TVFE_EDID_RAM_ADDR                     ,0x00000000,}, // TVFE_EDID_RAM_ADDR
    {TVFE_EDID_RAM_WDATA                    ,0x00000000,}, // TVFE_EDID_RAM_WDATA
    {TVFE_EDID_RAM_RDATA                    ,0x00000000,}, // TVFE_EDID_RAM_RDATA
    {TVFE_APB_ERR_CTRL_MUX1                 ,0x00000000,}, // TVFE_APB_ERR_CTRL_MUX1
    {TVFE_APB_ERR_CTRL_MUX2                 ,0x00000000,}, // TVFE_APB_ERR_CTRL_MUX2
    {TVFE_APB_INDICATOR1                    ,0x00000000,}, // TVFE_APB_INDICATOR1
    {TVFE_APB_INDICATOR2                    ,0x00000000,}, // TVFE_APB_INDICATOR2
    {TVFE_ADC_READBACK_CTRL                 ,0x80140003,}, // TVFE_ADC_READBACK_CTRL
    {TVFE_ADC_READBACK_INDICATOR            ,0x00000000,}, // TVFE_ADC_READBACK_INDICATOR
    {TVFE_INT_CLR                           ,0x00000000,}, // TVFE_INT_CLR
    {TVFE_INT_MSKN                          ,0x00000000,}, // TVFE_INT_MASKN
    {TVFE_INT_INDICATOR1                    ,0x00000000,}, // TVFE_INT_INDICATOR1
    {TVFE_INT_SET                           ,0x00000000,}, // TVFE_INT_SET
    //{TVFE_CHIP_VERSION                      ,0x00000000,}, // TVFE_CHIP_VERSION
    {0xFFFFFFFF                             ,0x00000000,}
};

#define     DECODER_MOTION_BUFFER_ADDR_OFFSET       0x70000
#define     DECODER_MOTION_BUFFER_4F_LENGTH         0x15a60//0xe946//
#define     DECODER_VBI_ADDR_OFFSET                 0x86000
#define     DECODER_VBI_VBI_SIZE                    0x1000
#define     DECODER_VBI_START_ADDR                  0x0

const static unsigned int  cvd_mem_4f_length[TVIN_SIG_FMT_CVBS_SECAM-TVIN_SIG_FMT_CVBS_NTSC_M+1] =
{
    0xe946 , // TVIN_SIG_FMT_CVBS_NTSC_M,
    0xe946 , // TVIN_SIG_FMT_CVBS_NTSC_443,
    0x15a60, // TVIN_SIG_FMT_CVBS_PAL_I,
    0xe905, // TVIN_SIG_FMT_CVBS_PAL_M,
    0x15a60, // TVIN_SIG_FMT_CVBS_PAL_60,
    0x117d9, // TVIN_SIG_FMT_CVBS_PAL_CN,
    0x15a60, // TVIN_SIG_FMT_CVBS_SECAM,
};


static void tvafe_cvd2_memory_init(enum tvin_sig_fmt_e fmt)
{
    if ((fmt < TVIN_SIG_FMT_CVBS_NTSC_M) || (fmt > TVIN_SIG_FMT_CVBS_SECAM)) {
        pr_info("cvd2 fmt error, can not write memory \n");
        return;
    }

    WRITE_APB_REG(CVD2_REG_96, cvd2_sig_status.cvd2_mem_addr);
    WRITE_APB_REG(ACD_REG_30, (cvd2_sig_status.cvd2_mem_addr + DECODER_MOTION_BUFFER_ADDR_OFFSET));

    WRITE_APB_REG_BITS(ACD_REG_2A, cvd_mem_4f_length[fmt - TVIN_SIG_FMT_CVBS_NTSC_M],
        REG_4F_MOTION_LENGTH_BIT, REG_4F_MOTION_LENGTH_WID);
    //WRITE_APB_REG_BITS(ACD_REG_2A, DECODER_MOTION_BUFFER_4F_LENGTH,
    //    REG_4F_MOTION_LENGTH_BIT, REG_4F_MOTION_LENGTH_WID);
    WRITE_APB_REG(ACD_REG_2F, (cvd2_sig_status.cvd2_mem_addr + DECODER_VBI_ADDR_OFFSET));
    WRITE_APB_REG_BITS(ACD_REG_21, DECODER_VBI_VBI_SIZE,
        AML_VBI_SIZE_BIT, AML_VBI_SIZE_WID);
    WRITE_APB_REG_BITS(ACD_REG_21, DECODER_VBI_START_ADDR,
        AML_VBI_START_ADDR_BIT, AML_VBI_START_ADDR_WID);

}


static void tvafe_cvd2_luma_agc_adjust(void)
{
    //if a MacroVision signal is detected and ¡°mv_hagc_mode¡± (02.6h) is set,
    //then this value is automatically reduced by 25%.--(221 * 0.75% = 166)
	unsigned int val = (READ_APB_REG(CVD2_STATUS_REGISTER1)&0x10) ? 166 : 221;
	WRITE_APB_REG_BITS(CVD2_LUMA_AGC_VALUE, val, 0, 8);
}


// *****************************************************************************
// Function: get CVD2 signal status
//
//   Params: none
//
//   Return: success/error
//
// *****************************************************************************
static int tvafe_cvd2_get_signal_status(void)
{
    int ret = 0;
    cvd2_sig_status.chroma_stable_cnt++;

    cvd2_sig_status.status_1 = READ_APB_REG(CVD2_STATUS_REGISTER1);
    //pr_info("cvd CVD2_STATUS_REGISTER1:0x%x\n", data_0);
    //signal status
    cvd2_sig_status.no_sig = (cvd2_sig_status.status_1 & 0x01) >> NO_SIGNAL_BIT;  //
    //pr_info("cvd cvd2_sig_status.no_sig:0x%x\n", cvd2_sig_status.no_sig);
    //lock status
    cvd2_sig_status.h_lock = (cvd2_sig_status.status_1 & 0x02) >> HLOCK_BIT;
    cvd2_sig_status.v_lock = (cvd2_sig_status.status_1 & 0x04) >> VLOCK_BIT;
    cvd2_sig_status.chroma_lock = (cvd2_sig_status.status_1 & 0x08) >> CHROMALOCK_BIT;
    //pr_info("cvd sig:%d,hlock:%d,vlock:%d,chromalock:%d,\n",cvd2_sig_status.no_sig,
    //    cvd2_sig_status.h_lock, cvd2_sig_status.v_lock, cvd2_sig_status.chroma_lock);

    cvd2_sig_status.status_2 = READ_APB_REG(CVD2_STATUS_REGISTER2);
    //color burst status
    //H/V frequency non standard status
    cvd2_sig_status.h_nonstd = (cvd2_sig_status.status_2 & 0x02) >> HNON_STD_BIT;
    cvd2_sig_status.v_nonstd = (cvd2_sig_status.status_2 & 0x04) >> VNON_STD_BIT;
    cvd2_sig_status.no_color_burst = (cvd2_sig_status.status_2 & 0x08) >> BKNWT_DETECTED_BIT;
    cvd2_sig_status.comb3d_off = (cvd2_sig_status.status_2 & 0x10) >> STATUS_COMB3D_OFF_BIT;

    cvd2_sig_status.status_3 = READ_APB_REG(CVD2_STATUS_REGISTER3);
    //video standard status
    cvd2_sig_status.pal = (cvd2_sig_status.status_3 & 0x01) >> PAL_DETECTED_BIT;
    cvd2_sig_status.secam = (cvd2_sig_status.status_3 & 0x02) >> SECAM_DETECTED_BIT;
    cvd2_sig_status.line625 = (cvd2_sig_status.status_3 & 0x04) >> LINES625_DETECTED_BIT;
    cvd2_sig_status.noisy = (cvd2_sig_status.status_3 & 0x08) >> NOISY_BIT;

    //VCR status
    cvd2_sig_status.vcr = (cvd2_sig_status.status_3 & 0x10) >> VCR_BIT;
    cvd2_sig_status.vcrtrick = (cvd2_sig_status.status_3 & 0x20) >> VCR_TRICK_BIT;
    cvd2_sig_status.vcrff = (cvd2_sig_status.status_3 & 0x40) >> VCR_FF_BIT;
    cvd2_sig_status.vcrrew = (cvd2_sig_status.status_3 & 0x80) >> VCR_REW_BIT;

    return ret;
}

// *****************************************************************************
// Function: check CVD2 lock status by register's status
//
//   Params: none
//
//   Return: success/error
//
// *****************************************************************************
enum tvafe_cvbs_video_e  tvafe_cvd2_video_locked(void)
{
    enum tvafe_cvbs_video_e ret = TVAFE_CVBS_VIDEO_UNLOCKED;

    //VCR detection status
//    if (cvd2_sig_status.vcr == 0) {  // non-vcr
        //H/V locked
//        if((cvd2_sig_status.h_lock == 1) || (cvd2_sig_status.v_lock == 1)) {
//            ret = TVAFE_CVBS_VIDEO_LOCKED;
//            cvd2_sig_status.hv_lock = 1;
//         } else
//            cvd2_sig_status.hv_lock = 0;
//    } else {                            // vcr mode
        //H/V locked
        if((cvd2_sig_status.h_lock == 1) && (cvd2_sig_status.v_lock == 1)) {
            ret = TVAFE_CVBS_VIDEO_LOCKED;
            cvd2_sig_status.hv_lock = 1;
        }
        else
            cvd2_sig_status.hv_lock = 0;
//    }

    return ret;
}

// *****************************************************************************
// Function: CVD2 soft reset in video format searching
//
//   Params: none
//
//   Return: success/error
//
// *****************************************************************************
//static void  tvafe_cvd2_soft_reset(void)
//{
//    int temp_data = 0;
//
//    //reset
//	WRITE_APB_REG(CVD2_RESET_REGISTER,	0x00);//soft rest
//	WRITE_APB_REG(CVD2_RESET_REGISTER,	0x01);
//	temp_data = READ_APB_REG(CVD2_RESET_REGISTER);
//	WRITE_APB_REG(CVD2_RESET_REGISTER,	0x00);//soft reset
//
//    return ;
//}

// *****************************************************************************
// Function: check CVD2 VCR mode status by register's status
//
//   Params: none
//
//   Return: success/error
//
// *****************************************************************************
//static int  tvafe_cvd2_vcr_mode(void)
//{
//    int ret = 0;

//    if ((cvd2_sig_status.vcr_trick_mode == 1 || cvd2_sig_status.vcr_ff_mode  || cvd2_sig_status.vcr_rewind) && cvd2_sig_status.vcr == 1)
//        ret = 1;

//    return ret;
//}

// *****************************************************************************
// Function: check CVD2 H/V frequency non standard status by register's status
//
//   Params: none
//
//   Return: success/error
//
// *****************************************************************************
//static bool  tvafe_cvd2_non_standard_mode(void)
//{
//    bool ret = 0;
//
//    if (cvd2_sig_status.h_nonstd == 1 && cvd2_sig_status.v_nonstd == 1)
//        ret = 1;
//
//    return ret;
//}

// *****************************************************************************
// Function: configure video mode settings
//
//   Params: video format
//
//   Return: success/error
//
// *****************************************************************************
static bool tvafe_cvd2_write_mode_reg(enum tvin_sig_fmt_e fmt)
{
    unsigned int i = 0;
//    pr_info("%s \n", __FUNCTION__);
//    return (true);
    if ((fmt<TVIN_SIG_FMT_CVBS_NTSC_M) || (fmt>TVIN_SIG_FMT_CVBS_SECAM)) {
        pr_info("%s: fmt = %d \n",__FUNCTION__, fmt);
        return (false);
    }
	WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);

    //load ACD reg
    for (i=0; i<(ACD_REG_MAX+1); i++)
    {
        if ((i == 0x1E) || (i == 0x31))
            continue;
        WRITE_APB_REG((ACD_BASE_ADD+i)<<2, (cvbs_acd_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
    }

    // load CVD2 reg 0x00~3f (char)
    for (i=0; i<CVD_PART1_REG_NUM; i++)
    {
        WRITE_APB_REG((CVD_BASE_ADD+CVD_PART1_REG_MIN+i)<<2, (cvd_part1_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
    }

    // load CVD2 reg 0x70~ff (char)
    for (i=0; i<CVD_PART2_REG_NUM; i++)
    {
        WRITE_APB_REG((CVD_BASE_ADD+CVD_PART2_REG_MIN+i)<<2, (cvd_part2_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][i]));
    }

    // reload CVD2 reg 0x87, 0x93, 0x94, 0x95, 0x96, 0xe6, 0xfa (int)
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_0)<<2, cvd_part3_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][0]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_1)<<2, cvd_part3_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][1]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_2)<<2, cvd_part3_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][2]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_3)<<2, cvd_part3_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][3]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_4)<<2, cvd_part3_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][4]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_5)<<2, cvd_part3_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][5]);
    WRITE_APB_REG((CVD_BASE_ADD+CVD_PART3_REG_6)<<2, cvd_part3_table[fmt-TVIN_SIG_FMT_CVBS_NTSC_M][6]);

//    pr_info("tvafe:  cvd memory init\n");
    tvafe_cvd2_memory_init(fmt);
//    msleep(1);
//    pr_info("tvafe:  cvd reset\n");
//    tvafe_cvd2_reset_reg();

	WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);

    tvafe_cvd2_reset_cnt();
    // general functions
    //tvafe_cvd2_luma_agc_adjust();
    //load cvd2 memory address and size
    //mem pointer

    return (true);
}

// *****************************************************************************
// Function: CVD2 search the new video mode
//
//   Params: none
//
//   Return: mode detecion result
//
// *****************************************************************************
static void tvafe_cvd2_search_video_mode(void)
{
    unsigned data;
    tvafe_cvd2_get_signal_status();

    if(cvd2_sig_status.new_fmt_cnt < NEW_FMT_CHECK_CNT) //after set parameter for new format,
    {               //wait 5 field at least, then to check the status
        cvd2_sig_status.new_fmt_cnt++;
        cvd2_sig_status.detected_sd_state = cvd2_sig_status.cur_sd_state;
        return;
    }


//    if(cvd2_sig_status.cur_sd_state == SD_SECAM) {
//        if(cvd2_sig_status.new_fmt_cnt < PAL_I_TO_SECAM_CNT) {
//            cvd2_sig_status.new_fmt_cnt++;     // wait 2000 millisec for secam flag to be set
//            cvd2_sig_status.detected_sd_state = cvd2_sig_status.cur_sd_state;
//            return;
//        } else
//            cvd2_sig_status.new_fmt_cnt = SECAM_STABLE_CNT;
//    }
    data = READ_APB_REG_BITS(CVD2_CORDIC_FREQUENCY_STATUS,
                STATUS_CORDIQ_FRERQ_BIT, STATUS_CORDIQ_FRERQ_WID);

#if 1
	// If no siganl is off and hv lock is on, the detected standard depends on the current standard
	switch (cvd2_sig_status.cur_sd_state) {
		case SD_NO_SIG:
        case SD_UNLOCK:
            if (tvafe_cvd2_video_locked())
            {
				cvd2_sig_status.detected_sd_state = SD_HV_LOCK;
            }
            else
            {
                cvd2_sig_status.detected_sd_state = SD_UNLOCK;
            }
//            tvafe_cvd2_reset_cnt();
			break;
		case SD_HV_LOCK:
        case SD_NONSTD:
			if (cvd2_sig_status.hv_lock)
            {
				if (cvd2_sig_status.line625)
				{
					cvd2_sig_status.detected_sd_state = SD_PAL_I;
				}
				else
				{
					cvd2_sig_status.detected_sd_state = SD_PAL_M;
				}
			}
            else
            {
                cvd2_sig_status.detected_sd_state = SD_UNLOCK;
            }
            tvafe_cvd2_reset_cnt();
			break;
		case SD_PAL_I:
            if((cvd2_sig_status.line625) && (cvd2_sig_status.no_color_burst))
            {
                cvd2_sig_status.detected_sd_state = SD_PAL_I;
            }
			else if ((cvd2_sig_status.line625) && (cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal))
            {  // check current state (PAL i)
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if( cvd2_sig_status.cordic_data_min > data)
                    cvd2_sig_status.cordic_data_min = data;
                if( cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT - 1) {
                    //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum -= cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max;
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (cvd2_sig_status.stable_cnt - 2);
        			cvd2_sig_status.fc_Less = ((cvd2_sig_status.cordic_data_sum < cvd2_sig_status.cordic.pali_to_paln_fc_less.max) \
                                        && (cvd2_sig_status.cordic_data_sum > cvd2_sig_status.cordic.pali_to_paln_fc_less.min)) ? 1 : 0;
                    //pr_info("cvd cvd2_sig_status.cordic_data_sum:%d \n",cvd2_sig_status.cordic_data_sum);
                    if(cvd2_sig_status.fc_Less)
        			{
        				cvd2_sig_status.detected_sd_state = SD_PAL_CN;
        			}
        			else
        			{
        				cvd2_sig_status.detected_sd_state = SD_PAL_I;
                        cvd2_sig_status.cvd_fmt_chg_cnt = 0;
        			}
                    tvafe_cvd2_reset_cnt();
                }
			}
            else {
                cvd2_print_status();
                //PAL_ i''s chroma subcarries & PAL_60's one  are  approximate
                if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_60;
                }
                else
                {
					cvd2_sig_status.detected_sd_state = SD_SECAM;
                }
            }
			break;
		case SD_PAL_M:  //6
            if((!cvd2_sig_status.line625) && (cvd2_sig_status.no_color_burst))
            {
                cvd2_sig_status.detected_sd_state = SD_PAL_M;
            }
			else if ((!cvd2_sig_status.line625) && (cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal) ) {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if( cvd2_sig_status.cordic_data_min > data)
                     cvd2_sig_status.cordic_data_min = data;
                if( cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1) {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (cvd2_sig_status.stable_cnt - 2);
        		    cvd2_sig_status.fc_more = ((cvd2_sig_status.cordic_data_sum < cvd2_sig_status.cordic.palm_to_pal60_fc_less.max) \
                        && (cvd2_sig_status.cordic_data_sum > cvd2_sig_status.cordic.palm_to_pal60_fc_less.min)) ? 1 : 0;
        		    if(cvd2_sig_status.fc_more)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_60;
        		    }
        		    else
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_M;
                        cvd2_sig_status.cvd_fmt_chg_cnt = 0;
        		    }
                    tvafe_cvd2_reset_cnt();
                }
			}
            else {
                cvd2_print_status();
                if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_CN;    //PAL_CN''s chroma subcarries & PAL_M's one  are  approximate
                }
                else {
					if ((!cvd2_sig_status.line625) && (!cvd2_sig_status.pal))
					{
						cvd2_sig_status.detected_sd_state = SD_NTSC;  //NTSC_M''s chroma subcarries & PAL_M's one  are  approximate
                    }
                    else
                    {
						cvd2_sig_status.detected_sd_state = SD_HV_LOCK;
                    }
				}
            }
			break;
		case SD_PAL_CN:
            if((cvd2_sig_status.line625) && (cvd2_sig_status.no_color_burst))
            {
                cvd2_sig_status.detected_sd_state = SD_PAL_CN;
            }
			else if ((cvd2_sig_status.line625) &&( cvd2_sig_status.pal) && (cvd2_sig_status.chroma_lock)) {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if( cvd2_sig_status.cordic_data_min > data)
                     cvd2_sig_status.cordic_data_min = data;
                if( cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1) {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (cvd2_sig_status.stable_cnt - 2);
        		    cvd2_sig_status.fc_more = ((cvd2_sig_status.cordic_data_sum < cvd2_sig_status.cordic.paln_to_pali_fc_more.max) \
                                        && (cvd2_sig_status.cordic_data_sum > cvd2_sig_status.cordic.paln_to_pali_fc_more.min)) ? 1 : 0;
        		    if(cvd2_sig_status.fc_more)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_I;
        		    }
        		    else
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_CN;
                        cvd2_sig_status.cvd_fmt_chg_cnt = 0;
        		    }
                    tvafe_cvd2_reset_cnt();
                }
			}
            else {
                cvd2_print_status();
                if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_M;     //PAL_CN''s chroma subcarries & PAL_M's one  are  approximate
                }
                else
                {
				    cvd2_sig_status.detected_sd_state = SD_HV_LOCK;
                }
            }
			break;
        case SD_NTSC_443:
            if((!cvd2_sig_status.line625) && (!cvd2_sig_status.pal) && (cvd2_sig_status.no_color_burst))
            {
                cvd2_sig_status.detected_sd_state = SD_NTSC_443;
            }
            else if ((!cvd2_sig_status.line625) && (!cvd2_sig_status.pal) && (cvd2_sig_status.chroma_lock)) {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if( cvd2_sig_status.cordic_data_min > data)
                     cvd2_sig_status.cordic_data_min = data;
                if( cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1) {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (cvd2_sig_status.stable_cnt - 2);
        		    cvd2_sig_status.fc_Less = ((cvd2_sig_status.cordic_data_sum < cvd2_sig_status.cordic.ntsc443_to_ntscm_fc_less.max) \
                                        && (cvd2_sig_status.cordic_data_sum > cvd2_sig_status.cordic.ntsc443_to_ntscm_fc_less.min)) ? 1 : 0;
        		    if(cvd2_sig_status.fc_Less)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_NTSC;
        		    }
        		    else
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_NTSC_443;
                        cvd2_sig_status.cvd_fmt_chg_cnt = 0;
        		    }
                    tvafe_cvd2_reset_cnt();
                }
			}
            else {
                cvd2_print_status();
				if((!cvd2_sig_status.line625)&& (cvd2_sig_status.pal))
				{
				    cvd2_sig_status.detected_sd_state = SD_PAL_60;      //NTSC_433''s chroma subcarries & PAL_60's one  are  approximate
				}
                else
                {
				    cvd2_sig_status.detected_sd_state = SD_HV_LOCK;
                }
            }
			break;
		case SD_NTSC:  //8
            if((!cvd2_sig_status.line625) && (!cvd2_sig_status.pal) && (cvd2_sig_status.no_color_burst))
            {
                cvd2_sig_status.detected_sd_state = SD_NTSC;
            }
			else if ((!cvd2_sig_status.line625) && (!cvd2_sig_status.pal) && (cvd2_sig_status.chroma_lock)) {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if( cvd2_sig_status.cordic_data_min > data)
                     cvd2_sig_status.cordic_data_min = data;
                if( cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1) {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (cvd2_sig_status.stable_cnt - 2);
        		    cvd2_sig_status.fc_more = ((cvd2_sig_status.cordic_data_sum < cvd2_sig_status.cordic.ntscm_to_ntsc443_fc_more.max) \
                                        && (cvd2_sig_status.cordic_data_sum > cvd2_sig_status.cordic.ntscm_to_ntsc443_fc_more.min)) ? 1 : 0;
        		    if(cvd2_sig_status.fc_more)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_NTSC_443;
        		    }
        		    else
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_NTSC;
                        cvd2_sig_status.cvd_fmt_chg_cnt = 0;
        		    }
                    tvafe_cvd2_reset_cnt();
			    }
            }
            else {
                cvd2_print_status();
				if((!cvd2_sig_status.line625)&& (cvd2_sig_status.pal))
				{
				    cvd2_sig_status.detected_sd_state = SD_PAL_M;      //NTSC_M''s chroma subcarries & PAL_M's one  are  approximate
				}
                else if((!cvd2_sig_status.line625)&& (!cvd2_sig_status.pal))
				{
				    cvd2_sig_status.detected_sd_state = SD_NTSC_443;      //NTSC_M''s chroma subcarries & PAL_M's one  are  approximate
				}
                else
                {
				    cvd2_sig_status.detected_sd_state = SD_HV_LOCK;
                }
            }
			break;
		case SD_PAL_60:
            if((!cvd2_sig_status.line625) && (cvd2_sig_status.no_color_burst))
            {
                cvd2_sig_status.detected_sd_state = SD_PAL_60;
            }
			else if ((!cvd2_sig_status.line625) && (cvd2_sig_status.pal) && (cvd2_sig_status.chroma_lock)) {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if( cvd2_sig_status.cordic_data_min > data)
                     cvd2_sig_status.cordic_data_min = data;
                if( cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1) {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (CORDIC_FILTER_COUNT - 2);
        		    cvd2_sig_status.fc_Less = ((cvd2_sig_status.cordic_data_sum < cvd2_sig_status.cordic.pal60_to_palm_fc_less.max) \
                                        && (cvd2_sig_status.cordic_data_sum > cvd2_sig_status.cordic.pal60_to_palm_fc_less.min)) ? 1 : 0;
        		    if(cvd2_sig_status.fc_Less)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_M;
         		    }
        		    else
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_60;
                        cvd2_sig_status.cvd_fmt_chg_cnt = 0;
        		    }
                    tvafe_cvd2_reset_cnt();
                }
			}
            else {
                cvd2_print_status();
                if ((cvd2_sig_status.chroma_lock)&&(cvd2_sig_status.pal))
                    cvd2_sig_status.detected_sd_state = SD_PAL_I;	//PAL_ i''s chroma subcarries & PAL_60's one  are  approximate
                else {
					if((!cvd2_sig_status.line625)&& (!cvd2_sig_status.pal))
					{
					    cvd2_sig_status.detected_sd_state = SD_NTSC_443;      //NTSC_433''s chroma subcarries & PAL_60's one  are  approximate
					}
                    else
                    {
					    cvd2_sig_status.detected_sd_state = SD_HV_LOCK;
                    }
			    }
            }
			break;
		case SD_SECAM:
            if ((cvd2_sig_status.line625) && (cvd2_sig_status.secam) && \
                ((cvd2_sig_status.chroma_lock) || (cvd2_sig_status.no_color_burst)))
            {
				cvd2_sig_status.detected_sd_state = SD_SECAM;
                cvd2_sig_status.cvd_fmt_chg_cnt = 0;
            }
			else {
                cvd2_print_status();
                if((cvd2_sig_status.line625) && (!cvd2_sig_status.secam) && (cvd2_sig_status.pal))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_I;
                }
                else
                {
					cvd2_sig_status.detected_sd_state = SD_UNLOCK;

                }
            }
			break;
        case SD_VCR :
            break;
		default:
			break;
	}



#else
	// If no siganl is off and hv lock is on, the detected standard depends on the current standard
	switch (cvd2_sig_status.cur_sd_state) {
		case SD_NO_SIG:
        case SD_UNLOCK:
            if (tvafe_cvd2_video_locked())
				cvd2_sig_status.detected_sd_state = SD_HV_LOCK;
            pr_info("cvd nosig-->hvlock \n");
			break;
		case SD_HV_LOCK:
        case SD_NONSTD:
			if (cvd2_sig_status.hv_lock)
            {
				if (cvd2_sig_status.line625)
				{
					cvd2_sig_status.detected_sd_state = SD_PAL_I;
                    pr_info("cvd hvlock-->pali \n");
				}
				else
				{
					cvd2_sig_status.detected_sd_state = SD_PAL_M;
                    pr_info("cvd hvlock-->palm \n");
				}
			}
			break;
		case SD_PAL_I:
			if ((cvd2_sig_status.line625) && (cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal))
            {  // check current state (PAL i)
                cvd2_sig_status.cordic_data_sum += data;  //check cordic 0x7D
                cvd2_sig_status.stable_cnt++;

                if (cvd2_sig_status.cordic_data_min > data)
                    cvd2_sig_status.cordic_data_min = data;
                if (cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if (cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT - 1)
                { //32 time?
                    //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum -= cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max;
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (CORDIC_FILTER_COUNT - 2);
        			cvd2_sig_status.fc_Less = ((cvd2_sig_status.cordic_data_sum < FC_LESS_PAL_I_TO_PAL_N_MAX) &&
                                   (cvd2_sig_status.cordic_data_sum > FC_LESS_PAL_I_TO_PAL_N_MIN)) ? 1 : 0;
        			if(cvd2_sig_status.fc_Less)
        			{
        				cvd2_sig_status.detected_sd_state = SD_PAL_CN;
                        pr_info("cvd pali-->palcn \n");
        			}
        			else
        			{
                        //pr_info("switch to pali state\n");
        				cvd2_sig_status.detected_sd_state = SD_PAL_I;
        			}
                    tvafe_cvd2_reset_cnt();
                }
			}
            else
            {
                cvd2_sig_status.stable_cnt++;
                //PAL_ i''s chroma subcarries & PAL_60's one  are  approximate
                if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal) && (cvd2_sig_status.stable_cnt > 2))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_60;
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd pali-->pal60 \n");
                }
                else if (cvd2_sig_status.stable_cnt > SWITCH_CNT2)  //try secam format
                    tvafe_cvd2_write_mode_reg(TVIN_SIG_FMT_CVBS_SECAM);
                else if (cvd2_sig_status.stable_cnt > SWITCH_CNT_SECAM)  // 10*100
                {
					if ((cvd2_sig_status.line625) && (cvd2_sig_status.secam))// SECAM detect done
					{
						cvd2_sig_status.detected_sd_state = SD_SECAM;
                        pr_info("cvd pali-->secam \n");
					}
					else
					{
						cvd2_sig_status.detected_sd_state = SD_PAL_CN;
                        pr_info("cvd pali-->palcn \n");
					}
					tvafe_cvd2_reset_cnt();
                }
            }

			break;
		case SD_PAL_M:
            // check current state (PAL M)
			if ((!cvd2_sig_status.line625) && (cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal))
            {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if (cvd2_sig_status.cordic_data_min > data)
                    cvd2_sig_status.cordic_data_min = data;
                if (cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if (cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1)
                {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (CORDIC_FILTER_COUNT - 2);
                    cvd2_sig_status.fc_more = ((cvd2_sig_status.cordic_data_sum < FC_MORE_PAL_M_TO_PAL_60_MAX) &&
                                   (cvd2_sig_status.cordic_data_sum > FC_MORE_PAL_M_TO_PAL_60_MIN)) ? 1 : 0;
        		    if (cvd2_sig_status.fc_more)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_60;
                        pr_info(" cvd palm-->pal60 \n");
        		    }
        		    else
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_M;                        //pr_info("switch to pal60 state\n");

        		    }
                    tvafe_cvd2_reset_cnt();
                }
			}
            else
            {
                cvd2_sig_status.stable_cnt++;

                if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal) && (cvd2_sig_status.stable_cnt > SWITCH_CNT1))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_CN;    //PAL_CN''s chroma subcarries & PAL_M's one  are  approximate
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd palm-->palcn \n");
                }
                else if (cvd2_sig_status.stable_cnt > SWITCH_CNT2)
                {
					if ((!cvd2_sig_status.line625) && (!cvd2_sig_status.pal))
					{
						cvd2_sig_status.detected_sd_state = SD_NTSC;  //NTSC_M''s chroma subcarries & PAL_M's one  are  approximate
                        pr_info("cvd palm-->ntsc \n");
                    }
                    else if ((!cvd2_sig_status.line625) && (cvd2_sig_status.fc_more))
                    {
						cvd2_sig_status.detected_sd_state = SD_PAL_60;
                        pr_info("cvd palm-->pal60 \n");
                    }
                    else
                    {
						cvd2_sig_status.detected_sd_state = SD_UNLOCK;
                        pr_info("cvd palm-->unlock \n");
                    }
                    tvafe_cvd2_reset_cnt();
				}
            }
			break;
		case SD_PAL_CN:
			if ((cvd2_sig_status.line625) && (cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal))
            {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if (cvd2_sig_status.cordic_data_min > data)
                    cvd2_sig_status.cordic_data_min = data;
                if (cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1)
                {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (CORDIC_FILTER_COUNT - 2);
        		    cvd2_sig_status.fc_more = ((cvd2_sig_status.cordic_data_sum < FC_MORE_PAL_N_TO_PAL_I_MAX) &&
                                   (cvd2_sig_status.cordic_data_sum > FC_MORE_PAL_N_TO_PAL_I_MIN)) ? 1 : 0;
        		    if (cvd2_sig_status.fc_more)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_I;
                        pr_info("cvd palcn-->pali \n");
        		    }
        		    else
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_CN;
                    tvafe_cvd2_reset_cnt();
                }
			}
            else
            {
			    cvd2_sig_status.stable_cnt++;
                //tvafe_cvd2_reset_cnt();

                if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal) && (cvd2_sig_status.stable_cnt > SWITCH_CNT1))
                {  //not 625 lines
                    cvd2_sig_status.detected_sd_state = SD_PAL_M;     //PAL_CN''s chroma subcarries & PAL_M's one  are  approximate
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd palcn-->palm \n");
                }
                else if (cvd2_sig_status.stable_cnt > SWITCH_CNT2)
                {
				    cvd2_sig_status.detected_sd_state = SD_UNLOCK;
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd palcn-->unlock \n");
                }
            }
			break;
        case SD_NTSC_443:
            if ((!cvd2_sig_status.line625) && (cvd2_sig_status.chroma_lock) && (!cvd2_sig_status.pal))
            {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if (cvd2_sig_status.cordic_data_min > data)
                    cvd2_sig_status.cordic_data_min = data;
                if (cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1)
                {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (CORDIC_FILTER_COUNT - 2);
        		    cvd2_sig_status.fc_Less = ((cvd2_sig_status.cordic_data_sum < FC_LESS_NTSC443_TO_NTSCM_MAX) &&
                                   (cvd2_sig_status.cordic_data_sum > FC_LESS_NTSC443_TO_NTSCM_MIN)) ? 1 : 0;
        		    if (cvd2_sig_status.fc_Less)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_NTSC;
                        pr_info("cvd ntsc443-->ntsc \n");
        		    }
        		    else
        		    	cvd2_sig_status.detected_sd_state = SD_NTSC_443;
                    tvafe_cvd2_reset_cnt();
                }
			}
            else
            {
                cvd2_sig_status.stable_cnt++;
				if((!cvd2_sig_status.line625) && (!cvd2_sig_status.pal) && (cvd2_sig_status.stable_cnt > SWITCH_CNT1))
				{
				    cvd2_sig_status.detected_sd_state = SD_NTSC_443;      //NTSC_433''s chroma subcarries & PAL_60's one  are  approximate
                    tvafe_cvd2_reset_cnt();
				}
                else if (cvd2_sig_status.stable_cnt > SWITCH_CNT2)
                {
				    cvd2_sig_status.detected_sd_state = SD_UNLOCK;
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd ntsc443-->unlock \n");
                }
            }
			break;
		case SD_NTSC:
			if ((!cvd2_sig_status.line625) && (cvd2_sig_status.chroma_lock) && (!cvd2_sig_status.pal))
            {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if (cvd2_sig_status.cordic_data_min > data)
                    cvd2_sig_status.cordic_data_min = data;
                if (cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1)
                {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (CORDIC_FILTER_COUNT - 2);
        		    cvd2_sig_status.fc_more = ((cvd2_sig_status.cordic_data_sum < FC_MORE_NTSCM_TO_NTSC443_MAX) &&
                                   (cvd2_sig_status.cordic_data_sum > FC_MORE_NTSCM_TO_NTSC443_MIN)) ? 1 : 0;
        		    if(cvd2_sig_status.fc_more)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_NTSC_443;
                        pr_info("cvd ntsc-->ntsc443 \n");
        		    }
        		    else
        		    	cvd2_sig_status.detected_sd_state = SD_NTSC;
                    tvafe_cvd2_reset_cnt();
			    }
            }
            else
            {
                cvd2_sig_status.stable_cnt++;
				if((!cvd2_sig_status.line625)&& (!cvd2_sig_status.pal) && (cvd2_sig_status.stable_cnt > SWITCH_CNT1))
				{
				    cvd2_sig_status.detected_sd_state = SD_NTSC;      //NTSC_M''s chroma subcarries & PAL_M's one  are  approximate
                    tvafe_cvd2_reset_cnt();
                }
                else if (cvd2_sig_status.stable_cnt > SWITCH_CNT2)
                {
				    cvd2_sig_status.detected_sd_state = SD_UNLOCK;
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd ntsc-->unlock \n");
                }
            }
			break;
		case SD_PAL_60:
			if ((!cvd2_sig_status.line625) && (cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal))
            {
                cvd2_sig_status.cordic_data_sum += data;
                cvd2_sig_status.stable_cnt++;

                if( cvd2_sig_status.cordic_data_min > data)
                     cvd2_sig_status.cordic_data_min = data;
                if( cvd2_sig_status.cordic_data_max < data)
                    cvd2_sig_status.cordic_data_max = data;

                if(cvd2_sig_status.stable_cnt > CORDIC_FILTER_COUNT -1) {
                    cvd2_sig_status.cordic_data_sum -= (cvd2_sig_status.cordic_data_min + cvd2_sig_status.cordic_data_max);  //get rid off the min & max value
                    cvd2_sig_status.cordic_data_sum = cvd2_sig_status.cordic_data_sum / (CORDIC_FILTER_COUNT - 2);
        		    cvd2_sig_status.fc_Less = ((cvd2_sig_status.cordic_data_sum < FC_LESS_PAL_60_TO_PAL_M_MAX) &&
                                   (cvd2_sig_status.cordic_data_sum > FC_LESS_PAL_60_TO_PAL_M_MIN)) ? 1 : 0;
        		    if(cvd2_sig_status.fc_Less)
        		    {
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_M;
                        pr_info("cvd pal60-->palm \n");
        		    }
        		    else
        		    	cvd2_sig_status.detected_sd_state = SD_PAL_60;
                    tvafe_cvd2_reset_cnt();
                }
			}
            else
            {
                //tvafe_cvd2_reset_cnt();
                cvd2_sig_status.stable_cnt++;
                if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal) && (cvd2_sig_status.line625) && (cvd2_sig_status.stable_cnt > SWITCH_CNT1))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_I;	//PAL_ i''s chroma subcarries & PAL_60's one  are  approximate
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd pal60-->pali \n");
                }
                else if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal) && (cvd2_sig_status.fc_Less) && (cvd2_sig_status.stable_cnt > SWITCH_CNT1))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_M;
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd pal60-->palm \n");
                }
                else if ((cvd2_sig_status.chroma_lock) && (cvd2_sig_status.pal) && (cvd2_sig_status.stable_cnt > SWITCH_CNT1))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_60;
                    tvafe_cvd2_reset_cnt();
                }
                else if ((!cvd2_sig_status.line625) && (!cvd2_sig_status.pal) && (cvd2_sig_status.stable_cnt > SWITCH_CNT2))
                {
					cvd2_sig_status.detected_sd_state = SD_NTSC;      //NTSC_433''s chroma subcarries & PAL_60's one  are  approximate
                    pr_info("cvd pal60-->ntsc \n");
			    }
            }
			break;
		case SD_SECAM:
            if ((cvd2_sig_status.line625) && (cvd2_sig_status.secam) && (cvd2_sig_status.chroma_lock))
				cvd2_sig_status.detected_sd_state = SD_SECAM;
			else
            {
                cvd2_sig_status.stable_cnt++;
                if ((cvd2_sig_status.line625) && (!cvd2_sig_status.secam) && (cvd2_sig_status.pal) && (cvd2_sig_status.stable_cnt > SWITCH_CNT1))
                {
                    cvd2_sig_status.detected_sd_state = SD_PAL_I;
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd secam-->pali \n");
                }
                else if (cvd2_sig_status.stable_cnt > SWITCH_CNT2)
                {
                    cvd2_sig_status.detected_sd_state = SD_UNLOCK;
                    tvafe_cvd2_reset_cnt();
                    pr_info("cvd secam-->unlock \n");
                }

            }
			break;
        case SD_VCR :
            break;
		default:
			break;
	}
#endif

    return ;
}

#ifdef VDIN_FIXED_FMT_TEST
void tvafe_cvd2_get_status_from_fmt(tvin_sig_fmt_t fmt)
{

    switch (fmt ) {
        case TVIN_SIG_FMT_CVBS_NTSC_M:
            cvd2_sig_status.cur_sd_state = SD_NTSC;
            break;
        case TVIN_SIG_FMT_CVBS_NTSC_443:
            cvd2_sig_status.cur_sd_state = SD_NTSC_443;
            break;
        case TVIN_SIG_FMT_CVBS_PAL_I:
            cvd2_sig_status.cur_sd_state = SD_PAL_I;
            break;
        case TVIN_SIG_FMT_CVBS_PAL_M:
            cvd2_sig_status.cur_sd_state = SD_PAL_M;
            break;
        case TVIN_SIG_FMT_CVBS_SECAM:
            cvd2_sig_status.cur_sd_state = SD_SECAM;
            break;
        case TVIN_SIG_FMT_CVBS_PAL_60:
            cvd2_sig_status.cur_sd_state = SD_PAL_60;
            break;
        case TVIN_SIG_FMT_CVBS_PAL_CN:
            cvd2_sig_status.cur_sd_state = SD_PAL_CN;
            break;
        default :
            //init cvd
            break;
    }

}
#endif

void tvafe_set_cvd2_default(unsigned int mem_addr, unsigned int mem_size, enum tvin_sig_fmt_e fmt)
{
    unsigned int i = 0;

    //get cvd2 memory address and size
    cvd2_sig_status.cvd2_mem_addr = mem_addr;
    cvd2_sig_status.cvd2_mem_size = mem_size;

    /** write 7740 register **/
    tvafe_adc_configure(fmt);

    //WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
    //WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);

    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 1);
    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 5);
    WRITE_APB_REG(((ADC_BASE_ADD+0x21)<<2), 7);

    /** write top register **/
    i = 0;
    while (cvbs_top_reg_default[i][0] != 0xFFFFFFFF) {
        WRITE_APB_REG(cvbs_top_reg_default[i][0], cvbs_top_reg_default[i][1]);
        i++;
    }


#ifndef VDIN_FIXED_FMT_TEST
    init_cvd2_reg_module(SD_PAL_I);
    tvafe_cvd2_memory_init(fmt);
#else
    //for fixed format test
    tvafe_cvd2_write_mode_reg(fmt);

    tvafe_cvd2_get_status_from_fmt(fmt);
    cvd2_sig_status.detected_sd_state = cvd2_sig_status.cur_sd_state;
#endif
    return;
}


enum tvin_sig_fmt_e tvafe_cvd2_get_format(void)
{
    enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
    switch (cvd2_sig_status.cur_sd_state) {
        case SD_NTSC:
            fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
            break;
        case SD_NTSC_443:
            fmt = TVIN_SIG_FMT_CVBS_NTSC_443;
            break;
        case SD_PAL_I:
            fmt = TVIN_SIG_FMT_CVBS_PAL_I;
            break;
        case SD_PAL_M:
            fmt = TVIN_SIG_FMT_CVBS_PAL_M;
            break;
        case SD_SECAM:
            fmt = TVIN_SIG_FMT_CVBS_SECAM;
            break;
        case SD_PAL_60:
            fmt = TVIN_SIG_FMT_CVBS_PAL_60;
            break;
        case SD_PAL_CN:
            fmt = TVIN_SIG_FMT_CVBS_PAL_CN;
            break;
        default :
            //init cvd
            break;
    }
    if (fmt == TVIN_SIG_FMT_NULL)
    {
        pr_info("CVD not find format, cur_sd_state = %d. \n", cvd2_sig_status.cur_sd_state);
    }

    return fmt;
}


// *****************************************************************************
// Function: configure video mode settings
//
//   Params: video format
//
//   Return: success/error
//
// *****************************************************************************
void tvafe_cvd2_video_mode_confiure(enum tvin_sig_fmt_e fmt, enum tvin_port_e port)
{
    tvafe_cvd2_write_mode_reg(fmt);

    //s-video setting
    if ((port >= TVIN_PORT_SVIDEO0) &&
        (port <= TVIN_PORT_SVIDEO7))
    {
	    WRITE_APB_REG_BITS(CVD2_CONTROL0, 1, YC_SRC_BIT, YC_SRC_WID);
	    WRITE_APB_REG_BITS(CVD2_COMB_FILTER_CONFIG, 0, SV_BF_BIT, SV_BF_WID);
    }

    return;
}
// *****************************************************************************
// Function: CVD2 video AGC handler
//
//   Params: none
//
//   Return: success/error
//
// *****************************************************************************
int  tvafe_cvd2_video_agc_handler(struct tvafe_info_s *info)
{
    int ret = 0;
    unsigned int diff;
    unsigned char diff_val = 0;

    //signal stable check
    if (info->param.status != TVIN_SIG_STATUS_STABLE)
    {
        //reset agv counter
        cvd2_sig_status.agc.cnt = 0;
        cvd2_sig_status.agc.dgain = 0;
        return ret;
    }

    if (cvd2_sig_status.agc.cnt++ < 800)
        return ret;        // Normal Range in the beginning 2 sec

        //if dgain is 200, do not need agc
    cvd2_sig_status.agc.dgain = READ_APB_REG_BITS(CVD2_AGC_GAIN_STATUS_7_0, AGC_GAIN_7_0_BIT, AGC_GAIN_7_0_WID);
    cvd2_sig_status.agc.dgain |= READ_APB_REG_BITS(CVD2_AGC_GAIN_STATUS_11_8, AGC_GAIN_11_8_BIT, AGC_GAIN_11_8_WID)<<8;

    // Gain adjust
    diff = ABS((signed int)cvd2_sig_status.agc.dgain - (signed int)DAGC_GAIN_STANDARD);
    if (diff > DAGC_GAIN_RANGE)
    {           // if large than 100, need adjust
        //pr_info("CVD: digital_gain_diff=0x%x, digital gain: 0x%x\n", diff,cvd2_sig_status.agc.dgain);
        if (diff > DAGC_GAIN_RANGE2)   // Big    Range
            diff_val = 0x04;
        else if (diff > DAGC_GAIN_RANGE1)   // Medium Range
            diff_val = 0x03;
        else if (diff > DAGC_GAIN_RANGE)    // Small  Range
            diff_val = 0x02;
        else                                // FineTune Range
            diff_val = 1;

        if (cvd2_sig_status.agc.dgain > DAGC_GAIN_STANDARD)
        {
            if ((cvd2_sig_status.agc.again + diff_val) > 0xFF)
                cvd2_sig_status.agc.again = 0xFF;
            else
                cvd2_sig_status.agc.again += diff_val;
            //pr_info("CVD: Analog_adj=0x%x, ++analog gain: 0x%x\n",diff_val, cvd2_sig_status.agc.again);
        }
        else
        {
            if (cvd2_sig_status.agc.again > diff_val)
                cvd2_sig_status.agc.again -= diff_val;
            else
                cvd2_sig_status.agc.again = 0;

            //pr_info("CVD: Analog_adj=0x%x, --analog gain: 0x%x\n",diff_val, cvd2_sig_status.agc.again);
        }
        //adjust adc gain value
        WRITE_APB_REG_BITS(ADC_REG_05, cvd2_sig_status.agc.again, ADCGAINA_BIT, ADCGAINA_WID);
    }

    cvd2_sig_status.agc.cnt = 780;
    cvd2_sig_status.agc.dgain = 0;

    return ret;
}

// call by 10ms_timer at frontend side
bool tvafe_cvd_no_sig(void)
{
    bool tmp = 0;

#ifndef VDIN_FIXED_FMT_TEST
    tvafe_cvd2_search_video_mode();

    if (cvd2_sig_status.no_sig == 1)
    {
        tmp = 1;
    }
    else
    {
        tmp = 0;
    }
#endif
    return tmp;
}
// *****************************************************************************
// Function:get ADC signal info(hcnt,vcnt,hpol,vpol)
//
//   Params: none
//
//   Return: none
//
// *****************************************************************************
bool tvafe_cvd_fmt_chg(void)
{
    bool tmp = 0;
#ifndef VDIN_FIXED_FMT_TEST
    if (cvd2_sig_status.detected_sd_state != cvd2_sig_status.cur_sd_state)
    {
        if (cvd2_sig_status.cvd_fmt_chg_cnt++ > NEW_FMT_CHANGE_CNT)
        {
            cvd2_sig_status.cvd_fmt_chg_cnt = 0;

            pr_info("%s: status  %d --> %d . \n", __FUNCTION__, cvd2_sig_status.cur_sd_state, cvd2_sig_status.detected_sd_state);
            cvd2_sig_status.cur_sd_state = cvd2_sig_status.detected_sd_state;

            tmp = 1;
            if (cvd2_sig_status.detected_sd_state != SD_NO_SIG)
            {
                init_cvd2_reg_module(cvd2_sig_status.detected_sd_state);
            }

        }
    }
#endif
    return tmp;
}

// *****************************************************************************
// Function: cvd2 state init
//
//   Params: none
//
//   Return: none
//
// *****************************************************************************
void tvafe_cvd2_state_init(void)
{
//    pr_info("%s \n", __FUNCTION__);
    cvd2_sig_status.cur_sd_state = SD_NO_SIG;
    cvd2_sig_status.detected_sd_state = SD_NO_SIG;

}

int tvafe_cvd2_get_cordic_para(struct tvafe_cvd2_fmt_cordic_s *info)
{
    if(info ==NULL)
       return -1;
    copy_to_user(info, &cvd2_sig_status.cordic, sizeof(struct tvafe_cvd2_fmt_cordic_s));

    return 0;
}

int tvafe_cvd2_set_cordic_para(struct tvafe_cvd2_fmt_cordic_s *info)
{
    if(info ==NULL)
       return -1;
    copy_from_user(&cvd2_sig_status.cordic, info, sizeof(struct tvafe_cvd2_fmt_cordic_s));
    return 0;
}