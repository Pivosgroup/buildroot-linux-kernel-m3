/*
 * Copyright (c) 2010-2011 Atheros Communications Inc.
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

#ifndef CORE_H
#define CORE_H

#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/firmware.h>
#include <linux/sched.h>
#include <linux/circ_buf.h>
#include <linux/ip.h>
#include <net/cfg80211.h>
#include "htc.h"
#include "wmi.h"
#include "bmi.h"
#include "target.h"
#include "dbglog.h"

#define MAKE_STR(symbol) #symbol
#define TO_STR(symbol) MAKE_STR(symbol)

/* The script (used for release builds) modifies the following line. */
#define __BUILD_VERSION_ 3.2.1.9999

/* Always turn on ATH6KL_DEBUG */
#undef CONFIG_ATH6KL_DEBUG 
#define CONFIG_ATH6KL_DEBUG 1

#define DRV_VERSION		TO_STR(__BUILD_VERSION_)

#define MAX_ATH6KL                        1
#define ATH6KL_MAX_RX_BUFFERS             16
#define ATH6KL_BUFFER_SIZE                1664
#define ATH6KL_MAX_AMSDU_RX_BUFFERS       4
#define ATH6KL_AMSDU_REFILL_THRESHOLD     3
#define ATH6KL_AMSDU_BUFFER_SIZE     (WMI_MAX_AMSDU_RX_DATA_FRAME_LENGTH + 128)
#define MAX_MSDU_SUBFRAME_PAYLOAD_LEN	1508
#define MIN_MSDU_SUBFRAME_PAYLOAD_LEN	46

#define USER_SAVEDKEYS_STAT_INIT     0
#define USER_SAVEDKEYS_STAT_RUN      1

#define ATH6KL_TX_TIMEOUT      10
#define ATH6KL_MAX_ENDPOINTS   4
#define MAX_NODE_NUM           15

/* Extra bytes for htc header alignment */
#define ATH6KL_HTC_ALIGN_BYTES 3

/* MAX_HI_COOKIE_NUM are reserved for high priority traffic */
#define MAX_DEF_COOKIE_NUM                180
#define MAX_HI_COOKIE_NUM                 18	/* 10% of MAX_COOKIE_NUM */
#define MAX_COOKIE_NUM                 (MAX_DEF_COOKIE_NUM + MAX_HI_COOKIE_NUM)

#define MAX_DEFAULT_SEND_QUEUE_DEPTH      (MAX_DEF_COOKIE_NUM / WMM_NUM_AC)

#define DISCON_TIMER_INTVAL               10000  /* in msec */
#define A_DEFAULT_LISTEN_INTERVAL         100
#define A_MAX_WOW_LISTEN_INTERVAL         1000

#define SKB_CB_VIF(__skb)	(*((struct ath6kl_vif **)__skb->cb))
/* AR6003 1.0 definitions */
#define AR6003_REV1_VERSION                 0x300002ba

/* AR6003 2.0 definitions */
#define AR6003_REV2_VERSION                 0x30000384
#define AR6003_REV2_PATCH_DOWNLOAD_ADDRESS  0x57e910
#define AR6003_REV2_OTP_FILE                "ath6k/AR6003/hw2.0/otp.bin.z77"
#define AR6003_REV2_FIRMWARE_FILE           "ath6k/AR6003/hw2.0/athwlan.bin.z77"
#define AR6003_REV2_PATCH_FILE              "ath6k/AR6003/hw2.0/data.patch.bin"
#define AR6003_REV2_BOARD_DATA_FILE         "ath6k/AR6003/hw2.0/bdata.bin"
#define AR6003_REV2_DEFAULT_BOARD_DATA_FILE "ath6k/AR6003/hw2.0/bdata.SD31.bin"

/* AR6003 3.0 definitions */
#define AR6003_REV3_VERSION                 0x30000582
#define AR6003_REV3_OTP_FILE                "ath6k/AR6003/hw2.1.1/otp.bin"
#define AR6003_REV3_FIRMWARE_FILE           "ath6k/AR6003/hw2.1.1/athwlan.bin"
#define AR6003_REV3_PATCH_FILE            "ath6k/AR6003/hw2.1.1/data.patch.bin"
#define AR6003_REV3_BOARD_DATA_FILE       "ath6k/AR6003/hw2.1.1/bdata.bin"
#define AR6003_REV3_DEFAULT_BOARD_DATA_FILE	\
	"ath6k/AR6003/hw2.1.1/bdata.SD31.bin"

