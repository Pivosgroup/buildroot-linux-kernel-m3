/*
Linux gpio.C

*/
#include <linux/module.h>
#include <mach/am_regs.h>
#include <mach/gpio.h>
#include <linux/interrupt.h>

struct gpio_addr {
    unsigned long mode_addr;
    unsigned long out_addr;
    unsigned long in_addr;
};
static struct gpio_addr gpio_addrs[] = {
    [PREG_PAD_GPIO0] = {PREG_PAD_GPIO0_EN_N, PREG_PAD_GPIO0_O, PREG_PAD_GPIO0_I},
    [PREG_PAD_GPIO1] = {PREG_PAD_GPIO1_EN_N, PREG_PAD_GPIO1_O, PREG_PAD_GPIO1_I},
    [PREG_PAD_GPIO2] = {PREG_PAD_GPIO2_EN_N, PREG_PAD_GPIO2_O, PREG_PAD_GPIO2_I},
    [PREG_PAD_GPIO3] = {PREG_PAD_GPIO3_EN_N, PREG_PAD_GPIO3_O, PREG_PAD_GPIO3_I},
    [PREG_PAD_GPIO4] = {PREG_PAD_GPIO4_EN_N, PREG_PAD_GPIO4_O, PREG_PAD_GPIO4_I},
    [PREG_PAD_GPIO5] = {PREG_PAD_GPIO5_EN_N, PREG_PAD_GPIO5_O, PREG_PAD_GPIO5_I},
    [PREG_PAD_GPIOAO] = {AO_GPIO_O_EN_N,     AO_GPIO_O_EN_N,   AO_GPIO_I},
};

int set_gpio_mode(gpio_bank_t bank, int bit, gpio_mode_t mode)
{
    unsigned long addr = gpio_addrs[bank].mode_addr;
#ifdef CONFIG_EXGPIO
    if (bank >= EXGPIO_BANK0) {
        set_exgpio_mode(bank - EXGPIO_BANK0, bit, mode);
        return 0;
    }
#endif
    if (bank==PREG_PAD_GPIOAO)
        WRITE_AOBUS_REG_BITS(addr, mode, bit, 1);
    else
        WRITE_CBUS_REG_BITS(addr, mode, bit, 1);
    return 0;
}

gpio_mode_t get_gpio_mode(gpio_bank_t bank, int bit)
{
    unsigned long addr = gpio_addrs[bank].mode_addr;
#ifdef CONFIG_EXGPIO
    if (bank >= EXGPIO_BANK0) {
        return get_exgpio_mode(bank - EXGPIO_BANK0, bit);
    }
#endif
    if (bank==PREG_PAD_GPIOAO)
        return (READ_AOBUS_REG_BITS(addr, bit, 1) > 0) ? (GPIO_INPUT_MODE) : (GPIO_OUTPUT_MODE);
    return (READ_CBUS_REG_BITS(addr, bit, 1) > 0) ? (GPIO_INPUT_MODE) : (GPIO_OUTPUT_MODE);
}


int set_gpio_val(gpio_bank_t bank, int bit, unsigned long val)
{
    unsigned long addr = gpio_addrs[bank].out_addr;
    unsigned int gpio_bit = 0;
#ifdef CONFIG_EXGPIO
    if (bank >= EXGPIO_BANK0) {
        set_exgpio_val(bank - EXGPIO_BANK0, bit, val);
        return 0;
    }
#endif
	 /* AO output: Because GPIO enable and output use the same register, we need shift 16 bit*/
	if(bank == PREG_PAD_GPIOAO) { /* AO output need shift 16 bit*/
		WRITE_AOBUS_REG_BITS(addr, val ? 1 : 0, bit+16, 1);
	} else {
		WRITE_CBUS_REG_BITS(addr, val ? 1 : 0, bit, 1);
	}

    return 0;
}

unsigned long  get_gpio_val(gpio_bank_t bank, int bit)
{
    unsigned long addr = gpio_addrs[bank].in_addr;
#ifdef CONFIG_EXGPIO
    if (bank >= EXGPIO_BANK0) {
        return get_exgpio_val(bank - EXGPIO_BANK0, bit);
    }
#endif
    if(bank == PREG_PAD_GPIOAO) 
        return READ_AOBUS_REG_BITS(addr, bit, 1);
    return READ_CBUS_REG_BITS(addr, bit, 1);
}

