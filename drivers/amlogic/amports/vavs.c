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
#include "avs_mc.h"

#define DRIVER_NAME "amvdec_avs"
#define MODULE_NAME "amvdec_avs"

//#define DEBUG_UCODE

#define HANDLE_AVS_IRQ
#define DEBUG_PTS

#define I_PICTURE   0
#define P_PICTURE   1
#define B_PICTURE   2

//#define ORI_BUFFER_START_ADDR   0x81000000
#define ORI_BUFFER_START_ADDR   0x80000000

#define INTERLACE_FLAG          0x80
#define BOTTOM_FIELD_FIRST_FLAG 0x40

/* protocol registers */
#define AVS_PIC_RATIO       AV_SCRATCH_0
#define AVS_ERROR_COUNT    AV_SCRATCH_6
#define AVS_SOS_COUNT     AV_SCRATCH_7
#define AVS_BUFFERIN       AV_SCRATCH_8
#define AVS_BUFFEROUT      AV_SCRATCH_9
#define AVS_REPEAT_COUNT    AV_SCRATCH_A
#define AVS_TIME_STAMP      AV_SCRATCH_B
#define AVS_OFFSET_REG      AV_SCRATCH_C
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

static vframe_t *vavs_vf_peek(void*);
static vframe_t *vavs_vf_get(void*);
static void vavs_vf_put(vframe_t *, void*);
static int  vavs_vf_states(vframe_states_t *states, void*);
static int vavs_event_cb(int type, void *data, void *private_data);
static void vavs_prot_init(void);
static void vavs_local_init(void);
static const char vavs_dec_id[] = "vavs-dev";

#define PROVIDER_NAME   "decoder.avs"

static const struct vframe_operations_s vavs_vf_provider = {
        .peek = vavs_vf_peek,
        .get = vavs_vf_get,
        .put = vavs_vf_put,
         .event_cb = vavs_event_cb,
         .vf_states = vavs_vf_states,
};

static struct vframe_provider_s vavs_vf_prov;

static struct vframe_s vfpool[VF_POOL_SIZE];
static u32 vfpool_idx[VF_POOL_SIZE];
static s32 vfbuf_use[4];
static s32 fill_ptr, get_ptr, putting_ptr, put_ptr;
static u32 frame_width, frame_height, frame_dur, frame_prog;
static struct timer_list recycle_timer;
static u32 stat;
static u32 buf_start, buf_size, buf_offset;
static u32 avi_flag = 0;
static u32 vavs_ratio;

static u32 pts_by_offset = 1;
static u32 total_frame;
static u32 next_pts;
#ifdef DEBUG_PTS
static u32 pts_hit, pts_missed, pts_i_hit, pts_i_missed;
#endif
static spinlock_t lock = SPIN_LOCK_UNLOCKED;
static struct dec_sysinfo vavs_amstream_dec_info;

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

        if (i >= VF_POOL_SIZE)
                i = 0;

        *ptr = i;
}

static void set_aspect_ratio(vframe_t *vf, unsigned pixel_ratio)
{
        int ar = 0;

        if (vavs_ratio == 0)
        {
                vf->ratio_control |=(0x90<<DISP_RATIO_ASPECT_RATIO_BIT); // always stretch to 16:9
        }
        else
        {
                switch (pixel_ratio)
                {
   		        case 1:
   		            ar = (vf->height*vavs_ratio)/vf->width;
   		            break;
   		        case 2:
   		            ar = (vf->height*3*vavs_ratio)/(vf->width*4);
   		            break;
   		        case 3:
   		            ar = (vf->height*9*vavs_ratio)/(vf->width*16);
   		            break;
   		        case 4:
   		            ar = (vf->height*100*vavs_ratio)/(vf->width*221);
   		            break;
                default:
                    ar = (vf->height*vavs_ratio)/vf->width;
                    break;
                }
        }

        ar = min(ar, DISP_RATIO_ASPECT_RATIO_MAX);

        vf->ratio_control = (ar<<DISP_RATIO_ASPECT_RATIO_BIT);
        //vf->ratio_control |= DISP_RATIO_FORCECONFIG | DISP_RATIO_KEEPRATIO;
}

