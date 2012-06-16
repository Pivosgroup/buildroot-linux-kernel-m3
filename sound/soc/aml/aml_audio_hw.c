#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <mach/am_regs.h>
#include <linux/clk.h>
#include <linux/module.h>

#include "aml_audio_hw.h"

#ifndef MREG_AIU_958_chstat0
#define AIU_958_chstat0	AIU_958_CHSTAT_L0
#endif

#ifndef MREG_AIU_958_chstat1
#define AIU_958_chstat1	AIU_958_CHSTAT_L1
#endif

unsigned ENABLE_IEC958 = 1;
unsigned IEC958_MODE   = AIU_958_MODE_PCM16;
unsigned I2S_MODE      = AIU_I2S_MODE_PCM16;

int  audio_in_buf_ready = 0;
int audio_out_buf_ready = 0;

extern int in_error_flag;
extern int in_error;

unsigned int IEC958_bpf;
unsigned int IEC958_brst;
unsigned int IEC958_length;
unsigned int IEC958_padsize;
unsigned int IEC958_mode;
unsigned int IEC958_syncword1;
unsigned int IEC958_syncword2;
unsigned int IEC958_syncword3;
unsigned int IEC958_syncword1_mask;
unsigned int IEC958_syncword2_mask;
unsigned int IEC958_syncword3_mask;
unsigned int IEC958_chstat0_l;
unsigned int IEC958_chstat0_r;
unsigned int IEC958_chstat1_l;
unsigned int IEC958_chstat1_r;
unsigned int IEC958_mode_raw;
unsigned int IEC958_mode_codec;

EXPORT_SYMBOL(IEC958_bpf);
EXPORT_SYMBOL(IEC958_brst);
EXPORT_SYMBOL(IEC958_length);
EXPORT_SYMBOL(IEC958_padsize);
EXPORT_SYMBOL(IEC958_mode);
EXPORT_SYMBOL(IEC958_syncword1);
EXPORT_SYMBOL(IEC958_syncword2);
EXPORT_SYMBOL(IEC958_syncword3);
EXPORT_SYMBOL(IEC958_syncword1_mask);
EXPORT_SYMBOL(IEC958_syncword2_mask);
EXPORT_SYMBOL(IEC958_syncword3_mask);
EXPORT_SYMBOL(IEC958_chstat0_l);
EXPORT_SYMBOL(IEC958_chstat0_r);
EXPORT_SYMBOL(IEC958_chstat1_l);
EXPORT_SYMBOL(IEC958_chstat1_r);
EXPORT_SYMBOL(IEC958_mode_raw);
EXPORT_SYMBOL(IEC958_mode_codec);

/*
                                fIn * (M)          
            Fout   =  -----------------------------
                      		(N) * (OD+1) * (XD)
*/                      
int audio_clock_config_table[][11][2]=
{
	/*{M, N, OD, XD-1*/
	{
	//24M
		{(71 <<  0) |(4   <<  9) | (1   << 14), (26-1)},//32K	
		{(143 <<  0) |(8   <<  9) | (1   << 14), (19-1)},//44.1K	
		{(128 <<  0) |(5   <<  9) | (1   << 14), (25-1)},//48K	
		{(128 <<  0) |(5   <<  9) | (0   << 14), (25-1)},//96K	
		{(213 <<  0) |(8   <<  9) | (0   << 14), (13-1)},//192K	 
		{(71 <<  0) |(8   <<  9) | (1   << 14), (52-1)},// 8K
		{(143 <<  0) |(8   <<  9) | (1   << 14), (76-1)},// 11025
		{(32 <<  0) |(5   <<  9) | (1   << 14), (25-1)},// 12K
		{(71 <<  0) |(8   <<  9) | (1   << 14), (26-1)},// 16K
		{(143 <<  0) |(8   <<  9) | (1   << 14), (38-1)},// 22050
		{(64 <<  0) |(5   <<  9) | (1   << 14), (25-1)}   // 24K
	},
	{
	//25M
		{(19 <<  0) |(1   <<  9) | (1   << 14), (29-1)},//32K	
		{(28 <<  0) |(1   <<  9) | (1   << 14), (31-1)},//44.1K	
		{(173 <<  0) |(8   <<  9) | (1   << 14), (22-1)},//48K	
		{(173 <<  0) |(8   <<  9) | (1   << 14), (11-1)},//96K	
		{(118 <<  0) |(5   <<  9) | (1   << 14), (6-1)},//192K	  
		{(19 <<  0) |(4   <<  9) | (1   << 14), (29-1)},// 8K
		{(7 <<  0) |(1   <<  9) | (1   << 14), (31-1)},// 11025
		{(173 <<  0) |(8   <<  9) | (1   << 14), (88-1)},// 12K
		{(19 <<  0) |(2   <<  9) | (1   << 14), (29-1)},// 16K
		{(14 <<  0) |(1   <<  9) | (1   << 14), (31-1)},// 22050
		{(173 <<  0) |(8   <<  9) | (1   << 14), (44-1)}// 24K
	}
};

