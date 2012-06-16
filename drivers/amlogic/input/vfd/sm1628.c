
#include <linux/input/vfd.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include "sm1628.h"

#define VFD_DEBUG
#ifdef VFD_DEBUG
#define DBG_VFD(msg)                    msg
#else
#define DBG_VFD(msg)                    
#endif

void MDrv_FrontPnl_Init(void);
void MDrv_FrontPnl_Update(char *U8str,int colon);
int MDrv_TM1623_FP_Get_Key(void);

int (*vfd_set_stb_pin_value)(int value);
int (*vfd_set_clock_pin_value)(int value);
int (*vfd_set_do_pin_value)(int value);
int (*vfd_get_di_pin_value)(void);


static int sm1628_init(struct vfd_platform_data *pvfd_platform_data)
{
		vfd_set_stb_pin_value = pvfd_platform_data->set_stb_pin_value;				
		vfd_set_clock_pin_value = pvfd_platform_data->set_clock_pin_value;		
		vfd_set_do_pin_value = pvfd_platform_data->set_do_pin_value;
		vfd_get_di_pin_value = pvfd_platform_data->get_di_pin_value;
		
		MDrv_FrontPnl_Init();
		
		return 0;	
}


static int get_sm1628_key_value(void)
{
	
		return MDrv_TM1623_FP_Get_Key();
}

	
static int set_sm1628_led_value(char *display_code)
{
		int i,j = 0;
		char data,display_char[8];
		int dot = 0;
		for(i = 0; i <= 8; i++)
		{
			data = display_code[i];
			if(data == ':')	{
				//display_char[i] = display_code[i+1];
			  dot++;
			}
			else{				
				display_char[j++] = display_code[i];
				if(data == '\0')break;
			}
		}
		DBG_VFD(printk("function[%s] line %d display string: %s .\n", __FUNCTION__,__LINE__,display_char));
/*
		printk("function: %s line %d .\n", __FUNCTION__,__LINE__);
		for(i=0;i<j;i++){
		 printk("set led display char[%d] is: %c \n", i,display_char[i]);
		 if(display_char[i] == '\0')	{
		 		printk("end char is: char[%d] \n", i);
		 		break;
		 	}
		}
*/		
		MDrv_FrontPnl_Update(display_char, dot);
		return 0;
}

#ifdef CONFIG_VFD_SM1628
int hardware_init(struct vfd_platform_data *pdev)
{
		int ret;					
		ret = sm1628_init(pdev);					
		return ret;	
}

int get_vfd_key_value(void)
{
		int key_value;
		key_value = get_sm1628_key_value();
		return key_value;
}

int set_vfd_led_value(char *display_code)
{
		int ret;		
		ret = set_sm1628_led_value(display_code);		
		return ret;
}
#endif

//==============================================================================
//----------------------------KEY Function--------------------------------------
//==============================================================================
//-------------------------------------------------------------------------------------------------
/// TM16231 Write Data
/// @return None
//-------------------------------------------------------------------------------------------------
void MDrv_TM1623_WriteData(U8 Value)
{
	U8 i, j;

	for (i = 0; i < 8; i++) {
		if (Value & 0x01) {
			PT6964_DIN_H;
		}
		else {
			PT6964_DIN_L;
		}

		PT6964_CLK_L;
		udelay(500);
		PT6964_CLK_H;
		Value >>= 1;
		j++;
	}
}

//-------------------------------------------------------------------------------------------------
/// TM16231 Write Data
/// @return None
//-------------------------------------------------------------------------------------------------
void MDrv_TM1623_Write_Adr_Data(U8 addr, U8 value)
{
	PT6964_STB_L;
	MDrv_TM1623_WriteData(0x44);
	PT6964_STB_H;

	PT6964_STB_L;
	MDrv_TM1623_WriteData(addr|0xc0);
	MDrv_TM1623_WriteData(value);
	PT6964_STB_H;
}

//-------------------------------------------------------------------------------------------------
/// TM16231 Clear Ram ( if you want to display new Char , you need Clear All)
/// @return None
//-------------------------------------------------------------------------------------------------
void MDrv_TM1623_Clear6964RAM(void)
{
	U8  i;

	PT6964_STB_L;
	MDrv_TM1623_WriteData(0x40);			//Command 1, increment address
	PT6964_STB_H;

	PT6964_STB_L;
	MDrv_TM1623_WriteData(0xc0);			//Command 2, RAM address = 0
	for(i=0;i<=13;i++)			                     //22 bytes
	{
		MDrv_TM1623_WriteData(0x00);
	}
	PT6964_STB_H;
}

