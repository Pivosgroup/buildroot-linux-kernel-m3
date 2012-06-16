/*
 * Amlogic Apollo
 * frame buffer driver
 *
 * Copyright (C) 2009 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 */

#ifndef TVREGS_H
#define TVREGS_H

#define MREG_END_MARKER 0xffff


	#define VIDEO_CLOCK_HD_25	0x00101529
	#define VIDEO_CLOCK_SD_25	0x00500a6c
	#define VIDEO_CLOCK_HD_24	0x00140863
	#define VIDEO_CLOCK_SD_24	0x0050042d


typedef struct reg_s {
    uint reg;
    uint val;
} reg_t;

typedef struct tvinfo_s {
    uint xres;
    uint yres;
    const char *id;
} tvinfo_t;
/*
24M
25M
*/
static const  reg_t tvreg_vclk_sd[]={
	{HHI_VID_PLL_CNTL,VIDEO_CLOCK_SD_24},//SD.24
    {HHI_VID_PLL_CNTL,VIDEO_CLOCK_SD_25},//SD,25  
};

static const  reg_t tvreg_vclk_hd[]={
    {HHI_VID_PLL_CNTL,VIDEO_CLOCK_HD_24},//HD,24
    {HHI_VID_PLL_CNTL,VIDEO_CLOCK_HD_25},//HD,25    
};

static const  reg_t tvregs_720p[] = {
    {VENC_VDAC_SETTING,          0xff,  },
#ifndef CONFIG_AM_VIDEO2
    {HHI_VID_CLK_CNTL,           0x0,},
#endif
    {HHI_VID_PLL_CNTL2,          0x00040003},
    {HHI_VID_PLL_CNTL3,          0x40e8},
    {HHI_HDMI_AFC_CNTL,          0x8c0000c3},
    {HHI_VID_PLL_CNTL,           0x0001043e,},
    {HHI_VID_DIVIDER_CNTL,       0x00010843,},
#ifndef CONFIG_AM_VIDEO2
    {HHI_VID_CLK_DIV,            0x100},

    {HHI_VID_CLK_CNTL,           0x80000,},
    {HHI_VID_CLK_CNTL,           0x88001,},
    {HHI_VID_CLK_CNTL,           0x80003,},

    {HHI_VIID_CLK_DIV,           0x00000101,},
#endif

    {ENCP_VIDEO_FILT_CTRL,       0x0052,},
    {VENC_DVI_SETTING,           0x2029,},
    {ENCP_VIDEO_MODE,            0x0040,},
    {ENCP_VIDEO_MODE_ADV,        0x0019,},
    {ENCP_VIDEO_YFP1_HTIME,      648,   },
    {ENCP_VIDEO_YFP2_HTIME,      3207,  },
    {ENCP_VIDEO_MAX_PXCNT,       3299,  },
    {ENCP_VIDEO_HSPULS_BEGIN,    80,    },
    {ENCP_VIDEO_HSPULS_END,      240,   },
    {ENCP_VIDEO_HSPULS_SWITCH,   80,    },
    {ENCP_VIDEO_VSPULS_BEGIN,    688,   },
    {ENCP_VIDEO_VSPULS_END,      3248,  },
    {ENCP_VIDEO_VSPULS_BLINE,    4,     },
    {ENCP_VIDEO_VSPULS_ELINE,    8,     },
    {ENCP_VIDEO_EQPULS_BLINE,    4,     },
    {ENCP_VIDEO_EQPULS_ELINE,    8,     },
    {ENCP_VIDEO_HAVON_BEGIN,     648,   },
    {ENCP_VIDEO_HAVON_END,       3207,  },
    {ENCP_VIDEO_VAVON_BLINE,     29,    },
    {ENCP_VIDEO_VAVON_ELINE,     748,   },
    {ENCP_VIDEO_HSO_BEGIN,       256    },
    {ENCP_VIDEO_HSO_END,         168,   },
    {ENCP_VIDEO_VSO_BEGIN,       168,   },
    {ENCP_VIDEO_VSO_END,         256,   },
    {ENCP_VIDEO_VSO_BLINE,       0,     },
    {ENCP_VIDEO_VSO_ELINE,       5,     },
    {ENCP_VIDEO_MAX_LNCNT,       749,   },
    {VENC_VIDEO_PROG_MODE,       0x100, },
    {VENC_SYNC_ROUTE,            0,     },
    {VENC_INTCTRL,               0x200, },
    {VFIFO2VD_CTL,               0,     },
    {VENC_VDAC_SETTING,          0,     },
    {VENC_UPSAMPLE_CTRL0,        0x9061,},
    {VENC_UPSAMPLE_CTRL1,        0xa061,},
    {VENC_UPSAMPLE_CTRL2,        0xb061,},
    {VENC_VDAC_DACSEL0,          0x0001,},
    {VENC_VDAC_DACSEL1,          0x0001,},
    {VENC_VDAC_DACSEL2,          0x0001,},
    {VENC_VDAC_DACSEL3,          0x0001,},
    {VENC_VDAC_DACSEL4,          0x0001,},
    {VENC_VDAC_DACSEL5,          0x0001,},
#ifndef CONFIG_AM_VIDEO2
    {VPU_VIU_VENC_MUX_CTRL,      0x000a,},
#endif
    {VENC_VDAC_FIFO_CTRL,        0x1000,},
    {ENCP_DACSEL_0,              0x3210,},
    {ENCP_DACSEL_1,              0x0054,},
    {ENCP_VIDEO_EN,              1,     },
    {ENCI_VIDEO_EN,              0,     },
    {MREG_END_MARKER,            0      }
};

