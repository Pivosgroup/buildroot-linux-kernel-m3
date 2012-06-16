#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
//#include <asm/arch/am_regs.h>
#include <linux/major.h>
#include <linux/slab.h>

//#include <asm/dsp/audiodsp_control.h>
#include "audiodsp_control.h"	// temp here

#include <asm/uaccess.h>
#include <linux/amports/amstream.h>

#include <mach/am_regs.h>

#include "audiodsp_module.h"
#include "dsp_control.h"
#include "dsp_microcode.h"
#include "dsp_mailbox.h"
#include "dsp_monitor.h"

#include "dsp_codec.h"
#include <linux/dma-mapping.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/timestamp.h>
#include <linux/amports/tsync.h>

extern void tsync_pcr_recover(void);extern void tsync_pcr_recover(void);

MODULE_DESCRIPTION("AMLOGIC APOLLO Audio dsp driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhou Zhi <zhi.zhou@amlogic.com>");
MODULE_VERSION("1.0.0");

extern unsigned IEC958_mode_raw;
extern unsigned IEC958_mode_codec;
static int IEC958_mode_raw_last = 0;
static int IEC958_mode_codec_last = 0;
extern struct audio_info * get_audio_info(void);
extern void	aml_alsa_hw_reprepare();
void audiodsp_moniter(unsigned long);
static struct audiodsp_priv *audiodsp_p;
#define  DSP_DRIVER_NAME	"audiodsp"
#define  DSP_NAME	"dsp"

#ifdef CONFIG_PM
typedef struct {
    int event;
    //
}audiodsp_pm_state_t;

static audiodsp_pm_state_t pm_state;

#endif

static void audiodsp_prevent_sleep(void)
{
    struct audiodsp_priv* priv = audiodsp_privdata();
    printk("audiodsp prevent sleep\n");
    wake_lock(&priv->wakelock);
}

static void audiodsp_allow_sleep(void)
{
    struct audiodsp_priv *priv=audiodsp_privdata();
    printk("audiodsp allow sleep\n");
    wake_unlock(&priv->wakelock);
}

int audiodsp_start(void)
{
	struct audiodsp_priv *priv=audiodsp_privdata();
	struct audiodsp_microcode *pmcode;
	struct audio_info *audio_info;
	int ret,i;
	priv->frame_format.valid=0;
	priv->decode_error_count=0;
	priv->last_valid_pts=0;
	priv->out_len_after_last_valid_pts = 0;
	priv->decode_fatal_err = 0;
	priv->first_lookup_over = 0;
	pmcode=audiodsp_find_supoort_mcode(priv,priv->stream_fmt);
	if(pmcode==NULL)
	{
		DSP_PRNT("have not find a valid mcode for fmt(0x%x)\n",priv->stream_fmt);
		return -1;
	}

	stop_audiodsp_monitor(priv);
	dsp_stop(priv);
	ret=dsp_start(priv,pmcode);
	if(ret==0){
 		start_audiodsp_monitor(priv);

#ifdef CONFIG_AM_VDEC_REAL
	if((pmcode->fmt == MCODEC_FMT_COOK) || 
	   (pmcode->fmt == MCODEC_FMT_RAAC) || 
	   (pmcode->fmt == MCODEC_FMT_AMR)  || 
	   (pmcode->fmt == MCODEC_FMT_WMA)  ||
	   (pmcode->fmt == MCODEC_FMT_ADPCM)|| 
	   (pmcode->fmt == MCODEC_FMT_PCM)  ||
	   (pmcode->fmt == MCODEC_FMT_WMAPRO)||
	   (pmcode->fmt == MCODEC_FMT_ALAC)||
	  (pmcode->fmt == MCODEC_FMT_AC3) ||
	  (pmcode->fmt == MCODEC_FMT_APE) ||
	  (pmcode->fmt == MCODEC_FMT_FLAC))

	{
		DSP_PRNT("dsp send audio info\n");
    		for(i = 0; i< 2000;i++){
                if(DSP_RD(DSP_AUDIOINFO_STATUS) == DSP_AUDIOINFO_READY)//maybe at audiodsp side,INT not enabled yet,so wait a while
                    break;
    		     msleep(1);
            }
		if(i == 2000)
			DSP_PRNT("audiodsp not ready for info  \n");
            DSP_WD(DSP_AUDIOINFO_STATUS,0);
		    audio_info = get_audio_info();
		DSP_PRNT("kernel sent info first 4 byte[0x%x],[0x%x],[0x%x],[0x%x]\n\t",audio_info->extradata[0],\
			audio_info->extradata[1],audio_info->extradata[2],audio_info->extradata[3]);
		DSP_WD(DSP_GET_EXTRA_INFO_FINISH, 0);
		while(1){
		    dsp_mailbox_send(priv, 1, M2B_IRQ4_AUDIO_INFO, 0, (const char*)audio_info, sizeof(struct audio_info));
		    msleep(100);

		    if(DSP_RD(DSP_GET_EXTRA_INFO_FINISH) == 0x12345678)
		        break;
		}
    }
#endif
     }
	return ret;
}