//-------------------------------------------------------------------------------------------------
/// Initialize TM16231
/// @return TRUE  - Success
///         FALSE - Failure
//-------------------------------------------------------------------------------------------------
void MDrv_TM1623_Init(void)
{
	U16 i , j ;

	PT6964_STB_H;					//Initial state
	PT6964_CLK_H;					//Intial state
	PT6964_DIN_H;

	for (i = 0; i < 1000; i++)
		for (j = 0; j < 1000; j++)
			{;}
	//mdelay(10);
	MDrv_TM1623_Clear6964RAM();

	PT6964_STB_L;
	MDrv_TM1623_WriteData(FP_LED_MODE);
	PT6964_STB_H;

	PT6964_STB_L;
	MDrv_TM1623_WriteData(0x8F);
	PT6964_STB_H;
}

//-------------------------------------------------------------------------------------------------
/// TM1623 Get Key
/// @return None
//-------------------------------------------------------------------------------------------------
int MDrv_TM1623_FP_Get_Key(void)
{
	int i, value = 0, j;

	PT6964_STB_L;
	MDrv_TM1623_WriteData(0x42);
	PT6964_DOUT_GET;
	for (i = 0; i < 40; i++) {
		PT6964_CLK_L;
		j++;
		value++;
		j++;
		if ((PT6964_DOUT_GET == 1 )) {
			PT6964_CLK_H;
			PT6964_STB_H;
			return (value);
		}
		PT6964_CLK_H;
	}

	PT6964_STB_H;
	value = 0;

	return (value);
}

//-------------------------------------------------------------------------------------------------
/// TM1623 display on
/// @return TRUE  - Success
///         FALSE - Failure
//-------------------------------------------------------------------------------------------------
void MDrv_TM1623_Display_On(void)
{
	PT6964_STB_L;
	MDrv_TM1623_WriteData(0x8F);
	PT6964_STB_H;
}

// Keypad uAPI
int  uKeypad_WakeUp(void)
{
	bool bGetKey = FALSE;

	//if( !_GPIO_PM2_Read() )
	if (MDrv_TM1623_FP_Get_Key()) {
		bGetKey = TRUE;
	}
	return bGetKey;
}

//==============================================================================
//----------------------------LED Function--------------------------------------
//==============================================================================
//-------------------------------------------------------------------------------------------------
/// Show boot
/// @return None
//-------------------------------------------------------------------------------------------------
void MDrv_TM1623_ShowBoot(void)
{
	MDrv_TM1623_Write_Adr_Data(0x00, 0x10);
	//   MDrv_TM1623_Write_Adr_Data(0x01, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x02, 0x10);
	//    MDrv_TM1623_Write_Adr_Data(0x03, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x04, 0x10);
	//   MDrv_TM1623_Write_Adr_Data(0x05, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x06, 0x10);
	//   MDrv_TM1623_Write_Adr_Data(0x07, 0x00);
}

//-------------------------------------------------------------------------------------------------
/// StandBy TM1623
/// @return None
//-------------------------------------------------------------------------------------------------
void MDrv_TM1623_StandBy(void)
{
	MDrv_TM1623_Write_Adr_Data(0x00, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x01, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x02, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x03, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x04, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x05, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x06, 0x00);
	MDrv_TM1623_Write_Adr_Data(0x07, 0x00);
}

