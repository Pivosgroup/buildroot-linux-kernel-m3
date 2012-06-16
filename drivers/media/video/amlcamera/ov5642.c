/*
 *OV5642 - This code emulates a real video device with v4l2 api
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

#define OV5642_CAMERA_MODULE_NAME "ov5642"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define OV5642_CAMERA_MAJOR_VERSION 0
#define OV5642_CAMERA_MINOR_VERSION 7
#define OV5642_CAMERA_RELEASE 0
#define OV5642_CAMERA_VERSION \
	KERNEL_VERSION(OV5642_CAMERA_MAJOR_VERSION, OV5642_CAMERA_MINOR_VERSION, OV5642_CAMERA_RELEASE)

MODULE_DESCRIPTION("ov5642 On Board");
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
static struct v4l2_queryctrl ov5642_qctrl[] = {
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
	}
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct ov5642_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct ov5642_fmt formats[] = {
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
		.name     = "12  Y/CbCr 4:2:0SP",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,	
	},
	{
		.name     = "12  Y/CbCr 4:2:0SP",
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

static struct ov5642_fmt *get_format(struct v4l2_format *f)
{
	struct ov5642_fmt *fmt;
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
struct ov5642_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct ov5642_fmt        *fmt;
};

struct ov5642_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(ov5642_devicelist);

struct ov5642_device {
	struct list_head			ov5642_devicelist;
	struct v4l2_subdev			sd;
	struct v4l2_device			v4l2_dev;

	spinlock_t                 slock;
	struct mutex				mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct ov5642_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int			   input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;
	
	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(ov5642_qctrl)];
};

static inline struct ov5642_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov5642_device, sd);
}

struct ov5642_fh {
	struct ov5642_device            *dev;

	/* video capture */
	struct ov5642_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
};

/* ------------------------------------------------------------------
	reg spec of OV5642
   ------------------------------------------------------------------*/

struct aml_camera_i2c_fig_s OV5642_script[] = {
//OV5642ISPCfgInfo
    { 0x3103, 0x93},
    { 0x3008, 0x82},
    { 0x3017, 0x7f},
    { 0x3018, 0xfc},
    { 0x3810, 0xc2},
    { 0x3615, 0xf0},
    //system and IO pad control
    { 0x3000, 0x00},
    { 0x3001, 0x00},
    { 0x3002, 0x5c},
    { 0x3003, 0x00},
    { 0x3004, 0xff},
    { 0x3005, 0xff},
    { 0x3006, 0x43},
    { 0x3007, 0x37},
    { 0x3011, 0x08},
    { 0x3010, 0x00},
    ///////////////////////////
    { 0x460c, 0x22},
    { 0x3815, 0x04},

    //sensor control
    { 0x370c, 0xa0},
    { 0x3602, 0xfc},
    { 0x3612, 0xff},
    { 0x3634, 0xc0},
    { 0x3613, 0x00},
    { 0x3605, 0x7c},
    { 0x3621, 0x09},
    { 0x3622, 0x60},
    { 0x3604, 0x40},
    { 0x3603, 0xa7},
    { 0x3603, 0x27},
    //////////////////////////
    //BLD
    { 0x4000, 0x21},
    { 0x401d, 0x22},
    //////////////////////////
    //sensor control
    { 0x3600, 0x54},
    { 0x3605, 0x04},
    { 0x3606, 0x3f},
    ///////////////////////////
    { 0x3c01, 0x80},
    { 0x5000, 0x47},//0x4f,
    { 0x5020, 0x04},

    //{ 0x5181, 0x79}
    //{ 0x5182, 0x00}
    //{ 0x5185, 0x22}
    //{ 0x5197, 0x01}
    { 0x5001, 0xff},
    { 0x5500, 0x0a},
    { 0x5504, 0x00},
    { 0x5505, 0x7f},
    { 0x5080, 0x08},
    { 0x300e, 0x18},
    { 0x4610, 0x00},
    { 0x471d, 0x05},
    { 0x4708, 0x06},
    { 0x3808, 0x02},
    { 0x3809, 0x80},
    { 0x380a, 0x01},
    { 0x380b, 0xe0},
    { 0x380e, 0x07},
    { 0x380f, 0xd0},
    { 0x501f, 0x00},
    { 0x5000, 0x47},//0x4f,
    { 0x4300, 0x31},

    { 0x3503, 0x07},//Manual

