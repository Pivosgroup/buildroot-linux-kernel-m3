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

#ifdef CONFIG_AM_UART_WITH_S_CORE 
#include <linux/uart-aml.h>
#endif
#include <mach/card_io.h>
#include <mach/pinmux.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <mach/clk_set.h>
#include "board-m3-reff10.h"


#ifdef CONFIG_ANDROID_PMEM
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/android_pmem.h>
#endif


#ifdef CONFIG_SENSORS_MMC328X
#include <linux/mmc328x.h>
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

#ifdef CONFIG_TOUCH_KEY_PAD_IT7230
#include <linux/i2c/it7230.h>
#endif

#ifdef CONFIG_AMLOGIC_PM
#include <linux/power_supply.h>
#include <linux/aml_power.h>
#endif

#ifdef CONFIG_AMLOGIC_MODEM
#include <linux/power_supply.h>
#include <linux/aml_modem.h>
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

#ifdef CONFIG_AR1520_GPS
#include <linux/ar1520.h>
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
    {KEY_MENU,          "menu", CHAN_4, 0, 60},
    {KEY_VOLUMEDOWN,    "vol-", CHAN_4, 140, 60},
    {KEY_VOLUMEUP,      "vol+", CHAN_4, 266, 60},
    {KEY_BACK,          "exit", CHAN_4, 386, 60},
    {KEY_HOME,          "home", CHAN_4, 508, 60},
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

#ifdef CONFIG_SN7325

static int sn7325_pwr_rst(void)
{
    //reset
    //set_gpio_val(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), 0); //low
    //set_gpio_mode(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), GPIO_OUTPUT_MODE);

    udelay(2); //delay 2us

    //set_gpio_val(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), 1); //high
    //set_gpio_mode(GPIOD_bank_bit2_24(20), GPIOD_bit_bit2_24(20), GPIO_OUTPUT_MODE);
    //end

    return 0;
}

static struct sn7325_platform_data sn7325_pdata = {
    .pwr_rst = &sn7325_pwr_rst,
};
#endif

#ifdef CONFIG_SIX_AXIS_SENSOR_MPU3050
#define GPIO_mpu3050_PENIRQ ((GPIOA_bank_bit0_27(14)<<16) | GPIOA_bit_bit0_27(14))
#define MPU3050_IRQ  INT_GPIO_1
static int mpu3050_init_irq(void)
{
    /* set input mode */
    gpio_direction_input(GPIO_mpu3050_PENIRQ);
    /* map GPIO_mpu3050_PENIRQ map to gpio interrupt, and triggered by rising edge(=0) */
    gpio_enable_edge_int(gpio_to_idx(GPIO_mpu3050_PENIRQ), 0, MPU3050_IRQ-INT_GPIO_0);
    return 0;
}

static struct mpu3050_platform_data mpu3050_data = {
    .int_config = 0x10,
    .orientation = {0,-1,0,-1,0,0,0,0,-1},
    .level_shifter = 0,
    .accel = {
                .get_slave_descr = get_accel_slave_descr,
                .adapt_num = 1, // The i2c bus to which the mpu device is
                // connected
                .bus = EXT_SLAVE_BUS_SECONDARY, //The secondary I2C of MPU
                .address = 0x1c,
                .orientation = {0,1,0,1,0,0,0,0,-1},
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


#ifdef CONFIG_TOUCH_KEY_PAD_IT7230
#include <linux/input.h>
#define GPIO_IT7230_ATTN ((GPIOA_bank_bit0_27(17)<<16) | GPIOA_bit_bit0_27(17))
#define IT7230_INT INT_GPIO_1

static int it7230_init_irq(void)
{
    /* set input mode */
    gpio_direction_input(GPIO_IT7230_ATTN);
    /* GPIO_IT7230_ATTN connect to gpio interrupt 1,  and triggered by falling edge(=1) */
    gpio_enable_edge_int(gpio_to_idx(GPIO_IT7230_ATTN), 1, IT7230_INT-INT_GPIO_0);
    return 0;
}

static int it7230_get_irq_level(void)
{
    return gpio_get_value(GPIO_IT7230_ATTN);
}

static struct cap_key it7230_keys[] = {
    { KEY_COMPOSE,         0x0008, "zoom"},
    { KEY_HOME,         0x0001, "home"},
    { KEY_LEFTMETA,     0x0002, "menu"},
    { KEY_TAB,          0x0004, "exit"},
};

static struct it7230_platform_data it7230_pdata = {
    .init_irq = it7230_init_irq,
    .get_irq_level = it7230_get_irq_level,
    .key = it7230_keys,
    .key_num = ARRAY_SIZE(it7230_keys),
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
    /* Hide uboot partition
            {
                    .name = "uboot",
                    .offset = 0,
                    .size = 0x3e000,
            },
    //*/
    {
        .name = "ubootenv",
        .offset = 0x80000,
        .size = 0x2000,
    },
    /* Hide recovery partition
            {
                    .name = "recovery",
                    .offset = 0x40000,
                    .size = 0x1c0000,
            },
    //*/
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

void extern_wifi_power(int is_power)
{
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<11));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<18));
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<8));
	if(is_power)
		SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
	else
		CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
}

