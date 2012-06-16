#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/amports/timestamp.h>
#include <linux/amports/tsync.h>

#include "amvdec.h"

#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
#include <mach/am_regs.h>
#endif

#if !defined(CONFIG_PREEMPT)
#define CONFIG_AM_TIMESYNC_LOG
#endif

#ifdef CONFIG_AM_TIMESYNC_LOG
#define AMLOG
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_ATTENTION 1
#define LOG_LEVEL_INFO      2
#define LOG_LEVEL_VAR       amlog_level_tsync
#define LOG_MASK_VAR        amlog_mask_tsync
#endif
#include <linux/amlog.h>
MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0, LOG_DEFAULT_LEVEL_DESC, LOG_DEFAULT_MASK_DESC);

//#define DEBUG
#define AVEVENT_FLAG_PARAM  0x01

//#define TSYNC_SLOW_SYNC

#define PCR_CHECK_INTERVAL  (HZ * 5)
#define PCR_DETECT_MARGIN_SHIFT_AUDIO_HI     7
#define PCR_DETECT_MARGIN_SHIFT_AUDIO_LO     7
#define PCR_DETECT_MARGIN_SHIFT_VIDEO_HI     4
#define PCR_DETECT_MARGIN_SHIFT_VIDEO_LO     4
#define PCR_MAINTAIN_MARGIN_SHIFT_AUDIO      4
#define PCR_MAINTAIN_MARGIN_SHIFT_VIDEO      1
#define PCR_RECOVER_PCR_ADJ 15

enum {
    PCR_SYNC_UNSET,
    PCR_SYNC_HI,
    PCR_SYNC_LO,
};

enum {
    PCR_TRIGGER_AUDIO,
    PCR_TRIGGER_VIDEO
};

typedef enum {
    TSYNC_STAT_PCRSCR_SETUP_NONE,
    TSYNC_STAT_PCRSCR_SETUP_VIDEO,
    TSYNC_STAT_PCRSCR_SETUP_AUDIO
} tsync_stat_t;

enum {
    TOGGLE_MODE_FIXED = 0,    // Fixed: use the Nominal M/N values
    TOGGLE_MODE_NORMAL_LOW,   // Toggle between the Nominal M/N values and the Low M/N values
    TOGGLE_MODE_NORMAL_HIGH,  // Toggle between the Nominal M/N values and the High M/N values
    TOGGLE_MODE_LOW_HIGH,     // Toggle between the Low M/N values and the High M/N Values
};

const static struct {
    const char *token;
    const u32 token_size;
    const avevent_t event;
    const u32 flag;
} avevent_token[] = {
    {"VIDEO_START", 11, VIDEO_START, AVEVENT_FLAG_PARAM},
    {"VIDEO_STOP",  10, VIDEO_STOP,  0},
    {"VIDEO_PAUSE", 11, VIDEO_PAUSE, AVEVENT_FLAG_PARAM},
    {"VIDEO_TSTAMP_DISCONTINUITY", 26, VIDEO_TSTAMP_DISCONTINUITY, AVEVENT_FLAG_PARAM},
    {"AUDIO_START", 11, AUDIO_START, AVEVENT_FLAG_PARAM},
    {"AUDIO_RESUME", 12, AUDIO_RESUME, 0},
    {"AUDIO_STOP",  10, AUDIO_STOP,  0},
    {"AUDIO_PAUSE", 11, AUDIO_PAUSE, 0},
    {"AUDIO_TSTAMP_DISCONTINUITY", 26, AUDIO_TSTAMP_DISCONTINUITY, AVEVENT_FLAG_PARAM},
};

const static char *tsync_mode_str[] = {
    "vmaster", "amaster"
};

static spinlock_t lock = SPIN_LOCK_UNLOCKED;
static tsync_mode_t tsync_mode = TSYNC_MODE_AMASTER;
static tsync_stat_t tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
static int tsync_enable = 0;   //1;
static int apts_discontinue = 0;
static int vpts_discontinue = 0;
static int pts_discontinue = 0;
static int tsync_abreak = 0;
static bool tsync_pcr_recover_enable = false;
static int pcr_sync_stat = PCR_SYNC_UNSET;
static int pcr_recover_trigger = 0;
static struct timer_list tsync_pcr_recover_timer;
static int tsync_trickmode = 0;
static int vpause_flag = 0;
static int apause_flag = 0;
static unsigned int tsync_av_thresh = AV_DISCONTINUE_THREDHOLD;
static unsigned int tsync_syncthresh = 1;
static int tsync_dec_reset_flag = 0;
static int tsync_dec_reset_video_start = 0;
static int tsync_automute_on = 0;

