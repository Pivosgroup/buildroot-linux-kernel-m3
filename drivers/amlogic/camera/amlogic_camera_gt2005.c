/*
 * Copyright (C) 2010-2011 AMLOGIC Incorporated.
 */
 
#include <linux/camera/amlogic_camera_common.h>



void set_camera_saturation_gt2005(enum camera_saturation_e saturation)
{
    unsigned char saturation_uv = 0;
    switch(saturation)
    {
        case SATURATION_N4_STEP:
            saturation_uv = 0x28;
            break;

        case SATURATION_N3_STEP:
            saturation_uv = 0x30;
            break;

        case SATURATION_N2_STEP:
            saturation_uv = 0x38;
            break;

        case SATURATION_N1_STEP:
            saturation_uv = 0x40;
            break;

        case SATURATION_P1_STEP:
            saturation_uv = 0x50;
            break;

        case SATURATION_P2_STEP:
            saturation_uv = 0x58;
            break;

        case SATURATION_P3_STEP:
            saturation_uv = 0x60;
            break;

        case SATURATION_P4_STEP:
            saturation_uv = 0x68;
            break;

        case SATURATION_0_STEP:
        default:
            saturation_uv = 0x48;
            break;
    }
    gt2005_write_byte(0x0202, saturation_uv);
}


void set_camera_brightness_gt2005(enum camera_brightness_e brighrness)
{
    unsigned char y_bright = 0;
    switch(brighrness)
    {
        case BRIGHTNESS_N4_STEP:
            y_bright = 0x80;
            break;

        case BRIGHTNESS_N3_STEP:
            y_bright = 0xa0;
            break;

        case BRIGHTNESS_N2_STEP:
            y_bright = 0xc0;
            break;

        case BRIGHTNESS_N1_STEP:
            y_bright = 0xe0;
            break;

        case BRIGHTNESS_P1_STEP:
            y_bright = 0x20;
            break;

        case BRIGHTNESS_P2_STEP:
            y_bright = 0x40;
            break;

        case BRIGHTNESS_P3_STEP:
            y_bright = 0x60;
            break;

        case BRIGHTNESS_P4_STEP:
            y_bright = 0x80;
            break;

        case BRIGHTNESS_0_STEP:
        default:
            y_bright = 0x00;
            break;
    }
    gt2005_write_byte(0x0201, y_bright);
 }

void set_camera_contrast_gt2005(enum camera_contrast_e contrast)
{
    unsigned char  v_data;
    switch(contrast)
    {
        case BRIGHTNESS_N4_STEP:
            v_data = 0x10;
            break;

        case BRIGHTNESS_N3_STEP:
            v_data = 0x18;
            break;

        case BRIGHTNESS_N2_STEP:
            v_data = 0x20;
            break;

        case BRIGHTNESS_N1_STEP:
            v_data = 0x28;
            break;

        case BRIGHTNESS_P1_STEP:
            v_data = 0x48;
            break;

        case BRIGHTNESS_P2_STEP:
            v_data = 0x50;
            break;

        case BRIGHTNESS_P3_STEP:
            v_data = 0x58;
            break;

        case BRIGHTNESS_P4_STEP:
            v_data = 0x60;
            break;

        case BRIGHTNESS_0_STEP:
        default:
            v_data = 0x30;
            break;
    }
    gt2005_write_byte(0x0200, v_data);
}

void set_camera_exposure_gt2005(enum camera_exposure_e exposure)
{
    unsigned char regd3; 
    switch(exposure)
    {
    	case EXPOSURE_N4_STEP:
    	    regd3 = 0x80;
    	    break;
    	case EXPOSURE_N3_STEP:
	    regd3 = 0xa0;
	    break;
	case EXPOSURE_N2_STEP:
	    regd3 = 0xc0;
	    break;
	case EXPOSURE_N1_STEP:
	    regd3 = 0xe0;
	    break;
	case EXPOSURE_P1_STEP:
	    regd3 = 0x20;
	    break;
	case EXPOSURE_P2_STEP:
	    regd3 = 0x40;
	    break;
	case EXPOSURE_P3_STEP:
	    regd3 = 0x60;
	    break;
	case EXPOSURE_P4_STEP:
	    regd3 = 0x80;
	    break;
	case EXPOSURE_0_STEP:
	    default:
	    regd3 = 0x80;
	    break;
    }
    gt2005_write_byte(0x0201, regd3);
}

