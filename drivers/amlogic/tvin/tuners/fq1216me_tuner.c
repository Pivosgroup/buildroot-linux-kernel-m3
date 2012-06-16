/*
 * PHILIPS FQ1216ME-MK3 Tuner Device Driver
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

#define FQ1216_TUNER_I2C_NAME         "fq1216me_tuner_i2c"

/* tv tuner system standard selection for Philips FQ1216ME
   this value takes the low bits of control byte 2
   from datasheet "1999 Nov 16" (supersedes "1999 Mar 23")
     standard 		BG	DK	I	L	L`
     picture carrier	38.90	38.90	38.90	38.90	33.95
     colour		34.47	34.47	34.47	34.47	38.38
     sound 1		33.40	32.40	32.90	32.40	40.45
     sound 2		33.16	-	-	-	-
     NICAM		33.05	33.05	32.35	33.05	39.80
 */
#define PHILIPS_SET_PAL_I	    0x01 /* Bit 2 always zero !*/
#define PHILIPS_SET_PAL_BGDK	0xce
#define PHILIPS_SET_PAL_L	    0x0b
#define TUNER_FL                0x40

#define FREQ_HIGH	            442000000
#define FREQ_LOW	            160000000

#define TUNER_STEP_SIZE_0         50000
#define TUNER_STEP_SIZE_1         31250
#define TUNER_STEP_SIZE_2         166700
#define TUNER_STEP_SIZE_3         62500



typedef struct fq1216me_s {
    tuner_std_id  std_id;
	struct tuner_freq_s freq;
    struct tuner_parm_s parm;
} tuner_fq1216me_t;

static struct fq1216me_s   fq1216me = {
    TUNER_STD_PAL_BG | TUNER_STD_PAL_DK |
    TUNER_STD_PAL_I |TUNER_STD_SECAM_L |
    TUNER_STD_SECAM_LC,                     //tuner_std_id  std_id;
    {48250000, 0},                              //u32 frequency;
    {48250000, 863250000, 0, 0, 0},        //struct tuner_parm_s parm;
};


static int fq1216me_std_setup(u8 *cb, u32 *step_size)
{
	/* tv norm specific stuff for multi-norm tuners */
    *cb = 0x86;

	if (fq1216me.std_id & (TUNER_STD_PAL_BG|TUNER_STD_PAL_DK))
	{
		*cb |= PHILIPS_SET_PAL_BGDK;        //(bit0)os == 0, normal mode
		                                    //(bit[5:3])T == 0, normal mode
		                                    //(bit[6])CP == 0, (charge pump current)fast tuning
        *step_size = TUNER_STEP_SIZE_3;      //base on the(bit[2:1] of cb) RAS & RSB, set the step size
	}
	else if (fq1216me.std_id & TUNER_STD_PAL_I)
	{
		*cb |= PHILIPS_SET_PAL_I;           //(bit0)os == 1, switch off the pll amplifier
		                                    //(bit[5:3])T == 0, normal mode
		                                    //(bit[6])CP == 1, (charge pump current) moderate tuning with slight better residual oscillator
        *step_size = TUNER_STEP_SIZE_0;      //base on the(bit[2:1] of cb) RAS & RSB, set the step size
	}
	else if (fq1216me.std_id & TUNER_STD_SECAM_L)
	{
		*cb |= PHILIPS_SET_PAL_L;           //(bit0)os == 1, switch off the pll amplifier
		                                    //(bit[5:3])T == 0, normal mode
		                                    //(bit[6])CP == 1, (charge pump current) moderate tuning with slight better residual oscillator
        *step_size = TUNER_STEP_SIZE_1;      //base on the(bit[2:1] of cb) RAS & RSB, set the step size
	}

	return 0;
}



static struct i2c_client *tuner_client = NULL;

static int  fq1216me_tuner_read(char *buf, int len)
{
    int  i2c_flag = -1;
    struct i2c_msg msg[] = {
		{
			.addr	= tuner_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
        },
	};

	i2c_flag = i2c_transfer(tuner_client->adapter, msg, 1);
    if (i2c_flag < 0) {
//        pr_info("error in read fq1216me_tuner,  %d byte(s) should be read,. \n", len);
        return -EIO;
    }
    else
    {
//        pr_info("fq1216me_tuner_read is ok. \n");
        return 0;
    }

}

static int  fq1216me_tuner_write(char *buf, int len)
{
    int  i2c_flag;
    struct i2c_msg msg[] = {
	    {
			.addr	= tuner_client->addr,
			.flags	= 0,    //|I2C_M_TEN,
			.len	= len,
			.buf	= buf,
		}

	};
        i2c_flag = i2c_transfer(tuner_client->adapter, msg, 1);
		if (i2c_flag < 0) {
//			pr_info("error in writing fq1216me_tuner,  %d byte(s) should be writen,. \n", len);
			return -EIO;
		}
		else
        {
//            pr_info("fq1216me_tuner_write is ok. \n");
            return 0;
        }

}


