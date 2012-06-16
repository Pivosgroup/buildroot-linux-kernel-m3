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
 * Author:  Qi Wang <qi.wang@amlogic.com>
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/amports/amstream.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/canvas.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <mach/am_regs.h>

#include "amvdec.h"
#include "vc1_mc.h"

#define DRIVER_NAME "amvdec_vc1"
#define MODULE_NAME "amvdec_vc1"

#define HANDLE_VC1_IRQ
#define DEBUG_PTS

#define I_PICTURE   0
#define P_PICTURE   1
#define B_PICTURE   2

#define ORI_BUFFER_START_ADDR   0x81000000

#define INTERLACE_FLAG          0x80
#define BOTTOM_FIELD_FIRST_FLAG 0x40

/* protocol registers */
#define VC1_PIC_RATIO       AV_SCRATCH_0
#define VC1_ERROR_COUNT    AV_SCRATCH_6
#define VC1_SOS_COUNT     AV_SCRATCH_7
#define VC1_BUFFERIN       AV_SCRATCH_8
#define VC1_BUFFEROUT      AV_SCRATCH_9
#define VC1_REPEAT_COUNT    AV_SCRATCH_A
#define VC1_TIME_STAMP      AV_SCRATCH_B
#define VC1_OFFSET_REG      AV_SCRATCH_C
#define MEM_OFFSET_REG      AV_SCRATCH_F

#define VF_POOL_SIZE        12
#define PUT_INTERVAL        (HZ/100)

#define INCPTR(p) ptr_atomic_wrap_inc(&p)

#define STAT_TIMER_INIT     0x01
#define STAT_MC_LOAD        0x02
#define STAT_ISR_REG        0x04
#define STAT_VF_HOOK        0x08
#define STAT_TIMER_ARM      0x10
#define STAT_VDEC_RUN       0x20

static vframe_t *vvc1_vf_peek(void*);
static vframe_t *vvc1_vf_get(void*);
static void vvc1_vf_put(vframe_t *, void*);
static int  vvc1_vf_states(vframe_states_t *states, void*);
static int vvc1_event_cb(int type, void *data, void *private_data);

static void vvc1_prot_init(void);
static void vvc1_local_init(void);

static const char vvc1_dec_id[] = "vvc1-dev";

#define PROVIDER_NAME   "decoder.vc1"
static const struct vframe_operations_s vvc1_vf_provider = {
    .peek = vvc1_vf_peek,
    .get = vvc1_vf_get,
    .put = vvc1_vf_put,
    .event_cb = vvc1_event_cb,
    .vf_states = vvc1_vf_states,
};
static struct vframe_provider_s vvc1_vf_prov;

static struct vframe_s vfpool[VF_POOL_SIZE];
static u32 vfpool_idx[VF_POOL_SIZE];
static s32 vfbuf_use[4];
static s32 fill_ptr, get_ptr, putting_ptr, put_ptr;
static u32 frame_width, frame_height, frame_dur, frame_prog;
static struct timer_list recycle_timer;
static u32 stat;
static u32 buf_start, buf_size, buf_offset;
static u32 avi_flag = 0;
static u32 vvc1_ratio;
static u32 vvc1_format;

static u32 intra_output;

static u32 pts_by_offset = 1;
static u32 total_frame;
static u32 next_pts;
#ifdef DEBUG_PTS
static u32 pts_hit, pts_missed, pts_i_hit, pts_i_missed;
#endif
static spinlock_t lock = SPIN_LOCK_UNLOCKED;

static struct dec_sysinfo vvc1_amstream_dec_info;

static inline u32 index2canvas(u32 index)
{
    const u32 canvas_tab[4] = {
        0x020100, 0x050403, 0x080706, 0x0b0a09
    };

    return canvas_tab[index];
}

static inline void ptr_atomic_wrap_inc(u32 *ptr)
{
    u32 i = *ptr;

    i++;

    if (i >= VF_POOL_SIZE) {
        i = 0;
    }

    *ptr = i;
}