void set_camera_mirror_flip_gt2005(enum camera_mirror_flip_e mirror_flip)
{
    switch(mirror_flip)
    {
    	case MF_MIRROR:
    	    gt2005_write_byte(0x0101, 0x01);
    	    break;
    	case MF_FLIP:
    	    gt2005_write_byte(0x0101, 0x02);
    	    break;
    	case MF_MIRROR_FLIP:
    	    gt2005_write_byte(0x0101, 0x03);
    	    break;
    	case MF_NORMAL:
    	default:
    	    gt2005_write_byte(0x0101, 0x00);
    	    break;
    }
}
/*************************************************************************
* FUNCTION
*    GT2005_set_param_effect
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
void GT2005_set_param_effect(enum camera_effect_flip_e para)//特效设置
{
  
switch (para)
	{
		case CAM_EFFECT_ENC_NORMAL://正常
			gt2005_write_byte(0x0115,0x00);
			break;		

		case CAM_EFFECT_ENC_GRAYSCALE://灰阶
			gt2005_write_byte(0x0115,0x06);
			break;

		case CAM_EFFECT_ENC_SEPIA://复古
		     	gt2005_write_byte(0x0115,0x0a);
			gt2005_write_byte(0x026e,0x60);
			gt2005_write_byte(0x026f,0xa0);
			break;		
				
		case CAM_EFFECT_ENC_SEPIAGREEN://复古绿
			gt2005_write_byte(0x0115,0x0a);
			gt2005_write_byte(0x026e,0x20);
			gt2005_write_byte(0x026f,0x00);
			break;					

		case CAM_EFFECT_ENC_SEPIABLUE://复古蓝
			gt2005_write_byte(0x0115,0x0a);
			gt2005_write_byte(0x026e,0xfb);
			gt2005_write_byte(0x026f,0x00);
			break;								

		case CAM_EFFECT_ENC_COLORINV://底片
			gt2005_write_byte(0x0115,0x09);
			break;		

		default:
			break;
	}


} /* GT2005_set_param_effect */

/*************************************************************************
* FUNCTION
*    GT2005_NightMode
*
* DESCRIPTION
*    This function night mode of GT2005.
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
void GT2005_NightMode(enum  camera_night_mode_flip_e enable)
{

	if (enable)
	{
		gt2005_write_byte(0x0312 , 0xc8); //Camera Enable night mode  1/5 Frame rate
	}
	else
	{
		gt2005_write_byte(0x0312 , 0x98); //Disable night mode  1/2 Frame rate
	}

}    /* GT2005_NightMode */

