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
#include "ITuner.h"
#include "ExtSharpBS2S7HZ6306.h"
#include "II2C.h"
#include "IDiseqc.h"
#include "IBlindScan.h"
#include "LockSignal_Api.h"
#include "ExtAV2011.h"
#include "ucPatchData.h"


struct Signal_Level  SignalLevel [47]=
{
	{8285,	-922},{10224,	-902},{12538,	-882},{14890,	-862},{17343,	-842},{19767,	-822},{22178,	-802},{24618,	-782},{27006,	-762},{29106,	-742},
	{30853,	-722},{32289,	-702},{33577,	-682},{34625,	-662},{35632,	-642},{36552,	-622},{37467,	-602},{38520,	-582},{39643,	-562},{40972,	-542},
	{42351,	-522},{43659,	-502},{44812,	-482},{45811,	-462},{46703,	-442},{47501,	-422},{48331,	-402},{49116,	-382},{49894,	-362},{50684,	-342},
	{51543,	-322},{52442,	-302},{53407,	-282},{54314,	-262},{55208,	-242},{56000,	-222},{56789,	-202},{57544,	-182},{58253,	-162},{58959,	-142},
	{59657,	-122},{60404,	-102},{61181,	-82},{62008,	-62},{63032,	-42},{65483,	-22},{65535,	-12}

};

extern const unsigned char ucPatchData [];


//extern AVL_uchar ucPatchData [];				//Defined in AVL6211_patch.dat.cpp.
//extern struct Signal_Level  SignalLevel [47];   //Defined in SignalLevel.cpp

#define Chip_ID					0x0F //0x01000002			//The Chip ID of AVL6211.
#define Diseqc_Tone_Frequency   22					//The DiSEqC bus speed in the unit of kHz. Normally, it should be 22kHz. 
#define IQ_Swap					Auto				//Controls the IQ swap setting			enum AVL_DVBS_IQ_Swap
#define standard				DVBS2				//Controls the standard setting			enum AVL_DVBS_standard

#define FontEnd_MaxCount 2
struct AVL_DVBSx_Chip  g_stAvlDVBSxChip[FontEnd_MaxCount];
struct AVL_Tuner g_stTuner[FontEnd_MaxCount];

enum AVL_DVBS_IQ_Swap
{
	Normal,						//< = 0 The received signal spectrum is not inverted.
	Invert,						//< = 1 The received signal spectrum is inverted.
	Auto						//< = 2 The demodulator will automatically detect the received signal spectrum.
};

enum AVL_DVBS_standard
{
	DVBS,
	DVBS2,
};

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
			Ref_clock_27M,
			AVL_DVBSx_MPM_Parallel,
			AVL_DVBSx_MPCP_Rising,//AVL_DVBSx_MPCP_Rising,
			AVL_DVBSx_MPF_TS,
			AVL_DVBSx_MPSP_DATA0,										

			0xC0,
			200,
			440,
			InputSymbolRate,
			AVL_DVBSx_RA_Invert,
			AVL_DVBSx_Spectrum_Normal,
			&AVL_DVBSx_ExtAV2011_Initialize,
			&AVL_DVBSx_ExtAV2011_GetLockStatus,
			&AVL_DVBSx_ExtAV2011_Lock,
	},
	{
			0,
			AVL_DVBSx_SA_0,
			Ref_clock_27M,
			AVL_DVBSx_MPM_Parallel,
			AVL_DVBSx_MPCP_Rising,
			AVL_DVBSx_MPF_TS,
			AVL_DVBSx_MPSP_DATA0,

			0xC0,
			200,
			440,
			InputSymbolRate,
			AVL_DVBSx_RA_Invert,
			AVL_DVBSx_Spectrum_Normal,
			&AVL_DVBSx_ExtAV2011_Initialize,
			&AVL_DVBSx_ExtAV2011_GetLockStatus,
			&AVL_DVBSx_ExtAV2011_Lock,
	},
};


AVL_uchar DVBS_SNR[6] ={12,32,41,52,58,62};
AVL_uchar DVBS2Qpsk_SNR[8] ={10,24,32,41,47,52,63,65};
AVL_uchar DVBS28psk_SNR[6] ={57,67,80,95,100,110};

