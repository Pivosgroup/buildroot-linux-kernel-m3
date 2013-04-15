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
#include <mach/usbclock.h>
#include <mach/am_regs.h>

#ifdef CONFIG_AM_UART_WITH_S_CORE 
#include <linux/uart-aml.h>
#endif
#include <mach/card_io.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <mach/clk_set.h>
#include "board-stv-mbx-mc.h"


#ifdef CONFIG_ANDROID_PMEM
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/android_pmem.h>
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

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE
#include <media/amlogic/aml_camera.h>
#endif






#ifdef CONFIG_EFUSE
#include <linux/efuse.h>
#endif

#ifdef CONFIG_AML_HDMI_TX
#include <linux/hdmi/hdmi_config.h>
#endif

#if defined(CONFIG_LEDS_GPIO)
#include <linux/leds.h>
#endif

/* GPIO Defines */
// LEDS
#define GPIO_LED_STATUS GPIO_AO(10)
#define GPIO_LED_POWER GPIO_AO(11)

#if defined(CONFIG_LEDS_GPIO)
/* LED Class Support for the leds */
static struct gpio_led aml_led_pins[] = {
	{
		.name		 = "Powerled",
		.default_trigger = "default-on",
		.gpio		 = GPIO_LED_POWER,
		.active_low	 = 0,
	},
	{
		.name		 = "Statusled",
#if defined(CONFIG_LEDS_TRIGGER_REMOTE_CONTROL)
		.default_trigger = "rc", 
#else
		.default_trigger = "none",
#endif
		.gpio		 = GPIO_LED_STATUS,
		.active_low	 = 1,
	},
};

static struct gpio_led_platform_data aml_led_data = {
	.leds	  = aml_led_pins,
	.num_leds = ARRAY_SIZE(aml_led_pins),
};

static struct platform_device aml_leds = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &aml_led_data,
	}
};
#endif

#if defined(CONFIG_AML_HDMI_TX)
static struct hdmi_phy_set_data brd_phy_data[] = {
//    {27, 0xf7, 0x0},    // an example: set Reg0xf7 to 0 in 27MHz
    {-1,   -1},         //end of phy setting
};

static struct hdmi_config_platform_data aml_hdmi_pdata ={
    .hdmi_5v_ctrl = NULL,
    .hdmi_3v3_ctrl = NULL,
    .hdmi_pll_vdd_ctrl = NULL,
    .hdmi_sspll_ctrl = NULL,
    .phy_data = brd_phy_data,
};

static struct platform_device aml_hdmi_device = {
    .name = "amhdmitx",
    .id   = -1,
    .dev  = {
        .platform_data = &aml_hdmi_pdata,
    }
};
#endif

#ifdef CONFIG_SUSPEND
static int suspend_state=0;
#endif

static int board_ver_id = 0; // Rony add 20120611 get hardware id 
static int board_dvb_id = 0; // Rony add 20120702 0 dvbt,1 dvbs

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
        .name="8726M3",
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

#if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
#include <linux/input.h>
#include <linux/adc_keypad.h>
#include <linux/saradc.h>

static struct adc_key adc_kp_key[] = {   
    {KEY_PLAYCD, "recovery", CHAN_4, 0,  60},
    /*
    {KEY_F1,		"menu",  	CHAN_4, 0, 	60},
    {KEY_ESC,		"exit",  	CHAN_4, 124, 60},
    {KEY_ENTER,		"enter",    CHAN_4, 247, 60},
    {KEY_UP,		"up",  		CHAN_4, 371, 60},
    {KEY_LEFT,		"left", 	CHAN_4, 495, 60},
    {KEY_DOWN,		"down", 	CHAN_4, 619, 60},
    {KEY_RIGHT,		"right",    CHAN_4, 745, 60},
    */
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
        set_gpio_mode(GPIOAO_bank_bit0_11(3), GPIOAO_bit_bit0_11(3), GPIO_INPUT_MODE);
//    WRITE_AOBUS_REG(AO_RTC_ADDR0, (READ_AOBUS_REG(AO_RTC_ADDR0) &~(1<<11)));
//    WRITE_AOBUS_REG(AO_RTC_ADDR1, (READ_AOBUS_REG(AO_RTC_ADDR1) &~(1<<3)));
    return 0;
}
static inline int key_scan(int *key_state_list)
{
    int ret = 0;
	 // GPIOAO_3
	 #ifdef CONFIG_SUSPEND
	 if(suspend_state)
	 	{
	 	// forse power key down
	 	suspend_state--;
	 	key_state_list[0] = 1;
	 	}
	 else
	 #endif
    key_state_list[0] = get_gpio_val(GPIOAO_bank_bit0_11(3), GPIOAO_bit_bit0_11(3))?0:1;
//    key_state_list[0] = ((READ_AOBUS_REG(AO_RTC_ADDR1) >> 2) & 1) ? 0 : 1;
    return ret;
}

static  struct key_input_platform_data  key_input_pdata = {
    .scan_period = 25,
    .fuzz_time = 40,
    .key_code_list = &_key_code_list[0],
    .key_num = ARRAY_SIZE(_key_code_list),
    .scan_func = key_scan,
    .init_func = key_input_init_func,
    .config =  2, 	// 0: interrupt;    	2: polling;
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

#if defined(CONFIG_AM_IR_RECEIVER)
#include <linux/input/irreceiver.h>

static int ir_init()
{
    unsigned int control_value;
    
    //mask--mux gpioao_7 to remote
    SET_AOBUS_REG_MASK(AO_RTI_PIN_MUX_REG,1<<0);
    
    //max frame time is 80ms, base rate is 20us
    control_value = 3<<28|(0x9c40 << 12)|0x13;
    WRITE_AOBUS_REG(AO_IR_DEC_REG0, control_value);
     
    /*[3-2]rising or falling edge detected
      [8-7]Measure mode
    */
    control_value = 0x8574;
    WRITE_AOBUS_REG(AO_IR_DEC_REG1, control_value);
    
    return 0;
}

static int pwm_init()
{
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<16));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<29));
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<26));
}

static struct irreceiver_platform_data irreceiver_data = {
    .pwm_no = PWM_A,
    .freq = 38000, //38k
    .init_pwm_pinmux = pwm_init,
    .init_ir_pinmux = ir_init,
};

