/*
 *
 * arch/arm/mach-meson/clock.c
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * License terms: GNU General Public License (GPL) version 2
 * Define clocks in the app platform.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/device.h>

#include <asm/clkdev.h>
#include <mach/clock.h>
#include <mach/hardware.h>
#include <mach/clk_set.h>
#include <mach/am_regs.h>
#include <mach/power_gate.h>

static DEFINE_SPINLOCK(clockfw_lock);

#ifdef CONFIG_INIT_A9_CLOCK_FREQ
static unsigned long __initdata init_clock = CONFIG_INIT_A9_CLOCK;
#else
static unsigned long __initdata init_clock = 0;
#endif

static unsigned long ddr_pll_clk = 0;

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
// For Example
//
// unsigend long    clk81_clk   = clk_util_clk_msr( 2,      // mux select 2
//                                                  50 );   // measure for 50uS
//
// returns a value in "clk81_clk" in Hz
//
// The "uS_gate_time" can be anything between 1uS and 65535 uS, but the limitation is
// the circuit will only count 65536 clocks.  Therefore the uS_gate_time is limited by
//
//   uS_gate_time <= 65535/(expect clock frequency in MHz)
//
// For example, if the expected frequency is 400Mhz, then the uS_gate_time should
// be less than 163.
//
// Your measurement resolution is:
//
//    100% / (uS_gate_time * measure_val )
//
//
unsigned int clk_util_clk_msr(unsigned int clk_mux)
{
    unsigned int regval = 0;
    WRITE_CBUS_REG(MSR_CLK_REG0, 0);
    // Set the measurement gate to 64uS
    CLEAR_CBUS_REG_MASK(MSR_CLK_REG0, 0xffff);
    SET_CBUS_REG_MASK(MSR_CLK_REG0, (64 - 1)); //64uS is enough for measure the frequence?
    // Disable continuous measurement
    // disable interrupts
    CLEAR_CBUS_REG_MASK(MSR_CLK_REG0, ((1 << 18) | (1 << 17)));
    CLEAR_CBUS_REG_MASK(MSR_CLK_REG0, (0x1f << 20));
    SET_CBUS_REG_MASK(MSR_CLK_REG0, (clk_mux << 20) | // Select MUX
                                    (1 << 19) |       // enable the clock
									(1 << 16));       //enable measuring
    // Wait for the measurement to be done
    regval = READ_CBUS_REG(MSR_CLK_REG0);
    do {
        regval = READ_CBUS_REG(MSR_CLK_REG0);
    } while (regval & (1 << 31));

    // disable measuring
    CLEAR_CBUS_REG_MASK(MSR_CLK_REG0, (1 << 16));
    regval = (READ_CBUS_REG(MSR_CLK_REG2) + 31) & 0x000FFFFF;
    // Return value in MHz*measured_val
    return (regval >> 6);
}

unsigned int clk_measure(char index)
{
	const char* clk_table[] = {
		"CTS_PWM_A_CLK(45)",
		"CTS_PWM_B_CLK(44)",
		" CTS_PWM_C_CLK(43)",
		" CTS_PWM_D_CLK(42)",
		" CTS_ETH_RX_TX (41)",
		" CTS_PCM_MCLK(40)",
		" CTS_PCM_SCLK(39)",
		" CTS_VDIN_MEAS_CLK(38)",
		" CTS_VDAC_CLK1(37)",
		" CTS_HDMI_TX_PIXEL_CLK(36)",
		" CTS_MALI_CLK (35)",
		" CTS_SDHC_CLK1(34)",
		" CTS_SDHC_CLK0(33)",
		" CTS_AUDAC_CLKPI(32)",
		" CTS_A9_CLK(31)",
		" CTS_DDR_CLK(30)",
		" CTS_VDAC_CLK0(29)",
		" CTS_SAR_ADC_CLK (28)",
		" CTS_ENCI_CL(27)",
		" SC_CLK_INT(26)",
		" USB_CLK_12MHZ (25)",
		" LVDS_FIFO_CLK (24)",
		" HDMI_CH3_TMDSCLK(23)",
		" MOD_ETH_CLK50_I (22)",
		" MOD_AUDIN_AMCLK_I  (21)",
		" CTS_BTCLK27 (20)",
		" CTS_HDMI_SYS_CLK(19)",
		" CTS_LED_PLL_CLK(18)",
		" CTS_VGHL_PLL_CLK (17)",
		" CTS_FEC_CLK_2(16)",
		" CTS_FEC_CLK_1 (15)",
		" CTS_FEC_CLK_0 (14)",
		" CTS_AMCLK(13)",
		" VID2_PLL_CLK(12)",
		" CTS_ETH_RMII(11)",
		" CTS_ENCT_CLK(10)",
		" CTS_ENCL_CLK(9)",
		" CTS_ENCP_CLK(8)",
		" CLK81(7)",
		" VID_PLL_CLK(6)",
		" AUD_PLL_CLK(5)",
		" MISC_PLL_CLK(4)",
		" DDR_PLL_CLK(3)",
		" SYS_PLL_CLK(2)",
		" AM_RING_OSC_CLK_OUT1(1)",
		" AM_RING_OSC_CLK_OUT0(0)",
	};   
	int i;
	
	if (index == 0xff) {
	 	for (i = 0; i < sizeof(clk_table) / sizeof(char *); i++) {
			printk("[%d] %s\n", clk_util_clk_msr(i), clk_table[45-i]);
		}
		return 0;
	}	
	printk("[%d MHz] %s\n", clk_util_clk_msr(index), clk_table[45-index]);

	return 0;
}
EXPORT_SYMBOL(clk_measure);

unsigned  int get_system_clk(void)
{
    static unsigned int sys_freq = 0;
    if (sys_freq == 0) {
        sys_freq = (clk_util_clk_msr(SYS_PLL_CLK) * 1000000);
    }
    return sys_freq;
}
EXPORT_SYMBOL(get_system_clk);

unsigned int get_mpeg_clk(void)
{
    static unsigned int clk81_freq = 0;
    if (clk81_freq == 0) {
        clk81_freq = (clk_util_clk_msr(CLK81) * 1000000);
    }
    return clk81_freq;
}
EXPORT_SYMBOL(get_mpeg_clk);

unsigned int get_misc_pll_clk(void)
{
    static unsigned int freq = 0;
    if (freq == 0) {
        freq = (clk_util_clk_msr(MISC_PLL_CLK) * 1000000);
    }
    return freq;
}
EXPORT_SYMBOL(get_misc_pll_clk);

unsigned int get_ddr_pll_clk(void)
{
    static unsigned int freq = 0;
    if (freq == 0) {
        freq = (clk_util_clk_msr(DDR_PLL_CLK) * 1000000);
    }
    return freq;
}
EXPORT_SYMBOL(get_ddr_pll_clk);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
    if (rate < clk->min) {
        return clk->min;
    }

    if (rate > clk->max) {
        return clk->max;
    }

    return rate;
}
EXPORT_SYMBOL(clk_round_rate);

unsigned long clk_get_rate(struct clk *clk)
{
    if (!clk) {
        return 0;
    }

    if (clk->get_rate) {
        return clk->get_rate(clk);
    }

    return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
    unsigned long flags;
    int ret;

    if (clk == NULL || clk->set_rate == NULL) {
        return -EINVAL;
    }

    spin_lock_irqsave(&clockfw_lock, flags);

    ret = clk->set_rate(clk, rate);

    spin_unlock_irqrestore(&clockfw_lock, flags);

    return ret;
}
EXPORT_SYMBOL(clk_set_rate);

static unsigned long xtal_get_rate(struct clk *clk)
{
    unsigned long rate;

    rate = get_xtal_clock();/*refresh from register*/
    clk->rate = rate;

    return rate;
}

