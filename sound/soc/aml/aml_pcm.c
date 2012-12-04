#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/soundcard.h>
#include <linux/timer.h>
#include <linux/debugfs.h>
#include <linux/major.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/aml_platform.h>

#include <mach/am_regs.h>
#include <mach/pinmux.h>

#include <linux/amports/amaudio.h>

#include "aml_pcm.h"
#include "aml_audio_hw.h"

//#define _AML_PCM_DEBUG_

#define AOUT_EVENT_IEC_60958_PCM                0x1
#define AOUT_EVENT_RAWDATA_AC_3                 0x2
#define AOUT_EVENT_RAWDATA_MPEG1                0x3
#define AOUT_EVENT_RAWDATA_MP3                  0x4
#define AOUT_EVENT_RAWDATA_MPEG2                0x5
#define AOUT_EVENT_RAWDATA_AAC                  0x6
#define AOUT_EVENT_RAWDATA_DTS                  0x7
#define AOUT_EVENT_RAWDATA_ATRAC                0x8
#define AOUT_EVENT_RAWDATA_ONE_BIT_AUDIO        0x9
#define AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS   0xA
#define AOUT_EVENT_RAWDATA_DTS_HD               0xB
#define AOUT_EVENT_RAWDATA_MAT_MLP              0xC
#define AOUT_EVENT_RAWDATA_DST                  0xD
#define AOUT_EVENT_RAWDATA_WMA_PRO              0xE

extern int aout_notifier_call_chain(unsigned long val, void *v);
extern void	aml_alsa_hw_reprepare();

extern unsigned IEC958_mode_raw;
extern unsigned IEC958_mode_codec;

extern int aml_m3_is_hp_pluged(void);
extern void mute_spk(struct snd_soc_codec* codec, int flag);
unsigned int aml_pcm_playback_start_addr = 0;
unsigned int aml_pcm_capture_start_addr  = 0;
unsigned int aml_pcm_playback_end_addr = 0;
unsigned int aml_pcm_capture_end_addr = 0;

unsigned int aml_pcm_playback_phy_start_addr = 0;
unsigned int aml_pcm_capture_phy_start_addr  = 0;
unsigned int aml_pcm_playback_phy_end_addr = 0;
unsigned int aml_pcm_capture_phy_end_addr = 0;
unsigned int aml_pcm_playback_off = 0;
unsigned int aml_pcm_playback_enable = 1;

unsigned int aml_iec958_playback_start_addr = 0;
unsigned int aml_iec958_playback_start_phy = 0;
unsigned int aml_iec958_playback_size = 0;  // in bytes

static  unsigned  playback_substream_handle = 0 ;
/*to keep the pcm status for clockgating*/
static unsigned clock_gating_status = 0;
static unsigned clock_gating_playback = 1;
static unsigned clock_gating_capture = 2;
static int audio_type_info = -1;
static int audio_sr_info = -1;

EXPORT_SYMBOL(aml_pcm_playback_start_addr);
EXPORT_SYMBOL(aml_pcm_capture_start_addr);
EXPORT_SYMBOL(aml_pcm_playback_off);
EXPORT_SYMBOL(aml_pcm_playback_enable);


/*--------------------------------------------------------------------------*\
 * Hardware definition
\*--------------------------------------------------------------------------*/
/* TODO: These values were taken from the AML platform driver, check
 *	 them against real values for AML
 */
static const struct snd_pcm_hardware aml_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED|
							SNDRV_PCM_INFO_BLOCK_TRANSFER|
				  		    SNDRV_PCM_INFO_PAUSE,
				  		
	.formats		= SNDRV_PCM_FMTBIT_S16_LE|SNDRV_PCM_FMTBIT_S24_LE|SNDRV_PCM_FMTBIT_S32_LE,

	.period_bytes_min	= 64,
	.period_bytes_max	= 8*1024,
	.periods_min		= 2,
	.periods_max		= 1024,
	.buffer_bytes_max	= 64 * 1024,
	
	.rate_min = 32000,
    .rate_max = 48000,
    .channels_min = 2,
    .channels_max = 2,
    .fifo_size = 0,
};

static const struct snd_pcm_hardware aml_pcm_capture = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED|
							SNDRV_PCM_INFO_BLOCK_TRANSFER|
							SNDRV_PCM_INFO_MMAP |
				  		SNDRV_PCM_INFO_MMAP_VALID |
				  		SNDRV_PCM_INFO_PAUSE,
				  		
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min	= 64,
	.period_bytes_max	= 8*1024,
	.periods_min		= 2,
	.periods_max		= 1024,
	.buffer_bytes_max	= 64 * 1024,

	.rate_min = 8000,
    .rate_max = 48000,
    .channels_min = 2,
    .channels_max = 2,
    .fifo_size = 0,
};

static char snd_pcm_tmp[32*1024];
/*--------------------------------------------------------------------------*\
 * Data types
\*--------------------------------------------------------------------------*/
struct aml_runtime_data {
	struct aml_pcm_dma_params *params;
	dma_addr_t dma_buffer;		/* physical address of dma buffer */
	dma_addr_t dma_buffer_end;	/* first address beyond DMA buffer */

	struct snd_pcm *pcm;
	audio_stream_t s;	
	struct timer_list timer;	// timeer for playback and capture
};

