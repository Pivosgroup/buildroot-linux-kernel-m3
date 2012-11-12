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
#include "debug.h"

/* 802.1d to AC mapping. Refer pg 57 of WMM-test-plan-v1.2 */
static const u8 up_to_ac[] = {
	WMM_AC_BE,
	WMM_AC_BK,
	WMM_AC_BK,
	WMM_AC_BE,
	WMM_AC_VI,
	WMM_AC_VI,
	WMM_AC_VO,
	WMM_AC_VO,
};

static int aggr_tx(struct ath6kl_vif *vif, struct sk_buff **skb);
static int aggr_tx_flush(struct ath6kl_vif *vif);

static u8 ath6kl_ibss_map_epid(struct sk_buff *skb, struct net_device *dev,
			       u32 *map_no)
{
	struct ath6kl_vif *vif = ath6kl_priv(dev);
	struct ath6kl *ar = vif->ar;
	struct ethhdr *eth_hdr;
	u32 i, ep_map = -1;
	u8 *datap;

	*map_no = 0;
	datap = skb->data;
	eth_hdr = (struct ethhdr *) (datap + sizeof(struct wmi_data_hdr));

	if (is_multicast_ether_addr(eth_hdr->h_dest))
		return ENDPOINT_2;

	for (i = 0; i < vif->node_num; i++) {
		if (memcmp(eth_hdr->h_dest, vif->node_map[i].mac_addr,
			   ETH_ALEN) == 0) {
			*map_no = i + 1;
			vif->node_map[i].tx_pend++;
			return vif->node_map[i].ep_id;
		}

		if ((ep_map == -1) && !vif->node_map[i].tx_pend)
			ep_map = i;
	}

	if (ep_map == -1) {
		ep_map = vif->node_num;
		vif->node_num++;
		if (vif->node_num > MAX_NODE_NUM)
			return ENDPOINT_UNUSED;
	}

	memcpy(vif->node_map[ep_map].mac_addr, eth_hdr->h_dest, ETH_ALEN);

	for (i = ENDPOINT_2; i <= ENDPOINT_5; i++) {
		if (!ar->tx_pending[i]) {
			vif->node_map[ep_map].ep_id = i;
			break;
		}

		/*
		 * No free endpoint is available, start redistribution on
		 * the inuse endpoints.
		 */
		if (i == ENDPOINT_5) {
			vif->node_map[ep_map].ep_id = ar->next_ep_id;
			ar->next_ep_id++;
			if (ar->next_ep_id > ENDPOINT_5)
				ar->next_ep_id = ENDPOINT_2;
		}
	}

	*map_no = ep_map + 1;
	vif->node_map[ep_map].tx_pend++;

	return vif->node_map[ep_map].ep_id;
}

static bool ath6kl_powersave_ap(struct ath6kl_vif *vif, struct sk_buff *skb,
				u32* flags)
{
	struct ethhdr *datap = (struct ethhdr *) skb->data;
	struct ath6kl_sta *conn = NULL;
	bool ps_queued = false;
	bool is_psq_empty = false;
	bool trigger = false;

	if (is_multicast_ether_addr(datap->h_dest)) {
		u8 ctr = 0;
		bool q_mcast = false;

		for (ctr = 0; ctr < AP_MAX_NUM_STA; ctr++) {
			if (vif->sta_list[ctr].sta_flags & STA_PS_SLEEP) {
				q_mcast = true;
				break;
			}
		}

		if (q_mcast) {
			/*
			 * If this transmit is not because of a Dtim Expiry
			 * q it.
			 */
			if (!test_bit(DTIM_EXPIRED, &vif->flag)) {
				bool is_mcastq_empty = false;

				spin_lock_bh(&vif->mcastpsq_lock);
				is_mcastq_empty =
					skb_queue_empty(&vif->mcastpsq);
				skb_queue_tail(&vif->mcastpsq, skb);
				spin_unlock_bh(&vif->mcastpsq_lock);

				/*
				 * If this is the first Mcast pkt getting
				 * queued indicate to the target to set the
				 * BitmapControl LSB of the TIM IE.
				 */
				if (is_mcastq_empty)
					ath6kl_wmi_set_pvb_cmd(vif,
							       MCAST_AID, 1);

				ps_queued = true;
			} else {
				/*
				 * This transmit is because of Dtim expiry.
				 * Determine if MoreData bit has to be set.
				 */
				spin_lock_bh(&vif->mcastpsq_lock);
				if (!skb_queue_empty(&vif->mcastpsq))
					*flags |= WMI_DATA_HDR_FLAGS_MORE;
				spin_unlock_bh(&vif->mcastpsq_lock);
			}
		}
	} else {
		conn = ath6kl_find_sta(vif, datap->h_dest);
		if (!conn) {
			dev_kfree_skb(skb);

			/* Inform the caller that the skb is consumed */
			return true;
		}

		if (conn->sta_flags & STA_PS_SLEEP) {
			if (!((conn->sta_flags & STA_PS_POLLED) || (conn->sta_flags & STA_PS_APSD_TRIGGER))) {
				if (conn->apsd_info) {
					u8 up = 0;
					u8 traffic_class;

					if (vif->wmm_enabled) {
						u16 ether_type;
						u16 ip_type = IP_ETHERTYPE;
						u8 *ip_hdr;

						ether_type = datap->h_proto;
						if (is_ethertype(be16_to_cpu(ether_type))) {
							/* packet is in DIX format  */
							ip_hdr = (u8*)(datap + 1);
						} else {
							/* packet is in 802.3 format */
							struct ath6kl_llc_snap_hdr *llc_hdr;

							llc_hdr = (struct ath6kl_llc_snap_hdr *)(datap + 1);
							ether_type = llc_hdr->eth_type;
							ip_hdr = (u8*)(llc_hdr + 1);
						}

						if (ether_type == cpu_to_be16(ip_type)) {
							up = ath6kl_wmi_determine_user_priority(ip_hdr, 0);
						}
					}
					traffic_class = ath6kl_wmi_convert_user_priority_to_traffic_class(up);
					if (conn->apsd_info & (1 << traffic_class)) {
						trigger = true;
					}
				}
				/* Queue the frames if the STA is sleeping */
				spin_lock_bh(&conn->lock);
				is_psq_empty = skb_queue_empty(&conn->psq) && ath6kl_mgmt_queue_empty(&conn->mgmt_psq);
				ath6kl_sk_buff_set_age(skb, 0);
				skb_queue_tail(&conn->psq, skb);
				spin_unlock_bh(&conn->lock);

                               	if (is_psq_empty) {
					if (trigger) {
						ath6kl_wmi_set_apsd_buffered_traffic_cmd(vif, conn->aid, 1, 0);
					}
					/* If this is the first pkt getting queued
					 * for this STA, update the PVB for this STA
                                	 */
					ath6kl_wmi_set_pvb_cmd(vif, conn->aid, 1);

				}
				ps_queued = true;
			} else {
				/*
				 * This tx is because of a PsPoll or trigger. Determine if
				 * MoreData bit has to be set
				 */
				spin_lock_bh(&conn->lock);
                               	if (!skb_queue_empty(&conn->psq) || !ath6kl_mgmt_queue_empty(&conn->mgmt_psq)) {
					*flags |= WMI_DATA_HDR_FLAGS_MORE;
				}
				if (!(conn->sta_flags & STA_PS_POLLED)) {
					/*
					 * This tx is because of a uAPSD trigger, determine
					 * more and EOSP bit. Set EOSP is queue is empty
					 * or sufficient frames is delivered for this trigger
					 */
					*flags |= WMI_DATA_HDR_FLAGS_TRIGGERED;
					if (conn->sta_flags & STA_PS_APSD_EOSP) {
						*flags |= WMI_DATA_HDR_FLAGS_EOSP;
					}
				} else {
					*flags |= WMI_DATA_HDR_FLAGS_PSPOLLED;
				}
				spin_unlock_bh(&conn->lock);
			}
		}
	}

	return ps_queued;
}

/* Tx functions */

int __ath6kl_control_tx(void *devt, struct sk_buff *skb,
		      enum htc_endpoint_id eid, int sync)
{
	struct ath6kl *ar = devt;
	int status = 0;
	struct ath6kl_cookie *cookie = NULL;

	spin_lock_bh(&ar->lock);

	ath6kl_dbg(ATH6KL_DBG_WLAN_TX,
		   "%s: skb=0x%p, len=0x%x eid =%d\n", __func__,
		   skb, skb->len, eid);

	if (test_bit(WMI_CTRL_EP_FULL, &ar->flag) && (eid == ar->ctrl_ep)) {
		/*
		 * Control endpoint is full, don't allocate resources, we
		 * are just going to drop this packet.
		 */
		cookie = NULL;
		ath6kl_err("wmi ctrl ep full, dropping pkt : 0x%p, len:%d\n",
			   skb, skb->len);
	} else
		cookie = ath6kl_alloc_cookie(ar);

	if (cookie == NULL) {
		spin_unlock_bh(&ar->lock);
		status = -ENOMEM;
		goto fail_ctrl_tx;
	}

	ar->tx_pending[eid]++;

	if (eid != ar->ctrl_ep)
		ar->total_tx_data_pend++;

	spin_unlock_bh(&ar->lock);

	cookie->skb = skb;
	cookie->map_no = 0;
	set_htc_pkt_info(&cookie->htc_pkt, cookie, skb->data, skb->len, eid,
		sync ? ATH6KL_CONTROL_PKT_SYNC_TAG : ATH6KL_CONTROL_PKT_TAG);
	cookie->htc_pkt.netbufcontext = skb;

	/*
	 * This interface is asynchronous, if there is an error, cleanup
	 * will happen in the TX completion callback.
	 */
	ath6kl_htc_tx(ar->htc_target, &cookie->htc_pkt);

	return 0;

fail_ctrl_tx:
	dev_kfree_skb(skb);
	return status;
}

