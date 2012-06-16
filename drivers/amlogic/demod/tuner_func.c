#include <linux/kernel.h>
#include <linux/i2c.h>

#include <dvb_frontend.h>

#include "aml_demod.h"
#include "demod_func.h"

#include "mxl/MxL5007_API.h"

#if 1
static int set_tuner_MxL5007(struct aml_demod_sta *demod_sta, 
			     struct aml_demod_i2c *adap)
{
    MxL_ERR_MSG            Status = MxL_OK;
    int                    RFSynthLock , REFSynthLock;
    //SINT32               RF_Input_Level;
    SINT32                 out = 0;
    MxL5007_TunerConfigS   myTuner;
    unsigned long ch_freq;
    int ch_if;
    int ch_bw;

    ch_freq = demod_sta->ch_freq; // kHz
    ch_if   = demod_sta->ch_if;   // kHz 
    ch_bw   = demod_sta->ch_bw / 1000; // MHz

    printk("Set Tuner MxL5007 to %ld kHz\n", ch_freq);

    myTuner.I2C_Addr = adap->addr; // MxL_I2C_ADDR_96;
    myTuner.Mode = demod_sta->dvb_mode==0 ? 
	MxL_MODE_CABLE : MxL_MODE_DVBT;
    myTuner.IF_Freq = ch_if < 5000 ? MxL_IF_4_57_MHZ :
	ch_if < 9900 ? MxL_IF_9_1915_MHZ : MxL_IF_36_15_MHZ;

    myTuner.IF_Diff_Out_Level = -8;
    myTuner.Xtal_Freq         = MxL_XTAL_24_MHZ;
    myTuner.ClkOut_Setting    = MxL_CLKOUT_ENABLE;
    myTuner.ClkOut_Amp        = MxL_CLKOUT_AMP_0;
    myTuner.I2C_adap          = (u32)adap;

    Status = MxL_Tuner_Init(&myTuner);
    if (Status!=0) {
	printk("Error: MxL_Tuner_Init fail!\n");
	return -1;
    }
    Status = MxL_Tuner_RFTune(&myTuner, ch_freq*KHz, ch_bw);
    if (Status!=0) {
	printk("Error: MxL_Tuner_RFTune fail!\n");
	return -1;
    }
    MxL_RFSynth_Lock_Status(&myTuner,&RFSynthLock);
    MxL_REFSynth_Lock_Status(&myTuner, &REFSynthLock);

    // MxL_Stand_By(&myTuner);
    // MxL_Wake_Up(&myTuner);
	
    if (RFSynthLock!=0)
	out = out + 1;
    if (REFSynthLock!=0)
	out = out + 2;

    return (out);
}

static int set_tuner_TD1316(struct aml_demod_sta *demod_sta, 
			    struct aml_demod_i2c *adap)
{
    int ret = 0;
    unsigned char data[4];
    unsigned long ch_freq, ftmp;
    unsigned char CB1, SB, CB2, AB;
    int cp_mode = 0;
    int ch_bw;
    int agc_mode;
    struct i2c_msg msg;

    msg.addr = adap->addr;
    msg.flags = 0;
    msg.len = 4;
    msg.buf = data;
    
    ch_freq = demod_sta->ch_freq;
    ch_bw = demod_sta->ch_bw;
    agc_mode = demod_sta->agc_mode;

    printk("Set Tuner TD1316 to %ld kHz\n", ch_freq);

    ftmp = (ch_freq*1000 + 36350*1000 + 83350)/166700;
    data[0] = ftmp>>8&0xff;
    data[1] = ftmp&0xff;

    /**********************************************************
    Low CP : (MHz)
    1) 44  <= freq <=  171;  low band
    2) 132 <= freq <= 471 ;  middle band
    3) 405 <= freq <= 899 ;  high band
    Medium CP:(MHz)
    1) 44  <= freq <=  171;  low band
    2) 132 <= freq <= 471 ;  middle band
    3) 405 <= freq <= 899 ;  high band
    High CP:(MHz)
    1) 44  <= freq <=  171;  low band
    2) 132 <= freq <= 471 ;  middle band
    3) 554 <= freq <= 900 ; high band
    *********************************************************/

    // Control Data Bytes
    //printk("Choose Charge Pump? Low is 0, Medium is 1, high is others");
    //scanf("%d",&cp_mode);
    if (cp_mode == 0) {
	CB1 = 0xbc; CB2 = 0x9c; // Low CP & 166.7Khz PLL stepsize 
    }
    else if (cp_mode == 1) {
	CB1 = 0xf4; CB2 = 0xdc;  // Medium CP & 166.7Khz PLL stepsize, Recommend for PAL
    }
    else { 
	CB1 = 0xfc; CB2 = 0xdc;  // High CP & 166.7Khz PLL stepsize, 
    }
	  
    // Switchport Bytes
    SB = 0;
    if ( ((ch_freq/1000) <= 152))   SB = SB | 0x1;// Low band
    else if (ch_freq/1000 <= 438 )  SB = SB | 0x2;// mid band
    else if (ch_freq/1000 <= 900 )  SB = SB | 0x4;// high band
    else {
	SB = SB | 0x3;// standby 
	printk("Error: Tuner set to Standby mode!\n");
    }

