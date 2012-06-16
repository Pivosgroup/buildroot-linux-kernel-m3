#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>

#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/gpio.h>

#include "aml_dai.h"
#include "aml_pcm.h"
#include "../codecs/wm8900.h"

#define HP_DET	0//1

#if HP_DET
static struct timer_list timer;
#endif

static int aml_m1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
		struct snd_soc_pcm_runtime *rtd = substream->private_data;
		struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
		struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
		int ret;
		// TODO
printk("***Entered %s:%s\n", __FILE__,__func__);				
		
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S|SND_SOC_DAIFMT_NB_NF|SND_SOC_DAIFMT_CBS_CFS);
		if(ret<0)
			return ret;
			
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S|SND_SOC_DAIFMT_NB_NF|SND_SOC_DAIFMT_CBS_CFS);
		if(ret<0)
			return ret;
		
		
	return 0;
}
	
static struct snd_soc_ops aml_m1_ops = {
	.hw_params = aml_m1_hw_params,
};

static int aml_m1_set_bias_level(struct snd_soc_card *card,
					enum snd_soc_bias_level level)
{
	int ret = 0;
	// TODO
printk("***Entered %s:%s: %d\n", __FILE__,__func__, level);
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
#if HP_DET		
		del_timer_sync(&timer);
		timer.expires = jiffies + HZ*1;
		del_timer(&timer);
		add_timer(&timer);
#endif		
		break;
	case SND_SOC_BIAS_OFF:
	case SND_SOC_BIAS_STANDBY:
#if HP_DET		
		del_timer(&timer);
#endif		
		break;
	};
	
	return ret;
}

static const struct snd_soc_dapm_widget aml_m1_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("AVout Jack", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Headphone Mic", NULL),
};

static const struct snd_soc_dapm_route intercon[] = {

	/* speaker connected to LINEOUT */
	{"AVout Jack", NULL, "LINEOUT1L"},
	{"AVout Jack", NULL, "LINEOUT1R"},
	
	{"Headphone Jack", NULL, "HP_L"},
	{"Headphone Jack", NULL, "HP_R"},
	
	/* input */
/*
	{"RINPUT2", NULL, "Mic Bias"},
	{"LINPUT2", NULL, "Headphone Mic"},
	{"Mic Bias", NULL, "Headphone Mic"},	
*/	
};

#if HP_DET

/* Hook switch */

static struct snd_soc_jack hp_jack;

static struct snd_soc_jack_pin hp_jack_pins[] = {
	{ .pin = "Headphone Jack", .mask = SND_JACK_HEADSET },
};

static struct snd_soc_jack av_jack;

static struct snd_soc_jack_pin av_jack_pins[] = {
	{ .pin = "AVout Jack", .mask = SND_JACK_AVOUT },
};

// use LED_CS1 as detect pin

/*
 * 函数名：inner_cs_input_level
 * 用途：使用内置VGHL B/L的反馈CS0/1做输入.
 * 平台: AML8726-M_DEV_BOARD 2010-10-11_V1.0
 * 引脚：P4(VGHL_CS0/1)   T2(LED_CS1)  T1(LED_CS0)
 * 注意：输入脚的高电平需大于0.7V，低电平需小于0.4V
 * 入口参数
 * 返回参数：
       level[12]  <-->  VGHL_CS1上的逻辑电平
       level[ 8]  <-->  VGHL_CS0上的逻辑电平
       level[ 4]  <-->  LED_CS1上的逻辑电平
       level[ 0]  <-->  LED_CS0上的逻辑电平
 */
#define PWM_TCNT        (600-1)
#define PWM_MAX_VAL    (420)
 
