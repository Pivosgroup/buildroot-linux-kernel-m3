/*******************************************************************
 * 
 *  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
 *
 *  Description: 
 *
 *  Author: Herbert.hu 
 *  Created: 07/26 2011
 *
 *******************************************************************/

#include "log_stream.h"
#include <linux/amports/dsp_register.h>
#include <linux/dma-mapping.h>

typedef struct
{
    unsigned long wpointer;        /* write pointer */
    unsigned long rpointer;        /* read pointer */
    unsigned long size;            /* size of buffer */
    unsigned long start;           /* buffer start addr */
    unsigned long end;             /* buffer end addr */
} log_stream_t;

#ifdef MIN
#undef MIN
#endif

#define MIN(x,y)	(((x)<(y))?(x):(y))
#define MIN_CACHE_LINE(x,y) (MIN(x,y) & (~0x1f))

#define SAMPLE_BUFFER_RESERVED     (32)         

static log_stream_t log_stream = {0};

static inline int internal_read(unsigned char *buf, int size, int intact);

static inline unsigned long get_read_pointer(void);
static inline void set_read_pointer(void);
static inline unsigned long get_write_pointer(void);
static inline void cache_flush(unsigned int addr, unsigned int size);
static inline void cache_invalidate(unsigned int addr, unsigned int size);

static inline unsigned long get_read_pointer(void)
{
	return ARC_2_ARM_ADDR_SWAP(DSP_RD(DSP_LOG_RD_ADDR));
}

static inline void set_read_pointer(void)
{
	DSP_WD(DSP_LOG_RD_ADDR, ARM_2_ARC_ADDR_SWAP(log_stream.rpointer));
}

static inline unsigned long get_write_pointer(void)
{
	return ARC_2_ARM_ADDR_SWAP(DSP_RD(DSP_LOG_WD_ADDR));
}

static inline void cache_flush(unsigned int addr, unsigned int size)
{
    //AV_dcache_flush_range(addr, addr + size);
    dma_addr_t buf_map;
    buf_map = dma_map_single(NULL, (void *)addr, size, DMA_FROM_DEVICE);
    dma_unmap_single(NULL, buf_map, size, DMA_FROM_DEVICE);
}

static inline void cache_invalidate(unsigned int addr, unsigned int size)
{
    //AV_dcache_inv_range(addr, addr + size);
    dma_addr_t buf_map;
    buf_map = dma_map_single(NULL, (void *)addr, size, DMA_FROM_DEVICE);
    dma_unmap_single(NULL, buf_map, size, DMA_FROM_DEVICE);
}

int log_stream_space(void)
{
    int space;

    log_stream.rpointer = get_read_pointer(); 
    log_stream.wpointer = get_write_pointer(); 

    if(log_stream.wpointer >= log_stream.rpointer){
        space =  log_stream.size - (log_stream.wpointer - log_stream.rpointer);
    }else{
        space =  log_stream.rpointer - log_stream.wpointer;
    }

    space = space > SAMPLE_BUFFER_RESERVED ? space - SAMPLE_BUFFER_RESERVED: 0;

    return space;
}

int log_stream_content(void)
{
    int content;

    log_stream.rpointer = get_read_pointer(); 
    log_stream.wpointer = get_write_pointer(); 

    if(log_stream.rpointer > log_stream.wpointer){
        content =  log_stream.size - (log_stream.rpointer - log_stream.wpointer);
    }else{
        content =  log_stream.wpointer - log_stream.rpointer;
    }

    content = content > SAMPLE_BUFFER_RESERVED ? content - SAMPLE_BUFFER_RESERVED: 0;

    return content;
}

static inline int internal_read(unsigned char *buf, int size, int intact)
{
    int len;
    int content = log_stream_content();
    int tail = 0;

    //printf("[internal_read] content: %d, size: %d, intact:%d\n", content, size, intact);

    if(intact){
        len = size > content ? -1 : size;
    }else{
        len = size <=32 ? MIN(size, content) : MIN_CACHE_LINE(size, content);
    }

    if(len <= 0){
        return 0;
    }

    if(log_stream.rpointer + len > log_stream.end){

        tail = log_stream.end - log_stream.rpointer;
        cache_invalidate(log_stream.rpointer, tail);
        memcpy((void *)buf, (void *)(log_stream.rpointer), tail);

        log_stream.rpointer = log_stream.start;
        cache_invalidate(log_stream.rpointer, len - tail);
        memcpy((void *)(buf + tail), (void *)(log_stream.rpointer), len - tail);
        log_stream.rpointer += len - tail;

    }else{
        cache_invalidate(log_stream.rpointer, len);
        memcpy((void *)buf, (void *)(log_stream.rpointer), len);
        log_stream.rpointer += len;
    }

    set_read_pointer();

    return len;
}


int log_stream_read(unsigned char *buf, int size)
{
    if(buf == 0 || log_stream.start == 0){
        return 0;
    }

    return internal_read(buf, size, 0);
}

int log_stream_init(void)
{
	log_stream.start = ARC_2_ARM_ADDR_SWAP(DSP_RD(DSP_LOG_START_ADDR));
	log_stream.end = ARC_2_ARM_ADDR_SWAP(DSP_RD(DSP_LOG_END_ADDR));
	log_stream.size = log_stream.end - log_stream.start;
	return 0;
}

int log_stream_deinit(void)
{
	log_stream.start = 0;
	log_stream.end = 0;
	log_stream.size = 0; 
	return 0;
}
