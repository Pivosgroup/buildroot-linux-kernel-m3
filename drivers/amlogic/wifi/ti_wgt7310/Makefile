# wgt7310
EXTRA_CFLAGS += -DLINUX -DHOST_COMPILE -DFW_RUNNING_AS_STA -DSTATIC_LIB -DDEBUG -DTI_DBG -DREPORT_LOG  -DO2 \
			-D__BYTE_ORDER_LITTLE_ENDIAN  -DTNETW1273 -DSUPPL_WPS_SUPPORT -D__LINUX__\
			-Idrivers/amlogic/wifi/ti_wgt7310/inc/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/AirLink_Managment/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/Application/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/Connection_Managment/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/Ctrl_Interface/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/Data_link/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/Sta_Management/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/Test/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/Ctrl/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/Ctrl/Export_Inc/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/Data_Service/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/Data_Service/Export_Inc/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/FirmwareApi/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/FW_Transfer/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/FW_Transfer/Export_Inc/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/MacServices/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/MacServices/Export_Inc/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/TWDriver/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/TWD/TwIf/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/Txn/ \
			-Idrivers/amlogic/wifi/ti_wgt7310/utils/

SRC = SdioDrv.c WlanDrvIf.c osapi.c osmemapi.c osRgstry_parser.c ipc_k.c \
	WlanDrvWext.c CmdInterpretWext.c RxBuf.c host_platform.c \
	SdioAdapter.c osRgstry.c

SRC += AirLink_Managment/measurementMgr.c AirLink_Managment/measurementMgrSM.c AirLink_Managment/regulatoryDomain.c \
			AirLink_Managment/requestHandler.c AirLink_Managment/SoftGemini.c AirLink_Managment/spectrumMngmntMgr.c AirLink_Managment/SwitchChannel.c

SRC += Application/roamingMngr.c Application/roamingMngr_autoSM.c  Application/roamingMngr_manualSM.c Application/scanMngr.c

SRC += Connection_Managment/mainSecKeysOnly.c Connection_Managment/mainSecNull.c Connection_Managment/mainSecSm.c \
		Connection_Managment/rsn.c Connection_Managment/sme.c Connection_Managment/smeSelect.c Connection_Managment/smeSm.c \
		 Connection_Managment/unicastKey802_1x.c Connection_Managment/unicastKeyNone.c Connection_Managment/unicastKeySM.c \
		 Connection_Managment/admCtrl.c Connection_Managment/admCtrlNone.c Connection_Managment/admCtrlWep.c \
		 Connection_Managment/admCtrlWpa2.c Connection_Managment/admCtrlWpa.c Connection_Managment/apConn.c \
		 Connection_Managment/broadcastKey802_1x.c Connection_Managment/broadcastKeyNone.c Connection_Managment/broadcastKeySM.c \
		 Connection_Managment/conn.c Connection_Managment/connIbss.c Connection_Managment/connInfra.c \
		 Connection_Managment/externalSec.c Connection_Managment/keyDerive.c Connection_Managment/keyDeriveAes.c \
		 Connection_Managment/keyDeriveCkip.c Connection_Managment/keyDeriveTkip.c Connection_Managment/keyDeriveWep.c \
		 Connection_Managment/keyParser.c Connection_Managment/keyParserExternal.c Connection_Managment/keyParserWep.c \
		 Connection_Managment/mainKeysSm.c

SRC += Ctrl_Interface/CmdDispatcher.c Ctrl_Interface/CmdHndlr.c Ctrl_Interface/DrvMain.c Ctrl_Interface/EvHandler.c

SRC += Data_link/Ctrl.c Data_link/GeneralUtil.c  Data_link/rx.c Data_link/TrafficMonitor.c Data_link/txCtrl.c \
		 Data_link/txCtrlParams.c Data_link/txCtrlServ.c Data_link/TxDataClsfr.c Data_link/txDataQueue.c \
		 Data_link/txMgmtQueue.c Data_link/txPort.c

