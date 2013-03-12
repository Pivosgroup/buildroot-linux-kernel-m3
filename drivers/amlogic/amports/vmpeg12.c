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
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/amstream.h>
#include <linux/amports/canvas.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vfp.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <mach/am_regs.h>

#ifdef CONFIG_AM_VDEC_MPEG12_LOG
#define AMLOG
#define LOG_LEVEL_VAR       amlog_level_vmpeg
#define LOG_MASK_VAR        amlog_mask_vmpeg
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_INFO      1
#define LOG_LEVEL_DESC  "0:ERROR, 1:INFO"
#endif
#include <linux/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_LEVEL_DESC, LOG_DEFAULT_MASK_DESC);

#include "amvdec.h"
#include "vmpeg12_mc.h"

#define DRIVER_NAME "amvdec_mpeg12"
#define MODULE_NAME "amvdec_mpeg12"

/* protocol registers */
#define MREG_SEQ_INFO       AV_SCRATCH_4
#define MREG_PIC_INFO       AV_SCRATCH_5
#define MREG_PIC_WIDTH      AV_SCRATCH_6
#define MREG_PIC_HEIGHT     AV_SCRATCH_7
#define MREG_BUFFERIN       AV_SCRATCH_8
#define MREG_BUFFEROUT      AV_SCRATCH_9

#define MREG_CMD            AV_SCRATCH_A
#define MREG_CO_MV_START    AV_SCRATCH_B
#define MREG_ERROR_COUNT    AV_SCRATCH_C
#define MREG_FRAME_OFFSET   AV_SCRATCH_D

#define PICINFO_ERROR       0x80000000
#define PICINFO_TYPE_MASK   0x00030000
#define PICINFO_TYPE_I      0x00000000
#define PICINFO_TYPE_P      0x00010000
#define PICINFO_TYPE_B      0x00020000

#define PICINFO_PROG        0x8000
#define PICINFO_RPT_FIRST   0x4000
#define PICINFO_TOP_FIRST   0x2000

#define SEQINFO_EXT_AVAILABLE   0x80000000
#define SEQINFO_PROG            0x00010000

#define VF_POOL_SIZE        12
#define PUT_INTERVAL        HZ/100

#define INCPTR(p) ptr_atomic_wrap_inc(&p)

#define STAT_TIMER_INIT     0x01
#define STAT_MC_LOAD        0x02
#define STAT_ISR_REG        0x04
#define STAT_VF_HOOK        0x08
#define STAT_TIMER_ARM      0x10
#define STAT_VDEC_RUN       0x20

#define DEC_CONTROL_FLAG_FORCE_2500_720_576_INTERLACE  0x0002
#define DEC_CONTROL_FLAG_FORCE_3000_704_480_INTERLACE  0x0004
#define DEC_CONTROL_FLAG_FORCE_2500_704_576_INTERLACE  0x0008
#define DEC_CONTROL_FLAG_FORCE_2500_544_576_INTERLACE  0x0010
#define DEC_CONTROL_FLAG_FORCE_2500_480_576_INTERLACE  0x0020


static vframe_t *vmpeg_vf_peek(void*);
static vframe_t *vmpeg_vf_get(void*);
static void vmpeg_vf_put(vframe_t *, void*);
static int  vmpeg_vf_states(vframe_states_t *states, void*);
static int vmpeg_event_cb(int type, void *data, void *private_data);

static void vmpeg12_prot_init(void);
static void vmpeg12_local_init(void);

static const char vmpeg12_dec_id[] = "vmpeg12-dev";
#define PROVIDER_NAME   "decoder.mpeg12"
static const struct vframe_operations_s vmpeg_vf_provider =
{
    .peek = vmpeg_vf_peek,
    .get  = vmpeg_vf_get,
    .put  = vmpeg_vf_put,
    .event_cb = vmpeg_event_cb,
    .vf_states = vmpeg_vf_states,
};
static struct vframe_provider_s vmpeg_vf_prov;

static const u32 frame_rate_tab[16] = {
    96000 / 30, 96000 / 24, 96000 / 24, 96000 / 25,
    96000 / 30, 96000 / 30, 96000 / 50, 96000 / 60,
    96000 / 60,
    /* > 8 reserved, use 24 */
    96000 / 24, 96000 / 24, 96000 / 24, 96000 / 24,
    96000 / 24, 96000 / 24, 96000 / 24
};

