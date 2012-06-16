/*
 *gc0308 - This code emulates a real video device with v4l2 api
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
#include <mach/pinmux.h>
#include <linux/tvin/tvin.h>
#include "common/plat_ctrl.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend gc0308_early_suspend;
#endif

#define GC0308_CAMERA_MODULE_NAME "gc0308"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define GC0308_CAMERA_MAJOR_VERSION 0
#define GC0308_CAMERA_MINOR_VERSION 7
#define GC0308_CAMERA_RELEASE 0
#define GC0308_CAMERA_VERSION \
	KERNEL_VERSION(GC0308_CAMERA_MAJOR_VERSION, GC0308_CAMERA_MINOR_VERSION, GC0308_CAMERA_RELEASE)

MODULE_DESCRIPTION("gc0308 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static int gc0308_have_open=0;
extern int gl_vm_skip_count;

/* supported controls */
static struct v4l2_queryctrl gc0308_qctrl[] = {
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
		.id            = V4L2_CID_BLUE_BALANCE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "scene mode",
		.minimum       = 0,
		.maximum       = 1,
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

struct gc0308_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct gc0308_fmt formats[] = {
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
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV21,
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

static struct gc0308_fmt *get_format(struct v4l2_format *f)
{
	struct gc0308_fmt *fmt;
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
struct gc0308_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct gc0308_fmt        *fmt;
};

struct gc0308_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(gc0308_devicelist);

struct gc0308_device {
	struct list_head			gc0308_devicelist;
	struct v4l2_subdev			sd;
	struct v4l2_device			v4l2_dev;

	spinlock_t                 slock;
	struct mutex				mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct gc0308_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int			   input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;

	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(gc0308_qctrl)];
};

static inline struct gc0308_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc0308_device, sd);
}

struct gc0308_fh {
	struct gc0308_device            *dev;

	/* video capture */
	struct gc0308_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
};

static inline struct gc0308_fh *to_fh(struct gc0308_device *dev)
{
	return container_of(dev, struct gc0308_fh, dev);
}

static struct v4l2_frmsize_discrete gc0308_prev_resolution[2]= //should include 320x240 and 640x480, those two size are used for recording
{
	{352,288},
	{640,480},
};

static struct v4l2_frmsize_discrete gc0308_pic_resolution[1]=
{
	{640,480},
};

/* ------------------------------------------------------------------
	reg spec of GC0308
   ------------------------------------------------------------------*/

#if 1

struct aml_camera_i2c_fig1_s GC0308_script[] = {
    {0xfe,0x80},
    {0xfe,0x00},
    {0x22,0x55},
    {0x03,0x02},
    {0x04,0x58},//12c
    {0x5a,0x56},
    {0x5b,0x40},
    {0x5c,0x4a},
    {0x22,0x57},

#if 0
//25M mclk
#if 1   // 50hz  20fps
	{0x01 , 0x32},
	{0x02 , 0x89},
	{0x0f , 0x01},


	{0xe2 , 0x00},   //anti-flicker step [11:8]
	{0xe3 , 0x7d},   //anti-flicker step [7:0]

	{0xe4 , 0x02},
	{0xe5 , 0x71},
	{0xe6 , 0x02},
	{0xe7 , 0x71},
	{0xe8 , 0x02},
	{0xe9 , 0x71},
	{0xea , 0x0c},
	{0xeb , 0x35},
#else  // 60hz  20fps
	{0x01 , 0x92},
	{0x02 , 0x84},
	{0x0f , 0x00},


	{0xe2 , 0x00},   //anti-flicker step [11:8]
	{0xe3 , 0x7c},   //anti-flicker step [7:0]

	{0xe4 , 0x02},
	{0xe5 , 0x6c},
	{0xe6 , 0x02},
	{0xe7 , 0x6c},
	{0xe8 , 0x02},
	{0xe9 , 0x6c},
	{0xea , 0x0c},
	{0xeb , 0x1c},
#endif

#else

#if 1   // 50hz   24M MCLKauto
	{0x01 , 0x32},  //6a
	{0x02 , 0x70},
	{0x0f , 0x01},

	{0xe2 , 0x00},   //anti-flicker step [11:8]
	{0xe3 , 0x78},   //anti-flicker step [7:0]

	{0xe4 , 0x02},
	{0xe5 , 0x58},
	{0xe6 , 0x03},
	{0xe7 , 0x48},
	{0xe8 , 0x04},
	{0xe9 , 0xb0},
	{0xea , 0x05},
	{0xeb , 0xa0},
#else  // 60hz   8.3fps~16.6fps auto
	{0x01 , 0x32},
	{0x02 , 0x89},
	{0x0f , 0x01},