#define M_HIGH_DIFF  10
#define M_LOW_DIFF   10
#define PLL_FACTOR   10000

#define LOW_TOGGLE_TIME           499
#define NORMAL_TOGGLE_TIME        99
#define HIGH_TOGGLE_TIME          499

#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
extern void set_timestamp_inc_factor(u32 factor);
#endif

static void tsync_pcr_recover_with_audio(void)
{
    u32 ab_level = READ_MPEG_REG(AIU_MEM_AIFIFO_LEVEL);
    u32 ab_size = READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR)
                  - READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR) + 8;
    u32 vb_level = READ_MPEG_REG(VLD_MEM_VIFIFO_LEVEL);
    u32 vb_size = READ_MPEG_REG(VLD_MEM_VIFIFO_END_PTR)
                  - READ_MPEG_REG(VLD_MEM_VIFIFO_START_PTR) + 8;

    if ((READ_MPEG_REG(AIU_MEM_I2S_CONTROL) &
         (MEM_CTRL_EMPTY_EN | MEM_CTRL_EMPTY_EN)) == 0) {
        return;
    }

    //printk("ab_size:%d ab_level:%d vb_size:%d vb_level:%d\n", ab_size, ab_level, vb_size, vb_level);

    if ((unlikely(pcr_sync_stat != PCR_SYNC_LO)) &&
        ((ab_level < (ab_size >> PCR_DETECT_MARGIN_SHIFT_AUDIO_LO)) ||
         (vb_level < (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_LO)))) {

        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) &
                       (~((1 << 31) | (TOGGLE_MODE_LOW_HIGH << 28))));
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) | (TOGGLE_MODE_NORMAL_LOW << 28));
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) | (1 << 31));

#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
	{
		u32 inc, M_nom, N_nom;

		M_nom = READ_MPEG_REG(HHI_AUD_PLL_CNTL) & 0x1ff;
		N_nom = (READ_MPEG_REG(HHI_AUD_PLL_CNTL) >> 9) & 0x1f;

		inc = (M_nom*(NORMAL_TOGGLE_TIME+1)+(M_nom-M_LOW_DIFF)*(LOW_TOGGLE_TIME+1))*PLL_FACTOR/((NORMAL_TOGGLE_TIME+LOW_TOGGLE_TIME+2)*M_nom);
		set_timestamp_inc_factor(inc);
		printk("pll low inc: %d factor: %d\n", inc, PLL_FACTOR);
	}
#endif

        pcr_sync_stat = PCR_SYNC_LO;
        printk("pcr_sync_stat = PCR_SYNC_LO ");
        if (ab_level < (ab_size >> PCR_DETECT_MARGIN_SHIFT_AUDIO_LO)) {
            pcr_recover_trigger |= (1 << PCR_TRIGGER_AUDIO);
            printk("audio: 0x%x < 0x%x, vb_level 0x%x\n", ab_level, (ab_size >> PCR_DETECT_MARGIN_SHIFT_AUDIO_LO), vb_level);
        }
        if (vb_level < (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_LO)) {
            pcr_recover_trigger |= (1 << PCR_TRIGGER_VIDEO);
            printk("video: 0x%x < 0x%x, ab_level 0x%x\n", vb_level, (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_LO), ab_level);
        }
    } else if ((unlikely(pcr_sync_stat != PCR_SYNC_HI)) &&
		((((ab_level + (ab_size >> PCR_DETECT_MARGIN_SHIFT_AUDIO_HI)) > ab_size) ||
                ((vb_level + (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_HI)) > vb_size)))) {

        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) &
                       (~((1 << 31) | (TOGGLE_MODE_LOW_HIGH << 28))));
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) | (TOGGLE_MODE_NORMAL_HIGH << 28));
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) | (1 << 31));
#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
	{
		u32 inc, M_nom, N_nom;

		M_nom = READ_MPEG_REG(HHI_AUD_PLL_CNTL) & 0x1ff;
		N_nom = (READ_MPEG_REG(HHI_AUD_PLL_CNTL) >> 9) & 0x1f;

		inc = (M_nom*(NORMAL_TOGGLE_TIME+1)+(M_nom+M_HIGH_DIFF)*(HIGH_TOGGLE_TIME+1))*PLL_FACTOR/((NORMAL_TOGGLE_TIME+HIGH_TOGGLE_TIME+2)*M_nom);
		set_timestamp_inc_factor(inc);
		printk("pll high inc: %d factor: %d\n", inc, PLL_FACTOR);
	}
