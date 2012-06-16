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
#include <mach/am_eth_pinmux.h>
#include <mach/nand.h>
#include <linux/i2c.h>
#include <linux/i2c-aml.h>
#include <mach/power_gate.h>
#include <linux/aml_bl.h>

#ifdef CONFIG_AM_UART_WITH_S_CORE 
#include <linux/uart-aml.h>
#endif
#include <mach/card_io.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <mach/clk_set.h>
#include "board-8726m-refc08.h"

#if defined(CONFIG_TOUCHSCREEN_ADS7846)
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/spi/ads7846.h>
#endif

#ifdef CONFIG_ANDROID_PMEM
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/android_pmem.h>
#endif

#ifdef CONFIG_SENSORS_MXC622X
#include <linux/mxc622x.h>
#endif

#ifdef CONFIG_SENSORS_MMC31XX
#include <linux/mmc31xx.h>
#endif

#ifdef CONFIG_AMLOGIC_PM
#include <linux/power_supply.h>
#include <linux/aml_power.h>
#endif

#ifdef CONFIG_USB_ANDROID
#include <linux/usb/android_composite.h>
#endif

#ifdef CONFIG_SUSPEND
#include <mach/pm.h>
#endif

#ifdef CONFIG_SO340010_CAPACITIVE_KEYPAD
#include <linux/i2c/so340010.h>
#endif

