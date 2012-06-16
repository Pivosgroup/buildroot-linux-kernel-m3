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

//cpt CLAA070MA22BW panel
#define LCD_WIDTH       800 
#define LCD_HEIGHT      600
#define MAX_WIDTH       1010
#define MAX_HEIGHT      660
#define VIDEO_ON_PIXEL  48
#define VIDEO_ON_LINE   22

#define BL_ON            1
#define BL_OFF           0

//Define backlight control method
#define BL_CTL_GPIO		0
#define BL_CTL_PWM		1
#define BL_CTL			BL_CTL_GPIO

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
    .video_on_line = VIDEO_ON_LINE,
    .pll_ctrl = 0x1021f,
	.div_ctrl = 0x18803,
#ifdef CONFIG_AM_TV_OUTPUT2
    .clk_ctrl = 0x111b,	//pll_sel,div_sel,vclk_sel,xd
#else
    .clk_ctrl = 0x100b,	//pll_sel,div_sel,vclk_sel,xd
#endif    
    .gamma_cntl_port = (1 << LCD_GAMMA_EN) | (0 << LCD_GAMMA_RVS_OUT) | (1 << LCD_GAMMA_VCOM_POL),
    .gamma_vcom_hswitch_addr = 0,
    .rgb_base_addr = 0xf0,
    .rgb_coeff_addr = 0x74a,
    .pol_cntl_addr = (0x0 << LCD_CPH1_POL) |(0x1 << LCD_HS_POL) | (0x1 << LCD_VS_POL),
    .dith_cntl_addr = 0x400,
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
    .dual_port_cntl_addr = (1<<LCD_TTL_SEL) | (1<<LCD_ANALOG_SEL_CPH3) | (1<<LCD_ANALOG_3PHI_CLK_SEL) | (1<<RGB_SWP) | (1<<BIT_SWP),
    .flags = 0,
    .screen_width = 4,
    .screen_height = 3,
    .sync_duration_num = 507,
    .sync_duration_den = 10,
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
	set_tcon_pinmux();
	WRITE_CBUS_REG_BITS(LED_PWM_REG0, 1, 12, 2); 
	msleep(300); // wait for PWM charge
	
	PRINT_INFO(" w7 power_on_backlight \n");
	//dump_stack();
	
    set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 1);
    set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE);	
    //printk("======================= backlight on ==========================\n");
}

void power_off_backlight(void)
{ 
    PRINT_INFO(" w7 power_off_backlight \n");
    //BL_EN -> GPIOD_1: 0
    set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 0);
    set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE);
    //printk("======================= backlight off ==========================\n");
	
	clear_tcon_pinmux();
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
/*
    if (level > BL_MIN_LEVEL)
    	{
    	set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 1);
    	set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE);
        //power_on_backlight();
        }
    else
    	{
    	set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 0);
    	set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE);
        //power_off_backlight();
        }
*/
	level = level * 15 / BL_MAX_LEVEL;	
	level = 15 - level;
	WRITE_CBUS_REG_BITS(LED_PWM_REG0, level, 0, 4);	
#elif (BL_CTL==BL_CTL_PWM)	
	level = level * PWM_MAX / BL_MAX_LEVEL ;	
	WRITE_CBUS_REG_BITS(PWM_PWM_D, (PWM_MAX - level), 0, 16);  //pwm low
    WRITE_CBUS_REG_BITS(PWM_PWM_D, level, 16, 16);  //pwm high	
#endif	
}

static void power_on_lcd(void)
{
    PRINT_INFO(" w7 power_on_lcd \n");
    //printk("======================= panel on ==========================\n");
	//GPIOA27 -> LCD_PWR_EN#: 0  lcd 3.3v
    set_gpio_val(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), 0);
    set_gpio_mode(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), GPIO_OUTPUT_MODE);
    
    //GPIOC2 -> VCCx3_EN: 1
    set_gpio_val(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), 1);
    set_gpio_mode(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), GPIO_OUTPUT_MODE);
}

static void power_off_lcd(void)
{
    PRINT_INFO(" w7 power_off_lcd \n");    
    //printk("======================= panel off ==========================\n");
    //GPIOC2 -> VCCx3_EN: 0
    set_gpio_val(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), 0);
    set_gpio_mode(GPIOC_bank_bit0_15(2), GPIOC_bit_bit0_15(2), GPIO_OUTPUT_MODE);
    
    //GPIOA27 -> LCD_PWR_EN#: 1  lcd 3.3v
    set_gpio_val(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), 1);
    set_gpio_mode(GPIOA_bank_bit0_27(27), GPIOA_bit_bit0_27(27), GPIO_OUTPUT_MODE);
}

static void set_tcon_pinmux(void)
{
    PRINT_INFO("\nlcd: enable lcd signal ports.\n");
	/* TCON control pins pinmux */
    clear_mio_mux(1, 0x0f<<11); // disable cph50(11),cph1(12),cph2(13),cph3(14)
#ifdef USE_CLKO
    set_mio_mux(1, 1<<21); // enable clko
#else
    set_mio_mux(1, 1<<14); // enable cph1
#endif
    set_mio_mux(1, 1<<17); // enable oeh
    set_mio_mux(0, 0x3f<<0);   //For 8bits RGB
}

static void clear_tcon_pinmux(void)
{
    PRINT_INFO("\nlcd: disable lcd signal ports.\n");
	/* TCON control pins pinmux */
    clear_mio_mux(1, 0x0f<<11); // disable cph50(11),cph1(12),cph2(13),cph3(14)
#ifdef USE_CLKO
    clear_mio_mux(1, 1<<21); // enable clko
#else
    clear_mio_mux(1, 1<<14); // enable cph1
#endif
    clear_mio_mux(1, 1<<17); // enable oeh
    clear_mio_mux(0, 0x3f<<0);   //For 8bits RGB
	
	WRITE_MPEG_REG(0x2012, READ_MPEG_REG(0x2012) | ((1<<20)|(1<<23)));  //set oeh & cph channel as GPIO input
	WRITE_MPEG_REG(0x2010, READ_MPEG_REG(0x2010) & ~(0xffffff<<0));  //For RGB 8bit, set RGB data channel as GPIO input
	WRITE_MPEG_REG(0x200f, READ_MPEG_REG(0x200f) & ~(0xffffff<<0));  //For RGB 8bit, set RGB data channel as GPIO input
	//WRITE_MPEG_REG(0x200f, READ_MPEG_REG(0x200f) | ((0x3f<<2)|(0x3f<<10)|(0x3f<<18)));  //For RGB 6bit, set RGB data channel as GPIO input
}

#ifdef CONFIG_AM_LOGO
extern void (*Power_on_bl)(void);
//called when kernel logo is displayed.
//backlight will be powered on right here
static void power_on_bl(void)
{
    PRINT_INFO(" w7 power_on_bl \n");
	msleep(50);
	//set_tcon_pinmux();
    power_on_backlight();
}
#endif

static void t13_power_on(void)
{
    video_dac_disable();
	//set_tcon_pinmux();
	power_on_lcd();
    PRINT_INFO("\n\nt13_power_on...\n\n");
    Power_on_bl = power_on_bl;
}

static void t13_power_off(void)
{
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
    .id   = 0,
    .num_resources = ARRAY_SIZE(lcd_resources),
    .resource      = lcd_resources,
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