#endif
        pcr_sync_stat = PCR_SYNC_HI;
        printk("pcr_sync_stat = PCR_SYNC_HI ");
        if ((ab_level + (ab_size >> PCR_DETECT_MARGIN_SHIFT_AUDIO_HI)) > ab_size) {
            pcr_recover_trigger |= (1 << PCR_TRIGGER_AUDIO);
            printk("audio: 0x%x+0x%x > 0x%x, vb_level 0x%x\n", ab_level, (ab_size >> PCR_DETECT_MARGIN_SHIFT_AUDIO_HI), ab_size, vb_level);
        }
        if ((vb_level + (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_HI)) > vb_size) {
            pcr_recover_trigger |= (1 << PCR_TRIGGER_VIDEO);
            printk("video: 0x%x+0x%x > 0x%x, ab_level 0x%x\n", vb_level, (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_HI), vb_size, ab_level);
        }
    } else if (
    		(((pcr_sync_stat == PCR_SYNC_LO) &&
                ((!(pcr_recover_trigger & (1 << PCR_TRIGGER_AUDIO))) || (ab_level > (ab_size >> PCR_MAINTAIN_MARGIN_SHIFT_AUDIO)))
                &&
                ((!(pcr_recover_trigger & (1 << PCR_TRIGGER_VIDEO))) || ((vb_level + (vb_size >> PCR_MAINTAIN_MARGIN_SHIFT_VIDEO)) > vb_size)))
               ||
               ((pcr_sync_stat == PCR_SYNC_HI) &&
                ((!(pcr_recover_trigger & (1 << PCR_TRIGGER_AUDIO))) || ((ab_level + (ab_size >> PCR_MAINTAIN_MARGIN_SHIFT_AUDIO)) < ab_size))
                &&
                ((!(pcr_recover_trigger & (1 << PCR_TRIGGER_VIDEO))) || (vb_level < (vb_size >> PCR_MAINTAIN_MARGIN_SHIFT_VIDEO)))))) {

        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) &
                       (~((1 << 31) | (TOGGLE_MODE_LOW_HIGH << 28))));
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) | (TOGGLE_MODE_FIXED << 28));
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) | (1 << 31));

#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
	{
		set_timestamp_inc_factor(PLL_FACTOR);
		printk("pll normal inc:%d\n", PLL_FACTOR);
	}
#endif

        pcr_sync_stat = PCR_SYNC_UNSET;
        pcr_recover_trigger = 0;
        printk("pcr_sync_stat = PCR_SYNC_UNSET ab_level: 0x%x, vb_level: 0x%x\n", ab_level, vb_level);
    }
}

static void tsync_pcr_recover_with_video(void)
{
    u32 vb_level = READ_MPEG_REG(VLD_MEM_VIFIFO_LEVEL);
    u32 vb_size = READ_MPEG_REG(VLD_MEM_VIFIFO_END_PTR)
                  - READ_MPEG_REG(VLD_MEM_VIFIFO_START_PTR) + 8;

    if (vb_level < (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_LO)) {
        timestamp_pcrscr_set_adj(-PCR_RECOVER_PCR_ADJ);
        printk(" timestamp_pcrscr_set_adj(-%d);\n", PCR_RECOVER_PCR_ADJ);
    } else if ((vb_level + (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_HI)) > vb_size) {
        timestamp_pcrscr_set_adj(PCR_RECOVER_PCR_ADJ);
        printk(" timestamp_pcrscr_set_adj(%d);\n", PCR_RECOVER_PCR_ADJ);
    } else if ((vb_level > (vb_size >> PCR_MAINTAIN_MARGIN_SHIFT_VIDEO)) ||
               (vb_level < (vb_size - (vb_size >> PCR_MAINTAIN_MARGIN_SHIFT_VIDEO)))) {
        timestamp_pcrscr_set_adj(0);
    }
}

static bool tsync_pcr_recover_use_video(void)
{
    /* This is just a hacking to use audio output enable
     * as the flag to check if this is a video only playback.
     * Such processing can not handle an audio output with a
     * mixer so audio playback has no direct relationship with
     * applications. TODO.
     */
    return ((READ_MPEG_REG(AIU_MEM_I2S_CONTROL) &
             (MEM_CTRL_EMPTY_EN | MEM_CTRL_EMPTY_EN)) == 0);
}