/*--------------------------------------------------------------------------*\
 * audio clock gating
\*--------------------------------------------------------------------------*/
#if defined(CONFIG_SND_AML_M3)
static void aml_audio_clock_gating_disable(void)
{
	struct snd_soc_codec* codec;
	//printk("***Entered %s:%s\n", __FILE__,__func__);
	//WRITE_CBUS_REG(HHI_GCLK_MPEG0, READ_CBUS_REG(HHI_GCLK_MPEG0)&~(1<<18));
	WRITE_CBUS_REG(HHI_GCLK_MPEG1, READ_CBUS_REG(HHI_GCLK_MPEG1)&~(1<<2)
								    //&~(0xFF<<6)
								    );
	//WRITE_CBUS_REG(HHI_GCLK_MPEG2, READ_CBUS_REG(HHI_GCLK_MPEG2)&~(1<<10));
	//WRITE_CBUS_REG(HHI_GCLK_OTHER, READ_CBUS_REG(HHI_GCLK_OTHER)&~(1<<10)
								    //&~(1<<18)
								    //&~(0x7<<14));
	mute_spk(codec,1);							    
	WRITE_APB_REG(APB_ADAC_POWER_CTRL_REG2, READ_APB_REG(APB_ADAC_POWER_CTRL_REG2)&(~(1<<7)));
	adac_latch();
	
}

static void aml_audio_clock_gating_enable(void)
{
	struct snd_soc_codec* codec;
	printk("***Entered %s:%s\n", __FILE__,__func__);
	//WRITE_CBUS_REG(HHI_GCLK_MPEG0, READ_CBUS_REG(HHI_GCLK_MPEG0)|(1<<18));
	WRITE_CBUS_REG(HHI_GCLK_MPEG1, READ_CBUS_REG(HHI_GCLK_MPEG1)|(1<<2)
								    //|(0xFF<<6)
								    );
	//WRITE_CBUS_REG(HHI_GCLK_MPEG2, READ_CBUS_REG(HHI_GCLK_MPEG2)|(1<<10));
	//WRITE_CBUS_REG(HHI_GCLK_OTHER, READ_CBUS_REG(HHI_GCLK_OTHER)|(1<<10)
								    //|(1<<18)
								    //|(0x7<<14));
	WRITE_APB_REG(APB_ADAC_POWER_CTRL_REG2, READ_APB_REG(APB_ADAC_POWER_CTRL_REG2)|(1<<7));
	if(aml_m3_is_hp_pluged()){
		mute_spk(codec,1);	
	}
	else 
		mute_spk(codec,0);
	adac_latch();
}

static int aml_clock_gating(unsigned int status)
{
	if(status){
		aml_audio_clock_gating_enable();
	}
	else{
		aml_audio_clock_gating_disable();
	}
}
/*--------------------------------------------------------------------------*\
 * audio power gating
 * power up/down the audio module
\*--------------------------------------------------------------------------*/
static void aml_audio_dac_power_gating(int flag)//flag=1 : on; flag=0 : off
{
	u32 value;
	value = READ_APB_REG(APB_ADAC_POWER_CTRL_REG1);
	if(flag){
		value |= 3;
	}
	else{
		value &= ~3;
	}
	WRITE_APB_REG(APB_ADAC_POWER_CTRL_REG1, value);
}
#endif
static void aml_audio_adc_power_gating(int flag)//flag=1 : on; flag=0 : off
{
	u32 value;
	value = READ_APB_REG(APB_ADAC_POWER_CTRL_REG2);
	if(flag){
		value |= 3;
	}
	else{
		value &= ~3;
	}
	WRITE_APB_REG(APB_ADAC_POWER_CTRL_REG2, value);
}
/*--------------------------------------------------------------------------*\
 * Helper functions
\*--------------------------------------------------------------------------*/
static int aml_pcm_preallocate_dma_buffer(struct snd_pcm *pcm,
	int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	
	size_t size = 0;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		size = aml_pcm_hardware.buffer_bytes_max;
		buf->dev.type = SNDRV_DMA_TYPE_DEV;
		buf->dev.dev = pcm->card->dev;
		buf->private_data = NULL;
        /* one size for i2s output, another for 958, and 128 for alignment */
		buf->area = dma_alloc_coherent(pcm->card->dev, size+4096,
					  &buf->addr, GFP_KERNEL);
		printk("aml-pcm %d:"
		"preallocate_dma_buffer: area=%p, addr=%p, size=%d\n", stream,
		(void *) buf->area,
		(void *) buf->addr,
		size);

        aml_pcm_playback_start_addr = buf->area;
		aml_pcm_playback_end_addr = buf->area + size;

		aml_pcm_playback_phy_start_addr = buf->addr;
		aml_pcm_playback_phy_end_addr = buf->addr+size;

        /* alloc iec958 buffer */
        aml_iec958_playback_start_addr = dma_alloc_coherent(pcm->card->dev, size*4,
           &aml_iec958_playback_start_phy, GFP_KERNEL);
        if(aml_iec958_playback_start_addr == 0){
          printk("aml-pcm %d: alloc iec958 buffer failed\n");
          return -ENOMEM;
        }
        aml_iec958_playback_size = size*4;
        printk("iec958 %d: preallocate dma buffer start=%p, size=%x\n", aml_iec958_playback_start_addr, size*4);
	}else{
		size = aml_pcm_capture.buffer_bytes_max;
		buf->dev.type = SNDRV_DMA_TYPE_DEV;
		buf->dev.dev = pcm->card->dev;
		buf->private_data = NULL;
		buf->area = dma_alloc_coherent(pcm->card->dev, size*2,
					  &buf->addr, GFP_KERNEL);
		printk("aml-pcm %d:"
		"preallocate_dma_buffer: area=%p, addr=%p, size=%d\n", stream,
		(void *) buf->area,
		(void *) buf->addr,
		size);

        aml_pcm_capture_start_addr = buf->area;
		aml_pcm_capture_end_addr = buf->area+size;
		aml_pcm_capture_phy_start_addr = buf->addr;
		aml_pcm_capture_phy_end_addr = buf->addr+size;		
	}

	if (!buf->area)
		return -ENOMEM;

	buf->bytes = size;
	return 0;
}
/*--------------------------------------------------------------------------*\
 * ISR
\*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*\
 * PCM operations
\*--------------------------------------------------------------------------*/
static int aml_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
//	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	audio_stream_t *s = &prtd->s;
	
	/* this may get called several times by oss emulation
	 * with different params */

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aml_iec958_playback_size = runtime->dma_bytes*4;
	s->I2S_addr = runtime->dma_addr;

    /*
     * Both capture and playback need to reset the last ptr to the start address
     * */
    //if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
        /* s->last_ptr must initialized as dma buffer's start addr */
        s->last_ptr = runtime->dma_addr;
    //}
	
	return 0;
}

