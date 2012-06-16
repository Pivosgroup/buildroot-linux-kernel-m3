/*
 * Copyright (C) 2010-2011 AMLOGIC Incorporated.
 */
 
#include <linux/camera/amlogic_camera_common.h>



void set_camera_light_mode_ov5640(enum camera_light_mode_e mode)
{
    int i ;
    struct aml_camera_i2c_fig0_s ov5640_data[16] = {
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


void set_camera_saturation_ov5640(enum camera_saturation_e saturation)
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


void set_camera_brightness_ov5640(enum camera_brightness_e brighrness)
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

void set_camera_contrast_ov5640(enum camera_contrast_e contrast)
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

void set_camera_hue_ov5640(enum camera_hue_e hue)
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
void set_camera_special_effect_ov5640(enum camera_special_effect_e special_effect)
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

void set_camera_exposure_ov5640(enum camera_exposure_e exposure)
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
void set_camera_sharpness_ov5640(enum camera_sharpness_e sharpness)
{
    int i;
    struct aml_camera_i2c_fig0_s ov5640_data[6] = {
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

void set_camera_mirror_flip_ov5640(enum camera_mirror_flip_e mirror_flip)
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

void set_camera_test_mode_ov5640(enum camera_test_mode_e test_mode)
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

void set_camera_resolution_timing_ov5640(enum tvin_sig_fmt_e resolution)
{
    unsigned char reg3618=0, reg3612=0, i=0;
    struct aml_camera_i2c_fig0_s ov5640_data[11] = {  //default value for 720p output
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

void set_camera_resolution_others_ov5640(enum tvin_sig_fmt_e resolution)
{
    unsigned int i;
    struct aml_camera_i2c_fig0_s ov5640_data[4] = {  //default value for 720p output
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

void reset_sw_camera_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig_s ov5640_data[6] = {
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

void power_down_sw_camera_ov5640(void)
{
    ov5640_write_byte(0x3008, 0x42);
}

void wakeup_sw_camera_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig_s ov5640_data[4] = {
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

void set_frame_rate_ov5640(void)
{
    ov5640_write_word(0x3034, 0x111a);  //set PCLK output frequency(168Mhz)
    ov5640_write_word(0x3036, 0x1369);
    ov5640_write_byte(0x3108, 0x01);

}

void sensor_analog_ctl_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig_s ov5640_data[16] = {
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
void sensor_sys_ctl_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig_s ov5640_data[9] = {
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

    set_camera_mirror_flip_ov5640(amlogic_camera_info_ov5640.para.mirro_flip);
}

void sensor_sys_ctl_0_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig_s ov5640_data[11] = {
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
void sensor_set_color_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig_s ov5640_data[5] = {
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

    set_camera_saturation_ov5640(amlogic_camera_info_ov5640.para.saturation);
    set_camera_brightness_ov5640(amlogic_camera_info_ov5640.para.brighrness);
    set_camera_contrast_ov5640(amlogic_camera_info_ov5640.para.contrast);
    set_camera_hue_ov5640(amlogic_camera_info_ov5640.para.hue);

}


void set_camera_light_freq_ov5640(void)
{
        unsigned int i;
        struct aml_camera_i2c_fig0_s ov5640_data[4] = {  //default value for 720p output
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

void set_other_para1_ov5640(void)
{
    ov5640_write_word(0x3708, 0x5262);
    ov5640_write_byte(0x370c, 0x03);

}

void set_other_para2_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig_s ov5640_data[8] = {
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

void set_other_para3_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig0_s ov5640_data[5] = {
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


void sensor_sys_ctl_1_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig_s ov5640_data[9] = {
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

void set_camera_gamma_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig0_s ov5640_data[8] = {
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

void set_lenc_para_ov5640(void)
{
    unsigned int i;
    struct aml_camera_i2c_fig0_s ov5640_data[30] = {
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


struct aml_camera_i2c_fig_s ov5640_script[] = {
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
void ov5640_read_regs(void)
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

void start_camera_ov5640(void)
{
#if 0
    ov5640_write_regs();

#else

    reset_sw_camera_ov5640();
    set_frame_rate_ov5640();
    sensor_analog_ctl_ov5640();
    sensor_sys_ctl_ov5640();
    set_camera_light_freq_ov5640();
    set_camera_resolution_timing_ov5640(amlogic_camera_info_ov5640.para.resolution);
    set_other_para1_ov5640();
    set_camera_resolution_others_ov5640(amlogic_camera_info_ov5640.para.resolution);
    set_other_para2_ov5640();
    sensor_sys_ctl_0_ov5640();
    set_camera_light_mode_ov5640(amlogic_camera_info_ov5640.awb_mode);
    set_other_para3_ov5640();
    set_camera_sharpness_ov5640(amlogic_camera_info_ov5640.para.sharpness);
    set_camera_gamma_ov5640();
    sensor_set_color_ov5640();
    set_lenc_para_ov5640();
    set_camera_exposure_ov5640(amlogic_camera_info_ov5640.para.exposure);
    wakeup_sw_camera_ov5640();
    sensor_sys_ctl_1_ov5640();

//    set_camera_test_mode(amlogic_camera_info_ov5640.test_mode);
#endif
    //ov5640_read_regs();
    return;
}

void stop_camera_ov5640(void)
{
    power_down_sw_camera_ov5640();
    return;
}

void set_camera_para_ov5640(struct camera_info_s *para)
{
    //printk(KERN_INFO " set camera para. \n ");

    if(amlogic_camera_info_ov5640.para.resolution != para->resolution)
    {
        memcpy(&amlogic_camera_info_ov5640.para, para,sizeof(struct camera_info_s) );
        start_camera_ov5640();
        return;
    }

    if(amlogic_camera_info_ov5640.para.saturation != para->saturation)
    {
        set_camera_saturation_ov5640(para->saturation);
        amlogic_camera_info_ov5640.para.saturation = para->saturation;
    }

    if(amlogic_camera_info_ov5640.para.brighrness != para->brighrness)
    {
        set_camera_brightness_ov5640(para->brighrness);
        amlogic_camera_info_ov5640.para.brighrness = para->brighrness;
    }

    if(amlogic_camera_info_ov5640.para.contrast != para->contrast)
    {
        set_camera_contrast_ov5640(para->contrast);
        amlogic_camera_info_ov5640.para.contrast = para->contrast;
    }

    if(amlogic_camera_info_ov5640.para.hue != para->hue)
    {
        set_camera_hue_ov5640(para->hue);
        amlogic_camera_info_ov5640.para.hue = para->hue;
    }

//  if(amlogic_camera_info_ov5640.para.special_effect != para->special_effect)
//  {
//      set_camera_special_effect(para->special_effect);
//      amlogic_camera_info_ov5640.para.special_effect = para->special_effect;
//  }

    if(amlogic_camera_info_ov5640.para.exposure != para->exposure)
    {
        set_camera_exposure_ov5640(para->exposure);
        amlogic_camera_info_ov5640.para.exposure = para->exposure;
    }

    if(amlogic_camera_info_ov5640.para.sharpness != para->sharpness)
    {
        set_camera_sharpness_ov5640(para->sharpness);
        amlogic_camera_info_ov5640.para.sharpness = para->sharpness;
    }

    if(amlogic_camera_info_ov5640.para.mirro_flip != para->mirro_flip)
    {
        set_camera_mirror_flip_ov5640(para->mirro_flip);
        amlogic_camera_info_ov5640.para.mirro_flip = para->mirro_flip;
    }

    return;
}





















