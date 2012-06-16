#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <mach/am_regs.h>
#include "aml_audio_hw.h"

struct snd_soc_codec_device soc_codec_dev_aml_m3;
static struct snd_soc_codec *aml_m3_codec;

extern int aml_m3_is_hp_pluged(void);
extern void mute_spk(struct snd_soc_codec* codec, int flag);

/* codec private data */
struct aml_m3_codec_priv {
	struct snd_soc_codec codec;
	u16 reg_cache[ADAC_MAXREG];
	unsigned int sysclk;
};

unsigned long aml_rate_table[] = {
    8000, 11025, 12000, 16000, 22050, 24000, 32000, 
    44100, 48000, 88200, 96000, 192000
};

void latch_ (struct snd_soc_codec* codec)
{
    int latch;
    latch = 1;
    snd_soc_write(codec, ADAC_LATCH, latch);
    latch = 0;
    snd_soc_write(codec, ADAC_LATCH, latch);
}


typedef enum  {
    AML_PWR_DOWN,
    AML_PWR_UP,
    AML_PWR_KEEP,
} AML_PATH_SET_TYPE;

void aml_reset_path(struct snd_soc_codec* codec, AML_PATH_SET_TYPE type)
{
    unsigned int pwr_reg2 = snd_soc_read(codec, ADAC_POWER_CTRL_REG2);
    latch_(codec);
    snd_soc_write(codec, ADAC_POWER_CTRL_REG2, pwr_reg2&(~(1<<7)));
    latch_(codec);
    snd_soc_write(codec, ADAC_POWER_CTRL_REG2, pwr_reg2|(1<<7));
    latch_(codec);
     
    if (AML_PWR_DOWN == type)
    {
        snd_soc_write(codec, ADAC_POWER_CTRL_REG2, pwr_reg2&(~(1<<7)));
        latch_(codec);
    }
    
    if (AML_PWR_KEEP == type)
    {
        snd_soc_write(codec, ADAC_POWER_CTRL_REG2, pwr_reg2);
        latch_(codec);
    }
}

