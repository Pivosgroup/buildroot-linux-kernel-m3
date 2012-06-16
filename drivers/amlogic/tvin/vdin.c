/*
 * VDIN driver
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *         Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
//#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/mm.h>

/* Amlogic headers */
#include <linux/amports/canvas.h>
#include <mach/am_regs.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <linux/tvin/tvin.h>

/* TVIN headers */
#include "tvin_global.h"
#include "tvin_format_table.h"
#include "tvin_notifier.h"
#include "vdin_regs.h"
#include "vdin.h"
#include "vdin_vf.h"
#include "vdin_ctl.h"


#define VDIN_NAME               "vdin"
#define VDIN_DRIVER_NAME        "vdin"
#define VDIN_MODULE_NAME        "vdin"
#define VDIN_DEVICE_NAME        "vdin"
#define VDIN_CLASS_NAME         "vdin"
#define PROVIDER_NAME   "vdin"
#if defined(CONFIG_ARCH_MESON)
#define VDIN_COUNT              1
#elif defined(CONFIG_ARCH_MESON2)
#define VDIN_COUNT              2
#elif defined(CONFIG_ARCH_MESON3)
#define VDIN_COUNT              1
#endif

#define VDIN_PUT_INTERVAL       1    //(HZ/100)   //10ms, #define HZ 100

#define INVALID_VDIN_INPUT      0xffffffff

static dev_t vdin_devno;
static struct class *vdin_clsp;

static int debug;

#if defined(CONFIG_ARCH_MESON)
unsigned int vdin_addr_offset[VDIN_COUNT] = {0x00};
#elif defined(CONFIG_ARCH_MESON2)
unsigned int vdin_addr_offset[VDIN_COUNT] = {0x00, 0x70};
#elif defined(CONFIG_ARCH_MESON3)
unsigned int vdin_addr_offset[VDIN_COUNT] = {0x00};
#endif

static vdin_dev_t *vdin_devp[VDIN_COUNT];

//#define VDIN_DBG_MSG_CNT

#ifdef VDIN_DBG_MSG_CNT
typedef struct  vdin_dbg_msg_s {
    u32 vdin_isr_hard_counter;
    u32 vdin_tasklet_counter;
    u32 vdin_get_new_frame_cnt;
    u32 vdin_dec_run_none_cnt;
    u32 vdin_tasklet_invalid_type_cnt;
    u32 vdin_tasklet_valid_type_cnt;
    u32 vdin_from_video_recycle_cnt;
    u32 vdin_irq_short_time_cnt;
    u32 vdin_timer_puch_nf_cnt;

}vdin_dbg_msg_t;

static vdin_dbg_msg_t vdin_dbg_msg = {
    .vdin_isr_hard_counter = 0,
    .vdin_tasklet_counter = 0,  //u32 ;
    .vdin_get_new_frame_cnt = 0,  //u32 ;
    .vdin_dec_run_none_cnt = 0,  //u32 ;
    .vdin_tasklet_invalid_type_cnt = 0,  //u32 ;
    .vdin_tasklet_valid_type_cnt = 0,  //u32 ;
    .vdin_from_video_recycle_cnt = 0,  //u32 ;
    .vdin_irq_short_time_cnt = 0,
    .vdin_timer_puch_nf_cnt = 0,
};

#endif

void tvin_dec_register(struct vdin_dev_s *devp, struct tvin_dec_ops_s *ops)
{
    ulong flags;

    if (devp->dec_ops)
        tvin_dec_unregister(devp);

    spin_lock_irqsave(&devp->dec_lock, flags);

    devp->dec_ops = ops;

    spin_unlock_irqrestore(&devp->dec_lock, flags);
}

void tvin_dec_unregister(struct vdin_dev_s *devp)
{
    ulong flags;
    spin_lock_irqsave(&devp->dec_lock, flags);

    devp->dec_ops = NULL;

    spin_unlock_irqrestore(&devp->dec_lock, flags);
}

void vdin_info_update(struct vdin_dev_s *devp, struct tvin_parm_s *para)
{
    //check decoder signal status
    devp->para.status = para->status;
    devp->para.fmt_info.fmt= para->fmt_info.fmt;
	devp->para.fmt_info.v_active= para->fmt_info.v_active;
	devp->para.fmt_info.h_active= para->fmt_info.h_active;
	devp->para.fmt_info.frame_rate=para->fmt_info.frame_rate;

    if((para->status != TVIN_SIG_STATUS_STABLE) || (para->fmt_info.fmt== TVIN_SIG_FMT_NULL))
        return;

    //write vdin registers
    vdin_set_all_regs(devp);

}

