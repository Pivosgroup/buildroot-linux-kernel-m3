#ifndef __HDMI_MDDC_CAT_H__
#define __HDMI_MDDC_CAT_H__
 typedef enum { MDDC_IIC_OK =
	    0, MDDC_IIC_CAPTURED, MDDC_IIC_NOACK, MDDC_CAPTURED, MDDC_NOACK,
	    MDDC_FIFO_FULL, MDDC_MAX 
} mddc_i2c_state_t;
typedef enum _SYS_STATUS { ER_SUCCESS = 0, ER_FAIL, ER_RESERVED 
} SYS_STATUS;
extern void CAT_ClearDDCFIFO(void);
extern void CAT_AbortDDC(void);
extern void CAT_GenerateDDCSCLK(void);

#endif	/*  */
    