/* AR6004 1.0 definitions */
#define AR6004_REV1_VERSION                 0x30000623
#define AR6004_REV1_FIRMWARE_FILE           "ath6k/AR6004/hw1.0/fw.ram.bin"
#define AR6004_REV1_BOARD_DATA_FILE         "ath6k/AR6004/hw1.0/bdata.bin"
#define AR6004_REV1_DEFAULT_BOARD_DATA_FILE "ath6k/AR6004/hw1.0/bdata.DB132.bin"
#define AR6004_REV1_EPPING_FIRMWARE_FILE "ath6k/AR6004/hw1.0/endpointping.bin"

/* AR6004 1.1 definitions */
#define AR6004_REV2_VERSION                 0x30000001
#define AR6004_REV2_FIRMWARE_FILE           "ath6k/AR6004/hw1.1/fw.ram.bin"
#define AR6004_REV2_FIRMWARE_TX99_FILE           "ath6k/AR6004/hw1.1/fw.ram.tx99.bin"

#define AR6004_REV2_BOARD_DATA_FILE         "ath6k/AR6004/hw1.1/bdata.bin"
#define AR6004_REV2_DEFAULT_BOARD_DATA_FILE "ath6k/AR6004/hw1.1/bdata.DB132.bin"
#define AR6004_REV2_EPPING_FIRMWARE_FILE "ath6k/AR6004/hw1.1/endpointping.bin"

/* AR6006 1.0 definitions */
#define AR6006_REV1_VERSION                 0x300007a5
#define AR6006_REV1_FIRMWARE_FILE           "ath6k/AR6006/hw1.0/fw.ram.bin"
#define AR6006_REV1_BOARD_DATA_FILE         "ath6k/AR6006/hw1.0/bdata.bin"
#define AR6006_REV1_DEFAULT_BOARD_DATA_FILE "ath6k/AR6006/hw1.0/bdata.DB132.bin"
#define AR6006_REV1_EPPING_FIRMWARE_FILE    "ath6k/AR6006/hw1.0/endpointping.bin"

/* Per STA data, used in AP mode */
#define STA_PS_AWAKE		BIT(0)
#define	STA_PS_SLEEP		BIT(1)
#define	STA_PS_POLLED		BIT(2)
#define STA_PS_APSD_TRIGGER     BIT(3)
#define STA_PS_APSD_EOSP        BIT(4)

/* HTC TX packet tagging definitions */
#define ATH6KL_CONTROL_PKT_TAG              HTC_TX_PACKET_TAG_USER_DEFINED
#define ATH6KL_DATA_PKT_TAG                 (ATH6KL_CONTROL_PKT_TAG + 1)
#define ATH6KL_CONTROL_PKT_SYNC_TAG         (ATH6KL_CONTROL_PKT_TAG + 2)

#define AR6003_CUST_DATA_SIZE 16

#define AGGR_WIN_IDX(x, y)          ((x) % (y))
#define AGGR_INCR_IDX(x, y)         AGGR_WIN_IDX(((x) + 1), (y))
#define AGGR_DCRM_IDX(x, y)         AGGR_WIN_IDX(((x) - 1), (y))
#define ATH6KL_MAX_SEQ_NO		0xFFF
#define ATH6KL_NEXT_SEQ_NO(x)		(((x) + 1) & ATH6KL_MAX_SEQ_NO)

#define NUM_OF_TIDS         8
#define AGGR_SZ_DEFAULT     8

#define AGGR_WIN_SZ_MIN     2
#define AGGR_WIN_SZ_MAX     8

#define TID_WINDOW_SZ(_x)   ((_x) << 1)

#define AGGR_NUM_OF_FREE_NETBUFS    16

#define AGGR_RX_TIMEOUT          50	/* in ms */
#define AGGR_RX_TIMEOUT_SHORT    10  /* Triger inside timeout trigger, assume bad condition. reduced timeout */

#define AGGR_GET_RXTID_STATS(_p, _x)     (&(_p->stat[(_x)]))
#define AGGR_GET_RXTID(_p, _x)           (&(_p->rx_tid[(_x)]))

