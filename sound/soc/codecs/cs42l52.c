/*
 * cs42l52.c -- CS42L52 ALSA SoC audio driver
 *
 * Copyright 2007 CirrusLogic, Inc. 
 *
 * Author: Bo Liu <Bo.Liu@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * Revision history
 * Nov 2007  Initial version.
 * Oct 2008  Updated to 2.6.26
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "cs42l52.h"

#define DEBUG
#ifdef DEBUG
#define SOCDBG(fmt, arg...)	printk(KERN_ERR "%s: %s() " fmt, SOC_CS42L52_NAME, __FUNCTION__, ##arg)
#else
#define SOCDBG(fmt, arg...)
#endif
#define SOCINF(fmt, args...)	printk(KERN_INFO "%s: " fmt, SOC_CS42L52_NAME,  ##args)
#define SOCERR(fmt, args...)	printk(KERN_ERR "%s: " fmt, SOC_CS42L52_NAME,  ##args)
static const u8 soc_cs42l52_reg_default[] = {
	0x00, 0xE0, 0x01, 0x07, 0x05, /*4*/
	0xa0, 0x00, 0x00, 0x81, /*8*/
	0x81, 0xa5, 0x00, 0x00, /*12*/
	0x60, 0x02, 0x00, 0x00, /*16*/
	0x00, 0x00, 0x00, 0x00, /*20*/
	0x00, 0x00, 0x00, 0x80, /*24*/
	0x80, 0x00, 0x00, 0x00, /*28*/
	0x00, 0x00, 0x88, 0x00, /*32*/
	0x00, 0x00, 0x00, 0x00, /*36*/
	0x00, 0x00, 0x00, 0x7f, /*40*/
	0xc0, 0x00, 0x3f, 0x00, /*44*/
	0x00, 0x00, 0x00, 0x00, /*48*/
	0x00, 0x3b, 0x00, 0x5f, /*52*/
};
struct cs42l52_priv {
	unsigned int sysclk;
	struct snd_soc_codec codec;
	struct snd_pcm_hw_constraint_list *sysclk_constraints;
	u16 reg_cache[53];
};
static struct snd_soc_device *soc_cs42l52_pdev;

static int soc_cs42l52_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level);
static int soc_cs42l52_pcm_hw_params(struct snd_pcm_substream *substream,
                        struct snd_pcm_hw_params *params, struct snd_soc_dai *dai);
static int soc_cs42l52_set_sysclk(struct snd_soc_dai *codec_dai,
                        int clk_id, u_int freq, int dir);
static int soc_cs42l52_digital_mute(struct snd_soc_dai *dai, int mute);
static int soc_cs42l52_set_fmt(struct snd_soc_dai *codec_dai,
                        u_int fmt);
static unsigned int snd_soc_read(struct snd_soc_codec *codec,
		                u_int reg);
unsigned int tca_cs42L52_initcodec(struct snd_soc_codec *hI2C);
static void soc_cs42l52_work(struct work_struct *work);

static int cs42l52_i2c_write(struct i2c_client *client, char *buf, int count)
{
	printk("%s\n",__FUNCTION__);
	return i2c_master_send(client, buf, count);
}

static int cs42l52_i2c_read(struct i2c_client *client, char *buf, int count)
{
	printk("%s\n",__FUNCTION__);
	return i2c_master_recv(client, buf, count);
}

 static int ascii2int(char code)
{
	int val = 0;
    if ((code >= 'a') && (code <= 'f'))
    	val = code - 'a' + 10;
    else if ((code >= 'A') && (code <= 'F'))
    	val = code - 'A' + 10;
    else if ((code >= '0') && (code <= '9'))
    	val = code - '0';
    return val;
}

static ssize_t cs42l52_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);	
    u8 reg_addr, reg_data;
    
    if (!strcmp(attr->attr.name, "reg")) {
            printk(KERN_ALERT"input = %s\n", buf);
            reg_addr = ascii2int(buf[4])*16 + ascii2int(buf[5]);
    	    reg_data = ascii2int(buf[9])*16 + ascii2int(buf[10]);
			//sscanf(buf+2, "%x", &reg_addr);
			//sscanf(buf+7, "%x", &reg_data);
			if (buf[0]== 'w') {
				printk(KERN_ALERT"write register(0x%x)= 0x%x\n",reg_addr, reg_data);
				snd_soc_write(codec, reg_addr, reg_data);
			}
			else if (buf[0]== 'r') {
				for (; reg_addr <0x35;reg_addr++){
					reg_data = snd_soc_read(codec, reg_addr);
					printk(KERN_ALERT"read register(0x%x)= 0x%x\n",reg_addr, reg_data);
				}
			}
    }
    return count;
}

static DEVICE_ATTR(reg, S_IRWXUGO, 0, cs42l52_write);

static struct attribute *cs42l52_attr[] = {
    &dev_attr_reg.attr,
    NULL
};

static struct attribute_group cs42l52_attr_group = {
    .name = NULL,
    .attrs = cs42l52_attr,
};



/**
 * snd_soc_get_volsw - single mixer get callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to get the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_cs42l5x_get_volsw(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_value *ucontrol)
{
	printk("%s\n",__FUNCTION__);
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int reg = kcontrol->private_value & 0xff;
    int shift = (kcontrol->private_value >> 8) & 0x0f;
    int rshift = (kcontrol->private_value >> 12) & 0x0f;
    int max = (kcontrol->private_value >> 16) & 0xff;
    int mask = (1 << fls(max)) - 1;
    int min = (kcontrol->private_value >> 24) & 0xff;

    ucontrol->value.integer.value[0] =
	((snd_soc_read(codec, reg) >> shift) - min) & mask;
    if (shift != rshift)
	ucontrol->value.integer.value[1] =
	    ((snd_soc_read(codec, reg) >> rshift) - min) & mask;

    return 0;
}

/**
 * snd_soc_put_volsw - single mixer put callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to set the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_cs42l5x_put_volsw(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_value *ucontrol)
{
	printk("%s\n",__FUNCTION__);
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int reg = kcontrol->private_value & 0xff;
    int shift = (kcontrol->private_value >> 8) & 0x0f;
    int rshift = (kcontrol->private_value >> 12) & 0x0f;
    int max = (kcontrol->private_value >> 16) & 0xff;
    int mask = (1 << fls(max)) - 1;
    int min = (kcontrol->private_value >> 24) & 0xff;
    unsigned short val, val2, val_mask;

    val = ((ucontrol->value.integer.value[0] + min) & mask);

    val_mask = mask << shift;
    val = val << shift;
    if (shift != rshift) {
	val2 = ((ucontrol->value.integer.value[1] + min) & mask);
	val_mask |= mask << rshift;
	val |= val2 << rshift;
    }
    return snd_soc_update_bits(codec, reg, val_mask, val);
}

/**
 * snd_soc_info_volsw_2r - double mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a double mixer control that
 * spans 2 codec registers.
 *
 * Returns 0 for success.
 */
int snd_soc_cs42l5x_info_volsw_2r(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_info *uinfo)
{
	printk("%s\n",__FUNCTION__);
    int max = (kcontrol->private_value >> 8) & 0xff;

    if (max == 1)
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
    else
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

    uinfo->count = 2;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = max;
    return 0;
}

/**
 * snd_soc_get_volsw_2r - double mixer get callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to get the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 */
int snd_soc_cs42l5x_get_volsw_2r(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_value *ucontrol)
{
		printk("%s\n",__FUNCTION__);
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int reg = kcontrol->private_value & 0xff;
    int reg2 = (kcontrol->private_value >> 24) & 0xff;
    int max = (kcontrol->private_value >> 8) & 0xff;
    int min = (kcontrol->private_value >> 16) & 0xff;
    int mask = (1<<fls(max))-1;
    int val, val2;

    val = snd_soc_read(codec, reg);
    val2 = snd_soc_read(codec, reg2);
    ucontrol->value.integer.value[0] = (val - min) & mask;
    ucontrol->value.integer.value[1] = (val2 - min) & mask;
    return 0;
}

int snd_soc_cs42l5x_put_volsw_2r(struct snd_kcontrol *kcontrol,
    struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    int reg = kcontrol->private_value & 0xff;
    int reg2 = (kcontrol->private_value >> 24) & 0xff;
    int max = (kcontrol->private_value >> 8) & 0xff;
    int min = (kcontrol->private_value >> 16) & 0xff;
    int mask = (1 << fls(max)) - 1;
    int err;
    unsigned short val, val2;

    val = (ucontrol->value.integer.value[0] + min) & mask;
    val2 = (ucontrol->value.integer.value[1] + min) & mask;

    if ((err = snd_soc_update_bits(codec, reg, mask, val)) < 0)
	return err;

    err = snd_soc_update_bits(codec, reg2, mask, val2);
    return err;
}