#if 0
/*Default acodec_regbank value*/
unsigned int acodec_regbank[74] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Reg  0 -  5
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Reg  6 - 11
                                   0x88, 0x01, 0x00, 0x00, 0x00, 0x00,  // Reg 12 - 17
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Reg 18 - 23
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Reg 24 - 29
                                   0x00, 0x00, 0x54, 0x54, 0x28, 0x28,  // Reg 30 - 35
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Reg 36 - 41
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Reg 42 - 47
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Reg 48 - 53
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Reg 54 - 59
                                   0x00, 0x00, 0x00, 0x00, 0x12, 0x12,  // Reg 60 - 65
                                   0x14, 0x14, 0x12, 0x12, 0x00, 0x00,  // Reg 66 - 71
                                   0x00, 0x00                           // Reg 72 - 73
                                  };
#endif

void audio_set_aiubuf(u32 addr, u32 size)
{
    WRITE_MPEG_REG(AIU_MEM_I2S_START_PTR, addr & 0xffffffc0);
    WRITE_MPEG_REG(AIU_MEM_I2S_RD_PTR, addr & 0xffffffc0);
    WRITE_MPEG_REG(AIU_MEM_I2S_END_PTR, (addr & 0xffffffc0) + (size & 0xffffffc0) - 64);   //this is for 16bit 2 channel
	
    WRITE_MPEG_REG(AIU_I2S_MISC,		0x0004);	// Hold I2S
	WRITE_MPEG_REG(AIU_I2S_MUTE_SWAP,	0x0000);	// No mute, no swap
	// As the default amclk is 24.576MHz, set i2s and iec958 divisor appropriately so as not to exceed the maximum sample rate.
	WRITE_MPEG_REG(AIU_I2S_MISC,		0x0010 );	// Release hold and force audio data to left or right
	
	WRITE_MPEG_REG(AIU_MEM_I2S_MASKS,		(24 << 16) |	// [31:16] IRQ block.
								(0x3 << 8) |	// [15: 8] chan_mem_mask. Each bit indicates which channels exist in memory
								(0x3 << 0));	// [ 7: 0] chan_rd_mask.  Each bit indicates which channels are READ from memory

    // 16 bit PCM mode
    //  WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 1, 6, 1);
	// Set init high then low to initilize the I2S memory logic
	WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 1, 0, 1 );
	WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 0, 0, 1 );

	WRITE_MPEG_REG(AIU_MEM_I2S_BUF_CNTL, 1 | (0 << 1));
    WRITE_MPEG_REG(AIU_MEM_I2S_BUF_CNTL, 0 | (0 << 1));

    audio_out_buf_ready = 1;
}

void audio_set_958outbuf(u32 addr, u32 size,int flag)
{
    if (ENABLE_IEC958) {
        WRITE_MPEG_REG(AIU_MEM_IEC958_START_PTR, addr & 0xffffffc0);
        WRITE_MPEG_REG(AIU_MEM_IEC958_RD_PTR, addr & 0xffffffc0);
        if(flag == 0){
          WRITE_MPEG_REG(AIU_MEM_IEC958_END_PTR, (addr & 0xffffffc0) + (size & 0xffffffc0) - 64);    // this is for 16bit 2 channel
        }else{
          WRITE_MPEG_REG(AIU_MEM_IEC958_END_PTR, (addr & 0xffffffc0) + (size & 0xffffffc0) - 1);    // this is for RAW mode
        }

        WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 1, 0, 1);
        WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 0, 1);

        WRITE_MPEG_REG(AIU_MEM_IEC958_BUF_CNTL, 1 | (0 << 1));
        WRITE_MPEG_REG(AIU_MEM_IEC958_BUF_CNTL, 0 | (0 << 1));
    }
}
void audio_in_i2s_set_buf(u32 addr, u32 size)
{
	WRITE_MPEG_REG(AUDIN_FIFO0_START, addr & 0xffffffc0);
	WRITE_MPEG_REG(AUDIN_FIFO0_PTR, (addr&0xffffffc0));
	WRITE_MPEG_REG(AUDIN_FIFO0_END, (addr&0xffffffc0) + (size&0xffffffc0)-8);

	WRITE_MPEG_REG(AUDIN_FIFO0_CTRL, (1<<AUDIN_FIFO0_EN)	// FIFO0_EN
    								|(1<<AUDIN_FIFO0_LOAD)	// load start address./* AUDIN_FIFO0_LOAD */
									|(1<<AUDIN_FIFO0_DIN_SEL)	// DIN from i2sin./* AUDIN_FIFO0_DIN_SEL */ 
	    							//|(1<<6)	// 32 bits data in./*AUDIN_FIFO0_D32b */ 
									//|(0<<7)	// put the 24bits data to  low 24 bits./* AUDIN_FIFO0_h24b */16bit 
									|(4<<AUDIN_FIFO0_ENDIAN)	// /*AUDIN_FIFO0_ENDIAN */
									|(2<<AUDIN_FIFO0_CHAN)//2 channel./* AUDIN_FIFO0_CHAN*/
		    						|(0<<16)	//to DDR
                                    |(1<<AUDIN_FIFO0_UG)    // Urgent request.  DDR SDRAM urgent request enable.
                                    |(0<<17)    // Overflow Interrupt mask
                                    |(0<<18)    // Audio in INT
			                    	//|(1<<19)	//hold 0 enable
								    |(0<<AUDIN_FIFO0_UG)	// hold0 to aififo																	
				  );

    WRITE_MPEG_REG(AUDIN_FIFO0_CTRL1,    0 << 4                       // fifo0_dest_sel
                                       | 2 << 2                       // fifo0_din_byte_num
                                       | 0 << 0);                      // fifo0_din_pos

	
	WRITE_MPEG_REG(AUDIN_I2SIN_CTRL, //(0<<I2SIN_SIZE)			///*bit8*/  16bit
									 (3<<I2SIN_SIZE)
									|(1<<I2SIN_CHAN_EN)		/*bit10~13*/ //2 channel 
									|(1<<I2SIN_POS_SYNC)	
									//|(0<<I2SIN_POS_SYNC)	
									|(1<<I2SIN_LRCLK_SKEW)
                                    |(1<<I2SIN_LRCLK_INVT)
									|(1<<I2SIN_CLK_SEL)
									|(1<<I2SIN_LRCLK_SEL)
				    				|(1<<I2SIN_DIR)
				  );															
    audio_in_buf_ready = 1;

    in_error_flag = 0;
    in_error = 0;
}
void audio_in_spdif_set_buf(u32 addr, u32 size)
{
}
extern void audio_in_enabled(int flag);

