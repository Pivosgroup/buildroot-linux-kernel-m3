/*
 *  video post process. 
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/canvas.h>
#include <linux/vout/vinfo.h>
#include <linux/vout/vout_notify.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vfp.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <mach/am_regs.h>
#include <linux/amlog.h>
#include <linux/ge2d/ge2d_main.h>
#include <linux/ge2d/ge2d.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include "freescale_log.h"
#include "freescale_pri.h"
#include "freescale_dev.h"
//#include <mach/mod_gate.h>
#define VF_POOL_SIZE 4
#ifdef CONFIG_MIX_FREE_SCALE
#define ASS_POOL_SIZE 2
#else
#define ASS_POOL_SIZE 1
#endif

//#define DDD
#define RECEIVER_NAME "freescale"
#define PROVIDER_NAME   "freescale"


#define THREAD_INTERRUPT 0
#define THREAD_RUNNING 1
#define EnableVideoLayer()  \
    do { SET_MPEG_REG_MASK(VPP_MISC, \
         VPP_VD1_PREBLEND | VPP_PREBLEND_EN | VPP_VD1_POSTBLEND); \
    } while (0)

#define DisableVideoLayer() \
    do { CLEAR_MPEG_REG_MASK(VPP_MISC, \
         VPP_VD1_PREBLEND|VPP_VD1_POSTBLEND ); \
    } while (0)

#define DisableVideoLayer_PREBELEND() \
    do { CLEAR_MPEG_REG_MASK(VPP_MISC, \
         VPP_VD1_PREBLEND); \
    } while (0)

typedef struct ppframe_s {
    vframe_t  frame;
    int       index;
    int       angle;
    vframe_t  *dec_frame;
} ppframe_t;

static int ass_index; 
#ifdef CONFIG_MIX_FREE_SCALE
static int backup_index = -1;
static int backup_content_w = 0, backup_content_h = 0;
static int scaler_x = 0,scaler_y = 0,scaler_w = 0,scaler_h = 0;
static int scale_clear_count = 0;
static int scaler_pos_changed = 0 ;
extern u32 amvideo_get_scaler_mode(void);
extern u32 amvideo_get_scaler_para(int* x, int* y, int* w, int* h, u32* ratio);
#endif
int print_count = 0 ;
static DEFINE_SPINLOCK(lock);
static bool freescale_blocking = false;
static bool freescale_inited = false;

static struct ppframe_s vfp_pool[VF_POOL_SIZE];
static struct vframe_s *vfp_pool_free[VF_POOL_SIZE+1];
static struct vframe_s *vfp_pool_ready[VF_POOL_SIZE+1];

static vfq_t q_ready, q_free;

static struct semaphore thread_sem;
static DEFINE_MUTEX(freescale_mutex);

const vframe_receiver_op_t* vf_freescale_reg_provider();
void vf_freescale_unreg_provider(void);
void vf_freescale_reset(void);
static inline void freescale_vf_put_dec(vframe_t *vf);
static inline vframe_t *freescale_vf_peek_dec(void);

#define to_ppframe(vf)	\
	container_of(vf, struct ppframe_s, frame)

/************************************************
*
*   Canvas helpers.
*
*************************************************/
static inline u32 index2canvas(u32 index)
{
    const u32 canvas_tab[4] = {
        FREESCALE_CANVAS_INDEX+0, FREESCALE_CANVAS_INDEX+1, FREESCALE_CANVAS_INDEX+2, FREESCALE_CANVAS_INDEX+3
    };
    return canvas_tab[index];
}

/************************************************
*
*   freescale as a frame provider
*
*************************************************/
static int task_running = 0;
static int still_picture_notify = 0 ;
static int q_free_set = 0 ;
static vframe_t *freescale_vf_peek(void)
{
    vframe_t* vf;
    vf= 	vfq_peek(&q_ready);
   return vf;
}

static vframe_t *freescale_vf_get(void)
{
    return vfq_pop(&q_ready);
}

static void freescale_vf_put(vframe_t *vf)
{
    ppframe_t *pp_vf = to_ppframe(vf);

    /* the frame is in bypass mode, put the decoder frame */
    if (pp_vf->dec_frame)
        freescale_vf_put_dec(pp_vf->dec_frame);
    vfq_push(&q_free, vf);
}

static int get_input_format(vframe_t* vf)
{
    int format= GE2D_FORMAT_M24_YUV420;
    if(vf->type&VIDTYPE_VIU_422){
        format =  GE2D_FORMAT_S16_YUV422;
    }else{
    	 if(vf->type &VIDTYPE_INTERLACE_BOTTOM){
    	 		format =  GE2D_FORMAT_M24_YUV420|(GE2D_FMT_M24_YUV420B & (3<<3));
    	 	}else if(vf->type &VIDTYPE_INTERLACE_TOP){
    	 	format =  GE2D_FORMAT_M24_YUV420|(GE2D_FORMAT_M24_YUV420T & (3<<3));
    	 }else{
        	format =  GE2D_FORMAT_M24_YUV420;
    	 }
    }
    return format;
}

extern int fsl_get_property_change(void) ;
extern void fsl_set_property_change(int flag) ;
static int freescale_event_cb(int type, void *data, void *private_data)
{
    if (type & VFRAME_EVENT_RECEIVER_PUT) {
#ifdef DDD
        printk("video put, avail=%d, free=%d\n", vfq_level(&q_ready),  vfq_level(&q_free));
#endif
        up(&thread_sem);
    }
#ifdef CONFIG_MIX_FREE_SCALE
    if(type & VFRAME_EVENT_RECEIVER_POS_CHANGED){
        if(task_running){
            scaler_pos_changed = 1;
            //printk("--freescale: get pos changed msg.\n");
            up(&thread_sem);
        }
    }
#endif
    if(type & VFRAME_EVENT_RECEIVER_FRAME_WAIT){
        if(task_running){
            if(fsl_get_property_change()){
                //printk("--freescale: get angle changed msg.\n");
                fsl_set_property_change(0);
#ifdef CONFIG_MIX_FREE_SCALE
                if(!amvideo_get_scaler_mode()){
                    still_picture_notify = 1;
                    up(&thread_sem);
                }
#else
                still_picture_notify = 1;
                up(&thread_sem);
#endif
            }
        }
    }
    return 0;        
}

static int freescale_vf_states(vframe_states_t *states)
{
    unsigned long flags;
    spin_lock_irqsave(&lock, flags);

    states->vf_pool_size = VF_POOL_SIZE;
    states->buf_recycle_num = 0;
    states->buf_free_num = vfq_level(&q_free);
    states->buf_avail_num = vfq_level(&q_ready);

    spin_unlock_irqrestore(&lock, flags);

    return 0;
}

