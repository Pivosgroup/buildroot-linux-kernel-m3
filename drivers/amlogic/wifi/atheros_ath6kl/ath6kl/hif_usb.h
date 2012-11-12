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

#ifndef HIF_USB_H
#define HIF_USB_H

#include <linux/usb.h>
#include "hif.h"

/**
 * @brief List of callbacks - filled in by HTC.
 */
struct hif_callbacks {
	int (*tx_completion) (struct htc_target *context, struct sk_buff * skb);
	int (*rx_completion) (struct htc_target *context,
				struct sk_buff *skb, u8 pipe);
	void (*tx_resource_available) (struct htc_target *context, u8 pipe);
};

/**
 * @brief: This API is used by the HTC layer to initialize the HIF layer and to
 * register different callback routines. Support for following events has
 * been captured - DSR, Read/Write completion, Device insertion/removal,
 * Device suspension/resumption/wakeup. In addition to this, the API is
 * also used to register the name and the revision of the chip. The latter
 * can be used to verify the revision of the chip read from the device
 * before reporting it to HTC.
 * @param[in]: callbacks - List of HTC callbacks
 * @param[out]:
 * @return: an opaque HIF handle
 */
void hif_postinit(struct ath6kl *ar,
		void *htc_context,
		struct hif_callbacks *callbacks);

void hif_start(struct ath6kl *ar);

void hif_stop(struct ath6kl *ar);

/**
 * @brief: Send a buffer to HIF for transmission to the target.
 * @param[in]: dev - HIF handle
 * @param[in]: pipeID - pipe to use
 * @param[in]: netbuf - buffer to send
 * @param[out]:
 * @return: Status of the send operation.
 */
int hif_send(struct ath6kl *ar, u8 pipe, struct sk_buff *hdr_buf,
	     struct sk_buff *buf);

int hif_send_multiple(struct ath6kl *ar, u8 PipeID, struct sk_buff **msg_bundle, 
        int num_msgs);

int hif_send_sync(struct ath6kl *ar, u8 PipeID, struct sk_buff *buf,
        int timeout);
         
void hif_get_default_pipe(struct ath6kl *ar, u8 *pipe_ul, u8 *pipe_dl);

int hif_map_service_pipe(struct ath6kl *ar, u16 service_id, u8 *pipe_ul,
			 u8 *pipe_dl);

u16 hif_get_free_queue_number(struct ath6kl *ar, u8 pipe);
u16 hif_get_max_queue_number(struct ath6kl *ar, u8 pipe);

void hif_detach_htc(struct ath6kl *ar);

/* constants */
#define VENDOR_ATHR             0x0CF3
#define VENDOR_PANA             0x04DA

#define TX_URB_COUNT            32
#define RX_URB_COUNT            32
#define HIF_USB_RX_BUFFER_SIZE  2048
#define HIF_USB_RX_BUNDLE_BUFFER_SIZE  16896
#define HIF_USB_TX_BUNDLE_BUFFER_SIZE  16384

/* tx/rx pipes for usb */
enum HIF_USB_PIPE_ID {
	HIF_TX_CTRL_PIPE = 0,
	HIF_TX_DATA_LP_PIPE,
	HIF_TX_DATA_MP_PIPE,
	HIF_TX_DATA_HP_PIPE,
	HIF_RX_CTRL_PIPE,
	HIF_RX_DATA_PIPE,
	HIF_RX_DATA2_PIPE,
	HIF_RX_INT_PIPE,
	HIF_USB_PIPE_MAX
};

#define HIF_USB_PIPE_INVALID HIF_USB_PIPE_MAX

struct hif_usb_pipe_stat {
	u32 num_rx_comp;
	u32 num_tx_comp;
	u32 num_io_comp;

	u32 num_max_tx;
	u32 num_max_rx;

	u32 num_tx_resche;
	u32 num_rx_resche;

	u32 num_tx_sync;
	u32 num_tx;
	u32 num_tx_err;
	u32 num_tx_err_others;