    { 0x3501, 0x73},
    { 0x3502, 0x80},
    { 0x350b, 0x00},
    { 0x3503, 0x07},
    { 0x3824, 0x11},
    { 0x3501, 0x1e},
    { 0x3502, 0x80},
    { 0x350b, 0x7f},
    { 0x380c, 0x0c},
    { 0x380d, 0x80},
    { 0x380e, 0x03},
    { 0x380f, 0xe8},
    { 0x3a0d, 0x04},
    { 0x3a0e, 0x03},
    { 0x3818, 0xa1},
    { 0x3705, 0xdb},
    { 0x370a, 0x81},
    { 0x3801, 0x80},
    { 0x3621, 0xa7},
    { 0x3801, 0x50},
    { 0x3803, 0x08},
    { 0x3827, 0x08},
    { 0x3810, 0x40},
    { 0x3804, 0x05},
    { 0x3805, 0x00},
    { 0x5682, 0x05},
    { 0x5683, 0x00},
    { 0x3806, 0x03},
    { 0x3807, 0xc0},
    { 0x5686, 0x03},
    { 0x5687, 0xbc},
    { 0x3a00, 0x78},
    { 0x3a1a, 0x05},
    { 0x3a13, 0x30},
    { 0x3a18, 0x00},
    { 0x3a19, 0x7c},
    { 0x3a08, 0x12},
    { 0x3a09, 0xc0},
    { 0x3a0a, 0x0f},
    { 0x3a0b, 0xa0},
    { 0x350c, 0x07},
    { 0x350d, 0xd0},
    { 0x3500, 0x00},
    { 0x3501, 0x00},
    { 0x3502, 0x00},
    { 0x350a, 0x00},
    { 0x350b, 0x00},
    { 0x3503, 0x00},//Auto
    { 0x528a, 0x02},
    { 0x528b, 0x04},
    { 0x528c, 0x08},
    { 0x528d, 0x08},
    { 0x528e, 0x08},
    { 0x528f, 0x10},
    { 0x5290, 0x10},
    { 0x5292, 0x00},
    { 0x5293, 0x02},
    { 0x5294, 0x00},
    { 0x5295, 0x02},
    { 0x5296, 0x00},
    { 0x5297, 0x02},
    { 0x5298, 0x00},
    { 0x5299, 0x02},
    { 0x529a, 0x00},
    { 0x529b, 0x02},
    { 0x529c, 0x00},
    { 0x529d, 0x02},
    { 0x529e, 0x00},
    { 0x529f, 0x02},
    { 0x3a0f, 0x3c},
    { 0x3a10, 0x30},
    { 0x3a1b, 0x3c},
    { 0x3a1e, 0x30},
    { 0x3a11, 0x70},
    { 0x3a1f, 0x10},
    { 0x3030, 0x2b},
    { 0x3a02, 0x00},
    { 0x3a03, 0x7d},
    { 0x3a04, 0x00},
    { 0x3a14, 0x00},
    { 0x3a15, 0x7d},
    { 0x3a16, 0x00},
    { 0x3a00, 0x78},
    { 0x3a08, 0x09},
    { 0x3a09, 0x60},
    { 0x3a0a, 0x07},
    { 0x3a0b, 0xd0},
    { 0x3a0d, 0x08},
    { 0x3a0e, 0x06},
    { 0x5193, 0x70},
    { 0x589b, 0x04},
    { 0x589a, 0xc5},
    { 0x4001, 0x42},
    { 0x401c, 0x04},
    { 0x528a, 0x01},
    { 0x528b, 0x04},
    { 0x528c, 0x08},
    { 0x528d, 0x10},
    { 0x528e, 0x20},
    { 0x528f, 0x28},
    { 0x5290, 0x30},
    { 0x5292, 0x00},
    { 0x5293, 0x01},
    { 0x5294, 0x00},
    { 0x5295, 0x04},
    { 0x5296, 0x00},
    { 0x5297, 0x08},
    { 0x5298, 0x00},
    { 0x5299, 0x10},
    { 0x529a, 0x00},
    { 0x529b, 0x20},
    { 0x529c, 0x00},
    { 0x529d, 0x28},
    { 0x529e, 0x00},
    { 0x529f, 0x30},
    { 0x5282, 0x00},
    { 0x5300, 0x00},
    { 0x5301, 0x20},
    { 0x5302, 0x00},
    { 0x5303, 0x7c},
    { 0x530c, 0x00},
    { 0x530d, 0x0c},
    { 0x530e, 0x20},
    { 0x530f, 0x80},
    { 0x5310, 0x20},
    { 0x5311, 0x80},
    { 0x5308, 0x20},
    { 0x5309, 0x40},
    { 0x5304, 0x00},
    { 0x5305, 0x30},
    { 0x5306, 0x00},
    { 0x5307, 0x80},
    { 0x5314, 0x08},
    { 0x5315, 0x20},
    { 0x5319, 0x30},
    { 0x5316, 0x10},
    { 0x5317, 0x00},
    { 0x5318, 0x02},
    ///////////////////////////
//OV5642CMTXCfgInfo    
    { 0x5380, 0x01},
    { 0x5381, 0x00},
    { 0x5382, 0x00},
    { 0x5383, 0x17},
    { 0x5384, 0x00},
    { 0x5385, 0x01},
    { 0x5386, 0x00},
    { 0x5387, 0x00},
    { 0x5388, 0x01},
    { 0x5389, 0x16},
    { 0x538a, 0x00},
    { 0x538b, 0x38},
    { 0x538c, 0x00},
    { 0x538d, 0x00},
    { 0x538e, 0x00},
    { 0x538f, 0x09},
    { 0x5390, 0x00},
    { 0x5391, 0xb9},
    { 0x5392, 0x00},
    { 0x5393, 0x20},
    { 0x5394, 0x08},
//OV5642GammaCfgInfo	
{ 0x5480, 0x14},
{ 0x5481, 0x21},
{ 0x5482, 0x36},
{ 0x5483, 0x57},
{ 0x5484, 0x65},
{ 0x5485, 0x71},
{ 0x5486, 0x7d},
{ 0x5487, 0x87},
{ 0x5488, 0x91},
{ 0x5489, 0x9a},
{ 0x548a, 0xaa},
{ 0x548b, 0xb8},
{ 0x548c, 0xcd},
{ 0x548d, 0xdd},
{ 0x548e, 0xea},
{ 0x548f, 0x1d},

{ 0x5490, 0x05},
{ 0x5491, 0x00},
{ 0x5492, 0x04},
{ 0x5493, 0x20},
{ 0x5494, 0x03},
{ 0x5495, 0x60},
{ 0x5496, 0x02},
{ 0x5497, 0xb8},
{ 0x5498, 0x02},
{ 0x5499, 0x86},
{ 0x549a, 0x02},
{ 0x549b, 0x5b},
{ 0x549c, 0x02},
{ 0x549d, 0x3b},
{ 0x549e, 0x02},
{ 0x549f, 0x1c},

{ 0x54a0, 0x02},
{ 0x54a1, 0x04},
{ 0x54a2, 0x01},
{ 0x54a3, 0xed},
{ 0x54a4, 0x01},
{ 0x54a5, 0xc5},
{ 0x54a6, 0x01},
{ 0x54a7, 0xa5},
{ 0x54a8, 0x01},
{ 0x54a9, 0x6c},
{ 0x54aa, 0x01},
{ 0x54ab, 0x41},
{ 0x54ac, 0x01},
{ 0x54ad, 0x20},
{ 0x54ae, 0x00},
{ 0x54af, 0x16},

{ 0x54b0, 0x01},
{ 0x54b1, 0x20},
{ 0x54b2, 0x00},
{ 0x54b3, 0x10},
{ 0x54b4, 0x00},
{ 0x54b5, 0xf0},
{ 0x54b6, 0x00},
{ 0x54b7, 0xdf},
//OV5642AWBCfgInfo
{ 0x5402, 0x3f},
{ 0x5403, 0x00},
{ 0x3406, 0x00},

{ 0x5180, 0xff},
{ 0x5181, 0x52},
{ 0x5182, 0x11},
{ 0x5183, 0x14},
{ 0x5184, 0x25},
{ 0x5185, 0x24},
{ 0x5186, 0x0c},//0x0f,
{ 0x5187, 0x0f},
{ 0x5188, 0x18},//0x0f,
{ 0x5189, 0x7c},
{ 0x518a, 0x60},
{ 0x518b, 0xb2},
{ 0x518c, 0xb2},
{ 0x518d, 0x43},//0x44,
{ 0x518e, 0x3d},
{ 0x518f, 0x6f},//0x5b,
{ 0x5190, 0x46},
{ 0x5191, 0xf8},
{ 0x5192, 0x04},
{ 0x5193, 0x70},
{ 0x5194, 0xf0},
{ 0x5195, 0xf0},
{ 0x5196, 0x03},
{ 0x5197, 0x01},
{ 0x5198, 0x06},
{ 0x5199, 0x79},
{ 0x519a, 0x04},
{ 0x519b, 0x00},
{ 0x519c, 0x05},
{ 0x519d, 0x1e},
{ 0x519e, 0x00},
//OV5642LensCfgInfo
{0x5000, 0xdf},
/////////;G
{ 0x5800, 0x36},
{ 0x5801, 0x19},
{ 0x5802, 0x1d},
{ 0x5803, 0x19},
{ 0x5804, 0x19},
{ 0x5805, 0x1c},
{ 0x5806, 0x19},
{ 0x5807, 0x34},
{ 0x5808, 0x12},
{ 0x5809, 0x16},
{ 0x580a, 0xf},
{ 0x580b, 0xd},
{ 0x580c, 0xd},
{ 0x580d, 0xe},
{ 0x580e, 0x14},
{ 0x580f, 0x16},
{ 0x5810, 0x11},
{ 0x5811, 0xb},
{ 0x5812, 0x7},
{ 0x5813, 0x5},
{ 0x5814, 0x4},
{ 0x5815, 0x6},
{ 0x5816, 0xa},
{ 0x5817, 0x11},
{ 0x5818, 0x8},
{ 0x5819, 0x8},
{ 0x581a, 0x2},
{ 0x581b, 0x0},
{ 0x581c, 0x0},
{ 0x581d, 0x2},
{ 0x581e, 0x6},
{ 0x581f, 0xa},
{ 0x5820, 0x8},
{ 0x5821, 0x7},
{ 0x5822, 0x2},
{ 0x5823, 0x0},
{ 0x5824, 0x0},
{ 0x5825, 0x1},
{ 0x5826, 0x5},
{ 0x5827, 0xa},
{ 0x5828, 0xd},
{ 0x5829, 0x9},
{ 0x582a, 0x5},
{ 0x582b, 0x3},
{ 0x582c, 0x3},
{ 0x582d, 0x4},
{ 0x582e, 0x7},
{ 0x582f, 0xe},
{ 0x5830, 0x14},
{ 0x5831, 0x11},
{ 0x5832, 0xb},
{ 0x5833, 0xa},
{ 0x5834, 0x9},
{ 0x5835, 0xb},
{ 0x5836, 0xe},
{ 0x5837, 0x11},
{ 0x5838, 0x19},
{ 0x5839, 0x1e},
{ 0x583a, 0x17},
{ 0x583b, 0x14},
{ 0x583c, 0x15},
{ 0x583d, 0x15},
{ 0x583e, 0x19},
{ 0x583f, 0x1d},

////////////////;B
{ 0x5840, 0xa},
{ 0x5841, 0xf},
{ 0x5842, 0x10},
{ 0x5843, 0xf},
{ 0x5844, 0x10},
{ 0x5845, 0xb},
{ 0x5846, 0xe},
{ 0x5847, 0x11},
{ 0x5848, 0x10},
{ 0x5849, 0x10},
{ 0x584a, 0xf},
{ 0x584b, 0xf},
{ 0x584c, 0xb},
{ 0x584d, 0x10},
{ 0x584e, 0x10},
{ 0x584f, 0x10},
{ 0x5850, 0xe},
{ 0x5851, 0xf},
{ 0x5852, 0x9},
{ 0x5853, 0xf},
{ 0x5854, 0x10},
{ 0x5855, 0x10},
{ 0x5856, 0x10},
{ 0x5857, 0xc},
{ 0x5858, 0xc},
{ 0x5859, 0xe},
{ 0x585a, 0xf},
{ 0x585b, 0x10},
{ 0x585c, 0xe},
{ 0x585d, 0xf},
{ 0x585e, 0x12},
{ 0x585f, 0x11},
{ 0x5860, 0xf},
{ 0x5861, 0xf},
{ 0x5862, 0x15},
{ 0x5863, 0xd},
/////////////////;R
{ 0x5864, 0xa},
{ 0x5865, 0x1d},
{ 0x5866, 0x1f},
{ 0x5867, 0x1f},
{ 0x5868, 0x1e},
{ 0x5869, 0x9},
{ 0x586a, 0x1f},
{ 0x586b, 0x15},
{ 0x586c, 0x14},
{ 0x586d, 0x14},
{ 0x586e, 0x18},
{ 0x586f, 0x1d},
{ 0x5870, 0x1d},
{ 0x5871, 0x12},
{ 0x5872, 0x11},
{ 0x5873, 0x11},
{ 0x5874, 0x14},
{ 0x5875, 0x1a},
{ 0x5876, 0x1f},
{ 0x5877, 0x10},
{ 0x5878, 0x10},
{ 0x5879, 0xf},
{ 0x587a, 0x13},
{ 0x587b, 0x18},
{ 0x587c, 0x19},
{ 0x587d, 0x16},
{ 0x587e, 0x14},
{ 0x587f, 0x14},
{ 0x5880, 0x17},
{ 0x5881, 0x1c},
{ 0x5882, 0x1f},
{ 0x5883, 0x18},
{ 0x5884, 0x16},
{ 0x5885, 0x17},
{ 0x5886, 0x16},
{ 0x5887, 0x1f},
//OV5642ImgCfgInfo
		{ 0x5025, 0x80},
		//AEC
		{ 0x3a0f, 0x38},
		{ 0x3a10, 0x30},
		{ 0x3a1b, 0x3a},
		{ 0x3a1e, 0x2e},
		{ 0x3a11, 0x60},
		{ 0x3a1f, 0x10},
		///////////////////////////
		//scale/average
		{ 0x5688, 0xa6},
		{ 0x5689, 0x6a},
		{ 0x568a, 0xea},
		{ 0x568b, 0xae},
		{ 0x568c, 0xa6},
		{ 0x568d, 0x6a},
		{ 0x568e, 0x62},
		{ 0x568f, 0x26},
		{ 0x5583, 0x40},
		{ 0x5584, 0x40},
		{ 0x5580, 0x02},
	
