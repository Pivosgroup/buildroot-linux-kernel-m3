/*	TI SDIO function */


#include <linux/err.h>

#include <linux/workqueue.h>

#include <linux/slab.h> 

#include <linux/dma-mapping.h>

#include <linux/delay.h>

#include <linux/cardreader/sdio.h>

#include <linux/cardreader/card_block.h>

#include "SdioDrv.h"

//#define SDIO_IO

typedef enum{

SDIO_DEBUGLEVEL_EMERG=1,

SDIO_DEBUGLEVEL_ALERT,

SDIO_DEBUGLEVEL_CRIT,

SDIO_DEBUGLEVEL_ERR=4,

SDIO_DEBUGLEVEL_WARNING,

SDIO_DEBUGLEVEL_NOTICE,

SDIO_DEBUGLEVEL_INFO,

SDIO_DEBUGLEVEL_DEBUG=8

}sdio_debuglevel;
 


#define SW_VERSION_STR      "0.9.0.0"



int g_sdio_debug_level = SDIO_DEBUGLEVEL_DEBUG;
extern int sdio_io_rw_extended(struct memory_card *card, int write, unsigned fn, unsigned addr, int incr_addr, u8 *buf, unsigned blocks, unsigned blksz);
extern int sdio_io_rw_direct(struct memory_card *card, int write, unsigned fn, unsigned addr, u8 in, u8 *out);
int SDIO_LOCKED_FLAG = 0;

#ifdef SDIO_DEBUG


#define PERR(format, args... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_ERR) printk(format , ##args)

#define PDEBUG(format, args... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_DEBUG) printk(format , ##args)

#define PINFO(format, ... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_INFO) printk( format , ##__VA_ARGS__)

#define PNOTICE(format, ... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_NOTICE) printk( format , ##__VA_ARGS__)

#define PWARNING(format, ... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_WARNING) printk(format , ##__VA_ARGS__)



#else



#define PERR(format, args... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_ERR) printk(format , ##args)

#define PDEBUG(format, args... )

#define PINFO(format, ... )

#define PNOTICE(format, ... )

#define PWARNING(format, ... )



#endif




typedef struct _sdiodrv

{

  	struct clk    		*fclk, *iclk, *dbclk;

  	int           		dma_tx_channel;

  	int           		dma_rx_channel;

  	void          		(*BusTxnCB)(void* BusTxnHandle, int status);

  	void*         		BusTxnHandle;

  	unsigned int  		uBlkSize;

  	unsigned int  		uBlkSizeShift;

	int           		async_status;

	struct sdio_func	*func;

	int 				max_blocksize;

	int 				int_enabled;

	int			sdio_host_claim_ref;

}  TI_sdiodrv_t;



static TI_sdiodrv_t ti_drv;

static struct sdio_func *tiwlan_func[1 + SDIO_TOTAL_FUNCS];

/************************************************************************

 * Defines

 ************************************************************************/

//#define SDIO_BITS_CODE   0x80 /* 1 bits */

#define SDIO_BITS_CODE   0x82 /* 4 bits */



#define SYNC_ASYNC_LENGTH_THRESH 4096



#define MAX_RETRIES                 10

#define MAX_BUS_TXN_SIZE            8192  /* Max bus transaction size in bytes (for the DMA buffer allocation) */



/* For block mode configuration */

#define FN0_FBR2_REG_108                    0x210

#define FN0_FBR2_REG_108_BIT_MASK           0xFFF 



/* Card Common Control Registers (CCCR) */

#define CCCR_SDIO_REVISION                  0x00

#define CCCR_SD_SPECIFICATION_REVISION      0x01

#define CCCR_IO_ENABLE                      0x02

#define CCCR_IO_READY                       0x03

#define CCCR_INT_ENABLE                     0x04

#define CCCR_INT_PENDING                    0x05

#define CCCR_IO_ABORT                       0x06

#define CCCR_BUS_INTERFACE_CONTOROL         0x07

#define CCCR_CARD_CAPABILITY	            0x08

#define CCCR_COMMON_CIS_POINTER             0x09 /*0x09-0x0B*/

#define CCCR_FNO_BLOCK_SIZE	                0x10 /*0x10-0x11*/

#define FN0_CCCR_REG_32                     0x64



#define TXN_FUNC_ID_CTRL         0

#define TXN_FUNC_ID_BT           1

#define TXN_FUNC_ID_WLAN         2



#define TESTDRV_CODE_RAM_PART_START_ADDR 		0

#define TESTDRV_SDIO_FUNC1_OFFSET           	0x1FFC0  /* address of the partition table */

#define TESTDRV_MAX_PART_SIZE  					0x1F000 	/* 124k	*/



#define TESTDRV_REG_PART_START_ADDR 			0x300000

#define TESTDRV_REG_DOWNLOAD_PART_SIZE 			0x8800 		/* 44k	*/ 	

#define TESTDRV_REG_WORKING_PART_SIZE 			0xB000 		/* 44k	*/



/* Partition Size Left for Memory */

#define TESTDRV_MEM_WORKING_PART_SIZE  			(TESTDRV_MAX_PART_SIZE - TESTDRV_REG_WORKING_PART_SIZE) 	 

#define TESTDRV_MEM_DOWNLOAD_PART_SIZE  		(TESTDRV_MAX_PART_SIZE - TESTDRV_REG_DOWNLOAD_PART_SIZE) 	 



typedef enum

{

    TXN_STATUS_NONE = 2,	/**< */

    TXN_STATUS_OK,         	/**< */

    TXN_STATUS_COMPLETE,  	/**< */ 

    TXN_STATUS_PENDING,    	/**< */

    TXN_STATUS_ERROR     	/**< */



} ETxnStatus;


static unsigned int g_mem_partition_size = TESTDRV_MEM_DOWNLOAD_PART_SIZE;

static unsigned int g_reg_partition_size = TESTDRV_REG_DOWNLOAD_PART_SIZE;



#define TESTDRV_CODE_RAM_PART_START_ADDR 		0		

#define TESTDRV_DATA_RAM_PART_START_ADDR 		0x20000000

