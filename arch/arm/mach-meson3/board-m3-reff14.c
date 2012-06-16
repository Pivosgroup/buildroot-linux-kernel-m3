/*
 *
 * arch/arm/mach-meson/meson.c
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * License terms: GNU General Public License (GPL) version 2
 * Platform machine definition.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/device.h>
#include <linux/spi/flash.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/memory.h>
#include <mach/clock.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <mach/lm.h>
#include <asm/memory.h>
#include <asm/mach/map.h>
#include <mach/nand.h>
#include <linux/i2c.h>
#include <linux/i2c-aml.h>
#include <mach/power_gate.h>
#include <linux/aml_bl.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <mach/card_io.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <mach/clk_set.h>
#include "board-m3-reff14.h"


#ifdef CONFIG_USB_ANDROID
#include <linux/usb/android_composite.h>
#endif

#ifdef CONFIG_SUSPEND
#include <mach/pm.h>
#endif

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE
#include <media/amlogic/aml_camera.h>
#include <linux/camera/amlogic_camera_common.h>
#endif

#ifdef CONFIG_GPS_POWERCTRL
#include <linux/gps_power.h>
#endif


#ifdef CONFIG_EFUSE
#include <linux/efuse.h>
#endif

#ifdef CONFIG_SARADC_AM
#include <linux/saradc.h>
static struct platform_device saradc_device = {
    .name = "saradc",
    .id = 0,
    .dev = {
        .platform_data = NULL,
    },
};
#endif

#if defined(CONFIG_INPUT_LSM303DLM_ACCEL)||defined(CONFIG_INPUT_LSM303DLM_MAGNE)
#include <linux/i2c/lsm303dlm.h>
#endif
#ifdef CONFIG_INPUT_L3G4200D
#include <linux/i2c/l3g4200d.h>
#endif

#if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
#include <linux/input.h>
#include <linux/adc_keypad.h>

static struct adc_key adc_kp_key[] = {
    {KEY_MENU,          "menu", CHAN_4, 0, 60},
    {KEY_VOLUMEDOWN,    "vol-", CHAN_4, 140, 60},
    {KEY_VOLUMEUP,      "vol+", CHAN_4, 266, 60},
    {KEY_BACK,          "exit", CHAN_4, 386, 60},
    {KEY_HOMEPAGE,          "home", CHAN_4, 508, 60},
    {KEY_SEARCH,          "search", CHAN_4, 620, 60},
};

static struct adc_kp_platform_data adc_kp_pdata = {
    .key = &adc_kp_key[0],
    .key_num = ARRAY_SIZE(adc_kp_key),
};

static struct platform_device adc_kp_device = {
    .name = "m1-adckp",
    .id = 0,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
    .platform_data = &adc_kp_pdata,
    }
};
#endif

#if defined(CONFIG_KEY_INPUT_CUSTOM_AM) || defined(CONFIG_KEY_INPUT_CUSTOM_AM_MODULE)
#include <linux/input.h>
#include <linux/input/key_input.h>

int _key_code_list[] = {KEY_POWER};

static inline int key_input_init_func(void)
{
    WRITE_AOBUS_REG(AO_RTC_ADDR0, (READ_AOBUS_REG(AO_RTC_ADDR0) &~(1<<11)));
    WRITE_AOBUS_REG(AO_RTC_ADDR1, (READ_AOBUS_REG(AO_RTC_ADDR1) &~(1<<3)));
    return 0;
}
static inline int key_scan(int *key_state_list)
{
    int ret = 0;
    key_state_list[0] = ((READ_AOBUS_REG(AO_RTC_ADDR1) >> 2) & 1) ? 0 : 1;
    return ret;
}

static  struct key_input_platform_data  key_input_pdata = {
    .scan_period = 20,
    .fuzz_time = 60,
    .key_code_list = &_key_code_list[0],
    .key_num = ARRAY_SIZE(_key_code_list),
    .scan_func = key_scan,
    .init_func = key_input_init_func,
    .config = 0,
};

static struct platform_device input_device_key = {
    .name = "m1-keyinput",
    .id = 0,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &key_input_pdata,
    }
};
#endif
#ifdef CONFIG_INPUT_L3G4200D
struct l3g4200d_gyr_platform_data l3g4200d_gyro_plt_dat = {
	.fs_range = L3G4200D_GYR_FS_2000DPS,
	.axis_map_x = 0,	  //x = x
	.axis_map_y = 1,      //y = y
	.axis_map_z = 2,      //z = z
	.negate_x = 0,      //x = +x 
	.negate_y = 0,      //y = +y 
	.negate_z = 0,      //z = +z 
};
#endif

#if defined(CONFIG_FB_AM)
static struct resource fb_device_resources[] = {
    [0] = {
        .start = OSD1_ADDR_START,
        .end   = OSD1_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
#if defined(CONFIG_FB_OSD2_ENABLE)
    [1] = {
        .start = OSD2_ADDR_START,
        .end   = OSD2_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
#endif
};

static struct platform_device fb_device = {
    .name       = "mesonfb",
    .id         = 0,
    .num_resources = ARRAY_SIZE(fb_device_resources),
    .resource      = fb_device_resources,
};
#endif
#ifdef CONFIG_USB_PHY_CONTROL
static struct resource usb_phy_control_device_resources[] = {
	{
		.start = CBUS_REG_ADDR(PREI_USB_PHY_REG),
		.end = -1,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device usb_phy_control_device = {
	.name = "usb_phy_control",
	.id = -1,
	.resource = usb_phy_control_device_resources,
};
#endif
#ifdef CONFIG_USB_DWC_OTG_HCD
static void set_usb_a_vbus_power(char is_power_on)
{
#define USB_A_POW_GPIO          GPIOD_bank_bit0_9(9)
#define USB_A_POW_GPIO_BIT      GPIOD_bit_bit0_9(9)
#define USB_A_POW_GPIO_BIT_ON   1
#define USB_A_POW_GPIO_BIT_OFF  0
    if(is_power_on) {
        printk(KERN_INFO "set usb port power on (board gpio %d)!\n",USB_A_POW_GPIO_BIT);
        set_gpio_mode(USB_A_POW_GPIO, USB_A_POW_GPIO_BIT, GPIO_OUTPUT_MODE);
        set_gpio_val(USB_A_POW_GPIO, USB_A_POW_GPIO_BIT, USB_A_POW_GPIO_BIT_ON);
    } else    {
        printk(KERN_INFO "set usb port power off (board gpio %d)!\n",USB_A_POW_GPIO_BIT);
        set_gpio_mode(USB_A_POW_GPIO, USB_A_POW_GPIO_BIT, GPIO_OUTPUT_MODE);
        set_gpio_val(USB_A_POW_GPIO, USB_A_POW_GPIO_BIT, USB_A_POW_GPIO_BIT_OFF);
    }
}
//usb_a is OTG port
static struct lm_device usb_ld_a = {
    .type = LM_DEVICE_TYPE_USB,
    .id = 0,
    .irq = INT_USB_A,
    .resource.start = IO_USB_A_BASE,
    .resource.end = -1,
    .dma_mask_room = DMA_BIT_MASK(32),
    .port_type = USB_PORT_TYPE_OTG,
    .port_speed = USB_PORT_SPEED_DEFAULT,
    .dma_config = USB_DMA_BURST_SINGLE,
    .set_vbus_power = set_usb_a_vbus_power,
};
static struct lm_device usb_ld_b = {
    .type = LM_DEVICE_TYPE_USB,
    .id = 1,
    .irq = INT_USB_B,
    .resource.start = IO_USB_B_BASE,
    .resource.end = -1,
    .dma_mask_room = DMA_BIT_MASK(32),
    .port_type = USB_PORT_TYPE_HOST,
    .port_speed = USB_PORT_SPEED_DEFAULT,
    .dma_config = USB_DMA_BURST_SINGLE , //   USB_DMA_DISABLE,
    .set_vbus_power = 0,
};

#endif

#if defined(CONFIG_AM_STREAMING)
static struct resource codec_resources[] = {
    [0] = {
        .start =  CODEC_ADDR_START,
        .end   = CODEC_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = STREAMBUF_ADDR_START,
	 .end = STREAMBUF_ADDR_END,
	 .flags = IORESOURCE_MEM,
    },
};

static struct platform_device codec_device = {
    .name       = "amstream",
    .id         = 0,
    .num_resources = ARRAY_SIZE(codec_resources),
    .resource      = codec_resources,
};
#endif

#if defined(CONFIG_AM_DEINTERLACE) || defined (CONFIG_DEINTERLACE)
static struct resource deinterlace_resources[] = {
    [0] = {
        .start =  DI_ADDR_START,
        .end   = DI_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device deinterlace_device = {
    .name       = "deinterlace",
    .id         = 0,
    .num_resources = ARRAY_SIZE(deinterlace_resources),
    .resource      = deinterlace_resources,
};
#endif

#if defined(CONFIG_TVIN_VDIN)
static struct resource vdin_resources[] = {
    [0] = {
        .start =  VDIN_ADDR_START,  //pbufAddr
        .end   = VDIN_ADDR_END,     //pbufAddr + size
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = VDIN_ADDR_START,
        .end   = VDIN_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
    [2] = {
        .start = INT_VDIN_VSYNC,
        .end   = INT_VDIN_VSYNC,
        .flags = IORESOURCE_IRQ,
    },
    [3] = {
        .start = INT_VDIN_VSYNC,
        .end   = INT_VDIN_VSYNC,
        .flags = IORESOURCE_IRQ,
    },
};

static struct platform_device vdin_device = {
    .name       = "vdin",
    .id         = -1,
    .num_resources = ARRAY_SIZE(vdin_resources),
    .resource      = vdin_resources,
};
#endif

#ifdef CONFIG_TVIN_BT656IN
//add pin mux info for bt656 input
#if 0
static struct resource bt656in_resources[] = {
    [0] = {
        .start =  VDIN_ADDR_START,      //pbufAddr
        .end   = VDIN_ADDR_END,             //pbufAddr + size
        .flags = IORESOURCE_MEM,
    },
    [1] = {     //bt656/camera/bt601 input resource pin mux setting
        .start =  0x3000,       //mask--mux gpioD 15 to bt656 clk;  mux gpioD 16:23 to be bt656 dt_in
        .end   = PERIPHS_PIN_MUX_5 + 0x3000,
        .flags = IORESOURCE_MEM,
    },

    [2] = {         //camera/bt601 input resource pin mux setting
        .start =  0x1c000,      //mask--mux gpioD 12 to bt601 FIQ; mux gpioD 13 to bt601HS; mux gpioD 14 to bt601 VS;
        .end   = PERIPHS_PIN_MUX_5 + 0x1c000,
        .flags = IORESOURCE_MEM,
    },

    [3] = {         //bt601 input resource pin mux setting
        .start =  0x800,        //mask--mux gpioD 24 to bt601 IDQ;;
        .end   = PERIPHS_PIN_MUX_5 + 0x800,
        .flags = IORESOURCE_MEM,
    },

};
#endif

static struct platform_device bt656in_device = {
    .name       = "amvdec_656in",
    .id         = -1,
//    .num_resources = ARRAY_SIZE(bt656in_resources),
//    .resource      = bt656in_resources,
};
#endif

#if defined(CONFIG_CARDREADER)
static struct resource amlogic_card_resource[] = {
    [0] = {
        .start = 0x1200230,   //physical address
        .end   = 0x120024c,
        .flags = 0x200,
    }
};
/* WIFI ON Flag */
static int WIFI_ON;
/* BT ON Flag */
static int BT_ON;
/* WL_BT_REG_ON control function */
static void reg_on_control(int is_on)
{
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<11));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<18));
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<8));
    
    if(is_on){
		SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
    }
	else{
        /* only pull donw reg_on pin when wifi and bt off */
        if((!WIFI_ON) && (!BT_ON)){
		    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
            printk("WIFI BT Power down\n");
        }
	}
}

