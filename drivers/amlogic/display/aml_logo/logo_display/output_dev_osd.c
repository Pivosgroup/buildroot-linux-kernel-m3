//we will need ge2d device to transfer logo data ,
//so will also startup ge2d hardware.
#include "logo.h"
#include "logo_dev_osd.h"
#include "dev_ge2d.h"
#include <linux/wait.h>
#include	"amlogo_log.h" 
#include <linux/amlog.h>

#define DisableVideoLayer() \
    do { CLEAR_MPEG_REG_MASK(VPP_MISC, \
         VPP_VD1_PREBLEND | VPP_VD1_POSTBLEND); \
    } while (0)

static  logo_output_dev_t   output_osd0={
	.idx=LOGO_DEV_OSD0,
	.hw_initialized=0,	
	.op={
		.init=osd0_init,
		.transfer=osd_transfer,
		.enable=osd_enable_set,
		.deinit=osd_deinit,
		},
};
static  logo_output_dev_t   output_osd1={
	.idx=LOGO_DEV_OSD1,
	.hw_initialized=0,		
	.op={
		.init=osd1_init,
		.transfer=osd_transfer,
		.enable=osd_enable_set,
		.deinit=osd_deinit,
		},
};
static  inline void  setup_color_mode(const color_bit_define_t *color,u32  reg)
{
	u32  data32;

	data32 =READ_MPEG_REG(reg)&(~(0xf<<8));
	data32 |=  color->hw_blkmode<< 8; /* osd_blk_mode */
	WRITE_MPEG_REG(reg, data32);
}
static inline u32 get_curr_color_depth(u32 reg)
{
	u32  data32=0;

	data32 =(READ_MPEG_REG(reg)>>8)&0xf;
	switch(data32)
	{
		case 4:
		data32=16;
		break;
		case 7:
		data32=24;
		break;
		case 5:
		data32=32;
		break;
	}
	return data32;
}
static int osd_hw_setup(logo_object_t *plogo)
{
	struct osd_ctl_s  osd_ctl;
	const color_bit_define_t  *color;

	osd_ctl.addr=plogo->dev->output_dev.osd.mem_start;
	osd_ctl.index=plogo->dev->idx;
	plogo->dev->output_dev.osd.color_depth=plogo->parser->logo_pic_info.color_info;
	color=&default_color_format_array[plogo->dev->output_dev.osd.color_depth];
	
	osd_ctl.xres=plogo->dev->vinfo->width ;					//logo pic.	
	osd_ctl.yres=plogo->dev->vinfo->height;
	osd_ctl.xres_virtual=plogo->dev->vinfo->width ;
	osd_ctl.yres_virtual=plogo->dev->vinfo->height<<1;
	osd_ctl.disp_start_x=0;
	osd_ctl.disp_end_x=osd_ctl.xres -1;
	osd_ctl.disp_start_y=0;
	osd_ctl.disp_end_y=osd_ctl.yres-1;
	osd_init_hw(0);
	DisableVideoLayer();
	setup_color_mode(color,osd_ctl.index==0?VIU_OSD1_BLK0_CFG_W0:VIU_OSD2_BLK0_CFG_W0);
	
	osd_setup(&osd_ctl, \
					0, \
					0, \
					osd_ctl.xres, \
					osd_ctl.yres, \
					osd_ctl.xres_virtual, \
					osd_ctl.yres_virtual, \
					osd_ctl.disp_start_x, \
					osd_ctl.disp_start_y, \
					osd_ctl.disp_end_x, \
					osd_ctl.disp_end_y, \
					osd_ctl.addr, \
					color, \
					osd_ctl.index) ;
	return SUCCESS;
	
}
static int osd0_init(logo_object_t *plogo)
{
	if(plogo->para.output_dev_type==output_osd0.idx)
	{
		if((plogo->platform_res[output_osd0.idx].mem_end - plogo->platform_res[output_osd0.idx].mem_start) ==0 ) 
		{
			return OUTPUT_DEV_UNFOUND;
		}
		if(plogo->para.loaded)
		{
			osd_init_hw(plogo->para.loaded);
			plogo->para.vout_mode|=VMODE_LOGO_BIT_MASK;
		}
		set_current_vmode(plogo->para.vout_mode);
		output_osd0.vinfo=get_current_vinfo();
		plogo->dev=&output_osd0;
		plogo->dev->window.x=0;
		plogo->dev->window.y=0;
		plogo->dev->window.w=plogo->dev->vinfo->width;
		plogo->dev->window.h=plogo->dev->vinfo->height;
		plogo->dev->output_dev.osd.mem_start=plogo->platform_res[LOGO_DEV_OSD0].mem_start;
		plogo->dev->output_dev.osd.mem_end=plogo->platform_res[LOGO_DEV_OSD0].mem_end;
		plogo->dev->output_dev.osd.color_depth=get_curr_color_depth(VIU_OSD1_BLK0_CFG_W0);//setup by uboot
		return OUTPUT_DEV_FOUND;
	}
	return OUTPUT_DEV_UNFOUND;
}
static int osd1_init(logo_object_t *plogo)
{
	if(plogo->para.output_dev_type==output_osd1.idx)
	{
		if((plogo->platform_res[output_osd1.idx].mem_end - plogo->platform_res[output_osd1.idx].mem_start) ==0)
		{
			return OUTPUT_DEV_UNFOUND;
		}
		if(plogo->para.loaded)
		{
			osd_init_hw(plogo->para.loaded);
			plogo->para.vout_mode|=VMODE_LOGO_BIT_MASK;
		}
		set_current_vmode(plogo->para.vout_mode);
		output_osd1.vinfo=get_current_vinfo();
		plogo->dev=&output_osd1;
		plogo->dev->window.x=0;
		plogo->dev->window.y=0;
		plogo->dev->window.w=plogo->dev->vinfo->width;
		plogo->dev->window.h=plogo->dev->vinfo->height;
		plogo->dev->output_dev.osd.mem_start=plogo->platform_res[LOGO_DEV_OSD1].mem_start;
		plogo->dev->output_dev.osd.mem_end=plogo->platform_res[LOGO_DEV_OSD1].mem_end;
		plogo->dev->output_dev.osd.color_depth=get_curr_color_depth(VIU_OSD2_BLK0_CFG_W0);//setup by uboot
		return OUTPUT_DEV_FOUND;
	}
	return OUTPUT_DEV_UNFOUND;
}

