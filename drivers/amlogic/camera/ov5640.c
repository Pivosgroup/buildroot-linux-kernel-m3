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
#include "ov_camera.h"


/* specify your i2c device address according the device datasheet */
//#define I2C_CAMERA  0x60  /* @todo to be changed according your device */

#define OV5640_DEVICE_NAME      "camera_ov5640"
#define OV5640_I2C_NAME         "ov5640_i2c"


#define CAMERA_COUNT              1

static dev_t camera_devno;
static struct class *camera_clsp = NULL;

static camera_dev_t *camera_devp = NULL;

static struct i2c_client *camera_client = NULL;

static struct ov_ctl_s ov5640_info = {
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

static int  ov5640_read(char *buf, int len)
{
    int  i2c_flag = -1;
	struct i2c_msg msgs[] = {
		{
			.addr	= camera_client->addr,
			.flags	= 0,
			.len	= 2,
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

static int  ov5640_write(char *buf, int len)
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
//			pr_info("error in writing ov5640 register(%x). \n", off_addr);
			return -1;
		}
		else
			return 0;
}

static int  ov5640_write_byte(unsigned short addr, unsigned char data)
{
    unsigned char buff[4];
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    buff[2] = data;
    if((ov5640_write(buff, 3)) < 0)
        return -1;
    else
        return 0;
}

static int  ov5640_write_word(unsigned short addr, unsigned short data)
{
    unsigned char buff[4];
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    buff[2] = (unsigned char)(data & 0xff);
    buff[3] = (unsigned char)((data >> 8) & 0xff);
    if((ov5640_write(buff, 4)) < 0)
        return -1;
    else
        return 0;

}


static unsigned char  ov5640_read_byte(unsigned short addr )
{
    unsigned char buff[4];
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    if((ov5640_read(buff, 1)) < 0)
        return -1;
    else
    {
        return buff[0];
    }
}

static unsigned short ov5640_read_word(unsigned short addr)
{
    unsigned short data;
    unsigned char buff[2];
    buff[0] = (unsigned char)((addr >> 8) & 0xff);
    buff[1] = (unsigned char)(addr & 0xff);
    if((ov5640_read(buff, 2)) < 0)
        return -1;
    else
    {
        data = buff[1];
        data = (data << 8) | buff[0];
        return data;
    }
}

static void set_camera_light_mode(enum camera_light_mode_e mode)
{
    int i ;
    struct ov_i2c_fig0_s ov5640_data[16] = {
        {0x5180, 0xf2ff},
        {0x5182, 0x1400},
        {0x5184, 0x2425},
        {0x5186, 0x0909},
        {0x5188, 0x7509},
        {0x518a, 0xe054},
        {0x518c, 0x42b2},
        {0x518e, 0x563d},
        {0x5190, 0xf846},
        {0x5192, 0x7004},
        {0x5194, 0xf0f0},
        {0x5196, 0x0103},
        {0x5198, 0x1204},
        {0x519a, 0x0004},
        {0x519c, 0x8206},
        {0x518e, 0x563d},
    };

    switch(mode)
    {
        case SIMPLE_AWB:
            ov5640_write_byte(0x3406, 0);
            ov5640_write_byte(0x5183, 0x94);   //
            ov5640_write_byte(0x5191, 0xff);  //AWB control:[15:8]bottom limit, [7:0} top limit
            ov5640_write_byte(0x5192, 0x00);
            break;

        case MANUAL_DAY:
            ov5640_write_byte(0x3406, 1);
            ov5640_write_byte(0x3400, 6);
            ov5640_write_byte(0x3401, 0x1c);
            ov5640_write_byte(0x3402, 4);
            ov5640_write_byte(0x3403, 0);
            ov5640_write_byte(0x3404, 4);
            ov5640_write_byte(0x3405, 0xf3);
            break;

        case MANUAL_A:
            ov5640_write_byte(0x3406, 1);
            ov5640_write_byte(0x3400, 4);
            ov5640_write_byte(0x3401, 0x10);
            ov5640_write_byte(0x3402, 4);
            ov5640_write_byte(0x3403, 0);
            ov5640_write_byte(0x3404, 8);
            ov5640_write_byte(0x3405, 0xb6);
            break;

        case MANUAL_CWF:
            ov5640_write_byte(0x3406, 1);
            ov5640_write_byte(0x3400, 5);
            ov5640_write_byte(0x3401, 0x48);
            ov5640_write_byte(0x3402, 4);
            ov5640_write_byte(0x3403, 0);
            ov5640_write_byte(0x3404, 7);
            ov5640_write_byte(0x3405, 0xcf);
            break;

         case MANUAL_CLOUDY:
             ov5640_write_byte(0x3406, 1);
             ov5640_write_byte(0x3400, 6);
             ov5640_write_byte(0x3401, 0x48);
             ov5640_write_byte(0x3402, 4);
             ov5640_write_byte(0x3403, 0);
             ov5640_write_byte(0x3404, 4);
             ov5640_write_byte(0x3405, 0xd3);
            break;

        case ADVANCED_AWB:
        default:
            ov5640_write_byte(0x3406, 0);
            for(i = 0; i < 16; i++)
            {
              ov5640_write_word(ov5640_data[i].addr, ov5640_data[i].val);
            }
            break;

    }
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
    ov5640_write_byte(0x5583, saturation_u);
    ov5640_write_byte(0x5584, saturation_v);

 }


static void set_camera_brightness(enum camera_brightness_e brighrness)
{
    unsigned char y_bright = 0;
    switch(brighrness)
    {
        case BRIGHTNESS_N4_STEP:
            y_bright = 0x00;
            break;

        case BRIGHTNESS_N3_STEP:
            y_bright = 0x08;
            break;

        case BRIGHTNESS_N2_STEP:
            y_bright = 0x10;
            break;

        case BRIGHTNESS_N1_STEP:
            y_bright = 0x18;
            break;

        case BRIGHTNESS_P1_STEP:
            y_bright = 0x28;
            break;

        case BRIGHTNESS_P2_STEP:
            y_bright = 0x30;
            break;

        case BRIGHTNESS_P3_STEP:
            y_bright = 0x38;
            break;

        case BRIGHTNESS_P4_STEP:
            y_bright = 0x40;
            break;

        case BRIGHTNESS_0_STEP:
        default:
            y_bright = 0x20;
            break;
    }
    ov5640_write_byte(0x5589, y_bright);   //
 }

static void set_camera_contrast(enum camera_contrast_e contrast)
{
    unsigned char u_data, v_data;
    switch(contrast)
    {
        case BRIGHTNESS_N4_STEP:
            u_data = 0x10;
            v_data = 0x10;
            break;

        case BRIGHTNESS_N3_STEP:
            u_data = 0x14;
            v_data = 0x14;
            break;

        case BRIGHTNESS_N2_STEP:
            u_data = 0x18;
            v_data = 0x18;
            break;

        case BRIGHTNESS_N1_STEP:
            u_data = 0x1c;
            v_data = 0x1c;
            break;

        case BRIGHTNESS_P1_STEP:
            u_data = 0x24;
            v_data = 0x24;
            break;

        case BRIGHTNESS_P2_STEP:
            u_data = 0x28;
            v_data = 0x28;
            break;

        case BRIGHTNESS_P3_STEP:
            u_data = 0x2c;
            v_data = 0x2c;
            break;

        case BRIGHTNESS_P4_STEP:
            u_data = 0x30;
            v_data = 0x30;
            break;

        case BRIGHTNESS_0_STEP:
        default:
            u_data = 0x20;
            v_data = 0x20;
            break;
    }
    ov5640_write_byte(0x5585, 0x80);   //
    ov5640_write_byte(0x5586, 0x80);   //
}

static void set_camera_hue(enum camera_hue_e hue)
{
    unsigned char cos_data, sin_data, ctl_data;
    switch(hue)
    {
        case HUE_N180_DEGREE:
            cos_data = 0x80;
            sin_data = 0x00;
            ctl_data = 0x72;
            break;

        case HUE_N150_DEGREE:
            cos_data = 0x6f;
            sin_data = 0x40;
            ctl_data = 0x72;
            break;

        case HUE_N120_DEGREE:
            cos_data = 0x40;
            sin_data = 0x6f;
            ctl_data = 0x72;
            break;

        case HUE_N90_DEGREE:
            cos_data = 0x00;
            sin_data = 0x80;
            ctl_data = 0x42;
            break;

        case HUE_N60_DEGREE:
            cos_data = 0x40;
            sin_data = 0x6f;
            ctl_data = 0x42;
            break;

        case HUE_N30_DEGREE:
            cos_data = 0x6f;
            sin_data = 0x40;
            ctl_data = 0x42;
            break;

        case HUE_P30_DEGREE:
            cos_data = 0x6f;
            sin_data = 0x40;
            ctl_data = 0x41;

            break;

        case HUE_P60_DEGREE:
            cos_data = 0x40;
            sin_data = 0x6f;
            ctl_data = 0x41;

            break;

        case HUE_P90_DEGREE:
            cos_data = 0x00;
            sin_data = 0x80;
            ctl_data = 0x71;

            break;

        case HUE_P120_DEGREE:
            cos_data = 0x40;
            sin_data = 0x00;
            ctl_data = 0x71;

            break;

        case HUE_P150_DEGREE:
            cos_data = 0x6f;
            sin_data = 0x40;
            ctl_data = 0x71;
            break;

        case HUE_0_DEGREE:
        default:
            cos_data = 0x80;
            sin_data = 0x00;
            ctl_data = 0x40;
            break;
    }
    ov5640_write_byte(0x5581, cos_data);   //
    ov5640_write_byte(0x5582, sin_data);   //
    ov5640_write_byte(0x5588, ctl_data);   //
 }

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

static void set_camera_exposure(enum camera_exposure_e exposure)
{
    unsigned char reg3a0f, reg3a10, reg3a1b, reg3a1e, reg3a11, reg3a1f;
    switch(exposure)
    {
        case EXPOSURE_N4_STEP:
            reg3a0f = 0x10;
            reg3a10 = 0x08;
            reg3a1b = 0x10;
            reg3a1e = 0x08;
            reg3a11 = 0x20;
            reg3a1f = 0x10;
            break;

        case EXPOSURE_N3_STEP:
            reg3a0f = 0x18;
            reg3a10 = 0x00;
            reg3a1b = 0x18;
            reg3a1e = 0x10;
            reg3a11 = 0x30;
            reg3a1f = 0x10;
            break;

        case EXPOSURE_N2_STEP:
            reg3a0f = 0x20;
            reg3a10 = 0x18;
            reg3a11 = 0x41;
            reg3a1b = 0x20;
            reg3a1e = 0x18;
            reg3a1f = 0x10;
            break;

        case EXPOSURE_N1_STEP:
            reg3a0f = 0x28;
            reg3a10 = 0x20;
            reg3a11 = 0x51;
            reg3a1b = 0x28;
            reg3a1e = 0x20;
            reg3a1f = 0x10;
            break;

        case EXPOSURE_P1_STEP:
            reg3a0f = 0x48;
            reg3a10 = 0x40;
            reg3a11 = 0x80;
            reg3a1b = 0x48;
            reg3a1e = 0x40;
            reg3a1f = 0x20;
            break;

        case EXPOSURE_P2_STEP:
            reg3a0f = 0x50;
            reg3a10 = 0x48;
            reg3a11 = 0x90;
            reg3a1b = 0x50;
            reg3a1e = 0x48;
            reg3a1f = 0x20;
            break;

        case EXPOSURE_P3_STEP:
            reg3a0f = 0x58;
            reg3a10 = 0x50;
            reg3a11 = 0x91;
            reg3a1b = 0x58;
            reg3a1e = 0x50;
            reg3a1f = 0x10;
            break;

        case EXPOSURE_P4_STEP:
            reg3a0f = 0x60;
            reg3a10 = 0x58;
            reg3a11 = 0xa0;
            reg3a1b = 0x60;
            reg3a1e = 0x58;
            reg3a1f = 0x20;
            break;

        case EXPOSURE_0_STEP:
        default:
            reg3a0f = 0x30;
            reg3a10 = 0x28;
            reg3a1b = 0x30;
            reg3a1e = 0x26;
            reg3a11 = 0x60;
            reg3a1f = 0x14;
            break;
    }

    ov5640_write_byte(0x3a0f, reg3a0f);
    ov5640_write_byte(0x3a10, reg3a10);
    ov5640_write_byte(0x3a1b, reg3a1b);
    ov5640_write_byte(0x3a1e, reg3a1e);
    ov5640_write_byte(0x3a11, reg3a11);
    ov5640_write_byte(0x3a1f, reg3a1f);
}

static void set_camera_sharpness(enum camera_sharpness_e sharpness)
{
    int i;
    struct ov_i2c_fig0_s ov5640_data[6] = {
        {0x5300, 0x3008},  //color interpolation min gain
        {0x5302, 0x0010},   //color interpolation max gain
        {0x5304, 0x3008},   //color interpolation min intnoise
        {0x5306, 0x1608},     //color interpolation max intnoise
        {0x5309, 0x3008},   //disable sharpen manual
        {0x530b, 0x0604},
    };

    switch(sharpness)
    {
        case SHARPNESS_1_STEP:
            ov5640_write_byte(0x5308 ,0x65);
            ov5640_write_byte(0x5302 ,0x02);
            break;

        case SHARPNESS_2_STEP:
            ov5640_write_byte(0x5308 ,0x65);
            ov5640_write_byte(0x5302 ,0x04);
            break;

        case SHARPNESS_3_STEP:
            ov5640_write_byte(0x5308 ,0x65);
            ov5640_write_byte(0x5302 ,0x08);
            break;

        case SHARPNESS_4_STEP:
            ov5640_write_byte(0x5308 ,0x65);
            ov5640_write_byte(0x5302 ,0x0c);
            break;

        case SHARPNESS_5_STEP:
            ov5640_write_byte(0x5308 ,0x65);
            ov5640_write_byte(0x5302 ,0x10);
            break;

        case SHARPNESS_6_STEP:
            ov5640_write_byte(0x5308 ,0x65);
            ov5640_write_byte(0x5302 ,0x14);
            break;

        case SHARPNESS_7_STEP:
            ov5640_write_byte(0x5308 ,0x65);
            ov5640_write_byte(0x5302 ,0x18);
            break;

        case SHARPNESS_8_STEP:
            ov5640_write_byte(0x5308 ,0x65);
            ov5640_write_byte(0x5302 ,0x20);
            break;

        case SHARPNESS_AUTO_STEP:
        default:
            ov5640_write_byte(0x5308, 0x25);
            for(i = 0; i < 6; i++)
            {
              ov5640_write_word(ov5640_data[i].addr, ov5640_data[i].val);
            }
            break;
    }

}

static void set_camera_mirror_flip(enum camera_mirror_flip_e mirror_flip)
{
    unsigned char mirror, flip;
    mirror = (ov5640_read_byte(0x3821)) & 0xf9;
    flip = (ov5640_read_byte(0x3820)) & 0xf9;

    switch(mirror_flip)
    {
        case MF_MIRROR:
            mirror |= 0x06;
            break;

        case MF_FLIP:
            flip |= 0x06;
            break;

        case MF_MIRROR_FLIP:
            mirror |= 0x06;
            flip |= 0x06;
            break;

        case MF_NORMAL:
        default:
            flip = 0x41;
            mirror = 0x27;
            break;
    }
    ov5640_write_byte(0x3820, flip);
    ov5640_write_byte(0x3821, mirror);
}

static void set_camera_test_mode(enum camera_test_mode_e test_mode)
{

    switch(test_mode)
    {
        case TEST_1:
            ov5640_write_byte(0x503d, 0x80);
            ov5640_write_byte(0x4741, 0x1);
            break;

        case TEST_2:
            ov5640_write_byte(0x503d, 0x80);
            ov5640_write_byte(0x4741, 0x3);
            break;

        case TEST_3:
            ov5640_write_byte(0x503d, 0x82);
            ov5640_write_byte(0x4741, 0x1);
            break;

        case TEST_OFF:
        default:
            ov5640_write_byte(0x503d, 0x00);
            ov5640_write_byte(0x4741, 0x1);
            break;
    }
}

static void set_camera_resolution_timing(enum tvin_sig_fmt_e resolution)
{
    unsigned char reg3618, reg3612, i;
    struct ov_i2c_fig0_s ov5640_data[11] = {  //default value for 720p output
        {0x3814 , 0x3131},
        {0x3800 , 0x0000},  //HREF horizontal start point
        {0x3802 , 0xfa00},  //HREF vertical start point
        {0x3804 , 0x3f0a},  //HREF horizontal width(scale input)
        {0x3806 , 0xa906},  //HREF horizontal height(scale input)
        {0x3808 , 0x0005},  //DVP output horizontal width
        {0x380a , 0xd002},  //DVP output vertical height
        {0x380c , 0x6407},     //DVP output horizontal total ,
        {0x380e , 0xe402},    //DVP output vertical total
        {0x3810 , 0x1000},    //horizotal & vertical offset setting, vertical thumbnai output size
        {0x3812 , 0x0400},  //horizotal thumbnai output size
    };

    switch(resolution)
    {
        case TVIN_SIG_FMT_CAMERA_1920X1080P_30Hz:
            //modify ov5640_data[].value for 1080p resolution

            reg3618 = 0x04;
            reg3612 = 0x2b;

            break;

        case TVIN_SIG_FMT_CAMERA_1280X720P_30Hz:
            reg3618 = 0x00;
            reg3612 = 0x29;
            break;

//        case TVIN_SIG_FMT_CAMERA_1024X768P_30Hz:

//            break;

//        case TVIN_SIG_FMT_CAMERA_800X600P_30Hz:

//            break;

        case TVIN_SIG_FMT_CAMERA_640X480P_30Hz:

            break;

        default:
            printk(KERN_INFO "camera set invalid resolution. \n");
            return;
    }
    for(i = 0; i < 11; i++)
    {
        ov5640_write_word(ov5640_data[i].addr, ov5640_data[i].val);
    }
    ov5640_write_byte(0x3618, reg3618);
    ov5640_write_byte(0x3612, reg3612);

}

static void set_camera_resolution_others(enum tvin_sig_fmt_e resolution)
{
    unsigned int i;
    struct ov_i2c_fig0_s ov5640_data[4] = {  //default value for 720p output
        {0x3a02, 0xe402},
        {0x3a08, 0xbc01},  //50hz band width
        {0x3a0a, 0x7201},  //60hz band width
        {0x3a0d, 0x0102}, //50hz maz band in one frame, 60hz maz band in one frame
    };

    switch(resolution)
    {
        case TVIN_SIG_FMT_CAMERA_1920X1080P_30Hz:
             //modify ov5640_data[].value for 1080p resolution

            break;

        case TVIN_SIG_FMT_CAMERA_1280X720P_30Hz:

            break;

//        case TVIN_SIG_FMT_CAMERA_1024X768P_30Hz:

//            break;

//        case TVIN_SIG_FMT_CAMERA_800X600P_30Hz:

//            break;

        case TVIN_SIG_FMT_CAMERA_640X480P_30Hz:

            break;

        default:
            printk(KERN_INFO "camera set invalid resolution . \n");
            return;
    }
    for(i = 0; i < 4; i++)
    {
        ov5640_write_word(ov5640_data[i].addr, ov5640_data[i].val);
    }
}

static void reset_sw_camera(void)
{
    unsigned int i;
    struct ov_i2c_fig_s ov5640_data[6] = {
        {0x3103, 0x11},
        {0x3008, 0x82},
        {0x3008, 0x42},
        {0x3103, 0x03},
        {0x3017, 0xff}, //mux FREX/VSYNC/HREF/PCLK/D0~D9/GPIO0/GPIO1 to output mode
        {0x3018, 0xff},
    };
    for(i = 0; i < 6; i++)
    {
        ov5640_write_byte(ov5640_data[i].addr, ov5640_data[i].val);
    }
}

static void power_down_sw_camera(void)
{
    ov5640_write_byte(0x3008, 0x42);
}

static void wakeup_sw_camera(void)
{
    unsigned int i;
    struct ov_i2c_fig_s ov5640_data[4] = {
        {0x3008, 0x02}, //normal work mode
        //{0x3035, 0x31},    //set the pixel clock output  --  51hz
        {0x3035, 0x21},    //set the pixel clock output  --  80hz
        {0x3002, 0x1c},
        {0x3006, 0xc3},   //enable some modules
    };
    for(i = 0; i < 4; i++)
    {
        ov5640_write_byte(ov5640_data[i].addr, ov5640_data[i].val);
    }
}

static void set_frame_rate(void)
{
    ov5640_write_word(0x3034, 0x111a);  //set PCLK output frequency(168Mhz)
    ov5640_write_word(0x3036, 0x1369);
    ov5640_write_byte(0x3108, 0x01);

}

static void sensor_analog_ctl(void)
{
    unsigned int i;
    struct ov_i2c_fig_s ov5640_data[16] = {
        {0x3630, 0x2e},
        {0x3632, 0xe2},
        {0x3633, 0x23},
        {0x3621, 0xe0},
        {0x3704, 0xa0},
        {0x3703, 0x5a},
        {0x3715, 0x78},
        {0x3717, 0x01},
        {0x370b, 0x60},
        {0x3705, 0x1a},
        {0x3905, 0x02},
        {0x3906, 0x10},
        {0x3901, 0x0a},
        {0x3731, 0x12},
        {0x3600, 0x08},
        {0x3601, 0x33},

    };
    for(i = 0; i < 16; i++)
    {
        ov5640_write_byte(ov5640_data[i].addr, ov5640_data[i].val);
    }
}
static void sensor_sys_ctl(void)
{
    unsigned int i;
    struct ov_i2c_fig_s ov5640_data[9] = {
        {0x302d, 0x60},
        {0x3620, 0x52},
        {0x371b, 0x20},
        {0x471c, 0x50},    //compression mode 2
        {0x3a18, 0x00},
        {0x3a19, 0xf8},
        {0x3635, 0x1c},
        {0x3634, 0x40},    //auto light frequency detection
        {0x3622, 0x01},
    };
    for(i = 0; i < 9; i++)
    {
        ov5640_write_byte(ov5640_data[i].addr, ov5640_data[i].val);
    }

    set_camera_mirror_flip(ov5640_info.para.mirro_flip);
}

static void sensor_sys_ctl_0(void)
{
    unsigned int i;
    struct ov_i2c_fig_s ov5640_data[11] = {
        {0x300e, 0x58},
        {0x302e, 0x00},
        {0x4300, 0x30},
        {0x501f, 0x00},    //ISPYUV
        {0x4713, 0x02},
        {0x4407, 0x04}, //compress quality
        {0x460b, 0x37},
        {0x460c, 0x22},
        {0x3824, 0x04},
        {0x5000, 0xa7},     //bit[7]: LENC correction enable,
                            //bit[6]: gamma(in YUV domain) enable,
                            //bit[5]: RAW gamma enable,
                            //bit[4]: even odd remove enable,
                            //bit[3]: de-noise  enable,
                            //bit[2]: black pixel cancellation enable,
                            //bit[1]: white pixel cancellation enable,
                            //bit[0]: color interpolation enable,
        {0x5001, 0x83}, //0xff);   //bit[7]: special digital enable,
                                    //bit[6]: uv adjust enable,
                                    //bit[5]: vertical scale enable,
                                    //bit[4]: horzontal scale enable,
                                    //bit[3]: line stretch  enable,
                                    //bit[2]: uv arverage  enable,
                                    //bit[1]: color matrix  enable,
                                    //bit[0]: auto white balance enable,
    };
    for(i = 0; i < 11; i++)
    {
        ov5640_write_byte(ov5640_data[i].addr, ov5640_data[i].val);
    }
}
static void sensor_set_color(void)
{
    unsigned int i;
    struct ov_i2c_fig_s ov5640_data[5] = {
//        (0x460b, 0x35);
        {0x5580, 0x02}, //0x9f);  //enable some special functions
                                    //bit[7]: fixd y enable -- work with register 0x5587
                                    //bit[6]: negative y enable
                                    //bit[5]: gray image enable
                                    //bit[4]: fixd v enable-- work with register 0x5586
                                    //bit[3]: fixd u enable-- work with register 0x5585
                                    //bit[2]: contrast enable --work with register 0x5587, 0x5588, 0x5589
                                    //bit[1]: saturation enable -- work with register 0x5583, 0x5584
                                    //bit[0]: hue enable
        {0x5588, 0x40},   //enable VU adjust manual
        {0x5589, 0x10},   //enable some special functions
        {0x558a, 0x00},     //hue control bits
        {0x558b, 0xf8},

    };
    for(i = 0; i < 5; i++)
    {
        ov5640_write_byte(ov5640_data[i].addr, ov5640_data[i].val);
    }

    set_camera_saturation(ov5640_info.para.saturation);
    set_camera_brightness(ov5640_info.para.brighrness);
    set_camera_contrast(ov5640_info.para.contrast);
    set_camera_hue(ov5640_info.para.hue);

}


static void set_camera_light_freq(void)
{
        unsigned int i;
        struct ov_i2c_fig0_s ov5640_data[4] = {  //default value for 720p output
            {0x3c04, 0x9828},
            {0x3c06, 0x0700},
            {0x3c08, 0x1c00},
            {0x3c0a, 0x409c},
        };
    ov5640_write_byte(0x3c01, 0x34); //bit[7] == 0:  enable light frequency auto detection
    for(i = 0; i < 4; i++)
    {
        ov5640_write_word(ov5640_data[i].addr, ov5640_data[i].val);
    }
}

static void set_other_para1(void)
{
    ov5640_write_word(0x3708, 0x5262);
    ov5640_write_byte(0x370c, 0x03);

}

static void set_other_para2(void)
{
    unsigned int i;
    struct ov_i2c_fig_s ov5640_data[8] = {
        {0x3a14, 0x02},     //50hz  maximum exposure output limit
        {0x3a15, 0xe4},
        {0x4001, 0x02},     //BLC ctl
        {0x4004, 0x02},
        {0x3000, 0x00},    //some function -- normal mode
        {0x3002, 0x00},
        {0x3004, 0xff},    //enable some modules
        {0x3006, 0xff},    //enable some modules
    };
    for(i = 0; i < 8; i++)
    {
        ov5640_write_byte(ov5640_data[i].addr, ov5640_data[i].val);
    }
}

static void set_other_para3(void)
{
    unsigned int i;
    struct ov_i2c_fig0_s ov5640_data[5] = {
        {0x5381, 0x5a1c},
        {0x5383, 0x0a06},
        {0x5385, 0x887e},
        {0x5387, 0x6c7c},
        {0x5389, 0x0110},
    };
    for(i = 0; i < 5; i++)
    {
        ov5640_write_word(ov5640_data[i].addr, ov5640_data[i].val);
    }
    ov5640_write_byte(0x538b, 0x98);
}


static void sensor_sys_ctl_1(void)
{
    unsigned int i;
    struct ov_i2c_fig_s ov5640_data[9] = {
        {0x5025, 0x00},
        {0x3821, 0x07},
        {0x4300, 0x31},
        {0x501f, 0x00},
        {0x460c, 0x20},
        {0x3824, 0x04},
        {0x460b, 0x37},
        {0x4740, 0x21}, //set pixel clock, vsync hsync phase
        {0x4300, 0x31},	//set yuv order
    };
    for(i = 0; i < 9; i++)
    {
        ov5640_write_byte(ov5640_data[i].addr, ov5640_data[i].val);
    }
}

static void set_camera_gamma(void)
{
    unsigned int i;
    struct ov_i2c_fig0_s ov5640_data[8] = {
        {0x5480, 0x0801},
        {0x5482, 0x2814},
        {0x5484, 0x6551},
        {0x5486, 0x7d71},
        {0x5488, 0x9187},
        {0x548a, 0xaa9a},
        {0x548c, 0xcdb8},
        {0x548e, 0xeadd},
    };
    for(i = 0; i < 8; i++)
    {
        ov5640_write_word(ov5640_data[i].addr, ov5640_data[i].val);
    }

    ov5640_write_byte(0x5490, 0x1d);

}

static void set_lenc_para(void)
{
    unsigned int i;
    struct ov_i2c_fig0_s ov5640_data[30] = {
        {0x5800, 0x1523},
        {0x5802, 0x1010},
        {0x5804, 0x2315},
        {0x5806, 0x080c},
        {0x5808, 0x0505},
        {0x580a, 0x0c08},
        {0x580c, 0x0307},
        {0x580e, 0x0000},
        {0x5810, 0x0703},
        {0x5812, 0x0307},
        {0x5814, 0x0000},
        {0x5816, 0x0703},
        {0x5818, 0x080b},
        {0x581a, 0x0505},
        {0x581c, 0x0b07},
        {0x581e, 0x162a},
        {0x5820, 0x1111},
        {0x5822, 0x2915},
        {0x5824, 0xafbf},
        {0x5826, 0xaf9f},
        {0x5828, 0x6fdf},
        {0x582a, 0xab8e},
        {0x582c, 0x7f9e},
        {0x582e, 0x894f},
        {0x5830, 0x9886},
        {0x5832, 0x4f6f},
        {0x5834, 0x7b6e},
        {0x5836, 0x6f7e},
        {0x5838, 0xbfde},
        {0x583a, 0xbf9f},
    };

    for(i = 0; i < 30; i++)
    {
        ov5640_write_word(ov5640_data[i].addr, ov5640_data[i].val);
    }
    ov5640_write_byte(0x583c, 0xec);

}


#if 1

#define SCRIPT_LEN          258




struct ov_i2c_fig_s ov5640_script[] = {
     {0x3103, 0x11},
    {0x3008, 0x82},
    {0x3008, 0x42},
    {0x3103, 0x03},
    {0x3017, 0xff},
    {0x3018, 0xff},
    {0x3034, 0x1a},
    {0x3035, 0x11},
    {0x3036, 0x69},
    {0x3037, 0x13},
    {0x3108, 0x01},
    {0x3630, 0x2e},
    {0x3632, 0xe2},
    {0x3633, 0x23},
    {0x3621, 0xe0},
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    {0x3905, 0x02},
    {0x3906, 0x10},
    {0x3901, 0x0a},
    {0x3731, 0x12},
    {0x3600, 0x08},
    {0x3601, 0x33},
    {0x302d, 0x60},
    {0x3620, 0x52},
    {0x371b, 0x20},
    {0x471c, 0x50},
    {0x3a18, 0x00},
    {0x3a19, 0xf8},
    {0x3635, 0x1c},
    {0x3634, 0x40},
    {0x3622, 0x01},
    {0x3c01, 0x34},
    {0x3c04, 0x28},
    {0x3c05, 0x98},
    {0x3c06, 0x00},
    {0x3c07, 0x07},
    {0x3c08, 0x00},
    {0x3c09, 0x1c},
    {0x3c0a, 0x9c},
    {0x3c0b, 0x40},
    {0x3820, 0x41},
    {0x3821, 0x27},
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0xfa},
    {0x3804, 0x0a},
    {0x3805, 0x3f},
    {0x3806, 0x06},
    {0x3807, 0xa9},
    {0x3808, 0x05},
    {0x3809, 0x00},
    {0x380a, 0x02},
    {0x380b, 0xd0},
            //for a fixed resolution, the tatal line & pixel are fixed.
    {0x380c, 0x07},     //total horizontal,
    {0x380d, 0x64},
    {0x380e, 0x02},     //total vertical
    {0x380f, 0xe4},
    {0x3810, 0x00},
    {0x3811, 0x10},
    {0x3812, 0x00},
    {0x3813, 0x04},
    {0x3618, 0x00},
    {0x3612, 0x29},
    {0x3708, 0x62},
    {0x3709, 0x52},
    {0x370c, 0x03},
    {0x3a02, 0x02},
    {0x3a03, 0xe4},
    {0x3a08, 0x01},
    {0x3a09, 0xbc},
    {0x3a0a, 0x01},
    {0x3a0b, 0x72},
    {0x3a0e, 0x01},
    {0x3a0d, 0x02},
    {0x3a14, 0x02},
    {0x3a15, 0xe4},
    {0x4001, 0x02},
    {0x4004, 0x02},
    {0x3000, 0x00},
    {0x3002, 0x00},
    {0x3004, 0xff},
    {0x3006, 0xff},
    {0x300e, 0x58},
    {0x302e, 0x00},
    {0x4300, 0x30},
    {0x501f, 0x00},
    {0x4713, 0x02},
    {0x4407, 0x04},
    {0x460b, 0x35},
    {0x460c, 0x22},
    {0x3824, 0x04},
    {0x5000, 0xa7},
    {0x5001, 0x83},
    {0x5180, 0xff},
    {0x5181, 0xf2},
    {0x5182, 0x00},
    {0x5183, 0x14},
    {0x5184, 0x25},
    {0x5185, 0x24},
    {0x5186, 0x09},
    {0x5187, 0x09},
    {0x5188, 0x09},
    {0x5189, 0x75},
    {0x518a, 0x54},
    {0x518b, 0xe0},
    {0x518c, 0xb2},
    {0x518d, 0x42},
    {0x518e, 0x3d},
    {0x518f, 0x56},
    {0x5190, 0x46},
    {0x5191, 0xf8},
    {0x5192, 0x04},
    {0x5193, 0x70},
    {0x5194, 0xf0},
    {0x5195, 0xf0},
    {0x5196, 0x03},
    {0x5197, 0x01},
    {0x5198, 0x04},
    {0x5199, 0x12},
    {0x519a, 0x04},
    {0x519b, 0x00},
    {0x519c, 0x06},
    {0x519d, 0x82},
    {0x519e, 0x38},
    {0x5381, 0x1c},
    {0x5382, 0x5a},
    {0x5383, 0x06},
    {0x5384, 0x0a},
    {0x5385, 0x7e},
    {0x5386, 0x88},
    {0x5387, 0x7c},
    {0x5388, 0x6c},
    {0x5389, 0x10},
    {0x538a, 0x01},
    {0x538b, 0x98},
    {0x5300, 0x08},
    {0x5301, 0x30},
    {0x5302, 0x10},
    {0x5303, 0x00},
    {0x5304, 0x08},
    {0x5305, 0x30},
    {0x5306, 0x08},
    {0x5307, 0x16},
    {0x5309, 0x08},
    {0x530a, 0x30},
    {0x530b, 0x04},
    {0x530c, 0x06},
    {0x5480, 0x01},
    {0x5481, 0x08},
    {0x5482, 0x14},
    {0x5483, 0x28},
    {0x5484, 0x51},
    {0x5485, 0x65},
    {0x5486, 0x71},
    {0x5487, 0x7d},
    {0x5488, 0x87},
    {0x5489, 0x91},
    {0x548a, 0x9a},
    {0x548b, 0xaa},
    {0x548c, 0xb8},
    {0x548d, 0xcd},
    {0x548e, 0xdd},
    {0x548f, 0xea},
    {0x5490, 0x1d},
    {0x5580, 0x02},
    {0x5583, 0x40},
    {0x5584, 0x10},
    {0x5589, 0x10},
    {0x558a, 0x00},
    {0x558b, 0xf8},
    {0x5800, 0x23},
    {0x5801, 0x15},
    {0x5802, 0x10},
    {0x5803, 0x10},
    {0x5804, 0x15},
    {0x5805, 0x23},
    {0x5806, 0x0c},
    {0x5807, 0x08},
    {0x5808, 0x05},
    {0x5809, 0x05},
    {0x580a, 0x08},
    {0x580b, 0x0c},
    {0x580c, 0x07},
    {0x580d, 0x03},
    {0x580e, 0x00},
    {0x580f, 0x00},
    {0x5810, 0x03},
    {0x5811, 0x07},
    {0x5812, 0x07},
    {0x5813, 0x03},
    {0x5814, 0x00},
    {0x5815, 0x00},
    {0x5816, 0x03},
    {0x5817, 0x07},
    {0x5818, 0x0b},
    {0x5819, 0x08},
    {0x581a, 0x05},
    {0x581b, 0x05},
    {0x581c, 0x07},
    {0x581d, 0x0b},
    {0x581e, 0x2a},
    {0x581f, 0x16},
    {0x5820, 0x11},
    {0x5821, 0x11},
    {0x5822, 0x15},
    {0x5823, 0x29},
    {0x5824, 0xbf},
    {0x5825, 0xaf},
    {0x5826, 0x9f},
    {0x5827, 0xaf},
    {0x5828, 0xdf},
    {0x5829, 0x6f},
    {0x582a, 0x8e},
    {0x582b, 0xab},
    {0x582c, 0x9e},
    {0x582d, 0x7f},
    {0x582e, 0x4f},
    {0x582f, 0x89},
    {0x5830, 0x86},
    {0x5831, 0x98},
    {0x5832, 0x6f},
    {0x5833, 0x4f},
    {0x5834, 0x6e},
    {0x5835, 0x7b},
    {0x5836, 0x7e},
    {0x5837, 0x6f},
    {0x5838, 0xde},
    {0x5839, 0xbf},
    {0x583a, 0x9f},
    {0x583b, 0xbf},
    {0x583c, 0xec},
    {0x5025, 0x00},
    {0x3a0f, 0x30},
    {0x3a10, 0x28},
    {0x3a1b, 0x30},
    {0x3a1e, 0x26},
    {0x3a11, 0x60},
    {0x3a1f, 0x14},
    {0x3008, 0x02},
    {0x3035, 0x31},     //set the pixel clock output  --  94Mhz
    {0x3002, 0x1c},
    {0x3006, 0xc3},
    {0x3821, 0x07},
    {0x4300, 0x31},
    {0x501f, 0x00},
    {0x460c, 0x20},
    {0x3824, 0x04},
    {0x460b, 0x37},
    {0x4740, 0x21}, //set pixel clock, vsync hsync phase
    {0x4300, 0x31}, //set yuv order

};



//load ov5640 parameters
void ov5640_write_regs(void)
{
    int i;//,j;
    unsigned char buf[3];

    printk("ov5640: camera_client->addr = %x\n", camera_client->addr);

    for(i = 0; i < SCRIPT_LEN; i++)
    {
        buf[0] = (unsigned char)((ov5640_script[i].addr >> 8) & 0xff);
        buf[1] = (unsigned char)(ov5640_script[i].addr & 0xff);
	    buf[2] = ov5640_script[i].val;
        if((ov5640_write(buf, 3)) < 0)
        	break;
    }
    if(i >= SCRIPT_LEN)
    	printk("success in initial ov5640.\n");
    else
    	printk("fail in initial ov5640. \n");

    return;

}
static void ov5640_read_regs(void)
{
    u16 i, reg_addr = 0x300a ;//,j;
    unsigned char buf[4];

    buf[0] = (unsigned char)((reg_addr >> 8) & 0xff);
    buf[1] = (unsigned char)(reg_addr & 0xff);
    if((ov5640_read(buf, 2)) < 0)
    	printk("fail in read ov5640 regs %x. \n", reg_addr);
    else
    {
        printk(" ov5640 regs(%x) : %4x, %4x,\n", reg_addr, buf[0],buf[1] );
    }


    for(i = 0; i < SCRIPT_LEN; i++)
    {
        buf[0] = (unsigned char)((ov5640_script[i].addr >> 8) & 0xff);
        buf[1] = (unsigned char)(ov5640_script[i].addr & 0xff);
        if((ov5640_read(buf, 1)) < 0)
            break;
        else
           printk(" ov5640 regs(%x) : %4x, \n", ov5640_script[i].addr, buf[0]);
    }
    if(i >= SCRIPT_LEN)
        printk("success in initial ov5640.\n");
    else
        printk("fail in initial ov5640. \n");


    return;
}


#endif



void start_camera(void)
{
#if 0
    ov5640_write_regs();

#else

    reset_sw_camera();
    set_frame_rate();
    sensor_analog_ctl();
    sensor_sys_ctl();
    set_camera_light_freq();
    set_camera_resolution_timing(ov5640_info.para.resolution);
    set_other_para1();
    set_camera_resolution_others(ov5640_info.para.resolution);
    set_other_para2();
    sensor_sys_ctl_0();
    set_camera_light_mode(ov5640_info.awb_mode);
    set_other_para3();
    set_camera_sharpness(ov5640_info.para.sharpness);
    set_camera_gamma();
    sensor_set_color();
    set_lenc_para();
    set_camera_exposure(ov5640_info.para.exposure);
    wakeup_sw_camera();
    sensor_sys_ctl_1();

//    set_camera_test_mode(ov5640_info.test_mode);
#endif
    //ov5640_read_regs();
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

    if(ov5640_info.para.resolution != para->resolution)
    {
        memcpy(&ov5640_info.para, para,sizeof(struct camera_info_s) );
        start_camera();
        return;
    }

    if(ov5640_info.para.saturation != para->saturation)
    {
        set_camera_saturation(para->saturation);
        ov5640_info.para.saturation = para->saturation;
    }

    if(ov5640_info.para.brighrness != para->brighrness)
    {
        set_camera_brightness(para->brighrness);
        ov5640_info.para.brighrness = para->brighrness;
    }

    if(ov5640_info.para.contrast != para->contrast)
    {
        set_camera_contrast(para->contrast);
        ov5640_info.para.contrast = para->contrast;
    }

    if(ov5640_info.para.hue != para->hue)
    {
        set_camera_hue(para->hue);
        ov5640_info.para.hue = para->hue;
    }

//  if(ov5640_info.para.special_effect != para->special_effect)
//  {
//      set_camera_special_effect(para->special_effect);
//      ov5640_info.para.special_effect = para->special_effect;
//  }

    if(ov5640_info.para.exposure != para->exposure)
    {
        set_camera_exposure(para->exposure);
        ov5640_info.para.exposure = para->exposure;
    }

    if(ov5640_info.para.sharpness != para->sharpness)
    {
        set_camera_sharpness(para->sharpness);
        ov5640_info.para.sharpness = para->sharpness;
    }

    if(ov5640_info.para.mirro_flip != para->mirro_flip)
    {
        set_camera_mirror_flip(para->mirro_flip);
        ov5640_info.para.mirro_flip = para->mirro_flip;
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
                memcpy(&ov5640_info.para, &para,sizeof(struct camera_info_s) );
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
            copy_to_user((void __user *)arg, &ov5640_info.para, sizeof(struct camera_info_s));
            break;

        default:
                printk("camera ioctl cmd is invalid. \n");
                break;
    }
    return res;
}



static int ov5640_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	//unsigned char data[16] = {0};
	int res = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		return -1;
	}
	camera_client = client;
