#ifndef MESON_PINMUX_H
#define MESON_PINMUX_H

/*
Linux Pinmux.h
*/

int clear_mio_mux(unsigned mux_index, unsigned mux_mask);
int set_mio_mux(unsigned mux_index, unsigned mux_mask);
void   clearall_pinmux(void);


#include <mach/am_eth_pinmux.h>
int eth_set_pinmux(int bank_id,int clk_in_out_id,unsigned long ext_msk);

/*UART*/
#define UART_PORT_A	(0)
#define UART_PORT_B		(1)

#define UART_A_TMS_TDOI		(0)
#define UART_A_GPIO_B2_B3		(1)
#define UART_A_GPIO_C9_C10		(2)
#define UART_A_GPIO_C21_D22	(3)
#define UART_A_GPIO_D21_D22	(4)
#define UART_A_GPIO_E14_E15	(5)

#define UART_B_TCK_TDO			(6)
#define UART_B_GPIO_B0_B1		(7)
#define UART_B_GPIO_C13_C14	(8)
#define UART_B_GPIO_E18_E19	(9)
void uart_set_pinmux(int port,int uart_bank);

/*AUDIO*/

#define AUDIO_OUT_JTAG				0
#define AUDIO_OUT_GPIOA				1
#define AUDIO_OUT_TEST_N			2
#define AUDIO_IN_JTAG					3
#define SPDIF_OUT_GPIOA				4
#define SPDIF_OUT_GPIOB				5
#define SPDIF_OUT_TEST_N			6
#define SPDIF_IN_GPIOA				7
#define SPDIF_IN_GPIOB				8

void set_audio_pinmux(int type);

#endif