static int clk_set_rate_sys_pll(struct clk *clk, unsigned long rate)
{
    unsigned long r = rate;
    int ret;

    if (r < 1000) {
        r = r * 1000000;
    }

    ret = sys_clkpll_setting(0, r);
    if (ret == 0) {
        clk->rate = r;
    }

    return ret;
}

static int clk_set_rate_misc_pll(struct clk *clk, unsigned long rate)
{
    unsigned long r = rate;
    int ret;

    if (r < 1000) {
        r = r * 1000000;
    }

    ret = misc_pll_setting(0, r);

    if (ret == 0) {
        clk->rate = r;
    }

    return ret;
}

static unsigned long clk_get_rate_clk81(struct clk *clk)
{
    unsigned long rate;

    rate = clk_util_clk_msr(CLK81) * 1000000;
    clk->rate = rate;
    return rate;
}

static int clk_set_rate_clk81(struct clk *clk, unsigned long rate)
{
    unsigned long r = rate;
    struct clk *father_clk;
    unsigned long r1, clk81;
    int ret;

    if (r < 1000) {
        r = r * 1000000;
    }

    father_clk = clk_get_sys("clk_misc_pll", NULL);

    r1 = clk_get_rate(father_clk);

    if (r1 != r * 4) {
    	CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, 1<<8); // clk81 to xtal
        ret = father_clk->set_rate(father_clk, r * 4);

        if (ret != 0) {
        	SET_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, 1<<8); // clk81 to pll
            return ret;
        }
    }

    clk->rate = r;

    WRITE_MPEG_REG(HHI_MPEG_CLK_CNTL,
                   (2 << 12) |          // select SYS PLL
                   ((4 - 1) << 0) |     // div1
                   (1 << 7));           // cntl_hi_mpeg_div_en, enable gating
    r1 = READ_MPEG_REG(HHI_MPEG_CLK_CNTL);
    SET_MPEG_REG_MASK(HHI_MPEG_CLK_CNTL,
                   (1 << 8));           // Connect clk81 to the PLL divider output

    clk81 = clk_util_clk_msr(CLK81);
    printk("(CTS_CLK81) = %ldMHz\n", clk81);
    return 0;
}

