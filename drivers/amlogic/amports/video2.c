/*
 * AMLOGIC Audio/Video streaming port driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fiq_bridge.h>
#include <linux/fs.h>
#include <mach/am_regs.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/timestamp.h>
#include <linux/amports/tsync.h>
#include <linux/amports/canvas.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <linux/amports/amstream.h>
#include <linux/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/logo/logo.h>

#ifdef CONFIG_PM
#include <linux/delay.h>
#include <linux/pm.h>
#endif

#include <asm/fiq.h>
#include <asm/uaccess.h>

#ifdef CONFIG_TVIN_VIUIN
#include <linux/tvin/tvin.h>
#endif

#include "videolog.h"

#ifdef CONFIG_AM_VIDEO_LOG
#define AMLOG
#endif
#include <linux/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_DEFAULT_LEVEL_DESC, LOG_MASK_DESC);

#include "video.h"
#include "vpp.h"

#undef CONFIG_AM_DEINTERLACE
#ifdef CONFIG_AM_DEINTERLACE
#include "deinterlace.h"
#endif

#include "linux/amports/ve.h"
#include "linux/amports/cm.h"

#include "ve_regs.h"
#include "amve.h"
#include "cm_regs.h"
#include "amcm.h"

static u32 debug = 0;

#define RECEIVER_NAME "amvideo2"
static int video_receiver_event_fun(int type, void* data, void*);

static const struct vframe_receiver_op_s video_vf_receiver =
{
    .event_cb = video_receiver_event_fun
};
static struct vframe_receiver_s video_vf_recv;

#define DRIVER_NAME "amvideo2"
#define MODULE_NAME "amvideo2"
#define DEVICE_NAME "amvideo2"

#define FIQ_VSYNC

//#define SLOW_SYNC_REPEAT
//#define INTERLACE_FIELD_MATCH_PROCESS
void vdin0_set_hscale(
                    int src_w, 
                    int dst_w, 
                    int hsc_en, 
                    int prehsc_en, 
                    int hsc_bank_length,
                    int hsc_rpt_p0_num,
                    int hsc_ini_rcv_num,
                    int hsc_ini_phase,
                    int short_lineo_en
                    );


#ifdef FIQ_VSYNC
#define BRIDGE_IRQ  INT_TIMER_D
#define BRIDGE_IRQ_SET() WRITE_CBUS_REG(ISA_TIMERD, 1)
#endif

#define RESERVE_CLR_FRAME

#define EnableVideoLayer()  \
    do { SET_MPEG_REG_MASK(VPP2_MISC, \
         VPP_VD1_PREBLEND | VPP_PREBLEND_EN | VPP_VD1_POSTBLEND); \
    } while (0)

#define EnableVideoLayer2()  \
    do { SET_MPEG_REG_MASK(VPP2_MISC, \
         VPP_VD2_PREBLEND | (0x1ff << VPP_VD2_ALPHA_BIT)); \
    } while (0)

#define DisableVideoLayer() \
    do { CLEAR_MPEG_REG_MASK(VPP2_MISC, \
         VPP_VD1_PREBLEND|VPP_VD2_PREBLEND|VPP_VD2_POSTBLEND|VPP_VD1_POSTBLEND ); \
    } while (0)

#define DisableVideoLayer_PREBELEND() \
    do { CLEAR_MPEG_REG_MASK(VPP2_MISC, \
         VPP_VD1_PREBLEND|VPP_VD2_PREBLEND); \
    } while (0)

/*********************************************************/

#define VOUT_TYPE_TOP_FIELD 0
#define VOUT_TYPE_BOT_FIELD 1
#define VOUT_TYPE_PROG      2

#define VIDEO_DISABLE_NONE    0
#define VIDEO_DISABLE_NORMAL  1
#define VIDEO_DISABLE_FORNEXT 2

#define MAX_ZOOM_RATIO 300

#define DUR2PTS(x) ((x) - ((x) >> 4))
#define DUR2PTS_RM(x) ((x) & 0xf)

static const char video_dev_id[] = "amvideo2-dev";

#ifdef CONFIG_PM
typedef struct {
    int event;
    u32 vpp_misc;
} video_pm_state_t;

static video_pm_state_t pm_state;
#endif

static DEFINE_MUTEX(video_module_mutex);
static spinlock_t lock = SPIN_LOCK_UNLOCKED;
static u32 frame_par_ready_to_set, frame_par_force_to_set;
static u32 vpts_remainder;
static bool video_property_changed = false;
static u32 video_notify_flag = 0;

#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
static u32 video_scaler_mode = 0;
static int content_top = 0, content_left = 0, content_w = 0, content_h = 0;
static int scaler_pos_changed = 0;
#endif
#if 0
int video_property_notify(int flag)
{
    video_property_changed  = flag;	
    return 0;
}

#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
int video_scaler_notify(int flag)
{
    video_scaler_mode  = flag;	
    video_property_changed = true;
    return 0;
}

u32 amvideo2_get_scaler_para(int* x, int* y, int* w, int* h, u32* ratio)
{
    *x = content_left;
    *y = content_top;
    *w = content_w;
    *h = content_h;
    //*ratio = 100;
    return video_scaler_mode;
}

void amvideo2_set_scaler_para(int x, int y, int w, int h,int flag)
{
    mutex_lock(&video_module_mutex);
    if(w < 2)
        w = 0;
    if(h < 2)
        h = 0;
    if(flag){
        if((content_left!=x)||(content_top!=y)||(content_w!=w)||(content_h!=h))
            scaler_pos_changed = 1;
        content_left = x;
        content_top = y;
        content_w = w;
        content_h = h;
    }else{
        vpp2_set_video_layer_position(x, y, w, h); 
    }
    video_property_changed = true;
    mutex_unlock(&video_module_mutex);
    return;
}


u32 amvideo2_get_scaler_mode(void)
{
    return video_scaler_mode;
}
#endif
#endif
/* display canvas */
static u32 disp_canvas_index[6] = {
    DISPLAY2_CANVAS_BASE_INDEX,
    DISPLAY2_CANVAS_BASE_INDEX + 1,
    DISPLAY2_CANVAS_BASE_INDEX + 2,
    DISPLAY2_CANVAS_BASE_INDEX + 3,
    DISPLAY2_CANVAS_BASE_INDEX + 4,
    DISPLAY2_CANVAS_BASE_INDEX + 5,
};
static u32 disp_canvas[2];
static u32 post_canvas = 0;
static ulong keep_y_addr = 0, *keep_y_addr_remap = NULL;
static ulong keep_u_addr = 0, *keep_u_addr_remap = NULL;
static ulong keep_v_addr = 0, *keep_v_addr_remap = NULL;
#define Y_BUFFER_SIZE   0x400000 // for 1280*800*3 yuv444 case
#define U_BUFFER_SIZE   0x80000
#define V_BUFFER_SIZE   0x80000

/* zoom information */
static u32 zoom_start_x_lines;
static u32 zoom_end_x_lines;
static u32 zoom_start_y_lines;
static u32 zoom_end_y_lines;

/* wide settings */
static u32 wide_setting;

/* black out policy */
#if defined(CONFIG_JPEGLOGO)
static u32 blackout = 0;
#else
static u32 blackout = 1;
#endif

/* disable video */
static u32 disable_video = VIDEO_DISABLE_NONE;

#ifdef SLOW_SYNC_REPEAT
/* video frame repeat count */
static u32 frame_repeat_count = 0;
#endif

static u32 clone = 1;
static u32 clone_vpts_remainder;
static int clone_frame_rate_delay = 0;
static int clone_frame_rate_set_value = 0;
static int clone_frame_rate_force = 0;
static int clone_frame_rate = 30; 
static int clone_frame_scale_width = 0;
static int throw_frame = 0;
/* vout */

static const vinfo_t *vinfo = NULL;


/* config */
static vframe_t *cur_dispbuf = NULL;
static vframe_t vf_local;
static u32 vsync_pts_inc;

/* frame rate calculate */
static u32 last_frame_count = 0;
static u32 frame_count = 0;
static u32 last_frame_time = 0;
static u32 timer_count  =0 ;
static vpp_frame_par_t *cur_frame_par, *next_frame_par;
static vpp_frame_par_t frame_parms[2];

/* vsync pass flag */
static u32 wait_sync;

#ifdef FIQ_VSYNC
static bridge_item_t vsync_fiq_bridge;
#endif

/* trickmode i frame*/
static u32 trickmode_i = 0;

/* trickmode ff/fb */
static u32 trickmode_fffb = 0;
static atomic_t trickmode_framedone = ATOMIC_INIT(0);

static const f2v_vphase_type_t vpp_phase_table[4][3] = {
    {F2V_P2IT,  F2V_P2IB,  F2V_P2P },   /* VIDTYPE_PROGRESSIVE */
    {F2V_IT2IT, F2V_IT2IB, F2V_IT2P},   /* VIDTYPE_INTERLACE_TOP */
    {F2V_P2IT,  F2V_P2IB,  F2V_P2P },
    {F2V_IB2IT, F2V_IB2IB, F2V_IB2P}    /* VIDTYPE_INTERLACE_BOTTOM */
};

static const u8 skip_tab[6] = { 0x24, 0x04, 0x68, 0x48, 0x28, 0x08 };
/* wait queue for poll */
static wait_queue_head_t amvideo_trick_wait;

#if 0
/* video enhancement */
static struct ve_bext_s ve_bext;
static struct ve_dnlp_s ve_dnlp;
static struct ve_hsvs_s ve_hsvs;
static struct ve_ccor_s ve_ccor;
static struct ve_benh_s ve_demo;
static struct ve_demo_s ve_demo;

typedef struct ve_regs_s {
    unsigned val  : 32;
    unsigned reg  : 14;
    unsigned port :  2; // port port_addr            port_data            remark
                        // 0    NA                   NA                   direct access
                        // 1    VPP_CHROMA_ADDR_PORT VPP_CHROMA_DATA_PORT CM port registers
                        // 2    NA                   NA                   reserved
                        // 3    NA                   NA                   reserved
    unsigned bit  :  5;
    unsigned wid  :  5;
    unsigned mode :  1; // 0:read, 1:write
    unsigned rsv  :  5;
} ve_regs_t;

static uchar ve_dnlp_tgt[64], ve_dnlp_rt;
static ulong ve_dnlp_lpf[64], ve_dnlp_reg[16];

static ulong ve_benh_ve_benh_inv[32][2] = { // [0]: inv_10_0, [1]: inv_11
    {2047, 1}, {2047, 1}, {   0, 1}, {1365, 0}, {1024, 0}, { 819, 0}, { 683, 0}, { 585, 0},
    { 512, 0}, { 455, 0}, { 410, 0}, { 372, 0}, { 341, 0}, { 315, 0}, { 293, 0}, { 273, 0},
    { 256, 0}, { 241, 0}, { 228, 0}, { 216, 0}, { 205, 0}, { 195, 0}, { 186, 0}, { 178, 0},
    { 171, 0}, { 164, 0}, { 158, 0}, { 152, 0}, { 146, 0}, { 141, 0}, { 137, 0}, { 132, 0},
};

static ulong ve_reg_limit(ulong val, ulong wid)
{
    if (val < (1 << wid)) {
        return(val);
    } else {
        return((1 << wid) - 1);
    }
}

#endif

/*********************************************************/
static inline vframe_t *video_vf_peek(void)
{
#ifdef CONFIG_AM_DEINTERLACE
    int deinterlace_mode = get_deinterlace_mode();

    if (deinterlace_mode == 2) {
        int di_pre_recycle_buf = get_di_pre_recycle_buf();

        if (di_pre_recycle_buf == 1) {
            return peek_di_out_buf();
        } else if (di_pre_recycle_buf == 0) {
            return vf_peek(RECEIVER_NAME);
        }
    } else {
        return vf_peek(RECEIVER_NAME);
    }

    return NULL;
#else
    return vf_peek(RECEIVER_NAME);
#endif
}

