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
#include <linux/amports/ptsserv.h>
#include <linux/amports/amstream.h>
#include <linux/device.h>

#include <asm/uaccess.h>
#include <mach/am_regs.h>

#include "streambuf_reg.h"
#include "streambuf.h"

#include "tsdemux.h"

const static char tsdemux_fetch_id[] = "tsdemux-fetch-id";
const static char tsdemux_irq_id[] = "tsdemux-irq-id";

static DECLARE_WAIT_QUEUE_HEAD(wq);
static u32 fetch_done;
static u32 discontinued_counter;

static int demux_skipbyte;

#ifdef ENABLE_DEMUX_DRIVER
static struct tsdemux_ops *demux_ops = NULL;
static irq_handler_t       demux_handler;
static void               *demux_data;
static DEFINE_SPINLOCK(demux_ops_lock);

void tsdemux_set_ops(struct tsdemux_ops *ops)
{
    unsigned long flags;

    spin_lock_irqsave(&demux_ops_lock, flags);
    demux_ops = ops;
    spin_unlock_irqrestore(&demux_ops_lock, flags);
}

EXPORT_SYMBOL(tsdemux_set_ops);

int tsdemux_set_reset_flag(void)
{
    unsigned long flags;
    int r;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->set_reset_flag) {
        r = demux_ops->set_reset_flag();
    } else {
        WRITE_MPEG_REG(FEC_INPUT_CONTROL, 0);
        r = 0;
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);

    return r;
}

static int tsdemux_reset(void)
{
    unsigned long flags;
    int r;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->reset) {
        r = demux_ops->reset();
    } else {
        WRITE_MPEG_REG(RESET1_REGISTER, RESET_DEMUXSTB);
        WRITE_MPEG_REG(STB_TOP_CONFIG, 0);
        WRITE_MPEG_REG(DEMUX_CONTROL, 0);
        r = 0;
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);

    return r;
}

static irqreturn_t tsdemux_default_isr_handler(int irq, void *dev_id)
{
    u32 int_status = READ_MPEG_REG(STB_INT_STATUS);

    if (demux_handler) {
        demux_handler(irq, (void*)0);
    }

    WRITE_MPEG_REG(STB_INT_STATUS, int_status);
    return IRQ_HANDLED;
}

static int tsdemux_request_irq(irq_handler_t handler, void *data)
{
    unsigned long flags;
    int r;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->request_irq) {
        r = demux_ops->request_irq(handler, data);
    } else {
        demux_handler = handler;
        demux_data    = data;
        r = request_irq(INT_DEMUX, tsdemux_default_isr_handler,
                        IRQF_SHARED, "tsdemux-irq",
                        (void *)tsdemux_irq_id);
        WRITE_MPEG_REG(STB_INT_MASK,
                       (1 << SUB_PES_READY)
                       | (1 << NEW_PDTS_READY)
                       | (1 << DIS_CONTINUITY_PACKET));
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);

    return r;
}

static int tsdemux_free_irq(void)
{
    unsigned long flags;
    int r;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->free_irq) {
        r = demux_ops->free_irq();
    } else {
        WRITE_MPEG_REG(STB_INT_MASK, 0);
        free_irq(INT_DEMUX, (void *)tsdemux_irq_id);
        r = 0;
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);

    return r;
}

static int tsdemux_set_vid(int vpid)
{
    unsigned long flags;
    int r = 0;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->set_vid) {
        r = demux_ops->set_vid(vpid);
    } else if ((vpid >= 0) && (vpid < 0x1FFF)) {
        u32 data = READ_MPEG_REG(FM_WR_DATA);
        WRITE_MPEG_REG(FM_WR_DATA,
                       (((vpid & 0x1fff) | (VIDEO_PACKET << 13)) << 16) |
                       (data & 0xffff));
        WRITE_MPEG_REG(FM_WR_ADDR, 0x8000);
        while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000) {
            ;
        }
        WRITE_MPEG_REG(MAX_FM_COMP_ADDR, 1);
        r = 0;
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);

    return r;
}

static int tsdemux_set_aid(int apid)
{
    unsigned long flags;
    int r = 0;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->set_aid) {
        r = demux_ops->set_aid(apid);
    } else if ((apid >= 0) && (apid < 0x1FFF)) {
        u32 data = READ_MPEG_REG(FM_WR_DATA);
        WRITE_MPEG_REG(FM_WR_DATA,
                       ((apid & 0x1fff) | (AUDIO_PACKET << 13)) |
                       (data & 0xffff0000));
        WRITE_MPEG_REG(FM_WR_ADDR, 0x8000);
        while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000) {
            ;
        }
        WRITE_MPEG_REG(MAX_FM_COMP_ADDR, 1);
        r = 0;
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);

    return r;
}

