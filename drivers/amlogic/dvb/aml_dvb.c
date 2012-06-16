/*
 * AMLOGIC Smart card driver.
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
 */

#define ENABLE_DEMUX_DRIVER

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/amports/amstream.h>
#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
#include <mach/am_regs.h>
#endif
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/amdsc.h>
#include "aml_dvb.h"

#if 1
#define pr_dbg(fmt, args...) printk(KERN_DEBUG "DVB: " fmt, ## args)
#else
#define pr_dbg(fmt, args...)
#endif

#define pr_error(fmt, args...) printk(KERN_ERR "DVB: " fmt, ## args)

#define CARD_NAME "amlogic-dvb"

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

MODULE_PARM_DESC(demux0_irq, "\n\t\t Irq number of demux0");
static int demux0_irq = -1;
module_param(demux0_irq, int, S_IRUGO);

MODULE_PARM_DESC(demux1_irq, "\n\t\t Irq number of demux1");
static int demux1_irq = -1;
module_param(demux1_irq, int, S_IRUGO);

MODULE_PARM_DESC(asyncfifo0_irq, "\n\t\t Irq number of ASYNC FIFO0");
static int asyncfifo0_irq = -1;
module_param(asyncfifo0_irq, int, S_IRUGO);

MODULE_PARM_DESC(asyncfifo1_irq, "\n\t\t Irq number of ASYNC FIFO1");
static int asyncfifo1_irq = -1;
module_param(asyncfifo1_irq, int, S_IRUGO);

static struct aml_dvb aml_dvb_device;
static struct class   aml_stb_class;

static int aml_tsdemux_reset(void);
static int aml_tsdemux_set_reset_flag(void);
static int aml_tsdemux_request_irq(irq_handler_t handler, void *data);
static int aml_tsdemux_free_irq(void);
static int aml_tsdemux_set_vid(int vpid);
static int aml_tsdemux_set_aid(int apid);
static int aml_tsdemux_set_sid(int spid);

static struct tsdemux_ops aml_tsdemux_ops = {
.reset          = aml_tsdemux_reset,
.set_reset_flag = aml_tsdemux_set_reset_flag,
.request_irq    = aml_tsdemux_request_irq,
.free_irq       = aml_tsdemux_free_irq,
.set_vid        = aml_tsdemux_set_vid,
.set_aid        = aml_tsdemux_set_aid,
.set_sid        = aml_tsdemux_set_sid
};

static void aml_dvb_dmx_release(struct aml_dvb *advb, struct aml_dmx *dmx)
{
	int i;
	
	dvb_net_release(&dmx->dvb_net);
	aml_dmx_hw_deinit(dmx);
	dmx->demux.dmx.close(&dmx->demux.dmx);
	dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->mem_fe);
	
	for (i=0; i<DMX_DEV_COUNT; i++) {
		dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->hw_fe[i]);
	}
	
	dvb_dmxdev_release(&dmx->dmxdev);
	dvb_dmx_release(&dmx->demux);
}

