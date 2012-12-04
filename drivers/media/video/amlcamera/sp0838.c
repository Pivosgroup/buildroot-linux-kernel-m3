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
#include <mach/pinmux.h>
#include <linux/tvin/tvin.h>
#include "common/plat_ctrl.h"
#include "common/vmapi.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend sp0838_early_suspend;
#endif

#define SP0838_CAMERA_MODULE_NAME "sp0838"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */



#define  Pre_Value_P0_0x30  0x00
//Filter en&dis
#define  Pre_Value_P0_0x56  0x70
#define  Pre_Value_P0_0x57  0x10  //filter outdoor
#define  Pre_Value_P0_0x58  0x10  //filter indoor
#define  Pre_Value_P0_0x59  0x10  //filter night
#define  Pre_Value_P0_0x5a  0x02  //smooth outdoor
#define  Pre_Value_P0_0x5b  0x02  //smooth indoor
#define  Pre_Value_P0_0x5c  0x20  //smooht night
//outdoor sharpness
#define  Pre_Value_P0_0x65  0x03
#define  Pre_Value_P0_0x66  0x01
#define  Pre_Value_P0_0x67  0x03
#define  Pre_Value_P0_0x68  0x46
//indoor sharpness
#define  Pre_Value_P0_0x6b  0x04
#define  Pre_Value_P0_0x6c  0x01
#define  Pre_Value_P0_0x6d  0x03
#define  Pre_Value_P0_0x6e  0x46
//night sharpness
#define  Pre_Value_P0_0x71  0x05
#define  Pre_Value_P0_0x72  0x01
#define  Pre_Value_P0_0x73  0x03
#define  Pre_Value_P0_0x74  0x46
//color
#define  Pre_Value_P0_0x7f  0xd7  //R 
#define  Pre_Value_P0_0x87  0xf8  //B
//satutation
#define  Pre_Value_P0_0xd8  0x48
#define  Pre_Value_P0_0xd9  0x48
#define  Pre_Value_P0_0xda  0x48
#define  Pre_Value_P0_0xdb  0x48
//AE target
#define  Pre_Value_P0_0xf7  0x78
#define  Pre_Value_P0_0xf8  0x63
#define  Pre_Value_P0_0xf9  0x68
#define  Pre_Value_P0_0xfa  0x53
//HEQ
#define  Pre_Value_P0_0xdd  0x70
#define  Pre_Value_P0_0xde  0x90
//AWB pre gain
#define  Pre_Value_P1_0x28  0x75
#define  Pre_Value_P1_0x29  0x4e

//VBLANK
#define  Pre_Value_P0_0x05  0x00
#define  Pre_Value_P0_0x06  0x00
//HBLANK
#define  Pre_Value_P0_0x09  0x01
#define  Pre_Value_P0_0x0a  0x76





#define SP0838_CAMERA_MAJOR_VERSION 0
#define SP0838_CAMERA_MINOR_VERSION 7
#define SP0838_CAMERA_RELEASE 0
#define SP0838_CAMERA_VERSION \
	KERNEL_VERSION(SP0838_CAMERA_MAJOR_VERSION, SP0838_CAMERA_MINOR_VERSION, SP0838_CAMERA_RELEASE)

