//请参考《GX1001P软件包说明-V8.2.doc》
//Please refer to <GX1001 Software Developer Kit User's Manual_V8.2>
/*
Abbreviation
    GX		--	GUOXIN 
    IF		--	intermediate frequency
    RF		--      radiate frequency
    SNR		--	signal to noise ratio
    OSC		--	oscillate
    SPEC	--	spectrum
    FREQ	--	frequency
*/
#ifndef _GX1001_H
#define _GX1001_H

#include <linux/dvb/frontend.h>
#include <linux/i2c.h>
#include "../../../media/dvb/dvb-core/dvb_frontend.h"
#include "../aml_dvb.h"

typedef int GX_STATE;

//#define WRITE	1
//#define READ  	0

#define SUCCESS 1
#define FAILURE	-1	//don't set 0 

#define NEWONE  1
#define OLDONE  -1

#define ENABLE   1
#define DISABLE  0

#define TUNER_RETRY_TIMES 5

/*-- Register Address Defination begin ---------------*/

#define GX_CHIP_ID                 0x00
#define GX_MAN_PARA                0x10
#define GX_INT_PO1_SEL             0x11
#define GX_SYSOK_PO2_SEL           0x12
#define GX_STATE_IND               0x13
#define GX_TST_SEL                 0x14
#define GX_I2C_RST                 0x15
#define GX_MAN_RST                 0x16
#define GX_BIST                    0x18
#define GX_MODE_AGC                0x20
#define GX_AGC_PARA                0x21
#define GX_AGC2_THRES              0x22
#define GX_AGC12_RATIO             0x23
#define GX_AGC_STD                 0x24
#define GX_SCAN_TIME               0x25
#define GX_DCO_CENTER_H            0x26
#define GX_DCO_CENTER_L            0x27
#define GX_BBC_TST_SEL             0x28
#define GX_AGC_ERR_MEAN            0x2B
#define GX_FREQ_OFFSET_H           0x2C
#define GX_FREQ_OFFSET_L           0x2D
#define GX_AGC1_CTRL               0x2E
#define GX_AGC2_CTRL               0x2F
#define GX_FSAMPLE_H               0x40
#define GX_FSAMPLE_M               0x41
#define GX_FSAMPLE_L               0x42
#define GX_SYMB_RATE_H             0x43
#define GX_SYMB_RATE_M             0x44
#define GX_SYMB_RATE_L             0x45
#define GX_TIM_LOOP_CTRL_L         0x46
#define GX_TIM_LOOP_CTRL_H         0x47
#define GX_TIM_LOOP_BW             0x48
#define GX_EQU_CTRL                0x50
#define GX_SUM_ERR_POW_L           0x51
#define GX_SUM_ERR_POW_H           0x52
#define GX_EQU_BYPASS              0x53
#define GX_EQU_TST_SEL             0x54
#define GX_EQU_COEF_L              0x55
#define GX_EQU_COEF_M              0x56
#define GX_EQU_COEF_H              0x57
#define GX_EQU_IND                 0x58
#define GX_RSD_CONFIG              0x80
#define GX_ERR_SUM_1               0x81
#define GX_ERR_SUM_2               0x82
#define GX_ERR_SUM_3               0x83
#define GX_ERR_SUM_4               0x84
#define GX_RSD_DEFAULT             0x85
#define GX_OUT_FORMAT              0x90

