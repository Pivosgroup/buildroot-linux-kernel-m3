/******************************Includes************************************/
#include <linux/errno.h>
#include <mach/am_regs.h>

#include "tvin_global.h"
#include "tvafe.h"
#include "tvafe_regs.h"
#include "tvafe_adc.h"
#include "tvafe_cvd.h"
#include "tvafe_general.h"

/***************************Global Variables**********************************/

enum tvafe_adc_pin_e            tvafe_default_cvbs_out;
enum tvafe_state_e   tvafe_state;
unsigned int         tvafe_state_counter;       // used in TVAFE_STATE_NOSIG & TVAFE_STATE_UNSTABLE
unsigned int         tvafe_exit_nosig_counter;  // used in TVAFE_STATE_NOSIG
unsigned int         tvafe_back_nosig_counter;  // used in TVAFE_STATE_UNSTABLE
unsigned int         tvafe_exit_stable_counter; // used in TVAFE_STATE_STABLE
unsigned int         tvafe_back_stable_counter; // used in TVAFE_STATE_UNSTABLE

// *****************************************************************************
// Function:
//
//   Params:
//
//   Return:
//
// *****************************************************************************
enum tvafe_adc_pin_e tvafe_get_free_pga_pin(struct tvafe_pin_mux_s *pinmux)
{
    unsigned int i = 0;
    unsigned int flag = 0;
    enum tvafe_adc_pin_e ret = TVAFE_ADC_PIN_NULL;

    for (i=0; i<TVAFE_SRC_SIG_MAX_NUM; i++)
    {
        if (pinmux->pin[i] == TVAFE_ADC_PIN_A_PGA_0)
            flag |= 0x00000001;
        if (pinmux->pin[i] == TVAFE_ADC_PIN_A_PGA_1)
            flag |= 0x00000002;
        if (pinmux->pin[i] == TVAFE_ADC_PIN_A_PGA_2)
            flag |= 0x00000004;
        if (pinmux->pin[i] == TVAFE_ADC_PIN_A_PGA_3)
            flag |= 0x00000008;
    }
    if (!(flag&0x00000001))
    {
        ret = TVAFE_ADC_PIN_A_PGA_0;
    }
    else if (!(flag&0x00000002))
    {
        ret = TVAFE_ADC_PIN_A_PGA_1;
    }
    else if (!(flag&0x00000004))
    {
        ret = TVAFE_ADC_PIN_A_PGA_2;
    }
    else if (!(flag&0x00000008))
    {
        ret = TVAFE_ADC_PIN_A_PGA_3;
    }
    else // In the worst case, CVBS_OUT links to TV
    {
        ret = pinmux->pin[CVBS0_Y];
    }
    return ret;
}
#include <linux/kernel.h>
static inline enum tvafe_adc_ch_e tvafe_pin_adc_muxing(enum tvafe_adc_pin_e pin)
{
    enum tvafe_adc_ch_e ret = TVAFE_ADC_CH_NULL;

    if ((pin >= TVAFE_ADC_PIN_A_PGA_0) && (pin <= TVAFE_ADC_PIN_A_PGA_3))
    {
        WRITE_APB_REG_BITS(ADC_REG_06, 1, ENPGA_BIT, ENPGA_WID);
        WRITE_APB_REG_BITS(ADC_REG_17, pin-TVAFE_ADC_PIN_A_PGA_0, INMUXA_BIT, INMUXA_WID);
        ret = TVAFE_ADC_CH_PGA;
    }
    else if ((pin >= TVAFE_ADC_PIN_A_0) && (pin <= TVAFE_ADC_PIN_A_3))
    {
        WRITE_APB_REG_BITS(ADC_REG_06, 0, ENPGA_BIT, ENPGA_WID);
        WRITE_APB_REG_BITS(ADC_REG_17, pin-TVAFE_ADC_PIN_A_0, INMUXA_BIT, INMUXA_WID);
        ret = TVAFE_ADC_CH_A;
    }
    else if ((pin >= TVAFE_ADC_PIN_B_0) && (pin <= TVAFE_ADC_PIN_B_4))
    {
        WRITE_APB_REG_BITS(ADC_REG_17, pin-TVAFE_ADC_PIN_B_0, INMUXB_BIT, INMUXB_WID);
        ret = TVAFE_ADC_CH_B;
    }
    else if ((pin >= TVAFE_ADC_PIN_C_0) && (pin <= TVAFE_ADC_PIN_C_4))
    {
        WRITE_APB_REG_BITS(ADC_REG_18, pin-TVAFE_ADC_PIN_C_0, INMUXC_BIT, INMUXC_WID);
        ret = TVAFE_ADC_CH_C;
    }
    return ret;
}

