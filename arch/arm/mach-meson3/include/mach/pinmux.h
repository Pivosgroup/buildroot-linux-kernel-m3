#ifndef MESON_PINMUX_H
#define MESON_PINMUX_H

/*
Linux Pinmux.h
*/

int clear_mio_mux(unsigned mux_index, unsigned mux_mask);
int set_mio_mux(unsigned mux_index, unsigned mux_mask);
void clearall_pinmux(void);

/*Ethernet*/
/*
"RMII_MDIOREG6[8]"
"RMII_MDCREG6[9]"
"RMII_TX_DATA0REG6[10]"
"RMII_TX_DATA1REG6[11]"
"RMII_TX_ENREG6[12]"
"RMII_RX_DATA0REG6[13]"
"RMII_RX_DATA1REG6[14]"
"RMII_RX_CRS_DVREG6[15]"
"RMII_RX_ERRREG6[16]"
Bank0_GPIOY1-Y9
*/
#define ETH_BANK0_GPIOY1_Y9     0
#define ETH_BANK0_REG1          6
#define ETH_BANK0_REG1_VAL              (0x1ff<<8)

#define ETH_CLK_IN_GPIOY0_REG6_18       0
#define ETH_CLK_OUT_GPIOY0_REG6_17      1

int eth_set_pinmux(int bank_id, int clk_in_out_id, unsigned long ext_msk);

/*UART*/
#define UART_PORT_AO                (0)   /* ALWAYS_ON GPIOAO_0-> GPIOAO-3*/
#define UART_PORT_A                 (1)
#define UART_PORT_B                 (2)
#define UART_PORT_C                 (3)

#define UART_AO_GPIO_AO0_AO1_STD    (0)
#define UART_AO_GPIO_AO0_AO3_FULL   (1)
#define UART_A_GPIO_X13_X14_STD     (2)
#define UART_A_GPIO_X13_X16_FULL    (3)
#define UART_B_GPIO_X17_X18_STD     (4)
#define UART_B_GPIO_X17_X20_FULL    (5)
#define UART_B_GPIO_X23_X24_STD     (6)
#define UART_C_GPIO_X21_X22_STD     (7)
#define UART_C_GPIO_X21_X24_FULL    (8)

void uart_set_pinmux(int port, int uart_bank);

/*AUDIO*/

#define AUDIO_OUT_JTAG              0
#define AUDIO_OUT_GPIOA             1
#define AUDIO_OUT_TEST_N            2
#define AUDIO_IN_JTAG               3
#define SPDIF_OUT_GPIOA             4
#define SPDIF_OUT_GPIOB             5
#define SPDIF_OUT_TEST_N            6
#define SPDIF_IN_GPIOA              7
#define SPDIF_IN_GPIOB              8

void set_audio_pinmux(int type);

#endif

