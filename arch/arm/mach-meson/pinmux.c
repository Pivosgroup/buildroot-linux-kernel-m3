/*
Linux PINMUX.C

*/
#include <linux/module.h>
#include <mach/am_regs.h>
#include <mach/am_eth_reg.h>

#include <mach/pinmux.h>

int  clear_mio_mux(unsigned mux_index, unsigned mux_mask)
{
    unsigned mux_reg[] = {PERIPHS_PIN_MUX_0, PERIPHS_PIN_MUX_1, PERIPHS_PIN_MUX_2,PERIPHS_PIN_MUX_3,
		PERIPHS_PIN_MUX_4,PERIPHS_PIN_MUX_5,PERIPHS_PIN_MUX_6,PERIPHS_PIN_MUX_7,PERIPHS_PIN_MUX_8,
		PERIPHS_PIN_MUX_9,PERIPHS_PIN_MUX_10,PERIPHS_PIN_MUX_11,PERIPHS_PIN_MUX_12};
    if (mux_index < 13) {
        CLEAR_CBUS_REG_MASK(mux_reg[mux_index], mux_mask);
		return 0;
    }
	return -1;
}
EXPORT_SYMBOL(clear_mio_mux);

int  set_mio_mux(unsigned mux_index, unsigned mux_mask)
{
    unsigned mux_reg[] = {PERIPHS_PIN_MUX_0, PERIPHS_PIN_MUX_1, PERIPHS_PIN_MUX_2,PERIPHS_PIN_MUX_3,
		PERIPHS_PIN_MUX_4,PERIPHS_PIN_MUX_5,PERIPHS_PIN_MUX_6,PERIPHS_PIN_MUX_7,PERIPHS_PIN_MUX_8,
		PERIPHS_PIN_MUX_9,PERIPHS_PIN_MUX_10,PERIPHS_PIN_MUX_11,PERIPHS_PIN_MUX_12};
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
void   clearall_pinmux(void)
{
	int i;
	for(i=0;i<13;i++)
		clear_mio_mux(i,0xffffffff);
	return;
}
EXPORT_SYMBOL(clearall_pinmux);

/*ETH PINMUX SETTING
More details can get from am_eth_pinmux.h
*/
int eth_set_pinmux(int bank_id,int clk_in_out_id,unsigned long ext_msk)
{
	int ret=0;
	switch(bank_id)
	{
		case	ETH_BANK0_GPIOC3_C12:
				if(ext_msk>0)
					set_mio_mux(ETH_BANK0_REG1,ext_msk);
				else
					set_mio_mux(ETH_BANK0_REG1,ETH_BANK0_REG1_VAL);
				break;
		case 	ETH_BANK1_GPIOD2_D11:
				if(ext_msk>0)
					set_mio_mux(ETH_BANK1_REG1,ext_msk);
				else
					set_mio_mux(ETH_BANK1_REG1,ETH_BANK1_REG1_VAL);
				break;
		case 	ETH_BANK2_GPIOD15_D23:
				if(ext_msk>0)
					set_mio_mux(ETH_BANK2_REG1,ext_msk);
				else
					set_mio_mux(ETH_BANK2_REG1,ETH_BANK2_REG1_VAL);
				break;
		default:
				printk(KERN_ERR "UNknow pinmux setting of ethernet!error bankid=%d,must be 0-2\n",bank_id);
				ret=-1;
				
	}
	switch(clk_in_out_id)
	{
		case  ETH_CLK_IN_GPIOC2_REG4_26:
				set_mio_mux(4,1<<26);	
				break;
		case  ETH_CLK_IN_GPIOC12_REG3_0:
				set_mio_mux(3,1<<0);	
				break;
 		case  ETH_CLK_IN_GPIOD7_REG4_19:
				set_mio_mux(4,1<<19);	
				break;
	 	case  ETH_CLK_IN_GPIOD14_REG7_12:
				set_mio_mux(7,1<<12);	
				break;
 		case  ETH_CLK_IN_GPIOD24_REG5_0:
				set_mio_mux(5,1<<0);	
				break;
 		case  ETH_CLK_OUT_GPIOC2_REG4_27:
				set_mio_mux(4,1<<27);	
				break;
 		case  ETH_CLK_OUT_GPIOC12_REG3_1:
				set_mio_mux(3,1<<1);	
				break;
		case  ETH_CLK_OUT_GPIOD7_REG4_20:
				set_mio_mux(4,1<<20);	
				break;
		case  ETH_CLK_OUT_GPIOD14_REG7_13:
				set_mio_mux(7,1<<13);	
				break;
 		case  ETH_CLK_OUT_GPIOD24_REG5_1:
				set_mio_mux(5,1<<1);	
				break;
		default:
				printk(KERN_ERR "UNknow clk_in_out_id setting of ethernet!error clk_in_out_id=%d,must be 0-9\n",clk_in_out_id);
				ret=-1;
	}
	return ret;
}
EXPORT_SYMBOL(eth_set_pinmux);

void uart_set_pinmux(int port,int uart_bank)
{
	
	if(port==UART_PORT_A)/*PORT A*/
	{
		switch(uart_bank)
		{
			case UART_A_TMS_TDOI:/*JTAG-TMS/TDI*/
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<10)|(1<<14));
				break;
			case UART_A_GPIO_B2_B3:/*GPIO_B2,B3*//*7266-m_SZ*/
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<11)|(1<<15));
				break;	
			case UART_A_GPIO_C9_C10: //GPIO C9-C10
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<23)|(1<<24));
				break;		
			case UART_A_GPIO_C21_D22:/*6236m_dpf_sz :GPIOC_21_22*/
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_7,((1 << 11) | (1 << 8)));
				break;
			case UART_A_GPIO_D21_D22: //GPIO D21-D22
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<18)|(1<<19));
				break;	
			case UART_A_GPIO_E14_E15://GPIO E14-E15
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<0)|(1<<1));
				break;		
			default:
				printk("UartA pinmux set error\n");
		}
	}
	else	 if(port==UART_PORT_B)/*UART_B*/
	{
		switch(uart_bank)
		{
			case UART_B_TCK_TDO://JTAG=TCK,TDO
			        //CLEAR_CBUS_REG_MASK(PREG_JTAG_GPIO_ADDR,1<<3);
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, 1<<30);
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<18)|(1<<22));
				break;
			case UART_B_GPIO_B0_B1://GPIO+B0.B1
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<4)|(1<<7));
				break;		
			case UART_B_GPIO_C13_C14:	///GPIO_C13_C14
				/*6236-SH*/
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<27)|(1<<30));
				break;	
			case UART_B_GPIO_E18_E19://GPIO+E18.E19
				SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<23)|(1<<24));
				break;		
			default:
				printk("UartB pinmux set error\n");
		}
	}
	else
	{
		printk("Unknow Uart Port%d!(0,1)\n",port);
	}
}

