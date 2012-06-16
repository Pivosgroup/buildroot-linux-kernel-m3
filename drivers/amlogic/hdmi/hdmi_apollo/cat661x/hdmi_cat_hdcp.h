#ifndef __HDMI_HDCP_CAT_H__
#define __HDMI_HDCP_CAT_H__
#include "hdmi_global.h"

#define HDCPRX_SLV 0x74
    
//--------------------------------------------------------------
// HDCP DDC side (HDCP receiver or repearer)
    
#define DDC_BKSV_ADDR       0x00	// 5 bytes, Read only (RO), receiver KSV
#define DDC_Ri_ADDR         0x08	// Ack from receiver RO
#define DDC_AKSV_ADDR       0x10	// 5 bytes WO, transmitter KSV
#define DDC_AN_ADDR         0x18	// 8 bytes WO, random value.
#define DDC_V_ADDR          0x20	// 20 bytes RO
#define DDC_RSVD_ADDR       0x34	// 12 byte RO
#define DDC_BCAPS_ADDR      0x40	// Capabilities Status byte RO
    
#define DDC_BIT_HDMI_CAP    0x80
#define DDC_BIT_REPEATER    0x40
#define DDC_BIT_FIFO_READY  0x20
#define DDC_BIT_FAST_I2C    0x10
    
#define DDC_BSTATUS_ADDR         0x41
#define DDC_BSTATUS_1_ADDR       0x41
#define DDC_BSTATUS_2_ADDR       0x42
    
#define DDC_BIT_HDMI_MODE        0x10
    
#define DDC_KSV_FIFO_ADDR        0x43
extern void CAT_HDCP_ResumeAuthentication(Hdmi_info_para_t * info);
extern int CAT_EnableHDCP(Hdmi_info_para_t * info, int bEnable);
extern int CAT_HDCP_Authenticate(Hdmi_info_para_t * info);
extern int CAT_HDCP_ReadRi(void);

#endif	/*  */
    
