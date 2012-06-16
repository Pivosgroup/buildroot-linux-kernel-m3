#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
//include <asm/arch/am_regs.h>
#include <mach/am_regs.h>
//#include <asm/bsp.h>
#include <linux/dma-mapping.h>

#include "dsp_microcode.h"
#include "audiodsp_module.h"
#include "dsp_control.h"


//#include <asm/dsp/dsp_register.h>
#include <linux/amports/dsp_register.h>


#include "dsp_mailbox.h"
#include <linux/delay.h>
#include <linux/clk.h>

#define MIN_CACHE_ALIGN(x)	(((x-4)&(~0x1f)))
#define MAX_CACHE_ALIGN(x)	((x+0x1f)&(~0x1f))

extern unsigned IEC958_mode_raw;

static int decopt = 0x0000ffff;

#define RESET_AUD_ARC	(1<<13)
static void	enable_dsp(int flag)
{	
	struct clk *clk;
	int xtal = 0;

	/* RESET DSP */

	 if(!flag)
	  	 CLEAR_MPEG_REG_MASK(MEDIA_CPU_CTL, 1);
	/*write more for make the dsp is realy reset!*/
	 SET_MPEG_REG_MASK(RESET2_REGISTER, RESET_AUD_ARC);
	// M1 has this bug also????
	// SET_MPEG_REG_MASK(RESET2_REGISTER, RESET_AUD_ARC);
	 //SET_MPEG_REG_MASK(RESET2_REGISTER, RESET_AUD_ARC);
	 
    	/* Enable BVCI low 16MB address mapping to DDR */
	
//    	SET_MPEG_REG_MASK(AUD_ARC_CTRL, (1<<DDR_CTL_MAPDDR));
    	/* polling highest bit of IREG_DDR_CTRL until the mapping is done */
	
        if (flag) {
		    SET_MPEG_REG_MASK(MEDIA_CPU_CTL, 1);
		    CLEAR_MPEG_REG_MASK(MEDIA_CPU_CTL, 1);
		    clk=clk_get_sys("a9_clk", NULL);
		    if(!clk)
			{
				printk(KERN_ERR "can't find clk %s for a9_clk SETTING!\n\n","clk_xtal");
			}
			else
			{
				xtal=clk_get_rate(clk);				
			}
		    DSP_WD(DSP_ARM_REF_CLK_VAL, xtal);
	}
}

