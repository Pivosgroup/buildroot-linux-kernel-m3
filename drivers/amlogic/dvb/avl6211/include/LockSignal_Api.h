/*
 *           Copyright 2012 Availink, Inc.
 *
 *  This software contains Availink proprietary information and
 *  its use and disclosure are restricted solely to the terms in
 *  the corresponding written license agreement. It shall not be 
 *  disclosed to anyone other than valid licensees without
 *  written permission of Availink, Inc.
 *
 */


///$Date: 2012-2-9 17:36 $
///



#ifndef LockSignal_source_h_h
    #define LockSignal_source_h_h

    #include "avl_dvbsx.h"
   

	#ifdef AVL_CPLUSPLUS
extern "C" {
	#endif
/*#include"ITuner.h"
#include "IRx.h"
#include "DiSEqC_source.h"	
#include "IBlindScan.h"
#include "IBlindscanAPI.h"	
#include "II2C.h"
#include "IBase.h"*/



    struct Signal_Level
    {
	    AVL_uint16 SignalLevel;
        AVL_int16 SignalDBM;
    };

    void AVL_DVBSx_Error_Dispose(AVL_DVBSx_ErrorCode r);
    AVL_DVBSx_ErrorCode AVL6211_Initialize(struct AVL_DVBSx_Chip * pAVLChip,struct AVL_Tuner * pTuner);
    AVL_DVBSx_ErrorCode AVL6211_LockSignal_Init(void);
   AVL_DVBSx_ErrorCode CPU_Halt(struct AVL_DVBSx_Chip * pAVLChip);
   void AVL_Set_LPF(struct AVL_Tuner * pTuner, AVL_uint32 m_uiSymbolRate_Hz);
	int AVL_Get_Quality_Percent(struct AVL_DVBSx_Chip * pAVLChip);	
	AVL_int16 AVL_Get_Level_Percent(struct AVL_DVBSx_Chip * pAVLChip);
   
	AVL_uint32 AVL6211_GETBer(void);
	AVL_uint32 AVL6211_GETPer(void);
	AVL_uint32 AVL6211_GETSnr(void);
	AVL_uint32 AVL6211_GETSignalLevel(void);
	AVL_uint32 AVL6211_GETLockStatus(void);

	
	#ifdef AVL_CPLUSPLUS
}
	#endif
#endif
