#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "aml_demod.h"
#include "demod_func.h"

static u32 dvbc_get_ch_power(void)
{
    u32 tmp;
    u32 ad_power;
    u32 agc_gain;
    u32 ch_power;

    tmp = apb_read_reg(0, 0x09c);

    ad_power = (tmp>>22)&0x1ff;
    agc_gain = (tmp>>0)&0x7ff;

    ad_power = ad_power>>4;
    // ch_power = lookuptable(agc_gain) + ad_power; TODO
    ch_power = (ad_power & 0xffff) + ((agc_gain & 0xffff) << 16) ;

    return ch_power;
}

static u32 dvbc_get_snr(void)
{
    u32 tmp, snr;

    tmp = apb_read_reg(0, 0x14)&0xfff;
    snr = tmp * 100 / 32;  // * 1e2
    
    return snr;
}

static u32 dvbc_get_ber(void)
{
    u32 rs_ber;
    u32 rs_packet_len;

    rs_packet_len = apb_read_reg(0, 0x10)&0xffff;
    rs_ber = apb_read_reg(0, 0x14)>>12&0xfffff;
    
    // rs_ber = rs_ber / 204.0 / 8.0 / rs_packet_len;
    if(rs_packet_len == 0)
	rs_ber = 1000000;
    else
	rs_ber = rs_ber * 613 / rs_packet_len;  // 1e-6
    
    return rs_ber;
}

static u32 dvbc_get_per(void)
{
    u32 rs_per;
    u32 rs_packet_len;

    rs_packet_len = apb_read_reg(0, 0x10)&0xffff;
    rs_per = apb_read_reg(0, 0x18)>>16&0xffff;

    u32 acc_rs_per_times = apb_read_reg(0, 0xcc)&0xffff;
    //rs_per = rs_per / rs_packet_len;
    
    if(rs_packet_len == 0)
	rs_per = 10000;
    else
	rs_per = 10000 * rs_per / rs_packet_len;  // 1e-4

    //return rs_per;
    return acc_rs_per_times;
}

static u32 dvbc_get_symb_rate(void)
{
    u32 tmp;
    u32 adc_freq;
    u32 symb_rate;

    adc_freq = apb_read_reg(0, 0x34)>>16&0xffff;
    tmp = apb_read_reg(0, 0xb8);

    if((tmp>>15) == 0)
	symb_rate = 0;
    else
	symb_rate = 10 * (adc_freq<<12) / (tmp>>15); // 1e4 
    
    return symb_rate;
}

static int dvbc_get_freq_off(void)
{
    int tmp;
    int symb_rate;
    int freq_off;

    symb_rate = dvbc_get_symb_rate();
    tmp = apb_read_reg(0, 0xe0)&0x3fffffff;
    if (tmp>>29&1) tmp -= (1<<30);
    
    freq_off = ((tmp>>16) * 25 * (symb_rate>>10)) >> 3;

    return freq_off;
}

static void dvbc_set_test_bus(u8 sel)
{
    u32 tmp;

    tmp = apb_read_reg(0, 0x08);
    tmp &= ~(0x1f<<4);
    tmp |= ((sel&0x1f)<<4) | (1<<3);
    apb_write_reg(0, 0x08, tmp);
}

void dvbc_get_test_out(u8 sel, u32 len, u32 *buf)
{
    int i, cnt;

    dvbc_set_test_bus(sel);

    for (i=0, cnt=0; i<len-4 && cnt<1000000; i++) {
	buf[i] = apb_read_reg(0, 0xb0);
	if (buf[i]>>11&1) {
	    buf[i++] = apb_read_reg(0, 0xb0);
	    buf[i++] = apb_read_reg(0, 0xb0);
	    buf[i++] = apb_read_reg(0, 0xb0);
	    buf[i++] = apb_read_reg(0, 0xb0);
	}
	else {
	    i--;
	}

	cnt++;
    }
}

