/*
 *OV3640 - This code emulates a real video device with v4l2 api
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence, GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <media/videobuf-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include <linux/i2c.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-i2c-drv.h>
#include <media/amlogic/aml_camera.h>

#include <mach/am_regs.h>
//#include <mach/am_eth_pinmux.h>
#include <mach/pinmux.h>
#include <linux/tvin/tvin.h>
#include "common/plat_ctrl.h"
#include "ov3640_firmware.h"
#include "common/vmapi.h"

#define OV3640_CAMERA_MODULE_NAME "ov3640"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define OV3640_CAMERA_MAJOR_VERSION 0
#define OV3640_CAMERA_MINOR_VERSION 7
#define OV3640_CAMERA_RELEASE 0
#define OV3640_CAMERA_VERSION \
	KERNEL_VERSION(OV3640_CAMERA_MAJOR_VERSION, OV3640_CAMERA_MINOR_VERSION, OV3640_CAMERA_RELEASE)

MODULE_DESCRIPTION("ov3640 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");


//extern int disable_ov3640;
static int ov3640_have_opened = 0;
static struct i2c_client *this_client;

static void do_download(struct work_struct *work);
static DECLARE_DELAYED_WORK(dl_work, do_download);

typedef enum camera_focus_mode_e {
    CAM_FOCUS_MODE_RELEASE = 0,
    CAM_FOCUS_MODE_FIXED,
    CAM_FOCUS_MODE_INFINITY,
    CAM_FOCUS_MODE_AUTO,
    CAM_FOCUS_MODE_MACRO,
    CAM_FOCUS_MODE_EDOF,
    CAM_FOCUS_MODE_CONTI_VID,
    CAM_FOCUS_MODE_CONTI_PIC,
}camera_focus_mode_t;

/* supported controls */
static struct v4l2_queryctrl ov3640_qctrl[] = {
    {
        .id            = V4L2_CID_BRIGHTNESS,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "Brightness",
        .minimum       = 0,
        .maximum       = 255,
        .step          = 1,
        .default_value = 127,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    }, {
        .id            = V4L2_CID_CONTRAST,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "Contrast",
        .minimum       = 0x10,
        .maximum       = 0x60,
        .step          = 0xa,
        .default_value = 0x30,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    },/* {
        .id            = V4L2_CID_SATURATION,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "Saturation",
        .minimum       = 0x28,
        .maximum       = 0x60,
        .step          = 0x8,
        .default_value = 0x48,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    }, */{
        .id            = V4L2_CID_HFLIP,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "flip on horizontal",
        .minimum       = 0,
        .maximum       = 1,
        .step          = 0x1,
        .default_value = 0,
        .flags         = V4L2_CTRL_FLAG_DISABLED,
    } ,{
        .id            = V4L2_CID_VFLIP,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "flip on vertical",
        .minimum       = 0,
        .maximum       = 1,
        .step          = 0x1,
        .default_value = 0,
        .flags         = V4L2_CTRL_FLAG_DISABLED,
    },{
        .id            = V4L2_CID_DO_WHITE_BALANCE,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "white balance",
        .minimum       = 0,
        .maximum       = 6,
        .step          = 0x1,
        .default_value = 0,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    },{
        .id            = V4L2_CID_EXPOSURE,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "exposure",
        .minimum       = 0,
        .maximum       = 8,
        .step          = 0x1,
        .default_value = 4,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    },{
        .id            = V4L2_CID_COLORFX,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "effect",
        .minimum       = 0,
        .maximum       = 6,
        .step          = 0x1,
        .default_value = 0,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    },{
        .id            = V4L2_CID_WHITENESS,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "banding",
        .minimum       = 0,
        .maximum       = 1,
        .step          = 0x1,
        .default_value = 0,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    },{
        .id            = V4L2_CID_FOCUS_AUTO,
        .type          = V4L2_CTRL_TYPE_MENU,
        .name          = "auto focus",
        .minimum       = CAM_FOCUS_MODE_RELEASE,
        .maximum       = CAM_FOCUS_MODE_CONTI_PIC,
        .step          = 0x1,
        .default_value = CAM_FOCUS_MODE_CONTI_PIC,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    }
};

struct v4l2_querymenu ov3640_qmenu_autofocus[] = {
    {
        .id         = V4L2_CID_FOCUS_AUTO,
        .index      = CAM_FOCUS_MODE_INFINITY,
        .name       = "infinity",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_FOCUS_AUTO,
        .index      = CAM_FOCUS_MODE_AUTO,
        .name       = "auto",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_FOCUS_AUTO,
        .index      = CAM_FOCUS_MODE_CONTI_VID,
        .name       = "continuous-video",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_FOCUS_AUTO,
        .index      = CAM_FOCUS_MODE_CONTI_PIC,
        .name       = "continuous-picture",
        .reserved   = 0,
    }
};

typedef struct {
    __u32   id;
    int     num;
    struct v4l2_querymenu* ov3640_qmenu;
}ov3640_qmenu_set_t;

ov3640_qmenu_set_t ov3640_qmenu_set[] = {
    {
        .id             = V4L2_CID_FOCUS_AUTO,
        .num            = ARRAY_SIZE(ov3640_qmenu_autofocus),
        .ov3640_qmenu   = ov3640_qmenu_autofocus,
    }
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct ov3640_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct ov3640_fmt formats[] = {
    {
        .name     = "RGB565 (BE)",
        .fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
        .depth    = 16,
    }, {
        .name     = "RGB888 (24)",
        .fourcc   = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
        .depth    = 24,
    }, {
        .name     = "BGR888 (24)",
        .fourcc   = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
        .depth    = 24,
    }, {
        .name     = "12  Y/CbCr 4:2:0SP",
        .fourcc   = V4L2_PIX_FMT_NV12,
        .depth    = 12,    
    }, {
        .name     = "12  Y/CbCr 4:2:0SP",
        .fourcc   = V4L2_PIX_FMT_NV21,
        .depth    = 12,    
    }, {
        .name     = "YUV420P",
        .fourcc   = V4L2_PIX_FMT_YUV420,
        .depth    = 12,
    }
};

static struct ov3640_fmt *get_format(struct v4l2_format *f)
{
	struct ov3640_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
    	fmt = &formats[k];
    	if (fmt->fourcc == f->fmt.pix.pixelformat)
        	break;
    }

	if (k == ARRAY_SIZE(formats))
    	return NULL;

	return &formats[k];
}

struct sg_to_addr {
	int pos;
	struct scatterlist *sg;
};

/* buffer for one video frame */
struct ov3640_buffer {
    /* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct ov3640_fmt        *fmt;
};

struct ov3640_dmaqueue {
	struct list_head       active;

    /* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
    /* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

typedef enum resulution_size_type{
	SIZE_NULL = 0,
	SIZE_QVGA_320x240,
	SIZE_VGA_640X480,
	SIZE_UXGA_1600X1200,
	SIZE_QXGA_2048X1536,
	SIZE_QSXGA_2560X2048,
} resulution_size_type_t;

typedef struct resolution_param {
	struct v4l2_frmsize_discrete frmsize;
	struct v4l2_frmsize_discrete active_frmsize;
	int active_fps;
	resulution_size_type_t size_type;
	struct aml_camera_i2c_fig_s* reg_script;
} resolution_param_t;

static LIST_HEAD(ov3640_devicelist);

struct ov3640_device {
	struct list_head	    	ov3640_devicelist;
	struct v4l2_subdev	    	sd;
	struct v4l2_device	    	v4l2_dev;

	spinlock_t                 slock;
	struct mutex	        	mutex;

	int                        users;

    /* various device info */
	struct video_device        *vdev;

	struct ov3640_dmaqueue       vidq;

    /* Several counters */
	unsigned long              jiffies;

    /* Input Number */
	int	           input;

    /* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;
    
    /* Control 'registers' */
	int                qctl_regs[ARRAY_SIZE(ov3640_qctrl)];
	
	/* current resolution param for preview and capture */
	resolution_param_t* cur_resolution_param;
	
	/* for down load firmware */
	struct work_struct dl_work;
	
	int firmware_ready;

};

static inline struct ov3640_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov3640_device, sd);
}

struct ov3640_fh {
	struct ov3640_device            *dev;

    /* video capture */
	struct ov3640_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int	           input;     /* Input Number on bars */
	int  stream_on;
};

static inline struct ov3640_fh *to_fh(struct ov3640_device *dev)
{
	return container_of(dev, struct ov3640_fh, dev);
}

/* ------------------------------------------------------------------
	reg spec of OV3640
   ------------------------------------------------------------------*/
static struct aml_camera_i2c_fig_s OV3640_script[] = {
#if 0
	{0x3086, 0x08}, 
	{0x308d, 0x14}, 
	{0x300e, 0x32}, 
	{0x304d, 0x45}, 
                    
