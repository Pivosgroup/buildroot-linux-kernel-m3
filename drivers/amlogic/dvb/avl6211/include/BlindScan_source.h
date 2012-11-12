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

#ifndef BlindScan_source_h_h
    #define BlindScan_source_h_h

    #include "avl_dvbsx.h"

    #ifdef AVL_CPLUSPLUS
extern "C" {
	#endif

	void AVL_DVBSx_Error_Dispose(AVL_DVBSx_ErrorCode r);
    AVL_DVBSx_ErrorCode Initialize(struct AVL_DVBSx_Chip * pAVLChip,struct AVL_Tuner * pTuner);
	AVL_DVBSx_ErrorCode BlindScan(void);

	#ifdef AVL_CPLUSPLUS
}
	#endif

#endif
