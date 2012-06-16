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
#include <linux/ppmgr/ppmgr.h>
#include <linux/ppmgr/ppmgr_status.h>
#include <linux/platform_device.h>
#include <linux/ge2d/ge2d_main.h>
#include <linux/ge2d/ge2d.h>
#include <linux/amlog.h>
#include <linux/ctype.h>
#include <linux/vout/vout_notify.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>



#include "ppmgr_log.h"
#include "ppmgr_pri.h"
#include "ppmgr_dev.h"

/***********************************************************************
*
* global status.
*
************************************************************************/
static int ppmgr_enable_flag=0;
static int property_change = 0;
static int buff_change = 0;
ppmgr_device_t  ppmgr_device;

int get_bypass_mode(void)
{
    return ppmgr_device.bypass;
}

int get_property_change(void)
{
    return property_change;	
}
void set_property_change(int flag)
{
    property_change = flag;	
}

int get_buff_change(void)
{
    return buff_change;	
}
void set_buff_change(int flag)
{
    buff_change = flag;	
}

int get_ppmgr_status(void) {
    return ppmgr_enable_flag;
}

void set_ppmgr_status(int flag) {
    if(flag) ppmgr_enable_flag=1;
    else ppmgr_enable_flag=0;
}

/***********************************************************************
*
* Utilities.
*
************************************************************************/
static ssize_t _ppmgr_angle_write(unsigned long val)
{
    unsigned long angle = val;

    if(angle>3) {
        if(angle==90) angle=1;
        else if(angle==180) angle=2;
        else if(angle==270) angle=3;
        else {
            printk("invalid orientation value\n");
            printk("you should set 0 or 0 for 0 clock wise,");
            printk("1 or 90 for 90 clockwise,2 or 180 for 180 clockwise");
            printk("3 or 270 for 270 clockwise\n");
            return -EINVAL;
        }
    }

    if(angle != ppmgr_device.angle ){		
        property_change = 1;
    }

    ppmgr_device.angle = angle;
    ppmgr_device.videoangle = (ppmgr_device.angle+ ppmgr_device.orientation)%4;
    printk("angle:%d,orientation:%d,videoangle:%d \n",ppmgr_device.angle ,
        ppmgr_device.orientation, ppmgr_device.videoangle);

    return 0;
}

/***********************************************************************
*
* class property info.
*
************************************************************************/

#define    	PPMGR_CLASS_NAME   				"ppmgr"
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

static ssize_t show_ppmgr_info(struct class *cla,struct class_attribute *attr,char *buf)
{
    char *bstart;
    unsigned int bsize;
    get_ppmgr_buf_info(&bstart,&bsize);
    return snprintf(buf,80,"buffer:\n start:%x.\tsize:%d\n",(unsigned int)bstart,bsize/(1024*1024));
}

static ssize_t angle_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    return snprintf(buf,80,"current angel is %d\n",ppmgr_device.angle);
}

static ssize_t angle_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    ssize_t size;
    char *endp;
    unsigned long angle  =  simple_strtoul(buf, &endp, 0);
    printk("==%ld==\n",angle);

    if (_ppmgr_angle_write(angle) < 0) {
        return -EINVAL;
    }
/*	
    if(angle != ppmgr_device.angle ){		
        property_change = 1;
    }
    ppmgr_device.angle = angle;
    ppmgr_device.videoangle = (ppmgr_device.angle+ ppmgr_device.orientation)%4;
    printk("angle:%d,orientation:%d,videoangle:%d \n",ppmgr_device.angle ,
    ppmgr_device.orientation, ppmgr_device.videoangle);
*/
    size = endp - buf;
    return count;
}

static ssize_t orientation_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    //ppmgr_device_t* ppmgr_dev=(ppmgr_device_t*)cla;
    return snprintf(buf,80,"current orientation is %d\n",ppmgr_device.orientation*90);
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
    ppmgr_device.orientation = angle;
    ppmgr_device.videoangle = (ppmgr_device.angle+ ppmgr_device.orientation)%4;
    printk("angle:%d,orientation:%d,videoangle:%d \n",ppmgr_device.angle ,
        ppmgr_device.orientation, ppmgr_device.videoangle);
    size = endp - buf;
    return count;
}