static void tsync_pcr_recover_timer_real(void)
{
    ulong flags;

    spin_lock_irqsave(&lock, flags);

    if (tsync_pcr_recover_enable) {
        if (tsync_pcr_recover_use_video()) {
            tsync_pcr_recover_with_video();
        } else {
            timestamp_pcrscr_set_adj(0);
            tsync_pcr_recover_with_audio();
        }
    }

    spin_unlock_irqrestore(&lock, flags);
}

static void tsync_pcr_recover_timer_func(unsigned long arg)
{
    tsync_pcr_recover_timer_real();
    tsync_pcr_recover_timer.expires = jiffies + PCR_CHECK_INTERVAL;
    add_timer(&tsync_pcr_recover_timer);
}

void tsync_avevent_locked(avevent_t event, u32 param)
{
    u32 t;

    switch (event) {
    case VIDEO_START:
        if (tsync_enable) {
            tsync_mode = TSYNC_MODE_AMASTER;
        } else {
            tsync_mode = TSYNC_MODE_VMASTER;
        }

        if (tsync_dec_reset_flag) {
            tsync_dec_reset_video_start = 1;
        }

#ifndef TSYNC_SLOW_SYNC
        if (tsync_stat == TSYNC_STAT_PCRSCR_SETUP_NONE)
#endif
        {
#ifndef TSYNC_SLOW_SYNC
            if (tsync_syncthresh && (tsync_mode == TSYNC_MODE_AMASTER)) {
                timestamp_pcrscr_set(param - VIDEO_HOLD_THRESHOLD);
            } else {
                timestamp_pcrscr_set(param);
            }
#else
            timestamp_pcrscr_set(param);
#endif

            tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
            amlog_level(LOG_LEVEL_INFO, "vpts to scr, apts = 0x%x, vpts = 0x%x\n",
                        timestamp_apts_get(),
                        timestamp_vpts_get());
        }

        if (tsync_stat == TSYNC_STAT_PCRSCR_SETUP_AUDIO) {
            t = timestamp_pcrscr_get();
            if (abs(param - t) > tsync_av_thresh) {
                /* if this happens, then play */
                tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
                timestamp_pcrscr_set(param);
            }
        }
        if (/*tsync_mode == TSYNC_MODE_VMASTER && */!vpause_flag) {
            timestamp_pcrscr_enable(1);
        }
        break;

    case VIDEO_STOP:
        tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
        timestamp_vpts_set(0);
        timestamp_pcrscr_enable(0);
        break;

        /* Note:
         * Video and audio PTS discontinue happens typically with a loopback playback,
         * with same bit stream play in loop and PTS wrap back from start point.
         * When VIDEO_TSTAMP_DISCONTINUITY happens early, PCRSCR is set immedately to
         * make video still keep running in VMATSER mode. This mode is restored to
         * AMASTER mode when AUDIO_TSTAMP_DISCONTINUITY reports, or apts is close to
         * scr later.
         * When AUDIO_TSTAMP_DISCONTINUITY happens early, VMASTER mode is set to make
         * video still keep running w/o setting PCRSCR. This mode is restored to
         * AMASTER mode when VIDEO_TSTAMP_DISCONTINUITY reports, and scr is restored
         * along with new video time stamp also.
         */
    case VIDEO_TSTAMP_DISCONTINUITY:
        t = timestamp_pcrscr_get();

        if (abs(param - t) > AV_DISCONTINUE_THREDHOLD) {
            if ((tsync_mode == TSYNC_MODE_VMASTER) && (tsync_enable))
                /* restore to AMASTER mode when both video and audio
                 * send discontinue event
                 */
            {
                tsync_mode = TSYNC_MODE_AMASTER;
            } else
                /* make system time updated by itself. */
            {
                tsync_mode = TSYNC_MODE_VMASTER;
            }
            tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;

            timestamp_vpts_set(param);

            timestamp_pcrscr_set(param);

			vpts_discontinue = 1;
			printk("video pts discontinue, set pts_discontinue");

            amlog_level(LOG_LEVEL_ATTENTION, "reset scr from vpts to 0x%x\n", param);

        }
        break;

    case AUDIO_TSTAMP_DISCONTINUITY:
		timestamp_apts_set(param);
        amlog_level(LOG_LEVEL_ATTENTION, "audio discontinue, reset apts, 0x%x\n", param);	
		 
        if (!tsync_enable) {
            break;
        }		
			
        t = timestamp_pcrscr_get();

        amlog_level(LOG_LEVEL_ATTENTION, "AUDIO_TSTAMP_DISCONTINUITY, 0x%x, 0x%x\n", t, param);

        if (abs(param - t) > AV_DISCONTINUE_THREDHOLD) {
            /* switch tsync mode to free run mode,
             * making system time updated by itself.
             */
            tsync_mode = TSYNC_MODE_VMASTER;

            timestamp_apts_set(param);			
			apts_discontinue = 1;
			printk("audio pts discontinue, set pts_discontinue");

            amlog_level(LOG_LEVEL_ATTENTION, "apts interrupt: 0x%x\n", param);			
        } else {
            tsync_mode = TSYNC_MODE_AMASTER;
        }
        break;

    case AUDIO_START:		
		timestamp_apts_set(param);

		amlog_level(LOG_LEVEL_INFO, "audio start, reset apts = 0x%x\n", param);

        timestamp_apts_enable(1);
		 
        if (!tsync_enable) {
            break;
        }

        t = timestamp_pcrscr_get();

        amlog_level(LOG_LEVEL_INFO, "[%s]param %d, t %d, tsync_abreak %d\n",
                    __FUNCTION__, param, t, tsync_abreak);

        if (tsync_abreak && (abs(param - t) > TIME_UNIT90K / 10)) { // 100ms, then wait to match
            break;
        }

        tsync_abreak = 0;
        if (tsync_dec_reset_flag) { // after reset, video should be played first
            int vpts = timestamp_vpts_get();
            if ((param < vpts) || (!tsync_dec_reset_video_start)) {
                timestamp_pcrscr_set(param);
            } else {
                timestamp_pcrscr_set(vpts);
            }
            tsync_dec_reset_flag = 0;
            tsync_dec_reset_video_start = 0;
        } else {
            timestamp_pcrscr_set(param);
        }
       
        tsync_stat = TSYNC_STAT_PCRSCR_SETUP_AUDIO;

        amlog_level(LOG_LEVEL_INFO, "apts reset scr = 0x%x\n", param);

        timestamp_pcrscr_enable(1);
        apause_flag = 0;
        break;

    case AUDIO_RESUME:
		timestamp_apts_enable(1);
		
        if (!tsync_enable) {
            break;
        }
        timestamp_pcrscr_enable(1);
        apause_flag = 0;
        break;

    case AUDIO_STOP:
		timestamp_apts_set(-1);
        tsync_abreak = 0;
        if (tsync_trickmode) {
            tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
        } else {
            tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
        }
        apause_flag = 0;
        break;

    case AUDIO_PAUSE:
        apause_flag = 1;
		timestamp_apts_enable(0);
		
        if (!tsync_enable) {
            break;
        }

        timestamp_pcrscr_enable(0);
        break;

    case VIDEO_PAUSE:
        if (param == 1) {
            vpause_flag = 1;
        } else {
            vpause_flag = 0;
        }
		if(param == 1){
        	timestamp_pcrscr_enable(0);
			amlog_level(LOG_LEVEL_INFO, "video pause!\n");
		}else{
		       if (!apause_flag) {
			timestamp_pcrscr_enable(1);
			amlog_level(LOG_LEVEL_INFO, "video resume\n");
                      }
		}
        break;	

    default:
        break;
    }
    switch (event) {
    case VIDEO_START:
    case AUDIO_START:
    case AUDIO_RESUME:
        amvdev_resume();
        break;
    case VIDEO_STOP:
    case AUDIO_STOP:
    case AUDIO_PAUSE:
        amvdev_pause();
        break;
    case VIDEO_PAUSE:
        if (vpause_flag)
            amvdev_pause();
        else
            amvdev_resume();
        break;
    default:
        break;
    }
}
EXPORT_SYMBOL(tsync_avevent_locked);