#if defined(CONFIG_JPEGLOGO)
static struct resource jpeglogo_resources[] = {
    [0] = {
        .start = CONFIG_JPEGLOGO_ADDR,
        .end   = CONFIG_JPEGLOGO_ADDR + CONFIG_JPEGLOGO_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = CODEC_ADDR_START,
        .end   = CODEC_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device jpeglogo_device = {
	.name = "jpeglogo-dev",
	.id   = 0,
    .num_resources = ARRAY_SIZE(jpeglogo_resources),
    .resource      = jpeglogo_resources,
};
#endif

#if defined(CONFIG_KEYPADS_AM)||defined(CONFIG_KEYPADS_AM_MODULE)
static struct resource intput_resources[] = {
	{
		.start = 0x0,
		.end = 0x0,
		.name="8726",
		.flags = IORESOURCE_IO,
	},
};

static struct platform_device input_device = {
	.name = "m1-kp",
	.id = 0,
	.num_resources = ARRAY_SIZE(intput_resources),
	.resource = intput_resources,
	
};
#endif

#ifdef CONFIG_SINTEK_CAPACITIVE_TOUCHSCREEN
#include <linux/i2c/stkxxx.h>

//GPIOC_4
#define GPIO_STK_PENIRQ	((GPIOC_bank_bit0_26(4)<<16) |GPIOC_bit_bit0_26(4)) 
//GPIOC_0
#define GPIO_STK_RST		((GPIOC_bank_bit0_26(0)<<16) |GPIOC_bit_bit0_26(0)) 

static int stkxxx_init_irq(void)
{
/* memson
	Bit(s)	Description
	256-105	Unused
	104		JTAG_TDO
	103		JTAG_TDI
	102		JTAG_TMS
	101		JTAG_TCK
	100		gpioA_23
	99		gpioA_24
	98		gpioA_25
	97		gpioA_26
	98-75	gpioE[21:0]
	75-50	gpioD[24:0]
	49-23	gpioC[26:0]
	22-15	gpioB[22;15]
	14-0		gpioA[14:0]
 */

	/* set input mode */
	gpio_direction_input(GPIO_STK_PENIRQ);
	/* set gpio interrupt #0 source=GPIOC_4, and triggered by falling edge(=1) */
	gpio_enable_edge_int(27, 1, 0);

	gpio_direction_output(GPIO_STK_RST, 1);

	return 0;
}
static int stkxxx_get_irq_level(void)
{
	return gpio_get_value(GPIO_STK_PENIRQ);
}

static struct stkxxx_platform_data stkxxx_pdata = {
	.init_irq = &stkxxx_init_irq,
	.get_irq_level = &stkxxx_get_irq_level,
};
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

#ifdef CONFIG_ADC_TOUCHSCREEN_AM
#include <linux/adc_ts.h>

static struct adc_ts_platform_data adc_ts_pdata = {
	.irq = -1,	//INT_SAR_ADC
	.x_plate_ohms = 400,
};

static struct platform_device adc_ts_device = {
	.name = "adc_ts",
	.id = 0,
	.dev = {
		.platform_data = &adc_ts_pdata,
	},
};
#endif

#if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
#include <linux/input.h>
#include <linux/adc_keypad.h>

static struct adc_key adc_kp_key[] = {
#if 1	// w10
	{KEY_PAGEDOWN,		"vol-",	CHAN_4,	0, 60},	// 0v	102
	{KEY_PAGEUP,		"vol+",	CHAN_4,	306, 60},	// 0v	28
	{KEY_TAB,	             "exit",     CHAN_4,	602, 60},	// 0v	15
	{KEY_LEFTMETA,		"menu",	CHAN_4,	760, 60},	// 0v	125
#else // arm
	{KEY_MENU,		"menu",	CHAN_4,	0, 60},	// 0v
	{KEY_UP,			"up",		CHAN_4, 180, 60},	// 0.58v
	{KEY_DOWN,		"down",	CHAN_4, 285, 60},	// 0.92v
	{KEY_LEFT,		"left",	CHAN_4, 400, 60},	// 1.29v
	{KEY_RIGHT,		"right",	CHAN_4, 505, 60},	// 1.63v
	{KEY_EXIT,		"exit",	CHAN_4, 624, 60},	// 2.01v
	{KEY_OK,		"ok",		CHAN_4, 850, 60},	// 2.74v
#endif
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
    WRITE_CBUS_REG(0x21d0/*RTC_ADDR0*/, (READ_CBUS_REG(0x21d0/*RTC_ADDR0*/) &~(1<<11)));
    WRITE_CBUS_REG(0x21d1/*RTC_ADDR0*/, (READ_CBUS_REG(0x21d1/*RTC_ADDR0*/) &~(1<<3)));
}
static inline int key_scan(int *key_state_list)
{
    int ret = 0;
    key_state_list[0] = ((READ_CBUS_REG(0x21d1/*RTC_ADDR1*/) >> 2) & 1) ? 0 : 1;
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
#ifdef CONFIG_USB_DWC_OTG_HCD
static void set_usb_a_vbus_power(char is_power_on)
{
#define USB_A_POW_GPIO	PREG_EGPIO
#define USB_A_POW_GPIO_BIT	3
#define USB_A_POW_GPIO_BIT_ON	1
#define USB_A_POW_GPIO_BIT_OFF	0
	if(is_power_on){
		printk(KERN_INFO "set usb port power on (board gpio %d)!\n",USB_A_POW_GPIO_BIT);
		set_gpio_mode(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,GPIO_OUTPUT_MODE);
		set_gpio_val(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,USB_A_POW_GPIO_BIT_ON);
	}
	else	{
		printk(KERN_INFO "set usb port power off (board gpio %d)!\n",USB_A_POW_GPIO_BIT);		
		set_gpio_mode(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,GPIO_OUTPUT_MODE);
		set_gpio_val(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,USB_A_POW_GPIO_BIT_OFF);		
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
	.dma_config = USB_DMA_BURST_SINGLE,
	.set_vbus_power = 0,
};

#endif
#ifdef CONFIG_SATA_DWC_AHCI
static struct lm_device sata_ld = {
	.type = LM_DEVICE_TYPE_SATA,
	.id = 2,
	.irq = INT_SATA,
	.dma_mask_room = DMA_BIT_MASK(32),
	.resource.start = IO_SATA_BASE,
	.resource.end = -1,
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

#if defined(CONFIG_AM_VIDEO)
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
        .start =  VDIN_ADDR_START,		//pbufAddr
        .end   = VDIN_ADDR_END,				//pbufAddr + size
        .flags = IORESOURCE_MEM,
    },


};

static struct platform_device vdin_device = {
    .name       = "vdin",
    .id         = -1,
    .num_resources = ARRAY_SIZE(vdin_resources),
    .resource      = vdin_resources,
};

//add pin mux info for bt656 input
static struct resource bt656in_resources[] = {
    [0] = {
        .start =  VDIN_ADDR_START,		//pbufAddr
        .end   = VDIN_ADDR_END,				//pbufAddr + size
        .flags = IORESOURCE_MEM,
    },
    [1] = {		//bt656/camera/bt601 input resource pin mux setting
        .start =  0x3000,		//mask--mux gpioD 15 to bt656 clk;  mux gpioD 16:23 to be bt656 dt_in
        .end   = PERIPHS_PIN_MUX_5 + 0x3000,	 
        .flags = IORESOURCE_MEM,
    },

    [2] = {			//camera/bt601 input resource pin mux setting
        .start =  0x1c000,		//mask--mux gpioD 12 to bt601 FIQ; mux gpioD 13 to bt601HS; mux gpioD 14 to bt601 VS;
        .end   = PERIPHS_PIN_MUX_5 + 0x1c000,				
        .flags = IORESOURCE_MEM,
    },

    [3] = {			//bt601 input resource pin mux setting
        .start =  0x800,		//mask--mux gpioD 24 to bt601 IDQ;;
        .end   = PERIPHS_PIN_MUX_5 + 0x800,			
        .flags = IORESOURCE_MEM,
    },

};

static struct platform_device bt656in_device = {
    .name       = "amvdec_656in",
    .id         = -1,
    .num_resources = ARRAY_SIZE(bt656in_resources),
    .resource      = bt656in_resources,
};
#endif

#if defined(CONFIG_CARDREADER)
static struct resource amlogic_card_resource[]  = {
	[0] = {
		.start = 0x1200230,   //physical address
		.end   = 0x120024c,
		.flags = 0x200,
	}
};

void sdio_extern_init(void)
{
	CLEAR_CBUS_REG_MASK(CARD_PIN_MUX_8, ((1<<5)));
	CLEAR_CBUS_REG_MASK(CARD_PIN_MUX_12, ((1<<11)));
	CLEAR_CBUS_REG_MASK(CARD_PIN_MUX_0, ((1<<13)));
	CLEAR_CBUS_REG_MASK(CARD_PIN_MUX_1, ((1<<2)));
	CLEAR_CBUS_REG_MASK(PREG_EGPIO_EN_N, (1<<7));
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_EN_N, (1 <<8));
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_O, (1<<8));
	SET_CBUS_REG_MASK(PREG_EGPIO_O, (1<<7));
}

void inand_extern_init(void)
{
	CLEAR_CBUS_REG_MASK(CARD_PIN_MUX_1,( (1<<29) | (1<<27) | (1<<25) | (1<<23)|0x3f));
	CLEAR_CBUS_REG_MASK(CARD_PIN_MUX_6,0x7fff);
	/*attension, PINMUX 7 not 0x3d<<24 */
	SET_CBUS_REG_MASK(CARD_PIN_MUX_7,(0x3f<<24));
}
static struct mtd_partition inand_partition_info[] = 
{
	{
		.name = "bootloader",
		.offset = 0,
		.size=4*1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "environment",
		.offset = 4*1024*1024,
		.size=4*1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "splash",
		.offset = 8*1024*1024,
		.size=4*1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "recovery",
		.offset = 12*1024*1024,
		.size = 16 * 1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "boot",
		.offset = 28*1024*1024,
		.size = 16 * 1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "system",
		.offset = 44 * 1024*1024,
		.size = 240 * 1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "cache",
		.offset = 284*1024*1024,
		.size = 36*1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "userdata",
		.offset= 320*1024*1024,
		.size= 256 * 1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "media",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
};

static struct aml_card_info  amlogic_card_info[] = {
	[0] = {
		.name = "sd_card",
		.work_mode = CARD_HW_MODE,
		.io_pad_type = SDIO_GPIOA_9_14,
		.card_ins_en_reg = EGPIO_GPIOC_ENABLE,
		.card_ins_en_mask = PREG_IO_5_MASK,
		.card_ins_input_reg = EGPIO_GPIOC_INPUT,
		.card_ins_input_mask = PREG_IO_5_MASK,
		.card_power_en_reg = 0,
		.card_power_en_mask = 0,
		.card_power_output_reg = 0,
		.card_power_output_mask = 0,
		.card_power_en_lev = 0,
		.card_wp_en_reg = EGPIO_GPIOA_ENABLE,
		.card_wp_en_mask = PREG_IO_11_MASK,
		.card_wp_input_reg = EGPIO_GPIOA_INPUT,
		.card_wp_input_mask = PREG_IO_11_MASK,
		.card_extern_init = 0,
	},
	[1] = {
		/*
		.name = "sdio_card",
		.work_mode = CARD_HW_MODE,
		.io_pad_type = SDIO_GPIOB_2_7,
		.card_ins_en_reg = 0,
		.card_ins_en_mask = 0,
		.card_ins_input_reg = 0,
		.card_ins_input_mask = 0,
		.card_power_en_reg = EGPIO_GPIOA_ENABLE,
		.card_power_en_mask = PREG_IO_8_MASK,
		.card_power_output_reg = EGPIO_GPIOA_OUTPUT,
		.card_power_output_mask = PREG_IO_8_MASK,
		*/
		.name = "sdio_card",
		.work_mode = CARD_HW_MODE,
		.io_pad_type = SDIO_GPIOB_2_7,
		.card_ins_en_reg = 0,
		.card_ins_en_mask = 0,
		.card_ins_input_reg = 0,
		.card_ins_input_mask = 0,
		.card_power_en_reg = EGPIO_GPIOC_ENABLE,
		.card_power_en_mask = PREG_IO_2_MASK,
		.card_power_output_reg = EGPIO_GPIOC_OUTPUT,
		.card_power_output_mask = PREG_IO_2_MASK,
		.card_power_en_lev = 1,
		.card_wp_en_reg = 0,
		.card_wp_en_mask = 0,
		.card_wp_input_reg = 0,
		.card_wp_input_mask = 0,
		.card_extern_init = sdio_extern_init,
	},
	[2] = {
		.name = "inand_card",
		.work_mode = CARD_HW_MODE,
		.io_pad_type = SDIO_GPIOE_6_11,
		.card_ins_en_reg = 0,
		.card_ins_en_mask = 0,
		.card_ins_input_reg = 0,
		.card_ins_input_mask = 0,
		.card_power_en_reg = 0,
		.card_power_en_mask = 0,
		.card_power_output_reg = 0,
		.card_power_output_mask = 0,
		.card_power_en_lev = 0,
		.card_wp_en_reg = 0,
		.card_wp_en_mask = 0,
		.card_wp_input_reg = 0,
		.card_wp_input_mask = 0,
		.card_extern_init = inand_extern_init,
		.partitions = inand_partition_info,
		.nr_partitions = ARRAY_SIZE(inand_partition_info),
	},
};

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

static struct resource aml_m1_audio_resource[]={
		[0]	=	{
				.start 	=	0,
				.end		=	0,
				.flags	=	IORESOURCE_MEM,
		},
};
#ifdef CONFIG_SND_SOC_WM8990
static struct platform_device aml_audio={
		.name 				= "aml_m1_audio_wm8900",
		.id 					= -1,
		.resource 		=	aml_m1_audio_resource,
		.num_resources	=	ARRAY_SIZE(aml_m1_audio_resource),
};
#endif

#ifdef CONFIG_SND_SOC_RT5621
static struct platform_device aml_audio={
		.name 				= "aml_m1_audio_rt5621",
		.id 					= -1,
		.resource 		=	aml_m1_audio_resource,
		.num_resources	=	ARRAY_SIZE(aml_m1_audio_resource),
};
#endif
#if defined(CONFIG_TOUCHSCREEN_ADS7846)
#define SPI_0		0
#define SPI_1		1
#define SPI_2		2

// GPIOC_8(G20, XPT_CLK)
#define GPIO_SPI_SCK		((GPIOC_bank_bit0_26(8)<<16) |GPIOC_bit_bit0_26(8)) 
// GPIOC_7(G21, XPT_IN)
#define GPIO_SPI_MOSI		((GPIOC_bank_bit0_26(7)<<16) |GPIOC_bit_bit0_26(7)) 
// GPIOC_6(G22, XPT_OUT)
#define GPIO_SPI_MISO		((GPIOC_bank_bit0_26(6)<<16) |GPIOC_bit_bit0_26(6))
// GPIOC_0(J20, XPT_NCS)
#define GPIO_TSC2046_CS	((GPIOC_bank_bit0_26(0)<<16) |GPIOC_bit_bit0_26(0)) 
// GPIOC_4(H20, NPEN_IRQ)
#define GPIO_TSC2046_PENDOWN	((GPIOC_bank_bit0_26(4)<<16) |GPIOC_bit_bit0_26(4)) 

static const struct spi_gpio_platform_data spi_gpio_pdata = {
	.sck = GPIO_SPI_SCK,
	.mosi = GPIO_SPI_MOSI,
	.miso = GPIO_SPI_MISO,
	.num_chipselect = 1,
};

static struct platform_device spi_gpio = {
	.name       = "spi_gpio",
	.id         = SPI_2,
	.dev = {
		.platform_data = (void *)&spi_gpio_pdata,
	},
};

static const struct ads7846_platform_data ads7846_pdata = {
	.model = 7846,
	.vref_delay_usecs = 100,
	.vref_mv = 2500,
	.keep_vref_on = false,
	.swap_xy = 0,
	.settle_delay_usecs = 10,
	.penirq_recheck_delay_usecs = 0,
	.x_plate_ohms  =500,
	.y_plate_ohms = 500,

	.x_min = 0,
	.x_max = 0xfff,
	.y_min = 0,
	.y_max = 0xfff,
	.pressure_min = 0,
	.pressure_max = 0xfff,

	.debounce_max = 0,
	.debounce_tol = 0,
	.debounce_rep = 0,
	
	.gpio_pendown = GPIO_TSC2046_PENDOWN,
	.get_pendown_state =NULL,
	
	.filter_init = NULL,
	.filter = NULL,
	.filter_cleanup = NULL,
	.wait_for_sync = NULL,
	.wakeup = false,
};

static struct spi_board_info spi_board_info_list[] = {
	[0] = {
		.modalias = "ads7846",
		.platform_data = (void *)&ads7846_pdata,
		.controller_data = (void *)GPIO_TSC2046_CS,
		.irq = INT_GPIO_0,
		.max_speed_hz = 500000,
		.bus_num = SPI_2,
		.chip_select = 0,
		.mode = SPI_MODE_0,
	},
};

static int ads7846_init_gpio(void)
{
/* memson
	Bit(s)	Description
	256-105	Unused
	104		JTAG_TDO
	103		JTAG_TDI
	102		JTAG_TMS
	101		JTAG_TCK
	100		gpioA_23
	99		gpioA_24
	98		gpioA_25
	97		gpioA_26
	98-75	gpioE[21:0]
	75-50	gpioD[24:0]
	49-23	gpioC[26:0]
	22-15	gpioB[22;15]
	14-0		gpioA[14:0]
 */

	/* set input mode */
	gpio_direction_input(GPIO_TSC2046_PENDOWN);
	/* set gpio interrupt #0 source=GPIOC_4, and triggered by falling edge(=1) */
	gpio_enable_edge_int(27, 1, 0);

//	// reg2 bit24~26
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_2,
//	(1<<24) | (1<<25) | (1<<26));
//	// reg3 bit5~7,12,16~18,22
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3,
//	(1<<5) | (1<<6) | (1<<7) | (1<<9) | (1<<12) | (1<<16) | (1<<17) | (1<<18) | (1<<22));
//	// reg4 bit26~27
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_4,
//	(1<<26) | (1<<27));
//	// reg9 bit0,4,6~8
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_9,
//	(1<<0) | (1<<4) | (1<<6) | (1<<7) | (1<<8));
//	// reg10 bit0,4,6~8
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_10,
//	(1<<0) | (1<<4) | (1<<6) | (1<<7) | (1<<8));

	return 0;
}
#endif

#ifdef CONFIG_SO340010_CAPACITIVE_KEYPAD
#include <linux/input.h>
//GPIOC_1
#define GPIO_SO340010_ATTN	((GPIOC_bank_bit0_26(1)<<16) |GPIOC_bit_bit0_26(1)) 
static int so340010_init_irq(void)
{
	/* set input mode */
	gpio_direction_input(GPIO_SO340010_ATTN);
	/* set gpio interrupt #1 source=GPIOC_1, and triggered by falling edge(=1) */
	gpio_enable_edge_int(23+1, 1, 1);
	return 0;
}

static int so340010_get_irq_level(void)
{
	return gpio_get_value(GPIO_SO340010_ATTN);
}

static struct cap_key so340010_keys[] = {
	{ KEY_SEARCH,	0x0001,	"search"},
	{ KEY_HOME,		0x0002,	"home"},
	{ KEY_MENU,		0x0004,	"menu"},
	{ KEY_BACK,		0x0008,	"back"},
};

static struct so340010_platform_data so340010_pdata = {
	.init_irq = so340010_init_irq,
	.get_irq_level = so340010_get_irq_level,
	.key = so340010_keys,
	.key_num = ARRAY_SIZE(so340010_keys),
};
#endif

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_data =
{
	.name = "pmem",
	.start = PMEM_START,
	.size = PMEM_SIZE,
	.no_allocator = 1,
	.cached = 0,
};

static struct platform_device android_pmem_device =
{
	.name = "android_pmem",
	.id = 0,
	.dev = {
		.platform_data = &pmem_data,
	},
};
#endif

#if defined(CONFIG_AML_RTC)
static	struct platform_device aml_rtc_device = {
      		.name            = "aml_rtc",
      		.id               = -1,
	};
#endif

#if defined(CONFIG_SUSPEND)

static void set_vccx2(int power_on)
{
	int i;
	    printk(KERN_INFO "vccx2 function!\n");
        SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<18));
        CLEAR_CBUS_REG_MASK(PREG_EGPIO_EN_N, (1<<4));	              
    if (power_on){
        //set clk for wifi
        	    printk(KERN_INFO "set GPIOA0 to clk output!\n");
        SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<18));
        CLEAR_CBUS_REG_MASK(PREG_EGPIO_EN_N, (1<<4));	              
    }
    else{
        //disable wifi clk
                	    printk(KERN_INFO "clear GPIOA0 to clk output!\n");
        CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<18));
        SET_CBUS_REG_MASK(PREG_EGPIO_EN_N, (1<<4));	        

    }
}
static struct meson_pm_config aml_pm_pdata = {
    .pctl_reg_base = IO_APB_BUS_BASE,
    .mmc_reg_base = APB_REG_ADDR(0x1000),
    .hiu_reg_base = CBUS_REG_ADDR(0x1000),
    .power_key = CBUS_REG_ADDR(RTC_ADDR1),
    .ddr_clk = 0x00110820,
    .sleepcount = 128,
    .set_vccx2 = set_vccx2,
    .core_voltage_adjust = 5,
};