static ssize_t bypass_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    //ppmgr_device_t* ppmgr_dev=(ppmgr_device_t*)cla;
    return snprintf(buf,80,"current bypass is %d\n",ppmgr_device.bypass);
}

static ssize_t bypass_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    ssize_t size;
    char *endp;

    ppmgr_device.bypass = simple_strtoul(buf, &endp, 0);
    size = endp - buf;
    return count;
}


static ssize_t rect_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    return snprintf(buf,80,"rotate rect:\nl:%d,t:%d,w:%d,h:%d\n",
			ppmgr_device.left,ppmgr_device.top,ppmgr_device.width,ppmgr_device.height);
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
	
    if(value_array[0]>=0) ppmgr_device.left= value_array[0];
    if(value_array[1]>=0) ppmgr_device.left= value_array[1];
    if(value_array[2]>0) ppmgr_device.left= value_array[2];
    if(value_array[3]>0) ppmgr_device.left= value_array[3];

    return count;
}

static ssize_t disp_read(struct class *cla,struct class_attribute *attr,char *buf)
{	
    return snprintf(buf,80,"disp width is %d ; disp height is %d \n",ppmgr_device.disp_width, ppmgr_device.disp_height);
}
static void set_disp_para(const char *para)
{
    int parsed[2];

    if (likely(parse_para(para, 2, parsed) == 2)) {
        int w, h;
        w = parsed[0] ;
        h = parsed[1];
        if((ppmgr_device.disp_width != w)||(ppmgr_device.disp_height != h))
            buff_change = 1;
        ppmgr_device.disp_width = w ;
        ppmgr_device.disp_height =  h ;
    }
}

static ssize_t disp_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    set_disp_para(buf);
    return 0;
}

#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
extern int video_scaler_notify(int flag);
extern void amvideo_set_scaler_para(int x, int y, int w, int h,int flag);

static ssize_t ppscaler_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    return snprintf(buf,80,"current ppscaler mode is %s\n",(ppmgr_device.ppscaler_flag)?"enabled":"disabled");
}

static ssize_t ppscaler_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    ssize_t size;
    char *endp;
    int flag = simple_strtoul(buf, &endp, 0);
    if((flag<2)&&(flag != ppmgr_device.ppscaler_flag)){
        if(flag)
            video_scaler_notify(1);
        else
            video_scaler_notify(0);
        ppmgr_device.ppscaler_flag = flag;
    }
    size = endp - buf;
    return count;
}


static void set_ppscaler_para(const char *para)
{
    int parsed[5];

    if (likely(parse_para(para, 5, parsed) == 5)) {
        ppmgr_device.scale_h_start = parsed[0];
        ppmgr_device.scale_v_start = parsed[1];
        ppmgr_device.scale_h_end = parsed[2];
        ppmgr_device.scale_v_end = parsed[3];
        amvideo_set_scaler_para(ppmgr_device.scale_h_start,ppmgr_device.scale_v_start,
                                ppmgr_device.scale_h_end-ppmgr_device.scale_h_start+1,
                                ppmgr_device.scale_v_end-ppmgr_device.scale_v_start+1,parsed[4]);
    }
}

static ssize_t ppscaler_rect_read(struct class *cla,struct class_attribute *attr,char *buf)
{
    return snprintf(buf,80,"ppscaler rect:\nx:%d,y:%d,w:%d,h:%d\n",
            ppmgr_device.scale_h_start,ppmgr_device.scale_v_start,
            ppmgr_device.scale_h_end-ppmgr_device.scale_h_start+1,
            ppmgr_device.scale_v_end-ppmgr_device.scale_v_start+1);
}

static ssize_t ppscaler_rect_write(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
    set_ppscaler_para(buf);
    return 0;
}
#endif

extern int  vf_ppmgr_get_states(vframe_states_t *states);

