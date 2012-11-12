#include "MsCommon.h"
#include "drvGPIO.h"
#include "Type.h"
#include "Board.h"
#include "User.h"
#include "error.h"

#define StartStopDelay 3
#define DataClkDelay 2
#define ClkHighDelay 3
#define ClkLowDelay 5

Byte I2c_address = 0x38;

void Us_delay (
    IN  Demodulator*    demodulator,
    IN  Dword           dwUs
) {
      MS_U32 index = 0;
      MS_U32 count = 0;
      for(count = 0; count < dwUs;count++)
      {
         for(index = 0; index < 120;index++)
         {
         }
      }

}
extern void IntSetClockHigh (
    IN  Demodulator*    demodulator
);

int GPIO_InputSDA (
    IN  Demodulator*    demodulator
) {
    MS_U32 ret = 0;
    //MS_U32 index = 0;
    //for(index = 0; index <100; index++)
    {
      mdrv_gpio_init();    
      SWI2C_SET_SDA_INPUT(); 
      Us_delay (demodulator, 10);
      IntSetClockHigh (demodulator);  
      ret = SWI2C_GET_SDA();
      //printf("\nGPIO_InputSDA ret = %d\n",ret);    
      //if(!ret)
      //   break;
//      Us_delay(demodulator,1000);
    }
    //printf("\nGPIO_InputSDA fail ret = %d\n",ret);
    return ret;
}


void GPIO_OutSDAHigh(
    IN  Demodulator*    demodulator
) {

    SWI2C_SET_SDA_H();
    return;
}


void GPIO_OutSDALow(
    IN  Demodulator*    demodulator
) {

    SWI2C_SET_SDA_L();
    return;
}


void GPIO_OutSCLHigh(
    IN  Demodulator*    demodulator
) {

    SWI2C_SET_SCL_H();
    return;
}

void GPIO_OutSCLLow(
    IN  Demodulator*    demodulator
) {

    SWI2C_SET_SCL_L();
    return;
}


bool IntGetSDA (
    IN  Demodulator*    demodulator
) {
    int ucValue;

    ucValue = (int) GPIO_InputSDA (demodulator);
    return (ucValue);
}


bool IntGetSCL (
    IN  Demodulator*    demodulator
) {
    return (true);
}



void IntSetSCLHigh (
    IN  Demodulator*    demodulator
) {
    GPIO_OutSCLHigh (demodulator);
}

void IntSetSCLLow (
    IN  Demodulator*    demodulator
) {
    GPIO_OutSCLLow (demodulator);
}

void IntSetSDAHigh (
    IN  Demodulator*    demodulator
){
    GPIO_OutSDAHigh (demodulator);
}

void IntSetSDALow (
    IN  Demodulator*    demodulator
) {
    GPIO_OutSDALow (demodulator);
}

Dword IntSetDataHigh (
    IN  Demodulator*    demodulator
) {
    Dword error = Error_NO_ERROR;

    IntSetSDAHigh (demodulator);
    return error;
}

Dword IntSetDataLow (
    IN  Demodulator*    demodulator
) {
    Dword error = Error_NO_ERROR;

    IntSetSDALow (demodulator);
    return error;
}

void IntSetClockHigh (
    IN  Demodulator*    demodulator
) {
    IntSetSCLHigh (demodulator);
}


void IntSetClockLow (
    IN  Demodulator*    demodulator
) {
    IntSetSCLLow (demodulator);
}

    
    
    
Dword IntStart2Wire (
    IN  Demodulator*    demodulator
) {
    Dword   error = Error_NO_ERROR;

    IntSetClockHigh (demodulator);

    Us_delay (demodulator, StartStopDelay);

    IntSetDataHigh (demodulator);

    Us_delay (demodulator, StartStopDelay * 3);

    IntSetDataLow (demodulator);

    Us_delay (demodulator, StartStopDelay);

    IntSetClockLow (demodulator);
    
    Us_delay (demodulator, ClkLowDelay * 2);

    return (error);
}


Dword IntStop2Wire (
    IN  Demodulator*    demodulator
) {
    Dword error = Error_NO_ERROR;

    IntSetDataLow (demodulator);
    
    Us_delay (demodulator, StartStopDelay);

    IntSetClockLow (demodulator);
    
    Us_delay (demodulator, StartStopDelay * 3);
    
    IntSetClockHigh (demodulator);

    Us_delay (demodulator, StartStopDelay);

    error = IntSetDataHigh (demodulator);
    if (error) goto exit;

exit :
    return (error);
}



Dword IntWriteAck (
    IN  Demodulator*    demodulator,
    IN  Byte            byte
) {
    Dword error = Error_NO_ERROR;
    short bit;

    for (bit = 7; bit >= 0; bit--) {
        if (byte & 0x80) {
            error = IntSetDataHigh (demodulator);               /** SDA --> HI */
            if (error) {
                goto exit;
            }
        } else {
            error = IntSetDataLow (demodulator);                /** SDA --> LO */
            if (error) {
                goto exit;
            }
        }
        byte <<= 1;
        
        Us_delay (demodulator, DataClkDelay);

        IntSetClockHigh (demodulator);                          /** SCL --> HI */
        
        Us_delay (demodulator, ClkHighDelay);

        IntSetClockLow (demodulator);                           /** SCL --> LO */
        
        Us_delay (demodulator, ClkLowDelay);
    }


    /** ACK phase */
    IntSetSDAHigh(demodulator);
//    IntSetClockHigh (demodulator);                              /** SCL --> HI */
    
    //Us_delay (demodulator, ClkHighDelay);
    //printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);

    if (IntGetSDA (demodulator)) {
       Us_delay (demodulator, ClkHighDelay);
        IntSetClockLow (demodulator);                           /** SCL --> LO */
        error = Error_I2C_WRITE_NO_ACK;
        goto exit;
    }
     Us_delay (demodulator, ClkHighDelay);
    IntSetClockLow (demodulator);                               /** SCL --> LO */
    
    Us_delay (demodulator, ClkLowDelay);

exit :
    return (error);
}