static void set_aspect_ratio(vframe_t *vf, unsigned pixel_ratio)
{
    int ar = 0;

    if (vvc1_ratio == 0) {
        vf->ratio_control |= (0x90 << DISP_RATIO_ASPECT_RATIO_BIT); // always stretch to 16:9
    } else if (pixel_ratio > 0x0f) {
        ar = (vvc1_amstream_dec_info.height * (pixel_ratio & 0xff) * vvc1_ratio) / (vvc1_amstream_dec_info.width * (pixel_ratio >> 8));
    } else {
        switch (pixel_ratio) {
        case 0:
            ar = (vvc1_amstream_dec_info.height * vvc1_ratio) / vvc1_amstream_dec_info.width;
            break;
        case 1:
            ar = (vf->height * vvc1_ratio) / vf->width;
            break;
        case 2:
            ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 12);
            break;
        case 3:
            ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 10);
            break;
        case 4:
            ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 16);
            break;
        case 5:
            ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 40);
            break;
        case 6:
            ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 24);
            break;
        case 7:
            ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 20);
            break;
        case 8:
            ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 32);
            break;
        case 9:
            ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 80);
            break;
        case 10:
            ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 18);
            break;
        case 11:
            ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 15);
            break;
        case 12:
            ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 64);
            break;
        case 13:
            ar = (vf->height * 99 * vvc1_ratio) / (vf->width * 160);
            break;
        default:
            ar = (vf->height * vvc1_ratio) / vf->width;
            break;
        }
    }

    ar = min(ar, DISP_RATIO_ASPECT_RATIO_MAX);

    vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
    //vf->ratio_control |= DISP_RATIO_FORCECONFIG | DISP_RATIO_KEEPRATIO;
}

