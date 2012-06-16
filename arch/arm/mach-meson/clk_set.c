#include <linux/module.h>
#include <linux/delay.h>
#include <mach/am_regs.h>
#include <mach/clk_set.h>

struct pll_reg_table
{
unsigned  long xtal_clk;
unsigned  long out_clk;
unsigned  long settings;	 
};
unsigned long get_xtal_clock(void)
{
	unsigned long clk;
	
	clk=READ_CBUS_REG_BITS(PREG_CTLREG0_ADDR,4,5);
	clk=clk*1000*1000;
	return clk;
}

/*
Get two number's max common divisor;
*/

static int get_max_common_divisor(int a,int b){
        while(b){
                int temp=b;
                b=a%b;
                a=temp;
        }
        return a;
}


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


int  eth_clk_set(int selectclk,unsigned long clk_freq,unsigned long out_clk)
{
	int n;
	printk("select eth clk-%d,source=%ld,out=%ld\n",selectclk,clk_freq,out_clk);
	if(((clk_freq)%out_clk)!=0)
		{
			printk(KERN_ERR "ERROR:source clk must n times of out_clk=%ld ,source clk=%ld\n",out_clk,clk_freq);
			return -1;
		}
	else
		{
			n=(int)((clk_freq)/out_clk);
		}
	
	WRITE_CBUS_REG(HHI_ETH_CLK_CNTL,
		(n-1)<<0 |
		selectclk<<9 |
		1<<8//enable clk
		); 
	
	//writel(0x70b,(0xc1100000+0x1076*4));  // enable Ethernet clocks   for other clock 600/12 
	//writel(0x107,(0xc1100000+0x1076*4));  // enable Ethernet clocks   for sys clock 1200/3/8
	udelay(100);
	return 0;
}
int auto_select_eth_clk(void)
{
	return -1;
}

/*
out_freq=crystal_req*m/n
out_freq=crystal_req*m/n-->
n=crystal_req*m/out_freq
m=out_freq*n/crystal_req
*/
int demod_apll_setting(unsigned crystal_req,unsigned out_freq)
{
	int n,m;
	unsigned long crys_M,out_M,middle_freq;
	if(!crystal_req)  crystal_req=get_xtal_clock();
	crys_M=crystal_req/1000000;
	out_M=out_freq/1000000;
	middle_freq=get_max_common_divisor(crys_M,out_M);
	n=crys_M/middle_freq;
	m=out_M/(middle_freq);
	
	if(n>(1<<5)-1)
		{
		printk(KERN_ERR "demod_apll_setting setting error, n is too bigger n=%d,crys_M=%ldM,out_M=%ldM\n",
			   n,crys_M,out_M);
		return -1;
		}
	if(m>(1<<9)-1)
		{
		printk(KERN_ERR "demod_apll_setting setting error, m is too bigger m=%d,crys_M=%ldM,out_M=%ldM\n",
			   m,crys_M,out_M);
		return -2;
		}
	printk("demod_apll_setting crystal_req=%ld,out_freq=%ld,n=%d,m=%dM\n",crys_M,out_M,n,m);
	/*==========Set Demod PLL, should be in system setting===========*/
	//Set 1.2G PLL
	CLEAR_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL,0xFFFFFFFF);
	SET_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL,n<<9 | m<< 0);//

	//Set 400M PLL
	CLEAR_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL3,0xFFFF0000);
	SET_CBUS_REG_MASK(HHI_DEMOD_PLL_CNTL3,0x0C850000);//400M 300M 240M enable
	return 0;
	
}

int sys_clkpll_setting(unsigned crystal_freq,unsigned  out_freq)
{
	int n,m;
	unsigned long crys_M,out_M,middle_freq,flags;
	if(!crystal_freq)  crystal_freq=get_xtal_clock();
	crys_M=crystal_freq/1000000;
	out_M=out_freq/1000000;
	middle_freq=get_max_common_divisor(crys_M,out_M);
	n=crys_M/middle_freq;
	m=out_M/(middle_freq);
	if(n>(1<<5)-1)
	{
//		printk(KERN_ERR "sys_clk_setting  error, n is too bigger n=%d,crys_M=%ldM,out=%ldM\n",n,crys_M,out_M);
		return -1;
	}
	if(m>(1<<9)-1)
	{
//		printk(KERN_ERR "sys_clk_setting  error, m is too bigger m=%d,crys_M=%ldM,out=%ldM\n",m,crys_M,out_M);
		return -2;
	}
	if(out_freq>1600*CLK_1M ||out_freq<400*CLK_1M)
	{
		printk(KERN_WARNING"sys_clk_setting  warning,VCO may no support out_freq,crys_M=%ldM,out=%ldM\n",crys_M,out_M);
	}
//	printk(KERN_INFO "a9_clk_setting crystal_req=%ld,out_freq=%ld,n=%d,m=%d\n",crys_M,out_M,n,m);
	local_irq_save(flags);
	WRITE_MPEG_REG(HHI_SYS_PLL_CNTL, m<<0 | n<<9); // system PLL
	WRITE_MPEG_REG(HHI_A9_CLK_CNTL, // A9 clk set to system clock/2
                        (0 << 10) | // 0 - sys_pll_clk, 1 - audio_pll_clk
                        (1 << 0 ) | // 1 - sys/audio pll clk, 0 - XTAL
                        (1 << 4 ) | // APB_CLK_ENABLE
                        (1 << 5 ) | // AT_CLK_ENABLE
                        (0 << 2 ) | // div1
                        (1 << 7 )); // Connect A9 to the PLL divider output
	udelay(10);
	local_irq_restore(flags);
	return 0;
}

