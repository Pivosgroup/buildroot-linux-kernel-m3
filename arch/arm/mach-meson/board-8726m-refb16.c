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
#include <linux/reboot.h>
#include <linux/syscalls.h>

#ifdef CONFIG_AM_UART_WITH_S_CORE 
#include <linux/uart-aml.h>
#endif
#include <mach/card_io.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <mach/clk_set.h>
#include "board-8726m-refb16.h"

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

#ifdef CONFIG_SIX_AXIS_SENSOR_MPU3050
#ifdef CONFIG_MPU_PRE_V340
#include <linux/mpu.h>
#else
#include <linux/mpu_new/mpu.h>
#endif
#endif

#ifdef CONFIG_SN7325
#include <linux/sn7325.h>
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

#ifdef CONFIG_SND_AML_M1_MID_WM8900
#include <sound/wm8900.h>
#endif

#ifdef CONFIG_SND_SOC_RT5621
#include <sound/rt5621.h>
#endif

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE
#include <media/amlogic/aml_camera.h>
#endif

#ifdef CONFIG_BQ27x00_BATTERY
#include <linux/bq27x00_battery.h>
#endif

#ifdef CONFIG_EFUSE
#include <linux/efuse.h>
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
    .irq = -1,  //INT_SAR_ADC
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
    {KEY_LEFTMETA,          "menu", CHAN_4, 0, 60},
    {KEY_PAGEDOWN,          "vol-", CHAN_4, 180, 60},  //0.58v
    {KEY_PAGEUP,            "vol+", CHAN_4, 398, 60},  //1.286v
    {KEY_TAB,               "exit", CHAN_4, 622, 60},
    {KEY_HOME,              "home", CHAN_4, 852, 60},
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
    return 0;
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

#ifdef CONFIG_SN7325

static int sn7325_pwr_rst(void)
{
	
    set_gpio_val(GPIOA_bank_bit0_14(3), GPIOA_bit_bit0_14(3), 0); //low
    set_gpio_mode(GPIOA_bank_bit0_14(3), GPIOA_bit_bit0_14(3), GPIO_OUTPUT_MODE);

    udelay(2); //delay 2us

    set_gpio_val(GPIOA_bank_bit0_14(3), GPIOA_bit_bit0_14(3), 1); //high
    set_gpio_mode(GPIOA_bank_bit0_14(3), GPIOA_bit_bit0_14(3), GPIO_OUTPUT_MODE);
    //end
    //set_gpio_val(GPIOA_bank_bit0_14(7), GPIOA_bit_bit0_14(7), 1); //high  //PWM H:
    //set_gpio_mode(GPIOA_bank_bit0_14(7), GPIOA_bit_bit0_14(7), GPIO_OUTPUT_MODE);
		
    return 0;
}

static struct sn7325_platform_data sn7325_pdata = {
    .pwr_rst = &sn7325_pwr_rst,
};
#endif
#ifdef CONFIG_SIX_AXIS_SENSOR_MPU3050
static struct mpu3050_platform_data mpu3050_data = {
    .int_config = 0x10,
    .orientation = {0,1,0,1,0,0,0,0,-1},
    .level_shifter = 0,
    .accel = {
                .get_slave_descr = mma8451_get_slave_descr,
                .adapt_num = 0, // The i2c bus to which the mpu device is
                // connected
                .bus = EXT_SLAVE_BUS_SECONDARY, //The secondary I2C of MPU
                .address = 0x1c,
                .orientation = {-1,0,0,0,1,0,0,0,-1},
            },
    #ifdef CONFIG_SENSORS_MMC314X
    .compass = {
        .get_slave_descr = mmc314x_get_slave_descr,
        .adapt_num = 0, // The i2c bus to which the compass device is. 
        // It can be difference with mpu
        // connected
        .bus = EXT_SLAVE_BUS_PRIMARY,
        .address = 0x30,
        .orientation = { -1, 0, 0,  0, 1, 0,  0, 0, -1 },
        }  
    #endif           
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
#define USB_A_POW_GPIO          PREG_EGPIO
#define USB_A_POW_GPIO_BIT      3
#define USB_A_POW_GPIO_BIT_ON   1
#define USB_A_POW_GPIO_BIT_OFF  0
    if(is_power_on) {
        printk(KERN_INFO "set usb port power on (board gpio %d)!\n",USB_A_POW_GPIO_BIT);
        set_gpio_mode(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,GPIO_OUTPUT_MODE);
        set_gpio_val(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,USB_A_POW_GPIO_BIT_ON);
    }
    else {
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

void extern_wifi_power(int is_power)
{//GPIOD_20
    if(1 == is_power){
		set_gpio_val(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), 1);
    set_gpio_mode(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), GPIO_OUTPUT_MODE);		 
    }
    else{
		set_gpio_val(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), 0);
    set_gpio_mode(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), GPIO_OUTPUT_MODE);      
    }
    
    return;
}

void sdio_extern_init(void)
{
    extern_wifi_power(1);
}