	//initial settin
	{0x30a7, 0x5e}, 
	{0x3087, 0x16}, 
	{0x309c, 0x1a}, 
	{0x30a2, 0xe4}, 
	{0x30aa, 0x42}, 
	{0x30a9, 0xb5}, 
	{0x30b0, 0xff}, 
	{0x30b1, 0xff}, 
	{0x30b2, 0x18}, 
	{0x300e, 0x32}, 
	{0x300f, 0x21}, 
	{0x3010, 0x20}, 
	{0x3011, 0x04}, 
	{0x304c, 0x81}, 
	{0x30d7, 0x10}, 
	{0x30d9, 0x0d}, 
	{0x30db, 0x08}, 
	{0x3016, 0x82}, 
	{0x3018, 0x38}, 
	{0x3019, 0x30}, 
	{0x301a, 0x61}, 
	{0x307d, 0x00}, 
	{0x3087, 0x02}, 
	{0x3082, 0x20}, 
	{0x3015, 0x12}, 
	{0x3014, 0x04}, 
	{0x3016, 0x92}, 
	{0x3013, 0xf7}, 
	{0x303c, 0x08}, 
	{0x303d, 0x18}, 
	{0x303e, 0x06}, 
	{0x303f, 0x0c}, 
	{0x3030, 0x62}, 
	{0x3031, 0x26}, 
	{0x3032, 0xe6}, 
	{0x3033, 0x6e}, 
	{0x3034, 0xea}, 
	{0x3035, 0xae}, 
	{0x3036, 0xa6}, 
	{0x3037, 0x6a}, 
	{0x3104, 0x02}, 
	{0x3105, 0xfd}, 
	{0x3106, 0x00}, 
	{0x3107, 0xff}, 
	{0x3301, 0xde}, 
	{0x3302, 0xef}, 
	{0x3312, 0x26}, 
	{0x3314, 0x42}, 
	{0x3313, 0x2b}, 
	{0x3315, 0x42}, 
	{0x3310, 0xd0}, 
	{0x3311, 0xbd}, 
	{0x330c, 0x18}, 
	{0x330d, 0x18}, 
	{0x330e, 0x56}, 
	{0x330f, 0x5c}, 
	{0x330b, 0x1c}, 
	{0x3306, 0x5c}, 
	{0x3307, 0x11}, 
	{0x336a, 0x52}, 
	{0x3370, 0x46}, 
	{0x3376, 0x38}, 
	{0x3300, 0x13}, 
	{0x30b8, 0x20}, 
	{0x30b9, 0x17}, 
	{0x30ba, 0x02}, 
	{0x30bb, 0x08}, 
	{0x3507, 0x06}, 
	{0x350a, 0x4f}, 
	{0x3100, 0x02}, 
	{0x3301, 0xde}, 
	{0x3304, 0x00}, 
	{0x3400, 0x00}, 
	{0x3404, 0x01}, 
	{0x335f, 0x68}, 
	{0x3360, 0x18}, 
	{0x3361, 0x0c}, 
	{0x3362, 0x12}, 
	{0x3363, 0x88}, 
	{0x3364, 0xe4}, 
	{0x3403, 0x42}, 
	{0x3088, 0x02}, 
	{0x3089, 0x80}, 
	{0x308a, 0x01}, 
	{0x308b, 0xe0}, 
	{0x30d7, 0x10}, 
	{0x3302, 0xef}, 
	{0x335f, 0x68}, 
	{0x3360, 0x18}, 
	{0x3361, 0x0c}, 
	{0x3362, 0x12}, 
	{0x3363, 0x88}, 
	{0x3364, 0xe4}, 
	{0x3403, 0x42}, 
	{0x3088, 0x12}, 
	{0x3089, 0x80}, 
	{0x308a, 0x01}, 
	{0x308b, 0xe0}, 
	{0x304c, 0x84}, 
	{0x332a, 0x18}, 
	{0x331b, 0x04}, 
	{0x331c, 0x13}, 
	{0x331d, 0x2b}, 
	{0x331e, 0x53}, 
	{0x331f, 0x66}, 
	{0x3320, 0x73}, 
	{0x3321, 0x80}, 
	{0x3322, 0x8c}, 
	{0x3323, 0x95}, 
	{0x3324, 0x9d}, 
	{0x3325, 0xac}, 
	{0x3326, 0xb8}, 
	{0x3327, 0xcc}, 
	{0x3328, 0xdd}, 
	{0x3329, 0xee}, 
	{0x3300, 0x13}, 
	{0x3367, 0x23}, 
	{0x3368, 0xb5}, 
	{0x3369, 0xc8}, 
	{0x336A, 0x46}, 
	{0x336B, 0x07}, 
	{0x336C, 0x00}, 
	{0x336D, 0x23}, 
	{0x336E, 0xbb}, 
	{0x336F, 0xcc}, 
	{0x3370, 0x49}, 
	{0x3371, 0x07}, 
	{0x3372, 0x00}, 
	{0x3373, 0x23}, 
	{0x3374, 0xab}, 
	{0x3375, 0xcc}, 
	{0x3376, 0x46}, 
	{0x3377, 0x07}, 
	{0x3378, 0x00}, 
	{0x332a, 0x1d}, 
	{0x331b, 0x08}, 
	{0x331c, 0x16}, 
	{0x331d, 0x2d}, 
	{0x331e, 0x54}, 
	{0x331f, 0x66}, 
	{0x3320, 0x73}, 
	{0x3321, 0x80}, 
	{0x3322, 0x8c}, 
	{0x3323, 0x95}, 
	{0x3324, 0x9d}, 
	{0x3325, 0xac}, 
	{0x3326, 0xb8}, 
	{0x3327, 0xcc}, 
	{0x3328, 0xdd}, 
	{0x3329, 0xee}, 
	{0x3317, 0x04}, 
	{0x3316, 0xf8}, 
	{0x3312, 0x31}, 
	{0x3314, 0x57}, 
	{0x3313, 0x28}, 
	{0x3315, 0x3d}, 
	{0x3311, 0xd0}, 
	{0x3310, 0xb6}, 
	{0x330c, 0x16}, 
	{0x330d, 0x16}, 
	{0x330e, 0x5F}, 
	{0x330f, 0x5C}, 
	{0x330b, 0x18}, 
	{0x3306, 0x5c}, 
	{0x3307, 0x11}, 
	{0x3308, 0x25}, 
	{0x3318, 0x62}, 
	{0x3319, 0x62}, 
	{0x331a, 0x62}, 
	{0x3340, 0x20}, 
	{0x3341, 0x58}, 
	{0x3342, 0x08}, 
	{0x3343, 0x21}, 
	{0x3344, 0xbe}, 
	{0x3345, 0xe0}, 
	{0x3346, 0xca}, 
	{0x3347, 0xc6}, 
	{0x3348, 0x04}, 
	{0x3349, 0x98}, 
	{0x333F, 0x06}, 
	{0x332e, 0x04}, 
	{0x332f, 0x05}, 
	{0x3331, 0x03}, 
	{0x302B, 0x6D}, 
	{0x308d, 0x04}, 
	{0x3086, 0x03}, 
	{0x3086, 0x00}, 
	{0x307d, 0x00}, 
	{0x3085, 0x00}, 
	{0x306c, 0x10}, 
	{0x307b, 0x40}, 
	{0x361d, 0x50},
#else
	{0x3012, 0x80},
	{0x304d, 0x45}, 
	{0x3087, 0x16},//clock sel
	{0x30aa, 0x45},
	{0x30b0, 0xff},
	{0x30b1, 0xff},
	{0x30b2, 0x10},

	//24Mhz
	{0x300e, 0x32}, 
	{0x300f, 0x21}, 
	{0x3010, 0x20}, 
	{0x3011, 0x00},
	{0x304c, 0x82},
	{0x30d7, 0x10},
    
	//aec/agc auto setting
	{0x3018, 0x38}, //aec
	{0x3019, 0x30}, //
	{0x301a, 0x61}, //
	{0x307d, 0x00}, //aec isp
	{0x3087, 0x02}, //
	{0x3082, 0x20}, //
	{0x3015, 0x12}, // 8x gain, auto 1/2
	{0x3014, 0x04}, // auto frame off
	{0x3013, 0xf7}, //
    
	//aecweight;06142007
	{0x303c, 0x08}, //
	{0x303d, 0x18}, //
	{0x303e, 0x06}, //
	{0x303F, 0x0c}, //
	{0x3030, 0x62}, //
	{0x3031, 0x26}, //
	{0x3032, 0xe6}, //
	{0x3033, 0x6e}, //
	{0x3034, 0xea}, //
	{0x3035, 0xae}, //
	{0x3036, 0xa6}, //
	{0x3037, 0x6a}, //
    
	//ISP Common 
	{0x3104, 0x02}, //isp system control
	{0x3105, 0xfd}, 
	{0x3106, 0x00}, 
	{0x3107, 0xff}, 
	
	{0x3300, 0x12}, //lens on/off,bit[0]
	{0x3301, 0xde}, //aec gamma- 06142007
    
	//ISP setting
	{0x3302, 0xcf}, //sde, uv_adj, gam, awb
  
	//gamma
	{0x331b, 0x0e},//
	{0x331c, 0x1a},//
	{0x331d, 0x31},//
	{0x331e, 0x5a},//
	{0x331f, 0x69},//
	{0x3320, 0x75},//
	{0x3321, 0x7e},//
	{0x3322, 0x88},//
	{0x3323, 0x8f},//
	{0x3324, 0x96},//
	{0x3325, 0xa3},//
	{0x3326, 0xaf},//
	{0x3327, 0xc4},//
	{0x3328, 0xd7},//
	{0x3329, 0xe8},//
	{0x332a, 0x20},//

