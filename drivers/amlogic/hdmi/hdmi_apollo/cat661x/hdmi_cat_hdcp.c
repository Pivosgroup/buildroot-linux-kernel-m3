
#include <linux/version.h> 
#include <linux/kernel.h>
#include <linux/string.h>
    
#include "hdmi_debug.h"
    
#include "hdmi_cat_defstx.h"
#include "hdmi_i2c.h"
#include "hdmi_cat_hdcp.h"
#include "hdmi_cat_mddc.h"
#include "hdmi_cat_interrupt.h"
    
#define rol(x,y) (((x) << (y)) | (((unsigned long)x) >> (32-y)))
extern int hdmi_cat6611_SetAVmute(Hdmi_info_para_t * info);
extern int hdmi_cat6611_ClearAVmute(Hdmi_info_para_t * info);
static unsigned char KSVList[32];
static unsigned char Vr[20];
static unsigned char M0[8];
static unsigned char SHABuff[64];
static unsigned char V[20];
static unsigned long w[80];
static unsigned long digest[5];
static void CAT_HDCP_ClearAuthInterrupt(void) 
{
	unsigned char temp_data[3];
	temp_data[0] = 0;
	temp_data[1] = B_KSVLISTCHK_MASK | B_AUTH_DONE_MASK | B_AUTH_FAIL_MASK;
	temp_data[2] = 0;
	CAT_Enable_Interrupt_Masks(temp_data);
	WriteByteHDMITX_CAT(REG_INT_CLR0, B_CLR_AUTH_FAIL | B_CLR_AUTH_DONE | B_CLR_KSVLISTCHK);	//Write 1 to clear related bit.
	WriteByteHDMITX_CAT(REG_INT_CLR1, 0);
	
	    //To clear all interrupt indicated by reg0C and reg0D bits set as 1, write reg0E[0] = 1, then write reg0E[0] = 0.
	    WriteByteHDMITX_CAT(REG_SYS_STATUS, B_INTACTDONE);
} static void CAT_HDCP_Reset(void) 
{
	unsigned char uc;
	WriteByteHDMITX_CAT(REG_LISTCTRL, 0);
	WriteByteHDMITX_CAT(REG_HDCP_DESIRE, 0);	// bit0 = 0: disable HDCP; bit1 = 0: not support HDCP 1.1 feature,
	uc = ReadByteHDMITX_CAT(REG_SW_RST);
	uc |= B_HDCP_RST;
	WriteByteHDMITX_CAT(REG_SW_RST, uc);	//enalbe HDCP reset
	
	    //Switch HDCP controller or PC host to command the DDC port: 0: HDCP; 1: PC
	    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERDDC | B_MASTERHOST);	//switch PC 
	CAT_HDCP_ClearAuthInterrupt();
	CAT_AbortDDC();
} int CAT_HDCP_ReadRi(void) 
{
	unsigned char ucdata, TimeOut;
	unsigned char mddc_info[4];
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	mddc_info[0] = B_MASTERDDC | B_MASTERHOST;	//Switch HDCP controller or PC host to command the DDC port: 0: HDCP; 1: PC
	mddc_info[1] = DDC_HDCP_ADDRESS;
	mddc_info[2] = DDC_Ri_ADDR;	//Register address( BCaps offset )
	mddc_info[3] = 2;
	WriteBlockHDMITX_CAT(REG_DDC_MASTER_CTRL, 4, mddc_info);
	
//    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
//    WriteByteHDMITX_CAT(REG_DDC_HEADER,DDC_HDCP_ADDRESS) ;
//    WriteByteHDMITX_CAT(REG_DDC_REQOFF,0x08) ; // Ri offset
//    WriteByteHDMITX_CAT(REG_DDC_REQCOUNT,2) ;
	    WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_DDC_SEQ_BURSTREAD);
	for (TimeOut = 200; TimeOut > 0; TimeOut--)
		 {
		AVTimeDly(1);
		ucdata = ReadByteHDMITX_CAT(REG_DDC_STATUS);
		if (ucdata & B_DDC_DONE)
			 {
			hdmi_dbg_print("HDCP_ReadRi(): DDC Done.\n");
			return 0;
			}
		if (ucdata & B_DDC_ERROR)
			 {
			hdmi_dbg_print("HDCP_ReadRi(): DDC error.\n");
			return -1;
			}
		}
	return 0;
}