void aml_m3_reset(struct snd_soc_codec* codec, bool first_time)
{
	unsigned long data32;

	if (first_time)
	{
        //audio_set_clk(AUDIO_CLK_FREQ_48,0);
        audio_set_clk(AUDIO_CLK_FREQ_192,0);
	 	set_acodec_source(AIU_I2SOUT_TO_DAC);   // 0=no clock to CODEC; 1=pcmout to DAC; 2=Aiu I2S out to DAC.

    	// --------------------------------------------------------
    	// Configure audio DAC control interface
    	// --------------------------------------------------------

	    data32  = 0;
	    data32 |= 0     << 15;  // [15]     audac_soft_reset_n
	    data32 |= 0     << 14;  // [14]     audac_reset_ctrl: 0=use audac_reset_n pulse from reset module; 1=use audac_soft_reset_n.
	    data32 |= 0     << 9;   // [9]      delay_rd_en
	    data32 |= 0     << 8;   // [8]      audac_reg_clk_inv
	    data32 |= 0x55  << 1;   // [7:1]    audac_i2caddr
	    data32 |= 0     << 0;   // [0]      audac_intfsel: 0=use host bus; 1=use I2C.
	    WRITE_MPEG_REG(AIU_AUDAC_CTRL0, data32);
			
			
	    WRITE_MPEG_REG( HHI_AUD_PLL_CNTL, READ_MPEG_REG(HHI_AUD_PLL_CNTL) & ~(1 << 15));
	    WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 1, 23, 1);
	    WRITE_MPEG_REG(AUDIN_SOURCE_SEL, (1<<0)); // select audio codec output as I2S source
	    msleep(100);
	 
	    // Enable APB3 fail on error
	    data32  = 0;
	    data32 |= 1     << 15;  // [15]     err_en
	    data32 |= 255   << 0;   // [11:0]   max_err
	    WRITE_MPEG_REG(AIU_AUDAC_CTRL1, data32);
	    // Check read back data
	    data32 = READ_MPEG_REG(AIU_AUDAC_CTRL0);
	    if (data32 != (0x55 << 1)) {
			printk("audiocodec init error: AIU_AUDAC_CTRL0 = %lx\n", data32);
		}
			
		data32 = READ_MPEG_REG(AIU_AUDAC_CTRL1);
		if (data32 != 0x80ff) {
			printk("audiocodec init error: AIU_AUDAC_CTRL1 = %lx\n", data32);
		}

		wr_regbank (0,			// rstdpz: active low.
					0,			// mclksel[3:0]: master clock freq sel. 0=256Fs, 1=384Fs, ... 
					8,		    // i2sfsadc[3:0]: sample freq sel. 0=8kHz, 1=11.025k, 2=12k, 3=16k, 4=22.05k, 5=24k, 6=32k, 7=44.1k, 8=48k, 9=88.2k, 10=96k, 11=192k, >11=Rsrv.
					8,		    // i2sfsdac[3:0]: sample freq sel. 0=8kHz, 1=11.025k, 2=12k, 3=16k, 4=22.05k, 5=24k, 6=32k, 7=44.1k, 8=48k, 9=88.2k, 10=96k, 11=192k, >11=Rsrv.
					1,			// i2ssplit: 0=single mode, 1=dual mode.
					1,			// i2smode[2:0]: Data format sel. 0=Right justify, 1=I2S, 2=Left justify, 3=Burst1, 4=Burst2, 5=Mono burst1, 6=Mono burst2, 7=Rsrv.
					1,			// pdauxdrvrz: 0=power down active, 1=normal.
					1,			// pdauxdrvlz: 0=power down active, 1=normal.
					1,			// pdhsdrvrz: 0=power down active, 1=normal.
					1,			// pdhsdrvlz: 0=power down active, 1=normal.
					1,			// pdlsdrvz: 0=power down active, 1=normal.
					1,			// pddacrz: 0=power down active, 1=normal.
					1,			// pddaclz: 0=power down active, 1=normal.
					0,			// pdz: 0=power down active, 1=normal.
					1,			// pdmbiasz: 0=power down active, 1=normal.
					1,			// pdvcmbufz: 0=power down active, 1=normal.
					1,			// pdrpgaz: 0=power down active, 1=normal.
					1,			// pdlpgaz: 0=power down active, 1=normal.
					1,			// pdadcrz: 0=power down active, 1=normal.
					1,			// pdadclz: 0=power down active, 1=normal.
					0,			// hsmute[1:0]: bit[1] Analog playback right channel mute, bit[0] Analog playback left channel mute.
					0,			// recmute[1:0]: bit[1] Digital record right channel mute, bit[0] Digital record left channel mute.
					0,			// micmute[1:0]: bit[1] Input PGA right channel mute, bit[0] Input PGA left channel mute.
					0,			// lmmute[1:0]: bit[1] Digital playback right channel mute, bit[0] Digital playback left channel mute.
					0,			// lsmute: loud speaker mute.
					0,			// lmmix: Playback digital mixer sel.
					0,			// recmix: Record digital mixer sel.
					0,			// ctr[1:0]: test mode sel. 0=Normal, 1=Digital filter loopback, 2=Digital filter bypass, 3=Digital audio I/F loopback.
					1,			// enhp: Record channel high pass filter enable.
					0x5454, 	// lmvol[15:0]: Digital playback volumn control, [15:8] control right channel, [7:0] control left channel.
								// 0=-126dB, 1=-124.5dB, ..., 0x53=-1.5dB, 0x54=0dB, >=0x55 Rsrv.
					0x2828, 	// hsvol[15:0]: Analog playback headset volumn control, [15:8] control right channel, [7:0] control left channel.
								// 0=-40dB, 1=-39dB, ..., 0x28=0dB, ..., 0x2e=6dB, >=0x2f Rsrv.
					0,			// pbmix[15:0]: Playback channel mixing sel. [15:8] control right channel, [7:0] control left channel.
								// 0=Left(Right) digital source only, 1=Analog source only, 2=Analog + left(right) digital,
								// 3=Right(Left) digital sourcd only, 4=Left(Right) digital + right(left) digital, 5=Analog + right(left) digital,
								// 6=Analog + left(right) digital + right(left) digital, >=7 Rsrv.
					0x0001, 	// lsmix[15:0]
					0x1212, 	// micvol[15:0]: Input PGA volume control. [15:8] control right channel, [7:0] control left channel.
								// 0=-6dB, 1=-4.5dB, ..., 4=0dB, ..., 0x11=19.5dB, 0x12=21dB, >=0x13 Rsrv.
					0x1111, 	// recvol[15:0]: Digital record channel master volume control. [15:8] control right channel, [7:0] control left channel.
								// 0=30dB, 1=28.5dB, ..., 0x13=1.5dB, 0x14=0dB, 0x15=-1.5dB, ..., 0x54=-96dB, >=0x55 Rsrv.
					0x0101);	// recsel[15:0]: Input PGA selection. [15:8] control right channel, [7:0] control left channel.
								// 0x01=input1, 0x03=input2, 0x05=input3, 0x09=input4,
								// 0x11=input5, 0x21=input6, 0x41=input7, 0x81=input8, others=Rsrv.
        adac_power_up_mode_2();
        adac_startup_seq();

	    udelay(10);
	    
	    if(aml_m3_is_hp_pluged()) {
	    	data32 = snd_soc_read(codec, ADAC_MUTE_CTRL_REG1);
		    data32 &= ~0xc0;
		    snd_soc_write(codec, ADAC_MUTE_CTRL_REG1, data32); //unmute HP
    	    mute_spk(codec, 1);
    	    latch_(codec);
    	}
    	else {
    		data32 = snd_soc_read(codec, ADAC_MUTE_CTRL_REG1);
	    	data32 |= 0xc0;
		    snd_soc_write(codec, ADAC_MUTE_CTRL_REG1, data32);//mute HP
    		mute_spk(codec, 0);
	    	latch_(codec);
        }
        WRITE_CBUS_REG(HHI_GCLK_MPEG1, READ_CBUS_REG(HHI_GCLK_MPEG1)&~(1<<2));
        aml_reset_path(codec, AML_PWR_DOWN);//first time pwr down
        latch_(codec);
	    
/*			snd_soc_write(codec, ADAC_RESET, (0<<1));
	    	snd_soc_write(codec, ADAC_RESET, (0<<1));
	    	snd_soc_write(codec, ADAC_RESET, (0<<1));
	    	snd_soc_write(codec, ADAC_RESET, (0<<1));
	    	snd_soc_write(codec, ADAC_RESET, (0<<1));
	    	msleep(100);
	        

	    	snd_soc_write(codec,ADAC_CLOCK, 0); // 256fs
	    	snd_soc_write(codec, ADAC_I2S_CONFIG_REG1, (7<<4)|7);	 // samplerate for ADC&DAC
	    	snd_soc_write(codec, ADAC_I2S_CONFIG_REG2, 1|(1<<3)); 		// I2S | split

	        snd_soc_write(codec, ADAC_MUTE_CTRL_REG1,0);
	    	snd_soc_write(codec, ADAC_MUTE_CTRL_REG2, 0);
	        
	        snd_soc_write(codec,ADAC_DAC_ADC_MIXER, 0);

	        snd_soc_write(codec,ADAC_PLAYBACK_VOL_CTRL_LSB, 0x54);
	        snd_soc_write(codec,ADAC_PLAYBACK_VOL_CTRL_MSB, 0x54);
	        snd_soc_write(codec,ADAC_STEREO_HS_VOL_CTRL_LSB, 0x28);
	        snd_soc_write(codec,ADAC_STEREO_HS_VOL_CTRL_MSB, 0x28); 

	        snd_soc_write(codec, ADAC_PLAYBACK_MIX_CTRL_LSB, 0);
	        snd_soc_write(codec, ADAC_PLAYBACK_MIX_CTRL_MSB, 0);

	        snd_soc_write(codec, ADAC_STEREO_PGA_VOL_LSB, 4);
	        snd_soc_write(codec, ADAC_STEREO_PGA_VOL_MSB, 4);

	        snd_soc_write(codec, ADAC_RECVOL_CTRL_LSB, 0x14);
	        snd_soc_write(codec, ADAC_RECVOL_CTRL_MSB, 0x14);

	        snd_soc_write(codec, ADAC_REC_CH_SEL_LSB, 1|(1<<(1-1)));// 1|(1<<(channel-1))
	        snd_soc_write(codec, ADAC_REC_CH_SEL_MSB, 1|(1<<(1-1)));// 1|(1<<(channel-1))

	        snd_soc_write(codec, ADAC_POWER_CTRL_REG1, 0xf7);
	      	snd_soc_write(codec, ADAC_POWER_CTRL_REG2, 0x2f);

	        snd_soc_write(codec, ADAC_LS_MIX_CTRL_LSB, 1);
	        snd_soc_write(codec, ADAC_LS_MIX_CTRL_MSB, 0);
	   
	    	aml_reset_path(codec, AML_PWR_UP);
	        aml_reset_path(codec, AML_PWR_DOWN);
*/

	}
    else
    {
        snd_soc_write(codec, ADAC_LS_MIX_CTRL_LSB, 1);
        snd_soc_write(codec, ADAC_LS_MIX_CTRL_MSB, 0);
        aml_reset_path(codec, AML_PWR_UP);

    	latch_(codec);
	    snd_soc_write(codec, ADAC_RESET, (0<<1));
        latch_(codec);
	    latch_(codec);
    	latch_(codec);
	    snd_soc_write(codec, ADAC_RESET, (1<<1));
        latch_(codec);
	    latch_(codec);
    }
    msleep(300);
}