#ifdef HANDLE_AVS_IRQ
static irqreturn_t vavs_isr(int irq, void *dev_id)
#else
static void vavs_isr(void)
#endif
{
        u32 reg;
        vframe_t *vf;
        u32 repeat_count;
        u32 picture_type;
        u32 buffer_index;
        unsigned int pts, pts_valid=0, offset;
        
#ifdef DEBUG_UCODE
       printk("vavs isr\n");
       if(READ_MPEG_REG(AV_SCRATCH_E)!=0){
            printk("dbg%x: %x\n",  READ_MPEG_REG(AV_SCRATCH_E), READ_MPEG_REG(AV_SCRATCH_D));
            WRITE_MPEG_REG(AV_SCRATCH_E, 0);
       }

#endif

        reg = READ_MPEG_REG(AVS_BUFFEROUT);

        if (reg)
        {
#ifdef DEBUG_UCODE
                printk("AVS_BUFFEROUT=%x\n", reg);
#endif
                if (pts_by_offset)
                {
                        offset = READ_MPEG_REG(AVS_OFFSET_REG);
                        if (pts_lookup_offset(PTS_TYPE_VIDEO, offset, &pts, 0) == 0)
                        {
                                pts_valid = 1;
                        #ifdef DEBUG_PTS
                                pts_hit++;
                        #endif
                        }
                        else
                        {
                        #ifdef DEBUG_PTS
                                pts_missed++;
                        #endif
                        }
                }

                repeat_count = READ_MPEG_REG(AVS_REPEAT_COUNT);
                buffer_index = ((reg & 0x7) - 1) & 3;
                picture_type = (reg >> 3) & 7;
            #ifdef DEBUG_PTS
                if (picture_type == I_PICTURE)
                {
                    //printk("I offset 0x%x, pts_valid %d\n", offset, pts_valid);
                    if (!pts_valid)
                        pts_i_missed++;
                    else
                        pts_i_hit++;
                }
            #endif

                if (reg & INTERLACE_FLAG) // interlace
                {
#ifdef DEBUG_UCODE
                        printk("interlace\n");
#endif
                        vfpool_idx[fill_ptr] = buffer_index;
                        vf = &vfpool[fill_ptr];
                        vf->width = vavs_amstream_dec_info.width;
                        vf->height = vavs_amstream_dec_info.height;
                        vf->bufWidth = 1920;

                        if ((I_PICTURE == picture_type) && pts_valid)
                        {
                                vf->pts = pts;
                                if ((repeat_count > 1) && avi_flag)
                                {
                                        next_pts = pts + (vavs_amstream_dec_info.rate * repeat_count >> 1)*15/16;
                                }
                                else
                                {
                                        next_pts = 0;
                                }
                        }
                        else
                        {
                                vf->pts = next_pts;
                                if ((repeat_count > 1) && avi_flag)
                                {
                                        vf->duration = vavs_amstream_dec_info.rate * repeat_count >> 1;
                                        if (next_pts != 0)
                                        {
                                                next_pts += ((vf->duration) - ((vf->duration)>>4));
                                        }
                                }
                                else
                                {
                                        vf->duration = vavs_amstream_dec_info.rate >> 1;
                                        next_pts = 0;
                                }
                        }

                        vf->duration_pulldown = 0;
                        vf->type = (reg & BOTTOM_FIELD_FIRST_FLAG) ? VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
                        vf->canvas0Addr = vf->canvas1Addr = index2canvas(buffer_index);
#ifdef DEBUG_UCODE
                        printk("buffer_index %d, canvas addr %x\n",buffer_index, vf->canvas0Addr);
#endif
                        set_aspect_ratio(vf, READ_MPEG_REG(AVS_PIC_RATIO));

                        vfbuf_use[buffer_index]++;

                        INCPTR(fill_ptr);
                        vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);

                        vfpool_idx[fill_ptr] = buffer_index;
                        vf = &vfpool[fill_ptr];
                        vf->width = vavs_amstream_dec_info.width;
                        vf->height = vavs_amstream_dec_info.height;
                        vf->bufWidth = 1920;

                        vf->pts = next_pts;
                        if ((repeat_count > 1) && avi_flag)
                        {
                                vf->duration = vavs_amstream_dec_info.rate * repeat_count >> 1;
                                if (next_pts != 0)
                                {
                                        next_pts += ((vf->duration) - ((vf->duration)>>4));
                                }
                        }
                        else
                        {
                                vf->duration = vavs_amstream_dec_info.rate >> 1;
                                next_pts = 0;
                        }

                        vf->duration_pulldown = 0;
                        vf->type = (reg & BOTTOM_FIELD_FIRST_FLAG) ? VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
                        vf->canvas0Addr = vf->canvas1Addr = index2canvas(buffer_index);

                        set_aspect_ratio(vf, READ_MPEG_REG(AVS_PIC_RATIO));

                        vfbuf_use[buffer_index]++;

                        INCPTR(fill_ptr);
                        vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
                }
                else  // progressive
                {
#ifdef DEBUG_UCODE
                    printk("progressive\n");
#endif
                        vfpool_idx[fill_ptr] = buffer_index;
                        vf = &vfpool[fill_ptr];
                        vf->width = vavs_amstream_dec_info.width;
                        vf->height = vavs_amstream_dec_info.height;
                        vf->bufWidth = 1920;

                        if ((I_PICTURE == picture_type) && pts_valid)
                        {
                                vf->pts = pts;
                                if ((repeat_count > 1) && avi_flag)
                                {
                                        next_pts = pts + (vavs_amstream_dec_info.rate * repeat_count)*15/16;
                                }
                                else
                                {
                                        next_pts = 0;
                                }
                        }
                        else
                        {
                                vf->pts = next_pts;
                                if ((repeat_count > 1) && avi_flag)
                                {
                                        vf->duration = vavs_amstream_dec_info.rate * repeat_count;
                                        if (next_pts != 0)
                                        {
                                                next_pts += ((vf->duration) - ((vf->duration)>>4));
                                        }
                                }
                                else
                                {
                                        vf->duration = vavs_amstream_dec_info.rate;
                                        next_pts = 0;
                                }
                        }

                        vf->duration_pulldown = 0;
                        vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
                        vf->canvas0Addr = vf->canvas1Addr = index2canvas(buffer_index);
#ifdef DEBUG_UCODE
                        printk("buffer_index %d, canvas addr %x\n",buffer_index, vf->canvas0Addr);
#endif
                        set_aspect_ratio(vf, READ_MPEG_REG(AVS_PIC_RATIO));

                        vfbuf_use[buffer_index]++;

                        INCPTR(fill_ptr);
                        vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);
                }

                total_frame++;

                printk("PicType = %d, duration=%d, PTS = 0x%x\n", picture_type, vf->duration,vf->pts);
                WRITE_MPEG_REG(AVS_BUFFEROUT, 0);
        }

        WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

