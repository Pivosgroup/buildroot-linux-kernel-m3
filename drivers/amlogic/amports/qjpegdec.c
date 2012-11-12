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

#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/amstream.h>
#include <linux/amports/canvas.h>
#include <linux/amports/jpegdec.h>
#include <mach/am_regs.h>

#include "amvdec.h"
#include "qjpegdec_mc.h"

#ifdef CONFIG_AM_VDEC_MJPEG_LOG
#define AMLOG
#define LOG_LEVEL_VAR       amlog_level_vmjpeg
#define LOG_MASK_VAR        amlog_mask_vmjpeg
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_INFO      1
#define LOG_LEVEL_DESC  "0:ERROR, 1:INFO"
#endif
#include <linux/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_LEVEL_DESC, LOG_DEFAULT_MASK_DESC);

#ifdef DEBUG
#define pr_dbg(fmt, args...) printk(KERN_DEBUG "amjpegdec: " fmt, ## args)
#else
#define pr_dbg(fmt, args...)
#endif
#define pr_error(fmt, args...) printk(KERN_ERR "amjpegdec: " fmt, ## args)

#define DRIVER_NAME "amqjpegdec"
#define MODULE_NAME "amqjpegdec"
#define DEVICE_NAME "amqjpegdec"

/* protocol register usage
    AV_SCRATCH_0 - AV_SCRATCH_1 : 1)initial display buffer fifo  
    AV_SCRATCH_2 - AV_SCRATCH_3 : decoder settings
    AV_SCRATCH_4 - AV_SCRATCH_7 : display buffer spec
    AV_SCRATCH_8 - AV_SCRATCH_9 : amrisc/host display buffer management
    AV_SCRATCH_a                : time stamp

int isr:
* AV_SCRATCH_0: colornum
* AV_SCRATCH_1: width
* AV_SCRATCH_A: height
* AV_SCRATCH_9: continue to decoding.
*/


#define MREG_DECODE_PARAM   AV_SCRATCH_2 /* bit 0-3: pico_addr_mode */
                                         /* bit 15-4: reference height */
#define MREG_TO_AMRISC      AV_SCRATCH_8
#define MREG_FROM_AMRISC    AV_SCRATCH_9
#define MREG_FRAME_OFFSET   AV_SCRATCH_A

#define PICINFO_BUF_IDX_MASK        0x0007

#define VF_POOL_SIZE        12
#define PUT_INTERVAL        HZ/100

#define STAT_TIMER_INIT     0x01
#define STAT_MC_LOAD        0x02
#define STAT_ISR_REG        0x04
#define STAT_VF_HOOK        0x08
#define STAT_TIMER_ARM      0x10
#define STAT_VDEC_RUN       0x20

typedef struct {
    jpegdec_config_t conf;
    jpegdec_info_t   info;
    unsigned         state;
} jpegdec_t;

static const char vmjpeg_dec_id[] = "qjpeg-dev";
static struct class *amjpegdec_class;
static struct device *amjpegdec_dev;

static DEFINE_MUTEX(jpegdec_module_mutex);
static jpegdec_t *dec = NULL;
static u32 pbufAddr;
static u32 pbufSize;
static jpegdec_mem_info_t jegdec_mem_info;

static u32 stat;

static irqreturn_t vmjpeg_isr(int irq, void *dev_id)
{
	u32 reg = 0;

	WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);

	reg = READ_MPEG_REG(MREG_FROM_AMRISC);
	//printk("===reg:%x===\n",reg);
	if(reg == 0xf8) {
		dec->info.width = READ_MPEG_REG(AV_SCRATCH_1);
		dec->info.height = READ_MPEG_REG(AV_SCRATCH_A);
		dec->info.comp_num = READ_MPEG_REG(AV_SCRATCH_0);

		pr_dbg("ucode report picture size %dx%d, comp_num %d\n",
		       dec->info.width, dec->info.height, dec->info.comp_num);

		if ((dec->info.comp_num != 1) && (dec->info.comp_num != 3)) {
		    dec->state |= JPEGDEC_STAT_INFO_READY | JPEGDEC_STAT_UNSUPPORT;
		} else {
		    dec->state |= JPEGDEC_STAT_INFO_READY | JPEGDEC_STAT_WAIT_DECCONFIG;
		}
		WRITE_MPEG_REG(MREG_FROM_AMRISC, 1);
	} else if (reg & PICINFO_BUF_IDX_MASK) {
		u32 index = ((reg & PICINFO_BUF_IDX_MASK) - 1) & 3;
		WRITE_MPEG_REG(MREG_FROM_AMRISC, 0);
		dec->state &= ~JPEGDEC_STAT_WAIT_DATA;
		dec->state |= JPEGDEC_STAT_DONE;
	}

	return IRQ_HANDLED;
}

