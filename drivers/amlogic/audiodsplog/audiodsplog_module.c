#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	
#include <linux/device.h>	
#include "log_stream.h"	
#include <linux/amports/dsp_register.h>

MODULE_LICENSE("GPL");

#define AUDIODSPLOG_GET_RING_BUF_SIZE      _IOR('l', 0x01, unsigned long)
#define AUDIODSPLOG_GET_RING_BUF_CONTENT   _IOR('l', 0x02, unsigned long)
#define AUDIODSPLOG_GET_RING_BUF_SPACE     _IOR('l', 0x03, unsigned long)

static int __init audiodsplog_init_module(void);
static void __exit audiodsplog_exit_module(void);
static int audiodsplog_open(struct inode *, struct file *);
static int audiodsplog_release(struct inode *, struct file *);
static ssize_t audiodsplog_read(struct file *, char *, size_t, loff_t *);
static ssize_t audiodsplog_write(struct file *, const char *, size_t, loff_t *);
static int audiodsplog_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long args);
static int audiodsplog_create_stream_buffer(void);
static int audiodsplog_destroy_stream_buffer(void);

#define SUCCESS 0
#define DEVICE_NAME "audiodsplog"	

#define MIN_CACHE_ALIGN(x)	(((x-4)&(~0x1f)))
#define MAX_CACHE_ALIGN(x)	((x+0x1f)&(~0x1f))

#ifdef MIN
#undef MIN
#endif
#define MIN(x,y)	(((x)<(y))?(x):(y))


static int major;		
static struct class *class_log;
static struct device *dev_log;
static int device_opened = 0;	/* Is device open?  
                             * Used to prevent multiple access to device */
static char *buf = NULL;	

typedef struct {
    void *stream_buffer_mem;
	unsigned int stream_buffer_mem_size;
	unsigned long stream_buffer_start;
	unsigned long stream_buffer_end;
	unsigned long stream_buffer_size;	
}priv_data_t; 

static priv_data_t priv_data = {0};

static struct file_operations fops = {
    .read = audiodsplog_read,
    .ioctl = audiodsplog_ioctl,
    .write = audiodsplog_write,
    .open = audiodsplog_open,
    .release = audiodsplog_release
};

static int __init audiodsplog_init_module(void)
{
    void *ptr_err;
    int ret = 0;
    major = register_chrdev(0, DEVICE_NAME, &fops);

    if (major < 0) {
        printk(KERN_ALERT "Registering char device %s failed with %d\n", DEVICE_NAME, major);
        return major;
    }

    class_log = class_create(THIS_MODULE, DEVICE_NAME);
    if(IS_ERR(ptr_err = class_log)){
        goto err0;
    }

    dev_log = device_create(class_log, NULL, MKDEV(major,0),NULL, DEVICE_NAME);
    if(IS_ERR(ptr_err = dev_log)){
        goto err1;
    }

    priv_data.stream_buffer_mem_size = 512*1024;

    ret = audiodsplog_create_stream_buffer(); 
    if(ret){
        goto err2;
    }

    printk(KERN_INFO "amlogic audio dsp log device init!\n");

    return SUCCESS;

err2:
    device_destroy(class_log, MKDEV(major, 0));
err1:
    class_destroy(class_log);
err0:
    unregister_chrdev(major, DEVICE_NAME);
    return PTR_ERR(ptr_err);
}

static void __exit audiodsplog_exit_module(void)
{
    device_destroy(class_log, MKDEV(major, 0));
    class_destroy(class_log);
    unregister_chrdev(major, DEVICE_NAME);

    audiodsplog_destroy_stream_buffer();
}

static int audiodsplog_open(struct inode *inode, struct file *file)
{
    if (device_opened)
        return -EBUSY;

    device_opened++;
    try_module_get(THIS_MODULE);
    if(buf == NULL)	
    	buf = (char *)kmalloc(priv_data.stream_buffer_size, GFP_KERNEL);
    if(buf == NULL){
	 device_opened--; 	
    	 module_put(THIS_MODULE);
        return -ENOMEM;
    }
    audiodsplog_create_stream_buffer(); //init the r/p register	
    log_stream_init();

    return SUCCESS;
}

static int audiodsplog_release(struct inode *inode, struct file *file)
{
    device_opened--;		
    module_put(THIS_MODULE);
#if 0    
    if(buf){
        kfree(buf);
        buf = NULL;
    }
#endif
    log_stream_deinit();

    return SUCCESS;
}