#ifdef HANDLE_AVS_IRQ
        return IRQ_HANDLED;
#else
        return;
#endif
}

static int run_flag=1;
static int step_flag=0;
static vframe_t *vavs_vf_peek(void* op_arg)
{
        if(run_flag==0)
            return NULL;
        if (get_ptr == fill_ptr)
                return NULL;

        return &vfpool[get_ptr];
}

static vframe_t *vavs_vf_get(void* op_arg)
{
        vframe_t *vf;
        if(run_flag==0)
            return NULL;

        if (get_ptr == fill_ptr)
                return NULL;

        vf = &vfpool[get_ptr];

        INCPTR(get_ptr);
        if(step_flag)
            run_flag=0;        
        return vf;
}

static void vavs_vf_put(vframe_t *vf, void* op_arg)
{
        INCPTR(putting_ptr);
}
static int vavs_vf_states(vframe_states_t *states, void* op_arg)
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

static int vavs_event_cb(int type, void *data, void *private_data)
{
    if(type & VFRAME_EVENT_RECEIVER_RESET){
        unsigned long flags;
        amvdec_stop();
#ifndef CONFIG_POST_PROCESS_MANAGER
        vf_light_unreg_provider(&vavs_vf_prov);
#endif
        spin_lock_irqsave(&lock, flags);
        vavs_local_init();
        vavs_prot_init();
        spin_unlock_irqrestore(&lock, flags); 
#ifndef CONFIG_POST_PROCESS_MANAGER
        vf_reg_provider(&vavs_vf_prov);
#endif              
        amvdec_start();
    }
    return 0;        
}