static int audiodsp_open(struct inode *node, struct file *file)
{
	DSP_PRNT("dsp_open\n");
	audiodsp_prevent_sleep();
	return 0;

}

static int audiodsp_ioctl(struct inode *node, struct file *file, unsigned int cmd,
		      unsigned long args)
{
	struct audiodsp_priv *priv=audiodsp_privdata();
	struct audiodsp_cmd *a_cmd;
	char name[64];
	int len;
	unsigned long pts;
	int ret=0;
	unsigned long *val=(unsigned long *)args;
#ifdef ENABLE_WAIT_FORMAT
	static int wait_format_times=0;
#endif	
	switch(cmd)
		{
		case AUDIODSP_SET_FMT:
			priv->stream_fmt=args;
            if(args == MCODEC_FMT_DTS)
              	IEC958_mode_codec = 1;
            else if(args == MCODEC_FMT_AC3)
            	IEC958_mode_codec = 2; 	
            else
            	IEC958_mode_codec = 0;
			break;
		case AUDIODSP_START:
			if(IEC958_mode_raw_last != IEC958_mode_raw || (IEC958_mode_raw&&(IEC958_mode_codec_last !=  IEC958_mode_codec)))
			{
				IEC958_mode_raw_last = IEC958_mode_raw;
				IEC958_mode_codec_last = IEC958_mode_codec;
				aml_alsa_hw_reprepare();
			}	
			priv->decoded_nb_frames = 0;
			priv->format_wait_count = 0;
			if(priv->stream_fmt<=0)
				{
				DSP_PRNT("Audio dsp steam format have not set!\n");
				}
			else
				{
				ret=audiodsp_start();
				}
			break;
		case AUDIODSP_STOP:
			//DSP_PRNT("audiodsp command stop\n");
			stop_audiodsp_monitor(priv);
			dsp_stop(priv);
			priv->decoded_nb_frames = 0;
			priv->format_wait_count = 0;
			break;
#ifdef ENABLE_WAIT_FORMAT
		case AUDIODSP_DECODE_START:			
			if(priv->dsp_is_started)
				{
				dsp_codec_start(priv);
				wait_format_times=0;
				}
			else
				{
				DSP_PRNT("Audio dsp have not started\n");
				}			
			break;
		case AUDIODSP_WAIT_FORMAT:
			if(priv->dsp_is_started)
				{
				struct audio_info *audio_format;
				int ch = 0;
				audio_format = get_audio_info();
				
				wait_format_times++;
				
				if(wait_format_times>100){
					int audio_info = DSP_RD(DSP_AUDIO_FORMAT_INFO);
					if(audio_info){
						priv->frame_format.channel_num = audio_info&0xf;
						if(priv->frame_format.channel_num)
							priv->frame_format.valid |= CHANNEL_VALID;
						priv->frame_format.data_width= (audio_info>>4)&0x3f;
						if(priv->frame_format.data_width)
							priv->frame_format.valid |= DATA_WIDTH_VALID;
						priv->frame_format.sample_rate = (audio_info>>10);
						if(priv->frame_format.sample_rate)
							priv->frame_format.valid |= SAMPLE_RATE_VALID;
						DSP_PRNT("warning::got info from mailbox failed,read from regiser\n");
						ret = 0;
					}else{
						DSP_PRNT("dsp have not set the codec stream's format details,valid=%x\n",
						priv->frame_format.valid);		
						priv->format_wait_count++;
						if(priv->format_wait_count > 5){						
							if(audio_format->channels&&audio_format->sample_rate){
								priv->frame_format.channel_num = audio_format->channels>2?2:audio_format->channels;
								priv->frame_format.sample_rate = audio_format->sample_rate;
								priv->frame_format.data_width = 16;
								priv->frame_format.valid = CHANNEL_VALID|DATA_WIDTH_VALID|SAMPLE_RATE_VALID;
								DSP_PRNT("we have not got format details from dsp,so use the info got from the header parsed instead\n");
								ret = 0;
							}else{
								ret = -1;
							}
						}else{
							ret=-1;
						}
					}
				}else if(priv->frame_format.valid == (CHANNEL_VALID|DATA_WIDTH_VALID|SAMPLE_RATE_VALID)){
					       DSP_PRNT("audio info from header: sr %d,ch %d\n",audio_format->sample_rate,audio_format->channels);
						if(audio_format->channels > 0 ){
							if(audio_format->channels > 2)
								ch = 2;
							else
								ch = audio_format->channels;
							if(ch != priv->frame_format.channel_num){
								DSP_PRNT(" ch num info from dsp and header not match,[dsp %d ch],[header %d ch]", \
									priv->frame_format.channel_num,ch);
								priv->frame_format.channel_num = ch;
							}	
							
						}
						if(audio_format->sample_rate&&audio_format->sample_rate != priv->frame_format.sample_rate){
								DSP_PRNT(" sr num info from dsp and header not match,[dsp %d ],[header %d ]", \
									priv->frame_format.sample_rate,audio_format->sample_rate);
							priv->frame_format.sample_rate  = audio_format->sample_rate;
						}
						DSP_PRNT("applied audio sr %d,ch num %d\n",priv->frame_format.sample_rate,priv->frame_format.channel_num);
						
						/*Reset the PLL. Added by GK*/
						tsync_pcr_recover();
				}
				else{
					ret = -EAGAIN;
				}
				}
			else
				{
				DSP_PRNT("Audio dsp have not started\n");
				}			
			break;
#else /*!defined(ENABLE_WAIT_FORMAT)*/
		case AUDIODSP_DECODE_START:			
			if(priv->dsp_is_started)
				{
				int waittime=0;
				int ch = 0;
				struct audio_info *audio_format;
				dsp_codec_start(priv);
				audio_format = get_audio_info();
				while(!((priv->frame_format.valid & CHANNEL_VALID) &&
					(priv->frame_format.valid & SAMPLE_RATE_VALID) &&
					(priv->frame_format.valid & DATA_WIDTH_VALID)))
					{
					
					waittime++;
					if(waittime>100)
					{
						int audio_info = DSP_RD(DSP_AUDIO_FORMAT_INFO);
						if(audio_info){
							priv->frame_format.channel_num = audio_info&0xf;
							if(priv->frame_format.channel_num)
								priv->frame_format.valid |= CHANNEL_VALID;
							priv->frame_format.data_width= (audio_info>>4)&0x3f;
							if(priv->frame_format.data_width)
								priv->frame_format.valid |= DATA_WIDTH_VALID;
							priv->frame_format.sample_rate = (audio_info>>10);
							if(priv->frame_format.sample_rate)
								priv->frame_format.valid |= SAMPLE_RATE_VALID;
							DSP_PRNT("warning::got info from mailbox failed,read from regiser\n");
							continue;
						}
						DSP_PRNT("dsp have not set the codec stream's format details,valid=%x\n",
						priv->frame_format.valid);		
						priv->format_wait_count++;
						if(priv->format_wait_count > 5){
							
							if(audio_format->channels&&audio_format->sample_rate){
								priv->frame_format.channel_num = audio_format->channels>2?2:audio_format->channels;
								priv->frame_format.sample_rate = audio_format->sample_rate;
								priv->frame_format.data_width = 16;
								priv->frame_format.valid = CHANNEL_VALID|DATA_WIDTH_VALID|SAMPLE_RATE_VALID;
								DSP_PRNT("we have not got format details from dsp,so use the info got from the header parsed instead\n");
								ret = 0;
								break;
							}
						}
						ret=-1;
						break;
					}
					msleep(10);/*wait codec start and decode the format*/
					}
				/* check the info got from dsp with the info parsed from header,we use the header info as the base */
				if(priv->frame_format.valid == (CHANNEL_VALID|DATA_WIDTH_VALID|SAMPLE_RATE_VALID)){
					       DSP_PRNT("audio info from header: sr %d,ch %d\n",audio_format->sample_rate,audio_format->channels);
						if(audio_format->channels > 0 ){
							if(audio_format->channels > 2)
								ch = 2;
							else
								ch = audio_format->channels;
							if(ch != priv->frame_format.channel_num){
								DSP_PRNT(" ch num info from dsp and header not match,[dsp %d ch],[header %d ch]", \
									priv->frame_format.channel_num,ch);
								priv->frame_format.channel_num = ch;
							}	
							
						}
						if(audio_format->sample_rate&&audio_format->sample_rate != priv->frame_format.sample_rate){
								DSP_PRNT(" sr num info from dsp and header not match,[dsp %d ],[header %d ]", \
									priv->frame_format.sample_rate,audio_format->sample_rate);
							priv->frame_format.sample_rate  = audio_format->sample_rate;
						}
						DSP_PRNT("applied audio sr %d,ch num %d\n",priv->frame_format.sample_rate,priv->frame_format.channel_num);
				}
				/*Reset the PLL. Added by GK*/
				tsync_pcr_recover();
				}
			else
				{
				DSP_PRNT("Audio dsp have not started\n");
				}			
			break;
#endif /*ENABLE_WAIT_FORMAT*/
		case AUDIODSP_DECODE_STOP:
			if(priv->dsp_is_started)
				{
				dsp_codec_stop(priv);
				}
			else
				{
				DSP_PRNT("Audio dsp have not started\n");
				}
			
			break;
		case AUDIODSP_REGISTER_FIRMWARE:
			a_cmd=(struct audiodsp_cmd *)args;
		//	DSP_PRNT("register firware,%d,%s\n",a_cmd->fmt,a_cmd->data);
			len=a_cmd->data_len>64?64:a_cmd->data_len;
			copy_from_user(name,a_cmd->data,len);
			name[len]='\0';
			ret=audiodsp_microcode_register(priv,
								a_cmd->fmt,
								name);
			break;
		case AUDIODSP_UNREGISTER_ALLFIRMWARE:
			  audiodsp_microcode_free(priv);
			break;
		case AUDIODSP_GET_CHANNELS_NUM: 
			*val=-1;/*mask data is not valid*/
			if(priv->frame_format.valid & CHANNEL_VALID)
				{
				*val=priv->frame_format.channel_num; 
				}
			break;
		case AUDIODSP_GET_SAMPLERATE: 
			*val=-1;/*mask data is not valid*/
			if(priv->frame_format.valid & SAMPLE_RATE_VALID)
				{
				*val=priv->frame_format.sample_rate; 
				} 
			break;
		case AUDIODSP_GET_DECODED_NB_FRAMES: 			
				*val=priv->decoded_nb_frames;					
			break;
		case AUDIODSP_GET_BITS_PER_SAMPLE: 
			*val=-1;/*mask data is not valid*/
			if(priv->frame_format.valid & DATA_WIDTH_VALID)
				{
				*val=priv->frame_format.data_width; 
				} 
			break;
		case AUDIODSP_GET_PTS:
			/*val=-1 is not valid*/
			*val=dsp_codec_get_current_pts(priv);
			break;
		case AUDIODSP_GET_FIRST_PTS_FLAG:
			if(priv->stream_fmt == MCODEC_FMT_COOK || priv->stream_fmt == MCODEC_FMT_RAAC)
				*val = 1;
			else
				*val = first_pts_checkin_complete(PTS_TYPE_AUDIO);
			break;
			
		case AUDIODSP_SYNC_AUDIO_START:

			if(get_user(pts, (unsigned long __user *)args)) {
				printk("Get start pts from user space fault! \n");
				return -EFAULT;
			}
			tsync_avevent(AUDIO_START, pts);
			
			break;
			
		case AUDIODSP_SYNC_AUDIO_PAUSE:

			tsync_avevent(AUDIO_PAUSE, 0);
			break;

		case AUDIODSP_SYNC_AUDIO_RESUME:

			tsync_avevent(AUDIO_RESUME, 0);
			break;

		case AUDIODSP_SYNC_AUDIO_TSTAMP_DISCONTINUITY:

			if(get_user(pts, (unsigned long __user *)args)){
				printk("Get audio discontinuity pts fault! \n");
				return -EFAULT;
			}
			tsync_avevent(AUDIO_TSTAMP_DISCONTINUITY, pts);
			
			break;

		case AUDIODSP_SYNC_GET_APTS:

			pts = timestamp_apts_get();
			
			if(put_user(pts, (unsigned long __user *)args)){
				printk("Put audio pts to user space fault! \n");
				return -EFAULT;
			}
			
			break;

		case AUDIODSP_SYNC_GET_PCRSCR:

			pts = timestamp_pcrscr_get();

			if(put_user(pts, (unsigned long __user *)args)){
				printk("Put pcrscr to user space fault! \n");
				return -EFAULT;
			}
			
			break;

		case AUDIODSP_SYNC_SET_APTS:

			if(get_user(pts, (unsigned long __user *)args)){
				printk("Get audio pts from user space fault! \n");
				return -EFAULT;
			}
			tsync_set_apts(pts);
			
			break;

                case AUDIODSP_AUTOMUTE_ON:
                        tsync_set_automute_on(1);
                        break;

                case AUDIODSP_AUTOMUTE_OFF:
                        tsync_set_automute_on(0);
                        break;
			
		default:
			DSP_PRNT("unsupport cmd number%d\n",cmd);
			ret=-1;
		}
	return ret;
}