/*************************************************************************
* FUNCTION
*    GT2005_set_param_wb
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
void GT2005_set_param_wb(enum  camera_wb_flip_e para)//白平衡
{

    switch (para)
	{
		
		case CAM_WB_AUTO://自动
			gt2005_write_byte(0x031a , 0x81);
			gt2005_write_byte(0x0320 , 0x24);
			gt2005_write_byte(0x0321 , 0x14);
			gt2005_write_byte(0x0322 , 0x1a);
			gt2005_write_byte(0x0323 , 0x24);
			gt2005_write_byte(0x0441 , 0x4B);
			gt2005_write_byte(0x0442 , 0x00);
			gt2005_write_byte(0x0443 , 0x00);
			gt2005_write_byte(0x0444 , 0x31);
			break;

		case CAM_WB_CLOUD: //阴天
			gt2005_write_byte(0x0320 , 0x02);
			gt2005_write_byte(0x0321 , 0x02);
			gt2005_write_byte(0x0322 , 0x02);
			gt2005_write_byte(0x0323 , 0x02);
			gt2005_write_byte(0x0441 , 0x80);
			gt2005_write_byte(0x0442 , 0x00);
			gt2005_write_byte(0x0443 , 0x00);
			gt2005_write_byte(0x0444 , 0x0D);
			break;

		case CAM_WB_DAYLIGHT: //
			gt2005_write_byte(0x0320 , 0x02);
			gt2005_write_byte(0x0321 , 0x02);
			gt2005_write_byte(0x0322 , 0x02);
			gt2005_write_byte(0x0323 , 0x02);
			gt2005_write_byte(0x0441 , 0x60);
			gt2005_write_byte(0x0442 , 0x00);
			gt2005_write_byte(0x0443 , 0x00);
			gt2005_write_byte(0x0444 , 0x14);
			break;

		case CAM_WB_INCANDESCENCE: 
			gt2005_write_byte(0x0320 , 0x02);
			gt2005_write_byte(0x0321 , 0x02);
			gt2005_write_byte(0x0322 , 0x02);
			gt2005_write_byte(0x0323 , 0x02);
			gt2005_write_byte(0x0441 , 0x50);
			gt2005_write_byte(0x0442 , 0x00);
			gt2005_write_byte(0x0443 , 0x00);
			gt2005_write_byte(0x0444 , 0x30);
			break;
			
		case CAM_WB_TUNGSTEN: 
			gt2005_write_byte(0x0320 , 0x02);
			gt2005_write_byte(0x0321 , 0x02);
			gt2005_write_byte(0x0322 , 0x02);
			gt2005_write_byte(0x0323 , 0x02);
			gt2005_write_byte(0x0441 , 0x0B);
			gt2005_write_byte(0x0442 , 0x00);
			gt2005_write_byte(0x0443 , 0x00);
			gt2005_write_byte(0x0444 , 0x5E);
			break;

      	case CAM_WB_FLUORESCENT:
      		gt2005_write_byte(0x0320 , 0x02);
			gt2005_write_byte(0x0321 , 0x02);
			gt2005_write_byte(0x0322 , 0x02);
			gt2005_write_byte(0x0323 , 0x02);
			gt2005_write_byte(0x0441 , 0x43);
			gt2005_write_byte(0x0442 , 0x00);
			gt2005_write_byte(0x0443 , 0x00);
			gt2005_write_byte(0x0444 , 0x4B);
			break;

		case CAM_WB_MANUAL:
		    	// TODO
			break;
	}
	

} /* GT2005_set_param_wb */

/*************************************************************************
* FUNCTION
*    GT2005_set_param_exposure
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
void GT2005_set_param_exposure(enum camera_exposure_e para)//曝光调节
{


    switch (para)
	{
		case EXPOSURE_N4_STEP:  //负4档  
            		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0x40);
			gt2005_write_byte(0x0201 , 0x90);
			break;
			
		case EXPOSURE_N3_STEP:
            		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0x50);
			gt2005_write_byte(0x0201 , 0xa0);
			break;
			
		case EXPOSURE_N2_STEP:
            		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0x60);
			gt2005_write_byte(0x0201 , 0xb0);
			break;
			
		case EXPOSURE_N1_STEP:
            		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0x70);
			gt2005_write_byte(0x0201 , 0xd0);
			break;
			
		case EXPOSURE_0_STEP://默认零档
           		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0x80);
			gt2005_write_byte(0x0201 , 0x0c);
			break;
			
		case EXPOSURE_P1_STEP://正一档
            		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0x90);
			gt2005_write_byte(0x0201 , 0x30);
			break;
			
		case EXPOSURE_P2_STEP:
            		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0xa0);
			gt2005_write_byte(0x0201 , 0x50);
			break;
			
		case EXPOSURE_P3_STEP:
            		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0xb0);
			gt2005_write_byte(0x0201 , 0x60);
			break;
			
		case EXPOSURE_P4_STEP:	
            		gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0xc0);
			gt2005_write_byte(0x0201 , 0x70);
			break;
			
		default:
			gt2005_write_byte(0x0300 , 0x81);
			gt2005_write_byte(0x0301 , 0x80);
			gt2005_write_byte(0x0201 , 0x0c);
			break;
	}


} /* GT2005_set_param_exposure */