static struct vframe_s vfqool[VF_POOL_SIZE];
static s32 vfbuf_use[VF_POOL_SIZE];
static struct vframe_s *vfp_pool_newframe[VF_POOL_SIZE+1];
static struct vframe_s *vfp_pool_display[VF_POOL_SIZE+1];
static struct vframe_s *vfp_pool_recycle[VF_POOL_SIZE+1];
static vfq_t newframe_q, display_q, recycle_q;

static u32 dec_control = 0;
static u32 frame_width, frame_height, frame_dur, frame_prog;
static struct timer_list recycle_timer;
static u32 stat;
static u32 buf_start, buf_size;
static spinlock_t lock = SPIN_LOCK_UNLOCKED;

/* for error handling */
static s32 frame_force_skip_flag = 0;
static s32 error_frame_skip_level = 0;

static inline u32 index2canvas(u32 index)
{
    const u32 canvas_tab[4] = {
        0x020100, 0x050403, 0x080706, 0x0b0a09
    };

    return canvas_tab[index];
}

static void set_frame_info(vframe_t *vf)
{
    unsigned ar_bits;

#ifdef CONFIG_AM_VDEC_MPEG12_LOG
    bool first = (frame_width == 0) && (frame_height == 0);
#endif

    vf->width  = frame_width = READ_MPEG_REG(MREG_PIC_WIDTH);
    vf->height = frame_height = READ_MPEG_REG(MREG_PIC_HEIGHT);
	
    if(frame_height==1088)
    vf->height=frame_height=1080;
	
    if (frame_dur > 0) {
        vf->duration = frame_dur;
    } else {
        vf->duration = frame_dur =
                           frame_rate_tab[(READ_MPEG_REG(MREG_SEQ_INFO) >> 4) & 0xf];
    }

    ar_bits = READ_MPEG_REG(MREG_SEQ_INFO) & 0xf;

    if (ar_bits == 0x2) {
        vf->ratio_control = 0xc0 << DISP_RATIO_ASPECT_RATIO_BIT;

    } else if (ar_bits == 0x3) {
        vf->ratio_control = 0x90 << DISP_RATIO_ASPECT_RATIO_BIT;

    } else if (ar_bits == 0x4) {
        vf->ratio_control = 0x74 << DISP_RATIO_ASPECT_RATIO_BIT;

    } else {
        vf->ratio_control = 0;
    }

    amlog_level_if(first, LOG_LEVEL_INFO, "mpeg2dec: w(%d), h(%d), dur(%d), dur-ES(%d)\n",
                   frame_width,
                   frame_height,
                   frame_dur,
                   frame_rate_tab[(READ_MPEG_REG(MREG_SEQ_INFO) >> 4) & 0xf]);
}

static bool error_skip(u32 info, vframe_t *vf)
{
    if (error_frame_skip_level) {
        /* skip error frame */
        if ((info & PICINFO_ERROR) || (frame_force_skip_flag)) {
            if ((info & PICINFO_ERROR) == 0) {
                if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) {
                    frame_force_skip_flag = 0;
                }
            } else {
                if (error_frame_skip_level >= 2) {
                    frame_force_skip_flag = 1;
                }
            }
            if ((info & PICINFO_ERROR) || (frame_force_skip_flag)) {
                return true;
            }
        }
    }

    return false;
}