static int audiodsp_release(struct inode *node, struct file *file)
{
	DSP_PRNT("dsp_release\n");
	audiodsp_allow_sleep();
	return 0;
}



ssize_t audiodsp_read(struct file * file, char __user * ubuf, size_t size,
		  loff_t * loff)
{
	struct audiodsp_priv *priv=audiodsp_privdata();
	unsigned long rp,orp;
	size_t len;
	size_t else_len;
	size_t wlen;
	size_t w_else_len;
	int wait=0;
	 char __user *pubuf=ubuf;
     dma_addr_t buf_map;



	#define MIN_READ	2	// 1 channel * 2 bytes per sample
	#define PCM_DATA_MIN	2
	#define PCM_DATA_ALGIN(x) (x & (~(PCM_DATA_MIN-1)))
	#define MAX_WAIT		HZ/10
	
	mutex_lock(&priv->stream_buffer_mutex);	
	if(priv->stream_buffer_mem==NULL || !priv->dsp_is_started)
		goto error_out;
	
	do{
			len=dsp_codec_get_bufer_data_len(priv);
			if(len>MIN_READ)
				break;
			else
				{
				if(wait>0)
					break;
				wait++;
				init_completion(&priv->decode_completion);
				wait_for_completion_timeout(&priv->decode_completion, MAX_WAIT);
				}
		}while(len<MIN_READ);
	if(len>priv->stream_buffer_size || len <0)
		{
		DSP_PRNT("audio stream buffer is bad len=%d\n",len);
		goto error_out;
		}
	len=min(len,size);
	len=PCM_DATA_ALGIN(len);
	else_len=len;
	rp=dsp_codec_get_rd_addr(priv);
	orp=rp;
	while(else_len>0)
		{
		
		wlen=priv->stream_buffer_end-rp;
		wlen=min(wlen,else_len);
///		dma_cache_inv((unsigned long)rp,wlen);    
        buf_map = dma_map_single(NULL, (void *)rp, wlen, DMA_FROM_DEVICE);
		w_else_len=copy_to_user((void*)pubuf,(const char *)(rp),wlen);
		if(w_else_len!=0)
			{
			DSP_PRNT("copyed error,%d,%d,[%p]<---[%lx]\n",w_else_len,wlen,pubuf,rp);
			wlen-=w_else_len;
			}      
        dma_unmap_single(NULL, buf_map, wlen, DMA_FROM_DEVICE);
		else_len-=wlen;
		pubuf+=wlen;
		rp=dsp_codec_inc_rd_addr(priv,wlen);
		}
	priv->out_len_after_last_valid_pts+=len;
	mutex_unlock(&priv->stream_buffer_mutex);
	//u32 timestamp_pcrscr_get(void);
	//printk("current pts=%ld,src=%ld\n",dsp_codec_get_current_pts(priv),timestamp_pcrscr_get());
	return len;
error_out:
	mutex_unlock(&priv->stream_buffer_mutex);
	return 0;
}