		///////////////////////////
		{ 0x3710, 0x10},
		{ 0x3632, 0x51},
		{ 0x3702, 0x10},
		{ 0x3703, 0xb2},
		{ 0x3704, 0x18},
		{ 0x370b, 0x40},
		{ 0x370d, 0x03},
		{ 0x3631, 0x01},
		{ 0x3632, 0x52},
		{ 0x3606, 0x24},
		{ 0x3620, 0x96},
		{ 0x5785, 0x07},
		{ 0x3a13, 0x30},
		{ 0x3600, 0x52},
		{ 0x3604, 0x48},
		{ 0x3606, 0x1b},
		{ 0x370d, 0x0b},
		{ 0x370f, 0xc0},
		{ 0x3709, 0x01},
		{ 0x3823, 0x00},
		{ 0x5007, 0x00},
		{ 0x5009, 0x00},
		{ 0x5011, 0x00},
		{ 0x5013, 0x00},
		{ 0x519e, 0x00},
		{ 0x5086, 0x00},
		{ 0x5087, 0x00},
		{ 0x5088, 0x00},
		{ 0x5089, 0x00},
		{0x4740, 0x21}, //set pixel clock, vsync hsync phase
		{ 0x302b, 0x00},
	//	{0x503d,0x80},
		//{0x503e,0x00},
		{0xffff, 0xff}

};