static void GT2005_write_shutter(void)
{
/* YUV : setShutter would not be called */  
unsigned char shutter,AGain_shutter,DGain_shutter;

	gt2005_write_byte(0x0300, 0xc1);
	shutter= (gt2005_read_byte(0x0012)<<8 )|( gt2005_read_byte(0x0013));
	AGain_shutter= (gt2005_read_byte(0x0014)<<8 )|( gt2005_read_byte(0x0015));
	DGain_shutter= (gt2005_read_byte(0x0016)<<8 )|( gt2005_read_byte(0x0017));
	gt2005_write_byte(0x0300, 0x41); //close ALC

	#ifndef INTERPOLATION_3M
	shutter = shutter / 2; 
	#else
	shutter = shutter / 4; 
	#endif

	gt2005_write_byte(0x0305,  shutter&0xff);
	gt2005_write_byte(0x0304, (shutter >>8)&0xff); 
	gt2005_write_byte(0x0307,  AGain_shutter&0xff); 
	gt2005_write_byte(0x0306, (AGain_shutter >>8)&0xff); //AG
	gt2005_write_byte(0x0308,  DGain_shutter&0xff);   //DG

}    /* GT2005_write_shutter */

void GT2005_Capture(void)
{
//拍照函数

	/* 2M FULL Mode */
	GT2005_write_shutter();
	//Sharpness&Noise
	gt2005_write_byte(0x020B , 0x28);
	gt2005_write_byte(0x020C , 0x44);

	gt2005_write_byte(0x040A , 0x00);
	gt2005_write_byte(0x040B , 0xf6);

	//Resolution Setting
	gt2005_write_byte(0x0109 ,0x01);
	gt2005_write_byte(0x010A ,0x00);
	gt2005_write_byte(0x010B ,0x00);
	gt2005_write_byte(0x0110 ,0x06);
	gt2005_write_byte(0x0111 ,0x40);
	gt2005_write_byte(0x0112 ,0x04);
	gt2005_write_byte(0x0113 ,0xb0);
		
	// 2M Capture
	#ifndef INTERPOLATION_3M
	//PLL Setting 4.65FPS Under 24MHz PCLK
	gt2005_write_byte(0x0119 ,0x02);
	gt2005_write_byte(0x011B ,0x01);
	#else
	gt2005_write_byte(0x0119 ,0x05);
	gt2005_write_byte(0x011B ,0x01);
	#endif
	printk("GT2005_Capture.\n");


}    /* GT2005_Capture */

static void power_down_sw_camera_gt2005(void)
{
	gt2005_write_byte(0x0104, 0x00);
	gt2005_write_byte(0x0100, 0x00);
}

static void wakeup_sw_camera_gt2005(void)
{
	gt2005_write_byte(0x0100, 0x01);
	gt2005_write_byte(0x0104, 0x03);
}

#if 1
struct aml_camera_i2c_fig_s GT2005_script[] = { 
#ifdef CONFIG_MACH_MESON_8726M_REFB10
	{0x0101 , 0x02},
#endif
	{0x0102 , 0x01},
	{0x0103 , 0x00},

	//Hcount&Vcount
	{0x0105 , 0x00},
	{0x0106 , 0xF0},
	{0x0107 , 0x00},
	{0x0108 , 0x1C},

	//Binning&Resoultion
	{0x0109 , 0x00},
	{0x010A , 0x04},	
	{0x010B , 0x0F},
	{0x010C , 0x00},
	{0x010D , 0x08},
	{0x010E , 0x00},
	{0x010F , 0x08},
	{0x0110 , 0x03},					
	{0x0111 , 0x20},					
	{0x0112 , 0x02},					
	{0x0113 , 0x58},

	//YUV Mode
	{0x0114 , 0x06},

	//Picture Effect
	{0x0115 , 0x00},

	//PLL&Frame Rate
/*	{0x0116 , 0x02},
	{0x0117 , 0x00},
	{0x0118 , 0x34},
	{0x0119 , 0x01},
	{0x011A , 0x04},
	{0x011B , 0x00},
	*/
	
