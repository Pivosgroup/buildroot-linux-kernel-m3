/*
 * TVAFE char device driver.
 *
 * Copyright (c) 2010 Bo Yang <bo.yang@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>

/* Amlogic headers */
#include <linux/amports/canvas.h>
#include <mach/am_regs.h>
#include <linux/amports/vframe.h>
/* Local include */
#include "tvin_global.h"
#include "vdin_ctl.h"
#include "tvin_notifier.h"
#include "tvafe_regs.h"

#include "tvafe.h"           /* For user used */
#include "tvafe_general.h"   /* For Kernel used only */
#include "tvafe_adc.h"       /* For Kernel used only */
#include "tvafe_cvd.h"       /* For Kernel used only */

#define TVAFE_NAME               "tvafe"
#define TVAFE_DRIVER_NAME        "tvafe"
#define TVAFE_MODULE_NAME        "tvafe"
#define TVAFE_DEVICE_NAME        "tvafe"
#define TVAFE_CLASS_NAME         "tvafe"

#define TVAFE_COUNT              1

static dev_t                     tvafe_devno;
static struct class              *tvafe_clsp;

typedef struct tvafe_dev_s {
    int                          index;
    struct cdev                  cdev;

} tvafe_dev_t;

static struct tvafe_dev_s *tvafe_devp[TVAFE_COUNT];

static struct tvafe_info_s tvafe_info = {
    0,
    0,
    NULL,
    //signal parameters
    {
        TVIN_PORT_NULL,
        TVIN_SIG_FMT_NULL,
        TVIN_SIG_STATUS_NULL,
        0x81000000,
        0,
    },
    TVAFE_SRC_TYPE_NULL,
    //VGA settings
    0,
    TVAFE_CMD_STATUS_IDLE,
    //adc calibration data
    {
        0,
        0,
        0,
        0,

        0,
        0,
        0,
        0,

        0,
        0,
        0,
        0
    },
    //WSS data
    {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
    },

    //for canvas
    (0xFF - (TVAFE_VF_POOL_SIZE + 2)),
    0,
    {
        VIDTYPE_INTERLACE_BOTTOM,
        VIDTYPE_INTERLACE_TOP,
        VIDTYPE_INTERLACE_BOTTOM,
        VIDTYPE_INTERLACE_TOP,
        VIDTYPE_INTERLACE_BOTTOM,
        VIDTYPE_INTERLACE_TOP
    },
    0x81000000,
    0x70000,
    0,

    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static struct vdin_dev_s *vdin_devp_tvafe = NULL;

//CANVAS module should be moved to vdin
static inline unsigned int tvafe_index2canvas(u32 index)
{
    int tv_group_index ;

    if(vdin_devp_tvafe->index )
        tv_group_index = 1;
    else
        tv_group_index = 0;

    if(index < tvafe_info.canvas_total_count)
        return tvin_canvas_tab[tv_group_index][index];
    else
        return 0xff;

}

static void tvafe_canvas_init(unsigned int mem_start, unsigned int mem_size, enum tvin_port_e port)
{
    unsigned i, canvas_start_index;
    unsigned int canvas_width  = 1920 << 1;
    unsigned int canvas_height = 1080;

    tvafe_info.decbuf_size   = 0x400000;

    if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_VGA7))
    {
        WRITE_CBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000fff);
        canvas_width  = 1600*3;  //tvin_fmt_tbl[tvafe_info.param.fmt].h_active * 3;
        canvas_height = 1200;  //tvin_fmt_tbl[tvafe_info.param.fmt].v_active;
        tvafe_info.decbuf_size   = 0x600000;
    }

    tvafe_info.pbufAddr  = mem_start ;
    i = (unsigned)(mem_size / tvafe_info.decbuf_size);
    if(i % 2)
        i -= 1;     //sure the counter is even

    tvafe_info.canvas_total_count = (TVAFE_VF_POOL_SIZE > i)? i : TVAFE_VF_POOL_SIZE;


    if(vdin_devp_tvafe->index )
        canvas_start_index = tvin_canvas_tab[1][0];
    else
        canvas_start_index = tvin_canvas_tab[0][0];

    pr_info("tvafe: index = %d, port = %x, decbuf_size = %x, canvas_total_count :%x \n",
            vdin_devp_tvafe->index, port, tvafe_info.decbuf_size, tvafe_info.canvas_total_count);

    for ( i = 0; i < tvafe_info.canvas_total_count; i++)
    {
        canvas_config(canvas_start_index + i, mem_start + i * tvafe_info.decbuf_size,
            canvas_width, canvas_height, CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
    }
}

