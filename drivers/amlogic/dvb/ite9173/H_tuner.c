/**
 * @(#)Afatech_SAMBA.cpp
 *
 * ==========================================================
 * Version: 2.0
 * Date:    2009.06.15
 * ==========================================================
 *
 * ==========================================================
 * History:
 *
 * Date         Author      Description
 * ----------------------------------------------------------
 *
 * 2009.06.15   M.-C. Ho    new tuner
 * ==========================================================
 *
 * Copyright 2009 Afatech, Inc. All rights reserved.
 *
 */


//#include <stdio.h>
#include "type.h"
#include "error.h"
#include "user.h"
#include "register.h"
#define __SAMBADEMOD_H__
#include "standard.h"
#include "tuner.h"
#include "samba.h"
#include "Afa_Samba_Script.h"

Demodulator* Afatech_SAMBA_demodulator;

Dword SAMBA_open (
	IN  Demodulator*	demodulator
) {
	Dword error = Error_NO_ERROR;
	Afatech_SAMBA_demodulator = demodulator;
	error = samba_init();

	return (error);
}

Dword SAMBA_close (
	IN  Demodulator*	demodulator
) {
	return (Error_NO_ERROR);
}

Dword SAMBA_set (
	IN  Demodulator*	demodulator,
	IN  Word			bandwidth,
	IN  Dword			frequency
) {
	Dword error = Error_NO_ERROR;
	Afatech_SAMBA_demodulator = demodulator;
	error = samba_setfreq((unsigned int)bandwidth, (unsigned int)frequency);
	
	return (error);
}


TunerDescription tunerDescription= {
    SAMBA_open,
    SAMBA_close,
    SAMBA_set,
    SAMBA_scripts,
    SAMBA_scriptSets,
    SAMBA_ADDRESS,			/** tuner i2c address */
    2,						/** length of tuner register address */
    0,						/** tuner if */
    False,					/** spectrum inverse */
    0x70,					/** tuner id */
};

Dword SAMBA_supportLNA (
    IN  Demodulator*    demodulator,
    IN  Byte            supporttype
 ) {
	Dword error = Error_INVALID_DEV_TYPE;
	
	return error;
}
