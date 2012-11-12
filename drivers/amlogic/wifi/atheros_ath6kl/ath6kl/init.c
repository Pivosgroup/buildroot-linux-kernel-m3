/*
 * Copyright (c) 2011 Atheros Communications Inc.
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

#include <linux/mmc/sdio_func.h>
#include "core.h"
#include "cfg80211.h"
#include "target.h"
#include "debug.h"
#include "hif-ops.h"

unsigned int debug_mask;//=ATH6KL_DBG_ANY;
static unsigned short reg_domain = 0xffff;
unsigned int debug_fpga = 0;
unsigned int debug_fpga_rom512 = 1;
unsigned int ath6kl_p2p;
unsigned int ath6kl_tx99;
unsigned int ath6kl_wow_ext;
unsigned int ath6kl_wow_gpio = 0;
/* not enabling bundle recv/send by default */
unsigned int htc_bundle_recv = 0;
unsigned int htc_bundle_send = 0;
unsigned int chip_pwd_l = 1;
unsigned int chip_pwd_l_pin = 4;
unsigned int ath6kl_lpl_attach = 0;
unsigned int ath6kl_mimo_ps_enable = 0;
unsigned int refclk_hz = 40;
unsigned int starving_prevention = 0;
unsigned int ani = 1;

extern const char fw_ar6004_1_1[];
extern int fw_ar6004_1_1_size;
extern const char bdata_ar6004_1_1[];
extern int bdata_ar6004_1_1_size;

module_param(debug_mask, uint, 0644);
module_param(debug_fpga, uint, 0644);
module_param(debug_fpga_rom512, uint, 0644);
module_param(ath6kl_p2p, uint, 0644);
module_param(ath6kl_tx99, uint, 0644);
module_param(reg_domain, ushort, 0644);
module_param(htc_bundle_recv, uint, 0644);
module_param(htc_bundle_send, uint, 0644);
module_param(ath6kl_wow_ext, uint, 0644);
module_param(ath6kl_wow_gpio, uint, 0644);
module_param(chip_pwd_l, uint, 0644);
module_param(chip_pwd_l_pin, uint, 0644);
module_param(ath6kl_lpl_attach, uint, 0644);
module_param(ath6kl_mimo_ps_enable, uint, 0644);
module_param(refclk_hz, uint, 0644);
module_param(starving_prevention, uint, 0644);
module_param(ani, uint, 0644);

/*
 * Include definitions here that can be used to tune the WLAN module
 * behavior. Different customers can tune the behavior as per their needs,
 * here.
 */

/*
 * This configuration item enable/disable keepalive support.
 * Keepalive support: In the absence of any data traffic to AP, null
 * frames will be sent to the AP at periodic interval, to keep the association
 * active. This configuration item defines the periodic interval.
 * Use value of zero to disable keepalive support
 * Default: 60 seconds
 */
#define WLAN_CONFIG_KEEP_ALIVE_INTERVAL 60

/*
 * This configuration item sets the value of disconnect timeout
 * Firmware delays sending the disconnec event to the host for this
 * timeout after is gets disconnected from the current AP.
 * If the firmware successly roams within the disconnect timeout
 * it sends a new connect event
 */
#define WLAN_CONFIG_DISCONNECT_TIMEOUT 5

#define CONFIG_AR600x_DEBUG_UART_TX_PIN 8
#define CONFIG_AR6004_DEBUG_UART_TX_PIN 11
#define CONFIG_AR6006_DEBUG_UART_TX_PIN 11

enum addr_type {
	DATASET_PATCH_ADDR,
	APP_LOAD_ADDR,
	APP_START_OVERRIDE_ADDR,
};

#define ATH6KL_DATA_OFFSET    64
struct sk_buff *ath6kl_buf_alloc(int size)
{
	struct sk_buff *skb;
	u16 reserved;

	/* Add chacheline space at front and back of buffer */
	reserved = (2 * L1_CACHE_BYTES) + ATH6KL_DATA_OFFSET + sizeof(struct htc_packet);
	skb = dev_alloc_skb(size + reserved);

	if (skb)
		skb_reserve(skb, reserved - L1_CACHE_BYTES);
	return skb;
}

void ath6kl_init_profile_info(struct ath6kl_vif *vif)
{
	vif->ssid_len = 0;
	memset(vif->ssid, 0, sizeof(vif->ssid));

	vif->dot11_auth_mode = OPEN_AUTH;
	vif->auth_mode = NONE_AUTH;
	vif->prwise_crypto = NONE_CRYPT;
	vif->prwise_crypto_len = 0;
	vif->grp_crypto = NONE_CRYPT;
	vif->grp_crypto_len = 0;
	memset(vif->wep_key_list, 0, sizeof(vif->wep_key_list));
	memset(vif->req_bssid, 0, sizeof(vif->req_bssid));
	memset(vif->bssid, 0, sizeof(vif->bssid));
	vif->bss_ch = 0;
	vif->nw_type = vif->next_mode = INFRA_NETWORK;
        vif->wmm_enabled =  true;
}

static int ath6kl_set_rd(struct ath6kl *ar) {
	u8 buf[16];
	u16 o_sum, o_ver, o_rd, o_rd_next;
	u32 n_rd, n_sum;
	u32 bd_addr = 0;
	int ret;
    u32 rd_offset;

	switch (ar->target_type) {
	case TARGET_TYPE_AR6003:
	    rd_offset = AR6003_RD_OFFSET;
		ret = ath6kl_bmi_read(ar, AR6003_BOARD_DATA_ADDR, (u8 *)&bd_addr, 4);
		break;
	case TARGET_TYPE_AR6004:
	    rd_offset = AR6004_RD_OFFSET;	
		ret = ath6kl_bmi_read(ar, AR6004_BOARD_DATA_ADDR, (u8 *)&bd_addr, 4);
		break;
	case TARGET_TYPE_AR6006:
	    if (debug_fpga) {
	        rd_offset = AR6006_RD_FPGA_OFFSET;	
		} 
		else {
	        rd_offset = AR6006_RD_OFFSET;	
	    }
		ret = ath6kl_bmi_read(ar, AR6006_BOARD_DATA_ADDR, (u8 *)&bd_addr, 4);
		break;
	default:
		return -EINVAL;
	}

	if (ret)
		return ret;

	memset(buf, 0, sizeof(buf));
	ret = ath6kl_bmi_read(ar, bd_addr, buf, sizeof(buf));
	if (ret)
		return ret;
	switch (ar->target_type) {
	case TARGET_TYPE_AR6003:
		memcpy((u8 *)&o_sum, buf + AR6003_BOARD_DATA_OFFSET, 2);
		memcpy((u8 *)&o_ver, buf + AR6003_BOARD_DATA_OFFSET + 2, 2);
		memcpy((u8 *)&o_rd, buf + AR6003_RD_OFFSET, 2);
		memcpy((u8 *)&o_rd_next, buf + AR6003_RD_OFFSET + 2, 2);
		break;
	case TARGET_TYPE_AR6004:
		memcpy((u8 *)&o_sum, buf + AR6004_BOARD_DATA_OFFSET, 2);
		memcpy((u8 *)&o_ver, buf + AR6004_BOARD_DATA_OFFSET + 2, 2);
		memcpy((u8 *)&o_rd, buf + AR6004_RD_OFFSET, 2);
		memcpy((u8 *)&o_rd_next, buf + AR6004_RD_OFFSET + 2, 2);
		break;
	case TARGET_TYPE_AR6006:
		memcpy((u8 *)&o_sum, buf + AR6006_BOARD_DATA_OFFSET, 2);
		memcpy((u8 *)&o_ver, buf + AR6006_BOARD_DATA_OFFSET + 2, 2);
		memcpy((u8 *)&o_rd, buf + rd_offset, 2);
		memcpy((u8 *)&o_rd_next, buf + rd_offset + 2, 2);
		break;
	default:
		return -EINVAL;
	}
	n_rd = (o_rd_next << 16) + reg_domain;
	ret = ath6kl_bmi_write(ar, bd_addr + rd_offset, (u8 *)&n_rd, 4);		
	if (ret)
		return ret;

	n_sum = (o_ver << 16) + (o_sum ^ o_rd ^ reg_domain);
	switch (ar->target_type) {
	case TARGET_TYPE_AR6003:
		ret = ath6kl_bmi_write(ar, bd_addr + AR6003_BOARD_DATA_OFFSET,
			       (u8 *)&n_sum, 4);
		break;
	case TARGET_TYPE_AR6004:
		ret = ath6kl_bmi_write(ar, bd_addr + AR6004_BOARD_DATA_OFFSET,
			       (u8 *)&n_sum, 4);
		break;
	case TARGET_TYPE_AR6006:
		ret = ath6kl_bmi_write(ar, bd_addr + AR6006_BOARD_DATA_OFFSET,
			       (u8 *)&n_sum, 4);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int ath6kl_set_host_app_area(struct ath6kl *ar)
{
	u32 address, data;
	struct host_app_area host_app_area;

	/* Fetch the address of the host_app_area_s
	 * instance in the host interest area */
	address = ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_app_host_interest));
	address = TARG_VTOP(ar->target_type, address);

	if (ath6kl_diag_read32(ar, address, &data))
		return -EIO;

	address = TARG_VTOP(ar->target_type, data);
	host_app_area.wmi_protocol_ver = WMI_PROTOCOL_VERSION;
	if (ath6kl_diag_write(ar, address,
				(u8 *)&host_app_area,
				sizeof(struct host_app_area)))
		return -EIO;

	return 0;
}