#define AGGR_BA_EVT_GET_CONNID(_conn)    ((_conn) >> 4)
#define AGGR_BA_EVT_GET_TID(_tid)        ((_tid) & 0xF)

#define AGGR_TX_MAX_AGGR_SIZE   1600	/* Sync to max. PDU size of host size. */
#define AGGR_TX_MAX_PDU_SIZE    120
#define AGGR_TX_MIN_PDU_SIZE    64		/* 802.3(14) + LLC(8) + IP/TCP(20) = 42 */
#define AGGR_TX_MAX_NUM			6
#define AGGR_TX_TIMEOUT         4

#define AGGR_GET_TXTID(_p, _x)           (&(_p->tx_tid[(_x)]))

#define WMI_TIMEOUT (2 * HZ)

#define MBOX_YIELD_LIMIT 99

#define ATH6KL_SCAN_TIMEOUT (29 * HZ)  /* less than timeout value of supplicant */

/* configuration lags */
/*
 * ATH6KL_CONF_IGNORE_ERP_BARKER: Ignore the barker premable in
 * ERP IE of beacon to determine the short premable support when
 * sending (Re)Assoc req.
 * ATH6KL_CONF_IGNORE_PS_FAIL_EVT_IN_SCAN: Don't send the power
 * module state transition failure events which happen during
 * scan, to the host.
 */
#define ATH6KL_CONF_IGNORE_ERP_BARKER		BIT(0)
#define ATH6KL_CONF_IGNORE_PS_FAIL_EVT_IN_SCAN  BIT(1)
#define ATH6KL_CONF_ENABLE_11N			BIT(2)
#define ATH6KL_CONF_ENABLE_TX_BURST		BIT(3)

enum wlan_spatial_stream_state {
	WLAN_SPATIAL_STREAM_11,
	WLAN_SPATIAL_STREAM_22,
};

enum wlan_low_pwr_state {
	WLAN_POWER_STATE_ON,
	WLAN_POWER_STATE_CUT_PWR,
	WLAN_POWER_STATE_DEEP_SLEEP,
	WLAN_POWER_STATE_WOW
};

enum sme_state {
	SME_DISCONNECTED,
	SME_CONNECTING,
	SME_CONNECTED
};

struct skb_hold_q {
	struct sk_buff *skb;
	bool is_amsdu;
	u16 seq_no;
};

struct rxtid {
	bool aggr;
	bool progress;
	bool timer_mon;
	u16 win_sz;
	u16 seq_next;
	u32 hold_q_sz;
	struct skb_hold_q *hold_q;
	struct sk_buff_head q;
	spinlock_t lock;
    u16 timerwait_seq_num; /* current wait seq_no next */
};

struct rxtid_stats {
	u32 num_into_aggr;
	u32 num_dups;
	u32 num_oow;
	u32 num_mpdu;
	u32 num_amsdu;
	u32 num_delivered;
	u32 num_timeouts;
	u32 num_hole;
	u32 num_bar;
};

enum {
	AGGR_TX_OK = 0,
	AGGR_TX_DONE,
	AGGR_TX_BYPASS,
	AGGR_TX_DROP,
	AGGR_TX_UNKNOW,
};

struct txtid {
	u8 tid;
	u16 aid;
	u16 max_aggr_sz;			/* 0 means disable */
	struct timer_list timer;
	struct sk_buff *amsdu_skb;
	u8 amsdu_cnt;				/* current aggr count */
	u8 *amsdu_start;			/* start pointer of amsdu frame */
	u16 amsdu_len;				/* current aggr length */
	u16 amsdu_lastpdu_len;		/* last PDU length */
	spinlock_t lock;
	struct ath6kl_vif *vif;

	u32 num_pdu;
	u32 num_amsdu;
	u32 num_timeout;
	u32 num_flush;
	u32 num_tx_null;
};

struct aggr_info {
	struct ath6kl_vif *vif;
	struct sk_buff_head free_q;

	/* TX A-MSDU */
	bool tx_amsdu_enable;		/* IOT : treat A-MPDU & A-MSDU are exclusive. */
	bool tx_amsdu_seq_pkt;
	u8 tx_amsdu_max_aggr_num;
	u16 tx_amsdu_max_pdu_len;
	u16 tx_amsdu_timeout; 		/* in ms */
};