void extern_wifi_power(int is_power)
{
    WIFI_ON = is_power;
    reg_on_control(is_power);
    /*
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<11));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<18));
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<8));
	if(is_power)
		SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
	else
		CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
		*/
}

EXPORT_SYMBOL(extern_wifi_power);

#define GPIO_WIFI_HOSTWAKE  ((GPIOX_bank_bit0_31(11)<<16) |GPIOX_bit_bit0_31(11))

void sdio_extern_init(void)
{
    #if defined(CONFIG_BCM4329_HW_OOB) || defined(CONFIG_BCM4329_OOB_INTR_ONLY)/* Jone add */
    gpio_direction_input(GPIO_WIFI_HOSTWAKE);
    gpio_enable_level_int(gpio_to_idx(GPIO_WIFI_HOSTWAKE), 0, 4);
    gpio_enable_edge_int(gpio_to_idx(GPIO_WIFI_HOSTWAKE), 0, 4);
    #endif /* (CONFIG_BCM4329_HW_OOB) || (CONFIG_BCM4329_OOB_INTR_ONLY) Jone add */
    extern_wifi_power(1);
}

static struct aml_card_info  amlogic_card_info[] = {
    [0] = {
        .name = "sd_card",
        .work_mode = CARD_HW_MODE,
        .io_pad_type = SDIO_B_CARD_0_5,
        .card_ins_en_reg = CARD_GPIO_ENABLE,
        .card_ins_en_mask = PREG_IO_29_MASK,
        .card_ins_input_reg = CARD_GPIO_INPUT,
        .card_ins_input_mask = PREG_IO_29_MASK,
        .card_power_en_reg = CARD_GPIO_ENABLE,
        .card_power_en_mask = PREG_IO_31_MASK,
        .card_power_output_reg = CARD_GPIO_OUTPUT,
        .card_power_output_mask = PREG_IO_31_MASK,
        .card_power_en_lev = 0,
        .card_wp_en_reg = 0,
        .card_wp_en_mask = 0,
        .card_wp_input_reg = 0,
        .card_wp_input_mask = 0,
        .card_extern_init = 0,
    },
    [1] = {
        .name = "sdio_card",
        .work_mode = CARD_HW_MODE,
        .io_pad_type = SDIO_A_GPIOX_0_3,
        .card_ins_en_reg = 0,
        .card_ins_en_mask = 0,
        .card_ins_input_reg = 0,
        .card_ins_input_mask = 0,
        .card_power_en_reg = EGPIO_GPIOC_ENABLE,
        .card_power_en_mask = PREG_IO_7_MASK,
        .card_power_output_reg = EGPIO_GPIOC_OUTPUT,
        .card_power_output_mask = PREG_IO_7_MASK,
        .card_power_en_lev = 1,
        .card_wp_en_reg = 0,
        .card_wp_en_mask = 0,
        .card_wp_input_reg = 0,
        .card_wp_input_mask = 0,
        .card_extern_init = sdio_extern_init,
    },
};

void extern_wifi_reset(int is_on)
{
    unsigned int val;
    
    /*output*/
    val = readl(amlogic_card_info[1].card_power_en_reg);
    val &= ~(amlogic_card_info[1].card_power_en_mask);
    writel(val, amlogic_card_info[1].card_power_en_reg);
        
    if(is_on){
        /*high*/
        val = readl(amlogic_card_info[1].card_power_output_reg);
        val |=(amlogic_card_info[1].card_power_output_mask);
        writel(val, amlogic_card_info[1].card_power_output_reg);
        printk("on val = %x\n", val);
    }
    else{
        /*low*/
        val = readl(amlogic_card_info[1].card_power_output_reg);
        val &=~(amlogic_card_info[1].card_power_output_mask);
        writel(val, amlogic_card_info[1].card_power_output_reg);
        printk("off val = %x\n", val);
    }

    printk("ouput %x, bit %d, level %x, bit %d\n",
            amlogic_card_info[1].card_power_en_reg,
            amlogic_card_info[1].card_power_en_mask,
            amlogic_card_info[1].card_power_output_reg,
            amlogic_card_info[1].card_power_output_mask);
    return;
}
EXPORT_SYMBOL(extern_wifi_reset);

