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
 * Author:  Chen Zhang <chen.zhang@amlogic.com>
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
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <asm/atomic.h>

#include <mach/am_regs.h>

#include "amvdec.h"
#include "vh264_mc.h"

#define DRIVER_NAME "amvdec_h264"
#define MODULE_NAME "amvdec_h264"

#define HANDLE_H264_IRQ
#define DEBUG_PTS
#define DROP_B_FRAME_FOR_1080P_50_60FPS

/* 12M for L41 */
#define MAX_DPB_BUFF_SIZE       (12*1024*1024)
#define DEFAULT_MEM_SIZE        (32*1024*1024)
#define AVIL_DPB_BUFF_SIZE      0x01ec2000

#define DEF_BUF_START_ADDR            0x81000000
#define MEM_HEADER_CPU_BASE           (0x81110000 + buf_offset)
#define MEM_DATA_CPU_BASE             (0x81111000 + buf_offset)
#define MEM_MMCO_CPU_BASE             (0x81112000 + buf_offset)
#define MEM_LIST_CPU_BASE             (0x81113000 + buf_offset)
#define MEM_SLICE_CPU_BASE            (0x81114000 + buf_offset)
#define MEM_SWAP_SIZE                 (0x5000*4)
#define V_BUF_ADDR_START              0x8113e000

#define PIC_SINGLE_FRAME        0
#define PIC_TOP_BOT_TOP         1
#define PIC_BOT_TOP_BOT         2
#define PIC_DOUBLE_FRAME        3
#define PIC_TRIPLE_FRAME        4
#define PIC_TOP_BOT              5
#define PIC_BOT_TOP              6
#define PIC_INVALID              7

#define EXTEND_SAR                      0xff

#define VF_POOL_SIZE        72
#define VF_BUF_NUM          24
#define PUT_INTERVAL        (HZ/100)
#define NO_DISP_WD_COUNT    (3 * HZ / PUT_INTERVAL)

#define STAT_TIMER_INIT     0x01
#define STAT_MC_LOAD        0x02
#define STAT_ISR_REG        0x04
#define STAT_VF_HOOK        0x08
#define STAT_TIMER_ARM      0x10
#define STAT_VDEC_RUN       0x20

#define DEC_CONTROL_FLAG_FORCE_2997_1080P_INTERLACE 0x0001
#define DEC_CONTROL_FLAG_FORCE_2500_576P_INTERLACE  0x0002

#define INCPTR(p) ptr_atomic_wrap_inc(&p)

typedef struct {
    unsigned int y_addr;
    unsigned int u_addr;
    unsigned int v_addr;

    int y_canvas_index;
    int u_canvas_index;
    int v_canvas_index;
} buffer_spec_t;

#define spec2canvas(x)  \
    (((x)->v_canvas_index << 16) | \
     ((x)->u_canvas_index << 8)  | \
     ((x)->y_canvas_index << 0))

static vframe_t *vh264_vf_peek(void*);
static vframe_t *vh264_vf_get(void*);
static void vh264_vf_put(vframe_t *, void*);
static int  vh264_vf_states(vframe_states_t *states, void*);
static int vh264_event_cb(int type, void *data, void *private_data);

static void vh264_prot_init(void);
static void vh264_local_init(void);
static void vh264_put_timer_func(unsigned long arg);

static const char vh264_dec_id[] = "vh264-dev";

#define PROVIDER_NAME   "decoder.h264"

static const struct vframe_operations_s vh264_vf_provider = {
    .peek = vh264_vf_peek,
    .get = vh264_vf_get,
    .put = vh264_vf_put,
    .event_cb = vh264_event_cb,
    .vf_states = vh264_vf_states,
};
static struct vframe_provider_s vh264_vf_prov;

static u32 frame_buffer_size;
static u32 frame_width, frame_height, frame_dur, frame_prog;
static struct vframe_s vfpool[VF_POOL_SIZE];
static u32 vfpool_idx[VF_POOL_SIZE];
static s32 vfbuf_use[VF_BUF_NUM];
static s32 fill_ptr, get_ptr, putting_ptr, put_ptr;
static unsigned char buffer_for_recycle[VF_BUF_NUM];
static int buffer_for_recycle_rd, buffer_for_recycle_wr;
static buffer_spec_t buffer_spec[VF_BUF_NUM];

static struct timer_list recycle_timer;
static u32 stat;
static u32 buf_start, buf_size;
static s32 buf_offset;
static u32 pts_outside = 0;
static u32 sync_outside = 0;
static u32 dec_control = 0;
static u32 vh264_ratio;
static u32 vh264_rotation;

static u32 seq_info;
static u32 aspect_ratio_info;
static u32 num_units_in_tick;
static u32 time_scale;
static u32 h264_ar;
#ifdef DROP_B_FRAME_FOR_1080P_50_60FPS
static u32 last_interlaced;
#endif
static u8 neg_poc_counter;
static unsigned char h264_first_pts_ready;
static u32 h264pts1, h264pts2;
static u32 h264_pts_count, duration_from_pts_done;
static u32 vh264_error_count;
static u32 vh264_no_disp_count;
#if 0
static u32 vh264_no_disp_wd_count;
#endif
static u32 vh264_running;
static struct vframe_s *p_last_vf;
static s32 last_ptr;
static u32 wait_buffer_counter;
static uint error_recovery_mode = 0;

#ifdef DEBUG_PTS
static unsigned long pts_missed, pts_hit;
#endif

static atomic_t vh264_active = ATOMIC_INIT(0);
static struct work_struct error_wd_work;