int audio_dac_set(unsigned freq)
{
  return 0;
}

static int post_reset(struct snd_soc_dapm_widget *w,
	    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
    
	if (SND_SOC_DAPM_POST_PMU == event)
		{
        aml_m3_reset(codec,false);
        }
    else if (SND_SOC_DAPM_POST_PMD == event && codec->active == 0)
        aml_reset_path(codec, AML_PWR_DOWN);
	return 0;
}

static int aml_switch_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
    struct snd_soc_dapm_widget *w;
    char *lname = NULL;
    char *rname = NULL;

    switch (e->reg)
    {
    case ADAC_POWER_CTRL_REG1:
        if (6 == e->shift_l)
        {
            lname = "LINEOUTL";
            rname = "LINEOUTR";
        }
        else if (4 == e->shift_l)
        {
            lname = "HP_L";
            rname = "HP_R";
        }
        else if (2 == e->shift_l)
        {
            lname = "SPEAKER";
        }
    break;
    case ADAC_POWER_CTRL_REG2:
        if (2 == e->shift_l)
        {
            lname = "LINEINL";
            rname = "LINEINR";
        }
    break;
    default:
    break;
    }
    
	list_for_each_entry(w, &codec->dapm_widgets, list) {
        if (lname && !strcmp(lname, w->name))
            ucontrol->value.enumerated.item[0] = w->connected;
        if (rname && !strcmp(rname, w->name))
            ucontrol->value.enumerated.item[0] = w->connected;
	}

	return 0;
}

