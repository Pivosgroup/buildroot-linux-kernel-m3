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
#include <mach/usbclock.h>
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
#include "board-m3-reff11-P800.h"

#ifdef CONFIG_AW_AXP
#include <linux/power_supply.h>
#include <linux/apm_bios.h>
#include <linux/apm-emulation.h>
#include <linux/regulator/machine.h>
#include <mach/irqs.h>
#include "../../../drivers/amlogic/power/axp_power/axp-gpio.h"
#include "../../../drivers/amlogic/power/axp_power/axp-mfd.h"
#include "../../../drivers/amlogic/power/axp_power/axp-cfg.h"
#endif

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

#ifdef CONFIG_EFUSE
#include <linux/efuse.h>
#endif

#ifdef CONFIG_AW_AXP
extern void axp_power_off(void);
#endif

#ifdef CONFIG_AW_AXP20

int pmu_used;
int pmu_twi_id;
int pmu_irq_id;
int pmu_twi_addr;
int pmu_battery_rdc;
int pmu_battery_cap;
int pmu_init_chgcur;
int pmu_suspend_chgcur;
int pmu_resume_chgcur;
int pmu_shutdown_chgcur;
int pmu_init_chgvol;
int pmu_init_chgend_rate;
int pmu_init_chg_enabled;
int pmu_init_adc_freq;
int pmu_init_adc_freqc;
int pmu_init_chg_pretime;
int pmu_init_chg_csttime;

int pmu_bat_para1;
int pmu_bat_para2;
int pmu_bat_para3;
int pmu_bat_para4;
int pmu_bat_para5;
int pmu_bat_para6;
int pmu_bat_para7;
int pmu_bat_para8;
int pmu_bat_para9;
int pmu_bat_para10;
int pmu_bat_para11;
int pmu_bat_para12;
int pmu_bat_para13;
int pmu_bat_para14;
int pmu_bat_para15;
int pmu_bat_para16;

int pmu_usbvol_limit;
int pmu_usbvol;
int pmu_usbcur_limit;
int pmu_usbcur;

int pmu_pwroff_vol;
int pmu_pwron_vol;

int dcdc2_vol;
int dcdc3_vol;
int ldo2_vol;
int ldo3_vol;
int ldo4_vol;

int pmu_pekoff_time;
int pmu_pekoff_en;
int pmu_peklong_time;
int pmu_pekon_time;
int pmu_pwrok_time;
int pmu_pwrnoe_time;
int pmu_intotp_en;

/* Reverse engineered partly from Platformx drivers */
enum axp_regls{

	vcc_ldo1,
	vcc_ldo2,
	vcc_ldo3,
	vcc_ldo4,
	vcc_ldo5,

	vcc_buck2,
	vcc_buck3,
	vcc_ldoio0,
};

/* The values of the various regulator constraints are obviously dependent
 * on exactly what is wired to each ldo.  Unfortunately this information is
 * not generally available.  More information has been requested from Xbow
 * but as of yet they haven't been forthcoming.
 *
 * Some of these are clearly Stargate 2 related (no way of plugging
 * in an lcd on the IM2 for example!).
 */

static struct regulator_consumer_supply ldo1_data[] = {
		{
			.supply = "axp20_rtc",
		},
	};


static struct regulator_consumer_supply ldo2_data[] = {
		{
			.supply = "axp20_analog/fm",
		},
	};

static struct regulator_consumer_supply ldo3_data[] = {
		{
			.supply = "axp20_pll",
		},
	};

static struct regulator_consumer_supply ldo4_data[] = {
		{
			.supply = "axp20_hdmi",
		},
	};

static struct regulator_consumer_supply ldoio0_data[] = {
		{
			.supply = "axp20_mic",
		},
	};


static struct regulator_consumer_supply buck2_data[] = {
		{
			.supply = "axp20_core",
		},
	};

static struct regulator_consumer_supply buck3_data[] = {
		{
			.supply = "axp20_ddr",
		},
	};