static int aml_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct aml_runtime_data *prtd = substream->runtime->private_data;
	struct aml_pcm_dma_params *params = prtd->params;
	if (params != NULL) {
					
	}

	return 0;
}
/*
the I2S hw  and IEC958 PCM output initation,958 initation here, 
for the case that only use our ALSA driver for PCM s/pdif output.
*/
static void  aml_hw_i2s_init(struct snd_pcm_runtime *runtime)
{

      
		switch(runtime->format){
		case SNDRV_PCM_FORMAT_S32_LE:
			I2S_MODE = AIU_I2S_MODE_PCM32;
		// IEC958_MODE = AIU_958_MODE_PCM32;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			I2S_MODE = AIU_I2S_MODE_PCM24;
		// IEC958_MODE = AIU_958_MODE_PCM24;
			break;
		case SNDRV_PCM_FORMAT_S16_LE:
			I2S_MODE = AIU_I2S_MODE_PCM16;
		// IEC958_MODE = AIU_958_MODE_PCM16;
			break;
		}
		audio_set_i2s_mode(I2S_MODE);
		audio_set_aiubuf(runtime->dma_addr, runtime->dma_bytes);
		memset((void*)runtime->dma_area,0,runtime->dma_bytes + 4096);
		/* update the i2s hw buffer end addr as android may update that */
		aml_pcm_playback_phy_end_addr = aml_pcm_playback_phy_start_addr+runtime->dma_bytes;
		printk("I2S hw init,i2s mode %d\n",I2S_MODE);

}
static int audio_notify_hdmi_info(int audio_type, void *v){
    struct snd_pcm_substream *substream =(struct snd_pcm_substream*)v;
	if(substream->runtime->rate != audio_sr_info || audio_type_info != audio_type){
		printk("audio info changed.notify to hdmi: type %d,sr %d\n",audio_type,substream->runtime->rate);
		aout_notifier_call_chain(audio_type,v);
	}
	audio_sr_info = substream->runtime->rate;
	audio_type_info = audio_type;
	
}
static void iec958_notify_hdmi_info()
{
	unsigned audio_type = AOUT_EVENT_IEC_60958_PCM;
	if(playback_substream_handle){
		if(IEC958_mode_codec == 2) //dd
			audio_type = AOUT_EVENT_RAWDATA_AC_3;
		else if(IEC958_mode_codec == 4)//dd+
			audio_type = AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS;
		else if(IEC958_mode_codec == 1|| IEC958_mode_codec == 3)//dts
			audio_type = AOUT_EVENT_RAWDATA_DTS;
		else 
			audio_type = AOUT_EVENT_IEC_60958_PCM;
		printk("iec958 nodify hdmi audio type %d\n",	audio_type);
		audio_notify_hdmi_info(audio_type, (struct snd_pcm_substream *)playback_substream_handle);
	}
	else{
		printk("substream for playback NULL\n");
	}
		
}
/*
special call by the audiodsp,add these code,as there are three cases for 958 s/pdif output
1)NONE-PCM  raw output ,only available when ac3/dts audio,when raw output mode is selected by user.
2)PCM  output for  all audio, when pcm mode is selected by user .
3)PCM  output for audios except ac3/dts,when raw output mode is selected by user
*/
static void aml_hw_iec958_init(void)
{
    _aiu_958_raw_setting_t set;
    _aiu_958_channel_status_t chstat;
    unsigned start,size;
	memset((void*)(&set), 0, sizeof(set));
	memset((void*)(&chstat), 0, sizeof(chstat));
	set.chan_stat = &chstat;
   	/* case 1,raw mode enabled */
	if(IEC958_mode_codec){
	  if(IEC958_mode_codec == 1){ //dts, use raw sync-word mode
	    	IEC958_MODE = AIU_958_MODE_RAW;
			printk("iec958 mode RAW\n");
	  }	
	  else{ //ac3,use the same pcm mode as i2s configuration
		IEC958_MODE = AIU_958_MODE_PCM_RAW;
		printk("iec958 mode %s\n",(I2S_MODE == AIU_I2S_MODE_PCM32)?"PCM32_RAW":((I2S_MODE == AIU_I2S_MODE_PCM24)?"PCM24_RAW":"PCM16_RAW"));				
	  }	
	}else{	/* case 2,3 */
	  if(I2S_MODE == AIU_I2S_MODE_PCM32)
	  	IEC958_MODE = AIU_958_MODE_PCM32;
	  else if(I2S_MODE == AIU_I2S_MODE_PCM24)
	  	IEC958_MODE = AIU_958_MODE_PCM24;
	  else		
	  	IEC958_MODE = AIU_958_MODE_PCM16;
  	  printk("iec958 mode %s\n",(I2S_MODE == AIU_I2S_MODE_PCM32)?"PCM32":((I2S_MODE == AIU_I2S_MODE_PCM24)?"PCM24":"PCM16"));
	}

	if(IEC958_MODE == AIU_958_MODE_PCM16 || IEC958_MODE == AIU_958_MODE_PCM24 ||
	  IEC958_MODE == AIU_958_MODE_PCM32){
	    set.chan_stat->chstat0_l = 0x0100;
		set.chan_stat->chstat0_r = 0x0100;
		set.chan_stat->chstat1_l = 0X200;
		set.chan_stat->chstat1_r = 0X200;              
        start = (aml_pcm_playback_phy_start_addr);
        size = aml_pcm_playback_phy_end_addr - aml_pcm_playback_phy_start_addr;
		audio_set_958outbuf(start, size, 0);
	  }else{
		set.chan_stat->chstat0_l = 0x1902;//NONE-PCM
		set.chan_stat->chstat0_r = 0x1902;
		set.chan_stat->chstat1_l = 0X200;
		set.chan_stat->chstat1_r = 0X200;
        // start = ((aml_pcm_playback_phy_end_addr + 4096)&(~127));
        // size  = aml_pcm_playback_phy_end_addr - aml_pcm_playback_phy_start_addr;
        start = aml_iec958_playback_start_phy;
        size = aml_iec958_playback_size;
		audio_set_958outbuf(start, size, (IEC958_MODE == AIU_958_MODE_RAW)?1:0);
		memset((void*)aml_iec958_playback_start_addr,0,size);
		
	}
	audio_set_958_mode(IEC958_MODE, &set);
	if(IEC958_mode_codec == 4)  //dd+
		WRITE_MPEG_REG_BITS(AIU_CLK_CTRL, 0, 4, 2); // 4x than i2s
	else
		WRITE_MPEG_REG_BITS(AIU_CLK_CTRL, 3, 4, 2);
	iec958_notify_hdmi_info();


}

