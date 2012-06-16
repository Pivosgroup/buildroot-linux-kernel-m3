/*
 *
 * arch/arm/mach-meson/sataclock.c
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
#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/memory.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/lm.h>


static const char * clock_src_name[]={
		"XTAL input",
		"DDR PLL output",
		"other PLL output",
		"Demod 400M PLL output"
};
int set_sata_phy_clk(int sel)
{

    int i;
    int time_dly = 50000;

	/*==========Set Demod PLL, should be in system setting===========*/
	//Set 1.2G PLL
	CLEAR_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL,0xFFFFFFFF);
	SET_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL,0x232);// 24M XTAL

	//Set 400M PLL
	CLEAR_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL3,0xFFFF0000);
	SET_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL3,0x0C850000);//400M 300M 240M
	
	/*=======================================*/

	// ------------------------------------------------------------
	//  CLK_SEL: These bits select the source for the 12Mhz: 
	// 0 = XTAL input 
	// 1 = DDR PLL output
	// 2 = other PLL output
	// 3 = demod 400Mhz PLL output
	CLEAR_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, (3 << SATA_PHY_CLK_SEL_OFFSET));

	
	printk(KERN_NOTICE"SATA PHY clock souce: %s\n",clock_src_name[sel]);
	SET_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, (sel << SATA_PHY_CLK_SEL_OFFSET ));

	CLEAR_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, ( 0x7F << SATA_PHY_CLK_DIV));
	switch(sel)
	{
		case SATA_PHY_CLOCK_SEL_XTAL:
			//XTAL 25M, 
			SET_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, (0 << SATA_PHY_CLK_DIV));
			break;
			
		case SATA_PHY_CLOCK_SEL_DDR_PLL:
			//DDR 400M, Divide by (400/(7+1)=50)
			//SET_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, (1 << SATA_PHY_CLK_DIV));
			printk("Can't use this sel for SATA pll clock!\n");
			break;
			
		case SATA_PHY_CLOCK_SEL_OTHER_PLL:
			//Other PLL running at 540M (540/(44+1)=12)
			//SET_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, (44 << SATA_PHY_CLK_DIV));
			printk("Can't use this sel for SATA pll clock!\n");
			break;
			
		case SATA_PHY_CLOCK_SEL_DEMOD_PLL:
			// demod 400M (400/(15+1) = 25)
			SET_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, (7 << SATA_PHY_CLK_DIV));
			break;
	}
#ifdef CONFIG_MACH_MESON_8726M_DVBC
	SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_11, (0xF << 2));
#else
	SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_11, (0xF << 6));
#endif
	i=0;
	while(i++<time_dly){};
	SET_CBUS_REG_MASK(RESET3_REGISTER, (1 << 1));
	i=0;
	while(i++<time_dly){};
	CLEAR_CBUS_REG_MASK(PREG_SATA_REG2, (3 << 12));
	i=0;
	while(i++<time_dly){};
	//SET_CBUS_REG_MASK(PREG_SATA_REG2, (2 << 12));
	//i=0;
	//while(i++<time_dly){};

	// Enable clock
	SET_CBUS_REG_MASK(HHI_SATA_CLK_CNTL, (1 << SATA_PHY_CLK_ENABLE));
	return 0;
}

EXPORT_SYMBOL(set_sata_phy_clk);
