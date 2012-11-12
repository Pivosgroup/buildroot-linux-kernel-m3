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
//#include "stdio.h"
#include "IBSP.h"
#include "avl_dvbsx.h"
#include "IBase.h"
#include "IRx.h"
#include "II2C.h"
#include "IDiseqc.h"
#include "DiSEqC_source.h"

extern AVL_uchar ucPatchData[];			//defined in AVL6211_patch.dat.cpp
static 	struct AVL_DVBSx_Chip AVL_DVBSxChip;
#define Chip_ID					0x0F //0x01000002		//The Chip ID of AVL6211.
#define DiSEqC_Tone_Frequency   22              // The DiSEqC bus speed in the unit of kHz. Normally, it should be 22kHz. 
#define EAST 0
#define WEST 1

/*void AVL_DVBSx_Error_Dispose(AVL_DVBSx_ErrorCode r)
{
    switch(r)
	{
	    case AVL_DVBSx_EC_OK:
		    printf("AVL_DVBSx_EC_OK !\n");
			break;
	    case AVL_DVBSx_EC_GeneralFail:
			printf("AVL_DVBSx_EC_GeneralFail !\n");
			break;
		case AVL_DVBSx_EC_I2CFail:
			printf("AVL_DVBSx_EC_I2CFail !\n");
			break;
		case AVL_DVBSx_EC_TimeOut:
			printf("AVL_DVBSx_EC_TimeOut !\n");
			break;
		case AVL_DVBSx_EC_Running:
			printf("AVL_DVBSx_EC_Running !\n");
			break;
		case AVL_DVBSx_EC_InSleepMode:
			printf("AVL_DVBSx_EC_InSleepMode !\n");
			break; 		
		case AVL_DVBSx_EC_MemoryRunout:
			printf("AVL_DVBSx_EC_MemoryRunout !\n");
			break;
		case AVL_DVBSx_EC_BSP_ERROR1:
			printf("AVL_DVBSx_EC_BSP_ERROR1 !\n");
			break;
		case AVL_DVBSx_EC_BSP_ERROR2:
			printf("AVL_DVBSx_EC_BSP_ERROR2 !\n");
			break;
	}
}*/
AVL_DVBSx_ErrorCode AVL6211_22K_Control(AVL_uchar OnOff)
{
	AVL_DVBSx_ErrorCode r=AVL_DVBSx_EC_OK;
	struct  AVL_DVBSx_Chip * pAVLChip  = &AVL_DVBSxChip;  
	if(OnOff)
	{
		r=AVL_DVBSx_IDiseqc_StartContinuous(pAVLChip);
	}else{
		r=AVL_DVBSx_IDiseqc_StopContinuous(pAVLChip);
	}
	if(r!=AVL_DVBSx_EC_OK)
	{
		printf("[AVL6211_22K_Control] Err:0x%x\n",r);
	}	
	return r;
}
AVL_DVBSx_ErrorCode AVL6211_SetToneOut(AVL_uchar ucTone)
{
	AVL_DVBSx_ErrorCode r=AVL_DVBSx_EC_OK;
	struct AVL_DVBSx_Diseqc_TxStatus TxStatus;
	struct AVL_DVBSx_Chip * pAVLChip = &AVL_DVBSxChip;
	AVL_DVBSx_IDiseqc_SendTone( ucTone, 1, pAVLChip);
	if(r != AVL_DVBSx_EC_OK)
	{
		printf("AVL_DVBSx_IDiseqc_SendTone failed !\n");
	}
	else
	{
		do
		{
			r= AVL_DVBSx_IDiseqc_GetTxStatus(&TxStatus, pAVLChip);
		}while(TxStatus.m_TxDone != 1);
		if(r ==AVL_DVBSx_EC_OK )
		{

		}
		else
		{
			printf("AVL_DVBSx_IDiseqc_SendTone Err. !\n");
		}
	}
	return r;
	
}
void AVL6211_DiseqcSendCmd(AVL_puchar pCmd,AVL_uchar CmdSize)
{
	AVL_DVBSx_ErrorCode r=AVL_DVBSx_EC_OK;
	struct AVL_DVBSx_Diseqc_TxStatus TxStatus;
	struct AVL_DVBSx_Chip * pAVLChip = &AVL_DVBSxChip;
	
	r=AVL_DVBSx_IDiseqc_SendModulationData(pCmd, CmdSize, pAVLChip);
	if(r != AVL_DVBSx_EC_OK)
	{
		printf("AVL_DVBSx_IDiseqc_SendModulationData failed !\n");
	}
	else
	{
		do
		{
			r= AVL_DVBSx_IDiseqc_GetTxStatus(&TxStatus, pAVLChip);
		}while(TxStatus.m_TxDone != 1);
		if(r ==AVL_DVBSx_EC_OK )
		{

		}
		else
		{
			printf("AVL_DVBSx_IDiseqc_SendModulationData Err. !\n");
		}		
	}
}


