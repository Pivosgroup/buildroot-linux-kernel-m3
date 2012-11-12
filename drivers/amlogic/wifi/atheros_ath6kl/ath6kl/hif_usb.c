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
#include <linux/reboot.h>
#include "hif_usb.h"
#include "hif-ops.h"
#include "debug.h"
#include "cfg80211.h"

/* function declarations */
static void hif_usb_recv_complete(struct urb *urb);
static void hif_usb_recv_bundle_complete(struct urb *urb);
extern unsigned int htc_bundle_recv;
extern unsigned int htc_bundle_send;

#define ATH6KL_USB_IS_BULK_EP(attr) (((attr) & 3) == 0x02)
#define ATH6KL_USB_IS_INT_EP(attr)  (((attr) & 3) == 0x03)
#define ATH6KL_USB_IS_ISOC_EP(attr)  (((attr) & 3) == 0x01)
#define ATH6KL_USB_IS_DIR_IN(addr)  ((addr) & 0x80)

#define AR6004_1_0_ALIGN_WAR 1

#define REGISTER_DUMP_LEN_MAX   60
#define REG_DUMP_COUNT_AR6004   60

static void ath6kl_hif_dump_fw_crash(struct ath6kl *ar)
{
	static __le32 regdump_val[REGISTER_DUMP_LEN_MAX];
	u32 i, address, regdump_addr = 0;
	int ret;

	if (ar->target_type != TARGET_TYPE_AR6004)
		return;

	/* the reg dump pointer is copied to the host interest area */
	address = ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_failure_state));
	address = TARG_VTOP(ar->target_type, address);

	/* read RAM location through diagnostic window */
	ret = ath6kl_diag_read32(ar, address, &regdump_addr);

	if (!regdump_addr) {
		ath6kl_warn("failed to get ptr to register dump area: %d\n", 
				ret);
		return;
	}  

	regdump_addr = TARG_VTOP(ar->target_type, regdump_addr);

	BUILD_BUG_ON(REG_DUMP_COUNT_AR6004 % 4);

	/* fetch register dump data */

	ret = ath6kl_diag_read(ar, regdump_addr, (u8 *)&regdump_val[0],
				  REG_DUMP_COUNT_AR6004*sizeof(u32));

	if (ret) {
		ath6kl_warn("failed to get register dump: %d\n", ret);
		return;
	}


	ath6kl_info("firmware crash dump:\n");
	ath6kl_info("hw 0x%x fw %s\n", ar->wiphy->hw_version,
		    ar->wiphy->fw_version);

	for (i = 0; i < REG_DUMP_COUNT_AR6004 ; i+=4) { 
		ath6kl_info("%d: 0x%08x 0x%08x 0x%08x 0x%08x\n", 
				4 * i, 
				le32_to_cpu(regdump_val[i]), 
				le32_to_cpu(regdump_val[i + 1]), 
				le32_to_cpu(regdump_val[i + 2]), 
				le32_to_cpu(regdump_val[i + 3]));
	}

	/* reset target and trigger USB disconnect */
	if (ar->hif_type == HIF_TYPE_USB) {

		u32 address;
		__le32 data;

		data = RESET_CONTROL_COLD_RST|(1<< 30); //for usb controller reset
		address = RTC_BASE_ADDRESS;

		if (ath6kl_diag_write32(ar, address, data))
			ath6kl_err("failed to reset target\n");

	}
}

void firmware_crash_work(struct work_struct *work)
{
        struct ath6kl *ar= container_of(work, struct ath6kl, firmware_crash_dump_deferred_work);

        ath6kl_hif_dump_fw_crash(ar);
}

/* pipe/urb operations */
static struct hif_urb_context *hif_usb_alloc_urb_from_pipe(struct hif_usb_pipe
							   *pipe)
{
	struct hif_urb_context *urb_context = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pipe->ar_usb->cs_lock, flags);
	if (!list_empty(&pipe->urb_list_head)) {
		urb_context =
		    list_first_entry(&pipe->urb_list_head,
				     struct hif_urb_context, link);
		list_del(&urb_context->link);
		pipe->urb_cnt--;
	}
	spin_unlock_irqrestore(&pipe->ar_usb->cs_lock, flags);

	return urb_context;
}

static void hif_usb_free_urb_to_pipe(struct hif_usb_pipe *pipe,
				     struct hif_urb_context *urb_context)
{
	unsigned long flags;

	spin_lock_irqsave(&pipe->ar_usb->cs_lock, flags);
	pipe->urb_cnt++;

	list_add(&urb_context->link, &pipe->urb_list_head);
	spin_unlock_irqrestore(&pipe->ar_usb->cs_lock, flags);
}

static void hif_usb_cleanup_recv_urb(struct hif_urb_context *urb_context)
{
	if (urb_context->buf != NULL) {
		dev_kfree_skb(urb_context->buf);
		urb_context->buf = NULL;
	}

	hif_usb_free_urb_to_pipe(urb_context->pipe, urb_context);
}

static inline struct ath6kl_usb *ath6kl_usb_priv(struct ath6kl *ar)
{
	return ar->hif_priv;
}

/* pipe resource allocation/cleanup */
static int hif_usb_alloc_pipe_resources(struct hif_usb_pipe *pipe, int urb_cnt)
{
	int status = 0;
	int i;
	struct hif_urb_context *urb_context;

	INIT_LIST_HEAD(&pipe->urb_list_head);
	init_usb_anchor(&pipe->urb_submitted);

	for (i = 0; i < urb_cnt; i++) {
		urb_context = (struct hif_urb_context *)
		    kzalloc(sizeof(struct hif_urb_context), GFP_KERNEL);
		if (urb_context == NULL)
			break;

		memset(urb_context, 0, sizeof(struct hif_urb_context));
		urb_context->pipe = pipe;

		/*
		 * we are only allocate the urb contexts here, the actual URB
		 * is allocated from the kernel as needed to do a transaction
		 */
		pipe->urb_alloc++;
        
        if (htc_bundle_send) {
            /* In tx bundle mode, only pre-allocate bundle buffers for data pipes */
            if (pipe->logical_pipe_num >= HIF_TX_DATA_LP_PIPE && pipe->logical_pipe_num <= HIF_TX_DATA_HP_PIPE) {
                urb_context->buf = dev_alloc_skb(HIF_USB_TX_BUNDLE_BUFFER_SIZE);
                if (NULL == urb_context->buf) {
                    ath6kl_dbg(ATH6KL_DBG_USB,
                        "athusb: alloc send bundle buffer %d-byte failed\n",
                        HIF_USB_TX_BUNDLE_BUFFER_SIZE);
                }
            }
            skb_queue_head_init(&urb_context->comp_queue);
        }
        
		hif_usb_free_urb_to_pipe(pipe, urb_context);
	}
	ath6kl_dbg(ATH6KL_DBG_USB,
		   "ath6kl usb: alloc resources lpipe:%d"
		   "hpipe:0x%X urbs:%d\n",
		   pipe->logical_pipe_num, pipe->usb_pipe_handle,
		   pipe->urb_alloc);

	return status;
}

static void hif_usb_free_pipe_resources(struct hif_usb_pipe *pipe)
{
	struct hif_urb_context *urb_context;
    struct sk_buff *buf = NULL;

	if (pipe->ar_usb == NULL) {
		/* nothing allocated for this pipe */
		return;
	}

	ath6kl_dbg(ATH6KL_DBG_USB,
		   "ath6kl usb: free resources lpipe:%d"
		   "hpipe:0x%X urbs:%d avail:%d\n",
		   pipe->logical_pipe_num, pipe->usb_pipe_handle,
		   pipe->urb_alloc, pipe->urb_cnt);

	if (pipe->urb_alloc != pipe->urb_cnt) {
		ath6kl_dbg(ATH6KL_DBG_USB,
			   "ath6kl usb: urb leak! lpipe:%d"
			   "hpipe:0x%X urbs:%d avail:%d\n",
			   pipe->logical_pipe_num, pipe->usb_pipe_handle,
			   pipe->urb_alloc, pipe->urb_cnt);
	}

	while (true) {
		urb_context = hif_usb_alloc_urb_from_pipe(pipe);
		if (urb_context == NULL)
			break;
            
        if (htc_bundle_send) {
            while((buf = skb_dequeue(&urb_context->comp_queue)) != NULL) {
                dev_kfree_skb(buf);
            }
        }    
            
		kfree(urb_context);
	}

}