static struct dec_sysinfo vh264_amstream_dec_info;
extern u32 trickmode_i;

static spinlock_t lock = SPIN_LOCK_UNLOCKED;

static int vh264_stop(void);
static s32 vh264_init(void);

void spec_set_canvas(buffer_spec_t *spec,
                     unsigned width,
                     unsigned height)
{
    canvas_config(spec->y_canvas_index,
                  spec->y_addr,
                  width,
                  height,
                  CANVAS_ADDR_NOWRAP,
                  CANVAS_BLKMODE_32X32);

    canvas_config(spec->u_canvas_index,
                  spec->u_addr,
                  width / 2,
                  height / 2,
                  CANVAS_ADDR_NOWRAP,
                  CANVAS_BLKMODE_32X32);

    canvas_config(spec->v_canvas_index,
                  spec->v_addr,
                  width / 2,
                  height / 2,
                  CANVAS_ADDR_NOWRAP,
                  CANVAS_BLKMODE_32X32);

    return;
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

static vframe_t *vh264_vf_peek(void* op_arg)
{
    if (get_ptr == fill_ptr) {
        return NULL;
    }

    return &vfpool[get_ptr];
}

static vframe_t *vh264_vf_get(void* op_arg)
{
    vframe_t *vf;

    if (get_ptr == fill_ptr) {
        return NULL;
    }

    vf = &vfpool[get_ptr];

    INCPTR(get_ptr);

    return vf;
}

static void vh264_vf_put(vframe_t *vf, void* op_arg)
{
    INCPTR(putting_ptr);
}

static int vh264_event_cb(int type, void *data, void *private_data)
{
#if 0  // currently for h264, disable it
    if(type & VFRAME_EVENT_RECEIVER_RESET){
        unsigned long flags;
        amvdec_stop();
#ifndef CONFIG_POST_PROCESS_MANAGER
        vf_light_unreg_provider(&vh264_vf_provider);
#endif
        spin_lock_irqsave(&lock, flags);
        const struct vframe_receiver_op_s *vf_receiver_bak = vf_receiver;
        vh264_local_init();
        vf_receiver = vf_receiver_bak;
        vh264_prot_init();
        spin_unlock_irqrestore(&lock, flags); 
#ifndef CONFIG_POST_PROCESS_MANAGER
        vf_reg_provider(&vh264_vf_provider);
#endif              
        amvdec_start();
    }
#endif
    return 0;        
}

static int  vh264_vf_states(vframe_states_t *states, void* op_arg)
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

static void set_frame_info(vframe_t *vf)
{
    unsigned int ar = 0;

    vf->width = frame_width;
    vf->height = frame_height;
    vf->duration = frame_dur;
    vf->orientation = vh264_rotation;

    if (vh264_ratio == 0) {
        vf->ratio_control |= (0x90 << DISP_RATIO_ASPECT_RATIO_BIT); // always stretch to 16:9
    } else {
        //h264_ar = ((float)frame_height/frame_width)*customer_ratio;
        ar = min(h264_ar, (u32)DISP_RATIO_ASPECT_RATIO_MAX);

        vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
    }

    return;
}

#ifdef CONFIG_POST_PROCESS_MANAGER
static void vh264_ppmgr_reset(void)
{
    vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_RESET,NULL);

    vh264_local_init();

    //vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_START,NULL);
    
    printk("vh264dec: vf_ppmgr_reset\n");
}
#endif