ssize_t audiodsp_write(struct file * file, const char __user * ubuf, size_t size,
		   loff_t * loff)
{
	struct audiodsp_priv *priv=audiodsp_privdata();
	// int dsp_codec_start( struct audiodsp_priv *priv);
	// int dsp_codec_stop( struct audiodsp_priv *priv);
	audiodsp_microcode_register(priv,
								MCODEC_FMT_COOK,
								"audiodsp_codec_cook.bin");
	priv->stream_fmt=MCODEC_FMT_COOK;
	audiodsp_start();
	dsp_codec_start(priv);
	//dsp_codec_stop(priv);
	
	return size;
}


const static struct file_operations audiodsp_fops = {
	.owner = THIS_MODULE,
	.open =audiodsp_open,
	.read = audiodsp_read,
	.write = audiodsp_write,
	.release = audiodsp_release,
	.ioctl = audiodsp_ioctl,
};
static int audiodsp_get_status(struct adec_status *astatus)
{
	struct audiodsp_priv *priv=audiodsp_privdata();
	if(!astatus)
		return -EINVAL;
	if(priv->frame_format.valid & CHANNEL_VALID)
		astatus->channels=priv->frame_format.channel_num;
	else
		astatus->channels=0;
	if(priv->frame_format.valid & SAMPLE_RATE_VALID)
		astatus->sample_rate=priv->frame_format.sample_rate;
	else
		astatus->sample_rate=0;
	if(priv->frame_format.valid & DATA_WIDTH_VALID)
		astatus->resolution=priv->frame_format.data_width;
	else
		astatus->resolution=0;
	astatus->error_count=priv->decode_error_count;
	astatus->status=priv->dsp_is_started?0:1;
	return 0;
}
static int audiodsp_init_mcode(struct audiodsp_priv *priv)
{
	spin_lock_init(&priv->mcode_lock);
	priv->mcode_id=0;
	priv->dsp_stack_start=0;
	priv->dsp_gstack_start=0;
	priv->dsp_heap_start=0;
	priv->code_mem_size=AUDIO_DSP_MEM_SIZE -REG_MEM_SIZE;
	priv->dsp_code_start=AUDIO_DSP_START_ADDR;
    DSP_PRNT("DSP start addr 0x%x\n",AUDIO_DSP_START_ADDR);
	priv->dsp_stack_size=1024*64;
	priv->dsp_gstack_size=512;
	priv->dsp_heap_size=1024*1024;
	priv->stream_buffer_mem=NULL;
	priv->stream_buffer_mem_size=32*1024;
	priv->stream_fmt=-1;
	INIT_LIST_HEAD(&priv->mcode_list);
	init_completion(&priv->decode_completion);
	mutex_init(&priv->stream_buffer_mutex);
	mutex_init(&priv->dsp_mutex);
	priv->last_stream_fmt=-1;
	priv->last_valid_pts=0;
	priv->out_len_after_last_valid_pts=0;
	priv->dsp_work_details = (struct dsp_working_info*)DSP_WORK_INFO;
	return 0;
}