static struct platform_device aml_pm_device = {
	.name           = "pm-meson",
	.dev = {
		.platform_data	= &aml_pm_pdata,
	},
	.id             = -1,
};
#endif

#if defined(CONFIG_I2C_SW_AML)

static struct aml_sw_i2c_platform aml_sw_i2c_plat = {
	.sw_pins = {
		.scl_reg_out		= MESON_I2C_PREG_GPIOB_OUTLVL,
		.scl_reg_in		= MESON_I2C_PREG_GPIOB_INLVL,
		.scl_bit			= 2,	/*MESON_I2C_MASTER_A_GPIOB_2_REG*/
		.scl_oe			= MESON_I2C_PREG_GPIOB_OE,
		.sda_reg_out		= MESON_I2C_PREG_GPIOB_OUTLVL,
		.sda_reg_in		= MESON_I2C_PREG_GPIOB_INLVL,
		.sda_bit			= 3,	/*MESON_I2C_MASTER_A_GPIOB_3_BIT*/
		.sda_oe			= MESON_I2C_PREG_GPIOB_OE,
	},	
	.udelay			= 2,
	.timeout			= 100,
};

static struct platform_device aml_sw_i2c_device = {
	.name		  = "aml-sw-i2c",
	.id		  = -1,
	.dev = {
		.platform_data = &aml_sw_i2c_plat,
	},
};