static int aml_switch_put_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
    struct snd_soc_dapm_widget *w;
    char *lname = NULL;
    char *rname = NULL;
    unsigned int pwr_reg;

    switch (e->reg)
    {
    case ADAC_POWER_CTRL_REG1:
        if (6 == e->shift_l)
        {
            lname = "LINEOUTL";
            rname = "LINEOUTR";
        }
        else if (4 == e->shift_l)
        {
            lname = "HP_L";
            rname = "HP_R";
        }
        else if (2 == e->shift_l)
        {
            lname = "SPEAKER";
        }
    break;
    case ADAC_POWER_CTRL_REG2:
        if (2 == e->shift_l)
        {
            lname = "LINEINL";
            rname = "LINEINR";
        }
    break;
    default:
    break;
    }

    pwr_reg = snd_soc_read(codec, e->reg);
    if(ucontrol->value.enumerated.item[0] == 0){
    snd_soc_write(codec, e->reg, (pwr_reg&(~(0x1<<(e->shift_l)|0x1<<(e->shift_r)))));
    }
    else{
    snd_soc_write(codec, e->reg, (pwr_reg|(0x1<<(e->shift_l)|0x1<<(e->shift_r))));
    }

	list_for_each_entry(w, &codec->dapm_widgets, list) {
        if (lname && !strcmp(lname, w->name))
        {
            w->connected = ucontrol->value.enumerated.item[0];
            printk("%s:connect=%d\n",w->name,w->connected);
        }
        if (rname && !strcmp(rname, w->name))
        {
            w->connected = ucontrol->value.enumerated.item[0];
            printk("%s:connect=%d\n",w->name,w->connected);
        }
	}

	return 0;
}

static int aml_put_volsw_2r(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int err = snd_soc_put_volsw_2r(kcontrol, ucontrol);
    if (err < 0)
        return err;

    aml_reset_path(codec, AML_PWR_KEEP);
    return 0;
}

static int aml_ai_source_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    //struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    //struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

    if (READ_MPEG_REG(AUDIN_SOURCE_SEL) == 0)
        WRITE_MPEG_REG(AUDIN_SOURCE_SEL, (1<<0)); // select audio codec output as I2S source
    
    if (READ_MPEG_REG(AUDIN_SOURCE_SEL) == 1) 
        ucontrol->value.enumerated.item[0] = 0;// linein
    else
        ucontrol->value.enumerated.item[0] = 1;//hdmi
        
    return 0;
}

static int aml_ai_source_put_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    //struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
    
    if (ucontrol->value.enumerated.item[0] == 0)
        WRITE_MPEG_REG(AUDIN_SOURCE_SEL, (1<<0)); // select audio codec output as I2S source
    else
        WRITE_MPEG_REG(AUDIN_SOURCE_SEL,    (1<<4) | (2 << 0));    // Select HDMI RX output as AUDIN source

    snd_soc_write(codec, ADAC_RESET, (0<<1));
    latch_(codec);
    snd_soc_write(codec, ADAC_RESET, (1<<1));
    latch_(codec);
    
    return 0;
}

void aml_linein_start(void)
{
    // power up adc
    unsigned int pwr_reg2 = snd_soc_read(aml_m3_codec, ADAC_POWER_CTRL_REG2);
    snd_soc_write(aml_m3_codec, ADAC_POWER_CTRL_REG2, pwr_reg2|0xF);
    latch_(aml_m3_codec);
    // power up codec
    aml_reset_path(aml_m3_codec, AML_PWR_UP);
    snd_soc_write(aml_m3_codec, ADAC_RESET, (0<<1));
    latch_(aml_m3_codec);
    snd_soc_write(aml_m3_codec, ADAC_RESET, (1<<1));
    latch_(aml_m3_codec);
}

void aml_linein_stop(void)
{
    // power down adc
    unsigned int pwr_reg2 = snd_soc_read(aml_m3_codec, ADAC_POWER_CTRL_REG2);
    snd_soc_write(aml_m3_codec, ADAC_POWER_CTRL_REG2, pwr_reg2&(~0xF));
    latch_(aml_m3_codec);
    // power down codec
    if (aml_m3_codec->active == 0)
        aml_reset_path(aml_m3_codec, AML_PWR_DOWN);
}

static const DECLARE_TLV_DB_SCALE(lineout_volume, -12600, 150, 0);
static const DECLARE_TLV_DB_SCALE(hs_volume, -4000, 100, 0);
static const DECLARE_TLV_DB_SCALE(linein_volume, -9600, 150, 0);
static const DECLARE_TLV_DB_SCALE(linein_pga_volume, -600, 150, 0);

static const char *left_linein_texts[] = {
	"Left Line In 1", "Left Line In 2", "Left Line In 3", "Left Line In 4",
	"Left Line In 5", "Left Line In 6", "Left Line In 7", "Left Line In 8"
	};

