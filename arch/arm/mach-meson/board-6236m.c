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
#include <mach/am_regs.h>
#include <mach/am_eth_pinmux.h>
#include <mach/nand.h>
#include <mach/card_io.h>
#include <linux/i2c.h>
#include <linux/i2c-aml.h>
#ifdef CONFIG_AM_UART_WITH_S_CORE 
#include <linux/uart-aml.h>
#endif
#include <mach/pinmux.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <mach/clk_set.h>
#include "board-6236m.h"
#if defined(CONFIG_TOUCHSCREEN_ADS7846)
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/spi/ads7846.h>
//#include <linux/gpio.h>
#endif
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <mach/nand.h>


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
		.name="6236",
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
#ifdef CONFIG_USB_DWC_OTG_HCD
static void set_usb_a_vbus_power(char is_power_on)
{
#if 0
codes for 8626m, 6236m gpio need redefined!
#define USB_A_POW_GPIO	PREG_HGPIO
#define USB_A_POW_GPIO_BIT	20
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
#endif
}
//usb_a is OTG port
static struct lm_device usb_ld_a = {
	.type = LM_DEVICE_TYPE_USB,
	.id = 0,
	.irq = INT_USB_A,
	.resource.start = IO_USB_A_BASE,
	.resource.end = -1,
	.dma_mask_room = DMA_BIT_MASK(32),
	.port_type = USB_PORT_TYPE_HOST,
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
};

static struct platform_device codec_device = {
    .name       = "amstream",
    .id         = 0,
    .num_resources = ARRAY_SIZE(codec_resources),
    .resource      = codec_resources,
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

static struct aml_card_info  amlogic_card_info[] = {
	[0] = {
		.name = "sd_card",
		.work_mode = CARD_HW_MODE,
		.io_pad_type = SDIO_GPIOA_9_14,
		.card_ins_en_reg = EGPIO_GPIOA_ENABLE,
		.card_ins_en_mask = PREG_IO_3_MASK,
		.card_ins_input_reg = EGPIO_GPIOA_INPUT,
		.card_ins_input_mask = PREG_IO_3_MASK,
		.card_power_en_reg = JTAG_GPIO_ENABLE,
		.card_power_en_mask = PREG_IO_16_MASK,
		.card_power_output_reg = JTAG_GPIO_OUTPUT,
		.card_power_output_mask = PREG_IO_20_MASK,
		.card_power_en_lev = 0,
		.card_wp_en_reg = EGPIO_GPIOA_ENABLE,
		.card_wp_en_mask = PREG_IO_11_MASK,
		.card_wp_input_reg = EGPIO_GPIOA_INPUT,
		.card_wp_input_mask = PREG_IO_11_MASK,
		.card_extern_init = 0,
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

#if defined(CONFIG_TOUCHSCREEN_ADS7846)
#define SPI_0		0
#define SPI_1		1
#define SPI_2		2

// GPIOD_19 (pin208)
#define GPIO_SPI_SCK		((GPIOD_bank_bit2_24(19)<<16) |GPIOD_bit_bit2_24(19)) 
// GPIOD_20 (pin209)
#define GPIO_SPI_MOSI		((GPIOD_bank_bit2_24(20)<<16) |GPIOD_bit_bit2_24(20)) 
// GPIOD_21(pin210)
#define GPIO_SPI_MISO		((GPIOD_bank_bit2_24(21)<<16) |GPIOD_bit_bit2_24(21))
// GPIOD_18 (pin207)
#define GPIO_TSC2046_CS	((GPIOD_bank_bit2_24(18)<<16) |GPIOD_bit_bit2_24(18)) 
// GPIOC_10 (pin171)
#define GPIO_TSC2046_PENDOWN	((GPIOC_bank_bit0_26(10)<<16) |GPIOC_bit_bit0_26(10)) 

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


int ads7846_init_gpio(void)
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
	/* set gpio interrupt #0 source=GPIOC_10, and triggered by falling edge(=1) */
	gpio_enable_edge_int(33, 1, 0);

	// reg12 bit22,23,24,25
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_12, 0xf<<22);
	// reg7 bit19
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, 0x1<<19);
	// reg7 bit14,15,16
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, 0x7<<14);
	// reg5 bit17
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, 0x1<<17);
	// reg5 bit12
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, 0x1<<12);
	// reg5 bit4,5,6,7
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, 0xf<<4);
	// reg8 bit25,26,27,28
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, 0xf<<25);

	return 0;
}
#endif