AVL_DVBSx_ErrorCode AVL6211_LNB_PIO_Control(AVL_char nPIN_Index,AVL_char nValue)
{
	struct AVL_DVBSx_Chip * pAVLChip = &AVL_DVBSxChip;
	AVL_DVBSx_ErrorCode r=AVL_DVBSx_EC_OK;
	if(nPIN_Index == LNB1_PIN_60)
	{
		if(nValue)
			r=AVL_DVBSx_IDiseqc_SetLNB1Out(1,pAVLChip);//set LNB1_PIN60 1: Hight 
		else
			r=AVL_DVBSx_IDiseqc_SetLNB1Out(0,pAVLChip);	//set LNB1_PIN60 1: Low 
	}
	else if(nPIN_Index == LNB0_PIN_59)
	{
		if(nValue)
			r=AVL_DVBSx_IDiseqc_SetLNBOut(1,pAVLChip);//set LNB0_PIN59 1: Hight 
		else
			r=AVL_DVBSx_IDiseqc_SetLNBOut(0,pAVLChip);	//set LNB0_PIN59 1: Low 
	}		

	if(r!=AVL_DVBSx_EC_OK)
	{
		printf("[AVL6211_LNB_PIO_Control] set nPIN_Index:0x%x,Err\n",r);
	}
	return r;
}
#if 0
AVL_DVBSx_ErrorCode Initialize(struct AVL_DVBSx_Chip * pAVLChip)
{
    struct AVL_DVBSx_Diseqc_Para sDiseqcPara;
	AVL_DVBSx_ErrorCode r=AVL_DVBSx_EC_OK;
	struct AVL_DVBSx_VerInfo VerInfo;
	//AVL_uint32 uiTemp;
	AVL_uint32 uiDeviceID=0;
/* 
	//This function should be implemented by customer.
	//This function should be called before all other functions to prepare everything for a BSP operation.
	r = AVL_DVBSx_IBSP_Initialize();
	if( AVL_DVBSx_EC_OK !=r )
	{
		printf("BSP Initialization failed !\n");
		return (r);
	}
*/

    // This function should be called after bsp initialized to initialize the chip object.
    r = Init_AVL_DVBSx_ChipObject(pAVLChip, AVL_DVBSx_SA_0);	
	if( AVL_DVBSx_EC_OK !=r ) 
	{
		printf("Chip Object Initialization failed !\n");
		return (r);
	}

	//Judge the chip ID of current chip.
	r= AVL_DVBSx_IRx_GetDeviceID(  pAVLChip, &uiDeviceID);
	//r = AVL_DVBSx_II2C_Read32(pAVLChip, rom_ver_addr, &uiTemp);
	if (AVL_DVBSx_EC_OK != r)
	{
		printf("Get Chip ID failed !\n");
		return (r);
	}
	//if ( uiTemp != Chip_ID )
	if(uiDeviceID != Chip_ID )
	{
		printf("uiDeviceID:0x%x,Chip ID isn't correct!\n",uiDeviceID);
		return AVL_DVBSx_EC_GeneralFail;
	}


    //This function should be called after chip object initialized to initialize the IBase,using reference clock as 10M. Make sure you pickup the right pll_conf since it may be modified in BSP.
	r = AVL_DVBSx_IBase_Initialize(const_cast<AVL_DVBSx_PllConf *>(pll_conf+2), ucPatchData, pAVLChip); 
	if( AVL_DVBSx_EC_OK !=r ) 
	{
		printf("IBase Initialization failed !\n");
		return (r);
	}
	AVL_DVBSx_IBSP_Delay(100);	  //Wait 100 ms to assure that the AVLDVBSx chip boots up.This function should be implemented by customer.
  
	//This function should be called to verify the AVLDVBSx chip has completed its initialization procedure.
    r = AVL_DVBSx_IBase_GetStatus(pAVLChip);
	if( AVL_DVBSx_EC_OK != r )       
	{
		printf("Booted failed !\n");
		return (r);
	}
    printf("Booted !\n");

	//Get Chip ID, Patch version and SDK version.
	AVL_DVBSx_IBase_GetVersion( &VerInfo, pAVLChip);
	printf("Chip Ver:{%d}.{%d}.{%d}  API Ver:{%d}.{%d}.{%d}  Patch Ver:{%d}.{%d}.{%d} \n", 
		VerInfo.m_Chip.m_Major, VerInfo.m_Chip.m_Minor, VerInfo.m_Chip.m_Build, 
		VerInfo.m_API.m_Major, VerInfo.m_API.m_Minor, VerInfo.m_API.m_Build, 
		VerInfo.m_Patch.m_Major, VerInfo.m_Patch.m_Minor, VerInfo.m_Patch.m_Build);	

    //This function should be called after IBase initialized to initialize the demod.
	r = AVL_DVBSx_IRx_Initialize(pAVLChip);
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Demod Initialization failed !\n");
		return (r);
	}

	//This function should be called after demod initialized to set RF AGC polar.
	//User does not need to setup this for Sharp tuner since it is the default value. But for other tuners, user may need to do it here.
	r |= AVL_DVBSx_IRx_SetRFAGCPola(AVL_DVBSx_RA_Invert, pAVLChip);
	r |= AVL_DVBSx_IRx_DriveRFAGC(pAVLChip);

	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Set RF AGC Polar failed !\n");
		return (r);
	}

	//This function should be called after demod initialized to set spectrum polar.
	r = AVL_DVBSx_IBase_SetSpectrumPolarity(AVL_DVBSx_Spectrum_Normal, pAVLChip);
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Set Spectrum Polar failed !\n");
		return (r);
	}

    //Setup DiSEqC parameters for DiSEqC initialization.
	sDiseqcPara.m_RxTimeout = AVL_DVBSx_DRT_150ms;
	sDiseqcPara.m_RxWaveForm = AVL_DVBSx_DWM_Normal;
	sDiseqcPara.m_ToneFrequency_kHz = DiSEqC_Tone_Frequency;		
	sDiseqcPara.m_TXGap = AVL_DVBSx_DTXG_15ms;
	sDiseqcPara.m_TxWaveForm = AVL_DVBSx_DWM_Normal;

	//This function should be called after IBase initialized to initialize the DiSEqC.
	r = AVL_DVBSx_IDiseqc_Initialize(&sDiseqcPara, pAVLChip);
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("DiSEqC Initialization failed !\n");
	}


	return (r);
}