static struct aml_card_platform amlogic_card_platform = {
    .card_num = ARRAY_SIZE(amlogic_card_info),
    .card_info = amlogic_card_info,
};

static struct platform_device amlogic_card_device = { 
    .name = "AMLOGIC_CARD", 
    .id    = -1,
    .num_resources = ARRAY_SIZE(amlogic_card_resource),
    .resource = amlogic_card_resource,
    .dev = {
        .platform_data = &amlogic_card_platform,
    },
};

#endif

#if defined(CONFIG_AML_AUDIO_DSP)
static struct resource audiodsp_resources[] = {
    [0] = {
        .start = AUDIODSP_ADDR_START,
        .end   = AUDIODSP_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device audiodsp_device = {
    .name       = "audiodsp",
    .id         = 0,
    .num_resources = ARRAY_SIZE(audiodsp_resources),
    .resource      = audiodsp_resources,
};
#endif

static struct resource aml_m3_audio_resource[] = {
    [0] =   {
        .start  =   0,
        .end        =   0,
        .flags  =   IORESOURCE_MEM,
    },
};

extern char* get_vout_mode_internal(void);

/* Check current mode, 0: panel; 1: !panel*/
int get_display_mode(void) {
	int ret = 0;
	if(strncmp("panel", get_vout_mode_internal(), 5))
		ret = 1;
	return ret;
}

#if defined(CONFIG_SND_AML_M3)
static struct platform_device aml_audio = {
    .name               = "aml_m3_audio",
    .id                     = -1,
    .resource       =   aml_m3_audio_resource,
    .num_resources  =   ARRAY_SIZE(aml_m3_audio_resource),
};

int aml_m3_is_hp_pluged(void)
{
	if(get_display_mode() != 0) //if !panel, return 1 to mute spk		
		return 1;
		
	return !READ_CBUS_REG_BITS(PREG_PAD_GPIO0_I, 19, 1); //return 1: hp pluged, 0: hp unpluged.
}


struct aml_m3_platform_data {
    int (*is_hp_pluged)(void);
};

void mute_spk(struct snd_soc_codec* codec, int flag)
{
#ifdef _AML_M3_HW_DEBUG_
	printk("***Entered %s:%s\n", __FILE__,__func__);
#endif
    if(flag){
		set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 0);	 // mute speak
		set_gpio_mode(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), GPIO_OUTPUT_MODE);
	}else{
		set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 1);	 // unmute speak
		set_gpio_mode(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), GPIO_OUTPUT_MODE);
	}
}


static struct aml_m3_platform_data aml_m3_pdata = {
    .is_hp_pluged = &aml_m3_is_hp_pluged,
};
#endif

#ifdef  CONFIG_INPUT_LSM303DLM_ACCEL
struct lsm303dlm_acc_platform_data lsm303dlc_acc_plt_dat = {
	.poll_interval = 50,          //Driver polling interval as 50ms
	.min_interval = 10,    //Driver polling interval minimum 10ms
	.g_range = LSM303DLM_ACC_G_2G,    //Full Scale of LSM303DLH Accelerometer
	.axis_map_x = 0,      //x = x
	.axis_map_y = 1,      //y = y
	.axis_map_z = 2,      //z = z
	.negate_x = 0,      //x = +x
	.negate_y = 0,      //y = +y
	.negate_z = 1,      //z = +z
	.gpio_int1=-1,
	.gpio_int2=-1,
};
#endif

#ifdef CONFIG_INPUT_LSM303DLM_MAGNE
struct lsm303dlm_mag_platform_data lsm303dlc_mag_plt_dat = {
	.poll_interval = 50,    //Driver polling interval as 50ms
	.min_interval = 10,    //Driver polling interval minimum 10ms
	.h_range = LSM303DLM_MAG_H_8_1G,  //Full Scale of LSM303DLH Magnetometer
	.axis_map_x = 0,      //x = x
	.axis_map_y = 1,      //y = y
	.axis_map_z = 2,      //z = z
	.negate_x = 0,      //x = +x
	.negate_y = 0,      //y = +y
	.negate_z = 0,      //z = +z
};
#endif

#ifdef CONFIG_FOCALTECH_CAPACITIVE_TOUCHSCREEN
#include <linux/ft5x06_ts.h>
/* GPIOD_24 */
#define GPIO_KEY_LED  ((GPIOA_bank_bit0_27(18)<<16) |GPIOA_bit_bit0_27(18))
#define GPIO_FT_RST  ((GPIOC_bank_bit0_15(3)<<16) |GPIOC_bit_bit0_15(3))
#define GPIO_FT_IRQ  ((GPIOA_bank_bit0_27(16)<<16) |GPIOA_bit_bit0_27(16))
#define FT_IRQ	INT_GPIO_0

static void key_led_ctrl(int on)
{
	gpio_direction_output(GPIO_KEY_LED, on);
}

static void ts_power(int on)
{
	gpio_direction_output(GPIO_FT_RST, on);
}

static int ts_init_irq(void)
{
	gpio_direction_input(GPIO_FT_IRQ);
	gpio_enable_edge_int(gpio_to_idx(GPIO_FT_IRQ), 1, FT_IRQ - INT_GPIO_0);

	return 0;
}

static int ts_get_irq_level(void)
{
	return gpio_get_value(GPIO_FT_IRQ);
}

static int IS_AC_connected(void)
{
	//return (READ_CBUS_REG(ASSIST_HW_REV)&(1<<9))? 1:0;//GP_INPUT1
	return 0;
}

static struct tp_key tp_key_list[] = {
	{KEY_HOME, 1280, 1400, 748, 768},
	{KEY_MENU, 1280, 1400, 688, 728},
	{KEY_BACK, 1280, 1400, 628, 668},
};

static struct ts_platform_data ts_pdata = {
    .irq = FT_IRQ,
    .init_irq = ts_init_irq,
    .get_irq_level = ts_get_irq_level,
    .power = ts_power,
    .Ac_is_connect= IS_AC_connected,
    .screen_max_x=1024,
    .screen_max_y=600,
    .swap_xy = 0,
    .xpol = 0,
    .ypol = 0,
    .tp_key = &tp_key_list[0],
    .tp_key_num = ARRAY_SIZE(tp_key_list),
    .key_led_ctrl = key_led_ctrl,
};

#endif


#if defined(CONFIG_AML_RTC)
static  struct platform_device aml_rtc_device = {
            .name            = "aml_rtc",
            .id               = -1,
    };
#endif

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308
int gc0308_init(void)
{
    // set camera power disable
    set_gpio_val(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), 0);    // set camera power disable
    set_gpio_mode(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), GPIO_OUTPUT_MODE);
    msleep(20);

    set_gpio_val(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), 1);    // set camera power disable
    set_gpio_mode(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), GPIO_OUTPUT_MODE);
    msleep(20);
    
    // set camera power enable
    set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 0);    // set camera power enable
    set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
    msleep(20);
    return 0;
}
#endif

#if defined (CONFIG_AMLOGIC_VIDEOIN_MANAGER)
static struct resource vm_resources[] = {
    [0] = {
        .start =  VM_ADDR_START,
        .end   = VM_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};
static struct platform_device vm_device =
{
	.name = "vm",
	.id = 0,
    .num_resources = ARRAY_SIZE(vm_resources),
    .resource      = vm_resources,
};
#endif /* AMLOGIC_VIDEOIN_MANAGER */

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE
static void __init camera_power_on_init(void)
{
    udelay(1000);
    SET_CBUS_REG_MASK(HHI_ETH_CLK_CNTL,0x30f);// 24M XTAL
    SET_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL,0x232);// 24M XTAL

    //eth_set_pinmux(ETH_BANK0_GPIOC3_C12,ETH_CLK_OUT_GPIOC12_REG3_1, 1);		
}
#endif

