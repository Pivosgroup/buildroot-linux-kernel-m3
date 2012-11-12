/*
 * Copyright (c) 2004-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "core.h"
#include "hif-ops.h"
#include "cfg80211.h"
#include "target.h"
#include "debug.h"

struct ath6kl_sta *ath6kl_find_sta(struct ath6kl_vif *vif, u8 *node_addr)
{
	struct ath6kl_sta *conn = NULL;
	u8 i, max_conn;

	if (vif->nw_type != AP_NETWORK)
		return &vif->sta_list[0];

	max_conn = (vif->nw_type == AP_NETWORK) ? AP_MAX_NUM_STA : 0;

	for (i = 0; i < max_conn; i++) {
		if (memcmp(node_addr, vif->sta_list[i].mac, ETH_ALEN) == 0) {
			conn = &vif->sta_list[i];
			break;
		}
	}

	return conn;
}

struct ath6kl_sta *ath6kl_find_sta_by_aid(struct ath6kl_vif *vif, u8 aid)
{
	struct ath6kl_sta *conn = NULL;
	u8 ctr;

	if (vif->nw_type != AP_NETWORK) {
		conn = &vif->sta_list[0];
	} else {
		for (ctr = 0; ctr < AP_MAX_NUM_STA; ctr++) {
			if (vif->sta_list[ctr].aid == aid) {
				conn = &vif->sta_list[ctr];
				break;
			}
		}
	}

	return conn;
}

static void ath6kl_add_new_sta(struct ath6kl_vif *vif, u8 *mac, u16 aid, u8 *wpaie,
			u8 ielen, u8 keymgmt, u8 ucipher, u8 auth, u8 apsd_info)
{
	struct ath6kl_sta *sta;
	u8 free_slot;

	free_slot = aid - 1;

	sta = &vif->sta_list[free_slot];
	spin_lock_bh(&sta->lock);
	memcpy(sta->mac, mac, ETH_ALEN);
	if (ielen <= ATH6KL_MAX_IE)
		memcpy(sta->wpa_ie, wpaie, ielen);
	sta->aid = aid;
	sta->keymgmt = keymgmt;
	sta->ucipher = ucipher;
	sta->auth = auth;
	sta->apsd_info = apsd_info;
	aggr_reset_state(sta->aggr_conn_cntxt);
	sta->vif = vif;
	init_timer(&sta->psq_age_timer);
	sta->psq_age_timer.function = ath6kl_psq_age_handler;
	sta->psq_age_timer.data = (unsigned long)sta;

	vif->sta_list_index = vif->sta_list_index | (1 << free_slot);
	vif->ap_stats.sta[free_slot].aid = cpu_to_le32(aid);
	spin_unlock_bh(&sta->lock);
}

static void ath6kl_sta_cleanup(struct ath6kl_vif *vif, u8 i)
{
	struct ath6kl_sta *sta = &vif->sta_list[i];

	del_timer_sync(&sta->psq_age_timer);
	spin_lock_bh(&sta->lock);
	sta->vif = NULL;
	/* empty the queued pkts in the PS queue if any */
	skb_queue_purge(&sta->psq);
	ath6kl_mgmt_queue_purge(&sta->mgmt_psq);

	memset(&vif->ap_stats.sta[sta->aid - 1], 0,
	       sizeof(struct wmi_per_sta_stat));
	memset(sta->mac, 0, ETH_ALEN);
	memset(sta->wpa_ie, 0, ATH6KL_MAX_IE);
	sta->aid = 0;
	sta->sta_flags = 0;
	aggr_reset_state(sta->aggr_conn_cntxt);

	vif->sta_list_index = vif->sta_list_index & ~(1 << i);
	spin_unlock_bh(&sta->lock);
}

static u8 ath6kl_remove_sta(struct ath6kl_vif *vif, u8 *mac, u16 reason)
{
	u8 i, removed = 0;

	if (is_zero_ether_addr(mac))
		return removed;

	if (is_broadcast_ether_addr(mac)) {
		ath6kl_dbg(ATH6KL_DBG_TRC, "deleting all station\n");

		for (i = 0; i < AP_MAX_NUM_STA; i++) {
			if (!is_zero_ether_addr(vif->sta_list[i].mac)) {
				ath6kl_sta_cleanup(vif, i);
				removed = 1;
			}
		}
	} else {
		for (i = 0; i < AP_MAX_NUM_STA; i++) {
			if (memcmp(vif->sta_list[i].mac, mac, ETH_ALEN) == 0) {
				ath6kl_dbg(ATH6KL_DBG_TRC,
					   "deleting station %pM aid=%d reason=%d\n",
					   mac, vif->sta_list[i].aid, reason);
				ath6kl_sta_cleanup(vif, i);
				removed = 1;
				break;
			}
		}
	}

	return removed;
}

void ath6kl_mgmt_queue_init(struct mgmt_buff_head* psq)
{
	INIT_LIST_HEAD(&psq->list);
	psq->len = 0;
	spin_lock_init(&psq->lock);
}

void ath6kl_mgmt_queue_purge(struct mgmt_buff_head* psq)
{
	unsigned long flags;
	struct mgmt_buff* mgmt_buf, *mgmt_n;

	spin_lock_irqsave(&psq->lock, flags);
	if (psq->len <= 0) {
		spin_unlock_irqrestore(&psq->lock, flags);
		return;
	}
	list_for_each_entry_safe(mgmt_buf, mgmt_n, &psq->list, list) {
		list_del(&mgmt_buf->list);
		kfree(mgmt_buf);
	}
	psq->len = 0;
	spin_unlock_irqrestore(&psq->lock, flags);
}

int ath6kl_mgmt_queue_empty(struct mgmt_buff_head* psq)
{
	unsigned long flags;
	int tmp;

	spin_lock_irqsave(&psq->lock, flags);
	tmp = (psq->len == 0);
	spin_unlock_irqrestore(&psq->lock, flags);

	return tmp;
}

int ath6kl_mgmt_queue_tail(struct mgmt_buff_head* psq, const u8* buf, u16 len, u32 id, u32 freq, u32 wait)
{
	unsigned long flags;
	struct mgmt_buff* mgmt_buf;
	int mgmt_buf_size;


	mgmt_buf_size = len + sizeof(struct mgmt_buff) - 1;
	mgmt_buf = kmalloc(mgmt_buf_size, GFP_ATOMIC);
	if (!mgmt_buf) {
		return -ENOMEM;
	}
	
	INIT_LIST_HEAD(&mgmt_buf->list);
	mgmt_buf->id = id;
	mgmt_buf->freq = freq;
	mgmt_buf->wait = wait;
	mgmt_buf->len = len;
	mgmt_buf->age = 0;
	memcpy(mgmt_buf->buf, buf, len);
	
	spin_lock_irqsave(&psq->lock, flags);
	list_add_tail(&mgmt_buf->list, &psq->list);
	psq->len++;
	spin_unlock_irqrestore(&psq->lock, flags);

	return 0;
}

struct mgmt_buff* ath6kl_mgmt_dequeue_head(struct mgmt_buff_head* psq)
{
	unsigned long flags;
	struct mgmt_buff* mgmt_buf;
	
	spin_lock_irqsave(&psq->lock, flags);
	if (psq->len == 0) {
		spin_unlock_irqrestore(&psq->lock, flags);
		return NULL;
	}
	