static void tvafe_release_canvas(void)
{
    //to do ...

}

static int tvafe_get_current_video_buff_point(void)
{
    unsigned  cur_canvas_index, ii, tvafe_buff_addr, video_buff_addr = 0;
    if(vdin_devp_tvafe->index)
        cur_canvas_index = (READ_MPEG_REG(VD2_IF0_CANVAS0)) & 0xff ;
    else
        cur_canvas_index = (READ_MPEG_REG(VD1_IF0_CANVAS0)) & 0xff ;

    video_buff_addr = canvas_get_addr(cur_canvas_index);

	 for(ii = 0; ii < tvafe_info.canvas_total_count; ii++)
	 {
        tvafe_buff_addr = tvafe_info.pbufAddr + ii * tvafe_info.decbuf_size;
        if(video_buff_addr == tvafe_buff_addr)
            break;
	 }
    if(ii < tvafe_info.canvas_total_count)
        return(ii);
    else
        return(0xff);		 	//current video don't display tvafe input data
}

static void tvafe_send_interlace_buff_to_display_fifo(vframe_t * info)
{
    info->width = tvin_fmt_tbl[tvafe_info.param.fmt].h_active;
    info->height = tvin_fmt_tbl[tvafe_info.param.fmt].v_active*2;
    info->duration = tvin_fmt_tbl[tvafe_info.param.fmt].duration;
    if((info->width == 0) || (info->height == 0) || (info->duration == 0))
        return;

    tvafe_info.rd_canvas_index++;
    if(tvafe_info.rd_canvas_index  > (tvafe_info.canvas_total_count -1))
    {
           tvafe_info.rd_canvas_index = 0;
           tvafe_info.wrap_flag = 0;
    }

    if((tvafe_info.buff_flag[tvafe_info.rd_canvas_index] & 0xf) == VIDTYPE_INTERLACE_BOTTOM)
        info->type = VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_422 | VIDTYPE_VIU_FIELD | VIDTYPE_INTERLACE_BOTTOM;
    else
        info->type = VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_422 | VIDTYPE_VIU_FIELD | VIDTYPE_INTERLACE_TOP;

    //if (tvafe_info.src_type == TVAFE_SRC_TYPE_VGA )
    //    info->type |= VIDTYPE_VIU_444;
    //else
    //    info->type |= VIDTYPE_VIU_422;

    info->canvas0Addr = info->canvas1Addr = tvafe_index2canvas(tvafe_info.rd_canvas_index);

}

static void tvafe_send_progressive_buff_to_display_fifo(vframe_t * info)
{
    info->width= tvin_fmt_tbl[tvafe_info.param.fmt].h_active;
    info->height = tvin_fmt_tbl[tvafe_info.param.fmt].v_active;
    info->duration = tvin_fmt_tbl[tvafe_info.param.fmt].duration;
    if((info->width == 0) || (info->height == 0) || (info->duration == 0))
        return;

    tvafe_info.rd_canvas_index++;
    if(tvafe_info.rd_canvas_index  > (tvafe_info.canvas_total_count -1))
    {
        tvafe_info.rd_canvas_index = 0;
        tvafe_info.wrap_flag = 0;
    }

    info->canvas0Addr = info->canvas1Addr = tvafe_index2canvas(tvafe_info.rd_canvas_index);

    info->type = VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD | VIDTYPE_PROGRESSIVE;

    if (tvafe_info.src_type == TVAFE_SRC_TYPE_VGA ) {
            info->type |= VIDTYPE_VIU_444;
    } else
        info->type |= VIDTYPE_VIU_422;

    info->canvas0Addr = info->canvas1Addr = tvafe_index2canvas(tvafe_info.rd_canvas_index);

    return;
}


