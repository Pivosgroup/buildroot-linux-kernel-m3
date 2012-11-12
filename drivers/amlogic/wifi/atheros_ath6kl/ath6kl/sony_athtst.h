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

/*
 * This file contains the definitions of the WMI protocol specified in the
 * Wireless Module Interface (WMI).  It includes definitions of all the
 * commands and events. Commands are messages from the host to the WM.
 * Events and Replies are messages from the WM to the host.
 */

#ifndef SONY_WMI_H
#define SONY_WMI_H

#include <linux/ieee80211.h>

#include "htc.h"
#include "wmi.h"
#include "ioctl_vendor_sony.h"
#include "tx99_wcmd.h"

struct wmi_reg_cmd {
	u32 addr;
    u32 val;
} __packed;

#define ATHCFG_WCMD_ADDR_LEN             6
struct WMI_CUSTOMER_STAINFO_STRUCT {
//#define ATHCFG_WCMD_STAINFO_LINKUP        0x1 
//#define ATHCFG_WCMD_STAINFO_SHORTGI       0x2
    u32                    flags;                       /* Flags */
    athcfg_wcmd_phymode_t  phymode;                     /* STA Connection Type */
    u8                     bssid[ATHCFG_WCMD_ADDR_LEN]; /* STA Current BSSID */
    u32                    assoc_time;                  /* STA association time */
    u32                    wpa_4way_handshake_time;     /* STA 4-way time */
    u32                    wpa_2way_handshake_time;     /* STA 2-way time */  
    u32                    rx_rate_kbps;                /* STA latest RX Rate (Kbps) */    
//#define ATHCFG_WCMD_STAINFO_MCS_NULL      0xff    
    u8                     rx_rate_mcs;                 /* STA latest RX Rate MCS (11n) */    
    u32                    tx_rate_kbps;                /* STA latest TX Rate (Kbps) */    
    u8                     rx_rssi;                     /* RSSI of all received frames */
    u8                     rx_rssi_beacon;              /* RSSI of Beacon */
    /* TBD : others information */
} __packed;

/**
 * @brief get txpower-info
 */
struct WMI_CUSTOMER_TXPOW_STRUCT {
//#define ATHCFG_WCMD_TX_POWER_TABLE_SIZE 32
#define ATHCFG_WCMD_TX_POWER_TABLE_SIZE 48//for ar6002
    u8                       txpowertable[ATHCFG_WCMD_TX_POWER_TABLE_SIZE];
} __packed;

/**
 * @brief get version-info
 */
struct WMI_CUSTOMER_VERSION_INFO_STRUCT{
    //u8    driver[32];         /* Driver Short Name */
    //u8    version[32];        /* Driver Version */
    u32   sw_version;           /*SW version*/
    u8    driver[32];         /* Driver Short Name */
} __packed;

/**
 * @brief  set/get testmode-info
 */
struct WMI_CUSTOMER_TESTMODE_STRUCT{
    u8      bssid[ATHCFG_WCMD_ADDR_LEN];
    u32     chan;         /* ChanID */

#define ATHCFG_WCMD_TESTMODE_CHAN     0x1    
#define ATHCFG_WCMD_TESTMODE_BSSID    0x2    
#define ATHCFG_WCMD_TESTMODE_RX       0x3    
#define ATHCFG_WCMD_TESTMODE_RESULT   0x4    
#define ATHCFG_WCMD_TESTMODE_ANT      0x5    
    u16     operation;    /* Operation */
    u8      antenna;      /* RX-ANT */
    u8      rx;           /* RX START/STOP */
    u32     rssi_combined;/* RSSI */
    u32     rssi0;        /* RSSI */
    u32     rssi1;        /* RSSI */
    u32     rssi2;        /* RSSI */
} __packed;






#if 0
typedef enum {
    TX99_WCMD_ENABLE,
    TX99_WCMD_DISABLE,
    TX99_WCMD_SET_FREQ,
    TX99_WCMD_SET_RATE,
    TX99_WCMD_SET_POWER,
    TX99_WCMD_SET_TXMODE,
    TX99_WCMD_SET_CHANMASK,
    TX99_WCMD_SET_TYPE,
    TX99_WCMD_GET
} tx99_wcmd_type_t;