static struct aml_card_info  amlogic_card_info[] = {
    [0] = {
        .name = "sd_card",
        .work_mode = CARD_HW_MODE,
        .io_pad_type = SDIO_GPIOA_9_14,
        .card_ins_en_reg = EGPIO_GPIOC_ENABLE,
        .card_ins_en_mask = PREG_IO_0_MASK,
        .card_ins_input_reg = EGPIO_GPIOC_INPUT,
        .card_ins_input_mask = PREG_IO_0_MASK,
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
        .name = "sdio_card",                   //WIFI modules
        .work_mode = CARD_HW_MODE,
        .io_pad_type = SDIO_GPIOB_2_7,
        .card_ins_en_reg = 0,
        .card_ins_en_mask = 0,
        .card_ins_input_reg = 0,
        .card_ins_input_mask = 0,
        .card_power_en_reg = EGPIO_GPIOD_ENABLE,
        .card_power_en_mask = PREG_IO_10_MASK,
        .card_power_output_reg = EGPIO_GPIOD_OUTPUT,
        .card_power_output_mask = PREG_IO_10_MASK,
        .card_power_en_lev = 1,
        .card_wp_en_reg = 0,
        .card_wp_en_mask = 0,
        .card_wp_input_reg = 0,
        .card_wp_input_mask = 0,
        .card_extern_init = sdio_extern_init,
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
        [0] =   {
                .start  =   0,
                .end        =   0,
                .flags  =   IORESOURCE_MEM,
        },
};

#ifdef CONFIG_SND_AML_M1_MID_WM8900

static struct platform_device aml_audio={
        .name               = "aml_m1_audio_wm8900",
        .id                     = -1,
        .resource       =   aml_m1_audio_resource,
        .num_resources  =   ARRAY_SIZE(aml_m1_audio_resource),
};


//use LED_CS1 as hp detect pin
#define PWM_TCNT    (600-1)
#define PWM_MAX_VAL (420)
int get_display_mode(void) {
	int fd;
	int ret = 0;
	char mode[8];	
	
  fd = sys_open("/sys/class/display/mode", O_RDWR | O_NDELAY, 0);
  if(fd >= 0) {
  	memset(mode,0,8);
  	sys_read(fd,mode,8);
  	if(strncmp("panel",mode,5))
  		ret = 1;
  	sys_close(fd);
  }

  return ret;
}
int wm8900_is_hp_pluged(void)
{
    int level = 0;
    int cs_no = 0;
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
    cs_no = READ_CBUS_REG(LED_PWM_REG3);
    if(cs_no &(1<<14))
      level |= (1<<0);
    // temp patch to mute speaker when hdmi output
    if(level == 0)
    	if(get_display_mode() != 0)	
    			return 1;
    return (level == 0)?(0):(1); //return 1: hp pluged, 0: hp unpluged.
}

static struct wm8900_platform_data wm8900_pdata = {
    .is_hp_pluged = &wm8900_is_hp_pluged,
};
#endif

#ifdef CONFIG_SND_SOC_RT5621
static struct platform_device aml_audio={
		.name 				= "aml_m1_audio_rt5621",
		.id 					= -1,
		.resource 		=	aml_m1_audio_resource,
		.num_resources	=	ARRAY_SIZE(aml_m1_audio_resource),
};

// use LED_CS1 as detect pin
#define PWM_TCNT        (600-1)
#define PWM_MAX_VAL		(420)
static unsigned int rt5621_is_hp_pluged()
{
		//EIO-->OD5 : 0
		configIO(0, 0);
		setIO_level(0, 0, 5);
		
		int level = 1;
    int cs_no = 0;
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
                                (0 << 0));           // DIMCTL Analog dimmer
    cs_no = READ_CBUS_REG(LED_PWM_REG3);

		if( cs_no & ( 1 << 14 ) )
		level = 0;
		
    return level;	//return 0: hp pluged, 1: hp unpluged.
}
static struct rt5621_platform_data rt5621_pdata = {
    .is_hp_pluged = &rt5621_is_hp_pluged,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_ADS7846)
#define SPI_0       0
#define SPI_1       1
#define SPI_2       2

// GPIOC_8(G20, XPT_CLK)
#define GPIO_SPI_SCK        ((GPIOC_bank_bit0_26(8)<<16) |GPIOC_bit_bit0_26(8)) 
// GPIOC_7(G21, XPT_IN)
#define GPIO_SPI_MOSI       ((GPIOC_bank_bit0_26(7)<<16) |GPIOC_bit_bit0_26(7)) 
// GPIOC_6(G22, XPT_OUT)
#define GPIO_SPI_MISO       ((GPIOC_bank_bit0_26(6)<<16) |GPIOC_bit_bit0_26(6))
// GPIOC_0(J20, XPT_NCS)
#define GPIO_TSC2046_CS ((GPIOC_bank_bit0_26(0)<<16) |GPIOC_bit_bit0_26(0)) 
// GPIOC_4(H20, NPEN_IRQ)
#define GPIO_TSC2046_PENDOWN    ((GPIOC_bank_bit0_26(4)<<16) |GPIOC_bit_bit0_26(4)) 

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
    Bit(s)  Description
    256-105 Unused
    104     JTAG_TDO
    103     JTAG_TDI
    102     JTAG_TMS
    101     JTAG_TCK
    100     gpioA_23
    99      gpioA_24
    98      gpioA_25
    97      gpioA_26
    98-75   gpioE[21:0]
    75-50   gpioD[24:0]
    49-23   gpioC[26:0]
    22-15   gpioB[22;15]
    14-0        gpioA[14:0]
 */

    /* set input mode */
    gpio_direction_input(GPIO_TSC2046_PENDOWN);
    /* set gpio interrupt #0 source=GPIOC_4, and triggered by falling edge(=1) */
    gpio_enable_edge_int(27, 1, 0);

//  // reg2 bit24~26
//  CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_2,
//  (1<<24) | (1<<25) | (1<<26));
//  // reg3 bit5~7,12,16~18,22
//  CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3,
//  (1<<5) | (1<<6) | (1<<7) | (1<<9) | (1<<12) | (1<<16) | (1<<17) | (1<<18) | (1<<22));
//  // reg4 bit26~27
//  CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_4,
//  (1<<26) | (1<<27));
//  // reg9 bit0,4,6~8
//  CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_9,
//  (1<<0) | (1<<4) | (1<<6) | (1<<7) | (1<<8));
//  // reg10 bit0,4,6~8
//  CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_10,
//  (1<<0) | (1<<4) | (1<<6) | (1<<7) | (1<<8));

    return 0;
}
#endif

