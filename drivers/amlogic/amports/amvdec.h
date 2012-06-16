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

#ifndef AMVDEC_H
#define AMVDEC_H

#define UCODE_ALIGN         8
#define UCODE_ALIGN_MASK    7UL

extern  s32 amvdec_loadmc(const u32 *p);

extern void amvdec_start(void);

extern void amvdec_stop(void);

extern void amvdec_enable(void);

extern void amvdec_disable(void);

extern int amvdev_pause(void);
extern int amvdev_resume(void);


#ifdef CONFIG_PM
extern int amvdec_suspend(struct platform_device *dev, pm_message_t event);
extern int amvdec_resume(struct platform_device *dec);
#endif

#endif /* AMVDEC_H */
