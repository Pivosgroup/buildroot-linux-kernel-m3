/*
 *  include/linux/hdmi/hdmi_conf.h
 *
 *  Copyright (C) 2012 Amlogic,Inc All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _HDMI_CONFIG_H_
#define _HDMI_CONFIG_H_

// For some boards, HDMI PHY setting may diff from ref board.
struct hdmi_phy_set_data{
    unsigned long freq;     // always run at 27, 74, or 148
    unsigned long addr;     // refer to hdmi ip spec
    unsigned long data;     // should set with Oscilloscope
};

struct hdmi_config_platform_data{
    void (*hdmi_5v_ctrl)(unsigned int pwr);         // Refer to board's Sche
    void (*hdmi_3v3_ctrl)(unsigned int pwr);
    void (*hdmi_pll_vdd_ctrl)(unsigned int pwr);
    void (*hdmi_sspll_ctrl)(unsigned int level);    // SSPLL control level
    struct hdmi_phy_set_data *phy_data;             // For some boards, HDMI PHY setting may diff from ref board.
};

#endif
