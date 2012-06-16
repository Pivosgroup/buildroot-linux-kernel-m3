#include <linux/module.h>

#include <mach/am_regs.h>

static s32 system_time_inc_adj = 0;
static u32 system_time = 0;
static u32 system_time_up = 0;
static u32 audio_pts_up = 0;

u32 timestamp_vpts_get(void)
{
    return READ_MPEG_REG(VIDEO_PTS);
}

EXPORT_SYMBOL(timestamp_vpts_get);

void timestamp_vpts_set(u32 pts)
{
    WRITE_MPEG_REG(VIDEO_PTS, pts);
}

EXPORT_SYMBOL(timestamp_vpts_set);

void timestamp_vpts_inc(s32 val)
{
    WRITE_MPEG_REG(VIDEO_PTS, READ_MPEG_REG(VIDEO_PTS) + val);
}

EXPORT_SYMBOL(timestamp_vpts_inc);

u32 timestamp_apts_get(void)
{
    return READ_MPEG_REG(AUDIO_PTS);
}

EXPORT_SYMBOL(timestamp_apts_get);

void timestamp_apts_set(u32 pts)
{
    WRITE_MPEG_REG(AUDIO_PTS, pts);
}

EXPORT_SYMBOL(timestamp_apts_set);

void timestamp_apts_inc(s32 inc)
{
	if(audio_pts_up){
    	WRITE_MPEG_REG(AUDIO_PTS, READ_MPEG_REG(AUDIO_PTS) + inc);
	}
}

EXPORT_SYMBOL(timestamp_apts_inc);

void timestamp_apts_enable(u32 enable)
{
    audio_pts_up = enable;
}

EXPORT_SYMBOL(timestamp_apts_enable);

u32 timestamp_pcrscr_get(void)
{
    return system_time;
}

EXPORT_SYMBOL(timestamp_pcrscr_get);

void timestamp_pcrscr_set(u32 pts)
{
    system_time = pts;
}

EXPORT_SYMBOL(timestamp_pcrscr_set);

void timestamp_pcrscr_inc(s32 inc)
{
    if (system_time_up) {
        system_time += inc + system_time_inc_adj;
    }
}

EXPORT_SYMBOL(timestamp_pcrscr_inc);

void timestamp_pcrscr_set_adj(s32 inc)
{
    system_time_inc_adj = inc;
}

EXPORT_SYMBOL(timestamp_pcrscr_set_adj);

void timestamp_pcrscr_enable(u32 enable)
{
    system_time_up = enable;
}

EXPORT_SYMBOL(timestamp_pcrscr_enable);

u32 timestamp_pcrscr_enable_state(void)
{
    return system_time_up;
}

EXPORT_SYMBOL(timestamp_pcrscr_enable_state);

MODULE_DESCRIPTION("AMLOGIC time sync management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
