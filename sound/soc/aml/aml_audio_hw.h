#ifndef __AML_AUDIO_HW_H__
#define __AML_AUDIO_HW_H__

#if defined (CONFIG_ARCH_MESON) || defined (CONFIG_ARCH_MESON2) || defined (CONFIG_ARCH_MESON3)

extern unsigned int acodec_regbank[];

/* assumming PLL source is 24M */

#define AUDIO_384FS_PLL_192K        0x507d  /* 36.864M */
#define AUDIO_384FS_PLL_192K_MUX    12
#define AUDIO_384FS_CLK_192K        0x5eb

#define AUDIO_384FS_PLL_176K        0x0e7c  /* 33.8688M */
#define AUDIO_384FS_PLL_176K_MUX    25
#define AUDIO_384FS_CLK_176K        0x5eb

#define AUDIO_384FS_PLL_96K         0x507d  /* 36.864M */
#define AUDIO_384FS_PLL_96K_MUX     12
#define AUDIO_384FS_CLK_96K         0x5ef

#define AUDIO_384FS_PLL_88K         0x0e7c  /* 33.8688M */
#define AUDIO_384FS_PLL_88K_MUX     25
#define AUDIO_384FS_CLK_88K         0x5ef

#define AUDIO_384FS_PLL_48K         0x487d  /* 18.432M */
#define AUDIO_384FS_PLL_48K_MUX     12
#define AUDIO_384FS_CLK_48K_AC3     0x5ed
#define AUDIO_384FS_CLK_48K         0x5ef

#define AUDIO_384FS_PLL_44K         0x0aa3  /* 16.9344M */
#define AUDIO_384FS_PLL_44K_MUX     23
#define AUDIO_384FS_CLK_44K         0x5ef

#define AUDIO_384FS_PLL_32K         0x1480  /* 12.288M */
#define AUDIO_384FS_PLL_32K_MUX     24
#define AUDIO_384FS_CLK_32K         0x5ef

#define AUDIO_384FS_DAC_CFG         0x6

#define AUDIO_256FS_PLL_192K        0x0a53  /* 24.576M */
#define AUDIO_256FS_PLL_192K_MUX    17
#define AUDIO_256FS_CLK_192K        0x5c7

#define AUDIO_256FS_PLL_176K        0x0eba  /* 22.5792M */
#define AUDIO_256FS_PLL_176K_MUX    25
#define AUDIO_256FS_CLK_176K        0x5c7

#define AUDIO_256FS_PLL_96K         0x0a53  /* 24.576M */
#define AUDIO_256FS_PLL_96K_MUX     17
#define AUDIO_256FS_CLK_96K         0x5db

#define AUDIO_256FS_PLL_88K         0x0eba  /* 22.5792M */
#define AUDIO_256FS_PLL_88K_MUX     25
#define AUDIO_256FS_CLK_88K         0x5db

#define AUDIO_256FS_PLL_48K         0x08d3  /* 12.288M */
#define AUDIO_256FS_PLL_48K_MUX     27
#define AUDIO_256FS_CLK_48K_AC3     0x5d9
#define AUDIO_256FS_CLK_48K         0x5db

#define AUDIO_256FS_PLL_44K         0x06b9  /* 11.2896M */
#define AUDIO_256FS_PLL_44K_MUX     29
#define AUDIO_256FS_CLK_44K         0x5db

#define AUDIO_256FS_PLL_32K         0x4252  /* 8.192M */
#define AUDIO_256FS_PLL_32K_MUX     14
#define AUDIO_256FS_CLK_32K         0x5db
#define AUDIO_256FS_DAC_CFG         0x7

#endif

typedef struct {
    unsigned short pll;
    unsigned short mux;
    unsigned short devisor;
} _aiu_clk_setting_t;

typedef struct {
    unsigned short chstat0_l;
    unsigned short chstat1_l;
    unsigned short chstat0_r;
    unsigned short chstat1_r;
} _aiu_958_channel_status_t;

typedef struct {
    /* audio clock */
    unsigned short clock;
    /* analog output */
    unsigned short i2s_mode;
    unsigned short i2s_dac_mode;
    unsigned short i2s_preemphsis;
    /* digital output */
    unsigned short i958_buf_start_addr;
    unsigned short i958_buf_blksize;
    unsigned short i958_int_flag;
    unsigned short i958_mode;
    unsigned short i958_sync_mode;
    unsigned short i958_preemphsis;
    unsigned short i958_copyright;
    unsigned short bpf;
    unsigned short brst;
    unsigned short length;
    unsigned short paddsize;
    _aiu_958_channel_status_t chan_status;
} audio_output_config_t;