static int aml_dvb_dmx_init(struct aml_dvb *advb, struct aml_dmx *dmx, int id)
{
	int i, ret;
	struct resource *res;
	char buf[32];
	
	dmx->dmx_irq = id?demux1_irq:demux0_irq;
	if(dmx->dmx_irq==-1) {
		snprintf(buf, sizeof(buf), "demux%d_irq", id);
		res = platform_get_resource_byname(advb->pdev, IORESOURCE_IRQ, buf);
		if (!res) {
			pr_error("cannot get resource %s\n", buf);
			return -1;
		}
		dmx->dmx_irq = res->start;
	}
	
	dmx->source  = AM_TS_SRC_TS0;
	dmx->dvr_irq = -1;
	/*
	if(id==0) {
		dmx->dvr_irq = dvr0_irq;
		if(dmx->dvr_irq==-1) {
			snprintf(buf, sizeof(buf), "dvr%d_irq", id);
			res = platform_get_resource_byname(advb->pdev, IORESOURCE_IRQ, buf);
			if (!res) {
				pr_error("cannot get resource %s\n", buf);
				return -1;
			}
			dmx->dvr_irq = res->start;
		}
	} else {
		dmx->dvr_irq = -1;
	}
	*/
	dmx->demux.dmx.capabilities = (DMX_TS_FILTERING | DMX_SECTION_FILTERING | DMX_PES_FILTERING | DMX_MEMORY_BASED_FILTERING | DMX_TS_DESCRAMBLING);
	dmx->demux.filternum = dmx->demux.feednum = FILTER_COUNT;
	dmx->demux.priv = advb;
	dmx->demux.start_feed = aml_dmx_hw_start_feed;
	dmx->demux.stop_feed = aml_dmx_hw_stop_feed;
	dmx->demux.write_to_decoder = NULL;
	spin_lock_init(&dmx->slock);
	
	if ((ret = dvb_dmx_init(&dmx->demux)) < 0) {
		pr_error("dvb_dmx failed: error %d",ret);
		goto error_dmx_init;
	}
	
	dmx->dmxdev.filternum = dmx->demux.feednum;
	dmx->dmxdev.demux = &dmx->demux.dmx;
	dmx->dmxdev.capabilities = 0;
	if ((ret = dvb_dmxdev_init(&dmx->dmxdev, &advb->dvb_adapter)) < 0) {
		pr_error("dvb_dmxdev_init failed: error %d",ret);
		goto error_dmxdev_init;
	}
	
	for (i=0; i<DMX_DEV_COUNT; i++) {
		int source = i+DMX_FRONTEND_0;
		dmx->hw_fe[i].source = source;
		
		if ((ret = dmx->demux.dmx.add_frontend(&dmx->demux.dmx, &dmx->hw_fe[i])) < 0) {
			pr_error("adding hw_frontend to dmx failed: error %d",ret);
			dmx->hw_fe[i].source = 0;
			goto error_add_hw_fe;
		}
	}
	
	dmx->mem_fe.source = DMX_MEMORY_FE;
	if ((ret = dmx->demux.dmx.add_frontend(&dmx->demux.dmx, &dmx->mem_fe)) < 0) {
		pr_error("adding mem_frontend to dmx failed: error %d",ret);
		goto error_add_mem_fe;
	}
	
	if ((ret = dmx->demux.dmx.connect_frontend(&dmx->demux.dmx, &dmx->hw_fe[1])) < 0) {
		pr_error("connect frontend failed: error %d",ret);
		goto error_connect_fe;
	}
	
	dmx->id = id;
	dmx->aud_chan = -1;
	dmx->vid_chan = -1;
	dmx->sub_chan = -1;
	
	if ((ret = aml_dmx_hw_init(dmx)) <0) {
		pr_error("demux hw init error %d", ret);
		dmx->id = -1;
		goto error_dmx_hw_init;
	}
	
	dvb_net_init(&advb->dvb_adapter, &dmx->dvb_net, &dmx->demux.dmx);
	
	return 0;
error_dmx_hw_init:
error_connect_fe:
	dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->mem_fe);
error_add_mem_fe:
error_add_hw_fe:
	for (i=0; i<DMX_DEV_COUNT; i++) {
		if (dmx->hw_fe[i].source)
			dmx->demux.dmx.remove_frontend(&dmx->demux.dmx, &dmx->hw_fe[i]);
	}
	dvb_dmxdev_release(&dmx->dmxdev);
error_dmxdev_init:
	dvb_dmx_release(&dmx->demux);
error_dmx_init:
	return ret;
}
struct aml_dvb* aml_get_dvb_device(void)
{
	return &aml_dvb_device;
}

EXPORT_SYMBOL(aml_get_dvb_device);

static int dvb_dsc_open(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct aml_dvb *dvb = dvbdev->priv;
	struct aml_dsc *dsc;
	int err, id;
	unsigned long flags;

	spin_lock_irqsave(&dvb->slock, flags);

	for(id=0; id<DSC_COUNT; id++) {
		if(!dvb->dsc[id].used) {
			dvb->dsc[id].used = 1;
			dvbdev->users++;
			break;
		}
	}

	spin_unlock_irqrestore(&dvb->slock, flags);

	if(id>=DSC_COUNT) {
		pr_error("too many descrambler\n");
		return -EBUSY;
	}
	
	dsc = &dvb->dsc[id];
	dsc->id   = id;
	dsc->pid  = -1;
	dsc->set  = 0;
	dsc->dvb  = dvb;
	
	file->private_data = dsc;
	return 0;
}

