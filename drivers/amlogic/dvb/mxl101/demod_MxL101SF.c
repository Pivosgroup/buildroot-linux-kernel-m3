////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2008 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// (¡§MStar Confidential Information¡¨) by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
/// @file MSB122x.c
/// @brief MSB122x DVBT, VIF Controller Interface
/// @author MStar Semiconductor, Inc.
//
////////////////////////////////////////////////////////////////////////////////
#include "demod_MxL101SF.h"
#include "MxL101SF_OEM_Drv.h"
#include "MxL101SF_PhyCtrlApi.h"
#include "MxL101SF_PhyDefs.h"

//#define MS_DEBUG
#ifdef MS_DEBUG
#define MXL_MSB(x)          x
#else
#define MXL_MSB(x)          //x
#endif

void MxL101SF_Init(void)
{
  //UINT32 loop;
  MXL_DEV_INFO_T mxlDevInfo;
  MXL_XTAL_CFG_T mxlXtalCfg;
  MXL_DEV_MODE_CFG_T mxlDevMode;
  MXL_MPEG_CFG_T mxlMpegOutCfg;
  MXL_TOP_MASTER_CFG_T mxlTopMasterCfg;
  //MXL_RF_TUNE_CFG_T mxlChanCfg;
  //MXL_DEMOD_LOCK_STATUS_T rsLockStatus;
  //MXL_DEMOD_SNR_INFO_T snr;
  //MXL_DEMOD_BER_INFO_T ber;
 // MXL_SIGNAL_STATS_T sigStrength;
  //MXL_DEMOD_CELL_ID_INFO_T tpsCellIdInfo;
  
  // Open LPT driver for I2C communcation
  //Ctrl_I2cConnect(99);

  // 1. Do SW Reset
  MxLWare_API_ConfigDevice(MXL_DEV_SOFT_RESET_CFG, NULL);
  
  // 2. Read Back chip id and version
  //    Expecting CHIP ID = 0x61, Version = 0x6

  MxLWare_API_GetDeviceStatus(MXL_DEV_ID_VERSION_REQ, &mxlDevInfo);
  printf("MxL101SF : ChipId = 0x%x, Version = 0x%x\n", mxlDevInfo.DevId, mxlDevInfo.DevVer);

  // 3. Init Tuner and Demod
  MxLWare_API_ConfigDevice(MXL_DEV_101SF_OVERWRITE_DEFAULTS_CFG, NULL);

  // Step 4
  // Enable Loop Through if needed
  // Enable Clock Out and configure Clock Out gain if needed
  // Configure MxL101SF XTAL frequency
  // Configure XTAL Bias value if needed

  // Xtal Capacitance value must be configured in accordance 
  // with XTAL datasheet’s requirement.
  mxlXtalCfg.XtalFreq = XTAL_24MHz;
  mxlXtalCfg.LoopThruEnable = MXL_ENABLE;//MXL_DISABLE;
  mxlXtalCfg.XtalBiasCurrent = XTAL_BIAS_NA;
  mxlXtalCfg.XtalCap = 0x12; //0x12
  mxlXtalCfg.XtalClkOutEnable = MXL_DISABLE;
  mxlXtalCfg.XtalClkOutGain = CLK_OUT_NA;
  MxLWare_API_ConfigDevice(MXL_DEV_XTAL_SETTINGS_CFG, &mxlXtalCfg);
   
  // 5. Set Baseband mode, SOC or Tuner only mode
  mxlDevMode.DeviceMode = MXL_SOC_MODE;
  MxLWare_API_ConfigDevice(MXL_DEV_OPERATIONAL_MODE_CFG, &mxlDevMode);
  

  // 6. Configure MPEG Out 
  // CLK, MPEG_CLK_INV, Polarity of MPEG Valid/MPEG Sync
  mxlMpegOutCfg.MpegClkFreq = MPEG_CLOCK_36_571429MHz;//MPEG_CLOCK_9_142857MHz;//MPEG_CLOCK_36_571429MHz;
  mxlMpegOutCfg.LsbOrMsbFirst = MPEG_SERIAL_MSB_1ST;
  mxlMpegOutCfg.MpegClkPhase = MPEG_CLK_IN_PHASE;//MPEG_CLK_IN_PHASE;
  mxlMpegOutCfg.MpegSyncPol = MPEG_CLK_IN_PHASE;
  mxlMpegOutCfg.MpegValidPol = MPEG_CLK_IN_PHASE;

#ifdef CONFIG_AMLOGIC_S_TS2
  mxlMpegOutCfg.SerialOrPar = MPEG_DATA_SERIAL;
#else
  mxlMpegOutCfg.SerialOrPar = MPEG_DATA_PARALLEL;
#endif

  MxLWare_API_ConfigDevice(MXL_DEV_MPEG_OUT_CFG, &mxlMpegOutCfg);

  // 7. Enable Top Master Control
  mxlTopMasterCfg.TopMasterEnable = MXL_ENABLE;
  MxLWare_API_ConfigTuner(MXL_TUNER_TOP_MASTER_CFG, &mxlTopMasterCfg);	


  

}