void tsync_avevent(avevent_t event, u32 param)
{
    ulong flags;
    ulong fiq_flag;
    amlog_level(LOG_LEVEL_INFO, "[%s]event:%d, param %d\n",
                __FUNCTION__, event, param);
    spin_lock_irqsave(&lock, flags);

    raw_local_save_flags(fiq_flag);

    local_fiq_disable();

    tsync_avevent_locked(event, param);

    raw_local_irq_restore(fiq_flag);

    spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(tsync_avevent);

void tsync_audio_break(int audio_break)
{
    tsync_abreak = audio_break;
    return;
}
EXPORT_SYMBOL(tsync_audio_break);

void tsync_trick_mode(int trick_mode)
{
    tsync_trickmode = trick_mode;
    return;
}
EXPORT_SYMBOL(tsync_trick_mode);

void tsync_set_avthresh(unsigned int av_thresh)
{
    tsync_av_thresh = av_thresh;
    return;
}
EXPORT_SYMBOL(tsync_set_avthresh);

void tsync_set_syncthresh(unsigned int sync_thresh)
{
    tsync_syncthresh = sync_thresh;
    return;
}
EXPORT_SYMBOL(tsync_set_syncthresh);

void tsync_set_dec_reset(void)
{
    tsync_dec_reset_flag = 1;
}
EXPORT_SYMBOL(tsync_set_dec_reset);

void tsync_set_enable(int enable)
{
    tsync_enable = enable;
}
EXPORT_SYMBOL(tsync_set_enable);

int tsync_get_sync_adiscont(void)
{	
    return apts_discontinue;
}
EXPORT_SYMBOL(tsync_get_sync_adiscont);

int tsync_get_sync_vdiscont(void)
{	
    return vpts_discontinue;
}
EXPORT_SYMBOL(tsync_get_sync_vdiscont);

void tsync_set_sync_adiscont(int syncdiscont)
{
    apts_discontinue = syncdiscont;
}
EXPORT_SYMBOL(tsync_set_sync_adiscont);

void tsync_set_sync_vdiscont(int syncdiscont)
{
    vpts_discontinue = syncdiscont;
}
EXPORT_SYMBOL(tsync_set_sync_vdiscont);

void tsync_set_automute_on(int automute_on)
{
    tsync_automute_on = automute_on;
}
EXPORT_SYMBOL(tsync_set_automute_on);

int tsync_set_apts(unsigned pts)
{
    unsigned  t;
    //ssize_t r;

    timestamp_apts_set(pts);

    if (tsync_abreak) {
        tsync_abreak = 0;
    }

    if (!tsync_enable) {
        return 0;
    }

    t = timestamp_pcrscr_get();
    if (tsync_mode == TSYNC_MODE_AMASTER) {
        if (abs(pts - t) > tsync_av_thresh) {
            tsync_mode = TSYNC_MODE_VMASTER;
            amlog_level(LOG_LEVEL_INFO, "apts 0x%x shift scr 0x%x too much, switch to TSYNC_MODE_VMASTER\n",
                        pts, t);
        } else {
            timestamp_pcrscr_set(pts);
            amlog_level(LOG_LEVEL_INFO, "apts set to scr 0x%x->0x%x\n", t, pts);
        }
    } else {
        if (abs(pts - t) <= tsync_av_thresh) {
            tsync_mode = TSYNC_MODE_AMASTER;
            amlog_level(LOG_LEVEL_INFO, "switch to TSYNC_MODE_AMASTER\n");

            timestamp_pcrscr_set(pts);
        }
    }

    return 0;
}
EXPORT_SYMBOL(tsync_set_apts);

/*********************************************************/

static ssize_t show_pcr_recover(struct class *class,
                                struct class_attribute *attr,
                                char *buf)
{
    return sprintf(buf, "%s %s\n", ((tsync_pcr_recover_enable) ? "on" : "off"), ((pcr_sync_stat == PCR_SYNC_UNSET) ? ("UNSET") : ((pcr_sync_stat == PCR_SYNC_LO) ? "LO" : "HI")));
}

void tsync_pcr_recover(void)
{
	unsigned long M_nom, N_nom;
	
	if (tsync_pcr_recover_enable) {

        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_LOW_TCNT,  LOW_TOGGLE_TIME);       // Set low toggle time (oscillator clock cycles)
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_NOM_TCNT,  NORMAL_TOGGLE_TIME);    // Set nominal toggle time (oscillator clock cycles)
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_HIGH_TCNT, HIGH_TOGGLE_TIME);      // Set high toggle time (oscillator clock cycles)

        M_nom   = READ_MPEG_REG(HHI_AUD_PLL_CNTL) & 0x1ff;
        N_nom   = (READ_MPEG_REG(HHI_AUD_PLL_CNTL) >> 9) & 0x1f;

        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0, (0 << 31)      |
                       (TOGGLE_MODE_FIXED << 28)  |   // Toggle mode
                       (N_nom << 23)              |   // N high value (not used)
                       ((M_nom + M_HIGH_DIFF) << 14)          | // M high value
                       (N_nom << 9)               |   // N low value (not used)
                       ((M_nom - M_LOW_DIFF) << 0));  // M low value
        pcr_sync_stat = PCR_SYNC_UNSET;
        pcr_recover_trigger = 0;

	tsync_pcr_recover_timer_real();
	}
}

