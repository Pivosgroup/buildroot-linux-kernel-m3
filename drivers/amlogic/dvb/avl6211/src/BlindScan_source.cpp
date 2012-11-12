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
#include <conio.h>
#include "stdio.h"
#include "IBSP.h"
#include "avl_dvbsx.h"
#include "IBase.h"
#include "IRx.h"
#include "ITuner.h"
#include "ExtSharpBS2S7HZ6306.h"
#include "II2C.h"
#include "IDiseqc.h"
#include "IBlindScan.h"
#include "IBlindscanAPI.h"
#include "BlindScan_source.h"

extern AVL_uchar ucPatchData [];				//Defined in AVL6211_patch.dat.cpp.

#define Chip_ID					0x0F //0x01000002		//The Chip ID of AVL6211.
#define bs_start_freq			950				//The start RF frequency, 950MHz
#define bs_stop_freq			2150			//The stop RF frequency, 2150MHz
#define Blindscan_Mode   AVL_DVBSx_BS_Slow_Mode	//The Blind scan mode.	AVL_DVBSx_BS_Fast_Mode = 0,AVL_DVBSx_BS_Slow_Mode = 1
#define Diseqc_Tone_Frequency   22              //The DiSEqC bus speed in the unit of kHz. Normally, it should be 22kHz. 

#define FontEnd_MaxCount 2
struct AVL_DVBSx_Chip  g_stAvlDVBSxChip[FontEnd_MaxCount];
struct AVL_Tuner g_stTuner[FontEnd_MaxCount];
enum AVL_Demod_ReferenceClock_Select_t
{
	Ref_clock_4M=0,
	Ref_clock_4M5=1,
	Ref_clock_10M=2,
	Ref_clock_16M=3,
	Ref_clock_27M=4,
	Ref_clock_Enhance_4M=5,
	Ref_clock_Enhance_4M5=6,
	Ref_clock_Enhance_10M=7,
	Ref_clock_Enhance_16M=8,
	Ref_clock_Enhance_27M=9,
};

enum AVL_TunerLPF_Calculation_Flag
{
	InputLPF = 0,
	InputSymbolRate = 1,
};

struct AVL_Demod_Tuner_Configuration_t
{
	////////////////////////////Demod Configure///////////////////////////////

	AVL_char m_ChannelId;												///< Bus identifier.
	AVL_uint16 m_uiDemodAddress;										///< Device I2C slave address.
	enum AVL_Demod_ReferenceClock_Select_t   m_DemodReferenceClk;		///< Configures the Availink device's PLL.Refer to enum AVL_Demod_ReferenceClock_Select_t

	///< The MPEG output mode. The default value in the Availink device is \a AVL_DVBSx_MPM_Parallel
	enum AVL_DVBSx_MpegMode m_TSOutPutMode;								///< AVL_DVBSx_MPM_Parallel = 0;	Output MPEG data in parallel mode
																		///< AVL_DVBSx_MPM_Serial = 1;		Output MPEG data in serial mode

	///< The MPEG output clock polarity. The clock polarity should be configured to meet the back end device's requirement.The default value in the Availink device is \a AVL_DVBSx_MPCP_Rising.
	enum AVL_DVBSx_MpegClockPolarity m_TSClockPolarity;					///< AVL_DVBSx_MPCP_Falling = 0;	The MPEG data is valid on the falling edge of the clock.
																		///< AVL_DVBSx_MPCP_Rising = 1;		The MPEG data is valid on the rising edge of the clock.

	///< The MPEG output format. The default value in the Availink device is \a AVL_DVBSx_MPF_TS
	enum AVL_DVBSx_MpegFormat m_TSFormat;								///< AVL_DVBSx_MPF_TS = 0;		Transport stream format.
																		///< AVL_DVBSx_MPF_TSP = 1;		Transport stream plus parity format.

	///< Defines the pin on which the Availink device outputs the MPEG data when the MPEG interface has been configured to operate in serial mode.
	enum AVL_DVBSx_MpegSerialPin m_SerDataPin;							///< AVL_DVBSx_MPSP_DATA0 = 0;		Serial data is output on pin MPEG_DATA_0
																		///< AVL_DVBSx_MPSP_DATA7 = 1;		Serial data is output on pin MPEG_DATA_7

