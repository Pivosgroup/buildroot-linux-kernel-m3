#include <linux/module.h>
#include <linux/delay.h>
#include <mach/am_regs.h>
#include <mach/clk_set.h>
#include <mach/clock.h>

struct pll_reg_table {
    unsigned  long xtal_clk;
    unsigned  long out_clk;
    unsigned  long settings;
};
unsigned long get_xtal_clock(void)
{
    unsigned long clk;

    clk = READ_CBUS_REG_BITS(PREG_CTLREG0_ADDR, 4, 5);
    clk = clk * 1000 * 1000;
    return clk;
}

/*
Get two number's max common divisor;
*/

static int get_max_common_divisor(int a, int b)
{
    while (b) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}


/*
    select clk:
    7-EXT_XTAL_CLK(25MHz)
    6-VID2_PLL_CLK
    5-VID_PLL_CLK
    4-AUDIO_PLL_CLK
    3-DDR_PLL_CLK
    2-MISC_PLL_CLK
    1-SYS_PLL_CLK
    0-XTAL

    clk_freq:50M=50000000
    output_clk:50000000;
    aways,maybe changed for others?
*/


int eth_clk_set(int selectclk, unsigned long clk_freq, unsigned long out_clk, unsigned int clk_invert)
{
    int n;
    printk("select eth clk-%d,source=%ld,out=%ld\n", selectclk, clk_freq, out_clk);
    if (((clk_freq) % out_clk) != 0) {
        printk(KERN_ERR "ERROR:source clk must n times of out_clk=%ld ,source clk=%ld\n", out_clk, clk_freq);
        return -1;
    } else {
        n = (int)((clk_freq) / out_clk);
    }

    WRITE_CBUS_REG(HHI_ETH_CLK_CNTL,
                   (n - 1) << 0 |
                   selectclk << 9 |
                   ((clk_invert == 1) ? 1 : 0) << 14 | //PAD signal invert
                   1 << 8 //enable clk
                  );

    udelay(100);
    return 0;
}

int auto_select_eth_clk(void)
{
    return -1;
}

//0x065e11ff,0x0249a941, // min current for 750~1300
//0x065e31ff,0xbe49a941, // max current for 500~750
static unsigned pll_setting[17][3]={
    {0x2021a,0x065e11ff,0x0249a941}, 
    {0x20220,0x065e11ff,0x0249a941}, 
    {0x20226,0x065e11ff,0x0249a941}, 
    {0x2022d,0x065e11ff,0x0249a941}, 
    {0x20233,0x065e11ff,0x0249a941}, 
    {0x20239,0x065e31ff,0xbe49a941}, 
    {0x10220,0x065e11ff,0x0249a941}, 
    {0x10223,0x065e11ff,0x0249a941}, 
    {0x10227,0x065e11ff,0x0249a941}, 
    {0x1022a,0x065e11ff,0x0249a941}, 
    {0x1022d,0x065e11ff,0x0249a941}, 
    {0x10230,0x065e11ff,0x0249a941}, 
    {0x10233,0x065e11ff,0x0249a941}, 
    {0x10236,0x065e11ff,0x0249a941}, 
    {0x1023a,0x065e31ff,0xbe49a941}, 
    {0x1023d,0x065e31ff,0xbe49a941}, 
    {0x00220,0x065e11ff,0x0249a941}, 
};

unsigned get_sys_clkpll_setting(unsigned crystal_freq, unsigned out_freq)
{
    unsigned long crys_M, out_M, i;
    if (!crystal_freq)
        crystal_freq = get_xtal_clock();
    crys_M = crystal_freq / 1000000;
    out_M = out_freq / 1000000;
    i = (out_M-200)/50;
    if (i>16) i=16;
    return pll_setting[i][0];
}

int sys_clkpll_setting(unsigned crystal_freq, unsigned out_freq)
{
    unsigned long crys_M, out_M;
    int i;
#if 0
    unsigned long result_freq, target_freq;
    int lock_flag;
    unsigned lock_time=0;
    unsigned long freq_log[64];
    int log_index;
#endif
    unsigned target_pll_setting;

    if (!crystal_freq)
        crystal_freq = get_xtal_clock();
    crys_M = crystal_freq / 1000000;
    out_M = out_freq / 1000000;
    i = (out_M-200)/50;
    if (i>16) i=16;
    target_pll_setting = pll_setting[i][0];
    if (READ_MPEG_REG(HHI_SYS_PLL_CNTL)!=target_pll_setting){
        WRITE_MPEG_REG(HHI_SYS_PLL_CNTL, target_pll_setting); 
        WRITE_MPEG_REG(HHI_SYS_PLL_CNTL2, pll_setting[i][1]); 
        WRITE_MPEG_REG(HHI_SYS_PLL_CNTL3, pll_setting[i][2]);
        WRITE_MPEG_REG(RESET5_REGISTER, (1<<2));        // reset sys pll
#if 0
        lock_flag = 0;
        log_index = 0;
        target_freq = ((target_pll_setting&0x1ff)*crys_M)>>(target_pll_setting>>16);
        for (i=0;i<64;i++){
            result_freq = clk_util_clk_msr(SYS_PLL_CLK);
            if ((result_freq <= target_freq+1)&&(result_freq >= target_freq-1)){
                lock_flag++;
                if (lock_flag>=1)
                    break;
            }
            if (log_index<64) 
                freq_log[log_index++]=result_freq;
            else 
                break;
            lock_time+=64;
        }
        lock_time-=64;
       
        //printk("sys clk changed");
        //for (i=0;i<log_index;i++)
        //    printk("-%d", freq_log[i]);
        //printk("\ncpu_clk_changed: out_freq=%ld,pll_setting=%x,locktime=%dus\n",out_M,target_pll_setting,lock_time);
#else
		udelay(100);
#endif
    }
    return 0;
}