#define TESTDRV_PACKET_RAM_PART_START_ADDR 		0x40000

#define TESTDRV_CODE_RAM_SIZE  					0x30000		/* 192K	*/

#define TESTDRV_DATA_RAM_SIZE 					0xC000		/* 48K 	*/

#define TESTDRV_PACKET_RAM_SIZE 				0xD000		/* 52K 	*/

#define SDIO_TEST_NO_OF_TRANSACTIONS			(3)

#define TESTDRV_READ_TEST         0x00000001

#define TESTDRV_WRITE_TEST        0x00000002

#define TESTDRV_COMPARE_TEST      0x00000004

#define TESTDRV_FUNC0_TEST        0x00000008

#define TESTDRV_FUNC0_READ_TEST   0x00000010

#define TESTDRV_TESTING_DATA_LENGTH 512

#define SDIO_TEST_FIRST_VALID_DMA_ADDR			(0x00000008)	/* used for escaping addressing invalid DMA Addresses */

#define SDIO_TEST_ALIGNMENT				4

#define SDIO_TEST_R_W_BUFS_NUM			10

#define SDIO_TEST_R_W_BUF_LEN			512

#define SDIO_TEST_MAX_R_W_BUF_ARRAY		(SDIO_TEST_R_W_BUF_LEN*SDIO_TEST_R_W_BUFS_NUM)

#define TESTDRV_512_SDIO_BLOCK					(512)

#define TESTDRV_MAX_SDIO_BLOCK					(TESTDRV_512_SDIO_BLOCK /* - 4 */)

#define DO_ON_ERROR(RETURN_VALUE, STRING) \
	printk(STRING); \
	if (RETURN_VALUE!=0) \
	{ \
		printk(" FAILED !!!\n"); \
		return ret; \
	} \
	else \
	{ \
		printk(" PASSED\n" ); \
	} \

char	              *read_buf=NULL;

char	              *read_buf_array[SDIO_TEST_R_W_BUFS_NUM];

char	              *write_buf=NULL;

char	              *write_buf_array[SDIO_TEST_R_W_BUFS_NUM];


void sdioDrv_ClaimHost(unsigned int uFunc)
{
    if (ti_drv.sdio_host_claim_ref)
        return;

/* currently only wlan sdio function is supported */
    BUG_ON(uFunc != SDIO_WLAN_FUNC);
    BUG_ON(tiwlan_func[uFunc] == NULL);

    ti_drv.sdio_host_claim_ref = 1;

    //sdio_claim_host(tiwlan_func[uFunc]);
}

void sdioDrv_ReleaseHost(unsigned int uFunc)
{
    if (!ti_drv.sdio_host_claim_ref)
        return;

    /* currently only wlan sdio function is supported */
    BUG_ON(uFunc != SDIO_WLAN_FUNC);
    BUG_ON(tiwlan_func[uFunc] == NULL);

    ti_drv.sdio_host_claim_ref = 0;

    //sdio_release_host(tiwlan_func[uFunc]);
}
int sdioDrv_ConnectBus (void *fCbFunc, void *hCbArg, unsigned int uBlkSizeShift, unsigned int uSdioThreadPriority)

{

    ti_drv.BusTxnCB      = fCbFunc;

    ti_drv.BusTxnHandle  = hCbArg;

    ti_drv.uBlkSizeShift = uBlkSizeShift;  

    ti_drv.uBlkSize      = 1 << uBlkSizeShift;

//	printk("sdioDrv_ConnectBus BusTxnCB: %x BusTxnHandle: %x\n", ti_drv.BusTxnCB, ti_drv.BusTxnHandle);

//	printk("sdioDrv_ConnectBus uBlkSizeShift: %d(%d) uBlkSize: %d(%d)\n", ti_drv.uBlkSizeShift, uBlkSizeShift, ti_drv.uBlkSize, (1 << uBlkSizeShift));

    return 0;

}



#if 0

int sdioDrv_ExecuteCmd (unsigned int uCmd, 

                        unsigned int uArg, 

                        unsigned int uRespType, 

                        void *       pResponse, 

                        unsigned int uLen)

{

	return 0;

}

#endif



int sdioDrv_WriteSyncBytes (unsigned int  uFunc, 

                            unsigned int  uHwAddr, 

                            unsigned char *pData, 

                            unsigned int  uLen, 

                            unsigned int  bMore)

{

	unsigned int i;

	int	iStatus;

        if(!SDIO_LOCKED_FLAG){


	sdio_claim_host(ti_drv.func);

    for (i = 0; i < uLen; i++) 

    {

#ifdef SDIO_IO
        sdio_writeb(ti_drv.func, *pData, uHwAddr, &iStatus);
#else

        iStatus = sdio_io_rw_direct(ti_drv.func->card, 1, uFunc, uHwAddr, *pData, NULL);

#endif

        if (iStatus) 

        {

            PERR("sdioDrv_WriteSyncBytes() SDIO Command error status = 0x%x\n", iStatus);

			sdio_release_host(ti_drv.func);

            return iStatus;

        }



        pData++;

        uHwAddr++;

    }

	sdio_release_host(ti_drv.func);


}
    return 0;

}

static void sdioDrv_irq(struct sdio_func *func)
{
	PDEBUG("%s:\n", __func__);
}

int sdioDrv_ReadSyncBytes (unsigned int  uFunc, 

                           unsigned int  uHwAddr, 

                           unsigned char *pData, 

                           unsigned int  uLen, 

                           unsigned int  bMore)

{

	unsigned int i;

	int	iStatus;


        if(!SDIO_LOCKED_FLAG){

	sdio_claim_host(ti_drv.func);

    for (i = 0; i < uLen; i++) 

    {

#ifdef SDIO_IO
	*pData = sdio_readb(ti_drv.func, uHwAddr, &iStatus);
#else

        iStatus = sdio_io_rw_direct(ti_drv.func->card, 0, uFunc, uHwAddr, 0, pData);

#endif        



        if (iStatus) 

        {

            PERR("sdioDrv_ReadSyncBytes() SDIO Command error status = 0x%x\n", iStatus);

			sdio_release_host(ti_drv.func);

            return iStatus;

        }



        pData++;

        uHwAddr++;

    }

	sdio_release_host(ti_drv.func);


}
    return 0;

}



