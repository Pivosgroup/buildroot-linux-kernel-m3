/*
Linux PINMUX.C

*/
#include <linux/module.h>
#include <mach/am_regs.h>
#include <mach/am_eth_reg.h>

#include <mach/pinmux.h>

int  clear_mio_mux(unsigned mux_index, unsigned mux_mask)
{
    unsigned mux_reg[] = {PERIPHS_PIN_MUX_0, PERIPHS_PIN_MUX_1, PERIPHS_PIN_MUX_2, PERIPHS_PIN_MUX_3,
                          PERIPHS_PIN_MUX_4, PERIPHS_PIN_MUX_5, PERIPHS_PIN_MUX_6, PERIPHS_PIN_MUX_7, 
						  PERIPHS_PIN_MUX_8, PERIPHS_PIN_MUX_9, PERIPHS_PIN_MUX_10, PERIPHS_PIN_MUX_11,
						  PERIPHS_PIN_MUX_12
                         };
    if (mux_index < 13) {
        CLEAR_CBUS_REG_MASK(mux_reg[mux_index], mux_mask);
        return 0;
    }
    return -1;
}
EXPORT_SYMBOL(clear_mio_mux);

int  set_mio_mux(unsigned mux_index, unsigned mux_mask)
{
    unsigned mux_reg[] = {PERIPHS_PIN_MUX_0, PERIPHS_PIN_MUX_1, PERIPHS_PIN_MUX_2, PERIPHS_PIN_MUX_3,
                          PERIPHS_PIN_MUX_4, PERIPHS_PIN_MUX_5, PERIPHS_PIN_MUX_6, PERIPHS_PIN_MUX_7,
						  PERIPHS_PIN_MUX_8, PERIPHS_PIN_MUX_9, PERIPHS_PIN_MUX_10, PERIPHS_PIN_MUX_11,
						  PERIPHS_PIN_MUX_12
                         };
    if (mux_index < 13) {
        SET_CBUS_REG_MASK(mux_reg[mux_index], mux_mask);
        return 0;
    }
    return -1;
}

EXPORT_SYMBOL(set_mio_mux);


/*
call it before pinmux init;
call it before soft reset;
*/
void clearall_pinmux(void)
{
    int i;
    for (i = 0; i < 13; i++) {
        clear_mio_mux(i, 0xffffffff);
    }
    return;
}
EXPORT_SYMBOL(clearall_pinmux);

/*ETH PINMUX SETTING
More details can get from pinmux.h
*/
int eth_set_pinmux(int bank_id, int clk_in_out_id, unsigned long ext_msk)
{
    int ret = 0;
    switch (bank_id) {
    case    ETH_BANK0_GPIOY1_Y9:
        if (ext_msk > 0) {
            set_mio_mux(ETH_BANK0_REG1, ext_msk);
        } else {
            set_mio_mux(ETH_BANK0_REG1, ETH_BANK0_REG1_VAL);
        }
        break;
    default:
        printk(KERN_ERR "UNknow pinmux setting of ethernet!error bankid=%d,must be 0-2\n", bank_id);
        ret = -1;

    }
    switch (clk_in_out_id) {
    case  ETH_CLK_IN_GPIOY0_REG6_18:
        set_mio_mux(6, 1 << 18);
        break;
    case  ETH_CLK_OUT_GPIOY0_REG6_17:
        set_mio_mux(6, 1 << 17);
        break;
    default:
        printk(KERN_ERR "UNknow clk_in_out_id setting of ethernet!error clk_in_out_id=%d,must be 0-9\n", clk_in_out_id);
        ret = -1;
    }
    return ret;
}
EXPORT_SYMBOL(eth_set_pinmux);

