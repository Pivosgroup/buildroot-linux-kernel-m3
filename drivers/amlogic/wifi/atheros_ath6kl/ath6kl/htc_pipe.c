/*
 * Copyright (c) 2007-2011 Atheros Communications Inc.
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

#include "htc_pipe_internal.h"

#include "core.h"
#include "debug.h"

extern unsigned int htc_bundle_recv;
extern unsigned int htc_bundle_send;
extern unsigned int starving_prevention;
static struct htc_packet *htc_lookup_tx_packet_sync(struct htc_pipe_target *target,
					       int sync_id);

#define USB_HIF_SINGLE_PIPE_DATA_SCHED
#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
#define DATA_EP_SIZE 4
#endif

/* htc pipe tx path */
static inline void restore_tx_packet(struct htc_packet *packet)
{
	if (packet->info.tx.flags & HTC_TX_PACKET_FLAG_FIXUP_NETBUF) {
		skb_pull(packet->netbufcontext,
			 sizeof(struct htc_frame_hdr));
		packet->info.tx.flags &= ~HTC_TX_PACKET_FLAG_FIXUP_NETBUF;
	}
}

static void do_send_completion(struct htc_pipe_endpoint *ep,
			       struct list_head *queue_to_indicate)
{
	struct htc_packet *packet;
	struct htc_pipe_target *target = ep->target;

	if (list_empty(queue_to_indicate)) {
		/* nothing to indicate */
		return;
	}

	/* clear sync_id sending state in sync queue */
	if (target->sync_id_sending) {
		list_for_each_entry(packet, queue_to_indicate, list) {
			if (packet->info.tx.sync_id == target->sync_id_sending) {
				packet->info.tx.sync_id = target->sync_id_sending = 0;
				if (list_empty(&target->sync_queue))
					target->sync_processing = 0;
				break;
			}
		}
	}

	if (ep->ep_callbacks.tx_comp_multi != NULL) {
		ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
			   "%s: calling ep %d, send complete multiple callback"
			   "(%d pkts)\n", __func__,
			   ep->id, get_queue_depth(queue_to_indicate));

		/*
		 * a multiple send complete handler is being used,
		 * pass the queue to the handler
		 */
		ep->ep_callbacks.tx_comp_multi(
				(struct htc_target *) ep->target,
				queue_to_indicate);
		/*
		 * all packets are now owned by the callback,
		 * reset queue to be safe
		 */
		INIT_LIST_HEAD(queue_to_indicate);
	} else {
		/* using legacy EpTxComplete */
		do {
			packet = list_first_entry(queue_to_indicate,
					struct htc_packet, list);

			list_del(&packet->list);

			ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
				   "%s: calling ep %d send complete callback on "
				   "packet 0x%lX\n", __func__,
				   ep->id, (unsigned long)(packet));
			ep->ep_callbacks.tx_complete(
				(struct htc_target *) ep->target,
				packet);
		} while (!list_empty(queue_to_indicate));
	}

}

static void send_packet_completion(struct htc_pipe_target *target,
				   struct htc_packet *packet)
{
	struct htc_pipe_endpoint *ep = &target->endpoint[packet->endpoint];
	struct list_head container;

	restore_tx_packet(packet);
	INIT_LIST_HEAD(&container);
	list_add_tail(&packet->list, &container);
	/* do completion */
	do_send_completion(ep, &container);
}

static void get_htc_packet_credit_based(struct htc_pipe_target *target,
					struct htc_pipe_endpoint *ep,
					struct list_head *queue)
{
	int credits_required;
	int remainder;
	u8 send_flags;
	struct htc_packet *packet;
	unsigned int transfer_len; 
	int starving = ep->starving;

	int msgs_upper_limit = (htc_bundle_send)? HTC_HOST_MAX_MSG_PER_BUNDLE: 1;

	/* NOTE : the TX lock is held when this function is called */

	/* loop until we can grab as many packets out of the queue as we can */
	while (true) {
		send_flags = 0;
		if (list_empty(&ep->tx_queue))
			break;

		/* get packet at head, but don't remove it */
		packet = list_first_entry(&ep->tx_queue,
					struct htc_packet, list);
		if (packet == NULL)
			break;

		ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
			   "%s: got head packet:0x%lX , queue depth: %d\n",
			   __func__, (unsigned long)packet,
			   get_queue_depth(&ep->tx_queue));

		transfer_len = packet->act_len + HTC_HDR_LENGTH;
		if (transfer_len <= target->target_credit_size) {
			credits_required = 1;
		} else {
			/* figure out how many credits this message requires */
			credits_required =
			    transfer_len / target->target_credit_size;
			remainder = transfer_len % target->target_credit_size;

			if (remainder)
				credits_required++;
		}

		ath6kl_dbg(ATH6KL_DBG_HTC_SEND, "%s: creds required:%d"
				"got:%d\n", __func__, credits_required,
				ep->tx_credits);

		if (ep->id == ENDPOINT_0) {
			/*
			 * endpoint 0 is special, it always has a credit and
			 * does not require credit based flow control
			 */
			credits_required = 0;
		} else if (ep->id >= ENDPOINT_2 && ep->id <= ENDPOINT_5) {
			if (target->avail_tx_credits < credits_required)
				break;

			if (htc_bundle_send && !msgs_upper_limit)
				break;

			target->avail_tx_credits -= credits_required;
			ep->ep_st.cred_cosumd += credits_required;
			ep->starving = 0;
			ep->last_sent = jiffies;

			if (target->avail_tx_credits < 1) {
				/* tell the target we need credits ASAP! */
				send_flags |= HTC_FLAGS_NEED_CREDIT_UPDATE;
				ep->ep_st.cred_low_indicate += 1;
				ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
					"%s: host needs credits\n", __func__);
			}
		} else {

			if (ep->tx_credits < credits_required)
				break;

			if (htc_bundle_send && !msgs_upper_limit)
			    break;
                
			ep->tx_credits -= credits_required;
			ep->ep_st.cred_cosumd += credits_required;

			/* check if we need credits back from the target */
			if (ep->tx_credits < ep->tx_credits_per_maxmsg) {
				/* tell the target we need credits ASAP! */
				send_flags |= HTC_FLAGS_NEED_CREDIT_UPDATE;
				ep->ep_st.cred_low_indicate += 1;
				ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
					"%s: host needs credits\n", __func__);
			}
		}

		/* now we can fully dequeue */
		packet = list_first_entry(&ep->tx_queue,
				struct htc_packet, list);

		list_del(&packet->list);
		/* save the number of credits this packet consumed */
		packet->info.tx.cred_used = credits_required;
		/* save send flags */
		packet->info.tx.flags = send_flags;
		packet->info.tx.seqno = ep->seq_no;
		ep->seq_no++;
		/* queue this packet into the caller's queue */
		list_add_tail(&packet->list, queue);
        
		if (htc_bundle_send) 
		    msgs_upper_limit--;

		/* if it is a starving EP, consumes only one credit */
		if (starving)
		    break;
	}

}

static void get_htc_packet(struct htc_pipe_target *target,
			   struct htc_pipe_endpoint *ep,
			   struct list_head *queue, int resources)
{

	struct htc_packet *packet;

    int msgs_upper_limit = (htc_bundle_send)? HTC_HOST_MAX_MSG_PER_BUNDLE: resources;
    
	/* NOTE : the TX lock is held when this function is called */
    if (htc_bundle_send && !resources) {
        return;
    }
    
	/* loop until we can grab as many packets out of the queue as we can */
	while (msgs_upper_limit) {
		if (list_empty(&ep->tx_queue))
			break;

		packet = list_first_entry(&ep->tx_queue,
					struct htc_packet, list);
		list_del(&packet->list);

		ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
			   "%s: got packet:0x%lX , new queue depth: %d\n",
			   __func__, (unsigned long)packet,
			   get_queue_depth(&ep->tx_queue));
		packet->info.tx.seqno = ep->seq_no;
		packet->info.tx.flags = 0;
		packet->info.tx.cred_used = 0;
		ep->seq_no++;
		/* queue this packet into the caller's queue */
		list_add_tail(&packet->list, queue);
		msgs_upper_limit--;
	}

}

static int htc_issue_packets_sync(struct htc_pipe_target *target,
			     struct htc_pipe_endpoint *ep,
			     struct list_head *pkt_queue)
{
	int status = 0;
	u16 payload_len;
	struct sk_buff *nbuf;
	struct htc_frame_hdr *htc_hdr;
	struct htc_packet *packet;
	unsigned int transfer_len;
	int credits_required;
	int remainder;
	u8 send_flags;

	spin_lock_bh(&target->htc_txlock);
	while (!list_empty(pkt_queue)) {
		packet = list_first_entry(pkt_queue,
					struct htc_packet, list);
		if (packet == NULL)
			break;

		transfer_len = packet->act_len + HTC_HDR_LENGTH;
		if (transfer_len <= target->target_credit_size) {
			credits_required = 1;
		} else {
			/* figure out how many credits this message requires */
			credits_required =
			    transfer_len / target->target_credit_size;
			remainder = transfer_len % target->target_credit_size;

			if (remainder)
				credits_required++;
		}		

		if (ep->tx_credit_flow_enabled) {
			if (ep->id >= ENDPOINT_2 && ep->id <= ENDPOINT_5) {
				target->avail_tx_credits -= credits_required;
				ep->ep_st.cred_cosumd += credits_required;

				if (target->avail_tx_credits < 1) {
					/* tell the target we need credits ASAP! */
					send_flags |= HTC_FLAGS_NEED_CREDIT_UPDATE;
					ep->ep_st.cred_low_indicate += 1;
					ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
						"%s: host needs credits\n", __func__);
				}

			}
		}

		nbuf = packet->netbufcontext;
		if (!nbuf) {
			WARN_ON(1);
			status = -EINVAL;
			break;
		}

		payload_len = packet->act_len;
		/* setup HTC frame header */
		htc_hdr = (struct htc_frame_hdr *)skb_push(nbuf,
					sizeof(struct htc_frame_hdr));
		if (!htc_hdr) {
			WARN_ON(1);
			status = -EINVAL;
			break;
		}

		list_del(&packet->list);

