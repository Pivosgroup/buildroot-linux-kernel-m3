#include "MsCommon.h"
//#include "drvIIC.h"
//#include "drvTuner.h"
#include "drvGPIO.h"
#include "drvSAR.h"
#include "drvCofdmDmd.h" //Temp for use
//#include "Default.h"
#include "demod_MxL101SF.h"

//#define MS_DEBUG
#ifdef MS_DEBUG
#define DBG_MSB(x)          x
#else
#define DBG_MSB(x)          //x
#endif

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------
#define DVBC_MSEC_LOOP              ( CPU_CLOCK_FREQ/1000/4 )
#define DVBC_DELAY(_loop)           { volatile int i; for (i=0; i<(_loop)*DVBC_MSEC_LOOP; i++); }

#define COFDMDMD_MUTEX_TIMEOUT       (2000)
//#define Mon_Signal_MUTEX_TIMEOUT         100
#define COFDMDMD_EVENT_TIMEOUT          100// MSOS_WAIT_FOREVER

//-------------------------------------------------------------------------------------------------
//  Type and Structure
//-------------------------------------------------------------------------------------------------


#define CHG_COFDMDMD_EVENT_CLEAR(_event)    	MsOS_ClearEvent(_s32EventId, 0xFFFFFFFF);   \
                                        MsOS_SetEvent(_s32EventId, _event);
#define CHG_COFDMDMD_EVENT(_event)    	MsOS_SetEvent(_s32EventId, _event);


//-------------------------------------------------------------------------------------------------
//  Local Variables
//-------------------------------------------------------------------------------------------------
static MS_BOOL 	 					bInited     = FALSE;
#define MONITOR_Signal_TASK_STACK              (2048*2)
static MS_S32                       _s32TaskId = -1;
static MS_S32                       _s32MutexId;
static MS_S32                       _s32EventId;
//static TaskState                    _eTaskState;
static MS_BOOL _plock = FALSE;


CofdmDMD_Param _pParam;

#define MON_SIGNAL_DBG_ENABLE

#ifdef MON_SIGNAL_DBG_ENABLE
#define MON_SIGNAL_DBG(_print)          _print
#else
#define MON_SIGNAL_DBG(_print)
#endif

#ifdef MON_SIGNAL_DBG_ENABLE
#define MON_SIGNAL_ERR(_print)	        _print
#else
#define MON_SIGNAL_ERR(_print)
#endif

/// Channel scan return value definition
typedef enum _COFDMDMD_result
{
    E_COFDMDMD_OK,                                                        ///< No error
    E_COFDMDMD_FAIL,                                                      ///< Function error
    E_COFDMDMD_PARAM_ERROR,                                               ///< Function parameter error
    E_COFDMDMD_TIMEOUT,                                                   ///< COFDMDMD timeout
} __attribute__((packed)) COFDMDMD_Result;

typedef enum _COFDMDMD_Event
{
    E_COFDMDMD_EVENT_TUNE     = 0x0000001,
    E_COFDMDMD_EVENT_WAITLOCK     = 0x0000002,
    E_COFDMDMD_EVENT_END     = 0x0000004,
} __attribute__((packed)) CCOFDMDMD_Event;


COFDMDMD_Result MApi_COFDMDMD_GetEvent(CCOFDMDMD_Event *peEvent)
{
    MS_U32              u32Events = 0;
    //printf("[+]MApi_COFDMDMD_GetEvent\n");
    if (MsOS_WaitEvent(_s32EventId, E_COFDMDMD_EVENT_TUNE | E_COFDMDMD_EVENT_WAITLOCK | E_COFDMDMD_EVENT_END,
                       &u32Events, E_OR_CLEAR, COFDMDMD_EVENT_TIMEOUT))
    {
        *peEvent = (CCOFDMDMD_Event)u32Events;
        //printf("[-]MApi_COFDMDMD_GetEvent,u32Events=0x%x,OK\n",u32Events);
        return E_COFDMDMD_OK;
    }

    *peEvent =  (CCOFDMDMD_Event)u32Events;
    //printf("[-]MApi_COFDMDMD_GetEvent,u32Events=0x%x,FAIL\n",u32Events);
    return E_COFDMDMD_TIMEOUT;
}