void halt_dsp( struct audiodsp_priv *priv)
{
	if(DSP_RD(DSP_STATUS)==DSP_STATUS_RUNING)
		{
#ifndef AUDIODSP_RESET
	  int i;
	  dsp_mailbox_send(priv,1,M2B_IRQ0_DSP_SLEEP,0,0,0);
        for(i = 0; i< 100;i++)
            {
                if(DSP_RD(DSP_STATUS)== DSP_STATUS_SLEEP)
                    break;
		        msleep(1);/*waiting arc2 sleep*/
            }
        if(i == 100)
           DSP_PRNT("warning: dsp is not sleeping when call dsp_stop\n"); 
#else
		dsp_mailbox_send(priv,1,M2B_IRQ0_DSP_HALT,0,0,0);
		msleep(1);/*waiting arc2 sleep*/
#endif
       }
#ifdef AUDIODSP_RESET	
	if(DSP_RD(DSP_STATUS)!=DSP_STATUS_RUNING)
		{
		DSP_WD(DSP_STATUS, DSP_STATUS_HALT);
		return ;
		}
#endif
    if(!priv->dsp_is_started){

	    enable_dsp(0);/*hardware halt the cpu*/
           DSP_WD(DSP_STATUS, DSP_STATUS_HALT);
          priv->last_stream_fmt=-1;/*mask the stream format is not valid*/
    }   
    else
        DSP_WD(DSP_STATUS, DSP_STATUS_SLEEP);
	
}
void reset_dsp( struct audiodsp_priv *priv)
{
    halt_dsp(priv);
    //flush_and_inv_dcache_all();
    /* map DSP 0 address so that reset vector points to same vector table as ARC1 */
    CLEAR_MPEG_REG_MASK(MEDIA_CPU_CTL, (0xfff << 4));
 //   SET_MPEG_REG_MASK(SDRAM_CTL0,1);//arc mapping to ddr memory
    SET_MPEG_REG_MASK(MEDIA_CPU_CTL, ((AUDIO_DSP_START_PHY_ADDR)>> 20) << 4);
// decode option    
    DSP_WD(DSP_DECODE_OPTION, decopt|(IEC958_mode_raw<<31));
    printk("reset dsp : dec opt=%x\n", DSP_RD(DSP_DECODE_OPTION));
    if(!priv->dsp_is_started){
        DSP_PRNT("dsp reset now\n");
        enable_dsp(1);
        }
    else{
       	dsp_mailbox_send(priv,1,M2B_IRQ0_DSP_WAKEUP,0,0,0);
        DSP_WD(DSP_STATUS, DSP_STATUS_WAKEUP);
        msleep(1);/*waiting arc625 run again */

    }

    return;    
}
static inline int dsp_set_stack( struct audiodsp_priv *priv)
{
      dma_addr_t buf_map;
	if(priv->dsp_stack_start==0)
		priv->dsp_stack_start=(unsigned long)kmalloc(priv->dsp_stack_size,GFP_KERNEL);
	if(priv->dsp_stack_start==0)
		{
		DSP_PRNT("kmalloc error,no memory for audio dsp stack\n");
		return -ENOMEM;
		}
	memset((void*)priv->dsp_stack_start,0,priv->dsp_stack_size);
        buf_map = dma_map_single(NULL, (void *)priv->dsp_stack_start, priv->dsp_stack_size, DMA_FROM_DEVICE);
	 dma_unmap_single(NULL, buf_map,  priv->dsp_stack_size, DMA_FROM_DEVICE);

	DSP_WD(DSP_STACK_START,MAX_CACHE_ALIGN(ARM_2_ARC_ADDR_SWAP(priv->dsp_stack_start)));
	DSP_WD(DSP_STACK_END,MIN_CACHE_ALIGN(ARM_2_ARC_ADDR_SWAP(priv->dsp_stack_start)+priv->dsp_stack_size));
	DSP_PRNT("DSP statck start =%#lx,size=%#lx\n",ARM_2_ARC_ADDR_SWAP(priv->dsp_stack_start),priv->dsp_stack_size);
	if(priv->dsp_gstack_start==0)
		priv->dsp_gstack_start=(unsigned long)kmalloc(priv->dsp_gstack_size,GFP_KERNEL);
	if(priv->dsp_gstack_start==0)
		{
		DSP_PRNT("kmalloc error,no memory for audio dsp gp stack\n");
		kfree((void *)priv->dsp_stack_start);
		return -ENOMEM;
		}
	memset((void*)priv->dsp_gstack_start,0,priv->dsp_gstack_size);
        buf_map = dma_map_single(NULL, (void *)priv->dsp_gstack_start, priv->dsp_gstack_size, DMA_FROM_DEVICE);
	 dma_unmap_single(NULL, buf_map,  priv->dsp_gstack_size, DMA_FROM_DEVICE);
	DSP_WD(DSP_GP_STACK_START,MAX_CACHE_ALIGN(ARM_2_ARC_ADDR_SWAP(priv->dsp_gstack_start)));
	DSP_WD(DSP_GP_STACK_END,MIN_CACHE_ALIGN(ARM_2_ARC_ADDR_SWAP(priv->dsp_gstack_start)+priv->dsp_gstack_size));
	DSP_PRNT("DSP gp statck start =%#lx,size=%#lx\n",ARM_2_ARC_ADDR_SWAP(priv->dsp_gstack_start),priv->dsp_gstack_size);
		
	return 0;
}
static inline int dsp_set_heap( struct audiodsp_priv *priv)
{
      dma_addr_t buf_map;
	if(priv->dsp_heap_size==0)
		return 0;
	if(priv->dsp_heap_start==0)
		priv->dsp_heap_start=(unsigned long)kmalloc(priv->dsp_heap_size,GFP_KERNEL);
	if(priv->dsp_heap_start==0)
		{
		DSP_PRNT("kmalloc error,no memory for audio dsp dsp_set_heap\n");
		return -ENOMEM;
		}
	memset((void *)priv->dsp_heap_start,0,priv->dsp_heap_size);
       buf_map = dma_map_single(NULL, (void *)priv->dsp_heap_start, priv->dsp_heap_size, DMA_FROM_DEVICE);
	dma_unmap_single(NULL, buf_map,  priv->dsp_heap_size, DMA_FROM_DEVICE);
	DSP_WD(DSP_MEM_START,MAX_CACHE_ALIGN(ARM_2_ARC_ADDR_SWAP(priv->dsp_heap_start)));
	DSP_WD(DSP_MEM_END,MIN_CACHE_ALIGN(ARM_2_ARC_ADDR_SWAP(priv->dsp_heap_start)+priv->dsp_heap_size));
	DSP_PRNT("DSP heap start =%#lx,size=%#lx\n",ARM_2_ARC_ADDR_SWAP(priv->dsp_heap_start),priv->dsp_heap_size);
	return 0;
}