int sdioDrv_ReadAsync (unsigned int uFunc, 

                       unsigned int uHwAddr, 

                       void *       pData, 

                       unsigned int uLen, 

                       unsigned int bBlkMode,

                       unsigned int bIncAddr,

                       unsigned int bMore)

{

	int         iStatus;
	u8 			*pData1;
#ifndef SDIO_IO
	int         nsize;
	unsigned 	remainder = uLen;
#endif

	sdio_claim_host(ti_drv.func);

	pData1 = (u8 *)pData;

	

#ifdef SDIO_IO
	iStatus = sdio_memcpy_fromio(ti_drv.func, pData1, uHwAddr, uLen);
#else

	while (remainder > 512) {

		unsigned blocks;



		blocks = remainder / 512;

		nsize = blocks * 512;



		iStatus = sdio_io_rw_extended(ti_drv.func->card, 0, uFunc, uHwAddr, bIncAddr, pData1, blocks, 512);

		if (iStatus)

		{

			PERR("sdioDrv_ReadAsync() buffer disabled! length = %d PSTATE = 0x%x\n", 

			  		remainder, iStatus);

			sdio_release_host(ti_drv.func);

			return iStatus;

		}



		remainder -= nsize;

		pData1 += nsize;

		uHwAddr += nsize;

	}

	

	while (remainder > 0)

	{

		if (remainder > 512) nsize = 512;

		else

			nsize = remainder;

		iStatus = sdio_io_rw_extended(ti_drv.func->card, 0, uFunc, uHwAddr, bIncAddr, pData1, 1, nsize);



		if (iStatus)

		{

        	PERR("sdioDrv_ReadAsync() FAILED(%x)!!\n", iStatus);

			sdio_release_host(ti_drv.func);

        	return iStatus;

		}

		remainder -= nsize;

		uHwAddr += nsize;

		pData1 += nsize;

	}

#endif

	

	if (ti_drv.BusTxnCB)

	{

//		printk("sdioDrv_WriteAsync BusTxnCB= %x, BusTxnHandle= %x, iStatus= %d\n", ti_drv.BusTxnCB, ti_drv.BusTxnHandle, iStatus);

		ti_drv.BusTxnCB(ti_drv.BusTxnHandle, iStatus);

	}

	

	sdio_release_host(ti_drv.func);

	return iStatus;

}



int sdioDrv_WriteAsync (unsigned int uFunc, 

                        unsigned int uHwAddr, 

                        void *       pData, 

                        unsigned int uLen, 

                        unsigned int bBlkMode,

                        unsigned int bIncAddr,

                        unsigned int bMore)

{

	int         iStatus;
	u8 			*pData1;
#ifndef SDIO_IO
	int         nsize;
	unsigned 	remainder = uLen;
#endif

	printk("%s\n", __func__);
	sdio_claim_host(ti_drv.func);

	pData1 = (u8 *)pData;

	

#ifdef SDIO_IO
	iStatus = sdio_memcpy_toio(ti_drv.func, uHwAddr, pData1, uLen);
#else	

	while (remainder > 512) {

		unsigned blocks;



		blocks = remainder / 512;

		nsize = blocks * 512;



		iStatus = sdio_io_rw_extended(ti_drv.func->card, 1, uFunc, uHwAddr, bIncAddr, pData1, blocks, 512);

		if (iStatus)

		{

			PERR("sdioDrv_WriteAsync() buffer disabled! length = %d PSTATE = 0x%x\n", 

			  		remainder, iStatus);

			sdio_release_host(ti_drv.func);

			return iStatus;

		}



		remainder -= nsize;

		pData1 += nsize;

		uHwAddr += nsize;

	}

	

	while (remainder > 0)

	{

		if (remainder > 512) nsize = 512;

		else

			nsize = remainder;

		iStatus = sdio_io_rw_extended(ti_drv.func->card, 1, uFunc, uHwAddr, bIncAddr, pData1, 1, nsize);



		if (iStatus)

		{

        	PERR("sdioDrv_WriteAsync() FAILED(%x)!!\n", iStatus);

			sdio_release_host(ti_drv.func);

        	return iStatus;

		}

		remainder -= nsize;

		uHwAddr += nsize;

		pData1 += nsize;

	}

#endif

	

	if (ti_drv.BusTxnCB)

	{

//		printk("sdioDrv_WriteAsync BusTxnCB= %x, BusTxnHandle= %x, iStatus= %d\n", ti_drv.BusTxnCB, ti_drv.BusTxnHandle, iStatus);

		ti_drv.BusTxnCB(ti_drv.BusTxnHandle, iStatus);

	}

	

	sdio_release_host(ti_drv.func);

	return iStatus;

}



int sdioDrv_ReadSync (unsigned int uFunc, 

                      unsigned int uHwAddr, 

                      void *       pData, 

                      unsigned int uLen,

                      unsigned int bIncAddr,

                      unsigned int bMore)

{

	int         iStatus;
	u8 			*pData1;
#ifndef SDIO_IO
	int         nsize;
	unsigned 	remainder = uLen;
#endif

        if(!SDIO_LOCKED_FLAG){

	sdio_claim_host(ti_drv.func);

	pData1 = (u8 *)pData;

	

#ifdef SDIO_IO
	iStatus = sdio_memcpy_fromio(ti_drv.func, pData1, uHwAddr, uLen);
#else	

	while (remainder > 512) {

		unsigned blocks;



		blocks = remainder / 512;

		nsize = blocks * 512;



		iStatus = sdio_io_rw_extended(ti_drv.func->card, 0, uFunc, uHwAddr, bIncAddr, pData1, blocks, 512);

		if (iStatus)

		{

			PERR("sdioDrv_ReadSync() buffer disabled! length = %d PSTATE = 0x%x\n", 

			  		remainder, iStatus);

			sdio_release_host(ti_drv.func);

			return iStatus;

		}



		remainder -= nsize;

		pData1 += nsize;

		uHwAddr += nsize;

	}

	

	while (remainder > 0)

	{

		if (remainder > 512) nsize = 512;

		else

			nsize = remainder;

		iStatus = sdio_io_rw_extended(ti_drv.func->card, 0, uFunc, uHwAddr, bIncAddr, pData1, 1, nsize);



		if (iStatus)

		{

        	PERR("sdioDrv_ReadSync() FAILED(%x)!!\n", iStatus);

			sdio_release_host(ti_drv.func);

			return iStatus;

		}

		

		remainder -= nsize;

		uHwAddr += nsize;

		pData1 += nsize;

	}

#endif



	sdio_release_host(ti_drv.func);

	return iStatus;
}
return 0;
}