static int tsdemux_set_sid(int spid)
{
    unsigned long flags;
    int r = 0;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->set_sid) {
        r = demux_ops->set_sid(spid);
    } else if ((spid >= 0) && (spid < 0x1FFF)) {
        WRITE_MPEG_REG(FM_WR_DATA,
                       (((spid & 0x1fff) | (SUB_PACKET << 13)) << 16) | 0xffff);
        WRITE_MPEG_REG(FM_WR_ADDR, 0x8001);
        while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000) {
            ;
        }
        WRITE_MPEG_REG(MAX_FM_COMP_ADDR, 1);
        r = 0;
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);

    return r;
}
static int tsdemux_set_skip_byte(int skipbyte)
{
    unsigned long flags;
    int r = 0;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->set_skipbyte) {
        r = demux_ops->set_skipbyte(skipbyte);
    }else{
        demux_skipbyte = skipbyte;
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);

    return r;
}

static int tsdemux_config(void)
{
    unsigned long flags;

    spin_lock_irqsave(&demux_ops_lock, flags);

    if (!demux_ops) {
        WRITE_MPEG_REG(STB_INT_MASK, 0);
        WRITE_MPEG_REG(STB_INT_STATUS, 0xffff);

        /* TS data path */
        WRITE_MPEG_REG(FEC_INPUT_CONTROL, 0x7000);
        WRITE_MPEG_REG(DEMUX_MEM_REQ_EN,
                       (1 << VIDEO_PACKET) |
                       (1 << AUDIO_PACKET) |
                       (1 << SUB_PACKET));
        WRITE_MPEG_REG(DEMUX_ENDIAN,
                       (7 << OTHER_ENDIAN)  |
                       (7 << BYPASS_ENDIAN) |
                       (0 << SECTION_ENDIAN));
        WRITE_MPEG_REG(TS_HIU_CTL, 1 << USE_HI_BSF_INTERFACE);
        WRITE_MPEG_REG(TS_FILE_CONFIG,
                       (demux_skipbyte << 16)                  |
                       (6 << DES_OUT_DLY)                      |
                       (3 << TRANSPORT_SCRAMBLING_CONTROL_ODD) |
                       (1 << TS_HIU_ENABLE)                    |
                       (4 << FEC_FILE_CLK_DIV));

        /* enable TS demux */
        WRITE_MPEG_REG(DEMUX_CONTROL, (1 << STB_DEMUX_ENABLE));
    }

    spin_unlock_irqrestore(&demux_ops_lock, flags);
    return 0;
}
#endif /*ENABLE_DEMUX_DRIVER*/

static irqreturn_t tsdemux_isr(int irq, void *dev_id)
{
#ifndef ENABLE_DEMUX_DRIVER
    u32 int_status = READ_MPEG_REG(STB_INT_STATUS);
#else
    int id = (int)dev_id;
    u32 int_status = id ? READ_MPEG_REG(STB_INT_STATUS_2) : READ_MPEG_REG(STB_INT_STATUS);
#endif

    if (int_status & (1 << NEW_PDTS_READY)) {
#ifndef ENABLE_DEMUX_DRIVER
        u32 pdts_status = READ_MPEG_REG(STB_PTS_DTS_STATUS);

        if (pdts_status & (1 << VIDEO_PTS_READY))
            pts_checkin_wrptr(PTS_TYPE_VIDEO,
                              READ_MPEG_REG(VIDEO_PDTS_WR_PTR),
                              READ_MPEG_REG(VIDEO_PTS_DEMUX));

        if (pdts_status & (1 << AUDIO_PTS_READY))
            pts_checkin_wrptr(PTS_TYPE_AUDIO,
                              READ_MPEG_REG(AUDIO_PDTS_WR_PTR),
                              READ_MPEG_REG(AUDIO_PTS_DEMUX));

        WRITE_MPEG_REG(STB_PTS_DTS_STATUS, pdts_status);
#else
        u32 pdts_status = id ? READ_MPEG_REG(STB_PTS_DTS_STATUS_2) : READ_MPEG_REG(STB_PTS_DTS_STATUS);

        if (pdts_status & (1 << VIDEO_PTS_READY))
            pts_checkin_wrptr(PTS_TYPE_VIDEO,
                              id ? READ_MPEG_REG(VIDEO_PDTS_WR_PTR_2) : READ_MPEG_REG(VIDEO_PDTS_WR_PTR),
                              id ? READ_MPEG_REG(VIDEO_PTS_DEMUX_2) : READ_MPEG_REG(VIDEO_PTS_DEMUX));

        if (pdts_status & (1 << AUDIO_PTS_READY))
            pts_checkin_wrptr(PTS_TYPE_AUDIO,
                              id ? READ_MPEG_REG(AUDIO_PDTS_WR_PTR_2) : READ_MPEG_REG(AUDIO_PDTS_WR_PTR),
                              id ? READ_MPEG_REG(AUDIO_PTS_DEMUX_2) : READ_MPEG_REG(AUDIO_PTS_DEMUX));

        if (id) {
            WRITE_MPEG_REG(STB_PTS_DTS_STATUS_2, pdts_status);
        } else {
            WRITE_MPEG_REG(STB_PTS_DTS_STATUS, pdts_status);
        }
#endif
    }
    if (int_status & (1 << DIS_CONTINUITY_PACKET)) {
        discontinued_counter++;
        //printk("discontinued counter=%d\n",discontinued_counter);
    }
    if (int_status & (1 << SUB_PES_READY)) {
        /* TODO: put data to somewhere */
        //printk("subtitle pes ready\n");
        wakeup_sub_poll();
    }
#ifndef ENABLE_DEMUX_DRIVER
    WRITE_MPEG_REG(STB_INT_STATUS, int_status);
#endif
    return IRQ_HANDLED;
}