void audio_in_i2s_enable(int flag)
{
  int rd = 0, start=0;
		if(flag){
          /* reset only when start i2s input */
reset_again:
		    WRITE_MPEG_REG_BITS(AUDIN_FIFO0_CTRL, 1, 1, 1); // reset FIFO 0
            WRITE_MPEG_REG(AUDIN_FIFO0_PTR, 0);
            rd = READ_MPEG_REG(AUDIN_FIFO0_PTR);
            start = READ_MPEG_REG(AUDIN_FIFO0_START);
            if(rd != start){
              printk("error %08x, %08x !!!!!!!!!!!!!!!!!!!!!!!!\n", rd, start);
              goto reset_again;
            }

        	WRITE_MPEG_REG_BITS(AUDIN_I2SIN_CTRL, 1, I2SIN_EN, 1);
		}else{
				WRITE_MPEG_REG_BITS(AUDIN_I2SIN_CTRL, 0, I2SIN_EN, 1);
		}
        in_error_flag = 0;
        in_error = 0;
        audio_in_enabled(flag);
}

int if_audio_in_i2s_enable()
{
	return READ_MPEG_REG_BITS(AUDIN_I2SIN_CTRL, I2SIN_EN, 1);
}

void audio_in_spdif_enable(int flag)
{
		WRITE_MPEG_REG_BITS(AUDIN_FIFO1_CTRL, 1, 1, 1); // reset FIFO 1
		if(flag){
		}else{
		}
}
unsigned int audio_in_i2s_rd_ptr(void)
{
	unsigned int val;
	val = READ_MPEG_REG(AUDIN_FIFO0_RDPTR);
	printk("audio in i2s rd ptr: %x\n", val);
	return val;
}
unsigned int audio_in_i2s_wr_ptr(void)
{
	unsigned int val;
    WRITE_MPEG_REG(AUDIN_FIFO0_PTR, 1);
	val = READ_MPEG_REG(AUDIN_FIFO0_PTR);
	return (val)&(~0x3F);
	//return val&(~0x7);
}
void audio_in_i2s_set_wrptr(unsigned int val)
{
	WRITE_MPEG_REG(AUDIN_FIFO0_RDPTR, val);
}

void audio_set_i2s_mode(u32 mode)
{
    const unsigned short mask[4] = {
        0x303,                  /* 2x16 */
        0x303,                  /* 2x24 */
        0x303,                 /* 8x24 */
        0x303,                  /* 2x32 */
    };

    if (mode < sizeof(mask)/ sizeof(unsigned short)) {
       /* four two channels stream */
        WRITE_MPEG_REG(AIU_I2S_SOURCE_DESC, 1);

        if (mode == AIU_I2S_MODE_PCM16) {
            WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 1, 6, 1);
            WRITE_MPEG_REG_BITS(AIU_I2S_SOURCE_DESC, 0, 5, 1);
        } else if(mode == AIU_I2S_MODE_PCM32){
            WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 0, 6, 1);
            WRITE_MPEG_REG_BITS(AIU_I2S_SOURCE_DESC, 1, 5, 1);
        }else if(mode == AIU_I2S_MODE_PCM24){
            WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 0, 6, 1);
            WRITE_MPEG_REG_BITS(AIU_I2S_SOURCE_DESC, 1, 5, 1);
        }

        WRITE_MPEG_REG_BITS(AIU_MEM_I2S_MASKS, mask[mode], 0, 16);

        //WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 1, 0, 1);
        //WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 0, 0, 1);

        if (ENABLE_IEC958) {
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_MASKS, mask[mode], 0,
                                16);
            //WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 1, 0, 1);
            //WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 0, 1);
        }
    }

}