AVL_DVBSx_ErrorCode DiSEqC(void)
{
	struct AVL_DVBSx_Diseqc_TxStatus sTxStatus;
	struct AVL_DVBSx_Diseqc_RxStatus sRxStatus;
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uchar ucData[8];
	AVL_uchar i,i1;

	struct AVL_DVBSx_Chip * pAVLChip = &AVL_DVBSxChip;

    //This function do all the initialization work. It should be called only once at the beginning.
	r = Initialize(pAVLChip);
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Initialization failed !\n");
		return (r);
	}
	printf("Initialization success !\n");	

/*PIN 59/60 I/O Control exmples*/
	r=AVL6211_LNB_PIO_Control(LNB0_PIN_59,1);
	if(r== AVL_DVBSx_EC_OK)
	{
		printf("Set PIO 59 to 1,OK\n");
	}
       AVL_DVBSx_IBSP_Delay(1000);	 
	r=AVL6211_LNB_PIO_Control(LNB0_PIN_59,0);	
	if(r== AVL_DVBSx_EC_OK)
	{
		printf("Set PIO 59 to 0,OK\n");
	}
	AVL_DVBSx_IBSP_Delay(1000);	 
	r=AVL6211_LNB_PIO_Control(LNB1_PIN_60,1);
	if(r== AVL_DVBSx_EC_OK)
	{
		printf("Set PIO 60 to 1,OK\n");
	}
	  AVL_DVBSx_IBSP_Delay(100);	 
	r=AVL6211_LNB_PIO_Control(LNB1_PIN_60,0);
	if(r== AVL_DVBSx_EC_OK)
	{
		printf("Set PIO 60 to 0,OK\n");
	}
	AVL_DVBSx_IBSP_Delay(1000);	

