#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#include <linux/timer.h>

#include <asm/cacheflush.h>
//#include <asm/arch/am_regs.h>
#include <mach/am_regs.h>

#include <linux/amports/tsync.h>
#include <linux/amports/timestamp.h>
#include "dsp_mailbox.h"
#include "dsp_codec.h"
extern set_pcminfo_data(void * pcm_encoded_info);
static void audiodsp_mailbox_work_queue(struct work_struct*);
static struct audiodsp_work_t{
char* buf;
struct work_struct audiodsp_workqueue;
}audiodsp_work;

extern unsigned int IEC958_bpf;
extern unsigned int IEC958_brst;
extern unsigned int IEC958_length;
extern unsigned int IEC958_padsize;
extern unsigned int IEC958_mode;
extern unsigned int IEC958_syncword1;
extern unsigned int IEC958_syncword2;
extern unsigned int IEC958_syncword3;
extern unsigned int IEC958_syncword1_mask;
extern unsigned int IEC958_syncword2_mask;
extern unsigned int IEC958_syncword3_mask;
extern unsigned int IEC958_chstat0_l;
extern unsigned int IEC958_chstat0_r;
extern unsigned int IEC958_chstat1_l;
extern unsigned int IEC958_chstat1_r;
extern unsigned int IEC958_mode_raw;
extern unsigned int IEC958_mode_codec;

int dsp_mailbox_send(struct audiodsp_priv *priv,int overwrite,int num,int cmd,const char *data,int len)
{
	unsigned long flags;
	int res=-1;
	struct mail_msg *m;
    	dma_addr_t buf_map;

	m=&priv->mailbox_reg2[num];

	local_irq_save(flags);
	if(overwrite || m->status==0)
	{
		
		m->cmd=cmd;
		m->data=(char *)ARM_2_ARC_ADDR_SWAP((unsigned)data);
		m->len=len;
		m->status=1;
		after_change_mailbox(m);
		if(data!=NULL && len >0)
		{
			buf_map = dma_map_single(NULL, (void *)data, len, DMA_TO_DEVICE);
			dma_unmap_single(NULL, buf_map, len, DMA_TO_DEVICE);
    		}
		MAIBOX2_IRQ_ENABLE(num);
		DSP_TRIGGER_IRQ(num);
		res=0;
	}
	local_irq_restore(flags);
	return res;
}


int get_mailbox_data(struct audiodsp_priv *priv,int num,struct mail_msg *msg)
{
	unsigned long flags;
    	dma_addr_t buf_map;	
	struct mail_msg *m;
	if(num>31 || num <0)
			return -1;
	local_irq_save(flags);
	m=&priv->mailbox_reg[num];
	pre_read_mailbox(m);
    //dsp_addr_map = dma_map_single(priv->dev,(void*)m,sizeof(*m),DMA_FROM_DEVICE);
    //dma_unmap_single(priv->dev,dsp_addr_map,sizeof(*m),DMA_FROM_DEVICE);
	msg->cmd=m->cmd; 
	msg->data=m->data;
       msg->data = (char *)((unsigned)msg->data+AUDIO_DSP_START_ADDR);
	msg->status=m->status;
	msg->len=m->len;
	if(msg->len && msg->data != NULL){
	    buf_map = dma_map_single(priv->dev,(void*)msg->data ,msg->len,DMA_FROM_DEVICE);
	    dma_unmap_single(priv->dev,buf_map,msg->len,DMA_FROM_DEVICE);
	}
	m->status=0;
	after_change_mailbox(m);
	local_irq_restore(flags);
	return 0;
}