#ifdef HANDLE_H264_IRQ
static irqreturn_t vh264_isr(int irq, void *dev_id)
#else
static void vh264_isr(void)
#endif
{
    unsigned int buffer_index;
    vframe_t *vf;
    unsigned int cpu_cmd;
    unsigned int pts, pts_valid = 0, pts_duration = 0;
    bool force_interlaced_frame = false;

    WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

    if (0 == (stat & STAT_VDEC_RUN)) {
        printk("decoder is not running\n");
#ifdef HANDLE_H264_IRQ
        return IRQ_HANDLED;
#else
        return;
#endif
    }

    cpu_cmd = READ_MPEG_REG(AV_SCRATCH_0);

#ifdef DROP_B_FRAME_FOR_1080P_50_60FPS
    if((frame_dur < 2000) && 
       (frame_width >= 1400) &&
       (frame_height >= 1000) &&
       (last_interlaced == 0)) {
        SET_MPEG_REG_MASK(AV_SCRATCH_F, 0x8);
    }
#endif

    if ((cpu_cmd & 0xff) == 1) {
        int timing_info_present_flag, aspect_ratio_info_present_flag, aspect_ratio_idc;
        int mb_width, mb_total, max_dpb_size, actual_dpb_size, max_reference_size;
        int i, mb_height, mb_mv_byte;
        unsigned addr;
        unsigned int post_canvas;

        if (vh264_running) {
#ifdef CONFIG_POST_PROCESS_MANAGER
            vh264_ppmgr_reset();
#else 
            vf_light_unreg_provider(&vh264_vf_prov);
            vh264_local_init();
            vf_reg_provider(&vh264_vf_prov);
#endif       	
            WRITE_MPEG_REG(AV_SCRATCH_7, 0);
            WRITE_MPEG_REG(AV_SCRATCH_8, 0);
            WRITE_MPEG_REG(AV_SCRATCH_9, 0);
            vh264_running = 0;
        }

        h264_first_pts_ready = 0;
        buffer_for_recycle_rd = 0;
        buffer_for_recycle_wr = 0;

        post_canvas = get_post_canvas();

        mb_width = READ_MPEG_REG(AV_SCRATCH_1);
        seq_info = READ_MPEG_REG(AV_SCRATCH_2);
        aspect_ratio_info = READ_MPEG_REG(AV_SCRATCH_3);
        num_units_in_tick = READ_MPEG_REG(AV_SCRATCH_4);
        time_scale = READ_MPEG_REG(AV_SCRATCH_5);

        mb_total = (mb_width >> 8) & 0xffff;
        max_reference_size = (mb_width >> 24) & 0x7f;
        mb_mv_byte = (mb_width & 0x80000000) ? 24 : 96;
        mb_width = mb_width & 0xff;
        mb_height = mb_total / mb_width;

        if (frame_width == 0 || frame_height == 0) {
            frame_width = mb_width << 4;
            frame_height = mb_height << 4;
            if (frame_height == 1088) {
                frame_height = 1080;
            }
        }

        mb_width = (mb_width + 3) & 0xfffffffc;
        mb_height = (mb_height + 3) & 0xfffffffc;
        mb_total = mb_width * mb_height;

        max_dpb_size = (frame_buffer_size - mb_total * 384 * 4 - mb_total * mb_mv_byte) / (mb_total * 384 + mb_total * mb_mv_byte);
        if (max_reference_size <= max_dpb_size) {
            max_dpb_size = MAX_DPB_BUFF_SIZE / (mb_total * 384);
            if (max_dpb_size > 16) {
                max_dpb_size = 16;
            }

            if (max_reference_size < max_dpb_size) {
                max_reference_size = max_dpb_size + 1;
            } else {
                max_dpb_size = max_reference_size;
                max_reference_size++;
            }
        } else {
            max_dpb_size = max_reference_size;
            max_reference_size++;
        }

        if (mb_total * 384 * (max_dpb_size + 3) + mb_total * mb_mv_byte * max_reference_size > frame_buffer_size) {
            max_dpb_size = (frame_buffer_size - mb_total * 384 * 3 - mb_total * mb_mv_byte) / (mb_total * 384 + mb_total * mb_mv_byte);
            max_reference_size = max_dpb_size + 1;
        }

        actual_dpb_size = (frame_buffer_size - mb_total * mb_mv_byte * max_reference_size) / (mb_total * 384);
        if (actual_dpb_size > 24) {
            actual_dpb_size = 24;
        }

        if (max_dpb_size > 5) {
            if (actual_dpb_size < max_dpb_size + 3) {
                actual_dpb_size = max_dpb_size + 3;
                max_reference_size = (frame_buffer_size - mb_total * 384 * actual_dpb_size) / (mb_total * mb_mv_byte);
            }
        } else {
            if (actual_dpb_size < max_dpb_size + 4) {
                actual_dpb_size = max_dpb_size + 4;
                max_reference_size = (frame_buffer_size - mb_total * 384 * actual_dpb_size) / (mb_total * mb_mv_byte);
            }
        }

        if (!(READ_MPEG_REG(AV_SCRATCH_F) & 0x1)) {
            addr = buf_start;

            if (actual_dpb_size <= 21) {
                for (i = 0 ; i < actual_dpb_size ; i++) {
                    buffer_spec[i].y_addr = addr;
                    addr += mb_total << 8;
                    buffer_spec[i].u_addr = addr;
                    addr += mb_total << 6;
                    buffer_spec[i].v_addr = addr;
                    addr += mb_total << 6;
                    vfbuf_use[i] = 0;

                    buffer_spec[i].y_canvas_index = 128 + i * 3;
                    buffer_spec[i].u_canvas_index = 128 + i * 3 + 1;
                    buffer_spec[i].v_canvas_index = 128 + i * 3 + 2;

                    canvas_config(128 + i * 3, buffer_spec[i].y_addr, mb_width << 4, mb_height << 4,
                                  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                    canvas_config(128 + i * 3 + 1, buffer_spec[i].u_addr, mb_width << 3, mb_height << 3,
                                  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                    canvas_config(128 + i * 3 + 2, buffer_spec[i].v_addr, mb_width << 3, mb_height << 3,
                                  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                    WRITE_MPEG_REG(ANC0_CANVAS_ADDR + i, spec2canvas(&buffer_spec[i]));
                }
            } else {
                for (i = 0 ; i < 21 ; i++) {
                    buffer_spec[i].y_addr = addr;
                    addr += mb_total << 8;
                    buffer_spec[i].u_addr = addr;
                    addr += mb_total << 6;
                    buffer_spec[i].v_addr = addr;
                    addr += mb_total << 6;
                    vfbuf_use[i] = 0;

                    buffer_spec[i].y_canvas_index = 128 + i * 3;
                    buffer_spec[i].u_canvas_index = 128 + i * 3 + 1;
                    buffer_spec[i].v_canvas_index = 128 + i * 3 + 2;

                    canvas_config(128 + i * 3, buffer_spec[i].y_addr, mb_width << 4, mb_height << 4,
                                  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                    canvas_config(128 + i * 3 + 1, buffer_spec[i].u_addr, mb_width << 3, mb_height << 3,
                                  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                    canvas_config(128 + i * 3 + 2, buffer_spec[i].v_addr, mb_width << 3, mb_height << 3,
                                  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                    WRITE_MPEG_REG(ANC0_CANVAS_ADDR + i, spec2canvas(&buffer_spec[i]));
                }

                for (i = 21 ; i < actual_dpb_size ; i++) {
                    buffer_spec[i].y_canvas_index = 3 * (i - 21) + 0;
                    buffer_spec[i].y_addr = addr;
                    addr += mb_total << 8;
                    buffer_spec[i].u_canvas_index = 3 * (i - 21) + 1;
                    buffer_spec[i].u_addr = addr;
                    addr += mb_total << 6;
                    buffer_spec[i].v_canvas_index = 3 * (i - 21) + 2;
                    buffer_spec[i].v_addr = addr;
                    addr += mb_total << 6;
                    vfbuf_use[i] = 0;

                    spec_set_canvas(&buffer_spec[i], mb_width << 4, mb_height << 4);
                    WRITE_MPEG_REG(ANC0_CANVAS_ADDR + i, spec2canvas(&buffer_spec[i]));
                }
            }
        } else {
            addr = buf_start + mb_total * 384 * actual_dpb_size;
        }

        timing_info_present_flag = seq_info & 0x2;
        aspect_ratio_info_present_flag = seq_info & 0x1;
        aspect_ratio_idc = (seq_info >> 16) & 0xff;

        if (timing_info_present_flag) {
            if ((num_units_in_tick * 120) >= time_scale && (!sync_outside)) {
                frame_dur = div_u64(96000ULL * 2 * num_units_in_tick, time_scale);
            }
        }

        if (aspect_ratio_info_present_flag) {
            if (aspect_ratio_idc == EXTEND_SAR) {
                printk("v264dec: aspect_ratio_idc = EXTEND_SAR, aspect_ratio_info = 0x%x\n", aspect_ratio_info);
                h264_ar = 0x100 * (aspect_ratio_info >> 16) * frame_height / ((aspect_ratio_info & 0xffff) * frame_width);
            } else {
                printk("v264dec: aspect_ratio_idc = %d\n", aspect_ratio_idc);

                switch (aspect_ratio_idc) {
                case 1:
                    h264_ar = 0x100 * frame_height / frame_width;
                    break;
                case 2:
                    h264_ar = 0x100 * frame_height * 11 / (frame_width * 12);
                    break;
                case 3:
                    h264_ar = 0x100 * frame_height * 11 / (frame_width * 10);
                    break;
                case 4:
                    h264_ar = 0x100 * frame_height * 11 / (frame_width * 16);
                    break;
                case 5:
                    h264_ar = 0x100 * frame_height * 33 / (frame_width * 40);
                    break;
                case 6:
                    h264_ar = 0x100 * frame_height * 11 / (frame_width * 24);
                    break;
                case 7:
                    h264_ar = 0x100 * frame_height * 11 / (frame_width * 20);
                    break;
                case 8:
                    h264_ar = 0x100 * frame_height * 11 / (frame_width * 32);
                    break;
                case 9:
                    h264_ar = 0x100 * frame_height * 33 / (frame_width * 80);
                    break;
                case 10:
                    h264_ar = 0x100 * frame_height * 11 / (frame_width * 18);
                    break;
                case 11:
                    h264_ar = 0x100 * frame_height * 11 / (frame_width * 15);
                    break;
                case 12:
                    h264_ar = 0x100 * frame_height * 33 / (frame_width * 64);
                    break;
                case 13:
                    h264_ar = 0x100 * frame_height * 99 / (frame_width * 160);
                    break;
                case 14:
                    h264_ar = 0x100 * frame_height * 3 / (frame_width * 4);
                    break;
                case 15:
                    h264_ar = 0x100 * frame_height * 2 / (frame_width * 3);
                    break;
                case 16:
                    h264_ar = 0x100 * frame_height * 1 / (frame_width * 2);
                    break;
                default:
                    h264_ar = frame_height * vh264_ratio / frame_width;
                    break;
                }
            }
        } else {
            printk("v264dec: aspect_ratio not available from source\n");
            h264_ar = frame_height * vh264_ratio / frame_width;
        }

        WRITE_MPEG_REG(AV_SCRATCH_1, addr);
        //WRITE_MPEG_REG(AV_SCRATCH_2, (unsigned)vpts_map);
        WRITE_MPEG_REG(AV_SCRATCH_3, post_canvas); // should be modified later
        addr += mb_total * mb_mv_byte * max_reference_size;
        WRITE_MPEG_REG(AV_SCRATCH_4, addr);
        WRITE_MPEG_REG(AV_SCRATCH_0, (max_reference_size << 24) | (actual_dpb_size << 16) | (max_dpb_size << 8));
    } else if ((cpu_cmd & 0xff) == 2) {
        int pic_struct_present, pic_struct, prog_frame, poc_sel, idr_flag, neg_poc;
        int i, status, num_frame, b_offset;
        int current_error_count;

        vh264_running = 1;
        vh264_no_disp_count = 0;
        num_frame = (cpu_cmd >> 8) & 0xff;
        pic_struct_present = seq_info & 0x10;

        current_error_count = READ_MPEG_REG(AV_SCRATCH_D);
        if (vh264_error_count != current_error_count) {
            //printk("decoder error happened, count %d\n", current_error_count);
            vh264_error_count = current_error_count;
        }

        for (i = 0 ; i < num_frame ; i++) {
            status = READ_MPEG_REG(AV_SCRATCH_1 + i);
            buffer_index = status & 0x1f;

            if ((p_last_vf != NULL) && (vfpool_idx[last_ptr] == buffer_index)) {
                continue;
            }

            if (buffer_index >= VF_BUF_NUM) {
                buffer_for_recycle[buffer_for_recycle_wr++] = vfpool_idx[put_ptr] + 1;
                if (buffer_for_recycle_wr == VF_BUF_NUM) {
                    buffer_for_recycle_wr = 0;
                }

                continue;
            }

            pic_struct = (status >> 5) & 0x7;
            prog_frame = status & 0x100;
            poc_sel = status & 0x200;
            idr_flag = status & 0x400;
            neg_poc = status & 0x800;
            b_offset = (status >> 16) & 0xffff;
#ifdef DROP_B_FRAME_FOR_1080P_50_60FPS
            last_interlaced = prog_frame ? 0 : 1;
#endif
            vf = &vfpool[fill_ptr];
            vfpool_idx[fill_ptr] = buffer_index;
            vf->ratio_control = 0;
            set_frame_info(vf);

            switch (i) {
            case 0:
                b_offset |= (READ_MPEG_REG(AV_SCRATCH_A) & 0xffff) << 16;
                break;
            case 1:
                b_offset |= READ_MPEG_REG(AV_SCRATCH_A) & 0xffff0000;
                break;
            case 2:
                b_offset |= (READ_MPEG_REG(AV_SCRATCH_B) & 0xffff) << 16;
                break;
            case 3:
                b_offset |= READ_MPEG_REG(AV_SCRATCH_B) & 0xffff0000;
                break;
            case 4:
                b_offset |= (READ_MPEG_REG(AV_SCRATCH_C) & 0xffff) << 16;
                break;
            case 5:
                b_offset |= READ_MPEG_REG(AV_SCRATCH_C) & 0xffff0000;
                break;
            default:
                break;
            }

            if (pts_lookup_offset(PTS_TYPE_VIDEO, b_offset, &pts, 0) == 0) {
                pts_valid = 1;
#ifdef DEBUG_PTS
                pts_hit++;
#endif
            } else {
                pts_valid = 0;
#ifdef DEBUG_PTS
                pts_missed++;
#endif
            }

            if (sync_outside == 0) {
                if (h264_first_pts_ready == 0) {
                    if (pts_valid == 0) {
                        buffer_for_recycle[buffer_for_recycle_wr++] = vfpool_idx[put_ptr] + 1;
                        if (buffer_for_recycle_wr == VF_BUF_NUM) {
                            buffer_for_recycle_wr = 0;
                        }

                        continue;
                    }

                    h264pts1 = pts;
                    h264_pts_count = 0;
                    h264_first_pts_ready = 1;
                } else {
                    if (pts_valid && (pts > h264pts1)) {
                        if (duration_from_pts_done == 0) {
                            h264pts2 = pts;
                            pts_duration = ((h264pts2 - h264pts1) / h264_pts_count) * 16 / 15;
                            duration_from_pts_done = 1;

                            if ((pts_duration != frame_dur) && (!pts_outside)) {
                                frame_dur = pts_duration;
                            }
                        }
                    }
                }

                h264_pts_count++;
            } else {
                if (idr_flag && pts_valid) {
                    pts += neg_poc_counter * (frame_dur - (frame_dur >> 4));
                } else {
                    pts = 0;
                }

                if (neg_poc) {
                    neg_poc_counter++;
                } else {
                    neg_poc_counter = 0;
                }
            }

            if ((dec_control & DEC_CONTROL_FLAG_FORCE_2997_1080P_INTERLACE) &&
                (frame_width == 1920) &&
                (frame_height >= 1080) &&
                (vf->duration == 3203)) {
                force_interlaced_frame = true;
            }
            else if ((dec_control & DEC_CONTROL_FLAG_FORCE_2500_576P_INTERLACE) &&
                (frame_width == 720) &&
                (frame_height == 576) &&
                (vf->duration == 3840)) {
                force_interlaced_frame = true;
            }

            if ((!force_interlaced_frame) &&
                (prog_frame || (pic_struct_present && pic_struct <= PIC_TRIPLE_FRAME))) {
                if (pic_struct_present) {
                    if (pic_struct == PIC_TOP_BOT_TOP || pic_struct == PIC_BOT_TOP_BOT) {
                        vf->duration += vf->duration >> 1;
                    } else if (pic_struct == PIC_DOUBLE_FRAME) {
                        vf->duration += vf->duration;
                    } else if (pic_struct == PIC_TRIPLE_FRAME) {
                        vf->duration += vf->duration << 1;
                    }
                }

                vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
                vf->duration_pulldown = 0;

                vf->pts = (pts_valid) ? pts : 0;

                vf->canvas0Addr = vf->canvas1Addr = spec2canvas(&buffer_spec[buffer_index]);
                vfbuf_use[buffer_index]++;

                p_last_vf = vf;
                last_ptr = fill_ptr;

                INCPTR(fill_ptr);
                vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
            } else {
                if (pic_struct_present && pic_struct == PIC_TOP_BOT) {
                    vf->type = VIDTYPE_INTERLACE_TOP;
                } else if (pic_struct_present && pic_struct == PIC_BOT_TOP) {
                    vf->type = VIDTYPE_INTERLACE_BOTTOM;
                } else {
                    vf->type = poc_sel ? VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
                }

                vf->type |= VIDTYPE_INTERLACE_FIRST;

                vf->duration >>= 1;
                vf->duration_pulldown = 0;

                vf->pts = (pts_valid) ? pts : 0;

                vf->canvas0Addr = vf->canvas1Addr = spec2canvas(&buffer_spec[buffer_index]);
                vfbuf_use[buffer_index]++;

                INCPTR(fill_ptr);

                vf = &vfpool[fill_ptr];
                vfpool_idx[fill_ptr] = buffer_index;

                vf->ratio_control = 0;
                set_frame_info(vf);

                if (pic_struct_present && pic_struct == PIC_TOP_BOT) {
                    vf->type = VIDTYPE_INTERLACE_BOTTOM;
                } else if (pic_struct_present && pic_struct == PIC_BOT_TOP) {
                    vf->type = VIDTYPE_INTERLACE_TOP;
                } else {
                    vf->type = poc_sel ? VIDTYPE_INTERLACE_TOP : VIDTYPE_INTERLACE_BOTTOM;
                }
                
                if ((READ_MPEG_REG(AV_SCRATCH_F) & 3) == 2)
                    vf->type = VIDTYPE_INTERLACE_TOP;

                vf->duration >>= 1;
                vf->duration_pulldown = 0;

                vf->pts = 0;

                vf->canvas0Addr = vf->canvas1Addr = spec2canvas(&buffer_spec[buffer_index]);
                vfbuf_use[buffer_index]++;

                p_last_vf = vf;
                last_ptr = fill_ptr;

                INCPTR(fill_ptr);
                vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
            }
        }

        WRITE_MPEG_REG(AV_SCRATCH_0, 0);
    } else if ((cpu_cmd & 0xff) == 3) {
        vh264_running = 1;
        // check all picture displayed then black screen
        WRITE_MPEG_REG(AV_SCRATCH_0, 0);
    } else if ((cpu_cmd & 0xff) == 4) {
        vh264_running = 1;
        // reserved for slice group
        WRITE_MPEG_REG(AV_SCRATCH_0, 0);
    } else if ((cpu_cmd & 0xff) == 5) {
        vh264_running = 1;
        // reserved for slice group
        WRITE_MPEG_REG(AV_SCRATCH_0, 0);
    } else if ((cpu_cmd & 0xff) == 6) {
        vh264_running = 0;
        // this is fatal error, need restart
        printk("fatal error happend\n");
        schedule_work(&error_wd_work);
    }

#ifdef HANDLE_H264_IRQ
    return IRQ_HANDLED;
#else
    return;
#endif
}

static void vh264_put_timer_func(unsigned long arg)
{
    struct timer_list *timer = (struct timer_list *)arg;
    unsigned int wait_buffer_status;
    unsigned int wait_i_pass_frames;
    unsigned int reg_val;
    receviver_start_e state = RECEIVER_INACTIVE ;
    if (vf_get_receiver(PROVIDER_NAME)){
        state = vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_QUREY_STATE,NULL);
        if((state == RECEIVER_STATE_NULL)||(state == RECEIVER_STATE_NONE)){
            /* receiver has no event_cb or receiver's event_cb does not process this event */
            state  = RECEIVER_INACTIVE ;    
        }
    }else{
         state  = RECEIVER_INACTIVE ;
    }
#ifndef HANDLE_H264_IRQ
    vh264_isr();
#endif

    reg_val = READ_MPEG_REG(AV_SCRATCH_9);
    wait_buffer_status = reg_val & (1 << 31);
    wait_i_pass_frames = reg_val & 0xff;
    if (wait_buffer_status) {
        if ((get_ptr == fill_ptr)
            && (putting_ptr == put_ptr)
            && (buffer_for_recycle_rd == buffer_for_recycle_wr)
            &&(state == RECEIVER_INACTIVE)) {
            printk("$$$$$$decoder is waiting for buffer\n");
            if (++wait_buffer_counter > 2) {
                amvdec_stop();
                
#ifdef CONFIG_POST_PROCESS_MANAGER
                vh264_ppmgr_reset();
#else 
                vf_light_unreg_provider(&vh264_vf_prov);
                vh264_local_init();
                vf_reg_provider(&vh264_vf_prov);
#endif                            
                vh264_prot_init();
                amvdec_start();
            }
        }
    } else if (wait_i_pass_frames > 10) {
        printk("i passed frames > 10\n");
        amvdec_stop();
#ifdef CONFIG_POST_PROCESS_MANAGER
        vh264_ppmgr_reset();
#else 
        vf_light_unreg_provider(&vh264_vf_prov);
        vh264_local_init();
        vf_reg_provider(&vh264_vf_prov);
#endif
        vh264_prot_init();
        amvdec_start();
    }

#if 0
    if (!wait_buffer_status) {
        if (vh264_no_disp_count++ > NO_DISP_WD_COUNT) {
            printk("$$$decoder did not send frame out\n");
            amvdec_stop();
#ifdef CONFIG_POST_PROCESS_MANAGER
            vh264_ppmgr_reset();
#else
            vf_light_unreg_provider(&vh264_vf_prov);
            vh264_local_init();
            vf_reg_provider(vh264_vf_prov);
#endif
            vh264_prot_init();
            amvdec_start();

            vh264_no_disp_count = 0;
            vh264_no_disp_wd_count++;
        }
    }
#endif

    if (putting_ptr != put_ptr) {
        u32 index = vfpool_idx[put_ptr];

        if (index >= VF_BUF_NUM) {
            printk("index %d\n", index);
        }

        if (--vfbuf_use[index] == 0) {
            buffer_for_recycle[buffer_for_recycle_wr++] = index + 1;
            if (buffer_for_recycle_wr == VF_BUF_NUM) {
                buffer_for_recycle_wr = 0;
            }
        }

        INCPTR(put_ptr);
    }

    if (buffer_for_recycle_rd != buffer_for_recycle_wr) {
        if (READ_MPEG_REG(AV_SCRATCH_7) == 0) {
            WRITE_MPEG_REG(AV_SCRATCH_7, buffer_for_recycle[buffer_for_recycle_rd]);

            if (++buffer_for_recycle_rd == VF_BUF_NUM) {
                buffer_for_recycle_rd = 0;
            }
        } else if (READ_MPEG_REG(AV_SCRATCH_8) == 0) {
            WRITE_MPEG_REG(AV_SCRATCH_8, buffer_for_recycle[buffer_for_recycle_rd]);

            if (++buffer_for_recycle_rd == VF_BUF_NUM) {
                buffer_for_recycle_rd = 0;
            }
        }
    }

    timer->expires = jiffies + PUT_INTERVAL;

    add_timer(timer);
}

int vh264_dec_status(struct vdec_status *vstatus)
{
    vstatus->width = frame_width;
    vstatus->height = frame_height;
    if (frame_dur != 0) {
        vstatus->fps = 96000 / frame_dur;
    } else {
        vstatus->fps = -1;
    }
    vstatus->error_count = READ_MPEG_REG(AV_SCRATCH_D);
    vstatus->status = stat;
    return 0;
}

int vh264_set_trickmode(unsigned long trickmode)
{
    if (trickmode == TRICKMODE_I) {
        WRITE_MPEG_REG(AV_SCRATCH_F, (READ_MPEG_REG(AV_SCRATCH_F) & 0xfffffffc) | 2);
        trickmode_i = 1;
    } else if (trickmode == TRICKMODE_NONE) {
        WRITE_MPEG_REG(AV_SCRATCH_F, READ_MPEG_REG(AV_SCRATCH_F) & 0xfffffffc);
        trickmode_i = 0;
    }

    return 0;
}

static void vh264_prot_init(void)
{

    while (READ_MPEG_REG(DCAC_DMA_CTRL) & 0x8000) {
        ;
    }
    while (READ_MPEG_REG(LMEM_DMA_CTRL) & 0x8000) {
        ;    // reg address is 0x350
    }

    WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC | RESET_VLD_PART);
    READ_MPEG_REG(RESET0_REGISTER);
    WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC | RESET_VLD_PART);

    WRITE_MPEG_REG(RESET2_REGISTER, RESET_PIC_DC | RESET_DBLK);
    WRITE_MPEG_REG(POWER_CTL_VLD, READ_MPEG_REG(POWER_CTL_VLD) | (0 << 10) | (1 << 9) | (1 << 6));

    /* disable PSCALE for hardware sharing */
    WRITE_MPEG_REG(PSCALE_CTRL, 0);

    WRITE_MPEG_REG(AV_SCRATCH_0, 0);
    WRITE_MPEG_REG(AV_SCRATCH_1, buf_offset);
    WRITE_MPEG_REG(AV_SCRATCH_7, 0);
    WRITE_MPEG_REG(AV_SCRATCH_8, 0);
    WRITE_MPEG_REG(AV_SCRATCH_9, 0);
    WRITE_MPEG_REG(AV_SCRATCH_F, (READ_MPEG_REG(AV_SCRATCH_F) & 0xffffffcf) | ((error_recovery_mode & 0x3) << 4));

    /* clear mailbox interrupt */
    WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

    /* enable mailbox interrupt */
    WRITE_MPEG_REG(ASSIST_MBOX1_MASK, 1);
}

static void vh264_local_init(void)
{
    int i;

    //vh264_ratio = vh264_amstream_dec_info.ratio;
    vh264_ratio = 0x100;

    vh264_rotation = (((u32)vh264_amstream_dec_info.param) >> 16) & 0xffff;

    fill_ptr = get_ptr = put_ptr = putting_ptr = 0;

    frame_buffer_size = AVIL_DPB_BUFF_SIZE + buf_size - DEFAULT_MEM_SIZE;
    frame_prog = 0;
    frame_width = vh264_amstream_dec_info.width;
    frame_height = vh264_amstream_dec_info.height;
    frame_dur = vh264_amstream_dec_info.rate;
    pts_outside = ((u32)vh264_amstream_dec_info.param) & 0x01;
    sync_outside = ((u32)vh264_amstream_dec_info.param & 0x02) >> 1;

    buffer_for_recycle_rd = 0;
    buffer_for_recycle_wr = 0;

    for (i = 0; i < VF_BUF_NUM; i++) {
        vfbuf_use[i] = 0;
    }

    for (i = 0; i < VF_POOL_SIZE; i++) {
        vfpool_idx[i] = VF_BUF_NUM;
        vfpool[i].bufWidth = 1920;
    }

#ifdef DROP_B_FRAME_FOR_1080P_50_60FPS
    last_interlaced = 1;
#endif
    neg_poc_counter = 0;
    h264_first_pts_ready = 0;
    h264pts1 = 0;
    h264pts2 = 0;
    h264_pts_count = 0;
    duration_from_pts_done = 0;
    vh264_error_count = READ_MPEG_REG(AV_SCRATCH_D);

    p_last_vf = NULL;
    last_ptr = VF_POOL_SIZE - 1;
    wait_buffer_counter = 0;
    vh264_no_disp_count = 0;
#ifdef DEBUG_PTS
    pts_missed = 0;
    pts_hit = 0;
#endif

    return;
}

static s32 vh264_init(void)
{
    void __iomem *p = ioremap_nocache(MEM_HEADER_CPU_BASE, MEM_SWAP_SIZE);

    if (!p) {
        printk("\nvh264_init: Cannot remap ucode swapping memory\n");
        return -ENOMEM;
    }

    printk("\nvh264_init\n");
    init_timer(&recycle_timer);

    stat |= STAT_TIMER_INIT;

    vh264_local_init();

    amvdec_enable();

    if (amvdec_loadmc(vh264_mc) < 0) {
        amvdec_disable();
        return -EBUSY;
    }

    memcpy(p,
           vh264_header_mc, sizeof(vh264_header_mc));

    memcpy((void *)((ulong)p + 0x1000),
           vh264_data_mc, sizeof(vh264_data_mc));

    memcpy((void *)((ulong)p + 0x2000),
           vh264_mmco_mc, sizeof(vh264_mmco_mc));

    memcpy((void *)((ulong)p + 0x3000),
           vh264_list_mc, sizeof(vh264_list_mc));

    memcpy((void *)((ulong)p + 0x4000),
           vh264_slice_mc, sizeof(vh264_slice_mc));

    iounmap(p);

    stat |= STAT_MC_LOAD;

    /* enable AMRISC side protocol */
    vh264_prot_init();

#ifdef HANDLE_H264_IRQ
    if (request_irq(INT_MAILBOX_1A, vh264_isr,
                    IRQF_SHARED, "vh264-irq", (void *)vh264_dec_id)) {
        printk("vh264 irq register error.\n");
        amvdec_disable();
        return -ENOENT;
    }
#endif

    stat |= STAT_ISR_REG;


 #ifdef CONFIG_POST_PROCESS_MANAGER
    vf_provider_init(&vh264_vf_prov, PROVIDER_NAME, &vh264_vf_provider, NULL);
    vf_reg_provider(&vh264_vf_prov);
    vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_START,NULL);
 #else 
    vf_provider_init(&vh264_vf_prov, PROVIDER_NAME, &vh264_vf_provider, NULL);
    vf_reg_provider(&vh264_vf_prov);
 #endif 
    stat |= STAT_VF_HOOK;

    recycle_timer.data = (ulong) & recycle_timer;
    recycle_timer.function = vh264_put_timer_func;
    recycle_timer.expires = jiffies + PUT_INTERVAL;

    add_timer(&recycle_timer);

    stat |= STAT_TIMER_ARM;
	vh264_running = 0;
    amvdec_start();

    stat |= STAT_VDEC_RUN;

    set_vdec_func(&vh264_dec_status);
    set_trickmode_func(&vh264_set_trickmode);

    return 0;
}

static int vh264_stop(void)
{
    if (stat & STAT_VDEC_RUN) {
        amvdec_stop();
        stat &= ~STAT_VDEC_RUN;
    }

    if (stat & STAT_ISR_REG) {
        WRITE_MPEG_REG(ASSIST_MBOX1_MASK, 0);
        free_irq(INT_MAILBOX_1A,
                 (void *)vh264_dec_id);
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
        vf_unreg_provider(&vh264_vf_prov);
        stat &= ~STAT_VF_HOOK;
    }

    amvdec_disable();

    return 0;
}

static void error_do_work(struct work_struct *work)
{
    if (atomic_read(&vh264_active)) {
        vh264_stop();
        vh264_init();
    }
}

static int amvdec_h264_probe(struct platform_device *pdev)
{
    struct resource *mem;

    printk("amvdec_h264 probe start.\n");

    if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0))) {
        printk("\namvdec_h264 memory resource undefined.\n");
        return -EFAULT;
    }

    buf_size = mem->end - mem->start + 1;
    if (buf_size < DEFAULT_MEM_SIZE) {
        printk("\namvdec_h264 memory size not enough.\n");
        return -ENOMEM;
    }

    buf_offset = mem->start - DEF_BUF_START_ADDR;
    buf_start = V_BUF_ADDR_START + buf_offset;

    memcpy(&vh264_amstream_dec_info, (void *)mem[1].start, sizeof(vh264_amstream_dec_info));

    if (vh264_init() < 0) {
        printk("\namvdec_h264 init failed.\n");

        return -ENODEV;
    }

    INIT_WORK(&error_wd_work, error_do_work);

    atomic_set(&vh264_active, 1);

    printk("amvdec_h264 probe end.\n");

    return 0;
}