void sdio_extern_init(void)
{
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
    if(flag){
		set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 0);	 // mute speak
		set_gpio_mode(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), GPIO_OUTPUT_MODE);
	}else{
		set_gpio_val(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), 1);	 // unmute speak
		set_gpio_mode(GPIOC_bank_bit0_15(4), GPIOC_bit_bit0_15(4), GPIO_OUTPUT_MODE);
	}
}

#endif

#ifdef CONFIG_ITK_CAPACITIVE_TOUCHSCREEN
#include <linux/i2c/itk.h>
#define GPIO_ITK_PENIRQ ((GPIOA_bank_bit0_27(16)<<16) | GPIOA_bit_bit0_27(16))
#define GPIO_ITK_RST ((GPIOC_bank_bit0_15(3)<<16) | GPIOC_bit_bit0_15(3))
#define ITK_INT INT_GPIO_0
static int itk_init_irq(void)
{
    /* set input mode */
    gpio_direction_input(GPIO_ITK_PENIRQ);
    /* set gpio interrupt #0 source=GPIOD_24, and triggered by falling edge(=1) */
    gpio_enable_edge_int(gpio_to_idx(GPIO_ITK_PENIRQ), 1, ITK_INT-INT_GPIO_0);

    return 0;
}
static int itk_get_irq_level(void)
{
    return gpio_get_value(GPIO_ITK_PENIRQ);
}

void touch_on(int flag)
{
	printk("enter %s flag=%d \n",__FUNCTION__,flag);
	if(flag)
		gpio_direction_output(GPIO_ITK_RST, 1);
	else
		gpio_direction_output(GPIO_ITK_RST, 0);
}