EXPORT_SYMBOL(vdin_info_update);

static void vdin_put_timer_func(unsigned long arg)
{
    struct timer_list *timer = (struct timer_list *)arg;

    while (!vfq_empty_recycle()) {
        vframe_t *vf = vfq_pop_recycle();
        vfq_push_newframe(vf);
#ifdef VDIN_DBG_MSG_CNT
        vdin_dbg_msg.vdin_timer_puch_nf_cnt++;
#endif
    }

    tvin_check_notifier_call(TVIN_EVENT_INFO_CHECK, NULL);

    timer->expires = jiffies + VDIN_PUT_INTERVAL;
    add_timer(timer);
}

static void vdin_start_dec(struct vdin_dev_s *devp)
{
    vdin_vf_init();
    vdin_reg_vf_provider();
     vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_START,NULL);
#ifdef VDIN_DBG_MSG_CNT
        vdin_dbg_msg.vdin_isr_hard_counter = 0;
        vdin_dbg_msg.vdin_tasklet_counter = 0;
        vdin_dbg_msg.vdin_get_new_frame_cnt = 0;
        vdin_dbg_msg.vdin_dec_run_none_cnt = 0;
        vdin_dbg_msg.vdin_tasklet_invalid_type_cnt = 0;
        vdin_dbg_msg.vdin_tasklet_valid_type_cnt = 0;
        vdin_dbg_msg.vdin_from_video_recycle_cnt = 0;
        vdin_dbg_msg.vdin_irq_short_time_cnt = 0;
        vdin_dbg_msg.vdin_timer_puch_nf_cnt = 0;
#endif


    vdin_set_default_regmap(devp->addr_offset);

    tvin_dec_notifier_call(TVIN_EVENT_DEC_START, devp);

    //write vdin registers
//    vdin_set_all_regs(devp);

	return;
}

static void vdin_stop_dec(struct vdin_dev_s *devp)
{
    vdin_unreg_vf_provider();
    //load default setting for vdin
    vdin_set_default_regmap(devp->addr_offset);
    tvin_dec_notifier_call(TVIN_EVENT_DEC_STOP, devp);
}


int start_tvin_service(int no ,tvin_parm_t *para)
{
    struct vdin_dev_s  *devp;
    if((!para)||(no > 1)){
        return -1;    
    }
    devp = vdin_devp[no];
    devp->para.port = para->port;
    devp->para.fmt_info.fmt= para->fmt_info.fmt;
	devp->para.fmt_info.h_active= para->fmt_info.h_active;
	devp->para.fmt_info.v_active= para->fmt_info.v_active;
	devp->para.fmt_info.hsync_phase= para->fmt_info.hsync_phase;
	devp->para.fmt_info.vsync_phase= para->fmt_info.vsync_phase;	
	devp->para.fmt_info.frame_rate= para->fmt_info.frame_rate;
    devp->flags |= VDIN_FLAG_DEC_STARTED;  
  
    vdin_start_dec(devp);  
    msleep(10);
    tasklet_enable(&devp->isr_tasklet);
    devp->pre_irq_time = jiffies,
    enable_irq(devp->irq);      
    
}

int stop_tvin_service(int no)
{
    struct vdin_dev_s *devp;
    if(no > 1){
        return -1;    
    }    
    devp = vdin_devp[no];
    devp->flags &= (~VDIN_FLAG_DEC_STARTED);  
            disable_irq_nosync(devp->irq);
            tasklet_disable_nosync(&devp->isr_tasklet);     
//    vdin_notify_receiver(VFRAME_EVENT_PROVIDER_UNREG,NULL ,NULL);
//vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_UNREG,NULL);
    vdin_stop_dec(devp);
    
}

static int canvas_start_index = VDIN_START_CANVAS ;
static int canvas_total_num = BT656IN_VF_POOL_SIZE;
void set_tvin_canvas_info(int start , int num)
{
    canvas_start_index = start;
    canvas_total_num = num;    
}

void get_tvin_canvas_info(int* start , int* num)
{
    *start = canvas_start_index ;
    *num = canvas_total_num;
}


