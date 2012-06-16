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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/amports/canvas.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>

#include <mach/am_regs.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>

#include "../amports/amvdec.h"
#include "../amports/vmjpeg_mc.h"

//#define DEBUG

#define PADDINGSIZE		1024
#define JPEG_TAG		0xff
#define JPEG_TAG_SOI	0xd8
#define JPEG_TAG_SOF0	0xc0

/* protocol register usage
    AV_SCRATCH_0 - AV_SCRATCH_1 : initial display buffer fifo
    AV_SCRATCH_2 - AV_SCRATCH_3 : decoder settings
    AV_SCRATCH_4 - AV_SCRATCH_7 : display buffer spec
    AV_SCRATCH_8 - AV_SCRATCH_9 : amrisc/host display buffer management
    AV_SCRATCH_a                : time stamp
*/

#define MREG_DECODE_PARAM   AV_SCRATCH_2 /* bit 0-3: pico_addr_mode */
                                         /* bit 15-4: reference height */
#define MREG_TO_AMRISC      AV_SCRATCH_8
#define MREG_FROM_AMRISC    AV_SCRATCH_9

#define PICINFO_BUF_IDX_MASK        0x0007
#define PICINFO_INTERLACE           0x0020
#define PICINFO_INTERLACE_TOP       0x0010

enum {
	PIC_NA = 0,
	PIC_DECODED = 1,
	PIC_FETCHED = 2
};

static u32 logo_paddr;
static u32 logo_size;
static u32 disp;

static volatile u32 logo_state = PIC_NA;
static vframe_t vf;
static const char jpeglogo_id[] = "jpeglogo-dev";

static vframe_t *jpeglogo_vf_peek(void)
{
	return (logo_state == PIC_DECODED) ? &vf : NULL;
}

static vframe_t *jpeglogo_vf_get(void)
{
	if (logo_state == PIC_DECODED) {
		logo_state = PIC_FETCHED;
		return &vf;
	}
	
	return NULL;
}

static const struct vframe_provider_s jpeglogo_vf_provider =
{
    .peek = jpeglogo_vf_peek,
    .get  = jpeglogo_vf_get,
    .put  = NULL,
};

static inline u32 index2canvas(u32 index)
{
    const u32 canvas_tab[4] = {
        0x020100, 0x050403, 0x080706, 0x0b0a09
    };

    return canvas_tab[index];
}

static irqreturn_t jpeglogo_isr(int irq, void *dev_id)
{
    u32 reg, index;

    WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

    reg = READ_MPEG_REG(MREG_FROM_AMRISC);

    if (reg & PICINFO_BUF_IDX_MASK) {
        index = ((reg & PICINFO_BUF_IDX_MASK) - 1) & 3;

        vf.type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
        vf.canvas0Addr = vf.canvas1Addr = index2canvas(index);

        logo_state = PIC_DECODED;
#ifdef DEBUG
		printk("[jpeglogo]: One frame decoded\n");
#endif
    }

    return IRQ_HANDLED;
}

static void jpeglogo_canvas_init(void)
{
    int i;
    u32 canvas_width, canvas_height;
    u32 decbuf_size, decbuf_y_size, decbuf_uv_size;

    if ((vf.width < 768) && (vf.height < 576)) {
        /* SD only */
        canvas_width   = 768;
        canvas_height  = 576;
        decbuf_y_size  = 0x80000;
        decbuf_uv_size = 0x20000;
        decbuf_size    = 0x100000;
    }
    else {
        /* HD & SD */
        canvas_width   = 1920;
        canvas_height  = 1088;
        decbuf_y_size  = 0x200000;
        decbuf_uv_size = 0x80000;
        decbuf_size    = 0x300000;
    }

    for (i = 0; i < 4; i++) {
        canvas_config(3 * i + 0,
                      disp + i * decbuf_size,
                      canvas_width, canvas_height,
                      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
        canvas_config(3 * i + 1,
                      disp + i * decbuf_size + decbuf_y_size,
                      canvas_width / 2, canvas_height / 2,
                      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
        canvas_config(3 * i + 2,
                      disp + i * decbuf_size + decbuf_y_size + decbuf_uv_size,
                      canvas_width/2, canvas_height/2,
                      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
    }
}

static void init_scaler(void)
{
    /* 4 point triangle */
    const unsigned filt_coef[] = {
        0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
        0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
        0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
        0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
        0x18382808, 0x18382808, 0x17372909, 0x17372909,
        0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
        0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
        0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
        0x10303010
    };
    int i;

    /* pscale enable, PSCALE cbus bmem enable */
    WRITE_MPEG_REG(PSCALE_CTRL, 0xc000);

    /* write filter coefs */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 0);
    for (i = 0; i < 33; i++) {
        WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0);
        WRITE_MPEG_REG(PSCALE_BMEM_DAT, filt_coef[i]);
    }

    /* Y horizontal initial info */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 37*2);
    /* [35]: buf repeat pix0,
     * [34:29] => buf receive num,
     * [28:16] => buf blk x,
     * [15:0] => buf phase
     */
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x0008);
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x60000000);

    /* C horizontal initial info */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 41*2);
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x0008);
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x60000000);

    /* Y vertical initial info */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 39*2);
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x0008);
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x60000000);

    /* C vertical initial info */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 43*2);
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x0008);
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x60000000);

    /* Y horizontal phase step */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 36*2 + 1);
    /* [19:0] => Y horizontal phase step */
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x10000);
    /* C horizontal phase step */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 40*2 + 1);
    /* [19:0] => C horizontal phase step */
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x10000);

    /* Y vertical phase step */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 38*2+1);
    /* [19:0] => Y vertical phase step */
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x10000);
    /* C vertical phase step */
    WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 42*2+1);
    /* [19:0] => C horizontal phase step */
    WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x10000);

    /* reset pscaler */
    WRITE_MPEG_REG(RESET2_REGISTER, RESET_PSCALE);
	READ_MPEG_REG(RESET2_REGISTER);
	READ_MPEG_REG(RESET2_REGISTER);
	READ_MPEG_REG(RESET2_REGISTER);

    WRITE_MPEG_REG(PSCALE_RST, 0x7);
    WRITE_MPEG_REG(PSCALE_RST, 0x0);
}