static void hif_usb_cleanup_pipe_resources(struct ath6kl_usb *device)
{
	int i;

	for (i = 0; i < HIF_USB_PIPE_MAX; i++)
		hif_usb_free_pipe_resources(&device->pipes[i]);

}

static u8 hif_usb_get_logical_pipe_num(struct ath6kl_usb *device,
				       u8 ep_address, int *urb_count)
{
	u8 pipe_num = HIF_USB_PIPE_INVALID;

	switch (ep_address) {
	case USB_EP_ADDR_APP_CTRL_IN:
		pipe_num = HIF_RX_CTRL_PIPE;
		*urb_count = RX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA_IN:
		pipe_num = HIF_RX_DATA_PIPE;
		*urb_count = RX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_INT_IN:
		pipe_num = HIF_RX_INT_PIPE;
		*urb_count = RX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA2_IN:
		pipe_num = HIF_RX_DATA2_PIPE;
		*urb_count = RX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_CTRL_OUT:
		pipe_num = HIF_TX_CTRL_PIPE;
		*urb_count = TX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA_LP_OUT:
		pipe_num = HIF_TX_DATA_LP_PIPE;
		*urb_count = TX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA_MP_OUT:
		pipe_num = HIF_TX_DATA_MP_PIPE;
		*urb_count = TX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA_HP_OUT:
		pipe_num = HIF_TX_DATA_HP_PIPE;
		*urb_count = TX_URB_COUNT;
		break;
	default:
		/* note: there may be endpoints not currently used */
		break;
	}

	return pipe_num;
}

static int hif_usb_setup_pipe_resources(struct ath6kl_usb *device)
{
	struct usb_interface *interface = device->interface;
	struct usb_host_interface *iface_desc = interface->cur_altsetting;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	int urbcount;
	int status = 0;
	struct hif_usb_pipe *pipe;
	u8 pipe_num;
	ath6kl_dbg(ATH6KL_DBG_USB, "setting up USB Pipes using interface\n");
	/* walk decriptors and setup pipes */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (ATH6KL_USB_IS_BULK_EP(endpoint->bmAttributes)) {
			ath6kl_dbg(ATH6KL_DBG_USB,
				   "%s Bulk Ep:0x%2.2X maxpktsz:%d\n",
				   ATH6KL_USB_IS_DIR_IN
				   (endpoint->bEndpointAddress) ?
				   "RX" : "TX", endpoint->bEndpointAddress,
				   le16_to_cpu(endpoint->wMaxPacketSize));
		} else if (ATH6KL_USB_IS_INT_EP(endpoint->bmAttributes)) {
			ath6kl_dbg(ATH6KL_DBG_USB,
				   "%s Int Ep:0x%2.2X maxpktsz:%d interval:%d\n",
				   ATH6KL_USB_IS_DIR_IN
				   (endpoint->bEndpointAddress) ?
				   "RX" : "TX", endpoint->bEndpointAddress,
				   le16_to_cpu(endpoint->wMaxPacketSize),
				   endpoint->bInterval);
		} else if (ATH6KL_USB_IS_ISOC_EP(endpoint->bmAttributes)) {
			/* TODO for ISO */
			ath6kl_dbg(ATH6KL_DBG_USB,
				   "%s ISOC Ep:0x%2.2X maxpktsz:%d interval:%d\n",
				   ATH6KL_USB_IS_DIR_IN
				   (endpoint->bEndpointAddress) ?
				   "RX" : "TX", endpoint->bEndpointAddress,
				   le16_to_cpu(endpoint->wMaxPacketSize),
				   endpoint->bInterval);
		}
		urbcount = 0;

		pipe_num =
		    hif_usb_get_logical_pipe_num(device,
						 endpoint->bEndpointAddress,
						 &urbcount);
		if (pipe_num == HIF_USB_PIPE_INVALID)
			continue;

		pipe = &device->pipes[pipe_num];
		if (pipe->ar_usb != NULL) {
			/* hmmm..pipe was already setup */
			continue;
		}

		pipe->ar_usb = device;
		pipe->logical_pipe_num = pipe_num;
		pipe->ep_address = endpoint->bEndpointAddress;
		pipe->max_packet_size = le16_to_cpu(endpoint->wMaxPacketSize);

		if (ATH6KL_USB_IS_BULK_EP(endpoint->bmAttributes)) {
			if (ATH6KL_USB_IS_DIR_IN(pipe->ep_address)) {
				pipe->usb_pipe_handle =
				    usb_rcvbulkpipe(device->udev,
						    pipe->ep_address);
			} else {
				pipe->usb_pipe_handle =
				    usb_sndbulkpipe(device->udev,
						    pipe->ep_address);
			}
		} else if (ATH6KL_USB_IS_INT_EP(endpoint->bmAttributes)) {
			if (ATH6KL_USB_IS_DIR_IN(pipe->ep_address)) {
				pipe->usb_pipe_handle =
				    usb_rcvintpipe(device->udev,
						   pipe->ep_address);
			} else {
				pipe->usb_pipe_handle =
				    usb_sndintpipe(device->udev,
						   pipe->ep_address);
			}
		} else if (ATH6KL_USB_IS_ISOC_EP(endpoint->bmAttributes)) {
			/* TODO for ISO */
			if (ATH6KL_USB_IS_DIR_IN(pipe->ep_address)) {
				pipe->usb_pipe_handle =
				    usb_rcvisocpipe(device->udev,
						    pipe->ep_address);
			} else {
				pipe->usb_pipe_handle =
				    usb_sndisocpipe(device->udev,
						    pipe->ep_address);
			}
		}
		pipe->ep_desc = endpoint;

		if (!ATH6KL_USB_IS_DIR_IN(pipe->ep_address))
			pipe->flags |= HIF_USB_PIPE_FLAG_TX;

		status = hif_usb_alloc_pipe_resources(pipe, urbcount);
		if (status != 0)
			break;

	}

	return status;
}

/* pipe operations */
static void hif_usb_post_recv_transfers(struct hif_usb_pipe *recv_pipe,
					int buffer_length)
{
	struct hif_urb_context *urb_context;
	u8 *data;
	u32 len;
	struct urb *urb;
	int usb_status;

	while (1) {

		urb_context = hif_usb_alloc_urb_from_pipe(recv_pipe);
		if (urb_context == NULL)
			break;

		urb_context->buf = dev_alloc_skb(buffer_length);
		if (urb_context->buf == NULL) {
			goto err_cleanup_urb;
		}
		
		//data=(char *)kmalloc(urb_context->buf->len,GFP_ATOMIC);
	    //memset(data,0,urb_context->buf->len);
	    //memcpy(data,urb_context->buf->data,urb_context->buf->len);
		data = urb_context->buf->data;
		len = urb_context->buf->len;

		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (urb == NULL) {
			goto err_cleanup_urb;
		}

		usb_fill_bulk_urb(urb,
				  recv_pipe->ar_usb->udev,
				  recv_pipe->usb_pipe_handle,
				  data,
				  buffer_length,
				  hif_usb_recv_complete, urb_context);

		ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			   "ath6kl usb: bulk recv submit:%d, 0x%X"
			   "(ep:0x%2.2X), %d bytes buf:0x%p\n",
			   recv_pipe->logical_pipe_num,
			   recv_pipe->usb_pipe_handle, recv_pipe->ep_address,
			   buffer_length, urb_context->buf);

		usb_anchor_urb(urb, &recv_pipe->urb_submitted);
		usb_status = usb_submit_urb(urb, GFP_ATOMIC);

		if (usb_status) {
			ath6kl_dbg(ATH6KL_DBG_USB_BULK,
				   "ath6kl usb : usb bulk recv failed %d\n",
				   usb_status);
			usb_unanchor_urb(urb);
			usb_free_urb(urb);
			goto err_cleanup_urb;
		}
		usb_free_urb(urb);
	}
	return;

