/*
 *sp0838 - This code emulates a real video device with v4l2 api
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
#include <mach/am_eth_pinmux.h>
#include <mach/pinmux.h>
#include <linux/tvin/tvin.h>
#include "common/plat_ctrl.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend sp0838_early_suspend;
#endif

#define SP0838_CAMERA_MODULE_NAME "sp0838"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define SP0838_CAMERA_MAJOR_VERSION 0
#define SP0838_CAMERA_MINOR_VERSION 7
#define SP0838_CAMERA_RELEASE 0
#define SP0838_CAMERA_VERSION \
	KERNEL_VERSION(SP0838_CAMERA_MAJOR_VERSION, SP0838_CAMERA_MINOR_VERSION, SP0838_CAMERA_RELEASE)

MODULE_DESCRIPTION("so0838 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");


/* supported controls */
static struct v4l2_queryctrl sp0838_qctrl[] = {
	/*{
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
	}, {
		.id            = V4L2_CID_SATURATION,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Saturation",
		.minimum       = 0x28,
		.maximum       = 0x60,
		.step          = 0x8,
		.default_value = 0x48,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_HFLIP,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "flip on horizontal",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	} ,{
		.id            = V4L2_CID_VFLIP,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "flip on vertical",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},*/{
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
	}
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct sp0838_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct sp0838_fmt formats[] = {
	{
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth    = 16,
	},
	
	{
		.name     = "RGB888 (24)",
		.fourcc   = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
		.depth    = 24,
	},	
	{
		.name     = "BGR888 (24)",
		.fourcc   = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth    = 24,
	},		
	{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,	
	},
	{
		.name     = "YUV420P",
		.fourcc   = V4L2_PIX_FMT_YUV420,
		.depth    = 12,
	}
#if 0
	{
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = V4L2_PIX_FMT_VYUY,
		.depth    = 16,	
	},
	{
		.name     = "RGB565 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
		.depth    = 16,
	},
	{
		.name     = "RGB555 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB555, /* gggbbbbb arrrrrgg */
		.depth    = 16,
	},
	{
		.name     = "RGB555 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB555X, /* arrrrrgg gggbbbbb */
		.depth    = 16,
	},
#endif
};

static struct sp0838_fmt *get_format(struct v4l2_format *f)
{
	struct sp0838_fmt *fmt;
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
struct sp0838_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct sp0838_fmt        *fmt;
};

struct sp0838_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(sp0838_devicelist);

struct sp0838_device {
	struct list_head			sp0838_devicelist;
	struct v4l2_subdev			sd;
	struct v4l2_device			v4l2_dev;

	spinlock_t                 slock;
	struct mutex				mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct sp0838_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int			   input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;
	
	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(sp0838_qctrl)];
};

static inline struct sp0838_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sp0838_device, sd);
}

struct sp0838_fh {
	struct sp0838_device            *dev;

	/* video capture */
	struct sp0838_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
};

static inline struct sp0838_fh *to_fh(struct sp0838_device *dev)
{
	return container_of(dev, struct sp0838_fh, dev);
}


/* ------------------------------------------------------------------
	reg spec of SP0838
   ------------------------------------------------------------------*/