#ifdef HANDLE_VC1_IRQ
static irqreturn_t vvc1_isr(int irq, void *dev_id)
#else
static void vvc1_isr(void)
#endif
{
    u32 reg;
    vframe_t *vf;
    u32 repeat_count;
    u32 picture_type;
    u32 buffer_index;
    unsigned int pts, pts_valid = 0, offset;

    reg = READ_MPEG_REG(VC1_BUFFEROUT);

    if (reg) {
        if (pts_by_offset) {
            offset = READ_MPEG_REG(VC1_OFFSET_REG);
            if (pts_lookup_offset(PTS_TYPE_VIDEO, offset, &pts, 0) == 0) {
                pts_valid = 1;
#ifdef DEBUG_PTS
                pts_hit++;
#endif
            } else {
#ifdef DEBUG_PTS
                pts_missed++;
#endif
            }
        }

        repeat_count = READ_MPEG_REG(VC1_REPEAT_COUNT);
        buffer_index = ((reg & 0x7) - 1) & 3;
        picture_type = (reg >> 3) & 7;

        if ((intra_output == 0) && (picture_type != 0)) {
            WRITE_MPEG_REG(VC1_BUFFERIN, ~(1 << buffer_index));
            WRITE_MPEG_REG(VC1_BUFFEROUT, 0);
            WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

#ifdef HANDLE_VC1_IRQ
            return IRQ_HANDLED;
#else
            return;
#endif
        }

        intra_output = 1;

#ifdef DEBUG_PTS
        if (picture_type == I_PICTURE) {
            //printk("I offset 0x%x, pts_valid %d\n", offset, pts_valid);
            if (!pts_valid) {
                pts_i_missed++;
            } else {
                pts_i_hit++;
            }
        }
#endif

        if (reg & INTERLACE_FLAG) { // interlace
            vfpool_idx[fill_ptr] = buffer_index;
            vf = &vfpool[fill_ptr];
            vf->width = vvc1_amstream_dec_info.width;
            vf->height = vvc1_amstream_dec_info.height;
            vf->bufWidth = 1920;

            if (pts_valid) {
                vf->pts = pts;
                if ((repeat_count > 1) && avi_flag) {
                    vf->duration = vvc1_amstream_dec_info.rate * repeat_count >> 1;
                    next_pts = pts + (vvc1_amstream_dec_info.rate * repeat_count >> 1) * 15 / 16;
                } else {
                    vf->duration = vvc1_amstream_dec_info.rate >> 1;
                    next_pts = 0;
                }
            } else {
                vf->pts = next_pts;
                if ((repeat_count > 1) && avi_flag) {
                    vf->duration = vvc1_amstream_dec_info.rate * repeat_count >> 1;
                    if (next_pts != 0) {
                        next_pts += ((vf->duration) - ((vf->duration) >> 4));
                    }
                } else {
                    vf->duration = vvc1_amstream_dec_info.rate >> 1;
                    next_pts = 0;
                }
            }

            vf->duration_pulldown = 0;
            vf->type = (reg & BOTTOM_FIELD_FIRST_FLAG) ? VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
            vf->canvas0Addr = vf->canvas1Addr = index2canvas(buffer_index);

            set_aspect_ratio(vf, READ_MPEG_REG(VC1_PIC_RATIO));

            vfbuf_use[buffer_index]++;

            INCPTR(fill_ptr);
		vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
            vfpool_idx[fill_ptr] = buffer_index;
            vf = &vfpool[fill_ptr];
            vf->width = vvc1_amstream_dec_info.width;
            vf->height = vvc1_amstream_dec_info.height;
            vf->bufWidth = 1920;

            vf->pts = next_pts;
            if ((repeat_count > 1) && avi_flag) {
                vf->duration = vvc1_amstream_dec_info.rate * repeat_count >> 1;
                if (next_pts != 0) {
                    next_pts += ((vf->duration) - ((vf->duration) >> 4));
                }
            } else {
                vf->duration = vvc1_amstream_dec_info.rate >> 1;
                next_pts = 0;
            }

            vf->duration_pulldown = 0;
            vf->type = (reg & BOTTOM_FIELD_FIRST_FLAG) ? VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
            vf->canvas0Addr = vf->canvas1Addr = index2canvas(buffer_index);

            set_aspect_ratio(vf, READ_MPEG_REG(VC1_PIC_RATIO));

            vfbuf_use[buffer_index]++;

            INCPTR(fill_ptr);
            vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
        } else { // progressive
            vfpool_idx[fill_ptr] = buffer_index;
            vf = &vfpool[fill_ptr];
            vf->width = vvc1_amstream_dec_info.width;
            vf->height = vvc1_amstream_dec_info.height;
            vf->bufWidth = 1920;

            if (pts_valid) {
                vf->pts = pts;
                if ((repeat_count > 1) && avi_flag) {
                    vf->duration = vvc1_amstream_dec_info.rate * repeat_count;
                    next_pts = pts + (vvc1_amstream_dec_info.rate * repeat_count) * 15 / 16;
                } else {
                    vf->duration = vvc1_amstream_dec_info.rate;
                    next_pts = 0;
                }
            } else {
                vf->pts = next_pts;
                if ((repeat_count > 1) && avi_flag) {
                    vf->duration = vvc1_amstream_dec_info.rate * repeat_count;
                    if (next_pts != 0) {
                        next_pts += ((vf->duration) - ((vf->duration) >> 4));
                    }
                } else {
                    vf->duration = vvc1_amstream_dec_info.rate;
                    next_pts = 0;
                }
            }

            vf->duration_pulldown = 0;
            vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
            vf->canvas0Addr = vf->canvas1Addr = index2canvas(buffer_index);

            set_aspect_ratio(vf, READ_MPEG_REG(VC1_PIC_RATIO));

            vfbuf_use[buffer_index]++;

            INCPTR(fill_ptr);
            vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
        }

        total_frame++;

        //printk("PicType = %d, PTS = 0x%x, repeat count %d\n", picture_type, vf->pts, repeat_count);
        WRITE_MPEG_REG(VC1_BUFFEROUT, 0);
    }

    WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

#ifdef HANDLE_VC1_IRQ
    return IRQ_HANDLED;
#else
    return;
#endif
}

static vframe_t *vvc1_vf_peek(void* op_arg)
{
    if (get_ptr == fill_ptr) {
        return NULL;
    }

    return &vfpool[get_ptr];
}

static vframe_t *vvc1_vf_get(void* op_arg)
{
    vframe_t *vf;

    if (get_ptr == fill_ptr) {
        return NULL;
    }

    vf = &vfpool[get_ptr];

    INCPTR(get_ptr);

    return vf;
}