static void tvafe_wr_vdin_canvas(void)
{
    unsigned char canvas_id;
    tvafe_info.wr_canvas_index++;

    if(tvafe_info.wr_canvas_index > (tvafe_info.canvas_total_count -1) )
    {
        tvafe_info.wr_canvas_index = 0;
        tvafe_info.wrap_flag = 1;
    }
    canvas_id = tvafe_index2canvas(tvafe_info.wr_canvas_index);

    vdin_set_canvas_id(vdin_devp_tvafe->index, canvas_id);

}


static void tvafe_vga_pinmux_enable(void)
{
    // diable TCON
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_7, 0,  1, 1);
    // diable DVIN
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 27, 1);
    // HS0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 15, 1);
    // VS0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 14, 1);
    // DDC_SDA0
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 13, 1);
    // DDC_SCL0
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 1, 12, 1);
}

static void tvafe_vga_pinmux_disable(void)
{
    // HS0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 15, 1);
    // VS0
    WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 14, 1);
    // DDC_SDA0
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 13, 1);
    // DDC_SCL0
    //WRITE_CBUS_REG_BITS(PERIPHS_PIN_MUX_6, 0, 12, 1);
}
#ifdef VDIN_FIXED_FMT_TEST
static int tvafe_start_dec(unsigned int mem_start, unsigned int mem_size, tvin_port_t port, tvin_sig_fmt_t fmt)
#else
static int tvafe_start_dec(unsigned int mem_start, unsigned int mem_size, tvin_port_t port)
#endif
{
    int ret = 0;

    pr_info("tvafe: tvafe_start_dec.\n");

    /** init canvas and variables**/
    //init canvas
    tvafe_canvas_init(mem_start, mem_size, port);

    tvafe_info.rd_canvas_index = 0xFF - (tvafe_info.canvas_total_count + 2);
    tvafe_info.wr_canvas_index = 0;
    tvafe_info.param.port = port;
    if (tvafe_info.param.port == TVIN_PORT_VGA0)
        tvafe_vga_pinmux_enable();


    tvafe_reset_module(tvafe_info.param.port);
    tvafe_init_state_handler();
    if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_VGA7))
        tvafe_set_vga_default(TVIN_SIG_FMT_VGA_1024X768P_60D004);
    else if ((port >= TVIN_PORT_COMP0) && (port <= TVIN_PORT_COMP7))
        tvafe_set_comp_default(TVIN_SIG_FMT_COMP_720P_59D940);
    else //if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS7))
    {
#ifdef VDIN_FIXED_FMT_TEST
        tvafe_set_cvd2_default(tvafe_info.cvd2_mem_addr, tvafe_info.cvd2_mem_size, fmt);
#else
        tvafe_set_cvd2_default(tvafe_info.cvd2_mem_addr, tvafe_info.cvd2_mem_size,
                        TVIN_SIG_FMT_CVBS_PAL_I);
#endif

    }

    ret = tvafe_source_muxing(&tvafe_info);
    pr_info("tvafe: tvafe_start_dec finished \n");

    return ret;

}

static void tvafe_stop_dec(void)
{
    pr_info("tvafe: tvafe_stop_dec.\n");

    if (tvafe_info.param.port == TVIN_PORT_VGA0)
        tvafe_vga_pinmux_disable();
    if ((tvafe_info.param.port >= TVIN_PORT_CVBS0) &&
        (tvafe_info.param.port <= TVIN_PORT_CVBS7))
	    WRITE_APB_REG_BITS(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);
    //tvafe_stop_module(&tvafe_info);
    tvafe_release_canvas();
    tvafe_info.param.fmt = TVIN_SIG_FMT_NULL;
    tvafe_info.param.status = TVIN_SIG_STATUS_NULL;
    tvafe_info.param.cap_addr = 0;
    tvafe_info.param.flag = 0;
    //need to do ...

}