	////////////////////////////Tuner Configure///////////////////////////////

	AVL_uint16 m_uiTunerAddress;										///< Tuner I2C slave address.
	AVL_uint16 m_uiTuner_I2Cbus_clock;									///< The clock speed of the tuner dedicated I2C bus, in a unit of kHz.
	AVL_uint16 m_uiTunerMaxLPF_100Khz;									///< The max low pass filter bandwidth of the tuner.

	///< Defines the LPF's forms of computation.
	enum AVL_TunerLPF_Calculation_Flag m_LPF_Config_flag;				///< InputLPF = 0;			The LPF will be calculated by formula which defined by user.
																		///< InputSymbolRate = 1;	The LPF will be calculated in tuner driver according to the SymbolRate.

	///< Defines the polarity of the RF AGC control signal.The polarity of the RF AGC control signal must be configured to match that required by the tuner.
	enum AVL_DVBSx_RfagcPola m_TunerRFAGC;								///< AVL_DVBSx_RA_Normal = 0;	Normal polarization. This setting is used for a tuner whose gain increases with increased AGC voltage.
																		///< AVL_DVBSx_RA_Invert = 1;	Inverted polarization. The default value. Most tuners fall into this category.  This setting is used for a tuner whose gain decreases with increased AGC voltage.

	///< Defines the device spectrum polarity setting. 
	enum AVL_DVBSx_SpectrumPolarity m_Tuner_IQ_SpectrumMode;			///< AVL_DVBSx_Spectrum_Normal = 0;	The received signal spectrum is not inverted.
																		///< AVL_DVBSx_Spectrum_Invert = 1;	The received signal spectrum is inverted.

	AVL_DVBSx_ErrorCode (* m_pInitializeFunc)(struct AVL_Tuner *);	 	///< A pointer to the tuner initialization function.
	AVL_DVBSx_ErrorCode (* m_pGetLockStatusFunc)(struct AVL_Tuner *); 	///< A pointer to the tuner GetLockStatus function.
	AVL_DVBSx_ErrorCode (* m_pLockFunc)(struct AVL_Tuner *);			///< A pointer to the tuner Lock function.      
};


/*Here please according to customer needs, defining the array index*/

static AVL_char g_nDemodTunerArrayIndex = 0;

struct AVL_Demod_Tuner_Configuration_t g_DemodTuner_Config[]=
{
	{
			0,
			AVL_DVBSx_SA_0,
			Ref_clock_10M,
			AVL_DVBSx_MPM_Parallel,
			AVL_DVBSx_MPCP_Rising,
			AVL_DVBSx_MPF_TSP,
			AVL_DVBSx_MPSP_DATA0,

			0xC0,
			200,
			340,
			InputLPF,
			AVL_DVBSx_RA_Invert,
			AVL_DVBSx_Spectrum_Normal,
			&ExtSharpBS2S7HZ6306_Initialize,
			&ExtSharpBS2S7HZ6306_GetLockStatus,
			&ExtSharpBS2S7HZ6306_Lock,
	},
	{
			0,
			AVL_DVBSx_SA_0,
			Ref_clock_10M,
			AVL_DVBSx_MPM_Parallel,
			AVL_DVBSx_MPCP_Rising,
			AVL_DVBSx_MPF_TSP,
			AVL_DVBSx_MPSP_DATA0,

			0xC0,
			200,
			340,
			InputLPF,
			AVL_DVBSx_RA_Invert,
			AVL_DVBSx_Spectrum_Normal,
			&ExtSharpBS2S7HZ6306_Initialize,
			&ExtSharpBS2S7HZ6306_GetLockStatus,
			&ExtSharpBS2S7HZ6306_Lock,
	},
};

void AVL_DVBSx_Error_Dispose(AVL_DVBSx_ErrorCode r)
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
}

