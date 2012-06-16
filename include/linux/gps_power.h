#ifndef _GPS_POWER_H_
#define _GPS_POWER_H_
struct gps_power_platform_data{
	void (*power_on)(void);
	void (*power_off)(void);
};


#endif

