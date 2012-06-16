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
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/amports/ptsserv.h>

#include <asm/cacheflush.h>
#include <asm/uaccess.h>
#include <mach/am_regs.h>

#include "streambuf_reg.h"
#include "streambuf.h"

#define STBUF_SIZE   (64*1024)
#define STBUF_WAIT_INTERVAL  HZ/100

ulong fetchbuf = 0;
ulong *fetchbuf_remap = NULL;

static s32 _stbuf_alloc(stream_buf_t *buf)
{
    if (buf->buf_size == 0) {
        return -ENOBUFS;
    }

    if (buf->buf_start == 0) {
        buf->buf_start = __get_free_pages(GFP_KERNEL, get_order(buf->buf_size));

        if (!buf->buf_start) {
            return -ENOMEM;
        }

        printk("%s stbuf alloced at 0x%x, size = %d\n",
               (buf->type == BUF_TYPE_VIDEO) ? "Video" : (buf->type == BUF_TYPE_AUDIO) ? "Audio" : "Subtitle",
               buf->buf_start, buf->buf_size);
    }

    buf->flag |= BUF_FLAG_ALLOC;

    return 0;
}

int stbuf_change_size(struct stream_buf_s *buf, int size)
{
    u32 old_buf, old_size;
    int ret;

    printk("buffersize=%d,%d,start=%x\n", size, buf->buf_size, buf->buf_start);

    if (buf->buf_size == size && buf->buf_start != 0) {
        return 0;
    }

    old_buf = buf->buf_start;
    old_size = buf->buf_size;

    buf->buf_start = 0;
    buf->buf_size = size;
    ret = size;

    if (size == 0 || ((ret = _stbuf_alloc(buf)) == 0)) {
        /*
         * size=0:We only free the old memory;
         * alloc ok,changed to new buffer
         */
        if (old_buf != 0) {
            free_pages(old_buf, get_order(old_size));
        }

        printk("changed the (%d) buffer size from %d to %d\n", buf->type, old_size, size);
    } else {
        /* alloc failed */
        buf->buf_start = old_buf;
        buf->buf_size = old_size;
        printk("changed the (%d) buffer size from %d to %d,failed\n", buf->type, old_size, size);
    }

    return ret;
}

int stbuf_fetch_init(void)
{
    if (0 != fetchbuf) {
        return 0;
    }

    fetchbuf = __get_free_pages(GFP_KERNEL, get_order(FETCHBUF_SIZE));

    if (!fetchbuf) {
        printk("%s: Can not allocate fetch working buffer\n", __FUNCTION__);
        return -ENOMEM;
    }

    fetchbuf_remap = ioremap_nocache(virt_to_phys((u8 *)fetchbuf), FETCHBUF_SIZE);
    if (!fetchbuf_remap) {
        printk("%s: Can not remap fetch working buffer\n", __FUNCTION__);
        free_pages(fetchbuf, get_order(FETCHBUF_SIZE));

        fetchbuf = 0;

        return -ENOMEM;
    }

    return 0;
}

void stbuf_fetch_release(void)
{
    if (fetchbuf_remap) {
        iounmap(fetchbuf_remap);
        fetchbuf_remap = 0;
    }

    if (fetchbuf) {
        free_pages(fetchbuf, get_order(FETCHBUF_SIZE));
        fetchbuf = 0;
    }

    return;
}

static inline u32 _stbuf_wp(stream_buf_t *buf)
{
    return _READ_ST_REG(WP);
}

static void _stbuf_timer_func(unsigned long arg)
{
    struct stream_buf_s *p = (struct stream_buf_s *)arg;

    if (stbuf_space(p) < p->wcnt) {
        p->timer.expires = jiffies + STBUF_WAIT_INTERVAL;

        add_timer(&p->timer);
    } else {
        wake_up_interruptible(&p->wq);
    }

}

u32 stbuf_level(struct stream_buf_s *buf)
{
    return _READ_ST_REG(LEVEL);
}

u32 stbuf_rp(struct stream_buf_s *buf)
{
    return _READ_ST_REG(RP);
}