int sdioDrv_WriteSync (unsigned int uFunc, 

                       unsigned int uHwAddr, 

                       void *       pData, 

                       unsigned int uLen,

                       unsigned int bIncAddr,

                       unsigned int bMore)

{

	int         iStatus;
	u8 			*pData1;
#ifndef SDIO_IO
	int         nsize;
	unsigned 	remainder = uLen;
#endif
        if(!SDIO_LOCKED_FLAG){
	sdio_claim_host(ti_drv.func);

	pData1 = (u8 *)pData;

	

#ifdef SDIO_IO
	iStatus = sdio_memcpy_toio(ti_drv.func, uHwAddr, pData1, uLen);
#else	

	while (remainder > 512) {

		unsigned blocks;



		blocks = remainder / 512;

		nsize = blocks * 512;



		iStatus = sdio_io_rw_extended(ti_drv.func->card, 1, uFunc, uHwAddr, bIncAddr, pData1, blocks, 512);

		if (iStatus)

		{

			PERR("sdioDrv_WriteSync() buffer disabled! length = %d PSTATE = 0x%x\n", 

			  		remainder, iStatus);

			sdio_release_host(ti_drv.func);

			return iStatus;

		}



		remainder -= nsize;

		pData1 += nsize;

		uHwAddr += nsize;

	}

	

	while (remainder > 0)

	{

		if (remainder > 512) nsize = 512;

		else

			nsize = remainder;

		iStatus = sdio_io_rw_extended(ti_drv.func->card, 1, uFunc, uHwAddr, bIncAddr, pData1, 1, nsize);



		if (iStatus)

		{

        	PERR("sdioDrv_WriteSync() FAILED(%x)!!\n", iStatus);

			sdio_release_host(ti_drv.func);

			return iStatus;

		}

		remainder -= nsize;

		uHwAddr += nsize;

		pData1 += nsize;

	}

#endif

	

	sdio_release_host(ti_drv.func);

	return iStatus;
}
return 0;
}



int sdioDrv_DisconnectBus (void)

{

    return 0;

}

int sdioDrv_DisableFunction(unsigned int uFunc)
{
	PDEBUG("%s: func %d\n", __func__, uFunc);

	/* currently only wlan sdio function is supported */
//	BUG_ON(uFunc != SDIO_WLAN_FUNC);
//	BUG_ON(tiwlan_func[uFunc] == NULL);
	
	return sdio_disable_func(tiwlan_func[uFunc]);
}

int sdioDrv_EnableFunction(unsigned int uFunc)
{
	PDEBUG("%s: func %d\n", __func__, uFunc);

	/* currently only wlan sdio function is supported */
	BUG_ON(uFunc != SDIO_WLAN_FUNC);
    BUG_ON(tiwlan_func[uFunc] == NULL);
	
	return sdio_enable_func(tiwlan_func[uFunc]);
}
int sdioDrv_EnableInterrupt(unsigned int uFunc)
{
	PDEBUG("%s: func %d\n", __func__, uFunc);

	/* currently only wlan sdio function is supported */
	BUG_ON(uFunc != SDIO_WLAN_FUNC);
	BUG_ON(tiwlan_func[uFunc] == NULL);

	return sdio_claim_irq(tiwlan_func[uFunc], sdioDrv_irq);
}

int sdioDrv_DisableInterrupt(unsigned int uFunc)
{
	PDEBUG("%s: func %d\n", __func__, uFunc);

	/* currently only wlan sdio function is supported */
	BUG_ON(uFunc != SDIO_WLAN_FUNC);
	BUG_ON(tiwlan_func[uFunc] == NULL);

	return sdio_release_irq(tiwlan_func[uFunc]);
}

int sdioDrv_SetBlockSize(unsigned int uFunc, unsigned int blksz)
{
	PDEBUG("%s: func %d\n", __func__, uFunc);

	/* currently only wlan sdio function is supported */
	BUG_ON(uFunc != SDIO_WLAN_FUNC);
	BUG_ON(tiwlan_func[uFunc] == NULL);

	return sdio_set_block_size(tiwlan_func[uFunc], blksz);
}



/*----------------------------SDIO Test Function----------------------*/

struct parts

{

	unsigned long mem_size;

	unsigned long mem_off;

	unsigned long reg_size;

	unsigned long reg_off;

}g_parts;



#define MEM_PART_ADDR 0

#define MEM_PART_SIZE 0x16800

#define REG_PART_ADDR 0x300000

#define REG_PART_SIZE 0x8800



typedef enum

{

	SdioSync= 0,

	SdioAsync= 1,

	SdioSyncMax

} SdioSyncMode_e;



/*--------------------------------------------------------------------------------------*/

inline int set_partition(int clientID, 

						 unsigned long mem_partition_size,

						 unsigned long mem_partition_offset,

						 unsigned long reg_partition_size,

						 unsigned long reg_partition_offset)