err_cleanup_urb:
	hif_usb_cleanup_recv_urb(urb_context);
	return;
}

static void hif_usb_post_recv_bundle_transfers(struct hif_usb_pipe *recv_pipe,
                    int buffer_length)
{
    struct hif_urb_context *urb_context;
	u8 *data;
	u32 len;
	struct urb *urb;
	int usb_status;
    
    while (1) {
        
        urb_context = hif_usb_alloc_urb_from_pipe(recv_pipe);    
        if (urb_context == NULL)
            break;
        
        if (buffer_length) {
            urb_context->buf = dev_alloc_skb(buffer_length);
            if (urb_context->buf == NULL) {
                hif_usb_cleanup_recv_urb(urb_context);
                break;
            }
        }

        data = urb_context->buf->data;
		len = urb_context->buf->len;
        
        urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (urb == NULL) {
			hif_usb_cleanup_recv_urb(urb_context);
            break;
		}

        usb_fill_bulk_urb(urb, 
                          recv_pipe->ar_usb->udev,
                          recv_pipe->usb_pipe_handle,
                          data, 
                          HIF_USB_RX_BUNDLE_BUFFER_SIZE,
                          hif_usb_recv_bundle_complete, urb_context);

		ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			   "ath6kl usb: bulk recv submit:%d, 0x%X"
			   "(ep:0x%2.2X), %d bytes buf:0x%p\n",
			   recv_pipe->logical_pipe_num,
			   recv_pipe->usb_pipe_handle, recv_pipe->ep_address,
			   buffer_length, urb_context->buf);
        
        usb_anchor_urb(urb, &recv_pipe->urb_submitted);
        usb_status = usb_submit_urb(urb,GFP_ATOMIC);
        
        if (usb_status) {
            ath6kl_dbg(ATH6KL_DBG_USB_BULK,
				   "ath6kl usb : usb bulk recv failed %d\n",
				   usb_status);
            usb_unanchor_urb(urb);
            usb_free_urb(urb);
            hif_usb_free_urb_to_pipe(urb_context->pipe, urb_context);
            break;    
        }
        usb_free_urb(urb);
    }
    return;
}

static void hif_usb_flush_all(struct ath6kl_usb *device)
{
	int i;

	for (i = 0; i < HIF_USB_PIPE_MAX; i++) {
		if (device->pipes[i].ar_usb != NULL)
			usb_kill_anchored_urbs(&device->pipes[i].urb_submitted);
	}

	/* flushing any pending I/O may schedule work
	 * this call will block until all scheduled work runs to completion */
	flush_scheduled_work();
}

static void hif_usb_start_recv_pipes(struct ath6kl_usb *device)
{
	/*
	 * note: control pipe is no longer used
	 * device->pipes[HIF_RX_CTRL_PIPE].urb_cnt_thresh =
	 *      device->pipes[HIF_RX_CTRL_PIPE].urb_alloc/2;
	 * hif_usb_post_recv_transfers(&device->pipes[HIF_RX_CTRL_PIPE],
	 *       HIF_USB_RX_BUFFER_SIZE);
	 */

	device->pipes[HIF_RX_DATA_PIPE].urb_cnt_thresh =
	    device->pipes[HIF_RX_DATA_PIPE].urb_alloc / 2;
        
    if (!htc_bundle_recv) {
	hif_usb_post_recv_transfers(&device->pipes[HIF_RX_DATA_PIPE],
				    HIF_USB_RX_BUFFER_SIZE);
    } else {
        hif_usb_post_recv_bundle_transfers(&device->pipes[HIF_RX_DATA_PIPE], 
                        HIF_USB_RX_BUNDLE_BUFFER_SIZE);
    }
	/*
	* Disable rxdata2 directly, it will be enabled
	* if FW enable rxdata2
	*/
	if (0) {
		device->pipes[HIF_RX_DATA2_PIPE].urb_cnt_thresh =
		    device->pipes[HIF_RX_DATA2_PIPE].urb_alloc / 2;
		hif_usb_post_recv_transfers(&device->pipes[HIF_RX_DATA2_PIPE],
					    HIF_USB_RX_BUFFER_SIZE);
	}
}

/* hif usb rx/tx completion functions */
static void hif_usb_recv_complete(struct urb *urb)
{
	struct hif_urb_context *urb_context =
	    (struct hif_urb_context *)urb->context;
	int status = 0;
	struct sk_buff *buf = NULL;
	struct hif_usb_pipe *pipe = urb_context->pipe;
	struct hif_usb_pipe_stat *pipe_st = &pipe->usb_pipe_stat;

	ath6kl_dbg(ATH6KL_DBG_USB_BULK,
		   "%s: recv pipe: %d, stat:%d, len:%d urb:0x%p\n", __func__,
		   pipe->logical_pipe_num, urb->status, urb->actual_length,
		   urb);

	if (urb->status != 0) {
		pipe_st->num_rx_comp_err++;
		status = -EIO;
		switch (urb->status) {
		case -EOVERFLOW:
			urb->actual_length = HIF_USB_RX_BUFFER_SIZE;
			status = 0;
			break;
		case -ECONNRESET:
		case -ENOENT:
		case -ESHUTDOWN:
			/*
			 * no need to spew these errors when device
			 * removed or urb killed due to driver shutdown
			 */
			status = -ECANCELED;
			break;
		default:
			ath6kl_dbg(ATH6KL_DBG_USB_BULK,
				   "%s recv pipe: %d (ep:0x%2.2X), failed:%d\n",
				   __func__, pipe->logical_pipe_num,
				   pipe->ep_address, urb->status);
			break;
		}
		goto cleanup_recv_urb;
	}
	if (urb->actual_length == 0)
		goto cleanup_recv_urb;

	buf = urb_context->buf;
	
	/*mcli if(buf->data[18]==0xd0 && buf->data[19]==0)
	{
			//printk(KERN_ERR "mcli: Receive P2P device connect request!!!\n");
		printk(KERN_ERR "buf->data[18-81]:\n");
		printk(KERN_ERR "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				buf->data[18],buf->data[19],buf->data[20],buf->data[21],buf->data[22],buf->data[23],buf->data[24],buf->data[25],
				buf->data[26],buf->data[27],buf->data[28],buf->data[29],buf->data[30],buf->data[31],buf->data[32],buf->data[33]);
		printk(KERN_ERR "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				buf->data[34],buf->data[35],buf->data[36],buf->data[37],buf->data[38],buf->data[39],buf->data[40],buf->data[41],
				buf->data[42],buf->data[43],buf->data[44],buf->data[45],buf->data[46],buf->data[47],buf->data[48],buf->data[49]);
		printk(KERN_ERR "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				buf->data[50],buf->data[51],buf->data[52],buf->data[53],buf->data[54],buf->data[55],buf->data[56],buf->data[57],
				buf->data[58],buf->data[59],buf->data[60],buf->data[61],buf->data[62],buf->data[63],buf->data[64],buf->data[65]);
		printk(KERN_ERR "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				buf->data[66],buf->data[67],buf->data[68],buf->data[69],buf->data[70],buf->data[71],buf->data[72],buf->data[73],
				buf->data[74],buf->data[75],buf->data[76],buf->data[77],buf->data[78],buf->data[79],buf->data[80],buf->data[81]);
	}*/		  	
	
	/* we are going to pass it up */
	urb_context->buf = NULL;
	skb_put(buf, urb->actual_length);
	/* note: queue implements a lock */
	skb_queue_tail(&pipe->io_comp_queue, buf);
	pipe_st->num_rx_comp++;
	schedule_work(&pipe->io_complete_work);

cleanup_recv_urb:
	hif_usb_cleanup_recv_urb(urb_context);

	if (status == 0) {
		if (pipe->urb_cnt >= pipe->urb_cnt_thresh) {
			/* our free urbs are piling up, post more transfers */
			hif_usb_post_recv_transfers(pipe,
						    HIF_USB_RX_BUFFER_SIZE);
		}
	}
	return;
}