static struct regulator_init_data axp_regl_init_data[] = {
	[vcc_ldo1] = {
		.constraints = { /* board default 1.25V */
			.name = "axp20_ldo1",
			.min_uV =  AXP20LDO1 * 1000,
			.max_uV =  AXP20LDO1 * 1000,
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo1_data),
		.consumer_supplies = ldo1_data,
	},
	[vcc_ldo2] = {
		.constraints = { /* board default 3.0V */
			.name = "axp20_ldo2",
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				//.uV = ldo2_vol * 1000,
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo2_data),
		.consumer_supplies = ldo2_data,
	},
	[vcc_ldo3] = {
		.constraints = {/* default is 1.8V */
			.name = "axp20_ldo3",
			.min_uV =  700 * 1000,
			.max_uV =  3500* 1000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				//.uV = ldo3_vol * 1000,
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo3_data),
		.consumer_supplies = ldo3_data,
	},
	[vcc_ldo4] = {
		.constraints = {
			/* board default is 3.3V */
			.name = "axp20_ldo4",
			.min_uV = 1250000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				//.uV = ldo4_vol * 1000,
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(ldo4_data),
		.consumer_supplies = ldo4_data,
	},
	[vcc_buck2] = {
		.constraints = { /* default 1.24V */
			.name = "axp20_buck2",
			.min_uV = 700 * 1000,
			.max_uV = 2275 * 1000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				//.uV = dcdc2_vol * 1000,
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(buck2_data),
		.consumer_supplies = buck2_data,
	},
	[vcc_buck3] = {
		.constraints = { /* default 2.5V */
			.name = "axp20_buck3",
			.min_uV = 700 * 1000,
			.max_uV = 3500 * 1000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
			.initial_state = PM_SUSPEND_STANDBY,
			.state_standby = {
				//.uV = dcdc3_vol * 1000,
				.enabled = 1,
			}
		},
		.num_consumer_supplies = ARRAY_SIZE(buck3_data),
		.consumer_supplies = buck3_data,
	},
	[vcc_ldoio0] = {
		.constraints = { /* default 2.5V */
			.name = "axp20_ldoio0",
			.min_uV = 1800 * 1000,
			.max_uV = 3300 * 1000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ldoio0_data),
		.consumer_supplies = ldoio0_data,
	},
};

static struct axp_funcdev_info axp_regldevs[] = {
	{
		.name = "axp20-regulator",
		.id = AXP20_ID_LDO1,
		.platform_data = &axp_regl_init_data[vcc_ldo1],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_LDO2,
		.platform_data = &axp_regl_init_data[vcc_ldo2],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_LDO3,
		.platform_data = &axp_regl_init_data[vcc_ldo3],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_LDO4,
		.platform_data = &axp_regl_init_data[vcc_ldo4],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_BUCK2,
		.platform_data = &axp_regl_init_data[vcc_buck2],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_BUCK3,
		.platform_data = &axp_regl_init_data[vcc_buck3],
	}, {
		.name = "axp20-regulator",
		.id = AXP20_ID_LDOIO0,
		.platform_data = &axp_regl_init_data[vcc_ldoio0],
	},
};

static struct power_supply_info battery_data ={
		.name ="PTI PL336078",
		.technology = POWER_SUPPLY_TECHNOLOGY_LiFe,
		//.voltage_max_design = pmu_init_chgvol,
		//.voltage_min_design = pmu_pwroff_vol,
		//.energy_full_design = pmu_battery_cap,
		.use_for_apm = 1,
};


static struct axp_supply_init_data axp_sply_init_data = {
	.battery_info = &battery_data,
	//.chgcur = pmu_init_chgcur,
	//.chgvol = pmu_init_chgvol,
	//.chgend = pmu_init_chgend_rate,
	//.chgen = pmu_init_chg_enabled,
	//.sample_time = pmu_init_adc_freq,
	//.chgpretime = pmu_init_chg_pretime,
	//.chgcsttime = pmu_init_chg_csttime,
};

static struct axp_funcdev_info axp_splydev[]={
   	{
   		.name = "axp20-supplyer",
			.id = AXP20_ID_SUPPLY,
      .platform_data = &axp_sply_init_data,
    },
};

static struct axp_funcdev_info axp_gpiodev[]={
   	{   .name = "axp20-gpio",
   		.id = AXP20_ID_GPIO,
    },
};

static struct axp_platform_data axp_pdata = {
	.num_regl_devs = ARRAY_SIZE(axp_regldevs),
	.num_sply_devs = ARRAY_SIZE(axp_splydev),
	.num_gpio_devs = ARRAY_SIZE(axp_gpiodev),
	.regl_devs = axp_regldevs,
	.sply_devs = axp_splydev,
	.gpio_devs = axp_gpiodev,
	.gpio_base = 0,
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

#ifdef CONFIG_ANDROID_TIMED_GPIO	//add by sz.wu.zhu
#ifndef _LINUX_TIMED_GPIO_H
#define _LINUX_TIMED_GPIO_H

#define TIMED_GPIO_NAME "timed-gpio"

struct timed_gpio {
	const char *name;
	unsigned 	gpio;
	int		max_timeout;
	u8 		active_low;
};

struct timed_gpio_platform_data {
	int 		num_gpios;
	struct timed_gpio *gpios;
};

#endif

static struct timed_gpio amlogic_gpio_vibravor_gpios[] ={
	{
		.name	="vibrator",
		.gpio	=( GPIOC_bank_bit0_15(14) << 16 )|GPIOC_bit_bit0_15(14),	//gpioc_14
		.max_timeout	= 15000,											//15s
		.active_low	= 1,
	},
};

static struct timed_gpio_platform_data amlogic_gpio_vibravor_data= {
	.num_gpios	= 1,
	.gpios		= amlogic_gpio_vibravor_gpios,
};

static struct platform_device amlogic_gpio_vibravor = {
	.name = TIMED_GPIO_NAME,
	.dev={
		.platform_data= &amlogic_gpio_vibravor_data,
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
static struct platform_device bt656in_device = {
    .name       = "amvdec_656in",
    .id         = -1,
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
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<6));
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_6, (1<<20));
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<5));
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_6, (1<<22));
    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO0_EN_N, (1<<4));
    SET_CBUS_REG_MASK(PREG_PAD_GPIO0_O, (1<<4));

    //delay at least 100us
    udelay(500);

    //SHUTDOWN high
    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO0_EN_N, (1<<6));
    SET_CBUS_REG_MASK(PREG_PAD_GPIO0_O, (1<<6));

    //delay at least 100us
    udelay(500);

    //VDD 1V2 low
    if(is_power)
            CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO0_O, (1<<4));
    else
            SET_CBUS_REG_MASK(PREG_PAD_GPIO0_O, (1<<4));

    printk("[extern_wifi_power] 1V2 %d\n", is_power);
}