	//DNS & EDGE
	{0x332c, 0x06},
	{0x332d, 0x63},
	{0x332e, 0x05},
	{0x332f, 0x05},
	{0x3330, 0x00},
	{0x3331, 0x02},

	//simple AWB
	{0x3316, 0xff},
	{0x3317, 0x00},   
	{0x3308, 0xa5},
	{0x3312, 0x26},
	{0x3314, 0x42},
	{0x3313, 0x2b},
	{0x3315, 0x42},
	{0x3310, 0xd0},
	{0x3311, 0xbd},
	{0x330c, 0x18},
	{0x330d, 0x18},
	{0x330e, 0x56},
	{0x330f, 0x5c},
	{0x330b, 0x1c},
	{0x3306, 0x5c},
	{0x3307, 0x11},

	//Lens correction
	{0x336a, 0x52},//052207
	{0x3370, 0x46},
	{0x3376, 0x38},

	//UV
	{0x30b8, 0x20},
	{0x30b9, 0x17},
	{0x30ba, 0x04},
	{0x30bb, 0x08},

	//Compression
	{0x3507, 0x06},
	{0x350a, 0x4f},

	//Output format
	{0x3100, 0x02},
	{0x3304, 0x00},
	{0x3400, 0x00},
	{0x3404, 0x01},
	{0x3500, 0x00},
	{0x3600, 0xc0},
	{0x3610, 0x40},
	{0x350a, 0x4f},

	//DVP QXGA
	{0x3088, 0x08},
	{0x3089, 0x00},
	{0x308a, 0x06},
	{0x308b, 0x00},

	{0x308d, 0x04},
	{0x3086, 0x03},
	{0x3086, 0x00},
	{0x304c, 0x82},
#endif
		                
	{0xffff, 0xff}
};

struct aml_camera_i2c_fig_s OV3640_preview_script[] = {
	{0x300e, 0x32}, 
#if 0	
    //preview setting
	{0x3013, 0xf7}, 
	{0x302d, 0x00}, 
	{0x302e, 0x00}, 
	{0x3011, 0x00}, 
	{0x304c, 0x84}, 
	{0x3014, 0x8c}, 
	{0x3302, 0xef}, 
	{0x335f, 0x68}, 
	{0x3360, 0x18}, 
	{0x3361, 0x0c}, 
	{0x3362, 0x12}, 
	{0x3363, 0x88}, 
	{0x3364, 0xe4}, 
	{0x3403, 0x42}, 
	{0x3088, 0x02}, 
	{0x3089, 0x80}, 
	{0x308a, 0x01}, 
	{0x308b, 0xe0}, 
	{0x3100, 0x02}, 
	{0x3301, 0xde}, 
	{0x3304, 0x00}, 
	{0x3400, 0x00}, 
	{0x3404, 0x01}, 
	{0x3600, 0xc0}, 
	{0x308d, 0x04}, 
	//{0x3086, 0x03}, 
	//{0x3086, 0x00}, 
#else
		{0x3012, 0x10}, //
	{0x3020, 0x01}, //
	{0x3021, 0x1d}, //horizontal start point 285
	{0x3022, 0x00}, //
	{0x3023, 0x06}, //0a;vertical start point 10
	{0x3024, 0x08}, //
	{0x3025, 0x18}, //horizontal width 2072
	{0x3026, 0x03}, //
	{0x3027, 0x04}, //vertical height 1548
	{0x302a, 0x03}, //
	{0x302b, 0x10}, //

	{0x3075, 0x24}, //
	{0x300d, 0x01}, //
	{0x30d7, 0x80}, //
	{0x3069, 0x40}, //
	{0x303e, 0x00}, //
	{0x303f, 0xc0}, //
	//XGA::VGA 
	{0x3302, 0xef},
	{0x335f, 0x34},
	{0x3360, 0x0c},
	{0x3361, 0x04},
	{0x3362, 0x12}, 
	{0x3363, 0x88}, 
	{0x3364, 0xe4}, 
	{0x3403, 0x42},
	{0x3088, 0x12}, 
	{0x3089, 0x80}, 
	{0x308a, 0x01}, 
	{0x308b, 0xe0}, 
    
	//XGA 15fps
	{0x300e, 0x39},
	{0x3011, 0x00},
	{0x3010, 0x20},
	{0x3014, 0x08},
	{0x302e, 0x00},
	{0x302d, 0x00},

	{0x304c, 0x82},
#endif      
    {0xffff, 0xff}
};

struct aml_camera_i2c_fig_s OV3640_preview_QVGA_script[] = {
	{0x335f, 0x68},
	{0x3360, 0x18},
	{0x3361, 0x0c},
	{0x3362, 0x01},
	{0x3363, 0x48},
	{0x3364, 0xf4},
	{0x3403, 0x42},
	{0x3088, 0x01},
	{0x3089, 0x40},
	{0x308a, 0x00},
	{0x308b, 0xf0},
	
    {0xffff, 0xff}
};

struct aml_camera_i2c_fig_s OV3640_capture_3M_script[] = {
	{0x300e, 0x39},
	{0x300f, 0x21},
	{0x3010, 0x20},
	{0x304c, 0x81},
	{0x3302, 0xef},
	{0x335f, 0x68},
	{0x3360, 0x18},
	{0x3361, 0x0C},
	{0x3362, 0x68},
	{0x3363, 0x08},
	{0x3364, 0x04},
	{0x3403, 0x42},
	{0x3088, 0x08},
	{0x3089, 0x00},
	{0x308a, 0x06},
	{0x308b, 0x00},
	
	{0x3075, 0x44},
	{0x300d, 0x0 },
	{0x30d7, 0x10},
	{0x3069, 0x44},
	{0x303e, 0x06},
	{0x303f, 0x0c},
	{0x3012, 0x0 },
	{0x3020, 0x01}, 
	{0x3021, 0x1d}, 
	{0x3022, 0x00}, 
	{0x3023, 0xa },
	{0x3024, 0x08}, 
	{0x3025, 0x18}, 
	{0x3026, 0x06},
	{0x3027, 0x0c},
	{0x302a, 0x06},
	{0x302b, 0x6d},
	
    {0xffff, 0xff}
};

static resolution_param_t  prev_resolution_array[] = {
	{
		.frmsize			= {640, 480},
		.active_frmsize		= {640, 478},
		.active_fps			= 236,
		.size_type			= SIZE_VGA_640X480,
		.reg_script			= OV3640_preview_script,
	}, {
		.frmsize			= {320, 240},
		.active_frmsize		= {320, 238},
		.active_fps			= 236,
		.size_type			= SIZE_QVGA_320x240,
		.reg_script			= OV3640_preview_QVGA_script,
	},
};

static resolution_param_t  capture_resolution_array[] = {
	{
		.frmsize			= {1600, 1200},
		.active_frmsize		= {2048, 1534},
		.active_fps			= 150,
		.size_type			= SIZE_UXGA_1600X1200,
		.reg_script			= OV3640_capture_3M_script,
	},{
		.frmsize			= {2048, 1536},
		.active_frmsize		= {2048, 1534},
		.active_fps			= 150,
		.size_type			= SIZE_QXGA_2048X1536,
		.reg_script			= OV3640_capture_3M_script,
	},
};

// download firmware by single i2c write
int OV3640_download_firmware(struct ov3640_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    int i=0;
    while(1 && ov3640_have_opened)
    {
        if (OV3640_AF_firmware[i].val==0xff&&OV3640_AF_firmware[i].addr==0xffff) 
        {
        	printk("download firmware success in initial ov3640.\n");
        	break;
        }
        if((i2c_put_byte(client,OV3640_AF_firmware[i].addr, OV3640_AF_firmware[i].val)) < 0)
        {
        	printk("fail in download firmware ov3640. i=%d\n",i);
            break;
        }
    	i++;
    }

    return 0;
}

static void do_download(struct work_struct *work)
{
	struct ov3640_device *dev = container_of(work, struct ov3640_device, dl_work);
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int mcu_on = 0, afc_on = 0;
	int ret;
	if(OV3640_download_firmware(dev) >= 0) {
		msleep(10);
	        while(1) {
			ret = i2c_get_byte(client, 0x3029);
			if (ret == 0x70)
			break;
			else if(ret = -1)
				continue;
			printk("waiting for firmware ready\n");
			msleep(5);
		}
	}
	dev->firmware_ready = 1;
}

