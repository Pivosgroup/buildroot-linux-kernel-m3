/*
 * AMLOGIC T13 LCD panel driver.
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

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/vout/tcon.h>

#ifdef CONFIG_SN7325
#include <linux/sn7325.h>
#endif

#include <mach/gpio.h>
#include <mach/am_regs.h>
#include <mach/pinmux.h>
#include <mach/power_gate.h>

static void lvds_port_enable(void);
static void lvds_port_disable(void);

static void t13_power_on(void);
static void t13_power_off(void);
void power_on_backlight(void);
void power_off_backlight(void);
unsigned get_backlight_level(void);
void set_backlight_level(unsigned level);

//AT070TNA2
#define LCD_WIDTH       1024
#define LCD_HEIGHT      768
#define MAX_WIDTH       1344
#define MAX_HEIGHT      834
#define VIDEO_ON_PIXEL  80
#define VIDEO_ON_LINE   22

//Define backlight control method
#define BL_CTL_GPIO		0
#define BL_CTL_PWM		1
#define BL_CTL			BL_CTL_GPIO

// Define LVDS physical PREM SWING VCM REF
static lvds_phy_control_t lcd_lvds_phy_control = 
{
    .lvds_prem_ctl = 0x1,		//0xf  //0x1
    .lvds_swing_ctl = 0x3,	    //0x3  //0x1
    .lvds_vcm_ctl = 0x1,
    .lvds_ref_ctl = 0xf, 
};

//Define LVDS data mapping, bit num.
static lvds_config_t lcd_lvds_config=
{
	.lvds_repack=1,   //data mapping  //0:THine mode, 1:VESA mode
	.pn_swap=0,		  //0:normal, 1:swap
	.bit_num=1,       // 0:10bits, 1:8bits, 2:6bits, 3:4bits
	.lvds_phy_control = &lcd_lvds_phy_control,
};

static lcdConfig_t lcd_config =
{
    .width      = LCD_WIDTH,
    .height     = LCD_HEIGHT,
    .max_width  = MAX_WIDTH,
    .max_height = MAX_HEIGHT,
	.video_on_pixel = VIDEO_ON_PIXEL,
    .video_on_line = VIDEO_ON_LINE,
    .pll_ctrl = 0x10221,  //0x1021e
    .div_ctrl = 0x18803,  //0x18803
	.clk_ctrl = 0x1111,	//pll_sel,div_sel,vclk_sel,xd
	
    .gamma_cntl_port = (1 << LCD_GAMMA_EN) | (0 << LCD_GAMMA_RVS_OUT) | (1 << LCD_GAMMA_VCOM_POL),
    .gamma_vcom_hswitch_addr = 0,
	
    .rgb_base_addr = 0xf0,
    .rgb_coeff_addr = 0x764,
    .pol_cntl_addr = (0x0 << LCD_CPH1_POL) |(0x1 << LCD_HS_POL) | (0x1 << LCD_VS_POL),
    .dith_cntl_addr = 0x600,

	.sth1_hs_addr = 10,
    .sth1_he_addr = 20,
    .sth1_vs_addr = 0,
    .sth1_ve_addr = MAX_HEIGHT - 1,
	.stv1_hs_addr = 10,
    .stv1_he_addr = 20,
    .stv1_vs_addr = 2,
    .stv1_ve_addr = 4,

    .inv_cnt_addr = (0<<LCD_INV_EN) | (0<<LCD_INV_CNT),
    .tcon_misc_sel_addr = (1<<LCD_STV1_SEL) | (1<<LCD_STV2_SEL),
    .dual_port_cntl_addr = (1<<LCD_TTL_SEL) | (1<<LCD_ANALOG_SEL_CPH3) | (1<<LCD_ANALOG_3PHI_CLK_SEL),
	
    .flags = LCD_DIGITAL_LVDS,
    .screen_width = 4,
    .screen_height = 3,
    .sync_duration_num = 505,  // 60
    .sync_duration_den = 10,	 // 1	
	.lvds_config = &lcd_lvds_config,
    .power_on=t13_power_on,
    .power_off=t13_power_off,
    .backlight_on = power_on_backlight,
    .backlight_off = power_off_backlight,
	.get_bl_level = get_backlight_level,
    .set_bl_level = set_backlight_level,
};
static struct resource lcd_resources[] = {
    [0] = {
        .start = (ulong)&lcd_config,
        .end   = (ulong)&lcd_config + sizeof(lcdConfig_t) - 1,
        .flags = IORESOURCE_MEM,
    },
};

static void t13_setup_gama_table(lcdConfig_t *pConf)
{
    int i;
    const unsigned short gamma_adjust[256] = {
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
        32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
        64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
    };

    for (i=0; i<256; i++) {
        pConf->GammaTableR[i] = gamma_adjust[i] << 2;
        pConf->GammaTableG[i] = gamma_adjust[i] << 2;
        pConf->GammaTableB[i] = gamma_adjust[i] << 2;
    }
}

#define PWM_MAX			60000   //set pwm_freq=24MHz/PWM_MAX (Base on XTAL frequence: 24MHz, 0<PWM_MAX<65535)
#define BL_MAX_LEVEL	255
#define BL_MIN_LEVEL	0
void power_on_backlight(void)
{
	lvds_port_enable();
	
	//BL_EN: GPIOD_1(PWM_D)
	WRITE_CBUS_REG_BITS(LED_PWM_REG0, 1, 12, 2); 
	msleep(300);

#if (BL_CTL==BL_CTL_GPIO)	
    WRITE_MPEG_REG(0x2013, READ_MPEG_REG(0x2013) | (1 << 17));    
    WRITE_MPEG_REG(0x2012, READ_MPEG_REG(0x2012) & ~(1 << 17));
#elif (BL_CTL==BL_CTL_PWM)
	int pwm_div=0;  //pwm_freq=24M/(pwm_div+1)/PWM_MAX	
	WRITE_CBUS_REG_BITS(PWM_PWM_D, 0, 0, 16);  //pwm low
    WRITE_CBUS_REG_BITS(PWM_PWM_D, PWM_MAX, 16, 16);  //pwm high
	SET_CBUS_REG_MASK(PWM_MISC_REG_CD, ((1 << 23) | (pwm_div<<16) | (1<<1)));  //enable pwm clk & pwm output
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<3));  //enable pwm pinmux
#endif

   //printk("\n\nLCD: power_on_backlight.\n\n");
}

void power_off_backlight(void)
{
	//BL_EN: GPIOD_1(PWM_D)    
#if (BL_CTL==BL_CTL_GPIO)	
    WRITE_MPEG_REG(0x2013, READ_MPEG_REG(0x2013) & ~(1 << 17));
#elif (BL_CTL==BL_CTL_PWM)
	WRITE_CBUS_REG_BITS(PWM_PWM_D, PWM_MAX, 0, 16);  //pwm low
    WRITE_CBUS_REG_BITS(PWM_PWM_D, 0, 16, 16);  //pwm high
    CLEAR_CBUS_REG_MASK(PWM_MISC_REG_CD, ((1 << 23) | (1<<1)));  //disable pwm clk & pwm output
#endif	
	
	msleep(20);
	lvds_port_disable();
	msleep(20);	
}

static unsigned bl_level;
unsigned get_backlight_level(void)
{
	return bl_level;
}

void set_backlight_level(unsigned level)
{
	//printk("set_backlight_level %d\n", level);
	level = level>BL_MAX_LEVEL ? BL_MAX_LEVEL:(level<BL_MIN_LEVEL ? BL_MIN_LEVEL:level);		
#if (BL_CTL==BL_CTL_GPIO)
  if (level == BL_MIN_LEVEL){
  	level = 15;
  }
  else {
	  level = level * 10 / BL_MAX_LEVEL;	
	  level = 13 - level;
	}
	WRITE_CBUS_REG_BITS(LED_PWM_REG0, level, 0, 4);	// [3-13] 10 level
#elif (BL_CTL==BL_CTL_PWM)	
	level = level * PWM_MAX / BL_MAX_LEVEL ;	
	WRITE_CBUS_REG_BITS(PWM_PWM_D, (PWM_MAX - level), 0, 16);  //pwm low
    WRITE_CBUS_REG_BITS(PWM_PWM_D, level, 16, 16);  //pwm high	
#endif
}

static void power_on_lcd(void)
{
	//printk("\n\nLCD: power on lcd.\n");
	//LCD_PWR_EN
	WRITE_MPEG_REG(0x200d, READ_MPEG_REG(0x200d) & ~(1 << 27));    
    WRITE_MPEG_REG(0x200c, READ_MPEG_REG(0x200c) & ~(1 << 27));
    msleep(100);
	
	//VCCx3_EN
	WRITE_MPEG_REG(0x2013, READ_MPEG_REG(0x2013) | (1 << 2));    
	WRITE_MPEG_REG(0x2012, READ_MPEG_REG(0x2012) & ~(1 << 2));

//#ifdef CONFIG_AW_AXP    //Added by Leon 20111124
//	axp_gpio_set_io(3,1);
//	axp_gpio_set_value(3, 0);
//#endif
	 set_gpio_val(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), 1);    
    set_gpio_mode(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), GPIO_OUTPUT_MODE);
	msleep(10);
}

static void power_off_lcd(void)
{
	//printk("\n\nLCD: power off lcd.\n");
	//VCCx3_EN
//#ifdef CONFIG_AW_AXP    //Added by Leon 20111124
//        axp_gpio_set_io(3,1);
//		axp_gpio_set_value(3, 1); 
//#endif
 set_gpio_val(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), 0);
    set_gpio_mode(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), GPIO_OUTPUT_MODE);
	WRITE_MPEG_REG(0x2013, READ_MPEG_REG(0x2013) & ~(1 << 2));
    //WRITE_MPEG_REG(0x2012, READ_MPEG_REG(0x2012) & ~(1 << 2));

	msleep(50);
	
	//LCD_PWR_EN
	WRITE_MPEG_REG(0x200d, READ_MPEG_REG(0x200d) | (1 << 27));    
    //WRITE_MPEG_REG(0x200c, READ_MPEG_REG(0x200c) & ~(1 << 27));
    msleep(10);
}

static void lvds_port_enable(void)
{	
	//printk("\n\nLCD: enable LVDS fifo.\n");
	// disable lvds fifo
    //WRITE_MPEG_REG(LVDS_GEN_CNTL, READ_MPEG_REG(LVDS_GEN_CNTL) & ~((1<<0)|(1<<3)));
    // enable lvds fifo
    WRITE_MPEG_REG(LVDS_GEN_CNTL, READ_MPEG_REG(LVDS_GEN_CNTL) | (1<<3));
	
	//printk("\n\nLCD: LVDS port enable.\n");	
	//enable minilvds_data channel
	WRITE_MPEG_REG(LVDS_PHY_CNTL4, READ_MPEG_REG(LVDS_PHY_CNTL4) | (0x2f<<0));	
	
	msleep(30);
	//printk("\n\nLCD: LVDS serializer soft reset.\n");
	// Set soft Reset lvds_phy_ser_top
    //WRITE_MPEG_REG(LVDS_PHY_CLK_CNTL, READ_MPEG_REG(LVDS_PHY_CLK_CNTL) | (1<<14));
    // Release soft Reset lvds_phy_ser_top
    //WRITE_MPEG_REG(LVDS_PHY_CLK_CNTL, READ_MPEG_REG(LVDS_PHY_CLK_CNTL) & ~(1<<14));	
}

static void lvds_port_disable(void)
{		
	//printk("\n\nLCD: LVDS port disable.\n");
	//disable minilvds_data channel
	WRITE_MPEG_REG(LVDS_PHY_CNTL4, READ_MPEG_REG(LVDS_PHY_CNTL4) & ~(0x7f<<0));	
	
	//printk("\n\nLCD: disable LVDS fifo.\n");
	// disable lvds fifo
	WRITE_MPEG_REG(LVDS_GEN_CNTL, READ_MPEG_REG(LVDS_GEN_CNTL) & ~(1<<3));
}

static void t13_power_on(void)
{
    video_dac_disable();	
	power_on_lcd();	
	//power_on_backlight();   //disable when required power sequence.
    //printk("\n\nt13_power_on...\n\n");
}
static void t13_power_off(void)
{    
	power_off_backlight();
	power_off_lcd();
}

static void t13_io_init(void)
{
    //printk("\n\nT13 LCD Init.\n\n");    
    power_on_lcd();	
	//power_on_backlight();	//disable when required power sequence.
}

static struct platform_device lcd_dev = {
    .name = "tcon-dev",
    .id   = 0,
    .num_resources = ARRAY_SIZE(lcd_resources),
    .resource      = lcd_resources,
};

static int __init t13_init(void)
{
    power_off_backlight();
    t13_setup_gama_table(&lcd_config);
    t13_io_init();

    platform_device_register(&lcd_dev);

    return 0;
}

static void __exit t13_exit(void)
{
    power_off_backlight();
    power_off_lcd();

    platform_device_unregister(&lcd_dev);
}

subsys_initcall(t13_init);
//module_init(t13_init);
module_exit(t13_exit);

MODULE_DESCRIPTION("AMLOGIC T13 LCD panel driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