void	aml_alsa_hw_reprepare()
{
	/* diable 958 module before call initiation */
	audio_hw_958_enable(0);
  aml_hw_iec958_init();
}

static int aml_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	audio_stream_t *s = &prtd->s;
    int iec958 = 0;
	
	if(prtd == 0)
		return 0;
	
	switch(runtime->rate){
		case 192000:
			s->sample_rate	=	AUDIO_CLK_FREQ_192;
			break;
		case 176400:
			s->sample_rate	=	AUDIO_CLK_FREQ_1764;
			break;
		case 96000:	
			s->sample_rate	=	AUDIO_CLK_FREQ_96;
			break;
		case 88200:	
			s->sample_rate	=	AUDIO_CLK_FREQ_882;
			break;
		case 48000:	
			s->sample_rate	=	AUDIO_CLK_FREQ_48;
            iec958 = 2;
			break;
		case 44100:	
			s->sample_rate	=	AUDIO_CLK_FREQ_441;
            iec958 = 0;
			break;
		case 32000:	
			s->sample_rate	=	AUDIO_CLK_FREQ_32;
            iec958 = 3;
			break;
		case 8000:
			s->sample_rate	=	AUDIO_CLK_FREQ_8;
			break;
		case 11025:
			s->sample_rate	=	AUDIO_CLK_FREQ_11;
			break;	
		case 16000:
			s->sample_rate	=	AUDIO_CLK_FREQ_16;
			break;		
		case 22050:	
			s->sample_rate	=	AUDIO_CLK_FREQ_22;
			break;	
		case 12000:
			s->sample_rate	=	AUDIO_CLK_FREQ_12;
			break;
		case 24000:
			s->sample_rate	=	AUDIO_CLK_FREQ_22;
			break;
		default:
			s->sample_rate	=	AUDIO_CLK_FREQ_441;
			break;	
	};
	audio_set_clk(s->sample_rate, AUDIO_CLK_256FS);
	audio_util_set_dac_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		aml_hw_i2s_init(runtime);		
		aml_hw_iec958_init();
	}
	else{
			//printk("aml_pcm_prepare SNDRV_PCM_STREAM_CAPTURE: dma_addr=%x, dma_bytes=%x\n", runtime->dma_addr, runtime->dma_bytes);
			audio_in_i2s_set_buf(runtime->dma_addr, runtime->dma_bytes*2);
			memset((void*)runtime->dma_area,0,runtime->dma_bytes*2);
            {
			  int * ppp = (int*)(runtime->dma_area+runtime->dma_bytes*2-8);
			  ppp[0] = 0x78787878;
			  ppp[1] = 0x78787878;
            }
	}
    if( IEC958_MODE == AIU_958_MODE_PCM_RAW){
		if(IEC958_mode_codec == 4 ){ // need Over clock for dd+
		    WRITE_MPEG_REG_BITS(AIU_CLK_CTRL, 0, 4, 2);	// 4x than i2s
		    audio_notify_hdmi_info(AOUT_EVENT_RAWDATA_DOBLY_DIGITAL_PLUS, substream);
		}else if(IEC958_mode_codec == 3 ||IEC958_mode_codec == 1 ){ // no-over clock for dts pcm mode
		    audio_notify_hdmi_info(AOUT_EVENT_RAWDATA_DTS, substream);
		}
		else  //dd
			audio_notify_hdmi_info(AOUT_EVENT_RAWDATA_AC_3, substream);
			
    }else if(IEC958_mode_codec == 1){
        audio_notify_hdmi_info(AOUT_EVENT_RAWDATA_DTS, substream);
    }else{
	    audio_notify_hdmi_info(AOUT_EVENT_IEC_60958_PCM, substream);
    }

