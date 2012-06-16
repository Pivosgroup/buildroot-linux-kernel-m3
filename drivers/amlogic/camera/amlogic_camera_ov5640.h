/*
 * Copyright (C) 2010-2011 AMLOGIC Incorporated.
 */

#ifndef _AMLOGIC_CAMERA_OV5640_H
#define _AMLOGIC_CAMERA_OV5640_H

extern void set_camera_light_mode_ov5640(enum camera_light_mode_e mode);
extern void set_camera_saturation_ov5640(enum camera_saturation_e saturation);
extern void set_camera_brightness_ov5640(enum camera_brightness_e brighrness);
extern void set_camera_contrast_ov5640(enum camera_contrast_e contrast);
extern void set_camera_hue_ov5640(enum camera_hue_e hue);
extern void set_camera_exposure_ov5640(enum camera_exposure_e exposure);
extern void set_camera_sharpness_ov5640(enum camera_sharpness_e sharpness);
extern void set_camera_mirror_flip_ov5640(enum camera_mirror_flip_e mirror_flip);
extern void set_camera_test_mode_ov5640(enum camera_test_mode_e test_mode);
extern void set_camera_resolution_timing_ov5640(enum tvin_sig_fmt_e resolution);
extern void set_camera_resolution_others_ov5640(enum tvin_sig_fmt_e resolution);
extern void reset_sw_camera_ov5640(void);
extern void power_down_sw_camera_ov5640(void);
extern void wakeup_sw_camera_ov5640(void);
extern void set_frame_rate_ov5640(void);
extern void sensor_analog_ctl_ov5640(void);
extern void sensor_sys_ctl_ov5640(void);
extern void sensor_sys_ctl_0_ov5640(void);
extern void sensor_set_color_ov5640(void);
extern void set_camera_light_freq_ov5640(void);
extern void set_other_para1_ov5640(void);
extern void set_other_para2_ov5640(void);
extern void set_other_para3_ov5640(void);
extern void sensor_sys_ctl_1_ov5640(void);
extern void set_camera_gamma_ov5640(void);
extern void set_lenc_para_ov5640(void);
extern void ov5640_write_regs(void);
extern void ov5640_read_regs(void);
extern void start_camera_ov5640(void);
extern void stop_camera_ov5640(void);
extern void set_camera_para_ov5640(struct camera_info_s *para);
#endif /* #define _AMLOGIC_CAMERA_OV5640_H */