#if 1
//P0838 30M 50Hz 20-6fps maxgain 0x70 10.0.txt
struct aml_camera_i2c_fig1_s SP0838_script[] = {  
#if 0 //30M	
  {0xfd , 0x00},  //P0
  {0x35 , 0x40},  //Outmode1 
  {0x1B , 0x02},
  {0x27 , 0xe8},
  {0x28 , 0x0B},
  {0x32 , 0x00},
  {0x22 , 0xc0},
  {0x26 , 0x10},
  {0x31 , 0x70},  //Upside/mirr/Pclk inv/sub
  {0x5f , 0x11},  //Bayer order
  {0xfd , 0x01},  //P1
  {0x25 , 0x1a},  //Awb start
  {0x26 , 0xfb},
  {0x28 , 0x61},
  {0x29 , 0x49},
  {0x31 , 0x60}, //64
  {0x32 , 0x18},
  {0x4d , 0xdc},
  {0x4e , 0x6b},
  {0x41 , 0x8c},
  {0x42 , 0x66},
  {0x55 , 0xff},
  {0x56 , 0x00},
  {0x59 , 0x82},
  {0x5a , 0x00},
  {0x5d , 0xff},
  {0x5e , 0x6f},
  {0x57 , 0xff},
  {0x58 , 0x00},
  {0x5b , 0xff},
  {0x5c , 0xa8},
  {0x5f , 0x75},
  {0x60 , 0x00},
  {0x2d , 0x00},
  {0x2e , 0x00},
  {0x2f , 0x00},
  {0x30 , 0x00},
  {0x33 , 0x00},
  {0x34 , 0x00},
  {0x37 , 0x00},
  {0x38 , 0x00},  //awb end
  {0xfd , 0x00},  //P0
  {0x33 , 0x6f},  //LSC BPC EN
  {0x51 , 0x3f},  //BPC debug start
  {0x52 , 0x09},  
  {0x53 , 0x00},  
  {0x54 , 0x00},
  {0x55 , 0x10},  //BPC debug end
  {0x4f , 0xff},  //blueedge
  {0x50 , 0xff},  
  {0x57 , 0x40},  //Raw filter debut start
  {0x58 , 0x40},  //4
  {0x59 , 0x10},  //4
  {0x56 , 0x70},
  {0x5a , 0x02},
  {0x5b , 0x02},
  {0x5c , 0x20},  //Raw filter debut end 
  {0x65 , 0x06},  //Sharpness debug start
  {0x66 , 0x01},
  {0x67 , 0x03},
  {0x68 , 0xc6},
  {0x69 , 0x7f},
  {0x6a , 0x01},
  {0x6b , 0x06},
  {0x6c , 0x01},
  {0x6d , 0x03},  //Edge gain normal
  {0x6e , 0xc6},  //Edge gain normal
  {0x6f , 0x7f},
  {0x70 , 0x01},
  {0x71 , 0x0a},
  {0x72 , 0x10},
  {0x73 , 0x03},
  {0x74 , 0xc4},
  {0x75 , 0x7f},
  {0x76 , 0x01},  //Sharpness debug end
  {0xcb , 0x07},  //HEQ&Saturation debug start 
  {0xcc , 0x04},
  {0xce , 0xff},
  {0xcf , 0x10},
  {0xd0 , 0x20},
  {0xd1 , 0x00},
  {0xd2 , 0x1c},
  {0xd3 , 0x16},
  {0xd4 , 0x00},
  {0xd6 , 0x1c},
  {0xd7 , 0x16},
  {0xdd , 0x70},  //Contrast
  {0xde , 0x90},  //HEQ&Saturation debug end
  {0x7f , 0x96},  //Color Correction start  
  {0x80 , 0xf2},
  {0x81 , 0xfe},
  {0x82 , 0xde},
  {0x83 , 0xa3},
  {0x84 , 0xff},
  {0x85 , 0xea},
  {0x86 , 0x81},
  {0x87 , 0x16},
  {0x88 , 0x3c},
  {0x89 , 0x33},
  {0x8a , 0x1f},  //Color Correction end
  {0x8b , 0x0 },  //gamma start
  {0x8c , 0x1a},
  {0x8d , 0x29},
  {0x8e , 0x41},
  {0x8f , 0x62},
  {0x90 , 0x7c},
  {0x91 , 0x90},
  {0x92 , 0xa2},
  {0x93 , 0xaf},
  {0x94 , 0xbc},
  {0x95 , 0xc5},
  {0x96 , 0xcd},
  {0x97 , 0xd5},
  {0x98 , 0xdd},
  {0x99 , 0xe5},
  {0x9a , 0xed},
  {0x9b , 0xf5},
  {0xfd , 0x01},  //P1
  {0x8d , 0xfd},
  {0x8e , 0xff},  //gamma end
  {0xfd , 0x00},  //P0
  {0xca , 0xcf},
  {0xd8 , 0x65},  //UV outdoor
  {0xd9 , 0x65},  //UV indoor 
  {0xda , 0x65},  //UV dummy
  {0xdb , 0x50},  //UV lowlight
  {0xb9 , 0x00},  //Ygamma start
  {0xba , 0x04},
  {0xbb , 0x08},
  {0xbc , 0x10},
  {0xbd , 0x20},
  {0xbe , 0x30},
  {0xbf , 0x40},
  {0xc0 , 0x50},
  {0xc1 , 0x60},
  {0xc2 , 0x70},
  {0xc3 , 0x80},
  {0xc4 , 0x90},
  {0xc5 , 0xA0},
  {0xc6 , 0xB0},
  {0xc7 , 0xC0},
  {0xc8 , 0xD0},
  {0xc9 , 0xE0},
  {0xfd , 0x01},  //P1
  {0x89 , 0xf0},
  {0x8a , 0xff},  //Ygamma end
  {0xfd , 0x00},  //P0
  {0xe8 , 0x30},  //AEdebug start
  {0xe9 , 0x30},
  {0xea , 0x40},  //Alc Window sel
  {0xf4 , 0x1b},  //outdoor mode sel
  {0xf5 , 0x80},
  {0xf7 , 0x78},  //AE target 
  {0xf8 , 0x63},  
  {0xf9 , 0x68},  //AE target 
  {0xfa , 0x53},
  {0xfd , 0x01},  //P1
  {0x09 , 0x31},  //AE Step 3.0
  {0x0a , 0x85},
  {0x0b , 0x0b},  //AE Step 3.0
  {0x14 , 0x20},
  {0x15 , 0x0f},
  {0xfd , 0x00}, //p0
  {0x05 , 0x0},
  {0x06 , 0x0},
  {0x09 , 0x2},
  {0x0a , 0xa8},
  {0xf0 , 0x62},
  {0xf1 , 0x0},
  {0xf2 , 0x5f},
  {0xf5 , 0x78},
  {0xfd , 0x01},  //P1
  {0x00 , 0xb8},
  {0x0f , 0x60},
  {0x16 , 0x60},
  {0x17 , 0xa8},
  {0x18 , 0xb0},
  {0x1b , 0x60},
  {0x1c , 0xb0},
  {0xb4 , 0x20},
  {0xb5 , 0x3a},
  {0xb6 , 0x5e},
  {0xb9 , 0x40},
  {0xba , 0x4f},
  {0xbb , 0x47},
  {0xbc , 0x45},
  {0xbd , 0x43},
  {0xbe , 0x42},
  {0xbf , 0x42},
  {0xc0 , 0x42},
  {0xc1 , 0x41},
  {0xc2 , 0x41},
  {0xc3 , 0x41},
  {0xc4 , 0x41},
  {0xc5 , 0x41},
  {0xc6 , 0x41},
  {0xca , 0x70},
  {0xcb , 0x10},   //;AEdebug end
  {0xfd , 0x00},  //;P0
  {0x32 , 0x15 }, //;Auto_mode set
  {0x34 , 0x26},  //;Isp_mode set
 
