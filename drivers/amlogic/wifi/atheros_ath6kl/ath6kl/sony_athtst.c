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
#include "ioctl_vendor_sony.h"
#include "sony_athtst.h"
#include "hif-ops.h"
#include "tx99_wcmd.h"

#define FY13

#ifdef FY13//add by randy
u32 addr_private;
u32 val_private;
athcfg_wcmd_sta_t stainfo_private;
athcfg_wcmd_txpower_t txpow_private;
athcfg_wcmd_version_info_t version_info_private;
athcfg_wcmd_testmode_t testmode_private;
#define LINUX_PVT_WIOCTL                (SIOCDEVPRIVATE + 1)
#define ATHCFG_WCMD_IOCTL               (SIOCDEVPRIVATE+15)     /*IEEE80211_IOCTL_VENDOR*/

#define SIOCIOCTLTX99                   (SIOCDEVPRIVATE+13)

void ath6kl_tgt_sony_get_reg_event(struct ath6kl_vif *vif, u8 *ptr, u32 len)
{
	struct wmi_reg_cmd *p = (struct wmi_reg_cmd *) ptr;
    u32 addr;
    u32 val;
    addr_private = addr = le32_to_cpu(p->addr);
    val_private = val = le32_to_cpu(p->val);
   // printk("%s[%d]addr=0x%x,val=0x%x\n\r",__func__,__LINE__,addr,val);
    
    
	if (test_bit(SONY_WMI_UPDATE, &vif->flag)) {
		clear_bit(SONY_WMI_UPDATE, &vif->flag);
        //ath6kl_wakeup_event(vif->ar->wmi->parent_dev);        
        wake_up(&vif->event_wq);
    }
}

void ath6kl_tgt_sony_get_version_info_event(struct ath6kl_vif *vif, u8 *ptr, u32 len)
{
	struct WMI_CUSTOMER_VERSION_INFO_STRUCT *p = (struct WMI_CUSTOMER_VERSION_INFO_STRUCT *) ptr;
    //int i;
    
    memcpy(version_info_private.driver,p->driver,sizeof(version_info_private.driver));
   // memcpy(version_info_private.version,p->version,sizeof(version_info_private.version));
   // memcpy(version_info_private.fw_version,p->version,sizeof(version_info_private.fw_version));
    p->sw_version = le32_to_cpu(p->sw_version);
    sprintf(version_info_private.version,"%d.%d.%d.%d",(p->sw_version&0xf0000000)>>28,(p->sw_version&0x0f000000)>>24,
            (p->sw_version&0x00ff0000)>>16,(p->sw_version&0x0000000f));
    sprintf(version_info_private.fw_version,"%d.%d.%d.%d",(p->sw_version&0xf0000000)>>28,(p->sw_version&0x0f000000)>>24,
            (p->sw_version&0x00ff0000)>>16,(p->sw_version&0x0000000f)); 
	if (test_bit(SONY_WMI_UPDATE, &vif->flag)) {
		clear_bit(SONY_WMI_UPDATE, &vif->flag);      
        wake_up(&vif->event_wq);
    }

}

void ath6kl_tgt_sony_get_testmode_event(struct ath6kl_vif *vif, u8 *ptr, u32 len)
{
	struct WMI_CUSTOMER_TESTMODE_STRUCT *p = (struct WMI_CUSTOMER_TESTMODE_STRUCT *) ptr;
    //int i;
    //printk("%s[%d]p->operation=%d\n",__func__,__LINE__,p->operation);
    switch(p->operation) {
    case ATHCFG_WCMD_TESTMODE_CHAN:   
        testmode_private.chan = le16_to_cpu(p->chan);
        break;
    case ATHCFG_WCMD_TESTMODE_BSSID:
        memcpy(testmode_private.bssid,p->bssid,sizeof(testmode_private.bssid));
        break;
    case ATHCFG_WCMD_TESTMODE_RX:
        testmode_private.rx = p->rx; 
        break;
    case ATHCFG_WCMD_TESTMODE_RESULT:
        testmode_private.rssi_combined = le32_to_cpu(p->rssi_combined);
        testmode_private.rssi0 = le32_to_cpu(p->rssi0);
        testmode_private.rssi1 = le32_to_cpu(p->rssi1);
        testmode_private.rssi2 = le32_to_cpu(p->rssi2); 
        break;
    case ATHCFG_WCMD_TESTMODE_ANT:
        testmode_private.antenna = p->antenna;
        break;
    }
    
	if (test_bit(SONY_WMI_TESTMODE_GET, &vif->flag)) {
		clear_bit(SONY_WMI_TESTMODE_GET, &vif->flag);      
        wake_up(&vif->event_wq);
    }

}

void ath6kl_tgt_sony_get_txpow_event(struct ath6kl_vif *vif, u8 *ptr, u32 len)
{
	struct WMI_CUSTOMER_TXPOW_STRUCT *p = (struct WMI_CUSTOMER_TXPOW_STRUCT *) ptr;
    //int i;
    
    memcpy(&txpow_private.txpowertable[0],&p->txpowertable[0],sizeof(struct WMI_CUSTOMER_TXPOW_STRUCT ));

	if (test_bit(SONY_WMI_UPDATE, &vif->flag)) {
		clear_bit(SONY_WMI_UPDATE, &vif->flag);      
        wake_up(&vif->event_wq);
    }

}

