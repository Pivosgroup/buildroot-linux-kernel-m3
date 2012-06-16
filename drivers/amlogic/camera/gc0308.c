/*
 * I2C chip deivce driver template
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:
 *
 */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>

#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>  //copy_from_user copy_to_user



/* Amlogic headers */
#include <mach/am_regs.h>
#include <mach/pinmux.h>
#include "gc0308.h"

#ifdef CONFIG_SN7325
#include <linux/sn7325.h>
#endif

/* specify your i2c device address according the device datasheet */
//#define I2C_CAMERA  0x60  /* @todo to be changed according your device */

#define GC0308_DEVICE_NAME      "camera_gc0308"
#define GC0308_I2C_NAME         "gc0308_i2c"


#define CAMERA_COUNT              1

static dev_t camera_devno;
static struct class *camera_clsp = NULL;

static camera_dev_t *camera_devp = NULL;

static struct i2c_client *camera_client = NULL;

static struct GC0308_ctl_s GC0308_info = {
    .para = {
        .saturation = SATURATION_0_STEP,
        .brighrness = BRIGHTNESS_0_STEP,
        .contrast = CONTRAST_0_STEP,
        .hue = HUE_0_DEGREE,
//      .special_effect = SPECIAL_EFFECT_NORMAL,
        .exposure = EXPOSURE_0_STEP,
        .sharpness = SHARPNESS_AUTO_STEP,
        .mirro_flip = MF_NORMAL,
        .resolution = TVIN_SIG_FMT_CAMERA_1280X720P_30Hz,
    },
    .awb_mode = ADVANCED_AWB,
    .test_mode = TEST_OFF,
};

