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

long clk_round_rate(struct clk *clk,unsigned long rate)
{
    if (rate < clk->min)
        return clk->min;

    if (rate > clk->max)
        return clk->max;

    return rate;
}
EXPORT_SYMBOL(clk_round_rate);

unsigned long clk_get_rate(struct clk *clk)
{
    if (!clk)
        return 0;

    if(clk->get_rate)
        return clk->get_rate(clk);

    return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
    unsigned long flags;
    int ret;

    if (clk == NULL || clk->set_rate==NULL)
        return -EINVAL;

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
	
    if (r < 1000)
        r = r * 1000000;

    ret = sys_clkpll_setting(0, r);

    if (ret == 0)
         clk->rate = r;

    return ret;
}

static int clk_set_rate_other_pll(struct clk *clk, unsigned long rate)
{
    unsigned long r = rate;
    int ret;
	
    if (r < 1000)
        r = r * 1000000;

    ret = other_pll_setting(0, r);

    if (ret == 0)
        clk->rate = r;

    return ret;
}

static int clk_set_rate_clk81(struct clk *clk, unsigned long rate)
{
    unsigned long r = rate;
    struct clk *father_clk;
    unsigned long r1;
    int ret;
	
    if (r < 1000)
        r = r * 1000000;

    father_clk = clk_get_sys("clk_other_pll", NULL);

    r1 = clk_get_rate(father_clk);

    if (r1 != r*4) {
        ret = father_clk->set_rate(father_clk, r*4);

        if (ret != 0)
            return ret;
    }

    clk->rate = r;

    WRITE_MPEG_REG(HHI_MPEG_CLK_CNTL,
              (1 << 12) |          // select other PLL
              ((4 - 1) << 0 ) |    // div1
              (1 << 7 ) |          // cntl_hi_mpeg_div_en, enable gating
              (1 << 8 ));          // Connect clk81 to the PLL divider output

    return 0;
}

static int clk_set_rate_a9_clk(struct clk *clk, unsigned long rate)
{
    unsigned long r = rate;
    struct clk *father_clk;
    unsigned long r1;
    int ret;
	
    if (r < 1000)
        r = r * 1000000;

    father_clk = clk_get_sys("clk_sys_pll", NULL);

    r1 = clk_get_rate(father_clk);

    if (!r1)
        return -1;

    if (r1!=r*2 && r!=0) {
        ret = father_clk->set_rate(father_clk, r*2);

        if (ret != 0)
            return ret;
    }

    clk->rate = r;

    WRITE_MPEG_REG(HHI_A9_CLK_CNTL,
              (0 << 10) |          // 0 - sys_pll_clk, 1 - audio_pll_clk
              (1 << 0 ) |          // 1 - sys/audio pll clk, 0 - XTAL
              (1 << 4 ) |          // APB_CLK_ENABLE
              (1 << 5 ) |          // AT_CLK_ENABLE
              (0 << 2 ) |          // div1
              (1 << 7 ));          // Connect A9 to the PLL divider output
    return 0;
}

static struct clk xtal_clk = {
    .name       = "clk_xtal",
    .rate       = 24000000,
    .get_rate   = xtal_get_rate,
    .set_rate   = NULL,
};

static struct clk clk_sys_pll = {
    .name       = "clk_sys_pll",
    .rate       = 1200000000,
    .min        = 200000000,
    .max        = 2000000000,
    .set_rate   = clk_set_rate_sys_pll,
};

static struct clk clk_other_pll = {
    .name       = "clk_other_pll",
    .rate       = 540000000,
    .min        = 200000000,
    .max        = 800000000,
    .set_rate   = clk_set_rate_other_pll,
};

static struct clk clk_ddr_pll = {
    .name       = "clk_ddr",
    .rate       = 400000000,
    .set_rate   = NULL,
};

static struct clk clk81 = {
    .name       = "clk81",
    .rate       = 180000000,
    .min        = 100000000,
    .max        = 400000000,
    .set_rate   = clk_set_rate_clk81,
};

static struct clk a9_clk = {
    .name       = "a9_clk",
    .rate       = 600000000,
    .min        = 200000000,
    .max        = 800000000,
    .set_rate   = clk_set_rate_a9_clk,
};