 #else
{0xfd , 0x00},//P0
{0x35 , 0x40},//Outmode1
{0x1B , 0x02},
{0x27 , 0xe8},
{0x28 , 0x0B},
{0x32 , 0x00},
{0x22 , 0xc0},
{0x26 , 0x10},
{0x5f , 0x11},  //Bayer order
{0xfd , 0x01},  //P1
{0x25 , 0x1a},  //Awb start
{0x26 , 0xfb},
{0x28 , 0x61},
{0x29 , 0x49},
{0x31 , 0x60},//64
{0x32 , 0x18},
{0x4d , 0xdc},
{0x4e , 0x6b},
{0x41 , 0x8c},
{0x42 , 0x66},
{0x55 , 0xff},
{0x56 , 0x00},
{0x59 , 0x82},
{0x5a , 0x00},
{0x5d , 0xff},
{0x5e , 0x6f},
{0x57 , 0xff},
{0x58 , 0x00},
{0x5b , 0xff},
{0x5c , 0xa8},
{0x5f , 0x75},
{0x60 , 0x00},
{0x2d , 0x00},
{0x2e , 0x00},
{0x2f , 0x00},
{0x30 , 0x00},
{0x33 , 0x00},
{0x34 , 0x00},
{0x37 , 0x00},
{0x38 , 0x00}, //awb end
{0xfd , 0x00}, //P0
{0x33 , 0x6f}, //LSC BPC EN
{0x51 , 0x3f}, //BPC debug start
{0x52 , 0x09}, 
{0x53 , 0x00}, 
{0x54 , 0x00},
{0x55 , 0x10}, //BPC debug end
{0x4f , 0xff}, //blueedge
{0x50 , 0xff},
{0x57 , 0x40}, //Raw filter debut start
{0x58 , 0x40},
{0x59 , 0x10},
{0x56 , 0x70},
{0x5a , 0x02},
{0x5b , 0x02},
{0x5c , 0x20}, //Raw filter debut end 
{0x65 , 0x06}, //Sharpness debug start
{0x66 , 0x01},
{0x67 , 0x03},
{0x68 , 0xc6},
{0x69 , 0x7f},
{0x6a , 0x01},
{0x6b , 0x06},
{0x6c , 0x01},
{0x6d , 0x03}, //Edge gain normal
{0x6e , 0xc6}, //Edge gain normal
{0x6f , 0x7f},
{0x70 , 0x01},
{0x71 , 0x0a},
{0x72 , 0x10},
{0x73 , 0x03},
{0x74 , 0xc4},
{0x75 , 0x7f},
{0x76 , 0x01}, //Sharpness debug end
{0xcb , 0x07}, //HEQ&Saturation debug start 
{0xcc , 0x04},
{0xce , 0xff},
{0xcf , 0x10},
{0xd0 , 0x20},
{0xd1 , 0x00},
{0xd2 , 0x1c},
{0xd3 , 0x16},
{0xd4 , 0x00},
{0xd6 , 0x1c},
{0xd7 , 0x16},
{0xdd , 0x70}, //Contrast
{0xde , 0x90}, //HEQ&Saturation debug end
{0x7f , 0x96}, //Color Correction start
{0x80 , 0xf2},                         
{0x81 , 0xfe},                         
{0x82 , 0xde},                         
{0x83 , 0xa3},                         
{0x84 , 0xff},                         
{0x85 , 0xea},                         
{0x86 , 0x81},                         
{0x87 , 0x16},                         
{0x88 , 0x3c},                         
{0x89 , 0x33},                         
{0x8a , 0x1f}, //Color Correction end  
{0x8b , 0x00}, //gamma start
{0x8c , 0x1a},              
{0x8d , 0x29},              
{0x8e , 0x41},              
{0x8f , 0x62},              
{0x90 , 0x7c},              
{0x91 , 0x90},              
{0x92 , 0xa2},              
{0x93 , 0xaf},              
{0x94 , 0xbc},              
{0x95 , 0xc5},              
{0x96 , 0xcd},              
{0x97 , 0xd5},              
{0x98 , 0xdd},              
{0x99 , 0xe5},              
{0x9a , 0xed},              
{0x9b , 0xf5},              
{0xfd , 0x01}, //P1         
{0x8d , 0xfd},              
{0x8e , 0xff}, //gamma end  
{0xfd , 0x00}, //P0
{0xca , 0xcf},
{0xd8 , 0x65}, //UV outdoor
{0xd9 , 0x65}, //UV indoor 
{0xda , 0x65}, //UV dummy
{0xdb , 0x50}, //UV lowlight
{0xb9 , 0x00}, //Ygamma start
{0xba , 0x04},
{0xbb , 0x08},
{0xbc , 0x10},
{0xbd , 0x20},
{0xbe , 0x30},
{0xbf , 0x40},
{0xc0 , 0x50},
{0xc1 , 0x60},
{0xc2 , 0x70},
{0xc3 , 0x80},
{0xc4 , 0x90},
{0xc5 , 0xA0},
{0xc6 , 0xB0},
{0xc7 , 0xC0},
{0xc8 , 0xD0},
{0xc9 , 0xE0},
{0xfd , 0x01}, //P1
{0x89 , 0xf0},
{0x8a , 0xff}, //Ygamma end
{0xfd , 0x00}, //P0
{0xe8 , 0x30}, //AEdebug start
{0xe9 , 0x30},
{0xea , 0x40}, //Alc Window sel
{0xf4 , 0x1b}, //outdoor mode sel
{0xf5 , 0x80},
{0xf7 , 0x78}, //AE target
{0xf8 , 0x63}, 
{0xf9 , 0x68}, //AE target
{0xfa , 0x53},
{0xfd , 0x01}, //P1
{0x09 , 0x31}, //AE Step 3.0
{0x0a , 0x85},
{0x0b , 0x0b}, //AE Step 3.0
{0x14 , 0x20},
{0x15 , 0x0f},
{0xfd , 0x00}, //P0
{0x05 , 0x00}, 
{0x06 , 0x00},
{0x09 , 0x01},
{0x0a , 0x76},
{0xf0 , 0x62},
{0xf1 , 0x00},
{0xf2 , 0x5f},
{0xf5 , 0x78},
{0xfd , 0x01}, //P1
{0x00 , 0xba},
{0x0f , 0x60},
{0x16 , 0x60},
{0x17 , 0xa2},
{0x18 , 0xaa},
{0x1b , 0x60},
{0x1c , 0xaa},
{0xb4 , 0x20},
{0xb5 , 0x3a},
{0xb6 , 0x5e},
{0xb9 , 0x40},
{0xba , 0x4f},
{0xbb , 0x47},
{0xbc , 0x45},
{0xbd , 0x43},
{0xbe , 0x42},
{0xbf , 0x42},
{0xc0 , 0x42},
{0xc1 , 0x41},
{0xc2 , 0x41},
{0xc3 , 0x41},
{0xc4 , 0x41},
{0xc5 , 0x78},
{0xc6 , 0x41},
{0xca , 0x78},
{0xcb , 0x0c}, //AEdebug end

#if 1//24Mhz 50hz 6~20 FPS	
{0xfd , 0x00},
{0x05 , 0x0 },
{0x06 , 0x0 },
{0x09 , 0x1 },
{0x0a , 0x76},
{0xf0 , 0x62},
{0xf1 , 0x0 },
{0xf2 , 0x5f},
{0xf5 , 0x78},
{0xfd , 0x01},
{0x00 , 0xb8},
{0x0f , 0x60},
{0x16 , 0x60},
{0x17 , 0xa8},
{0x18 , 0xb0},
{0x1b , 0x60},
{0x1c , 0xb0},
{0xb4 , 0x20},
{0xb5 , 0x3a},
{0xb6 , 0x5e},
{0xb9 , 0x40},
{0xba , 0x4f},
{0xbb , 0x47},
{0xbc , 0x45},
{0xbd , 0x43},
{0xbe , 0x42},
{0xbf , 0x42},
{0xc0 , 0x42},
{0xc1 , 0x41},
{0xc2 , 0x41},
{0xc3 , 0x41},
{0xc4 , 0x41},
{0xc5 , 0x41},
{0xc6 , 0x41},
{0xca , 0x70},
{0xcb , 0x10},
{0xfd , 0x00},
#endif	

{0xfd , 0x00}, //P0
{0x32 , 0x15}, //Auto_mode set
{0x34 , 0x26}, //Isp_mode set
 #endif 
{0xff , 0xff},

};