void audio_util_set_dac_format(unsigned format)
{
    //WRITE_MPEG_REG(AIU_CLK_CTRL_MORE,	 0x0000);	 // i2s_divisor_more does not take effect    
	WRITE_MPEG_REG(AIU_CLK_CTRL_MORE,	 0x0003);	 // i2s_divisor_more divided by 4
	
	WRITE_MPEG_REG(AIU_CLK_CTRL,		 (0 << 12) | // 958 divisor more, if true, divided by 2, 4, 6, 8.
							(1 <<  8) | // alrclk skew: 1=alrclk transitions on the cycle before msb is sent
							(1 <<  6) | // invert aoclk
                                                        (1 <<  7) | // invert lrclk
							//(1 <<  4) | // 958 divisor: 0=no div; 1=div by 2; 2=div by 3; 3=div by 4.
							//(2 <<  2) | // i2s divisor: 0=no div; 1=div by 2; 2=div by 4; 3=div by 8.
							(0 <<  2) | // i2s divisor: 0=no div; 1=div by 2; 2=div by 4; 3=div by 8.
							(1 <<  1) |  
							(1 <<  0)); // enable I2S clock
    if (format == AUDIO_ALGOUT_DAC_FORMAT_DSP) {
        WRITE_MPEG_REG_BITS(AIU_CLK_CTRL, 1, 8, 2);
    } else if (format == AUDIO_ALGOUT_DAC_FORMAT_LEFT_JUSTIFY) {
        WRITE_MPEG_REG_BITS(AIU_CLK_CTRL, 0, 8, 2);
    }
 
    WRITE_MPEG_REG(AIU_I2S_DAC_CFG, 	0x000f);	// Payload 24-bit, Msb first, alrclk = aoclk/64
	WRITE_MPEG_REG(AIU_I2S_SOURCE_DESC, 0x0001);	// four 2-channel
}

