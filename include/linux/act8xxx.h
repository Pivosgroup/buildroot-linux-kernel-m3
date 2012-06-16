/* 
 * act8942 i2c interface
 * Copyright (C) 2011 Amlogic, Inc.
 *
 *
 * Author:  elvis yu<elvis.yu@amlogic.com>
 */

#ifndef _PMU_ACT8xxx_H
#define _PMU_ACT8xxx_H

#define ACT8xxx_ADDR 0x5b

#ifdef CONFIG_PMU_ACT8942
#define	ACT8xxx_DEVICE_NAME		"pmu_act8942"
#define	ACT8xxx_CLASS_NAME		"act8942_class"
#define ACT8xxx_I2C_NAME		"act8942-i2c"
#elif defined(CONFIG_PMU_ACT8862)
#define	ACT8xxx_DEVICE_NAME		"pmu_act8862"
#define	ACT8xxx_CLASS_NAME		"act8862_class"
#define ACT8xxx_I2C_NAME		"act8862-i2c"
#endif


#define ACT8xxx_PMU_DEBUG_LOG		0

#if ACT8xxx_PMU_DEBUG_LOG == 1
	#define logd(x...)  	pr_info(x)
#else
	#define logd(x...)	
#endif

typedef enum act8xxx_reg { 
	ACT8xxx_REG1 = 1,
	ACT8xxx_REG2,
	ACT8xxx_REG3,
	ACT8xxx_REG4,
	ACT8xxx_REG5,
	ACT8xxx_REG6,
	ACT8xxx_REG7,
} act8xxx_regx;

#define ACT8xxx_SYS_ADDR 0x00
#define ACT8xxx_REG1_ADDR 0x20
#define ACT8xxx_REG2_ADDR 0x30
#define ACT8xxx_REG3_ADDR 0x40
#define ACT8xxx_REG4_ADDR 0x50
#define ACT8xxx_REG5_ADDR 0x54
#define ACT8xxx_REG6_ADDR 0x60
#define ACT8xxx_REG7_ADDR 0x64
#define ACT8942_APCH_ADDR 0x70

typedef struct act8xxx_i2c_msg
{
	u8 reg;
	u8 val;
}	act8xxx_i2c_msg_t;

#ifdef CONFIG_PMU_ACT8942
struct act8942_operations {
	int (*is_ac_online)(void);
	int (*is_usb_online)(void);
	void (*set_bat_off)(void);
	int (*get_charge_status)(void);
	int (*set_charge_current)(int level);
	int (*measure_voltage)(void);
	int (*measure_current)(void);
	int (*measure_capacity_charging)(void);
	int (*measure_capacity_battery)(void);

	unsigned int update_period; /* msecs, default is 5000 */
	unsigned int asn; /* Average Sample Number: 0 or 1 is disabled */
	unsigned int rvp; /* reverse voltage protection: 1:enable; 0:disable */
};
#endif

typedef union act8xxx_register_data
{
	/** raw register data */
	uint8_t d8;

	/** register bits */
	unsigned REGx_VSET : 6;

	struct 
	{
		/**
		 * System Voltage Detect Threshold. Defines the SYSLEV voltage
		 * threshold.
	 	 */
		unsigned SYSLEV : 4;

		/**
		 * System Voltage Status. Value is 1 when VVSYS is lower than the
		 * SYSLEV voltage threshold, value is 0 when VVSYS is higher than
		 * the system voltage detection threshold.
	 	 */
		unsigned nSYSSTAT : 1;

		/**
		 * System Voltage Level Interrupt Mask. SYSLEV interrupt is
		 * masked by default, set to 1 to unmask this interrupt.
	 	 */
		unsigned nSYSLEVMSK : 1;

		/**
		 * SYSLEV Mode Select. Defines the response to the SYSLEV
		 * voltage detector, 1: Generate an interrupt when VVSYS falls below
		 * the programmed SYSLEV threshold, 0: automatic shutdown
		 * when VVSYS falls below the programmed SYSLEV threshold.
	 	 */
		unsigned nSYSMODE : 1;

		/**
		 * Reset Timer Setting. Defines the reset time-out threshold. Reset
		 * time-out is 65ms when value is 1, reset time-out is 260ms when
		 * value is 0.
	 	 */
		unsigned TRST : 1;
	} SYS_0;
	
	struct 
	{
		/**
		 * Scratchpad Bits. Non-functional bits, maybe be used by user to
		 * store system status information. Volatile bits, which are cleared
		 * when system voltage falls below UVLO threshold.
	 	 */
		unsigned SCRATCH : 4;
		
		unsigned Reserved : 4;
	} SYS_1;
	
	struct 
	{
		/**
		 * Regulator Power-OK Status. Value is 1 when output voltage
		 * exceeds the power-OK threshold, value is 0 otherwise.
	 	 */
		unsigned OK : 1;

		/**
		 * Regulator Fault Mask Control. Set bit to 1 enable fault-interrupts,
		 * clear bit to 0 to disable fault-interrupts.
	 	 */
		unsigned nFLTMSK : 1;

		/** Regulator Turn-On Delay Control. */
		unsigned DELAY : 3;

		/**
		 * Regulator Mode Select. Set bit to 1 for fixed-frequency PWM
		 * under all load conditions, clear bit to 0 to transit to power-savings
		 * mode under light-load conditions.
	 	 */
		unsigned MODE : 1;

		/**
		 * Regulator Phase Control. Set bit to 1 for the regulator to operate
		 * 180¡ã out of phase with the oscillator, clear bit to 0 for the
		 * regulator to operate in phase with the oscillator.
	 	 */
		unsigned PHASE : 1;

		/**
		 * Regulator Enable Bit. Set bit to 1 to enable the regulator, clear bit
		 * to 0 to disable the regulator.
	 	 */
		unsigned ON : 1;
	} REGx_a;
	
	struct 
	{
		/**
		 * Regulator Power-OK Status. Value is 1 when output voltage
		 * exceeds the power-OK threshold, value is 0 otherwise.
	 	 */
		unsigned OK : 1;
		
		/**
		 * Regulator Fault Mask Control. Set bit to 1 enable faultinterrupts,
		 * clear bit to 0 to disable fault-interrupts.
	 	 */
		unsigned nFLTMSK : 1;

 		/** Regulator Turn-On Delay Control. */
		unsigned DELAY : 3;

		/**
		 * LDO Low-IQ Mode Control. Set bit to 1 for low-power
		 * operating mode, clear bit to 0 for normal mode.
	 	 */
		unsigned LOWIQ : 1;

		/**
		 * Output Discharge Control. When activated, LDO output is
		 * discharged to GA through 1.5k¦¸ resistor when in shutdown.
		 * Set bit to 1 to enable output voltage discharge in shutdown,
		 * clear bit to 0 to disable this function.
	 	 */
		unsigned DIS : 1;

		/**
		 * Regulator Enable Bit. Set bit to 1 to enable the regulator,
		 * clear bit to 0 to disable the regulator.
	 	 */
		unsigned ON : 1;
	} REGx_b;
} act8xxx_register_data_t;

int act8xxx_is_ac_online(void);

#endif