{

	int status;

	char byteData[4];

//	struct mmc_card *card = ti_drv.func->card;



	g_parts.mem_size 	= mem_partition_size;

	g_parts.mem_off 	= mem_partition_offset;

	g_parts.reg_size 	= reg_partition_size;

	g_parts.reg_off 	= reg_partition_offset;



	/* set mem partition size */

	byteData[0] = g_parts.mem_size & 0xff;

	byteData[1] = (g_parts.mem_size>>8) & 0xff;

	byteData[2] = (g_parts.mem_size>>16) & 0xff;

	byteData[3] = (g_parts.mem_size>>24) & 0xff;

	//printk("SDIO_test 1: 0x%x 0x%x 0x%x 0x%x\n",byteData[0],byteData[1],byteData[2],byteData[3]);

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET,   &byteData[0], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+1, &byteData[1], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+2, &byteData[2], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+3, &byteData[3], 1, 0);

	if(status)return status;



	/* set mem partition offset */

	byteData[0] = g_parts.mem_off & 0xff;

	byteData[1] = (g_parts.mem_off>>8) & 0xff;

	byteData[2] = (g_parts.mem_off>>16) & 0xff;

	byteData[3] = (g_parts.mem_off>>24) & 0xff;

	//printk("SDIO_test 2: 0x%x 0x%x 0x%x 0x%x\n",byteData[0],byteData[1],byteData[2],byteData[3]);

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+4, &byteData[0], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+5, &byteData[1], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+6, &byteData[2], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+7, &byteData[3], 1, 0);

	if(status)return status;



	/* set reg partition size */

	byteData[0] = g_parts.reg_size & 0xff;

	byteData[1] = (g_parts.reg_size >> 8) & 0xff;

	byteData[2] = (g_parts.reg_size>>16) & 0xff;

	byteData[3] = (g_parts.reg_size>>24) & 0xff;

	//printk("SDIO_test 3: 0x%x 0x%x 0x%x 0x%x\n",byteData[0],byteData[1],byteData[2],byteData[3]);

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+8, &byteData[0], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+9, &byteData[1], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+10, &byteData[2], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+11, &byteData[3], 1, 0);

	if(status)return status;



	/* set reg partition offset */

	byteData[0] = g_parts.reg_off & 0xff;

	byteData[1] = (g_parts.reg_off>>8) & 0xff;

	byteData[2] = (g_parts.reg_off>>16) & 0xff;

	byteData[3] = (g_parts.reg_off>>24) & 0xff;

	//printk("SDIO_test 4: 0x%x 0x%x 0x%x 0x%x\n",byteData[0],byteData[1],byteData[2],byteData[3]);

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+12, &byteData[0], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+13, &byteData[1], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+14, &byteData[2], 1, 0);

	if(status)return status;

	status = sdioDrv_WriteSyncBytes(ti_drv.func->num, TESTDRV_SDIO_FUNC1_OFFSET+15, &byteData[3], 1, 0);

	if(status)return status;



	/* printk("exiting %s\n",__FUNCTION__); */

	return status;

} /* set_partition */



static int sync_read_test(int number_of_transactions, int block_len)

{

	int i;

	int ret = 0;

	for(i=0; i<number_of_transactions; i++){

		ret = sdioDrv_ReadSync(ti_drv.func->num, SDIO_TEST_FIRST_VALID_DMA_ADDR, read_buf_array[i%SDIO_TEST_R_W_BUFS_NUM], block_len, 1, 1);

//		printk("read: 0x%x\n",(char *)read_buf_array[i%SDIO_TEST_R_W_BUFS_NUM]);

		if(ret){

			return -ENODEV;

		}

	}



	return ret;



} /* sync_read_test() */



static int sync_write_test(int number_of_transactions, int block_len)

{

	int i;

	int ret = 0;

	for(i=0;i<number_of_transactions;i++)

	{

	    ret = sdioDrv_WriteSync(ti_drv.func->num, SDIO_TEST_FIRST_VALID_DMA_ADDR, write_buf_array[i%SDIO_TEST_R_W_BUFS_NUM], block_len, 1, 1);

		//printk("write: 0x%x\n",(char *)write_buf_array[i%SDIO_TEST_R_W_BUFS_NUM]);

        if(ret)

		{

			return -ENODEV;

		}

	}



	return ret;



} /* sync_write_test() */



static int sync_compare_test(int number_of_transactions, int block_len)

{

	int i,j,ret = 0,*my_read_buf,*my_write_buf;



	for(i=0;i<number_of_transactions;i++)

	{

		my_read_buf  = (int *)read_buf_array[i%SDIO_TEST_R_W_BUFS_NUM];

		my_write_buf = (int *)write_buf_array[i%SDIO_TEST_R_W_BUFS_NUM];

		ret = sdioDrv_WriteSync(ti_drv.func->num, SDIO_TEST_FIRST_VALID_DMA_ADDR, my_write_buf, block_len, 1, 1);

		if(ret){

			return -ENODEV;

		}

		ret = sdioDrv_ReadSync(ti_drv.func->num, SDIO_TEST_FIRST_VALID_DMA_ADDR, my_read_buf, block_len, 1, 1);

		if(ret){

			return -ENODEV;

		}

		if (memcmp(my_write_buf,my_read_buf,block_len))

		{

		  printk("\nERROR: Write(0x%x)/Read(0x%x) (%d) buffers are not identical!\n",(int)my_write_buf,(int)my_read_buf,i);

		  for (j=0; j < block_len/4; j++,my_read_buf++,my_write_buf++)

		  {

		    printk("  R[%d] = 0x%x, W[%d] = 0x%x\n", j, *my_read_buf, j, *my_write_buf);

		  }

		  return -1;

		}

	}



	return ret;



} /* sync_compare_test() */



static int sync_func0_test(int number_of_transactions, int block_len)

{

	int i,j,ret = 0,*my_read_buf,*my_write_buf;



	for(i=0;i<number_of_transactions;i++)

	{

		my_read_buf  = (int *)read_buf_array[i%SDIO_TEST_R_W_BUFS_NUM];

		my_write_buf = (int *)write_buf_array[i%SDIO_TEST_R_W_BUFS_NUM];

		*my_write_buf = i;

		ret = sdioDrv_WriteSync(TXN_FUNC_ID_CTRL, 0xC0, my_write_buf, block_len, 1, 1);

		if(ret){

			return -ENODEV;

		}

		ret = sdioDrv_ReadSync(TXN_FUNC_ID_CTRL, 0xC0, my_read_buf, block_len, 1, 1);

		if(ret){

			return -ENODEV;

		}

		if (memcmp(my_write_buf,my_read_buf,block_len))

		{

		  printk("\nERROR: Write(0x%x)/Read(0x%x) (%d) buffers are not identical!\n",(int)my_write_buf,(int)my_read_buf,i);

		  for (j=0; j < block_len/4; j++,my_read_buf++,my_write_buf++)

		  {

		    printk("  R[%d] = 0x%x, W[%d] = 0x%x\n", j, *my_read_buf, j, *my_write_buf);

		  }

		  return -1;

		}

	}



	return ret;



} /* sync_func0_test() */