#ifdef CONFIG_EETI_CAPACITIVE_TOUCHSCREEN
#include <linux/i2c/eeti.h>

//GPIOD_24
#define GPIO_EETI_PENIRQ ((GPIOD_bank_bit2_24(24)<<16) |GPIOD_bit_bit2_24(24)) 
#define GPIO_EETI_RST

static int eeti_init_irq(void)
{
/* memson
    Bit(s)  Description
    256-105 Unused
    104     JTAG_TDO
    103     JTAG_TDI
    102     JTAG_TMS
    101     JTAG_TCK
    100     gpioA_23
    99      gpioA_24
    98      gpioA_25
    97      gpioA_26
    98-76    gpioE[21:0]
    75-50   gpioD[24:0]
    49-23   gpioC[26:0]
    22-15   gpioB[22;15]
    14-0    gpioA[14:0]
 */

    /* set input mode */
    gpio_direction_input(GPIO_EETI_PENIRQ);
    /* set gpio interrupt #0 source=GPIOD_24, and triggered by falling edge(=1) */
    gpio_enable_edge_int(50+24, 1, 0);

    return 0;
}
static int eeti_get_irq_level(void)
{
    return gpio_get_value(GPIO_EETI_PENIRQ);
}

void touch_on(int flag)
{
       printk("enter %s flag=%d \n",__FUNCTION__,flag);
	if(flag){
		set_gpio_val(GPIOD_bank_bit2_24(23), GPIOD_bit_bit2_24(23), 1);
        set_gpio_mode(GPIOD_bank_bit2_24(23), GPIOD_bit_bit2_24(23), GPIO_OUTPUT_MODE);		
        configIO(0, 0);
        setIO_level(0,0, 1);   
	}
	else{
	    set_gpio_val(GPIOD_bank_bit2_24(23), GPIOD_bit_bit2_24(23), 0);
        set_gpio_mode(GPIOD_bank_bit2_24(23), GPIOD_bit_bit2_24(23), GPIO_OUTPUT_MODE);	
        configIO(0, 0);
        setIO_level(0,1, 1);               
	}

}


static struct eeti_platform_data eeti_pdata = {
    .init_irq = &eeti_init_irq,
    .get_irq_level = &eeti_get_irq_level,
    .touch_on =  touch_on,
    .tp_max_width = 32752,
    .tp_max_height = 32752,
    .lcd_max_width = 800,
    .lcd_max_height = 600,
};
#endif

