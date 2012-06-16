/*
 * Copyright (C) 2010 MEMSIC, Inc.
 *
 * Initial Code:
 *	Robbie Cao
 * 	Dale Hou
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/* ------------------ */
/* - Include Files. - */
/* ------------------ */

#ifdef __KERNEL__
#include <linux/module.h>
#endif

#include "mpu.h"
#include "mlsl.h"
#include "mlos.h"

#include <log.h>
#undef MPL_LOG_TAG
#define MPL_LOG_TAG "MPL-compass"

#define DEBUG		0

#define MMC328X_I2C_NAME	"mmc328x"
/*
 * This address comes must match the part# on your target.
 * Address to the sensor part# support as following list:
 *	MMC3280MS - 0110000b
 *	MMC3281MS - 0110001b
 *	MMC3282MS - 0110010b
 *	MMC3283MS - 0110011b
 *	MMC3284MS - 0110100b
 *	MMC3285MS - 0110101b
 *	MMC3286MS - 0110110b
 *	MMC3287MS - 0110111b
 * Please refer to sensor datasheet for detail.
 */
#define MMC328X_I2C_ADDR		0x30

/* MMC328X register address */
#define MMC328X_REG_CTRL		0x07
#define MMC328X_REG_DATA		0x00
#define MMC328X_REG_DS			0x06

/* MMC328X control bit */
#define MMC328X_CTRL_TM			0x01
#define MMC328X_CTRL_RM			0x20
#define MAX_FAILURE_COUNT	3
#define READMD			1

#define MMC328X_DELAY_TM	10	/* ms */
#define MMC328X_DELAY_RM	10	/* ms */
#define MMC328X_DELAY_STDN	1	/* ms */

#define MMC328X_RETRY_COUNT	3
#define MMC328X_RESET_INTV	10

#define MMC328X_DEV_NAME	"mmc328x"

static u32 read_idx = 0;
enum read_stat_en {
    READ_STAT_DATA, //read data
    READ_STAT_RM,   //RM
    READ_STAT_TM,    //TM
};

static enum read_stat_en read_stat = READ_STAT_DATA;



static int reset_int = 1000;
static int read_count = 1;


int mmc328x_suspend(void *mlsl_handle,
		    struct ext_slave_descr *slave,
		    struct ext_slave_platform_data *pdata)
{
	int result = ML_SUCCESS;

	return result;
}

int mmc328x_resume(void *mlsl_handle,
		   struct ext_slave_descr *slave,
		   struct ext_slave_platform_data *pdata)
{

	int result;
#if 1
	result =
	    MLSLSerialWriteSingle(mlsl_handle, pdata->address,
				  MMC328X_REG_CTRL, MMC328X_CTRL_RM);
	ERROR_CHECK(result);
	MLOSSleep(MMC328X_DELAY_RM);
#endif
	result =
	    MLSLSerialWriteSingle(mlsl_handle, pdata->address,
				  MMC328X_REG_CTRL, MMC328X_CTRL_TM);
	ERROR_CHECK(result);
	MLOSSleep(MMC328X_DELAY_TM);
	read_count = 1;
	read_stat = READ_STAT_DATA;
	return ML_SUCCESS;
}

int mmc328x_read(void *mlsl_handle,
		 struct ext_slave_descr *slave,
		 struct ext_slave_platform_data *pdata,
		 unsigned char *data)
{
    int result, ii;
    short tmp[3];
    unsigned char tmpdata[6];


    if (read_count > 1000)
	read_count = 1;

    if (read_count % reset_int == 0) {
	if(READ_STAT_DATA == read_stat)
	{
	    read_stat = READ_STAT_RM;
	    /* RM */
	    /* not check return value here, assume it always OK */
	    result =
		MLSLSerialWriteSingle(mlsl_handle,
			pdata->address,
			MMC328X_REG_CTRL,
			MMC328X_CTRL_RM);
	    read_stat = READ_STAT_TM;
	    /* wait external capacitor charging done for next RM */
	    return ML_ERROR_COMPASS_DATA_NOT_READY;
	}
	else if (READ_STAT_TM == read_stat) //TM
	{
	    /* send TM cmd before read */
	    /* not check return value here, assume it always OK */
	    result =
		MLSLSerialWriteSingle(mlsl_handle,
			pdata->address,
			MMC328X_REG_CTRL,
			MMC328X_CTRL_TM);

	    read_stat = READ_STAT_DATA;
	    read_count ++; //break "if (read_count % reset_int == 0)"
	    /* wait TM done for coming data read */
	    return ML_ERROR_COMPASS_DATA_NOT_READY;
	}
    }

    result =
	MLSLSerialRead(mlsl_handle, pdata->address, MMC328X_REG_DS,
		1, (unsigned char *) data);
    if( 0 == 0x01&data[0] )
    {
	read_count ++;
	return ML_ERROR_COMPASS_DATA_NOT_READY;
    }

    result =
	MLSLSerialRead(mlsl_handle, pdata->address, MMC328X_REG_DATA,
		6, (unsigned char *) data);
    ERROR_CHECK(result);

    for (ii = 0; ii < 6; ii++)
	tmpdata[ii] = data[ii];

    for (ii = 0; ii < 3; ii++) {
	tmp[ii] =
	    (short) ((tmpdata[2 * ii + 1] << 8) + tmpdata[2 * ii]);
	tmp[ii] = tmp[ii] - 4096; //?
	tmp[ii] = tmp[ii] * 16;   //?
    }

    for (ii = 0; ii < 3; ii++) {
	data[2 * ii] = (unsigned char) (tmp[ii] >> 8);
	data[2 * ii + 1] = (unsigned char) (tmp[ii]);
    }
    /* send TM cmd before read */
    /* not check return value here, assume it always OK */
    result =
	MLSLSerialWriteSingle(mlsl_handle,
		pdata->address,
		MMC328X_REG_CTRL,
		MMC328X_CTRL_TM);

    read_stat = READ_STAT_DATA;
    read_count ++;

    return ML_SUCCESS;
}

struct ext_slave_descr mmc328x_descr = {
	/*.init             = */ NULL,
	/*.exit             = */ NULL,
	/*.suspend          = */ mmc328x_suspend,
	/*.resume           = */ mmc328x_resume,
	/*.read             = */ mmc328x_read,
	/*.config           = */ NULL,
	/*.get_config       = */ NULL,
	/*.name             = */ "mmc328x",
	/*.type             = */ EXT_SLAVE_TYPE_COMPASS,
	/*.id               = */ COMPASS_ID_MMC328X,
	/*.reg              = */ 0x00,
	/*.len              = */ 6,
	/*.endian           = */ EXT_SLAVE_BIG_ENDIAN,
	/*.range            = */ {400, 0},
};

struct ext_slave_descr *mmc328x_get_slave_descr(void)
{
	return &mmc328x_descr;
}
EXPORT_SYMBOL(mmc328x_get_slave_descr);

/**
 *  @}
**/