static void hif_usb_recv_bundle_complete(struct urb *urb)
{
    struct hif_urb_context *urb_context =
	    (struct hif_urb_context *)urb->context;
	int status = 0;
	struct sk_buff *buf = NULL;
	struct hif_usb_pipe *pipe = urb_context->pipe;
	struct hif_usb_pipe_stat *pipe_st = &pipe->usb_pipe_stat;

    u8 *netdata, *netdata_new;
    u32 netlen, netlen_new;
    struct htc_frame_hdr *htc_hdr;
    u16 payload_len;
    struct sk_buff *new_skb = NULL;

    
    ath6kl_dbg(ATH6KL_DBG_USB_BULK,
           "%s: recv pipe: %d, stat:%d, len:%d urb:0x%p\n", __func__,
           pipe->logical_pipe_num, urb->status, urb->actual_length,
           urb);
         
    do {
        
        if (urb->status != 0) {     
			pipe_st->num_rx_bundle_comp_err++;
            status = -EIO;
            switch (urb->status) {
                case -EOVERFLOW:
			        urb->actual_length = HIF_USB_RX_BUFFER_SIZE;
			        status = 0;
			        break;
                case -ECONNRESET:
                case -ENOENT:
                case -ESHUTDOWN:    
                    /*
			         * no need to spew these errors when device
			         * removed or urb killed due to driver shutdown
			         */
                    status = -ECANCELED; 
                    break;
                default :
                    ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                           "%s recv pipe: %d (ep:0x%2.2X), failed:%d\n",
                           __func__, pipe->logical_pipe_num,
                           pipe->ep_address, urb->status);            
            }      
            break;
        }
        
        if (urb->actual_length == 0) {
            break;
        }
        
        buf = urb_context->buf;

        netdata = buf->data;
        netlen = buf->len;
        netlen = urb->actual_length;
        
        do {
#if defined(AR6004_1_0_ALIGN_WAR)
            u8 extra_pad = 0;
            u16 act_frame_len = 0;
#endif
            u16 frame_len;
            
            /* Hack into HTC header for bundle processing */
            htc_hdr = (struct htc_frame_hdr *)netdata;
            if (htc_hdr->eid >= ENDPOINT_MAX) {
                ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                       "athusb: Rx: invalid eid=%d\n", htc_hdr->eid);
                break;
            }

            payload_len = le16_to_cpu(get_unaligned((u16 *)&htc_hdr->payld_len));

#if defined(AR6004_1_0_ALIGN_WAR)
            act_frame_len = (HTC_HDR_LENGTH + payload_len);

            if (htc_hdr->eid == 0 || htc_hdr->eid == 1) {
            	 /* assumption: target won't pad on HTC endpoint 0 & 1. */
                extra_pad = 0;
            } else {
                extra_pad = get_unaligned((u8 *)&htc_hdr->ctrl[1]);
            }
#endif

            if (payload_len > HIF_USB_RX_BUFFER_SIZE) {
                ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                       "athusb: payload_len too long %u\n", payload_len);
                break;
            }

#if defined(AR6004_1_0_ALIGN_WAR)
            frame_len = (act_frame_len + extra_pad);
#else
            frame_len = (HTC_HDR_LENGTH + payload_len);
#endif

            if (netlen >= frame_len) {
                /* allocate a new skb and copy */
#if defined(AR6004_1_0_ALIGN_WAR)
                new_skb = dev_alloc_skb(act_frame_len);
                if (new_skb == NULL) {
                    ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                           "athusb: allocate skb (len=%u) failed\n", act_frame_len);
                    break;
                }

                netdata_new = new_skb->data;
                netlen_new = new_skb->len;
                memcpy(netdata_new, netdata, act_frame_len);
                skb_put(new_skb, act_frame_len);
#else
                new_skb = dev_alloc_skb(frame_len);
                if (new_skb == NULL) {
                    ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                           "athusb: allocate skb (len=%u) failed\n", frame_len);
                    break;
                }

                netdata_new = new_skb->data;
                netlen_new = new_skb->len;
                memcpy(netdata_new, netdata, frame_len);
                skb_put(new_skb, frame_len);
#endif
                skb_queue_tail(&pipe->io_comp_queue, new_skb);
                new_skb = NULL;
                
                netdata += frame_len;
                netlen -= frame_len;
            }
            else {
                ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                       "athusb: subframe length %d not fitted into bundle packet length %d\n", 
                    netlen, frame_len);
                break;
            }
            
        } while (netlen);

		pipe_st->num_rx_bundle_comp++;
        schedule_work(&pipe->io_complete_work);  

    } while (0);
        
    if (urb_context->buf == NULL) {
        ath6kl_dbg(ATH6KL_DBG_USB_BULK,
               "athusb: buffer in urb_context is NULL\n");
    }

    hif_usb_free_urb_to_pipe(urb_context->pipe, urb_context);

    if (status == 0) {
        if (pipe->urb_cnt >= pipe->urb_cnt_thresh) {
                /* our free urbs are piling up, post more transfers */
            hif_usb_post_recv_bundle_transfers(pipe,
                         0 /* pass zero for not allocating urb-buffer again */); 
        }
    }
    return;
}

static void hif_usb_usb_transmit_complete(struct urb *urb)
{
	struct hif_urb_context *urb_context =
	    (struct hif_urb_context *)urb->context;
	struct sk_buff *buf;
	struct hif_usb_pipe *pipe = urb_context->pipe;
	struct hif_usb_pipe_stat *pipe_st = &pipe->usb_pipe_stat;

	ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			"%s: pipe: %d, stat:%d, len:%d\n",
			__func__, pipe->logical_pipe_num, urb->status,
			urb->actual_length);

	if (urb->status != 0) {
			pipe_st->num_tx_comp_err++;
		ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			"%s:  pipe: %d, failed:%d\n",
			__func__, pipe->logical_pipe_num, urb->status);
	}

	buf = urb_context->buf;
	urb_context->buf = NULL;
	hif_usb_free_urb_to_pipe(urb_context->pipe, urb_context);

	/* note: queue implements a lock */
	skb_queue_tail(&pipe->io_comp_queue, buf);
	pipe_st->num_tx_comp++;
	schedule_work(&pipe->io_complete_work);
}

static void hif_usb_io_comp_work(struct work_struct *work)
{
	struct hif_usb_pipe *pipe =
	    container_of(work, struct hif_usb_pipe, io_complete_work);
	struct sk_buff *buf;
	struct ath6kl_usb *device;
	struct hif_usb_pipe_stat *pipe_st = &pipe->usb_pipe_stat;
	u32 tx = 0, rx = 0;

	pipe_st->num_io_comp++;
	device = pipe->ar_usb;
	while ((buf = skb_dequeue(&pipe->io_comp_queue))) {
		if (pipe->flags & HIF_USB_PIPE_FLAG_TX) {
			ath6kl_dbg(ATH6KL_DBG_USB_BULK,
				   "ath6kl usb xmit callback buf:0x%p\n", buf);
			device->htc_callbacks.
				tx_completion(device->claimed_context->
					htc_target, buf);

			if (tx++ > device->max_sche_tx) {
				pipe_st->num_tx_resche++;
				goto reschedule;
			}
		} else {
			ath6kl_dbg(ATH6KL_DBG_USB_BULK,
				   "ath6kl usb recv callback buf:0x%p\n", buf);
			device->htc_callbacks.
				rx_completion(device->claimed_context->
					htc_target, buf,
					pipe->logical_pipe_num);

			if (rx++ > device->max_sche_rx) { 
				pipe_st->num_rx_resche++;
				goto reschedule;
			}
		}
	}

	if (tx > pipe_st->num_max_tx)
		pipe_st->num_max_tx = tx;

	if (rx > pipe_st->num_max_rx)
		pipe_st->num_max_rx = rx;

	return;

reschedule:
	/* Re-schedule it to avoid one direction to starve another direction. */
	ath6kl_dbg(ATH6KL_DBG_USB_BULK,
				   "ath6kl usb re-schedule work.\n");
	schedule_work(&pipe->io_complete_work);

	return;
}