#ifdef CONFIG_PIXCIR_CAPACITIVE_TOUCHSCREEN
#include <linux/i2c/pixcir_i2c_ts.h>
static struct pixcir_i2c_ts_platform_data pixcir_pdata = {
	.gpio_shutdown = (GPIOD_bank_bit2_24(23)<<16) | GPIOD_bit_bit2_24(23),
	.gpio_irq = (GPIOD_bank_bit2_24(24)<<16) | GPIOD_bit_bit2_24(24),
	.xmin = 0,
	.xmax = 1024,
	.ymin = 0,
	.ymax = 768,
  .swap_xy = 1,
  .xpol = 0,
  .ypol = 0,
  .point_id_available = 0,	
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
static  struct platform_device aml_rtc_device = {
            .name            = "aml_rtc",
            .id               = -1,
    };
#endif
#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005)

int gt2005_init(void)
{
    udelay(1000);
    WRITE_CBUS_REG(HHI_ETH_CLK_CNTL,0x30f);// 24M XTAL
    WRITE_CBUS_REG(HHI_DEMOD_PLL_CNTL,0x232);// 24M XTAL
	udelay(1000);
	    
   #ifdef CONFIG_SN7325
	printk( "amlogic camera driver: init CONFIG_SN7325. \n");
	configIO(1, 0);
	setIO_level(1, 1, 4);//PP4 -->1
	msleep(300);
	setIO_level(1, 1, 0);//PP0-->0
	msleep(300);
    #endif

}
#endif /* VIDEO_AMLOGIC_CAPTURE_GT2005 */
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308
int gc0308_init(void)
{
    udelay(1000);
    WRITE_CBUS_REG(HHI_ETH_CLK_CNTL,0x31e);// 24M XTAL
    WRITE_CBUS_REG(HHI_DEMOD_PLL_CNTL,0x232);// 24M XTAL
	udelay(1000);
	
    // set camera power disable
    clear_mio_mux(5, (1<<31));
    set_gpio_val(GPIOE_bank_bit16_21(21), GPIOE_bit_bit16_21(21), 0);
    set_gpio_mode(GPIOE_bank_bit16_21(21), GPIOE_bit_bit16_21(21), GPIO_OUTPUT_MODE);
    msleep(20);
   //pp0
    #ifdef CONFIG_SN7325
	printk( "amlogic camera driver 0308: init CONFIG_SN7325. \n");
	configIO(1, 0);
	setIO_level(1, 1, 0);//30m PWR_Down
	msleep(20);
    #endif	
    
    set_gpio_val(GPIOE_bank_bit16_21(21), GPIOE_bit_biT16_21(21), 1);    // set camera power enable
    msleep(20);
    
    #ifdef CONFIG_SN7325    	
	configIO(1, 0);
	setIO_level(1, 0, 0);//30m PWR_On
	msleep(20);
    #endif
}
#endif
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_SP0838
int sp0838_init(void)
{
    udelay(1000);
 //   WRITE_CBUS_REG(HHI_ETH_CLK_CNTL,0x31e);// 12M XTAL
    WRITE_CBUS_REG(HHI_ETH_CLK_CNTL,0x30f);// 24M XTAL
    WRITE_CBUS_REG(HHI_DEMOD_PLL_CNTL,0x232);// 24M XTAL
	udelay(1000);
	
	
    // set camera power disable
	configIO(1, 0);
	setIO_level(1, 1, 2);//PP2 -->1  SENSORPWR_EN
	msleep(300); 
	configIO(1, 0);
	setIO_level(1, 0, 2);//PP2 -->0  SENSORPWR_EN
	msleep(300);    


	configIO(0, 0);
	setIO_level(0, 0, 2);//RST 0  --//OD2	
  msleep(20);
  configIO(0, 0);
	setIO_level(0, 1, 2);//RST 1 --       //OD2   
	   
    #ifdef CONFIG_SN7325
   #ifdef CONFIG_SN7325
	printk( "amlogic camera driver sp0803: init CONFIG_SN7325. \n");
	configIO(1, 0);       
	setIO_level(1, 0, 0);//PP0-->0    ITU601_PWDN_B    //pin14
	msleep(300);
    #endif
    #endif	
    

    
    #ifdef CONFIG_SN7325    	
	configIO(1, 0);
	setIO_level(1, 1, 6);//PP6 -->0  ITU601_PWDN_F //H--power down  //pin 15
	msleep(300); 
    #endif  
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

    eth_set_pinmux(ETH_BANK0_GPIOC3_C12,ETH_CLK_OUT_GPIOC12_REG3_1, 1);		
}
#endif
#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005)
static int gt2005_v4l2_init(void)
{
	gt2005_init();
}
static int gt2005_v4l2_uninit(void)
{
   #ifdef CONFIG_SN7325
	printk( "amlogic camera driver: uninit gt2005_v4l2_uninit. \n");
	configIO(1, 0);
	setIO_level(1, 1, 4);
	msleep(300);
	setIO_level(1, 1, 0);
    #endif

}

aml_plat_cam_data_t video_gt2005_data = {
	.name="video-gt2005",
	.video_nr=0,
	.device_init= gt2005_v4l2_init,
	.device_uninit=gt2005_v4l2_uninit,
};
#endif
#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308)
static int gc0308_v4l2_init(void)
{
	gc0308_init();
}
static int gc0308_v4l2_uninit(void)
{
	//pp0
#ifdef CONFIG_SN7325
	 printk( "amlogic camera driver: gc0308_v4l2_uninit CONFIG_SN7325. \n");
	 configIO(1, 0);
	 setIO_level(1, 1, 0);//30m PWR_Down
 #endif

    msleep(300);
    set_gpio_val(GPIOE_bank_bit16_21(21), GPIOE_bit_bit16_21(21), 0);
    set_gpio_mode(GPIOE_bank_bit16_21(21), GPIOE_bit_bit16_21(21), GPIO_OUTPUT_MODE);
}
aml_plat_cam_data_t video_gc0308_data = {
	.name="video-gc0308",
	.video_nr=0,//1,
	.device_init= gc0308_v4l2_init,
	.device_uninit=gc0308_v4l2_uninit,
};


#endif

#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_SP0838)
static int sp0838_v4l2_init(void)
{
	sp0838_init();
}
static int sp0838_v4l2_uninit(void)
{
	//pp0
#ifdef CONFIG_SN7325
	 printk( "amlogic camera driver: sp0838_v4l2_uninit CONFIG_SN7325. \n");
	 configIO(1, 0);
	 setIO_level(1, 1, 0);//30m PWR_Down
 #endif

    msleep(300);
    set_gpio_val(GPIOE_bank_bit16_21(21), GPIOE_bit_bit16_21(21), 0);
    set_gpio_mode(GPIOE_bank_bit16_21(21), GPIOE_bit_bit16_21(21), GPIO_OUTPUT_MODE);
}
aml_plat_cam_data_t video_sp0838_data = {
	.name="video-sp0838",
	.video_nr=0,//1,
	.device_init= sp0838_v4l2_init,
	.device_uninit=sp0838_v4l2_uninit,
};


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

#define MAX_GPIO 0
static gpio_data_t gpio_data[MAX_GPIO] = {
	// ----------------------------------- power ctrl ---------------------------------
	{"GPIOC_3 -- AVDD_EN",		GPIOC_bank_bit0_26(3),		GPIOC_bit_bit0_26(3),	GPIO_OUTPUT_MODE, 1, 1},
//	{"GPIOA_7 -- BL_PWM",		GPIOA_bank_bit0_14(7),		GPIOA_bit_bit0_14(7),	GPIO_OUTPUT_MODE, 1, 1},
//	{"GPIOA_6 -- VCCx2_EN",		GPIOA_bank_bit0_14(6),		GPIOA_bit_bit0_14(6),	GPIO_OUTPUT_MODE, 1, 1},
	// ----------------------------------- i2s ---------------------------------
	{"TEST_N -- I2S_DOUT",		GPIOJTAG_bank_bit(16),		GPIOJTAG_bit_bit16(16),	GPIO_OUTPUT_MODE, 1, 1},
	// ----------------------------------- wifi&bt ---------------------------------
	{"GPIOD_12 -- WL_RST_N",	GPIOD_bank_bit2_24(12), 	GPIOD_bit_bit2_24(12), 	GPIO_OUTPUT_MODE, 1, 1},
	{"GPIOD_14 -- BT/GPS_RST_N",GPIOD_bank_bit2_24(14),		GPIOD_bit_bit2_24(14), 	GPIO_OUTPUT_MODE, 1, 1},
	{"GPIOD_18 -- UART_CTS_N",	GPIOD_bank_bit2_24(18), 	GPIOD_bit_bit2_24(18), 	GPIO_OUTPUT_MODE, 1, 1},
	{"GPIOD_21 -- BT/GPS",		GPIOD_bank_bit2_24(21), 	GPIOD_bit_bit2_24(21), 	GPIO_OUTPUT_MODE, 1, 1},
	// ----------------------------------- lcd ---------------------------------
	{"GPIOC_12 -- LCD_U/D",		GPIOC_bank_bit0_26(12), 	GPIOC_bit_bit0_26(12), 	GPIO_OUTPUT_MODE, 1, 1},
//	{"GPIOA_3 -- LCD_PWR_EN",	GPIOA_bank_bit0_14(3),		GPIOA_bit_bit0_14(3),	GPIO_OUTPUT_MODE, 1, 1},
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


#define MAX_PINMUX	12

pinmux_data_t pinmux_data[MAX_PINMUX] = {
	{"HDMI", 	0, (1<<2)|(1<<1)|(1<<0), 						1},
	{"TCON", 	0, (1<<14)|(1<<11), 							1},
	{"I2S_OUT",	0, (1<<18),						 				1},
	{"I2S_CLK",	1, (1<<19)|(1<<15)|(1<<11),		 				1},
	{"SPI",		1, (1<<29)|(1<<27)|(1<<25)|(1<<23),				1},
	{"I2C",		2, (1<<5)|(1<<2),								1},
	{"SD",		2, (1<<15)|(1<<14)|(1<<13)|(1<<12)|(1<<8),		1},
	{"PWM",		2, (1<<31),										1},
	{"UART_A",	3, (1<<24)|(1<23),								0},
	{"RGB",		4, (1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|(1<<0),	1},
	{"UART_B",	5, (1<<24)|(1<23),								0},
	{"REMOTE",	5, (1<<31),										1},
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
        set_gpio_val(GPIOA_bank_bit0_14(6), GPIOA_bit_bit0_14(6), 1);
        set_gpio_mode(GPIOA_bank_bit0_14(6), GPIOA_bit_bit0_14(6), GPIO_OUTPUT_MODE);        
        //set clk for wifi
    }
    else{
        printk(KERN_INFO "set_vccx2 power down\n");        
        set_gpio_val(GPIOA_bank_bit0_14(6), GPIOA_bit_bit0_14(6), 0);
        set_gpio_mode(GPIOA_bank_bit0_14(6), GPIOA_bit_bit0_14(6), GPIO_OUTPUT_MODE);   

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
    .core_voltage_adjust = 8,  //5,8
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

static struct aml_sw_i2c_platform aml_sw_i2c_plat = {
    .sw_pins = {
        .scl_reg_out        = MESON_I2C_PREG_GPIOB_OUTLVL,
        .scl_reg_in     = MESON_I2C_PREG_GPIOB_INLVL,
        .scl_bit            = 2,    /*MESON_I2C_MASTER_A_GPIOB_2_REG*/
        .scl_oe         = MESON_I2C_PREG_GPIOB_OE,
        .sda_reg_out        = MESON_I2C_PREG_GPIOB_OUTLVL,
        .sda_reg_in     = MESON_I2C_PREG_GPIOB_INLVL,
        .sda_bit            = 3,    /*MESON_I2C_MASTER_A_GPIOB_3_BIT*/
        .sda_oe         = MESON_I2C_PREG_GPIOB_OE,
    },  
    .udelay         = 2,
    .timeout            = 100,
};

static struct platform_device aml_sw_i2c_device = {
    .name         = "aml-sw-i2c",
    .id       = -1,
    .dev = {
        .platform_data = &aml_sw_i2c_plat,
    },
};

#endif

#if defined(CONFIG_I2C_AML)
static struct aml_i2c_platform aml_i2c_plat = {
    .wait_count     = 1000000,
    .wait_ack_interval  = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
    .master_no      = AML_I2C_MASTER_B,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_300K, //100k

    .master_b_pinmux = {
        .scl_reg    = MESON_I2C_MASTER_B_GPIOB_0_REG,
        .scl_bit    = MESON_I2C_MASTER_B_GPIOB_0_BIT,
        .sda_reg    = MESON_I2C_MASTER_B_GPIOB_1_REG,
        .sda_bit    = MESON_I2C_MASTER_B_GPIOB_1_BIT,
    }
};

static struct resource aml_i2c_resource[] = {
    [0] = {/*master a*/
        .start =    MESON_I2C_MASTER_A_START,
        .end   =    MESON_I2C_MASTER_A_END,
        .flags =    IORESOURCE_MEM,
    },
    [1] = {/*master b*/
        .start =    MESON_I2C_MASTER_B_START,
        .end   =    MESON_I2C_MASTER_B_END,
        .flags =    IORESOURCE_MEM,
    },
    [2] = {/*slave*/
        .start =    MESON_I2C_SLAVE_START,
        .end   =    MESON_I2C_SLAVE_END,
        .flags =    IORESOURCE_MEM,
    },
};

static struct platform_device aml_i2c_device = {
    .name         = "aml-i2c",
    .id       = -1,
    .num_resources    = ARRAY_SIZE(aml_i2c_resource),
    .resource     = aml_i2c_resource,
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
//  return 0;
//}

static void set_charge(int flags)
{
    //extern io sn7325 pp3
    //low: fast charge high: slow charge
    if(flags == 1){//fast charge
        configIO(1, 0);
        setIO_level(1, 0, 3);        
    }
    else{//slow charge
        configIO(1, 0);
        setIO_level(1, 1, 3);        
    }
        

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

static int get_charge_status(void)
{
    return (READ_CBUS_REG(ASSIST_HW_REV)&(1<<8))? 1:0;
}

static void set_bat_off(void)
{
    //BL_PWM power off
//    set_gpio_val(GPIOA_bank_bit(7), GPIOA_bit_bit0_14(7), 0);
//    set_gpio_mode(GPIOA_bank_bit(7), GPIOA_bit_bit0_14(7), GPIO_OUTPUT_MODE);

    //VCCx2 power down
    set_vccx2(0);
    if(is_ac_connected()){ //AC in after power off press
        kernel_restart("reboot");
    }
    //Power hold down
//    set_gpio_val(GPIOA_bank_bit(8), GPIOA_bit_bit0_14(8), 0);
//    set_gpio_mode(GPIOA_bank_bit(8), GPIOA_bit_bit0_14(8), GPIO_OUTPUT_MODE);

}

static int bat_value_table[36]={
0,  //0    
700,//0
726,//4
732,//10
737,//15
740,//18
743,//20
745,//23
747,//26
748,//29
750,//32
752,//35
753,//37
754,//40
755,//43
757,//46
758,//49
760,//51
762,//54
765,//57
768,//60
771,//63
773,//66
776,//68
779,//71
782,//74
785,//77
788,//80
791,//83
794,//85
796,//88
799,//91
803,//95
806,//97
814,//100
814 //100
};

static int bat_charge_value_table[36]={
0,  //0    
732,//0
770,//4
780,//10
782,//15
783,//18
785,//20
786,//23
788,//26
789,//29
791,//32
793,//35
794,//37
796,//40
797,//43
799,//46
800,//49
802,//51
804,//54
805,//57
806,//60
807,//63
808,//66
809,//68
804,//71
806,//74
809,//77
813,//80
815,//83
818,//85
820,//88
823,//91
826,//95
829,//97
830,//100
830 //100
};

static int bat_level_table[36]={
0,
0,
4,
10,
15,
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

static struct aml_power_pdata power_pdata = {
	.is_ac_online	= is_ac_connected,
	//.is_usb_online	= is_usb_connected,
	.set_charge = set_charge,
	.get_bat_vol = get_bat_vol,
	.get_charge_status = get_charge_status,
	.set_bat_off = set_bat_off,
	.bat_value_table = bat_value_table,
	.bat_charge_value_table = bat_charge_value_table,
	.bat_level_table = bat_level_table,
	.bat_table_len = 36,		
	.is_support_usb_charging = 0,
	//.supplied_to = supplicants,
	//.num_supplicants = ARRAY_SIZE(supplicants),
};

static struct platform_device power_dev = {
    .name       = "aml-power",
    .id     = -1,
    .dev = {
        .platform_data  = &power_pdata,
    },
};
#endif
#ifdef CONFIG_BQ27x00_BATTERY
static int is_ac_connected(void)
{
	return (READ_CBUS_REG(ASSIST_HW_REV)&(1<<9))? 1:0;//GP_INPUT1
}

static int get_charge_status()
{
    return (READ_CBUS_REG(ASSIST_HW_REV)&(1<<8))? 1:0;//GP_INPUT0
}

static void set_charge(int flags)
{
	//GPIOD_22 low: fast charge high: slow charge
   // CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<18));
    if(flags == 1)
        set_gpio_val(GPIOD_bank_bit2_24(22), GPIOD_bit_bit2_24(22), 0); //fast charge
    else
        set_gpio_val(GPIOD_bank_bit2_24(22), GPIOD_bit_bit2_24(22), 1); //slow charge
    set_gpio_mode(GPIOD_bank_bit2_24(22), GPIOD_bit_bit2_24(22), GPIO_OUTPUT_MODE);
}

static void set_bat_off(void)
{
    if(is_ac_connected()){ //AC in after power off press
        kernel_restart("reboot");
    }
//    set_gpio_val(GPIOA_bank_bit(8), GPIOA_bit_bit0_14(8), 0);
//    set_gpio_mode(GPIOA_bank_bit(8), GPIOA_bit_bit0_14(8), GPIO_OUTPUT_MODE);

}

static struct bq27x00_battery_pdata bq27x00_pdata = {
	.is_ac_online	= is_ac_connected,
	.get_charge_status = get_charge_status,	
	.set_charge = set_charge,
	.set_bat_off = set_bat_off,
    .chip = 1,
};
#endif

#define PINMUX_UART_A   UART_A_GPIO_D21_D22
#define PINMUX_UART_B   UART_B_GPIO_E18_E19

#if defined(CONFIG_AM_UART_WITH_S_CORE)

#if defined(CONFIG_AM_UART0_SET_PORT_A)
#define UART_0_PORT     UART_A
#define UART_1_PORT     UART_B
#elif defined(CONFIG_AM_UART0_SET_PORT_B)
#define UART_0_PORT     UART_B
#define UART_1_PORT     UART_A
#endif

static struct aml_uart_platform aml_uart_plat = {
    .uart_line[0]       =   UART_0_PORT,
    .uart_line[1]       =   UART_1_PORT
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
{
	return true;
}

static struct efuse_platform_data aml_efuse_plat = {
    .pos = 337,
    .count = 20,
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
/*static struct mtd_partition partition_info[] = 
{
    {
		.name = "U-BOOT",
        .offset = 0,
        .size=4*1024*1024,
    //  .set_flags=0,
    //  .dual_partnum=0,
    },
    {
		.name = "Boot Para",
        .offset = 4*1024*1024,
        .size=4*1024*1024,
    //  .set_flags=0,
    //  .dual_partnum=0,
    },
    {
		.name = "Kernel",
        .offset = 8*1024*1024,
        .size=4*1024*1024,
    //  .set_flags=0,
    //  .dual_partnum=0,
    },
    {
		.name = "YAFFS2",
		.offset=MTDPART_OFS_APPEND,
        .size = MTDPART_SIZ_FULL,
    //  .set_flags=0,
    //  .dual_partnum=0,
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
    .bch_mode=1,            //BCH8
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
    .spare_size= 224,       //for micron ABA 4GB
    .erase_size=1024*1024,
    .bch_mode=    3,        //BCH16
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
		.name = "logo",
		.offset = 72*SZ_1M,
		.size = 16*SZ_1M,
	},
	{
		.name = "aml_logo",
		.offset = 88*SZ_1M,
		.size = 16*SZ_1M,
	},
	{
		.name = "recovery",
		.offset = 104*SZ_1M,
		.size = 32*SZ_1M,
	},
	{
		.name = "boot",       //kernel
		.offset = 136*SZ_1M,
		.size = 32*SZ_1M,
	},
	{
		.name = "system",     //rootfs
		.offset = 168*SZ_1M,
		.size = 256*SZ_1M,
	},
	{
		.name = "cache",
		.offset = 424*SZ_1M,
		.size = 128*SZ_1M,
	},
	{
		.name = "userdata",
		.offset = 552*SZ_1M,
		.size = 128*SZ_1M,
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
#include <linux/aml_bl.h>

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
    printk("\n\nBacklight init.\n\n");
}
static unsigned bl_level;
static unsigned aml_8726m_get_bl_level(void)
{
    return bl_level;
}
static void aml_8726m_set_bl_level(unsigned level)
{
    unsigned cs_level;
    if (level < 10)
    {
        cs_level = 15;
    }
    else if (level < 30)
    {
        cs_level = 14;
    }
    else if (level >=30 && level < 256)
    {
        cs_level = 13-((level - 30)/28);
    }
    else
        cs_level = 3;

    WRITE_CBUS_REG_BITS(VGHL_PWM_REG0, cs_level, 0, 4);
}

static void aml_8726m_power_on_bl(void)
{
    printk("backlight on\n");
        //BL_PWM -> GPIOA_7: 1 Pull high, For En_5V
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<30));
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_12, (1<<6));
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<8));
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<28));
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<21));
//	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_9, (1<<23));    

    set_gpio_val(GPIOA_bank_bit0_14(3), GPIOA_bit_bit0_14(3), 0); //low
    set_gpio_mode(GPIOA_bank_bit0_14(3), GPIOA_bit_bit0_14(3), GPIO_OUTPUT_MODE);

    udelay(2); //delay 2us

    //end
    set_gpio_val(GPIOA_bank_bit0_14(3), GPIOA_bit_bit0_14(3), 1); //high
    set_gpio_mode(GPIOA_bank_bit0_14(3), GPIOA_bit_bit0_14(3), GPIO_OUTPUT_MODE);
      
   set_gpio_val(GPIOA_bank_bit0_14(7), GPIOA_bit_bit0_14(7), 1);
   set_gpio_mode(GPIOA_bank_bit0_14(7), GPIOA_bit_bit0_14(7), GPIO_OUTPUT_MODE);


#ifdef CONFIG_SN7325
    configIO(0, 0);
    setIO_level(0, 0, 7);   //OD7 bl_en 
    udelay(2000); //delay 2us
    configIO(0, 0);
    setIO_level(0, 1, 7);   //OD7 bl_en  
    
    configIO(0, 0);
    setIO_level(0, 0, 0);   //OD0 LCD_PWR_EN 
#endif
}

static void aml_8726m_power_off_bl(void)
{
    printk("backlight off\n");
    //BL_PWM -> GPIOA_7: 0
    set_gpio_val(GPIOA_bank_bit0_14(7), GPIOA_bit_bit0_14(7), 0);
    set_gpio_mode(GPIOA_bank_bit0_14(7), GPIOA_bit_bit0_14(7), GPIO_OUTPUT_MODE);
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

#ifdef CONFIG_BT_DEVICE
#include <linux/bt-device.h>

static struct platform_device bt_device = {
	.name             = "bt-dev",
	.id               = -1,
};

static void bt_device_init(void)
{
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_12, (1<<29));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_12, (1<<22));
	
	//CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<19));
	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<20));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<17));
	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<14));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<12));
	
	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<4));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<13));
	
	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<12));
	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<21));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<28));
	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_12, (1<<23));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<14));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<17));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<12));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<5));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<27));
	
	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_12, (1<<27));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<18));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<12));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<9));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<23));
	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_12, (1<<26));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<17));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<17));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<12));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<8));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<24));
	
	/* WIFI/BT_EN */
		set_gpio_val(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), 1);
    set_gpio_mode(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), GPIO_OUTPUT_MODE);		

	
	/* BT_RST_N */
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_EN_N, (1<<12));
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_O, (1<<12));	
	msleep(200);	
	SET_CBUS_REG_MASK(PREG_GGPIO_O, (1<<12));	
	
	/* UART_TX */

	configIO(1, 0);
	setIO_level(1, 0, 4);//PP4 -->0   for BT UART
	
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_EN_N, (1<<19));
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_O, (1<<19));	
	
	/* UART_CTS_N */
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_EN_N, (1<<16));
    CLEAR_CBUS_REG_MASK(PREG_GGPIO_O, (1<<16));
		
	/* BG wakeup high 
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_EN_N, (1<<14));
	SET_CBUS_REG_MASK(PREG_GGPIO_O, (1<<14));*/
}

static void bt_device_on(void)
{
    /* BT_RST_N */
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_EN_N, (1<<12));
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_O, (1<<12));	
	msleep(200);	
	SET_CBUS_REG_MASK(PREG_GGPIO_O, (1<<12));	
}