int ath6kl_control_tx(void *devt, struct sk_buff *skb,
		      enum htc_endpoint_id eid)
{
	return __ath6kl_control_tx(devt, skb, eid, 0);
}

int ath6kl_control_tx_sync(void *devt, struct sk_buff *skb,
		      enum htc_endpoint_id eid)
{
	return __ath6kl_control_tx(devt, skb, eid, 1);
}

/* indicate tx activity or inactivity on a WMI stream */

int ath6kl_data_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct ath6kl_vif *vif = ath6kl_priv(dev);
	struct ath6kl *ar = vif->ar;
	struct ath6kl_cookie *cookie = NULL;
	enum htc_endpoint_id eid = ENDPOINT_UNUSED;
	u32 map_no = 0;
	u16 htc_tag = ATH6KL_DATA_PKT_TAG;
	u8 ac = 99 ; /* initialize to unmapped ac */
	u32 wmi_data_flags = 0;
	bool chk_adhoc_ps_mapping = false;
	struct wmi_tx_meta_v2 meta_v2;
	int ret, aggr_tx_status = AGGR_TX_UNKNOW;

	ath6kl_dbg(ATH6KL_DBG_WLAN_TX,
		   "%s: skb=0x%p, data=0x%p, len=0x%x\n", __func__,
		   skb, skb->data, skb->len);

	/* If target is not associated */
	if (!test_bit(CONNECTED, &vif->flag)) {
		dev_kfree_skb(skb);
		return 0;
	}

	if (!test_bit(WMI_READY, &ar->flag))
		goto fail_tx;

	/* AP mode Power saving processing */
	if (vif->nw_type == AP_NETWORK) {
		if (ath6kl_powersave_ap(vif, skb, &wmi_data_flags))
			return 0;
	}

	if (test_bit(WMI_ENABLED, &ar->flag)) {
		memset(&meta_v2, 0, sizeof(meta_v2));

		if (skb_headroom(skb) < dev->needed_headroom) {
			WARN_ON(1);
			goto fail_tx;
		}

		if (ath6kl_wmi_dix_2_dot3(ar->wmi, skb)) {
			ath6kl_err("ath6kl_wmi_dix_2_dot3 failed\n");
			goto fail_tx;
		}

		if (ath6kl_wmi_data_hdr_add(vif, skb, DATA_MSGTYPE,
					    wmi_data_flags, 0, 0, NULL)) {
			ath6kl_err("wmi_data_hdr_add failed\n");
			goto fail_tx;
		}

		if ((vif->nw_type == ADHOC_NETWORK) &&
		     vif->ibss_ps_enable && test_bit(CONNECTED, &vif->flag))
			chk_adhoc_ps_mapping = true;
		else {
			/* get the stream mapping */
			ret = ath6kl_wmi_implicit_create_pstream(vif, skb,
				    0, test_bit(WMM_ENABLED, &vif->flag), &ac);
			if (ret)
				goto fail_tx;
		}
	} else
		goto fail_tx;

	/* TX A-MSDU */
	if ((test_bit(AMSDU_ENABLED, &vif->flag)) &&
		(vif->aggr_cntxt->tx_amsdu_enable) &&
		(!chk_adhoc_ps_mapping) &&
		(vif->nw_type == INFRA_NETWORK)) {			/* TBD : Only STA mode now. */
		aggr_tx_status = aggr_tx(vif, &skb);

		if (aggr_tx_status == AGGR_TX_OK)
			return 0;
		else if (aggr_tx_status == AGGR_TX_DROP) 
			goto fail_tx;

		WARN_ON(skb == NULL);

		if ((vif->aggr_cntxt->tx_amsdu_seq_pkt) &&
			(aggr_tx_status == AGGR_TX_BYPASS))
			aggr_tx_flush(vif);
	}

	spin_lock_bh(&ar->lock);

	if (chk_adhoc_ps_mapping)
		eid = ath6kl_ibss_map_epid(skb, dev, &map_no);
	else
		eid = ar->ac2ep_map[ac];

	if (eid == 0 || eid == ENDPOINT_UNUSED) {
		ath6kl_err("eid %d is not mapped!\n", eid);
		spin_unlock_bh(&ar->lock);
		goto fail_tx;
	}

	/* allocate resource for this packet */
	cookie = ath6kl_alloc_cookie(ar);

	if (!cookie) {
		spin_unlock_bh(&ar->lock);
		goto fail_tx;
	}

	/* update counts while the lock is held */
	ar->tx_pending[eid]++;
	ar->total_tx_data_pend++;

	spin_unlock_bh(&ar->lock);

	cookie->skb = skb;
	cookie->map_no = map_no;
	set_htc_pkt_info(&cookie->htc_pkt, cookie, skb->data, skb->len,
			 eid, htc_tag);
	cookie->htc_pkt.netbufcontext = skb;

	SKB_CB_VIF(skb)  = vif;

	ath6kl_dbg_dump(ATH6KL_DBG_RAW_BYTES, __func__, skb->data, skb->len);

	/*
	 * HTC interface is asynchronous, if this fails, cleanup will
	 * happen in the ath6kl_tx_complete callback.
	 */
	ath6kl_htc_tx(ar->htc_target, &cookie->htc_pkt);

	return 0;

fail_tx:
	dev_kfree_skb(skb);

	vif->net_stats.tx_dropped++;
	vif->net_stats.tx_aborted_errors++;

	return 0;
}

/* indicate tx activity or inactivity on a WMI stream */
void ath6kl_indicate_tx_activity(void *devt, u8 traffic_class, bool active)
{
	struct ath6kl *ar = devt;
	enum htc_endpoint_id eid;
	int i;

	eid = ar->ac2ep_map[traffic_class];

	if (!test_bit(WMI_ENABLED, &ar->flag))
		goto notify_htc;

	spin_lock_bh(&ar->lock);

	ar->ac_stream_active[traffic_class] = active;

	if (active) {
		/*
		 * Keep track of the active stream with the highest
		 * priority.
		 */
		if (ar->ac_stream_pri_map[traffic_class] >
		    ar->hiac_stream_active_pri)
			/* set the new highest active priority */
			ar->hiac_stream_active_pri =
					ar->ac_stream_pri_map[traffic_class];

	} else {
		/*
		 * We may have to search for the next active stream
		 * that is the highest priority.
		 */
		if (ar->hiac_stream_active_pri ==
			ar->ac_stream_pri_map[traffic_class]) {
			/*
			 * The highest priority stream just went inactive
			 * reset and search for the "next" highest "active"
			 * priority stream.
			 */
			ar->hiac_stream_active_pri = 0;

			for (i = 0; i < WMM_NUM_AC; i++) {
				if (ar->ac_stream_active[i] &&
				    (ar->ac_stream_pri_map[i] >
				     ar->hiac_stream_active_pri))
					/*
					 * Set the new highest active
					 * priority.
					 */
					ar->hiac_stream_active_pri =
						ar->ac_stream_pri_map[i];
			}
		}
	}

	spin_unlock_bh(&ar->lock);

notify_htc:
	/* notify HTC, this may cause credit distribution changes */
	ath6kl_htc_indicate_activity_change(ar->htc_target, eid, active);
}

enum htc_send_full_action ath6kl_tx_queue_full(struct htc_target *target,
					       struct htc_packet *packet)
{
	struct ath6kl *ar = target->dev->ar;
	enum htc_endpoint_id endpoint = packet->endpoint;


	if (endpoint == ar->ctrl_ep) {
		/*
		 * Under normal WMI if this is getting full, then something
		 * is running rampant the host should not be exhausting the
		 * WMI queue with too many commands the only exception to
		 * this is during testing using endpointping.
		 */
		spin_lock_bh(&ar->lock);
		set_bit(WMI_CTRL_EP_FULL, &ar->flag);
		spin_unlock_bh(&ar->lock);
		schedule_work(&ar->firmware_crash_dump_deferred_work);
		ath6kl_err("wmi ctrl ep is full\n");
		return HTC_SEND_FULL_KEEP;
	}

	if (packet->info.tx.tag == ATH6KL_CONTROL_PKT_TAG)
		return HTC_SEND_FULL_KEEP;
	if (packet->info.tx.tag == ATH6KL_CONTROL_PKT_SYNC_TAG) {
		ath6kl_err("no sync packet should be here\n");
		return HTC_SEND_FULL_DROP;
	}

	if (ar->vif[0]->nw_type == ADHOC_NETWORK)
		/*
		 * In adhoc mode, we cannot differentiate traffic
		 * priorities so there is no need to continue, however we
		 * should stop the network.
		 */
		goto stop_net_queues;

	/*
	 * The last MAX_HI_COOKIE_NUM "batch" of cookies are reserved for
	 * the highest active stream.
	 */
	if (ar->ac_stream_pri_map[ar->ep2ac_map[endpoint]] <
	    ar->hiac_stream_active_pri &&
	    ar->cookie_count <= MAX_HI_COOKIE_NUM)
		/*
		 * Give preference to the highest priority stream by
		 * dropping the packets which overflowed.
		 */
		return HTC_SEND_FULL_DROP;

stop_net_queues:
	spin_lock_bh(&ar->lock);
	set_bit(NETQ_STOPPED, &ar->vif[0]->flag);
	spin_unlock_bh(&ar->lock);
	netif_stop_queue(ar->vif[0]->net_dev);

	return HTC_SEND_FULL_KEEP;
}

/* TODO this needs to be looked at */
static void ath6kl_tx_clear_node_map(struct ath6kl_vif *vif,
				     enum htc_endpoint_id eid, u32 map_no)
{
	u32 i;
	struct ath6kl *ar = vif->ar;

	if (vif->nw_type != ADHOC_NETWORK)
		return;

	if (!vif->ibss_ps_enable)
		return;

	if (eid == ar->ctrl_ep)
		return;

	if (map_no == 0)
		return;

	map_no--;
	vif->node_map[map_no].tx_pend--;

	if (vif->node_map[map_no].tx_pend)
		return;

