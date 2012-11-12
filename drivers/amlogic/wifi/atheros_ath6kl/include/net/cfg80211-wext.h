#ifndef __NET_CFG80211_WEXT_H
#define __NET_CFG80211_WEXT_H
/*
 * 802.11 device and configuration interface -- wext handlers
 *
 * Copyright 2006-2010	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>

/*
 * Temporary wext handlers & helper functions
 *
 * These are used only by drivers that aren't yet fully
 * converted to cfg80211.
 */
int cfg80211_wext_giwname_ath6kl(struct net_device *dev,
			  struct iw_request_info *info,
			  char *name, char *extra);
int cfg80211_wext_siwmode_ath6kl(struct net_device *dev, struct iw_request_info *info,
			  u32 *mode, char *extra);
int cfg80211_wext_giwmode_ath6kl(struct net_device *dev, struct iw_request_info *info,
			  u32 *mode, char *extra);
int cfg80211_wext_siwscan_ath6kl(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra);
int cfg80211_wext_giwscan_ath6kl(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *data, char *extra);
int cfg80211_wext_giwrange_ath6kl(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *data, char *extra);
int cfg80211_wext_siwrts_ath6kl(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *rts, char *extra);
int cfg80211_wext_giwrts_ath6kl(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *rts, char *extra);
int cfg80211_wext_siwfrag_ath6kl(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *frag, char *extra);
int cfg80211_wext_giwfrag_ath6kl(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *frag, char *extra);
int cfg80211_wext_giwretry_ath6kl(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_param *retry, char *extra);

#endif /* __NET_CFG80211_WEXT_H */
