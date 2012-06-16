/*
 * PHILIPS FQ1216ME-MK3 Demodulation Device Driver
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


/* Standard Liniux Headers */
#include <linux/module.h>
#include <linux/i2c.h>

/* Amlogic Headers */
#include <linux/tvin/tvin.h>

/* Local Headers */
#include "fq1216me_demod.h"

/*********************IF configure*****************************/
//// first reg (b)
#define VideoTrapBypassOFF     0x00    // bit b0
#define VideoTrapBypassON      0x01    // bit b0

#define AutoMuteFmInactive     0x00    // bit b1
#define AutoMuteFmActive       0x02    // bit b1

#define Intercarrier           0x00    // bit b2
#define QSS                    0x04    // bit b2

#define PositiveAmTV           0x00    // bit b3:4
#define FmRadio                0x08    // bit b3:4
#define NegativeFmTV           0x10    // bit b3:4


#define ForcedMuteAudioON      0x20    // bit b5
#define ForcedMuteAudioOFF     0x00    // bit b5

#define OutputPort1Active      0x00    // bit b6
#define OutputPort1Inactive    0x40    // bit b6

#define OutputPort2Active      0x00    // bit b7
#define OutputPort2Inactive    0x80    // bit b7


//// second reg (c)
#define DeemphasisOFF          0x00    // bit c5
#define DeemphasisON           0x20    // bit c5

#define Deemphasis75           0x00    // bit c6
#define Deemphasis50           0x40    // bit c6

#define AudioGain0             0x00    // bit c7
#define AudioGain6             0x80    // bit c7

#define TopMask                0x1f    // bit c0:4
#define TopDefault		       0x10 	// bit c0:4

//// third reg (e)
#define AudioIF_4_5             0x00    // bit e0:1
#define AudioIF_5_5             0x01    // bit e0:1
#define AudioIF_6_0             0x02    // bit e0:1
#define AudioIF_6_5             0x03    // bit e0:1


#define VideoIFMask		        0x1c	// bit e2:4
/* Video IF selection in TV Mode (bit B3=0) */
#define VideoIF_58_75           0x00    // bit e2:4
#define VideoIF_45_75           0x04    // bit e2:4
#define VideoIF_38_90           0x08    // bit e2:4
#define VideoIF_38_00           0x0C    // bit e2:4
#define VideoIF_33_90           0x10    // bit e2:4
#define VideoIF_33_40           0x14    // bit e2:4
#define RadioIF_45_75           0x18    // bit e2:4
#define RadioIF_38_90           0x1C    // bit e2:4

/* IF1 selection in Radio Mode (bit B3=1) */
#define RadioIF_33_30		    0x00	// bit e2,4 (also 0x10,0x14)
#define RadioIF_41_30		    0x04	// bit e2,4

/* Output of AFC pin in radio mode when bit E7=1 */
#define RadioAGC_SIF		    0x00	// bit e3
#define RadioAGC_FM		        0x08	// bit e3

#define TunerGainNormal         0x00    // bit e5
#define TunerGainLow            0x20    // bit e5

#define Gating_18               0x00    // bit e6
#define Gating_36               0x40    // bit e6

#define AgcOutON                0x80    // bit e7
#define AgcOutOFF               0x00    // bit e7


#define FQ1216_DEMOD_I2C_NAME         "fq1216me_demod_i2c"


typedef struct if_param_s {
	tuner_std_id      std;
	unsigned char     b;
	unsigned char     c;
	unsigned char     e;
} if_param_t;

static struct if_param_s if_param[] = {
	{
		.std   = TUNER_STD_PAL_BG,
		.b     = (  NegativeFmTV  |
                    AutoMuteFmActive |
			        QSS           ),
		.c     = (  DeemphasisON  |
			        Deemphasis50     |
			        TopDefault),
		.e     = (  Gating_36     |
                    AudioIF_5_5      |
			        VideoIF_38_90),
	},
	{
		.std   = TUNER_STD_PAL_I,
		.b     = (  NegativeFmTV  |
                    AutoMuteFmActive |
			        QSS           ),
		.c     = (  DeemphasisON  |
                    Deemphasis50     |
                    TopDefault),
		.e     = (  Gating_36     |
                    AudioIF_6_0      |
			        VideoIF_38_90 ),
	},
	{
		.std   = TUNER_STD_PAL_DK,
		.b     = (  NegativeFmTV  |
                    AutoMuteFmActive |
			        QSS           ),
		.c     = (  DeemphasisON  |
			        Deemphasis50     |
			        TopDefault),
		.e     = (  Gating_36     |
			        AudioIF_6_5      |
			        VideoIF_38_90 ),
	},
	{
		.std   = TUNER_STD_SECAM_L,
		.b     = (  PositiveAmTV  |
                    AutoMuteFmActive |
			        QSS           ),
		.c     = (  TopDefault),
		.e     = (  Gating_36	    |
			        AudioIF_6_5      |
			        VideoIF_38_90 ),
	},
	{
		.std   = TUNER_STD_SECAM_LC,
		.b     = (  OutputPort2Inactive |
                    AutoMuteFmActive |
			        PositiveAmTV           |
			        QSS           ),
		.c     = (  TopDefault),
		.e     = (  Gating_36	    |
                    AudioIF_6_5      |
                    VideoIF_33_90 ),
	}
};