extern unsigned int get_ddr_pll_clk(void);
void audio_set_clk(unsigned freq, unsigned fs_config)
{
    int i;
    struct clk *clk;
    int xtal = 0;
    
    int (*audio_clock_config)[2];
    
   // if (fs_config == AUDIO_CLK_256FS) {
   if(1){
		int index=0;
		switch(freq)
		{
			case AUDIO_CLK_FREQ_192:
				index=4;
				break;
			case AUDIO_CLK_FREQ_96:
				index=3;
				break;
			case AUDIO_CLK_FREQ_48:
				index=2;
				break;
			case AUDIO_CLK_FREQ_441:
				index=1;
				break;
			case AUDIO_CLK_FREQ_32:
				index=0;
				break;
			case AUDIO_CLK_FREQ_8:
				index = 5;
				break;
			case AUDIO_CLK_FREQ_11:
				index = 6;
				break;
			case AUDIO_CLK_FREQ_12:
				index = 7;
				break;
			case AUDIO_CLK_FREQ_16:
				index = 8;
				break;
			case AUDIO_CLK_FREQ_22:
				index = 9;
				break;
			case AUDIO_CLK_FREQ_24:
				index = 10;
				break;
			default:
				index=0;
				break;
		};
	// get system crystal freq
		clk=clk_get_sys("clk_xtal", NULL);
		if(!clk)
		{
			printk(KERN_ERR "can't find clk %s for AUDIO PLL SETTING!\n\n","clk_xtal");
			//return -1;
		}
		else
		{
			xtal=clk_get_rate(clk);
			xtal=xtal/1000000;
			if(xtal>=24 && xtal <=25)/*current only support 24,25*/
			{
				xtal-=24;
			}
			else
			{
				printk(KERN_WARNING "UNsupport xtal setting for audio xtal=%d,default to 24M\n",xtal);	
				xtal=0;
			}
		}
		
		audio_clock_config = audio_clock_config_table[xtal];		
	
#ifdef CONFIG_SND_AML_M3
	if (((clk_get_rate(clk)==24000000)&&(get_ddr_pll_clk()==516000000)&&(index=2))) // 48k
	{
		WRITE_MPEG_REG( HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) & ~(1 << 8)); // audio clock off
		WRITE_MPEG_REG( HHI_AUD_PLL_CNTL, READ_MPEG_REG(HHI_AUD_PLL_CNTL) | (1 << 15)); // audio pll off
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 4, 9, 3); // select ddr
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 42-1, 0, 8); // 516/42
		WRITE_MPEG_REG_BITS(AIU_CODEC_ADC_LRCLK_CTRL, 64-1, 0, 12);//set codec adc ratio---lrclk
		WRITE_MPEG_REG_BITS(AIU_CODEC_DAC_LRCLK_CTRL, 64-1, 0, 12);//set codec dac ratio---lrclk
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1<<23));// gate audac_clkpi
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1 << 8));
		printk(KERN_INFO "audio 48k clock from ddr pll %dM\n", 516);
		return;
	}
	else if (((clk_get_rate(clk)==24000000)&&(get_ddr_pll_clk()==508000000)&&(index=1))) // 44.1k
	{
		WRITE_MPEG_REG( HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) & ~(1 << 8)); // audio clock off
		WRITE_MPEG_REG( HHI_AUD_PLL_CNTL, READ_MPEG_REG(HHI_AUD_PLL_CNTL) | (1 << 15)); // audio pll off
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 4, 9, 3); // select ddr
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 45-1, 0, 8); // 508/45
		WRITE_MPEG_REG_BITS(AIU_CODEC_ADC_LRCLK_CTRL, 64-1, 0, 12);//set codec adc ratio---lrclk
		WRITE_MPEG_REG_BITS(AIU_CODEC_DAC_LRCLK_CTRL, 64-1, 0, 12);//set codec dac ratio---lrclk
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1<<23));// gate audac_clkpi
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1 << 8));
		printk(KERN_INFO "audio 44.1k clock from ddr pll %dM\n", 508);
		return;
	}
	else if (((clk_get_rate(clk)==24000000)&&(get_ddr_pll_clk()==486000000)&&(index=1))) // 44.1k
	{
		WRITE_MPEG_REG( HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) & ~(1 << 8)); // audio clock off
		WRITE_MPEG_REG( HHI_AUD_PLL_CNTL, READ_MPEG_REG(HHI_AUD_PLL_CNTL) | (1 << 15)); // audio pll off
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 4, 9, 3); // select ddr
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 43-1, 0, 8); // 486/42
		WRITE_MPEG_REG_BITS(AIU_CODEC_ADC_LRCLK_CTRL, 64-1, 0, 12);//set codec adc ratio---lrclk
		WRITE_MPEG_REG_BITS(AIU_CODEC_DAC_LRCLK_CTRL, 64-1, 0, 12);//set codec dac ratio---lrclk
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1<<23));// gate audac_clkpi
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1 << 8));
		printk(KERN_INFO "audio 44.1k clock from ddr pll %dM\n", 486);
		return;
	}
	else if (((clk_get_rate(clk)==24000000)&&(get_ddr_pll_clk()==474000000)&&(index=1))) // 44.1k
	{
		WRITE_MPEG_REG( HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) & ~(1 << 8)); // audio clock off
		WRITE_MPEG_REG( HHI_AUD_PLL_CNTL, READ_MPEG_REG(HHI_AUD_PLL_CNTL) | (1 << 15)); // audio pll off
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 4, 9, 3); // select ddr
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 42-1, 0, 8); // 474/42
		WRITE_MPEG_REG_BITS(AIU_CODEC_ADC_LRCLK_CTRL, 64-1, 0, 12);//set codec adc ratio---lrclk
		WRITE_MPEG_REG_BITS(AIU_CODEC_DAC_LRCLK_CTRL, 64-1, 0, 12);//set codec dac ratio---lrclk
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1<<23));// gate audac_clkpi
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1 << 8));
		printk(KERN_INFO "audio 44.1k clock from ddr pll %dM\n", 474);
		return;
	}
#endif		

    // gate the clock off
    WRITE_MPEG_REG( HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) & ~(1 << 8));

//#ifdef CONFIG_SND_AML_M3
#ifdef CONFIG_ARCH_MESON3
    WRITE_MPEG_REG(HHI_AUD_PLL_CNTL2, 0x065e31ff);
    WRITE_MPEG_REG(HHI_AUD_PLL_CNTL3, 0x9649a941);
		// select Audio PLL as MCLK source
		//WRITE_MPEG_REG( HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) & ~(1 << 9));
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 0, 9, 3);    
		//WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 25-1, 0, 8); 
		
		WRITE_MPEG_REG_BITS(HHI_AUD_CLK_CNTL, 13-1, 0, 8); 
#endif		
    // Put the PLL to sleep
    WRITE_MPEG_REG( HHI_AUD_PLL_CNTL, READ_MPEG_REG(HHI_AUD_PLL_CNTL) | (1 << 15));//found

//#ifdef CONFIG_SND_AML_M3
#ifdef CONFIG_ARCH_MESON3
		WRITE_MPEG_REG_BITS(AIU_CODEC_ADC_LRCLK_CTRL, 64-1, 0, 12);//set codec adc ratio---lrclk
		WRITE_MPEG_REG_BITS(AIU_CODEC_DAC_LRCLK_CTRL, 64-1, 0, 12);//set codec dac ratio---lrclk
