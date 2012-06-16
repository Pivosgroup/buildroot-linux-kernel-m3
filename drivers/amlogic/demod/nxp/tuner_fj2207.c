#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include "../aml_demod.h"
#include "../demod_func.h"

#include "tmNxTypes.h"
#include "tmCompId.h"
#include "tmFrontEnd.h"
#include "tmbslFrontEndTypes.h"
#include "tmbslNT220HN.h"

struct aml_demod_i2c *I2C_adap;

//*--------------------------------------------------------------------------------------
//* Prototype of function to be provided by customer
//*--------------------------------------------------------------------------------------
tmErrorCode_t 	UserWrittenI2CRead(tmUnitSelect_t tUnit,UInt32 AddrSize, UInt8* pAddr,UInt32 ReadLen, UInt8* pData);
tmErrorCode_t 	UserWrittenI2CWrite (tmUnitSelect_t tUnit, UInt32 AddrSize, UInt8* pAddr,UInt32 WriteLen, UInt8* pData);
tmErrorCode_t 	UserWrittenWait(tmUnitSelect_t tUnit, UInt32 tms);
tmErrorCode_t 	UserWrittenPrint(UInt32 level, const char* format, ...);
tmErrorCode_t  	UserWrittenMutexInit(ptmbslFrontEndMutexHandle *ppMutexHandle);
tmErrorCode_t  	UserWrittenMutexDeInit( ptmbslFrontEndMutexHandle pMutex);
tmErrorCode_t  	UserWrittenMutexAcquire(ptmbslFrontEndMutexHandle pMutex, UInt32 timeOut);
tmErrorCode_t  	UserWrittenMutexRelease(ptmbslFrontEndMutexHandle pMutex);

int init_tuner_fj2207(struct aml_demod_sta *demod_sta, 
		     struct aml_demod_i2c *adap)
{
   tmErrorCode_t err = TM_OK;
   tmbslFrontEndDependency_t sSrvTunerFunc;
   
   I2C_adap = adap;

/* Low layer struct set-up to link with user written functions */
   sSrvTunerFunc.sIo.Write             = UserWrittenI2CWrite;
   sSrvTunerFunc.sIo.Read              = UserWrittenI2CRead;
   sSrvTunerFunc.sTime.Get             = Null;
   sSrvTunerFunc.sTime.Wait            = UserWrittenWait;
   sSrvTunerFunc.sDebug.Print          = UserWrittenPrint;
   sSrvTunerFunc.sMutex.Init           = UserWrittenMutexInit;
   sSrvTunerFunc.sMutex.DeInit         = UserWrittenMutexDeInit;
   sSrvTunerFunc.sMutex.Acquire        = UserWrittenMutexAcquire;
   sSrvTunerFunc.sMutex.Release        = UserWrittenMutexRelease;
   sSrvTunerFunc.dwAdditionalDataSize  = 0;
   sSrvTunerFunc.pAdditionalData       = Null;
   
    err = tmbslNT220xInit(0, &sSrvTunerFunc);
   if(err != TM_OK)
       return err;
    
    err = tmbslNT220xReset(0);
   if(err != TM_OK)
       return err;

       return err;
}

char *NT220x_mode_name[] = {
    "DVBT_6MHz ",
    "DVBT_7MHz ",
    "DVBT_8MHz ",
    "QAM_6MHz  ",
    "QAM_8MHz  ",
    "ISDBT_6MHz",
    "ATSC_6MHz ",
    "DMBT_8MHz ",
    "ScanXpress",
    "Mode_Max  "};

int set_tuner_fj2207(struct aml_demod_sta *demod_sta, 
		     struct aml_demod_i2c *adap)
{
    tmErrorCode_t err = TM_OK;
    tmNT220xStandardMode_t StandardMode;
    tmbslFrontEndState_t PLLLock;
    UInt32 ch_if;
    UInt32 ch_pow;

    unsigned long ch_freq;
    int ch_bw;
    
    ch_freq = demod_sta->ch_freq; // kHz
    ch_bw   = demod_sta->ch_bw / 1000; // MHz

    printk("Set Tuner FJ2207 to %ld kHz\n", ch_freq);
    ch_freq *= 1000; // Hz

    StandardMode = demod_sta->dvb_mode==0 ?
	(ch_bw<8 ? tmNT220x_QAM_6MHz : tmNT220x_QAM_8MHz) : 
	(ch_bw-6 + tmNT220x_DVBT_6MHz);

    printk("Mode: %s\n", NT220x_mode_name[StandardMode]);
    err = tmbslNT220xSetStandardMode(0, StandardMode);
   if(err != TM_OK)
       return err;

    err = tmbslNT220xSetRf(0, ch_freq);
   if(err != TM_OK)
       return err;

    err = tmbslNT220xGetLockStatus(0, &PLLLock);
   if(err != TM_OK)
       return err;
  
    err = tmbslNT220xGetIF(0, &ch_if);
   if(err != TM_OK)
       return err;
    demod_sta->ch_if = ch_if / 1000;
    printk("IF: %d kHz\n", demod_sta->ch_if);

    err = tmbslNT220xGetPowerLevel(0, &ch_pow);
   if(err != TM_OK)
       return err;
    printk("Signal Power: %d dBuV\n", (int)ch_pow);

    return err;
}

int get_fj2207_ch_power(void)
{
    tmErrorCode_t err = TM_OK;
    UInt32 ch_pow;

    err = tmbslNT220xGetPowerLevel(0, &ch_pow);
    if(err != TM_OK) ch_pow = 0;

    return ch_pow;
}