extern int get_sys_clkpll_setting(unsigned crystal_freq, unsigned out_freq);
static int clk_set_rate_a9_clk(struct clk *clk, unsigned long rate)
{
    struct clk *father_clk;
    int ret, divider;
    unsigned long flags;
    unsigned int clk_a9 = 0;
    unsigned long target_clk;
    int mali_divider;
    int mali_change_flag;
    int mali_busy_flag;
    unsigned mali_cur_divider;
	
    father_clk = clk_get_sys("clk_sys_pll", NULL);
	divider = 0;
	while ((rate<<divider)<200000000)
		divider++;
	if (divider>2){
		return -1;
	}

    target_clk = get_sys_clkpll_setting(0, rate<<divider);
    target_clk = (((target_clk&0x1ff)*get_xtal_clock()/1000000)>>(target_clk>>16))>>divider;
    if (!ddr_pll_clk)
    	ddr_pll_clk = clk_util_clk_msr(CTS_DDR_CLK);
    mali_divider = 1;
	while ((mali_divider * target_clk < ddr_pll_clk) || (264 * mali_divider < ddr_pll_clk)) // assume mali max 264M
		mali_divider++;
    
    local_irq_save(flags); // disable interrupt

    mali_cur_divider = (READ_MPEG_REG(HHI_MALI_CLK_CNTL)&0x7f)+1;
    mali_change_flag = (mali_divider != mali_cur_divider); // mali divider need to change
    mali_busy_flag = READ_CBUS_REG(HHI_MALI_CLK_CNTL)&(1<<8);

    if (mali_change_flag // mali divider need to change
    	&& mali_busy_flag // mali busy
    	&& (target_clk*mali_cur_divider<=ddr_pll_clk)){ // a9 <= mali
    	local_irq_restore(flags);
    	return -1;
	}

    if ((!mali_busy_flag) && mali_change_flag){ // mali not busy
	    WRITE_CBUS_REG(HHI_MALI_CLK_CNTL,
	               (3 << 9)    |   		// select ddr pll as clock source
	               ((mali_divider-1) << 0)); // ddr clk / divider
		mali_change_flag = 0;
		mali_cur_divider = mali_divider;
	}
    CLEAR_CBUS_REG_MASK(HHI_SYS_CPU_CLK_CNTL, 1<<7); // cpu use xtal
    ret = father_clk->set_rate(father_clk, rate<<divider);
	if (divider<2){
	    WRITE_MPEG_REG(HHI_SYS_CPU_CLK_CNTL,
	                   (1 << 0) |  // 1 - sys pll clk
	                   (divider << 2) |  // sys pll div 1 or 2
	                   (1 << 4) |  // APB_CLK_ENABLE
	                   (1 << 5) |  // AT_CLK_ENABLE
	                   (0 << 8)); 
	}
	else
	{
	    WRITE_MPEG_REG(HHI_SYS_CPU_CLK_CNTL,
	                   (1 << 0) |  // 1 - sys pll clk
	                   (3 << 2) |  // sys pll div 4
	                   (1 << 4) |  // APB_CLK_ENABLE
	                   (1 << 5) |  // AT_CLK_ENABLE
	                   (2 << 8)); 
	}
    clk_a9 = READ_MPEG_REG(HHI_SYS_CPU_CLK_CNTL); // read cbus for a short delay
    SET_CBUS_REG_MASK(HHI_SYS_CPU_CLK_CNTL, 1<<7); // cpu use sys pll
    clk->rate = rate;
    local_irq_restore(flags);

    //printk(KERN_INFO "-----------------------------------\n");
   // printk(KERN_INFO "(CTS_MALI_CLK) = %ldMHz\n", ddr_pll_clk/mali_cur_divider);
	//printk(KERN_INFO "(CTS_A9_CLK) = %ldMHz\n", rate/1000000);
	return mali_change_flag;
}

static struct clk xtal_clk = {
    .name       = "clk_xtal",
    .rate       = 24000000,
    .get_rate   = xtal_get_rate,
    .set_rate   = NULL,
};

static struct clk clk_sys_pll = {
    .name       = "clk_sys_pll",
    .rate       =  800000000,
    .min        =  200000000,
    .max        = 1600000000,
    .set_rate   = clk_set_rate_sys_pll,
};

static struct clk clk_misc_pll = {
    .name       = "clk_misc_pll",
    .rate       = 480000000,
    .min        = 400000000,
    .max        = 800000000,
    .set_rate   = clk_set_rate_misc_pll,
};

static struct clk clk_ddr_pll = {
    .name       = "clk_ddr",
    .rate       = 528000000,
    .set_rate   = NULL,
};

static struct clk clk81 = {
    .name       = "clk81",
    .rate       = 120000000,
    .min        = 100000000,
    .max        = 200000000,
    .get_rate   = clk_get_rate_clk81,
    .set_rate   = clk_set_rate_clk81,
};

static struct clk a9_clk = {
    .name       = "a9_clk",
    .rate       = 600000000,
    .min        = 100000000,
    .max        = 1000000000, //1000000000
    .set_rate   = clk_set_rate_a9_clk,
};