static void dvbc_sw_reset(int addr, int idx)
{
    u32 tmp;
    
    tmp = apb_read_reg(0, addr);
    
    tmp &= ~(1<<idx);
    apb_write_reg(0, addr, tmp);

    udelay(1);

    tmp |= (1<<idx);
    apb_write_reg(0, addr, tmp);
}

static void dvbc_reset(void)
{
    dvbc_sw_reset(0x04, 0);
}

static void dvbc_eq_reset(void)
{
    dvbc_sw_reset(0x50, 3);
}

static void dvbc_eq_smma_reset(void)
{
    dvbc_sw_reset(0xe8, 0);
}

static void dvbc_reg_initial(struct aml_demod_sta *demod_sta)
{
    u32 clk_freq;   
    u32 adc_freq;   
    u8  tuner;
    u8  ch_mode;    
    u8  agc_mode;   
    u32 ch_freq;    
    u16 ch_if;      
    u16 ch_bw;      
    u16 symb_rate;  
    u32 phs_cfg;
    int afifo_ctr;

    clk_freq  = demod_sta->clk_freq ; // kHz    
    adc_freq  = demod_sta->adc_freq ; // kHz
    tuner     = demod_sta->tuner    ;
    ch_mode   = demod_sta->ch_mode  ;  
    agc_mode  = demod_sta->agc_mode ; 
    ch_freq   = demod_sta->ch_freq  ; // kHz 
    ch_if     = demod_sta->ch_if    ; // kHz   
    ch_bw     = demod_sta->ch_bw    ; // kHz   
    symb_rate = demod_sta->symb_rate; // k/sec

    // disable irq
    apb_write_reg(0, 0xd0, 0);

    // reset
    dvbc_reset();
    
    apb_write_reg(0, 0x000, 0x00000000);  // QAM_STATUS
    apb_write_reg(0, 0x004, 0x00000f00);  // QAM_GCTL0
    apb_write_reg(0, 0x008, (ch_mode&7));  // qam mode

    switch (ch_mode) {
    case 0 : // 16 QAM
       	apb_write_reg(0, 0x054, 0x23460224);  // EQ_FIR_CTL, 
       	apb_write_reg(0, 0x068, 0x00c000c0);  // EQ_CRTH_SNR
       	apb_write_reg(0, 0x074,  0x50001a0);  // EQ_TH_LMS  40db  13db
       	apb_write_reg(0, 0x07c, 0x003001e9);  // EQ_NORM and EQ_TH_MMA
       	//apb_write_reg(0, 0x080, 0x000be1ff);  // EQ_TH_SMMA0
       	apb_write_reg(0, 0x080, 0x000e01fe);  // EQ_TH_SMMA0
       	apb_write_reg(0, 0x084, 0x00000000);  // EQ_TH_SMMA1
       	apb_write_reg(0, 0x088, 0x00000000);  // EQ_TH_SMMA2
       	apb_write_reg(0, 0x08c, 0x00000000);  // EQ_TH_SMMA3
       	//apb_write_reg(0, 0x094, 0x7f800d2b);  // AGC_CTRL  ALPS tuner
       	//apb_write_reg(0, 0x094, 0x7f80292b);  // Pilips Tuner
       	//apb_write_reg(0, 0x094, 0x7f80292d);  // Pilips Tuner    
       	apb_write_reg(0, 0x094, 0x7f80092d);  // Pilips Tuner    
	break;
    
    case 1 : // 32 QAM
       	apb_write_reg(0, 0x054, 0x24560506);  // EQ_FIR_CTL, 
       	apb_write_reg(0, 0x068, 0x00c000c0);  // EQ_CRTH_SNR
       	//apb_write_reg(0, 0x074, 0x5000260);   // EQ_TH_LMS  40db  19db
       	apb_write_reg(0, 0x074,  0x50001f0);  // EQ_TH_LMS  40db  17.5db
       	apb_write_reg(0, 0x07c, 0x00500102);  // EQ_TH_MMA  0x000001cc
       	apb_write_reg(0, 0x080, 0x00077140);  // EQ_TH_SMMA0
       	apb_write_reg(0, 0x084, 0x001fb000);  // EQ_TH_SMMA1
       	apb_write_reg(0, 0x088, 0x00000000);  // EQ_TH_SMMA2
       	apb_write_reg(0, 0x08c, 0x00000000);  // EQ_TH_SMMA3 	
       	//apb_write_reg(0, 0x094, 0x7f800d2b);  // AGC_CTRL  ALPS tuner
       	//apb_write_reg(0, 0x094, 0x7f80292b);  // Pilips Tuner
       	apb_write_reg(0, 0x094, 0x7f80092b);  // Pilips Tuner
	break;
    
    case 2 : // 64 QAM
	apb_write_reg(0, 0x054, 0x2256033a);  // EQ_FIR_CTL, 
	apb_write_reg(0, 0x068, 0x00c000c0);  // EQ_CRTH_SNR
	//apb_write_reg(0, 0x074, 0x5000260);  // EQ_TH_LMS  40db  19db
	apb_write_reg(0, 0x074,  0x5000230);  // EQ_TH_LMS  40db  17.5db
	apb_write_reg(0, 0x07c, 0x007001bd);  // EQ_TH_MMA
	apb_write_reg(0, 0x080, 0x000580ed);  // EQ_TH_SMMA0
	apb_write_reg(0, 0x084, 0x001771fb);  // EQ_TH_SMMA1
	apb_write_reg(0, 0x088, 0x00000000);  // EQ_TH_SMMA2
	apb_write_reg(0, 0x08c, 0x00000000);  // EQ_TH_SMMA3
	//apb_write_reg(0, 0x094, 0x7f800d2c); // AGC_CTRL  ALPS tuner
	//apb_write_reg(0, 0x094, 0x7f80292c); // Pilips & maxlinear Tuner
	apb_write_reg(0, 0x094, 0x7f802b3d);  // Pilips Tuner & maxlinear Tuner
	//apb_write_reg(0, 0x094, 0x7f802b3a);  // Pilips Tuner & maxlinear Tuner
	break;
    
    case 3 : // 128 QAM
	apb_write_reg(0, 0x054, 0x2557046a);  // EQ_FIR_CTL, 
	apb_write_reg(0, 0x068, 0x00c000d0);  // EQ_CRTH_SNR
	// apb_write_reg(0, 0x074, 0x02440240);  // EQ_TH_LMS  18.5db  18db  
	// apb_write_reg(0, 0x074, 0x04000400);  // EQ_TH_LMS  22db  22.5db
	apb_write_reg(0, 0x074,  0x5000260);  // EQ_TH_LMS  40db  19db
	apb_write_reg(0, 0x07c, 0x00b000f2);  // EQ_TH_MMA0x000000b2
	apb_write_reg(0, 0x080, 0x0003a09d);  // EQ_TH_SMMA0
	apb_write_reg(0, 0x084, 0x000f8150);  // EQ_TH_SMMA1
	apb_write_reg(0, 0x088, 0x001a51f8);  // EQ_TH_SMMA2
	apb_write_reg(0, 0x08c, 0x00000000);  // EQ_TH_SMMA3  
	//apb_write_reg(0, 0x094, 0x7f800d2c);  // AGC_CTRL  ALPS tuner
	//apb_write_reg(0, 0x094, 0x7f80292c);  // Pilips Tuner
	apb_write_reg(0, 0x094, 0x7f80092c);  // Pilips Tuner
	break;
	
    case 4 : // 256 QAM
	apb_write_reg(0, 0x054, 0xa2580588);  // EQ_FIR_CTL,
	apb_write_reg(0, 0x068, 0x01e00220);  // EQ_CRTH_SNR
	apb_write_reg(0, 0x074,  0x50002a0);  // EQ_TH_LMS  40db  19db
	apb_write_reg(0, 0x07c, 0x00f001a5);  // EQ_TH_MMA
	apb_write_reg(0, 0x080, 0x0002c077);  // EQ_TH_SMMA0
	apb_write_reg(0, 0x084, 0x000bc0fe);  // EQ_TH_SMMA1
	apb_write_reg(0, 0x088, 0x0013f17e);  // EQ_TH_SMMA2
	apb_write_reg(0, 0x08c, 0x01bc01f9);  // EQ_TH_SMMA3
	//apb_write_reg(0, 0x094, 0x7f800d2c);  // AGC_CTRL  ALPS tuner
	//apb_write_reg(0, 0x094, 0x7f80292c);  // Pilips Tuner
	//apb_write_reg(0, 0x094, 0x7f80292d);  // Maxlinear Tuner
	apb_write_reg(0, 0x094, 0x7f80092d);  // Maxlinear Tuner
	break;
    }
    
    apb_write_reg(0, 0x00c, 0xfffffffe);  // adc_cnt, symb_cnt
	
    if (clk_freq == 0)
        afifo_ctr = 0;
    else
        afifo_ctr = (adc_freq * 256 / clk_freq) + 2;
    if(afifo_ctr > 255) afifo_ctr = 255;
    apb_write_reg(0, 0x010, (afifo_ctr<<16) | 8000); // afifo, rs_cnt_cfg
    
    apb_write_reg(0, 0x020, 0x21353e54);  // PHS_reset & TIM_CTRO_ACCURATE  sw_tim_select=0
    // apb_write_reg(0, 0x020, 0x12b53e54);  // PHS_reset & TIM_CTRO_ACCURATE sw_tim_select=1;
             
    if (adc_freq == 0)
        phs_cfg = 0;
    else
        phs_cfg = (1<<31) / adc_freq * ch_if / (1<<8);   //  8*fo/fs*2^20 fo=36.125, fs = 28.57114, = 21d775
    // printk("phs_cfg = %x\n", phs_cfg);
    apb_write_reg(0, 0x024, 0x4c000000 | (phs_cfg&0x7fffff));  // PHS_OFFSET, IF offset, 

    //apb_write_reg(0, 0x030, 0x035bf454);  // TIM_CTL0 start speed is 10,   fast search speed in multipath
    apb_write_reg(0, 0x030, 0x011bf400);  // TIM_CTL0 start speed is 0,  when know symbol rate

    apb_write_reg(0, 0x034, ((adc_freq & 0xffff) << 16) | (symb_rate&0xffff) );
    
    apb_write_reg(0, 0x038, 0x00200000);  // TIM_SWEEP_RANGE 16000

/************* hw state machine config **********/ 		
    apb_write_reg(0, 0x040, 0x003c); // configure symbol rate step step 0

    // modified 0x44 0x48
    apb_write_reg(0, 0x044, (symb_rate&0xffff)*256); // blind search, configure max symbol_rate      for 7218  fb=3.6M  
    //apb_write_reg(0, 0x048, 3600*256); // configure min symbol_rate fb = 6.95M
    apb_write_reg(0, 0x048, 3600*256); // configure min symbol_rate fb = 6.95M  		
 		
    //apb_write_reg(0, 0x0c0, 0xffffff68); // threshold
    //apb_write_reg(0, 0x0c0, 0xffffff6f); // threshold
    //apb_write_reg(0, 0x0c0, 0xfffffd68); // threshold
    //apb_write_reg(0, 0x0c0, 0xffffff68); // threshold
    apb_write_reg(0, 0x0c0, 0xffffff68); // threshold
/************* hw state machine config **********/ 		
 		
    apb_write_reg(0, 0x04c, 0x00008800);  // reserved

    apb_write_reg(0, 0x050, 0x00000002);  // EQ_CTL0
    apb_write_reg(0, 0x058, 0xff550e1e);  // EQ_FIR_INITPOS
    apb_write_reg(0, 0x05c, 0x019a0000);  // EQ_FIR_INITVAL0
    apb_write_reg(0, 0x060, 0x019a0000);  // EQ_FIR_INITVAL1

    apb_write_reg(0, 0x064, 0x01101128);  // EQ_CRTH_TIMES
    apb_write_reg(0, 0x06c, 0x00041a05);  // EQ_CRTH_PPM

    apb_write_reg(0, 0x070, 0xffb9aa01);  // EQ_CRLP
    
    //apb_write_reg(0, 0x090, 0x00020bd5); // agc control                 
    apb_write_reg(0, 0x090, 0x00000bd5); // agc control                 
    
    // agc control
    // apb_write_reg(0, 0x094, 0x7f800d2c);   // AGC_CTRL  ALPS tuner
    // apb_write_reg(0, 0x094, 0x7f80292c);     // Pilips Tuner
    if ((agc_mode&1)==0)   
	apb_write_reg(0, 0x094, apb_read_reg(0, 0x94) | (0x1 << 10));     // freeze if agc
    if ((agc_mode&2)==0) // IF control
	apb_write_reg(0, 0x094, apb_read_reg(0, 0x94) | (0x1 << 13));     // freeze rf agc
    
    //apb_write_reg(0, 0x094, 0x7f80292d);     // Maxlinear Tuner
	              
    apb_write_reg(0, 0x098, 0x9fcc8190);  // AGC_IFGAIN_CTRL  
    //apb_write_reg(0, 0x0a0, 0x0e028c00);  // AGC_RFGAIN_CTRL 0x0e020800
    //apb_write_reg(0, 0x0a0, 0x0e03cc00);  // AGC_RFGAIN_CTRL 0x0e020800
    apb_write_reg(0, 0x0a0, 0x0e028700);  // AGC_RFGAIN_CTRL 0x0e020800
    //apb_write_reg(0, 0x0a0, 0x0e028600);  // AGC_RFGAIN_CTRL 0x0e020800
      
    apb_write_reg(0, 0x004, apb_read_reg(0, 0x004)|3);  // IMQ, QAM Enable
    
    // start hardware machine
    dvbc_sw_reset(0x004, 4); 
    apb_write_reg(0, 0x0e8, (apb_read_reg(0, 0x0e8)|(1<<2)));  	

    // clear irq status
    apb_read_reg(0, 0xd4);

    // enable irq
    apb_write_reg(0, 0xd0, 0x7fff<<3);
} 