AVL_DVBSx_ErrorCode CPU_Halt(struct AVL_DVBSx_Chip * pAVLChip)
{
	AVL_DVBSx_ErrorCode r;
	AVL_uint16 i= 0;

	r = AVL_DVBSx_IBase_SendRxOP(OP_RX_HALT, pAVLChip );

	if(AVL_DVBSx_EC_OK == r)
	{
		while(i++<20)
		{
			r = AVL_DVBSx_IBase_GetRxOPStatus(pAVLChip);
			if(AVL_DVBSx_EC_OK == r)
			{
				break;
			}
			else
			{
				AVL_DVBSx_IBSP_Delay(10);
			}
		}
	}
	return (r);
}

static void AVL_Set_LPF(struct AVL_Tuner * pTuner)
{	
	struct AVL_Demod_Tuner_Configuration_t *pDemodTunerConfig = &g_DemodTuner_Config[g_nDemodTunerArrayIndex];

	if (pDemodTunerConfig->m_LPF_Config_flag == InputSymbolRate)
	{
		pTuner->m_uiLPF_100kHz = pTuner->m_uiSymbolRate_Hz;
	}
	else
	{
		pTuner->m_uiLPF_100kHz = pTuner->m_uiSymbolRate_Hz*75/10000000+40;
	}

	if(pTuner->m_uiLPF_100kHz > pDemodTunerConfig->m_uiTunerMaxLPF_100Khz)
	{
		pTuner->m_uiLPF_100kHz = pDemodTunerConfig->m_uiTunerMaxLPF_100Khz;
	}

}

AVL_DVBSx_ErrorCode AVL_Lock(struct AVL_DVBSx_Chip * pAVLChip,struct AVL_Tuner * pTuner,struct AVL_DVBSx_Channel * pChannel,AVL_uint16 * uiLockStatus)
{	
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	struct AVL_DVBSx_Channel Channel;
	AVL_uint16 uiCounter;

	pTuner->m_uiFrequency_100kHz = pChannel->m_uiFrequency_kHz/100;  
	pTuner->m_uiSymbolRate_Hz = pChannel->m_uiSymbolRate_Hz;
	AVL_Set_LPF(pTuner);
	r = pTuner->m_pLockFunc(pTuner);

	AVL_DVBSx_IBSP_Delay(50);		//Wait a while for tuner to lock in certain frequency.

	Channel.m_uiSymbolRate_Hz = pChannel->m_uiSymbolRate_Hz;
	Channel.m_Flags = (CI_FLAG_IQ_NO_SWAPPED) << CI_FLAG_IQ_BIT;	//Normal IQ
	Channel.m_Flags |= (CI_FLAG_IQ_AUTO_BIT_AUTO) << CI_FLAG_IQ_AUTO_BIT;	//Enable automatic IQ swap detection
	Channel.m_Flags |= (CI_FLAG_DVBS2_UNDEF) << CI_FLAG_DVBS2_BIT;			//Enable automatic standard detection

	//This function should be called after tuner locked to lock the channel.
	r |= AVL_DVBSx_IRx_LockChannel(&Channel, pAVLChip);  

	//------------------------Check if Channel was locked-----------------------------
	if(Channel.m_uiSymbolRate_Hz < 5000000)
		uiCounter = 25;
	else if(Channel.m_uiSymbolRate_Hz < 10000000)
		uiCounter = 12;
	else
		uiCounter = 5;

	do
	{
		AVL_DVBSx_IBSP_Delay(100);    //Wait 100ms for demod to lock the channel.

		r = AVL_DVBSx_IRx_GetLockStatus(uiLockStatus, pAVLChip);

		if ((AVL_DVBSx_EC_OK == r)&&(1 == *uiLockStatus))
			break;
	}while(--uiCounter);
	//--------------------------------------------------------------------------------

	return r;
}

void AVL_Display_TP_Info(struct AVL_DVBSx_Channel * pChannel, AVL_uint16 Channel_Num)
{
	AVL_uint16 i;
	printf("\n\n");
	for(i=0; i < Channel_Num; i++)
	{
		printf("Ch%2d: RF: %4d SR: %5d ",i+1, (pChannel[i].m_uiFrequency_kHz/1000),(pChannel[i].m_uiSymbolRate_Hz/1000));
		switch((pChannel[i].m_Flags & CI_FLAG_DVBS2_BIT_MASK) >> CI_FLAG_DVBS2_BIT)
		{
		case CI_FLAG_DVBS:
			printf("   DVBS ");
			break;
		case CI_FLAG_DVBS2:
			printf("  DVBS2 ");
			break;
		case CI_FLAG_DVBS2_UNDEF:
			printf("Unknown ");
			break;
		} 
		switch((pChannel[i].m_Flags & CI_FLAG_IQ_BIT_MASK)>>CI_FLAG_IQ_BIT)
		{
		case CI_FLAG_IQ_NO_SWAPPED:
			printf("Normal ");
			break;
		case CI_FLAG_IQ_SWAPPED:
			printf("Invert ");
			break;
		}
		printf("\n");
	}	
}