static const  reg_t tvregs_720p_50hz[] = {
    {VENC_VDAC_SETTING,          0xff,  },
	{HHI_VID_CLK_DIV,			 1		},
    {HHI_VID_CLK_CNTL,        	 0x0421,},
    {ENCP_VIDEO_FILT_CTRL,       0x0052,},

    {VENC_DVI_SETTING,           0x202d,},
    {ENCP_VIDEO_MAX_PXCNT,       3959,  },
    {ENCP_VIDEO_MAX_LNCNT,       749,   },

     //analog vidoe position in horizontal
    {ENCP_VIDEO_HSPULS_BEGIN,    80,    },
    {ENCP_VIDEO_HSPULS_END,      240,   },
    {ENCP_VIDEO_HSPULS_SWITCH,   80,    },

    //DE position in horizontal
    {ENCP_VIDEO_HAVON_BEGIN,     648,   },
    {ENCP_VIDEO_HAVON_END,       3207,  },

    //ditital hsync positon in horizontal
    {ENCP_VIDEO_HSO_BEGIN,       128 ,},
    {ENCP_VIDEO_HSO_END,         208 , },

    /* vsync horizontal timing */
    {ENCP_VIDEO_VSPULS_BEGIN,    688,   },
    {ENCP_VIDEO_VSPULS_END,      3248,  },

    /* vertical timing settings */
    {ENCP_VIDEO_VSPULS_BLINE,    4,     },
    {ENCP_VIDEO_VSPULS_ELINE,    8,     },
    {ENCP_VIDEO_EQPULS_BLINE,    4,     },
    {ENCP_VIDEO_EQPULS_ELINE,    8,     },

    //DE position in vertical
    {ENCP_VIDEO_VAVON_BLINE,     29,    },
    {ENCP_VIDEO_VAVON_ELINE,     748,   },

    //adjust the vsync start point and end point
    {ENCP_VIDEO_VSO_BEGIN,       128,},  //168,   },
    {ENCP_VIDEO_VSO_END,         128, },  //256,   },

    //adjust the vsync start line and end line
    {ENCP_VIDEO_VSO_BLINE,       0,     },
    {ENCP_VIDEO_VSO_ELINE,       5,     },

    /* filter & misc settings */
    {ENCP_VIDEO_YFP1_HTIME,      648,   },
    {ENCP_VIDEO_YFP2_HTIME,      3207,  },


    {VENC_VIDEO_PROG_MODE,       0x100, },
    {ENCP_VIDEO_MODE,            0x0040,},  //Enable Hsync and equalization pulse switch in center
    {ENCP_VIDEO_MODE_ADV,        0x0019,},//bit6:swap PbPr; bit4:YPBPR gain as HDTV type;
                                                 //bit3:Data input from VFIFO;bit[2}0]:repreat pixel a time

     {ENCP_VIDEO_SYNC_MODE,       0x407,  },//Video input Synchronization mode ( bit[7:0] -- 4:Slave mode, 7:Master mode)
                                                 //bit[15:6] -- adjust the vsync vertical position
    {ENCP_VIDEO_YC_DLY,          0,     },      //Y/Cb/Cr delay
    {VENC_SYNC_ROUTE,            0,     },
    {VENC_INTCTRL,               0x200, },
    {VFIFO2VD_CTL,               0,     },
    {VENC_VDAC_SETTING,          0,     },
    {ENCP_VIDEO_EN,              1,     },
    {ENCI_VIDEO_EN,              0,     },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_480i[] = {
    {VENC_VDAC_SETTING,          0xff,  },
    {HHI_VID_CLK_DIV,            4,     },
    {HHI_VID_CLK_CNTL,        	 0x05a1,},
    {ENCI_CFILT_CTRL,            0x0810,},
    {VENC_DVI_SETTING,           0,     },
    {ENCI_SYNC_HSO_BEGIN,        5,     },
    {ENCI_SYNC_HSO_END,          129,   },
    {ENCI_SYNC_VSO_EVNLN,        0x0003 },
    {ENCI_SYNC_VSO_ODDLN,        0x0104 },
    {ENCI_MACV_MAX_AMP,          0x810b },
    {VENC_VIDEO_PROG_MODE,       0xff   },
    {ENCI_VIDEO_MODE,            0x08   },
    {ENCI_VIDEO_MODE_ADV,        0x26,  },
    {ENCI_VIDEO_SCH,             0x20,  },
    {ENCI_SYNC_MODE,             0x07,  },
    {ENCI_YC_DELAY,              0x341, },
    {VFIFO2VD_PIXEL_START,       0x00f3 },
    {VFIFO2VD_PIXEL_END,         0x0693 },
    {VFIFO2VD_LINE_TOP_START,    0x0012 },
    {VFIFO2VD_LINE_TOP_END,      0x0102 },
    {VFIFO2VD_LINE_BOT_START,    0x0013 },
    {VFIFO2VD_LINE_BOT_END,      0x0103 },
    {VFIFO2VD_CTL,              0x4e01, },
    {ENCI_DBG_PX_RST,            0,     },
    {VENC_INTCTRL,               0x2,   },
    {VENC_VDAC_SETTING,          0,     },
    {ENCP_VIDEO_EN,              0,     },
    {ENCI_VIDEO_EN,              1,     },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_480cvbs[] = {
    {VENC_VDAC_SETTING,          0xff,  },
    {HHI_VID_CLK_DIV,            4,     },
    {HHI_VID_CLK_CNTL,        	 0x05a1,},
    {ENCI_CFILT_CTRL,            0x0810,},
    {VENC_DVI_SETTING,           0,     },
    {ENCI_SYNC_HSO_BEGIN,        5,     },
    {ENCI_SYNC_HSO_END,          129,   },
    {ENCI_SYNC_VSO_EVNLN,        0x0003 },
    {ENCI_SYNC_VSO_ODDLN,        0x0104 },
    {ENCI_MACV_MAX_AMP,          0x810b },
    {VENC_VIDEO_PROG_MODE,       0xff   },
    {ENCI_VIDEO_MODE,            0x08   },
    {ENCI_VIDEO_MODE_ADV,        0x26,  },
    {ENCI_VIDEO_SCH,             0x20,  },
    {ENCI_SYNC_MODE,             0x07,  },
    {ENCI_YC_DELAY,              0x341, },
    {VFIFO2VD_PIXEL_START,       0x00f3 },
    {VFIFO2VD_PIXEL_END,         0x0693 },
    {VFIFO2VD_LINE_TOP_START,    0x0012 },
    {VFIFO2VD_LINE_TOP_END,      0x0102 },
    {VFIFO2VD_LINE_BOT_START,    0x0013 },
    {VFIFO2VD_LINE_BOT_END,      0x0103 },
    {VFIFO2VD_CTL,              0x4e01, },
    {ENCI_DBG_PX_RST,            0,     },
    {VENC_INTCTRL,               0x2,   },
    {VENC_VDAC_SETTING,          0,     },
    {ENCP_VIDEO_EN,              0,     },
    {ENCI_VIDEO_EN,              1,     },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_480p[] = {
    {VENC_VDAC_SETTING,          0xff,  },
#ifndef CONFIG_AM_VIDEO2
    {HHI_VID_CLK_CNTL,           0x0,},
#endif
    {HHI_VID_PLL_CNTL2,          0x00040003},
    {HHI_VID_PLL_CNTL3,          0x40e8},
    {HHI_HDMI_AFC_CNTL,          0x8c0000c3},
    {HHI_VID_PLL_CNTL,           0x0001042d,},
    {HHI_VID_DIVIDER_CNTL,       0x00010843,},
#ifndef CONFIG_AM_VIDEO2
    {HHI_VID_CLK_DIV,            0x100},

    {HHI_VID_CLK_CNTL,           0x88001,},
    {HHI_VID_CLK_CNTL,           0x80001,},

    {HHI_VID_CLK_DIV,            0x01000100,},
#endif
    {ENCP_VIDEO_FILT_CTRL,       0x2052,},
    {VENC_DVI_SETTING,           0x21,  },
    {ENCP_VIDEO_MODE,            0,     },
    {ENCP_VIDEO_MODE_ADV,        9,     },
    {ENCP_VIDEO_YFP1_HTIME,      244,   },
    {ENCP_VIDEO_YFP2_HTIME,      1630,  },
    {ENCP_VIDEO_YC_DLY,          0,     },
    {ENCP_VIDEO_MAX_PXCNT,       1715,  },
    {ENCP_VIDEO_MAX_LNCNT,       524,   },
    {ENCP_VIDEO_HSPULS_BEGIN,    0x22,  },
    {ENCP_VIDEO_HSPULS_END,      0xa0,  },
    {ENCP_VIDEO_HSPULS_SWITCH,   88,    },
    {ENCP_VIDEO_VSPULS_BEGIN,    0,     },
    {ENCP_VIDEO_VSPULS_END,      1589   },
    {ENCP_VIDEO_VSPULS_BLINE,    0,     },
    {ENCP_VIDEO_VSPULS_ELINE,    5,     },
    {ENCP_VIDEO_HAVON_BEGIN,     249,   },
    {ENCP_VIDEO_HAVON_END,       1689,  },
    {ENCP_VIDEO_VAVON_BLINE,     42,    },
    {ENCP_VIDEO_VAVON_ELINE,     521,   },
    {ENCP_VIDEO_SYNC_MODE,       0x07,  },
    {VENC_VIDEO_PROG_MODE,       0x0,   },
    {VENC_VIDEO_EXSRC,           0x0,   },
    {ENCP_VIDEO_HSO_BEGIN,       0x3,   },
    {ENCP_VIDEO_HSO_END,         0x5,   },
    {ENCP_VIDEO_VSO_BEGIN,       0x3,   },
    {ENCP_VIDEO_VSO_END,         0x5,   },
    {ENCP_VIDEO_VSO_BLINE,       0,     },  //added by JZD. Switch Panel to 480p first time, movie video flicks if not set this to 0
    {VENC_SYNC_ROUTE,            0,     },
    {VENC_INTCTRL,               0x200, },
    {VFIFO2VD_CTL,               0,     },
    {VENC_VDAC_SETTING,          0,     },
    {VENC_UPSAMPLE_CTRL0,        0x9061,},
    {VENC_UPSAMPLE_CTRL1,        0xa061,},
    {VENC_UPSAMPLE_CTRL2,        0xb061,},
    {VENC_VDAC_DACSEL0,          0xf003,},
    {VENC_VDAC_DACSEL1,          0xf003,},
    {VENC_VDAC_DACSEL2,          0xf003,},
    {VENC_VDAC_DACSEL3,          0xf003,},
    {VENC_VDAC_DACSEL4,          0xf003,},
    {VENC_VDAC_DACSEL5,          0xf003,},
#ifndef CONFIG_AM_VIDEO2
    {VPU_VIU_VENC_MUX_CTRL,      0x000a,},
#endif
    {VENC_VDAC_FIFO_CTRL,        0x1fc0,},
    {ENCP_DACSEL_0,              0x3210,},
    {ENCP_DACSEL_1,              0x0054,},
    {ENCI_VIDEO_EN,              0      },
    {ENCP_VIDEO_EN,              1      },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_576i[] = {
    {VENC_VDAC_SETTING,          0xff,  },
//	{VCLK_SD},
	{HHI_VID_CLK_DIV,			 4,		},
    {HHI_VID_CLK_CNTL,        	 0x05a1,},
    {ENCI_CFILT_CTRL,            0x0810,},
    {VENC_DVI_SETTING,           0,     },
    {ENCI_SYNC_HSO_BEGIN,        3,     },
    {ENCI_SYNC_HSO_END,          129,   },
    {ENCI_SYNC_VSO_EVNLN,        0x0003 },
    {ENCI_SYNC_VSO_ODDLN,        0x0104 },
    {ENCI_MACV_MAX_AMP,          0x8107 },
    {VENC_VIDEO_PROG_MODE,       0xff   },
    {ENCI_VIDEO_MODE,            0x13   },
    {ENCI_VIDEO_MODE_ADV,        0x26,  },
    {ENCI_VIDEO_SCH,             0x28,  },
    {ENCI_SYNC_MODE,             0x07,  },
    {ENCI_YC_DELAY,              0x341, },
    {VFIFO2VD_PIXEL_START,       0x010b },
    {VFIFO2VD_PIXEL_END,             0x06ab },
    {VFIFO2VD_LINE_TOP_START,    0x0016 },
    {VFIFO2VD_LINE_TOP_END,      0x0136 },
    {VFIFO2VD_LINE_BOT_START,    0x0017 },
    {VFIFO2VD_LINE_BOT_END,      0x0137 },
    {VFIFO2VD_CTL,              0x4e01, },
    {ENCI_DBG_PX_RST,            0,     },
    {VENC_INTCTRL,               0x2,   },
    {VENC_VDAC_SETTING,          0,     },
    {ENCP_VIDEO_EN,              0,     },
    {ENCI_VIDEO_EN,              1,     },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_576cvbs[] = {
    {VENC_VDAC_SETTING,          0xff,  },
//	{VCLK_SD},
	{HHI_VID_CLK_DIV,			 4,		},
    {HHI_VID_CLK_CNTL,        	 0x05a1,},
    {ENCI_CFILT_CTRL,            0x0810,},
    {VENC_DVI_SETTING,           0,     },
    {ENCI_SYNC_HSO_BEGIN,        3,     },
    {ENCI_SYNC_HSO_END,          129,   },
    {ENCI_SYNC_VSO_EVNLN,        0x0003 },
    {ENCI_SYNC_VSO_ODDLN,        0x0104 },
    {ENCI_MACV_MAX_AMP,          0x8107 },
    {VENC_VIDEO_PROG_MODE,       0xff   },
    {ENCI_VIDEO_MODE,            0x13   },
    {ENCI_VIDEO_MODE_ADV,        0x26,  },
    {ENCI_VIDEO_SCH,             0x28,  },
    {ENCI_SYNC_MODE,             0x07,  },
    {ENCI_YC_DELAY,              0x341, },
    {VFIFO2VD_PIXEL_START,       0x010b },
    {VFIFO2VD_PIXEL_END,             0x06ab },
    {VFIFO2VD_LINE_TOP_START,    0x0016 },
    {VFIFO2VD_LINE_TOP_END,      0x0136 },
    {VFIFO2VD_LINE_BOT_START,    0x0017 },
    {VFIFO2VD_LINE_BOT_END,      0x0137 },
    {VFIFO2VD_CTL,              0x4e01, },
    {ENCI_DBG_PX_RST,            0,     },
    {VENC_INTCTRL,               0x2,   },
    {VENC_VDAC_SETTING,          0,     },
    {ENCP_VIDEO_EN,              0,     },
    {ENCI_VIDEO_EN,              1,     },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_576p[] = {
    {VENC_VDAC_SETTING,          0xff,  },
//	{VCLK_SD},
	{HHI_VID_CLK_DIV,			 4,		},
    {HHI_VID_CLK_CNTL,        	 0x0561,},
    {ENCP_VIDEO_FILT_CTRL,       0x52,  },
    {VENC_DVI_SETTING,           0x21,  },
    {ENCP_VIDEO_MODE,            0,     },
    {ENCP_VIDEO_MODE_ADV,        9,     },
    {ENCP_VIDEO_YFP1_HTIME,      235,   },
    {ENCP_VIDEO_YFP2_HTIME,      1674,  },
    {ENCP_VIDEO_YC_DLY,          0xf,   },
    {ENCP_VIDEO_MAX_PXCNT,       1727,  },
    {ENCP_VIDEO_MAX_LNCNT,       624,   },
    {ENCP_VIDEO_HSPULS_BEGIN,    0,     },
    {ENCP_VIDEO_HSPULS_END,      0x80,  },
    {ENCP_VIDEO_VSPULS_BEGIN,    0,     },
    {ENCP_VIDEO_VSPULS_END,      1599   },
    {ENCP_VIDEO_VSPULS_BLINE,    0,     },
    {ENCP_VIDEO_VSPULS_ELINE,    4,     },
    {ENCP_VIDEO_HAVON_BEGIN,     235,   },
    {ENCP_VIDEO_HAVON_END,       1674,  },
    {ENCP_VIDEO_VAVON_BLINE,     44,    },
    {ENCP_VIDEO_VAVON_ELINE,     619,   },
    {ENCP_VIDEO_SYNC_MODE,       0x07,  },
    {VENC_VIDEO_PROG_MODE,       0x0,   },
    {VENC_VIDEO_EXSRC,           0x0,   },
    {ENCP_VIDEO_HSO_BEGIN,       0x80,  },
    {ENCP_VIDEO_HSO_END,         0x0,   },
    {ENCP_VIDEO_VSO_BEGIN,       0x0,   },
    {ENCP_VIDEO_VSO_END,         0x5,   },
    {VENC_SYNC_ROUTE,            0,     },
    {VENC_INTCTRL,               0x200, },
    {VFIFO2VD_CTL,               0,     },
    {VENC_VDAC_SETTING,          0,     },
    {ENCI_VIDEO_EN,              0      },
    {ENCP_VIDEO_EN,              1      },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_1080i[] = {
    {VENC_VDAC_SETTING,          0xff,  },
//	{VCLK_HD},
	{HHI_VID_CLK_DIV,			 1		},
    {HHI_VID_CLK_CNTL,        	 0x0421,},
    {ENCP_VIDEO_FILT_CTRL,       0x0052,},
    {VENC_DVI_SETTING,           0x2029,},
    {ENCP_VIDEO_MAX_PXCNT,       4399,  },
    {ENCP_VIDEO_MAX_LNCNT,       1124,  },
    {ENCP_VIDEO_HSPULS_BEGIN,    88,    },
    {ENCP_VIDEO_HSPULS_END,      264,   },
    {ENCP_VIDEO_HSPULS_SWITCH,   88,    },
    {ENCP_VIDEO_HAVON_BEGIN,     516,   },
    {ENCP_VIDEO_HAVON_END,       4355,  },
    {ENCP_VIDEO_HSO_BEGIN,       264,   },
    {ENCP_VIDEO_HSO_END,         176,   },
    {ENCP_VIDEO_EQPULS_BEGIN,    2288,  },
    {ENCP_VIDEO_EQPULS_END,      2464,  },
    {ENCP_VIDEO_VSPULS_BEGIN,    440,   },
    {ENCP_VIDEO_VSPULS_END,      2200,  },
    {ENCP_VIDEO_VSPULS_BLINE,    0,     },
    {ENCP_VIDEO_VSPULS_ELINE,    4,     },
    {ENCP_VIDEO_EQPULS_BLINE,    0,     },
    {ENCP_VIDEO_EQPULS_ELINE,    4,     },
    {ENCP_VIDEO_VAVON_BLINE,     20,    },
    {ENCP_VIDEO_VAVON_ELINE,     559,   },
    {ENCP_VIDEO_VSO_BEGIN,       88,    },
    {ENCP_VIDEO_VSO_END,         88,    },
    {ENCP_VIDEO_VSO_BLINE,       0,     },
    {ENCP_VIDEO_VSO_ELINE,       5,     },
    {ENCP_VIDEO_YFP1_HTIME,      516,   },
    {ENCP_VIDEO_YFP2_HTIME,      4355,  },
    {VENC_VIDEO_PROG_MODE,       0x100, },
    {ENCP_VIDEO_OFLD_VOAV_OFST,  0x11   },
    {ENCP_VIDEO_MODE,            0x1ffc,},
    {ENCP_VIDEO_MODE_ADV,        0x0019,},
    {ENCP_VIDEO_SYNC_MODE,       0x207, },
    {VENC_SYNC_ROUTE,            0,     },
    {VENC_INTCTRL,               0x200, },
    {VFIFO2VD_CTL,               0,     },
    {VENC_VDAC_SETTING,          0,     },
    {ENCI_VIDEO_EN,              0,     },
    {ENCP_VIDEO_EN,              1,     },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_1080i_50hz[] = {
    {VENC_VDAC_SETTING,          0xff,  },
//	{VCLK_HD},
	{HHI_VID_CLK_DIV,			 1		},
    {HHI_VID_CLK_CNTL,        	 0x0421,},
    {ENCP_VIDEO_FILT_CTRL,       0x0052,},

    {VENC_DVI_SETTING,           0x202d,},
    {ENCP_VIDEO_MAX_PXCNT,       5279,  },
    {ENCP_VIDEO_MAX_LNCNT,       1124,  },

    //analog vidoe position in horizontal
    {ENCP_VIDEO_HSPULS_BEGIN,    88,    },
    {ENCP_VIDEO_HSPULS_END,      264,   },
    {ENCP_VIDEO_HSPULS_SWITCH,   88,    },

    //DE position in horizontal
    {ENCP_VIDEO_HAVON_BEGIN,     526,   },
    {ENCP_VIDEO_HAVON_END,       4365,  },

    //ditital hsync positon in horizontal
    {ENCP_VIDEO_HSO_BEGIN,       142,   },
    {ENCP_VIDEO_HSO_END,         230,   },

    /* vsync horizontal timing */
    {ENCP_VIDEO_EQPULS_BEGIN,    2728,  },
    {ENCP_VIDEO_EQPULS_END,      2904,  },
    {ENCP_VIDEO_VSPULS_BEGIN,    440,   },
    {ENCP_VIDEO_VSPULS_END,      2200,  },

    {ENCP_VIDEO_VSPULS_BLINE,    0,     },
    {ENCP_VIDEO_VSPULS_ELINE,    4,     },
    {ENCP_VIDEO_EQPULS_BLINE,    0,     },
    {ENCP_VIDEO_EQPULS_ELINE,    4,     },

    //DE position in vertical
    {ENCP_VIDEO_VAVON_BLINE,     20,    },
    {ENCP_VIDEO_VAVON_ELINE,     559,   },

    //adjust vsync start point and end point
    {ENCP_VIDEO_VSO_BEGIN,       142,    },
    {ENCP_VIDEO_VSO_END,         142,    },

    //adjust the vsync start line and end line
    {ENCP_VIDEO_VSO_BLINE,       0,     },
    {ENCP_VIDEO_VSO_ELINE,       5,     },

    /* filter & misc settings */
    {ENCP_VIDEO_YFP1_HTIME,      526,   },
    {ENCP_VIDEO_YFP2_HTIME,      4365,  },

    {VENC_VIDEO_PROG_MODE,       0x100, },  // Select clk108 as DAC clock, progressive mode
    {ENCP_VIDEO_OFLD_VOAV_OFST,  0x11   },//bit[15:12]: Odd field VSO  offset begin,
                                                        //bit[11:8]: Odd field VSO  offset end,
                                                        //bit[7:4]: Odd field VAVON offset begin,
                                                        //bit[3:0]: Odd field VAVON offset end,
    {ENCP_VIDEO_MODE,            0x1ffc,},//Enable Hsync and equalization pulse switch in center
    {ENCP_VIDEO_MODE_ADV,        0x0019,}, //bit6:swap PbPr; bit4:YPBPR gain as HDTV type;
                                                 //bit3:Data input from VFIFO;bit[2}0]:repreat pixel a time
    {ENCP_VIDEO_SYNC_MODE,       0x7, }, //bit[15:8] -- adjust the vsync vertical position
    {VENC_SYNC_ROUTE,            0,     },
    {VENC_INTCTRL,               0x200, },
    {VFIFO2VD_CTL,               0,     },
    {VENC_VDAC_SETTING,          0,     },
    {ENCI_VIDEO_EN,              0,     },
    {ENCP_VIDEO_EN,              1,     },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_1080p[] = {
    {VENC_VDAC_SETTING,          0xff,  },
//	{VCLK_HD},
#ifndef CONFIG_AM_VIDEO2
	{HHI_VID_CLK_DIV,			 1		},
    {HHI_VID_CLK_CNTL,        	 0x0421,},
#endif
    {ENCP_VIDEO_FILT_CTRL,       0x1052,},
    {VENC_DVI_SETTING,           0x0001,},
    {ENCP_VIDEO_MODE,            0x0040,},
    {ENCP_VIDEO_MODE_ADV,        0x0018,},
    {ENCP_VIDEO_YFP1_HTIME,      140,   },
    {ENCP_VIDEO_YFP2_HTIME,      2060,  },
    {ENCP_VIDEO_MAX_PXCNT,       2199,  },
    {ENCP_VIDEO_HSPULS_BEGIN,    2156,  },//1980
    {ENCP_VIDEO_HSPULS_END,      44,    },
    {ENCP_VIDEO_HSPULS_SWITCH,   44,    },
    {ENCP_VIDEO_VSPULS_BEGIN,    140,   },
    {ENCP_VIDEO_VSPULS_END,      2059,  },
    {ENCP_VIDEO_VSPULS_BLINE,    0,     },
    {ENCP_VIDEO_VSPULS_ELINE,    4,     },//35
    {ENCP_VIDEO_HAVON_BEGIN,     148,   },
    {ENCP_VIDEO_HAVON_END,       2067,  },
    {ENCP_VIDEO_VAVON_BLINE,     41,    },
    {ENCP_VIDEO_VAVON_ELINE,     1120,  },
    {ENCP_VIDEO_HSO_BEGIN,       44,    },
    {ENCP_VIDEO_HSO_END,         2156,  },
    {ENCP_VIDEO_VSO_BEGIN,       2100,  },
    {ENCP_VIDEO_VSO_END,         2164,  },
    {ENCP_VIDEO_VSO_BLINE,       0,     },
    {ENCP_VIDEO_VSO_ELINE,       5,     },
    {ENCP_VIDEO_MAX_LNCNT,       1124,  },
#ifndef CONFIG_AM_VIDEO2
    {VPU_VIU_VENC_MUX_CTRL,      0x000a,},      //New Add. If not set, when system boots up, switch panel to HDMI 1080P, nothing on TV.
#endif
    {VENC_VIDEO_PROG_MODE,       0x100, },
    {VENC_SYNC_ROUTE,            0,     },
    {VENC_INTCTRL,               0x200, },
    {VFIFO2VD_CTL,               0,     },
    {VENC_VDAC_SETTING,          0,     },
    {ENCI_VIDEO_EN,              0,     },
    {ENCP_VIDEO_EN,              1,     },
    {MREG_END_MARKER,            0      }
};

static const reg_t tvregs_1080p_50hz[] = {
    {VENC_VDAC_SETTING,          0xff,  },
//	{VCLK_HD},
	{HHI_VID_CLK_DIV,			 1		},
    {HHI_VID_CLK_CNTL,        	 0x0421,},
    {ENCP_VIDEO_FILT_CTRL,       0x1052,},

    // bit 13    1          (delayed prog_vs)
    // bit 5:4:  2          (pixel[0])
    // bit 3:    1          invert vsync or not
    // bit 2:    1          invert hsync or not
    // bit1:     1          (select viu sync)
    // bit0:     1          (progressive)
    {VENC_DVI_SETTING,           0x000d,},
    {ENCP_VIDEO_MAX_PXCNT,       2639,  },
    {ENCP_VIDEO_MAX_LNCNT,       1124,  },
    /* horizontal timing settings */
    {ENCP_VIDEO_HSPULS_BEGIN,    44,  },//1980
    {ENCP_VIDEO_HSPULS_END,      132,    },
    {ENCP_VIDEO_HSPULS_SWITCH,   44,    },

    //DE position in horizontal
    {ENCP_VIDEO_HAVON_BEGIN,     271,   },
    {ENCP_VIDEO_HAVON_END,       2190,  },

    //ditital hsync positon in horizontal
    {ENCP_VIDEO_HSO_BEGIN,       79 ,    },
    {ENCP_VIDEO_HSO_END,         123,  },

    /* vsync horizontal timing */
    {ENCP_VIDEO_VSPULS_BEGIN,    220,   },
    {ENCP_VIDEO_VSPULS_END,      2140,  },

    /* vertical timing settings */
    {ENCP_VIDEO_VSPULS_BLINE,    0,     },
    {ENCP_VIDEO_VSPULS_ELINE,    4,     },//35
    {ENCP_VIDEO_EQPULS_BLINE,    0,     },
    {ENCP_VIDEO_EQPULS_ELINE,    4,     },//35
    {ENCP_VIDEO_VAVON_BLINE,     41,    },
    {ENCP_VIDEO_VAVON_ELINE,     1120,  },

    //adjust the hsync & vsync start point and end point
    {ENCP_VIDEO_VSO_BEGIN,       79,  },
    {ENCP_VIDEO_VSO_END,         79,  },

    //adjust the vsync start line and end line
    {ENCP_VIDEO_VSO_BLINE,       0,     },
    {ENCP_VIDEO_VSO_ELINE,       5,     },

    {ENCP_VIDEO_YFP1_HTIME,      271,   },
    {ENCP_VIDEO_YFP2_HTIME,      2190,  },
    {VENC_VIDEO_PROG_MODE,       0x100, },
    {ENCP_VIDEO_MODE,            0x0040,},
    {ENCP_VIDEO_MODE_ADV,        0x0018,},

    {ENCP_VIDEO_SYNC_MODE,       0x7, }, //bit[15:8] -- adjust the vsync vertical position

    {ENCP_VIDEO_YC_DLY,          0,     },      //Y/Cb/Cr delay

    {ENCP_VIDEO_RGB_CTRL, 2,},       // enable sync on B

    {VENC_SYNC_ROUTE,            0,     },
    {VENC_INTCTRL,               0x200, },
    {VFIFO2VD_CTL,               0,     },
    {VENC_VDAC_SETTING,          0,     },
    {ENCI_VIDEO_EN,              0,     },
    {ENCP_VIDEO_EN,              1,     },
    {MREG_END_MARKER,            0      }
};


/* The sequence of register tables items must match the enum define in tvmode.h */
static const reg_t *tvregsTab[] = {
    tvregs_480i,
    tvregs_480cvbs,		
    tvregs_480p,
    tvregs_576i,
    tvregs_576cvbs,
    tvregs_576p,
    tvregs_720p,
    tvregs_1080i,       //Adjust tvregs_* sequences and match the enum define in tvmode.h
    tvregs_1080p,
    tvregs_720p_50hz,
    tvregs_1080i_50hz,
    tvregs_1080p_50hz,
};

static const tvinfo_t tvinfoTab[] = {
    {.xres =  720, .yres =  480, .id = "480i"},
    {.xres =  720, .yres =  480, .id = "480cvbs"},		
    {.xres =  720, .yres =  480, .id = "480p"},
    {.xres =  720, .yres =  576, .id = "576i"},
    {.xres =  720, .yres =  576, .id = "576cvbs"},
    {.xres =  720, .yres =  576, .id = "576p"},
    {.xres = 1280, .yres =  720, .id = "720p"},
    {.xres = 1280, .yres =  720, .id = "720p50hz"},
    {.xres = 1920, .yres = 1080, .id = "1080i"},
    {.xres = 1920, .yres = 1080, .id = "1080i50hz"},
    {.xres = 1920, .yres = 1080, .id = "1080p"},
    {.xres = 1920, .yres = 1080, .id = "1080p50hz"}
};

static inline void setreg(const reg_t *r)
{
    WRITE_MPEG_REG(r->reg, r->val);
    
// For debug only. 
#if 0 
    printk("[0x%x] = 0x%x\n", r->reg, r->val);
#endif
}

#endif /* TVREGS_H */