int dvbc_set_ch(struct aml_demod_sta *demod_sta, 
		struct aml_demod_i2c *demod_i2c, 
		struct aml_demod_dvbc *demod_dvbc)
{
    int ret = 0;
    u16 symb_rate;
    u8  mode;
    u32 ch_freq;

   printk("f=%d, s=%d, q=%d\n", demod_dvbc->ch_freq, demod_dvbc->symb_rate, demod_dvbc->mode);
   
    mode      = demod_dvbc->mode;
    symb_rate = demod_dvbc->symb_rate;
    ch_freq   = demod_dvbc->ch_freq;

    if (mode > 4) {
	printk("Error: Invalid QAM mode option %d\n", mode);
	mode = 2;
       	ret = -1;
    }

    if (symb_rate<1000 || symb_rate>7000) {
	printk("Error: Invalid Symbol Rate option %d\n", symb_rate);
	symb_rate = 6875;
	ret = -1;
    }

    if (ch_freq<1000 || ch_freq>900000) {
	printk("Error: Invalid Channel Freq option %d\n", ch_freq);
	ch_freq = 474000;
	ret = -1;
    }

    // if (ret != 0) return ret;

    // Set DVB-C
    (*DEMOD_REG0) &= ~1;

    demod_sta->dvb_mode  = 0;    // 0:dvb-c, 1:dvb-t
    demod_sta->ch_mode   = mode; // 0:16, 1:32, 2:64, 3:128, 4:256
    demod_sta->agc_mode  = 1;    // 0:NULL, 1:IF, 2:RF, 3:both
    demod_sta->ch_freq   = ch_freq;
    demod_sta->tuner     = demod_i2c->tuner;