	{0xe2 , 0x00},   //anti-flicker step [11:8]
	{0xe3 , 0x68},   //anti-flicker step [7:0]

	{0xe4 , 0x02},
	{0xe5 , 0x71},
	{0xe6 , 0x02},
	{0xe7 , 0x71},
	{0xe8 , 0x04},
	{0xe9 , 0xe2},
	{0xea , 0x07},
	{0xeb , 0x53},
#endif

#endif


	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x02},
	{0x09,0x01},
	{0x0a,0xea},// ea
	{0x0b,0x02},
	{0x0c,0x88},
	{0x0d,0x02},
	{0x0e,0x02},
	{0x10,0x26},
	{0x11,0x0d},
	{0x12,0x2a},
	{0x13,0x00},
	{0x14,0x12}, // h_v
	{0x15,0x0a},
	{0x16,0x05},
	{0x17,0x01},
	{0x18,0x44},
	{0x19,0x44},
	{0x1a,0x2a},
	{0x1b,0x00},
	{0x1c,0x49},
	{0x1d,0x9a},
	{0x1e,0x61},
	{0x1f,0x16},
	{0x20,0xff},//ff
	{0x21,0xf8},//fa
	{0x22,0x57},
	{0x24,0xa3},
	{0x25,0x0f},
#ifdef CONFIG_ARCH_MESON3
	{0x26,0x01}, //03
#else
	{0x26,0x03}, //03
#endif
	{0x2f,0x01},
	{0x30,0xf7},
	{0x31,0x50},
	{0x32,0x00},
	{0x39,0x04},
	{0x3a,0x20},
	{0x3b,0x20},
	{0x3c,0x02},
	{0x3d,0x02}, //0x00
	{0x3e,0x02},
	{0x3f,0x02},
	{0x50,0x14}, // 0x14
	{0x53,0x80},
	{0x54,0x87},
	{0x55,0x87},
	{0x56,0x80},

	{0x57,0x7a},// r ratio
	{0x58,0x7e},// g ratio
	{0x59,0x84},//b ratio

	{0x8b,0x10},
	{0x8c,0x10},
	{0x8d,0x10},
	{0x8e,0x10},
	{0x8f,0x10},
	{0x90,0x10},
	{0x91,0x3c},
	{0x92,0x50},
	{0x5d,0x12},
	{0x5e,0x1a},
	{0x5f,0x24},
	{0x60,0x07},
	{0x61,0x15},
	{0x62,0x08}, // 0x08
	{0x64,0x03},  // 0x03
	{0x66,0xe8},
	{0x67,0x86},
	{0x68,0xa2},
	{0x69,0x18},
	{0x6a,0x0f},
	{0x6b,0x00},
	{0x6c,0x5f},
	{0x6d,0x8f},
	{0x6e,0x55},
	{0x6f,0x38},
	{0x70,0x15},
	{0x71,0x33}, // low light startion
	{0x72,0xdc},
	{0x73,0x80},
	{0x74,0x02},
	{0x75,0x3f},
	{0x76,0x02},
	{0x77,0x45}, // 0x47 //0x54
	{0x78,0x88},
	{0x79,0x81},
	{0x7a,0x81},
	{0x7b,0x22},
	{0x7c,0xff},