static void hif_usb_destroy(struct ath6kl_usb *device)
{
	unregister_reboot_notifier(&device->reboot_notifier);

	hif_usb_flush_all(device);

	hif_usb_cleanup_pipe_resources(device);

	usb_set_intfdata(device->interface, NULL);

	kfree(device->diag_cmd_buffer);
	kfree(device->diag_resp_buffer);

	kfree(device);
}

extern void ath6kl_reset_device(struct ath6kl *ar, u32 target_type,
				bool wait_fot_compltn, bool cold_reset);

static int ath6kl_usb_reboot(struct notifier_block *nb, unsigned long val,
			       void *v)
{
	struct ath6kl_usb *device;
	struct ath6kl *ar ;

	device= container_of(nb, struct ath6kl_usb, reboot_notifier);
	if (device == NULL)
		return NOTIFY_DONE;

	ar = (struct ath6kl *) device->claimed_context;
	if (ar != NULL) {
		ath6kl_reset_device(ar, ar->target_type, true, true);
	}
	return NOTIFY_DONE;
}

static struct ath6kl_usb *hif_usb_create(struct usb_interface *interface)
{
	struct ath6kl_usb *device = NULL;
	struct usb_device *dev = interface_to_usbdev(interface);
	int status = 0;
	int i;
	struct hif_usb_pipe *pipe;

	device = (struct ath6kl_usb *)
	    kzalloc(sizeof(struct ath6kl_usb), GFP_KERNEL);
	if (device == NULL)
		goto fail_hif_usb_create;

	memset(device, 0, sizeof(struct ath6kl_usb));
	usb_set_intfdata(interface, device);
	spin_lock_init(&(device->cs_lock));
	spin_lock_init(&(device->rx_lock));
	spin_lock_init(&(device->tx_lock));
	device->udev = dev;
	device->interface = interface;

	for (i = 0; i < HIF_USB_PIPE_MAX; i++) {
		pipe = &device->pipes[i];
		INIT_WORK(&pipe->io_complete_work,
			  hif_usb_io_comp_work);
		skb_queue_head_init(&pipe->io_comp_queue);
	}

	device->diag_cmd_buffer =
			kzalloc(USB_CTRL_MAX_DIAG_CMD_SIZE, GFP_KERNEL);
	if (device->diag_cmd_buffer == NULL) {
		status = -ENOMEM;
		goto fail_hif_usb_create;
	}
	device->diag_resp_buffer =
		kzalloc(USB_CTRL_MAX_DIAG_RESP_SIZE, GFP_KERNEL);
	if (device->diag_resp_buffer == NULL) {
		status = -ENOMEM;
		goto fail_hif_usb_create;
	}

	device->max_sche_tx =
	device->max_sche_rx = HIF_USB_MAX_SCHE_PKT;

	status = hif_usb_setup_pipe_resources(device);

	device->reboot_notifier.notifier_call= ath6kl_usb_reboot;
	register_reboot_notifier(&device->reboot_notifier);
	
fail_hif_usb_create:
	if (status != 0) {
		hif_usb_destroy(device);
		device = NULL;
	}
	return device;
}

static void hif_usb_suspend(struct usb_interface *interface)
{
	struct ath6kl_usb *device;
	struct ath6kl *ar;

	device = (struct ath6kl_usb *)usb_get_intfdata(interface);
	ar = (struct ath6kl *) device->claimed_context;

	if (!test_bit(WLAN_WOW_ENABLE, &ar->vif[0]->flag)) {
		ath6kl_deep_sleep_enable(ar->vif[0]);
	}
	hif_usb_flush_all(device);
}

static void hif_usb_resume(struct usb_interface *interface)
{
	struct ath6kl_usb *device;
	device = (struct ath6kl_usb *)usb_get_intfdata(interface);
	/* re-post urbs? */
	if (0) {
		hif_usb_post_recv_transfers(&device->pipes[HIF_RX_CTRL_PIPE],
					    HIF_USB_RX_BUFFER_SIZE);
	}
    if (!htc_bundle_recv) {
	hif_usb_post_recv_transfers(&device->pipes[HIF_RX_DATA_PIPE],
				    HIF_USB_RX_BUFFER_SIZE);
	hif_usb_post_recv_transfers(&device->pipes[HIF_RX_DATA2_PIPE],
				    HIF_USB_RX_BUFFER_SIZE);
    } else {
        hif_usb_post_recv_bundle_transfers(&device->pipes[HIF_RX_DATA_PIPE],
                        0 /* not allocating urb-buffer again */); 
        hif_usb_post_recv_bundle_transfers(&device->pipes[HIF_RX_DATA2_PIPE],
                        0 /* not allocating urb-buffer again */); 
    }
}

static void hif_usb_device_detached(struct usb_interface *interface,
				    u8 surprise_removed)
{
	struct ath6kl_usb *device;

	device = (struct ath6kl_usb *)usb_get_intfdata(interface);
	if (device == NULL)
		return;

	ath6kl_stop_txrx(device->claimed_context);

	device->surprise_removed = surprise_removed;

	/* inform upper layer if it is still interested */
	if (surprise_removed && device->claimed_context != NULL)
		ath6kl_unavail_ev(device->claimed_context);

	hif_usb_destroy(device);
}

/* exported hif usb APIs for htc pipe */
void hif_start(struct ath6kl *ar)
{
	struct ath6kl_usb *device = ath6kl_usb_priv(ar);
	int i;
	hif_usb_start_recv_pipes(device);

	/* set the TX resource avail threshold for each TX pipe */
	for (i = HIF_TX_CTRL_PIPE; i <= HIF_TX_DATA_HP_PIPE; i++) {
		device->pipes[i].urb_cnt_thresh =
		    device->pipes[i].urb_alloc / 2;
	}
}

static void hif_usb_transmit_bundle_complete(struct urb *urb)
{
    struct hif_urb_context *urb_context =
	    (struct hif_urb_context *)urb->context;
	struct hif_usb_pipe *pipe = urb_context->pipe;
	struct hif_usb_pipe_stat *pipe_st = &pipe->usb_pipe_stat;
    struct sk_buff *tmp_buf;


    ath6kl_dbg(ATH6KL_DBG_USB_BULK,
            "+%s: pipe: %d, stat:%d, len:%d \n",
            __func__, pipe->logical_pipe_num, urb->status,
			urb->actual_length);

    if (urb->status != 0) {
		pipe_st->num_tx_bundle_comp_err++;
        ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			"%s:  pipe: %d, failed:%d\n",
			__func__, pipe->logical_pipe_num, urb->status);
    }
    
    //a_mem_trace(urb_context->buf);        

    hif_usb_free_urb_to_pipe(urb_context->pipe, urb_context);

    while ((tmp_buf = skb_dequeue(&urb_context->comp_queue))) {
        skb_queue_tail(&pipe->io_comp_queue, tmp_buf);
    }
	pipe_st->num_tx_bundle_comp++;
    schedule_work(&pipe->io_complete_work);
}