static inline void set_ac2_ep_map(struct ath6kl *ar,
				  u8 ac,
				  enum htc_endpoint_id ep)
{
	ar->ac2ep_map[ac] = ep;
	ar->ep2ac_map[ep] = ac;
}

/* connect to a service */
static int ath6kl_connectservice(struct ath6kl *ar,
				 struct htc_service_connect_req  *con_req,
				 char *desc)
{
	int status;
	struct htc_service_connect_resp response;

	memset(&response, 0, sizeof(response));

	status = ath6kl_htc_conn_service(ar->htc_target, con_req, &response);
	if (status) {
		ath6kl_err("failed to connect to %s service status:%d\n",
			   desc, status);
		return status;
	}

	switch (con_req->svc_id) {
	case WMI_CONTROL_SVC:
		if (test_bit(WMI_ENABLED, &ar->flag))
			ath6kl_wmi_set_control_ep(ar->wmi, response.endpoint);
		ar->ctrl_ep = response.endpoint;
		break;
	case WMI_DATA_BE_SVC:
		set_ac2_ep_map(ar, WMM_AC_BE, response.endpoint);
		break;
	case WMI_DATA_BK_SVC:
		set_ac2_ep_map(ar, WMM_AC_BK, response.endpoint);
		break;
	case WMI_DATA_VI_SVC:
		set_ac2_ep_map(ar, WMM_AC_VI, response.endpoint);
		break;
	case WMI_DATA_VO_SVC:
		set_ac2_ep_map(ar, WMM_AC_VO, response.endpoint);
		break;
	default:
		ath6kl_err("service id is not mapped %d\n", con_req->svc_id);
		return -EINVAL;
	}

	return 0;
}

static int ath6kl_init_service_ep(struct ath6kl *ar)
{
	struct htc_service_connect_req connect;

	memset(&connect, 0, sizeof(connect));

	/* these fields are the same for all service endpoints */
	connect.ep_cb.tx_comp_multi = ath6kl_tx_complete;
	connect.ep_cb.rx = ath6kl_rx;
	connect.ep_cb.rx_refill = ath6kl_rx_refill;
	connect.ep_cb.tx_full = ath6kl_tx_queue_full;

	/*
	 * Set the max queue depth so that our ath6kl_tx_queue_full handler
	 * gets called.
	*/
	connect.max_txq_depth = MAX_DEFAULT_SEND_QUEUE_DEPTH;
	connect.ep_cb.rx_refill_thresh = ATH6KL_MAX_RX_BUFFERS / 4;
	if (!connect.ep_cb.rx_refill_thresh)
		connect.ep_cb.rx_refill_thresh++;

	/* connect to control service */
	connect.svc_id = WMI_CONTROL_SVC;
	if (ath6kl_connectservice(ar, &connect, "WMI CONTROL"))
		return -EIO;

	connect.flags |= HTC_FLGS_TX_BNDL_PAD_EN;

	/*
	 * Limit the HTC message size on the send path, although e can
	 * receive A-MSDU frames of 4K, we will only send ethernet-sized
	 * (802.3) frames on the send path.
	 */
	connect.max_rxmsg_sz = WMI_MAX_TX_DATA_FRAME_LENGTH;

	/*
	 * To reduce the amount of committed memory for larger A_MSDU
	 * frames, use the recv-alloc threshold mechanism for larger
	 * packets.
	 */
	connect.ep_cb.rx_alloc_thresh = ATH6KL_BUFFER_SIZE;
	connect.ep_cb.rx_allocthresh = ath6kl_alloc_amsdu_rxbuf;

	/*
	 * For the remaining data services set the connection flag to
	 * reduce dribbling, if configured to do so.
	 */
	connect.conn_flags |= HTC_CONN_FLGS_REDUCE_CRED_DRIB;
	connect.conn_flags &= ~HTC_CONN_FLGS_THRESH_MASK;
	connect.conn_flags |= HTC_CONN_FLGS_THRESH_LVL_HALF;

	connect.svc_id = WMI_DATA_BE_SVC;

	if (ath6kl_connectservice(ar, &connect, "WMI DATA BE"))
		return -EIO;

	/* connect to back-ground map this to WMI LOW_PRI */
	connect.svc_id = WMI_DATA_BK_SVC;
	if (ath6kl_connectservice(ar, &connect, "WMI DATA BK"))
		return -EIO;

	/* connect to Video service, map this to to HI PRI */
	connect.svc_id = WMI_DATA_VI_SVC;
	if (ath6kl_connectservice(ar, &connect, "WMI DATA VI"))
		return -EIO;

	/*
	 * Connect to VO service, this is currently not mapped to a WMI
	 * priority stream due to historical reasons. WMI originally
	 * defined 3 priorities over 3 mailboxes We can change this when
	 * WMI is reworked so that priorities are not dependent on
	 * mailboxes.
	 */
	connect.svc_id = WMI_DATA_VO_SVC;
	if (ath6kl_connectservice(ar, &connect, "WMI DATA VO"))
		return -EIO;

	return 0;
}

static void ath6kl_uninit_control_info(struct ath6kl_vif *vif)
{
	u8 ctr;
	for (ctr = 0; ctr < AP_MAX_NUM_STA; ctr++) {
		if (!skb_queue_empty(&vif->sta_list[ctr].psq)) {
			skb_queue_purge(&vif->sta_list[ctr].psq);
		}
		if (!ath6kl_mgmt_queue_empty(&vif->sta_list[ctr].mgmt_psq)) {
			ath6kl_mgmt_queue_purge(&vif->sta_list[ctr].mgmt_psq);
		}
	}
}

static void ath6kl_init_control_info(struct ath6kl_vif *vif)
{
	u8 ctr;

	ath6kl_init_profile_info(vif);
	vif->def_txkey_index = 0;
	memset(vif->wep_key_list, 0, sizeof(vif->wep_key_list));
	vif->ch_hint = 0;
	vif->listen_intvl_t = A_DEFAULT_LISTEN_INTERVAL;
	vif->listen_intvl_b = 0;
	vif->tx_pwr = 0;
	clear_bit(SKIP_SCAN, &vif->flag);
	/* enable WMM for wlan0 device only */
	if (vif->device_index == 0)
		set_bit(WMM_ENABLED, &vif->flag);
	vif->intra_bss = 1;
	memset(&vif->sc_params, 0, sizeof(vif->sc_params));
	vif->sc_params.short_scan_ratio = WMI_SHORTSCANRATIO_DEFAULT;
	vif->sc_params.scan_ctrl_flags = DEFAULT_SCAN_CTRL_FLAGS;

	memset((u8 *)vif->sta_list, 0,
	       AP_MAX_NUM_STA * sizeof(struct ath6kl_sta));

	spin_lock_init(&vif->mcastpsq_lock);

	/* Init the PS queues */
	for (ctr = 0; ctr < AP_MAX_NUM_STA; ctr++) {
		spin_lock_init(&vif->sta_list[ctr].lock);
		skb_queue_head_init(&vif->sta_list[ctr].psq);
		ath6kl_mgmt_queue_init(&vif->sta_list[ctr].mgmt_psq);
		vif->sta_list[ctr].aggr_conn_cntxt = NULL;
	}

	/* Init the wow attributes */
	vif->ar->wiphy->wowlan.flags = WIPHY_WOWLAN_ANY |
						WIPHY_WOWLAN_MAGIC_PKT |
						WIPHY_WOWLAN_DISCONNECT |
						WIPHY_WOWLAN_SUPPORTS_GTK_REKEY |
						WIPHY_WOWLAN_GTK_REKEY_FAILURE |
						WIPHY_WOWLAN_EAP_IDENTITY_REQ	|
						WIPHY_WOWLAN_4WAY_HANDSHAKE	|
						WIPHY_WOWLAN_RFKILL_RELEASE;

	vif->ar->wiphy->wowlan.n_patterns = 
			(WOW_MAX_FILTER_LISTS*WOW_MAX_FILTERS_PER_LIST);
	vif->ar->wiphy->wowlan.pattern_min_len = 
			WOW_MIN_PATTERN_SIZE;
	vif->ar->wiphy->wowlan.pattern_max_len =
			WOW_MAX_PATTERN_SIZE;
	ath6kl_dbg(ATH6KL_DBG_WOWLAN, "wow attr: flags: %x, n_patterns: %d, ",
			vif->ar->wiphy->wowlan.flags, 
					vif->ar->wiphy->wowlan.n_patterns);
	ath6kl_dbg(ATH6KL_DBG_WOWLAN, "min_len: %d, max_len: %d\n",
			vif->ar->wiphy->wowlan.pattern_min_len, 
					vif->ar->wiphy->wowlan.pattern_max_len);

	skb_queue_head_init(&vif->mcastpsq);

	memcpy(vif->ap_country_code, DEF_AP_COUNTRY_CODE, 3);
}