static const char *right_linein_texts[] = {
	"Right Line In 1", "Right Line In 2", "Right Line In 3", "Right Line In 4",
	"Right Line In 5", "Right Line In 6", "Right Line In 7", "Right Line In 8"
	};

static const unsigned int linein_values[] = {
    1|(1<<(1-1)),
    1|(1<<(2-1)),
    1|(1<<(3-1)),
    1|(1<<(4-1)),
    1|(1<<(5-1)),
    1|(1<<(6-1)),
    1|(1<<(7-1)),
    1|(1<<(8-1))
    };

static const char *iis_split_texts[] = {
	"iis_not_split", "iis_split"
	};

static const unsigned int iis_split_values[] = {
	0,
	1
    };


static const SOC_VALUE_ENUM_SINGLE_DECL(left_linein_select, ADAC_REC_CH_SEL_LSB,
		0, 0xff, left_linein_texts, linein_values);
static const SOC_VALUE_ENUM_SINGLE_DECL(right_linein_select, ADAC_REC_CH_SEL_MSB,
		0, 0xff, right_linein_texts, linein_values);
static const SOC_VALUE_ENUM_SINGLE_DECL(iis_split_select, ADAC_I2S_CONFIG_REG2,
		3, 0xff, iis_split_texts, iis_split_values);

static const char *switch_op_modes_texts[] = {
	"OFF", "ON"
};
static const struct soc_enum lineout_op_modes_enum =
	SOC_ENUM_DOUBLE(ADAC_POWER_CTRL_REG1, 6, 7,
			ARRAY_SIZE(switch_op_modes_texts),
			switch_op_modes_texts);
static const struct soc_enum hp_op_modes_enum =
	SOC_ENUM_DOUBLE(ADAC_POWER_CTRL_REG1, 4, 5,
			ARRAY_SIZE(switch_op_modes_texts),
			switch_op_modes_texts);
static const struct soc_enum linein_op_modes_enum =
	SOC_ENUM_DOUBLE(ADAC_POWER_CTRL_REG2, 2, 2,
			ARRAY_SIZE(switch_op_modes_texts),
			switch_op_modes_texts);
static const struct soc_enum sp_op_modes_enum =
	SOC_ENUM_DOUBLE(ADAC_POWER_CTRL_REG1, 2, 3,
			ARRAY_SIZE(switch_op_modes_texts),
			switch_op_modes_texts);

static const char *audio_in_source_texts[] = {
	"LINEIN", "HDMI"
};
static const struct soc_enum audio_in_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(audio_in_source_texts),
			audio_in_source_texts);


static const struct snd_kcontrol_new amlm3_snd_controls[] = {
	SOC_DOUBLE_R_EXT_TLV("LINEOUT Playback Volume", ADAC_PLAYBACK_VOL_CTRL_LSB, ADAC_PLAYBACK_VOL_CTRL_MSB,
	       0, 84, 0, snd_soc_get_volsw_2r, aml_put_volsw_2r, lineout_volume),
	      
	 SOC_DOUBLE_R_EXT_TLV("HeadSet Playback Volume", ADAC_STEREO_HS_VOL_CTRL_LSB, ADAC_STEREO_HS_VOL_CTRL_MSB,
	       0, 46, 0, snd_soc_get_volsw_2r, aml_put_volsw_2r, hs_volume),

    SOC_DOUBLE_R_EXT_TLV("LINEIN Capture Volume", ADAC_RECVOL_CTRL_LSB, ADAC_RECVOL_CTRL_MSB,
	       0, 84, 1, snd_soc_get_volsw_2r, aml_put_volsw_2r, linein_volume),
    
    SOC_DOUBLE_R_EXT_TLV("LINEIN PGA Volume", ADAC_STEREO_PGA_VOL_LSB, ADAC_STEREO_PGA_VOL_MSB,
	       0, 18, 0, snd_soc_get_volsw_2r, aml_put_volsw_2r, linein_pga_volume),
	       
	SOC_VALUE_ENUM("Left LINEIN Select",left_linein_select),
	SOC_VALUE_ENUM("Right LINEIN Select",right_linein_select),
	SOC_VALUE_ENUM("IIS Split Select", iis_split_select),

    SOC_ENUM_EXT("LOUT Playback Switch", lineout_op_modes_enum,
		aml_switch_get_enum,aml_switch_put_enum),
		
    SOC_ENUM_EXT("HP Playback Switch", hp_op_modes_enum,
		aml_switch_get_enum,aml_switch_put_enum),
		
	SOC_ENUM_EXT("LIN Capture Switch", linein_op_modes_enum,
		aml_switch_get_enum,aml_switch_put_enum),
		
	SOC_ENUM_EXT("SP Playback Switch", sp_op_modes_enum,
		aml_switch_get_enum,aml_switch_put_enum),

    SOC_ENUM_EXT("Audio In Source", audio_in_source_enum,
        aml_ai_source_get_enum,aml_ai_source_put_enum),

};

static const struct snd_kcontrol_new lineoutl_switch_controls =
	SOC_DAPM_SINGLE("Switch", ADAC_MUTE_CTRL_REG1, 0, 1, 1);