static inline vframe_t *video_vf_get(void)
{
    vframe_t *vf = NULL;
#ifdef CONFIG_AM_DEINTERLACE
    int deinterlace_mode = get_deinterlace_mode();

    if (deinterlace_mode == 2) {
        int di_pre_recycle_buf = get_di_pre_recycle_buf();

        if (di_pre_recycle_buf == 1) {
            vframe_t *disp_buf = peek_di_out_buf();

            if (disp_buf) {
                set_post_di_mem(disp_buf->blend_mode);
                inc_field_counter();
            }

            return disp_buf;
        } else if (di_pre_recycle_buf == 0) {
            vf = vf_get(RECEIVER_NAME);

                if (vf) video_notify_flag |= VIDEO_NOTIFY_PROVIDER_GET;
                return vf;
            }
    } else {
        vf = vf_get(RECEIVER_NAME);

            if (vf) video_notify_flag |= VIDEO_NOTIFY_PROVIDER_GET;
            return vf;
        }

    return NULL;
#else
    vf = vf_get(RECEIVER_NAME);

    if (vf) video_notify_flag |= VIDEO_NOTIFY_PROVIDER_GET;
    return vf;

#endif
}
static int  vf_get_states(vframe_states_t *states)
{
    int ret = -1;
    unsigned long flags;
    struct vframe_provider_s *vfp;
    vfp = vf_get_provider(RECEIVER_NAME);
    spin_lock_irqsave(&lock, flags);
    if (vfp && vfp->ops && vfp->ops->vf_states) {
        ret=vfp->ops->vf_states(states, vfp->op_arg);
    }
    spin_unlock_irqrestore(&lock, flags);
    return ret;
}

static inline void video_vf_put(vframe_t *vf)
{
    struct vframe_provider_s *vfp = vf_get_provider(RECEIVER_NAME);
#ifdef CONFIG_AM_DEINTERLACE
    int deinterlace_mode = get_deinterlace_mode();

    if (deinterlace_mode == 2) {
        int di_pre_recycle_buf = get_di_pre_recycle_buf();

        if (di_pre_recycle_buf == 0) {
            if (vfp) {
                vf_put(vf, RECEIVER_NAME);
                video_notify_flag |= VIDEO_NOTIFY_PROVIDER_PUT;
            }
        }
    } else {
        if (vfp) {
            vf_put(vf, RECEIVER_NAME);
            video_notify_flag |= VIDEO_NOTIFY_PROVIDER_PUT;
        }
    }
#else
    if (vfp) {
        vf_put(vf, RECEIVER_NAME);
        video_notify_flag |= VIDEO_NOTIFY_PROVIDER_PUT;
    }
#endif
}

static void vpp_settings_h(vpp_frame_par_t *framePtr)
{
    vppfilter_mode_t *vpp_filter = &framePtr->vpp_filter;
    u32 r1, r2, r3;

    r1 = framePtr->VPP_hsc_linear_startp - framePtr->VPP_hsc_startp;
    r2 = framePtr->VPP_hsc_linear_endp   - framePtr->VPP_hsc_startp;
    r3 = framePtr->VPP_hsc_endp          - framePtr->VPP_hsc_startp;

    WRITE_MPEG_REG(VPP2_POSTBLEND_VD1_H_START_END,
                   ((framePtr->VPP_hsc_startp & VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
                   ((framePtr->VPP_hsc_endp   & VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));

    //WRITE_MPEG_REG(VPP_BLEND_VD2_H_START_END,
    //               ((framePtr->VPP_hsc_startp & VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
    //               ((framePtr->VPP_hsc_endp   & VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));

    WRITE_MPEG_REG(VPP2_HSC_REGION12_STARTP,
                   (0 << VPP_REGION1_BIT) |
                   ((r1 & VPP_REGION_MASK) << VPP_REGION2_BIT));

    WRITE_MPEG_REG(VPP2_HSC_REGION34_STARTP,
                   ((r2 & VPP_REGION_MASK) << VPP_REGION3_BIT) |
                   ((r3 & VPP_REGION_MASK) << VPP_REGION4_BIT));
    WRITE_MPEG_REG(VPP2_HSC_REGION4_ENDP, r3);

    WRITE_MPEG_REG(VPP2_HSC_START_PHASE_STEP,
                   vpp_filter->vpp_hf_start_phase_step);

    WRITE_MPEG_REG(VPP2_LINE_IN_LENGTH, framePtr->VPP_line_in_length_);
    WRITE_MPEG_REG(VPP2_PREBLEND_H_SIZE, framePtr->VPP_line_in_length_);
}

static void vpp_settings_v(vpp_frame_par_t *framePtr)
{
    vppfilter_mode_t *vpp_filter = &framePtr->vpp_filter;
    u32 r;

    r = framePtr->VPP_vsc_endp - framePtr->VPP_vsc_startp;

    WRITE_MPEG_REG(VPP2_POSTBLEND_VD1_V_START_END,
                   ((framePtr->VPP_vsc_startp & VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
                   ((framePtr->VPP_vsc_endp   & VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));

    //WRITE_MPEG_REG(VPP2_BLEND_VD2_V_START_END,
    //               (((framePtr->VPP_vsc_endp / 2) & VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
    //               (((framePtr->VPP_vsc_endp) & VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));

    WRITE_MPEG_REG(VPP2_VSC_REGION12_STARTP, 0);
    WRITE_MPEG_REG(VPP2_VSC_REGION34_STARTP,
                   ((r & VPP_REGION_MASK) << VPP_REGION3_BIT) |
                   ((r & VPP_REGION_MASK) << VPP_REGION4_BIT));
    WRITE_MPEG_REG(VPP2_VSC_REGION4_ENDP, r);

    WRITE_MPEG_REG(VPP2_VSC_START_PHASE_STEP,
                   vpp_filter->vpp_vsc_start_phase_step);
}

static void zoom_display_horz(void)
{
    WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA_X0,
                   (zoom_start_x_lines << VDIF_PIC_START_BIT) |
                   (zoom_end_x_lines   << VDIF_PIC_END_BIT));

    WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA_X0,
                   (zoom_start_x_lines / 2 << VDIF_PIC_START_BIT) |
                   (zoom_end_x_lines / 2   << VDIF_PIC_END_BIT));

    WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA_X1,
                   (zoom_start_x_lines << VDIF_PIC_START_BIT) |
                   (zoom_end_x_lines   << VDIF_PIC_END_BIT));

    WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA_X1,
                   (zoom_start_x_lines / 2 << VDIF_PIC_START_BIT) |
                   (zoom_end_x_lines / 2   << VDIF_PIC_END_BIT));

    WRITE_MPEG_REG(VIU2_VD1_FMT_W,
                   ((zoom_end_x_lines - zoom_start_x_lines + 1) << VD1_FMT_LUMA_WIDTH_BIT) |
                   ((zoom_end_x_lines / 2 - zoom_start_x_lines / 2 + 1) << VD1_FMT_CHROMA_WIDTH_BIT));
#if 0
    WRITE_MPEG_REG(VD2_IF0_LUMA_X0,
                   (zoom_start_x_lines << VDIF_PIC_START_BIT) |
                   (zoom_end_x_lines   << VDIF_PIC_END_BIT));

    WRITE_MPEG_REG(VD2_IF0_CHROMA_X0,
                   (zoom_start_x_lines / 2 << VDIF_PIC_START_BIT) |
                   (zoom_end_x_lines / 2   << VDIF_PIC_END_BIT));

    WRITE_MPEG_REG(VD2_IF0_LUMA_X1,
                   (zoom_start_x_lines << VDIF_PIC_START_BIT) |
                   (zoom_end_x_lines   << VDIF_PIC_END_BIT));

    WRITE_MPEG_REG(VD2_IF0_CHROMA_X1,
                   (zoom_start_x_lines / 2 << VDIF_PIC_START_BIT) |
                   (zoom_end_x_lines / 2   << VDIF_PIC_END_BIT));

    WRITE_MPEG_REG(VIU_VD2_FMT_W,
                   ((zoom_end_x_lines - zoom_start_x_lines + 1) << VD1_FMT_LUMA_WIDTH_BIT) |
                   ((zoom_end_x_lines / 2 - zoom_start_x_lines / 2 + 1) << VD1_FMT_CHROMA_WIDTH_BIT));
#endif
}

static void zoom_display_vert(void)
{
    if ((cur_dispbuf) && (cur_dispbuf->type & VIDTYPE_MVC)) {
        WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA_Y0,
                       (zoom_start_y_lines * 2 << VDIF_PIC_START_BIT) |
                       (zoom_end_y_lines * 2   << VDIF_PIC_END_BIT));

        WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA_Y0,
                       ((zoom_start_y_lines) << VDIF_PIC_START_BIT) |
                       ((zoom_end_y_lines)   << VDIF_PIC_END_BIT));

        //WRITE_MPEG_REG(VD2_IF0_LUMA_Y0,
        //               (zoom_start_y_lines * 2 << VDIF_PIC_START_BIT) |
        //               (zoom_end_y_lines * 2   << VDIF_PIC_END_BIT));

        //WRITE_MPEG_REG(VD2_IF0_CHROMA_Y0,
        //               ((zoom_start_y_lines) << VDIF_PIC_START_BIT) |
        //               ((zoom_end_y_lines)   << VDIF_PIC_END_BIT));
    } else {
        WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA_Y0,
                       (zoom_start_y_lines << VDIF_PIC_START_BIT) |
                       (zoom_end_y_lines   << VDIF_PIC_END_BIT));

        WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA_Y0,
                       ((zoom_start_y_lines / 2) << VDIF_PIC_START_BIT) |
                       ((zoom_end_y_lines / 2)   << VDIF_PIC_END_BIT));

        WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA_Y1,
                       (zoom_start_y_lines << VDIF_PIC_START_BIT) |
                       (zoom_end_y_lines << VDIF_PIC_END_BIT));

        WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA_Y1,
                       ((zoom_start_y_lines / 2) << VDIF_PIC_START_BIT) |
                       ((zoom_end_y_lines / 2) << VDIF_PIC_END_BIT));
    }
}

static void vsync_toggle_frame(vframe_t *vf)
{
    u32 first_picture = 0;
#if 0
#ifdef CONFIG_AM_DEINTERLACE
    int deinterlace_mode = get_deinterlace_mode();
#endif
    if(vf->early_process_fun){
        vf->early_process_fun(vf->private_data);
    }
    else{
#ifndef CONFIG_AM_DEINTERLACE
        if(READ_MPEG_REG(DI_IF1_GEN_REG)&0x1){
            //disable post di
    	      WRITE_MPEG_REG(DI_POST_CTRL, 0x3 << 30);
            WRITE_MPEG_REG(DI_POST_SIZE, (32-1) | ((128-1) << 16));
	          WRITE_MPEG_REG(DI_IF1_GEN_REG, READ_MPEG_REG(DI_IF1_GEN_REG) & 0xfffffffe);
	      }
#endif
    }
#endif

    if(debug&0x4){
        printk("[Video2] %s vf=%x\n", __func__, vf);    
    }

    if ((vf->width == 0) && (vf->height == 0)) {
        amlog_level(LOG_LEVEL_ERROR, "Video: invalid frame dimension\n");
        return;
    }
    if ((cur_dispbuf) && (cur_dispbuf != &vf_local) && (cur_dispbuf != vf)
    &&(video_property_changed != 2)) {
        video_vf_put(cur_dispbuf);

    } else {
        first_picture = 1;
    }

    if (video_property_changed) {
        video_property_changed = false;
        first_picture = 1;
    }

    /* switch buffer */
    post_canvas = vf->canvas0Addr;
    canvas_copy(vf->canvas0Addr & 0xff, disp_canvas_index[0]);
    canvas_copy((vf->canvas0Addr >> 8) & 0xff, disp_canvas_index[1]);
    canvas_copy((vf->canvas0Addr >> 16) & 0xff, disp_canvas_index[2]);
    canvas_copy(vf->canvas1Addr & 0xff, disp_canvas_index[3]);
    canvas_copy((vf->canvas1Addr >> 8) & 0xff, disp_canvas_index[4]);
    canvas_copy((vf->canvas1Addr >> 16) & 0xff, disp_canvas_index[5]);

    WRITE_MPEG_REG(VIU2_VD1_IF0_CANVAS0, disp_canvas[0]);
    WRITE_MPEG_REG(VIU2_VD1_IF0_CANVAS1, disp_canvas[1]);
    //WRITE_MPEG_REG(VD2_IF0_CANVAS0, disp_canvas[1]);
    //WRITE_MPEG_REG(VD2_IF0_CANVAS1, disp_canvas[1]);

    /* set video PTS */
    if(clone == 0){
        if (cur_dispbuf != vf) {
            if (vf->pts != 0) {
                amlog_mask(LOG_MASK_TIMESTAMP,
                           "vpts to vf->pts: 0x%x, scr: 0x%x, abs_scr: 0x%x\n",
                           vf->pts, timestamp_pcrscr_get(), READ_MPEG_REG(SCR_HIU));
    
                timestamp_vpts_set(vf->pts);
            } else if (cur_dispbuf) {
                amlog_mask(LOG_MASK_TIMESTAMP,
                           "vpts inc: 0x%x, scr: 0x%x, abs_scr: 0x%x\n",
                           timestamp_vpts_get() + DUR2PTS(cur_dispbuf->duration),
                           timestamp_pcrscr_get(), READ_MPEG_REG(SCR_HIU));
    
                timestamp_vpts_inc(DUR2PTS(cur_dispbuf->duration));
    
                vpts_remainder += DUR2PTS_RM(cur_dispbuf->duration);
                if (vpts_remainder >= 0xf) {
                    vpts_remainder -= 0xf;
                    timestamp_vpts_inc(-1);
                }
            }
    
            vf->type_backup = vf->type;
        }
    }
    /* enable new config on the new frames */
    if ((first_picture) ||
        (cur_dispbuf->bufWidth != vf->bufWidth) ||
        (cur_dispbuf->width != vf->width) ||
        (cur_dispbuf->height != vf->height) ||
        (cur_dispbuf->ratio_control != vf->ratio_control) ||
        ((cur_dispbuf->type_backup & VIDTYPE_INTERLACE) !=
         (vf->type_backup & VIDTYPE_INTERLACE))) {
        amlog_mask(LOG_MASK_FRAMEINFO,
                   "%s %dx%d ar=0x%x\n",
                   ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) ?
                   "interlace-top" :
                   ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_BOTTOM) ?
                   "interlace-bottom" :
                   "progressive",
                   vf->width,
                   vf->height,
                   vf->ratio_control);

        next_frame_par = (&frame_parms[0] == next_frame_par) ?
                         &frame_parms[1] : &frame_parms[0];