void OV3640_init_regs(struct ov3640_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    int i=0;
    
    //software reset
    i2c_put_byte(client, 0x3012, 0x80);
    msleep(5); //delay 5ms

    while(1)
    {
        if (OV3640_script[i].val==0xff&&OV3640_script[i].addr==0xffff)
        {
        	printk("success in initial OV3640.\n");
        	break;
        }
        if((i2c_put_byte(client,OV3640_script[i].addr, OV3640_script[i].val)) < 0)
        {
        	printk("fail in initial OV3640. \n");
		return;
		}
		i++;
    }
    aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
    if (plat_dat&&plat_dat->custom_init_script) {
		i=0;
		aml_camera_i2c_fig_t*  custom_script = (aml_camera_i2c_fig_t*)plat_dat->custom_init_script;
		while(1)
		{
			if (custom_script[i].val==0xff&&custom_script[i].addr==0xffff)
			{
				printk("OV3640_write_custom_regs success in initial OV3640.\n");
				break;
			}
			if((i2c_put_byte(client,custom_script[i].addr, custom_script[i].val)) < 0)
			{
				printk("fail in initial OV3640 custom_regs. \n");
    			return;
			}
			i++;
		}
    }
    return;
}

#define Capture_PCLK_Frequency		56
#define Default_Reg0x3028			0x09
#define Default_Reg0x3029			0x47
#define Capture_Max_Gain16			32
static u16 Capture_Dummy_Pixels = 0;

static void OV3640_cal_ae(struct ov3640_device *dev, u16 image_target_width, u16 *exposure, u16 *gain)
{
	u8 temp_reg1, temp_reg2;
	u16 shutter, extra_lines, Preview_Gain, preview_dummy_pixels,Capture_Exposure;
	u16 Capture_Line_Width,Capture_Banding_Filter,Capture_Gain,Capture_Maximum_Shutter;
	u32 Gain_Exposure;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	
	temp_reg1 = i2c_get_byte(client, 0x3003); // AEC[b7~b0]
	temp_reg2 = i2c_get_byte(client, 0x3002); // AEC[b15~b8]
	shutter = (temp_reg1) | (temp_reg2 << 8);
	temp_reg1 = i2c_get_byte(client, 0x302E); // EXVTS[b7~b0]
	temp_reg2 = i2c_get_byte(client, 0x302D); // EXVTS[b15~b8]
	extra_lines = (temp_reg1) | (temp_reg2 << 8);
	temp_reg1 = i2c_get_byte(client, 0x3001);
	Preview_Gain = (((temp_reg1 & 0xF0) >> 4) + 1) * (16 + (temp_reg1 &0x0F));
	temp_reg1 = i2c_get_byte(client, 0x3028); // AEC[b7~b0]
	temp_reg2 = i2c_get_byte(client, 0x3029); // AEC[b15~b8]
	preview_dummy_pixels = ((temp_reg1 - Default_Reg0x3028) & 0xf0)<<8 + temp_reg2-Default_Reg0x3029;
	switch(image_target_width){
		case 2048:
		case 1600:
		case 1280:
		default:
			Capture_Line_Width = 2376 + Capture_Dummy_Pixels;
			Capture_Maximum_Shutter = 1640;
			break;
	}
	Capture_Exposure = (shutter + extra_lines)/2;// * Capture_PCLK_Frequency / Preview_PCLK_Frequency;
	//Calculate banding filter value
	if(ov3640_qctrl[7].default_value == CAM_BANDING_50HZ) {// (50Hz)
		Capture_Banding_Filter = Capture_PCLK_Frequency * 1000000 / 100 / (2 * Capture_Line_Width);
	}else {// (60Hz)
		Capture_Banding_Filter = Capture_PCLK_Frequency * 1000000 / 120 / (2 * Capture_Line_Width);
	}//redistribute gain and exposure
	Gain_Exposure = Preview_Gain * Capture_Exposure;
	if (Gain_Exposure < Capture_Banding_Filter * 16) {
		// Exposure < 1/100
		Capture_Exposure = Gain_Exposure / 16;
		if(0 == Capture_Exposure) {
			Capture_Exposure = 1;
		}
		Capture_Gain = (Gain_Exposure*2 + 1) / Capture_Exposure / 2;
	} else {
		if (Gain_Exposure > Capture_Maximum_Shutter * 16) {
			// Exposure > Capture_Maximum_Shutter
			Capture_Exposure = Capture_Maximum_Shutter;
			Capture_Gain = (Gain_Exposure*2 + 1)/Capture_Maximum_Shutter/2;
			if (Capture_Gain > Capture_Max_Gain16) {
				// gain reach maximum, insert extra line
				Capture_Exposure = Gain_Exposure * 11 / Capture_Max_Gain16/10;
				if(0 == Capture_Exposure) {
					Capture_Exposure = 1;
				}
				// For 50Hz, Exposure = n/100; For 60Hz, Exposure = n/120
				Capture_Exposure = Capture_Exposure/ Capture_Banding_Filter;
				Capture_Exposure = Capture_Exposure * Capture_Banding_Filter;
				Capture_Gain = (Gain_Exposure *2+1)/ Capture_Exposure/2;
			}
		} else {
			// 1/100(120) < Exposure < Capture_Maximum_Shutter, Exposure = n/100(120)
			Capture_Exposure = Gain_Exposure / 16/ Capture_Banding_Filter;
			Capture_Exposure = Capture_Exposure * Capture_Banding_Filter;
			if(0 == Capture_Exposure) {                                                        
				Capture_Exposure = 1;                                    
			}                                                        
			Capture_Gain = (Gain_Exposure*2 +1) / Capture_Exposure/2;
		}                                                        
	}                                                        
	*exposure = Capture_Exposure;                            
	*gain = Capture_Gain;                                    
}

void write_OV3640_shutter(struct ov3640_device *dev, u16 shutter)
{
	u8 iTemp;
	u16 extra_exposure_lines;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	
	if (shutter <= 1640/*FULL_EXPOSURE_LIMITATION*/) {
		extra_exposure_lines = 0;
	} else {
		extra_exposure_lines = shutter - 1640;//FULL_EXPOSURE_LIMITATION;
	}
	if (shutter > 1640/*FULL_EXPOSURE_LIMITATION*/) {
		shutter = 1640;//FULL_EXPOSURE_LIMITATION;
	}
	// set extra exposure line
	printk("extra_exposure_lines = %d\n", extra_exposure_lines);
	printk("caculated shutter value is %d\n", shutter);
	i2c_put_byte(client, 0x302E, extra_exposure_lines & 0xFF); // EXVTS[b7~b0]
	i2c_put_byte(client, 0x302D, (extra_exposure_lines & 0xFF00) >> 8); // EXVTS[b15~b8]
	/* Max exporsure time is 1 frmae period event if Tex is set longer than 1 frame period */
	i2c_put_byte(client, 0x3003, shutter & 0xFF); //AEC[7:0]
	i2c_put_byte(client, 0x3002, (shutter & 0xFF00) >> 8); //AEC[8:15]
}

void write_OV3640_gain(struct ov3640_device *dev, u16 Capture_Gain16)
{
	u16 Gain = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	
	if (Capture_Gain16 > 31) {
		Capture_Gain16 = Capture_Gain16 /2;
		Gain = 0x10;
	}
	if (Capture_Gain16 > 31) {
		Capture_Gain16 = Capture_Gain16 /2;
		Gain = Gain | 0x20;
	}
	if (Capture_Gain16 > 31) {
		Capture_Gain16 = Capture_Gain16 /2;
		Gain = Gain | 0x40;
	}
	if (Capture_Gain16 > 31) {
		Capture_Gain16 = Capture_Gain16 /2;
		Gain = Gain | 0x80;
	}
	if (Capture_Gain16 > 16) {
		Gain = Gain | (Capture_Gain16 -16);
	}
	i2c_put_byte(client, 0x3001, Gain);
}

