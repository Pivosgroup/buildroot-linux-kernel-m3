/*******************************************************************
 *
 *  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
 *
 *  Description:
 *
 *  Author: Amlogic Software
 *  Created: 2010/4/1   19:46
 *
 *******************************************************************/
#include <linux/freescale/freescale.h>
#include <linux/freescale/freescale_status.h>
#include <linux/platform_device.h>
#include <linux/ge2d/ge2d_main.h>
#include <linux/ge2d/ge2d.h>
#include <linux/amlog.h>
#include <linux/ctype.h>
#include <linux/vout/vout_notify.h>

#include "freescale_log.h"
#include "freescale_pri.h"
#include "freescale_dev.h"

/***********************************************************************
*
* global status.
*
************************************************************************/
static int freescale_enable_flag=0;
static int property_change = 0;
freescale_device_t  freescale_device;

int fsl_get_bypass_mode(void)
{
    return freescale_device.bypass;
}

int fsl_get_property_change(void)
{
    return 0; //property_change;	
}
void fsl_set_property_change(int flag)
{
    property_change = flag;	
}

int get_freescale_status(void) {
    return freescale_enable_flag;
}

void set_freescale_status(int flag) {
    if(flag) freescale_enable_flag=1;
    else freescale_enable_flag=0;
}

/***********************************************************************
*
* class property info.
*
************************************************************************/

#define    	FREESCALE_CLASS_NAME   				"freescale"
static int parse_para(const char *para, int para_num, int *result)
{
    char *endp;
    const char *startp = para;
    int *out = result;
    int len = 0, count = 0;

    if (!startp) {
        return 0;
    }

    len = strlen(startp);

    do {
        //filter space out
        while (startp && (isspace(*startp) || !isgraph(*startp)) && len) {
            startp++;
            len--;
        }

        if (len == 0) {
            break;
        }

        *out++ = simple_strtol(startp, &endp, 0);

        len -= endp - startp;
        startp = endp;
        count++;

    } while ((endp) && (count < para_num) && (len > 0));

    return count;
}

static ssize_t show_freescale_info(struct class *cla,struct class_attribute *attr,char *buf)
{
    char *bstart;
    unsigned int bsize;
    get_freescale_buf_info(&bstart,&bsize);
    return snprintf(buf,80,"buffer:\n start:%x.\tsize:%d\n",(unsigned int)bstart,bsize/(1024*1024));
}

static ssize_t angle_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    return snprintf(buf,80,"current angel is %d\n",freescale_device.angle);
}

static ssize_t angle_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    ssize_t size;
    char *endp;
    int angle  =  simple_strtoul(buf, &endp, 0);
    //printk("==%d==\n",angle);
    if(angle>3) {
        if(angle==90) angle=1;
        else if(angle==180) angle=2;
        else if(angle==270) angle=3;
        else {
            printk("invalid angle value\n");
            printk("you should set 0 or 0 for 0 clock wise,");
            printk("1 or 90 for 90 clockwise,2 or 180 for 180 clockwise");
            printk("3 or 270 for 270 clockwise\n");
            return -EINVAL;
        }
    }
	
    if(angle != freescale_device.angle ){		
        property_change = 1;
    }
    freescale_device.angle = angle;
    freescale_device.videoangle = (freescale_device.angle+ freescale_device.orientation)%4;
    /*printk("angle:%d,orientation:%d,videoangle:%d \n",freescale_device.angle ,
    freescale_device.orientation, freescale_device.videoangle);*/
    size = endp - buf;
    return count;
}

static ssize_t orientation_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    //freescale_device_t* freescale_dev=(freescale_device_t*)cla;
    return snprintf(buf,80,"current orientation is %d\n",freescale_device.orientation*90);
}

/* set the initial orientation for video, it should be set before video start. */
static ssize_t orientation_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    ssize_t ret = -EINVAL, size;
    char *endp;
    unsigned angle  =  simple_strtoul(buf, &endp, 0);
    //if(property_change) return ret;
    if(angle>3) {
        if(angle==90) angle=1;
        else if(angle==180) angle=2;
        else if(angle==270) angle=3;
        else {
            printk("invalid orientation value\n");
            printk("you should set 0 or 0 for 0 clock wise,");
            printk("1 or 90 for 90 clockwise,2 or 180 for 180 clockwise");
            printk("3 or 270 for 270 clockwise\n");
            return ret;
        }
    }
    freescale_device.orientation = angle;
    freescale_device.videoangle = (freescale_device.angle+ freescale_device.orientation)%4;
    printk("angle:%d,orientation:%d,videoangle:%d \n",freescale_device.angle ,
        freescale_device.orientation, freescale_device.videoangle);
    size = endp - buf;
    return count;
}