REGISTER_CLK(DDR);
REGISTER_CLK(VLD_CLK);
REGISTER_CLK(IQIDCT_CLK);
REGISTER_CLK(MC_CLK);
REGISTER_CLK(AHB_BRIDGE);
REGISTER_CLK(ISA);
REGISTER_CLK(APB_CBUS);
REGISTER_CLK(_1200XXX);
REGISTER_CLK(SPICC);
REGISTER_CLK(I2C);
REGISTER_CLK(SAR_ADC);
REGISTER_CLK(SMART_CARD_MPEG_DOMAIN);
REGISTER_CLK(RANDOM_NUM_GEN);
REGISTER_CLK(UART0);
REGISTER_CLK(SDHC);
REGISTER_CLK(STREAM);
REGISTER_CLK(ASYNC_FIFO);
REGISTER_CLK(SDIO);
REGISTER_CLK(AUD_BUF);
REGISTER_CLK(HIU_PARSER);
REGISTER_CLK(RESERVED0);
REGISTER_CLK(AMRISC);
REGISTER_CLK(BT656_IN);
REGISTER_CLK(ASSIST_MISC);
REGISTER_CLK(VENC_I_TOP);
REGISTER_CLK(VENC_P_TOP);
REGISTER_CLK(VENC_T_TOP);
REGISTER_CLK(VENC_DAC);
REGISTER_CLK(VI_CORE);
REGISTER_CLK(RESERVED1);
REGISTER_CLK(SPI2);
REGISTER_CLK(MDEC_CLK_ASSIST);
REGISTER_CLK(MDEC_CLK_PSC);
REGISTER_CLK(SPI1);
REGISTER_CLK(AUD_IN);
REGISTER_CLK(ETHERNET);
REGISTER_CLK(DEMUX);
REGISTER_CLK(RESERVED2);
REGISTER_CLK(AIU_AI_TOP_GLUE);
REGISTER_CLK(AIU_IEC958);
REGISTER_CLK(AIU_I2S_OUT);
REGISTER_CLK(AIU_AMCLK_MEASURE);
REGISTER_CLK(AIU_AIFIFO2);
REGISTER_CLK(AIU_AUD_MIXER);
REGISTER_CLK(AIU_MIXER_REG);
REGISTER_CLK(AIU_ADC);
REGISTER_CLK(BLK_MOV);
REGISTER_CLK(RESERVED3);
REGISTER_CLK(UART1);
REGISTER_CLK(LED_PWM);
REGISTER_CLK(VGHL_PWM);
REGISTER_CLK(RESERVED4);
REGISTER_CLK(GE2D);
REGISTER_CLK(USB0);
REGISTER_CLK(USB1);
REGISTER_CLK(RESET);
REGISTER_CLK(NAND);
REGISTER_CLK(HIU_PARSER_TOP);
REGISTER_CLK(MDEC_CLK_DBLK);
REGISTER_CLK(MDEC_CLK_PIC_DC);
REGISTER_CLK(VIDEO_IN);
REGISTER_CLK(AHB_ARB0);
REGISTER_CLK(EFUSE);
REGISTER_CLK(ROM_CLK);
REGISTER_CLK(RESERVED5);
REGISTER_CLK(AHB_DATA_BUS);
REGISTER_CLK(AHB_CONTROL_BUS);
REGISTER_CLK(HDMI_INTR_SYNC);
REGISTER_CLK(HDMI_PCLK);
REGISTER_CLK(RESERVED6);
REGISTER_CLK(RESERVED7);
REGISTER_CLK(RESERVED8);
REGISTER_CLK(MISC_USB1_TO_DDR);
REGISTER_CLK(MISC_USB0_TO_DDR);
REGISTER_CLK(AIU_PCLK);
REGISTER_CLK(MMC_PCLK);
REGISTER_CLK(MISC_DVIN);
REGISTER_CLK(MISC_RDMA);
REGISTER_CLK(RESERVED9);
REGISTER_CLK(UART2);
REGISTER_CLK(VENCI_INT);
REGISTER_CLK(VIU2);
REGISTER_CLK(VENCP_INT);
REGISTER_CLK(VENCT_INT);
REGISTER_CLK(VENCL_INT);
REGISTER_CLK(VENC_L_TOP);
REGISTER_CLK(VCLK2_VENCI);
REGISTER_CLK(VCLK2_VENCI1);
REGISTER_CLK(VCLK2_VENCP);
REGISTER_CLK(VCLK2_VENCP1);
REGISTER_CLK(VCLK2_VENCT);
REGISTER_CLK(VCLK2_VENCT1);
REGISTER_CLK(VCLK2_OTHER);
REGISTER_CLK(VCLK2_ENCI);
REGISTER_CLK(VCLK2_ENCP);
REGISTER_CLK(DAC_CLK);
REGISTER_CLK(AIU_AOCLK);
REGISTER_CLK(AIU_AMCLK);
REGISTER_CLK(AIU_ICE958_AMCLK);
REGISTER_CLK(VCLK1_HDMI);
REGISTER_CLK(AIU_AUDIN_SCLK);
REGISTER_CLK(ENC480P);
REGISTER_CLK(VCLK2_ENCT);
REGISTER_CLK(VCLK2_ENCL);
REGISTER_CLK(MMC_CLK);
REGISTER_CLK(VCLK2_VENCL);
REGISTER_CLK(VCLK2_OTHER1);