#define SOC_SINGLE_CS42L52(xname, reg, shift, max, min) \
{       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
        .info = snd_soc_info_volsw, .get = snd_soc_cs42l5x_get_volsw,\
        .put = snd_soc_cs42l5x_put_volsw, \
        .private_value =  SOC_SINGLE_VALUE(reg, shift, max, min) }

#define SOC_DOUBLE_CS42L52(xname, reg, shift_left, shift_right, max, min) \
{       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname),\
        .info = snd_soc_info_volsw, .get = snd_soc_cs42l5x_get_volsw, \
        .put = snd_soc_cs42l5x_put_volsw, \
        .private_value = (reg) | ((shift_left) << 8) | \
                ((shift_right) << 12) | ((max) << 16) | ((min) << 24) }

/* No shifts required */
#define SOC_DOUBLE_R_CS42L52(xname, reg_left, reg_right, max, min) \
{       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
        .info = snd_soc_cs42l5x_info_volsw_2r, \
        .get = snd_soc_cs42l5x_get_volsw_2r, .put = snd_soc_cs42l5x_put_volsw_2r, \
        .private_value = (reg_left) | ((max) << 8) | ((min) << 16) | \
		((reg_right) << 24) }


/*
 * CS42L52 register default value
 */



static inline int soc_cs42l52_get_revison(struct snd_soc_codec *codec)
{
	u8 data;
        u8 addr;
	int num, ret = 0;
	printk("%s\n",__FUNCTION__);
	addr = CODEC_CS42L52_CHIP;

	if(cs42l52_i2c_write(codec->control_data, &addr, 1) == 1)
	{
		if(cs42l52_i2c_read(codec->control_data, &data, 1) == 1)
		{
			if((data & CHIP_ID_MASK) != CHIP_ID )
			{
				SOCDBG("data is %04x\n",  data);
				ret = -ENODEV;
			}
		}
		else
			ret = -EIO;
	}
	else
		ret = -EIO;

	return ret < 0 ? ret : data;
}

static const char *cs42l52_mic_bias[] = {"0.5VA", "0.6VA", "0.7VA", "0.8VA", "0.83VA", "0.91VA"};
static const char *cs42l52_hpf_freeze[] = {"Continuous DC Subtraction", "Frozen DC Subtraction"};
static const char *cs42l52_hpf_corner_freq[] = {"Normal", "119Hz", "236Hz", "464Hz"};
static const char *cs42l52_adc_sum[] = {"Normal", "Sum half", "Sub half", "Inverted"};
static const char *cs42l52_sig_polarity[] = {"Normal", "Inverted"};
static const char *cs42l52_spk_mono_channel[] = {"ChannelA", "ChannelB"};
static const char *cs42l52_beep_type[] = {"Off", "Single", "Multiple", "Continuous"};
static const char *cs42l52_treble_freq[] = {"5kHz", "7kHz", "10kHz", "15kHz"};
static const char *cs42l52_bass_freq[] = {"50Hz", "100Hz", "200Hz", "250Hz"};
static const char *cs42l52_target_sel[] = {"Apply Specific", "Apply All"};
static const char *cs42l52_noise_gate_delay[] = {"50ms", "100ms", "150ms", "200ms"};
static const char *cs42l52_adc_mux[] = {"AIN1", "AIN2", "AIN3", "AIN4", "PGA"};
static const char *cs42l52_mic_mux[] = {"MIC1", "MIC2"};
static const char *cs42l52_stereo_mux[] = {"Mono", "Stereo"};
static const char *cs42l52_off[] = {"On", "Off"};
static const char *cs42l52_hpmux[] = {"Off", "On"};

static const struct soc_enum soc_cs42l52_enum[] = {
SOC_ENUM_DOUBLE(CODEC_CS42L52_ANALOG_HPF_CTL, 4, 6, 2, cs42l52_hpf_freeze), /*0*/
SOC_ENUM_SINGLE(CODEC_CS42L52_ADC_HPF_FREQ, 0, 4, cs42l52_hpf_corner_freq),
SOC_ENUM_SINGLE(CODEC_CS42L52_ADC_MISC_CTL, 4, 4, cs42l52_adc_sum),
SOC_ENUM_DOUBLE(CODEC_CS42L52_ADC_MISC_CTL, 2, 3, 2, cs42l52_sig_polarity),
SOC_ENUM_DOUBLE(CODEC_CS42L52_PB_CTL1, 2, 3, 2, cs42l52_sig_polarity),
SOC_ENUM_SINGLE(CODEC_CS42L52_PB_CTL2, 2, 2, cs42l52_spk_mono_channel), /*5*/
SOC_ENUM_SINGLE(CODEC_CS42L52_BEEP_TONE_CTL, 6, 4, cs42l52_beep_type),
SOC_ENUM_SINGLE(CODEC_CS42L52_BEEP_TONE_CTL, 3, 4, cs42l52_treble_freq),
SOC_ENUM_SINGLE(CODEC_CS42L52_BEEP_TONE_CTL, 1, 4, cs42l52_bass_freq),
SOC_ENUM_SINGLE(CODEC_CS42L52_LIMITER_CTL2, 6, 2, cs42l52_target_sel),
SOC_ENUM_SINGLE(CODEC_CS42L52_NOISE_GATE_CTL, 7, 2, cs42l52_target_sel), /*10*/
SOC_ENUM_SINGLE(CODEC_CS42L52_NOISE_GATE_CTL, 0, 4, cs42l52_noise_gate_delay),
SOC_ENUM_SINGLE(CODEC_CS42L52_ADC_PGA_A, 5, 5, cs42l52_adc_mux),
SOC_ENUM_SINGLE(CODEC_CS42L52_ADC_PGA_B, 5, 5, cs42l52_adc_mux),
SOC_ENUM_SINGLE(CODEC_CS42L52_MICA_CTL, 6, 2, cs42l52_mic_mux),
SOC_ENUM_SINGLE(CODEC_CS42L52_MICB_CTL, 6, 2, cs42l52_mic_mux), /*15*/
SOC_ENUM_SINGLE(CODEC_CS42L52_MICA_CTL, 5, 2, cs42l52_stereo_mux),
SOC_ENUM_SINGLE(CODEC_CS42L52_MICB_CTL, 5, 2, cs42l52_stereo_mux),
SOC_ENUM_SINGLE(CODEC_CS42L52_IFACE_CTL2, 0, 6, cs42l52_mic_bias), /*18*/
SOC_ENUM_SINGLE(CODEC_CS42L52_PWCTL2, 0, 2, cs42l52_off),
SOC_ENUM_SINGLE(CODEC_CS42L52_MISC_CTL, 6, 2, cs42l52_hpmux),
SOC_ENUM_SINGLE(CODEC_CS42L52_MISC_CTL, 7, 2, cs42l52_hpmux),
};

