#ifndef __MESON_GPIO_H__
#define	 __MESON_GPIO_H__
/*
// Pre-defined GPIO addresses
// ----------------------------
#define PREG_EGPIO_EN_N                            0x200c
#define PREG_EGPIO_O                               0x200d
#define PREG_EGPIO_I                               0x200e
// ----------------------------
#define PREG_FGPIO_EN_N                            0x200f
#define PREG_FGPIO_O                               0x2010
#define PREG_FGPIO_I                               0x2011
// ----------------------------
#define PREG_GGPIO_EN_N                            0x2012
#define PREG_GGPIO_O                               0x2013
#define PREG_GGPIO_I                               0x2014
// ----------------------------
#define PREG_HGPIO_EN_N                            0x2015
#define PREG_HGPIO_O                               0x2016
#define PREG_HGPIO_I                               0x2017
// ----------------------------
*/

#ifdef CONFIG_TCA6424
#include <linux/tca6424.h>
#define CONFIG_EXGPIO
#endif

#ifdef CONFIG_SN7325
#include <linux/sn7325.h>
#ifndef CONFIG_EXGPIO
#define CONFIG_EXGPIO
#endif
#endif

typedef enum gpio_bank
{
	PREG_EGPIO=0,
	PREG_FGPIO,
	PREG_GGPIO,
	PREG_HGPIO,
	PREG_JTAG_GPIO,
#ifdef CONFIG_EXGPIO
	EXGPIO_BANK0,
	EXGPIO_BANK1,
	EXGPIO_BANK2,
	EXGPIO_BANK3
#endif
}gpio_bank_t;


typedef enum gpio_mode
{
	GPIO_OUTPUT_MODE,
	GPIO_INPUT_MODE,
}gpio_mode_t;

int set_gpio_mode(gpio_bank_t bank,int bit,gpio_mode_t mode);
gpio_mode_t get_gpio_mode(gpio_bank_t bank,int bit);

int set_gpio_val(gpio_bank_t bank,int bit,unsigned long val);
unsigned long  get_gpio_val(gpio_bank_t bank,int bit);


#define GPIOA_bank_bit(bit)		(PREG_EGPIO)

#define GPIOA_bank_bit23_26(bit)	(PREG_EGPIO)
#define GPIOA_bit_bit23_26(bit)		(bit-23)

#define GPIOA_bank_bit0_14(bit)		(PREG_EGPIO)
#define GPIOA_bit_bit0_14(bit)		(bit+4)
										
#define GPIOB_bank_bit0_7(bit)		(PREG_EGPIO)
#define GPIOB_bit_bit0_7(bit)		(bit+19)		

#define GPIOC_bank_bit0_26(bit)		(PREG_FGPIO)
#define GPIOC_bit_bit0_26(bit)		(bit)		

#define GPIOD_bank_bit2_24(bit)		(PREG_GGPIO)
#define GPIOD_bit_bit2_24(bit)		(bit-2)		

#define GPIOE_bank_bit0_15(bit)		(PREG_HGPIO)
#define GPIOE_bit_bit0_15(bit)		(bit)		

#define GPIOE_bank_bit16_21(bit)	(PREG_HGPIO)
#define GPIOE_bit_bit16_21(bit)		(bit)		

#define GPIOJTAG_bank_bit(bit)		(PREG_JTAG_GPIO)
#define GPIOJTAG_bit_bit0_3(bit)	(bit)
#define GPIOJTAG_bit_bit16(bit)		(bit)

#define GPIO_JTAG_TCK_BIT			0
#define GPIO_JTAG_TMS_BIT			1
#define GPIO_JTAG_TDI_BIT			2
#define GPIO_JTAG_TDO_BIT			3
#define GPIO_TEST_N_BIT				16

enum {
    GPIOA_IDX = 0,
    GPIOB_IDX = 15,
    GPIOC_IDX = 23,
    GPIOD_IDX = 50,
    GPIOE_IDX = 75,
    GPIOA_26_IDX = 97,
    GPIOA_25_IDX = 98,
    GPIOA_24_IDX = 99,
    GPIOA_23_IDX = 100,
    JTAG_TCK_IDX = 101,
    JTAG_TMS_IDX = 102,
    JTAG_TDI_IDX = 103,
    JTAG_TDO_IDX = 104,
};

extern int gpio_to_idx(unsigned gpio);

/**
 * enable gpio edge interrupt
 *	
 * @param [in] pin  index number of the chip, start with 0 up to 255 
 * @param [in] flag rising(0) or falling(1) edge 
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
extern void gpio_enable_edge_int(int pin , int flag, int group);
/**
 * enable gpio level interrupt
 *	
 * @param [in] pin  index number of the chip, start with 0 up to 255 
 * @param [in] flag high(0) or low(1) level 
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
extern void gpio_enable_level_int(int pin , int flag, int group);

/**
 * enable gpio interrupt filter
 *
 * @param [in] filter from 0~7(*222ns)
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
extern void gpio_enable_int_filter(int filter, int group);

extern int gpio_is_valid(int number);
extern int gpio_request(unsigned gpio, const char *label);
extern void gpio_free(unsigned gpio);
extern int gpio_direction_input(unsigned gpio);
extern int gpio_direction_output(unsigned gpio, int value);
extern void gpio_set_value(unsigned gpio, int value);
extern int gpio_get_value(unsigned gpio);


#ifdef CONFIG_EXGPIO
#define MAX_EXGPIO_BANK 4

static inline unsigned char get_exgpio_port(gpio_bank_t bank)
{
    unsigned char port = bank - EXGPIO_BANK0;
    
    return (port >= MAX_EXGPIO_BANK)? MAX_EXGPIO_BANK : port;        
}

static inline int set_exgpio_mode(unsigned char port,int bit,gpio_mode_t mode)
{
    int bank_mode ;
    if( port == MAX_EXGPIO_BANK )
        return -1;
    
    bank_mode = get_configIO(port);
    
    bank_mode &= ~(1 << bit);
    bank_mode |= mode << bit;
    configIO(port, bank_mode);
    return 0;
}

static inline gpio_mode_t get_exgpio_mode(unsigned char port,int bit)
{
    if( port == MAX_EXGPIO_BANK )
        return -1;
    return (get_configIO(port) >> bit) & 1;
}

static inline int set_exgpio_val(unsigned char port,int bit,unsigned long val)
{
    if( port == MAX_EXGPIO_BANK )
        return -1;
    
#ifdef CONFIG_TCA6424
    int bank_val = getIO_level(port);
    
    bank_val &= ~(1 << bit);
    bank_val |= val << bit;
    setIO_level(port, bank_val);
#endif

#ifdef CONFIG_SN7325
    setIO_level(port, val, bit);
#endif
    return 0;
}

static inline unsigned long  get_exgpio_val(unsigned char port, int bit)
{
    if( port == MAX_EXGPIO_BANK )
        return -1;
#ifdef CONFIG_TCA6424    
    return (getIO_level(port) >> bit) & 1;
#endif

#ifdef CONFIG_SN7325
    return getIObit_level(port, bit);
#endif
}
#endif

#endif
