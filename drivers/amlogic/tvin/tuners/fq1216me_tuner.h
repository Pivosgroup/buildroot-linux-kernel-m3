/*
 * PHILIPS FQ1216ME-MK3 Tuner Device Driver
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


#ifndef __TVIN_TUNER_FQ1216ME_TUNER_H
#define __TVIN_TUNER_FQ1216ME_TUNER_H


/* Standard Liniux Headers */
#include <linux/i2c.h>

/* Amlogic Headers */
#include <linux/tvin/tvin.h>

extern int fq1216me_set_tuner(void);
extern void fq1216me_get_tuner(struct tuner_parm_s *ptp);
extern void fq1216me_get_std(tuner_std_id *ptstd);

extern void fq1216me_set_tuner_std(tuner_std_id ptstd);

extern void fq1216me_get_freq(struct tuner_freq_s *ptf);

extern void fq1216me_set_freq(struct tuner_freq_s tf);







#endif //__TVIN_TUNER_FQ1216ME_TUNER_H