/*as use the spin_lock,
 *1--there is no sleep,
 *2--it is better to shorter the time,
 *3--it is better to shorter the time,
*/
static irqreturn_t vdin_isr(int irq, void *dev_id)
{
    ulong flags, vdin_cur_irq_time;
    struct timeval now;
    struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;

    spin_lock_irqsave(&devp->isr_lock, flags);
    do_gettimeofday(&now);
    vdin_cur_irq_time = (now.tv_sec*1000) + (now.tv_usec/1000);

#ifdef VDIN_DBG_MSG_CNT
    vdin_dbg_msg.vdin_isr_hard_counter++;
    if((vdin_dbg_msg.vdin_isr_hard_counter % 3600) == 0)
    {
        pr_info("%s--vdin_dbg_msg :\n%d \n %d \n %d \n %d \n %d \n %d \n %d \n %d \n %d \n", __FUNCTION__,
                vdin_dbg_msg.vdin_isr_hard_counter,
                vdin_dbg_msg.vdin_tasklet_counter,
                vdin_dbg_msg.vdin_get_new_frame_cnt,
                vdin_dbg_msg.vdin_dec_run_none_cnt,
                vdin_dbg_msg.vdin_tasklet_invalid_type_cnt,
                vdin_dbg_msg.vdin_tasklet_valid_type_cnt,
                vdin_dbg_msg.vdin_from_video_recycle_cnt,
                vdin_dbg_msg.vdin_irq_short_time_cnt,
                vdin_dbg_msg.vdin_timer_puch_nf_cnt
            );
    }

#endif

    if(time_after(vdin_cur_irq_time, devp->pre_irq_time))
    {
        if((vdin_cur_irq_time - devp->pre_irq_time) < 7)   //short time
        {
            devp->pre_irq_time = vdin_cur_irq_time;
#ifdef VDIN_DBG_MSG_CNT
            vdin_dbg_msg.vdin_irq_short_time_cnt++;
#endif
    		spin_unlock_irqrestore(&devp->isr_lock, flags);
            return IRQ_HANDLED;
        }
    }
    devp->pre_irq_time = vdin_cur_irq_time;
    tasklet_hi_schedule(&devp->isr_tasklet);
    spin_unlock_irqrestore(&devp->isr_lock, flags);

    return IRQ_HANDLED;
}
#include <linux/videodev2.h>
extern int vm_fill_buffer(struct videobuf_buffer* vb , int format , int magic);
static void vdin_isr_tasklet(unsigned long arg)
{
    int ret = 0;
    vframe_t *vf = NULL;
    struct vdin_dev_s *devp = (struct vdin_dev_s*)arg;
#ifdef VDIN_DBG_MSG_CNT
        vdin_dbg_msg.vdin_tasklet_counter++;
#endif

    spin_lock(&devp->isr_lock);
    vf = vfq_pop_newframe();

    if(vf == NULL )
    {
        vfq_push_recycle(vf);
        spin_unlock(&devp->isr_lock);
        return;
    }
#ifdef VDIN_DBG_MSG_CNT
    vdin_dbg_msg.vdin_get_new_frame_cnt++;
#endif
    if (!devp->dec_ops || !devp->dec_ops->dec_run)
    {
        pr_err("vdin%d: no registered decode\n", devp->index);
        vfq_push_recycle(vf);

#ifdef VDIN_DBG_MSG_CNT
    vdin_dbg_msg.vdin_dec_run_none_cnt++;
#endif
        spin_unlock(&devp->isr_lock);
        return;
    }
    vf->type = INVALID_VDIN_INPUT;
    vf->pts = 0;
    ret = devp->dec_ops->dec_run(vf);

    if(vf->type == INVALID_VDIN_INPUT)
    {

        vfq_push_recycle(vf);

#ifdef VDIN_DBG_MSG_CNT
        vdin_dbg_msg.vdin_tasklet_invalid_type_cnt++;
#endif
    }
    else
    {
        vdin_set_vframe_prop_info(vf, devp->addr_offset);

        vfq_push_display(vf);   
            
       // vdin_notify_receiver(VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL ,NULL);
 	 vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_VFRAME_READY,NULL);           
#ifdef VDIN_DBG_MSG_CNT
        vdin_dbg_msg.vdin_tasklet_valid_type_cnt++;
#endif
    }

    spin_unlock(&devp->isr_lock);
}


static int vdin_open(struct inode *inode, struct file *file)
{
    vdin_dev_t *devp;

    /* Get the per-device structure that contains this cdev */
    devp = container_of(inode->i_cdev, vdin_dev_t, cdev);
    file->private_data = devp;

	if (devp->index >= VDIN_COUNT)
        return -ENXIO;

    return 0;
}