/*---GX1001B New Address Defination Start--*/
#define GX_CHIP_IDD					0x01
#define GX_CHIP_VERSION				0x02
#define GX_PLL_M					0x1A	
#define GX_PLL_N					0x1B	 
#define GX_PLL_L					0x1B	
#define GX_MODE_SCAN_ENA			0x29	
#define GX_FM_CANCEL_CTRL			0x30	
#define GX_SF_DECT_FM 				0x30
#define	GX_FM_CANCEL_ON				0x30
#define GX_FM_SUB_ENA				0x30	
#define GX_FM_ENA_FM				0x30
#define GX_FM_UNLOCK_TIME			0x30
#define GX_FM_SUBENA_TIME			0x30	
#define GX_FM_CANCEL_LMT			0x31
#define GX_FM_UNLOCK_LMT			0x31
#define GX_FM_SUBENA_LMT			0x31		
#define GX_SF_CANCEL_CTRL			0x32		
#define GX_SF_DECT_SF				0x32	
#define GX_SF_BUSY					0x32	
#define GX_SF_LOCKED				0x32		
#define GX_FM_ENA_SF				0x32		
#define GX_SF_BW_SEL				0x32		
#define GX_SCAN_TOUT_SET			0x32	
#define GX_SF_LMT_FM				0x33		
#define GX_SF_LMT_H					0x34		
#define GX_SF_LMT_L					0x35		
#define GX_AGC_SET					0x39		
#define	GX_AGC_HOLD					0x39	
#define GX_RF_SET_EN				0x39	
#define GX_IF_SET_EN				0x39	
#define GX_RF_SET_DAT				0x3A		
#define GX_IF_SET_DAT				0x3B		
#define GX_RF_MIN					0x3C		
#define	GX_RF_MAX					0x3D		
#define	GX_IF_MIN					0x3E		
#define GX_IF_MAX					0x3F		
#define GX_SF_FREQ_OUT_H			0xA0		
#define GX_SF_FREQ_OUT_L			0xA1		
#define GX_FM_FREQ_OUT_H			0xA2		
#define GX_FM_FREQ_OUT_L			0xA3      
#define GX_AUTO_THRESH				0xA6		
#define GX_THRESH_AUTO				0xA6	
#define GX_THRESH_STEP				0xA6
#define GX_THRESH_OUT				0xA7		
#define GX_TIM_JIT_BOUND			0x49		
#define GX_TIM_SCAN_SPEED			0x4A		
#define GX_TIM_SCAN_ENA				0x4B		
#define GX_AGC_LMT_2DELTA			0x4C		
#define GX_AGC_LMT_3DELTA			0x4D		
#define GX_DIGITAL_AGC_ON			0x4E		
#define GX_agc_amp_ditter_on		        0x4E
#define GX_SPECTRUM_INV				0x60		
#define GX_PIN_SEL_1				0x91		
#define GX_PIN_SEL_2				0x92
#define GX_PIN_SEL_3				0x93
#define GX_PIN_SEL_4				0x94
#define GX_PIN_SEL_5				0x95
#define GX_PIN_SEL_6				0x96

/*--GX1001B New Address Defination End-----*/

/*-- Register Address Defination end ---------------*/
//#define PERI_BASE_ADDR               0xC1200000
//#define WRITE_PERI_REG(reg, val) *(volatile unsigned *)(PERI_BASE_ADDR + (reg)) = (val)
//#define READ_PERI_REG(reg)  *(volatile unsigned *)(PERI_BASE_ADDR + (reg))
//#define CLEAR_PERI_REG_MASK(reg, mask) WRITE_PERI_REG(reg, (READ_PERI_REG(reg)&(~mask)))
//#define SET_PERI_REG_MASK(reg, mask)   WRITE_PERI_REG(reg, (READ_PERI_REG(reg)|(mask)))

struct gx1001_fe_config {
	int                   i2c_id;
	int                 reset_pin;
	int                 demod_addr;
	int                 tuner_addr;
};


struct gx1001_state {
	struct gx1001_fe_config config;
	struct i2c_adapter *i2c;
	u32                 freq;
        fe_modulation_t     mode;
        u32                 symbol_rate;
        struct dvb_frontend fe;
};


/* -------------------------- User-defined GX1001 software interface begin  ---------------------*/
uint  GetTunerStatus(struct gx1001_state *state) ;

void GX_Delay_N_ms(unsigned int ms_value);

GX_STATE GX_I2cReadWrite(struct gx1001_state *state, int WR_flag, unsigned char ChipAddress,unsigned char RegAddress,unsigned char *data, int data_number);

/*-------------------------- User-defined GX1001 software interface end -------------------------*/

GX_STATE GX_Set_RFFrequency(struct gx1001_state *state, unsigned long fvalue);

GX_STATE GX_Set_AGC_Parameter(struct gx1001_state *state);


/*-------------------------- Control set begin ------------------------*/

GX_STATE GX_Write_one_Byte(struct gx1001_state *state, int RegAddress,int WriteValue);

GX_STATE GX_Write_one_Byte_NoReadTest(struct gx1001_state *state, int RegAddress,int WriteValue);

int GX_Read_one_Byte(struct gx1001_state *state, int RegAddress);

GX_STATE GX_Init_Chip(struct gx1001_state *state);

GX_STATE GX_HotReset_CHIP(struct gx1001_state *state);

GX_STATE GX_CoolReset_CHIP(struct gx1001_state *state);