//    res = misc_register(&ov5640_device);
    printk( "ov5640: camera_client->addr = %x\n", camera_client->addr);
    msleep(10);
	return res;

}

static int ov5640_i2c_remove(struct i2c_client *client)
{

	return 0;
}


static const struct i2c_device_id oc5640_i2c_id[] = {
	{ OV5640_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver ov5640_driver = {
    .id_table	= oc5640_i2c_id,
	.probe 		= ov5640_i2c_probe,
	.remove 	= ov5640_i2c_remove,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= OV5640_I2C_NAME,
	},
};




static struct file_operations camera_fops = {
    .owner   = THIS_MODULE,
    .open    = camera_open,
    .release = camera_release,
    .ioctl   = camera_ioctl,
};


static int ov5640_probe(struct platform_device *pdev)
{
    int ret;
    struct device *devp;

    printk( "camera: start ov5640_probe. \n");

    camera_devp = kmalloc(sizeof(struct camera_dev_s), GFP_KERNEL);
    if (!camera_devp)
    {
        pr_err( "camera: failed to allocate memory for camera device\n");
        return -ENOMEM;
    }
    msleep(10);

    ret = alloc_chrdev_region(&camera_devno, 0, CAMERA_COUNT, OV5640_DEVICE_NAME);
	if (ret < 0) {
		pr_err("camera: failed to allocate chrdev. \n");
		return 0;
	}

    camera_clsp = class_create(THIS_MODULE, OV5640_DEVICE_NAME);
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
    ret = i2c_add_driver(&ov5640_driver);
    if (ret < 0 || camera_client == NULL) {
        pr_err("camera: failed to add i2c driver. \n");
        ret = -ENOTSUPP;
    }

    printk( "camera: driver initialized ok\n");
//    reset_hw_camera();      //remove into ***_board.c
    return ret;
}

static int ov5640_remove(struct platform_device *pdev)
{
//    power_down_hw_camera();     //remove into ***_board.c
    power_down_sw_camera();
    i2c_del_driver(&ov5640_driver);
    device_destroy(camera_clsp, MKDEV(MAJOR(camera_devno), 0));
    cdev_del(&camera_devp->cdev);
    class_destroy(camera_clsp);
    unregister_chrdev_region(camera_devno, CAMERA_COUNT);
    kfree(camera_devp);

     return 0;
}

static struct platform_driver ov5640_camera_driver = {
	.probe = ov5640_probe,
    .remove = ov5640_remove,
	.driver = {
		.name = OV5640_DEVICE_NAME,
//		.owner = THIS_MODULE,
	},
};


static int __init ov5640_init(void)
{
    int ret = 0;
    printk( "ov5640 driver: init. \n");
    ret = platform_driver_register(&ov5640_camera_driver);
    if (ret != 0) {
        printk(KERN_ERR "failed to register ov5640 module, error %d\n", ret);
        return -ENODEV;
    }
    return ret;

//	return i2c_add_driver(&ov5640_driver);
}

static void __exit ov5640_exit(void)
{
	pr_info("ov5640 driver: exit\n");
    platform_driver_unregister(&ov5640_camera_driver);
}

module_init(ov5640_init);
module_exit(ov5640_exit);

MODULE_AUTHOR("a@amlogic.com>");
MODULE_DESCRIPTION("camera device driver ov5640");
MODULE_LICENSE("GPL");
