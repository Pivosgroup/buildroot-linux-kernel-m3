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

#ifdef CONFIG_AW_AXP
#include "../../../power/axp_power/axp-gpio.h"
#endif
#include <mach/mlvds_regs.h>

static void lvds_port_enable(void);
static void lvds_port_disable(void);

static void t13_power_on(void);
static void t13_power_off(void);
void power_on_backlight(void);
void power_off_backlight(void);
unsigned get_backlight_level(void);
void set_backlight_level(unsigned level);


//AT070TNA2
#define LCD_WIDTH       800
#define LCD_HEIGHT      1280
#define MAX_WIDTH       1056//1344
#define MAX_HEIGHT      1320//635
#define VIDEO_ON_PIXEL  80
#define VIDEO_ON_LINE   22

//Define backlight control method
#define BL_CTL_GPIO		0
#define BL_CTL_PWM		1
#define BL_CTL			BL_CTL_GPIO
#define BL_MAX_LEVEL_2D		220
#define BL_MIN_LEVEL_2D		0
#define BL_MAX_LEVEL_3D		255
#define BL_MIN_LEVEL_3D		20
static unsigned BL_MAX=220;
static unsigned BL_MIN=0;
static unsigned bl_level;

// Define LVDS physical PREM SWING VCM REF
static lvds_phy_control_t lcd_lvds_phy_control = 
{
    .lvds_prem_ctl = 0x1,		//0xf  //0x1
    .lvds_swing_ctl = 0x1,	    //0x3  //0x1
    .lvds_vcm_ctl = 0x1,
    .lvds_ref_ctl = 0xf, 
};

//Define LVDS data mapping, bit num.
static lvds_config_t lcd_lvds_config=
{
	.lvds_repack=0,   //data mapping  //0:THine mode, 1:VESA mode
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
    .pll_ctrl = 0x10229,
    .div_ctrl = 0x18803,
	.clk_ctrl = 0x1111,	//pll_sel,div_sel,vclk_sel,xd
	
    .gamma_cntl_port = (1 << LCD_GAMMA_EN) | (0 << LCD_GAMMA_RVS_OUT) | (1 << LCD_GAMMA_VCOM_POL),
    .gamma_vcom_hswitch_addr = 0,
	
    .rgb_base_addr = 0xf0,
    .rgb_coeff_addr = 0x74a,
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
    .screen_width = 5,
    .screen_height = 8,
    .sync_duration_num = 504,
    .sync_duration_den = 10,
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
#define  fe_H  115
#define  fe_L  100
static void t13_setup_gama_table(lcdConfig_t *pConf, int flag)
{
	int i,j,org,k;					
	int duang[6] = {0,16,24,180,240,256};
	
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

	if (flag==1)  //for 3D gamma
	{
		for(i=duang[0];i<duang[1];i++)
		{
			pConf->GammaTableR[i]=gamma_adjust[i]<<2;
			pConf->GammaTableG[i]=gamma_adjust[i]<<2;
			pConf->GammaTableB[i]=gamma_adjust[i]<<2;
			j = gamma_adjust[i];
		}
		
		k = (fe_H*gamma_adjust[duang[2]-1])/fe_L;
		org = ((k-j)*100)/(duang[2]-duang[1]);

		for(i=duang[1];i<duang[2];i++)
		{
			pConf->GammaTableR[i]=(j+(org*(i-duang[1]+1)/100))<<2;
			pConf->GammaTableG[i]=(j+(org*(i-duang[1]+1)/100))<<2;
			pConf->GammaTableB[i]=(j+(org*(i-duang[1]+1)/100))<<2;
		}
		
		for(i=duang[2];i<duang[3];i++)
		{
			k = (fe_H*gamma_adjust[i])/fe_L;
			pConf->GammaTableR[i]=k<<2;
			pConf->GammaTableG[i]=k<<2;
			pConf->GammaTableB[i]=k<<2;
			j = k;
		}
		org = ((gamma_adjust[duang[4]]-j)*1000)/(duang[4]-duang[3]);
		for(i=duang[3];i<duang[4];i++)
		{
			pConf->GammaTableR[i]=(j+org*(i-duang[3]+1)/1000)<<2;
			pConf->GammaTableG[i]=(j+org*(i-duang[3]+1)/1000)<<2;
			pConf->GammaTableB[i]=(j+org*(i-duang[3]+1)/1000)<<2;
		}
		for(i=duang[4];i<duang[5];i++)
		{
			pConf->GammaTableR[i]=gamma_adjust[i]<<2;
			pConf->GammaTableG[i]=gamma_adjust[i]<<2;
			pConf->GammaTableB[i]=gamma_adjust[i]<<2;
		}		
	}
	else   //for 2D gamma
	{
		for (i=0; i<256; i++) 
		{
			pConf->GammaTableR[i] = gamma_adjust[i] << 2;
			pConf->GammaTableG[i] = gamma_adjust[i] << 2;
			pConf->GammaTableB[i] = gamma_adjust[i] << 2;
		}
	}
}

static void set_gamma_table(u16 *data, u32 rgb_mask)
{
    int i;	
	
    while (!(READ_MPEG_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY)));
    WRITE_MPEG_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
                                    (0x1 << rgb_mask)   |
                                    (0x0 << HADR));
    for (i=0;i<256;i++) {
        while (!( READ_MPEG_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY) )) ;
        WRITE_MPEG_REG(L_GAMMA_DATA_PORT, data[i]);
    }
    while (!(READ_MPEG_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY)));
    WRITE_MPEG_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
                                    (0x1 << rgb_mask)   |
                                    (0x23 << HADR));
}