static int  GC0308_read(char *buf, int len)
{
    int  i2c_flag = -1;
	struct i2c_msg msgs[] = {
		{
			.addr	= camera_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= camera_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	i2c_flag = i2c_transfer(camera_client->adapter, msgs, 2);

	return i2c_flag;
}

static int  GC0308_write(char *buf, int len)
{
	struct i2c_msg msg[] = {
	    {
			.addr	= camera_client->addr,
			.flags	= 0,    //|I2C_M_TEN,
			.len	= len,
			.buf	= buf,
		}

	};

		if (i2c_transfer(camera_client->adapter, msg, 1) < 0) {
//			unsigned short off_addr = buf[0] | (buf[1] << 8);
//			pr_info("error in writing GC0308 register(%x). \n", off_addr);
			return -1;
		}
		else
			return 0;
}

static int  GC0308_write_byte(unsigned char addr, unsigned char data)
{
    unsigned char buff[4];
    buff[0] = addr ; //(unsigned char)((addr >> 8) & 0xff);
   // buff[1] = (unsigned char)(addr & 0xff);
    buff[2] = data;
    if((GC0308_write(buff, 2)) < 0)
        return -1;
    else
        return 0;
}


static unsigned char  GC0308_read_byte(unsigned char addr )
{
    unsigned char buff[4];
    buff[0] = addr;//(unsigned char)((addr >> 8) & 0xff);
    //buff[1] = (unsigned char)(addr & 0xff);
    if((GC0308_read(buff, 1)) < 0)
        return -1;
    else
    {
        return buff[0];
    }
}

static void set_camera_light_mode(enum camera_light_mode_e mode)
{
	return;
	/*
    int i ;
    struct GC0308_i2c_fig0_s GC0308_data[16] = {};

    switch(mode)
    {
        case SIMPLE_AWB:
            break;

        case MANUAL_DAY:
            break;

        case MANUAL_A:
            break;

        case MANUAL_CWF:
            break;

         case MANUAL_CLOUDY:
            break;

        case ADVANCED_AWB:
        default:
            break;

    }
   */
}


static void set_camera_saturation(enum camera_saturation_e saturation)
{
    unsigned char saturation_u = 0, saturation_v = 0;
    switch(saturation)
    {
        case SATURATION_N4_STEP:
            saturation_u = 0x00;
            saturation_v = 0x00;
            break;

        case SATURATION_N3_STEP:
            saturation_u = 0x10;
            saturation_v = 0x10;
            break;

        case SATURATION_N2_STEP:
            saturation_u = 0x20;
            saturation_v = 0x20;
            break;

        case SATURATION_N1_STEP:
            saturation_u = 0x30;
            saturation_v = 0x30;
            break;

        case SATURATION_P1_STEP:
            saturation_u = 0x50;
            saturation_v = 0x50;
            break;

        case SATURATION_P2_STEP:
            saturation_u = 0x60;
            saturation_v = 0x60;
            break;

        case SATURATION_P3_STEP:
            saturation_u = 0x70;
            saturation_v = 0x70;
            break;

        case SATURATION_P4_STEP:
            saturation_u = 0x80;
            saturation_v = 0x80;
            break;

        case SATURATION_0_STEP:
        default:
            saturation_u = 0x40;
            saturation_v = 0x40;
            break;
    }
    GC0308_write_byte(0xb1, saturation_u);
    GC0308_write_byte(0xb2, saturation_v);

 }


static void set_camera_brightness(enum camera_brightness_e brighrness)
{
    unsigned char y_bright = 0;
    switch(brighrness)
    {
        case BRIGHTNESS_N4_STEP:
            y_bright = 0xe0;
            break;

        case BRIGHTNESS_N3_STEP:
            y_bright = 0xe8;
            break;

        case BRIGHTNESS_N2_STEP:
            y_bright = 0xf0;
            break;

        case BRIGHTNESS_N1_STEP:
            y_bright = 0xf8;
            break;

        case BRIGHTNESS_P1_STEP:
            y_bright = 0x08;
            break;

        case BRIGHTNESS_P2_STEP:
            y_bright = 0x10;
            break;

        case BRIGHTNESS_P3_STEP:
            y_bright = 0x18;
            break;

        case BRIGHTNESS_P4_STEP:
            y_bright = 0x20;
            break;

        case BRIGHTNESS_0_STEP:
        default:
            y_bright = 0x00;
            break;
    }
    GC0308_write_byte(0xb5, y_bright);   //
 }

static void set_camera_contrast(enum camera_contrast_e contrast)
{
    unsigned char  v_data;
    switch(contrast)
    {
        case BRIGHTNESS_N4_STEP:
            v_data = 0x30;
            break;

        case BRIGHTNESS_N3_STEP:
            v_data = 0x34;
            break;

        case BRIGHTNESS_N2_STEP:
            v_data = 0x38;
            break;

        case BRIGHTNESS_N1_STEP:
            v_data = 0x3c;
            break;

        case BRIGHTNESS_P1_STEP:
            v_data = 0x44;
            break;

        case BRIGHTNESS_P2_STEP:
            v_data = 0x48;
            break;

        case BRIGHTNESS_P3_STEP:
            v_data = 0x4c;
            break;

        case BRIGHTNESS_P4_STEP:
            v_data = 0x50;
            break;

        case BRIGHTNESS_0_STEP:
        default:
            v_data = 0x40;
            break;
    }
    GC0308_write_byte(0xb3, 0x40);   //
}

static void set_camera_hue(enum camera_hue_e hue)
{}

#if 0
static void set_camera_special_effect(enum camera_special_effect_e special_effect)
{

    switch(special_effect)
    {
        case SPECIAL_EFFECT_BW:

            break;

        case SPECIAL_EFFECT_BLUISH:

            break;

        case SPECIAL_EFFECT_SEPIA:

            break;

        case SPECIAL_EFFECT_REDDISH:

            break;

        case SPECIAL_EFFECT_GREENISH:

            break;

        case SPECIAL_EFFECT_NORMAL:
        default:

            break;
    }
}
#endif

static void set_camera_exposure(enum camera_exposure_e exposure)  // 
{
	unsigned char regd3, regb5;
	switch(exposure)
	    {
	        case EXPOSURE_N4_STEP:
				regd3 = 0x60;
				break;

	        case EXPOSURE_N3_STEP:
				regd3 = 0x68;
				break;

	        case EXPOSURE_N2_STEP:
				regd3 = 0x70;
				break;

	        case EXPOSURE_N1_STEP:
				regd3 = 0x78;
				break;

	        case EXPOSURE_P1_STEP:
				regd3 = 0x88;
				break;

	        case EXPOSURE_P2_STEP:
				regd3 = 0x90;
				break;

	        case EXPOSURE_P3_STEP:
				regd3 = 0x98;
				break;

	        case EXPOSURE_P4_STEP:
				regd3 = 0xa0;
	            break;

	        case EXPOSURE_0_STEP:
	        default:
				regd3 = 0x80;
	            break;
	    }

    GC0308_write_byte(0xd3, regd3);


}

static void set_camera_sharpness(enum camera_sharpness_e sharpness)
{
#if 0
		printk(" set_camera_sharpness  :  %d,\n", sharpness);

	switch(sharpness)
	    {
	        case SHARPNESS_1_STEP:
    		GC0308_write_byte(0x77, 0x36);
			break;

	        case SHARPNESS_2_STEP:
    		GC0308_write_byte(0x77, 0x36);
	            break;

	        case SHARPNESS_3_STEP:
    		GC0308_write_byte(0x77, 0x36);
	            break;

	        case SHARPNESS_4_STEP:
    		GC0308_write_byte(0x77, 0x36);
	            break;

	        case SHARPNESS_5_STEP:
    		GC0308_write_byte(0x77, 0x36);
	            break;

	        case SHARPNESS_6_STEP:
    		GC0308_write_byte(0x77, 0x36);
	            break;

	        case SHARPNESS_7_STEP:
    		GC0308_write_byte(0x77, 0x36);
	            break;

	        case SHARPNESS_8_STEP:
    		GC0308_write_byte(0x77, 0x36);
	            break;
		}
	#endif
}

static void set_camera_mirror_flip(enum camera_mirror_flip_e mirror_flip)
{

		switch(mirror_flip)
		    {
        		case MF_MIRROR:
				GC0308_write_byte(0x14, 0x11);
		            break;

		        case MF_FLIP:
				GC0308_write_byte(0x14, 0x12);
		            break;

		        case MF_MIRROR_FLIP:
				GC0308_write_byte(0x14, 0x13);
		            break;

		        case MF_NORMAL:
		        default:
				GC0308_write_byte(0x14, 0x10);
		            break;
		    }
}

static void set_camera_test_mode(enum camera_test_mode_e test_mode)
{}

static void set_camera_resolution_timing(enum tvin_sig_fmt_e resolution)
{}

static void set_camera_resolution_others(enum tvin_sig_fmt_e resolution)
{}

static void reset_sw_camera(void)
{}

static void power_down_sw_camera(void)
{

		GC0308_write_byte(0x25, 0x00);
		GC0308_write_byte(0x1a, 0x17);

}

static void wakeup_sw_camera(void)
{

		GC0308_write_byte(0x25, 0x0f);
		GC0308_write_byte(0x1a, 0x2a);
}

static void set_frame_rate(void)
{}

static void sensor_analog_ctl(void)
{}
static void sensor_sys_ctl(void)
{}

static void sensor_sys_ctl_0(void)
{}
static void sensor_set_color(void)
{}


static void set_camera_light_freq(void)
{}

static void set_other_para1(void)
{}

static void set_other_para2(void)
{}

static void set_other_para3(void)
{}


static void sensor_sys_ctl_1(void)
{}

static void set_camera_gamma(void)
{

}

static void set_lenc_para(void)
{}


#if 1





struct GC0308_i2c_fig_s GC0308_script[] = {  
	{0xfe,0x00},
	{0x0f,0x00},

#if 0

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

#if 1   // 50hz   8.3fps~16.6fps auto
	{0x01 , 0xfa},                                    
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
	{0x08,0x00},
	{0x09,0x01},
	{0x0a,0xe8},
	{0x0b,0x02},
	{0x0c,0x88},
	{0x0d,0x02},
	{0x0e,0x02},
	{0x10,0x26},
	{0x11,0x0d},
	{0x12,0x2a},
	{0x13,0x00},
	{0x14,0x10}, // h_v
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
	{0x20,0xff},
	{0x21,0xfa},
	{0x22,0x57},
	{0x24,0xa3},
	{0x25,0x0f},
	{0x26,0x03}, 
	{0x2f,0x01},
	{0x30,0xf7},
	{0x31,0x50},
	{0x32,0x00},
	{0x39,0x04},
	{0x3a,0x20},
	{0x3b,0x20},
	{0x3c,0x00},
	{0x3d,0x00},
	{0x3e,0x00},
	{0x3f,0x00},
	{0x50,0x16}, // 0x14
	{0x53,0x80},
	{0x54,0x87},
	{0x55,0x87},
	{0x56,0x80},
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
	{0x62,0x0f}, // 0x08
	{0x64,0x01},  // 0x03
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
	{0x71,0x33},
	{0x72,0xdc},
	{0x73,0x80},
	{0x74,0x02},
	{0x75,0x3f},
	{0x76,0x02},
	{0x77,0x36}, // 0x47
	{0x78,0x88},
	{0x79,0x81},
	{0x7a,0x81},
	{0x7b,0x22},
	{0x7c,0xff},
	{0x93,0x48},
	{0x94,0x00},
	{0x95,0x04},
	{0x96,0xe0},
	{0x97,0x46},
	{0x98,0xf3},
	{0xb1,0x3c},
	{0xb2,0x3c},
	{0xb3,0x44}, //0x40
	{0xb6,0xe0},
	{0xbd,0x3C},
	{0xbe,0x36},
	{0xd0,0xC9},
	{0xd1,0x10},
	{0xd2,0x90},
	{0xd3,0x88},
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
#if 0
	{0x9F,0x0E},
	{0xA0,0x1C},
	{0xA1,0x34},
	{0xA2,0x48},
	{0xA3,0x5A},
	{0xA4,0x6B},
	{0xA5,0x7B},
	{0xA6,0x95},
	{0xA7,0xAB},
	{0xA8,0xBF},
	{0xA9,0xCE},
	{0xAA,0xD9},
	{0xAB,0xE4},
	{0xAC,0xEC},
	{0xAD,0xF7},
	{0xAE,0xFD},
	{0xAF,0xFF},
#elif 0
	{0x9F,0x10},
	{0xA0,0x20},
	{0xA1,0x38},
	{0xA2,0x4e},
	{0xA3,0x63},
	{0xA4,0x76},
	{0xA5,0x87},
	{0xA6,0xa2},
	{0xA7,0xb8},
	{0xA8,0xca},
	{0xA9,0xd8},
	{0xAA,0xe3},
	{0xAB,0xEb},
	{0xAC,0xf0},
	{0xAD,0xF8},
	{0xAE,0xFD},
	{0xAF,0xFF},
#else
	{0x9F,0x14},
	{0xA0,0x28},
	{0xA1,0x44},
	{0xA2,0x5d},
	{0xA3,0x72},
	{0xA4,0x86},
	{0xA5,0x95},
	{0xA6,0xb1},
	{0xA7,0xc6},
	{0xA8,0xd5},
	{0xA9,0xe1},
	{0xAA,0xea},
	{0xAB,0xf1},
	{0xAC,0xf5},
	{0xAD,0xFb},
	{0xAE,0xFe},
	{0xAF,0xFF},
#endif
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
	{0xf2,0x04},
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
	{0xfe,0x00}, 
	{0xfe,0x00}, 
	{0xff,0xff}, 
};



//load GC0308 parameters
void GC0308_write_regs(void)
{
    int i=0;//,j;
    unsigned char buf[2];

    printk("GC0308: camera_client->addr = %x\n", camera_client->addr);

    while(1)
    {
        buf[0] = GC0308_script[i].addr;//(unsigned char)((GC0308_script[i].addr >> 8) & 0xff);
        //buf[1] = (unsigned char)(GC0308_script[i].addr & 0xff);
	    buf[1] = GC0308_script[i].val;
		i++;
	 if (GC0308_script[i].val==0xff&&GC0308_script[i].addr==0xff) 
	 	{
 	    	printk("GC0308_write_regs success in initial GC0308.\n");
	 	break;
	 	}
        if((GC0308_write(buf, 2)) < 0)
        	{
    	    	printk("fail in initial GC0308. \n");
		break;
        	}
    }
   // if(i >= SCRIPT_LEN)
    	//printk("success in initial GC0308.\n");
    //else

    return;

}
static void GC0308_read_regs(void)
{
    u16 i, reg_addr = 0x14 ;//,j;
    unsigned char buf[4];

	buf[0] = reg_addr ;
    //buf[0] = (unsigned char)((reg_addr >> 8) & 0xff);
    //buf[1] = (unsigned char)(reg_addr & 0xff);
    if((GC0308_read(buf, 1)) < 0)
    	printk("GC0308_read_regs111 fail in read GC0308 regs %x. \n", reg_addr);
    else
    {
        printk(" GC0308_read_regs111 GC0308 regs(%x) : %4x, %4x,\n", reg_addr, buf[0] );
    }


while(1)    {
        buf[0] = GC0308_script[i].addr;

        //buf[0] = (unsigned char)((GC0308_script[i].addr >> 8) & 0xff);
        //buf[1] = (unsigned char)(GC0308_script[i].addr & 0xff);
        if((GC0308_read(buf, 1)) < 0)
            break;
        else
           printk(" GC0308_read_regs222 GC0308 regs(%x) : %4x, \n", GC0308_script[i].addr, buf[0]);
			i++;
	if(i>=0xff){    //(GC0308_script[i].addr=0xff){
		printk("GC0308_read_regs222 success in initial GC0308.\n");
		break;
		}
	else {
		printk("GC0308_read_regs222 fail in initial GC0308.\n");
		break;
		}
    }


    return;
}


#endif



void start_camera(void)
{
#if 1
    GC0308_write_regs();

    set_camera_sharpness(GC0308_info.para.sharpness);


#else

    reset_sw_camera();
    set_frame_rate();
    sensor_analog_ctl();
    sensor_sys_ctl();
    set_camera_light_freq();
    set_camera_resolution_timing(GC0308_info.para.resolution);
    set_other_para1();
    set_camera_resolution_others(GC0308_info.para.resolution);
    set_other_para2();
    sensor_sys_ctl_0();
    set_camera_light_mode(GC0308_info.awb_mode);
    set_other_para3();
    set_camera_sharpness(GC0308_info.para.sharpness);
    set_camera_gamma();
    sensor_set_color();
    set_lenc_para();
    set_camera_exposure(GC0308_info.para.exposure);
    wakeup_sw_camera();
    sensor_sys_ctl_1();

//    set_camera_test_mode(GC0308_info.test_mode);
#endif
  //  GC0308_read_regs();
    return;
}



void stop_camera(void)
{

    power_down_sw_camera();
    return;
}

void set_camera_para(struct camera_info_s *para)
{
    printk(KERN_INFO " set camera para. \n ");

    if(GC0308_info.para.resolution != para->resolution)
    {
        memcpy(&GC0308_info.para, para,sizeof(struct camera_info_s) );
        start_camera();
        return;
    }

    if(GC0308_info.para.saturation != para->saturation)
    {
        set_camera_saturation(para->saturation);
        GC0308_info.para.saturation = para->saturation;
    }

    if(GC0308_info.para.brighrness != para->brighrness)
    {
        set_camera_brightness(para->brighrness);
        GC0308_info.para.brighrness = para->brighrness;
    }

    if(GC0308_info.para.contrast != para->contrast)
    {
        set_camera_contrast(para->contrast);
        GC0308_info.para.contrast = para->contrast;
    }

    if(GC0308_info.para.hue != para->hue)
    {
        set_camera_hue(para->hue);
        GC0308_info.para.hue = para->hue;
    }

//  if(GC0308_info.para.special_effect != para->special_effect)
//  {
//      set_camera_special_effect(para->special_effect);
//      GC0308_info.para.special_effect = para->special_effect;
//  }

    if(GC0308_info.para.exposure != para->exposure)
    {
        set_camera_exposure(para->exposure);
        GC0308_info.para.exposure = para->exposure;
    }

    if(GC0308_info.para.sharpness != para->sharpness)
    {
        set_camera_sharpness(para->sharpness);
        GC0308_info.para.sharpness = para->sharpness;
    }

    if(GC0308_info.para.mirro_flip != para->mirro_flip)
    {
        set_camera_mirror_flip(para->mirro_flip);
        GC0308_info.para.mirro_flip = para->mirro_flip;
    }



    return;
}



static int camera_open(struct inode *inode, struct file *file)
{
    camera_dev_t *devp;

    /* Get the per-device structure that contains this cdev */
    devp = container_of(inode->i_cdev, camera_dev_t, cdev);
    file->private_data = devp;

    return 0;
}

static int camera_release(struct inode *inode, struct file *file)
{
//    camera_dev_t *devp = file->private_data;
    file->private_data = NULL;

    return 0;
}
static int camera_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int res = 0;
    void __user *argp = (void __user *)arg;
    struct camera_info_s para;
    switch(cmd)
    {
        case CAMERA_IOC_START:
			
            printk( " start camera engineer. \n ");
            if(copy_from_user(&para, argp, sizeof(struct camera_info_s)))
            {
                printk(KERN_ERR " para is error, use the default value. \n ");
            }
            else
            {
                memcpy(&GC0308_info.para, &para,sizeof(struct camera_info_s) );
            }
            printk("saturation = %d, brighrness = %d, contrast = %d, hue = %d, exposure = %d, sharpness = %d, mirro_flip = %d, resolution = %d . \n",
                para.saturation, para.brighrness, para.contrast, para.hue, para.exposure, para.sharpness, para.mirro_flip, para.resolution);
            start_camera();
            break;

        case CAMERA_IOC_STOP:
            printk(KERN_INFO " stop camera engineer. \n ");
            stop_camera();
            break;

        case CAMERA_IOC_SET_PARA:
            if(copy_from_user(&para, argp, sizeof(struct camera_info_s)))
            {
                pr_err(" no para for camera setting, do nothing. \n ");
            }
            else
            {
                set_camera_para(&para);
            }
            break;

        case CAMERA_IOC_GET_PARA:
            copy_to_user((void __user *)arg, &GC0308_info.para, sizeof(struct camera_info_s));
            break;

        default:
                printk("camera ioctl cmd is invalid. \n");
                break;
    }
    return res;
}



static int GC0308_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	//unsigned char data[16] = {0};
	int res = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		return -1;
	}
	camera_client = client;