///////CC/////
	{0x93,0x42},  //0x48
	{0x94,0x00},
	{0x95,0x0c},//04
	{0x96,0xe0},
	{0x97,0x46},
	{0x98,0xf3},

	{0xb1,0x40},// startion
	{0xb2,0x40},
	{0xb3,0x3c}, //0x40  contrast
	{0xb5,0x00}, //
	{0xb6,0xe0},
	{0xbd,0x3C},
	{0xbe,0x36},
	{0xd0,0xCb},//c9
	{0xd1,0x10},
	{0xd2,0x90},
	{0xd3,0x50},//88
	{0xd5,0xF2},
	{0xd6,0x10},
	{0xdb,0x92},
	{0xdc,0xA5},
	{0xdf,0x23},
	{0xd9,0x00},
	{0xda,0x00},
	{0xe0,0x09},
	{0xed,0x04},
	{0xee,0xa0},
	{0xef,0x40},
	{0x80,0x03},
	/*******************
	{0x9F, 0x0B},//case 1   //smallest gamma curve
	{0xA0, 0x16},
	{0xA1, 0x29},
	{0xA2, 0x3C},
	{0xA3, 0x4F},
	{0xA4, 0x5F},
	{0xA5, 0x6F},
	{0xA6, 0x8A},
	{0xA7, 0x9F},
	{0xA8, 0xB4},
	{0xA9, 0xC6},
	{0xAA, 0xD3},
	{0xAB, 0xDD},
	{0xAC, 0xE5},
	{0xAD, 0xF1},
	{0xAE, 0xFA},
	{0xAF, 0xFF},


	{0x9F, 0x0E},//	case 2
	{0xA0, 0x1C},
	{0xA1, 0x34},
	{0xA2, 0x48},
	{0xA3, 0x5A},
	{0xA4, 0x6B},
	{0xA5, 0x7B},
	{0xA6, 0x95},
	{0xA7, 0xAB},
	{0xA8, 0xBF},
	{0xA9, 0xCE},
	{0xAA, 0xD9},
	{0xAB, 0xE4},
	{0xAC, 0xEC},
	{0xAD, 0xF7},
	{0xAE, 0xFD},
	{0xAF, 0xFF},


	{0x9F, 0x10},//		case 3:
	{0xA0, 0x20},
	{0xA1, 0x38},
	{0xA2, 0x4E},
	{0xA3, 0x63},
	{0xA4, 0x76},
	{0xA5, 0x87},
	{0xA6, 0xA2},
	{0xA7, 0xB8},
	{0xA8, 0xCA},
	{0xA9, 0xD8},
	{0xAA, 0xE3},
	{0xAB, 0xEB},
	{0xAC, 0xF0},
	{0xAD, 0xF8},
	{0xAE, 0xFD},
	{0xAF, 0xFF},



	{0x9F, 0x14},//		case 4:
	{0xA0, 0x28},
	{0xA1, 0x44},
	{0xA2, 0x5D},
	{0xA3, 0x72},
	{0xA4, 0x86},
	{0xA5, 0x95},
	{0xA6, 0xB1},
	{0xA7, 0xC6},
	{0xA8, 0xD5},
	{0xA9, 0xE1},
	{0xAA, 0xEA},
	{0xAB, 0xF1},
	{0xAC, 0xF5},
	{0xAD, 0xFB},
	{0xAE, 0xFE},
	{0xAF, 0xFF},


	{0x9F, 0x15},//	case 5:	 largest gamma curve
	{0xA0, 0x2A},
	{0xA1, 0x4A},
	{0xA2, 0x67},
	{0xA3, 0x79},
	{0xA4, 0x8C},
	{0xA5, 0x9A},
	{0xA6, 0xB3},
	{0xA7, 0xC5},
	{0xA8, 0xD5},
	{0xA9, 0xDF},
	{0xAA, 0xE8},
	{0xAB, 0xEE},
	{0xAC, 0xF3},
	{0xAD, 0xFA},
	{0xAE, 0xFD},
	{0xAF, 0xFF},
	********************/

	{0x9F, 0x10},//		case 3:
	{0xA0, 0x20},
	{0xA1, 0x38},
	{0xA2, 0x4E},
	{0xA3, 0x63},
	{0xA4, 0x76},
	{0xA5, 0x87},
	{0xA6, 0xA2},
	{0xA7, 0xB8},
	{0xA8, 0xCA},
	{0xA9, 0xD8},
	{0xAA, 0xE3},
	{0xAB, 0xEB},
	{0xAC, 0xF0},
	{0xAD, 0xF8},
	{0xAE, 0xFD},
	{0xAF, 0xFF},

	{0xc0,0x00},
	{0xc1,0x14},
	{0xc2,0x21},
	{0xc3,0x36},
	{0xc4,0x49},
	{0xc5,0x5B},
	{0xc6,0x6B},
	{0xc7,0x7B},
	{0xc8,0x98},
	{0xc9,0xB4},
	{0xca,0xCE},
	{0xcb,0xE8},
	{0xcc,0xFF},
	{0xf0,0x02},
	{0xf1,0x01},
	{0xf2,0x01},
	{0xf3,0x30},
	{0xf9,0x9f},
	{0xfa,0x78},
	{0xfe,0x01},
	{0x00,0xf5},
	{0x02,0x20},
	{0x04,0x10},
	{0x05,0x10},
	{0x06,0x20},
	{0x08,0x15},
	{0x0a,0xa0},
	{0x0b,0x64},
	{0x0c,0x08},
	{0x0e,0x4C},
	{0x0f,0x39},
	{0x10,0x41},
	{0x11,0x37},
	{0x12,0x24},
	{0x13,0x39},
	{0x14,0x45},
	{0x15,0x45},
	{0x16,0xc2},
	{0x17,0xA8},
	{0x18,0x18},
	{0x19,0x55},
	{0x1a,0xd8},
	{0x1b,0xf5},

	{0x1c,0x60}, //r gain limit

	{0x70,0x40},
	{0x71,0x58},
	{0x72,0x30},
	{0x73,0x48},
	{0x74,0x20},
	{0x75,0x60},
	{0x77,0x20},
	{0x78,0x32},
	{0x30,0x03},
	{0x31,0x40},
	{0x32,0x10},
	{0x33,0xe0},
	{0x34,0xe0},
	{0x35,0x00},
	{0x36,0x80},
	{0x37,0x00},
	{0x38,0x04},
	{0x39,0x09},
	{0x3a,0x12},
	{0x3b,0x1C},
	{0x3c,0x28},
	{0x3d,0x31},
	{0x3e,0x44},
	{0x3f,0x57},
	{0x40,0x6C},
	{0x41,0x81},
	{0x42,0x94},
	{0x43,0xA7},
	{0x44,0xB8},
	{0x45,0xD6},
	{0x46,0xEE},
	{0x47,0x0d},
	{0xfe , 0x00},// update

	{0x10 , 0x26},
	{0x11 , 0x0d},  // fd,modified by mormo 2010/07/06
	{0x1a , 0x2a},  // 1e,modified by mormo 2010/07/06

	{0x1c , 0x49}, // c1,modified by mormo 2010/07/06
	{0x1d , 0x9a}, // 08,modified by mormo 2010/07/06
	{0x1e , 0x61}, // 60,modified by mormo 2010/07/06

	{0x3a , 0x20},

	{0x50 , 0x14},  // 10,modified by mormo 2010/07/06
	{0x53 , 0x80},
	{0x56 , 0x80},

	{0x8b , 0x20}, //LSC
	{0x8c , 0x20},
	{0x8d , 0x20},
	{0x8e , 0x14},
	{0x8f , 0x10},
	{0x90 , 0x14},

	{0x94 , 0x02},
	{0x95 , 0x0c}, //0x07
	{0x96 , 0xe0},

	{0xb1 , 0x40}, // YCPT
	{0xb2 , 0x40},
	{0xb3 , 0x3c},
	{0xb6 , 0xe0},

	{0xd0 , 0xcb}, // AECT  c9,modifed by mormo 2010/07/06
	{0xd3 , 0x50}, // 80,modified by mormor 2010/07/06   58

	{0xf2 , 0x02},
	{0xf7 , 0x12},
	{0xf8 , 0x0a},
	//Registers of Page1
	{0xfe , 0x01},

	{0x02 , 0x20},
	{0x04 , 0x10},
	{0x05 , 0x08},
	{0x06 , 0x20},
	{0x08 , 0x0a},

	{0x0e , 0x44},
	{0x0f , 0x32},
	{0x10 , 0x41},
	{0x11 , 0x37},
	{0x12 , 0x22},
	{0x13 , 0x19},
	{0x14 , 0x44},
	{0x15 , 0x44},

	{0x19 , 0x50},
	{0x1a , 0xd8},

	{0x32 , 0x10},

	{0x35 , 0x00},
	{0x36 , 0x80},
	{0x37 , 0x00},
	//-----------Update the registers end---------//
	{0xfe,0x00},
	{0xfe,0x00},
	{0xff,0xff},
};