EXPORT_SYMBOL(tsync_pcr_recover);

static ssize_t store_pcr_recover(struct class *class,
                                 struct class_attribute *attr,
                                 const char *buf,
                                 size_t size)
{
    unsigned val;
    unsigned long M_nom, N_nom;
    ssize_t r;

    r = sscanf(buf, "%d", &val);
    if (r != 1) {
        return -EINVAL;
    }

    if (tsync_pcr_recover_enable == (val != 0)) {
        return size;
    }

    tsync_pcr_recover_enable = (val != 0);

    if (tsync_pcr_recover_enable) {

        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_LOW_TCNT,  LOW_TOGGLE_TIME);       // Set low toggle time (oscillator clock cycles)
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_NOM_TCNT,  NORMAL_TOGGLE_TIME);    // Set nominal toggle time (oscillator clock cycles)
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_HIGH_TCNT, HIGH_TOGGLE_TIME);      // Set high toggle time (oscillator clock cycles)

        M_nom   = READ_MPEG_REG(HHI_AUD_PLL_CNTL) & 0x1ff;
        N_nom   = (READ_MPEG_REG(HHI_AUD_PLL_CNTL) >> 9) & 0x1f;

        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0, (0 << 31)      |
                       (TOGGLE_MODE_FIXED << 28)  |   // Toggle mode
                       (N_nom << 23)              |   // N high value (not used)
                       ((M_nom + M_HIGH_DIFF) << 14)          | // M high value
                       (N_nom << 9)               |   // N low value (not used)
                       ((M_nom - M_LOW_DIFF) << 0));  // M low value
        pcr_sync_stat = PCR_SYNC_UNSET;
        pcr_recover_trigger = 0;
	
	tsync_pcr_recover_timer_real();

    } else {
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) &
                       (~((1 << 31) | (TOGGLE_MODE_LOW_HIGH << 28))));
        WRITE_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0,  READ_MPEG_REG(HHI_AUD_PLL_MOD_CNTL0) | (TOGGLE_MODE_FIXED << 28));
        pcr_sync_stat = PCR_SYNC_UNSET;
        pcr_recover_trigger = 0;
    }

    return size;
}