//load OV5642 parameters
void OV5642_init_regs(struct ov5642_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    int i=0;
unsigned char temp;
	temp = i2c_get_byte(client,0x3100);
	printk("heming add temp=%x\n",temp);
    while(1)
    {
        if (OV5642_script[i].val==0xff&&OV5642_script[i].addr==0xffff) 
        {
        	printk("OV5642_write_regs success in initial OV5642.\n");
        	break;
        }
        if((i2c_put_byte(client,OV5642_script[i].addr, OV5642_script[i].val)) < 0)
        {
        	printk("fail in initial OV5642. i=%d\n",i);
		break;
		}
		i++;
    }

    return;
}
/*************************************************************************
* FUNCTION
*    OV5642_set_param_wb
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
void OV5642_set_param_wb(struct ov5642_device *dev,enum  camera_wb_flip_e para)//白平衡
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    switch (para)
	{
		
		case CAM_WB_AUTO://自动
			i2c_put_word(client,0x5180,0xf2ff);
			i2c_put_word(client,0x5182,0x1400);
			i2c_put_word(client,0x5184,0x2425);
			i2c_put_word(client,0x5186,0x0909);
			i2c_put_word(client,0x5188,0x7509);
			i2c_put_word(client,0x518a,0xe054);
			i2c_put_word(client,0x518c,0x42b2);
			i2c_put_word(client,0x518e,0x563d);
			i2c_put_word(client,0x5190,0xf846);
			i2c_put_word(client,0x5192,0x7004);
			i2c_put_word(client,0x5194,0xf0f0);
			i2c_put_word(client,0x5196,0x0103);
			i2c_put_word(client,0x5198,0x1204);
			i2c_put_word(client,0x519a,0x0004);
			i2c_put_word(client,0x519c,0x8206);
			i2c_put_word(client,0x519e,0x563d);
			break;

		case CAM_WB_CLOUD: //阴天
			i2c_put_byte(client,0x3406 , 0x01);
			i2c_put_byte(client,0x3400 , 0x06);
			i2c_put_byte(client,0x3401 , 0x48);
			i2c_put_byte(client,0x3402 , 0x04);
			i2c_put_byte(client,0x3403 , 0x00);
			i2c_put_byte(client,0x3404 , 0x04);
			i2c_put_byte(client,0x3405 , 0xd3);
			break;

		case CAM_WB_DAYLIGHT: //
			i2c_put_byte(client,0x3406 , 0x01);
			i2c_put_byte(client,0x3400 , 0x06);
			i2c_put_byte(client,0x3401 , 0x1c);
			i2c_put_byte(client,0x3402 , 0x04);
			i2c_put_byte(client,0x3403 , 0x00);
			i2c_put_byte(client,0x3404 , 0x04);
			i2c_put_byte(client,0x3405 , 0xf3);
			break;

		case CAM_WB_INCANDESCENCE: 
			/*i2c_put_byte(client,0x0320 , 0x02);
			i2c_put_byte(client,0x0321 , 0x02);
			i2c_put_byte(client,0x0322 , 0x02);
			i2c_put_byte(client,0x0323 , 0x02);
			i2c_put_byte(client,0x0441 , 0x50);
			i2c_put_byte(client,0x0442 , 0x00);
			i2c_put_byte(client,0x0443 , 0x00);
			i2c_put_byte(client,0x0444 , 0x30);*/
			break;
			
		case CAM_WB_TUNGSTEN: 
			/*i2c_put_byte(client,0x0320 , 0x02);
			i2c_put_byte(client,0x0321 , 0x02);
			i2c_put_byte(client,0x0322 , 0x02);
			i2c_put_byte(client,0x0323 , 0x02);
			i2c_put_byte(client,0x0441 , 0x0B);
			i2c_put_byte(client,0x0442 , 0x00);
			i2c_put_byte(client,0x0443 , 0x00);
			i2c_put_byte(client,0x0444 , 0x5E);*/
			break;

      	case CAM_WB_FLUORESCENT:
      			i2c_put_byte(client,0x3406 , 0x01);
			i2c_put_byte(client,0x3400 , 0x05);
			i2c_put_byte(client,0x3401 , 0x48);
			i2c_put_byte(client,0x3402 , 0x04);
			i2c_put_byte(client,0x3403 , 0x00);
			i2c_put_byte(client,0x3404 , 0x07);
			i2c_put_byte(client,0x3405 , 0xcf);
			break;

		case CAM_WB_MANUAL:
		    	// TODO
			break;
	}
	

} /* OV5642_set_param_wb */
/*************************************************************************
* FUNCTION
*    OV5642_set_param_exposure
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
void OV5642_set_param_exposure(struct ov5642_device *dev,enum camera_exposure_e para)//曝光调节
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);


    switch (para)
	{
		case EXPOSURE_N4_STEP:  //负4档  
            		i2c_put_byte(client,0x3a0f , 0x10);
			i2c_put_byte(client,0x3a10 , 0x08);//40
			i2c_put_byte(client,0x3a1b , 0x10);
			i2c_put_byte(client,0x3a1e , 0x08);
			i2c_put_byte(client,0x3a11 , 0x20);
			i2c_put_byte(client,0x3a1f , 0x10);
			break;
			
		case EXPOSURE_N3_STEP:
            		i2c_put_byte(client,0x3a0f , 0x18);
			i2c_put_byte(client,0x3a10 , 0x00);//50
			i2c_put_byte(client,0x3a1b , 0x18);
			i2c_put_byte(client,0x3a1e , 0x10);
			i2c_put_byte(client,0x3a11 , 0x30);
			i2c_put_byte(client,0x3a1f , 0x10);
			break;
			
		case EXPOSURE_N2_STEP:
            		i2c_put_byte(client,0x3a0f , 0x20);
			i2c_put_byte(client,0x3a10 , 0x18);//b0
			i2c_put_byte(client,0x3a1b , 0x41);
			i2c_put_byte(client,0x3a1e , 0x20);
			i2c_put_byte(client,0x3a11 , 0x18);
			i2c_put_byte(client,0x3a1f , 0x10);
			break;
			
		case EXPOSURE_N1_STEP:
            		i2c_put_byte(client,0x3a0f , 0x28);
			i2c_put_byte(client,0x3a10 , 0x20);//d0
			i2c_put_byte(client,0x3a1b , 0x51);
			i2c_put_byte(client,0x3a1e , 0x28);
			i2c_put_byte(client,0x3a11 , 0x20);
			i2c_put_byte(client,0x3a1f , 0x10);
			break;
			
		case EXPOSURE_0_STEP://默认零档
            		i2c_put_byte(client,0x3a0f , 0x30);
			i2c_put_byte(client,0x3a10 , 0x28);//0c
			i2c_put_byte(client,0x3a1b , 0x30);
			i2c_put_byte(client,0x3a1e , 0x26);
			i2c_put_byte(client,0x3a11 , 0x60);
			i2c_put_byte(client,0x3a1f , 0x14);
			break;
			
		case EXPOSURE_P1_STEP://正一档
            		i2c_put_byte(client,0x3a0f , 0x48);
			i2c_put_byte(client,0x3a10 , 0x40);//30
			i2c_put_byte(client,0x3a1b , 0x80);
			i2c_put_byte(client,0x3a1e , 0x48);
			i2c_put_byte(client,0x3a11 , 0x40);
			i2c_put_byte(client,0x3a1f , 0x20);
			break;
			
		case EXPOSURE_P2_STEP:
            		i2c_put_byte(client,0x3a0f , 0x50);
			i2c_put_byte(client,0x3a10 , 0x48);
			i2c_put_byte(client,0x3a1b , 0x90);
			i2c_put_byte(client,0x3a1e , 0x50);
			i2c_put_byte(client,0x3a11 , 0x48);
			i2c_put_byte(client,0x3a1f , 0x20);
			break;
			
		case EXPOSURE_P3_STEP:
            		i2c_put_byte(client,0x3a0f , 0x58);
			i2c_put_byte(client,0x3a10 , 0x50);
			i2c_put_byte(client,0x3a1b , 0x91);
			i2c_put_byte(client,0x3a1e , 0x58);
			i2c_put_byte(client,0x3a11 , 0x50);
			i2c_put_byte(client,0x3a1f , 0x10);
			break;
			
		case EXPOSURE_P4_STEP:	
            		i2c_put_byte(client,0x3a0f , 0x60);
			i2c_put_byte(client,0x3a10 , 0x58);
			i2c_put_byte(client,0x3a1b , 0xa0);
			i2c_put_byte(client,0x3a1e , 0x60);
			i2c_put_byte(client,0x3a11 , 0x58);
			i2c_put_byte(client,0x3a1f , 0x20);
			break;
			
		default:
            		i2c_put_byte(client,0x3a0f , 0x30);
			i2c_put_byte(client,0x3a10 , 0x28);//0c
			i2c_put_byte(client,0x3a1b , 0x30);
			i2c_put_byte(client,0x3a1e , 0x26);
			i2c_put_byte(client,0x3a11 , 0x60);
			i2c_put_byte(client,0x3a1f , 0x14);
			break;
	}


} /* OV5642_set_param_exposure */
/*************************************************************************
* FUNCTION
*    OV5642_set_param_effect
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
void OV5642_set_param_effect(struct ov5642_device *dev,enum camera_effect_flip_e para)//特效设置
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
  
    switch (para)
	{
		case CAM_EFFECT_ENC_NORMAL://正常
			i2c_put_byte(client,0x5001,0x03);//disable effect
			break;		

		case CAM_EFFECT_ENC_GRAYSCALE://灰阶
			i2c_put_byte(client,0x5001,0x83);
			i2c_put_byte(client,0x5580,0x20);
			break;

		case CAM_EFFECT_ENC_SEPIA://复古
		     	/*i2c_put_byte(client,0x0115,0x0a);
			i2c_put_byte(client,0x026e,0x60);
			i2c_put_byte(client,0x026f,0xa0);*/
			break;		
				
		case CAM_EFFECT_ENC_SEPIAGREEN://复古绿
			/*i2c_put_byte(client,0x0115,0x0a);
			i2c_put_byte(client,0x026e,0x20);
			i2c_put_byte(client,0x026f,0x00);*/
			break;					

		case CAM_EFFECT_ENC_SEPIABLUE://复古蓝
			/*i2c_put_byte(client,0x0115,0x0a);
			i2c_put_byte(client,0x026e,0xfb);
			i2c_put_byte(client,0x026f,0x00);*/
			break;								

		case CAM_EFFECT_ENC_COLORINV://底片
			i2c_put_byte(client,0x5001,0x83);
			i2c_put_byte(client,0x5580,0x40);
			break;		

		default:
			break;
	}


} /* OV5642_set_param_effect */