#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308)
static void gc0308_v4l2_init(void)
{
	printk( "amlogic camera driver: gc0308_v4l2_init. \n");
	gc0308_init();
}
static void gc0308_v4l2_uninit(void)
{

	printk( "amlogic camera driver: gc0308_v4l2_uninit. \n");
    set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 1);    // set camera power disable
    set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
}
static void gc0308_v4l2_early_suspend(void)
{
    set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 1);    // set camera power disable
    set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
}

static void gc0308_v4l2_late_resume(void)
{
    set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 0);    // set camera power enable
    set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
}

struct aml_camera_i2c_fig1_s custom_gc0308_script[] = {  
	{0x14,0x11}, // h_v
	{0x26,0x01},
	{0xff,0xff}, 
};


aml_plat_cam_data_t video_gc0308_data = {
	.name="video-gc0308",
	.video_nr=1,//1,
	.device_init= gc0308_v4l2_init,
	.device_uninit=gc0308_v4l2_uninit,
	.custom_init_script = custom_gc0308_script,
	.early_suspend = gc0308_v4l2_early_suspend,
	.late_resume = gc0308_v4l2_late_resume,
};

#endif

#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005)
struct aml_camera_i2c_fig_s custom_gt2005_script[] = {  
	{0x0101,0x03}, 
	{0xffff,0xff}, 
};

static void gt2005_v4l2_init(void)
{
    // reset low 
    printk( "amlogic camera driver: gt2005_v4l2_init. \n");
    set_gpio_val(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), 0);   
    set_gpio_mode(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), GPIO_OUTPUT_MODE);
	
    // set camera power disanable
    set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 0);    
    set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
	
    msleep(20);
	
	// reset high
    set_gpio_val(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), 1);    
    set_gpio_mode(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), GPIO_OUTPUT_MODE);
    msleep(20);
    
    // set camera power enable
    set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 1);    
    set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
    msleep(20);
}
static void gt2005_v4l2_uninit(void)
{
	printk( "amlogic camera driver: gt2005_v4l2_uninit.\n");
    set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 0);    // set camera power disable
    set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
}
static void gt2005_v4l2_disable(void)
{

}

static void gt2005_v4l2_early_suspend(void)
{
	printk( "amlogic camera driver: gt2005_v4l2_early_suspend. \n");
    set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 0);    // set camera power disable
    set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
}

static void gt2005_v4l2_late_resume(void)
{
	printk( "amlogic camera driver: gt2005_v4l2_late_resume. \n");
    set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 1);    // set camera power enable
    set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
}

aml_plat_cam_data_t video_gt2005_data = {
	.name="video-gt2005",
	.video_nr=0,   //    1
	.device_init= gt2005_v4l2_init,
	.device_uninit=gt2005_v4l2_uninit,
	.custom_init_script = custom_gt2005_script,
	.early_suspend = gt2005_v4l2_early_suspend,
	.late_resume = gt2005_v4l2_late_resume,
	.device_disable=gt2005_v4l2_disable,
};
#endif /* VIDEO_AMLOGIC_CAPTURE_GT2005 */

#if defined(CONFIG_SUSPEND)

typedef struct {
	char name[32];
	unsigned bank;
	unsigned bit;
	gpio_mode_t mode;
	unsigned value;
	unsigned enable;
} gpio_data_t;

#define MAX_GPIO 0
static gpio_data_t gpio_data[MAX_GPIO] = {
};	

static void save_gpio(int port) 
{
	gpio_data[port].mode = get_gpio_mode(gpio_data[port].bank, gpio_data[port].bit);
	if (gpio_data[port].mode==GPIO_OUTPUT_MODE)
	{
		if (gpio_data[port].enable){
			printk("change %s output %d to input\n", gpio_data[port].name, gpio_data[port].value); 
			gpio_data[port].value = get_gpio_val(gpio_data[port].bank, gpio_data[port].bit);
			set_gpio_mode(gpio_data[port].bank, gpio_data[port].bit, GPIO_INPUT_MODE);
		}
		else{
			printk("no change %s output %d\n", gpio_data[port].name, gpio_data[port].value); 
		}
	}
}

static void restore_gpio(int port)
{
	if ((gpio_data[port].mode==GPIO_OUTPUT_MODE)&&(gpio_data[port].enable))
	{
		set_gpio_val(gpio_data[port].bank, gpio_data[port].bit, gpio_data[port].value);
		set_gpio_mode(gpio_data[port].bank, gpio_data[port].bit, GPIO_OUTPUT_MODE);
		// printk("%s output %d\n", gpio_data[port].name, gpio_data[port].value); 
	}
}

typedef struct {
	char name[32];
	unsigned reg;
	unsigned bits;
	unsigned enable;
} pinmux_data_t;


#define MAX_PINMUX	1

pinmux_data_t pinmux_data[MAX_PINMUX] = {
	{"HDMI", 	0, (1<<2)|(1<<1)|(1<<0), 						1},
};

static unsigned pinmux_backup[6];

static void save_pinmux(void)
{
	int i;
	for (i=0;i<6;i++)
		pinmux_backup[i] = READ_CBUS_REG(PERIPHS_PIN_MUX_0+i);
	for (i=0;i<MAX_PINMUX;i++){
		if (pinmux_data[i].enable){
			printk("%s %x\n", pinmux_data[i].name, pinmux_data[i].bits);
			clear_mio_mux(pinmux_data[i].reg, pinmux_data[i].bits);
		}
	}
}

static void restore_pinmux(void)
{
	int i;
	for (i=0;i<6;i++)
		 WRITE_CBUS_REG(PERIPHS_PIN_MUX_0+i, pinmux_backup[i]);
}

static void set_vccx2(int power_on)
{
	int i;
    if (power_on){

		restore_pinmux();
		for (i=0;i<MAX_GPIO;i++)
			restore_gpio(i);
        printk(KERN_INFO "set_vccx2 power up\n");
        set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 0);
              
    }
    else{
        printk(KERN_INFO "set_vccx2 power down\n");        
        set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 1);
		save_pinmux();
		for (i=0;i<MAX_GPIO;i++)
			save_gpio(i);
    }
}

static struct meson_pm_config aml_pm_pdata = {
    .pctl_reg_base = IO_APB_BUS_BASE,
    .mmc_reg_base = APB_REG_ADDR(0x1000),
    .hiu_reg_base = CBUS_REG_ADDR(0x1000),
    .power_key = (1<<8),
    .ddr_clk = 0x00110820,
    .sleepcount = 128,
    .set_vccx2 = set_vccx2,
    .core_voltage_adjust = 7,  //5,8
};

static struct platform_device aml_pm_device = {
    .name           = "pm-meson",
    .dev = {
        .platform_data  = &aml_pm_pdata,
    },
    .id             = -1,
};
#endif


#if defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)
static struct aml_i2c_platform aml_i2c_plat = {
    .wait_count     = 50000,
    .wait_ack_interval  = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
    .master_no      = AML_I2C_MASTER_A,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_300K,

    .master_pinmux = {
        .scl_reg    = MESON_I2C_MASTER_GPIOX_26_REG,
        .scl_bit    = MESON_I2C_MASTER_GPIOX_26_BIT,
        .sda_reg    = MESON_I2C_MASTER_GPIOX_25_REG,
        .sda_bit    = MESON_I2C_MASTER_GPIOX_25_BIT,
    }
};

static struct aml_i2c_platform aml_i2c_plat1 = {
    .wait_count     = 50000,
    .wait_ack_interval  = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
    .master_no      = AML_I2C_MASTER_B,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_300K,

    .master_pinmux = {
        .scl_reg    = MESON_I2C_MASTER_GPIOX_28_REG,
        .scl_bit    = MESON_I2C_MASTER_GPIOX_28_BIT,
        .sda_reg    = MESON_I2C_MASTER_GPIOX_27_REG,
        .sda_bit    = MESON_I2C_MASTER_GPIOX_27_BIT,
    }
};

static struct aml_i2c_platform aml_i2c_plat2 = {
    .wait_count     = 50000,
    .wait_ack_interval  = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
    .master_no      = AML_I2C_MASTER_AO,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_100K,