static void chang_gamma_table(int flag)
{
	t13_setup_gama_table(&lcd_config, flag);	
	
	WRITE_CBUS_REG_BITS(L_GAMMA_CNTL_PORT, 0, 0, 1);
	set_gamma_table(lcd_config.GammaTableR, H_SEL_R);
    set_gamma_table(lcd_config.GammaTableG, H_SEL_G);
    set_gamma_table(lcd_config.GammaTableB, H_SEL_B);
    WRITE_CBUS_REG_BITS(L_GAMMA_CNTL_PORT, 1, 0, 1);
}

static void pwm_3d(int flag)
{
	if (flag==1)
	{		
		clear_mio_mux(1, (1<<5));   //clear other pinmux
		clear_mio_mux(3, ((1<<24)|(1<<25)));   //clear other pinmux
		clear_mio_mux(7, (1<<17));   //clear other pinmux
		
		set_mio_mux(0, ((1<<16)|(1<<19))); //enable 3d PWM channel
		
	}
	else
	{
		clear_mio_mux(0, ((1<<16)|(1<<19)));  //disable 3d PWM channel				
		
		WRITE_MPEG_REG(0x2013, READ_MPEG_REG(0x2013) & ~((1<<6)|(1<<9)));  	 //set pwm channel output low
		WRITE_MPEG_REG(0x2012, READ_MPEG_REG(0x2012) & ~((1<<6)|(1<<9)));  	 //set pwm channel as GPIO output
	}
}
void control_lcd_3d(char flag_3d) {
    chang_gamma_table(flag_3d);
    pwm_3d(flag_3d);
    if (flag_3d)
    {
    	BL_MAX=BL_MAX_LEVEL_3D;
    	BL_MIN=BL_MIN_LEVEL_3D;
    }
    else
    {
    	BL_MAX=BL_MAX_LEVEL_2D;
    	BL_MIN=BL_MIN_LEVEL_2D;
    }
    set_backlight_level(bl_level);    //change BL @ 2D/3D mode
}