    SB = (ch_bw==8000)? (SB | 0x8) : SB; // set 7/8M SAW filter	
    //SB = (ch_bw==8)? (SB | 0xb) : SB; // set 7/8M SAW filter	
 
    // AB 
    // AB = 0x50; // For PAL applications AGC loop top -12dB
    // AB = 0x20; // For DVB-T Medium AGC loop top -3dB
    // AB = 0x30; // For DVB-T Low    AGC loop top -6dB  
    // AB = 0x70; // The tuner internal AGC detector is disabled. 
    // With no external AGC voltage applied to the tuner, the RF-gain is always set to maximum.   
    // AB = 0x10;  // For High (0dB reference);	  

    AB = agc_mode == 1 ? 0x60 : 0x50;

    data[2] = CB1;
    data[3] = SB;
	  
    ret = am_demod_i2c_xfer(adap, &msg, 1);
    if (ret == 0) {
	return ret;
    }

    data[2]  = CB2;
    data[3]  = AB;
    ret = am_demod_i2c_xfer(adap, &msg, 1);
    return ret;
}
#endif

static int set_tuner_DCT7070X(struct aml_demod_sta *demod_sta, 
			      struct aml_demod_i2c *adap)
{
    int ret = 0;
    unsigned char data[6];
    unsigned long ch_freq, ftmp;
    struct i2c_msg msg;
    
    ch_freq = demod_sta->ch_freq;
    //printk("Set Tuner DCT7070X to %ld kHz\n", ch_freq);
		
    ftmp = (ch_freq+36125)*10/625;  // ftmp=(ch_freq+GX_IF_FREQUENCY)
    data[0] = ftmp>>8&0xff;	
    data[1] = ftmp&0xff;	
    data[2] = 0x8b;              // 62.5 kHz 
		
    if (ch_freq < 153000) 
	data[3] = 0x01;
    else if (ch_freq < 430000) 
	data[3] = 0x06;
    else 
	data[3] = 0x0c; 
		
    data[4] = 0xc3;
		
    msg.addr = adap->addr;
    msg.flags = 0; // I2C_M_IGNORE_NAK; 
    msg.len = 5;
    msg.buf = data;

    ret = am_demod_i2c_xfer(adap, &msg, 1);

    return ret;
}

int tuner_set_ch(struct aml_demod_sta *demod_sta, struct aml_demod_i2c *adap)
{
    int ret = 0;

    switch (adap->tuner) {
    case 0 : // NULL
	printk("Warning: NULL Tuner\n");
	break;
	
    case 1 : // DCT
	ret = set_tuner_DCT7070X(demod_sta, adap);
	break;

    case 2 : // Maxliner
	ret = set_tuner_MxL5007(demod_sta, adap);
	break;

    case 3 : // NXP
	ret = set_tuner_fj2207(demod_sta, adap);
	break;
/*
    case 4 : // TD1316
	ret = set_tuner_TD1316(demod_sta, adap);
	break;
*/
    default :
	return -1;
    }

    return 0;
}

int tuner_get_ch_power(struct aml_demod_i2c *adap)
{
    int ret = 0;

    switch (adap->tuner) {
    case 0 : // NULL
	// printk("Warning: NULL Tuner\n");
	break;
	
    case 1 : // DCT
	// ret = set_tuner_DCT7070X(demod_sta, adap);
	break;

    case 2 : // Maxliner
	// ret = set_tuner_MxL5007(demod_sta, adap);
	break;

    case 3 : // NXP
	ret = get_fj2207_ch_power();
	break;

    case 4 : // TD1316
	// ret = set_tuner_TD1316(demod_sta, adap);
	break;

    default :
	return -1;
    }

    return ret;
}

struct dvb_tuner_info * tuner_get_info( int type, int mode)
{
	/*type :  0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316*/
	/*mode: 0-DVBC 1-DVBT */
	static struct dvb_tuner_info tinfo_null = {};

	static struct dvb_tuner_info tinfo_MXL5003S[2] = {
		[1] = {/*DVBT*/
			.name = "Maxliner", 
			.frequency_min = 44000000,
			.frequency_max = 885000000,
		}
	};
	static struct dvb_tuner_info tinfo_FJ2207[2] = {
		[0] = {/*DVBC*/
			.name = "FJ2207", 
			.frequency_min = 54000000,
			.frequency_max = 870000000,
		},
		[1] = {/*DVBT*/
			.name = "FJ2207", 
			.frequency_min = 174000000,
			.frequency_max = 864000000,
		},
	};
	static struct dvb_tuner_info tinfo_DCT7070[2] = {
		[0] = {/*DVBC*/
			.name = "DCT7070", 
			.frequency_min = 51000000,
			.frequency_max = 860000000,
		}
	};
	static struct dvb_tuner_info tinfo_TD1316[2] = {
		[1] = {/*DVBT*/
			.name = "TD1316", 
			.frequency_min = 51000000,
			.frequency_max = 858000000,
		}
	};
	
	struct dvb_tuner_info *tinfo[5] = {
		&tinfo_null, 
		tinfo_DCT7070,
		tinfo_MXL5003S,
		tinfo_FJ2207,
		tinfo_TD1316
	};

	if((type<0)||(type>4)||(mode<0)||(mode>1))
		return tinfo[0];
	
	return &tinfo[type][mode];
}