/*
 * Set HTC/Mbox operational parameters, this can only be called when the
 * target is in the BMI phase.
 */
static int ath6kl_set_htc_params(struct ath6kl *ar, u32 mbox_isr_yield_val,
				 u8 htc_ctrl_buf)
{
	int status;
	u32 blk_size;

	blk_size = ar->mbox_info.block_size;

	if (htc_ctrl_buf)
		blk_size |=  ((u32)htc_ctrl_buf) << 16;

	/* set the host interest area for the block size */
	status = ath6kl_bmi_write(ar,
			ath6kl_get_hi_item_addr(ar,
			HI_ITEM(hi_mbox_io_block_sz)),
			(u8 *)&blk_size,
			4);
	if (status) {
		ath6kl_err("bmi_write_memory for IO block size failed\n");
		goto out;
	}

	ath6kl_dbg(ATH6KL_DBG_TRC, "block size set: %d (target addr:0x%X)\n",
		   blk_size,
		   ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_mbox_io_block_sz)));

	if (mbox_isr_yield_val) {
		/* set the host interest area for the mbox ISR yield limit */
		status = ath6kl_bmi_write(ar,
				ath6kl_get_hi_item_addr(ar,
				HI_ITEM(hi_mbox_isr_yield_limit)),
				(u8 *)&mbox_isr_yield_val,
				4);
		if (status) {
			ath6kl_err("bmi_write_memory for yield limit failed\n");
			goto out;
		}
	}

out:
	return status;
}

#define REG_DUMP_COUNT_AR6003   60
#define REGISTER_DUMP_LEN_MAX   60

static void ath6kl_dump_target_assert_info(struct ath6kl *ar)
{
	u32 address;
	u32 regdump_loc = 0;
	int status;
	u32 regdump_val[REGISTER_DUMP_LEN_MAX];
	u32 i;

	if (ar->target_type != TARGET_TYPE_AR6003)
		return;

	/* the reg dump pointer is copied to the host interest area */
	address = ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_failure_state));
	address = TARG_VTOP(ar->target_type, address);

	/* read RAM location through diagnostic window */
	status = ath6kl_diag_read32(ar, address, &regdump_loc);

	if (status || !regdump_loc) {
		ath6kl_err("failed to get ptr to register dump area\n");
		return;
	}

	ath6kl_dbg(ATH6KL_DBG_TRC, "location of register dump data: 0x%X\n",
		regdump_loc);
	regdump_loc = TARG_VTOP(ar->target_type, regdump_loc);

	/* fetch register dump data */
	status = ath6kl_diag_read(ar,
					regdump_loc,
					(u8 *)&regdump_val[0],
					REG_DUMP_COUNT_AR6003 * (sizeof(u32)));

	if (status) {
		ath6kl_err("failed to get register dump\n");
		return;
	}
	ath6kl_dbg(ATH6KL_DBG_TRC, "Register Dump:\n");

	for (i = 0; i < REG_DUMP_COUNT_AR6003; i++)
		ath6kl_dbg(ATH6KL_DBG_TRC, " %d :  0x%8.8X\n",
			   i, regdump_val[i]);

}

void ath6kl_target_failure(struct ath6kl *ar)
{
	ath6kl_err("target asserted\n");

	/* try dumping target assertion information (if any) */
	ath6kl_dump_target_assert_info(ar);

}

static int ath6kl_target_config_wlan_params(struct ath6kl_vif *vif)
{
	int status = 0;
	int ret;

	/*
	 * Configure the device for rx dot11 header rules. "0,0" are the
	 * default values. Required if checksum offload is needed. Set
	 * RxMetaVersion to 2.
	 */
	if (ath6kl_wmi_set_rx_frame_format_cmd(vif,
					       vif->ar->rx_meta_ver, 0, 0)) {
		ath6kl_err("unable to set the rx frame format\n");
		status = -EIO;
	}

	if (vif->ar->conf_flags & ATH6KL_CONF_IGNORE_PS_FAIL_EVT_IN_SCAN) {
		if ((ath6kl_wmi_pmparams_cmd(vif, 0, 3, 0, 0, 1,
		     IGNORE_POWER_SAVE_FAIL_EVENT_DURING_SCAN)) != 0) {
			ath6kl_err("unable to set power save fail event policy\n");
			status = -EIO;
		}
	} else {
	    /* ps_poll_num is 3 */
		if ((ath6kl_wmi_pmparams_cmd(vif, 0, 3, 3, 1, 0, 0)) != 0) {
			ath6kl_err("unable to set power save params\n");
			status = -EIO;
		}
    }

	if (!(vif->ar->conf_flags & ATH6KL_CONF_IGNORE_ERP_BARKER))
		if ((ath6kl_wmi_set_lpreamble_cmd(vif, 0,
		     WMI_DONOT_IGNORE_BARKER_IN_ERP)) != 0) {
			ath6kl_err("unable to set barker preamble policy\n");
			status = -EIO;
		}

	if (ath6kl_wmi_set_keepalive_cmd(vif,
			WLAN_CONFIG_KEEP_ALIVE_INTERVAL)) {
		ath6kl_err("unable to set keep alive interval\n");
		status = -EIO;
	}

	if (ath6kl_wmi_disctimeout_cmd(vif,
			WLAN_CONFIG_DISCONNECT_TIMEOUT)) {
		ath6kl_err("unable to set disconnect timeout\n");
		status = -EIO;
	}

	if (!(vif->ar->conf_flags & ATH6KL_CONF_ENABLE_TX_BURST))
		if (ath6kl_wmi_set_wmm_txop(vif, WMI_TXOP_DISABLED)) {
			ath6kl_err("unable to set txop bursting\n");
			status = -EIO;
		}

	if (vif->p2p) {
		ret = ath6kl_wmi_info_req_cmd(vif,
					P2P_FLAG_CAPABILITIES_REQ |
					P2P_FLAG_MACADDR_REQ |
					P2P_FLAG_HMODEL_REQ);
		if (ret) {
			printk(KERN_DEBUG "ath6l: Failed to request P2P "
				"capabilities (%d) - assuming P2P not "
				"supported\n", ret);
			vif->ar->p2p = vif->p2p = 0;
		}
	}

	if (vif->ar->p2p) {
		/* Enable Probe Request reporting for P2P */
		ret = ath6kl_wmi_probe_report_req_cmd(vif, true);
		if (ret) {
			printk(KERN_DEBUG "ath6l: Failed to enable Probe Request "
			       "reporting (%d)\n", ret);
		}
	}

	return status;
}

int ath6kl_configure_target(struct ath6kl *ar)
{
	u32 param, ram_reserved_size;
	u32 fw_iftype, fw_sub_iftype;
	int i;

/* ATH6KL temp uart temp enable */
       param = 115200;
       if (ath6kl_bmi_write(ar,
               ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_desired_baud_rate)),
               (u8 *)&param, 4) != 0) {
               ath6kl_err("%s: failed to set hprx traffic ratio in target\n",
                       __func__);
               return -EIO;
       }

	/* Ratio of high priority RX traffic to total RX traffic expressed
	 * in percentage. Use 0 to disable HP RX Q */
	param = 0;
	if (ath6kl_bmi_write(ar,
		ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_hp_rx_traffic_ratio)),
		(u8 *)&param, 4) != 0) {
		ath6kl_err("%s: failed to set hprx traffic ratio in target\n",
			__func__);
		return -EIO;
	}

	/* Number of buffers used on the target for logging packets; use
	 * zero to disable logging */
	param = 0;
	if (ath6kl_bmi_write(ar,
		ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_pktlog_num_buffers)),
		(u8 *)&param, 4) != 0) {
		ath6kl_err("%s: failed to set pktlog buffers in target\n",
			__func__);
		return -EIO;
	}

	param = 1;
	if (ath6kl_bmi_write(ar,
			     ath6kl_get_hi_item_addr(ar,
			     HI_ITEM(hi_serial_enable)),
			     (u8 *)&param, 4) != 0) {
		ath6kl_err("%s: failed to enable uart print in target\n",
			__func__);
		return -EIO;
	}
	ath6kl_dbg(ATH6KL_DBG_TRC, "serial console prints enabled\n");