struct aggr_conn_info {
	u8 aggr_sz;
	u8 timer_scheduled;
	struct timer_list timer;
	struct aggr_info *aggr_cntxt;
	struct net_device *dev;
	struct rxtid rx_tid[NUM_OF_TIDS];
	struct rxtid_stats stat[NUM_OF_TIDS];

	/* TX A-MSDU */
	struct txtid tx_tid[NUM_OF_TIDS];
};

struct ath6kl_wep_key {
	u8 key_index;
	u8 key_len;
	u8 key[64];
};

#define ATH6KL_KEY_SEQ_LEN 8

struct ath6kl_key {
	u8 key[WLAN_MAX_KEY_LEN];
	u8 key_len;
	u8 seq[ATH6KL_KEY_SEQ_LEN];
	u8 seq_len;
	u32 cipher;
};

struct ath6kl_node_mapping {
	u8 mac_addr[ETH_ALEN];
	u8 ep_id;
	u8 tx_pend;
};

struct ath6kl_cookie {
	struct sk_buff *skb;
	u32 map_no;
	struct htc_packet htc_pkt;
	struct ath6kl_cookie *arc_list_next;
};


struct mgmt_buff {
	struct list_head list;
	u32 freq;
	u32 wait;
	u32 id;
	u32 age;
	size_t len;
	u8 buf[1];
};


struct mgmt_buff_head {
	struct list_head list;
	spinlock_t lock;
	u32 len;
};

struct ath6kl_sta {
	u16 sta_flags;
	u8 mac[ETH_ALEN];
	u8 aid;
	u8 keymgmt;
	u8 ucipher;
	u8 auth;
	u8 wpa_ie[ATH6KL_MAX_IE];
	spinlock_t lock;
	struct sk_buff_head psq;
	struct mgmt_buff_head mgmt_psq;
	u8 apsd_info;
	struct ath6kl_vif *vif;
	struct timer_list psq_age_timer;
	struct aggr_conn_info *aggr_conn_cntxt;	
};

struct ath6kl_version {
	u32 target_ver;
	u32 wlan_ver;
	u32 abi_ver;
};

struct ath6kl_bmi {
	u32 cmd_credits;
	bool done_sent;
	u8 *cmd_buf;
};

struct target_stats {
	u64 tx_pkt;
	u64 tx_byte;
	u64 tx_ucast_pkt;
	u64 tx_ucast_byte;
	u64 tx_mcast_pkt;
	u64 tx_mcast_byte;
	u64 tx_bcast_pkt;
	u64 tx_bcast_byte;
	u64 tx_rts_success_cnt;
	u64 tx_pkt_per_ac[4];

	u64 tx_err;
	u64 tx_fail_cnt;
	u64 tx_retry_cnt;
	u64 tx_mult_retry_cnt;
	u64 tx_rts_fail_cnt;

	u64 rx_pkt;
	u64 rx_byte;
	u64 rx_ucast_pkt;
	u64 rx_ucast_byte;
	u64 rx_mcast_pkt;
	u64 rx_mcast_byte;
	u64 rx_bcast_pkt;
	u64 rx_bcast_byte;
	u64 rx_frgment_pkt;

	u64 rx_err;
	u64 rx_crc_err;
	u64 rx_key_cache_miss;
	u64 rx_decrypt_err;
	u64 rx_dupl_frame;

	u64 tkip_local_mic_fail;
	u64 tkip_cnter_measures_invoked;
	u64 tkip_replays;
	u64 tkip_fmt_err;
	u64 ccmp_fmt_err;
	u64 ccmp_replays;

	u64 pwr_save_fail_cnt;

	u64 cs_bmiss_cnt;
	u64 cs_low_rssi_cnt;
	u64 cs_connect_cnt;
	u64 cs_discon_cnt;

	s32 tx_ucast_rate;
	u8 tx_sgi;
	u8 tx_mcs;
	s32 rx_ucast_rate;
	u8 rx_sgi;
	u8 rx_mcs;

	u32 lq_val;

	u32 wow_pkt_dropped;
	u16 wow_evt_discarded;

	s16 noise_floor_calib;
	s16 cs_rssi;
	s16 cs_ave_beacon_rssi;
	u8 cs_ave_beacon_snr;
	u8 cs_last_roam_msec;
	u8 cs_snr;