#endif

#if defined(CONFIG_I2C_AML)
static struct aml_i2c_platform aml_i2c_plat = {
	.wait_count		= 1000000,
	.wait_ack_interval	= 5,
	.wait_read_interval	= 5,
	.wait_xfer_interval	= 5,
	.master_no		= AML_I2C_MASTER_B,
	.use_pio			= 0,
	.master_i2c_speed	= AML_I2C_SPPED_100K,

	.master_b_pinmux = {
		.scl_reg	= MESON_I2C_MASTER_B_GPIOB_0_REG,
		.scl_bit	= MESON_I2C_MASTER_B_GPIOB_0_BIT,
		.sda_reg	= MESON_I2C_MASTER_B_GPIOB_1_REG,
		.sda_bit	= MESON_I2C_MASTER_B_GPIOB_1_BIT,
	}
};

static struct resource aml_i2c_resource[] = {
	[0] = {/*master a*/
		.start = 	MESON_I2C_MASTER_A_START,
		.end   = 	MESON_I2C_MASTER_A_END,
		.flags = 	IORESOURCE_MEM,
	},
	[1] = {/*master b*/
		.start = 	MESON_I2C_MASTER_B_START,
		.end   = 	MESON_I2C_MASTER_B_END,
		.flags = 	IORESOURCE_MEM,
	},
	[2] = {/*slave*/
		.start = 	MESON_I2C_SLAVE_START,
		.end   = 	MESON_I2C_SLAVE_END,
		.flags = 	IORESOURCE_MEM,
	},
};

