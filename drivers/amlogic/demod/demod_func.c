#include <linux/kernel.h>
#include <linux/i2c.h>
#include "aml_demod.h"
#include "demod_func.h"

static u32 demod_addr_chg(int mode, int addr)
{
    u32 tmp;
    
    switch (mode) {
    case 0 : tmp = DVBC_BASE + addr; break;
    case 1 : tmp = DEMOD_BASE + addr; break;
    case 2 : tmp = DVBT_BASE + addr*4; break;
    case 3 : tmp = IO_CBUS_BASE + addr*4; break; 
    default : tmp = addr;
    }

    return tmp;
}

void apb_write_reg(int mode, int addr, int data)
{
    u32 tmp;

    tmp = demod_addr_chg(mode, addr);
    *(volatile unsigned *)tmp = data;
}

u32 apb_read_reg(int mode, int addr)
{
    u32 tmp, ret;

    tmp = demod_addr_chg(mode, addr);
    ret = *(volatile unsigned *)tmp;
    
    return ret;
}

void demod_set_reg(struct aml_demod_reg *demod_reg)
{
    apb_write_reg(demod_reg->mode, demod_reg->addr, demod_reg->val);
}

void demod_get_reg(struct aml_demod_reg *demod_reg)
{
    demod_reg->val = apb_read_reg(demod_reg->mode, demod_reg->addr);
}

void demod_msr_clk(int clk_mux)
{
    u32 tmp;
    int clk_freq;

    tmp = Rd(MSR_CLK_REG0);
    tmp &= ~((0xf<<20) | (3<<17) | 0xffff);
    Wr(MSR_CLK_REG0, tmp | (clk_mux<<20) | (1<<19) | (1<<16) | (1000-1));
    
    while (Rd(MSR_CLK_REG0)>>31&1);

    tmp = Rd(MSR_CLK_REG0);
    tmp &= ~(1<<16);
    Wr(MSR_CLK_REG0, tmp);

    tmp = Rd(MSR_CLK_REG2);
    clk_freq = tmp;  // 1Mhz/1000uS

    printk("Demod Clk%d measured %d kHz\n", clk_mux, clk_freq);
}

void demod_calc_clk(struct aml_demod_sta *demod_sta)
{
    u8 adc_en;
    u8 clk_en, clk_src, clk_div, pll_n, pll_od, pll_sys_xd, pll_adc_xd;
    u16 pll_m;
    u32 tmp;
    int adc_freq, clk_freq;
    
    tmp = (*DEMOD_REG1);
    adc_en = tmp>>31&1;

    tmp = (*P_HHI_DEMOD_CLK_CNTL);
    clk_src = tmp>>9&3;
    clk_en  = tmp>>8&1;
    clk_div = tmp&0x7f;

    tmp = (*P_HHI_DEMOD_PLL_CNTL);
    pll_n = tmp>>9&0x1f;
    pll_m = tmp&0x1ff;
    pll_od = (tmp>>16&0x7f) ? 2 : 1;

    tmp = (*P_HHI_DEMOD_PLL_CNTL2);
    pll_adc_xd = tmp>>21&0x1f;

    tmp = (*P_HHI_DEMOD_PLL_CNTL3);
    pll_sys_xd = tmp>>16&0x1f;

    clk_freq = 24000 * pll_m / pll_n / pll_od / pll_sys_xd;
    adc_freq = 24000 * pll_m / pll_n / pll_od / pll_adc_xd / 2;

    if (clk_src == 1) {
	clk_freq = 396000;
    }
    else if (clk_src == 2) {
	clk_freq = 540000;
    }
    else if (clk_src == 3) {
	clk_freq = 300000;
    }
	 
    clk_freq = clk_freq / (clk_div + 1);

    demod_sta->clk_en = clk_en;
    demod_sta->adc_en = adc_en;
    demod_sta->clk_freq = clk_freq;
    demod_sta->adc_freq = adc_freq;

    if (demod_sta->debug) {
	printk("Demod SYS Clock %d kHz %3s\n", clk_freq, clk_en?"ON ":"OFF");
	printk("Demod ADC Clock %d kHz %3s\n", adc_freq, adc_en?"ON ":"OFF");
    }
}

static int demod_set_agc(struct aml_demod_sys *demod_sys)
{
    u8 agc_sel;
    int ret = 0;

    agc_sel = demod_sys->agc_sel;

    if (agc_sel == 1) 
	(*P_PERIPHS_PIN_MUX_6) |= (3<<30);
    else if (agc_sel == 2)
	(*P_PERIPHS_PIN_MUX_2) |= (3<<28); 
    else
	ret = -1;

    return ret;
}