/*
000: abc
001: acb
010: bac
011: bca
100: cab
101: cba
*/
static inline int tvafe_adc_top_muxing(enum tvafe_adc_ch_e gy,
                                        enum tvafe_adc_ch_e bpb,
                                        enum tvafe_adc_ch_e rpr,
                                        unsigned int s_video_flag)
{
    int ret = 0;

    switch (gy)
    {
        case TVAFE_ADC_CH_PGA:
        case TVAFE_ADC_CH_A:
            switch (bpb)
            {
                case TVAFE_ADC_CH_B:
                    // abc => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_C))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                case TVAFE_ADC_CH_C:
                    // acb => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_B))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                default:
                    ret = -EFAULT;
                    break;
            }
            break;
        case TVAFE_ADC_CH_B:
            switch (bpb)
            {
                case TVAFE_ADC_CH_PGA:
                case TVAFE_ADC_CH_A:
                    // bac => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_C))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 2, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                case TVAFE_ADC_CH_C:
                    // bca => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_PGA)
                        || (rpr == TVAFE_ADC_CH_A))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 3, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                default:
                    ret = -EFAULT;
                    break;
            }
            break;
        case TVAFE_ADC_CH_C:
            switch (bpb)
            {
                case TVAFE_ADC_CH_PGA:
                case TVAFE_ADC_CH_A:
                    // cab => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_B))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 4, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                case TVAFE_ADC_CH_B:
                    // cba => abc
                    if (s_video_flag || (rpr == TVAFE_ADC_CH_PGA)
                        || (rpr == TVAFE_ADC_CH_A))
                    {
                        WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 5, SWT_GY_BCB_RCR_IN_BIT,
                            SWT_GY_BCB_RCR_IN_WID);
                    }
                    else
                    {
                        ret = -EFAULT;
                    }
                    break;
                default:
                    ret = -EFAULT;
                    break;
            }
            break;
        default:
            ret = -EFAULT;
            break;
    }
    return ret;
}

int tvafe_source_muxing(struct tvafe_info_s *info)
{
    int ret = 0;

    switch (info->param.port)
    {
        case TVIN_PORT_CVBS0:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS0_Y]) == TVAFE_ADC_CH_PGA)
            {
                if (info->pinmux->pin[CVBS0_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS0_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_CVBS;
			}
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS1:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS1_Y]) == TVAFE_ADC_CH_PGA)
            {
                if (info->pinmux->pin[CVBS1_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS1_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_CVBS;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS2:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS2_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS2_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS2_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS2_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_CVBS;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS3:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS3_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS3_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS3_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS3_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_CVBS;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS4:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS4_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS4_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS4_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_CVBS;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
         case TVIN_PORT_CVBS5:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS5_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS5_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS5_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_CVBS;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS6:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS6_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS6_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS6_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS6_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_CVBS;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_CVBS7:
            if (tvafe_pin_adc_muxing(info->pinmux->pin[CVBS7_Y]) == TVAFE_ADC_CH_PGA)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, info->pinmux->pin[CVBS7_Y]-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[CVBS7_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[CVBS7_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_CVBS;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO0:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO0_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO0_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO0_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO0_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_SVIDEO;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO1:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO1_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO1_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO1_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO1_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_SVIDEO;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO2:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO2_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO2_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO2_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO2_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_SVIDEO;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO3:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO3_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO3_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO3_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO3_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_SVIDEO;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO4:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO4_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO4_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO4_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO4_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_SVIDEO;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO5:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO5_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO5_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO5_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_SVIDEO;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO6:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO6_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO6_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO6_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO6_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_SVIDEO;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_SVIDEO7:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO7_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[S_VIDEO7_C]), TVAFE_ADC_CH_NULL, 1) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[S_VIDEO7_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[S_VIDEO7_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_SVIDEO;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA0:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA0_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA0_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA0_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA0_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA0_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);

                WRITE_APB_REG_BITS(ADC_REG_39, 0, INSYNCMUXCTRL_BIT, INSYNCMUXCTRL_WID);
                info->src_type = TVAFE_SRC_TYPE_VGA;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA1:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA1_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA1_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA1_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA1_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA1_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                WRITE_APB_REG_BITS(ADC_REG_39, 1, INSYNCMUXCTRL_BIT, INSYNCMUXCTRL_WID);
                info->src_type = TVAFE_SRC_TYPE_VGA;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA2:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA2_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA2_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA2_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA2_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA2_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_VGA;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA3:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA3_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA3_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA3_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA3_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA3_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_VGA;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA4:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA4_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA4_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA4_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA4_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA4_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_VGA;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA5:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA5_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA5_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA5_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA5_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_VGA;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA6:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA6_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA6_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA6_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA6_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA6_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_VGA;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_VGA7:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[VGA7_G]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA7_B]), tvafe_pin_adc_muxing(info->pinmux->pin[VGA7_R]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[VGA7_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[VGA7_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_VGA;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP0:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP0_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP0_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP0_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP0_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP0_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_COMP;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP1:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP1_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP1_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP1_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP1_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP1_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_COMP;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP2:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP2_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP2_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP2_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP2_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP2_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_COMP;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP3:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP3_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP3_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP3_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP3_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP3_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_COMP;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP4:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP4_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP4_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP4_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP4_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP4_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_COMP;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP5:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP5_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP5_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP5_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP5_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP5_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_COMP;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP6:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP6_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP6_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP6_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP6_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP6_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_COMP;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        case TVIN_PORT_COMP7:
            if (tvafe_adc_top_muxing(tvafe_pin_adc_muxing(info->pinmux->pin[COMP7_Y]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP7_PB]), tvafe_pin_adc_muxing(info->pinmux->pin[COMP7_PR]), 0) == 0)
            {
                WRITE_APB_REG_BITS(ADC_REG_20, tvafe_default_cvbs_out-TVAFE_ADC_PIN_A_PGA_0, INMUXBUF_BIT, INMUXBUF_WID);
                if (info->pinmux->pin[COMP7_SOG] >= TVAFE_ADC_PIN_SOG_0)
                    WRITE_APB_REG_BITS(ADC_REG_24, (info->pinmux->pin[COMP7_SOG] - TVAFE_ADC_PIN_SOG_0), INMUXSOG_BIT, INMUXSOG_WID);
                info->src_type = TVAFE_SRC_TYPE_COMP;
            }
            else
            {
                ret = -EFAULT;
            }
            break;
        default:
            ret = -EFAULT;
            break;
    }

	if (!ret)
		info->sig_status_cnt = 0;

    return ret;
}

void tvafe_vga_set_edid(struct tvafe_vga_edid_s *edid)
{
    unsigned int i = 0;

    // diable TCON
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_7, 0,  1, 1);
    // diable DVIN
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 27, 1);
    // DDC_SDA0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 13, 1);
    // DDC_SCL0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 12, 1);

    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, EDID_CLK_EN_BIT,EDID_CLK_EN_WID); // VGA_CLK_EN
    // APB Bus accessing mode
    WRITE_APB_REG(TVFE_EDID_CONFIG, 0x00000000);
    WRITE_APB_REG(TVFE_EDID_RAM_ADDR, 0x00000000);
    for (i=0; i<256; i++)
        WRITE_APB_REG(TVFE_EDID_RAM_WDATA, (unsigned int)edid->value[i]);
    // Slave IIC acessing mode, 8-bit standard IIC protocol
    WRITE_APB_REG(TVFE_EDID_CONFIG, 0x01800050);
}