AVL_DVBSx_ErrorCode Initialize(struct AVL_DVBSx_Chip * pAVLChip,struct AVL_Tuner * pTuner)
{
	struct AVL_DVBSx_Diseqc_Para sDiseqcPara;
	struct AVL_DVBSx_MpegInfo sMpegMode;
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	struct AVL_Demod_Tuner_Configuration_t *pDemodTunerConfig = &g_DemodTuner_Config[g_nDemodTunerArrayIndex];
	struct AVL_DVBSx_VerInfo VerInfo;
	//AVL_uint32 uiTemp;
	AVL_uint32 uiDeviceID=0;

#if 0  
	//This function should be implemented by customer.
	//This function should be called before all other functions to prepare everything for a BSP operation.
	r = AVL_DVBSx_IBSP_Initialize();

	if( AVL_DVBSx_EC_OK !=r )
	{
		printf("BSP Initialization failed !\n");
		return (r);
	}
#endif

	pAVLChip->m_uiBusId=pDemodTunerConfig->m_ChannelId;

	// This function should be called after bsp initialized to initialize the chip object.
	r = Init_AVL_DVBSx_ChipObject(pAVLChip, pDemodTunerConfig->m_uiDemodAddress);	
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
		printf("Chip ID isn't correct !\n");
		return AVL_DVBSx_EC_GeneralFail;
	}

	//This function should be called after chip object initialized to initialize the IBase,using reference clock as 10M. Make sure you pickup the right pll_conf since it may be modified in BSP.
	r = AVL_DVBSx_IBase_Initialize(&(pll_conf[pDemodTunerConfig->m_DemodReferenceClk]), ucPatchData, pAVLChip); 
	if( AVL_DVBSx_EC_OK !=r ) 
	{
		printf("IBase Initialization failed !\n");
		return (r);
	}
	AVL_DVBSx_IBSP_Delay(100);	  //Wait 100 ms to assure that the AVL_DVBSx chip boots up.This function should be implemented by customer.

	//This function should be called to verify the AVL_DVBSx chip has completed its initialization procedure.
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
	r |= AVL_DVBSx_IRx_SetRFAGCPola(pDemodTunerConfig->m_TunerRFAGC, pAVLChip);
	r |= AVL_DVBSx_IRx_DriveRFAGC(pAVLChip);

	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Set RF AGC Polar failed !\n");
		return (r);
	}

	//This function should be called after demod initialized to set spectrum polar.
	r = AVL_DVBSx_IBase_SetSpectrumPolarity(pDemodTunerConfig->m_Tuner_IQ_SpectrumMode, pAVLChip);
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Set Spectrum Polar failed !\n");
		return (r);
	}

	//Setup MPEG mode parameters.
	sMpegMode.m_MpegFormat = pDemodTunerConfig->m_TSFormat;
	sMpegMode.m_MpegMode = pDemodTunerConfig->m_TSOutPutMode;
	sMpegMode.m_MpegClockPolarity = pDemodTunerConfig->m_TSClockPolarity;

	//This function should be called after demod initialized to set MPEG mode.(These parameters will be valid after call lock channel function)
	r = AVL_DVBSx_IRx_SetMpegMode(&sMpegMode,pAVLChip );

	if(sMpegMode.m_MpegMode == AVL_DVBSx_MPM_Serial)
	{
		AVL_DVBSx_IRx_SetMpegSerialPin(pAVLChip,pDemodTunerConfig->m_SerDataPin);
	}
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Set MPEG output mode failed !\n");
		return (r);
	}

	// Enable the MPEG output (this function call has no effect for the AVL_DVBSxLG and AVL_DVBSxLGa devices)
	r = AVL_DVBSx_IRx_DriveMpegOutput(pAVLChip);

	//Setup tuner parameters for tuner initialization.
	pTuner->m_uiSlaveAddress = pDemodTunerConfig->m_uiTunerAddress;
	pTuner->m_uiI2CBusClock_kHz = pDemodTunerConfig->m_uiTuner_I2Cbus_clock;
	pTuner->m_pParameters = 0;
	pTuner->m_pAVLChip = pAVLChip;
	pTuner->m_pInitializeFunc = pDemodTunerConfig->m_pInitializeFunc;
	pTuner->m_pLockFunc = pDemodTunerConfig->m_pLockFunc;
	pTuner->m_pGetLockStatusFunc = pDemodTunerConfig->m_pGetLockStatusFunc;

	//This function should be called after IBase initialized to initialize the tuner.
	r = pTuner->m_pInitializeFunc(pTuner);
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Tuner Initialization failed !\n");
		return (r);
	}

	//Setup DiSEqC parameters for DiSEqC initialization.
	sDiseqcPara.m_RxTimeout = AVL_DVBSx_DRT_150ms;
	sDiseqcPara.m_RxWaveForm = AVL_DVBSx_DWM_Normal;
	sDiseqcPara.m_ToneFrequency_kHz = Diseqc_Tone_Frequency;		
	sDiseqcPara.m_TXGap = AVL_DVBSx_DTXG_15ms;
	sDiseqcPara.m_TxWaveForm = AVL_DVBSx_DWM_Normal;

	//The DiSEqC should be initialized if AVL_DVBSx need to supply power to LNB. This function should be called after IBase initialized to initialize the DiSEqC.
	r = AVL_DVBSx_IDiseqc_Initialize(&sDiseqcPara, pAVLChip);
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("DiSEqC Initialization failed !\n");
	}

	return (r);
}