EXPORT_SYMBOL(extern_wifi_power);

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
		set_gpio_val(GPIOA_bank_bit0_27(2), GPIOA_bit_bit0_27(2), 0);	 // mute speak
		set_gpio_mode(GPIOA_bank_bit0_27(2), GPIOA_bit_bit0_27(2), GPIO_OUTPUT_MODE);
	}else{
		set_gpio_val(GPIOA_bank_bit0_27(2), GPIOA_bit_bit0_27(2), 1);	 // unmute speak
		set_gpio_mode(GPIOA_bank_bit0_27(2), GPIOA_bit_bit0_27(2), GPIO_OUTPUT_MODE);
	}
}

static struct aml_m3_platform_data aml_m3_pdata = {
    .is_hp_pluged = &aml_m3_is_hp_pluged,
};
#endif

#ifdef CONFIG_FOCALTECH_CAPACITIVE_TOUCHSCREEN
#include <linux/ft5x06_ts.h>
/* GPIOD_24 */
#define GPIO_KEY_LED  ((GPIOA_bank_bit0_27(18)<<16) |GPIOA_bit_bit0_27(18))
#define GPIO_FT_RST  ((GPIOA_bank_bit0_27(1)<<16) |GPIOA_bit_bit0_27(1))
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
    .screen_max_x=1280,
    .screen_max_y=768,
    .swap_xy = 0,
    .xpol = 0,
    .ypol = 0,
    .tp_key = &tp_key_list[0],
    .tp_key_num = ARRAY_SIZE(tp_key_list),
    .key_led_ctrl = key_led_ctrl,
};