		/* save the number of credits this packet consumed */
		packet->info.tx.cred_used = credits_required;
		/* save send flags */
		packet->info.tx.flags = send_flags;
		packet->info.tx.seqno = ep->seq_no;
		ep->seq_no++;

		packet->info.tx.flags |= HTC_TX_PACKET_FLAG_FIXUP_NETBUF;

		/* Endianess? */
		put_unaligned((u16) payload_len, &htc_hdr->payld_len);
		htc_hdr->flags = packet->info.tx.flags;
		htc_hdr->eid = (u8) packet->endpoint;
		htc_hdr->ctrl[0] = 0;
		htc_hdr->ctrl[1] = (u8) packet->info.tx.seqno;

		spin_unlock_bh(&target->htc_txlock);
		status = hif_send_sync(target->parent.dev->ar,
				ep->pipeid_ul, nbuf, HZ);

		spin_lock_bh(&target->htc_txlock);
		if (status != 0) {
			if (status != -ENOMEM) {
				/* TODO: if more than 1 endpoint maps to the
				 * same PipeID, it is possible to run out of
				 * resources in the HIF layer.
				 * Don't emit the error
				 */
				ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
					   "%s: failed status:%d\n",
					   __func__, status);
			}
			/* put it back into the callers queue */
			list_add(&packet->list, pkt_queue);

			if (ep->tx_credit_flow_enabled) {
				/* reclaim credits */
				if (ep->id >= ENDPOINT_2 && ep->id <= ENDPOINT_5)
					target->avail_tx_credits += packet->info.tx.cred_used;
			}
			break;
		} else {
			packet->status = status;
			send_packet_completion(target, packet);
		}
	}

	spin_unlock_bh(&target->htc_txlock);
	if (status != 0) {
		while (!list_empty(pkt_queue)) {
			if (status != -ENOMEM) {
				ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
					   "%s: failed pkt:0x%p status:%d\n",
					   __func__, packet, status);
			}
			packet = list_first_entry(pkt_queue,
					struct htc_packet, list);
			list_del(&packet->list);
			packet->status = status;
			send_packet_completion(target, packet);
		}
	}

	return status;
}

static int htc_issue_packets(struct htc_pipe_target *target,
			     struct htc_pipe_endpoint *ep,
			     struct list_head *pkt_queue)
{
	int status = 0;
	u16 payload_len;
	struct sk_buff *nbuf;
	struct htc_frame_hdr *htc_hdr;
	struct htc_packet *packet;

	ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
		   "%s: queue: 0x%lX, pkts %d\n", __func__,
		   (unsigned long)pkt_queue, get_queue_depth(pkt_queue));

    if (htc_bundle_send && (ep->pipeid_ul != 0 /* HIF_TX_CTRL_PIPE */)) {
        /* only for HIF data pipes */
        struct sk_buff *msg_bundle[HTC_HOST_MAX_MSG_PER_BUNDLE];
        int msgs_to_bundle = 0;
        while (!list_empty(pkt_queue)) {
            packet = list_first_entry(pkt_queue,
					struct htc_packet, list);
            list_del(&packet->list);
            
            nbuf = packet->netbufcontext;
            if (!nbuf) {
                WARN_ON(1);
                status = -EINVAL;
                break;
            }
            
            payload_len = packet->act_len;
            /* setup HTC frame header */
            htc_hdr = (struct htc_frame_hdr *)skb_push(nbuf,
					sizeof(struct htc_frame_hdr));
            if (!htc_hdr) {
                WARN_ON(1);
                status = -EINVAL;
                break;
            }
            packet->info.tx.flags |= HTC_TX_PACKET_FLAG_FIXUP_NETBUF;
            packet->info.tx.flags |= HTC_FLAGS_SEND_BUNDLE;

            /* Endianess? */
            put_unaligned((u16) payload_len, &htc_hdr->payld_len);
            htc_hdr->flags = packet->info.tx.flags;
            htc_hdr->eid = (u8) packet->endpoint;
            htc_hdr->ctrl[0] = 0;
            htc_hdr->ctrl[1] = (u8) packet->info.tx.seqno;
            
            spin_lock_bh(&target->htc_txlock);
            /* store in look up queue to match completions */
            list_add_tail(&packet->list, &ep->tx_lookup_queue);
            ep->ep_st.tx_issued += 1;
            spin_unlock_bh(&target->htc_txlock);
            
            /* pkt_queue is less than HTC_HOST_MAX_MSG_PER_BUNDLE */
            msg_bundle[msgs_to_bundle] = nbuf;
            msgs_to_bundle++;
        }
        
        status = hif_send_multiple(target->parent.dev->ar, 
                ep->pipeid_ul, msg_bundle, msgs_to_bundle);
    }
    else {
           
	while (!list_empty(pkt_queue)) {
		packet = list_first_entry(pkt_queue,
					struct htc_packet, list);
		list_del(&packet->list);

		nbuf = packet->netbufcontext;
		if (!nbuf) {
			WARN_ON(1);
			status = -EINVAL;
			break;
		}

		payload_len = packet->act_len;
		/* setup HTC frame header */
		htc_hdr = (struct htc_frame_hdr *)skb_push(nbuf,
					sizeof(struct htc_frame_hdr));
		if (!htc_hdr) {
			WARN_ON(1);
			status = -EINVAL;
			break;
		}

		packet->info.tx.flags |= HTC_TX_PACKET_FLAG_FIXUP_NETBUF;

		/* Endianess? */
		put_unaligned((u16) payload_len, &htc_hdr->payld_len);
		htc_hdr->flags = packet->info.tx.flags;
		htc_hdr->eid = (u8) packet->endpoint;
		htc_hdr->ctrl[0] = 0;
		htc_hdr->ctrl[1] = (u8) packet->info.tx.seqno;

		spin_lock_bh(&target->htc_txlock);
		/* store in look up queue to match completions */
		list_add_tail(&packet->list, &ep->tx_lookup_queue);
		ep->ep_st.tx_issued += 1;
		spin_unlock_bh(&target->htc_txlock);

		status = hif_send(target->parent.dev->ar,
				ep->pipeid_ul, NULL, nbuf);

		if (status != 0) {
			if (status != -ENOMEM) {
				/* TODO: if more than 1 endpoint maps to the
				 * same PipeID, it is possible to run out of
				 * resources in the HIF layer.
				 * Don't emit the error
				 */
				ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
					   "%s: failed status:%d\n",
					   __func__, status);
			}
			spin_lock_bh(&target->htc_txlock);
			list_del(&packet->list);
			/* reclaim credits */
			if (ep->id >= ENDPOINT_2 && ep->id <= ENDPOINT_5)
				target->avail_tx_credits += packet->info.tx.cred_used;
			else
				ep->tx_credits += packet->info.tx.cred_used;

			spin_unlock_bh(&target->htc_txlock);
			/* put it back into the callers queue */
			list_add(&packet->list, pkt_queue);
			break;
		}

	}

	if (status != 0) {
		while (!list_empty(pkt_queue)) {
			if (status != -ENOMEM) {
				ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
					   "%s: failed pkt:0x%p status:%d\n",
					   __func__, packet, status);
			}
			packet = list_first_entry(pkt_queue,
					struct htc_packet, list);
			list_del(&packet->list);
			packet->status = status;
			send_packet_completion(target, packet);
		}
	}

    }
    
	return status;
}


static int htc_try_send_sync(struct htc_pipe_target *target,
					  struct htc_pipe_endpoint *ep,
					  struct list_head *callers_send_queue)
{
	struct list_head send_queue;
	struct htc_packet *packet = NULL;
	struct htc_pipe_endpoint *sync_ep;
	static int use_hif_sync = 1;

	/* init the local send queue */
	INIT_LIST_HEAD(&send_queue);

	if (callers_send_queue != NULL && !list_empty(callers_send_queue))
		packet = list_first_entry(callers_send_queue,
				struct htc_packet, list);

	spin_lock_bh(&target->htc_txlock);

	if (packet && use_hif_sync &&
	    packet->info.tx.tag == ATH6KL_CONTROL_PKT_SYNC_TAG) {
	    spin_unlock_bh(&target->htc_txlock);
	    htc_issue_packets_sync(target, ep, callers_send_queue);
	    return 1;
	}

	/* check if this is a sync packet */
	if ((packet) &&
	    (packet->info.tx.tag == ATH6KL_CONTROL_PKT_SYNC_TAG || target->sync_processing)) {
		struct htc_packet *tmp;
		/* queue packet in a queue which is ep-independent */
		list_for_each_entry_safe(packet, tmp, callers_send_queue, list) {
			list_del(&packet->list);
			list_add_tail(&packet->list, &target->sync_queue);
			packet->info.tx.sync_id = target->sync_id_latest++;
		}
		target->sync_processing = 1;
	}

	/* no packet in the queue so return */
	if (list_empty(&target->sync_queue) &&
	    !target->sync_id_sending) {
		spin_unlock_bh(&target->htc_txlock);
		target->sync_processing = 0;
		return 0;
	}

	/* check if any sync packet is sending */
	if (target->sync_id_sending &&
	    htc_lookup_tx_packet_sync(target, target->sync_id_sending)) {
		spin_unlock_bh(&target->htc_txlock);
		return 1;
	}

	/* no sync packet is sending so send next */
	packet = list_first_entry(&target->sync_queue,
					struct htc_packet, list);
	list_del(&packet->list);
	list_add_tail(&packet->list, &send_queue);
	target->sync_id_sending = packet->info.tx.sync_id;

	sync_ep = &target->endpoint[packet->endpoint];

	spin_unlock_bh(&target->htc_txlock);

	htc_issue_packets(target, sync_ep, &send_queue);

	return 1;
}