static void _Mon_Signal_Task(void)
{
    static MS_U32 _u32TimeOut = 10;
    CCOFDMDMD_Event     u32Events;
	MS_U32 _u32tunetime = 0;
     //int i;
    while (1)
    {
        {
            MApi_COFDMDMD_GetEvent(&u32Events);
            switch(u32Events)
            {
                case E_COFDMDMD_EVENT_TUNE:
                    MsOS_ClearEvent(_s32EventId, 0xFFFFFFFF);
                    MON_SIGNAL_DBG(printf("\n E_TUNE_START\n"));
					_u32tunetime = MsOS_GetSystemTime();
					MON_SIGNAL_DBG(printf("\n Before tune :%d\n",_u32tunetime));
					MxL101SF_Tune(_pParam.u32TunerFreq,_pParam.u8BandWidth);
					MON_SIGNAL_DBG(printf("\n After tune :%d\n",MsOS_GetSystemTime()));
					MON_SIGNAL_DBG(printf("\n tune time :%d\n",MsOS_GetSystemTime()-_u32tunetime));
                    CHG_COFDMDMD_EVENT(E_COFDMDMD_EVENT_END);
					break;
					/*
					//not used by kevin.deng
					_u32TimeOut = 1;
                    CHG_COFDMDMD_EVENT(E_COFDMDMD_EVENT_WAITLOCK);
                    break;
					*/
                case E_COFDMDMD_EVENT_WAITLOCK:
                    MON_SIGNAL_DBG(printf("\n E_TUNE_WAIT_LOCK\n"));
                    MsOS_DelayTask(50);
                    if(MxL101SF_GetLock())
                    {
                        _plock = TRUE;
                        MON_SIGNAL_DBG(printf("\n _TUNE_LOCK\n"));
                        MsOS_ClearEvent(_s32EventId, E_COFDMDMD_EVENT_WAITLOCK);
                        CHG_COFDMDMD_EVENT(E_COFDMDMD_EVENT_END);
                    }
                    else if(_u32TimeOut--)
                    {
                        MON_SIGNAL_DBG(printf("\n _u32TimeOut\n"));
                        CHG_COFDMDMD_EVENT(E_COFDMDMD_EVENT_WAITLOCK);
                    }
                    else
                    {
                        _plock = FALSE;
                        MON_SIGNAL_DBG(printf("\n _TUNE_UNLOCK\n"));
                        CHG_COFDMDMD_EVENT_CLEAR(E_COFDMDMD_EVENT_TUNE);
                    }
                    break;
                case E_COFDMDMD_EVENT_TUNE|E_COFDMDMD_EVENT_WAITLOCK:
                    MON_SIGNAL_DBG(printf("\n E_COFDMDMD_EVENT_TUNE|E_COFDMDMD_EVENT_WAITLOCK\n"));
                    CHG_COFDMDMD_EVENT_CLEAR(E_COFDMDMD_EVENT_TUNE);
                    break;

                case E_COFDMDMD_EVENT_TUNE|E_COFDMDMD_EVENT_END:
                    MON_SIGNAL_DBG(printf("\n E_COFDMDMD_EVENT_TUNE|E_COFDMDMD_EVENT_END\n"));
                    CHG_COFDMDMD_EVENT_CLEAR(E_COFDMDMD_EVENT_TUNE);
                    break;
                case E_COFDMDMD_EVENT_END:
                    MsOS_ClearEvent(_s32EventId, 0xFFFFFFFF);
                    MON_SIGNAL_DBG(printf("\n E_TUNE_END\n"));
                    break;
                default:
                    break;

            }
        }
    }
}
 MS_BOOL MApi_Mon_Signal_Init(void)
{
#if (!(DYNMEM_STACK))
    static MS_U8 _pTaskStack[MONITOR_Signal_TASK_STACK];
#endif
    if (_s32TaskId >= 0)
    {
        return FALSE;
    }

    _s32MutexId = MsOS_CreateMutex(E_MSOS_FIFO, "OfDmd_Mutex");
    if (_s32MutexId < 0)
    {
        GEN_EXCEP;
        return FALSE;
    }
    _s32EventId = MsOS_CreateEventGroup("CCOFDMDMD_Event");
    if (_s32EventId < 0)
    {
        GEN_EXCEP;
        MsOS_DeleteMutex(_s32MutexId);
        return FALSE;
    }
#if (DYNMEM_STACK)
    _pTaskStack = MsOS_AllocateMemory(MONITOR_Signal_TASK_STACK, gs32CachedPoolID);
    if (_pTaskStack == NULL)
    {
        GEN_EXCEP;
        MsOS_DeleteEventGroup(_s32EventId);
        MsOS_DeleteMutex(_s32MutexId);
        return FALSE;
    }
#endif
    _s32TaskId = MsOS_CreateTask((TaskEntry)_Mon_Signal_Task,
                                 NULL,
                                 E_TASK_PRI_LOWEST, // Note! Adjust the task priority to E_TASK_PRI_LOW.
                                 TRUE,
                                 _pTaskStack,
                                 MONITOR_Signal_TASK_STACK,
                                 "Mon_Signal_Task");
    if (_s32TaskId < 0)
    {
        GEN_EXCEP;
        //MsOS_FreeMemory(_pTaskStack, gs32CachedPoolID);
        MsOS_DeleteEventGroup(_s32EventId);
        MsOS_DeleteMutex(_s32MutexId);
        return FALSE;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//  Local Variables
//-------------------------------------------------------------------------------------------------
MS_U32 SysDelay(MS_U32 dwMs)
{
    if (bInited)
    {
        MsOS_DelayTask(dwMs);
    }
    else
    {
        DVBC_DELAY(dwMs);
    }
    return (0);

}


static void _Demod_HwReset(void)
{
    extern void MApi_Demod_HWReset(MS_BOOL bReset);
    MApi_Demod_HWReset(TRUE);
    SysDelay(100);
    MApi_Demod_HWReset(FALSE);
}

static MS_BOOL _Demod_Init(void)
{
    _Demod_HwReset();
	SysDelay(200);
	MxL101SF_Init();
	return TRUE;
}

/*
//Mstar Cofdm comman interface
MS_BOOL MDrv_CofdmDmd_Check_lock(void)
{	
	static MS_U8 u8Check = 0;
    if(MxL101SF_GetLock() != _plock)
    {
    	u8Check++;
		if(u8Check>=20)
		{
			u8Check = 0;
        	_plock = !_plock;
        	printf("\n Retune deomod\n");
        	CHG_COFDMDMD_EVENT(E_COFDMDMD_EVENT_TUNE);
		}
    }
	else
	{
		u8Check = 0;
	}
	return _plock;	
}

*/
MS_BOOL MDrv_CofdmDmd_Init(void)
{
    bInited = FALSE;
	_Demod_Init();
    MApi_Mon_Signal_Init();
    bInited = TRUE;
    return TRUE;
}


MS_BOOL MDrv_CofdmDmd_Open(void)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}


MS_BOOL MDrv_CofdmDmd_Close(void)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_Reset(void)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_U32  Mdrv_DigitalTuner_SetFreq(CofdmDMD_Param *pParam)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_Restart(CofdmDMD_Param *pParam)
{
    MS_U32  			dwError = 0;
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
       _pParam = *pParam;
       CHG_COFDMDMD_EVENT(E_COFDMDMD_EVENT_TUNE);
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return (dwError ) ? FALSE : TRUE;

}

MS_BOOL MDrv_CofdmDmd_SetMode(CofdmDMD_Mode *pMode)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_TsOut(MS_BOOL bEnable)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_PowerOnOff(MS_BOOL bPowerOn)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    	MxL101SF_PowerOnOff(bPowerOn);
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_SetBW(MS_U32 u32BW)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_GetBW(MS_U32 *pu32BW)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_GetLock(MS_BOOL *pbLock)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
        *pbLock = MxL101SF_GetLock();
    }
    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_GetSNR(MS_U32 *pu32SNR)
{
	#define DEFAULT_SNR_MIN 0
    #define DEFAULT_SNR_MAX 35
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
		*pu32SNR = MxL101SF_GetSNR();
		DBG_MSB(printf("snr=%d\n",*pu32SNR));
    }
    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE ;
}

MS_BOOL MDrv_CofdmDmd_GetBER(float *pfBER)
{
	*pfBER = MxL101SF_GetBER();
	return TRUE;
}


MS_BOOL MDrv_CofdmDmd_GetPWR(MS_S32 *ps32Signal)
{
	#define DEFAULT_PWR_MIN (255)
	#define DEFAULT_PWR_MAX (0)

    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
        if (TRUE == MxL101SF_GetLock() )
        {
			*ps32Signal = MxL101SF_GetSigStrength();
        }

        else
        {
	        *ps32Signal = 0;
        }
    }

    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE ;

}


MS_BOOL MDrv_CofdmDmd_GetParam(CofdmDMD_Param *pParam)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {
    }
    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_Config(MS_U8 *pRegParam)
{
    if (MsOS_ObtainMutex(_s32MutexId, COFDMDMD_MUTEX_TIMEOUT) == FALSE)
    {
        DBG_MSB(printf("%s: Obtain mutex failed.\n", __FUNCTION__));
        return FALSE;
    }
    else
    {

    }
    MsOS_ReleaseMutex(_s32MutexId);
    return TRUE;
}

MS_BOOL MDrv_CofdmDmd_I2C_ByPass(MS_BOOL bOn)
{
    return TRUE;
}