static int dvb_dsc_ioctl(struct inode *inode, struct file *file,
                 unsigned int cmd, unsigned long arg)
{
	struct aml_dsc *dsc = file->private_data;
	struct aml_dvb *dvb = dsc->dvb;
	struct am_dsc_key key;
	int ret = 0, i;
	unsigned long flags;
	
	spin_lock_irqsave(&dvb->slock, flags);
	
	switch(cmd) {
		case AMDSC_IOC_SET_PID:
			for(i=0; i<DSC_COUNT; i++) {
				if(i==dsc->id)
					continue;
				if(dvb->dsc[i].used && (dvb->dsc[i].pid==arg)) {
					ret = -EBUSY;
				}
			}
			dsc->pid = arg;
			dsc_set_pid(dsc, dsc->pid);
		break;
		case AMDSC_IOC_SET_KEY:
			if (copy_from_user(&key, (void __user *)arg, sizeof(struct am_dsc_key))) {
				ret = -EFAULT;
			} else {
				if(key.type)
					memcpy(dsc->odd, key.key, 8);
				else
					memcpy(dsc->even, key.key, 8);
				dsc->set |= 1<<(key.type);
				dsc_set_key(dsc, key.type, key.key);
			}
		break;
		default:
			ret = -EINVAL;
		break;
	}
	
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	return ret;
}

static int dvb_dsc_release(struct inode *inode, struct file *file)
{
	struct aml_dsc *dsc = file->private_data;
	struct aml_dvb *dvb = dsc->dvb;
	unsigned long flags;
	
	//dvb_generic_release(inode, file);
	
	spin_lock_irqsave(&dvb->slock, flags);

	dsc->used = 0;
	dsc_release(dsc);
	dvb->dsc_dev->users--;
	
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	return 0;
}

static int aml_dvb_asyncfifo_init(struct aml_dvb *advb, struct aml_asyncfifo *asyncfifo, int id)
{
	struct resource *res;
	char buf[32];
	
	asyncfifo->asyncfifo_irq = id?asyncfifo1_irq:asyncfifo0_irq;
	if(asyncfifo->asyncfifo_irq==-1) {
		snprintf(buf, sizeof(buf), "dvr%d_irq", id);
		res = platform_get_resource_byname(advb->pdev, IORESOURCE_IRQ, buf);
		if (!res) {
			pr_error("cannot get resource %s\n", buf);
			return -1;
		}
		asyncfifo->asyncfifo_irq = res->start;
	}

	asyncfifo->dvb = advb;
	asyncfifo->id = id;
	asyncfifo->init = 0;
	
	return aml_asyncfifo_hw_init(asyncfifo);
}

static void aml_dvb_asyncfifo_release(struct aml_dvb *advb, struct aml_asyncfifo *asyncfifo)
{
	aml_asyncfifo_hw_deinit(asyncfifo);
}

/*Show the STB input source*/
static ssize_t stb_show_source(struct class *class, struct class_attribute *attr,char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;
	char *src;
	
	switch(dvb->stb_source) {
		case AM_TS_SRC_TS0:
		case AM_TS_SRC_S2P0:
			src = "ts0";
		break;
		case AM_TS_SRC_TS1:
		case AM_TS_SRC_S2P1:
			src = "ts1";
		break;
		case AM_TS_SRC_TS2:
			src = "ts2";
		break;		
		case AM_TS_SRC_HIU:
			src = "hiu";
		break;
		default:
			src = "";
		break;
	}
	
	ret = sprintf(buf, "%s\n", src);
	return ret;
}