/*as use the spin_lock,
 *1--there is no sleep,
 *2--it is better to shorter the time,
*/
static int tvafe_isr_run(struct vframe_s *vf_info)
{
    unsigned int canvas_id = 0;
    unsigned char field_status = 0, last_recv_index = 0, cur_canvas_index, next_buffs_index;
    enum tvin_scan_mode_e scan_mode = 0;
	/* Fetch WSS data */

	/* Monitoring CVBS amplitude */

    //video frame info setting
    if ((tvafe_info.param.status != TVIN_SIG_STATUS_STABLE) ||
        (tvafe_info.param.fmt == TVIN_SIG_FMT_NULL)) {
        return 0;
    }
    switch (tvafe_info.src_type) {
        case TVAFE_SRC_TYPE_CVBS:
		case TVAFE_SRC_TYPE_SVIDEO:
			/* TVAFE CVD2 3D works abnormally => reset cvd2 */
            if(tvafe_info.param.status == TVIN_SIG_STATUS_STABLE)
			    tvafe_check_cvbs_3d_comb();
			break;
		case TVAFE_SRC_TYPE_VGA:
		case TVAFE_SRC_TYPE_COMP:
			break;
		case TVAFE_SRC_TYPE_SCART:
			break;
		default:
			break;
    }

    tvafe_vga_vs_cnt();


    if ((vdin_devp_tvafe->para.flag & TVIN_PARM_FLAG_CAP) != 0)  //frame capture function
    {
#if 1
        if (tvafe_info.wr_canvas_index == 0)
            last_recv_index = tvafe_info.canvas_total_count - 1;
        else
            last_recv_index = tvafe_info.wr_canvas_index - 1;

        if (last_recv_index != tvafe_info.rd_canvas_index)
            canvas_id = last_recv_index;
        else
        {
            if(tvafe_info.wr_canvas_index == tvafe_info.canvas_total_count - 1)
                canvas_id = 0;
            else
                canvas_id = tvafe_info.wr_canvas_index + 1;
        }
#else
        canvas_id = tvafe_info.rd_canvas_index; //kernel & user don't use the same memory
#endif
        vdin_devp_tvafe->para.cap_addr = tvafe_info.pbufAddr +
                    (tvafe_info.decbuf_size * canvas_id);
         vdin_devp_tvafe->para.cap_size = tvafe_info.decbuf_size;
         vdin_devp_tvafe->para.canvas_index= canvas_id;
        return -1;
    }

    scan_mode = tvin_fmt_tbl[tvafe_info.param.fmt].scan_mode;

    if (scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) {
        if ((tvafe_info.rd_canvas_index > 0xF0) && (tvafe_info.rd_canvas_index < 0xFF))
        {
            tvafe_info.rd_canvas_index++;
            tvafe_wr_vdin_canvas();
            return 0;
        }
        else if (tvafe_info.rd_canvas_index == 0xFF)
        {
            tvafe_info.rd_canvas_index = 0;
            tvafe_info.wrap_flag = 0;
        }
        else
        {
            cur_canvas_index = tvafe_get_current_video_buff_point();  //get current display index
            if (tvafe_info.wr_canvas_index >= (tvafe_info.canvas_total_count -1) )
            {
                next_buffs_index = 0;
            }
            else
            {
                next_buffs_index = tvafe_info.wr_canvas_index + 1;
            }

            if(((tvafe_info.wr_canvas_index == cur_canvas_index) || (next_buffs_index == cur_canvas_index)) &&
               (tvafe_info.wrap_flag == 1))       //avoid the overflow
            {
                tvafe_send_progressive_buff_to_display_fifo(vf_info);
                return 0;
            }

            if (tvafe_info.rd_canvas_index > (tvafe_info.canvas_total_count - 1))
                next_buffs_index = 0;
            else
                next_buffs_index = tvafe_info.rd_canvas_index + 1;

            if ((next_buffs_index == tvafe_info.wr_canvas_index) &&
                (tvafe_info.wrap_flag == 0))   //avoid the underflow
            {
                tvafe_wr_vdin_canvas();
                return 0;
            }
        }
        tvafe_send_progressive_buff_to_display_fifo(vf_info);
        tvafe_wr_vdin_canvas();

    }
    else {
        field_status = READ_CBUS_REG_BITS(VDIN_COM_STATUS0, 0, 1);
        if (tvafe_info.wr_canvas_index == 0)
            last_recv_index = tvafe_info.canvas_total_count - 1;
        else
            last_recv_index = tvafe_info.wr_canvas_index - 1;

        if (field_status == 1)
        {
            tvafe_info.buff_flag[tvafe_info.wr_canvas_index] = VIDTYPE_INTERLACE_BOTTOM;
            if((tvafe_info.buff_flag[last_recv_index] & 0x0f) == VIDTYPE_INTERLACE_BOTTOM)
            {
                return 0;
            }
        }
        else
        {
            tvafe_info.buff_flag[tvafe_info.wr_canvas_index] = VIDTYPE_INTERLACE_TOP;
            if((tvafe_info.buff_flag[last_recv_index] & 0x0f) == VIDTYPE_INTERLACE_TOP)
            {
                return 0;
            }
        }

        if((tvafe_info.rd_canvas_index > 0xF0) && (tvafe_info.rd_canvas_index < 0xFF))
        {
            tvafe_info.rd_canvas_index++;
            tvafe_wr_vdin_canvas();
            return 0;
        }
        else if (tvafe_info.rd_canvas_index == 0xFF)
        {
            tvafe_info.rd_canvas_index = 0;
            tvafe_info.wrap_flag = 0;
        }
        else
        {
            cur_canvas_index = tvafe_get_current_video_buff_point();
            if(tvafe_info.wr_canvas_index >= (tvafe_info.canvas_total_count -1) )
            {
                next_buffs_index = 0;
            }
            else
            {
                next_buffs_index = tvafe_info.wr_canvas_index + 1;
            }

            if(((tvafe_info.wr_canvas_index == cur_canvas_index) || (next_buffs_index == cur_canvas_index))
                && (tvafe_info.wrap_flag == 1))       //avoid the overflow
            {
                tvafe_send_interlace_buff_to_display_fifo(vf_info);
                return 0;
            }

             if(tvafe_info.rd_canvas_index  > (tvafe_info.canvas_total_count -1))
                next_buffs_index = 0;
             else
                next_buffs_index = tvafe_info.rd_canvas_index + 1;

             if((next_buffs_index == tvafe_info.wr_canvas_index) && (tvafe_info.wrap_flag == 0))   //avoid the underflow
             {
                tvafe_wr_vdin_canvas();
                return 0;
             }

            tvafe_send_interlace_buff_to_display_fifo(vf_info);
            tvafe_wr_vdin_canvas();
        }
    }

    // fetch WSS data
    tvafe_get_wss_data(&tvafe_info.comp_wss);

    return 0;
}