/*************************************************************************
* FUNCTION
*    OV5642_NightMode
*
* DESCRIPTION
*    This function night mode of OV5642.
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
void OV5642_NightMode(struct ov5642_device *dev,enum  camera_night_mode_flip_e enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	if (enable)
	{
		i2c_put_byte(client,0x3a00 , 0x7c);
		i2c_put_byte(client,0x3a05 , 0x50);
	}
	else
	{
		i2c_put_byte(client,0x3a00 , 0x78);
		i2c_put_byte(client,0x3a05 , 0x30);
	}

}    /* OV5642_NightMode */


unsigned char v4l_2_ov5642(int val)
{
	int ret=val/0x20;
	if(ret<4) return ret*0x20+0x80;
	else if(ret<8) return ret*0x20+0x20;
	else return 0;
}

static int ov5642_setting(struct ov5642_device *dev,int PROP_ID,int value ) 
{
	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
	case V4L2_CID_BRIGHTNESS:
		dprintk(dev, 1, "setting brightned:%d\n",v4l_2_ov5642(value));
		ret=i2c_put_byte(client,0x0201,v4l_2_ov5642(value));
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
        if(ov5642_qctrl[4].default_value!=value){
			ov5642_qctrl[4].default_value=value;
			OV5642_set_param_wb(dev,value);
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
        	}
		break;
	case V4L2_CID_EXPOSURE:
        if(ov5642_qctrl[5].default_value!=value){
			ov5642_qctrl[5].default_value=value;
			OV5642_set_param_exposure(dev,value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
        	}
		break;
	case V4L2_CID_COLORFX:
        if(ov5642_qctrl[6].default_value!=value){
			ov5642_qctrl[6].default_value=value;
			OV5642_set_param_effect(dev,value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
        	}
		break;
	default:
		ret=-1;
		break;
	}
	return ret;
	
}

static void power_down_ov5642(struct ov5642_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	/*i2c_put_byte(client,0x0104, 0x00);
	i2c_put_byte(client,0x0100, 0x00);*/
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

extern   int vm_fill_buffer(struct videobuf_buffer* vb , int v4l2_format , int magic,void* vaddr);
#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void ov5642_fillbuff(struct ov5642_fh *fh, struct ov5642_buffer *buf)
{
	struct ov5642_device *dev = fh->dev;
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

static void ov5642_thread_tick(struct ov5642_fh *fh)
{
	struct ov5642_buffer *buf;
	struct ov5642_device *dev = fh->dev;
	struct ov5642_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 1, "No active queue to serve\n");
		goto unlock;
	}

	buf = list_entry(dma_q->active.next,
			 struct ov5642_buffer, vb.queue);
    dprintk(dev, 1, "%s\n", __func__);
    dprintk(dev, 1, "list entry get buf is %x\n",buf);

	/* Nobody is waiting on this buffer, return */
	if (!waitqueue_active(&buf->vb.done))
		goto unlock;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	ov5642_fillbuff(fh, buf);
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

static void ov5642_sleep(struct ov5642_fh *fh)
{
	struct ov5642_device *dev = fh->dev;
	struct ov5642_dmaqueue *dma_q = &dev->vidq;

	int timeout;
	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	timeout = msecs_to_jiffies(frames_to_ms(1));

	ov5642_thread_tick(fh);

	schedule_timeout_interruptible(timeout);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int ov5642_thread(void *data)
{
	struct ov5642_fh  *fh = data;
	struct ov5642_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		ov5642_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int ov5642_start_thread(struct ov5642_fh *fh)
{
	struct ov5642_device *dev = fh->dev;
	struct ov5642_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(ov5642_thread, fh, "ov5642");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void ov5642_stop_thread(struct ov5642_dmaqueue  *dma_q)
{
	struct ov5642_device *dev = container_of(dma_q, struct ov5642_device, vidq);

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
	struct ov5642_fh  *fh = vq->priv_data;
	struct ov5642_device *dev  = fh->dev;
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

static void free_buffer(struct videobuf_queue *vq, struct ov5642_buffer *buf)
{
	struct ov5642_fh  *fh = vq->priv_data;
	struct ov5642_device *dev  = fh->dev;

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
	struct ov5642_fh     *fh  = vq->priv_data;
	struct ov5642_device    *dev = fh->dev;
	struct ov5642_buffer *buf = container_of(vb, struct ov5642_buffer, vb);
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
	struct ov5642_buffer    *buf  = container_of(vb, struct ov5642_buffer, vb);
	struct ov5642_fh        *fh   = vq->priv_data;
	struct ov5642_device       *dev  = fh->dev;
	struct ov5642_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct ov5642_buffer   *buf  = container_of(vb, struct ov5642_buffer, vb);
	struct ov5642_fh       *fh   = vq->priv_data;
	struct ov5642_device      *dev  = (struct ov5642_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops ov5642_video_qops = {
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
	struct ov5642_fh  *fh  = priv;
	struct ov5642_device *dev = fh->dev;

	strcpy(cap->driver, "ov5642");
	strcpy(cap->card, "ov5642");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = OV5642_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct ov5642_fmt *fmt;

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
	struct ov5642_fh *fh = priv;

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
	struct ov5642_fh  *fh  = priv;
	struct ov5642_device *dev = fh->dev;
	struct ov5642_fmt *fmt;
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
	struct ov5642_fh *fh = priv;
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
	struct ov5642_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov5642_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov5642_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov5642_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct ov5642_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ov5642_fh  *fh = priv;
    tvin_parm_t para;
    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;

    para.port  = TVIN_PORT_CAMERA;
    para.fmt = TVIN_SIG_FMT_CAMERA_1280X720P_30Hz;
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
    start_tvin_service(0,&para);
	    fh->stream_on        = 1;
	}
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ov5642_fh  *fh = priv;

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
	struct ov5642_fh *fh = priv;
	struct ov5642_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct ov5642_fh *fh = priv;
	struct ov5642_device *dev = fh->dev;

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

	for (i = 0; i < ARRAY_SIZE(ov5642_qctrl); i++)
		if (qc->id && qc->id == ov5642_qctrl[i].id) {
			memcpy(qc, &(ov5642_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct ov5642_fh *fh = priv;
	struct ov5642_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5642_qctrl); i++)
		if (ctrl->id == ov5642_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct ov5642_fh *fh = priv;
	struct ov5642_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5642_qctrl); i++)
		if (ctrl->id == ov5642_qctrl[i].id) {
			if (ctrl->value < ov5642_qctrl[i].minimum ||
			    ctrl->value > ov5642_qctrl[i].maximum ||
			    ov5642_setting(dev,ctrl->id,ctrl->value)<0) {
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

static int ov5642_open(struct file *file)
{
	struct ov5642_device *dev = video_drvdata(file);
	struct ov5642_fh *fh = NULL;
	int retval = 0;
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	OV5642_init_regs(dev);
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

	videobuf_queue_vmalloc_init(&fh->vb_vidq, &ov5642_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct ov5642_buffer), fh);

	ov5642_start_thread(fh);

	return 0;
}

static ssize_t
ov5642_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct ov5642_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
ov5642_poll(struct file *file, struct poll_table_struct *wait)
{
	struct ov5642_fh        *fh = file->private_data;
	struct ov5642_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int ov5642_close(struct file *file)
{
	struct ov5642_fh         *fh = file->private_data;
	struct ov5642_device *dev       = fh->dev;
	struct ov5642_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);

	ov5642_stop_thread(vidq);
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
	power_down_ov5642(dev);
#endif
	msleep(2);

	if(dev->platform_dev_data.device_uninit) {
		dev->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
	}
	msleep(2); 
	return 0;
}

static int ov5642_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ov5642_fh  *fh = file->private_data;
	struct ov5642_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations ov5642_fops = {
	.owner		= THIS_MODULE,
	.open           = ov5642_open,
	.release        = ov5642_close,
	.read           = ov5642_read,
	.poll		= ov5642_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = ov5642_mmap,
};

static const struct v4l2_ioctl_ops ov5642_ioctl_ops = {
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

static struct video_device ov5642_template = {
	.name		= "ov5642_v4l",
	.fops           = &ov5642_fops,
	.ioctl_ops 	= &ov5642_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int ov5642_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV5642, 0);
}

static const struct v4l2_subdev_core_ops ov5642_core_ops = {
	.g_chip_ident = ov5642_g_chip_ident,
};

static const struct v4l2_subdev_ops ov5642_ops = {
	.core = &ov5642_core_ops,
};

static int ov5642_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int pgbuf;
	int err;
	struct ov5642_device *t;
	struct v4l2_subdev *sd;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &ov5642_ops);
	mutex_init(&t->mutex);

	/* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		kfree(client);
		return -ENOMEM;
	}
	memcpy(t->vdev, &ov5642_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);

	/* Register it */
	aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	if (plat_dat) {
		t->platform_dev_data.device_init=plat_dat->device_init;
		t->platform_dev_data.device_uninit=plat_dat->device_uninit;
		if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
	}
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}
	return 0;
}

static int ov5642_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5642_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	kfree(t);
	return 0;
}

static const struct i2c_device_id ov5642_id[] = {
	{ "ov5642_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5642_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "ov5642",
	.probe = ov5642_probe,
	.remove = ov5642_remove,
	.id_table = ov5642_id,
};