static struct platform_device aml_i2c_device = {
	.name		  = "aml-i2c",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(aml_i2c_resource),
	.resource	  = aml_i2c_resource,
	.dev = {
		.platform_data = &aml_i2c_plat,
	},
};
#endif

#ifdef CONFIG_AMLOGIC_PM

static int is_ac_connected(void)
{
	return (READ_CBUS_REG(ASSIST_HW_REV)&(1<<9))? 1:0;
}

//static int is_usb_connected(void)
//{
//	return 0;
//}

static void set_charge(int flags)
{
    return;
}

#ifdef CONFIG_SARADC_AM
extern int get_adc_sample(int chan);
#endif
static int get_bat_vol(void)
{
#ifdef CONFIG_SARADC_AM
	return get_adc_sample(5);
#else
    return 0;
#endif
}

static int get_charge_status()
{
    int stat1 = 0;
    int stat2 = 0;
    stat1 = (READ_CBUS_REG(ASSIST_HW_REV)&(1<<8))?1:0;
    stat2 = (READ_CBUS_REG(ASSIST_HW_REV)&(1<<10))?1:0;
    switch(((stat2<<2)|stat1))
    {
        case 0:
        case 1:
            return 0;
        case 2:
            return 1;
        default:
            return 0;
    }
}

static struct aml_power_pdata power_pdata = {
	.is_ac_online	= is_ac_connected,
	//.is_usb_online	= is_usb_connected,
	//.set_charge = set_charge,
	.get_bat_vol = get_bat_vol,
	.get_charge_status = get_charge_status,
	//.supplied_to = supplicants,
	//.num_supplicants = ARRAY_SIZE(supplicants),
};

static struct platform_device power_dev = {
	.name		= "aml-power",
	.id		= -1,
	.dev = {
		.platform_data	= &power_pdata,
	},
};
#endif

#define PINMUX_UART_A   UART_A_GPIO_C9_C10
#define PINMUX_UART_B	UART_B_GPIO_E18_E19

#if defined(CONFIG_AM_UART_WITH_S_CORE)

#if defined(CONFIG_AM_UART0_SET_PORT_A)
#define UART_0_PORT		UART_A
#define UART_1_PORT		UART_B
#elif defined(CONFIG_AM_UART0_SET_PORT_B)
#define UART_0_PORT		UART_B
#define UART_1_PORT		UART_A
#endif

static struct aml_uart_platform aml_uart_plat = {
    .uart_line[0]		=	UART_0_PORT,
    .uart_line[1]		=	UART_1_PORT
};

static struct platform_device aml_uart_device = {	
    .name         = "am_uart",  
    .id       = -1, 
    .num_resources    = 0,  
    .resource     = NULL,   
    .dev = {        
                .platform_data = &aml_uart_plat,  
           },
};
#endif

#ifdef CONFIG_AM_NAND
/*static struct mtd_partition partition_info[] = 
{
	{
		.name = "U-BOOT",
		.offset = 0,
		.size=4*1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "Boot Para",
		.offset = 4*1024*1024,
		.size=4*1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "Kernel",
		.offset = 8*1024*1024,
		.size=4*1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "YAFFS2",
		.offset = MTDPART_OFS_APPEND,
        .size = MTDPART_SIZ_FULL,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
//	{	.name="FTL_Part",
//		.offset=MTDPART_OFS_APPEND,
//		.size=MTDPART_SIZ_FULL,
//	//	.set_flags=MTD_AVNFTL,
//	//	.dual_partnum=1,
//	}
};

static struct aml_m1_nand_platform aml_2kpage128kblocknand_platform = {
	.page_size = 2048,
	.spare_size=64,
	.erase_size= 128*1024,
	.bch_mode=1,			//BCH8
	.encode_size=528,
	.timing_mode=5,
	.ce_num=1,
	.onfi_mode=0,
	.partitions = partition_info,
	.nr_partitions = ARRAY_SIZE(partition_info),
};
*/

/*static struct aml_m1_nand_platform aml_Micron4GBABAnand_platform = 
{
	.page_size = 2048*2,
	.spare_size= 224,		//for micron ABA 4GB
	.erase_size=1024*1024,
	.bch_mode=	  3,		//BCH16
	.encode_size=540,				
	.timing_mode=5,
	.onfi_mode=1,
	.ce_num=1,
	.partitions = partition_info,
	.nr_partitions = ARRAY_SIZE(partition_info),
};

static struct resource aml_nand_resources[] = {
	{
		.start = 0xc1108600,
		.end = 0xc1108624,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device aml_nand_device = {
	.name = "aml_m1_nand",
	.id = 0,
	.num_resources = ARRAY_SIZE(aml_nand_resources),
	.resource = aml_nand_resources,
	.dev = {
		.platform_data = &aml_Micron4GBABAnand_platform,
	},
};*/

/*static struct mtd_partition normal_partition_info[] = 
{
	{
		.name = "environment",
		.offset = 8*1024*1024,
		.size = 8*1024*1024,
	},
	{
		.name = "splash",
		.offset = 16*1024*1024,
		.size = 4*1024*1024,
	},
	{
		.name = "recovery",
		.offset = 20*1024*1024,
		.size = 16*1024*1024,
	},
	{
		.name = "boot",
		.offset = 36*1024*1024,
		.size = 16*1024*1024,
	},
	{
		.name = "cache",
		.offset = 52*1024*1024,
		.size = 32*1024*1024,
	},
};*/

