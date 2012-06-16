/*
 * TVIN Notifier
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TVIN_NOTIFIER_H
#define __TVIN_NOTIFIER_H

#include <linux/notifier.h>


/*
* source dec operations notifier
*/
#define TVIN_EVENT_DEC_START        0x00010000
#define TVIN_EVENT_DEC_STOP         0x00020000
#define TVIN_EVENT_INFO_CHECK       0x00040000

extern int tvin_dec_notifier_register(struct notifier_block *nb);
extern int tvin_dec_notifier_unregister(struct notifier_block *nb);
extern int tvin_dec_notifier_call(unsigned long, void *);

extern int tvin_check_notifier_register(struct notifier_block *nb);
extern int tvin_check_notifier_unregister(struct notifier_block *nb);
extern int tvin_check_notifier_call(unsigned long, void *);

#endif