static void demod_get_agc(struct aml_demod_sys *demod_sys)
{
    u8 agc_sel = 0;
    u32 tmp;

    tmp = (*P_PERIPHS_PIN_MUX_6);
    if ((tmp>>30&3) == 3) agc_sel = 1;

    tmp = (*P_PERIPHS_PIN_MUX_2);
    if ((tmp>>28&3) == 3) agc_sel = 2;

    demod_sys->agc_sel = agc_sel;
}

static void demod_set_adc(struct aml_demod_sys *demod_sys)
{
    u8 adc_en;
    u32 tmp;

    adc_en = demod_sys->adc_en;

    tmp = (*DEMOD_REG1);
    tmp &= ~((0xf<<28) | (7<<7) | (1<<6));
    (*DEMOD_REG1) = tmp | (adc_en<<31) | (1<<7) | (1<<6); 
}

static void demod_get_adc(struct aml_demod_sys *demod_sys)
{
    u8 adc_en;
    u32 tmp;

    tmp = (*DEMOD_REG1);
    adc_en = tmp>>31&1;

    demod_sys->adc_en = adc_en;
}

static void demod_set_clk(struct aml_demod_sys *demod_sys)
{
    u8 clk_en, clk_src, clk_div, pll_n, pll_od, pll_sys_xd, pll_adc_xd;
    u16 pll_m;
    u32 tmp;

    clk_en =     demod_sys->clk_en;
    clk_src =    demod_sys->clk_src;
    clk_div =    demod_sys->clk_div;
    pll_n =      demod_sys->pll_n;
    pll_m =      demod_sys->pll_m;
    pll_od =     demod_sys->pll_od;
    pll_sys_xd = demod_sys->pll_sys_xd;
    pll_adc_xd = demod_sys->pll_adc_xd;

    tmp = (*P_HHI_DEMOD_PLL_CNTL);
    tmp &= ~((0x7f<<16) | (0x1f<<9) | 0x1ff);
    (*P_HHI_DEMOD_PLL_CNTL) = tmp | (pll_od<<16) | (pll_n<<9) | pll_m;

    tmp = (*P_HHI_DEMOD_PLL_CNTL2);
    tmp &= ~(0x1f<<21);
    (*P_HHI_DEMOD_PLL_CNTL2) = tmp | (pll_adc_xd<<21);

    tmp = (*P_HHI_DEMOD_PLL_CNTL3);
    tmp &= ~((0x1f<<16) | (0xf<<12));
    (*P_HHI_DEMOD_PLL_CNTL3) = tmp | (pll_sys_xd<<16) | (2<<12);

    tmp = (*P_HHI_DEMOD_CLK_CNTL);
    tmp &= ~((3<<9) | (1<<8) | 0x7f);
    (*P_HHI_DEMOD_CLK_CNTL) = tmp | (clk_src<<9) | (clk_en<<8) | clk_div;
}

static void demod_get_clk(struct aml_demod_sys *demod_sys)
{
    u8 clk_en, clk_src, clk_div, pll_n, pll_od, pll_sys_xd, pll_adc_xd;
    u16 pll_m;
    u32 tmp;
    
    tmp = (*P_HHI_DEMOD_CLK_CNTL);
    clk_src = tmp>>9&3;
    clk_en  = tmp>>8&1;
    clk_div = tmp&0x7f;

    tmp = (*P_HHI_DEMOD_PLL_CNTL);
    pll_n = tmp>>9&0x1f;
    pll_m = tmp&0x1ff;
    pll_od = tmp>>16&0x7f;

    tmp = (*P_HHI_DEMOD_PLL_CNTL2);
    pll_adc_xd = tmp>>21&0x1f;

    tmp = (*P_HHI_DEMOD_PLL_CNTL3);
    pll_sys_xd = tmp>>16&0x1f;

    demod_sys->clk_en     = clk_en;
    demod_sys->clk_src    = clk_src;
    demod_sys->clk_div    = clk_div;
    demod_sys->pll_n      = pll_n;
    demod_sys->pll_m      = pll_m;
    demod_sys->pll_od     = pll_od;
    demod_sys->pll_sys_xd = pll_sys_xd;
    demod_sys->pll_adc_xd = pll_adc_xd;
}