typedef struct tx99_wcmd_data {
    u_int32_t freq;	/* tx frequency (MHz) */
    u_int32_t htmode;	/* tx bandwidth (HT20/HT40) */
	u_int32_t htext;	/* extension channel offset (0(none), 1(plus) and 2(minus)) */
	u_int32_t rate;	/* Kbits/s */
	u_int32_t power;  	/* (dBm) */
	u_int32_t txmode; 	/* wireless mode, 11NG(8), auto-select(0) */
	u_int32_t chanmask; /* tx chain mask */
	u_int32_t type;
} tx99_wcmd_data_t;
#endif

/**
 * @brief  set/get testmode-info
 */
struct WMI_TX99_STRUCT{
    char                if_name[IFNAMSIZ];/**< Interface name */
    tx99_wcmd_type_t    type;             /**< Type of wcmd */
    tx99_wcmd_data_t    data;             /**< Data */     
} __packed;

void ath6kl_tgt_sony_get_reg_event(struct ath6kl_vif *vif, u8 *ptr, u32 len);
void ath6kl_tgt_sony_get_version_info_event(struct ath6kl_vif *vif, u8 *ptr, u32 len);
void ath6kl_tgt_sony_get_testmode_event(struct ath6kl_vif *vif, u8 *ptr, u32 len);
void ath6kl_tgt_sony_get_txpow_event(struct ath6kl_vif *vif, u8 *ptr, u32 len);
void ath6kl_tgt_sony_get_stainfo_event(struct ath6kl_vif *vif, u8 *ptr, u32 len);
void ath6kl_tgt_sony_get_stainfo_beacon_rssi_event(struct ath6kl_vif *vif, u8 *ptr, u32 len);
void ath6kl_tgt_sony_get_scaninfo_event(struct ath6kl_vif *vif, u8 *datap, u32 len);
void ath6kl_tgt_sony_set_scan_done_event(struct ath6kl_vif *vif, u8 *ptr, u32 len);

int ath6kl_wmi_get_customer_product_info_cmd(struct ath6kl_vif *vif, athcfg_wcmd_product_info_t *val);

int ath6kl_wmi_set_customer_reg_cmd(struct ath6kl_vif *vif, u32 addr,u32 val);

int ath6kl_wmi_get_customer_reg_cmd(struct ath6kl_vif *vif, u32 addr,u32 *val);

int ath6kl_wmi_set_customer_scan_cmd(struct ath6kl_vif *vif);

int ath6kl_wmi_get_customer_scan_cmd(struct ath6kl_vif *vif,athcfg_wcmd_scan_t *val);

int ath6kl_wmi_get_customer_testmode_cmd(struct ath6kl_vif *vif,athcfg_wcmd_testmode_t *val);

int ath6kl_wmi_set_customer_testmode_cmd(struct ath6kl_vif *vif,athcfg_wcmd_testmode_t *val);

int ath6kl_wmi_get_customer_scan_time_cmd(struct ath6kl_vif *vif,athcfg_wcmd_scantime_t *val);

int ath6kl_wmi_get_customer_version_info_cmd(struct ath6kl_vif *vif, athcfg_wcmd_version_info_t *val);

int ath6kl_wmi_get_customer_txpow_cmd(struct ath6kl_vif *vif, athcfg_wcmd_txpower_t *val);

int ath6kl_wmi_get_customer_stainfo_cmd(struct ath6kl_vif *vif, athcfg_wcmd_sta_t *val);

int ath6kl_wmi_get_customer_mode_cmd(struct ath6kl_vif *vif, athcfg_wcmd_phymode_t *val);

int ath6kl_wmi_get_customer_stats_cmd(struct ath6kl_vif *vif, athcfg_wcmd_stats_t *val);
#endif /* SONY_WMI_H */