static const struct snd_kcontrol_new lineoutr_switch_controls =
	SOC_DAPM_SINGLE("Switch", ADAC_MUTE_CTRL_REG1, 1, 1, 1);
static const struct snd_kcontrol_new hsl_switch_controls =
	SOC_DAPM_SINGLE("Switch", ADAC_MUTE_CTRL_REG1, 6, 1, 1);
static const struct snd_kcontrol_new hsr_switch_controls =
	SOC_DAPM_SINGLE("Switch", ADAC_MUTE_CTRL_REG1, 7, 1, 1);
static const struct snd_kcontrol_new spk_switch_controls =
	SOC_DAPM_SINGLE("Switch", ADAC_MUTE_CTRL_REG2, 2, 1, 1);

static const struct snd_kcontrol_new lineinl_switch_controls =
	SOC_DAPM_SINGLE("Switch", ADAC_MUTE_CTRL_REG2, 4, 1, 1);
static const struct snd_kcontrol_new lineinr_switch_controls =
	SOC_DAPM_SINGLE("Switch", ADAC_MUTE_CTRL_REG2, 5, 1, 1);

static const struct snd_soc_dapm_widget amlm3_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("LINEOUTL"),
	SND_SOC_DAPM_OUTPUT("LINEOUTR"),
	SND_SOC_DAPM_OUTPUT("HP_L"),
	SND_SOC_DAPM_OUTPUT("HP_R"),
	SND_SOC_DAPM_OUTPUT("SPEAKER"),

	SND_SOC_DAPM_INPUT("LINEINL"),
	SND_SOC_DAPM_INPUT("LINEINR"),
	
	SND_SOC_DAPM_DAC("DACL", "Left DAC Playback", ADAC_POWER_CTRL_REG1, 0, 0),
	SND_SOC_DAPM_DAC("DACR", "Right DAC Playback", ADAC_POWER_CTRL_REG1, 1, 0),
	SND_SOC_DAPM_ADC("ADCL", "Left ADC Capture", ADAC_POWER_CTRL_REG2, 0, 0),
	SND_SOC_DAPM_ADC("ADCR", "Right ADC Capture", ADAC_POWER_CTRL_REG2, 1, 0),

	SND_SOC_DAPM_SWITCH("LINEOUTL Switch", ADAC_POWER_CTRL_REG1, 6, 0,
			    &lineoutl_switch_controls),
	SND_SOC_DAPM_SWITCH("LINEOUTR Switch", ADAC_POWER_CTRL_REG1, 7, 0,
			    &lineoutr_switch_controls),
	SND_SOC_DAPM_SWITCH("HP_L Switch", ADAC_POWER_CTRL_REG1, 4, 0,
			    &hsl_switch_controls),
	SND_SOC_DAPM_SWITCH("HP_R Switch", ADAC_POWER_CTRL_REG1, 5, 0,
			    &hsr_switch_controls),
	SND_SOC_DAPM_SWITCH("SPEAKER Switch", ADAC_POWER_CTRL_REG1, 2, 0,
			    &spk_switch_controls),

	SND_SOC_DAPM_SWITCH("LINEINL Switch", ADAC_POWER_CTRL_REG2, 2, 0,
			    &lineinl_switch_controls),
	SND_SOC_DAPM_SWITCH("LINEINR Switch", ADAC_POWER_CTRL_REG2, 3, 0,
			    &lineinr_switch_controls),

    SND_SOC_DAPM_POST("RESET", post_reset),
	
	//SND_SOC_DAPM_PGA("HSL", ADAC_POWER_CTRL_REG1, 4, 0, NULL, 0),
	//SND_SOC_DAPM_PGA("HSR", ADAC_POWER_CTRL_REG1, 5, 0, NULL, 0),
	
	//SND_SOC_DAPM_PGA("PDZ", ADAC_POWER_CTRL_REG2, 7, 0, NULL, 0),
	SND_SOC_DAPM_MICBIAS("MICBIAS", ADAC_POWER_CTRL_REG2, 5, 0)
};

/* Target, Path, Source */

static const struct snd_soc_dapm_route audio_map[] = {
	{"LINEOUTL", NULL, "LINEOUTL Switch"},
	{"LINEOUTL Switch", NULL, "DACL"},
	{"LINEOUTR", NULL, "LINEOUTR Switch"},
	{"LINEOUTR Switch", NULL, "DACR"},
	
	{"HP_L", NULL, "HP_L Switch"},
	{"HP_L Switch", NULL, "DACL"},
	{"HP_R", NULL, "HP_R Switch"},
	{"HP_R Switch", NULL, "DACR"},

	{"SPEAKER", NULL, "SPEAKER Switch"},
	{"SPEAKER Switch", NULL, "DACL"},
	{"SPEAKER Switch", NULL, "DACR"},

    {"ADCL", NULL, "LINEINL Switch"},
    {"LINEINL Switch", NULL, "LINEINL"},
	{"ADCR", NULL, "LINEINR Switch"},
	{"LINEINR Switch", NULL, "LINEINR"},
};