/*Set the STB input source*/
static ssize_t stb_store_source(struct class *class,struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
    dmx_source_t src = -1;
	
	if(!strncmp("ts0", buf, 3)) {
    	src = DMX_SOURCE_FRONT0;
    } else if(!strncmp("ts1", buf, 3)) {
    	src = DMX_SOURCE_FRONT1;
    }else if(!strncmp("ts2", buf, 3)) {
    	src = DMX_SOURCE_FRONT2;
    } else if(!strncmp("hiu", buf, 3)) {
    	src = DMX_SOURCE_DVR0;
    }
    if(src!=-1) {
    	aml_stb_hw_set_source(&aml_dvb_device, src);
    }
    
    return size;
}
/*Show the descrambler's input source*/
static ssize_t dsc_show_source(struct class *class,struct class_attribute *attr, char *buf)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	ssize_t ret = 0;
	char *src;
	
	switch(dvb->dsc_source) {
		case AM_DMX_0:
			src = "dmx0";
		break;
		case AM_DMX_1:
			src = "dmx1";
		break;
		case AM_DMX_2:
			src = "dmx2";
		break;

		default:
			src = "";
		break;
	}
	
	ret = sprintf(buf, "%s\n", src);
	return ret;
}

/*Set the descrambler's input source*/
static ssize_t dsc_store_source(struct class *class,struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
	dmx_source_t src = -1;

	if(!strncmp("dmx0", buf, 4)) {
		src = AM_DMX_0;
	} else if(!strncmp("dmx1", buf, 4)) {
		src = AM_DMX_1;
	}else if(!strncmp("dmx2", buf, 4)) {
		src = AM_DMX_2;
	}

	if(src!=-1) {
		aml_dsc_hw_set_source(&aml_dvb_device, src);
	}

	return size;
}


/*Show the STB input source*/
#define DEMUX_SOURCE_FUNC_DECL(i)  \
static ssize_t demux##i##_show_source(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_dmx *dmx = &dvb->dmx[i];\
	ssize_t ret = 0;\
	char *src;\
	switch(dmx->source) {\
		case AM_TS_SRC_TS0:\
		case AM_TS_SRC_S2P0:\
			src = "ts0";\
		break;\
		case AM_TS_SRC_TS1:\
		case AM_TS_SRC_S2P1:\
			src = "ts1";\
		break;\
		case AM_TS_SRC_TS2:\
			src = "ts2";\
		break;\
		case AM_TS_SRC_HIU:\
			src = "hiu";\
		break;\
		default:\
			src = "";\
		break;\
	}\
	ret = sprintf(buf, "%s\n", src);\
	return ret;\
}\
static ssize_t demux##i##_store_source(struct class *class,  struct class_attribute *attr,const char *buf, size_t size)\
{\
    dmx_source_t src = -1;\
    \
	if(!strncmp("ts0", buf, 3)) {\
    	src = DMX_SOURCE_FRONT0;\
    } else if(!strncmp("ts1", buf, 3)) {\
    	src = DMX_SOURCE_FRONT1;\
    } else if(!strncmp("ts2", buf, 3)) {\
    	src = DMX_SOURCE_FRONT2;\
    } else if(!strncmp("hiu", buf, 3)) {\
    	src = DMX_SOURCE_DVR0;\
    }\
    if(src!=-1) {\
    	aml_dmx_hw_set_source(aml_dvb_device.dmx[i].dmxdev.demux, src);\
    }\
    return size;\
}

#if DMX_DEV_COUNT>0
	DEMUX_SOURCE_FUNC_DECL(0)
#endif
#if DMX_DEV_COUNT>1
	DEMUX_SOURCE_FUNC_DECL(1)
#endif
#if DMX_DEV_COUNT>2
	DEMUX_SOURCE_FUNC_DECL(2)
#endif

/*Show the async fifo source*/
#define ASYNCFIFO_SOURCE_FUNC_DECL(i)  \
static ssize_t asyncfifo##i##_show_source(struct class *class,  struct class_attribute *attr,char *buf)\
{\
	struct aml_dvb *dvb = &aml_dvb_device;\
	struct aml_asyncfifo *afifo = &dvb->asyncfifo[i];\
	ssize_t ret = 0;\
	char *src;\
	switch(afifo->source) {\
		case AM_DMX_0:\
			src = "dmx0";\
		break;\
		case AM_DMX_1:\
			src = "dmx1";\
		break;\
		case AM_DMX_2:\
			src = "dmx2";\
		break;\
		default:\
			src = "";\
		break;\
	}\
	ret = sprintf(buf, "%s\n", src);\
	return ret;\
}\
static ssize_t asyncfifo##i##_store_source(struct class *class,  struct class_attribute *attr,const char *buf, size_t size)\
{\
    aml_dmx_id_t src = -1;\
    \
	if(!strncmp("dmx0", buf, 4)) {\
    	src = AM_DMX_0;\
    } else if(!strncmp("dmx1", buf, 4)) {\
    	src = AM_DMX_1;\
    } else if(!strncmp("dmx2", buf, 4)) {\
    	src = AM_DMX_2;\
    }\
    if(src!=-1) {\
    	aml_asyncfifo_hw_set_source(&aml_dvb_device.asyncfifo[i], src);\
    }\
    return size;\
}

