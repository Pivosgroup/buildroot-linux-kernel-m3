/* arch/arm/mach-meason3/include/mach/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King
 *  Moved from linux/arch/arm/kernel/debug.S by Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include "hardware.h"


#define UART_WFIFO                              (0<<2)
#define UART_RFIFO                              (1<<2)
#define UART_CONTROL                            (2<<2)
#define UART_STATUS                             (3<<2)
#define UART_MISC                               (4<<2)

#define UART_STAT_MASK_RFIFO_CNT                (0x7f<<0)
#define UART_STAT_MASK_TFIFO_CNT                (0x7f<<8)
#define UART_STAT_MASK_PRTY_ERR                 (1<<16)
#define UART_STAT_MASK_FRAM_ERR                 (1<<17)
#define UART_STAT_MASK_WFULL_ERR                (1<<18)
#define UART_STAT_MASK_RFIFO_FULL               (1<<19)
#define UART_STAT_MASK_RFIFO_EMPTY              (1<<20)
#define UART_STAT_MASK_TFIFO_FULL               (1<<21)
#define UART_STAT_MASK_TFIFO_EMPTY              (1<<22)

		.macro	addruart, rx, tmp
@		ldr	\rx, =AOBUS_REG_ADDR(AO_UART_WFIFO)		@ physical
		ldr	\rx, =0xc81004c0		@ physical
		.endm

		.macro	senduart, rd, rx
		str	\rd, [\rx]
		.endm

		.macro	waituart, rd, rx
1001:	ldr	\rd, [\rx, #UART_STATUS]
        and \rd, \rd, #UART_STAT_MASK_TFIFO_FULL
        teq \rd, #UART_STAT_MASK_TFIFO_FULL
		beq	1001b
		.endm

		.macro	busyuart, rd, rx
1002:	ldr	\rd, [\rx, #UART_STATUS]
        and \rd, \rd, #UART_STAT_MASK_TFIFO_EMPTY
        teq \rd, #UART_STAT_MASK_TFIFO_EMPTY
		bne	1002b
		.endm
