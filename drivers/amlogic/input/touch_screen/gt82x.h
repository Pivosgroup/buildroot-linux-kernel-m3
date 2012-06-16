/*
 * 
 * Copyright (C) 2011 Goodix, Inc.
 * 
 * Author: Scott
 * Date: 2012.01.05
 */

#ifndef _LINUX_GOODIX_TOUCH_H
#define _LINUX_GOODIX_TOUCH_H

#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>

#define fail    0
#define success 1

#define false   0
#define true    1


#if 0
#define DEBUG(fmt, arg...) printk("<--GT-DEBUG-->"fmt, ##arg)
#else
#define DEBUG(fmt, arg...)
#endif

#if 1
#define NOTICE(fmt, arg...) printk("<--GT-NOTICE-->"fmt, ##arg)
#else
#define NOTICE(fmt, arg...)
#endif

#if 1
#define WARNING(fmt, arg...) printk("<--GT-WARNING-->"fmt, ##arg)
#else
#define WARNING(fmt, arg...)
#endif

#if 0
#define DEBUG_MSG(fmt, arg...) printk("<--GT msg-->"fmt, ##arg)
#else
#define DEBUG_MSG(fmt, arg...)
#endif

#if 0
#define DEBUG_UPDATE(fmt, arg...) printk("<--GT update-->"fmt, ##arg)
#else
#define DEBUG_UPDATE(fmt, arg...)
#endif 

#if 0
#define DEBUG_COOR(fmt, arg...) printk(fmt, ##arg)
#define DEBUG_COORD
#else
#define DEBUG_COOR(fmt, arg...)
#endif

#if 0
#define DEBUG_ARRAY(array, num)   do{\
                                   int i;\
                                   u8* a = array;\
                                   for (i = 0; i < (num); i++)\
                                   {\
                                       printk("%02x   ", (a)[i]);\
                                       if ((i + 1 ) %10 == 0)\
                                       {\
                                           printk("\n");\
                                       }\
                                   }\
                                   printk("\n");\
                                  }while(0)
#else
#define DEBUG_ARRAY(array, num) 
#endif 

#define ADDR_MAX_LENGTH     2
#define ADDR_LENGTH         ADDR_MAX_LENGTH

#define CREATE_WR_NODE
//#define AUTO_UPDATE_GUITAR             //如果定义了则上电会自动判断是否需要升级

//--------------------------For user redefine-----------------------------//
//-------------------------GPIO REDEFINE START----------------------------//
#define GPIO_DIRECTION_INPUT(port)          gpio_direction_input(port)
#define GPIO_DIRECTION_OUTPUT(port, val)    gpio_direction_output(port, val)
#define GPIO_SET_VALUE(port, val)           gpio_set_value(port, val)
#define GPIO_FREE(port)                     gpio_free(port)
#define GPIO_REQUEST(port, name)            gpio_request(port, name)
#define GPIO_PULL_UPDOWN(port, val)         //s3c_gpio_setpull(port, val)
#define GPIO_CFG_PIN(port, irq, cfg)        gpio_enable_irq(port, irq, cfg)
//-------------------------GPIO REDEFINE END------------------------------//


//*************************TouchScreen Work Part Start**************************
#define TOUCH_PWR_EN				((GPIOA_bank_bit0_27(12)<<16) |GPIOA_bit_bit0_27(12))  
#define RESET_PORT          ((GPIOA_bank_bit0_27(1)<<16) |GPIOA_bit_bit0_27(1))         //RESET管脚号
#define INT_PORT            ((GPIOA_bank_bit0_27(16)<<16) |GPIOA_bit_bit0_27(16))         //Int IO port
#ifdef INT_PORT
    #define TS_INT          INT_GPIO_0      //Interrupt Number,EINT18(119)
    #define INT_CFG         IRQF_TRIGGER_FALLING            //IO configer as EINT
#else
    #define TS_INT          0
#endif

#define INT_TRIGGER         IRQ_TYPE_EDGE_FALLING       
#define POLL_TIME           10        //actual query spacing interval:POLL_TIME+6

#define GOODIX_MULTI_TOUCH
#ifdef GOODIX_MULTI_TOUCH
    #define MAX_FINGER_NUM    5
#else
    #define MAX_FINGER_NUM    1
#endif

#define swap(x, y) do { typeof(x) z = x; x = y; y = z; } while (0)

struct goodix_ts_data {
    u8 bad_data;
    u8 irq_is_disable;
    u16 addr;
    s32 use_reset;        //use RESET flag
    s32 use_irq;          //use EINT flag
    u32 version;
    s32 (*power)(struct goodix_ts_data * ts, s32 on);
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct hrtimer timer;
    struct work_struct work;
    struct early_suspend early_suspend;
    s8 phys[32];
};
//*************************TouchScreen Work Part End****************************

#endif /* _LINUX_GOODIX_TOUCH_H */