//static int cnt = 0;
static void check_tvafe_format(void )
{

    if (vdin_devp_tvafe == NULL)  //check tvafe status
    {
        pr_info("tvafe vdin_devp_tvafe == NULL\n");
        return;
    }

    //get frame capture flag from vdin
    tvafe_info.param.flag = vdin_devp_tvafe->para.flag;

    tvafe_run_state_handler(&tvafe_info);
    if (tvafe_info.vga_auto_flag == 1)
    {
        tvafe_vga_auto_adjust_handler(&tvafe_info);
        if ((tvafe_info.cmd_status == TVAFE_CMD_STATUS_FAILED) ||
            (tvafe_info.cmd_status == TVAFE_CMD_STATUS_SUCCESSFUL)
           )
            tvafe_info.vga_auto_flag = 0;
    }



    return;
}

static int tvafe_check_callback(struct notifier_block *block, unsigned long cmd , void *para)
{

    int ret = 0;
    switch(cmd)
    {
        case TVIN_EVENT_INFO_CHECK:
            if(vdin_devp_tvafe != NULL)
            {
                //cvd agc handler
                if (tvafe_info.src_type == TVAFE_SRC_TYPE_CVBS)
                    tvafe_cvd2_video_agc_handler(&tvafe_info);

                check_tvafe_format();
                if ((tvafe_info.param.fmt != vdin_devp_tvafe->para.fmt) ||
                    (tvafe_info.param.status != vdin_devp_tvafe->para.status))
                {
                    tvafe_set_fmt(&tvafe_info);
                    vdin_info_update(vdin_devp_tvafe, &tvafe_info.param);
                }
                ret = NOTIFY_STOP_MASK;
            }
            break;
        default:
            //pr_dbg("command is not exit ./n");
            break;
    }
    return ret;
}