#endif

#ifdef CONFIG_SSD25XX_CAPACITIVE_TOUCHSCREEN
#include <linux/ssd253x-ts_TP.h>
#define FT_IRQ	INT_GPIO_0
static struct ts_platform_data ts_pdata = {
	.irq_no 			= FT_IRQ,
	.reset_gpio_no	= ((GPIOA_bank_bit0_27(1)<<16) |GPIOA_bit_bit0_27(1)),
	.irq_gpio_no		= ((GPIOA_bank_bit0_27(16)<<16) |GPIOA_bit_bit0_27(16)),
	.power_gpio_no      = ((GPIOA_bank_bit0_27(9)<<16) |GPIOA_bit_bit0_27(9)),
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
#if defined(CONFIG_TVIN_BT656IN)
static void __init bt656in_pinmux_init(void)
{
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, 0xf<<6);
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, 1<<21);
}
#endif
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308
int gc0308_init(void)
{
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<29)); 
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<2));

    unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
    SET_CBUS_REG_MASK(PWM_MISC_REG_CD, (1<<15)|(0<<8)|(1<<4)|(1<<0)); //select ddr pll for source, and clk divide 
	msleep(20);
    // set camera power enable
    set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 1);    // set camera power enable
    set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
    msleep(20);
    
    set_gpio_val(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), 0);    // reset IO
    set_gpio_mode(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), GPIO_OUTPUT_MODE);
    msleep(20);

    set_gpio_val(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), 1);    // reset IO
    set_gpio_mode(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), GPIO_OUTPUT_MODE);
    msleep(20);
    
    // set camera power enable
    set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 0);    // set camera power enable
    set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
    msleep(20);
    return 0;
}
#endif

static int gc0308_have_inited = 0;
static int gt2005_have_inited = 0;
static int ov2655_have_inited = 0;

#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308)
static int gc0308_v4l2_init(void)
{
    gc0308_have_inited=1;
	gc0308_init();
}
static int gc0308_v4l2_uninit(void)
{
    gc0308_have_inited=0;
	printk( "amlogic camera driver: gc0308_v4l2_uninit. \n");
    set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 1);    // set camera power disable
    set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
	msleep(5);
	unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
	#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_OV2655)
	if(ov2655_have_inited==0)
	#endif
	#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005)
	if(gt2005_have_inited==0)
	#endif
	CLEAR_CBUS_REG_MASK(PWM_MISC_REG_CD, (1 << 0)|(1 << 2));
}
static void gc0308_v4l2_early_suspend(void)
{
    //set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 1);    // set camera power disable
    //set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
}

static void gc0308_v4l2_late_resume(void)
{
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<29)); 
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<2));

    unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
    SET_CBUS_REG_MASK(PWM_MISC_REG_CD, (1<<15)|(0<<8)|(1<<4)|(1<<0)); //select ddr pll for source, and clk divide 
	msleep(20);

    set_gpio_val(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), 1);    // reset IO
    set_gpio_mode(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), GPIO_OUTPUT_MODE);
    msleep(20);
    
    // set camera power enable
    set_gpio_val(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), 0);    // set camera power enable
    set_gpio_mode(GPIOA_bank_bit0_27(25), GPIOA_bit_bit0_27(25), GPIO_OUTPUT_MODE);
    msleep(20);
}

static struct aml_camera_i2c_fig1_s gc0308_custom_init_script[] = {
	{0x14,0x11}, 
	{0xff,0xff},
};	
	
aml_plat_cam_data_t video_gc0308_data = {
	.name="video-gc0308",
	.video_nr=0,//1,
	.device_init= gc0308_v4l2_init,
	.device_uninit=gc0308_v4l2_uninit,
	.early_suspend = gc0308_v4l2_early_suspend,
	.late_resume = gc0308_v4l2_late_resume,
	.custom_init_script = gc0308_custom_init_script,
};