void set_audio_pinmux(int type)
{
		if(type == AUDIO_OUT_JTAG){
				set_mio_mux(1,	
					(1<<6)|(1<<11)|(1<<15)|(1<<19)
					);
		}
		else if(type == AUDIO_OUT_GPIOA){
			set_mio_mux(1, 
					(1<<16)|(1<<11)|(1<<15)|(1<<19)
					);
			set_mio_mux(8, (1<<5));
		}
		else if(type == AUDIO_OUT_TEST_N){
			clear_mio_mux(1, (1<<6));
			clear_mio_mux(0, (1<<17));
			clear_mio_mux(6, (1<<24));
			WRITE_CBUS_REG_BITS(0x200b,0,16,1); //Set TEST_N Output mode
			set_mio_mux(1, (1<<11)|(1<<15)|(1<<19));
			set_mio_mux(0, (1<<18));
		}
		else if(type == AUDIO_IN_JTAG){
			clear_mio_mux(1, (1<<6));
			set_mio_mux(1,(1<<11)|(1<<15)|(1<<19));
			set_mio_mux(8, (1<<8)|(1<<9)|(1<<10));
			set_mio_mux(8, (1<<11));
		}
		else if(type == SPDIF_OUT_GPIOA){
			set_mio_mux(0, (1<<19));
		}
		else if(type == SPDIF_OUT_GPIOB){
			set_mio_mux(2, (1<<1));
		}
		else if(type == SPDIF_OUT_TEST_N){
			set_mio_mux(0, (1<<17));
		}
		else if(type == SPDIF_IN_GPIOA){
			set_mio_mux(0, (1<<20));
		}
		else if(type == SPDIF_IN_GPIOB){
			set_mio_mux(2, (1<<0));
		}
}
EXPORT_SYMBOL(set_audio_pinmux);