void tvafe_vga_get_edid(struct tvafe_vga_edid_s *edid)
{
    unsigned int i = 0;

    // diable TCON
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_7, 0,  1, 1);
    // diable DVIN
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 27, 1);
    // DDC_SDA0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 13, 1);
    // DDC_SCL0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 12, 1);

    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, EDID_CLK_EN_BIT,EDID_CLK_EN_WID); // VGA_CLK_EN
    // APB Bus accessing mode
    WRITE_APB_REG(TVFE_EDID_CONFIG, 0x00000000);
    WRITE_APB_REG(TVFE_EDID_RAM_ADDR, 0x00000100);
    for (i=0; i<256; i++)
        edid->value[i] = (unsigned char)(READ_APB_REG_BITS(TVFE_EDID_RAM_RDATA, EDID_RAM_RDATA_BIT, EDID_RAM_RDATA_WID));
    // Slave IIC acessing mode, 8-bit standard IIC protocol
    WRITE_APB_REG(TVFE_EDID_CONFIG, 0x01800050);

    return;
}

///////////////////TVFE top control////////////////////
const static unsigned int aafilter_ctl[][2] = {

    //TVIN_SIG_FMT_NULL = 0,
	{0,0},
    //VDIN_SIG_FORMAT_VGA_512X384P_60D147,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_560X384P_60D147,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X200P_59D924,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X350P_85D080,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X400P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X400P_85D080,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X400P_59D638,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X400P_56D416,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480I_29D970,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_66D619,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_66D667,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_72D809,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_75D000_A,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_85D008,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_59D638,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X480P_75D000_B,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_640X870P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X350P_70D086,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X400P_85D039,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X400P_70D086,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X400P_87D849,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X400P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_720X480P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_752X484I_29D970,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_768X574I_25D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_56D250,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_60D317,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_72D188,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_800X600P_85D061,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_832X624P_75D087,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_848X480P_84D751,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_59D278,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_74D927,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768I_43D479,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_60D004,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_70D069,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_75D029,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_84D997,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_74D925,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_75D020,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_70D008,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_75D782,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_77D069,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X768P_71D799,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1024X1024P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1053X754I_43D453,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1056X768I_43D470,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1120X750I_40D021,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X864P_70D012,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X864P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X864P_84D999,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X870P_75D062,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X900P_65D950,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X900P_66D004,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X900P_76D047,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1152X900P_76D149,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1244X842I_30D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X768P_59D995,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X768P_74D893,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X768P_84D837,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X960P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X960P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X960P_85D002,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024I_43D436,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_60D020,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_75D025,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_85D024,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_59D979,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_72D005,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_60D002,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_67D003,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_74D112,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_76D179,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_66D718,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_66D677,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_76D107,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1280X1024P_59D996,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1360X768P_59D799,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1360X1024I_51D476,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1440X1080P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200I_48D040,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_65D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_70D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_80D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1200P_85D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1600X1280P_66D931,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1680X1080P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1792X1344P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1792X1344P_74D997,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1856X1392P_59D995,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1856X1392P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1868X1200P_75D000,
    //VDIN_SIG_FORMAT_VGA_1920X1080P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1080P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1080P_85D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1200P_84D932,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1200P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1200P_85D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1234P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1234P_85D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1440P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_1920X1440P_75D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_VGA_2048X1536P_60D000_A,
	{
	        0x00082222, //
	        0x252b39c6, //
    },
    //VDIN_SIG_FORMAT_VGA_2048X1536P_75D000,
    {0,0},
    //VDIN_SIG_FORMAT_VGA_2048X1536P_60D000_B,
    {0,0},
    //VDIN_SIG_FORMAT_VGA_2048X2048P_60D008,
    {0,0},
    //TVIN_SIG_FMT_VGA_MAX,
    {0,0},

///////////////////////////////////////////////////////////////
    //VDIN_SIG_FORMAT_COMPONENT_480P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_480I_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_576P_50D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_576I_50D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_720P_59D940,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_720P_50D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_23D976,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_24D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_25D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_30D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_50D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080P_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_29D970,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_47D952,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_48D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_50D000_A,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_50D000_B,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_50D000_C,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_COMPONENT_1080I_60D000,
	{
	        0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //TVIN_SIG_FMT_COMP_MAX,
    {0,0},

    //VDIN_SIG_FORMAT_CVBS_NTSC_M,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_NTSC_443,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_PAL_I,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_PAL_M,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_PAL_60,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_PAL_CN,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },
    //VDIN_SIG_FORMAT_CVBS_SECAM,
	{       0x00082222, // TVFE_AAFILTER_CTRL1
	        0x252b39c6, // TVFE_AAFILTER_CTRL2
    },

    //VDIN_SIG_FORMAT_MAX,
	{0,0 }
};

// *****************************************************************************
// Function:set aafilter control
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_aafilter_control(enum tvin_sig_fmt_e fmt)
{

    WRITE_APB_REG(TVFE_AAFILTER_CTRL1, aafilter_ctl[fmt][0]);
    WRITE_APB_REG(TVFE_AAFILTER_CTRL2, aafilter_ctl[fmt][1]);


    return;
}

void tvafe_top_set_cpump_para(enum tvin_sig_fmt_e fmt)
{
    unsigned char clamp_type = 0;
    unsigned int clamping_delay;

    //check top charge pump clamping bit
    clamp_type = READ_APB_REG_BITS(TVFE_CLAMP_INTF, CLAMP_EN_BIT, CLAMP_EN_WID);
    if (clamp_type == 1) {
        clamping_delay = tvin_fmt_tbl[fmt].hs_front + tvin_fmt_tbl[fmt].hs_width
                    + tvin_fmt_tbl[fmt].h_active;

        WRITE_APB_REG_BITS(TVFE_CLP_MUXCTRL1, clamping_delay, CLAMPING_DLY_BIT,
            CLAMPING_DLY_WID);
    }

}
// *****************************************************************************
// Function:set bp gate of tvfe top module
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_bp_gate(enum tvin_sig_fmt_e fmt)
{
    unsigned int h_bp_end,h_bp_start;
    unsigned int v_bp_end,v_bp_start;

    h_bp_start = tvin_fmt_tbl[fmt].hs_width + 1;
    WRITE_APB_REG_BITS(TVFE_BPG_BACKP_H, h_bp_start, BACKP_H_ST_BIT, BACKP_H_ST_WID);

    h_bp_end = tvin_fmt_tbl[fmt].h_total
                - tvin_fmt_tbl[fmt].hs_front + 1;
    WRITE_APB_REG_BITS(TVFE_BPG_BACKP_H, h_bp_end, BACKP_H_ED_BIT, BACKP_H_ED_WID);

    v_bp_start = tvin_fmt_tbl[fmt].vs_width + 1;
    WRITE_APB_REG_BITS(TVFE_BPG_BACKP_V, v_bp_start, BACKP_V_ST_BIT, BACKP_V_ST_WID);

    v_bp_end = tvin_fmt_tbl[fmt].v_total
                - tvin_fmt_tbl[fmt].vs_front + 1;
    WRITE_APB_REG_BITS(TVFE_BPG_BACKP_V, v_bp_end, BACKP_V_ED_BIT, BACKP_V_ED_WID);

    return;
}

// *****************************************************************************
// Function:set mvdet control of tvfe module
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_mvdet_control(enum tvin_sig_fmt_e fmt)
{
    unsigned int sd_mvd_reg_15_1b[] = {0, 0, 0, 0, 0, 0, 0,};

    if ((fmt > TVIN_SIG_FMT_COMP_480P_60D000)
        && (fmt < TVIN_SIG_FMT_COMP_576I_50D000)) {
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL1, sd_mvd_reg_15_1b[0]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL2, sd_mvd_reg_15_1b[1]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL3, sd_mvd_reg_15_1b[2]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL4, sd_mvd_reg_15_1b[3]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL5, sd_mvd_reg_15_1b[4]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL6, sd_mvd_reg_15_1b[5]);
        WRITE_APB_REG(TVFE_DVSS_MVDET_CTRL7, sd_mvd_reg_15_1b[6]);

    }

    return;
}