static irqreturn_t vmpeg12_isr(int irq, void *dev_id)
{
    u32 reg, info, seqinfo, offset, pts, pts_valid = 0;
    vframe_t *vf;
    ulong flags;

    WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

    reg = READ_MPEG_REG(MREG_BUFFEROUT);

    if (reg) {
        info = READ_MPEG_REG(MREG_PIC_INFO);
        offset = READ_MPEG_REG(MREG_FRAME_OFFSET);

        if (((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) &&
            (pts_lookup_offset(PTS_TYPE_VIDEO, offset, &pts, 0) == 0)) {
            pts_valid = 1;
        }

        /*if (frame_prog == 0)*/ {
            frame_prog = info & PICINFO_PROG;
        }

        if ((dec_control & DEC_CONTROL_FLAG_FORCE_2500_720_576_INTERLACE) &&
            (frame_width == 720) &&
            (frame_height == 576) &&
            (frame_dur == 3840)) {
            frame_prog = 0;
        }
        else if ((dec_control & DEC_CONTROL_FLAG_FORCE_3000_704_480_INTERLACE) &&
            (frame_width == 704) &&
            (frame_height == 480) &&
            (frame_dur == 3200)) {
            frame_prog = 0;
        }
        else if ((dec_control & DEC_CONTROL_FLAG_FORCE_2500_704_576_INTERLACE) &&
            (frame_width == 704) &&
            (frame_height == 576) &&
            (frame_dur == 3840)) {
            frame_prog = 0;
        }
        else if ((dec_control & DEC_CONTROL_FLAG_FORCE_2500_544_576_INTERLACE) &&
            (frame_width == 544) &&
            (frame_height == 576) &&
            (frame_dur == 3840)) {
            frame_prog = 0;
        }
	else if ((dec_control & DEC_CONTROL_FLAG_FORCE_2500_480_576_INTERLACE) &&
            (frame_width == 480) &&
            (frame_height == 576) &&
            (frame_dur == 3840)) {
            frame_prog = 0;
        }


        if (frame_prog & PICINFO_PROG) {
            u32 index = ((reg & 7) - 1) & 3;

            seqinfo = READ_MPEG_REG(MREG_SEQ_INFO);

            vf = vfq_pop(&newframe_q);

            set_frame_info(vf);

            vf->index = index;
            vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;

            if ((seqinfo & SEQINFO_EXT_AVAILABLE) && (seqinfo & SEQINFO_PROG)) {
                if (info & PICINFO_RPT_FIRST) {
                    if (info & PICINFO_TOP_FIRST) {
                        vf->duration = vf->duration * 3;    // repeat three times
                    } else {
                        vf->duration = vf->duration * 2;    // repeat two times
                    }
                }
                vf->duration_pulldown = 0; // no pull down

            } else {
                vf->duration_pulldown = (info & PICINFO_RPT_FIRST) ?
                                        vf->duration >> 1 : 0;
            }

            vf->duration += vf->duration_pulldown;
            vf->canvas0Addr = vf->canvas1Addr = index2canvas(index);
            vf->pts = (pts_valid) ? pts : 0;

            vfbuf_use[index]++;

            if (error_skip(info, vf)) {
                spin_lock_irqsave(&lock, flags);
                vfq_push(&recycle_q, vf);
                spin_unlock_irqrestore(&lock, flags);
            } else {
                vfq_push(&display_q, vf);
            }
            vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);

        } else {
            u32 index = ((reg & 7) - 1) & 3;

            vf = vfq_pop(&newframe_q);

            vfbuf_use[index] += 2;

            set_frame_info(vf);

            vf->index = index;
            vf->type = (info & PICINFO_TOP_FIRST) ?
                       VIDTYPE_INTERLACE_TOP : VIDTYPE_INTERLACE_BOTTOM;
            vf->duration >>= 1;
            vf->duration_pulldown = (info & PICINFO_RPT_FIRST) ?
                                    vf->duration >> 1 : 0;
            vf->duration += vf->duration_pulldown;
            vf->canvas0Addr = vf->canvas1Addr = index2canvas(index);
            vf->pts = (pts_valid) ? pts : 0;

            if (error_skip(info, vf)) {
                vfq_push(&recycle_q, vf);
            } else {
                vfq_push(&display_q, vf);
                vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
            }

            vf = vfq_pop(&newframe_q);

            set_frame_info(vf);

            vf->index = index;
            vf->type = (info & PICINFO_TOP_FIRST) ?
                       VIDTYPE_INTERLACE_TOP : VIDTYPE_INTERLACE_BOTTOM;
            vf->duration >>= 1;
            vf->duration_pulldown = (info & PICINFO_RPT_FIRST) ?
                                    vf->duration >> 1 : 0;
            vf->duration += vf->duration_pulldown;
            vf->canvas0Addr = vf->canvas1Addr = index2canvas(index);
            vf->pts = 0;

            if (error_skip(info, vf)) {
                vfq_push(&recycle_q, vf);
            } else {
                vfq_push(&display_q, vf);
                vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
            }
        }

        WRITE_MPEG_REG(MREG_BUFFEROUT, 0);
    }

    return IRQ_HANDLED;
}