static void bt_device_off(void)
{
    /* BT_RST_N */
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_EN_N, (1<<12));
	CLEAR_CBUS_REG_MASK(PREG_GGPIO_O, (1<<12));	
	msleep(200);	
	//CLEAR_CBUS_REG_MASK(PREG_GGPIO_O, (1<<12));	
}

struct bt_dev_data bt_dev = {
    .bt_dev_init    = bt_device_init,
    .bt_dev_on      = bt_device_on,
    .bt_dev_off     = bt_device_off,
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
    #endif
    #if defined(CONFIG_TVIN_BT656IN)
		&bt656in_device,
    #endif
    #if defined(CONFIG_AML_AUDIO_DSP)
        &audiodsp_device,
    #endif
	#if defined(CONFIG_SND_AML_M1_MID_WM8900) || defined(CONFIG_SND_SOC_RT5621)
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
    #ifdef CONFIG_AMLOGIC_VIDEOIN_MANAGER
		&vm_device,
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
    #ifdef CONFIG_EFUSE
	&aml_efuse_device,
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

#ifdef CONFIG_SND_AML_M1_MID_WM8900
    {
        I2C_BOARD_INFO("wm8900", 0x1A),
        .platform_data = (void *)&wm8900_pdata,
    },
#endif

#ifdef CONFIG_SND_SOC_RT5621
	{
		I2C_BOARD_INFO(RT5621_I2C_NAME, RT5621_I2C_ADDR),
		.platform_data = (void *)&rt5621_pdata,
	},
#endif

#ifdef CONFIG_SN7325
    {
        I2C_BOARD_INFO("sn7325", 0x59),
        .platform_data = (void *)&sn7325_pdata,
    },
#endif

#ifdef CONFIG_EETI_CAPACITIVE_TOUCHSCREEN
    {
        I2C_BOARD_INFO("eeti", 0x04),
        .irq = INT_GPIO_0,
        .platform_data = (void *)&eeti_pdata,
    },
#endif

#ifdef CONFIG_PIXCIR_CAPACITIVE_TOUCHSCREEN
    {
        I2C_BOARD_INFO("pixcir168", 0x5c),
        .irq = INT_GPIO_0,
        .platform_data = (void *)&pixcir_pdata,
    },
#endif

#ifdef CONFIG_SIX_AXIS_SENSOR_MPU3050
    {
        I2C_BOARD_INFO("mpu3050", 0x68),
        .platform_data = (void *)&mpu3050_data,
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
        /*ov5640 i2c address is 0x78*/
    	I2C_BOARD_INFO("gt2005_i2c",  0x78 >> 1 ),
    	.platform_data = (void *)&video_gt2005_data
	},
#endif
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_OV5640
	{
        /*ov5640 i2c address is 0x78*/
		I2C_BOARD_INFO("ov5640_i2c",  0x78 >> 1),
		.platform_data = (void *)&video_ov5640_data,
	},
#endif
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_SP0838