static int vdin_release(struct inode *inode, struct file *file)
{
    vdin_dev_t *devp = file->private_data;
    file->private_data = NULL;

    //printk(KERN_INFO "vdin: device %d release ok.\n", devp->index);
    return 0;
}

static inline bool vdin_port_valid(enum tvin_port_e port)
{
    bool ret = false;

#if defined(CONFIG_ARCH_MESON)|| defined (CONFIG_ARCH_MESON3)
    switch (port>>8)
    {
        case 0x01: // mpeg
        case 0x02: // 656
        case 0x80: // dvin
            ret = true;
            break;
        default:
            break;
    }
#elif defined(CONFIG_ARCH_MESON2)
    switch (port>>8)
    {
        case 0x01: // mpeg
        case 0x02: // 656
        case 0x04: // VGA
        case 0x08: // COMPONENT
        case 0x10: // CVBS
        case 0x20: // SVIDEO
        case 0x40: // hdmi
        case 0x80: // dvin
            ret = true;
        default:
            break;
    }
#endif
    return ret;
}

static int vdin_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    vdin_dev_t *devp;
    void __user *argp = (void __user *)arg;

	if (_IOC_TYPE(cmd) != TVIN_IOC_MAGIC) {
		return -EINVAL;
	}

    devp = container_of(inode->i_cdev, vdin_dev_t, cdev);

    switch (cmd)
    {
        case TVIN_IOC_START_DEC:
        {
            struct tvin_parm_s para;
            pr_info("vdin%d: TVIN_IOC_START_DEC (0x%x)\n", devp->index, devp->flags);
            if(devp->flags & VDIN_FLAG_DEC_STARTED)
            {
                pr_err("vdin%d: decode started already\n", devp->index);
                ret = -EINVAL;
                break;
            }

            if (copy_from_user(&para, argp, sizeof(struct tvin_parm_s)))
            {
                pr_err("vdin%d: invalid paramenter\n", devp->index);
                ret = -EFAULT;
                break;
            }

            if (!vdin_port_valid(para.port))
            {
                pr_err("vdin%d: not supported port 0x%x\n", devp->index, para.port);
                ret = -EFAULT;
                break;
            }

            //init vdin signal info
            devp->para.port = para.port;
            devp->para.fmt_info.fmt= para.fmt_info.fmt;
			devp->para.fmt_info.frame_rate= para.fmt_info.frame_rate;
			devp->para.fmt_info.h_active=para.fmt_info.h_active;
			devp->para.fmt_info.v_active=para.fmt_info.v_active;
			devp->para.fmt_info.reserved=para.fmt_info.reserved;
            devp->para.status = TVIN_SIG_STATUS_NULL;
            devp->para.cap_addr = 0x85100000;
            devp->flags |= VDIN_FLAG_DEC_STARTED;
            //printk("devp addr is %x",devp);
            //printk("addr_offset is %x",devp->addr_offset);
            
            vdin_start_dec(devp);
            pr_info("vdin%d: TVIN_IOC_START_DEC ok\n", devp->index);
#if defined(CONFIG_ARCH_MESON2)
            vdin_set_meas_mux(devp->addr_offset, devp->para.port);
#endif
            msleep(10);
            tasklet_enable(&devp->isr_tasklet);
            devp->pre_irq_time = jiffies,
            enable_irq(devp->irq);
            pr_info("TVIN_IOC_START_DEC ok, vdin%d_irq is enabled.\n", devp->index);

            break;
        }
        case TVIN_IOC_STOP_DEC:
        {
            pr_info("vdin%d: TVIN_IOC_STOP_DEC (flags:0x%x)\n", devp->index, devp->flags);
            if(!(devp->flags & VDIN_FLAG_DEC_STARTED))
            {
                pr_err("vdin%d: can't stop, decode havn't started\n", devp->index);
                ret = -EINVAL;
                break;
            }
            disable_irq_nosync(devp->irq);
            tasklet_disable_nosync(&devp->isr_tasklet);
            vdin_stop_dec(devp);
            devp->flags &= (~VDIN_FLAG_DEC_STARTED);

            break;
        }
        case TVIN_IOC_G_PARM:
        {
		    if (copy_to_user((void __user *)arg, &devp->para, sizeof(struct tvin_parm_s)))
		    {
               ret = -EFAULT;
		    }
            break;
        }

        case TVIN_IOC_S_PARM:
        {
            struct tvin_parm_s para = {TVIN_PORT_NULL, {TVIN_SIG_FMT_NULL,0,0,0,0}, TVIN_SIG_STATUS_NULL, 0, 0, 0,0};
            if (copy_from_user(&para, argp, sizeof(struct tvin_parm_s)))
		    {
                ret = -EFAULT;
                break;
            }
            //get tvin port selection and other setting
            devp->para.flag = para.flag;
            if(!(devp->para.flag & TVIN_PARM_FLAG_CAP))
            {
                devp->para.cap_addr = 0;  //reset frame capture address 0 means null data
                devp->para.cap_size = 0;
                devp->para.canvas_index = 0;
            }
            if(devp->para.port != para.port)
            {
                //to do

		    }
            break;
        }
        default:
            ret = -ENOIOCTLCMD;
            break;
    }

    return ret;
}

