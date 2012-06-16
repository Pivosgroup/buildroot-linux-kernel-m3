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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/amports/ptsserv.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <mach/am_regs.h>

#include "streambuf_reg.h"
#include "streambuf.h"
#include "esparser.h"

#define SAVE_SCR 0

#define ES_START_CODE_PATTERN 0x00000100
#define ES_START_CODE_MASK    0xffffff00
#define SEARCH_PATTERN_LEN   512
#define ES_PARSER_POP      READ_MPEG_REG(PFIFO_DATA)

#define PARSER_WRITE        (ES_WRITE | ES_PARSER_START)
#define PARSER_VIDEO        (ES_TYPE_VIDEO)
#define PARSER_AUDIO        (ES_TYPE_AUDIO)
#define PARSER_SUBPIC       (ES_TYPE_SUBTITLE)
#define PARSER_PASSTHROUGH  (ES_PASSTHROUGH | ES_PARSER_START)
#define PARSER_AUTOSEARCH   (ES_SEARCH | ES_PARSER_START)
#define PARSER_DISCARD      (ES_DISCARD | ES_PARSER_START)
#define PARSER_BUSY         (ES_PARSER_BUSY)

static unsigned char *search_pattern;
static dma_addr_t search_pattern_map;

const static char esparser_id[] = "esparser-id";

static DECLARE_WAIT_QUEUE_HEAD(wq);

static struct tasklet_struct esparser_tasklet;
static volatile u32 search_done;
static u32 video_data_parsed;
static u32 audio_data_parsed;
static atomic_t esparser_use_count = ATOMIC_INIT(0);
static DEFINE_MUTEX(esparser_mutex);

static void parser_tasklet(ulong data)
{
    u32 int_status = READ_MPEG_REG(PARSER_INT_STATUS);

    WRITE_MPEG_REG(PARSER_INT_STATUS, int_status);

    if (int_status & PARSER_INTSTAT_SC_FOUND) {
        WRITE_MPEG_REG(PFIFO_RD_PTR, 0);
        WRITE_MPEG_REG(PFIFO_WR_PTR, 0);
        search_done = 1;
        wake_up_interruptible(&wq);
    }
}

static irqreturn_t parser_isr(int irq, void *dev_id)
{
    tasklet_schedule(&esparser_tasklet);

    return IRQ_HANDLED;
}

static ssize_t _esparser_write(const char __user *buf, size_t count, u32 type)
{
    size_t r = count;
    const char __user *p = buf;
    u32 len = 0;
    u32 parser_type;
    int ret;
    u32 wp,wp_reg;
	
    if (type == BUF_TYPE_VIDEO) {
		wp_reg=VLD_MEM_VIFIFO_WP;
		
        parser_type = PARSER_VIDEO;
    } else if (type == BUF_TYPE_AUDIO) {
   	 	wp_reg=AIU_MEM_AIFIFO_MAN_WP;
        parser_type = PARSER_AUDIO;
    } else {
    	wp_reg=PARSER_SUB_START_PTR;
        parser_type = PARSER_SUBPIC;
    }
	wp=READ_MPEG_REG(wp_reg);
    if (r > 0) {
        len = min(r, (size_t)FETCHBUF_SIZE);

        copy_from_user(fetchbuf_remap, p, len);
	wmb();
        // reset the Write and read pointer to zero again
        WRITE_MPEG_REG(PFIFO_RD_PTR, 0);
        WRITE_MPEG_REG(PFIFO_WR_PTR, 0);

        WRITE_MPEG_REG_BITS(PARSER_CONTROL, len, ES_PACK_SIZE_BIT, ES_PACK_SIZE_WID);
        WRITE_MPEG_REG_BITS(PARSER_CONTROL,
                            parser_type | PARSER_WRITE | PARSER_AUTOSEARCH,
                            ES_CTRL_BIT, ES_CTRL_WID);

        WRITE_MPEG_REG(PARSER_FETCH_ADDR, fetchbuf);
        WRITE_MPEG_REG(PARSER_FETCH_CMD,
                       (7 << FETCH_ENDIAN) | len);

        search_done = 0;

        WRITE_MPEG_REG(PARSER_FETCH_ADDR, search_pattern_map);
        WRITE_MPEG_REG(PARSER_FETCH_CMD,
                       (7 << FETCH_ENDIAN) | SEARCH_PATTERN_LEN);

        ret = wait_event_interruptible_timeout(wq, search_done != 0, HZ/10);
        if (ret == 0) {
            WRITE_MPEG_REG(PARSER_FETCH_CMD, 0);
	    printk("esparser write timeout\n");
	    if(wp==READ_MPEG_REG(wp_reg))/*no data fetched*/
            	return -EAGAIN;
	    else{
		printk("write timeout, but fetched ok,len=%d,wpdiff=%d\n",len,wp-(u32)READ_MPEG_REG(wp_reg));
	    }
        } else if (ret < 0) {
            return -ERESTARTSYS;
        }
    }

    if (type == BUF_TYPE_VIDEO) {
        video_data_parsed += len;
    } else if (type == BUF_TYPE_AUDIO) {
        audio_data_parsed += len;
    }

    return len;
}