		//PLL&Frame Rate
	{0x0116 , 0x01},
	{0x0117 , 0x00},
	{0x0118 , 0x34},
	{0x0119 , 0x01},
	{0x011A , 0x04},
	{0x011B , 0x00},

	//DCLK Polarity
	{0x011C , 0x00},

	//Do not change
	{0x011D , 0x02},
	{0x011E , 0x00},
	
	{0x011F , 0x00},
	{0x0120 , 0x1C},
	{0x0121 , 0x00},
	{0x0122 , 0x04},
	{0x0123 , 0x00},
	{0x0124 , 0x00},
	{0x0125 , 0x00},
	{0x0126 , 0x00},
	{0x0127 , 0x00},
	{0x0128 , 0x00},

	//Contrast
	{0x0200 , 0x10}, // 0x30 kim 

	//Brightness
	{0x0201 , 0x10}, //0x00 kim 

	//Saturation
	{0x0202 , 0x40}, // 0x48 kim 

	//Do not change
	{0x0203 , 0x00},
	{0x0204 , 0x03},
	{0x0205 , 0x1F},
	{0x0206 , 0x0B},
	{0x0207 , 0x20},
	{0x0208 , 0x00},
	{0x0209 , 0x2A},
	{0x020A , 0x01},

	//Sharpness
	{0x020B , 0x48},
	{0x020C , 0x64},

	//Do not change
	{0x020D , 0xC8},
	{0x020E , 0xBC},
	{0x020F , 0x08},
	{0x0210 , 0xE6},
	{0x0211 , 0x00},
	{0x0212 , 0x20},
	{0x0213 , 0x81},
	{0x0214 , 0x15},
	{0x0215 , 0x00},
	{0x0216 , 0x00},
	{0x0217 , 0x00},
	{0x0218 , 0x46},
	{0x0219 , 0x30},
	{0x021A , 0x03},
	{0x021B , 0x28},
	{0x021C , 0x02},
	{0x021D , 0x60},
	{0x021E , 0x00},
	{0x021F , 0x00},

	{0x0220 , 0x08},
	{0x0221 , 0x08},
	{0x0222 , 0x04},
	{0x0223 , 0x00},
	{0x0224 , 0x1F},
	{0x0225 , 0x1E},
	{0x0226 , 0x18},
	{0x0227 , 0x1D},
	{0x0228 , 0x1F},
	{0x0229 , 0x1F},
	{0x022A , 0x01},
	{0x022B , 0x04},
	{0x022C , 0x05},
	{0x022D , 0x05},
	{0x022E , 0x04},
	{0x022F , 0x03},
	{0x0230 , 0x02},
	{0x0231 , 0x1F},
	{0x0232 , 0x1A},
	{0x0233 , 0x19},
	{0x0234 , 0x19},
	{0x0235 , 0x1B},
	{0x0236 , 0x1F},
	{0x0237 , 0x04},
	{0x0238 , 0xEE},
	{0x0239 , 0xFF},
	{0x023A , 0x00},
	{0x023B , 0x00},
	{0x023C , 0x00},
	{0x023D , 0x00},
	{0x023E , 0x00},
	{0x023F , 0x00},
	{0x0240 , 0x00},
	{0x0241 , 0x00},
	{0x0242 , 0x00},
	{0x0243 , 0x21},
	{0x0244 , 0x42},
	{0x0245 , 0x53},
	{0x0246 , 0x54},
	{0x0247 , 0x54},
	{0x0248 , 0x54},
	{0x0249 , 0x33},
	{0x024A , 0x11},
	{0x024B , 0x00},
	{0x024C , 0x00},
	{0x024D , 0xFF},
	{0x024E , 0xEE},
	{0x024F , 0xDD},
	{0x0250 , 0x00},
	{0x0251 , 0x00},
	{0x0252 , 0x00},
	{0x0253 , 0x00},
	{0x0254 , 0x00},
	{0x0255 , 0x00},
	{0x0256 , 0x00},
	{0x0257 , 0x00},
	{0x0258 , 0x00},
	{0x0259 , 0x00},
	{0x025A , 0x00},
	{0x025B , 0x00},
	{0x025C , 0x00},
	{0x025D , 0x00},
	{0x025E , 0x00},
	{0x025F , 0x00},
	{0x0260 , 0x00},
	{0x0261 , 0x00},
	{0x0262 , 0x00},
	{0x0263 , 0x00},
	{0x0264 , 0x00},
	{0x0265 , 0x00},
	{0x0266 , 0x00},
	{0x0267 , 0x00},
	{0x0268 , 0x8F},
	{0x0269 , 0xA3},
	{0x026A , 0xB4},
	{0x026B , 0x90},
	