static const struct snd_kcontrol_new soc_cs42l52_controls[] = {

SOC_ENUM("Mic VA Capture Switch", soc_cs42l52_enum[18]), /*0*/
SOC_DOUBLE("HPF Capture Switch", CODEC_CS42L52_ANALOG_HPF_CTL, 5, 7, 1, 0),
SOC_ENUM("HPF Freeze Capture Switch", soc_cs42l52_enum[0]),

SOC_DOUBLE("Analog SR Capture Switch", CODEC_CS42L52_ANALOG_HPF_CTL, 1, 3, 1, 1),
SOC_DOUBLE("Analog ZC Capture Switch", CODEC_CS42L52_ANALOG_HPF_CTL, 0, 2, 1, 1),
SOC_ENUM("HPF corner freq Capture Switch", soc_cs42l52_enum[1]), /*5*/

SOC_SINGLE("Ganged Ctl Capture Switch", CODEC_CS42L52_ADC_MISC_CTL, 7, 1, 1), /* should be enabled init */
SOC_ENUM("Mix/Swap Capture Switch",soc_cs42l52_enum[2]),
SOC_ENUM("Signal Polarity Capture Switch", soc_cs42l52_enum[3]),

SOC_SINGLE("HP Analog Gain Playback Volume", CODEC_CS42L52_PB_CTL1, 5, 7, 0),
SOC_SINGLE("HP Analog Gain Playback Volume", CODEC_CS42L52_PB_CTL1, 5, 1, 0),
SOC_SINGLE("Playback B=A Volume Playback Switch", CODEC_CS42L52_PB_CTL1, 4, 1, 0), /*10*/ /*should be enabled init*/ 
SOC_ENUM("PCM Signal Polarity Playback Switch",soc_cs42l52_enum[4]),

SOC_SINGLE("Digital De-Emphasis Playback Switch", CODEC_CS42L52_MISC_CTL, 2, 1, 0),
SOC_SINGLE("Digital SR Playback Switch", CODEC_CS42L52_MISC_CTL, 1, 1, 0),
SOC_SINGLE("Digital ZC Playback Switch", CODEC_CS42L52_MISC_CTL, 0, 1, 0),

SOC_SINGLE("Spk Volume Equal Playback Switch", CODEC_CS42L52_PB_CTL2, 3, 1, 0) , /*15*/ /*should be enabled init*/
SOC_SINGLE("Spk Mute 50/50 Playback Switch", CODEC_CS42L52_PB_CTL2, 0, 1, 0),
SOC_ENUM("Spk Swap Channel Playback Switch", soc_cs42l52_enum[5]),
SOC_SINGLE("Spk Full-Bridge Playback Switch", CODEC_CS42L52_PB_CTL2, 1, 1, 0),
SOC_DOUBLE_R("Mic Gain Capture Volume", CODEC_CS42L52_MICA_CTL, CODEC_CS42L52_MICB_CTL, 0, 31, 0),

SOC_DOUBLE_R("ALC SR Capture Switch", CODEC_CS42L52_PGAA_CTL, CODEC_CS42L52_PGAB_CTL, 7, 1, 1), /*20*/
SOC_DOUBLE_R("ALC ZC Capture Switch", CODEC_CS42L52_PGAA_CTL, CODEC_CS42L52_PGAB_CTL, 6, 1, 1),
SOC_DOUBLE_R_CS42L52("PGA Capture Volume", CODEC_CS42L52_PGAA_CTL, CODEC_CS42L52_PGAB_CTL, 0x30, 0x18),

SOC_DOUBLE_R_CS42L52("Passthru Playback Volume", CODEC_CS42L52_PASSTHRUA_VOL, CODEC_CS42L52_PASSTHRUB_VOL, 0x90, 0x88),
SOC_DOUBLE("Passthru Playback Switch", CODEC_CS42L52_MISC_CTL, 4, 5, 1, 1),
SOC_DOUBLE_R_CS42L52("ADC Capture Volume", CODEC_CS42L52_ADCA_VOL, CODEC_CS42L52_ADCB_VOL, 0x80, 0xA0),
SOC_DOUBLE("ADC Capture Switch", CODEC_CS42L52_ADC_MISC_CTL, 0, 1, 1, 1),
SOC_DOUBLE_R_CS42L52("ADC Mixer Capture Volume", CODEC_CS42L52_ADCA_MIXER_VOL, CODEC_CS42L52_ADCB_MIXER_VOL, 0x7f, 0x19),
SOC_DOUBLE_R("ADC Mixer Capture Switch", CODEC_CS42L52_ADCA_MIXER_VOL, CODEC_CS42L52_ADCB_MIXER_VOL, 7, 1, 1),
SOC_DOUBLE_R_CS42L52("PCM Mixer Playback Volume", CODEC_CS42L52_PCMA_MIXER_VOL, CODEC_CS42L52_PCMB_MIXER_VOL, 0x7f, 0x19),
SOC_DOUBLE_R("PCM Mixer Playback Switch", CODEC_CS42L52_PCMA_MIXER_VOL, CODEC_CS42L52_PCMB_MIXER_VOL, 7, 1, 1),

SOC_SINGLE("Beep Freq", CODEC_CS42L52_BEEP_FREQ, 4, 15, 0),
SOC_SINGLE("Beep OnTime", CODEC_CS42L52_BEEP_FREQ, 0, 15, 0), /*30*/
SOC_SINGLE_CS42L52("Beep Volume", CODEC_CS42L52_BEEP_VOL, 0, 0x1f, 0x07),
SOC_SINGLE("Beep OffTime", CODEC_CS42L52_BEEP_VOL, 5, 7, 0),
SOC_ENUM("Beep Type", soc_cs42l52_enum[6]),
SOC_SINGLE("Beep Mix Switch", CODEC_CS42L52_BEEP_TONE_CTL, 5, 1, 1),

SOC_ENUM("Treble Corner Freq Playback Switch", soc_cs42l52_enum[7]), /*35*/
SOC_ENUM("Bass Corner Freq Playback Switch",soc_cs42l52_enum[8]),
SOC_SINGLE("Tone Control Playback Switch", CODEC_CS42L52_BEEP_TONE_CTL, 0, 1, 0),
SOC_SINGLE("Treble Gain Playback Volume", CODEC_CS42L52_TONE_CTL, 4, 15, 1),
SOC_SINGLE("Bass Gain Playback Volume", CODEC_CS42L52_TONE_CTL, 0, 15, 1),

SOC_DOUBLE_R_CS42L52("Master Playback Volume", CODEC_CS42L52_MASTERA_VOL, CODEC_CS42L52_MASTERB_VOL,0xe4, 0x34), /*40*/

SOC_DOUBLE_R_CS42L52("HP Digital Playback Volume", CODEC_CS42L52_HPA_VOL, CODEC_CS42L52_HPB_VOL, 0xff, 0x1),
SOC_DOUBLE("HP Digital Playback Switch", CODEC_CS42L52_PB_CTL2, 6, 7, 1, 1),
SOC_DOUBLE_R_CS42L52("Speaker Playback Volume", CODEC_CS42L52_SPKA_VOL, CODEC_CS42L52_SPKB_VOL, 0xff, 0x1),
SOC_DOUBLE("Speaker Playback Switch", CODEC_CS42L52_PB_CTL2, 4, 5, 1, 1),

SOC_SINGLE("Limiter Max Threshold Playback Volume", CODEC_CS42L52_LIMITER_CTL1, 5, 7, 0),
SOC_SINGLE("Limiter Cushion Threshold Playback Volume", CODEC_CS42L52_LIMITER_CTL1, 2, 7, 0),
SOC_SINGLE("Limiter SR Playback Switch", CODEC_CS42L52_LIMITER_CTL1, 1, 1, 0), /*45*/
SOC_SINGLE("Limiter ZC Playback Switch", CODEC_CS42L52_LIMITER_CTL1, 0, 1, 0),
SOC_SINGLE("Limiter Playback Switch", CODEC_CS42L52_LIMITER_CTL2, 7, 1, 0),
SOC_ENUM("Limiter Attnenuate Playback Switch", soc_cs42l52_enum[9]),
SOC_SINGLE("Limiter Release Rate Playback Volume", CODEC_CS42L52_LIMITER_CTL2, 0, 63, 0),
SOC_SINGLE("Limiter Attack Rate Playback Volume", CODEC_CS42L52_LIMITER_AT_RATE, 0, 63, 0), /*50*/

SOC_DOUBLE("ALC Capture Switch",CODEC_CS42L52_ALC_CTL, 6, 7, 1, 0),
SOC_SINGLE("ALC Attack Rate Capture Volume", CODEC_CS42L52_ALC_CTL, 0, 63, 0),
SOC_SINGLE("ALC Release Rate Capture Volume", CODEC_CS42L52_ALC_RATE, 0, 63, 0),
SOC_SINGLE("ALC Max Threshold Capture Volume", CODEC_CS42L52_ALC_THRESHOLD, 5, 7, 0),
SOC_SINGLE("ALC Min Threshold Capture Volume", CODEC_CS42L52_ALC_THRESHOLD, 2, 7, 0), /*55*/

SOC_ENUM("Noise Gate Type Capture Switch", soc_cs42l52_enum[10]),
SOC_SINGLE("Noise Gate Capture Switch", CODEC_CS42L52_NOISE_GATE_CTL, 6, 1, 0),
SOC_SINGLE("Noise Gate Boost Capture Switch", CODEC_CS42L52_NOISE_GATE_CTL, 5, 1, 1),
SOC_SINGLE("Noise Gate Threshold Capture Volume", CODEC_CS42L52_NOISE_GATE_CTL, 2, 7, 0),
SOC_ENUM("Noise Gate Delay Time Capture Switch", soc_cs42l52_enum[11]), /*60*/

SOC_SINGLE("Batt Compensation Switch", CODEC_CS42L52_BATT_COMPEN, 7, 1, 0),
SOC_SINGLE("Batt VP Monitor Switch", CODEC_CS42L52_BATT_COMPEN, 6, 1, 0),
SOC_SINGLE("Batt VP ref", CODEC_CS42L52_BATT_COMPEN, 0, 0x0f, 0),
SOC_SINGLE("Playback Charge Pump Freq", CODEC_CS42L52_CHARGE_PUMP, 4, 15, 0), /*64*/

};

