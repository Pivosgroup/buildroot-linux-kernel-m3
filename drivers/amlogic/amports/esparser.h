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

#ifndef ESPARSER_H
#define ESPARSER_H

extern s32 esparser_init(struct stream_buf_s *buf);

extern void esparser_release(struct stream_buf_s *buf);

extern ssize_t esparser_write(struct file *file,
                              struct stream_buf_s *stbuf,
                              const char __user *buf, size_t count);

extern int es_vpts_checkin(struct stream_buf_s *buf, u32 pts);

extern int es_apts_checkin(struct stream_buf_s *buf, u32 pts);

extern void esparser_audio_reset(struct stream_buf_s *buf);

extern void esparser_sub_reset(void);

#endif /* ESPARSER_H */

