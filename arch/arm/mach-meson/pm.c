/*
 * Meson Power Management Routines
 *
 * Copyright (C) 2010 Amlogic, Inc. http://www.amlogic.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/fs.h>

#include <asm/cacheflush.h>
#include <asm/delay.h>
#include <asm/uaccess.h>

#include <mach/pm.h>
#include <mach/am_regs.h>
#include <mach/sram.h>
#include <mach/power_gate.h>
#include <mach/gpio.h>
#include <mach/pctl.h>
#include <mach/clock.h>

#ifdef CONFIG_WAKELOCK
#include <linux/wakelock.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend early_suspend;
static int early_suspend_flag = 0;
#endif

#define ON  1
#define OFF 0

#include "sleep.h"
#define EARLY_SUSPEND_USE_XTAL

static void (*meson_sram_suspend) (struct meson_pm_config *);
static struct meson_pm_config *pdata;
static int mask_save[4];

static void meson_sram_push(void *dest, void *src, unsigned int size)
{
    int res = 0;
    memcpy(dest, src, size);
    flush_icache_range((unsigned long)dest, (unsigned long)(dest + size));
    res = memcmp(dest,src,size);
    printk("compare code in sram addr = 0x%x, size = 0x%x, result = %d", (unsigned)dest, size, res);
}

#define GATE_OFF(_MOD) do {power_gate_flag[GCLK_IDX_##_MOD] = IS_CLK_GATE_ON(_MOD);CLK_GATE_OFF(_MOD);} while(0)
#define GATE_ON(_MOD) do {if (power_gate_flag[GCLK_IDX_##_MOD]) CLK_GATE_ON(_MOD);} while(0)
#define GATE_SWITCH(flag, _MOD) do {if (flag) GATE_ON(_MOD); else GATE_OFF(_MOD);} while(0)
static int power_gate_flag[GCLK_IDX_MAX];

void power_gate_init(void)
{
    GATE_INIT(AHB_BRIDGE);
    GATE_INIT(AHB_SRAM);
    GATE_INIT(AIU_ADC);
    GATE_INIT(AIU_MIXER_REG);
    GATE_INIT(AIU_AUD_MIXER);
    GATE_INIT(AIU_AIFIFO2);
    GATE_INIT(AIU_AMCLK_MEASURE);
    GATE_INIT(AIU_I2S_OUT);
    GATE_INIT(AIU_IEC958);
    GATE_INIT(AIU_AI_TOP_GLUE);
    GATE_INIT(AIU_AUD_DAC);
    GATE_INIT(AIU_ICE958_AMCLK);
    GATE_INIT(AIU_I2S_DAC_AMCLK);
    GATE_INIT(AIU_I2S_SLOW);
    GATE_INIT(AIU_AUD_DAC_CLK);
    GATE_INIT(ASSIST_MISC);
    GATE_INIT(AMRISC);
    GATE_INIT(AUD_BUF);
    GATE_INIT(AUD_IN);
    GATE_INIT(BLK_MOV);
    GATE_INIT(BT656_IN);
    GATE_INIT(DEMUX);
    GATE_INIT(MMC_DDR);
    GATE_INIT(DDR);
    GATE_INIT(ETHERNET);
    GATE_INIT(GE2D);
    GATE_INIT(HDMI_MPEG_DOMAIN);
    GATE_INIT(HIU_PARSER_TOP);
    GATE_INIT(HIU_PARSER);
    GATE_INIT(ISA);
    GATE_INIT(MEDIA_CPU);
    GATE_INIT(MISC_USB0_TO_DDR);
    GATE_INIT(MISC_USB1_TO_DDR);
    GATE_INIT(MISC_SATA_TO_DDR);
    GATE_INIT(AHB_CONTROL_BUS);
    GATE_INIT(AHB_DATA_BUS);
    GATE_INIT(AXI_BUS);
    GATE_INIT(ROM_CLK);
    GATE_INIT(EFUSE);
    GATE_INIT(AHB_ARB0);
    GATE_INIT(RESET);
    GATE_INIT(MDEC_CLK_PIC_DC);
    GATE_INIT(MDEC_CLK_DBLK);
    GATE_INIT(MDEC_CLK_PSC);
    GATE_INIT(MDEC_CLK_ASSIST);
    GATE_INIT(MC_CLK);
    GATE_INIT(IQIDCT_CLK);
    GATE_INIT(VLD_CLK);
    GATE_INIT(NAND);
    GATE_INIT(RESERVED0);
    GATE_INIT(VGHL_PWM);
    GATE_INIT(LED_PWM);
    GATE_INIT(UART1);
    GATE_INIT(SDIO);
    GATE_INIT(ASYNC_FIFO);
    GATE_INIT(STREAM);
    GATE_INIT(RTC);
    GATE_INIT(UART0);
    GATE_INIT(RANDOM_NUM_GEN);
    GATE_INIT(SMART_CARD_MPEG_DOMAIN);
    GATE_INIT(SMART_CARD);
    GATE_INIT(SAR_ADC);
    GATE_INIT(I2C);
    GATE_INIT(IR_REMOTE);
    GATE_INIT(_1200XXX);
    GATE_INIT(SATA);
    GATE_INIT(SPI1);
    GATE_INIT(SPI2);
    GATE_INIT(USB1);
    GATE_INIT(USB0);
    GATE_INIT(VI_CORE);
    GATE_INIT(LCD);
    GATE_INIT(ENC480P_MPEG_DOMAIN);
    GATE_INIT(ENC480I);
    GATE_INIT(VENC_MISC);
    GATE_INIT(ENC480P);
    GATE_INIT(HDMI);
    GATE_INIT(VCLK3_DAC);
    GATE_INIT(VCLK3_MISC);
    GATE_INIT(VCLK3_DVI);
    GATE_INIT(VCLK2_VIU);
    GATE_INIT(VCLK2_VENC_DVI);
    GATE_INIT(VCLK2_VENC_ENC480P);
    GATE_INIT(VCLK2_VENC_BIST);
    GATE_INIT(VCLK1_VENC_656);
    GATE_INIT(VCLK1_VENC_DVI);
    GATE_INIT(VCLK1_VENC_ENCI);
    GATE_INIT(VCLK1_VENC_BIST);
    GATE_INIT(VIDEO_IN);
    GATE_INIT(WIFI);
}

void power_init_off(void)
{
    GATE_OFF(BT656_IN);
    GATE_OFF(VIDEO_IN);
    GATE_OFF(GE2D);
    GATE_OFF(DEMUX);
    GATE_OFF(ETHERNET);
    GATE_OFF(WIFI);
    CLEAR_CBUS_REG_MASK(HHI_DEMOD_CLK_CNTL, (1<<8));
    CLEAR_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, (1<<8));
    CLEAR_CBUS_REG_MASK(HHI_ETH_CLK_CNTL, (1<<8));
    CLEAR_CBUS_REG_MASK(HHI_WIFI_CLK_CNTL, (1<<0));
    SET_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL, (1<<15));
}

void power_gate_switch(int flag)
{
#ifndef ADJUST_CORE_VOLTAGE   
	GATE_SWITCH(flag, LED_PWM);
#endif
    GATE_SWITCH(flag, VGHL_PWM);
    GATE_SWITCH(flag, VI_CORE);
    GATE_SWITCH(flag, MDEC_CLK_PIC_DC);
    GATE_SWITCH(flag, MDEC_CLK_DBLK);
    GATE_SWITCH(flag, MDEC_CLK_PSC);
    GATE_SWITCH(flag, MDEC_CLK_ASSIST);
    GATE_SWITCH(flag, MC_CLK);
    GATE_SWITCH(flag, IQIDCT_CLK);
    GATE_SWITCH(flag, VLD_CLK);
    //GATE_SWITCH(flag, AHB_BRIDGE);
    //GATE_SWITCH(flag, AHB_SRAM);
    GATE_SWITCH(flag, AIU_ADC);
    GATE_SWITCH(flag, AIU_MIXER_REG);
    GATE_SWITCH(flag, AIU_AUD_MIXER);
    GATE_SWITCH(flag, AIU_AIFIFO2);
    GATE_SWITCH(flag, AIU_AMCLK_MEASURE);
    GATE_SWITCH(flag, AIU_I2S_OUT);
    GATE_SWITCH(flag, AIU_IEC958);
    GATE_SWITCH(flag, AIU_AI_TOP_GLUE);
    GATE_SWITCH(flag, AIU_AUD_DAC);
    GATE_SWITCH(flag, AIU_ICE958_AMCLK);
    GATE_SWITCH(flag, AIU_I2S_DAC_AMCLK);
    GATE_SWITCH(flag, AIU_I2S_SLOW);
    GATE_SWITCH(flag, AIU_AUD_DAC_CLK);
    //GATE_SWITCH(flag, ASSIST_MISC);
    GATE_SWITCH(flag, AUD_BUF);
    GATE_SWITCH(flag, BLK_MOV);
    GATE_SWITCH(flag, DEMUX);
    //GATE_SWITCH(flag, MMC_DDR);
    //GATE_SWITCH(flag, DDR);
    GATE_SWITCH(flag, ETHERNET);
    GATE_SWITCH(flag, HDMI_MPEG_DOMAIN);
    //GATE_SWITCH(flag, HIU_PARSER);
    GATE_SWITCH(flag, HIU_PARSER_TOP);
    //GATE_SWITCH(flag, ISA);
    GATE_SWITCH(flag, MEDIA_CPU);
    GATE_SWITCH(flag, MISC_USB0_TO_DDR);
    GATE_SWITCH(flag, MISC_USB1_TO_DDR);
    GATE_SWITCH(flag, MISC_SATA_TO_DDR);
    //GATE_SWITCH(flag, AHB_CONTROL_BUS);
    //GATE_SWITCH(flag, AHB_DATA_BUS);
    //GATE_SWITCH(flag, AXI_BUS);
    //GATE_SWITCH(flag, AHB_ARB0);
    //GATE_SWITCH(flag, RESET);
    GATE_SWITCH(flag, NAND);
    GATE_SWITCH(flag, RESERVED0);
    //GATE_SWITCH(flag, UART1);
    GATE_SWITCH(flag, SDIO);
    GATE_SWITCH(flag, ASYNC_FIFO);
    GATE_SWITCH(flag, STREAM);
    //GATE_SWITCH(flag, RTC);
    //GATE_SWITCH(flag, UART0);
    GATE_SWITCH(flag, RANDOM_NUM_GEN);
    GATE_SWITCH(flag, SMART_CARD_MPEG_DOMAIN);
    GATE_SWITCH(flag, SMART_CARD);
    GATE_SWITCH(flag, SAR_ADC);
    GATE_SWITCH(flag, I2C);
    GATE_SWITCH(flag, IR_REMOTE);
    //GATE_SWITCH(flag, _1200XXX);
    GATE_SWITCH(flag, SATA);
    GATE_SWITCH(flag, SPI1);
    GATE_SWITCH(flag, SPI2);
    GATE_SWITCH(flag, USB1);
    GATE_SWITCH(flag, USB0);
    GATE_SWITCH(flag, WIFI);
}
EXPORT_SYMBOL(power_gate_switch);

void early_power_gate_switch(int flag)
{
    GATE_SWITCH(flag, AMRISC);
    GATE_SWITCH(flag, AUD_IN);
    GATE_SWITCH(flag, BLK_MOV);
    GATE_SWITCH(flag, BT656_IN);
    GATE_SWITCH(flag, GE2D);
    GATE_SWITCH(flag, ROM_CLK);
    GATE_SWITCH(flag, EFUSE);
    GATE_SWITCH(flag, RESERVED0);
    GATE_SWITCH(flag, LCD);
    GATE_SWITCH(flag, ENC480P_MPEG_DOMAIN);
    GATE_SWITCH(flag, ENC480I);
    GATE_SWITCH(flag, VENC_MISC);
    GATE_SWITCH(flag, ENC480P);
    GATE_SWITCH(flag, HDMI);
    GATE_SWITCH(flag, VCLK3_DAC);
    GATE_SWITCH(flag, VCLK3_MISC);
    GATE_SWITCH(flag, VCLK3_DVI);
    GATE_SWITCH(flag, VCLK2_VIU);
    GATE_SWITCH(flag, VCLK2_VENC_DVI);
    GATE_SWITCH(flag, VCLK2_VENC_ENC480P);
    GATE_SWITCH(flag, VCLK2_VENC_BIST);
    GATE_SWITCH(flag, VCLK1_VENC_656);
    GATE_SWITCH(flag, VCLK1_VENC_DVI);
    GATE_SWITCH(flag, VCLK1_VENC_ENCI);
    GATE_SWITCH(flag, VCLK1_VENC_BIST);
    GATE_SWITCH(flag, VIDEO_IN);
}
EXPORT_SYMBOL(early_power_gate_switch);

#define CLK_COUNT 9
static char clk_flag[CLK_COUNT];
static unsigned clks[CLK_COUNT]={
    HHI_DEMOD_CLK_CNTL,
    HHI_SATA_CLK_CNTL,
    HHI_ETH_CLK_CNTL,
    HHI_WIFI_CLK_CNTL,
    HHI_VID_CLK_CNTL,
    HHI_AUD_CLK_CNTL,
    HHI_MALI_CLK_CNTL,
    HHI_HDMI_CLK_CNTL,
    HHI_MPEG_CLK_CNTL,
};

static char clks_name[CLK_COUNT][32]={
    "HHI_DEMOD_CLK_CNTL",
    "HHI_SATA_CLK_CNTL",
    "HHI_ETH_CLK_CNTL",
    "HHI_WIFI_CLK_CNTL",
    "HHI_VID_CLK_CNTL",
    "HHI_AUD_CLK_CNTL",
    "HHI_MALI_CLK_CNTL",
    "HHI_HDMI_CLK_CNTL",
    "HHI_MPEG_CLK_CNTL",
};

#ifdef EARLY_SUSPEND_USE_XTAL
#define EARLY_CLK_COUNT 6
#else
#define EARLY_CLK_COUNT 5
#endif
static char early_clk_flag[EARLY_CLK_COUNT];
static unsigned early_clks[EARLY_CLK_COUNT]={
    HHI_DEMOD_CLK_CNTL,
    HHI_SATA_CLK_CNTL,
    HHI_ETH_CLK_CNTL,
    HHI_WIFI_CLK_CNTL,
    HHI_VID_CLK_CNTL,
#ifdef EARLY_SUSPEND_USE_XTAL
    HHI_MPEG_CLK_CNTL,
#endif
};

static char early_clks_name[EARLY_CLK_COUNT][32]={
    "HHI_DEMOD_CLK_CNTL",
    "HHI_SATA_CLK_CNTL",
    "HHI_ETH_CLK_CNTL",
    "HHI_WIFI_CLK_CNTL",
    "HHI_VID_CLK_CNTL",
#ifdef EARLY_SUSPEND_USE_XTAL
    "HHI_MPEG_CLK_CNTL",
#endif
};

static unsigned uart_rate_backup;
static unsigned xtal_uart_rate_backup;
void clk_switch(int flag)
{
    int i;

    if (flag){
        for (i=CLK_COUNT-1;i>=0;i--){
            if (clk_flag[i]){
                if ((clks[i] == HHI_VID_CLK_CNTL)||(clks[i] == HHI_WIFI_CLK_CNTL)){
                    SET_CBUS_REG_MASK(clks[i], 1);
                }
                else if (clks[i] == HHI_MPEG_CLK_CNTL){
                    udelay(1000);
                    SET_CBUS_REG_MASK(clks[i], (1<<8)); // normal

                    CLEAR_CBUS_REG_MASK(UART0_CONTROL, (1 << 19) | 0xFFF);
                    SET_CBUS_REG_MASK(UART0_CONTROL, (((uart_rate_backup / (115200 * 4)) - 1) & 0xfff));
                    CLEAR_CBUS_REG_MASK(UART1_CONTROL, (1 << 19) | 0xFFF);
                    SET_CBUS_REG_MASK(UART1_CONTROL, (((uart_rate_backup / (115200 * 4)) - 1) & 0xfff));
                }
                else{
                    SET_CBUS_REG_MASK(clks[i], (1<<8));
                }
                clk_flag[i] = 0;
                printk(KERN_INFO "clk %s(%x) on\n", clks_name[i], clks[i]);
            }
        }
    }
    else{
        for (i=0;i<CLK_COUNT;i++){
            if ((clks[i] == HHI_VID_CLK_CNTL)||(clks[i] == HHI_WIFI_CLK_CNTL)){
                clk_flag[i] = READ_CBUS_REG_BITS(clks[i], 0, 1);
                if (clk_flag[i]){
                    CLEAR_CBUS_REG_MASK(clks[i], 1);
                }
            }
            else if (clks[i] == HHI_MPEG_CLK_CNTL){
                if (READ_CBUS_REG(clks[i])&(1<<8)){
                    clk_flag[i] = 1;
                    
                    udelay(1000);
                    CLEAR_CBUS_REG_MASK(clks[i], (1<<8)); // 24M
                    
                    CLEAR_CBUS_REG_MASK(UART0_CONTROL, (1 << 19) | 0xFFF);
                    SET_CBUS_REG_MASK(UART0_CONTROL, (((xtal_uart_rate_backup / (115200 * 4)) - 1) & 0xfff));
                    CLEAR_CBUS_REG_MASK(UART1_CONTROL, (1 << 19) | 0xFFF);
                    SET_CBUS_REG_MASK(UART1_CONTROL, (((xtal_uart_rate_backup / (115200 * 4)) - 1) & 0xfff));
                }
            }
            else{
                clk_flag[i] = READ_CBUS_REG_BITS(clks[i], 8, 1) ? 1 : 0;
                if (clk_flag[i])    
                    CLEAR_CBUS_REG_MASK(clks[i], (1<<8));
            }
            if (clk_flag[i])
                printk(KERN_INFO "clk %s(%x) off\n", clks_name[i], clks[i]);
        }
    }
}
EXPORT_SYMBOL(clk_switch);

void early_clk_switch(int flag)
{
    int i;
    struct clk *sys_clk;

    if (flag){
        for (i=EARLY_CLK_COUNT-1;i>=0;i--){
            if (early_clk_flag[i]){
                if ((early_clks[i] == HHI_VID_CLK_CNTL)||(early_clks[i] == HHI_WIFI_CLK_CNTL)){
                    SET_CBUS_REG_MASK(early_clks[i], 1);
                }
                else if (early_clks[i] == HHI_MPEG_CLK_CNTL){
                    udelay(1000);
                    SET_CBUS_REG_MASK(early_clks[i], (1<<8)); // clk81 back to normal
                    
                    CLEAR_CBUS_REG_MASK(UART0_CONTROL, (1 << 19) | 0xFFF);
                    SET_CBUS_REG_MASK(UART0_CONTROL, (((uart_rate_backup / (115200 * 4)) - 1) & 0xfff));
                    CLEAR_CBUS_REG_MASK(UART1_CONTROL, (1 << 19) | 0xFFF);
                    SET_CBUS_REG_MASK(UART1_CONTROL, (((uart_rate_backup / (115200 * 4)) - 1) & 0xfff));
                }
                else{
                    SET_CBUS_REG_MASK(early_clks[i], (1<<8));
                }
                printk(KERN_INFO "late clk %s(%x) on\n", early_clks_name[i], early_clks[i]);
                early_clk_flag[i] = 0;
            }
        }
    }
    else{
        sys_clk = clk_get_sys("clk81", NULL);
        uart_rate_backup = sys_clk->rate;
        sys_clk = clk_get_sys("clk_xtal", NULL);
        xtal_uart_rate_backup = sys_clk->rate;

        for (i=0;i<EARLY_CLK_COUNT;i++){
            if ((early_clks[i] == HHI_VID_CLK_CNTL)||(early_clks[i] == HHI_WIFI_CLK_CNTL)){
                early_clk_flag[i] = READ_CBUS_REG_BITS(early_clks[i], 0, 1);
                if (early_clk_flag[i]){
                    CLEAR_CBUS_REG_MASK(early_clks[i], 1);
                }
            }
            else if (early_clks[i] == HHI_MPEG_CLK_CNTL){
                early_clk_flag[i] = 1;
                 
                udelay(1000);
                CLEAR_CBUS_REG_MASK(early_clks[i], (1<<8)); // 24M
                   
                CLEAR_CBUS_REG_MASK(UART0_CONTROL, (1 << 19) | 0xFFF);
                SET_CBUS_REG_MASK(UART0_CONTROL, (((xtal_uart_rate_backup / (115200 * 4)) - 1) & 0xfff));
                CLEAR_CBUS_REG_MASK(UART1_CONTROL, (1 << 19) | 0xFFF);
                SET_CBUS_REG_MASK(UART1_CONTROL, (((xtal_uart_rate_backup / (115200 * 4)) - 1) & 0xfff)); 
            }
            else{
                early_clk_flag[i] = READ_CBUS_REG_BITS(early_clks[i], 8, 1) ? 1 : 0;
                if (early_clk_flag[i]){
                    CLEAR_CBUS_REG_MASK(early_clks[i], (1<<8));
                }
            }
            if (early_clk_flag[i])
                printk(KERN_INFO "early clk %s(%x) off\n", early_clks_name[i], early_clks[i]);
        }
    }
}
EXPORT_SYMBOL(early_clk_switch);

#define PLL_COUNT 4
static char pll_flag[PLL_COUNT];
static unsigned plls[PLL_COUNT]={
    HHI_DEMOD_PLL_CNTL,
    HHI_VID_PLL_CNTL,
    HHI_AUD_PLL_CNTL,
    HHI_OTHER_PLL_CNTL,
};

static char plls_name[PLL_COUNT][32]={
    "HHI_DEMOD_PLL_CNTL",
    "HHI_VID_PLL_CNTL",
    "HHI_AUD_PLL_CNTL",
    "HHI_OTHER_PLL_CNTL",
};

#define EARLY_PLL_COUNT 2
static char early_pll_flag[EARLY_PLL_COUNT];
static unsigned early_plls[EARLY_PLL_COUNT]={
    HHI_DEMOD_PLL_CNTL,
    HHI_VID_PLL_CNTL,
};

static char early_plls_name[EARLY_PLL_COUNT][32]={
    "HHI_DEMOD_PLL_CNTL",
    "HHI_VID_PLL_CNTL",
};

void pll_switch(int flag)
{
    int i;
    if (flag){
        for (i=PLL_COUNT-1;i>=0;i--){
            if (pll_flag[i]) {
                CLEAR_CBUS_REG_MASK(plls[i], (1<<15));
                pll_flag[i] = 0;
                printk(KERN_INFO "pll %s(%x) on\n", plls_name[i], plls[i]);
            }
        }
        udelay(1000);
    }
    else{
        for (i=0;i<PLL_COUNT;i++){
            pll_flag[i] = READ_CBUS_REG_BITS(plls[i], 15, 1) ? 0 : 1;
            if (pll_flag[i]){
                printk(KERN_INFO "pll %s(%x) off\n", plls_name[i], plls[i]);
                SET_CBUS_REG_MASK(plls[i], (1<<15));
            }
        }
    }
}
EXPORT_SYMBOL(pll_switch);

void early_pll_switch(int flag)
{
    int i;
    if (flag){
        for (i=EARLY_PLL_COUNT-1;i>=0;i--){
            if (early_pll_flag[i]) {
                CLEAR_CBUS_REG_MASK(early_plls[i], (1<<15));
                early_pll_flag[i] = 0;
                printk(KERN_INFO "late pll %s(%x) on\n", early_plls_name[i], early_plls[i]);
            }
        }
        udelay(1000);
    }
    else{
        for (i=0;i<EARLY_PLL_COUNT;i++){
            early_pll_flag[i] = READ_CBUS_REG_BITS(early_plls[i], 15, 1) ? 0 : 1;
            if (early_pll_flag[i]){
                printk(KERN_INFO "early pll %s(%x) off\n", early_plls_name[i], early_plls[i]);
                SET_CBUS_REG_MASK(early_plls[i], (1<<15));
            }
        }
    }
}
EXPORT_SYMBOL(early_pll_switch);

typedef struct {
	char name[32];
	unsigned reg_addr;
	unsigned set_bits;
	unsigned clear_bits;
	unsigned reg_value;
	unsigned enable; // 1:cbus 2:apb 3:ahb 0:disable
} analog_t;

#define ANALOG_COUNT	8
static analog_t analog_regs[ANALOG_COUNT] = {
	{"SAR_ADC",				SAR_ADC_REG3, 		1<<28,			(1<<30)|(1<<21), 	0,	1},
	{"LED_PWM_REG0",		LED_PWM_REG0, 		1<<13,			1<<12,				0,	0}, // needed for core voltage adjustment, so not off
	{"VGHL_PWM_REG0",		VGHL_PWM_REG0, 		1<<13,			1<<12,				0,	1},
	{"WIFI_ADC_SAMPLING",	WIFI_ADC_SAMPLING, 	0,				1<<18,				0,	1},
	{"ADC_EN_ADC",			ADC_EN_ADC,			0,				1<<31,				0,	2},
	{"WIFI_ADC_DAC",		WIFI_ADC_DAC,		(3<<10)|0xff,	0,					0,	3},
	{"ADC_EN_CMLGEN_RES",	ADC_EN_CMLGEN_RES,	0,				(1<<26)|(1<<25),	0, 	3},
	{"WIFI_SARADC",			WIFI_SARADC,		0,				1<<2,				0, 	3},
};


void analog_switch(int flag)
{
	int i;
	unsigned reg_value = 0;

    if (flag){
        printk(KERN_INFO "analog on\n");
        SET_CBUS_REG_MASK(AM_ANALOG_TOP_REG0, 1<<1);  		// set 0x206e bit[1] 1 to power on top analog
		for (i=0;i<ANALOG_COUNT;i++){
			if (analog_regs[i].enable && (analog_regs[i].set_bits || analog_regs[i].clear_bits)){
				if (analog_regs[i].enable == 1)
					WRITE_CBUS_REG(analog_regs[i].reg_addr, analog_regs[i].reg_value);
				else if (analog_regs[i].enable == 2)
					WRITE_APB_REG(analog_regs[i].reg_addr, analog_regs[i].reg_value);
				else if (analog_regs[i].enable == 3)
					WRITE_AHB_REG(analog_regs[i].reg_addr, analog_regs[i].reg_value);
			}
		}
    }
    else{
        printk(KERN_INFO "analog off\n");
		for (i=0;i<ANALOG_COUNT;i++){
			if (analog_regs[i].enable && (analog_regs[i].set_bits || analog_regs[i].clear_bits)){
				if (analog_regs[i].enable == 1){
					analog_regs[i].reg_value = READ_CBUS_REG(analog_regs[i].reg_addr);
					printk("%s(0x%x):0x%x", analog_regs[i].name, CBUS_REG_ADDR(analog_regs[i].reg_addr), analog_regs[i].reg_value);
					if (analog_regs[i].clear_bits){
						CLEAR_CBUS_REG_MASK(analog_regs[i].reg_addr, analog_regs[i].clear_bits);
						printk(" & ~0x%x", analog_regs[i].clear_bits);
					}
					if (analog_regs[i].set_bits){
						SET_CBUS_REG_MASK(analog_regs[i].reg_addr, analog_regs[i].set_bits);
						printk(" | 0x%x", analog_regs[i].set_bits);
					}
					reg_value = READ_CBUS_REG(analog_regs[i].reg_addr);
					printk(" = 0x%x\n", reg_value);
				}
				else if (analog_regs[i].enable == 2){
					analog_regs[i].reg_value = READ_APB_REG(analog_regs[i].reg_addr);
					printk("%s(0x%x):0x%x", analog_regs[i].name, APB_REG_ADDR(analog_regs[i].reg_addr), analog_regs[i].reg_value);
					if (analog_regs[i].clear_bits){
						CLEAR_APB_REG_MASK(analog_regs[i].reg_addr, analog_regs[i].clear_bits);
						printk(" & ~0x%x", analog_regs[i].clear_bits);
					}
					if (analog_regs[i].set_bits){
						SET_APB_REG_MASK(analog_regs[i].reg_addr, analog_regs[i].set_bits);
						printk(" | 0x%x", analog_regs[i].set_bits);
					}
					reg_value = READ_APB_REG(analog_regs[i].reg_addr);
					printk(" = 0x%x\n", reg_value);
				}
				else if (analog_regs[i].enable == 3){
					analog_regs[i].reg_value = READ_AHB_REG(analog_regs[i].reg_addr);
					printk("%s(0x%x):0x%x", analog_regs[i].name, AHB_REG_ADDR(analog_regs[i].reg_addr), analog_regs[i].reg_value);
					if (analog_regs[i].clear_bits){
						CLEAR_AHB_REG_MASK(analog_regs[i].reg_addr, analog_regs[i].clear_bits);
						printk(" & ~0x%x", analog_regs[i].clear_bits);
					}
					if (analog_regs[i].set_bits){
						SET_AHB_REG_MASK(analog_regs[i].reg_addr, analog_regs[i].set_bits);
						printk(" | 0x%x", analog_regs[i].set_bits);
					}
					reg_value = READ_AHB_REG(analog_regs[i].reg_addr);
					printk(" = 0x%x\n", reg_value);
				}
			}
		}
		CLEAR_CBUS_REG_MASK(AM_ANALOG_TOP_REG0, 1<<1);  	// set 0x206e bit[1] 0 to shutdown top analog
    }
}

void usb_switch(int is_on,int ctrl)
{
	int index,por;

	if(ctrl == 0)
		index = USB_CTL_INDEX_A;
	else
		index = USB_CTL_INDEX_B;

	if(is_on)
		por = USB_CTL_POR_ON;
	else 
		por = USB_CTL_POR_OFF;

	set_usb_ctl_por(index,por);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void meson_system_early_suspend(struct early_suspend *h)
{
    if (!early_suspend_flag){
        printk(KERN_INFO "sys_suspend\n");
		if(pdata->set_exgpio_early_suspend){
			pdata->set_exgpio_early_suspend(OFF);
		}
        early_power_gate_switch(OFF);
        early_clk_switch(OFF);
        early_pll_switch(OFF);
        early_suspend_flag=1;
    }
}

static void meson_system_late_resume(struct early_suspend *h)
{
    if (early_suspend_flag){
        early_pll_switch(ON);
        early_clk_switch(ON);
        early_power_gate_switch(ON);
        early_suspend_flag = 0;
		if(pdata->set_exgpio_early_suspend){
			pdata->set_exgpio_early_suspend(ON);
		}
        printk(KERN_INFO "sys_resume\n");
    }
}
#endif

#define         MODE_DELAYED_WAKE       0
#define         MODE_IRQ_DELAYED_WAKE   1
#define         MODE_IRQ_ONLY_WAKE      2

static void auto_clk_gating_setup(
	unsigned long sleep_dly_tb, unsigned long mode, unsigned long clear_fiq, unsigned long clear_irq,
	unsigned long   start_delay,unsigned long   clock_gate_dly,unsigned long   sleep_time,unsigned long   enable_delay)
{
	WRITE_CBUS_REG(HHI_A9_AUTO_CLK0,
		(sleep_dly_tb << 24)    |   // sleep timebase
		(sleep_time << 16)      |   // sleep time                                
		(clear_irq << 5)        |   // clear IRQ
		(clear_fiq << 4)        |   // clear FIQ
		(mode << 2));                // mode
	WRITE_CBUS_REG(HHI_A9_AUTO_CLK1,
		(0 << 20)               |   // start delay timebase
		(enable_delay << 12)    |   // enable delay
		(clock_gate_dly << 8)   |   // clock gate delay
		(start_delay << 0));         // start delay  
	SET_CBUS_REG_MASK(HHI_A9_AUTO_CLK0, 1 << 0);
}

static void meson_pm_suspend(void)
{
#ifdef SAVE_DDR_REGS
    int *p = pdata->ddr_reg_backup;
    int i;
#endif
    unsigned ddr_clk_N;
    unsigned mpeg_clk_backup;

    printk(KERN_INFO "enter meson_pm_suspend!\n");

    pdata->ddr_clk = READ_CBUS_REG(HHI_DDR_PLL_CNTL);

    ddr_clk_N = (pdata->ddr_clk>>9)&0x1f;
    ddr_clk_N = ddr_clk_N*4; // N*4
    if (ddr_clk_N>0x1f)
        ddr_clk_N=0x1f;
    pdata->ddr_clk &= ~(0x1f<<9);
    pdata->ddr_clk |= ddr_clk_N<<9;
    
    printk(KERN_INFO "target ddr clock 0x%x!\n", pdata->ddr_clk);

    analog_switch(OFF);

    usb_switch(OFF,0);
    usb_switch(OFF,1);
    
    if(pdata->set_vccx2){
        pdata->set_vccx2(OFF);
    }

    power_gate_switch(OFF);

#ifdef SAVE_DDR_REGS
    printk("PCTL_TOGCNT1U_ADDR %x\n", READ_APB_REG(PCTL_TOGCNT1U_ADDR));
    printk("PCTL_TOGCNT100N_ADDR %x\n", READ_APB_REG(PCTL_TOGCNT100N_ADDR));
    printk("PCTL_TREFI_ADDR %x\n", READ_APB_REG(PCTL_TREFI_ADDR));
    printk("PCTL_ZQCR_ADDR %x\n", READ_APB_REG(PCTL_ZQCR_ADDR));
    printk("PCTL_ODTCFG_ADDR %x\n", READ_APB_REG(PCTL_ODTCFG_ADDR));
    printk("PCTL_TMRD_ADDR %x\n", READ_APB_REG(PCTL_TMRD_ADDR));
    printk("PCTL_TRFC_ADDR %x\n", READ_APB_REG(PCTL_TRFC_ADDR));
    printk("PCTL_TRP_ADDR %x\n", READ_APB_REG(PCTL_TRP_ADDR));
    printk("PCTL_TAL_ADDR %x\n", READ_APB_REG(PCTL_TAL_ADDR));
    printk("PCTL_TCWL_ADDR %x\n", READ_APB_REG(PCTL_TCWL_ADDR));
    printk("PCTL_TCL_ADDR %x\n", READ_APB_REG(PCTL_TCL_ADDR));
    printk("PCTL_TRAS_ADDR %x\n", READ_APB_REG(PCTL_TRAS_ADDR));
    printk("PCTL_TRC_ADDR %x\n", READ_APB_REG(PCTL_TRC_ADDR));
    printk("PCTL_TRCD_ADDR %x\n", READ_APB_REG(PCTL_TRCD_ADDR));
    printk("PCTL_TRRD_ADDR %x\n", READ_APB_REG(PCTL_TRRD_ADDR));
    printk("PCTL_TRTP_ADDR %x\n", READ_APB_REG(PCTL_TRTP_ADDR));
    printk("PCTL_TWR_ADDR %x\n", READ_APB_REG(PCTL_TWR_ADDR));
    printk("PCTL_TWTR_ADDR %x\n", READ_APB_REG(PCTL_TWTR_ADDR));
    printk("PCTL_TEXSR_ADDR %x\n", READ_APB_REG(PCTL_TEXSR_ADDR));
    printk("PCTL_TXP_ADDR %x\n", READ_APB_REG(PCTL_TXP_ADDR));
    printk("PCTL_TDQS_ADDR %x\n", READ_APB_REG(PCTL_TDQS_ADDR));
    printk("PCTL_MCFG_ADDR %x\n", READ_APB_REG(PCTL_MCFG_ADDR));
    printk("PCTL_RSLR0_ADDR %x\n", READ_APB_REG(PCTL_RSLR0_ADDR));
    printk("PCTL_RDGR0_ADDR %x\n", READ_APB_REG(PCTL_RDGR0_ADDR));
    printk("MMC_DDR_CTRL %x\n", READ_APB_REG(MMC_DDR_CTRL));
#endif

    clk_switch(OFF);
    
    pll_switch(OFF);


    printk("meson_sram_suspend params 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", 
        (unsigned)pdata->pctl_reg_base, (unsigned)pdata->mmc_reg_base, (unsigned)pdata->hiu_reg_base, 
        (unsigned)pdata->power_key, (unsigned)pdata->ddr_clk, (unsigned)pdata->ddr_reg_backup);

    meson_sram_push(meson_sram_suspend, meson_cpu_suspend,
                        meson_cpu_suspend_sz);

    printk(KERN_INFO "sleep ...\n");

    mpeg_clk_backup = READ_CBUS_REG(HHI_MPEG_CLK_CNTL);	// save clk81 ctrl

#ifdef SYSTEM_16K
    if (READ_CBUS_REG(HHI_MPEG_CLK_CNTL)&(1<<8))
        CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1<<8)); // clk81 = xtal
    SET_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1<<9));       // xtal_rtc = rtc
    WRITE_CBUS_REG_BITS(HHI_MPEG_CLK_CNTL, 0x1, 0, 6);	// devider = 2
    WRITE_CBUS_REG_BITS(HHI_MPEG_CLK_CNTL, 0, 12, 2);	// clk81 src -> xtal_rtc
    SET_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1<<8));		// clk81 = xtal_rtc / devider
#else
    if (READ_CBUS_REG(HHI_MPEG_CLK_CNTL)&(1<<8))
        CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1<<8)); // clk81 = xtal
    WRITE_CBUS_REG_BITS(HHI_MPEG_CLK_CNTL, 0x7f, 0, 6);	// devider = 128
    WRITE_CBUS_REG_BITS(HHI_MPEG_CLK_CNTL, 0, 12, 2);	// clk81 src -> xtal_rtc
    SET_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1<<8));		// clk81 = xtal_rtc / devider
#endif
    CLEAR_CBUS_REG_MASK(HHI_A9_CLK_CNTL, (1<<7));		// clka9 = xtal_rtc / 2
#ifdef SYSTEM_16K
	SET_CBUS_REG_MASK(PREG_CTLREG0_ADDR, 1);
#endif
	auto_clk_gating_setup(	2,						    // select 100uS timebase							
							MODE_IRQ_ONLY_WAKE, 	    // Set interrupt wakeup only
							0,						    // don't clear the FIQ global mask
							0,						    // don't clear the IRQ global mask
							1,						    // 1us start delay
							1,						    // 1uS gate delay							  
							1,						    // Set the delay wakeup time (1mS)
							1); 					    // 1uS enable delay 
    SET_CBUS_REG_MASK(HHI_SYS_PLL_CNTL, (1<<15));		// turn off sys pll
     
#if (defined CONFIG_MACH_MESON_8726M_REFC01) || (defined CONFIG_MACH_MESON_8726M_REFC03) || (defined CONFIG_MACH_MESON_8726M_REFC06) 
    WRITE_CBUS_REG(A9_0_IRQ_IN0_INTR_MASK, pdata->power_key);     // enable remote interrupt only
#else
    WRITE_CBUS_REG(A9_0_IRQ_IN2_INTR_MASK, pdata->power_key);     // enable rtc interrupt only
#endif
    meson_sram_suspend(pdata);

    CLEAR_CBUS_REG_MASK(HHI_SYS_PLL_CNTL, (1<<15));		// turn on sys pll
    udelay(10);
#ifdef SYSTEM_16K
	CLEAR_CBUS_REG_MASK(PREG_CTLREG0_ADDR, 1);
#endif
	SET_CBUS_REG_MASK(HHI_A9_CLK_CNTL, (1<<7));			// clka9 = sys pll / devider
	CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1<<8));		// clk81 = xtal
#ifdef SYSTEM_16K
	CLEAR_CBUS_REG_MASK(HHI_MPEG_CLK_CNTL, (1<<9));     // xtal_rtc = xtal
#endif
    WRITE_CBUS_REG(HHI_MPEG_CLK_CNTL, mpeg_clk_backup);	// restore clk81 ctrl

    printk(KERN_INFO "... wake up\n");

    if(pdata->set_vccx2){
        pdata->set_vccx2(ON);
    }
    
    pll_switch(ON);

    clk_switch(ON);

#ifdef SAVE_DDR_REGS
    for (i=0;i<100/4;i++)
        printk("%x\n", p[i]);
#endif
       
    power_gate_switch(ON);

    usb_switch(ON,0);
    usb_switch(ON,1);

    analog_switch(ON);
}

static int meson_pm_prepare(void)
{
    printk(KERN_INFO "enter meson_pm_prepare!\n");
    mask_save[0] = READ_CBUS_REG(A9_0_IRQ_IN0_INTR_MASK);
    mask_save[1] = READ_CBUS_REG(A9_0_IRQ_IN1_INTR_MASK);
    mask_save[2] = READ_CBUS_REG(A9_0_IRQ_IN2_INTR_MASK);
    mask_save[3] = READ_CBUS_REG(A9_0_IRQ_IN3_INTR_MASK);
    WRITE_CBUS_REG(A9_0_IRQ_IN0_INTR_MASK, 0x0);
    WRITE_CBUS_REG(A9_0_IRQ_IN1_INTR_MASK, 0x0);
    WRITE_CBUS_REG(A9_0_IRQ_IN2_INTR_MASK, 0x0);
    WRITE_CBUS_REG(A9_0_IRQ_IN3_INTR_MASK, 0x0);
    meson_sram_push(meson_sram_suspend, meson_cpu_suspend,
                        meson_cpu_suspend_sz);
    return 0;
}

static int meson_pm_enter(suspend_state_t state)
{
    int ret = 0;

    switch (state) {
    case PM_SUSPEND_STANDBY:
    case PM_SUSPEND_MEM:
        meson_pm_suspend();
        break;
    default:
        ret = -EINVAL;
    }

    return ret;
}

static void meson_pm_finish(void)
{
    printk(KERN_INFO "enter meson_pm_finish!\n");
    WRITE_CBUS_REG(A9_0_IRQ_IN0_INTR_MASK, mask_save[0]);
    WRITE_CBUS_REG(A9_0_IRQ_IN1_INTR_MASK, mask_save[1]);
    WRITE_CBUS_REG(A9_0_IRQ_IN2_INTR_MASK, mask_save[2]);
    WRITE_CBUS_REG(A9_0_IRQ_IN3_INTR_MASK, mask_save[3]);
}

static struct platform_suspend_ops meson_pm_ops = {
    .enter        = meson_pm_enter,
    .prepare      = meson_pm_prepare,
    .finish       = meson_pm_finish,
    .valid        = suspend_valid_only_mem,
};

static int __init meson_pm_probe(struct platform_device *pdev)
{
    printk(KERN_INFO "enter meson_pm_probe!\n");

    power_init_off();
    power_gate_init();

#ifdef CONFIG_HAS_EARLYSUSPEND
    early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
    early_suspend.suspend = meson_system_early_suspend;
    early_suspend.resume = meson_system_late_resume;
    early_suspend.param = pdev;
	register_early_suspend(&early_suspend);
#endif

    pdata = pdev->dev.platform_data;
    if (!pdata) {
        dev_err(&pdev->dev, "cannot get platform data\n");
        return -ENOENT;
    }
    pdata->ddr_reg_backup = sram_alloc(32*4);
    if (!pdata->ddr_reg_backup){
        dev_err(&pdev->dev, "cannot allocate SRAM memory\n");
        return -ENOMEM;
    }
    
    meson_sram_suspend = sram_alloc(meson_cpu_suspend_sz);
    if (!meson_sram_suspend) {
        dev_err(&pdev->dev, "cannot allocate SRAM memory\n");
        return -ENOMEM;
    }

    meson_sram_push(meson_sram_suspend, meson_cpu_suspend,
                        meson_cpu_suspend_sz);

    suspend_set_ops(&meson_pm_ops);
    printk(KERN_INFO "meson_pm_probe done 0x%x %d!\n", (unsigned)meson_sram_suspend, meson_cpu_suspend_sz);
    return 0;
}

static int __exit meson_pm_remove(struct platform_device *pdev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&early_suspend);
#endif
    return 0;
}

static struct platform_driver meson_pm_driver = {
    .driver = {
        .name     = "pm-meson",
        .owner     = THIS_MODULE,
    },
    .remove = __exit_p(meson_pm_remove),
};

static int __init meson_pm_init(void)
{
    return platform_driver_probe(&meson_pm_driver, meson_pm_probe);
}
late_initcall(meson_pm_init);