static struct mtd_partition multi_partition_info[] = 
{
	{
		.name = "environment",
		.offset = 8*1024*1024,
		.size = 40*1024*1024,
	},
	{
		.name = "logo",
		.offset = 48*1024*1024,
		.size = 16*1024*1024,
	},
	{
		.name = "recovery",
		.offset = 64*1024*1024,
		.size = 16*1024*1024,
	},
	{
		.name = "uImage",
		.offset = 80*1024*1024,
		.size = 16*1024*1024,
	},
	{
		.name = "system",
		.offset = 96*1024*1024,
		.size = 256*1024*1024,
	},
	{
		.name = "cache",
		.offset = 352*1024*1024,
		.size = 32*1024*1024,
	},
	{
		.name = "userdata",
		.offset = 384*1024*1024,
		.size = 256*1024*1024,
	},
	{
		.name = "NFTL_Part",
		.offset = ((384 + 256)*1024*1024),
		.size = ((0x200000000 - (384 + 256)*1024*1024)),
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
				.options = (NAND_TIMING_MODE5 | NAND_ECC_BCH16_MODE),
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
				.options = (NAND_TIMING_MODE5 | NAND_ECC_BCH16_MODE | NAND_TWO_PLANE_MODE),
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
	.name = "aml_m1_nand",
	.id = 0,
	.num_resources = ARRAY_SIZE(aml_nand_resources),
	.resource = aml_nand_resources,
	.dev = {
		.platform_data = &aml_nand_mid_device,
	},
};
#endif

#if defined(CONFIG_AMLOGIC_BACKLIGHT)

#define PWM_TCNT        (600-1)
#define PWM_MAX_VAL    (420)

static void aml_8726m_bl_init(void)
{
    unsigned val;
    
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_0, 0, 22, 1);
    WRITE_CBUS_REG_BITS(PREG_AM_ANALOG_ADDR, 1, 0, 1);
    WRITE_CBUS_REG(VGHL_PWM_REG0, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG1, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG2, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG3, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG4, 0);
    val = (0 << 31)           |       // disable the overall circuit
          (0 << 30)           |       // 1:Closed Loop  0:Open Loop
          (PWM_TCNT << 16)    |       // PWM total count
          (0 << 13)           |       // Enable
          (1 << 12)           |       // enable
          (0 << 10)           |       // test
          (3 << 7)            |       // CS0 REF, Voltage FeedBack: about 0.27V
          (7 << 4)            |       // CS1 REF, Current FeedBack: about 0.54V
          (0 << 0);                   // DIMCTL Analog dimmer
    WRITE_CBUS_REG(VGHL_PWM_REG0, val);
    val = (1 << 30)           |       // enable high frequency clock
          (PWM_MAX_VAL << 16) |       // MAX PWM value
          (0 << 0);                  // MIN PWM value
    WRITE_CBUS_REG(VGHL_PWM_REG1, val);
    val = (0 << 31)       |       // disable timeout test mode
          (0 << 30)       |       // timeout based on the comparator output
          (0 << 16)       |       // timeout = 10uS
          (0 << 13)       |       // Select oscillator as the clock (just for grins)
          (1 << 11)       |       // 1:Enable OverCurrent Portection  0:Disable
          (3 << 8)        |       // Filter: shift every 3 ticks
          (0 << 6)        |       // Filter: count 1uS ticks
          (0 << 5)        |       // PWM polarity : negative
          (0 << 4)        |       // comparator: negative, Different with NikeD3
          (1 << 0);               // +/- 1
    WRITE_CBUS_REG(VGHL_PWM_REG2, val);
    val = (   1 << 16) |    // Feedback down-sampling = PWM_freq/1 = PWM_freq
          (   1 << 14) |    // enable to re-write MATCH_VAL
          ( 210 <<  0) ;  // preset PWM_duty = 50%
    WRITE_CBUS_REG(VGHL_PWM_REG3, val);
    val = (   0 << 30) |    // 1:Digital Dimmer  0:Analog Dimmer
          (   2 << 28) |    // dimmer_timebase = 1uS
          (1000 << 14) |    // Digital dimmer_duty = 0%, the most darkness
          (1000 <<  0) ;    // dimmer_freq = 1KHz
    WRITE_CBUS_REG(VGHL_PWM_REG4, val);
}
static unsigned bl_level;
static unsigned aml_8726m_get_bl_level(void)
{
//    unsigned level = 0;
//
//    WRITE_CBUS_REG_BITS(VGHL_PWM_REG0, 1, 31, 1);
//    WRITE_CBUS_REG_BITS(VGHL_PWM_REG4, 0, 30, 1);
//    level = READ_CBUS_REG_BITS(VGHL_PWM_REG0, 0, 4);
    return bl_level;
}
#define BL_MAX_LEVEL 60000
static void aml_8726m_set_bl_level(unsigned level)
{
    unsigned cs_level,pwm_level,low,hi;
    
    bl_level = level;
    
    level = level*179/255;
    if(level>=120){ //120 - 179
        cs_level = 9 -(level - 120)/15;
        pwm_level = 85 + (level - 120)%15;   
    }
    else if(level>=20){ //20 - 119
        cs_level = 13 - (level -20)/25;
        pwm_level = 75 + (level - 20)%25;   
    }
    else{  //  <20
        cs_level = 13;
        pwm_level = 0;           
    }
        
    hi = (BL_MAX_LEVEL/100)*pwm_level;
    low = BL_MAX_LEVEL - hi;
    WRITE_CBUS_REG_BITS(VGHL_PWM_REG0, cs_level, 0, 4);   
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<31)); 
    SET_CBUS_REG_MASK(PWM_MISC_REG_AB, (1 << 0));         
    WRITE_CBUS_REG_BITS(PWM_PWM_A,low,0,16);  //low
    WRITE_CBUS_REG_BITS(PWM_PWM_A,hi,16,16);  //hi   
}

static void aml_8726m_power_on_bl(void)
{
    WRITE_CBUS_REG_BITS(PREG_EGPIO_EN_N, 0, 12, 1);
    WRITE_CBUS_REG_BITS(PREG_EGPIO_O, 1, 12, 1);

    WRITE_CBUS_REG_BITS(PREG_EGPIO_EN_N, 0, 7, 1);
    WRITE_CBUS_REG_BITS(PREG_EGPIO_O, 1, 7, 1);
    
    aml_8726m_set_bl_level(0);
}

static void aml_8726m_power_off_bl(void)
{
    WRITE_CBUS_REG_BITS(PREG_EGPIO_EN_N, 0, 12, 1);
    WRITE_CBUS_REG_BITS(PREG_EGPIO_O, 0, 12, 1);

    WRITE_CBUS_REG_BITS(PREG_EGPIO_EN_N, 0, 7, 1);
    WRITE_CBUS_REG_BITS(PREG_EGPIO_O, 0, 7, 1);
}