//////////////////////////////////////////////////////////////////////
// Function: CAT_HDCP_GetBCaps
// Parameter: pBCaps - pointer of byte to get BCaps.
//            pBStatus - pointer of two bytes to get BStatus
// Return: 0 if successfully got BCaps and BStatus.
// Remark: get B status and capability from HDCP reciever via DDC bus.
//////////////////////////////////////////////////////////////////////
static int CAT_HDCP_GetBCaps(unsigned char *pBCaps, unsigned short *pBStatus) 
{
	unsigned char ucdata, TimeOut;
	unsigned char mddc_info[4];
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	mddc_info[0] = B_MASTERDDC | B_MASTERHOST;	//Switch HDCP controller or PC host to command the DDC port: 0: HDCP; 1: PC
	mddc_info[1] = DDC_HDCP_ADDRESS;
	mddc_info[2] = DDC_BCAPS_ADDR;	//Register address( BCaps offset )
	mddc_info[3] = 3;
	WriteBlockHDMITX_CAT(REG_DDC_MASTER_CTRL, 4, mddc_info);
	
//    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERDDC|B_MASTERHOST) ;     
//    WriteByteHDMITX_CAT(REG_DDC_HEADER, DDC_HDCP_ADDRESS) ;
//    WriteByteHDMITX_CAT(REG_DDC_REQOFF, DDC_BCAPS_ADDR) ; 
//    WriteByteHDMITX_CAT(REG_DDC_REQCOUNT,3) ;
	    WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_DDC_SEQ_BURSTREAD);	//PC DDC request command
	for (TimeOut = 200; TimeOut > 0; TimeOut--)
		 {
		AVTimeDly(1);
		ucdata = ReadByteHDMITX_CAT(REG_DDC_STATUS);
		if (ucdata & B_DDC_DONE)
			 {
			hdmi_dbg_print("HDCP_GetBCaps(): DDC Done.\n");
			
//            printf("HDCP_GetBCaps(): DDC Done.\n"); 
			    break;
			}
		if (ucdata & B_DDC_ERROR)
			 {
			hdmi_dbg_print("HDCP_GetBCaps(): DDC fail by reg16\n");
			
//            printf("HDCP_GetBCaps(): DDC fail by reg16=%02X.\n",ucdata) ;
			    CAT_ClearDDCFIFO();
			return -1;
			}
		}
	if (TimeOut == 0)
		 {
		hdmi_dbg_print("HDCP_GetBCaps(): TimeOut\n");
		
//        printf("HDCP_GetBCaps(): TimeOut\n") ;
		    return -1;
		}
	
	else			//read the datas
	{
	}
	AVTimeDly(1);
	ReadBlockHDMITX_CAT(REG_BSTAT, 2, mddc_info);	//read Rx status from reg44/45
	*pBStatus = (unsigned short)(mddc_info[0] | (mddc_info[1] << 8));
	mddc_info[0] = ReadByteHDMITX_CAT(REG_BCAP);	//Rx capability from reg43.
	*pBCaps = mddc_info[0];
	return 0;
}


//////////////////////////////////////////////////////////////////////
// Function: CAT_HDCP_GetBKSV
// Parameter: pBKSV - pointer of 5 bytes buffer for getting BKSV
// Return: 0 if successfuly got BKSV from Rx.
// Remark: Get BKSV from HDCP reciever.
//////////////////////////////////////////////////////////////////////
static int CAT_HDCP_GetBKSV(Hdmi_info_para_t * info, unsigned char *pBKSV) 
{
	unsigned char ucdata, TimeOut, i, j, Mask, Ones;
	unsigned char mddc_info[4];
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	mddc_info[0] = B_MASTERDDC | B_MASTERHOST;	//Switch HDCP controller or PC host to command the DDC port: 0: HDCP; 1: PC
	mddc_info[1] = DDC_HDCP_ADDRESS;
	mddc_info[2] = DDC_BKSV_ADDR;	// BKSV offset
	mddc_info[3] = 5;
	WriteBlockHDMITX_CAT(REG_DDC_MASTER_CTRL, 4, mddc_info);
	
//    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERDDC|B_MASTERHOST);    
//    WriteByteHDMITX_CAT(REG_DDC_HEADER, DDC_HDCP_ADDRESS) ;
//    WriteByteHDMITX_CAT(REG_DDC_REQOFF, DDC_BKSV_ADDR) ; 
//    WriteByteHDMITX_CAT(REG_DDC_REQCOUNT,5) ;
	    WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_DDC_SEQ_BURSTREAD);
	for (TimeOut = 200; TimeOut > 0; TimeOut--)
		 {
		AVTimeDly(1);
		ucdata = ReadByteHDMITX_CAT(REG_DDC_STATUS);
		if (ucdata & B_DDC_DONE)
			 {
			hdmi_dbg_print("HDCP_GetBKSV(): DDC Done.\n");
			
//            printf("HDCP_GetBKSV(): DDC Done.\n") ;
			    break;
			}
		if (ucdata & B_DDC_ERROR)
			 {
			hdmi_dbg_print
			    ("CAT_HDCP_GetBKSV(): DDC No ack or arbilose, maybe cable did not connected. Fail.\n");
			
//            printf("CAT_HDCP_GetBKSV(): DDC No ack or arbilose, maybe cable did not connected. Fail.\n") ;
			    return -1;
			}
		}
	if (TimeOut == 0)
		 {
		return -1;
		}
	ReadBlockHDMITX_CAT(REG_BKSV, 5, pBKSV);
	for (i = 0; i < 5; i++)
		 {
		if (pBKSV[i] == 0xFF)
			 {
			info->auth_state = HDCP_AUTHENTICATED;
			return 0;
			}
		}
	Ones = 0;
	for (i = 0; i < 5; i++)
		 {
		Mask = 0x01;
		for (j = 0; j < 8; j++) {
			if (pBKSV[i] & Mask)
				Ones++;
			Mask <<= 1;
		}
		}
	if (Ones == 20)
		return 0;
	
	else
		return -1;
}