#endif
#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GT2005)
static void gt2005_init(void)
{
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<29)); 
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<2));

    unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
    SET_CBUS_REG_MASK(PWM_MISC_REG_CD, (1<<15)|(0<<8)|(1<<4)|(1<<0)); //select ddr pll for source, and clk divide 
    
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
static void gt2005_v4l2_init(void)
{
    gt2005_have_inited=1;
	gt2005_init();
}
static void gt2005_v4l2_uninit(void)
{
    gt2005_have_inited=0;
    set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 0);    // set camera power disable
    set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
	msleep(5);
	unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
	#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308)
	if(gc0308_have_inited==0)
	#endif
	CLEAR_CBUS_REG_MASK(PWM_MISC_REG_CD, (1 << 0)|(1 << 2));
	//printk( "amlogic camera driver: gt2005_v4l2_uninit.\n");
    //set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 0);    // set camera power disable
    //set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
}
static void gt2005_v4l2_disable(void)
{

}

static void gt2005_v4l2_early_suspend(void)
{
	//printk( "amlogic camera driver: gt2005_v4l2_early_suspend. \n");
    //set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 0);    // set camera power disable
    //set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
}

static void gt2005_v4l2_late_resume(void)
{
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<29)); 
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<2));

    unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
    SET_CBUS_REG_MASK(PWM_MISC_REG_CD, (1<<15)|(0<<8)|(1<<4)|(1<<0)); //select ddr pll for source, and clk divide 
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

aml_plat_cam_data_t video_gt2005_data = {
	.name="video-gt2005",
	.video_nr=1,   //    1
	.device_init= gt2005_v4l2_init,
	.device_uninit=gt2005_v4l2_uninit,
	.early_suspend = gt2005_v4l2_early_suspend,
	.late_resume = gt2005_v4l2_late_resume,
	.device_disable=gt2005_v4l2_disable,
};
#endif

#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_OV2655)

static int ov2655_init(void)
{
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<29)); 
    SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<2));

    unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
    SET_CBUS_REG_MASK(PWM_MISC_REG_CD, (1<<15)|(0<<8)|(1<<4)|(1<<0)); //select ddr pll for source, and clk divide 
    udelay(1000);
    
    set_gpio_val(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), 1);    // reset IO
    set_gpio_mode(GPIOY_bank_bit0_22(10), GPIOY_bit_bit0_22(10), GPIO_OUTPUT_MODE);
    msleep(5);
    
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<12));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<7));
    
    // set camera power enable
    set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 0);    // set camera power enable
    set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
    msleep(5);
    

}
static int ov2655_v4l2_init(void)
{
    ov2655_have_inited=1;
	ov2655_init();
}

static int ov2655_v4l2_uninit(void)
{
    ov2655_have_inited=0;
	printk( "amlogic camera driver: ov2655_v4l2_uninit. \n");
    set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 1);    // set camera power disable
    set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
	msleep(5);
	unsigned pwm_cnt = get_ddr_pll_clk()/48000000 - 1;
    pwm_cnt &= 0xffff;
    WRITE_CBUS_REG(PWM_PWM_C, (pwm_cnt<<16) | pwm_cnt);
	#if defined(CONFIG_VIDEO_AMLOGIC_CAPTURE_GC0308)
	if(gc0308_have_inited==0)
	#endif
	CLEAR_CBUS_REG_MASK(PWM_MISC_REG_CD, (1 << 0)|(1 << 2));
}

static void ov2655_v4l2_early_suspend(void)
{
    /// set camera VDD disable
    //set_gpio_val(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), 1);    // set camera power disable
    //set_gpio_mode(GPIOA_bank_bit0_27(24), GPIOA_bit_bit0_27(24), GPIO_OUTPUT_MODE);
}

static void ov2655_v4l2_late_resume(void)
{
	ov2655_init();	
}

