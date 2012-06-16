/*
 * CS4334 ALSA SoC (ASoC) codec driver
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "cs4334.h"

struct cs4334_private {
	struct snd_soc_codec codec;	
};

static struct snd_soc_codec *cs4334_codec = NULL;

#define CS4334_RATES		(SNDRV_PCM_RATE_8000_48000)
#define CS4334_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)


static int cs4334_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	return 0;
}


static int cs4334_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	return 0;
}


static int cs4334_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static struct snd_soc_dai_ops cs4334_ops = {
	.hw_params		= cs4334_pcm_hw_params,
	.set_fmt		= cs4334_set_dai_fmt,
	.digital_mute	= cs4334_mute,
};

struct snd_soc_dai cs4334_dai = {
	.name = "CS4334",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = CS4334_RATES,
		.formats = CS4334_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 0,
		.channels_max = 0,
		.rates = 0,
		.formats = 0,
	 },
	.ops = &cs4334_ops,
	.symmetric_rates = 1,
};
EXPORT_SYMBOL_GPL(cs4334_dai);

static int cs4334_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;
    printk("***Entered %s:%s\n", __FILE__,__func__);
	if (cs4334_codec == NULL) {
		dev_err(&pdev->dev, "Codec device not registered\n");
		return -ENODEV;
	}
	
	codec = cs4334_codec;
	socdev->card->codec = codec;
	
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register new PCMs\n");
		goto pcm_err;
	}

	//ret = snd_soc_init_card(socdev);
	//if (ret < 0) {
	//    dev_err(codec->dev, "failed to register card: %d\n", ret);
	//    goto card_err;
	//}

	return ret;
	
card_err:
	snd_soc_free_pcms(socdev);
pcm_err:
	return ret;
}

static int cs4334_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	
	snd_soc_free_pcms(socdev);
	
	return 0;
};


struct snd_soc_codec_device soc_codec_dev_cs4334 = {
	.probe = 	cs4334_probe,
	.remove = 	cs4334_remove,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_cs4334);


static int cs4334_register(struct cs4334_private *cs4334)
{
	struct snd_soc_codec *codec = &cs4334->codec;
	int ret;

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->private_data = cs4334;
	codec->name = "CS4334";
	codec->owner = THIS_MODULE;
	codec->dai = &cs4334_dai;
	codec->num_dai = 1;
	codec->write = NULL;
	codec->read = NULL;
	codec->reg_cache_size = 0;
	codec->reg_cache = NULL;

	cs4334_dai.dev = codec->dev;
	cs4334_codec = codec;

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_dai(&cs4334_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		snd_soc_unregister_codec(codec);
		goto err_codec;
	}

	return 0;

err_codec:
	snd_soc_unregister_codec(codec);
err:
	kfree(cs4334);
	return ret;
}

static void cs4334_unregister(struct cs4334_private *cs4334)
{
	snd_soc_unregister_dai(&cs4334_dai);
	snd_soc_unregister_codec(&cs4334->codec);
	kfree(cs4334);
}

static int cs4334_codec_platform_probe(struct platform_device *pdev)
{
	struct cs4334_private *cs4334;
	struct snd_soc_codec *codec;

	cs4334 = kzalloc(sizeof(struct cs4334_private), GFP_KERNEL);
	if (cs4334 == NULL) {
		return -ENOMEM;
	}

	codec = &cs4334->codec;

	codec->control_data = NULL;
	codec->hw_write = NULL;

	codec->dev = &pdev->dev;
	platform_set_drvdata(pdev, cs4334);
     
	return cs4334_register(cs4334);
}

static int __exit cs4334_codec_platform_remove(struct platform_device *pdev)
{
	struct cs4334_private *cs4334 = platform_get_drvdata(pdev);

	cs4334_unregister(cs4334);
	return 0;
}

static struct platform_driver cs4334_platform_driver = {
	.driver = {
		.name = "CS4334",
		.owner = THIS_MODULE,
		},
	.probe = cs4334_codec_platform_probe,
	.remove = __exit_p(cs4334_codec_platform_remove),
};

static int __init cs4334_init(void)
{
	return platform_driver_register(&cs4334_platform_driver);
}

static void __exit cs4334_exit(void)
{
	platform_driver_unregister(&cs4334_platform_driver);
}

module_init(cs4334_init);
module_exit(cs4334_exit);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("ASoC CS4334 driver");
MODULE_LICENSE("GPL");