static struct platform_device irreceiver_device = {
    .name = "irreceiver",
    .id = 0,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &irreceiver_data,
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

#if defined(CONFIG_AMLOGIC_SPI_NOR)
static struct mtd_partition spi_partition_info[] = {
#ifdef CONFIG_STV_RECOVERY
	{
    .name = "spi",
    .offset = 0,
    .size = 0x3fe000,
	},
  {
    .name = "uboot",
    .offset = 0,
    .size = 0x4e000,
  },
#endif
	{
		.name = "ubootenv",
		.offset = 0x4e000,
		.size = 0x2000,
	},
#ifdef CONFIG_STV_RECOVERY
  {
    .name = "recovery",
    .offset = 0x50000,
    .size = 0x3ae000,
  },
#endif
  {
    .name = "ids",
    .offset = 0x3fe000,
    .size = 0x2000,
  },
};

static struct flash_platform_data amlogic_spi_platform = {
    .parts = spi_partition_info,
    .nr_parts = ARRAY_SIZE(spi_partition_info),
};

static struct resource amlogic_spi_nor_resources[] = {
    {
        .start = 0xc1800000,
        .end = 0xc1ffffff,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device amlogic_spi_nor_device = {
    .name = "AMLOGIC_SPI_NOR",
    .id = -1,
    .num_resources = ARRAY_SIZE(amlogic_spi_nor_resources),
    .resource = amlogic_spi_nor_resources,
    .dev = {
        .platform_data = &amlogic_spi_platform,
    },
};
#endif

#ifdef CONFIG_USB_DWC_OTG_HCD
static void set_usb_a_vbus_power(char is_power_on)
{
}

static void set_usb_b_vbus_power(char is_power_on)
{ /*wifi rtl8188cus power control*/
#define USB_B_POW_GPIO         GPIOC_bank_bit0_15(5)
#define USB_B_POW_GPIO_BIT     GPIOC_bit_bit0_15(5)
#define USB_B_POW_GPIO_BIT_ON   1
#define USB_B_POW_GPIO_BIT_OFF  0
	//Rony add,if board_ver_id equal 0x02 then inverse wifi power level 20120612
	if((board_ver_id == 0x02) || (board_ver_id == 0x03)){
		if(is_power_on){
			is_power_on = 0;
		}else{
			is_power_on = 1;
		}
	}
	//Rony add end
    if(is_power_on) {
        printk(KERN_INFO "set usb b port power on (board gpio %d)!\n",USB_B_POW_GPIO_BIT);
        set_gpio_mode(USB_B_POW_GPIO, USB_B_POW_GPIO_BIT, GPIO_OUTPUT_MODE);
        set_gpio_val(USB_B_POW_GPIO, USB_B_POW_GPIO_BIT, USB_B_POW_GPIO_BIT_ON);
    } else    {
        printk(KERN_INFO "set usb b port power off (board gpio %d)!\n",USB_B_POW_GPIO_BIT);
        set_gpio_mode(USB_B_POW_GPIO, USB_B_POW_GPIO_BIT, GPIO_OUTPUT_MODE);
        set_gpio_val(USB_B_POW_GPIO, USB_B_POW_GPIO_BIT, USB_B_POW_GPIO_BIT_OFF);
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
    .dma_config = USB_DMA_BURST_SINGLE , //   USB_DMA_DISABLE,
    .set_vbus_power = set_usb_b_vbus_power,
};

#endif


/* usb wifi power 1:power on  0:power off */
void extern_usb_wifi_power(int is_power)
{
/*    printk(KERN_INFO "usb_wifi_power %s\n", is_power ? "On" : "Off");	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<6));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<15));
	//Rony add,if board_ver_id equal 0x02 then inverse wifi power level 20120612
	if((board_ver_id == 0x02) || (board_ver_id == 0x03)){
		if(is_power){
			is_power = 0;
		}else{
			is_power = 1;
		}
	}
	//Rony add end
    set_gpio_mode(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), GPIO_OUTPUT_MODE);
	if(is_power)
       set_gpio_val(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), 1);
	else
       set_gpio_val(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), 0);  
   */          
}

EXPORT_SYMBOL(extern_usb_wifi_power);

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

#if defined(CONFIG_SDIO_DHD_CDC_WIFI_40181_MODULE_MODULE)
/******************************
*WL_REG_ON	-->GPIOC_8
*WIFI_32K		-->GPIOC_15(CLK_OUT1)
*WIFIWAKE(WL_HOST_WAKE)-->GPIOX_11
*******************************/
#define WL_REG_ON_USE_GPIOC_6
void extern_wifi_set_enable(int enable)
{
	if(enable){
#ifdef WL_REG_ON_USE_GPIOC_6
		SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));
#else
		SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
#endif
		printk("Enable WIFI  Module!\n");
	}
    	else{
#ifdef WL_REG_ON_USE_GPIOC_6
		CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));
#else
		CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
#endif
		printk("Disable WIFI  Module!\n");
	}
}
EXPORT_SYMBOL(extern_wifi_set_enable);

static void wifi_set_clk_enable(int on)
{
    //set clk for wifi
	printk("set WIFI CLK Pin GPIOC_15 32KHz ***%d\n",on);
	WRITE_CBUS_REG(HHI_GEN_CLK_CNTL,(READ_CBUS_REG(HHI_GEN_CLK_CNTL)&(~(0x7f<<0)))|((0<<0)|(1<<8)|(7<<9)) );
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<15));   
	if(on)
		SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<22));
	else
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<22));	
}

static void wifi_gpio_init(void)
{
#ifdef WL_REG_ON_USE_GPIOC_6
    //set WL_REG_ON Pin GPIOC_6 out
        CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<16));
        CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<5));
        CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<6));  //GPIOC_6
#else
    //set WL_REG_ON Pin GPIOC_8 out 
   	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<23));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<18));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<10));
     	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<8));  //GPIOC_8
#endif
}


static void aml_wifi_bcm4018x_init()
{
	wifi_set_clk_enable(1);
	wifi_gpio_init();
	extern_wifi_set_enable(0);
        msleep(5);
	extern_wifi_set_enable(1);
}

#endif

#if defined(CONFIG_CARDREADER)
static struct resource amlogic_card_resource[] = {
    [0] = {
        .start = 0x1200230,   //physical address
        .end   = 0x120024c,
        .flags = 0x200,
    }
};

#if defined(CONFIG_SDIO_DHD_CDC_WIFI_40181_MODULE_MODULE)
#define GPIO_WIFI_HOSTWAKE  ((GPIOX_bank_bit0_31(11)<<16) |GPIOX_bit_bit0_31(11))
void sdio_extern_init(void)
{
	printk("sdio_extern_init !\n");
	printk("40183 set oob mode !\n");
  SET_CBUS_REG_MASK(PAD_PULL_UP_REG4, (1<<11));
	gpio_direction_input(GPIO_WIFI_HOSTWAKE);
#if defined(CONFIG_BCM40181_WIFI)
	gpio_enable_level_int(gpio_to_idx(GPIO_WIFI_HOSTWAKE), 0, 4);  //for 40181	
#endif
#if defined(CONFIG_BCM40183_WIFI)
	gpio_enable_edge_int(gpio_to_idx(GPIO_WIFI_HOSTWAKE), 1, 5);     //for 40183
#endif 
	extern_wifi_set_enable(1);
}
#endif

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
#if defined(CONFIG_SDIO_DHD_CDC_WIFI_40181_MODULE_MODULE)
    [1] = {
        .name = "sdio_card",
        .work_mode = CARD_HW_MODE,
        .io_pad_type = SDIO_A_GPIOX_0_3,
        .card_ins_en_reg = 0,
        .card_ins_en_mask = 0,
        .card_ins_input_reg = 0,
        .card_ins_input_mask = 0,
        .card_power_en_reg = 0,
        .card_power_en_mask = 0,
        .card_power_output_reg = 0,
        .card_power_output_mask = 0,
        .card_power_en_lev = 1,
        .card_wp_en_reg = 0,
        .card_wp_en_mask = 0,
        .card_wp_input_reg = 0,
        .card_wp_input_mask = 0,
        .card_extern_init = sdio_extern_init,
    },
#endif
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

static struct resource aml_m3_audio_resource[] = {
    [0] =   {
        .start  =   0,
        .end        =   0,
        .flags  =   IORESOURCE_MEM,
    },
};

#if defined(CONFIG_SND_AML_M3)
static struct platform_device aml_audio = {
    .name               = "aml_m3_audio",
    .id                     = -1,
    .resource       =   aml_m3_audio_resource,
    .num_resources  =   ARRAY_SIZE(aml_m3_audio_resource),
};

