/*
 * AMLOGIC Audio/Video streaming port driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/amports/vformat.h>
#include <mach/am_regs.h>


#define MC_SIZE (4096 * 4)

#define SUPPORT_VCODEC_NUM  1
static int inited_vcodec_num = 0;

static struct platform_device *vdec_device = NULL;
struct am_reg {
    char *name;
    int offset;
};

static struct resource amvdec_mem_resource[]  = {
    [0] = {
        .start = 0,
        .end   = 0,
        .flags = 0,
    },
    [1] = {
        .start = 0,
        .end   = 0,
        .flags = 0,
    }
};

static const char *vdec_device_name[] = {
    "amvdec_mpeg12",
    "amvdec_mpeg4",
    "amvdec_h264",
    "amvdec_mjpeg",
    "amvdec_real",
    "amjpegdec",
    "amvdec_vc1",
    "amvdec_avs",
    "amvdec_yuv",
    "amvdec_h264mvc"
};

/*
This function used for change the memory reasource on system run;
It can used for system swap memory for codec and some othor use;
We must call it at the amstream start for register a memory resource
*/
int vdec_set_resource(struct resource *s, void *param)
{
    if (inited_vcodec_num != 0) {
        printk("ERROR:We can't support the change resource at code running\n");
        return -1;
    }

    amvdec_mem_resource[0].start = s->start;
    amvdec_mem_resource[0].end = s->end;
    amvdec_mem_resource[0].flags = s->flags;

    amvdec_mem_resource[1].start = (resource_size_t)param;

    return 0;
}

s32 vdec_init(vformat_t vf)
{
    s32 r;

    if (inited_vcodec_num >= SUPPORT_VCODEC_NUM) {
        printk("We only support the one video code at each time\n");
        return -EIO;
    }

    inited_vcodec_num++;

    if (amvdec_mem_resource[0].flags != IORESOURCE_MEM) {
        printk("no memory resouce for codec,Maybe have not set it\n");
        inited_vcodec_num--;
        return -ENOMEM;
    }

    vdec_device = platform_device_alloc(vdec_device_name[vf], -1);

    if (!vdec_device) {
        printk("vdec: Device allocation failed\n");
        r = -ENOMEM;
        goto error;
    }

    r = platform_device_add_resources(vdec_device, amvdec_mem_resource,
                                      ARRAY_SIZE(amvdec_mem_resource));

    if (r) {
        printk("vdec: Device resource addition failed (%d)\n", r);
        goto error;
    }

    r = platform_device_add(vdec_device);

    if (r) {
        printk("vdec: Device addition failed (%d)\n", r);
        goto error;
    }

    return 0;

error:
    if (vdec_device) {
        platform_device_put(vdec_device);
        vdec_device = NULL;
    }

    inited_vcodec_num--;

    return r;
}

s32 vdec_release(vformat_t vf)
{
    if (vdec_device) {
        platform_device_unregister(vdec_device);
    }

    inited_vcodec_num--;

    vdec_device = NULL;

    return 0;
}

static struct am_reg am_risc[] = {
    {"MSP", 0x300},
    {"MPSR", 0x301 },
    {"MCPU_INT_BASE", 0x302 },
    {"MCPU_INTR_GRP", 0x303 },
    {"MCPU_INTR_MSK", 0x304 },
    {"MCPU_INTR_REQ", 0x305 },
    {"MPC-P", 0x306 },
    {"MPC-D", 0x307 },
    {"MPC_E", 0x308 },
    {"MPC_W", 0x309 }
};

static ssize_t amrisc_regs_show(struct class *class, struct class_attribute *attr, char *buf)
{
    char *pbuf = buf;
    struct am_reg *regs = am_risc;
    int rsize = sizeof(am_risc) / sizeof(struct am_reg);
    int i;
    unsigned  val;

    pbuf += sprintf(pbuf, "amrisc registers show:\n");
    for (i = 0; i < rsize; i++) {
        val = READ_MPEG_REG(regs[i].offset);
        pbuf += sprintf(pbuf, "%s(%#x)\t:%#x(%d)\n",
                        regs[i].name, regs[i].offset, val, val);
    }
    return (pbuf - buf);
}



static struct class_attribute vdec_class_attrs[] = {
    __ATTR_RO(amrisc_regs),
    __ATTR_NULL
};

static struct class vdec_class = {
        .name = "vdec",
        .class_attrs = vdec_class_attrs,
    };

s32 vdec_dev_register(void)
{
    s32 r;

    r = class_register(&vdec_class);
    if (r) {
        printk("vdec class create fail.\n");
        return r;
    }
    return 0;
}
s32 vdec_dev_unregister(void)
{
    class_unregister(&vdec_class);
    return 0;
}