	{
        /*sp0838 i2c address is 0x30/0x31*/
		I2C_BOARD_INFO("sp0838_i2c",  0x30 >> 1),
		.platform_data = (void *)&video_sp0838_data,
	},
#endif
#ifdef CONFIG_BQ27x00_BATTERY
    {
        I2C_BOARD_INFO("bq27200", 0x55),
        .platform_data = (void *)&bq27x00_pdata,
    },
#endif

};

static int __init aml_i2c_init(void)
{

    i2c_register_board_info(0, aml_i2c_bus_info,
        ARRAY_SIZE(aml_i2c_bus_info));
    return 0;
}

#if defined(CONFIG_TVIN_BT656IN)
static void __init bt656in_pinmux_init(void)
{
    set_mio_mux(3, 0xf000);   //mask--mux gpio_c3 to bt656 clk;  mux gpioc[4:11] to be bt656 dt_in
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, 0x0f000000);
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, 0x01be07fc);
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, 0x0c000000);
}


#endif
static void __init eth_pinmux_init(void)
{
    eth_set_pinmux(ETH_BANK2_GPIOD15_D23,ETH_CLK_OUT_GPIOD24_REG5_1,0);
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
#if 0
//  set_gpio_mode(GPIOE_bank_bit16_21(16),GPIOE_bit_bit16_21(16),GPIO_OUTPUT_MODE);
//  set_gpio_val(GPIOE_bank_bit16_21(16),GPIOE_bit_bit16_21(16),0);
//  udelay(100);    //GPIOE_bank_bit16_21(16) reset end;
//  set_gpio_val(GPIOE_bank_bit16_21(16),GPIOE_bit_bit16_21(16),1);
#endif
    aml_i2c_init();
}