int AVL_Get_Quality_Percent(struct AVL_DVBSx_Chip * pAVLChip)
{
    AVL_DVBSx_ErrorCode r=AVL_DVBSx_EC_OK;
    AVL_uint32 uiSNR;
    AVL_uint16 uiLockStatus=0;
    AVL_uchar SNRrefer = 0;;
    AVL_uchar Quality=5;
	AVL_uchar i;
    struct AVL_DVBSx_SignalInfo SignalInfo;      

	for(i=0;i<5;i++)
	{
		AVL_DVBSx_IBSP_Delay(10);
    	r |= AVL_DVBSx_IRx_GetLockStatus(&uiLockStatus, pAVLChip);
		if(uiLockStatus!=1)		break;
	}
    if(i==5)
    {
        r |= AVL_DVBSx_IRx_GetSNR(&uiSNR, pAVLChip);
        r |= AVL_DVBSx_IRx_GetSignalInfo(&SignalInfo, pAVLChip);
    }
	else
		return Quality;
	
    if (SignalInfo.m_coderate < RX_DVBS2_1_4)
    {
        SNRrefer = DVBS_SNR[SignalInfo.m_coderate];
    }
    else
    {
        if (SignalInfo.m_modulation == AVL_DVBSx_MM_8PSK)
            SNRrefer = DVBS28psk_SNR[SignalInfo.m_coderate -RX_DVBS2_3_5];
        else
            SNRrefer = DVBS2Qpsk_SNR[SignalInfo.m_coderate -RX_DVBS2_1_2];
    }
	
    if ((uiSNR/10) > SNRrefer)
    {
    	uiSNR = uiSNR/10 - SNRrefer;
        if(uiSNR>=100)
            Quality = 99;
        else if(uiSNR>=50)  //  >5.0dB
            Quality = 80+ (uiSNR-50)*20/50;
        else if(uiSNR>=25)  //  > 2.5dB
            Quality = 50+ (uiSNR-25)*30/25;
        else if(uiSNR>=10)  //  > 1dB
            Quality = 25+ (uiSNR-10)*25/15;			
        else 
            Quality = 5+ (uiSNR)*20/10;
    }
    else
    {
        Quality = 5;
    }
	
	return Quality;
}    


struct Signal_Level  AGC_LUT [91]=
{
    {63688,  0},{62626, -1},{61840, -2},{61175, -3},{60626, -4},{60120, -5},{59647, -6},{59187, -7},{58741, -8},{58293, -9},
    {57822,-10},{57387,-11},{56913,-12},{56491,-13},{55755,-14},{55266,-15},{54765,-16},{54221,-17},{53710,-18},{53244,-19},
    {52625,-20},{52043,-21},{51468,-22},{50904,-23},{50331,-24},{49772,-25},{49260,-26},{48730,-27},{48285,-28},{47804,-29},
    {47333,-30},{46880,-31},{46460,-32},{46000,-33},{45539,-34},{45066,-35},{44621,-36},{44107,-37},{43611,-38},{43082,-39},
    {42512,-40},{41947,-41},{41284,-42},{40531,-43},{39813,-44},{38978,-45},{38153,-46},{37294,-47},{36498,-48},{35714,-49},
    {35010,-50},{34432,-51},{33814,-52},{33315,-53},{32989,-54},{32504,-55},{32039,-56},{31608,-57},{31141,-58},{30675,-59},
    {30215,-60},{29711,-61},{29218,-62},{28688,-63},{28183,-64},{27593,-65},{26978,-66},{26344,-67},{25680,-68},{24988,-69},
    {24121,-70},{23285,-71},{22460,-72},{21496,-73},{20495,-74},{19320,-75},{18132,-76},{16926,-77},{15564,-78},{14398,-79},
    {12875,-80},{11913,-81},{10514,-82},{ 9070,-83},{ 7588,-84},{ 6044,-85},{ 4613,-86},{ 3177,-87},{ 1614,-88},{  123,-89},
    {    0,-90}
};
	