int aml_m3_is_hp_pluged(void)
{
	return READ_CBUS_REG_BITS(PREG_PAD_GPIO0_I, 19, 1); //return 1: hp pluged, 0: hp unpluged.
}


struct aml_m3_platform_data {
    int (*is_hp_pluged)(void);
};


static struct aml_m3_platform_data aml_m3_pdata = {
    .is_hp_pluged = &aml_m3_is_hp_pluged,
};

void mute_spk(struct snd_soc_codec* codec, int flag)
{
#ifdef _AML_M3_HW_DEBUG_
	printk("***Entered %s:%s\n", __FILE__,__func__);
#endif

// Rony add it reversal level 20120611 
	//printk("====================== mute spk flag = %d\n",flag);
	if(board_ver_id == 0x02){
		if(flag){
			flag = 0;
		}else{
			flag = 1;
		}
	}
	//printk("====================== mute spk flag1 = %d\n",flag);
// Rony add end
    if(flag){
		set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 0);	 // mute speak
		set_gpio_mode(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), GPIO_OUTPUT_MODE);
	}else{
		set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 1);	 // unmute speak
		set_gpio_mode(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), GPIO_OUTPUT_MODE);
	}
}



#define APB_BASE 0x4000
void mute_headphone(void* codec, int flag)
{
	int reg_val;
#ifdef _AML_M3_HW_DEBUG_
	printk("***Entered %s:%s\n", __FILE__,__func__);
#endif
	reg_val = READ_APB_REG(APB_BASE+(0x18<<2));
    if(flag){
		reg_val |= 0xc0;
		WRITE_APB_REG((APB_BASE+(0x18<<2)), reg_val);			// mute headphone
	}else{
		reg_val &= ~0xc0;
		WRITE_APB_REG((APB_BASE+(0x18<<2)), reg_val);			// unmute headphone
	}
}


#endif

#ifdef CONFIG_SND_AML_M3_CS4334
static struct platform_device aml_sound_card={
       .name                   = "aml_m3_audio_cs4334",
       .id                     = -1,
       .resource               = aml_m3_audio_resource,
       .num_resources          = ARRAY_SIZE(aml_m3_audio_resource),
};

/* --------------------------------------------------------------------------*/
/**
 * * @brief  set_audio_codec_pinmux
 * *
 * * @return
 * */
/* --------------------------------------------------------------------------*/
static void __init set_audio_codec_pinmux(void)
{
    /* for gpiox_17~20 I2S_AMCLK I2S_AOCLK I2S_LRCLK I2S_OUT */
    clear_mio_mux(7, (1 << 18) | (1 << 19) | (1 << 20) | (1 << 21) | (1 << 22) | (1 << 23));
    set_mio_mux(8, (1 << 27) | (1 << 26) | (1 << 25) | (1 << 24));
}
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
static  struct platform_device aml_rtc_device = {
            .name            = "aml_rtc",
            .id               = -1,
    };
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

#if defined(CONFIG_SUSPEND)

typedef struct {
	char name[32];
	unsigned bank;
	unsigned bit;
	gpio_mode_t mode;
	unsigned value;
	unsigned enable;
} gpio_data_t;

#define MAX_GPIO 3
static gpio_data_t gpio_data[MAX_GPIO] = {
{"GPIOD6--HDMI", 	GPIOD_bank_bit0_9(6), 	GPIOD_bit_bit0_9(6), 	GPIO_OUTPUT_MODE, 1, 1},
{"GPIOD9--VCC5V", GPIOD_bank_bit0_9(9), 	GPIOD_bit_bit0_9(9), 	GPIO_OUTPUT_MODE, 1, 1},
{"GPIOX29--MUTE", 	GPIOX_bank_bit0_31(29), GPIOX_bit_bit0_31(29), GPIO_OUTPUT_MODE, 1, 1},
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
        printk(KERN_INFO "set_vcc power up\n");

		#ifdef CONFIG_AML_SUSPEND
       suspend_state=5;
       #endif
//        set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
//        set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 0);
              
    }
    else{
        printk(KERN_INFO "set_vcc power down\n");       			
//        set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
//        set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 1);
		save_pinmux();
		for (i=0;i<MAX_GPIO;i++)
			save_gpio(i);
    }
}

