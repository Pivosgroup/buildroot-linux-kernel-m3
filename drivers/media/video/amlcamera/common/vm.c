/*******************************************************************
 *
 *  Copyright C 2010 by Amlogic, Inc. All Rights Reserved.
 *
 *  Description:
 *
 *  Author: Amlogic Software
 *  Created: 2010/4/1   19:46
 *
 *******************************************************************/
 
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/vout/vinfo.h>
#include <linux/vout/vout_notify.h>
#include <linux/platform_device.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/canvas.h>
#include <linux/amports/vframe.h>
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
#include <linux/platform_device.h>
#include <linux/ge2d/ge2d_main.h>
#include <linux/ge2d/ge2d.h>
#include "vm_log.h"
#include "vm.h"
#include <linux/amlog.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/videodev2.h>
#include <media/videobuf-core.h>
#include <media/videobuf-dma-contig.h>
#include <media/videobuf-vmalloc.h>
#include <media/videobuf-dma-sg.h>
#include <linux/tvin/tvin.h>
#include <linux/ctype.h>

/*class property info.*/
#include "vmcls.h"

//#define DEBUG
#define MAGIC_SG_MEM 0x17890714
#define MAGIC_DC_MEM 0x0733ac61
#define MAGIC_VMAL_MEM 0x18221223

#define VIDTYPE_3D_LR           0x10000
#define VIDTYPE_3D_BT           0x20000
#define MAX_VM_POOL_SIZE 8
#define MAX_VF_POOL_SIZE 8

/*same as tvin pool*/
static int VM_POOL_SIZE = 6 ; 
static int VF_POOL_SIZE = 6;
static int VM_CANVAS_INDEX = 24;
/*same as tvin pool*/

int vm_skip_count = 0 ; //skip 5 frames from vdin
static int test_zoom = 0;
int gl_vm_skip_count = 0 ;

static inline void vm_vf_put_from_provider(vframe_t *vf);
#define INCPTR(p) ptr_atomic_wrap_inc(&p)

#define VM_DEPTH_16_CANVAS 0x50         //for single canvas use ,RGB16, YUV422,etc

#define VM_DEPTH_24_CANVAS 0x52

#define VM_DEPTH_8_CANVAS_Y  0x54     // for Y/CbCr 4:2:0
#define VM_DEPTH_8_CANVAS_UV 0x56
#define VM_DEPTH_8_CANVAS_U 0x57
#define VM_DEPTH_8_CANVAS_V 0x58
#define VM_DMA_CANVAS_INDEX 0x5e
#define VM_CANVAS_MX 0x5f

#define GE2D_ENDIAN_SHIFT        24
#define GE2D_ENDIAN_MASK            (0x1 << GE2D_ENDIAN_SHIFT)
#define GE2D_BIG_ENDIAN             (0 << GE2D_ENDIAN_SHIFT)
#define GE2D_LITTLE_ENDIAN          (1 << GE2D_ENDIAN_SHIFT)

#define PROVIDER_NAME "vm"
#define RECEIVER_NAME "vm"
static spinlock_t lock = SPIN_LOCK_UNLOCKED;

static inline void ptr_atomic_wrap_inc(u32 *ptr)
{
        u32 i = *ptr;
        i++;
        if (i >= VM_POOL_SIZE)
                i = 0;
        *ptr = i;
}

int start_vm_task(void) ;
int start_simulate_task(void);

static struct vframe_s vfpool[MAX_VF_POOL_SIZE];
/*static u32 vfpool_idx[MAX_VF_POOL_SIZE];*/
static s32 vfbuf_use[MAX_VF_POOL_SIZE];
static s32 fill_ptr, get_ptr, putting_ptr, put_ptr;
struct semaphore  vb_start_sema;
struct semaphore  vb_done_sema;

static inline vframe_t *vm_vf_get_from_provider(void);
static inline vframe_t *vm_vf_peek_from_provider(void);
static inline void vm_vf_put_from_provider(vframe_t *vf);
static vframe_receiver_op_t* vf_vm_unreg_provider(void);
static vframe_receiver_op_t* vf_vm_reg_provider(void);
static void stop_vm_task(void) ;
static int prepare_vframe(vframe_t *vf);


/************************************************
*
*   buffer op for video sink.
*
*************************************************/
static inline u32 index2canvas(u32 index)
{
    int i;
    int start_canvas , count;
    u32 canvas_tab[6] ;
    get_tvin_canvas_info(&start_canvas,&count);
    VM_POOL_SIZE  = count ;
    VF_POOL_SIZE  = count ;
    VM_CANVAS_INDEX = start_canvas;    
    for(i =0 ; i< count;i++){
        canvas_tab[i] =  VM_CANVAS_INDEX +i;       
    }
    return canvas_tab[index];
}

static vframe_t *vm_vf_peek(void *op_arg)
{
	vframe_t *vf = NULL;
	vf = vm_vf_peek_from_provider();
	if(vf){
		if(vm_skip_count > 0){
			vm_skip_count--;	
			vm_vf_get_from_provider();	
			vm_vf_put_from_provider(vf); 
			vf = NULL;						
		}	
	}
    return vf;
}

