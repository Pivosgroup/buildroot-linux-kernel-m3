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

#ifndef TSDEMUX_H
#define TSDEMUX_H

extern s32 tsdemux_init(u32 vid, u32 aid, u32 sid);

extern void tsdemux_release(void);

extern ssize_t tsdemux_write(struct file *file,
                             struct stream_buf_s *vbuf,
                             struct stream_buf_s *abuf,
                             const char __user *buf, size_t count);

int     tsdemux_class_register(void);
void  tsdemux_class_unregister(void);
void tsdemux_change_avid(unsigned int vid, unsigned int aid);
void tsdemux_change_sid(unsigned int sid);
void tsdemux_audio_reset(void);
void tsdemux_sub_reset(void);
void tsdemux_set_skipbyte(int skipbyte);
void tsdemux_set_demux(int dev);
#endif /* TSDEMUX_H */