static int sync_func0_read_test(int number_of_transactions, int block_len)

{

    int ret = 0;



    memset(write_buf_array[0], 0, SDIO_TEST_R_W_BUF_LEN);

    *(write_buf_array[0]) = 0x55;

    *(write_buf_array[0] + 1) = 0xaa;



    ret = sdioDrv_ReadSync(TXN_FUNC_ID_CTRL, 0xC0, read_buf_array[0], 4, 1, 1);

    if (ret) { return -ENODEV; }



    ret = sdioDrv_WriteSync(TXN_FUNC_ID_CTRL, 0xC0, write_buf_array[0], 2, 1, 1);

    if (ret) { return -ENODEV; }



    ret = sdioDrv_ReadSync(TXN_FUNC_ID_CTRL, 0xC0, read_buf_array[0], 4, 1, 1);

    if (ret) { return -ENODEV; }



    printk("C0 = 0x%x, C1 = 0x%x, C2 = 0x%x, C3 = 0x%x, \n",

           *(read_buf_array[0]), *(read_buf_array[0]+1), *(read_buf_array[0]+2), *(read_buf_array[0]+3));



	return ret;



} /* sync_func0_test() */



static int perform_test(char* string, int (*test_func)(int, int), int number_of_transactions, int block_len)

{

	unsigned long exectime,basetime,endtime;

	int ret;



	printk("%s", string);

	basetime = jiffies;

	ret = test_func(number_of_transactions, block_len);

	endtime = jiffies;

	exectime = endtime>=basetime? endtime-basetime : 0xffffffff-basetime+endtime;

	exectime = jiffies_to_msecs(exectime);

	printk(": %d*%d bytes took %lu [msecs] ", number_of_transactions, block_len, exectime);

	if (exectime!=0)

		printk("=> %d [Mbps]\n", (int)((number_of_transactions*8*block_len)/(exectime*1000)));

	else

		printk("\n");



	return ret;



} /* perform_test() */



static int do_test_sync(unsigned long tests)

{

	int ret=0;



	if (tests & TESTDRV_READ_TEST)

	{

	  ret = perform_test("Starting sync read test\n", sync_read_test, 10000, TESTDRV_TESTING_DATA_LENGTH);

	  DO_ON_ERROR(ret, "sync read test\n");

	}

	if (tests & TESTDRV_WRITE_TEST)

	{

	  ret = perform_test("Starting sync write test\n", sync_write_test, 10000, TESTDRV_TESTING_DATA_LENGTH);

	  DO_ON_ERROR(ret, "sync write test\n");

	}

	if (tests & TESTDRV_COMPARE_TEST)

	{

	  ret = perform_test("Starting sync wr/rd compare test\n", sync_compare_test, 10000, TESTDRV_TESTING_DATA_LENGTH);

	  DO_ON_ERROR(ret, "sync wr/rd compare test\n");

	}

	if (tests & TESTDRV_FUNC0_TEST)

	{

	  ret = perform_test("Starting sync func-0 wr/rd compare test\n", sync_func0_test, 10000, 4);

	  DO_ON_ERROR(ret, "sync func-0 wr/rd compare test\n");

	}

	if (tests & TESTDRV_FUNC0_READ_TEST)

	{

	  ret = perform_test("Starting sync func-0 read test", sync_func0_read_test, 0, 0);

	  DO_ON_ERROR(ret, "sync func-0 read test");

	}



	return ret;



} /* do_test_sync() */



static int init_read_buf(void)

{

	int i;



	if ((read_buf = kmalloc(SDIO_TEST_MAX_R_W_BUF_ARRAY, __GFP_DMA)) == NULL)

	{

		kfree(read_buf);

		return -ENOMEM;

	}



	for(i=0 ; i<SDIO_TEST_R_W_BUFS_NUM ; i++)

	{

		/* zero read buffers */

		read_buf_array[i] = read_buf+i*SDIO_TEST_R_W_BUF_LEN;

		memset(read_buf_array[i],0,SDIO_TEST_R_W_BUF_LEN);

	}



	return 0;

}



static int init_write_buf(void)

{

	int i, z;



	if ((write_buf = kmalloc(SDIO_TEST_MAX_R_W_BUF_ARRAY, __GFP_DMA)) == NULL)

	{

		kfree(write_buf);

		return -ENOMEM;

	}



	for(i=0 ; i<SDIO_TEST_R_W_BUFS_NUM ; i++)

	{

		/* fill write buffers with non-sero content */

		write_buf_array[i] = write_buf+i*SDIO_TEST_R_W_BUF_LEN;

		for (z=0; z<SDIO_TEST_R_W_BUF_LEN; z++)

			write_buf_array[i][z] = z%250 + 1;

	}



	return 0;

}





static int init_buffers(void)

{

	if (init_write_buf() != 0)

	{

		printk("init_buffers: init_write_buf failed\n");

		return (-1);

	}



	if (init_read_buf() != 0)

	{

		printk("init_buffers: init_read_buf failed\n");

		return (-1);

	}



	return(0);



} /* init_buffers() */



static unsigned long sdio_test_get_base(char target)

{

	unsigned long base;



	switch (target)

	{

		case 'r':		/* WLAN Registers */

		case 'R':

			base = g_mem_partition_size;

			break;



		case 'm':		/* WLAN Memory */

		case 'M':

			base = 0;

			break;



		default:

			base = 0;

			break;

	}



	return base;



} /* sdio_test_get_base() */



#define BUFLEN 100



static void sdio_test_write(const char* buffer, unsigned int base, SdioSyncMode_e SyncMode, unsigned int eBlockMode)