//load GT2005 parameters
void SP0838_init_regs(struct sp0838_device *dev)
{
    int i=0;//,j;
    unsigned char buf[2];
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    while(1)
    {
        buf[0] = SP0838_script[i].addr;//(unsigned char)((SP0838_script[i].addr >> 8) & 0xff);
        //buf[1] = (unsigned char)(SP0838_script[i].addr & 0xff);
	    buf[1] = SP0838_script[i].val;
		i++;
	 if (SP0838_script[i].val==0xff&&SP0838_script[i].addr==0xff) 
	 	{
 	    	printk("SP0838_write_regs success in initial SP0838.\n");
	 	break;
	 	}
	 	
//        unsigned char	temp_reg=i2c_get_byte_add8(client,0x02);
	 	
 
//    	    	printk("camrea sp0838 slave ID :%s \n",temp_reg);
	 	
        if((i2c_put_byte_add8(client,buf, 2)) < 0)
        	{
    	    	printk("fail in initial SP0838. i = %d \n", i);
		break;
        	}
    }

    return;

}

/*void SP0838_init_regs(struct sp0838_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    int i=0;
    
    while(1)
    {
        if (SP0838_script[i].val==0xff&&SP0838_script[i].addr==0xff) 
        {
        	//printk("GT2005_write_regs success in initial GT2005.\n");
        	break;
        }
        if((i2c_put_byte(client,SP0838_script[i].addr, SP0838_script[i].val)) < 0)
        {
        	printk("fail in initial SP0838. \n");
		break;
		}
		i++;
    }

    return;
}
*/
#endif