static unsigned int inner_cs_input_level()
{
		unsigned int level = 0;
    unsigned int cs_no = 0;
    //pin64 LED_CS0
    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0, (3<<21));
    // Enable VBG_EN
    WRITE_CBUS_REG_BITS(PREG_AM_ANALOG_ADDR, 1, 0, 1);
    // pin mux 
    // wire            pm_gpioA_7_led_pwm          = pin_mux_reg0[22];
    WRITE_CBUS_REG(LED_PWM_REG0, 0);
    WRITE_CBUS_REG(LED_PWM_REG1, 0);
    WRITE_CBUS_REG(LED_PWM_REG2, 0);
    WRITE_CBUS_REG(LED_PWM_REG3, 0);
    WRITE_CBUS_REG(LED_PWM_REG4, 0);
    WRITE_CBUS_REG(LED_PWM_REG0,  (0 << 31)           |       // disable the overall circuit
                            (0 << 30)           |       // 1:Closed Loop  0:Open Loop
                            (PWM_TCNT << 16)    |       // PWM total count
                            (0 << 13)           |       // Enable
                            (1 << 12)           |       // enable
                            (0 << 10)           |       // test
                            (7 << 7)            |       // CS0 REF, Voltage FeedBack: about 0.505V
                            (7 << 4)            |       // CS1 REF, Current FeedBack: about 0.505V
                            (0 << 0)                   // DIMCTL Analog dimmer
                      );
     WRITE_CBUS_REG(LED_PWM_REG1,   (1 << 30)           |       // enable high frequency clock
                            (PWM_MAX_VAL << 16) |       // MAX PWM value
                            (0  << 0)                  // MIN PWM value
                      );
     WRITE_CBUS_REG(LED_PWM_REG2,    (0 << 31)       |       // disable timeout test mode
                            (0 << 30)       |       // timeout based on the comparator output
                            (0 << 16)       |       // timeout = 10uS
                            (0 << 13)       |       // Select oscillator as the clock (just for grins)
                            (1 << 11)       |       // 1:Enable OverCurrent Portection  0:Disable
                            (3 << 8)        |       // Filter: shift every 3 ticks
                            (0 << 6)        |       // Filter: count 1uS ticks
                            (0 << 5)        |       // PWM polarity : negative
                            (0 << 4)        |       // comparator: negative, Different with NikeD3
                            (1 << 0)               // +/- 1
                      );
    WRITE_CBUS_REG(LED_PWM_REG3,  (   1 << 16) |    // Feedback down-sampling = PWM_freq/1 = PWM_freq
                          (   1 << 14) |    // enable to re-write MATCH_VAL
                          (   210 <<  0)   // preset PWM_duty = 50%
                      );
    WRITE_CBUS_REG(LED_PWM_REG4,  (   0 << 30) |    // 1:Digital Dimmer  0:Analog Dimmer
                          (   2 << 28) |    // dimmer_timebase = 1uS
                          (1000 << 14) |    // Digital dimmer_duty = 0%, the most darkness
                          (1000 <<  0)     // dimmer_freq = 1KHz
                      );
    cs_no = READ_CBUS_REG(LED_PWM_REG3);
    
    if(cs_no &(1<<14))
      level |= (1<<0);
    if(cs_no &(1<<15))
      level |= (1<<4);
      
    WRITE_CBUS_REG(VGHL_PWM_REG0, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG1, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG2, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG3, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG4, 0);
    WRITE_CBUS_REG(VGHL_PWM_REG0, (0 << 31)           |       // disable the overall circuit
                            (0 << 30)           |       // 1:Closed Loop  0:Open Loop
                            (PWM_TCNT << 16)    |       // PWM total count
                            (0 << 13)           |       // Enable
                            (1 << 12)           |       // enable
                            (0 << 10)           |       // test
                            (7 << 7)            |       // CS0 REF, Voltage FeedBack: about 0.505V
                            (7 << 4)            |       // CS1 REF, Current FeedBack: about 0.505V
                            (0 << 0)                   // DIMCTL Analog dimmer
                       );
     WRITE_CBUS_REG(VGHL_PWM_REG1,   (1 << 30)           |       // enable high frequency clock
                            (PWM_MAX_VAL << 16) |       // MAX PWM value
                            (0  << 0)                  // MIN PWM value
                       );
     WRITE_CBUS_REG(VGHL_PWM_REG2,   (0 << 31)       |       // disable timeout test mode
                            (0 << 30)       |       // timeout based on the comparator output
                            (0 << 16)       |       // timeout = 10uS
                            (0 << 13)       |       // Select oscillator as the clock (just for grins)
                            (1 << 11)       |       // 1:Enable OverCurrent Portection  0:Disable
                            (3 << 8)        |       // Filter: shift every 3 ticks
                            (0 << 6)        |       // Filter: count 1uS ticks
                            (0 << 5)        |       // PWM polarity : negative
                            (0 << 4)        |       // comparator: negative, Different with NikeD3
                            (1 << 0)               // +/- 1
                       );
    WRITE_CBUS_REG (VGHL_PWM_REG3,  (   1 << 16) |    // Feedback down-sampling = PWM_freq/1 = PWM_freq
                          (   1 << 14) |    // enable to re-write MATCH_VAL
                          (   210 <<  0)   // preset PWM_duty = 50%
                       );
    WRITE_CBUS_REG(VGHL_PWM_REG4,  (   0 << 30) |    // 1:Digital Dimmer  0:Analog Dimmer
                          (   2 << 28) |    // dimmer_timebase = 1uS
                          (1000 << 14) |    // Digital dimmer_duty = 0%, the most darkness
                          (1000 <<  0)     // dimmer_freq = 1KHz
                       );
    cs_no = READ_CBUS_REG(VGHL_PWM_REG3);
    
    if(cs_no &(1<<14))
      level |= (1<<8);
    if(cs_no &(1<<15))
      level |= (1<<12);

    return level;
}