void ath6kl_tgt_sony_get_stainfo_event(struct ath6kl_vif *vif, u8 *ptr, u32 len)
{
	struct WMI_CUSTOMER_STAINFO_STRUCT *p = (struct WMI_CUSTOMER_STAINFO_STRUCT *) ptr;

    stainfo_private.flags = p->flags;
    stainfo_private.phymode = p->phymode;
    memcpy(&stainfo_private.bssid,p->bssid,sizeof(stainfo_private.bssid));
    stainfo_private.assoc_time = le32_to_cpu(p->assoc_time);
    stainfo_private.wpa_4way_handshake_time = le32_to_cpu(p->wpa_4way_handshake_time);
    stainfo_private.wpa_2way_handshake_time = le32_to_cpu(p->wpa_2way_handshake_time);
    stainfo_private.rx_rate_kbps = le32_to_cpu(p->rx_rate_kbps);
    stainfo_private.rx_rate_mcs = p->rx_rate_mcs;
    stainfo_private.tx_rate_kbps = le32_to_cpu(p->tx_rate_kbps);
    stainfo_private.rx_rssi = p->rx_rssi;
    stainfo_private.rx_rssi_beacon = p->rx_rssi_beacon;
    
    #if 0
    printk("%s[%d]stainfo_private.flags = %d\n\r",__func__,__LINE__,stainfo_private.flags);
    printk("%s[%d]stainfo_private.phymode = %d\n\r",__func__,__LINE__,stainfo_private.phymode);
    printk("%s[%d]stainfo_private.assoc_time = %d\n\r",__func__,__LINE__,stainfo_private.assoc_time);
    printk("%s[%d]stainfo_private.wpa_4way_handshake_time = %d\n\r",__func__,__LINE__,stainfo_private.wpa_4way_handshake_time);
    printk("%s[%d]stainfo_private.wpa_2way_handshake_time = %d\n\r",__func__,__LINE__,stainfo_private.wpa_2way_handshake_time);
    printk("%s[%d]stainfo_private.rx_rate_kbps = %d\n\r",__func__,__LINE__,stainfo_private.rx_rate_kbps);
    printk("%s[%d]stainfo_private.rx_rate_mcs = %d\n\r",__func__,__LINE__,stainfo_private.rx_rate_mcs);
    printk("%s[%d]stainfo_private.tx_rate_kbps = %d\n\r",__func__,__LINE__,stainfo_private.tx_rate_kbps);
    printk("%s[%d]stainfo_private.rx_rssi = %d\n\r",__func__,__LINE__,stainfo_private.rx_rssi);
    printk("%s[%d]stainfo_private.rx_rssi_beacon = %d\n\r",__func__,__LINE__,stainfo_private.rx_rssi_beacon);
    #endif
	if (test_bit(SONY_WMI_UPDATE, &vif->flag)) {
		clear_bit(SONY_WMI_UPDATE, &vif->flag);      
        wake_up(&vif->event_wq);
    }
}
void ath6kl_tgt_sony_get_stainfo_beacon_rssi_event(struct ath6kl_vif *vif, u8 *datap, u32 len)
{
	struct wmi_bss_info_hdr2 *bih;
	u8 *buf;
	struct ieee80211_channel *channel;
	//struct ieee80211_mgmt *mgmt;
	//struct cfg80211_bss *bss;

	if (len <= sizeof(struct wmi_bss_info_hdr2))
		return;

	bih = (struct wmi_bss_info_hdr2 *) datap;
	buf = datap + sizeof(struct wmi_bss_info_hdr2);
	len -= sizeof(struct wmi_bss_info_hdr2);

	printk(
		   "bss info evt - ch %u, snr %d, rssi %d, bssid \"%pM\" "
		   "frame_type=%d\n",
		   bih->ch, bih->snr, bih->snr - 95, bih->bssid,
		   bih->frame_type);

	if (bih->frame_type != BEACON_FTYPE &&
	    bih->frame_type != PROBERESP_FTYPE)
		return; /* Only update BSS table for now */

	channel = ieee80211_get_channel(vif->wdev->wiphy, le16_to_cpu(bih->ch));
	if (channel == NULL)
		return;

	if (len < 8 + 2 + 2)
		return;
    stainfo_private.rx_rssi_beacon = (bih->snr < 0) ? 0 : bih->snr;
	if (test_bit(SONY_WMI_UPDATE, &vif->flag)) {
		clear_bit(SONY_WMI_UPDATE, &vif->flag);      
        wake_up(&vif->event_wq);
    }    
    return;
}
static int total_bss_info = 0;
static athcfg_wcmd_scan_result_t scaninfor_db[100];
struct ieee80211_ie_header {
    u_int8_t    element_id;     /* Element Id */
    u_int8_t    length;         /* IE Length */
} __packed;
enum {
    IEEE80211_ELEMID_SSID             = 0,
    IEEE80211_ELEMID_RATES            = 1,
    IEEE80211_ELEMID_FHPARMS          = 2,
    IEEE80211_ELEMID_DSPARMS          = 3,
    IEEE80211_ELEMID_CFPARMS          = 4,
    IEEE80211_ELEMID_TIM              = 5,
    IEEE80211_ELEMID_IBSSPARMS        = 6,
    IEEE80211_ELEMID_COUNTRY          = 7,
    IEEE80211_ELEMID_REQINFO          = 10,
    IEEE80211_ELEMID_QBSS_LOAD        = 11,
    IEEE80211_ELEMID_CHALLENGE        = 16,
    /* 17-31 reserved for challenge text extension */
    IEEE80211_ELEMID_PWRCNSTR         = 32,
    IEEE80211_ELEMID_PWRCAP           = 33,
    IEEE80211_ELEMID_TPCREQ           = 34,
    IEEE80211_ELEMID_TPCREP           = 35,
    IEEE80211_ELEMID_SUPPCHAN         = 36,
    IEEE80211_ELEMID_CHANSWITCHANN    = 37,
    IEEE80211_ELEMID_MEASREQ          = 38,
    IEEE80211_ELEMID_MEASREP          = 39,
    IEEE80211_ELEMID_QUIET            = 40,
    IEEE80211_ELEMID_IBSSDFS          = 41,
    IEEE80211_ELEMID_ERP              = 42,
    IEEE80211_ELEMID_HTCAP_ANA        = 45,
    IEEE80211_ELEMID_RESERVED_47      = 47,
    IEEE80211_ELEMID_RSN              = 48,
    IEEE80211_ELEMID_XRATES           = 50,
    IEEE80211_ELEMID_HTCAP            = 51,
    IEEE80211_ELEMID_HTINFO           = 52,
    IEEE80211_ELEMID_MOBILITY_DOMAIN  = 54,
    IEEE80211_ELEMID_FT               = 55,
    IEEE80211_ELEMID_TIMEOUT_INTERVAL = 56,
    IEEE80211_ELEMID_EXTCHANSWITCHANN = 60,
    IEEE80211_ELEMID_HTINFO_ANA       = 61,
	IEEE80211_ELEMID_WAPI		      = 68,   /*IE for WAPI*/
    IEEE80211_ELEMID_2040_COEXT       = 72,
    IEEE80211_ELEMID_2040_INTOL       = 73,
    IEEE80211_ELEMID_OBSS_SCAN        = 74,
    IEEE80211_ELEMID_XCAPS            = 127,
    IEEE80211_ELEMID_RESERVED_133     = 133,
    IEEE80211_ELEMID_TPC              = 150,
    IEEE80211_ELEMID_CCKM             = 156,
    IEEE80211_ELEMID_VENDOR           = 221,  /* vendor private */
};
#define	WPA_OUI_TYPE		0x01
#define	WPA_OUI			0xf25000
#define LE_READ_4(p)					\
    ((a_uint32_t)					\
     ((((const a_uint8_t *)(p))[0]      ) |		\
      (((const a_uint8_t *)(p))[1] <<  8) |		\
      (((const a_uint8_t *)(p))[2] << 16) |		\
      (((const a_uint8_t *)(p))[3] << 24)))
#define BE_READ_4(p)                        \
    ((a_uint32_t)                            \
     ((((const a_uint8_t *)(p))[0] << 24) |      \
      (((const a_uint8_t *)(p))[1] << 16) |      \
      (((const a_uint8_t *)(p))[2] <<  8) |      \
      (((const a_uint8_t *)(p))[3]      )))      