static int amvdec_h264_remove(struct platform_device *pdev)
{
    vh264_stop();

    atomic_set(&vh264_active, 0);

#ifdef DEBUG_PTS
    printk("pts missed %ld, pts hit %ld, pts_outside %d, duration %d, sync_outside %d\n",
           pts_missed, pts_hit, pts_outside, frame_dur, sync_outside);
#endif

    return 0;
}

/****************************************/

static struct platform_driver amvdec_h264_driver = {
    .probe   = amvdec_h264_probe,
    .remove  = amvdec_h264_remove,
#ifdef CONFIG_PM
    .suspend = amvdec_suspend,
    .resume  = amvdec_resume,
#endif
    .driver  = {
        .name = DRIVER_NAME,
    }
};

static struct codec_profile_t amvdec_h264_profile = {
	.name = "h264",
	.profile = ""
};

static int __init amvdec_h264_driver_init_module(void)
{
    printk("amvdec_h264 module init\n");

    if (platform_driver_register(&amvdec_h264_driver)) {
        printk("failed to register amvdec_h264 driver\n");
        return -ENODEV;
    }
	vcodec_profile_register(&amvdec_h264_profile);
    return 0;
}

static void __exit amvdec_h264_driver_remove_module(void)
{
    printk("amvdec_h264 module remove.\n");

    platform_driver_unregister(&amvdec_h264_driver);
}

/****************************************/

module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_h264 stat \n");
module_param(error_recovery_mode, uint, 0664);
MODULE_PARM_DESC(error_recovery_mode, "\n amvdec_h264 error_recovery_mode \n");
module_param(sync_outside, uint, 0664);
MODULE_PARM_DESC(sync_outside, "\n amvdec_h264 sync_outside \n");
module_param(dec_control, uint, 0664);
MODULE_PARM_DESC(dec_control, "\n amvdec_h264 decoder control \n");
module_init(amvdec_h264_driver_init_module);
module_exit(amvdec_h264_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC H264 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chen Zhang <chen.zhang@amlogic.com>");