AVL_DVBSx_ErrorCode BlindScan(void)
{
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uint16	index = 0;
	struct AVL_DVBSx_Channel * pChannel;
	AVL_uchar HandIndex = 0;
	struct AVL_DVBSx_Chip * pAVLChip = &g_stAvlDVBSxChip[HandIndex];
	struct AVL_Tuner * pTuner = &g_stTuner[HandIndex];
	struct AVL_DVBSx_BlindScanAPI_Setting BSsetting;
	enum AVL_DVBSx_BlindScanAPI_Status BS_Status;
	struct AVL_DVBSx_BlindScanAPI_Setting * pBSsetting = &BSsetting;
	struct AVL_Demod_Tuner_Configuration_t *pDemodTunerConfig = &g_DemodTuner_Config[g_nDemodTunerArrayIndex];
	BS_Status = AVL_DVBSx_BS_Status_Init;

	//This function do all the initialization work.It should be called only once at the beginning.It needn't be recalled when we want to lock a new channel.
	r = Initialize(pAVLChip,pTuner);
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Initialization failed !\n");
		return (r);
	}
	printf("Initialization success !\n");

	while(BS_Status != AVL_DVBSx_BS_Status_Exit)
	{
		switch(BS_Status)
		{
		case AVL_DVBSx_BS_Status_Init:			{
												AVL_DVBSx_IBlindScanAPI_Initialize(pBSsetting);//this function set the parameters blind scan process needed.	

												AVL_DVBSx_IBlindScanAPI_SetFreqRange(pBSsetting, bs_start_freq, bs_stop_freq); //Default scan rang is from 950 to 2150. User may call this function to change scan frequency rang.
												AVL_DVBSx_IBlindScanAPI_SetScanMode(pBSsetting, Blindscan_Mode);

												AVL_DVBSx_IBlindScanAPI_SetSpectrumMode(pBSsetting, pDemodTunerConfig->m_Tuner_IQ_SpectrumMode); //Default set is AVL_DVBSx_Spectrum_Normal, it must be set correctly according Board HW configuration
												AVL_DVBSx_IBlindScanAPI_SetMaxLPF(pBSsetting, pDemodTunerConfig->m_uiTunerMaxLPF_100Khz); //Set Tuner max LPF value, this value will difference according tuner type

												BS_Status = AVL_DVBSx_BS_Status_Start;
												break;
											}

		case AVL_DVBSx_BS_Status_Start:		{													
												r = AVL_DVBSx_IBlindScanAPI_Start(pAVLChip, pTuner, pBSsetting);
												if(AVL_DVBSx_EC_OK != r)
												{
													BS_Status = AVL_DVBSx_BS_Status_Exit;
												}
												BS_Status = AVL_DVBSx_BS_Status_Wait;
												break;
											}

		case AVL_DVBSx_BS_Status_Wait: 		{
												r = AVL_DVBSx_IBlindScanAPI_GetCurrentScanStatus(pAVLChip, pBSsetting);
												if(AVL_DVBSx_EC_GeneralFail == r)
												{
													BS_Status = AVL_DVBSx_BS_Status_Exit;
												}
												if(AVL_DVBSx_EC_OK == r)
												{
													BS_Status = AVL_DVBSx_BS_Status_Adjust;
												}
												if(AVL_DVBSx_EC_Running == r)
												{
													AVL_DVBSx_IBSP_Delay(100);
												}
												break;
											}

		case AVL_DVBSx_BS_Status_Adjust:		{
												r = AVL_DVBSx_IBlindScanAPI_Adjust(pAVLChip, pBSsetting);
												if(AVL_DVBSx_EC_OK != r)
												{
													BS_Status = AVL_DVBSx_BS_Status_Exit;
												}
												BS_Status = AVL_DVBSx_BS_Status_User_Process;
												break;
											}

		case AVL_DVBSx_BS_Status_User_Process:	{
												//------------Custom code start-------------------
												//customer can add the callback function here such as adding TP information to TP list or lock the TP for parsing PSI
												//Add custom code here; Following code is an example

												/*----- example 1: print Blindscan progress ----*/
												printf(" %2d%% \n", AVL_DVBSx_IBlindscanAPI_GetProgress(pBSsetting)); //display progress Percent of blindscan process

												/*----- example 2: print TP information if found valid TP ----*/
												while(index < pBSsetting->m_uiChannelCount) //display new TP info found in current stage
												{
													pChannel = &pBSsetting->channels[index++];
													printf("      Ch%2d: RF: %4d SR: %5d ",index, (pChannel->m_uiFrequency_kHz/1000),(pChannel->m_uiSymbolRate_Hz/1000));

												#if 0 //Lock signal for testing
													AVL_uint16 uiLockStatus = 0;
													struct AVL_DVBSx_Channel channel;

													channel.m_uiFrequency_kHz = pChannel->m_uiFrequency_kHz;
													channel.m_uiSymbolRate_Hz = pChannel->m_uiSymbolRate_Hz;

													AVL_Lock(pAVLChip, pTuner, &channel, &uiLockStatus);
													if(uiLockStatus)
														printf("Locked!");
													else		
														printf("Unlock!");
												#endif
													printf("\n");
												}													
												index = pBSsetting->m_uiChannelCount;

												/*----- example 3: Break blindscan process when check key pressed ----*/
												#if 0
												if ( _kbhit() ) // demonstrate blindscan exit while process status machine is running
												{
													if( _getch() == 'e' )
													{
														printf("Exit by user.\n");
														BS_Status = Blindscan_Status_Cancel;
														break;
													}
												}
												#endif
												//------------Custom code end -------------------

												if ( (AVL_DVBSx_IBlindscanAPI_GetProgress(pBSsetting) < 100))
													BS_Status = AVL_DVBSx_BS_Status_Start;
												else											
													BS_Status = AVL_DVBSx_BS_Status_Cancel;
												break;
											}

		case AVL_DVBSx_BS_Status_Cancel:		{ 
												r = AVL_DVBSx_IBlindScanAPI_Exit(pAVLChip,pBSsetting);
												BS_Status = AVL_DVBSx_BS_Status_Exit;
												break;
											}

		default:						    {
												BS_Status = AVL_DVBSx_BS_Status_Cancel;
												break;
											}
		}
	}

	//print all of the TP info found in blindscan process. this isn't necessary for the customer
	AVL_Display_TP_Info(pBSsetting->channels,pBSsetting->m_uiChannelCount);

	//-------------------------Blindscan Band Process End------------------------
	return (r);

}