static inline a_int8_t
iswpaoui(const a_uint8_t *frm)
{
    return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}
#define WPS_OUI  0xf25000
#define WPS_OUI_TYPE  0x04
#define WSC_OUI 0x0050f204
static inline a_int8_t
iswpsoui(const a_uint8_t *frm)
{
    return frm[1] > 3 && BE_READ_4(frm+2) == WSC_OUI;
}
#define WME_OUI                     0xf25000
#define WME_OUI_TYPE                    0x02
#define WME_INFO_OUI_SUBTYPE            0x00
#define WME_PARAM_OUI_SUBTYPE           0x01
#define WME_TSPEC_OUI_SUBTYPE           0x02
static inline int 
iswmeparam(const u_int8_t *frm)
{
    return ((frm[1] > 5) && (LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI)) &&
        (frm[6] == WME_PARAM_OUI_SUBTYPE));
}
#define ATH_OUI                     0x7f0300    /* Atheros OUI */
#define ATH_OUI_TYPE                    0x01
static int inline
isatherosoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}
/* Temporary vendor specific IE for 11n pre-standard interoperability */
#define VENDOR_HT_OUI       0x00904c
#define VENDOR_HT_CAP_ID    51
#define VENDOR_HT_INFO_ID   52
static inline int 
ishtcap(const u_int8_t *frm)
{
    return ((frm[1] > 3) && (BE_READ_4(frm+2) == ((VENDOR_HT_OUI<<8)|VENDOR_HT_CAP_ID)));
}
static inline int 
ishtinfo(const u_int8_t *frm)
{
    return ((frm[1] > 3) && (BE_READ_4(frm+2) == ((VENDOR_HT_OUI<<8)|VENDOR_HT_INFO_ID)));
}
struct ieee80211_ie_htcap_cmn {
    u_int16_t   hc_cap;         /* HT capabilities */
//#if _BYTE_ORDER == _BIG_ENDIAN
//   u_int8_t    hc_reserved     : 3,    /* B5-7 reserved */
//                hc_mpdudensity  : 3,    /* B2-4 MPDU density (aka Minimum MPDU Start Spacing) */
//                hc_maxampdu     : 2;    /* B0-1 maximum rx A-MPDU factor */
//#else
    u_int8_t    hc_maxampdu     : 2,    /* B0-1 maximum rx A-MPDU factor */
                hc_mpdudensity  : 3,    /* B2-4 MPDU density (aka Minimum MPDU Start Spacing) */
                hc_reserved     : 3;    /* B5-7 reserved */
//#endif
    u_int8_t    hc_mcsset[16];          /* supported MCS set */
    u_int16_t   hc_extcap;              /* extended HT capabilities */
    u_int32_t   hc_txbf;                /* txbf capabilities */
    u_int8_t    hc_antenna;             /* antenna capabilities */
} __packed;
void ath6kl_tgt_sony_get_scaninfo_event(struct ath6kl_vif *vif, u8 *datap, u32 len)
{
	struct wmi_bss_info_hdr2 *bih;
	u8 *buf;
	struct ieee80211_channel *channel;
	struct ieee80211_mgmt *mgmt;
	//struct cfg80211_bss *bss;

	if (len <= sizeof(struct wmi_bss_info_hdr2))
		return;

	bih = (struct wmi_bss_info_hdr2 *) datap;
	buf = datap + sizeof(struct wmi_bss_info_hdr2);
	len -= sizeof(struct wmi_bss_info_hdr2);

	printk(
		   "bss info evt - ch %u, snr %d, rssi %d, bssid \"%pM\" "
		   "frame_type=%d\n",
		   bih->ch, bih->snr, bih->snr - 95, bih->bssid,
		   bih->frame_type);

	if (bih->frame_type != BEACON_FTYPE &&
	    bih->frame_type != PROBERESP_FTYPE)
		return; /* Only update BSS table for now */

	channel = ieee80211_get_channel(vif->wdev->wiphy, le16_to_cpu(bih->ch));
	if (channel == NULL)
		return;

	if (len < 8 + 2 + 2)
		return;
	/*
	 * In theory, use of cfg80211_inform_bss_ath6kl() would be more natural here
	 * since we do not have the full frame. However, at least for now,
	 * cfg80211 can only distinguish Beacon and Probe Response frames from
	 * each other when using cfg80211_inform_bss_frame_ath6kl(), so let's build a
	 * fake IEEE 802.11 header to be able to take benefit of this.
	 */
	mgmt = kmalloc(24 + len, GFP_ATOMIC);
	if (mgmt == NULL)
		return;

	if (bih->frame_type == BEACON_FTYPE) {
		mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
						  IEEE80211_STYPE_BEACON);
		memset(mgmt->da, 0xff, ETH_ALEN);
	} else {
		struct net_device *dev = vif->net_dev;

		mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
						  IEEE80211_STYPE_PROBE_RESP);
		memcpy(mgmt->da, dev->dev_addr, ETH_ALEN);
	}
	mgmt->duration = cpu_to_le16(0);
	memcpy(mgmt->sa, bih->bssid, ETH_ALEN);
	memcpy(mgmt->bssid, bih->bssid, ETH_ALEN);
	mgmt->seq_ctrl = cpu_to_le16(0);

	memcpy(&mgmt->u.beacon, buf, len);
    
    
    {//save to scan information database
        athcfg_wcmd_scan_result_t   scan_entry;
        
        
        memset(&scan_entry,0x00,sizeof(athcfg_wcmd_scan_result_t));
        scan_entry.isr_freq = bih->ch;
        scan_entry.isr_ieee = (unsigned char)ieee80211_frequency_to_channel_ath6kl(bih->ch);
        scan_entry.isr_rssi = (bih->snr < 0) ? 0 : bih->snr;
        memcpy(&scan_entry.isr_bssid[0],&bih->bssid[0],ATHCFG_WCMD_ADDR_LEN);
        scan_entry.isr_capinfo = le16_to_cpu(mgmt->u.beacon.capab_info);
        //parse ie information
        {
            struct ieee80211_ie_header *ie_element;
            unsigned char *temp_ptr;
            int remained_len;
            ie_element = (struct ieee80211_ie_header *)&(mgmt->u.beacon.variable);
            remained_len = len - 5*sizeof(u8);
            while(remained_len >= 0) {   
                remained_len = remained_len - sizeof(struct ieee80211_ie_header)- ie_element->length;
                if (ie_element->length == 0) {
                    ie_element += 1;    /* next IE */
                    continue;
                }                
                if (remained_len < ie_element->length) {
                    /* Incomplete/bad info element */
                    //printk("EOF\n");
                    break;
                }                
                temp_ptr = (unsigned char *)ie_element;
                temp_ptr = temp_ptr+sizeof(struct ieee80211_ie_header);//point to data area
                
                switch (ie_element->element_id) {
                    case IEEE80211_ELEMID_SSID:
                        memcpy(&scan_entry.isr_ssid,temp_ptr,ie_element->length);
                        //printk("info_element->length=%d\n",ie_element->length); 
                        //printk("SSID=%s\n",scan_entry.isr_ssid);
                        break;
                    case IEEE80211_ELEMID_RATES:
                        memcpy(&scan_entry.isr_rates[0],temp_ptr,ie_element->length);
                        scan_entry.isr_nrates = ie_element->length;
                        break;
                    case IEEE80211_ELEMID_XRATES:        
                        memcpy(&scan_entry.isr_rates[scan_entry.isr_nrates],temp_ptr,ie_element->length);
                        scan_entry.isr_nrates += ie_element->length;
                        break;
                    case IEEE80211_ELEMID_ERP:
                        memcpy(&scan_entry.isr_erp,temp_ptr,ie_element->length);                      
                        break;
                    case IEEE80211_ELEMID_RSN:
                        scan_entry.isr_rsn_ie.len = ie_element->length;
                        memcpy(&scan_entry.isr_rsn_ie.data,temp_ptr,ie_element->length);    
                        break;    
                    case IEEE80211_ELEMID_HTCAP_ANA:
                        if (scan_entry.isr_htcap_ie.len == 0) {
                            scan_entry.isr_htcap_ie.len = ie_element->length;
                            memcpy(&scan_entry.isr_htcap_ie.data[0],temp_ptr,ie_element->length);
                        }
                        break;
                    case IEEE80211_ELEMID_HTINFO_ANA:
                        /* we only care if there isn't already an HT IE (ANA) */
                        if (scan_entry.isr_htinfo_ie.len == 0) {
                            scan_entry.isr_htinfo_ie.len = ie_element->length;
                            memcpy(&scan_entry.isr_htinfo_ie.data[0],temp_ptr,ie_element->length);
                        }
                        break;                     
                    case IEEE80211_ELEMID_HTCAP:
                        /* we only care if there isn't already an HT IE (ANA) */
                        if (scan_entry.isr_htcap_ie.len == 0) {
                            scan_entry.isr_htcap_ie.len = ie_element->length;
                            memcpy(&scan_entry.isr_htcap_ie.data[0],temp_ptr,ie_element->length);
                        }
                        break;     
                    case IEEE80211_ELEMID_HTINFO:
                        /* we only care if there isn't already an HT IE (ANA) */
                        if (scan_entry.isr_htinfo_ie.len == 0) {
                            scan_entry.isr_htinfo_ie.len = ie_element->length;
                            memcpy(&scan_entry.isr_htinfo_ie.data[0],temp_ptr,ie_element->length);
                        }
                        break;                        
                    case IEEE80211_ELEMID_VENDOR:   
                        if (iswpaoui((u_int8_t *) ie_element)) {
                            scan_entry.isr_wpa_ie.len = ie_element->length;
                            memcpy(&scan_entry.isr_wpa_ie.data[0],temp_ptr,ie_element->length);
                        } else if (iswpsoui((u_int8_t *) ie_element)) {
                            scan_entry.isr_wps_ie.len = ie_element->length;
                            memcpy(&scan_entry.isr_wps_ie.data[0],temp_ptr,ie_element->length);
                        } else if (iswmeparam((u_int8_t *) ie_element)) {
                            scan_entry.isr_wme_ie.len = ie_element->length;
                            memcpy(&scan_entry.isr_wme_ie.data[0],temp_ptr,ie_element->length);                        
                        } else if (isatherosoui((u_int8_t *) ie_element)) {
                            scan_entry.isr_ath_ie.len = ie_element->length;
                            memcpy(&scan_entry.isr_ath_ie.data[0],temp_ptr,ie_element->length);                      
                        } else if (ishtcap((u_int8_t *) ie_element)) {
                            if (scan_entry.isr_htcap_ie.len == 0) {
                                scan_entry.isr_htcap_ie.len = ie_element->length;
                                memcpy(&scan_entry.isr_htcap_ie.data[0],temp_ptr,ie_element->length);               
                            }
                        }
                        else if (ishtinfo((u_int8_t *) ie_element)) {
                            if (scan_entry.isr_htinfo_ie.len == 0) {
                                scan_entry.isr_htinfo_ie.len = ie_element->length;
                                memcpy(&scan_entry.isr_htinfo_ie.data[0],temp_ptr,ie_element->length);
                            }
                        } else {
                            //printk("Unknow know !!info_element->length=%d\n",ie_element->length); 
                        }
                        break;
                    default:
                        //printk("Unknow know info_element->length=%d\n",ie_element->length); 
                        break;
                }
                ie_element = (struct ieee80211_ie_header *)(temp_ptr + ie_element->length);
            }
        }
        
        
        //find the exist entry or add new one
        {
            int i,entry_match_item;
            bool entry_match = 0;
            
            if(total_bss_info >= 100) {
                printk("Excess max entry 100\n");        
                kfree(mgmt);
                return;
            }
            
            for(i=0;i<total_bss_info;i++) {
                if(scaninfor_db[i].isr_freq == scan_entry.isr_freq) {
                    if(memcmp(scaninfor_db[i].isr_bssid ,scan_entry.isr_bssid,ATHCFG_WCMD_ADDR_LEN) == 0) {
                        //find it                     
                        if(strcmp(scaninfor_db[i].isr_ssid ,scan_entry.isr_ssid) == 0) {
                            //update it
                            entry_match = 1;
                            entry_match_item = i;
                            //printk("fully match!! i=%d,update it,total_bss_info=%d\n",i,total_bss_info);
                            memcpy(&scaninfor_db[i],&scan_entry,sizeof(athcfg_wcmd_scan_result_t));
                            break;
                        }
                    }
                }
            }
            
            if(entry_match == 0) {
                memcpy(&scaninfor_db[total_bss_info],&scan_entry,sizeof(athcfg_wcmd_scan_result_t));
                #if 0
                printk("[%d]Freq=%d\n",total_bss_info,scaninfor_db[total_bss_info].isr_freq);        
                printk("[%d]CH=%d\n",total_bss_info,scaninfor_db[total_bss_info].isr_ieee);        
                printk("[%d]RSSI=%d\n",total_bss_info,scaninfor_db[total_bss_info].isr_rssi);  
                printk("[%d]BSSID=%pM\n",total_bss_info,scaninfor_db[total_bss_info].isr_bssid);
                printk("[%d]SSID=%s\n",total_bss_info,scaninfor_db[total_bss_info].isr_ssid);
                printk("[%d]isr_htinfo_ie.len=%d\n",total_bss_info,scaninfor_db[total_bss_info].isr_htinfo_ie.len);  
                #endif                
                total_bss_info++;
                //printk("               next entry=%d\n",total_bss_info);
            } else {
                #if 0
                printk("[%d]Freq=%d\n",entry_match_item,scaninfor_db[entry_match_item].isr_freq);        
                printk("[%d]CH=%d\n",entry_match_item,scaninfor_db[entry_match_item].isr_ieee);        
                printk("[%d]RSSI=%d\n",entry_match_item,scaninfor_db[entry_match_item].isr_rssi);  
                printk("[%d]BSSID=%pM\n",entry_match_item,scaninfor_db[entry_match_item].isr_bssid);
                printk("[%d]SSID=%s\n",entry_match_item,scaninfor_db[entry_match_item].isr_ssid);
                printk("[%d]isr_htinfo_ie.len=%d\n",entry_match_item,scaninfor_db[entry_match_item].isr_htinfo_ie.len);  
                #endif                 
            }
        }               
    }
    
    kfree(mgmt);
    ///total_bss_info++;
    return;
}

