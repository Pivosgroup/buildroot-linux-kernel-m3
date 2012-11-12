
#include "type.h"


StreamType streamType = StreamType_DVBT_PARALLEL;//StreamType_DVBT_SERIAL;//StreamType_DVBT_PARALLEL;//StreamType_DVBT_SERIAL;//Modified by Roan 2012-03-14

DefaultDemodulator demod = {
	NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    12000,
    2025000,
//20480,
//2048000,
    StreamType_DVBT_PARALLEL,//StreamType_DVBT_PARALLEL,//StreamType_DVBT_SERIAL,//Modified by Roan 2012-03-14
    8000,
    642000,
    0x00000000,
	{False, False, 0, 0},
    0,
    False,
    False,
	0,
	User_I2C_ADDRESS,
	False
};