	if (map_no != (vif->node_num - 1))
		return;

	for (i = vif->node_num; i > 0; i--) {
		if (vif->node_map[i - 1].tx_pend)
			break;

		memset(&vif->node_map[i - 1], 0,
		       sizeof(struct ath6kl_node_mapping));
		vif->node_num--;
	}
}

void ath6kl_tx_complete(struct htc_target *target,
				struct list_head *packet_queue)
{
	struct ath6kl *ar = target->dev->ar;
	struct ath6kl_vif  *vif = NULL;
	struct sk_buff_head skb_queue;
	struct htc_packet *packet;
	struct sk_buff *skb;
	struct ath6kl_cookie *ath6kl_cookie;
	u32 map_no = 0;
	int status;
	enum htc_endpoint_id eid;
	bool wake_event = false;
	bool flushing = false;

	skb_queue_head_init(&skb_queue);

	/* lock the driver as we update internal state */
	spin_lock_bh(&ar->lock);

	/* reap completed packets */
	while (!list_empty(packet_queue)) {

		packet = list_first_entry(packet_queue, struct htc_packet,
					  list);
		list_del(&packet->list);

		ath6kl_cookie = (struct ath6kl_cookie *)packet->pkt_cntxt;
		if (!ath6kl_cookie)
			goto fatal;

		status = packet->status;
		skb = ath6kl_cookie->skb;
		eid = packet->endpoint;
		map_no = ath6kl_cookie->map_no;

		if (!skb || !skb->data)
			goto fatal;

		vif = SKB_CB_VIF(skb);

		packet->buf = skb->data;

		__skb_queue_tail(&skb_queue, skb);

		if (!status && (packet->act_len != skb->len))
			goto fatal;

		ar->tx_pending[eid]--;

		if (eid != ar->ctrl_ep)
			ar->total_tx_data_pend--;

		if (eid == ar->ctrl_ep) {
			if (test_bit(WMI_CTRL_EP_FULL, &ar->flag))
				clear_bit(WMI_CTRL_EP_FULL, &ar->flag);

			if (ar->tx_pending[eid] == 0)
				wake_event = true;
		}

		if (status) {
			if (status == -ECANCELED)
				/* a packet was flushed  */
				flushing = true;

			vif->net_stats.tx_errors++;
#if 0
			if (status != -ENOSPC)
				ath6kl_err("tx error, status: 0x%x\n", status);
#endif
			ath6kl_dbg(ATH6KL_DBG_WLAN_TX,
				   "%s: skb=0x%p data=0x%p len=0x%x eid=%d %s\n",
				   __func__, skb, packet->buf, packet->act_len,
				   eid, "error!");
		} else {
			ath6kl_dbg(ATH6KL_DBG_WLAN_TX,
				   "%s: skb=0x%p data=0x%p len=0x%x eid=%d %s\n",
				   __func__, skb, packet->buf, packet->act_len,
				   eid, "OK");

			flushing = false;
			vif->net_stats.tx_packets++;
			vif->net_stats.tx_bytes += skb->len;
		}

		ath6kl_tx_clear_node_map(vif, eid, map_no);

		ath6kl_free_cookie(ar, ath6kl_cookie);

		if (test_bit(NETQ_STOPPED, &vif->flag))
			clear_bit(NETQ_STOPPED, &vif->flag);
	}

	spin_unlock_bh(&ar->lock);

	__skb_queue_purge(&skb_queue);

	if (vif) {
		if (test_bit(CONNECTED, &vif->flag)) {
			if (!flushing)
				netif_wake_queue(vif->net_dev);
		}

		if (wake_event)
			wake_up(&vif->event_wq);
	}

	return;

fatal:
	WARN_ON(1);
	spin_unlock_bh(&ar->lock);
	return;
}

void ath6kl_tx_data_cleanup(struct ath6kl *ar)
{
	int i;

	/* flush all the data (non-control) streams */
	for (i = 0; i < WMM_NUM_AC; i++)
		ath6kl_htc_flush_txep(ar->htc_target, ar->ac2ep_map[i],
				      ATH6KL_DATA_PKT_TAG);
}

/* Rx functions */

static void ath6kl_deliver_frames_to_nw_stack(struct net_device *dev,
					      struct sk_buff *skb)
{
	if (!skb)
		return;

	skb->dev = dev;

	if (!(skb->dev->flags & IFF_UP)) {
		dev_kfree_skb(skb);
		return;
	}

	skb->protocol = eth_type_trans(skb, skb->dev);

	netif_rx_ni(skb);
}

static void ath6kl_alloc_netbufs(struct sk_buff_head *q, u16 num)
{
	struct sk_buff *skb;

	while (num) {
		skb = ath6kl_buf_alloc(ATH6KL_BUFFER_SIZE);
		if (!skb) {
			ath6kl_err("netbuf allocation failed\n");
			return;
		}
		skb_queue_tail(q, skb);
		num--;
	}
}

static struct sk_buff *aggr_get_free_skb(struct aggr_conn_info *aggr_conn)
{
	struct aggr_info *aggr = aggr_conn->aggr_cntxt;
	struct sk_buff *skb = NULL;

	WARN_ON(!aggr);
	if (skb_queue_len(&aggr->free_q) < (AGGR_NUM_OF_FREE_NETBUFS >> 2))
		ath6kl_alloc_netbufs(&aggr->free_q, AGGR_NUM_OF_FREE_NETBUFS);

	skb = skb_dequeue(&aggr->free_q);

	return skb;
}


void ath6kl_rx_refill(struct htc_target *target, enum htc_endpoint_id endpoint)
{
	struct ath6kl *ar = target->dev->ar;
	struct sk_buff *skb;
	int rx_buf;
	int n_buf_refill;
	struct htc_packet *packet;
	struct list_head queue;

	n_buf_refill = ATH6KL_MAX_RX_BUFFERS -
			  ath6kl_htc_get_rxbuf_num(ar->htc_target, endpoint);

	if (n_buf_refill <= 0)
		return;

	INIT_LIST_HEAD(&queue);

	ath6kl_dbg(ATH6KL_DBG_WLAN_RX,
		   "%s: providing htc with %d buffers at eid=%d\n",
		   __func__, n_buf_refill, endpoint);

	for (rx_buf = 0; rx_buf < n_buf_refill; rx_buf++) {
		skb = ath6kl_buf_alloc(ATH6KL_BUFFER_SIZE);
		if (!skb)
			break;

		packet = (struct htc_packet *) skb->head;
		skb->data = PTR_ALIGN(skb->data - 4, 4);
		set_htc_rxpkt_info(packet, skb, skb->data,
				ATH6KL_BUFFER_SIZE, endpoint);
		packet->netbufcontext = skb;

		list_add_tail(&packet->list, &queue);
	}

	if (!list_empty(&queue))
		ath6kl_htc_add_rxbuf_multiple(ar->htc_target, &queue);
}

void ath6kl_refill_amsdu_rxbufs(struct ath6kl *ar, int count)
{
	struct htc_packet *packet;
	struct sk_buff *skb;

	while (count) {
		skb = ath6kl_buf_alloc(ATH6KL_AMSDU_BUFFER_SIZE);
		if (!skb)
			return;

		packet = (struct htc_packet *) skb->head;
		skb->data = PTR_ALIGN(skb->data - 4, 4);
		set_htc_rxpkt_info(packet, skb, skb->data,
				   ATH6KL_AMSDU_BUFFER_SIZE, 0);
		packet->netbufcontext = skb;

		spin_lock_bh(&ar->lock);
		list_add_tail(&packet->list, &ar->amsdu_rx_buffer_queue);
		spin_unlock_bh(&ar->lock);
		count--;
	}
}

/*
 * Callback to allocate a receive buffer for a pending packet. We use a
 * pre-allocated list of buffers of maximum AMSDU size (4K).
 */
struct htc_packet *ath6kl_alloc_amsdu_rxbuf(struct htc_target *target,
					    enum htc_endpoint_id endpoint,
					    int len)
{
	struct ath6kl *ar = target->dev->ar;
	struct htc_packet *packet = NULL;
	struct list_head *pkt_pos;
	int refill_cnt = 0, depth = 0;

	ath6kl_dbg(ATH6KL_DBG_WLAN_RX, "%s: eid=%d, len:%d\n",
		   __func__, endpoint, len);

	if ((len <= ATH6KL_BUFFER_SIZE) ||
	    (len > ATH6KL_AMSDU_BUFFER_SIZE))
		return NULL;

	spin_lock_bh(&ar->lock);

	if (list_empty(&ar->amsdu_rx_buffer_queue)) {
		spin_unlock_bh(&ar->lock);
		refill_cnt = ATH6KL_MAX_AMSDU_RX_BUFFERS;
		goto refill_buf;
	}

	packet = list_first_entry(&ar->amsdu_rx_buffer_queue,
				  struct htc_packet, list);
	list_del(&packet->list);
	list_for_each(pkt_pos, &ar->amsdu_rx_buffer_queue)
		depth++;

	refill_cnt = ATH6KL_MAX_AMSDU_RX_BUFFERS - depth;
	spin_unlock_bh(&ar->lock);

	/* set actual endpoint ID */
	packet->endpoint = endpoint;

refill_buf:
	if (refill_cnt >= ATH6KL_AMSDU_REFILL_THRESHOLD)
		ath6kl_refill_amsdu_rxbufs(ar, refill_cnt);

	return packet;
}

static void aggr_slice_amsdu(struct aggr_conn_info *aggr_conn,
			     struct rxtid *rxtid, struct sk_buff *skb)
{
	struct sk_buff *new_skb;
	struct ethhdr *hdr;
	u16 frame_8023_len, payload_8023_len, mac_hdr_len, amsdu_len;
	u8 *framep;

	mac_hdr_len = sizeof(struct ethhdr);
	framep = skb->data + mac_hdr_len;
	amsdu_len = skb->len - mac_hdr_len;

