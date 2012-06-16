/*
 * VDIN driver
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *         Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __TVIN_VDIN_H
#define __TVIN_VDIN_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/irqreturn.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>

/* Amlogic Headers */
#include <linux/amports/vframe.h>
#include <linux/tvin/tvin.h>


/* values of vdin_dev_t.flags */
#define VDIN_FLAG_NULL          0x00000000
#define VDIN_FLAG_DEC_STARTED   0x00000001


//*** vdin device structure
typedef struct tvin_dec_ops_s {
    int (*dec_run) (struct vframe_s *vf);
} tvin_dec_ops_t;

typedef struct vdin_dev_s {
    int                         index;
    unsigned int                flags;
    unsigned int                mem_start;
    unsigned int                mem_size;
    unsigned int                irq;
    unsigned int                addr_offset;  //address offset(vdin0/vdin1/...)
    unsigned int                meas_th;
    unsigned int                meas_tv;
    unsigned long               pre_irq_time;

    struct tvin_parm_s          para;

    struct cdev                 cdev;
    irqreturn_t (*vdin_isr) (int irq, void *dev_id);
    struct timer_list           timer;

    spinlock_t                  dec_lock;
    struct tvin_dec_ops_s       *dec_ops;
    spinlock_t                  isr_lock;
    struct tasklet_struct       isr_tasklet;

    struct mutex                mm_lock; /* lock for mmap */
} vdin_dev_t;


typedef struct vdin_regs_s {
    unsigned int val  : 32;
    unsigned int reg  : 14;
    unsigned int port :  2; // port port_addr            port_data            remark
                        // 0    NA                   NA                   direct access
                        // 1    VPP_CHROMA_ADDR_PORT VPP_CHROMA_DATA_PORT CM port registers
                        // 2    NA                   NA                   reserved
                        // 3    NA                   NA                   reserved
    unsigned int bit  :  5;
    unsigned int wid  :  5;
    unsigned int mode :  1; // 0:read, 1:write
    unsigned int rsv  :  5;
} vdin_regs_t;

extern void tvin_dec_register(struct vdin_dev_s *devp, struct tvin_dec_ops_s *op);
extern void tvin_dec_unregister(struct vdin_dev_s *devp);
extern void vdin_info_update(struct vdin_dev_s *devp, struct tvin_parm_s *para);



#endif // __TVIN_VDIN_H

