/*
 *  arch/arm/mach-meson/include/mach/clock.h
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ARCH_ARM_MESON3_CLOCK_H
#define __ARCH_ARM_MESON3_CLOCK_H

// -----------------------------------------
// clk_util_clk_msr
// -----------------------------------------
// from twister_core.v
//        .clk_to_msr_in          ( { 18'h0,                      // [63:46]
//                                    cts_pwm_A_clk,              // [45]
//                                    cts_pwm_B_clk,              // [44]
//                                    cts_pwm_C_clk,              // [43]
//                                    cts_pwm_D_clk,              // [42]
//                                    cts_eth_rx_tx,              // [41]
//                                    cts_pcm_mclk,               // [40]
//                                    cts_pcm_sclk,               // [39]
//                                    cts_vdin_meas_clk,          // [38]
//                                    cts_vdac_clk[1],            // [37]
//                                    cts_hdmi_tx_pixel_clk,      // [36]
//                                    cts_mali_clk,               // [35]
//                                    cts_sdhc_clk1,              // [34]
//                                    cts_sdhc_clk0,              // [33]
//                                    cts_audac_clkpi,            // [32]
//                                    cts_a9_clk,                 // [31]
//                                    cts_ddr_clk,                // [30]
//                                    cts_vdac_clk[0],            // [29]
//                                    cts_sar_adc_clk,            // [28]
//                                    cts_enci_clk,               // [27]
//                                    sc_clk_int,                 // [26]
//                                    usb_clk_12mhz,              // [25]
//                                    lvds_fifo_clk,              // [24]
//                                    HDMI_CH3_TMDSCLK,           // [23]
//                                    mod_eth_clk50_i,            // [22]
//                                    mod_audin_amclk_i,          // [21]
//                                    cts_btclk27,                // [20]
//                                    cts_hdmi_sys_clk,           // [19]
//                                    cts_led_pll_clk,            // [18]
//                                    cts_vghl_pll_clk,           // [17]
//                                    cts_FEC_CLK_2,              // [16]
//                                    cts_FEC_CLK_1,              // [15]
//                                    cts_FEC_CLK_0,              // [14]
//                                    cts_amclk,                  // [13]
//                                    vid2_pll_clk,               // [12]
//                                    cts_eth_rmii,               // [11]
//                                    cts_enct_clk,               // [10]
//                                    cts_encl_clk,               // [9]
//                                    cts_encp_clk,               // [8]
//                                    clk81,                      // [7]
//                                    vid_pll_clk,                // [6]
//                                    aud_pll_clk,                // [5]
//                                    misc_pll_clk,               // [4]
//                                    ddr_pll_clk,                // [3]
//                                    sys_pll_clk,                // [2]
//                                    am_ring_osc_clk_out[1],     // [1]
//                                    am_ring_osc_clk_out[0]} ),  // [0]
//

#define CTS_PWM_A_CLK                  (45)
#define CTS_PWM_B_CLK                  (44)
#define CTS_PWM_C_CLK                  (43)
#define CTS_PWM_D_CLK                  (42)
#define CTS_ETH_RX_TX                  (41)
#define CTS_PCM_MCLK                   (40)
#define CTS_PCM_SCLK                   (39)
#define CTS_VDIN_MEAS_CLK              (38)
#define CTS_VDAC_CLK1                  (37)
#define CTS_HDMI_TX_PIXEL_CLK          (36)
#define CTS_MALI_CLK                   (35)
#define CTS_SDHC_CLK1                  (34)
#define CTS_SDHC_CLK0                  (33)
#define CTS_AUDAC_CLKPI                (32)
#define CTS_A9_CLK                     (31)
#define CTS_DDR_CLK                    (30)
#define CTS_VDAC_CLK0                  (29)
#define CTS_SAR_ADC_CLK                (28)
#define CTS_ENCI_CLK                   (27)
#define SC_CLK_INT                     (26)
#define USB_CLK_12MHZ                  (25)
#define LVDS_FIFO_CLK                  (24)
#define HDMI_CH3_TMDSCLK               (23)
#define MOD_ETH_CLK50_I                (22)
#define MOD_AUDIN_AMCLK_I              (21)
#define CTS_BTCLK27                    (20)
#define CTS_HDMI_SYS_CLK               (19)
#define CTS_LED_PLL_CLK                (18)
#define CTS_VGHL_PLL_CLK               (17)
#define CTS_FEC_CLK_2                  (16)
#define CTS_FEC_CLK_1                  (15)
#define CTS_FEC_CLK_0                  (14)
#define CTS_AMCLK                      (13)
#define VID2_PLL_CLK                   (12)
#define CTS_ETH_RMII                   (11)
#define CTS_ENCT_CLK                   (10)
#define CTS_ENCL_CLK                   (9)
#define CTS_ENCP_CLK                   (8)
#define CLK81                          (7)
#define VID_PLL_CLK                    (6)
#define AUD_PLL_CLK                    (5)
#define MISC_PLL_CLK                   (4)
#define DDR_PLL_CLK                    (3)
#define SYS_PLL_CLK                    (2)
#define AM_RING_OSC_CLK_OUT1           (1)
#define AM_RING_OSC_CLK_OUT0           (0)
struct clk {
    const char *name;
    unsigned long rate;
    unsigned long min;
    unsigned long max;
    int source_clk;
    /* for clock gate */
    unsigned char clock_index;
    unsigned clock_gate_reg_adr;
    unsigned clock_gate_reg_mask;
    /**/
    unsigned long(*get_rate)(struct clk *);
    int (*set_rate)(struct clk *, unsigned long);
};

extern int set_usb_phy_clk(int rate);
extern int set_sata_phy_clk(int sel);

#define USB_CTL_POR_ON          10
#define USB_CTL_POR_OFF     11
#define USB_CTL_POR_ENABLE  12
#define USB_CTL_POR_DISABLE 13

#define USB_CTL_INDEX_A 0
#define USB_CTL_INDEX_B 1
extern void set_usb_ctl_por(int index, int por_flag);

unsigned int clk_util_clk_msr(unsigned int clk_mux);
unsigned int get_mpeg_clk(void );
unsigned int get_system_clk(void );
unsigned int get_misc_pll_clk(void );
unsigned int get_ddr_pll_clk(void );
unsigned int clk_measure(char);
#endif //__ARCH_ARM_MESON3_CLOCK_H
