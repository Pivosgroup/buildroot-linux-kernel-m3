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
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/amports/vformat.h>
#include <linux/clk.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#ifdef CONFIG_WAKELOCK
#include <linux/wakelock.h>
#endif

#include <mach/am_regs.h>
#include <mach/power_gate.h>
#include "amvdec.h"

#define MC_SIZE (4096 * 4)

#ifdef CONFIG_WAKELOCK
static struct wake_lock amvdec_lock;
struct timer_list amvdevtimer;
#define WAKE_CHECK_INTERVAL (100*HZ/100)
#endif
#define AMVDEC_USE_STATIC_MEMORY
static void *mc_addr=NULL;
static dma_addr_t mc_addr_map;

static int video_running=0;
static int video_stated_changed=1;

static void amvdec_pg_enable(bool enable)
{
    ulong timeout;

    if (enable) {
        CLK_GATE_ON(MDEC_CLK_PIC_DC);
        CLK_GATE_ON(MDEC_CLK_DBLK);
        CLK_GATE_ON(MC_CLK);
        CLK_GATE_ON(IQIDCT_CLK);
        //CLK_GATE_ON(VLD_CLK);
        CLK_GATE_ON(AMRISC);
    } else {
        CLK_GATE_OFF(AMRISC);

        timeout = jiffies + HZ / 10;

        while (READ_MPEG_REG(MDEC_PIC_DC_STATUS) != 0) {
            if (time_after(jiffies, timeout)) {
                WRITE_MPEG_REG_BITS(MDEC_PIC_DC_CTRL, 1, 0, 1);
                WRITE_MPEG_REG_BITS(MDEC_PIC_DC_CTRL, 0, 0, 1);
                READ_MPEG_REG(MDEC_PIC_DC_STATUS);
                READ_MPEG_REG(MDEC_PIC_DC_STATUS);
                READ_MPEG_REG(MDEC_PIC_DC_STATUS);
                break;
            }
        }

        CLK_GATE_OFF(MDEC_CLK_PIC_DC);

        timeout = jiffies + HZ / 10;

        while (READ_MPEG_REG(DBLK_STATUS) & 1) {
            if (time_after(jiffies, timeout)) {
                WRITE_MPEG_REG(DBLK_CTRL, 3);
                WRITE_MPEG_REG(DBLK_CTRL, 0);
                READ_MPEG_REG(DBLK_STATUS);
                READ_MPEG_REG(DBLK_STATUS);
                READ_MPEG_REG(DBLK_STATUS);
                break;
            }
        }
        CLK_GATE_OFF(MDEC_CLK_DBLK);

        timeout = jiffies + HZ / 10;

        while (READ_MPEG_REG(MC_STATUS0) & 1) {
            if (time_after(jiffies, timeout)) {
                SET_MPEG_REG_MASK(MC_CTRL1, 0x9);
                CLEAR_MPEG_REG_MASK(MC_CTRL1, 0x9);
                READ_MPEG_REG(MC_STATUS0);
                READ_MPEG_REG(MC_STATUS0);
                READ_MPEG_REG(MC_STATUS0);
                break;
            }
        }
        CLK_GATE_OFF(MC_CLK);

        timeout = jiffies + HZ / 10;
        while (READ_MPEG_REG(DCAC_DMA_CTRL) & 0x8000) {
            if (time_after(jiffies, timeout)) {
                break;
            }
        }

        WRITE_MPEG_REG(RESET0_REGISTER, 4);
        READ_MPEG_REG(RESET0_REGISTER);
        READ_MPEG_REG(RESET0_REGISTER);
        READ_MPEG_REG(RESET0_REGISTER);
        CLK_GATE_OFF(IQIDCT_CLK);

        //CLK_GATE_OFF(VLD_CLK);
    }
}

#ifdef CONFIG_WAKELOCK
int amvdec_wake_lock(void)
{
    wake_lock(&amvdec_lock);
    return 0;
}

int amvdec_wake_unlock(void)
{
    wake_unlock(&amvdec_lock);
    return 0;
}
#else
#define amvdec_wake_lock()
#define amvdec_wake_unlock();
#endif

s32 amvdec_loadmc(const u32 *p)
{
    ulong timeout;
    s32 ret = 0;
#ifdef AMVDEC_USE_STATIC_MEMORY
	if(mc_addr==NULL)
#endif
	{
		mc_addr = kmalloc(MC_SIZE, GFP_KERNEL);
		printk("Alloc new mc addr to %p\n",mc_addr);
	}
    if (!mc_addr) {
        return -ENOMEM;
    }

    memcpy(mc_addr, p, MC_SIZE);

    mc_addr_map = dma_map_single(NULL, mc_addr, MC_SIZE, DMA_TO_DEVICE);

    WRITE_MPEG_REG(MPSR, 0);
    WRITE_MPEG_REG(CPSR, 0);

    /* Read CBUS register for timing */
    timeout = READ_MPEG_REG(MPSR);
    timeout = READ_MPEG_REG(MPSR);

    timeout = jiffies + HZ;

    WRITE_MPEG_REG(IMEM_DMA_ADR, mc_addr_map);
    WRITE_MPEG_REG(IMEM_DMA_COUNT, 0x1000);
    WRITE_MPEG_REG(IMEM_DMA_CTRL, (0x8000 | (7 << 16)));

    while (READ_MPEG_REG(IMEM_DMA_CTRL) & 0x8000) {
        if (time_before(jiffies, timeout)) {
            schedule();
        } else {
            printk("vdec load mc error\n");
            ret = -EBUSY;
            break;
        }
    }

    dma_unmap_single(NULL, mc_addr_map, MC_SIZE, DMA_TO_DEVICE);
#ifndef AMVDEC_USE_STATIC_MEMORY
	kfree(mc_addr);
	mc_addr=NULL;
#endif	

    return ret;
}