	mgmt_buf = list_first_entry(&psq->list, struct mgmt_buff, list);
	list_del(&mgmt_buf->list);
	psq->len--;
	spin_unlock_irqrestore(&psq->lock, flags);

	return mgmt_buf;
}

static void ath6kl_data_psq_aging(struct ath6kl_sta* sta)
{
	struct sk_buff* skb;
	struct sk_buff* skb_n = NULL;
	u32 age;
	bool last = false;

	if (skb_queue_empty(&sta->psq)) {
		return;
	}

	skb = skb_peek(&sta->psq);
	do {
		if (skb_queue_is_last(&sta->psq, skb)) {
			last = true;
		} else {
			skb_n = skb_queue_next(&sta->psq, skb);
		}

		age = ath6kl_sk_buff_get_age(skb);
		age++;
		if (age >= ATH6KL_PSQ_MAX_AGE) {
			skb_unlink(skb, &sta->psq);
			kfree_skb(skb);
		} else {
			ath6kl_sk_buff_set_age(skb, age);
		}

		if (last) {
			break;
		} else {
			skb = skb_n;		
		}
	} while (1);

	if (ath6kl_mgmt_queue_empty(&sta->mgmt_psq) && skb_queue_empty(&sta->psq)) {
		ath6kl_wmi_set_pvb_cmd(sta->vif, sta->aid, 0);
	}
}

static void ath6kl_mgmt_psq_aging(struct ath6kl_sta* sta)
{
	struct mgmt_buff* mgmt_buf, *mgmt_n;
	unsigned long flags;
	u32 age;	
	int is_empty;

	spin_lock_irqsave(&sta->mgmt_psq.lock, flags);
	if (sta->mgmt_psq.len == 0) {
		spin_unlock_irqrestore(&sta->mgmt_psq.lock, flags);
		return;
	}
	list_for_each_entry_safe(mgmt_buf, mgmt_n, &sta->mgmt_psq.list, list) {
		age = ath6kl_mgmt_buff_get_age(mgmt_buf);
		age++;
		if (age >= ATH6KL_PSQ_MAX_AGE) {
			list_del(&mgmt_buf->list);
			kfree(mgmt_buf);
			sta->mgmt_psq.len--;
		} else {
			ath6kl_mgmt_buff_set_age(mgmt_buf, age);
		}
	}
	is_empty = (sta->mgmt_psq.len == 0) && skb_queue_empty(&sta->psq);
	spin_unlock_irqrestore(&sta->mgmt_psq.lock, flags);

	if (is_empty) {
		ath6kl_wmi_set_pvb_cmd(sta->vif, sta->aid, 0);
	}
}


void ath6kl_psq_age_handler(unsigned long ptr)
{
	struct ath6kl_sta* sta = (struct ath6kl_sta*)ptr;

	spin_lock_bh(&sta->lock);
	ath6kl_data_psq_aging(sta);
	ath6kl_mgmt_psq_aging(sta);
	spin_unlock_bh(&sta->lock);
	mod_timer(&sta->psq_age_timer, jiffies + msecs_to_jiffies(1000));
}

void ath6kl_psq_age_start(struct ath6kl_sta* sta)
{
	mod_timer(&sta->psq_age_timer, jiffies + msecs_to_jiffies(1000));
}

void ath6kl_psq_age_stop(struct ath6kl_sta* sta)
{
	del_timer_sync(&sta->psq_age_timer);
}

enum htc_endpoint_id ath6kl_ac2_endpoint_id(void *devt, u8 ac)
{
	struct ath6kl *ar = devt;
	return ar->ac2ep_map[ac];
}

struct ath6kl_cookie *ath6kl_alloc_cookie(struct ath6kl *ar)
{
	struct ath6kl_cookie *cookie;

	cookie = ar->cookie_list;
	if (cookie != NULL) {
		ar->cookie_list = cookie->arc_list_next;
		ar->cookie_count--;
	}

	return cookie;
}

void ath6kl_cookie_init(struct ath6kl *ar)
{
	u32 i;

	ar->cookie_list = NULL;
	ar->cookie_count = 0;

	memset(ar->cookie_mem, 0, sizeof(ar->cookie_mem));

	for (i = 0; i < MAX_COOKIE_NUM; i++)
		ath6kl_free_cookie(ar, &ar->cookie_mem[i]);
}

void ath6kl_cookie_cleanup(struct ath6kl *ar)
{
	ar->cookie_list = NULL;
	ar->cookie_count = 0;
}

void ath6kl_free_cookie(struct ath6kl *ar, struct ath6kl_cookie *cookie)
{
	/* Insert first */

	if (!ar || !cookie)
		return;

	cookie->arc_list_next = ar->cookie_list;
	ar->cookie_list = cookie;
	ar->cookie_count++;
}

