/*
 *********************************************************************
 * File        : bat_sbi.c
 * Description : This file implement the Smart battery interface.
 * Note      : it depends on bat_i2c driver.
 * Author   : Shoufu Zhao <shoufu.zhao@amlogic.com> 2010/11/24
 *********************************************************************
 */
#include <linux/kernel.h>
#include <linux/err.h> 

#include "bat_i2c.h"
#include "bat_sbi.h"

#define BAT_SBI_DEBUG_LOG		0

#if BAT_SBI_DEBUG_LOG == 1
	#define logd(x...)  	printk(x)
#else
	#define logd(x...)		NULL
#endif


int Temperature(unsigned short *temperature)
{
	if(bat_i2c_read_word(SBI_TEMPERATURE, (unsigned short *)temperature) < 0){
		logd("Temperature err.\r\n");
		return -1;
	}
	return 0;
}

int Voltage(unsigned short *voltage)
{
	if(bat_i2c_read_word(SBI_VOLTAGE, (unsigned short *)voltage) < 0 ){
		logd ("Voltage err.\r\n");
		return -1;
	}
	return 0;
}

int Current(unsigned short *tmp_current)
{
	if(bat_i2c_read_word(SBI_CURRENT, (unsigned short *)tmp_current) < 0){
		logd ("Current err.\r\n");
		return -1;
	}
	return 0;
}

int RelativeStateOfCharge(unsigned short *rsoc)
{
	if(bat_i2c_read_word(SBI_RELATIVE_STATE_OF_CHARGE, (unsigned short *)rsoc) < 0){
		logd("RelativeStateOfCharge err.\r\n");
		return -1;
	}
	return 0;
}

int RemainingCapacity(unsigned short *capacity)
{
	if(bat_i2c_read_word(SBI_REMAINING_CAPACITY, (unsigned short *)capacity) < 0){
		logd("RemainingCapacity err.\r\n");
		return -1;
	}
	return 0;
}

int FullChargeCapacity(unsigned short *capacity)
{
	if(bat_i2c_read_word(SBI_FULL_CHARGE_CAPACITY, (unsigned short *)capacity) < 0){
		logd("FullChargeCapacity err.\r\n");
		return -1;
	}
	return 0;
}

int AverageTimeToEmpty(unsigned short *time)
{
	if(bat_i2c_read_word(SBI_AVERAGE_TIME_TO_EMPTY, (unsigned short *)time) < 0){
		logd("AverageTimeToEmpty err.\r\n");
		return -1;
	}
	return 0;
}

int BatteryStatus(unsigned short *status)
{
	if(bat_i2c_read_word(SBI_BATTERY_STATUS, (unsigned short *)status) < 0){
		logd("BatteryStatus err.\r\n");
		return -1;
	}
	return 0;
}

int ChargingCurrent(unsigned short *tmp_current)
{
	if(bat_i2c_read_word(SBI_CHARGING_CURRENT, (unsigned short *)tmp_current) < 0){
		logd("ChargingCurrent err.\r\n");
		return -1;
	}
	return 0;
}

int ChargingVoltage(unsigned short *voltage)
{
	if(bat_i2c_read_word(SBI_CHARGING_VOLTAGE, (unsigned short *)voltage) < 0){
		logd("ChargingVoltage err.\r\n");
		return -1;
	}
	return 0;
}

int DesignCapacity(unsigned short *capacity)
{
	if(bat_i2c_read_word(SBI_DESIGN_CAPACITY, (unsigned short *)capacity) < 0){
		logd("DesignCapacity err.\r\n");
		return -1;
	}
	return 0;
}

int DesignVoltage(unsigned short *voltage)
{
	if(bat_i2c_read_word(SBI_DESIGN_VOLTAGE, (unsigned short *)voltage) < 0){
		logd("DesignVoltage err.\r\n");
		return -1;
	}
	return 0;
}

int ManufactureDate(unsigned short *date)
{
	if(bat_i2c_read_word(SBI_MANUFACTURE_DATE, (unsigned short *)date) < 0){
		logd("ManufactureDate err.\r\n");
		return -1;
	}
	return 0;
}

int SerialNumber(unsigned short *number)
{
	if(bat_i2c_read_word(SBI_SERIAL_NUMBER, (unsigned short *)number) < 0){
		logd("SerialNumber err.\r\n");
		return -1;
	}
	return 0;
}

int ManufactureName(unsigned char *name)
{
	if(bat_i2c_block_read(SBI_MANUFACTURE_NAME, (unsigned char *)name) < 0){
		logd("SerialNumber err.\r\n");
		return -1;
	}
	return 0;
}

int DeviceName(unsigned char *name)
{
	if(bat_i2c_block_read(SBI_DEVICE_NAME, (unsigned char *)name) < 0){
		logd("SerialNumber err.\r\n");
		return -1;
	}
	return 0;
}

int DeviceChemistry(unsigned char *chemistry)
{
	if(bat_i2c_block_read(SBI_DEVICE_CHEMISTRY, (unsigned char *)chemistry) < 0){
		logd("SerialNumber err.\r\n");
		return -1;
	}
	return 0;
}

int AverageVoltage(unsigned short *voltage) 
{
	if(bat_i2c_read_word(SBI_AVERAGE_VOLTAGE, (unsigned short *)voltage) < 0){
		logd("AverageVoltage err.\r\n");
		return -1;
	}
	return 0;
}

int AverageCurrent(short *tmp_current)
{
	if(bat_i2c_read_word(SBI_AVERAGE_CURRENT, (unsigned short *)tmp_current) < 0){
		logd("AverageCurrent err.\r\n");
		return -1;
	}
	return 0;
}

int RunTimeToEmpty(unsigned short *minute)
{
	if(bat_i2c_read_word(SBI_RUN_TIME_TO_EMPTY, (unsigned short *)minute) < 0){
		logd("RunTimeToEmpty err.\r\n");
		return -1;
	}
	return 0;
}

int StateOfHealth(unsigned short *health)
{
	if(bat_i2c_read_word(SBI_STATE_OF_HEALTH, (unsigned short *)health) < 0){
		logd("StateOfHealth err.\r\n");
		return -1;
	}
	return 0;
}

