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



#ifndef Diseqc_source_h_h
    #define Diseqc_source_h_h

	#include "avl_dvbsx.h"

    #ifdef AVL_CPLUSPLUS
extern "C" {
	#endif

    void AVL_DVBSx_Error_Dispose(AVL_DVBSx_ErrorCode r);
    AVL_DVBSx_ErrorCode Initialize(struct AVL_DVBSx_Chip * pAVLChip);
   AVL_DVBSx_ErrorCode DiSEqC(void);
	AVL_DVBSx_ErrorCode AVL6211_SetToneOut(AVL_uchar ucTone);
	void AVL6211_DiseqcSendCmd(AVL_puchar pCmd,AVL_uchar CmdSize);
	AVL_DVBSx_ErrorCode AVL6211_LNB_PIO_Control(AVL_char nPIN_Index,AVL_char nValue);
	AVL_DVBSx_ErrorCode AVL6211_22K_Control(AVL_uchar OnOff);
	
	#define LNB1_PIN_60 60
	#define LNB0_PIN_59 59
	#ifdef AVL_CPLUSPLUS
}
	#endif

#endif