#if 0
	printk("Audio Parameters:\n");
	printk("\tsample rate: %d\n", runtime->rate);
	printk("\tchannel: %d\n", runtime->channels);
	printk("\tsample bits: %d\n", runtime->sample_bits);
    printk("\tformat: %s\n", snd_pcm_format_name(runtime->format));
	printk("\tperiod size: %ld\n", runtime->period_size);
	printk("\tperiods: %d\n", runtime->periods);
    printk("\tiec958 mode: %d, raw=%d, codec=%d\n", IEC958_MODE, IEC958_mode_raw, IEC958_mode_codec);
#endif	
	
	return 0;
}

static int aml_pcm_trigger(struct snd_pcm_substream *substream,
	int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	struct aml_runtime_data *prtd = rtd->private_data;
	audio_stream_t *s = &prtd->s;
	int ret = 0;
	
	spin_lock(&s->lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		
		del_timer_sync(&prtd->timer);
		
		prtd->timer.expires = jiffies + 1;
    del_timer(&prtd->timer);
    add_timer(&prtd->timer);
        
		// TODO
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		//    printk("aml_pcm_trigger: playback start\n");
#if defined(CONFIG_SND_AML_M3)
			clock_gating_status |= clock_gating_playback;
			aml_clock_gating(clock_gating_status);
#endif
			audio_enable_ouput(1);
		}else{
		//	printk("aml_pcm_trigger: capture start\n");
#if defined(CONFIG_SND_AML_M3)
			clock_gating_status |= clock_gating_capture;
			aml_clock_gating(clock_gating_status);
#endif
			audio_in_i2s_enable(1);
            {
              int * ppp = (int*)(rtd->dma_area+rtd->dma_bytes*2-8);
			  ppp[0] = 0x78787878;
			  ppp[1] = 0x78787878;
            }

		}
		
		s->active = 1;
		
		break;		/* SNDRV_PCM_TRIGGER_START */
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		// TODO
		s->active = 0;
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
        //    printk("aml_pcm_trigger: playback stop\n");
				audio_enable_ouput(0);
#if defined(CONFIG_SND_AML_M3)
			clock_gating_status &= clock_gating_capture;
			aml_clock_gating(clock_gating_status);
#endif
		}else{
        //    printk("aml_pcm_trigger: capture stop\n");
				audio_in_i2s_enable(0);
#if defined(CONFIG_SND_AML_M3)
			clock_gating_status &= clock_gating_playback;
			//aml_clock_gating(clock_gating_status);
#endif
		}
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		// TODO
		s->active = 1;
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
        //    printk("aml_pcm_trigger: playback resume\n");
				audio_enable_ouput(1);
#if defined(CONFIG_SND_AML_M3)
			clock_gating_status |= clock_gating_playback;
			aml_clock_gating(clock_gating_status);
#endif
		}else{
        //    printk("aml_pcm_trigger: capture resume\n");
			  audio_in_i2s_enable(1);
#if defined(CONFIG_SND_AML_M3)
			clock_gating_status |= clock_gating_capture;
			aml_clock_gating(clock_gating_status);
#endif
              {
                int * ppp = (int*)(rtd->dma_area+rtd->dma_bytes*2-8);
			    ppp[0] = 0x78787878;
			    ppp[1] = 0x78787878;
              }
		}
		
		break;

	default:
		ret = -EINVAL;
	}
	spin_unlock(&s->lock);
	return ret;
}

static snd_pcm_uframes_t aml_pcm_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd = runtime->private_data;
	audio_stream_t *s = &prtd->s;
	
	unsigned int addr, ptr;
	
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
			ptr = read_i2s_rd_ptr();
	    addr = ptr - s->I2S_addr;
	    return bytes_to_frames(runtime, addr);
	}else{
			ptr = audio_in_i2s_wr_ptr();
			addr = ptr - s->I2S_addr;			
			return bytes_to_frames(runtime, addr)/2;
	}
	
	return 0;
}