static inline int dsp_set_stream_buffer( struct audiodsp_priv *priv)
{
      dma_addr_t buf_map;
	if(priv->stream_buffer_mem_size==0)
		{
		DSP_WD(DSP_DECODE_OUT_START_ADDR,0);
		DSP_WD(DSP_DECODE_OUT_END_ADDR,0);
		DSP_WD(DSP_DECODE_OUT_RD_ADDR,0);
		DSP_WD(DSP_DECODE_OUT_WD_ADDR,0);
		return 0;
		}
	if(priv->stream_buffer_mem==NULL)
		priv->stream_buffer_mem=(void*)kmalloc(priv->stream_buffer_mem_size,GFP_KERNEL);
	if(priv->stream_buffer_mem==NULL)
		{
		DSP_PRNT("kmalloc error,no memory for audio dsp stream buffer\n");
		return -ENOMEM;
		}
	memset((void *)priv->stream_buffer_mem,0,priv->stream_buffer_mem_size);
    	buf_map = dma_map_single(NULL, (void *)priv->stream_buffer_mem, priv->stream_buffer_mem_size, DMA_FROM_DEVICE);
	dma_unmap_single(NULL, buf_map,  priv->stream_buffer_mem_size, DMA_FROM_DEVICE);

	priv->stream_buffer_start=MAX_CACHE_ALIGN((unsigned long)priv->stream_buffer_mem);
	priv->stream_buffer_end=MIN_CACHE_ALIGN((unsigned long)priv->stream_buffer_mem+priv->stream_buffer_mem_size);
	priv->stream_buffer_size=priv->stream_buffer_end-priv->stream_buffer_start;
	if(priv->stream_buffer_size<0)
		{
		DSP_PRNT("Stream buffer set error,must more larger,mensize=%d,buffer size=%ld\n",
			priv->stream_buffer_mem_size,priv->stream_buffer_size
			);
		kfree(priv->stream_buffer_mem);
		priv->stream_buffer_mem=NULL;
		return -2;
		}
		
	DSP_WD(DSP_DECODE_OUT_START_ADDR,ARM_2_ARC_ADDR_SWAP(priv->stream_buffer_start));
	DSP_WD(DSP_DECODE_OUT_END_ADDR,ARM_2_ARC_ADDR_SWAP(priv->stream_buffer_end));
	DSP_WD(DSP_DECODE_OUT_RD_ADDR,ARM_2_ARC_ADDR_SWAP(priv->stream_buffer_start));
	DSP_WD(DSP_DECODE_OUT_WD_ADDR,ARM_2_ARC_ADDR_SWAP(priv->stream_buffer_start));
	
	DSP_PRNT("DSP stream buffer to [%#lx-%#lx]\n",ARM_2_ARC_ADDR_SWAP(priv->stream_buffer_start),ARM_2_ARC_ADDR_SWAP(priv->stream_buffer_end));
	return 0;
}


 int dsp_start( struct audiodsp_priv *priv, struct audiodsp_microcode *mcode)
 {
	int i;
	int res;
	mutex_lock(&priv->dsp_mutex);		
	halt_dsp(priv);
	if(priv->stream_fmt!=priv->last_stream_fmt) // remove the trick, bug fixed on dsp side
		{
		if(audiodsp_microcode_load(audiodsp_privdata(),mcode)!=0)
			{
			printk("load microcode error\n");
			res=-1;
			goto exit;
			}
		priv->last_stream_fmt=priv->stream_fmt;
		}
	if((res=dsp_set_stack(priv)))
		goto exit;
	if((res=dsp_set_heap(priv)))
		goto exit;
	if((res=dsp_set_stream_buffer(priv)))
		goto exit;
    if(!priv->dsp_is_started)
	    reset_dsp(priv);
    else{
        dsp_mailbox_send(priv,1,M2B_IRQ0_DSP_WAKEUP,0,0,0);
        msleep(1);/*waiting arc625 run again */
    }    
	priv->dsp_start_time=jiffies;
    
	for(i=0;i<1000;i++)
		{            
		if(DSP_RD(DSP_STATUS)==DSP_STATUS_RUNING)
			break;
		msleep(1);
		}
	if(i>=1000)
		{
		DSP_PRNT("dsp not running \n");
		res=-1;
		}
	else
		{
		DSP_PRNT("dsp status=%lx\n",DSP_RD(DSP_STATUS));
		priv->dsp_is_started=1;
		res=0;
		}
exit:
	mutex_unlock(&priv->dsp_mutex);		
	return res;
 }

 int dsp_stop( struct audiodsp_priv *priv)
 	{
 	mutex_lock(&priv->dsp_mutex);		
#ifdef AUDIODSP_RESET	
 	priv->dsp_is_started=0;
#endif
 	halt_dsp(priv);
	priv->dsp_end_time=jiffies;
#if 0	
	if(priv->dsp_stack_start!=0)
		kfree((void*)priv->dsp_stack_start);
	priv->dsp_stack_start=0;
	if(priv->dsp_gstack_start!=0)
		kfree((void*)priv->dsp_gstack_start);
	priv->dsp_gstack_start=0;
	if(priv->dsp_heap_start!=0)
		kfree((void*)priv->dsp_heap_start);
	priv->dsp_heap_start=0;
	if(priv->stream_buffer_mem!=NULL)
		{
		kfree(priv->stream_buffer_mem);
		priv->stream_buffer_mem=NULL;		
		}
	#endif
	mutex_unlock(&priv->dsp_mutex);	
	return 0;
 	}

static  int __init decode_option_setup(char *s)
{
    unsigned long value = 0xffffffffUL;
    if(strict_strtoul(s, 16, &value)){
      decopt = 0x0000ffff;
      return -1;
    }
    decopt = (int)value;
    return 0;
}
__setup("decopt=",decode_option_setup) ;