static void vvc1_vf_put(vframe_t *vf, void* op_arg)
{
    INCPTR(putting_ptr);
}

static int vvc1_vf_states(vframe_states_t *states, void* op_arg)
{
    unsigned long flags;
    int i;
    spin_lock_irqsave(&lock, flags);
    states->vf_pool_size = VF_POOL_SIZE;

    i = put_ptr - fill_ptr;
    if (i < 0) i += VF_POOL_SIZE;
    states->buf_free_num = i;
    
    i = putting_ptr - put_ptr;
    if (i < 0) i += VF_POOL_SIZE;
    states->buf_recycle_num = i;
    
    i = fill_ptr - get_ptr;
    if (i < 0) i += VF_POOL_SIZE;
    states->buf_avail_num = i;
    
    spin_unlock_irqrestore(&lock, flags);
    return 0;
}

static int vvc1_event_cb(int type, void *data, void *private_data)
{
    if(type & VFRAME_EVENT_RECEIVER_RESET){
        unsigned long flags;
        amvdec_stop();
#ifndef CONFIG_POST_PROCESS_MANAGER
        vf_light_unreg_provider(&vvc1_vf_prov);
#endif
        spin_lock_irqsave(&lock, flags);
        vvc1_local_init();
        vvc1_prot_init();
        spin_unlock_irqrestore(&lock, flags); 
#ifndef CONFIG_POST_PROCESS_MANAGER
        vf_reg_provider(&vvc1_vf_prov);
#endif              
        amvdec_start();
    }
    return 0;        
}

int vvc1_dec_status(struct vdec_status *vstatus)
{
    vstatus->width = vvc1_amstream_dec_info.width;
    vstatus->height = vvc1_amstream_dec_info.height;
    if (0 != vvc1_amstream_dec_info.rate) {
        vstatus->fps = 96000 / vvc1_amstream_dec_info.rate;
    } else {
        vstatus->fps = 96000;
    }
    vstatus->error_count = READ_MPEG_REG(AV_SCRATCH_4);
    vstatus->status = stat;

    return 0;
}

/****************************************/
static void vvc1_canvas_init(void)
{
    int i;
    u32 canvas_width, canvas_height;
    u32 decbuf_size, decbuf_y_size, decbuf_uv_size;
    u32 disp_addr = 0xffffffff;

    if (buf_size <= 0x00400000) {
        /* SD only */
        canvas_width = 768;
        canvas_height = 576;
        decbuf_y_size = 0x80000;
        decbuf_uv_size = 0x20000;
        decbuf_size = 0x100000;
    } else {
        /* HD & SD */
        canvas_width = 1920;
        canvas_height = 1088;
        decbuf_y_size = 0x200000;
        decbuf_uv_size = 0x80000;
        decbuf_size = 0x300000;
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
}

static void vvc1_prot_init(void)
{
    WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC | RESET_VLD_PART);
    READ_MPEG_REG(RESET0_REGISTER);
    WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC | RESET_VLD_PART);

    WRITE_MPEG_REG(RESET2_REGISTER, RESET_PIC_DC | RESET_DBLK);

    WRITE_MPEG_REG(POWER_CTL_VLD, 0x10);
    WRITE_MPEG_REG_BITS(VLD_MEM_VIFIFO_CONTROL, 2, MEM_FIFO_CNT_BIT, 2);
    WRITE_MPEG_REG_BITS(VLD_MEM_VIFIFO_CONTROL, 8, MEM_LEVEL_CNT_BIT, 6);

    vvc1_canvas_init();

    /* index v << 16 | u << 8 | y */
    WRITE_MPEG_REG(AV_SCRATCH_0, 0x020100);
    WRITE_MPEG_REG(AV_SCRATCH_1, 0x050403);
    WRITE_MPEG_REG(AV_SCRATCH_2, 0x080706);
    WRITE_MPEG_REG(AV_SCRATCH_3, 0x0b0a09);

    /* notify ucode the buffer offset */
    WRITE_MPEG_REG(AV_SCRATCH_F, buf_offset);

    /* disable PSCALE for hardware sharing */
    WRITE_MPEG_REG(PSCALE_CTRL, 0);

    WRITE_MPEG_REG(VC1_SOS_COUNT, 0);
    WRITE_MPEG_REG(VC1_BUFFERIN, 0);
    WRITE_MPEG_REG(VC1_BUFFEROUT, 0);

    /* clear mailbox interrupt */
    WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

    /* enable mailbox interrupt */
    WRITE_MPEG_REG(ASSIST_MBOX1_MASK, 1);

}