    if(demod_i2c->tuner == 1)
        demod_sta->ch_if     = 36130; // TODO  DCT tuner
    else if (demod_i2c->tuner == 2)
        demod_sta->ch_if     = 4570; // TODO  Maxlinear tuner
        
    demod_sta->ch_bw     = 8000;  // TODO
    demod_sta->symb_rate = symb_rate;  
    
    // Set Tuner
    tuner_set_ch(demod_sta, demod_i2c);

    dvbc_reg_initial(demod_sta);

    return ret;
}  

int dvbc_status(struct aml_demod_sta *demod_sta, 
		struct aml_demod_i2c *demod_i2c, 
		struct aml_demod_sts *demod_sts)
{
    demod_sts->ch_sts = apb_read_reg(0, 0x18);
    demod_sts->ch_pow = dvbc_get_ch_power();
    demod_sts->ch_snr = dvbc_get_snr();
    demod_sts->ch_ber = dvbc_get_ber();
    demod_sts->ch_per = dvbc_get_per();
    demod_sts->symb_rate = dvbc_get_symb_rate();
    demod_sts->freq_off = dvbc_get_freq_off();
    demod_sts->dat0 = apb_read_reg(0, 0x28);
  //  demod_sts->dat1 = tuner_get_ch_power(demod_i2c);

