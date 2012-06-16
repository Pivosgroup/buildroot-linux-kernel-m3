/*
 * TVIN attribute debug
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __TVIN_DEBUG_H
#define __TVIN_DEBUG_H

/* Standard Linux Headers */
#include <linux/device.h>
#include <linux/types.h>

extern ssize_t vdin_dbg_store(struct device *dev,
    struct device_attribute *attr, const char * buf, size_t count);

#endif //__TVIN_DEBUG_H

