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

#ifndef PSPARSER_H
#define PSPARSER_H

extern s32 psparser_init(u32 vid, u32 aid, u32 sid);

extern void psparser_release(void);

extern ssize_t psparser_write(struct file *file,
                              struct stream_buf_s *vbuf,
                              struct stream_buf_s *abuf,
                              const char __user *buf, size_t count);

extern void psparser_change_avid(unsigned int vid, unsigned int aid);

extern void psparser_change_sid(unsigned int sid);

extern void psparser_audio_reset(void);

extern void psparser_sub_reset(void);

extern u8 psparser_get_sub_found_num(void);

extern u8 psparser_get_sub_info(struct subtitle_info *sub_infos[]);

#endif /* PSPARSER_H */