static vframe_t *vm_vf_get(void *op_arg)
{
    return vm_vf_get_from_provider();
}

static void vm_vf_put(vframe_t *vf, void *op_arg)
{
    prepare_vframe(vf);
}

static int  vm_vf_states(vframe_states_t *states, void *op_arg)
{
    return 0;
}


static vframe_t *local_vf_peek(void)
{
    if (get_ptr == fill_ptr)
        return NULL;
    return &vfpool[get_ptr];
}

static vframe_t *local_vf_get(void)
{
    vframe_t *vf;

    if (get_ptr == fill_ptr)
        return NULL;

    vf = &vfpool[get_ptr];

    INCPTR(get_ptr);

    return vf;
}

static void local_vf_put(vframe_t *vf)
{
    int i;
    int  canvas_addr;
    if(!vf) {
        return;    
    }
    INCPTR(putting_ptr);
    for (i = 0; i < VF_POOL_SIZE; i++) { 
        canvas_addr = index2canvas(i);        
        if(vf->canvas0Addr == canvas_addr ){
            vfbuf_use[i] = 0;   
            vm_vf_put_from_provider(vf); 
        }    
    }           
}


/*static int  local_vf_states(vframe_states_t *states)
{
    unsigned long flags;
    int i;
    spin_lock_irqsave(&lock, flags);
    states->vf_pool_size = VF_POOL_SIZE;

    i = put_ptr - fill_ptr;
    if (i < 0) i += VF_POOL_SIZE;
    states->buf_free_num = i;
    
    i = putting_ptr - put_ptr;
    if (i < 0) i += VF_POOL_SIZE;
    states->buf_recycle_num = i;
    
    i = fill_ptr - get_ptr;
    if (i < 0) i += VF_POOL_SIZE;
    states->buf_avail_num = i;
    
    spin_unlock_irqrestore(&lock, flags);
    return 0;
}*/

static const struct vframe_operations_s vm_vf_provider =
{
    .peek = vm_vf_peek,
    .get  = vm_vf_get,
    .put  = vm_vf_put,
    .vf_states=vm_vf_states,
};
static struct vframe_provider_s vm_vf_prov;

static int vm_receiver_event_fun(int type, void* data, void*);

static vframe_receiver_op_t vm_vf_receiver =
{
    .event_cb = vm_receiver_event_fun
};
static struct vframe_receiver_s vm_vf_recv;
static int vm_receiver_event_fun(int type, void* data, void* private_data)
{
    switch(type){
        case VFRAME_EVENT_PROVIDER_VFRAME_READY:
            //up(&vb_start_sema);
            break;
        case VFRAME_EVENT_PROVIDER_START:
		vf_vm_reg_provider();
			vm_skip_count = gl_vm_skip_count; 
            test_zoom = 0;
            break;
        case VFRAME_EVENT_PROVIDER_UNREG:        
            vm_local_init();
		vf_vm_unreg_provider();
            break;
            
        default:
        break;        
    }    
    return 0;
}

int get_unused_vm_index(void)
{
    int i ;
    for (i = 0; i < VF_POOL_SIZE; i++){
        if(vfbuf_use[i] == 0){
            return i;    
        }        
    }
    return -1;    
}
static int prepare_vframe(vframe_t *vf)
{    
    vframe_t* new_vf;  
    int index ;
    index = get_unused_vm_index();
    if(index < 0 ){
        return -1;    
    }
    new_vf = &vfpool[fill_ptr];
    memcpy(new_vf , vf, sizeof(vframe_t));  
    vfbuf_use[index]++;
    INCPTR(fill_ptr);
    return 0;
}

/*************************************************
*
*   buffer op for decoder, camera, etc. 
*
*************************************************/
//static const vframe_provider_t *vfp = NULL;

void vm_local_init(void) 
{
    int i;
    for(i=0;i<MAX_VF_POOL_SIZE;i++) 
    {
        vfbuf_use[i] = 0;
    }
    fill_ptr=get_ptr=putting_ptr=put_ptr=0;
}

static vframe_receiver_op_t* vf_vm_unreg_provider(void)
{
//    ulong flags;    
//    stop_vm_task();
//    spin_lock_irqsave(&lock, flags); 
//    vfp = NULL;
//    spin_unlock_irqrestore(&lock, flags);
    vf_unreg_provider(&vm_vf_prov);
    return (vframe_receiver_op_t*)NULL;
}
static vframe_receiver_op_t* vf_vm_reg_provider( )
{
    ulong flags;

    spin_lock_irqsave(&lock, flags);
    spin_unlock_irqrestore(&lock, flags);
    
    vf_reg_provider(&vm_vf_prov);
    start_vm_task();   
#if 0   
    start_simulate_task();
#endif    
    
    return &vm_vf_receiver;
}


EXPORT_SYMBOL(vf_vm_reg_provider);
EXPORT_SYMBOL(vf_vm_unreg_provider);


/*static const struct vframe_provider_s * vm_vf_get_vfp_from_provider(void)
{
	return vfp;
} */