// *****************************************************************************
// Function:set wss of tvfe top module
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_wss_control(enum tvin_sig_fmt_e fmt)
{
    unsigned int hd_mvd_reg_2a_2d[] = {0, 0, 0, 0};

    if (fmt > TVIN_SIG_FMT_COMP_720P_59D940
        && fmt < TVIN_SIG_FMT_COMP_1080I_60D000) {
        WRITE_APB_REG(TVFE_MISC_WSS1_MUXCTRL1, hd_mvd_reg_2a_2d[0]);
        WRITE_APB_REG(TVFE_MISC_WSS1_MUXCTRL2, hd_mvd_reg_2a_2d[1]);
        WRITE_APB_REG(TVFE_MISC_WSS2_MUXCTRL1, hd_mvd_reg_2a_2d[2]);
        WRITE_APB_REG(TVFE_MISC_WSS2_MUXCTRL2, hd_mvd_reg_2a_2d[3]);
    }

    return;
}

// *****************************************************************************
// Function:set sfg control of tvfe top module
//
//   Params: format index
//
//   Return: none
//
// *****************************************************************************
void tvafe_top_set_sfg_mux_control(enum tvin_sig_fmt_e fmt)
{
    unsigned int tmp;

    tmp = tvin_fmt_tbl[fmt].h_total/4;
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, tmp, SFG_DET_HSTART_BIT, SFG_DET_HSTART_WID);
    tmp = tvin_fmt_tbl[fmt].h_total*3/4;
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, tmp, SFG_DET_HEND_BIT, SFG_DET_HEND_WID);

    return;
}

enum tvin_scan_mode_e tvafe_top_get_scan_mode(void)
{
    unsigned int scan_mode;

