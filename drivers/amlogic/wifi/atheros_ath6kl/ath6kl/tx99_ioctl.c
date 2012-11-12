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

#include <linux/vmalloc.h>

#include "core.h"
#include "hif-ops.h"
#include "cfg80211.h"
#include "target.h"
#include "debug.h"
//#include "ioctl_vendor_sony.h"
#include "sony_athtst.h"
#include "hif-ops.h"
#include "tx99_wcmd.h"

static inline struct sk_buff *ath6kl_wmi_get_new_buf(u32 size)
{
	struct sk_buff *skb;

	skb = ath6kl_buf_alloc(size);
	if (!skb)
		return NULL;

	skb_put(skb, size);
	if (size)
		memset(skb->data, 0, size);

	return skb;
}

int ath6kl_wmi_tx99_cmd(struct net_device *netdev, void *data)
{
	struct ath6kl_vif *vif = ath6kl_priv(netdev);
    tx99_wcmd_t     *req = NULL;
    unsigned long      req_len = sizeof(tx99_wcmd_t);
    unsigned long      status = EIO;

    req = vmalloc(req_len);
   
    if(!req)
        return ENOMEM;
    
    memset(req, 0, req_len);
 
    /* XXX: Optimize only copy the amount thats valid */
    if(copy_from_user(req, data, req_len))
        goto done;
        
//try to use exist WMI command to implement ATHTST command behavior
//TBD
    printk("%s[%d]req->if_name=%s\n\r",__func__,__LINE__,req->if_name);
    printk("%s[%d]req->type=%d\n\r",__func__,__LINE__,req->type);
    printk("%s[%d]req->data.freq=%d\n\r",__func__,__LINE__,req->data.freq);
    printk("%s[%d]req->data.htmode=%d\n\r",__func__,__LINE__,req->data.htmode);
    printk("%s[%d]req->data.htext=%d\n\r",__func__,__LINE__,req->data.htext);
    printk("%s[%d]req->data.rate=%d\n\r",__func__,__LINE__,req->data.rate);
    printk("%s[%d]req->data.power=%d\n\r",__func__,__LINE__,req->data.power);
    printk("%s[%d]req->data.txmode=%d\n\r",__func__,__LINE__,req->data.txmode);
    printk("%s[%d]req->data.chanmask=%d\n\r",__func__,__LINE__,req->data.chanmask);
    printk("%s[%d]req->data.type=%d\n\r",__func__,__LINE__,req->data.type);
    
    #if 1
{    
	struct sk_buff *skb;
	struct WMI_TX99_STRUCT *cmd;
	int ret;

    skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
    if (!skb)
        return -ENOMEM;

    cmd = (struct WMI_TX99_STRUCT *) skb->data;
    //transfer to little endian
    memcpy(cmd->if_name, req->if_name,  sizeof(cmd->if_name));
    
    cmd->type = cpu_to_le32(req->type); 
    
    cmd->data.freq = cpu_to_le32(req->data.freq);
    cmd->data.htmode = cpu_to_le32(req->data.htmode);
    cmd->data.htext = cpu_to_le32(req->data.htext);
    cmd->data.rate = cpu_to_le32(req->data.rate);
    cmd->data.power = cpu_to_le32(req->data.power);
    cmd->data.txmode = cpu_to_le32(req->data.txmode);
    cmd->data.chanmask = cpu_to_le32(req->data.chanmask);
    cmd->data.type = cpu_to_le32(req->data.type);
    
    ret = ath6kl_wmi_cmd_send(vif, skb, WMI_TX99TOOL_CMDID,
                  NO_SYNC_WMIFLAG);
}	    
    #endif
    
    //status = __athtst_wioctl_sw[req->iic_cmd](sc, (athcfg_wcmd_data_t *)&req->iic_data);       
    status = 0;

    /* XXX: Optimize only copy the amount thats valid */
    if(copy_to_user(data, req, req_len))
        status = -EIO;

done:
    vfree(req);

    return status;
}