aml_plat_cam_data_t video_ov2655_data = {
	.name="video-ov2655",
	.video_nr=1,
	.device_init=ov2655_v4l2_init,
	.device_uninit=ov2655_v4l2_uninit,
	.early_suspend=ov2655_v4l2_early_suspend,
	.late_resume=ov2655_v4l2_late_resume,
	.device_probe=ov2655_init,
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


#define MAX_PINMUX	0

pinmux_data_t pinmux_data[MAX_PINMUX] = {
	//{"HDMI", 	0, (1<<2)|(1<<1)|(1<<0), 						1},
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
		//Gadmei board needn't it because it uses PMU AXP202 to control.
        //set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
       // set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 0);
              
    }
    else{
        printk(KERN_INFO "set_vccx2 power down\n"); 
		//Gadmei board needn't it because it uses PMU AXP202 to control.       
        //set_gpio_mode(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), GPIO_OUTPUT_MODE);
        //set_gpio_val(GPIOA_bank_bit0_27(26), GPIOA_bit_bit0_27(26), 1);
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
		.size = 384*SZ_1M+128*SZ_1M,
	},
	{
		.name = "cache",
		.offset = 512*SZ_1M+40*SZ_1M+128*SZ_1M,
		.size = 128*SZ_1M,
	},
	{
		.name = "userdata",
		.offset = 640*SZ_1M+40*SZ_1M+128*SZ_1M,
		.size = 1024*SZ_1M,
	},
	{//put some files by gadmei apk
		.name = "usr",
		.offset = 1664*SZ_1M+128*SZ_1M+40*SZ_1M,
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
		.ready_busy_pad = (AML_NAND_CE0 | (AML_NAND_CE1 << 4) | (AML_NAND_CE1 << 8) | (AML_NAND_CE1 << 12)),
		.platform_nand_data = {
			.chip =  {
				.nr_chips = 4,
				.nr_partitions = ARRAY_SIZE(multi_partition_info),
				.partitions = multi_partition_info,
				.options = (NAND_TIMING_MODE5 | NAND_ECC_BCH60_1K_MODE /*| NAND_TWO_PLANE_MODE*/),
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
   // .power_on_bl = power_on_backlight,
   // .power_off_bl = power_off_backlight,
   // .get_bl_level = get_backlight_level,
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
       .vendor = " ",
       .product = "Andriod",
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
       .product_name   = "Andriod",
       .manufacturer_name = " ",
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

#ifdef CONFIG_EFUSE
	&aml_efuse_device,
#endif
#ifdef CONFIG_POST_PROCESS_MANAGER
    &ppmgr_device,
#endif
#if defined(CONFIG_USB_PHY_CONTROL)
    &usb_phy_control_device,
#endif
#ifdef CONFIG_ANDROID_TIMED_GPIO
	&amlogic_gpio_vibravor,
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info[] = {

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
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_OV2655
	{
		I2C_BOARD_INFO("ov2655_i2c", 0x60 >> 1),
		.platform_data = (void *)&video_ov2655_data,
	},
#endif
#ifdef CONFIG_FOCALTECH_CAPACITIVE_TOUCHSCREEN
    {
        I2C_BOARD_INFO("ft5x06", 0x38),
        .platform_data = (void *)&ts_pdata,
    },
#endif
#ifdef CONFIG_SSD25XX_CAPACITIVE_TOUCHSCREEN
    {
        I2C_BOARD_INFO("ssd253x-ts", 0x48),
        .platform_data = (void *)&ts_pdata,
    },
#endif

};

static struct i2c_board_info __initdata aml_i2c_bus_info_1[] = {
#ifdef CONFIG_BOSCH_BMA222
	{
		I2C_BOARD_INFO("bma222",  0x08),
		//.irq = INT_GPIO_1,
	},
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info_2[] = {
#ifdef CONFIG_AW_AXP20
	{
	    I2C_BOARD_INFO("axp20_mfd", AXP20_ADDR),
		.platform_data = &axp_pdata,
		//.irq = pmu_irq_id,
	},
#endif

};

static int __init aml_i2c_init(void)
{
    
#ifdef CONFIG_AW_AXP20
            pmu_twi_id = AXP20_I2CBUS;
            pmu_irq_id = AXP20_IRQNO;
            pmu_twi_addr = AXP20_ADDR;
            pmu_battery_rdc = BATRDC;
            pmu_battery_cap = BATTERYCAP;
            pmu_init_chgcur = INTCHGCUR ;
            pmu_suspend_chgcur = SUSCHGCUR ;
            pmu_resume_chgcur = RESCHGCUR ;
            pmu_shutdown_chgcur = CLSCHGCUR ;       
            pmu_init_chgvol = INTCHGVOL ;
            pmu_init_chgend_rate = INTCHGENDRATE;
            pmu_init_chg_enabled = INTCHGENABLED;
            pmu_init_adc_freq = INTADCFREQ;
            pmu_init_adc_freq = INTADCFREQC;
            pmu_init_chg_pretime = INTCHGPRETIME;
            pmu_init_chg_csttime = INTCHGCSTTIME;
            pmu_bat_para1 = OCVREG0;
            pmu_bat_para2 = OCVREG1;
            pmu_bat_para3 = OCVREG2;   
            pmu_bat_para4 = OCVREG3;   
            pmu_bat_para5 = OCVREG4;   
            pmu_bat_para6 = OCVREG5;
            pmu_bat_para7 = OCVREG6;
            pmu_bat_para8 = OCVREG7;
            pmu_bat_para9 = OCVREG8;
            pmu_bat_para10 = OCVREG9;   
            pmu_bat_para11 = OCVREGA;  
            pmu_bat_para12 = OCVREGB;
            pmu_bat_para13 = OCVREGC;    
            pmu_bat_para14 = OCVREGD;  
            pmu_bat_para15 = OCVREGE;    
            pmu_bat_para16 = OCVREGF;   
            pmu_usbvol_limit = 1;    
            pmu_usbvol = 4000;   
            pmu_usbcur_limit = 0; 
            pmu_usbcur = 900;
            pmu_pwroff_vol = 2600; 
            pmu_pwron_vol = 2600;    
            dcdc2_vol = 1520;//1400;   
            dcdc3_vol = 1220;//1250;    
            ldo2_vol = 3000;    
            ldo3_vol = 2800;       
            ldo4_vol = 2800;       
            pmu_pekoff_time = 6000;       
            pmu_pekoff_en   = 1;       
            pmu_peklong_time = 1500;       
            pmu_pwrok_time    = 64;      
            pmu_pwrnoe_time = 2000;       
            pmu_intotp_en = 1;       
            pmu_pekon_time = 1000;
                  

        //axp_regl_init_data[1].constraints.state_standby.uV = ldo2_vol * 1000;
        //axp_regl_init_data[2].constraints.state_standby.uV = ldo3_vol * 1000;
        //axp_regl_init_data[3].constraints.state_standby.uV = ldo4_vol * 1000;
        axp_regl_init_data[5].constraints.state_standby.uV = dcdc2_vol * 1000;
        axp_regl_init_data[6].constraints.state_standby.uV = dcdc3_vol * 1000;
        battery_data.voltage_max_design = pmu_init_chgvol;
        battery_data.voltage_min_design = pmu_pwroff_vol * 1000;
        battery_data.energy_full_design = pmu_battery_cap;
        axp_sply_init_data.chgcur = pmu_init_chgcur;
        axp_sply_init_data.chgvol = pmu_init_chgvol;
        axp_sply_init_data.chgend = pmu_init_chgend_rate;
        axp_sply_init_data.chgen = pmu_init_chg_enabled;
        axp_sply_init_data.sample_time = pmu_init_adc_freq;
        axp_sply_init_data.chgpretime = pmu_init_chg_pretime;
        axp_sply_init_data.chgcsttime = pmu_init_chg_csttime;
        aml_i2c_bus_info_2[0].addr = pmu_twi_addr;
        aml_i2c_bus_info_2[0].irq = pmu_irq_id;

#endif
    i2c_register_board_info(0, aml_i2c_bus_info,
        ARRAY_SIZE(aml_i2c_bus_info));
    i2c_register_board_info(1, aml_i2c_bus_info_1,
        ARRAY_SIZE(aml_i2c_bus_info_1)); 
    i2c_register_board_info(2, aml_i2c_bus_info_2,
        ARRAY_SIZE(aml_i2c_bus_info_2)); 
    return 0;
}


static void __init device_pinmux_init(void )
{
    clearall_pinmux();
    aml_i2c_init();
    //set_audio_pinmux(AUDIO_OUT_TEST_N);
   // set_audio_pinmux(AUDIO_IN_JTAG);
   

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

/* usb wifi power 1:power on  0:power off */
/*void extern_usb_wifi_power(int is_power)
{
    printk(KERN_INFO "usb_wifi_power %s\n", is_power ? "On" : "Off");	
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<11));
	CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<18));
	CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<8));
	if(is_power)
		CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));
	else
		SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<8));   
}*/
/* usb wifi power 1:power on  0:power off */
void extern_usb_wifi_power(int is_power)
{
#if 1 //old board
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<6));
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_6, (1<<19));
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<5));
	set_gpio_mode(GPIOA_bank_bit0_27(7), GPIOA_bit_bit0_27(7), GPIO_OUTPUT_MODE);
    if(is_power)
            set_gpio_val(GPIOA_bank_bit0_27(7), GPIOA_bit_bit0_27(7), 0);
    else
            set_gpio_val(GPIOA_bank_bit0_27(7), GPIOA_bit_bit0_27(7), 1);
//new board
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (1<<6));
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_6, (1<<22));
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<5));
	set_gpio_mode(GPIOA_bank_bit0_27(4), GPIOA_bit_bit0_27(4), GPIO_OUTPUT_MODE);
    if(is_power)
            set_gpio_val(GPIOA_bank_bit0_27(4), GPIOA_bit_bit0_27(4), 0);
    else
            set_gpio_val(GPIOA_bank_bit0_27(4), GPIOA_bit_bit0_27(4), 1);