    return 0;
}
 
void dvbc_enable_irq(int dvbc_irq)
{
    u32 mask;
    
    // clear status
    apb_read_reg(0, 0xd4);
    // enable irq
    mask = apb_read_reg(0, 0xd0);
    mask |= (1<<dvbc_irq);
    apb_write_reg(0, 0xd0, mask);
}

void dvbc_disable_irq(int dvbc_irq)
{
    u32 mask;

    // disable irq
    mask = apb_read_reg(0, 0xd0);
    mask &= ~(1<<dvbc_irq);
    apb_write_reg(0, 0xd0, mask);
    // clear status
    apb_read_reg(0, 0xd4);
}

char *dvbc_irq_name[] = {
    "      ADC",
    "   Symbol",
    "       RS",
    " In_Sync0",
    " In_Sync1",
    " In_Sync2",
    " In_Sync3",
    " In_Sync4",
    "Out_Sync0",
    "Out_Sync1",
    "Out_Sync2",
    "Out_Sync3",
    "Out_Sync4",
    "In_SyncCo",
    "OutSyncCo",
    "  In_Dagc",
    " Out_Dagc",
    "  Eq_Mode",
    "RS_Uncorr"};

void dvbc_isr(struct aml_demod_sta *demod_sta)
{
    u32 stat, mask;
    int dvbc_irq;
    
    stat = apb_read_reg(0, 0xd4);
    mask = apb_read_reg(0, 0xd0);
    stat &= mask;
    
    for (dvbc_irq=0; dvbc_irq<20; dvbc_irq++) {
	if (stat>>dvbc_irq&1) {
	    if (demod_sta->debug) 
		printk("irq: dvbc %2d %s %8x\n", 
		       dvbc_irq, dvbc_irq_name[dvbc_irq], stat);
	    // dvbc_disable_irq(dvbc_irq);
	}
    }
}

int dvbc_isr_islock(void)
{
#define IN_SYNC4_MASK (0x80)

    u32 stat, mask;

    stat = apb_read_reg(0, 0xd4);
    mask = apb_read_reg(0, 0xd0);
    stat &= mask;

    return ((stat&IN_SYNC4_MASK)==IN_SYNC4_MASK);
}