static void jpeglogo_prot_init(void)
{
    WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC);

    jpeglogo_canvas_init();

    WRITE_MPEG_REG(AV_SCRATCH_0, 12);
    WRITE_MPEG_REG(AV_SCRATCH_1, 0x031a);
    WRITE_MPEG_REG(AV_SCRATCH_4, 0x020100);
    WRITE_MPEG_REG(AV_SCRATCH_5, 0x050403);
    WRITE_MPEG_REG(AV_SCRATCH_6, 0x080706);
    WRITE_MPEG_REG(AV_SCRATCH_7, 0x0b0a09);

    init_scaler();

    /* clear buffer IN/OUT registers */
    WRITE_MPEG_REG(MREG_TO_AMRISC, 0);
    WRITE_MPEG_REG(MREG_FROM_AMRISC, 0);

    WRITE_MPEG_REG(MCPU_INTR_MSK, 0xffff);
    WRITE_MPEG_REG(MREG_DECODE_PARAM, 0);

    /* clear mailbox interrupt */
    WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);
    /* enable mailbox interrupt */
    WRITE_MPEG_REG(ASSIST_MBOX1_MASK, 1);
}

static void setup_vb(u32 addr, int s)
{
    WRITE_MPEG_REG(VLD_MEM_VIFIFO_START_PTR, addr);
    WRITE_MPEG_REG(VLD_MEM_VIFIFO_CURR_PTR, addr);
    WRITE_MPEG_REG(VLD_MEM_VIFIFO_END_PTR, (addr + s + PADDINGSIZE + 7) & ~7);

    SET_MPEG_REG_MASK(VLD_MEM_VIFIFO_CONTROL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(VLD_MEM_VIFIFO_CONTROL, MEM_BUFCTRL_INIT);

    WRITE_MPEG_REG(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_MANUAL);
    WRITE_MPEG_REG(VLD_MEM_VIFIFO_WP, addr);

    SET_MPEG_REG_MASK(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
    CLEAR_MPEG_REG_MASK(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

    SET_MPEG_REG_MASK(VLD_MEM_VIFIFO_CONTROL, MEM_FILL_ON_LEVEL | MEM_CTRL_FILL_EN | MEM_CTRL_EMPTY_EN);
}

static inline void feed_vb(s32 s)
{
	u32 addr = READ_MPEG_REG(VLD_MEM_VIFIFO_START_PTR);

    WRITE_MPEG_REG(VLD_MEM_VIFIFO_WP, (addr + s + PADDINGSIZE + 7) & ~7);
}

static s32 sanity_check(u8 *dp, int size)
{
	int len = 0;
	u8 *p = dp + 2;
	u8 tag;

	if ((dp[0] != JPEG_TAG) ||
		(dp[1] != JPEG_TAG_SOI) ||
		(size < 8))
		return -EINVAL;

	if (*p++ != JPEG_TAG)
		return -EINVAL;
		
	tag = *p++;
	len = ((u32)(p[0]) << 8) | p[1];

#ifdef DEBUG
	printk("[jpeglogo] tag: (0x%x, %d)\n", tag, len);
#endif

	while ((p + len) < (dp + size)) {
		if (tag == JPEG_TAG_SOF0) {
			vf.height = ((u32)p[3] << 8) | p[4];
			vf.width = ((u32)p[5] << 8) | p[6];
#ifdef DEBUG
			printk("[jpeglogo]: Valid JPEG header found : %dx%d\n", vf.width, vf.height);
#endif
			return 0;
		}
		
		p += len;
		if (*p++ != JPEG_TAG)
			break;

		tag = *p++;
		len = ((u32)p[0] << 8) | p[1];

#ifdef DEBUG
		printk("[jpeglogo] tag: (0x%x, %d)\n", tag, len);
#endif
	}
	
	return -EINVAL;
}

/* addr must be aligned at 8 bytes boundary */
static void swap_tailzero_data(u8 *addr, u32 s)
{
	register u8 d;
	u8 *p = addr;
	u32 len = (s + 7) >> 3;
	
	memset(addr + s, 0, PADDINGSIZE);

	while (len > 0) {
		d = p[0]; p[0] = p[7]; p[7] = d;
		d = p[1]; p[1] = p[6]; p[6] = d;
		d = p[2]; p[2] = p[5]; p[5] = d;
		d = p[3]; p[3] = p[4]; p[4] = d;
		p += 8;
		len --;
	}
}

/*********************************************************/

static int jpeglogo_probe(struct platform_device *pdev)
{
	int r;
	ulong timeout;
	u32 *mc_addr_aligned;
	struct resource *s;
	void __iomem *logo_vaddr;

	printk("[jpeglogo]: loading ...\n");
	mc_addr_aligned = (u32 *)vmjpeg_mc;

	s = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!s) {
		printk("[jpeglogo]: No logo data resource specified.\n");
		return -EINVAL;
	}

	logo_paddr = s->start;
	logo_size = s->end - s->start + 1;
	
	logo_vaddr = ioremap_wc(logo_paddr, logo_size + PADDINGSIZE);
	if (!logo_vaddr) {
		printk("[jpeglogo]: Remapping logo data failed.\n");
		return -EINVAL;
	}
		
	s = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!s) {
		printk("[jpeglogo]: No display buffer resource specified.\n");
		return -EINVAL;
	}
	disp = s->start;

	if ((!logo_paddr) || (logo_size == 0)) {
		printk("[jpeglogo]: Invalid data specified.\n");
		return -EINVAL;
	}

	if ((logo_paddr & 7) != 0) {
		printk("[jpeglogo]: Logo data must be aligned at 8 bytes boundary.\n");
		return -EINVAL;
	}

	if (!disp) {
		printk("[jpeglogo]: No display buffer specified.\n");
		return -EINVAL;
	}

	printk("[jpeglogo]: logo data at 0x%x, size %d, display buffer 0x%x\n",
		logo_paddr, logo_size, disp);

	memset(&vf, 0, sizeof(vframe_t));

	/* a little bit sanity check */
	if (sanity_check((u8 *)logo_vaddr, logo_size) < 0) {
		printk("[jpeglogo]: Invalid jpeg data.\n");
		return -EINVAL;
	}

	swap_tailzero_data((u8 *)logo_vaddr, logo_size);

    WRITE_MPEG_REG(RESET0_REGISTER, RESET_VCPU | RESET_CCPU);

    if (amvdec_loadmc(mc_addr_aligned) < 0) {
		printk("[jpeglogo]: Can not loading HW decoding ucode.\n");
        return -EBUSY;
    }
    
    jpeglogo_prot_init();

    r = request_irq(INT_MAILBOX_1A, jpeglogo_isr,
                    IRQF_SHARED, "jpeglogo-irq", (void *)jpeglogo_id);

    if (r) {
        printk("jpeglogo_init irq register error.\n");
        return -ENOENT;
    }

	setup_vb(logo_paddr, logo_size);

	WRITE_MPEG_REG(M4_CONTROL_REG, 0x0300);
	WRITE_MPEG_REG(POWER_CTL_VLD, 0);

    amvdec_start();
    
    feed_vb(logo_size);
    
    timeout = jiffies + HZ * 2;
    
    while (time_before(jiffies, timeout)) {
		if (logo_state == PIC_DECODED) {
			/* disable OSD layer to expose logo on video layer ?? */
			CLEAR_MPEG_REG_MASK(VPP_MISC, VPP_OSD1_POSTBLEND | VPP_OSD2_POSTBLEND);
#ifdef DEBUG
			printk("[jpeglogo]: logo decoded.\n");
#endif
			break;
		}
    }

    amvdec_stop();

    free_irq(INT_MAILBOX_1A, (void *)jpeglogo_id);

	if (logo_state > PIC_NA) {
	    vf_reg_provider(&jpeglogo_vf_provider);
    	timeout = jiffies + HZ/8;
    	while (time_before(jiffies, timeout)) {
			if (logo_state == PIC_FETCHED)
				break;
		}

		vf_unreg_provider(&jpeglogo_vf_provider);
	}
	
	iounmap(logo_vaddr);

	if (logo_state == PIC_FETCHED)
		printk("[jpeglogo]: Logo loaded.\n");
	else
		printk("[jpeglogo]: Logo loading failed.\n");

    return (0);
}

static int jpeglogo_remove(struct platform_device *pdev)
{
    printk("[jpeglogo]: device removed\n");

    return 0;
}

static struct platform_driver
jpeglogo_driver = {
    .probe      = jpeglogo_probe,
    .remove     = jpeglogo_remove,
    .driver     = {
        .name   = "jpeglogo-dev",
    }
};

static int __init jpeglogo_init(void)
{
    if (platform_driver_register(&jpeglogo_driver)) {
        printk("failed to register amstream module\n");
        return -ENODEV;
    }

    return 0;
}

subsys_initcall(jpeglogo_init);

MODULE_DESCRIPTION("AMLOGIC jpeg logo driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