static int hp_detect_flag = 0;
static spinlock_t lock;
static void wm8900_hp_detect_queue(struct work_struct*);
static struct wm8900_work_t{
   unsigned long data;
   struct work_struct wm8900_workqueue;
}wm8900_work;

static void wm8900_hp_detect_queue(struct work_struct* work)
{
	int level = inner_cs_input_level();
	struct wm8900_work_t* pwork = container_of(work,struct wm8900_work_t, wm8900_workqueue);
	struct snd_soc_codec* codec = (struct snd_soc_codec*)(pwork->data);
		
		if(level == 0x1 && hp_detect_flag!= 0x1){	// HP
			snd_soc_dapm_disable_pin(codec, "AVout Jack");
	   		snd_soc_dapm_sync(codec);
			snd_soc_jack_report(&hp_jack, SND_JACK_HEADSET, SND_JACK_HEADSET);
			hp_detect_flag = 0x1;
		}else if(level == 0x10 && hp_detect_flag != 0x10){	// AV
			
			snd_soc_jack_report(&av_jack, SND_JACK_AVOUT, SND_JACK_AVOUT);
			hp_detect_flag = 0x10;
		}else if(level != hp_detect_flag){	// HDMI
			snd_soc_dapm_disable_pin(codec, "AVout Jack");
			snd_soc_dapm_disable_pin(codec, "Headphone Jack");
	   		snd_soc_dapm_sync(codec);
			hp_detect_flag = level;
		}
}

static void wm8900_hp_detect_timer(unsigned long data)
{
		struct snd_soc_codec *codec = (struct snd_soc_codec*) data;
		wm8900_work.data = (unsigned long)codec;
		schedule_work(&wm8900_work.wm8900_workqueue);
		mod_timer(&timer, jiffies + HZ*1);
}

#endif

