/*
 * author:	wei.liu@amlogic.com
 * date:	2012-08-01
 * usage: 	security support for IPTV
 */ 

#ifndef __SECURITY_H
#define __SECURITY_H

#include "apis.h"

typedef struct security_ts_s { // 时间戳, TS (TimeStamp), 14 bytes, yyyymmddhhmmss
  u8 data[14];
} security_ts_t;

typedef struct security_rn_s { // 随机数, RN (RandomNumber), 16 bytes
  u8 data[16];
} security_rn_t;


typedef struct security_sn_s { // SN, 24 bytes
  u8 factory_id[4];            // 厂商代码
  u8 product_model[4];         // 终端型号
  u8 product_batch[8];         // 产品批次
  u8 product_sn[8];            // 产品序号
} security_sn_t;

typedef struct security_me_s { // 云端挑战字, ME, 128 bytes
  u8 data[128];                // RSA1024解密算法中的Block 大小
} security_me_t;

typedef struct security_oi_s { // 运营商标识, OI (OperatorID), 4 bytes
  u8 data[4];
} security_oi_t;

typedef struct security_cd_s { // 云端挑战字明文, CD, 64 bytes
  security_oi_t oi;            // 运营商标识
  security_ts_t t1;            // 终端时间戳
  security_rn_t r1;            // 终端随机数
  security_ts_t t2;            // 云端时间戳
  security_rn_t r2;            // 云端随机数
} security_cd_t;

typedef struct security_iv_s { // 终端挑战字引文, IV, 34 bytes
  security_oi_t oi;            // 运营商标识
  security_ts_t t2;            // 云端时间戳
  security_rn_t r2;            // 云端随机数
} security_iv_t;

typedef struct security_xx_s { // 终端挑战字, XX, 32 bytes
  u8 data[32];
} security_xx_t;

typedef struct security_mac_s {
	u8 data[6];
} security_mac_t;

// ***************************************************************************
// *** IOCTL command definition **********************************************
// ***************************************************************************

#define SECURITY_IOC_MAGIC 'S'

#define SECURITY_IOC_G_SN       _IOR(SECURITY_IOC_MAGIC, 0x01, security_sn_t)
#define SECURITY_IOC_G_RN       _IOR(SECURITY_IOC_MAGIC, 0x02, security_rn_t)
// #define SECURITY_IOC_S_ME    _IOW(SECURITY_IOC_MAGIC, 0x03, security_me_t)
// #define SECURITY_IOC_G_CD    _IOR(SECURITY_IOC_MAGIC, 0x04, security_cd_t)
// #define SECURITY_IOC_S_IV    _IOW(SECURITY_IOC_MAGIC, 0x05, security_iv_t)
// #define SECURITY_IOC_G_XX    _IOR(SECURITY_IOC_MAGIC, 0x06, security_xx_t)
#define SECURITY_IOC_S_ME_G_CD  _IOWR(SECURITY_IOC_MAGIC, 0x03, security_me_t)
#define SECURITY_IOC_S_IV_G_XX  _IOWR(SECURITY_IOC_MAGIC, 0x05, security_iv_t)
#define SECURITY_IOC_B_SN       _IOW(SECURITY_IOC_MAGIC, 0x07, security_sn_t)
#define SECURITY_IOC_B_IN       _IOW(SECURITY_IOC_MAGIC, 0x08, security_in_t)
//#define SECURITY_IOC_U_RA     _IOW(SECURITY_IOC_MAGIC, 0x09, security_ra_t)
#define SECURITY_IOC_S_PR       _IOW(SECURITY_IOC_MAGIC, 0x0A, signed int) // set procedure
#define SECURITY_IOC_D_HS       _IOW(SECURITY_IOC_MAGIC, 0x0B, signed int)  // decrypt hash table
#define SECURITY_IOC_G_HS       _IOR(SECURITY_IOC_MAGIC, 0x0C, signed int)
#define SECURITY_IOC_S_HS_LEN   _IOW(SECURITY_IOC_MAGIC, 0x0D, signed int) // set hashtable legnth
#define SECURITY_IOC_G_HS_LEN   _IOR(SECURITY_IOC_MAGIC, 0x0E, signed int)
#define SECURITY_IOC_S_ME_LEN   _IOW(SECURITY_IOC_MAGIC, 0x0F, signed int)
#define SECURITY_IOC_G_CD_LEN   _IOR(SECURITY_IOC_MAGIC, 0x10, signed int)
#define SECURITY_IOC_S_IV_LEN   _IOW(SECURITY_IOC_MAGIC, 0x11, signed int)
#define SECURITY_IOC_G_XX_LEN   _IOR(SECURITY_IOC_MAGIC, 0x12, signed int)
#define SECURITY_IOC_G_IN_STU   _IOR(SECURITY_IOC_MAGIC, 0x13, signed int) // get in status
#define SECURITY_IOC_B_SN_RAW	_IOW(SECURITY_IOC_MAGIC, 0x14, security_sn_t)
#define SECURITY_IOC_B_IN_RAW	_IOW(SECURITY_IOC_MAGIC, 0x15, security_in_t)
#define SECURITY_IOC_B_MAC	 	_IOW(SECURITY_IOC_MAGIC, 0x16, security_mac_t)
#define SECURITY_IOC_G_MAC		_IOR(SECURITY_IOC_MAGIC, 0x17, security_mac_t)
// get hash256 when burn the chip
#define SECURITY_IOC_G_SH_256	_IOWR(SECURITY_IOC_MAGIC, 0x18, signed int)

#define SECURITY_DEVICE_NODE_NAME  "security"
#define SECURITY_DEVICE_FILE_NAME  "security_device"
#define SECURITY_DEVICE_PROC_NAME  "security"
#define SECURITY_DEVICE_CLASS_NAME "security_class"

#define SECURITY_SN_LENGTH 24
#define SECURITY_RN_LENGTH 16
#define SECURITY_ME_LENGTH 128
#define SECURITY_CD_LENGTH 64
#define SECURITY_IV_LENGTH 34
#define SECURITY_XX_LENGTH 32
#define SECURITY_TS_LENGTH 14
#define SECURITY_OI_LENGTH 4

typedef int(*function)(char *pSRC, int nDataLen,char *pOUT,int *pOUTLen,int procedure, int cmd);

#endif // __SECURITY_H