#ifdef CONFIG_AM_DEINTERLACE
        if ((deinterlace_mode != 0)
            && (vf->type & VIDTYPE_INTERLACE)
#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
            && (vf->width <= 720)
#endif
           ) {
            vf->type &= ~VIDTYPE_TYPEMASK;

            if (deinterlace_mode == 1) {
                vf->type |= VIDTYPE_VIU_FIELD;
                inc_field_counter();
            }
        }
#endif

        vpp2_set_filters(wide_setting, vf, next_frame_par, vinfo);

        /* apply new vpp settings */
        frame_par_ready_to_set = 1;
    } else {
#ifdef CONFIG_AM_DEINTERLACE
        if ((deinterlace_mode != 0)
            && (vf->type & VIDTYPE_INTERLACE)
#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
            && (vf->width <= 720)
#endif
           ) {
            vf->type &= ~VIDTYPE_TYPEMASK;

            if (deinterlace_mode == 1) {
                vf->type |= VIDTYPE_VIU_FIELD;
                inc_field_counter();
            }
        }
#endif
    }

    cur_dispbuf = vf;

    if ((vf->type & VIDTYPE_NO_VIDEO_ENABLE) == 0) {
        if (disable_video == VIDEO_DISABLE_FORNEXT) {
            EnableVideoLayer();
            disable_video = VIDEO_DISABLE_NONE;
        }
        if (first_picture && (disable_video != VIDEO_DISABLE_NORMAL)) {
            EnableVideoLayer();
        
            if (cur_dispbuf->type & VIDTYPE_MVC)
                EnableVideoLayer2();
        }
    }

    if (first_picture) {
        frame_par_ready_to_set = 1;

#ifdef CONFIG_AM_DEINTERLACE
        if (deinterlace_mode != 2) {
            disable_deinterlace();
        } else {
            disable_post_deinterlace();
        }
#endif
    }
}

static int vsync_hold_line = 17;
static void viu_set_dcu(vpp_frame_par_t *frame_par, vframe_t *vf)
{
    u32 r;
    u32 vphase, vini_phase;
    u32 pat, loop;
    static const u32 vpat[] = {0, 0x8, 0x9, 0xa, 0xb, 0xc};

    r = (3 << VDIF_URGENT_BIT) |
        (vsync_hold_line << VDIF_HOLD_LINES_BIT) |
        VDIF_FORMAT_SPLIT  |
        VDIF_CHRO_RPT_LAST |
        VDIF_ENABLE |
        VDIF_RESET_ON_GO_FIELD;

    if ((vf->type & VIDTYPE_VIU_SINGLE_PLANE) == 0) {
        r |= VDIF_SEPARATE_EN;
    } else {
        if (vf->type & VIDTYPE_VIU_422) {
            r |= VDIF_FORMAT_422;
        } else {
            r |= VDIF_FORMAT_RGB888_YUV444 | VDIF_DEMUX_MODE_RGB_444;
        }
    }

    WRITE_MPEG_REG(VIU2_VD1_IF0_GEN_REG, r);
    //WRITE_MPEG_REG(VD2_IF0_GEN_REG, r);

    /* chroma formatter */
    if (vf->type & VIDTYPE_VIU_444) {
        WRITE_MPEG_REG(VIU2_VD1_FMT_CTRL, HFORMATTER_YC_RATIO_1_1);
        //WRITE_MPEG_REG(VIU_VD2_FMT_CTRL, HFORMATTER_YC_RATIO_1_1);
    } else if (vf->type & VIDTYPE_VIU_FIELD) {
        vini_phase = 0xc << VFORMATTER_INIPHASE_BIT;
        vphase = ((vf->type & VIDTYPE_VIU_422) ? 0x10 : 0x08) << VFORMATTER_PHASE_BIT;

        WRITE_MPEG_REG(VIU2_VD1_FMT_CTRL,
                       HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
                       VFORMATTER_RPTLINE0_EN | vini_phase | vphase | VFORMATTER_EN);

        //WRITE_MPEG_REG(VIU_VD2_FMT_CTRL,
        //               HFORMATTER_YC_RATIO_2_1 | HFORMATTER_EN |
        //               VFORMATTER_RPTLINE0_EN | vini_phase | vphase | VFORMATTER_EN);
    } else if (vf->type & VIDTYPE_MVC) {
        WRITE_MPEG_REG(VIU2_VD1_FMT_CTRL,
                       HFORMATTER_YC_RATIO_2_1 |
                       HFORMATTER_EN |
                       VFORMATTER_RPTLINE0_EN |
                       (0xe << VFORMATTER_INIPHASE_BIT) |
                       (((vf->type & VIDTYPE_VIU_422) ? 0x10 : 0x08) << VFORMATTER_PHASE_BIT) |
                       VFORMATTER_EN);
        /*WRITE_MPEG_REG(VIU_VD2_FMT_CTRL,
                       HFORMATTER_YC_RATIO_2_1 |
                       HFORMATTER_EN |
                       VFORMATTER_RPTLINE0_EN |
                       (0xa << VFORMATTER_INIPHASE_BIT) |
                       (((vf->type & VIDTYPE_VIU_422) ? 0x10 : 0x08) << VFORMATTER_PHASE_BIT) |
                       VFORMATTER_EN);*/
    } else if ((vf->type & VIDTYPE_INTERLACE) &&
               (((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP))) {
        WRITE_MPEG_REG(VIU2_VD1_FMT_CTRL,
                       HFORMATTER_YC_RATIO_2_1 |
                       HFORMATTER_EN |
                       VFORMATTER_RPTLINE0_EN |
                       (0xe << VFORMATTER_INIPHASE_BIT) |
                       (((vf->type & VIDTYPE_VIU_422) ? 0x10 : 0x08) << VFORMATTER_PHASE_BIT) |
                       VFORMATTER_EN);

        /*WRITE_MPEG_REG(VIU_VD2_FMT_CTRL,
                       HFORMATTER_YC_RATIO_2_1 |
                       HFORMATTER_EN |
                       VFORMATTER_RPTLINE0_EN |
                       (0xe << VFORMATTER_INIPHASE_BIT) |
                       (((vf->type & VIDTYPE_VIU_422) ? 0x10 : 0x08) << VFORMATTER_PHASE_BIT) |
                       VFORMATTER_EN);*/
    } else {
        WRITE_MPEG_REG(VIU2_VD1_FMT_CTRL,
                       HFORMATTER_YC_RATIO_2_1 |
                       HFORMATTER_EN |
                       VFORMATTER_RPTLINE0_EN |
                       (0xa << VFORMATTER_INIPHASE_BIT) |
                       (((vf->type & VIDTYPE_VIU_422) ? 0x10 : 0x08) << VFORMATTER_PHASE_BIT) |
                       VFORMATTER_EN);

        /*WRITE_MPEG_REG(VIU_VD2_FMT_CTRL,
                       HFORMATTER_YC_RATIO_2_1 |
                       HFORMATTER_EN |
                       VFORMATTER_RPTLINE0_EN |
                       (0xa << VFORMATTER_INIPHASE_BIT) |
                       (((vf->type & VIDTYPE_VIU_422) ? 0x10 : 0x08) << VFORMATTER_PHASE_BIT) |
                       VFORMATTER_EN);*/
    }

    /* LOOP/SKIP pattern */
    pat = vpat[frame_par->vscale_skip_count];

    if (vf->type & VIDTYPE_VIU_FIELD) {
        loop = 0;

        if (vf->type & VIDTYPE_INTERLACE) {
            pat = vpat[frame_par->vscale_skip_count - 1];
        }
    } else if (vf->type & VIDTYPE_MVC) {
        loop = 0x11;
        pat = 0x80;
    } else if ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
        loop = 0x11;
        pat <<= 4;
    } else {
        loop = 0;
    }

    WRITE_MPEG_REG(VIU2_VD1_IF0_RPT_LOOP,
                   (loop << VDIF_CHROMA_LOOP1_BIT) |
                   (loop << VDIF_LUMA_LOOP1_BIT)   |
                   (loop << VDIF_CHROMA_LOOP0_BIT) |
                   (loop << VDIF_LUMA_LOOP0_BIT));

    /*WRITE_MPEG_REG(VD2_IF0_RPT_LOOP,
                   (loop << VDIF_CHROMA_LOOP1_BIT) |
                   (loop << VDIF_LUMA_LOOP1_BIT)   |
                   (loop << VDIF_CHROMA_LOOP0_BIT) |
                   (loop << VDIF_LUMA_LOOP0_BIT));*/

    WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA0_RPT_PAT,   pat);
    WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA0_RPT_PAT, pat);
    WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA1_RPT_PAT,   pat);
    WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA1_RPT_PAT, pat);

    if (vf->type & VIDTYPE_MVC)
        pat = 0x88;
    /*
    WRITE_MPEG_REG(VD2_IF0_LUMA0_RPT_PAT,   pat);
    WRITE_MPEG_REG(VD2_IF0_CHROMA0_RPT_PAT, pat);
    WRITE_MPEG_REG(VD2_IF0_LUMA1_RPT_PAT,   pat);
    WRITE_MPEG_REG(VD2_IF0_CHROMA1_RPT_PAT, pat);
    */
    /* picture 0/1 control */
    if (((vf->type & VIDTYPE_INTERLACE) == 0) &&
        ((vf->type & VIDTYPE_VIU_FIELD) == 0) &&
        ((vf->type & VIDTYPE_MVC) == 0)) {
        /* progressive frame in two pictures */
        WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA_PSEL,
                       (2 << 26) |    /* two pic mode */
                       (2 << 24) |    /* use own last line */
                       (2 << 8)  |    /* toggle pic 0 and 1, use pic0 first */
                       (0x01));       /* loop pattern */
        WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA_PSEL,
                       (2 << 26) |    /* two pic mode */
                       (2 << 24) |    /* use own last line */
                       (2 << 8)  |    /* toggle pic 0 and 1, use pic0 first */
                       (0x01));       /* loop pattern */
    } else {
        WRITE_MPEG_REG(VIU2_VD1_IF0_LUMA_PSEL, 0);
        WRITE_MPEG_REG(VIU2_VD1_IF0_CHROMA_PSEL, 0);
        //WRITE_MPEG_REG(VD2_IF0_LUMA_PSEL, 0);
        //WRITE_MPEG_REG(VD2_IF0_CHROMA_PSEL, 0);
    }
}

static int detect_vout_type(void)
{
#if defined(CONFIG_AM_TCON_OUTPUT)
    return VOUT_TYPE_PROG;
#else
    int vout_type;
    int encp_enable = READ_MPEG_REG(ENCP_VIDEO_EN) & 1;

    if (encp_enable) {
        if (READ_MPEG_REG(ENCP_VIDEO_MODE) & (1 << 12)) {
            /* 1080I */
            if (READ_MPEG_REG(VENC_ENCP_LINE) < 562) {
                vout_type = VOUT_TYPE_TOP_FIELD;

            } else {
                vout_type = VOUT_TYPE_BOT_FIELD;
            }

        } else {
            vout_type = VOUT_TYPE_PROG;
        }

    } else {
        vout_type = (READ_MPEG_REG(VENC_STATA) & 1) ?
                    VOUT_TYPE_BOT_FIELD : VOUT_TYPE_TOP_FIELD;
    }

    return vout_type;
#endif
}