//load GT2005 parameters
void GC0308_init_regs(struct gc0308_device *dev)
{
    int i=0;//,j;
    unsigned char buf[2];
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    while(1)
    {
        buf[0] = GC0308_script[i].addr;//(unsigned char)((GC0308_script[i].addr >> 8) & 0xff);
        //buf[1] = (unsigned char)(GC0308_script[i].addr & 0xff);
        buf[1] = GC0308_script[i].val;
        if(GC0308_script[i].val==0xff&&GC0308_script[i].addr==0xff){
            printk("GC0308_write_regs success in initial GC0308.\n");
            break;
        }
        if((i2c_put_byte_add8(client,buf, 2)) < 0){
            printk("fail in initial GC0308. \n");
            return;
        }
        i++;
    }
    aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
    if (plat_dat&&plat_dat->custom_init_script) {
        i=0;
        aml_camera_i2c_fig1_t*  custom_script = (aml_camera_i2c_fig1_t*)plat_dat->custom_init_script;
        while(1)
        {
            buf[0] = custom_script[i].addr;
            buf[1] = custom_script[i].val;
            if (custom_script[i].val==0xff&&custom_script[i].addr==0xff){
                printk("GC0308_write_custom_regs success in initial GC0308.\n");
                break;
            }
            if((i2c_put_byte_add8(client,buf, 2)) < 0){
                printk("fail in initial GC0308 custom_regs. \n");
                return;
            }
            i++;
        }
    }
    return;

}

#endif