u32 stbuf_space(struct stream_buf_s *buf)
{
    /* reserved space for safe write, the parser fifo size is 1024byts, so reserve it */
    int size = (buf->buf_size - _READ_ST_REG(LEVEL)) - 1024;

    return size > 0 ? size : 0;
}

u32 stbuf_size(struct stream_buf_s *buf)
{
    return buf->buf_size;
}

s32 stbuf_init(struct stream_buf_s *buf)
{
    s32 r;
    u32 dummy;
    u32 phy_addr;

    if (buf->flag & BUF_FLAG_IOMEM) {
        phy_addr = buf->buf_start;
    } else {
        r = _stbuf_alloc(buf);
        if (r < 0) {
            return r;
        }
        phy_addr = virt_to_phys((void *)buf->buf_start);
    }

    init_waitqueue_head(&buf->wq);

    _WRITE_ST_REG(CONTROL, 0);

    if (buf->type == BUF_TYPE_VIDEO) {
        /* reset VLD before setting all pointers */
        WRITE_MPEG_REG(RESET0_REGISTER, RESET_VLD);
        dummy = READ_MPEG_REG(RESET0_REGISTER);
        WRITE_MPEG_REG(POWER_CTL_VLD, 1 << 4);
    } else if (buf->type == BUF_TYPE_AUDIO) {
        WRITE_MPEG_REG(AIU_AIFIFO_GBIT, 0x80);
    }

    if (buf->type == BUF_TYPE_SUBTITLE) {
        WRITE_MPEG_REG(PARSER_SUB_RP, phy_addr);
        WRITE_MPEG_REG(PARSER_SUB_START_PTR, phy_addr);
        WRITE_MPEG_REG(PARSER_SUB_END_PTR, phy_addr + buf->buf_size - 8);

        return 0;
    }

    _WRITE_ST_REG(START_PTR, phy_addr);
    _WRITE_ST_REG(CURR_PTR, phy_addr);
    _WRITE_ST_REG(END_PTR, phy_addr + buf->buf_size - 8);

    _SET_ST_REG_MASK(CONTROL, MEM_BUFCTRL_INIT);
    _CLR_ST_REG_MASK(CONTROL, MEM_BUFCTRL_INIT);

    _WRITE_ST_REG(BUF_CTRL, MEM_BUFCTRL_MANUAL);
    _WRITE_ST_REG(WP, phy_addr);

    _SET_ST_REG_MASK(BUF_CTRL, MEM_BUFCTRL_INIT);
    _CLR_ST_REG_MASK(BUF_CTRL, MEM_BUFCTRL_INIT);

    _SET_ST_REG_MASK(CONTROL, MEM_FILL_ON_LEVEL | MEM_CTRL_FILL_EN | MEM_CTRL_EMPTY_EN);

    return 0;
}

s32 stbuf_wait_space(struct stream_buf_s *stream_buf, size_t count)
{
    stream_buf_t *p = stream_buf;
    long time_out = 20;

    p->wcnt = count;

    setup_timer(&p->timer, _stbuf_timer_func, (ulong)p);

    mod_timer(&p->timer, jiffies + STBUF_WAIT_INTERVAL);
    
    if (wait_event_interruptible_timeout(p->wq, stbuf_space(p) >= count, time_out) == 0) {
        del_timer_sync(&p->timer);

        return -EAGAIN;
    }

    del_timer_sync(&p->timer);

    return 0;
}

void stbuf_release(struct stream_buf_s *buf)
{
    buf->first_tstamp = INVALID_PTS;
	stbuf_init(buf);	//reinit buffer 
    if (buf->flag & BUF_FLAG_IOMEM) {
        buf->flag = BUF_FLAG_IOMEM;
    } else {
        buf->flag = 0;
    }
}

u32 stbuf_sub_rp_get(void)
{
    return READ_MPEG_REG(PARSER_SUB_RP);
}

void stbuf_sub_rp_set(unsigned int sub_rp)
{
    WRITE_MPEG_REG(PARSER_SUB_RP, sub_rp);
    return;
}

u32 stbuf_sub_wp_get(void)
{
    return READ_MPEG_REG(PARSER_SUB_WP);
}

u32 stbuf_sub_start_get(void)
{
    return READ_MPEG_REG(PARSER_SUB_START_PTR);
}