/* ATH6KL temp end */
	/* Tell target which HTC version it is used*/
	param = HTC_PROTOCOL_VERSION;
	if (ath6kl_bmi_write(ar,
			     ath6kl_get_hi_item_addr(ar,
			     HI_ITEM(hi_app_host_interest)),
			     (u8 *)&param, 4) != 0) {
		ath6kl_err("bmi_write_memory for htc version failed\n");
		return -EIO;
	}

	/* set the firmware mode to STA/IBSS/AP */
	param = 0;

	if (ath6kl_bmi_read(ar,
			    ath6kl_get_hi_item_addr(ar,
			    HI_ITEM(hi_option_flag)),
			    (u8 *)&param, 4) != 0) {
		ath6kl_err("bmi_read_memory for setting fwmode failed\n");
		return -EIO;
	}

	/*
	 * Set the firmware mode to station type.
	 * Configure two vifs as none sub type.
	 * Configure the last vif as p2p dev sub type
	 */
	fw_iftype = fw_sub_iftype = 0;
	/*
	 * FIXME: enable both p2p with multi-if and wow_ext
	 * feature will be leak of head size.
	 * Currently we can enable one at the same time.
	 */
	if (ar->p2p) {
		for (i = 0; i < NUM_DEV; i++) {
			fw_iftype |= (HI_OPTION_FW_MODE_BSS_STA <<
				(i  * HI_OPTION_FW_MODE_BITS));
			if (i == (NUM_DEV - 1))
				fw_sub_iftype = (HI_OPTION_FW_SUBMODE_P2PDEV <<
						(i  * HI_OPTION_FW_SUBMODE_BITS));
		}
		param |= (NUM_DEV << HI_OPTION_NUM_DEV_SHIFT);
	} else {
		fw_iftype = HI_OPTION_FW_MODE_BSS_STA;
		param |= (1 << HI_OPTION_NUM_DEV_SHIFT);
	}
	param |= (fw_iftype << HI_OPTION_FW_MODE_SHIFT);
	param |= (fw_sub_iftype << HI_OPTION_FW_SUBMODE_SHIFT);

	param |= (0 << HI_OPTION_MAC_ADDR_METHOD_SHIFT);
	param |= (0 << HI_OPTION_FW_BRIDGE_SHIFT);

	printk("fw_iftype: 0x%x, fw_sub_iftype: 0x%x, param: 0x%x\n", 
		fw_iftype, fw_sub_iftype, param);

	if (ath6kl_bmi_write(ar,
			     ath6kl_get_hi_item_addr(ar,
			     HI_ITEM(hi_option_flag)),
			     (u8 *)&param,
			     4) != 0) {
		ath6kl_err("bmi_write_memory for setting fwmode failed\n");
		return -EIO;
	}

	ath6kl_dbg(ATH6KL_DBG_TRC, "firmware mode set\n");

	/*
	 * Hardcode the address use for the extended board data
	 * Ideally this should be pre-allocate by the OS at boot time
	 * But since it is a new feature and board data is loaded
	 * at init time, we have to workaround this from host.
	 * It is difficult to patch the firmware boot code,
	 * but possible in theory.
	 */

	if (ar->target_type == TARGET_TYPE_AR6003) {
		if (ar->version.target_ver == AR6003_REV2_VERSION) {
			param = AR6003_REV2_BOARD_EXT_DATA_ADDRESS;
			ram_reserved_size =  AR6003_REV2_RAM_RESERVE_SIZE;
		} else {
			param = AR6003_REV3_BOARD_EXT_DATA_ADDRESS;
			ram_reserved_size =  AR6003_REV3_RAM_RESERVE_SIZE;
		}

		if (ath6kl_bmi_write(ar,
				     ath6kl_get_hi_item_addr(ar,
				     HI_ITEM(hi_board_ext_data)),
				     (u8 *)&param, 4) != 0) {
			ath6kl_err("bmi_write_memory for hi_board_ext_data failed\n");
			return -EIO;
		}
		if (ath6kl_bmi_write(ar,
				     ath6kl_get_hi_item_addr(ar,
				     HI_ITEM(hi_end_ram_reserve_sz)),
				     (u8 *)&ram_reserved_size, 4) != 0) {
			ath6kl_err("bmi_write_memory for hi_end_ram_reserve_sz failed\n");
			return -EIO;
		}
	}

	/* set the block size for the target */
	if (ath6kl_set_htc_params(ar, MBOX_YIELD_LIMIT, 0))
		/* use default number of control buffers */
		return -EIO;

	return 0;
}

void ath6kl_if_remove(struct ath6kl_vif *vif)
{
	int i;

	for (i = 0; i < AP_MAX_NUM_STA ; i++)
		aggr_module_destroy_conn(vif->sta_list[i].aggr_conn_cntxt);	

	aggr_module_destroy(vif->aggr_cntxt);
	
	if ( test_bit(NETDEV_REGISTERED, &vif->flag)) {
		unregister_netdev(vif->net_dev);
		clear_bit(NETDEV_REGISTERED, &vif->flag);
	}
	ath6kl_cfg80211_deinit(vif);
	ath6kl_uninit_control_info(vif);
	free_netdev(vif->net_dev);
}


int ath6kl_if_add(struct ath6kl *ar, struct net_device **dev, char *name, u8 if_type, u8 dev_index)
{
	struct wireless_dev *wdev;
	struct ath6kl_vif *vif;
	struct net_device *ndev;
	int i, ret;

	wdev = kzalloc(sizeof(struct wireless_dev), GFP_KERNEL);
	if (!wdev) {
		ath6kl_err("couldn't allocate wireless device\n");
		return -ENOMEM;
	}

	wdev->wiphy = ar->wiphy;

	if(!name)
	    if ( ath6kl_p2p == 2 )
		    ndev = alloc_netdev(sizeof(struct ath6kl_vif), "sta%d", ether_setup);
		else
		    ndev = alloc_netdev(sizeof(struct ath6kl_vif), "wlan%d", ether_setup);
	else
		ndev = alloc_netdev(sizeof(struct ath6kl_vif), name, ether_setup);
	if (!ndev) {
		ath6kl_err("no memory for network device instance\n");
		kfree(wdev);
		return -ENOMEM;
	}

	vif = netdev_priv(ndev);

	printk("ar = %p\t vif = %p, num_dev=%d \n", ar, vif, ar->num_dev);
	vif->ar = ar;
	vif->wdev = wdev;
	wdev->iftype = if_type;
	vif->device_index = dev_index;

	ndev->ieee80211_ptr = wdev;
	SET_NETDEV_DEV(ndev, wiphy_dev(wdev->wiphy));
	wdev->netdev = ndev;

	init_netdev(ndev);

	vif->net_dev = ndev;
	ath6kl_init_control_info(vif);
	vif->sme_state = SME_DISCONNECTED;
	
	init_waitqueue_head(&vif->event_wq);

	setup_timer(&vif->disconnect_timer, disconnect_timer_handler,
		    (unsigned long) ndev);
	setup_timer(&vif->scan_timer, ath6kl_scan_timer_handler,
		    (unsigned long) vif);
        vif->scan_req = NULL;

	ret = register_netdevice(ndev);
               
	if (ret) {
		ath6kl_err("register_netdev failed\n");
		ath6kl_if_remove(vif);
		return ret;
	}

	set_bit(NETDEV_REGISTERED, &vif->flag);

	vif->aggr_cntxt = aggr_init(vif);
	if (!vif->aggr_cntxt) {
		ath6kl_err("failed to initialize aggr\n");
		ath6kl_if_remove(vif);
		return -1;
	}

	for (i = 0; i < AP_MAX_NUM_STA; i++) {
		vif->sta_list[i].aggr_conn_cntxt = aggr_init_conn(vif);
		if (!vif->sta_list[i].aggr_conn_cntxt) {
			ath6kl_err("failed to initialize aggr_node\n");
			ath6kl_if_remove(vif);
			return -ENOMEM;
		}
	}

	ar->vif[dev_index] = vif;
	printk("dev_index = %d, vif = %p, ar = %p\n", dev_index, vif, ar);

	if (dev_index == NUM_DEV - 1)
		vif->p2p = ar->p2p;

	if(ar->num_dev) {
		memcpy(ndev->dev_addr, ar->dev_addr , ETH_ALEN);
		ndev->dev_addr[0] = (((ndev->dev_addr[0]) ^ (1 << ar->num_dev))) | 0x02;
		ath6kl_target_config_wlan_params(vif);
		/* PS is disabled in target for p2p interface by default */
		if (vif->p2p)
			wdev->ps = NL80211_PS_DISABLED;
	}

	if (dev)
		*dev = ndev;

	ar->num_dev++;
	return ret;
}


struct ath6kl *ath6kl_core_alloc(struct device *sdev)
{
	struct ath6kl *ar;
	struct wiphy *wiphy;

	wiphy = ath6kl_init_wiphy(sdev);
	if (!wiphy)
		return NULL;

	ar = wiphy_priv(wiphy);

	ar->wiphy = wiphy;
	ar->dev = sdev;

	set_bit(WLAN_ENABLED, &ar->flag);
	clear_bit(WMI_ENABLED, &ar->flag);
	ar->wlan_pwr_state = WLAN_POWER_STATE_ON;