    scan_mode = READ_APB_REG_BITS(TVFE_SYNCTOP_INDICATOR3,
                        SFG_PROGRESSIVE_BIT, SFG_PROGRESSIVE_WID);

    if (scan_mode == 0)
        return TVIN_SCAN_MODE_INTERLACED;
    else
        return TVIN_SCAN_MODE_PROGRESSIVE;

}

// *****************************************************************************
// Function: get & set cal result
//
//   Params: system info
//
//   Return: none
//
// *****************************************************************************
void tvafe_set_cal_value(struct tvafe_adc_cal_s *para)
{
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET1, 1, OGO_EN_BIT, OGO_EN_WID);

    WRITE_APB_REG_BITS(ADC_REG_07, para->a_analog_gain, ADCGAINA_BIT, ADCGAINA_WID);
    WRITE_APB_REG_BITS(ADC_REG_08, para->b_analog_gain, ADCGAINB_BIT, ADCGAINB_WID);
    WRITE_APB_REG_BITS(ADC_REG_09, para->c_analog_gain, ADCGAINC_BIT, ADCGAINC_WID);

    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET1, para->a_digital_offset1,
        OGO_YG_OFFSET1_BIT, OGO_YG_OFFSET1_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET1, para->b_digital_offset1,
        OGO_UB_OFFSET1_BIT, OGO_UB_OFFSET1_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET3, para->c_digital_offset1,
        OGO_VR_OFFSET1_BIT, OGO_VR_OFFSET1_WID);

    WRITE_APB_REG_BITS(TVFE_OGO_GAIN1, para->a_digital_gain,
        OGO_YG_GAIN_BIT, OGO_YG_GAIN_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_GAIN1, para->b_digital_gain,
        OGO_UB_GAIN_BIT, OGO_UB_GAIN_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_GAIN2, para->c_digital_gain,
        OGO_VR_GAIN_BIT, OGO_VR_GAIN_WID);

    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET2, para->a_digital_offset2,
        OGO_YG_OFFSET2_BIT, OGO_YG_OFFSET2_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET2, para->b_digital_offset2,
        OGO_UB_OFFSET2_BIT, OGO_UB_OFFSET2_WID);
    WRITE_APB_REG_BITS(TVFE_OGO_OFFSET3, para->c_digital_offset2,
        OGO_VR_OFFSET2_BIT, OGO_VR_OFFSET2_WID);
}

void tvafe_get_cal_value(struct tvafe_adc_cal_s *para)
{
    para->a_analog_gain = READ_APB_REG_BITS(ADC_REG_07, ADCGAINA_BIT, ADCGAINA_WID);
    para->b_analog_gain = READ_APB_REG_BITS(ADC_REG_08, ADCGAINB_BIT, ADCGAINB_WID);
    para->c_analog_gain = READ_APB_REG_BITS(ADC_REG_09, ADCGAINC_BIT, ADCGAINC_WID);

    para->a_digital_offset1 = READ_APB_REG_BITS(TVFE_OGO_OFFSET1, OGO_YG_OFFSET1_BIT, OGO_YG_OFFSET1_WID);
    para->b_digital_offset1 = READ_APB_REG_BITS(TVFE_OGO_OFFSET1, OGO_UB_OFFSET1_BIT, OGO_UB_OFFSET1_WID);
    para->c_digital_offset1 = READ_APB_REG_BITS(TVFE_OGO_OFFSET3, OGO_VR_OFFSET1_BIT, OGO_VR_OFFSET1_WID);

    para->a_digital_gain = READ_APB_REG_BITS(TVFE_OGO_GAIN1, OGO_YG_GAIN_BIT, OGO_YG_GAIN_WID);
    para->b_digital_gain = READ_APB_REG_BITS(TVFE_OGO_GAIN1, OGO_UB_GAIN_BIT, OGO_UB_GAIN_WID);
    para->c_digital_gain = READ_APB_REG_BITS(TVFE_OGO_GAIN2, OGO_VR_GAIN_BIT, OGO_VR_GAIN_WID);

    para->a_digital_offset2 = READ_APB_REG_BITS(TVFE_OGO_OFFSET2, OGO_YG_OFFSET2_BIT, OGO_YG_OFFSET2_WID);
    para->b_digital_offset2 = READ_APB_REG_BITS(TVFE_OGO_OFFSET2, OGO_UB_OFFSET2_BIT, OGO_UB_OFFSET2_WID);
    para->c_digital_offset2 = READ_APB_REG_BITS(TVFE_OGO_OFFSET3, OGO_VR_OFFSET2_BIT, OGO_VR_OFFSET2_WID);
}

// *****************************************************************************
// Function: fetch WSS data
//
//   Params: system info
//
//   Return: none
//
// *****************************************************************************
void tvafe_get_wss_data(struct tvafe_comp_wss_s *para)
{
    para->wss1[0] = READ_APB_REG(TVFE_MISC_WSS1_INDICATOR1);
    para->wss1[1] = READ_APB_REG(TVFE_MISC_WSS1_INDICATOR2);
    para->wss1[2] = READ_APB_REG(TVFE_MISC_WSS1_INDICATOR3);
    para->wss1[3] = READ_APB_REG(TVFE_MISC_WSS1_INDICATOR4);
    para->wss1[4] = READ_APB_REG_BITS(TVFE_MISC_WSS1_INDICATOR5, WSS1_DATA_143_128_BIT, WSS1_DATA_143_128_WID);
    para->wss2[0] = READ_APB_REG(TVFE_MISC_WSS2_INDICATOR1);
    para->wss2[1] = READ_APB_REG(TVFE_MISC_WSS2_INDICATOR2);
    para->wss2[2] = READ_APB_REG(TVFE_MISC_WSS2_INDICATOR3);
    para->wss2[3] = READ_APB_REG(TVFE_MISC_WSS2_INDICATOR4);
    para->wss2[4] = READ_APB_REG_BITS(TVFE_MISC_WSS2_INDICATOR5, WSS2_DATA_143_128_BIT, WSS2_DATA_143_128_WID);
}