void amvdec_start(void)
{
#ifdef CONFIG_WAKELOCK
    amvdec_wake_lock();
#endif

    /* additional cbus dummy register reading for timing control */
    READ_MPEG_REG(RESET0_REGISTER);
    READ_MPEG_REG(RESET0_REGISTER);
    READ_MPEG_REG(RESET0_REGISTER);
    READ_MPEG_REG(RESET0_REGISTER);

    WRITE_MPEG_REG(RESET0_REGISTER, RESET_VCPU | RESET_CCPU);

    READ_MPEG_REG(RESET0_REGISTER);
    READ_MPEG_REG(RESET0_REGISTER);
    READ_MPEG_REG(RESET0_REGISTER);

    WRITE_MPEG_REG(MPSR, 0x0001);
}

void amvdec_stop(void)
{
    ulong timeout = jiffies + HZ;

    WRITE_MPEG_REG(MPSR, 0);
    WRITE_MPEG_REG(CPSR, 0);

    while (READ_MPEG_REG(IMEM_DMA_CTRL) & 0x8000) {
        if (time_after(jiffies, timeout)) {
            break;
        }
    }

    WRITE_MPEG_REG(RESET0_REGISTER, RESET_VCPU | RESET_CCPU);

    /* additional cbus dummy register reading for timing control */
    READ_MPEG_REG(RESET0_REGISTER);
    READ_MPEG_REG(RESET0_REGISTER);
    READ_MPEG_REG(RESET0_REGISTER);
    READ_MPEG_REG(RESET0_REGISTER);

#ifdef CONFIG_WAKELOCK
    amvdec_wake_unlock();
#endif
}

void amvdec_enable(void)
{
    amvdec_pg_enable(true);
}

void amvdec_disable(void)
{
    amvdec_pg_enable(false);
}

#ifdef CONFIG_PM
int amvdec_suspend(struct platform_device *dev, pm_message_t event)
{
    amvdec_pg_enable(false);

    return 0;
}

int amvdec_resume(struct platform_device *dev)
{
    amvdec_pg_enable(true);
    return 0;
}
#endif

#ifdef CONFIG_WAKELOCK

static int vdec_is_paused(void)
{
    static unsigned long old_wp = -1, old_rp = -1, old_level = -1;
    unsigned long wp, rp, level;
    static int  paused_time = 0;

    wp = READ_MPEG_REG(VLD_MEM_VIFIFO_WP);
    rp = READ_MPEG_REG(VLD_MEM_VIFIFO_RP);
    level = READ_MPEG_REG(VLD_MEM_VIFIFO_LEVEL);
    if ((rp == old_rp && level > 1024) || /*have data,but output buffer is fulle*/
        (rp == old_rp && wp == old_wp && level == level)) { /*no write && not read*/
        paused_time++;
    } else {
        paused_time = 0;
    }
    old_wp = wp;
    old_rp = rp;
    old_level = level;

    if (paused_time > 10) {
        return 1;
    }
    return 0;
}

int amvdev_pause(void)
{
    video_running=0;
    video_stated_changed=1;
    return 0;
}
int amvdev_resume(void)
{
    video_running=1;
    video_stated_changed=1;
    return 0;
}

static void vdec_paused_check_timer(unsigned long arg)
{
    if(video_stated_changed){
	if(!video_running){
    		if (vdec_is_paused()) {
        		printk("vdec paused and release wakelock now\n");
        		amvdec_wake_unlock();
			video_stated_changed=0;
	    	}
         }else{
	    	amvdec_wake_lock();
            	video_stated_changed=0; 
	}
    }
    mod_timer(&amvdevtimer, jiffies + WAKE_CHECK_INTERVAL);
}
#else
int amvdev_pause(void)
{
    return 0;
}
int amvdev_resume(void)
{
    return 0;
}
#endif


int __init amvdec_init(void)
{
#ifdef CONFIG_WAKELOCK
    wake_lock_init(&amvdec_lock, WAKE_LOCK_IDLE, "amvdec_lock");
    init_timer(&amvdevtimer);
    amvdevtimer.data = (ulong) & amvdevtimer;
    amvdevtimer.function = vdec_paused_check_timer;
#endif
    CLK_GATE_OFF(MDEC_CLK_PIC_DC);
    CLK_GATE_OFF(MDEC_CLK_DBLK);
    CLK_GATE_OFF(MC_CLK);
    CLK_GATE_OFF(IQIDCT_CLK);
    //CLK_GATE_OFF(VLD_CLK);
    CLK_GATE_OFF(AMRISC);    
    return 0;
}
static void __exit amvdec_exit(void)
{
#ifdef CONFIG_WAKELOCK
    del_timer_sync(&amvdevtimer);
#endif
    return ;
}

module_init(amvdec_init);
module_exit(amvdec_exit);


EXPORT_SYMBOL(amvdec_loadmc);
EXPORT_SYMBOL(amvdec_start);
EXPORT_SYMBOL(amvdec_stop);
EXPORT_SYMBOL(amvdec_enable);
EXPORT_SYMBOL(amvdec_disable);
#ifdef CONFIG_PM
EXPORT_SYMBOL(amvdec_suspend);
EXPORT_SYMBOL(amvdec_resume);
#endif

MODULE_DESCRIPTION("Amlogic Video Decoder Utility Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