typedef struct {
    unsigned short int_flag;
    unsigned short bpf;
    unsigned short brst;
    unsigned short length;
    unsigned short paddsize;
    _aiu_958_channel_status_t *chan_stat;
} _aiu_958_raw_setting_t;

#define AUDIO_CLK_256FS             0
#define AUDIO_CLK_384FS             1

#define AUDIO_CLK_FREQ_192  0
#define AUDIO_CLK_FREQ_1764 1
#define AUDIO_CLK_FREQ_96   2
#define AUDIO_CLK_FREQ_882  3
#define AUDIO_CLK_FREQ_48   4
#define AUDIO_CLK_FREQ_441  5
#define AUDIO_CLK_FREQ_32   6

#define AUDIO_CLK_FREQ_8		7
#define AUDIO_CLK_FREQ_11		8
#define AUDIO_CLK_FREQ_12		9
#define AUDIO_CLK_FREQ_16		10
#define AUDIO_CLK_FREQ_22		11
#define AUDIO_CLK_FREQ_24		12


#define AIU_958_MODE_RAW    0
#define AIU_958_MODE_PCM16  1
#define AIU_958_MODE_PCM24  2
#define AIU_958_MODE_PCM32  3
#define AIU_958_MODE_PCM_RAW  4

#define AIU_I2S_MODE_PCM16   0
#define AIU_I2S_MODE_PCM24   2
#define AIU_I2S_MODE_PCM32   3

#define AUDIO_ALGOUT_DAC_FORMAT_DSP             0
#define AUDIO_ALGOUT_DAC_FORMAT_LEFT_JUSTIFY    1

extern unsigned ENABLE_IEC958;
extern unsigned IEC958_MODE;
extern unsigned I2S_MODE;

void audio_set_aiubuf(u32 addr, u32 size);
void audio_set_958outbuf(u32 addr, u32 size, int flag);
void audio_in_i2s_set_buf(u32 addr, u32 size);
void audio_in_spdif_set_buf(u32 addr, u32 size);
void audio_in_i2s_enable(int flag);
void audio_in_spdif_enable(int flag);
unsigned int audio_in_i2s_rd_ptr(void);
unsigned int audio_in_i2s_wr_ptr(void);
void audio_set_i2s_mode(u32 mode);
void audio_set_clk(unsigned freq, unsigned fs_config);
void audio_enable_ouput(int flag);
unsigned int read_i2s_rd_ptr(void);
void audio_i2s_unmute(void);
void audio_i2s_mute(void);
void audio_util_set_dac_format(unsigned format);
void audio_set_958_mode(unsigned mode, _aiu_958_raw_setting_t * set);
unsigned int read_i2s_mute_swap_reg(void);
void audio_i2s_swap_left_right(unsigned int flag);
int audio_dac_set(unsigned freq);
int if_audio_out_enable(void);
int if_audio_in_i2s_enable(void);
void audio_hw_958_enable(unsigned flag);
void audio_out_enabled(int flag);
void audio_util_set_dac_format(unsigned format);
void set_acodec_source (unsigned int src);
void adac_wr_reg (unsigned long addr, unsigned long data);
unsigned long adac_rd_reg (unsigned long addr);
void wr_regbank (    unsigned long rstdpz,
					 unsigned long mclksel, 
					 unsigned long i2sfsadc, 
					 unsigned long i2sfsdac, 
					 unsigned long i2ssplit, 
					 unsigned long i2smode, 
					 unsigned long pdauxdrvrz, 
					 unsigned long pdauxdrvlz, 
					 unsigned long pdhsdrvrz, 
					 unsigned long pdhsdrvlz, 
					 unsigned long pdlsdrvz, 
					 unsigned long pddacrz, 
					 unsigned long pddaclz, 
					 unsigned long pdz, 
					 unsigned long pdmbiasz, 
					 unsigned long pdvcmbufz,
					 unsigned long pdrpgaz, 
					 unsigned long pdlpgaz, 
					 unsigned long pdadcrz, 
					 unsigned long pdadclz, 
					 unsigned long hsmute, 
					 unsigned long recmute, 
					 unsigned long micmute, 
					 unsigned long lmmute, 
					 unsigned long lsmute, 
					 unsigned long lmmix, 
					 unsigned long recmix, 
					 unsigned long ctr, 
					 unsigned long enhp, 
					 unsigned long lmvol, 
					 unsigned long hsvol, 
					 unsigned long pbmix, 
					 unsigned long lsmix, 
					 unsigned long micvol, 
					 unsigned long recvol, 
					 unsigned long recsel);