static int aml_m1_codec_init(struct snd_soc_codec *codec)
{
		struct snd_soc_dai *codec_dai = codec->dai;
		struct snd_soc_card *card = codec->socdev->card;
	
		int err;
 
		//Add board specific DAPM widgets and routes
		err = snd_soc_dapm_new_controls(codec, aml_m1_dapm_widgets, ARRAY_SIZE(aml_m1_dapm_widgets));
		if(err){
			dev_warn(card->dev, "Failed to register DAPM widgets\n");
			return 0;
		}
		
		err = snd_soc_dapm_add_routes(codec, intercon,
				      ARRAY_SIZE(intercon));
		if(err){
			dev_warn(card->dev, "Failed to setup dapm widgets routine\n");
			return 0;
		}
		
		//hook switch
#if HP_DET
		hp_detect_flag = 0;
				
		err = snd_soc_jack_new(card, "hp_switch",
				SND_JACK_HEADSET, &hp_jack);
		if(err){
			dev_warn(card->dev, "Failed to alloc resource for hook switch\n");
		}else{
				err = snd_soc_jack_add_pins(&hp_jack,
						ARRAY_SIZE(hp_jack_pins),
						hp_jack_pins);
				if(err){
						dev_warn(card->dev, "Failed to setup hook hp jack pin\n");
				}
		}
		
		err =	snd_soc_jack_new(card, "av_switch",
				SND_JACK_HEADSET, &av_jack);
		if(err){
				dev_warn(card->dev, "Failed to alloc resource for av hook switch\n");
		}else{		
			err = snd_soc_jack_add_pins(&av_jack,
						ARRAY_SIZE(av_jack_pins),
						av_jack_pins);
				if(err){
						dev_warn(card->dev, "Failed to setup hook av jack pin\n");
				}			
		}		
		// create a timer to poll the HP IN status
		timer.function = &wm8900_hp_detect_timer;
  	timer.data = (unsigned long)codec;
  	timer.expires = jiffies + HZ*1;
  	init_timer(&timer);
#endif 
		snd_soc_dapm_nc_pin(codec,"LINPUT1");
		snd_soc_dapm_nc_pin(codec,"RINPUT1");
		
		snd_soc_dapm_enable_pin(codec, "AVout Jack");
		snd_soc_dapm_disable_pin(codec, "Headphone Jack");
		snd_soc_dapm_disable_pin(codec, "Headphone Mic");
		
		snd_soc_dapm_sync(codec);

		return 0;
}


static struct snd_soc_dai_link aml_m1_dai = {
	.name = "AML-M1",
	.stream_name = "AML M1 PCM",
	.cpu_dai = &aml_dai[0],  //////
	.codec_dai = &wm8900_dai,
	.init = aml_m1_codec_init,
	.ops = &aml_m1_ops,
};

static struct snd_soc_card snd_soc_aml_m1 = {
	.name = "AML-M1",
	.platform = &aml_soc_platform,
	.dai_link = &aml_m1_dai,
	.num_links = 1,
	.set_bias_level = aml_m1_set_bias_level,
};

static struct snd_soc_device aml_m1_snd_devdata = {
	.card = &snd_soc_aml_m1,
	.codec_dev = &soc_codec_dev_wm8900,
};

static struct platform_device *aml_m1_snd_device;
static struct platform_device *aml_m1_platform_device;

static int aml_m1_audio_probe(struct platform_device *pdev)
{
		int ret;
		//pdev->dev.platform_data;
		// TODO
printk("***Entered %s:%s\n", __FILE__,__func__);
		aml_m1_snd_device = platform_device_alloc("soc-audio", -1);
		if (!aml_m1_snd_device) {
			printk(KERN_ERR "ASoC: Platform device allocation failed\n");
			ret = -ENOMEM;
		}
	
		platform_set_drvdata(aml_m1_snd_device,&aml_m1_snd_devdata);
		aml_m1_snd_devdata.dev = &aml_m1_snd_device->dev;
	
		ret = platform_device_add(aml_m1_snd_device);
		if (ret) {
			printk(KERN_ERR "ASoC: Platform device allocation failed\n");
			goto error;
		}
		
		aml_m1_platform_device = platform_device_register_simple("aml_m1_codec",
								-1, NULL, 0);
		return 0;							
error:								
		platform_device_put(aml_m1_snd_device);								
		return ret;
}

static int aml_m1_audio_remove(struct platform_device *pdev)
{
printk("***Entered %s:%s\n", __FILE__,__func__);

#if HP_DET		
		del_timer_sync(&timer);
#endif
		platform_device_unregister(aml_m1_snd_device);
		return 0;
}

static struct platform_driver aml_m1_audio_driver = {
	.probe  = aml_m1_audio_probe,
	.remove = aml_m1_audio_remove,
	.driver = {
		.name = "aml_m1_audio_wm8900",
		.owner = THIS_MODULE,
	},
};

static int __init aml_m1_init(void)
{
		return platform_driver_register(&aml_m1_audio_driver);
}

static void __exit aml_m1_exit(void)
{
		platform_driver_unregister(&aml_m1_audio_driver);
}

module_init(aml_m1_init);
module_exit(aml_m1_exit);

/* Module information */
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("ALSA SoC AML M1 AUDIO");
MODULE_LICENSE("GPL");