	{0x026C , 0x00},
	{0x026D , 0xD0},
	{0x026E , 0x60},
	{0x026F , 0xA0},
	{0x0270 , 0x40},
	{0x0300 , 0x81},
	{0x0301 , 0x90}, // 0x80  kim
	{0x0302 , 0x22},
	{0x0303 , 0x06},
	{0x0304 , 0x03},
	{0x0305 , 0x83},
	{0x0306 , 0x00},
	{0x0307 , 0x22},
	{0x0308 , 0x00},
	{0x0309 , 0x55},
	{0x030A , 0x55},
	{0x030B , 0x55},
	{0x030C , 0x54},
	{0x030D , 0x13},
	{0x030E , 0x13},
	{0x030F , 0x10},
	{0x0310 , 0x04},
	{0x0311 , 0xFF},
	{0x0312 , 0x08},

	//Banding Setting{50Hz}
		{0x0313 , 0x38},
	{0x0314 , 0x76},
	//{0x0313 , 0x34},
	//{0x0314 , 0x3b},
	{0x0315 , 0x16},
	{0x0316 , 0x26},
	
	{0x0317 , 0x02},
	{0x0318 , 0x08},
	{0x0319 , 0x0C},

	//AWB
	//A LIGHT CORRECTION
	{0x031A , 0x81},
	{0x031B , 0x00},
	{0x031C , 0x1D},
	{0x031D , 0x00},
	{0x031E , 0xFD},
	{0x031F , 0x00},
	{0x0320 , 0xE1},
	{0x0321 , 0x1A},
	{0x0322 , 0xDE},
	{0x0323 , 0x11},
	{0x0324 , 0x1A},
	{0x0325 , 0xEE},
	{0x0326 , 0x50},
	{0x0327 , 0x18},
	{0x0328 , 0x25},
	{0x0329 , 0x37},
	{0x032A , 0x24},
	{0x032B , 0x32},
	{0x032C , 0xA9},
	{0x032D , 0x32},
	{0x032E , 0xFF},
	{0x032F , 0x7F},
	{0x0330 , 0xBA},
	{0x0331 , 0x7F},
	{0x0332 , 0x7F},
	{0x0333 , 0x14},
	{0x0334 , 0x81},
	{0x0335 , 0x14},
	{0x0336 , 0xFF},
	{0x0337 , 0x20},
	{0x0338 , 0x46},
	{0x0339 , 0x04},
	{0x033A , 0x04},
	{0x033B , 0x00},
	{0x033C , 0x00},
	{0x033D , 0x00},

	//Do not change
	{0x033E , 0x03},
	{0x033F , 0x28},
	{0x0340 , 0x02},
	{0x0341 , 0x60},
	{0x0342 , 0xAC},
	{0x0343 , 0x97},
	{0x0344 , 0x7F},
	{0x0400 , 0xE8},
	{0x0401 , 0x40},
	{0x0402 , 0x00},
	{0x0403 , 0x00},
	{0x0404 , 0xF8},
	{0x0405 , 0x03},
	{0x0406 , 0x03},
	{0x0407 , 0x85},
	{0x0408 , 0x44},
	{0x0409 , 0x1F},
	{0x040A , 0x40},
	{0x040B , 0x33},