// *****************************************************************************
// Function: set hardcode of tvfe top module
//
//   Params: none
//
//   Return: none
//
// *****************************************************************************

void tvafe_check_cvbs_3d_comb(void)
{
#if 0
    unsigned int interrupt_status = READ_APB_REG(TVFE_INT_INDICATOR1);
    if ((interrupt_status>>WARNING_3D_BIT) & 0x01) {
        WRITE_APB_REG_BITS(TVFE_INT_INDICATOR1, 0, WARNING_3D_BIT, WARNING_3D_WID);
        WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);
        WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);
    }
#else
    unsigned int cvd2_3d_status = READ_APB_REG(CVD2_REG_95);
    if(cvd2_3d_status & 0x1ffff)  {
        //pr_info("%s: cvd2_3d_status = %x \n",__FUNCTION__, cvd2_3d_status);
        WRITE_APB_REG_BITS(CVD2_REG_B2, 1, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
        WRITE_APB_REG_BITS(CVD2_REG_B2, 0, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
    }

#endif
}

void tvafe_top_set_de(enum tvin_sig_fmt_e fmt)
{
    unsigned int hs = 0, he = 0, vs = 0, ve = 0;

    hs = tvin_fmt_tbl[fmt].hs_width + tvin_fmt_tbl[fmt].hs_bp;
    vs = tvin_fmt_tbl[fmt].vs_width + tvin_fmt_tbl[fmt].vs_bp;
    he = hs + tvin_fmt_tbl[fmt].h_active - 1;
    ve = vs + tvin_fmt_tbl[fmt].v_active - 1;
    WRITE_APB_REG_BITS(TVFE_DEG_H, hs,
                       DEG_HSTART_BIT, DEG_HSTART_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_H, he,
                       DEG_HEND_BIT, DEG_HEND_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VODD, vs,
                       DEG_VSTART_ODD_BIT, DEG_VSTART_ODD_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VODD, ve,
                       DEG_VEND_ODD_BIT, DEG_VEND_ODD_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VEVEN, vs + 1,
                       DEG_VSTART_EVEN_BIT, DEG_VSTART_EVEN_WID);
    WRITE_APB_REG_BITS(TVFE_DEG_VEVEN, ve + 1,
                       DEG_VEND_EVEN_BIT, DEG_VEND_EVEN_WID);
}

void tvafe_top_set_field_gen(enum tvin_sig_fmt_e fmt)
{
    unsigned int hs = 0, he = 0, vs = 0, ve = 0;

    hs = tvin_fmt_tbl[fmt].hs_width + tvin_fmt_tbl[fmt].hs_bp;
    vs = tvin_fmt_tbl[fmt].vs_width + tvin_fmt_tbl[fmt].vs_bp;
    he = hs + tvin_fmt_tbl[fmt].h_active - 1;
    ve = vs + tvin_fmt_tbl[fmt].v_active - 1;
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, hs,
                       DEG_HSTART_BIT, DEG_HSTART_WID);
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, he,
                       DEG_HEND_BIT, DEG_HEND_WID);
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, vs,
                       DEG_VSTART_ODD_BIT, DEG_VSTART_ODD_WID);
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, ve,
                       DEG_VEND_ODD_BIT, DEG_VEND_ODD_WID);
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, vs + 1,
                       DEG_VSTART_EVEN_BIT, DEG_VSTART_EVEN_WID);
    WRITE_APB_REG_BITS(TVFE_SYNCTOP_SFG_MUXCTRL1, ve + 1,
                       DEG_VEND_EVEN_BIT, DEG_VEND_EVEN_WID);
}


static void tvafe_top_config(enum tvin_sig_fmt_e fmt)
{
    //tvafe_top_set_aafilter_control(fmt);
    //tvafe_top_set_bp_gate(fmt);
    //tvafe_top_set_mvdet_control(fmt);
    //tvafe_top_set_sfg_mux_control(fmt);
    tvafe_top_set_wss_control(fmt);
    tvafe_top_set_de(fmt);
}


static void tvafe_stop_vga(void)
{
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, COMP_CLK_ENABLE_BIT, COMP_CLK_ENABLE_WID);
    //WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, EDID_CLK_EN_BIT, EDID_CLK_EN_WID);
    //WRITE_CBUS_REG_BITS(HHI_TVFE_AUTOMODE_CLK_CNTL, 0, 7, 1);  //?
    //...

}
#include <linux/kernel.h>

static void tvafe_reset_vga(void)
{
    //    pr_info("tvafe: tvafe_reset_vga.CVD2_RESET_REGISTER %x 1\n", CVD2_RESET_REGISTER);

    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);
    //pr_info("tvafe: tvafe_reset_vga. 2\n");
    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);  //SOFT RESET memory
    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, COMP_CLK_ENABLE_BIT, COMP_CLK_ENABLE_WID);
    //WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, EDID_CLK_EN_BIT, EDID_CLK_EN_WID);
    //pr_info("tvafe: tvafe_reset_vga. 3\n");
    //WRITE_CBUS_REG_BITS(HHI_TVFE_AUTOMODE_CLK_CNTL, 1, 7, 1);  //enable auto mode clock
    //...
    //pr_info("tvafe: tvafe_reset_vga. 4\n");
    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 1, ADC_CLK_RST_BIT, ADC_CLK_RST_WID);
    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 1, EDID_RST_BIT, EDID_RST_WID);
    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 1, VAFE_RST_BIT, VAFE_RST_WID);

}