void ath6kl_tgt_sony_set_scan_done_event(struct ath6kl_vif *vif, u8 *datap, u32 len)
{
	//struct wmi_bss_info_hdr2 *bih;
	//u8 *buf;
	//struct ieee80211_channel *channel;

	if (test_bit(SONY_WMI_SCAN, &vif->flag)) {
		clear_bit(SONY_WMI_SCAN, &vif->flag);    
        wake_up(&vif->event_wq);
    }    
    return;
}

//SONY WMI funciton
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

int ath6kl_wmi_get_customer_product_info_cmd(struct ath6kl_vif *vif, athcfg_wcmd_product_info_t *val)
{
	int ret = 0;
    extern void ath6kl_usb_get_usbinfo(void *product_info);
    ath6kl_usb_get_usbinfo((void*)val); 
	return ret;
}

int ath6kl_wmi_set_customer_reg_cmd(struct ath6kl_vif *vif, u32 addr,u32 val)
{
	struct sk_buff *skb;
	struct wmi_reg_cmd *cmd;
	int ret;

	skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_reg_cmd *) skb->data;
	cmd->addr = addr;
    cmd->val = val;
    //printk("%s[%d]reg addr=0x%x,val=0x%x\n\r",__func__,__LINE__,cmd->addr,cmd->val);
	ret = ath6kl_wmi_cmd_send(vif, skb, WMI_SET_CUSTOM_REG,
				  NO_SYNC_WMIFLAG);
	return ret;
}