void uart_set_pinmux(int port, int uart_bank)
{
    if (port == UART_PORT_AO) { /*PORT AO ALWAYS_ON GPIOAO_0-> GPIOAO-3*/
        switch (uart_bank) {
        case UART_AO_GPIO_AO0_AO1_STD:/*UART_A_GPIO_A00_AO1_STD*/
            SET_AOBUS_REG_MASK(AO_RTI_PIN_MUX_REG, (0x3<<11));
            break;
        case UART_AO_GPIO_AO0_AO3_FULL:/*UART_A_GPIO_A00_AO3_FULL*/
            SET_AOBUS_REG_MASK(AO_RTI_PIN_MUX_REG, (0xf<<9));
            break;
        default:
            printk("UartAO pinmux set error\n");
			break;
        }
	} else if (port == UART_PORT_A) { /*PORT A*/
        switch (uart_bank) {
        case UART_A_GPIO_X13_X14_STD:/*UART_A_GPIO_X13_X14_STD*/
            SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (0x3 << 12));
            break;
        case UART_A_GPIO_X13_X16_FULL:/*UART_A_GPIO_X13_X16_FULL*/
            SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (0xf << 10));
            break;
        default:
            printk("UartA pinmux set error\n");
			break;
        }
    } else if (port == UART_PORT_B) { /*UART_B*/
        switch (uart_bank) {
        case UART_B_GPIO_X17_X18_STD://GPIO+X17.X18
            SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (0x3 << 8));
            break;
        case UART_B_GPIO_X17_X20_FULL://GPIO+X17.X20
            SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (0xf << 6));
			break;
        case UART_B_GPIO_X23_X24_STD://GPIO+X23.X24
            SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (0x3 << 4));
			break;
        default:
            printk("UartB pinmux set error\n");
			break;
        }
    } else if (port == UART_PORT_C) { /*UART_C*/
        switch (uart_bank) {
        case UART_C_GPIO_X21_X22_STD://GPIO+X21.X22
            SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (0x3 << 2));
            break;
        case UART_C_GPIO_X21_X24_FULL://GPIO+X21.X24
            SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_4, (0xf << 0));
			break;
        default:
            printk("UartC pinmux set error\n");
			break;
        }
    } else {
        printk("Unknow Uart Port%d!(0,1)\n", port);
    }
}

void set_audio_pinmux(int type)
{
    if (type == AUDIO_OUT_JTAG) {
        set_mio_mux(1,
                    (1 << 6) | (1 << 11) | (1 << 15) | (1 << 19)
                   );
    } else if (type == AUDIO_OUT_GPIOA) {
        set_mio_mux(1,
                    (1 << 16) | (1 << 11) | (1 << 15) | (1 << 19)
                   );
        set_mio_mux(8, (1 << 5));
    } else if (type == AUDIO_OUT_TEST_N) {
        clear_mio_mux(1, (1 << 6));
        clear_mio_mux(0, (1 << 17));
        clear_mio_mux(6, (1 << 24));
        WRITE_CBUS_REG_BITS(0x200b, 0, 16, 1); //Set TEST_N Output mode
        set_mio_mux(1, (1 << 11) | (1 << 15) | (1 << 19));
        set_mio_mux(0, (1 << 18));
    } else if (type == AUDIO_IN_JTAG) {
        clear_mio_mux(1, (1 << 6));
        set_mio_mux(1, (1 << 11) | (1 << 15) | (1 << 19));
        set_mio_mux(8, (1 << 8) | (1 << 9) | (1 << 10));
        set_mio_mux(8, (1 << 11));
    } else if (type == SPDIF_OUT_GPIOA) {
        set_mio_mux(0, (1 << 19));
    } else if (type == SPDIF_OUT_GPIOB) {
        set_mio_mux(2, (1 << 1));
    } else if (type == SPDIF_OUT_TEST_N) {
        set_mio_mux(0, (1 << 17));
    } else if (type == SPDIF_IN_GPIOA) {
        set_mio_mux(0, (1 << 20));
    } else if (type == SPDIF_IN_GPIOB) {
        set_mio_mux(2, (1 << 0));
    }
}
EXPORT_SYMBOL(set_audio_pinmux);