/* ---------------------------------------------------------------------- */
static enum tuner_signal_status_e tuner_islocked(void)
{
    unsigned char status = 0;

    fq1216me_tuner_read(&status, 1);

    if ((status & TUNER_FL) != 0)
        return TUNER_SIGNAL_STATUS_STRONG;
    else
        return TUNER_SIGNAL_STATUS_WEAK;
}

/* ---------------------------------------------------------------------- */
int fq1216me_set_tuner(void)
{
	u8 buffer[5] = {0};
	u8 ab, cb, bb;
	u32 div, step_size = 0;
	unsigned int f_if;
    int  i2c_flag;

/* IFPCoff = Video Intermediate Frequency - Vif:
		940  =16*58.75  NTSC/J (Japan)
		732  =16*45.75  M/N STD
		704  =16*44     ATSC (at DVB code)
		632  =16*39.50  I U.K.
		622.4=16*38.90  B/G D/K I, L STD
		592  =16*37.00  D China
		590  =16.36.875 B Australia
		543.2=16*33.95  L' STD
		171.2=16*10.70  FM Radio (at set_radio_freq)
	*/

	if (fq1216me.std_id == TUNER_STD_SECAM_LC)
		f_if      = 33950000;
	else
		f_if      = 38900000;



    if (fq1216me.freq.freq < FREQ_LOW)
        bb = 0x01;          //low band
    else if (fq1216me.freq.freq > FREQ_HIGH)
        bb = 0x04;          //high band
    else
        bb = 0x02;          //mid band

	//tuner_dbg("Freq= %d.%02d MHz, V_IF=%d.%02d MHz, "
	//	  "Offset=%d.%02d MHz, div=%0d\n",
	//	  params->frequency / 16, params->frequency % 16 * 100 / 16,
	//	  IFPCoff / 16, IFPCoff % 16 * 100 / 16,
	//	  offset / 16, offset % 16 * 100 / 16, div);

	/* tv norm specific stuff for multi-norm tuners */
	fq1216me_std_setup(&cb, &step_size);

	div = (fq1216me.freq.freq + f_if)/step_size;  //with 62.5Khz step

    ab = 0x60;      //external AGC

	buffer[0] = (div >> 8) & 0xff;
	buffer[1] = div & 0xff;
	buffer[2] = cb;
	buffer[3] = bb;
	buffer[4] = ab;

    i2c_flag = fq1216me_tuner_write(buffer, 5);

    return i2c_flag;
}

void fq1216me_get_tuner( struct tuner_parm_s *ptp)
{
    ptp->rangehigh  = fq1216me.parm.rangehigh;
    ptp->rangelow   = fq1216me.parm.rangelow;
    ptp->signal     = tuner_islocked();
}


void fq1216me_get_std(tuner_std_id *ptstd)
{
    *ptstd = fq1216me.std_id;
}

void fq1216me_set_tuner_std(tuner_std_id ptstd)
{
    fq1216me.std_id = ptstd;
}

void fq1216me_get_freq(struct tuner_freq_s *ptf)
{
    ptf->freq = fq1216me.freq.freq;
}

void fq1216me_set_freq(struct tuner_freq_s tf)
{
    fq1216me.freq.freq = tf.freq;
}


static int fq1216me_tuner_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_info("%s: functionality check failed\n", __FUNCTION__);
        return -ENODEV;
    }
    tuner_client = client;
    printk( "tuner_client->addr = %x\n", tuner_client->addr);

	return 0;
}

static int fq1216me_tuner_remove(struct i2c_client *client)
{
    pr_info("%s driver removed ok.\n", client->name);
	return 0;
}

static const struct i2c_device_id fq1216me_tuner_id[] = {
	{ FQ1216_TUNER_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fq1216me_tuner_id);


static struct i2c_driver fq1216me_tuner_driver = {
	.driver = {
        .owner  = THIS_MODULE,
		.name = FQ1216_TUNER_I2C_NAME,
	},
	.probe		= fq1216me_tuner_probe,
	.remove		= fq1216me_tuner_remove,
	.id_table	= fq1216me_tuner_id,
};



static int __init fq1216me_tuner_init(void)
{
    int ret = 0;
    pr_info( "%s . \n", __FUNCTION__ );

    ret = i2c_add_driver(&fq1216me_tuner_driver);

    if (ret < 0 || tuner_client == NULL) {
        pr_err("tuner: failed to add i2c driver. \n");
        ret = -ENOTSUPP;
    }

	return ret;
}

static void __exit f11216me_tuner_exit(void)
{
    pr_info( "%s . \n", __FUNCTION__ );
    i2c_del_driver(&fq1216me_tuner_driver);
}

MODULE_AUTHOR("Bobby Yang <bo.yang@amlogic.com>");
MODULE_DESCRIPTION("Philips FQ1216ME-MK3 tuner i2c device driver");
MODULE_LICENSE("GPL");

module_init(fq1216me_tuner_init);
module_exit(f11216me_tuner_exit);

