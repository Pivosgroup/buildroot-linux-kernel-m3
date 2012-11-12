/*
 *  Copyright (c) 2010 Atheros Communications Inc.
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

#ifndef _IOCTL_VENDOR_SONY_H_
#define _IOCTL_VENDOR_SONY_H_
/* 
 * SONY Specific IOCTL API : This file shares between kernel and user space.
 */
#define ATHCFG_WCMD_VENDORID            (0x2011)                /* SONY - 2011 Project */
#define ATHCFG_WCMD_IOCTL               (SIOCDEVPRIVATE+15)     /*IEEE80211_IOCTL_VENDOR*/

/**
 * Defines
 */
#define a_int32_t   signed int 
#define a_uint8_t   unsigned char     
#define a_uint16_t  unsigned short int
#define a_uint64_t  unsigned long long
#define a_uint32_t  unsigned int
#define a_int8_t    signed char
#define a_int16_t   signed short int 

#define ATHCFG_WCMD_NAME_SIZE           16 /* Max Device name size */
#define ATHCFG_WCMD_ADDR_LEN             6
#define ATHCFG_WCMD_MAC_STR_LEN         17

#define ATHCFG_WCMD_MAX_AP               8
#define ATHCFG_WCMD_RATE_MAXSIZE        30 
#define ATHCFG_WCMD_MAX_HT_MCSSET       16 /* 128-bit */
#define ATHCFG_WCMD_MAX_SSID            32
#define ATHCFG_WCMD_IE_MAXLEN          256 /* Max Len for IE */

/**
 * @brief Get/Set wireless commands
 */
#define IOCTL_SET_MASK     (0x80000000)
#define IOCTL_GET_MASK     (0x40000000)
#define IOCTL_PHYDEV_MASK  (0x08000000)
#define IOCTL_VIRDEV_MASK  (0x04000000)

/* 
 * IEEE80211_IOCTL_ : for Virtual Device           (athX)
 * ATH_IOCTL_       : for Physical Device          (wifiX)
 * DRIVER_IOCTO_    : for Physical/VIrtual Device  (wifiX/athX)
 */
#define ATH_IOCTL_PRODINFO                  (IOCTL_PHYDEV_MASK | 1)       /* Production Information */  
#define ATH_IOCTL_REGRW                     (IOCTL_PHYDEV_MASK | 2)       /* Register Read/Write */  
#define ATH_IOCTL_VERSINFO                  (IOCTL_PHYDEV_MASK | 3)       /* Version Information */  
#define ATH_IOCTL_TX_POWER                  (IOCTL_PHYDEV_MASK | 4)       /* TX Power Information */  
#define ATH_IOCTL_STATS                     (IOCTL_PHYDEV_MASK | 5)       /* Device Statistic Information */  

#define IEEE80211_IOCTL_TESTMODE            (IOCTL_VIRDEV_MASK | 1)       /* Test Mode for Factory */
#define IEEE80211_IOCTL_STAINFO             (IOCTL_VIRDEV_MASK | 2)       /* Station Information */          
#define IEEE80211_IOCTL_PHYMODE             (IOCTL_VIRDEV_MASK | 3)       /* PHY Mode */          
#define IEEE80211_IOCTL_SCANTIME            (IOCTL_VIRDEV_MASK | 4)       /* Foreground Active Scan Time (T8) */          
#define IEEE80211_IOCTL_SCAN                (IOCTL_VIRDEV_MASK | 5)       /* Foreground Active Scan */          

typedef enum athcfg_wcmd_type{
    ATHCFG_WCMD_GET_DEV_PRODUCT_INFO = (ATH_IOCTL_PRODINFO           | IOCTL_GET_MASK),
    ATHCFG_WCMD_GET_REG              = (ATH_IOCTL_REGRW              | IOCTL_GET_MASK),        
    ATHCFG_WCMD_SET_REG              = (ATH_IOCTL_REGRW              | IOCTL_SET_MASK),
    ATHCFG_WCMD_GET_DEV_VERSION_INFO = (ATH_IOCTL_VERSINFO           | IOCTL_GET_MASK),
    ATHCFG_WCMD_GET_TX_POWER         = (ATH_IOCTL_TX_POWER           | IOCTL_GET_MASK),    
    ATHCFG_WCMD_GET_STATS            = (ATH_IOCTL_STATS              | IOCTL_GET_MASK),    
    ATHCFG_WCMD_GET_TESTMODE         = (IEEE80211_IOCTL_TESTMODE     | IOCTL_GET_MASK),
    ATHCFG_WCMD_SET_TESTMODE         = (IEEE80211_IOCTL_TESTMODE     | IOCTL_SET_MASK),
    ATHCFG_WCMD_GET_STAINFO          = (IEEE80211_IOCTL_STAINFO      | IOCTL_GET_MASK),
    ATHCFG_WCMD_GET_MODE             = (IEEE80211_IOCTL_PHYMODE      | IOCTL_GET_MASK),
    ATHCFG_WCMD_GET_SCANTIME         = (IEEE80211_IOCTL_SCANTIME     | IOCTL_GET_MASK),
    ATHCFG_WCMD_GET_SCAN             = (IEEE80211_IOCTL_SCAN         | IOCTL_GET_MASK),
    ATHCFG_WCMD_SET_SCAN             = (IEEE80211_IOCTL_SCAN         | IOCTL_SET_MASK),
}athcfg_wcmd_type_t;