//////////////////////////////////////////////////////////////////////
// Function: CAT_HDCP_Auth_Fire()
// Parameter: N/A
// Return: N/A
// Remark: write anything to reg21 to enable HDCP authentication by HW
//////////////////////////////////////////////////////////////////////
static void CAT_HDCP_Auth_Fire(void) 
{
	
	    //the DDC master switch to HDCP Tx core and all DDC communication will be automatically done   
	    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERDDC | B_MASTERHDCP);	// MASTERHDCP,no need command but fire.
	WriteByteHDMITX_CAT(REG_AUTHFIRE, 1);	//Write ¡®1¡¯ to stare HDCP authentication process
} 

//////////////////////////////////////////////////////////////////////
// Function: CAT_HDCP_GenerateAn
// Parameter: N/A
// Return: N/A
// Remark: start An ciper random run at first,then stop it. Software can get
//         an in reg30~reg38,the write to reg28~2F
// Side-Effect:
// To generate an available An, set Reg1F[0] as ¡®0¡¯ to stop cipher running, 
// and an new useful An will be generated in Reg30~Reg37.
//////////////////////////////////////////////////////////////////////
static void CAT_HDCP_GenerateAn(void) 
{
	unsigned char Data[8];
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	// switch bank action should start on direct register writting of each function.
	WriteByteHDMITX_CAT(REG_AN_GENERATE, B_START_CIPHER_GEN);	//to enable Cipher Hardware generating a random number.
	AVTimeDly(1);
	WriteByteHDMITX_CAT(REG_AN_GENERATE, B_STOP_CIPHER_GEN);	//to stop the Cipher Hardware.The generated Random number can be read back from register 0x30~0x37
	ReadBlockHDMITX_CAT(REG_AN_GEN, 8, Data);	// new An is ready from register 0x30~0x37
	WriteBlockHDMITX_CAT(REG_AN, 8, Data);	//write the new An into register 0x28~0x2f
} static void CAT_HDCP_CancelRepeaterAuthenticate(void) 
{
	hdmi_dbg_print("CAT_HDCP_CancelRepeaterAuthenticate. \n");
	WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERDDC | B_MASTERHOST);	//switch PC
	CAT_AbortDDC();
	WriteByteHDMITX_CAT(REG_LISTCTRL, B_LISTFAIL | B_LISTDONE);	//Write bit0 ¡®1¡¯ after PC check KSV FIFO list. bit1: HDCP Authentication FSM will return to wait and try state.
	CAT_HDCP_ClearAuthInterrupt();
} static int CAT_HDCP_GetKSVList(unsigned char *pKSVList,
				     unsigned char cDownStream) 
{
	unsigned char ucdata, TimeOut = 100;
	unsigned char mddc_info[4];
	if (cDownStream == 0 || pKSVList == NULL)
		 {
		return -1;
		}
	mddc_info[0] = B_MASTERHOST;	//Switch HDCP controller or PC host to command the DDC port: 0: HDCP; 1: PC
	mddc_info[1] = DDC_HDCP_ADDRESS;
	mddc_info[2] = REG_BCAP;
	mddc_info[3] = cDownStream * 5;
	WriteBlockHDMITX_CAT(REG_DDC_MASTER_CTRL, 4, mddc_info);
	
//    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERHOST) ;
//    WriteByteHDMITX_CAT(REG_DDC_HEADER, DDC_HDCP_ADDRESS) ;
//    WriteByteHDMITX_CAT(REG_DDC_REQOFF, REG_BCAP) ;
//    WriteByteHDMITX_CAT(REG_DDC_REQCOUNT, cDownStream * 5) ;
	    WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_DDC_SEQ_BURSTREAD);
	for (TimeOut = 200; TimeOut > 0; TimeOut--)
		 {
		ucdata = ReadByteHDMITX_CAT(REG_DDC_STATUS);
		if (ucdata & B_DDC_DONE)
			 {
			hdmi_dbg_print("HDCP_GetKSVList(): DDC Done.\n");
			
//            printf("HDCP_GetKSVList(): DDC Done.\n") ;
			    break;
			}
		if (ucdata & B_DDC_ERROR)
			 {
			hdmi_dbg_print
			    ("HDCP_GetKSVList(): DDC Fail by REG_DDC_STATUS \n");
			
//            printf("HDCP_GetKSVList(): DDC Fail by REG_DDC_STATUS \n") ;
			    CAT_ClearDDCFIFO();
			return -1;
			}
		AVTimeDly(5);
		}
	if (TimeOut == 0)
		 {
		return -1;
		}
	hdmi_dbg_print("HDCP_GetKSVList(): KSV \n");
	