static vframe_t *vmpeg_vf_peek(void* op_arg)
{
    return vfq_peek(&display_q);
}

static vframe_t *vmpeg_vf_get(void* op_arg)
{
    return vfq_pop(&display_q);
}

static void vmpeg_vf_put(vframe_t *vf, void* op_arg)
{
    vfq_push(&recycle_q, vf);
}

static int vmpeg_event_cb(int type, void *data, void *private_data)
{
    if(type & VFRAME_EVENT_RECEIVER_RESET){
        unsigned long flags;
        amvdec_stop();
#ifndef CONFIG_POST_PROCESS_MANAGER
        vf_light_unreg_provider(&vmpeg_vf_prov);
#endif
        spin_lock_irqsave(&lock, flags);
        vmpeg12_local_init();
        vmpeg12_prot_init();
        spin_unlock_irqrestore(&lock, flags); 
#ifndef CONFIG_POST_PROCESS_MANAGER
        vf_reg_provider(&vmpeg_vf_prov);
#endif              
        amvdec_start();
    }
    return 0;        
}

static int  vmpeg_vf_states(vframe_states_t *states, void* op_arg)
{
    unsigned long flags;
    spin_lock_irqsave(&lock, flags);
    states->vf_pool_size = VF_POOL_SIZE;
    states->buf_recycle_num = vfq_level(&recycle_q);
    states->buf_free_num = vfq_level(&newframe_q);
    states->buf_avail_num = vfq_level(&display_q);
    spin_unlock_irqrestore(&lock, flags);
    return 0;
}

static void vmpeg_put_timer_func(unsigned long arg)
{
    struct timer_list *timer = (struct timer_list *)arg;

    while (!vfq_empty(&recycle_q) && (READ_MPEG_REG(MREG_BUFFERIN) == 0)) {
        vframe_t *vf = vfq_pop(&recycle_q);

        if (--vfbuf_use[vf->index] == 0) {
            WRITE_MPEG_REG(MREG_BUFFERIN, vf->index + 1);
        }

        vfq_push(&newframe_q, vf);
    }

    timer->expires = jiffies + PUT_INTERVAL;

    add_timer(timer);
}

int vmpeg12_dec_status(struct vdec_status *vstatus)
{
    vstatus->width = frame_width;
    vstatus->height = frame_height;
    if (frame_dur != 0) {
        vstatus->fps = 96000 / frame_dur;
    } else {
        vstatus->fps = 96000;
    }
    vstatus->error_count = READ_MPEG_REG(AV_SCRATCH_C);
    vstatus->status = stat;

    return 0;
}