static ssize_t audiodsplog_read(struct file *filp,	
        char __user *buffer,	
        size_t length,	
        loff_t * offset)
{
    int bytes_read = 0;
    int len = 0;

    if(buf == NULL){
        return 0;
    }

    len = MIN(length, log_stream_content());
    bytes_read = log_stream_read(buf, len);
    copy_to_user((void *)buffer, (const char *)buf, bytes_read);
    return bytes_read;
}

static ssize_t audiodsplog_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
    return -EINVAL;
}

static int audiodsplog_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long args)
{
    int ret = 0;
    unsigned long *val = (unsigned long *)args;

    switch(cmd){
        case AUDIODSPLOG_GET_RING_BUF_SIZE:
        *val = priv_data.stream_buffer_size;
        break;
        case AUDIODSPLOG_GET_RING_BUF_CONTENT:
        *val = log_stream_content();
        break;
        case AUDIODSPLOG_GET_RING_BUF_SPACE:
        *val = log_stream_space();
        break;
    }

    return ret;
}

static int audiodsplog_create_stream_buffer(void)
{
    dma_addr_t buf_map;

    DSP_WD(DSP_LOG_START_ADDR, ARM_2_ARC_ADDR_SWAP(0));
    DSP_WD(DSP_LOG_END_ADDR,ARM_2_ARC_ADDR_SWAP(0));
    DSP_WD(DSP_LOG_RD_ADDR,ARM_2_ARC_ADDR_SWAP(0));
    DSP_WD(DSP_LOG_WD_ADDR,ARM_2_ARC_ADDR_SWAP(0));

    if(priv_data.stream_buffer_mem_size == 0){
        return 0;
    }

    if(priv_data.stream_buffer_mem == NULL)
        priv_data.stream_buffer_mem = (void*)kmalloc(priv_data.stream_buffer_mem_size,GFP_KERNEL);
    if(priv_data.stream_buffer_mem == NULL){
        printk("kmalloc error,no memory for audio dsp log stream buffer\n");
        return -ENOMEM;
    }

    memset((void *)priv_data.stream_buffer_mem, 0, priv_data.stream_buffer_mem_size);
    buf_map = dma_map_single(NULL, (void *)priv_data.stream_buffer_mem, priv_data.stream_buffer_mem_size, DMA_FROM_DEVICE);
    dma_unmap_single(NULL, buf_map,  priv_data.stream_buffer_mem_size, DMA_FROM_DEVICE);

    priv_data.stream_buffer_start = MAX_CACHE_ALIGN((unsigned long)priv_data.stream_buffer_mem);
    priv_data.stream_buffer_end = MIN_CACHE_ALIGN((unsigned long)priv_data.stream_buffer_mem + priv_data.stream_buffer_mem_size);
    priv_data.stream_buffer_size = priv_data.stream_buffer_end - priv_data.stream_buffer_start;

    if(priv_data.stream_buffer_size < 0){
        printk("DSP log Stream buffer set error,must more larger,mensize = %d,buffer size=%ld\n",
                priv_data.stream_buffer_mem_size,priv_data.stream_buffer_size);

        kfree(priv_data.stream_buffer_mem);
        priv_data.stream_buffer_mem = NULL;
        return -2;
    }

    DSP_WD(DSP_LOG_START_ADDR,ARM_2_ARC_ADDR_SWAP(priv_data.stream_buffer_start));
    DSP_WD(DSP_LOG_END_ADDR,ARM_2_ARC_ADDR_SWAP(priv_data.stream_buffer_end));
    DSP_WD(DSP_LOG_RD_ADDR,ARM_2_ARC_ADDR_SWAP(priv_data.stream_buffer_start));
    DSP_WD(DSP_LOG_WD_ADDR,ARM_2_ARC_ADDR_SWAP(priv_data.stream_buffer_start));

    printk("DSP log stream buffer to [%#lx-%#lx]\n",ARM_2_ARC_ADDR_SWAP(priv_data.stream_buffer_start), ARM_2_ARC_ADDR_SWAP(priv_data.stream_buffer_end));
    return 0;
}

static int audiodsplog_destroy_stream_buffer(void)
{
    if(priv_data.stream_buffer_mem){
        kfree(priv_data.stream_buffer_mem);
        priv_data.stream_buffer_mem = NULL;
    }

    DSP_WD(DSP_LOG_START_ADDR, ARM_2_ARC_ADDR_SWAP(0));
    DSP_WD(DSP_LOG_END_ADDR,ARM_2_ARC_ADDR_SWAP(0));
    DSP_WD(DSP_LOG_RD_ADDR,ARM_2_ARC_ADDR_SWAP(0));
    DSP_WD(DSP_LOG_WD_ADDR,ARM_2_ARC_ADDR_SWAP(0));
    return 0;
}

module_init(audiodsplog_init_module);
module_exit(audiodsplog_exit_module);