#ifdef INTERLACE_FIELD_MATCH_PROCESS
static inline bool interlace_field_type_match(int vout_type, vframe_t *vf)
{
    if (DUR2PTS(vf->duration) != vsync_pts_inc) {
        return false;
    }

    if ((vout_type == VOUT_TYPE_TOP_FIELD) &&
        ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)) {
        return true;
    } else if ((vout_type == VOUT_TYPE_BOT_FIELD) &&
               ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_BOTTOM)) {
        return true;
    }

    return false;
}
#endif

static int calc_hold_line(void)
{
    if ((READ_MPEG_REG(ENCI_VIDEO_EN) & 1) == 0) {
        return READ_MPEG_REG(ENCP_VIDEO_VAVON_BLINE) >> 1;
    } else {
        return READ_MPEG_REG(VFIFO2VD_LINE_TOP_START) >> 1;
    }
}

#ifdef SLOW_SYNC_REPEAT
/* add a new function to check if current display frame has been
displayed for its duration */
static inline bool duration_expire(vframe_t *cur_vf, vframe_t *next_vf, u32 dur)
{
    u32 pts;
    s32 dur_disp;
    static s32 rpt_tab_idx = 0;
    static const u32 rpt_tab[4] = {0x100, 0x100, 0x300, 0x300};

    if ((cur_vf == NULL) || (cur_dispbuf == &vf_local)) {
        return true;
    }

    pts = next_vf->pts;
    if (pts == 0) {
        dur_disp = DUR2PTS(cur_vf->duration);
    } else {
        dur_disp = pts - timestamp_vpts_get();
    }

    if ((dur << 8) >= (dur_disp * rpt_tab[rpt_tab_idx & 3])) {
        rpt_tab_idx = (rpt_tab_idx + 1) & 3;
        return true;
    } else {
        return false;
    }
}
#endif

static inline bool vpts_expire(vframe_t *cur_vf, vframe_t *next_vf)
{
    u32 pts = next_vf->pts;
    u32 systime;

     if ((cur_vf == NULL) || (cur_dispbuf == &vf_local)) {
        return true;
    }
    if ((trickmode_i == 1) || ((trickmode_fffb == 1))) {
        if (0 == atomic_read(&trickmode_framedone)) {
            return true;
        } else {
            return false;
        }
    }

    if (next_vf->duration == 0) {
        return true;
    }

    systime = timestamp_pcrscr_get();

    if ((pts == 0) && (cur_dispbuf != &vf_local)) {
        pts = timestamp_vpts_get() + (cur_vf ? DUR2PTS(cur_vf->duration) : 0);
    }
    /* check video PTS discontinuity */
    else if (abs(systime - pts) > tsync_vpts_discontinuity_margin()) {
        pts = timestamp_vpts_get() + (cur_vf ? DUR2PTS(cur_vf->duration) : 0);

        if ((systime - pts) >= 0) {
            tsync_avevent_locked(VIDEO_TSTAMP_DISCONTINUITY, next_vf->pts);
			printk("video discontinue, system=0x%x vpts=0x%x\n", systime, pts);
            return true;
        }
    }

    return ((int)(timestamp_pcrscr_get() - pts) >= 0);
}

#if defined(CONFIG_CLK81_DFS)
extern int check_and_set_clk81(void);
#endif

static void vsync_notify(void)
{
    if (video_notify_flag & VIDEO_NOTIFY_TRICK_WAIT) {
        wake_up_interruptible(&amvideo_trick_wait);
        video_notify_flag &= ~VIDEO_NOTIFY_TRICK_WAIT;
    }
    if (video_notify_flag & VIDEO_NOTIFY_FRAME_WAIT) {
        video_notify_flag &= ~VIDEO_NOTIFY_FRAME_WAIT;
        vf_notify_provider(RECEIVER_NAME, VFRAME_EVENT_RECEIVER_FRAME_WAIT, NULL); 
    }
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
    if (video_notify_flag & VIDEO_NOTIFY_POS_CHANGED) {
        video_notify_flag &= ~VIDEO_NOTIFY_POS_CHANGED;
        vf_notify_provider(RECEIVER_NAME, VFRAME_EVENT_RECEIVER_POS_CHANGED, NULL); 
    }
#endif
    if (video_notify_flag & (VIDEO_NOTIFY_PROVIDER_GET | VIDEO_NOTIFY_PROVIDER_PUT)) {
            int event = 0;

            if (video_notify_flag & VIDEO_NOTIFY_PROVIDER_GET)
                event |= VFRAME_EVENT_RECEIVER_GET;
            if (video_notify_flag & VIDEO_NOTIFY_PROVIDER_PUT)
                event |= VFRAME_EVENT_RECEIVER_PUT;

        vf_notify_provider(RECEIVER_NAME, event, NULL); 

        video_notify_flag &= ~(VIDEO_NOTIFY_PROVIDER_GET | VIDEO_NOTIFY_PROVIDER_PUT);
    }
    
#if defined(CONFIG_CLK81_DFS)   
	check_and_set_clk81();
#endif
}

#ifdef FIQ_VSYNC
static irqreturn_t vsync_bridge_isr(int irq, void *dev_id)
{
    vsync_notify();

    return IRQ_HANDLED;
}
#endif

#if 1
#define INTERVAL_BUF_SIZE 1200
static unsigned int last_isr_enter_time = 0;
//static unsigned int timerb_interval_buf[INTERVAL_BUF_SIZE];
//static int timerb_interval_wr_pos = 0;
static unsigned int isr_interval_max = 0;
static unsigned int isr_run_time_max = 0;
#endif