static irqreturn_t parser_isr(int irq, void *dev_id)
{
    u32 int_status = READ_MPEG_REG(PARSER_INT_STATUS);

    WRITE_MPEG_REG(PARSER_INT_STATUS, int_status);

    if (int_status & PARSER_INTSTAT_FETCH_CMD) {
        fetch_done = 1;

        wake_up_interruptible(&wq);
    }


    return IRQ_HANDLED;
}

static ssize_t _tsdemux_write(const char __user *buf, size_t count)
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
        WRITE_MPEG_REG(PARSER_FETCH_CMD, (7 << FETCH_ENDIAN) | len);

        ret = wait_event_interruptible_timeout(wq, fetch_done != 0, HZ/10);
        if (ret == 0) {
            WRITE_MPEG_REG(PARSER_FETCH_CMD, 0);
			printk("write timeout, retry\n");
            return -EAGAIN;
        } else if (ret < 0) {
            return -ERESTARTSYS;
        }

        p += len;
        r -= len;
    }

    return count - r;
}

s32 tsdemux_init(u32 vid, u32 aid, u32 sid)
{
    s32 r;
    u32 parser_sub_start_ptr;
    u32 parser_sub_end_ptr;
    u32 parser_sub_rp;

    parser_sub_start_ptr = READ_MPEG_REG(PARSER_SUB_START_PTR);
    parser_sub_end_ptr = READ_MPEG_REG(PARSER_SUB_END_PTR);
    parser_sub_rp = READ_MPEG_REG(PARSER_SUB_RP);

    WRITE_MPEG_REG(RESET1_REGISTER, RESET_PARSER);

#ifdef ENABLE_DEMUX_DRIVER
    tsdemux_reset();
#else
    WRITE_MPEG_REG(RESET1_REGISTER, RESET_PARSER | RESET_DEMUXSTB);

    WRITE_MPEG_REG(STB_TOP_CONFIG, 0);
    WRITE_MPEG_REG(DEMUX_CONTROL, 0);
#endif

    /* set PID filter */
    printk("tsdemux video_pid = 0x%x, audio_pid = 0x%x, sub_pid = 0x%x\n",
           vid, aid, sid);
#ifndef ENABLE_DEMUX_DRIVER
    WRITE_MPEG_REG(FM_WR_DATA,
                   (((vid & 0x1fff) | (VIDEO_PACKET << 13)) << 16) |
                   ((aid & 0x1fff) | (AUDIO_PACKET << 13)));
    WRITE_MPEG_REG(FM_WR_ADDR, 0x8000);
    while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000) {
        ;
    }

    WRITE_MPEG_REG(FM_WR_DATA,
                   (((sid & 0x1fff) | (SUB_PACKET << 13)) << 16) | 0xffff);
    WRITE_MPEG_REG(FM_WR_ADDR, 0x8001);
    while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000) {
        ;
    }

    WRITE_MPEG_REG(MAX_FM_COMP_ADDR, 1);

    WRITE_MPEG_REG(STB_INT_MASK, 0);
    WRITE_MPEG_REG(STB_INT_STATUS, 0xffff);

    /* TS data path */
    WRITE_MPEG_REG(FEC_INPUT_CONTROL, 0x7000);
    WRITE_MPEG_REG(DEMUX_MEM_REQ_EN,
                   (1 << VIDEO_PACKET) |
                   (1 << AUDIO_PACKET) |
                   (1 << SUB_PACKET));
    WRITE_MPEG_REG(DEMUX_ENDIAN,
                   (7 << OTHER_ENDIAN)  |
                   (7 << BYPASS_ENDIAN) |
                   (0 << SECTION_ENDIAN));
    WRITE_MPEG_REG(TS_HIU_CTL, 1 << USE_HI_BSF_INTERFACE);
    WRITE_MPEG_REG(TS_FILE_CONFIG,
                   (demux_skipbyte << 16)                  |
                   (6 << DES_OUT_DLY)                      |
                   (3 << TRANSPORT_SCRAMBLING_CONTROL_ODD) |
                   (1 << TS_HIU_ENABLE)                    |
                   (4 << FEC_FILE_CLK_DIV));

    /* enable TS demux */
    WRITE_MPEG_REG(DEMUX_CONTROL, (1 << STB_DEMUX_ENABLE) | (1 << KEEP_DUPLICATE_PACKAGE));