REGISTER_CLK(AHB_BRIDGE);
REGISTER_CLK(AHB_SRAM);
REGISTER_CLK(AIU_ADC);
REGISTER_CLK(AIU_MIXER_REG);
REGISTER_CLK(AIU_AUD_MIXER);
REGISTER_CLK(AIU_AIFIFO2);
REGISTER_CLK(AIU_AMCLK_MEASURE);
REGISTER_CLK(AIU_I2S_OUT);
REGISTER_CLK(AIU_IEC958);
REGISTER_CLK(AIU_AI_TOP_GLUE);
REGISTER_CLK(AIU_AUD_DAC);
REGISTER_CLK(AIU_ICE958_AMCLK);
REGISTER_CLK(AIU_I2S_DAC_AMCLK);
REGISTER_CLK(AIU_I2S_SLOW);
REGISTER_CLK(AIU_AUD_DAC_CLK);
REGISTER_CLK(ASSIST_MISC);
REGISTER_CLK(AMRISC);
REGISTER_CLK(AUD_BUF);
REGISTER_CLK(AUD_IN);
REGISTER_CLK(BLK_MOV);
REGISTER_CLK(BT656_IN);
REGISTER_CLK(DEMUX);
REGISTER_CLK(MMC_DDR);
REGISTER_CLK(DDR);
REGISTER_CLK(ETHERNET);
REGISTER_CLK(GE2D);
REGISTER_CLK(HDMI_MPEG_DOMAIN);
REGISTER_CLK(HIU_PARSER_TOP);
REGISTER_CLK(HIU_PARSER);
REGISTER_CLK(ISA);
REGISTER_CLK(MEDIA_CPU);
REGISTER_CLK(MISC_USB0_TO_DDR);
REGISTER_CLK(MISC_USB1_TO_DDR);
REGISTER_CLK(MISC_SATA_TO_DDR);
REGISTER_CLK(AHB_CONTROL_BUS);
REGISTER_CLK(AHB_DATA_BUS);
REGISTER_CLK(AXI_BUS);
REGISTER_CLK(ROM_CLK);
REGISTER_CLK(EFUSE);
REGISTER_CLK(AHB_ARB0);
REGISTER_CLK(RESET);
REGISTER_CLK(MDEC_CLK_PIC_DC);
REGISTER_CLK(MDEC_CLK_DBLK);
REGISTER_CLK(MDEC_CLK_PSC);
REGISTER_CLK(MDEC_CLK_ASSIST);
REGISTER_CLK(MC_CLK);
REGISTER_CLK(IQIDCT_CLK);
REGISTER_CLK(VLD_CLK);
REGISTER_CLK(NAND);
REGISTER_CLK(RESERVED0);
REGISTER_CLK(VGHL_PWM);
REGISTER_CLK(LED_PWM);
REGISTER_CLK(UART1);
REGISTER_CLK(SDIO);
REGISTER_CLK(ASYNC_FIFO);
REGISTER_CLK(STREAM);
REGISTER_CLK(RTC);
REGISTER_CLK(UART0);
REGISTER_CLK(RANDOM_NUM_GEN);
REGISTER_CLK(SMART_CARD_MPEG_DOMAIN);
REGISTER_CLK(SMART_CARD);
REGISTER_CLK(SAR_ADC);
REGISTER_CLK(I2C);
REGISTER_CLK(IR_REMOTE);
REGISTER_CLK(_1200XXX);
REGISTER_CLK(SATA);
REGISTER_CLK(SPI1);
REGISTER_CLK(USB1);
REGISTER_CLK(USB0);
REGISTER_CLK(VI_CORE);
REGISTER_CLK(LCD);
REGISTER_CLK(ENC480P_MPEG_DOMAIN);
REGISTER_CLK(ENC480I);
REGISTER_CLK(VENC_MISC);
REGISTER_CLK(ENC480P);
REGISTER_CLK(HDMI);
REGISTER_CLK(VCLK3_DAC);
REGISTER_CLK(VCLK3_MISC);
REGISTER_CLK(VCLK3_DVI);
REGISTER_CLK(VCLK2_VIU);
REGISTER_CLK(VCLK2_VENC_DVI);
REGISTER_CLK(VCLK2_VENC_ENC480P);
REGISTER_CLK(VCLK2_VENC_BIST);
REGISTER_CLK(VCLK1_VENC_656);
REGISTER_CLK(VCLK1_VENC_DVI);
REGISTER_CLK(VCLK1_VENC_ENCI);
REGISTER_CLK(VCLK1_VENC_BIST);
REGISTER_CLK(VIDEO_IN);
REGISTER_CLK(WIFI);

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
        .dev_id = "clk_other_pll",
        .clk    = &clk_other_pll,
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
    CLK_LOOKUP_ITEM(AHB_BRIDGE),
    CLK_LOOKUP_ITEM(AHB_SRAM),
    CLK_LOOKUP_ITEM(AIU_ADC),
    CLK_LOOKUP_ITEM(AIU_MIXER_REG),
    CLK_LOOKUP_ITEM(AIU_AUD_MIXER),
    CLK_LOOKUP_ITEM(AIU_AIFIFO2),
    CLK_LOOKUP_ITEM(AIU_AMCLK_MEASURE),
    CLK_LOOKUP_ITEM(AIU_I2S_OUT),
    CLK_LOOKUP_ITEM(AIU_IEC958),
    CLK_LOOKUP_ITEM(AIU_AI_TOP_GLUE),
    CLK_LOOKUP_ITEM(AIU_AUD_DAC),
    CLK_LOOKUP_ITEM(AIU_ICE958_AMCLK),
    CLK_LOOKUP_ITEM(AIU_I2S_DAC_AMCLK),
    CLK_LOOKUP_ITEM(AIU_I2S_SLOW),
    CLK_LOOKUP_ITEM(AIU_AUD_DAC_CLK),
    CLK_LOOKUP_ITEM(ASSIST_MISC),
    CLK_LOOKUP_ITEM(AMRISC),
    CLK_LOOKUP_ITEM(AUD_BUF),
    CLK_LOOKUP_ITEM(AUD_IN),
    CLK_LOOKUP_ITEM(BLK_MOV),
    CLK_LOOKUP_ITEM(BT656_IN),
    CLK_LOOKUP_ITEM(DEMUX),
    CLK_LOOKUP_ITEM(MMC_DDR),
    CLK_LOOKUP_ITEM(DDR),
    CLK_LOOKUP_ITEM(ETHERNET),
    CLK_LOOKUP_ITEM(GE2D),
    CLK_LOOKUP_ITEM(HDMI_MPEG_DOMAIN),
    CLK_LOOKUP_ITEM(HIU_PARSER_TOP),
    CLK_LOOKUP_ITEM(HIU_PARSER),
    CLK_LOOKUP_ITEM(ISA),
    CLK_LOOKUP_ITEM(MEDIA_CPU),
    CLK_LOOKUP_ITEM(MISC_USB0_TO_DDR),
    CLK_LOOKUP_ITEM(MISC_USB1_TO_DDR),
    CLK_LOOKUP_ITEM(MISC_SATA_TO_DDR),
    CLK_LOOKUP_ITEM(AHB_CONTROL_BUS),
    CLK_LOOKUP_ITEM(AHB_DATA_BUS),
    CLK_LOOKUP_ITEM(AXI_BUS),
    CLK_LOOKUP_ITEM(ROM_CLK),
    CLK_LOOKUP_ITEM(EFUSE),
    CLK_LOOKUP_ITEM(AHB_ARB0),
    CLK_LOOKUP_ITEM(RESET),
    CLK_LOOKUP_ITEM(MDEC_CLK_PIC_DC),
    CLK_LOOKUP_ITEM(MDEC_CLK_DBLK),
    CLK_LOOKUP_ITEM(MDEC_CLK_PSC),
    CLK_LOOKUP_ITEM(MDEC_CLK_ASSIST),
    CLK_LOOKUP_ITEM(MC_CLK),
    CLK_LOOKUP_ITEM(IQIDCT_CLK),
    CLK_LOOKUP_ITEM(VLD_CLK),
    CLK_LOOKUP_ITEM(NAND),
    CLK_LOOKUP_ITEM(RESERVED0),
    CLK_LOOKUP_ITEM(VGHL_PWM),
    CLK_LOOKUP_ITEM(LED_PWM),
    CLK_LOOKUP_ITEM(UART1),
    CLK_LOOKUP_ITEM(SDIO),
    CLK_LOOKUP_ITEM(ASYNC_FIFO),
    CLK_LOOKUP_ITEM(STREAM),
    CLK_LOOKUP_ITEM(RTC),
    CLK_LOOKUP_ITEM(UART0),
    CLK_LOOKUP_ITEM(RANDOM_NUM_GEN),
    CLK_LOOKUP_ITEM(SMART_CARD_MPEG_DOMAIN),
    CLK_LOOKUP_ITEM(SMART_CARD),
    CLK_LOOKUP_ITEM(SAR_ADC),
    CLK_LOOKUP_ITEM(I2C),
    CLK_LOOKUP_ITEM(IR_REMOTE),
    CLK_LOOKUP_ITEM(_1200XXX),
    CLK_LOOKUP_ITEM(SATA),
    CLK_LOOKUP_ITEM(SPI1),
    CLK_LOOKUP_ITEM(USB1),
    CLK_LOOKUP_ITEM(USB0),
    CLK_LOOKUP_ITEM(VI_CORE),
    CLK_LOOKUP_ITEM(LCD),
    CLK_LOOKUP_ITEM(ENC480P_MPEG_DOMAIN),
    CLK_LOOKUP_ITEM(ENC480I),
    CLK_LOOKUP_ITEM(VENC_MISC),
    CLK_LOOKUP_ITEM(ENC480P),
    CLK_LOOKUP_ITEM(HDMI),
    CLK_LOOKUP_ITEM(VCLK3_DAC),
    CLK_LOOKUP_ITEM(VCLK3_MISC),
    CLK_LOOKUP_ITEM(VCLK3_DVI),
    CLK_LOOKUP_ITEM(VCLK2_VIU),
    CLK_LOOKUP_ITEM(VCLK2_VENC_DVI),
    CLK_LOOKUP_ITEM(VCLK2_VENC_ENC480P),
    CLK_LOOKUP_ITEM(VCLK2_VENC_BIST),
    CLK_LOOKUP_ITEM(VCLK1_VENC_656),
    CLK_LOOKUP_ITEM(VCLK1_VENC_DVI),
    CLK_LOOKUP_ITEM(VCLK1_VENC_ENCI),
    CLK_LOOKUP_ITEM(VCLK1_VENC_BIST),
    CLK_LOOKUP_ITEM(VIDEO_IN),
    CLK_LOOKUP_ITEM(WIFI) 
};

