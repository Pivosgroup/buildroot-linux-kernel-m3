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

#ifndef VPP_H
#define VPP_H

#define VPP_FLAG_WIDEMODE_MASK      0x00000007
#define VPP_FLAG_INTERLACE_OUT      0x00000010
#define VPP_FLAG_INTERLACE_IN       0x00000020
#define VPP_FLAG_CBCR_SEPARATE      0x00000040
#define VPP_FLAG_ZOOM_SHORTSIDE     0x00000080
#define VPP_FLAG_AR_MASK            0x0003ff00
#define VPP_FLAG_AR_BITS            8
#define VPP_FLAG_PORTRAIT_MODE      0x00040000

#define IDX_H           (2 << 8)
#define IDX_V_Y         (1 << 13)
#define IDX_V_CBCR      ((1 << 13) | (1 << 8))

#define ASPECT_4_3      ((3<<8)/4)
#define ASPECT_16_9     ((9<<8)/16)

typedef enum {
    F2V_IT2IT = 0,
    F2V_IB2IB,
    F2V_IT2IB,
    F2V_IB2IT,
    F2V_P2IT,
    F2V_P2IB,
    F2V_IT2P,
    F2V_IB2P,
    F2V_P2P,
    F2V_TYPE_MAX
} f2v_vphase_type_t;   /* frame to video conversion type */

typedef struct {
    s8 repeat_skip;
    u8 phase;
} f2v_vphase_t;

typedef struct {
    u32  vpp_hf_start_phase_step;
    u32  vpp_hf_start_phase_slope;
    u32  vpp_hf_end_phase_slope;

    const u32 *vpp_vert_coeff;
    const u32 *vpp_horz_coeff;
    u32  vpp_sc_misc_;
    u32  vpp_vsc_start_phase_step;
    u32  vpp_hsc_start_phase_step;
} vppfilter_mode_t;

typedef struct {
    vppfilter_mode_t *top;
    vppfilter_mode_t *bottom;
} vpp_filters_t;

typedef struct {
    u32 VPP_hd_start_lines_;
    u32 VPP_hd_end_lines_;
    u32 VPP_vd_start_lines_;
    u32 VPP_vd_end_lines_;

    u32 VPP_vsc_startp;
    u32 VPP_vsc_endp;

    u32 VPP_hsc_startp;
    u32 VPP_hsc_linear_startp;
    u32 VPP_hsc_linear_endp;
    u32 VPP_hsc_endp;

    u32 VPP_hf_ini_phase_;
    f2v_vphase_t VPP_vf_ini_phase_[9];

    u32 VPP_pic_in_height_;
    u32 VPP_line_in_length_;

    u32 VPP_post_blend_vd_v_start_;
    u32 VPP_post_blend_vd_v_end_;
    u32 VPP_post_blend_vd_h_start_;
    u32 VPP_post_blend_vd_h_end_;
    u32 VPP_post_blend_h_size_;

    vppfilter_mode_t vpp_filter;

    u32 VPP_postproc_misc_;
    u32 vscale_skip_count;
} vpp_frame_par_t;

extern void
vpp_set_filters(u32 wide_mode, vframe_t * vf,
                vpp_frame_par_t * next_frame_par, const vinfo_t *vinfo);

extern void
vpp_set_video_layer_position(s32 x, s32 y, s32 w, s32 h);

extern void
vpp_get_video_layer_position(s32 *x, s32 *y, s32 *w, s32 *h);

extern void
vpp_set_zoom_ratio(u32 r);

extern u32
vpp_get_zoom_ratio(void);

#ifdef CONFIG_AM_VIDEO2
extern void
vpp2_set_filters(u32 wide_mode, vframe_t * vf,
                vpp_frame_par_t * next_frame_par, const vinfo_t *vinfo);

extern void
vpp2_set_video_layer_position(s32 x, s32 y, s32 w, s32 h);

extern void
vpp2_get_video_layer_position(s32 *x, s32 *y, s32 *w, s32 *h);

extern void
vpp2_set_zoom_ratio(u32 r);

extern u32
vpp2_get_zoom_ratio(void);
#endif

#endif /* VPP_H */
