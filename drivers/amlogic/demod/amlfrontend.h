/*****************************************************************
**
**  Copyright (C) 2010 Amlogic,Inc.
**  All rights reserved
**        Filename : amlfrontend.h
**
**  comment:
**        Driver for aml demodulator
** 
*****************************************************************/

#ifndef _AMLFRONTEND_H
#define _AMLFRONTEND_H

struct amlfe_config {
	int                   fe_mode;
	int                   i2c_id;
	int                   tuner_type;
	int                   tuner_addr;
};

/*
struct dvb_frontend *aml_fe_dvbc_attach(const struct aml_fe_config *config, const int id);
struct dvb_frontend *aml_fe_dvbt_attach(const struct aml_fe_config *config, const int id);
*/

#endif