	u8 wow_host_pkt_wakeups;
	u8 wow_host_evt_wakeups;

	u32 arp_received;
	u32 arp_matched;
	u32 arp_replied;
};

struct ath6kl_mbox_info {
	u32 htc_addr;
	u32 htc_ext_addr;
	u32 htc_ext_sz;

	u32 block_size;

	u32 gmbox_addr;

	u32 gmbox_sz;
};

/*
 * 802.11i defines an extended IV for use with non-WEP ciphers.
 * When the EXTIV bit is set in the key id byte an additional
 * 4 bytes immediately follow the IV for TKIP.  For CCMP the
 * EXTIV bit is likewise set but the 8 bytes represent the
 * CCMP header rather than IV+extended-IV.
 */

#define ATH6KL_KEYBUF_SIZE 16
#define ATH6KL_MICBUF_SIZE (8+8)	/* space for both tx and rx */

#define ATH6KL_KEY_XMIT  0x01
#define ATH6KL_KEY_RECV  0x02
#define ATH6KL_KEY_DEFAULT   0x80	/* default xmit key */

/*
 * WPA/RSN get/set key request.  Specify the key/cipher
 * type and whether the key is to be used for sending and/or
 * receiving.  The key index should be set only when working
 * with global keys (use IEEE80211_KEYIX_NONE for ``no index'').
 * Otherwise a unicast/pairwise key is specified by the bssid
 * (on a station) or mac address (on an ap).  They key length
 * must include any MIC key data; otherwise it should be no
 * more than ATH6KL_KEYBUF_SIZE.
 */
struct ath6kl_req_key {
	bool valid;
	u8 key_index;
	int key_type;
	u8 key[WLAN_MAX_KEY_LEN];
	u8 key_len;
};

/* flag info for ar */
#define WMI_ENABLED             0  /* ar */
#define WMI_READY               1  /* ar */
#define WMI_CTRL_EP_FULL        2  /* ar */
#define DESTROY_IN_PROGRESS     3  /* ar */
#define WLAN_ENABLED            4  /* ar */
#define WLAN_WOW_ENABLE         5  /* ar */
/* flag info for vif */
#define CONNECTED               10 /* vif */
#define CONNECT_PEND            11 /* vif */
#define STATS_UPDATE_PEND       12 /* vif */
#define WMM_ENABLED             13 /* vif */
#define NETQ_STOPPED            14 /* vif */
#define DTIM_EXPIRED            15 /* vif */
#define NETDEV_REGISTERED       16 /* vif */
#define SKIP_SCAN               17 /* vif */
#define AMSDU_ENABLED           18 /* vif */

#define SONY_BASE         		19 /* vif */
#define SONY_WMI_UPDATE         SONY_BASE /* vif */
#define SONY_WMI_SCAN           (SONY_BASE+1) /* vif */
#define SONY_WMI_TESTMODE_RX    (SONY_BASE+2) /* vif */
#define SONY_WMI_TESTMODE_GET   (SONY_BASE+3) /* vif */


enum hif_type {
	HIF_TYPE_SDIO,
	HIF_TYPE_USB
};

struct ath6kl_vif {
	struct net_device *net_dev;
			int ssid_len;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 next_mode;
	u8 nw_type;
	u8 dot11_auth_mode;
	u8 auth_mode;
	u8 prwise_crypto;
	u8 prwise_crypto_len;
	u8 grp_crypto;
	u8 grp_crypto_len;
	u8 def_txkey_index;
	struct ath6kl_wep_key wep_key_list[WMI_MAX_KEY_INDEX + 1];
	u8 bssid[ETH_ALEN];
	u8 req_bssid[ETH_ALEN];
	u16 ch_hint;
	u16 bss_ch;
	u16 listen_intvl_b;
	u16 listen_intvl_t;
	u8 lrssi_roam_threshold;

	
	u8 tx_pwr;
	struct ath6kl_node_mapping node_map[MAX_NODE_NUM];
	u8 ibss_ps_enable;
	u8 node_num;
	u32 connect_ctrl_flags;
	u32 user_key_ctrl;
	u8 usr_bss_filter;
	struct cfg80211_scan_request *scan_req;
	struct timer_list scan_timer;
	struct ath6kl_key keys[WMI_MAX_KEY_INDEX + 1];
	enum sme_state sme_state;
	struct wmi_scan_params_cmd sc_params;
	u16 next_chan;
	u32 send_action_id;
	u8 intra_bss;
	struct aggr_info *aggr_cntxt;
	struct wmi_ap_mode_stat ap_stats;
	u8 ap_country_code[3];
	