static int board_ver = 0; //2

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
    //eth_pinmux_init();
    aml_i2c_init();
#if defined(CONFIG_TVIN_BT656IN)
    bt656in_pinmux_init();
#endif
	if(board_ver == 0){
		#ifdef CONFIG_SND_SOC_RT5621
			set_audio_pinmux(AUDIO_OUT_TEST_N);
		#else
			set_audio_pinmux(AUDIO_OUT_JTAG);
		#endif
    }
	else{
        set_audio_pinmux(AUDIO_OUT_TEST_N);
        set_audio_pinmux(AUDIO_IN_JTAG);
	}
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
//    set_gpio_val(GPIOA_bank_bit(8), GPIOA_bit_bit0_14(8), 1);
//    set_gpio_mode(GPIOA_bank_bit(8), GPIOA_bit_bit0_14(8), GPIO_OUTPUT_MODE);
    
    //VCCx2 power up
    set_vccx2(1);
    
    // set cpu power  to 1.26V   
    //wm8900_is_hp_pluged(); 
    WRITE_CBUS_REG_BITS(LED_PWM_REG0,1,0,4); 
}

static __init void m1_init_machine(void)
{
    meson_cache_init();

    power_hold();
    device_clk_setting();
    device_pinmux_init();
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE
    camera_power_on_init();
#endif
    platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));

#ifdef CONFIG_USB_DWC_OTG_HCD
    set_usb_phy_clk(USB_PHY_CLOCK_SEL_XTAL_DIV2);
    lm_device_register(&usb_ld_a);
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
        .virtual    = PAGE_ALIGN(__phys_to_virt(RESERVED_MEM_START)),
        .pfn        = __phys_to_pfn(RESERVED_MEM_START),
        .length     = RESERVED_MEM_END-RESERVED_MEM_START+1,
        .type       = MT_DEVICE,
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
