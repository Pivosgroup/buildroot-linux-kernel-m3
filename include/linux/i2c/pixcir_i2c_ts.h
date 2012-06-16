#ifndef _LINUX_PIXCIR_I2C_TS_H_
#define _LINUX_PIXCIR_I2C_TS_H_


struct pixcir_i2c_ts_platform_data {
	unsigned int gpio_shutdown;
	unsigned int gpio_irq;
	
	unsigned int xmin;
	unsigned int xmax;
	unsigned int ymin;
	unsigned int ymax;

  unsigned int swap_xy :1;
  unsigned int xpol :1;
  unsigned int ypol :1;
  unsigned int point_id_available :1;
  
  unsigned short *key_code;
  unsigned short key_num;
};

#endif