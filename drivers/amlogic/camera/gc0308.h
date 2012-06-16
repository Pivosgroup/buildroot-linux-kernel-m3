/*
 * Copyright (C) 2008-2009 QUALCOMM Incorporated.
 */

#ifndef _CAMERA_GC0308_H
#define _CAMERA_GC0308_H

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/tvin/tvin.h>


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


typedef struct GC0308_ctl_s {
    struct camera_info_s para;
    enum camera_light_mode_e awb_mode;
    enum camera_test_mode_e test_mode;
}GC0308_ctl_t;


typedef struct GC0308_i2c_fig_s{
    unsigned char   addr;
    unsigned char    val;
} GC0308_i2c_fig_t;

typedef struct GC0308_i2c_fig0_s{
    unsigned char   addr;
    unsigned char    val;
} GC0308_i2c_fig0_t;


#endif /* #define _CAMERA_GC0308_H */
