#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <mach/hardware.h>

#include "aml_dai.h"

//#define _AML_DAI_DEBUG_

static int aml_dai_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
//  struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	  	
#ifdef _AML_DAI_DEBUG_
printk("***Entered %s:%s\n", __FILE__,__func__);
#endif
		return 0;
}

static void aml_dai_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
//  struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
#ifdef _AML_DAI_DEBUG_
printk("***Entered %s:%s\n", __FILE__,__func__);
#endif
}
static int aml_dai_set_dai_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
#ifdef _AML_DAI_DEBUG_
printk("***Entered %s:%s\n", __FILE__,__func__);
#endif
	return 0;
}

static int aml_dai_set_dai_clkdiv(struct snd_soc_dai *cpu_dai,
	int div_id, int div)
{
#ifdef _AML_DAI_DEBUG_
printk("***Entered %s:%s\n", __FILE__,__func__);
#endif
	return 0;
}

static int aml_dai_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
//	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
//	int id = rtd->dai->cpu_dai->id;
	printk("***Entered %s:%s\n", __FILE__,__func__);

	return 0;
}

static int aml_dai_prepare(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
//	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
//printk("***Entered %s:%s\n", __FILE__,__func__);
	return 0;
}

#ifdef CONFIG_PM
static int aml_dai_suspend(struct snd_soc_dai *cpu_dai)
{
	printk("***Entered %s:%s\n", __FILE__,__func__);
    return 0;
}

static int aml_dai_resume(struct snd_soc_dai *cpu_dai)
{
    printk("***Entered %s:%s\n", __FILE__,__func__);
    return 0;
}

#else /* CONFIG_PM */
#  define aml_dai_suspend	NULL
#  define aml_dai_resume	NULL
#endif /* CONFIG_PM */
			     							       
#define AML_DAI_RATES (SNDRV_PCM_RATE_8000_96000)

#define AML_DAI_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops aml_dai_ops = {
	.startup	= aml_dai_startup,
	.shutdown	= aml_dai_shutdown,
	.prepare	= aml_dai_prepare,
	.hw_params	= aml_dai_hw_params,
	.set_fmt	= aml_dai_set_dai_fmt,
	.set_clkdiv	= aml_dai_set_dai_clkdiv,
};
			  
struct snd_soc_dai aml_dai[1] = {
	{	.name = "aml-dai0",
		.id = 0,
		.suspend = aml_dai_suspend,
		.resume = aml_dai_resume,
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = AML_DAI_RATES,
			.formats = AML_DAI_FORMATS,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = AML_DAI_RATES,
			.formats = AML_DAI_FORMATS,},
		.ops = &aml_dai_ops,
		.private_data =NULL,
	}
};

EXPORT_SYMBOL_GPL(aml_dai);

static int __init aml_dai_modinit(void)
{
	return snd_soc_register_dais(aml_dai, ARRAY_SIZE(aml_dai));
}
module_init(aml_dai_modinit);

static void __exit aml_dai_modexit(void)
{
	snd_soc_unregister_dais(aml_dai, ARRAY_SIZE(aml_dai));
}
module_exit(aml_dai_modexit);

/* Module information */
MODULE_AUTHOR("Sedji Gaouaou, sedji.gaouaou@atmel.com, www.atmel.com");
MODULE_DESCRIPTION("ATMEL SSC ASoC Interface");
MODULE_LICENSE("GPL");