	while (amsdu_len > mac_hdr_len) {
		hdr = (struct ethhdr *) framep;
		payload_8023_len = ntohs(hdr->h_proto);

		if (payload_8023_len < MIN_MSDU_SUBFRAME_PAYLOAD_LEN ||
		    payload_8023_len > MAX_MSDU_SUBFRAME_PAYLOAD_LEN) {
			ath6kl_err("802.3 AMSDU frame bound check failed. len %d\n",
				   payload_8023_len);
			break;
		}

		frame_8023_len = payload_8023_len + mac_hdr_len;
		new_skb = aggr_get_free_skb(aggr_conn);
		if (!new_skb) {
			ath6kl_err("no buffer available\n");
			break;
		}

		memcpy(new_skb->data, framep, frame_8023_len);
		skb_put(new_skb, frame_8023_len);
		if (ath6kl_wmi_dot3_2_dix(new_skb)) {
			ath6kl_err("dot3_2_dix error\n");
			dev_kfree_skb(new_skb);
			break;
		}

		skb_queue_tail(&rxtid->q, new_skb);

		/* Is this the last subframe within this aggregate ? */
		if ((amsdu_len - frame_8023_len) == 0)
			break;

		/* Add the length of A-MSDU subframe padding bytes -
		 * Round to nearest word.
		 */
		frame_8023_len = ALIGN(frame_8023_len, 4);

		framep += frame_8023_len;
		amsdu_len -= frame_8023_len;
	}

	dev_kfree_skb(skb);
}

static void aggr_deque_frms(struct aggr_conn_info *aggr_conn, u8 tid,
			    u16 seq_no, u8 order)
{
	struct sk_buff *skb;
	struct rxtid *rxtid;
	struct skb_hold_q *node;
	u16 idx, idx_end, seq_end;
	struct rxtid_stats *stats;
	struct net_device *dev;

	if (!aggr_conn)
		return;

	rxtid = AGGR_GET_RXTID(aggr_conn, tid);
	stats = AGGR_GET_RXTID_STATS(aggr_conn, tid);

	idx = AGGR_WIN_IDX(rxtid->seq_next, rxtid->hold_q_sz);

	/*
	 * idx_end is typically the last possible frame in the window,
	 * but changes to 'the' seq_no, when BAR comes. If seq_no
	 * is non-zero, we will go up to that and stop.
	 * Note: last seq no in current window will occupy the same
	 * index position as index that is just previous to start.
	 * An imp point : if win_sz is 7, for seq_no space of 4095,
	 * then, there would be holes when sequence wrap around occurs.
	 * Target should judiciously choose the win_sz, based on
	 * this condition. For 4095, (TID_WINDOW_SZ = 2 x win_sz
	 * 2, 4, 8, 16 win_sz works fine).
	 * We must deque from "idx" to "idx_end", including both.
	 */
	seq_end = seq_no ? seq_no : rxtid->seq_next;
	idx_end = AGGR_WIN_IDX(seq_end, rxtid->hold_q_sz);

	spin_lock_bh(&rxtid->lock);

	do {
		node = &rxtid->hold_q[idx];
		if ((order == 1) && (!node->skb))
			break;

		if (node->skb) {
			if (node->is_amsdu)
				aggr_slice_amsdu(aggr_conn, rxtid, node->skb);
			else
				skb_queue_tail(&rxtid->q, node->skb);
			node->skb = NULL;
		} else
			stats->num_hole++;

		rxtid->seq_next = ATH6KL_NEXT_SEQ_NO(rxtid->seq_next);
		idx = AGGR_WIN_IDX(rxtid->seq_next, rxtid->hold_q_sz);
	} while (idx != idx_end);

	spin_unlock_bh(&rxtid->lock);

	stats->num_delivered += skb_queue_len(&rxtid->q);

	WARN_ON(!aggr_conn->dev);
	dev = aggr_conn->dev;	
	while ((skb = skb_dequeue(&rxtid->q)))
		ath6kl_deliver_frames_to_nw_stack(dev, skb);
}

static bool aggr_process_recv_frm(struct aggr_conn_info *aggr_conn, u8 tid,
				  u16 seq_no,
				  bool is_amsdu, struct sk_buff *frame)
{
	struct rxtid *rxtid;
	struct rxtid_stats *stats;
	struct sk_buff *skb;
	struct skb_hold_q *node;
	u16 idx, st, cur, end;
	bool is_queued = false;
	u16 extended_end;

	rxtid = AGGR_GET_RXTID(aggr_conn, tid);
	stats = AGGR_GET_RXTID_STATS(aggr_conn, tid);

	stats->num_into_aggr++;

	if (!rxtid->aggr) {
		if (is_amsdu) {
			struct net_device *dev;

			aggr_slice_amsdu(aggr_conn, rxtid, frame);
			is_queued = true;
			stats->num_amsdu++;

			WARN_ON(!aggr_conn->dev);
			dev = aggr_conn->dev;
			while ((skb = skb_dequeue(&rxtid->q)))
				ath6kl_deliver_frames_to_nw_stack(dev,
								  skb);
		}
		return is_queued;
	}

	/* Check the incoming sequence no, if it's in the window */
	st = rxtid->seq_next;
	cur = seq_no;
	end = (st + rxtid->hold_q_sz-1) & ATH6KL_MAX_SEQ_NO;

	if (((st < end) && (cur < st || cur > end)) ||
	    ((st > end) && (cur > end) && (cur < st))) {
		extended_end = (end + rxtid->hold_q_sz - 1) &
			ATH6KL_MAX_SEQ_NO;

		if (((end < extended_end) &&
		     (cur < end || cur > extended_end)) ||
		    ((end > extended_end) && (cur > extended_end) &&
		     (cur < end))) {
			aggr_deque_frms(aggr_conn, tid, 0, 0);
			if (cur >= rxtid->hold_q_sz - 1)
				rxtid->seq_next = cur - (rxtid->hold_q_sz - 1);
			else
				rxtid->seq_next = ATH6KL_MAX_SEQ_NO -
						  (rxtid->hold_q_sz - 2 - cur);
		} else {
			/*
			 * Dequeue only those frames that are outside the
			 * new shifted window.
			 */
			if (cur >= rxtid->hold_q_sz - 1)
				st = cur - (rxtid->hold_q_sz - 1);
			else
				st = ATH6KL_MAX_SEQ_NO -
					(rxtid->hold_q_sz - 2 - cur);

			aggr_deque_frms(aggr_conn, tid, st, 0);
		}

		stats->num_oow++;
	}

	idx = AGGR_WIN_IDX(seq_no, rxtid->hold_q_sz);

	node = &rxtid->hold_q[idx];

	spin_lock_bh(&rxtid->lock);

	/*
	 * Is the cur frame duplicate or something beyond our window(hold_q
	 * -> which is 2x, already)?
	 *
	 * 1. Duplicate is easy - drop incoming frame.
	 * 2. Not falling in current sliding window.
	 *  2a. is the frame_seq_no preceding current tid_seq_no?
	 *      -> drop the frame. perhaps sender did not get our ACK.
	 *         this is taken care of above.
	 *  2b. is the frame_seq_no beyond window(st, TID_WINDOW_SZ);
	 *      -> Taken care of it above, by moving window forward.
	 */
	dev_kfree_skb(node->skb);
	stats->num_dups++;

	node->skb = frame;
	is_queued = true;
	node->is_amsdu = is_amsdu;
	node->seq_no = seq_no;

	if (node->is_amsdu)
		stats->num_amsdu++;
	else
		stats->num_mpdu++;

	spin_unlock_bh(&rxtid->lock);

	aggr_deque_frms(aggr_conn, tid, 0, 1);
    
    if(aggr_conn->timer_scheduled &&
       rxtid->timerwait_seq_num != rxtid->seq_next) {
        del_timer(&aggr_conn->timer);
	    aggr_conn->timer_scheduled = false;
    }

	if (aggr_conn->timer_scheduled)
		rxtid->progress = true;
	else
		for (idx = 0 ; idx < rxtid->hold_q_sz; idx++) {
			if (rxtid->hold_q[idx].skb) {
				/*
				 * There is a frame in the queue and no
				 * timer so start a timer to ensure that
				 * the frame doesn't remain stuck
				 * forever.
				 */
				aggr_conn->timer_scheduled = true;
                rxtid->timerwait_seq_num = rxtid->seq_next;
				mod_timer(&aggr_conn->timer,
					  (jiffies +
					   HZ * (AGGR_RX_TIMEOUT) / 1000));
				rxtid->progress = false;
				rxtid->timer_mon = true;
				break;
			}
		}

	return is_queued;
}