int other_pll_setting(unsigned crystal_freq,unsigned  out_freq)
{
	int n,m,od;
	unsigned long crys_M,out_M,middle_freq;
	if(!crystal_freq)  crystal_freq=get_xtal_clock();
	crys_M=crystal_freq/1000000;
	out_M=out_freq/1000000;
	if(out_M<400)
		{/*if <400M, Od=1*/
			od=1;/*out=pll_out/(1<<od)
				 */
			out_M=out_M<<1;	
		}
	else
		{
			od=0;
		}

	middle_freq=get_max_common_divisor(crys_M,out_M);
	n=crys_M/middle_freq;
	m=out_M/(middle_freq);
	if(n>(1<<5)-1)
	{
		printk(KERN_ERR "other_pll_setting  error, n is too bigger n=%d,crys_M=%ldM,out=%ldM\n",
		n,crys_M,out_M);
		return -1;
	}
	if(m>(1<<9)-1)
	{
		printk(KERN_ERR "other_pll_setting  error, m is too bigger m=%d,crys_M=%ldM,out=%ldM\n",
		m,crys_M,out_M);
		return -2;
	}
	WRITE_MPEG_REG(HHI_OTHER_PLL_CNTL,
					m|
					n<<9 |
					(od&1)<<16
					); // other PLL
	printk(KERN_INFO "other pll setting to crystal_req=%ld,out_freq=%ld,n=%d,m=%d,od=%d\n",crys_M,out_M/(od+1),n,m,od);	
	return 0;
}

	
int audio_pll_setting(unsigned crystal_freq,unsigned  out_freq)
{
	int n,m,od;
	unsigned long crys_M,out_M,middle_freq;
	/*
	FIXME:If we need can't exact setting this clock,Can used a pll table?
	*/
	if(!crystal_freq)  crystal_freq=get_xtal_clock();
	crys_M=crystal_freq/1000000;
	out_M=out_freq/1000000;
	if(out_M<400)
		{/*if <400M, Od=1*/
			od=1;/*out=pll_out/(1<<od)
				 */
			out_M=out_M<<1;	
		}
	else
		{
			od=0;
		}
	middle_freq=get_max_common_divisor(crys_M,out_M);
	n=crys_M/middle_freq;
	m=out_M/(middle_freq);
	if(n>(1<<5)-1)
	{
		printk(KERN_ERR "audio_pll_setting  error, n is too bigger n=%d,crys_M=%ldM,out=%ldM\n",
		n,crys_M,out_M);
		return -1;
	}
	if(m>(1<<9)-1)
	{
		printk(KERN_ERR "audio_pll_setting  error, m is too bigger m=%d,crys_M=%ldM,out=%ldM\n",
		m,crys_M,out_M);
		return -2;
	}
	WRITE_MPEG_REG(HHI_AUD_PLL_CNTL,
					m|
					n<<9 |
					(od&1)<<14
					); // other PLL
	printk(KERN_INFO "audio_pll_setting to crystal_req=%ld,out_freq=%ld,n=%d,m=%d,od=%d\n",crys_M,out_M/(od+1),n,m,od);	
	return 0;
}

int video_pll_setting(unsigned crystal_freq,unsigned  out_freq,int powerdown,int flags)
{
	int n,m,od;
	unsigned long crys_M,out_M,middle_freq;
	int ret=0;
	/*
	flags can used for od1/xd settings
	FIXME:If we need can't exact setting this clock,Can used a pll table?
	*/
	if(!crystal_freq)  crystal_freq=get_xtal_clock();
	crys_M=crystal_freq/1000000;
	out_M=out_freq/1000000;

	if(out_M<400)
		{/*if <400M, Od=1*/
			od=1;/*out=pll_out/(1<<od)
				 */
			out_M=out_M<<1;	
		}
	else
		{
			od=0;
		}
	middle_freq=get_max_common_divisor(crys_M,out_M);
	n=crys_M/middle_freq;
	m=out_M/(middle_freq);
	if(n>(1<<5)-1)
	{
		printk(KERN_ERR "video_pll_setting  error, n is too bigger n=%d,crys_M=%ldM,out=%ldM\n",
		n,crys_M,out_M);
		ret=-1;
	}
	if(m>(1<<9)-1)
	{
		printk(KERN_ERR "video_pll_setting  error, m is too bigger m=%d,crys_M=%ldM,out=%ldM\n",
		m,crys_M,out_M);
		ret=-2;
	}
	if(ret)
		return ret;
	WRITE_MPEG_REG(HHI_VID_PLL_CNTL,
					m|
					n<<9 |
					(od&1)<<16 |
					(!!powerdown)<<15 /*is power down mode?*/
					); // other PLL
	printk(KERN_INFO "video_pll_setting to crystal_req=%ld,out_freq=%ld,n=%d,m=%d,od=%d\n",crys_M,out_M/(od+1),n,m,od);	
	return 0;
}


