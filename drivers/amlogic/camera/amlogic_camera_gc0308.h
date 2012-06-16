/*
 * Copyright (C) 2010-2011 AMLOGIC Incorporated.
 */

#ifndef _AMLOGIC_CAMERA_GC0308_H
#define _AMLOGIC_CAMERA_GC0308_H


extern void set_camera_saturation_gc0308(enum camera_saturation_e saturation);
extern void set_camera_brightness_gc0308(enum camera_brightness_e brighrness);
extern void set_camera_contrast_gc0308(enum camera_contrast_e contrast);
extern void set_camera_exposure_gc0308(enum camera_exposure_e exposure);
extern void set_camera_mirror_flip_gc0308(enum camera_mirror_flip_e mirror_flip);
extern void power_down_sw_camera_gc0308(void);
extern void wakeup_sw_camera_gc0308(void);
extern void GC0308_write_regs(void);
extern void GC0308_read_regs(void);
extern void start_camera_gc0308(void);
extern void stop_camera_gc0308(void);
extern void set_camera_para_gc0308(struct camera_info_s *para);
#endif /* #define _AMLOGIC_CAMERA_OV5640_H */