//    printf("HDCP_GetKSVList(): KSV ") ;
	    for (TimeOut = 0; TimeOut < cDownStream * 5; TimeOut++)
		 {
		pKSVList[TimeOut] = ReadByteHDMITX_CAT(REG_DDC_READFIFO);	//There are 32 DDC FIFO, which can read back from the byte.
//        printf(" %02X",pKSVList[TimeOut]) ;
		}
	
//  printf("\n"); 
	    return 0;
}
static int CAT_HDCP_GetVr(unsigned char *pVr) 
{
	unsigned char TimeOut, ucdata;
	unsigned char mddc_info[4];
	if (pVr == NULL)
		 {
		return -1;
		}
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	// switch bank action should start on direct register writting of each function.
	mddc_info[0] = B_MASTERHOST;	//Switch HDCP controller or PC host to command the DDC port: 0: HDCP; 1: PC, switch PC
	mddc_info[1] = DDC_HDCP_ADDRESS;
	mddc_info[2] = DDC_V_ADDR;
	mddc_info[3] = 20;
	WriteBlockHDMITX_CAT(REG_DDC_MASTER_CTRL, 4, mddc_info);
	
//    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERHOST) ;   
//    WriteByteHDMITX_CAT(REG_DDC_HEADER, DDC_HDCP_ADDRESS) ;
//    WriteByteHDMITX_CAT(REG_DDC_REQOFF, REG_HDCP_DESIRE) ;  //// 20 bytes RO
//    WriteByteHDMITX_CAT(REG_DDC_REQCOUNT, 20) ;
	    WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_DDC_SEQ_BURSTREAD);
	for (TimeOut = 200; TimeOut > 0; TimeOut--)
		 {
		ucdata = ReadByteHDMITX_CAT(REG_DDC_STATUS);
		if (ucdata & B_DDC_DONE)
			 {
			hdmi_dbg_print("HDCP_GetVr(): DDC Done.\n");
			
//            printf("HDCP_GetVr(): DDC Done.\n") ;
			    break;
			}
		if (ucdata & B_DDC_ERROR)
			 {
			hdmi_dbg_print
			    ("HDCP_GetVr(): DDC fail by REG_DDC_STATUS \n");
			
//            printf("HDCP_GetVr(): DDC fail by REG_DDC_STATUS \n") ;
			    CAT_ClearDDCFIFO();
			return -1;
			}
		AVTimeDly(5);
		}
	if (TimeOut == 0)
		 {
		hdmi_dbg_print("HDCP_GetVr(): DDC fail by timeout.\n");
		
//        printf("HDCP_GetVr(): DDC fail by timeout.\n") ;
		    CAT_ClearDDCFIFO();
		return -1;
		}
	for (TimeOut = 0; TimeOut < 5; TimeOut++)
		 {
		WriteByteHDMITX_CAT(REG_SHA_SEL, TimeOut);
		pVr[TimeOut * 4 + 3] =
		    (unsigned long)ReadByteHDMITX_CAT(REG_SHA_RD_BYTE1);
		pVr[TimeOut * 4 + 2] =
		    (unsigned long)ReadByteHDMITX_CAT(REG_SHA_RD_BYTE2);
		pVr[TimeOut * 4 + 1] =
		    (unsigned long)ReadByteHDMITX_CAT(REG_SHA_RD_BYTE3);
		pVr[TimeOut * 4] =
		    (unsigned long)ReadByteHDMITX_CAT(REG_SHA_RD_BYTE4);
		
//                  printf("V' = %02X %02X %02X %02X\n",pVr[TimeOut*4],pVr[TimeOut*4+1],pVr[TimeOut*4+2],pVr[TimeOut*4+3]) ;         
		} return 0;
}
static int CAT_HDCP_GetM0(unsigned char *pM0) 
{
	if (!pM0)
		 {
		return -1;
		}
	WriteByteHDMITX_CAT(REG_SHA_SEL, 5);	// read m0[31:0] from reg51~reg54
	ReadBlockHDMITX_CAT(REG_SHA_RD_BYTE1, 4, pM0);
	WriteByteHDMITX_CAT(REG_SHA_SEL, 0);	// read m0[39:32] from reg55
	pM0[4] = ReadByteHDMITX_CAT(REG_AKSV_RD_BYTE5);
	WriteByteHDMITX_CAT(REG_SHA_SEL, 1);	// read m0[47:40] from reg55
	pM0[5] = ReadByteHDMITX_CAT(REG_AKSV_RD_BYTE5);
	WriteByteHDMITX_CAT(REG_SHA_SEL, 2);	// read m0[55:48] from reg55
	pM0[6] = ReadByteHDMITX_CAT(REG_AKSV_RD_BYTE5);
	WriteByteHDMITX_CAT(REG_SHA_SEL, 3);	// read m0[63:56] from reg55
	pM0[7] = ReadByteHDMITX_CAT(REG_AKSV_RD_BYTE5);
	
//    printf("M[] =") ;
//        for(int i = 0 ; i < 8 ; i++){
//                printf("0x%02x,",pM0[i]) ;
//        }
//      printf("\n") ;
	    return 0;
}
static void SHATransform(unsigned long *h) 
{
	unsigned long t;
	for (t = 16; t < 80; t++)
		 {
		unsigned long tmp =
		    w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16];
		w[t] = rol(tmp, 1);
		} h[0] = 0x67452301;
	h[1] = 0xefcdab89;
	h[2] = 0x98badcfe;
	h[3] = 0x10325476;
	h[4] = 0xc3d2e1f0;
	for (t = 0; t < 20; t++) {
		unsigned long tmp =
		    rol(h[0],
			5) + ((h[1] & h[2]) | (h[3] & ~h[1])) + h[4] + w[t] +
		    0x5a827999;
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1], 30);
		h[1] = h[0];
		h[0] = tmp;
	} for (t = 20; t < 40; t++) {
		unsigned long tmp =
		    rol(h[0],
			5) + (h[1] ^ h[2] ^ h[3]) + h[4] + w[t] + 0x6ed9eba1;
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1], 30);
		h[1] = h[0];
		h[0] = tmp;
	} for (t = 40; t < 60; t++) {
		unsigned long tmp =
		    rol(h[0],
			5) + ((h[1] & h[2]) | (h[1] & h[3]) | (h[2] & h[3])) +
		    h[4] + w[t] + 0x8f1bbcdc;
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1], 30);
		h[1] = h[0];
		h[0] = tmp;
	} for (t = 60; t < 80; t++) {
		unsigned long tmp =
		    rol(h[0],
			5) + (h[1] ^ h[2] ^ h[3]) + h[4] + w[t] + 0xca62c1d6;
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1], 30);
		h[1] = h[0];
		h[0] = tmp;
	} h[0] += 0x67452301;
	h[1] += 0xefcdab89;
	h[2] += 0x98badcfe;
	h[3] += 0x10325476;
	h[4] += 0xc3d2e1f0;
} 