static ssize_t ppmgr_vframe_states_show(struct class *cla, struct class_attribute* attr, char* buf)
{
    int ret = 0;
    vframe_states_t states;

    if (vf_ppmgr_get_states(&states) == 0) {
        ret += sprintf(buf + ret, "vframe_pool_size=%d\n", states.vf_pool_size);
        ret += sprintf(buf + ret, "vframe buf_free_num=%d\n", states.buf_free_num);
        ret += sprintf(buf + ret, "vframe buf_recycle_num=%d\n", states.buf_recycle_num);
        ret += sprintf(buf + ret, "vframe buf_avail_num=%d\n", states.buf_avail_num);

    } else {
        ret += sprintf(buf + ret, "vframe no states\n");
    }

    return ret;
}

static struct class_attribute ppmgr_class_attrs[] = {
    __ATTR(info,
           S_IRUGO | S_IWUSR,
           show_ppmgr_info,
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
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
    __ATTR(ppscaler,
           S_IRUGO | S_IWUSR,
           ppscaler_read,
           ppscaler_write),       
    __ATTR(ppscaler_rect,
           S_IRUGO | S_IWUSR,
           ppscaler_rect_read,
           ppscaler_rect_write),   
#endif
    __ATTR_RO(ppmgr_vframe_states),
    __ATTR_NULL
};

static struct class ppmgr_class = {
    .name = PPMGR_CLASS_NAME,
    .class_attrs = ppmgr_class_attrs,
};

struct class* init_ppmgr_cls() {
    int  ret=0;
    ret = class_register(&ppmgr_class);
    if(ret<0 )
    {
        amlog_level(LOG_LEVEL_HIGH,"error create ppmgr class\r\n");
        return NULL;
    }
    return &ppmgr_class;
}

/***********************************************************************
*
* file op section.
*
************************************************************************/

void set_ppmgr_buf_info(char* start,unsigned int size) {
    ppmgr_device.buffer_start=(char*)start;
    ppmgr_device.buffer_size=size;
}

void get_ppmgr_buf_info(char** start,unsigned int* size) {
    *start=ppmgr_device.buffer_start;
    *size=ppmgr_device.buffer_size;
}

static int ppmgr_open(struct inode *inode, struct file *file) 
{
    ppmgr_device.open_count++;
    return 0;
}

static int ppmgr_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long args)
{
    void  __user* argp =(void __user*)args;
    int ret = 0;
#if 0
    ge2d_context_t *context=(ge2d_context_t *)filp->private_data;
    config_para_t     ge2d_config;	
    ge2d_para_t  para ;
    int flag;    	
    frame_info_t frame_info;
#endif

    switch (cmd)
    {
#if 0
        case PPMGR_IOC_2OSD0:
            break;
        case PPMGR_IOC_ENABLE_PP:
            flag=(int)argp;
            set_ppmgr_status(flag);
            break;
        case PPMGR_IOC_CONFIG_FRAME:
            copy_from_user(&frame_info,argp,sizeof(frame_info_t));
            break;
#endif
        case PPMGR_IOC_GET_ANGLE:
            *((unsigned int *)argp) = ppmgr_device.angle;
            break;
        case PPMGR_IOC_SET_ANGLE:
            ret = _ppmgr_angle_write(args);
            break;
        default :
            return -ENOIOCTLCMD;
		
    }
    return ret;
}