AVL_int16 AVL_Get_Level_Percent(struct AVL_DVBSx_Chip * pAVLChip)
{

	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uint16 Level;
	AVL_int16 i;
	AVL_int16 Percent;
/*
	#define Level_High_Stage	36
	#define Level_Low_Stage 	70

	#define Percent_Space_High	6
	#define Percent_Space_Mid	44
	#define Percent_Space_Low	50		//Percent_Space_High+Percent_Space_Mid+Percent_Space_Low = 100

*/	
	#define Level_High_Stage	36
	#define Level_Low_Stage		76

	#define Percent_Space_High	10
	#define Percent_Space_Mid	30
	#define Percent_Space_Low	60


	i = 0;
	Percent = 0;

	r = AVL_DVBSx_IRx_GetSignalLevel(&Level,pAVLChip);

	while(Level < AGC_LUT[i++].SignalLevel);
	
	if(i<= Level_High_Stage)
		Percent = Percent_Space_Low+Percent_Space_Mid+ (Level_High_Stage-i)*Percent_Space_High/Level_High_Stage;
	else if(i<=Level_Low_Stage)
		Percent = Percent_Space_Low+ (Level_Low_Stage-i)*Percent_Space_Mid/(Level_Low_Stage-Level_High_Stage);
	else
		Percent =(90-i)*Percent_Space_Low/(90-Level_Low_Stage);

	return Percent;	
}




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
	printf("%s  r is %d",__FUNCTION__,r);
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
	 printf("%s  r is %d",__FUNCTION__,r);
	return (r);
}

void AVL_Set_LPF(struct AVL_Tuner * pTuner, AVL_uint32 m_uiSymbolRate_Hz)
{	
	struct AVL_Demod_Tuner_Configuration_t *pDemodTunerConfig = &g_DemodTuner_Config[g_nDemodTunerArrayIndex];

	if (pDemodTunerConfig->m_LPF_Config_flag == InputSymbolRate)
	{
		pTuner->m_uiLPF_100kHz = m_uiSymbolRate_Hz/(1000*100);
	}
	else
	{
		pTuner->m_uiLPF_100kHz = (m_uiSymbolRate_Hz*75)/(1000*100*100)+40;
	}

	if (pTuner->m_uiLPF_100kHz > pDemodTunerConfig->m_uiTunerMaxLPF_100Khz)
	{
		pTuner->m_uiLPF_100kHz = pDemodTunerConfig->m_uiTunerMaxLPF_100Khz;
	}
}


AVL_DVBSx_ErrorCode AVL6211_Initialize(struct AVL_DVBSx_Chip * pAVLChip,struct AVL_Tuner * pTuner)
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

	if( AVL_DVBSx_EC_OK != r )
	{
		printf("BSP Initialization failed !\n");
		return (r);
	}
#endif

	pAVLChip->m_uiBusId=pDemodTunerConfig->m_ChannelId;

    // This function should be called after bsp initialized to initialize the chip object.
    r = Init_AVL_DVBSx_ChipObject(pAVLChip, pDemodTunerConfig->m_uiDemodAddress);	
	if( AVL_DVBSx_EC_OK != r ) 
	{
		printf("Chip Object Initialization failed !\n");
		return (r);
	}

	//Judge the chip ID of current chip.
	//r = AVL_DVBSx_II2C_Read32(pAVLChip, rom_ver_addr, &uiTemp);
	
	r= AVL_DVBSx_IRx_GetDeviceID(  pAVLChip, &uiDeviceID);
	printk("r is %x,uiDeviceID is %x\n",r,uiDeviceID);
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
	if( AVL_DVBSx_EC_OK != r ) 
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


struct AVL_Tuner *avl6211pTuner;
struct AVL_DVBSx_Chip * pAVLChip_all;



AVL_DVBSx_ErrorCode AVL6211_LockSignal_Init(void)
{
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uchar HandIndex = 0;
	struct AVL_DVBSx_Chip * pAVLChip = &g_stAvlDVBSxChip[HandIndex];
	struct AVL_Tuner * pTuner = &g_stTuner[HandIndex];
	avl6211pTuner=pTuner;
    //This function do all the initialization work.It should be called only once at the beginning.It needn't be recalled when we want to lock a new channel.
	r = AVL6211_Initialize(pAVLChip,pTuner);
	pAVLChip_all=pAVLChip;
	if(AVL_DVBSx_EC_OK != r)
	{
		printf("Initialization failed !\n");
		return (r);
	}
	printf("Initialization success !\n");
	return (r);
}