/*************************************************************************
* FUNCTION
*    OV3640_set_param_wb
*
* DESCRIPTION
*    wb setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV3640_set_param_wb(struct ov3640_device *dev,enum  camera_wb_flip_e para)//°×Æ½ºâ
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    switch (para) {      
    	case CAM_WB_AUTO:
			i2c_put_byte(client, 0x332b, 0x00);

        	break;

    	case CAM_WB_CLOUD: 
			i2c_put_byte(client, 0x332b, 0x08);
			i2c_put_byte(client, 0x33a7, 0x68);
			i2c_put_byte(client, 0x33a8, 0x40);
			i2c_put_byte(client, 0x33a9, 0x4e);

        	break;

    	case CAM_WB_DAYLIGHT: 
			i2c_put_byte(client, 0x332b, 0x08);
			i2c_put_byte(client, 0x33a7, 0x5e);
			i2c_put_byte(client, 0x33a8, 0x40);
			i2c_put_byte(client, 0x33a9, 0x46);
			
        	break;

    	case CAM_WB_INCANDESCENCE: 
			i2c_put_byte(client, 0x332b, 0x08);
			i2c_put_byte(client, 0x33a7, 0x44);
			i2c_put_byte(client, 0x33a8, 0x40);
			i2c_put_byte(client, 0x33a9, 0x70);

        	break;
            
    	case CAM_WB_TUNGSTEN: 
			i2c_put_byte(client, 0x332b, 0x08);
			i2c_put_byte(client, 0x33a7, 0x44);
			i2c_put_byte(client, 0x33a8, 0x40);
			i2c_put_byte(client, 0x33a9, 0x70);

        	break;

      	case CAM_WB_FLUORESCENT:
			i2c_put_byte(client, 0x332b, 0x08);
			i2c_put_byte(client, 0x33a7, 0x52);
			i2c_put_byte(client, 0x33a8, 0x40);
			i2c_put_byte(client, 0x33a9, 0x58);
			
        	break;

    	case CAM_WB_MANUAL:
                // TODO
        	break;
    }
    

} /* OV3640_set_param_wb */
/*************************************************************************
* FUNCTION
*    OV3640_set_param_exposure
*
* DESCRIPTION
*    exposure setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV3640_set_param_exposure(struct ov3640_device *dev,enum camera_exposure_e para)//ÆØ¹âµ÷½Ú
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);


    switch (para) {
    	case EXPOSURE_N4_STEP:  //¸º4µµ  
			i2c_put_byte(client, 0x3018, 0x18);
			i2c_put_byte(client, 0x3019, 0x10);
			i2c_put_byte(client, 0x301a, 0x31);
        	
        	break;
            
    	case EXPOSURE_N3_STEP:
			i2c_put_byte(client, 0x3018, 0x20);
			i2c_put_byte(client, 0x3019, 0x18);
			i2c_put_byte(client, 0x301a, 0x41);
			
        	break;
            
    	case EXPOSURE_N2_STEP:
			i2c_put_byte(client, 0x3018, 0x28);
			i2c_put_byte(client, 0x3019, 0x20);
			i2c_put_byte(client, 0x301a, 0x51);
			
        	break;
            
    	case EXPOSURE_N1_STEP:
			i2c_put_byte(client, 0x3018, 0x30);
			i2c_put_byte(client, 0x3019, 0x28);
			i2c_put_byte(client, 0x301a, 0x61);
			
        	break;
            
    	case EXPOSURE_0_STEP://Ä¬ÈÏÁãµµ
			i2c_put_byte(client, 0x3018, 0x38);
			i2c_put_byte(client, 0x3019, 0x30);
			i2c_put_byte(client, 0x301a, 0x61);
			
        	break;
            
    	case EXPOSURE_P1_STEP://ÕýÒ»µµ
			i2c_put_byte(client, 0x3018, 0x40);
			i2c_put_byte(client, 0x3019, 0x38);
			i2c_put_byte(client, 0x301a, 0x71);
			
        	break;
            
    	case EXPOSURE_P2_STEP:
			i2c_put_byte(client, 0x3018, 0x48);
			i2c_put_byte(client, 0x3019, 0x40);
			i2c_put_byte(client, 0x301a, 0x81);
			
        	break;
            
    	case EXPOSURE_P3_STEP:
			i2c_put_byte(client, 0x3018, 0x50);
			i2c_put_byte(client, 0x3019, 0x48);
			i2c_put_byte(client, 0x301a, 0x91);
			
        	break;
            
    	case EXPOSURE_P4_STEP:    
			i2c_put_byte(client, 0x3018, 0x58);
			i2c_put_byte(client, 0x3019, 0x50);
			i2c_put_byte(client, 0x301a, 0x91);
			
        	break;
            
    	default:
			i2c_put_byte(client, 0x3018, 0x38);
			i2c_put_byte(client, 0x3019, 0x30);
			i2c_put_byte(client, 0x301a, 0x61);
			
        	break;
    }
} /* OV3640_set_param_exposure */
/*************************************************************************
* FUNCTION
*    OV3640_set_param_effect
*
* DESCRIPTION
*    effect setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV3640_set_param_effect(struct ov3640_device *dev,enum camera_effect_flip_e para)//ÌØÐ§ÉèÖÃ
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
  
    switch (para) {
    	case CAM_EFFECT_ENC_NORMAL://Õý³£
			i2c_put_byte(client,0x3302, 0xef);
			i2c_put_byte(client,0x3355, 0x00);
        	break;        

    	case CAM_EFFECT_ENC_GRAYSCALE://»Ò½×
			i2c_put_byte(client,0x3302, 0xef);
			i2c_put_byte(client,0x3355, 0x18);
			i2c_put_byte(client,0x335a, 0x80);
			i2c_put_byte(client,0x335b, 0x80);
        	break;

    	case CAM_EFFECT_ENC_SEPIA://¸´¹Å
			i2c_put_byte(client,0x3302, 0xef);
			i2c_put_byte(client,0x3355, 0x18);
			i2c_put_byte(client,0x335a, 0x40);
			i2c_put_byte(client,0x335b, 0xa6);
        	break;        
                
    	case CAM_EFFECT_ENC_SEPIAGREEN://¸´¹ÅÂÌ
			i2c_put_byte(client,0x3302, 0xef);
			i2c_put_byte(client,0x3355, 0x18);
			i2c_put_byte(client,0x335a, 0x60);
			i2c_put_byte(client,0x335b, 0x60);
        	break;                    

    	case CAM_EFFECT_ENC_SEPIABLUE://¸´¹ÅÀ¶
			i2c_put_byte(client,0x3302, 0xef);
			i2c_put_byte(client,0x3355, 0x18);
			i2c_put_byte(client,0x335a, 0xa0);
			i2c_put_byte(client,0x335b, 0x40);
        	break;                                

    	case CAM_EFFECT_ENC_COLORINV://µ×Æ¬
			i2c_put_byte(client,0x3302, 0xef);
			i2c_put_byte(client,0x3355, 0x40);
			
        	break;        

    	default:
			i2c_put_byte(client,0x3302, 0xef);
			i2c_put_byte(client,0x3355, 0x00);
        	break;
    }


} /* OV3640_set_param_effect */

/*************************************************************************
* FUNCTION
*	OV3640_night_mode
*
* DESCRIPTION
*    This function night mode of OV3640.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV3640_set_param_banding(struct ov3640_device *dev,enum  camera_night_mode_flip_e banding)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    unsigned char reg_val;
    switch(banding) {
        case CAM_BANDING_60HZ:
        	printk("set banding 60Hz\n");
        	reg_val = i2c_get_byte(client, 0x3014);
        	reg_val &= ~(1<<7);
        	i2c_put_byte(client, 0x3014, reg_val & 0xff);
            break;
        case CAM_BANDING_50HZ:
        	printk("set banding 50Hz\n");
        	reg_val = i2c_get_byte(client, 0x3014);
        	reg_val |= 1<<7;
        	i2c_put_byte(client, 0x3014, reg_val & 0xff);
            break;
    }
}

int OV3640_AutoFocus(struct ov3640_device *dev, int focus_mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int ret = 0;
    int i = 0;
    
	switch (focus_mode) {
        case CAM_FOCUS_MODE_AUTO:
            i2c_put_byte(client, 0x3f01 , 0x1);
            i2c_put_byte(client, 0x3f00 , 0x3); //start to auto focus
            while(i2c_get_byte(client, 0x3f01) && i < 100) {//wait for the auto focus to be done
            	i++;       
            	//pr_info("waiting focus\n");
                msleep(10);
            }
            
            if (i2c_get_byte(client, 0x3f06) == 0)
                ret = -1;
            else {
            	printk("pause auto focus\n");
            	msleep(10);
            	i2c_put_byte(client, 0x3f01 , 0x1);  
            	i2c_put_byte(client, 0x3f00 , 0x6); //pause the auto focus
            }
            break;
            
    	case CAM_FOCUS_MODE_CONTI_VID:
    	case CAM_FOCUS_MODE_CONTI_PIC:
    		i2c_put_byte(client, 0x3f01 , 0x1); 
            i2c_put_byte(client, 0x3f00 , 0x4); //start to auto focus
            /*while(i2c_get_byte(client, 0x3f01) == 0x1) {
            	msleep(10);
            }*/
            printk("start continous focus\n");
            break;
            
        case CAM_FOCUS_MODE_RELEASE:
        case CAM_FOCUS_MODE_FIXED:
        default:
            i2c_put_byte(client, 0x3f01 , 0x1);
            i2c_put_byte(client, 0x3f00 , 0x8);
            printk("release focus to infinit\n");
            break;
    }
    return ret;

}    /* OV3640_AutoFocus */

static resulution_size_type_t get_size_type(int width, int height)
{
	resulution_size_type_t rv = SIZE_NULL;
	if ((width >= 2500) && (height >= 1900))
		rv = SIZE_QSXGA_2560X2048;
	else if ((width >= 2000) && (height >= 1500))
		rv = SIZE_QXGA_2048X1536;
	else if ((width >= 1600) && (height >= 1200))
		rv = SIZE_UXGA_1600X1200;
	else if ((width >= 600) && (height >= 400))
		rv = SIZE_VGA_640X480;
	else if ((width >= 300) && (height >= 200))
		rv = SIZE_QVGA_320x240;
	return rv;
}

static resolution_param_t* get_resolution_param(struct ov3640_device *dev, int is_capture, int width, int height)
{
	int i = 0;
	int arry_size = 0;
	resolution_param_t* tmp_resolution_param = NULL;
	resulution_size_type_t res_type = SIZE_NULL;
	res_type = get_size_type(width, height);
	if (res_type == SIZE_NULL)
		return NULL;
	if (is_capture) {
		tmp_resolution_param = capture_resolution_array;
		arry_size = sizeof(capture_resolution_array);
	} else {
		tmp_resolution_param = prev_resolution_array;
		arry_size = sizeof(prev_resolution_array);
	}
            
	for (i = 0; i < arry_size; i++) {
		if (tmp_resolution_param[i].size_type == res_type)
			return &tmp_resolution_param[i];
    }
	return NULL;
}