static int vdin_mmap(struct file *file, struct vm_area_struct * vma)
{
    vdin_dev_t *devp = file->private_data;
	unsigned long off, pfn;
	unsigned long start, size;
    u32 len;

    if (!devp)
    {
        pr_err("%s: the device is not exist. \n", __func__);
        return -ENODEV;
    }
    if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
    {
        pr_err("%s: memory address is exist. \n", __func__);
        return -EINVAL;
    }
    if(!(devp->para.flag & TVIN_PARM_FLAG_CAP))
    {
        pr_err("%s: the capture flag is not set . \n", __func__);
        return -EINVAL;
    }
 /*   if((devp->para.cap_addr == 0) || (devp->para.canvas_index == 0))
    {
        pr_err("%s: the capture address is invalid . \n", __func__);
        return -EINVAL;
    }

    pr_info("cap_addr = 0x%x; cap_size = %d\n", devp->para.cap_addr, devp->para.cap_size);
*/
	off = vma->vm_pgoff << PAGE_SHIFT;


    mutex_lock(&devp->mm_lock);
	start = devp->para.cap_addr;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + devp->para.cap_size);
    mutex_unlock(&devp->mm_lock);


	start &= PAGE_MASK;

	if ((vma->vm_end - vma->vm_start + off) > len)
	{
        pr_err("%s: memory address is overflow. \n", __func__);
        return -EINVAL;
	}
	off += start;

	vma->vm_pgoff = off >> PAGE_SHIFT;

	vma->vm_flags |= VM_IO | VM_RESERVED;
    size = vma->vm_end - vma->vm_start;
    pfn  = off >> PAGE_SHIFT;


	if (io_remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
	{
        pr_err("%s: remap is failing. \n", __func__);
        return -EAGAIN;
	}
	return 0;
}


static struct file_operations vdin_fops = {
    .owner   = THIS_MODULE,
    .open    = vdin_open,
    .release = vdin_release,
    .ioctl   = vdin_ioctl,
    .mmap    = vdin_mmap,
};