	spin_lock_init(&ar->lock);
	sema_init(&ar->sem, 1);
	sema_init(&ar->wmi_evt_sem, 1);
	clear_bit(DESTROY_IN_PROGRESS, &ar->flag);

	return ar;
}

int ath6kl_unavail_ev(struct ath6kl *ar)
{
	ath6kl_destroy(ar, 1);
	wiphy_unregister_ath6kl(ar->wiphy);
	wiphy_free_ath6kl(ar->wiphy);
	return 0;
}

/* firmware upload */
static u32 ath6kl_get_load_address(u32 target_ver, enum addr_type type)
{
	WARN_ON(target_ver != AR6003_REV2_VERSION &&
		target_ver != AR6003_REV3_VERSION &&
		target_ver != AR6004_REV1_VERSION &&
		target_ver != AR6004_REV2_VERSION &&
		target_ver != AR6006_REV1_VERSION);

	switch (type) {
	case DATASET_PATCH_ADDR:
		return (target_ver == AR6003_REV2_VERSION) ?
			AR6003_REV2_DATASET_PATCH_ADDRESS :
			AR6003_REV3_DATASET_PATCH_ADDRESS;
	case APP_LOAD_ADDR:
		return (target_ver == AR6003_REV2_VERSION) ?
			AR6003_REV2_APP_LOAD_ADDRESS :
			0x1234;
	case APP_START_OVERRIDE_ADDR:
		return (target_ver == AR6003_REV2_VERSION) ?
			AR6003_REV2_APP_START_OVERRIDE :
			AR6003_REV3_APP_START_OVERRIDE;
	default:
		return 0;
	}
}

static int ath6kl_get_fw(struct ath6kl *ar, const char *filename,
			 u8 **fw, size_t *fw_len)
{
	const struct firmware *fw_entry;
	int ret;

	ret = request_firmware(&fw_entry, filename, ar->dev);
	if (ret)
		return ret;

	*fw_len = fw_entry->size;
	*fw = kmemdup(fw_entry->data, fw_entry->size, GFP_KERNEL);

	if (*fw == NULL)
		ret = -ENOMEM;

	release_firmware(fw_entry);

	return ret;
}

static int ath6kl_fetch_board_file(struct ath6kl *ar)
{
	const char *filename;
	int ret;

#if 1/* CONFIG_ATH6KL_FW_ARRAY_SUPPORT */
	switch (ar->version.target_ver) {
	case AR6003_REV2_VERSION:
		printk("%s: AR6003_REV2_VERSION Not support now\n",__func__);	
		break;
	case AR6004_REV1_VERSION:
		printk("%s: AR6004_REV1_VERSION Not support now\n",__func__);	
		break;
	case AR6004_REV2_VERSION:
		printk("AR6004_REV2_VERSION bdata=%p, len=%d \n",bdata_ar6004_1_1, bdata_ar6004_1_1_size);		
		ar->fw_board = bdata_ar6004_1_1;
		ar->fw_board_len = bdata_ar6004_1_1_size;	
		break;
	case AR6006_REV1_VERSION:
		printk("%s: AR6006_REV1_VERSION Not support now\n",__func__);	
		break;
	default:
		break;
	}
	return 0;
#else
	switch (ar->version.target_ver) {
	case AR6003_REV2_VERSION:
		filename = AR6003_REV2_BOARD_DATA_FILE;
		break;
	case AR6004_REV1_VERSION:
		filename = AR6004_REV1_BOARD_DATA_FILE;
		break;
	case AR6004_REV2_VERSION:
		filename = AR6004_REV2_BOARD_DATA_FILE;
		break;
	case AR6006_REV1_VERSION:
		filename = AR6006_REV1_BOARD_DATA_FILE;
		break;
	default:
		filename = AR6003_REV3_BOARD_DATA_FILE;
		break;
	}

	ret = ath6kl_get_fw(ar, filename, &ar->fw_board,
			    &ar->fw_board_len);
	if (ret == 0) {
		/* managed to get proper board file */
		return 0;
	}

	/* there was no proper board file, try to use default instead */
	ath6kl_warn("Failed to get board file %s (%d), trying to find default board file.\n",
		    filename, ret);

	switch (ar->version.target_ver) {
	case AR6003_REV2_VERSION:
		filename = AR6003_REV2_DEFAULT_BOARD_DATA_FILE;
		break;
	case AR6004_REV1_VERSION:
		filename = AR6004_REV1_DEFAULT_BOARD_DATA_FILE;
		break;
	case AR6004_REV2_VERSION:
		filename = AR6004_REV2_DEFAULT_BOARD_DATA_FILE;
		break;
	case AR6006_REV1_VERSION:
		filename = AR6006_REV1_DEFAULT_BOARD_DATA_FILE;
		break;
	default:
		filename = AR6003_REV3_DEFAULT_BOARD_DATA_FILE;
		break;
	}

	ret = ath6kl_get_fw(ar, filename, &ar->fw_board,
			    &ar->fw_board_len);
	if (ret) {
		ath6kl_err("Failed to get default board file %s: %d\n",
			   filename, ret);
		return ret;
	}

	ath6kl_warn("WARNING! No proper board file was not found, instead using a default board file.\n");
	ath6kl_warn("Most likely your hardware won't work as specified. Install correct board file!\n");

	return 0;
#endif	
}


static int ath6kl_upload_board_file(struct ath6kl *ar)
{
	u32 board_address, board_ext_address, param;
	u32 board_data_size, board_ext_data_size;
	int ret;

	if (ar->fw_board == NULL) {
		ret = ath6kl_fetch_board_file(ar);
		if (ret)
			return ret;
	}

	/*
	 * Determine where in Target RAM to write Board Data.
	 * For AR6004, host determine Target RAM address for
	 * writing board data.
	 */
	if (ar->target_type == TARGET_TYPE_AR6004) {
		if (ar->version.target_ver == AR6004_REV1_VERSION)
			board_address = AR6004_REV1_BOARD_DATA_ADDRESS;
		else
			board_address = AR6004_REV2_BOARD_DATA_ADDRESS;
			
		ath6kl_bmi_write(ar,
				ath6kl_get_hi_item_addr(ar,
				HI_ITEM(hi_board_data)),
				(u8 *) &board_address, 4);
	} else if (ar->target_type == TARGET_TYPE_AR6006) {
		board_address = AR6006_REV1_BOARD_DATA_ADDRESS;
		ath6kl_bmi_write(ar,
				ath6kl_get_hi_item_addr(ar,
				HI_ITEM(hi_board_data)),
				(u8 *) &board_address, 4);
	} else {
		ath6kl_bmi_read(ar,
				ath6kl_get_hi_item_addr(ar,
				HI_ITEM(hi_board_data)),
				(u8 *) &board_address, 4);
	}

	ath6kl_dbg(ATH6KL_DBG_TRC, "board data download addr: 0x%x\n",
		   board_address);

	/* determine where in target ram to write extended board data */
	ath6kl_bmi_read(ar,
			ath6kl_get_hi_item_addr(ar,
			HI_ITEM(hi_board_ext_data)),
			(u8 *) &board_ext_address, 4);

	ath6kl_dbg(ATH6KL_DBG_TRC, "board file download addr: 0x%x\n",
		   board_ext_address);

	switch (ar->target_type) {
	case TARGET_TYPE_AR6003:
		board_data_size = AR6003_BOARD_DATA_SZ;
		board_ext_data_size = AR6003_BOARD_EXT_DATA_SZ;
		break;
	case TARGET_TYPE_AR6004:
		board_data_size = AR6004_BOARD_DATA_SZ;
		board_ext_data_size = AR6004_BOARD_EXT_DATA_SZ;
		break;
	case TARGET_TYPE_AR6006:
		board_data_size = AR6006_BOARD_DATA_SZ;
		board_ext_data_size = AR6006_BOARD_EXT_DATA_SZ;
		break;
	default:
		WARN_ON(1);
		return -EINVAL;
		break;
	}

	if ((board_ext_address) &&
	    (ar->fw_board_len == (board_data_size + board_ext_data_size))) {

		/* write extended board data */
		ret = ath6kl_bmi_write(ar, board_ext_address,
				       ar->fw_board + board_data_size,
				       board_ext_data_size);
		if (ret) {
			ath6kl_err("Failed to write extended board data: %d\n",
				   ret);
			return ret;
		}

		/* record that extended board data is initialized */
		param = (board_ext_data_size << 16) | 1;

		ath6kl_bmi_write(ar,
				 ath6kl_get_hi_item_addr(ar,
				 HI_ITEM(hi_board_ext_data_config)),
				 (unsigned char *) &param, 4);
	}

	if (ar->fw_board_len < board_data_size) {
		ath6kl_err("Too small board file: %zu\n", ar->fw_board_len);
		ret = -EINVAL;
		return ret;
	}

	ret = ath6kl_bmi_write(ar, board_address, ar->fw_board,
			       board_data_size);

	if (ret) {
		ath6kl_err("Board file bmi write failed: %d\n", ret);
		return ret;
	}

	/* record the fact that Board Data IS initialized */
	param = 1;
	ath6kl_bmi_write(ar,
			 ath6kl_get_hi_item_addr(ar,
			 HI_ITEM(hi_board_data_initialized)),
			 (u8 *)&param, 4);

	return ret;
}