static int ppmgr_release(struct inode *inode, struct file *file)
{
#ifdef CONFIG_ARCH_MESON
    ge2d_context_t *context=(ge2d_context_t *)file->private_data;
	
    if(context && (0==destroy_ge2d_work_queue(context)))
    {
        ppmgr_device.open_count--;
        return 0;
    }
    amlog_level(LOG_LEVEL_LOW,"release one ppmgr device\n");
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

static const struct file_operations ppmgr_fops = {
    .owner   = THIS_MODULE,
    .open    = ppmgr_open,  
    .ioctl   = ppmgr_ioctl,
    .release = ppmgr_release, 	
};

int  init_ppmgr_device(void)
{
    int  ret=0;

    strcpy(ppmgr_device.name,"ppmgr");
    ret=register_chrdev(0,ppmgr_device.name,&ppmgr_fops);
    if(ret <=0) 
    {
        amlog_level(LOG_LEVEL_HIGH,"register ppmgr device error\r\n");
        return  ret ;
    }
    ppmgr_device.major=ret;
    ppmgr_device.dbg_enable=0;
	
    ppmgr_device.angle=0;
    ppmgr_device.videoangle=0;
    ppmgr_device.orientation=0;
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
    ppmgr_device.ppscaler_flag = 0;
    ppmgr_device.scale_h_start = 0;
    ppmgr_device.scale_h_end = 0;
    ppmgr_device.scale_v_start = 0;
    ppmgr_device.scale_v_end = 0;
#endif
	ppmgr_device.video_out=0;
    amlog_level(LOG_LEVEL_LOW,"ppmgr_dev major:%d\r\n",ret);
    
    if((ppmgr_device.cla = init_ppmgr_cls())==NULL) return -1;
    ppmgr_device.dev=device_create(ppmgr_device.cla,NULL,MKDEV(ppmgr_device.major,0),NULL,ppmgr_device.name);
    if (IS_ERR(ppmgr_device.dev)) {
        amlog_level(LOG_LEVEL_HIGH,"create ppmgr device error\n");
        goto unregister_dev;
    }
    buff_change = 0;
    ppmgr_register();    
    if(ppmgr_buffer_init()<0) goto unregister_dev;
    //if(start_vpp_task()<0) return -1;
    
    return 0;
	
unregister_dev:
    class_unregister(ppmgr_device.cla);
    return -1;
}

int uninit_ppmgr_device(void)
{
    stop_ppmgr_task();
    
    if(ppmgr_device.cla)
    {
        if(ppmgr_device.dev)
            device_destroy(ppmgr_device.cla, MKDEV(ppmgr_device.major, 0));
        class_unregister(ppmgr_device.cla);
    }
    
    unregister_chrdev(ppmgr_device.major, ppmgr_device.name);
    return  0;
}

/*******************************************************************
 * 
 * interface for Linux driver
 * 
 * ******************************************************************/

MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0xff, LOG_LEVEL_DESC, LOG_MASK_DESC);

static struct platform_device *ppmgr_dev0 = NULL;

/* for driver. */
static int ppmgr_driver_probe(struct platform_device *pdev)
{
    char* buf_start;
    unsigned int buf_size;
    struct resource *mem;

    if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0)))
    {
        amlog_level(LOG_LEVEL_HIGH, "ppmgr memory resource undefined.\n");
        return -EFAULT;
    }

    buf_start = (char *)mem->start;
    buf_size = mem->end - mem->start + 1;
    set_ppmgr_buf_info((char *)mem->start,buf_size);
    init_ppmgr_device();
    return 0;
}

static int ppmgr_drv_remove(struct platform_device *plat_dev)
{
    //struct rtc_device *rtc = platform_get_drvdata(plat_dev);
    //rtc_device_unregister(rtc);
    //device_remove_file(&plat_dev->dev, &dev_attr_irq);
    uninit_ppmgr_device();
    return 0;
}

/* general interface for a linux driver .*/
struct platform_driver ppmgr_drv = {
    .probe  = ppmgr_driver_probe,
    .remove = ppmgr_drv_remove,
    .driver = {
        .name = "ppmgr",
        .owner = THIS_MODULE,
    }
};

static int __init
ppmgr_init_module(void)
{
    int err;

    amlog_level(LOG_LEVEL_HIGH,"ppmgr_init\n");
    if ((err = platform_driver_register(&ppmgr_drv))) {
        return err;
    }

    return err;
	
}

static void __exit
ppmgr_remove_module(void)
{
    platform_device_put(ppmgr_dev0);
    platform_driver_unregister(&ppmgr_drv);
    amlog_level(LOG_LEVEL_HIGH,"ppmgr module removed.\n");
}

module_init(ppmgr_init_module);
module_exit(ppmgr_remove_module);

MODULE_DESCRIPTION("AMLOGIC  ppmgr driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("aml-sh <kasin.li@amlogic.com>");


