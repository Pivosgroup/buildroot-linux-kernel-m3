#ifndef __EFUSE_H
#define __EFUSE_H

#include <linux/ioctl.h>

#define EFUSE_ENCRYPT_DISABLE   _IO('f', 0x10)
#define EFUSE_ENCRYPT_ENABLE    _IO('f', 0x20)
#define EFUSE_ENCRYPT_RESET     _IO('f', 0x30)
#define EFUSE_INFO_GET				_IO('f', 0x40)

#ifdef __DEBUG
#define __D(fmt, args...) fprintf(stderr, "debug: " fmt, ## args)
#else
#define __D(fmt, args...)
#endif

#ifdef __ERROR
#define __E(fmt, args...) fprintf(stderr, "error: " fmt, ## args)
#else
#define __E(fmt, args...)
#endif



#define BCH_T           1
#define BCH_M           8
#define BCH_P_BYTES     30

#define EFUSE_BITS             3072
#define EFUSE_BYTES            384  //(EFUSE_BITS/8)
#define EFUSE_DWORDS            96  //(EFUSE_BITS/32)

#define DOUBLE_WORD_BYTES        4
#define EFUSE_IS_OPEN           (0x01)

#define EFUSE_NONE_ID			0
#define EFUSE_VERSION_ID		1
#define EFUSE_LICENCE_ID		2
#define EFUSE_MAC_ID				3
#define EFUSE_MAC_WIFI_ID	4
#define EFUSE_MAC_BT_ID		5
#define EFUSE_HDCP_ID			6
#define EFUSE_USID_ID				7

void efuse_bch_enc(const char *ibuf, int isize, char *obuf, int reverse);
void efuse_bch_dec(const char *ibuf, int isize, char *obuf, int reverse);

struct efuse_platform_data {
	loff_t pos;
	size_t count;
	bool (*data_verify)(unsigned char *usid);
};

typedef struct efuseinfo_item{
	char title[40];	
	unsigned id;
	unsigned offset;    // write offset	
	unsigned data_len;		
} efuseinfo_item_t;


typedef struct efuseinfo{
	struct efuseinfo_item *efuseinfo_version;
	int size;
	int version;
	int bch_en;    // bch enable 
}efuseinfo_t;

typedef int (*pfn) (unsigned id, efuseinfo_item_t *info); 

#endif