static ssize_t show_vpts(struct class *class,
                         struct class_attribute *attr,
                         char *buf)
{
    return sprintf(buf, "0x%x\n", timestamp_vpts_get());
}

static ssize_t store_vpts(struct class *class,
                          struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
    unsigned pts;
    ssize_t r;

    r = sscanf(buf, "0x%x", &pts);
    if (r != 1) {
        return -EINVAL;
    }

    timestamp_vpts_set(pts);
    return size;
}

static ssize_t show_apts(struct class *class,
                         struct class_attribute *attr,
                         char *buf)
{
    return sprintf(buf, "0x%x\n", timestamp_apts_get());
}

static ssize_t store_apts(struct class *class,
                          struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
    unsigned pts, t;
    ssize_t r;

    r = sscanf(buf, "0x%x", &pts);
    if (r != 1) {
        return -EINVAL;
    }

    timestamp_apts_set(pts);

    if (tsync_abreak) {
        tsync_abreak = 0;
    }

    if (!tsync_enable) {
        return size;
    }

    t = timestamp_pcrscr_get();
    if (tsync_mode == TSYNC_MODE_AMASTER) {
        if (abs(pts - t) > tsync_av_thresh) {
            tsync_mode = TSYNC_MODE_VMASTER;
            amlog_level(LOG_LEVEL_INFO, "apts 0x%x shift scr 0x%x too much, switch to TSYNC_MODE_VMASTER\n",
                        pts, t);
        } else {
            timestamp_pcrscr_set(pts);
            amlog_level(LOG_LEVEL_INFO, "apts set to scr 0x%x->0x%x\n", t, pts);
        }
    } else {
        if (abs(pts - t) <= tsync_av_thresh) {
            tsync_mode = TSYNC_MODE_AMASTER;
            amlog_level(LOG_LEVEL_INFO, "switch to TSYNC_MODE_AMASTER\n");

            timestamp_pcrscr_set(pts);
        }
    }

    return size;
}

static ssize_t show_pcrscr(struct class *class,
                           struct class_attribute *attr,
                           char *buf)
{
    return sprintf(buf, "0x%x\n", timestamp_pcrscr_get());
}

static ssize_t store_pcrscr(struct class *class,
                            struct class_attribute *attr,
                            const char *buf,
                            size_t size)
{
    unsigned pts;
    ssize_t r;

    r = sscanf(buf, "0x%x", &pts);
    if (r != 1) {
        return -EINVAL;
    }

    timestamp_pcrscr_set(pts);

    return size;
}