static inline vframe_t *vm_vf_peek_from_provider(void)
{
    struct vframe_provider_s *vfp;
    vframe_t *vf;
    vfp = vf_get_provider(RECEIVER_NAME);
    if (!(vfp && vfp->ops && vfp->ops->peek))
        return NULL;
    vf  = vfp->ops->peek(vfp->op_arg);
    return vf;
}

static inline vframe_t *vm_vf_get_from_provider(void)
{
    struct vframe_provider_s *vfp;
    vfp = vf_get_provider(RECEIVER_NAME);
    if (!(vfp && vfp->ops && vfp->ops->peek))
        return NULL;
    return vfp->ops->get(vfp->op_arg);
}

static inline void vm_vf_put_from_provider(vframe_t *vf)
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
static int get_input_format(vframe_t* vf)
{
    int format= GE2D_FORMAT_M24_YUV420;
    if(vf->type&VIDTYPE_VIU_422){
        format =  GE2D_FORMAT_S16_YUV422;    
    }else{
        format =  GE2D_FORMAT_M24_YUV420;
    } 
    return format;
}

static int  get_input_frame(display_frame_t* frame ,vframe_t* vf)
{
	int ret = 0 ;	
	int top, left,  bottom ,right;
	if(!vf){
		return -1;	
	}
	frame->frame_top  =     0;   
	frame->frame_left  =     0 ;   
	frame->frame_width   =  vf->width;
	frame->frame_height   = vf->height;
	ret = get_curren_frame_para(&top ,&left , &bottom, &right);	
	if(ret >= 0 ){
  		frame->content_top     =  top&(~1);
		frame->content_left    =  left&(~1);
		frame->content_width   =  vf->width - 2*frame->content_left ;
		frame->content_height  =  vf->height - 2*frame->content_top;
	}else{
		frame->content_top     = 0;             
		frame->content_left    =  0 ;           
		frame->content_width   = vf->width;     
		frame->content_height  = vf->height   	;
	}
	return 0;
}

static int get_output_format(int v4l2_format)
{
    int format = GE2D_FORMAT_S24_YUV444;
    switch(v4l2_format){
        case V4L2_PIX_FMT_RGB565X:
        format = GE2D_FORMAT_S16_RGB_565;
        break;
        case V4L2_PIX_FMT_YUV444:
        format = GE2D_FORMAT_S24_YUV444;
        break;
        case V4L2_PIX_FMT_VYUY:
        format = GE2D_FORMAT_S16_YUV422;
        break;
        case V4L2_PIX_FMT_BGR24:
        format = GE2D_FORMAT_S24_RGB ;
        break;
        case V4L2_PIX_FMT_RGB24:
        format = GE2D_FORMAT_S24_BGR; 
        break;
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_YUV420:
        format = GE2D_FORMAT_S8_Y;
        break;
        default:
        break;            
    }   
    return format;
}

typedef struct output_para{
    int width;
    int height;
    int bytesperline;
    int v4l2_format;
    int index;
    int v4l2_memory;
    unsigned vaddr;
}output_para_t;

static output_para_t output_para = {0,0,0,0,0};

typedef struct vm_dma_contig_memory {
	u32 magic;
	void *vaddr;
	dma_addr_t dma_handle;
	unsigned long size;
	int is_userptr;
}vm_contig_memory_t;

int is_need_ge2d_pre_process(void)
{
    int ret = 0;
    switch(output_para.v4l2_format){
        case  V4L2_PIX_FMT_RGB565X:
        case  V4L2_PIX_FMT_YUV444:
        case  V4L2_PIX_FMT_VYUY:
        case  V4L2_PIX_FMT_BGR24:      
        case  V4L2_PIX_FMT_RGB24: 
        case  V4L2_PIX_FMT_YUV420:  
        case  V4L2_PIX_FMT_NV12:
        case  V4L2_PIX_FMT_NV21:      
           ret = 1;
           break;
           
        default:
        break;
    }
    return ret;
}

int is_need_sw_post_process(void)
{
    int ret = 0;
    switch(output_para.v4l2_memory){
        case MAGIC_DC_MEM:        
            goto exit;
            break;
        case MAGIC_SG_MEM:
        case MAGIC_VMAL_MEM:           
        default:
            ret = 1;
            break; 
    }  
exit:
    return ret;    
}


int get_canvas_index(int v4l2_format ,int* depth)
{
    int canvas = VM_DEPTH_16_CANVAS ;
    *depth = 16;
    switch(v4l2_format){
        case V4L2_PIX_FMT_RGB565X:
        case V4L2_PIX_FMT_VYUY:
            canvas = VM_DEPTH_16_CANVAS;
            *depth = 16 ;
            break;
        case V4L2_PIX_FMT_YUV444:                
        case V4L2_PIX_FMT_BGR24:      
        case V4L2_PIX_FMT_RGB24:  
            canvas = VM_DEPTH_24_CANVAS;
            *depth = 24;
            break; 
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21: 
			canvas = VM_DEPTH_8_CANVAS_Y|(VM_DEPTH_8_CANVAS_U<<8)|(VM_DEPTH_8_CANVAS_V<<16);
			*depth = 12;   
			break;
        case V4L2_PIX_FMT_YUV420:
			canvas = VM_DEPTH_8_CANVAS_Y|(VM_DEPTH_8_CANVAS_U<<8)|(VM_DEPTH_8_CANVAS_V<<16);
			*depth = 12;
			break;
        default:
        break;            
    }   
    return canvas;        
}