int ath6kl_read_fwlogs(struct ath6kl *ar)
{
	struct ath6kl_dbglog_hdr debug_hdr;
	struct ath6kl_dbglog_buf debug_buf;
	u32 address, length, dropped, firstbuf, debug_hdr_addr;
	int ret = 0, loop;
	u8 *buf;

	buf = kmalloc(ATH6KL_FWLOG_PAYLOAD_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	address = TARG_VTOP(ar->target_type,
			    ath6kl_get_hi_item_addr(ar,
						    HI_ITEM(hi_dbglog_hdr)));

	ret = ath6kl_diag_read32(ar, address, &debug_hdr_addr);
	if (ret)
		goto out;

	/* Get the contents of the ring buffer */
	if (debug_hdr_addr == 0) {
		ath6kl_warn("Invalid address for debug_hdr_addr\n");
		ret = -EINVAL;
		goto out;
	}

	address = TARG_VTOP(ar->target_type, debug_hdr_addr);
	ath6kl_diag_read(ar, address, &debug_hdr, sizeof(debug_hdr));

	address = TARG_VTOP(ar->target_type,
			    le32_to_cpu(debug_hdr.dbuf_addr));
	firstbuf = address;
	dropped = le32_to_cpu(debug_hdr.dropped);
	ath6kl_diag_read(ar, address, &debug_buf, sizeof(debug_buf));

	loop = 100;

	do {
		address = TARG_VTOP(ar->target_type,
				    le32_to_cpu(debug_buf.buffer_addr));
		length = le32_to_cpu(debug_buf.length);

		if (length != 0 && (le32_to_cpu(debug_buf.length) <=
				    le32_to_cpu(debug_buf.bufsize))) {
			length = ALIGN(length, 4);

			ret = ath6kl_diag_read(ar, address,
					       buf, length);
			if (ret)
				goto out;

			ath6kl_debug_fwlog_event(ar, buf, length);
		}

		address = TARG_VTOP(ar->target_type,
				    le32_to_cpu(debug_buf.next));
		ath6kl_diag_read(ar, address, &debug_buf, sizeof(debug_buf));
		if (ret)
			goto out;

		loop--;

		if (WARN_ON(loop == 0)) {
			ret = -ETIMEDOUT;
			goto out;
		}
	} while (address != firstbuf);

out:
	kfree(buf);

	return ret;
}

int ath6kl_diag_read(struct ath6kl *ar, u32 address, void *data, u32 length)
{
	u32 count, *buf = data;
	int ret;

	if (WARN_ON(length % 4))
		return -EINVAL;

	for (count = 0; count < length / 4; count++, address += 4) {
		ret = ath6kl_diag_read32(ar, address, &buf[count]);
		if (ret)
			return ret;
	}

	return 0;
}

int ath6kl_diag_write(struct ath6kl *ar, u32 address, void *data, u32 length)
{
	u32 count;
	__le32 *buf = data;
	int ret;

	if (WARN_ON(length % 4))
		return -EINVAL;

	for (count = 0; count < length / 4; count++, address += 4) {
		ret = ath6kl_diag_write32(ar, address, buf[count]);
		if (ret)
			return ret;
	}

	return 0;
}

void ath6kl_reset_device(struct ath6kl *ar, u32 target_type,
				bool wait_fot_compltn, bool cold_reset)
{
	int status = 0;
	u32 address;
	__le32 data;

	if (target_type != TARGET_TYPE_AR6003 &&
		target_type != TARGET_TYPE_AR6004 &&
		target_type != TARGET_TYPE_AR6006)
		return;

	data = cold_reset ? RESET_CONTROL_COLD_RST : RESET_CONTROL_MBOX_RST;

	address = RTC_BASE_ADDRESS;
	status = ath6kl_diag_write32(ar, address, data);

	if (status)
		ath6kl_err("failed to reset target\n");
}

void ath6kl_stop_endpoint(struct net_device *dev, bool keep_profile,
			  bool get_dbglogs)
{
	struct ath6kl_vif *vif = ath6kl_priv(dev);
	static u8 bcast_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	bool discon_issued;

	netif_stop_queue(dev);

	/* disable the target and the interrupts associated with it */
	discon_issued = (test_bit(CONNECTED, &vif->flag) ||
			 test_bit(CONNECT_PEND, &vif->flag));
	ath6kl_disconnect(vif);
	if (!keep_profile)
		ath6kl_init_profile_info(vif);

	del_timer(&vif->disconnect_timer);

	/*
	 * After wmi_shudown all WMI events will be dropped. We
	 * need to cleanup the buffers allocated in AP mode and
	 * give disconnect notification to stack, which usually
	 * happens in the disconnect_event. Simulate the disconnect
	 * event by calling the function directly. Sometimes
	 * disconnect_event will be received when the debug logs
	 * are collected.
	 */
	if (discon_issued)
		ath6kl_disconnect_event(vif, DISCONNECT_CMD,
					(vif->nw_type & AP_NETWORK) ?
					bcast_mac : vif->bssid,
					0, NULL, 0);

	vif->user_key_ctrl = 0;
}

static void ath6kl_install_static_wep_keys(struct ath6kl_vif *vif)
{
	u8 index;
	u8 keyusage;

	for (index = WMI_MIN_KEY_INDEX; index <= WMI_MAX_KEY_INDEX; index++) {
		if (vif->wep_key_list[index].key_len) {
			keyusage = GROUP_USAGE;
			if (index == vif->def_txkey_index)
				keyusage |= TX_USAGE;

			ath6kl_wmi_addkey_cmd(vif,
					      index,
					      WEP_CRYPT,
					      keyusage,
					      vif->wep_key_list[index].key_len,
					      NULL,
					      vif->wep_key_list[index].key,
					      KEY_OP_INIT_VAL, NULL,
					      NO_SYNC_WMIFLAG);
		}
	}
}

void ath6kl_connect_ap_mode_bss(struct ath6kl_vif *vif, u16 channel)
{
	struct ath6kl_req_key *ik;
	int res;
	u8 key_rsc[ATH6KL_KEY_SEQ_LEN];

	ik = &vif->ap_mode_bkey;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "AP mode started on %u MHz\n", channel);

	switch (vif->auth_mode) {
	case NONE_AUTH:
		if (vif->prwise_crypto == WEP_CRYPT)
			ath6kl_install_static_wep_keys(vif);
		break;
	case WPA_PSK_AUTH:
	case WPA2_PSK_AUTH:
	case (WPA_PSK_AUTH | WPA2_PSK_AUTH):
		if (!ik->valid)
			break;

		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "Delayed addkey for "
			   "the initial group key for AP mode\n");
		memset(key_rsc, 0, sizeof(key_rsc));
		res = ath6kl_wmi_addkey_cmd(
			vif, ik->key_index, ik->key_type,
			GROUP_USAGE, ik->key_len, key_rsc, ik->key,
			KEY_OP_INIT_VAL, NULL, SYNC_BOTH_WMIFLAG);
		if (res) {
			ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "Delayed "
				   "addkey failed: %d\n", res);
		}
		break;
	}


	ath6kl_wmi_bssfilter_cmd(vif, NONE_BSS_FILTER, 0);
	set_bit(CONNECTED, &vif->flag);
	netif_carrier_on(vif->net_dev);
}

void ath6kl_connect_ap_mode_sta(struct ath6kl_vif *vif, u16 aid, u8 *mac_addr,
				u8 keymgmt, u8 ucipher, u8 auth,
				u8 assoc_req_len, u8 *assoc_info, u8 apsd_info)
{
	u8 *ies = NULL, *wpa_ie = NULL, *pos;
	size_t ies_len = 0;
	struct station_info sinfo;

	ath6kl_dbg(ATH6KL_DBG_TRC, "new station %pM aid=%d\n", mac_addr, aid);

	if (assoc_req_len > sizeof(struct ieee80211_hdr_3addr)) {
		struct ieee80211_mgmt *mgmt =
			(struct ieee80211_mgmt *) assoc_info;
		if (ieee80211_is_assoc_req(mgmt->frame_control) &&
		    assoc_req_len >= sizeof(struct ieee80211_hdr_3addr) +
		    sizeof(mgmt->u.assoc_req)) {
			ies = mgmt->u.assoc_req.variable;
			ies_len = assoc_info + assoc_req_len - ies;
		} else if (ieee80211_is_reassoc_req(mgmt->frame_control) &&
			   assoc_req_len >= sizeof(struct ieee80211_hdr_3addr)
			   + sizeof(mgmt->u.reassoc_req)) {
			ies = mgmt->u.reassoc_req.variable;
			ies_len = assoc_info + assoc_req_len - ies;
		}
	}

	pos = ies;
	while (pos && pos + 1 < ies + ies_len) {
		if (pos + 2 + pos[1] > ies + ies_len)
			break;
		if (pos[0] == WLAN_EID_RSN)
			wpa_ie = pos; /* RSN IE */
		else if (pos[0] == WLAN_EID_VENDOR_SPECIFIC &&
			 pos[1] >= 4 &&
			 pos[2] == 0x00 && pos[3] == 0x50 && pos[4] == 0xf2) {
			if (pos[5] == 0x01)
				wpa_ie = pos; /* WPA IE */
			else if (pos[5] == 0x04) {
				wpa_ie = pos; /* WPS IE */
				break; /* overrides WPA/RSN IE */
			}
		}
		pos += 2 + pos[1];
	}

	ath6kl_add_new_sta(vif, mac_addr, aid, wpa_ie,
			   wpa_ie ? 2 + wpa_ie[1] : 0,
			   keymgmt, ucipher, auth, apsd_info);

	/* send event to application */
	memset(&sinfo, 0, sizeof(sinfo));

	/* TODO: sinfo.generation */

	sinfo.assoc_req_ies = ies;
	sinfo.assoc_req_ies_len = ies_len;
        sinfo.filled |= STATION_INFO_ASSOC_REQ_IES;

	cfg80211_new_sta_ath6kl(vif->net_dev, mac_addr, &sinfo, GFP_KERNEL);

	netif_wake_queue(vif->net_dev);
}