static ssize_t store_event(struct class *class,
                           struct class_attribute *attr,
                           const char *buf,
                           size_t size)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(avevent_token); i++) {
        if (strncmp(avevent_token[i].token, buf, avevent_token[i].token_size) == 0) {
            if (avevent_token[i].flag & AVEVENT_FLAG_PARAM) {
                char *param_str = strchr(buf, ':');

                if (!param_str) {
                    return -EINVAL;
                }

                tsync_avevent(avevent_token[i].event,
                              simple_strtoul(param_str + 1, NULL, 16));
            } else {
                tsync_avevent(avevent_token[i].event, 0);
            }

            return size;
        }
    }

    return -EINVAL;
}

static ssize_t show_mode(struct class *class,
                         struct class_attribute *attr,
                         char *buf)
{
    if (tsync_mode <= TSYNC_MODE_AMASTER) {
        return sprintf(buf, "%d: %s\n", tsync_mode, tsync_mode_str[tsync_mode]);
    }

    return sprintf(buf, "invalid mode");
}

static ssize_t show_enable(struct class *class,
                           struct class_attribute *attr,
                           char *buf)
{
    if (tsync_enable) {
        return sprintf(buf, "1: enabled\n");
    }

    return sprintf(buf, "0: disabled\n");
}

static ssize_t store_enable(struct class *class,
                            struct class_attribute *attr,
                            const char *buf,
                            size_t size)
{
    unsigned mode;
    ssize_t r;

    r = sscanf(buf, "%d", &mode);
    if ((r != 1)) {
        return -EINVAL;
    }

    if (!tsync_automute_on) {
        tsync_enable = mode ? 1 : 0;
    }

    return size;
}

static ssize_t show_discontinue(struct class *class,
                                struct class_attribute *attr,
                                char *buf)
{
	pts_discontinue = vpts_discontinue || apts_discontinue;
    if (pts_discontinue) {
        return sprintf(buf, "1: pts_discontinue\n");
    }

    return sprintf(buf, "0: pts_continue\n");
}

static ssize_t store_discontinue(struct class *class,
                                 struct class_attribute *attr,
                                 const char *buf,
                                 size_t size)
{
    unsigned discontinue;
    ssize_t r;

    r = sscanf(buf, "%d", &discontinue);
    if ((r != 1)) {
        return -EINVAL;
    }

    pts_discontinue = discontinue ? 1 : 0;

    return size;
}

static struct class_attribute tsync_class_attrs[] = {
    __ATTR(pts_video,  S_IRUGO | S_IWUSR, show_vpts,    store_vpts),
    __ATTR(pts_audio,  S_IRUGO | S_IWUSR, show_apts,    store_apts),
    __ATTR(pts_pcrscr, S_IRUGO | S_IWUSR, show_pcrscr,  store_pcrscr),
    __ATTR(event,      S_IRUGO | S_IWUSR, NULL,         store_event),
    __ATTR(mode,       S_IRUGO | S_IWUSR, show_mode,    NULL),
    __ATTR(enable,     S_IRUGO | S_IWUSR, show_enable,  store_enable),
    __ATTR(pcr_recover, S_IRUGO | S_IWUSR, show_pcr_recover,  store_pcr_recover),
    __ATTR(discontinue, S_IRUGO | S_IWUSR|S_IWGRP, show_discontinue,  store_discontinue),
    __ATTR_NULL
};

static struct class tsync_class = {
        .name = "tsync",
        .class_attrs = tsync_class_attrs,
    };

static int __init tsync_init(void)
{
    int r;

    r = class_register(&tsync_class);

    if (r) {
        amlog_level(LOG_LEVEL_ERROR, "tsync class create fail.\n");
        return r;
    }

    /* init audio pts to -1, others to 0 */
    timestamp_apts_set(-1);
    timestamp_vpts_set(0);
    timestamp_pcrscr_set(0);

    init_timer(&tsync_pcr_recover_timer);

    tsync_pcr_recover_timer.function = tsync_pcr_recover_timer_func;
    tsync_pcr_recover_timer.expires = jiffies + PCR_CHECK_INTERVAL;
    pcr_sync_stat = PCR_SYNC_UNSET;
    pcr_recover_trigger = 0;

    add_timer(&tsync_pcr_recover_timer);

    return (0);
}

static void __exit tsync_exit(void)
{
    del_timer_sync(&tsync_pcr_recover_timer);

    class_unregister(&tsync_class);
}

module_init(tsync_init);
module_exit(tsync_exit);

MODULE_DESCRIPTION("AMLOGIC time sync management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