MODULE_DESCRIPTION("sp0838 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static int sp0838_have_open=0;

static int sp0838_h_active=320;
static int sp0838_v_active=240;
static int sp0838_frame_rate = 150;

static int	sp0838_banding_60HZ = 0;


typedef enum
{
	SP0838_RGB_Gamma_m1 = 1,
	SP0838_RGB_Gamma_m2,
	SP0838_RGB_Gamma_m3,
	SP0838_RGB_Gamma_m4,
	SP0838_RGB_Gamma_m5,
	SP0838_RGB_Gamma_m6,
	SP0838_RGB_Gamma_night
}SP0838_GAMMA_TAG;

//static void SP0838AwbEnable(struct sp0838_device *dev, int Enable);
//void SP0838GammaSelect(struct sp0838_device *dev, SP0838_GAMMA_TAG GammaLvl);
	// void SP0838write_more_registers(struct sp0838_device *dev);
static struct
{
    bool BypassAe;
    bool BypassAwb;
    bool CapState; /* TRUE: in capture state, else in preview state */
    bool PvMode; /* TRUE: preview size output, else full size output */
	bool VideoMode; /* TRUE: video mode, else preview mode */
	bool NightMode;/*TRUE:work in night mode, else normal mode*/
    unsigned char BandingFreq; /* SP0838_50HZ or SP0838_60HZ for 50Hz/60Hz */
    unsigned InternalClock; /* internal clock which using process pixel(for exposure) */
    unsigned Pclk; /* output clock which output to baseband */
    unsigned Gain; /* base on 0x40 */
    unsigned Shutter; /* unit is (linelength / internal clock) s */
	unsigned FrameLength; /* total line numbers in one frame(include dummy line) */
    unsigned LineLength; /* total pixel numbers in one line(include dummy pixel) */
    //IMAGE_SENSOR_INDEX_ENUM SensorIdx;
    //sensor_data_struct *NvramData;
} SP0838Sensor;



/* supported controls */
static struct v4l2_queryctrl sp0838_qctrl[] = {
	{
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
	int 	qctl_regs[ARRAY_SIZE(sp0838_qctrl)];
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

static struct v4l2_frmsize_discrete sp0838_prev_resolution[3]= //should include 320x240 and 640x480, those two size are used for recording
{
	{320, 240},
	{352, 288},
	{640, 480},
};

static struct v4l2_frmsize_discrete sp0838_pic_resolution[1]=
{
	{640, 480},
};

/* ------------------------------------------------------------------
	reg spec of SP0838
   ------------------------------------------------------------------*/
struct aml_camera_i2c_fig1_s SP0838_script[] = {     
	//SP0838 ini
	{0xfd,0x00},//P0
	{0x1B,0x02},
	{0x27,0xe8},
	{0x28,0x0B},
	{0x32,0x00},
	{0x22,0xc0},
	{0x26,0x10}, 
	{0x5f,0x11},//Bayer order
	{0xfd,0x01},//P1
	{0x25,0x1a},//Awb start
	{0x26,0xfb},
	{0x28,Pre_Value_P1_0x28},
	{0x29,Pre_Value_P1_0x29},
	{0xfd,0x00},
	{0xe7,0x03},
	{0xe7,0x00},
	{0xfd,0x01},
	{0x31,0x60},//64
	{0x32,0x18},
	{0x4d,0xdc},
	{0x4e,0x53},
	{0x41,0x8c},
	{0x42,0x57},
	{0x55,0xff},
	{0x56,0x00},
	{0x59,0x82},
	{0x5a,0x00},
	{0x5d,0xff},
	{0x5e,0x6f},
	{0x57,0xff},
	{0x58,0x00},
	{0x5b,0xff},
	{0x5c,0xa8},
	{0x5f,0x75},
	{0x60,0x00},
	{0x2d,0x00},
	{0x2e,0x00},
	{0x2f,0x00},
	{0x30,0x00},
	{0x33,0x00},
	{0x34,0x00},
	{0x37,0x00},
	{0x38,0x00},//awb end
	{0xfd,0x00},//P0
	{0x33,0x6f},//LSC BPC EN
	{0x51,0x3f},//BPC debug start
	{0x52,0x09},
	{0x53,0x00},
	{0x54,0x00},
	{0x55,0x10},//BPC debug end
	{0x4f,0x08},//blueedge
	{0x50,0x08},
	{0x57,Pre_Value_P0_0x57},//Raw filter debut start
	{0x58,Pre_Value_P0_0x58},
	{0x59,Pre_Value_P0_0x59},
	{0x56,Pre_Value_P0_0x56},
	{0x5a,Pre_Value_P0_0x5a},
	{0x5b,Pre_Value_P0_0x5b},
	{0x5c,Pre_Value_P0_0x5c},//Raw filter debut end 
	{0x65,Pre_Value_P0_0x65},//Sharpness debug start
	{0x66,Pre_Value_P0_0x66},
	{0x67,Pre_Value_P0_0x67},
	{0x68,Pre_Value_P0_0x68},
	{0x69,0x7f},
	{0x6a,0x01},
	{0x6b,Pre_Value_P0_0x6b},
	{0x6c,Pre_Value_P0_0x6c},
	{0x6d,Pre_Value_P0_0x6d},//Edge gain normal
	{0x6e,Pre_Value_P0_0x6e},//Edge gain normal
	{0x6f,0x7f},
	{0x70,0x01},
	{0x71,Pre_Value_P0_0x71}, //锐化阈值          
	{0x72,Pre_Value_P0_0x72}, //弱轮廓阈值        
	{0x73,Pre_Value_P0_0x73}, //边缘正向增益值    
	{0x74,Pre_Value_P0_0x74}, //边缘反向增益值    
	{0x75,0x7f},              //使能位            
	{0x76,0x01},//Sharpness debug end
	{0xcb,0x07},//HEQ&Saturation debug start 
	{0xcc,0x04},
	{0xce,0xff},
	{0xcf,0x10},
	{0xd0,0x20},
	{0xd1,0x00},
	{0xd2,0x1c},
	{0xd3,0x16},
	{0xd4,0x00},
	{0xd6,0x1c},
	{0xd7,0x16},
	{0xdd,Pre_Value_P0_0xdd},//Contrast
	{0xde,Pre_Value_P0_0xde},//HEQ&Saturation debug end
	{0x7f,Pre_Value_P0_0x7f},//Color Correction start
	{0x80,0xbc},                        
	{0x81,0xed},                        
	{0x82,0xd7},                        
	{0x83,0xd4},                        
	{0x84,0xd6},                        
	{0x85,0xff},                        
	{0x86,0x89},                        
	{0x87,Pre_Value_P0_0x87},                        
	{0x88,0x3c},                        
	{0x89,0x33},                        
	{0x8a,0x0f},//Color Correction end  
	{0x8b,0x0 },//gamma start
	{0x8c,0x1a},             
	{0x8d,0x29},             
	{0x8e,0x41},             
	{0x8f,0x62},             
	{0x90,0x7c},             
	{0x91,0x90},             
	{0x92,0xa2},             
	{0x93,0xaf},             
	{0x94,0xbc},             
	{0x95,0xc5},             
	{0x96,0xcd},             
	{0x97,0xd5},             
	{0x98,0xdd},             
	{0x99,0xe5},             
	{0x9a,0xed},             
	{0x9b,0xf5},             
	{0xfd,0x01},//P1         
	{0x8d,0xfd},             
	{0x8e,0xff},//gamma end  
	{0xfd,0x00},//P0
	{0xca,0xcf},
	{0xd8,Pre_Value_P0_0xd8},//UV outdoor
	{0xd9,Pre_Value_P0_0xd9},//UV indoor 
	{0xda,Pre_Value_P0_0xda},//UV dummy
	{0xdb,Pre_Value_P0_0xdb},//UV lowlight
	{0xb9,0x00},//Ygamma start
	{0xba,0x04},
	{0xbb,0x08},
	{0xbc,0x10},
	{0xbd,0x20},
	{0xbe,0x30},
	{0xbf,0x40},
	{0xc0,0x50},
	{0xc1,0x60},
	{0xc2,0x70},
	{0xc3,0x80},
	{0xc4,0x90},
	{0xc5,0xA0},
	{0xc6,0xB0},
	{0xc7,0xC0},
	{0xc8,0xD0},
	{0xc9,0xE0},
	{0xfd,0x01},//P1
	{0x89,0xf0},
	{0x8a,0xff},//Ygamma end
	{0xfd,0x00},//P0
	{0xe8,0x30},//AEdebug start
	{0xe9,0x30},
	{0xea,0x40},//Alc Window sel
	{0xf4,0x1b},//outdoor mode sel
	{0xf5,0x80},
	{0xf7,Pre_Value_P0_0xf7},//AE target
	{0xf8,Pre_Value_P0_0xf8},
	{0xf9,Pre_Value_P0_0xf9},//AE target 
	{0xfa,Pre_Value_P0_0xfa},
	{0xfd,0x01},//P1
	{0x09,0x31},//AE Step 3.0
	{0x0a,0x85},
	{0x0b,0x0b},//AE Step 3.0
	{0x14,0x20},
	{0x15,0x0f},
	{0xfd,0x00},//P0
	{0x05,0x00},  
	{0x06,0x00},
	{0x09,0x01},
	{0x0a,0x76},
	{0xf0,0x62},
	{0xf1,0x00},
	{0xf2,0x5f},
	{0xf5,0x78},
	{0xfd,0x01},//P1
	{0x00,0xba},
	{0x0f,0x60},
	{0x16,0x60},
	{0x17,0xa2},
	{0x18,0xaa},
	{0x1b,0x60},
	{0x1c,0xaa},
	{0xb4,0x20},
	{0xb5,0x3a},
	{0xb6,0x5e},
	{0xb9,0x40},
	{0xba,0x4f},
	{0xbb,0x47},
	{0xbc,0x45},
	{0xbd,0x43},
	{0xbe,0x42},
	{0xbf,0x42},
	{0xc0,0x42},
	{0xc1,0x41},
	{0xc2,0x41},
	{0xc3,0x41},
	{0xc4,0x41},
	{0xc5,0x78},
	{0xc6,0x41},
	{0xca,0x78},
	{0xcb,0x0c},//AEdebug end
	#if 1//caprure preview daylight 24M 50hz 20-8FPS maxgain:0x70	
	{0xfd,0x00},
	{0x05,Pre_Value_P0_0x05 },
	{0x06,Pre_Value_P0_0x06 },
	{0x09,Pre_Value_P0_0x09 },
	{0x0a,Pre_Value_P0_0x0a },
	{0xf0,0x62},
	{0xf1,0x0 },
	{0xf2,0x5f},
	{0xf5,0x78},
	{0xfd,0x01},
	{0x00,0xb2},
	{0x0f,0x60},
	{0x16,0x60},
	{0x17,0xa2},
	{0x18,0xaa},
	{0x1b,0x60},
	{0x1c,0xaa},
	{0xb4,0x20},
	{0xb5,0x3a},
	{0xb6,0x5e},
	{0xb9,0x40},
	{0xba,0x4f},
	{0xbb,0x47},
	{0xbc,0x45},
	{0xbd,0x43},
	{0xbe,0x42},
	{0xbf,0x42},
	{0xc0,0x42},
	{0xc1,0x41},
	{0xc2,0x41},
	{0xc3,0x41},
	{0xc4,0x41},
	{0xc5,0x70},
	{0xc6,0x41},
	{0xca,0x70},
	{0xcb,0xc },
	{0xfd,0x00},
	#endif	
	{0xfd,0x00},  //P0
	{0x31,0x40},
	{0x32,0x15},  //Auto_mode set
	{0x34,0x66},  //Isp_mode set
	//{0x35,0xc0},  //out format
	{0x35,0x41},
	{0x36,0x80},
	//{0x0d,0x1c},   // seltest
	//{0x30,0x02},
	{0xFF,0xFF},
};


void SP0838_init_regs(struct sp0838_device *dev)
{
    int i=0;//,j;
    unsigned char buf[2];
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    while(1) {
        buf[0] = SP0838_script[i].addr;
        buf[1] = SP0838_script[i].val;
        if(SP0838_script[i].val==0xff&&SP0838_script[i].addr==0xff){
            printk("SP0838_write_regs success in initial SP0838.\n");
            break;
        }
        if((i2c_put_byte_add8(client,buf, 2)) < 0){
            printk("fail in initial SP0838. \n");
            return;
        }   
        i++;
    }
    aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;

    if (plat_dat&&plat_dat->custom_init_script) {
        i=0;
        aml_camera_i2c_fig1_t*  custom_script = (aml_camera_i2c_fig1_t*)plat_dat->custom_init_script;
        while(1) {
            buf[0] = custom_script[i].addr;
            buf[1] = custom_script[i].val;
            if (custom_script[i].val==0xff&&custom_script[i].addr==0xff){
                printk("SP0838_write_custom_regs success in initial SP0838.\n");
                break;
            }
            if((i2c_put_byte_add8(client,buf, 2)) < 0){
                printk("fail in initial SP0838 custom_regs. \n");
                return;
            }
            i++;
        }
    }
    return;

}

static struct aml_camera_i2c_fig1_s resolution_320x240_script[] = {
	{0xfd, 0x00},
	{0x47, 0x00},
	{0x48, 0x78},
	{0x49, 0x00},
	{0x4a, 0xf0},
	{0x4b, 0x00},
	{0x4c, 0xa0},
	{0x4d, 0x01},
	{0x4e, 0x40},
                
	{0xff, 0xff}

};

static struct aml_camera_i2c_fig1_s resolution_640x480_script[] = {
	{0xfd, 0x00},
	{0x47, 0x00},
	{0x48, 0x00},
	{0x49, 0x01},
	{0x4a, 0xe0},
	{0x4b, 0x00},
	{0x4c, 0x00},
	{0x4d, 0x02},
	{0x4e, 0x80},

                
	{0xff, 0xff}

};

static void sp0838_set_resolution(struct sp0838_device *dev,int height,int width)
{
	int i=0;
    unsigned char buf[2];
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	struct aml_camera_i2c_fig1_s* resolution_script;
	if (height >= 480) {
		printk("set resolution 640X480\n");
		resolution_script = resolution_640x480_script;
		sp0838_h_active = 640;
		sp0838_v_active = 478;
		//SP0838_init_regs(dev);
		//return;
	} else {
		printk("set resolution 320X240\n");
		sp0838_h_active = 320;
		sp0838_v_active = 238;
		resolution_script = resolution_320x240_script;
	}
	
	while(1) {
        buf[0] = resolution_script[i].addr;
        buf[1] = resolution_script[i].val;
        if(resolution_script[i].val==0xff&&resolution_script[i].addr==0xff) {
            break;
        }
        if((i2c_put_byte_add8(client,buf, 2)) < 0) {
            printk("fail in setting resolution \n");
            return;
        }
        i++;
    }
	
}



/*************************************************************************
* FUNCTION
*   SP0838AwbEnable
*
* DESCRIPTION
*   disable/enable awb
*
* PARAMETERS
*   Enable
*
* RETURNS
*   None
*
* LOCAL AFFECTED
*
*************************************************************************/

static void SP0838AwbEnable(struct sp0838_device *dev, int Enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    if (SP0838Sensor.BypassAwb)
    {
        Enable = 0;
    }
    // TODO: enable or disable AWB here
    {
		unsigned char temp = i2c_get_byte_add8(client,0x32);
		if (Enable)
		{
			i2c_put_byte_add8_new(client,0x32, (temp | 0x10));
		}
		else
		{
			i2c_put_byte_add8_new(client,0x32, (temp & 0xef));
		}
    }
}




/*************************************************************************
* FUNCTION
*	SP0838GammaSelect
*
* DESCRIPTION
*	This function is served for FAE to select the appropriate GAMMA curve.
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void SP0838GammaSelect(struct sp0838_device *dev, SP0838_GAMMA_TAG GammaLvl)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

return; // superpix fcs test
	switch(GammaLvl)
	{
		case SP0838_RGB_Gamma_m1:						//smallest gamma curve
			i2c_put_byte_add8_new(client, (unsigned char)0xfe, (unsigned char)0x00);
			i2c_put_byte_add8_new(client, (unsigned char)0xbf, (unsigned char)0x06);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x12);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x22);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x35);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x4b);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x5f);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x72);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0x8d);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0xa4);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xb8);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xc8);
			i2c_put_byte_add8_new(client, (unsigned char)0xca, (unsigned char)0xd4);
			i2c_put_byte_add8_new(client, (unsigned char)0xcb, (unsigned char)0xde);
			i2c_put_byte_add8_new(client, (unsigned char)0xcc, (unsigned char)0xe6);
			i2c_put_byte_add8_new(client, (unsigned char)0xcd, (unsigned char)0xf1);
			i2c_put_byte_add8_new(client, (unsigned char)0xce, (unsigned char)0xf8);
			i2c_put_byte_add8_new(client, (unsigned char)0xcf, (unsigned char)0xfd);
			break;
		case SP0838_RGB_Gamma_m2:
			i2c_put_byte_add8_new(client,(unsigned char)0xBF, (unsigned char)0x08);
			i2c_put_byte_add8_new(client,(unsigned char)0xc0, (unsigned char)0x0F);
			i2c_put_byte_add8_new(client,(unsigned char)0xc1, (unsigned char)0x21);
			i2c_put_byte_add8_new(client,(unsigned char)0xc2, (unsigned char)0x32);
			i2c_put_byte_add8_new(client,(unsigned char)0xc3, (unsigned char)0x43);
			i2c_put_byte_add8_new(client,(unsigned char)0xc4, (unsigned char)0x50);
			i2c_put_byte_add8_new(client,(unsigned char)0xc5, (unsigned char)0x5E);
			i2c_put_byte_add8_new(client,(unsigned char)0xc6, (unsigned char)0x78);
			i2c_put_byte_add8_new(client,(unsigned char)0xc7, (unsigned char)0x90);
			i2c_put_byte_add8_new(client,(unsigned char)0xc8, (unsigned char)0xA6);
			i2c_put_byte_add8_new(client,(unsigned char)0xc9, (unsigned char)0xB9);
			i2c_put_byte_add8_new(client,(unsigned char)0xcA, (unsigned char)0xC9);
			i2c_put_byte_add8_new(client,(unsigned char)0xcB, (unsigned char)0xD6);
			i2c_put_byte_add8_new(client,(unsigned char)0xcC, (unsigned char)0xE0);
			i2c_put_byte_add8_new(client,(unsigned char)0xcD, (unsigned char)0xEE);
			i2c_put_byte_add8_new(client,(unsigned char)0xcE, (unsigned char)0xF8);
			i2c_put_byte_add8_new(client,(unsigned char)0xcF, (unsigned char)0xFF);
			break;               
			
		case SP0838_RGB_Gamma_m3:			
			i2c_put_byte_add8_new(client, (unsigned char)0xBF, (unsigned char)0x0B);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x16);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x29);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x3C);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x4F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x5F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x6F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0x8A);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0x9F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xB4);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xC6);
			i2c_put_byte_add8_new(client, (unsigned char)0xcA, (unsigned char)0xD3);
			i2c_put_byte_add8_new(client, (unsigned char)0xcB, (unsigned char)0xDD);
			i2c_put_byte_add8_new(client, (unsigned char)0xcC, (unsigned char)0xE5);
			i2c_put_byte_add8_new(client, (unsigned char)0xcD, (unsigned char)0xF1);
			i2c_put_byte_add8_new(client, (unsigned char)0xcE, (unsigned char)0xFA);
			i2c_put_byte_add8_new(client, (unsigned char)0xcF, (unsigned char)0xFF);
			break;                    
			
		case SP0838_RGB_Gamma_m4:
			i2c_put_byte_add8_new(client, (unsigned char)0xBF, (unsigned char)0x0E);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x1C);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x34);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x48);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x5A);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x6B);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x7B);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0x95);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0xAB);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xBF);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xCE);
			i2c_put_byte_add8_new(client, (unsigned char)0xcA, (unsigned char)0xD9);
			i2c_put_byte_add8_new(client, (unsigned char)0xcB, (unsigned char)0xE4);
			i2c_put_byte_add8_new(client, (unsigned char)0xcC, (unsigned char)0xEC);
			i2c_put_byte_add8_new(client, (unsigned char)0xcD, (unsigned char)0xF7);
			i2c_put_byte_add8_new(client, (unsigned char)0xcE, (unsigned char)0xFD);
			i2c_put_byte_add8_new(client, (unsigned char)0xcF, (unsigned char)0xFF);
			break;               
			
		case SP0838_RGB_Gamma_m5:
			i2c_put_byte_add8_new(client, (unsigned char)0xBF, (unsigned char)0x10);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x20);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x38);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x4E);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x63);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x76);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x87);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0xA2);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0xB8);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xCA);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xD8);
			i2c_put_byte_add8_new(client, (unsigned char)0xcA, (unsigned char)0xE3);
			i2c_put_byte_add8_new(client, (unsigned char)0xcB, (unsigned char)0xEB);
			i2c_put_byte_add8_new(client, (unsigned char)0xcC, (unsigned char)0xF0);
			i2c_put_byte_add8_new(client, (unsigned char)0xcD, (unsigned char)0xF8);
			i2c_put_byte_add8_new(client, (unsigned char)0xcE, (unsigned char)0xFD);
			i2c_put_byte_add8_new(client, (unsigned char)0xcF, (unsigned char)0xFF);
			break;                   
			
		case SP0838_RGB_Gamma_m6:										// largest gamma curve
			i2c_put_byte_add8_new(client, (unsigned char)0xBF, (unsigned char)0x14);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x28);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x44);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x5D);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x72);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x86);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x95);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0xB1);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0xC6);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xD5);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xE1);
			i2c_put_byte_add8_new(client, (unsigned char)0xcA, (unsigned char)0xEA);
			i2c_put_byte_add8_new(client, (unsigned char)0xcB, (unsigned char)0xF1);
			i2c_put_byte_add8_new(client, (unsigned char)0xcC, (unsigned char)0xF5);
			i2c_put_byte_add8_new(client, (unsigned char)0xcD, (unsigned char)0xFB);
			i2c_put_byte_add8_new(client, (unsigned char)0xcE, (unsigned char)0xFE);
			i2c_put_byte_add8_new(client, (unsigned char)0xcF, (unsigned char)0xFF);
			break;                 
		case SP0838_RGB_Gamma_night:									//Gamma for night mode
			i2c_put_byte_add8_new(client, (unsigned char)0xBF, (unsigned char)0x0B);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x16);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x29);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x3C);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x4F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x5F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x6F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0x8A);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0x9F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xB4);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xC6);
			i2c_put_byte_add8_new(client, (unsigned char)0xcA, (unsigned char)0xD3);
			i2c_put_byte_add8_new(client, (unsigned char)0xcB, (unsigned char)0xDD);
			i2c_put_byte_add8_new(client, (unsigned char)0xcC, (unsigned char)0xE5);
			i2c_put_byte_add8_new(client, (unsigned char)0xcD, (unsigned char)0xF1);
			i2c_put_byte_add8_new(client, (unsigned char)0xcE, (unsigned char)0xFA);
			i2c_put_byte_add8_new(client, (unsigned char)0xcF, (unsigned char)0xFF);
			break;                   
		default:/*
			//SP0838_RGB_Gamma_m1
			i2c_put_byte_add8_new(client, (unsigned char)0xfe, (unsigned char)0x00);
			i2c_put_byte_add8_new(client, (unsigned char)0xbf, (unsigned char)0x06);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x12);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x22);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x35);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x4b);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x5f);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x72);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0x8d);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0xa4);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xb8);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xc8);
			i2c_put_byte_add8_new(client, (unsigned char)0xca, (unsigned char)0xd4);
			i2c_put_byte_add8_new(client, (unsigned char)0xcb, (unsigned char)0xde);
			i2c_put_byte_add8_new(client, (unsigned char)0xcc, (unsigned char)0xe6);
			i2c_put_byte_add8_new(client, (unsigned char)0xcd, (unsigned char)0xf1);
			i2c_put_byte_add8_new(client, (unsigned char)0xce, (unsigned char)0xf8);
			i2c_put_byte_add8_new(client, (unsigned char)0xcf, (unsigned char)0xfd);
			*/
			// SP0838_RGB_Gamma_m3:			
			/*
			i2c_put_byte_add8_new(client, (unsigned char)0xBF, (unsigned char)0x0B);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x16);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x29);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x3C);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x4F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x5F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x6F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0x8A);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0x9F);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xB4);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xC6);
			i2c_put_byte_add8_new(client, (unsigned char)0xcA, (unsigned char)0xD3);
			i2c_put_byte_add8_new(client, (unsigned char)0xcB, (unsigned char)0xDD);
			i2c_put_byte_add8_new(client, (unsigned char)0xcC, (unsigned char)0xE5);
			i2c_put_byte_add8_new(client, (unsigned char)0xcD, (unsigned char)0xF1);
			i2c_put_byte_add8_new(client, (unsigned char)0xcE, (unsigned char)0xFA);
			i2c_put_byte_add8_new(client, (unsigned char)0xcF, (unsigned char)0xFF);
			*/
			
			i2c_put_byte_add8_new(client, (unsigned char)0xbf, (unsigned char)0x06);
			i2c_put_byte_add8_new(client, (unsigned char)0xc0, (unsigned char)0x14);
			i2c_put_byte_add8_new(client, (unsigned char)0xc1, (unsigned char)0x27);
			i2c_put_byte_add8_new(client, (unsigned char)0xc2, (unsigned char)0x3b);
			i2c_put_byte_add8_new(client, (unsigned char)0xc3, (unsigned char)0x4f);
			i2c_put_byte_add8_new(client, (unsigned char)0xc4, (unsigned char)0x62);
			i2c_put_byte_add8_new(client, (unsigned char)0xc5, (unsigned char)0x72);
			i2c_put_byte_add8_new(client, (unsigned char)0xc6, (unsigned char)0x8d);
			i2c_put_byte_add8_new(client, (unsigned char)0xc7, (unsigned char)0xa4);
			i2c_put_byte_add8_new(client, (unsigned char)0xc8, (unsigned char)0xb8);
			i2c_put_byte_add8_new(client, (unsigned char)0xc9, (unsigned char)0xc9);
			i2c_put_byte_add8_new(client, (unsigned char)0xcA, (unsigned char)0xd6);
			i2c_put_byte_add8_new(client, (unsigned char)0xcB, (unsigned char)0xe0);
			i2c_put_byte_add8_new(client, (unsigned char)0xcC, (unsigned char)0xe8);
			i2c_put_byte_add8_new(client, (unsigned char)0xcD, (unsigned char)0xf4);
			i2c_put_byte_add8_new(client, (unsigned char)0xcE, (unsigned char)0xFc);
			i2c_put_byte_add8_new(client, (unsigned char)0xcF, (unsigned char)0xFF);
			
			break;
	}
}

void SP0838write_more_registers(struct sp0838_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);	
	SP0838GammaSelect(dev,0);// SP0838_RGB_Gamma_m3);	
}

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
//	uint16 rgain=0x80, ggain=0x80, bgain=0x80;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	printk(" camera set_SP0838_param_wb=%d. \n ",para);
	switch (para)
	{
		case CAM_WB_AUTO:
			
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x01);
			i2c_put_byte_add8_new(client, (unsigned char)0x28, (unsigned char)Pre_Value_P1_0x28);
			i2c_put_byte_add8_new(client, (unsigned char)0x29, (unsigned char)Pre_Value_P1_0x29);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x00);
	//		i2c_put_byte_add8_new(client, (unsigned char)0x32, (unsigned char)0x15);
	//		i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x00);

			SP0838AwbEnable(dev, 1);
			break;

		case CAM_WB_CLOUD:
			SP0838AwbEnable(dev, 0);

			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x01);
			i2c_put_byte_add8_new(client, (unsigned char)0x28, (unsigned char)0x71);
			i2c_put_byte_add8_new(client, (unsigned char)0x29, (unsigned char)0x41);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x00);

			break;

		case CAM_WB_DAYLIGHT:   // tai yang guang
			SP0838AwbEnable(dev, 0);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x01);
			i2c_put_byte_add8_new(client, (unsigned char)0x28, (unsigned char)0x6b);
			i2c_put_byte_add8_new(client, (unsigned char)0x29, (unsigned char)0x48);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x00);
			break;

		case CAM_WB_INCANDESCENCE:   // bai re guang
			SP0838AwbEnable(dev, 0);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x01);
			i2c_put_byte_add8_new(client, (unsigned char)0x28, (unsigned char)0x41);
			i2c_put_byte_add8_new(client, (unsigned char)0x29, (unsigned char)0x71);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x00);
			break;

		case CAM_WB_FLUORESCENT:   //ri guang deng
			SP0838AwbEnable(dev, 0);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x01);
			i2c_put_byte_add8_new(client, (unsigned char)0x28, (unsigned char)0x5a);
			i2c_put_byte_add8_new(client, (unsigned char)0x29, (unsigned char)0x62);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x00);
			break;

		case CAM_WB_TUNGSTEN:   // wu si deng
			SP0838AwbEnable(dev, 0);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x01);
			i2c_put_byte_add8_new(client, (unsigned char)0x28, (unsigned char)0x57);
			i2c_put_byte_add8_new(client, (unsigned char)0x29, (unsigned char)0x66);
			i2c_put_byte_add8_new(client, (unsigned char)0xfd, (unsigned char)0x00);
			break;

		case CAM_WB_MANUAL:
			// TODO
			break;
		default:
			break;
	}