static int ath6kl_upload_otp(struct ath6kl *ar)
{
	const char *filename;
	u32 address, param;
	int ret;

	switch (ar->version.target_ver) {
	case AR6003_REV2_VERSION:
		filename = AR6003_REV2_OTP_FILE;
		break;
	case AR6004_REV1_VERSION:
	case AR6004_REV2_VERSION:
		ath6kl_dbg(ATH6KL_DBG_TRC, "AR6004 doesn't need OTP file\n");
		return 0;
		break;
	case AR6006_REV1_VERSION:
		ath6kl_dbg(ATH6KL_DBG_TRC, "AR6006 doesn't need OTP file\n");
		return 0;
		break;
	default:
		filename = AR6003_REV3_OTP_FILE;
		break;
	}

	if (ar->fw_otp == NULL) {
		ret = ath6kl_get_fw(ar, filename, &ar->fw_otp,
				    &ar->fw_otp_len);
		if (ret) {
			ath6kl_err("Failed to get OTP file %s: %d\n",
				   filename, ret);
			return ret;
		}
	}

	address = ath6kl_get_load_address(ar->version.target_ver,
					  APP_LOAD_ADDR);

	ret = ath6kl_bmi_fast_download(ar, address, ar->fw_otp,
				       ar->fw_otp_len);
	if (ret) {
		ath6kl_err("Failed to upload OTP file: %d\n", ret);
		return ret;
	}

	/* execute the OTP code */
	param = 0;
	address = ath6kl_get_load_address(ar->version.target_ver,
					  APP_START_OVERRIDE_ADDR);
	ath6kl_bmi_execute(ar, address, &param);

	return ret;
}

static int ath6kl_upload_firmware(struct ath6kl *ar)
{
	const char *filename;
	u32 address;
	int ret;

#if 1/* CONFIG_ATH6KL_FW_ARRAY_SUPPORT */
	switch (ar->version.target_ver) {
	case AR6003_REV2_VERSION:
		printk("%s: AR6003_REV2_VERSION Not support now\n",__func__);	
		break;
	case AR6004_REV1_VERSION:
	    printk("%s: AR6004_REV1_VERSION Not support now\n",__func__);
		break;
	case AR6004_REV2_VERSION:
		printk("AR6004_REV2_VERSION fw=%p, len=%d \n",fw_ar6004_1_1, fw_ar6004_1_1_size);				
		ar->fw = fw_ar6004_1_1;
		ar->fw_len = fw_ar6004_1_1_size;	
		break;
	case AR6006_REV1_VERSION:
		printk("%s: AR6006_REV1_VERSION Not support now\n",__func__);	
		break;
	default:
		printk("%s:0x%x Not support now\n",__func__, ar->version.target_ver);	
		break;
	}
#else
	switch (ar->version.target_ver) {
	case AR6003_REV2_VERSION:
		filename = AR6003_REV2_FIRMWARE_FILE;
		break;
	case AR6004_REV1_VERSION:
		filename = AR6004_REV1_FIRMWARE_FILE;
		break;
	case AR6004_REV2_VERSION:
		if(ar->tx99 == 0) 
		    filename = AR6004_REV2_FIRMWARE_FILE;
		else
			filename = AR6004_REV2_FIRMWARE_TX99_FILE;
		break;
	case AR6006_REV1_VERSION:
		filename = AR6006_REV1_FIRMWARE_FILE;
		break;
	default:
		filename = AR6003_REV3_FIRMWARE_FILE;
		break;
	}
#endif

	if (ar->fw == NULL) {
		ret = ath6kl_get_fw(ar, filename, &ar->fw, &ar->fw_len);
		if (ret) {
			ath6kl_err("Failed to get firmware file %s: %d\n",
				   filename, ret);
			return ret;
		}
	}


	address = ath6kl_get_load_address(ar->version.target_ver,
					  APP_LOAD_ADDR);

	ret = ath6kl_bmi_fast_download(ar, address, ar->fw, ar->fw_len);

	if (ret) {
		ath6kl_err("Failed to write firmware: %d\n", ret);
		return ret;
	}

	/*
	 * Set starting address for firmware
	 * Don't need to setup app_start override addr on AR6004
	 */
	if (ar->target_type != TARGET_TYPE_AR6004 &&
		ar->target_type != TARGET_TYPE_AR6006) {
		address = ath6kl_get_load_address(ar->version.target_ver,
						  APP_START_OVERRIDE_ADDR);
		ath6kl_bmi_set_app_start(ar, address);
	}
	return ret;
}

static int ath6kl_upload_patch(struct ath6kl *ar)
{
	const char *filename;
	u32 address, param;
	int ret;

	switch (ar->version.target_ver) {
	case AR6003_REV2_VERSION:
		filename = AR6003_REV2_PATCH_FILE;
		break;
	case AR6004_REV1_VERSION:
	case AR6004_REV2_VERSION:
		/* FIXME: implement for AR6004 */
		return 0;
	case AR6006_REV1_VERSION:
		/* FIXME: implement for AR6006 */
		return 0;
		break;
	default:
		filename = AR6003_REV3_PATCH_FILE;
		break;
	}

	if (ar->fw_patch == NULL) {
		ret = ath6kl_get_fw(ar, filename, &ar->fw_patch,
				    &ar->fw_patch_len);
		if (ret) {
			ath6kl_err("Failed to get patch file %s: %d\n",
				   filename, ret);
			return ret;
		}
	}

	address = ath6kl_get_load_address(ar->version.target_ver,
					  DATASET_PATCH_ADDR);

	ret = ath6kl_bmi_write(ar, address, ar->fw_patch, ar->fw_patch_len);
	if (ret) {
		ath6kl_err("Failed to write patch file: %d\n", ret);
		return ret;
	}

	param = address;
	ath6kl_bmi_write(ar,
			 ath6kl_get_hi_item_addr(ar,
			 HI_ITEM(hi_dset_list_head)),
			 (unsigned char *) &param, 4);

	return 0;
}