static void ath6kl_uapsd_trigger_frame_rx(struct ath6kl_vif *vif, struct ath6kl_sta *conn)
{
	bool is_psq_empty;
	bool is_psq_empty_at_start;
	u32 num_frames_to_deliver;

	/* If the APSD q for this STA is not empty, dequeue and send a pkt from
	 * the head of the q. Also update the More data bit in the WMI_DATA_HDR
	 * if there are more pkts for this STA in the APSD q. If there are no more
	 * pkts for this STA, update the APSD bitmap for this STA.
	 */

	num_frames_to_deliver = (conn->apsd_info >> 4) & 0xF;

	/* Number of frames to send in a service period is indicated by the station
	 * in the QOS_INFO of the association request
	 * If it is zero, send all frames
	 */
	if (!num_frames_to_deliver) {
		num_frames_to_deliver = 0xFFFF;
	}

	spin_lock_bh(&conn->lock);
	is_psq_empty  = skb_queue_empty(&conn->psq) && ath6kl_mgmt_queue_empty(&conn->mgmt_psq);
	spin_unlock_bh(&conn->lock);
	is_psq_empty_at_start  = is_psq_empty;
	while ((!is_psq_empty) && (num_frames_to_deliver)) {
		spin_lock_bh(&conn->lock);
		if (!ath6kl_mgmt_queue_empty(&conn->mgmt_psq)) {
			struct mgmt_buff* mgmt_buf;
			mgmt_buf = ath6kl_mgmt_dequeue_head(&conn->mgmt_psq);
			is_psq_empty  = skb_queue_empty(&conn->psq) && ath6kl_mgmt_queue_empty(&conn->mgmt_psq);
			spin_unlock_bh(&conn->lock);
			
			/* Set the STA flag to Trigger delivery, so that the frame will go out */
			conn->sta_flags |= STA_PS_APSD_TRIGGER;
			num_frames_to_deliver--;

			/* Last frame in the service period, set EOSP or queue empty */
			if ((is_psq_empty) || (!num_frames_to_deliver)) {
				conn->sta_flags |= STA_PS_APSD_EOSP;
			}
			ath6kl_wmi_send_action_cmd(vif,
						mgmt_buf->id,
						mgmt_buf->freq,
						mgmt_buf->wait,
						mgmt_buf->buf,
						mgmt_buf->len);
			kfree(mgmt_buf);
			conn->sta_flags &= ~(STA_PS_APSD_TRIGGER);
			conn->sta_flags &= ~(STA_PS_APSD_EOSP);
		} else {
			struct sk_buff *skb;

			skb = skb_dequeue(&conn->psq);
			is_psq_empty  = skb_queue_empty(&conn->psq) && ath6kl_mgmt_queue_empty(&conn->mgmt_psq);
			spin_unlock_bh(&conn->lock);

			/* Set the STA flag to Trigger delivery, so that the frame will go out */
			conn->sta_flags |= STA_PS_APSD_TRIGGER;
			num_frames_to_deliver--;

			/* Last frame in the service period, set EOSP or queue empty */
			if ((is_psq_empty) || (!num_frames_to_deliver)) {
				conn->sta_flags |= STA_PS_APSD_EOSP;
			}
			ath6kl_data_tx(skb, vif->net_dev);
			conn->sta_flags &= ~(STA_PS_APSD_TRIGGER);
			conn->sta_flags &= ~(STA_PS_APSD_EOSP);
		}
	}

	if (is_psq_empty) {
		ath6kl_wmi_set_pvb_cmd(vif, conn->aid, 0);
		if (is_psq_empty_at_start) {
			ath6kl_wmi_set_apsd_buffered_traffic_cmd(vif, conn->aid, 0,
						 WMI_AP_APSD_NO_DELIVERY_FRAMES_FOR_THIS_TRIGGER);
		} else {
			ath6kl_wmi_set_apsd_buffered_traffic_cmd(vif, conn->aid, 0, 0);
		}
	}

	return;
}

void ath6kl_rx(struct htc_target *target, struct htc_packet *packet)
{
	struct ath6kl *ar = target->dev->ar;
	struct ath6kl_vif *vif;
	struct sk_buff *skb = packet->pkt_cntxt;
	struct wmi_rx_meta_v2 *meta;
	struct wmi_data_hdr *dhdr;
	int min_hdr_len;
	u8 meta_type, dot11_hdr = 0;
	u8 pad_before_data_start;
	int status = packet->status;
	enum htc_endpoint_id ept = packet->endpoint;
	bool is_amsdu, prev_ps, ps_state = false;
        bool trigger_state = false;
	struct ath6kl_sta *conn = NULL;
	struct sk_buff *skb1 = NULL;
	struct ethhdr *datap = NULL;
	u16 seq_no;
	u8 tid, devid;

	ath6kl_dbg(ATH6KL_DBG_WLAN_RX,
		   "%s: ar=0x%p eid=%d, skb=0x%p, data=0x%p, len=0x%x status:%d",
		   __func__, ar, ept, skb, packet->buf,
		   packet->act_len, status);

	if (status || !(skb->data + HTC_HDR_LENGTH)) {
		dev_kfree_skb(skb);
		return;
	}

	skb_put(skb, packet->act_len + HTC_HDR_LENGTH);
	skb_pull(skb, HTC_HDR_LENGTH);

	/*
	 * Take lock to protect buffer counts and adaptive power throughput
	 * state.
	 */

        if (ept == ar->ctrl_ep) {
                struct wmi_cmd_hdr *cmd;
                cmd  = (struct wmi_cmd_hdr *) skb->data;
                devid = le16_to_cpu(cmd->info1) & WMI_CMD_HDR_IF_ID_MASK;
                vif = ar->vif[devid];
        } else {
                struct wmi_data_hdr *data;
                data = (struct wmi_data_hdr *) skb->data;
                devid = le16_to_cpu(data->info3) & WMI_DATA_HDR_IF_IDX_MASK;
                vif = ar->vif[devid];
        }

        if (vif == NULL) {
		dev_kfree_skb(skb);
		return;
	}

	spin_lock_bh(&ar->lock);

	vif->net_stats.rx_packets++;
	vif->net_stats.rx_bytes += packet->act_len;

	spin_unlock_bh(&ar->lock);


	ath6kl_dbg_dump(ATH6KL_DBG_RAW_BYTES, __func__, skb->data, skb->len);

	skb->dev = vif->net_dev;

	if (!test_bit(WMI_ENABLED, &ar->flag)) {
		if (EPPING_ALIGNMENT_PAD > 0)
			skb_pull(skb, EPPING_ALIGNMENT_PAD);
		ath6kl_deliver_frames_to_nw_stack(vif->net_dev, skb);
		return;
	}

	if (ept == ar->ctrl_ep) {
		ath6kl_wmi_control_rx(vif, skb);
		return;
	}

	min_hdr_len = sizeof(struct ethhdr) + sizeof(struct wmi_data_hdr) +
		      sizeof(struct ath6kl_llc_snap_hdr);

	dhdr = (struct wmi_data_hdr *) skb->data;

	is_amsdu = wmi_data_hdr_is_amsdu(dhdr) ? true : false;
	tid = wmi_data_hdr_get_up(dhdr);
	seq_no = wmi_data_hdr_get_seqno(dhdr);
	meta_type = wmi_data_hdr_get_meta(dhdr);
	dot11_hdr = wmi_data_hdr_get_dot11(dhdr);
	pad_before_data_start =
		(dhdr->info3 >> WMI_DATA_HDR_PAD_BEFORE_DATA_SHIFT)
			& WMI_DATA_HDR_PAD_BEFORE_DATA_MASK;
	/*
	 * In the case of AP mode we may receive NULL data frames
	 * that do not have LLC hdr. They are 16 bytes in size.
	 * Allow these frames in the AP mode.
	 */
	if (vif->nw_type != AP_NETWORK &&
	    ((packet->act_len < min_hdr_len) ||
	     (packet->act_len > WMI_MAX_AMSDU_RX_DATA_FRAME_LENGTH))) {
		ath6kl_info("frame len is too short or too long\n");
		vif->net_stats.rx_errors++;
		vif->net_stats.rx_length_errors++;
		dev_kfree_skb(skb);
		return;
	}

	skb_pull(skb, sizeof(struct wmi_data_hdr));

	switch (meta_type) {
	case WMI_META_VERSION_1:
		skb_pull(skb, sizeof(struct wmi_rx_meta_v1));
		break;
	case WMI_META_VERSION_2:
		meta = (struct wmi_rx_meta_v2 *) skb->data;
		if (meta->csum_flags & 0x1) {
			skb->ip_summed = CHECKSUM_COMPLETE;
			skb->csum = (__force __wsum) meta->csum;
		}
		skb_pull(skb, sizeof(struct wmi_rx_meta_v2));
		break;
	default:
		break;
	}

	skb_pull(skb, pad_before_data_start);

	if (dot11_hdr)
		status = ath6kl_wmi_dot11_hdr_remove(ar->wmi, skb);
	else if (!is_amsdu)
		status = ath6kl_wmi_dot3_2_dix(skb);

	if (status) {
		/*
		 * Drop frames that could not be processed (lack of
		 * memory, etc.)
		 */
		dev_kfree_skb(skb);
		return;
	}

	/* Get the Power save state of the STA */
        if (vif->nw_type == AP_NETWORK) { meta_type = wmi_data_hdr_get_meta(dhdr);

                ps_state = !!((dhdr->info >> WMI_DATA_HDR_PS_SHIFT) &
                              WMI_DATA_HDR_PS_MASK);

		trigger_state = WMI_DATA_HDR_IS_TRIGGER(dhdr);

                datap = (struct ethhdr *) (skb->data);
                conn = ath6kl_find_sta(vif, datap->h_source);
                if (!conn) {
                        dev_kfree_skb(skb);
                        return;
                }

                /*
                 * If there is a change in PS state of the STA,
                 * take appropriate steps:
                 *
                 * 1. If Sleep-->Awake, flush the psq for the STA
                 *    Clear the PVB for the STA.
                 * 2. If Awake-->Sleep, Starting queueing frames
                 *    the STA.
                 */
                prev_ps = !!(conn->sta_flags & STA_PS_SLEEP);

                if (ps_state) {
                        conn->sta_flags |= STA_PS_SLEEP;
			if (!prev_ps) {
				ath6kl_psq_age_start(conn);
			}
		} else {
                        conn->sta_flags &= ~STA_PS_SLEEP;
			if (prev_ps) {
				ath6kl_psq_age_stop(conn);
			}
		}

		if (conn->sta_flags & STA_PS_SLEEP) {
			/* Accept trigger only when the station is in sleep */
			if (trigger_state) {
				ath6kl_uapsd_trigger_frame_rx(vif, conn);
			}
		}

                if (prev_ps ^ !!(conn->sta_flags & STA_PS_SLEEP)) {
                        if (!(conn->sta_flags & STA_PS_SLEEP)) {
                                struct sk_buff *skbuff = NULL;
                                struct mgmt_buff* mgmt_buf;
				bool is_psq_empty_at_start;

                                spin_lock_bh(&conn->lock);
				is_psq_empty_at_start = skb_queue_empty(&conn->psq) && ath6kl_mgmt_queue_empty(&conn->mgmt_psq);
                                while ((mgmt_buf = ath6kl_mgmt_dequeue_head(&conn->mgmt_psq)) != NULL) {
                                        spin_unlock_bh(&conn->lock);
                                        ath6kl_wmi_send_action_cmd(vif,
                                                                   mgmt_buf->id,
                                                                   mgmt_buf->freq,
                                                                   mgmt_buf->wait,
                                                                   mgmt_buf->buf,
                                                                   mgmt_buf->len);
                                        kfree(mgmt_buf);
                                        spin_lock_bh(&conn->lock);
                                }

                                while ((skbuff = skb_dequeue(&conn->psq)) != NULL) {
                                        spin_unlock_bh(&conn->lock);
                                        ath6kl_data_tx(skbuff, vif->net_dev);
                                        spin_lock_bh(&conn->lock);
                                }
                                spin_unlock_bh(&conn->lock);

				if (!is_psq_empty_at_start) {
					ath6kl_wmi_set_apsd_buffered_traffic_cmd(vif, conn->aid, 0, 0);
				}
                                /* Clear the PVB for this STA */
                                ath6kl_wmi_set_pvb_cmd(vif, conn->aid, 0);
                        }
                }

                /* drop NULL data frames here */
                if ((packet->act_len < min_hdr_len) ||
                    (packet->act_len >
                     WMI_MAX_AMSDU_RX_DATA_FRAME_LENGTH)) {
                        dev_kfree_skb(skb);
                        return;
                }
        }

	if (!(vif->net_dev->flags & IFF_UP)) {
		dev_kfree_skb(skb);
		return;
	}

	if (vif->nw_type == AP_NETWORK) {
		datap = (struct ethhdr *) skb->data;
		if (is_multicast_ether_addr(datap->h_dest))
			/*
			 * Bcast/Mcast frames should be sent to the
			 * OS stack as well as on the air.
			 */
			skb1 = skb_copy(skb, GFP_ATOMIC);
		else {
			/*
			 * Search for a connected STA with dstMac
			 * as the Mac address. If found send the
			 * frame to it on the air else send the
			 * frame up the stack.
			 */
			struct ath6kl_sta *to_conn = NULL;
			to_conn = ath6kl_find_sta(vif, datap->h_dest);

			if (to_conn && vif->intra_bss) {
				skb1 = skb;
				skb = NULL;
			} else if (to_conn && !vif->intra_bss) {
				dev_kfree_skb(skb);
				skb = NULL;
			}
		}
		if (skb1)
			ath6kl_data_tx(skb1, vif->net_dev);
	}

	if (vif->nw_type != AP_NETWORK) 
		conn = &vif->sta_list[0];

	if (skb == NULL)
		return;

	datap = (struct ethhdr *) skb->data;

	if (is_unicast_ether_addr(datap->h_dest) &&
	    aggr_process_recv_frm(conn->aggr_conn_cntxt, tid, seq_no,
				  is_amsdu, skb))
		/* aggregation code will handle the skb */
		return;

	ath6kl_deliver_frames_to_nw_stack(vif->net_dev, skb);	
}