static ssize_t codec_fmt_show(struct class* cla, struct class_attribute* attr, char* buf)
{
    size_t ret = 0;
    struct audiodsp_priv *priv = audiodsp_privdata();
    ret = sprintf(buf, "The codec Format %d\n", priv->stream_fmt);
    return ret;
}

static ssize_t codec_fatal_err_show(struct class* cla, struct class_attribute* attr, char* buf)
{
    struct audiodsp_priv *priv = audiodsp_privdata();
	
    return sprintf(buf, "%d\n", priv->decode_fatal_err);
}
static ssize_t swap_buf_ptr_show(struct class *cla, struct class_attribute* attr, char* buf)
{
    char *pbuf = buf;

    pbuf += sprintf(pbuf, "swap buffer wp: %lx\n", DSP_RD(DSP_DECODE_OUT_WD_PTR));
    pbuf += sprintf(pbuf, "swap buffer rp: %lx\n", DSP_RD(DSP_DECODE_OUT_RD_ADDR));

    return (pbuf - buf);
}
 
static ssize_t dsp_working_status_show(struct class* cla, struct class_attribute* attr, char* buf)
{
    struct audiodsp_priv *priv = audiodsp_privdata();
    struct dsp_working_info *info = priv->dsp_work_details;
    char *pbuf = 	buf;
    pbuf += sprintf(pbuf, "\tdsp status  0x%lx\n", DSP_RD(DSP_STATUS));
    pbuf += sprintf(pbuf, "\tdsp sp  0x%x\n", info->sp);
 //   pbuf += sprintf(pbuf, "\tdsp pc  0x%x\n", info->pc);
    pbuf += sprintf(pbuf, "\tdsp ilink1  0x%x\n", info->ilink1);
    pbuf += sprintf(pbuf, "\tdsp ilink2  0x%x\n", info->ilink2);
    pbuf += sprintf(pbuf, "\tdsp blink  0x%x\n", info->blink);
    pbuf += sprintf(pbuf, "\tdsp jeffies  0x%lx\n", DSP_RD(DSP_JIFFIES));
    pbuf += sprintf(pbuf, "\tdsp pcm wp  0x%lx\n", DSP_RD(DSP_DECODE_OUT_WD_ADDR));
    pbuf += sprintf(pbuf, "\tdsp pcm rp  0x%lx\n", DSP_RD(DSP_DECODE_OUT_RD_ADDR));
    pbuf += sprintf(pbuf, "\tdsp pcm buffered size  0x%lx\n", DSP_RD(DSP_BUFFERED_LEN));
    pbuf += sprintf(pbuf, "\tdsp es read offset  0x%lx\n", DSP_RD(DSP_AFIFO_RD_OFFSET1));

    return 	(pbuf- buf);
}

