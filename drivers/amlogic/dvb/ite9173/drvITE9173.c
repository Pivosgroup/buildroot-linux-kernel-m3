#include "MsCommon.h"
#include "drvIIC.h"
#include "drvTuner.h"
#include "drvGPIO.h"
#include "drvSAR.h"
#include "drvCofdmDmd.h" //Temp for use
#include "Type.h"
//#include "api.h"
#include "error.h"
#include "IT9173.h"

#include "drvDemod.h"


#ifdef MS_DEBUG
#define DMD_ERR(_print)	     			_print
#define DMD_MSG(_print)      			_print
#else
#define DMD_ERR(_print)	     			//_print
#define DMD_MSG(_print)      			//_print
#endif


StreamType streamType = StreamType_DVBT_PARALLEL;//StreamType_DVBT_SERIAL;//StreamType_DVBT_PARALLEL;//StreamType_DVBT_SERIAL;//Modified by Roan 2012-03-14
extern Handle I2c_handle;


DefaultDemodulator demod = {
	NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    20480,
    20480000,
    StreamType_DVBT_PARALLEL,//StreamType_DVBT_PARALLEL,//StreamType_DVBT_SERIAL,//Modified by Roan 2012-03-14
    8000,
    642000,
    0x00000000,
	{False, False, 0, 0},
    0,
    False,
    False,
	0,
	User_I2C_ADDRESS,
	False
};
//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------
//#define DVBC_MSEC_LOOP              ( CPU_CLOCK_FREQ/1000/4 )
//#define DVBC_DELAY(_loop)           { volatile int i; for (i=0; i<(_loop)*DVBC_MSEC_LOOP; i++); }
extern MS_U32 SysDelay(MS_U32 dwMs);

#define COFDMDMD_STACK_SIZE          (2048)
#define COFDMDMD_MUTEX_TIMEOUT       (2000)
#define COFDMDMD_AUTO_TUN_MAX        (2 * 5)
#define COFDMDMD_AUTO_TUN_RETRY_MAX  (2)
#if 0
#if   (FRONTEND_DEMOD_CTRL == PIU_IIC_0)
#define IIC_WRITE                   	MDrv_IIC_Write
#define IIC_READ                    	MDrv_IIC_Read
//#error "0"
#elif (FRONTEND_DEMOD_CTRL == PIU_IIC_1)
#define IIC_WRITE                   	MDrv_IIC1_Write
#define IIC_READ                    	MDrv_IIC1_Read
//#error "1"
#elif (FRONTEND_DEMOD_CTRL == PIU_IIC_NONE)
//#error "2"
#define REG(addr)                   	(*(volatile MS_U8*)(addr))
#else
#error "FRONTEND_TUNER_CTRL"
#endif
#endif


#define DEMOD_901X_ADR                  0x38

#define CHG_COFDMDMD_EVENT(_event)    	MsOS_ClearEvent(_s32EventId, 0xFFFFFFFF);   \
                                        MsOS_SetEvent(_s32EventId, _event);


//-------------------------------------------------------------------------------------------------
//  Type and Structure
//-------------------------------------------------------------------------------------------------
typedef enum _COFDMDMD_Event
{
    E_COFDMDMD_EVENT_RUN     = 0x0000001,
} __attribute__((packed)) CCOFDMDMD_Event;


//-------------------------------------------------------------------------------------------------
//  Local Variables
//-------------------------------------------------------------------------------------------------
//static void*                        _pTaskStack;
static MS_S32                       _s32TaskId 	= -1;
static MS_S32                       _s32MutexId;
//static MS_S32                       _s32EventId;
//static COFDMDMD_TunState             _eTunState;
//static MS_BOOL                      _bTuned;
static MS_BOOL 	 					bInited     = FALSE;
//static MS_U32                       u32CurrentTime = 0;
MS_U8                               u8DemodAddr = DEMOD_901X_ADR;

typedef     unsigned long   tDWORD;     // 4 bytes

//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------


tDWORD SysDelay(tDWORD dwMs)
{
    //OS_Sleep(dwMs >> 3);
    if (bInited)
    {
        MsOS_DelayTask(dwMs);
    }
    else
    {
        MsOS_DelayTask(dwMs);
    }
    return (0);
}

/*
static void _af901x_HwReset(void)
{
#ifdef URANUS

#else
	DMD_MSG(printf("---> Hw Rest for af9013 \n"));
    MDrv_SAR_GPIO_Init(TRUE);
    //MDrv_GPIO_N_Write(0, 0);
    MDrv_SAR_GPIO_SAR0_SAR3_Write(2, 0);
    //MDrv_GPIO_AU_Write(6,1);
    SysDelay(100);
    //MDrv_GPIO_AU_Write(6,0);
    MDrv_SAR_GPIO_SAR0_SAR3_Write(2, 1);
    //MDrv_GPIO_N_Write(0, 1)0;
    //SysDelay(100);
    //MDrv_GPIO_AU_Write(6,1);
#endif
}
*/

static MS_BOOL _af901x_Init(void)
{
	if(Error_NO_ERROR == Demodulator_initialize ((Demodulator*)&demod, streamType))
		return TRUE;

	return FALSE;
}