#if defined(CONFIG_NAND_FLASH_DRIVER_BASE_OPERATE)
static struct mtd_partition partition_info[] = 
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
		.size = 4 * 1024*1024,
	//	.set_flags=0,
	//	.dual_partnum=0,
	},
	{
		.name = "YAFFS2",
		.offset=MTDPART_OFS_APPEND,
		.size=MTDPART_SIZ_FULL,
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

static struct resource aml_nand_resources[] = 
{
	{
		.start = 0xc1108600,
		.end = 0xc1108624,
		.flags = IORESOURCE_MEM,
	},
};
//for  6236M MID 
static struct aml_m1_nand_platform aml_Micron4GBABAnand_platform = 
{
	.page_size = 2048*2,
	.spare_size= 224,		//for micron ABA 4GB
	.erase_size=1024*1024,
	.bch_mode=		3,		//BCH16
	.encode_size=540,				
	.timing_mode=5,
	.onfi_mode=1,
	.ce_num=1,
	.partitions = partition_info,
	.nr_partitions = ARRAY_SIZE(partition_info),
};

//for ref 
static struct aml_m1_nand_platform aml_2Kpage128Kblocknand_platform = 
{
	.page_size = 2048,
	.spare_size= 64,		
	.erase_size= 128*1024,
	.bch_mode  =  1,		//BCH8
	.encode_size= 528,				
	.timing_mode= 5,
	.ce_num     = 1,
	.partitions = partition_info,
	.nr_partitions = ARRAY_SIZE(partition_info),
};


static struct platform_device aml_nand_device = 
{
	.name = "aml_m1_nand",
	.id = 0,
	.num_resources = ARRAY_SIZE(aml_nand_resources),
	.resource = aml_nand_resources,
	.dev = {
	//	.platform_data = &aml_Micron4GBABAnand_platform,
		.platform_data = &aml_2Kpage128Kblocknand_platform,
	},
};
#endif

#define PINMUX_UART_A   UART_A_GPIO_C21_D22
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

static struct platform_device __initdata *platform_devs[] = {
    #if defined(CONFIG_JPEGLOGO)
		&jpeglogo_device,
	#endif	
    #if defined(CONFIG_FB_AM)
    	&fb_device,
    #endif
    #if  defined(CONFIG_AM_TV_OUTPUT)||defined(CONFIG_AM_TCON_OUTPUT)
       &vout_device,	
    #endif		
    #if defined(CONFIG_AM_STREAMING)
		&codec_device,
    #endif
    #if defined(CONFIG_AM_UART_WITH_S_CORE)
        &aml_uart_device,
    #endif
    #if defined(CONFIG_TVIN_VDIN)
        &vdin_device,
		&bt656in_device,

    #endif
	#if defined(CONFIG_AML_AUDIO_DSP)
		&audiodsp_device,
	#endif
	#if defined(CONFIG_CARDREADER)
    	&amlogic_card_device,
    #endif
    #if defined(CONFIG_KEYPADS_AM)||defined(CONFIG_VIRTUAL_REMOTE)||defined(CONFIG_KEYPADS_AM_MODULE) 
		&input_device,
    #endif	
#if defined(CONFIG_TOUCHSCREEN_ADS7846)
	&spi_gpio,
#endif
	 #if defined(CONFIG_NAND_FLASH_DRIVER_BASE_OPERATE)
		&aml_nand_device,
    #endif		
};

static void __init eth_pinmux_init(void)
{
    	//eth_clk_set(ETH_CLKSRC_SYS_D3,900*CLK_1M/3,50*CLK_1M);
	/*for dpf_sz with ethernet*/	
    	eth_set_pinmux(ETH_BANK1_GPIOD2_D11,ETH_CLK_OUT_GPIOD7_REG4_20,(0xf<<15|1<<21 |3<<24));
	//RMII RX_D0/D1
	eth_set_pinmux(ETH_BANK2_GPIOD15_D23,ETH_CLK_OUT_GPIOD7_REG4_20,(1<<4 | 1<<5));
	CLEAR_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);
	SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, (1 << 1));
	SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);
	udelay(100);
	/*reset*/
	set_gpio_mode(PREG_FGPIO,26,GPIO_OUTPUT_MODE);
	set_gpio_val(PREG_FGPIO,26,0);
	udelay(100);	//waiting reset end;
	set_gpio_val(PREG_FGPIO,26,1);
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
}

static void __init  device_clk_setting(void)
{
	/*Demod CLK for eth and sata*/
	demod_apll_setting(0,1200*CLK_1M);
	/*eth clk*/
    	eth_clk_set(ETH_CLKSRC_APLL_CLK,400*CLK_1M,50*CLK_1M);
}

static __init void m1_init_machine(void)
{
	meson_cache_init();

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

MACHINE_START(MESON_6236M, "AMLOGIC MESON-M1 6236M SZ")
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
