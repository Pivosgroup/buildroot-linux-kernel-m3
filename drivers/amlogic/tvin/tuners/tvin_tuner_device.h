/*
 * TVIN Tuner Bridge Driver
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __TVIN_TUNER_DEVICE_H
#define __TVIN_TUNER_DEVICE_H

#include <linux/module.h>

#include <linux/tvin/tvin.h>

typedef struct tvin_tuner_ops_s {
    void (*get_std) (tuner_std_id *ptstd);
    void (*set_std) (tuner_std_id tstd);
    void (*get_freq) (struct tuner_freq_s *ptf);
    void (*set_freq) (struct tuner_freq_s tf);
    void (*get_parm) (struct tuner_parm_s *ptp);
}tvin_tuner_ops_t;

typedef struct tvin_tuner_device_s {
    /* device name */
    char name[32];
    /* module owner */
    struct module *owner;
    /* tuner ops */
    struct tvin_tuner_ops_s tops;
}tvin_tuner_device_t;

int tvin_register_tuner(struct tvin_tuner_device_s *tdev);
void tvin_unregister_tuner(struct tvin_tuner_device_s *tdev);

#endif //__TVIN_TUNER_DEVICE_H