/**
 * ***************************Structures***********************
 */
/**
 * brief get connection type
 */
typedef enum athcfg_wcmd_phymode{   
    ATHCFG_WCMD_PHYMODE_AUTO,               /* autoselect */
    ATHCFG_WCMD_PHYMODE_11A,                /* 5GHz, OFDM */
    ATHCFG_WCMD_PHYMODE_11B,                /* 2GHz, CCK  */
    ATHCFG_WCMD_PHYMODE_11G,                /* 2GHz, OFDM */
    ATHCFG_WCMD_PHYMODE_11NA_HT20,          /* 5GHz, HT20 */
    ATHCFG_WCMD_PHYMODE_11NG_HT20,          /* 2GHz, HT20 */
    ATHCFG_WCMD_PHYMODE_11NA_HT40,          /* 5GHz, HT40 */    
    ATHCFG_WCMD_PHYMODE_11NG_HT40,          /* 2GHz, HT40 */
}athcfg_wcmd_phymode_t;

/**
 * @brief get station-info
 */
typedef struct athcfg_wcmd_sta{
#define ATHCFG_WCMD_STAINFO_LINKUP        0x1 
#define ATHCFG_WCMD_STAINFO_SHORTGI       0x2
    a_uint32_t                    flags;                       /* Flags */
    athcfg_wcmd_phymode_t         phymode;                     /* STA Connection Type */
    a_uint8_t                     bssid[ATHCFG_WCMD_ADDR_LEN]; /* STA Current BSSID */
    a_uint32_t                    assoc_time;                  /* STA association time */
    a_uint32_t                    wpa_4way_handshake_time;     /* STA 4-way time */
    a_uint32_t                    wpa_2way_handshake_time;     /* STA 2-way time */  
    a_uint32_t                    rx_rate_kbps;                /* STA latest RX Rate (Kbps) */    
#define ATHCFG_WCMD_STAINFO_MCS_NULL      0xff    
    a_uint8_t                     rx_rate_mcs;                 /* STA latest RX Rate MCS (11n) */    
    a_uint32_t                    tx_rate_kbps;                /* STA latest TX Rate (Kbps) */    
    a_uint8_t                     rx_rssi;                     /* RSSI of all received frames */
    a_uint8_t                     rx_rssi_beacon;              /* RSSI of Beacon */
    /* TBD : others information */
}athcfg_wcmd_sta_t;

/**
 * @brief get scan-time (T8)
 */
typedef struct athcfg_wcmd_scantime{
    a_int32_t   scan_time;          /* Fully Channel Scan Processing Time */
} athcfg_wcmd_scantime_t ;

/**
 * @brief get version-info
 */
typedef struct athcfg_wcmd_version_info{
    a_int8_t    driver[32];         /* Driver Short Name */
    a_int8_t    version[32];        /* Driver Version */
    a_int8_t    fw_version[32];     /* firmware Version */
} athcfg_wcmd_version_info_t ;

/**
 * @brief get txpower-info
 */
typedef struct athcfg_wcmd_txpower{   
#define ATHCFG_WCMD_TX_POWER_TABLE_SIZE 48//32
    a_uint8_t                       txpowertable[ATHCFG_WCMD_TX_POWER_TABLE_SIZE];
}athcfg_wcmd_txpower_t;

/**
 * @brief get stats-info
 */
typedef struct athcfg_wcmd_stats{   
    a_uint64_t                       txPackets;       /* Total TX Packets */
    a_uint64_t                       txRetry;         /* Total TX Retry */
    a_uint64_t                       txAggrRetry;     /* Total TX Aggr-Pkt Retry */
    a_uint64_t                       txAggrSubRetry;  /* Total TX Sub-Frame Retry */    
    a_uint64_t                       txAggrExcessiveRetry;     /* Total TX Aggr-Pkt Excessive Retry */
    a_uint64_t                       txSubRetry;     /* Total Subframe Retry */
}athcfg_wcmd_stats_t;

/**
 * @brief  set/get testmode-info
 */
typedef struct  athcfg_wcmd_testmode{
    a_uint8_t     bssid[ATHCFG_WCMD_ADDR_LEN];
    a_int32_t     chan;         /* ChanID */

#define ATHCFG_WCMD_TESTMODE_CHAN     0x1    
#define ATHCFG_WCMD_TESTMODE_BSSID    0x2    
#define ATHCFG_WCMD_TESTMODE_RX       0x3    
#define ATHCFG_WCMD_TESTMODE_RESULT   0x4    
#define ATHCFG_WCMD_TESTMODE_ANT      0x5    
    a_uint16_t    operation;    /* Operation */
    a_uint8_t     antenna;      /* RX-ANT */
    a_uint8_t     rx;           /* RX START/STOP */
    a_int32_t     rssi_combined;/* RSSI */
    a_int32_t     rssi0;        /* RSSI */
    a_int32_t     rssi1;        /* RSSI */
    a_int32_t     rssi2;        /* RSSI */
} athcfg_wcmd_testmode_t;