static enum htc_send_queue_result htc_try_send(struct htc_pipe_target *target,
					  struct htc_pipe_endpoint *ep,
					  struct list_head *callers_send_queue)
{
	struct list_head send_queue;	/* temp queue to hold packets */
	struct htc_packet *packet, *tmp_pkt;
	struct ath6kl *ar = target->parent.dev->ar;
	int tx_resources;
	int starving;
	int overflow, txqueue_depth;

	ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
		   "%s: (queue:0x%lX depth:%d)\n",
		   __func__, (unsigned long)callers_send_queue,
		   (callers_send_queue ==
		    NULL) ? 0 : get_queue_depth(callers_send_queue));

	/* init the local send queue */
	INIT_LIST_HEAD(&send_queue);

	/* handle sync queue */
	if (htc_try_send_sync(target, ep, callers_send_queue))
		return HTC_SEND_QUEUE_OK;

	/*
	 * callers_send_queue equals to NULL means
	 * caller didn't provide a queue, just wants us to
	 * check queues and send
	 */
	if (callers_send_queue != NULL) {
		if (list_empty(callers_send_queue)) {
			/* empty queue */
			return HTC_SEND_QUEUE_DROP;
		}

		spin_lock_bh(&target->htc_txlock);
		txqueue_depth = get_queue_depth(&ep->tx_queue);
		spin_unlock_bh(&target->htc_txlock);

		if (txqueue_depth >= ep->max_txqueue_depth) {
			/* we've already overflowed */
			overflow = get_queue_depth(callers_send_queue);
		} else {
			/* get how much we will overflow by */
			overflow = txqueue_depth;
			overflow += get_queue_depth(callers_send_queue);
			/* get how much we will overflow the TX queue by */
			overflow -= ep->max_txqueue_depth;
		}

		/* if overflow is negative or zero, we are okay */
		if (overflow > 0) {
			ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
				   "%s: Endpoint %d, TX queue will overflow :%d, "
				   "Tx Depth:%d, Max:%d\n", __func__,
				   ep->id, overflow, txqueue_depth,
				   ep->max_txqueue_depth);
		}
		if ((overflow <= 0)
		    || (ep->ep_callbacks.tx_full == NULL)) {
			/*
			 * all packets will fit or caller did not provide send
			 * full indication handler -- just move all of them
			 * to the local send_queue object
			 */
			list_splice_tail_init(callers_send_queue,
							&send_queue);
		} else {
			int i;
			int good_pkts =
				get_queue_depth(callers_send_queue) - overflow;
			if (good_pkts < 0) {
				WARN_ON(1);
				return HTC_SEND_QUEUE_DROP;
			}

			/* we have overflowed, and a callback is provided */
			/* dequeue all non-overflow packets to the sendqueue */
			for (i = 0; i < good_pkts; i++) {
				/* pop off caller's queue */
				packet = list_first_entry(
						callers_send_queue,
						struct htc_packet, list);
				list_del(&packet->list);
				/* insert into local queue */
				list_add_tail(&packet->list, &send_queue);
			}

			/*
			 * the caller's queue has all the packets that won't fit
			 * walk through the caller's queue and indicate each to
			 * the send full handler
			 */
			list_for_each_entry_safe(packet, tmp_pkt,
						 callers_send_queue, list) {

				ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
					   "%s: Indicat overflowed TX pkts: %lX\n",
					   __func__, (unsigned long)packet);
				if (ep->ep_callbacks.
						tx_full((struct htc_target *)
						ep->target,
						packet) == HTC_SEND_FULL_DROP) {
					/* callback wants the packet dropped */
					ep->ep_st.tx_dropped += 1;

					/* leave this one in the caller's queue
					 * for cleanup */
				} else {
					/* callback wants to keep this packet,
					 * remove from caller's queue */
					list_del(&packet->list);
					/* put it in the send queue */
					list_add_tail(&packet->list,
						&send_queue);
				}

			}

			if (list_empty(&send_queue)) {
				/* no packets made it in, caller will cleanup */
				return HTC_SEND_QUEUE_DROP;
			}
		}
	}

	if (!ep->tx_credit_flow_enabled) {
		tx_resources =
		    hif_get_free_queue_number(ar, ep->pipeid_ul);
	} else {
		tx_resources = 0;
	}

	spin_lock_bh(&target->htc_txlock);
	if (!list_empty(&send_queue)) {
		/* transfer packets to tail */
		list_splice_tail_init(&send_queue, &ep->tx_queue);
		if (!list_empty(&send_queue)) {
			WARN_ON(1);
			spin_unlock_bh(&target->htc_txlock);
			return HTC_SEND_QUEUE_DROP;
		}
		INIT_LIST_HEAD(&send_queue);
	}

	/* increment tx processing count on entry */
	ep->tx_process_count++;

	if (ep->tx_process_count > 1) {
		/*
		 * another thread or task is draining the TX queues on this
		 * endpoint that thread will reset the tx processing count when
		 * the queue is drained
		 */
		ep->tx_process_count--;
		spin_unlock_bh(&target->htc_txlock);
		return HTC_SEND_QUEUE_OK;
	}

	/***** beyond this point only 1 thread may enter ******/

	/*
	 * now drain the endpoint TX queue for transmission as long as we have
	 * enough transmit resources
	 */
	starving = ep->starving;
	while (true) {

		if (get_queue_depth(&ep->tx_queue) == 0)
			break;

		if (ep->tx_credit_flow_enabled) {
			/* credit based mechanism provides flow control based on
			 * target transmit resource availability, we assume that
			 * the HIF layer will always have bus resources greater
			 * than target transmit resources
			 */
			get_htc_packet_credit_based(target, ep, &send_queue);
		} else {
			/* get all packets for this endpoint that we can for
			 * this pass
			 */
			get_htc_packet(target, ep, &send_queue, tx_resources);
		}

		if (get_queue_depth(&send_queue) == 0) {
			/* didn't get packets due to out of resources or
			 * TX queue was drained
			 */
			break;
		}

		spin_unlock_bh(&target->htc_txlock);

		/* send what we can */
		htc_issue_packets(target, ep, &send_queue);

		if (!ep->tx_credit_flow_enabled) {
			tx_resources =
			    hif_get_free_queue_number(ar,
						      ep->pipeid_ul);
		}

		spin_lock_bh(&target->htc_txlock);

		/* if it is a starving EP, consumes only one credit */
		if (starving)
		    break;

	}
	/* done with this endpoint, we can clear the count */
	ep->tx_process_count = 0;
	spin_unlock_bh(&target->htc_txlock);

	return HTC_SEND_QUEUE_OK;
}

static void htc_tx_resource_available(struct htc_target *context, u8 pipeid)
{
	int i;
	struct htc_pipe_target *target = (struct htc_pipe_target *) context;
	struct htc_pipe_endpoint *ep = NULL;

	for (i = 0; i < ENDPOINT_MAX; i++) {
		ep = &target->endpoint[i];
		if (ep->service_id != 0) {
			if (ep->pipeid_ul == pipeid)
				break;
		}
	}

	if (i >= ENDPOINT_MAX) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "Invalid pipe indicated for TX resource avail : %d!\n",
			   pipeid);
		return;
	}

	ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
		   "%s: hIF indicated more resources for pipe:%d\n",
		   __func__, pipeid);

	htc_try_send(target, ep, NULL);
}

/* htc control packet manipulation */
static void destroy_htc_txctrl_packet(struct htc_packet *packet)
{
	struct sk_buff *netbuf;
	netbuf = (struct sk_buff *) packet->netbufcontext;
	if (netbuf != NULL)
		dev_kfree_skb(netbuf);

	kfree(packet);
}

static struct htc_packet *build_htc_txctrl_packet(void)
{
	struct htc_packet *packet = NULL;
	struct sk_buff *netbuf;

	packet = kzalloc(sizeof(struct htc_packet), GFP_KERNEL);
	if (packet == NULL)
		return NULL;

	memset(packet, 0, sizeof(struct htc_packet));
	netbuf = __dev_alloc_skb(HTC_CONTROL_BUFFER_SIZE, GFP_KERNEL);

	if (netbuf == NULL) {
		kfree(packet);
		return NULL;
	}
	packet->netbufcontext = netbuf;

	return packet;
}

static void htc_free_txctrl_packet(struct htc_pipe_target *target,
				   struct htc_packet *packet)
{
	destroy_htc_txctrl_packet(packet);
}

static struct htc_packet *htc_alloc_txctrl_packet(struct htc_pipe_target
						*target)
{
	return build_htc_txctrl_packet();
}

static void htc_txctrl_complete(struct htc_target *context,
						struct htc_packet *packet)
{

	struct htc_pipe_target *target =
		(struct htc_pipe_target *) context;
	htc_free_txctrl_packet(target, packet);
}

#define MAX_MESSAGE_SIZE 1536

static int htc_setup_target_buffer_assignments(struct htc_pipe_target *target)
{
	struct htc_pipe_txcredit_alloc *entry;
	int status;
	int credits;
	int credit_per_maxmsg;
	unsigned int hif_usbaudioclass = 0;

	credit_per_maxmsg = MAX_MESSAGE_SIZE / target->target_credit_size;
	if (MAX_MESSAGE_SIZE % target->target_credit_size)
		credit_per_maxmsg++;

	/* TODO, this should be configured by the caller! */

	credits = target->total_transmit_credits;
	entry = &target->txcredit_alloc[0];

	status = -ENOMEM;

	if (hif_usbaudioclass) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "htc_setup_target_buffer_assignments "
			   "For USB Audio Class- Total:%d\n", credits);
		entry++;
		entry++;
		/* Setup VO Service To have Max Credits */
		entry->service_id = WMI_DATA_VO_SVC;
		entry->credit_alloc = (credits - 6);
		if (entry->credit_alloc == 0)
			entry->credit_alloc++;

		credits -= (int)entry->credit_alloc;
		if (credits <= 0)
			return status;

		entry++;
		entry->service_id = WMI_CONTROL_SVC;
		entry->credit_alloc = credit_per_maxmsg;
		credits -= (int)entry->credit_alloc;
		if (credits <= 0)
			return status;

		/* leftovers go to best effort */
		entry++;
		entry++;
		entry->service_id = WMI_DATA_BE_SVC;
		entry->credit_alloc = (u8) credits;
		status = 0;
	} else {
		entry++;
		entry->service_id = WMI_DATA_VI_SVC;
		entry->credit_alloc = credits / 4;
		if (entry->credit_alloc == 0)
			entry->credit_alloc++;

		credits -= (int)entry->credit_alloc;
		if (credits <= 0)
			return status;

		entry++;
		entry->service_id = WMI_DATA_VO_SVC;
		entry->credit_alloc = credits / 4;
		if (entry->credit_alloc == 0)
			entry->credit_alloc++;

		credits -= (int)entry->credit_alloc;
		if (credits <= 0)
			return status;

		entry++;
		entry->service_id = WMI_CONTROL_SVC;
		entry->credit_alloc = credit_per_maxmsg;
		credits -= (int)entry->credit_alloc;
		if (credits <= 0)
			return status;

		entry++;
		entry->service_id = WMI_DATA_BK_SVC;
		entry->credit_alloc = 4;
		credits -= (int)entry->credit_alloc;
		if (credits <= 0)
			return status;

		/* leftovers go to best effort */
		entry++;
		entry->service_id = WMI_DATA_BE_SVC;
		entry->credit_alloc = (u8) credits;
		status = 0;
	}

	if (status == 0) {
		if (AR_DBG_LVL_CHECK(ATH6KL_DBG_HTC_PIPE)) {
			int i;
			for (i = 0; i < HTC_MAX_SERVICE_ALLOC_ENTRIES; i++) {
				if (target->txcredit_alloc[i].service_id != 0) {
					ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
						   "HTC Service Index : %d TX : 0x%2.2X :"
						   "alloc:%d\n", i,
						   target->txcredit_alloc[i].
						   service_id,
						   target->txcredit_alloc[i].
						   credit_alloc);
				}
			}
		}
	}
	return status;
}

