/* plat/wifi_tiwlan.h
 *
 * This file contains the WLAN CHIP specific data.
 *
 * Copyright (C) 2009 Texas Instruments.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_WIFI_TIWLAN_H_
#define _LINUX_WIFI_TIWLAN_H_

struct wifi_platform_data {
	int (*set_power) (int val);
	int (*set_reset) (int val);
	int (*set_carddetect) (int val);
	void *(*mem_prealloc) (int section, unsigned long size);
};

#endif