#endif		
    // Bring out of reset but keep bypassed to allow to stablize
    //Wr( HHI_AUD_PLL_CNTL, (1 << 15) | (0 << 14) | (hiu_reg & 0x3FFF) );
    WRITE_MPEG_REG( HHI_AUD_PLL_CNTL, (1 << 15) | (audio_clock_config[index][0] & 0x7FFF) );//found
    // Set the XD value
    WRITE_MPEG_REG( HHI_AUD_CLK_CNTL, (READ_MPEG_REG(HHI_AUD_CLK_CNTL) & ~(0xff << 0)) | audio_clock_config[index][1]);//found
    // delay 5uS
	//udelay(5);
	for (i = 0; i < 500000; i++) ;
    // Bring the PLL out of sleep
    WRITE_MPEG_REG( HHI_AUD_PLL_CNTL, READ_MPEG_REG(HHI_AUD_PLL_CNTL) & ~(1 << 15));//found
    
    // gate the clock on
    WRITE_MPEG_REG( HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) | (1 << 8));//found
#if ((defined CONFIG_SND_AML_M1) || (defined CONFIG_SND_AML_M2)||(defined CONFIG_SND_AML_M3))
		WRITE_MPEG_REG(HHI_AUD_CLK_CNTL, READ_MPEG_REG(HHI_AUD_CLK_CNTL) |(1<<23));// gate audac_clkpi
#endif    
    // delay 2uS
	//udelay(2);
	for (i = 0; i < 200000; i++) ;

    } else if (fs_config == AUDIO_CLK_384FS) {
    }
}

void adac_wr_reg (unsigned long addr, unsigned long data)
{
    WRITE_APB_REG((APB_BASE+(addr<<2)), data);
    //acodec_regbank[addr] = data;
} /* adac_wr_reg */

unsigned long adac_rd_reg (unsigned long addr)
{
    unsigned long data;
    data = READ_APB_REG(APB_BASE+(addr<<2));
    return (data);
} /* adac_rd_reg */

void wr_regbank (unsigned long rstdpz,
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
                 unsigned long recsel) 
{
    adac_wr_reg(0, (rstdpz<<1));
    adac_wr_reg(2, (mclksel<<0));
    adac_wr_reg(12, (i2sfsadc<<4) | (i2sfsdac<<0));
    adac_wr_reg(13, (i2ssplit<<3) | (i2smode<<0));
    adac_wr_reg(16, (pdauxdrvrz<<7) | (pdauxdrvlz<<6) | (pdhsdrvrz<<5) | (pdhsdrvlz<<4) | (pdlsdrvz<<2) | (pddacrz<<1) | (pddaclz<<0));
    adac_wr_reg(17, (pdz<<7) | (pdmbiasz<<5) | (pdvcmbufz<<4) | (pdrpgaz<<3) | (pdlpgaz<<2) | (pdadcrz<<1) | (pdadclz<<0));
    adac_wr_reg(24, (hsmute<<6) | (recmute<<4) | (micmute<<2) | (lmmute<<0));
    adac_wr_reg(25, (lsmute<<2));
    adac_wr_reg(26, (lmmix<<5) | (recmix<<3) | (ctr<<1) | (enhp<<0));
    adac_wr_reg(32, (lmvol&0xff));
    adac_wr_reg(33, (lmvol>>8));
    adac_wr_reg(34, (hsvol&0xff));
    adac_wr_reg(35, (hsvol>>8));
    adac_wr_reg(36, (pbmix&0xff));
    adac_wr_reg(37, (pbmix>>8));
    adac_wr_reg(38, (lsmix&0xff));
    adac_wr_reg(39, (lsmix>>8));
    adac_wr_reg(64, (micvol&0xff));
    adac_wr_reg(65, (micvol>>8));
    adac_wr_reg(66, (recvol&0xff));
    adac_wr_reg(67, (recvol>>8));
    adac_wr_reg(72, (recsel&0xff));
    adac_wr_reg(73, (recsel>>8));
} /* wr_regbank */

void adac_power_up_mode_2(void)
{
    adac_wr_reg(224, 0x11);
}

void adac_latch(void)
{
    adac_wr_reg(1, 1);
    adac_wr_reg(1, 0);
}

void adac_startup_seq(void)
{
    /* toggle pdz 0->1 */
    adac_wr_reg(17, adac_rd_reg(17) & ~0x80);
    adac_latch();
    adac_wr_reg(17, adac_rd_reg(17) | 0x80);
    adac_latch();

    /* toggle rstdpz 0->1 */
    adac_wr_reg(0, adac_rd_reg(0) & ~2);
    adac_latch();
    adac_wr_reg(0, adac_rd_reg(0) | 2);
    adac_latch();
}