/* process credit reports and call distribution function */
static void htc_process_credit_report(struct htc_pipe_target *target,
				      struct htc_credit_report *rpt,
				      int num_entries,
				      enum htc_endpoint_id from_ep)
{
	int i;
	struct htc_pipe_endpoint *ep;
	int total_credits = 0;

	/* lock out TX while we update credits */
	spin_lock_bh(&target->htc_txlock);

	for (i = 0; i < num_entries; i++, rpt++) {
		if (rpt->eid >= ENDPOINT_MAX) {
			WARN_ON(1);
			spin_unlock_bh(&target->htc_txlock);
			return;
		}

		ep = &target->endpoint[rpt->eid];
		
		if (ep->id >= ENDPOINT_2 && ep->id <= ENDPOINT_5) {
			enum htc_endpoint_id eid[DATA_EP_SIZE] = {ENDPOINT_5, ENDPOINT_4, ENDPOINT_2, ENDPOINT_3};
			int epid_idx;

			target->avail_tx_credits += rpt->credits;
			
			for (epid_idx = 0; epid_idx < DATA_EP_SIZE; epid_idx++) {
				ep = &target->endpoint[eid[epid_idx]];
				if (get_queue_depth(&ep->tx_queue)) {
					break;
				}	
				
			}
			spin_unlock_bh(&target->htc_txlock);

			for (epid_idx = 0; epid_idx < DATA_EP_SIZE; epid_idx++) { 
			    struct htc_pipe_endpoint *starving_ep;
			    starving_ep = &target->endpoint[eid[epid_idx]];
			    if (ep == starving_ep)
				continue;

			    if (starving_ep->starving) 
				htc_try_send(target, starving_ep, NULL);
                       }
			htc_try_send(target, ep, NULL);
			spin_lock_bh(&target->htc_txlock);
		} else {
			ep->tx_credits += rpt->credits;

			if (ep->tx_credits && get_queue_depth(&ep->tx_queue)) {
				spin_unlock_bh(&target->htc_txlock);
				htc_try_send(target, ep, NULL);
				spin_lock_bh(&target->htc_txlock);
			}
		}
		total_credits += rpt->credits;
	}
	ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
		   "  Report indicated %d credits to distribute\n",
		   total_credits);

	spin_unlock_bh(&target->htc_txlock);
}

/* flush endpoint TX queue */
static void htc_flush_tx_endpoint(struct htc_pipe_target *target,
				  struct htc_pipe_endpoint *ep, HTC_TX_TAG Tag)
{
	struct htc_packet *packet;

	spin_lock_bh(&target->htc_txlock);
	while (get_queue_depth(&ep->tx_queue)) {
		packet = list_first_entry(&ep->tx_queue,
					struct htc_packet, list);
		list_del(&packet->list);
		packet->status = 0;
		send_packet_completion(target, packet);
	}
	spin_unlock_bh(&target->htc_txlock);
}

/*
 * In the adapted HIF layer, struct sk_buff * are passed between HIF and HTC,
 * since upper layers expects struct htc_packet containers we use the completed
 * netbuf and lookup it's corresponding HTC packet buffer from a lookup list.
 * This is extra overhead that can be fixed by re-aligning HIF interfaces with
 * HTC.
 */
static struct htc_packet *htc_lookup_tx_packet(struct htc_pipe_target *target,
					       struct htc_pipe_endpoint *ep,
					       struct sk_buff *netbuf)
{
	struct htc_packet *packet, *tmp_pkt;
	struct htc_packet *found_packet = NULL;

	spin_lock_bh(&target->htc_txlock);

	/*
	 * interate from the front of tx lookup queue
	 * this lookup should be fast since lower layers completes in-order and
	 * so the completed packet should be at the head of the list generally
	 */
	list_for_each_entry_safe(packet, tmp_pkt, &ep->tx_lookup_queue, list) {
		/* check for removal */
		if (netbuf == packet->netbufcontext) {
			/* found it */
			list_del(&packet->list);
			found_packet = packet;
			break;
		}

	}

	spin_unlock_bh(&target->htc_txlock);

	return found_packet;
}

static struct htc_packet *htc_lookup_tx_packet_sync(struct htc_pipe_target *target,
					       int sync_id)
{
	struct htc_packet *packet;
	struct htc_packet *found_packet = NULL;
	struct htc_pipe_endpoint *ep;
	int i;

	/* must called with lock held */
	for (i = ENDPOINT_0; i < ENDPOINT_MAX; i++) {
		ep = &target->endpoint[i];
		list_for_each_entry(packet, &ep->tx_lookup_queue, list) {
			if (packet->info.tx.sync_id == sync_id) {
				found_packet = packet;
				break;
			}
		}
	}

	return found_packet;
}

static int htc_tx_completion(struct htc_target *context, struct sk_buff *netbuf)
{
	struct htc_pipe_target *target = (struct htc_pipe_target *)context;
	u8 *netdata;
	u32 netlen;
	struct htc_frame_hdr *htc_hdr;
	u8 EpID;
	struct htc_pipe_endpoint *ep;
	struct htc_packet *packet;
#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
	struct ath6kl *ar;
	enum htc_endpoint_id eid[DATA_EP_SIZE] = {ENDPOINT_5, ENDPOINT_4, ENDPOINT_2, ENDPOINT_3};
	int epid_idx;
	u16 resources_thresh[DATA_EP_SIZE]; //urb resources
	u16 resources;
	u16 resources_max;
#endif        
	netdata = netbuf->data;
	netlen = netbuf->len;

	htc_hdr = (struct htc_frame_hdr *)netdata;

	EpID = htc_hdr->eid;
	ep = &target->endpoint[EpID];

	packet = htc_lookup_tx_packet(target, ep, netbuf);
	if (packet == NULL) {
		/* may have already been flushed and freed */
		ath6kl_err("HTC TX lookup failed!\n");
	} else {
		/* will be giving this buffer back to upper layers */
		packet->status = 0;
		send_packet_completion(target, packet);
	}
	netbuf = NULL;


        for (epid_idx = 0; epid_idx < DATA_EP_SIZE; epid_idx++) {
            struct htc_pipe_endpoint *starving_ep;
            starving_ep = &target->endpoint[eid[epid_idx]];
            if (ep == starving_ep)
               continue;

	    if (starving_ep->starving) 
		htc_try_send(target, starving_ep, NULL);
        }

	if (!ep->tx_credit_flow_enabled) {
#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
		if (((enum htc_endpoint_id)EpID >= ENDPOINT_2 && 
			(enum htc_endpoint_id)EpID <= ENDPOINT_5) &&
		    !target->sync_id_sending &&
		    list_empty(&target->sync_queue)) {

			ar = target->parent.dev->ar;

			resources_max = hif_get_max_queue_number(ar, ep->pipeid_ul);

			resources_thresh[0] = 0;                           	//VO
			resources_thresh[1] = (resources_max *  2) / 32;    //VI
			resources_thresh[2] = (resources_max *  3) / 32;    //BE
			resources_thresh[3] = (resources_max *  4) / 32;    //BK

			resources = hif_get_free_queue_number(ar, ep->pipeid_ul);
            
			spin_lock_bh(&target->htc_txlock);
			for (epid_idx = 0; epid_idx < DATA_EP_SIZE; epid_idx++) {
				ep = &target->endpoint[eid[epid_idx]];

				if (!get_queue_depth(&ep->tx_queue)) {
					continue;
				}

				if (resources >= resources_thresh[epid_idx])
					break;
			}
			spin_unlock_bh(&target->htc_txlock);

			if (epid_idx == DATA_EP_SIZE) {
				#if 0
				if (resources) {
					for (epid_idx = 0; epid_idx < DATA_EP_SIZE; epid_idx++) {
						ep = &target->endpoint[eid[epid_idx]];
						if (get_queue_depth(&ep->tx_queue)) {
							break;
						}
					}
				} else
				#endif
				return 0;
			}
		}
#endif        
		/*
		 * note: when using TX credit flow, the re-checking of queues
		 * happens when credits flow back from the target. in the
		 * non-TX credit case, we recheck after the packet completes
		 */
		htc_try_send(target, ep, NULL);
	}

	return 0;
}

