#ifndef __AML_PCM_H__
#define __AML_PCM_H__

//#define debug_printk
#ifdef debug_printk
#define dug_printk(fmt, args...)  printk (fmt, ## args)
#else
#define dug_printk(fmt, args...)
#endif

typedef struct audio_stream {
    int stream_id;
    int active;
    unsigned int last_ptr;
    unsigned int size;
    unsigned int sample_rate;
    unsigned int I2S_addr;
    spinlock_t lock;
    struct snd_pcm_substream *stream;
} audio_stream_t;

typedef struct aml_audio {
    struct snd_card *card;
    struct snd_pcm *pcm;
    audio_stream_t s[2];
} aml_audio_t;

typedef struct audio_mixer_control {
    int output_devide;
    int input_device;
    int direction;
    int input_volume;
    int output_volume;
} audio_mixer_control_t;

typedef struct audio_tone_control {
    unsigned short * tone_source;
    unsigned short * tone_data;
    int tone_data_len;
    int tone_count;
    int tone_flag;
}audio_tone_control_t;

struct aml_pcm_dma_params{
		char *name;			/* stream identifier */
		struct snd_pcm_substream *substream;
		void (*dma_intr_handler)(u32, struct snd_pcm_substream *);
	
};

extern struct snd_soc_platform aml_soc_platform;

#endif