#ifdef FIQ_VSYNC
void vsync_fisr2(void)
#else
static irqreturn_t vsync_isr(int irq, void *dev_id)
#endif
{
    int hold_line;
#ifdef CONFIG_AM_DEINTERLACE
    int deinterlace_mode;
#endif
    s32 i, vout_type;
    vframe_t *vf;
#ifdef CONFIG_AM_VIDEO_LOG
    int toggle_cnt;
#endif
#if 1
        unsigned int cur_timerb_value;
        unsigned int interval;
#endif
#ifdef CONFIG_AM_DEINTERLACE
    deinterlace_mode = get_deinterlace_mode();
#endif

#ifdef CONFIG_AM_VIDEO_LOG
    toggle_cnt = 0;
#endif
        if(clone_frame_rate_delay!=0){
            clone_frame_rate_delay--;
            if(clone_frame_rate_delay==0){
                clone_frame_rate = clone_frame_rate_set_value;
            }
        }

#if 1
        if(READ_MPEG_REG(ISA_TIMERB)==0){
            WRITE_MPEG_REG(ISA_TIMER_MUX, (READ_MPEG_REG(ISA_TIMER_MUX)&(~(3<<TIMER_B_INPUT_BIT)))
                |(TIMER_UNIT_1us<<TIMER_B_INPUT_BIT)|(1<<13)|(1<<17));
            WRITE_MPEG_REG(ISA_TIMERB, 0xffff);
            printk("Deinterlace: Init 100us TimerB\n");
        }
        cur_timerb_value = (unsigned int)(0x10000-(READ_MPEG_REG(ISA_TIMERB)>>16));
        if(cur_timerb_value > last_isr_enter_time){
            interval = cur_timerb_value-last_isr_enter_time;
        }
        else{
            interval = cur_timerb_value + 0x10000 - last_isr_enter_time;
        }
        last_isr_enter_time = cur_timerb_value;
        if(interval > isr_interval_max){
            isr_interval_max = interval;       
        } 
        
    if(debug&0x1){
        printk("[Video2] Enter %s %u %u\n", __func__, interval, cur_timerb_value);
    }
#endif

    if(debug&0x100){
        do{
            vf=vf_get(RECEIVER_NAME);        
            if(vf){
                vf_put(vf, RECEIVER_NAME);        
            }
        }while(vf);
    }
    frame_count ++;
    timer_count ++;	
    vout_type = detect_vout_type();
    hold_line = calc_hold_line();

    if(clone == 0){
      timestamp_pcrscr_inc(vsync_pts_inc);
	    timestamp_apts_inc(vsync_pts_inc);
	  }
#ifdef SLOW_SYNC_REPEAT
    frame_repeat_count++;
#endif

    if ((!cur_dispbuf) || (cur_dispbuf == &vf_local)) {

        vf = video_vf_peek();

        if (vf) {
            if(clone == 0){
                tsync_avevent_locked(VIDEO_START,
                          (vf->pts) ? vf->pts : timestamp_vpts_get());
            }
#ifdef SLOW_SYNC_REPEAT
            frame_repeat_count = 0;
#endif

        } else if ((cur_dispbuf == &vf_local) && (video_property_changed)) {
            if (!blackout) {
#ifdef CONFIG_AM_DEINTERLACE
                if ((deinterlace_mode == 0) || (cur_dispbuf->duration == 0)
#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
                    || (cur_dispbuf->width > 720)
#endif
                   )
#endif
                {
                    /* setting video display property in unregister mode */
                    u32 cur_index = cur_dispbuf->canvas0Addr;
                    canvas_update_addr(cur_index & 0xff, (u32)keep_y_addr);
                    canvas_update_addr((cur_index >> 8) & 0xff, (u32)keep_u_addr);
                    canvas_update_addr((cur_index >> 16) & 0xff, (u32)keep_v_addr);
                }

                vsync_toggle_frame(cur_dispbuf);
            } else {
                video_property_changed = false;
            }
        } else {
            goto exit;
        }
    }

    /* buffer switch management */
    vf = video_vf_peek();
    
    if(debug&0x2){
        printk("[Video2] %s vf_peek=>%x\n", __func__, vf);    
    }
    /* setting video display property in underflow mode */
    if ((!vf) && cur_dispbuf && (video_property_changed)) {
        vsync_toggle_frame(cur_dispbuf);
    }

    while (vf) {
        if(clone == 1){
            if(throw_frame){
                vf = video_vf_get();
                video_vf_put(vf);
            }
            else if(clone_vpts_remainder < vsync_pts_inc){
                vf = video_vf_get();
#ifdef CONFIG_TVIN_VIUIN
                if(clone_frame_scale_width != 0){
                    vdin0_set_hscale(
                        vf->width, //int src_w, 
                        clone_frame_scale_width, //int dst_w, 
                        1, //int hsc_en, 
                        0, //int prehsc_en, 
                        4, //int hsc_bank_length,
                        1, //int hsc_rpt_p0_num,
                        4, //int hsc_ini_rcv_num,
                        0, //int hsc_ini_phase,
                        1  //int short_lineo_en
                    ); 
                    vf->width = clone_frame_scale_width;
                }
#endif                
                vsync_toggle_frame(vf);
                //clone_vpts_remainder += DUR2PTS(vf->duration);
                if(clone_frame_rate_force){
                    clone_vpts_remainder += (90000/clone_frame_rate_force);
                }
                else{
                    clone_vpts_remainder += (90000/clone_frame_rate);
                }
            }
            else{
                clone_vpts_remainder -= vsync_pts_inc;
            }
            break;
        }
        else if (vpts_expire(cur_dispbuf, vf)
#ifdef INTERLACE_FIELD_MATCH_PROCESS
            || interlace_field_type_match(vout_type, vf)
#endif
           ) {
            amlog_mask(LOG_MASK_TIMESTAMP,
                       "VIDEO_PTS = 0x%x, cur_dur=0x%x, next_pts=0x%x, scr = 0x%x\n",
                       timestamp_vpts_get(),
                       (cur_dispbuf) ? cur_dispbuf->duration : 0,
                       vf->pts,
                       timestamp_pcrscr_get());

            amlog_mask_if(toggle_cnt > 0, LOG_MASK_FRAMESKIP, "skipped\n");

            vf = video_vf_get();

            vsync_toggle_frame(vf);

            if (trickmode_fffb == 1) {
                atomic_set(&trickmode_framedone, 1);
                video_notify_flag |= VIDEO_NOTIFY_TRICK_WAIT;
                break;
            }

#ifdef SLOW_SYNC_REPEAT
            frame_repeat_count = 0;
#endif
            vf = video_vf_peek();
            
            if(debug&0x10000){
                return;
            }
        } else {
#ifdef SLOW_SYNC_REPEAT
            /* check if current frame's duration has expired, in this example
             * it compares current frame display duration with 1/1/1/1.5 frame duration
             * every 4 frames there will be one frame play longer than usual.
             * you can adjust this array for any slow sync control as you want.
             * The playback can be smoother than previous method.
             */
            if (duration_expire(cur_dispbuf, vf, frame_repeat_count * vsync_pts_inc) && timestamp_pcrscr_enable_state()) {
                amlog_mask(LOG_MASK_SLOWSYNC,
                           "slow sync toggle, frame_repeat_count = %d\n",
                           frame_repeat_count);
                amlog_mask(LOG_MASK_SLOWSYNC,
                           "system time = 0x%x, video time = 0x%x\n",
                           timestamp_pcrscr_get(), timestamp_vpts_get());
                vf = video_vf_get();
                vsync_toggle_frame(vf);
                frame_repeat_count = 0;

                vf = video_vf_peek();
            } else
#endif
                /* setting video display property in pause mode */
                if (video_property_changed && cur_dispbuf) {
                    if (blackout) {
                        if (cur_dispbuf != &vf_local) {
                            vsync_toggle_frame(cur_dispbuf);
                        }
                    } else {
                        vsync_toggle_frame(cur_dispbuf);
                    }
                }

            break;
        }
#ifdef CONFIG_AM_VIDEO_LOG
        toggle_cnt++;
#endif
    }

    /* filter setting management */
    if ((frame_par_ready_to_set) || (frame_par_force_to_set)) {
        cur_frame_par = next_frame_par;
    }

    if (cur_dispbuf) {
        f2v_vphase_t *vphase;
        u32 vin_type = cur_dispbuf->type & VIDTYPE_TYPEMASK;

#ifdef CONFIG_AM_DEINTERLACE
        if ((deinterlace_mode == 0) || (cur_dispbuf->duration == 0)
#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
            || (cur_dispbuf->width > 720)
#endif
           )
#endif
        {
            viu_set_dcu(cur_frame_par, cur_dispbuf);
        }

        /* vertical phase */
        vphase = &cur_frame_par->VPP_vf_ini_phase_[vpp_phase_table[vin_type][vout_type]];
        WRITE_MPEG_REG(VPP2_VSC_INI_PHASE, ((u32)(vphase->phase) << 8));

        if (vphase->repeat_skip >= 0) {
            /* skip lines */
            WRITE_MPEG_REG_BITS(VPP2_VSC_PHASE_CTRL,
                                skip_tab[vphase->repeat_skip],
                                VPP_PHASECTL_INIRCVNUMT_BIT,
                                VPP_PHASECTL_INIRCVNUM_WID +
                                VPP_PHASECTL_INIRPTNUM_WID);

        } else {
            /* repeat first line */
            WRITE_MPEG_REG_BITS(VPP2_VSC_PHASE_CTRL, 4,
                                VPP_PHASECTL_INIRCVNUMT_BIT,
                                VPP_PHASECTL_INIRCVNUM_WID);
            WRITE_MPEG_REG_BITS(VPP2_VSC_PHASE_CTRL,
                                1 - vphase->repeat_skip,
                                VPP_PHASECTL_INIRPTNUMT_BIT,
                                VPP_PHASECTL_INIRPTNUM_WID);
        }
    }

    if (((frame_par_ready_to_set) || (frame_par_force_to_set)) &&
        (cur_frame_par)) {
        vppfilter_mode_t *vpp_filter = &cur_frame_par->vpp_filter;

        if (cur_dispbuf) {
            u32 zoom_start_y, zoom_end_y;

            if (cur_dispbuf->type & VIDTYPE_INTERLACE) {
                if (cur_dispbuf->type & VIDTYPE_VIU_FIELD) {
                    zoom_start_y = cur_frame_par->VPP_vd_start_lines_ >> 1;
                    zoom_end_y = (cur_frame_par->VPP_vd_end_lines_ + 1) >> 1;
                } else {
                    zoom_start_y = cur_frame_par->VPP_vd_start_lines_;
                    zoom_end_y = cur_frame_par->VPP_vd_end_lines_;
                }
            } else {
                if (cur_dispbuf->type & VIDTYPE_VIU_FIELD) {
                    zoom_start_y = cur_frame_par->VPP_vd_start_lines_;
                    zoom_end_y = cur_frame_par->VPP_vd_end_lines_;
                } else {
                    zoom_start_y = cur_frame_par->VPP_vd_start_lines_ >> 1;
                    zoom_end_y = (cur_frame_par->VPP_vd_end_lines_ + 1) >> 1;
                }
            }

            zoom_start_x_lines = cur_frame_par->VPP_hd_start_lines_;
            zoom_end_x_lines   = cur_frame_par->VPP_hd_end_lines_;
            zoom_display_horz();

            zoom_start_y_lines = zoom_start_y;
            zoom_end_y_lines   = zoom_end_y;
            zoom_display_vert();
        }

        /* vpp filters */
        SET_MPEG_REG_MASK(VPP2_SC_MISC,
                          VPP_SC_TOP_EN | VPP_SC_VERT_EN | VPP_SC_HORZ_EN);

        /* horitontal filter settings */
        WRITE_MPEG_REG_BITS(VPP2_SC_MISC,
                            vpp_filter->vpp_horz_coeff[0],
                            VPP_SC_HBANK_LENGTH_BIT,
                            VPP_SC_BANK_LENGTH_WID);

        if (vpp_filter->vpp_horz_coeff[1] & 0x8000) {
            WRITE_MPEG_REG(VPP2_SCALE_COEF_IDX, VPP_COEF_HORZ | VPP_COEF_9BIT);
        } else {
            WRITE_MPEG_REG(VPP2_SCALE_COEF_IDX, VPP_COEF_HORZ);
        }

        for (i = 0; i < (vpp_filter->vpp_horz_coeff[1] & 0xff); i++) {
            WRITE_MPEG_REG(VPP2_SCALE_COEF, vpp_filter->vpp_horz_coeff[i + 2]);
        }

        /* vertical filter settings */
        WRITE_MPEG_REG_BITS(VPP2_SC_MISC,
                            vpp_filter->vpp_vert_coeff[0],
                            VPP_SC_VBANK_LENGTH_BIT,
                            VPP_SC_BANK_LENGTH_WID);

        WRITE_MPEG_REG(VPP2_SCALE_COEF_IDX, VPP_COEF_VERT);
        for (i = 0; i < vpp_filter->vpp_vert_coeff[1]; i++) {
            WRITE_MPEG_REG(VPP2_SCALE_COEF,
                           vpp_filter->vpp_vert_coeff[i + 2]);
        }

        WRITE_MPEG_REG(VPP2_PIC_IN_HEIGHT,
                       cur_frame_par->VPP_pic_in_height_);

        WRITE_MPEG_REG_BITS(VPP2_HSC_PHASE_CTRL,
                            cur_frame_par->VPP_hf_ini_phase_,
                            VPP_HSC_TOP_INI_PHASE_BIT,
                            VPP_HSC_TOP_INI_PHASE_WID);
        WRITE_MPEG_REG(VPP2_POSTBLEND_VD1_H_START_END,
                       ((cur_frame_par->VPP_post_blend_vd_h_start_ & VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
                       ((cur_frame_par->VPP_post_blend_vd_h_end_   & VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
        WRITE_MPEG_REG(VPP2_POSTBLEND_VD1_V_START_END,
                       ((cur_frame_par->VPP_post_blend_vd_v_start_ & VPP_VD_SIZE_MASK) << VPP_VD1_START_BIT) |
                       ((cur_frame_par->VPP_post_blend_vd_v_end_   & VPP_VD_SIZE_MASK) << VPP_VD1_END_BIT));
        WRITE_MPEG_REG(VPP2_POSTBLEND_H_SIZE, cur_frame_par->VPP_post_blend_h_size_);

        vpp_settings_h(cur_frame_par);
        vpp_settings_v(cur_frame_par);

        frame_par_ready_to_set = 0;
        frame_par_force_to_set = 0;
    } /* VPP one time settings */

    wait_sync = 0;

#ifdef CONFIG_AM_DEINTERLACE
    if ((deinterlace_mode != 0) && cur_dispbuf && (cur_dispbuf->duration > 0)
#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
        && (cur_dispbuf->width <= 720)
#endif
       ) {
        run_deinterlace(zoom_start_x_lines, zoom_end_x_lines, zoom_start_y_lines, zoom_end_y_lines, cur_dispbuf->type_backup, cur_dispbuf->blend_mode, hold_line);
    }
    else
#endif
    if(cur_dispbuf && cur_dispbuf->process_fun && (cur_dispbuf->duration > 0)){
        /* for new deinterlace driver */
        cur_dispbuf->process_fun(cur_dispbuf->private_data, zoom_start_x_lines, zoom_end_x_lines, zoom_start_y_lines, zoom_end_y_lines);
    }

exit:
    if(timer_count > 50){
        timer_count = 0 ;
        video_notify_flag |= VIDEO_NOTIFY_FRAME_WAIT;	
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
        if((video_scaler_mode)&&(scaler_pos_changed)){
            video_notify_flag |= VIDEO_NOTIFY_POS_CHANGED;
            scaler_pos_changed = 0;
        }else{
            scaler_pos_changed = 0;
            video_notify_flag &= ~VIDEO_NOTIFY_POS_CHANGED;
        }
#endif        	
    }
#ifdef FIQ_VSYNC
    if (video_notify_flag)
        fiq_bridge_pulse_trigger(&vsync_fiq_bridge);

#if 1
        cur_timerb_value = (unsigned int)(0x10000-(READ_MPEG_REG(ISA_TIMERB)>>16));
        if(cur_timerb_value >= last_isr_enter_time){
            interval = cur_timerb_value-last_isr_enter_time;
        }
        else{
            interval = cur_timerb_value + 0x10000 - last_isr_enter_time;
        }

        if(interval > isr_run_time_max){
            isr_run_time_max = interval;       
        } 

    if(debug&0x1){
        printk("[Video2] Leave %s %u %u\n", __func__, interval, cur_timerb_value);
    }
#endif    

#else
    if (video_notify_flag)
        vsync_notify();

    return IRQ_HANDLED;
#endif

}

static int alloc_keep_buffer(void)
{
    amlog_mask(LOG_MASK_KEEPBUF, "alloc_keep_buffer\n");
    keep_y_addr = __get_free_pages(GFP_KERNEL, get_order(Y_BUFFER_SIZE));
    if (!keep_y_addr) {
        amlog_mask(LOG_MASK_KEEPBUF, "%s: failed to alloc y addr\n", __FUNCTION__);
        goto err1;
    }

    keep_y_addr_remap = ioremap_nocache(virt_to_phys((u8 *)keep_y_addr), Y_BUFFER_SIZE);
    if (!keep_y_addr_remap) {
        amlog_mask(LOG_MASK_KEEPBUF, "%s: failed to remap y addr\n", __FUNCTION__);
        goto err2;
    }

    keep_u_addr = __get_free_pages(GFP_KERNEL, get_order(U_BUFFER_SIZE));
    if (!keep_u_addr) {
        amlog_mask(LOG_MASK_KEEPBUF, "%s: failed to alloc u addr\n", __FUNCTION__);
        goto err3;
    }

    keep_u_addr_remap = ioremap_nocache(virt_to_phys((u8 *)keep_u_addr), U_BUFFER_SIZE);
    if (!keep_u_addr_remap) {
        amlog_mask(LOG_MASK_KEEPBUF, "%s: failed to remap u addr\n", __FUNCTION__);
        goto err4;
    }

    keep_v_addr = __get_free_pages(GFP_KERNEL, get_order(V_BUFFER_SIZE));
    if (!keep_v_addr) {
        amlog_mask(LOG_MASK_KEEPBUF, "%s: failed to alloc v addr\n", __FUNCTION__);
        goto err5;
    }

    keep_v_addr_remap = ioremap_nocache(virt_to_phys((u8 *)keep_v_addr), U_BUFFER_SIZE);
    if (!keep_v_addr_remap) {
        amlog_mask(LOG_MASK_KEEPBUF, "%s: failed to remap v addr\n", __FUNCTION__);
        goto err6;
    }

    return 0;

err6:
    free_pages(keep_v_addr, get_order(U_BUFFER_SIZE));
    keep_v_addr = 0;
err5:
    iounmap(keep_u_addr_remap);
    keep_u_addr_remap = NULL;
err4:
    free_pages(keep_u_addr, get_order(U_BUFFER_SIZE));
    keep_u_addr = 0;
err3:
    iounmap(keep_y_addr_remap);
    keep_y_addr_remap = NULL;
err2:
    free_pages(keep_y_addr, get_order(Y_BUFFER_SIZE));
    keep_y_addr = 0;
err1:
    return -ENOMEM;
}

/*********************************************************
 * FIQ Routines
 *********************************************************/

static void vsync_fiq_up(void)
{
#ifdef  FIQ_VSYNC
    request_fiq(INT_VIU2_VSYNC, &vsync_fisr2);
#else
    int r;
    r = request_irq(INT_VIU2_VSYNC, &vsync_isr,
                    IRQF_SHARED, "vsync2",
                    (void *)video_dev_id);
#endif
}

static void vsync_fiq_down(void)
{
#ifdef FIQ_VSYNC
    free_fiq(INT_VIU2_VSYNC, &vsync_fisr2);
#else
    free_irq(INT_VIU2_VSYNC, (void *)video_dev_id);
#endif
}

/*int get_curren_frame_para(int* top ,int* left , int* bottom, int* right)
{
	if(!cur_frame_par){
		return -1;	
	}
	*top    =  cur_frame_par->VPP_vd_start_lines_ ;
	*left   =  cur_frame_par->VPP_hd_start_lines_ ;
	*bottom =  cur_frame_par->VPP_vd_end_lines_ ;
	*right  =  cur_frame_par->VPP_hd_end_lines_;
	return 	0;
}*/

static void video_vf_unreg_provider(void)
{
    ulong flags;

#ifdef CONFIG_AM_DEINTERLACE
    int deinterlace_mode = get_deinterlace_mode();
#endif

    spin_lock_irqsave(&lock, flags);

    if (cur_dispbuf) {
        vf_local = *cur_dispbuf;
        cur_dispbuf = &vf_local;
    }

    if (trickmode_fffb) {
        atomic_set(&trickmode_framedone, 0);
    }

    if (blackout) {
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
        if(video_scaler_mode)
            DisableVideoLayer_PREBELEND();
        else
            DisableVideoLayer();
#else
        DisableVideoLayer();
#endif	
    }

    //if (!trickmode_fffb)
    {
        vf_keep_current();
    }

    if(clone == 0){
        tsync_avevent(VIDEO_STOP, 0);
    }
#ifdef CONFIG_AM_DEINTERLACE
    if (deinterlace_mode == 2) {
        disable_pre_deinterlace();
    }
#endif
    spin_unlock_irqrestore(&lock, flags);
}

static void video_vf_light_unreg_provider(void)
{
    ulong flags;

    spin_lock_irqsave(&lock, flags);

    if (cur_dispbuf) {
        vf_local = *cur_dispbuf;
        cur_dispbuf = &vf_local;
    }


    spin_unlock_irqrestore(&lock, flags);
}

static int video_receiver_event_fun(int type, void* data, void* private_data)
{
    if(type == VFRAME_EVENT_PROVIDER_UNREG){
        video_vf_unreg_provider();
    }
    else if(type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG){
        video_vf_light_unreg_provider();
    }
    return 0;
}
/*unsigned int get_post_canvas(void)
{
    return post_canvas;
}*/

static int canvas_dup(ulong *dst, ulong src_paddr, ulong size)
{
    void __iomem *p = ioremap_wc(src_paddr, size);

    if (p) {
        memcpy(dst, p, size);
        iounmap(p);

        return 1;
    }

    return 0;
}
#if 0
unsigned int vf_keep_current(void)
{
    u32 cur_index;
    u32 y_index, u_index, v_index;
    canvas_t cs0,cs1,cs2,cd;


#ifdef CONFIG_AM_DEINTERLACE
    int deinterlace_mode = get_deinterlace_mode();
#endif
    if (blackout) {
        return 0;
    }

    if (0 == (READ_MPEG_REG(VPP2_MISC) & VPP_VD1_POSTBLEND)) {
        return 0;
    }

#ifdef CONFIG_AM_DEINTERLACE
    if ((deinterlace_mode != 0) && cur_dispbuf && (cur_dispbuf->duration > 0)
#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
        && (cur_dispbuf->width <= 720)
#endif
       ) {
        return 0;
    }
#endif

    if (!keep_y_addr_remap) {
        //if (alloc_keep_buffer())
        return -1;
    }

    cur_index = READ_MPEG_REG(VIU2_VD1_IF0_CANVAS0);
    y_index = cur_index & 0xff;
    u_index = (cur_index >> 8) & 0xff;
    v_index = (cur_index >> 16) & 0xff;

    if ((cur_dispbuf->type & VIDTYPE_VIU_422) == VIDTYPE_VIU_422) {
	 canvas_read(y_index,&cd);
        if (keep_y_addr != canvas_get_addr(y_index) && /*must not the same address*/
            canvas_dup(keep_y_addr_remap, canvas_get_addr(y_index), (cd.width)*(cd.height))) {
            canvas_update_addr(y_index, (u32)keep_y_addr);
        }
    } else if ((cur_dispbuf->type & VIDTYPE_VIU_444) == VIDTYPE_VIU_444) {
    	 canvas_read(y_index,&cd);
        if (keep_y_addr != canvas_get_addr(y_index) && /*must not the same address*/ 
            canvas_dup(keep_y_addr_remap, canvas_get_addr(y_index), (cd.width)*(cd.height))){
            canvas_update_addr(y_index, (u32)keep_y_addr);
        }
    } else {
        canvas_read(y_index,&cs0);
        canvas_read(u_index,&cs1);
        canvas_read(v_index,&cs2);
        
        if (keep_y_addr != canvas_get_addr(y_index) && /*must not the same address*/
            canvas_dup(keep_y_addr_remap, canvas_get_addr(y_index), (cs0.width *cs0.height)) &&
            canvas_dup(keep_u_addr_remap, canvas_get_addr(u_index), (cs1.width *cs1.height)) &&
            canvas_dup(keep_v_addr_remap, canvas_get_addr(v_index), (cs2.width *cs2.height))) {
            canvas_update_addr(y_index, (u32)keep_y_addr);
            canvas_update_addr(u_index, (u32)keep_u_addr);
            canvas_update_addr(v_index, (u32)keep_v_addr);
        }
    }

    return 0;
}

EXPORT_SYMBOL(get_post_canvas);
EXPORT_SYMBOL(vf_keep_current);
#endif
/*********************************************************
 * /dev/amvideo APIs
 *********************************************************/
static int amvideo_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int amvideo_release(struct inode *inode, struct file *file)
{
    if (blackout) {
        ///DisableVideoLayer();/*don't need it ,it have problem on  pure music playing*/
    }
    return 0;
}

static int amvideo_ioctl(struct inode *inode, struct file *file,
                         unsigned int cmd, ulong arg)
{
    int ret = 0;

    switch (cmd) {
    case AMSTREAM_IOC_TRICKMODE:
        if (arg == TRICKMODE_I) {
            trickmode_i = 1;
        } else if (arg == TRICKMODE_FFFB) {
            trickmode_fffb = 1;
        } else {
            trickmode_i = 0;
            trickmode_fffb = 0;
        }
        atomic_set(&trickmode_framedone, 0);
        tsync_trick_mode(trickmode_fffb);
        break;

    case AMSTREAM_IOC_TRICK_STAT:
        *((u32 *)arg) = atomic_read(&trickmode_framedone);
        break;

    case AMSTREAM_IOC_VPAUSE:
        tsync_avevent(VIDEO_PAUSE, arg);
        break;

    case AMSTREAM_IOC_AVTHRESH:
        tsync_set_avthresh(arg);
        break;

    case AMSTREAM_IOC_SYNCTHRESH:
        tsync_set_syncthresh(arg);
        break;

    case AMSTREAM_IOC_SYNCENABLE:
        tsync_set_enable(arg);
        break;

    //case AMSTREAM_IOC_SET_SYNCDISCON:
    //    tsync_set_syncdiscont(arg);
    //    break;

    //case AMSTREAM_IOC_GET_SYNCDISCON:
    //    *((u32 *)arg) = tsync_get_syncdiscont();
    //    break;

    case AMSTREAM_IOC_CLEAR_VBUF: {
        unsigned long flags;
        spin_lock_irqsave(&lock, flags);
        cur_dispbuf = NULL;
        spin_unlock_irqrestore(&lock, flags);
    }
    break;

    /**********************************************************************
    video enhancement ioctl
    **********************************************************************/
    case AMSTREAM_IOC_VE_DEBUG: {
        struct ve_regs_s data;
#if 0
        if (get_user((unsigned long long)data, (void __user *)arg))
#else
        if (copy_from_user(&data, (void __user *)arg, sizeof(struct ve_regs_s)))
#endif
        {
            ret = -EFAULT;
        } else {
            ve_set_regs(&data);
            if (!(data.mode)) { // read
#if 0
                if (put_user((unsigned long long)data, (void __user *)arg))
#else
                if (copy_to_user(&data, (void __user *)arg, sizeof(struct ve_regs_s)))
#endif
                {
                    ret = -EFAULT;
                }
            }
        }

        break;
    }
    case AMSTREAM_IOC_VE_BEXT: {
        struct ve_bext_s ve_bext;
        if (copy_from_user(&ve_bext, (void __user *)arg, sizeof(struct ve_bext_s))) {
            ret = -EFAULT;
            break;
        }
        ve_set_bext(&ve_bext);
        break;
    }

    case AMSTREAM_IOC_VE_DNLP: {
        struct ve_dnlp_s ve_dnlp;
        if (copy_from_user(&ve_dnlp, (void __user *)arg, sizeof(struct ve_dnlp_s))) {
            ret = -EFAULT;
            break;
        }
        ve_set_dnlp(&ve_dnlp);
        break;
    }

    case AMSTREAM_IOC_VE_HSVS: {
        struct ve_hsvs_s ve_hsvs;
        if (copy_from_user(&ve_hsvs, (void __user *)arg, sizeof(struct ve_hsvs_s))) {
            ret = -EFAULT;
            break;
        }
        ve_set_hsvs(&ve_hsvs);
        break;
    }

    case AMSTREAM_IOC_VE_CCOR: {
        struct ve_ccor_s ve_ccor;
        if (copy_from_user(&ve_ccor, (void __user *)arg, sizeof(struct ve_ccor_s))) {
            ret = -EFAULT;
            break;
        }
        ve_set_ccor(&ve_ccor);
        break;
    }

    case AMSTREAM_IOC_VE_BENH: {
        struct ve_benh_s ve_benh;
        if (copy_from_user(&ve_benh, (void __user *)arg, sizeof(struct ve_benh_s))) {
            ret = -EFAULT;
            break;
        }
        ve_set_benh(&ve_benh);
        break;
    }

    case AMSTREAM_IOC_VE_DEMO: {
        struct ve_demo_s ve_demo;
        if (copy_from_user(&ve_demo, (void __user *)arg, sizeof(struct ve_demo_s))) {
            ret = -EFAULT;
            break;
        }
        ve_set_demo(&ve_demo);
        break;
    }

    /**********************************************************************
    color management ioctl
    **********************************************************************/
    case AMSTREAM_IOC_CM_DEBUG: {
        struct cm_regs_s data;
        if (copy_from_user(&data, (void __user *)arg, sizeof(struct cm_regs_s))) {
            ret = -EFAULT;
        } else {
            cm_set_regs(&data);
            if (!(data.mode)) { // read
                if (copy_to_user(&data, (void __user *)arg, sizeof(struct cm_regs_s))) {
                    ret = -EFAULT;
                }
            }
        }
        break;
    }

    case AMSTREAM_IOC_CM_REGION: {
        struct cm_region_s cm_region;
        if (copy_from_user(&cm_region, (void __user *)arg, sizeof(struct cm_region_s))) {
            ret = -EFAULT;
            break;
        }
        cm_set_region(&cm_region);
        break;
    }

    case AMSTREAM_IOC_CM_TOP: {
        struct cm_top_s cm_top;
        if (copy_from_user(&cm_top, (void __user *)arg, sizeof(struct cm_top_s))) {
            ret = -EFAULT;
            break;
        }
        cm_set_top(&cm_top);
        break;
    }

    case AMSTREAM_IOC_CM_DEMO: {
        struct cm_demo_s cm_demo;
        if (copy_from_user(&cm_demo, (void __user *)arg, sizeof(struct cm_demo_s))) {
            ret = -EFAULT;
            break;
        }
        cm_set_demo(&cm_demo);
        break;
    }

    default:
        return -EINVAL;
    }

    return 0;
}

static unsigned int amvideo_poll(struct file *file, poll_table *wait_table)
{
    poll_wait(file, &amvideo_trick_wait, wait_table);

    if (atomic_read(&trickmode_framedone)) {
        atomic_set(&trickmode_framedone, 0);
        return POLLOUT | POLLWRNORM;
    }

    return 0;
}

const static struct file_operations amvideo_fops = {
    .owner    = THIS_MODULE,
    .open     = amvideo_open,
    .release  = amvideo_release,
    .ioctl    = amvideo_ioctl,
    .poll     = amvideo_poll,
};

/*********************************************************
 * SYSFS property functions
 *********************************************************/
#define MAX_NUMBER_PARA 10
#define AMVIDEO_CLASS_NAME "video2"

static int parse_para(const char *para, int para_num, int *result)
{
    char *endp;
    const char *startp = para;
    int *out = result;
    int len = 0, count = 0;

    if (!startp) {
        return 0;
    }

    len = strlen(startp);

    do {
        //filter space out
        while (startp && (isspace(*startp) || !isgraph(*startp)) && len) {
            startp++;
            len--;
        }

        if (len == 0) {
            break;
        }

        *out++ = simple_strtol(startp, &endp, 0);

        len -= endp - startp;
        startp = endp;
        count++;

    } while ((endp) && (count < para_num) && (len > 0));

    return count;
}

static void set_video_window(const char *para)
{
    int parsed[4];

    if (likely(parse_para(para, 4, parsed) == 4)) {
        int w, h;

        w = parsed[2] - parsed[0] + 1;
        h = parsed[3] - parsed[1] + 1;

#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
        if(video_scaler_mode){
            if ((w == 1) && (h == 1)){
                w= 0;
                h = 0;
            }
            if((content_left!=parsed[0])||(content_top!=parsed[1])||(content_w!=w)||(content_h!=h))
                scaler_pos_changed = 1;
            content_left = parsed[0];
            content_top = parsed[1];
            content_w = w;
            content_h = h;   
            //video_notify_flag = video_notify_flag|VIDEO_NOTIFY_POS_CHANGED;
        }else
#endif
        {
            if ((w == 1) && (h == 1)) {
                w = h = 0;
                vpp2_set_video_layer_position(parsed[0], parsed[1], 0, 0);
            } else if ((w > 0) && (h > 0)) {
                vpp2_set_video_layer_position(parsed[0], parsed[1], w, h);
            }
        }
        video_property_changed = true;
    }
    amlog_mask(LOG_MASK_SYSFS,
               "video=>x0:%d,y0:%d,x1:%d,y1:%d\r\n ",
               parsed[0], parsed[1], parsed[2], parsed[3]);
}

static ssize_t video_axis_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    int x, y, w, h;
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
    if(video_scaler_mode){
        x = content_left;
        y = content_top;
        w = content_w;
        h = content_h;
    }else
#endif
    {
        vpp2_get_video_layer_position(&x, &y, &w, &h);
    }
    return snprintf(buf, 40, "%d %d %d %d\n", x, y, x + w - 1, y + h - 1);
}

static ssize_t video_axis_store(struct class *cla, struct class_attribute *attr, const char *buf,
                                size_t count)
{
    mutex_lock(&video_module_mutex);

    set_video_window(buf);

    mutex_unlock(&video_module_mutex);

    return strnlen(buf, count);
}

static ssize_t video_zoom_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    u32 r = vpp2_get_zoom_ratio();

    return snprintf(buf, 40, "%d\n", r);
}

static ssize_t video_zoom_store(struct class *cla, struct class_attribute *attr, const char *buf,
                                size_t count)
{
    u32 r;
    char *endp;
    
    r = simple_strtoul(buf, &endp, 0);
    if ((r <= MAX_ZOOM_RATIO) && (r != vpp2_get_zoom_ratio())) {
        vpp2_set_zoom_ratio(r);
        video_property_changed = true;
    }
    return count;
}

static ssize_t video_screen_mode_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    const char *wide_str[] = {"normal", "full stretch", "4-3", "16-9", "normal-noscaleup"};

    if (wide_setting < ARRAY_SIZE(wide_str)) {
        return sprintf(buf, "%d:%s\n", wide_setting, wide_str[wide_setting]);
    } else {
        return 0;
    }
}

static ssize_t video_screen_mode_store(struct class *cla, struct class_attribute *attr, const char *buf,
                                       size_t count)
{
    unsigned long mode;
    char *endp;

    mode = simple_strtol(buf, &endp, 0);

    if ((mode < VIDEO_WIDEOPTION_MAX) && (mode != wide_setting)) {
        wide_setting = mode;
        video_property_changed = true;
    }

    return count;
}

static ssize_t video_blackout_policy_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", blackout);
}

static ssize_t video_blackout_policy_store(struct class *cla, struct class_attribute *attr, const char *buf,
        size_t count)
{
    size_t r;

    r = sscanf(buf, "%d", &blackout);
    if (r != 1) {
        return -EINVAL;
    }

    return count;
}

static ssize_t video_brightness_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    s32 val = (READ_MPEG_REG(VPP2_VADJ1_Y) >> 8) & 0x1ff;

    val = (val << 23) >> 23;

    return sprintf(buf, "%d\n", val);
}