	struct timer_list disconnect_timer;

	struct ath6kl_sta sta_list[AP_MAX_NUM_STA];
	u8 sta_list_index;
	struct ath6kl_req_key ap_mode_bkey;
	struct sk_buff_head mcastpsq;
	spinlock_t mcastpsq_lock;
	int reconnect_flag;
	struct net_device_stats net_stats;
	struct wireless_dev *wdev;
	u8 if_type;
	u8 if_sub_type;
	u8 device_index;
	struct ath6kl *ar;

	bool probe_req_report;
	bool p2p;
	unsigned long flag;
	u16 assoc_bss_beacon_int;
	u8 assoc_bss_dtim_period;
	wait_queue_head_t event_wq;
	bool wmm_enabled;
	bool next_mode_p2p;
};

#define NUM_DEV 2
struct ath6kl {
	struct device *dev;
	struct ath6kl_vif *vif[NUM_DEV];
	struct wiphy *wiphy;
	struct ath6kl_bmi bmi;
	const struct ath6kl_hif_ops *hif_ops;
	struct wmi *wmi;
	struct list_head amsdu_rx_buffer_queue;
	int tx_pending[ENDPOINT_MAX];
	int total_tx_data_pend;
	struct htc_target *htc_target;
	enum hif_type hif_type;
	void *hif_priv;
	u8 num_dev;
	u8 dev_addr[ETH_ALEN];
	spinlock_t lock;
	struct semaphore sem;
	struct semaphore wmi_evt_sem;
	struct target_stats target_stats;

	u8 next_ep_id;
	struct ath6kl_cookie *cookie_list;
	u32 cookie_count;
	enum htc_endpoint_id ac2ep_map[WMM_NUM_AC];
	bool ac_stream_active[WMM_NUM_AC];
	u8 ac_stream_pri_map[WMM_NUM_AC];
	u8 hiac_stream_active_pri;
	u8 ep2ac_map[ENDPOINT_MAX];
	enum htc_endpoint_id ctrl_ep;
	struct htc_credit_state_info credit_state_info;
	u32 target_type;	
	struct ath6kl_version version;
	u8 rx_meta_ver;
	enum wlan_low_pwr_state wlan_pwr_state;
#define AR_MCAST_FILTER_MAC_ADDR_SIZE  4

	u16 conf_flags;
	wait_queue_head_t event_wq;
	struct ath6kl_mbox_info mbox_info;

	struct ath6kl_cookie cookie_mem[MAX_COOKIE_NUM];
	int reconnect_flag;
	unsigned long flag;

	u8 *fw_board;
	size_t fw_board_len;

	u8 *fw_otp;
	size_t fw_otp_len;

	u8 *fw;
	size_t fw_len;

	u8 *fw_patch;
	size_t fw_patch_len;

	struct workqueue_struct *ath6kl_wq;

	struct dentry *debugfs_phy;

	bool probe_req_report;
	u32 send_action_id;
	u16 next_chan;

	bool p2p;

	bool tx99;

#ifdef CONFIG_ATH6KL_DEBUG
	struct {
		struct circ_buf fwlog_buf;
		spinlock_t fwlog_lock;
		void *fwlog_tmp;
		u32 fwlog_mask;
		unsigned int dbgfs_diag_reg;
		u32 diag_reg_addr_wr;
		u32 diag_reg_val_wr;
        u64 set_tx_series;
        u8 min_rx_bundle_frame;
        u8 min_rx_bundle_timeout;
		u8 mimo_ps_enable;

		struct green_tx_param {
			u32 green_tx_enable;
			u8 next_probe_count;
			u8 max_backoff;
			u8 min_gtx_rssi;
			u8 force_backoff;
		} green_tx_params;

		struct smps_param {
			u8 flags;
			u8 rssi_thresh;
			u8 data_thresh;
			u8 mode;
			u8 automatic;
		} smps_params;