static int __init meson_clock_init(void)
{
    if (init_clock && init_clock!=a9_clk.rate) {
        if (sys_clkpll_setting(0,init_clock<<1)==0) {
            a9_clk.rate = init_clock;
            clk_sys_pll.rate = init_clock<<1;
        }
    }

    /* Register the lookups */
    clkdev_add_table(lookups,ARRAY_SIZE(lookups));

    return 0;
}

/* initialize clocking early to be available later in the boot */
core_initcall(meson_clock_init);

unsigned long long clkparse(const char *ptr, char **retptr)
{
    char *endptr;	/* local pointer to end of parsed string */

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

    if (retptr)
        *retptr = endptr;

    return ret;
}

static int __init a9_clock_setup(char *ptr)
{
    init_clock = clkparse(ptr, 0);

    if (sys_clkpll_setting(0, init_clock<<1) == 0) {
        a9_clk.rate = init_clock;
        clk_sys_pll.rate=init_clock<<1;
    }

    return 0;
}
__setup("a9_clk=", a9_clock_setup);

static int __init clk81_clock_setup(char *ptr)
{
    int clock = clkparse(ptr,0);

    if (other_pll_setting(0, clock*4) == 0) {
        /* todo: uart baudrate depends on clk81, assume 115200 baudrate */
        int baudrate = (clock / (115200 * 4)) - 1;

        clk_other_pll.rate = clock * 4;
        clk81.rate = clock;

        WRITE_MPEG_REG(HHI_MPEG_CLK_CNTL,   // MPEG clk81 set to other/4
                  (1 << 12) |               // select other PLL
                  ((4 - 1) << 0 ) |         // div1
                  (1 << 7 ) |               // cntl_hi_mpeg_div_en, enable gating
                  (1 << 8 ));               // Connect clk81 to the PLL divider output

        CLEAR_CBUS_REG_MASK(UART0_CONTROL, (1 << 19) | 0xFFF);
        SET_CBUS_REG_MASK(UART0_CONTROL, (baudrate & 0xfff));

        CLEAR_CBUS_REG_MASK(UART1_CONTROL, (1 << 19) | 0xFFF);
        SET_CBUS_REG_MASK(UART1_CONTROL, (baudrate & 0xfff));
    }

    return 0;
}
__setup("clk81=", clk81_clock_setup);

int clk_enable(struct clk *clk)
{
    unsigned long flags;

    spin_lock_irqsave(&clockfw_lock, flags);

    if (clk->clock_index>=0 && clk->clock_index<GCLK_IDX_MAX && clk->clock_gate_reg_adr!=0) {
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

    if (clk->clock_index>=0 && clk->clock_index<GCLK_IDX_MAX && clk->clock_gate_reg_adr!=0) {
        if ((GCLK_ref[clk->clock_index] != 0) && (--GCLK_ref[clk->clock_index] == 0))
            CLEAR_CBUS_REG_MASK(clk->clock_gate_reg_adr, clk->clock_gate_reg_mask); 
    }

    spin_unlock_irqrestore(&clockfw_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