static int set_resolution_param(struct ov3640_device *dev, resolution_param_t* res_param)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int rc = -1;
	if (!res_param->reg_script) {
		printk("error, resolution reg script is NULL\n");
		return -1;
	}
    int i=0;
    while(1) {
        if (res_param->reg_script[i].val==0xff&&res_param->reg_script[i].addr==0xffff) {
            printk("setting resolutin param complete\n");
            break;
        }
        if((i2c_put_byte(client,res_param->reg_script[i].addr, res_param->reg_script[i].val)) < 0) {
            printk("fail in setting resolution param. i=%d\n",i);
        	break;
        }
        i++;
    }
	dev->cur_resolution_param = res_param;
}

unsigned char v4l_2_ov3640(int val)
{
	int ret=val/0x20;
	if(ret<4) return ret*0x20+0x80;
	else if(ret<8) return ret*0x20+0x20;
	else return 0;
}

static int ov3640_setting(struct ov3640_device *dev,int PROP_ID,int value ) 
{
	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
	case V4L2_CID_BRIGHTNESS:
    	pr_info("setting brightness:%d\n",v4l_2_ov3640(value));
    	//ret=i2c_put_byte(client,0x0201,v4l_2_ov3640(value));
    	break;
	case V4L2_CID_CONTRAST:
		pr_info("setting contrast:%d\n",value);
    	//ret=i2c_put_byte(client,0x0200, value);
    	break;    
	case V4L2_CID_SATURATION:
		pr_info("setting saturation:%d\n",value);
    	//ret=i2c_put_byte(client,0x0202, value);
    	break;
#if 0	
	case V4L2_CID_EXPOSURE:
    	ret=i2c_put_byte(client,0x0201, value);
    	break;    
	case V4L2_CID_HFLIP:    /* set flip on H. */
    	ret=i2c_get_byte(client,0x0101);
    	if(ret>0) {
        	cur_val=(char)ret;
        	if(value!=0)
            	cur_val=cur_val|0x1;
        	else
            	cur_val=cur_val&0xFE;
        	ret=i2c_put_byte(client,0x0101,cur_val);
        	if(ret<0) dprintk(dev, 1, "V4L2_CID_HFLIP setting error\n");
        }  else {
        	dprintk(dev, 1, "vertical read error\n");
        }
    	break;
	case V4L2_CID_VFLIP:    /* set flip on V. */
    	ret=i2c_get_byte(client,0x0101);
    	if(ret>0) {
        	cur_val=(char)ret;
        	if(value!=0)
            	cur_val=cur_val|0x10;
        	else
            	cur_val=cur_val&0xFD;
        	ret=i2c_put_byte(client,0x0101,cur_val);
        } else {
        	dprintk(dev, 1, "vertical read error\n");
        }
    	break;   
#endif 
	case V4L2_CID_DO_WHITE_BALANCE:
        if(ov3640_qctrl[4].default_value!=value){
        	ov3640_qctrl[4].default_value=value;
        	OV3640_set_param_wb(dev,value);
        	printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
        }
    	break;
	case V4L2_CID_EXPOSURE:
        if(ov3640_qctrl[5].default_value!=value){
        	ov3640_qctrl[5].default_value=value;
        	OV3640_set_param_exposure(dev,value);
        	printk(KERN_INFO " set camera  exposure=%d. \n ",value);
        }
    	break;
	case V4L2_CID_COLORFX:
        if(ov3640_qctrl[6].default_value!=value){
        	ov3640_qctrl[6].default_value=value;
        	OV3640_set_param_effect(dev,value);
        	printk(KERN_INFO " set camera  effect=%d. \n ",value);
        }
    	break;
    case V4L2_CID_WHITENESS:
		if(ov3640_qctrl[7].default_value!=value){
			ov3640_qctrl[7].default_value=value;
			OV3640_set_param_banding(dev,value);
			printk(KERN_INFO " set camera  banding=%d. \n ",value);
		}
		break;
	case V4L2_CID_FOCUS_AUTO:
        if (dev->firmware_ready) 
        	ret = OV3640_AutoFocus(dev,value);
        else
        	ret = -1;
        break;
	default:
    	ret=-1;
    	break;
    }
	return ret;
    
}

static void power_down_ov3640(struct ov3640_device *dev)
{
	//struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	//i2c_put_byte(client,0x3022, 0x8); //release focus
	//i2c_put_byte(client,0x3008, 0x42);//in soft power down mode
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void ov3640_fillbuff(struct ov3640_fh *fh, struct ov3640_buffer *buf)
{
	struct ov3640_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	vm_output_para_t para ={0};
	dprintk(dev,1,"%s\n", __func__);	
	if (!vbuf)
		return;
 /*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
	para.mirror = -1;// not set
	para.v4l2_format = fh->fmt->fourcc;
	para.v4l2_memory = 0x18221223;
	para.zoom = -1;
	para.angle = 0;
	para.vaddr = (unsigned)vbuf;
	vm_fill_buffer(&buf->vb,&para);
	buf->vb.state = VIDEOBUF_DONE;
}

static void ov3640_thread_tick(struct ov3640_fh *fh)
{
	struct ov3640_buffer *buf;
	struct ov3640_device *dev = fh->dev;
	struct ov3640_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
    	dprintk(dev, 1, "No active queue to serve\n");
    	goto unlock;
    }

	buf = list_entry(dma_q->active.next,
             struct ov3640_buffer, vb.queue);
    dprintk(dev, 1, "%s\n", __func__);
    dprintk(dev, 1, "list entry get buf is %x\n",buf);

    /* Nobody is waiting on this buffer, return */
	if (!waitqueue_active(&buf->vb.done))
    	goto unlock;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

    /* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	ov3640_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);

	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

#define frames_to_ms(frames)                    \
    ((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static void ov3640_sleep(struct ov3640_fh *fh)
{
	struct ov3640_device *dev = fh->dev;
	struct ov3640_dmaqueue *dma_q = &dev->vidq;

	int timeout;
	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
        (unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
    	goto stop_task;

    /* Calculate time to wake up */
	timeout = msecs_to_jiffies(frames_to_ms(1));

	ov3640_thread_tick(fh);

	schedule_timeout_interruptible(timeout);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int ov3640_thread(void *data)
{
	struct ov3640_fh  *fh = data;
	struct ov3640_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
    	ov3640_sleep(fh);

    	if (kthread_should_stop())
        	break;
    }
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int ov3640_start_thread(struct ov3640_fh *fh)
{
	struct ov3640_device *dev = fh->dev;
	struct ov3640_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(ov3640_thread, fh, "ov3640");

	if (IS_ERR(dma_q->kthread)) {
    	v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
    	return PTR_ERR(dma_q->kthread);
    }
    /* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void ov3640_stop_thread(struct ov3640_dmaqueue  *dma_q)
{
	struct ov3640_device *dev = container_of(dma_q, struct ov3640_device, vidq);

	dprintk(dev, 1, "%s\n", __func__);
    /* shutdown control thread */
	if (dma_q->kthread) {
    	kthread_stop(dma_q->kthread);
    	dma_q->kthread = NULL;
    }
}

/* ------------------------------------------------------------------
	Videobuf operations
   ------------------------------------------------------------------*/
static int
buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	struct ov3640_fh  *fh = vq->priv_data;
	struct ov3640_device *dev  = fh->dev;
    //int bytes = fh->fmt->depth >> 3 ;
    *size = (fh->width*fh->height*fh->fmt->depth)>>3;    
	if (0 == *count)
        *count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
        (*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__,
        *count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct ov3640_buffer *buf)
{
	struct ov3640_fh  *fh = vq->priv_data;
	struct ov3640_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	if (in_interrupt())
    	BUG();

	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 3000
#define norm_maxh() 3000
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
                    	enum v4l2_field field)
{
	struct ov3640_fh     *fh  = vq->priv_data;
	struct ov3640_device    *dev = fh->dev;
	struct ov3640_buffer *buf = container_of(vb, struct ov3640_buffer, vb);
	int rc;
    //int bytes = fh->fmt->depth >> 3 ;
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	BUG_ON(NULL == fh->fmt);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
        fh->height < 32 || fh->height > norm_maxh())
    	return -EINVAL;

	buf->vb.size = (fh->width*fh->height*fh->fmt->depth)>>3;
	if (0 != buf->vb.baddr  &&  buf->vb.bsize < buf->vb.size)
    	return -EINVAL;

    /* These properties only change when queue is idle, see s_fmt */
	buf->fmt       = fh->fmt;
	buf->vb.width  = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field  = field;

    //precalculate_bars(fh);

	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
    	rc = videobuf_iolock(vq, &buf->vb, NULL);
    	if (rc < 0)
        	goto fail;
    }

	buf->vb.state = VIDEOBUF_PREPARED;

	return 0;

fail:
	free_buffer(vq, buf);
	return rc;
}