static void set_gpio_suspend_resume(int power_on)
{
    if(power_on)
    	{
    	printk("set gpio resume.\n");
		 // HDMI
        extern void hdmi_wr_reg(unsigned long addr, unsigned long data);
        hdmi_wr_reg(0x8005, 2); 
		 udelay(50);
        hdmi_wr_reg(0x8005, 1); 
        // LED
        WRITE_CBUS_REG(PWM_PWM_C, (0xff00<<16) |(0xff00<<0));
    	}
	else
		{
    	printk("set gpio suspend.\n");
		 // LED
        WRITE_CBUS_REG(PWM_PWM_C, (0xff00<<16) |(0<<0));
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
    .set_exgpio_early_suspend = set_gpio_suspend_resume,
};

static struct platform_device aml_pm_device = {
    .name           = "pm-meson",
    .dev = {
        .platform_data  = &aml_pm_pdata,
    },
    .id             = -1,
};
#endif

#if defined(CONFIG_I2C_SW_AML)

//add by stevevn for sw i2c
#define MESON_I2C_PREG_GPIOX_OE			CBUS_REG_ADDR(0x2018)
#define MESON_I2C_PREG_GPIOX_OUTLVL		CBUS_REG_ADDR(0x2019)
#define MESON_I2C_PREG_GPIOX_INLVL		CBUS_REG_ADDR(0x201a)

static struct aml_sw_i2c_platform aml_sw_i2c_plat = {
    .sw_pins = {
        .scl_reg_out        = MESON_I2C_PREG_GPIOX_OUTLVL,
        .scl_reg_in     = MESON_I2C_PREG_GPIOX_INLVL,
        .scl_bit            = 28, 
        .scl_oe         = MESON_I2C_PREG_GPIOX_OE,
        .sda_reg_out        = MESON_I2C_PREG_GPIOX_OUTLVL,
        .sda_reg_in     = MESON_I2C_PREG_GPIOX_INLVL,
        .sda_bit            = 27,    
        .sda_oe         = MESON_I2C_PREG_GPIOX_OE,
    },  
    .udelay         = 5, //2,
    .timeout            = 100,
};

static struct platform_device aml_sw_i2c_device = {
    .name         = "aml-sw-i2c",
    .id       = 0,
    .dev = {
        .platform_data = &aml_sw_i2c_plat,
    },
};

#define MESON3_I2C_PREG_GPIOX_OE		CBUS_REG_ADDR(PREG_PAD_GPIO4_EN_N)
#define MESON3_I2C_PREG_GPIOX_OUTLVL	CBUS_REG_ADDR(PREG_PAD_GPIO4_O)
#define MESON3_I2C_PREG_GPIOX_INLVL	CBUS_REG_ADDR(PREG_PAD_GPIO4_I)


static struct aml_sw_i2c_platform aml_sw_i2c_plat_tuner = {
    .sw_pins = {
        .scl_reg_out        = MESON3_I2C_PREG_GPIOX_OUTLVL,
        .scl_reg_in     = MESON3_I2C_PREG_GPIOX_INLVL,
        .scl_bit            = 26, 
        .scl_oe         = MESON3_I2C_PREG_GPIOX_OE,
        .sda_reg_out        = MESON3_I2C_PREG_GPIOX_OUTLVL,
        .sda_reg_in     = MESON3_I2C_PREG_GPIOX_INLVL,
        .sda_bit            = 25,
        .sda_oe         = MESON3_I2C_PREG_GPIOX_OE,
    },  
    .udelay         = 2,
    .timeout            = 100,
};

static struct platform_device aml_sw_i2c_device_tuner = {
    .name         = "aml-sw-i2c",
    .id       = 1,
    .dev = {
        .platform_data = &aml_sw_i2c_plat_tuner,
    },
};


#endif

#if defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)
static struct aml_i2c_platform aml_i2c_plat = {
    .wait_count     = 50000,
    .wait_ack_interval  = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
//    .master_no      = 0,
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
//    .master_no      = 1,
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
//    .master_no      = 2,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_300K,

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



#if defined(CONFIG_AM_UART_WITH_S_CORE)
static struct aml_uart_platform aml_uart_plat = {
    .uart_line[0]       =   UART_AO,
    .uart_line[1]       =   UART_A,
    .uart_line[2]       =   UART_B,
    .uart_line[3]       =   UART_C
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

#ifdef CONFIG_AM_NAND

static struct mtd_partition multi_partition_info[] = 
{ // 2G
#ifndef CONFIG_AMLOGIC_SPI_NOR
/* Hide uboot partition
	{
		.name = "uboot",
		.offset = 0,
		.size = 4*1024*1024,
	},
//*/
    {
        .name = "ubootenv",
        .offset = 4*1024*1024,
        .size = 0x2000,
    },
/* Hide recovery partition
    {
        .name = "recovery",
        .offset = 6*1024*1024,
        .size = 2*1024*1024,
    },
//*/
#endif
	{//4M for logo
		.name = "logo",
		.offset = 0*1024*1024,
		.size = 4*1024*1024,
	},
	{//8M for kernel
		.name = "boot",
		.offset = (0+4)*1024*1024,
		.size = 8*1024*1024,
	},
	{//512M for android system.
	    .name = "system",
	    .offset = (0+4+8)*1024*1024,
	    .size = 512*1024*1024,
	},
	{//500M for cache
	    .name = "cache",
	    .offset = (0+4+8+512)*1024*1024,
	    .size = 500*1024*1024,
	},
	
#ifdef CONFIG_AML_NFTL
	{//1G for NFTL_part
	    .name = "NFTL_Part",
	    .offset=(0+4+8+512+500)*1024*1024,
	    .size=512*1024*1024,
	},
	{//other for user data
	.name = "userdata",
	.offset = MTDPART_OFS_APPEND,
	.size = MTDPART_SIZ_FULL,
	},
#else
    {
        .name = "userdata",
        .offset=MTDPART_OFS_APPEND,
        .size=MTDPART_SIZ_FULL,
    },
#endif

};

static struct mtd_partition multi_partition_info_4G_or_More[] =
{ // 4G
#ifndef CONFIG_AMLOGIC_SPI_NOR
/* Hide uboot partition
	{
		.name = "uboot",
		.offset = 0,
		.size = 4*1024*1024,
	},
//*/
    {
        .name = "ubootenv",
        .offset = 4*1024*1024,
        .size = 0x2000,
    },
/* Hide recovery partition
    {
        .name = "recovery",
        .offset = 6*1024*1024,
        .size = 2*1024*1024,
    },
//*/
#endif
	{//4M for logo
		.name = "logo",
		.offset = 0*1024*1024,
		.size = 6*1024*1024,
	},
	{//8M for kernel
		.name = "boot",
		.offset = (0+6)*1024*1024,
		.size = 10*1024*1024,
	},
	{//512M for android system.
	    .name = "system",
	    .offset = (0+6+10)*1024*1024,
	    .size = 512*1024*1024,
	},
	{//300M for cache
	    .name = "cache",
	    .offset = (0+6+10+512)*1024*1024,
	    .size = 300*1024*1024,
	},

#ifdef CONFIG_AML_NFTL
	{//1G for NFTL_part
	    .name = "NFTL_Part",
	    .offset=(0+6+10+512+300)*1024*1024,
	    .size=1024*1024*1024,
	},
	{//200M for backup
	    .name = "backup",
	    .offset = (0+6+10+512+300+1024)*1024*1024,
	    .size = 200*1024*1024,
	},
	{//other for user data
	.name = "userdata",
	.offset = MTDPART_OFS_APPEND,
	.size = MTDPART_SIZ_FULL,
	},
#else
    {
        .name = "userdata",
        .offset=MTDPART_OFS_APPEND,
        .size=MTDPART_SIZ_FULL,
    },
#endif
};

static void nand_set_parts(uint64_t size, struct platform_nand_chip *chip)
{
    printk("set nand parts for chip %lldMB\n", (size/(1024*1024)));

    if (size/(1024*1024) == (1024*2)) {
        chip->partitions = multi_partition_info;
        chip->nr_partitions = ARRAY_SIZE(multi_partition_info);
        }
    else if (size/(1024*1024) >= (1024*4)) {
        chip->partitions = multi_partition_info_4G_or_More;
        chip->nr_partitions = ARRAY_SIZE(multi_partition_info_4G_or_More);
        }
    else {
        // Undefined
        chip->partitions = NULL;
        chip->nr_partitions = 0;
        }
    return;
}

static struct aml_nand_platform aml_nand_mid_platform[] = {
#ifndef CONFIG_AMLOGIC_SPI_NOR
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
#endif
{
		.name = NAND_MULTI_NAME,
		.chip_enable_pad = (AML_NAND_CE0 | (AML_NAND_CE1 << 4) | (AML_NAND_CE2 << 8) | (AML_NAND_CE3 << 12)),
		.ready_busy_pad = (AML_NAND_CE0 | (AML_NAND_CE0 << 4) | (AML_NAND_CE1 << 8) | (AML_NAND_CE1 << 12)),
		.platform_nand_data = {
			.chip =  {
				.nr_chips = 4,
				.nr_partitions = ARRAY_SIZE(multi_partition_info),
				.partitions = multi_partition_info,
				.set_parts = nand_set_parts,
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
#if  defined(CONFIG_AM_TV_OUTPUT2) // Rony merge 20120521 
static struct resource vout2_device_resources[] = {
    [0] = {
        .start = 0,
        .end   = 0,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device vout2_device = {
    .name       = "mesonvout2",
    .id         = 0,
    .num_resources = ARRAY_SIZE(vout2_device_resources),
    .resource      = vout2_device_resources,
};
#endif

 #ifdef CONFIG_POST_PROCESS_MANAGER
static struct resource ppmgr_resources[] = {
    [0] = {
        .start =  PPMGR_ADDR_START,
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
#ifdef CONFIG_FREE_SCALE
static struct resource freescale_resources[] = {
    [0] = {
        .start = FREESCALE_ADDR_START,
        .end   = FREESCALE_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device freescale_device =
{
    .name           = "freescale",
    .id             = 0,
    .num_resources  = ARRAY_SIZE(freescale_resources),
    .resource       = freescale_resources,
};
#endif
#ifdef CONFIG_USB_ANDROID
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
static struct usb_mass_storage_platform_data mass_storage_pdata = {
       .nluns = 2,
       .vendor = "DEV",
       .product = "FROYO",
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
       .product_name   = "FROYO",
       .manufacturer_name = "DEV",
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

#ifdef CONFIG_BT_DEVICE
#include <linux/bt-device.h>

static struct platform_device bt_device = {
	.name             = "bt-dev",
	.id               = -1,
};

static void bt_device_init(void)
{
}

static void bt_device_on(void)
{
}

static void bt_device_off(void)
{
}

struct bt_dev_data bt_dev = {
    .bt_dev_init    = bt_device_init,
    .bt_dev_on      = bt_device_on,
    .bt_dev_off     = bt_device_off,
};
#endif

#if defined(CONFIG_AM_DVB)
static struct resource amlogic_dvb_resource[]  = {
	[0] = {
		.start = INT_DEMUX,                   //demux 0 irq
		.end   = INT_DEMUX,
		.flags = IORESOURCE_IRQ,
		.name  = "demux0_irq"
	},
	[1] = {
		.start = INT_DEMUX_1,                    //demux 1 irq
		.end   = INT_DEMUX_1,
		.flags = IORESOURCE_IRQ,
		.name  = "demux1_irq"
	},
	[2] = {
		.start = INT_DEMUX_2,                    //demux 2 irq
		.end   = INT_DEMUX_2,
		.flags = IORESOURCE_IRQ,
		.name  = "demux2_irq"
	},	
	[3] = {
		.start = INT_ASYNC_FIFO_FLUSH,                   //dvr 0 irq
		.end   = INT_ASYNC_FIFO_FLUSH,
		.flags = IORESOURCE_IRQ,
		.name  = "dvr0_irq"
	},
	[4] = {
		.start = INT_ASYNC_FIFO2_FLUSH,          //dvr 1 irq
		.end   = INT_ASYNC_FIFO2_FLUSH,
		.flags = IORESOURCE_IRQ,
		.name  = "dvr1_irq"
	},	
};

static  struct platform_device amlogic_dvb_device = {
	.name             = "amlogic-dvb",
	.id               = -1,
	.num_resources    = ARRAY_SIZE(amlogic_dvb_resource),
	.resource         = amlogic_dvb_resource,
};
#endif

#ifdef CONFIG_AM_DVB
static struct resource mxl101_resource[]  = {

	[0] = {
		.start = 1,                                    //frontend  i2c adapter id
		.end   = 1,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_i2c"
	},
	[1] = {
		.start = 0xc0,                                 //frontend 0 demod address
		.end   = 0xc0,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_demod_addr"
	},
	[2] = {
		.start = (GPIOD_bank_bit0_9(8)<<16)|GPIOD_bit_bit0_9(8), //reset pin
		.end   = (GPIOD_bank_bit0_9(8)<<16)|GPIOD_bit_bit0_9(8),
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_reset_pin"
	},
	[3] = {
		.start = 0, //reset enable value
		.end   = 0,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_reset_value_enable"
	},
	[4] = {
		.start = (GPIOC_bank_bit0_15(3)<<16)|GPIOC_bit_bit0_15(3),  //power enable pin
		.end   = (GPIOC_bank_bit0_15(3)<<16)|GPIOC_bit_bit0_15(3),
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_reset"
	//	.name  = "frontend0_power_pin"
	},	
	[5] = {
	.start = 0xc0,                                 //is mxl101
	.end   = 0xc0,
	.flags = IORESOURCE_MEM,
	.name  = "frontend0_tuner_addr"
	},	
};

static  struct platform_device mxl101_device = {
	.name             = "mxl101",
	.id               = -1,
	.num_resources    = ARRAY_SIZE(mxl101_resource),
	.resource         = mxl101_resource,
};

static struct resource avl6211_resource[]  = {

	[0] = {
		.start = 1,                                    //frontend  i2c adapter id
		.end   = 1,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_i2c"
	},
	[1] = {
		.start = 0xc0,                                 //frontend 0 demod address
		.end   = 0xc0,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_demod_addr"
	},
	[2] = {
		.start = (GPIOD_bank_bit0_9(8)<<16)|GPIOD_bit_bit0_9(8), //reset pin
		.end   = (GPIOD_bank_bit0_9(8)<<16)|GPIOD_bit_bit0_9(8),
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_reset_pin"
	},
	[3] = {
		.start = 0, //reset enable value
		.end   = 0,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_reset_value_enable"
	},
	[4] = {
		.start = (GPIOC_bank_bit0_15(3)<<16)|GPIOC_bit_bit0_15(3),  //DVBS2 LNBON/OFF pin
		.end   = (GPIOC_bank_bit0_15(3)<<16)|GPIOC_bit_bit0_15(3),
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_LNBON/OFF"
	},	
	[5] = {
	.start = 0xc0,                                 //is avl6211
	.end   = 0xc0,
	.flags = IORESOURCE_MEM,
	.name  = "frontend0_tuner_addr"
	},	
	[6] = {
	.start = (GPIOD_bank_bit0_9(9)<<16)|GPIOD_bit_bit0_9(9),        //is avl6211
	.end   = (GPIOD_bank_bit0_9(9)<<16)|GPIOD_bit_bit0_9(9),
	.flags = IORESOURCE_MEM,
	.name  = "frontend0_POWERON/OFF"
	},	
};

static  struct platform_device avl6211_device = {
	.name             = "avl6211",
	.id               = -1,
	.num_resources    = ARRAY_SIZE(avl6211_resource),
	.resource         = avl6211_resource,
};




static struct resource gx1001_resource[]  = {
	[0] = {
		.start = (GPIOX_bank_bit0_31(31)<<16)|GPIOX_bit_bit0_31(31),                           //frontend 0 reset pin
		.end   = (GPIOX_bank_bit0_31(31)<<16)|GPIOX_bit_bit0_31(31),
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_reset"
	},
	[1] = {
		.start = 1,                                    //frontend 0 i2c adapter id
		.end   = 1,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_i2c"
	},
	[2] = {
		.start = 0xC0,                                 //frontend 0 tuner address
		.end   = 0xC0,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_tuner_addr"
	},
	[3] = {
		.start = 0x18,                                 //frontend 0 demod address
		.end   = 0x18,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_demod_addr"
	},
};

static  struct platform_device gx1001_device = {
	.name             = "gx1001",
	.id               = -1,
	.num_resources    = ARRAY_SIZE(gx1001_resource),
	.resource         = gx1001_resource,
};


static struct resource ite9173_resource[]  = {
	[0] = {
		.start = (GPIOD_bank_bit0_9(8)<<16)|GPIOD_bit_bit0_9(8), //reset pin
		.end   = (GPIOD_bank_bit0_9(8)<<16)|GPIOD_bit_bit0_9(8),
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_reset"
	},
	[1] = {
		.start = 1,                                    //frontend 0 i2c adapter id
		.end   = 1,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_i2c"
	},
	[2] = {
		.start = 0x9E,                                 //frontend 0 tuner address
		.end   = 0x9E,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_tuner_addr"
	},
	[3] = {
		.start =  0x38,                                 //frontend 0 demod address
		.end   =  0x38,
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_demod_addr"
	},
	[4] = {
		.start = (GPIOB_bank_bit0_23(23)<<16)|GPIOB_bit_bit0_23(23),  //// ANT_PWR_CTRL pin
		.end   = (GPIOB_bank_bit0_23(23)<<16)|GPIOB_bit_bit0_23(23),
		.flags = IORESOURCE_MEM,
		.name  = "frontend0_power"
	},
};

static  struct platform_device ite9173_device = {
	.name             = "ite9173",
	.id               = -1,
	.num_resources    = ARRAY_SIZE(ite9173_resource),
	.resource         = ite9173_resource,
};
#endif
#if defined(CONFIG_AML_WATCHDOG)
static struct platform_device aml_wdt_device = {
    .name = "aml_wdt",
    .id   = -1,
    .num_resources = 0,
};
#endif


static struct platform_device __initdata *platform_devs[] = {
#if defined(CONFIG_LEDS_GPIO)
    &aml_leds,
#endif
#if defined(CONFIG_JPEGLOGO)
    &jpeglogo_device,
#endif
#if defined(CONFIG_AML_HDMI_TX)
    &aml_hdmi_device,
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
#if defined(CONFIG_KEYPADS_AM)||defined(CONFIG_VIRTUAL_REMOTE)||defined(CONFIG_KEYPADS_AM_MODULE)
    &input_device,
#endif
#if defined(CONFIG_AMLOGIC_SPI_NOR)
    &amlogic_spi_nor_device,
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
#if defined(CONFIG_NAND_FLASH_DRIVER_MULTIPLANE_CE)
    &aml_nand_device,
#endif
#ifdef CONFIG_BT_DEVICE
 	&bt_device,
#endif
#if defined(CONFIG_AML_RTC)
    &aml_rtc_device,
#endif
#ifdef CONFIG_AMLOGIC_VIDEOIN_MANAGER
	&vm_device,
#endif
#if defined(CONFIG_SUSPEND)
    &aml_pm_device,
#endif
#if defined(CONFIG_ANDROID_PMEM) || defined(CONFIG_CMEM)
    &android_pmem_device,
#endif
#if defined(CONFIG_I2C_SW_AML)
    &aml_sw_i2c_device_tuner,
    &aml_sw_i2c_device,
#endif
#if defined(CONFIG_I2C_AML)|| defined(CONFIG_I2C_HW_AML)
    &aml_i2c_device,
    &aml_i2c_device1,
    &aml_i2c_device2,
#endif
#if defined(CONFIG_AM_UART_WITH_S_CORE)
    &aml_uart_device,
#endif

#if defined(CONFIG_AM_TV_OUTPUT)||defined(CONFIG_AM_TCON_OUTPUT)
    &vout_device,   
#endif
#if defined(CONFIG_AM_TV_OUTPUT2) // Rony merge 20120521
    &vout2_device,   
#endif
#ifdef CONFIG_USB_ANDROID
    &android_usb_device,
    #ifdef CONFIG_USB_ANDROID_MASS_STORAGE
        &usb_mass_storage_device,
    #endif
#endif
#ifdef CONFIG_POST_PROCESS_MANAGER
    &ppmgr_device,
#endif
#ifdef CONFIG_FREE_SCALE
        &freescale_device,
#endif   

#ifdef CONFIG_EFUSE
	&aml_efuse_device,
#endif
#ifdef CONFIG_AM_DVB
	&amlogic_dvb_device,
	&mxl101_device,
	&gx1001_device,
	&avl6211_device,
	&ite9173_device,
#endif
 #if defined(CONFIG_AML_WATCHDOG)
        &aml_wdt_device,
 #endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info[] = {

#ifdef CONFIG_AT24CXX
	{
		I2C_BOARD_INFO("at24cxx",  0x50),
	},
#endif

#ifdef CONFIG_AT88SCXX
	{
		I2C_BOARD_INFO("at88scxx",  0xB6),
	},
#endif

#ifdef CONFIG_IR810_POWEROFF
	{
		I2C_BOARD_INFO("ir810_poweroff",  0x60),
	},
#endif
};
static struct i2c_board_info __initdata aml_i2c_bus_info_1[] = {
};

static struct i2c_board_info __initdata aml_i2c_bus_info_2[] = {
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

static void __init eth_pinmux_init(void)
{
	//Rony add for eth used external clock 20120509
	//int board_hwid = 0;
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_6,(3<<17));//reg6[17/18]=0
	
	
	//board_hwid = = get_hardware_id();	// Rony modify 20120611 move in device_hardware_id_init
	if((board_ver_id == 0x00) || (board_ver_id == 0x01)) //internal Rony add id 0x01 used internal clock
	{
		printk("eth used internal clock\n");
	 	eth_set_pinmux(ETH_BANK0_GPIOY1_Y9, ETH_CLK_OUT_GPIOY0_REG6_17, 0);
	}
	else //external
	{
	  	eth_set_pinmux(ETH_BANK0_GPIOY1_Y9, ETH_CLK_IN_GPIOY0_REG6_18, 0);
		printk("eth used external clock\n");
	}
	 // end Rony add for eth used external clock 20120509
	 
    //power hold
    //setbits_le32(P_PREG_AGPIO_O,(1<<8));
    //clrbits_le32(P_PREG_AGPIO_EN_N,(1<<8));
    //set_gpio_mode(GPIOA_bank_bit(4),GPIOA_bit_bit0_14(4),GPIO_OUTPUT_MODE);
    //set_gpio_val(GPIOA_bank_bit(4),GPIOA_bit_bit0_14(4),1);

    CLEAR_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);           // Disable the Ethernet clocks
	// ---------------------------------------------
	// Test 50Mhz Input Divide by 2
	// ---------------------------------------------
	// Select divide by 2
    CLEAR_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, (1<<3));     // desc endianess "same order" 
    CLEAR_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, (1<<2));     // ata endianess "little"
    SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, (1 << 1));     // divide by 2 for 100M
    SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);            // enable Ethernet clocks
    udelay(100);
    /* reset phy with GPIOA_23*/
    set_gpio_mode(GPIOA_bank_bit0_27(23), GPIOA_bit_bit0_27(23), GPIO_OUTPUT_MODE);
    set_gpio_val(GPIOA_bank_bit0_27(23), GPIOA_bit_bit0_27(23), 0);
    udelay(100);    //GPIOA_bank_bit0_27(23) reset end;
    set_gpio_val(GPIOA_bank_bit0_27(23), GPIOA_bit_bit0_27(23), 1);
    udelay(100);    //waiting reset end;
#if 0
    CLEAR_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);
    SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, (1 << 1));
    SET_CBUS_REG_MASK(PREG_ETHERNET_ADDR0, 1);
    udelay(100);
    /*reset*/

//  set_gpio_mode(GPIOE_bank_bit16_21(16),GPIOE_bit_bit16_21(16),GPIO_OUTPUT_MODE);
//  set_gpio_val(GPIOE_bank_bit16_21(16),GPIOE_bit_bit16_21(16),0);
//  udelay(100);    //GPIOE_bank_bit16_21(16) reset end;
//  set_gpio_val(GPIOE_bank_bit16_21(16),GPIOE_bit_bit16_21(16),1);
#endif
}

	//add by steven for spdif output.
static void __init spdif_pinmux_init(void)
{
	printk("SPDIF output.\n"); 
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<19)); 
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3,(1<<25)); 
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7,(1<<17)); 
	SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<24));
}

static void __init gpio_init(void)
{
    printk(KERN_INFO "power hold set high!\n");
  //  set_gpio_val(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), 1);
  //  set_gpio_mode(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), GPIO_OUTPUT_MODE);

        // 5V
        set_gpio_mode(GPIOD_bank_bit0_9(9), GPIOD_bit_bit0_9(9), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOD_bank_bit0_9(9), GPIOD_bit_bit0_9(9), 1);

		 // 3.3v
        set_gpio_mode(GPIOAO_bank_bit0_11(2), GPIOAO_bit_bit0_11(2), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(2), GPIOAO_bit_bit0_11(2), 1);

		 // 1.28v
        set_gpio_mode(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), 1);

		 // 1.2v
        set_gpio_mode(GPIOAO_bank_bit0_11(4), GPIOAO_bit_bit0_11(4), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(4), GPIOAO_bit_bit0_11(4), 0);

		 // ddr1.5v
        set_gpio_mode(GPIOAO_bank_bit0_11(5), GPIOAO_bit_bit0_11(5), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(5), GPIOAO_bit_bit0_11(5), 0);
	
	
		 // hdmi power on
//        set_gpio_mode(GPIOD_bank_bit0_9(6), GPIOD_bit_bit0_9(6), GPIO_OUTPUT_MODE);
//        set_gpio_val(GPIOD_bank_bit0_9(6), GPIOD_bit_bit0_9(6), 1);

/*		 // ethernet reset
        set_gpio_mode(GPIOD_bank_bit0_9(7), GPIOD_bit_bit0_9(7), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOD_bank_bit0_9(7), GPIOD_bit_bit0_9(7), 0);
		 mdelay(100);
        set_gpio_val(GPIOD_bank_bit0_9(7), GPIOD_bit_bit0_9(7), 1);

		// usb power
		printk(KERN_INFO "set_vccx2: set usb power on.\n");
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<10));
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<18));
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3,(1<<23));
		set_gpio_mode(GPIOC_bank_bit0_15(8), GPIOC_bit_bit0_15(8), GPIO_OUTPUT_MODE);
		set_gpio_val(GPIOC_bank_bit0_15(8), GPIOC_bit_bit0_15(8), 1);

		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<5));
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<16));
		set_gpio_mode(GPIOC_bank_bit0_15(6), GPIOC_bit_bit0_15(6), GPIO_OUTPUT_MODE);
		set_gpio_val(GPIOC_bank_bit0_15(6), GPIOC_bit_bit0_15(6), 1);

       // LED
        set_gpio_mode(GPIOD_bank_bit0_9(0), GPIOD_bit_bit0_9(0), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOD_bank_bit0_9(0), GPIOD_bit_bit0_9(0), 1);
				
		 CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<17));
		 CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(7<<2)|(1<<11));
        set_gpio_mode(GPIOC_bank_bit0_15(7), GPIOC_bit_bit0_15(7), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOC_bank_bit0_15(7), GPIOC_bit_bit0_15(7), 1);

//        SET_AOBUS_REG_MASK(AO_RTI_PIN_MUX_REG, 13);
//		 CLEAR_AOBUS_REG_MASK(AO_RTI_PIN_MUX_REG, 14);
        set_gpio_mode(GPIOAO_bank_bit0_11(10), GPIOAO_bit_bit0_11(10), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(10), GPIOAO_bit_bit0_11(10), 0);
*/

		// wifi power
	  // CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<6));
	  // CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<15));
    //   set_gpio_mode(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), GPIO_OUTPUT_MODE);
	   //Rony add,if board_ver_id equal 0x02 then inverse wifi power level 20120612
	  // if(board_ver_id == 0x02){
     //  	set_gpio_val(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), 0);
	   //}else
	   //Rony add end
	   //{
     //  	set_gpio_val(GPIOC_bank_bit0_15(5), GPIOC_bit_bit0_15(5), 1);
	   //}
    //VCCx2 power up
    printk(KERN_INFO "set_vccx2 power up\n");
    set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
    set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 0);
}