#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
static int htc_send_pkts_sched_check(struct htc_pipe_target *target, enum htc_endpoint_id id)
{    
	struct htc_pipe_endpoint *endpoint;
	enum htc_endpoint_id eid;
	struct list_head *tx_queue;
	u16 ac_queue_status[DATA_EP_SIZE] = {0, 0, 0, 0};

	if (id < ENDPOINT_2 || id > ENDPOINT_5 ||
	    target->sync_id_sending || !list_empty(&target->sync_queue)) {
		return 1;
	}

	spin_lock_bh(&target->htc_txlock);

	for (eid = ENDPOINT_2; eid <= ENDPOINT_5; eid++) {
		endpoint = &target->endpoint[eid];
		tx_queue = &endpoint->tx_queue;

		if (list_empty(tx_queue)) {
			ac_queue_status[eid - 2] = 1;
		}
	}

	spin_unlock_bh(&target->htc_txlock);

	switch (id)
	{
		case ENDPOINT_2: //BE
			return (ac_queue_status[2] && ac_queue_status[3]);
		case ENDPOINT_3: //BK
			return (ac_queue_status[0] && ac_queue_status[2] && ac_queue_status[3]);
		case ENDPOINT_4: //VI
			return (ac_queue_status[3]);
		case ENDPOINT_5: //VO
			return (1);
		default:
			ath6kl_err("incorrect epid (%d) while checking\n", id);
			break;
	}

	return 1;
}

static int htc_send_pkts_sched_queue(struct htc_pipe_target *target, struct list_head *pkt_queue, enum htc_endpoint_id eid)
{
	struct htc_pipe_endpoint *endpoint;
	struct list_head *tx_queue;
	struct htc_packet *packet, *tmp_pkt;
	int good_pkts;

	endpoint = &target->endpoint[eid];
	tx_queue = &endpoint->tx_queue;

	spin_lock_bh(&target->htc_txlock);

	good_pkts = endpoint->max_txqueue_depth - get_queue_depth(tx_queue);

	if (good_pkts > 0){
		while (!list_empty(pkt_queue)) {
			packet = list_first_entry(pkt_queue, struct htc_packet, list);
			list_del(&packet->list);
			list_add_tail(&packet->list, tx_queue);
            
			good_pkts--;
			if (good_pkts <= 0) {
				break;
			}
		}
	}

	if (get_queue_depth(pkt_queue)) {
		list_for_each_entry_safe(packet, tmp_pkt, pkt_queue, list) {
			if (endpoint->ep_callbacks.tx_full((struct htc_target *)
					endpoint->target,
					packet) == HTC_SEND_FULL_DROP) {
				endpoint->ep_st.tx_dropped += 1;
			} else {
				list_del(&packet->list);
				list_add_tail(&packet->list, tx_queue);
			}
		}
	}

	spin_unlock_bh(&target->htc_txlock);

	return 0;
}

#endif

static int htc_send_packets_multiple(struct htc_target *handle,
				struct list_head *pkt_queue)
{
	struct htc_pipe_target *target = (struct htc_pipe_target *) handle;
	struct htc_pipe_endpoint *ep;
	struct htc_packet *packet, *tmp_pkt;

	if (list_empty(pkt_queue))
		return -EINVAL;

	/* get first packet to find out which ep the packets will go into */
	packet = list_first_entry(pkt_queue, struct htc_packet, list);
	if (packet == NULL) {
		return -EINVAL;
	}

	if (packet->endpoint >= ENDPOINT_MAX) {
		WARN_ON(1);
		return -EINVAL;
	}
	ep = &target->endpoint[packet->endpoint];

#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED

	if (!htc_send_pkts_sched_check(target, ep->id) && 
		packet->info.tx.tag != ATH6KL_CONTROL_PKT_SYNC_TAG) {

	    htc_send_pkts_sched_queue(target, pkt_queue, ep->id);

	    /* checking for starving */ 
	    if (starving_prevention &&
		time_after(jiffies, ep->last_sent + (HZ >> 3))) 
		ep->starving =1;
	} else { 
	    htc_try_send(target, ep, pkt_queue); 
	}
#else 
	htc_try_send(target, ep, pkt_queue);
#endif


	/* do completion on any packets that couldn't get in */
	if (!list_empty(pkt_queue)) {

		list_for_each_entry_safe(packet, tmp_pkt, pkt_queue, list) {
			if (target->htc_state_flags & HTC_STATE_STOPPING)
				packet->status = -ECANCELED;
			else
				packet->status = -ENOMEM;
		}

		do_send_completion(ep, pkt_queue);
	}

	return 0;
}

/* htc pipe rx path */
static struct htc_packet *alloc_htc_packet_container(struct htc_pipe_target
						     *target)
{
	struct htc_packet *packet;
	spin_lock_bh(&target->htc_rxlock);

	if (target->htc_packet_pool == NULL) {
		spin_unlock_bh(&target->htc_rxlock);
		return NULL;
	}

	packet = target->htc_packet_pool;
	target->htc_packet_pool = (struct htc_packet *)packet->list.next;

	spin_unlock_bh(&target->htc_rxlock);

	packet->list.next = NULL;
	return packet;
}

static void free_htc_packet_container(struct htc_pipe_target *target,
				      struct htc_packet *packet)
{
	spin_lock_bh(&target->htc_rxlock);

	if (target->htc_packet_pool == NULL) {
		target->htc_packet_pool = packet;
		packet->list.next = NULL;
	} else {
		packet->list.next = (struct list_head *)target->htc_packet_pool;
		target->htc_packet_pool = packet;
	}

	spin_unlock_bh(&target->htc_rxlock);
}

static int htc_process_trailer(struct htc_pipe_target *target,
			       u8 *buffer,
			       int len, enum htc_endpoint_id from_ep)
{
	struct htc_record_hdr *record;
	u8 *record_buf;
	u8 *orig_buf;
	int orig_len;
	int status;

	orig_buf = buffer;
	orig_len = len;
	status = 0;

	while (len > 0) {

		if (len < sizeof(struct htc_record_hdr)) {
			status = -EINVAL;
			break;
		}
		/* these are byte aligned structs */
		record = (struct htc_record_hdr *)buffer;
		len -= sizeof(struct htc_record_hdr);
		buffer += sizeof(struct htc_record_hdr);

		if (record->len > len) {
			/* no room left in buffer for record */
			ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
				   " invalid length: %d (id:%d) buffer has: %d bytes left\n",
				   record->len, record->rec_id, len);
			status = -EINVAL;
			break;
		}
		/* start of record follows the header */
		record_buf = buffer;

		switch (record->rec_id) {
		case HTC_RECORD_CREDITS:
			if (record->len <
				sizeof(struct htc_credit_report)) {
				WARN_ON(1);
				return -EINVAL;
			}

			htc_process_credit_report(target,
						  (struct htc_credit_report *)
						  record_buf,
						  record->len /
						  (sizeof
						   (struct htc_credit_report)),
						  from_ep);
			break;
		default:
			ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
				   " unhandled record: id:%d length:%d\n",
				   record->rec_id, record->len);
			break;
		}

		if (status != 0)
			break;

		/* advance buffer past this record for next time around */
		buffer += record->len;
		len -= record->len;
	}

	return status;
}

static void do_recv_completion(struct htc_pipe_endpoint *ep,
			       struct list_head *queue_to_indicate)
{
	struct htc_packet *packet;
	if (list_empty(queue_to_indicate)) {
		/* nothing to indicate */
		return;
	}

	/* using legacy EpRecv */
	while (!list_empty(queue_to_indicate)) {
		packet = list_first_entry(queue_to_indicate,
					struct htc_packet, list);
		list_del(&packet->list);
		ep->ep_callbacks.rx((struct htc_target *)
				    ep->target, packet);
	}

	return;
}

static void recv_packet_completion(struct htc_pipe_target *target,
				   struct htc_pipe_endpoint *ep,
				   struct htc_packet *packet)
{
	struct list_head container;
	INIT_LIST_HEAD(&container);
	list_add_tail(&packet->list, &container);
	/* do completion */
	do_recv_completion(ep, &container);
}

struct sk_buff *rx_sg_to_single_netbuf(struct htc_pipe_target *target)
{
	struct sk_buff *skb;
	u8 *anbdata;
	u8 *anbdata_new;
	u32 anblen;
	struct sk_buff *new_skb = NULL;
	struct sk_buff_head *rx_sg_queue = &target->rx_sg_q;

	if (skb_queue_empty(rx_sg_queue)) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			"%s: invalid sg queue len\n", __func__);
		goto _failed;
	}

	new_skb = __dev_alloc_skb(target->rx_sg_total_len_exp, GFP_KERNEL);
	if (new_skb == NULL) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			"%s: can't allocate %u size netbuf\n",
			__func__, target->rx_sg_total_len_exp);
		goto _failed;
	}

	anbdata_new = new_skb->data;
	anblen = new_skb->len;

	while ((skb = skb_dequeue(rx_sg_queue))) {
		anbdata = skb->data;
		memcpy(anbdata_new, anbdata, skb->len);
		skb_put(new_skb, skb->len);
		anbdata_new += skb->len;
		dev_kfree_skb(skb);
	};
	
	target->rx_sg_total_len_exp = 0;
	target->rx_sg_total_len_cur = 0;
	target->rx_sg_in_progress = false;

	return new_skb;

_failed:

	while ((skb = skb_dequeue(rx_sg_queue))) {
		dev_kfree_skb(skb);
	}
	target->rx_sg_total_len_exp = 0;
	target->rx_sg_total_len_cur = 0;
	target->rx_sg_in_progress = false;
	return NULL;    
}

static int htc_rx_completion(struct htc_target *context,
				struct sk_buff *netbuf, u8 pipeid)
{
	int status = 0;
	struct htc_frame_hdr *htc_hdr;
	struct htc_pipe_target *target = (struct htc_pipe_target *)context;
	u8 *netdata;
	u8 hdr_info;
	u32 netlen;
	struct htc_pipe_endpoint *ep;
	struct htc_packet *packet;
	u16 payload_len;
	u32 trailerlen = 0;

	spin_lock_bh(&target->htc_rxlock);
	if (target->rx_sg_in_progress) {
		target->rx_sg_total_len_cur += netbuf->len;
		skb_queue_tail(&target->rx_sg_q, netbuf);
		if (target->rx_sg_total_len_cur == target->rx_sg_total_len_exp) {
			netbuf = rx_sg_to_single_netbuf(target);
			if (netbuf == NULL) {
				spin_unlock_bh(&target->htc_rxlock);
				goto free_netbuf;
			}
		}
		else {
			netbuf = NULL;
			spin_unlock_bh(&target->htc_rxlock);
			goto free_netbuf;
		}
	}
	
