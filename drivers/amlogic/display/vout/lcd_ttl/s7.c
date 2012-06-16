/*
 * AMLOGIC LCD panel driver.
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

static void clear_tcon_pinmux(void);
static void set_tcon_pinmux(void);
static void t13_power_on(void);
static void t13_power_off(void);
void power_on_backlight(void);
void power_off_backlight(void);
unsigned get_backlight_level(void);
void set_backlight_level(unsigned level);

#define LCD_WIDTH       800 
#define LCD_HEIGHT      480
#define MAX_WIDTH       928
#define MAX_HEIGHT      525
#define VIDEO_ON_PIXEL  48
#define VIDEO_ON_LINE   22 //17

//Define backlight control method
#define BL_CTL_GPIO		0
#define BL_CTL_PWM		1
#define BL_CTL			BL_CTL_PWM

//#define PRINT_DEBUG_INFO
#ifdef PRINT_DEBUG_INFO
#define PRINT_INFO(...)		printk(__VA_ARGS__)
#else
#define PRINT_INFO(...)
#endif

static lcdConfig_t lcd_config =
{
    .width      = LCD_WIDTH,
    .height     = LCD_HEIGHT,
    .max_width  = MAX_WIDTH,
    .max_height = MAX_HEIGHT,
    .video_on_pixel = VIDEO_ON_PIXEL,
    .video_on_line = VIDEO_ON_LINE,
    .pll_ctrl = 0x1021e,//30MHz
    .div_ctrl = 0x18803,
    .clk_ctrl = 0x100c,//pll_sel,div_sel,vclk_sel,xd
    .gamma_cntl_port = (1 << LCD_GAMMA_EN) | (0 << LCD_GAMMA_RVS_OUT) | (1 << LCD_GAMMA_VCOM_POL),
    .gamma_vcom_hswitch_addr = 0,
    .rgb_base_addr = 0xf0,
    .rgb_coeff_addr = 0x74a,
    .pol_cntl_addr = (0x0 << LCD_CPH1_POL) | (0x1 << LCD_HS_POL) | (0x1 << LCD_VS_POL),
    .dith_cntl_addr = 0x600,
    .sth1_hs_addr = 0,
    .sth1_he_addr = 0,
    .sth1_vs_addr = 0,
    .sth1_ve_addr = 0,
    .oeh_hs_addr = 67,
    .oeh_he_addr = 67+LCD_WIDTH,
    .oeh_vs_addr = VIDEO_ON_LINE,
    .oeh_ve_addr = VIDEO_ON_LINE+LCD_HEIGHT-1,
    .vcom_hswitch_addr = 0,
    .vcom_vs_addr = 0,
    .vcom_ve_addr = 0,
    .cpv1_hs_addr = 0,
    .cpv1_he_addr = 0,
    .cpv1_vs_addr = 0,
    .cpv1_ve_addr = 0,
    .stv1_hs_addr = 0,
    .stv1_he_addr = 0,
    .stv1_vs_addr = 0,
    .stv1_ve_addr = 0,
    .oev1_hs_addr = 0,
    .oev1_he_addr = 0,
    .oev1_vs_addr = 0,
    .oev1_ve_addr = 0,
    .inv_cnt_addr = (0<<LCD_INV_EN) | (0<<LCD_INV_CNT),
    .tcon_misc_sel_addr = (1<<LCD_STV1_SEL) | (1<<LCD_STV2_SEL),
    .dual_port_cntl_addr = (1<<LCD_TTL_SEL) | (1<<LCD_ANALOG_SEL_CPH3) | (1<<LCD_ANALOG_3PHI_CLK_SEL) | (0<<RGB_SWP) | (0<<BIT_SWP),
    .flags = LCD_DIGITAL_TTL,
    .screen_width = 16,
    .screen_height = 9,
    .sync_duration_num = 493,
    .sync_duration_den = 8,
    .power_on = t13_power_on,
    .power_off = t13_power_off,
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
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,16,17,18,18,19,20,20,21,22,23,23,24,25,25,26,27,28,
        28,29,30,30,31,32,33,33,34,35,36,37,37,38,38,39,40,41,42,42,43,44,45,45,46,47,48,49,49,50,51,52,
        53,54,55,56,57,58,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,74,75,76,77,77,78,79,79,80,
        81,81,82,82,83,84,84,85,85,86,86,87,88,88,89,89,90,91,91,92,93,93,94,95,96,96,97,98,99,100,100,101,
        102,102,103,104,105,105,106,107,108,108,109,110,110,111,112,113,113,114,115,115,116,117,118,119,119,120,121,122,123,124,125,126,
        127,129,129,130,131,133,134,134,136,136,137,138,139,140,141,142,143,144,144,145,146,147,148,149,149,150,152,152,154,154,155,157,
        157,158,159,160,161,162,163,164,165,166,167,168,169,170,170,171,172,173,174,175,176,177,178,179,181,182,184,185,187,189,191,192,
        194,196,197,199,201,202,203,205,206,208,209,211,212,214,215,217,219,220,222,223,225,227,228,230,232,234,236,239,242,245,250,255
    };

    for (i=0; i<256; i++) {
        pConf->GammaTableR[i] = gamma_adjust[i] << 2;
        pConf->GammaTableG[i] = gamma_adjust[i] << 2;
        pConf->GammaTableB[i] = gamma_adjust[i] << 2;
    }
}

#define PWM_MAX			60000   //set pwm_freq=24MHz/PWM_MAX (Base on XTAL frequence: 24MHz, 0<PWM_MAX<65535)
#define BL_MAX_LEVEL	255
#define BL_MIN_LEVEL	25		
void power_on_backlight(void)
{
    msleep(20);
    set_tcon_pinmux();
    msleep(100);

    //LCD_BL_5V -> GPIOA_9: 1
    clear_mio_mux(3, (1<<1));
    clear_mio_mux(0, (1<<6));
    set_gpio_mode(GPIOA_bank_bit0_27(9),GPIOA_bit_bit0_27(9),GPIO_OUTPUT_MODE);
    set_gpio_val(GPIOA_bank_bit0_27(9),GPIOA_bit_bit0_27(9), 1);
    msleep(50);

    //BL_EN -> GPIOD_1: 1
#if (BL_CTL==BL_CTL_GPIO)
	WRITE_CBUS_REG_BITS(LED_PWM_REG0, 1, 12, 2);
    clear_mio_mux(1,(1<<28));
    clear_mio_mux(2,(1<<3));
    set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE); 
    set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 1);
#elif (BL_CTL==BL_CTL_PWM)
    int pwm_div=0;  //pwm_freq=24M/(pwm_div+1)/PWM_MAX
    WRITE_CBUS_REG_BITS(PWM_PWM_D, 0, 0, 16);  //pwm low
    WRITE_CBUS_REG_BITS(PWM_PWM_D, PWM_MAX, 16, 16);  //pwm high
    SET_CBUS_REG_MASK(PWM_MISC_REG_CD, ((1 << 23) | (pwm_div<<16) | (1<<1)));  //enable pwm clk & pwm output
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<3));  //enable pwm pinmux
#endif
    msleep(100);

    PRINT_INFO("\nlcd parameter: power_on_backlight.\n");
}

void power_off_backlight(void)
{
    //BL_EN -> GPIOD_1: 1
#if (BL_CTL==BL_CTL_GPIO)
    clear_mio_mux(1,(1<<28));
    clear_mio_mux(2,(1<<3));
    set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE); 
    set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 0);
#elif (BL_CTL==BL_CTL_PWM)
    WRITE_CBUS_REG_BITS(PWM_PWM_D, PWM_MAX, 0, 16);  //pwm low
    WRITE_CBUS_REG_BITS(PWM_PWM_D, 0, 16, 16);  //pwm high
    CLEAR_CBUS_REG_MASK(PWM_MISC_REG_CD, ((1 << 23) | (1<<1)));  //disable pwm clk & pwm output
#endif
    msleep(20);

    //LCD_BL_5V->GPIOA_9: 0
    clear_mio_mux(3, (1<<1));
    clear_mio_mux(0, (1<<6));
    set_gpio_mode(GPIOA_bank_bit0_27(9),GPIOA_bit_bit0_27(9),GPIO_OUTPUT_MODE);
    set_gpio_val(GPIOA_bank_bit0_27(9),GPIOA_bit_bit0_27(9), 0);
    msleep(20);

    PRINT_INFO("\nlcd parameter: power_off_backlight.\n");
}

static unsigned bl_level;
unsigned get_backlight_level(void)
{
    PRINT_INFO("\nlcd parameter: get backlight level: %d.\n", bl_level);
    return bl_level;
}

void set_backlight_level(unsigned level)
{
    level = level>BL_MAX_LEVEL ? BL_MAX_LEVEL:(level<BL_MIN_LEVEL ? BL_MIN_LEVEL:level);
		
    PRINT_INFO("\n\nlcd parameter: set backlight level: %d.\n\n", level);
		
#if (BL_CTL==BL_CTL_GPIO)
    level = level * 15 / BL_MAX_LEVEL;
    level = 15 - level;
    WRITE_CBUS_REG_BITS(LED_PWM_REG0, level, 0, 4);
#elif (BL_CTL==BL_CTL_PWM)
    level = level * PWM_MAX / BL_MAX_LEVEL;
    WRITE_CBUS_REG_BITS(PWM_PWM_D, (PWM_MAX - level), 0, 16);  //pwm low
    WRITE_CBUS_REG_BITS(PWM_PWM_D, level, 16, 16);  //pwm high
#endif
}

static void power_on_lcd(void)
{
    //GPIOC_7 -> EN_VDD_PNL#: 1  LCD7_+3.3V
    clear_mio_mux(1,((1<<2) | (1<<3) | (1<<4) | (1<<11)));
    clear_mio_mux(0,(1<<17));
    set_gpio_mode(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), GPIO_OUTPUT_MODE);
    set_gpio_val(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), 1);
    msleep(50);

    //GPIOA_27 -> EN_VDD_BL#: 1  LCD_+10V,VGH_+16V,VGL_-7V
    clear_mio_mux(1,((1<<2) | (1<<3) | (1<<4) | (1<<11)));
    clear_mio_mux(0,(1<<17));
    set_gpio_mode(GPIOC_bank_bit0_15(7), GPIOC_bit_bit0_15(7), GPIO_OUTPUT_MODE); 
    set_gpio_val(GPIOC_bank_bit0_15(7), GPIOC_bit_bit0_15(7), 1);
    msleep(50);
}

static void power_off_lcd(void)
{
    power_off_backlight();
    msleep(50);	
	clear_tcon_pinmux();
	msleep(20);
    //GPIOA_27 -> EN_VDD_BL#: 0  LCD_+10V,VGH_+16V,VGL_-7V
    clear_mio_mux(1,((1<<2) | (1<<3) | (1<<4) | (1<<11)));
    clear_mio_mux(0,(1<<17));
    set_gpio_mode(GPIOC_bank_bit0_15(7), GPIOC_bit_bit0_15(7), GPIO_OUTPUT_MODE); 
    set_gpio_val(GPIOC_bank_bit0_15(7), GPIOC_bit_bit0_15(7), 0);
    msleep(30);

    //GPIOC_7 -> EN_VDD_PNL#: 0  LCD7_+3.3V
    clear_mio_mux(1,((1<<2) | (1<<3) | (1<<4) | (1<<11)));
    clear_mio_mux(0,(1<<17));
    set_gpio_mode(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), GPIO_OUTPUT_MODE); 
    set_gpio_val(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), 0);
    msleep(30);
}

static void set_tcon_pinmux(void)
{
    /* TCON control pins pinmux */
    clear_mio_mux(1, ((1<<11) | (1<<12) | (1<<13))); // disable cph50(11),cph2(13),cph3(12)
    clear_mio_mux(0,((1<<24) | (1<<27))); //disable mLVDS

    set_mio_mux(1, 1<<14); // enable cph1
    set_mio_mux(1, 1<<17); // enable oeh
    //set_mio_mux(0, 0x3f<<0);//For 8bits RGB
    set_mio_mux(0,(1<<0)|(1<<2)|(1<<4));//For 6bits RGB
	
    PRINT_INFO("\nlcd parameter: enable lcd signal ports.\n");
}