//static const struct vframe_provider_s freescale_vf_provider =
static const struct vframe_operations_s freescale_vf_provider = 
{
    .peek = freescale_vf_peek,
    .get  = freescale_vf_get,
    .put  = freescale_vf_put,
    .event_cb = freescale_event_cb,
    .vf_states = freescale_vf_states,
};
static struct vframe_provider_s freescale_vf_prov;
/************************************************
*
*   freescale as a frame receiver
*
*************************************************/

static int freescale_receiver_event_fun(int type, void* data, void*);

static const struct vframe_receiver_op_s freescale_vf_receiver =
{
    .event_cb = freescale_receiver_event_fun
};
static struct vframe_receiver_s freescale_vf_recv;

static int freescale_receiver_event_fun(int type, void *data, void *private_data)
{
    vframe_states_t states;
    switch(type) {
        case VFRAME_EVENT_PROVIDER_VFRAME_READY:
#ifdef DDD
            printk("dec put, avail=%d, free=%d\n", vfq_level(&q_ready),  vfq_level(&q_free));
#endif
		//printk("dec put, avail=%d, free=%d\n", vfq_level(&q_ready),  vfq_level(&q_free));
		if(freescale_vf_peek_dec()){
        	    up(&thread_sem);
        	}	
            break;
        case VFRAME_EVENT_PROVIDER_QUREY_STATE:
            freescale_vf_states(&states);
            if(states.buf_avail_num > 0){
                return RECEIVER_ACTIVE ;		
            }else{
                return RECEIVER_INACTIVE;
            }
            break;   
            case VFRAME_EVENT_PROVIDER_START:
#ifdef DDD
        printk("register now \n");
#endif                        
            vf_freescale_reg_provider();
            break;
            case VFRAME_EVENT_PROVIDER_UNREG:
#ifdef DDD
        printk("unregister now \n");
#endif            
            vf_freescale_unreg_provider();
            break;
            case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		vf_freescale_reset();            
            break;                 
            case VFRAME_EVENT_PROVIDER_RESET       :
            	vf_freescale_reset();
            	break;
        default:
            break;        
    }    		
    return 0;
}

static void vf_local_init(void) 
{
    int i;

    fsl_set_property_change(0); 
    still_picture_notify=0;
#ifdef CONFIG_MIX_FREE_SCALE
    scaler_pos_changed = 0;
    scaler_x = scaler_y = scaler_w = scaler_h = 0;
    backup_content_w = backup_content_h = 0;
#endif
    vfq_init(&q_free, VF_POOL_SIZE+1, &vfp_pool_free[0]);
    vfq_init(&q_ready, VF_POOL_SIZE+1, &vfp_pool_ready[0]);

    for (i=0; i < VF_POOL_SIZE; i++) {
        vfp_pool[i].index = i;
        vfq_push(&q_free, &vfp_pool[i].frame);
    }
    
    sema_init(&thread_sem,1);
}

static const struct vframe_provider_s *dec_vfp = NULL;

const vframe_receiver_op_t* vf_freescale_reg_provider()
{  
    const vframe_receiver_op_t *r = NULL;
    
    mutex_lock(&freescale_mutex);

    vf_local_init();
	print_count = 10 ;
    vf_reg_provider(&freescale_vf_prov);

    if (start_freescale_task() == 0) {
        r = &freescale_vf_receiver;
    }
    
    mutex_unlock(&freescale_mutex);

    return r;
}

void vf_freescale_unreg_provider(void)
{
    mutex_lock(&freescale_mutex);

    stop_freescale_task();

    vf_unreg_provider(&freescale_vf_prov);

    dec_vfp = NULL;

    mutex_unlock(&freescale_mutex);
}

void vf_freescale_reset(void)
{
    if(freescale_inited){
        freescale_blocking = true;
        up(&thread_sem);
    }
}

void vf_freescale_init_receiver(void)
{
    vf_receiver_init(&freescale_vf_recv, RECEIVER_NAME, &freescale_vf_receiver, NULL);
}

void vf_freescale_reg_receiver(void)
{
    vf_reg_receiver(&freescale_vf_recv);
}
void vf_freescale_init_provider()
{
	vf_provider_init(&freescale_vf_prov, PROVIDER_NAME ,&freescale_vf_provider, NULL);
}

static inline vframe_t *freescale_vf_peek_dec(void)
{
    struct vframe_provider_s *vfp;
    vframe_t *vf;
    vfp = vf_get_provider(RECEIVER_NAME);
    if (!(vfp && vfp->ops && vfp->ops->peek))
        return NULL;


    vf  = vfp->ops->peek(vfp->op_arg);
    return vf;	
}

static inline vframe_t *freescale_vf_get_dec(void)
{
    struct vframe_provider_s *vfp;
    vframe_t *vf;
    unsigned canvas_addr ;
    vfp = vf_get_provider(RECEIVER_NAME);
    if (!(vfp && vfp->ops && vfp->ops->peek))
        return NULL;
    vf =   vfp->ops->get(vfp->op_arg);
    return vf;
}

static inline void freescale_vf_put_dec(vframe_t *vf)
{
	struct vframe_provider_s *vfp;
	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->peek))
	return;
	vfp->ops->put(vf,vfp->op_arg);
}


/************************************************
*
*   main task functions.
*
*************************************************/
static void vf_rotate_adjust(vframe_t *vf, vframe_t *new_vf, int angle)
{
    int w, h;

    if (angle & 1) {
        int ar = (vf->ratio_control >> DISP_RATIO_ASPECT_RATIO_BIT) & 0x3ff;

        h = min((int)vf->width, (int)freescale_device.disp_height);

        if (ar == 0)
            w = vf->height * h / vf->width;
        else
            w = (ar * h) >> 8;
		if(w > freescale_device.disp_width ){			
			h = (h * freescale_device.disp_width)/w ;
			w = freescale_device.disp_width;
		}
        new_vf->ratio_control = DISP_RATIO_PORTRAIT_MODE;

    } else {
        if ((vf->width <= freescale_device.disp_width) && (vf->height <= freescale_device.disp_height)) {
            w = vf->width;
            h = vf->height;
        } else {
            if ((vf->width * freescale_device.disp_height) > (freescale_device.disp_width * vf->height)) {
                w = freescale_device.disp_width;
                h = freescale_device.disp_width * vf->height / vf->width;
				if(h > freescale_device.disp_height ){			
					w = (w * freescale_device.disp_height)/h ;
					h = freescale_device.disp_height;
				}					
            } else {
                h = freescale_device.disp_height;
                w = freescale_device.disp_height * vf->width / vf->height;
				if(w > freescale_device.disp_width ){			
					h = (h * freescale_device.disp_width)/w ;
					w = freescale_device.disp_width;
				}				
            }
			
        }

        new_vf->ratio_control = vf->ratio_control;
    }
    
    new_vf->width = w;
    new_vf->height = h;
}