char board_id[8];
static int __init get_board_id(char* str)
{
	sprintf(board_id,str, sizeof(str));
	return 0;
}

__setup("ID=", get_board_id);

static void __init device_pinmux_init(void )
{
    clearall_pinmux();
    /*other deivce power on*/
    /*GPIOA_200e_bit4..usb/eth/YUV power on*/
    //set_gpio_mode(PREG_EGPIO,1<<4,GPIO_OUTPUT_MODE);
    //set_gpio_val(PREG_EGPIO,1<<4,1);
    //uart_set_pinmux(UART_PORT_A,PINMUX_UART_A);
    //uart_set_pinmux(UART_PORT_B,PINMUX_UART_B);
    /*pinmux of eth*/
    eth_pinmux_init();
    aml_i2c_init();
    printk("SPDIF output.\n");
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<19));
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3,(1<<25));
		CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7,(1<<17));
		SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<24));
//    set_audio_pinmux(AUDIO_OUT_TEST_N);
   // set_audio_pinmux(AUDIO_IN_JTAG);

#ifdef CONFIG_AM_MXL101
	//for mxl101
	if(board_dvb_id == 0){
		//set_mio_mux(3, 0x3F);
		//clear_mio_mux(6, 0x1F<<19);
	
		set_mio_mux(3, 0x3F<<6);
		clear_mio_mux(0, 0xF);
		clear_mio_mux(5, 0x1<<23);
	
		clear_mio_mux(0, 1<<6);
		//pwr pin;
		clear_mio_mux(0, 1<<13);
		clear_mio_mux(1, 1<<8);
		//rst pin;
		clear_mio_mux(0, 1<<28);
		clear_mio_mux(1, 1<<20);
	/*	set_mio_mux(3, 1<<0);
		set_mio_mux(3, 1<<1);
		set_mio_mux(3, 1<<2);
		set_mio_mux(3, 1<<3);
		set_mio_mux(3, 1<<4);
		clear_mio_mux(0, 1<<6);*/
	}