static void printk_sys(struct aml_demod_sys *demod_sys)
{
    printk("sys.clk_en     %d\n", demod_sys->clk_en);
    printk("sys.clk_src    %d\n", demod_sys->clk_src);       
    printk("sys.clk_div    %d\n", demod_sys->clk_div);       
    printk("sys.pll_n      %d\n", demod_sys->pll_n);     
    printk("sys.pll_m      %d\n", demod_sys->pll_m);     
    printk("sys.pll_od     %d\n", demod_sys->pll_od);
    printk("sys.pll_sys_xd %d\n", demod_sys->pll_sys_xd);
    printk("sys.pll_adc_xd %d\n", demod_sys->pll_adc_xd);
    printk("sys.agc_sel    %d\n", demod_sys->agc_sel);
    printk("sys.adc_en     %d\n", demod_sys->adc_en);
}

static void printk_i2c(struct aml_demod_i2c *adap)
{
    printk("i2c.tuner   %d\n",     adap->tuner  );    
    printk("i2c.addr    0x%02x\n", adap->addr   );
    printk("i2c.scl_oe  0x%08x\n", adap->scl_oe );
    printk("i2c.scl_out 0x%08x\n", adap->scl_out);
    printk("i2c.scl_in  0x%08x\n", adap->scl_in );
    printk("i2c.scl_bit %d\n",     adap->scl_bit);
    printk("i2c.sda_oe  0x%08x\n", adap->sda_oe );
    printk("i2c.sda_out 0x%08x\n", adap->sda_out);
    printk("i2c.sda_in  0x%08x\n", adap->sda_in );
    printk("i2c.sda_bit %d\n",     adap->sda_bit);
    printk("i2c.udelay  %d\n",     adap->udelay );
    printk("i2c.retries %d\n",     adap->retries);
}

int demod_set_sys(struct aml_demod_sta *demod_sta, 
		  struct aml_demod_i2c *demod_i2c, 
		  struct aml_demod_sys *demod_sys)
{
    int ret = 0;
    struct i2c_adapter *adapter;

    if (demod_sys->debug) printk_sys(demod_sys);

    demod_set_clk(demod_sys);
    demod_set_adc(demod_sys);
    if (demod_set_agc(demod_sys) < 0) {
	printk("Error: Demod Set AGC, Invalid AGC option.\n");
	ret = -1;
    }
    
    demod_sta->debug = demod_sys->debug;
    demod_calc_clk(demod_sta);

    *demod_i2c = *(struct aml_demod_i2c *)demod_sys->i2c;
    if (demod_i2c->debug) printk_i2c(demod_i2c);

    /*in debug mode, setup i2c here, i2c_id is in demod_i2c->scl_out*/
    if ((demod_i2c->scl_oe ==0) && (demod_i2c->sda_oe == 1)) {
		printk("[amlfe: debug mode, i2c id = %d]\n", demod_i2c->scl_out);
		adapter = i2c_get_adapter(demod_i2c->scl_out);
		demod_i2c->i2c_id = demod_i2c->scl_out;
		demod_i2c->i2c_priv = adapter;
		if(!adapter){
			printk("can not get i2c adapter[%d] \n", demod_i2c->scl_out);
			return -1;
		}
	}

/*    if (demod_i2c->scl_oe ==0 || 
	demod_i2c->scl_out==0 || 
	demod_i2c->scl_in== 0 ||
	demod_i2c->sda_oe ==0 || 
	demod_i2c->sda_out==0 || 
	demod_i2c->sda_in ==0) {
	demod_i2c->tuner = 0;
	ret = -1;
    }
    else {
	//aml_i2c_sw_test_bus(demod_i2c, "i2c");
*/	
	if (demod_i2c->tuner == 3)
	    init_tuner_fj2207(demod_sta, demod_i2c);
/*    }
*/

    return ret;
}

int demod_get_sys(struct aml_demod_i2c *demod_i2c, 
		  struct aml_demod_sys *demod_sys)
{
    *(struct aml_demod_i2c *)demod_sys->i2c = *demod_i2c;
    demod_get_clk(demod_sys);
    demod_get_adc(demod_sys);
    demod_get_agc(demod_sys);
    
    printk_sys(demod_sys);

    return 0;
}

int demod_turn_on(struct aml_demod_sta *demod_sta, 
		  struct aml_demod_sys *demod_sys)
{
    int ret = 0;

    demod_sys->clk_en = 1;
    demod_sys->adc_en = 1;

    demod_set_clk(demod_sys);
    demod_set_adc(demod_sys);
    
    demod_calc_clk(demod_sta);

    return ret;
}

int demod_turn_off(struct aml_demod_sta *demod_sta, 
		   struct aml_demod_sys *demod_sys)
{
    int ret = 0;

    demod_sys->clk_en = 0;
    demod_sys->adc_en = 0;

    demod_set_adc(demod_sys);
    demod_set_clk(demod_sys);
    
    demod_calc_clk(demod_sta);

    return ret;
}