//----------------------------------------------------
// Function Name: Mdrv_6961_LED_DISP
// Description:
//		Update PT6961/PT6964/ET6201 LED display data RAM.
// Parameters:
//		U8_LedBit, LED number
//		U8_LowByte, LowByte data in RAM     LSB~MSB : SEG1~ SEG8
//		U8_HighByte, High Byte data in RAM	LSB~MSB : SEG9 ~ SEG12 + xxxx
// 			LED bit sequence Can be address by the following Macro:
//		LED_WORD1_ADDR /  LED_WORD2_ADDR /LED_WORD3_ADDR / LED_WORD4_ADDR
//------------------------------------------------------------------------
void MDrv_TM1623_LED_DISP(char U8_Ledbit, char U8_LowByte, char U8_HighByte)
{
#if 1
	switch (U8_Ledbit) {
	case 0:
		MDrv_TM1623_Write_Adr_Data(LED_WORD1_ADDR, U8_LowByte);
		MDrv_TM1623_Write_Adr_Data(LED_WORD1_ADDR + 1, U8_HighByte);
		break;
	case 1:
		MDrv_TM1623_Write_Adr_Data(LED_WORD2_ADDR, U8_LowByte);
		MDrv_TM1623_Write_Adr_Data(LED_WORD2_ADDR + 1, U8_HighByte);
		break;
	case 2:
		MDrv_TM1623_Write_Adr_Data(LED_WORD3_ADDR, U8_LowByte);
		MDrv_TM1623_Write_Adr_Data(LED_WORD3_ADDR + 1, U8_HighByte);
		break;
	case 3:
		MDrv_TM1623_Write_Adr_Data(LED_WORD4_ADDR, U8_LowByte);
		MDrv_TM1623_Write_Adr_Data(LED_WORD4_ADDR + 1, U8_HighByte);
		break;
	case 4:
		MDrv_TM1623_Write_Adr_Data(LED_WORD5_ADDR, U8_LowByte);
		MDrv_TM1623_Write_Adr_Data(LED_WORD5_ADDR + 1, U8_HighByte);
		break;
	case 5:
		MDrv_TM1623_Write_Adr_Data(LED_WORD6_ADDR, U8_LowByte);
		MDrv_TM1623_Write_Adr_Data(LED_WORD6_ADDR + 1, U8_HighByte);
		break;
	case 6:
		MDrv_TM1623_Write_Adr_Data(LED_COLON_ADDR, U8_LowByte);
		MDrv_TM1623_Write_Adr_Data(LED_COLON_ADDR + 1, U8_HighByte);
		break;						
	default:
		break;
	}
	#else  //test vfd

	MDrv_TM1623_Write_Adr_Data(0x00, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x02, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x04, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x06, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x08, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x0A, 0x08);

	MDrv_TM1623_Write_Adr_Data(0x01, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x03, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x05, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x07, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x09, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x0B, 0x08);
	MDrv_TM1623_Write_Adr_Data(0x0D, 0x08);

	MDrv_TM1623_Write_Adr_Data(0x0C, 0x08);

#endif
}

//-------------------------------------------------------------
// Function: MDrv_FP_LED_CharSet
// Description :
//			Set dedicated LED data, including dot
// parameters:
//			ledBit, led bit select
//			U8LedChar: char data

//-------------------------------------------------------------------
bool MDrv_FP_LED_CharSet(char ledBit, char U8LEDChar)
{
	char i;
	char low,high;

	DBG_VFD(printk("[%s]:line %d now display char : %c \n", __FUNCTION__,__LINE__,U8LEDChar));
	for (i = 0; i < sizeof(_char2SegmentTable) / sizeof (Char2Segment); i++)
	{
		if (U8LEDChar == _char2SegmentTable[i].u8Char) {

			low = (char)_char2SegmentTable[i].u8SegmentLowByte;
			high = (char)_char2SegmentTable[i].u8SegmentHighByte;
			for(i = 0; i < 8; i++) {
				if((low >> i)&0x01)
					dig_value[i] |= 1 << bit2seg[ledBit];
			}

			//MDrv_TM1623_LED_DISP(ledBit, low, high);
			return 0;
		}
	}
	return 1;
}

void MDrv_FrontPnl_Update(char *U8str, int colon)
{
	char i = 0;
	char U8Data;
	
	for(i = 0; i < LED_BYTE_NUM; i++)	{
		dig_value[i] = 0;
	}
		
	for (i = 0; i < LED_NUM; i++) {
		U8Data = U8str[i];
		if (U8Data == '\0')	break;
		if (MDrv_FP_LED_CharSet(i, U8Data))
			DBG_VFD(printk("FP LED char not defined.\n"));
	}

	if(colon){
		if(colon&0x01)	dig_value[FP_DOT1_DIG] |=  1 << (FP_DOT_SEG_NUM-1);
		if(colon&0x02) 	{ 
			dig_value[FP_DOT1_DIG] |=  1 << (FP_DOT_SEG_NUM-1);
			dig_value[FP_DOT2_DIG] |=  1 << (FP_DOT_SEG_NUM-1);
		}
	}
				
	for(i = 0; i < LED_BYTE_NUM; i++)	{
		MDrv_TM1623_LED_DISP(i, dig_value[i], 0);
	}

	MDrv_TM1623_Display_On();

}

// Frontpanel API
void MDrv_FrontPnl_Init(void)
{
	MDrv_TM1623_Init();
	MDrv_TM1623_ShowBoot();
	MDrv_FrontPnl_Update((char *)"----", 0);
}


//------------- END OF FILE ------------------------
