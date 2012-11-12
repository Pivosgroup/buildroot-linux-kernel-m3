


#ifndef _ITE9173SF_H_
#define _ITE9173SF_H_



#include <linux/dvb/frontend.h>
#include "../../../media/dvb/dvb-core/dvb_frontend.h"
#include "../aml_dvb.h"


#define printf printk


struct ite9173_fe_config {
	int                   i2c_id;
	int                 reset_pin;
	int                 demod_addr;
	int                 tuner_addr;
	void 			  *i2c_adapter;
};


struct ite9173_state {
	struct ite9173_fe_config config;
	struct i2c_adapter *i2c;
	u32                 freq;
        fe_modulation_t     mode;
        u32                 symbol_rate;
        struct dvb_frontend fe;
};

#endif

