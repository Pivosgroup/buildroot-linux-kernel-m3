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

#ifndef STREAMBUF_REG_H
#define STREAMBUF_REG_H

#define VLD_MEM_VIFIFO_REG_BASE     VLD_MEM_VIFIFO_START_PTR
#define AIU_MEM_AIFIFO_REG_BASE     AIU_MEM_AIFIFO_START_PTR
#define START_PTR   0
#define CURR_PTR    1
#define END_PTR     2
#define BYTES_AVAIL 3
#define CONTROL     4
#define WP          5
#define RP          6
#define LEVEL       7
#define BUF_CTRL    8

#define _WRITE_ST_REG(r, val)             WRITE_MPEG_REG(buf->reg_base + (r), (val))
#define _WRITE_ST_REG_BITS(r, val, s, e)  WRITE_MPEG_REG(buf->reg_base + (r), (val), (s), (e))
#define _SET_ST_REG_MASK(r, val)          SET_MPEG_REG_MASK(buf->reg_base + (r), (val))
#define _CLR_ST_REG_MASK(r, val)          CLEAR_MPEG_REG_MASK(buf->reg_base + (r), (val))
#define _READ_ST_REG(r)                   READ_MPEG_REG(buf->reg_base + (r))

#endif /* STREAMBUF_REG_H */