	//Lens Shading Correction{Default Sunny}
	{0x040C , 0xA0},
	{0x040D , 0x00},
	{0x040E , 0x00},
	{0x040F , 0x00},
	{0x0410 , 0x02},
	{0x0411 , 0x02},
	{0x0412 , 0x00},
	{0x0413 , 0x00},
	{0x0414 , 0x00},
	{0x0415 , 0x00},
	{0x0416 , 0x00},
	{0x0417 , 0x04},
	{0x0418 , 0x02},
	{0x0419 , 0x02},
	{0x041a , 0x00},
	{0x041b , 0x00},
	{0x041c , 0x04},
	{0x041d , 0x04},
	{0x041e , 0x00},
	{0x041f , 0x00},
	{0x0420 , 0x2A},
	{0x0421 , 0x2A},
	{0x0422 , 0x27},
	{0x0423 , 0x22},
	{0x0424 , 0x2A}, 
	{0x0425 , 0x2A},
	{0x0426 , 0x27},
	{0x0427 , 0x22},
	{0x0428 , 0x07},
	{0x0429 , 0x07},
	{0x042a , 0x08},
	{0x042b , 0x06},
	{0x042c , 0x11},
	{0x042d , 0x11},
	{0x042e , 0x0F},
	{0x042f , 0x0D},
	{0x0430 , 0x00},
	{0x0431 , 0x00},
	{0x0432 , 0x00},
	{0x0433 , 0x00},
	{0x0434 , 0x00},
	{0x0435 , 0x00},
	{0x0436 , 0x00},
	{0x0437 , 0x00},
	{0x0438 , 0x00},
	{0x0439 , 0x00},
	{0x043a , 0x00},
	{0x043b , 0x00},
	{0x043c , 0x00},
	{0x043d , 0x00},
	{0x043e , 0x00},
	{0x043f , 0x00},

	//PWB Gain
	{0x0440 , 0x00},
	{0x0441 , 0x5F},
	{0x0442 , 0x00},
	{0x0443 , 0x00},
	{0x0444 , 0x40}, //0x17 kim 

	{0x0445 , 0x00},
	{0x0446 , 0x00},
	{0x0447 , 0x00},
	{0x0448 , 0x00},
	{0x0449 , 0x00},
	{0x044A , 0x00},
	{0x044D , 0xE0},
	{0x044E , 0x05},
	{0x044F , 0x07},
	{0x0450 , 0x00},
	{0x0451 , 0x00},
	{0x0452 , 0x00},
	{0x0453 , 0x00},
	{0x0454 , 0x00},
	{0x0455 , 0x00},
	{0x0456 , 0x00},
	{0x0457 , 0x00},
	{0x0458 , 0x00},
	{0x0459 , 0x00},
	{0x045A , 0x00},
	{0x045B , 0x00},
	{0x045C , 0x00},
	{0x045D , 0x00},
	{0x045E , 0x00},
	{0x045F , 0x00},

	//GAMMA Correction
	{0x0460 , 0x80},
	{0x0461 , 0x10},
	{0x0462 , 0x10},
	{0x0463 , 0x10},
	{0x0464 , 0x08},
	{0x0465 , 0x08},
	{0x0466 , 0x11},
	{0x0467 , 0x09},
	{0x0468 , 0x23},
	{0x0469 , 0x2A},
	{0x046A , 0x2A},
	{0x046B , 0x47},
	{0x046C , 0x52},
	{0x046D , 0x42},
	{0x046E , 0x36},
	{0x046F , 0x46},
	{0x0470 , 0x3A},
	{0x0471 , 0x32},
	{0x0472 , 0x32},
	{0x0473 , 0x38},
	{0x0474 , 0x3D},
	{0x0475 , 0x2F},
	{0x0476 , 0x29},
	{0x0477 , 0x48},

	{0x0686 , 0x6F},
	
	//Output Enable
	{0x0100 , 0x01},
	{0x0102 , 0x02},
	{0x0104 , 0x03},
	{0xffff,0xff},
};

//load GT2005 parameters
void GT2005_write_regs(void)
{
    int i=0;
    
    while(1)
    {
        if (GT2005_script[i].val==0xff&&GT2005_script[i].addr==0xffff) 
        {
        	printk("GT2005_write_regs success in initial GT2005.\n");
        	break;
        }
        
        if((gt2005_write_byte(GT2005_script[i].addr, GT2005_script[i].val)) < 0)
        {
        	printk("fail in initial GT2005. \n");
		break;
	}
	i++;
    }
    return;
}

