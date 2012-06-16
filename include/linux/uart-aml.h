/* 
* linux/arch/arm/mach-meson/include/mach/i2c.h
*/
#ifndef AML_MACH_UART
#define AML_MACH_UART

#include <mach/am_regs.h>

#if defined (CONFIG_ARCH_MESON3)
#define UART_AO    0
#define UART_A     1
#define UART_B     2
#define UART_C     3
#else
#define UART_A     0
#define UART_B     1
#endif

struct aml_uart_platform{
#if defined (CONFIG_ARCH_MESON3)
	int uart_line[4];
#else
	int uart_line[2];
#endif
};

#endif

