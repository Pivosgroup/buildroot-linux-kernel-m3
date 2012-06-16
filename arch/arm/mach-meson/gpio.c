/*
Linux gpio.C

*/
#include <linux/module.h>
#include <mach/am_regs.h>
#include <mach/gpio.h>

struct gpio_addr
{
	unsigned long mode_addr;
	unsigned long out_addr;
	unsigned long in_addr;
};
static struct gpio_addr gpio_addrs[]=
{
	[PREG_EGPIO]={PREG_EGPIO_EN_N,PREG_EGPIO_O,PREG_EGPIO_I},
	[PREG_FGPIO]={PREG_FGPIO_EN_N,PREG_FGPIO_O,PREG_FGPIO_I},
	[PREG_GGPIO]={PREG_GGPIO_EN_N,PREG_GGPIO_O,PREG_GGPIO_I},
	[PREG_HGPIO]={PREG_HGPIO_EN_N,PREG_HGPIO_O,PREG_HGPIO_I},
	[PREG_JTAG_GPIO]={PREG_JTAG_GPIO_ADDR,PREG_JTAG_GPIO_ADDR,PREG_JTAG_GPIO_ADDR},
};

char jtag_bits_map[][3] =
{
	[0]={0, 4, 8},
	[1]={1, 5, 9},
	[2]={2, 6, 10},
	[3]={3, 7, 11},
	[16]={16, 19, 24},
};

static inline int gpio_bits(int type, gpio_bank_t bank,int bit)
{
	if ((bank == PREG_JTAG_GPIO)&&(type<3)) {
		return jtag_bits_map[bit][type];
	}
	else {
		return bit;
	}
}

int set_gpio_mode(gpio_bank_t bank,int bit,gpio_mode_t mode)
{
#ifdef CONFIG_EXGPIO
	if (bank >= EXGPIO_BANK0) {
		set_exgpio_mode(bank - EXGPIO_BANK0, bit, mode);
		return 0;
	}
#endif
	unsigned long addr=gpio_addrs[bank].mode_addr;
	WRITE_CBUS_REG_BITS(addr,mode,gpio_bits(0, bank, bit),1);
	return 0;
}

gpio_mode_t get_gpio_mode(gpio_bank_t bank,int bit)
{
#ifdef CONFIG_EXGPIO
	if (bank >= EXGPIO_BANK0) {
		return get_exgpio_mode(bank - EXGPIO_BANK0, bit);
	}
#endif
	unsigned long addr=gpio_addrs[bank].mode_addr;
	return (READ_CBUS_REG_BITS(addr,gpio_bits(0, bank, bit),1)>0)?(GPIO_INPUT_MODE):(GPIO_OUTPUT_MODE);
}


int set_gpio_val(gpio_bank_t bank,int bit,unsigned long val)
{
#ifdef CONFIG_EXGPIO
	if (bank >= EXGPIO_BANK0) {
		set_exgpio_val(bank - EXGPIO_BANK0, bit, val);
		return 0;
	}
#endif
	unsigned long addr=gpio_addrs[bank].out_addr;
	WRITE_CBUS_REG_BITS(addr,val?1:0,gpio_bits(1, bank, bit),1);

	return 0;
}

unsigned long  get_gpio_val(gpio_bank_t bank,int bit)
{
#ifdef CONFIG_EXGPIO
	if (bank >= EXGPIO_BANK0) {
		return get_exgpio_val(bank - EXGPIO_BANK0, bit);
	}
#endif
	unsigned long addr=gpio_addrs[bank].in_addr;
	return READ_CBUS_REG_BITS(addr,gpio_bits(2, bank, bit),1);
}

int gpio_to_idx(unsigned gpio)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    int idx = -1;
    
    if (bank == PREG_EGPIO) {
        if (bit < 4) idx = GPIOA_23_IDX - bit;
        else if (bit < 19) idx = GPIOA_IDX + bit - 4;
        else idx = GPIOB_IDX + bit - 19;
    }
     else if (bank == PREG_FGPIO)
        idx = GPIOC_IDX + bit;
     else if (bank == PREG_GGPIO)
        idx = GPIOD_IDX + bit + 2;
     else if (bank == PREG_HGPIO)
        idx = GPIOE_IDX + bit;
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
	unsigned ireg = GPIO_INTR_GPIO_SEL0 + (group>>2);
	int value = 0;

	value = READ_CBUS_REG(ireg);
	value |= (pin<<((group&3)<<3));
	SET_CBUS_REG_MASK(ireg, value);
	
	value = READ_CBUS_REG(GPIO_INTR_EDGE_POL);
	value |= ((flag<<(16+group)) | (1<<group));
	SET_CBUS_REG_MASK(GPIO_INTR_EDGE_POL, value);	
//	WRITE_CBUS_REG_BITS(A9_0_IRQ_IN2_INTR_STAT_CLR, 0, group, 1);
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
	unsigned ireg = GPIO_INTR_GPIO_SEL0 + (group>>2);
	SET_CBUS_REG_MASK(ireg, pin<<((group&3)<<3));
	SET_CBUS_REG_MASK(GPIO_INTR_EDGE_POL, ((flag<<(16+group)) | (0<<group)));	
//	WRITE_CBUS_REG_BITS(A9_0_IRQ_IN2_INTR_STAT_CLR, 0, group, 1);
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
	unsigned ireg = GPIO_INTR_FILTER_SEL0;
	SET_CBUS_REG_MASK(ireg, filter<<(group<<2));
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
	printk( "set gpio%d.%d input\n", bank, bit);
	return 0;
}

int gpio_direction_output(unsigned gpio, int value)
{
	gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
	int bit = gpio & 0xFFFF;
	set_gpio_val(bank, bit, value?1:0);
	set_gpio_mode(bank, bit, GPIO_OUTPUT_MODE);
	printk( "set gpio%d.%d output\n", bank, bit);
	return 0;
}

void gpio_set_value(unsigned gpio, int value)
{
	gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
	int bit = gpio & 0xFFFF;
	set_gpio_val(bank, bit, value?1:0);
}

int gpio_get_value(unsigned gpio)
{
	gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
	int bit = gpio & 0xFFFF;
	return (get_gpio_val(bank, bit));
}