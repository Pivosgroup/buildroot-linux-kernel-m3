/*
 * Copyright (C) 2008-2009 QUALCOMM Incorporated.
 */

#ifndef _AMLOGIC_CAMERA_COMMON_H
#define _AMLOGIC_CAMERA_COMMON_H

#ifdef _AMLOGIC_CAMERA_COMMON_C
#define FUNCRION_SCOPE 
#else
#define FUNCRION_SCOPE extern
#endif


#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/tvin/tvin.h>
#include <linux/string.h>
/*


*/
// 1 is for OV5640 camera address format :double bytes
#define FIRST_CAMERA_I2C_ADDRESS_FORMAT	 	1

// 0 is for gc0308 camera address format :single byte
#define SECOND_CAMERA_I2C_ADDRESS_FORMAT	 0 

#define AMLOGIC_CAMERA_OV5640_NAME     			"camera_ov5640"
#define AMLOGIC_CAMERA_OV5640_I2C_NAME			"ov5640_i2c"

#define AMLOGIC_CAMERA_GC0308_NAME     			"camera_gc0308"
#define AMLOGIC_CAMERA_GC0308_I2C_NAME        	"gc0308_i2c"

#define AMLOGIC_CAMERA_GT2005_NAME     			"camera_gt2005"
#define AMLOGIC_CAMERA_GT2005_I2C_NAME        	        "gt2005_i2c"
#ifdef CONFIG_CAMERA_OV5640
#define AMLOGIC_CAMERA_DEVICE_NAME_FIRST      AMLOGIC_CAMERA_OV5640_NAME
#define AMLOGIC_CAMERA_I2C_NAME_FIRST         AMLOGIC_CAMERA_OV5640_I2C_NAME
#endif

#ifdef CONFIG_CAMERA_GT2005
#define FIRST_CAMERA_I2C_ADDRESS_FORMAT	 	1

#define AMLOGIC_CAMERA_DEVICE_NAME_FIRST      AMLOGIC_CAMERA_GT2005_NAME
#define AMLOGIC_CAMERA_I2C_NAME_FIRST         AMLOGIC_CAMERA_GT2005_I2C_NAME
#endif


#ifdef CONFIG_CAMERA_GC0308
#define FIRST_CAMERA_I2C_ADDRESS_FORMAT	 	0

#define AMLOGIC_CAMERA_DEVICE_NAME_FIRST      AMLOGIC_CAMERA_GC0308_NAME
#define AMLOGIC_CAMERA_I2C_NAME_FIRST         AMLOGIC_CAMERA_GC0308_I2C_NAME
#endif

#ifdef CONFIG_AMLOGIC_SECOND_CAMERA_ENABLE
#define AMLOGIC_CAMERA_DEVICE_NAME_SECOND     AMLOGIC_CAMERA_GC0308_NAME
#define AMLOGIC_CAMERA_I2C_NAME_SECOND        AMLOGIC_CAMERA_GC0308_I2C_NAME
#endif




/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/


typedef struct camera_dev_s {
    /* ... */
    struct cdev         cdev;             /* The cdev structure */
} camera_dev_t;


typedef enum camera_test_mode_e {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
}camera_test_mode_t;


typedef enum camera_reg_update_e {
	REG_INIT, 				/* registers that need to be updated during initialization */
	UPDATE_PERIODIC, 	/* registers that needs periodic I2C writes */
	UPDATE_ALL, 			/* all registers will be updated */
	UPDATE_INVALID
}camera_reg_update_t;

typedef enum camera_setting_e {
	RES_PREVIEW,
	RES_CAPTURE
}camera_setting_t;


typedef struct aml_camera_ctl_s {
    struct camera_info_s para;
    enum camera_light_mode_e awb_mode;
    enum camera_test_mode_e test_mode;
}aml_camera_ctl_t;


typedef struct aml_camera_i2c_fig_s{
    unsigned short   addr;
    unsigned char    val;
} aml_camera_i2c_fig_t;

typedef struct aml_camera_i2c_fig0_s{
    unsigned short   addr;
    unsigned short    val;
} aml_camera_i2c_fig0_t;

typedef struct aml_camera_i2c_fig1_s{
    unsigned char   addr;
    unsigned char    val;
} aml_camera_i2c_fig1_t;

#ifdef CONFIG_CAMERA_OV5640
FUNCRION_SCOPE int  ov5640_read(char *buf, int len);
FUNCRION_SCOPE int  ov5640_write(char *buf, int len);
FUNCRION_SCOPE int  ov5640_write_byte(unsigned short addr, unsigned char data);
FUNCRION_SCOPE int  ov5640_write_word(unsigned short addr, unsigned short data);
FUNCRION_SCOPE unsigned short ov5640_read_word(unsigned short addr);
FUNCRION_SCOPE unsigned char  ov5640_read_byte(unsigned short addr );
FUNCRION_SCOPE struct aml_camera_ctl_s amlogic_camera_info_ov5640;
#endif

#ifdef CONFIG_CAMERA_GC0308
FUNCRION_SCOPE int  gc0308_write(char *buf, int len);
FUNCRION_SCOPE int  gc0308_write_byte(unsigned short addr, unsigned char data);
FUNCRION_SCOPE int  gc0308_read(char *buf, int len);
FUNCRION_SCOPE struct aml_camera_ctl_s amlogic_camera_info_gc0308;
#endif

#ifdef CONFIG_CAMERA_GT2005
FUNCRION_SCOPE int  gt2005_write(char *buf, int len);
FUNCRION_SCOPE int  gt2005_write_byte(unsigned short addr, unsigned char data);
FUNCRION_SCOPE int  gt2005_read(char *buf, int len);
FUNCRION_SCOPE unsigned char gt2005_read_byte(unsigned short addr);
FUNCRION_SCOPE struct aml_camera_ctl_s amlogic_camera_info_gt2005;
#endif
struct amlogic_camera_platform_data {
	const char *	name;				/* 2007. */
	int (*first_init)(void);
	int (*second_init)(void);
};

#endif /* #define _AMLOGIC_CAMERA_H */