static const struct snd_kcontrol_new cs42l52_adca_mux =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[12]);

static const struct snd_kcontrol_new cs42l52_adcb_mux =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[13]);

static const struct snd_kcontrol_new cs42l52_mica_mux =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[14]);

static const struct snd_kcontrol_new cs42l52_micb_mux =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[15]);

static const struct snd_kcontrol_new cs42l52_mica_stereo_mux =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[16]);

static const struct snd_kcontrol_new cs42l52_micb_stereo_mux =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[17]);

static const struct snd_kcontrol_new cs42l52_passa_switch =
SOC_DAPM_SINGLE("Switch", CODEC_CS42L52_MISC_CTL, 6, 1, 0);

static const struct snd_kcontrol_new cs42l52_passb_switch =
SOC_DAPM_SINGLE("Switch", CODEC_CS42L52_MISC_CTL, 7, 1, 0);

static const struct snd_kcontrol_new cs42l52_micbias_switch =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[19]);

static const struct snd_kcontrol_new cs42l52_hpa_mux =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[20]);

static const struct snd_kcontrol_new cs42l52_hpb_mux =
SOC_DAPM_ENUM("Route", soc_cs42l52_enum[21]);

static const struct snd_soc_dapm_widget soc_cs42l52_dapm_widgets[] = {
	/* Input path */
	SND_SOC_DAPM_ADC("ADC Left", "Capture", CODEC_CS42L52_PWCTL1, 1, 1),
	SND_SOC_DAPM_ADC("ADC Right", "Capture", CODEC_CS42L52_PWCTL1, 2, 1),

	SND_SOC_DAPM_MUX("MICA Mux Capture Switch", SND_SOC_NOPM, 0, 0, &cs42l52_mica_mux),
	SND_SOC_DAPM_MUX("MICB Mux Capture Switch", SND_SOC_NOPM, 0, 0, &cs42l52_micb_mux),
	SND_SOC_DAPM_MUX("MICA Stereo Mux Capture Switch", SND_SOC_NOPM, 1, 0, &cs42l52_mica_stereo_mux),
	SND_SOC_DAPM_MUX("MICB Stereo Mux Capture Switch", SND_SOC_NOPM, 2, 0, &cs42l52_micb_stereo_mux),

	SND_SOC_DAPM_MUX("ADC Mux Left Capture Switch", SND_SOC_NOPM, 1, 1, &cs42l52_adca_mux),
	SND_SOC_DAPM_MUX("ADC Mux Right Capture Switch", SND_SOC_NOPM, 2, 1, &cs42l52_adcb_mux),

	/* Sum switches */
	SND_SOC_DAPM_PGA("AIN1A Switch", CODEC_CS42L52_ADC_PGA_A, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AIN2A Switch", CODEC_CS42L52_ADC_PGA_A, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AIN3A Switch", CODEC_CS42L52_ADC_PGA_A, 2, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AIN4A Switch", CODEC_CS42L52_ADC_PGA_A, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MICA Switch" , CODEC_CS42L52_ADC_PGA_A, 4, 0, NULL, 0),

	SND_SOC_DAPM_PGA("AIN1B Switch", CODEC_CS42L52_ADC_PGA_B, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AIN2B Switch", CODEC_CS42L52_ADC_PGA_B, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AIN3B Switch", CODEC_CS42L52_ADC_PGA_B, 2, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AIN4B Switch", CODEC_CS42L52_ADC_PGA_B, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MICB Switch" , CODEC_CS42L52_ADC_PGA_B, 4, 0, NULL, 0),

	/* MIC PGA Power */
	SND_SOC_DAPM_PGA("PGA MICA", CODEC_CS42L52_PWCTL2, PWCTL2_PDN_MICA_SHIFT, 1, NULL, 0),
	SND_SOC_DAPM_PGA("PGA MICB", CODEC_CS42L52_PWCTL2, PWCTL2_PDN_MICB_SHIFT, 1, NULL, 0),

	/* MIC bias switch */
	SND_SOC_DAPM_MUX("Mic Bias Capture Switch", SND_SOC_NOPM, 0, 0, &cs42l52_micbias_switch),
	SND_SOC_DAPM_PGA("Mic-Bias", CODEC_CS42L52_PWCTL2, 0, 1, NULL, 0),

	/* PGA Power */
	SND_SOC_DAPM_PGA("PGA Left", CODEC_CS42L52_PWCTL1, PWCTL1_PDN_PGAA_SHIFT, 1, NULL, 0),
	SND_SOC_DAPM_PGA("PGA Right", CODEC_CS42L52_PWCTL1, PWCTL1_PDN_PGAB_SHIFT, 1, NULL, 0),

	/* Output path */
	SND_SOC_DAPM_MUX("Passthrough Left Playback Switch", SND_SOC_NOPM, 0, 0, &cs42l52_hpa_mux),
	SND_SOC_DAPM_MUX("Passthrough Right Playback Switch", SND_SOC_NOPM, 0, 0, &cs42l52_hpb_mux),

	SND_SOC_DAPM_DAC("DAC Left", "Playback", SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Right", "Playback", SND_SOC_NOPM, 0, 0),

	//SND_SOC_DAPM_PGA("HP Amp Left", CODEC_CS42L52_PWCTL3, 4, 1, NULL, 0),
	//SND_SOC_DAPM_PGA("HP Amp Right", CODEC_CS42L52_PWCTL3, 6, 1, NULL, 0),

	//SND_SOC_DAPM_PGA("SPK Pwr Left", CODEC_CS42L52_PWCTL3, 0, 1, NULL, 0),
	//SND_SOC_DAPM_PGA("SPK Pwr Right", CODEC_CS42L52_PWCTL3, 2, 1, NULL, 0),

	SND_SOC_DAPM_OUTPUT("HPA"),
	SND_SOC_DAPM_OUTPUT("HPB"),
	SND_SOC_DAPM_OUTPUT("SPKA"),
	SND_SOC_DAPM_OUTPUT("SPKB"),
	SND_SOC_DAPM_OUTPUT("MICBIAS"),

	SND_SOC_DAPM_INPUT("INPUT1A"),
	SND_SOC_DAPM_INPUT("INPUT2A"),
	SND_SOC_DAPM_INPUT("INPUT3A"),
	SND_SOC_DAPM_INPUT("INPUT4A"),
	SND_SOC_DAPM_INPUT("INPUT1B"),
	SND_SOC_DAPM_INPUT("INPUT2B"),
	SND_SOC_DAPM_INPUT("INPUT3B"),
	SND_SOC_DAPM_INPUT("INPUT4B"),
	SND_SOC_DAPM_INPUT("MICA"),
	SND_SOC_DAPM_INPUT("MICB"),
};

static const struct snd_soc_dapm_route soc_cs42l52_audio_map[] = {

	/* adc select path */
	{"ADC Mux Left Capture Switch", "AIN1", "INPUT1A"},
	{"ADC Mux Right Capture Switch", "AIN1", "INPUT1B"},
	{"ADC Mux Left Capture Switch", "AIN2", "INPUT2A"},
        {"ADC Mux Right Capture Switch", "AIN2", "INPUT2B"},
	{"ADC Mux Left Capture Switch", "AIN3", "INPUT3A"},
        {"ADC Mux Right Capture Switch", "AIN3", "INPUT3B"},
	{"ADC Mux Left Capture Switch", "AIN4", "INPUT4A"},
        {"ADC Mux Right Capture Switch", "AIN4", "INPUT4B"},

	/* left capture part */
        {"AIN1A Switch", NULL, "INPUT1A"},
        {"AIN2A Switch", NULL, "INPUT2A"},
        {"AIN3A Switch", NULL, "INPUT3A"},
        {"AIN4A Switch", NULL, "INPUT4A"},
        {"MICA Switch",  NULL, "MICA"},
        {"PGA MICA", NULL, "MICA Switch"},

        {"PGA Left", NULL, "AIN1A Switch"},
        {"PGA Left", NULL, "AIN2A Switch"},
        {"PGA Left", NULL, "AIN3A Switch"},
        {"PGA Left", NULL, "AIN4A Switch"},
        {"PGA Left", NULL, "PGA MICA"},

	/* right capture part */
        {"AIN1B Switch", NULL, "INPUT1B"},
        {"AIN2B Switch", NULL, "INPUT2B"},
        {"AIN3B Switch", NULL, "INPUT3B"},
        {"AIN4B Switch", NULL, "INPUT4B"},
        {"MICB Switch",  NULL, "MICB"},
        {"PGA MICB", NULL, "MICB Switch"},

        {"PGA Right", NULL, "AIN1B Switch"},
        {"PGA Right", NULL, "AIN2B Switch"},
        {"PGA Right", NULL, "AIN3B Switch"},
        {"PGA Right", NULL, "AIN4B Switch"},
        {"PGA Right", NULL, "PGA MICB"},

	{"ADC Mux Left Capture Switch", "PGA", "PGA Left"},
        {"ADC Mux Right Capture Switch", "PGA", "PGA Right"},
	{"ADC Left", NULL, "ADC Mux Left Capture Switch"},
	{"ADC Right", NULL, "ADC Mux Right Capture Switch"},

	/* Mic Bias */
        {"Mic Bias Capture Switch", "On", "PGA MICA"},
        {"Mic Bias Capture Switch", "On", "PGA MICB"},
	{"Mic-Bias", NULL, "Mic Bias Capture Switch"},
	{"Mic-Bias", NULL, "Mic Bias Capture Switch"},
        {"ADC Mux Left Capture Switch",  "PGA", "Mic-Bias"},
        {"ADC Mux Right Capture Switch", "PGA", "Mic-Bias"},
        {"Passthrough Left Playback Switch",  "On", "Mic-Bias"},

        {"Passthrough Right Playback Switch", "On", "Mic-Bias"},

	/* loopback path */
	{"Passthrough Left Playback Switch",  "On",  "PGA Left"},
	{"Passthrough Right Playback Switch", "On",  "PGA Right"},
	{"Passthrough Left Playback Switch",  "Off", "DAC Left"},
	{"Passthrough Right Playback Switch", "Off", "DAC Right"},

/* Output map */
	/* Headphone */
	{"HP Amp Left",  NULL, "Passthrough Left Playback Switch"},
	{"HP Amp Right", NULL, "Passthrough Right Playback Switch"},
	{"HPA", NULL, "HP Amp Left"},
	{"HPB", NULL, "HP Amp Right"},

	/* Speakers */
	
	{"SPK Pwr Left",  NULL, "DAC Left"},
	{"SPK Pwr Right", NULL, "DAC Right"},
	{"SPKA", NULL, "SPK Pwr Left"},
	{"SPKB", NULL, "SPK Pwr Right"},

	/* terminator */
     //   {NULL, NULL, NULL},
};

struct soc_cs42l52_clk_para {
	u32 mclk;
	u32 rate;
	u8 speed;
	u8 group;
	u8 videoclk;
	u8 ratio;
	u8 mclkdiv2;
};


static const struct soc_cs42l52_clk_para clk_map_table[] = {
	/*8k*/
	{12288000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{18432000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{12000000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},
	{24000000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},
	{27000000, 8000, CLK_CTL_S_QS_MODE, CLK_CTL_32K_SR, CLK_CTL_27M_MCLK, CLK_CTL_RATIO_125, 0}, /*4*/

	/*11.025k*/
	{11289600, 11025, CLK_CTL_S_QS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{16934400, 11025, CLK_CTL_S_QS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	
	/*16k*/
	{12288000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{18432000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{12000000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},/*9*/
	{24000000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},
	{27000000, 16000, CLK_CTL_S_HS_MODE, CLK_CTL_32K_SR, CLK_CTL_27M_MCLK, CLK_CTL_RATIO_125, 1},

	/*22.05k*/
	{11289600, 22050, CLK_CTL_S_HS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{16934400, 22050, CLK_CTL_S_HS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	
	/* 32k */
	{12288000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},/*14*/
	{18432000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{12000000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},
	{24000000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},
	{27000000, 32000, CLK_CTL_S_SS_MODE, CLK_CTL_32K_SR, CLK_CTL_27M_MCLK, CLK_CTL_RATIO_125, 0},

	/* 44.1k */
	{11289600, 44100, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},/*19*/
	{16934400, 44100, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},

	/* 48k */
	{12288000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{18432000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{12000000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},
	{24000000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},/*24*/
	{27000000, 48000, CLK_CTL_S_SS_MODE, CLK_CTL_NOT_32K, CLK_CTL_27M_MCLK, CLK_CTL_RATIO_125, 1},

	/* 88.2k */
	{11289600, 88200, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},
	{16934400, 88200, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},

	/* 96k */
	{12288000, 96000, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},

	{18432000, 96000, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_128, 0},/*29*/
	{12000000, 96000, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 0},
	{24000000, 96000, CLK_CTL_S_DS_MODE, CLK_CTL_NOT_32K, CLK_CTL_NOT_27M, CLK_CTL_RATIO_125, 1},
};


static int soc_cs42l52_get_clk(int mclk, int rate)
{
	printk("%s\n",__FUNCTION__);
	int i , ret = 0;
	u_int mclk1, mclk2 = 0;

	for(i = 0; i < ARRAY_SIZE(clk_map_table); i++)
	{
		if(clk_map_table[i].rate == rate)
		{
			mclk1 = clk_map_table[i].mclk;
			{
				if(abs(mclk - mclk1) < abs(mclk - mclk2))
				{
					mclk2 = mclk1;
					ret = i;
				}
			}
		}
	}

	return ret < ARRAY_SIZE(clk_map_table) ? ret : -EINVAL;
}




/* #define CONFIG_MANUAL_CLK */

/* page 37 from cs42l52 datasheet */
static void soc_cs42l52_required_setup(struct snd_soc_codec *codec)
{
	printk("%s\n",__FUNCTION__);
	u8 data;
	snd_soc_write(codec, 0x00, 0x99);
	snd_soc_write(codec, 0x3e, 0xba);
	snd_soc_write(codec, 0x47, 0x80);
	data = snd_soc_read(codec, 0x32);
	snd_soc_write(codec, 0x32, data | 0x80);
	snd_soc_write(codec, 0x32, data & 0x7f);
	snd_soc_write(codec, 0x00, 0x00);
}
static int soc_cs42l52_set_sysclk(struct snd_soc_dai *codec_dai,
			int clk_id, u_int freq, int dir)
{
	printk("%s\n",__FUNCTION__);
	int ret = 0;
	struct snd_soc_codec *codec = codec_dai->codec;
	//struct cs42l52_priv *cs42l52 = snd_soc_codec_get_drvdata(codec);
    struct cs42l52_priv *cs42l52 =codec->private_data;
	if((freq >= SOC_CS42L52_MIN_CLK) && (freq <= SOC_CS42L52_MAX_CLK))
	{
		cs42l52->sysclk = freq;
		SOCDBG("sysclk %d\n", cs42l52->sysclk);
	}
	else{
		SOCDBG("invalid paramter\n");
		ret = -EINVAL;
	}
	return ret;
}
unsigned int tca_cs42L52_initcodec(struct snd_soc_codec *codec)
{
//power on
//reset gpio
	printk("%s\n",__FUNCTION__);
  	snd_soc_write(codec, CODEC_CS42L52_PWCTL1,0x1F);   
 	snd_soc_write(codec, CODEC_CS42L52_PWCTL2, 0x07);
 	snd_soc_write(codec, CODEC_CS42L52_PWCTL3, 0x50); 
	//snd_soc_write(codec, CODEC_CS42L52_CLK_CTL, 0x80);
    snd_soc_write(codec, CODEC_CS42L52_CLK_CTL, 0xa0);
	//snd_soc_write(codec, CODEC_CS42L52_IFACE_CTL1, 0x04);
    snd_soc_write(codec, CODEC_CS42L52_IFACE_CTL1, 0x24);
	snd_soc_write(codec, CODEC_CS42L52_IFACE_CTL2, 0x00);			
	snd_soc_write(codec, CODEC_CS42L52_PB_CTL1, 0x60); 
	//snd_soc_write(codec, CODEC_CS42L52_PB_CTL1, 0x00);
    /*
	snd_soc_write(codec, 0x00, 0x99);	
	snd_soc_write(codec, 0x3E, 0xBA);	
	snd_soc_write(codec, 0x00, 0x00);
	*/
	return 0;
}

static int soc_cs42l52_set_fmt(struct snd_soc_dai *codec_dai,
			u_int fmt)
{
	printk("%s\n",__FUNCTION__);
	struct snd_soc_codec *soc_codec = codec_dai->codec;
	int ret = 0;
	u8 iface = 0;

	/* set master/slave audio interface */
        switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {

	case SND_SOC_DAIFMT_CBM_CFM:
		SOCDBG("codec dai fmt master\n");
		iface = IFACE_CTL1_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		SOCDBG("codec dai fmt slave\n");
		break;
	default:
		SOCDBG("invaild formate\n");
		ret = -EINVAL;
		goto done;
	}

	 /* interface format */
        switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {

	case SND_SOC_DAIFMT_I2S:
		SOCDBG("codec dai fmt i2s\n");
		iface |= (IFACE_CTL1_ADC_FMT_I2S | IFACE_CTL1_DAC_FMT_I2S);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		SOCDBG("codec dai fmt right justified\n");
		iface |= IFACE_CTL1_DAC_FMT_RIGHT_J;
		SOCINF("warning only playback stream support this format\n");
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		SOCDBG("codec dai fmt left justified\n");
		iface |= (IFACE_CTL1_ADC_FMT_LEFT_J | IFACE_CTL1_DAC_FMT_LEFT_J);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= IFACE_CTL1_DSP_MODE_EN;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		SOCINF("unsupported format\n");
		ret = -EINVAL;
		goto done;
	default:
		SOCINF("invaild format\n");
		ret = -EINVAL;
		goto done;
	}

	/* clock inversion */
        switch (fmt & SND_SOC_DAIFMT_INV_MASK) { /*tonyliu*/


	case SND_SOC_DAIFMT_NB_NF:
		SOCDBG("codec dai fmt normal sclk\n");
		break;
	case SND_SOC_DAIFMT_IB_IF:
		SOCDBG("codec dai fmt inversed sclk\n");
		iface |= IFACE_CTL1_INV_SCLK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= IFACE_CTL1_INV_SCLK;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		SOCDBG("unsupported format\n");
		ret = -EINVAL;
	}

done:
	return ret;

}

static int soc_cs42l52_pcm_hw_params(struct snd_pcm_substream *substream,
                        struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	int ret = 0;
	u32 clk = 0;
	printk("%s\n",__FUNCTION__);
	struct snd_soc_codec *codec = dai->codec;
	//struct cs42l52_priv *cs42l52 = snd_soc_codec_get_drvdata(codec);
    struct cs42l52_priv *cs42l52 = codec->private_data;
	int index = soc_cs42l52_get_clk(cs42l52->sysclk, params_rate(params));
	if(index >= 0)
	{
		cs42l52->sysclk = clk_map_table[index].mclk;
		clk |= (clk_map_table[index].speed << CLK_CTL_SPEED_SHIFT) | 

		      (clk_map_table[index].group << CLK_CTL_32K_SR_SHIFT) | 
		      (clk_map_table[index].videoclk << CLK_CTL_27M_MCLK_SHIFT) | 
		      (clk_map_table[index].ratio << CLK_CTL_RATIO_SHIFT) | 
		      clk_map_table[index].mclkdiv2;
#ifdef CONFIG_MANUAL_CLK
		snd_soc_write(codec, CODEC_CS42L52_CLK_CTL, clk);
#else
		snd_soc_write(codec, CODEC_CS42L52_CLK_CTL, 0xa0);
#endif

	}
	else{
		SOCDBG("can't find out right mclk\n");
		ret = -EINVAL;
	}
	return ret;
}

static int soc_cs42l52_digital_mute(struct snd_soc_dai *dai, int mute)
{
	printk("%s\n",__FUNCTION__);
#if defined CS42L52_SPEAK_IN_ONE
	struct snd_soc_codec *soc_codec = dai->codec;
	u8 mute_val = snd_soc_read(soc_codec, CODEC_CS42L52_PB_CTL1) & PB_CTL1_MUTE_MASK;

	if(mute)
	{
		snd_soc_write(soc_codec, CODEC_CS42L52_PB_CTL1, mute_val | PB_CTL1_MSTB_MUTE | PB_CTL1_MSTA_MUTE);
	}
	else{
		snd_soc_write(soc_codec, CODEC_CS42L52_PB_CTL1, mute_val );
	}
#endif
	return 0;
}



int soc_cs42l52_set_bias_level(struct snd_soc_codec *codec, enum snd_soc_bias_level level)
{
		printk("%s\n",__FUNCTION__);
	u8 pwctl1 = snd_soc_read(codec, CODEC_CS42L52_PWCTL1) & 0x9f;
	u8 pwctl2 = snd_soc_read(codec, CODEC_CS42L52_PWCTL2) & 0x07;

	switch (level) {
        case SND_SOC_BIAS_ON: /* full On */
		SOCDBG("full on\n");
		break;
        case SND_SOC_BIAS_PREPARE: /* partial On */
		SOCDBG("partial on\n");
		pwctl1 &= ~(PWCTL1_PDN_CHRG | PWCTL1_PDN_CODEC);
                snd_soc_write(codec, CODEC_CS42L52_PWCTL1, pwctl1);
                break;
        case SND_SOC_BIAS_STANDBY: /* Off, with power */
		SOCDBG("off with power\n");
		pwctl1 &= ~(PWCTL1_PDN_CHRG | PWCTL1_PDN_CODEC);
                snd_soc_write(codec, CODEC_CS42L52_PWCTL1, pwctl1);
                break;
        case SND_SOC_BIAS_OFF: /* Off, without power */
		SOCDBG("off without power\n");
                snd_soc_write(codec, CODEC_CS42L52_PWCTL1, pwctl1 | 0x9f);
		snd_soc_write(codec, CODEC_CS42L52_PWCTL2, pwctl2 | 0x07);
                break;
        }
        codec->bias_level = level;
        return 0;
}

#define SOC_CS42L52_RATES ( SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_11025 | \
                            SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | \
                            SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
                            SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
                            SNDRV_PCM_RATE_96000 ) /*refer to cs42l52 datasheet page35*/

#define SOC_CS42L52_FORMATS ( SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | \
                              SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_U18_3LE | \
                              SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_U20_3LE | \
                              SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE )

static struct snd_soc_dai_ops cs42l52_dai_ops = {
                        .hw_params = soc_cs42l52_pcm_hw_params,
                        .set_sysclk = soc_cs42l52_set_sysclk,
                        .set_fmt = soc_cs42l52_set_fmt,
                        .digital_mute = soc_cs42l52_digital_mute,
                };
				
struct  snd_soc_dai soc_cs42l52_dai = {
                .name = SOC_CS42L52_NAME,
                .playback = {
                        .stream_name = "Playback",
                        .channels_min = 1,
                        .channels_max = SOC_CS42L52_DEFAULT_MAX_CHANS,
                        .rates = SOC_CS42L52_RATES,
                        .formats = SOC_CS42L52_FORMATS,
                },
                .capture = {
                        .stream_name = "Capture",
                        .channels_min = 1,
                        .channels_max = SOC_CS42L52_DEFAULT_MAX_CHANS,
                        .rates = SOC_CS42L52_RATES,
                        .formats = SOC_CS42L52_FORMATS,
                },
                .ops = &cs42l52_dai_ops,
		.symmetric_rates = 1,
};

EXPORT_SYMBOL_GPL(soc_cs42l52_dai);

static int soc_cs42l52_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("%s\n",__FUNCTION__);
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *soc_codec = socdev->card->codec;
	
	snd_soc_write(soc_codec, CODEC_CS42L52_PWCTL1, PWCTL1_PDN_CODEC);
	soc_cs42l52_set_bias_level(soc_codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int soc_cs42l52_resume(struct platform_device *pdev)
{
	printk("%s\n",__FUNCTION__);
	struct snd_soc_device *socdev = (struct snd_soc_device*) platform_get_drvdata(pdev);
	struct snd_soc_codec *soc_codec = socdev->card->codec;
	int i, reg;
	u8 data[2];
	u8 *reg_cache = (u8*) soc_codec->reg_cache;
	soc_codec->num_dai = 1;
	/* Sync reg_cache with the hardware */
	for(i = 0; i < soc_codec->num_dai; i++) {

	    for(reg = 0; reg < ARRAY_SIZE(soc_cs42l52_reg_default); reg++) {
		data[0] = reg;
		data[1] = reg_cache[reg];
		if(cs42l52_i2c_write(soc_codec->control_data, data, 2) != 2)
		    break;
	    }
	}

	soc_cs42l52_set_bias_level(soc_codec, SND_SOC_BIAS_STANDBY);

	/*charge cs42l52 codec*/
	if(soc_codec->suspend_bias_level == SND_SOC_BIAS_ON)
	{
		soc_cs42l52_set_bias_level(soc_codec, SND_SOC_BIAS_PREPARE);
		soc_codec->bias_level = SND_SOC_BIAS_ON;
		schedule_delayed_work(&soc_codec->delayed_work, msecs_to_jiffies(1000));
	}

	return 0;

}

static struct snd_soc_codec *cs42l52_codec;

static int soc_cs42l52_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	if (cs42l52_codec == NULL) {
		dev_err(&pdev->dev, "Codec device not registered\n");
		return -ENODEV;
	}

	socdev->card->codec = cs42l52_codec;
	codec = cs42l52_codec;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(codec->dev, "failed to create pcms: %d\n", ret);
		goto pcm_err;
	}

	snd_soc_add_controls(codec, soc_cs42l52_controls,
				ARRAY_SIZE(soc_cs42l52_controls));
	snd_soc_dapm_new_controls(codec, soc_cs42l52_dapm_widgets,
				  ARRAY_SIZE(soc_cs42l52_dapm_widgets));
	snd_soc_dapm_add_routes(codec, soc_cs42l52_audio_map, ARRAY_SIZE(soc_cs42l52_audio_map));

	return ret;

pcm_err:
	return ret;
}

static int soc_cs42l52_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
	return 0;
}

struct snd_soc_codec_device soc_codec_cs42l52_dev = {
	.probe = soc_cs42l52_probe,
	.remove = soc_cs42l52_remove,
	.suspend = soc_cs42l52_suspend,
	.resume = soc_cs42l52_resume,
};

EXPORT_SYMBOL_GPL(soc_codec_cs42l52_dev);

static int cs42l52_register(struct cs42l52_priv *cs42l52,
			   enum snd_soc_control_type control)
{
	struct snd_soc_codec *codec = &cs42l52->codec;
	int ret,i;
	u16 reg;

	printk("%s\n",__FUNCTION__);
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);	
	//snd_soc_codec_set_drvdata(codec, cs42l52);
	codec->private_data = cs42l52;
	codec->name = "cs42l52";
	codec->owner = THIS_MODULE;
	codec->dai = &soc_cs42l52_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = sizeof(soc_cs42l52_reg_default);
	codec->reg_cache = kmemdup(soc_cs42l52_reg_default, sizeof(soc_cs42l52_reg_default), GFP_KERNEL);
	codec->bias_level = SND_SOC_BIAS_OFF;
	codec->set_bias_level = soc_cs42l52_set_bias_level;
	codec->pcm_devs = 0;  /*pcm device num index*/
	codec->pop_time = 2;	
	//ret = snd_soc_codec_set_cache_io(codec, 7, 9, control);
    ret = snd_soc_codec_set_cache_io(codec, 8, 8, control);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		goto err;
	}
	
	/*initialize codec*/
    tca_cs42L52_initcodec(codec);
	for(i = 0; i < codec->num_dai; i++)
	{
		SOCINF("Cirrus CS42L52 codec , revision %d\n", snd_soc_read(codec, CODEC_CS42L52_CHIP) & CHIP_REV_MASK);
		/*set hp default volume*/
		snd_soc_write(codec, CODEC_CS42L52_HPA_VOL, DEFAULT_HP_VOL);
        dev_info(codec->dev, "CODEC_CS42L52_HPA_VOL(%02x): %02x\n", CODEC_CS42L52_HPA_VOL, snd_soc_read(codec, CODEC_CS42L52_HPA_VOL));
		snd_soc_write(codec, CODEC_CS42L52_HPB_VOL, DEFAULT_HP_VOL);
        dev_info(codec->dev, "CODEC_CS42L52_HPB_VOL(%02x): %02x\n", CODEC_CS42L52_HPB_VOL, snd_soc_read(codec, CODEC_CS42L52_HPB_VOL));
	
		/*set spk default volume*/
		snd_soc_write(codec, CODEC_CS42L52_SPKA_VOL, DEFAULT_SPK_VOL);
        dev_info(codec->dev, "CODEC_CS42L52_SPKA_VOL(%02x): %02x\n", CODEC_CS42L52_SPKA_VOL, snd_soc_read(codec, CODEC_CS42L52_SPKA_VOL));
		snd_soc_write(codec, CODEC_CS42L52_SPKB_VOL, DEFAULT_SPK_VOL);
        dev_info(codec->dev, "CODEC_CS42L52_SPKB_VOL(%02x): %02x\n", CODEC_CS42L52_SPKB_VOL, snd_soc_read(codec, CODEC_CS42L52_SPKB_VOL));

		/*set output default powerstate*/
		//snd_soc_write(codec, CODEC_CS42L52_PWCTL3, DEFAULT_OUTPUT_STATE);
		snd_soc_write(codec, CODEC_CS42L52_PWCTL3, 0x50);
        dev_info(codec->dev, "CODEC_CS42L52_PWCTL3(%02x): %02x\n", CODEC_CS42L52_PWCTL3, snd_soc_read(codec, CODEC_CS42L52_PWCTL3));
#ifdef CONFIG_MANUAL_CLK
		snd_soc_write(codec, CODEC_CS42L52_CLK_CTL, 
				(snd_soc_read(codec, CODEC_CS42L52_CLK_CTL) 
				 & ~CLK_CTL_AUTODECT_ENABLE));        
#else
		snd_soc_write(codec, CODEC_CS42L52_CLK_CTL,
				(snd_soc_read(codec, CODEC_CS42L52_CLK_CTL)
				 |CLK_CTL_AUTODECT_ENABLE));
#endif
        dev_info(codec->dev, "CODEC_CS42L52_CLK_CTL(%02x): %02x\n", CODEC_CS42L52_CLK_CTL, snd_soc_read(codec, CODEC_CS42L52_CLK_CTL));
	
		/*default output stream configure*/
		snd_soc_write(codec, CODEC_CS42L52_PB_CTL1,0x60);
        dev_info(codec->dev, "CODEC_CS42L52_PB_CTL1(%02x): %02x\n", CODEC_CS42L52_PB_CTL1, snd_soc_read(codec, CODEC_CS42L52_PB_CTL1));
		snd_soc_write(codec, CODEC_CS42L52_MISC_CTL,
				(snd_soc_read(codec, CODEC_CS42L52_MISC_CTL))
				| (MISC_CTL_DEEMPH | MISC_CTL_DIGZC | MISC_CTL_DIGSFT));
        dev_info(codec->dev, "CODEC_CS42L52_MISC_CTL(%02x): %02x\n", CODEC_CS42L52_MISC_CTL, snd_soc_read(codec, CODEC_CS42L52_MISC_CTL));
	
		/*default input stream configure*/
        snd_soc_write(codec, CODEC_CS42L52_ADC_PGA_A, 0x81);
        dev_info(codec->dev, "CODEC_CS42L52_ADC_PGA_A(%02x): %02x\n", CODEC_CS42L52_ADC_PGA_A, snd_soc_read(codec, CODEC_CS42L52_ADC_PGA_A));
        snd_soc_write(codec, CODEC_CS42L52_ADC_PGA_B, 0x81);
        dev_info(codec->dev, "CODEC_CS42L52_ADC_PGA_B(%02x): %02x\n", CODEC_CS42L52_ADC_PGA_B, snd_soc_read(codec, CODEC_CS42L52_ADC_PGA_B));

		snd_soc_write(codec, CODEC_CS42L52_MICA_CTL,0x00);/*pre-amplifer 16db*/
        dev_info(codec->dev, "CODEC_CS42L52_MICA_CTL(%02x): %02x\n", CODEC_CS42L52_MICA_CTL, snd_soc_read(codec, CODEC_CS42L52_MICA_CTL));
		snd_soc_write(codec, CODEC_CS42L52_MICB_CTL,0x00);
        dev_info(codec->dev, "CODEC_CS42L52_MICB_CTL(%02x): %02x\n", CODEC_CS42L52_MICB_CTL, snd_soc_read(codec, CODEC_CS42L52_MICB_CTL));
	
		/*default input stream path configure*/
		snd_soc_write(codec, CODEC_CS42L52_ANALOG_HPF_CTL, 
				 (snd_soc_read(codec, CODEC_CS42L52_ANALOG_HPF_CTL)
				 | HPF_CTL_ANLGSFTB | HPF_CTL_ANLGSFTA));
        dev_info(codec->dev, "CODEC_CS42L52_ANALOG_HPF_CTL(%02x): %02x\n", CODEC_CS42L52_ANALOG_HPF_CTL, snd_soc_read(codec, CODEC_CS42L52_ANALOG_HPF_CTL));
		snd_soc_write(codec, CODEC_CS42L52_PGAA_CTL, PGAX_CTL_VOL_6DB);
        dev_info(codec->dev, "CODEC_CS42L52_PGAA_CTL(%02x): %02x\n", CODEC_CS42L52_PGAA_CTL, snd_soc_read(codec, CODEC_CS42L52_PGAA_CTL));
		snd_soc_write(codec, CODEC_CS42L52_PGAB_CTL, PGAX_CTL_VOL_6DB); /*PGA volume*/
        dev_info(codec->dev, "CODEC_CS42L52_PGAB_CTL(%02x): %02x\n", CODEC_CS42L52_PGAB_CTL, snd_soc_read(codec, CODEC_CS42L52_PGAB_CTL));

		snd_soc_write(codec, CODEC_CS42L52_ADCA_VOL, ADCX_VOL_12DB);
        dev_info(codec->dev, "CODEC_CS42L52_ADCA_VOL(%02x): %02x\n", CODEC_CS42L52_ADCA_VOL, snd_soc_read(codec, CODEC_CS42L52_ADCA_VOL));
		snd_soc_write(codec, CODEC_CS42L52_ADCB_VOL, ADCX_VOL_12DB); /*ADC volume*/
        dev_info(codec->dev, "CODEC_CS42L52_ADCB_VOL(%02x): %02x\n", CODEC_CS42L52_ADCB_VOL, snd_soc_read(codec, CODEC_CS42L52_ADCB_VOL));

		snd_soc_write(codec, CODEC_CS42L52_ALC_CTL,
				 (ALC_CTL_ALCB_ENABLE | ALC_CTL_ALCA_ENABLE)); /*enable ALC*/
        dev_info(codec->dev, "CODEC_CS42L52_ALC_CTL(%02x): %02x\n", CODEC_CS42L52_ALC_CTL, snd_soc_read(codec, CODEC_CS42L52_ALC_CTL));

		snd_soc_write(codec, CODEC_CS42L52_ALC_THRESHOLD,
				 ((ALC_RATE_0DB << ALC_MAX_RATE_SHIFT)
				  | (ALC_RATE_3DB << ALC_MIN_RATE_SHIFT)));/*ALC max and min threshold*/
        dev_info(codec->dev, "CODEC_CS42L52_ALC_THRESHOLD(%02x): %02x\n", CODEC_CS42L52_ALC_THRESHOLD, snd_soc_read(codec, CODEC_CS42L52_ALC_THRESHOLD));

		snd_soc_write(codec, CODEC_CS42L52_NOISE_GATE_CTL,
				 (NG_ENABLE | (NG_MIN_70DB << NG_THRESHOLD_SHIFT)
				  | (NG_DELAY_100MS << NG_DELAY_SHIFT))); /*Noise Gate enable*/
        dev_info(codec->dev, "CODEC_CS42L52_NOISE_GATE_CTL(%02x): %02x\n", CODEC_CS42L52_NOISE_GATE_CTL, snd_soc_read(codec, CODEC_CS42L52_NOISE_GATE_CTL));

		snd_soc_write(codec, CODEC_CS42L52_BEEP_VOL, BEEP_VOL_12DB);
        dev_info(codec->dev, "CODEC_CS42L52_BEEP_VOL(%02x): %02x\n", CODEC_CS42L52_BEEP_VOL, snd_soc_read(codec, CODEC_CS42L52_BEEP_VOL));

		snd_soc_write(codec, CODEC_CS42L52_ADCA_MIXER_VOL, 0x80 | ADC_MIXER_VOL_12DB);
        dev_info(codec->dev, "CODEC_CS42L52_ADCA_MIXER_VOL(%02x): %02x\n", CODEC_CS42L52_ADCA_MIXER_VOL, snd_soc_read(codec, CODEC_CS42L52_ADCA_MIXER_VOL));
		snd_soc_write(codec, CODEC_CS42L52_ADCB_MIXER_VOL, 0x80 | ADC_MIXER_VOL_12DB);
        dev_info(codec->dev, "CODEC_CS42L52_ADCB_MIXER_VOL(%02x): %02x\n", CODEC_CS42L52_ADCB_MIXER_VOL, snd_soc_read(codec, CODEC_CS42L52_ADCB_MIXER_VOL));

		snd_soc_write(codec, CODEC_CS42L52_MASTERA_VOL, 0);
		snd_soc_write(codec, CODEC_CS42L52_MASTERB_VOL, 0);
		snd_soc_write(codec, CODEC_CS42L52_HPA_VOL, 0);
		snd_soc_write(codec, CODEC_CS42L52_HPB_VOL, 0);
		snd_soc_write(codec, CODEC_CS42L52_SPKA_VOL, 0);
		snd_soc_write(codec, CODEC_CS42L52_SPKB_VOL, 0);
		snd_soc_write(codec, CODEC_CS42L52_PB_CTL1, 0x68);
		snd_soc_write(codec, CODEC_CS42L52_MISC_CTL, 0x02);
		snd_soc_write(codec, CODEC_CS42L52_ADCA_MIXER_VOL, 0x80);
		snd_soc_write(codec, CODEC_CS42L52_ADCB_MIXER_VOL, 0x80);
		
		//for record
		snd_soc_write(codec, CODEC_CS42L52_PWCTL2, 0x00); //0x03
		snd_soc_write(codec, CODEC_CS42L52_ADC_PGA_A, 0x90); //0x08
		snd_soc_write(codec, CODEC_CS42L52_ADC_PGA_B, 0x90); //0x09
		snd_soc_write(codec, CODEC_CS42L52_ANALOG_HPF_CTL, 0xAF); //0x0a
		snd_soc_write(codec, CODEC_CS42L52_MICA_CTL, 0x4F); //0x10
		snd_soc_write(codec, CODEC_CS42L52_MICB_CTL, 0x4F); //0x11
	}

/* ---------------------------------------------------------------------------
   - E: Initialize WM8988 register for M57TE
   ---------------------------------------------------------------------------*/
	
	soc_cs42l52_set_bias_level(&cs42l52->codec, SND_SOC_BIAS_STANDBY);

	soc_cs42l52_dai.dev = codec->dev;
	cs42l52_codec = codec;
	
	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}
	
	ret = snd_soc_register_dai(&soc_cs42l52_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		goto err_codec;
	}

	return 0;

err_codec:
	snd_soc_unregister_codec(codec);
err:
	kfree(cs42l52);
	return ret;
}

static void cs42l52_unregister(struct cs42l52_priv *cs42l52)
{
	printk("%s	\n",__FUNCTION__);
	soc_cs42l52_set_bias_level(&cs42l52->codec, SND_SOC_BIAS_OFF);
	snd_soc_unregister_dai(&soc_cs42l52_dai);
	snd_soc_unregister_codec(&cs42l52->codec);
	kfree(cs42l52);
}

static int cs42l52_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	struct cs42l52_priv *cs42l52;
	struct snd_soc_codec *codec;
	struct cs42l52_platform_data *pdata = client->dev.platform_data;

	if(pdata&&pdata->cs42l52_pwr_rst){
		printk("***cs42l52 reset***\n");
		pdata->cs42l52_pwr_rst();
	}
	else{
		printk("***cs42l52 no reset***\n");
		}
	printk("%s	\n",__FUNCTION__);
	cs42l52 = kzalloc(sizeof(struct cs42l52_priv), GFP_KERNEL);
	if (cs42l52 == NULL)
		return -ENOMEM;
	codec = &cs42l52->codec;

	i2c_set_clientdata(client, cs42l52);

	codec->control_data = client;
	codec->dev = &client->dev;
	struct device *dev = &client->dev;
	sysfs_create_group(&dev->kobj, &cs42l52_attr_group);
	dev_set_drvdata(dev, codec);
	return cs42l52_register(cs42l52, SND_SOC_I2C);

}

static int cs42l52_i2c_remove(struct i2c_client *client)
{

	struct cs42l52_priv *cs42l52;
	printk("%s\n",__FUNCTION__);
	cs42l52 = i2c_get_clientdata(client);
    cs42l52_unregister(cs42l52);
    return 0;
}

static const struct i2c_device_id cs42l52_i2c_id[] = {
	{ "cs42l52", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cs42l52_i2c_id);

static struct i2c_driver cs42l52_i2c_drv = {
	.driver = {
		.name = "cs42l52",
		.owner = THIS_MODULE,
	},
	.probe =    cs42l52_i2c_probe,
	.remove =   __devexit_p(cs42l52_i2c_remove),
	.id_table = cs42l52_i2c_id,

};

static int __init cs42l52_modinit(void)
{
	return i2c_add_driver(&cs42l52_i2c_drv);
}
module_init(cs42l52_modinit);

static void __exit cs42l52_exit(void)
{	
	i2c_del_driver(&cs42l52_i2c_drv);
}
module_exit(cs42l52_exit);

MODULE_DESCRIPTION("ALSA SoC CS42L52 Codec");
MODULE_AUTHOR("Bo Liu, Bo.Liu@cirrus.com, www.cirrus.com");
MODULE_LICENSE("GPL");