static void vvc1_local_init(void)
{
    int i;

    vvc1_ratio = vvc1_amstream_dec_info.ratio;

    avi_flag = (u32)vvc1_amstream_dec_info.param;

    fill_ptr = get_ptr = put_ptr = putting_ptr = 0;

    frame_width = frame_height = frame_dur = frame_prog = 0;

    total_frame = 0;

    next_pts = 0;

#ifdef DEBUG_PTS
    pts_hit = pts_missed = pts_i_hit = pts_i_missed = 0;
#endif

    for (i = 0; i < 4; i++) {
        vfbuf_use[i] = 0;
    }
}

#ifdef CONFIG_POST_PROCESS_MANAGER
static void vvc1_ppmgr_reset(void)
{
    vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_RESET,NULL);

    vvc1_local_init();

    //vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_START,NULL);
    
    printk("vvc1dec: vf_ppmgr_reset\n");
}
#endif

static void vvc1_put_timer_func(unsigned long arg)
{
    struct timer_list *timer = (struct timer_list *)arg;

#ifndef HANDLE_VC1_IRQ
    vvc1_isr();
#endif

#if 1
    if (READ_MPEG_REG(VC1_SOS_COUNT) > 10) {
        amvdec_stop();
#ifdef CONFIG_POST_PROCESS_MANAGER
        vvc1_ppmgr_reset();
#else 
        vf_light_unreg_provider(&vvc1_vf_prov);
        vvc1_local_init();
        vf_reg_provider(&vvc1_vf_prov);
#endif         
        vvc1_prot_init();
        amvdec_start();
    }
#endif

    if ((putting_ptr != put_ptr) && (READ_MPEG_REG(VC1_BUFFERIN) == 0)) {
        u32 index = vfpool_idx[put_ptr];

        if (--vfbuf_use[index] == 0) {
            WRITE_MPEG_REG(VC1_BUFFERIN, ~(1 << index));
        }

        INCPTR(put_ptr);
    }

    timer->expires = jiffies + PUT_INTERVAL;

    add_timer(timer);
}

static s32 vvc1_init(void)
{
    printk("vvc1_init\n");
    init_timer(&recycle_timer);

    stat |= STAT_TIMER_INIT;

    intra_output = 0;
    amvdec_enable();

    vvc1_local_init();

    if (vvc1_amstream_dec_info.format == VIDEO_DEC_FORMAT_WMV3) {
        printk("WMV3 dec format\n");
        vvc1_format = VIDEO_DEC_FORMAT_WMV3;
        WRITE_MPEG_REG(AV_SCRATCH_4, 0);
    } else if (vvc1_amstream_dec_info.format == VIDEO_DEC_FORMAT_WVC1) {
        printk("WVC1 dec format\n");
        vvc1_format = VIDEO_DEC_FORMAT_WVC1;
        WRITE_MPEG_REG(AV_SCRATCH_4, 1);
    } else {
        printk("not supported VC1 format\n");
    }

    if (amvdec_loadmc(vc1_mc) < 0) {
        amvdec_disable();

        printk("failed\n");
        return -EBUSY;
    }

    stat |= STAT_MC_LOAD;

    /* enable AMRISC side protocol */
    vvc1_prot_init();

#ifdef HANDLE_VC1_IRQ
    if (request_irq(INT_MAILBOX_1A, vvc1_isr,
                    IRQF_SHARED, "vvc1-irq", (void *)vvc1_dec_id)) {
        amvdec_disable();

        printk("vvc1 irq register error.\n");
        return -ENOENT;
    }
#endif

    stat |= STAT_ISR_REG;
 #ifdef CONFIG_POST_PROCESS_MANAGER
    vf_provider_init(&vvc1_vf_prov, PROVIDER_NAME, &vvc1_vf_provider, NULL);
    vf_reg_provider(&vvc1_vf_prov);
    vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_START,NULL);
 #else 
    vf_provider_init(&vvc1_vf_prov, PROVIDER_NAME, &vvc1_vf_provider, NULL);
    vf_reg_provider(&vvc1_vf_prov);
 #endif 

    stat |= STAT_VF_HOOK;

    recycle_timer.data = (ulong) & recycle_timer;
    recycle_timer.function = vvc1_put_timer_func;
    recycle_timer.expires = jiffies + PUT_INTERVAL;

    add_timer(&recycle_timer);

    stat |= STAT_TIMER_ARM;

    amvdec_start();

    stat |= STAT_VDEC_RUN;

    set_vdec_func(&vvc1_dec_status);

    return 0;
}