#if ASYNCFIFO_COUNT>0
	ASYNCFIFO_SOURCE_FUNC_DECL(0)
#endif
#if ASYNCFIFO_COUNT>1
	ASYNCFIFO_SOURCE_FUNC_DECL(1)
#endif


extern int dmx_reset_hw(struct aml_dvb *dvb);

/*Reset the Demux*/
static ssize_t demux_do_reset(struct class *class,struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
	if(!strncmp("1", buf, 1)) {
		struct aml_dvb *dvb = &aml_dvb_device;
		unsigned long flags;
		
		spin_lock_irqsave(&dvb->slock, flags);
		pr_info("Reset demux, call dmx_reset_hw\n");
		dmx_reset_hw(dvb);
		spin_unlock_irqrestore(&dvb->slock, flags);
	}
	
	return size;
}


static struct file_operations dvb_dsc_fops = {
        .owner          = THIS_MODULE,
        .read           = NULL,
        .write          = NULL,
        .ioctl          = dvb_dsc_ioctl,
        .open           = dvb_dsc_open,
        .release        = dvb_dsc_release,
        .poll           = NULL,
};

static struct dvb_device dvbdev_dsc = {
        .priv           = NULL,
        .users          = DSC_COUNT,
        .writers        = DSC_COUNT,
        .fops           = &dvb_dsc_fops,
};