static irqreturn_t audiodsp_mailbox_irq(int irq, void *data)
{
	struct audiodsp_priv *priv=(struct audiodsp_priv *)data;
	unsigned long status,fiq_mask;
	struct mail_msg msg;
	int i = 0;
	status=READ_MPEG_REG(ASSIST_MBOX1_IRQ_REG); 
	fiq_mask=READ_MPEG_REG(ASSIST_MBOX1_FIQ_SEL); 
	status=status&fiq_mask;
	if(status&(1<<M1B_IRQ0_PRINT))
		{
		get_mailbox_data(priv,M1B_IRQ0_PRINT,&msg);
		SYS_CLEAR_IRQ(M1B_IRQ0_PRINT);
	//	inv_dcache_range((unsigned  long )msg.data,(unsigned long)msg.data+msg.len);
	
	    audiodsp_work.buf = msg.data;
        schedule_work(&audiodsp_work.audiodsp_workqueue);		
		}
	if(status&(1<<M1B_IRQ1_BUF_OVERFLOW))
		{
		SYS_CLEAR_IRQ(M1B_IRQ1_BUF_OVERFLOW);
		DSP_PRNT("DSP BUF over flow\n");
		}
	if(status&(1<<M1B_IRQ2_BUF_UNDERFLOW))
		{
		SYS_CLEAR_IRQ(M1B_IRQ2_BUF_UNDERFLOW);
		DSP_PRNT("DSP BUF over flow\n");
		}
	if(status&(1<<M1B_IRQ3_DECODE_ERROR))
		{
		SYS_CLEAR_IRQ(M1B_IRQ3_DECODE_ERROR);
		priv->decode_error_count++;
		}
	if(status&(1<<M1B_IRQ4_DECODE_FINISH_FRAME))
		{
		struct frame_info *info;
		SYS_CLEAR_IRQ(M1B_IRQ4_DECODE_FINISH_FRAME);
		get_mailbox_data(priv,M1B_IRQ4_DECODE_FINISH_FRAME,&msg);
		info=(struct frame_info *)msg.data;
		if(info!=NULL)
			{
			priv->cur_frame_info.offset=info->offset;
			priv->cur_frame_info.buffered_len=info->buffered_len;
			}
		priv->decoded_nb_frames ++;		
		complete(&priv->decode_completion);
		}
	if(status& (1<<M1B_IRQ5_STREAM_FMT_CHANGED))
		{
		struct frame_fmt *fmt;
		SYS_CLEAR_IRQ(M1B_IRQ5_STREAM_FMT_CHANGED);
		get_mailbox_data(priv,M1B_IRQ5_STREAM_FMT_CHANGED,&msg);
		fmt=(void *)msg.data;
		//DSP_PRNT("frame format changed");
		if(fmt==NULL || (sizeof(struct frame_fmt )<msg.len))
			{
			DSP_PRNT("frame format message error\n");
			}
		else
			{
			DSP_PRNT("frame format changed,fmt->valid 0x%x\n",fmt->valid);
			if(fmt->valid&SUB_FMT_VALID)
				{
				priv->frame_format.sub_fmt=fmt->sub_fmt;
				priv->frame_format.valid|=SUB_FMT_VALID;
				}
			if(fmt->valid&CHANNEL_VALID)
				{
				priv->frame_format.channel_num=((fmt->channel_num > 2) ? 2 : (fmt->channel_num));
				priv->frame_format.valid|=CHANNEL_VALID;
				}
			if(fmt->valid&SAMPLE_RATE_VALID)
				{
				priv->frame_format.sample_rate=fmt->sample_rate;
				priv->frame_format.valid|=SAMPLE_RATE_VALID;
				}
			if(fmt->valid&DATA_WIDTH_VALID)
				{
				priv->frame_format.data_width=fmt->data_width;
				priv->frame_format.valid|=DATA_WIDTH_VALID;
				}
			}
			if(fmt->data.pcm_encoded_info){
				set_pcminfo_data(fmt->data.pcm_encoded_info);
			}
		}
        if(status & (1<<M1B_IRQ8_IEC958_INFO)){
            struct digit_raw_output_info* info;
            SYS_CLEAR_IRQ(M1B_IRQ8_IEC958_INFO);
            get_mailbox_data(priv, M1B_IRQ8_IEC958_INFO, &msg);
            info = (void*)msg.data;

            IEC958_bpf = info->bpf;
            IEC958_brst = info->brst;
            IEC958_length = info->length;
            IEC958_padsize = info->padsize;
            IEC958_mode = info->mode;
            IEC958_syncword1 = info->syncword1;
            IEC958_syncword2 = info->syncword2;
            IEC958_syncword3 = info->syncword3;
            IEC958_syncword1_mask = info->syncword1_mask;
            IEC958_syncword2_mask = info->syncword2_mask;
            IEC958_syncword3_mask = info->syncword3_mask;
            IEC958_chstat0_l = info->chstat0_l;
            IEC958_chstat0_r = info->chstat0_r;
            IEC958_chstat1_l = info->chstat1_l;
            IEC958_chstat1_r = info->chstat1_r;
  //          IEC958_mode_codec = info->can_bypass;
            
            strcpy(audiodsp_work.buf, "MAILBOX: got IEC958 info\n");
            schedule_work(&audiodsp_work.audiodsp_workqueue);		
        }

    	if(status& (1<<M1B_IRQ5_STREAM_RD_WD_TEST)){
            DSP_WD((0x84100000-4096+20*20),0);
    		SYS_CLEAR_IRQ(M1B_IRQ5_STREAM_RD_WD_TEST);
    		get_mailbox_data(priv,M1B_IRQ5_STREAM_RD_WD_TEST,&msg);
            
            for(i = 0;i<12;i++){
                if((DSP_RD((0x84100000-512*1024+i*20)))!= (0xff00|i)){
                    DSP_PRNT("a9 read dsp reg error ,now 0x%lx, should be 0x%x \n",(DSP_RD((0x84100000-512*1024+i*20))),12-i);
                }
               // DSP_PRNT("A9 audio dsp reg%d value 0x%x\n",i,DSP_RD((0x84100000-4096+i*20)));
            }
            for(i = 0;i<12;i++){
                DSP_WD((0x84100000-512*1024+i*20),(i%2)?i:(0xf0|i));
               
            }
            DSP_WD((0x84100000-512*1024+20*20),DSP_STATUS_HALT);
        //    DSP_PRNT("A9 mail box handle finished\n");
           // dsp_mailbox_send(priv, 1, M1B_IRQ5_STREAM_RD_WD_TEST, 0, NULL,0);

        }

	if(status & (1<<M1B_IRQ7_DECODE_FATAL_ERR)){
		int err_code;
		
		SYS_CLEAR_IRQ(M1B_IRQ7_DECODE_FATAL_ERR);
		get_mailbox_data(priv,M1B_IRQ7_DECODE_FATAL_ERR,&msg);

		err_code = msg.cmd;
		priv->decode_fatal_err = err_code;

		if(err_code & 0x01){
			timestamp_pcrscr_set(timestamp_vpts_get());
			timestamp_pcrscr_enable(1);
		}
		else if(err_code & 0x02){
		printk("Set decode_fatal_err flag, Reset audiodsp!\n");
		}
	}

	return 0;
}
static void audiodsp_mailbox_work_queue(struct work_struct*work)
{
    struct audiodsp_work_t* pwork = container_of(work,struct audiodsp_work_t, audiodsp_workqueue);
    char* message;
    if(pwork){
      message = pwork->buf;
      if(message)
        printk(KERN_INFO "%s",message);
      else
        printk(KERN_INFO "the message from mailbox is NULL\n");
    }else{
      printk(KERN_INFO "the work queue for audiodsp mailbox is empty\n");
    }
}