int ath6kl_wmi_get_customer_testmode_cmd(struct ath6kl_vif *vif, athcfg_wcmd_testmode_t *val)
{
	struct sk_buff *skb;
	struct WMI_CUSTOMER_TESTMODE_STRUCT *cmd;
	int ret;
    int left;
    
	skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
	if (!skb)
		return -ENOMEM;
    //printk("%s[%d]\n",__func__,__LINE__);
	cmd = (struct WMI_CUSTOMER_TESTMODE_STRUCT *) skb->data;

	//transfer to little endian
    memcpy(cmd->bssid, val->bssid,  sizeof(cmd->bssid));
    cmd->chan = cpu_to_le32(val->chan); 
    cmd->operation = cpu_to_le16(val->operation);
    cmd->antenna = val->antenna;
    cmd->rx = val->rx; 

    cmd->rssi_combined = cpu_to_le32(val->rssi_combined); 
    cmd->rssi0 = cpu_to_le32(val->rssi0); 
    cmd->rssi1 = cpu_to_le32(val->rssi1); 
    cmd->rssi2 = cpu_to_le32(val->rssi2);

    set_bit(SONY_WMI_TESTMODE_GET, &vif->flag);

	ret = ath6kl_wmi_cmd_send(vif, skb, WMI_GET_CUSTOM_TESTMODE,
				  NO_SYNC_WMIFLAG);
            
	left = wait_event_interruptible_timeout(vif->event_wq,
			!test_bit(SONY_WMI_TESTMODE_GET, &vif->flag), WMI_TIMEOUT);      
                        
	if (left == 0)
		return -ETIMEDOUT;                  
    switch(val->operation) 
    {
        case ATHCFG_WCMD_TESTMODE_CHAN:
            val->chan = testmode_private.chan;
            break;
        case ATHCFG_WCMD_TESTMODE_BSSID:
            memcpy(val->bssid,testmode_private.bssid,sizeof(val->bssid));
            break;            
        case ATHCFG_WCMD_TESTMODE_RX:
            val->rx = testmode_private.rx;
            break;
        case ATHCFG_WCMD_TESTMODE_RESULT:
            val->rssi_combined = testmode_private.rssi_combined;
            val->rssi0 = testmode_private.rssi0;
            val->rssi1 = testmode_private.rssi1;
            val->rssi2 = testmode_private.rssi2;   
            break;
        case ATHCFG_WCMD_TESTMODE_ANT:
            val->antenna = testmode_private.antenna;
            break;
        default:
            printk("%s[%d]Not support\n",__func__,__LINE__);
            break;
    }        
	return ret;
}