static struct class_attribute aml_stb_class_attrs[] = {
	__ATTR(source,  S_IRUGO | S_IWUSR, stb_show_source, stb_store_source),
	__ATTR(dsc_source,  S_IRUGO | S_IWUSR, dsc_show_source, dsc_store_source),
#define DEMUX_SOURCE_ATTR_DECL(i)\
		__ATTR(demux##i##_source,  S_IRUGO | S_IWUSR, demux##i##_show_source, demux##i##_store_source)
#if DMX_DEV_COUNT>0
	DEMUX_SOURCE_ATTR_DECL(0),
#endif
#if DMX_DEV_COUNT>1
	DEMUX_SOURCE_ATTR_DECL(1),
#endif
#if DMX_DEV_COUNT>2
	DEMUX_SOURCE_ATTR_DECL(2),
#endif
#define ASYNCFIFO_SOURCE_ATTR_DECL(i)\
		__ATTR(asyncfifo##i##_source,  S_IRUGO | S_IWUSR, asyncfifo##i##_show_source, asyncfifo##i##_store_source)
#if ASYNCFIFO_COUNT>0
	ASYNCFIFO_SOURCE_ATTR_DECL(0),
#endif
#if ASYNCFIFO_COUNT>1
	ASYNCFIFO_SOURCE_ATTR_DECL(1),
#endif
	__ATTR(demux_reset,  S_IRUGO | S_IWUSR, NULL, demux_do_reset),
	
	__ATTR_NULL
};

static struct class aml_stb_class = {
	.name = "stb",
	.class_attrs = aml_stb_class_attrs,
};


static int aml_dvb_probe(struct platform_device *pdev)
{
	struct aml_dvb *advb;
	int i, ret = 0;
	
	advb = &aml_dvb_device;
	memset(advb, 0, sizeof(aml_dvb_device));
	
	spin_lock_init(&advb->slock);
	
	advb->dev  = &pdev->dev;
	advb->pdev = pdev;
	advb->dsc_source=AM_DMX_MAX;
	
	for (i=0; i<DMX_DEV_COUNT; i++) {
		advb->dmx[i].dmx_irq = -1;
		advb->dmx[i].dvr_irq = -1;
	}
	
	ret = dvb_register_adapter(&advb->dvb_adapter, CARD_NAME, THIS_MODULE, advb->dev, adapter_nr);
	if (ret < 0) {
		return ret;
	}
	
	for (i=0; i<DMX_DEV_COUNT; i++)
		advb->dmx[i].id = -1;
	
	advb->dvb_adapter.priv = advb;
	dev_set_drvdata(advb->dev, advb);
	
	for (i=0; i<DMX_DEV_COUNT; i++) {
		if ((ret=aml_dvb_dmx_init(advb, &advb->dmx[i], i))<0) {
			goto error;
		}
	}
	
	/*Register descrambler device*/
	ret = dvb_register_device(&advb->dvb_adapter, &advb->dsc_dev,
                                   &dvbdev_dsc, advb, DVB_DEVICE_DSC);
	if(ret<0) {
		goto error;
	}

	/*Init the async fifos*/
	for (i=0; i<ASYNCFIFO_COUNT; i++) {
		if ((ret=aml_dvb_asyncfifo_init(advb, &advb->asyncfifo[i], i))<0) {
			goto error;
		}
	}
	
	extern int aml_regist_dmx_class();
	aml_regist_dmx_class();
	
	if(class_register(&aml_stb_class)<0) {
                pr_error("register class error\n");
                goto error;
        }
        
        tsdemux_set_ops(&aml_tsdemux_ops);
	
        tsdemux_set_ops(&aml_tsdemux_ops);
	

	return ret;
error:
	for (i=0; i<ASYNCFIFO_COUNT; i++) {
		if (advb->asyncfifo[i].id!=-1) {
			aml_dvb_asyncfifo_release(advb, &advb->asyncfifo[i]);
		}
	}
	
	if(advb->dsc_dev) {
		dvb_unregister_device(advb->dsc_dev);
	}
	
	for (i=0; i<DMX_DEV_COUNT; i++) {
		if (advb->dmx[i].id!=-1) {
			aml_dvb_dmx_release(advb, &advb->dmx[i]);
		}
	}
	dvb_unregister_adapter(&advb->dvb_adapter);
	
	return ret;
}

static int aml_dvb_remove(struct platform_device *pdev)
{
	struct aml_dvb *advb = (struct aml_dvb*)dev_get_drvdata(&pdev->dev);
	int i;
	
	tsdemux_set_ops(NULL);


	extern int aml_unregist_dmx_class();
	aml_unregist_dmx_class();
	class_unregister(&aml_stb_class);
	
	dvb_unregister_device(advb->dsc_dev);
	
	for (i=0; i<DMX_DEV_COUNT; i++) {
		aml_dvb_dmx_release(advb, &advb->dmx[i]);
	}
	dvb_unregister_adapter(&advb->dvb_adapter);



	return 0;
}

static struct platform_driver aml_dvb_driver = {
	.probe		= aml_dvb_probe,
	.remove		= aml_dvb_remove,	
	.driver		= {
		.name	= "amlogic-dvb",
		.owner	= THIS_MODULE,
	}
};

static int __init aml_dvb_init(void)
{
	printk("aml_dvb_init \n");
	return platform_driver_register(&aml_dvb_driver);
}

static void __exit aml_dvb_exit(void)
{
	 platform_driver_unregister(&aml_dvb_driver);
}

/*Get the STB source demux*/
static struct aml_dmx* get_stb_dmx(void)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	int i;
	
	for(i=0; i<DMX_DEV_COUNT; i++) {
		dmx = &dvb->dmx[i];
		if(dmx->source==dvb->stb_source) {
			return dmx;
		}
	}
	
	return NULL;
}

static int aml_tsdemux_reset(void)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	unsigned long flags;
	
	spin_lock_irqsave(&dvb->slock, flags);
	if(dvb->reset_flag) {
		dvb->reset_flag = 0;
		dmx_reset_hw(dvb);
	}
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	return 0;
}

static int aml_tsdemux_set_reset_flag(void)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	unsigned long flags;
	
	spin_lock_irqsave(&dvb->slock, flags);
	dvb->reset_flag = 1;
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	return 0;

}