static struct itk_platform_data itk_pdata = {
    .init_irq = &itk_init_irq,
    .get_irq_level = &itk_get_irq_level,
    .touch_on =  touch_on,
    .tp_max_width = 3328,
    .tp_max_height = 2432,
    .lcd_max_width = 800,
    .lcd_max_height = 600,
    .xpol = 1,
    .ypol = 1,
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

#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308
int gc0308_init(void)
{
//    udelay(1000);
//    WRITE_CBUS_REG(HHI_ETH_CLK_CNTL,0x30f);// 24M XTAL
//    WRITE_CBUS_REG(HHI_DEMOD_PLL_CNTL,0x232);// 24M XTAL
//	udelay(1000);
	
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
static int gc0308_v4l2_init(void)
{
	gc0308_init();
}
static int gc0308_v4l2_uninit(void)
{

	printk( "amlogic camera driver: gc0308_v4l2_uninit CONFIG_SN7325. \n");
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

aml_plat_cam_data_t video_gc0308_data = {
	.name="video-gc0308",
	.video_nr=0,//1,
	.device_init= gc0308_v4l2_init,
	.device_uninit=gc0308_v4l2_uninit,
	.early_suspend = gc0308_v4l2_early_suspend,
	.late_resume = gc0308_v4l2_late_resume,
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

#ifdef CONFIG_AMLOGIC_MODEM
static int modem_enable(void)
{
#ifdef CONFIG_SN7325
    printk(" dgt added modem_enable for Mf210 ! \n ");
    //enable
    configIO(1, 0);
    setIO_level(1, 1, 4);//PP4

    return 0 ;
#endif
}

static int modem_disable(void)
{
#ifdef CONFIG_SN7325
    printk(" dgt added modem_enable for Mf210 ! \n ");
    //enable
    configIO(1, 0);
    setIO_level(1, 0, 4);//PP4

    return 0 ;
#endif
}

static int modem_reset(void)
{
#ifdef CONFIG_SN7325
    //reset
    printk(" start modem Mf210 reset !\n ");
    configIO(1, 0);
    setIO_level(1, 0, 5);//PP5

    mdelay(200);

     configIO(1, 0);
    setIO_level(1, 1, 5);//PP5

    printk(" complete modem Mf210 reset !\n ");

    return 0 ;
#endif
}

static int modem_power_on(void)
{
#ifdef CONFIG_SN7325
    printk(" dgt added modem_power_on for Mf210 ! \n ");
    //power on
    configIO(1, 0);
    setIO_level(1, 1, 7);//PP7

    return 0 ;
#endif
}

static int modem_power_off(void)
{

#ifdef CONFIG_SN7325
    printk(" dgt added modem_power_off for Mf210 !\n ");
    configIO(1, 0);
    setIO_level(1, 0, 7);//PP7

    return 0 ;
#endif
}

static struct aml_modem_pdata modem_pdata = {
    .power_on = modem_power_on ,
    .power_off = modem_power_off ,
    .enable = modem_enable,
    .disable = modem_disable,
    .reset = modem_reset,
};

static struct platform_device modem_dev = {
    .name       = "aml-modem",
    .id     = -1,
    .dev = {
        .platform_data  = &modem_pdata,
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
    //GPIOD_22 low: fast charge high: slow charge
   // CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<18));
   // if(flags == 1)
        //set_gpio_val(GPIOD_bank_bit2_24(22), GPIOD_bit_bit2_24(22), 0); //fast charge
    //else
        //set_gpio_val(GPIOD_bank_bit2_24(22), GPIOD_bit_bit2_24(22), 1); //slow charge
    //set_gpio_mode(GPIOD_bank_bit2_24(22), GPIOD_bit_bit2_24(22), GPIO_OUTPUT_MODE);
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

static void power_off(void)
{
    //Power hold down
    set_gpio_val(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), 0);
    set_gpio_mode(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), GPIO_OUTPUT_MODE);
}

static void set_bat_off(void)
{
    //BL_PWM power off
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<31));
    CLEAR_CBUS_REG_MASK(PWM_MISC_REG_AB, (1 << 0));
    //set_gpio_val(GPIOA_bank_bit(7), GPIOA_bit_bit0_14(7), 0);
    //set_gpio_mode(GPIOA_bank_bit(7), GPIOA_bit_bit0_14(7), GPIO_OUTPUT_MODE);

    //VCCx2 power down
#if defined(CONFIG_SUSPEND)
    set_vccx2(0);
#endif
}

static int bat_value_table[37]={
0,  //0    
737,//0
742,//4
750,//10
752,//15
753,//16
754,//18
755,//20
756,//23
757,//26
758,//29
760,//32
761,//35
762,//37
763,//40
766,//43
768,//46
770,//49
772,//51
775,//54
778,//57
781,//60
786,//63
788,//66
791,//68
795,//71
798,//74
800,//77
806,//80
810,//83
812,//85
817,//88
823,//91
828,//95
832,//97
835,//100
835 //100
};

static int bat_charge_value_table[37]={
0,  //0    
766,//0
773,//4
779,//10
780,//15
781,//16
782,//18
783,//20
784,//23
785,//26
786,//29
787,//32
788,//35
789,//37
790,//40
791,//43
792,//46
794,//49
796,//51
798,//54
802,//57
804,//60
807,//63
809,//66
810,//68
813,//71
815,//74
818,//77
820,//80
823,//83
825,//85
828,//88
831,//91
836,//95
839,//97
842,//100
842 //100
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
	.bat_table_len = 37,		
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
}

static void set_bat_off(void)
{

}

static struct bq27x00_battery_pdata bq27x00_pdata = {
	.is_ac_online	= is_ac_connected,
	.get_charge_status = get_charge_status,	
	.set_charge = set_charge,
	.set_bat_off = set_bat_off,
    .chip = 1,
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

#ifdef CONFIG_AR1520_GPS
static void ar1520_power_on(void)
{
#ifdef CONFIG_SN7325
	printk("power on gps\n");
	configIO(0, 0);
	setIO_level(0, 1, 7);//OD7
#endif	
}

static void ar1520_power_off(void)
{
#ifdef CONFIG_SN7325
	printk("power off gps\n");
	configIO(0, 0);
	setIO_level(0, 0, 7);//OD7
#endif	
}

static void ar1520_reset(void)
{
#ifdef CONFIG_SN7325
	printk("reset gps\n");
	msleep(200);
	configIO(0, 0);
	setIO_level(0, 0, 4);//OD4
	msleep(200);
	configIO(0, 0);
	setIO_level(0, 1, 4);//OD4
	msleep(200);
	configIO(0, 0);
	setIO_level(0, 0, 4);//OD4
	msleep(200);
	configIO(0, 0);
	setIO_level(0, 1, 4);//OD4
#endif	
}

static struct ar1520_platform_data aml_ar1520_plat = {
	.power_on = ar1520_power_on,
	.power_off = ar1520_power_off,
	.reset = ar1520_reset,
};

static struct platform_device aml_ar1520_device = {	
    .name         = "ar1520_gps",  
    .id       = -1, 
    .dev = {        
                .platform_data = &aml_ar1520_plat,  
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

#ifdef CONFIG_PMU_ACT8942
#include <linux/act8942.h>  


static void power_off(void)
{
    //Power hold down
    set_gpio_val(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), 0);
    set_gpio_mode(GPIOAO_bank_bit0_11(6), GPIOAO_bit_bit0_11(6), GPIO_OUTPUT_MODE);
}



/*
 *	DC_DET(GPIOA_20)	enable internal pullup
 *		High:		Disconnect
 *		Low:		Connect
 */
static inline int is_ac_online(void)
{
	int val;
	
	SET_CBUS_REG_MASK(PAD_PULL_UP_REG0, (1<<20));	//enable internal pullup
	set_gpio_mode(GPIOA_bank_bit0_27(20), GPIOA_bit_bit0_27(20), GPIO_INPUT_MODE);
	val = get_gpio_val(GPIOA_bank_bit0_27(20), GPIOA_bit_bit0_27(20));
	
	logd("%s: get from gpio is %d.\n", __FUNCTION__, val);
	
	return !val;
}

//temporary
static inline int is_usb_online(void)
{
	u8 val;

	return 0;
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

	logd("%s: get from gpio is %d.\n", __FUNCTION__, val);
	
	return val;
}

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
737,//0
742,//4
750,//10
752,//15
753,//16
754,//18
755,//20
756,//23
757,//26
758,//29
760,//32
761,//35
762,//37
763,//40
766,//43
768,//46
770,//49
772,//51
775,//54
778,//57
781,//60
786,//63
788,//66
791,//68
795,//71
798,//74
800,//77
806,//80
810,//83
812,//85
817,//88
823,//91
828,//95
832,//97
835,//100
835 //100
};

static int bat_charge_value_table[37]={
0,  //0    
766,//0
773,//4
779,//10
780,//15
781,//16
782,//18
783,//20
784,//23
785,//26
786,//29
787,//32
788,//35
789,//37
790,//40
791,//43
792,//46
794,//49
796,//51
798,//54
802,//57
804,//60
807,//63
809,//66
810,//68
813,//71
815,//74
818,//77
820,//80
823,//83
825,//85
828,//88
831,//91
836,//95
839,//97
842,//100
842 //100
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
		if ((adc_vaule > adc_table[i]) && (adc_vaule <= adc_table[i+1]))
			break;
	}
	//printk("%s: adc_vaule=%d, i=%d, per_table[i]=%d \n", __FUNCTION__, adc_vaule, i, per_table[i]);

	return per_table[i];
}

static int act8942_measure_capacity_charging(void)
{
	int adc = get_bat_adc_value();
	int table_size = sizeof(bat_charge_value_table)/sizeof(bat_charge_value_table[0]);
	
	return get_bat_percentage(adc, bat_charge_value_table, bat_level_table, table_size);
}

static int act8942_measure_capacity_battery(void)
{
	int adc = get_bat_adc_value();
	int table_size = sizeof(bat_value_table)/sizeof(bat_value_table[0]);
	
	return get_bat_percentage(adc, bat_value_table, bat_level_table, table_size);
}

//temporary
static int set_bat_off(void)
{
	return 0;
}


static struct act8942_operations act8942_pdata = {
	.is_ac_online = is_ac_online,
	.is_usb_online = is_usb_online,
	.set_bat_off = set_bat_off,
	.get_charge_status = get_charge_status,
	.measure_voltage = measure_voltage,
	.measure_current = measure_current,
	.measure_capacity_charging = act8942_measure_capacity_charging,
	.measure_capacity_battery = act8942_measure_capacity_battery,
	.update_period = 2000,	//2S
};


static struct platform_device aml_pmu_device = {
    .name	= "pmu_act8942",
    .id	= -1,
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
        .size = 0x20000,
    },
#endif
  	{
		.name = "aml_logo",
		.offset = 8*1024*1024,
		.size=8*1024*1024,
	},
    {
        .name = "recovery",
        .offset = 16*1024*1024,
        .size = 16*1024*1024,
    },
    {
        .name = "boot",
        .offset = 32*1024*1024,
        .size = 16*1024*1024,
    },
	{
        .name = "system",
        .offset = 48*1024*1024,
        .size = 156*1024*1024,
    },
    {
        .name = "cache",
        .offset = 204*1024*1024,
        .size = 128*1024*1024,
    },
#ifdef CONFIG_AML_NFTL
   {
        .name = "userdata",
        .offset=332*1024*1024,
        .size=512*1024*1024,
    },
    {
		.name = "NFTL_Part",
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
#if 0
	{
		.name = "logo",
		.offset = 32*SZ_1M+40*SZ_1M,
		.size = 16*SZ_1M,
	},
	{
		.name = "aml_logo",
		.offset = 48*SZ_1M+40*SZ_1M,
		.size = 16*SZ_1M,
	},
	{
		.name = "recovery",
		.offset = 64*SZ_1M+40*SZ_1M,
		.size = 32*SZ_1M,
	},
	{
		.name = "boot",
		.offset = 96*SZ_1M+40*SZ_1M,
		.size = 32*SZ_1M,
	},
	{
		.name = "system",
		.offset = 128*SZ_1M+40*SZ_1M,
		.size = 256*SZ_1M,
	},
	{
		.name = "cache",
		.offset = 384*SZ_1M+40*SZ_1M,
		.size = 128*SZ_1M,
	},
	{
		.name = "userdata",
		.offset = 512*SZ_1M+40*SZ_1M,
		.size = 512*SZ_1M,
	},
	{
		.name = "NFTL_Part",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
	},
#endif	
};


static struct aml_nand_platform aml_nand_mid_platform[] = {
{
		.name = NAND_BOOT_NAME,
		.chip_enable_pad = AML_NAND_CE0,
		//.ready_busy_pad = AML_NAND_CE0,
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
		//.ready_busy_pad = (AML_NAND_CE0 | (AML_NAND_CE0 << 4) | (AML_NAND_CE1 << 8) | (AML_NAND_CE1 << 12)),
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

#define PWM_TCNT        (600-1)
#define PWM_MAX_VAL    (420)

static void aml_8726m_bl_init(void)
{
    SET_CBUS_REG_MASK(PWM_MISC_REG_AB, (1 << 0));
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<31));
    printk("\n\nBacklight init.\n\n");
}
static unsigned bl_level;
static unsigned aml_8726m_get_bl_level(void)
{
    return bl_level;
}
#define BL_MAX_LEVEL 60000
static void aml_8726m_set_bl_level(unsigned level)
{
	/*
    unsigned cs_level, hi, low;
    if(level < 20){
        cs_level = 0;
    }
    else if (level < 30)
    {
        cs_level = 1750;
    }
    else if (level >= 30 && level < 256)
    {
        cs_level = (level - 31) * 260 + 1760;
    }
    else
        cs_level = BL_MAX_LEVEL;
    

    hi = cs_level;
    low = BL_MAX_LEVEL - hi;


    WRITE_CBUS_REG_BITS(PWM_PWM_A,low,0,16);  //low
    WRITE_CBUS_REG_BITS(PWM_PWM_A,hi,16,16);  //hi
    */
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

    WRITE_CBUS_REG_BITS(LED_PWM_REG0, cs_level, 0, 4);
}

static void aml_8726m_power_on_bl(void)
{
    msleep(100);
    SET_CBUS_REG_MASK(PWM_MISC_REG_AB, (1 << 0));
    msleep(100);
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<31));
    msleep(100);
    //EIO -> OD4: 1
#ifdef CONFIG_SN7325
    configIO(0, 0);
    setIO_level(0, 1, 4);
#endif
}