struct aml_bl_platform_data aml_bl_platform =
{
    .bl_init = aml_8726m_bl_init,
    .power_on_bl = aml_8726m_power_on_bl,
    .power_off_bl = aml_8726m_power_off_bl,
    .get_bl_level = aml_8726m_get_bl_level,
    .set_bl_level = aml_8726m_set_bl_level,
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
       .vendor = "AMLOGIC",
       .product = "Android MID",
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
       .product_name   = "Android MID",
       .manufacturer_name = "AMLOGIC",
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

#if (defined(CONFIG_SMBA1002_BATTERY) || defined(CONFIG_SMBA1011_BATTERY))
static struct platform_device smba1002_battery_device = {
	.name = "smba10xx_battery",
	.id = -1,
};
#endif

static struct platform_device __initdata *platform_devs[] = {
    #if defined(CONFIG_JPEGLOGO)
		&jpeglogo_device,
	#endif
	#if defined (CONFIG_AMLOGIC_PM)
		&power_dev,
	#endif	
    #if defined(CONFIG_FB_AM)
    	&fb_device,
    #endif
    #if defined(CONFIG_AM_STREAMING)
		&codec_device,
    #endif
    #if defined(CONFIG_AM_VIDEO)
		&deinterlace_device,
    #endif
    #if defined(CONFIG_TVIN_VDIN)
        &vdin_device,
		&bt656in_device,
    #endif
	#if defined(CONFIG_AML_AUDIO_DSP)
		&audiodsp_device,
	#endif
	#if defined(CONFIG_SND_SOC_WM8990) || defined(CONFIG_SND_SOC_RT5621)
		&aml_audio,
	#endif
	#if defined(CONFIG_CARDREADER)
    	&amlogic_card_device,
    #endif
    #if defined(CONFIG_KEYPADS_AM)||defined(CONFIG_VIRTUAL_REMOTE)||defined(CONFIG_KEYPADS_AM_MODULE)
		&input_device,
    #endif	
#ifdef CONFIG_SARADC_AM
		&saradc_device,
#endif
#ifdef CONFIG_ADC_TOUCHSCREEN_AM
		&adc_ts_device,
#endif
    #if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
		&adc_kp_device,
    #endif
    #if defined(CONFIG_KEY_INPUT_CUSTOM_AM) || defined(CONFIG_KEY_INPUT_CUSTOM_AM_MODULE)
		&input_device_key,  //changed by Elvis
    #endif
	#if defined(CONFIG_TOUCHSCREEN_ADS7846)
		&spi_gpio,
	#endif
    #ifdef CONFIG_AM_NAND
		&aml_nand_device,
    #endif		
    #if defined(CONFIG_NAND_FLASH_DRIVER_MULTIPLANE_CE)
		&aml_nand_device,
    #endif		
    #if defined(CONFIG_AML_RTC)
		&aml_rtc_device,
    #endif
	#if defined(CONFIG_SUSPEND)
		&aml_pm_device,
    #endif
    #if defined(CONFIG_ANDROID_PMEM)
		&android_pmem_device,
    #endif
    #if defined(CONFIG_I2C_SW_AML)
		&aml_sw_i2c_device,
    #endif
    #if defined(CONFIG_I2C_AML)
		&aml_i2c_device,
    #endif
	#if defined(CONFIG_AM_UART_WITH_S_CORE)
        &aml_uart_device,
    #endif
    #if defined(CONFIG_AMLOGIC_BACKLIGHT)
        &aml_bl_device,
    #endif
    #if  defined(CONFIG_AM_TV_OUTPUT)||defined(CONFIG_AM_TCON_OUTPUT)
       &vout_device,	
    #endif
	#ifdef CONFIG_USB_ANDROID
		&android_usb_device,
		#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
			&usb_mass_storage_device,
		#endif
	#endif
	#if (defined(CONFIG_SMBA1011_BATTERY)||defined(CONFIG_SMBA1002_BATTERY))
		&smba1002_battery_device,
	#endif
};
static struct i2c_board_info __initdata aml_i2c_bus_info[] = {

#ifdef CONFIG_SENSORS_MMC31XX
	{
		I2C_BOARD_INFO(MMC31XX_I2C_NAME,  MMC31XX_I2C_ADDR),
	},
#endif

#ifdef CONFIG_SENSORS_MXC622X
	{
		I2C_BOARD_INFO(MXC622X_I2C_NAME,  MXC622X_I2C_ADDR),
	},
#endif

#ifdef CONFIG_SND_SOC_WM8990
	{
		I2C_BOARD_INFO("wm8900", 0x1A),
	},
#endif

#ifdef CONFIG_MXC_MMA7660
	{
		I2C_BOARD_INFO("mma7660", 0x4C),
	},
#endif

#if (defined(CONFIG_SMBA1011_BATTERY)||defined(CONFIG_SMBA1002_BATTERY))
	{
		I2C_BOARD_INFO("bat_i2c", 0x0b),
	},
#endif

#ifdef CONFIG_SND_SOC_RT5621
	{
		I2C_BOARD_INFO("rt5621", 0x1A),
	},
#endif

#ifdef CONFIG_SINTEK_CAPACITIVE_TOUCHSCREEN
	{
		I2C_BOARD_INFO("stkxxx", 0x5C),
		.irq = INT_GPIO_0,
		.platform_data = (void *)&stkxxx_pdata,
	},
#endif

#ifdef CONFIG_SO340010_CAPACITIVE_KEYPAD
	{
		I2C_BOARD_INFO("so340010", 0x2c),
		.irq = INT_GPIO_1,
		.platform_data = (void *)&so340010_pdata,
	},
#endif
};


static int __init aml_i2c_init(void)
{

	i2c_register_board_info(0, aml_i2c_bus_info,
			ARRAY_SIZE(aml_i2c_bus_info));
	return 0;
}

static void __init eth_pinmux_init(void)
{
	//eth_set_pinmux(ETH_BANK2_GPIOD15_D23,ETH_CLK_OUT_GPIOD24_REG5_1,0);		
	eth_set_pinmux(ETH_BANK1_GPIOD2_D11,ETH_CLK_OUT_GPIOD7_REG4_20,0);	
		//power hold	
	//setbits_le32(P_PREG_AGPIO_O,(1<<8));
	//clrbits_le32(P_PREG_AGPIO_EN_N,(1<<8));
	//set_gpio_mode(GPIOA_bank_bit(4),GPIOA_bit_bit0_14(4),GPIO_OUTPUT_MODE);
	//set_gpio_val(GPIOA_bank_bit(4),GPIOA_bit_bit0_14(4),1);
		
	CLEAR_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);
	SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, (1 << 1));
	SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);
	udelay(100);
	/*reset*/
#if 1
	set_gpio_mode(GPIOC_bank_bit0_26(26),GPIOC_bit_bit0_26(26),GPIO_OUTPUT_MODE);
	set_gpio_val(GPIOC_bank_bit0_26(16),GPIOC_bit_bit0_26(26),0);
	udelay(100);	//GPIOE_bank_bit16_21(16) reset end;
	set_gpio_val(GPIOC_bank_bit0_26(26),GPIOC_bit_bit0_26(26),1);