    .master_pinmux = {
        .scl_reg    = MESON_I2C_MASTER_GPIOAO_4_REG,
        .scl_bit    = MESON_I2C_MASTER_GPIOAO_4_BIT,
        .sda_reg    = MESON_I2C_MASTER_GPIOAO_5_REG,
        .sda_bit    = MESON_I2C_MASTER_GPIOAO_5_BIT,
    }
};

static struct resource aml_i2c_resource[] = {
	[0]= {
		.start =    MESON_I2C_MASTER_A_START,
		.end   =    MESON_I2C_MASTER_A_END,
		.flags =    IORESOURCE_MEM,
	}
};

static struct resource aml_i2c_resource1[] = {
	[0]= {
		.start =    MESON_I2C_MASTER_A_START,
		.end   =    MESON_I2C_MASTER_A_END,
		.flags =    IORESOURCE_MEM,
  }
};

static struct resource aml_i2c_resource2[] = {
	[0]= {
		.start =    MESON_I2C_MASTER_AO_START,
		.end   =    MESON_I2C_MASTER_AO_END,
		.flags =    IORESOURCE_MEM,
	}
};

static struct platform_device aml_i2c_device = {
    .name         = "aml-i2c",
    .id       = 0,
    .num_resources    = ARRAY_SIZE(aml_i2c_resource),
    .resource     = aml_i2c_resource,
    .dev = {
        .platform_data = &aml_i2c_plat,
    },
};

static struct platform_device aml_i2c_device1 = {
    .name         = "aml-i2c",
    .id       = 1,
    .num_resources    = ARRAY_SIZE(aml_i2c_resource1),
    .resource     = aml_i2c_resource1,
    .dev = {
        .platform_data = &aml_i2c_plat1,
    },
};

static struct platform_device aml_i2c_device2 = {
    .name         = "aml-i2c",
    .id       = 2,
    .num_resources    = ARRAY_SIZE(aml_i2c_resource2),
    .resource     = aml_i2c_resource2,
    .dev = {
        .platform_data = &aml_i2c_plat2,
    },
};

#endif

#ifdef CONFIG_GPS_POWERCTRL
static void gps_power_on(void)
{
       printk("power on gps\n");
       set_gpio_val(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), 1);
       set_gpio_mode(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), GPIO_OUTPUT_MODE);
}

static void gps_power_off(void)
{
       printk("power off gps\n");
       set_gpio_val(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), 0);
       set_gpio_mode(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), GPIO_OUTPUT_MODE);
}

static struct gps_power_platform_data aml_gps_plat = {
       .power_on = gps_power_on,
       .power_off = gps_power_off,
};

static struct platform_device aml_gps_device = {
    .name         = "gps_power",
    .id       = -1,
    .dev = {
                .platform_data = &aml_gps_plat,
           },
};

static void gps_pinmux_init(void)
{
    //uart_cts_b not connected
    //uart_rts_b not connected
    //uart_tx_b
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_4,(1<<1)||(1<<15));
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4,(1<<5));
    //uart_rx_b
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_4,(1<<0)||(1<<14));
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4,(1<<4));
}
#endif

#ifdef CONFIG_EFUSE
static bool efuse_data_verify(unsigned char *usid)
{  int len;
  
    len = strlen(usid);
    if((len > 8)&&(len<31) )
        return true;
		else
				return false;
}

static struct efuse_platform_data aml_efuse_plat = {
    .pos = 337,
    .count = 30,
    .data_verify = efuse_data_verify,
};

static struct platform_device aml_efuse_device = {
    .name	= "efuse",
    .id	= -1,
    .dev = {
                .platform_data = &aml_efuse_plat,
           },
};
#endif

#ifdef CONFIG_PMU_ACT8xxx
#include <linux/act8xxx.h>  
extern int act8xxx_is_ac_online(void);
//Amlogic Power Management Support

/*
 *	DC_DET(GPIOA_20)	enable internal pullup
 *		High:		Disconnect
 *		Low:		Connect
 */
static inline int gpio_is_ac_online(void)
{
	int val;
	
	SET_CBUS_REG_MASK(PAD_PULL_UP_REG0, (1<<20));	//enable internal pullup
	set_gpio_mode(GPIOA_bank_bit0_27(20), GPIOA_bit_bit0_27(20), GPIO_INPUT_MODE);
	val = get_gpio_val(GPIOA_bank_bit0_27(20), GPIOA_bit_bit0_27(20));
	
	//logd("%s: get from gpio is %d.\n", __FUNCTION__, val);
	
	return !val;
}

static int is_ac_online(void)
{
	return act8xxx_is_ac_online();
}

static void power_off(void)
{
    if(is_ac_online()){ //AC in after power off press
        kernel_restart("charging_reboot");
    }
    
    //BL_PWM power off
    set_gpio_val(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), 0);
    set_gpio_mode(GPIOD_bank_bit0_9(1), GPIOD_bit_bit0_9(1), GPIO_OUTPUT_MODE);

    //VCCx2 power down
    set_vccx2(0);
    
    //Power hold down
    set_gpio_val(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), 0);
    set_gpio_mode(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), GPIO_OUTPUT_MODE);
}

/*
 *	nSTAT OUTPUT(GPIOA_21)	enable internal pullup
 *		High:		Full
 *		Low:		Charging
 */
static inline int get_charge_status(void)
{
	int val;
	
	SET_CBUS_REG_MASK(PAD_PULL_UP_REG0, (1<<21));	//enable internal pullup
	set_gpio_mode(GPIOA_bank_bit0_27(21), GPIOA_bit_bit0_27(21), GPIO_INPUT_MODE);
	val = get_gpio_val(GPIOA_bank_bit0_27(21), GPIOA_bit_bit0_27(21));

	//logd("%s: get from gpio is %d.\n", __FUNCTION__, val);
	
	return val;
}

/*
 *	Fast charge when CHG_CON(GPIOAO_11) is High.
 *	Slow charge when CHG_CON(GPIOAO_11) is Low.
 */
static int set_charge_current(int level)
{
	set_gpio_mode(GPIOAO_bank_bit0_11(11), GPIOAO_bit_bit0_11(11), GPIO_OUTPUT_MODE);
	set_gpio_val(GPIOAO_bank_bit0_11(11), GPIOAO_bit_bit0_11(11), (level ? 1 : 0));
	return 0;
}

//temporary
static int set_bat_off(void)
{
	return 0;
}

/*
//temporary
static inline int is_usb_online(void)
{
	u8 val;

	return 0;
}
*/

#ifdef CONFIG_PMU_ACT8942
static inline int get_bat_adc_value(void)
{
	return get_adc_sample(5);
}

/*
 *	When BAT_SEL(GPIOA_22) is High Vbat=Vadc*2
 */
static inline int measure_voltage(void)
{
	int val;
	msleep(2);
	set_gpio_mode(GPIOA_bank_bit0_27(22), GPIOA_bit_bit0_27(22), GPIO_OUTPUT_MODE);
	set_gpio_val(GPIOA_bank_bit0_27(22), GPIOA_bit_bit0_27(22), 1);
	val = get_bat_adc_value() * (2 * 2500000 / 1023);
	logd("%s: get from adc is %dmV.\n", __FUNCTION__, val);
	return val;
}

/*
 *	Get Vhigh when BAT_SEL(GPIOA_22) is High.
 *	Get Vlow when BAT_SEL(GPIOA_22) is Low.
 *	I = Vdiff / 0.02R
 *	Vdiff = Vhigh - Vlow
 */
static inline int measure_current(void)
{
	int val, Vh, Vl, Vdiff;
	set_gpio_mode(GPIOA_bank_bit0_27(22), GPIOA_bit_bit0_27(22), GPIO_OUTPUT_MODE);
	set_gpio_val(GPIOA_bank_bit0_27(22), GPIOA_bit_bit0_27(22), 1);
	msleep(2);
	Vl = get_adc_sample(5) * (2 * 2500000 / 1023);
	logd("%s: Vh is %dmV.\n", __FUNCTION__, Vh);
	set_gpio_mode(GPIOA_bank_bit0_27(22), GPIOA_bit_bit0_27(22), GPIO_OUTPUT_MODE);
	set_gpio_val(GPIOA_bank_bit0_27(22), GPIOA_bit_bit0_27(22), 0);
	msleep(2);
	Vh = get_adc_sample(5) * (2 * 2500000 / 1023);
	logd("%s: Vl is %dmV.\n", __FUNCTION__, Vl);
	Vdiff = Vh - Vl;
	val = Vdiff * 50;
	logd("%s: get from adc is %dmA.\n", __FUNCTION__, val);
	return val;
}