	spin_unlock_bh(&target->htc_rxlock);

	netdata = netbuf->data;
	netlen = netbuf->len;

	htc_hdr = (struct htc_frame_hdr *)netdata;

	ep = &target->endpoint[htc_hdr->eid];

	if (htc_hdr->eid >= ENDPOINT_MAX) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "HTC Rx: invalid EndpointID=%d\n",
			   htc_hdr->eid);
		status = -EINVAL;
		goto free_netbuf;
	}

	payload_len = le16_to_cpu(get_unaligned(&htc_hdr->payld_len));

	if (netlen < (payload_len + HTC_HDR_LENGTH)) {
		spin_lock_bh(&target->htc_rxlock);
		target->rx_sg_in_progress = true;
		skb_queue_head_init(&target->rx_sg_q);
		skb_queue_tail(&target->rx_sg_q, netbuf);
		target->rx_sg_total_len_exp = (payload_len + HTC_HDR_LENGTH);
		target->rx_sg_total_len_cur += netlen;
		spin_unlock_bh(&target->htc_rxlock);
		netbuf = NULL;
		goto free_netbuf;
	}

	/* get flags to check for trailer */
	hdr_info = htc_hdr->flags;
	if (hdr_info & HTC_FLG_RX_TRAILER) {
		/* extract the trailer length */
		hdr_info = htc_hdr->ctrl[0];
		if ((hdr_info < sizeof(struct htc_record_hdr))
		    || (hdr_info > payload_len)) {
			ath6kl_dbg(ATH6KL_DBG_HTC_RECV,
				   "invalid header: payloadlen"
				   "should be %d, CB[0]: %d\n",
				   payload_len, hdr_info);
			status = -EINVAL;
			goto free_netbuf;
		}

		trailerlen = hdr_info;
		/* process trailer after hdr/apps payload */
		status = htc_process_trailer(target,
					     ((u8 *) htc_hdr +
					      HTC_HDR_LENGTH +
					      payload_len -
					      hdr_info), hdr_info,
					     htc_hdr->eid);
		if (status != 0)
			goto free_netbuf;
	}

	if (((int)payload_len - (int)trailerlen) <= 0) {
		/* zero length packet with trailer, just drop these */
		goto free_netbuf;
	}

	if (htc_hdr->eid == ENDPOINT_0) {
		/* handle HTC control message */
		if (target->ctrl_response_processing) {
			/*
			 * fatal: target should not send unsolicited
			 * messageson the endpoint 0
			 */
			ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
				   "HTC Rx Ctrl still processing\n");
			status = -EINVAL;
			goto free_netbuf;
		}

		/* remove HTC header */
		skb_pull(netbuf, HTC_HDR_LENGTH);

		netdata = netbuf->data;
		netlen = netbuf->len;

		spin_lock_bh(&target->htc_rxlock);

		target->ctrl_response_valid = true;
		target->ctrl_response_len =
		    min((int)netlen, HTC_MAX_CTRL_MSG_LEN);
		memcpy(target->ctrl_response_buf, netdata,
		       target->ctrl_response_len);

		spin_unlock_bh(&target->htc_rxlock);

		dev_kfree_skb(netbuf);
		netbuf = NULL;
		goto free_netbuf;
	}

	/*
	 * TODO: the message based HIF architecture allocates net bufs
	 * for recv packets since it bridges that HIF to upper layers,
	 * which expects HTC packets, we form the packets here
	 */
	packet = alloc_htc_packet_container(target);
	if (packet == NULL) {
		status = -ENOMEM;
		goto free_netbuf;
	}
	packet->status = 0;
	packet->endpoint = htc_hdr->eid;
	packet->pkt_cntxt = netbuf;
	/* TODO: for backwards compatibility */
	packet->buf = skb_push(netbuf, 0) + HTC_HDR_LENGTH;
	packet->act_len = netlen - HTC_HDR_LENGTH - trailerlen;

	/*
	 * TODO: this is a hack because the driver layer will set the
	 * actual len of the skb again which will just double the len
	 */
	skb_trim(netbuf, 0);

	recv_packet_completion(target, ep, packet);
	/* recover the packet container */
	free_htc_packet_container(target, packet);
	netbuf = NULL;

free_netbuf:
	if (netbuf != NULL)
		dev_kfree_skb(netbuf);

	return status;

}

void htc_flush_rx_queue(struct htc_pipe_target *target,
			struct htc_pipe_endpoint *ep)
{
	struct htc_packet *packet;
	struct list_head container;

	spin_lock_bh(&target->htc_rxlock);

	while (1) {
		if (list_empty(&ep->rx_buffer_queue))
			break;
		packet = list_first_entry(&ep->rx_buffer_queue,
					struct htc_packet, list);
		list_del(&packet->list);

		spin_unlock_bh(&target->htc_rxlock);
		packet->status = -ECANCELED;
		packet->act_len = 0;
		ath6kl_dbg(ATH6KL_DBG_HTC_RECV,
			   "  Flushing RX packet:0x%lX, length:%d, ep:%d\n",
			   (unsigned long)packet, packet->buf_len,
			   packet->endpoint);
		INIT_LIST_HEAD(&container);
		list_add_tail(&packet->list, &container);
		/* give the packet back */
		do_recv_completion(ep, &container);
		spin_lock_bh(&target->htc_rxlock);
	}

	spin_unlock_bh(&target->htc_rxlock);
}

/* polling routine to wait for a control packet to be received */
int htc_wait_recv_ctrl_message(struct htc_pipe_target *target)
{
	int count = HTC_TARGET_MAX_RESPONSE_POLL;

	while (count > 0) {
		spin_lock_bh(&target->htc_rxlock);

		if (target->ctrl_response_valid) {
			target->ctrl_response_valid = false;
			/* caller will clear this flag */
			target->ctrl_response_processing = true;
			spin_unlock_bh(&target->htc_rxlock);
			break;
		}

		spin_unlock_bh(&target->htc_rxlock);

		count--;

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout((HZ / 1000) * (HTC_TARGET_RESPONSE_POLL_MS));
		set_current_state(TASK_RUNNING);
	}
	if (count <= 0) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "%s: Timeout!\n", __func__);
		return -ECOMM;
	}

	return 0;
}

void htc_rxctrl_complete(struct htc_target *context, struct htc_packet *packet)
{
	/* TODO, can't really receive HTC control messages yet.... */
	ath6kl_dbg(ATH6KL_DBG_HTC_PIPE, "%s: invalid call function\n",
			__func__);
}

/* htc pipe initialization */
static void reset_endpoint_states(struct htc_pipe_target *target)
{
	struct htc_pipe_endpoint *ep;
	int i;

	for (i = ENDPOINT_0; i < ENDPOINT_MAX; i++) {
		ep = &target->endpoint[i];
		ep->service_id = 0;
		ep->max_msg_len = 0;
		ep->max_txqueue_depth = 0;
		ep->id = i;
		INIT_LIST_HEAD(&ep->tx_queue);
		INIT_LIST_HEAD(&ep->tx_lookup_queue);
		INIT_LIST_HEAD(&ep->rx_buffer_queue);
		ep->target = target;
		ep->tx_credit_flow_enabled = (bool) 1;
		ep->starving = 0;
		ep->last_sent = jiffies;
	}
	INIT_LIST_HEAD(&target->sync_queue);
	target->sync_id_sending = 0;
	target->sync_id_latest = 1;
}

/* start HTC, this is called after all services are connected */
static int htc_config_target_hif_pipe(struct htc_pipe_target *target)
{
	return 0;
}

/* htc service functions */
u8 htc_get_credit_alloc(struct htc_pipe_target *target, u16 service_id)
{
	u8 allocation = 0;
	int i;

	for (i = 0; i < HTC_MAX_SERVICE_ALLOC_ENTRIES; i++) {
		if (target->txcredit_alloc[i].service_id == service_id)
			allocation = target->txcredit_alloc[i].credit_alloc;
	}

	if (allocation == 0) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "HTC Service TX : 0x%2.2X : allocation is zero!\n",
			   service_id);
	}

	return allocation;
}

int ath6kl_htc_conn_service(struct htc_target *handle,
		     struct htc_service_connect_req *conn_req,
		     struct htc_service_connect_resp *conn_resp)
{
	struct ath6kl *ar = handle->dev->ar;
	struct htc_pipe_target *target = (struct htc_pipe_target *) handle;
	int status = 0;
	struct htc_packet *packet = NULL;
	struct htc_conn_service_resp *resp_msg;
	struct htc_conn_service_msg *conn_msg;
	enum htc_endpoint_id assigned_epid = ENDPOINT_MAX;
	struct htc_pipe_endpoint *ep;
	unsigned int max_msg_size = 0;
	struct sk_buff *netbuf;
	u8 tx_alloc;
	int length;
	bool disable_credit_flowctrl = false;

	if (conn_req->svc_id == 0) {
		WARN_ON(1);
		status = -EINVAL;
		goto free_packet;
	}