#endif

    printk("[extern_wifi_power] 1V2 %d\n", is_power);
}

EXPORT_SYMBOL(extern_usb_wifi_power);

#if defined(CONFIG_AML_INIT_GATE_OFF)
#define GATE_INIT_OFF(_MOD) CLEAR_CBUS_REG_MASK(GCLK_REG_##_MOD, GCLK_MASK_##_MOD);

static __init void init_gate_off(void) 
{
	//turn of video gates
	GATE_INIT_OFF(VCLK2_VENCP1);
	GATE_INIT_OFF(VCLK2_VENCP);
	GATE_INIT_OFF(VCLK2_VENCL);
	GATE_INIT_OFF(VCLK2_ENCL);
	GATE_INIT_OFF(VCLK2_OTHER1);
	GATE_INIT_OFF(VCLK2_VENCI1);
	GATE_INIT_OFF(VCLK2_VENCI);
	GATE_INIT_OFF(VENC_P_TOP);
	GATE_INIT_OFF(VENC_L_TOP);
	GATE_INIT_OFF(VENC_I_TOP);
	//GATE_INIT_OFF(VCLK2_VENCT);
	//GATE_INIT_OFF(VCLK2_ENCT);
	GATE_INIT_OFF(VENCP_INT);
	GATE_INIT_OFF(VENCL_INT);
	GATE_INIT_OFF(VCLK2_ENCI);
	GATE_INIT_OFF(VCLK2_ENCP);
	GATE_INIT_OFF(VCLK2_OTHER);
	GATE_INIT_OFF(ENC480P);
	GATE_INIT_OFF(VENC_DAC);
	GATE_INIT_OFF(DAC_CLK);
}
#endif    

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
// Gadmei uses PMU to control.
//    pm_power_off = power_off;		//Elvis fool
    device_pinmux_init();
	extern_usb_wifi_power(0);
#if defined(CONFIG_AML_INIT_GATE_OFF)
    init_gate_off();
#endif    
    platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));

#ifdef CONFIG_USB_DWC_OTG_HCD
    set_usb_phy_clk(USB_PHY_CLOCK_SEL_XTAL_DIV2);
    lm_device_register(&usb_ld_a);
	set_usb_phy_id_mode(USB_PHY_PORT_B,USB_PHY_MODE_SW_HOST);
    lm_device_register(&usb_ld_b);
#endif
    disable_unused_model();
    // hp detect
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