static int ath6kl_init_upload(struct ath6kl *ar)
{
	u32 param, options, sleep, address;
	int status = 0;

	if (ar->target_type != TARGET_TYPE_AR6003 &&
		ar->target_type != TARGET_TYPE_AR6004 &&
		ar->target_type != TARGET_TYPE_AR6006)
		return -EINVAL;

	/* temporarily disable system sleep */
	address = MBOX_BASE_ADDRESS + LOCAL_SCRATCH_ADDRESS;
	status = ath6kl_bmi_reg_read(ar, address, &param);
	if (status)
		return status;

	options = param;

	param |= ATH6KL_OPTION_SLEEP_DISABLE;
	status = ath6kl_bmi_reg_write(ar, address, param);
	if (status)
		return status;

	address = RTC_BASE_ADDRESS + SYSTEM_SLEEP_ADDRESS;
	status = ath6kl_bmi_reg_read(ar, address, &param);
	if (status)
		return status;

	sleep = param;

	param |= SM(SYSTEM_SLEEP_DISABLE, 1);
	status = ath6kl_bmi_reg_write(ar, address, param);
	if (status)
		return status;

	ath6kl_dbg(ATH6KL_DBG_TRC, "old options: %d, old sleep: %d\n",
		   options, sleep);

	/* program analog PLL register */
	/* no need to control 40/44MHz clock on AR6004 */
	if (ar->target_type != TARGET_TYPE_AR6004 &&
		ar->target_type != TARGET_TYPE_AR6006) {
		status = ath6kl_bmi_reg_write(ar, ATH6KL_ANALOG_PLL_REGISTER,
					      0xF9104001);

		if (status)
			return status;

		/* Run at 80/88MHz by default */
		param = SM(CPU_CLOCK_STANDARD, 1);

		address = RTC_BASE_ADDRESS + CPU_CLOCK_ADDRESS;
		status = ath6kl_bmi_reg_write(ar, address, param);
		if (status)
			return status;
	}

	if (debug_fpga) {
		param = 0;
		address = RTC_BASE_ADDRESS + CPU_CLOCK_ADDRESS;
		status = ath6kl_bmi_reg_write(ar, address, param);
		if (status)
			return status;
		printk("%s(%d)-Run at 40/44MHz by default\n", __FUNCTION__, __LINE__);
	}

	if(ath6kl_wow_ext) {
		/* FIXME:
		 *  - adjust the size of wow ext param 
		 *  - configure gpio trigger in runtime, '0' is disabled
		 */
		param = 0x80000000 |(ath6kl_wow_gpio<<18);
		status = ath6kl_bmi_write(ar,
				     ath6kl_get_hi_item_addr(ar,
				     HI_ITEM(hi_wow_ext_config)),
				     (u8 *)&param, 4);

		WARN_ON(status);
		ath6kl_dbg(ATH6KL_DBG_WOWLAN, "Enable wow extension with gpio#%d, param: 0x%08x\n", 
			ath6kl_wow_gpio, param);
	}

	if (ath6kl_lpl_attach) {
		status = ath6kl_bmi_reg_read(ar,
					ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_pwr_save_flags)),
					&param);

		if (status) {
			ath6kl_dbg(ATH6KL_DBG_PM, "Unable to read power save flags\n");
			return status;
		}

		/* change the byte order */
		param = le32_to_cpu(param);
		param |= HI_PWR_SAVE_LPL_ENABLED;
		param = cpu_to_le32(param);

		status = ath6kl_bmi_write(ar,
					ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_pwr_save_flags)),
					(u8 *)&param, sizeof(param));

		WARN_ON(status);
		ath6kl_dbg(ATH6KL_DBG_PM, "attach LPL module\n");
	}

	if (ath6kl_mimo_ps_enable) {
		status = ath6kl_bmi_reg_read(ar,
					ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_smps_options)),
					&param);

		if (status) {
			ath6kl_dbg(ATH6KL_DBG_PM, "Unable to read smps flags\n");
			return status;
		}

		/* change the byte order */
		param = le32_to_cpu(param);
		param |= HI_SMPS_ALLOW_MASK;
		param = cpu_to_le32(param);

		status = ath6kl_bmi_write(ar,
					ath6kl_get_hi_item_addr(ar, HI_ITEM(hi_smps_options)),
					(u8 *)&param, sizeof(param));

		WARN_ON(status);
		ath6kl_dbg(ATH6KL_DBG_PM, "Enable MIMO PS support\n");
	}

	if (ar->target_type == TARGET_TYPE_AR6004 ||
		ar->target_type == TARGET_TYPE_AR6006) {

		param = refclk_hz*1000000;
		status = ath6kl_bmi_write(ar,
				     ath6kl_get_hi_item_addr(ar,
				     HI_ITEM(hi_refclk_hz)),
				     (u8 *)&param, 4);
	}

	param = 0;
	address = RTC_BASE_ADDRESS + LPO_CAL_ADDRESS;
	param = SM(LPO_CAL_ENABLE, 1);
	status = ath6kl_bmi_reg_write(ar, address, param);
	if (status)
		return status;

	/* WAR to avoid SDIO CRC err */
	if (ar->version.target_ver == AR6003_REV2_VERSION) {
		ath6kl_err("temporary war to avoid sdio crc error\n");

		param = 0x20;

		address = GPIO_BASE_ADDRESS + GPIO_PIN10_ADDRESS;
		status = ath6kl_bmi_reg_write(ar, address, param);
		if (status)
			return status;

		address = GPIO_BASE_ADDRESS + GPIO_PIN11_ADDRESS;
		status = ath6kl_bmi_reg_write(ar, address, param);
		if (status)
			return status;

		address = GPIO_BASE_ADDRESS + GPIO_PIN12_ADDRESS;
		status = ath6kl_bmi_reg_write(ar, address, param);
		if (status)
			return status;

		address = GPIO_BASE_ADDRESS + GPIO_PIN13_ADDRESS;
		status = ath6kl_bmi_reg_write(ar, address, param);
		if (status)
			return status;
	}

	if (!debug_fpga) {
		/* write EEPROM data to Target RAM */
		status = ath6kl_upload_board_file(ar);
		if (status)
			return status;

		/* transfer One time Programmable data */
		status = ath6kl_upload_otp(ar);
		if (status)
		{
		    printk(KERN_ERR "%s %s %d\n",__FILE__,__FUNCTION__,__LINE__);    
			return status;
		}

		/* Download Target firmware */
		status = ath6kl_upload_firmware(ar);
		if (status)
		{
		    printk(KERN_ERR "%s %s %d\n",__FILE__,__FUNCTION__,__LINE__);    
			return status;
		}

		status = ath6kl_upload_patch(ar);
		if (status)
			return status;
	} else {

        if (ar->target_type == TARGET_TYPE_AR6006) {
            if (debug_fpga_rom512) {
                param = 0x465400; //512 ROM + 256 DATA RAM
            } else {
                param = 0x44d400; //640 ROM + 160 DATA RAM
            }
        } else {    //AR6004
            param = 0x43d400;
        } 
		if (ath6kl_bmi_write(ar,
				     ath6kl_get_hi_item_addr(ar,
				     HI_ITEM(hi_board_data)),
				     (u8 *)&param, 4) != 0) {
			ath6kl_err("bmi_write_memory for hi_board_data failed\n");
			return -EIO;
		}
	    printk("%s(%d)-Set boarddata addr:0x%x.\n", __FUNCTION__, __LINE__, param);

		param = 0x0;
		if (ath6kl_bmi_write(ar,
				     ath6kl_get_hi_item_addr(ar,
				     HI_ITEM(hi_board_data_initialized)),
				     (u8 *)&param, 4) != 0) {
			ath6kl_err("bmi_write_memory for hi_board_data_initialized failed\n");
			return -EIO;
		}
	    printk("%s(%d)-Set boarddata init:0x%x.\n", __FUNCTION__, __LINE__, param);

		param = 11264;
		if (ath6kl_bmi_write(ar,
				     ath6kl_get_hi_item_addr(ar,
				     HI_ITEM(hi_end_ram_reserve_sz)),
				     (u8 *)&param, 4) != 0) {
			ath6kl_err("bmi_write_memory for hi_end_ram_reserve_sz failed\n");
			return -EIO;
		}
	    printk("%s(%d)-Set end ram reserve size:%d.\n", __FUNCTION__, __LINE__, param);

		param = 0x8;
		if (ath6kl_bmi_write(ar,
				     ath6kl_get_hi_item_addr(ar,
				     HI_ITEM(hi_option_flag2)),
				     (u8 *)&param, 4) != 0) {
			ath6kl_err("bmi_write_memory for hi_option_flag2 failed\n");
			return -EIO;
		}
	    printk("%s(%d)-Disable radio retension.\n", __FUNCTION__, __LINE__);

		/* Download Target firmware */
		status = ath6kl_upload_firmware(ar);
		if (status)
			return status;
	} //debug_fpga

	/* Restore system sleep */
	address = RTC_BASE_ADDRESS + SYSTEM_SLEEP_ADDRESS;
	status = ath6kl_bmi_reg_write(ar, address, sleep);
	if (status)
		return status;

	address = MBOX_BASE_ADDRESS + LOCAL_SCRATCH_ADDRESS;
	if (!ani || ar->version.target_ver != AR6004_REV2_VERSION) {
		param = options | 0x20;
	} else {
		param = options;
	}
	status = ath6kl_bmi_reg_write(ar, address, param);
	if (status)
		return status;

	/* Configure GPIO AR6003 UART */
	if (!debug_fpga) {
		if (ar->target_type == TARGET_TYPE_AR6004) {
	  		param = CONFIG_AR6004_DEBUG_UART_TX_PIN;
		} else if (ar->target_type == TARGET_TYPE_AR6006) {
	  		param = CONFIG_AR6006_DEBUG_UART_TX_PIN;
	    } else {
	  		param = CONFIG_AR600x_DEBUG_UART_TX_PIN;
	    }
		status = ath6kl_bmi_write(ar,
					  ath6kl_get_hi_item_addr(ar,
					  HI_ITEM(hi_dbg_uart_txpin)),
					  (u8 *)&param, 4);
	}

	return status;
}