{

	unsigned long  addr, buflen=4, j;

	u8 *value;

    unsigned long len;



	if (( value = kmalloc(BUFLEN*2,__GFP_DMA)) == NULL)

	{

	  printk("sdio_test_write: sdio_test_write() kmalloc(%d) FAILED !!!\n",(unsigned int)BUFLEN);

	  return;

	}

	sscanf(buffer, "%X %X %d", (unsigned int *)&addr, (unsigned int *)value, (unsigned int *)&len);



	addr = addr + base;



	if (base != g_mem_partition_size) /* memory */

	{

	  buflen = len;



	  for (j=0; j < buflen*2-1 ; j++)

	  {

	    value[j+1] = value[0]+j+1;

	  }

	}

	if (SyncMode == SdioSync)

	{

	  	sdioDrv_WriteSync(TXN_FUNC_ID_WLAN, addr, (u8*)value, buflen, 1, 1);

	}



	kfree(value);



} /* sdio_test_write() */



int sdioDrv_Test(void)

{

	//unsigned char  uByte;

 //   unsigned long  uLong;

    //unsigned short  uShort;

    //unsigned long  uCount = 0;

    //unsigned int   uBlkSize = 1 << 9;

	int            iStatus;    

	int            ChipID = 0;
	int count = 0;
	int tmp = 0;

	iStatus = sdioDrv_ConnectBus (NULL, NULL, 9, 0);

#if 0
    do

    {

        uByte = SDIO_BITS_CODE;

        iStatus = sdioDrv_WriteSyncBytes (TXN_FUNC_ID_CTRL, CCCR_BUS_INTERFACE_CONTOROL, &uByte, 1, 1);
        if (iStatus) { return iStatus; }



        iStatus = sdioDrv_ReadSyncBytes (TXN_FUNC_ID_CTRL, CCCR_BUS_INTERFACE_CONTOROL, &uByte, 1, 1);
        if (iStatus) { return iStatus; }

        

        iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, 0xC8, &uShort, 2, 1, 1);
        if (iStatus) { return iStatus; }

        uCount++;



    } while ((uByte != SDIO_BITS_CODE) && (uCount < MAX_RETRIES));





    uCount = 0;



    /* allow function 2 */

    do

    {

        uByte = ti_drv.func->num * 2;

        iStatus = sdioDrv_WriteSyncBytes (TXN_FUNC_ID_CTRL, CCCR_IO_ENABLE, &uByte, 1, 1);

        if (iStatus) { return iStatus; }



        iStatus = sdioDrv_ReadSyncBytes (TXN_FUNC_ID_CTRL, CCCR_IO_ENABLE, &uByte, 1, 1);

        if (iStatus) { return iStatus; }

        

        iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, 0xC8, &uShort, 2, 1, 1);

        if (iStatus) { return iStatus; }



        uCount++;



    } while ((uByte != 4) && (uCount < MAX_RETRIES));





#if 1    

    uCount = 0;

    

    /* set block size for SDIO block mode */

    do

    {

        uShort = uBlkSize;

        iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, FN0_FBR2_REG_108, &uShort, 2, 1, 1);

        if (iStatus) { return iStatus; }



        iStatus = sdioDrv_ReadSync (TXN_FUNC_ID_CTRL, FN0_FBR2_REG_108, &uShort, 2, 1, 1);

        if (iStatus) { return iStatus; }

        

        iStatus = sdioDrv_WriteSync (TXN_FUNC_ID_CTRL, 0xC8, &uShort, 2, 1, 1);

        if (iStatus) { return iStatus; }



        uCount++;



    } while (((uShort & FN0_FBR2_REG_108_BIT_MASK) != uBlkSize) && (uCount < MAX_RETRIES));





    if (uCount >= MAX_RETRIES)

    {

        /* Failed to write CMD52_WRITE to function 0 */

		printk("Failed to write CMD52_WRITE to function 0\n"); 

        return (int)uCount;

    }

#endif

#endif

	printk("Running set_partition status = "); 

	iStatus = set_partition(0, g_mem_partition_size, TESTDRV_CODE_RAM_PART_START_ADDR, 

							g_reg_partition_size, TESTDRV_REG_PART_START_ADDR);

	printk("%d %s\n", iStatus, iStatus ? "NOT OK" : "OK"); 



	sdioDrv_ReadSyncBytes(ti_drv.func->num, 0x16800 + 0x5674, (unsigned char *)&(ChipID), 1, 0);

	sdioDrv_ReadSyncBytes(ti_drv.func->num, 0x16800 + 0x5675, (unsigned char *)&(ChipID)+1, 1, 0);

	sdioDrv_ReadSyncBytes(ti_drv.func->num, 0x16800 + 0x5676, (unsigned char *)&(ChipID)+2, 1, 0);

	sdioDrv_ReadSyncBytes(ti_drv.func->num, 0x16800 + 0x5677, (unsigned char *)&(ChipID)+3, 1, 0);

  	printk("Read device ID via ReadByte: 0x%x\n", ChipID);

	mdelay(10);

	sdioDrv_ReadSync(ti_drv.func->num, 0x16800 + 0x5674, (void *)&(ChipID), 4, 1, 0);

  	printk("Read device ID via ReadSync: 0x%x\n", ChipID);

  	

    /* for 1273 only init sequence in order to get access to the memory */

    {

		unsigned long base;

    	char   *cmd = "wr 6040 3";



    	base = sdio_test_get_base(cmd[1]);

    	sdio_test_write(&cmd[2], base, SdioSync, 0);

    	cmd = "wr 6100 4";

    	sdio_test_write(&cmd[2], base, SdioSync, 0);



		mdelay(10);

  		init_buffers();

	//while(1){

		//sdio_test_once_sync_complete();

		//}

		//TESTDRV_FUNC0_READ_TEST

  	//do_test_sync(TESTDRV_READ_TEST | TESTDRV_WRITE_TEST | TESTDRV_COMPARE_TEST | TESTDRV_FUNC0_TEST);
#if 0
  		while(1){
			printk("Running set_partition status = "); 

			iStatus = set_partition(0, g_mem_partition_size, TESTDRV_CODE_RAM_PART_START_ADDR, 
							g_reg_partition_size, TESTDRV_REG_PART_START_ADDR);

			printk("%d %s\n", iStatus, iStatus ? "NOT OK" : "OK"); 

			do_test_sync(TESTDRV_COMPARE_TEST);

		}
#endif
		for (count =0 ; count <=10; count++)
		{
			printk("Running CMD52 1fffc = "); 
			iStatus = sdioDrv_WriteSyncBytes(ti_drv.func->num, 0x1FFFC,(unsigned char *)&(tmp), 1, 0);
			printk("%d %s\n", iStatus, iStatus ? "NOT OK" : "OK"); 
		}
	}

	return iStatus;

}