static tuner_std_id demod_std = 0;


void fq1216me_set_demod_std(tuner_std_id ptstd)
{
    demod_std = ptstd;
}

static struct i2c_client *tuner_afc_client = NULL;


static int  fq1216me_afc_read(char *buf, int len)
{
    int  i2c_flag = -1;
    struct i2c_msg msg[] = {
		{
			.addr	= tuner_afc_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
        },
	};


	i2c_flag = i2c_transfer(tuner_afc_client->adapter, msg, 1);
    if (i2c_flag < 0) {
//        pr_err("error in read fq1216me_afc, %d byte(s) should be read,. \n", len);
        return -EIO;
    }
    else
    {
//        pr_info("fq1216me_afc_read is ok. \n");
        return 0;
    }

}

static int  fq1216me_afc_write(char *buf, int len)
{
    int  i2c_flag;
    struct i2c_msg msg[] = {
	    {
			.addr	= tuner_afc_client->addr,
			.flags	= 0,    //|I2C_M_TEN,
			.len	= len,
			.buf	= buf,
		}

	};
        i2c_flag = i2c_transfer(tuner_afc_client->adapter, msg, 1);
		if (i2c_flag < 0) {
//			pr_err("error in writing fq1216me_afc, %d byte(s) should be writen,. \n", len);
			return -EIO;
		}
		else
        {
//            pr_info("fq1216me_afc_write is ok. \n");
            return 0;
        }

}



void fq1216me_get_afc(struct tuner_parm_s *ptp)
{
#if 0
	static int afc_bits_2_hz[] = {
		 -12500,  -37500,  -62500,  -87500,
		-112500, -137500, -162500, -187500,
		 187500,  162500,  137500,  112500,
		  87500,   62500,   37500,  12500
	};
#endif
    int i2c_flag;
    unsigned char status;
    i2c_flag = fq1216me_afc_read(&status, 1);
    if(i2c_flag == 0)
        ptp->if_status = status;	//afc_bits_2_hz[(status >> 1) & 0x0F];

}


int fq1216me_set_demod(void)
{
	struct if_param_s *param = NULL;
	unsigned char buf[4] = {0, 0, 0, 0};
	int i, i2c_flag = -1;

	for (i = 0; i < ARRAY_SIZE(if_param); i++) {
		if (if_param[i].std & demod_std) {
			param = if_param + i;
			break;
		}
	}

	if (NULL == param) {
		//tuner_dbg("Unsupported tvnorm entry - audio muted\n");
		return -1;
	}

	//tuner_dbg("configure for: %s\n", norm->name);
	buf[1] = param->b;
	buf[2] = param->c;
	buf[3] = param->e;

    i2c_flag = fq1216me_afc_write(buf, 4);

    return i2c_flag;
}



static int fq1216me_demod_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

 if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
     pr_info("%s: functionality check failed\n", __FUNCTION__);
     return -ENODEV;
 }
 tuner_afc_client = client;

 pr_info( " %s: tuner_demod_client->addr = %x\n", __FUNCTION__, tuner_afc_client->addr);

	return 0;
}

static int fq1216me_demod_remove(struct i2c_client *client)
{
    pr_info("%s driver removed ok.\n", client->name);
	return 0;
}

static const struct i2c_device_id fq1216me_demod_id[] = {
	{ FQ1216_DEMOD_I2C_NAME, 0 },
	{ }
};


static struct i2c_driver fq1216me_demod_driver = {
	.driver = {
        .owner  = THIS_MODULE,
		.name = FQ1216_DEMOD_I2C_NAME,
	},
	.probe		= fq1216me_demod_probe,
	.remove		= fq1216me_demod_remove,
	.id_table	= fq1216me_demod_id,
};



static int __init fq1216me_demod_init(void)
{
    int ret = 0;
    pr_info( "%s . \n", __FUNCTION__ );

    ret = i2c_add_driver(&fq1216me_demod_driver);

    if (ret < 0 || tuner_afc_client == NULL) {
        pr_err("tuner demod: failed to add i2c driver. \n");
        ret = -ENOTSUPP;
    }

	return ret;

}

static void __exit fq1216me_demod_exit(void)
{
    pr_info( "%s . \n", __FUNCTION__ );

	i2c_del_driver(&fq1216me_demod_driver);
}

MODULE_AUTHOR("Bobby Yang <bo.yang@amlogic.com>");
MODULE_DESCRIPTION("Philips FQ1216ME-MK3 demodulation i2c device driver");
MODULE_LICENSE("GPL");

module_init(fq1216me_demod_init);
module_exit(fq1216me_demod_exit);