int ath6kl_wmi_set_customer_testmode_cmd(struct ath6kl_vif *vif, athcfg_wcmd_testmode_t *val)
{
	struct sk_buff *skb;
	struct WMI_CUSTOMER_TESTMODE_STRUCT *cmd;
	int ret = 0;
    bool send_wmi_flag = false;
    
    switch(val->operation) 
    {
        case ATHCFG_WCMD_TESTMODE_BSSID:
            {
                if(memcmp(testmode_private.bssid,val->bssid,sizeof(testmode_private.bssid)) != 0) {
                    memcpy(testmode_private.bssid,val->bssid,sizeof(testmode_private.bssid));
                    send_wmi_flag = true;
                }
                //printk("%s[%d] testmode_private.bssid=\"%pM\"\n",__func__,__LINE__,testmode_private.bssid);
            }
        break;
        case ATHCFG_WCMD_TESTMODE_CHAN:
            {
                if(testmode_private.chan != val->chan) {
                    testmode_private.chan = val->chan;
                    send_wmi_flag = true;
                }
                //printk("%s[%d] testmode_private.chan=%d\n",__func__,__LINE__,testmode_private.chan);        
            }
            break;
        case ATHCFG_WCMD_TESTMODE_RX:
            {
                if(testmode_private.rx != val->rx) {
                    testmode_private.rx = val->rx;   
                    send_wmi_flag = true;
                }
            }        
            break;
        case ATHCFG_WCMD_TESTMODE_ANT:            
        default:
            printk("%s[%d]Not support\n",__func__,__LINE__);
            return -1;
    }
    
    //send WMI to target
    if(send_wmi_flag) {
        testmode_private.rx = val->rx;
        skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
        if (!skb)
            return -ENOMEM;

        cmd = (struct WMI_CUSTOMER_TESTMODE_STRUCT *) skb->data;
        //transfer to little endian
        memcpy(cmd->bssid, val->bssid,  sizeof(cmd->bssid));
        cmd->chan = cpu_to_le32(val->chan); 
        cmd->operation = cpu_to_le16(val->operation);
        cmd->antenna = val->antenna;
        cmd->rx = val->rx; 

        cmd->rssi_combined = cpu_to_le32(val->rssi_combined); 
        cmd->rssi0 = cpu_to_le32(val->rssi0); 
        cmd->rssi1 = cpu_to_le32(val->rssi1); 
        cmd->rssi2 = cpu_to_le32(val->rssi2); 
        
        ret = ath6kl_wmi_cmd_send(vif, skb, WMI_SET_CUSTOM_TESTMODE,
                      NO_SYNC_WMIFLAG);
    }  
    
    if(val->operation == ATHCFG_WCMD_TESTMODE_RX) {
        if(val->rx == 1) {
            s8 n_channels = 1;
            u16 *channels = NULL;
            int i;
            set_bit(SONY_WMI_TESTMODE_RX, &vif->flag);

            //set scan param
            ath6kl_wmi_scanparams_cmd(
                            vif, 
                            0, 
                            0xffff, 
                            0,     
                            vif->sc_params.minact_chdwell_time,
                            vif->sc_params.maxact_chdwell_time,//0xffff, 
                            1000,//vif->sc_params.pas_chdwell_time, msec
                            vif->sc_params.short_scan_ratio,
                            (vif->sc_params.scan_ctrl_flags & ~ACTIVE_SCAN_CTRL_FLAGS), 
                            //vif->sc_params.scan_ctrl_flags, 
                            vif->sc_params.max_dfsch_act_time, 
                            vif->sc_params.maxact_scan_per_ssid);                  

            //assign request channel
            channels = kzalloc(n_channels * sizeof(u16), GFP_KERNEL);
            if (channels == NULL) {
                n_channels = 0;
            }
            
            for (i = 0; i < n_channels; i++) {
                channels[i] = ieee80211_channel_to_frequency_ath6kl((testmode_private.chan == 0 ? 1:testmode_private.chan),IEEE80211_NUM_BANDS);                    
                //printk("%s[%d]channels[%d]=%d,testmode_private.chan=%d\n",__func__,__LINE__,i,channels[i],testmode_private.chan);
            }

            ret = ath6kl_wmi_bssfilter_cmd(
                 vif,
                 ALL_BSS_FILTER, 0);
            if (ret) {
                printk("%s[%d] Fail\n",__func__,__LINE__);
                goto rx_fail;
            }                 
            ret = ath6kl_wmi_startscan_cmd(vif, WMI_LONG_SCAN, 1,
                               false, 0, 0, n_channels, channels);                              
            if (ret) {
                printk("%s[%d] Fail\n",__func__,__LINE__);
                goto rx_fail;
            }
rx_fail:            
            kfree(channels);
        } else {
            //cancel test mode scan
            clear_bit(SONY_WMI_TESTMODE_RX, &vif->flag);                 
			ath6kl_wmi_scanparams_cmd(vif, 
									   vif->sc_params.fg_start_period, 
									   vif->sc_params.fg_end_period, 
									   vif->sc_params.bg_period, 
									   vif->sc_params.minact_chdwell_time, 
									   vif->sc_params.maxact_chdwell_time, 
									   vif->sc_params.pas_chdwell_time,
									   vif->sc_params.short_scan_ratio, 
									   vif->sc_params.scan_ctrl_flags, 
									   vif->sc_params.max_dfsch_act_time,
									   vif->sc_params.maxact_scan_per_ssid);	
        }
    }
	return ret;
}

int ath6kl_wmi_get_customer_reg_cmd(struct ath6kl_vif *vif, u32 addr,u32 *val)
{
	struct sk_buff *skb;
	struct wmi_reg_cmd *cmd;
	int ret;
    int left;

	skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_reg_cmd *) skb->data;
	cmd->addr = addr;
    //cmd->val = val;
    set_bit(SONY_WMI_UPDATE, &vif->flag);
	//vif->ar->wmi->pwr_mode = pwr_mode;
    //printk("%s[%d]Start get reg addr=0x%x\n\r",__func__,__LINE__,cmd->addr);
	ret = ath6kl_wmi_cmd_send(vif, skb, WMI_GET_CUSTOM_REG,
				  NO_SYNC_WMIFLAG);
    //printk("%s[%d]Wait\n\r",__func__,__LINE__);              
	left = wait_event_interruptible_timeout(vif->event_wq,
			!test_bit(SONY_WMI_UPDATE, &vif->flag), WMI_TIMEOUT);      
    //printk("%s[%d]Wait Fin ,left=%d\n\r",__func__,__LINE__,left);                          
	if (left == 0)
		return -ETIMEDOUT;
        
    //printk("%s[%d]reg addr=0x%x,val_private=0x%x\n\r",__func__,__LINE__,addr_private,val_private);        
    *val = val_private;
	return ret;
}

int ath6kl_wmi_set_customer_scan_cmd(struct ath6kl_vif *vif)
{
	//athcfg_wcmd_sta_t *cmd;
	int ret;
    int left;

    set_bit(SONY_WMI_SCAN, &vif->flag);
    //printk("%s[%d]Start sony scan,vif=0x%x\n\r",__func__,__LINE__,(int*)vif);        
    ret = ath6kl_wmi_bssfilter_cmd(
        vif,
         ALL_BSS_FILTER, 0);
    if (ret) {
        return ret;
    }
	
    total_bss_info = 0;
    memset(&scaninfor_db[0],0x00,sizeof(scaninfor_db));
	ret = ath6kl_wmi_startscan_cmd(vif, WMI_LONG_SCAN, 0,
				       false, 0, 0, 0, NULL);

    left = wait_event_interruptible_timeout(vif->event_wq,
            !test_bit(SONY_WMI_SCAN, &vif->flag), WMI_TIMEOUT*10);      
    
    ath6kl_wmi_bssfilter_cmd(vif, NONE_BSS_FILTER, 0);

	return ret;
}