	if (conn_req->svc_id == HTC_CTRL_RSVD_SVC) {
		/* special case for pseudo control service */
		assigned_epid = ENDPOINT_0;
		max_msg_size = HTC_MAX_CTRL_MSG_LEN;
		tx_alloc = 0;

	} else {

		tx_alloc =
		    htc_get_credit_alloc(target, conn_req->svc_id);
		if (tx_alloc == 0) {
			status = -ENOMEM;
			goto free_packet;
		}

		/* allocate a packet to send to the target */
		packet = htc_alloc_txctrl_packet(target);

		if (packet == NULL) {
			WARN_ON(1);
			status = -ENOMEM;
			goto free_packet;
		}

		netbuf = packet->netbufcontext;
		length = sizeof(struct htc_conn_service_msg);

		/* assemble connect service message */
		conn_msg =
		    (struct htc_conn_service_msg *)skb_put(netbuf,
							   length);
		if (conn_msg == NULL) {
			WARN_ON(1);
			status = -EINVAL;
			goto free_packet;
		}

		memset(conn_msg, 0,
		       sizeof(struct htc_conn_service_msg));
		conn_msg->msg_id = HTC_MSG_CONN_SVC_ID;
		conn_msg->svc_id = conn_req->svc_id;
		conn_msg->conn_flags =
		    conn_req->conn_flags & ~HTC_SET_RECV_ALLOC_MASK;
		/* tell target desired recv alloc for this ep */
		conn_msg->conn_flags |=
			tx_alloc << HTC_SET_RECV_ALLOC_SHIFT;

		if (conn_req->conn_flags &
		    HTC_CONN_FLGS_DISABLE_CRED_FLOW_CTRL) {
			disable_credit_flowctrl = true;
		}

		set_htc_pkt_info(packet, NULL, (u8 *) conn_msg,
				 length,
				 ENDPOINT_0, HTC_SERVICE_TX_PACKET_TAG);

		status = ath6kl_htc_tx(handle, packet);
		/* we don't own it anymore */
		packet = NULL;
		if (status != 0)
			goto free_packet;

		/* wait for response */
		status = htc_wait_recv_ctrl_message(target);
		if (status != 0)
			goto free_packet;

		/* we controlled the buffer creation so it has to be
		 * properly aligned
		 */
		resp_msg = (struct htc_conn_service_resp *)
		    target->ctrl_response_buf;

		if ((resp_msg->msg_id != HTC_MSG_CONN_SVC_RESP_ID)
		    || (target->ctrl_response_len <
			sizeof(struct htc_conn_service_resp))) {
			/* this message is not valid */
			WARN_ON(1);
			status = -EINVAL;
			goto free_packet;
		}

		ath6kl_dbg(ATH6KL_DBG_TRC,
			   "%s: service 0x%X conn resp: "
			   "status: %d ep: %d\n", __func__,
			   resp_msg->svc_id, resp_msg->status,
			   resp_msg->eid);

		conn_resp->resp_code = resp_msg->status;
		/* check response status */
		if (resp_msg->status != HTC_SERVICE_SUCCESS) {
			ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
				   " Target failed service 0x%X connect"
				   " request (status:%d)\n",
				   resp_msg->svc_id, resp_msg->status);
			status = -EINVAL;
			goto free_packet;
		}

		assigned_epid = (enum htc_endpoint_id)resp_msg->eid;
		max_msg_size = resp_msg->max_msg_sz;

		/* done processing response buffer */
		target->ctrl_response_processing = false;
	}

	/* the rest are parameter checks so set the error status */
	status = -EINVAL;

	if (assigned_epid >= ENDPOINT_MAX) {
		WARN_ON(1);
		goto free_packet;
	}

	if (max_msg_size == 0) {
		WARN_ON(1);
		goto free_packet;
	}

	ep = &target->endpoint[assigned_epid];
	ep->id = assigned_epid;
	if (ep->service_id != 0) {
		/* endpoint already in use! */
		WARN_ON(1);
		goto free_packet;
	}

	/* return assigned endpoint to caller */
	conn_resp->endpoint = assigned_epid;
	conn_resp->len_max = max_msg_size;

	/* setup the endpoint */
	ep->service_id = conn_req->svc_id; /* this marks ep in use */
	ep->max_txqueue_depth = conn_req->max_txq_depth;
	ep->max_msg_len = max_msg_size;
	ep->tx_credits = tx_alloc;
	ep->tx_credit_size = target->target_credit_size;
	ep->tx_credits_per_maxmsg =
	    max_msg_size / target->target_credit_size;
	if (max_msg_size % target->target_credit_size)
		ep->tx_credits_per_maxmsg++;

	/* copy all the callbacks */
	ep->ep_callbacks = conn_req->ep_cb;

	status = hif_map_service_pipe(
				ar,
				ep->service_id,
				&ep->pipeid_ul, &ep->pipeid_dl);
	if (status != 0)
		goto free_packet;

	ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
		   "SVC Ready: 0x%4.4X: ULpipe:%d DLpipe:%d id:%d\n",
		   ep->service_id, ep->pipeid_ul,
		   ep->pipeid_dl, ep->id);

	if (disable_credit_flowctrl && ep->tx_credit_flow_enabled) {
		ep->tx_credit_flow_enabled = false;
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "SVC: 0x%4.4X ep:%d TX flow control off\n",
			   ep->service_id, assigned_epid);
	}

free_packet:
	if (packet != NULL)
		htc_free_txctrl_packet(target, packet);
	return status;
}

/* htc export functions */
void *ath6kl_htc_create(struct ath6kl *ar)
{
	int status = 0;
	struct hif_callbacks htc_callbacks;
	struct htc_pipe_endpoint *ep = NULL;
	struct htc_pipe_target *target = NULL;
	struct htc_packet *packet;
	int i;

	target = kzalloc(sizeof(struct htc_pipe_target), GFP_KERNEL);
	if (target == NULL) {
		ath6kl_err("htc create unable to allocate memory\n");
		status = -ENOMEM;
		goto fail_htc_create;
	}

	memset(target, 0, sizeof(struct htc_pipe_target));

	spin_lock_init(&target->htc_lock);
	spin_lock_init(&target->htc_rxlock);
	spin_lock_init(&target->htc_txlock);

	reset_endpoint_states(target);

	for (i = 0; i < HTC_PACKET_CONTAINER_ALLOCATION; i++) {
		packet = (struct htc_packet *)
		    kzalloc(sizeof(struct htc_packet), GFP_KERNEL);
		if (packet != NULL) {
			memset(packet, 0, sizeof(struct htc_packet));
			free_htc_packet_container(target, packet);
		}
	}
	/* setup HIF layer callbacks */
	memset(&htc_callbacks, 0, sizeof(struct hif_callbacks));
	htc_callbacks.rx_completion = htc_rx_completion;
	htc_callbacks.tx_completion = htc_tx_completion;
	htc_callbacks.tx_resource_available = htc_tx_resource_available;

	target->parent.dev =
	    kzalloc(sizeof(*target->parent.dev), GFP_KERNEL);
	if (!target->parent.dev) {
		ath6kl_err("unable to allocate memory\n");
		status = -ENOMEM;
		goto fail_htc_create;
	}
	target->parent.dev->ar = ar;
	target->parent.dev->htc_cnxt = (struct htc_target *)target;

	/* Get HIF default pipe for HTC message exchange */
	ep = &target->endpoint[ENDPOINT_0];

	hif_postinit(ar, target, &htc_callbacks);
	hif_get_default_pipe(ar, &ep->pipeid_ul,
			     &ep->pipeid_dl);

	return (void *)target;

fail_htc_create:
	if (status != 0) {
		if (target != NULL)
			ath6kl_htc_cleanup((struct htc_target *)target);
		target = NULL;
	}
	return (void *)target;
}

/* cleanup the HTC instance */
void ath6kl_htc_cleanup(struct htc_target *handle)
{
	struct htc_packet *packet;
	struct ath6kl *ar = handle->dev->ar;
	struct htc_pipe_target *target = (struct htc_pipe_target *) handle;

	if (ar->hif_priv != NULL) {
		hif_detach_htc(ar);
	}

	while (true) {
		packet = alloc_htc_packet_container(target);
		if (packet == NULL)
			break;
		kfree(packet);
	}
	kfree(target->parent.dev);
	/* kfree our instance */
	kfree(target);
}

int ath6kl_htc_start(struct htc_target *handle)
{
	struct sk_buff *netbuf;
	struct htc_pipe_target *target = (struct htc_pipe_target *) handle;
	struct htc_setup_comp_ext_msg *setup_comp;
	struct htc_packet *packet;

	htc_config_target_hif_pipe(target);
	/* allocate a buffer to send */
	packet = htc_alloc_txctrl_packet(target);
	if (packet == NULL) {
		WARN_ON(1);
		return -ENOMEM;
	}

	netbuf = packet->netbufcontext;
	/* assemble setup complete message */
	setup_comp =
	    (struct htc_setup_comp_ext_msg *)skb_put(netbuf,
				sizeof(struct htc_setup_comp_ext_msg));
	memset(setup_comp, 0, sizeof(struct htc_setup_comp_ext_msg));
	setup_comp->msg_id = HTC_MSG_SETUP_COMPLETE_EX_ID;

	if (0) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "HTC will not use TX credit flow control\n");
		setup_comp->flags |=
		    HTC_SETUP_COMPLETE_FLAGS_DISABLE_TX_CREDIT_FLOW;
	} else {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "HTC using TX credit flow control\n");
	}

    if (htc_bundle_recv) {
        ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
                "HTC will use bundle recv\n");
        setup_comp->flags |= 
            HTC_SETUP_COMPLETE_FLAGS_ENABLE_BUNDLE_RECV;
    } else {
        ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
                "HTC will not use bundle recv\n");
    }
    
	set_htc_pkt_info(packet, NULL, (u8 *) setup_comp,
			 sizeof(struct htc_setup_comp_ext_msg),
			 ENDPOINT_0, HTC_SERVICE_TX_PACKET_TAG);

	return ath6kl_htc_tx(handle, packet);
}

void ath6kl_htc_stop(struct htc_target *handle)
{
	struct ath6kl *ar = handle->dev->ar;
	struct htc_pipe_target *target = (struct htc_pipe_target *) handle;
	int i;
	struct htc_pipe_endpoint *ep;
	struct sk_buff *skb;
	struct sk_buff_head *rx_sg_queue = &target->rx_sg_q;

	/* cleanup endpoints */
	for (i = 0; i < ENDPOINT_MAX; i++) {
		ep = &target->endpoint[i];
		htc_flush_rx_queue(target, ep);
		htc_flush_tx_endpoint(target, ep, HTC_TX_PACKET_TAG_ALL);
	}

	spin_lock_bh(&target->htc_rxlock);
	while ((skb = skb_dequeue(rx_sg_queue))) {
		dev_kfree_skb(skb);
	}
	target->rx_sg_total_len_exp = 0;
	target->rx_sg_total_len_cur = 0;
	target->rx_sg_in_progress = false;

	spin_unlock_bh(&target->htc_rxlock);

	hif_stop(ar);
	reset_endpoint_states(target);
}

int ath6kl_htc_get_rxbuf_num(struct htc_target *htc_context,
		      enum htc_endpoint_id endpoint)
{
	struct htc_pipe_target *target =
	    (struct htc_pipe_target *) htc_context;
	int num;

