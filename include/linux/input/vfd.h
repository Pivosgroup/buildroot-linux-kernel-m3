#ifndef __LINUX_VFD_H
#define __LINUX_VFD_H

struct vfd_key{
	int code;	/* input key code */
	unsigned char *name;
	int value;	/* voltage/3.3v * 1023 */
};

struct vfd_platform_data{
	int (*set_stb_pin_value)(int value);		
	int (*set_clock_pin_value)(int value);	
	int (*set_do_pin_value)(int value);
	int (*get_di_pin_value)(void);
	
	struct vfd_key *key;
	int key_num;
};

#endif

