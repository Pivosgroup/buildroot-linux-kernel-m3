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

#ifndef HTC_PIPE_INTERNAL_H
#define HTC_PIPE_INTERNAL_H

#include <asm/unaligned.h>

#include "htc.h"
#include "common.h"

#include "hif_usb.h"

/* htc pipe operational parameters */
#define HTC_TARGET_RESPONSE_TIMEOUT     2000	/* in ms */
#define HTC_TARGET__DEBUG_INTR_MASK     0x01
#define HTC_TARGET__CREDIT_INTR_MASK    0xF0

#define HTC_PACKET_CONTAINER_ALLOCATION 32
#define NUM_CONTROL_TX_BUFFERS          2
#define HTC_CONTROL_BUFFER_SIZE         (HTC_MAX_CTRL_MSG_LEN + HTC_HDR_LENGTH)
#define HTC_CONTROL_BUFFER_ALIGN        32
#define HTC_TARGET_RESPONSE_POLL_MS     10
#define HTC_TARGET_MAX_RESPONSE_POLL    200

struct htc_pipe_endpoint {
	enum htc_endpoint_id id;
	u16 service_id;		/* service ID this endpoint is bound to
				   non-zero value means this ep is in use */
	struct htc_ep_callbacks ep_callbacks;	/* cb associated with this ep */
	struct list_head tx_queue;	/* HTC frame buffer TX queue */
	int max_txqueue_depth;	/* max depth of the TX queue before we need to
				   call driver's full handler */
	int max_msg_len;	/* max length of endpoint message */
	u8 pipeid_ul;
	u8 pipeid_dl;

	/* queue to match skbs to htc packets */
	struct list_head tx_lookup_queue;
	/* temporary hold queue for back compatibility */
	struct list_head rx_buffer_queue;

	u8 seq_no;		/* TX seq no (helpful) for debugging */
	int tx_process_count;	/* serialization */
	struct htc_pipe_target *target;
	/* htc pipe credit management */
	bool tx_credit_flow_enabled;
	int tx_credits;		/* TX credits available on this endpoint */
	int tx_credits_per_maxmsg;	/* credits required (precalculated) */
	int tx_credit_size;	/* size in bytes of each credit (set by HTC) */
	/* statistics */
	struct htc_endpoint_stats ep_st;	/* endpoint statistics */
        u8 starving;
        unsigned long last_sent;
};

struct htc_pipe_txcredit_alloc {
	u16 service_id;
	u8 credit_alloc;
};

/* enable recv bundling from target */
#define HTC_SETUP_COMPLETE_FLAGS_ENABLE_BUNDLE_RECV     (1 << 0)
/* disable credit based flow control, */
#define HTC_SETUP_COMPLETE_FLAGS_DISABLE_TX_CREDIT_FLOW (1 << 1)

#define HTC_MAX_SERVICE_ALLOC_ENTRIES 8

/* our HTC target state */
struct htc_pipe_target {
	struct htc_target parent;	/* parent target structures */
	struct htc_pipe_endpoint endpoint[ENDPOINT_MAX];
	spinlock_t htc_lock;
	spinlock_t htc_rxlock;
	spinlock_t htc_txlock;
	u32 htc_state_flags;
	struct htc_packet *htc_packet_pool;	/* pool of HTC packets */
	u8 ctrl_response_buf[HTC_MAX_CTRL_MSG_LEN];
	int ctrl_response_len;
	bool ctrl_response_valid;
	bool ctrl_response_processing;
	int total_transmit_credits;
	int avail_tx_credits;
	struct htc_pipe_txcredit_alloc
	 txcredit_alloc[HTC_MAX_SERVICE_ALLOC_ENTRIES];
	int target_credit_size;

	struct sk_buff_head rx_sg_q;
	bool rx_sg_in_progress;
	u32 rx_sg_total_len_cur; /* current total length */
	u32 rx_sg_total_len_exp; /* expected total length */

	/* sync packet handling */
	struct list_head sync_queue;
	int sync_id_latest;
	int sync_id_sending;
	int sync_processing;
};

enum htc_send_queue_result {
	HTC_SEND_QUEUE_OK = 0,	/* packet was queued */
	HTC_SEND_QUEUE_DROP = 1,	/* this packet should be dropped */
};

#define HTC_STATE_STOPPING      (1 << 0)

/* macros for htc pipe services */
#define HTC_SET_RECV_ALLOC_SHIFT    8
#define HTC_SET_RECV_ALLOC_MASK     0xFF00

/* internal HTC functions */

/* Macros to manipulate HTC PIPE packets */
typedef u16 HTC_TX_TAG;

#define HTC_TX_PACKET_FLAG_FIXUP_NETBUF (1 << 0)

#endif