static ssize_t bypass_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    //freescale_device_t* freescale_dev=(freescale_device_t*)cla;
    return snprintf(buf,80,"current bypass is %d\n",freescale_device.bypass);
}

static ssize_t bypass_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    ssize_t size;
    char *endp;

    freescale_device.bypass = simple_strtoul(buf, &endp, 0);
    size = endp - buf;
    return count;
}


static ssize_t rect_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    return snprintf(buf,80,"rotate rect:\nl:%d,t:%d,w:%d,h:%d\n",
			freescale_device.left,freescale_device.top,freescale_device.width,freescale_device.height);
}

static ssize_t rect_write(struct class *cla,struct class_attribute *attr,const char *buf, size_t count)
{
    char* errstr="data error,access string is \"left,top,width,height\"\n";
    char* strp=(char*)buf;
    char* endp;
    int value_array[4];
    static int buflen;
    static char* tokenlen;
    int i;
    buflen=strlen(buf);
    value_array[0]=value_array[1]=value_array[2]=value_array[3]= -1;
	
    for(i=0;i<4;i++) {
        if(buflen==0) {
            printk(errstr);
            return  -EINVAL;
        }
        tokenlen=strnchr(strp,buflen,',');
        if(tokenlen!=NULL) *tokenlen='\0';
        value_array[i]= simple_strtoul(strp,&endp,0);
        if((endp-strp)>(tokenlen-strp)) break;
        if(tokenlen!=NULL)  {
            *tokenlen=',';
            strp= tokenlen+1;
            buflen=strlen(strp);
        }  else 
            break;
    }
	
    if(value_array[0]>=0) freescale_device.left= value_array[0];
    if(value_array[1]>=0) freescale_device.left= value_array[1];
    if(value_array[2]>0) freescale_device.left= value_array[2];
    if(value_array[3]>0) freescale_device.left= value_array[3];

    return count;
}

static ssize_t disp_read(struct class *cla,struct class_attribute *attr,char *buf)
{	
    return snprintf(buf,80,"disp width is %d ; disp height is %d \n",freescale_device.disp_width, freescale_device.disp_height);
}
static void set_disp_para(const char *para)
{
    int parsed[2];

    if (likely(parse_para(para, 2, parsed) == 2)) {
        int w, h;
        w = parsed[0] ;
        h = parsed[1];
        freescale_device.disp_width = w ;
        freescale_device.disp_height =  h ;
    }
}

static ssize_t disp_write(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
    set_disp_para(buf);
    return 0;
}

#ifdef CONFIG_MIX_FREE_SCALE
extern int video_scaler_notify(int flag);
extern void amvideo_set_scaler_para(int x, int y, int w, int h,int flag);

static ssize_t ppscaler_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    return snprintf(buf,80,"current ppscaler mode is %s\n",(freescale_device.ppscaler_flag)?"enabled":"disabled");
}

static ssize_t ppscaler_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    ssize_t size;
    char *endp;
    int flag = simple_strtoul(buf, &endp, 0);
    if((flag<2)&&(flag != freescale_device.ppscaler_flag)){
        if(flag)
            video_scaler_notify(1);
        else
            video_scaler_notify(0);
        freescale_device.ppscaler_flag = flag;
    }
    size = endp - buf;
    return count;
}


static void set_ppscaler_para(const char *para)
{
    int parsed[5];

    if (likely(parse_para(para, 5, parsed) == 5)) {
        freescale_device.scale_h_start = parsed[0];
        freescale_device.scale_v_start = parsed[1];
        freescale_device.scale_h_end = parsed[2];
        freescale_device.scale_v_end = parsed[3];
        amvideo_set_scaler_para(freescale_device.scale_h_start,freescale_device.scale_v_start,
                                freescale_device.scale_h_end-freescale_device.scale_h_start+1,
                                freescale_device.scale_v_end-freescale_device.scale_v_start+1,parsed[4]);
    }
}