static int ath6kl_init(struct ath6kl *ar)
{
	int status = 0;
	s32 timeleft;
	int i;

	if (!ar)
		return -EIO;

	INIT_LIST_HEAD(&ar->amsdu_rx_buffer_queue);

	/* Do we need to finish the BMI phase */
	if (ath6kl_bmi_done(ar)) {
		status = -EIO;
		goto ath6kl_init_done;
	}

	/* Indicate that WMI is enabled (although not ready yet) */
	set_bit(WMI_ENABLED, &ar->flag);
	ar->wmi = ath6kl_wmi_init(ar);
	if (!ar->wmi) {
		ath6kl_err("failed to initialize wmi\n");
		status = -EIO;
		goto ath6kl_init_done;
	}

	ath6kl_dbg(ATH6KL_DBG_TRC, "%s: got wmi @ 0x%p.\n", __func__, ar->wmi);

	/*
	 * The reason we have to wait for the target here is that the
	 * driver layer has to init BMI in order to set the host block
	 * size.
	 */
	if (ath6kl_htc_wait_target(ar->htc_target)) {
		status = -EIO;
		goto err_node_cleanup;
	}

	if (ath6kl_init_service_ep(ar)) {
		status = -EIO;
		goto err_cleanup_scatter;
	}

	/* setup access class priority mappings */
	ar->ac_stream_pri_map[WMM_AC_BK] = 0; /* lowest  */
	ar->ac_stream_pri_map[WMM_AC_BE] = 1;
	ar->ac_stream_pri_map[WMM_AC_VI] = 2;
	ar->ac_stream_pri_map[WMM_AC_VO] = 3; /* highest */

	/* give our connected endpoints some buffers */
	ath6kl_rx_refill(ar->htc_target, ar->ctrl_ep);
	ath6kl_rx_refill(ar->htc_target, ar->ac2ep_map[WMM_AC_BE]);

	/* allocate some buffers that handle larger AMSDU frames */
	ath6kl_refill_amsdu_rxbufs(ar, ATH6KL_MAX_AMSDU_RX_BUFFERS);

	/* setup credit distribution */
	ath6k_setup_credit_dist(ar->htc_target, &ar->credit_state_info);

	ath6kl_cookie_init(ar);

	/* start HTC */
	status = ath6kl_htc_start(ar->htc_target);

	if (status) {
		ath6kl_cookie_cleanup(ar);
		goto err_rxbuf_cleanup;
	}

	/* Wait for Wmi event to be ready */
	timeleft = wait_event_interruptible_timeout(ar->vif[0]->event_wq,
				test_bit(WMI_READY, &ar->flag), WMI_TIMEOUT);

	if (ar->version.abi_ver != ATH6KL_ABI_VERSION) {
		ath6kl_err("abi version mismatch: host(0x%x), target(0x%x)\n",
			   ATH6KL_ABI_VERSION, ar->version.abi_ver);
		status = -EIO;
		goto err_htc_stop;
	}

	if (!timeleft || signal_pending(current)) {
		ath6kl_err("wmi is not ready or wait was interrupted\n");
		status = -EIO;
		goto err_htc_stop;
	}

	ath6kl_dbg(ATH6KL_DBG_TRC, "%s: wmi is ready\n", __func__);

	/* communicate the wmi protocol verision to the target */
	if ((ath6kl_set_host_app_area(ar)) != 0)
		ath6kl_err("unable to set the host app area\n");

	ar->conf_flags = ATH6KL_CONF_IGNORE_ERP_BARKER |
			 ATH6KL_CONF_ENABLE_11N | ATH6KL_CONF_ENABLE_TX_BURST;


	for( i = 0; i < ar->num_dev; i++) {
		status = ath6kl_target_config_wlan_params(ar->vif[i]);
	}

	if (!status)
		goto ath6kl_init_done;

err_htc_stop:
	ath6kl_htc_stop(ar->htc_target);
err_rxbuf_cleanup:
	ath6kl_htc_flush_rx_buf(ar->htc_target);
	ath6kl_cleanup_amsdu_rxbufs(ar);
err_cleanup_scatter:
	ath6kl_hif_cleanup_scatter(ar);
err_node_cleanup:
	ath6kl_wmi_shutdown(ar->wmi);
	clear_bit(WMI_ENABLED, &ar->flag);
	ar->wmi = NULL;

ath6kl_init_done:
	return status;
}

int ath6kl_core_init(struct ath6kl *ar)
{
	int ret = 0;
	struct ath6kl_bmi_target_info targ_info;

	ar->ath6kl_wq = create_singlethread_workqueue("ath6kl");
	if (!ar->ath6kl_wq)
		return -ENOMEM;

    ret = ath6kl_bmi_init(ar);
	if (ret)
		goto err_wq;

    ret = ath6kl_bmi_get_target_info(ar, &targ_info);
	if (ret)
		goto err_bmi_cleanup;

    ar->version.target_ver = le32_to_cpu(targ_info.version);
	ar->target_type = le32_to_cpu(targ_info.type);
	ar->wiphy->hw_version = le32_to_cpu(targ_info.version);

    ret = ath6kl_configure_target(ar);
	if (ret)
		goto err_bmi_cleanup;

    ar->htc_target = ath6kl_htc_create(ar);

	if (!ar->htc_target) {
		ret = -ENOMEM;
		goto err_bmi_cleanup;
	}

    ret = ath6kl_init_upload(ar);
	if (ret)
		goto err_htc_cleanup;

    if (reg_domain != 0xffff) {
		ret = ath6kl_set_rd(ar);
		if (ret)
			goto err_htc_cleanup;
	}

	rtnl_lock();
	ret = ath6kl_if_add( ar, NULL, NULL, NL80211_IFTYPE_STATION, 0);
	rtnl_unlock();
	
	if (ret)
		goto err_htc_cleanup;

	if (ath6kl_debug_init(ar)) {
		ath6kl_err("Failed to initialize debugfs\n");
		goto err_iface_cleanup;
	}
    
	ret = ath6kl_init(ar);
	if (ret)
		goto err_iface_cleanup;	

	return ret;

err_iface_cleanup:
	ath6kl_if_remove(ar->vif[0]);    
err_htc_cleanup:
	ath6kl_htc_cleanup(ar->htc_target);
err_bmi_cleanup:
	ath6kl_bmi_cleanup(ar);
err_wq:
	destroy_workqueue(ar->ath6kl_wq);
	return ret;
}

void ath6kl_reset_device(struct ath6kl *ar, u32 target_type,
                                bool wait_fot_compltn, bool cold_reset);

void ath6kl_stop_txrx(struct ath6kl *ar)
{
	int i;
	struct net_device *ndev;

	if (down_interruptible(&ar->sem)) {
		ath6kl_err("down_interruptible failed\n");
		return;
	}

	set_bit(DESTROY_IN_PROGRESS, &ar->flag);

	if (test_bit(WMI_READY, &ar->flag)) {
		clear_bit(WMI_READY, &ar->flag);
		ath6kl_wmi_shutdown(ar->wmi);
		clear_bit(WMI_ENABLED, &ar->flag);
		ar->wmi = NULL;
	} else {
		ath6kl_dbg(ATH6KL_DBG_TRC,
			   "%s: wmi is not ready 0x%p 0x%p\n",
			   __func__, ar, ar->wmi);

		/* Shut down WMI if we have started it */
		if (test_bit(WMI_ENABLED, &ar->flag)) {
			ath6kl_dbg(ATH6KL_DBG_TRC,
				   "%s: shut down wmi\n", __func__);
			ath6kl_wmi_shutdown(ar->wmi);
			clear_bit(WMI_ENABLED, &ar->flag);
			ar->wmi = NULL;
		}
	}

	for (i = 0; i < ar->num_dev; i++) {

		ndev = ar->vif[i]->net_dev;
		if (!ndev)
			continue;

		if (ar->wlan_pwr_state != WLAN_POWER_STATE_CUT_PWR)
			ath6kl_stop_endpoint(ndev, false, true);
	}

	if (ar->htc_target) {
		ath6kl_dbg(ATH6KL_DBG_TRC, "%s: shut down htc\n", __func__);
		ath6kl_htc_stop(ar->htc_target);
	}

	/*
	 * Try to reset the device if we can. The driver may have been
	 * configure NOT to reset the target during a debug session.
	 */
	ath6kl_dbg(ATH6KL_DBG_TRC,
		   "attempting to reset target on instance destroy\n");
	ath6kl_reset_device(ar, ar->target_type, true, true);

	clear_bit(WLAN_ENABLED, &ar->flag);
}

/*
 * We need to differentiate between the surprise and planned removal of the
 * device because of the following consideration:
 *
 * - In case of surprise removal, the hcd already frees up the pending
 *   for the device and hence there is no need to unregister the function
 *   driver inorder to get these requests. For planned removal, the function
 *   driver has to explicitly unregister itself to have the hcd return all the
 *   pending requests before the data structures for the devices are freed up.
 *   Note that as per the current implementation, the function driver will
 *   end up releasing all the devices since there is no API to selectively
 *   release a particular device.
 *
 * - Certain commands issued to the target can be skipped for surprise
 *   removal since they will anyway not go through.
 */
void ath6kl_destroy(struct ath6kl *ar, unsigned int unregister)
{
	struct ath6kl_vif *vif;
	int i;

	if (!ar) {
		ath6kl_err("failed to get device structure\n");
		return;
	}


	for ( i = 0; i < ar->num_dev; i ++)
	{
		vif = ar->vif[i];
		ath6kl_if_remove(vif);
	}

	ath6kl_debug_cleanup(ar);

	if (ar->htc_target)
		ath6kl_htc_cleanup(ar->htc_target);

	destroy_workqueue(ar->ath6kl_wq);
	ath6kl_cookie_cleanup(ar);
	ath6kl_bmi_cleanup(ar);
	ath6kl_cleanup_amsdu_rxbufs(ar);

#if 1/* CONFIG_ATH6KL_FW_ARRAY_SUPPORT */
	ar->fw_board = NULL;
	ar->fw_otp = NULL;
	ar->fw = NULL;
	ar->fw_patch = NULL;
#else
	kfree(ar->fw_board);
	kfree(ar->fw_otp);
	kfree(ar->fw);
	kfree(ar->fw_patch);
#endif	
}