int vavs_dec_status(struct vdec_status *vstatus)
{
    vstatus->width = vavs_amstream_dec_info.width;
    vstatus->height = vavs_amstream_dec_info.height;
    if(0!=vavs_amstream_dec_info.rate)
        vstatus->fps = 96000/vavs_amstream_dec_info.rate;
    else
        vstatus->fps = 96000;
    vstatus->error_count = READ_MPEG_REG(AVS_ERROR_COUNT);
    vstatus->status = stat;

    return 0;
}

/****************************************/
static void vavs_canvas_init(void)
{
        int i;
        u32 canvas_width, canvas_height;
        u32 decbuf_size, decbuf_y_size, decbuf_uv_size;
        u32 disp_addr = 0xffffffff;
        int canvas_num = 3;

        if (buf_size <= 0x00400000)
        {
                /* SD only */
                canvas_width = 768;
                canvas_height = 576;
                decbuf_y_size = 0x80000;
                decbuf_uv_size = 0x20000;
                decbuf_size = 0x100000;
        printk("avs (SD only): buf_start %x, buf_size %x, buf_offset %x\n", buf_start, buf_size, buf_offset);
        }
        else
        {
                /* HD & SD */
                canvas_width = 1920;
                canvas_height = 1088;
                decbuf_y_size = 0x200000;
                decbuf_uv_size = 0x80000;
                decbuf_size = 0x300000;
        printk("avs: buf_start %x, buf_size %x, buf_offset %x\n", buf_start, buf_size, buf_offset);
        }

        if (READ_MPEG_REG(VPP_MISC) & VPP_VD1_POSTBLEND)
        {
	        canvas_t cur_canvas;

	        canvas_read((READ_MPEG_REG(VD1_IF0_CANVAS0) & 0xff), &cur_canvas);
            disp_addr = (cur_canvas.addr + 7) >> 3;
        }

        for (i = 0; i < 4; i++)
        {
            if (((buf_start + i * decbuf_size + 7) >> 3) == disp_addr)
            {
                canvas_config(canvas_num * i + 0,
                        buf_start + 4 * decbuf_size,
                        canvas_width, canvas_height,
                        CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                canvas_config(canvas_num * i + 1,
                        buf_start + 4 * decbuf_size + decbuf_y_size,
                        canvas_width / 2, canvas_height / 2,
                        CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                canvas_config(canvas_num * i + 2,
                        buf_start + 4 * decbuf_size + decbuf_y_size + decbuf_uv_size,
                        canvas_width/2, canvas_height/2,
                        CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                        
#ifdef DEBUG_UCODE
                printk("canvas config %d, addr %x\n", 4, buf_start + 4 * decbuf_size);        
#endif
            }
            else
            {
                canvas_config(canvas_num * i + 0,
                              buf_start + i * decbuf_size,
                              canvas_width, canvas_height,
                              CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                canvas_config(canvas_num * i + 1,
                              buf_start + i * decbuf_size + decbuf_y_size,
                              canvas_width / 2, canvas_height / 2,
                              CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
                canvas_config(canvas_num * i + 2,
                              buf_start + i * decbuf_size + decbuf_y_size + decbuf_uv_size,
                              canvas_width / 2, canvas_height / 2,
                              CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

#ifdef DEBUG_UCODE
                printk("canvas config %d, addr %x\n", i, buf_start + i * decbuf_size);        
#endif
            }
        }
}

static void vavs_prot_init(void)
{
        WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC | RESET_VLD_PART);
        READ_MPEG_REG(RESET0_REGISTER);
        WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC | RESET_VLD_PART);

        WRITE_MPEG_REG(RESET2_REGISTER, RESET_PIC_DC | RESET_DBLK);

        WRITE_MPEG_REG(POWER_CTL_VLD, 0x10);
    	WRITE_MPEG_REG_BITS(VLD_MEM_VIFIFO_CONTROL, 2, MEM_FIFO_CNT_BIT, 2);
    	WRITE_MPEG_REG_BITS(VLD_MEM_VIFIFO_CONTROL, 8, MEM_LEVEL_CNT_BIT, 6);

        vavs_canvas_init();

        /* index v << 16 | u << 8 | y */
        WRITE_MPEG_REG(AV_SCRATCH_0, 0x020100);
        WRITE_MPEG_REG(AV_SCRATCH_1, 0x050403);
        WRITE_MPEG_REG(AV_SCRATCH_2, 0x080706);
        WRITE_MPEG_REG(AV_SCRATCH_3, 0x0b0a09);

        /* notify ucode the buffer offset */
        WRITE_MPEG_REG(AV_SCRATCH_F, buf_offset);

        /* disable PSCALE for hardware sharing */
        WRITE_MPEG_REG(PSCALE_CTRL, 0);

        WRITE_MPEG_REG(AVS_SOS_COUNT, 0);
        WRITE_MPEG_REG(AVS_BUFFERIN, 0);
        WRITE_MPEG_REG(AVS_BUFFEROUT, 0);

        /* clear mailbox interrupt */
        WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

        /* enable mailbox interrupt */
        WRITE_MPEG_REG(ASSIST_MBOX1_MASK, 1);
#if 1 //def DEBUG_UCODE
        WRITE_MPEG_REG(AV_SCRATCH_D, 0);
#endif
}

static void vavs_local_init(void)
{
        int i;

        vavs_ratio = vavs_amstream_dec_info.ratio;

        avi_flag = (u32)vavs_amstream_dec_info.param;

		if(vavs_amstream_dec_info.height>1080)
			vavs_amstream_dec_info.height=1080;
        printk("avi_flag=%d,vavs_ratio=%d\n",avi_flag,vavs_ratio);
        fill_ptr = get_ptr = put_ptr = putting_ptr = 0;
        printk("width=%d,height=%d,rate=%d\n",
			vavs_amstream_dec_info.width,
			vavs_amstream_dec_info.height,
			vavs_amstream_dec_info.rate);
        frame_width = frame_height = frame_dur = frame_prog = 0;

        total_frame = 0;

        next_pts = 0;

#ifdef DEBUG_PTS
        pts_hit = pts_missed = pts_i_hit = pts_i_missed = 0;
#endif

        for (i = 0; i < 4; i++)
                vfbuf_use[i] = 0;
}

static void vavs_put_timer_func(unsigned long arg)
{
        struct timer_list *timer = (struct timer_list *)arg;

#ifndef HANDLE_AVS_IRQ
        vavs_isr();
#endif

#if 0
        if ( READ_MPEG_REG(AVS_SOS_COUNT) > 10 )
        {
                amvdec_stop();
                vf_light_unreg_provider(&vavs_vf_prov);
                vavs_local_init();
                vavs_prot_init();
                vf_reg_provider(&vavs_vf_prov);
                amvdec_start();
        }
#endif

        if ((putting_ptr != put_ptr) && (READ_MPEG_REG(AVS_BUFFERIN) == 0))
        {
                u32 index = vfpool_idx[put_ptr];

                if (--vfbuf_use[index] == 0)
                {
                        WRITE_MPEG_REG(AVS_BUFFERIN, ~(1<<index));
                }

                INCPTR(put_ptr);
        }

        timer->expires = jiffies + PUT_INTERVAL;

        add_timer(timer);
}

static s32 vavs_init(void)
{
        printk("vavs_init\n");
        init_timer(&recycle_timer);

        stat |= STAT_TIMER_INIT;

        vavs_local_init();

        if (amvdec_loadmc(avs_mc) < 0)
        {
                printk("failed\n");
                return -EBUSY;
        }

        stat |= STAT_MC_LOAD;

        /* enable AMRISC side protocol */
        vavs_prot_init();

#ifdef HANDLE_AVS_IRQ
        if (request_irq(INT_MAILBOX_1A, vavs_isr,
                        IRQF_SHARED, "vavs-irq", (void *)vavs_dec_id))
        {
                printk("vavs irq register error.\n");
                return -ENOENT;
        }
#endif

        stat |= STAT_ISR_REG;
 #ifdef CONFIG_POST_PROCESS_MANAGER
    vf_provider_init(&vavs_vf_prov, PROVIDER_NAME, &vavs_vf_provider, NULL);
    vf_reg_provider(&vavs_vf_prov);
    vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_START,NULL);
 #else 
    vf_provider_init(&vavs_vf_prov, PROVIDER_NAME, &vavs_vf_provider, NULL);
    vf_reg_provider(&vavs_vf_prov);
 #endif 

        stat |= STAT_VF_HOOK;

        recycle_timer.data = (ulong) & recycle_timer;
        recycle_timer.function = vavs_put_timer_func;
        recycle_timer.expires = jiffies + PUT_INTERVAL;

        add_timer(&recycle_timer);

        stat |= STAT_TIMER_ARM;

        amvdec_start();

        stat |= STAT_VDEC_RUN;

        set_vdec_func(&vavs_dec_status);

        return 0;
}

static int amvdec_avs_probe(struct platform_device *pdev)
{
        struct resource *mem;

        if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0)))
        {
                printk("amvdec_avs memory resource undefined.\n");
                return -EFAULT;
        }

        buf_start = mem->start;
        buf_size = mem->end - mem->start + 1;
        buf_offset = buf_start - ORI_BUFFER_START_ADDR;

        memcpy(&vavs_amstream_dec_info, (void *)mem[1].start, sizeof(vavs_amstream_dec_info));

        if (vavs_init() < 0)
        {
                printk("amvdec_avs init failed.\n");

                return -ENODEV;
        }

        return 0;
}