int hif_send_multiple(struct ath6kl *ar, u8 PipeID, struct sk_buff **msg_bundle, 
        int num_msgs)
{
    int status = 0;
    struct ath6kl_usb *device = ath6kl_usb_priv(ar);
	struct hif_usb_pipe *pipe = &device->pipes[PipeID];
	struct hif_usb_pipe_stat *pipe_st = &pipe->usb_pipe_stat;
	struct hif_urb_context *urb_context;
    struct urb *urb;
    struct sk_buff *stream_buf = NULL, *buf = NULL;
    int usb_status;
    int i;

	pipe_st->num_tx_multi++;
	ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			"+%s pipe : %d\n",
			__func__, PipeID);
    
    do {
        u8 *stream_netdata, *netdata, *stream_netdata_start;
        u32 stream_netlen, netlen;
        
        urb_context = hif_usb_alloc_urb_from_pipe(pipe);
        
        if (urb_context == NULL) {
			pipe_st->num_tx_multi_err_others++;
            /*
             * TODO: it is possible to run out of urbs if
             * 2 endpoints map to the same pipe ID
             */
            ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                   "%s pipe:%d no urbs left. URB Cnt : %d\n",
                   __func__, PipeID, pipe->urb_cnt);
            status = -ENOMEM;
            break;
        }
        
        urb = usb_alloc_urb(0, GFP_ATOMIC);
        if (urb == NULL) {
			pipe_st->num_tx_multi_err_others++;
            status = -ENOMEM;
            hif_usb_free_urb_to_pipe(urb_context->pipe,
                urb_context);
            break;
        }

        stream_buf = urb_context->buf;

        stream_netdata = stream_buf->data;
        stream_netlen = stream_buf->len;
        
        stream_netlen = 0;
        stream_netdata_start = stream_netdata;

        for (i = 0; i < num_msgs; i ++) {
            buf = msg_bundle[i];
            netdata = buf->data;
            netlen = buf->len;

            memcpy(stream_netdata, netdata, netlen);

            /* add additional dummy padding */
            stream_netdata += 1664; /* target credit size */
            stream_netlen += 1664;

            /* note: queue implements a lock */
            skb_queue_tail(&urb_context->comp_queue, buf);
        }

        usb_fill_bulk_urb(urb, 
                          device->udev,
                          pipe->usb_pipe_handle,
                          stream_netdata_start, 
                          stream_netlen, 
                          hif_usb_transmit_bundle_complete,
                          urb_context); 
        
        if ((stream_netlen % pipe->max_packet_size) == 0) {
                /* hit a max packet boundary on this pipe */
            urb->transfer_flags |= URB_ZERO_PACKET;
        }

        ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                "athusb bulk send submit:%d, 0x%X (ep:0x%2.2X), %d bytes\n",
                pipe->logical_pipe_num, pipe->usb_pipe_handle, 
                pipe->ep_address, stream_netlen);

        usb_anchor_urb(urb, &pipe->urb_submitted);
        usb_status = usb_submit_urb(urb, GFP_ATOMIC);        
        if (usb_status) {
			pipe_st->num_tx_multi_err++;
            ath6kl_dbg(ATH6KL_DBG_USB_BULK,
                    "ath6kl usb : usb bulk transmit failed %d \n",
                    usb_status);
            usb_unanchor_urb(urb);
            usb_free_urb(urb);
            hif_usb_free_urb_to_pipe(urb_context->pipe,
					 urb_context);
            status = -EINVAL;
            break;    
        }
        usb_free_urb(urb);
    } while (0);

    return status;
}

int hif_send_sync(struct ath6kl *ar, u8 PipeID, struct sk_buff *buf,
	int timeout)
{
	int retry = 32;
	int ret = 0, actual_len;
	struct ath6kl_usb *device = ath6kl_usb_priv(ar);
	struct hif_usb_pipe *pipe = &device->pipes[PipeID];

	pipe->usb_pipe_stat.num_tx_sync++;
	while(retry--) {
		ret = usb_bulk_msg(device->udev, pipe->usb_pipe_handle,
			buf->data, buf->len, &actual_len, timeout);
		if (ret == 0)
			break;
	}

	return ret;
}

int hif_send(struct ath6kl *ar, u8 PipeID, struct sk_buff *hdr_buf,
	     struct sk_buff *buf)
{
	int status = 0;
	struct ath6kl_usb *device = ath6kl_usb_priv(ar);
	struct hif_usb_pipe *pipe = &device->pipes[PipeID];
	struct hif_usb_pipe_stat *pipe_st = &pipe->usb_pipe_stat;
	struct hif_urb_context *urb_context;
	u8 *data;
	u32 len;
	struct urb *urb;
	int usb_status;

    data=(char *)kmalloc(buf->len,GFP_ATOMIC);
	memset(data,0,buf->len);
	memcpy(data,buf->data,buf->len);
	ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			"+%s pipe : %d, buf:0x%p\n",
			__func__, PipeID, buf);

	urb_context = hif_usb_alloc_urb_from_pipe(pipe);

	if (urb_context == NULL) {
		pipe_st->num_tx_err_others++;
		/*
		 * TODO: it is possible to run out of urbs if
		 * 2 endpoints map to the same pipe ID
		 */
		ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			   "%s pipe:%d no urbs left. URB Cnt : %d\n",
			   __func__, PipeID, pipe->urb_cnt);
		status = -ENOMEM;
		goto fail_hif_send;
	}
	urb_context->buf = buf;

	//data = buf->data;
	len = buf->len;
	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (urb == NULL) {
		pipe_st->num_tx_err_others++;
		status = -ENOMEM;
		hif_usb_free_urb_to_pipe(urb_context->pipe,
			urb_context);
		goto fail_hif_send;
	}
	
	usb_fill_bulk_urb(urb,
			  device->udev,
			  pipe->usb_pipe_handle,
			  data,
			  len,
			  hif_usb_usb_transmit_complete, urb_context);

	if ((len % pipe->max_packet_size) == 0) {
		/* hit a max packet boundary on this pipe */
		urb->transfer_flags |= URB_ZERO_PACKET;
	}

	ath6kl_dbg(ATH6KL_DBG_USB_BULK,
		   "athusb bulk send submit:%d, 0x%X (ep:0x%2.2X), %d bytes\n",
		   pipe->logical_pipe_num, pipe->usb_pipe_handle,
		   pipe->ep_address, len);

	usb_anchor_urb(urb, &pipe->urb_submitted);
	usb_status = usb_submit_urb(urb, GFP_ATOMIC);

	if (usb_status) {
		pipe_st->num_tx_err++;
		ath6kl_dbg(ATH6KL_DBG_USB_BULK,
			   "ath6kl usb : usb bulk transmit failed %d\n",
			   usb_status);
		usb_unanchor_urb(urb);
		hif_usb_free_urb_to_pipe(urb_context->pipe,
					 urb_context);
		status = -EINVAL;
	}
	usb_free_urb(urb);
	pipe_st->num_tx++;

fail_hif_send:
	return status;
}

void hif_stop(struct ath6kl *ar)
{
	struct ath6kl_usb *device = ath6kl_usb_priv(ar);

	hif_usb_flush_all(device);
}

void hif_get_default_pipe(struct ath6kl *ar, u8 *ULPipe, u8 *DLPipe)
{
	*ULPipe = HIF_TX_CTRL_PIPE;
	*DLPipe = HIF_RX_CTRL_PIPE;
}

int hif_map_service_pipe(struct ath6kl *ar, u16 ServiceId, u8 *ULPipe,
			 u8 *DLPipe)
{
	int status = 0;

	switch (ServiceId) {
	case HTC_CTRL_RSVD_SVC:
	case WMI_CONTROL_SVC:
		*ULPipe = HIF_TX_CTRL_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	case WMI_DATA_BE_SVC:
	case WMI_DATA_BK_SVC:
		*ULPipe = HIF_TX_DATA_LP_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	case WMI_DATA_VI_SVC:
		*ULPipe = HIF_TX_DATA_LP_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	case WMI_DATA_VO_SVC:
		*ULPipe = HIF_TX_DATA_LP_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	default:
		status = -EPERM;
		break;
	}

	return status;
}

void hif_postinit(struct ath6kl *ar,
		void *unused,
		struct hif_callbacks *callbacks)
{
	struct ath6kl_usb *device = ath6kl_usb_priv(ar);

	memcpy(&device->htc_callbacks, callbacks,
	       sizeof(struct hif_callbacks));
}

u16 hif_get_free_queue_number(struct ath6kl *ar, u8 PipeID)
{
	struct ath6kl_usb *device = ath6kl_usb_priv(ar);
	return device->pipes[PipeID].urb_cnt;
}

u16 hif_get_max_queue_number(struct ath6kl *ar, u8 PipeID)
{
	struct ath6kl_usb *device = ath6kl_usb_priv(ar);
	return device->pipes[PipeID].urb_alloc;
}

void hif_detach_htc(struct ath6kl *ar)
{
	struct ath6kl_usb *device = ath6kl_usb_priv(ar);

	hif_usb_flush_all(device);

	memset(&device->htc_callbacks, 0, sizeof(struct hif_callbacks));
}

