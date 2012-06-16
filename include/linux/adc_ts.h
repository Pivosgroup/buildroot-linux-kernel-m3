#ifndef __LINUX_ADCTS_H
#define __LINUX_ADCTS_H

struct adc_ts_platform_data {
	int irq;
	u16 x_plate_ohms;
	void (*service)(int cmd);
	int poll_delay;
	int poll_period;
	int abs_xmin;
	int abs_xmax;
	int abs_ymin;
	int abs_ymax;
	int (*convert)(int x, int y);
};

#endif
