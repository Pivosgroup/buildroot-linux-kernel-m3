#include <linux/types.h>
#include "efuse_regs.h"
#include <linux/efuse.h>


/**
 * efuse version 0.1 (for M3 ) 
 * M3 efuse: read all free efuse data maybe fail on addr 0 and addr 0x40
 * so M3 EFUSE layout avoid using 0 and 0x40
title				offset			datasize			checksize			totalsize			
reserved 		0					0						0						4
usid				4					33					2						35
mac_wifi		39				6						1						7
mac_bt		46				6						1						7
mac				53				6						1						7
licence			60				3						1						4
reserved 		64				0						0						4
hdcp			68				300					10					310
reserved		378				0						0						2
version		380				3						1						4    (version+machid, version=1)
*/


static efuseinfo_item_t efuseinfo_v1[] =
{
	{
		.title = "usid",
		.id = EFUSE_USID_ID,
		.offset = V1_EFUSE_USID_OFFSET, //4,		
		.data_len = V1_EFUSE_USID_DATA_LEN, //35,				
	},	
	{
		.title = "mac_wifi",
		.id = EFUSE_MAC_WIFI_ID,
		.offset = 39,		
		.data_len = 6,				
	},
	{
		.title = "mac_bt",
		.id = EFUSE_MAC_BT_ID,
		.offset = 46,		
		.data_len = 6,				
	},
	{
		.title = "mac",
		.id = EFUSE_MAC_ID,
		.offset = 53,		
		.data_len = 6,			
	},
	{
		.title = "licence",
		.id = EFUSE_LICENCE_ID,
		.offset = 60,		
		.data_len = 3,		
	},	
	{
		.title = "hdcp",
		.id = EFUSE_HDCP_ID,
		.offset = 68,		
		.data_len = 300,		
	},
	{
		.title= "version",     //1B(version=1)+2B(machid)
		.id = EFUSE_VERSION_ID,
		.offset=EFUSE_VERSION_OFFSET, //380,		
		.data_len = EFUSE_VERSION_DATA_LEN, //3,		
	},
};

efuseinfo_t efuseinfo[] = 
{
	{
		.efuseinfo_version = efuseinfo_v1,
		.size = sizeof(efuseinfo_v1)/sizeof(efuseinfo_item_t),
		.version =1,
		.bch_en = 1,
	},

};

int efuseinfo_num = sizeof(efuseinfo)/sizeof(efuseinfo_t);
int efuse_active_version = -1;
unsigned efuse_active_customerid = 0;
pfn efuse_getinfoex = 0;
pfn efuse_getinfoex_byPos=0;