static  int  osd_enable_set(int  enable)
{
	return SUCCESS;
}
static int osd_deinit(void)
{
	return SUCCESS;
}
//just an examble;
static  int  thread_progress(void *para)
{
#define	OFFSET_PROGRESS_X 15
#define  START_PROGRESS_X	70
#define  START_PROGRESS_Y	70
#define  PROGRESS_HEIGHT	25 
#define  PORGRESS_BORDER	3
	unsigned int progress= 0 ;
	unsigned int step =1;
	src_dst_info_t  op_info;
	logo_object_t *plogo=(logo_object_t*)para;
	ge2d_context_t  *context=plogo->dev->ge2d_context;
	wait_queue_head_t  wait_head;

	init_waitqueue_head(&wait_head);
	op_info.color=0x555555ff;
	op_info.dst_rect.x=START_PROGRESS_X+OFFSET_PROGRESS_X;
	op_info.dst_rect.y=plogo->dev->vinfo->height-START_PROGRESS_Y;
	op_info.dst_rect.w=(plogo->dev->vinfo->width -START_PROGRESS_X*2);
	op_info.dst_rect.h=PROGRESS_HEIGHT;
	dev_ge2d_cmd(context,CMD_FILLRECT,&op_info);

	op_info.dst_rect.x+=PORGRESS_BORDER;
	op_info.dst_rect.y+=PORGRESS_BORDER;
	op_info.dst_rect.w=(plogo->dev->vinfo->width -START_PROGRESS_X*2-PORGRESS_BORDER*2)*step/100;
	op_info.dst_rect.h=PROGRESS_HEIGHT -PORGRESS_BORDER*2;
	op_info.color=0x00ffff;
	while(progress < 100)
	{
		dev_ge2d_cmd(context,CMD_FILLRECT,&op_info);
		wait_event_interruptible_timeout(wait_head,0,7);
		progress+=step;
		op_info.dst_rect.x+=op_info.dst_rect.w;
		op_info.color-=(0xff*step/100)<<8; //color change smoothly.
	}
	op_info.dst_rect.w=(plogo->dev->vinfo->width -START_PROGRESS_X-PORGRESS_BORDER+ OFFSET_PROGRESS_X) - op_info.dst_rect.x ;
	dev_ge2d_cmd(context,CMD_FILLRECT,&op_info);
	return 0;
}
static  int  osd_transfer(logo_object_t *plogo)
{
	src_dst_info_t  op_info;
	ge2d_context_t  *context;
	config_para_t	ge2d_config;
	u32  	canvas_index;
	canvas_t	canvas;		
	
	if(!plogo->dev->hw_initialized) // hardware need initialized first .
	{
		if(osd_hw_setup(plogo))
		return  -OUTPUT_DEV_SETUP_FAIL;
		amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"osd hardware initiate success\n");
	}
	if (plogo->need_transfer==FALSE) return -EDEV_NO_TRANSFER_NEED;

	ge2d_config.src_dst_type=plogo->dev->idx?OSD1_OSD1:OSD0_OSD0;
	ge2d_config.alu_const_color=0x000000ff;
	context=dev_ge2d_setup(&ge2d_config);
	//we use ge2d to strechblit pic.
	if(NULL==context) return -OUTPUT_DEV_SETUP_FAIL;
	amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"logo setup ge2d device OK\n");
	plogo->dev->ge2d_context=context;
	//clear dst rect
	op_info.color=0x000000ff;
	op_info.dst_rect.x=0;
	op_info.dst_rect.y=0;
	op_info.dst_rect.w=plogo->dev->vinfo->width;
	op_info.dst_rect.h=plogo->dev->vinfo->height;
	dev_ge2d_cmd(context,CMD_FILLRECT,&op_info);
	amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"fill==dst:%d-%d-%d-%d\n",op_info.dst_rect.x,op_info.dst_rect.y,op_info.dst_rect.w,op_info.dst_rect.h);	

	op_info.src_rect.x=0;  //setup origin src rect 
	op_info.src_rect.y=0;
	op_info.src_rect.w=plogo->parser->logo_pic_info.width;
	op_info.src_rect.h=plogo->parser->logo_pic_info.height;

	switch (plogo->para.dis_mode)
	{
		case DISP_MODE_ORIGIN:
		op_info.dst_rect.x=op_info.src_rect.x;
		op_info.dst_rect.y=op_info.src_rect.y;
		op_info.dst_rect.w=op_info.src_rect.w;
		op_info.dst_rect.h=op_info.src_rect.h;
		break;
		case DISP_MODE_CENTER: //maybe offset is useful
		op_info.dst_rect.x=	(plogo->dev->vinfo->width - plogo->parser->logo_pic_info.width)>>1;
		op_info.dst_rect.y=(plogo->dev->vinfo->height - plogo->parser->logo_pic_info.height)>>1;
		op_info.dst_rect.w=op_info.src_rect.w;
		op_info.dst_rect.h=op_info.src_rect.h;
		break;
		case DISP_MODE_FULL_SCREEN:
		op_info.dst_rect.x=0;
		op_info.dst_rect.y=0;
		op_info.dst_rect.w=plogo->dev->vinfo->width;
		op_info.dst_rect.h=plogo->dev->vinfo->height;
		break;	
	}
	if(strcmp(plogo->parser->name,"bmp")==0)
	{
		//double buffer,bottom part stretchblit to upper part.
		op_info.src_rect.y+= plogo->dev->vinfo->height;
	}
	else if(strcmp(plogo->parser->name,"jpg")==0)
	{// transfer from video layer to osd layer.
		amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"transfer from video layer to osd layer\n");
		ge2d_config.src_dst_type=plogo->dev->idx?ALLOC_OSD1:ALLOC_OSD0;
		ge2d_config.alu_const_color=0x000000ff;
		canvas_index=plogo->parser->decoder.jpg.out_canvas_index;
		if(plogo->parser->logo_pic_info.color_info==24)//we only support this format
		{
			ge2d_config.src_format=GE2D_FORMAT_M24_YUV420;
			ge2d_config.dst_format=GE2D_FORMAT_S24_RGB;
			canvas_read(canvas_index&0xff,&canvas);
			if(canvas.addr==0) return FAIL;
			ge2d_config.src_planes[0].addr = canvas.addr;
			ge2d_config.src_planes[0].w = canvas.width;
			ge2d_config.src_planes[0].h = canvas.height;
			amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"Y:[0x%x][%d*%d]\n",(u32)canvas.addr,canvas.width,canvas.height);
			canvas_read(canvas_index>>8&0xff,&canvas);
			if(canvas.addr==0) return FAIL;
			ge2d_config.src_planes[1].addr = canvas.addr;
			ge2d_config.src_planes[1].w = canvas.width;
			ge2d_config.src_planes[1].h = canvas.height;
			amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"U:[0x%x][%d*%d]\n",(u32)canvas.addr,canvas.width,canvas.height);	
			canvas_read(canvas_index>>16&0xff,&canvas);
			if(canvas.addr==0) return FAIL;	
			ge2d_config.src_planes[2].addr =  canvas.addr;
			ge2d_config.src_planes[2].w = canvas.width;
			ge2d_config.src_planes[2].h = canvas.height;
			amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"V:[0x%x][%d*%d]\n",(u32)canvas.addr,canvas.width,canvas.height);
			context=dev_ge2d_setup(&ge2d_config);

			
		}else{
			amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"can't transfer unsupported jpg format\n");	
			return FAIL;
		}
	}else
	{
		amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"unsupported logo picture format format\n");	
		return FAIL ;
	}
	amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"blit==src:%d-%d-%d-%d\t",op_info.src_rect.x,op_info.src_rect.y,op_info.src_rect.w,op_info.src_rect.h);
	amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"dst:%d-%d-%d-%d\n",op_info.dst_rect.x,op_info.dst_rect.y,op_info.dst_rect.w,op_info.dst_rect.h);	
	amlog_mask_level(LOG_MASK_DEVICE,LOG_LEVEL_LOW,"move logo pic completed\n");
	dev_ge2d_cmd(context,CMD_STRETCH_BLIT,&op_info);

	if(plogo->para.progress) //need progress.
	{
		kernel_thread(thread_progress, plogo, 0);
	}
	return SUCCESS;	
}
int dev_osd_setup(void)
{
	register_logo_output_dev(&output_osd0);
	register_logo_output_dev(&output_osd1);
	return SUCCESS;
}