	spin_lock_bh(&target->htc_rxlock);
	num = get_queue_depth(&(target->endpoint[endpoint].rx_buffer_queue));
	spin_unlock_bh(&target->htc_rxlock);

	return num;
}

int ath6kl_htc_tx(struct htc_target *handle, struct htc_packet *packet)
{
	struct list_head queue;
	ath6kl_dbg(ATH6KL_DBG_HTC_SEND,
		   "%s: endPointId: %d, buffer: 0x%lX, length: %d\n",
		   __func__, packet->endpoint,
		   (unsigned long)packet->buf,
		   packet->act_len);
	INIT_LIST_HEAD(&queue);
	list_add_tail(&packet->list, &queue);
	return htc_send_packets_multiple(handle, &queue);
}

int ath6kl_htc_wait_target(struct htc_target *handle)
{
	int status = 0;
	struct ath6kl *ar = handle->dev->ar;
	struct htc_pipe_target *target = (struct htc_pipe_target *) handle;
	struct htc_ready_ext_msg *ready_msg;
	struct htc_service_connect_req connect;
	struct htc_service_connect_resp resp;

	hif_start(ar);
	status = htc_wait_recv_ctrl_message(target);

	if (status != 0)
		return status;

	if (target->ctrl_response_len <
	    (sizeof(struct htc_ready_ext_msg))) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "invalid htc ready msg len:%d!\n",
			   target->ctrl_response_len);
		return -ECOMM;
	}
	ready_msg =
	    (struct htc_ready_ext_msg *)target->ctrl_response_buf;

	if (ready_msg->ver2_0_info.msg_id != HTC_MSG_READY_ID) {
		ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
			   "invalid htc ready msg : 0x%X !\n",
			   ready_msg->ver2_0_info.msg_id);
		return -ECOMM;
	}

	ath6kl_dbg(ATH6KL_DBG_HTC_PIPE,
		   "Target Ready! : transmit resources : %d size:%d\n",
		   ready_msg->ver2_0_info.cred_cnt,
		   ready_msg->ver2_0_info.cred_sz);

	target->total_transmit_credits =
	    ready_msg->ver2_0_info.cred_cnt;
	/* reserve one for control path */
	target->avail_tx_credits = target->total_transmit_credits - 1;
	target->target_credit_size =
	    (int)ready_msg->ver2_0_info.cred_sz;
	if ((target->total_transmit_credits == 0)
	    || (target->target_credit_size == 0)) {
		return -ECOMM;
	}
	/* done processing */
	target->ctrl_response_processing = false;

	htc_setup_target_buffer_assignments(target);

	/* setup our pseudo HTC control endpoint connection */
	memset(&connect, 0, sizeof(connect));
	memset(&resp, 0, sizeof(resp));
	connect.ep_cb.tx_complete = htc_txctrl_complete;
	connect.ep_cb.rx = htc_rxctrl_complete;
	connect.max_txq_depth = NUM_CONTROL_TX_BUFFERS;
	connect.svc_id = HTC_CTRL_RSVD_SVC;

	/* connect fake service */
	status = ath6kl_htc_conn_service((void *)target, &connect, &resp);
	return status;
}

void ath6kl_htc_flush_txep(struct htc_target *handle,
		enum htc_endpoint_id Endpoint, HTC_TX_TAG Tag)
{
	struct htc_pipe_target *target = (struct htc_pipe_target *) handle;
	struct htc_pipe_endpoint *ep = &target->endpoint[Endpoint];

	if (ep->service_id == 0) {
		WARN_ON(1);
		/* not in use.. */
		return;
	}

	htc_flush_tx_endpoint(target, ep, Tag);
}

int ath6kl_htc_add_rxbuf_multiple(struct htc_target *handle,
			   struct list_head *pkt_queue)
{
	struct htc_pipe_target *target = (struct htc_pipe_target *) handle;
	struct htc_pipe_endpoint *ep;
	struct htc_packet *pFirstPacket;
	int status = 0;
	struct htc_packet *packet, *tmp_pkt;

	if (list_empty(pkt_queue))
		return -EINVAL;

	pFirstPacket = list_first_entry(pkt_queue, struct htc_packet, list);
	if (pFirstPacket == NULL) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (pFirstPacket->endpoint >= ENDPOINT_MAX) {
		WARN_ON(1);
		return -EINVAL;
	}

	ath6kl_dbg(ATH6KL_DBG_HTC_RECV,
		   "%s: epid: %d, cnt:%d, len: %d\n",
		   __func__, pFirstPacket->endpoint,
		   get_queue_depth(pkt_queue), pFirstPacket->buf_len);

	ep = &target->endpoint[pFirstPacket->endpoint];

	spin_lock_bh(&target->htc_rxlock);

	if (target->htc_state_flags & HTC_STATE_STOPPING) {
		status = -EPERM;
	} else {
		/* store receive packets */
		list_splice_tail_init(pkt_queue,
						&ep->rx_buffer_queue);
	}

	spin_unlock_bh(&target->htc_rxlock);

	if (status != 0) {
		/* walk through queue and mark each one canceled */
		list_for_each_entry_safe(packet, tmp_pkt, pkt_queue, list) {
			packet->status = -ECANCELED;
		}

		do_recv_completion(ep, pkt_queue);
	}

	return status;
}

void ath6kl_htc_indicate_activity_change(
				struct htc_target *handle,
				enum htc_endpoint_id Endpoint,
				bool Active)
{
	/* TODO */
}

void ath6kl_htc_flush_rx_buf(struct htc_target *target)
{
	/* TODO */
}

void ath6kl_htc_set_credit_dist(struct htc_target *target,
			 struct htc_credit_state_info *cred_info,
			 u16 svc_pri_order[], int len)
{
	/*
	 * not supported since this transport does not use a credit
	 * based flow control mechanism
	 */
}

int ath6kl_htc_stat(struct htc_target *target,
						 u8 *buf, int buf_len)
{
	struct htc_pipe_target *handle = (struct htc_pipe_target *)target;
	struct htc_pipe_endpoint *ep;
	struct htc_endpoint_stats *ep_st;
	struct list_head *tx_queue;
	int i, tmp, len = 0;

	if ((!handle) ||
		(!buf))
		return 0;

	len += snprintf(buf + len, buf_len - len, " \nCredit Size : %d         Avail Credit : %d/%d\n", 
												handle->target_credit_size,
												handle->avail_tx_credits,
												handle->total_transmit_credits);

	for (i = ENDPOINT_2; i < ENDPOINT_5; i++) {
		len += snprintf(buf + len, buf_len - len, "EP-%d\n", i);

		ep = &handle->endpoint[i];
		tx_queue = &ep->tx_queue;
		ep_st = &ep->ep_st;

		spin_lock_bh(&handle->htc_txlock);
		tmp = get_queue_depth(tx_queue);
		spin_unlock_bh(&handle->htc_txlock);

		len += snprintf(buf + len, buf_len - len, " tx_queue            : %d/%d\n", tmp, ep->max_txqueue_depth);
		len += snprintf(buf + len, buf_len - len, " pipeid_ul/dl        : %d/%d\n", ep->pipeid_ul, ep->pipeid_dl);
		len += snprintf(buf + len, buf_len - len, " seq_no              : %d\n", ep->seq_no);
		len += snprintf(buf + len, buf_len - len, " cred_low_indicate   : %d\n", ep_st->cred_low_indicate);
		len += snprintf(buf + len, buf_len - len, " cred_rpt_from_rx    : %d\n", ep_st->cred_rpt_from_rx);
		len += snprintf(buf + len, buf_len - len, " cred_rpt_from_other : %d\n", ep_st->cred_rpt_from_other);
		len += snprintf(buf + len, buf_len - len, " cred_rpt_ep0        : %d\n", ep_st->cred_rpt_ep0);
		len += snprintf(buf + len, buf_len - len, " cred_from_rx        : %d\n", ep_st->cred_from_rx);
		len += snprintf(buf + len, buf_len - len, " cred_from_other     : %d\n", ep_st->cred_from_other);
		len += snprintf(buf + len, buf_len - len, " cred_from_ep0       : %d\n", ep_st->cred_from_ep0);
		len += snprintf(buf + len, buf_len - len, " cred_cosumd         : %d\n", ep_st->cred_cosumd);
		len += snprintf(buf + len, buf_len - len, " cred_retnd          : %d\n", ep_st->cred_retnd);
		len += snprintf(buf + len, buf_len - len, " tx_issued           : %d\n", ep_st->tx_issued);
		len += snprintf(buf + len, buf_len - len, " tx_dropped          : %d\n", ep_st->tx_dropped);
		len += snprintf(buf + len, buf_len - len, " tx_cred_rpt         : %d\n", ep_st->tx_cred_rpt);
		len += snprintf(buf + len, buf_len - len, " rx_pkts             : %d\n", ep_st->rx_pkts);
		len += snprintf(buf + len, buf_len - len, " rx_lkahds           : %d\n", ep_st->rx_lkahds);
		len += snprintf(buf + len, buf_len - len, " rx_alloc_thresh_hit : %d\n", ep_st->rx_alloc_thresh_hit);
		len += snprintf(buf + len, buf_len - len, " rxalloc_thresh_byte : %d\n", ep_st->rxalloc_thresh_byte);

		/* Bundle mode */
		if (htc_bundle_recv || htc_bundle_send) {
			len += snprintf(buf + len, buf_len - len, " tx_pkt_bundled      : %d\n", ep_st->tx_pkt_bundled);
			len += snprintf(buf + len, buf_len - len, " tx_bundles          : %d\n", ep_st->tx_bundles);
			len += snprintf(buf + len, buf_len - len, " rx_bundl            : %d\n", ep_st->rx_bundl);
			len += snprintf(buf + len, buf_len - len, " rx_bundle_lkahd     : %d\n", ep_st->rx_bundle_lkahd);
			len += snprintf(buf + len, buf_len - len, " rx_bundle_from_hdr  : %d\n", ep_st->rx_bundle_from_hdr);
		}
	}

	return len;
}