static const struct sdio_device_id ti_sdio_ids[] = {

	{SDIO_DEVICE_CLASS(0x00)},

	{ /* end: all zeroes */	},

};


static struct sdio_func *wifi_sdio_fuc = NULL;

int wifi_sdio_check_func(void)
{
	if (wifi_sdio_fuc == NULL)
		return 1;

	return 0;
}

static int sdioDrv_probe(struct sdio_func *func,
			 const struct sdio_device_id *id)
{
	int card_unit;
	if (func->num != 2)
		return 0;

	memset(&ti_drv, 0, sizeof(TI_sdiodrv_t));

	ti_drv.func = func;
	ti_drv.max_blocksize = func->max_blksize;
	ti_drv.int_enabled = 1;
	tiwlan_func[SDIO_WLAN_FUNC] = func;
	wifi_sdio_fuc = func;
	
	/* Store our context in the MMC driver */

	//printk(KERN_INFO "ti_sdio_probe: Add glue driver\n");
	sdio_claim_host(func);
	card_unit = func->card->unit_state;
	func->card->card_insert_process(func->card);
	func->card->unit_state = card_unit;
	sdioDrv_SetBlockSize(SDIO_WLAN_FUNC, 512);
	sdio_set_drvdata(func, &ti_drv);
	sdio_enable_func(func);
	sdio_release_host(func);

//	setup_intr();
	sdioDrv_Test();
	return 0;
}


extern void wlanDrvIf_remove(void);
static void sdioDrv_remove(struct sdio_func *func)

{
	TI_sdiodrv_t *fdev = (TI_sdiodrv_t *)sdio_get_drvdata(func);
	struct card_host *host = func->card->host;

	tiwlan_func[SDIO_WLAN_FUNC] = NULL;
	tiwlan_func[SDIO_CTRL_FUNC] = NULL;
	wifi_sdio_fuc = NULL;

	if (func->num != 2)
		return;
	/* If there is a registered device driver, pass on the remove */
	printk(KERN_INFO "ti_sdio_remove: Free IRQ and remove device "
		       "driver\n");
//	wlanDrvIf_remove(); // nathan
	/* Unregister the IRQ handler first. */
//	sdio_claim_host(fdev->func);
//	sdio_release_irq(func);
//	sdio_release_host(fdev->func);
	if (!fdev->int_enabled) {
		fdev->int_enabled = 1;
		host->ops->enable_sdio_irq(host, 1);
	}
	/* Unregister the card context from the MMC driver. */
//	sdio_set_drvdata(func, NULL);
}



static struct sdio_driver sdio_ti_driver = {
	.name = "sdioDrv",
	.probe = sdioDrv_probe,
	.remove = sdioDrv_remove,
	.id_table = ti_sdio_ids,
};


//#include <linux/sn7325.h>
/* Module init and exit, register and unregister to the SDIO/MMC driver */
//static int __init sdioDrv_init(void);
//static int __init sdioDrv_init(void)
int sdioDrv_init(void)
{
	int err;
	
	SDIO_LOCKED_FLAG = 0;	
	//printk("SDIO_LOCKED_FLAG = %d , ---%s--- !!\n",SDIO_LOCKED_FLAG,__func__);
	//configIO(0, 0);
	//printk("%s high\n", __FUNCTION__);
    //setIO_level(0, 1, 5);

	printk(KERN_INFO "***SDIO***:Texas Instruments: Register to MMC/SDIO driver v%s\n",SW_VERSION_STR);
	/* Sleep a bit - otherwise if the mmc subsystem has just started, it
	 * will allow us to register, then immediatly remove us!
	 */

	err = sdio_register_driver(&sdio_ti_driver);
	if (err) {
		printk(KERN_ERR "Error: register sdio_ti_driver failed!\n");
	}else{
		printk(KERN_INFO "register sdio_ti_driver is OK!\n");
	}

	return err;
}



//static void __exit sdioDrv_exit(void)
void sdioDrv_exit(void)

{
	printk(KERN_INFO "***SDIO***:Texas Instruments: Unregister from MMC/SDIO driver\n");
	sdio_unregister_driver(&sdio_ti_driver);
}

#if 0

EXPORT_SYMBOL(sdioDrv_ConnectBus);

EXPORT_SYMBOL(sdioDrv_WriteSyncBytes);

EXPORT_SYMBOL(sdioDrv_ReadSyncBytes);

EXPORT_SYMBOL(sdioDrv_ReadAsync);

EXPORT_SYMBOL(sdioDrv_WriteAsync);

EXPORT_SYMBOL(sdioDrv_ReadSync);

EXPORT_SYMBOL(sdioDrv_WriteSync);

EXPORT_SYMBOL(sdioDrv_DisconnectBus);

EXPORT_SYMBOL(sdioDrv_EnableFunction);

EXPORT_SYMBOL(sdioDrv_EnableInterrupt);

EXPORT_SYMBOL(sdioDrv_DisableFunction);

EXPORT_SYMBOL(sdioDrv_DisableInterrupt);

EXPORT_SYMBOL(sdioDrv_SetBlockSize);

//EXPORT_SYMBOL(sdioDrv_Register_Notification);

EXPORT_SYMBOL(sdioDrv_ReleaseHost);

EXPORT_SYMBOL(sdioDrv_ClaimHost);


module_init(sdioDrv_init);

module_exit(sdioDrv_exit);

#endif

MODULE_DESCRIPTION("TI SDIO Link driver");

MODULE_AUTHOR("Jorjin Corp.");

MODULE_LICENSE("GPL");
