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
///
/// @file 
/// @brief Declare functions for external Sharp BS2S7HZ6306 tuner control
///
#ifndef ExtSharpBS2S7HZ6306_h_h
	#define ExtSharpBS2S7HZ6306_h_h

	#include "avl_dvbsx.h"
    #include "ITuner.h"

	//Included to support the blind scan fix made to blindscanform.cpp file ae per raptor.
	#define SHARP_TUNER_FACTOR_VALUE			9
	#define SHARP_HSYM_ACQ_TH_VALUE				20480
	#define SHARP_HSYM_CD_TH_VALUE				156
	//////////////////////////////////////////////////////////////////////////////////////
	
	#ifdef AVL_CPLUSPLUS
extern "C" {
	#endif

	AVL_DVBSx_ErrorCode ExtSharpBS2S7HZ6306_Initialize(struct AVL_Tuner * pTuner);
	AVL_DVBSx_ErrorCode ExtSharpBS2S7HZ6306_GetLockStatus(struct AVL_Tuner * pTuner );
	AVL_DVBSx_ErrorCode ExtSharpBS2S7HZ6306_Lock(struct AVL_Tuner * pTuner);
	AVL_DVBSx_ErrorCode ExtSharpBS2S7HZ6306_Check(struct AVL_Tuner * pTuner);

	#ifdef AVL_CPLUSPLUS
}
	#endif
#endif