static struct notifier_block tvafe_check_notifier = {
    .notifier_call  = tvafe_check_callback,
};

struct tvin_dec_ops_s tvafe_op = {
    .dec_run = tvafe_isr_run,
};

static int tvafe_notifier_callback(struct notifier_block *block,
                                        unsigned long cmd , void *para)
{
    int ret = 0;

    vdin_dev_t *p = NULL;

    pr_info("tvafe: start listen event\n");
    switch(cmd)
    {
        case TVIN_EVENT_DEC_START:
            if(para != NULL)
            {
                p = (vdin_dev_t*)para;
                if ((p->para.port < TVIN_PORT_VGA0) ||
                    (p->para.port > TVIN_PORT_SVIDEO7))
                {
                    pr_info("tvafe: ignore event TVIN_EVENT_DEC_START(port=%d)\n", p->para.port);
                    return 0;
                }
                pr_info("tvafe: start to response for event TVIN_EVENT_DEC_START(port=%d)\n", p->para.port);
                vdin_devp_tvafe = p;
#ifdef VDIN_FIXED_FMT_TEST
                if (tvafe_start_dec(p->mem_start, p->mem_size, p->para.port, p->para.fmt) < 0) {
#else
                if (tvafe_start_dec(p->mem_start, p->mem_size, p->para.port) < 0) {
#endif
                    pr_info("tvafe start error\n");
                    vdin_devp_tvafe = NULL;
                    return 0;
                }
                tvin_dec_register(p, &tvafe_op);
                tvin_check_notifier_register(&tvafe_check_notifier);
                ret = NOTIFY_STOP_MASK;
            }
            break;

        case TVIN_EVENT_DEC_STOP:
            if(para != NULL)
            {
                p = (vdin_dev_t*)para;
                if ((p->para.port < TVIN_PORT_VGA0) || (p->para.port > TVIN_PORT_SVIDEO7))
                {
                    pr_info("tvafe: ignore TVIN_EVENT_DEC_STOP, port=%d \n", p->para.port);
                    return ret;
                }

                tvafe_stop_dec();
                tvin_dec_unregister(vdin_devp_tvafe);
                tvin_check_notifier_unregister(&tvafe_check_notifier);
                vdin_devp_tvafe = NULL;
                ret = NOTIFY_STOP_MASK;
            }
            break;
    }

    return ret;
}

static struct notifier_block tvafe_notifier = {
    .notifier_call  = tvafe_notifier_callback,
};


static int tvafe_open(struct inode *inode, struct file *file)
{
    tvafe_dev_t *devp;

    /* Get the per-device structure that contains this cdev */
    devp = container_of(inode->i_cdev, tvafe_dev_t, cdev);
    file->private_data = devp;

    /* ... */

    return 0;
}

static int tvafe_release(struct inode *inode, struct file *file)
{
    tvafe_dev_t *devp = file->private_data;

    file->private_data = NULL;

    /* Release some other fields */
    /* ... */

    pr_info(KERN_ERR "tvafe: device %d release ok.\n", devp->index);

    return 0;
}


static int tvafe_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    enum tvafe_cvbs_video_e cvbs_lock_status = TVAFE_CVBS_VIDEO_UNLOCKED;
    void __user *argp = (void __user *)arg;

    struct tvafe_vga_edid_s edid;

    switch (cmd)
    {
        case TVIN_IOC_S_AFE_ADC_CAL:
            if (copy_from_user(&tvafe_info.adc_cal_val, argp, sizeof(struct tvafe_adc_cal_s)))
			{
				ret = -EFAULT;
			}
            else
            {
                tvafe_vga_auto_adjust_disable(&tvafe_info);
                tvafe_set_cal_value(&tvafe_info.adc_cal_val);
            }
            break;

		case TVIN_IOC_G_AFE_ADC_CAL:
            tvafe_get_cal_value(&tvafe_info.adc_cal_val);
			if (copy_to_user(argp, &tvafe_info.adc_cal_val, sizeof(struct tvafe_adc_cal_s)))
			{
				ret = -EFAULT;
			}

            break;
		case TVIN_IOC_G_AFE_COMP_WSS:
			if (copy_to_user(argp, &tvafe_info.comp_wss, sizeof(struct tvafe_comp_wss_s)))
			{
				ret = -EFAULT;
			}
			break;
        case TVIN_IOC_S_AFE_VGA_EDID:
            if (copy_from_user(&edid, argp, sizeof(struct tvafe_vga_edid_s)))
            {
                ret = -EFAULT;
            }
            else
            {
                tvafe_vga_set_edid(&edid);
            }
            break;
        case TVIN_IOC_G_AFE_VGA_EDID:
            tvafe_vga_get_edid(&edid);
            if (copy_to_user(argp, &edid, sizeof(struct tvafe_vga_edid_s)))
			{
				ret = -EFAULT;
			}
            break;
        case TVIN_IOC_S_AFE_VGA_PARM:
        {
            struct tvafe_vga_parm_s vga_parm = {0, 0, 0, 0, 0};
            if (copy_from_user(&vga_parm, argp, sizeof(struct tvafe_vga_parm_s)))
			{
				ret = -EFAULT;
			}
            else
            {
                tvafe_vga_auto_adjust_disable(&tvafe_info);
                tvafe_adc_set_param(&vga_parm, &tvafe_info);
            }
            break;
        }
        case TVIN_IOC_G_AFE_VGA_PARM:
        {
            struct tvafe_vga_parm_s vga_parm = {0, 0, 0, 0, 0};
            tvafe_adc_get_param(&vga_parm, &tvafe_info);
            if (copy_to_user(argp, &vga_parm, sizeof(struct tvafe_vga_parm_s)))
			{
				ret = -EFAULT;
			}
            break;
        }
        case TVIN_IOC_S_AFE_VGA_AUTO:
            ret = tvafe_vga_auto_adjust_enable(&tvafe_info);
            break;
        case TVIN_IOC_G_AFE_CMD_STATUS:
            if (copy_to_user(argp, &tvafe_info.cmd_status, sizeof(enum tvafe_cmd_status_e)))
			{
				ret = -EFAULT;
			}
			else
			{
			    if ((tvafe_info.cmd_status == TVAFE_CMD_STATUS_SUCCESSFUL)
                    || (tvafe_info.cmd_status == TVAFE_CMD_STATUS_FAILED)
                    || (tvafe_info.cmd_status == TVAFE_CMD_STATUS_TERMINATED)
                   )
			    {
			        tvafe_info.cmd_status = TVAFE_CMD_STATUS_IDLE;
			    }
			}
            break;
        case TVIN_IOC_G_AFE_CVBS_LOCK:
            cvbs_lock_status = tvafe_cvd2_video_locked();
            if (copy_to_user(argp, &cvbs_lock_status, sizeof(int)))
			{
				ret = -EFAULT;
			}
            break;

        case TVIN_IOC_S_CVD2_CORDIC:
            if(tvafe_cvd2_set_cordic_para(argp))
                ret = -EFAULT;
            break;

         case TVIN_IOC_G_CVD2_CORDIC:
             if(tvafe_cvd2_get_cordic_para(argp))
                 ret = -EFAULT;
            break;

        default:
            ret = -ENOIOCTLCMD;
            break;
    }
    return ret;
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations tvafe_fops = {
    .owner   = THIS_MODULE,         /* Owner */
    .open    = tvafe_open,          /* Open method */
    .release = tvafe_release,       /* Release method */
    .ioctl   = tvafe_ioctl,         /* Ioctl method */
    /* ... */
};

static int tvafe_probe(struct platform_device *pdev)
{
    int ret;
    int i;
    struct device *devp;
    struct resource *res;


    pr_info("tvafe: tvafe_probe start...\n");

    ret = alloc_chrdev_region(&tvafe_devno, 0, TVAFE_COUNT, TVAFE_NAME);
	if (ret < 0) {
		pr_info(KERN_ERR "tvafe: failed to allocate major number\n");
		return 0;
	}

    tvafe_clsp = class_create(THIS_MODULE, TVAFE_NAME);
    if (IS_ERR(tvafe_clsp))
    {
        pr_info(KERN_ERR "tvafe: can't get tvafe_clsp\n");
        unregister_chrdev_region(tvafe_devno, TVAFE_COUNT);
        return PTR_ERR(tvafe_clsp);
	}

    /* @todo do with resources */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res)
    {
        pr_info(KERN_ERR "tvafe: can't get memory resource\n");
        return -EFAULT;
    }

    for (i = 0; i < TVAFE_COUNT; ++i)
    {
        /* allocate memory for the per-device structure */
        tvafe_devp[i] = kmalloc(sizeof(struct tvafe_dev_s), GFP_KERNEL);
        if (!tvafe_devp[i])
        {
            pr_info(KERN_ERR "tvafe: failed to allocate memory for tvafe device\n");
            return -ENOMEM;
        }
        tvafe_devp[i]->index = i;

        /* connect the file operations with cdev */
        cdev_init(&tvafe_devp[i]->cdev, &tvafe_fops);
        tvafe_devp[i]->cdev.owner = THIS_MODULE;
        /* connect the major/minor number to the cdev */
        ret = cdev_add(&tvafe_devp[i]->cdev, (tvafe_devno + i), 1);
	    if (ret) {
    		pr_info(KERN_ERR "tvafe: failed to add device\n");
            /* @todo do with error */
    		return ret;
    	}
        /* create /dev nodes */
        devp = device_create(tvafe_clsp, NULL, MKDEV(MAJOR(tvafe_devno), i),
                            NULL, "tvafe%d", i);
        if (IS_ERR(devp)) {
            pr_info(KERN_ERR "tvafe: failed to create device node\n");
            class_destroy(tvafe_clsp);
            /* @todo do with error */
            return PTR_ERR(devp);;
	   }
#if 1
        /* get device memory */
        res = platform_get_resource(pdev, IORESOURCE_MEM, i);
        if (!res) {
            pr_info(KERN_ERR "tvafe: can't get memory resource\n");
            return -EFAULT;
        }
        tvafe_info.cvd2_mem_addr = res->start;
        tvafe_info.cvd2_mem_size  = res->end - res->start + 1;
        pr_info("CVD: cvd2_mem_addr=0x%x cvd2_mem_size=0x%x\n",
                            tvafe_info.cvd2_mem_addr, tvafe_info.cvd2_mem_size);
#endif
	    tvafe_info.pinmux = pdev->dev.platform_data;
	    if (!tvafe_info.pinmux) {
		    pr_info(" rvafe no platform_data!\n");
		    ret = -ENODEV;
	    }

    }

    tvin_dec_notifier_register(&tvafe_notifier);

    pr_info(KERN_INFO "tvafe: driver probe ok\n");

    return 0;
}