//    res = misc_register(&GC0308_device);
    printk( "GC0308: camera_client->addr = %x\n", camera_client->addr);
    msleep(10);
	return res;

}

static int GC0308_i2c_remove(struct i2c_client *client)
{

	return 0;
}


static const struct i2c_device_id GC0308_i2c_id[] = {
	{ GC0308_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver GC0308_driver = {
    .id_table	= GC0308_i2c_id,
	.probe 		= GC0308_i2c_probe,
	.remove 	= GC0308_i2c_remove,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= GC0308_I2C_NAME,
	},
};




static struct file_operations camera_fops = {
    .owner   = THIS_MODULE,
    .open    = camera_open,
    .release = camera_release,
    .ioctl   = camera_ioctl,
};


static int GC0308_probe(struct platform_device *pdev)
{
    int ret;
    struct device *devp;

    printk( "camera: start GC0308_probe. \n");

    camera_devp = kmalloc(sizeof(struct camera_dev_s), GFP_KERNEL);
    if (!camera_devp)
    {
        pr_err( "camera: failed to allocate memory for camera device\n");
        return -ENOMEM;
    }
    msleep(10);

    ret = alloc_chrdev_region(&camera_devno, 0, CAMERA_COUNT, GC0308_DEVICE_NAME);
	if (ret < 0) {
		pr_err("camera: failed to allocate chrdev. \n");
		return 0;
	}

    camera_clsp = class_create(THIS_MODULE, GC0308_DEVICE_NAME);
    if (IS_ERR(camera_clsp))
    {
        unregister_chrdev_region(camera_devno, CAMERA_COUNT);
        return PTR_ERR(camera_clsp);
    }

    /* connect the file operations with cdev */
    cdev_init(&camera_devp->cdev, &camera_fops);
    camera_devp->cdev.owner = THIS_MODULE;
    /* connect the major/minor number to the cdev */
    ret = cdev_add(&camera_devp->cdev, camera_devno, 1);
    if (ret) {
        pr_err( "camera: failed to add device. \n");
        /* @todo do with error */
        return ret;
    }
    /* create /dev nodes */
    devp = device_create(camera_clsp, NULL, MKDEV(MAJOR(camera_devno), 0),
                        NULL, "camera%d", 0);
    if (IS_ERR(devp)) {
        pr_err("camera: failed to create device node\n");
        class_destroy(camera_clsp);
        /* @todo do with error */
        return PTR_ERR(devp);;
    }

    /* We expect this driver to match with the i2c device registered
     * in the board file immediately. */
    ret = i2c_add_driver(&GC0308_driver);
    if (ret < 0 || camera_client == NULL) {
        pr_err("camera: failed to add i2c driver. \n");
        ret = -ENOTSUPP;
    }

    printk( "camera: driver initialized ok\n");
//    reset_hw_camera();      //remGC0308e into ***_board.c
    return ret;
}

static int GC0308_remove(struct platform_device *pdev)
{
//    power_down_hw_camera();     //remGC0308e into ***_board.c
    power_down_sw_camera();
    i2c_del_driver(&GC0308_driver);
    device_destroy(camera_clsp, MKDEV(MAJOR(camera_devno), 0));
    cdev_del(&camera_devp->cdev);
    class_destroy(camera_clsp);
    unregister_chrdev_region(camera_devno, CAMERA_COUNT);
    kfree(camera_devp);

     return 0;
}

static struct platform_driver GC0308_camera_driver = {
	.probe = GC0308_probe,
    .remove = GC0308_remove,
	.driver = {
	.name = GC0308_DEVICE_NAME,
//		.owner = THIS_MODULE,
	},
};


static int __init GC0308_init(void)
{
    int ret = 0;
    printk( "GC0308 driver: init. \n");

	eth_set_pinmux(ETH_BANK0_GPIOC3_C12,ETH_CLK_OUT_GPIOC12_REG3_1, 1);	
	
	#ifdef CONFIG_SN7325
    	configIO(1, 0);
    	setIO_level(1, 0, 0);
    #endif

    ret = platform_driver_register(&GC0308_camera_driver);
    if (ret != 0) {
        printk(KERN_ERR "failed to register GC0308 module, error %d\n", ret);
        return -ENODEV;
    }
    return ret;

//	return i2c_add_driver(&GC0308_driver);
}

static void __exit GC0308_exit(void)
{
	pr_info("GC0308 driver: exit\n");
    platform_driver_unregister(&GC0308_camera_driver);
}

module_init(GC0308_init);
module_exit(GC0308_exit);

MODULE_AUTHOR("frank.chen@amlogic.com>");
MODULE_DESCRIPTION("camera device driver GC0308");
MODULE_LICENSE("GPL");