static ssize_t ppscaler_rect_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    return snprintf(buf,80,"ppscaler rect:\nx:%d,y:%d,w:%d,h:%d\n",
            freescale_device.scale_h_start,freescale_device.scale_v_start,
            freescale_device.scale_h_end-freescale_device.scale_h_start+1,
            freescale_device.scale_v_end-freescale_device.scale_v_start+1);
}

static ssize_t ppscaler_rect_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    set_ppscaler_para(buf);
    return 0;
}
#endif

static struct class_attribute freescale_class_attrs[] = {
    __ATTR(info,
           S_IRUGO | S_IWUSR,
           show_freescale_info,
           NULL),
    __ATTR(angle,
           S_IRUGO | S_IWUSR,
           angle_read,
           angle_write),
    __ATTR(rect,
           S_IRUGO | S_IWUSR,
           rect_read,
           rect_write),
    __ATTR(bypass,
           S_IRUGO | S_IWUSR,
           bypass_read,
           bypass_write),   
           
    __ATTR(disp,
           S_IRUGO | S_IWUSR,
           disp_read,
           disp_write),          

    __ATTR(orientation,
           S_IRUGO | S_IWUSR,
           orientation_read,
           orientation_write),           
#ifdef CONFIG_MIX_FREE_SCALE
    __ATTR(ppscaler,
           S_IRUGO | S_IWUSR,
           ppscaler_read,
           ppscaler_write),       
    __ATTR(ppscaler_rect,
           S_IRUGO | S_IWUSR,
           ppscaler_rect_read,
           ppscaler_rect_write),   
#endif
    __ATTR_NULL
};

static struct class freescale_class = {
    .name = FREESCALE_CLASS_NAME,
    .class_attrs = freescale_class_attrs,
};

struct class* init_freescale_cls() {
    int  ret=0;
    ret = class_register(&freescale_class);
    if(ret<0 )
    {
       // amlog_level(LOG_LEVEL_HIGH,"error create freescale class\r\n");
         printk("error create freescale class\n");
        return NULL;
    }    
    return &freescale_class;
}

/***********************************************************************
*
* file op section.
*
************************************************************************/

void set_freescale_buf_info(char* start,unsigned int size) {
    freescale_device.buffer_start=(char*)start;
    freescale_device.buffer_size=(char*)size;
}

void get_freescale_buf_info(char** start,unsigned int* size) {
    *start=freescale_device.buffer_start;
    *size=freescale_device.buffer_size;
}

static int freescale_open(struct inode *inode, struct file *file) 
{
    freescale_device.open_count++;
    return 0;
}

/*static int freescale_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long args)
{

    ge2d_context_t *context=(ge2d_context_t *)filp->private_data;
    void  __user* argp =(void __user*)args;
    config_para_t     ge2d_config;	
    ge2d_para_t  para ;
    int  ret=0,flag;    	
    frame_info_t frame_info;

    switch (cmd)
    {
        case FREESCALE_IOC_2OSD0:
            break;
        case FREESCALE_IOC_ENABLE_PP:
            flag=(int)argp;
            set_freescale_status(flag);
            break;
        case FREESCALE_IOC_CONFIG_FRAME:
            copy_from_user(&frame_info,argp,sizeof(frame_info_t));
            break;
        default :
            return -ENOIOCTLCMD;
		
    }
    return ret;
}
*/

static int freescale_release(struct inode *inode, struct file *file)
{
#ifdef CONFIG_ARCH_MESON
    ge2d_context_t *context=(ge2d_context_t *)file->private_data;
	
    if(context && (0==destroy_ge2d_work_queue(context)))
    {
        freescale_device.open_count--;
        return 0;
    }
    amlog_level(LOG_LEVEL_LOW,"release one freescale device\n");
    return -1;
#else
    return 0;
#endif
}

/***********************************************************************
*
* file op initintg section.
*
************************************************************************/

static const struct file_operations freescale_fops = {
    .owner   = THIS_MODULE,
    .open    = freescale_open,  
    //.ioctl = freescale_ioctl,
    .release = freescale_release, 	
};