#endif
	aml_i2c_init();
	printk("READ_CBUS_REG(4)=0x%08x !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", READ_CBUS_REG(4));
}

static void __init device_pinmux_init(void )
{
	clearall_pinmux();
	/*other deivce power on*/
	/*GPIOA_200e_bit4..usb/eth/YUV power on*/
	set_gpio_mode(PREG_EGPIO,1<<4,GPIO_OUTPUT_MODE);
	set_gpio_val(PREG_EGPIO,1<<4,1);
	uart_set_pinmux(UART_PORT_A,PINMUX_UART_A);
	uart_set_pinmux(UART_PORT_B,PINMUX_UART_B);
	/*pinmux of eth*/
	eth_pinmux_init();
	aml_i2c_init();
	//set_audio_pinmux(AUDIO_OUT_JTAG);
	set_audio_pinmux(AUDIO_OUT_TEST_N);
	
        printk(KERN_INFO "device_pinmux_init function!\n");
        SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<18));
        CLEAR_CBUS_REG_MASK(PREG_EGPIO_EN_N, (1<<4));	 
}

static void __init  device_clk_setting(void)
{
	/*Demod CLK for eth and sata*/
	demod_apll_setting(0,1200*CLK_1M);
	/*eth clk*/
    	eth_clk_set(ETH_CLKSRC_APLL_CLK,400*CLK_1M,50*CLK_1M);
}

static void disable_unused_model(void)
{
	CLK_GATE_OFF(VIDEO_IN);
	CLK_GATE_OFF(BT656_IN);
	CLK_GATE_OFF(ETHERNET);
	CLK_GATE_OFF(SATA);
	CLK_GATE_OFF(WIFI);
	video_dac_disable();
	//audio_internal_dac_disable();
	 //disable wifi
	SET_CBUS_REG_MASK(HHI_GCLK_MPEG2, (1<<5)); 
	SET_CBUS_REG_MASK(HHI_WIFI_CLK_CNTL, (1<<0)); 	
	__raw_writel(0xCFF,0xC9320ED8);
	__raw_writel((__raw_readl(0xC9320EF0))&0xF9FFFFFF,0xC9320EF0);
	CLEAR_CBUS_REG_MASK(HHI_GCLK_MPEG2, (1<<5)); 
	CLEAR_CBUS_REG_MASK(HHI_WIFI_CLK_CNTL, (1<<0)); 		
	///disable demod
	SET_CBUS_REG_MASK(HHI_DEMOD_CLK_CNTL, (1<<8));//enable demod core digital clock
	SET_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL, (1<<15));//enable demod adc clock
	CLEAR_APB_REG_MASK(0x4004,(1<<31));  //disable analog demod adc
	CLEAR_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL, (1<<15));//disable demod adc clock	
	CLEAR_CBUS_REG_MASK(HHI_DEMOD_CLK_CNTL, (1<<8));//disable demod core digital clock	
}
static void __init power_hold(void)
{
    printk(KERN_INFO "power hold set high!\n");
	set_gpio_val(GPIOA_bank_bit(8), GPIOA_bit_bit0_14(8), 1);
    set_gpio_mode(GPIOA_bank_bit(8), GPIOA_bit_bit0_14(8), GPIO_OUTPUT_MODE);
    
        /* PIN28, GPIOA_6, Pull high, For En_5V */
    set_gpio_val(GPIOA_bank_bit(6), GPIOA_bit_bit0_14(6), 1);
    set_gpio_mode(GPIOA_bank_bit(6), GPIOA_bit_bit0_14(6), GPIO_OUTPUT_MODE);
}

static __init void m1_init_machine(void)
{
	meson_cache_init();

	power_hold();
	device_clk_setting();
	device_pinmux_init();
	platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));

#ifdef CONFIG_USB_DWC_OTG_HCD
	set_usb_phy_clk(USB_PHY_CLOCK_SEL_XTAL_DIV2);
	lm_device_register(&usb_ld_a);
	lm_device_register(&usb_ld_b);
#endif
#ifdef CONFIG_SATA_DWC_AHCI
	set_sata_phy_clk(SATA_PHY_CLOCK_SEL_DEMOD_PLL);
	lm_device_register(&sata_ld);
#endif
#if defined(CONFIG_TOUCHSCREEN_ADS7846)
	ads7846_init_gpio();
	spi_register_board_info(spi_board_info_list, ARRAY_SIZE(spi_board_info_list));
#endif
	disable_unused_model();
}

/*VIDEO MEMORY MAPING*/
static __initdata struct map_desc meson_video_mem_desc[] = {
	{
		.virtual	= PAGE_ALIGN(__phys_to_virt(RESERVED_MEM_START)),
		.pfn		= __phys_to_pfn(RESERVED_MEM_START),
		.length		= RESERVED_MEM_END-RESERVED_MEM_START+1,
		.type		= MT_DEVICE,
	},
};

static __init void m1_map_io(void)
{
	meson_map_io();
	iotable_init(meson_video_mem_desc, ARRAY_SIZE(meson_video_mem_desc));
}

static __init void m1_irq_init(void)
{
	meson_init_irq();
}

static __init void m1_fixup(struct machine_desc *mach, struct tag *tag, char **cmdline, struct meminfo *m)
{
	struct membank *pbank;
	m->nr_banks = 0;
	pbank=&m->bank[m->nr_banks];
	pbank->start = PAGE_ALIGN(PHYS_MEM_START);
	pbank->size  = SZ_64M & PAGE_MASK;
	pbank->node  = PHYS_TO_NID(PHYS_MEM_START);
	m->nr_banks++;
	pbank=&m->bank[m->nr_banks];
	pbank->start = PAGE_ALIGN(RESERVED_MEM_END+1);
	pbank->size  = (PHYS_MEM_END-RESERVED_MEM_END) & PAGE_MASK;
	pbank->node  = PHYS_TO_NID(RESERVED_MEM_END+1);
	m->nr_banks++;
}

MACHINE_START(MESON_8726M, "AMLOGIC MESON-M1 8726M SZ")
	.phys_io		= MESON_PERIPHS1_PHYS_BASE,
	.io_pg_offst	= (MESON_PERIPHS1_PHYS_BASE >> 18) & 0xfffc,
	.boot_params	= BOOT_PARAMS_OFFSET,
	.map_io			= m1_map_io,
	.init_irq		= m1_irq_init,
	.timer			= &meson_sys_timer,
	.init_machine	= m1_init_machine,
	.fixup			= m1_fixup,
	.video_start	= RESERVED_MEM_START,
	.video_end		= RESERVED_MEM_END,
MACHINE_END