static ssize_t video_brightness_store(struct class *cla, struct class_attribute *attr, const char *buf,
                                      size_t count)
{
    size_t r;
    int val;

    r = sscanf(buf, "%d", &val);
    if ((r != 1) || (val < -255) || (val > 255)) {
        return -EINVAL;
    }

    WRITE_MPEG_REG_BITS(VPP2_VADJ1_Y, val, 8, 9);
    WRITE_MPEG_REG(VPP2_VADJ_CTRL, VPP_VADJ1_EN);

    return count;
}

static ssize_t video_contrast_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", (int)(READ_MPEG_REG(VPP2_VADJ1_Y) & 0xff) - 0x80);
}

static ssize_t video_contrast_store(struct class *cla, struct class_attribute *attr, const char *buf,
                                    size_t count)
{
    size_t r;
    int val;

    r = sscanf(buf, "%d", &val);
    if ((r != 1) || (val < -127) || (val > 127)) {
        return -EINVAL;
    }

    val += 0x80;

    WRITE_MPEG_REG_BITS(VPP2_VADJ1_Y, val, 0, 8);
    WRITE_MPEG_REG(VPP2_VADJ_CTRL, VPP_VADJ1_EN);

    return count;
}

static ssize_t video_saturation_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", READ_MPEG_REG(VPP2_VADJ1_Y) & 0xff);
}