static int tvafe_remove(struct platform_device *pdev)
{
    int i = 0;

    tvin_dec_notifier_unregister(&tvafe_notifier);

    for (i = 0; i < TVAFE_COUNT; ++i)
    {
        device_destroy(tvafe_clsp, MKDEV(MAJOR(tvafe_devno), i));
        cdev_del(&tvafe_devp[i]->cdev);
        kfree(tvafe_devp[i]);
    }
    class_destroy(tvafe_clsp);
    unregister_chrdev_region(tvafe_devno, TVAFE_COUNT);

    pr_info(KERN_ERR "tvafe: driver removed ok.\n");

    return 0;
}

static struct platform_driver tvafe_driver = {
    .probe      = tvafe_probe,
    .remove     = tvafe_remove,
    .driver     = {
        .name   = TVAFE_DRIVER_NAME,
    }
};

static int __init tvafe_init(void)
{
    int ret = 0;

    ret = platform_driver_register(&tvafe_driver);
    if (ret != 0) {
        pr_info(KERN_ERR "failed to register tvafe module, error %d\n", ret);
        return -ENODEV;
    }

    pr_info("tvafe: tvafe_init.\n");
    return ret;
}

static void __exit tvafe_exit(void)
{
    platform_driver_unregister(&tvafe_driver);
    pr_info("tvafe: tvafe_exit.\n");
}

module_init(tvafe_init);
module_exit(tvafe_exit);

MODULE_DESCRIPTION("AMLOGIC TVAFE driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");