/****************************************/
static void vmjpeg_canvas_init(void)
{
	u32 canvas_width, canvas_height;
	u32 decbuf_size, decbuf_y_size, decbuf_uv_size;

	canvas_width   = 1920;
	canvas_height  = 1088;
	decbuf_y_size  = 0x200000;
	decbuf_uv_size = 0x80000;
	decbuf_size    = 0x300000;

	canvas_config(0,
		pbufAddr,
		canvas_width, canvas_height,
		CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	canvas_config(1,
		pbufAddr + decbuf_y_size,
		canvas_width / 2, canvas_height / 2,
		CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	canvas_config(2,
		pbufAddr + decbuf_y_size + decbuf_uv_size,
		canvas_width / 2, canvas_height / 2,
		CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
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
	WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 37 * 2);
	/* [35]: buf repeat pix0,
	* [34:29] => buf receive num,
	* [28:16] => buf blk x,
	* [15:0] => buf phase
	*/
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x60000000);

	/* C horizontal initial info */
	WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 41 * 2);
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x60000000);

	/* Y vertical initial info */
	WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 39 * 2);
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x60000000);

	/* C vertical initial info */
	WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 43 * 2);
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x0008);
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x60000000);

	/* Y horizontal phase step */
	WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 36 * 2 + 1);
	/* [19:0] => Y horizontal phase step */
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x10000);
	/* C horizontal phase step */
	WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 40 * 2 + 1);
	/* [19:0] => C horizontal phase step */
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x10000);

	/* Y vertical phase step */
	WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 38 * 2 + 1);
	/* [19:0] => Y vertical phase step */
	WRITE_MPEG_REG(PSCALE_BMEM_DAT, 0x10000);
	/* C vertical phase step */
	WRITE_MPEG_REG(PSCALE_BMEM_ADDR, 42 * 2 + 1);
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

static void vmjpeg_prot_init(void)
{
	WRITE_MPEG_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC);

	vmjpeg_canvas_init();

	WRITE_MPEG_REG(AV_SCRATCH_0, 12);
	WRITE_MPEG_REG(AV_SCRATCH_1, 0x031a);
	WRITE_MPEG_REG(AV_SCRATCH_4, 0x020100);
	WRITE_MPEG_REG(AV_SCRATCH_5, 0x020100);
	WRITE_MPEG_REG(AV_SCRATCH_6, 0x020100);
	WRITE_MPEG_REG(AV_SCRATCH_7, 0x020100);

	init_scaler();

	/* clear buffer IN/OUT registers */
	WRITE_MPEG_REG(MREG_TO_AMRISC, 0);
	WRITE_MPEG_REG(MREG_FROM_AMRISC, 0);

	WRITE_MPEG_REG(MCPU_INTR_MSK, 0xffff);
	WRITE_MPEG_REG(MREG_DECODE_PARAM, (1088 << 4) | 0x8000);

	/* clear mailbox interrupt */
	WRITE_MPEG_REG(ASSIST_MBOX1_CLR_REG, 1);
	/* enable mailbox interrupt */
	WRITE_MPEG_REG(ASSIST_MBOX1_MASK, 1);
	/* set interrupt mapping for vld */
	WRITE_MPEG_REG(ASSIST_AMR1_INT8, 8);
}