static ssize_t video_saturation_store(struct class *cla, struct class_attribute *attr, const char *buf,
                                      size_t count)
{
    size_t r;
    int val;

    r = sscanf(buf, "%d", &val);
    if ((r != 1) || (val < -127) || (val > 127)) {
        return -EINVAL;
    }

    WRITE_MPEG_REG_BITS(VPP2_VADJ1_Y, val, 0, 8);
    WRITE_MPEG_REG(VPP2_VADJ_CTRL, VPP_VADJ1_EN);

    return count;
}

static ssize_t video_disable_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", disable_video);
}

static ssize_t video_disable_store(struct class *cla, struct class_attribute *attr, const char *buf,
                                   size_t count)
{
    size_t r;
    int val;

    r = sscanf(buf, "%d", &val);
    if (r != 1) {
        return -EINVAL;
    }
    printk("video disable try be changed to %d by %s\n",val,current->comm);
    if ((val < VIDEO_DISABLE_NONE) || (val > VIDEO_DISABLE_FORNEXT)) {
        return -EINVAL;
    }

    disable_video = val;

    if (disable_video != VIDEO_DISABLE_NONE) {
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
        if(video_scaler_mode)
            DisableVideoLayer_PREBELEND();
        else
            DisableVideoLayer();
#else
        DisableVideoLayer();
#endif
        
        if ((disable_video == VIDEO_DISABLE_FORNEXT) && cur_dispbuf && (cur_dispbuf != &vf_local))
            video_property_changed = true;
            
    } else {
        if (cur_dispbuf && (cur_dispbuf != &vf_local)) {
            EnableVideoLayer();
        }
    }

    return count;
}

static int tvin_started = 0;
static void stop_clone(void)
{
#ifdef CONFIG_TVIN_VIUIN
    if(tvin_started){
        stop_tvin_service(0);
        tvin_started=0;
    }
#endif
}

static int start_clone(void)
{
    int ret = -1;
#ifdef CONFIG_TVIN_VIUIN
    tvin_parm_t para;
    const vinfo_t *info = get_current_vinfo();
    if(tvin_started){
      stop_tvin_service(0);
      tvin_started=0;
      ret = 0;
    }
    if(info){
        printk("%s source is %s\n", __func__, info->name);
        if(strcmp(info->name, "panel")==0){
            WRITE_CBUS_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 4, 4, 4); //reg0x271a, Select encT clock to VDIN            
            WRITE_CBUS_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 4, 8, 4); //reg0x271a,Enable VIU of ENC_T domain to VDIN;
        }
        else{
            WRITE_CBUS_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 2, 4, 4); //reg0x271a, Select encP clock to VDIN            
            WRITE_CBUS_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 2, 8, 4); //reg0x271a,Enable VIU of ENC_P domain to VDIN;
        }
        clone_vpts_remainder = 0;
        para.fmt_info.h_active = info->width;
        para.fmt_info.v_active = info->height;
        para.port  = TVIN_PORT_VIU_ENCT;
        para.fmt_info.fmt = TVIN_SIG_FMT_MAX+1;//TVIN_SIG_FMT_MAX+1;TVIN_SIG_FMT_CAMERA_1280X720P_30Hz
        para.fmt_info.frame_rate = clone_frame_rate*10;
        para.fmt_info.hsync_phase = 1;
      	para.fmt_info.vsync_phase  = 0;	
        start_tvin_service(0,&para);
        tvin_started = 1;
        printk("%s: source %dx%d\n", __func__,info->width, info->height);
        ret = 0;
    }
#endif
    return ret;
}


static ssize_t video_clone_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", clone);
}

static ssize_t video_clone_store(struct class *cla, struct class_attribute *attr, const char *buf,
                                   size_t count)
{
    size_t r;
    int val;

    r = sscanf(buf, "%d", &val);
    if (r != 1) {
        return -EINVAL;
    }
    printk("video clone try be changed to %d by %s\n",val,current->comm);
    if ((val < 0) || (val > 1)) {
        return -EINVAL;
    }

    if(clone != val){
        if (val != 0) {
            start_clone();
            clone = 1;
        } else {
            stop_clone();
            clone = 0;
        }
    }
    return count;
}

static ssize_t frame_addr_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    canvas_t canvas;
    u32 addr[3];

    if (cur_dispbuf) {
        canvas_read(cur_dispbuf->canvas0Addr & 0xff, &canvas);
        addr[0] = canvas.addr;
        canvas_read((cur_dispbuf->canvas0Addr >> 8) & 0xff, &canvas);
        addr[1] = canvas.addr;
        canvas_read((cur_dispbuf->canvas0Addr >> 16) & 0xff, &canvas);
        addr[2] = canvas.addr;

        return sprintf(buf, "0x%x-0x%x-0x%x\n", addr[0], addr[1], addr[2]);
    }

    return sprintf(buf, "NA\n");
}

static ssize_t frame_canvas_width_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    canvas_t canvas;
    u32 width[3];

    if (cur_dispbuf) {
        canvas_read(cur_dispbuf->canvas0Addr & 0xff, &canvas);
        width[0] = canvas.width;
        canvas_read((cur_dispbuf->canvas0Addr >> 8) & 0xff, &canvas);
        width[1] = canvas.width;
        canvas_read((cur_dispbuf->canvas0Addr >> 16) & 0xff, &canvas);
        width[2] = canvas.width;

        return sprintf(buf, "%d-%d-%d\n", width[0], width[1], width[2]);
    }

    return sprintf(buf, "NA\n");
}

static ssize_t frame_canvas_height_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    canvas_t canvas;
    u32 height[3];

    if (cur_dispbuf) {
        canvas_read(cur_dispbuf->canvas0Addr & 0xff, &canvas);
        height[0] = canvas.height;
        canvas_read((cur_dispbuf->canvas0Addr >> 8) & 0xff, &canvas);
        height[1] = canvas.height;
        canvas_read((cur_dispbuf->canvas0Addr >> 16) & 0xff, &canvas);
        height[2] = canvas.height;

        return sprintf(buf, "%d-%d-%d\n", height[0], height[1], height[2]);
    }

    return sprintf(buf, "NA\n");
}

static ssize_t frame_width_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    if (cur_dispbuf) {
        return sprintf(buf, "%d\n", cur_dispbuf->width);
    }

    return sprintf(buf, "NA\n");
}

static ssize_t frame_height_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    if (cur_dispbuf) {
        return sprintf(buf, "%d\n", cur_dispbuf->height);
    }

    return sprintf(buf, "NA\n");
}