/****************************************/
static void vmpeg12_canvas_init(void)
{
    int i;
    u32 canvas_width, canvas_height;
    u32 decbuf_size, decbuf_y_size, decbuf_uv_size;
    u32 disp_addr = 0xffffffff;

    if (buf_size <= 0x00400000) {
        /* SD only */
        canvas_width   = 768;
        canvas_height  = 576;
        decbuf_y_size  = 0x80000;
        decbuf_uv_size = 0x20000;
        decbuf_size    = 0x100000;
    } else {
        /* HD & SD */
        canvas_width   = 1920;
        canvas_height  = 1088;
        decbuf_y_size  = 0x200000;
        decbuf_uv_size = 0x80000;
        decbuf_size    = 0x300000;
    }

    if (READ_MPEG_REG(VPP_MISC) & VPP_VD1_POSTBLEND) {
        canvas_t cur_canvas;

        canvas_read((READ_MPEG_REG(VD1_IF0_CANVAS0) & 0xff), &cur_canvas);
        disp_addr = (cur_canvas.addr + 7) >> 3;
    }

    for (i = 0; i < 4; i++) {
        if (((buf_start + i * decbuf_size + 7) >> 3) == disp_addr) {
            canvas_config(3 * i + 0,
                          buf_start + 4 * decbuf_size,
                          canvas_width, canvas_height,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
            canvas_config(3 * i + 1,
                          buf_start + 4 * decbuf_size + decbuf_y_size,
                          canvas_width / 2, canvas_height / 2,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
            canvas_config(3 * i + 2,
                          buf_start + 4 * decbuf_size + decbuf_y_size + decbuf_uv_size,
                          canvas_width / 2, canvas_height / 2,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
        } else {
            canvas_config(3 * i + 0,
                          buf_start + i * decbuf_size,
                          canvas_width, canvas_height,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
            canvas_config(3 * i + 1,
                          buf_start + i * decbuf_size + decbuf_y_size,
                          canvas_width / 2, canvas_height / 2,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
            canvas_config(3 * i + 2,
                          buf_start + i * decbuf_size + decbuf_y_size + decbuf_uv_size,
                          canvas_width / 2, canvas_height / 2,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
        }
    }

    WRITE_MPEG_REG(MREG_CO_MV_START, buf_start + 4 * decbuf_size);

}

static void vmpeg12_prot_init(void)
{
    WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC);

    vmpeg12_canvas_init();

    WRITE_MPEG_REG(AV_SCRATCH_0, 0x020100);
    WRITE_MPEG_REG(AV_SCRATCH_1, 0x050403);
    WRITE_MPEG_REG(AV_SCRATCH_2, 0x080706);
    WRITE_MPEG_REG(AV_SCRATCH_3, 0x0b0a09);

    /* set to mpeg1 default */
    WRITE_MPEG_REG(MPEG1_2_REG, 0);
    /* disable PSCALE for hardware sharing */
    WRITE_MPEG_REG(PSCALE_CTRL, 0);
    /* for Mpeg1 default value */
    WRITE_MPEG_REG(PIC_HEAD_INFO, 0x380);
    /* disable mpeg4 */
    WRITE_MPEG_REG(M4_CONTROL_REG, 0);
    /* clear mailbox interrupt */
    WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);
    /* clear buffer IN/OUT registers */
    WRITE_MPEG_REG(MREG_BUFFERIN, 0);
    WRITE_MPEG_REG(MREG_BUFFEROUT, 0);
    /* set reference width and height */
    if ((frame_width != 0) && (frame_height != 0)) {
        WRITE_MPEG_REG(MREG_CMD, (frame_width << 16) | frame_height);
    } else {
        WRITE_MPEG_REG(MREG_CMD, 0);
    }
    /* clear error count */
    WRITE_MPEG_REG(MREG_ERROR_COUNT, 0);
}

static void vmpeg12_local_init(void)
{
    int i;
    vfq_init(&display_q, VF_POOL_SIZE+1, &vfp_pool_display[0]);
    vfq_init(&recycle_q, VF_POOL_SIZE+1, &vfp_pool_recycle[0]);
    vfq_init(&newframe_q, VF_POOL_SIZE+1, &vfp_pool_newframe[0]);

    for (i = 0; i < VF_POOL_SIZE; i++) {
        vfq_push(&newframe_q, &vfqool[i]);
    }

    frame_width = frame_height = frame_dur = frame_prog = 0;

    for (i = 0; i < 4; i++) {
        vfbuf_use[i] = 0;
    }

    frame_force_skip_flag = 0;
}

static s32 vmpeg12_init(void)
{
    int r;

    init_timer(&recycle_timer);

    stat |= STAT_TIMER_INIT;

    vmpeg12_local_init();

    amvdec_enable();

    if (amvdec_loadmc(vmpeg12_mc) < 0) {
        amvdec_disable();
        return -EBUSY;
    }

    stat |= STAT_MC_LOAD;

    /* enable AMRISC side protocol */
    vmpeg12_prot_init();

    r = request_irq(INT_MAILBOX_1A, vmpeg12_isr,
                    IRQF_SHARED, "vmpeg12-irq", (void *)vmpeg12_dec_id);

    if (r) {
        amvdec_disable();
        amlog_level(LOG_LEVEL_ERROR, "vmpeg12 irq register error.\n");
        return -ENOENT;
    }

    stat |= STAT_ISR_REG;
 #ifdef CONFIG_POST_PROCESS_MANAGER
    vf_provider_init(&vmpeg_vf_prov, PROVIDER_NAME, &vmpeg_vf_provider, NULL);
    vf_reg_provider(&vmpeg_vf_prov);
    vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_START,NULL);
 #else 
    vf_provider_init(&vmpeg_vf_prov, PROVIDER_NAME, &vmpeg_vf_provider, NULL);
    vf_reg_provider(&vmpeg_vf_prov);
 #endif 

    stat |= STAT_VF_HOOK;

    recycle_timer.data = (ulong)&recycle_timer;
    recycle_timer.function = vmpeg_put_timer_func;
    recycle_timer.expires = jiffies + PUT_INTERVAL;

    add_timer(&recycle_timer);

    stat |= STAT_TIMER_ARM;

    amvdec_start();

    stat |= STAT_VDEC_RUN;

    set_vdec_func(&vmpeg12_dec_status);

    return 0;
}

static int amvdec_mpeg12_probe(struct platform_device *pdev)
{
    struct resource *mem;

    amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 probe start.\n");

    if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0))) {
        amlog_level(LOG_LEVEL_ERROR, "amvdec_mpeg12 memory resource undefined.\n");
        return -EFAULT;
    }

    buf_start = mem->start;
    buf_size  = mem->end - mem->start + 1;

    if (vmpeg12_init() < 0) {
        amlog_level(LOG_LEVEL_ERROR, "amvdec_mpeg12 init failed.\n");

        return -ENODEV;
    }

    amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 probe end.\n");

    return 0;
}