void MxL101SF_Tune(UINT32 u32TunerFreq, UINT8 u8BandWidth)
{
	 MXL_RF_TUNE_CFG_T mxlChanCfg;
	 // 8. Tune RF with channel frequency and bandwidth
	/* if(u32TunerFreq<177500||u32TunerFreq>868000)
	 {
	 	printf("input u32TunerFreq=%d error\n",u32TunerFreq);
		return;
	 }
	 if(u8BandWidth<6||u8BandWidth>8)
	 {
	 	printf("input u8BandWidth=%d error\n",u8BandWidth);
	    return;
	 }*/

 	 // 8. Tune RF with channel frequency and bandwidth
 	 printf("u32TunerFreq is %d,u8BandWidth is %d\n",u32TunerFreq,u8BandWidth);
 	 mxlChanCfg.Bandwidth = u8BandWidth;
 	 mxlChanCfg.Frequency = u32TunerFreq;//u32TunerFreq*1000;
 	 mxlChanCfg.TpsCellIdRbCtrl = MXL_ENABLE;  // Enable TPS Cell ID feature
 	 MxLWare_API_ConfigTuner(MXL_TUNER_CHAN_TUNE_CFG, &mxlChanCfg);	 
}

UINT32 MxL101SF_GetSNR(void)
{
	MXL_DEMOD_SNR_INFO_T snr;
	// Read back SNR
    MxLWare_API_GetDemodStatus(MXL_DEMOD_SNR_REQ, &snr);
  //  MXL_MSB(printf("MxL101SF : Demod SNR = %d dB\n", snr.SNR));
	return snr.SNR/10000;
}

MXL_BOOL MxL101SF_GetLock(void)
{
	MXL_DEMOD_LOCK_STATUS_T rsLockStatus;
	// Read back RS Lock Status
    MxLWare_API_GetDemodStatus(MXL_DEMOD_RS_LOCK_REQ, &rsLockStatus);
    MXL_MSB(printf("MxL101SF : RS Lock Status = 0x%x\n", rsLockStatus.Status));
	return rsLockStatus.Status;
}

UINT32 MxL101SF_GetBER(void)
{
	MXL_DEMOD_BER_INFO_T ber;
	// Read back BER
    MxLWare_API_GetDemodStatus(MXL_DEMOD_BER_REQ, &ber);
  //  MXL_MSB(printf("MxL101SF : Demod BER = %d\n", ber.BER));
	return ber.BER;
}

SINT32 MxL101SF_GetSigStrength(void)
{
	MXL_SIGNAL_STATS_T sigStrength;
	// Read back Signal Strength
    MxLWare_API_GetTunerStatus(MXL_TUNER_SIGNAL_STRENGTH_REQ, &sigStrength);
 //   MXL_MSB(printf("MxL101SF : Signal Strength = %d dBm\n", sigStrength.SignalStrength));
	return sigStrength.SignalStrength;
}

UINT16 MxL101SF_GetTPSCellID(void)
{
	MXL_DEMOD_CELL_ID_INFO_T tpsCellIdInfo;
    // Read back TPS Cell ID
    MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_CELL_ID_REQ, &tpsCellIdInfo);
    MXL_MSB(printf("MxL101SF : TPS Cell ID = %04X\n", tpsCellIdInfo.TpsCellId));	
	return tpsCellIdInfo.TpsCellId;
}

void MxL101SF_PowerOnOff(UINT8 u8PowerOn)
{
	MXL_CMD_TYPE_E CmdType = MXL_DEV_101SF_POWER_MODE_CFG;
	MXL_PWR_MODE_CFG_T PwrMode;
	if(u8PowerOn==0)
	{
		PwrMode.PowerMode = STANDBY_OFF;
		if(MxLWare_API_ConfigDevice(CmdType,&PwrMode)!=MXL_TRUE)
		{
			printf("MxL101SF_PowerOff failed\n");
		}
		else
		{
			printf("MxL101SF_PowerOff ok\n");
		}
	}
}

void Mxl101SF_Debug()
{
	UINT32  tpsCellIdInfo;
	MxLWare_API_GetDemodStatus(MXL_DEMOD_SNR_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_BER_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_CODE_RATE_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_HIERARCHY_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_CONSTELLATION_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_FFT_MODE_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_HIERARCHICAL_ALPHA_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_GUARD_INTERVAL_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_LOCK_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_TPS_CELL_ID_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_PACKET_ERROR_COUNT_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_SYNC_LOCK_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_RS_LOCK_REQ, &tpsCellIdInfo);
	MxLWare_API_GetDemodStatus(MXL_DEMOD_CP_LOCK_REQ, &tpsCellIdInfo);
	

}

