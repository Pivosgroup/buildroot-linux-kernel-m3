 
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
    
#include "hdmi_debug.h"
#include "hdmi_global.h"
    
#include "hdmi_cat_defstx.h"
#include "hdmi_cat_mddc.h"
#include "hdmi_i2c.h"
    
////////////////////////////////////////////////////////////////////////////////
// Function: CAT_ClearDDCFIFO
// Parameter: N/A
// Return: N/A
// Remark: clear the DDC FIFO.
// Side-Effect: DDC master will set to be HOST.
////////////////////////////////////////////////////////////////////////////////
void CAT_ClearDDCFIFO(void) 
{
	WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERDDC | B_MASTERHOST);
	WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_FIFO_CLR);
} 

////////////////////////////////////////////////////////////////////////////////
// Function: CAT_AbortDDC
// Parameter: N/A
// Return: N/A
// Remark: Force abort DDC and reset DDC bus.
// Side-Effect: 
////////////////////////////////////////////////////////////////////////////////
void CAT_AbortDDC(void) 
{
	unsigned char CPDesire, SWReset, DDCMaster;
	unsigned char count, uc;
	
	    // save the SW reset, DDC master, and CP Desire setting.
	    SWReset = ReadByteHDMITX_CAT(REG_SW_RST);
	CPDesire = ReadByteHDMITX_CAT(REG_HDCP_DESIRE);
	DDCMaster = ReadByteHDMITX_CAT(REG_DDC_MASTER_CTRL);
	WriteByteHDMITX_CAT(REG_HDCP_DESIRE, CPDesire & (~B_CPDESIRE));	// bit0 = 0: disable HDCP
	WriteByteHDMITX_CAT(REG_SW_RST, SWReset | B_HDCP_RST);	//enable HDCP reset
	WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERDDC | B_MASTERHOST);
	WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_DDC_ABORT);
	for (count = 200; count > 0; count--)
		 {
		uc = ReadByteHDMITX_CAT(REG_DDC_STATUS);
		if (uc & B_DDC_DONE)
			 {
			break;
			}
		if (uc & B_DDC_ERROR)
			 {
			break;
			}
		AVTimeDly(1);	// delay 1 ms to stable.
		}
	WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_DDC_ABORT);
	for (count = 200; count > 0; count--)
		 {
		uc = ReadByteHDMITX_CAT(REG_DDC_STATUS);
		if (uc & B_DDC_DONE)
			 {
			break;
			}
		if (uc & B_DDC_ERROR)
			 {
			
			    // error when abort.
			    break;
			}
		AVTimeDly(1);	// delay 1 ms to stable.
		}
	
	    // restore the SW reset, DDC master, and CP Desire setting.
//    WriteByteHDMITX_CAT(REG_SW_RST, SWReset) ;
//    WriteByteHDMITX_CAT(REG_HDCP_DESIRE,CPDesire) ;
//    WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL,DDCMaster) ;
}
void CAT_GenerateDDCSCLK(void) 
{
	WriteByteHDMITX_CAT(REG_DDC_MASTER_CTRL, B_MASTERDDC | B_MASTERHOST);
	WriteByteHDMITX_CAT(REG_DDC_CMD, CMD_GEN_SCLCLK);
} 
