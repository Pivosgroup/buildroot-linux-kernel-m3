/*
 * Video Frame Manager For Provider and Receiver
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

#ifndef __AML_VFM_H
#define __AML_VFM_H

char* vf_get_provider_name(const char* receiver_name);

char* vf_get_receiver_name(const char* provider_name);

bool is_provider_active(const char* provider_name);

bool is_receiver_active(const char* receiver_name);

void provider_list(void);

extern int vfm_mode;

#endif