int audiodsp_init_mailbox(struct audiodsp_priv *priv)
{
    int err;
	err = request_irq(INT_MAILBOX_1B, audiodsp_mailbox_irq,
                    IRQF_SHARED, "audiodsp_mailbox", (void *)priv);
    if(err != 0){
      printk("request IRQ for dsp mailbox failed: errno = %x\n", err);
      return -1;
    }
	//WRITE_MPEG_REG(ASSIST_MBOX0_MASK, 0xffffffff);
	priv->mailbox_reg=(struct mail_msg *)MAILBOX1_REG(0);
	priv->mailbox_reg2=(struct mail_msg *)MAILBOX2_REG(0);
	
	INIT_WORK(&audiodsp_work.audiodsp_workqueue, audiodsp_mailbox_work_queue);
	
	return 0;
}
int audiodsp_release_mailbox(struct audiodsp_priv *priv)
{
	free_irq(INT_MAILBOX_1B,(void *)priv);
    return 0;
}
int  mailbox_send_audiodsp(int overwrite,int num,int cmd,const char *data,int len)
{
	int res = -1;
	res = dsp_mailbox_send(audiodsp_privdata(),overwrite,num,cmd,data,len);
	return res;
}
EXPORT_SYMBOL(mailbox_send_audiodsp);