int vm_fill_buffer(struct videobuf_buffer* vb , int v4l2_format , int magic,void* vaddr)
{
    vm_contig_memory_t *mem = NULL;
    char *buf_start,*vbuf_start;
    int buf_size;
    int depth=0;
    int ret = -1;    
    int canvas_index = -1 ;
    struct videobuf_buffer buf={0};
    get_vm_buf_info((const char **)&buf_start,&buf_size,&vbuf_start); 
#if 0    
    if(!vb){
        goto exit;
     }
#else
    if(!vb){
        buf.width = 640;
        buf.height = 480;  
        magic = MAGIC_VMAL_MEM ;
        v4l2_format =  V4L2_PIX_FMT_YUV444 ; 
        vb = &buf;
    }    
#endif             
     switch(magic){
        case   MAGIC_DC_MEM:
            mem = vb->priv;
            canvas_config(VM_DMA_CANVAS_INDEX,
                          mem->dma_handle,
                          vb->bytesperline, vb->height,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);        
            canvas_index =  VM_DMA_CANVAS_INDEX ;
            depth =  (vb->bytesperline <<3)/vb->width;                   
            break;
        case  MAGIC_SG_MEM:           
        case  MAGIC_VMAL_MEM:
            if(buf_start&&buf_size){
                canvas_index = get_canvas_index(v4l2_format,&depth) ;
            }            
            break;
        default:
            canvas_index = VM_DEPTH_16_CANVAS ;
            break;
     }
     output_para.width = vb->width;
     output_para.height = vb->height;
     output_para.bytesperline  = (vb->width *depth)>>3;
     output_para.index = canvas_index ;
     output_para.v4l2_format  = v4l2_format ;
     output_para.v4l2_memory   = magic ;
     output_para.vaddr = (unsigned)vaddr;
     up(&vb_start_sema);
     down_interruptible(&vb_done_sema);        
     ret = 0;  
    return ret;     
}