static struct clk_lookup lookups[] = {
    {
        .dev_id = "clk_xtal",
        .clk    = &xtal_clk,
    },
    {
        .dev_id = "clk_sys_pll",
        .clk    = &clk_sys_pll,
    },
    {
        .dev_id = "clk_misc_pll",
        .clk    = &clk_misc_pll,
    },
    {
        .dev_id = "clk_ddr_pll",
        .clk    = &clk_ddr_pll,
    },
    {
        .dev_id = "clk81",
        .clk    = &clk81,
    },
    {
        .dev_id = "a9_clk",
        .clk    = &a9_clk,
    },
    CLK_LOOKUP_ITEM(DDR),
    CLK_LOOKUP_ITEM(VLD_CLK),
    CLK_LOOKUP_ITEM(IQIDCT_CLK),
    CLK_LOOKUP_ITEM(MC_CLK),
    CLK_LOOKUP_ITEM(AHB_BRIDGE),
    CLK_LOOKUP_ITEM(ISA),
    CLK_LOOKUP_ITEM(APB_CBUS),
    CLK_LOOKUP_ITEM(_1200XXX),
    CLK_LOOKUP_ITEM(SPICC),
    CLK_LOOKUP_ITEM(I2C),
    CLK_LOOKUP_ITEM(SAR_ADC),
    CLK_LOOKUP_ITEM(SMART_CARD_MPEG_DOMAIN),
    CLK_LOOKUP_ITEM(RANDOM_NUM_GEN),
    CLK_LOOKUP_ITEM(UART0),
    CLK_LOOKUP_ITEM(SDHC),
    CLK_LOOKUP_ITEM(STREAM),
    CLK_LOOKUP_ITEM(ASYNC_FIFO),
    CLK_LOOKUP_ITEM(SDIO),
    CLK_LOOKUP_ITEM(AUD_BUF),
    CLK_LOOKUP_ITEM(HIU_PARSER),
    CLK_LOOKUP_ITEM(RESERVED0),
    CLK_LOOKUP_ITEM(AMRISC),
    CLK_LOOKUP_ITEM(BT656_IN),
    CLK_LOOKUP_ITEM(ASSIST_MISC),
    CLK_LOOKUP_ITEM(VENC_I_TOP),
    CLK_LOOKUP_ITEM(VENC_P_TOP),
    CLK_LOOKUP_ITEM(VENC_T_TOP),
    CLK_LOOKUP_ITEM(VENC_DAC),
    CLK_LOOKUP_ITEM(VI_CORE),
    CLK_LOOKUP_ITEM(RESERVED1),
    CLK_LOOKUP_ITEM(SPI2),
    CLK_LOOKUP_ITEM(MDEC_CLK_ASSIST),
    CLK_LOOKUP_ITEM(MDEC_CLK_PSC),
    CLK_LOOKUP_ITEM(SPI1),
    CLK_LOOKUP_ITEM(AUD_IN),
    CLK_LOOKUP_ITEM(ETHERNET),
    CLK_LOOKUP_ITEM(DEMUX),
    CLK_LOOKUP_ITEM(RESERVED2),
    CLK_LOOKUP_ITEM(AIU_AI_TOP_GLUE),
    CLK_LOOKUP_ITEM(AIU_IEC958),
    CLK_LOOKUP_ITEM(AIU_I2S_OUT),
    CLK_LOOKUP_ITEM(AIU_AMCLK_MEASURE),
    CLK_LOOKUP_ITEM(AIU_AIFIFO2),
    CLK_LOOKUP_ITEM(AIU_AUD_MIXER),
    CLK_LOOKUP_ITEM(AIU_MIXER_REG),
    CLK_LOOKUP_ITEM(AIU_ADC),
    CLK_LOOKUP_ITEM(BLK_MOV),
    CLK_LOOKUP_ITEM(RESERVED3),
    CLK_LOOKUP_ITEM(UART1),
    CLK_LOOKUP_ITEM(LED_PWM),
    CLK_LOOKUP_ITEM(VGHL_PWM),
    CLK_LOOKUP_ITEM(RESERVED4),
    CLK_LOOKUP_ITEM(GE2D),
    CLK_LOOKUP_ITEM(USB0),
    CLK_LOOKUP_ITEM(USB1),
    CLK_LOOKUP_ITEM(RESET),
    CLK_LOOKUP_ITEM(NAND),
    CLK_LOOKUP_ITEM(HIU_PARSER_TOP),
    CLK_LOOKUP_ITEM(MDEC_CLK_DBLK),
    CLK_LOOKUP_ITEM(MDEC_CLK_PIC_DC),
    CLK_LOOKUP_ITEM(VIDEO_IN),
    CLK_LOOKUP_ITEM(AHB_ARB0),
    CLK_LOOKUP_ITEM(EFUSE),
    CLK_LOOKUP_ITEM(ROM_CLK),
    CLK_LOOKUP_ITEM(RESERVED5),
    CLK_LOOKUP_ITEM(AHB_DATA_BUS),
    CLK_LOOKUP_ITEM(AHB_CONTROL_BUS),
    CLK_LOOKUP_ITEM(HDMI_INTR_SYNC),
    CLK_LOOKUP_ITEM(HDMI_PCLK),
    CLK_LOOKUP_ITEM(RESERVED6),
    CLK_LOOKUP_ITEM(RESERVED7),
    CLK_LOOKUP_ITEM(RESERVED8),
    CLK_LOOKUP_ITEM(MISC_USB1_TO_DDR),
    CLK_LOOKUP_ITEM(MISC_USB0_TO_DDR),
    CLK_LOOKUP_ITEM(AIU_PCLK),
    CLK_LOOKUP_ITEM(MMC_PCLK),
    CLK_LOOKUP_ITEM(MISC_DVIN),
    CLK_LOOKUP_ITEM(MISC_RDMA),
    CLK_LOOKUP_ITEM(RESERVED9),
    CLK_LOOKUP_ITEM(UART2),
    CLK_LOOKUP_ITEM(VENCI_INT),
    CLK_LOOKUP_ITEM(VIU2),
    CLK_LOOKUP_ITEM(VENCP_INT),
    CLK_LOOKUP_ITEM(VENCT_INT),
    CLK_LOOKUP_ITEM(VENCL_INT),
    CLK_LOOKUP_ITEM(VENC_L_TOP),
    CLK_LOOKUP_ITEM(VCLK2_VENCI),
    CLK_LOOKUP_ITEM(VCLK2_VENCI1),
    CLK_LOOKUP_ITEM(VCLK2_VENCP),
    CLK_LOOKUP_ITEM(VCLK2_VENCP1),
    CLK_LOOKUP_ITEM(VCLK2_VENCT),
    CLK_LOOKUP_ITEM(VCLK2_VENCT1),
    CLK_LOOKUP_ITEM(VCLK2_OTHER),
    CLK_LOOKUP_ITEM(VCLK2_ENCI),
    CLK_LOOKUP_ITEM(VCLK2_ENCP),
    CLK_LOOKUP_ITEM(DAC_CLK),
    CLK_LOOKUP_ITEM(AIU_AOCLK),
    CLK_LOOKUP_ITEM(AIU_AMCLK),
    CLK_LOOKUP_ITEM(AIU_ICE958_AMCLK),
    CLK_LOOKUP_ITEM(VCLK1_HDMI),
    CLK_LOOKUP_ITEM(AIU_AUDIN_SCLK),
    CLK_LOOKUP_ITEM(ENC480P),
    CLK_LOOKUP_ITEM(VCLK2_ENCT),
    CLK_LOOKUP_ITEM(VCLK2_ENCL),
    CLK_LOOKUP_ITEM(MMC_CLK),
    CLK_LOOKUP_ITEM(VCLK2_VENCL),
    CLK_LOOKUP_ITEM(VCLK2_OTHER1),
};

