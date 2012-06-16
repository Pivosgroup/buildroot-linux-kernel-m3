#include <linux/module.h>
#include <mach/am_regs.h>

#include <mach/power_gate.h>

unsigned char GCLK_ref[GCLK_IDX_MAX];

EXPORT_SYMBOL(GCLK_ref);

int  video_dac_enable(unsigned char enable_mask)
{
    CLEAR_CBUS_REG_MASK(VENC_VDAC_SETTING, enable_mask&0x1f);
	  return 0;
}
EXPORT_SYMBOL(video_dac_enable);

int  video_dac_disable()
{
    SET_CBUS_REG_MASK(VENC_VDAC_SETTING, 0x1f);
    return 0;    
}    
EXPORT_SYMBOL(video_dac_disable);

static void turn_off_audio_DAC (void)
{
    int wr_val;
    wr_val = 0;
    WRITE_APB_REG(ADAC_RESET, wr_val);
    WRITE_APB_REG(ADAC_POWER_CTRL_REG1, wr_val);
    WRITE_APB_REG(ADAC_POWER_CTRL_REG2, wr_val);

    wr_val = 1;
    WRITE_APB_REG(ADAC_LATCH, wr_val);
    wr_val = 0;
    WRITE_APB_REG(ADAC_LATCH, wr_val);
} /* turn_off_audio_DAC */

int audio_internal_dac_disable()
{
    turn_off_audio_DAC();
    return 0;    
}    
EXPORT_SYMBOL(audio_internal_dac_disable);
