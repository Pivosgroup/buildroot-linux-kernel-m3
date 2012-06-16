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
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/amports/amstream.h>
#include <linux/amports/ptsserv.h>
#include <mach/am_regs.h>
#include <asm/uaccess.h>

#include "streambuf.h"
#include "streambuf_reg.h"

#define MANAGE_PTS

static u32 fetch_done;
static u32 parse_halt;

static DECLARE_WAIT_QUEUE_HEAD(rm_wq);
const static char rmparser_id[] = "rmparser-id";

static irqreturn_t rm_parser_isr(int irq, void *dev_id)

{
    u32 int_status = READ_MPEG_REG(PARSER_INT_STATUS);

    if (int_status & PARSER_INTSTAT_FETCH_CMD) {
        WRITE_MPEG_REG(PARSER_INT_STATUS, PARSER_INTSTAT_FETCH_CMD);
        fetch_done = 1;

        wake_up_interruptible(&rm_wq);
    }

    return IRQ_HANDLED;
}

#ifdef CONFIG_AM_DVB
extern int tsdemux_set_reset_flag(void);
#endif

s32 rmparser_init(void)
{
    s32 r;
    parse_halt = 0;
    if (fetchbuf == 0) {
        printk("%s: no fetchbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

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

    /* hook stream buffer with PARSER */
    WRITE_MPEG_REG(PARSER_VIDEO_START_PTR,
                   READ_MPEG_REG(VLD_MEM_VIFIFO_START_PTR));
    WRITE_MPEG_REG(PARSER_VIDEO_END_PTR,
                   READ_MPEG_REG(VLD_MEM_VIFIFO_END_PTR));
    CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

    WRITE_MPEG_REG(PARSER_AUDIO_START_PTR,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
    WRITE_MPEG_REG(PARSER_AUDIO_END_PTR,
                   READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR));
    CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

    WRITE_MPEG_REG(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    WRITE_MPEG_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    WRITE_MPEG_REG(PFIFO_RD_PTR, 0);
    WRITE_MPEG_REG(PFIFO_WR_PTR, 0);

    WRITE_MPEG_REG(PARSER_SEARCH_MASK, 0);
    WRITE_MPEG_REG(PARSER_CONTROL, (ES_SEARCH | ES_PARSER_START));

#ifdef MANAGE_PTS
    if (pts_start(PTS_TYPE_VIDEO) < 0) {
        goto Err_1;
    }

    if (pts_start(PTS_TYPE_AUDIO) < 0) {
        goto Err_2;
    }
#endif

    /* enable interrupt */
    r = request_irq(INT_PARSER, rm_parser_isr,
                    IRQF_SHARED, "rmparser", (void *)rmparser_id);

    if (r) {
        printk("RM parser irq register failed.\n");
        goto Err_3;
    }

    WRITE_MPEG_REG(PARSER_INT_STATUS, 0xffff);
    WRITE_MPEG_REG(PARSER_INT_ENABLE,
                   ((PARSER_INT_ALL & (~PARSER_INTSTAT_FETCH_CMD)) << PARSER_INT_AMRISC_EN_BIT)
                   | (PARSER_INTSTAT_FETCH_CMD << PARSER_INT_HOST_EN_BIT));

    return 0;

Err_3:
    pts_stop(PTS_TYPE_AUDIO);
Err_2:
    pts_stop(PTS_TYPE_VIDEO);
Err_1:
    return -ENOENT;
}

void rmparser_release(void)
{
    WRITE_MPEG_REG(PARSER_INT_ENABLE, 0);

    free_irq(INT_PARSER, (void *)rmparser_id);

#ifdef MANAGE_PTS
    pts_stop(PTS_TYPE_VIDEO);
    pts_stop(PTS_TYPE_AUDIO);
#endif

    return;
}

static ssize_t _rmparser_write(const char __user *buf, size_t count)
{
    size_t r = count;
    const char __user *p = buf;
    u32 len;
    int ret;

    if (r > 0) {
        len = min(r, (size_t)FETCHBUF_SIZE);

        copy_from_user(fetchbuf_remap, p, len);

        fetch_done = 0;

        wmb();

        WRITE_MPEG_REG(PARSER_FETCH_ADDR, fetchbuf);
        WRITE_MPEG_REG(PARSER_FETCH_CMD,
                       (7 << FETCH_ENDIAN) | len);

        ret = wait_event_interruptible_timeout(rm_wq, fetch_done != 0, HZ/10);
        if (ret == 0) {
            WRITE_MPEG_REG(PARSER_FETCH_CMD, 0);
            parse_halt ++;
			printk("write timeout, retry,halt_count=%d parse_control=%x \n",
			    parse_halt,READ_MPEG_REG(PARSER_CONTROL));
			
			if(parse_halt > 10) {			    
			    WRITE_MPEG_REG(PARSER_CONTROL, (ES_SEARCH | ES_PARSER_START));
			    printk("reset parse_control=%x\n",READ_MPEG_REG(PARSER_CONTROL));
			}			
            return -EAGAIN;
        } else if (ret < 0) {
            return -ERESTARTSYS;
        }

        p += len;
        r -= len;
        parse_halt = 0;
    }   
    return count - r;
}

ssize_t rmparser_write(struct file *file,
                       struct stream_buf_s *vbuf,
                       struct stream_buf_s *abuf,
                       const char __user *buf, size_t count)
{
    s32 r;
    stream_port_t *port = (stream_port_t *)file->private_data;

    if ((stbuf_space(vbuf) < count) ||
        (stbuf_space(abuf) < count)) {
        if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }

        if ((port->flag & PORT_FLAG_VID)
            && (stbuf_space(vbuf) < count)) {
            r = stbuf_wait_space(vbuf, count);
            if (r < 0) {
                return r;
            }
        }
        if ((port->flag & PORT_FLAG_AID)
            && (stbuf_space(abuf) < count)) {
            r = stbuf_wait_space(abuf, count);
            if (r < 0) {
                return r;
            }
        }
    }

    return _rmparser_write(buf, count);
}

void rm_set_vasid(u32 vid, u32 aid)
{
    printk("rm_set_vasid aid %d, vid %d\n", aid, vid);
    WRITE_MPEG_REG(VAS_STREAM_ID, (aid << 8) | vid);

    return;
}

void rm_audio_reset(void)
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

    spin_unlock_irqrestore(&lock, flags);

    return;
}