static int cpu_clk_setting(unsigned long cpu_freq)
{
    int ret = 0;
    unsigned long target_clk;
    unsigned long flags;
	int divider = 0;
	
	while ((cpu_freq<<divider)<200000000)
		divider++;
	if (divider>2)
		return -1;
    local_irq_save(flags);
    CLEAR_MPEG_REG_MASK(HHI_MALI_CLK_CNTL, 1<<8); // mali off
    CLEAR_CBUS_REG_MASK(HHI_SYS_CPU_CLK_CNTL, 1<<7); // cpu use xtal
    
    ret = sys_clkpll_setting(0, cpu_freq<<divider); 
	if (divider<2){
	    WRITE_MPEG_REG(HHI_SYS_CPU_CLK_CNTL,
	                   (1 << 0) |  // 1 - sys pll clk
	                   (divider << 2) |  // sys pll div 1 or 2
	                   (1 << 4) |  // APB_CLK_ENABLE
	                   (1 << 5) |  // AT_CLK_ENABLE
	                   (0 << 7) |  // not connect A9 to the PLL divider output
	                   (0 << 8)); 
	}
	else{
	    WRITE_MPEG_REG(HHI_SYS_CPU_CLK_CNTL,
	                   (1 << 0) |  // 1 - sys pll clk
	                   (2 << 2) |  // sys pll div 4
	                   (1 << 4) |  // APB_CLK_ENABLE
	                   (1 << 5) |  // AT_CLK_ENABLE
	                   (0 << 7) |  // not connect A9 to the PLL divider output
	                   (2 << 8)); 
	}	
    target_clk = READ_MPEG_REG(HHI_SYS_CPU_CLK_CNTL); // read cbus for a short delay
    SET_CBUS_REG_MASK(HHI_SYS_CPU_CLK_CNTL, 1<<7); // cpu use sys pll
    local_irq_restore(flags);

    if (!ddr_pll_clk){
    	ddr_pll_clk = clk_util_clk_msr(CTS_DDR_CLK);
    	printk("(CTS_DDR_CLK) = %ldMHz\n", ddr_pll_clk);
    }
    target_clk = READ_MPEG_REG(HHI_SYS_PLL_CNTL);
    target_clk = (((target_clk&0x1ff)*get_xtal_clock()/1000000)>>(target_clk>>16))>>divider;
    //printk("(CTS_A9_CLK) = %ldMHz\n", target_clk);
	divider = 1;
	while ((divider * target_clk < ddr_pll_clk) || (264 * divider < ddr_pll_clk))
		divider++;
    if ((divider-1) != (READ_CBUS_REG(HHI_MALI_CLK_CNTL)&0x7f)){
    	WRITE_CBUS_REG(HHI_MALI_CLK_CNTL,
                       (3 << 9)    |   		// select ddr pll as clock source
                       (1 << 8)    |   		// enable clock gating
                       ((divider-1) << 0)); // ddr clk / divider
    	printk("(CTS_MALI_CLK) = %ldMHz\n", ddr_pll_clk/divider);
    	//target_clk = clk_util_clk_msr(CTS_MALI_CLK);
    	//printk("(CTS_MALI_CLK) = %ldMHz\n", target_clk);
	}
	else{
		SET_MPEG_REG_MASK(HHI_MALI_CLK_CNTL, 1<<8); // mali on
	}
	printk("(CTS_A9_CLK) = %ldMHz\n", cpu_freq/1000000);
                       
    return ret;
}

#if defined(CONFIG_CLK81_DFS)
#define MAX_FREQ_LEVEL 1

static int clk81_freq_level = 0;
static int new_clk81_freq_level = 0;
static int crystal_freq = 0;

static int level_freq[MAX_FREQ_LEVEL+1] = {
	128000000,
	192000000,
};

