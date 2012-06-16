/*
  Copyright (C) 2006-2009 NXP B.V., All Rights Reserved.
  This source code and any compilation or derivative thereof is the proprietary
  information of NXP B.V. and is confidential in nature. Under no circumstances
  is this software to be  exposed to or placed under an Open Source License of
  any type without the expressed written permission of NXP B.V.
 *
 * \file          tmbslNT220x_InstanceCustom.h
 *
 *                1
 *
 * \date          %modify_time%
 *
 * \brief         Describe briefly the purpose of this file.
 *
 * REFERENCE DOCUMENTS :
 *                
 *
 * Detailed description may be added here.
 *
 * \section info Change Information
 *
*/

#ifndef _TMBSL_NT220x_INSTANCE_CUSTOM_H
#define _TMBSL_NT220x_INSTANCE_CUSTOM_H


#ifdef __cplusplus
extern "C"
{
#endif

/*============================================================================*/
/* Types and defines:                                                         */
/*============================================================================*/

/* Driver settings version definition */
#define TMBSL_NT220x_SETTINGS_CUSTOMER_NUM    0  /* SW Settings Customer Number */
#define TMBSL_NT220x_SETTINGS_PROJECT_NUM     0  /* SW Settings Project Number  */
#define TMBSL_NT220x_SETTINGS_MAJOR_VER       0  /* SW Settings Major Version   */
#define TMBSL_NT220x_SETTINGS_MINOR_VER       0  /* SW Settings Minor Version   */

/* Custom Driver Instance Parameters: (Path 0) */
#define TMBSL_NT220x_INSTANCE_CUSTOM_MASTER                                                 \
        tmNT220x_PowerStandbyWithXtalOn,                      /* Current Power state */               \
        tmNT220x_PowerStandbyWithXtalOn,                      /* Minimum Power state */               \
        0,                                                      /* RF */               \
        tmNT220x_DVBT_8MHz,                                   /* Standard mode */               \
        True,                                                   /* Master */   \
        0,                                                      /* LT_Enable */    \
        1,                                                      /* PSM_AGC1 */        \
        0,                                                      /* AGC1_6_15dB */            \

#define TMBSL_NT220x_INSTANCE_CUSTOM_MASTER_DIGITAL                                           \
        tmNT220x_PowerStandbyWithXtalOn,                      /* Current Power state */               \
        tmNT220x_PowerStandbyWithXtalOn, 											/* Minimum Power state */               \
        0,                                                      /* RF */               \
        tmNT220x_DVBT_8MHz,                                   /* Standard mode */               \
        True,                                                   /* Master */   \
        1,                                                      /* LT_Enable */    \
        0,                                                      /* PSM_AGC1 */        \
        0,                                                      /* AGC1_6_15dB */            \

#define TMBSL_NT220x_INSTANCE_CUSTOM_SLAVE                                                                 \
        tmNT220x_PowerStandbyWithXtalOn,                      /* Current Power state */               \
        tmNT220x_PowerStandbyWithXtalOn,                      /* Minimum Power state */               \
        0,                                                      /* RF */               \
        tmNT220x_DVBT_8MHz,                                   /* Standard mode */               \
        False,                                                  /* Master */   \
        0,                                                      /* LT_Enable */    \
        1,                                                      /* PSM_AGC1 */        \
        0,                                                      /* AGC1_6_15dB */            \

#define TMBSL_NT220x_INSTANCE_CUSTOM_SLAVE_DIGITAL                                                                 \
        tmNT220x_PowerStandbyWithXtalOn,                      /* Current Power state */               \
        tmNT220x_PowerStandbyWithXtalOn,                      /* Minimum Power state */               \
        0,                                                      /* RF */               \
        tmNT220x_DVBT_8MHz,                                   /* Standard mode */               \
        False,                                                  /* Master */   \
        0,                                                      /* LT_Enable */    \
        1,                                                      /* PSM_AGC1 */        \
        0,                                                      /* AGC1_6_15dB */            \

#define TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL                                                                 \
            {   /* Std_Array */                                 /* DVB-T 6MHz */               \
                3250000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_6MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_1Vpp_min_6_24dB,         /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_0_4MHz,                       /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_102dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d110_u105dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d110_u105dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Enabled,                  /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_2_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Enabled,              /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Enabled,            /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* DVB-T 7MHz */               \
                3500000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_7MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_min_8pc,                   /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_1Vpp_min_6_24dB,         /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_102dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d110_u105dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d110_u105dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Enabled,                  /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_2_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Enabled,              /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Enabled,            /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* DVB-T 8MHz */               \
                4000000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_8MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_1Vpp_min_6_24dB,         /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_102dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d110_u105dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d110_u105dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Enabled,                  /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_2_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Enabled,              /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Enabled,            /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* QAM 6MHz */               \
                3600000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_6MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_min_8pc,                   /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_1Vpp_min_6_24dB,         /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d110_u105dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d110_u105dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				True											/* AGC1_freeze */ \
            },               \
            {                                                   /* QAM 8MHz */               \
                5000000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_9MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_min_8pc,                   /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,         /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_0_85MHz,                      /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d110_u105dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d110_u105dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				True											/* AGC1_freeze */ \
            },               \
            {                                                   /* ISDBT 6MHz */               \
                3250000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_6MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_6Vpp_min_10_3_19_7dB,  /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_0_4MHz,                       /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_102dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d110_u105dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d110_u105dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Enabled,                  /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_2_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Enabled,              /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Enabled,            /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* ATSC 6MHz */               \
                3250000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_6MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_6Vpp_min_10_3_19_7dB,  /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_0_4MHz,                       /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d100_u94dBuV,           /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_104dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_104dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d112_u107dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d112_u107dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Enabled,                  /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_3_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Enabled,              /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Enabled,            /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* DMB-T 8MHz */               \
                4000000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_8MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_1Vpp_min_6_24dB,         /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_102dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d110_u105dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d110_u105dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Enabled,                  /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_2_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Enabled,              /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Enabled,            /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \

#define  TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG                                          \
            {                                                   /* Analog M/N */               \
                5400000,                                        /* IF */               \
                1750000,                                        /* CF_Offset */               \
                tmNT220x_LPF_6MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Disabled,                   /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Enabled,           /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x01,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* Analog B */               \
                6400000,                                        /* IF */               \
                2250000,                                        /* CF_Offset */               \
                tmNT220x_LPF_7MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Disabled,                   /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band*/               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Enabled,           /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x01,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* Analog G/H */               \
                6750000,                                        /* IF */               \
                2750000,                                        /* CF_Offset */               \
                tmNT220x_LPF_8MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Disabled,                   /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Enabled,           /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x01,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* Analog I */               \
                7250000,                                        /* IF */               \
                2750000,                                        /* CF_Offset */               \
                tmNT220x_LPF_8MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Disabled,                   /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Enabled,           /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x01,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* Analog D/K */               \
                6850000,                                        /* IF */               \
                2750000,                                        /* CF_Offset */               \
                tmNT220x_LPF_8MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Disabled,                   /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Enabled,           /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x01,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False   										/* AGC1_freeze */ \
            },               \
            {                                                   /* Analog L */               \
                6750000,                                        /* IF */               \
                2750000,                                        /* CF_Offset */               \
                tmNT220x_LPF_8MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Disabled,                   /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Enabled,           /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x01,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* Analog L' */               \
                1250000,                                        /* IF */               \
                -2750000,                                       /* CF_Offset */               \
                tmNT220x_LPF_8MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Disabled,                   /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x01,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* Analog FM Radio */               \
                1250000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_1_5MHz,                          /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_0_85MHz,                      /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x02,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* Blind Scanning copy of PAL-I */               \
                7250000,                                        /* IF */               \
                2750000,                                        /* CF_Offset */               \
                tmNT220x_LPF_8MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_0_7Vpp_min_9_21dB,       /* IF_Gain */               \
                tmNT220x_IF_Notch_Disabled,                   /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Disabled,                   /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_96dBuV,              /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d105_u100dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d105_u100dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Enabled,           /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Disabled,                 /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_0_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Disabled,             /* AGC5_Atten_3dB */               \
                0x01,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Disabled,           /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Frozen,                     /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            },               \
            {                                                   /* ScanXpress */               \
                5000000,                                        /* IF */               \
                0,                                              /* CF_Offset */               \
                tmNT220x_LPF_9MHz,                            /* LPF */               \
                tmNT220x_LPFOffset_0pc,                       /* LPF_Offset */               \
                tmNT220x_IF_AGC_Gain_1Vpp_min_6_24dB,         /* IF_Gain */               \
                tmNT220x_IF_Notch_Enabled,                    /* IF_Notch */               \
                tmNT220x_IF_HPF_Disabled,                     /* IF_HPF */               \
                tmNT220x_DC_Notch_Enabled,                    /* DC_Notch */               \
                tmNT220x_AGC1_LNA_TOP_d95_u89dBuV,            /* AGC1_LNA_TOP */               \
                tmNT220x_AGC2_RF_Attenuator_TOP_d90_u84dBuV,  /* AGC2_RF_Attenuator_TOP */               \
                tmNT220x_AGC3_RF_AGC_TOP_100dBuV,             /* AGC3_RF_AGC_TOP_Low_band */               \
                tmNT220x_AGC3_RF_AGC_TOP_102dBuV,             /* AGC3_RF_AGC_TOP_High_band */               \
                tmNT220x_AGC4_IR_Mixer_TOP_d110_u105dBuV,     /* AGC4_IR_Mixer_TOP */               \
                tmNT220x_AGC5_IF_AGC_TOP_d110_u105dBuV,       /* AGC5_IF_AGC_TOP */               \
                tmNT220x_AGC5_Detector_HPF_Disabled,          /* AGC5_Detector_HPF */               \
                tmNT220x_AGC3_Adapt_Enabled,                  /* AGC3_Adapt */               \
                tmNT220x_AGC3_Adapt_TOP_2_Step,        /* AGC3_Adapt_TOP */               \
                tmNT220x_AGC5_Atten_3dB_Enabled,              /* AGC5_Atten_3dB */               \
                0x0e,                                           /* GSK : settings V2.0.0  */               \
                tmNT220x_H3H5_VHF_Filter6_Enabled,            /* H3H5_VHF_Filter6 */               \
                tmNT220x_LPF_Gain_Free,                       /* LPF_Gain */               \
				False											/* AGC1_freeze */ \
            }
/* Custom Driver Instance Parameters: (Path 1) */

/******************************************************************/
/* Mode selection for PATH0                                       */
/******************************************************************/

#ifdef TMBSL_NT220HN

#define TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH0 TMBSL_NT220x_INSTANCE_CUSTOM_MASTER
#define TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0 TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL
#define TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH0 TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG

#else

#define TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH0 TMBSL_NT220x_INSTANCE_CUSTOM_MASTER_DIGITAL
#define TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0 TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL

#endif

/******************************************************************/
/* Mode selection for PATH1                                       */
/******************************************************************/

#ifdef TMBSL_NT220HN

#define TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH1 TMBSL_NT220x_INSTANCE_CUSTOM_SLAVE
#define TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH1 TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL
#define TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH1 TMBSL_NT220x_INSTANCE_CUSTOM_STD_ANALOG

#else

#define TMBSL_NT220x_INSTANCE_CUSTOM_MODE_PATH1 TMBSL_NT220x_INSTANCE_CUSTOM_SLAVE_DIGITAL

#define TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH1 TMBSL_NT220x_INSTANCE_CUSTOM_STD_DIGITAL

#endif

/******************************************************************/
/* End of Mode selection                                          */
/******************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _TMBSL_NT220x_INSTANCE_CUSTOM_H */