//	sleep_task(20);
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
void SP0838_BANDING_NIGHT_MODE(struct i2c_client *client)
{
	if(SP0838Sensor.NightMode == 0) {
		if(SP0838Sensor.BandingFreq== CAM_BANDING_50HZ) {
			//caprure preview daylight 24M 50hz 20-8FPS maxgain:0x70   
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x00);
			i2c_put_byte_add8_new(client,(unsigned char)0x05,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0x06,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0x09,(unsigned char)0x1 );
			i2c_put_byte_add8_new(client,(unsigned char)0x0a,(unsigned char)0x76);
			i2c_put_byte_add8_new(client,(unsigned char)0xf0,(unsigned char)0x62);
			i2c_put_byte_add8_new(client,(unsigned char)0xf1,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0xf2,(unsigned char)0x5f);
			i2c_put_byte_add8_new(client,(unsigned char)0xf5,(unsigned char)0x78);
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x01);
			i2c_put_byte_add8_new(client,(unsigned char)0x00,(unsigned char)0xb2);
			i2c_put_byte_add8_new(client,(unsigned char)0x0f,(unsigned char)0x60);
			i2c_put_byte_add8_new(client,(unsigned char)0x16,(unsigned char)0x60);
			i2c_put_byte_add8_new(client,(unsigned char)0x17,(unsigned char)0xa2);
			i2c_put_byte_add8_new(client,(unsigned char)0x18,(unsigned char)0xaa);
			i2c_put_byte_add8_new(client,(unsigned char)0x1b,(unsigned char)0x60);
			i2c_put_byte_add8_new(client,(unsigned char)0x1c,(unsigned char)0xaa);
			i2c_put_byte_add8_new(client,(unsigned char)0xb4,(unsigned char)0x20);
			i2c_put_byte_add8_new(client,(unsigned char)0xb5,(unsigned char)0x3a);
			i2c_put_byte_add8_new(client,(unsigned char)0xb6,(unsigned char)0x5e);
			i2c_put_byte_add8_new(client,(unsigned char)0xb9,(unsigned char)0x40);
			i2c_put_byte_add8_new(client,(unsigned char)0xba,(unsigned char)0x4f);
			i2c_put_byte_add8_new(client,(unsigned char)0xbb,(unsigned char)0x47);
			i2c_put_byte_add8_new(client,(unsigned char)0xbc,(unsigned char)0x45);
			i2c_put_byte_add8_new(client,(unsigned char)0xbd,(unsigned char)0x43);
			i2c_put_byte_add8_new(client,(unsigned char)0xbe,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xbf,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xc0,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xc1,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc2,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc3,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc4,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc5,(unsigned char)0x70);
			i2c_put_byte_add8_new(client,(unsigned char)0xc6,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xca,(unsigned char)0x70);
			i2c_put_byte_add8_new(client,(unsigned char)0xcb,(unsigned char)0xc );
			i2c_put_byte_add8_new(client,(unsigned char)0x14,(unsigned char)0x20);
			i2c_put_byte_add8_new(client,(unsigned char)0x15,(unsigned char)0x0f);
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x00);
		} else {

			//caprure preview daylight 24M 60hz 20-8FPS maxgain:0x70	       
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x00);
			i2c_put_byte_add8_new(client,(unsigned char)0x05,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0x06,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0x09,(unsigned char)0x1 );
			i2c_put_byte_add8_new(client,(unsigned char)0x0a,(unsigned char)0x76);
			i2c_put_byte_add8_new(client,(unsigned char)0xf0,(unsigned char)0x51);
			i2c_put_byte_add8_new(client,(unsigned char)0xf1,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0xf2,(unsigned char)0x5c);
			i2c_put_byte_add8_new(client,(unsigned char)0xf5,(unsigned char)0x75);
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x01);
			i2c_put_byte_add8_new(client,(unsigned char)0x00,(unsigned char)0xb4);
			i2c_put_byte_add8_new(client,(unsigned char)0x0f,(unsigned char)0x5d);
			i2c_put_byte_add8_new(client,(unsigned char)0x16,(unsigned char)0x5d);
			i2c_put_byte_add8_new(client,(unsigned char)0x17,(unsigned char)0xa4);
			i2c_put_byte_add8_new(client,(unsigned char)0x18,(unsigned char)0xac);
			i2c_put_byte_add8_new(client,(unsigned char)0x1b,(unsigned char)0x5d);
			i2c_put_byte_add8_new(client,(unsigned char)0x1c,(unsigned char)0xac);
			i2c_put_byte_add8_new(client,(unsigned char)0xb4,(unsigned char)0x21);
			i2c_put_byte_add8_new(client,(unsigned char)0xb5,(unsigned char)0x3d);
			i2c_put_byte_add8_new(client,(unsigned char)0xb6,(unsigned char)0x4d);
			i2c_put_byte_add8_new(client,(unsigned char)0xb9,(unsigned char)0x40);
			i2c_put_byte_add8_new(client,(unsigned char)0xba,(unsigned char)0x4f);
			i2c_put_byte_add8_new(client,(unsigned char)0xbb,(unsigned char)0x47);
			i2c_put_byte_add8_new(client,(unsigned char)0xbc,(unsigned char)0x45);
			i2c_put_byte_add8_new(client,(unsigned char)0xbd,(unsigned char)0x43);
			i2c_put_byte_add8_new(client,(unsigned char)0xbe,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xbf,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xc0,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xc1,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc2,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc3,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc4,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc5,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc6,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xca,(unsigned char)0x70);
			i2c_put_byte_add8_new(client,(unsigned char)0xcb,(unsigned char)0xf );
			i2c_put_byte_add8_new(client,(unsigned char)0x14,(unsigned char)0x20);
			i2c_put_byte_add8_new(client,(unsigned char)0x15,(unsigned char)0x0f);
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x00);
		}	
	} else {
		if(SP0838Sensor.BandingFreq== CAM_BANDING_50HZ) {
			//SCI_TRACE_LOW("night mode 50hz\r\n");
			//caprure preview night 24M 50hz 20-6FPS maxgain:0x78		   
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x00);
			i2c_put_byte_add8_new(client,(unsigned char)0x05,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0x06,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0x09,(unsigned char)0x1 );
			i2c_put_byte_add8_new(client,(unsigned char)0x0a,(unsigned char)0x76);
			i2c_put_byte_add8_new(client,(unsigned char)0xf0,(unsigned char)0x62);
			i2c_put_byte_add8_new(client,(unsigned char)0xf1,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0xf2,(unsigned char)0x5f);
			i2c_put_byte_add8_new(client,(unsigned char)0xf5,(unsigned char)0x78);
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x01);
			i2c_put_byte_add8_new(client,(unsigned char)0x00,(unsigned char)0xc0);
			i2c_put_byte_add8_new(client,(unsigned char)0x0f,(unsigned char)0x60);
			i2c_put_byte_add8_new(client,(unsigned char)0x16,(unsigned char)0x60);
			i2c_put_byte_add8_new(client,(unsigned char)0x17,(unsigned char)0xa8);
			i2c_put_byte_add8_new(client,(unsigned char)0x18,(unsigned char)0xb0);
			i2c_put_byte_add8_new(client,(unsigned char)0x1b,(unsigned char)0x60);
			i2c_put_byte_add8_new(client,(unsigned char)0x1c,(unsigned char)0xb0);
			i2c_put_byte_add8_new(client,(unsigned char)0xb4,(unsigned char)0x20);
			i2c_put_byte_add8_new(client,(unsigned char)0xb5,(unsigned char)0x3a);
			i2c_put_byte_add8_new(client,(unsigned char)0xb6,(unsigned char)0x5e);
			i2c_put_byte_add8_new(client,(unsigned char)0xb9,(unsigned char)0x40);
			i2c_put_byte_add8_new(client,(unsigned char)0xba,(unsigned char)0x4f);
			i2c_put_byte_add8_new(client,(unsigned char)0xbb,(unsigned char)0x47);
			i2c_put_byte_add8_new(client,(unsigned char)0xbc,(unsigned char)0x45);
			i2c_put_byte_add8_new(client,(unsigned char)0xbd,(unsigned char)0x43);
			i2c_put_byte_add8_new(client,(unsigned char)0xbe,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xbf,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xc0,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xc1,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc2,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc3,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc4,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc5,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc6,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xca,(unsigned char)0x78);
			i2c_put_byte_add8_new(client,(unsigned char)0xcb,(unsigned char)0x10);
			i2c_put_byte_add8_new(client,(unsigned char)0x14,(unsigned char)0x20);
			i2c_put_byte_add8_new(client,(unsigned char)0x15,(unsigned char)0x1f);
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x00);
		} else {
			
			//caprure preview night 24M 60hz 20-6FPS maxgain:0x78
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x00);
			i2c_put_byte_add8_new(client,(unsigned char)0x05,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0x06,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0x09,(unsigned char)0x1 );
			i2c_put_byte_add8_new(client,(unsigned char)0x0a,(unsigned char)0x76);
			i2c_put_byte_add8_new(client,(unsigned char)0xf0,(unsigned char)0x51);
			i2c_put_byte_add8_new(client,(unsigned char)0xf1,(unsigned char)0x0 );
			i2c_put_byte_add8_new(client,(unsigned char)0xf2,(unsigned char)0x5c);
			i2c_put_byte_add8_new(client,(unsigned char)0xf5,(unsigned char)0x75);
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x01);
			i2c_put_byte_add8_new(client,(unsigned char)0x00,(unsigned char)0xc1);
			i2c_put_byte_add8_new(client,(unsigned char)0x0f,(unsigned char)0x5d);
			i2c_put_byte_add8_new(client,(unsigned char)0x16,(unsigned char)0x5d);
			i2c_put_byte_add8_new(client,(unsigned char)0x17,(unsigned char)0xa9);
			i2c_put_byte_add8_new(client,(unsigned char)0x18,(unsigned char)0xb1);
			i2c_put_byte_add8_new(client,(unsigned char)0x1b,(unsigned char)0x5d);
			i2c_put_byte_add8_new(client,(unsigned char)0x1c,(unsigned char)0xb1);
			i2c_put_byte_add8_new(client,(unsigned char)0xb4,(unsigned char)0x21);
			i2c_put_byte_add8_new(client,(unsigned char)0xb5,(unsigned char)0x3d);
			i2c_put_byte_add8_new(client,(unsigned char)0xb6,(unsigned char)0x4d);
			i2c_put_byte_add8_new(client,(unsigned char)0xb9,(unsigned char)0x40);
			i2c_put_byte_add8_new(client,(unsigned char)0xba,(unsigned char)0x4f);
			i2c_put_byte_add8_new(client,(unsigned char)0xbb,(unsigned char)0x47);
			i2c_put_byte_add8_new(client,(unsigned char)0xbc,(unsigned char)0x45);
			i2c_put_byte_add8_new(client,(unsigned char)0xbd,(unsigned char)0x43);
			i2c_put_byte_add8_new(client,(unsigned char)0xbe,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xbf,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xc0,(unsigned char)0x42);
			i2c_put_byte_add8_new(client,(unsigned char)0xc1,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc2,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc3,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc4,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc5,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xc6,(unsigned char)0x41);
			i2c_put_byte_add8_new(client,(unsigned char)0xca,(unsigned char)0x78);
			i2c_put_byte_add8_new(client,(unsigned char)0xcb,(unsigned char)0x14);
			i2c_put_byte_add8_new(client,(unsigned char)0x14,(unsigned char)0x20);
			i2c_put_byte_add8_new(client,(unsigned char)0x15,(unsigned char)0x1f);
			i2c_put_byte_add8_new(client,(unsigned char)0xfd,(unsigned char)0x00);
		}		
	}
}