static int hif_usb_submit_ctrl_out(struct ath6kl_usb *device,
				   u8 req, u16 value, u16 index, void *data,
				   u32 size)
{
	u32 result = 0;
	int ret = 0;
	u8 *buf = NULL;

	if (size > 0) {
		buf = kmalloc(size, GFP_KERNEL);
		if (buf == NULL)
			return -ENOMEM;

		memcpy(buf, (u8 *) data, size);
	}
	result = usb_control_msg(device->udev,
				 usb_sndctrlpipe(device->udev, 0),
				 req,
				 USB_DIR_OUT | USB_TYPE_VENDOR |
				 USB_RECIP_DEVICE, value, index, buf,
				 size, 1000);

	if (result < 0) {
		ath6kl_dbg(ATH6KL_DBG_USB, "%s failed,result = %d\n",
			   __func__, result);
		ret = -EPERM;
	}

	kfree(buf);

	return ret;
}

static int hif_usb_submit_ctrl_in(struct ath6kl_usb *device,
				  u8 req, u16 value, u16 index, void *data,
				  u32 size)
{
	u32 result = 0;
	int ret = 0;
	u8 *buf = NULL;

	if (size > 0) {
		buf = kmalloc(size, GFP_KERNEL);
		if (buf == NULL)
			return -ENOMEM;
	}
	result = usb_control_msg(device->udev,
				 usb_rcvctrlpipe(device->udev, 0),
				 req,
				 USB_DIR_IN | USB_TYPE_VENDOR |
				 USB_RECIP_DEVICE, value, index, buf,
				 size, 2 * HZ);

	if (result < 0) {
		ath6kl_dbg(ATH6KL_DBG_USB, "%s failed,result = %d\n",
			   __func__, result);
		ret = -EPERM;
	}
	memcpy((u8 *) data, buf, size);

	kfree(buf);

	return ret;
}

static int hif_usb_ctrl_msg_exchange(struct ath6kl_usb *device,
				     u8 req_val, u8 *req_buf, u32 req_len,
				     u8 resp_val, u8 *resp_buf, u32 *resp_len)
{
	int status;

	/* send command */
	status =
	    hif_usb_submit_ctrl_out(device, req_val, 0, 0,
				    req_buf, req_len);

	if (status != 0)
		return status;

	if (resp_buf == NULL) {
		/* no expected response */
		return status;
	}

	/* get response */
	status =
	    hif_usb_submit_ctrl_in(device, resp_val, 0, 0,
				   resp_buf, *resp_len);

	return status;
}

static int ath6kl_usb_diag_read32(struct ath6kl *ar, u32 address, u32 *value)
{
	struct ath6kl_usb *device = (struct ath6kl_usb *)ar->hif_priv;
	int status;
	struct usb_ctrl_diag_cmd_read *cmd;
	u32 resp_len;

	cmd = (struct usb_ctrl_diag_cmd_read *)device->diag_cmd_buffer;

	memset(cmd, 0, sizeof(struct usb_ctrl_diag_cmd_read));
	cmd->cmd = cpu_to_le32(USB_CTRL_DIAG_CC_READ);
	cmd->address = cpu_to_le32(address);
	resp_len = sizeof(struct usb_ctrl_diag_resp_read);

	status = hif_usb_ctrl_msg_exchange(device,
					   USB_CONTROL_REQ_DIAG_CMD,
					   (u8 *) cmd,
					   sizeof(struct
						  usb_ctrl_diag_cmd_read),
					   USB_CONTROL_REQ_DIAG_RESP,
					   device->diag_resp_buffer, &resp_len);

	if (status == 0) {
		struct usb_ctrl_diag_resp_read *pResp =
		    (struct usb_ctrl_diag_resp_read *)device->diag_resp_buffer;
		*value = pResp->value;
	}

	return status;
}

static int ath6kl_usb_diag_write32(struct ath6kl *ar, u32 address, __le32 value)
{
	struct ath6kl_usb *device = (struct ath6kl_usb *)ar->hif_priv;
	struct usb_ctrl_diag_cmd_write *cmd;

	cmd = (struct usb_ctrl_diag_cmd_write *)device->diag_cmd_buffer;

	memset(cmd, 0, sizeof(struct usb_ctrl_diag_cmd_write));
	cmd->cmd = cpu_to_le32(USB_CTRL_DIAG_CC_WRITE);
	cmd->address = cpu_to_le32(address);
	cmd->value = cpu_to_le32(value);

	return hif_usb_ctrl_msg_exchange(device,
					 USB_CONTROL_REQ_DIAG_CMD,
					 (u8 *) cmd,
					 sizeof(struct usb_ctrl_diag_cmd_write),
					 0, NULL, 0);

}

static int ath6kl_usb_bmi_recv_buf(struct ath6kl *ar,
				   u8 *buf, u32 len, bool want_timeout)
{
	int status;
	struct ath6kl_usb *device = (struct ath6kl_usb *)ar->hif_priv;
	/* get response */
	status = hif_usb_submit_ctrl_in(device, USB_CONTROL_REQ_RECV_BMI_RESP,
					0, 0, buf, len);

	if (status != 0) {
		ath6kl_err("Unable to read the bmi data from the device: %d\n",
			   status);
		return status;
	}

	return 0;
}

static int ath6kl_usb_bmi_send_buf(struct ath6kl *ar, u8 * buf, u32 len)
{
	int status;
	struct ath6kl_usb *device = (struct ath6kl_usb *)ar->hif_priv;
	/* send command */
	status =
	    hif_usb_submit_ctrl_out(device, USB_CONTROL_REQ_SEND_BMI_CMD, 0, 0,
				    buf, len);

	if (status != 0) {
		ath6kl_err("unable to send the bmi data to the device\n");
		return status;
	}
	return 0;
}


static int ath6kl_usb_pipe_stat(struct ath6kl *ar, u8 *buf, int buf_len)
{
    struct ath6kl_usb *device = ath6kl_usb_priv(ar);
	struct hif_usb_pipe_stat *pipe_st;
	int i, len = 0;

	if ((!device) ||
		(!buf))
		return 0;
		
	for (i = 0; i < HIF_USB_PIPE_MAX; i++) {
		if ((i == HIF_RX_INT_PIPE) ||
			(i == HIF_RX_DATA2_PIPE))
			continue;

		pipe_st = &device->pipes[i].usb_pipe_stat;

		len += snprintf(buf + len, buf_len - len, "\nPIPE-%d\n", i);
		len += snprintf(buf + len, buf_len - len, " num_rx_comp        : %d\n", pipe_st->num_rx_comp);
		len += snprintf(buf + len, buf_len - len, " num_tx_comp        : %d\n", pipe_st->num_tx_comp);
		len += snprintf(buf + len, buf_len - len, " num_io_comp        : %d\n", pipe_st->num_io_comp);
		len += snprintf(buf + len, buf_len - len, " num_max_tx         : %d\n", pipe_st->num_max_tx);
		len += snprintf(buf + len, buf_len - len, " num_max_rx         : %d\n", pipe_st->num_max_rx);
		len += snprintf(buf + len, buf_len - len, " num_tx_resche      : %d\n", pipe_st->num_tx_resche);
		len += snprintf(buf + len, buf_len - len, " num_rx_resche      : %d\n", pipe_st->num_rx_resche);
		len += snprintf(buf + len, buf_len - len, " num_tx_sync        : %d\n", pipe_st->num_tx_sync);
		len += snprintf(buf + len, buf_len - len, " num_tx             : %d\n", pipe_st->num_tx);
		len += snprintf(buf + len, buf_len - len, " num_tx_err         : %d\n", pipe_st->num_tx_err);
		len += snprintf(buf + len, buf_len - len, " num_tx_err_others  : %d\n", pipe_st->num_tx_err_others);
		len += snprintf(buf + len, buf_len - len, " num_tx_comp_err    : %d\n", pipe_st->num_tx_comp_err);
		len += snprintf(buf + len, buf_len - len, " num_rx_comp_err    : %d\n", pipe_st->num_rx_comp_err);

		/* Bundle mode */
		if (htc_bundle_recv || htc_bundle_send) {
			len += snprintf(buf + len, buf_len - len, " num_rx_bundle_comp      : %d\n", pipe_st->num_rx_bundle_comp);
			len += snprintf(buf + len, buf_len - len, " num_tx_bundle_comp      : %d\n", pipe_st->num_tx_bundle_comp);
			len += snprintf(buf + len, buf_len - len, " num_tx_multi            : %d\n", pipe_st->num_tx_multi);
			len += snprintf(buf + len, buf_len - len, " num_tx_multi_err        : %d\n", pipe_st->num_tx_multi_err);
			len += snprintf(buf + len, buf_len - len, " num_tx_multi_err_others : %d\n", pipe_st->num_tx_multi_err_others);
			len += snprintf(buf + len, buf_len - len, " num_rx_bundle_comp_err  : %d\n", pipe_st->num_rx_bundle_comp_err);
			len += snprintf(buf + len, buf_len - len, " num_tx_bundle_comp_err  : %d\n", pipe_st->num_tx_bundle_comp_err);
		}
	}

	return len;
}