#endif

#ifdef CONFIG_AM_AVL6211

//for avl6211
	printk("CONFIG_AM_AVL6211 set pinmux\n");
	if(board_dvb_id == 1){
		set_mio_mux(3, 0x3F<<6);
	//	clear_mio_mux(0, 1<<4);
		clear_mio_mux(0, 0x7);
	}

#endif
	//add by steven
	spdif_pinmux_init();
	gpio_init();
	
#ifdef CONFIG_AM_ITE9173
//for ite9173
	printk("CONFIG_AM_ITE9173 set pinmux\n");
	set_mio_mux(3, 0x3F<<6);
//	clear_mio_mux(0, 1<<4);
	clear_mio_mux(0, 0x7);
#endif

#if defined(CONFIG_SDIO_DHD_CDC_WIFI_40181_MODULE_MODULE)
    aml_wifi_bcm4018x_init();
#endif


}

static void __init  device_clk_setting(void)
{
    /*Demod CLK for eth and sata*/
    //demod_apll_setting(0,1200*CLK_1M);
    /*eth clk*/
	//Rony add for eth used external clock 20120509
	//int board_hwid = 0;
	
	//board_hwid = = get_hardware_id();	// Rony modify 20120611 move in get_hardware_id
	if((board_ver_id == 0x00) || (board_ver_id == 0x01)){ // internal Rony add id 0x01 used internal clock
    	eth_clk_set(ETH_CLKSRC_MISC_CLK, get_misc_pll_clk(), (50 * CLK_1M), 0);
	}else{ // external
		eth_clk_set(ETH_CLKSRC_EXT_XTAL_CLK, (50 * CLK_1M), (50 * CLK_1M), 1);
	}
	//end Rony add for eth used external clock 20120509
    //eth_clk_set(1, get_system_clk(), (50 * CLK_1M), 0);
}

