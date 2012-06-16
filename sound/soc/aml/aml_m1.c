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

#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/gpio.h>

#include "aml_dai.h"
#include "aml_pcm.h"
#include "aml_m1_codec.h"

	
static int aml_m1_set_bias_level(struct snd_soc_card *card,
					enum snd_soc_bias_level level)
{
	int ret = 0;
    //struct snd_soc_codec *codec = card->codec;
    switch(level){
      case SND_SOC_BIAS_ON:
      case SND_SOC_BIAS_PREPARE:
        break;
      case SND_SOC_BIAS_OFF:
      case SND_SOC_BIAS_STANDBY:
        break;
    }
	return ret;
}

static const struct snd_soc_dapm_widget aml_m1_dapm_widgets[] = {
    SND_SOC_DAPM_SPK("Ext Spk", NULL),
    SND_SOC_DAPM_LINE("HP", NULL),
};

static const struct snd_soc_dapm_route intercon[] = {
    {"Ext Spk", NULL, "LINEOUTL"},
    {"Ext Spk", NULL, "LINEOUTR"},
    {"HP", NULL, "HP_L"},
    {"HP", NULL, "HP_R"},
};

static int aml_m1_codec_init(struct snd_soc_codec *codec)
{
    struct snd_soc_card *card = codec->socdev->card;
    int err;
    err = snd_soc_dapm_new_controls(codec, aml_m1_dapm_widgets, ARRAY_SIZE(aml_m1_dapm_widgets));
    if(err){
      dev_warn(card->dev, "Failed to register DAPM widgets\n");
      return 0;
    }
    err = snd_soc_dapm_add_routes(codec, intercon, ARRAY_SIZE(intercon));
    if(err){
      dev_warn(card->dev, "Failed to setup dapm widgets routine\n");
      return 0;
    }
    snd_soc_dapm_enable_pin(codec, "Ext Spk");
    snd_soc_dapm_enable_pin(codec, "HP");
    snd_soc_dapm_sync(codec);
	return 0;
}


static struct snd_soc_dai_link aml_m1_dai = {
	.name = "AML-M1",
	.stream_name = "AML M1 PCM",
	.cpu_dai = &aml_dai[0],  //////
	.codec_dai = &aml_m1_codec_dai,
	.init = aml_m1_codec_init,
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
	.codec_dev = &soc_codec_dev_aml_m1,
};

static struct platform_device *aml_m1_snd_device;
static struct platform_device *aml_m1_platform_device;

static int aml_m1_audio_probe(struct platform_device *pdev)
{
		int ret;
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
		platform_device_unregister(aml_m1_snd_device);
		return 0;
}

static struct platform_driver aml_m1_audio_driver = {
	.probe  = aml_m1_audio_probe,
	.remove = aml_m1_audio_remove,
	.driver = {
		.name = "aml_m1_audio",
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
