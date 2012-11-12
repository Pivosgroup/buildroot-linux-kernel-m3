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
#define PPMGR_IOC_VIEW_MODE  _IOW(PPMGR_IOC_MAGIC,0X03,unsigned int)

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
    int format;
} frame_info_t;


#define MODE_3D_ENABLE      0x00000001
#define MODE_AUTO           0x00000002
#define MODE_2D_TO_3D       0x00000004
#define MODE_LR             0x00000008
#define MODE_BT             0x00000010
#define MODE_LR_SWITCH      0x00000020
#define MODE_FIELD_DEPTH    0x00000040
#define MODE_3D_TO_2D_L     0x00000080
#define MODE_3D_TO_2D_R     0x00000100

#define LR_FORMAT_INDICATOR   0x00000200
#define BT_FORMAT_INDICATOR   0x00000400


#define TYPE_NONE           0
#define TYPE_2D_TO_3D       1
#define TYPE_LR             2
#define TYPE_BT             3
#define TYPE_LR_SWITCH      4
#define TYPE_FILED_DEPTH    5
#define TYPE_3D_TO_2D_L     6
#define TYPE_3D_TO_2D_R     7


typedef enum {
    VIEWMODE_NULL  = 0,
    VIEWMODE_4_3,
    VIEWMODE_16_9
} view_mode_t;
                           
#endif