/*************************************************************************
* FUNCTION
*	set_SP0838_param_wb
*
* DESCRIPTION
*	SP0838 wb setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED  白平衡参数
*
*************************************************************************/
void set_SP0838_param_wb(struct sp0838_device *dev,enum  camera_wb_flip_e para)
{
//	kal_uint16 rgain=0x80, ggain=0x80, bgain=0x80;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	unsigned char buf[4];

	unsigned char  temp_reg;	
	//temp_reg=sp0838_read_byte(0x22);
	buf[0]=0x22;
	temp_reg=i2c_get_byte_add8(client,buf);


	switch (para)
	{
		case CAM_WB_AUTO:
			buf[0]=0x5a;
			buf[1]=0x56;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0x4a;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x22;
			buf[1]=temp_reg|0x02;
			i2c_put_byte_add8(client,buf,2);
			break;	
	  
		case CAM_WB_CLOUD:
			buf[0]=0x22;
			buf[1]=temp_reg&~0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5a;
			buf[1]=0x8c;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x50;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			break;		

		case CAM_WB_DAYLIGHT:   // tai yang guang
		    buf[0]=0x22;
			buf[1]=temp_reg&~0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5a;
			buf[1]=0x74;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x52;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);		
			break;		

		case CAM_WB_INCANDESCENCE:   // bai re guang
		    buf[0]=0x22;
			buf[1]=temp_reg&~0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5a;
			buf[1]=0x48;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0x5c;
			i2c_put_byte_add8(client,buf,2);
			break;		

		case CAM_WB_FLUORESCENT:   //ri guang deng
		    buf[0]=0x22;
			buf[1]=temp_reg&~0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5a;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x42;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0x50;
			i2c_put_byte_add8(client,buf,2);	
			break;		

		case CAM_WB_TUNGSTEN:   // wu si deng
		    buf[0]=0x22;
			buf[1]=temp_reg&~0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5a;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x54;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0x70;
			i2c_put_byte_add8(client,buf,2);	
			break;

		case CAM_WB_MANUAL: 	
			// TODO
			break;		
		default:
			break;		
	}		
//	kal_sleep_task(20);
}

/*************************************************************************
* FUNCTION
*	SP0838_night_mode
*
* DESCRIPTION
*	This function night mode of SP0838.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void SP0838_night_mode(struct sp0838_device *dev,enum  camera_night_mode_flip_e enable)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[4];

	unsigned char  temp_reg;	
	//temp_reg=sp0838_read_byte(0x22);
	buf[0]=0x20;
	temp_reg=i2c_get_byte_add8(client,buf);
	
    if(enable)
    {
		buf[0]=0xec;
		buf[1]=0x30;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x20;
		buf[1]=temp_reg&0x5f;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x3c;
		buf[1]=0x08;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x3d;
		buf[1]=0x08;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x3e;
		buf[1]=0x08;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x3f;
		buf[1]=0x08;
		i2c_put_byte_add8(client,buf,2);

     }
    else
     {
		buf[0]=0xec;
		buf[1]=0x20;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x20;
		buf[1]=temp_reg|0x20;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x3c;
		buf[1]=0x02;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x3d;
		buf[1]=0x02;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x3e;
		buf[1]=0x02;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x3f;
		buf[1]=0x02;
		i2c_put_byte_add8(client,buf,2);

	}

}

/*************************************************************************
* FUNCTION
*	set_SP0838_param_exposure
*
* DESCRIPTION
*	SP0838 exposure setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED  亮度等级 调节参数
*
*************************************************************************/
void set_SP0838_param_exposure(struct sp0838_device *dev,enum camera_exposure_e para)//曝光调节
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	unsigned char buf1[2];
	unsigned char buf2[2];

	switch (para)
	{
		case EXPOSURE_N4_STEP:
			buf1[0]=0xb5;
			buf1[1]=0xf8;
			buf2[0]=0xd3;
			buf2[1]=0x28;
			break;		
		case EXPOSURE_N3_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x00;
			buf2[0]=0xd3;
			buf2[1]=0x28;
			break;		
		case EXPOSURE_N2_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x08;
			buf2[0]=0xd3;
			buf2[1]=0x30;
			break;				
		case EXPOSURE_N1_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x10;
			buf2[0]=0xd3;
			buf2[1]=0x30;
			break;				
		case EXPOSURE_0_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x18;//48
			buf2[0]=0xd3;
			buf2[1]=0x38;//6a
			break;				
		case EXPOSURE_P1_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x30;
			buf2[0]=0xd3;
			buf2[1]=0x54;
			break;				
		case EXPOSURE_P2_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x44;
			buf2[0]=0xd3;
			buf2[1]=0x64;
			break;				
		case EXPOSURE_P3_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x54;
			buf2[0]=0xd3;
			buf2[1]=0x74;
			break;				
		case EXPOSURE_P4_STEP:	
			buf1[0]=0xb5;
			buf1[1]=0x64;
			buf2[0]=0xd3;
			buf2[1]=0x84;
			break;
		default:
			buf1[0]=0xb5;
			buf1[1]=0x00;
			buf2[0]=0xd3;
			buf2[1]=0x60;
			break;    
	}			
	//msleep(300);
	i2c_put_byte_add8(client,buf1,2);
	i2c_put_byte_add8(client,buf2,2);
	
}