int  init_freescale_device(void)
{
    int  ret=0;

    strcpy(freescale_device.name,"freescale");
    ret=register_chrdev(0,freescale_device.name,&freescale_fops);
    if(ret <=0) 
    {
        amlog_level(LOG_LEVEL_HIGH,"register freescale device error\r\n");
        return  ret ;
    }
    freescale_device.major=ret;
    freescale_device.dbg_enable=0;
	
    freescale_device.angle=2;
    freescale_device.videoangle=2;
    freescale_device.orientation=0;
#ifdef CONFIG_MIX_FREE_SCALE
    freescale_device.ppscaler_flag = 0;
    freescale_device.scale_h_start = 0;
    freescale_device.scale_h_end = 0;
    freescale_device.scale_v_start = 0;
    freescale_device.scale_v_end = 0;
#endif
    amlog_level(LOG_LEVEL_LOW,"freescale_dev major:%d\r\n",ret);
    
    if((freescale_device.cla = init_freescale_cls())==NULL) return -1;
    freescale_device.dev=device_create(freescale_device.cla,NULL,MKDEV(freescale_device.major,0),NULL,freescale_device.name);
    if (IS_ERR(freescale_device.dev)) {
        amlog_level(LOG_LEVEL_HIGH,"create freescale device error\n");
        printk("create freescale device error\n");
        goto unregister_dev;
    }
    freescale_register();
    if(freescale_buffer_init()<0) 
    {
    		printk("freescale_buffer_init fail\n");
    		goto unregister_dev;
	}
    //if(start_vpp_task()<0) return -1;
    
    return 0;
	
unregister_dev:
	printk("case 1 :freescale class_unregister\n");
    class_unregister(freescale_device.cla);
    return -1;
}

int uninit_freescale_device(void)
{
    stop_freescale_task();
    
    if(freescale_device.cla)
    {
        if(freescale_device.dev)
            device_destroy(freescale_device.cla, MKDEV(freescale_device.major, 0));
            printk("case 0 : freescale class_unregister\n");
        class_unregister(freescale_device.cla);
    }
    
    unregister_chrdev(freescale_device.major, freescale_device.name);
    return  0;
}

/*******************************************************************
 * 
 * interface for Linux driver
 * 
 * ******************************************************************/

MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0xff, LOG_LEVEL_DESC, LOG_MASK_DESC);

static struct platform_device *freescale_dev0 = NULL;

/* for driver. */
static int freescale_driver_probe(struct platform_device *pdev)
{
    char* buf_start;
    unsigned int buf_size;
    struct resource *mem;

    if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0)))
    {
        amlog_level(LOG_LEVEL_HIGH, "freescale memory resource undefined.\n");
        return -EFAULT;
    }

    buf_start = mem->start;
    buf_size = mem->end - mem->start + 1;
    set_freescale_buf_info(mem->start,buf_size);
    init_freescale_device();
    return 0;
}

static int freescale_drv_remove(struct platform_device *plat_dev)
{
    //struct rtc_device *rtc = platform_get_drvdata(plat_dev);
    //rtc_device_unregister(rtc);
    //device_remove_file(&plat_dev->dev, &dev_attr_irq);
    uninit_freescale_device();
    return 0;
}

/* general interface for a linux driver .*/
struct platform_driver freescale_drv = {
    .probe  = freescale_driver_probe,
    .remove = freescale_drv_remove,
    .driver = {
        .name = "freescale",
        .owner = THIS_MODULE,
    }
};

static int __init
freescale_init_module(void)
{
    int err;

    //amlog_level(LOG_LEVEL_HIGH,"freescale_init\n");
    printk("freescale_init\n");
    if ((err = platform_driver_register(&freescale_drv))) {
        return err;
    }

    return err;
	
}

static void __exit
freescale_remove_module(void)
{
    platform_device_put(freescale_dev0);
    platform_driver_unregister(&freescale_drv);
    amlog_level(LOG_LEVEL_HIGH,"freescale module removed.\n");
}

module_init(freescale_init_module);
module_exit(freescale_remove_module);

MODULE_DESCRIPTION("AMLOGIC  freescale driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("aml-sh <kasin.li@amlogic.com>");