static void
buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct ov3640_buffer    *buf  = container_of(vb, struct ov3640_buffer, vb);
	struct ov3640_fh        *fh   = vq->priv_data;
	struct ov3640_device       *dev  = fh->dev;
	struct ov3640_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
               struct videobuf_buffer *vb)
{
	struct ov3640_buffer   *buf  = container_of(vb, struct ov3640_buffer, vb);
	struct ov3640_fh       *fh   = vq->priv_data;
	struct ov3640_device      *dev  = (struct ov3640_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops ov3640_video_qops = {
    .buf_setup      = buffer_setup,
    .buf_prepare    = buffer_prepare,
    .buf_queue      = buffer_queue,
    .buf_release    = buffer_release,
};

/* ------------------------------------------------------------------
	IOCTL vidioc handling
   ------------------------------------------------------------------*/
static int vidioc_querycap(struct file *file, void  *priv,
                	struct v4l2_capability *cap)
{
	struct ov3640_fh  *fh  = priv;
	struct ov3640_device *dev = fh->dev;

	strcpy(cap->driver, "ov3640");
	strcpy(cap->card, "ov3640");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = OV3640_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
            	V4L2_CAP_STREAMING     |
            	V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
                	struct v4l2_fmtdesc *f)
{
	struct ov3640_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
    	return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
                	struct v4l2_format *f)
{
	struct ov3640_fh *fh = priv;

	printk("vidioc_g_fmt_vid_cap...fh->width =%d,fh->height=%d\n",fh->width,fh->height);
	f->fmt.pix.width        = fh->width;
	f->fmt.pix.height       = fh->height;
	f->fmt.pix.field        = fh->vb_vidq.field;
	f->fmt.pix.pixelformat  = fh->fmt->fourcc;
	f->fmt.pix.bytesperline =
        (f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
    	f->fmt.pix.height * f->fmt.pix.bytesperline;

	return (0);
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
        	struct v4l2_format *f)
{
	struct ov3640_fh  *fh  = priv;
	struct ov3640_device *dev = fh->dev;
	struct ov3640_fmt *fmt;
	enum v4l2_field field;
	unsigned int maxw, maxh;

	fmt = get_format(f);
	if (!fmt) {
    	dprintk(dev, 1, "Fourcc format (0x%08x) invalid.\n",
        	f->fmt.pix.pixelformat);
    	return -EINVAL;
    }

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY) {
    	field = V4L2_FIELD_INTERLACED;
    } else if (V4L2_FIELD_INTERLACED != field) {
    	dprintk(dev, 1, "Field type invalid.\n");
    	return -EINVAL;
    }

	maxw  = norm_maxw();
	maxh  = norm_maxh();

	f->fmt.pix.field = field;
	v4l_bound_align_image(&f->fmt.pix.width, 48, maxw, 2,
                  &f->fmt.pix.height, 32, maxh, 0, 0);
	f->fmt.pix.bytesperline =
        (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
    	f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
                	struct v4l2_format *f)
{
	struct ov3640_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	struct ov3640_device *dev = fh->dev;
	resolution_param_t* res_param = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char gain = 0, exposurelow = 0, exposuremid = 0, exposurehigh = 0;

	int ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
    	return ret;

	mutex_lock(&q->vb_lock);

	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
    	dprintk(fh->dev, 1, "%s queue busy\n", __func__);
    	ret = -EBUSY;
    	goto out;
    }

	fh->fmt           = get_format(f);
	fh->width         = f->fmt.pix.width;
	fh->height        = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type          = f->type;

    printk("system aquire ...fh->height=%d, fh->width= %d\n",fh->height,fh->width);//potti
    if(f->fmt.pix.pixelformat==V4L2_PIX_FMT_RGB24){
    	res_param = get_resolution_param(dev, 1, fh->width,fh->height);
    	if (!res_param) {
    		printk("error, resolution param not get\n");
    		goto out;
   		}
    	
    	u16 shutter, gain;
    	//OV3640_cal_ae(dev, 2048, &shutter, &gain);

    	set_resolution_param(dev, res_param);
    	
    	//write_OV3640_shutter(dev, shutter);
    	//write_OV3640_gain(dev, gain);
    } else {
        res_param = get_resolution_param(dev, 0, fh->width,fh->height);
        if (!res_param) {
    		printk("error, resolution param not get\n");
    		goto out;
   		}
   		set_resolution_param(dev, res_param);
    }
    
    //pr_info("reg 0x3088 = %x\n", i2c_get_byte(client, 0x3088));
    //pr_info("reg 0x3089 = %x\n", i2c_get_byte(client, 0x3089));
    //pr_info("reg 0x308a = %x\n", i2c_get_byte(client, 0x308a));
    //pr_info("reg 0x308b = %x\n", i2c_get_byte(client, 0x308b));
#ifdef CONFIG_VIDEO_AMLOGIC_FLASHLIGHT	
	if (dev->platform_dev_data.flash_support) {
		if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24) {
			if (get_flashlightflag() == FLASHLIGHT_ON) {
				set_flashlight(true);
			}
		} else if(f->fmt.pix.pixelformat == V4L2_PIX_FMT_NV21){
			if (get_flashlightflag() != FLASHLIGHT_TORCH) {
				set_flashlight(false);
			}		
    	}
    }
#endif
    ret = 0;
out:
    mutex_unlock(&q->vb_lock);

    return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
              struct v4l2_requestbuffers *p)
{
	struct ov3640_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov3640_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov3640_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov3640_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
            	file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct ov3640_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ov3640_fh  *fh = priv;
    tvin_parm_t para;
    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    	return -EINVAL;
	if (i != fh->type)
    	return -EINVAL;

    para.port  = TVIN_PORT_CAMERA;
    para.fmt_info.fmt = TVIN_SIG_FMT_MAX+1;//TVIN_SIG_FMT_MAX+1;;TVIN_SIG_FMT_CAMERA_1280X720P_30Hz
    if (fh->dev->cur_resolution_param) {
		para.fmt_info.frame_rate = fh->dev->cur_resolution_param->active_fps;//175;
		para.fmt_info.h_active = fh->dev->cur_resolution_param->active_frmsize.width;
		para.fmt_info.v_active = fh->dev->cur_resolution_param->active_frmsize.height;
	} else {
		para.fmt_info.frame_rate = 175;
		para.fmt_info.h_active = 640;
		para.fmt_info.v_active = 478;
	}
	printk("change resolution: h_active = %d; v_active = %d\n", para.fmt_info.h_active, para.fmt_info.v_active);
	para.fmt_info.hsync_phase = 1;
	para.fmt_info.vsync_phase  = 0;    
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
    start_tvin_service(0,&para);
        fh->stream_on = 1;
    }
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ov3640_fh  *fh = priv;

    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    	return -EINVAL;
	if (i != fh->type)
    	return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if(ret == 0 ){
    stop_tvin_service(0);
        fh->stream_on        = 0;
    }
	return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,struct v4l2_frmsizeenum *fsize)
{
	int ret = 0,i=0;
	struct ov3640_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;
	for (i = 0; i < ARRAY_SIZE(formats); i++) {
    	if (formats[i].fourcc == fsize->pixel_format){
        	fmt = &formats[i];
        	break;
        }
    }
	if (fmt == NULL)
    	return -EINVAL;
	if (fmt->fourcc == V4L2_PIX_FMT_NV21){
	printk("ov3640_prev_resolution[fsize->index]   before fsize->index== %d\n",fsize->index);//potti
    	if (fsize->index >= ARRAY_SIZE(prev_resolution_array))
        	return -EINVAL;
    	frmsize = &prev_resolution_array[fsize->index].frmsize;
	printk("ov3640_prev_resolution[fsize->index]   after fsize->index== %d\n",fsize->index);
    	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    	fsize->discrete.width = frmsize->width;
    	fsize->discrete.height = frmsize->height;
    }
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
	printk("ov3640_pic_resolution[fsize->index]   before fsize->index== %d\n",fsize->index);
    	if (fsize->index >= ARRAY_SIZE(capture_resolution_array))
        	return -EINVAL;
    	frmsize = &capture_resolution_array[fsize->index].frmsize;
	printk("ov3640_pic_resolution[fsize->index]   after fsize->index== %d\n",fsize->index);    
    	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    	fsize->discrete.width = frmsize->width;
    	fsize->discrete.height = frmsize->height;
    }
	return ret;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id *i)
{
	return 0;
}

/* only one input in this sample driver */
static int vidioc_enum_input(struct file *file, void *priv,
            	struct v4l2_input *inp)
{
    //if (inp->index >= NUM_INPUTS)
        //return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	sprintf(inp->name, "Camera %u", inp->index);

	return (0);
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct ov3640_fh *fh = priv;
	struct ov3640_device *dev = fh->dev;

    *i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct ov3640_fh *fh = priv;
	struct ov3640_device *dev = fh->dev;

    //if (i >= NUM_INPUTS)
        //return -EINVAL;

	dev->input = i;
    //precalculate_bars(fh);

	return (0);
}

    /* --- controls ---------------------------------------------- */