s32 es_vpts_checkin(struct stream_buf_s *buf, u32 pts)
{
#if 0
    if (buf->first_tstamp == INVALID_PTS) {
        buf->flag |= BUF_FLAG_FIRST_TSTAMP;
        buf->first_tstamp = pts;

        return 0;
    }
#endif
    return pts_checkin_offset(PTS_TYPE_VIDEO, video_data_parsed, pts);

}

s32 es_apts_checkin(struct stream_buf_s *buf, u32 pts)
{
#if 0
    if (buf->first_tstamp == INVALID_PTS) {
        buf->flag |= BUF_FLAG_FIRST_TSTAMP;
        buf->first_tstamp = pts;

        return 0;
    }
#endif

    return pts_checkin_offset(PTS_TYPE_AUDIO, audio_data_parsed, pts);
}

#ifdef CONFIG_AM_DVB
extern int tsdemux_set_reset_flag(void);
#endif

s32 esparser_init(struct stream_buf_s *buf)
{
    s32 r;
    u32 pts_type;
    u32 parser_sub_start_ptr;
    u32 parser_sub_end_ptr;
    u32 parser_sub_rp;

    if (buf->type == BUF_TYPE_VIDEO) {
        pts_type = PTS_TYPE_VIDEO;
    } else if (buf->type & BUF_TYPE_AUDIO) {
        pts_type = PTS_TYPE_AUDIO;
    } else if (buf->type & BUF_TYPE_SUBTITLE) {
        pts_type = PTS_TYPE_MAX;
    } else {
        return -EINVAL;
    }

    parser_sub_start_ptr = READ_MPEG_REG(PARSER_SUB_START_PTR);
    parser_sub_end_ptr = READ_MPEG_REG(PARSER_SUB_END_PTR);
    parser_sub_rp = READ_MPEG_REG(PARSER_SUB_RP);

    buf->flag |= BUF_FLAG_PARSER;

    if (atomic_add_return(1, &esparser_use_count) == 1) {
        if (fetchbuf == 0) {
            printk("%s: no fetchbuf\n", __FUNCTION__);
            return -ENOMEM;
        }

        if (search_pattern == NULL) {
            search_pattern = (unsigned char *)kcalloc(1, SEARCH_PATTERN_LEN, GFP_KERNEL);

            if (search_pattern == NULL) {
                printk("%s: no search_pattern\n", __FUNCTION__);
                return -ENOMEM;
            }

            /* build a fake start code to get parser interrupt */
            search_pattern[0] = 0x00;
            search_pattern[1] = 0x00;
            search_pattern[2] = 0x01;
            search_pattern[3] = 0xff;

            search_pattern_map = dma_map_single(NULL, search_pattern,
                                                SEARCH_PATTERN_LEN, DMA_TO_DEVICE);
        }

        /* reset PARSER with first esparser_init() call */
        WRITE_MPEG_REG(RESET1_REGISTER, RESET_PARSER);

        /* TS data path */
#ifndef CONFIG_AM_DVB
        WRITE_MPEG_REG(FEC_INPUT_CONTROL, 0);
#else
        tsdemux_set_reset_flag();
#endif
        CLEAR_MPEG_REG_MASK(TS_HIU_CTL, 1 << USE_HI_BSF_INTERFACE);
        CLEAR_MPEG_REG_MASK(TS_HIU_CTL_2, 1 << USE_HI_BSF_INTERFACE);
        CLEAR_MPEG_REG_MASK(TS_HIU_CTL_3, 1 << USE_HI_BSF_INTERFACE);

        CLEAR_MPEG_REG_MASK(TS_FILE_CONFIG, (1 << TS_HIU_ENABLE));

        WRITE_MPEG_REG(PARSER_CONFIG,
                       (10 << PS_CFG_PFIFO_EMPTY_CNT_BIT) |
                       (1  << PS_CFG_MAX_ES_WR_CYCLE_BIT) |
                       (16 << PS_CFG_MAX_FETCH_CYCLE_BIT));

        WRITE_MPEG_REG(PFIFO_RD_PTR, 0);
        WRITE_MPEG_REG(PFIFO_WR_PTR, 0);

        WRITE_MPEG_REG(PARSER_SEARCH_PATTERN, ES_START_CODE_PATTERN);
        WRITE_MPEG_REG(PARSER_SEARCH_MASK, ES_START_CODE_MASK);

        WRITE_MPEG_REG(PARSER_CONFIG,
                       (10 << PS_CFG_PFIFO_EMPTY_CNT_BIT) |
                       (1  << PS_CFG_MAX_ES_WR_CYCLE_BIT) |
                       PS_CFG_STARTCODE_WID_24   |
                       PS_CFG_PFIFO_ACCESS_WID_8 | /* single byte pop */
                       (16 << PS_CFG_MAX_FETCH_CYCLE_BIT));

        WRITE_MPEG_REG(PARSER_CONTROL, PARSER_AUTOSEARCH);

        tasklet_init(&esparser_tasklet, parser_tasklet, 0);
    }

    /* hook stream buffer with PARSER */
    if (pts_type == PTS_TYPE_VIDEO) {
        WRITE_MPEG_REG(PARSER_VIDEO_START_PTR,
                       READ_MPEG_REG(VLD_MEM_VIFIFO_START_PTR));
        WRITE_MPEG_REG(PARSER_VIDEO_END_PTR,
                       READ_MPEG_REG(VLD_MEM_VIFIFO_END_PTR));
        CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

        WRITE_MPEG_REG(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
        CLEAR_MPEG_REG_MASK(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

        video_data_parsed = 0;
    } else if (pts_type == PTS_TYPE_AUDIO) {
        WRITE_MPEG_REG(PARSER_AUDIO_START_PTR,
                       READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
        WRITE_MPEG_REG(PARSER_AUDIO_END_PTR,
                       READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR));
        CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

        WRITE_MPEG_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
        CLEAR_MPEG_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

        audio_data_parsed = 0;
    } else if (buf->type & BUF_TYPE_SUBTITLE) {
        WRITE_MPEG_REG(PARSER_SUB_START_PTR, parser_sub_start_ptr);
        WRITE_MPEG_REG(PARSER_SUB_END_PTR, parser_sub_end_ptr);
        WRITE_MPEG_REG(PARSER_SUB_RP, parser_sub_rp);
        SET_MPEG_REG_MASK(PARSER_ES_CONTROL, (7 << ES_SUB_WR_ENDIAN_BIT) | ES_SUB_MAN_RD_PTR);
    }

    if (pts_type < PTS_TYPE_MAX) {
        r = pts_start(pts_type);
        if (r < 0) {
            printk("esparser_init: pts_start failed\n");
            goto Err_1;
        }
    }

#if 0
    if (buf->flag & BUF_FLAG_FIRST_TSTAMP) {
        if (buf->type == BUF_TYPE_VIDEO) {
            es_vpts_checkin(buf, buf->first_tstamp);
        } else if (buf->type == BUF_TYPE_AUDIO) {
            es_apts_checkin(buf, buf->first_tstamp);
        }

        buf->flag &= ~BUF_FLAG_FIRST_TSTAMP;
    }
#endif

    if (atomic_read(&esparser_use_count) == 1) {
        r = request_irq(INT_PARSER, parser_isr,
                        IRQF_SHARED, "esparser", (void *)esparser_id);
        if (r) {
            printk("esparser_init: irq register failed.\n");
            goto Err_2;
        }

        WRITE_MPEG_REG(PARSER_INT_STATUS, 0xffff);
        WRITE_MPEG_REG(PARSER_INT_ENABLE,
                       PARSER_INTSTAT_SC_FOUND << PARSER_INT_HOST_EN_BIT);
    }

    return 0;

Err_2:
    pts_stop(pts_type);

Err_1:
    buf->flag &= ~BUF_FLAG_PARSER;
    return r;
}

void esparser_audio_reset(struct stream_buf_s *buf)
{
    ulong flags;
    spinlock_t lock = SPIN_LOCK_UNLOCKED;

    spin_lock_irqsave(&lock, flags);

    WRITE_MPEG_REG(PARSER_AUDIO_WP,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
    WRITE_MPEG_REG(PARSER_AUDIO_RP,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));

    WRITE_MPEG_REG(PARSER_AUDIO_START_PTR,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
    WRITE_MPEG_REG(PARSER_AUDIO_END_PTR,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR));
    CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

    WRITE_MPEG_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    buf->flag |= BUF_FLAG_PARSER;

    audio_data_parsed = 0;
    spin_unlock_irqrestore(&lock, flags);

    return;
}

void esparser_release(struct stream_buf_s *buf)
{
    u32 pts_type;

    /* check if esparser_init() is ever called */
    if ((buf->flag & BUF_FLAG_PARSER) == 0) {
        return;
    }

    if (atomic_dec_and_test(&esparser_use_count)) {
        WRITE_MPEG_REG(PARSER_INT_ENABLE, 0);
        free_irq(INT_PARSER, (void *)esparser_id);

        if (search_pattern) {
            dma_unmap_single(NULL, search_pattern_map, SEARCH_PATTERN_LEN, DMA_TO_DEVICE);
            kfree(search_pattern);
            search_pattern = NULL;
        }
    }

    if (buf->type == BUF_TYPE_VIDEO) {
        pts_type = PTS_TYPE_VIDEO;
    } else if (buf->type == BUF_TYPE_AUDIO) {
        pts_type = PTS_TYPE_AUDIO;
    } else if (buf->type == BUF_TYPE_SUBTITLE) {
        buf->flag &= ~BUF_FLAG_PARSER;
        return;
    } else {
        return;
    }

    buf->flag &= ~BUF_FLAG_PARSER;

    pts_stop(pts_type);
}

ssize_t esparser_write(struct file *file,
                       struct stream_buf_s *stbuf,
                       const char __user *buf, size_t count)
{
    s32 r;
    u32 len = count;
    if (buf == NULL || count == 0) {
        return -EINVAL;
    }

    if (stbuf_space(stbuf) < count) {
        if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }

        len = min(stbuf_size(stbuf) / 8, len);

        if (stbuf_space(stbuf) < len) {
            r = stbuf_wait_space(stbuf, len);
            if (r < 0) {
                return r;
            }
        }
    }
    mutex_lock(&esparser_mutex);
    r = _esparser_write(buf, len, stbuf->type);
    mutex_unlock(&esparser_mutex);

    return r;
}

void esparser_sub_reset(void)
{
    ulong flags;
    spinlock_t lock = SPIN_LOCK_UNLOCKED;
    u32 parser_sub_start_ptr;
    u32 parser_sub_end_ptr;

    spin_lock_irqsave(&lock, flags);

    parser_sub_start_ptr = READ_MPEG_REG(PARSER_SUB_START_PTR);
    parser_sub_end_ptr = READ_MPEG_REG(PARSER_SUB_END_PTR);

    WRITE_MPEG_REG(PARSER_SUB_START_PTR, parser_sub_start_ptr);
    WRITE_MPEG_REG(PARSER_SUB_END_PTR, parser_sub_end_ptr);
    WRITE_MPEG_REG(PARSER_SUB_RP, parser_sub_start_ptr);
    WRITE_MPEG_REG(PARSER_SUB_WP, parser_sub_start_ptr);
    SET_MPEG_REG_MASK(PARSER_ES_CONTROL, (7 << ES_SUB_WR_ENDIAN_BIT) | ES_SUB_MAN_RD_PTR);

    spin_unlock_irqrestore(&lock, flags);

    return;
}