static ssize_t control_enable3d(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{
    char flag_3d = buf[0] ;    
    if(flag_3d == '1') {
        flag_3d = 1;
        printk("\nLCD 3D on.\n");
    }else{
        flag_3d = 0;
        printk("\nLCD 3D off.\n");
    }
    
    control_lcd_3d(flag_3d);

	return count;
}

static struct class_attribute enable3d_class_attrs[] = {
    __ATTR(enable-3d,  S_IRUGO | S_IWUSR, NULL,    control_enable3d), 
    __ATTR_NULL
};

static struct class enable3d_class = {
    .name = "enable3d",
    .class_attrs = enable3d_class_attrs,
};

#define EVEN_CHANNEL 	4   //3D PWM+
#define PCLK_CHANNEL 	7   //3D PWM-
static mlvds_tcon_config_t lcd_mlvds_tcon_config[2]=
{	
    {EVEN_CHANNEL, 1, 0, MAX_WIDTH-1, 0, (MAX_HEIGHT/2)-1, 0, 0, 0, 0},    
	{PCLK_CHANNEL, 1, 0, MAX_WIDTH-1, MAX_HEIGHT/2, MAX_HEIGHT-1, 0, 0, 0, 0}
};

static void write_tcon_double(mlvds_tcon_config_t *mlvds_tcon)
{
    unsigned int tmp;
    int channel_num = mlvds_tcon->channel_num;
    int hv_sel = mlvds_tcon->hv_sel;
    int hstart_1 = mlvds_tcon->tcon_1st_hs_addr;
    int hend_1 = mlvds_tcon->tcon_1st_he_addr;
    int vstart_1 = mlvds_tcon->tcon_1st_vs_addr;
    int vend_1 = mlvds_tcon->tcon_1st_ve_addr;
    int hstart_2 = mlvds_tcon->tcon_2nd_hs_addr;
    int hend_2 = mlvds_tcon->tcon_2nd_he_addr;
    int vstart_2 = mlvds_tcon->tcon_2nd_vs_addr;
    int vend_2 = mlvds_tcon->tcon_2nd_ve_addr;

    tmp = READ_MPEG_REG(L_TCON_MISC_SEL_ADDR);
    switch(channel_num)
    {
        case 0 :
            WRITE_MPEG_REG(MTCON0_1ST_HS_ADDR, hstart_1);
            WRITE_MPEG_REG(MTCON0_1ST_HE_ADDR, hend_1);
            WRITE_MPEG_REG(MTCON0_1ST_VS_ADDR, vstart_1);
            WRITE_MPEG_REG(MTCON0_1ST_VE_ADDR, vend_1);
            WRITE_MPEG_REG(MTCON0_2ND_HS_ADDR, hstart_2);
            WRITE_MPEG_REG(MTCON0_2ND_HE_ADDR, hend_2);
            WRITE_MPEG_REG(MTCON0_2ND_VS_ADDR, vstart_2);
            WRITE_MPEG_REG(MTCON0_2ND_VE_ADDR, vend_2);
            tmp &= (~(1 << STH1_SEL)) | (hv_sel << STH1_SEL);
            WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 1 :
            WRITE_MPEG_REG(MTCON1_1ST_HS_ADDR, hstart_1);
            WRITE_MPEG_REG(MTCON1_1ST_HE_ADDR, hend_1);
            WRITE_MPEG_REG(MTCON1_1ST_VS_ADDR, vstart_1);
            WRITE_MPEG_REG(MTCON1_1ST_VE_ADDR, vend_1);
            WRITE_MPEG_REG(MTCON1_2ND_HS_ADDR, hstart_2);
            WRITE_MPEG_REG(MTCON1_2ND_HE_ADDR, hend_2);
            WRITE_MPEG_REG(MTCON1_2ND_VS_ADDR, vstart_2);
            WRITE_MPEG_REG(MTCON1_2ND_VE_ADDR, vend_2);
            tmp &= (~(1 << CPV1_SEL)) | (hv_sel << CPV1_SEL);
            WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 2 :
            WRITE_MPEG_REG(MTCON2_1ST_HS_ADDR, hstart_1);
            WRITE_MPEG_REG(MTCON2_1ST_HE_ADDR, hend_1);
            WRITE_MPEG_REG(MTCON2_1ST_VS_ADDR, vstart_1);
            WRITE_MPEG_REG(MTCON2_1ST_VE_ADDR, vend_1);
            WRITE_MPEG_REG(MTCON2_2ND_HS_ADDR, hstart_2);
            WRITE_MPEG_REG(MTCON2_2ND_HE_ADDR, hend_2);
            WRITE_MPEG_REG(MTCON2_2ND_VS_ADDR, vstart_2);
            WRITE_MPEG_REG(MTCON2_2ND_VE_ADDR, vend_2);
            tmp &= (~(1 << STV1_SEL)) | (hv_sel << STV1_SEL);
            WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 3 :
            WRITE_MPEG_REG(MTCON3_1ST_HS_ADDR, hstart_1);
            WRITE_MPEG_REG(MTCON3_1ST_HE_ADDR, hend_1);
            WRITE_MPEG_REG(MTCON3_1ST_VS_ADDR, vstart_1);
            WRITE_MPEG_REG(MTCON3_1ST_VE_ADDR, vend_1);
            WRITE_MPEG_REG(MTCON3_2ND_HS_ADDR, hstart_2);
            WRITE_MPEG_REG(MTCON3_2ND_HE_ADDR, hend_2);
            WRITE_MPEG_REG(MTCON3_2ND_VS_ADDR, vstart_2);
            WRITE_MPEG_REG(MTCON3_2ND_VE_ADDR, vend_2);
            tmp &= (~(1 << OEV1_SEL)) | (hv_sel << OEV1_SEL);
            WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 4 :
            WRITE_MPEG_REG(MTCON4_1ST_HS_ADDR, hstart_1);
            WRITE_MPEG_REG(MTCON4_1ST_HE_ADDR, hend_1);
            WRITE_MPEG_REG(MTCON4_1ST_VS_ADDR, vstart_1);
            WRITE_MPEG_REG(MTCON4_1ST_VE_ADDR, vend_1);
            tmp &= (~(1 << STH2_SEL)) | (hv_sel << STH2_SEL);
            WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 5 :
            WRITE_MPEG_REG(MTCON5_1ST_HS_ADDR, hstart_1);
            WRITE_MPEG_REG(MTCON5_1ST_HE_ADDR, hend_1);
            WRITE_MPEG_REG(MTCON5_1ST_VS_ADDR, vstart_1);
            WRITE_MPEG_REG(MTCON5_1ST_VE_ADDR, vend_1);
            tmp &= (~(1 << CPV2_SEL)) | (hv_sel << CPV2_SEL);
            WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 6 :
            WRITE_MPEG_REG(MTCON6_1ST_HS_ADDR, hstart_1);
            WRITE_MPEG_REG(MTCON6_1ST_HE_ADDR, hend_1);
            WRITE_MPEG_REG(MTCON6_1ST_VS_ADDR, vstart_1);
            WRITE_MPEG_REG(MTCON6_1ST_VE_ADDR, vend_1);
            tmp &= (~(1 << OEH_SEL)) | (hv_sel << OEH_SEL);
            WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, tmp);
            break;
        case 7 :
            WRITE_MPEG_REG(MTCON7_1ST_HS_ADDR, hstart_1);
            WRITE_MPEG_REG(MTCON7_1ST_HE_ADDR, hend_1);
            WRITE_MPEG_REG(MTCON7_1ST_VS_ADDR, vstart_1);
            WRITE_MPEG_REG(MTCON7_1ST_VE_ADDR, vend_1);
            tmp &= (~(1 << OEV3_SEL)) | (hv_sel << OEV3_SEL);
            WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, tmp);
            break;
        default:
            break;
    }
}