#endif

    if (fetchbuf == 0) {
        printk("%s: no fetchbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

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

    WRITE_MPEG_REG(PARSER_CONFIG,
                   (10 << PS_CFG_PFIFO_EMPTY_CNT_BIT) |
                   (1  << PS_CFG_MAX_ES_WR_CYCLE_BIT) |
                   (16 << PS_CFG_MAX_FETCH_CYCLE_BIT));

    WRITE_MPEG_REG(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    WRITE_MPEG_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    WRITE_MPEG_REG(PARSER_SUB_START_PTR, parser_sub_start_ptr);
    WRITE_MPEG_REG(PARSER_SUB_END_PTR, parser_sub_end_ptr);
    WRITE_MPEG_REG(PARSER_SUB_RP, parser_sub_rp);
    SET_MPEG_REG_MASK(PARSER_ES_CONTROL, (7 << ES_SUB_WR_ENDIAN_BIT) | ES_SUB_MAN_RD_PTR);

    if ((r = pts_start(PTS_TYPE_VIDEO)) < 0) {
        printk("Video pts start  failed.(%d)\n", r);
        goto err1;
    }

    if ((r = pts_start(PTS_TYPE_AUDIO)) < 0) {
        printk("Audio pts start failed.(%d)\n", r);
        goto err2;
    }

    r = request_irq(INT_PARSER, parser_isr,
                    IRQF_SHARED, "tsdemux-fetch",
                    (void *)tsdemux_fetch_id);
    if (r) {
        goto err3;
    }

    WRITE_MPEG_REG(PARSER_INT_STATUS, 0xffff);
    WRITE_MPEG_REG(PARSER_INT_ENABLE, PARSER_INTSTAT_FETCH_CMD << PARSER_INT_HOST_EN_BIT);

    WRITE_MPEG_REG(PARSER_VIDEO_HOLE, 0x400);
    WRITE_MPEG_REG(PARSER_AUDIO_HOLE, 0x400);

    discontinued_counter = 0;
#ifndef ENABLE_DEMUX_DRIVER
    r = request_irq(INT_DEMUX, tsdemux_isr,
                    IRQF_SHARED, "tsdemux-irq",
                    (void *)tsdemux_irq_id);
    WRITE_MPEG_REG(STB_INT_MASK,
                   (1 << SUB_PES_READY)
                   | (1 << NEW_PDTS_READY)
                   | (1 << DIS_CONTINUITY_PACKET));
    if (r) {
        goto err4;
    }
#else
    tsdemux_config();
    tsdemux_request_irq(tsdemux_isr, (void *)tsdemux_irq_id);
    if (vid < 0x1FFF) {
        tsdemux_set_vid(vid);
    }
    if (aid < 0x1FFF) {
        tsdemux_set_aid(aid);
    }
    if (sid < 0x1FFF) {
        tsdemux_set_sid(sid);
    }

#endif

    return 0;

#ifndef ENABLE_DEMUX_DRIVER
err4:
    free_irq(INT_PARSER, (void *)tsdemux_fetch_id);
#endif
err3:
    pts_stop(PTS_TYPE_AUDIO);
err2:
    pts_stop(PTS_TYPE_VIDEO);
err1:
    printk("TS Demux init failed.\n");
    return -ENOENT;
}

void tsdemux_release(void)
{


    WRITE_MPEG_REG(PARSER_INT_ENABLE, 0);
    WRITE_MPEG_REG(PARSER_VIDEO_HOLE, 0);
    WRITE_MPEG_REG(PARSER_AUDIO_HOLE, 0);
    free_irq(INT_PARSER, (void *)tsdemux_fetch_id);

#ifndef ENABLE_DEMUX_DRIVER
    WRITE_MPEG_REG(STB_INT_MASK, 0);
    free_irq(INT_DEMUX, (void *)tsdemux_irq_id);
#else

    tsdemux_set_aid(0xffff);
    tsdemux_set_vid(0xffff);
    tsdemux_set_sid(0xffff);
    tsdemux_free_irq();

#endif


    pts_stop(PTS_TYPE_VIDEO);
    pts_stop(PTS_TYPE_AUDIO);

}

ssize_t tsdemux_write(struct file *file,
                      struct stream_buf_s *vbuf,
                      struct stream_buf_s *abuf,
                      const char __user *buf, size_t count)
{
    s32 r;
    stream_port_t *port = (stream_port_t *)file->private_data;
    size_t wait_size, write_size;

    if ((stbuf_space(vbuf) < count) ||
        (stbuf_space(abuf) < count)) {
        if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }

        wait_size = min(stbuf_size(vbuf) / 8, stbuf_size(abuf) / 4);
        if ((port->flag & PORT_FLAG_VID)
            && (stbuf_space(vbuf) < wait_size)) {
            r = stbuf_wait_space(vbuf, wait_size);

            if (r < 0) {
                return r;
            }
        }

        if ((port->flag & PORT_FLAG_AID)
            && (stbuf_space(abuf) < wait_size)) {
            r = stbuf_wait_space(abuf, wait_size);

            if (r < 0) {
                return r;
            }
        }
    }
    write_size = min(stbuf_space(vbuf), stbuf_space(abuf));
    write_size = min(count, write_size);
    if (write_size > 0) {
        return _tsdemux_write(buf, write_size);
    } else {
        return -EAGAIN;
    }
}

static ssize_t show_discontinue_counter(struct class *class, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", discontinued_counter);
}

static struct class_attribute tsdemux_class_attrs[] = {
    __ATTR(discontinue_counter,  S_IRUGO, show_discontinue_counter, NULL),
    __ATTR_NULL
};

static struct class tsdemux_class = {
        .name = "tsdemux",
        .class_attrs = tsdemux_class_attrs,
    };


int     tsdemux_class_register(void)
{
    int r;
    if ((r = class_register(&tsdemux_class)) < 0) {
        printk("register tsdemux class error!\n");
    }
    discontinued_counter = 0;
    return r;
}
void  tsdemux_class_unregister(void)
{
    class_unregister(&tsdemux_class);
}

void tsdemux_change_avid(unsigned int vid, unsigned int aid)
{
#ifndef ENABLE_DEMUX_DRIVER
    WRITE_MPEG_REG(FM_WR_DATA,
                   (((vid & 0x1fff) | (VIDEO_PACKET << 13)) << 16) |
                   ((aid & 0x1fff) | (AUDIO_PACKET << 13)));
    WRITE_MPEG_REG(FM_WR_ADDR, 0x8000);
    while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000) {
        ;
    }
#else
    tsdemux_set_vid(vid);
    tsdemux_set_aid(aid);
#endif
    return;
}