static void aml_pcm_timer_callback(unsigned long data)
{
    struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
    struct snd_pcm_runtime *runtime = substream->runtime;
    struct aml_runtime_data *prtd = runtime->private_data;
		audio_stream_t *s = &prtd->s;

    unsigned int last_ptr, size;
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
				if(s->active == 1){
						spin_lock(&s->lock);
						last_ptr = read_i2s_rd_ptr();
						if (last_ptr < s->last_ptr) {
				        size = runtime->dma_bytes + last_ptr - (s->last_ptr);
				    } else {
				        size = last_ptr - (s->last_ptr);
				    }
    				s->last_ptr = last_ptr;
    				s->size += bytes_to_frames(substream->runtime, size);
    				if (s->size >= runtime->period_size) {
				        s->size %= runtime->period_size;
				        spin_unlock(&s->lock);
				        snd_pcm_period_elapsed(substream);
				        spin_lock(&s->lock);
				    }
				    mod_timer(&prtd->timer, jiffies + 1);
   					spin_unlock(&s->lock);
				}else{
						 mod_timer(&prtd->timer, jiffies + 1);
				}
		}else{
				if(s->active == 1){
						spin_lock(&s->lock);
						last_ptr = (audio_in_i2s_wr_ptr() - s->I2S_addr) / 2;
						if (last_ptr < s->last_ptr) {
				        size = runtime->dma_bytes + last_ptr - (s->last_ptr);
				    } else {
				        size = last_ptr - (s->last_ptr);
				    }
    				s->last_ptr = last_ptr;
    				s->size += bytes_to_frames(substream->runtime, size);
    				if (s->size >= runtime->period_size) {
				        s->size %= runtime->period_size;
				        spin_unlock(&s->lock);
				        snd_pcm_period_elapsed(substream);
				        spin_lock(&s->lock);
				    }
				    mod_timer(&prtd->timer, jiffies + 1);
   					spin_unlock(&s->lock);
				}else{
						 mod_timer(&prtd->timer, jiffies + 1);
				}
		}    
}


static int aml_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_runtime_data *prtd;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		playback_substream_handle = (unsigned long)substream;		
		snd_soc_set_runtime_hwparams(substream, &aml_pcm_hardware);
	}else{
		snd_soc_set_runtime_hwparams(substream, &aml_pcm_capture);
	}
	
	/* ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
	{
		printk("set period error\n");
		goto out;
	}

	prtd = kzalloc(sizeof(struct aml_runtime_data), GFP_KERNEL);
	if (prtd == NULL) {
		printk("alloc aml_runtime_data error\n");
		ret = -ENOMEM;
		goto out;
	}
	
	prtd->pcm = substream->pcm;
	prtd->timer.function = &aml_pcm_timer_callback;
	prtd->timer.data = (unsigned long)substream;
	init_timer(&prtd->timer);
	
	runtime->private_data = prtd;
	
	spin_lock_init(&prtd->s.lock);
 out:
	return ret;
}

static int aml_pcm_close(struct snd_pcm_substream *substream)
{
	struct aml_runtime_data *prtd = substream->runtime->private_data;
	
	del_timer_sync(&prtd->timer);
	
	kfree(prtd);
	if(substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
#if defined(CONFIG_SND_AML_M3)
	aml_audio_clock_gating_disable();
#endif
    }
	else if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		playback_substream_handle = 0;
	return 0;
}


static int aml_pcm_copy_playback(struct snd_pcm_runtime *runtime, int channel,
		    snd_pcm_uframes_t pos,
		    void __user *buf, snd_pcm_uframes_t count)
{
    int res = 0;
    int n;
    int i = 0, j = 0;
    int  align = runtime->channels * 32 / runtime->byte_align;
    char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos);
    n = frames_to_bytes(runtime, count);
    if(aml_pcm_playback_enable == 0)
      return res;
    if(access_ok(VERIFY_READ, buf, frames_to_bytes(runtime, count))){
	  if(runtime->format == SNDRV_PCM_FORMAT_S16_LE && I2S_MODE == AIU_I2S_MODE_PCM16){
        int16_t * tfrom, *to, *left, *right;
        tfrom = (int16_t*)buf;
        to = (int16_t*)hwbuf;

        left = to;
		right = to + 16;
		if (pos % align) {
		    printk("audio data unligned: pos=%d, n=%d, align=%d\n", (int)pos, n, align);
		}
		for (j = 0; j < n; j += 64) {
		    for (i = 0; i < 16; i++) {
	          *left++ = (*tfrom++) ;
	          *right++ = (*tfrom++);
		    }
		    left += 16;
		    right += 16;
		 }
      }else if(runtime->format == SNDRV_PCM_FORMAT_S24_LE && I2S_MODE == AIU_I2S_MODE_PCM24){
        int32_t *tfrom, *to, *left, *right;
        tfrom = (int32_t*)buf;
        to = (int32_t*) hwbuf;

        left = to;
        right = to + 8;

        if(pos % align){
          printk("audio data unaligned: pos=%d, n=%d, align=%d\n", (int)pos, n, align);
        }
        for(j=0; j< n; j+= 64){
          for(i=0; i<8; i++){
            *left++  =  (*tfrom ++);
            *right++  = (*tfrom ++);
          }
          left += 8;
          right += 8;
        }

      }else if(runtime->format == SNDRV_PCM_FORMAT_S32_LE && I2S_MODE == AIU_I2S_MODE_PCM32){
        int32_t *tfrom, *to, *left, *right;
        tfrom = (int32_t*)buf;
        to = (int32_t*) hwbuf;
        
        left = to;
        right = to + 8;
        
        if(pos % align){
          printk("audio data unaligned: pos=%d, n=%d, align=%d\n", (int)pos, n, align);
        }
        for(j=0; j< n; j+= 64){
          for(i=0; i<8; i++){
            *left++  =  (*tfrom ++)>>8;
            *right++  = (*tfrom ++)>>8;
          }
          left += 8;
          right += 8;
        }
      }

	}else{
	  res = -EFAULT;
	}
		
	return res;
}
		    

static int aml_pcm_copy_capture(struct snd_pcm_runtime *runtime, int channel,
		    snd_pcm_uframes_t pos,
		    void __user *buf, snd_pcm_uframes_t count)
{
		unsigned int *tfrom, *left, *right;
		unsigned short *to;
		int res = 0;
		int n;
    int i = 0, j = 0;
    unsigned int t1, t2;
    char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos)*2;
    
    to = (unsigned short *)snd_pcm_tmp;//buf;
    tfrom = (unsigned int *)hwbuf;	// 32bit buffer
    n = frames_to_bytes(runtime, count);
    if(n > 32*1024){
      printk("Too many datas to read\n");
      return -EINVAL;
    }
    
		if(access_ok(VERIFY_WRITE, buf, frames_to_bytes(runtime, count))){
				left = tfrom;
		    right = tfrom + 8;
		    if (pos % 8) {
		        printk("audio data unligned\n");
		    }
		    if((n*2)%64){
		    		printk("audio data unaligned 64 bytes\n");
		    }
		    for (j = 0; j < n*2 ; j += 64) {
		        for (i = 0; i < 8; i++) {
		        	t1 = (*left++);
		        	t2 = (*right++);
		        	//printk("%08x,%08x,", t1, t2);
	              *to++ = (unsigned short)((t1>>8)&0xffff);
	              *to++ = (unsigned short)((t1>>8)&0xffff);//copy left channel to right
	              //*to++ = (unsigned short)((t2>>8)&0xffff);
		         }
		         //printk("\n");
		        left += 8;
		        right += 8;
		    }
		}
        res = copy_to_user(buf, snd_pcm_tmp,n);
		return res;
}

static int aml_pcm_copy(struct snd_pcm_substream *substream, int channel,
		    snd_pcm_uframes_t pos,
		    void __user *buf, snd_pcm_uframes_t count)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    int ret = 0;	

 	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
 		ret = aml_pcm_copy_playback(runtime, channel,pos, buf, count);
 	}else{
 		ret = aml_pcm_copy_capture(runtime, channel,pos, buf, count);
 	}
    return ret;
} 		

int aml_pcm_silence(struct snd_pcm_substream *substream, int channel, 
		       snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
		char* ppos;
		int n;
		struct snd_pcm_runtime *runtime = substream->runtime;
		
		n = frames_to_bytes(runtime, count);
		ppos = runtime->dma_area + frames_to_bytes(runtime, pos);
		memset(ppos, 0, n);
		return 0;
}
		       		    
static struct snd_pcm_ops aml_pcm_ops = {
	.open		= aml_pcm_open,
	.close		= aml_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= aml_pcm_hw_params,
	.hw_free	= aml_pcm_hw_free,
	.prepare	= aml_pcm_prepare,
	.trigger	= aml_pcm_trigger,
	.pointer	= aml_pcm_pointer,
	.copy 		= aml_pcm_copy,
	.silence	=	aml_pcm_silence,
};


/*--------------------------------------------------------------------------*\
 * ASoC platform driver
\*--------------------------------------------------------------------------*/
static u64 aml_pcm_dmamask = 0xffffffff;

