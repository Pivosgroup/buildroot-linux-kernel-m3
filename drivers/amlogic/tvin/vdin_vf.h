/*
 * VDIN vframe support
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __AML_TVIN_VDIN_VF_H
#define __AML_TVIN_VDIN_VF_H

#include <linux/amports/vframe.h>
#include "tvin_global.h"



typedef struct vfq_s{
	struct vframe_s *pool[BT656IN_VF_POOL_SIZE];
	u32	rd_index;
	u32 wr_index;
} vfq_t;

bool vfq_empty(vfq_t *q);
bool vfq_empty_newframe(void);
bool vfq_empty_display(void);
bool vfq_empty_recycle(void);

void vfq_push(vfq_t *q, vframe_t *vf);
void vfq_push_newframe(vframe_t *vf);
void vfq_push_display(vframe_t *vf);
void vfq_push_recycle(vframe_t *vf);

vframe_t *vfq_pop(vfq_t *q);
vframe_t *vfq_pop_newframe(void);
vframe_t *vfq_pop_display(void);
vframe_t *vfq_pop_recycle(void);
void vdin_vf_init(void);


void vdin_reg_vf_provider(void);
void vdin_unreg_vf_provider(void);
void vdin_notify_receiver(int type, void* data, void* private_data);
#endif