static int amlm3_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, amlm3_dapm_widgets,
				  ARRAY_SIZE(amlm3_dapm_widgets));

	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	return 0;
}
static int aml_m3_volatile_register(unsigned int reg)
{
	return 0;
}
static int aml_m3_write(struct snd_soc_codec *codec, unsigned int reg,
							unsigned int value)
{
    u16 *reg_cache = codec->reg_cache;
	
	//printk("***Entered %s:%s:\nWriting reg is %#x; value=%#x\n",__FILE__,__func__, reg, value);
	if (reg >= codec->reg_cache_size)
		return -EINVAL;
	WRITE_APB_REG((APB_BASE+(reg<<2)), value);
	reg_cache[reg] = value;

//printk("Read back reg is %#x value=%#x\n", reg, READ_APB_REG(APB_BASE+(reg<<2)));

	return 0;
}

static unsigned int aml_m3_read(struct snd_soc_codec *codec,
							unsigned int reg)
{
	//u16 *reg_cache = codec->reg_cache;
	if (reg >= codec->reg_cache_size)
		return -EINVAL;
	
	if(codec->volatile_register(reg)){
		//return READ_APB_REG(APB_BASE+(reg<<2));
	}
	return READ_APB_REG(APB_BASE+(reg<<2));
	//return reg_cache[reg];
}

static int aml_m3_codec_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
    unsigned int i2sfs;
    unsigned long rate = params_rate(params);
    int rate_idx = 0;

    for (rate_idx = 0; rate_idx < ARRAY_SIZE(aml_rate_table); rate_idx++)
        if (aml_rate_table[rate_idx] == rate)
            break;
    if (ARRAY_SIZE(aml_rate_table) == rate_idx)
        rate_idx = 0;

    i2sfs = snd_soc_read(codec, ADAC_I2S_CONFIG_REG1);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
        snd_soc_write(codec, ADAC_I2S_CONFIG_REG1, (i2sfs&0x0f)|(rate_idx<<4));
    else
        snd_soc_write(codec, ADAC_I2S_CONFIG_REG1, (i2sfs&0xf0)|rate_idx);

    aml_reset_path(codec, AML_PWR_KEEP);
	return 0;
}


static int aml_m3_codec_pcm_prepare(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	/* set active */
	
	// TODO

	return 0;
}

static void aml_m3_codec_shutdown(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	/* deactivate */
	if (!codec->active) {
		udelay(50);
		
		// TODO
	}
}

static int aml_m3_codec_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 reg;
	// TODO

	reg = snd_soc_read(codec, ADAC_MUTE_CTRL_REG1);
	if(mute){
		reg |= 3;
	}
	else{
		reg &= ~3;
	}
	printk("aml_m3_codec_mute mute=%d\n",mute);
//	snd_soc_write(codec, ADAC_MUTE_CTRL_REG1, reg);
	return 0;
}

static int aml_m3_codec_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aml_m3_codec_priv *aml = codec->private_data;
	unsigned long data = 0;
	
	aml->sysclk = freq;
	switch (freq) {
	case 32000:
		data = 6;
		break;
	case 44100:
		data = 7;
		break;
	case 48000:
		data = 8;
		break;
	case 96000:
		data = 10;
		break;
	default:
		data = 6;
		break;
	}
	//snd_soc_write(codec,ADAC_CLOCK, 0);
	//snd_soc_write(codec,ADAC_I2S_CONFIG_REG1, data);
	return 0;
}


static int aml_m3_codec_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	u16 iface = 0;
	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface |= 0x0040;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}

	/* set iface */
	
	// TODO
	
	return 0;
}

#define AML_RATES SNDRV_PCM_RATE_8000_96000

#define AML_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE)


static struct snd_soc_dai_ops aml_m3_codec_dai_ops = {
	.prepare	= aml_m3_codec_pcm_prepare,
	.hw_params	= aml_m3_codec_hw_params,
	.shutdown	= aml_m3_codec_shutdown,
	.digital_mute	= aml_m3_codec_mute,
	.set_sysclk	= aml_m3_codec_set_dai_sysclk,
	.set_fmt	= aml_m3_codec_set_dai_fmt,
};

struct snd_soc_dai aml_m3_codec_dai = {
	.name = "AML-M3",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AML_RATES,
		.formats = AML_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AML_RATES,
		.formats = AML_FORMATS,},
	.ops = &aml_m3_codec_dai_ops,
	.symmetric_rates = 1,
};
EXPORT_SYMBOL_GPL(aml_m3_codec_dai);

static int aml_m3_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
	    break;
	default:
	    break;
	}
	codec->bias_level = level;
	return 0;
}

static int aml_m3_codec_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	if (!aml_m3_codec) {
		dev_err(&pdev->dev, "AML_M3_CODEC not yet discovered\n");
		return -ENODEV;
	}
	codec = aml_m3_codec;			
	socdev->card->codec = codec;	
	
	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		kfree(codec);
		dev_err(codec->dev, "aml m3 codec: failed to create pcms: %d\n", ret);
		goto pcm_err;
	}

	snd_soc_add_controls(codec, amlm3_snd_controls,
				ARRAY_SIZE(amlm3_snd_controls));
	amlm3_add_widgets(codec);
	