void GT2005_read_regs(void)
{
    int i=0;
    unsigned short reg_addr = 0x0100;
    unsigned char val = 0;
    
    if((val = gt2005_read_byte(reg_addr)) < 0)
    {
    	printk("GT2005_read_regs 1 fail in read GT2005 regs %x. \n", reg_addr);
    }
    else
    {
        printk(" GT2005_read_regs 1 GT2005 regs(%x) :  %4x,\n", reg_addr, val);
    }
    
    while(1)    
    {
    	reg_addr = GT2005_script[i].addr;

        if((val = gt2005_read_byte(reg_addr)) < 0)
        {
        	printk("GT2005_read_regs 2 fail in initial GT2005.\n");
        	break;
        }
        else
        {
        	printk(" GT2005_read_regs 2 GT2005 regs(%x) : %4x, \n", reg_addr, val);
        }
           
        i++;
        
	if(GT2005_script[i].addr==0xffff)
	{
		printk("GT2005_read_regs 2 success in initial GT2005.\n");
		break;
	}
    }
    return;
}
#endif

void start_camera_gt2005(void)
{
#if 1
    GT2005_write_regs();   
#else
    //set_camera_test_mode(GT2005_info.test_mode);
#endif
    //GT2005_read_regs();
    return;
}

void stop_camera_gt2005(void)
{
    power_down_sw_camera_gt2005();
    return;
}

void set_camera_para_gt2005(struct camera_info_s *para)
{
	if(amlogic_camera_info_gt2005.para.resolution != para->resolution)
	{
		memcpy(&amlogic_camera_info_gt2005.para, para,sizeof(struct camera_info_s) );
		return;
	}

	if(amlogic_camera_info_gt2005.para.saturation != para->saturation)
	{
		set_camera_saturation_gt2005(para->saturation);
		amlogic_camera_info_gt2005.para.saturation = para->saturation;
	}

	if(amlogic_camera_info_gt2005.para.brighrness != para->brighrness)
	{
		set_camera_brightness_gt2005(para->brighrness);
		amlogic_camera_info_gt2005.para.brighrness = para->brighrness;
	}

	if(amlogic_camera_info_gt2005.para.contrast != para->contrast)
	{
		set_camera_contrast_gt2005(para->contrast);
		amlogic_camera_info_gt2005.para.contrast = para->contrast;
	}

	if(amlogic_camera_info_gt2005.para.hue != para->hue)
	{
		amlogic_camera_info_gt2005.para.hue = para->hue;
	}

	if(amlogic_camera_info_gt2005.para.exposure != para->exposure)
	{
		GT2005_set_param_exposure(para->exposure);
		amlogic_camera_info_gt2005.para.exposure = para->exposure;
	}

	if(amlogic_camera_info_gt2005.para.sharpness != para->sharpness)
	{
		amlogic_camera_info_gt2005.para.sharpness = para->sharpness;
	}

	//if(amlogic_camera_info_gt2005.para.mirro_flip != para->mirro_flip)
	{
		set_camera_mirror_flip_gt2005(para->mirro_flip);
		amlogic_camera_info_gt2005.para.mirro_flip = para->mirro_flip;
	}
	if(amlogic_camera_info_gt2005.para.white_balance != para->white_balance)
	{
		GT2005_set_param_wb(para->white_balance);
		amlogic_camera_info_gt2005.para.white_balance = para->white_balance;
	}
	if(amlogic_camera_info_gt2005.para.night_mode != para->night_mode)
	{
		GT2005_NightMode(para->night_mode);
		amlogic_camera_info_gt2005.para.night_mode = para->night_mode;
	}
	if(amlogic_camera_info_gt2005.para.effect != para->effect)
	{
		GT2005_set_param_effect(para->effect);
		amlogic_camera_info_gt2005.para.effect = para->effect;
	}
	return ;
}


  
  



