static void aggr_tx_reset_aggr(struct txtid *txtid, bool free_buf, bool timer_stop)
{
	/* Need protected by tid->lock. */
	if (timer_stop)
		del_timer(&txtid->timer);
	
	if ((free_buf) &&
		(txtid->amsdu_skb))
		dev_kfree_skb(txtid->amsdu_skb);

	txtid->amsdu_skb = NULL;
	txtid->amsdu_start = NULL;
	txtid->amsdu_cnt = 0;
	txtid->amsdu_len = 0;
	txtid->amsdu_lastpdu_len = 0;

	return;
}

static void aggr_tx_delete_tid_state(struct aggr_conn_info *aggr_conn, u8 tid)
{
	struct txtid *txtid;

	txtid = AGGR_GET_TXTID(aggr_conn, tid);

	spin_lock_bh(&txtid->lock);
	txtid->aid = 0;
	txtid->max_aggr_sz = 0;

	aggr_tx_reset_aggr(txtid, true, true);

	txtid->num_pdu = 0;
	txtid->num_amsdu = 0;
	txtid->num_timeout = 0;
	txtid->num_flush = 0;
	txtid->num_tx_null = 0;
	spin_unlock_bh(&txtid->lock);

	return;
}

static int aggr_tx(struct ath6kl_vif *vif, struct sk_buff **skb)
{
#define	ETHERTYPE_IP	0x0800		/* IP  protocol */
#define IP_PROTO_TCP	0x6			/* TCP protocol */
	struct ethhdr *eth_hdr;
	struct ath6kl_llc_snap_hdr *llc_hdr;
	struct aggr_info *aggr;
	struct txtid *txtid;
	int pdu_len, subframe_len;
	int hdr_len = /*WMI_MAX_TX_META_SZ + */sizeof(struct wmi_data_hdr);

	pdu_len = (*skb)->len - hdr_len;
	aggr = vif->aggr_cntxt;

	/* 
	 *	Only aggr IP/TCP frames, focus on small TCP-ACK frams.
	 *  Bypass multicast and non-IP/TCP frames.
	 *
	 *  Reserved 14 bytes 802.3 header ahead of A-MSDU frame for target 
	 *  to transfer to 802.11 header.
	 *
	 *  TBD : Check whether skb's size is enough.
	 */
	if (pdu_len > aggr->tx_amsdu_max_pdu_len)
		return AGGR_TX_BYPASS;

	eth_hdr = (struct ethhdr *)((*skb)->data + hdr_len);
	if (is_multicast_ether_addr(eth_hdr->h_dest))
		return AGGR_TX_BYPASS;

	llc_hdr = (struct ath6kl_llc_snap_hdr *)((*skb)->data + hdr_len + sizeof(struct ethhdr));
	if (llc_hdr->eth_type == htons(ETHERTYPE_IP)) {
		struct iphdr *ip_hdr = (struct iphdr *)((u8 *)eth_hdr + sizeof(struct ethhdr) + sizeof(struct ath6kl_llc_snap_hdr));

		if (ip_hdr->protocol == IP_PROTO_TCP) {
			struct ath6kl_sta *conn = ath6kl_find_sta(vif, eth_hdr->h_dest);

			ath6kl_dbg_dump(ATH6KL_DBG_RAW_BYTES, __func__, (*skb)->data, (*skb)->len);

			if (conn) {
				struct sk_buff *amsdu_skb;
				struct wmi_data_hdr *wmi_hdr = (struct wmi_data_hdr *)((u8 *)eth_hdr - sizeof(struct wmi_data_hdr));

				txtid = AGGR_GET_TXTID(conn->aggr_conn_cntxt, ((wmi_hdr->info >> WMI_DATA_HDR_UP_SHIFT) & WMI_DATA_HDR_UP_MASK));

				spin_lock_bh(&txtid->lock);
				if (!txtid->max_aggr_sz) {
					spin_unlock_bh(&txtid->lock);
					return AGGR_TX_BYPASS;
				}

				amsdu_skb = txtid->amsdu_skb;
				if (amsdu_skb == NULL) {
					amsdu_skb = dev_alloc_skb(AGGR_TX_MAX_AGGR_SIZE);
					if (amsdu_skb == NULL) {
						spin_unlock_bh(&txtid->lock);
						return AGGR_TX_BYPASS;
					}

					/* Change to A-MSDU type */
					wmi_hdr->info |= (WMI_DATA_HDR_DATA_TYPE_AMSDU << WMI_DATA_HDR_DATA_TYPE_SHIFT);

					/* Clone meta-data & WMI-header. */
					memcpy(amsdu_skb->data - hdr_len, (*skb)->data, hdr_len);

					aggr_tx_reset_aggr(txtid, false, true);
					txtid->amsdu_skb = amsdu_skb;
					txtid->amsdu_start = amsdu_skb->data;
					
					amsdu_skb->data += sizeof(struct ethhdr);

					/* Start tx timeout timer */
					mod_timer(&txtid->timer, jiffies + msecs_to_jiffies(aggr->tx_amsdu_timeout));		
				}

				/* Zero padding */
				subframe_len = roundup(pdu_len, 4);
				memset(amsdu_skb->data + subframe_len - 4, 0, 4);		

				/* Append PDU to A-MSDU */
				memcpy(amsdu_skb->data, eth_hdr, pdu_len);
				amsdu_skb->len += subframe_len;
				amsdu_skb->data += subframe_len;

				ath6kl_dbg(ATH6KL_DBG_WLAN_TX_AMSDU,
							   "%s: subframe_len=%d, tid=%d, amsdu_cnt=%d, amsdu_len=%d\n", __func__,
							   subframe_len,
							   ((wmi_hdr->info >> WMI_DATA_HDR_UP_SHIFT) & WMI_DATA_HDR_UP_MASK),
							   txtid->amsdu_cnt, amsdu_skb->len);

				txtid->amsdu_cnt++;
				txtid->amsdu_lastpdu_len = pdu_len;

				dev_kfree_skb(*skb);
				*skb = NULL;

				if (txtid->amsdu_cnt >= aggr->tx_amsdu_max_aggr_num) {
					/* No padding in last MSDU */
					amsdu_skb->len -= (4 - (pdu_len % 4));

					/* Update A-MSDU frame header */
					eth_hdr = (struct ethhdr *)txtid->amsdu_start;
					if (vif->nw_type == INFRA_NETWORK) {
						memcpy(eth_hdr->h_dest, vif->bssid, ETH_ALEN);
						memcpy(eth_hdr->h_source, vif->net_dev->dev_addr, ETH_ALEN);
					} else {
						memcpy(eth_hdr->h_dest, conn->mac, ETH_ALEN);
						memcpy(eth_hdr->h_source, vif->bssid, ETH_ALEN);
					}
					eth_hdr->h_proto = htons(amsdu_skb->len);

					/* Correct final skb's data and length. */
					amsdu_skb->len += (hdr_len + sizeof(struct ethhdr));
					amsdu_skb->data = txtid->amsdu_start - hdr_len;

					ath6kl_dbg_dump(ATH6KL_DBG_RAW_BYTES, __func__, amsdu_skb->data, amsdu_skb->len);

					/* update stat. */
					txtid->num_amsdu++;
					txtid->num_pdu += txtid->amsdu_cnt;

					*skb = amsdu_skb;
					aggr_tx_reset_aggr(txtid, false, true);
					spin_unlock_bh(&txtid->lock);

					return AGGR_TX_DONE;
				} else {
					spin_unlock_bh(&txtid->lock);
					return AGGR_TX_OK;
				}
			} else		
				return AGGR_TX_DROP;
		}
    }

	return AGGR_TX_BYPASS;
}