static void aml_8726m_power_off_bl(void)
{
    //EIO -> OD4: 0
#ifdef CONFIG_SN7325
    configIO(0, 0);
    setIO_level(0, 0, 4);
#endif
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<31));
    CLEAR_CBUS_REG_MASK(PWM_MISC_REG_AB, (1 << 0));
    //set_gpio_val(GPIOA_bank_bit(7), GPIOA_bit_bit0_14(7), 0);
    //set_gpio_mode(GPIOA_bank_bit(7), GPIOA_bit_bit0_14(7), GPIO_OUTPUT_MODE);
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
}

static void bt_device_on(void)
{
	/* BT_RST_N */
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<6));
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));	
	msleep(200);	
	SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));
}

static void bt_device_off(void)
{
	/* BT_RST_N */
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<6));
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<6));	
	msleep(200);	
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

#if defined (CONFIG_AMLOGIC_MODEM)
    &modem_dev,
#endif  

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
#if defined(CONFIG_KEYPADS_AM)||defined(CONFIG_VIRTUAL_REMOTE)||defined(CONFIG_KEYPADS_AM_MODULE)
    &input_device,
#endif
#if defined(CONFIG_AMLOGIC_SPI_NOR)
    &amlogic_spi_nor_device,
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
#if defined(CONFIG_I2C_AML)|| defined(CONFIG_I2C_HW_AML)
    &aml_i2c_device,
    &aml_i2c_device1,
    &aml_i2c_device2,
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
#ifdef CONFIG_AR1520_GPS
	&aml_ar1520_device,