void SP0838_night_mode(struct sp0838_device *dev,enum  camera_night_mode_flip_e enable)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	SP0838Sensor.NightMode = (bool)enable;
	SP0838_BANDING_NIGHT_MODE(client);
		

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

void SP0838_set_param_banding(struct sp0838_device *dev,enum  camera_night_mode_flip_e banding)
{     
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    switch(banding) {
        case CAM_BANDING_60HZ:
			SP0838Sensor.BandingFreq = CAM_BANDING_50HZ;
			sp0838_banding_60HZ = 1;
            break;
        case CAM_BANDING_50HZ:
			SP0838Sensor.BandingFreq = CAM_BANDING_60HZ;
			sp0838_banding_60HZ = 0;
            break;
    }
	
	SP0838_BANDING_NIGHT_MODE(client);
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


	unsigned char value_luma, value_Y;

//	return;
	/*switch night or normal mode*/
//	value_luma = (SP0838Sensor.NightMode?0x2b:0x08);
//	value_Y = (SP0838Sensor.NightMode?0x68:0x58);
	switch (para) {
		case EXPOSURE_N4_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0xc0);
			break;
		case EXPOSURE_N3_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0xd0);
			break;
		case EXPOSURE_N2_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0xe0);
			break;
		case EXPOSURE_N1_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0xf0);
			break;
		case EXPOSURE_0_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0x00);
			break;
		case EXPOSURE_P1_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0x10);
			break;
		case EXPOSURE_P2_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0x20);
			break;
		case EXPOSURE_P3_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0x30);
			break;
		case EXPOSURE_P4_STEP:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0x40);
			break;
		default:
			i2c_put_byte_add8_new(client,0xfd, 0x00);
			i2c_put_byte_add8_new(client,0xdc, 0x00);
			break;
	}
	//msleep(300);
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