static int set_clk81_level(int level)
{
	int clock = level_freq[level];
	unsigned long clk;
	int baudrate;

	if (((ddr_pll_clk==516)||(ddr_pll_clk==508)||(ddr_pll_clk==474)
		||(ddr_pll_clk==486))&& level == 0){
		clock = clk81.rate = ddr_pll_clk*1000000/4;
		baudrate = (clock / (115200 * 4)) - 1;
		CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1 << 8)); // use xtal
		WRITE_MPEG_REG(HHI_MPEG_CLK_CNTL,	// MPEG clk81 set to misc/4
				(3 << 12)	|	// select ddr PLL
				((4 - 1) << 0)	|	// div3
				(1 << 7)	|	// cntl_hi_mpeg_div_en, enable gating
				(1 << 15));		// Production clock enable
		SET_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1 << 8)); // use pll
		WRITE_AOBUS_REG_BITS(AO_UART_CONTROL, baudrate & 0xfff, 0, 12);
		SET_CBUS_REG_MASK(HHI_OTHER_PLL_CNTL, (1 << 15)); // other pll off

		clk = clk_util_clk_msr(CLK81);
		printk("(CLK81) = %ldMHz\n", clk);
		return 0;			
	}

	CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, 1<<8);
	baudrate = (clock / (115200 * 4)) - 1;
	clk81.rate = clock;
	WRITE_MPEG_REG(HHI_MPEG_CLK_CNTL,	// MPEG clk81 set to misc/4
				(2 << 12)	|	// select misc PLL
				((4 - 1) << 0)	|	// div1
				(1 << 7)	|	// cntl_hi_mpeg_div_en, enable gating
				(1 << 15));		// Production clock enable
	WRITE_AOBUS_REG_BITS(AO_UART_CONTROL, baudrate & 0xfff, 0, 12);
	SET_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, 1<<8);
	return 0;
}

int check_and_set_clk81(void)
{
	if (clk81_freq_level != new_clk81_freq_level) {
		set_clk81_level(new_clk81_freq_level);
		clk81_freq_level = new_clk81_freq_level;
	}
	return 0;
}	
EXPORT_SYMBOL(check_and_set_clk81);

static int get_max_common_divisor(int a, int b)
{
    while (b) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

static clk81_pll_setting(unsigned crystal_freq, unsigned  out_freq)
{
    int n, m, od;
    unsigned long crys_M, out_M, middle_freq;
    unsigned long flags;
    
    
    crys_M = crystal_freq / 1000000;
    out_M = out_freq / 1000000;
    if (out_M < 400) {
        /*if <400M, Od=1*/
        od = 1;/*out=pll_out/(1<<od)*/
        out_M = out_M << 1;
    } else {
        od = 0;
    }

    middle_freq = get_max_common_divisor(crys_M, out_M);
    n = crys_M / middle_freq;
    m = out_M / (middle_freq);
    if (n > (1 << 5) - 1) {
        printk(KERN_ERR "misc_pll_setting  error, n is too bigger n=%d,crys_M=%ldM,out=%ldM\n",
               n, crys_M, out_M);
        return -1;
    }
    if (m > (1 << 9) - 1) {
        printk(KERN_ERR "misc_pll_setting  error, m is too bigger m=%d,crys_M=%ldM,out=%ldM\n",
               m, crys_M, out_M);
        return -2;
    }
    local_irq_save(flags);
    WRITE_MPEG_REG(HHI_OTHER_PLL_CNTL,
                   m |
                   n << 9 |
                   (od & 1) << 16
                  ); // misc PLL
    WRITE_MPEG_REG(RESET5_REGISTER, (1<<1));        // reset misc pll
    //WRITE_AOBUS_REG_BITS(AO_UART_CONTROL, (((out_freq/4) / (115200*4)) - 1) & 0xfff, 0, 12);
    local_irq_restore(flags);
	udelay(100);
    return 0;	
}

static ssize_t  clk81_level_store(struct class *cla, struct class_attribute *attr, char *buf,size_t count)
{
	//printk("set freq as level %s\n", buf);
	int val = 0;
	if (!crystal_freq) {
		crystal_freq = get_xtal_clock();
	}
	if(sscanf(buf, "%d", &val) == 1) {
		if (val > MAX_FREQ_LEVEL) {
			printk("level can not greater than %d\n", MAX_FREQ_LEVEL);
			return -1;
		}
		if (val < 0) {
			printk("level can not less  than 0 \n");
			return -1;
		}
		if (clk81_freq_level == val)
			return count;
		//set_clk81_level(val);
		if (val == 1) {
			SET_CBUS_REG_MASK(HHI_OTHER_PLL_CNTL, (0 << 15)); // other pll on
			if  (clk81_pll_setting(crystal_freq, level_freq[1] * 4) != 0) {
				printk("the misc pll can not be set\n");
				SET_CBUS_REG_MASK(HHI_OTHER_PLL_CNTL, (1 << 15)); // other pll off
				return -1;
			}
		}
		new_clk81_freq_level = val;
		return count;
	} else
		return -1;
}

static ssize_t  clk81_level_show(struct class *cla, struct class_attribute *attr, char *buf)
{
	printk("level is %d\n",  clk81_freq_level);
	return 0;
}

static struct class_attribute clk81_class_attrs[] = {
	__ATTR(clk81_freq_level, S_IRWXU, clk81_level_show, clk81_level_store),
	__ATTR_NULL,
};

static struct class clk81_class = {    
	.name = "aml_clk81",
	.class_attrs = clk81_class_attrs,
};
#endif

static int __init meson_clock_init(void)
{
    int ret;
    if (init_clock && init_clock != a9_clk.rate) {
        ret = cpu_clk_setting(init_clock);
    	if (ret>=0){
            a9_clk.rate = init_clock;
            clk_sys_pll.rate = init_clock << ret;
        }
    }

#if defined(CONFIG_CLK81_DFS)
    ret = class_register(&clk81_class);
#endif

    /* Register the lookups */
    clkdev_add_table(lookups, ARRAY_SIZE(lookups));

    return 0;
}

/* initialize clocking early to be available later in the boot */
core_initcall(meson_clock_init);

unsigned long long clkparse(const char *ptr, char **retptr)
{
    char *endptr;   /* local pointer to end of parsed string */

    unsigned long long ret = simple_strtoull(ptr, &endptr, 0);

    switch (*endptr) {
    case 'G':
    case 'g':
        ret *= 1000;
    case 'M':
    case 'm':
        ret *= 1000;
    case 'K':
    case 'k':
        ret *= 1000;
        endptr++;
    default:
        break;
    }

    if (retptr) {
        *retptr = endptr;
    }

    return ret;
}

static int __init a9_clock_setup(char *ptr)
{
    return cpu_clk_setting(clkparse(ptr, 0));
}
__setup("a9_clk=", a9_clock_setup);

static int __init clk81_clock_setup(char *ptr)
{
    int clock = clkparse(ptr, 0);
    unsigned long clk;
    int baudrate;

	clk = clk_util_clk_msr(CLK81);
    printk("CLK81(from MSR_CLK_REG) = %ldMHz, a9_clk(from args) = %ld\n", (clk*1000000), clock);

    if (!ddr_pll_clk){
    	ddr_pll_clk = clk_util_clk_msr(CTS_DDR_CLK);
    	printk("(CTS_DDR_CLK) = %ldMHz\n", ddr_pll_clk);
    }

	if((clk*1000000) == clock)
	{
		printk("CLK81 is the same as args\n");
		return 0;
	}

	if ((ddr_pll_clk==516)||(ddr_pll_clk==508)||(ddr_pll_clk==474)){
        clock = clk81.rate = ddr_pll_clk*1000000/3;
        baudrate = (clock / (115200 * 4)) - 1;
        CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1 << 8)); // use xtal
        WRITE_MPEG_REG(HHI_MPEG_CLK_CNTL,   // MPEG clk81 set to misc/4
                       (3 << 12) |               // select ddr PLL
                       ((3 - 1) << 0) |          // div3
                       (1 << 7) |                // cntl_hi_mpeg_div_en, enable gating
					   (1 << 15));                // Production clock enable
	SET_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1 << 8)); // use pll
        WRITE_AOBUS_REG_BITS(AO_UART_CONTROL, baudrate & 0xfff, 0, 12);
        SET_CBUS_REG_MASK(HHI_OTHER_PLL_CNTL, (1 << 15)); // other pll off

        clk = clk_util_clk_msr(CLK81);
        printk("set CLK81 to %ldMHz, ddr_pll_clk = %ldMHz\n", clk, ddr_pll_clk);
