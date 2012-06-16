#include "hdmi_cat_defstx.h"
#include "hdmi_i2c.h"
 
//------------------------------------------------------------
//1: disable this interrupt; 0: Enable this interrupt
void CAT_Set_Interrupt_Masks(unsigned char *data) 
{
	WriteBlockHDMITX_CAT(REG_INT_MASK1, 3, data);
} 

//------------------------------------------------------------
//1: disable this interrupt; 0: Enable this interrupt
void CAT_Disable_Interrupt_Masks(unsigned char *data) 
{
	unsigned char temp[3];
	ReadBlockHDMITX_CAT(REG_INT_MASK1, 3, temp);
	temp[0] |= data[0];
	temp[1] |= data[1];
	temp[2] |= data[2];
	WriteBlockHDMITX_CAT(REG_INT_MASK1, 3, temp);
} 

//------------------------------------------------------------
//1: disable this interrupt; 0: Enable this interrupt
void CAT_Enable_Interrupt_Masks(unsigned char *data) 
{
	unsigned char temp[3];
	ReadBlockHDMITX_CAT(REG_INT_MASK1, 3, temp);
	temp[0] &= ~data[0];
	temp[1] &= ~data[1];
	temp[2] &= ~data[2];
	WriteBlockHDMITX_CAT(REG_INT_MASK1, 3, temp);
}