static void clear_tcon_pinmux(void)
{
    /* TCON control pins pinmux */
    /* GPIOD_7 -> LCD_Clk, GPIOD_4 -> TCON_OEH, GPIOD_3 -> TCON_STV, GPIOD_2 -> TCON_STH */
    clear_mio_mux(1, 1<<14); // disable cph1
    clear_mio_mux(1, 1<<17); // disable oeh
    //clear_mio_mux(0, 0x3f<<0);//For 8bits RGB
    clear_mio_mux(0,(1<<0)|(1<<2)|(1<<4));//For 6bits RGB
	
    WRITE_MPEG_REG(0x2012, READ_MPEG_REG(0x2012) | ((1<<20)|(1<<23)));
    //WRITE_MPEG_REG(0x200f, READ_MPEG_REG(0x200f) | (0xffffff<<0));//For RGB 8bit	
    WRITE_MPEG_REG(0x200f, READ_MPEG_REG(0x200f) | ((0x3f<<2)|(0x3f<<10)|(0x3f<<18)));//For RGB 6bit
	
    PRINT_INFO("\nlcd parameter: disable lcd signal ports.\n");
}

static void t13_power_on(void)
{
    video_dac_disable();
    //set_tcon_pinmux();
    power_on_lcd();
    PRINT_INFO("\n\nt13_power_on...\n\n");
}

static void t13_power_off(void)
{
    power_off_backlight();
    power_off_lcd();
}

static void t13_io_init(void)
{
    PRINT_INFO("\n\nT13 LCD Init.\n\n");

    //set_tcon_pinmux();
    power_on_lcd();
}


static struct platform_device lcd_dev = {
    .name = "tcon-dev",
    .id = 0,
    .num_resources = ARRAY_SIZE(lcd_resources),
    .resource = lcd_resources,
};

static int __init t13_init(void)
{
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



