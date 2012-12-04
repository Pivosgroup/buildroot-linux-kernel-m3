/*
 * Linux cfg80211 driver
 *
 * Copyright (C) 1999-2011, Broadcom Corporation
 * 
 *         Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: wl_cfg80211.c,v 1.1.4.1.2.14 2011/02/09 01:40:07 Exp $
 */

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <linux/kernel.h>

#include <bcmutils.h>
#include <bcmwifi.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/802.11.h>
#include <linux/if_arp.h>
#include <asm/uaccess.h>

#include <dngl_stats.h>
#include <dhd.h>
#include <dhdioctl.h>
#include <wlioctl.h>
#include <dhd_cfg80211.h>

#include <proto/ethernet.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <linux/wait.h>
#include <net/cfg80211.h>
#include <net/rtnetlink.h>

#include <wlioctl.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>
#include <wl_cfgp2p.h>

void wl_cfg80211_enable_trace(int level)
{
}

s32 wl_cfg80211_down(void *para)
{
	s32 err = 0;
	return err;
}

int wl_cfg80211_hang(struct net_device *dev, u16 reason)
{
	return 0;
}

s32 wl_cfg80211_set_p2p_noa(struct net_device *net, char* buf, int len)
{
	return 0;
}

void wl_cfg80211_detach(void *para)
{
}

s32 wl_cfg80211_set_p2p_ps(struct net_device *net, char* buf, int len)
{
	return 0;
}

int dhd_monitor_init(void *dhd_pub)
{
	return 0;
}

void wl_cfg80211_set_parent_dev(void *dev)
{
}

int wl_cfg80211_set_btcoex_dhcp(struct net_device *dev, char *command)
{
	snprintf(command, 3, "OK");
	return (strlen("OK"));
}

int dhd_monitor_uninit(void)
{
	return 0;
}

s32
wl_cfg80211_is_progress_ifchange(void)
{
	return 0;
}

int wl_cfg80211_get_p2p_dev_addr(struct net_device *net, struct ether_addr *p2pdev_addr)
{
	return 0;
}

s32
wl_cfg80211_notify_ifadd(struct net_device *ndev, s32 idx, s32 bssidx,
	void* _net_attach)
{
	return 0;
}

s32 wl_cfg80211_up(void *para)
{
	return 0;
}

s32
wl_cfg80211_is_progress_ifadd(void)
{
	return 0;
}

s32
wl_cfg80211_attach(struct net_device *ndev, void *data)
{
	return 0;
}

s32
wl_cfg80211_notify_ifchange(void)
{
	return 0;
}

s32 wl_cfg80211_set_wps_p2p_ie(struct net_device *net, char *buf, int len,
	enum wl_management_type type)
{
	return 0;
}

void
wl_cfg80211_event(struct net_device *ndev, const wl_event_msg_t * e, void *data)
{
}

s32
wl_cfg80211_notify_ifdel(struct net_device *ndev)
{
	return 0;
}
