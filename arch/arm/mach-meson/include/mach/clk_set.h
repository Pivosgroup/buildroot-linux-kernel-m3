#ifndef __CLK_SET_HEADER_
#define __CLK_SET_HEADER_
/*
	select clk:
	5,6,7 sata
	4-extern pad
	3-other_pll_clk
	2-ddr_pll_clk
	1-APLL_CLK_OUT_400M
	0----sys_pll_div3 (333~400Mhz)

	clk_freq:50M=50000000
	output_clk:50000000;
	aways,maybe changed for others?
	
*/
#define ETH_CLKSRC_SYS_D3				(0)
#define ETH_CLKSRC_APLL_CLK			(1)
#define ETH_CLKSRC_DDR_CLK				(2)
#define ETH_CLKSRC_OTHER_CLK			(3)
#define ETH_CLKSRC_EXTERN_PAD_CLK		(4)
#define ETH_CLKSRC_SATA_5				(5)
#define ETH_CLKSRC_SATA_6				(6)
#define ETH_CLKSRC_SATA_7				(7)
#define CLK_1M							(1000000)
#define ETH_VALIDE_CLKSRC(clk,out_clk)			((clk%out_clk)==0)
int  eth_clk_set(int selectclk,unsigned long clk_freq,unsigned long out_clk);

int demod_apll_setting(unsigned crystal_req,unsigned out_freq);
int sys_clkpll_setting(unsigned crystal_freq,unsigned out_freq);
unsigned long get_xtal_clock(void);
int other_pll_setting(unsigned crystal_freq,unsigned  out_freq);
int audio_pll_setting(unsigned crystal_freq,unsigned  out_freq);
int video_pll_setting(unsigned crystal_freq,unsigned  out_freq,int powerdown,int flags);


#endif