//------------------------------------------------------------------------------
// set_acodec_source(unsigned int src)
//
// Description:
// Select audio CODEC clock source, DAC's data source.
//
// Parameters:
//  src -- 0=no clock to CODEC; 1=pcmout to DAC; 2=Aiu I2S out to DAC.
//------------------------------------------------------------------------------
void set_acodec_source (unsigned int src)
{
    unsigned long data32;
	
    // Disable acodec clock input and its DAC input
    data32  = 0;
    data32 |= 0     << 4;   // [5:4]    acodec_data_sel: 00=disable acodec_sdin; 01=Select pcm data; 10=Select AIU I2S data; 11=Not allowed.
    data32 |= 0     << 0;   // [1:0]    acodec_clk_sel: 00=Disable acodec_sclk; 01=Select pcm clock; 10=Select AIU aoclk; 11=Not allowed.
    WRITE_MPEG_REG(AIU_CODEC_CLK_DATA_CTRL, data32);

    // Enable acodec clock from the selected source
    data32  = 0;
    data32 |= 0      << 4;  // [5:4]    acodec_data_sel: 00=disable acodec_sdin; 01=Select pcm data; 10=Select AIU I2S data; 11=Not allowed.
    data32 |= src   << 0;   // [1:0]    acodec_clk_sel: 00=Disable acodec_sclk; 01=Select pcm clock; 10=Select AIU aoclk; 11=Not allowed.
    WRITE_MPEG_REG(AIU_CODEC_CLK_DATA_CTRL, data32);
    
    // Enable acodec DAC input from the selected source
    data32  = 0;
    data32 |= src   << 4;   // [5:4]    acodec_data_sel: 00=disable acodec_sdin; 01=Select pcm data; 10=Select AIU I2S data; 11=Not allowed.
    data32 |= src   << 0;   // [1:0]    acodec_clk_sel: 00=Disable acodec_sclk; 01=Select pcm clock; 10=Select AIU aoclk; 11=Not allowed.
    WRITE_MPEG_REG(AIU_CODEC_CLK_DATA_CTRL, data32);

}


extern void audio_out_enabled(int flag);
void audio_hw_958_raw();

void audio_enable_ouput(int flag)
{
    if (flag) {
        WRITE_MPEG_REG(AIU_RST_SOFT, 0x05);
        READ_MPEG_REG(AIU_I2S_SYNC);
        WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 3, 1, 2);

        if (ENABLE_IEC958) {
            if(IEC958_MODE == AIU_958_MODE_RAW)   
            {
              audio_hw_958_raw();
            }
            //else
            {
              WRITE_MPEG_REG(AIU_958_FORCE_LEFT, 0);
              WRITE_MPEG_REG_BITS(AIU_958_DCU_FF_CTRL, 1, 0, 1);
              //WRITE_MPEG_REG(AIU_958_DCU_FF_CTRL, 1);

              WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 3, 1, 2);
            }
        }
        // Maybe cause POP noise
        //audio_i2s_unmute();
    } else {
        WRITE_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 0, 1, 2);

        if (ENABLE_IEC958) {
            WRITE_MPEG_REG(AIU_958_DCU_FF_CTRL, 0);
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 1, 2);
        }
        // Maybe cause POP noise
        //audio_i2s_mute();
    }
    audio_out_enabled(flag);
}

int if_audio_out_enable()
{
	return READ_MPEG_REG_BITS(AIU_MEM_I2S_CONTROL, 1, 2);
}

unsigned int read_i2s_rd_ptr(void)
{
    unsigned int val;
    val = READ_MPEG_REG(AIU_MEM_I2S_RD_PTR);
    return val;
}

void audio_i2s_unmute(void)
{
    WRITE_MPEG_REG_BITS(AIU_I2S_MUTE_SWAP, 0, 8, 8);
    WRITE_MPEG_REG_BITS(AIU_958_CTRL, 0, 3, 2);
}

void audio_i2s_mute(void)
{
    WRITE_MPEG_REG_BITS(AIU_I2S_MUTE_SWAP, 0xff, 8, 8);
    WRITE_MPEG_REG_BITS(AIU_958_CTRL, 3, 3, 2);
}

void audio_hw_958_reset(unsigned slow_domain, unsigned fast_domain)
{
    WRITE_MPEG_REG(AIU_958_DCU_FF_CTRL, 0);

    WRITE_MPEG_REG(AIU_RST_SOFT,
                   (slow_domain << 3) | (fast_domain << 2));
}

