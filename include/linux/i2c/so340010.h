#ifndef __LINUX_SO340010_H
#define __LINUX_SO340010_H

struct cap_key{
	int code;	/* input key code */
	int mask;
	unsigned char *name;
};

struct so340010_platform_data{
	int (*init_irq)(void);
	int (*get_irq_level)(void);
	struct cap_key *key;
	int key_num;
};

#endif