static s32 vmjpeg_init(void)
{
	int r;

	stat |= STAT_TIMER_INIT;

	amvdec_enable();

	if (amvdec_loadmc(qjpegdec_mc) < 0) {
		amvdec_disable();

		return -EBUSY;
	}
	stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vmjpeg_prot_init();

	r = request_irq(INT_MAILBOX_1A, vmjpeg_isr,
		    IRQF_SHARED, "vmjpeg-irq", (void *)vmjpeg_dec_id);
	if (r) {
		amvdec_disable();

		amlog_level(LOG_LEVEL_ERROR, "vmjpeg irq register error.\n");
		return -ENOENT;
	}
	stat |= STAT_ISR_REG;
	dec->state = JPEGDEC_STAT_WAIT_DATA | JPEGDEC_STAT_WAIT_INFOCONFIG;
	amvdec_start();
	stat |= STAT_VDEC_RUN;
	

	return 0;
}

static int amjpegdec_open(struct inode *inode, struct file *file)
{
	int r = 0;
	mutex_lock(&jpegdec_module_mutex);

	if (dec != NULL) {
		r = -EBUSY;
	} else if ((dec = (jpegdec_t *)kcalloc(1, sizeof(jpegdec_t), GFP_KERNEL)) == NULL) {
		r = -ENOMEM;
	}

	mutex_unlock(&jpegdec_module_mutex);

	if (r == 0) {
		if ((r = vmjpeg_init()) < 0) {
			kfree(dec);
			dec = NULL;
		} else {
			file->private_data = dec;
		}
	}
    return r;
}

static int amjpegdec_release(struct inode *inode, struct file *file)
{
	if (dec) {
		kfree(dec);
		dec = NULL;
	}

	if (stat & STAT_VDEC_RUN) {
		amvdec_stop();
		stat &= ~STAT_VDEC_RUN;
	}

	if (stat & STAT_ISR_REG) {
		free_irq(INT_MAILBOX_1A, (void *)vmjpeg_dec_id);
		stat &= ~STAT_ISR_REG;
	}

	amvdec_disable();

	return 0;
}

static int amjpegdec_ioctl(struct inode *inode, struct file *file,
                           unsigned int cmd, ulong arg)
{
	int r = 0;

	switch (cmd) {
	case JPEGDEC_IOC_INFOCONFIG:
		if (dec->state & JPEGDEC_STAT_WAIT_INFOCONFIG) {
			pr_dbg("amjpegdec_ioctl:  JPEGDEC_IOC_INFOCONFIG\n");

			dec->conf.opt |= arg & (JPEGDEC_OPT_THUMBNAIL_ONLY | JPEGDEC_OPT_THUMBNAIL_PREFERED);

			//WRITE_MPEG_REG(JPEG_DECODE_START,(dec->conf.opt & JPEGDEC_OPT_THUMBNAIL_PREFERED) ? 1 : 0);

			dec->state &= ~JPEGDEC_STAT_WAIT_INFOCONFIG;
			dec->state |= JPEGDEC_STAT_WAIT_DATA | JPEGDEC_STAT_WAIT_DECCONFIG;

			//amvdec_start();
		} else {
			r = -EPERM;
		}
		break;
	case JPEGDEC_IOC_DECCONFIG:
		if (dec->state & JPEGDEC_STAT_WAIT_DECCONFIG) {
			copy_from_user(&dec->conf, (void *)arg, sizeof(jpegdec_config_t));

			pr_dbg("amjpegdec_ioctl:  JPEGDEC_IOC_DECCONFIG, target (%d-%d-%d-%d)\n",
			   dec->conf.dec_x, dec->conf.dec_y, dec->conf.dec_w, dec->conf.dec_h);
			pr_dbg("planes (0x%lx-0x%lx-0x%lx), pbufAddr=0x%lx\n",
			   dec->conf.addr_y, dec->conf.addr_u, dec->conf.addr_v, pbufAddr);

			dec->state &= ~JPEGDEC_STAT_WAIT_DECCONFIG;

			//_dec_run();

		} else {
			r = -EPERM;
		}
		break;
	case JPEGDEC_IOC_INFO:
		if (dec->state & JPEGDEC_STAT_INFO_READY) {
			pr_dbg("amjpegdec_ioctl:  JPEGDEC_IOC_INFO, %dx%d\n",
				dec->info.width, dec->info.height);
			copy_to_user((void *)arg, &dec->info, sizeof(jpegdec_info_t));
		} else {
			r = -EAGAIN;
		}
		break;
	case JPEGDEC_IOC_STAT:
		return dec->state;
	case JPEGDEC_G_MEM_INFO:
		if (copy_from_user(&jegdec_mem_info, (void __user *)arg, sizeof(jpegdec_mem_info_t))) {
			r = -EFAULT;
		} else {
			jegdec_mem_info.canv_addr = pbufAddr;
			jegdec_mem_info.canv_len  = pbufSize;

			if (copy_to_user((void __user *)arg, &jegdec_mem_info, sizeof(jpegdec_mem_info_t))) {
			r = -EFAULT;
			}
		}
		break;
	default:
		return -ENOIOCTLCMD;
	}

	return r;
}