#ifdef CONFIG_STV_CHECK
extern void machine_halt(void);
extern void machine_power_off(void);
extern void machine_restart(char *cmd);
extern int i2c_eeprom_read(unsigned char *buff, int addr, unsigned len);
extern int i2c_eeprom_write(unsigned char *buff, int addr, unsigned len);
extern int i2c_board_check_simple(void);
void board_check(void)
{
	
	/*
	char bufr;
	static int res = 0;		
	printk("Board checking ... \n");
	
	bufr = 'G';
	res = i2c_eeprom_write(&bufr,0,1);
	bufr = 0;
	mdelay(300);
	res = i2c_eeprom_read(&bufr,0,1);

	if(res>0 ){
		printk("read ok:%c\n",bufr);
		goto CHKOK;
	}	else
		goto CHKNG;
		
	*/
	printk("Board checking ... \n");	
	if(i2c_board_check_simple()==0)
		goto CHKOK;
	else
		goto CHKNG;
	
		
CHKNG:
		printk("Board checking faild! system restarting ...\n");
		machine_halt();
		machine_power_off();
		machine_restart("update_reboot");
		
CHKOK:
		printk("Board checking OK, continue ...\n");
}
#endif

static void disable_unused_model(void)
{
    CLK_GATE_OFF(VIDEO_IN);
    CLK_GATE_OFF(BT656_IN);
  //  CLK_GATE_OFF(ETHERNET);
//    CLK_GATE_OFF(SATA);
//    CLK_GATE_OFF(WIFI);
    video_dac_disable();
 }