	u32 num_tx_comp_err;
	u32 num_rx_comp_err;

	u32 num_rx_bundle_comp;
	u32 num_tx_bundle_comp;

	u32 num_tx_multi;
	u32 num_tx_multi_err;
	u32 num_tx_multi_err_others;

	u32 num_rx_bundle_comp_err;
	u32 num_tx_bundle_comp_err;
};

struct hif_usb_pipe {
	struct list_head urb_list_head;
	struct usb_anchor urb_submitted;
	u32 urb_alloc;
	u32 urb_cnt;
	u32 urb_cnt_thresh;
	unsigned int usb_pipe_handle;
	u32 flags;
	u8 ep_address;
	u8 logical_pipe_num;
	struct ath6kl_usb *ar_usb;
	u16 max_packet_size;
	struct work_struct io_complete_work;
	struct sk_buff_head io_comp_queue;
	struct usb_endpoint_descriptor *ep_desc;
	struct hif_usb_pipe_stat usb_pipe_stat;
};

#define HIF_USB_PIPE_FLAG_TX    (1 << 0)

/* usb device object */
struct ath6kl_usb {
	spinlock_t cs_lock;
	spinlock_t tx_lock;
	spinlock_t rx_lock;
	struct hif_callbacks htc_callbacks;
	struct usb_device *udev;
	struct usb_interface *interface;
	struct hif_usb_pipe pipes[HIF_USB_PIPE_MAX];
	u8 surprise_removed;
	u8 *diag_cmd_buffer;
	u8 *diag_resp_buffer;
	struct ath6kl *claimed_context;
	struct notifier_block reboot_notifier;  /* default mode before reboot */

	u32 max_sche_tx;
	u32 max_sche_rx;
};

/* usb urb object */
struct hif_urb_context {
	struct list_head link;
	struct hif_usb_pipe *pipe;
	struct sk_buff *buf;
    struct sk_buff_head comp_queue;
};

/* USB endpoint definitions */
#define USB_EP_ADDR_APP_CTRL_IN          0x81
#define USB_EP_ADDR_APP_DATA_IN          0x82
#define USB_EP_ADDR_APP_DATA2_IN         0x83
#define USB_EP_ADDR_APP_INT_IN           0x84

#define USB_EP_ADDR_APP_CTRL_OUT         0x01
#define USB_EP_ADDR_APP_DATA_LP_OUT      0x02
#define USB_EP_ADDR_APP_DATA_MP_OUT      0x03
#define USB_EP_ADDR_APP_DATA_HP_OUT      0x04

/* diagnostic command defnitions */
#define USB_CONTROL_REQ_SEND_BMI_CMD        1
#define USB_CONTROL_REQ_RECV_BMI_RESP       2
#define USB_CONTROL_REQ_DIAG_CMD            3
#define USB_CONTROL_REQ_DIAG_RESP           4

#define USB_CTRL_DIAG_CC_READ               0
#define USB_CTRL_DIAG_CC_WRITE              1

/* Disable it by default */
#define HIF_USB_MAX_SCHE_PKT				(0xffffffff)

struct usb_ctrl_diag_cmd_write {
	u32 cmd;
	u32 address;
	u32 value;
	u32 _pad[1];
} __packed;

struct usb_ctrl_diag_cmd_read {
	u32 cmd;
	u32 address;
} __packed;

struct usb_ctrl_diag_resp_read {
	u32 value;
} __packed;

#define USB_CTRL_MAX_DIAG_CMD_SIZE  (sizeof(struct usb_ctrl_diag_cmd_write))
#define USB_CTRL_MAX_DIAG_RESP_SIZE (sizeof(struct usb_ctrl_diag_resp_read))

typedef struct hif_product_info_t {
    uint16_t idVendor;
    uint16_t idProduct;
    uint8_t  product[64];
    uint8_t  manufacturer[64];
    uint8_t  serial[64];
} hif_product_info_t;
#endif