Dword IntReadAck (
    IN  Demodulator*    demodulator,
    IN  Byte*           ucpByte
) {
    Dword error = Error_NO_ERROR;
    short bit;
    Byte byte = 0;
	MS_U32 ret = 0;

    for (bit = 7; bit >= 0; bit--) {
        byte <<= 1;

		mdrv_gpio_init();
		SWI2C_SET_SDA_INPUT();

		Us_delay (demodulator, 10);
		
        IntSetClockHigh (demodulator);
        
 //       Us_delay (demodulator, DataClkDelay * 2);

        ret = SWI2C_GET_SDA();

		if((bool)ret) byte++;
        
        Us_delay (demodulator, ClkHighDelay);

        IntSetClockLow (demodulator);
        
        Us_delay (demodulator, ClkLowDelay);
    }

    /** ACK phase */
    /** Set SDA low */
    IntSetDataLow (demodulator);
    
    Us_delay (demodulator, DataClkDelay);

    /** Set SCL high */
    IntSetClockHigh (demodulator);                          /** SCL --> HI */
    
    Us_delay (demodulator, ClkHighDelay);

    /** Set SCL low */
    IntSetClockLow (demodulator);                           /** SCL --> LO */
    *ucpByte = byte;
    
    Us_delay (demodulator, ClkLowDelay);

    IntSetSDAHigh (demodulator);
    
//    Us_delay (demodulator, StartStopDelay * 2);

    return (error);
}


Dword IntLastReadAck (
    IN  Demodulator*    demodulator,
    IN  Byte*           ucpByte
) {
    Dword error = Error_NO_ERROR;
    short bit;
    Byte byte = 0;
	MS_U32 ret = 0;

    for (bit = 7; bit >= 0; bit--) {
        byte <<= 1;

		mdrv_gpio_init();
		SWI2C_SET_SDA_INPUT();

		Us_delay (demodulator, 10);
		
        IntSetClockHigh (demodulator);
        
//        Us_delay (demodulator, DataClkDelay);

		ret = SWI2C_GET_SDA();

        if ((bool)ret) byte++;
        
        Us_delay (demodulator, ClkHighDelay);

        IntSetClockLow (demodulator);
        
        Us_delay (demodulator, ClkLowDelay);
    }

    IntSetDataHigh (demodulator);
    
    Us_delay (demodulator, DataClkDelay);

    IntSetClockHigh (demodulator);
    
    Us_delay (demodulator, ClkHighDelay);

    IntSetClockLow (demodulator);
    *ucpByte = byte;
    
    Us_delay (demodulator, ClkLowDelay);

    IntSetSDAHigh (demodulator);
    
    Us_delay (demodulator, StartStopDelay);

    return (error);
}


Dword I2c_writeControlBus (
    IN  Demodulator*    demodulator,
    IN  Dword           bufferLength,
    IN  Byte*           buffer
) {
    Dword   error = Error_NO_ERROR;
    Dword   i;
    //printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);
    
    IntStart2Wire (demodulator);

//    printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);
    
        error = IntWriteAck (demodulator, I2c_address);

    //printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);

    if (error) goto exit;
    
    for (i = 0; i < bufferLength; i++) {
        error = IntWriteAck (demodulator, *(buffer + i));
        if (error) goto exit;
    }

exit :

    Us_delay (demodulator, StartStopDelay * 3);
    
    //printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);

    if (error)
        IntStop2Wire (demodulator);
    else
        error = IntStop2Wire (demodulator);

    return (error);

}



Dword I2c_readControlBus (
    IN  Demodulator*    demodulator,
    IN  Dword           bufferLength,
    OUT Byte*           buffer
) {
    Dword   error = Error_NO_ERROR;
    Dword   i;
    
    //printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);
    
    error = IntStart2Wire (demodulator);
    if (error) goto exit;

    //printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);
    
        error = IntWriteAck (demodulator, I2c_address | 0x01);
        
    if (error) goto exit;

        //printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);

    for (i = 0; i < bufferLength; i++) {
        if (i == (bufferLength - 1)) {
            error = IntLastReadAck (demodulator, buffer + i);
            if (error) goto exit;
        } else {
            error = IntReadAck (demodulator, buffer + i);
            if (error) goto exit;
        }
    }

exit:

    Us_delay (demodulator, StartStopDelay * 3);
    
    //printf("\n\n -%s, %d\n\n",__FUNCTION__,__LINE__);

    if (error)
        IntStop2Wire (demodulator);
    else
        error = IntStop2Wire (demodulator);

    return (error);
}