static int aggr_tx_tid(struct txtid *txtid, bool timer_stop)
{
	struct ath6kl_vif *vif = txtid->vif;
	struct ath6kl *ar = vif->ar;
	struct ath6kl_cookie *cookie;
	enum htc_endpoint_id eid;
	struct wmi_data_hdr *wmi_hdr;
	struct sk_buff *amsdu_skb, *skb = NULL;
	struct ethhdr *eth_hdr;
	int ac;
	int hdr_len = /*WMI_MAX_TX_META_SZ + */sizeof(struct wmi_data_hdr);

	spin_lock_bh(&txtid->lock);
	amsdu_skb = txtid->amsdu_skb;
	if (amsdu_skb == NULL) {
		txtid->num_tx_null++;
		spin_unlock_bh(&txtid->lock);
		//ath6kl_err("aggr_tx_tid NULL, tid = %d aid = %d\n", txtid->tid, txtid->aid);
		return -EINVAL;
	}

	if (timer_stop)
		txtid->num_flush++;
	else
		txtid->num_timeout++;

	ath6kl_dbg(ATH6KL_DBG_WLAN_TX_AMSDU,
		   "%s: amsdu_skb=0x%p, data=0x%p, len=0x%x, amsdu_cnt=%d\n", __func__,
		   amsdu_skb, amsdu_skb->data, amsdu_skb->len, txtid->amsdu_cnt);

	/* No padding in last MSDU */
	amsdu_skb->len -= (4 - (txtid->amsdu_lastpdu_len % 4));
					
	/* Update A-MSDU frame header */
	eth_hdr = (struct ethhdr *)txtid->amsdu_start;
	if (vif->nw_type == INFRA_NETWORK) {
		memcpy(eth_hdr->h_dest, vif->bssid, ETH_ALEN);
		memcpy(eth_hdr->h_source, vif->net_dev->dev_addr, ETH_ALEN);
	} else {
		struct ath6kl_sta *conn = ath6kl_find_sta_by_aid(vif, txtid->aid);

		if (!conn) {
			memcpy(eth_hdr->h_dest, conn->mac, ETH_ALEN);
			memcpy(eth_hdr->h_source, vif->bssid, ETH_ALEN);
		} else {
			aggr_tx_reset_aggr(txtid, true, timer_stop);
			spin_unlock_bh(&txtid->lock);

			ath6kl_err("aggr_tx_tid error, no STA found, AID = %d\n", 
							txtid->aid);
			return -EINVAL;
		}
	}
	eth_hdr->h_proto = htons(amsdu_skb->len);

	/* Correct final skb's data and length. */
	amsdu_skb->len += (hdr_len + sizeof(struct ethhdr));
	amsdu_skb->data = txtid->amsdu_start - hdr_len;

	/* update stat. */
	txtid->num_amsdu++;
	txtid->num_pdu += txtid->amsdu_cnt;

	skb = amsdu_skb;
	aggr_tx_reset_aggr(txtid, false, timer_stop);
	spin_unlock_bh(&txtid->lock);

	spin_lock_bh(&ar->lock);
	wmi_hdr = (struct wmi_data_hdr *)(skb->data + hdr_len - sizeof(struct wmi_data_hdr));
	ac = up_to_ac[(wmi_hdr->info >> WMI_DATA_HDR_UP_SHIFT) & WMI_DATA_HDR_UP_MASK];
	eid = ar->ac2ep_map[ac];

	ath6kl_dbg(ATH6KL_DBG_WLAN_TX_AMSDU,
		   "%s: eid=%d, ac=%d\n", __func__, eid, ac);

	if (eid == 0 || eid == ENDPOINT_UNUSED) {
		ath6kl_err("eid %d is not mapped!\n", eid);
		spin_unlock_bh(&ar->lock);
		goto fail_tx;
	}

	/* allocate resource for this packet */
	cookie = ath6kl_alloc_cookie(ar);

	if (!cookie) {
		spin_unlock_bh(&ar->lock);
		goto fail_tx;
	}

	/* update counts while the lock is held */
	ar->tx_pending[eid]++;
	ar->total_tx_data_pend++;

	spin_unlock_bh(&ar->lock);

	cookie->skb = skb;
	cookie->map_no = 0;
	set_htc_pkt_info(&cookie->htc_pkt, cookie, skb->data, skb->len,
			 eid, ATH6KL_DATA_PKT_TAG);
	cookie->htc_pkt.netbufcontext = skb;

	SKB_CB_VIF(skb) = vif;

	ath6kl_dbg_dump(ATH6KL_DBG_RAW_BYTES, __func__, skb->data, skb->len);

	/*
	 * HTC interface is asynchronous, if this fails, cleanup will
	 * happen in the ath6kl_tx_complete callback.
	 */
	ath6kl_htc_tx(ar->htc_target, &cookie->htc_pkt);

	return 0;

fail_tx:
	dev_kfree_skb(skb);

	vif->net_stats.tx_dropped++;
	vif->net_stats.tx_aborted_errors++;

	return -EINVAL;
}

static void aggr_tx_timeout(unsigned long arg)
{
	struct txtid *txtid = (struct txtid *)arg;

	ath6kl_dbg(ATH6KL_DBG_WLAN_TX_AMSDU,
		   "%s: aid %d, tid %d", __func__, txtid->aid, txtid->tid);

	aggr_tx_tid(txtid, false);

	return;
}

static int aggr_tx_flush(struct ath6kl_vif *vif)
{
	int sta_num = (vif->nw_type == AP_NETWORK) ? AP_MAX_NUM_STA : 1;
	int i, tid;

	/* TBD : Only flush the same DA in AP mode. */
	for (i = 0; i < sta_num; i++) {
		struct ath6kl_sta *conn = &vif->sta_list[i];

		for (tid = (NUM_OF_TIDS - 1); tid >= 0; tid--) {
			struct txtid *txtid = AGGR_GET_TXTID(conn->aggr_conn_cntxt, tid);

			ath6kl_dbg(ATH6KL_DBG_WLAN_TX_AMSDU, "%s: flush sta-%d %d\n", __func__, i, sta_num);
			aggr_tx_tid(txtid, true);
		}
	}

	return 0;
}

static void aggr_timeout(unsigned long arg)
{
	u8 i, j;
	struct aggr_conn_info *aggr_conn = (struct aggr_conn_info *) arg;
	struct rxtid *rxtid;
	struct rxtid_stats *stats;
    u16 pushseq;

	for (i = 0; i < NUM_OF_TIDS; i++) {
		rxtid = AGGR_GET_RXTID(aggr_conn, i);
		stats = AGGR_GET_RXTID_STATS(aggr_conn, i);

		if (!rxtid->aggr || !rxtid->timer_mon)
			continue;

		/*
		 * FIXME: these timeouts happen quite fruently, something
		 * line once within 60 seconds. Investigate why.
		 */
        if(rxtid->timerwait_seq_num == rxtid->seq_next ||
           rxtid->progress == false) {
            stats->num_timeouts++;
            pushseq = (rxtid->seq_next + rxtid->hold_q_sz/8) & 
                        ATH6KL_MAX_SEQ_NO;
            ath6kl_dbg(ATH6KL_DBG_AGGR,
                   "aggr timeout (st %d end %d)\n",
                   rxtid->seq_next,
                   ((rxtid->seq_next + rxtid->hold_q_sz-1) &
                    ATH6KL_MAX_SEQ_NO));
            aggr_deque_frms(aggr_conn, i, pushseq, 0);
        }
	}

	aggr_conn->timer_scheduled = false;

	for (i = 0; i < NUM_OF_TIDS; i++) {
		rxtid = AGGR_GET_RXTID(aggr_conn, i);

		if (rxtid->aggr && rxtid->hold_q) {
			for (j = 0; j < rxtid->hold_q_sz; j++) {
				if (rxtid->hold_q[j].skb) {
                    rxtid->timerwait_seq_num = rxtid->seq_next;
					aggr_conn->timer_scheduled = true;
					rxtid->timer_mon = true;
					rxtid->progress = false;
					break;
				}
			}

			if (j >= rxtid->hold_q_sz)
				rxtid->timer_mon = false;
		}
	}

	if (aggr_conn->timer_scheduled)
		mod_timer(&aggr_conn->timer,
			  jiffies + msecs_to_jiffies(AGGR_RX_TIMEOUT_SHORT));
}

static void aggr_delete_tid_state(struct aggr_conn_info *aggr_conn, u8 tid)
{
	struct rxtid *rxtid;
	struct rxtid_stats *stats;

	if (!aggr_conn || tid >= NUM_OF_TIDS)
		return;

	rxtid = AGGR_GET_RXTID(aggr_conn, tid);
	stats = AGGR_GET_RXTID_STATS(aggr_conn, tid);


	if (rxtid->aggr)
		aggr_deque_frms(aggr_conn, tid, 0, 0);

	rxtid->aggr = false;
	rxtid->progress = false;
	rxtid->timer_mon = false;
	rxtid->win_sz = 0;
	rxtid->seq_next = 0;
	rxtid->hold_q_sz = 0;

	kfree(rxtid->hold_q);
	rxtid->hold_q = NULL;

	memset(stats, 0, sizeof(struct rxtid_stats));
}