/*Add the amstream irq handler*/
static int aml_tsdemux_request_irq(irq_handler_t handler, void *data)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	
	spin_lock_irqsave(&dvb->slock, flags);
	
	dmx = get_stb_dmx();
	if(dmx) {
		dmx->irq_handler = handler;
		dmx->irq_data = data;
	}
	
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	return 0;
}

/*Free the amstream irq handler*/
static int aml_tsdemux_free_irq(void)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	
	spin_lock_irqsave(&dvb->slock, flags);
	
	dmx = get_stb_dmx();
	if(dmx) {
		dmx->irq_handler = NULL;
		dmx->irq_data = NULL;
	}
	
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	return 0;
}

/*Reset the video PID*/
static int aml_tsdemux_set_vid(int vpid)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	int ret = 0;
	
	spin_lock_irqsave(&dvb->slock, flags);
	dmx = get_stb_dmx();
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	if(dmx) {
		mutex_lock(&dmx->dmxdev.mutex);
		
		spin_lock_irqsave(&dmx->slock, flags);
		
		if(dmx->vid_chan!=-1) {
			dmx_free_chan(dmx, dmx->vid_chan);
			dmx->vid_chan = -1;
		}
		
		if((vpid>=0) && (vpid<0x1FFF)) {
			dmx->vid_chan = dmx_alloc_chan(dmx, DMX_TYPE_TS, DMX_TS_PES_VIDEO, vpid);
			if(dmx->vid_chan==-1) {
				ret = -1;
			}
		}
		
		spin_unlock_irqrestore(&dmx->slock, flags);
		
		mutex_unlock(&dmx->dmxdev.mutex);
	}
	
	return ret;
}

/*Reset the audio PID*/
static int aml_tsdemux_set_aid(int apid)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	int ret = 0;
	
	spin_lock_irqsave(&dvb->slock, flags);
	dmx = get_stb_dmx();
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	if(dmx) {
		mutex_lock(&dmx->dmxdev.mutex);
		
		spin_lock_irqsave(&dmx->slock, flags);
		
		if(dmx->aud_chan!=-1) {
			dmx_free_chan(dmx, dmx->aud_chan);
			dmx->aud_chan = -1;
		}
		
		if((apid>=0) && (apid<0x1FFF)) {
			dmx->aud_chan = dmx_alloc_chan(dmx, DMX_TYPE_TS, DMX_TS_PES_AUDIO, apid);
			if(dmx->aud_chan==-1) {
				ret = -1;
			}
		}
		
		spin_unlock_irqrestore(&dmx->slock, flags);
		
		mutex_unlock(&dmx->dmxdev.mutex);
	}
	
	return ret;
}

/*Reset the subtitle PID*/
static int aml_tsdemux_set_sid(int spid)
{
	struct aml_dvb *dvb = &aml_dvb_device;
	struct aml_dmx *dmx;
	unsigned long flags;
	int ret = 0;
	
	spin_lock_irqsave(&dvb->slock, flags);
	dmx = get_stb_dmx();
	spin_unlock_irqrestore(&dvb->slock, flags);
	
	if(dmx) {
		mutex_lock(&dmx->dmxdev.mutex);
		
		spin_lock_irqsave(&dmx->slock, flags);
		
		if(dmx->sub_chan!=-1) {
			dmx_free_chan(dmx, dmx->sub_chan);
			dmx->sub_chan = -1;
		}
		
		if((spid>=0) && (spid<0x1FFF)) {
			dmx->sub_chan = dmx_alloc_chan(dmx, DMX_TYPE_TS, DMX_TS_PES_SUBTITLE, spid);
			if(dmx->sub_chan==-1) {
				ret = -1;
			}
		}
		
		spin_unlock_irqrestore(&dmx->slock, flags);
		
		mutex_unlock(&dmx->dmxdev.mutex);
	}
	
	return ret;
}

module_init(aml_dvb_init);
module_exit(aml_dvb_exit);

MODULE_DESCRIPTION("driver for the AMLogic DVB card");
MODULE_AUTHOR("AMLOGIC");
MODULE_LICENSE("GPL");