//*--------------------------------------------------------------------------------------
//* Template of function to be provided by customer
//*--------------------------------------------------------------------------------------

//*--------------------------------------------------------------------------------------
//* Function Name       : UserWrittenI2CRead
//* Object              : 
//* Input Parameters    : 	tmUnitSelect_t tUnit
//* 						UInt32 AddrSize,
//* 						UInt8* pAddr,
//* 						UInt32 ReadLen,
//* 						UInt8* pData
//* Output Parameters   : tmErrorCode_t.
//*--------------------------------------------------------------------------------------
tmErrorCode_t UserWrittenI2CRead(tmUnitSelect_t tUnit,	UInt32 AddrSize, UInt8* pAddr,
UInt32 ReadLen, UInt8* pData)
{
    tmErrorCode_t err = TM_OK;
    int ret;
    struct i2c_msg msg[2];
    
    msg[0].addr = I2C_adap->addr;
    msg[0].flags = 0 | I2C_M_IGNORE_NAK;
    msg[0].len = AddrSize;
    msg[0].buf = pAddr;
       
    msg[1].addr = I2C_adap->addr;
    msg[1].flags = I2C_M_RD | I2C_M_IGNORE_NAK;
    msg[1].len = ReadLen;
    msg[1].buf = pData;

    ret = am_demod_i2c_xfer(I2C_adap, msg, 2);
    if (ret != 2) err = -1;

    return err;
}

//*--------------------------------------------------------------------------------------
//* Function Name       : UserWrittenI2CWrite
//* Object              : 
//* Input Parameters    : 	tmUnitSelect_t tUnit
//* 						UInt32 AddrSize,
//* 						UInt8* pAddr,
//* 						UInt32 WriteLen,
//* 						UInt8* pData
//* Output Parameters   : tmErrorCode_t.
//*--------------------------------------------------------------------------------------
tmErrorCode_t UserWrittenI2CWrite (tmUnitSelect_t tUnit, 	UInt32 AddrSize, UInt8* pAddr,
UInt32 WriteLen, UInt8* pData)
{
    tmErrorCode_t err = TM_OK;
    int i, ret;
    struct i2c_msg msg;
    UInt8 buf[70];

    for (i=0; i<AddrSize; i++) buf[i] = pAddr[i];
    for (i=0; i<WriteLen; i++) buf[AddrSize+i] = pData[i];

    msg.addr = I2C_adap->addr;
    msg.flags = 0 | I2C_M_IGNORE_NAK;
    msg.len = AddrSize + WriteLen;
    msg.buf = buf;
    
    ret = am_demod_i2c_xfer(I2C_adap, &msg, 1);
    if (ret != 1) err = -1;

    return err;
}

//*--------------------------------------------------------------------------------------
//* Function Name       : UserWrittenWait
//* Object              : 
//* Input Parameters    : 	tmUnitSelect_t tUnit
//* 						UInt32 tms
//* Output Parameters   : tmErrorCode_t.
//*--------------------------------------------------------------------------------------
tmErrorCode_t UserWrittenWait(tmUnitSelect_t tUnit, UInt32 tms)
{
    tmErrorCode_t err = TM_OK;

    mdelay(tms);

    return err;
}

//*--------------------------------------------------------------------------------------
//* Function Name       : UserWrittenPrint
//* Object              : 
//* Input Parameters    : 	UInt32 level, const char* format, ...
//* 						
//* Output Parameters   : tmErrorCode_t.
//*--------------------------------------------------------------------------------------
tmErrorCode_t UserWrittenPrint(UInt32 level, const char* format, ...)
{
    /* Variable declarations */
    tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

   return err;
}

//*--------------------------------------------------------------------------------------
//* Function Name       : UserWrittenMutexInit
//* Object              : 
//* Input Parameters    : 	ptmbslFrontEndMutexHandle *ppMutexHandle
//* Output Parameters   : tmErrorCode_t.
//*--------------------------------------------------------------------------------------
tmErrorCode_t  UserWrittenMutexInit(ptmbslFrontEndMutexHandle *ppMutexHandle)
{
   /* Variable declarations */
   tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

   return err;
}


//*--------------------------------------------------------------------------------------
//* Function Name       : UserWrittenMutexDeInit
//* Object              : 
//* Input Parameters    : 	 ptmbslFrontEndMutexHandle pMutex
//* Output Parameters   : tmErrorCode_t.
//*--------------------------------------------------------------------------------------
tmErrorCode_t  UserWrittenMutexDeInit( ptmbslFrontEndMutexHandle pMutex)
{
   /* Variable declarations */
   tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

   return err;
}



//*--------------------------------------------------------------------------------------
//* Function Name       : UserWrittenMutexAcquire
//* Object              : 
//* Input Parameters    : 	ptmbslFrontEndMutexHandle pMutex, UInt32 timeOut
//* Output Parameters   : tmErrorCode_t.
//*--------------------------------------------------------------------------------------
tmErrorCode_t  UserWrittenMutexAcquire(ptmbslFrontEndMutexHandle pMutex, UInt32 timeOut)
{
   /* Variable declarations */
   tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

   return err;
}

//*--------------------------------------------------------------------------------------
//* Function Name       : UserWrittenMutexRelease
//* Object              : 
//* Input Parameters    : 	ptmbslFrontEndMutexHandle pMutex
//* Output Parameters   : tmErrorCode_t.
//*--------------------------------------------------------------------------------------
tmErrorCode_t  UserWrittenMutexRelease(ptmbslFrontEndMutexHandle pMutex)
{
   /* Variable declarations */
   tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

   return err;
}