/* Functions for Tx credit handling */
void ath6k_credit_init(struct htc_credit_state_info *cred_info,
		       struct list_head *ep_list,
		       int tot_credits)
{
	struct htc_endpoint_credit_dist *cur_ep_dist;
	int count;

	cred_info->cur_free_credits = tot_credits;
	cred_info->total_avail_credits = tot_credits;

	list_for_each_entry(cur_ep_dist, ep_list, list) {
		if (cur_ep_dist->endpoint == ENDPOINT_0)
			continue;

		cur_ep_dist->cred_min = cur_ep_dist->cred_per_msg;

		if (tot_credits > 4)
			if ((cur_ep_dist->svc_id == WMI_DATA_BK_SVC) ||
			    (cur_ep_dist->svc_id == WMI_DATA_BE_SVC)) {
				ath6kl_deposit_credit_to_ep(cred_info,
						cur_ep_dist,
						cur_ep_dist->cred_min);
				cur_ep_dist->dist_flags |= HTC_EP_ACTIVE;
			}

		if (cur_ep_dist->svc_id == WMI_CONTROL_SVC) {
			ath6kl_deposit_credit_to_ep(cred_info, cur_ep_dist,
						    cur_ep_dist->cred_min);
			/*
			 * Control service is always marked active, it
			 * never goes inactive EVER.
			 */
			cur_ep_dist->dist_flags |= HTC_EP_ACTIVE;
		} else if (cur_ep_dist->svc_id == WMI_DATA_BK_SVC)
			/* this is the lowest priority data endpoint */
			cred_info->lowestpri_ep_dist = cur_ep_dist->list;

		/*
		 * Streams have to be created (explicit | implicit) for all
		 * kinds of traffic. BE endpoints are also inactive in the
		 * beginning. When BE traffic starts it creates implicit
		 * streams that redistributes credits.
		 *
		 * Note: all other endpoints have minimums set but are
		 * initially given NO credits. credits will be distributed
		 * as traffic activity demands
		 */
	}

	WARN_ON(cred_info->cur_free_credits <= 0);

	list_for_each_entry(cur_ep_dist, ep_list, list) {
		if (cur_ep_dist->endpoint == ENDPOINT_0)
			continue;

		if (cur_ep_dist->svc_id == WMI_CONTROL_SVC)
			cur_ep_dist->cred_norm = cur_ep_dist->cred_per_msg;
		else {
			/*
			 * For the remaining data endpoints, we assume that
			 * each cred_per_msg are the same. We use a simple
			 * calculation here, we take the remaining credits
			 * and determine how many max messages this can
			 * cover and then set each endpoint's normal value
			 * equal to 3/4 this amount.
			 */
			count = (cred_info->cur_free_credits /
				 cur_ep_dist->cred_per_msg)
				* cur_ep_dist->cred_per_msg;
			count = (count * 3) >> 2;
			count = max(count, cur_ep_dist->cred_per_msg);
			cur_ep_dist->cred_norm = count;

		}
	}
}

/* initialize and setup credit distribution */
int ath6k_setup_credit_dist(void *htc_handle,
			    struct htc_credit_state_info *cred_info)
{
	u16 servicepriority[5];

	memset(cred_info, 0, sizeof(struct htc_credit_state_info));

	servicepriority[0] = WMI_CONTROL_SVC;  /* highest */
	servicepriority[1] = WMI_DATA_VO_SVC;
	servicepriority[2] = WMI_DATA_VI_SVC;
	servicepriority[3] = WMI_DATA_BE_SVC;
	servicepriority[4] = WMI_DATA_BK_SVC; /* lowest */

	/* set priority list */
	ath6kl_htc_set_credit_dist(htc_handle, cred_info, servicepriority, 5);

	return 0;
}

/* reduce an ep's credits back to a set limit */
static void ath6k_reduce_credits(struct htc_credit_state_info *cred_info,
				 struct htc_endpoint_credit_dist  *ep_dist,
				 int limit)
{
	int credits;

	ep_dist->cred_assngd = limit;

	if (ep_dist->credits <= limit)
		return;

	credits = ep_dist->credits - limit;
	ep_dist->credits -= credits;
	cred_info->cur_free_credits += credits;
}

static void ath6k_credit_update(struct htc_credit_state_info *cred_info,
				struct list_head *epdist_list)
{
	struct htc_endpoint_credit_dist *cur_dist_list;

	list_for_each_entry(cur_dist_list, epdist_list, list) {
		if (cur_dist_list->endpoint == ENDPOINT_0)
			continue;

		if (cur_dist_list->cred_to_dist > 0) {
			cur_dist_list->credits +=
					cur_dist_list->cred_to_dist;
			cur_dist_list->cred_to_dist = 0;
			if (cur_dist_list->credits >
			    cur_dist_list->cred_assngd)
				ath6k_reduce_credits(cred_info,
						cur_dist_list,
						cur_dist_list->cred_assngd);

			if (cur_dist_list->credits >
			    cur_dist_list->cred_norm)
				ath6k_reduce_credits(cred_info, cur_dist_list,
						     cur_dist_list->cred_norm);

			if (!(cur_dist_list->dist_flags & HTC_EP_ACTIVE)) {
				if (cur_dist_list->txq_depth == 0)
					ath6k_reduce_credits(cred_info,
							     cur_dist_list, 0);
			}
		}
	}
}

/*
 * HTC has an endpoint that needs credits, ep_dist is the endpoint in
 * question.
 */
void ath6k_seek_credits(struct htc_credit_state_info *cred_info,
			struct htc_endpoint_credit_dist *ep_dist)
{
	struct htc_endpoint_credit_dist *curdist_list;
	int credits = 0;
	int need;

	if (ep_dist->svc_id == WMI_CONTROL_SVC)
		goto out;

	if ((ep_dist->svc_id == WMI_DATA_VI_SVC) ||
	    (ep_dist->svc_id == WMI_DATA_VO_SVC))
		if ((ep_dist->cred_assngd >= ep_dist->cred_norm))
			goto out;

	/*
	 * For all other services, we follow a simple algorithm of:
	 *
	 * 1. checking the free pool for credits
	 * 2. checking lower priority endpoints for credits to take
	 */

	credits = min(cred_info->cur_free_credits, ep_dist->seek_cred);

	if (credits >= ep_dist->seek_cred)
		goto out;

	/*
	 * We don't have enough in the free pool, try taking away from
	 * lower priority services The rule for taking away credits:
	 *
	 *   1. Only take from lower priority endpoints
	 *   2. Only take what is allocated above the minimum (never
	 *      starve an endpoint completely)
	 *   3. Only take what you need.
	 */