static void process_vf_rotate(vframe_t *vf, ge2d_context_t *context, config_para_ex_t *ge2d_config)
{
    vframe_t *new_vf;
    ppframe_t *pp_vf;
    canvas_t cs0,cs1,cs2,cd;
    u32 mode = 0;
#ifdef CONFIG_MIX_FREE_SCALE
    int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
    u32 ratio = 100;
    mode = amvideo_get_scaler_para(&rect_x, &rect_y, &rect_w, &rect_h, &ratio);
#endif
    int pic_struct = 0;

    new_vf = vfq_pop(&q_free);
    
    if (unlikely((!new_vf) || (!vf)))
        return;

    int interlace_mode = vf->type & VIDTYPE_TYPEMASK;

    pp_vf = to_ppframe(new_vf);
    pp_vf->angle = 0;
    pp_vf->dec_frame = (freescale_device.bypass || (freescale_device.videoangle == 0)) ? vf : NULL;

#ifdef CONFIG_MIX_FREE_SCALE
    if(mode)
        pp_vf->dec_frame = NULL;
#endif

    if (pp_vf->dec_frame) {
        /* bypass mode */
        *new_vf = *vf;
        vfq_push(&q_ready, new_vf);
        return;
    }
    pp_vf->angle   =  freescale_device.videoangle;
    new_vf->duration = vf->duration;
    new_vf->duration_pulldown = vf->duration_pulldown;
    new_vf->pts = vf->pts;
    new_vf->type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
    new_vf->canvas0Addr = new_vf->canvas1Addr = index2canvas(pp_vf->index);

    if(interlace_mode == VIDTYPE_INTERLACE_TOP)
        pic_struct = (GE2D_FORMAT_M24_YUV420T & (3<<3));
    else if(interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
        pic_struct = (GE2D_FORMAT_M24_YUV420B & (3<<3));
	memset(ge2d_config,0,sizeof(config_para_ex_t));
#ifndef CONFIG_MIX_FREE_SCALE
    vf_rotate_adjust(vf, new_vf, freescale_device.videoangle);
#else
    if(!mode){
        vf_rotate_adjust(vf, new_vf, freescale_device.videoangle);
        scale_clear_count = 0;
    }else{
        pp_vf->angle = 0;
        new_vf->width = freescale_device.disp_width;
        new_vf->height = freescale_device.disp_height;
        new_vf->ratio_control = DISP_RATIO_FORCECONFIG|DISP_RATIO_NO_KEEPRATIO;
        if((rect_x != scaler_x)||(rect_w != scaler_w)
          ||(rect_y != scaler_y)||(rect_h != scaler_h)){
            scale_clear_count = VF_POOL_SIZE;
            scaler_x = rect_x;
            scaler_y = rect_y;
            scaler_w = rect_w;
            scaler_h = rect_h; 
            //printk("--freescale new rect x:%d, y:%d, w:%d, h:%d.\n", rect_x, rect_y, rect_w, rect_h);
        }
        if((!rect_w)||(!rect_h)){
            //printk("++freescale scale out of range 1.\n");
            freescale_vf_put_dec(vf);
            vfq_push(&q_free, new_vf);
            return;
        }
        if(((rect_x+rect_w)<0)||(rect_x>=(int)new_vf->width)
          ||((rect_y+rect_h)<0)||(rect_y>=(int)new_vf->height)){
            //printk("++freescale scale out of range 2.\n");
            freescale_vf_put_dec(vf);
            vfq_push(&q_free, new_vf);
            return;
        }
    }

    if(scale_clear_count>0){
    /* data operating. */ 
        ge2d_config->alu_const_color= 0;//0x000000ff;
        ge2d_config->bitmask_en  = 0;
        ge2d_config->src1_gb_alpha = 0;//0xff;
        ge2d_config->dst_xy_swap = 0;
    
        canvas_read(new_vf->canvas0Addr&0xff,&cd);
        ge2d_config->src_planes[0].addr = cd.addr;
        ge2d_config->src_planes[0].w = cd.width;
        ge2d_config->src_planes[0].h = cd.height;
        ge2d_config->dst_planes[0].addr = cd.addr;
        ge2d_config->dst_planes[0].w = cd.width;
        ge2d_config->dst_planes[0].h = cd.height;

        ge2d_config->src_key.key_enable = 0;
        ge2d_config->src_key.key_mask = 0;
        ge2d_config->src_key.key_mode = 0;

        ge2d_config->src_para.canvas_index=new_vf->canvas0Addr;
        ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
        ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
        ge2d_config->src_para.fill_color_en = 0;
        ge2d_config->src_para.fill_mode = 0;
        ge2d_config->src_para.x_rev = 0;
        ge2d_config->src_para.y_rev = 0;
        ge2d_config->src_para.color = 0;
        ge2d_config->src_para.top = 0;
        ge2d_config->src_para.left = 0;
        ge2d_config->src_para.width = new_vf->width;
        ge2d_config->src_para.height = new_vf->height;

        ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

        ge2d_config->dst_para.canvas_index=new_vf->canvas0Addr;
        ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
        ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
        ge2d_config->dst_para.fill_color_en = 0;
        ge2d_config->dst_para.fill_mode = 0;
        ge2d_config->dst_para.x_rev = 0;
        ge2d_config->dst_para.y_rev = 0;
        ge2d_config->dst_para.color = 0;
        ge2d_config->dst_para.top = 0;
        ge2d_config->dst_para.left = 0;
        ge2d_config->dst_para.width = new_vf->width;
        ge2d_config->dst_para.height = new_vf->height;
        
        if(ge2d_context_config_ex(context,ge2d_config)<0) {
            printk("++ge2d configing error.\n");
            freescale_vf_put_dec(vf);
            vfq_push(&q_free, new_vf);
            return;
        }
        fillrect(context, 0, 0, new_vf->width, new_vf->height, 0x008080ff);
        scale_clear_count--;
    }

    if((backup_index>0)&&(mode)){
        unsigned dst_w  = vf->width, dst_h = vf->height;
        if((dst_w > freescale_device.disp_width)||(dst_h > freescale_device.disp_height)){
            if((dst_w * freescale_device.disp_height)>(dst_h*freescale_device.disp_width)){
                dst_h = (dst_w * freescale_device.disp_height)/freescale_device.disp_width;
                dst_w = freescaleprintk_device.disp_width;                
            }else{
                dst_w = (dst_h*freescale_device.disp_width)/freescale_device.disp_height;
                dst_h = freescale_device.disp_height;
            }
            dst_w = dst_w&(0xfffffffe);
            dst_h = dst_h&(0xfffffffe);
        }
        ge2d_config->alu_const_color= 0;//0x000000ff;
        ge2d_config->bitmask_en  = 0;
        ge2d_config->src1_gb_alpha = 0;//0xff;
        ge2d_config->dst_xy_swap = 0;

        canvas_read(vf->canvas0Addr&0xff,&cs0);
        canvas_read((vf->canvas0Addr>>8)&0xff,&cs1);
        canvas_read((vf->canvas0Addr>>16)&0xff,&cs2);
        ge2d_config->src_planes[0].addr = cs0.addr;
        ge2d_config->src_planes[0].w = cs0.width;
        ge2d_config->src_planes[0].h = cs0.height;
        ge2d_config->src_planes[1].addr = cs1.addr;
        ge2d_config->src_planes[1].w = cs1.width;
        ge2d_config->src_planes[1].h = cs1.height;
        ge2d_config->src_planes[2].addr = cs2.addr;
        ge2d_config->src_planes[2].w = cs2.width;
        ge2d_config->src_planes[2].h = cs2.height;
    
        canvas_read(new_vf->canvas0Addr&0xff,&cd);
        ge2d_config->dst_planes[0].addr = cd.addr;
        ge2d_config->dst_planes[0].w = cd.width;
        ge2d_config->dst_planes[0].h = cd.height;

        ge2d_config->src_key.key_enable = 0;
        ge2d_config->src_key.key_mask = 0;
        ge2d_config->src_key.key_mode = 0;

        ge2d_config->src_para.canvas_index=vf->canvas0Addr;
        ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
        //ge2d_config->src_para.format = GE2D_FORMAT_M24_YUV420|pic_struct;
        ge2d_config->src_para.format =get_input_format(vf);
        ge2d_config->src_para.fill_color_en = 0;
        ge2d_config->src_para.fill_mode = 0;
        ge2d_config->src_para.x_rev = 0;
        ge2d_config->src_para.y_rev = 0;
        ge2d_config->src_para.color = 0xffffffff;
        ge2d_config->src_para.top = 0;
        ge2d_config->src_para.left = 0;
        ge2d_config->src_para.width = vf->width;
        ge2d_config->src_para.height = (pic_struct)?(vf->height/2):vf->height;

        ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

        ge2d_config->dst_para.canvas_index=backup_index;
        ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
        ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
        ge2d_config->dst_para.fill_color_en = 0;
        ge2d_config->dst_para.fill_mode = 0;
        ge2d_config->dst_para.x_rev = 0;
        ge2d_config->dst_para.y_rev = 0;
        ge2d_config->dst_xy_swap=0;

        ge2d_config->dst_para.color = 0;
        ge2d_config->dst_para.top = 0;
        ge2d_config->dst_para.left = 0;
        ge2d_config->dst_para.width = new_vf->width;
        ge2d_config->dst_para.height = new_vf->height;

        if(ge2d_context_config_ex(context,ge2d_config)<0) {
            printk("++ge2d configing error.\n");
            freescale_vf_put_dec(vf);
            vfq_push(&q_free, new_vf);
            return;
        }
        stretchblt_noalpha(context,0,0,vf->width,(pic_struct)?(vf->height/2):vf->height,0,0,dst_w,dst_h);
        backup_content_w = dst_w;
        backup_content_h = dst_h;
        //printk("--freescale: backup data size: content:%d*%d\n",backup_content_w,backup_content_h);
    }
#endif

    /* data operating. */ 
    ge2d_config->alu_const_color= 0;//0x000000ff;
    ge2d_config->bitmask_en  = 0;
    ge2d_config->src1_gb_alpha = 0;//0xff;
    ge2d_config->dst_xy_swap = 0;

    canvas_read(vf->canvas0Addr&0xff,&cs0);
    canvas_read((vf->canvas0Addr>>8)&0xff,&cs1);
    canvas_read((vf->canvas0Addr>>16)&0xff,&cs2);
    ge2d_config->src_planes[0].addr = cs0.addr;
    ge2d_config->src_planes[0].w = cs0.width;
    ge2d_config->src_planes[0].h = cs0.height;
    ge2d_config->src_planes[1].addr = cs1.addr;
    ge2d_config->src_planes[1].w = cs1.width;
    ge2d_config->src_planes[1].h = cs1.height;
    ge2d_config->src_planes[2].addr = cs2.addr;
    ge2d_config->src_planes[2].w = cs2.width;
    ge2d_config->src_planes[2].h = cs2.height;
    
    canvas_read(new_vf->canvas0Addr&0xff,&cd);
    ge2d_config->dst_planes[0].addr = cd.addr;
    ge2d_config->dst_planes[0].w = cd.width;
    ge2d_config->dst_planes[0].h = cd.height;

    ge2d_config->src_key.key_enable = 0;
    ge2d_config->src_key.key_mask = 0;
    ge2d_config->src_key.key_mode = 0;

    ge2d_config->src_para.canvas_index=vf->canvas0Addr;
    ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
    //ge2d_config->src_para.format = GE2D_FORMAT_M24_YUV420|pic_struct;
	ge2d_config->src_para.format =get_input_format(vf);
    ge2d_config->src_para.fill_color_en = 0;
    ge2d_config->src_para.fill_mode = 0;
    ge2d_config->src_para.x_rev = 0;
    ge2d_config->src_para.y_rev = 0;
    ge2d_config->src_para.color = 0xffffffff;
    ge2d_config->src_para.top = 0;
    ge2d_config->src_para.left = 0;
    ge2d_config->src_para.width = vf->width;
    ge2d_config->src_para.height = (pic_struct)?(vf->height/2):vf->height;

    ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

    ge2d_config->dst_para.canvas_index=new_vf->canvas0Addr;
    ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
    //ge2d_config->dst_para.mem_type = CANVAS_OSD0;
    //ge2d_config->dst_para.format = GE2D_FORMAT_M24_YUV420;
    ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
    ge2d_config->dst_para.fill_color_en = 0;
    ge2d_config->dst_para.fill_mode = 0;
    ge2d_config->dst_para.x_rev = 0;
    ge2d_config->dst_para.y_rev = 0;
    ge2d_config->dst_xy_swap=0;
	
	if(!mode){
        if(freescale_device.videoangle==1){
            ge2d_config->dst_xy_swap=1;
            ge2d_config->dst_para.x_rev = 1;
        }
        else if(freescale_device.videoangle==2){
            ge2d_config->dst_para.x_rev = 1;
            ge2d_config->dst_para.y_rev=1;        
        }
        else if(freescale_device.videoangle==3)  {
            ge2d_config->dst_xy_swap=1;
            ge2d_config->dst_para.y_rev=1;
        }
    }
    ge2d_config->dst_para.color = 0;
    ge2d_config->dst_para.top = 0;
    ge2d_config->dst_para.left = 0;
    ge2d_config->dst_para.width = new_vf->width;
    ge2d_config->dst_para.height = new_vf->height;

    if(ge2d_context_config_ex(context,ge2d_config)<0) {
        printk("++ge2d configing error.\n");
        vfq_push(&q_free, new_vf);
        return;
    }
    
    if(!mode)
        pp_vf->angle = freescale_device.videoangle ;

#ifdef CONFIG_MIX_FREE_SCALE
    if(mode){
        int sx,sy,sw,sh, dx,dy,dw,dh;
        unsigned ratio_x = (vf->width<<8)/rect_w;
        unsigned ratio_y = (vf->height<<8)/rect_h;
        if(rect_x<0){
            sx = ((0-rect_x)*ratio_x)>>8;
            sx = sx&(0xfffffffe);
            dx = 0;
        }else{
            sx = 0;
            dx = rect_x;
        }
        if((rect_x+rect_w)>new_vf->width){
            sw = ((rect_x + rect_w - new_vf->width))*ratio_x>>8;
            sw = vf->width - sx -sw;
            sw = sw&(0xfffffffe);
            if(rect_x<0)
                dw = new_vf->width;
            else
                dw = new_vf->width -dx;
        }else{
            sw = vf->width - sx;
            sw = sw&(0xfffffffe);
            if(rect_x<0)
                dw = rect_w+rect_x;
            else
                dw = rect_w;
        }

        if(rect_y<0){
            sy = ((0-rect_y)*ratio_y)>>8;
            sy = sy&(0xfffffffe);
            dy = 0;
        }else{
            sy = 0;
            dy = rect_y;
        }
        if((rect_y+rect_h)>new_vf->height){
            sh = ((rect_y + rect_h - new_vf->height))*ratio_y>>8;
            sh = vf->height - sy -sh;
            sh = sh&(0xfffffffe);
            if(rect_y<0)
                dh = new_vf->height;
            else
                dh = new_vf->height -dy;
        }else{
            sh = vf->height - sy;
            sh = sh&(0xfffffffe);
            if(rect_y<0)
                dh = rect_h+rect_y;
            else
                dh = rect_h;
        }printk
        //if(scale_clear_count==3)
        //    printk("--freescale scale rect: src x:%d, y:%d, w:%d, h:%d. dst x:%d, y:%d, w:%d, h:%d.\n", sx, sy, sw, sh,dx,dy,dw,dh);
        stretchblt_noalpha(context,sx,(pic_struct)?(sy/2):sy,sw,(pic_struct)?(sh/2):sh,dx,dy,dw,dh);
    }else{
        stretchblt_noalpha(context,0,0,vf->width,(pic_struct)?(vf->height/2):vf->height,0,0,new_vf->width,new_vf->height);
    }
#else
    stretchblt_noalpha(context,0,0,vf->width,(pic_struct)?(vf->height/2):vf->height,0,0,new_vf->width,new_vf->height);
	
#endif
    freescale_vf_put_dec(vf);

    vfq_push(&q_ready, new_vf);

#ifdef DDD
    printk("rotate avail=%d, free=%d\n", vfq_level(&q_ready),  vfq_level(&q_free));
#endif
}

static void process_vf_change(vframe_t *vf, ge2d_context_t *context, config_para_ex_t *ge2d_config)
{
    vframe_t  temp_vf;
    ppframe_t *pp_vf = to_ppframe(vf);
    canvas_t cs0,cs1,cs2,cd;
    int pic_struct = 0;

    temp_vf.duration = vf->duration;
    temp_vf.duration_pulldown = vf->duration_pulldown;
    temp_vf.pts = vf->pts;
    temp_vf.type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
    temp_vf.canvas0Addr = temp_vf.canvas1Addr = ass_index;
    int temp_angle = 0;
    temp_angle = (freescale_device.videoangle  >= pp_vf->angle )?(freescale_device.videoangle -  pp_vf->angle) :  (freescale_device.videoangle + 4 -  pp_vf->angle)   ;
    
    pp_vf->angle = freescale_device.videoangle ;
    vf_rotate_adjust(vf, &temp_vf, temp_angle);

    int interlace_mode = vf->type & VIDTYPE_TYPEMASK;
    if(interlace_mode == VIDTYPE_INTERLACE_TOP)
        pic_struct = (GE2D_FORMAT_M24_YUV420T & (3<<3));
    else if(interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
        pic_struct = (GE2D_FORMAT_M24_YUV420B & (3<<3));

    /* data operating. */ 
    ge2d_config->alu_const_color= 0;//0x000000ff;
    ge2d_config->bitmask_en  = 0;
    ge2d_config->src1_gb_alpha = 0;//0xff;
    ge2d_config->dst_xy_swap = 0;
    if (pp_vf->dec_frame){
        canvas_read(vf->canvas0Addr&0xff,&cs0);
        canvas_read((vf->canvas0Addr>>8)&0xff,&cs1);
        canvas_read((vf->canvas0Addr>>16)&0xff,&cs2);
        ge2d_config->src_planes[0].addr = cs0.addr;
        ge2d_config->src_planes[0].w = cs0.width;
        ge2d_config->src_planes[0].h = cs0.height;
        ge2d_config->src_planes[1].addr = cs1.addr;
        ge2d_config->src_planes[1].w = cs1.width;
        ge2d_config->src_planes[1].h = cs1.height;
        ge2d_config->src_planes[2].addr = cs2.addr;
        ge2d_config->src_planes[2].w = cs2.width;
        ge2d_config->src_planes[2].h = cs2.height;
        ge2d_config->src_para.format = GE2D_FORMAT_M24_YUV420|pic_struct;
    }else{
        canvas_read(vf->canvas0Addr&0xff,&cd);
        ge2d_config->src_planes[0].addr = cd.addr;
        ge2d_config->src_planes[0].w = cd.width;
        ge2d_config->src_planes[0].h = cd.height;	 	
        ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444|pic_struct;
    }
    
    canvas_read(temp_vf.canvas0Addr&0xff,&cd);
    ge2d_config->dst_planes[0].addr = cd.addr;
    ge2d_config->dst_planes[0].w = cd.width;
    ge2d_config->dst_planes[0].h = cd.height;

    ge2d_config->src_key.key_enable = 0;
    ge2d_config->src_key.key_mask = 0;
    ge2d_config->src_key.key_mode = 0;

    ge2d_config->src_para.canvas_index=vf->canvas0Addr;
    ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
   
    ge2d_config->src_para.fill_color_en = 0;
    ge2d_config->src_para.fill_mode = 0;
    ge2d_config->src_para.x_rev = 0;
    ge2d_config->src_para.y_rev = 0;
    ge2d_config->src_para.color = 0xffffffff;
    ge2d_config->src_para.top = 0;
    ge2d_config->src_para.left = 0;
    ge2d_config->src_para.width = vf->width;
    ge2d_config->src_para.height = (pic_struct)?(vf->height/2):vf->height;

    ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

    ge2d_config->dst_para.canvas_index=temp_vf.canvas0Addr;
    ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
    ge2d_config->dst_para.fill_color_en = 0;
    ge2d_config->dst_para.fill_mode = 0;
    ge2d_config->dst_para.x_rev = 0;
    ge2d_config->dst_para.y_rev = 0;
    ge2d_config->dst_xy_swap=0;

    ge2d_config->dst_para.color = 0;
    ge2d_config->dst_para.top = 0;
    ge2d_config->dst_para.left = 0;
    ge2d_config->dst_para.width = temp_vf.width;
    ge2d_config->dst_para.height = temp_vf.height;

    if(temp_angle==1){
        ge2d_config->dst_xy_swap=1;
        ge2d_config->dst_para.x_rev = 1;
    }
    else if(temp_angle==2){
        ge2d_config->dst_para.x_rev = 1;
        ge2d_config->dst_para.y_rev=1;        
    }
    else if(temp_angle==3)  {
        ge2d_config->dst_xy_swap=1;
        ge2d_config->dst_para.y_rev=1;
    }
    if(ge2d_context_config_ex(context,ge2d_config)<0) {
        printk("++ge2d configing error.\n");
        //vfq_push(&q_free, new_vf);
        return;
    }

    stretchblt_noalpha(context,0,0,vf->width,(pic_struct)?(vf->height/2):vf->height,0,0,temp_vf.width,temp_vf.height);
    
    vf->type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;
    vf->canvas0Addr = vf->canvas1Addr = index2canvas(pp_vf->index);

    /* data operating. */ 
    ge2d_config->alu_const_color= 0;//0x000000ff;
    ge2d_config->bitmask_en  = 0;
    ge2d_config->src1_gb_alpha = 0;//0xff;
    ge2d_config->dst_xy_swap = 0;

    canvas_read(temp_vf.canvas0Addr&0xff,&cd);
    ge2d_config->src_planes[0].addr = cd.addr;
    ge2d_config->src_planes[0].w = cd.width;
    ge2d_config->src_planes[0].h = cd.height;	 	
    ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;

    canvas_read(vf->canvas0Addr&0xff,&cd);
    ge2d_config->dst_planes[0].addr = cd.addr;
    ge2d_config->dst_planes[0].w = cd.width;
    ge2d_config->dst_planes[0].h = cd.height;

    ge2d_config->src_key.key_enable = 0;
    ge2d_config->src_key.key_mask = 0;
    ge2d_config->src_key.key_mode = 0;

    ge2d_config->src_para.canvas_index=temp_vf.canvas0Addr;
    ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
   
    ge2d_config->src_para.fill_color_en = 0;
    ge2d_config->src_para.fill_mode = 0;
    ge2d_config->src_para.x_rev = 0;
    ge2d_config->src_para.y_rev = 0;
    ge2d_config->src_para.color = 0xffffffff;
    ge2d_config->src_para.top = 0;
    ge2d_config->src_para.left = 0;
    ge2d_config->src_para.width = temp_vf.width;
    ge2d_config->src_para.height = temp_vf.height;

    vf->width = temp_vf.width;
    vf->height = temp_vf.height ;
    ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

    ge2d_config->dst_para.canvas_index=vf->canvas0Addr;
    ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
    ge2d_config->dst_para.fill_color_en = 0;
    ge2d_config->dst_para.fill_mode = 0;
    ge2d_config->dst_para.x_rev = 0;
    ge2d_config->dst_para.y_rev = 0;
    ge2d_config->dst_xy_swap=0;
    ge2d_config->dst_para.color = 0;
    ge2d_config->dst_para.top = 0;
    ge2d_config->dst_para.left = 0;
    ge2d_config->dst_para.width = vf->width;
    ge2d_config->dst_para.height = vf->height;

    if(ge2d_context_config_ex(context,ge2d_config)<0) {
        printk("++ge2d configing error.\n");
        //vfq_push(&q_free, new_vf);
        return;
    }
    stretchblt_noalpha(context,0,0,temp_vf.width,temp_vf.height,0,0,vf->width,vf->height);
    //vf->duration = 0 ;
    if (pp_vf->dec_frame){
        freescale_vf_put_dec(pp_vf->dec_frame);
        pp_vf->dec_frame = 0 ;
    }
    vf->ratio_control = 0;    
}

#ifdef CONFIG_MIX_FREE_SCALE
static int process_vf_adjust(vframe_t *vf, ge2d_context_t *context, config_para_ex_t *ge2d_config)
{
    canvas_t cs,cd;
    int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
    u32 ratio = 100;
    u32 mode = amvideo_get_scaler_para(&rect_x, &rect_y, &rect_w, &rect_h, &ratio);
    
    if(!mode){
        //printk("--freescale adjust: scaler mode is disabled.\n");
        return -1;
    }

    if((rect_x == scaler_x)&&(rect_w == scaler_w)
      &&(rect_y == scaler_y)&&(rect_h == scaler_h)){
        //printk("--freescale adjust: same pos. need not adjust.\n");
        return -1;
    }

    if((!rect_w)||(!rect_h)){
        //printk("--freescale adjust: scale out of range 1.\n");
        return -1;
    }
    if(((rect_x+rect_w)<0)||(rect_x>=(int)freescale_device.disp_width)
      ||((rect_y+rect_h)<0)||(rect_y>=(int)freescale_device.disp_height)){
        //printk("--freescale adjust: scale out of range 2.\n");
        return -1;
    }
    if((!backup_content_w)||(!backup_content_h)){
        //printk("--freescale adjust: scale out of range 3.\n");
        return -1;
    }

    scale_clear_count = VF_POOL_SIZE;
    scaler_x = rect_x;
    scaler_y = rect_y;
    scaler_w = rect_w;
    scaler_h = rect_h; 

    ge2d_config->alu_const_color= 0;//0x000000ff;
    ge2d_config->bitmask_en  = 0;
    ge2d_config->src1_gb_alpha = 0;//0xff;
    ge2d_config->dst_xy_swap = 0;
    
    canvas_read(ass_index&0xff,&cd);
    ge2d_config->src_planes[0].addr = cd.addr;
    ge2d_config->src_planes[0].w = cd.width;
    ge2d_config->src_planes[0].h = cd.height;
    ge2d_config->dst_planes[0].addr = cd.addr;
    ge2d_config->dst_planes[0].w = cd.width;
    ge2d_config->dst_planes[0].h = cd.height;

    ge2d_config->src_key.key_enable = 0;
    ge2d_config->src_key.key_mask = 0;
    ge2d_config->src_key.key_mode = 0;

    ge2d_config->src_para.canvas_index=ass_index;
    ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
    ge2d_config->src_para.fill_color_en = 0;
    ge2d_config->src_para.fill_mode = 0;
    ge2d_config->src_para.x_rev = 0;
    ge2d_config->src_para.y_rev = 0;
    ge2d_config->src_para.color = 0;
    ge2d_config->src_para.top = 0;
    ge2d_config->src_para.left = 0;
    ge2d_config->src_para.width = freescale_device.disp_width;
    ge2d_config->src_para.height = freescale_device.disp_height;

    ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

    ge2d_config->dst_para.canvas_index=ass_index;
    ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
    ge2d_config->dst_para.fill_color_en = 0;
    ge2d_config->dst_para.fill_mode = 0;
    ge2d_config->dst_para.x_rev = 0;
    ge2d_config->dst_para.y_rev = 0;
    ge2d_config->dst_para.color = 0;
    ge2d_config->dst_para.top = 0;
    ge2d_config->dst_para.left = 0;
    ge2d_config->dst_para.width = freescale_device.disp_width;
    ge2d_config->dst_para.height = freescale_device.disp_height;
        
    if(ge2d_context_config_ex(context,ge2d_config)<0) {
        printk("++ge2d configing error.\n");
        return -2;
    }
    fillrect(context, 0, 0, freescale_device.disp_width, freescale_device.disp_height, 0x008080ff);

    ge2d_config->alu_const_color= 0;//0x000000ff;
    ge2d_config->bitmask_en  = 0;
    ge2d_config->src1_gb_alpha = 0;//0xff;
    ge2d_config->dst_xy_swap = 0;

    canvas_read(backup_index&0xff,&cs);
    ge2d_config->src_planes[0].addr = cs.addr;
    ge2d_config->src_planes[0].w = cs.width;
    ge2d_config->src_planes[0].h = cs.height;	 	

    canvas_read(ass_index&0xff,&cd);
    ge2d_config->dst_planes[0].addr = cd.addr;
    ge2d_config->dst_planes[0].w = cd.width;
    ge2d_config->dst_planes[0].h = cd.height;

    ge2d_config->src_key.key_enable = 0;
    ge2d_config->src_key.key_mask = 0;
    ge2d_config->src_key.key_mode = 0;

    ge2d_config->src_para.canvas_index=backup_index;
    ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;   
    ge2d_config->src_para.fill_color_en = 0;
    ge2d_config->src_para.fill_mode = 0;
    ge2d_config->src_para.x_rev = 0;
    ge2d_config->src_para.y_rev = 0;
    ge2d_config->src_para.color = 0xffffffff;
    ge2d_config->src_para.top = 0;
    ge2d_config->src_para.left = 0;
    ge2d_config->src_para.width = freescale_device.disp_width;
    ge2d_config->src_para.height = freescale_device.disp_height;

    ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

    ge2d_config->dst_para.canvas_index=ass_index;
    ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
    ge2d_config->dst_para.fill_color_en = 0;
    ge2d_config->dst_para.fill_mode = 0;
    ge2d_config->dst_para.x_rev = 0;
    ge2d_config->dst_para.y_rev = 0;
    ge2d_config->dst_xy_swap=0;

    ge2d_config->dst_para.color = 0;
    ge2d_config->dst_para.top = 0;
    ge2d_config->dst_para.left = 0;
    ge2d_config->dst_para.width = freescale_device.disp_width;
    ge2d_config->dst_para.height = freescale_device.disp_height;

    if(ge2d_context_config_ex(context,ge2d_config)<0) {
        printk("++ge2d configing error.\n");
        return -2;
    }

    int sx,sy,sw,sh, dx,dy,dw,dh;
    unsigned ratio_x = (backup_content_w<<8)/rect_w;
    unsigned ratio_y = (backup_content_h<<8)/rect_h;

    if(rect_x<0){
        sx = ((0-rect_x)*ratio_x)>>8;
        sx = sx&(0xfffffffe);
        dx = 0;
    }else{
        sx = 0;
        dx = rect_x;
    }
    if((rect_x+rect_w)>vf->width){
        sw = ((rect_x + rect_w - vf->width))*ratio_x>>8;
        sw = backup_content_w - sx -sw;
        sw = sw&(0xfffffffe);
        if(rect_x<0)
            dw = vf->width;
        else
            dw = vf->width -dx;
    }else{
        sw = backup_content_w - sx;
        sw = sw&(0xfffffffe);
        if(rect_x<0)
            dw = rect_w+rect_x;
        else
            dw = rect_w;
    }

    if(rect_y<0){
        sy = ((0-rect_y)*ratio_y)>>8;
        sy = sy&(0xfffffffe);
        dy = 0;
    }else{
        sy = 0;
        dy = rect_y;
    }
    if((rect_y+rect_h)>vf->height){
        sh = ((rect_y + rect_h - vf->height))*ratio_y>>8;
        sh = backup_content_h - sy -sh;
        sh = sh&(0xfffffffe);
        if(rect_y<0)
            dh = vf->height;
        else
            dh = vf->height -dy;
    }else{
        sh = backup_content_h - sy;
        sh = sh&(0xfffffffe);
        if(rect_y<0)
            dh = rect_h+rect_y;
        else
            dh = rect_h;
    }
    //printk("--freescale adjust: src x:%d, y:%d, w:%d, h:%d. dst x:%d, y:%d, w:%d, h:%d.\n", sx, sy, sw, sh,dx,dy,dw,dh);

    stretchblt_noalpha(context,sx,sy,sw,sh,dx,dy,dw,dh);
    
    /* data operating. */ 
    ge2d_config->alu_const_color= 0;//0x000000ff;
    ge2d_config->bitmask_en  = 0;
    ge2d_config->src1_gb_alpha = 0;//0xff;
    ge2d_config->dst_xy_swap = 0;

    canvas_read(ass_index&0xff,&cs);
    ge2d_config->src_planes[0].addr = cs.addr;
    ge2d_config->src_planes[0].w = cs.width;
    ge2d_config->src_planes[0].h = cs.height;	 	

    canvas_read(vf->canvas0Addr&0xff,&cd);
    ge2d_config->dst_planes[0].addr = cd.addr;
    ge2d_config->dst_planes[0].w = cd.width;
    ge2d_config->dst_planes[0].h = cd.height;

    ge2d_config->src_key.key_enable = 0;
    ge2d_config->src_key.key_mask = 0;
    ge2d_config->src_key.key_mode = 0;

    ge2d_config->src_para.canvas_index=ass_index;
    ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->src_para.format = GE2D_FORMAT_S24_YUV444;
    ge2d_config->src_para.fill_color_en = 0;
    ge2d_config->src_para.fill_mode = 0;
    ge2d_config->src_para.x_rev = 0;
    ge2d_config->src_para.y_rev = 0;
    ge2d_config->src_para.color = 0xffffffff;
    ge2d_config->src_para.top = 0;
    ge2d_config->src_para.left = 0;
    ge2d_config->src_para.width = freescale_device.disp_width;
    ge2d_config->src_para.height = freescale_device.disp_height;

    ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

    ge2d_config->dst_para.canvas_index=vf->canvas0Addr;
    ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->dst_para.format = GE2D_FORMAT_S24_YUV444;
    ge2d_config->dst_para.fill_color_en = 0;
    ge2d_config->dst_para.fill_mode = 0;
    ge2d_config->dst_para.x_rev = 0;
    ge2d_config->dst_para.y_rev = 0;
    ge2d_config->dst_xy_swap=0;
    ge2d_config->dst_para.color = 0;
    ge2d_config->dst_para.top = 0;
    ge2d_config->dst_para.left = 0;
    ge2d_config->dst_para.width = vf->width;
    ge2d_config->dst_para.height = vf->height;

    if(ge2d_context_config_ex(context,ge2d_config)<0) {
        printk("++ge2d configing error.\n");
        return -2;
    }
    stretchblt_noalpha(context,0,0,freescale_device.disp_width,freescale_device.disp_height,0,0,vf->width,vf->height);  
    return 0;
}
#endif

static struct task_struct *task=NULL;
extern int video_property_notify(int flag);
extern vframe_t* get_cur_dispbuf(void);
static int freescale_task(void *data)
{
    struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };
    ge2d_context_t *context=create_ge2d_work_queue();
    config_para_ex_t ge2d_config;
    memset(&ge2d_config,0,sizeof(config_para_ex_t));

    sched_setscheduler(current, SCHED_FIFO, &param);
    allow_signal(SIGTERM);
    while (down_interruptible(&thread_sem) == 0) {
        if (kthread_should_stop())
            break;

        vframe_t *vf = NULL;       
#ifdef CONFIG_MIX_FREE_SCALE
        if(scaler_pos_changed){
            scaler_pos_changed = 0;
            vf = get_cur_dispbuf();
            if(vf){
                if(process_vf_adjust(vf, context, &ge2d_config)>=0)	
                    EnableVideoLayer();
            }

            vf = vfq_peek(&q_ready);
            while(vf){
                vf = vfq_pop(&q_ready);
                freescale_vf_put(vf);
                vf = vfq_peek(&q_ready);		            		
            }                  		
         	                      
            up(&thread_sem);
            continue;
        }
#endif
        if(still_picture_notify){
            still_picture_notify = 0;
            DisableVideoLayer();		
				
            vf = get_cur_dispbuf();
            if(vf){
                process_vf_change(vf, context, &ge2d_config);	
            }
            video_property_notify(2);
            vfq_lookup_start(&q_ready);
            vf = vfq_peek(&q_ready);

            while(vf){
                vf = vfq_pop(&q_ready);
                process_vf_change(vf, context, &ge2d_config);
                vf = vfq_peek(&q_ready);		            		
            }                  		
            vfq_lookup_end(&q_ready);	
            EnableVideoLayer();            	                      
            up(&thread_sem);
            continue;
        }

        /* process when we have both input and output space */
        while (freescale_vf_peek_dec() && (!vfq_empty(&q_free)) && (!freescale_blocking)) {
            process_vf_rotate(freescale_vf_get_dec(), context, &ge2d_config);
        }
        
        if (freescale_blocking) {
            vf_light_unreg_provider(&freescale_vf_prov);
            vf_local_init();
            vf_reg_provider(&freescale_vf_prov);
            freescale_blocking = false;
            up(&thread_sem);
            printk("freescale rebuild from light-unregister\n");
        }

#ifdef DDD
        printk("process paused, dec %p, free %d, avail %d\n",
            freescale_vf_peek_dec(), vfq_level(&q_free), vfq_level(&q_ready));
#endif
    }

    destroy_ge2d_work_queue(context);

    return 0;
}

/************************************************
*
*   init functions.
*
*************************************************/
static int vout_notify_callback(struct notifier_block *block, unsigned long cmd , void *para)
{
    if (cmd == VOUT_EVENT_MODE_CHANGE)
        freescale_device.vinfo = get_current_vinfo();

    return 0;
}

static struct notifier_block vout_notifier = 
{
    .notifier_call  = vout_notify_callback,
};
int freescale_register()
{    
	vf_freescale_init_provider();
	vf_freescale_init_receiver();
	vf_freescale_reg_receiver();
	vout_register_client(&vout_notifier);
}


int freescale_buffer_init(void)
{
    int i,j;
    u32 canvas_width, canvas_height;
    u32 decbuf_size;
    char* buf_start;
    int buf_size;
    get_freescale_buf_info(&buf_start,&buf_size);   
    freescale_device.vinfo = get_current_vinfo();
    // if (freescale_device.disp_width == 0){
        // freescale_device.disp_width = freescale_device.vinfo->width;
        // if( freescale_device.disp_width >= 1366){
        	// freescale_device.disp_width = 1366 ;	
        // }
    // }

    // if (freescale_device.disp_height == 0){
        // freescale_device.disp_height = freescale_device.vinfo->height;
         // if( freescale_device.disp_height >= 768){
        	// freescale_device.disp_height = 768 ;	
        // }
    // }
	freescale_device.disp_width  = 800 ;
	freescale_device.disp_height = 600 ;
	
//    canvas_width = (freescale_device.disp_width +0xff) & ~0xff;
//    canvas_height = (freescale_device.disp_height+0xff) & ~0xff;
    canvas_width = (freescale_device.disp_width + 0x1f) & ~0x1f;
    canvas_height =( freescale_device.disp_height + 0x1f) & ~0x1f;
    decbuf_size = canvas_width * canvas_height * 3;

    if(decbuf_size*(VF_POOL_SIZE + ASS_POOL_SIZE )>buf_size) {
        amlog_level(LOG_LEVEL_HIGH, "size of freescale memory resource too small.\n");
        return -1;
    }

    for (i = 0; i < VF_POOL_SIZE; i++) {
        canvas_config(FREESCALE_CANVAS_INDEX + i,
                      (ulong)(buf_start + i * decbuf_size),
                      canvas_width*3, canvas_height,
                      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
    }
    
    for(j = 0 ; j< ASS_POOL_SIZE;j++){
        canvas_config(FREESCALE_CANVAS_INDEX + VF_POOL_SIZE + j,
                      (ulong)(buf_start + (VF_POOL_SIZE + j) * decbuf_size),
                      canvas_width*3, canvas_height,
                      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);    	
    }
    
    ass_index =  FREESCALE_CANVAS_INDEX + VF_POOL_SIZE ;
#ifdef CONFIG_MIX_FREE_SCALE
    backup_index = FREESCALE_CANVAS_INDEX + VF_POOL_SIZE + 1;
#endif
    freescale_blocking = false;
    freescale_inited = true;
    sema_init(&thread_sem,1);
    return 0;

}

static int start_freescale_task(void)
{
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 1);
#endif	
	freescale_buffer_init();
    if (!task) {
        vf_local_init();
        task = kthread_run(freescale_task, 0, "freescale");
    }
    task_running  = 1;
    return 0;
}

void stop_freescale_task(void)
{
    if (task) {
        send_sig(SIGTERM, task, 1);
        kthread_stop(task);
        task = NULL;
    }
    task_running = 0 ;
    vf_local_init();
}