//	return ;
	switch (para) {

		case CAM_EFFECT_ENC_NORMAL:
			i2c_put_byte_add8_new(client,0xfd , 0x00);
			i2c_put_byte_add8_new(client,0x62 , 0x00);
			i2c_put_byte_add8_new(client,0x63 , 0x80);
			i2c_put_byte_add8_new(client,0x64 , 0x80);

			break;
		case CAM_EFFECT_ENC_GRAYSCALE:
			i2c_put_byte_add8_new(client,0xfd , 0x00);
			i2c_put_byte_add8_new(client,0x62 , 0x40);
			i2c_put_byte_add8_new(client,0x63 , 0x80);
			i2c_put_byte_add8_new(client,0x64 , 0x80);
			break;
		case CAM_EFFECT_ENC_SEPIA:
			i2c_put_byte_add8_new(client,0xfd , 0x00);
			i2c_put_byte_add8_new(client,0x62 , 0x20);
			i2c_put_byte_add8_new(client,0x63 , 0xc0);
			i2c_put_byte_add8_new(client,0x64 , 0x20);

			break;
		case CAM_EFFECT_ENC_COLORINV:
			i2c_put_byte_add8_new(client,0xfd , 0x00);
			i2c_put_byte_add8_new(client,0x62 , 0x10);
			i2c_put_byte_add8_new(client,0x63 , 0x80);
			i2c_put_byte_add8_new(client,0x64 , 0x80);

			break;
		case CAM_EFFECT_ENC_SEPIAGREEN:
			i2c_put_byte_add8_new(client,0xfd , 0x00);
			i2c_put_byte_add8_new(client,0x62 , 0x20);
			i2c_put_byte_add8_new(client,0x63 , 0x20);
			i2c_put_byte_add8_new(client,0x64 , 0x20);

			break;
		case CAM_EFFECT_ENC_SEPIABLUE:
			i2c_put_byte_add8_new(client,0xfd , 0x00);
			i2c_put_byte_add8_new(client,0x62 , 0x20);
			i2c_put_byte_add8_new(client,0x63 , 0x20);
			i2c_put_byte_add8_new(client,0x64 , 0xf0);

			break;
		default:
			break;
	}

}