static int mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned vm_size = vma->vm_end - vma->vm_start;

	if (vm_size == 0)
		return -EAGAIN;
	//printk("mmap:%x\n",vm_size);
	off += jegdec_mem_info.canv_addr;

	vma->vm_flags |= VM_RESERVED | VM_IO;

	if (remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		printk("set_cached: failed remap_pfn_range\n");
		return -EAGAIN;
	}
	return 0;

}

const static struct file_operations amjpegdec_fops = {
	    .owner    = THIS_MODULE,
	    .open     = amjpegdec_open,
	    .mmap     = mmap,
	    .release  = amjpegdec_release,
	    .ioctl    = amjpegdec_ioctl,
};
static int HWJPEGDEC_MAJOR = 0; 
static int amvdec_mjpeg_probe(struct platform_device *pdev)
{
	struct resource *mem;

	amlog_level(LOG_LEVEL_INFO, "amvdec_mjpeg probe start.\n");

	HWJPEGDEC_MAJOR = register_chrdev(0, "amqjpegdev", &amjpegdec_fops);

	if (HWJPEGDEC_MAJOR < 0) {
		pr_error("Can't register major for amjpegdec device\n");
		return HWJPEGDEC_MAJOR;
	}

	amjpegdec_class = class_create(THIS_MODULE, DEVICE_NAME);

	amjpegdec_dev = device_create(amjpegdec_class, NULL,
				  MKDEV(HWJPEGDEC_MAJOR, 0), NULL,
				  DEVICE_NAME);

	if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0))) {
		amlog_level(LOG_LEVEL_ERROR, "amvdec_mjpeg memory resource undefined.\n");
		return -EFAULT;
	}

	pbufAddr = mem->start;
	pbufSize  = mem->end - mem->start + 1;

	amlog_level(LOG_LEVEL_INFO, "amvdec_mjpeg probe end.\n");

	return 0;
}

static int amvdec_mjpeg_remove(struct platform_device *pdev)
{
	device_destroy(amjpegdec_class, MKDEV(HWJPEGDEC_MAJOR, 0));

	class_destroy(amjpegdec_class);

	unregister_chrdev(HWJPEGDEC_MAJOR, DEVICE_NAME);

	amlog_level(LOG_LEVEL_INFO, "amvdec_qjpeg remove.\n");

	return 0;
}

/****************************************/

static struct platform_driver amvdec_mjpeg_driver = {
	.probe      = amvdec_mjpeg_probe,
	.remove     = amvdec_mjpeg_remove,
#ifdef CONFIG_PM
	    .suspend    = amvdec_suspend,
	    .resume     = amvdec_resume,
#endif
	.driver     = {
		.name   = DRIVER_NAME,
	}
};
static struct codec_profile_t amvdec_mjpeg_profile = {
	.name = "qjpeg",
	.profile = ""
};
static int __init amvdec_qjpeg_driver_init_module(void)
{
	amlog_level(LOG_LEVEL_INFO, "amvdec_mjpeg module init\n");

	if (platform_driver_register(&amvdec_mjpeg_driver)) {
		amlog_level(LOG_LEVEL_ERROR, "failed to register amvdec_mjpeg driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvdec_mjpeg_profile);
	return 0;
}

static void __exit amvdec_qjpeg_driver_remove_module(void)
{
	amlog_level(LOG_LEVEL_INFO, "amvdec_mjpeg module remove.\n");

	platform_driver_unregister(&amvdec_mjpeg_driver);
}

module_init(amvdec_qjpeg_driver_init_module);
module_exit(amvdec_qjpeg_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC JMPEG Decoder Without scaling Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kasin  Lee <kasin.li@amlogic.com>");
