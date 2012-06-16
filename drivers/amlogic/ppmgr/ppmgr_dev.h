#ifndef PPMGR_DEV_INCLUDE_H
#define PPMGR_DEV_INCLUDE_H

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
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
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
	
	int video_out;
}ppmgr_device_t;

extern ppmgr_device_t  ppmgr_device;
#endif /* PPMGR_DEV_INCLUDE_H. */