/*************************************************************************
* FUNCTION
*	set_GC0308_param_wb
*
* DESCRIPTION
*	GC0308 wb setting.
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
void set_GC0308_param_wb(struct gc0308_device *dev,enum  camera_wb_flip_e para)
{
//	kal_uint16 rgain=0x80, ggain=0x80, bgain=0x80;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	unsigned char buf[4];

	unsigned char  temp_reg;
	//temp_reg=gc0308_read_byte(0x22);
	buf[0]=0x22;
	temp_reg=i2c_get_byte_add8(client,buf);

	printk(" camera set_GC0308_param_wb=%d. \n ",para);
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
*	GC0308_night_mode
*
* DESCRIPTION
*	This function night mode of GC0308.
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
void GC0308_night_mode(struct gc0308_device *dev,enum  camera_night_mode_flip_e enable)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[4];

	unsigned char  temp_reg;
	//temp_reg=gc0308_read_byte(0x22);
	buf[0]=0x20;
	temp_reg=i2c_get_byte_add8(client,buf);
	temp_reg=0xff;

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
*	GC0308_night_mode
*
* DESCRIPTION
*	This function night mode of GC0308.
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

void GC0308_set_param_banding(struct gc0308_device *dev,enum  camera_night_mode_flip_e banding)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    unsigned char buf[4];
    switch(banding){
        case CAM_BANDING_60HZ:
            buf[0]=0x01;
            buf[1]=0x32;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0x02;
            buf[1]=0x70;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0x0f;
            buf[1]=0x01;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe3;
            buf[1]=0x64;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe4;
            buf[1]=0x02;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe5;
            buf[1]=0x58;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe6;
            buf[1]=0x03;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe7;
            buf[1]=0x84;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe8;
            buf[1]=0x04;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe9;
            buf[1]=0xb0;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xea;
            buf[1]=0x05;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xeb;
            buf[1]=0xdc;
            i2c_put_byte_add8(client,buf,2);
            break;
        case CAM_BANDING_50HZ:
            buf[0]=0x01;
            buf[1]=0x32;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0x02;
            buf[1]=0x70;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0x0f;
            buf[1]=0x01;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe3;
            buf[1]=0x78;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe4;
            buf[1]=0x02;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe5;
            buf[1]=0x58;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe6;
            buf[1]=0x03;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe7;
            buf[1]=0x48;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe8;
            buf[1]=0x04;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xe9;
            buf[1]=0xb0;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xea;
            buf[1]=0x05;
            i2c_put_byte_add8(client,buf,2);
            buf[0]=0xeb;
            buf[1]=0xa0;
            i2c_put_byte_add8(client,buf,2);
            break;
    }
}


/*************************************************************************
* FUNCTION
*	set_GC0308_param_exposure
*
* DESCRIPTION
*	GC0308 exposure setting.
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
void set_GC0308_param_exposure(struct gc0308_device *dev,enum camera_exposure_e para)//曝光调节
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	unsigned char buf1[2];
	unsigned char buf2[2];

	switch (para)
	{
			case EXPOSURE_N4_STEP:
			buf1[0]=0xb5;
			buf1[1]=0xc0;
			buf2[0]=0xd3;
			buf2[1]=0x30;
			break;
		case EXPOSURE_N3_STEP:
			buf1[0]=0xb5;
			buf1[1]=0xd0;
			buf2[0]=0xd3;
			buf2[1]=0x38;
			break;
		case EXPOSURE_N2_STEP:
			buf1[0]=0xb5;
			buf1[1]=0xe0;
			buf2[0]=0xd3;
			buf2[1]=0x40;
			break;
		case EXPOSURE_N1_STEP:
			buf1[0]=0xb5;
			buf1[1]=0xf0;
			buf2[0]=0xd3;
			buf2[1]=0x48;
			break;
		case EXPOSURE_0_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x08;//00
			buf2[0]=0xd3;
			buf2[1]=0x50;//6a
			break;
		case EXPOSURE_P1_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x10;
			buf2[0]=0xd3;
			buf2[1]=0x5c;
			break;
		case EXPOSURE_P2_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x20;
			buf2[0]=0xd3;
			buf2[1]=0x60;
			break;
		case EXPOSURE_P3_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x30;
			buf2[0]=0xd3;
			buf2[1]=0x68;
			break;
		case EXPOSURE_P4_STEP:
			buf1[0]=0xb5;
			buf1[1]=0x40;
			buf2[0]=0xd3;
			buf2[1]=0x70;
			break;
		default:
			buf1[0]=0xb5;
			buf1[1]=0x00;
			buf2[0]=0xd3;
			buf2[1]=0x50;
			break;
	}
	//msleep(300);
	i2c_put_byte_add8(client,buf1,2);
	i2c_put_byte_add8(client,buf2,2);

}

/*************************************************************************
* FUNCTION
*	set_GC0308_param_effect
*
* DESCRIPTION
*	GC0308 effect setting.
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
void set_GC0308_param_effect(struct gc0308_device *dev,enum camera_effect_flip_e para)//特效设置
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
		    buf[1]=0x3c;
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

unsigned char v4l_2_gc0308(int val)
{
	int ret=val/0x20;
	if(ret<4) return ret*0x20+0x80;
	else if(ret<8) return ret*0x20+0x20;
	else return 0;
}

static int gc0308_setting(struct gc0308_device *dev,int PROP_ID,int value )
{
	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
#if 0
	case V4L2_CID_BRIGHTNESS:
		dprintk(dev, 1, "setting brightned:%d\n",v4l_2_gc0308(value));
		ret=i2c_put_byte(client,0x0201,v4l_2_gc0308(value));
		break;
	case V4L2_CID_CONTRAST:
		ret=i2c_put_byte(client,0x0200, value);
		break;
	case V4L2_CID_SATURATION:
		ret=i2c_put_byte(client,0x0202, value);
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
		if(gc0308_qctrl[0].default_value!=value){
			gc0308_qctrl[0].default_value=value;
			set_GC0308_param_wb(dev,value);
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
		}
		break;
	case V4L2_CID_EXPOSURE:
		if(gc0308_qctrl[1].default_value!=value){
			gc0308_qctrl[1].default_value=value;
			set_GC0308_param_exposure(dev,value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
		}
		break;
	case V4L2_CID_COLORFX:
		if(gc0308_qctrl[2].default_value!=value){
			gc0308_qctrl[2].default_value=value;
			set_GC0308_param_effect(dev,value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
		}
		break;
	case V4L2_CID_WHITENESS:
		if(gc0308_qctrl[3].default_value!=value){
			gc0308_qctrl[3].default_value=value;
			GC0308_set_param_banding(dev,value);
			printk(KERN_INFO " set camera  banding=%d. \n ",value);
		}
		break;
	case V4L2_CID_BLUE_BALANCE:
		if(gc0308_qctrl[4].default_value!=value){
			gc0308_qctrl[4].default_value=value;
			GC0308_night_mode(dev,value);
			printk(KERN_INFO " set camera  scene mode=%d. \n ",value);
		}
		break;
	default:
		ret=-1;
		break;
	}
	return ret;

}

static void power_down_gc0308(struct gc0308_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[4];
	buf[0]=0x1a;
	buf[1]=0x17;
	i2c_put_byte_add8(client,buf,2);
	buf[0]=0x25;
	buf[1]=0x00;
	i2c_put_byte_add8(client,buf,2);
	
	msleep(5);
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

static void gc0308_fillbuff(struct gc0308_fh *fh, struct gc0308_buffer *buf)
{
	struct gc0308_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	dprintk(dev,1,"%s\n", __func__);
	if (!vbuf)
		return;
 /*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
    vm_fill_buffer(&buf->vb,fh->fmt->fourcc ,0x18221223,vbuf);
	buf->vb.state = VIDEOBUF_DONE;
}

static void gc0308_thread_tick(struct gc0308_fh *fh)
{
	struct gc0308_buffer *buf;
	struct gc0308_device *dev = fh->dev;
	struct gc0308_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 1, "No active queue to serve\n");
		goto unlock;
	}

	buf = list_entry(dma_q->active.next,
			 struct gc0308_buffer, vb.queue);
    dprintk(dev, 1, "%s\n", __func__);
    dprintk(dev, 1, "list entry get buf is %x\n",(unsigned)buf);

	/* Nobody is waiting on this buffer, return */
	if (!waitqueue_active(&buf->vb.done))
		goto unlock;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	gc0308_fillbuff(fh, buf);
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