void tsdemux_change_sid(unsigned int sid)
{
#ifndef ENABLE_DEMUX_DRIVER
    WRITE_MPEG_REG(FM_WR_DATA,
                   (((sid & 0x1fff) | (SUB_PACKET << 13)) << 16) | 0xffff);
    WRITE_MPEG_REG(FM_WR_ADDR, 0x8001);
    while (READ_MPEG_REG(FM_WR_ADDR) & 0x8000) {
        ;
    }
#else
    tsdemux_set_sid(sid);
#endif
    return;
}

void tsdemux_audio_reset(void)
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

void tsdemux_sub_reset(void)
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

void tsdemux_set_skipbyte(int skipbyte)
{
#ifndef ENABLE_DEMUX_DRIVER
    demux_skipbyte = skipbyte;
#else
    tsdemux_set_skip_byte(skipbyte);
#endif
    return;
}

void tsdemux_set_demux(int dev)
{
#ifdef ENABLE_DEMUX_DRIVER
    unsigned long flags;
    int r = 0;

    spin_lock_irqsave(&demux_ops_lock, flags);
    if (demux_ops && demux_ops->set_demux) {
        r = demux_ops->set_demux(dev);
    }
    spin_unlock_irqrestore(&demux_ops_lock, flags);
#endif
}