static ssize_t frame_format_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    if (cur_dispbuf) {
        if ((cur_dispbuf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
            return sprintf(buf, "interlace-top\n");
        } else if ((cur_dispbuf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_BOTTOM) {
            return sprintf(buf, "interlace-bottom\n");
        } else {
            return sprintf(buf, "progressive\n");
        }
    }

    return sprintf(buf, "NA\n");
}

static ssize_t frame_aspect_ratio_show(struct class *cla, struct class_attribute *attr, char *buf)
{
    if (cur_dispbuf) {
        u32 ar = (cur_dispbuf->ratio_control & DISP_RATIO_ASPECT_RATIO_MASK)
                 >> DISP_RATIO_ASPECT_RATIO_BIT;

        if (ar) {
            return sprintf(buf, "0x%x\n", ar);
        } else
            return sprintf(buf, "0x%x\n",
                           (cur_dispbuf->width << 8) / cur_dispbuf->height);
    }

    return sprintf(buf, "NA\n");
}

static ssize_t frame_rate_show(struct class *cla, struct class_attribute* attr, char* buf)
{
    u32 cnt = frame_count - last_frame_count;
    u32 time = jiffies;
    u32 tmp = time;
    u32 rate = 0;
    size_t ret;
    time -= last_frame_time;
    last_frame_time = tmp;
    last_frame_count = frame_count;
    rate = cnt * HZ / time;
    ret = sprintf(buf, "Frame rate is %d, and the panel refresh rate is %d, duration is: %d\n",
                  rate, vinfo->sync_duration_num / vinfo->sync_duration_den, time);
    return ret;
}

static ssize_t vframe_states_show(struct class *cla, struct class_attribute* attr, char* buf)
{
    int ret = 0;
    vframe_states_t states;

    if (vf_get_states(&states) == 0) {
        ret += sprintf(buf + ret, "vframe_pool_size=%d\n", states.vf_pool_size);
        ret += sprintf(buf + ret, "vframe buf_free_num=%d\n", states.buf_free_num);
        ret += sprintf(buf + ret, "vframe buf_recycle_num=%d\n", states.buf_recycle_num);
        ret += sprintf(buf + ret, "vframe buf_avail_num=%d\n", states.buf_avail_num);

    } else {
        ret += sprintf(buf + ret, "vframe no states\n");
    }

    return ret;
}

static ssize_t device_resolution_show(struct class *cla, struct class_attribute* attr, char* buf)
{
    const vinfo_t *info = get_current_vinfo2();

    if (info != NULL) {
        return sprintf(buf, "%dx%d\n", info->width, info->height);
    } else {
        return sprintf(buf, "0x0\n");
    }
}

static struct class_attribute amvideo_class_attrs[] = {
    __ATTR(axis,
    S_IRUGO | S_IWUSR,
    video_axis_show,
    video_axis_store),
    __ATTR(screen_mode,
    S_IRUGO | S_IWUSR,
    video_screen_mode_show,
    video_screen_mode_store),
    __ATTR(blackout_policy,
    S_IRUGO | S_IWUSR,
    video_blackout_policy_show,
    video_blackout_policy_store),
    __ATTR(disable_video,
    S_IRUGO | S_IWUSR,
    video_disable_show,
    video_disable_store),
    __ATTR(zoom,
    S_IRUGO | S_IWUSR,
    video_zoom_show,
    video_zoom_store),
    __ATTR(brightness,
    S_IRUGO | S_IWUSR,
    video_brightness_show,
    video_brightness_store),
    __ATTR(contrast,
    S_IRUGO | S_IWUSR,
    video_contrast_show,
    video_contrast_store),
    __ATTR(saturation,
    S_IRUGO | S_IWUSR,
    video_saturation_show,
    video_saturation_store),
    __ATTR(clone,
    S_IRUGO | S_IWUSR,
    video_clone_show,
    video_clone_store),
    __ATTR_RO(device_resolution),
    __ATTR_RO(frame_addr),
    __ATTR_RO(frame_canvas_width),
    __ATTR_RO(frame_canvas_height),
    __ATTR_RO(frame_width),
    __ATTR_RO(frame_height),
    __ATTR_RO(frame_format),
    __ATTR_RO(frame_aspect_ratio),
    __ATTR_RO(frame_rate),
    __ATTR_RO(vframe_states),
    __ATTR_NULL
};

#ifdef CONFIG_PM
static int amvideo_class_suspend(struct device *dev, pm_message_t state)
{
    pm_state.event = state.event;

    if (state.event == PM_EVENT_SUSPEND) {
        pm_state.vpp_misc = READ_MPEG_REG(VPP2_MISC);
        DisableVideoLayer();
        msleep(50);
    }

    return 0;
}

static int amvideo_class_resume(struct device *dev)
{
    if (pm_state.event == PM_EVENT_SUSPEND) {
        WRITE_MPEG_REG(VPP2_MISC, pm_state.vpp_misc);
        pm_state.event = -1;
    }

    return 0;
}
#endif

static struct class amvideo_class = {
        .name = AMVIDEO_CLASS_NAME,
        .class_attrs = amvideo_class_attrs,
#ifdef CONFIG_PM
        .suspend = amvideo_class_suspend,
        .resume = amvideo_class_resume,
#endif
    };

static struct device *amvideo_dev;

static int vout_notify_callback(struct notifier_block *block, unsigned long cmd , void *para)
{
    const vinfo_t *info;
    ulong flags;

    if (cmd != VOUT_EVENT_MODE_CHANGE) {
        return -1;
    }

    info = get_current_vinfo2();

    spin_lock_irqsave(&lock, flags);

    vinfo = info;

    /* pre-calculate vsync_pts_inc in 90k unit */
    vsync_pts_inc = 90000 * vinfo->sync_duration_den / vinfo->sync_duration_num;

    spin_unlock_irqrestore(&lock, flags);

    if(clone){
        start_clone();
    }
    return 0;
}


static struct notifier_block vout_notifier = {
    .notifier_call  = vout_notify_callback,
};

//vframe_t* get_cur_dispbuf(void)
//{
//	return  cur_dispbuf;	
//}

static void vout_hook(void)
{
    vout2_register_client(&vout_notifier);

/*
    vinfo = get_current_vinfo2();

    if (!vinfo) {
        set_current_vmode2(VMODE_720P);

        vinfo = get_current_vinfo2();
    }

    if (vinfo) {
        vsync_pts_inc = 90000 * vinfo->sync_duration_den / vinfo->sync_duration_num;
    }
*/
#ifdef CONFIG_AM_VIDEO_LOG
    if (vinfo) {
        amlog_mask(LOG_MASK_VINFO, "vinfo = %p\n", vinfo);
        amlog_mask(LOG_MASK_VINFO, "display platform %s:\n", vinfo->name);
        amlog_mask(LOG_MASK_VINFO, "\tresolution %d x %d\n", vinfo->width, vinfo->height);
        amlog_mask(LOG_MASK_VINFO, "\taspect ratio %d : %d\n", vinfo->aspect_ratio_num, vinfo->aspect_ratio_den);
        amlog_mask(LOG_MASK_VINFO, "\tsync duration %d : %d\n", vinfo->sync_duration_num, vinfo->sync_duration_den);
    }
#endif
}

/*********************************************************/
static int __init video2_early_init(void)
{
    logo_object_t  *init_logo_obj=NULL;
    init_logo_obj = NULL; //get_current_logo_obj();

    if(NULL==init_logo_obj || !init_logo_obj->para.loaded)
    {
    	WRITE_MPEG_REG_BITS(VPP2_OFIFO_SIZE, 0x300,
                        VPP_OFIFO_SIZE_BIT, VPP_OFIFO_SIZE_WID);
   	 CLEAR_MPEG_REG_MASK(VPP2_VSC_PHASE_CTRL, VPP_PHASECTL_TYPE_INTERLACE);
#ifndef CONFIG_FB_AML_TCON
    	SET_MPEG_REG_MASK(VPP2_MISC, VPP_OUT_SATURATE);
#endif
    	WRITE_MPEG_REG(VPP2_HOLD_LINES, 0x08080808);
    }
    return 0;
}
static int __init video2_init(void)
{
    int r = 0;
    ulong clk = clk_get_rate(clk_get_sys("clk_misc_pll", NULL));

#ifndef CONFIG_ARCH_MESON3
    /* MALI clock settings */
    if ((clk <= 750000000) &&
        (clk >= 600000000)) {
        WRITE_CBUS_REG(HHI_MALI_CLK_CNTL,
                       (2 << 9)    |   // select misc pll as clock source
                       (1 << 8)    |   // enable clock gating
                       (2 << 0));      // Misc clk / 3
    } else {
        WRITE_CBUS_REG(HHI_MALI_CLK_CNTL,
                       (3 << 9)    |   // select DDR clock as clock source
                       (1 << 8)    |   // enable clock gating
                       (1 << 0));      // DDR clk / 2
    }
#endif

#ifdef RESERVE_CLR_FRAME
    alloc_keep_buffer();
#endif

    DisableVideoLayer();
    cur_dispbuf = NULL;

#ifdef FIQ_VSYNC
    /* enable fiq bridge */
	vsync_fiq_bridge.handle = vsync_bridge_isr;
	vsync_fiq_bridge.key=(u32)vsync_bridge_isr;
	vsync_fiq_bridge.name="vsync_bridge_isr";

    r = register_fiq_bridge_handle(&vsync_fiq_bridge);

    if (r) {
        amlog_level(LOG_LEVEL_ERROR, "video fiq bridge register error.\n");
        r = -ENOENT;
        goto err0;
    }
#endif

    /* sysfs node creation */
    r = class_register(&amvideo_class);
    if (r) {
        amlog_level(LOG_LEVEL_ERROR, "create video class fail.\n");
#ifdef FIQ_VSYNC
        free_irq(BRIDGE_IRQ, (void *)video_dev_id);
#else
        free_irq(INT_VIU2_VSYNC, (void *)video_dev_id);
#endif
        goto err1;
    }

    /* create video device */
    r = register_chrdev(AMVIDEO2_MAJOR, "amvideo2", &amvideo_fops);
    if (r < 0) {
        amlog_level(LOG_LEVEL_ERROR, "Can't register major for amvideo device\n");
        goto err2;
    }

    amvideo_dev = device_create(&amvideo_class, NULL,
                                MKDEV(AMVIDEO2_MAJOR, 0), NULL,
                                DEVICE_NAME);

    if (IS_ERR(amvideo_dev)) {
        amlog_level(LOG_LEVEL_ERROR, "Can't create amvideo device\n");
        goto err3;
    }

    init_waitqueue_head(&amvideo_trick_wait);

    vout_hook();

    disp_canvas[0] = (disp_canvas_index[2] << 16) | (disp_canvas_index[1] << 8) | disp_canvas_index[0];
    disp_canvas[1] = (disp_canvas_index[5] << 16) | (disp_canvas_index[4] << 8) | disp_canvas_index[3];

    vsync_fiq_up();

    vf_receiver_init(&video_vf_recv, RECEIVER_NAME, &video_vf_receiver, NULL);
    vf_reg_receiver(&video_vf_recv);

    return (0);

err3:
    unregister_chrdev(AMVIDEO2_MAJOR, DEVICE_NAME);

err2:
#ifdef FIQ_VSYNC
    unregister_fiq_bridge_handle(&vsync_fiq_bridge);
#endif

err1:
    class_unregister(&amvideo_class);

#ifdef FIQ_VSYNC
err0:
#endif
    return r;
}

static void __exit video2_exit(void)
{
    vf_unreg_receiver(&video_vf_recv);

    DisableVideoLayer();

    //vsync_fiq_down();

    device_destroy(&amvideo_class, MKDEV(AMVIDEO2_MAJOR, 0));

    unregister_chrdev(AMVIDEO2_MAJOR, DEVICE_NAME);

#ifdef FIQ_VSYNC
    unregister_fiq_bridge_handle(&vsync_fiq_bridge);
#endif

    class_unregister(&amvideo_class);
}

void set_clone_frame_rate(unsigned int frame_rate, unsigned int delay)
{
    clone_frame_rate_delay = delay;
    clone_frame_rate_set_value = frame_rate;
    if(delay==0){
        clone_frame_rate = frame_rate;    
    }
}    
EXPORT_SYMBOL(set_clone_frame_rate);

arch_initcall(video2_early_init);
module_init(video2_init);
module_exit(video2_exit);

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n debug flag \n");

MODULE_PARM_DESC(clone_frame_rate, "\n clone_frame_rate\n");
module_param(clone_frame_rate, uint, 0664);

MODULE_PARM_DESC(clone_frame_rate_force, "\n clone_frame_rate_force\n");
module_param(clone_frame_rate_force, uint, 0664);

MODULE_PARM_DESC(clone_frame_scale_width, "\n clone_frame_scale_width\n");
module_param(clone_frame_scale_width, uint, 0664);

MODULE_PARM_DESC(vsync_hold_line, "\n vsync_hold_line\n");
module_param(vsync_hold_line, uint, 0664);

MODULE_PARM_DESC(isr_interval_max, "\n isr_interval_max\n");
module_param(isr_interval_max, uint, 0664);

MODULE_PARM_DESC(isr_run_time_max, "\n isr_run_time_max\n");
module_param(isr_run_time_max, uint, 0664);

MODULE_PARM_DESC(throw_frame, "\n throw_frame\n");
module_param(throw_frame, uint, 0664);
