#ifndef FREESCALE_DEV_INCLUDE_H
#define FREESCALE_DEV_INCLUDE_H

typedef  struct {
	struct class 		*cla;
	struct device		*dev;
	char  			name[20];
	unsigned int 		open_count;
	int	 			major;
	unsigned  int 		dbg_enable;
	char* buffer_start;
	unsigned int buffer_size;
	
	unsigned angle;
	unsigned orientation;
	unsigned videoangle; 
	
	int bypass;
	int disp_width;
	int disp_height;
#ifdef CONFIG_MIX_FREE_SCALE
	int ppscaler_flag;
	int scale_h_start;
	int scale_h_end;
	int scale_v_start;
	int scale_v_end;
#endif
	const vinfo_t *vinfo;
	int left;
	int top;
	int width;
	int height;
}freescale_device_t;

extern freescale_device_t  freescale_device;
#endif /* FREESCALE_DEV_INCLUDE_H. */