void aggr_recv_addba_req_evt(struct ath6kl_vif *vif, u8 tid, u16 seq_no, u8 win_sz)
{
	struct aggr_conn_info *aggr_conn;
	struct rxtid *rxtid;
	struct rxtid_stats *stats;
	struct ath6kl_sta *conn;
	u16 hold_q_size;
	u8 conn_tid, conn_aid;

	conn_tid = AGGR_BA_EVT_GET_TID(tid);
	conn_aid = AGGR_BA_EVT_GET_CONNID(tid);
	conn = ath6kl_find_sta_by_aid(vif, conn_aid);

	if (conn_tid >= NUM_OF_TIDS) {
		WARN_ON(1);
		return;
	}

	if (conn != NULL) {
		WARN_ON(!conn->aggr_conn_cntxt);

		aggr_conn = conn->aggr_conn_cntxt;
		rxtid = AGGR_GET_RXTID(aggr_conn, conn_tid);
		stats = AGGR_GET_RXTID_STATS(aggr_conn, conn_tid);

		if (win_sz < AGGR_WIN_SZ_MIN || win_sz > AGGR_WIN_SZ_MAX)
			ath6kl_dbg(ATH6KL_DBG_WLAN_RX, "%s: win_sz %d, tid %d, aid %d\n",
				   __func__, win_sz, conn_tid, conn_aid);

		if (rxtid->aggr)
			aggr_delete_tid_state(aggr_conn, conn_tid);

		rxtid->seq_next = seq_no;
		hold_q_size = TID_WINDOW_SZ(win_sz) * sizeof(struct skb_hold_q);
		rxtid->hold_q = kzalloc(hold_q_size, GFP_KERNEL);
		if (!rxtid->hold_q)
			return;

		rxtid->win_sz = win_sz;
		rxtid->hold_q_sz = TID_WINDOW_SZ(win_sz);
		if (!skb_queue_empty(&rxtid->q))
			return;

		rxtid->aggr = true;
	}
}

void aggr_recv_addba_resp_evt(struct ath6kl_vif *vif, u8 tid, u16 amsdu_sz, u8 status)
{
	struct aggr_conn_info *aggr_conn;
	struct txtid *txtid;
	struct ath6kl_sta *conn;
	u8 i, conn_tid, conn_aid;

	conn_tid = AGGR_BA_EVT_GET_TID(tid);
	conn_aid = AGGR_BA_EVT_GET_CONNID(tid);
	conn = ath6kl_find_sta_by_aid(vif, conn_aid);

	if (conn_tid >= NUM_OF_TIDS) {
		WARN_ON(1);
		return;
	}

	if (conn != NULL) {
		WARN_ON(!conn->aggr_conn_cntxt);

		ath6kl_dbg(ATH6KL_DBG_WMI, "%s: amsdu_sz %d, tid %d, aid %d, status %d\n",
				   __func__, amsdu_sz, conn_tid, conn_aid, status);

		aggr_conn = conn->aggr_conn_cntxt;
		txtid = AGGR_GET_TXTID(aggr_conn, conn_tid);

		spin_lock_bh(&txtid->lock);
		txtid->aid = conn_aid;
		if (status == 0)
			txtid->max_aggr_sz = amsdu_sz;
		else 
			txtid->max_aggr_sz = 0;			

		/* 0 means disable */
		if (!txtid->max_aggr_sz)
			aggr_tx_reset_aggr(txtid, true, true);
		spin_unlock_bh(&txtid->lock);

		if (vif->nw_type != AP_NETWORK) {
			vif->aggr_cntxt->tx_amsdu_enable = false;
			for (i = 0; i < NUM_OF_TIDS; i++) {
				txtid = AGGR_GET_TXTID(aggr_conn, i);
				if (txtid->max_aggr_sz) {
					vif->aggr_cntxt->tx_amsdu_enable = true;
					break;
				}
			}
		}
	}
}

struct aggr_info *aggr_init(struct ath6kl_vif *vif)
{
	struct aggr_info *aggr = NULL;

	aggr = kzalloc(sizeof(struct aggr_info), GFP_KERNEL);
	if (!aggr) {
		ath6kl_err("failed to alloc memory for aggr_module\n");
		return NULL;
	}

	aggr->vif = vif;

	skb_queue_head_init(&aggr->free_q);
	ath6kl_alloc_netbufs(&aggr->free_q, AGGR_NUM_OF_FREE_NETBUFS);

	aggr->tx_amsdu_enable = true;
	aggr->tx_amsdu_seq_pkt = true;
	aggr->tx_amsdu_max_aggr_num = AGGR_TX_MAX_NUM;
	aggr->tx_amsdu_max_pdu_len = AGGR_TX_MAX_PDU_SIZE;
	aggr->tx_amsdu_timeout = AGGR_TX_TIMEOUT;

	/* Always enable host-based A-MSDU. */
	set_bit(AMSDU_ENABLED, &vif->flag);

	return aggr;
}

struct aggr_conn_info *aggr_init_conn(struct ath6kl_vif *vif)
{
	struct aggr_conn_info *aggr_conn = NULL;
	struct rxtid *rxtid;
	struct txtid *txtid;
	u8 i;

	aggr_conn = kzalloc(sizeof(struct aggr_conn_info), GFP_KERNEL);
	if (!aggr_conn) {
		ath6kl_err("failed to alloc memory for aggr_node\n");
		return NULL;
	}

	aggr_conn->aggr_sz = AGGR_SZ_DEFAULT;
	aggr_conn->aggr_cntxt = vif->aggr_cntxt;
	aggr_conn->dev = vif->net_dev;
	init_timer(&aggr_conn->timer);
	aggr_conn->timer.function = aggr_timeout;
	aggr_conn->timer.data = (unsigned long) aggr_conn;

	aggr_conn->timer_scheduled = false;

	for (i = 0; i < NUM_OF_TIDS; i++) {
		rxtid = AGGR_GET_RXTID(aggr_conn, i);
		rxtid->aggr = false;
		rxtid->progress = false;
		rxtid->timer_mon = false;
		skb_queue_head_init(&rxtid->q);
		spin_lock_init(&rxtid->lock);

		/* TX A-MSDU */
		txtid = AGGR_GET_TXTID(aggr_conn, i);
		txtid->tid = i;
		txtid->vif = vif;
		init_timer(&txtid->timer);
		txtid->timer.function = aggr_tx_timeout;
		txtid->timer.data = (unsigned long)txtid;
		spin_lock_init(&txtid->lock);
	}

	return aggr_conn;
}

void aggr_recv_delba_req_evt(struct ath6kl_vif *vif, u8 tid)
{
	struct aggr_conn_info *aggr_conn;	
	struct rxtid *rxtid;
	struct ath6kl_sta *conn;
	u8 conn_tid, conn_aid;

	conn_tid = AGGR_BA_EVT_GET_TID(tid);
	conn_aid = AGGR_BA_EVT_GET_CONNID(tid);
	conn = ath6kl_find_sta_by_aid(vif, conn_aid);

	if (conn != NULL) {
		WARN_ON(!conn->aggr_conn_cntxt);

		aggr_conn = conn->aggr_conn_cntxt;
		rxtid = AGGR_GET_RXTID(aggr_conn, conn_tid);
		if (rxtid->aggr)
			aggr_delete_tid_state(aggr_conn, conn_tid);
	}
}

void aggr_reset_state(struct aggr_conn_info *aggr_conn)
{
	struct ath6kl_vif *vif = aggr_conn->aggr_cntxt->vif;
	u8 tid;

	for (tid = 0; tid < NUM_OF_TIDS; tid++) {
		aggr_delete_tid_state(aggr_conn, tid);
		aggr_tx_delete_tid_state(aggr_conn, tid);
	}

	if (vif->nw_type != AP_NETWORK)
		aggr_conn->aggr_cntxt->tx_amsdu_enable = false;

	ath6kl_dbg(ATH6KL_DBG_WLAN_TX_AMSDU, "%s: tx_amsdu_enable %d\n",
				   __func__, aggr_conn->aggr_cntxt->tx_amsdu_enable);

	return;
}

/* clean up our amsdu buffer list */
void ath6kl_cleanup_amsdu_rxbufs(struct ath6kl *ar)
{
	struct htc_packet *packet, *tmp_pkt;

	spin_lock_bh(&ar->lock);
	if (list_empty(&ar->amsdu_rx_buffer_queue)) {
		spin_unlock_bh(&ar->lock);
		return;
	}

	list_for_each_entry_safe(packet, tmp_pkt, &ar->amsdu_rx_buffer_queue,
				 list) {
		list_del(&packet->list);
		spin_unlock_bh(&ar->lock);
		dev_kfree_skb(packet->pkt_cntxt);
		spin_lock_bh(&ar->lock);
	}

	spin_unlock_bh(&ar->lock);
}

void aggr_module_destroy(struct aggr_info *aggr)
{
	if (!aggr)
		return;

	skb_queue_purge(&aggr->free_q);
	kfree(aggr);
}

void aggr_module_destroy_conn(struct aggr_conn_info *aggr_conn)
{
	struct rxtid *rxtid;
	struct txtid *txtid;
	u8 i, k;

	if (!aggr_conn)
		return;

	if (aggr_conn->timer_scheduled) {
		del_timer(&aggr_conn->timer);
		aggr_conn->timer_scheduled = false;
	}

	for (i = 0; i < NUM_OF_TIDS; i++) {
		rxtid = AGGR_GET_RXTID(aggr_conn, i);
		if (rxtid->hold_q) {
			for (k = 0; k < rxtid->hold_q_sz; k++)
				dev_kfree_skb(rxtid->hold_q[k].skb);
			kfree(rxtid->hold_q);
		}
		skb_queue_purge(&rxtid->q);

		/* TX A-MSDU */
		txtid = AGGR_GET_TXTID(aggr_conn, i);
		spin_lock_bh(&txtid->lock);
		aggr_tx_reset_aggr(txtid, true, true);
		spin_unlock_bh(&txtid->lock);
	}

	kfree(aggr_conn);
}