void audio_hw_958_raw()
{
    if (ENABLE_IEC958) {
         WRITE_MPEG_REG(AIU_958_MISC, 1);
         WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 1, 8, 1);  // raw
         WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 7, 1);  // 8bit 
         WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 1, 3, 3); // endian
    }

    WRITE_MPEG_REG(AIU_958_BPF, IEC958_bpf);
    WRITE_MPEG_REG(AIU_958_BRST, IEC958_brst);
    WRITE_MPEG_REG(AIU_958_LENGTH, IEC958_length);
    WRITE_MPEG_REG(AIU_958_PADDSIZE, IEC958_padsize);
    WRITE_MPEG_REG_BITS(AIU_958_DCU_FF_CTRL, 0, 2, 2);// disable int
    
    if(IEC958_mode == 1){ // search in byte
      WRITE_MPEG_REG_BITS(AIU_958_DCU_FF_CTRL, 7, 4, 3);
    }else if(IEC958_mode == 2) { // search in word
      WRITE_MPEG_REG_BITS(AIU_958_DCU_FF_CTRL, 5, 4, 3);
    }else{
      WRITE_MPEG_REG_BITS(AIU_958_DCU_FF_CTRL, 0, 4, 3);
    }
    WRITE_MPEG_REG(AIU_958_CHSTAT_L0, IEC958_chstat0_l);
    WRITE_MPEG_REG(AIU_958_CHSTAT_L1, IEC958_chstat1_l);
    WRITE_MPEG_REG(AIU_958_CHSTAT_R0, IEC958_chstat0_r);
    WRITE_MPEG_REG(AIU_958_CHSTAT_R1, IEC958_chstat1_r);

    WRITE_MPEG_REG(AIU_958_SYNWORD1, IEC958_syncword1);
    WRITE_MPEG_REG(AIU_958_SYNWORD2, IEC958_syncword2);
    WRITE_MPEG_REG(AIU_958_SYNWORD3, IEC958_syncword3);
    WRITE_MPEG_REG(AIU_958_SYNWORD1_MASK, IEC958_syncword1_mask);
    WRITE_MPEG_REG(AIU_958_SYNWORD2_MASK, IEC958_syncword2_mask);
    WRITE_MPEG_REG(AIU_958_SYNWORD3_MASK, IEC958_syncword3_mask);

    printk("%s: %d\n", __func__, __LINE__);
    printk("\tBPF: %x\n", IEC958_bpf);
    printk("\tBRST: %x\n", IEC958_brst);
    printk("\tLENGTH: %x\n", IEC958_length);
    printk("\tPADDSIZE: %x\n", IEC958_length);
    printk("\tsyncword: %x, %x, %x\n\n", IEC958_syncword1, IEC958_syncword2, IEC958_syncword3);
    
}

void set_958_channel_status(_aiu_958_channel_status_t * set)
{

    if (set) {
	WRITE_MPEG_REG(AIU_958_CHSTAT_L0, set->chstat0_l);
	WRITE_MPEG_REG(AIU_958_CHSTAT_L1, set->chstat1_l);
	WRITE_MPEG_REG(AIU_958_CHSTAT_R0, set->chstat0_r);
	WRITE_MPEG_REG(AIU_958_CHSTAT_R1, set->chstat1_r);	
    }
}

static void audio_hw_set_958_pcm24(_aiu_958_raw_setting_t * set)
{
    WRITE_MPEG_REG(AIU_958_BPF, 0x80); /* in pcm mode, set bpf to 128 */
    set_958_channel_status(set->chan_stat);
}

void audio_set_958_mode(unsigned mode, _aiu_958_raw_setting_t * set)
{
    if(mode == AIU_958_MODE_PCM_RAW)
    	mode = AIU_958_MODE_PCM16; //use 958 raw pcm mode
    if (mode == AIU_958_MODE_RAW) {
        
        audio_hw_958_raw();
        if (ENABLE_IEC958) {
            WRITE_MPEG_REG(AIU_958_MISC, 1);
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 1, 8, 1);  // raw
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 7, 1);  // 8bit 
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 1, 3, 3); // endian
        }
        
        printk("IEC958 RAW\n");
    }else if(mode == AIU_958_MODE_PCM32){
        audio_hw_set_958_pcm24(set);
        if(ENABLE_IEC958){
            WRITE_MPEG_REG(AIU_958_MISC, 0x2020 | (1 << 7));
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 8, 1);  // pcm
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 7, 1);  // 16bit
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 3, 3); // endian
        }
        printk("IEC958 PCM32 \n");
    }else if (mode == AIU_958_MODE_PCM24) {
        audio_hw_set_958_pcm24(set);
        if (ENABLE_IEC958) {
            WRITE_MPEG_REG(AIU_958_MISC, 0x2020 | (1 << 7));
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 8, 1);  // pcm
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 7, 1);  // 16bit
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 3, 3); // endian

        }
        printk("IEC958 24bit\n");
    } else if (mode == AIU_958_MODE_PCM16) {
        audio_hw_set_958_pcm24(set);
        if (ENABLE_IEC958) {
            WRITE_MPEG_REG(AIU_958_MISC, 0x2042);
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 8, 1);  // pcm
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 1, 7, 1);  // 16bit
            WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, 0, 3, 3); // endian

        }
        printk("IEC958 16bit\n");
    }

    audio_hw_958_reset(0, 1);

    WRITE_MPEG_REG(AIU_958_FORCE_LEFT, 1);
}

void audio_hw_958_enable(unsigned flag)
{
    if (ENABLE_IEC958) {
        WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, flag, 2, 1);
        WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, flag, 1, 1);
        WRITE_MPEG_REG_BITS(AIU_MEM_IEC958_CONTROL, flag, 0, 1);
    }
}

unsigned int read_i2s_mute_swap_reg(void)
{
	unsigned int val;
    	val = READ_MPEG_REG(AIU_I2S_MUTE_SWAP);
    	return val;
}

void audio_i2s_swap_left_right(unsigned int flag)
{
	WRITE_MPEG_REG_BITS(AIU_I2S_MUTE_SWAP, flag, 0, 2);
}