static ssize_t digital_raw_show(struct class*cla, struct class_attribute* attr, char* buf)
{
  char* pbuf = buf;
  pbuf += sprintf(pbuf, "Digital output mode: %s\n", (IEC958_mode_raw==0)?"0 - PCM":"1 - RAW");
  return (pbuf-buf);
}
static ssize_t digital_raw_store(struct class* class, struct class_attribute* attr,
   const char* buf, size_t count )
{
  printk("buf=%s\n", buf);
  if(buf[0] == '0'){
    IEC958_mode_raw = 0;
  }else if(buf[0] == '1'){
    IEC958_mode_raw = 1;
  }
  printk("IEC958_mode_raw=%d\n", IEC958_mode_raw);
  return count;
}


static struct class_attribute audiodsp_attrs[]={
    __ATTR_RO(codec_fmt),
   // __ATTR_RO(codec_mips),   //no need for M3
    __ATTR_RO(codec_fatal_err),
    __ATTR_RO(swap_buf_ptr),
    __ATTR_RO(dsp_working_status),
    __ATTR(digital_raw, S_IRUGO | S_IWUSR, digital_raw_show, digital_raw_store),
    __ATTR_NULL
};

#ifdef CONFIG_PM
static int audiodsp_suspend(struct device* dev, pm_message_t state)
{
     struct audiodsp_priv *priv = audiodsp_privdata();
    if(wake_lock_active(&priv->wakelock)){
        return -1; // please stop dsp first
    }
    pm_state.event = state.event;
    if(state.event == PM_EVENT_SUSPEND){
        // should sleep cpu2 here after RevC chip
        msleep(50);
    }
    printk("audiodsp suspend\n");
    return 0;
}