static int amvdec_vc1_probe(struct platform_device *pdev)
{
    struct resource *mem;

    if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0))) {
        printk("amvdec_vc1 memory resource undefined.\n");
        return -EFAULT;
    }

    buf_start = mem->start;
    buf_size = mem->end - mem->start + 1;
    buf_offset = buf_start - ORI_BUFFER_START_ADDR;

    memcpy(&vvc1_amstream_dec_info, (void *)mem[1].start, sizeof(vvc1_amstream_dec_info));

    if (vvc1_init() < 0) {
        printk("amvdec_vc1 init failed.\n");

        return -ENODEV;
    }

    return 0;
}

static int amvdec_vc1_remove(struct platform_device *pdev)
{
    if (stat & STAT_VDEC_RUN) {
        amvdec_stop();
        stat &= ~STAT_VDEC_RUN;
    }

    if (stat & STAT_ISR_REG) {
        free_irq(INT_MAILBOX_1A, (void *)vvc1_dec_id);
        stat &= ~STAT_ISR_REG;
    }

    if (stat & STAT_TIMER_ARM) {
        del_timer_sync(&recycle_timer);
        stat &= ~STAT_TIMER_ARM;
    }

    if (stat & STAT_VF_HOOK) {
        ulong flags;
        spin_lock_irqsave(&lock, flags);
        fill_ptr = get_ptr = put_ptr = putting_ptr = 0;
        spin_unlock_irqrestore(&lock, flags);
        vf_unreg_provider(&vvc1_vf_prov);
        stat &= ~STAT_VF_HOOK;
    }

    amvdec_disable();

#ifdef DEBUG_PTS
    printk("pts hit %d, pts missed %d, i hit %d, missed %d\n", pts_hit, pts_missed, pts_i_hit, pts_i_missed);
    printk("total frame %d, avi_flag %d, rate %d\n", total_frame, avi_flag, vvc1_amstream_dec_info.rate);
#endif

    return 0;
}

/****************************************/

static struct platform_driver amvdec_vc1_driver = {
    .probe  = amvdec_vc1_probe,
    .remove = amvdec_vc1_remove,
#ifdef CONFIG_PM
    .suspend = amvdec_suspend,
    .resume  = amvdec_resume,
#endif
    .driver = {
        .name = DRIVER_NAME,
    }
};

#ifdef CONFIG_ARCH_MESON3
static struct codec_profile_t amvdec_vc1_profile = {
	.name = "vc1",
	.profile = "progressive, interlace, wmv3"
};
#else
static struct codec_profile_t amvdec_vc1_profile = {
	.name = "vc1",
	.profile = "progressive, wmv3"
};
#endif

static int __init amvdec_vc1_driver_init_module(void)
{
    printk("amvdec_vc1 module init\n");

    if (platform_driver_register(&amvdec_vc1_driver)) {
        printk("failed to register amvdec_vc1 driver\n");
        return -ENODEV;
    }
	vcodec_profile_register(&amvdec_vc1_profile);
    return 0;
}

static void __exit amvdec_vc1_driver_remove_module(void)
{
    printk("amvdec_vc1 module remove.\n");

    platform_driver_unregister(&amvdec_vc1_driver);
}

/****************************************/

module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_vc1 stat \n");

module_init(amvdec_vc1_driver_init_module);
module_exit(amvdec_vc1_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC VC1 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qi Wang <qi.wang@amlogic.com>");