pcm_err:

	return ret;
}


static int aml_m3_codec_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
	return 0;
}

#ifdef CONFIG_PM
static int aml_m3_codec_suspend(struct platform_device* pdev, pm_message_t state)
{
    struct snd_soc_device *socdev = platform_get_drvdata(pdev);
    struct snd_soc_codec *codec = socdev->card->codec;

    printk("aml_m3_codec_suspend\n");

    WRITE_MPEG_REG( HHI_GCLK_MPEG1, READ_MPEG_REG(HHI_GCLK_MPEG1)&~(1 << 2));
    aml_reset_path(codec, AML_PWR_DOWN);
    return 0;
}

static int aml_m3_codec_resume(struct platform_device* pdev)
{
    struct snd_soc_device *socdev = platform_get_drvdata(pdev);
    struct snd_soc_codec *codec = socdev->card->codec;

    printk("aml_m3_codec resume\n");

    WRITE_MPEG_REG( HHI_GCLK_MPEG1, READ_MPEG_REG(HHI_GCLK_MPEG1)|(1 << 2));
    aml_m3_reset(codec, true);
    return 0;
}
#endif

struct snd_soc_codec_device soc_codec_dev_aml_m3 = {
	.probe =	aml_m3_codec_probe,
	.remove =	aml_m3_codec_remove,
#ifdef CONFIG_PM	
	.suspend = aml_m3_codec_suspend,
	.resume = aml_m3_codec_resume,
#else
	.suspend = NULL,
	.resume = NULL,
#endif
};
EXPORT_SYMBOL_GPL(soc_codec_dev_aml_m3);

static int aml_m3_register(struct aml_m3_codec_priv* aml_m3)
{
	struct snd_soc_codec* codec = &aml_m3->codec;
	int ret;
		
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	printk("***Entered %s:%s\n",__FILE__,__func__);

	codec->name = "AML_M3_CODEC";
	codec->owner = THIS_MODULE;
	codec->private_data = aml_m3;

	codec->dai = &aml_m3_codec_dai;
	codec->num_dai = 1;

	codec->reg_cache = &aml_m3->reg_cache;
	codec->reg_cache_size = ARRAY_SIZE(aml_m3->reg_cache);
	codec->read = aml_m3_read;
	
	codec->write = aml_m3_write;
	codec->volatile_register = aml_m3_volatile_register;
	
	codec->bias_level = SND_SOC_BIAS_OFF;
	codec->set_bias_level = aml_m3_set_bias_level;
	aml_m3_codec_dai.dev = codec->dev;
	
	aml_m3_reset(codec, true);
	aml_m3_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	aml_m3_codec = codec;

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_dai(&aml_m3_codec_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		goto err_codec;
	}

	return 0;

err_codec:
	snd_soc_unregister_codec(codec);
err:
	aml_m3_codec = NULL;
	kfree(aml_m3);
	return ret;
		
}

static void aml_m3_unregister(struct aml_m3_codec_priv *aml_m3)
{
	snd_soc_unregister_dai(&aml_m3_codec_dai);
	snd_soc_unregister_codec(&aml_m3->codec);
	aml_m3_codec = NULL;
	kfree(aml_m3);
}

static int aml_m3_codec_platform_probe(struct platform_device *pdev)
{
	struct aml_m3_codec_priv *aml_m3;
	struct snd_soc_codec *codec;

	aml_m3 = kzalloc(sizeof(struct aml_m3_codec_priv), GFP_KERNEL);
	if (aml_m3 == NULL)
		return -ENOMEM;

	codec = &aml_m3->codec;

	codec->control_data = NULL;
	codec->hw_write = NULL;
	codec->pop_time = 0;

	codec->dev = &pdev->dev;
	platform_set_drvdata(pdev, aml_m3);
	return aml_m3_register(aml_m3);
}

static int __exit aml_m3_codec_platform_remove(struct platform_device *pdev)
{
	struct aml_m3_codec_priv *aml_m3 = platform_get_drvdata(pdev);

	aml_m3_unregister(aml_m3);
	return 0;
}

static struct platform_driver aml_m3_codec_platform_driver = {
	.driver = {
		.name = "aml_m3_codec",
		.owner = THIS_MODULE,
		},
	.probe = aml_m3_codec_platform_probe,
	.remove = __exit_p(aml_m3_codec_platform_remove),
};

static int __init aml_m3_codec_modinit(void)
{
		return platform_driver_register(&aml_m3_codec_platform_driver);
}

static void __exit aml_m3_codec_exit(void)
{
		platform_driver_unregister(&aml_m3_codec_platform_driver);
}

module_init(aml_m3_codec_modinit);
module_exit(aml_m3_codec_exit);


MODULE_DESCRIPTION("ASoC AML M3 codec driver");
MODULE_AUTHOR("AMLogic Inc.");
MODULE_LICENSE("GPL");