static inline int measure_capacity(void)
{
	int val, tmp;
	tmp = measure_voltage();
	if((tmp>4200000) || (get_charge_status() == 0x1))
	{
		logd("%s: get from PMU and adc is 100.\n", __FUNCTION__);
		return 100;
	}
	
	val = (tmp - 3600000) / (600000 / 100);
	logd("%s: get from adc is %d.\n", __FUNCTION__, val);
	return val;
}

static int bat_value_table[37]={
0,  //0    
730,//0  
737,//4  
742,//10 
750,//15 
752,//16 
753,//18 
754,//20 
755,//23 
756,//26 
757,//29 
758,//32 
760,//35 
761,//37 
762,//40 
763,//43 
766,//46 
768,//49 
770,//51 
772,//54 
775,//57 
778,//60 
781,//63 
786,//66 
788,//68 
791,//71 
795,//74 
798,//77 
800,//80 
806,//83 
810,//85 
812,//88 
817,//91 
823,//95 
828,//97 
832,//100
835 //100
};

static int bat_charge_value_table[37]={
0,  //0    
770,//0
776,//4
781,//10
788,//15
790,//16
791,//18
792,//20
793,//23
794,//26
795,//29
796,//32
797,//35
798,//37
799,//40
800,//43
803,//46
806,//49
808,//51
810,//54
812,//57
815,//60
818,//63
822,//66
827,//68
830,//71
833,//74
836,//77
838,//80
843,//83
847,//85
849,//88
852,//91
854,//95
855,//97
856,//100
857 //100
}; 

static int bat_level_table[37]={
0,
0,
4,
10,
15,
16,
18,
20,
23,
26,
29,
32,
35,
37,
40,
43,
46,
49,
51,
54,
57,
60,
63,
66,
68,
71,
74,
77,
80,
83,
85,
88,
91,
95,
97,
100,
100  
};

static inline int get_bat_percentage(int adc_vaule, int *adc_table, 
										int *per_table, int table_size)
{
	int i;
	
	for(i=0; i<(table_size - 1); i++) {
		if ((adc_vaule >= adc_table[i]) && (adc_vaule < adc_table[i+1])) {
            break;
		}
	}
	return per_table[i];
}

static int act8942_measure_capacity_charging(void)
{
	//printk("------%s  ", __FUNCTION__);
	int adc = get_bat_adc_value();
	int table_size = ARRAY_SIZE(bat_charge_value_table);
	return get_bat_percentage(adc, bat_charge_value_table, bat_level_table, table_size);
}

static int act8942_measure_capacity_battery(void)
{
	//printk("------%s  ", __FUNCTION__);
	int adc = get_bat_adc_value();
	int table_size = ARRAY_SIZE(bat_value_table);

	return get_bat_percentage(adc, bat_value_table, bat_level_table, table_size);
}

static struct act8942_operations act8942_pdata = {
	.is_ac_online = is_ac_online,
	//.is_usb_online = is_usb_online,
	.set_bat_off = set_bat_off,
	.get_charge_status = get_charge_status,
	.set_charge_current = set_charge_current,
	.measure_voltage = measure_voltage,
	.measure_current = measure_current,
	.measure_capacity_charging = act8942_measure_capacity_charging,
	.measure_capacity_battery = act8942_measure_capacity_battery,
	.update_period = 2000,	//2S
	.asn = 5,				//Average Sample Number
	.rvp = 1,				//reverse voltage protection: 1:enable; 0:disable
};
#endif

static struct platform_device aml_pmu_device = {
    .name	= ACT8xxx_DEVICE_NAME,
    .id	= -1,
};
#endif

#ifdef CONFIG_AM_NAND
static struct mtd_partition multi_partition_info[] = 
{
	{
		.name = "logo",
		.offset = 32*SZ_1M+40*SZ_1M,
		.size = 8*SZ_1M,
	},
	{
		.name = "aml_logo",
		.offset = 48*SZ_1M+40*SZ_1M,
		.size = 8*SZ_1M,
	},
	{
		.name = "recovery",
		.offset = 64*SZ_1M+40*SZ_1M,
		.size = 8*SZ_1M,
	},
	{
		.name = "boot",
		.offset = 96*SZ_1M+40*SZ_1M,
		.size = 8*SZ_1M,
	},
	{
		.name = "system",
		.offset = 128*SZ_1M+40*SZ_1M,
		.size = 512*SZ_1M,
	},
	{
		.name = "cache",
		.offset = 640*SZ_1M+40*SZ_1M,
		.size = 128*SZ_1M,
	},
	{
		.name = "userdata",
		.offset = 768*SZ_1M+40*SZ_1M,
		.size = 512*SZ_1M,
	},
	{
		.name = "NFTL_Part",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
	},
};


static struct aml_nand_platform aml_nand_mid_platform[] = {
{
		.name = NAND_BOOT_NAME,
		.chip_enable_pad = AML_NAND_CE0,
		.ready_busy_pad = AML_NAND_CE0,
		.platform_nand_data = {
			.chip =  {
				.nr_chips = 1,
				.options = (NAND_TIMING_MODE5 | NAND_ECC_BCH60_1K_MODE),
			},
    	},
			.T_REA = 20,
			.T_RHOH = 15,
	},
	{
		.name = NAND_MULTI_NAME,
		.chip_enable_pad = (AML_NAND_CE0 | (AML_NAND_CE1 << 4) | (AML_NAND_CE2 << 8) | (AML_NAND_CE3 << 12)),
		.ready_busy_pad = (AML_NAND_CE0 | (AML_NAND_CE0 << 4) | (AML_NAND_CE1 << 8) | (AML_NAND_CE1 << 12)),
		.platform_nand_data = {
			.chip =  {
				.nr_chips = 4,
				.nr_partitions = ARRAY_SIZE(multi_partition_info),
				.partitions = multi_partition_info,
				.options = (NAND_TIMING_MODE5 | NAND_ECC_BCH60_1K_MODE | NAND_TWO_PLANE_MODE),
			},
    	},
			.T_REA = 20,
			.T_RHOH = 15,
	}
};

struct aml_nand_device aml_nand_mid_device = {
	.aml_nand_platform = aml_nand_mid_platform,
	.dev_num = ARRAY_SIZE(aml_nand_mid_platform),
};