AVL_uint32 AVL6211_GETLockStatus(void)
{
	AVL_uchar HandIndex = 0;
	struct AVL_DVBSx_Chip * pAVLChip = &g_stAvlDVBSxChip[HandIndex];
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uint16 uiLockStatus;
	r = AVL_DVBSx_IRx_GetLockStatus(&uiLockStatus, pAVLChip);
	printf("lock status is %d",uiLockStatus);
	if ((AVL_DVBSx_EC_OK == r)&&(1 == uiLockStatus));
		return	uiLockStatus;

	return	uiLockStatus;

}


AVL_uint32 AVL6211_GETBer(void)
{
	AVL_uchar HandIndex = 0;
	struct AVL_DVBSx_Chip * pAVLChip = &g_stAvlDVBSxChip[HandIndex];
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uint32 uiBER;
	//This function can be called to read back the current BER calculation result after function AVL_DVBSx_IDVBSxRx_ResetErrorStat called.
	r = AVL_DVBSx_IRx_GetDVBSBER(&uiBER, pAVLChip);
	if (AVL_DVBSx_EC_OK != r)
	{
		printf("Get DVBS BER failed.  This function should only be called if the input signal is a DVBS signal.\n");
	}	
    else
	{
	//	printf("BER=%.9f\n",(float)(uiBER*1.0e-9));
		printf("BER=%d*10-9\n",uiBER);
	}
	return uiBER;
}


AVL_uint32 AVL6211_GETPer(void)
{
	AVL_uchar HandIndex = 0;
	struct AVL_DVBSx_Chip * pAVLChip = &g_stAvlDVBSxChip[HandIndex];
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uint32 uiPER;
	//This function can be called to read back the current PER calculation result after function AVL_DVBSx_IDVBSxRx_ResetErrorStat called.
	r = AVL_DVBSx_IRx_GetPER(&uiPER, pAVLChip);
	if (AVL_DVBSx_EC_OK != r)
	{
		printf("Get PER --- Fail !\n");
	}	
    else
	{
	//	printf("PER=%.9f\n",(float)(uiPER*1.0e-9));
		printf("PER=%d*10-9\n",uiPER);
	}

	return uiPER;
}


AVL_uint32 AVL6211_GETSnr(void)
{
	AVL_uchar HandIndex = 0;
	struct AVL_DVBSx_Chip * pAVLChip = &g_stAvlDVBSxChip[HandIndex];
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uint32 uiSNR;
	//This function can be called to read back the current SNR estimate after the channel locked and some waiting time.
	r = AVL_DVBSx_IRx_GetSNR(&uiSNR, pAVLChip);
	if (AVL_DVBSx_EC_OK != r)
	{
		printf("Get SNR --- Fail !\n"); 
	}
    else
	{
	//	printf("SNR=%.2fdb\n",(float)(uiSNR/100.0));
		printf("SNR=%ddb\n",uiSNR/100);
	}
	return uiSNR;
}


AVL_uint32 AVL6211_GETSignalLevel(void)
{
	AVL_uchar HandIndex = 0;
	AVL_uint16 i;
	struct AVL_DVBSx_Chip * pAVLChip = &g_stAvlDVBSxChip[HandIndex];
	AVL_DVBSx_ErrorCode r = AVL_DVBSx_EC_OK;
	AVL_uint16 uiRFSignalLevel;
	AVL_int16  uiRFSignalDBM;
	//This function can be called to get the RF signal level after the channel locked.
	   r = AVL_DVBSx_IRx_GetSignalLevel(&uiRFSignalLevel, pAVLChip);
	   if (AVL_DVBSx_EC_OK != r)
	   {
		   printf("Get SignalLevel --- Fail !\n");
	   }
	   else
	   {
		   for(i=0; i<47; i++)
		   {
			   if(uiRFSignalLevel <= SignalLevel[i].SignalLevel)   
			   {
				   //Calculate the corresponding DBM value.
				   if((0==i)&&(uiRFSignalLevel < SignalLevel[i].SignalLevel))
				   {
					   printf("RFSignalLevel is too weak !");
				   }
				   else
				   {
					   uiRFSignalDBM = SignalLevel[i].SignalDBM;
				   }
				   break;
			   } 
		   }	   
	   //  printf("RFSignalLevel::%.1fdbm\n",(float)(uiRFSignalDBM/10.0));
		   printf("RFSignalLevel::%ddbm\n",uiRFSignalDBM/10);
	   }  

	return uiRFSignalDBM;
}