static int vidioc_queryctrl(struct file *file, void *priv,
                struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov3640_qctrl); i++)
    	if (qc->id && qc->id == ov3640_qctrl[i].id) {
            memcpy(qc, &(ov3640_qctrl[i]),
            	sizeof(*qc));
            if (ov3640_qctrl[i].type == V4L2_CTRL_TYPE_MENU)
                return ov3640_qctrl[i].maximum+1;
            else
        	return (0);
        }

	return -EINVAL;
}

static int vidioc_querymenu(struct file *file, void *priv,
                struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(ov3640_qmenu_set); i++)
    	if (a->id && a->id == ov3640_qmenu_set[i].id) {
    	    for(j = 0; j < ov3640_qmenu_set[i].num; j++)
    	        if (a->index == ov3640_qmenu_set[i].ov3640_qmenu[j].index) {
        	        memcpy(a, &( ov3640_qmenu_set[i].ov3640_qmenu[j]),
            	        sizeof(*a));
        	        return (0);
        	    }
        }

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
             struct v4l2_control *ctrl)
{
	struct ov3640_fh *fh = priv;
	struct ov3640_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(ov3640_qctrl); i++)
    	if (ctrl->id == ov3640_qctrl[i].id) {
        	ctrl->value = dev->qctl_regs[i];
        	return 0;
        }

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
            	struct v4l2_control *ctrl)
{
	struct ov3640_fh *fh = priv;
	struct ov3640_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(ov3640_qctrl); i++)
    	if (ctrl->id == ov3640_qctrl[i].id) {
        	if (ctrl->value < ov3640_qctrl[i].minimum ||
                ctrl->value > ov3640_qctrl[i].maximum ||
                ov3640_setting(dev,ctrl->id,ctrl->value)<0) {
            	return -ERANGE;
            }
        	dev->qctl_regs[i] = ctrl->value;
        	return 0;
        }
	return -EINVAL;
}

/* ------------------------------------------------------------------
	File operations for the device
   ------------------------------------------------------------------*/

static int ov3640_open(struct file *file)
{
	struct ov3640_device *dev = video_drvdata(file);
	struct ov3640_fh *fh = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int retval = 0;
	int reg_val;
	int i = 0;
	ov3640_have_opened=1;

	if(dev->platform_dev_data.device_init) {
    	dev->platform_dev_data.device_init();
    	printk("+++%s found a init function, and run it..\n", __func__);
    }
	OV3640_init_regs(dev);
	
	msleep(10);
	
	if(OV3640_download_firmware(dev) >= 0) {
		while(((i2c_get_byte(client, 0x3f07) != 0x7f) || (i2c_get_byte(client, 0x3f07) != 0x7e)) && i < 10) { //wait for the mcu ready 
        	printk("wait camera ready\n");
       		msleep(100);
        	i++;
    	}
    	dev->firmware_ready = 1;
	}
	
	//schedule_work(&(dev->dl_work));

	mutex_lock(&dev->mutex);
	dev->users++;
	if (dev->users > 1) {
    	dev->users--;
    	mutex_unlock(&dev->mutex);
    	return -EBUSY;
    }

	dprintk(dev, 1, "open %s type=%s users=%d\n",
    	video_device_node_name(dev->vdev),
    	v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->users);

        /* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);
    spin_lock_init(&dev->slock);
    /* allocate + initialize per filehandle data */
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (NULL == fh) {
    	dev->users--;
    	retval = -ENOMEM;
    }
	mutex_unlock(&dev->mutex);

	if (retval)
    	return retval;

	file->private_data = fh;
	fh->dev      = dev;

	fh->type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt      = &formats[0];
	fh->width    = 640;
	fh->height   = 480;
	fh->stream_on = 0 ;
    /* Resets frame counters */
	dev->jiffies = jiffies;
            
	videobuf_queue_vmalloc_init(&fh->vb_vidq, &ov3640_video_qops,
        	NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
        	sizeof(struct ov3640_buffer), fh);

	ov3640_start_thread(fh);

	return 0;
}

static ssize_t
ov3640_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct ov3640_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    	return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
                	file->f_flags & O_NONBLOCK);
    }
	return 0;
}

static unsigned int
ov3640_poll(struct file *file, struct poll_table_struct *wait)
{
	struct ov3640_fh        *fh = file->private_data;
	struct ov3640_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
    	return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int ov3640_close(struct file *file)
{
	struct ov3640_fh         *fh = file->private_data;
	struct ov3640_device *dev       = fh->dev;
	struct ov3640_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	ov3640_have_opened=0;
	dev->firmware_ready = 0;
	ov3640_stop_thread(vidq);
	videobuf_stop(&fh->vb_vidq);
	if(fh->stream_on){
        stop_tvin_service(0);     
    }
	videobuf_mmap_free(&fh->vb_vidq);

	kfree(fh);

	mutex_lock(&dev->mutex);
	dev->users--;
	mutex_unlock(&dev->mutex);

	dprintk(dev, 1, "close called (dev=%s, users=%d)\n",
    	video_device_node_name(vdev), dev->users);
#if 1	    
	ov3640_qctrl[4].default_value=0;
	ov3640_qctrl[5].default_value=4;
	ov3640_qctrl[6].default_value=0;
	power_down_ov3640(dev);
#endif
	msleep(2);

	if(dev->platform_dev_data.device_uninit) {
    	dev->platform_dev_data.device_uninit();
    	printk("+++%s found a uninit function, and run it..\n", __func__);
    }
	msleep(2); 
	return 0;
}

static int ov3640_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ov3640_fh  *fh = file->private_data;
	struct ov3640_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
        (unsigned long)vma->vm_start,
        (unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
    	ret);

	return ret;
}

static const struct v4l2_file_operations ov3640_fops = {
    .owner	    = THIS_MODULE,
    .open       = ov3640_open,
    .release    = ov3640_close,
    .read       = ov3640_read,
    .poll	    = ov3640_poll,
    .ioctl      = video_ioctl2, /* V4L2 ioctl handler */
    .mmap       = ov3640_mmap,
};

static const struct v4l2_ioctl_ops ov3640_ioctl_ops = {
    .vidioc_querycap      = vidioc_querycap,
    .vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
    .vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
    .vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
    .vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
    .vidioc_reqbufs       = vidioc_reqbufs,
    .vidioc_querybuf      = vidioc_querybuf,
    .vidioc_qbuf          = vidioc_qbuf,
    .vidioc_dqbuf         = vidioc_dqbuf,
    .vidioc_s_std         = vidioc_s_std,
    .vidioc_enum_input    = vidioc_enum_input,
    .vidioc_g_input       = vidioc_g_input,
    .vidioc_s_input       = vidioc_s_input,
    .vidioc_queryctrl     = vidioc_queryctrl,
    .vidioc_querymenu     = vidioc_querymenu,
    .vidioc_g_ctrl        = vidioc_g_ctrl,
    .vidioc_s_ctrl        = vidioc_s_ctrl,
    .vidioc_streamon      = vidioc_streamon,
    .vidioc_streamoff     = vidioc_streamoff,
    .vidioc_enum_framesizes = vidioc_enum_framesizes,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
    .vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device ov3640_template = {
    .name	        = "ov3640_v4l",
    .fops           = &ov3640_fops,
    .ioctl_ops      = &ov3640_ioctl_ops,
    .release	    = video_device_release,

    .tvnorms        = V4L2_STD_525_60,
    .current_norm   = V4L2_STD_NTSC_M,
};

static int ov3640_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV3640, 0);
}

static const struct v4l2_subdev_core_ops ov3640_core_ops = {
    .g_chip_ident = ov3640_g_chip_ident,
};

static const struct v4l2_subdev_ops ov3640_ops = {
    .core = &ov3640_core_ops,
};

static int ov3640_probe(struct i2c_client *client,
        	const struct i2c_device_id *id)
{
	int pgbuf;
	int err;
	struct ov3640_device *t;
	struct v4l2_subdev *sd;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
        	client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
    	return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &ov3640_ops);
	mutex_init(&t->mutex);

    /* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
    	kfree(t);
    	kfree(client);
    	return -ENOMEM;
    }
	memcpy(t->vdev, &ov3640_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);

	INIT_WORK(&(t->dl_work), do_download);

    /* Register it */
	aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	if (plat_dat) {
    	t->platform_dev_data.device_init=plat_dat->device_init;
    	t->platform_dev_data.device_uninit=plat_dat->device_uninit;
    	if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
    	if(t->platform_dev_data.device_init) {
        	t->platform_dev_data.device_init();
        	printk("+++%s found a device_probe function, and run it..\n", __func__);
        }
    	power_down_ov3640(t);
    	if(t->platform_dev_data.device_uninit) {
    	t->platform_dev_data.device_uninit();
    	printk("+++%s found a uninit function, and run it..\n", __func__);
        }
    }
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
    	video_device_release(t->vdev);
    	kfree(t);
    	return err;
    }
	return 0;
}

static int ov3640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov3640_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	kfree(t);
	return 0;
}

static const struct i2c_device_id ov3640_id[] = {
    { "ov3640_i2c", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, ov3640_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
    .name = "ov3640",
    .probe = ov3640_probe,
    .remove = ov3640_remove,
    .id_table = ov3640_id,
};