static void tvafe_stop_comp(void)
{
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, COMP_CLK_ENABLE_BIT, COMP_CLK_ENABLE_WID);
    //WRITE_CBUS_REG_BITS(HHI_TVFE_AUTOMODE_CLK_CNTL, 0, 7, 1);  //
}

static void tvafe_reset_comp(void)
{
    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);
    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);  //SOFT RESET memory
    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, COMP_CLK_ENABLE_BIT, COMP_CLK_ENABLE_WID);
    //WRITE_CBUS_REG_BITS(HHI_TVFE_AUTOMODE_CLK_CNTL, 1, 7, 1);  //enable auto mode clock
    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 1, ADC_CLK_RST_BIT, ADC_CLK_RST_WID);
    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 1, VAFE_RST_BIT, VAFE_RST_WID);

}

static void tvafe_stop_cvbs(void)
{
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 0, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);
}

static void tvafe_reset_cvbs(void)
{
    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);
    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);  //SOFT RESET memory
    //WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
    WRITE_APB_REG_BITS(TVFE_TOP_CTRL, 1, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);

    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 1, DCLK_RST_BIT, DCLK_RST_WID);
    WRITE_APB_REG_BITS(TVFE_RST_CTRL, 1, MCLK_RST_BIT, MCLK_RST_WID);

}


void tvafe_stop_module(struct tvafe_info_s *info)
{
    if ((info->param.port > TVIN_PORT_VGA0) && (info->param.port < TVIN_PORT_VGA7))
        tvafe_stop_vga();
    else if ((info->param.port > TVIN_PORT_COMP0) && (info->param.port < TVIN_PORT_COMP7))
        tvafe_stop_comp();
    else
        tvafe_stop_cvbs();
}

void tvafe_reset_module(enum tvin_port_e port)
{
    if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_VGA7))
        tvafe_reset_vga();
    else if ((port >= TVIN_PORT_COMP0) && (port <= TVIN_PORT_COMP7))
        tvafe_reset_comp();
    else
        tvafe_reset_cvbs();
}

void tvafe_set_fmt(struct tvafe_info_s *info)
{
    //check decoder signal status
    if((info->param.status != TVIN_SIG_STATUS_STABLE) || (info->param.fmt == TVIN_SIG_FMT_NULL))
    {
        //pr_info("TVAFE tvafe_set_fmt format abnormal \n");
        return;
    }

    pr_info("TVAFE tvafe_set_fmt:%d\n", info->param.fmt);

    if ((info->param.port >= TVIN_PORT_CVBS0) &&
        (info->param.port <= TVIN_PORT_SVIDEO7)
       )
    {
#ifndef VDIN_FIXED_FMT_TEST
        tvafe_cvd2_video_mode_confiure(info->param.fmt, info->param.port);
#endif
    }
    else
    {
        tvafe_adc_configure(info->param.fmt);
        tvafe_top_config(info->param.fmt);
    }
    //pin mux, must load pin mux again after load reg table
    tvafe_source_muxing(info);

    //cvd2 set fmt ...
}

static inline bool tvafe_is_nosig(struct tvafe_info_s *info)
{
   bool ret = 0;

    if ((info->src_type == TVAFE_SRC_TYPE_VGA) ||
        (info->src_type == TVAFE_SRC_TYPE_COMP))
        ret = tvafe_adc_no_sig();
    else if ((info->src_type == TVAFE_SRC_TYPE_CVBS) ||
        (info->src_type == TVAFE_SRC_TYPE_SVIDEO))
        ret = tvafe_cvd_no_sig();

    return ret;
}

static inline bool tvafe_fmt_chg(struct tvafe_info_s *info)
{
    bool ret = false;

    if ((info->src_type == TVAFE_SRC_TYPE_VGA) ||
        (info->src_type == TVAFE_SRC_TYPE_COMP))
        ret = tvafe_adc_fmt_chg(info);//(info->src_type, &info->param);
    else  if ((info->src_type == TVAFE_SRC_TYPE_CVBS) ||
        (info->src_type == TVAFE_SRC_TYPE_SVIDEO))
        ret = tvafe_cvd_fmt_chg();
    return ret;
}

static inline bool tvafe_pll_bad(struct tvafe_info_s *info)
{
    bool ret = true;

    if ((info->src_type == TVAFE_SRC_TYPE_VGA) ||
        (info->src_type == TVAFE_SRC_TYPE_COMP)
       )
        ret = tvafe_adc_get_pll_status();

    if (ret)
        return false;
    else
        return true;
}

static inline enum tvin_sig_fmt_e tvafe_get_fmt(struct tvafe_info_s *info)
{
    enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

    if ((info->src_type == TVAFE_SRC_TYPE_VGA) ||
        (info->src_type == TVAFE_SRC_TYPE_COMP))
        fmt = tvafe_adc_search_mode(info->src_type);
    else  if ((info->src_type == TVAFE_SRC_TYPE_CVBS) ||
        (info->src_type == TVAFE_SRC_TYPE_SVIDEO))
        fmt = tvafe_cvd2_get_format();

    return fmt;
}