unsigned char v4l_2_sp0838(int val)
{/*
	int ret=val/0x20;
	if(ret<4) return ret*0x20+0x80;
	else if(ret<8) return ret*0x20+0x20;
	else return 0;*/
}

static int sp0838_setting(struct sp0838_device *dev,int PROP_ID,int value )
{
	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
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
		case V4L2_CID_WHITENESS:
			if(sp0838_qctrl[3].default_value!=value){
				sp0838_qctrl[3].default_value=value;
				SP0838_set_param_banding(dev,value);
				printk(KERN_INFO " set camera  banding=%d. \n ",value);
			}
			break;
		case V4L2_CID_BLUE_BALANCE:
			if(sp0838_qctrl[4].default_value!=value){
				sp0838_qctrl[4].default_value=value;
				SP0838_night_mode(dev,value);
				printk(KERN_INFO " set camera  scene mode=%d. \n ",value);
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
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[4];
	return;
	msleep(5);
	return;
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/
#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void sp0838_fillbuff(struct sp0838_fh *fh, struct sp0838_buffer *buf)
{
	struct sp0838_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	vm_output_para_t para = {0};
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
    dprintk(dev, 1, "list entry get buf is %x\n",(unsigned)buf);

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
#if 1
	if(f->fmt.pix.pixelformat==V4L2_PIX_FMT_RGB24){
		sp0838_set_resolution(fh->dev,fh->height,fh->width);
	} else {
		sp0838_set_resolution(fh->dev,fh->height,fh->width);
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
	struct sp0838_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct sp0838_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

struct sp0838_device* g_dev;  

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct sp0838_fh *fh = priv;
	static int i = 0;
	i++;
	if(i > 50) {
		//printk("100 frame\n");       
		i = 0;
	}

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

    para.port  = TVIN_PORT_CAMERA_YUYV;
    para.fmt_info.fmt = TVIN_SIG_FMT_MAX+1;//TVIN_SIG_FMT_MAX+1;TVIN_SIG_FMT_CAMERA_1280X720P_30Hz
	para.fmt_info.frame_rate = 236;
	para.fmt_info.h_active = sp0838_h_active;
	para.fmt_info.v_active = sp0838_v_active;
	para.fmt_info.hsync_phase = 1;
	para.fmt_info.vsync_phase = 0;	
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

static int vidioc_enum_framesizes(struct file *file, void *fh,struct v4l2_frmsizeenum *fsize)
{
	int ret = 0,i=0;
	struct sp0838_fmt *fmt = NULL;
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
		if (fsize->index >= ARRAY_SIZE(sp0838_prev_resolution))
			return -EINVAL;
		frmsize = &sp0838_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
		if (fsize->index >= ARRAY_SIZE(sp0838_pic_resolution))
			return -EINVAL;
		frmsize = &sp0838_pic_resolution[fsize->index];
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
	sp0838_have_open=1;
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	SP0838_init_regs(dev);
	SP0838write_more_registers(dev);
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
	sp0838_have_open=0;

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
	sp0838_qctrl[0].default_value=0;
	sp0838_qctrl[1].default_value=4;
	sp0838_qctrl[2].default_value=0;
	sp0838_qctrl[3].default_value=0;
	sp0838_qctrl[4].default_value=0;

	//power_down_sp0838(dev);
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
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
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
static struct i2c_client *this_client;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void aml_sp0838_early_suspend(struct early_suspend *h)
{
	printk("enter -----> %s \n",__FUNCTION__);
	if(h && h->param && sp0838_have_open) {
		aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)h->param;
		if (plat_dat && plat_dat->early_suspend) {
			plat_dat->early_suspend();
		}
	}
}

static void aml_sp0838_late_resume(struct early_suspend *h)
{
	aml_plat_cam_data_t* plat_dat;
	if(sp0838_have_open){
	    printk("enter -----> %s \n",__FUNCTION__);
	    if(h && h->param) {
		    plat_dat= (aml_plat_cam_data_t*)h->param;
		    if (plat_dat && plat_dat->late_resume) {
			    plat_dat->late_resume();
		    }
	    }
	}
}
#endif

static int sp0838_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	aml_plat_cam_data_t* plat_dat;
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
	plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	/* test if devices exist. */
#if 0//ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_PROBE
	unsigned char buf[4];
	buf[0] = 0xfc;
	buf[0] = 0x16;
	plat_dat->device_init();
	err=i2c_put_byte_add8(client,buf, 2);
	msleep(20);
	buf[0]=0;
	err=i2c_get_byte_add8(client,buf);
	plat_dat->device_uninit();
	if(err<0 || err != 0xc0) {
		printk("can not get sp0838 id, id=%x\n", err);
		return  -ENODEV;
	}
	printk("sp0838 id is %x\n", err);
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
	memcpy(t->vdev, &sp0838_template, sizeof(*t->vdev));

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
			//power_down_sp0838(t);
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
	g_dev = t;

#ifdef CONFIG_HAS_EARLYSUSPEND
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
static int sp0838_suspend(struct i2c_client *client, pm_message_t state)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp0838_device *t = to_dev(sd);
	struct sp0838_fh  *fh = to_fh(t);
	if (sp0838_have_open) {
	    if(fh->stream_on == 1){
		    stop_tvin_service(0);
	    }
	    power_down_sp0838(t);
	}
	return 0;
}

static int sp0838_resume(struct i2c_client *client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct sp0838_device *t = to_dev(sd);
    struct sp0838_fh  *fh = to_fh(t);
    tvin_parm_t para;
    if (sp0838_have_open) {
        para.port  = TVIN_PORT_CAMERA_YUYV;
        para.fmt_info.fmt = TVIN_SIG_FMT_MAX+1;
        SP0838_init_regs(t);
	if(fh->stream_on == 1){
            start_tvin_service(0,&para);
	}
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