static void _Demod_HwReset(void)
{
    extern void MApi_Demod_HWReset(MS_BOOL bReset);
    printf("---> Hw Rest for DEMOD \n");
    SysDelay(2000);    
    MApi_Demod_HWReset(TRUE);
    SysDelay(200);
    //MApi_Demod_HWReset(FALSE);
    //SysDelay(100);
}
//Mstar Cofdm comman interface
MS_BOOL MDrv_Demod_Init(void)
{
    //MS_U8 u8Ret __UNUSED = 0 ;

    printf("\n--ITE 9173 init Begint--\n");
    bInited = FALSE;
    //bInited = TRUE;

    if (_s32TaskId >= 0)
    {
        return TRUE;
    }

    _s32MutexId = MsOS_CreateMutex(E_MSOS_FIFO, "OfDmd_Mutex", MSOS_PROCESS_SHARED);

    if (_s32MutexId < 0)
    {
        GEN_EXCEP;
        return FALSE;
    }
	
    _Demod_HwReset();
    if(TRUE == _af901x_Init())
	printf("\n--ITE 9173 init End OK--\n");
     else	
	printf("\n--ITE 9173 init End Failed--\n");
    
    bInited = TRUE;
    return TRUE;
}


MS_BOOL MDrv_Demod_Open(void)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}


MS_BOOL MDrv_Demod_Close(void)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
        //--Demodulator_enableControlPowerSaving


    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}


MS_BOOL MDrv_Demod_Reset(void)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_Demod_Restart(CofdmDMD_Param* pParam)
{
    tDWORD  			dwError = 0;

    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("MDrv_COFDMDMD_Restart: Obtain mutex failed.\n");)
        return FALSE;
    }
    else
    {
       pParam->u8BandWidth = 6;  //Honestar handy 2011/11/23 set default bandwidth 6000
       printf("!!!!!!![xsw]:emodulator_acquireChannel BandWidth=%d,Freq=%d;\n",(Word)pParam->u8BandWidth*1000,(Dword)pParam->u32TunerFreq);
	if(pParam->u32TunerFreq>0&&pParam->u32TunerFreq!=-1)
       {
             dwError = Demodulator_acquireChannel((Demodulator*)&demod, (Word)pParam->u8BandWidth*1000,(Dword)pParam->u32TunerFreq);
       }
	else
		printf("\n--[xsw]: Invalidate Fre!!!!!!!!!!!!!--\n");
       //DMD_MSG(printf("### -> %d \n", dwError));
    }

    MsOS_ReleaseMutex(_s32MutexId);
    printf("!!!!!!MDrv_CofdmDmd_Restart  Resoult :%d",dwError);
    return 0 == dwError ? TRUE : FALSE;

}

MS_BOOL MDrv_Demod_SetMode(Demod_Mode *pMode)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_Demod_TsOut(MS_BOOL bEnable)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_Demod_PowerOnOff(MS_BOOL bPowerOn)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_Demod_SetBW(MS_U32 u32BW)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_Demod_GetBW(MS_U32 *pu32BW)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_Demod_GetLock(MS_BOOL *pbLock)
{
    tDWORD              dwError;
	 SysDelay(10);
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    	dwError = Demodulator_isLocked ((Demodulator*)&demod, (Bool*)pbLock);
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return (dwError == 0) ? TRUE : FALSE;
}

MS_BOOL MDrv_Demod_TPSGetLock(MS_BOOL *pbLock)
{
    tDWORD              dwError;
	 SysDelay(10);
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    	dwError = Demodulator_isLocked ((Demodulator*)&demod, (Bool*)pbLock);
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return (dwError == 0) ? TRUE : FALSE;
}


MS_BOOL MDrv_Demod_MPEGGetLock(MS_BOOL *pbLock)
{
#if 0
    tDWORD              dwError;
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {       
       MS_U8 i = 3;
       for(i = 0; i <  3; i++)
       {
    	   dwError = Demodulator_isLocked ((Demodulator*)&demod, (Bool*)pbLock);
          if(dwError == 0)
          {
             goto exit;
          }
	   SysDelay(300);          
       }
    }
exit:
    MsOS_ReleaseMutex(_s32MutexId);
    return (dwError == 0) ? TRUE : FALSE;
#else
    tDWORD              dwError;
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    	dwError = Demodulator_isLocked ((Demodulator*)&demod, (Bool*)pbLock);
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return (dwError == 0) ? TRUE : FALSE;
#endif
}

//Honestar handy 2011/11/23
MS_BOOL MDrv_Demod_GetSNR(MS_U32 *pu32SNR)
{
    tDWORD dwError;
    MS_U8 u8Signal = 0;    
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
       // DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
        dwError = Demodulator_getSignalQuality((Demodulator*)&demod, &u8Signal);
        //printf("!!!!!!MDrv_Demod_GetPWR = %d\n",u8Signal);       
        *pu32SNR = (MS_S32)u8Signal;
    }
    MsOS_ReleaseMutex(_s32MutexId);
    return (dwError == 0) ? TRUE : FALSE;
}	

MS_BOOL MDrv_Demod_GetBER(float *pfBER)
{

    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }

//    else		Example_monitorStatistic ((Demodulator*)&demod);

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

//Honestar handy 2011/11/23
MS_BOOL MDrv_Demod_GetPWR(MS_S32 *ps32Signal)
{

    tDWORD dwError;
    MS_U8 u8Signal = 0;
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
        dwError = Demodulator_getSignalQuality((Demodulator*)&demod, &u8Signal);
        //printf("!!!!!!MDrv_Demod_GetPWR = %d\n",u8Signal);       
        *ps32Signal = (MS_S32)u8Signal;
     }
    MsOS_ReleaseMutex(_s32MutexId);
    return (dwError == 0) ? TRUE : FALSE;
}

MS_BOOL MDrv_Demod_GetParam(DEMOD_MS_FE_CARRIER_PARAM *pParam)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }
    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_Demod_Config(MS_U8 *pRegParam)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DMD_ERR(printf("%s: Obtain mutex failed.\n", __FUNCTION__);)
        return FALSE;
    }
    else
    {
    }
    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}