#endif
#ifdef CONFIG_EFUSE
	&aml_efuse_device,
#endif
#ifdef CONFIG_PMU_ACT8942
	&aml_pmu_device,
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info[] = {
#ifdef CONFIG_TOUCH_KEY_PAD_IT7230
    {
        I2C_BOARD_INFO("it7230", 0x46),
        .irq = IT7230_INT,
        .platform_data = (void *)&it7230_pdata,
    },
#endif
#ifdef CONFIG_ITK_CAPACITIVE_TOUCHSCREEN
    {
        I2C_BOARD_INFO("itk", 0x41),
        .irq = ITK_INT,
        .platform_data = (void *)&itk_pdata,
    },
#endif
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308

	{
        /*gc0308 i2c address is 0x42/0x43*/
		I2C_BOARD_INFO("gc0308_i2c",  0x42 >> 1),
		.platform_data = (void *)&video_gc0308_data,
	},
#endif
//#ifdef CONFIG_CAMERA_OV9650FSL
//#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info_1[] = {
#ifdef CONFIG_SENSORS_MMC328X
	{
		I2C_BOARD_INFO(MMC328X_I2C_NAME,  MMC328X_I2C_ADDR),
	},
#endif
#ifdef CONFIG_SIX_AXIS_SENSOR_MPU3050
    {
        I2C_BOARD_INFO("mpu3050", 0x68),
        .irq = MPU3050_IRQ,
        .platform_data = (void *)&mpu3050_data,
    },
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info_2[] = {
#ifdef CONFIG_BQ27x00_BATTERY
    {
        I2C_BOARD_INFO("bq27200", 0x55),
        .platform_data = (void *)&bq27x00_pdata,
    },
#endif
#ifdef CONFIG_PMU_ACT8942
	{
        I2C_BOARD_INFO("act8942-i2c", ACT8942_ADDR),
		.platform_data = (void *)&act8942_pdata,	
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
	 eth_set_pinmux(ETH_BANK0_GPIOY1_Y9, ETH_CLK_OUT_GPIOY0_REG6_17, 0);
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
    aml_i2c_init();
}

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
#if defined(CONFIG_TVIN_BT656IN)
    bt656in_pinmux_init();
#endif
    set_audio_pinmux(AUDIO_OUT_TEST_N);
   // set_audio_pinmux(AUDIO_IN_JTAG);
#ifdef CONFIG_SIX_AXIS_SENSOR_MPU3050
    mpu3050_init_irq();
#endif    
#if 1
    //set clk for wifi
    WRITE_CBUS_REG(HHI_GEN_CLK_CNTL,(READ_CBUS_REG(HHI_GEN_CLK_CNTL)&(~(0x7f<<0)))|((0<<0)|(1<<8)|(7<<9)) );
    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<15));    
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<22));
#else
    // set clk for camera
    WRITE_CBUS_REG(HHI_GEN_CLK_CNTL,(READ_CBUS_REG(HHI_GEN_CLK_CNTL)&(~(0x7f<<0)))|(0<<0)|(1<<8)|(0<<9));
    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<15));   
#endif
}

static void __init  device_clk_setting(void)
{
    /*Demod CLK for eth and sata*/
    //demod_apll_setting(0,1200*CLK_1M);
    /*eth clk*/
    eth_clk_set(ETH_CLKSRC_MISC_CLK, get_misc_pll_clk(), (50 * CLK_1M), 0);
}

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

static __init void m1_init_machine(void)
{
    meson_cache_init();
    
    LED_PWM_REG0_init();
    power_hold();
    pm_power_off = power_off;		//Elvis fool
    device_clk_setting();
    device_pinmux_init();
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE
    camera_power_on_init();
#endif
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