static int audiodsp_resume(struct device* dev)
{
    if(pm_state.event == PM_EVENT_SUSPEND){
        // wakeup cpu2 
        pm_state.event = -1;
    }
    printk("audiodsp resumed\n");
    return 0;
}
#endif

static struct class audiodsp_class = {
    .name = DSP_DRIVER_NAME,
    .class_attrs = audiodsp_attrs,
#ifdef CONFIG_PM
    .suspend = audiodsp_suspend,
    .resume = audiodsp_resume,
#else
    .suspend = NULL,
    .resume = NULL,
#endif
};

int audiodsp_probe(void )
{

	int res=0;
	struct audiodsp_priv *priv;
	priv=kmalloc(sizeof(struct audiodsp_priv),GFP_KERNEL);
	if(priv==NULL)
		{
		DSP_PRNT("Out of memory for audiodsp register\n");
		return -1;
		}
    priv->dsp_is_started=0;
/*
    priv->p = ioremap_nocache(AUDIO_DSP_START_PHY_ADDR, S_1M);
    if(priv->p)
        DSP_PRNT("DSP IOREMAP to addr 0x%x\n",(unsigned)priv->p);
    else{
        DSP_PRNT("DSP IOREMAP error\n");
        goto error1;
    }
*/    
	audiodsp_p=priv;
	audiodsp_init_mcode(priv);
	if(priv->dsp_heap_size){
		if(priv->dsp_heap_start==0)
			priv->dsp_heap_start=(unsigned long)kmalloc(priv->dsp_heap_size,GFP_KERNEL);
		if(priv->dsp_heap_start==0)
		{
			DSP_PRNT("kmalloc error,no memory for audio dsp dsp_set_heap\n");
			kfree(priv);
			return -ENOMEM;
		}
	}	
	res = register_chrdev(AUDIODSP_MAJOR, DSP_NAME, &audiodsp_fops);
	if (res < 0) {
		DSP_PRNT("Can't register  char devie for " DSP_NAME "\n");
		goto error1;
	} else {
		DSP_PRNT("register " DSP_NAME " to char divece(%d)\n",
			  AUDIODSP_MAJOR);
	}

	res = class_register(&audiodsp_class);
	if(res <0 ){
	    DSP_PRNT("Create audiodsp class failed\n");
	    res = -EEXIST;
	    goto error2;
	}
	priv->class = &audiodsp_class;
	priv->dev = device_create(priv->class,
					    NULL, MKDEV(AUDIODSP_MAJOR, 0),
					    NULL, "audiodsp0");
	if(priv->dev==NULL)
		{
		res = -EEXIST;
		goto error3;
		}
	audiodsp_init_mailbox(priv);
	init_audiodsp_monitor(priv);
	wake_lock_init(&priv->wakelock,WAKE_LOCK_SUSPEND, "audiodsp");
#ifdef CONFIG_AM_STREAMING	
	set_adec_func(audiodsp_get_status);
#endif
    memset((void*)DSP_REG_OFFSET,0,REG_MEM_SIZE);
    
	return res;

//error4:
	device_destroy(priv->class, MKDEV(AUDIODSP_MAJOR, 0));
error3:
	class_destroy(priv->class);
error2:
	unregister_chrdev(AUDIODSP_MAJOR, DSP_NAME);
error1:
	kfree(priv);
	return res;
}

struct audiodsp_priv *audiodsp_privdata(void)
{
	return audiodsp_p;
}


static int __init audiodsp_init_module(void)
{

	return audiodsp_probe();
}

static void __exit audiodsp_exit_module(void)
{
	struct audiodsp_priv *priv;
	priv=audiodsp_privdata();
#ifdef CONFIG_AM_STREAMING		
	set_adec_func(NULL);
#endif
	dsp_stop(priv);
	stop_audiodsp_monitor(priv);
	audiodsp_release_mailbox(priv);
	release_audiodsp_monitor(priv);
	audiodsp_microcode_free(priv);
  /*
    iounmap(priv->p);
  */
	device_destroy(priv->class, MKDEV(AUDIODSP_MAJOR, 0));
	class_destroy(priv->class);
	unregister_chrdev(AUDIODSP_MAJOR, DSP_NAME);
	kfree(priv);
	priv=NULL;
	return;
}

module_init(audiodsp_init_module);
module_exit(audiodsp_exit_module);
