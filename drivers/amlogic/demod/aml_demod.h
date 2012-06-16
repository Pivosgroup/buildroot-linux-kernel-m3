#ifndef AML_DEMOD_H
#define AML_DEMOD_H

struct aml_demod_i2c {
    u8  tuner;   // type
    u8  addr;    // slave addr
    u32 scl_oe;
    u32 scl_out;
    u32 scl_in;
    u8  scl_bit;
    u32 sda_oe;
    u32 sda_out;
    u32 sda_in;
    u8  sda_bit;
    u8  udelay;  // us  
    u8  retries;
    u8  debug;   // 1:debug
    u8  tmp;     // spare
    u8 i2c_id;
    void *i2c_priv;
};

struct aml_demod_sys {
    u8  clk_en;     // 1:on
    u8  clk_src;    // 2 bits
    u8  clk_div;    // 7 bits
    u8  pll_n;      // 5 bits
    u16 pll_m;      // 9 bits
    u8  pll_od;     // 7 bits
    u8  pll_sys_xd; // 5 bits
    u8  pll_adc_xd; // 5 bits
    u8  agc_sel;    // pin mux
    u8  adc_en;     // 1:on
    u8  debug;      // 1:debug
    u32 i2c;        // pointer
}; 

struct aml_demod_sts {
    u32 ch_sts;
    u32 freq_off;   // Hz
    u32 ch_pow;      
    u32 ch_snr;     
    u32 ch_ber;
    u32 ch_per;
    u32 symb_rate;
    u32 dat0;
    u32 dat1;
}; 

struct aml_demod_sta {
    u8  clk_en;     // on/off
    u8  adc_en;     // on/off 
    u32 clk_freq;   // kHz
    u32 adc_freq;   // kHz
    u8  dvb_mode;   // dvb-t/c mode
    u8  ch_mode;    // 16,32,..,256QAM or 2K,4K,8K   
    u8  agc_mode;   // if, rf or both.
    u8  tuner;      // type
    u32 ch_freq;    // kHz
    u16 ch_if;      // kHz
    u16 ch_bw;      // kHz
    u16 symb_rate;  // kHz
    u8  debug;
    u8  tmp;
    u32 sts;        // pointer
};

struct aml_demod_dvbc {
    u8  mode;
    u8  tmp;
    u16 symb_rate;
    u32 ch_freq;
    u32 dat0;
    u32 dat1;
}; 

struct aml_demod_dvbt {
    u8  bw;
    u8  sr;
    u8  ifreq;
    u8  agc_mode;
    u32 ch_freq;
    u32 dat0;
    u32 dat1;
}; 

struct aml_demod_reg {
    u8  mode; // 0: dvbc, 1: demod, 2: dvbt, 3:other
    u8  rw;   // 0: read, 1: write.
    u32 addr;
    u32 val;
}; 

#define AML_DEMOD_SET_SYS        _IOW('D',  0, struct aml_demod_sys)
#define AML_DEMOD_GET_SYS        _IOR('D',  1, struct aml_demod_sys)
#define AML_DEMOD_TEST           _IOR('D',  2, u32)
#define AML_DEMOD_TURN_ON        _IOR('D',  3, u32)
#define AML_DEMOD_TURN_OFF       _IOR('D',  4, u32)

#define AML_DEMOD_DVBC_SET_CH    _IOW('D', 10, struct aml_demod_dvbc)
#define AML_DEMOD_DVBC_GET_CH    _IOR('D', 11, struct aml_demod_dvbc)
#define AML_DEMOD_DVBC_TEST      _IOR('D', 12, u32)

#define AML_DEMOD_DVBT_SET_CH    _IOW('D', 20, struct aml_demod_dvbt)
#define AML_DEMOD_DVBT_GET_CH    _IOR('D', 21, struct aml_demod_dvbt)
#define AML_DEMOD_DVBT_TEST      _IOR('D', 22, u32)

#define AML_DEMOD_SET_REG        _IOW('D', 30, struct aml_demod_reg)
#define AML_DEMOD_GET_REG        _IOR('D', 31, struct aml_demod_reg)

#endif /* AML_DEMOD_H */