/*for decoder input processing
    1. output window should 1:1 as source frame size
    2. keep the frame ratio
    3. input format should be YUV420 , output format should be YUV444
*/
int vm_ge2d_pre_process(vframe_t* vf, ge2d_context_t *context,config_para_ex_t* ge2d_config)
{
	int ret;
	int src_top ,src_left ,src_width, src_height;
    canvas_t cs0,cs1,cs2,cd;
    int current_mirror = camera_mirror_flag;
    display_frame_t input_frame={0};
    ret = get_input_frame(&input_frame , vf);
	src_top =     input_frame.content_top   ;
	src_left =    input_frame.content_left  ;
	src_width =   input_frame.content_width ;
	src_height =  input_frame.content_height;
	if(test_zoom){
		test_zoom = 0;
		printk("top is %d , left is %d , width is %d , height is %d\n",input_frame.content_top ,input_frame.content_left,input_frame.content_width,input_frame.content_height);
	}
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
    canvas_read(output_para.index&0xff,&cd);
    ge2d_config->dst_planes[0].addr = cd.addr;
    ge2d_config->dst_planes[0].w = cd.width;
    ge2d_config->dst_planes[0].h = cd.height;
    ge2d_config->src_key.key_enable = 0;
    ge2d_config->src_key.key_mask = 0;
    ge2d_config->src_key.key_mode = 0;
    ge2d_config->src_para.canvas_index=vf->canvas0Addr;
    ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->src_para.format = get_input_format(vf);
    ge2d_config->src_para.fill_color_en = 0;
    ge2d_config->src_para.fill_mode = 0;
    ge2d_config->src_para.x_rev = 0;
    ge2d_config->src_para.y_rev = 0;
    ge2d_config->src_para.color = 0xffffffff;
    ge2d_config->src_para.top = 0;
    ge2d_config->src_para.left = 0;
    ge2d_config->src_para.width = vf->width;
    ge2d_config->src_para.height = vf->height;
//    printk("vf_width is %d , vf_height is %d \n",vf->width ,vf->height);
    ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->dst_para.canvas_index=output_para.index&0xff;
    ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
    ge2d_config->dst_para.format = get_output_format(output_para.v4l2_format)|GE2D_LITTLE_ENDIAN;     
    
  
    ge2d_config->dst_para.fill_color_en = 0;
    ge2d_config->dst_para.fill_mode = 0;
    ge2d_config->dst_para.x_rev = 0;
    ge2d_config->dst_para.y_rev = 0;
    ge2d_config->dst_para.color = 0;
    ge2d_config->dst_para.top = 0;
    ge2d_config->dst_para.left = 0;
    ge2d_config->dst_para.width = output_para.width;
    ge2d_config->dst_para.height = output_para.height;

    if(current_mirror==1){
        ge2d_config->dst_para.x_rev = 1;
        ge2d_config->dst_para.y_rev = 0;
    }else if(current_mirror==2){
        ge2d_config->dst_para.x_rev = 0;
        ge2d_config->dst_para.y_rev = 1;
    }else{
        ge2d_config->dst_para.x_rev = 0;
        ge2d_config->dst_para.y_rev = 0;
    }

//    printk("output_width is %d , output_height is %d \n",output_para.width ,output_para.height);
    if(ge2d_context_config_ex(context,ge2d_config)<0) {
        printk("++ge2d configing error.\n");
        return -1;
    }              
    stretchblt_noalpha(context,src_left ,src_top ,src_width, src_height,0,0,output_para.width,output_para.height);
    
    /* for cr of  yuv420p or yuv420sp. */
    if(output_para.v4l2_format==V4L2_PIX_FMT_YUV420) {
		/* for cb. */
		canvas_read((output_para.index>>8)&0xff,&cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index=(output_para.index>>8)&0xff;
		ge2d_config->dst_para.format=GE2D_FORMAT_S8_CB|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output_para.width/2;
		ge2d_config->dst_para.height = output_para.height/2;

                if(current_mirror==1){
                    ge2d_config->dst_para.x_rev = 1;
                    ge2d_config->dst_para.y_rev = 0;
                }else if(current_mirror==2){
                    ge2d_config->dst_para.x_rev = 0;
                    ge2d_config->dst_para.y_rev = 1;
                }else{
                    ge2d_config->dst_para.x_rev = 0;
                    ge2d_config->dst_para.y_rev = 0;
                }

		if(ge2d_context_config_ex(context,ge2d_config)<0) {
			printk("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context,src_left ,src_top ,src_width, src_height,0,0,ge2d_config->dst_para.width,ge2d_config->dst_para.height);
	} else if (output_para.v4l2_format==V4L2_PIX_FMT_NV12||
			output_para.v4l2_format==V4L2_PIX_FMT_NV21) { 
		canvas_read((output_para.index>>8)&0xff,&cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index=(output_para.index>>8)&0xff;
		ge2d_config->dst_para.format=GE2D_FORMAT_S8_CB|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output_para.width/2;
		ge2d_config->dst_para.height = output_para.height/2;

                if(current_mirror==1){
                    ge2d_config->dst_para.x_rev = 1;
                    ge2d_config->dst_para.y_rev = 0;
                }else if(current_mirror==2){
                    ge2d_config->dst_para.x_rev = 0;
                    ge2d_config->dst_para.y_rev = 1;
                }else{
                    ge2d_config->dst_para.x_rev = 0;
                    ge2d_config->dst_para.y_rev = 0;
                }

		if(ge2d_context_config_ex(context,ge2d_config)<0) {
			printk("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context,src_left ,src_top ,src_width, src_height,0,0,ge2d_config->dst_para.width,ge2d_config->dst_para.height);
	}

	/* for cb of yuv420p or yuv420sp. */
	if(output_para.v4l2_format==V4L2_PIX_FMT_YUV420||
		output_para.v4l2_format==V4L2_PIX_FMT_NV12||
		output_para.v4l2_format==V4L2_PIX_FMT_NV21) {
		canvas_read((output_para.index>>16)&0xff,&cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index=(output_para.index>>16)&0xff;
		ge2d_config->dst_para.format=GE2D_FORMAT_S8_CR|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output_para.width/2;
		ge2d_config->dst_para.height = output_para.height/2;

                if(current_mirror==1){
                    ge2d_config->dst_para.x_rev = 1;
                    ge2d_config->dst_para.y_rev = 0;
                }else if(current_mirror==2){
                    ge2d_config->dst_para.x_rev = 0;
                    ge2d_config->dst_para.y_rev = 1;
                }else{
                    ge2d_config->dst_para.x_rev = 0;
                    ge2d_config->dst_para.y_rev = 0;
                }

		if(ge2d_context_config_ex(context,ge2d_config)<0) {
			printk("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context,src_left ,src_top ,src_width, src_height,0,0,ge2d_config->dst_para.width,ge2d_config->dst_para.height);
	}
    return output_para.index;
}

extern void interleave_uv(unsigned char* pU, unsigned char* pV, unsigned char *pUV, unsigned int size_u_or_v);

int vm_sw_post_process(int canvas , int addr)
{
	int poss=0,posd=0;
	int i=0;
	void __iomem * buffer_y_start;
	void __iomem * buffer_u_start;
	void __iomem * buffer_v_start=0;
	canvas_t canvas_work_y;   
	canvas_t canvas_work_u;
	canvas_t canvas_work_v;
    
    if(!addr){
        return -1;
    }
	
	canvas_read(canvas&0xff,&canvas_work_y);
    buffer_y_start = ioremap_wc(canvas_work_y.addr,canvas_work_y.width*canvas_work_y.height);
    if (output_para.v4l2_format == V4L2_PIX_FMT_BGR24||
			output_para.v4l2_format == V4L2_PIX_FMT_RGB24||
			output_para.v4l2_format== V4L2_PIX_FMT_RGB565X) {
		
    	for(i=0;i<output_para.height;i++) {
    		memcpy((void *)(addr+poss),(void *)(buffer_y_start+posd),output_para.bytesperline);
    		poss+=output_para.bytesperline;
    		posd+= canvas_work_y.width;		
    	}
    } else if (output_para.v4l2_format== V4L2_PIX_FMT_NV12||
			output_para.v4l2_format== V4L2_PIX_FMT_NV21) {
		char* dst_buff=NULL;
		char* src_buff=NULL;
		char* src2_buff=NULL;
    	canvas_read((canvas>>8)&0xff,&canvas_work_u);
    	buffer_u_start = ioremap_wc(canvas_work_u.addr,canvas_work_u.width*canvas_work_u.height);
		
		poss = posd = 0 ;
		for(i=0;i<output_para.height;i+=2) { /* copy y */
    		memcpy((void *)(addr+poss),(void *)(buffer_y_start+posd),output_para.width);
    		poss+=output_para.width;
    		posd+= canvas_work_y.width;	
    		memcpy((void *)(addr+poss),(void *)(buffer_y_start+posd),output_para.width);
    		poss+=output_para.width;
    		posd+= canvas_work_y.width;
    	}	

    	posd=0;
    	canvas_read((canvas>>16)&0xff,&canvas_work_v);
    	buffer_v_start = ioremap_wc(canvas_work_v.addr,canvas_work_v.width*canvas_work_v.height);
        dst_buff= (char*)addr+output_para.width* output_para.height;
        src_buff = (char*)buffer_u_start;
        src2_buff= (char*)buffer_v_start;
        if(output_para.v4l2_format== V4L2_PIX_FMT_NV12) {
			for(i = 0 ;i < output_para.height/2; i++){
				interleave_uv(src_buff, src2_buff, dst_buff, output_para.width/2);
				src_buff +=  canvas_work_u.width;
				src2_buff += 	canvas_work_v.width;	
				dst_buff += output_para.width;
			}
		} else {
			for(i = 0 ;i < output_para.height/2; i++){
				interleave_uv(src2_buff, src_buff, dst_buff, output_para.width/2);
				src_buff +=  canvas_work_u.width;
				src2_buff += 	canvas_work_v.width;	
				dst_buff += output_para.width;
			}
		}
	
    	iounmap(buffer_u_start);
    	iounmap(buffer_v_start);
	} else if (output_para.v4l2_format == V4L2_PIX_FMT_YUV420) {
    	int uv_width = output_para.width>>1;
    	int uv_height = output_para.height>>1;
    	
		posd=0;
		for(i=output_para.height;i>0;i--) { /* copy y */
    		memcpy((void *)(addr+poss),(void *)(buffer_v_start+posd),output_para.width);
    		poss+=output_para.width;
    		posd+= canvas_work_y.width;		
    	}
    	
    	posd=0;
    	canvas_read((canvas>>8)&0xff,&canvas_work_u);
    	buffer_u_start = ioremap_wc(canvas_work_u.addr,canvas_work_u.width*canvas_work_u.height);
    	canvas_read((canvas>>16)&0xff,&canvas_work_v);
    	buffer_v_start = ioremap_wc(canvas_work_v.addr,canvas_work_v.width*canvas_work_v.height);
   		for(i=uv_height;i>0;i--) { /* copy y */
    		memcpy((void *)(addr+poss),(void *)(buffer_u_start+posd),uv_width);
    		poss+=uv_width;
    		posd+= canvas_work_u.width;		
    	}
    	posd=0;
   		for(i=uv_height;i>0;i--) { /* copy y */
    		memcpy((void *)(addr+poss),(void *)(buffer_v_start+posd),uv_width);
    		poss+=uv_width;
    		posd+= canvas_work_v.width;		
    	}
    	iounmap(buffer_u_start);
    	iounmap(buffer_v_start);
	}
	iounmap(buffer_y_start);   
    return 0;
}

static struct task_struct *task=NULL;
static struct task_struct *simulate_task_fd=NULL;

/* static int reset_frame = 1; */
static int vm_task(void *data) {
    vframe_t *vf;
    int src_canvas;
    int timer_count = 0 ;
    ge2d_context_t *context=create_ge2d_work_queue();
    config_para_ex_t ge2d_config;
    memset(&ge2d_config,0,sizeof(config_para_ex_t));
//    printk("vm task is running\n ");
    while(1) {        
//        printk("vm task wait\n");
        down_interruptible(&vb_start_sema);		
        timer_count = 0;
       
		/*wait for frame from 656 provider until 500ms runs out*/        
        while(((vf = local_vf_peek()) == NULL)&&(timer_count < 100)){
            timer_count ++;
            msleep(5);
        }            		
        vf = local_vf_get();
        if(vf){
            src_canvas = vf->canvas0Addr ;
            
			/*here we need translate 422 format to rgb format ,etc*/            
            if(is_need_ge2d_pre_process()){
                src_canvas = vm_ge2d_pre_process(vf,context,&ge2d_config);
            }  
            local_vf_put(vf);         
			
			/*here we need copy the translated data to vmalloc cache*/  
            if(is_need_sw_post_process()){          
                vm_sw_post_process(src_canvas ,output_para.vaddr);
            }
        }
        up(&vb_done_sema); 
    }
    
    destroy_ge2d_work_queue(context);
    return 0;
}

/*simulate v4l2 device to request filling buffer,only for test use*/ 
static int simulate_task(void *data) 
{
    while(1){
        msleep(50);    
        vm_fill_buffer(NULL,0,0,NULL);
        printk("simulate succeed\n");        
    }
    return 0;   
}

/************************************************
*
*   init functions.
*
*************************************************/
int vm_buffer_init(void)
{
    int i;
    u32 canvas_width, canvas_height;
    u32 decbuf_size;
    char *buf_start,*vbuf_start;
    int buf_size;
    int buf_num = 0;
    int local_pool_size = 0 ;
    get_vm_buf_info((const char **)&buf_start,&buf_size,&vbuf_start);
    init_MUTEX_LOCKED(&vb_start_sema);
    init_MUTEX_LOCKED(&vb_done_sema);    
    if(buf_start && buf_size){
        canvas_width = 1920;
        canvas_height = 1200;
        decbuf_size = 0x700000;
        
        buf_num  = buf_size/decbuf_size;
        if(buf_num > 0){
            local_pool_size   = 1;  
        }else{
            local_pool_size = 0 ;
            printk("need at least one buffer to handle 1920*1080 data.\n") ;       
        }   
        for (i = 0; i < local_pool_size; i++) 
        {
            canvas_config((VM_DEPTH_16_CANVAS+i),
                          (unsigned long)(buf_start + i * decbuf_size),
                          canvas_width*2, canvas_height,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
            canvas_config((VM_DEPTH_24_CANVAS+i),
                          (unsigned long)(buf_start + i * decbuf_size),
                          canvas_width*3, canvas_height,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
            canvas_config((VM_DEPTH_8_CANVAS_Y+ i),
                          (unsigned long)(buf_start + i*decbuf_size/2),
                          canvas_width, canvas_height,
                           CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
/*
                         
            canvas_config(VM_DEPTH_8_CANVAS_UV + i,
                          buf_start + (i+1)*decbuf_size/2,
                          canvas_width, canvas_height,
                          canvas_width, canvas_height/2,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
*/                          
            canvas_config((VM_DEPTH_8_CANVAS_U + i),
                          (unsigned long)(buf_start + (i+1)*decbuf_size/2),
                          canvas_width/2, canvas_height/2,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
            canvas_config((VM_DEPTH_8_CANVAS_V + i),
                          (unsigned long)(buf_start + (i+3)*decbuf_size/4),
                          canvas_width/2, canvas_height/2,
                          CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
            vfbuf_use[i] = 0;
        }
    }else{
 /*use external DMA buffer*/   
    
    }
//    printk("done\n");
    return 0;

}

int start_vm_task(void) {
    /* init the device. */
    vm_local_init();
    if(!task){
        task=kthread_create(vm_task,0,"vm");
        if(IS_ERR(task)) {
            amlog_level(LOG_LEVEL_HIGH, "thread creating error.\n");
            return -1;
        }
        wake_up_process(task);    
    }
    return 0;
}

int start_simulate_task(void)
{
    if(!simulate_task_fd){
        simulate_task_fd=kthread_create(simulate_task,0,"vm");
        if(IS_ERR(simulate_task_fd)) {
            amlog_level(LOG_LEVEL_HIGH, "thread creating error.\n");
            return -1;
        }
        wake_up_process(simulate_task_fd);    
    }
    return 0;    
}


void stop_vm_task(void) {
    if(task) kthread_stop(task);
    task=NULL;
    vm_local_init();
}


/***********************************************************************
*
* global status.
*
************************************************************************/

static int vm_enable_flag=0;

int get_vm_status() {
    return vm_enable_flag;
}

void set_vm_status(int flag) {
    if(flag >= 0){ 
        vm_enable_flag=flag;
    }   
    else {
        vm_enable_flag=0;
    }
}

/***********************************************************************
*
* file op section.
*
************************************************************************/

typedef  struct {
	char  			name[20];
	unsigned int 		open_count;
	int	 			major;
	unsigned  int 		dbg_enable;
	struct class 		*cla;
	struct device		*dev;
	char* buffer_start;
	unsigned int buffer_size;
	void __iomem * buffer_v_start;
}vm_device_t;

static vm_device_t  vm_device;


void set_vm_buf_info(char* start,unsigned int size) {
    vm_device.buffer_start=start;
    vm_device.buffer_size=size;
    vm_device.buffer_v_start = ioremap_wc((unsigned long)start,size);
//    printk("#############%x\n",vm_device.buffer_v_start);
}

void get_vm_buf_info(const char** start,unsigned int* size,char** vaddr) {
    *start=vm_device.buffer_start;
    *size=vm_device.buffer_size;
    *vaddr=vm_device.buffer_v_start;
}

static int vm_open(struct inode *inode, struct file *file) 
{
	 ge2d_context_t *context=NULL;
	 amlog_level(LOG_LEVEL_LOW,"open one vm device\n");
	 file->private_data=context;
	 vm_device.open_count++;
	 return 0;
}

static int vm_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long args)
{

	int  ret=0 ;
	ge2d_context_t *context;
	void  __user* argp;
	context=(ge2d_context_t *)filp->private_data;
	argp =(void __user*)args;
	/*config_para_t  ge2d_config={0};*/
	/*ge2d_para_t  para = {0};*/   	

	switch (cmd)
   	{
   	    case VM_IOC_2OSD0:
            break;
        case VM_IOC_ENABLE_PP:
            break;
        case VM_IOC_CONFIG_FRAME:
            break;
		default :
		    return -ENOIOCTLCMD;
		
   	}
 	return ret;
}

static int vm_release(struct inode *inode, struct file *file)
{
	ge2d_context_t *context=(ge2d_context_t *)file->private_data;
	
	if(context && (0==destroy_ge2d_work_queue(context)))
	{
		vm_device.open_count--;

		return 0;
	}
	amlog_level(LOG_LEVEL_LOW,"release one vm device\n");
	return -1;
}

/***********************************************************************
*
* file op init section.
*
************************************************************************/

static const struct file_operations vm_fops = {
	.owner		= THIS_MODULE,
	.open		=vm_open,  
	.ioctl		= vm_ioctl,
	.release		= vm_release, 	
};

int  init_vm_device(void)
{
    int  ret=0;
	
    strcpy(vm_device.name,"vm");
    ret=register_chrdev(0,vm_device.name,&vm_fops);
    if(ret <=0) 
    {
        amlog_level(LOG_LEVEL_HIGH,"register vm device error\r\n");
        return  ret ;
    }
    vm_device.major=ret;
    vm_device.dbg_enable=0;
    amlog_level(LOG_LEVEL_LOW,"vm_dev major:%d\r\n",ret);
    
    if((vm_device.cla = init_vm_cls())==NULL) return -1;
    vm_device.dev=device_create(vm_device.cla,NULL,MKDEV(vm_device.major,0),NULL,vm_device.name);
    if (IS_ERR(vm_device.dev)) {
        amlog_level(LOG_LEVEL_HIGH,"create vm device error\n");
        goto unregister_dev;
    }
    
    if(vm_buffer_init()<0) goto unregister_dev;
        vf_provider_init(&vm_vf_prov, PROVIDER_NAME ,&vm_vf_provider, NULL);	
    //vf_reg_provider(&vm_vf_prov);
    vf_receiver_init(&vm_vf_recv, RECEIVER_NAME, &vm_vf_receiver, NULL);    
    vf_reg_receiver(&vm_vf_recv);
    return 0;

unregister_dev:
    class_unregister(vm_device.cla);
    return -1;
}

int uninit_vm_device(void)
{
    stop_vm_task();
	if(vm_device.cla)
	{
		if(vm_device.dev)
		device_destroy(vm_device.cla, MKDEV(vm_device.major, 0));
	    	class_unregister(vm_device.cla);
	}
	
	unregister_chrdev(vm_device.major, vm_device.name);
	return  0;
}

/*******************************************************************
 * 
 * interface for Linux driver
 * 
 * ******************************************************************/

MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0xff, LOG_LEVEL_DESC, LOG_MASK_DESC);

/* for driver. */
static int vm_driver_probe(struct platform_device *pdev)
{
    char* buf_start;
    unsigned int buf_size;
    struct resource *mem;

    if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0)))
    {
        buf_start = 0 ;
        buf_size = 0;
    }else{
        buf_start = (char *)mem->start;
        buf_size = mem->end - mem->start + 1;
    }
    set_vm_buf_info((char *)mem->start,buf_size);
    init_vm_device();
    return 0;
}

static int vm_drv_remove(struct platform_device *plat_dev)
{
    uninit_vm_device();
    iounmap(vm_device.buffer_v_start);
	return 0;
}

/* general interface for a linux driver .*/
static struct platform_driver vm_drv = {
        .probe  = vm_driver_probe,
        .remove = vm_drv_remove,
        .driver = {
                .name = "vm",
                .owner = THIS_MODULE,
        }
};


static int __init
vm_init_module(void)
{
    int err;

   	amlog_level(LOG_LEVEL_HIGH,"vm_init\n");
	if ((err = platform_driver_register(&vm_drv))) {
        printk(KERN_ERR "Failed to register vm driver (error=%d\n", err);
		return err;
	}

    return err;
}

static void __exit
vm_remove_module(void)
{
    platform_driver_unregister(&vm_drv);
    amlog_level(LOG_LEVEL_HIGH,"vm module removed.\n");
}

module_init(vm_init_module);
module_exit(vm_remove_module);

MODULE_DESCRIPTION("Amlogic Video Input Manager");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Zheng <simon.zheng@amlogic.com>");