int gpio_to_idx(unsigned gpio)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    int idx = -1;

    switch(bank) {
    case PREG_PAD_GPIO0:
        idx = GPIOA_IDX + bit;
		break;
    case PREG_PAD_GPIO1:
        idx = GPIOB_IDX + bit;
		break;
    case PREG_PAD_GPIO2:
        idx = GPIOC_IDX + bit;
		break;
    case PREG_PAD_GPIO3:
		if( bit < 20 ) {
            idx = GPIO_BOOT_IDX + bit;
		} else {
            idx = GPIOX_IDX + (bit + 12);
		}
		break;
    case PREG_PAD_GPIO4:
        idx = GPIOX_IDX + bit;
		break;
    case PREG_PAD_GPIO5:
		if( bit < 23 ) {
            idx = GPIOY_IDX + bit;
		} else {
            idx = GPIO_CARD_IDX + (bit - 23) ;
		}
		break;
    case PREG_PAD_GPIOAO:
        idx = GPIOAO_IDX + bit;
		break;
	}

    return idx;
}

/**
 * enable gpio edge interrupt
 *
 * @param [in] pin  index number of the chip, start with 0 up to 255
 * @param [in] flag rising(0) or falling(1) edge
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
void gpio_enable_edge_int(int pin , int flag, int group)
{
	group &= 7;

	WRITE_CBUS_REG_BITS(GPIO_INTR_GPIO_SEL0+(group>>2), pin, (group&3)*8, 8);
	
	WRITE_CBUS_REG_BITS(GPIO_INTR_EDGE_POL, 1, group, 1);
	WRITE_CBUS_REG_BITS(GPIO_INTR_EDGE_POL, flag, group+16, 1);
}
/**
 * enable gpio level interrupt
 *
 * @param [in] pin  index number of the chip, start with 0 up to 255
 * @param [in] flag high(0) or low(1) level
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
void gpio_enable_level_int(int pin , int flag, int group)
{
	group &= 7;

	WRITE_CBUS_REG_BITS(GPIO_INTR_GPIO_SEL0+(group>>2), pin, (group&3)*8, 8);

	WRITE_CBUS_REG_BITS(GPIO_INTR_EDGE_POL, 0, group, 1);
	WRITE_CBUS_REG_BITS(GPIO_INTR_EDGE_POL, flag, group+16, 1);
}


int gpio_enable_irq(unsigned gpio, int irq, int irq_flag)
{
	int pin = gpio_to_idx(gpio);
	int group = irq-INT_GPIO_0;
	
	if (irq_flag == IRQF_TRIGGER_RISING)
		gpio_enable_edge_int(pin, 0, group);
	else if (irq_flag == IRQF_TRIGGER_FALLING)
		gpio_enable_edge_int(pin, 1, group);
	else if (irq_flag == IRQF_TRIGGER_HIGH)
		gpio_enable_level_int(pin, 0, group);
	else if (irq_flag == IRQF_TRIGGER_LOW)
		gpio_enable_level_int(pin, 1, group);
	else
		return -1;
	return 0;
}

/**
 * enable gpio interrupt filter
 *
 * @param [in] filter from 0~7(*222ns)
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
void gpio_enable_int_filter(int filter, int group)
{
	group &= 7;
	filter &= 7;
	WRITE_CBUS_REG_BITS(GPIO_INTR_FILTER_SEL0, filter, group*4, 3);
}

int gpio_is_valid(int number)
{
    return 1;
}

int gpio_request(unsigned gpio, const char *label)
{
    return 0;
}

void gpio_free(unsigned gpio)
{
}

int gpio_direction_input(unsigned gpio)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    set_gpio_mode(bank, bit, GPIO_INPUT_MODE);
    printk("set gpio%d.%d input\n", bank, bit);
    return (get_gpio_val(bank, bit));
}

int gpio_direction_output(unsigned gpio, int value)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    set_gpio_val(bank, bit, value ? 1 : 0);
    set_gpio_mode(bank, bit, GPIO_OUTPUT_MODE);
    printk("set gpio%d.%d output(%d)\n", bank, bit, value);
    return 0;
}

void gpio_set_value(unsigned gpio, int value)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    set_gpio_val(bank, bit, value ? 1 : 0);
}

int gpio_get_value(unsigned gpio)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    return (get_gpio_val(bank, bit));
}