static void gc0308_sleep(struct gc0308_fh *fh)
{
	struct gc0308_device *dev = fh->dev;
	struct gc0308_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	//timeout = msecs_to_jiffies(frames_to_ms(1));

	gc0308_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int gc0308_thread(void *data)
{
	struct gc0308_fh  *fh = data;
	struct gc0308_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		gc0308_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int gc0308_start_thread(struct gc0308_fh *fh)
{
	struct gc0308_device *dev = fh->dev;
	struct gc0308_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(gc0308_thread, fh, "gc0308");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void gc0308_stop_thread(struct gc0308_dmaqueue  *dma_q)
{
	struct gc0308_device *dev = container_of(dma_q, struct gc0308_device, vidq);

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
	struct gc0308_fh  *fh = vq->priv_data;
	struct gc0308_device *dev  = fh->dev;
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

static void free_buffer(struct videobuf_queue *vq, struct gc0308_buffer *buf)
{
	struct gc0308_fh  *fh = vq->priv_data;
	struct gc0308_device *dev  = fh->dev;

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
	struct gc0308_fh     *fh  = vq->priv_data;
	struct gc0308_device    *dev = fh->dev;
	struct gc0308_buffer *buf = container_of(vb, struct gc0308_buffer, vb);
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
	struct gc0308_buffer    *buf  = container_of(vb, struct gc0308_buffer, vb);
	struct gc0308_fh        *fh   = vq->priv_data;
	struct gc0308_device       *dev  = fh->dev;
	struct gc0308_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct gc0308_buffer   *buf  = container_of(vb, struct gc0308_buffer, vb);
	struct gc0308_fh       *fh   = vq->priv_data;
	struct gc0308_device      *dev  = (struct gc0308_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops gc0308_video_qops = {
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
	struct gc0308_fh  *fh  = priv;
	struct gc0308_device *dev = fh->dev;

	strcpy(cap->driver, "gc0308");
	strcpy(cap->card, "gc0308");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = GC0308_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct gc0308_fmt *fmt;

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
	struct gc0308_fh *fh = priv;

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
	struct gc0308_fh  *fh  = priv;
	struct gc0308_device *dev = fh->dev;
	struct gc0308_fmt *fmt;
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
	struct gc0308_fh *fh = priv;
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
	struct gc0308_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0308_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0308_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct gc0308_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct gc0308_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct gc0308_fh  *fh = priv;
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
	para.fmt_info.hsync_phase = 0;
	para.fmt_info.vsync_phase  = 1;	
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
    start_tvin_service(0,&para);
	    fh->stream_on        = 1;
	}
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct gc0308_fh  *fh = priv;

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
	struct gc0308_fmt *fmt = NULL;
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
		if (fsize->index >= ARRAY_SIZE(gc0308_prev_resolution))
			return -EINVAL;
		frmsize = &gc0308_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
		if (fsize->index >= ARRAY_SIZE(gc0308_pic_resolution))
			return -EINVAL;
		frmsize = &gc0308_pic_resolution[fsize->index];
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
	struct gc0308_fh *fh = priv;
	struct gc0308_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct gc0308_fh *fh = priv;
	struct gc0308_device *dev = fh->dev;

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

	for (i = 0; i < ARRAY_SIZE(gc0308_qctrl); i++)
		if (qc->id && qc->id == gc0308_qctrl[i].id) {
			memcpy(qc, &(gc0308_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct gc0308_fh *fh = priv;
	struct gc0308_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0308_qctrl); i++)
		if (ctrl->id == gc0308_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct gc0308_fh *fh = priv;
	struct gc0308_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gc0308_qctrl); i++)
		if (ctrl->id == gc0308_qctrl[i].id) {
			if (ctrl->value < gc0308_qctrl[i].minimum ||
			    ctrl->value > gc0308_qctrl[i].maximum ||
			    gc0308_setting(dev,ctrl->id,ctrl->value)<0) {
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

static int gc0308_open(struct file *file)
{
	struct gc0308_device *dev = video_drvdata(file);
	struct gc0308_fh *fh = NULL;
	int retval = 0;
	gc0308_have_open=1;
	gl_vm_skip_count=0;
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	GC0308_init_regs(dev);
	msleep(100);//40
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

	videobuf_queue_vmalloc_init(&fh->vb_vidq, &gc0308_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct gc0308_buffer), fh);

	gc0308_start_thread(fh);

	return 0;
}

static ssize_t
gc0308_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct gc0308_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
gc0308_poll(struct file *file, struct poll_table_struct *wait)
{
	struct gc0308_fh        *fh = file->private_data;
	struct gc0308_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int gc0308_close(struct file *file)
{
	struct gc0308_fh         *fh = file->private_data;
	struct gc0308_device *dev       = fh->dev;
	struct gc0308_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	gc0308_have_open=0;

	gc0308_stop_thread(vidq);
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
	gc0308_qctrl[0].default_value=0;
	gc0308_qctrl[1].default_value=4;
	gc0308_qctrl[2].default_value=0;
	gc0308_qctrl[3].default_value=0;
	gc0308_qctrl[4].default_value=0;

	//power_down_gc0308(dev);
#endif
	if(dev->platform_dev_data.device_uninit) {
		dev->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
	}
	return 0;
}

static int gc0308_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gc0308_fh  *fh = file->private_data;
	struct gc0308_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations gc0308_fops = {
	.owner		= THIS_MODULE,
	.open           = gc0308_open,
	.release        = gc0308_close,
	.read           = gc0308_read,
	.poll		= gc0308_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = gc0308_mmap,
};

static const struct v4l2_ioctl_ops gc0308_ioctl_ops = {
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
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device gc0308_template = {
	.name		= "gc0308_v4l",
	.fops           = &gc0308_fops,
	.ioctl_ops 	= &gc0308_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int gc0308_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC0308, 0);
}

static const struct v4l2_subdev_core_ops gc0308_core_ops = {
	.g_chip_ident = gc0308_g_chip_ident,
};

static const struct v4l2_subdev_ops gc0308_ops = {
	.core = &gc0308_core_ops,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void aml_gc0308_early_suspend(struct early_suspend *h)
{
	printk("enter -----> %s \n",__FUNCTION__);
	if(h && h->param) {
		aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)h->param;
		if (plat_dat && plat_dat->early_suspend) {
			plat_dat->early_suspend();
		}
	}
}
static struct i2c_client *this_client;

static void aml_gc0308_late_resume(struct early_suspend *h)
{
	aml_plat_cam_data_t* plat_dat;
	if(gc0308_have_open==0){
		printk("enter -----> %s \n",__FUNCTION__);
		if(h && h->param) {
			plat_dat= (aml_plat_cam_data_t*)h->param;
			if (plat_dat && plat_dat->late_resume) {
				plat_dat->late_resume();
			}
		}
		/*unsigned char buf[4];	    
		buf[0]=0x25;	    
		buf[1]=0x00;	    
		i2c_put_byte_add8(this_client,buf,2);		
		buf[0]=0x1a;	    
		buf[1]=0x17;	    
		i2c_put_byte_add8(this_client,buf,2);*/
		if(plat_dat->device_uninit) {
		plat_dat->device_uninit();
		printk("+++found a uninit function, and run it..\n");
	    }
		}
	else{
		if (plat_dat && plat_dat->device_init) {
				plat_dat->device_init();
			}
		}
}
#endif

static int gc0308_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	aml_plat_cam_data_t* plat_dat;
	int err;
	struct gc0308_device *t;
	struct v4l2_subdev *sd;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &gc0308_ops);
	plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	
	/* test if devices exist. */
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_PROBE
	unsigned char buf[4]; 
	buf[0]=0;
	plat_dat->device_init();
	err=i2c_get_byte_add8(client,buf);
	plat_dat->device_uninit();
	if(err<0) return  -ENODEV;
#endif
	/* Now create a video4linux device */
	mutex_init(&t->mutex);

	/* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		kfree(client);
		return -ENOMEM;
	}
	memcpy(t->vdev, &gc0308_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);

	this_client=client;

	/* Register it */
	if (plat_dat) {
		t->platform_dev_data.device_init=plat_dat->device_init;
		t->platform_dev_data.device_uninit=plat_dat->device_uninit;
		if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
			if(t->platform_dev_data.device_init) {
			t->platform_dev_data.device_init();
			printk("+++found a init function, and run it..\n");
		    }
			//power_down_gc0308(t);
			if(t->platform_dev_data.device_uninit) {
			t->platform_dev_data.device_uninit();
			printk("+++found a uninit function, and run it..\n");
		    }
	}
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
    printk("******* enter itk early suspend register *******\n");
    gc0308_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	gc0308_early_suspend.suspend = aml_gc0308_early_suspend;
	gc0308_early_suspend.resume = aml_gc0308_late_resume;
	gc0308_early_suspend.param = plat_dat;
	register_early_suspend(&gc0308_early_suspend);
#endif

	return 0;
}

static int gc0308_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0308_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	kfree(t);
	return 0;
}
static int gc0308_suspend(struct i2c_client *client, pm_message_t state)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0308_device *t = to_dev(sd);
	struct gc0308_fh  *fh = to_fh(t);
	if(fh->stream_on == 1){
		stop_tvin_service(0);
	}
	power_down_gc0308(t);
	return 0;
}

static int gc0308_resume(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0308_device *t = to_dev(sd);
    struct gc0308_fh  *fh = to_fh(t);
    tvin_parm_t para;
    para.port  = TVIN_PORT_CAMERA;
    para.fmt_info.fmt = TVIN_SIG_FMT_MAX+1;
    GC0308_init_regs(t); 
	if(fh->stream_on == 1){
        start_tvin_service(0,&para);
	}
	return 0;
}


static const struct i2c_device_id gc0308_id[] = {
	{ "gc0308_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc0308_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "gc0308",
	.probe = gc0308_probe,
	.remove = gc0308_remove,
	.suspend = gc0308_suspend,
	.resume = gc0308_resume,
	.id_table = gc0308_id,
};