static struct resource aml_nand_resources[] = {
    {
        .start = 0xc1108600,
        .end = 0xc1108624,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device aml_nand_device = {
    .name = "aml_m3_nand",
    .id = 0,
    .num_resources = ARRAY_SIZE(aml_nand_resources),
    .resource = aml_nand_resources,
    .dev = {
		.platform_data = &aml_nand_mid_device,
    },
};
#endif

#if defined(CONFIG_AMLOGIC_BACKLIGHT)
extern void power_on_backlight(void);
extern void power_off_backlight(void);
extern unsigned get_backlight_level(void);
extern void set_backlight_level(unsigned level);

struct aml_bl_platform_data aml_bl_platform =
{    
    .power_on_bl = power_on_backlight,
    .power_off_bl = power_off_backlight,
    .get_bl_level = get_backlight_level,
    .set_bl_level = set_backlight_level,
};

static struct platform_device aml_bl_device = {
    .name = "aml-bl",
    .id = -1,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &aml_bl_platform,
    },
};
#endif
#if  defined(CONFIG_AM_TV_OUTPUT)||defined(CONFIG_AM_TCON_OUTPUT)
static struct resource vout_device_resources[] = {
    [0] = {
        .start = 0,
        .end   = 0,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device vout_device = {
    .name       = "mesonvout",
    .id         = 0,
    .num_resources = ARRAY_SIZE(vout_device_resources),
    .resource      = vout_device_resources,
};
#endif

#ifdef CONFIG_USB_ANDROID
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
static struct usb_mass_storage_platform_data mass_storage_pdata = {
       .nluns = 2,
       .vendor = "Keenhi",
       .product = "ICS",
       .release = 0x0100,
};
static struct platform_device usb_mass_storage_device = {
       .name = "usb_mass_storage",
       .id = -1,
       .dev = {
               .platform_data = &mass_storage_pdata,
               },
};
#endif
static char *usb_functions[] = { "usb_mass_storage" };
static char *usb_functions_adb[] = { 
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
"usb_mass_storage", 
#endif

#ifdef CONFIG_USB_ANDROID_ADB
"adb" 
#endif
};
static struct android_usb_product usb_products[] = {
       {
               .product_id     = 0x0c01,
               .num_functions  = ARRAY_SIZE(usb_functions),
               .functions      = usb_functions,
       },
       {
               .product_id     = 0x0c02,
               .num_functions  = ARRAY_SIZE(usb_functions_adb),
               .functions      = usb_functions_adb,
       },
};

static struct android_usb_platform_data android_usb_pdata = {
       .vendor_id      = 0x0bb4,
       .product_id     = 0x0c01,
       .version        = 0x0100,
       .product_name   = "ICS",
       .manufacturer_name = "Keenhi",
       .num_products = ARRAY_SIZE(usb_products),
       .products = usb_products,
       .num_functions = ARRAY_SIZE(usb_functions_adb),
       .functions = usb_functions_adb,
};

static struct platform_device android_usb_device = {
       .name   = "android_usb",
       .id             = -1,
       .dev            = {
               .platform_data = &android_usb_pdata,
       },
};
#endif

#ifdef CONFIG_POST_PROCESS_MANAGER
static struct resource ppmgr_resources[] = {
    [0] = {
        .start = PPMGR_ADDR_START,
        .end   = PPMGR_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device ppmgr_device = {
    .name       = "ppmgr",
    .id         = 0,
    .num_resources = ARRAY_SIZE(ppmgr_resources),
    .resource      = ppmgr_resources,
};
#endif

#ifdef CONFIG_BT_DEVICE
#include <linux/bt-device.h>

static struct platform_device bt_device = {
	.name             = "bt-dev",
	.id               = -1,
};

static void bt_device_init(void)
{
	/* BT_RST_N */
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<16));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<5));
	
	/* UART_RTS_N(BT) */
	SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (1<<10));
		
	/* UART_CTS_N(BT) */ 
	SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (1<<11));
	
	/* UART_TX(BT) */
	SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (1<<13));
	
	/* UART_RX(BT) */
	SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (1<<12));

    /* BT_WAKE */
    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO4_EN_N, (1 << 10));
    SET_CBUS_REG_MASK(PREG_PAD_GPIO4_O, (1 << 10));
}

static void bt_device_on(void)
{
    /* reg_on */
    BT_ON = 1;
    reg_on_control(1);
	/* BT_RST_N */
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<6));
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));	
	msleep(200);	
	SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));
}

static void bt_device_off(void)
{
    BT_ON = 0;
    reg_on_control(0);
	/* BT_RST_N */
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<6));
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));	
	msleep(200);	
}

static void bt_device_suspend(void)
{
    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO4_O, (1 << 10));  
}

static void bt_device_resume(void)
{    
    SET_CBUS_REG_MASK(PREG_PAD_GPIO4_O, (1 << 10));
}

struct bt_dev_data bt_dev = {
    .bt_dev_init    = bt_device_init,
    .bt_dev_on      = bt_device_on,
    .bt_dev_off     = bt_device_off,
    .bt_dev_suspend = bt_device_suspend,
    .bt_dev_resume  = bt_device_resume,
};
#endif

static struct platform_device __initdata *platform_devs[] = {
#if defined(CONFIG_FB_AM)
    &fb_device,
#endif
#if defined(CONFIG_AM_STREAMING)
    &codec_device,
#endif
#if defined(CONFIG_AM_DEINTERLACE) || defined (CONFIG_DEINTERLACE)
    &deinterlace_device,
#endif
#if defined(CONFIG_TVIN_VDIN)
    &vdin_device,
#endif
#if defined(CONFIG_TVIN_BT656IN)
	&bt656in_device,
#endif
#if defined(CONFIG_AML_AUDIO_DSP)
    &audiodsp_device,
#endif
#if defined(CONFIG_SND_AML_M3)
    &aml_audio,
#endif
#if defined(CONFIG_CARDREADER)
    &amlogic_card_device,
#endif
#if defined(CONFIG_AML_RTC)
    &aml_rtc_device,
#endif
#ifdef CONFIG_SARADC_AM
&saradc_device,
#endif
#if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
    &adc_kp_device,
#endif
#if defined(CONFIG_KEY_INPUT_CUSTOM_AM) || defined(CONFIG_KEY_INPUT_CUSTOM_AM_MODULE)
    &input_device_key,  //changed by Elvis
#endif
#ifdef CONFIG_AM_NAND
    &aml_nand_device,
#endif

#ifdef CONFIG_AMLOGIC_VIDEOIN_MANAGER
	&vm_device,
#endif
#if defined(CONFIG_SUSPEND)
    &aml_pm_device,
#endif

#if defined(CONFIG_I2C_AML)|| defined(CONFIG_I2C_HW_AML)
    &aml_i2c_device,
    &aml_i2c_device1,
    &aml_i2c_device2,
#endif

#if defined(CONFIG_AMLOGIC_BACKLIGHT)
    &aml_bl_device,
#endif
#if defined(CONFIG_AM_TV_OUTPUT)||defined(CONFIG_AM_TCON_OUTPUT)
    &vout_device,   
#endif
#ifdef CONFIG_USB_ANDROID
    &android_usb_device,
    #ifdef CONFIG_USB_ANDROID_MASS_STORAGE
        &usb_mass_storage_device,
    #endif
#endif
#ifdef CONFIG_BT_DEVICE  
    &bt_device,
#endif
#ifdef CONFIG_GPS_POWERCTRL
       &aml_gps_device,
#endif
#ifdef CONFIG_EFUSE
	&aml_efuse_device,
#endif
#ifdef CONFIG_PMU_ACT8xxx
	&aml_pmu_device,
#endif
#ifdef CONFIG_POST_PROCESS_MANAGER
    &ppmgr_device,
#endif
#if defined(CONFIG_USB_PHY_CONTROL)
    &usb_phy_control_device,
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info[] = {

#ifdef CONFIG_FOCALTECH_CAPACITIVE_TOUCHSCREEN
    {
        I2C_BOARD_INFO("ft5x06", 0x38),
        .irq = INT_GPIO_0,
        .platform_data = (void *)&ts_pdata,
    },
#endif

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308

	{
        /*gc0308 i2c address is 0x42/0x43*/
		I2C_BOARD_INFO("gc0308_i2c",  0x42 >> 1),
		.platform_data = (void *)&video_gc0308_data,
	},
#endif

#if CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005
    {
    	/*gt2005 i2c address is 0x78/0x79*/
    	I2C_BOARD_INFO("gt2005_i2c",  0x78 >> 1 ),
    	.platform_data = (void *)&video_gt2005_data
    },
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info_1[] = {

#ifdef CONFIG_INPUT_LSM303DLM_ACCEL
  {
  	I2C_BOARD_INFO("lsm303dlm_acc", 0x18),      //acc IIC address 0x18
    .platform_data = (void *)&lsm303dlc_acc_plt_dat,
  },
#endif

#ifdef CONFIG_INPUT_LSM303DLM_MAGNE
  {
  	I2C_BOARD_INFO("lsm303dlm_mag", 0x1E), 	//mag IIC address 0x1E
    .platform_data = (void *)&lsm303dlc_mag_plt_dat,
  },
#endif
#ifdef CONFIG_INPUT_L3G4200D
  {
  	I2C_BOARD_INFO("l3g4200d_gyr", 0x68),
    .platform_data = (void *)&l3g4200d_gyro_plt_dat,
  },
#endif

};

static struct i2c_board_info __initdata aml_i2c_bus_info_2[] = {
#ifdef CONFIG_PMU_ACT8xxx
	{
        I2C_BOARD_INFO(ACT8xxx_I2C_NAME, ACT8xxx_ADDR),
#ifdef CONFIG_PMU_ACT8942
		.platform_data = (void *)&act8942_pdata,
#endif
    },
#endif
};

static int __init aml_i2c_init(void)
{
    i2c_register_board_info(0, aml_i2c_bus_info,
        ARRAY_SIZE(aml_i2c_bus_info));
    i2c_register_board_info(1, aml_i2c_bus_info_1,
        ARRAY_SIZE(aml_i2c_bus_info_1)); 
    i2c_register_board_info(2, aml_i2c_bus_info_2,
        ARRAY_SIZE(aml_i2c_bus_info_2)); 
    return 0;
}

#if defined(CONFIG_TVIN_BT656IN)
static void __init bt656in_pinmux_init(void)
{
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, 0xf<<6);
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, 1<<21);
}