static int vdin_probe(struct platform_device *pdev)
{
    int ret;
    int i;
    struct device *devp;
    struct resource *res;
    char name[12];

    ret = alloc_chrdev_region(&vdin_devno, 0, VDIN_COUNT, VDIN_NAME);
	if (ret < 0) {
		printk(KERN_ERR "vdin: failed to allocate major number\n");
		return 0;
	}

    vdin_clsp = class_create(THIS_MODULE, VDIN_NAME);
    if (IS_ERR(vdin_clsp))
    {
        unregister_chrdev_region(vdin_devno, VDIN_COUNT);
        return PTR_ERR(vdin_clsp);
    }

    for (i = 0; i < VDIN_COUNT; ++i)
    {
        /* allocate memory for the per-device structure */
        vdin_devp[i] = kmalloc(sizeof(struct vdin_dev_s), GFP_KERNEL);
        if (!vdin_devp[i])
        {
            printk(KERN_ERR "vdin: failed to allocate memory for vdin device\n");
            return -ENOMEM;
        }
        vdin_devp[i]->index = i;

        vdin_devp[i]->dec_lock = SPIN_LOCK_UNLOCKED;
        vdin_devp[i]->dec_ops = NULL;

        /* connect the file operations with cdev */
        cdev_init(&vdin_devp[i]->cdev, &vdin_fops);
        vdin_devp[i]->cdev.owner = THIS_MODULE;
        /* connect the major/minor number to the cdev */
        ret = cdev_add(&vdin_devp[i]->cdev, (vdin_devno + i), 1);
    	if (ret) {
    		printk(KERN_ERR "vdin: failed to add device\n");
            /* @todo do with error */
    		return ret;
    	}
        /* create /dev nodes */
        devp = device_create(vdin_clsp, NULL, MKDEV(MAJOR(vdin_devno), i),
                            NULL, "vdin%d", i);
        if (IS_ERR(devp)) {
            printk(KERN_ERR "vdin: failed to create device node\n");
            class_destroy(vdin_clsp);
            /* @todo do with error */
            return PTR_ERR(devp);;
    	}


        /* get device memory */
        res = platform_get_resource(pdev, IORESOURCE_MEM, i);
        if (!res) {
            printk(KERN_ERR "vdin: can't get memory resource\n");
            return -EFAULT;
        }
        vdin_devp[i]->mem_start = res->start;
        vdin_devp[i]->mem_size  = res->end - res->start + 1;
        pr_info(" vdin[%d] memory start addr is %x, mem_size is %x . \n",i,
            vdin_devp[i]->mem_start,vdin_devp[i]->mem_size);

        /* get device irq */
        res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
        if (!res) {
            printk(KERN_ERR "vdin: can't get memory resource\n");
            return -EFAULT;
        }
        vdin_devp[i]->irq = res->start;
        vdin_devp[i]->flags = VDIN_FLAG_NULL;
        pr_info("vdin%d: flags:0x%x\n", vdin_devp[i]->index, vdin_devp[i]->flags);

        vdin_devp[i]->addr_offset = vdin_addr_offset[i];
        vdin_devp[i]->para.flag = 0;

        sprintf(name, "vdin%d-irq", i);
        /* register vdin irq */
        ret = request_irq(vdin_devp[i]->irq, vdin_isr, IRQF_SHARED, name, (void *)vdin_devp[i]);
        if (ret) {
            printk(KERN_ERR "vdin: irq regist error.\n");
            return -ENOENT;
        }
        disable_irq(vdin_devp[i]->irq);
        init_timer(&vdin_devp[i]->timer);
        vdin_devp[i]->timer.data = (ulong) &vdin_devp[i]->timer;
        vdin_devp[i]->timer.function = vdin_put_timer_func;
        vdin_devp[i]->timer.expires = jiffies + VDIN_PUT_INTERVAL * 50;
        add_timer(&vdin_devp[i]->timer);    
        mutex_init(&vdin_devp[i]->mm_lock);
        vdin_devp[i]->isr_lock = SPIN_LOCK_UNLOCKED;
        tasklet_init(&vdin_devp[i]->isr_tasklet, vdin_isr_tasklet, (unsigned long)vdin_devp[i]);
        tasklet_disable(&vdin_devp[i]->isr_tasklet);
    }

    printk(KERN_INFO "vdin: driver initialized ok\n");
    return 0;
}

static int vdin_remove(struct platform_device *pdev)
{
    int i = 0;

    for (i = 0; i < VDIN_COUNT; ++i)
    {
        mutex_destroy(vdin_devp[i]->mm_lock);
        tasklet_kill(&vdin_devp[i]->isr_tasklet);
        del_timer_sync(&vdin_devp[i]->timer);
        free_irq(vdin_devp[i]->irq,(void *)vdin_devp[i]);
        del_timer(&vdin_devp[i]->timer);
        device_destroy(vdin_clsp, MKDEV(MAJOR(vdin_devno), i));
        cdev_del(&vdin_devp[i]->cdev);
        kfree(vdin_devp[i]);
    }
    class_destroy(vdin_clsp);
    unregister_chrdev_region(vdin_devno, VDIN_COUNT);

    printk(KERN_ERR "vdin: driver removed ok.\n");
    return 0;
}

static struct platform_driver vdin_driver = {
    .probe      = vdin_probe,
    .remove     = vdin_remove,
    .driver     = {
        .name   = VDIN_DRIVER_NAME,
    }
};

static int __init vdin_init(void)
{
    int ret = 0;
    ret = platform_driver_register(&vdin_driver);
    if (ret != 0) {
        printk(KERN_ERR "failed to register vdin module, error %d\n", ret);
        return -ENODEV;
    }
    return ret;
}

static void __exit vdin_exit(void)
{
    platform_driver_unregister(&vdin_driver);
}

module_init(vdin_init);
module_exit(vdin_exit);

MODULE_DESCRIPTION("AMLOGIC VDIN driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");