		struct lpl_force_enable_param {
			u8 lpl_policy;
			u8 no_blocker_detect;
			u8 no_rfb_detect;
		} lpl_force_enable_params;

		struct ht_cap_param {
			u8 isConfig;
			u8 band;
			u8 chan_width_40M_supported;
			u8 short_GI;
		} ht_cap_param[IEEE80211_NUM_BANDS];
		struct channel_list_param {
			u8 num_ch;
			u16 ch_list[WMI_MAX_CHANNELS * 2];
		} channel_list_param;

	} debug;
	struct work_struct firmware_crash_dump_deferred_work;
#endif /* CONFIG_ATH6KL_DEBUG */
};

static inline void *ath6kl_priv(struct net_device *dev)
{
	return netdev_priv(dev);
}

static inline void ath6kl_deposit_credit_to_ep(struct htc_credit_state_info
					       *cred_info,
					       struct htc_endpoint_credit_dist
					       *ep_dist, int credits)
{
	ep_dist->credits += credits;
	ep_dist->cred_assngd += credits;
	cred_info->cur_free_credits -= credits;
}

static inline u32 ath6kl_get_hi_item_addr(struct ath6kl *ar,
					  u32 item_offset)
{
	u32 addr = 0;

	if (ar->target_type == TARGET_TYPE_AR6003)
		addr = ATH6KL_AR6003_HI_START_ADDR + item_offset;
	else if (ar->target_type == TARGET_TYPE_AR6004)
		addr = ATH6KL_AR6004_HI_START_ADDR + item_offset;
	else if (ar->target_type == TARGET_TYPE_AR6006)
		addr = ATH6KL_AR6006_HI_START_ADDR + item_offset;

	return addr;
}

int ath6kl_if_add(struct ath6kl *ar, struct net_device **ndev, char *name, u8 if_type, u8 dev_index);
void ath6kl_destroy(struct ath6kl *ar, unsigned int unregister);
int ath6kl_configure_target(struct ath6kl *ar);
void ath6kl_detect_error(unsigned long ptr);
void disconnect_timer_handler(unsigned long ptr);
void init_netdev(struct net_device *dev);
void ath6kl_cookie_init(struct ath6kl *ar);
void ath6kl_cookie_cleanup(struct ath6kl *ar);
void ath6kl_rx(struct htc_target *target, struct htc_packet *packet);
void ath6kl_tx_complete(struct htc_target *context,
						struct list_head *packet_queue);
enum htc_send_full_action ath6kl_tx_queue_full(struct htc_target *target,
					       struct htc_packet *packet);
void ath6kl_stop_txrx(struct ath6kl *ar);
void ath6kl_cleanup_amsdu_rxbufs(struct ath6kl *ar);
int ath6kl_diag_write(struct ath6kl *ar, u32 address, void *data, u32 length);
int ath6kl_diag_read(struct ath6kl *ar, u32 address, void *data, u32 length);
int ath6kl_read_fwlogs(struct ath6kl *ar);
void ath6kl_init_profile_info(struct ath6kl_vif *vif);
void ath6kl_tx_data_cleanup(struct ath6kl *ar);
void ath6kl_stop_endpoint(struct net_device *dev, bool keep_profile,
			  bool get_dbglogs);

struct ath6kl_cookie *ath6kl_alloc_cookie(struct ath6kl *ar);
void ath6kl_free_cookie(struct ath6kl *ar, struct ath6kl_cookie *cookie);
int ath6kl_data_tx(struct sk_buff *skb, struct net_device *dev);

struct aggr_info *aggr_init(struct ath6kl_vif *vif);
struct aggr_conn_info *aggr_init_conn(struct ath6kl_vif *vif);

void ath6kl_rx_refill(struct htc_target *target,
		      enum htc_endpoint_id endpoint);
void ath6kl_refill_amsdu_rxbufs(struct ath6kl *ar, int count);
struct htc_packet *ath6kl_alloc_amsdu_rxbuf(struct htc_target *target,
					    enum htc_endpoint_id endpoint,
					    int len);

void aggr_module_destroy(struct aggr_info *aggr);
void aggr_module_destroy_conn(struct aggr_conn_info *aggr_conn);
void aggr_reset_state(struct aggr_conn_info *aggr_conn);