/* ----------------------------------------------------------------------
 * Outer SHA algorithm: take an arbitrary length byte string,
 * convert it into 16-word blocks with the prescribed padding at
 * the end,and pass those blocks to the core SHA algorithm.
 */ 
static void SHA_Simple(void *p, unsigned long len, unsigned char *output) 
{
	int i, t;
	unsigned long c;
	char *pBuff = p;
	for (i = 0; i < len; i++)
		 {
		t = i / 4;
		if (i % 4 == 0)
			 {
			w[t] = 0;
			}
		c = pBuff[i];
		c <<= (3 - (i % 4)) * 8;
		w[t] |= c;
		}
	t = i / 4;
	if (i % 4 == 0)
		 {
		w[t] = 0;
		}
	c = 0x80 << ((3 - i % 4) * 24);
	w[t] |= c;
	t++;
	for (; t < 15; t++)
		 {
		w[t] = 0;
		}
	w[15] = len * 8;
	SHATransform(digest);
	for (i = 0; i < 20; i += 4)
		 {
		output[i] = (unsigned char)((digest[i / 4] >> 24) & 0xFF);
		output[i + 1] = (unsigned char)((digest[i / 4] >> 16) & 0xFF);
		output[i + 2] = (unsigned char)((digest[i / 4] >> 8) & 0xFF);
		output[i + 3] = (unsigned char)(digest[i / 4] & 0xFF);
} } static int CAT_HDCP_CheckSHA(unsigned char pM0[],
				      unsigned short BStatus,
				      unsigned char pKSVList[], int cDownStream,
				      unsigned char Vr[]) 
{
	int i, n;
	for (i = 0; i < cDownStream * 5; i++)
		 {
		SHABuff[i] = pKSVList[i];
		}
	SHABuff[i++] = BStatus & 0xFF;
	SHABuff[i++] = (BStatus >> 8) & 0xFF;
	for (n = 0; n < 8; n++, i++)
		 {
		SHABuff[i] = pM0[n];
		}
	n = i;
	for (; i < 64; i++)
		 {
		SHABuff[i] = 0;
		}
	SHA_Simple(SHABuff, n, V);
	for (i = 0; i < 20; i++)
		 {
		if (V[i] != Vr[i])
			 {
			return -1;
			}
		}
	return 0;
}
static void CAT_HDCP_ResumeRepeaterAuthenticate(void) 
{
	WriteByteHDMITX_CAT(REG_LISTCTRL, B_LISTDONE);
	WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERHDCP);	//switch HDCP 
} static int CAT_HDCP_Authenticate_Repeater(Hdmi_info_para_t * info) 
{
	unsigned char uc, cDownStream, BCaps;
	unsigned short BStatus, TimeOut;
	hdmi_dbg_print("Authentication for repeater\n");
	CAT_HDCP_GetBCaps(&BCaps, &BStatus);
	AVTimeDly(2);
	CAT_HDCP_Auth_Fire();
	AVTimeDly(550);	// emily add for test
	for (TimeOut = 250 * 6; TimeOut > 0; TimeOut--)
		 {
		uc = ReadByteHDMITX_CAT(REG_INT_STAT1);
		if (uc & B_INT_DDC_BUS_HANG)
			 {
			hdmi_dbg_print("DDC Bus hang\n");
			goto CAT_HDCP_Repeater_Fail;
			}
		uc = ReadByteHDMITX_CAT(REG_INT_STAT2);
		if (uc & B_INT_AUTH_FAIL)
			 {
			hdmi_dbg_print
			    ("CAT_HDCP_Authenticate_Repeater(): B_INT_AUTH_FAIL.\n");
			goto CAT_HDCP_Repeater_Fail;
			}
		if (uc & B_INT_KSVLIST_CHK)
			 {
			WriteByteHDMITX_CAT(REG_INT_CLR0, B_CLR_KSVLISTCHK);
			WriteByteHDMITX_CAT(REG_INT_CLR1, 0);
			
			    //To clear all interrupt indicated by reg0C and reg0D bits set as 1, write reg0E[0] =1, then write reg0E[0] = 0.
			    WriteByteHDMITX_CAT(REG_SYS_STATUS, B_INTACTDONE);
			WriteByteHDMITX_CAT(REG_SYS_STATUS, 0);
			hdmi_dbg_print("B_INT_KSVLIST_CHK \n");
			break;
			}
		AVTimeDly(5);
		}
	if (TimeOut == 0)
		 {
		hdmi_dbg_print
		    ("Time out for wait KSV List checking interrupt\n");
		goto CAT_HDCP_Repeater_Fail;
		}
	
	    ///////////////////////////////////////
	    // clear KSVList check interrupt.
	    ///////////////////////////////////////
	    for (TimeOut = 500; TimeOut > 0; TimeOut--)
		 {
		if (CAT_HDCP_GetBCaps(&BCaps, &BStatus) == -1)
			 {
			hdmi_dbg_print("Get BCaps fail\n");
			goto CAT_HDCP_Repeater_Fail;
			}
		if (BCaps & B_CAP_KSV_FIFO_RDY)
			 {
			hdmi_dbg_print("FIFO Ready\n");
			break;
			}
		AVTimeDly(5);
		}
	if (TimeOut == 0)
		 {
		hdmi_dbg_print("Get KSV FIFO ready TimeOut\n");
		goto CAT_HDCP_Repeater_Fail;
		}
	CAT_ClearDDCFIFO();
	CAT_GenerateDDCSCLK();
	cDownStream = (BStatus & M_DOWNSTREAM_COUNT);	//Total number of attached downstream string devices.
	if (cDownStream == 0 || cDownStream > 6
	     || BStatus & (B_MAX_CASCADE_EXCEEDED | B_DOWNSTREAM_OVER))
		 {
		hdmi_dbg_print("Invalid Down stream count,fail\n");
		goto CAT_HDCP_Repeater_Fail;
		}
	if (CAT_HDCP_GetKSVList(KSVList, cDownStream) == -1)
		 {
		goto CAT_HDCP_Repeater_Fail;
		}
	
//    if(info->hw_sha_calculator_flag == 1)
//    {   
//       for(i = 0 ; i < cDownStream ; i++)
//       {
//                   revoked = 0 ;
//         HDCP_VerifyRevocationList(SRM1,&KSVList[i*5],&revoked) ;
//         if(revoked)
//          {
//                            goto CAT_HDCP_Repeater_Fail ;
//          }
//        }
//      }
	    if (CAT_HDCP_GetVr(Vr) == -1)
		 {
		goto CAT_HDCP_Repeater_Fail;
		}
	if (CAT_HDCP_GetM0(M0) == -1)
		 {
		goto CAT_HDCP_Repeater_Fail;
		}
	
	    // do check SHA
	    if (CAT_HDCP_CheckSHA(M0, BStatus, KSVList, cDownStream, Vr) == -1)
		 {
		goto CAT_HDCP_Repeater_Fail;
		}
	CAT_HDCP_ResumeRepeaterAuthenticate();
	info->auth_state = HDCP_AUTHENTICATED;
	return 0;
      CAT_HDCP_Repeater_Fail:CAT_HDCP_CancelRepeaterAuthenticate();
	return -1;
}
int CAT_HDCP_Authenticate(Hdmi_info_para_t * info) 
{
	unsigned char ucdata, BCaps;
	
	    //   unsigned char  revoked = 0 ;
	unsigned char BKSV[5] = { 0, 0, 0, 0, 0 };
	unsigned short BStatus, TimeOut;
	int error_flag;
	hdmi_dbg_print("CAT_HDCP_Authenticate():\n");
	
//    printf("CAT_HDCP_Authenticate():\n") ;
	    CAT_HDCP_Reset();	//del by xintan 
//        CAT_ClearDDCFIFO();
	for (TimeOut = 250; TimeOut > 0; TimeOut--)
		 {
		if (CAT_HDCP_GetBCaps(&BCaps, &BStatus) == -1)
			 {
			hdmi_dbg_print("HDCP_GetBCaps fail.\n");
			
//        printf("HDCP_GetBCaps fail.\n") ;
			    info->auth_state = HDCP_REAUTHENTATION_REQ;
			return -1;
			}
		if (info->output_state == CABLE_PLUGIN_HDMI_OUT)
			 {
			if ((BStatus & B_CAP_HDMI_MODE) != 0)
				 {
				break;
				}
			
			else
				 {
				hdmi_dbg_print
				    ("Sink HDCP in DVI mode over HDMI,do not authenticate and encryption. \n");
				}
			}
		
		else
			 {
			if ((BStatus & B_CAP_HDMI_MODE) == 0)
				 {
				break;
				}
			
			else
				 {
				hdmi_dbg_print
				    ("Sink HDCP in HDMI mode over DVI, do not authenticate and encryption. \n");
				}
			}
		AVTimeDly(1);
		}
	
//     printf("BCaps = %02X BStatus = %04X\n",BCaps, BStatus) ;
	    if (TimeOut == 0)
		 {
		if (info->output_state == CABLE_PLUGIN_HDMI_OUT)
			 {
			if ((BStatus & B_CAP_HDMI_MODE) == 0)	//DVI mode.
			{
				hdmi_dbg_print
				    ("Not a HDMI mode,do not authenticate and encryption. \n");
				
//                           printf("Not a HDMI mode,do not authenticate and encryption. \n") ;
				    info->auth_state = HDCP_REAUTHENTATION_REQ;
				return -1;
			}
			}
		
		else
			 {
			if ((BStatus & B_CAP_HDMI_MODE) != 0)
				 {
				hdmi_dbg_print
				    ("Not a HDMI mode,do not authenticate and encryption\n");
				return ER_FAIL;
				}
			}
		}
	error_flag = CAT_HDCP_GetBKSV(info, BKSV);
	
//    printf("BKSV %02X %02X %02X %02X %02X\n",BKSV[0],BKSV[1],BKSV[2],BKSV[3],BKSV[4]) ;
	    if (error_flag == -1)
		 {
		info->auth_state = HDCP_REAUTHENTATION_REQ;
		return error_flag;
		}
	
//     if(info->hw_sha_calculator_flag == 1)
//       {
//       HDCP_VerifyRevocationList(SRM1, BKSV, &revoked) ;
//       if(revoked)
//       {
//         hdmi_dbg_print("BKSV is revoked\n") ; 
//         return -1 ;
//        }
//      }
	    CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	// switch bank action should start on direct register writting of each function.
	ucdata = ReadByteHDMITX_CAT(REG_SW_RST);
	ucdata &= ~B_HDCP_RST;
	WriteByteHDMITX_CAT(REG_SW_RST, ucdata);	//disable HDCP reset       
	
//    if(BCaps & B_CAP_HDCP_1p1)  //support HDCP Enhanced encryption status signaling (EESS),Advance Cipher, and Enhanced Link Verification options.
//    {
//        printf("RX support HDCP 1.1\n"); 
	    WriteByteHDMITX_CAT(REG_HDCP_DESIRE, B_ENABLE_HDPC11 | B_CPDESIRE);	// bit0 = 1: ENable HDCPto desire copy protection; bit1 = : support HDCP 1.1 feature,
//    }
//    else
//    {
//        printf("RX not support HDCP 1.1\n");
	WriteByteHDMITX_CAT(REG_HDCP_DESIRE, B_CPDESIRE);	// bit0 = 1: ENable HDCP to desire copy protection.
//    }
	
//    CAT_HDCP_ClearAuthInterrupt() ;   //del by xintan
	    
//    printf("int2 = %02X DDC_Status = %02X\n",ReadByteHDMITX_CAT(REG_INT_STAT2), ReadByteHDMITX_CAT(REG_DDC_STATUS)) ;
	    CAT_HDCP_GenerateAn();
	WriteByteHDMITX_CAT(REG_LISTCTRL, 0);	//Write this register when process KSVList Check interrupt Routine.
	if ((BCaps & B_CAP_HDMI_REPEATER) == 0)	//without HDCP Repeater capability.
	{
		CAT_HDCP_Auth_Fire();
		CAT_HDCP_ClearAuthInterrupt();	//added by xintan 
		for (TimeOut = 250; TimeOut > 0; TimeOut--)
			 {
			AVTimeDly(5);
			ucdata = ReadByteHDMITX_CAT(REG_AUTH_STAT);
			
//            printf("reg46 = %02x reg16 = %02x\n",ucdata, ReadByteHDMITX_CAT(0x16)) ;
			    if (ucdata & B_AUTH_DONE)
				 {
				info->auth_state = HDCP_AUTHENTICATED;
				break;
				}
			ucdata = ReadByteHDMITX_CAT(REG_INT_STAT2);
			if (ucdata & B_INT_AUTH_FAIL)
				 {
				hdmi_dbg_print
				    ("CAT_HDCP_Authenticate(): Authenticate fail\n");
				
//                printf("CAT_HDCP_Authenticate(): Authenticate fail\n") ;
				    info->auth_state = HDCP_REAUTHENTATION_REQ;
				return -1;
				}
			}
		if (TimeOut == 0)
			 {
			hdmi_dbg_print
			    ("CAT_HDCP_Authenticate(): Time out. return fail\n");
			
//             printf("CAT_HDCP_Authenticate(): Time out. return fail\n") ;
			    info->auth_state = HDCP_REAUTHENTATION_REQ;
			return -1;
			}
		return 0;
	}
	
	else
		return CAT_HDCP_Authenticate_Repeater(info);
}