void adac_power_up_mode_2(void);
void adac_startup_seq(void);
void adac_latch(void);
#define APB_BASE	0x4000

#define ADAC_RESET                		0x00
#define ADAC_LATCH                		0x01
#define ADAC_CLOCK                		0x02
// 0x03-0x0b  reserved
#define ADAC_I2S_CONFIG_REG1      		0x0c
#define ADAC_I2S_CONFIG_REG2      		0x0d
// 0x0e - 0x0f reserved
#define ADAC_POWER_CTRL_REG1      		0x10
#define ADAC_POWER_CTRL_REG2      		0x11
#define ADAC_POWER_CTRL_REG3      		0x12
// 0x13-0x17 reserved
#define ADAC_MUTE_CTRL_REG1       		0x18
#define ADAC_MUTE_CTRL_REG2						0x19
#define ADAC_DAC_ADC_MIXER        		0x1a
// 0x1b-0x1f reserved
#define ADAC_PLAYBACK_VOL_CTRL_LSB              0x20
#define ADAC_PLAYBACK_VOL_CTRL_MSB              0x21
#define ADAC_STEREO_HS_VOL_CTRL_LSB             0x22
#define ADAC_STEREO_HS_VOL_CTRL_MSB             0x23

#define ADAC_PLAYBACK_MIX_CTRL_LSB	0x24
#define ADAC_PLAYBACK_MIX_CTRL_MSB	0x25

#define ADAC_LS_MIX_CTRL_LSB		0x26
#define ADAC_LS_MIX_CTRL_MSB		0x27

#define ADAC_STEREO_PGA_VOL_LSB	0x40
#define ADAC_STEREO_PGA_VOL_MSB	0x41

#define ADAC_RECVOL_CTRL_LSB	0x42
#define ADAC_RECVOL_CTRL_MSB	0x43

#define ADAC_REC_CH_SEL_LSB		0x48
#define ADAC_REC_CH_SEL_MSB		0x49

#define ADAC_VCM_REG1		0x80
#define ADAC_VCM_REG2		0x81

#define ADAC_TEST_REG1	0xe0
#define ADAC_TEST_REG2	0xe1
#define ADAC_TEST_REG3	0xe2
#define ADAC_TEST_REG4	0xe3

#define ADAC_MAXREG	0xe4

#define NO_CLOCK_TO_CODEC   0
#define PCMOUT_TO_DAC       1
#define AIU_I2SOUT_TO_DAC   2


#define APB_ADAC_RESET                		(APB_BASE+ADAC_RESET*4)
#define APB_ADAC_LATCH                		(APB_BASE+ADAC_LATCH*4)
#define APB_ADAC_CLOCK                		(APB_BASE+ADAC_CLOCK*4)
#define APB_ADAC_I2S_CONFIG_REG1      		(APB_BASE+ADAC_I2S_CONFIG_REG1*4)
#define APB_ADAC_I2S_CONFIG_REG2      		(APB_BASE+ADAC_I2S_CONFIG_REG2*4)
#define APB_ADAC_POWER_CTRL_REG1      		(APB_BASE+ADAC_POWER_CTRL_REG1*4)
#define APB_ADAC_POWER_CTRL_REG2      		(APB_BASE+ADAC_POWER_CTRL_REG2*4)
#define APB_ADAC_POWER_CTRL_REG3      		(APB_BASE+ADAC_POWER_CTRL_REG3*4)
#define APB_ADAC_MUTE_CTRL_REG1       		(APB_BASE+ADAC_MUTE_CTRL_REG1*4)
#define APB_ADAC_DAC_ADC_MIXER        		(APB_BASE+ADAC_DAC_ADC_MIXER*4)
#define APB_ADAC_PLAYBACK_VOL_CTRL_LSB              (APB_BASE+ADAC_PLAYBACK_VOL_CTRL_LSB*4)
#define APB_ADAC_PLAYBACK_VOL_CTRL_MSB              (APB_BASE+ADAC_PLAYBACK_VOL_CTRL_MSB*4)
#define APB_ADAC_STEREO_HS_VOL_CTRL_LSB             (APB_BASE+ADAC_STEREO_HS_VOL_CTRL_LSB*4)
#define APB_ADAC_STEREO_HS_VOL_CTRL_MSB             (APB_BASE+ADAC_STEREO_HS_VOL_CTRL_MSB*4)



#endif