static void set_pwm_3d_tcon(void)
{	
    int i=0;
	printk("write new tcon_A 0~7.\n");
    for(i = 0; i < 2; i++)
    {
		write_tcon_double(&lcd_mlvds_tcon_config[i]);
    }	
}

#define PWM_MAX			60000   //set pwm_freq=24MHz/PWM_MAX (Base on XTAL frequence: 24MHz, 0<PWM_MAX<65535)
#define BL_MAX_LEVEL	255
#define BL_MIN_LEVEL	0
void power_on_backlight(void)
{
	lvds_port_enable();
	WRITE_CBUS_REG_BITS(LED_PWM_REG0, 1, 12, 2);
    msleep(300);
	
	BL_MAX=BL_MAX_LEVEL_2D;
	BL_MIN=BL_MIN_LEVEL_2D;
	//BL_EN: GPIOD_1(PWM_D)
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
    //msleep(100);

    printk("\n\npower_on_backlight.\n\n");
}

void power_off_backlight(void)
{
    printk("\n\npower_off_backlight.\n\n");
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

unsigned get_backlight_level(void)
{
	return bl_level;
}

void set_backlight_level(unsigned level)
{
	bl_level=level;
	level = level>BL_MAX_LEVEL ? BL_MAX_LEVEL:(level<BL_MIN_LEVEL ? BL_MIN_LEVEL:level);
	level=(level*(BL_MAX-BL_MIN)/BL_MAX_LEVEL)+BL_MIN;	
#if (BL_CTL==BL_CTL_GPIO)
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
	//LCD_PWR_EN: GPIOA_27
	WRITE_MPEG_REG(0x200d, READ_MPEG_REG(0x200d) & ~(1 << 27));    
    WRITE_MPEG_REG(0x200c, READ_MPEG_REG(0x200c) & ~(1 << 27));
    msleep(10);
	
#ifdef CONFIG_AW_AXP
    axp_gpio_set_io(3,1);
    axp_gpio_set_value(3, 0);
#else
	//VCCx3_EN: GPIOD_2
    WRITE_MPEG_REG(0x2012, READ_MPEG_REG(0x2012) & ~(1 << 18));
	WRITE_MPEG_REG(0x2013, READ_MPEG_REG(0x2013) | (1 << 18));    
#endif

	msleep(10);
}

static void power_off_lcd(void)
{
#ifdef CONFIG_AW_AXP
    axp_gpio_set_io(3,0);
#else    
	//VCCx3_EN: GPIOD_2    
    WRITE_MPEG_REG(0x2012, READ_MPEG_REG(0x2012) & ~(1 << 18));
	WRITE_MPEG_REG(0x2013, READ_MPEG_REG(0x2013) & ~(1 << 18));    
#endif

	msleep(10);
	
	//LCD_PWR_EN: GPIOA_27
	WRITE_MPEG_REG(0x200d, READ_MPEG_REG(0x200d) | (1 << 27));    
    //WRITE_MPEG_REG(0x200c, READ_MPEG_REG(0x200c) & ~(1 << 27));
    msleep(10);
}

static void lvds_port_enable(void)
{	
	printk("\n\nLVDS port enable.\n");	
	WRITE_MPEG_REG( LVDS_GEN_CNTL, (READ_MPEG_REG(LVDS_GEN_CNTL) | (1 << 3))); // enable fifo
	//enable minilvds_data channel
	WRITE_MPEG_REG(LVDS_PHY_CNTL4, READ_MPEG_REG(LVDS_PHY_CNTL4) | (0x2f<<0));
	set_pwm_3d_tcon();
	control_lcd_3d(0);  //for test
}

static void lvds_port_disable(void)
{		
	//disable minilvds_data channel
	WRITE_MPEG_REG(LVDS_PHY_CNTL4, READ_MPEG_REG(LVDS_PHY_CNTL4) & ~(0x7f<<0));	
	WRITE_MPEG_REG( LVDS_GEN_CNTL, (READ_MPEG_REG(LVDS_GEN_CNTL) & ~(1 << 3))); // enable fifo
	control_lcd_3d(0);
}
#ifdef CONFIG_AM_LOGO
extern void (*Power_on_bl)(void);
//called when kernel logo is displayed.
//backlight will be powered on right here
static void power_on_bl(void)
{
    power_on_backlight();
}
#endif
static void t13_power_on(void)
{
    //video_dac_disable();	
    printk("\n\n t13_power_on.\n\n");
	power_on_lcd();	
   // PRINT_INFO("\n\nt13_power_on...\n\n");
    Power_on_bl = power_on_bl;
    set_backlight_level(250);
}
static void t13_power_off(void)
{
    	power_off_lcd();
}

static void t13_io_init(void)
{
    printk("\n\nT13 LCD Init.\n\n");    
    power_off_lcd();	
}

static struct platform_device lcd_dev = {
    .name = "tcon-dev",
    .id   = 0,
    .num_resources = ARRAY_SIZE(lcd_resources),
    .resource      = lcd_resources,
};

static int __init t13_init(void)
{
    int ret ;
    
    t13_setup_gama_table(&lcd_config, 0);
    t13_io_init();

    platform_device_register(&lcd_dev);
    ret = class_register(&enable3d_class);
	if(ret){
		printk(" class register enable3d_class fail!\n");
	}

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