/**
 * @brief Information Element
 */
typedef struct athcfg_ie_info {
    a_uint16_t         len;
    a_uint8_t          data[ATHCFG_WCMD_IE_MAXLEN];
}athcfg_ie_info_t;

/**
 * @brief Scan result data returned
 */
typedef struct athcfg_wcmd_scan_result {
    a_uint16_t  isr_freq;                             /* Frequency */
    a_uint8_t   isr_ieee;                             /* IEEE Channel */
    a_uint8_t   isr_rssi;                             /* Average RSSI */
    a_uint16_t  isr_capinfo;                          /* capabilities */
    a_uint8_t   isr_erp;                              /* ERP element */
    a_uint8_t   isr_bssid[ATHCFG_WCMD_ADDR_LEN]       /* BSSID */;
    a_uint8_t   isr_nrates;                          
    a_uint8_t   isr_rates[ATHCFG_WCMD_RATE_MAXSIZE];  /* Rates */
    a_uint8_t   isr_ssid_len;   
    a_uint8_t   isr_ssid[ATHCFG_WCMD_MAX_SSID];       /* SSID */

    athcfg_ie_info_t   isr_wpa_ie;                    /* WPA IE */
    athcfg_ie_info_t   isr_wme_ie;                    /* WMM IE */
    athcfg_ie_info_t   isr_ath_ie;                    /* ATH IE */
    athcfg_ie_info_t   isr_rsn_ie;                    /* RSN IE */
    athcfg_ie_info_t   isr_wps_ie;                    /* WPS IE */
    athcfg_ie_info_t   isr_htcap_ie;                  /* HTCAP IE */
    athcfg_ie_info_t   isr_htinfo_ie;                 /* HTINFO IE */
    a_uint8_t          isr_htcap_mcsset[ATHCFG_WCMD_MAX_HT_MCSSET]; 
} athcfg_wcmd_scan_result_t;

/**
 * @brief get scan-result
 */
typedef struct athcfg_wcmd_scan {
    athcfg_wcmd_scan_result_t   result[ATHCFG_WCMD_MAX_AP];
    a_uint32_t                  cnt;        /* entry for this run */
    a_uint8_t                   more;       /* need get again. */
    a_uint8_t                   offset;     /* offset of this run */
} athcfg_wcmd_scan_t;

/**
 * @brief get product-info
 */
typedef struct athcfg_wcmd_product_info {
    a_uint16_t idVendor;
    a_uint16_t idProduct;
    a_uint8_t  product[64];
    a_uint8_t  manufacturer[64];
    a_uint8_t  serial[64];
} athcfg_wcmd_product_info_t;

/**
 * @brief read/write register
 */ 
typedef struct athcfg_wcmd_reg{
    a_uint32_t addr;
    a_uint32_t val;
} athcfg_wcmd_reg_t;

/**
 * @brief Main wireless command data
 */
typedef union athcfg_wcmd_data{
    athcfg_wcmd_product_info_t      product_info;   
    athcfg_wcmd_testmode_t          testmode;
    athcfg_wcmd_reg_t               reg;
    athcfg_wcmd_sta_t               station_info;
    athcfg_wcmd_scantime_t          scantime;
    athcfg_wcmd_phymode_t           phymode;
    athcfg_wcmd_scan_t              *scan;
    athcfg_wcmd_version_info_t      version_info;
    athcfg_wcmd_txpower_t           txpower;
    athcfg_wcmd_stats_t             stats; 
} athcfg_wcmd_data_t;

/**
 * @brief ioctl structure to configure the wireless interface.
 */ 
typedef struct athcfg_wcmd{
    int                    iic_vendor;                          /* CMD Vendor */
    int                    iic_cmd;                             /* CMD Type */
    char                   iic_ifname[ATHCFG_WCMD_NAME_SIZE];   /* IF NAME */
    athcfg_wcmd_data_t     iic_data;                            /* CMD Data */       
} athcfg_wcmd_t;

/**
 * @brief helper macros
 */
#define d_productinfo           iic_data.product_info
#define d_testmode              iic_data.testmode
#define d_reg                   iic_data.reg
#define d_stainfo               iic_data.station_info
#define d_versioninfo           iic_data.version_info
#define d_scantime              iic_data.scantime
#define d_phymode               iic_data.phymode
#define d_txpower               iic_data.txpower
#define d_scan                  iic_data.scan
#define d_stats                 iic_data.stats
#endif  /* _IOCTL_VENDOR_SONY_H_ */