/*************************************************************************
* FUNCTION
*	set_SP0838_param_effect
*
* DESCRIPTION
*	SP0838 effect setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED  特效参数
*
*************************************************************************/
void set_SP0838_param_effect(struct sp0838_device *dev,enum camera_effect_flip_e para)//特效设置
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[4];
	switch (para)
	{	
		case CAM_EFFECT_ENC_NORMAL:
			buf[0]=0x23;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x2d;
		    buf[1]=0x0a;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x20;
		    buf[1]=0xff;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd2;
		    buf[1]=0x90;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x73;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x77;
		    buf[1]=0x54;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb3;
		    buf[1]=0x40;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
		    buf[1]=0x80;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xba;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbb;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
	
			break;		
		case CAM_EFFECT_ENC_GRAYSCALE:
			buf[0]=0x23;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x2d;
		    buf[1]=0x0a;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x20;
		    buf[1]=0xff;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd2;
		    buf[1]=0x90;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x73;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x77;
		    buf[1]=0x54;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb3;
		    buf[1]=0x40;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
		    buf[1]=0x80;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xba;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbb;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);

			break;		
		case CAM_EFFECT_ENC_SEPIA:
			buf[0]=0x23;
		    buf[1]=0x02;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x2d;
		    buf[1]=0x0a;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x20;
		    buf[1]=0xff;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd2;
		    buf[1]=0x90;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x73;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb3;
		    buf[1]=0x40;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
		    buf[1]=0x80;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xba;
		    buf[1]=0xd0;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbb;
		    buf[1]=0x28;
		    i2c_put_byte_add8(client,buf,2);

			break;		
		case CAM_EFFECT_ENC_COLORINV:	
			buf[0]=0x23;
		    buf[1]=0x01;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x2d;
		    buf[1]=0x0a;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x20;
		    buf[1]=0xff;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd2;
		    buf[1]=0x90;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x73;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb3;
		    buf[1]=0x40;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
		    buf[1]=0x80;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xba;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbb;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			break;		
		case CAM_EFFECT_ENC_SEPIAGREEN:
			buf[0]=0x23;
		    buf[1]=0x02;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x2d;
		    buf[1]=0x0a;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x20;
		    buf[1]=0xff;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd2;
		    buf[1]=0x90;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x77;
		    buf[1]=0x88;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb3;
		    buf[1]=0x40;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
		    buf[1]=0x80;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xba;
		    buf[1]=0xc0;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbb;
		    buf[1]=0xc0;
		    i2c_put_byte_add8(client,buf,2);
			break;					
		case CAM_EFFECT_ENC_SEPIABLUE:
			buf[0]=0x23;
		    buf[1]=0x02;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x2d;
		    buf[1]=0x0a;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x20;
		    buf[1]=0xff;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd2;
		    buf[1]=0x90;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0x73;
		    buf[1]=0x00;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb3;
		    buf[1]=0x40;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
		    buf[1]=0x80;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xba;
		    buf[1]=0x50;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbb;
		    buf[1]=0xe0;
		    i2c_put_byte_add8(client,buf,2);

			break;									
		default:
			break;	
	}
	
}

unsigned char v4l_2_sp0838(int val)
{
	int ret=val/0x20;
	if(ret<4) return ret*0x20+0x80;
	else if(ret<8) return ret*0x20+0x20;
	else return 0;
}