static int aml_pcm_new(struct snd_card *card,
	struct snd_soc_dai *dai, struct snd_pcm *pcm)
{
	int ret = 0;
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &aml_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (dai->playback.channels_min) {
		ret = aml_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->capture.channels_min) {
		pr_debug("aml-pcm:"
				"Allocating PCM capture DMA buffer\n");
		ret = aml_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
	
 out:
	return ret;
}

static void aml_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;
	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
		dma_free_coherent(pcm->card->dev, buf->bytes,
				  buf->area, buf->addr);
		buf->area = NULL;
	}
    aml_pcm_playback_start_addr = 0;
    aml_pcm_capture_start_addr  = 0;

    if(aml_iec958_playback_start_addr){
      dma_free_coherent(pcm->card->dev, aml_iec958_playback_size, aml_iec958_playback_start_addr, aml_iec958_playback_start_phy);
      aml_iec958_playback_start_addr = 0;
    }
}

#ifdef CONFIG_PM
static int aml_pcm_suspend(struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = dai->runtime;
	struct aml_runtime_data *prtd;
	struct aml_pcm_dma_params *params;
	if (!runtime)
		return 0;

	prtd = runtime->private_data;
	params = prtd->params;

	/* disable the PDC and save the PDC registers */
	// TODO
	printk("aml pcm suspend\n");	

	return 0;
}

static int aml_pcm_resume(struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = dai->runtime;
	struct aml_runtime_data *prtd;
	struct aml_pcm_dma_params *params;
	if (!runtime)
		return 0;

	prtd = runtime->private_data;
	params = prtd->params;

	/* restore the PDC registers and enable the PDC */
	// TODO
	printk("aml pcm resume\n");
	return 0;
}
#else
#define aml_pcm_suspend	NULL
#define aml_pcm_resume	NULL
#endif

#ifdef CONFIG_DEBUG_FS

static struct dentry *debugfs_root;
static struct dentry *debugfs_regs;
static struct dentry *debugfs_mems;

static int regs_open_file(struct inode *inode, struct file *file)
{
	return 0;
}

/**
 *	cat regs
 */