static void __init power_hold(void)
{
    printk(KERN_INFO "power hold set high!\n");
  //  set_gpio_val(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), 1);
  //  set_gpio_mode(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), GPIO_OUTPUT_MODE);

        // VCC5V
        set_gpio_mode(GPIOD_bank_bit0_9(9), GPIOD_bit_bit0_9(9), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOD_bank_bit0_9(9), GPIOD_bit_bit0_9(9), 1);
		 // hdmi power on
        set_gpio_mode(GPIOD_bank_bit0_9(6), GPIOD_bit_bit0_9(6), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOD_bank_bit0_9(6), GPIOD_bit_bit0_9(6), 1);

		// MUTE // Rony modify,used mute spk 20120611
       //set_gpio_mode(GPIOX_bank_bit0_31(29), GPIOX_bit_bit0_31(29), GPIO_OUTPUT_MODE);
       //set_gpio_val(GPIOX_bank_bit0_31(29), GPIOX_bit_bit0_31(29), 0);
		set_gpio_mode(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), GPIO_OUTPUT_MODE);
		if(board_ver_id == 0x02){
			set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 1);	 // mute speak
		}else{
			set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 0);	 // mute speak
		}
	   
      // PC Link
//       set_gpio_mode(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), GPIO_OUTPUT_MODE);
//       set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 1);
			 
		// VCC, set to high when suspend 
        set_gpio_mode(GPIOAO_bank_bit0_11(4), GPIOAO_bit_bit0_11(4), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(4), GPIOAO_bit_bit0_11(4), 0);
        set_gpio_mode(GPIOAO_bank_bit0_11(5), GPIOAO_bit_bit0_11(5), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(5), GPIOAO_bit_bit0_11(5), 0);

     // VCCK
        set_gpio_mode(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), 1);
	 // VCCIO
        set_gpio_mode(GPIOAO_bank_bit0_11(2), GPIOAO_bit_bit0_11(2), GPIO_OUTPUT_MODE);
        set_gpio_val(GPIOAO_bank_bit0_11(2), GPIOAO_bit_bit0_11(2), 1);
				
    //VCCx2 power up
    printk(KERN_INFO "set_vccx2 power up\n");
//    set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
//    set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 0);
}
static void __init LED_PWM_REG0_init(void)
{
#if 1 	// PWM_C
    printk(KERN_INFO "LED_PWM_REG0_init.\n");
	 SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2,(1<<2));
    WRITE_CBUS_REG(PWM_PWM_C, (0xff00<<16) |(0xff00<<0));
    WRITE_CBUS_REG(PWM_MISC_REG_CD, (1<<0)	// enable
																			|(0<<4)	// PWM_A_CLK_SEL: 0:XTAL;  1:ddr_pll_clk;  2:clk81;  3:sys_pll_clk;
																			|(0x7f<<8)	// PWM_A_CLK_DIV
																			|(1<<15)	// PWM_A_CLK_EN
																			);
#else
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
#endif
}

// Rony add 20120611 get hardware id 
// GPIOB23,GPIOB22,GPIOB21
void device_hardware_id_init(){	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<4));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3,(1<<12));
	WRITE_CBUS_REG( PREG_PAD_GPIO1_EN_N, READ_CBUS_REG(PREG_PAD_GPIO1_EN_N) | ((1<<21)|(1<<22)|(1<<23)) ); 
	board_ver_id = READ_CBUS_REG(PREG_PAD_GPIO1_I) >> 21;	
	printk("++++++++++++++++++++++++++ hardware id = 0x%x\n",board_ver_id);
}
// Rony add end

// Rony add 20120702 
void device_dvb_id_init(){
	set_gpio_mode(GPIOD_bank_bit0_9(9), GPIOD_bit_bit0_9(9), GPIO_INPUT_MODE);
	board_dvb_id = get_gpio_val(GPIOD_bank_bit0_9(9), GPIOD_bit_bit0_9(9));
	printk(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> dvb id = %s\n",board_dvb_id ? "dvbs" : "dvbt");
}

int get_device_dvb_id(){
	return board_dvb_id;
}
EXPORT_SYMBOL(get_device_dvb_id);
// Rony add end

static __init void m1_init_machine(void)
{
	meson_cache_init();
#ifdef CONFIG_AML_SUSPEND
	extern int (*pm_power_suspend)(void);
	pm_power_suspend = meson_power_suspend;
#endif /*CONFIG_AML_SUSPEND*/
	device_dvb_id_init();// Rony add it device_dvb_id_init
	device_hardware_id_init();// Rony add it check hardware id
    power_hold();
	device_clk_setting();
	device_pinmux_init();
	extern_usb_wifi_power(0);
    LED_PWM_REG0_init();
	platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));

#ifdef CONFIG_USB_DWC_OTG_HCD
    printk("***m1_init_machine: usb set mode.\n");
    set_usb_phy_clk(USB_PHY_CLOCK_SEL_XTAL_DIV2);
	set_usb_phy_id_mode(USB_PHY_PORT_A, USB_PHY_MODE_SW_HOST);
    lm_device_register(&usb_ld_a);
  	set_usb_phy_id_mode(USB_PHY_PORT_B,USB_PHY_MODE_SW_HOST);
    lm_device_register(&usb_ld_b);
#endif
    disable_unused_model();
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
        .virtual    = PAGE_ALIGN(0xdff00000),
        .pfn        = __phys_to_pfn(0x1ff00000),
        .length     = SZ_1M,
        .type       = MT_MEMORY,
    },
#endif
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
    .map_io         = m1_map_io,
    .init_irq       = m1_irq_init,
    .timer          = &meson_sys_timer,
    .init_machine   = m1_init_machine,
    .fixup          = m1_fixup,
    .video_start    = RESERVED_MEM_START,
    .video_end      = RESERVED_MEM_END,
MACHINE_END
