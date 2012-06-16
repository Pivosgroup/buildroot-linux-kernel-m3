#ifndef  _PPMGR_MAIN_H
#define  _PPMGR_MAIN_H
#include <linux/interrupt.h>
#include <mach/am_regs.h>
#include <linux/amports/canvas.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/sysfs.h>
#include  <linux/spinlock.h>
#include <linux/kthread.h>


/**************************************************************
**																	 **
**	macro define		 												 **
**																	 **
***************************************************************/

#define PPMGR_IOC_MAGIC  'P'
#define PPMGR_IOC_2OSD0		_IOW(PPMGR_IOC_MAGIC, 0x00, unsigned int)
#define PPMGR_IOC_ENABLE_PP _IOW(PPMGR_IOC_MAGIC,0X01,unsigned int)
#define PPMGR_IOC_CONFIG_FRAME  _IOW(PPMGR_IOC_MAGIC,0X02,unsigned int)
#define PPMGR_IOC_GET_ANGLE  _IOR(PPMGR_IOC_MAGIC,0X03,unsigned int)
#define PPMGR_IOC_SET_ANGLE  _IOW(PPMGR_IOC_MAGIC,0X04,unsigned int)

/**************************************************************
**																	 **
**	type  define		 												 **
**																	 **
***************************************************************/

typedef struct {
    int width;
    int height;
    int bpp;
    int angle;
} frame_info_t;
#endif
