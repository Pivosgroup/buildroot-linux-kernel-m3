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
 * Author:  Chen Zhang <chen.zhang@amlogic.com>
 *
 */

#ifndef RMPARSER_H
#define RMPARSER_H

extern void rm_set_vasid(u32 vid, u32 aid);

extern ssize_t rmparser_write(struct file *file,
                              struct stream_buf_s *vbuf,
                              struct stream_buf_s *abuf,
                              const char __user *buf, size_t count);

s32 rmparser_init(void);

extern void rmparser_release(void);

extern void rm_audio_reset(void);

#endif /* RMPARSER_H */


