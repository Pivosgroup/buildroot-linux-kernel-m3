/*
 * Copyright (C) 2010-2011 AMLOGIC Incorporated.
 */
 
#include <linux/camera/amlogic_camera_common.h>



void set_camera_saturation_gc0308(enum camera_saturation_e saturation)
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
    gc0308_write_byte(0xb1, saturation_u);
    gc0308_write_byte(0xb1, saturation_v);
}


void set_camera_brightness_gc0308(enum camera_brightness_e brighrness)
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
    gc0308_write_byte(0xb5, y_bright);   //
 }

void set_camera_contrast_gc0308(enum camera_contrast_e contrast)
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
    gc0308_write_byte(0xb3, 0x80);   //
    gc0308_write_byte(0x40, 0x80);   //
}

void set_camera_exposure_gc0308(enum camera_exposure_e exposure)
{
    unsigned char regd3; 
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

	gc0308_write_byte(0xd3, regd3);
}


void set_camera_mirror_flip_gc0308(enum camera_mirror_flip_e mirror_flip)
{
	switch(mirror_flip)
	    {
    		case MF_MIRROR:
			gc0308_write_byte(0x14, 0x11);
	            break;

	        case MF_FLIP:
			gc0308_write_byte(0x14, 0x12);
	            break;

	        case MF_MIRROR_FLIP:
			gc0308_write_byte(0x14, 0x13);
	            break;

	        case MF_NORMAL:
	        default:
			gc0308_write_byte(0x14, 0x10);
	            break;
	    }
}

void power_down_sw_camera_gc0308(void)
{
	gc0308_write_byte(0x25, 0x00);
	gc0308_write_byte(0x1a, 0x17);
}

void wakeup_sw_camera_gc0308(void)
{
	gc0308_write_byte(0x25, 0x0f);
	gc0308_write_byte(0x1a, 0x2a);
}

#if 1

struct aml_camera_i2c_fig1_s GC0308_script[] = {  
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
	{0x01 , 0x6a},                                    
	{0x02 , 0x70},                                  
	{0x0f , 0x00},                                  


	{0xe2 , 0x00},   //anti-flicker step [11:8]     
	{0xe3 , 0x96},   //anti-flicker step [7:0]      

	{0xe4 , 0x02},       
	{0xe5 , 0x58},                                  
	{0xe6 , 0x02},           
	{0xe7 , 0x58},                                  
	{0xe8 , 0x02},           
	{0xe9 , 0x58},                                  
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
	{0x1f,0x3f},//16
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
        if((gc0308_write(buf, 2)) < 0)
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
void GC0308_read_regs(void)
{
    u16 i=0, reg_addr = 0x14 ;//,j;
    unsigned char buf[4];

	buf[0] = reg_addr ;
    //buf[0] = (unsigned char)((reg_addr >> 8) & 0xff);
    //buf[1] = (unsigned char)(reg_addr & 0xff);
    if((gc0308_read(buf, 1)) < 0)
    	printk("GC0308_read_regs111 fail in read GC0308 regs %x. \n", reg_addr);
    else
    {
        printk(" GC0308_read_regs111 GC0308 regs(%x) :  %4x,\n", reg_addr, buf[0] );
    }

	while(1)    
	{
        buf[0] = GC0308_script[i].addr;

        //buf[0] = (unsigned char)((GC0308_script[i].addr >> 8) & 0xff);
        //buf[1] = (unsigned char)(GC0308_script[i].addr & 0xff);
        if((gc0308_read(buf, 1)) < 0)
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

void start_camera_gc0308(void)
{
#if 1
    GC0308_write_regs();   
#else

//    set_camera_test_mode(GC0308_info.test_mode);
#endif
  //  GC0308_read_regs();
    return;
}

void stop_camera_gc0308(void)
{
    power_down_sw_camera_gc0308();
    return;
}

void set_camera_para_gc0308(struct camera_info_s *para)
{
	return ;
}


  
  



















