#include "user.h"



/**
 * Variable of critical section
 */

Dword User_memoryCopy (
    IN  Demodulator*    demodulator,
    IN  void*           dest,
    IN  void*           src,
    IN  Dword           count
) {
    /*
     *  ToDo:  Add code here
     *
     *  //Pseudo code
     *  memcpy(dest, src, (size_t)count);
     *  return (0);
     */
    return (Error_NO_ERROR);
}

Dword User_delay (
    IN  Demodulator*    demodulator,
    IN  Dword           dwMs
) {
    /*
     *  ToDo:  Add code here
     *
     *  //Pseudo code
     *  delay(dwMs);
     *  return (0);
     */
	msleep(dwMs);
 //    SysDelay(dwMs);
    return (Error_NO_ERROR);
}


Dword User_enterCriticalSection (
    IN  Demodulator*    demodulator
) {
    /*
     *  ToDo:  Add code here
     *
     *  //Pseudo code
     *  return (0);
     */
    return (Error_NO_ERROR);
}


Dword User_leaveCriticalSection (
    IN  Demodulator*    demodulator
) {
    /*
     *  ToDo:  Add code here
     *
     *  //Pseudo code
     *  return (0);
     */
    return (Error_NO_ERROR);
}


Dword User_mpegConfig (
    IN  Demodulator*    demodulator
) {
    /*
     *  ToDo:  Add code here
     *
     */
    return (Error_NO_ERROR);
}


Dword User_busTx (
    IN  Demodulator*    demodulator,
    IN  Dword           bufferLength,
    IN  Byte*           buffer
) {

	unsigned long  error   = 0;

	DefaultDemodulator*   demod;
	demod     = (DefaultDemodulator*) demodulator;
	error = I2CWrite((Word)demod->demodAddr,buffer,(Word)bufferLength);

	return error;     
}


Dword User_busRx (
    IN  Demodulator*    demodulator,
    IN  Dword           bufferLength,
    OUT Byte*           buffer
) {

	unsigned long  error   = 0;

	DefaultDemodulator*   demod;
	demod     = (DefaultDemodulator*) demodulator;

	error = I2CRead((Word)demod->demodAddr,buffer,(Word)bufferLength);

	return error;     
}


Dword User_busRxData (
    IN  Demodulator*    demodulator,
    IN  Dword           bufferLength,
    OUT Byte*           buffer
) {
    return (Error_NO_ERROR);
}


extern int ite9133_get_fe_config(struct ite9133_fe_config *cfg);

Dword I2CWrite(Word I2CSlaveAddr,  Byte *data, Word length)
{
	int ret = 0;
	int i;
	struct i2c_msg msg;
//	printk("I2CSlaveAddr is %x, length is %d\n",I2CSlaveAddr,length);
	memset(&msg, 0, sizeof(msg));
	msg.addr = I2CSlaveAddr;
	msg.flags = 0;
	msg.buf = data;
	msg.len = length;

	struct	ite9133_fe_config cfg;	
	ite9133_get_fe_config(&cfg);

	ret = i2c_transfer((struct i2c_adapter *)cfg.i2c_adapter, &msg, 1);
	if(ret<0) {
		printk(" %s: writereg error, errno is %d \n", __FUNCTION__, ret);
		return 1;
	}

	return 0;
 }
 
Dword  I2CRead(Word I2CSlaveAddr,  Byte *data, Word length)
{
	 int ret = 0;
	 int i;
	 struct i2c_msg msg;
	 
	 memset(&msg, 0, sizeof(msg));
	 msg.addr = I2CSlaveAddr;
	 msg.flags |=  I2C_M_RD;
	 msg.buf = data;
	 msg.len = length;

	struct	ite9133_fe_config cfg;
	ite9133_get_fe_config(&cfg);

	 ret = i2c_transfer((struct i2c_adapter *)cfg.i2c_adapter, &msg, 1); 
	if(ret<0) {
		printk(" %s: readreg error, errno is %d \n", __FUNCTION__, ret);
		return 1;
	}

	return 0;
 }