#endif
static void __init eth_pinmux_init(void)
{

    CLEAR_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);
    SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, (1 << 1));
    SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);
    udelay(100);
    aml_i2c_init();
}

static void __init device_pinmux_init(void )
{
    clearall_pinmux();
    aml_i2c_init();
#if defined(CONFIG_TVIN_BT656IN)
    bt656in_pinmux_init();
#endif
    set_audio_pinmux(AUDIO_OUT_TEST_N);
   // set_audio_pinmux(AUDIO_IN_JTAG);
#if 1
    //set clk for wifi
    WRITE_CBUS_REG(HHI_GEN_CLK_CNTL,(READ_CBUS_REG(HHI_GEN_CLK_CNTL)&(~(0x7f<<0)))|((0<<0)|(1<<8)|(7<<9)) );
    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<15));    
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<22));

    // set clk for camera 
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<29)); 
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<2));

    unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
    SET_CBUS_REG_MASK(PWM_MISC_REG_CD, (1<<15)|(0<<8)|(1<<4)|(1<<0)); //select ddr pll for source, and clk divide 
#endif
#ifdef CONFIG_GPS_POWERCTRL
    gps_pinmux_init();
#endif
}


static void disable_unused_model(void)
{
    CLK_GATE_OFF(VIDEO_IN);
    CLK_GATE_OFF(BT656_IN);
    CLK_GATE_OFF(ETHERNET);
//    CLK_GATE_OFF(SATA);
//    CLK_GATE_OFF(WIFI);
    video_dac_disable();
 }
static void __init power_hold(void)
{
    printk(KERN_INFO "power hold set high!\n");
    set_gpio_val(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), 1);
    set_gpio_mode(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), GPIO_OUTPUT_MODE);
    
    //VCCx2 power up
    printk(KERN_INFO "set_vccx2 power up\n");
    set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
    set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 0);
}

static void __init LED_PWM_REG0_init(void)
{
        // Enable VBG_EN
    WRITE_CBUS_REG_BITS(PREG_AM_ANALOG_ADDR, 1, 0, 1);
    // wire pm_gpioA_7_led_pwm = pin_mux_reg0[22];
    WRITE_CBUS_REG(LED_PWM_REG0,(0 << 31)   |       // disable the overall circuit
                                (0 << 30)   |       // 1:Closed Loop  0:Open Loop
                                (0 << 16)   |       // PWM total count
                                (0 << 13)   |       // Enable
                                (1 << 12)   |       // enable
                                (0 << 10)   |       // test
                                (7 << 7)    |       // CS0 REF, Voltage FeedBack: about 0.505V
                                (7 << 4)    |       // CS1 REF, Current FeedBack: about 0.505V
                                READ_CBUS_REG(LED_PWM_REG0)&0x0f);           // DIMCTL Analog dimmer
                                
    WRITE_CBUS_REG_BITS(LED_PWM_REG0,1,0,4); //adust cpu1.2v   to 1.26V     

}

static __init void m3_init_machine(void)
{
    meson_cache_init();
#ifdef CONFIG_AML_SUSPEND
		extern int (*pm_power_suspend)(void);
		pm_power_suspend = meson_power_suspend;
#endif /*CONFIG_AML_SUSPEND*/

#if defined(CONFIG_AMLOGIC_BACKLIGHT)
	power_off_backlight();
#endif
    LED_PWM_REG0_init();
    power_hold();
    pm_power_off = power_off;		//Elvis fool
    device_pinmux_init();
    
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE
    camera_power_on_init();
#endif
    platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));

#ifdef CONFIG_USB_DWC_OTG_HCD
    set_usb_phy_clk(USB_PHY_CLOCK_SEL_XTAL_DIV2);
    set_usb_ctl_por(USB_CTL_INDEX_B,USB_CTL_POR_DISABLE); //disable usb_b    
    lm_device_register(&usb_ld_a);
    //lm_device_register(&usb_ld_b);
#endif
    disable_unused_model();
    WRITE_CBUS_REG_BITS(PAD_PULL_UP_REG0,1,19,1);
}

/*VIDEO MEMORY MAPING*/
static __initdata struct map_desc meson_video_mem_desc[] = {
    {
        .virtual    = PAGE_ALIGN(__phys_to_virt(RESERVED_MEM_START)),
        .pfn        = __phys_to_pfn(RESERVED_MEM_START),
        .length     = RESERVED_MEM_END-RESERVED_MEM_START+1,
        .type       = MT_DEVICE,
    },
#ifdef CONFIG_AML_SUSPEND
    {
        .virtual    = PAGE_ALIGN(__phys_to_virt(PHYS_OFFSET + CONFIG_AML_SUSPEND_FIRMWARE_BASE)),
        .pfn        = __phys_to_pfn(PHYS_OFFSET + CONFIG_AML_SUSPEND_FIRMWARE_BASE),
        .length     = SZ_1M,
        .type       = MT_MEMORY,
    },
#endif
};

static __init void m3_map_io(void)
{
    meson_map_io();
    iotable_init(meson_video_mem_desc, ARRAY_SIZE(meson_video_mem_desc));
}

static __init void m3_irq_init(void)
{
    meson_init_irq();
}

static __init void m3_fixup(struct machine_desc *mach, struct tag *tag, char **cmdline, struct meminfo *m)
{
    struct membank *pbank;
    m->nr_banks = 0;
    pbank=&m->bank[m->nr_banks];
    pbank->start = PAGE_ALIGN(PHYS_MEM_START);
    pbank->size  = SZ_64M & PAGE_MASK;
    pbank->node  = PHYS_TO_NID(PHYS_MEM_START);
    m->nr_banks++;
    // RESERVED_MEM_END ~ PHYS_MEM_END 
    pbank=&m->bank[m->nr_banks];
    pbank->start = PAGE_ALIGN(RESERVED_MEM_END+1);
#ifdef CONFIG_AML_SUSPEND
    pbank->size  = (PHYS_MEM_END-RESERVED_MEM_END-SZ_1M) & PAGE_MASK;
#else
    pbank->size  = (PHYS_MEM_END-RESERVED_MEM_END) & PAGE_MASK;
#endif
    pbank->node  = PHYS_TO_NID(RESERVED_MEM_END+1);
    m->nr_banks++;
}

MACHINE_START(MESON3_8726M_SKT, "AMLOGIC MESON3 8726M SKT SH")
    .phys_io        = MESON_PERIPHS1_PHYS_BASE,
    .io_pg_offst    = (MESON_PERIPHS1_PHYS_BASE >> 18) & 0xfffc,
    .boot_params    = BOOT_PARAMS_OFFSET,
    .map_io         = m3_map_io,
    .init_irq       = m3_irq_init,
    .timer          = &meson_sys_timer,
    .init_machine   = m3_init_machine,
    .fixup          = m3_fixup,
    .video_start    = RESERVED_MEM_START,
    .video_end      = RESERVED_MEM_END,
MACHINE_END
