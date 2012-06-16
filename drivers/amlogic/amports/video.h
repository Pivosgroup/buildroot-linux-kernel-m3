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

#ifndef VIDEO_H
#define VIDEO_H

enum {
    VIDEO_WIDEOPTION_NORMAL           = 0,
    VIDEO_WIDEOPTION_FULL_STRETCH     = 1,
    VIDEO_WIDEOPTION_4_3              = 2,
    VIDEO_WIDEOPTION_16_9             = 3,
    VIDEO_WIDEOPTION_NORMAL_NOSCALEUP = 4,
    VIDEO_WIDEOPTION_MAX              = 5
};

typedef  struct {
    s32 x ;
    s32 y ;
    s32 w ;
    s32 h ;
} disp_rect_t;

#define VIDEO_NOTIFY_TRICK_WAIT   0x01
#define VIDEO_NOTIFY_PROVIDER_GET 0x02
#define VIDEO_NOTIFY_PROVIDER_PUT 0x04
#define VIDEO_NOTIFY_FRAME_WAIT   0x08
#define VIDEO_NOTIFY_POS_CHANGED  0x10

#endif /* VIDEO_H */