//////////////////////////////////////////////////////////////////////
// Function: HDCP_EnableEncryption
// Parameter: N/A
// Remark: Set regC1 as zero to enable continue authentication.
// Side-Effect: register bank will reset to zero.
//////////////////////////////////////////////////////////////////////
static void CAT_HDCP_EnableEncryption(void) 
{
	CAT_Switch_HDMITX_Bank(TX_SLV0, 0);	//select bank 0
	WriteByteHDMITX_CAT(REG_ENCRYPTION, B_ENABLE_ENCRYPTION);
} 

//////////////////////////////////////////////////////////////////////
// Function: HDCP_ResumeAuthentication
// Parameter: N/A
// Return: N/A
// Remark: called by interrupt handler to restart Authentication and Encryption.
// Side-Effect: as Authentication and Encryption.
////////////////////////////////////////////////////////////////////// 
void CAT_HDCP_ResumeAuthentication(Hdmi_info_para_t * info) 
{
	hdmi_cat6611_SetAVmute(info);	//del by xintan
//    CAT_HDCP_Reset();                    //add by xintan   
	if (CAT_HDCP_Authenticate(info) == 0)
		 {
		hdmi_dbg_print("CAT_HDCP_EnableEncryption\n");
		CAT_HDCP_EnableEncryption();
		}
	
	else			//HDCP Authentication fail, set AVmute
	{
		
//                      hdmi_cat6611_SetAVmute(info) ;    //added by xintan
	}
	hdmi_cat6611_ClearAVmute(info);	//del by xintan
}
int CAT_EnableHDCP(Hdmi_info_para_t * info, int bEnable) 
{
	if (bEnable)
		 {
		if (CAT_HDCP_Authenticate(info) == -1)
			 {
			return -1;
			}
		}
	
	else
		 {
		
//        WriteByteHDMITX_CAT(REG_SW_RST, (ReadByteHDMITX_CAT(REG_SW_RST)) |B_HDCP_RST) ; //enable HDCP reset
//        WriteByteHDMITX_CAT(REG_HDCP_DESIRE, 0) ;   //bit0 = 0: disable HDCP
		    CAT_HDCP_Reset();
		AVTimeDly(1000);
		}
	return 0;
}