int ath6kl_wmi_get_customer_scan_cmd(struct ath6kl_vif *vif,athcfg_wcmd_scan_t *val)
{
	a_uint8_t			scan_offset;
	int ret=0,i;
    athcfg_wcmd_scan_t 	*custom_scan_temp;
    
    scan_offset = val->offset;
    custom_scan_temp = val;
    if( (scan_offset + ATHCFG_WCMD_MAX_AP) < total_bss_info) {
        custom_scan_temp->cnt = 8;
        custom_scan_temp->more = 1;
    } else {
        custom_scan_temp->cnt = total_bss_info - scan_offset;
        custom_scan_temp->more = 0;
    }
    //printk("%s[%d]scan_offset=%d,cnt=%d,more=%d\n",__func__,__LINE__,scan_offset,custom_scan_temp->cnt,custom_scan_temp->more);
    for(i=0; i<custom_scan_temp->cnt; i++){        
        if (scaninfor_db[scan_offset+i].isr_htcap_ie.len != 0) {
            memcpy(&scaninfor_db[scan_offset+i].isr_htcap_mcsset[0], 
              ((struct ieee80211_ie_htcap_cmn *)scaninfor_db[scan_offset+i].isr_htcap_ie.data)->hc_mcsset,
              ATHCFG_WCMD_MAX_HT_MCSSET);
        }          
        memcpy(&custom_scan_temp->result[i],&scaninfor_db[scan_offset+i],sizeof(athcfg_wcmd_scan_result_t));        
    }
    return ret;
}

int ath6kl_wmi_get_customer_scan_time_cmd(struct ath6kl_vif *vif,athcfg_wcmd_scantime_t *val)
{
	struct sk_buff *skb;
	athcfg_wcmd_sta_t *cmd;
	int ret;
    int left;
    unsigned long start_scan_timestamp;
    unsigned long end_scan_timestamp;
    unsigned long scan_time_msec;
    
	skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (athcfg_wcmd_sta_t *) skb->data;

    //set_bit(SONY_WMI_UPDATE, &vif->flag);
    set_bit(SONY_WMI_SCAN, &vif->flag);
   // printk("%s[%d]Start sony scan\n\r",__func__,__LINE__);        
    ret = ath6kl_wmi_bssfilter_cmd(
        vif,
         ALL_BSS_FILTER, 0);
    if (ret) {
        return ret;
    }
    start_scan_timestamp = jiffies;
	ret = ath6kl_wmi_startscan_cmd(vif, WMI_LONG_SCAN, 0,
				       false, 0, 0, 0, NULL);

    left = wait_event_interruptible_timeout(vif->event_wq,
            !test_bit(SONY_WMI_SCAN, &vif->flag), WMI_TIMEOUT*10);      
    
    end_scan_timestamp = jiffies;
    scan_time_msec = jiffies_to_msecs(end_scan_timestamp-start_scan_timestamp);
    if(val) 
        val->scan_time = scan_time_msec;
    //printk("%s[%d]scan_time_msec = %d msec \n\r",__func__,__LINE__,scan_time_msec);  
    ath6kl_wmi_bssfilter_cmd(vif, NONE_BSS_FILTER, 0);
        
        
    //printk("%s[%d]\n\r",__func__,__LINE__);        
    //memcpy(val ,&stainfo_private,sizeof(athcfg_wcmd_sta_t));
	return ret;
}

int ath6kl_wmi_get_customer_version_info_cmd(struct ath6kl_vif *vif, athcfg_wcmd_version_info_t *val)
{
    struct sk_buff *skb;
	athcfg_wcmd_version_info_t *cmd;
	int ret;
    int left;

	skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (athcfg_wcmd_version_info_t *) skb->data;
    
    set_bit(SONY_WMI_UPDATE, &vif->flag);

    //printk("%s[%d]Start get txpower\n\r",__func__,__LINE__);
	ret = ath6kl_wmi_cmd_send(vif, skb, WMI_GET_CUSTOM_VERSION_INFO,
				  NO_SYNC_WMIFLAG);
    //printk("%s[%d]Wait\n\r",__func__,__LINE__);              
	left = wait_event_interruptible_timeout(vif->event_wq,
			!test_bit(SONY_WMI_UPDATE, &vif->flag), WMI_TIMEOUT);      
    //printk("%s[%d]Wait Fin ,left=%d\n\r",__func__,__LINE__,left);                          
	if (left == 0)
		return -ETIMEDOUT;
        
    //printk("%s[%d]\n\r",__func__,__LINE__);        
    //get tx result
    memcpy(val ,&version_info_private,sizeof(athcfg_wcmd_version_info_t));
	return ret;
}

int ath6kl_wmi_get_customer_txpow_cmd(struct ath6kl_vif *vif, athcfg_wcmd_txpower_t *val)
{
	struct sk_buff *skb;
	athcfg_wcmd_txpower_t *cmd;
	int ret;
    int left;

	skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (athcfg_wcmd_txpower_t *) skb->data;
    
    set_bit(SONY_WMI_UPDATE, &vif->flag);

    //printk("%s[%d]Start get txpower\n\r",__func__,__LINE__);
	ret = ath6kl_wmi_cmd_send(vif, skb, WMI_GET_CUSTOM_WIFI_TXPOW,
				  NO_SYNC_WMIFLAG);
    //printk("%s[%d]Wait\n\r",__func__,__LINE__);              
	left = wait_event_interruptible_timeout(vif->event_wq,
			!test_bit(SONY_WMI_UPDATE, &vif->flag), WMI_TIMEOUT);      
    //printk("%s[%d]Wait Fin ,left=%d\n\r",__func__,__LINE__,left);                          
	if (left == 0)
		return -ETIMEDOUT;
        
    //printk("%s[%d]\n\r",__func__,__LINE__);        
    //get tx result
    memcpy(val ,&txpow_private,sizeof(athcfg_wcmd_txpower_t));
	return ret;
}

int ath6kl_wmi_get_customer_stainfo_cmd(struct ath6kl_vif *vif, athcfg_wcmd_sta_t *val)
{
	struct sk_buff *skb;
	athcfg_wcmd_sta_t *cmd;
	int ret;
    int left;

	skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (athcfg_wcmd_sta_t *) skb->data;
    set_bit(SONY_WMI_UPDATE, &vif->flag);
	ret = ath6kl_wmi_cmd_send(vif, skb, WMI_GET_CUSTOM_STAINFO,
				  NO_SYNC_WMIFLAG);
       
	left = wait_event_interruptible_timeout(vif->event_wq,
			!test_bit(SONY_WMI_UPDATE, &vif->flag), WMI_TIMEOUT);      
                      
	if (left == 0)
		return -ETIMEDOUT;

    //get beacon RSSI
    if(test_bit(CONNECTED, &vif->flag))
    {
        if(vif->nw_type != AP_NETWORK) {
            set_bit(SONY_WMI_UPDATE, &vif->flag);
            //printk("%s[%d]Start get beacon rssi,vif=0x%x\n\r",__func__,__LINE__,(int*)vif);        
            ret = ath6kl_wmi_bssfilter_cmd(
                 vif,
                 CURRENT_BSS_FILTER, 0);
            if (ret) {
                return ret;
            }
            left = wait_event_interruptible_timeout(vif->event_wq,
                    !test_bit(SONY_WMI_UPDATE, &vif->flag), WMI_TIMEOUT*10);      
            
            ath6kl_wmi_bssfilter_cmd(vif, NONE_BSS_FILTER, 0);                
        }
    }
   
    //printk("%s[%d]vif->nw_type=%d\n\r",__func__,__LINE__,vif->nw_type);        
    memcpy(val ,&stainfo_private,sizeof(athcfg_wcmd_sta_t));
	return ret;
}