static int sp0838_setting(struct sp0838_device *dev,int PROP_ID,int value ) 
{
	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
	case V4L2_CID_BRIGHTNESS:
		dprintk(dev, 1, "setting brightned:%d\n",v4l_2_sp0838(value));
		ret=i2c_put_byte(client,0x0201,v4l_2_sp0838(value));
		break;
	case V4L2_CID_CONTRAST:
		ret=i2c_put_byte(client,0x0200, value);
		break;	
	case V4L2_CID_SATURATION:
		ret=i2c_put_byte(client,0x0202, value);
		break;
#if 0	
	case V4L2_CID_EXPOSURE:
		ret=i2c_put_byte(client,0x0201, value);
		break;	
#endif
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
	case V4L2_CID_DO_WHITE_BALANCE:
        if(sp0838_qctrl[0].default_value!=value){
			sp0838_qctrl[0].default_value=value;
			set_SP0838_param_wb(dev,value);
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
        	}
		break;
	case V4L2_CID_EXPOSURE:
        if(sp0838_qctrl[1].default_value!=value){
			sp0838_qctrl[1].default_value=value;
			set_SP0838_param_exposure(dev,value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
        	}
		break;
	case V4L2_CID_COLORFX:
        if(sp0838_qctrl[2].default_value!=value){
			sp0838_qctrl[2].default_value=value;
			set_SP0838_param_effect(dev,value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
        	}
		break;
	default:
		ret=-1;
		break;
	}
	return ret;
	
}

static void power_down_sp0838(struct sp0838_device *dev)
{
	//struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	return;
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

extern   int vm_fill_buffer(struct videobuf_buffer* vb , int v4l2_format , int magic,void* vaddr);
#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void sp0838_fillbuff(struct sp0838_fh *fh, struct sp0838_buffer *buf)
{
	struct sp0838_device *dev = fh->dev;
	int h , pos = 0;
	int hmax  = buf->vb.height;
	int wmax  = buf->vb.width;
	struct timeval ts;
	char *tmpbuf;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	dprintk(dev,1,"%s\n", __func__);	
	if (!vbuf)
		return;
 /*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
    vm_fill_buffer(&buf->vb,fh->fmt->fourcc ,0x18221223,vbuf);
	buf->vb.state = VIDEOBUF_DONE;
}

static void sp0838_thread_tick(struct sp0838_fh *fh)
{
	struct sp0838_buffer *buf;
	struct sp0838_device *dev = fh->dev;
	struct sp0838_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 1, "No active queue to serve\n");
		goto unlock;
	}

	buf = list_entry(dma_q->active.next,
			 struct sp0838_buffer, vb.queue);
    dprintk(dev, 1, "%s\n", __func__);
    dprintk(dev, 1, "list entry get buf is %x\n",buf);

	/* Nobody is waiting on this buffer, return */
	if (!waitqueue_active(&buf->vb.done))
		goto unlock;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	sp0838_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);

	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

#define frames_to_ms(frames)					\
	((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static void sp0838_sleep(struct sp0838_fh *fh)
{
	struct sp0838_device *dev = fh->dev;
	struct sp0838_dmaqueue *dma_q = &dev->vidq;

	int timeout;
	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	//timeout = msecs_to_jiffies(frames_to_ms(1));

	sp0838_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int sp0838_thread(void *data)
{
	struct sp0838_fh  *fh = data;
	struct sp0838_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		sp0838_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int sp0838_start_thread(struct sp0838_fh *fh)
{
	struct sp0838_device *dev = fh->dev;
	struct sp0838_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(sp0838_thread, fh, "sp0838");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void sp0838_stop_thread(struct sp0838_dmaqueue  *dma_q)
{
	struct sp0838_device *dev = container_of(dma_q, struct sp0838_device, vidq);

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
	struct sp0838_fh  *fh = vq->priv_data;
	struct sp0838_device *dev  = fh->dev;
    //int bytes = fh->fmt->depth >> 3 ;
	*size = fh->width*fh->height*fh->fmt->depth >> 3;	
	if (0 == *count)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__,
		*count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct sp0838_buffer *buf)
{
	struct sp0838_fh  *fh = vq->priv_data;
	struct sp0838_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	if (in_interrupt())
		BUG();

	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 1024
#define norm_maxh() 768
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
						enum v4l2_field field)
{
	struct sp0838_fh     *fh  = vq->priv_data;
	struct sp0838_device    *dev = fh->dev;
	struct sp0838_buffer *buf = container_of(vb, struct sp0838_buffer, vb);
	int rc;
    //int bytes = fh->fmt->depth >> 3 ;
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	BUG_ON(NULL == fh->fmt);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
	    fh->height < 32 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = fh->width*fh->height*fh->fmt->depth >> 3;
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
	struct sp0838_buffer    *buf  = container_of(vb, struct sp0838_buffer, vb);
	struct sp0838_fh        *fh   = vq->priv_data;
	struct sp0838_device       *dev  = fh->dev;
	struct sp0838_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct sp0838_buffer   *buf  = container_of(vb, struct sp0838_buffer, vb);
	struct sp0838_fh       *fh   = vq->priv_data;
	struct sp0838_device      *dev  = (struct sp0838_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops sp0838_video_qops = {
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
	struct sp0838_fh  *fh  = priv;
	struct sp0838_device *dev = fh->dev;

	strcpy(cap->driver, "sp0838");
	strcpy(cap->card, "sp0838");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = SP0838_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct sp0838_fmt *fmt;

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
	struct sp0838_fh *fh = priv;

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
	struct sp0838_fh  *fh  = priv;
	struct sp0838_device *dev = fh->dev;
	struct sp0838_fmt *fmt;
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
	struct sp0838_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;

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

	ret = 0;
out:
	mutex_unlock(&q->vb_lock);

	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct sp0838_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct sp0838_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct sp0838_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct sp0838_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct sp0838_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct sp0838_fh  *fh = priv;
    tvin_parm_t para;
    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;

    para.port  = TVIN_PORT_CAMERA;
    para.fmt_info.fmt = TVIN_SIG_FMT_MAX+1;//TVIN_SIG_FMT_MAX+1;TVIN_SIG_FMT_CAMERA_1280X720P_30Hz
	para.fmt_info.frame_rate = 150;
	para.fmt_info.h_active = 640;
	para.fmt_info.v_active = 480;
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
    start_tvin_service(0,&para);
	    fh->stream_on        = 1;
	}
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct sp0838_fh  *fh = priv;

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
	struct sp0838_fh *fh = priv;
	struct sp0838_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct sp0838_fh *fh = priv;
	struct sp0838_device *dev = fh->dev;

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

	for (i = 0; i < ARRAY_SIZE(sp0838_qctrl); i++)
		if (qc->id && qc->id == sp0838_qctrl[i].id) {
			memcpy(qc, &(sp0838_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct sp0838_fh *fh = priv;
	struct sp0838_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(sp0838_qctrl); i++)
		if (ctrl->id == sp0838_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct sp0838_fh *fh = priv;
	struct sp0838_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(sp0838_qctrl); i++)
		if (ctrl->id == sp0838_qctrl[i].id) {
			if (ctrl->value < sp0838_qctrl[i].minimum ||
			    ctrl->value > sp0838_qctrl[i].maximum ||
			    sp0838_setting(dev,ctrl->id,ctrl->value)<0) {
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

static int sp0838_open(struct file *file)
{
	struct sp0838_device *dev = video_drvdata(file);
	struct sp0838_fh *fh = NULL;
	int retval = 0;
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	SP0838_init_regs(dev);
	msleep(40);
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
			
//    TVIN_SIG_FMT_CAMERA_640X480P_30Hz,
//    TVIN_SIG_FMT_CAMERA_800X600P_30Hz,
//    TVIN_SIG_FMT_CAMERA_1024X768P_30Hz, // 190
//    TVIN_SIG_FMT_CAMERA_1920X1080P_30Hz,
//    TVIN_SIG_FMT_CAMERA_1280X720P_30Hz,

	videobuf_queue_vmalloc_init(&fh->vb_vidq, &sp0838_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct sp0838_buffer), fh);

	sp0838_start_thread(fh);

	return 0;
}

static ssize_t
sp0838_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct sp0838_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
sp0838_poll(struct file *file, struct poll_table_struct *wait)
{
	struct sp0838_fh        *fh = file->private_data;
	struct sp0838_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int sp0838_close(struct file *file)
{
	struct sp0838_fh         *fh = file->private_data;
	struct sp0838_device *dev       = fh->dev;
	struct sp0838_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);

	sp0838_stop_thread(vidq);
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
	power_down_sp0838(dev);
#endif
	if(dev->platform_dev_data.device_uninit) {
		dev->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
	}
	return 0;
}

static int sp0838_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct sp0838_fh  *fh = file->private_data;
	struct sp0838_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations sp0838_fops = {
	.owner		= THIS_MODULE,
	.open           = sp0838_open,
	.release        = sp0838_close,
	.read           = sp0838_read,
	.poll		= sp0838_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = sp0838_mmap,
};

static const struct v4l2_ioctl_ops sp0838_ioctl_ops = {
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
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device sp0838_template = {
	.name		= "sp0838_v4l",
	.fops           = &sp0838_fops,
	.ioctl_ops 	= &sp0838_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int sp0838_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SP0838, 0);
}

static const struct v4l2_subdev_core_ops sp0838_core_ops = {
	.g_chip_ident = sp0838_g_chip_ident,
};

static const struct v4l2_subdev_ops sp0838_ops = {
	.core = &sp0838_core_ops,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void aml_sp0838_early_suspend(struct early_suspend *h)
{
	printk("enter -----> %s \n",__FUNCTION__);
	if(h && h->param) {
		aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)h->param;
		if (plat_dat && plat_dat->early_suspend) {
			plat_dat->early_suspend();
		}
	}
}

static void aml_sp0838_late_resume(struct early_suspend *h)
{
	printk("enter -----> %s \n",__FUNCTION__);
	if(h && h->param) {
		aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)h->param;
		if (plat_dat && plat_dat->late_resume) {
			plat_dat->late_resume();
		}
	}
}
#endif

static int sp0838_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int pgbuf;
	int err;
	struct sp0838_device *t;
	struct v4l2_subdev *sd;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &sp0838_ops);
	mutex_init(&t->mutex);

	/* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		kfree(client);
		return -ENOMEM;
	}
	memcpy(t->vdev, &sp0838_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);

	/* Register it */
	aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	if (plat_dat) {
		t->platform_dev_data.device_init=plat_dat->device_init;
		t->platform_dev_data.device_uninit=plat_dat->device_uninit;
		if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
		//if(t->platform_dev_data.device_init) {
		//t->platform_dev_data.device_init();
		//printk("+++found a init function, and run it..\n");
	//}
	}
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
    printk("******* enter itk early suspend register *******\n");
    sp0838_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	sp0838_early_suspend.suspend = aml_sp0838_early_suspend;
	sp0838_early_suspend.resume = aml_sp0838_late_resume;
	sp0838_early_suspend.param = plat_dat;
	register_early_suspend(&sp0838_early_suspend);
#endif

	return 0;
}

static int sp0838_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp0838_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	kfree(t);
	return 0;
}
static int sp0838_suspend(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp0838_device *t = to_dev(sd);	
	struct sp0838_fh  *fh = to_fh(t);
	if(fh->stream_on == 1){
		stop_tvin_service(0);
	}
	power_down_sp0838(t);
	return 0;
}

static int sp0838_resume(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp0838_device *t = to_dev(sd);
    struct sp0838_fh  *fh = to_fh(t);
    tvin_parm_t para;
    para.port  = TVIN_PORT_CAMERA;
    para.fmt_info.fmt = TVIN_SIG_FMT_CAMERA_1280X720P_30Hz;
    SP0838_init_regs(t); 
	if(fh->stream_on == 1){
        start_tvin_service(0,&para);
	}       	
	return 0;
}


static const struct i2c_device_id sp0838_id[] = {
	{ "sp0838_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sp0838_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "sp0838",
	.probe = sp0838_probe,
	.remove = sp0838_remove,
	.suspend = sp0838_suspend,
	.resume = sp0838_resume,		
	.id_table = sp0838_id,
};