static ssize_t regs_read_file(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
		
	ret = sprintf(buf, "Usage: \n"
										 "	echo base reg val >regs\t(set the register)\n"
										 "	echo base reg >regs\t(show the register)\n"
										 "	base -> c(cbus), x(aix), p(apb), h(ahb) \n"
									);
		
	if (ret >= 0)
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);
	kfree(buf);	
	
	return ret;
}

static int read_regs(char base, int reg)
{
	int val = 0;
	switch(base){
		case 'c':
			val = READ_CBUS_REG(reg);
			break;
		case 'x':
			val = READ_AXI_REG(reg);
			break;
		case 'p':
			val = READ_APB_REG(reg);
			break;
		case 'h':
			val = READ_AHB_REG(reg);
			break;
		default:
			break;
	};
	printk("\tReg %x = %x\n", reg, val);
	return val;
}

static void write_regs(char base, int reg, int val)
{
	switch(base){
		case 'c':
			WRITE_CBUS_REG(reg, val);
			break;
		case 'x':
			WRITE_AXI_REG(reg, val);
			break;
		case 'p':
			WRITE_APB_REG(reg, val);
			break;
		case 'h':
			WRITE_AHB_REG(reg, val);
			break;
		default:
			break;
	};
	printk("Write reg:%x = %x\n", reg, val);
}
static ssize_t regs_write_file(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size = 0;
	char *start = buf;
	unsigned long reg, value;
	char base;
	
	buf_size = min(count, (sizeof(buf)-1));
	
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;
	while (*start == ' ')
		start++;
		
	base = *start;
	start ++;
	if(!(base =='c' || base == 'x' || base == 'p' || base == 'h')){
		return -EINVAL;
	}
	
	while (*start == ' ')
		start++;
		
	reg = simple_strtoul(start, &start, 16);
	
	while (*start == ' ')
		start++;
		
	if (strict_strtoul(start, 16, &value))
	{
			read_regs(base, reg);
			return -EINVAL;
	}
	
	write_regs(base, reg, value);
	
	return buf_size;
}

static const struct file_operations regs_fops = {
	.open = regs_open_file,
	.read = regs_read_file,
	.write = regs_write_file,
};

static int mems_open_file(struct inode *inode, struct file *file)
{
	return 0;
}
static ssize_t mems_read_file(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
		
	ret = sprintf(buf, "Usage: \n"
										 "	echo vmem >mems\t(read 64 bytes from vmem)\n"
										 "	echo vmem val >mems (write int value to vmem\n"
									);
		
	if (ret >= 0)
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);
	kfree(buf);	
	
	return ret;
}

static ssize_t mems_write_file(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[256];
	int buf_size = 0;
	char *start = buf;
	unsigned long mem, value;
	int i=0;
	unsigned* addr = 0;
		
	buf_size = min(count, (sizeof(buf)-1));
	
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;
	
	while (*start == ' ')
		start++;
	
	mem = simple_strtoul(start, &start, 16);
	
	while (*start == ' ')
		start++;
		
	if (strict_strtoul(start, 16, &value))
	{
			addr = (unsigned*)mem;
			printk("%p: ", addr);
			for(i = 0; i< 8; i++){
				printk("%08x, ", addr[i]);
			}
			printk("\n");
			return -EINVAL;
	}
	addr = (unsigned*)mem;
	printk("%p: %08x\n", addr, *addr);
	*addr = value;
	printk("%p: %08x^\n", addr, *addr);
	
	return buf_size;
}
static const struct file_operations mems_fops={
	.open = mems_open_file,
	.read = mems_read_file,
	.write = mems_write_file,
};

static void aml_pcm_init_debugfs(void)
{
		debugfs_root = debugfs_create_dir("aml",NULL);
		if (IS_ERR(debugfs_root) || !debugfs_root) {
			printk("aml: Failed to create debugfs directory\n");
			debugfs_root = NULL;
		}
		
		debugfs_regs = debugfs_create_file("regs", 0644, debugfs_root, NULL, &regs_fops);
		if(!debugfs_regs){
			printk("aml: Failed to create debugfs file\n");
		}
		
		debugfs_mems = debugfs_create_file("mems", 0644, debugfs_root, NULL, &mems_fops);
		if(!debugfs_mems){
			printk("aml: Failed to create debugfs file\n");
		}
}
static void aml_pcm_cleanup_debugfs(void)
{
	debugfs_remove_recursive(debugfs_root);
}
#else
static void aml_pcm_init_debugfs()
{
}
static void aml_pcm_cleanup_debugfs()
{
}
#endif

struct snd_soc_platform aml_soc_platform = {
	.name		= "aml-audio",
	.pcm_ops 	= &aml_pcm_ops,
	.pcm_new	= aml_pcm_new,
	.pcm_free	= aml_pcm_free_dma_buffers,
	.suspend	= aml_pcm_suspend,
	.resume		= aml_pcm_resume,
};

EXPORT_SYMBOL_GPL(aml_soc_platform);

static int __init aml_alsa_audio_init(void)
{
		aml_pcm_init_debugfs();
		
		return snd_soc_register_platform(&aml_soc_platform);

}

static void __exit aml_alsa_audio_exit(void)
{
		aml_pcm_cleanup_debugfs();
        snd_soc_unregister_platform(&aml_soc_platform);
}

module_init(aml_alsa_audio_init);
module_exit(aml_alsa_audio_exit);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AML driver for ALSA");

//module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for AML soundcard.");