struct ath6kl_sta *ath6kl_find_sta(struct ath6kl_vif *vif, u8 * node_addr);
struct ath6kl_sta *ath6kl_find_sta_by_aid(struct ath6kl_vif *vif, u8 aid);

void ath6kl_ready_event(void *devt, u8 * datap, u32 sw_ver, u32 abi_ver);
int ath6kl_control_tx(void *devt, struct sk_buff *skb,
		      enum htc_endpoint_id eid);
int ath6kl_control_tx_sync(void *devt, struct sk_buff *skb,
		      enum htc_endpoint_id eid);
void ath6kl_connect_event(struct ath6kl_vif *vif, u16 channel,
			  u8 *bssid, u16 listen_int,
			  u16 beacon_int, enum network_type net_type,
			  u8 beacon_ie_len, u8 assoc_req_len,
			  u8 assoc_resp_len, u8 *assoc_info);
void ath6kl_connect_ap_mode_bss(struct ath6kl_vif *vif, u16 channel);
void ath6kl_connect_ap_mode_sta(struct ath6kl_vif *vif, u16 aid, u8 *mac_addr,
				u8 keymgmt, u8 ucipher, u8 auth,
				u8 assoc_req_len, u8 *assoc_info, u8 apsd_info);
void ath6kl_disconnect_event(struct ath6kl_vif *vif, u8 reason,
			     u8 *bssid, u8 assoc_resp_len,
			     u8 *assoc_info, u16 prot_reason_status);
void ath6kl_tkip_micerr_event(struct ath6kl_vif *vif, u8 keyid, bool ismcast);
void ath6kl_txpwr_rx_evt(void *devt, u8 tx_pwr);
void ath6kl_scan_complete_evt(struct ath6kl_vif *vif, int status);
void ath6kl_tgt_stats_event(struct ath6kl_vif *vif, u8 *ptr, u32 len);
void ath6kl_indicate_tx_activity(void *devt, u8 traffic_class, bool active);
enum htc_endpoint_id ath6kl_ac2_endpoint_id(void *devt, u8 ac);

void ath6kl_pspoll_event(struct ath6kl_vif *vif, u8 aid);

void ath6kl_dtimexpiry_event(struct ath6kl_vif *vif);
void ath6kl_disconnect(struct ath6kl_vif *vif);
void ath6kl_deep_sleep_enable(struct ath6kl_vif *vif);
void aggr_recv_delba_req_evt(struct ath6kl_vif *vif, u8 tid);
void aggr_recv_addba_req_evt(struct ath6kl_vif *vif, u8 tid, u16 seq_no,
			     u8 win_sz);
void aggr_recv_addba_resp_evt(struct ath6kl_vif *vif, u8 tid, u16 amsdu_sz, u8 status);
void ath6kl_wakeup_event(void *dev);
void ath6kl_target_failure(struct ath6kl *ar);
void ath6kl_scan_timer_handler(unsigned long ptr);

#define ATH6KL_PSQ_MAX_AGE   5
static inline u32 ath6kl_sk_buff_get_age(struct sk_buff* buf)
{
	return buf->csum;
}

static inline void ath6kl_sk_buff_set_age(struct sk_buff* buf, u32 age)
{
	buf->csum = age;
}

static inline u32 ath6kl_mgmt_buff_get_age(struct mgmt_buff* buf)
{
	return buf->age;
}

static inline void ath6kl_mgmt_buff_set_age(struct mgmt_buff* buf, u32 age)
{
	buf->age = age;
}

void ath6kl_mgmt_queue_init(struct mgmt_buff_head* psq);
void ath6kl_mgmt_queue_purge(struct mgmt_buff_head* psq);
int ath6kl_mgmt_queue_empty(struct mgmt_buff_head* psq);
int ath6kl_mgmt_queue_tail(struct mgmt_buff_head* psq, const u8* buf, u16 len, u32 id, u32 freq, u32 wait);
struct mgmt_buff* ath6kl_mgmt_dequeue_head(struct mgmt_buff_head* psq);
void ath6kl_psq_age_handler(unsigned long ptr);
void ath6kl_psq_age_start(struct ath6kl_sta* sta);
void ath6kl_psq_age_stop(struct ath6kl_sta* sta);

#endif /* CORE_H */