int ath6kl_wmi_get_customer_mode_cmd(struct ath6kl_vif *vif, athcfg_wcmd_phymode_t *val)
{
	struct sk_buff *skb;
	athcfg_wcmd_sta_t *cmd;
	int ret;
    int left;

	skb = ath6kl_wmi_get_new_buf(sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (athcfg_wcmd_sta_t *) skb->data;

    set_bit(SONY_WMI_UPDATE, &vif->flag);

	ret = ath6kl_wmi_cmd_send(vif, skb, WMI_GET_CUSTOM_STAINFO,
				  NO_SYNC_WMIFLAG);
    //printk("%s[%d]Wait\n\r",__func__,__LINE__);              
	left = wait_event_interruptible_timeout(vif->event_wq,
			!test_bit(SONY_WMI_UPDATE, &vif->flag), WMI_TIMEOUT);      
    //printk("%s[%d]Wait Fin ,left=%d\n\r",__func__,__LINE__,left);                          
	if (left == 0)
		return -ETIMEDOUT;

    //get beacon RSSI
    if(test_bit(CONNECTED, &vif->flag))
    {
        //printk("%s[%d]vif->nw_type=%d\n\r",__func__,__LINE__,vif->nw_type);        
        memcpy(val ,&stainfo_private.phymode,sizeof(athcfg_wcmd_phymode_t));
    } else {
        memset(val ,0x0,sizeof(athcfg_wcmd_phymode_t));
    }

	return ret;
}

int ath6kl_wmi_get_customer_stats_cmd(struct ath6kl_vif *vif, athcfg_wcmd_stats_t *val)
{
	//struct sk_buff *skb;
	//athcfg_wcmd_sta_t *cmd;
	int ret;
    int left;

	set_bit(STATS_UPDATE_PEND, &vif->flag);

	ret = ath6kl_wmi_get_stats_cmd(vif);

	if (ret != 0) {
		up(&vif->ar->sem);
		return -EIO;
	}

	left = wait_event_interruptible_timeout(vif->event_wq,
			!test_bit(STATS_UPDATE_PEND, &vif->flag), WMI_TIMEOUT);  
                       
	if (left == 0)
		return -ETIMEDOUT;
	else if (left < 0)
		return left;
        
        
	val->txPackets = vif->ar->target_stats.tx_pkt;
	
	val->txRetry = vif->ar->target_stats.tx_fail_cnt;//retry count,not include first tx
    val->txAggrRetry = 0;
    val->txAggrSubRetry = 0;
    val->txAggrExcessiveRetry = 0;
    val->txSubRetry = 0;

	return ret;
}

static int
_ath6kl_athtst_wioctl(struct net_device *netdev, void *data)
{
	struct ath6kl_vif *vif = ath6kl_priv(netdev);
    struct athcfg_wcmd     *req = NULL;
    unsigned long      req_len = sizeof(struct athcfg_wcmd);
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
//    printk("%s[%d]req->iic_cmd=%d\n\r",__func__,__LINE__,req->iic_cmd);
    switch(req->iic_cmd) {
        case ATHCFG_WCMD_GET_DEV_PRODUCT_INFO:  
            ath6kl_wmi_get_customer_product_info_cmd(vif, &req->iic_data.product_info);
            break;
        case ATHCFG_WCMD_GET_REG:          
            ath6kl_wmi_get_customer_reg_cmd(vif, req->iic_data.reg.addr,&req->iic_data.reg.val);
            break;
        case ATHCFG_WCMD_SET_REG:          
            ath6kl_wmi_set_customer_reg_cmd(vif, req->iic_data.reg.addr,req->iic_data.reg.val);
            break;            
        case ATHCFG_WCMD_GET_TESTMODE:    
            ath6kl_wmi_get_customer_testmode_cmd(vif, &req->iic_data.testmode); 
            break;    
        case ATHCFG_WCMD_SET_TESTMODE:  
            ath6kl_wmi_set_customer_testmode_cmd(vif, &req->iic_data.testmode);            
            break;                
        case ATHCFG_WCMD_GET_SCANTIME:
            ath6kl_wmi_get_customer_scan_time_cmd(vif,&req->iic_data.scantime);            
            break;
        case ATHCFG_WCMD_GET_MODE://get it from stainfo command
            ath6kl_wmi_get_customer_mode_cmd(vif, &req->iic_data.phymode);
            break;
        case ATHCFG_WCMD_GET_SCAN:
            ath6kl_wmi_get_customer_scan_cmd(vif,req->iic_data.scan);
            break;     
        case ATHCFG_WCMD_SET_SCAN:
            ath6kl_wmi_set_customer_scan_cmd(vif);
            break;    
        case ATHCFG_WCMD_GET_DEV_VERSION_INFO:
            ath6kl_wmi_get_customer_version_info_cmd(vif, &req->iic_data.version_info);
            break;       
        case ATHCFG_WCMD_GET_TX_POWER:
            ath6kl_wmi_get_customer_txpow_cmd(vif, &req->iic_data.txpower);
            break;  
        case ATHCFG_WCMD_GET_STAINFO:
            ath6kl_wmi_get_customer_stainfo_cmd(vif, &req->iic_data.station_info);
            break;
        case ATHCFG_WCMD_GET_STATS:
            ath6kl_wmi_get_customer_stats_cmd(vif, &req->iic_data.stats);
            break;
        default:
            goto done;
    }
    //status = __athtst_wioctl_sw[req->iic_cmd](sc, (athcfg_wcmd_data_t *)&req->iic_data);       
    status = 0;

    /* XXX: Optimize only copy the amount thats valid */
    if(copy_to_user(data, req, req_len))
        status = -EIO;

done:
    vfree(req);

    return status;
}

int ath6kl_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
    int error = EINVAL;
    //printk("%s[%d]cmd=%d\n\r",__func__,__LINE__,cmd);
    switch (cmd) {       
    case ATHCFG_WCMD_IOCTL:
         error =  _ath6kl_athtst_wioctl(netdev, ifr->ifr_data);
         //printk("%s[%d]ATHTST command\n\r",__func__,__LINE__);
         break;
    case SIOCIOCTLTX99:
         error =  ath6kl_wmi_tx99_cmd(netdev, ifr->ifr_data);
         //printk("%s[%d]ATHTST command\n\r",__func__,__LINE__);
         break;    
    default:
         printk("%s[%d]Not support\n\r",__func__,__LINE__);
         break ;
    }

    return error;
}
#endif