static int amvdec_mpeg12_remove(struct platform_device *pdev)
{
    if (stat & STAT_VDEC_RUN) {
        amvdec_stop();
        stat &= ~STAT_VDEC_RUN;
    }

    if (stat & STAT_ISR_REG) {
        free_irq(INT_MAILBOX_1A, (void *)vmpeg12_dec_id);
        stat &= ~STAT_ISR_REG;
    }

    if (stat & STAT_TIMER_ARM) {
        del_timer_sync(&recycle_timer);
        stat &= ~STAT_TIMER_ARM;
    }

    if (stat & STAT_VF_HOOK) {
        ulong flags;
        spin_lock_irqsave(&lock, flags);
        vfq_init(&display_q, VF_POOL_SIZE+1, &vfp_pool_display[0]);
        vfq_init(&recycle_q, VF_POOL_SIZE+1, &vfp_pool_recycle[0]);
        vfq_init(&newframe_q, VF_POOL_SIZE+1, &vfp_pool_newframe[0]);
        spin_unlock_irqrestore(&lock, flags);

    vf_unreg_provider(&vmpeg_vf_prov);
        stat &= ~STAT_VF_HOOK;
    }

    amvdec_disable();

    amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 remove.\n");

    return 0;
}

/****************************************/

static struct platform_driver amvdec_mpeg12_driver = {
    .probe      = amvdec_mpeg12_probe,
    .remove     = amvdec_mpeg12_remove,
#ifdef CONFIG_PM
    .suspend    = amvdec_suspend,
    .resume     = amvdec_resume,
#endif
    .driver     = {
        .name   = DRIVER_NAME,
    }
};

static struct codec_profile_t amvdec_mpeg12_profile = {
	.name = "mpeg12",
	.profile = ""
};

static int __init amvdec_mpeg12_driver_init_module(void)
{
    amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 module init\n");

    if (platform_driver_register(&amvdec_mpeg12_driver)) {
        amlog_level(LOG_LEVEL_ERROR, "failed to register amvdec_mpeg12 driver\n");
        return -ENODEV;
    }
	vcodec_profile_register(&amvdec_mpeg12_profile);
    return 0;
}

static void __exit amvdec_mpeg12_driver_remove_module(void)
{
    amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 module remove.\n");

    platform_driver_unregister(&amvdec_mpeg12_driver);
}

/****************************************/

module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_mpeg12 stat \n");
module_param(dec_control, uint, 0664);
MODULE_PARM_DESC(dec_control, "\n amvmpeg12 decoder control \n");

module_init(amvdec_mpeg12_driver_init_module);
module_exit(amvdec_mpeg12_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MPEG1/2 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