static int ath6kl_usb_max_sche(struct ath6kl *ar, u32 max_sche_tx, u32 max_sche_rx)
{
    struct ath6kl_usb *device = ath6kl_usb_priv(ar);

	device->max_sche_tx = max_sche_tx;
	device->max_sche_rx = max_sche_rx;

	ath6kl_dbg(ATH6KL_DBG_USB, "max_sche_tx = %d, max_sche_rx = %d\n", 
					device->max_sche_tx, device->max_sche_rx);

	return 0;
}

static const struct ath6kl_hif_ops ath6kl_usb_ops = {
	.diag_read32 = ath6kl_usb_diag_read32,
	.diag_write32 = ath6kl_usb_diag_write32,
	.bmi_recv_buf = ath6kl_usb_bmi_recv_buf,
	.bmi_send_buf = ath6kl_usb_bmi_send_buf,
	.get_stat = ath6kl_usb_pipe_stat,
	.set_max_sche = ath6kl_usb_max_sche,
};

static hif_product_info_t g_product_info;
void ath6kl_usb_get_usbinfo(void *product_info)
{
    memcpy(product_info,&g_product_info,sizeof(hif_product_info_t));
}

/* ath6kl usb driver registered functions */
static int ath6kl_usb_probe(struct usb_interface *interface,
			    const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(interface);
	struct ath6kl *ar;
	struct ath6kl_usb *ar_usb = NULL;
	int vendor_id, product_id;
	int result = 0;

	usb_get_dev(dev);

	g_product_info.idVendor = vendor_id = dev->descriptor.idVendor;
	g_product_info.idProduct = product_id = dev->descriptor.idProduct;   
    if(dev->product)
        memcpy(g_product_info.product,dev->product,sizeof(g_product_info.product)); 
    if(dev->manufacturer)
        memcpy(g_product_info.manufacturer,dev->manufacturer,sizeof(g_product_info.manufacturer));
    if(dev->serial)        
        memcpy(g_product_info.serial,dev->serial,sizeof(g_product_info.serial));          

	ath6kl_dbg(ATH6KL_DBG_USB, "vendor_id = %04x\n", vendor_id);
	ath6kl_dbg(ATH6KL_DBG_USB, "product_id = %04x\n", product_id);
	if (interface->cur_altsetting) {
		unsigned int i =
		    interface->cur_altsetting->desc.bInterfaceNumber;
		ath6kl_dbg(ATH6KL_DBG_USB, "USB Interface %d\n ", i);
	}

	if (dev->speed == USB_SPEED_HIGH)
		ath6kl_dbg(ATH6KL_DBG_USB, "USB 2.0 Host\n");
	else
		ath6kl_dbg(ATH6KL_DBG_USB, "USB 1.1 Host\n");

	ar_usb = hif_usb_create(interface);

	if (!ar_usb) {
		result = -ENOMEM;
		goto err_usb_device;
	}

	ar = ath6kl_core_alloc(&ar_usb->udev->dev);
	if (!ar) {
		ath6kl_err("Failed to alloc ath6kl core\n");
		result = -ENOMEM;
		goto err_ath6kl_core;
	}

	ar->hif_priv = ar_usb;
	ar->hif_type = HIF_TYPE_USB;
	ar->hif_ops = &ath6kl_usb_ops;
	ar->mbox_info.block_size = 16;
	ar_usb->claimed_context = ar;

	INIT_WORK(&ar->firmware_crash_dump_deferred_work, firmware_crash_work);

	result = ath6kl_core_init(ar);

	if (result) {
		ath6kl_err("Failed to init ath6kl core\n");
		goto err_ath6kl_core;
	}

	return result;

err_ath6kl_core:
	wiphy_unregister_ath6kl(ar->wiphy);
	wiphy_free_ath6kl(ar->wiphy);
	hif_usb_destroy(ar_usb);
err_usb_device:
	usb_put_dev(dev);
	return result;
}

static void ath6kl_usb_remove(struct usb_interface *interface)
{
	if (usb_get_intfdata(interface)) {
		usb_put_dev(interface_to_usbdev(interface));
		hif_usb_device_detached(interface, 1);
	}
}

#ifdef CONFIG_PM
static int ath6kl_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
	hif_usb_suspend(intf);
	return 0;
}

static int ath6kl_usb_resume(struct usb_interface *intf)
{
	hif_usb_resume(intf);
	return 0;
}

static int ath6kl_usb_reset_resume(struct usb_interface *intf)
{
	if (usb_get_intfdata(intf))
		ath6kl_usb_remove(intf);
	return 0;
}
#endif

/* table of devices that work with this driver */
static struct usb_device_id ath6kl_usb_ids[] = {
	{USB_DEVICE(VENDOR_ATHR, 0x9374)},
	{USB_DEVICE(VENDOR_ATHR, 0x1021)},
	{USB_DEVICE(VENDOR_ATHR, 0x9372)},
	{USB_DEVICE(VENDOR_PANA, 0x3908)},
	{ /* Terminating entry */ },
};

MODULE_DEVICE_TABLE(usb, ath6kl_usb_ids);

static struct usb_driver ath6kl_usb_driver = {
	.name = "ath6kl_usb",
	.probe = ath6kl_usb_probe,
#ifdef CONFIG_PM
	.suspend = ath6kl_usb_suspend,
	.resume = ath6kl_usb_resume,
	.reset_resume = ath6kl_usb_reset_resume,
#endif
	.disconnect = ath6kl_usb_remove,
	.id_table = ath6kl_usb_ids,
	.supports_autosuspend = true,
};

int ath6kl_usb_init(void)
{
	usb_register(&ath6kl_usb_driver);
	return 0;
}

void ath6kl_usb_exit(void)
{
	usb_deregister(&ath6kl_usb_driver);
}

EXPORT_SYMBOL(ath6kl_usb_get_usbinfo);

module_init(ath6kl_usb_init);
module_exit(ath6kl_usb_exit);

MODULE_AUTHOR("Atheros Communications, Inc.");
MODULE_DESCRIPTION("Driver support for Atheros AR600x USB devices");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_FIRMWARE(AR6004_REV1_FIRMWARE_FILE);
MODULE_FIRMWARE(AR6004_REV1_BOARD_DATA_FILE);
MODULE_FIRMWARE(AR6004_REV1_DEFAULT_BOARD_DATA_FILE);
MODULE_FIRMWARE(AR6004_REV2_FIRMWARE_FILE);
MODULE_FIRMWARE(AR6004_REV2_FIRMWARE_TX99_FILE);
MODULE_FIRMWARE(AR6004_REV2_BOARD_DATA_FILE);
MODULE_FIRMWARE(AR6004_REV2_DEFAULT_BOARD_DATA_FILE);
MODULE_FIRMWARE(AR6006_REV1_FIRMWARE_FILE);
MODULE_FIRMWARE(AR6006_REV1_BOARD_DATA_FILE);
MODULE_FIRMWARE(AR6006_REV1_DEFAULT_BOARD_DATA_FILE);