//22K Control examples
	r=AVL6211_22K_Control(1);
	if(r== AVL_DVBSx_EC_OK)
	{
		printf("Set 22K On,OK\n");
	}
	AVL_DVBSx_IBSP_Delay(1000);
	r=AVL6211_22K_Control(0);
	if(r== AVL_DVBSx_EC_OK)
	{
		printf("Set 22K Off,OK\n");
	}
	AVL_DVBSx_IBSP_Delay(1000);	
//Send the tone burst command	
	r=AVL6211_SetToneOut(1);
	if(r== AVL_DVBSx_EC_OK)
	{
		printf("Send ToneBurst 1,OK\n");
	}
	AVL_DVBSx_IBSP_Delay(1000);		
	r=AVL6211_SetToneOut(0);
	if(r== AVL_DVBSx_EC_OK)
	{
		printf("Send ToneBurst 0,OK\n");
	}
	AVL_DVBSx_IBSP_Delay(1000);	

	//LNB switch control
	ucData[0]=0xE0;
	ucData[1]=0x10;
	ucData[2]=0x38;
	ucData[3]=0xF0;

	AVL_uchar uPortBit=0; 
	AVL_uchar uLNBPort = 1;

	switch(uLNBPort) 
	{ 
		case 1: 
			uPortBit=0;
			break; 

		case 2: 
			uPortBit=0x04;
			break; 

		case 3:
			uPortBit=0x08;
			break; 

		case 4:
			uPortBit=0x0C;
			break; 

		default:
			uPortBit=0; 
			break; 

	} 
	ucData[3] += uPortBit; 

    //This function can be called after initialization to send out 4 modulation bytes to select the LNB port if used the 1/4 LNB switch.
	AVL6211_DiseqcSendCmd(ucData, 4);


	//Positioner control one degree. 
	ucData[0]=0xE0;
	ucData[1]=0x31;
	ucData[2]=0x68;
	ucData[3]=0xFF;

	AVL_uchar uDirection = EAST;
	AVL_uchar uCommandByte;

	switch(uDirection) 
	{ 
		case EAST: 
			uCommandByte=0x68;       //Turn east
			break; 

		case WEST: 
			uCommandByte=0x69;       //Turn west
			break; 

		default:
			uCommandByte=0x68;
			break; 

	}

	ucData[2] = uCommandByte; 
	AVL6211_DiseqcSendCmd(ucData, 4);
	
	//Before receiving modulation data, we should send some request data first.
	//Read input status. 
	do
	{
		r = AVL_DVBSx_IDiseqc_GetRxStatus(&sRxStatus, pAVLChip);    //Get current status of the DiSEqC receiver.
	}
	while(1 != sRxStatus.m_RxDone);       //Wait until operation finished.
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Read modulation bytes --- Fail!\n");
	}
	else
	{
	    if(0 != sRxStatus.m_RxFifoCount)      //Data received.
	    {
		    i = sRxStatus.m_RxFifoCount;
			//This function can be called to read data back from the DiSEqC input FIFO when there are data received.
		    r = AVL_DVBSx_IDiseqc_ReadModulationData(ucData, &i, pAVLChip);     
            if(AVL_DVBSx_EC_OK == r)
	        {
		       printf("Received %u modulation bytes:",i);
		       for(i1=0; i1<i; i1++)
	           {
		           printf("0x%x,", ucData[i1]);
	           } 
		       printf("\n");
	        }
	        else
	        {
		       printf("Read modulation data --- Fail!\n");
	        }  
	    }
	    else
	    {
		    printf("There is no data in input FIFO.\n");
	    }
	}
	return (r);
}
#endif