#if defined(CONFIG_CLK81_DFS)
        new_clk81_freq_level = clk81_freq_level = 0; 
#endif
        return 0;			
	}

	CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, 1<<8);
    if (misc_pll_setting(0, clock * 4) == 0) {
        /* todo: uart baudrate depends on clk81, assume 115200 baudrate */
        baudrate = (clock / (115200 * 4)) - 1;

        clk_misc_pll.rate = clock * 4;
        clk81.rate = clock;
        WRITE_MPEG_REG(HHI_MPEG_CLK_CNTL,   // MPEG clk81 set to misc/4
                       (2 << 12) |               // select misc PLL
                       ((4 - 1) << 0) |          // div1
                       (1 << 7) |                // cntl_hi_mpeg_div_en, enable gating
					   (1 << 15));                // Production clock enable
        WRITE_AOBUS_REG_BITS(AO_UART_CONTROL, baudrate & 0xfff, 0, 12);
    }
   	SET_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, 1<<8);
   	
    clk = clk_util_clk_msr(CLK81);
    printk("set CLK81 to %ldMHz\n", clk);
#if defined(CONFIG_CLK81_DFS)
    new_clk81_freq_level = clk81_freq_level = 1;
#endif
    return 0;
}
__setup("clk81=", clk81_clock_setup);

int clk_enable(struct clk *clk)
{
    unsigned long flags;

    spin_lock_irqsave(&clockfw_lock, flags);

    if (clk->clock_index >= 0 && clk->clock_index < GCLK_IDX_MAX && clk->clock_gate_reg_adr != 0) {
        if (GCLK_ref[clk->clock_index]++ == 0) {
            SET_CBUS_REG_MASK(clk->clock_gate_reg_adr, clk->clock_gate_reg_mask);
        }
    }

    spin_unlock_irqrestore(&clockfw_lock, flags);

    return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
    unsigned long flags;

    spin_lock_irqsave(&clockfw_lock, flags);

    if (clk->clock_index >= 0 && clk->clock_index < GCLK_IDX_MAX && clk->clock_gate_reg_adr != 0) {
        if ((GCLK_ref[clk->clock_index] != 0) && (--GCLK_ref[clk->clock_index] == 0)) {
            CLEAR_CBUS_REG_MASK(clk->clock_gate_reg_adr, clk->clock_gate_reg_mask);
        }
    }

    spin_unlock_irqrestore(&clockfw_lock, flags);
}
EXPORT_SYMBOL(clk_disable);