GX_STATE GX_Set_OutputMode(struct gx1001_state *state, int mode);

GX_STATE GX_Select_DVB_QAM_Size(struct gx1001_state *state, int size);

GX_STATE GX_Read_ALL_OK(struct gx1001_state *state);

GX_STATE GX_Read_EQU_OK(struct gx1001_state *state);

GX_STATE GX_Set_Tunner_Repeater_Enable(struct gx1001_state *state, int OnOff);

GX_STATE GX_Get_Version(struct gx1001_state *state);

GX_STATE GX_Set_Mode_Scan(struct gx1001_state *state, int mode);

GX_STATE GX_Set_FM(struct gx1001_state *state, int mode);

GX_STATE GX_Set_SF(struct gx1001_state *state, int mode);

GX_STATE GX_Set_RF_Min(struct gx1001_state *state, int value);

GX_STATE GX_Set_RF_Max(struct gx1001_state *state, int value);

GX_STATE GX_set_IF_Min(struct gx1001_state *state, int value);

GX_STATE GX_set_IF_Max(struct gx1001_state *state, int value);

GX_STATE GX_SET_AUTO_IF_THRES(struct gx1001_state *state, int mode);

GX_STATE GX_Set_Tim_Scan(struct gx1001_state *state, int mode);

GX_STATE GX_Set_Digital_AGC(struct gx1001_state *state, int mode);

GX_STATE GX_Set_Sleep(struct gx1001_state *state, int Sleep_Enable);

GX_STATE GX_Set_Pll_Value(struct gx1001_state *state, int Pll_M_Value,int Pll_N_Value,int Pll_L_Value);

/*-------------------------- Control set end ---------------------------*/


/*--------------- Symbol recovery begin --------------------------------*/

GX_STATE GX_SetSymbolRate(struct gx1001_state *state, unsigned long Symbol_Rate_Value);
GX_STATE GX_SetOSCFreq(struct gx1001_state *state);

/*--------------- Symbol recovery end ----------------------------------*/


//===========================================================================================================================================

unsigned char GX_Change2percent(int value,int low,int high);
int GX_100Log(int);

unsigned char GX_Get_Signal_Strength(struct gx1001_state *state);
int GX_Get_ErrorRate(struct gx1001_state *state);
int GX_Get_SNR(struct gx1001_state *state);

GX_STATE GX_SetSpecInvert(struct gx1001_state *state, int Spec_invert);
GX_STATE GX_Search_Signal(struct gx1001_state *state, unsigned long Symbol_1,unsigned long Symbol_2,int Spec_Mode,int Qam_Size,unsigned long RF_Freq,int Wait_OK_X_ms);


//===============================================================================================================
//================== for MT2060 =================================================================================

typedef unsigned char   U8Data;         /*  type corresponds to 8 bits      */
typedef unsigned int    UData_t;        /*  type must be at least 32 bits   */
typedef int             SData_t;        /*  type must be at least 32 bits   */
typedef void *          Handle_t;       /*  memory pointer type             */
typedef double          FData_t;        /*  floating point data type        */

#define MAX_UDATA         (4294967295)  /*  max value storable in UData_t   */


int Initialize_MT2060(void);
UData_t MT2060_LocateIF1(UData_t f_in, 
                         UData_t f_out, 
                         UData_t* f_IF1
                         );
int Wirite_MT2060_regs(unsigned long f_in);

//============================================================================================================================================

typedef u8  INT8U;
typedef s8  INT8S;
typedef s32 INT32S;
typedef u32 INT32U;
typedef int AM_ErrorCode_t;

AM_ErrorCode_t demod_init(struct gx1001_state *state);
AM_ErrorCode_t demod_deinit(struct gx1001_state *state);
AM_ErrorCode_t demod_check_locked(struct gx1001_state *state, unsigned char* lock);
AM_ErrorCode_t demod_connect(struct gx1001_state *state, unsigned int freq_khz, unsigned char qam_size, unsigned int sr_ks);
AM_ErrorCode_t demod_disconnect(struct gx1001_state *state);
AM_ErrorCode_t demod_get_signal_strength(struct gx1001_state *state, unsigned int* strength);
AM_ErrorCode_t demod_get_signal_quality(struct gx1001_state *state, unsigned int* quality);
uint demod_get_signal_errorate(struct gx1001_state *state, unsigned int* errorrate);

extern struct dvb_frontend* gx1001_attach(const struct gx1001_fe_config* config);

#endif