inline void tvafe_init_state_handler(void)
{
    tvafe_state               = TVAFE_STATE_NOSIG;
    tvafe_state_counter       = 0;
    tvafe_exit_nosig_counter  = 0;
    tvafe_back_nosig_counter  = 0;
    tvafe_exit_stable_counter = 0;
    tvafe_back_stable_counter = 0;
    tvafe_cvd2_state_init();
}
static unsigned int wait_fmt_set_cnt = 0;
static bool rst_fg = 0;

inline void tvafe_run_state_handler(struct tvafe_info_s *info)
{
    enum tvin_sig_fmt_e fmt = 0;

    //wait format stable after set format table
    if (info->src_type == TVAFE_SRC_TYPE_COMP)
    {
        if (wait_fmt_set_cnt++ <= 10)
            return;
        else
            wait_fmt_set_cnt = 10;
    }

    switch (tvafe_state)
    {
        case TVAFE_STATE_NOSIG:
            ++tvafe_state_counter;
            if (tvafe_is_nosig(info))
            {
                tvafe_exit_nosig_counter = 0;
                if (tvafe_state_counter >= TVAFE_SURE_NOSIG)
                {
                    tvafe_state_counter = TVAFE_SURE_NOSIG;
                    info->param.status        = TVIN_SIG_STATUS_NOSIG;
                    info->param.fmt           = TVIN_SIG_FMT_NULL;
                    //pr_info("TVAFE state: nosig\n");
                }
            }
            else
            {
                ++tvafe_exit_nosig_counter;
                if (tvafe_exit_nosig_counter >= TVAFE_EXIT_NOSIG)
                {
                    tvafe_exit_nosig_counter = 0;
                    tvafe_state_counter      = 0;
                    tvafe_state              = TVAFE_STATE_UNSTABLE;
                    rst_fg = 0;
                    pr_info("TVAFE state: nosig-->unstable\n");
                }
            }
            break;
        case TVAFE_STATE_UNSTABLE:
            ++tvafe_state_counter;
            if (tvafe_is_nosig(info))
            {
                tvafe_back_stable_counter = 0;
                ++tvafe_back_nosig_counter;
                if (tvafe_back_nosig_counter >= TVAFE_BACK_NOSIG)
                {
                    tvafe_back_nosig_counter = 0;
                    tvafe_state_counter      = 0;
                    tvafe_state              = TVAFE_STATE_NOSIG;
                    info->param.status       = TVIN_SIG_STATUS_NOSIG;
                    info->param.fmt          = TVIN_SIG_FMT_NULL;
                    pr_info("TVAFE state: unstable-->nosig\n");
                }
            }
            else
            {
                tvafe_back_nosig_counter = 0;
                // if (tvafe_fmt_chg(info) || tvafe_pll_bad(info))
                if (tvafe_fmt_chg(info))
                {
                    tvafe_back_stable_counter = 0;
                    if (tvafe_state_counter >= TVAFE_SURE_UNSTABLE)
                    {
                        tvafe_state_counter = TVAFE_SURE_UNSTABLE;
                        info->param.status  = TVIN_SIG_STATUS_UNSTABLE;
                        info->param.fmt     = TVIN_SIG_FMT_NULL;
                        if ((rst_fg == 0) &&
                            (info->src_type == TVAFE_SRC_TYPE_COMP))
                        {
                            rst_fg = 1;
                            tvafe_adc_digital_reset();
                            wait_fmt_set_cnt = 0;
                            pr_info("TVAFE: unstable, reset ADC\n");
                        }
                        pr_info("TVAFE state: unstable\n");
                    }
                }
                else
                {
                    ++tvafe_back_stable_counter;
                    if (tvafe_back_stable_counter >= TVAFE_BACK_STABLE*9)
                    {   //must wait enough time for cvd signal lock
                        tvafe_back_stable_counter = 0;
                        tvafe_state_counter       = 0;
                        tvafe_state               = TVAFE_STATE_STABLE;
                        info->param.status        = TVIN_SIG_STATUS_STABLE;
                        //info->param.fmt           = tvafe_get_fmt(info);
                        fmt                       = tvafe_get_fmt(info);
                    #if 1
                        if (info->src_type == TVAFE_SRC_TYPE_COMP)
                        {
                            if ((info->param.fmt == TVIN_SIG_FMT_NULL) &&
                                (rst_fg == 0))
                            {
                                rst_fg = 1;
                                tvafe_adc_digital_reset();
                                wait_fmt_set_cnt = 0;
                                pr_info("TVAFE: NULL fmt, reset ADC\n");
                            }
                            else if (info->param.fmt != fmt)
                                wait_fmt_set_cnt = 0;
                        }
                        info->param.fmt = fmt;
                    #else
                        info->param.fmt           = tvafe_get_fmt(info);
                    #endif
                        pr_info("TVAFE state: unstable-->stable fmt:%d\n", info->param.fmt);
                    }
                }
            }
            break;
        case TVAFE_STATE_STABLE:
            // if (tvafe_is_nosig(info) || tvafe_fmt_chg(info) || tvafe_pll_bad(info))
            if (tvafe_is_nosig(info) || tvafe_fmt_chg(info))
            {
                ++tvafe_exit_stable_counter;
                if (tvafe_exit_stable_counter >= TVAFE_EXIT_STABLE)
                {
                    tvafe_exit_stable_counter = 0;
                    tvafe_state               = TVAFE_STATE_UNSTABLE;
                    rst_fg = 0;
                    pr_info("TVAFE state: stable-->unstable\n");
                }
            }
            else
            {
                tvafe_exit_stable_counter = 0;
            }
            break;
        default:
            break;
    }
}