	list_for_each_entry_reverse(curdist_list,
				    &cred_info->lowestpri_ep_dist,
				    list) {
		if (curdist_list == ep_dist)
			break;

		need = ep_dist->seek_cred - cred_info->cur_free_credits;

		if ((curdist_list->cred_assngd - need) >=
		     curdist_list->cred_min) {
			/*
			 * The current one has been allocated more than
			 * it's minimum and it has enough credits assigned
			 * above it's minimum to fulfill our need try to
			 * take away just enough to fulfill our need.
			 */
			ath6k_reduce_credits(cred_info, curdist_list,
					curdist_list->cred_assngd - need);

			if (cred_info->cur_free_credits >=
			    ep_dist->seek_cred)
				break;
		}

		if (curdist_list->endpoint == ENDPOINT_0)
			break;
	}

	credits = min(cred_info->cur_free_credits, ep_dist->seek_cred);

out:
	/* did we find some credits? */
	if (credits)
		ath6kl_deposit_credit_to_ep(cred_info, ep_dist, credits);

	ep_dist->seek_cred = 0;
}

/* redistribute credits based on activity change */
static void ath6k_redistribute_credits(struct htc_credit_state_info *info,
				       struct list_head *ep_dist_list)
{
	struct htc_endpoint_credit_dist *curdist_list;

	list_for_each_entry(curdist_list, ep_dist_list, list) {
		if (curdist_list->endpoint == ENDPOINT_0)
			continue;

		if ((curdist_list->svc_id == WMI_DATA_BK_SVC)  ||
		    (curdist_list->svc_id == WMI_DATA_BE_SVC))
			curdist_list->dist_flags |= HTC_EP_ACTIVE;

		if ((curdist_list->svc_id != WMI_CONTROL_SVC) &&
		    !(curdist_list->dist_flags & HTC_EP_ACTIVE)) {
			if (curdist_list->txq_depth == 0)
				ath6k_reduce_credits(info,
						curdist_list, 0);
			else
				ath6k_reduce_credits(info,
						curdist_list,
						curdist_list->cred_min);
		}
	}
}

/*
 *
 * This function is invoked whenever endpoints require credit
 * distributions. A lock is held while this function is invoked, this
 * function shall NOT block. The ep_dist_list is a list of distribution
 * structures in prioritized order as defined by the call to the
 * htc_set_credit_dist() api.
 */
void ath6k_credit_distribute(struct htc_credit_state_info *cred_info,
			     struct list_head *ep_dist_list,
			     enum htc_credit_dist_reason reason)
{
	switch (reason) {
	case HTC_CREDIT_DIST_SEND_COMPLETE:
		ath6k_credit_update(cred_info, ep_dist_list);
		break;
	case HTC_CREDIT_DIST_ACTIVITY_CHANGE:
		ath6k_redistribute_credits(cred_info, ep_dist_list);
		break;
	default:
		break;
	}

	WARN_ON(cred_info->cur_free_credits > cred_info->total_avail_credits);
	WARN_ON(cred_info->cur_free_credits < 0);
}

void disconnect_timer_handler(unsigned long ptr)
{
	struct net_device *dev = (struct net_device *)ptr;
	struct ath6kl_vif *vif = ath6kl_priv(dev);

	ath6kl_init_profile_info(vif);
	ath6kl_disconnect(vif);
}

void ath6kl_disconnect(struct ath6kl_vif *vif)
{
	if (!test_bit(WMI_READY, &vif->ar->flag))
		return;

	if (test_bit(CONNECTED, &vif->flag) ||
	    test_bit(CONNECT_PEND, &vif->flag)) {
		ath6kl_wmi_disconnect_cmd(vif);
		/*
		 * Disconnect command is issued, clear the connect pending
		 * flag. The connected flag will be cleared in
		 * disconnect event notification.
		 */
		clear_bit(CONNECT_PEND, &vif->flag);
	}
}