static int amvdec_avs_remove(struct platform_device *pdev)
{
        if (stat & STAT_VDEC_RUN)
        {
                amvdec_stop();
                stat &= ~STAT_VDEC_RUN;
        }

        if (stat & STAT_ISR_REG)
        {
                free_irq(INT_MAILBOX_1A, (void *)vavs_dec_id);
                stat &= ~STAT_ISR_REG;
        }

        if (stat & STAT_TIMER_ARM)
        {
                del_timer_sync(&recycle_timer);
                stat &= ~STAT_TIMER_ARM;
        }

        if (stat & STAT_VF_HOOK)
        {
                vf_unreg_provider(&vavs_vf_prov);
                stat &= ~STAT_VF_HOOK;
        }

#ifdef DEBUG_PTS
       printk("pts hit %d, pts missed %d, i hit %d, missed %d\n", pts_hit, pts_missed, pts_i_hit, pts_i_missed);
       printk("total frame %d, avi_flag %d, rate %d\n", total_frame, avi_flag, vavs_amstream_dec_info.rate);
#endif

        return 0;
}

/****************************************/

static struct platform_driver amvdec_avs_driver = {
        .probe  = amvdec_avs_probe,
        .remove = amvdec_avs_remove,
        .driver = {
                .name = DRIVER_NAME,
        }
};

static int __init amvdec_avs_driver_init_module(void)
{
        printk("amvdec_avs module init\n");

        if (platform_driver_register(&amvdec_avs_driver))
        {
                printk("failed to register amvdec_avs driver\n");
                return -ENODEV;
        }

        return 0;
}

static void __exit amvdec_avs_driver_remove_module(void)
{
        printk("amvdec_avs module remove.\n");

        platform_driver_unregister(&amvdec_avs_driver);
}

/****************************************/

module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_avs stat \n");

module_param(run_flag, uint, 0664);
MODULE_PARM_DESC(run_flag, "\n run_flag\n");

module_param(step_flag, uint, 0664);
MODULE_PARM_DESC(step_flag, "\n step_flag\n");

module_init(amvdec_avs_driver_init_module);
module_exit(amvdec_avs_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC AVS Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qi Wang <qi.wang@amlogic.com>");