SRC += Sta_Management/openAuthSm.c Sta_Management/PowerMgr.c  Sta_Management/PowerMgrDbgPrint.c Sta_Management/PowerMgrKeepAlive.c \
		 Sta_Management/qosMngr.c Sta_Management/roamingInt.c Sta_Management/ScanCncn.c Sta_Management/ScanCncnApp.c \
		 Sta_Management/ScanCncnOsSm.c Sta_Management/ScanCncnSm.c Sta_Management/ScanCncnSmSpecific.c Sta_Management/scanResultTable.c \
		 Sta_Management/scr.c Sta_Management/sharedKeyAuthSm.c Sta_Management/siteHash.c Sta_Management/siteMgr.c \
		 Sta_Management/StaCap.c Sta_Management/systemConfig.c Sta_Management/templates.c Sta_Management/trafficAdmControl.c \
		 Sta_Management/assocSM.c Sta_Management/authSm.c Sta_Management/currBss.c Sta_Management/healthMonitor.c \
		 Sta_Management/mlmeBuilder.c Sta_Management/mlmeParser.c Sta_Management/mlmeSm.c

SRC += Test/connDebug.c Test/ctrlDbg.c  Test/debug.c Test/fwdriverdebug.c Test/HealthMonitorDbg.c Test/measurementDbg.c \
			 Test/MibDbg.c Test/PowerMgrDebug.c Test/qosMngrDbg.c Test/roamingMgrDebug.c Test/rsnDbg.c Test/RxDbg.c \
			 Test/scanCncnDbg.c Test/ScanMngrDbg.c Test/scrDbg.c Test/siteMgrDebug.c Test/smeDebug.c \
			 Test/SoftGeminiDbg.c Test/TWD_Debug.c Test/TwIfDebug.c Test/TxDbg.c

SRC += TWD/Ctrl/CmdBld.c TWD/Ctrl/CmdBldCfg.c TWD/Ctrl/CmdBldCfgIE.c TWD/Ctrl/CmdBldCmd.c TWD/Ctrl/CmdBldCmdIE.c \
		 	TWD/Ctrl/CmdBldItr.c TWD/Ctrl/CmdBldItrIE.c TWD/Ctrl/CmdQueue.c \
		 	TWD/Data_Service/RxQueue.c TWD/Data_Service/txCtrlBlk.c TWD/Data_Service/txHwQueue.c \
		 	TWD/FW_Transfer/CmdMBox.c TWD/FW_Transfer/eventMbox.c TWD/FW_Transfer/fwDebug.c \
		 	TWD/FW_Transfer/FwEvent.c TWD/FW_Transfer/HwInit.c TWD/FW_Transfer/RxXfer.c \
		 	TWD/FW_Transfer/txResult.c TWD/FW_Transfer/txXfer.c \
		 	TWD/MacServices/MacServices.c TWD/MacServices/MeasurementSrv.c TWD/MacServices/measurementSrvDbgPrint.c \
		 	TWD/MacServices/MeasurementSrvSM.c TWD/MacServices/PowerSrv.c TWD/MacServices/PowerSrvSM.c \
		 	TWD/MacServices/ScanSrv.c TWD/MacServices/ScanSrvSM.c \
		 	TWD/TWDriver/TWDriver.c TWD/TWDriver/TWDriverCtrl.c TWD/TWDriver/TWDriverRadio.c TWD/TWDriver/TWDriverTx.c \
		 	TWD/TwIf/TwIf.c \

SRC += Txn/SdioBusDrv.c Txn/TxnQueue.c

SRC += utils/context.c utils/freq.c utils/fsm.c utils/GenSM.c utils/queue.c \
			utils/rate.c utils/report.c utils/timer.c

MODULE:=tiwlan_drv.ko

obj-$(CONFIG_SDIO_TI_WIFI_WGT7310) := $(MODULE:.ko=.o)
$(MODULE:.ko=-y) := $(SRC:.c=.o)

EXTRA_LDFLAGS += --strip-debug