int misc_pll_setting(unsigned crystal_freq, unsigned  out_freq)
{
    int n, m, od;
    unsigned long crys_M, out_M, middle_freq;
    unsigned long flags;
    
    if (!crystal_freq) {
        crystal_freq = get_xtal_clock();
    }
    crys_M = crystal_freq / 1000000;
    out_M = out_freq / 1000000;
    if (out_M < 400) {
        /*if <400M, Od=1*/
        od = 1;/*out=pll_out/(1<<od)*/
        out_M = out_M << 1;
    } else {
        od = 0;
    }

    middle_freq = get_max_common_divisor(crys_M, out_M);
    n = crys_M / middle_freq;
    m = out_M / (middle_freq);
    if (n > (1 << 5) - 1) {
        printk(KERN_ERR "misc_pll_setting  error, n is too bigger n=%d,crys_M=%ldM,out=%ldM\n",
               n, crys_M, out_M);
        return -1;
    }
    if (m > (1 << 9) - 1) {
        printk(KERN_ERR "misc_pll_setting  error, m is too bigger m=%d,crys_M=%ldM,out=%ldM\n",
               m, crys_M, out_M);
        return -2;
    }
    local_irq_save(flags);
    WRITE_MPEG_REG(HHI_OTHER_PLL_CNTL,
                   m |
                   n << 9 |
                   (od & 1) << 16
                  ); // misc PLL
    WRITE_MPEG_REG(RESET5_REGISTER, (1<<1));        // reset misc pll
    WRITE_AOBUS_REG_BITS(AO_UART_CONTROL, (((out_freq/4) / (115200*4)) - 1) & 0xfff, 0, 12);
    local_irq_restore(flags);
	udelay(100);
    return 0;
}


int audio_pll_setting(unsigned crystal_freq, unsigned  out_freq)
{
    int n, m, od;
    unsigned long crys_M, out_M, middle_freq;
    /*
    FIXME:If we need can't exact setting this clock,Can used a pll table?
    */
    if (!crystal_freq) {
        crystal_freq = get_xtal_clock();
    }
    crys_M = crystal_freq / 1000000;
    out_M = out_freq / 1000000;
    if (out_M < 400) {
        /*if <400M, Od=1*/
        od = 1;/*out=pll_out/(1<<od)
                 */
        out_M = out_M << 1;
    } else {
        od = 0;
    }
    middle_freq = get_max_common_divisor(crys_M, out_M);
    n = crys_M / middle_freq;
    m = out_M / (middle_freq);
    if (n > (1 << 5) - 1) {
        printk(KERN_ERR "audio_pll_setting  error, n is too bigger n=%d,crys_M=%ldM,out=%ldM\n",
               n, crys_M, out_M);
        return -1;
    }
    if (m > (1 << 9) - 1) {
        printk(KERN_ERR "audio_pll_setting  error, m is too bigger m=%d,crys_M=%ldM,out=%ldM\n",
               m, crys_M, out_M);
        return -2;
    }
    WRITE_MPEG_REG(HHI_AUD_PLL_CNTL,
                   m |
                   n << 9 |
                   (od & 1) << 14
                  ); // audio PLL
    printk(KERN_INFO "audio_pll_setting to crystal_req=%ld,out_freq=%ld,n=%d,m=%d,od=%d\n", crys_M, out_M / (od + 1), n, m, od);
    return 0;
}

int video_pll_setting(unsigned crystal_freq, unsigned  out_freq, int powerdown, int flags)
{
    int n, m, od;
    unsigned long crys_M, out_M, middle_freq;
    int ret = 0;
    /*
    flags can used for od1/xd settings
    FIXME:If we needn't exact setting this clock,Can used a pll table?
    */
    if (!crystal_freq) {
        crystal_freq = get_xtal_clock();
    }
    crys_M = crystal_freq / 1000000;
    out_M = out_freq / 1000000;

    if (out_M < 750) {
        /* if <750M, Od=1 */
        od = 1;/* out=pll_out/(1<<od) */
        out_M = out_M << 1;
    } else {
        od = 0;
    }
    middle_freq = get_max_common_divisor(crys_M, out_M);
    n = crys_M / middle_freq;
    m = out_M / (middle_freq);
    if (n > (1 << 5) - 1) {
        printk(KERN_ERR "video_pll_setting  error, n is too bigger n=%d,crys_M=%ldM,out=%ldM\n",
               n, crys_M, out_M);
        ret = -1;
    }
    if (m > (1 << 9) - 1) {
        printk(KERN_ERR "video_pll_setting  error, m is too bigger m=%d,crys_M=%ldM,out=%ldM\n",
               m, crys_M, out_M);
        ret = -2;
    }
    if (ret) {
        return ret;
    }
	/* There are some differents between M1 and M3*/
    WRITE_MPEG_REG(HHI_VID_PLL_CNTL,
                   m |
                   n << 10 |
                   (od & 0x3) << 20 |
                   (!!powerdown) << 30 /*is power down mode?*/
                  ); // video PLL
    printk(KERN_INFO "video_pll_setting to crystal_req=%ld,out_freq=%ld,n=%d,m=%d,od=%d\n", crys_M, out_M / (od + 1), n, m, od);
    return 0;
}