void ath6kl_deep_sleep_enable(struct ath6kl_vif *vif)
{
	switch (vif->sme_state) {
	case SME_CONNECTING:
		cfg80211_connect_result_ath6kl(vif->net_dev, vif->bssid, NULL, 0,
					NULL, 0,
					WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
		break;
	case SME_CONNECTED:
	default:
		/*
		 * FIXME: oddly enough smeState is in DISCONNECTED during
		 * suspend, why? Need to send disconnected event in that
		 * state.
		 */
		cfg80211_disconnected_ath6kl(vif->net_dev, 0, NULL, 0, GFP_KERNEL);
		break;
	}

	if (test_and_clear_bit(CONNECTED, &vif->flag) ||
	    test_and_clear_bit(CONNECT_PEND, &vif->flag))
		ath6kl_wmi_disconnect_cmd(vif);

	vif->sme_state = SME_DISCONNECTED;

	/* disable scanning */
	if (ath6kl_wmi_scanparams_cmd(vif, 0xFFFF, 0, 0, 0, 0, 0, 0, 0,
				      0, 0) != 0)
		printk(KERN_WARNING "ath6kl: failed to disable scan "
		       "during suspend\n");

	ath6kl_cfg80211_scan_complete_event(vif, -ECANCELED);
}

/* WMI Event handlers */

static const char *get_hw_id_string(u32 id)
{
	switch (id) {
	case AR6003_REV1_VERSION:
		return "AR6003 1.0";
	case AR6003_REV2_VERSION:
		return "AR6003 2.0";
	case AR6003_REV3_VERSION:
		return "AR6003 2.1.1";
	case AR6004_REV1_VERSION:
		return "AR6004 1.0";
	case AR6004_REV2_VERSION:
		return "AR6004 1.1";
	default:
		return "unknown";
	}
}

void ath6kl_ready_event(void *devt, u8 *datap, u32 sw_ver, u32 abi_ver)
{
	struct ath6kl *ar = devt;
	struct net_device *dev = ar->vif[0]->net_dev;
	struct ath6kl_vif *vif = ar->vif[0];
	int i;

	for (i = 0 ; i < ar->num_dev; i++) {
		struct ath6kl_vif *vif = ar->vif[i];
		struct net_device *dev = vif->net_dev;

		memcpy(dev->dev_addr, datap, ETH_ALEN);
		memcpy(ar->dev_addr, datap, ETH_ALEN);
		if (i > 0)
			dev->dev_addr[0] = (((dev->dev_addr[0]) ^ (1 << i))) | 0x02;
	}

	ath6kl_dbg(ATH6KL_DBG_TRC, "%s: mac addr = %pM\n",
		   __func__, dev->dev_addr);

	ar->version.wlan_ver = sw_ver;
	ar->version.abi_ver = abi_ver;

	snprintf(vif->wdev->wiphy->fw_version,
		 sizeof(vif->wdev->wiphy->fw_version),
		 "%u.%u.%u.%u",
		 (ar->version.wlan_ver & 0xf0000000) >> 28,
		 (ar->version.wlan_ver & 0x0f000000) >> 24,
		 (ar->version.wlan_ver & 0x00ff0000) >> 16,
		 (ar->version.wlan_ver & 0x0000ffff));

	/* indicate to the waiting thread that the ready event was received */
	set_bit(WMI_READY, &ar->flag);
	wake_up(&vif->event_wq);

	ath6kl_info("HW: %s FW: %s\n",
		    get_hw_id_string(vif->wdev->wiphy->hw_version),
		    vif->wdev->wiphy->fw_version);
}

void ath6kl_scan_complete_evt(struct ath6kl_vif *vif, int status)
{
	ath6kl_cfg80211_scan_complete_event(vif, status);

	if (!vif->usr_bss_filter) {
		ath6kl_wmi_bssfilter_cmd(vif, NONE_BSS_FILTER, 0);
	}

	ath6kl_dbg(ATH6KL_DBG_WLAN_SCAN, "scan complete: %d\n", status);
}

void ath6kl_connect_event(struct ath6kl_vif *vif, u16 channel, u8 *bssid,
			  u16 listen_int, u16 beacon_int,
			  enum network_type net_type, u8 beacon_ie_len,
			  u8 assoc_req_len, u8 assoc_resp_len,
			  u8 *assoc_info)
{
	unsigned long flags;
	struct ath6kl *ar = vif->ar;

	ath6kl_cfg80211_connect_event(vif, channel, bssid,
				      listen_int, beacon_int,
				      net_type, beacon_ie_len,
				      assoc_req_len, assoc_resp_len,
				      assoc_info);

	memcpy(vif->bssid, bssid, sizeof(vif->bssid));
	vif->bss_ch = channel;

	if ((vif->nw_type == INFRA_NETWORK))
		ath6kl_wmi_listeninterval_cmd(vif, vif->listen_intvl_t,
					      vif->listen_intvl_b);

	netif_wake_queue(vif->net_dev);

	/* Update connect & link status atomically */
	spin_lock_irqsave(&ar->lock, flags);
	set_bit(CONNECTED, &vif->flag);
	clear_bit(CONNECT_PEND, &vif->flag);
	netif_carrier_on(vif->net_dev);
	spin_unlock_irqrestore(&ar->lock, flags);

	aggr_reset_state(vif->sta_list[0].aggr_conn_cntxt);
	vif->reconnect_flag = 0;

	if ((vif->nw_type == ADHOC_NETWORK) && vif->ibss_ps_enable) {
		memset(vif->node_map, 0, sizeof(vif->node_map));
		vif->node_num = 0;
		ar->next_ep_id = ENDPOINT_2;
	}

	if (!vif->usr_bss_filter) {
		ath6kl_wmi_bssfilter_cmd(vif, NONE_BSS_FILTER, 0);
	}
}

void ath6kl_tkip_micerr_event(struct ath6kl_vif *vif, u8 keyid, bool ismcast)
{
	struct ath6kl_sta *sta;
	u8 tsc[6];
	/*
	 * For AP case, keyid will have aid of STA which sent pkt with
	 * MIC error. Use this aid to get MAC & send it to hostapd.
	 */
	if (vif->nw_type == AP_NETWORK) {
		sta = ath6kl_find_sta_by_aid(vif, (keyid >> 2));
		if (!sta)
			return;

		ath6kl_dbg(ATH6KL_DBG_TRC,
			   "ap tkip mic error received from aid=%d\n", keyid);

		memset(tsc, 0, sizeof(tsc)); /* FIX: get correct TSC */
		cfg80211_michael_mic_failure_ath6kl(vif->net_dev, sta->mac,
					     NL80211_KEYTYPE_PAIRWISE, keyid,
					     tsc, GFP_KERNEL);
	} else
		ath6kl_cfg80211_tkip_micerr_event(vif, keyid, ismcast);

}

static void ath6kl_update_target_stats(struct ath6kl_vif *vif, u8 *ptr, u32 len)
{
	struct ath6kl *ar = vif->ar;
	struct wmi_target_stats *tgt_stats =
		(struct wmi_target_stats *) ptr;
	struct target_stats *stats = &ar->target_stats;
	struct tkip_ccmp_stats *ccmp_stats;
	u8 ac;
	enum wlan_spatial_stream_state ss_state;

	if (len < sizeof(*tgt_stats))
		return;

	ath6kl_dbg(ATH6KL_DBG_TRC, "updating target stats\n");

	stats->tx_pkt += le32_to_cpu(tgt_stats->stats.tx.pkt);
	stats->tx_byte += le32_to_cpu(tgt_stats->stats.tx.byte);
	stats->tx_ucast_pkt += le32_to_cpu(tgt_stats->stats.tx.ucast_pkt);
	stats->tx_ucast_byte += le32_to_cpu(tgt_stats->stats.tx.ucast_byte);
	stats->tx_mcast_pkt += le32_to_cpu(tgt_stats->stats.tx.mcast_pkt);
	stats->tx_mcast_byte += le32_to_cpu(tgt_stats->stats.tx.mcast_byte);
	stats->tx_bcast_pkt  += le32_to_cpu(tgt_stats->stats.tx.bcast_pkt);
	stats->tx_bcast_byte += le32_to_cpu(tgt_stats->stats.tx.bcast_byte);
	stats->tx_rts_success_cnt +=
		le32_to_cpu(tgt_stats->stats.tx.rts_success_cnt);

	for (ac = 0; ac < WMM_NUM_AC; ac++)
		stats->tx_pkt_per_ac[ac] +=
			le32_to_cpu(tgt_stats->stats.tx.pkt_per_ac[ac]);

	stats->tx_err += le32_to_cpu(tgt_stats->stats.tx.err);
	stats->tx_fail_cnt += le32_to_cpu(tgt_stats->stats.tx.fail_cnt);
	stats->tx_retry_cnt += le32_to_cpu(tgt_stats->stats.tx.retry_cnt);
	stats->tx_mult_retry_cnt +=
		le32_to_cpu(tgt_stats->stats.tx.mult_retry_cnt);
	stats->tx_rts_fail_cnt +=
		le32_to_cpu(tgt_stats->stats.tx.rts_fail_cnt);
	switch(ar->target_type) {
	case TARGET_TYPE_AR6004:
		ss_state = WLAN_SPATIAL_STREAM_22;
		break;
	default:
		ss_state = WLAN_SPATIAL_STREAM_11;
		break;
	}
	stats->tx_ucast_rate =
	    ath6kl_wmi_get_rate(a_sle32_to_cpu(tgt_stats->stats.tx.ucast_rate),
	    		&stats->tx_sgi, &stats->tx_mcs, ss_state);

	stats->rx_pkt += le32_to_cpu(tgt_stats->stats.rx.pkt);
	stats->rx_byte += le32_to_cpu(tgt_stats->stats.rx.byte);
	stats->rx_ucast_pkt += le32_to_cpu(tgt_stats->stats.rx.ucast_pkt);
	stats->rx_ucast_byte += le32_to_cpu(tgt_stats->stats.rx.ucast_byte);
	stats->rx_mcast_pkt += le32_to_cpu(tgt_stats->stats.rx.mcast_pkt);
	stats->rx_mcast_byte += le32_to_cpu(tgt_stats->stats.rx.mcast_byte);
	stats->rx_bcast_pkt += le32_to_cpu(tgt_stats->stats.rx.bcast_pkt);
	stats->rx_bcast_byte += le32_to_cpu(tgt_stats->stats.rx.bcast_byte);
	stats->rx_frgment_pkt += le32_to_cpu(tgt_stats->stats.rx.frgment_pkt);
	stats->rx_err += le32_to_cpu(tgt_stats->stats.rx.err);
	stats->rx_crc_err += le32_to_cpu(tgt_stats->stats.rx.crc_err);
	stats->rx_key_cache_miss +=
		le32_to_cpu(tgt_stats->stats.rx.key_cache_miss);
	stats->rx_decrypt_err += le32_to_cpu(tgt_stats->stats.rx.decrypt_err);
	stats->rx_dupl_frame += le32_to_cpu(tgt_stats->stats.rx.dupl_frame);
	stats->rx_ucast_rate =
	    ath6kl_wmi_get_rate(a_sle32_to_cpu(tgt_stats->stats.rx.ucast_rate),
	    		&stats->rx_sgi, &stats->rx_mcs, ss_state);

	ccmp_stats = &tgt_stats->stats.tkip_ccmp_stats;

	stats->tkip_local_mic_fail +=
		le32_to_cpu(ccmp_stats->tkip_local_mic_fail);
	stats->tkip_cnter_measures_invoked +=
		le32_to_cpu(ccmp_stats->tkip_cnter_measures_invoked);
	stats->tkip_fmt_err += le32_to_cpu(ccmp_stats->tkip_fmt_err);

	stats->ccmp_fmt_err += le32_to_cpu(ccmp_stats->ccmp_fmt_err);
	stats->ccmp_replays += le32_to_cpu(ccmp_stats->ccmp_replays);

	stats->pwr_save_fail_cnt +=
		le32_to_cpu(tgt_stats->pm_stats.pwr_save_failure_cnt);
	stats->noise_floor_calib =
		a_sle32_to_cpu(tgt_stats->noise_floor_calib);

	stats->cs_bmiss_cnt +=
		le32_to_cpu(tgt_stats->cserv_stats.cs_bmiss_cnt);
	stats->cs_low_rssi_cnt +=
		le32_to_cpu(tgt_stats->cserv_stats.cs_low_rssi_cnt);
	stats->cs_connect_cnt +=
		le16_to_cpu(tgt_stats->cserv_stats.cs_connect_cnt);
	stats->cs_discon_cnt +=
		le16_to_cpu(tgt_stats->cserv_stats.cs_discon_cnt);

	stats->cs_ave_beacon_rssi =
		a_sle16_to_cpu(tgt_stats->cserv_stats.cs_ave_beacon_rssi);

	stats->cs_last_roam_msec =
		tgt_stats->cserv_stats.cs_last_roam_msec;
	stats->cs_snr = tgt_stats->cserv_stats.cs_snr;
	stats->cs_rssi = a_sle16_to_cpu(tgt_stats->cserv_stats.cs_rssi);

	stats->lq_val = le32_to_cpu(tgt_stats->lq_val);

	stats->wow_pkt_dropped +=
		le32_to_cpu(tgt_stats->wow_stats.wow_pkt_dropped);
	stats->wow_host_pkt_wakeups +=
		tgt_stats->wow_stats.wow_host_pkt_wakeups;
	stats->wow_host_evt_wakeups +=
		tgt_stats->wow_stats.wow_host_evt_wakeups;
	stats->wow_evt_discarded +=
		le16_to_cpu(tgt_stats->wow_stats.wow_evt_discarded);

	if (test_bit(STATS_UPDATE_PEND, &vif->flag)) {
		clear_bit(STATS_UPDATE_PEND, &vif->flag);
		wake_up(&vif->event_wq);
	}
}

static void ath6kl_add_le32(__le32 *var, __le32 val)
{
	*var = cpu_to_le32(le32_to_cpu(*var) + le32_to_cpu(val));
}

void ath6kl_tgt_stats_event(struct ath6kl_vif *vif, u8 *ptr, u32 len)
{
	struct wmi_ap_mode_stat *p = (struct wmi_ap_mode_stat *) ptr;
	struct wmi_ap_mode_stat *ap = &vif->ap_stats;
	struct wmi_per_sta_stat *st_ap, *st_p;
	u8 ac;

	if (vif->nw_type == AP_NETWORK) {
		if (len < sizeof(*p))
			return;

		for (ac = 0; ac < AP_MAX_NUM_STA; ac++) {
			st_ap = &ap->sta[ac];
			st_p = &p->sta[ac];

			ath6kl_add_le32(&st_ap->tx_bytes, st_p->tx_bytes);
			ath6kl_add_le32(&st_ap->tx_pkts, st_p->tx_pkts);
			ath6kl_add_le32(&st_ap->tx_error, st_p->tx_error);
			ath6kl_add_le32(&st_ap->tx_discard, st_p->tx_discard);
			ath6kl_add_le32(&st_ap->rx_bytes, st_p->rx_bytes);
			ath6kl_add_le32(&st_ap->rx_pkts, st_p->rx_pkts);
			ath6kl_add_le32(&st_ap->rx_error, st_p->rx_error);
			ath6kl_add_le32(&st_ap->rx_discard, st_p->rx_discard);
		}

	} else {
		ath6kl_update_target_stats(vif, ptr, len);
	}
}

void ath6kl_wakeup_event(void *dev)
{
	struct ath6kl *ar = (struct ath6kl *) dev;

	wake_up(&ar->vif[0]->event_wq);
}

void ath6kl_txpwr_rx_evt(void *devt, u8 tx_pwr)
{
	struct ath6kl *ar = (struct ath6kl *) devt;
	struct ath6kl_vif *vif = ar->vif[0];
	vif->tx_pwr = tx_pwr;
	wake_up(&vif->event_wq);
}

void ath6kl_pspoll_event(struct ath6kl_vif *vif, u8 aid)
{
	struct ath6kl_sta *conn;
	struct sk_buff *skb;
	bool psq_empty = false;
	bool is_psq_empty, is_mgmt_psq_empty;
        struct mgmt_buff* mgmt_buf;

	conn = ath6kl_find_sta_by_aid(vif, aid);

	if (!conn)
		return;
	/*
	 * Send out a packet queued on ps queue. When the ps queue
	 * becomes empty update the PVB for this station.
	 */
	spin_lock_bh(&conn->lock);
	is_psq_empty  = skb_queue_empty(&conn->psq);
	is_mgmt_psq_empty  = ath6kl_mgmt_queue_empty(&conn->mgmt_psq);
	psq_empty  = is_psq_empty && is_mgmt_psq_empty;

	if (psq_empty) {
		/* TODO: Send out a NULL data frame */
		spin_unlock_bh(&conn->lock);
		return;
	}
        if (!is_mgmt_psq_empty) {
                mgmt_buf = ath6kl_mgmt_dequeue_head(&conn->mgmt_psq);
	        spin_unlock_bh(&conn->lock);
                
	        conn->sta_flags |= STA_PS_POLLED;
                ath6kl_wmi_send_action_cmd(vif, 
                                           mgmt_buf->id, 
                                           mgmt_buf->freq, 
                                           mgmt_buf->wait, 
                                           mgmt_buf->buf,
                                           mgmt_buf->len);
		conn->sta_flags &= ~STA_PS_POLLED;
                kfree(mgmt_buf);
        } else {
		skb = skb_dequeue(&conn->psq);
	        spin_unlock_bh(&conn->lock);

	        conn->sta_flags |= STA_PS_POLLED;
         	ath6kl_data_tx(skb, vif->net_dev);
		conn->sta_flags &= ~STA_PS_POLLED;
        }

	spin_lock_bh(&conn->lock);
	psq_empty  = skb_queue_empty(&conn->psq) && ath6kl_mgmt_queue_empty(&conn->mgmt_psq);
	spin_unlock_bh(&conn->lock);

	if (psq_empty) {
		ath6kl_wmi_set_pvb_cmd(vif, conn->aid, 0);
	}
}

void ath6kl_dtimexpiry_event(struct ath6kl_vif *vif)
{
	bool mcastq_empty = false;
	struct sk_buff *skb;

	/*
	 * If there are no associated STAs, ignore the DTIM expiry event.
	 * There can be potential race conditions where the last associated
	 * STA may disconnect & before the host could clear the 'Indicate
	 * DTIM' request to the firmware, the firmware would have just
	 * indicated a DTIM expiry event. The race is between 'clear DTIM
	 * expiry cmd' going from the host to the firmware & the DTIM
	 * expiry event happening from the firmware to the host.
	 */
	if (!vif->sta_list_index)
		return;

	spin_lock_bh(&vif->mcastpsq_lock);
	mcastq_empty = skb_queue_empty(&vif->mcastpsq);
	spin_unlock_bh(&vif->mcastpsq_lock);

	if (mcastq_empty)
		return;

	/* set the STA flag to dtim_expired for the frame to go out */
	set_bit(DTIM_EXPIRED, &vif->flag);

	spin_lock_bh(&vif->mcastpsq_lock);
	while ((skb = skb_dequeue(&vif->mcastpsq)) != NULL) {
		spin_unlock_bh(&vif->mcastpsq_lock);

		ath6kl_data_tx(skb, vif->net_dev);

		spin_lock_bh(&vif->mcastpsq_lock);
	}
	spin_unlock_bh(&vif->mcastpsq_lock);

	clear_bit(DTIM_EXPIRED, &vif->flag);

	/* clear the LSB of the BitMapCtl field of the TIM IE */
	ath6kl_wmi_set_pvb_cmd(vif, MCAST_AID, 0);
}

void ath6kl_disconnect_event(struct ath6kl_vif *vif, u8 reason, u8 *bssid,
			     u8 assoc_resp_len, u8 *assoc_info,
			     u16 prot_reason_status)
{
	struct ath6kl *ar = vif->ar;
	unsigned long flags;

	if (vif->nw_type == AP_NETWORK) {
		if (!ath6kl_remove_sta(vif, bssid, prot_reason_status))
			return;

		/* if no more associated STAs, empty the mcast PS q */
		if (vif->sta_list_index == 0) {
			spin_lock_bh(&vif->mcastpsq_lock);
			skb_queue_purge(&vif->mcastpsq);
			spin_unlock_bh(&vif->mcastpsq_lock);

			/* clear the LSB of the TIM IE's BitMapCtl field */
			if (test_bit(WMI_READY, &ar->flag))
				ath6kl_wmi_set_pvb_cmd(vif, MCAST_AID, 0);
		}

		if (!is_broadcast_ether_addr(bssid)) {
			/* send event to application */
			cfg80211_del_sta(vif->net_dev, bssid, GFP_KERNEL);
		}

		if (memcmp(vif->net_dev->dev_addr, bssid, ETH_ALEN) == 0)
			clear_bit(CONNECTED, &vif->flag);
		return;
	}

	ath6kl_cfg80211_disconnect_event(vif, reason, bssid,
				       assoc_resp_len, assoc_info,
				       prot_reason_status);

	aggr_reset_state(vif->sta_list[0].aggr_conn_cntxt);

	del_timer(&vif->disconnect_timer);

	ath6kl_dbg(ATH6KL_DBG_WLAN_CONNECT,
		   "disconnect reason is %d\n", reason);


	/*
	 * If the event is due to disconnect cmd from the host, only they
	 * the target would stop trying to connect. Under any other
	 * condition, target would keep trying to connect.
	 */
	if (reason == DISCONNECT_CMD) {
		if (!vif->usr_bss_filter && test_bit(WMI_READY, &ar->flag)) {
			ath6kl_wmi_bssfilter_cmd(vif, NONE_BSS_FILTER, 0);
		}
	} else {
		set_bit(CONNECT_PEND, &vif->flag);
		if (((reason == ASSOC_FAILED) &&
		    (prot_reason_status == 0x11)) ||
		    ((reason == ASSOC_FAILED) && (prot_reason_status == 0x0)
		     && (vif->reconnect_flag == 1))) {
			set_bit(CONNECTED, &vif->flag);
			return;
		}
	}

	/* update connect & link status atomically */
	spin_lock_irqsave(&ar->lock, flags);
	clear_bit(CONNECTED, &vif->flag);
	netif_carrier_off(vif->net_dev);
	spin_unlock_irqrestore(&ar->lock, flags);

	if ((reason != CSERV_DISCONNECT) || (vif->reconnect_flag != 1))
		vif->reconnect_flag = 0;

	if (reason != CSERV_DISCONNECT)
		vif->user_key_ctrl = 0;

	netif_stop_queue(vif->net_dev);
	memset(vif->bssid, 0, sizeof(vif->bssid));
	vif->bss_ch = 0;

	ath6kl_tx_data_cleanup(ar);
}

static int ath6kl_open(struct net_device *dev)
{
	struct ath6kl_vif *vif = ath6kl_priv(dev);
	struct ath6kl *ar = vif->ar;
	unsigned long flags;

	spin_lock_irqsave(&ar->lock, flags);

	set_bit(WLAN_ENABLED, &ar->flag);

	if (test_bit(CONNECTED, &vif->flag)) {
		netif_carrier_on(dev);
		netif_wake_queue(dev);
	} else
		netif_carrier_off(dev);

	spin_unlock_irqrestore(&ar->lock, flags);

	return 0;
}

static int ath6kl_close(struct net_device *dev)
{
	struct ath6kl_vif *vif = ath6kl_priv(dev);
	struct ath6kl *ar = vif->ar;

	if (test_bit(DESTROY_IN_PROGRESS, &ar->flag))
		return -EBUSY;

	netif_stop_queue(dev);

	ath6kl_disconnect(vif);

	if (test_bit(WMI_READY, &ar->flag)) {
		if (ath6kl_wmi_scanparams_cmd(vif, 0xFFFF, 0, 0, 0, 0, 0, 0,
					      0, 0, 0))
			return -EIO;

		clear_bit(WLAN_ENABLED, &ar->flag);
	}

	ath6kl_cfg80211_scan_complete_event(vif, -ECANCELED);

	return 0;
}

static struct net_device_stats *ath6kl_get_stats(struct net_device *dev)
{
	struct ath6kl_vif *vif = ath6kl_priv(dev);

	return &vif->net_stats;
}

extern int ath6kl_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd);

static struct net_device_ops ath6kl_netdev_ops = {
	.ndo_open               = ath6kl_open,
	.ndo_stop               = ath6kl_close,
	.ndo_start_xmit         = ath6kl_data_tx,
	.ndo_get_stats          = ath6kl_get_stats,
    .ndo_do_ioctl           = ath6kl_ioctl,
};

void init_netdev(struct net_device *dev)
{
	dev->netdev_ops = &ath6kl_netdev_ops;
	dev->watchdog_timeo = ATH6KL_TX_TIMEOUT;

	dev->needed_headroom = ETH_HLEN;
	dev->needed_headroom += sizeof(struct ath6kl_llc_snap_hdr) +
				sizeof(struct wmi_data_hdr) + HTC_HDR_LENGTH
				+ WMI_MAX_TX_META_SZ + ATH6KL_HTC_ALIGN_BYTES;

	return;
}
