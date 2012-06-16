/*******************************************************************
 * 
 *  Copyright C 2005 by Amlogic, Inc. All Rights Reserved.
 *
 *  Description: 
 *
 *  Author: Eric Zhang
 *  Created: 2/20/2006
 *
 *******************************************************************/
#ifndef CARD__READER_H
#define CARD__READER_H

#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/list.h>
#include <mach/card_io.h>
/**
 * @file cardreader.h
 * @addtogroup Card
 */
/*@{*/

#define CARD_INSERTED           1
#define CARD_REMOVED            0

#define CARD_REGISTERED         1
#define CARD_UNREGISTERED       0

#define CARD_UNIT_NOT_READY     0
#define CARD_UNIT_PROCESSING    1
#define CARD_UNIT_PROCESSED     2
#define CARD_UNIT_READY         3
#define CARD_UNIT_RESUMED         4

#define CARD_EVENT_NOT_INSERTED     0
#define CARD_EVENT_INSERTED     	1
#define CARD_EVENT_POST_INSERTED    2
#define CARD_EVENT_NOT_REMOVED      0
#define CARD_EVENT_REMOVED     		1

typedef enum {
	CARD_XD_PICTURE = 0,
	CARD_MEMORY_STICK,
    CARD_SECURE_DIGITAL,    
    CARD_SDIO,
    CARD_INAND,
    CARD_COMPACT_FLASH,   
    CARD_TYPE_UNKNOW
} CARD_TYPE_t;

#define CARD_MAX_UNIT           (CARD_TYPE_UNKNOW+1)

#define CARD_STRING_LEN			   13
#define CARD_MS_NAME_STR           "ms"
#define CARD_SD_NAME_STR           "sd"
#define CARD_INAND_NAME_STR        "inand"
#define CARD_SDIO_NAME_STR         "sdio"
#define CARD_XD_NAME_STR           "xd"
#define CARD_CF_NAME_STR           "cf"
#define CARD_UNKNOW_NAME_STR       "xx"
#define CARD_5IN1CARD_NAME_STR     "5in1"

//extern wait_queue_head_t     sdio_wait_event;

extern struct completion sdio_int_complete;
extern void sdio_open_host_interrupt(unsigned int_resource);
extern void sdio_clear_host_interrupt(unsigned int_resource);
extern unsigned sdio_check_interrupt(void);
extern void sdio_close_host_interrupt(unsigned int_resource);
/*@}*/
#endif // CARD__READER_H
