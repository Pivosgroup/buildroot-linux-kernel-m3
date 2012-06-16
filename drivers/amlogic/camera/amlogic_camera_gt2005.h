/*
 * Copyright (C) 2010-2011 AMLOGIC Incorporated.
 */

#ifndef _AMLOGIC_CAMERA_Gt2005_H
#define _AMLOGIC_CAMERA_Gt2005_H


extern void set_camera_saturation_gt2005(enum camera_saturation_e saturation);
extern void set_camera_brightness_gt2005(enum camera_brightness_e brighrness);
extern void set_camera_contrast_gt2005(enum camera_contrast_e contrast);
extern void set_camera_exposure_gt2005(enum camera_exposure_e exposure);
extern void set_camera_mirror_flip_gt2005(enum camera_mirror_flip_e mirror_flip);
extern void power_down_sw_camera_gt2005(void);
extern void wakeup_sw_camera_gt2005(void);
extern void GT2005_Capture(void);
extern void Gt2005_write_regs(void);
extern void Gt2005_read_regs(void);
extern void start_camera_gt2005(void);
extern void stop_camera_gt2005(void);
extern void set_camera_para_gt2005(struct camera_info_s *para);
#endif
